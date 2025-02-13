#include <stdio.h>
#include "DoomPlayer.h"
#if (USE_SYSTEMVIEW == 1)
#include "SEGGER_SYSVIEW.h"
#endif
#include <string.h>
#include "board_if.h"
#include "app_verify.h"
#include "btapi.h"
#include "fatfs.h"
#include "audio_output.h"
#include "m_misc.h"

extern HAL_DEVICE HalDevice;

TASK_DEF(guitask,     1400, osPriorityBelowNormal3)
TASK_DEF(shelltask,    500, osPriorityBelowNormal4)

#define REQCMD_DEPTH     6
static uint8_t reqcmdBuffer[REQCMD_DEPTH * sizeof(REQUEST_CMD)];
MESSAGEQ_DEF(reqcmdq, reqcmdBuffer, sizeof(reqcmdBuffer))
static osMessageQueueId_t  reqcmdqId;

static osEventFlagsId_t evreqFlagId;
static uint8_t inFlashUpdate;

EVFLAG_DEF(mainreqFlag)

extern void StartGuiTask(void *args);
extern void StartPlayerGuiTask(void *args);
extern void StartShellTask(void *arg);

extern void MX_FATFS_Init();
extern void tft_init(HAL_DEVICE *haldev);

extern lv_indev_data_t tp_data;
extern void bsp_process_touch(lv_indev_data_t *tp);

#define	MR_FLAG_CAPTURE	0x01

static const uint8_t bmpheader[] = {
  0x42, 0x4d,			// Magic Number
  0x8a, 0x08, 0x07, 0x00,	// File size
  0x00, 0x00, 0x00, 0x00,	// Reserved
  0x8a, 0x00, 0x00, 0x00,	// Offset

  0x7c, 0x00, 0x00, 0x00,	// V5 header size
  0xe0, 0x01, 0x00, 0x00,	// width
  0xc0, 0xfe, 0xff, 0xff,	// height
  0x01, 0x00,			// Number of plane
  0x18, 0x00,			// Number of bits per pixel
  0x00, 0x00, 0x00, 0x00,	// No compression
  0x00, 0x08, 0x07, 0x00,	// Picture data size
  0x00, 0x00, 0x00, 0x00,	// horizonatal resolution
  0x00, 0x00, 0x00, 0x00,	// vertivcal resolution
  0x00, 0x00, 0x00, 0x00,	// Number of colors
  0x00, 0x00, 0x00, 0x00,	// Number of important colors
  0x00, 0x00, 0xff, 0x00,	// Red color mask
  0x00, 0xff, 0x00, 0x00,	// Greeen color mask
  0xff, 0x00, 0x00, 0x00,	// Blue color mask
  0x00, 0x00, 0x00, 0x00,	// Alpha channel mask
  0x42, 0x47, 0x52, 0x73,	// Color space - sRGB

  0x8f, 0xc2, 0xf5, 0x28, 0x51, 0xb8,
  0x1e, 0x15, 0x1e, 0x85, 0xeb, 0x01,
  0x33, 0x33, 0x33, 0x13, 0x66, 0x66,
  0x66, 0x26, 0x66, 0x66, 0x66, 0x06,
  0x99, 0x99, 0x99, 0x09, 0x3d, 0x0a,
  0xd7, 0x03, 0x28, 0x5c, 0x8f, 0x32,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
};

void postMainRequest(int cmd, void *arg, int val)
{
  REQUEST_CMD request;
  if (inFlashUpdate && (cmd == REQ_SCREEN_SAVE))
  {
    osEventFlagsSet(evreqFlagId, MR_FLAG_CAPTURE);
  }
  else
  {
    request.cmd = cmd;
    request.arg = arg;
    request.val = val;
    osMessageQueuePut(reqcmdqId, &request, 0, 0);
  }
}

extern const HeapRegion_t xHeapRegions[];

