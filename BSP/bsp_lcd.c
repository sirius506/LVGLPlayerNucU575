/*
 *  @file bsp_lcd.c
 *  @brief ILI9488 TFT LCD driver for 16bit parallel interface
 */
#include <string.h>
#include "bsp.h"
#include "rtosdef.h"
#include "debug.h"
#include "board_if.h"

#define	ILICMD_READ_ID4		0xD3
#define	ILICMD_READ_SCAN	0x45

#define LCD_SEND_COMMAND(iodev, x)     ili_send_command(iodev, x, sizeof(x))

#define	CMD_MEM_WRITE	0x2C
#define	CMD_MEM_READ	0x2E

void bsp_release_lcd();

/* Definitions for tftsem */
SEMAPHORE_DEF(tftsem)
SEMAPHORE_DEF(lcd_sem)

static const uint16_t Cmd_PGAMCTRL[] = {
  0xE0,
  0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
  0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F
};

static const uint16_t Cmd_NGAMCTRL[] = {
  0xE1,
  0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
  0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F
};

static const uint16_t Cmd_PowerControl1[] = { 0xC0, 0x17, 0x15 };
static const uint16_t Cmd_PowerControl2[] = { 0xC1, 0x41 };
static const uint16_t Cmd_VCOMControl[] = { 0xC5, 0x00, 0x12, 0x80 };
static const uint16_t Cmd_WriteTearScan[] = { 0x44, 0x01, 0x80 };

static uint16_t Cmd_ColumnSet[] = { 0x2A, 0x00, 0x00, 0x00, 0x00 };
static uint16_t Cmd_PageSet[] =  { 0x2B, 0x00, 0x00, 0x00, 0x00 };

static const uint16_t Cmd_MemoryAccess_Landscape[] = { 0x36, 0xE8 };
static const uint16_t Cmd_ColumnSet_Landscape[] ={ 0x2A, 0x00, 0x00, 0x01, 0xDF };
static const uint16_t Cmd_PageSet_Landscape[] ={ 0x2B, 0x00, 0x00, 0x01, 0x3F };

static const uint16_t Cmd_InterfacePixel[] = { 0x3A, 0x55 };     // 16 bit
static const uint16_t Cmd_InterfaceMode[] = { 0xB0, 0x00 };
static const uint16_t Cmd_FrameRate[] = { 0xB1, 0xA0 };          // 60Hz
static const uint16_t Cmd_DisplayInversion[] = { 0xB4, 0x02 };   // 2-dot

static const uint16_t Cmd_DisplayInternalMem[] =  { 0xB6, 0x02, 0x02, 0x3B };
#ifdef USE_CAB
#ifdef BUILD_BASE
static const uint16_t Cmd_CAB_Control[] = { 0x55, 0x01 };
#else
static const uint16_t Cmd_CAB_Control[] = { 0x55, 0x03 };
#endif
#endif
static const uint16_t Cmd_SetImage[] = { 0xE9, 0x00 };           // Disable 24bit data
static const uint16_t Cmd_Adjust[] = { 0xF7, 0xA9, 0x51, 0x2C, 0x82 };
static const uint16_t Cmd_SleepOut[] = { 0x11 };
static const uint16_t Cmd_DisplayOn[] = { 0x29 };
static const uint16_t Cmd_DisplayOff[] = { 0x28 };
static const uint16_t Cmd_SleepIn[] = { 0x10 };
static const uint16_t Cmd_TearingOn[] = { 0x35, 0x00 };

void ili_send_command(HAL_DEVICE *iodev, const uint16_t *ptr, int len)
{
  volatile uint16_t *bp;

  bp = iodev->tft_cmd_addr;
  *bp = *ptr++;
  len--;
  bp = iodev->tft_data_addr;
  while (len > 0)
  {
    *bp = *ptr++;
    len--;
  }
}

