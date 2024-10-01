#ifndef BSP_H
#define BSP_H
#include "cmsis_os2.h"
#include "main.h"
#include "target.h"

#define	TTF_FONT_NAME	"AlbumFont.ttf"

typedef enum {
  QSPI_ACCESS_SPI = 0,
  QSPI_ACCESS_QPI = 2,
  QSPI_ACCESS_DTR = 4,
  QSPI_ACCESS_MMAP = 8
} QSPI_MODE;

typedef enum {
  DEV_MODE_FLASH = 0,
  DEV_MODE_PSRAM
} DEV_MODE;

typedef enum {
  BOOTM_DOOM = 0,
  BOOTM_A2DP
} BOOT_MODE;

typedef struct {
  I2C_HandleTypeDef  *hi2c;
  osSemaphoreId_t    *iosem;
} DOOM_I2C_Handle;

typedef struct {
  SAI_HandleTypeDef  *hsai;
  osSemaphoreId_t    *iosem;
  void (*saitx_half_comp)();
  void (*saitx_full_comp)();
} DOOM_SAI_Handle;

typedef struct {
  DAC_HandleTypeDef  *hdac;
  osSemaphoreId_t    *iosem;
} DOOM_DAC_Handle;

typedef struct {
  OSPI_HandleTypeDef *hospi;
  DEV_MODE           device_mode;
  QSPI_MODE          qspi_mode;
  osSemaphoreId_t    *sem_read;
  osSemaphoreId_t    *sem_write;
  osSemaphoreId_t    *sem_cmd;
  osSemaphoreId_t    *sem_status;
} DOOM_OSPI_Handle;

typedef struct {
  UART_HandleTypeDef *huart;
  osSemaphoreId_t    *recv_sem;
  osSemaphoreId_t    *send_sem;
  DMA_HandleTypeDef  *rxdma_handle;
  DMA_HandleTypeDef  *txdma_handle;
  void (*uarttx_comp)();
  void (*uartrx_comp)();
} DOOM_UART_Handle;

typedef struct {
  HASH_HandleTypeDef  *hhash;
  osSemaphoreId_t     *iosem;
} DOOM_HASH_Handle;

typedef struct {
  TIM_HandleTypeDef  *htimer;
} DOOM_TIM_Handle;

typedef struct {
  DMA2D_HandleTypeDef *hdma2d;
  TIM_HandleTypeDef   *pwm_timer;
  osSemaphoreId_t     *iosem;
  osSemaphoreId_t     *lcd_lock;
  int                 owner_task;
} DOOM_LCD_Handle;

typedef struct {
  DOOM_I2C_Handle  *touch_i2c;
  DOOM_I2C_Handle  *codec_i2c;
  DOOM_UART_Handle *bt_uart;
  DOOM_UART_Handle *shell_uart;
  DOOM_OSPI_Handle *qspi_flash;
  DOOM_OSPI_Handle *qspi_psram;
  DOOM_LCD_Handle   *tft_lcd;
  DOOM_HASH_Handle  *comp_hash;
  DOOM_SAI_Handle   *audio_sai;
  volatile uint16_t *tft_cmd_addr;
  volatile uint16_t *tft_data_addr;
  DCACHE_HandleTypeDef *dcache;
  CRC_HandleTypeDef *crc_comp;;
  SD_HandleTypeDef  *sdmmc;
  DOOM_DAC_Handle   *audio_dac;
  TIM_HandleTypeDef *pwm_timer;
  BOOT_MODE         boot_mode;
} HAL_DEVICE;

void bsp_init(HAL_DEVICE *haldev);
int bsp_touch_init(HAL_DEVICE *haldev);

int flash_read_id(DOOM_OSPI_Handle *qspi, uint8_t *bp, int idlen);
int psram_read_id(DOOM_OSPI_Handle *qspi, uint8_t *bp, int idlen);

extern HAL_DEVICE HalDevice;

void bsp_set_date(char *tb);
void bsp_get_date(char *tb);
void bsp_set_time(char *tb);
void bsp_get_time(char *tb);
void bsp_acquire_lcd(DOOM_LCD_Handle *lcd_handle, int thistask);
void bsp_release_lcd(DOOM_LCD_Handle *lcd_handle);
void bsp_wait_lcd();
void bsp_lcd_save(uint8_t *bp);

void bsp_codec_init(DOOM_I2C_Handle *codec_i2c, int sample_rate);
int bsp_codec_getvol(DOOM_I2C_Handle *codec_i2c);
void bsp_codec_setvol(DOOM_I2C_Handle *codec_i2c, int newvol);

void LCD_L8_Setup(HAL_DEVICE *iodev, int xpos, int ypos, int width, int height);
void LCD_L8_Write(uint8_t *bp, int width, int height);
void bsp_lcd_flush(HAL_DEVICE *iodev, uint8_t * ptr, int x1, int x2,  int y1,  int y2);

void bsp_ledpwm_update(HAL_DEVICE *haldev, int *pval);
int bsp_sdcard_inserted();

#endif
