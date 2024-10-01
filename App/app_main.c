#include <stdio.h>
#include "DoomPlayer.h"
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

extern void StartGuiTask(void *args);
extern void StartPlayerGuiTask(void *args);
extern void StartShellTask(void *arg);

extern void MX_FATFS_Init();
extern void tft_init(HAL_DEVICE *haldev);

extern lv_indev_data_t tp_data;
extern void bsp_process_touch(lv_indev_data_t *tp);

void postMainRequest(int cmd, void *arg, int val)
{
  REQUEST_CMD request;

  request.cmd = cmd;
  request.arg = arg;
  request.val = val;
  osMessageQueuePut(reqcmdqId, &request, 0, 0);
}

extern const HeapRegion_t xHeapRegions[];

#define	FNAME_LEN	22
#define	SCREEN_BUFF_SIZE	(480*3*320)
#define	SCREEN_DIR	"/Screen"
#define	WBSIZE	(1024*4)

static uint8_t wbuffer[WBSIZE];
extern void bsp_generate_snap_filename(char *cp, int size);

void SaveScreenFile(uint8_t *bp, int len)
{
  FIL *pfile;
  FRESULT res;
  int nw;
  UINT nb;
  char fname[FNAME_LEN];

  nb = 0;
  res = FR_OK;
  f_chdir(SCREEN_DIR);
  bsp_generate_snap_filename(fname, FNAME_LEN);
  pfile = CreateFATFile(fname);
  if (pfile)
  {
    while (len > 0 && res == FR_OK)
    {
      nw = (len > WBSIZE)? WBSIZE : len;
      memcpy(wbuffer, bp, nw);
      res = f_write(pfile, wbuffer, nw, &nb);
      if (res == FR_OK)
        bp += nb;
      len -= nb;
    }
    CloseFATFile(pfile);
    debug_printf("save %s.\n", (res == FR_OK)? "success" : "falied");
  }
  f_chdir("/");
}

void StartDefaultTask(void *argument)
{
  UNUSED(argument);
  GUI_EVENT guiev;
  int val, res;
  char *errs1, *errs2;
  uint8_t *screen_buffer;

  HAL_DEVICE *haldev = &HalDevice;

  bsp_init(haldev);

  val = Board_PSRAMInfo(haldev);
  debug_printf("PSRAM: %d MBit\n", val);
  Board_PSRAM_Init(haldev);

  vPortDefineHeapRegions(xHeapRegions );

  reqcmdqId = osMessageQueueNew(REQCMD_DEPTH, sizeof(REQUEST_CMD), &attributes_reqcmdq);

  tft_init(haldev);
  osThreadNew(StartGuiTask, haldev, &attributes_guitask);

  osThreadNew(StartShellTask, NULL, &attributes_shelltask);

  unsigned int wait_time = osWaitForever;

  errs1 = NULL;

  osDelay(400);

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
      guiev.evval0 = VerifyFlash((void *)&errs1, (void *)&errs2);
      guiev.evarg1 = errs1;
      guiev.evarg2 = errs2;
      postGuiEvent(&guiev);
      break; 
    case REQ_VERIFY_FONT:
      guiev.evcode = GUIEV_FONT_REPORT;
      guiev.evval0 = VerifyFlash((void *)&errs1, (void *)&errs2);
      guiev.evarg1 = errs1;
      guiev.evarg2 = errs2;
      postGuiEvent(&guiev);
      break; 
    case REQ_ERASE_FLASH:
      game = (WADLIST *) request.arg;
      CopyFlash(game, (uint32_t)res);
      break;
    case REQ_TOUCH_INT:
      bsp_process_touch(&tp_data);
      break;
    case REQ_END_DOOM:
      wait_time = 3000;
      btapi_shutdown();
      break;
    case REQ_DUMMY:
      if (wait_time != osWaitForever)
        wait_time = 500;
      break;
    case REQ_CAPTURE_SAVE:
      debug_printf("capture data @ %x\n", request.arg);
      break;
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
