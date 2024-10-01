/**
 *   @brief DoomPlayer for Nucleo-U575ZI GUI handler
 *
 */
#include "DoomPlayer.h"
#include "audio_output.h"
#include "btapi.h"
#include "gamepad.h"
#include "SDL.h"
#include "SDL_joystick.h"
#include "board_if.h"
#include "app_music.h"
#include "a2dp_player.h"
#include "src/display/lv_display_private.h"
#include "app_setup.h"

volatile DOOM_SCREEN_STATUS DoomScreenStatus;

TASK_DEF(doomTask, 800, osPriorityBelowNormal)
TASK_DEF(btstacktask,  500, osPriorityNormal2)

extern void StartBtstackTask(void *arg);
extern void KickOscMusic(HAL_DEVICE *haldev, OSCM_SCREEN *screen);

LV_IMG_DECLARE(imgtest)
LV_IMG_DECLARE(Action_Left)
LV_IMG_DECLARE(Action_Right)
LV_IMG_DECLARE(Arrow_Left)
LV_IMG_DECLARE(Arrow_Up)
LV_IMG_DECLARE(Arrow_Right)
LV_IMG_DECLARE(Arrow_Down)
LV_IMG_DECLARE(Arrow_Lower_Left)
LV_IMG_DECLARE(Arrow_Lower_Right)
LV_IMG_DECLARE(Arrow_Upper_Left)
LV_IMG_DECLARE(Arrow_Upper_Right)

LV_IMG_DECLARE(Button_Go)
LV_IMG_DECLARE(Button_Stop)
LV_IMG_DECLARE(Button_Shutdown)

void StartDoomTask(void *argument);

extern void bsp_process_touch(lv_indev_data_t *tp);
extern lv_obj_t *music_player_create(AUDIO_CONF *audio_config, lv_group_t *g, lv_style_t *btn_style, lv_indev_t *keypad_dev);

extern int doom_main(int argc, char **argv);

const GUI_LAYOUT GuiLayout = {
  .font_title = &lv_font_montserrat_20,
  .font_small = &lv_font_montserrat_12,
  .font_large = &lv_font_montserrat_16,

  .spinner_width = 12,

  .bt_yoffset = 40,

  /* Menu screen */

  .mb_yoffset = 80,
  .mb_height = 50,
  .mb_olw = 4,

  /* DualSense demo screen */

  .led_rad = 5,
  .joy_divisor = 5,

  .line_width = 5,

  .bar_width = 10,
  .bar_height = 100,
  .bar_xpos = -50,
  .bar_ypos =  -85,
};

const char * doom_argv[] = {
  "NucleoU575 Doom",
  NULL,
};

const GUI_EVENT reboot_event = {
  GUIEV_REBOOT,
  0,
  NULL,
  NULL
};

static void reboot_event_cb(lv_event_t *e)
{         
  UNUSED(e);
  postGuiEvent(&reboot_event); 
}

const WADPROP InvalidFlashGame =
 { 0, 0, NULL, "Game not written" };


#define GUIEVQ_DEPTH     8
  
static uint8_t guievqBuffer[GUIEVQ_DEPTH * sizeof(GUI_EVENT)];
 
MESSAGEQ_DEF(guievq, guievqBuffer, sizeof(guievqBuffer))

#define PADKEYQ_DEPTH   5
static uint8_t padkeyBuffer[PADKEYQ_DEPTH * sizeof(lv_indev_data_t)];

MESSAGEQ_DEF(padkeyq, padkeyBuffer, sizeof(padkeyBuffer));

osThreadId_t    doomtaskId;
static int lvgl_active;
static lv_display_t *display;

lv_indev_data_t tp_data;
static lv_indev_t *indev;
static lv_indev_t *keydev;

#define USE_BUF2
#ifdef USE_BUF2
static uint16_t buf_1[DISP_HOR_RES * 56];
static uint16_t buf_2[DISP_HOR_RES * 56];
#else
static uint16_t buf_1[DISP_HOR_RES * 80];
#endif
static lv_style_t style_title;
static lv_style_t style_focus;
static osMessageQueueId_t  guievqId;
static osMessageQueueId_t  padkeyqId;

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  int32_t x1 = area->x1;
  int32_t x2 = area->x2;
  int32_t y1 = area->y1;
  int32_t y2 = area->y2; 
   
#ifdef LCD_DEBUG
  if ((DoomScreenStatus == DOOM_SCREEN_SUSPEND) || (DoomScreenStatus == DOOM_SCREEN_SUSPENDED))
  {
debug_printf("flush (%d): (%d, %d) - (%d, %d)\n", DoomScreenStatus, x1, y1, x2, y2);
  }
#endif
  /* Return if the area is out of the screen */
  if ((x2 < 0) || (y2 < 0) || (x1 > DISP_HOR_RES - 1) || (y1 > DISP_VER_RES - 1))
  {
debug_printf("flush: (%d, %d) - (%d, %d)\n", x1, y1, x2, y2);
    lv_disp_flush_ready(disp);
    return;
  }

  /* Truncate the area to the screen */
  int32_t act_x1 = x1 < 0? 0 : x1;
  int32_t act_y1 = y1 < 0? 0 : y1;
  int32_t act_x2 = (x2 > DISP_HOR_RES - 1)? DISP_HOR_RES - 1 : x2;
  int32_t act_y2 = (y2 > DISP_VER_RES - 1)? DISP_VER_RES - 1 : y2;

  if (act_y1 == act_y2)
  {
debug_printf("flush: (%d, %d) - (%d, %d)\n", x1, y1, x2, y2);
    lv_display_flush_ready(disp);
    return;
  }
  bsp_lcd_flush(&HalDevice, (uint8_t *)px_map, act_x1, act_x2, act_y1, act_y2);
}

static void touch_read(lv_indev_t *drv, lv_indev_data_t *data)
{
  UNUSED(drv);
  static uint16_t lastx, lasty;
  *data = tp_data;
  if (data->state == LV_INDEV_STATE_RELEASED)
  {
    data->point.x = lastx;
    data->point.y = lasty;
  }
  else
  {
    lastx = data->point.x;
    lasty = data->point.y;
  }
//debug_printf("touch: %d @ (%d, %d)\n", data->state, data->point.x, data->point.y);
}

static void keypad_read(lv_indev_t *drv, lv_indev_data_t *data)
{
  UNUSED(drv);
  osStatus_t st;
  lv_indev_data_t psrc;

  st = osMessageQueueGet(padkeyqId, &psrc, NULL, 0);
  if (st == osOK)
  {
     *data = psrc;
  }
}

void postGuiEvent(const GUI_EVENT *event)
{
  while (guievqId == NULL)
  {
    osDelay(10);
  }
  if (osMessageQueuePut(guievqId, event, 0, 0) != osOK)
  {
    debug_printf("%s: put %d failed.\n", __FUNCTION__, event->evcode);
  }
}

void postGuiEventMessage(GUIEV_CODE evcode, uint32_t evval0, void *evarg1, void *evarg2)
{
  GUI_EVENT ev;

  ev.evcode = evcode;
  ev.evval0 = evval0;
  ev.evarg1 = evarg1;
  ev.evarg2 = evarg2;
  
  while (guievqId == NULL)
  {
    osDelay(10);
  }
  if (osMessageQueuePut(guievqId, &ev, 0, 0) != osOK)
  {
    debug_printf("%s: put failed. (%d)\n", __FUNCTION__, evcode);
  }
}

