/**
 *   Game controller interface
 */
#include "DoomPlayer.h"
#include "gamepad.h"

static GAMEPAD_INFO GamePadInfo;

/**
 * @brief List of supported gamepads.
 */
const struct sGamePad KnownGamePads[] = {
  { VID_SONY, PID_DUALSENSE, "DualSense",     &DualSenseDriver },
  { VID_SONY, PID_DUALSHOCK, "DualShock4",    &DualShockDriver },
  { VID_SONY, PID_ZERO2,     "8BitDo Zero 2", &Zero2Driver },
};

/*
 * Map Virtual button bitmask to LVGL Keypad code
 */
const PADKEY_DATA PadKeyDefs[] = {
  { VBMASK_DOWN,     LV_KEY_DOWN },
  { VBMASK_RIGHT,    LV_KEY_NEXT },
  { VBMASK_LEFT,     LV_KEY_PREV },
  { VBMASK_UP,       LV_KEY_UP },
  { VBMASK_PS,       LV_KEY_HOME },
  { VBMASK_TRIANGLE, LV_KEY_DEL },
  { VBMASK_CIRCLE,   LV_KEY_ENTER },
  { VBMASK_CROSS,    LV_KEY_DEL },
  { VBMASK_SQUARE,   LV_KEY_ENTER },
  { VBMASK_L1,       LV_KEY_LEFT },
  { VBMASK_R1,       LV_KEY_RIGHT },
  { 0, 0 },			/* Data END marker */
};

static GAMEPAD_BUFFERS gamepadBuffer;

GAMEPAD_BUFFERS *GetGamePadBuffer()
{
  return &gamepadBuffer;
}

/**
 * @brief See if game pad is supported
 * @param vid: Vendor ID
 * @param pid: Product ID
 * @return Pointer to GAMEPAD_INFO or NULL
 */
GAMEPAD_INFO *IsSupportedGamePad(uint16_t vid, uint16_t pid)
{
  unsigned int i;
  const struct sGamePad *gp = KnownGamePads;

  for (i = 0; i < sizeof(KnownGamePads)/sizeof(struct sGamePad); i++)
  {
    if (gp->vid == vid && gp->pid == pid)
    {
      GamePadInfo.name = gp->name;
      GamePadInfo.hid_mode = HID_MODE_LVGL;
      GamePadInfo.padDriver = gp->padDriver;
      return &GamePadInfo;
    }
    gp++;
  }
  debug_printf("This gamepad is not supported.\n");
  return NULL;
}

#define	PS_OUTPUT_CRC32_SEED	0xA2

static const uint8_t output_seed[] = { PS_OUTPUT_CRC32_SEED };

extern CRC_HandleTypeDef hcrc;

/**
 * @brief Compute CRC32 value for BT output report
 */
uint32_t bt_comp_crc(uint8_t *ptr, int len)
{
  uint32_t crcval;

  crcval = bsp_calc_crc((uint8_t *)output_seed, 1);
  crcval = bsp_accumulate_crc(ptr, len - 4);
  crcval = ~crcval;
  return crcval;
}
