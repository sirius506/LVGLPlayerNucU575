#include "DoomPlayer.h"
#include <string.h>
#include "qspi.h"
#include "board_if.h"
#include "audio_output.h"
#include "debug.h"

typedef struct
{
    char *name;
    const uint8_t *data;
    unsigned int w;
    unsigned int h;
} txt_font_t;

#include "../chocolate-doom-3.0.1/textscreen/fonts/tftfont.h"
#include "../chocolate-doom-3.0.1/textscreen/fonts/normal.h"
#include "../chocolate-doom-3.0.1/textscreen/fonts/small.h"

#define ENDOOM_W 80
#define ENDOOM_H 25

extern void LCD_L8_PartialCopy(HAL_DEVICE *iodev, uint8_t *map, int xpos, int ypos, int width, int height);
extern void LCD_L4_PartialCopy(HAL_DEVICE *iodev, uint8_t *map, int xpos, int ypos, int width, int height);

const uint8_t W25Q256_ID[3] = { 0xEF, 0x70, 0x19 };
const uint8_t APS6404_ID[2] = { 0x0D, 0x5D };

typedef struct {
  uint32_t FlashSize;                        /*!< Size of the flash                             */
  uint32_t EraseSectorSize;                  /*!< Size of sectors for the erase operation       */
  uint32_t EraseSectorsNumber;               /*!< Number of sectors for the erase operation     */
  uint32_t EraseSubSectorSize;               /*!< Size of subsector for the erase operation     */
  uint32_t EraseSubSectorNumber;             /*!< Number of subsector for the erase operation   */
  uint32_t EraseSubSector1Size;              /*!< Size of subsector 1 for the erase operation   */
  uint32_t EraseSubSector1Number;            /*!< Number of subsector 1 for the erase operation */
  uint32_t ProgPageSize;                     /*!< Size of pages for the program operation       */
  uint32_t ProgPagesNumber;                  /*!< Number of pages for the program operation     */
} Flash_Info_t;

static Flash_Info_t QSPI_FlashInfo;

/**
 * @brief Detect QSPI flash device.
 * @return Byte size of the flash or 0 if not found.
 */
int Board_FlashInfo(HAL_DEVICE *haldev)
{
  uint8_t flashid[3];

  qspi_reset(haldev->qspi_flash);

  if (flash_read_id(haldev->qspi_flash, flashid, sizeof(flashid)) == 0)
  {
    if (memcmp(flashid, W25Q256_ID, 3))
    {
      /* Device ID doesn't match. Try to exit from QPI mode. */
      debug_printf("Exit QPI mode.\n");
      qspi_exit_qpi(haldev->qspi_flash);
      qspi_reset(haldev->qspi_flash);

      if (flash_read_id(haldev->qspi_flash, flashid, sizeof(flashid)) != 0)
        return -1;
    }
    if (memcmp(flashid, W25Q256_ID, 3) == 0)
    {
      QSPI_FlashInfo.FlashSize          = (uint32_t)32 * 1024 * 1024;
      QSPI_FlashInfo.EraseSectorSize    = (uint32_t)64 * 1024;
      QSPI_FlashInfo.EraseSectorsNumber = (uint32_t)512;
      QSPI_FlashInfo.ProgPageSize       = (uint32_t)256;
      QSPI_FlashInfo.ProgPagesNumber    = (uint32_t)131072;
      debug_printf("W25Q256 detected.\n");
      return QSPI_FlashInfo.FlashSize;
    }
  }
  debug_printf("W25Q256 not found.\n");
  return -1;
}

int Board_PSRAMInfo(HAL_DEVICE *haldev)
{
  uint8_t psramid[3];
  int cap;

  qspi_exit_qpi(haldev->qspi_psram);
  psram_read_id(haldev->qspi_psram, psramid, 3);
  if (memcmp(psramid, APS6404_ID, 2) == 0)
  {
    cap = psramid[2] & 0xE0;
    debug_printf("PSRAM detected (%d)\n", cap);
#if 0
    haldev->qspi_psram->hospi.Instance->CR &= ~1;
    haldev->qspi_psram->hospi.Instance->DCR2 = 2;
    haldev->qspi_psram->hospi.Instance->CR |= 1;
#endif
    return cap;
  }
  return 0;
}