/**     
 * @brief Called when Game menu button has clicked.
 */     
static void menu_event_cb(lv_event_t *e)
{       
  int code = (int) lv_event_get_user_data(e);
        
  postGuiEventMessage(code, 0, NULL, NULL); 
}       
        
static void game_event_cb(lv_event_t *e)
{       
  lv_event_code_t code = lv_event_get_code(e);
  GUI_EVENT ev;
          
  if (code == LV_EVENT_LONG_PRESSED || code == LV_EVENT_SHORT_CLICKED)
  {
    ev.evcode = GUIEV_CHEAT_BUTTON;
    ev.evval0 = code;
    postGuiEvent(&ev);
  }
}

START_SCREEN StartScreen;
MENU_SCREEN  MenuScreen;
COPY_SCREEN  CopyScreen;
GAME_SCREEN  GameScreen;
SOUND_SCREEN SoundScreen;
A2DP_SCREEN  A2DPScreen;
SETUP_SCREEN SetupScreen;
OSCM_SCREEN  OSCMScreen;

/**       
 * @brief Callback called when flash game has selected.
 */         
static void flash_btn_event_cb(lv_event_t *e)
{         
  WADPROP *pwad = lv_event_get_user_data(e);
          
  postGuiEventMessage(GUIEV_FLASH_GAME_SELECT, 0, pwad, NULL);
}
          
/**
 * @brief Callback called when SD game has selected.
 */
static void sd_btn_event_cb(lv_event_t *e)
{
  WADLIST *pwad = lv_event_get_user_data(e);

  debug_printf("%x : %s\n", pwad, pwad->wadInfo->title);
  postGuiEventMessage(GUIEV_SD_GAME_SELECT, 0, pwad, NULL);
}

/**
 * @brief Callback called when message box on the copy screen has clicked.
 */ 
static void copy_event_cb(lv_event_t *e)
{ 
  GUI_EVENT ev;
  lv_obj_t *btn = lv_event_get_target(e);
  lv_obj_t *label = lv_obj_get_child(btn, 0);

  if (strncmp(lv_label_get_text(label), "Yes", 3) == 0)
  {
    ev.evcode = GUIEV_ERASE_START;
    ev.evval0 = 0; 
  }
  else
  {
    ev.evcode = GUIEV_REDRAW_START;
    ev.evval0 = 0;
  }
  ev.evarg1 = NULL;
  ev.evarg2 = NULL;
  postGuiEvent(&ev);
}

/* Space, USB, Space, Bluetooth, Space, Battery */
static char icon_label_string[13] = {
  ' ',                  // 0
  ' ', ' ', ' ',        // 1
  ' ',                  // 4
  ' ', ' ', ' ',        // 5
  ' ',                  // 8
  ' ', ' ', ' ',        // 9
  0x00 };
  
static const char *icon_map[] = {
  LV_SYMBOL_BATTERY_EMPTY,
  LV_SYMBOL_BATTERY_1,
  LV_SYMBOL_BATTERY_2,
  LV_SYMBOL_BATTERY_3,
  LV_SYMBOL_BATTERY_FULL,
};

GAMEPAD_INFO nullPad = { 
  .name = "Dummy Controller",
  .hid_mode = HID_MODE_LVGL,
  .padDriver = NULL,
};

void drv_wait_cb(lv_display_t *drv)
{
  UNUSED(drv);
  bsp_wait_lcd(drv->user_data);
  //lv_display_flush_ready(drv);
}

/*
 * Cheat code keyboard handler to detect OK and/or Cancel input
 */
static void keyboard_handler(lv_event_t *e)
{ 
  lv_event_code_t code = lv_event_get_code(e);
  GUI_EVENT event;
  
  
  event.evcode = 0;
  
  if (code == LV_EVENT_READY)
    event.evcode = GUIEV_KBD_OK;
  else if (code == LV_EVENT_CANCEL)
    event.evcode = GUIEV_KBD_CANCEL;
  
  if (event.evcode)
  {
    event.evval0 = 0;
    event.evarg1 = NULL;
    event.evarg2 = NULL;
    postGuiEvent(&event);
  }
}

static const char * const cheatcode_map[] = {
  "idfa", "idkfa", "iddqd", "idclip", "\n",
  "Cancel",
  "",
};
  
  
static void cheat_button_handler(lv_event_t *e)
{ 
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_target(e);
  uint32_t id;
  const char *txt;
  
  if (code == LV_EVENT_VALUE_CHANGED)
  {
    //debug_printf("%s selected\n", txt);
    id = lv_buttonmatrix_get_selected_button(obj);
    txt = lv_buttonmatrix_get_button_text(obj, id);
    if (strncmp(txt, "Cancel", 6) == 0)
      txt = "";
  
    postGuiEventMessage(GUIEV_CHEAT_SEL, 0, (void *)txt, NULL);
  }
}

typedef struct {
  uint16_t xpos;
  uint16_t ypos;
  const void *image;
  uint32_t btn_code;
} DOOM_BUTTON;

#define	VBMASK_BUTTON	0x80000000

const DOOM_BUTTON DoomGameButtons[] = {
  {   0, 182, &Arrow_Upper_Left,  (SDL_HAT_UP|SDL_HAT_LEFT) },
  {  45, 182, &Arrow_Up,          SDL_HAT_UP },
  {  90, 182, &Arrow_Upper_Right, (SDL_HAT_UP|SDL_HAT_RIGHT)  },
  {   0, 227, &Arrow_Left,        SDL_HAT_LEFT },
  {  90, 227, &Arrow_Right,       SDL_HAT_RIGHT },
  {   0, 272, &Arrow_Lower_Left,  (SDL_HAT_DOWN|SDL_HAT_LEFT) },
  {  45, 272, &Arrow_Down,        SDL_HAT_DOWN },
  {  90, 272, &Arrow_Lower_Right, (SDL_HAT_DOWN|SDL_HAT_RIGHT) },
  {  45,  40, &Button_Shutdown,   VBMASK_BUTTON|VBMASK_PS },
  {  15, 100, &Action_Left,       VBMASK_BUTTON|VBMASK_L1 },
  {  75, 100, &Action_Right,      VBMASK_BUTTON|VBMASK_R1 },
  { 300, 240, &Button_Stop,       VBMASK_BUTTON|VBMASK_SQUARE },
  { 400, 240, &Button_Go,         VBMASK_BUTTON|VBMASK_CIRCLE },
  { 0, 0, NULL, 0 },
};

static uint8_t  hat_code;
static uint16_t button_code;

