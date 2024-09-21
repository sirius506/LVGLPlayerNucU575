/**
 *   Game controller interface
 */
#include "DoomPlayer.h"
#include "gamepad.h"
#ifdef USE_FUSION
#include "Fusion.h"
#endif

static GAMEPAD_INFO GamePadInfo;

#ifdef USE_FUSION
static FusionAhrs ahrs;
static FusionOffset hoffset;

FUSION_ANGLE ImuAngle;

/* Variables for Analog stick */
static lv_point_precise_t points_left[2] =  { {JOY_RAD, JOY_RAD}, {JOY_RAD, JOY_RAD} };
static lv_point_precise_t points_right[2] = { {JOY_RAD, JOY_RAD}, {JOY_RAD, JOY_RAD} };
static lv_style_t stick_style;

static lv_point_precise_t points_pitch[2] = { {0, 50}, { 100, 50}};
#endif

/**
 * @brief List of supported gamepads.
 */
const struct sGamePad KnownGamePads[] = {
#ifdef USE_PAD_IMAGE
  { VID_SONY, PID_DUALSENSE, "DualSense",     &DualSenseDriver, &DualSenseImage },
  { VID_SONY, PID_DUALSHOCK, "DualShock4",    &DualShockDriver, &DualShock4Image },
  { VID_SONY, PID_ZERO2,     "8BitDo Zero 2", &Zero2Driver, &Zero2Image },
#else
  { VID_SONY, PID_DUALSENSE, "DualSense",     &DualSenseDriver, NULL },
  { VID_SONY, PID_DUALSHOCK, "DualShock4",    &DualShockDriver, NULL },
  { VID_SONY, PID_ZERO2,     "8BitDo Zero 2", &Zero2Driver, NULL },
#endif
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
      GamePadInfo.padImage = gp->padImage;
      return &GamePadInfo;
    }
    gp++;
  }
  debug_printf("This gamepad is not supported.\n");
  return NULL;
}

void GamepadHidMode(GAMEPAD_INFO *padInfo, int mode)
{
  padInfo->hid_mode = mode;
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

  crcval = HAL_CRC_Calculate(&hcrc, (uint32_t *)output_seed, 1);
  crcval = HAL_CRC_Accumulate(&hcrc, (uint32_t *)ptr, len - 4);
  crcval = ~crcval;
  return crcval;
}

#ifdef USE_FUSION
void setup_fusion(int sample_rate, const FusionAhrsSettings *psettings)
{
  FusionOffsetInitialise(&hoffset, sample_rate);
  FusionAhrsInitialise(&ahrs);

  FusionAhrsSetSettings(&ahrs, psettings);
}

void gamepad_reset_fusion() 
{
  FusionAhrsReset(&ahrs);
}

void gamepad_process_fusion(float sample_period, FusionVector gyroscope, FusionVector accelerometer)
{
  FusionEuler euler;
#ifdef XDEBUG
  static int seq_number;
#endif

  // Apply calibration
  gyroscope = FusionCalibrationInertial(gyroscope, FUSION_IDENTITY_MATRIX, FUSION_VECTOR_ONES, FUSION_VECTOR_ZERO );
  accelerometer = FusionCalibrationInertial(accelerometer, FUSION_IDENTITY_MATRIX, FUSION_VECTOR_ONES, FUSION_VECTOR_ZERO );

  gyroscope = FusionOffsetUpdate(&hoffset, gyroscope);
  FusionAhrsUpdateNoMagnetometer(&ahrs, gyroscope, accelerometer, sample_period);

  if (ahrs.initialising == 0)
  {
    euler  = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));

#ifdef XDEBUG
seq_number++;
    if ((seq_number & 0x01FF) == 0)
    {
      debug_printf("%d, %d, %d\n", (int)euler.angle.roll, (int)euler.angle.pitch, (int)euler.angle.yaw);
#ifdef SHOW_EULER
      debug_printf("%d, %d, %d\n", (int)euler.angle.roll, (int)euler.angle.pitch, (int)euler.angle.yaw);
#endif
#ifdef SHOW_ACCEL
      debug_printf("A: %d, %d, %d\n",
        (int)(accelerometer.array[0] * 1000.0f),        // X
        (int)(accelerometer.array[1] * 1000.0f),        // Z
        (int)(accelerometer.array[2] * 1000.0f));       // Y
#endif
#ifdef SHOW_GYRO
      debug_printf("G: %d, %d, %d\n",
        (int)(gyroscope.array[0] * 10.0f),      // X
        (int)(gyroscope.array[1] * 10.0f),      // Z
        (int)(gyroscope.array[2] * 10.0f));     // Y
#endif
      //debug_printf("Temp = %x (%d) @ %d\n", rp->Temperature, rp->Temperature, rp->timestamp);
    }
