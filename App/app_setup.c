#include "DoomPlayer.h"
#include "app_setup.h"
#include "btapi.h"
#include "board_if.h"

/* Declare Bluetooth button images on the menu screen */

LV_IMG_DECLARE(bluetooth_black)			// Bluetooth is inactive
LV_IMG_DECLARE(bluetooth_blue)			// Bluetooth connected
LV_IMG_DECLARE(bluetooth_scan)			// Scanning gamepad devices..

LV_IMG_DECLARE(volume_control)
LV_IMG_DECLARE(brightness)

extern osThreadId_t    doomtaskId;
extern volatile DOOM_SCREEN_STATUS DoomScreenStatus;

void SetBluetoothButtonState(BT_BUTTON_INFO *binfo, BT_STATE new_state)
{
  binfo->bst = new_state;

  switch (new_state)
  {
  case BT_STATE_READY:
    lv_imagebutton_set_src(binfo->btn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &bluetooth_black, NULL);
    break;
  case BT_STATE_CONNECT:
    lv_imagebutton_set_src(binfo->btn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &bluetooth_blue, NULL);
    break;
  case BT_STATE_SCAN:
    lv_imagebutton_set_src(binfo->btn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &bluetooth_scan, NULL);
    break;
  default:
    break;
  }
}

static void bt_button_handler(lv_event_t *e)
{
  BT_BUTTON_INFO *binfo = (BT_BUTTON_INFO *)lv_event_get_user_data(e);
  const lv_image_dsc_t *img = NULL;

  switch (binfo->bst)
  {
  case BT_STATE_READY:
    btapi_start_scan();
    binfo->bst = BT_STATE_SCAN;
    img = &bluetooth_scan;
    break;
  case BT_STATE_SCAN:
    btapi_stop_scan();
    binfo->bst = BT_STATE_READY;
    img = &bluetooth_black;
    break;
  case BT_STATE_CONNECT:
    btapi_disconnect();
    binfo->bst = BT_STATE_READY;
    img = &bluetooth_blue;
    break;
  default:
    break;
  }
  if (img)
  {
    lv_imagebutton_set_src(binfo->btn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, img, NULL);
  }
}

/**
 * @brief Called when setup screen is invoked by gesture operation.
 */
void enter_setup_event(lv_event_t *e)
{
  UNUSED(e);
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
  HAL_DEVICE *haldev = (HAL_DEVICE *)&HalDevice;

  if (DoomScreenStatus == DOOM_SCREEN_ACTIVE)
  {
     DoomScreenStatus = DOOM_SCREEN_SUSPEND;
  }
  if (dir == LV_DIR_BOTTOM)
  {
    lv_slider_set_value(SetupScreen.vol_slider,
          bsp_codec_getvol(haldev->codec_i2c), LV_ANIM_OFF);
    lv_screen_load_anim(SetupScreen.setup_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, false);
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
          bsp_codec_getvol(haldev->codec_i2c), LV_ANIM_OFF);
  lv_screen_load_anim(SetupScreen.setup_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, false);
}

void quit_setup_event(lv_event_t *e)
{
  UNUSED(e);
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

  if (dir == LV_DIR_TOP)
  {
    lv_screen_load_anim(SetupScreen.active_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
  }
}

void activate_screen(lv_obj_t *screen)
{
  lv_screen_load(screen);
  lv_obj_add_event_cb(screen, enter_setup_event, LV_EVENT_GESTURE, NULL);
  SetupScreen.active_screen = screen;
}

static void vol_event_cb(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target(e);
  HAL_DEVICE *haldev = (HAL_DEVICE *)lv_event_get_user_data(e);
  int32_t v;

  v = lv_slider_get_value(obj);
#ifdef OLD_CODE
  bsp_codec_setvol(haldev->codec_i2c, v * 2);
#else
  bsp_codec_setvol(haldev->codec_i2c, v);
#endif
}

static void brightness_event_cb(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target(e);
  HAL_DEVICE *haldev = (HAL_DEVICE *)lv_event_get_user_data(e);

  int32_t v = lv_slider_get_value(obj);
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

lv_obj_t *setup_screen_create(SETUP_SCREEN *setups, HAL_DEVICE *haldev)
{
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
#ifdef OLD_CODE
  lv_slider_set_range(setups->vol_slider, -63, 64);
#else
  lv_slider_set_range(setups->vol_slider, 0, 100);
#endif
  lv_obj_add_event_cb(setups->vol_slider, vol_event_cb, LV_EVENT_VALUE_CHANGED, haldev);
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
  lv_slider_set_range(setups->bright_slider, 0, 100);
  lv_slider_set_value(setups->bright_slider, Board_Get_Brightness(), LV_ANIM_OFF);
  lv_obj_add_event_cb(setups->bright_slider, brightness_event_cb, LV_EVENT_VALUE_CHANGED, haldev);
  img = lv_image_create(scr);
  lv_image_set_src(img, &brightness);
  lv_obj_align_to(img, bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

  setups->cont_bt = lv_imagebutton_create(scr);
  lv_imagebutton_set_src(setups->cont_bt, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &bluetooth_black, NULL);
  lv_obj_align(setups->cont_bt, LV_ALIGN_LEFT_MID, lv_pct(20), 0);
  lv_obj_set_width(setups->cont_bt, LV_SIZE_CONTENT);
  lv_obj_add_event_cb(setups->cont_bt, bt_button_handler, LV_EVENT_CLICKED, &setups->bt_button_info);
  lv_obj_add_flag(setups->cont_bt, LV_OBJ_FLAG_HIDDEN);

  setups->bt_button_info.btn = setups->cont_bt;
  return scr;
}
