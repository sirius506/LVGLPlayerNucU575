#include "DoomPlayer.h"
#include "btapi.h"

typedef struct {
  lv_obj_t *setup_screen;
  lv_obj_t *active_screen;
  lv_obj_t *cont_bt;
  lv_obj_t *hid_btn;
  lv_obj_t *a2dp_btn;
  lv_obj_t *vol_slider;
  lv_obj_t *bright_slider;
  lv_indev_t *keydev;
  lv_group_t *ing;
  lv_group_t *caller_ing;
  void     (*list_action)(void *ptr);
  void     *arg_ptr;
  BTSTACK_INFO *btinfo;
} SETUP_SCREEN;

void start_setup();
lv_obj_t *setup_screen_create(SETUP_SCREEN *setups, HAL_DEVICE *haldev, lv_indev_t *keydev);
void UpdateBluetoothButton(SETUP_SCREEN *screen);
void set_pad_focus(lv_group_t *gr);
void set_pad_defocus(lv_group_t *gr);
int IsPadAvailable();

extern SETUP_SCREEN SetupScreen;