#define	FNAME_LEN	(9+22)
#define	SCREEN_BUFF_SIZE	(480*3*320)
#define	SCREEN_DIR	"/Screen"
#define	WBSIZE	(1024*4)

static uint8_t wbuffer[WBSIZE];
extern void bsp_generate_snap_filename(char *cp, char *dirname, int size, char *ext);

void SaveScreenFile(uint8_t *bp, int len)
{
  FIL *pfile;
  FRESULT res;
  int nw;
  UINT nb;
  char fname[FNAME_LEN];

  nb = 0;
  res = FR_OK;
  {
    int i;
    uint8_t *wp, wb;

    /* Swap Red and Blue */
    wp = bp;
    for (i = 0; i < 480*320; i++)
    {
      wb = wp[2];
      wp[2] = wp[0];
      wp[0] = wb;
      wp += 3;
    }
  }
  bsp_generate_snap_filename(fname, SCREEN_DIR, FNAME_LEN, "bmp");
  pfile = CreateRGBFile(fname);
  if (pfile)
  {
    memcpy(wbuffer, bmpheader, sizeof(bmpheader));
    nw = WBSIZE - sizeof(bmpheader);
    memcpy(wbuffer + sizeof(bmpheader), bp, nw);
    res = f_write(pfile, wbuffer, WBSIZE, &nb);
    len -= nw;
    bp += nw;
    while (len > 0 && res == FR_OK)
    {
      nw = (len > WBSIZE)? WBSIZE : len;
      memcpy(wbuffer, bp, nw);
      res = f_write(pfile, wbuffer, nw, &nb);
      if (res == FR_OK)
        bp += nb;
      else
      {
        debug_printf("res = %d, nb = %d/%d\n", res, nb, nw);
        break;
      }
      len -= nb;
    }
    CloseRGBFile(pfile);
    debug_printf("save %s.\n", (res == FR_OK)? "success" : "falied");
  }
}

static uint8_t *screen_buffer;

void cpature_check()
{
  uint32_t evflag;
  HAL_DEVICE *haldev = &HalDevice;

  evflag = osEventFlagsGet(evreqFlagId);
  if (evflag & MR_FLAG_CAPTURE)
  {
    osEventFlagsClear(evreqFlagId, MR_FLAG_CAPTURE);
    if (screen_buffer)
    {
debug_printf("catpture in check!!\n");
        Board_Audio_Pause(haldev);
        bsp_lcd_save(screen_buffer);

        SaveScreenFile(screen_buffer, SCREEN_BUFF_SIZE);
        Board_Audio_Resume(haldev);
debug_printf("image saved.\n");
    }
  }
}

