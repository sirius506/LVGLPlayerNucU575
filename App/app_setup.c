#include "DoomPlayer.h"
#include "app_setup.h"
#include "btapi.h"
#include "board_if.h"
#include "audio_output.h"

/* Declare Bluetooth button images on the menu screen */

LV_IMG_DECLARE(bluetooth_black)			// Bluetooth is inactive
LV_IMG_DECLARE(bluetooth_blue)			// Bluetooth connected
LV_IMG_DECLARE(bluetooth_scan_black)		// Scanning gamepad devices..
LV_IMG_DECLARE(bluetooth_scan_blue)		// Scanning gamepad devices..

LV_IMG_DECLARE(volume_control)
LV_IMG_DECLARE(brightness)

extern osThreadId_t    doomtaskId;
extern volatile DOOM_SCREEN_STATUS DoomScreenStatus;

void UpdateBluetoothButton(SETUP_SCREEN *screen)
{
  BTSTACK_INFO *pinfo = screen->btinfo;

debug_printf("%s: %x\n", __FUNCTION__, pinfo->state);
  if (pinfo->state & BT_STATE_SCAN)
  {
    if (pinfo->state & (BT_STATE_HID_MASK|BT_STATE_A2DP_MASK))
      lv_imagebutton_set_src(screen->cont_bt, LV_IMAGEBUTTON_STATE_RELEASED,
          NULL, &bluetooth_scan_blue, NULL);
    else
      lv_imagebutton_set_src(screen->cont_bt, LV_IMAGEBUTTON_STATE_RELEASED,
          NULL, &bluetooth_scan_black, NULL);
  }
  else
  {
    if (pinfo->state & (BT_STATE_HID_MASK|BT_STATE_A2DP_MASK))
      lv_imagebutton_set_src(screen->cont_bt, LV_IMAGEBUTTON_STATE_RELEASED,
          NULL, &bluetooth_blue, NULL);
    else
      lv_imagebutton_set_src(screen->cont_bt, LV_IMAGEBUTTON_STATE_RELEASED,
          NULL, &bluetooth_black, NULL);
  }
}

static void bt_button_handler(lv_event_t *e)
{
  SETUP_SCREEN *screen = (SETUP_SCREEN *)lv_event_get_user_data(e);
  BTSTACK_INFO *pinfo;
  const lv_image_dsc_t *img = NULL;

  pinfo = screen->btinfo;

debug_printf("%s: %x\n", __FUNCTION__, pinfo->state);
  if ((pinfo->state & (BT_STATE_HID_MASK|BT_STATE_SCAN)) == 0)
  {
    btapi_start_scan();
    pinfo->state |= BT_STATE_SCAN;
    if (pinfo->state & BT_STATE_A2DP_MASK)
      img = &bluetooth_scan_blue;
    else
      img = &bluetooth_scan_black;
  }
  else if (pinfo->state & BT_STATE_SCAN)
  {
    btapi_stop_scan();
    pinfo->state &= ~BT_STATE_SCAN;
    if (pinfo->state & (BT_STATE_HID_MASK|BT_STATE_A2DP_MASK))
      img = &bluetooth_blue;
    else
      img = &bluetooth_black;
  }
  if (img)
  {
    lv_imagebutton_set_src(screen->cont_bt, LV_IMAGEBUTTON_STATE_RELEASED, NULL, img, NULL);
  }
}

/**
 * @brief Called when setup screen is invoked by gesture operation.
 */