int Board_Flash_Init(HAL_DEVICE *haldev, int mapmode)
{
  if (haldev->qspi_flash->qspi_mode & QSPI_ACCESS_MMAP)
  {
    qspi_disable_mappedmode(haldev->qspi_flash);
  }
#if 0
  if (qspi_write_enable_sr(haldev->qspi_flash))
    return -1;
  qspi_write_status3(haldev->qspi_flash, 0x00);
#endif
#define USE_QPI
#ifdef USE_QPI
  /* Enable QPI mode */
  if (qspi_write_enable_sr(haldev->qspi_flash))
    return -1;
  if (qspi_qpi_enable(haldev->qspi_flash))
    return -1;
  if (qspi_enter_qpi(haldev->qspi_flash))
    return -1;
#endif
  /* Enter 4 byte address mode */
  if (qspi_enter_4bytemode(haldev->qspi_flash))
    return -1;
  debug_printf("Initialize OK.\n");

  if (mapmode)
    qspi_enable_mappedmode(haldev->qspi_flash);
#if 0
  else
    qspi_disable_mappedmode(haldev->qspi_flash);
#endif
  return 0;
}

int Board_PSRAM_Init(HAL_DEVICE *haldev)
{
  if (qspi_enter_qpi(haldev->qspi_psram))
    return -1;
  qspi_enable_mappedmode(haldev->qspi_psram);

  return 0;
}

int Board_EraseSectorSize()
{
  int block_size;

  block_size = QSPI_FlashInfo.EraseSectorSize;
  return block_size;
}

void Board_Erase_Block(HAL_DEVICE *haldev, uint32_t baddr)
{
  qspi_erase_block(haldev->qspi_flash, (uint8_t *)baddr);
}

int Board_Flash_Write(HAL_DEVICE *haldev, uint8_t *bp, uint32_t baddr, int len)
{
  qspi_flash_write(haldev->qspi_flash, bp, (uint8_t *)baddr, len);
  return 0;
}

void Board_Flash_ReInit(int mapmode)
{
debug_printf("%s:\n", __FUNCTION__);
}

static int expand_mode;

void Board_DoomModeLCD(int expand)
{
  expand_mode = expand;
}

#define EXP_LINES       8
  
static const int ExpandIndex[8] = { 0, 0, 320, 320, 640, 960, 960, 1280 };
 

static lv_image_dsc_t game_image_desc = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.cf = LV_COLOR_FORMAT_I8,
  .header.flags = 0,
  .header.w = 480,
  .header.h = 320,
  .header.stride = 480,
  .data_size = 480*320 + 4 * 256,
  .data = NULL,
};

lv_image_dsc_t *GetGameImageDesc()
{
  return &game_image_desc;
}

void ExpandInMemory(uint8_t *bp)
{
  lv_image_dsc_t *idesc;
  int X, Y;
  uint8_t *mp, *cbp, *obp;
  uint8_t bd;
  int index;

  extern const uint8_t *GameImageData;

  idesc = &game_image_desc;
  if (idesc->data)
    free((void *)idesc->data);
  mp = malloc(idesc->data_size);
  idesc->data = mp;

  memcpy(mp, GameImageData, 4 * 256);	// Copy palette data

  cbp = obp = mp + 4 * 256;

  Y = 0;
  index = 0;

  do
  {
    X = 0;
    do
    {
      /* Horizontal expantion. We repeat even number pixel twice. */
      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;                      /* repeat even number pixel */
      *obp++ = bp[index++];             /* Odd number pixel */

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];
      X += 24;
    } while (X < 480);

    Y++;

    switch (Y & 7)
    {
    case 0:
      /* Evenry 8 lines, update index and source address */
      bp += 320 * 5;
      break;
    case 1:
    case 3:
    case 6:
      /* Repeat previouse line */
      Y++;
      cbp = obp - 480;
      memcpy(obp, cbp, 480);
      obp += 480;
      break;
    default:
      break;
    }
    index = ExpandIndex[Y & 7];
  } while (Y < 320);
}

static uint8_t ExpBuffer[480*EXP_LINES];

/*
 * @brief Expand 320x200 Doom screen to 480x320 pixels
 */