static void ili_recv_command(HAL_DEVICE *iodev, uint16_t cmd, uint16_t *ptr, int len)
{
  *iodev->tft_cmd_addr = cmd;
  *ptr = *iodev->tft_data_addr;		/* Ignore first byte */
  while (len > 0)
  {
    *ptr++ = *iodev->tft_data_addr;
    len--;
  }
}

void LCD_L4_PartialCopy(HAL_DEVICE *iodev, uint8_t *map, int xpos, int ypos, int width, int height)
{
  while(DMA2D->CR & DMA2D_CR_START_Msk) ;
  Cmd_ColumnSet[1] = xpos >> 8;
  Cmd_ColumnSet[2] = xpos & 255;
  Cmd_ColumnSet[3] = (xpos + width - 1)>> 8;
  Cmd_ColumnSet[4] = (xpos + width - 1) & 255;
  ili_send_command(iodev, Cmd_ColumnSet, 5);

  Cmd_PageSet[1] = ypos >> 8;
  Cmd_PageSet[2] = ypos & 255;
  Cmd_PageSet[3] = (ypos + height - 1)>> 8;
  Cmd_PageSet[4] = (ypos + height - 1) & 255;
  ili_send_command(iodev, Cmd_PageSet, 5);

  *iodev->tft_cmd_addr = CMD_MEM_WRITE;	// Memroy WRITE

  DMA2D->CR = 0;
  DMA2D->FGPFCCR = DMA2D_INPUT_L4;
  DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
  DMA2D->FGMAR = (uint32_t)map;
  DMA2D->OMAR = (uint32_t)iodev->tft_data_addr;
  DMA2D->FGOR = 0;
  DMA2D->OOR = 0;
  DMA2D->NLR = (width << DMA2D_NLR_PL_Pos) | (height << DMA2D_NLR_NL_Pos);
  DMA2D->CR = (1<<16);

  /*start transfer*/
  DMA2D->CR |= DMA2D_CR_START | DMA2D_CR_TEIE|DMA2D_CR_CEIE;
  while(DMA2D->CR & DMA2D_CR_START_Msk) ;
}

void LCD_L8_Setup(HAL_DEVICE *iodev, int xpos, int ypos, int width, int height)
{
  while(DMA2D->CR & DMA2D_CR_START_Msk) ;

  Cmd_ColumnSet[1] = xpos >> 8;
  Cmd_ColumnSet[2] = xpos & 255;
  Cmd_ColumnSet[3] = (xpos + width - 1)>> 8;
  Cmd_ColumnSet[4] = (xpos + width - 1) & 255;
  ili_send_command(iodev, Cmd_ColumnSet, 5);

  Cmd_PageSet[1] = ypos >> 8;
  Cmd_PageSet[2] = ypos & 255;
  Cmd_PageSet[3] = (ypos + height - 1)>> 8;
  Cmd_PageSet[4] = (ypos + height - 1) & 255;
  ili_send_command(iodev, Cmd_PageSet, 5);

  *iodev->tft_cmd_addr = CMD_MEM_WRITE;	// Memroy WRITE

  DMA2D->CR = 0;
  DMA2D->FGPFCCR = DMA2D_INPUT_L8;
  DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
  DMA2D->OMAR = (uint32_t)iodev->tft_data_addr;
  DMA2D->FGOR = 0;
  DMA2D->OOR = 0;
  DMA2D->NLR = (width << DMA2D_NLR_PL_Pos) | (height << DMA2D_NLR_NL_Pos);
  DMA2D->CR = (1<<16);
}

void LCD_L8_Write(uint8_t *bp, int width, int height)
{
  while (DMA2D->CR & DMA2D_CR_START_Msk) ;
  DMA2D->FGMAR = (uint32_t)bp;
  DMA2D->NLR = (width << DMA2D_NLR_PL_Pos) | (height << DMA2D_NLR_NL_Pos);
  DMA2D->CR |= DMA2D_CR_START | DMA2D_CR_TEIE|DMA2D_CR_CEIE;
}

