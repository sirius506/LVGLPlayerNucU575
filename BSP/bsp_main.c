#include <stdio.h>
#include <string.h>
#include "bsp.h"
#include "rtosdef.h"
#include "debug.h"
#include "lvgl.h"
#include "app_gui.h"

SEMAPHORE_DEF(flashcmd)
SEMAPHORE_DEF(flashread)
SEMAPHORE_DEF(flashwrite)
SEMAPHORE_DEF(flashstatus)

SEMAPHORE_DEF(psramcmd)
SEMAPHORE_DEF(psramread)
SEMAPHORE_DEF(psramwrite)
SEMAPHORE_DEF(psramstatus)

SEMAPHORE_DEF(touch)
SEMAPHORE_DEF(codec)

SEMAPHORE_DEF(hashsem)

MUTEX_DEF(crcmutex)

static osMutexId_t crcLockId;

extern RTC_HandleTypeDef hrtc;

const HeapRegion_t xHeapRegions[3] = {
  { (uint8_t *)RTOS_HEAP_ADDR,  RTOS_HEAP_SIZE },
  { (uint8_t *)QSPI_PSRAM_ADDR, QSPI_PSRAM_SIZE },
  { NULL, 0 }
};

void *malloc(size_t size)
{
  void *p;
  uint32_t maddr;

  p = (void *)pvPortMalloc(size);
#ifdef VERBOSE_DEBUG
  if (size > 64 * 1024)
    debug_printf("%s: %d --> %x\n", __FUNCTION__, (int)size, p);
#endif
  if (p == NULL)
  {
    debug_printf("%s: NO MEM %d --> %x\n", __FUNCTION__, (int)size, p);
    return NULL;
  }
  maddr = (uint32_t)p;
  if ((maddr < RTOS_HEAP_ADDR) || (maddr >= (RTOS_HEAP_ADDR + RTOS_HEAP_SIZE)))
  {
    if (maddr < 0x70000000 || maddr >= 0x80000000)
    {
      debug_printf("Bad alloced mem %x\n", maddr);
      while (1) ;
    }
  }
  return p;
}

void *calloc(size_t count, size_t size)
{
  size_t amount = count * size;
  void *p;

  p = (void *)pvPortMalloc(amount);
  if (p)
    memset(p, 0, amount);
  return p;
}

void free(void *ptr)
{
  vPortFree(ptr);
}

void *realloc(void *ptr, size_t size)
{
  void *p;

  if (ptr)
  {
    uint32_t *vp;
    int len;

    vp = (uint32_t *)((int32_t)ptr - 4);
    len = *vp & 0xffffff;
//debug_printf("realloc: vp = %x, %d -> %d\n", vp, len, size);
    p = malloc(size);
    if (p)
    {
      memcpy(p, ptr, len);
      free(ptr);
    }
  }
  else
  {
    p = malloc(size);
  }
  return p;
}

char *strdup(const char *s1)
{
  int slen = strlen(s1);
  char *dp;

  dp = pvPortMalloc(slen + 1);
  if (dp)
    memcpy(dp, s1, slen + 1);
  else
  {
    debug_printf("%s: NO MEM %d --> %x\n", __FUNCTION__, (int)slen, dp);
    while (1) osDelay(100);
  }
  return dp;
}

static void i2c_callback(I2C_HandleTypeDef *hi2c)
{
  DOOM_I2C_Handle *handle = (DOOM_I2C_Handle *)hi2c->UserPointer;

  osSemaphoreRelease(handle->iosem);
}

static void i2c_error_callback(I2C_HandleTypeDef *hi2c)
{
  DOOM_I2C_Handle *handle = (DOOM_I2C_Handle *)hi2c->UserPointer;

debug_printf("%s:\n", __FUNCTION__);
  osSemaphoreRelease(handle->iosem);
}


static void error_callback(OSPI_HandleTypeDef *hospi)
{
  UNUSED(hospi);
  debug_printf("OSPI Error!!\n");
}

static void cmd_callback(OSPI_HandleTypeDef *hospi)
{ 
  DOOM_OSPI_Handle *ospi = (DOOM_OSPI_Handle *)hospi->UserPointer;
  osSemaphoreRelease(ospi->sem_cmd);
}
  
static void rx_callback(OSPI_HandleTypeDef *hospi)
{ 
  DOOM_OSPI_Handle *ospi = (DOOM_OSPI_Handle *)hospi->UserPointer; 
  osSemaphoreRelease(ospi->sem_read);
} 
  
static void tx_callback(OSPI_HandleTypeDef *hospi)
{ 
  DOOM_OSPI_Handle *ospi = (DOOM_OSPI_Handle *)hospi->UserPointer;
  osSemaphoreRelease(ospi->sem_write);
} 