void Board_ScreenExpand(uint8_t *bp, uint32_t *palette)
{
  int X, Y;
  uint8_t *cbp, *obp;
  uint8_t bd;
  int index;
  HAL_DEVICE *haldev = &HalDevice;

  bsp_acquire_lcd(haldev->tft_lcd, 1);
  if (palette)
  { 
    int i;
    uint32_t *cptr = (uint32_t *)palette;
  
    for (i = 0; i < 256; i++)
    {
       DMA2D->FGCLUT[i] = *cptr++;
    }
  }

  if (expand_mode == 0)
  {
    LCD_L8_PartialCopy(haldev, bp, 160, 0, 320, 200);
    bsp_release_lcd(haldev->tft_lcd);

    if (DoomScreenStatus == DOOM_SCREEN_SUSPEND)
    {
      postGuiEventMessage(GUIEV_CHEAT_ACK, 0, NULL, NULL);
      osThreadSuspend(osThreadGetId());
  debug_printf("Resumed.\n");
    }
    return;
  }

  LCD_L8_Setup(haldev, 0, 0, 480, 320);

  cbp = obp = ExpBuffer;

  Y = 0;
  index = 0;

  do
  {
    X = 0;
    do
    {
      /* Horizontal expantion. We repeat even number pixel twice. */
      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;                      /* repeat even number pixel */
      *obp++ = bp[index++];             /* Odd number pixel */

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];

      bd = bp[index++];
      *obp++ = bd;
      *obp++ = bd;
      *obp++ = bp[index++];
      X += 24;
    } while (X < 480);

    Y++;

    switch (Y & 7)
    {
    case 0:
      /* Evenry 8 lines, update source address */
      LCD_L8_Write(ExpBuffer + 480 * 4, 480, 4);
      obp = ExpBuffer;
      bp += 320 * 5;
      break;
    case 1:
    case 3:
    case 6:
      /* Repeat previouse line */
      Y++;
      cbp = obp - 480;
      memcpy(obp, cbp, 480);
      obp += 480;
      if ((Y & 7) == 4)
      {
        LCD_L8_Write(ExpBuffer, 480, 4);
      }
      break;
    default:
      break;
    }
    index = ExpandIndex[Y & 7];
  } while (Y < 320);

  while(DMA2D->CR & DMA2D_CR_START_Msk) ;

  bsp_release_lcd(haldev->tft_lcd);

  if (DoomScreenStatus == DOOM_SCREEN_SUSPEND)
  {
    postGuiEventMessage(GUIEV_CHEAT_ACK, 0, NULL, NULL);
    osThreadSuspend(osThreadGetId());
debug_printf("Resumed.\n");
  }
}

static const uint8_t cindex[4*16] = {
// Blue Green Red   Alpha
  0x00, 0x00, 0x00, 0xff,       // 0: Black         #000000
  0xFF, 0x00, 0x00, 0xff,       // 1: Blue          #0000FF
  0x00, 0x80, 0x00, 0xff,       // 2: Green         #008000
  0xFF, 0xFF, 0x00, 0xff,       // 3: Cyan          #00FFFF
  0x00, 0x00, 0xFF, 0xff,       // 4: Red           #FF0000
  0xFF, 0x00, 0xFF, 0xff,       // 5: Magenta       #FF00FF
  0x2A, 0x2A, 0xA5, 0xff,       // 6: Brown         #A52A2A
  0xD3, 0xD3, 0xD3, 0xff,       // 7: Light Gray    #D3D3D3
  0xA9, 0xA9, 0xA9, 0xff,       // 8: Dark Gray     #A9A9A9
  0xE6, 0xD8, 0xAD, 0xff,       // 9: Light Blue    #ADD8E6
  0x90, 0xEE, 0x90, 0xff,       // A: Light Green   #90EE90
  0xFF, 0xFF, 0xE0, 0xff,       // B: Ligh Cyan     #E0FFFF
  0xCB, 0xCC, 0xFF, 0xff,       // C: C Light Red   #FFCCCB
  0xFF, 0x80, 0xFF, 0xff,       // D: Light Magenta #FF80FF
  0x00, 0xFF, 0xFF, 0xff,       // E: Yellow        #FFFF00
  0x00, 0xFF, 0xFF, 0xff,       // F: Bright White  #FFFFFF
};

static void setGlyph(uint8_t *pwp, const uint32_t boffset, const txt_font_t *font, uint8_t attr)
{
  uint8_t *wp;
  const uint8_t *gbp;
  uint8_t maskBit;
  uint16_t fgColor, bgColor, color;
  int x, y;

  gbp = font->data + boffset / 8;
  maskBit = 1 << (boffset & 7);
  fgColor = attr & 0x0F;
  bgColor = (attr >> 4) & 0x07;
  for (y = 0; y < font->h; y++)
  {
    wp = pwp;
    for (x = 0; x < font->w; x++)
    {
      if (maskBit == 0)
      {
        gbp++;
        maskBit = 0x01;
      }
      color = (*gbp & maskBit)? fgColor : bgColor;
      if (x & 1)
      {
        *wp++ |= color;
      }
      else
      {
        *wp = color << 4;
      }
      maskBit <<= 1;
    }
    pwp += font->w * ENDOOM_W / 2;
  }
}