void LCD_L8_PartialCopy(HAL_DEVICE *iodev, uint8_t *map, int xpos, int ypos, int width, int height)
{
if (DMA2D->CR & DMA2D_CR_START_Msk)
{
  debug_printf("DMA2D busy0.\n");
  while(DMA2D->CR & DMA2D_CR_START_Msk) ;
}
  Cmd_ColumnSet[1] = xpos >> 8;
  Cmd_ColumnSet[2] = xpos & 255;
  Cmd_ColumnSet[3] = (xpos + width - 1)>> 8;
  Cmd_ColumnSet[4] = (xpos + width - 1) & 255;
  ili_send_command(iodev, Cmd_ColumnSet, 5);

  Cmd_PageSet[1] = ypos >> 8;
  Cmd_PageSet[2] = ypos & 255;
  Cmd_PageSet[3] = (ypos + height - 1)>> 8;
  Cmd_PageSet[4] = (ypos + height - 1) & 255;
  ili_send_command(iodev, Cmd_PageSet, 5);

  *iodev->tft_cmd_addr = CMD_MEM_WRITE;	// Memroy WRITE

  DMA2D->CR = 0;
  DMA2D->FGPFCCR = DMA2D_INPUT_L8;
  DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
  DMA2D->FGMAR = (uint32_t)map;
  DMA2D->OMAR = (uint32_t)iodev->tft_data_addr;
  DMA2D->FGOR = 0;
  DMA2D->OOR = 0;
  DMA2D->NLR = (width << DMA2D_NLR_PL_Pos) | (height << DMA2D_NLR_NL_Pos);
  DMA2D->CR = (1<<16);

  /*start transfer*/
  DMA2D->CR |= DMA2D_CR_START | DMA2D_CR_TEIE|DMA2D_CR_CEIE;
  while(DMA2D->CR & DMA2D_CR_START_Msk) ;
}

void LCD_RGB565_PartialCopy(HAL_DEVICE *iodev, uint8_t *map, int xpos, int ypos, int width, int height)
{
  if (DMA2D->CR & DMA2D_CR_START_Msk)
  {
    debug_printf("DMA2D busy %d.\n", iodev->tft_lcd->owner_task);
    while(DMA2D->CR & DMA2D_CR_START_Msk) ;
  }
  Cmd_ColumnSet[1] = xpos >> 8;
  Cmd_ColumnSet[2] = xpos & 255;
  Cmd_ColumnSet[3] = (xpos + width - 1)>> 8;
  Cmd_ColumnSet[4] = (xpos + width - 1) & 255;
  ili_send_command(iodev, Cmd_ColumnSet, 5);

  Cmd_PageSet[1] = ypos >> 8;
  Cmd_PageSet[2] = ypos & 255;
  Cmd_PageSet[3] = (ypos + height - 1)>> 8;
  Cmd_PageSet[4] = (ypos + height - 1) & 255;
  ili_send_command(iodev, Cmd_PageSet, 5);

  *iodev->tft_cmd_addr = CMD_MEM_WRITE;	// Memroy WRITE

  DMA2D->CR = 0;
  DMA2D->FGPFCCR = DMA2D_INPUT_RGB565;
  DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
  DMA2D->FGMAR = (uint32_t)map;
  DMA2D->OMAR = (uint32_t)iodev->tft_data_addr;
  DMA2D->FGOR = 0;
  DMA2D->OOR = 0;
  DMA2D->NLR = (width << DMA2D_NLR_PL_Pos) | (height << DMA2D_NLR_NL_Pos);
  DMA2D->CR = (1<<16);

  /*start transfer and enable interrupt */
  DMA2D->CR |= DMA2D_CR_START | DMA2D_CR_TCIE|DMA2D_CR_TEIE|DMA2D_CR_CEIE;
}

static const uint16_t DeviceId[3] = { 0x00, 0x94, 0x88 };