static void doom_button_handler(lv_event_t *e)
{
  lv_event_code_t code;
  DOOM_BUTTON *gb;

  code = lv_event_get_code(e);
  gb = (DOOM_BUTTON *)lv_event_get_user_data(e);

  switch (code)
  {
  case LV_EVENT_PRESSED:
    if (gb->btn_code & VBMASK_BUTTON)
      button_code |= gb->btn_code & ~VBMASK_BUTTON;
    else
      hat_code |= gb->btn_code;
    SDL_JoyStickSetButtons(hat_code, button_code);
//debug_printf("PRESSED %x\n", button_code);
    break;
  case LV_EVENT_CLICKED:
    break;
  case LV_EVENT_VALUE_CHANGED:
    break;
  case LV_EVENT_RELEASED:
    if (gb->btn_code & VBMASK_BUTTON)
      button_code &= ~(gb->btn_code & ~VBMASK_BUTTON);
    else
      hat_code &= ~(gb->btn_code);
    SDL_JoyStickSetButtons(hat_code, button_code);
//debug_printf("RELEASED %x\n", button_code);
    break;
  default:
    break;
  }
}

void create_doom_buttons(lv_obj_t *parent)
{
  lv_obj_t *btn;
  const DOOM_BUTTON *gb;
  SDL_Joystick *joystick = SDL_GetJoystickPtr();

  for (gb = DoomGameButtons; gb->image; gb++)
  {
    btn = lv_imagebutton_create(parent);
    lv_imagebutton_set_src(btn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, gb->image, NULL);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, gb->xpos, gb->ypos);
    lv_obj_set_width(btn, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn, doom_button_handler, LV_EVENT_ALL, (void *)gb);
  }

  if (joystick)
  {
    joystick->name = "TouchPanel";
    joystick->naxes = 0;
    joystick->nhats = 1;
    joystick->nbuttons = NUM_VBUTTONS;
    joystick->hats = 0;
    joystick->buttons = 0;
  }
}

typedef struct {
  const char *label_text;
  const uint16_t label_pos;
} APP_LABEL_INFO;

static const APP_LABEL_INFO app_labels[] = {
 { "Doom Player", 30 },
 { "Bluetooth Player", 50 },
 { "Oscilloscope  Music",  70 },
};

#define	NUM_APPLICATION	(sizeof(app_labels)/sizeof(APP_LABEL_INFO))

static void app_select_handler(lv_event_t *e)
{
  int index = (int)lv_event_get_user_data(e);

  debug_printf("App %d selected.\n", index);

  postGuiEventMessage(GUIEV_APP_SELECT, index, NULL, NULL);
}

static int SelectApplication(lv_obj_t *sel_screen)
{
  unsigned int new_interval, timer_interval;
  osStatus_t st;
  lv_obj_t *title;
  lv_obj_t *mbox;
  lv_obj_t *btn;
  lv_obj_t *label;
  char sbuff[30];

  title = lv_label_create(sel_screen);
  lv_obj_add_style(title, &style_title, 0);
  lv_label_set_text(title, "LVGL Player (Nucleo-U575ZI)");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, lv_pct(7));

  /* Create application buttons */

  for (unsigned int i = 0; i < NUM_APPLICATION; i++)
  {

    btn = lv_btn_create(sel_screen);
    lv_obj_align(btn, LV_ALIGN_TOP_MID,  0, lv_pct(app_labels[i].label_pos));
    lv_obj_add_event_cb(btn, app_select_handler, LV_EVENT_CLICKED, (void *)i);

    label = lv_label_create(btn);
    lv_label_set_text(label, app_labels[i].label_text);
    lv_obj_center(label);
  }

  activate_screen(sel_screen);

  timer_interval = 3;

  while (1)
  {
    GUI_EVENT event;

    st = osMessageQueueGet(guievqId, &event, NULL, timer_interval);
    if (st == osOK)
    {
      switch (event.evcode)
      {
      case GUIEV_APP_SELECT:
        return (event.evval0);
        break;
      case GUIEV_SD_REPORT:
        /* SD card verification failed. Show message box to reboot. */

        lv_snprintf(sbuff, sizeof(sbuff)-1, "%s %s", (char *)event.evarg1, (char *)event.evarg2);
        mbox = lv_msgbox_create(NULL);
        lv_msgbox_add_title(mbox, "Bad SD Card");
        lv_msgbox_add_text(mbox, sbuff);
        btn = lv_msgbox_add_footer_button(mbox, "Reboot");
        lv_obj_update_layout(btn);
        debug_printf("btn size = %d x %d\n", lv_obj_get_width(btn), lv_obj_get_height(btn));

        lv_obj_add_event_cb(btn, reboot_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_center(mbox);
        break;
      case GUIEV_REBOOT:
        osDelay(200);
        NVIC_SystemReset();
        break;
      default:
        break;
      }
    }
    else
    {
      new_interval = lv_timer_handler();
      if (new_interval != timer_interval)
      {
        timer_interval = new_interval/2;
      }
    }
  }
}

void oscDraw(OSCM_SCREEN *screen, AUDIO_STEREO *mp);