#endif

/*
 * Roll: 0 --> -90 --> -180, 180 -> 90 --> 0  -180 .. 180
 * Pitch: 0 --> 90 --> 0 --> -90 --> 0 -90 -90 .. 90
 * Yaw:   0 -> 90 --> 180, -180 --> 90 --> 0
 */
   ImuAngle.roll = (int16_t) euler.angle.roll;
   ImuAngle.pitch = (int16_t) euler.angle.pitch;
   ImuAngle.yaw = (int16_t) euler.angle.yaw;

  }
}
#endif

#if 0
FS_DIRENT *find_flash_file(char *name)
{
  int i;
  FS_DIRENT *dirent;
  QSPI_DIRHEADER *dirInfo = (QSPI_DIRHEADER *)QSPI_FLASH_ADDR;

  dirent = dirInfo->fs_direntry;

  for (i = 1; i < NUM_DIRENT; i++)
  {
    if (dirent->foffset == 0xFFFFFFFF)
      return 0; 
    if (strncasecmp(dirent->fname, name, strlen(name)) == 0)
    {
      return dirent;
    }
    dirent++;
  }
  return NULL;
}
#endif

#ifdef USE_FUSION
void padtest_done()
{
  GUI_EVENT ev;

  ev.evcode = GUIEV_PADTEST_DONE;
  ev.evval0 = 0;
  ev.evarg1 = NULL;
  ev.evarg2 = NULL;
  postGuiEvent(&ev);
}

static lv_image_dsc_t padimgdesc;

