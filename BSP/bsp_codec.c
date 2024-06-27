#include "string.h"
#include "bsp.h"
#include "debug.h"

#define CODEC_ADDR	(0x18 << 1)

typedef struct {
 uint8_t reg;
 uint8_t valset;
} TLV_SETUP;

/* MCLK = 11.29MHz */
/* CODEC_CLKIN = NDAC * MDAC * DOSR * DAC_FS */
/* NDAC = 2, MDAC = 8, DOSR = 128 */

const TLV_SETUP TLVInitData[] = {
  { 0x00, 0x00 },	/* Select Page 0 */
  { 0x01, 0x01 }, 	/* Initialize the device through software reset */
  { 0x04, 0x03 },	/* PLL_CLKIN as MCLK and CODEC_CLKIN as PLL_CLK */
  { 0x05, 0x91 },	/* Power up PLL, set pll divider P=1 and R=1 */
  { 0x06, 0x08 },	/* Set pll divider J=8 */
  { 0xFE, 0x10 },
  { 0x0B, 0x82 },	/* NDAC = 2 */
  { 0x0C, 0x88 },	/* MDAC = 8 */
  { 0x0D, 0x00 },	/* DOSR = 0x0080 */
  { 0x0E, 0x80 }, 	/* DOSR = 128 */

  { 0x00, 0x00 },	/* Select Page 0 */
  { 0x3C, 0x01 },	/* Set the DAC Mode to PRB_P1 */

  { 0x00, 0x01 },	/* Select Page 1 */
  { 0x01, 0x08 },	/* Disable weak AVDD to DVDD connection */
  { 0x02, 0x01 },	/* Enable Analog Blocks, AVDD LDO Power up */
  { 0x7B, 0x01 },	/* Set the REF charging time to 40ms */
  { 0x14, 0x04 },	/* Set HP power up time for NO POP */

  { 0x00, 0x01 },	/* Select Page 1 */
  { 0x0C, 0x08 },	/* Route LDAC to HPL */
  { 0x0D, 0x08 },	/* Route RDAC to HPR */
#if 1
  { 0x03, 0x08 },	/* Set the DAC PTM mode to PTM_1 */
  { 0x04, 0x08 },
#endif

  { 0x00, 0x00 },	/* Select Page 0 */
  { 0x41, 0x00 },	/* DAC => 0dB */
  { 0x42, 0x00 },
  { 0x3F, 0xD6 },	/* Power up LDAC/RDAC */

  { 0x00, 0x01 },	/* Select Page 1 */
  { 0x10, 0x00 },	/* Unmute HPL driver, 0dB Gain */
  { 0x11, 0x00 },	/* Unmute HPR driver, 0dB Gain */
  { 0x09, 0x30 },	/* Power up HPL/HPR */

  { 0xFE, 250 },
  { 0x00, 0x00 },	/* Select Page 0 */
  { 0x3F, 0xD4 },
  { 0x40, 0x00 },	/* Unmute LDAC/RDAC */
  { 0xFF, 0xFF },
};

const uint8_t TLV_Signature[2] = { 0x11, 0x04 };

void bsp_codec_init(DOOM_I2C_Handle *codec_i2c)
{
  osStatus_t st;
  uint8_t regvals[4];
  uint16_t maddr;

  HAL_I2C_Mem_Read_IT(codec_i2c->hi2c, CODEC_ADDR, 5, I2C_MEMADD_SIZE_8BIT, regvals, 2);
  st = osSemaphoreAcquire(codec_i2c->iosem, 100);
  if (st == osOK)
  {
    if (memcmp(regvals, TLV_Signature, 2) == 0)
    {
      const TLV_SETUP *dp = TLVInitData;

      debug_printf("TLV320DAC3203 detected.\n");
      while (dp->reg != 0xFF)
      {
        if (dp->reg == 0xFE )
          if (dp->valset > 100)
            osDelay(dp->valset * 10);
          else
            osDelay(dp->valset);
        else
        {
          maddr = (uint16_t )dp->reg;
          HAL_I2C_Mem_Write_IT(codec_i2c->hi2c, CODEC_ADDR, maddr, I2C_MEMADD_SIZE_8BIT, (uint8_t *)&dp->valset, 1);
          st = osSemaphoreAcquire(codec_i2c->iosem, 100);
        }
        dp++;
      }
      debug_printf("TLV initialize finished.\n");

      /* Select page 0 for latter volume control */
      regvals[0] = 0;
      HAL_I2C_Mem_Write_IT(codec_i2c->hi2c, CODEC_ADDR, 0, I2C_MEMADD_SIZE_8BIT, regvals, 1);
      st = osSemaphoreAcquire(codec_i2c->iosem, 100);
    }
    else
    {
      debug_printf("codec_regs: %02x %02x\n", regvals[0] & 255, regvals[1] & 255);
    }
  }
  else
  {
    debug_printf("Failed to detect codec device.\n");
  } 
}

int bsp_codec_getvol(DOOM_I2C_Handle *codec_i2c)
{
  int8_t regvals[2];

  HAL_I2C_Mem_Read_IT(codec_i2c->hi2c, CODEC_ADDR, 65, I2C_MEMADD_SIZE_8BIT, (uint8_t *)regvals, 2);
  osSemaphoreAcquire(codec_i2c->iosem, 100);
  return (int) regvals[0];
}

void bsp_codec_setvol(DOOM_I2C_Handle *codec_i2c, int newvol)
{
  int8_t regvals[2];

  if (newvol > 24)
    newvol = 24;
  if (newvol < -63)
    newvol = -63;
  regvals[0] = regvals[1] = newvol;

  HAL_I2C_Mem_Write_IT(codec_i2c->hi2c, CODEC_ADDR, 65, I2C_MEMADD_SIZE_8BIT, (uint8_t *)regvals, 2);
  osSemaphoreAcquire(codec_i2c->iosem, 100);
}