void StartGuiTask(void *args)
{
  START_SCREEN *starts = &StartScreen;
  MENU_SCREEN *menus = &MenuScreen;
  COPY_SCREEN *copys = &CopyScreen;
  GAME_SCREEN *games = &GameScreen;
  SETUP_SCREEN *setups = &SetupScreen;
  SOUND_SCREEN *sounds = &SoundScreen;
  A2DP_SCREEN  *a2dps = &A2DPScreen;
  HAL_DEVICE *haldev = (HAL_DEVICE *)args;
  const GUI_LAYOUT *layout = &GuiLayout;
  osStatus_t st;
  lv_obj_t *sound_list;
  lv_obj_t *label;
  lv_obj_t *tlabel;
  lv_obj_t *btn;
  unsigned int new_interval, timer_interval;
  WADLIST *wadlist;
  WADPROP *flash_game;
  WADPROP *sel_flash_game;
  WADLIST *sel_sd_game;
  lv_obj_t *btn_row;
  lv_obj_t *fbutton;
  lv_style_t game_style;
  lv_style_t style_menubtn;
  lv_style_t style_cheat;
  AUDIO_CONF *audio_config;
  lv_obj_t *icon_label;
  uint16_t icon_value;
  GAMEPAD_INFO *padInfo;
  extern bool D_GrabMouseCallback();
  const char *kbtext;
  char sbuff[70];
  uint32_t cheatval;

  static lv_group_t *g;

  debug_printf("GuiTask started.\n");

  lv_init();
  display = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
  display->user_data = (void *)haldev->tft_lcd;

#ifdef USE_BUF2
  lv_display_set_buffers(display, buf_1, buf_2, sizeof(buf_1), LV_DISP_RENDER_MODE_PARTIAL);
#else
  lv_display_set_buffers(display, buf_1, NULL, sizeof(buf_1), LV_DISP_RENDER_MODE_PARTIAL);
#endif
  lv_display_set_flush_cb(display, disp_flush);
  lv_display_set_flush_wait_cb(display, drv_wait_cb);

  lvgl_active = 1;
  cheatval = 0;

  if (bsp_touch_init(haldev) == 0)
  {
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read);
  }

  bsp_codec_init(haldev->codec_i2c, 44100);

  keydev = lv_indev_create();
  lv_indev_set_type(keydev, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(keydev, keypad_read);

  lv_style_init(&style_title);
  //lv_style_set_text_font(&style_title, &lv_font_montserrat_20);
  lv_style_set_text_font(&style_title, layout->font_title);

  lv_style_init(&style_menubtn);
  lv_style_set_outline_width(&style_menubtn, layout->mb_olw);

  lv_style_init(&style_focus);
  lv_style_set_img_recolor_opa(&style_focus, LV_OPA_10);
  lv_style_set_img_recolor(&style_focus, lv_color_black());

  padInfo = &nullPad;
  icon_value = 0;

  guievqId = osMessageQueueNew(GUIEVQ_DEPTH, sizeof(GUI_EVENT), &attributes_guievq);
  padkeyqId = osMessageQueueNew(PADKEYQ_DEPTH, sizeof(lv_indev_data_t), &attributes_padkeyq);

  icon_label = lv_label_create(lv_layer_top());
  lv_obj_set_style_text_color(icon_label, lv_color_black(), 0);
  lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, 0, 0);
  //lv_label_set_text(icon_label, " " LV_SYMBOL_USB " " LV_SYMBOL_BLUETOOTH);
  lv_label_set_text(icon_label, (const char *)icon_label_string);

  /* Create Setup screen */

  setup_screen_create(setups, haldev);
  lv_obj_add_event_cb(setups->setup_screen, quit_setup_event, LV_EVENT_GESTURE, NULL);

  lv_obj_t *sel_screen;

  sel_screen = lv_obj_create(NULL);

  haldev->boot_mode = SelectApplication(sel_screen);

  if (haldev->boot_mode < 2)
    osThreadNew(StartBtstackTask, haldev, &attributes_btstacktask);

  starts->screen = lv_obj_create(NULL);
  menus->screen = lv_obj_create(NULL);
  copys->screen = lv_obj_create(NULL);
  games->screen = lv_obj_create(NULL);
  sounds->screen = lv_obj_create(NULL);

  /* Switch to initial startup screen */

  activate_screen(starts->screen);
  
  lv_obj_delete(sel_screen);
  
  starts->title = lv_label_create(starts->screen);
  lv_obj_add_style(starts->title, &style_title, 0);
  lv_label_set_text(starts->title, "Doom Player (Nucleo-U575ZI)");
  lv_obj_align(starts->title, LV_ALIGN_TOP_MID, 0, lv_pct(7));

  menus->title = lv_label_create(menus->screen);
  lv_obj_add_style(menus->title, &style_title, 0);
  lv_label_set_text(menus->title, "");
  lv_obj_align(menus->title, LV_ALIGN_TOP_MID, 0, lv_pct(7));

  copys->title = lv_label_create(copys->screen);
  lv_obj_add_style(copys->title, &style_title, 0);
  lv_label_set_text(copys->title, "");
  lv_obj_align(copys->title, LV_ALIGN_TOP_MID, 0, lv_pct(7));

  copys->operation = lv_label_create(copys->screen);
  lv_label_set_text(copys->operation, "");
  lv_obj_align(copys->operation, LV_ALIGN_TOP_MID, 0, lv_pct(30));

  copys->fname = lv_label_create(copys->screen);
  lv_label_set_text(copys->fname, "");
  lv_obj_align(copys->fname, LV_ALIGN_TOP_MID, 0, lv_pct(38));

  games->img = lv_image_create(games->screen);

  games->cheat_btn = lv_btn_create(games->screen);
  lv_obj_add_flag(games->img, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_style_all(games->cheat_btn);
  lv_style_init(&style_cheat);
  lv_style_set_bg_opa(&style_cheat, LV_OPA_TRANSP);
  lv_obj_set_size(games->cheat_btn, 50, 50);
  lv_obj_add_style(games->cheat_btn, &style_cheat, 0);
  lv_obj_center(games->cheat_btn);
  lv_obj_add_event_cb(games->cheat_btn, game_event_cb, LV_EVENT_ALL, (void *)0);

  lv_style_init(&game_style);
  lv_style_set_bg_color(&game_style, lv_color_black());
  lv_obj_add_style(games->screen, &game_style, 0);

  /* Prepare virtual keyboard. We'd like to draw the keyboard within DOOM screen area. */

#define USE_KBD
#ifdef USE_KBD
  games->kbd = lv_keyboard_create(games->screen);
  lv_obj_add_flag(games->kbd, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(games->kbd, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_width(games->kbd, DISP_HOR_RES);
  games->ta = lv_textarea_create(games->screen);        // Text area
  lv_obj_add_flag(games->ta, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(games->ta, LV_ALIGN_TOP_MID, 0, 20);
  lv_textarea_set_one_line(games->ta, true);
  lv_textarea_set_max_length(games->ta, 16);
  lv_obj_add_state(games->ta, LV_STATE_FOCUSED);
  lv_keyboard_set_textarea(games->kbd, games->ta);
  lv_obj_add_event_cb(games->kbd, keyboard_handler, LV_EVENT_ALL, (void *)0);
#endif
          
  /* Prepare cheat buttons */
  games->cheat_code = lv_buttonmatrix_create(games->screen);
  lv_obj_add_flag(games->cheat_code, LV_OBJ_FLAG_HIDDEN);
  lv_buttonmatrix_set_map(games->cheat_code, (const char **)cheatcode_map);
  lv_obj_align(games->cheat_code, LV_ALIGN_TOP_MID, 0, lv_pct(35));
  lv_obj_add_event_cb(games->cheat_code, cheat_button_handler, LV_EVENT_ALL, NULL);

  wadlist = NULL;
  tlabel = NULL;
  flash_game = NULL;
  sel_sd_game = NULL;
  menus->sub_scr = NULL;
  sound_list = NULL;

  audio_config = get_audio_config(&HalDevice);

  switch (haldev->boot_mode)
  {
  case BOOTM_DOOM:
    postMainRequest(REQ_VERIFY_SD, NULL, 0);	// Start SD card verification
    break;
  case BOOTM_A2DP:
    postMainRequest(REQ_VERIFY_FONT, NULL, 0);	// Start font file verification
    break;
  default:
    postGuiEventMessage(GUIEV_OSCM_START, 0, NULL, NULL);
    break;
  }

  timer_interval = 3;


  while (1)
  {
    GUI_EVENT event;

    st = osMessageQueueGet(guievqId, &event, NULL, timer_interval);
    if (st == osOK)
    {
      switch (event.evcode)
      {
      case GUIEV_TOUCH_INT:
        bsp_process_touch(&tp_data);
        break;
      case GUIEV_DOOM_REFRESH:
        lv_image_set_src(games->img, event.evarg1);
        lv_refr_now(NULL);
        break;
      case GUIEV_BTSTACK_READY:
        lv_obj_remove_flag(setups->cont_bt, LV_OBJ_FLAG_HIDDEN);
        setups->bt_button_info.bst = BT_STATE_READY;
        break;
      case GUIEV_PSEC_UPDATE:
        if (haldev->boot_mode)
          app_ppos_update((MIX_INFO *)event.evarg1);
        else if (lv_scr_act() == menus->play_scr)
          app_psec_update(event.evval0);
        break;
      case GUIEV_FFT_UPDATE:
        if ((lv_scr_act() == menus->play_scr) && (padInfo->hid_mode != HID_MODE_DOOM))
        {
          app_spectrum_update(event.evval0);
        }
        bsp_ledpwm_update(haldev, event.evarg1);
        break;
      case GUIEV_RIGHT_XDIR:
      case GUIEV_RIGHT_YDIR:
        if (lv_scr_act() == sounds->screen)
          sound_process_stick(event.evcode, event.evval0);
        else if (lv_scr_act() == menus->play_scr)
          music_process_stick(event.evcode, event.evval0);
        break;
      case GUIEV_LEFT_XDIR:
        {
          int brval = Board_Get_Brightness();

          if ((int)event.evval0 > 0)
           brval += 10;
          else
           brval -= 10;
          if (brval > 100) brval = 100;
          if (brval < 0) brval = 0;
          Board_Set_Brightness(haldev, brval);
        }
        break;
      case GUIEV_LEFT_YDIR:
        {
          int cvol = Mix_GetVolume();
          if ((int)event.evval0 < 0)
           cvol += 10;
          else
           cvol -= 10;
          if (cvol > 100) cvol = 100;
          if (cvol < 0) cvol = 0;
          Mix_VolumeMusic(cvol);
        }
        break;
      case GUIEV_MUSIC_FINISH:
        _lv_demo_inter_pause_start();
        break;
      case GUIEV_ICON_CHANGE:
        {
          int ival;
          char *sp;
      
          ival = event.evval0;
          if (ival & ICON_BATTERY)
          {
            icon_value &= ~ICON_BATTERY_MASK;
            icon_value |= ival & ICON_BATTERY_MASK;
            icon_value |= ICON_BATTERY;
          }
          else
          {
            if (ival & ICON_SET)
              icon_value |= (ival & (ICON_USB|ICON_BLUETOOTH));
            else
            {
              icon_value &= ~(ival & (ICON_USB|ICON_BLUETOOTH));
        
              if (ival & ICON_BLUETOOTH)
              {
                /* If BT connection is lost,
                 * we no longer able to get battery level.
                 */
                icon_value &= ~ICON_BATTERY;
              }
            }
          }
          sp = icon_label_string;
          *sp++ = ' ';
          if (icon_value & ICON_USB)
          {
            strncpy(sp, LV_SYMBOL_USB, 3);
            sp += 3;
            *sp++ = ' ';
          }
          if (icon_value & ICON_BLUETOOTH)
          {
            strncpy(sp, LV_SYMBOL_BLUETOOTH, 3);
            sp += 3;
            *sp++ = ' ';
          }
          if (icon_value & ICON_BATTERY)
          {
            strncpy(sp, icon_map[icon_value & ICON_BATTERY_MASK], 3);
            sp += 3;
          }
          *sp = 0;
          lv_label_set_text(icon_label, (const char *)icon_label_string);
        }
        break;
      case GUIEV_SD_REPORT:             // SD card verification has finished
        if (event.evval0 == 0)
        {
          /* SD card verify successfull. */

          label = lv_label_create(starts->screen);
          lv_label_set_text(label, LV_SYMBOL_OK " SD card verified.");
          lv_obj_align_to(label, starts->title, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

          /* Send SPI flash verify command. */

          tlabel = lv_label_create(starts->screen);
          lv_label_set_text(tlabel, " Checking SPI Flash...");
          lv_obj_align_to(tlabel, label, LV_ALIGN_OUT_BOTTOM_MID, 0, H_PERCENT(2));
          wadlist = (WADLIST *)event.evarg1;

          postMainRequest(REQ_VERIFY_FLASH, NULL, 0);

          /* Start spinner */

          starts->spinner = lv_spinner_create(starts->screen);
          lv_spinner_set_anim_params(starts->spinner, 1000, 60);
          lv_obj_set_size(starts->spinner, W_PERCENT(20), W_PERCENT(20));
          lv_obj_align(starts->spinner, LV_ALIGN_TOP_MID, 0, H_PERCENT(45));
          lv_obj_set_style_arc_width(starts->spinner, layout->spinner_width, LV_PART_MAIN);
          lv_obj_set_style_arc_width(starts->spinner, layout->spinner_width, LV_PART_INDICATOR);
        }
        else
        {
          /* SD card verification failed. Show message box to reboot. */

          lv_snprintf(sbuff, sizeof(sbuff)-1, "%s %s", (char *)event.evarg1, (char *)event.evarg2);
          starts->mbox = lv_msgbox_create(NULL);
          lv_msgbox_add_title(starts->mbox, "Bad SD Card");
          lv_msgbox_add_text(starts->mbox, sbuff);
          starts->btn = lv_msgbox_add_footer_button(starts->mbox, "Reboot");
          lv_obj_update_layout(starts->btn);
          debug_printf("btn size = %d x %d\n", lv_obj_get_width(starts->btn), lv_obj_get_height(starts->btn));

          g = lv_group_create();
          lv_group_add_obj(g, starts->btn);
          lv_indev_set_group(keydev, g);
          lv_obj_add_event_cb(starts->btn, reboot_event_cb, LV_EVENT_PRESSED, NULL);
          lv_obj_center(starts->mbox);
        }
        Start_SDLMixer();
        break;
      case GUIEV_FLASH_REPORT:                  // SPI flash verification finished
        lv_obj_delete(starts->spinner);    // Stop spinner

        {
          static lv_style_t style_flashbutton;
          static lv_style_t style_sdbutton;
          WADLIST *sdgame;

          lv_style_init(&style_flashbutton);
          lv_style_init(&style_sdbutton);
          lv_style_set_bg_color(&style_sdbutton, lv_palette_main(LV_PALETTE_ORANGE));

          if (event.evval0 == 0)
          {
            lv_style_set_bg_color(&style_flashbutton, lv_palette_main(LV_PALETTE_LIGHT_GREEN));
            lv_label_set_text(tlabel, LV_SYMBOL_OK " SPI Flash verified.");

            /* Display game title availabe on the Flash */

            flash_game = (WADPROP *)event.evarg1;
          }
          else
          {
            lv_style_set_bg_color(&style_flashbutton, lv_palette_main(LV_PALETTE_GREY));
            lv_label_set_text(tlabel, LV_SYMBOL_CLOSE " SPI Flash invalid.");
            flash_game = (WADPROP *)&InvalidFlashGame;
          }
          fbutton = lv_btn_create(starts->screen);
          lv_obj_align(fbutton, LV_ALIGN_TOP_MID,  0, lv_pct(35));
          if (event.evval0 == 0)
          {
            lv_obj_add_style(fbutton, &style_flashbutton, 0);
            lv_obj_add_event_cb(fbutton, flash_btn_event_cb, LV_EVENT_CLICKED, flash_game);
          }
          else
          {
            lv_obj_add_style(fbutton, &style_flashbutton, 0);
          }
          lv_obj_set_style_outline_width(fbutton, layout->mb_olw / 2, LV_STATE_FOCUS_KEY);

          lv_obj_t *blabel = lv_label_create(fbutton);
          lv_label_set_text(blabel, flash_game->title);
          lv_obj_center(blabel);

          /* Display titles on SD card */

          btn_row = lv_obj_create(starts->screen);
          lv_obj_set_size(btn_row, W_PERCENT(64), lv_pct(45));
          lv_obj_align_to(btn_row, fbutton, LV_ALIGN_OUT_BOTTOM_MID, 0, H_PERCENT(4));
          lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_COLUMN);

          WADLIST *wp;

          starts->ing = lv_group_create();
          lv_indev_set_group(keydev, starts->ing);

          lv_group_add_obj(starts->ing, fbutton);

          lv_gridnav_add(btn_row, LV_GRIDNAV_CTRL_ROLLOVER);
          lv_group_add_obj(starts->ing, btn_row);

          /* Create buttons for IWAD files on the SD card */

          for (wp = wadlist; wp->wadInfo; wp++)
          {
            sdgame = wp;
            if (sdgame->wadInfo != flash_game)  // Don't create button if the IWAD is on the SPI flash
            {
              lv_obj_t *obj;
              lv_obj_t *label;

              obj = lv_btn_create(btn_row);
              lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
              lv_obj_add_style(obj, &style_sdbutton, 0);
              lv_obj_add_event_cb(obj, sd_btn_event_cb, LV_EVENT_CLICKED, sdgame);
              lv_obj_set_style_outline_width(obj, layout->mb_olw / 2, LV_STATE_FOCUS_KEY);

              label = lv_label_create(obj);
              lv_label_set_text(label, sdgame->wadInfo->title);
              lv_obj_center(label);
            }
          }
        }
        break;
      case GUIEV_FONT_REPORT:
        {
          lv_obj_t *scr;

          scr = lv_obj_create(NULL);
          activate_screen(scr);
          menus->play_scr = scr;
          a2dp_player_create(a2dps, scr, NULL);
          Start_SDLMixer();
        }
        break;
      case GUIEV_OSCM_START:
        KickOscMusic(haldev, &OSCMScreen);
        break;
      case GUIEV_OSCM_FILE:
        lv_label_set_text(OSCMScreen.scope_label, event.evarg1);
        break;
      case GUIEV_FLASH_GAME_SELECT:
        /* Selected game reside on the SPI flash. */
          
        sel_flash_game = (WADPROP *)event.evarg1;
        debug_printf("GAME: %s\n", sel_flash_game->title);
        
        lv_label_set_text(menus->title, sel_flash_game->title);

        sounds->ing = lv_group_create();

        free(wadlist);
        menus->ing = lv_group_create();
        lv_indev_set_group(keydev, menus->ing);

        /* Create Menu buttons */

        lv_obj_t *cont = lv_obj_create(menus->screen);
        lv_obj_remove_style_all(cont);
        lv_obj_set_size(cont, DISP_HOR_RES - (DISP_VER_RES/8), layout->mb_height + 7);

        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, layout->mb_yoffset);
#define ADD_MENU_BUTTON(btn_name, btn_string, btn_code) \
        menus->btn_name = lv_btn_create(cont); \
        lv_obj_add_style(menus->btn_name, &style_menubtn, LV_STATE_FOCUS_KEY); \
        lv_obj_set_height(menus->btn_name, layout->mb_height); \
        label = lv_label_create(menus->btn_name); \
        lv_label_set_text(label, btn_string); \
        lv_obj_center(label); \
        lv_obj_add_event_cb(menus->btn_name, menu_event_cb, LV_EVENT_CLICKED, (void *)btn_code);  \
        lv_group_add_obj(menus->ing, menus->btn_name);

        ADD_MENU_BUTTON(btn_music, "Music", GUIEV_MPLAYER_START)
        ADD_MENU_BUTTON(btn_sound, "Sound", GUIEV_SPLAYER_START)
#ifdef USE_FUSION
        ADD_MENU_BUTTON(btn_dual, "Game Pad\n Demo", GUIEV_PADTEST_START)
#endif
        ADD_MENU_BUTTON(btn_game, "Game", GUIEV_GAME_START)

#ifdef USE_FUSION
        if (padInfo == &nullPad)
        {
          /* DualSense is not detected, yet. Disable DualSense demo button. */
          lv_obj_add_state(menus->btn_dual, LV_STATE_DISABLED);
        }
#endif

        int cvol = bsp_codec_getvol(haldev->codec_i2c);
        lv_slider_set_value(setups->vol_slider, cvol, LV_ANIM_OFF);

        activate_screen(menus->screen);

        /* We no longer need start screen. */

        lv_obj_delete(starts->screen);
        break;
      case GUIEV_SD_GAME_SELECT:
        sel_sd_game = (WADLIST *)event.evarg1;
        debug_printf("GAME: %s\n", sel_sd_game->wadInfo->title);

        /* Selected game resind on the SD card.
         * Copy it into the SPI flash.
         */
        lv_label_set_text(copys->title, sel_sd_game->wadInfo->title);
        activate_screen(copys->screen);

        g = lv_group_create();
        copys->mbox = lv_msgbox_create(copys->screen);
        lv_msgbox_add_title(copys->mbox, "Copy Game Image");
        lv_msgbox_add_text(copys->mbox, "Are you sure to erase & re-write flash contents?");
        btn = lv_msgbox_add_footer_button(copys->mbox, "Yes");
        lv_obj_add_event_cb(btn, copy_event_cb, LV_EVENT_CLICKED, NULL);
        lv_group_add_obj(g, btn);
        btn = lv_msgbox_add_footer_button(copys->mbox, "Cancel");
        lv_obj_add_event_cb(btn, copy_event_cb, LV_EVENT_CLICKED, NULL);
        lv_group_add_obj(g, btn);
        lv_obj_center(copys->mbox);
        lv_indev_set_group(keydev, g);
        break;
      case GUIEV_ERASE_START:
        lv_obj_delete(copys->mbox);
        lv_refr_now(NULL);
        copys->bar = lv_bar_create(copys->screen);
        lv_obj_set_size(copys->bar, lv_pct(40), lv_pct(8));
        lv_obj_align(copys->bar, LV_ALIGN_TOP_MID, 0, lv_pct(55));
        lv_bar_set_value(copys->bar, 0, LV_ANIM_OFF);
        postMainRequest(REQ_ERASE_FLASH, sel_sd_game, 0);
        lv_refr_now(NULL);
        break;
      case GUIEV_REDRAW_START:
        /*
         * Copy operation has aborted.
         * Redraw start screen.
         */ 
        activate_screen(starts->screen);
        lv_indev_set_group(keydev, starts->ing);
        break;
      case GUIEV_REBOOT:
        btapi_disconnect();
        osDelay(200);
        NVIC_SystemReset();
        break;
      case GUIEV_ERASE_REPORT:
        switch (event.evval0)
        {
        case OP_START:
          lv_label_set_text(copys->operation, "Erasing SPI Flash..");
          copys->progress = lv_label_create(copys->screen);
          lv_label_set_text(copys->progress, "");
          lv_obj_align_to(copys->progress, copys->bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
          break;
        case OP_ERROR:
          lv_snprintf(sbuff, sizeof(sbuff)-1, "%s %s", (char *)event.evarg1, (char *)event.evarg2);
          copys->mbox = lv_msgbox_create(NULL);
          lv_msgbox_add_title(copys->mbox, "Flash Erase Error");
          lv_msgbox_add_text(copys->mbox, sbuff);
          btn = lv_msgbox_add_footer_button(copys->mbox, "Reboot");
          lv_obj_add_event_cb(btn, reboot_event_cb, LV_EVENT_CLICKED, NULL);
          g = lv_group_create();
          lv_group_add_obj(g, btn);
          lv_indev_set_group(keydev, g);
          lv_obj_add_event_cb(copys->mbox, reboot_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
          lv_obj_center(copys->mbox);
          break;
        case OP_PROGRESS:
          lv_snprintf(sbuff, sizeof(sbuff)-1, "%d%%", (int)event.evarg1);
          lv_bar_set_value(copys->bar, (int)event.evarg1, LV_ANIM_OFF);
          lv_label_set_text(copys->progress, sbuff);
          lv_refr_now(NULL);
          break;
        }
        break;
      case GUIEV_COPY_REPORT:
        switch (event.evval0)
        {
        case OP_START:
          lv_label_set_text(copys->operation, "Copying to SPI Flash..");
          if (event.evarg1)
            lv_label_set_text(copys->fname, event.evarg1);
          else
            lv_label_set_text(copys->fname, "");
          lv_bar_set_value(copys->bar, 0, LV_ANIM_OFF);
          break;
        case OP_ERROR:
          lv_snprintf(sbuff, sizeof(sbuff)-1, "%s %s", (char *)event.evarg1, (char *)event.evarg2);
          copys->mbox = lv_msgbox_create(NULL);
          lv_msgbox_add_title(copys->mbox, "Flash Erase Error");
          lv_msgbox_add_text(copys->mbox, sbuff);

          btn = lv_msgbox_add_footer_button(copys->mbox, "Reboot");
          lv_obj_add_event_cb(btn, reboot_event_cb, LV_EVENT_CLICKED, NULL);
          g = lv_group_create();
          lv_group_add_obj(g, btn);
          lv_indev_set_group(keydev, g);
          lv_obj_center(copys->mbox);
          break;
        case OP_PROGRESS:
          lv_snprintf(sbuff, sizeof(sbuff)-1, "%d%%", (int)event.evarg1);
          lv_bar_set_value(copys->bar, (int)event.evarg1, LV_ANIM_OFF);
          lv_label_set_text(copys->progress, sbuff);
          lv_refr_now(NULL);
          break;
        case OP_DONE:
          lv_obj_delete(copys->operation);
          lv_obj_delete(copys->progress);
          lv_obj_delete(copys->bar);
          copys->mbox = lv_msgbox_create(NULL);
          lv_msgbox_add_title(copys->mbox, "Copy done");
          lv_msgbox_add_text(copys->mbox,  "Reboot to activate new game.");

          btn = lv_msgbox_add_footer_button(copys->mbox, "Reboot");
          lv_obj_add_event_cb(btn, reboot_event_cb, LV_EVENT_CLICKED, NULL);
          g = lv_group_create();
          lv_group_add_obj(g, btn);
          lv_indev_set_group(keydev, g);
          break;
        }
        break;
      case GUIEV_SPLAYER_START:
        if (sound_list == NULL)
        {
          sound_list = sound_screen_create(sounds->screen, sounds->ing, &style_menubtn);
        } 
        activate_screen(sounds->screen);
        lv_indev_set_group(keydev, sounds->ing);
        break;
      case GUIEV_MPLAYER_START:
        if (menus->play_scr == NULL)
        {
          menus->player_ing = lv_group_create();
          lv_indev_set_group(keydev, menus->player_ing);
          menus->play_scr = music_player_create(audio_config, menus->player_ing, &style_menubtn, keydev);
          activate_screen(menus->play_scr);
        }
        else
        {
          activate_screen(menus->play_scr);
          lv_indev_set_group(keydev, menus->player_ing);
        }
        break;
#ifdef USE_FUSION
      case GUIEV_PADTEST_START:
        if (padInfo->padDriver && padInfo->padDriver->ResetFusion)
        {
          padInfo->padDriver->ResetFusion();
          GamepadHidMode(padInfo, HID_MODE_TEST);
        } 
        if (menus->sub_scr == NULL)
        {
          menus->sub_scr = padtest_create(padInfo, g);
        }
        activate_screen(menus->sub_scr);
        break;
      case GUIEV_PADTEST_UPDATE:
        if (padInfo->hid_mode == HID_MODE_TEST)
        {
          padtest_update(padInfo, event.evarg1, event.evval0);
          //lv_refr_now(NULL);
        }
        break;
      case GUIEV_PADTEST_DONE:
        GamepadHidMode(padInfo, HID_MODE_LVGL);
        /* Fall down to .. */
#endif
      case GUIEV_MPLAYER_DONE:
        /*
         * Restore menu screen and its input group
         */
        activate_screen(menus->screen);
        lv_indev_set_group(keydev, menus->ing);
        if (menus->sub_scr)
        {
          lv_obj_delete(menus->sub_scr);
          menus->sub_scr = NULL;
        }
        break;
      case GUIEV_GAME_START:
        if (g)
          lv_group_del(g);
        lv_obj_add_flag(icon_label, LV_OBJ_FLAG_HIDDEN);


        Mix_HaltMusic();                // Make sure to stop music playing

        activate_screen(games->screen);

        if (menus->sub_scr)
        {
          lv_obj_delete(menus->sub_scr);
          menus->sub_scr = NULL;
        }
        /* Delete Menu screen */
        lv_obj_delete(menus->screen);
        menus->screen = NULL;

        if (padInfo == &nullPad)
        {
          /* Don't allow new BT connection any more. */
          btapi_power_off();
          Board_DoomModeLCD(0);
          lv_obj_add_flag(games->cheat_btn, LV_OBJ_FLAG_HIDDEN);
          //lv_obj_align(games->img, LV_ALIGN_TOP_LEFT, DISP_HOR_RES - 320, 10);
          create_doom_buttons(games->screen);
        }
        else
        {
          //lv_obj_align(games->img, LV_ALIGN_TOP_MID, 0, 0);
          Board_DoomModeLCD(1);
        }

        /* Start DOOM game task */
        doomtaskId = osThreadNew(StartDoomTask, &doom_argv, &attributes_doomTask);

        break;
      case GUIEV_DOOM_ACTIVE:
        padInfo->hid_mode = HID_MODE_DOOM;
        DoomScreenStatus = DOOM_SCREEN_ACTIVE;
debug_printf("GUIEV_DOOM_ACTIVE\n");
        break;
      case GUIEV_CHEAT_BUTTON:
        if (D_GrabMouseCallback())      // Not in menu/demo mode
        {
          cheatval = event.evval0;
debug_printf("Suspend request.\n");
          DoomScreenStatus = DOOM_SCREEN_SUSPEND;
        }
        break;
      case GUIEV_CHEAT_ACK:
debug_printf("CHEAT_ACK (%d).\n", cheatval);
        if (cheatval)
        {
          lv_obj_add_flag(games->cheat_btn, LV_OBJ_FLAG_HIDDEN);
#ifdef USE_KBD
          if ((cheatval == LV_EVENT_LONG_PRESSED) && lv_obj_has_flag(games->cheat_code, LV_OBJ_FLAG_HIDDEN))
          {
            lv_obj_remove_flag(games->kbd, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(games->ta, LV_OBJ_FLAG_HIDDEN);
            lv_textarea_set_text(games->ta, "id");
            cheatval = 0;
          }
          else
          {
            lv_obj_remove_flag(games->cheat_code, LV_OBJ_FLAG_HIDDEN);
          }
#else
          lv_obj_remove_flag(games->cheat_code, LV_OBJ_FLAG_HIDDEN);
#endif
        }
        break;
      case GUIEV_KBD_OK:
#ifdef USE_KBD
        DoomScreenStatus = DOOM_SCREEN_ACTIVE;
        if (doomtaskId) osThreadResume(doomtaskId);
        kbtext = lv_textarea_get_text(games->ta);
        if (strncmp(kbtext, "id", 2) == 0)      /* Cheat code must start width "id" */
        {
          while (*kbtext)
          {
            doom_send_cheat_key(*kbtext++);
          }
        }
        lv_obj_add_flag(games->kbd, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(games->ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(games->cheat_btn, LV_OBJ_FLAG_HIDDEN);
#endif
debug_printf("KBD_OK\n");
        break;
      case GUIEV_KBD_CANCEL:
        lv_obj_remove_flag(games->cheat_btn, LV_OBJ_FLAG_HIDDEN);
#ifdef USE_KBD
        lv_obj_add_flag(games->kbd, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(games->ta, LV_OBJ_FLAG_HIDDEN);
#endif
        DoomScreenStatus = DOOM_SCREEN_ACTIVE;
        if (doomtaskId) osThreadResume(doomtaskId);
debug_printf("KBD_CANCEL\n");
        break;
      case GUIEV_CHEAT_SEL:
        kbtext = event.evarg1;

        while (*kbtext)
        {
          doom_send_cheat_key(*kbtext++);
        }
        lv_obj_remove_flag(games->cheat_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(games->cheat_code, LV_OBJ_FLAG_HIDDEN);
        DoomScreenStatus = DOOM_SCREEN_ACTIVE;
        if (doomtaskId) osThreadResume(doomtaskId);
debug_printf("CHEAT_SEL\n");
        break;
      case GUIEV_HID_CONNECTED:
        if (menus->screen)
        {
          if (event.evval0 == 0)
          {
            SetBluetoothButtonState(&setups->bt_button_info, BT_STATE_READY);
#ifdef USE_FUSION
            if (menus->btn_dual)
              lv_obj_add_state(menus->btn_dual, LV_STATE_DISABLED);
#endif
            padInfo = &nullPad;
            if (menus->sub_scr)
              postGuiEventMessage(GUIEV_PADTEST_DONE, 0, NULL, NULL);
          }
          else
          {
            SetBluetoothButtonState(&setups->bt_button_info, BT_STATE_CONNECT);
          }
        }
        break;
      case GUIEV_GAMEPAD_READY:
        padInfo = (GAMEPAD_INFO *)event.evarg1;
        debug_printf("%s Detected.\n", padInfo->name);
#ifdef USE_FUSION
        if (menus->btn_dual)
          lv_obj_clear_state(menus->btn_dual, LV_STATE_DISABLED);
#endif
        break;
      case GUIEV_ENDOOM:
        debug_printf("ENDOOM called.\n");

        /* Prepare ENDOOM screen as LVGL image */
        Board_Endoom(event.evarg1);
        break;
      case GUIEV_DRAW:
        oscDraw(&OSCMScreen, event.evarg1);
        break;
      case GUIEV_AVRCP_CONNECT:
        show_a2dp_buttons();
        break;
      case GUIEV_AVRCP_DISC:
        hide_a2dp_buttons(a2dps);
        break;
      case GUIEV_STREAM_PLAY:
        set_playbtn_state(LV_IMAGEBUTTON_STATE_CHECKED_RELEASED);
        break;
      case GUIEV_STREAM_PAUSE:
        set_playbtn_state(LV_IMAGEBUTTON_STATE_RELEASED);
        break;
      case GUIEV_MUSIC_INFO:
        {
          char *sp;

          sp = (char *) event.evarg1;
          switch (event.evval0)
          {
          case 0:
            set_scroll_labeltext(a2dps->title_label, sp, a2dps->title_font);
            break;
          case 1:
            set_scroll_labeltext(a2dps->artist_label, sp, a2dps->artist_font);
            break;
          }
          free(sp);
        }
        break;
      case GUIEV_ERROR_MESSAGE:
        games->mbox = lv_msgbox_create(NULL);
        lv_msgbox_add_title(games->mbox, "DOOM Error");
        lv_msgbox_add_text(games->mbox,  event.evarg1);
        break;
      case GUIEV_TRACK_CHANGED:
        change_track_cover(a2dps);
        break;
      default:
        debug_printf("Not implemented (%d).\n", event.evcode);
        break;
      }
    }
    else
    {
      new_interval = lv_timer_handler();
      if (new_interval != timer_interval)
      {
        timer_interval = new_interval/2;
      }
    }
  }
  debug_printf("%s: ???\n", __FUNCTION__);
}

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *icons;
  lv_obj_t *title;
  lv_obj_t *artist;
} PLAYER_SCREEN;

PLAYER_SCREEN PlayerScreen;

FS_DIRENT *find_flash_file(char *name)
{
  int i;
  FS_DIRENT *dirent;
  QSPI_DIRHEADER *dirInfo = (QSPI_DIRHEADER *)QSPI_FLASH_ADDR;

  if (dirInfo->fs_magic != DOOMFS_MAGIC)
    return NULL;

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

void gui_timer_inc()
{
  if (lvgl_active)
    lv_tick_inc(1);
}

const GUI_EVENT btstack_ready_event = {
  GUIEV_BTSTACK_READY,
  0,
  NULL,
  NULL
};

void app_btstack_ready()
{
  postGuiEvent(&btstack_ready_event);
}

void StartDoomTask(void *argument)
{       
  doom_main(1, (char **)argument);

  while (1) osDelay(100);
}         
        
void app_endoom(uint8_t *bp)
{       
  postGuiEventMessage(GUIEV_ENDOOM, 0, bp, NULL);
}

/*
 * Called by hid_dualsense to send a LVGL keycode.
 */
void send_padkey(lv_indev_data_t *pdata)
{
  if (padkeyqId)
  {
    osMessageQueuePut(padkeyqId, pdata, 0, 0);
//debug_printf("padkey: %x\n", pdata->key);
  }
}

void app_doom_active()
{
  postGuiEventMessage(GUIEV_DOOM_ACTIVE, 0, NULL, NULL);
}

void postMusicInfo(int code, void *ptr, int size)
{
  uint8_t *bp;

  bp = malloc(size + 1);
  if (bp)
  {
    memcpy(bp, ptr, size);
    bp[size] = 0;
    postGuiEventMessage(GUIEV_MUSIC_INFO, code, bp, NULL);
  }
}

void Display_GamePad_Info(struct gamepad_inputs *rp, uint32_t vbutton)
{ 
  GUI_EVENT ev;

  ev.evcode = GUIEV_PADTEST_UPDATE;
  ev.evval0 = vbutton;
  ev.evarg1 = rp;
  ev.evarg2 = NULL;
  postGuiEvent(&ev);
} 

void app_screenshot()
{       
  postMainRequest(REQ_SCREEN_SAVE, NULL, 0);
}