void enter_setup_event(lv_event_t *e)
{
  SETUP_SCREEN *setups;
  void *param = lv_event_get_user_data(e);
  lv_dir_t dir;

  if (param == NULL)
  {
    dir = lv_indev_get_gesture_dir(lv_indev_active());
  }
  else
  {
    dir = (lv_dir_t) param;
  }
  HAL_DEVICE *haldev = (HAL_DEVICE *)&HalDevice;

  /* If Doom is running, request to suspend. */
  if (DoomScreenStatus == DOOM_SCREEN_ACTIVE)
  {
     DoomScreenStatus = DOOM_SCREEN_SUSPEND;
  }

  setups = &SetupScreen;
  if (dir == LV_DIR_BOTTOM)
  {
    setups->caller_ing = lv_indev_get_group(setups->keydev);
    lv_slider_set_value(setups->vol_slider,
          bsp_codec_getvol(haldev->codec_i2c) / 10, LV_ANIM_OFF);
    lv_screen_load_anim(setups->setup_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, false);
    lv_indev_set_group(setups->keydev, setups->ing);
  }
  else if (dir == LV_DIR_TOP)
  {
    if (setups->list_action)
    {
      (setups->list_action)(setups->arg_ptr);
    }
  }
}

/**
 * @brief Called when setup screen is invoked from Music Player
 */
void start_setup()
{
  HAL_DEVICE *haldev = (HAL_DEVICE *)&HalDevice;

  if (DoomScreenStatus == DOOM_SCREEN_ACTIVE)
  {
     DoomScreenStatus = DOOM_SCREEN_SUSPEND;
  }
  lv_slider_set_value(SetupScreen.vol_slider,
          bsp_codec_getvol(haldev->codec_i2c) / 10, LV_ANIM_OFF);
  lv_screen_load_anim(SetupScreen.setup_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, false);
}

