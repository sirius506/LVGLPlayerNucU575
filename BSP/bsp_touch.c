#include "bsp.h"
#include "rtosdef.h"
#include "target.h"
#include "debug.h"
#include "../lvgl/lvgl.h"

#define FTDEV_ADDR      (0x38 << 1)
#define	PANEL_ID	0x11

#define	REGADDR_PERIOD_ACTIVE	0x88
#define	REGADDR_FOCALTECH_ID	0xA8

typedef struct {
  uint8_t td_status;
  uint8_t p1_xh, p1_xl;
  uint8_t p1_yh, p1_yl;
  uint8_t p1_weight, p1_misc;
  uint8_t p2_xh, p2_xl;
  uint8_t p2_yh, p2_yl;
  uint8_t p2_weight, p2_misc;
} TOUCH_REGS;
  
static TOUCH_REGS touch_regs;

int bsp_touch_init(HAL_DEVICE *haldev)
{
  uint8_t regaddr, regval;
  osStatus_t st;

  /* Verify Touch Panel ID */
  regaddr = REGADDR_FOCALTECH_ID;

  HAL_I2C_Master_Transmit_IT(haldev->touch_i2c->hi2c, FTDEV_ADDR, &regaddr, 1);
  st = osSemaphoreAcquire(haldev->touch_i2c->iosem, 100);
  if (st != osOK)
  {
    debug_printf("Failed to detect touch device.\n");
    return -1;
  } 
  HAL_I2C_Master_Receive_IT(haldev->touch_i2c->hi2c, FTDEV_ADDR, &regval, 1);
  osSemaphoreAcquire(haldev->touch_i2c->iosem, osWaitForever);
  if (regval == PANEL_ID)
  {
    debug_printf("FT6X36 detected.\n");
#ifdef TPC_RATE_CHECK
    uint8_t regvals[2];
    regvals[0] = 0x88;
    regvals[1] = 5;
#if 1
    HAL_I2C_Master_Transmit_IT(haldev->touch_i2c->hi2c, FTDEV_ADDR, regvals, 2);
    osSemaphoreAcquire(haldev->touch_i2c->iosem, osWaitForever);
    osDelay(2);
#endif
    HAL_I2C_Master_Transmit_IT(haldev->touch_i2c->hi2c, FTDEV_ADDR, regvals, 1);
    osSemaphoreAcquire(haldev->touch_i2c->iosem, 100);
    HAL_I2C_Master_Receive_IT(haldev->touch_i2c->hi2c, FTDEV_ADDR, regvals, 2);
    osSemaphoreAcquire(haldev->touch_i2c->iosem, osWaitForever);
    debug_printf("rate is %d, %d.\n", regvals[0], regvals[1]);
#endif
  }
  else
  {
    debug_printf("PanelID: %x\n", regval);
  }
  return 0;
}

void bsp_process_touch(lv_indev_data_t *tp)
{
  uint8_t regaddr, flag;
  uint16_t xpos, ypos;
  HAL_DEVICE *haldev = &HalDevice;

  regaddr = 0x02;
  HAL_I2C_Master_Transmit_IT(haldev->touch_i2c->hi2c, FTDEV_ADDR, &regaddr, 1);
  osSemaphoreAcquire(haldev->touch_i2c->iosem, 100);
  HAL_I2C_Master_Receive_IT(haldev->touch_i2c->hi2c, FTDEV_ADDR, (uint8_t *)&touch_regs, sizeof(touch_regs));
  osSemaphoreAcquire(haldev->touch_i2c->iosem, osWaitForever);

  flag = (touch_regs.p1_xh >> 6) & 3;
  xpos = ((touch_regs.p1_xh << 8) | touch_regs.p1_xl) & 0x3fff;
  ypos = ((touch_regs.p1_yh << 8) | touch_regs.p1_yl) & 0x0fff;
  {
    uint16_t w = xpos;

    xpos = ypos;
    ypos = w;
    xpos = DISP_HOR_RES - xpos;
  }
  switch (flag)
  {
  case 0:       /* Down */
    tp->state = LV_INDEV_STATE_PRESSED;
#ifdef DEBUG_TOUCH
debug_printf("Down @ (%d,%d)\n", xpos, ypos);
#endif
    break;
  case 1:       /* Up */
    tp->state = LV_INDEV_STATE_RELEASED;
#ifdef DEBUG_TOUCH
debug_printf("Up @ (%d,%d)\n", xpos, ypos);
#endif
    break;
  case 2:       /* Move */
    tp->state = LV_INDEV_STATE_PRESSED;
    break;
  default:
    break;
  }

  if ((flag & 1) == 0)  /* Down or Move */
  {
    tp->point.x = xpos;
    tp->point.y = ypos;
  }
}