void StartDefaultTask(void *argument)
{
  UNUSED(argument);
  GUI_EVENT guiev;
  int val, res;
  char *errs1, *errs2;
  int32_t flash_size;

  HAL_DEVICE *haldev = &HalDevice;

  bsp_init(haldev);

#if (USE_SYSTEMVIEW == 1)
  SEGGER_SYSVIEW_Conf();
  debug_printf("SystemView enabled.\n");
#endif

debug_printf("MCU Rev: %x\n",  HAL_GetREVID());
  val = Board_PSRAMInfo(haldev);
  debug_printf("PSRAM: %d MBit\n", val);
  Board_PSRAM_Init(haldev);

  vPortDefineHeapRegions(xHeapRegions );

  evreqFlagId = osEventFlagsNew(&attributes_mainreqFlag);
  reqcmdqId = osMessageQueueNew(REQCMD_DEPTH, sizeof(REQUEST_CMD), &attributes_reqcmdq);

  tft_init(haldev);
  osThreadNew(StartGuiTask, haldev, &attributes_guitask);

  osThreadNew(StartShellTask, NULL, &attributes_shelltask);

  unsigned int wait_time = osWaitForever;

  errs1 = NULL;

  flash_size = Board_FlashInfo(haldev);
  Board_Flash_Init(haldev, 1);
  osDelay(40);

  if (bsp_sdcard_inserted())
  {
    MX_FATFS_Init();
    res = FR_OK;

    /* Let's try to create SCREEN_DIR and find possible initial errors. */

    res = f_mkdir(SCREEN_DIR);

    if (res != FR_OK)
    {
      switch (res)
      {
      case FR_NOT_READY:
        errs1 = "SD card not ready.";
        break;
      case FR_DENIED:
        errs1 = "SD card protected.";
        break;
      case FR_EXIST:
        res = FR_OK;
        break;
      case FR_INT_ERR:
      case FR_DISK_ERR:
      default:
        errs1 = "SD card error.";
        break;
      }
    }
  }
  else
  {
    res = FR_NOT_READY;
    errs1 = "SD card not ready.";
  } 

  if (res != FR_OK)
  {
    guiev.evcode = GUIEV_SD_REPORT;
    guiev.evval0 = res;
    guiev.evarg1 = errs1;
    guiev.evarg2 = "";
    postGuiEvent(&guiev);
  }

  /* Reserve screen_buffer space to hold RGB888 image data. */
  screen_buffer = (uint8_t *)malloc(SCREEN_BUFF_SIZE);

  for(;;)
  {
    REQUEST_CMD request;
    WADLIST *game;
    osStatus_t qst;

    qst = osMessageQueueGet(reqcmdqId, &request, NULL, wait_time);

    if (qst != osOK)
    {
      debug_printf("qst = %d\n", qst);
      NVIC_SystemReset();
    }

    switch (request.cmd)
    {
    case REQ_VERIFY_SD:
      if (errs1)	/* SD card error has already reported. Ignore verify request. */
        break;
      guiev.evcode = GUIEV_SD_REPORT;
      res = VerifySDCard((void *)&errs1, (void *)&errs2);
      guiev.evval0 = (res > 0)? 0 : res;
      guiev.evarg1 = errs1;
      guiev.evarg2 = errs2;
      postGuiEvent(&guiev);
      break;
    case REQ_VERIFY_FLASH: 
      guiev.evcode = GUIEV_FLASH_REPORT;
      guiev.evval0 = VerifyFlash((void *)&errs1, (void *)&errs2, flash_size);
      guiev.evarg1 = errs1;
      guiev.evarg2 = errs2;
      postGuiEvent(&guiev);
      break; 
    case REQ_VERIFY_FONT:
      guiev.evcode = GUIEV_FONT_REPORT;
      guiev.evval0 = VerifyFlash((void *)&errs1, (void *)&errs2, flash_size);
      guiev.evarg1 = errs1;
      guiev.evarg2 = errs2;
      postGuiEvent(&guiev);
      break; 
    case REQ_ERASE_FLASH:
      inFlashUpdate = 1;
      osEventFlagsClear(evreqFlagId, MR_FLAG_CAPTURE);
      game = (WADLIST *) request.arg;
      CopyFlash(game, (uint32_t)res);
      inFlashUpdate = 0;
      break;
    case REQ_TOUCH_INT:
      bsp_process_touch(&tp_data);
      break;
    case REQ_END_DOOM:
      wait_time = 3000;
      btapi_shutdown();
      break;
#if 0
    case REQ_DUMMY:
      if (wait_time != osWaitForever)
        wait_time = 500;
      break;
#endif
    case REQ_SCREEN_SAVE:
      if (screen_buffer)
      {
        Board_Audio_Pause(haldev);
        bsp_lcd_save(screen_buffer);
        SaveScreenFile(screen_buffer, SCREEN_BUFF_SIZE);
        Board_Audio_Resume(haldev);
      }
      break;
    default:
      break;
    }
  }
}

/**
 * @brief Generate touch event.
 * @note This functions is called with interrupt context.
 */
void app_touch_event()
{
  postMainRequest(REQ_TOUCH_INT, NULL, 0);
}