static void quit_setup_event(lv_event_t *e)
{
  UNUSED(e);
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

  if (dir == LV_DIR_TOP)
  {
    lv_indev_set_group(SetupScreen.keydev, SetupScreen.caller_ing);
    lv_screen_load_anim(SetupScreen.active_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
  }
}

static void vol_event_cb(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target(e);
  AUDIO_CONF *aconf = (AUDIO_CONF *)lv_event_get_user_data(e);
  int32_t v;

  v = lv_slider_get_value(obj) * 10;
  aconf->devconf->pDriver->SetVolume(aconf, v);
}

static void brightness_event_cb(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target(e);
  HAL_DEVICE *haldev = (HAL_DEVICE *)lv_event_get_user_data(e);

  int32_t v = lv_slider_get_value(obj) * 10;
  Board_Set_Brightness(haldev, v);
}

/**
 *  @brif Resume DOOM task when setup screen has unloaded.
 */
static void setupscr_event_cb(lv_event_t *ev)
{
  UNUSED(ev);

  if (DoomScreenStatus == DOOM_SCREEN_SUSPEND)
  {
    DoomScreenStatus = DOOM_SCREEN_ACTIVE;
    if (doomtaskId) osThreadResume(doomtaskId);
  }
}

static void process_hid_disc(lv_event_t *ev)
{
  UNUSED(ev);

  btapi_hid_disconnect();
}

static void process_a2dp_disc(lv_event_t *ev)
{
  UNUSED(ev);

  btapi_a2dp_disconnect();
}

lv_obj_t *setup_screen_create(SETUP_SCREEN *setups, HAL_DEVICE *haldev, lv_indev_t *keydev)
{
  lv_obj_t *label;
  lv_obj_t *bar, *img;
  lv_obj_t *scr = lv_obj_create(NULL);
  static lv_style_t style_knob;
  static lv_style_t style_setup;
  static lv_style_t style_bar;
  static lv_style_t style_indicator;

  lv_obj_set_size(scr, LV_HOR_RES, LV_VER_RES);
  lv_style_init(&style_setup);
  lv_style_set_bg_color(&style_setup, lv_color_hex3(0x777777));
  lv_obj_add_style(scr, &style_setup, 0);
  setups->setup_screen = scr;
  lv_obj_add_event_cb(scr, setupscr_event_cb, LV_EVENT_SCREEN_UNLOADED, haldev);

  lv_style_init(&style_bar);
  lv_style_init(&style_indicator);
  lv_style_set_bg_color(&style_bar, lv_color_hex3(0xEEEEEE));
  lv_style_set_bg_color(&style_indicator, lv_color_hex3(0xFFFFFF));

  lv_style_init(&style_knob);
  lv_style_set_radius(&style_knob, 5);
  lv_style_set_bg_color(&style_knob, lv_color_hex3(0xFFFFFF));

  bar = lv_slider_create(scr);
  lv_obj_set_size(bar, 25, 200);
  lv_obj_align(bar, LV_ALIGN_LEFT_MID, lv_pct(50), -15);
  lv_obj_add_style(bar, &style_knob, LV_PART_KNOB);
  lv_obj_add_style(bar, &style_bar, LV_PART_MAIN);
  lv_obj_add_style(bar, &style_indicator, LV_PART_INDICATOR);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_GESTURE_BUBBLE);
  setups->vol_slider = bar;
  lv_slider_set_range(setups->vol_slider, 0, 10);
  lv_obj_add_event_cb(setups->vol_slider, vol_event_cb, LV_EVENT_VALUE_CHANGED, get_audio_config(haldev));
  img = lv_image_create(scr);
  lv_image_set_src(img, &volume_control);
  lv_obj_align_to(img, bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

  bar = lv_slider_create(scr);
  lv_obj_set_size(bar, 25, 200);
  lv_obj_align(bar, LV_ALIGN_LEFT_MID, lv_pct(75), -15);
  lv_obj_add_style(bar, &style_knob, LV_PART_KNOB);
  lv_obj_add_style(bar, &style_bar, LV_PART_MAIN);
  lv_obj_add_style(bar, &style_indicator, LV_PART_INDICATOR);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_GESTURE_BUBBLE);
  setups->bright_slider = bar;
  lv_slider_set_range(setups->bright_slider, 0, 10);
  lv_slider_set_value(setups->bright_slider, Board_Get_Brightness() / 10, LV_ANIM_OFF);
  lv_obj_add_event_cb(setups->bright_slider, brightness_event_cb, LV_EVENT_VALUE_CHANGED, haldev);
  img = lv_image_create(scr);
  lv_image_set_src(img, &brightness);
  lv_obj_align_to(img, bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

  setups->cont_bt = lv_imagebutton_create(scr);
  lv_imagebutton_set_src(setups->cont_bt, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &bluetooth_black, NULL);
  lv_obj_align(setups->cont_bt, LV_ALIGN_LEFT_MID, lv_pct(20), -50);
  lv_obj_set_width(setups->cont_bt, LV_SIZE_CONTENT);
  lv_obj_add_event_cb(setups->cont_bt, bt_button_handler, LV_EVENT_CLICKED, setups);
  lv_obj_add_flag(setups->cont_bt, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_event_cb(setups->setup_screen, quit_setup_event, LV_EVENT_GESTURE, NULL);

  setups->hid_btn = lv_button_create(scr);
  lv_obj_add_flag(setups->hid_btn, LV_OBJ_FLAG_HIDDEN);
  label = lv_label_create(setups->hid_btn);
  lv_label_set_text_static(label, "HID");
  lv_obj_update_layout(setups->hid_btn);
  lv_obj_align_to(setups->hid_btn, setups->cont_bt, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
  lv_obj_add_event_cb(setups->hid_btn, process_hid_disc, LV_EVENT_CLICKED, NULL);

  setups->a2dp_btn = lv_button_create(scr);
  lv_obj_add_flag(setups->a2dp_btn, LV_OBJ_FLAG_HIDDEN);
  label = lv_label_create(setups->a2dp_btn);
  lv_label_set_text_static(label, "A2DP");
  lv_obj_update_layout(setups->a2dp_btn);
  lv_obj_align_to(setups->a2dp_btn, setups->cont_bt, LV_ALIGN_OUT_BOTTOM_MID, 0, 70);
  lv_obj_add_event_cb(setups->a2dp_btn, process_a2dp_disc, LV_EVENT_CLICKED, NULL);

  setups->keydev = keydev;
  setups->ing = lv_group_create();
  lv_group_add_obj(setups->ing, setups->vol_slider);
  lv_group_add_obj(setups->ing, setups->bright_slider);
  return scr;
}