static void lcd_fill(HAL_DEVICE *iodev)
{
  volatile uint16_t *bp = iodev->tft_data_addr;
  int pixels;

  *iodev->tft_cmd_addr = CMD_MEM_WRITE;	// Memroy WRITE
  pixels = 480 * 320;
  while (pixels > 0)
  {
    *bp = 0x0;
    *bp = 0x0;
    *bp = 0x0;
    *bp = 0x0;
    pixels -= 4;
  }
}

static uint16_t id_check[5];

static void dma2d_error_callback(DMA2D_HandleTypeDef *hdma2d)
{
  debug_printf("DMA2D error %d", hdma2d->ErrorCode);
  while (1) osDelay(100);
}

static void dma2d_callback(DMA2D_HandleTypeDef *hdma2d)
{
#if 0
  DOOM_DMA2D_Handle *dma2dhandle = (DOOM_DMA2D_Handle *)hdma2d->UserPointer;
#else
  DOOM_LCD_Handle *lcd_handle = (DOOM_LCD_Handle *)hdma2d->UserPointer;
#endif
  osStatus_t st;

#ifdef LCD_DEBUG
  if ((DoomScreenStatus == DOOM_SCREEN_SUSPEND) || (DoomScreenStatus == DOOM_SCREEN_SUSPENDED))
  {
debug_printf("DMA2D done\n");
  }
#endif
  st = osSemaphoreRelease(lcd_handle->iosem);
  if (st != osOK)
    debug_printf("%s: st = %d\n", __FUNCTION__, st);
#ifdef LCD_DEBUG
  if (DMA2D->CR & DMA2D_CR_START_Msk)
    debug_printf("comp busy: %x\n", DMA2D->CR);
#endif
  bsp_release_lcd(lcd_handle);
}

void bsp_wait_lcd(DOOM_LCD_Handle *tft_lcd)
{
  osStatus_t st;

  st = osSemaphoreAcquire(tft_lcd->iosem, osWaitForever);
  if (st != osOK)
    debug_printf("%s: st = %d\n", __FUNCTION__, st);
  while (DMA2D->CR & DMA2D_CR_START_Msk);
}

void bsp_acquire_lcd(DOOM_LCD_Handle *lcd_handle, int thistask)
{
  osStatus_t st;

  st = osSemaphoreAcquire(lcd_handle->lcd_lock, osWaitForever);
  if (st != osOK)
    debug_printf("%s: st = %d\n", __FUNCTION__, st);
  lcd_handle->owner_task = thistask;
}

void bsp_release_lcd(DOOM_LCD_Handle *lcd_handle)
{
  osStatus_t st;

  st = osSemaphoreRelease(lcd_handle->lcd_lock);
  if (st != osOK)
    debug_printf("%s: st = %d\n", __FUNCTION__, st);
}

