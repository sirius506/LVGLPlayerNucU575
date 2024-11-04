/**
 * @brief SONY Dual Sense Controller driver
 */
#include "DoomPlayer.h"
#include "gamepad.h"
#include "SDL.h"
#include "SDL_joystick.h"
#include "dualsense_report.h"
#include "classic/hid_host.h"
#include "btapi.h"
#include "board_if.h"

static int initial_report;

extern int fft_getcolor(uint8_t *p);
extern void GetPlayerHealthColor(uint8_t *cval);
extern volatile DOOM_SCREEN_STATUS DoomScreenStatus;

static void DualSense_LVGL_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton);
static void DualSense_DOOM_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton);

static void (*HidProcTable[])(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton) = {
      DualSense_LVGL_Keycode,
      DualSense_DOOM_Keycode,
};

/* 0x08:  No button
 * 0x00:  Up
 * 0x01:  RightUp
 * 0x02:  Right
 * 0x03:  RightDown
 * 0x04:  Down
 * 0x05:  LeftDown
 * 0x06:  Left
 * 0x07:  LeftUp
 */

/*
 * @brief Hat key to Virtual button mask conversion table
 */
static const uint32_t hatmap[16] = {
  VBMASK_UP,   VBMASK_UP|VBMASK_RIGHT,  VBMASK_RIGHT, VBMASK_RIGHT|VBMASK_DOWN,
  VBMASK_DOWN, VBMASK_LEFT|VBMASK_DOWN, VBMASK_LEFT,  VBMASK_UP|VBMASK_LEFT,
  0, 0, 0, 0,
  0, 0, 0, 0,
};

#define	VBMASK_CHECK	(VBMASK_DOWN|VBMASK_RIGHT|VBMASK_LEFT| \
			 VBMASK_UP|VBMASK_PS|VBMASK_TRIANGLE| \
                         VBMASK_L1|VBMASK_R1| \
			 VBMASK_CIRCLE|VBMASK_CROSS|VBMASK_SQUARE)

#define	TRIG_FEEDBACK	0x21

void TriggerFeedbackSetup(uint8_t *dst, int position, int strength)
{
  if (position > 9 || position < 0)
    return;
  if (strength > 8 || strength < 0)
    return;
  if (strength > 0)
  {
    uint8_t forceValue = (strength - 1) & 0x07;
    uint32_t forceZones = 0;
    uint16_t activeZones = 0;

    for (int i = position; i < 10; i++)
    {
      forceZones |= (uint32_t)(forceValue << (3 * i));
      activeZones |= (uint16_t)(i << i);
    }
    dst[0] = TRIG_FEEDBACK;
    dst[1] = (activeZones & 0xff);
    dst[2] = (activeZones >> 8) & 0xff;
    dst[3] = (forceZones & 0xff);
    dst[4] = (forceZones >> 8) & 0xff;
    dst[5] = (forceZones >> 16) & 0xff;
    dst[6] = (forceZones >> 24) & 0xff;
    dst[7] = 0x00;
    dst[8] = 0x00;
    dst[9] = 0x00;
    dst[10] = 0x00;
  }
}

static void set_joystick_params()
{
  SDL_Joystick *joystick = SDL_GetJoystickPtr();

  joystick->name = "DualSence";
  joystick->naxes = 1;
  joystick->nhats = 1;
  joystick->nbuttons = NUM_VBUTTONS;
  joystick->hats = 0;
  joystick->buttons = 0;;
}

static uint8_t prev_blevel;

static void DualSenseBtDisconnect()
{
  prev_blevel = 0;
}

/*
 * Decode DualSense Input report
 */
static void decode_report(struct dualsense_input_report *rp, int hid_mode)
{
  uint8_t hat;
  uint32_t vbutton;
  uint8_t blevel;

  hat = (rp->buttons[0] & 0x0f);
  vbutton = (rp->buttons[2] & 0x0f) << 12;	/* PS, Touch, Mute */
  vbutton |= rp->buttons[1] << 4;		/* L1, R1, L2, R2, L3, R3, Create, Option */
  vbutton |= (rp->buttons[0] & 0xf0)>> 4;	/* Square, Cross, Circle, Triangle */
  vbutton |= hatmap[hat];

  HidProcTable[hid_mode](rp, hat, vbutton);

  if (rp->battery_level != prev_blevel)
  {
    blevel = rp->battery_level & 0x0F;
    debug_printf("Battery: 0x%02x\n", blevel);
    if (blevel > 9) blevel = 9;
    blevel = blevel / 2;
    postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_BATTERY | blevel, NULL, NULL);
    prev_blevel = rp->battery_level;
  }
}

static void process_bt_reports(uint8_t hid_mode);