static void status_callback(OSPI_HandleTypeDef *hospi)
{ 
  DOOM_OSPI_Handle *ospi = (DOOM_OSPI_Handle *)hospi->UserPointer;
  osSemaphoreRelease(ospi->sem_status);
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  extern void app_touch_event();

  switch (GPIO_Pin)
  {
  case CTP_INT_Pin:
    app_touch_event();
    break;
  case USER_BUTTON_Pin:
    debug_printf("USER BUTTON\n");
    app_screenshot();
    break;
  default:
    break;
  }
}

static void hash_complete(HASH_HandleTypeDef *hhash)
{
  DOOM_HASH_Handle *comp_hash = (DOOM_HASH_Handle *)hhash->UserPointer;

  osSemaphoreRelease(comp_hash->iosem);
}

static void bsp_hash_init(DOOM_HASH_Handle *comp_hash)
{
  comp_hash->iosem = osSemaphoreNew(1, 0, &attributes_hashsem);
  HAL_HASH_RegisterCallback(comp_hash->hhash, HAL_HASH_INPUTCPLT_CB_ID, hash_complete);
}

void bsp_compute_digest(uint8_t *digest, uint8_t *pdata, int dlen)
{
  DOOM_HASH_Handle *comp_hash = HalDevice.comp_hash;

  HAL_HASH_SHA1_Start_DMA(comp_hash->hhash, pdata, dlen);
  osSemaphoreAcquire(comp_hash->iosem, osWaitForever);
  HAL_HASH_SHA1_Finish(comp_hash->hhash, digest, 10);
}