int tft_init(HAL_DEVICE *iodev)
{
  iodev->tft_lcd->iosem = osSemaphoreNew(1, 1, &attributes_tftsem);

  HAL_DMA2D_RegisterCallback(iodev->tft_lcd->hdma2d, HAL_DMA2D_TRANSFERCOMPLETE_CB_ID, dma2d_callback);
  HAL_DMA2D_RegisterCallback(iodev->tft_lcd->hdma2d, HAL_DMA2D_TRANSFERERROR_CB_ID, dma2d_error_callback);

  iodev->tft_lcd->lcd_lock = osSemaphoreNew(1, 1, &attributes_lcd_sem);

  ili_recv_command(iodev, ILICMD_READ_ID4, id_check, 4);
  if (memcmp(id_check, DeviceId, sizeof(DeviceId)) != 0)
  {
    debug_printf("Failed to detect ILI9488 (%04x, %04x, %04x)\n", id_check[0], id_check[1], id_check[2]);
    return -1;
  }
  debug_printf("ILI9488 detected.\n");

  LCD_SEND_COMMAND(iodev, Cmd_PGAMCTRL);
  LCD_SEND_COMMAND(iodev, Cmd_NGAMCTRL);

  LCD_SEND_COMMAND(iodev, Cmd_DisplayOff);
  osDelay(50);

  LCD_SEND_COMMAND(iodev, Cmd_PowerControl1);
  osDelay(5);
  LCD_SEND_COMMAND(iodev, Cmd_PowerControl2);
  osDelay(5);
  LCD_SEND_COMMAND(iodev, Cmd_VCOMControl);
  osDelay(5);


  LCD_SEND_COMMAND(iodev, Cmd_MemoryAccess_Landscape);
  LCD_SEND_COMMAND(iodev, Cmd_ColumnSet_Landscape);
  LCD_SEND_COMMAND(iodev, Cmd_PageSet_Landscape);

    LCD_SEND_COMMAND(iodev, Cmd_InterfacePixel);
    LCD_SEND_COMMAND(iodev, Cmd_InterfaceMode);
    LCD_SEND_COMMAND(iodev, Cmd_FrameRate);
    LCD_SEND_COMMAND(iodev, Cmd_DisplayInversion);
    LCD_SEND_COMMAND(iodev, Cmd_DisplayInternalMem);
    LCD_SEND_COMMAND(iodev, Cmd_SetImage);

  LCD_SEND_COMMAND(iodev, Cmd_Adjust);
  osDelay(5);

  lcd_fill(iodev);

  //tft_report(lcdInfo);

  LCD_SEND_COMMAND(iodev, Cmd_SleepOut);
  osDelay(150);
  LCD_SEND_COMMAND(iodev, Cmd_DisplayOn);
  osDelay(5);

#if 0
    LCD_SEND_COMMAND(iodev, Cmd_WriteTearScan);
#endif

#if 1
    ili_recv_command(iodev, ILICMD_READ_SCAN, id_check, 2);
    debug_printf("scan line = %02x %02x\n", id_check[0], id_check[1]);
#endif

    LCD_SEND_COMMAND(iodev, Cmd_TearingOn);

  HAL_TIM_PWM_Start(iodev->tft_lcd->pwm_timer, TIM_CHANNEL_1);
  Board_Set_Brightness(iodev, 80);
  return 0;
}

void ili_set_region(HAL_DEVICE *iodev, int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
  Cmd_ColumnSet[1] = x1 >> 8;
  Cmd_ColumnSet[2] = x1;
  Cmd_ColumnSet[3] = x2 >> 8;
  Cmd_ColumnSet[4] = x2;
  ili_send_command(iodev, Cmd_ColumnSet, 5);

  Cmd_PageSet[1] = y1 >> 8;
  Cmd_PageSet[2] = y1;
  Cmd_PageSet[3] = y2 >> 8;
  Cmd_PageSet[4] = y2;
  ili_send_command(iodev, Cmd_PageSet, 5);
}