lv_obj_t *padtest_create(GAMEPAD_INFO *padInfo, lv_group_t *g)
{
  lv_obj_t *scr;
  uint32_t faddr;
  FS_DIRENT *dirent;
  int i;
  const struct sGamePadImage *padImage = padInfo->padImage;
  const GUI_LAYOUT *layout = &GuiLayout;

  scr = lv_obj_create(NULL);

  lv_obj_set_size(scr, DISP_HOR_RES, DISP_VER_RES);

  /* Find gamepad photo image */

  padInfo->img = lv_image_create(scr);
  lv_obj_remove_style_all(padInfo->img);
  dirent = find_flash_file(padImage->ibin_name);
  if (dirent)
  {
      faddr = (uint32_t)(QSPI_FLASH_ADDR + dirent->foffset);
      memcpy(&padimgdesc.header, (void *)faddr, sizeof(padimgdesc));
      padimgdesc.data = (const uint8_t *)(faddr + 12);
      padimgdesc.data_size = dirent->fsize - 12;
      lv_image_set_src(padInfo->img, &padimgdesc);
      lv_obj_set_width(padInfo->img, padImage->ibin_width);
      lv_obj_set_height(padInfo->img, padImage->ibin_height);
      lv_obj_update_layout(padInfo->img);

      lv_obj_align(padInfo->img, LV_ALIGN_LEFT_MID, 0, 0);
  }

  /* Create touch pad points indicator */

  padInfo->pad1 = lv_led_create(padInfo->img);
  lv_obj_set_size(padInfo->pad1, layout->led_rad, layout->led_rad);
  lv_obj_add_flag(padInfo->pad1, LV_OBJ_FLAG_HIDDEN);

  padInfo->pad2 = lv_led_create(padInfo->img);
  lv_obj_set_size(padInfo->pad2, layout->led_rad, layout->led_rad);
  lv_obj_add_flag(padInfo->pad2, LV_OBJ_FLAG_HIDDEN);

  /* Create LED widgets for all available buttons */
  const lv_point_t *ppos = padImage->ButtonPositions;

  for (i = 0; i < NUM_PAD_BUTTONS; i++)
  {
    lv_obj_t *pled;

    if (ppos->x)
    {
      pled = lv_led_create(padInfo->img);
      lv_obj_set_size(pled, layout->led_rad, layout->led_rad);
      lv_obj_add_flag(pled, LV_OBJ_FLAG_HIDDEN);
      lv_led_set_color(pled, lv_palette_main(LV_PALETTE_RED));
      lv_obj_align(pled, LV_ALIGN_TOP_LEFT, ppos->x, ppos->y);

      padInfo->ButtonLeds[i] = pled;
      ppos++;
    }
  }

  if (padImage->image_flag & PADIMG_STICK)
  {
    lv_style_init(&stick_style);
    lv_style_set_line_width(&stick_style, layout->line_width);
    lv_style_set_line_color(&stick_style, lv_palette_main(LV_PALETTE_BLUE));

    padInfo->joyleft = lv_line_create(padInfo->img);
    lv_obj_set_size(padInfo->joyleft, JOY_DIA, JOY_DIA);
    lv_obj_add_style(padInfo->joyleft, &stick_style, 0);
    lv_obj_align(padInfo->joyleft, LV_ALIGN_TOP_LEFT, padImage->lstick_x - JOY_RAD, padImage->stick_y);
    padInfo->joyright = lv_line_create(padInfo->img);
    lv_obj_set_size(padInfo->joyright, JOY_DIA, JOY_DIA);
    lv_obj_add_style(padInfo->joyright, &stick_style, 0);
    lv_obj_align(padInfo->joyright, LV_ALIGN_TOP_LEFT, padImage->rstick_x - JOY_RAD, padImage->stick_y);
  }

  if (padImage->image_flag & PADIMG_IMU)
  {
    padInfo->roll_bar = lv_bar_create(scr);
    lv_obj_set_size(padInfo->roll_bar, layout->bar_width, layout->bar_height);
    lv_obj_align(padInfo->roll_bar, LV_ALIGN_BOTTOM_RIGHT, layout->bar_xpos, layout->bar_ypos);
    lv_bar_set_mode(padInfo->roll_bar, LV_BAR_MODE_RANGE);
    lv_bar_set_start_value(padInfo->roll_bar, 50, LV_ANIM_OFF);
    lv_bar_set_value(padInfo->roll_bar, 30, LV_ANIM_OFF);
    lv_bar_set_range(padInfo->roll_bar, 0, 100);

    padInfo->pitch = lv_line_create(scr);
    lv_obj_set_size(padInfo->pitch, layout->bar_height, layout->bar_height);
    lv_obj_add_style(padInfo->pitch, &stick_style, 0);
    lv_obj_align(padInfo->pitch, LV_ALIGN_BOTTOM_RIGHT, -8, layout->bar_ypos);

    padInfo->yaw = lv_arc_create(scr);
    lv_obj_set_size(padInfo->yaw, layout->bar_height, layout->bar_height);
    lv_obj_remove_flag(padInfo->yaw, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(padInfo->yaw, LV_ALIGN_BOTTOM_RIGHT, -8, layout->bar_ypos);
    lv_arc_set_bg_angles(padInfo->yaw, 0, 360);
    lv_arc_set_mode(padInfo->yaw, LV_ARC_MODE_NORMAL);
    lv_arc_set_angles(padInfo->yaw, 0, 0);
    lv_obj_set_style_arc_width(padInfo->yaw, layout->line_width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(padInfo->yaw, layout->line_width * 2, LV_PART_INDICATOR);
    lv_obj_set_style_pad_top(padInfo->yaw, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_bottom(padInfo->yaw, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_left(padInfo->yaw, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_right(padInfo->yaw, 0, LV_PART_KNOB);
  }

  lv_obj_t *home, *label;

  home = lv_btn_create(scr);
  lv_obj_set_size(home, W_PERCENT(21), H_PERCENT(12));
  lv_obj_align(home, LV_ALIGN_BOTTOM_RIGHT, -8, -20);
  lv_obj_add_event_cb(home, padtest_done, LV_EVENT_CLICKED, NULL);
  lv_obj_set_style_bg_color(home, lv_palette_main(LV_PALETTE_ORANGE), 0);
  label = lv_label_create(home);
  lv_label_set_text(label, LV_SYMBOL_HOME);
  lv_obj_center(label);

  return scr;
}
#endif

#if 0
#define	TP_XSCALE(padImage, xpos) ((xpos *  padImage->tpad_width / padImage->tpad_hor_res) + padImage->tpad_xpos)
#define	TP_YSCALE(padImage, ypos) ((ypos *  padImage->tpad_height / padImage->tpad_ver_res) + padImage->tpad_ypos)

void padtest_update(GAMEPAD_INFO *padInfo, struct gamepad_inputs *rp, uint32_t vbutton)
{
  const GUI_LAYOUT *layout = &GuiLayout;
  const struct sGamePadImage *padImage = padInfo->padImage;

  struct gamepad_touch_point *tp;
  int xpos, ypos;
  int i;
  int mask = 1;
#ifdef USE_FUSION
  int16_t angle;
#endif
  lv_obj_t *led;

  if (padInfo->padImage == NULL)
    return;

  /* Draw Button LED status */

  for ( i = 0; i < NUM_PAD_BUTTONS; i++)
  {
    led = padInfo->ButtonLeds[i];
    int size;

    if (vbutton & mask)
    {
      lv_obj_remove_flag(led, LV_OBJ_FLAG_HIDDEN);
      if (i == 6)
      {
        size = rp->z / 10;
        if (size < 3) size = 3;
        lv_obj_set_size(led, size, size);
      }
      else if (i == 7)
      {
        size = rp->rz / 10;
        if (size < 3) size = 3;
        lv_obj_set_size(led, size, size);
      }
    }
    else
    {
      lv_obj_add_flag(led, LV_OBJ_FLAG_HIDDEN);
    }
    mask <<= 1;
  }

  if (padImage->image_flag & PADIMG_TPAD)
  {
    /* Draw touch pad position */

    tp = &rp->points[0];		// first point
    if (tp->contact & 0x80)
    {
      lv_obj_add_flag(padInfo->pad1, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      xpos = TP_XSCALE(padImage, tp->xpos);
      ypos = TP_YSCALE(padImage, tp->ypos);
      lv_obj_align(padInfo->pad1, LV_ALIGN_TOP_LEFT, xpos, ypos);
      lv_obj_remove_flag(padInfo->pad1, LV_OBJ_FLAG_HIDDEN);
    }
    tp = &rp->points[1];		// second point
    if (tp->contact & 0x80)
    {
      lv_obj_add_flag(padInfo->pad2, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      xpos = TP_XSCALE(padImage, tp->xpos);
      ypos = TP_YSCALE(padImage, tp->ypos);
      lv_obj_align(padInfo->pad2, LV_ALIGN_TOP_LEFT, xpos, ypos);
      lv_obj_remove_flag(padInfo->pad2, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (padImage->image_flag & PADIMG_STICK)
  {
    /* Draw left and right joystick bars */

// debug_printf("(%d, %d, %d)\n", rp->x, rp->y, rp->z);  // 0..256
    points_left[1].x = rp->x / layout->joy_divisor;
    points_left[1].y = rp->y / layout->joy_divisor;
    lv_line_set_points(padInfo->joyleft, points_left, 2);
    points_right[1].x = rp->rx / layout->joy_divisor;
    points_right[1].y = rp->ry / layout->joy_divisor;
    lv_line_set_points(padInfo->joyright, points_right, 2);
  }

#ifdef USE_FUSION
  if (padImage->image_flag & PADIMG_IMU)
  {
    /* Draw IMU angles */

    /*
     * Roll:  0 --> -90 --> -180, 180 -> 90 --> 0  -180 .. 180
     * Pitch: 0 --> 90 --> 0 --> -90 --> 0 -90 -90 .. 90
     * Yaw:   0 -> 90 --> 180, -180 --> -90 --> 0
     */

    angle = ImuAngle.roll;
    points_pitch[0].x = (layout->bar_height/2) + ((layout->bar_height/2) * lv_trigo_cos(angle))/32768;
    points_pitch[0].y = (layout->bar_height/2) + ((layout->bar_height/2) * lv_trigo_sin(angle))/32768;
    points_pitch[1].x = (layout->bar_height/2) - ((layout->bar_height/2) * lv_trigo_cos(angle))/32768;
    points_pitch[1].y = (layout->bar_height/2) - ((layout->bar_height/2) * lv_trigo_sin(angle))/32768;
    lv_line_set_points(padInfo->pitch, points_pitch, 2);

    int val;

    angle = ImuAngle.pitch;
    val = 50 + (50 * lv_trigo_sin(angle)/32768);
    if (val >= 50)
    {
      lv_bar_set_start_value(padInfo->roll_bar, 50, LV_ANIM_OFF);
      lv_bar_set_value(padInfo->roll_bar, val, LV_ANIM_OFF);
    }
    else
    {
      lv_bar_set_start_value(padInfo->roll_bar, val, LV_ANIM_OFF);
      lv_bar_set_value(padInfo->roll_bar, 50, LV_ANIM_OFF);
    }
  

    angle = 360 - ImuAngle.yaw;
    angle -= 90;
    if (angle < 0)
      angle += 360;

    lv_arc_set_angles(padInfo->yaw, angle, angle);
  }
#endif
}
#endif