void bsp_init(HAL_DEVICE *haldev)
{
  bsp_hash_init(haldev->comp_hash);

  haldev->tft_cmd_addr = LCD_CMD_ADDR;
  haldev->tft_data_addr = LCD_DATA_ADDR;

  haldev->qspi_flash->sem_cmd = osSemaphoreNew(1, 0, &attributes_flashcmd);
  haldev->qspi_flash->sem_read = osSemaphoreNew(1, 0, &attributes_flashread);
  haldev->qspi_flash->sem_write = osSemaphoreNew(1, 0, &attributes_flashwrite);
  haldev->qspi_flash->sem_status = osSemaphoreNew(1, 0, &attributes_flashstatus);
  haldev->qspi_flash->device_mode = DEV_MODE_FLASH;
  haldev->qspi_flash->qspi_mode = QSPI_ACCESS_SPI;
  HAL_OSPI_RegisterCallback(haldev->qspi_flash->hospi, HAL_OSPI_CMD_CPLT_CB_ID, cmd_callback);
  HAL_OSPI_RegisterCallback(haldev->qspi_flash->hospi, HAL_OSPI_RX_CPLT_CB_ID, rx_callback);
  HAL_OSPI_RegisterCallback(haldev->qspi_flash->hospi, HAL_OSPI_TX_CPLT_CB_ID, tx_callback);
  HAL_OSPI_RegisterCallback(haldev->qspi_flash->hospi, HAL_OSPI_STATUS_MATCH_CB_ID, status_callback);
  HAL_OSPI_RegisterCallback(haldev->qspi_flash->hospi, HAL_OSPI_ERROR_CB_ID, error_callback);

  haldev->qspi_psram->sem_cmd = osSemaphoreNew(1, 0, &attributes_psramcmd);
  haldev->qspi_psram->sem_read = osSemaphoreNew(1, 0, &attributes_psramread);
  haldev->qspi_psram->sem_write = osSemaphoreNew(1, 0, &attributes_psramwrite);
  haldev->qspi_psram->sem_status = osSemaphoreNew(1, 0, &attributes_psramstatus);
  haldev->qspi_psram->device_mode = DEV_MODE_PSRAM;
  haldev->qspi_psram->qspi_mode = QSPI_ACCESS_SPI;
  HAL_OSPI_RegisterCallback(haldev->qspi_psram->hospi, HAL_OSPI_CMD_CPLT_CB_ID, cmd_callback);
  HAL_OSPI_RegisterCallback(haldev->qspi_psram->hospi, HAL_OSPI_RX_CPLT_CB_ID, rx_callback);
  HAL_OSPI_RegisterCallback(haldev->qspi_psram->hospi, HAL_OSPI_TX_CPLT_CB_ID, tx_callback);
  HAL_OSPI_RegisterCallback(haldev->qspi_psram->hospi, HAL_OSPI_STATUS_MATCH_CB_ID, status_callback);

  haldev->touch_i2c->iosem = osSemaphoreNew(1, 0, &attributes_touch);
  HAL_I2C_RegisterCallback(haldev->touch_i2c->hi2c, HAL_I2C_MASTER_TX_COMPLETE_CB_ID, i2c_callback);
  HAL_I2C_RegisterCallback(haldev->touch_i2c->hi2c, HAL_I2C_MASTER_RX_COMPLETE_CB_ID, i2c_callback);

  haldev->codec_i2c->iosem = osSemaphoreNew(1, 0, &attributes_codec);
  HAL_I2C_RegisterCallback(haldev->codec_i2c->hi2c, HAL_I2C_MASTER_TX_COMPLETE_CB_ID, i2c_callback);
  HAL_I2C_RegisterCallback(haldev->codec_i2c->hi2c, HAL_I2C_MASTER_RX_COMPLETE_CB_ID, i2c_callback);
  HAL_I2C_RegisterCallback(haldev->codec_i2c->hi2c, HAL_I2C_MEM_RX_COMPLETE_CB_ID, i2c_callback);
  HAL_I2C_RegisterCallback(haldev->codec_i2c->hi2c, HAL_I2C_MEM_TX_COMPLETE_CB_ID, i2c_callback);
  HAL_I2C_RegisterCallback(haldev->codec_i2c->hi2c, HAL_I2C_ERROR_CB_ID, i2c_error_callback);

  crcLockId = osMutexNew(&attributes_crcmutex);

  HAL_TIM_PWM_Start(haldev->pwm_timer, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(haldev->pwm_timer, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(haldev->pwm_timer, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(haldev->pwm_timer, TIM_CHANNEL_4);
}

const int itochan[4] = { TIM_CHANNEL_4, TIM_CHANNEL_3, TIM_CHANNEL_2, TIM_CHANNEL_1 };

void bsp_ledpwm_update(HAL_DEVICE *haldev, int *pval)
{
  int val, i;
  TIM_OC_InitTypeDef sConfigOC = {0};

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  for (i = 0; i < 4; i++)
  {
    val = *pval++ * 2;
    if (val > 200) val = 200;
    sConfigOC.Pulse = val;
    HAL_TIM_PWM_ConfigChannel(haldev->pwm_timer, &sConfigOC, itochan[i]);
  }
}

static RTC_TimeTypeDef timedef;
static RTC_DateTypeDef datedef;

void bsp_get_time(char *tb)
{
  HAL_RTC_GetTime(&hrtc, &timedef, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &datedef, RTC_FORMAT_BIN);
  sprintf(tb, "%02d:%02d:%02d\r\n", timedef.Hours, timedef.Minutes, timedef.Seconds);
}

void bsp_set_time(char *tb)
{
  tb[2] = 0;
  timedef.Hours = atoi(tb);
  tb += 3;
  tb[2] = 0;
  timedef.Minutes = atoi(tb);
  tb += 3;
  timedef.Seconds = atoi(tb);
  HAL_RTC_SetTime(&hrtc, &timedef, RTC_FORMAT_BIN);
}

void bsp_get_date(char *tb)
{
  HAL_RTC_GetTime(&hrtc, &timedef, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &datedef, RTC_FORMAT_BIN);
  sprintf(tb, "%02d-%02d-%02d\r\n", datedef.Year, datedef.Month, datedef.Date);
}

void bsp_generate_snap_filename(char *cp, int size)
{
  HAL_RTC_GetTime(&hrtc, &timedef, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &datedef, RTC_FORMAT_BIN);

  lv_snprintf(cp, size, "IMG%02d%02d%02d%02d%02d%02d.rgb",
      datedef.Year, datedef.Month, datedef.Date,
      timedef.Hours, timedef.Minutes, timedef.Seconds);
}

void bsp_set_date(char *tb)
{
  tb[2] = 0;
  datedef.Year = atoi(tb);
  tb += 3;
  tb[2] = 0;
  datedef.Month = atoi(tb);
  tb += 3;
  datedef.Date = atoi(tb);
  HAL_RTC_SetDate(&hrtc, &datedef, RTC_FORMAT_BIN);
}

int bsp_sdcard_inserted()
{
  if (HAL_GPIO_ReadPin(SDCARD_DET_GPIO_Port, SDCARD_DET_Pin) == GPIO_PIN_RESET)
    return 1;
  return 0;
}

uint32_t bsp_calc_crc(uint8_t *bp, int len)
{
  uint32_t crcval;

  osMutexAcquire(crcLockId, osWaitForever);
  crcval = HAL_CRC_Calculate(HalDevice.crc_comp, (uint32_t *)bp, len);
  osMutexRelease(crcLockId);
  return crcval;
}

uint32_t bsp_accumulate_crc(uint8_t *bp, int len)
{
  uint32_t crcval;

  osMutexAcquire(crcLockId, osWaitForever);
  crcval = HAL_CRC_Accumulate(HalDevice.crc_comp, (uint32_t *)bp, len);
  osMutexRelease(crcLockId);
  return crcval;
}

void _write(int f, char *bp, int len)
{
  UNUSED(f);
  SEGGER_RTT_Write(0, bp, len);
}

void toggle_test0()
{
#if 0
  HAL_GPIO_TogglePin(TEST0_Port, TEST0_Pin);
#endif
}
