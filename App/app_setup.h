#include "DoomPlayer.h"

typedef enum {
  BT_STATE_INIT = 0,
  BT_STATE_READY,
  BT_STATE_SCAN,
  BT_STATE_CONNECT,
} BT_STATE;

typedef struct {
  lv_obj_t *btn;
  BT_STATE bst;
} BT_BUTTON_INFO;

typedef struct {
  lv_obj_t *setup_screen;
  lv_obj_t *active_screen;
  lv_obj_t *cont_bt;
  lv_obj_t *vol_slider;
  lv_obj_t *bright_slider;
  void     (*list_action)(void *ptr);
  void     *arg_ptr;
  BT_BUTTON_INFO bt_button_info;
} SETUP_SCREEN;

void start_setup();
void activate_screen(lv_obj_t *screen, void (*list_action)(), void *arg_ptr);
lv_obj_t *setup_screen_create(SETUP_SCREEN *setups, HAL_DEVICE *haldev);
void SetBluetoothButtonState(BT_BUTTON_INFO *binfo, BT_STATE new_state);

extern SETUP_SCREEN SetupScreen;
