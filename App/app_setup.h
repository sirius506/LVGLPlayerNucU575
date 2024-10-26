#include "DoomPlayer.h"

typedef enum {
  BTBTN_STATE_INIT = 0,
  BTBTN_STATE_READY,
  BTBTN_STATE_SCAN,
  BTBTN_STATE_CONNECT,
} BTBTN_STATE;

typedef struct {
  lv_obj_t *btn;
  BTBTN_STATE bst;
} BT_BUTTON_INFO;

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
  BT_BUTTON_INFO bt_button_info;
} SETUP_SCREEN;

void start_setup();
void activate_screen(lv_obj_t *screen, void (*list_action)(), void *arg_ptr);
lv_obj_t *setup_screen_create(SETUP_SCREEN *setups, HAL_DEVICE *haldev, lv_indev_t *keydev);
void SetBluetoothButtonState(BT_BUTTON_INFO *binfo, BTBTN_STATE new_state);

extern SETUP_SCREEN SetupScreen;