static void DualSenseDecodeInputReport(HID_REPORT *report)
{
  struct dualsense_input_report *rp;
  static int dcount;

  if (report->len != DS_INPUT_REPORT_BT_SIZE)
  {
#if 0
    debug_printf("Bad report size..\n");
#endif
    return;
  }

  report->ptr += 1;
  rp = (struct dualsense_input_report *)report->ptr;


  if (rp->report_id != DS_INPUT_REPORT_BT)
  {
    debug_printf("Bad report ID. (%x) @ %x\n", rp->report_id, rp);
    return;
  }

  dcount++;

  if ((DoomScreenStatus == DOOM_SCREEN_SUSPEND) || (DoomScreenStatus == DOOM_SCREEN_SUSPENDED))
  {
    report->hid_mode = HID_MODE_LVGL;
    decode_report(rp, report->hid_mode);
  }
  else
  {
    decode_report(rp, report->hid_mode);
    process_bt_reports(report->hid_mode);
  }
}

static uint8_t bt_seq;

static void bt_out_init(struct dualsense_btout_report *rp)
{
  memset(rp, 0, sizeof(*rp));
  rp->report_id = DS_OUTPUT_REPORT_BT;
  rp->tag = DS_OUTPUT_TAG;
  rp->seq_tag = bt_seq << 4;
  bt_seq++;
  if (bt_seq >= 16)
    bt_seq = 0;
}

static void bt_emit_report(struct dualsense_btout_report *brp)
{
  int len;

  len = sizeof(*brp);
  brp->crc = bt_comp_crc((uint8_t *)brp, len);
  btapi_send_report((uint8_t *)brp, len);
}

static void SetBarColor(DS_OUTPUT_REPORT *rp, int seq, int mode)
{
  uint8_t cval[3];

  if (seq & 1)
  {
    rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;

    if (mode != HID_MODE_DOOM)
    {
      fft_getcolor(cval);
      rp->lightbar_red = cval[0];
      rp->lightbar_green = cval[1];
      rp->lightbar_blue = cval[2];
    }
    else
    {
      GetPlayerHealthColor(cval);
      rp->lightbar_red = cval[0];
      rp->lightbar_green = cval[1];
      rp->lightbar_blue = cval[2];
    }
  }
  else
  {
    rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE;
    if (mode != HID_MODE_DOOM)
    {
      int vval;

      vval = fft_getcolor(cval);
      if (vval > 250)
        rp->player_leds = 0x1f;		// Turn on all LEDs
      else if (vval > 150)
        rp->player_leds = 0x0e;
      else if (vval > 20)
        rp->player_leds = 0x04;		// Center only
      else
        rp->player_leds = 0;		// Turn off all LEDs
    }
    else
    {
      rp->player_leds = (1 << 2);
    }
  }
}

static struct dualsense_btout_report *get_report_buffer()
{
  struct dualsense_btout_report *brp;
  GAMEPAD_BUFFERS *gbp = GetGamePadBuffer();

  brp = (struct dualsense_btout_report *)(gbp->OutputReportBuffer);
  if (gbp->out_toggle)
    brp++;
  gbp->out_toggle ^= 1;
  return brp;
}

static int BtUpdateBarColor(uint8_t hid_mode)
{
  struct dualsense_btout_report *brp;

  brp = get_report_buffer();
  bt_out_init(brp);
  brp->report_id = DS_OUTPUT_REPORT_BT;

  SetBarColor(&brp->com_report, bt_seq, hid_mode);

  bt_emit_report(brp);
  return 1;
}

static void fill_output_request(int init_num, DS_OUTPUT_REPORT *rp)
{
  switch (init_num)
  {
    case 0:
      rp->valid_flag2 = DS_OUTPUT_VALID_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE;
      rp->lightbar_setup = DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT;
      break;
    case 1:
      rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_PLAYER_INDICATOR_CONTROL_ENABLE;
      rp->player_leds = 1 | (1 << 3);
      break;
    case 2:
      rp->valid_flag1 = DS_OUTPUT_VALID_FLAG1_LIGHTBAR_CONTROL_ENABLE;
      rp->lightbar_red = 0;
      rp->lightbar_green = 255;
      rp->lightbar_blue = 0;
      rp->led_brightness = 255;
      break;
    case 3:
      TriggerFeedbackSetup(rp->RightTriggerFFB,  3, 3);
      TriggerFeedbackSetup(rp->LeftTriggerFFB,  3, 3);
      rp->valid_flag0 = VALID_FLAG0_RIGHT_TRIGGER | VALID_FLAG0_LEFT_TRIGGER;
      break;
    default:
      break;
  }
}

static void process_bt_reports(uint8_t hid_mode)
{
  struct dualsense_btout_report *brp;
  DS_OUTPUT_REPORT *rp;

  if (initial_report < 4)
  {
    brp = get_report_buffer();
    bt_out_init(brp);
    rp = &brp->com_report;

    fill_output_request(initial_report, rp);
    initial_report++;

    bt_emit_report(brp);
    return;
  }
  BtUpdateBarColor(hid_mode);
}

static uint32_t last_button;
static int pad_timer;
static int16_t left_xinc, left_yinc;
static int16_t right_xinc, right_yinc;
extern lv_indev_data_t tp_data;