void bsp_lcd_flush(HAL_DEVICE *iodev, uint8_t * ptr, int x1, int x2,  int y1,  int y2)
{
  bsp_acquire_lcd(iodev->tft_lcd, 0);
  LCD_RGB565_PartialCopy(iodev, ptr, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

void SetScreenMode(HAL_DEVICE *iodev, int mode, uint8_t *fbaddr)
{
  if (mode == 0)
  {
    ili_set_region(iodev, 80, 399, 60, 259);
  }
  else
  {
    ili_set_region(iodev, 0, 479, 0, 319);
  }
}

/*
convert -depth 8 -size 480x320+0 rgb:rgb3.bin test.jpg 
*/

union cconv {
  uint16_t wdata[6];
  uint8_t  bdata[12];
} convbuffer;

void bsp_lcd_save(uint8_t *bp)
{
  HAL_DEVICE *iodev = &HalDevice;
  uint16_t wdata;
  volatile uint16_t *wp;

  if (bp)
  {
    bsp_acquire_lcd(iodev->tft_lcd, 2);

    if (DMA2D->CR & DMA2D_CR_START_Msk)
      debug_printf("%s: DMA2D busy.\n", __FUNCTION__);
    while(DMA2D->CR & DMA2D_CR_START_Msk) ;

    Cmd_ColumnSet[1] = 0;
    Cmd_ColumnSet[2] = 0;
    Cmd_ColumnSet[3] = 479 >> 8;
    Cmd_ColumnSet[4] = 479 & 0xff;
    ili_send_command(iodev, Cmd_ColumnSet, 5);

    Cmd_PageSet[1] = 0;
    Cmd_PageSet[2] = 0;
    Cmd_PageSet[3] = 319 >> 8;
    Cmd_PageSet[4] = 319 & 0xff;
    ili_send_command(iodev, Cmd_PageSet, 5);

    *iodev->tft_cmd_addr = CMD_MEM_READ;	// Memroy READ
    wp = iodev->tft_data_addr;
    wdata = *wp;				// dummy data
    (void) wdata;

#define USE_PSRAM_DMA2D
#ifdef USE_PSRAM_DMA2D
    DMA2D->CR = 0;
    DMA2D->FGPFCCR = DMA2D_INPUT_RGB888;
#if 1
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB888 | DMA2D_OPFCCR_SB;
#else
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565 | DMA2D_OPFCCR_SB;
#endif
    DMA2D->FGMAR = (uint32_t)iodev->tft_data_addr;
    DMA2D->OMAR = (uint32_t)bp;
    DMA2D->FGOR = 0;
    DMA2D->OOR = 0;
    DMA2D->NLR = (480 << DMA2D_NLR_PL_Pos) | (320 << DMA2D_NLR_NL_Pos);
    DMA2D->CR = (1<<16);

    DMA2D->CR |= DMA2D_CR_START | DMA2D_CR_TEIE|DMA2D_CR_CEIE;
    while(DMA2D->CR & DMA2D_CR_START_Msk) ;
    DMA2D->CR = 0;
#else
    int i;
    uint8_t bdata;

    for (i = 0; i < 480 * 3 * 320 / 6; i++)
    {
      wdata = *wp;
      bdata = wdata >> 8;	// R
      bdata |= (bdata & 0x08)? 0x07 : 0x00;
      *bp++ = bdata;
      bdata = wdata & 255;	// G
      bdata |= (bdata & 0x04)? 0x03 : 0x00;
      *bp++ = bdata;

      wdata = *wp;
      bdata = wdata >> 8;	// B
      bdata |= (bdata & 0x08)? 0x07 : 0x00;
      *bp++ = bdata;
      bdata = wdata & 255;	// R
      bdata |= (bdata & 0x08)? 0x07 : 0x00;
      *bp++ = bdata;

      wdata = *wp;
      bdata = wdata >> 8;	// G
      bdata |= (bdata & 0x04)? 0x03 : 0x00;
      *bp++ = bdata;
      bdata = wdata & 255;	// B
      bdata |= (bdata & 0x08)? 0x07 : 0x00;
      *bp++ = bdata;
    }
#endif

    bsp_release_lcd(iodev->tft_lcd);
  }
}

#ifdef TEST_MEMREAD
uint16_t TestData[] = {
  0xF800, 0xF800, 0xF800,
  0x07E0, 0x07E0, 0x07E0,
  0x001F, 0x001F, 0x001F
};

uint16_t TestOut[30];

void bsp_lcd_test()
{
  HAL_DEVICE *iodev = &HalDevice;
  uint16_t wdata;
  volatile uint16_t *wp;
  int i;

  ili_set_region(iodev, 0, 8, 0, 0);
  *iodev->tft_cmd_addr = CMD_MEM_WRITE;
  wp = iodev->tft_data_addr;
  for (i = 0; i < 9; i++)
  {
    *wp = TestData[i];
  }

  ili_set_region(iodev, 0, 8, 0, 0);

  *iodev->tft_cmd_addr = CMD_MEM_READ;	// Memroy READ
  wp = iodev->tft_data_addr;
  wdata = *wp;				// dummy data
  (void) wdata;
  for (i = 0; i < 30; i++)
  {
    TestOut[i] = *wp;
  }
  *iodev->tft_cmd_addr = 0x00;
}
#endif
