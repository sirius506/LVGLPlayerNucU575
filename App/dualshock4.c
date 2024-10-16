/**
 * @brief SONY DUALSHOCK 4 Controller driver
 */
#include "DoomPlayer.h"
#include "lvgl.h"
#include "app_gui.h"
#include "gamepad.h"
#include "classic/hid_host.h"
#include "SDL.h"
#include "SDL_joystick.h"
#include "btapi.h"

#define	SAMPLE_PERIOD	(0.00250f)
#define	SAMPLE_RATE	(400)

extern int fft_getcolor(uint8_t *p);
extern void GetPlayerHealthColor(uint8_t *cval);

static void DS4_LVGL_Keycode(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep);
static void DualShock_DOOM_Keycode(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep);

static const void (*ds4HidProcTable[])(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep) = {
      DS4_LVGL_Keycode,
      NULL,
      DualShock_DOOM_Keycode,
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

static void set_joystick_params()
{
  SDL_Joystick *joystick = SDL_GetJoystickPtr();

  joystick->name = "DualShock";
  joystick->naxes = 1;
  joystick->nhats = 1;
  joystick->nbuttons = NUM_VBUTTONS;
  joystick->hats = 0;
  joystick->buttons = 0;
}

void process_output_report(int hid_mode)
{
  struct ds4_bt_output_report *brp;
  struct ds4_output_report *rp;
  GAMEPAD_BUFFERS *gpb = GetGamePadBuffer();
  uint8_t cval[3];

  if (gpb->out_toggle)
    brp = (struct ds4_bt_output_report *)(gpb->OutputReportBuffer + sizeof(struct ds4_bt_output_report));
  else
    brp = (struct ds4_bt_output_report *)gpb->OutputReportBuffer;

  memset(brp, 0, sizeof(*brp));
  brp->report_id = DS4_OUTPUT_REPORT_BT;
  brp->hw_control = DS4_OUTPUT_HWCTL_HID | DS4_OUTPUT_HWCTL_CRC32;
  rp = &brp->out_report;

  gpb->out_toggle ^= 1;

  brp->report_id = DS4_OUTPUT_REPORT_BT;
  rp->valid_flag0 = DS4_OUTPUT_VALID_FLAG0_LED;

  if (hid_mode != HID_MODE_DOOM)
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

  int len;

  len = sizeof(struct ds4_bt_output_report);
  brp->crc = bt_comp_crc((uint8_t *)brp, len);
  btapi_send_report((uint8_t *)brp, len);
}

static uint8_t prev_blevel;

static void DualShockBtDisconnect()
{
  prev_blevel = 0;
}

/*
 * Decode DualShock Input report
 */
static void DualShockDecodeInputReport(HID_REPORT *report)
{
  DS4_INPUT_REPORT *rp;
  static uint32_t in_seq;
  uint8_t blevel;
  uint8_t ctype;
  static int dcount;

  if (report->len != DS4_INPUT_REPORT_BT_SIZE)
    return;

  report->ptr++;

  ctype = report->ptr[0];
  if (ctype != DS4_INPUT_REPORT_BT)
    return;

  rp = (DS4_INPUT_REPORT *)(report->ptr + 3);
 
  if (ctype != DS4_INPUT_REPORT_BT)
    return;

  dcount++;

  {
    uint8_t hat;
    uint32_t vbutton;

    hat = (rp->buttons[0] & 0x0f);
    vbutton = (rp->buttons[2] & 0x03) << 12;	/* Home, Pad */
    vbutton |= rp->buttons[1] << 4;		/* L1, R1, L2, R2, Share, Option, L3, R3 */
    vbutton |= (rp->buttons[0] & 0xf0)>> 4;	/* Square, Cross, Circle, Triangle */
    vbutton |= hatmap[hat];

    ds4HidProcTable[report->hid_mode](rp, hat, vbutton, report);
  }

  if ((rp->status[0] & 0x0F) != prev_blevel)
  {
    blevel = rp->status[0] & 0x0F;
    if (blevel > 9) blevel = 9;
    blevel = blevel / 2;
debug_printf("Battery: 0x%02x, %d\n", rp->status[0], blevel);
    postGuiEventMessage(GUIEV_ICON_CHANGE, ICON_BATTERY | blevel, NULL, NULL);
    prev_blevel = rp->status[0] & 0x0F;
  }

  if (report->ptr[0] == DS4_INPUT_REPORT_BT)
  {
#define ENABLE_OUTPUT_REPORT
#ifdef ENABLE_OUTPUT_REPORT
    if ((in_seq & 3) == 0)
      process_output_report(report->hid_mode);
#endif
    in_seq++;
  }
}

static uint32_t last_button;
static int pad_timer;
static int16_t left_xinc, left_yinc;
static int16_t right_xinc, right_yinc;
extern lv_indev_data_t tp_data;

static void decode_tp(struct ds4_input_report *rp, HID_REPORT *rep)
{
  UNUSED(rp);
  struct ds4_bt_input_report *bt_rep;
  struct ds4_touch_report *tp;
  int num_report;
  uint16_t state, xpos, ypos;

  if (rep->ptr[0] != DS4_INPUT_REPORT_BT)
    return;

  bt_rep = (struct ds4_bt_input_report *)rep->ptr;
  num_report = bt_rep->num_touch_reports;
  tp = bt_rep->reports;

  if (num_report > 0 && tp)
  {
    state = (tp->points[0].contact & 0x80)? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
    xpos = (tp->points[0].x_hi << 8) | (tp->points[0].x_lo);
    ypos = (tp->points[0].y_hi << 4) | (tp->points[0].y_lo);
    xpos = xpos * 480 / DS4_TOUCHPAD_WIDTH;
    ypos = ypos * 320 / DS4_TOUCHPAD_HEIGHT;
  }
  else
  {
    state = LV_INDEV_STATE_RELEASED;
  }
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

static void decode_stick(struct ds4_input_report *rp)
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
    {
      postGuiEventMessage(GUIEV_LEFT_XDIR, ax, NULL, NULL);
    }
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
static void DS4_LVGL_Keycode(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep)
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
  decode_tp(rp, rep);
}

static const uint8_t sdl_hatmap[16] = {
  SDL_HAT_UP,       SDL_HAT_RIGHTUP,   SDL_HAT_RIGHT,    SDL_HAT_RIGHTDOWN,
  SDL_HAT_DOWN,     SDL_HAT_LEFTDOWN,  SDL_HAT_LEFT,     SDL_HAT_LEFTUP,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
  SDL_HAT_CENTERED, SDL_HAT_CENTERED,  SDL_HAT_CENTERED, SDL_HAT_CENTERED,
};

static void DualShock_DOOM_Keycode(struct ds4_input_report *rp, uint8_t hat, uint32_t vbutton, HID_REPORT *rep)
{
  UNUSED(rep);
  SDL_JoyStickSetButtons(sdl_hatmap[hat], vbutton & 0x7FFF);
  decode_stick(rp);
  decode_tp(rp, rep);
}

void DualShockBtSetup(uint16_t hid_host_cid)
{
  GAMEPAD_BUFFERS *gpb = GetGamePadBuffer();

  gpb->out_toggle = 0;

  set_joystick_params();

  hid_host_send_get_report(hid_host_cid, HID_REPORT_TYPE_FEATURE, DS4_FEATURE_REPORT_CALIBRATION_BT);
}

const struct sGamePadDriver DualShockDriver = {
  DualShockDecodeInputReport,		// USB and BT
  DualShockBtSetup,			// BT
  NULL,					// BT
  NULL,
  DualShockBtDisconnect,		// BT
};