void Board_Endoom(uint8_t *bp)
{
  const txt_font_t *font;
  uint8_t cdata, attr;
  int fbsize;
  HAL_DEVICE *haldev = &HalDevice;

  int i;
  uint32_t *cptr = (uint32_t *)cindex;

  bsp_acquire_lcd(haldev->tft_lcd, 0);

  /* Set palette */
  for (i = 0; i < 16; i++)
  {
    DMA2D->FGCLUT[i] = *cptr++;
  }

  uint8_t *lbuff = ExpBuffer;
  font = expand_mode ? &tft_font : &small_font;

  fbsize = font->w * font->h;           // glyph bitmap size in bits

  uint8_t *pwp;
  int x, y;
  int xoff, yoff;

  osDelay(2);
  xoff = ((font->w * ENDOOM_W) >= 480)? 0 : 160;
  yoff = 0;
  for ( y = 0; y < ENDOOM_H; y++)
  {
    pwp = lbuff;
    for (x = 0; x < ENDOOM_W; x++)
    {
      cdata = *bp++;
      attr = *bp++ & 0x7F;
      if ((x+1) * font->w <= 480)
        setGlyph(pwp, cdata * fbsize, font, attr);
      pwp += font->w / 2;
    }
    LCD_L4_PartialCopy(&HalDevice, lbuff, xoff, yoff + y * font->h, font->w * ENDOOM_W, font->h);
  }
  bsp_release_lcd(haldev->tft_lcd);
}

static TIM_OC_InitTypeDef sConfigOC = {0};

int Board_Get_Brightness()
{
  return sConfigOC.Pulse / 2;
}

void Board_Set_Brightness(HAL_DEVICE *haldev, int brval)
{
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  brval = brval * 2;
  if (brval > 200) brval = 200;
  sConfigOC.Pulse = brval;
  HAL_TIM_PWM_ConfigChannel(haldev->tft_lcd->pwm_timer, &sConfigOC, TIM_CHANNEL_1);
}

void Board_Audio_Pause(HAL_DEVICE *haldev)
{
  AUDIO_CONF *aconf;
  AUDIO_OUTPUT_DRIVER *pDriver;

  aconf = get_audio_config(NULL);
  if (aconf->status == AUDIO_ST_PLAY)
  {
    pDriver = (AUDIO_OUTPUT_DRIVER *)aconf->devconf->pDriver;
    pDriver->Pause(aconf);
  }
}

void Board_Audio_Resume(HAL_DEVICE *haldev)
{
  AUDIO_CONF *aconf;
  AUDIO_OUTPUT_DRIVER *pDriver;

  aconf = get_audio_config(NULL);
  if (aconf->status == AUDIO_ST_PAUSE)
  {
    pDriver = (AUDIO_OUTPUT_DRIVER *)aconf->devconf->pDriver;
    pDriver->Resume(aconf);
  }
}

static void read_comp(UART_HandleTypeDef *huart)
{
  DOOM_UART_Handle *uart = (DOOM_UART_Handle *)huart->UserPointer;
  (*uart->uartrx_comp)(uart);
}

static void write_comp(UART_HandleTypeDef *huart)
{
  DOOM_UART_Handle *uart = (DOOM_UART_Handle *)huart->UserPointer;
  (*uart->uarttx_comp)(uart);
}

void Board_Uart_Init(DOOM_UART_Handle *uart)
{
  HAL_UART_RegisterCallback(uart->huart, HAL_UART_RX_COMPLETE_CB_ID, read_comp);
  HAL_UART_RegisterCallback(uart->huart, HAL_UART_TX_COMPLETE_CB_ID, write_comp);
}

void Board_Uart_Receive_IT(DOOM_UART_Handle *uart, uint8_t *buf, int len)
{
  HAL_UART_Receive_IT(uart->huart, buf, len);
}

void Board_Uart_Transmit_DMA(DOOM_UART_Handle *uart, uint8_t *buf, int cnt)
{
  HAL_UART_Transmit_DMA(uart->huart, buf, cnt);
}