static void decode_tp(struct dualsense_input_report *rp)
{
  struct dualsense_touch_point *tp;
  uint16_t state, xpos, ypos;

  tp = rp->points;

  state = (tp->contact & 0x80)? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
  xpos = (tp->x_hi << 8) | (tp->x_lo);
  ypos = (tp->y_hi << 4) | (tp->y_lo);
  xpos = xpos * 480 / DS_TOUCHPAD_WIDTH;
  ypos = ypos * 320 / DS_TOUCHPAD_HEIGHT;

  if (tp_data.state != state)
  {
    if (state == LV_INDEV_STATE_PRESSED)
    {
      tp_data.state = state;
      gamepad_grab_owner();
    }
    else if (gamepad_is_owner())
    {
      tp_data.state = state;
      gamepad_ungrab_owner();
    }
  }
  if (state == LV_INDEV_STATE_PRESSED)
  {
      tp_data.point.x = xpos;
      tp_data.point.y = ypos;
  }
}

static void decode_stick(struct dualsense_input_report *rp)
{
  int ix, iy, ax, ay;

  ix = rp->x - 128;
  iy = rp->y - 128;
  ax = (ix < 0)? -ix : ix;
  ay = (iy < 0)? -iy : iy;
  ax = (ax > 80) ? 1 : 0;
  ay = (ay > 80) ? 1 : 0;

  if (ax != 0)
  {
    if (ix < 0) ax = -1;
    if (ax != left_xinc)
      postGuiEventMessage(GUIEV_LEFT_XDIR, ax, NULL, NULL);
  }
  left_xinc = ax;

  if (ay != 0)
  {
    if (iy < 0) ay = -1;
    if (ay != left_yinc)
      postGuiEventMessage(GUIEV_LEFT_YDIR, ay, NULL, NULL);
  }
  left_yinc = ay;

  ix = rp->rx - 128;
  iy = rp->ry - 128;
  ax = (ix < 0)? -ix : ix;
  ay = (iy < 0)? -iy : iy;
  ax = (ax > 80) ? 1 : 0;
  ay = (ay > 80) ? 1 : 0;

  if (ax != 0)
  {
    if (ix < 0) ax = -1;
    if (ax != right_xinc)
      postGuiEventMessage(GUIEV_RIGHT_XDIR, ax, NULL, NULL);
  }
  right_xinc = ax;

  if (ay != 0)
  {
    if (iy < 0) ay = -1;
    if (ay != right_yinc)
      postGuiEventMessage(GUIEV_RIGHT_YDIR, ay, NULL, NULL);
  }
  right_yinc = ay;
}

/**
 * @brief Convert HID input report to LVGL kaycode
 */
static void DualSense_LVGL_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton)
{
  UNUSED(hat);
  static lv_indev_data_t pad_data;

  if (vbutton != last_button)
  {
    uint32_t changed;
    const PADKEY_DATA *padkey = PadKeyDefs;

    changed = last_button ^ vbutton;
    changed &= VBMASK_CHECK;

    while (changed && padkey->mask)
    {
      if (changed & padkey->mask)
      {
        changed &= ~padkey->mask;

        pad_data.key = padkey->lvkey;
        pad_data.state = (vbutton & padkey->mask)? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        pad_data.continue_reading = (changed != 0)? true : false;
        pad_timer = 0;
        send_padkey(&pad_data);

      }
      padkey++;
    }
    last_button = vbutton;
  }
  else if (pad_data.state == LV_INDEV_STATE_PRESSED)
  {
    /* Key has been pressed */

    pad_timer++;
#ifdef USE_PAD_TIMER
    if (pad_timer > 15)		/* Takes repeat start delay */
    {
      if ((pad_timer & 3) == 0)	/* inter repeat delay passed */
      {
        /* Generate release and press event */
        pad_data.state = LV_INDEV_STATE_RELEASED;
        send_padkey(&pad_data);
        pad_data.state = LV_INDEV_STATE_PRESSED;
        send_padkey(&pad_data);
      }
    }
#endif
  }

  decode_stick(rp);
  decode_tp(rp);
}

void DualSenseBtSetup(uint16_t hid_host_cid)
{
  GAMEPAD_BUFFERS *gpb = GetGamePadBuffer();

  gpb->out_toggle = 0;

  set_joystick_params();
  hid_host_send_get_report(hid_host_cid, HID_REPORT_TYPE_FEATURE, DS_FEATURE_REPORT_CALIBRATION);
}

static const uint8_t sdl_hatmap[16] = {
  SDL_HAT_UP,       SDL_HAT_RIGHTUP,   SDL_HAT_RIGHT,    SDL_HAT_RIGHTDOWN,
  SDL_HAT_DOWN,     SDL_HAT_LEFTDOWN,  SDL_HAT_LEFT,     SDL_HAT_LEFTUP,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
};

static void DualSense_DOOM_Keycode(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton)
{
  if (DoomScreenStatus == DOOM_SCREEN_SUSPEND)
  {
    DualSense_LVGL_Keycode(rp, hat, vbutton);
  }
  else
  {
    SDL_JoyStickSetButtons(sdl_hatmap[hat], vbutton & 0x7FFF);
    decode_stick(rp);
    decode_tp(rp);
  }
}

const struct sGamePadDriver DualSenseDriver = {
  DualSenseDecodeInputReport,		// USB and BT
  DualSenseBtSetup,			// BT
  NULL,
  NULL,
  DualSenseBtDisconnect,		// BT
};
