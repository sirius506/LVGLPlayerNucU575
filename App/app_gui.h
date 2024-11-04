#ifndef APP_GUI_H
#define APP_GUI_H
#include "bsp.h"
#include "target.h"

#include "lvgl.h"

#define	W_PERCENT(x)	(DISP_HOR_RES * x / 100)
#define	H_PERCENT(x)	(DISP_VER_RES * x / 100)

#define	ZOOM_BASE	LV_IMG_ZOOM_NONE
#define JOY_RAD         25
#define JOY_DIA         51
#define JOY_DIV         5

typedef struct {
  const lv_font_t *font_title;
  const lv_font_t *font_small;
  const lv_font_t *font_large;

  int16_t  spinner_width;

  /* Menu screen */
  uint16_t mb_yoffset;
  uint16_t mb_height;
  uint16_t mb_olw;              // Menu butoon outline width
} GUI_LAYOUT;

/* Main task request commands definitions */

typedef enum {
  REQ_VERIFY_SD = 1,            // Verify SD card contents
  REQ_VERIFY_FLASH,             // Verify OCTO SPI flash contents
  REQ_ERASE_FLASH,              // Erase OCTO SPI flash contents
  REQ_COPY_FLASH,               // Copy SD game image onto SPI flash
  REQ_END_DOOM,
  REQ_TOUCH_INT,
  REQ_DUMMY,
  REQ_CAPTURE_SAVE,
  REQ_SCREEN_SAVE,
  REQ_VERIFY_FONT,
} REQ_CODE;

typedef struct {
  REQ_CODE  cmd;
  void      *arg;
  int       val;
} REQUEST_CMD;

#define ICON_SET                0x80
#define ICON_CLEAR              0x00
#define ICON_USB                0x20
#define ICON_BLUETOOTH          0x10
#define ICON_BATTERY            0x08
#define ICON_BATTERY_MASK       0x07

/* GUI task event definitions */

typedef enum {
  GUIEV_DOOM_REFRESH = 1,
  GUIEV_DOOM_ACTIVE,
  GUIEV_BTSTACK_READY,		// BTStack Ready
  GUIEV_BTDEV_CONNECTED,	// Bluetooth device connected/disconnected
  GUIEV_GAMEPAD_READY,          // Gamepad controller has found
  GUIEV_APP_SELECT,
  GUIEV_SD_REPORT,              // Report REQ_VERIFY_SD result
  GUIEV_FLASH_REPORT,           // Report REQ_VERIFY_FLASH result
  GUIEV_FONT_REPORT,
  GUIEV_OSCM_START,
  GUIEV_OSCM_FILE,
  GUIEV_ICON_CHANGE,            // Set/Change Icon label
  GUIEV_MPLAYER_START,          // Start Music Player
  GUIEV_SPLAYER_START,          // Start Sound Player
  GUIEV_MPLAYER_DONE,
  GUIEV_MUSIC_FINISH,
  GUIEV_ERASE_START,
  GUIEV_ERASE_REPORT,
  GUIEV_COPY_REPORT,
  GUIEV_GAME_START,
  GUIEV_CHEAT_BUTTON,
  GUIEV_CHEAT_ACK,
  GUIEV_FFT_UPDATE,
  GUIEV_PSEC_UPDATE,
  GUIEV_REBOOT,
  GUIEV_FLASH_GAME_SELECT,      // Flash game image selected
  GUIEV_SD_GAME_SELECT,         // SD game image selected
  GUIEV_REDRAW_START,
  GUIEV_LEFT_XDIR,
  GUIEV_LEFT_YDIR,
  GUIEV_RIGHT_XDIR,
  GUIEV_RIGHT_YDIR,
  GUIEV_KBD_OK,
  GUIEV_KBD_CANCEL,
  GUIEV_CHEAT_SEL,
  GUIEV_ENDOOM,
  GUIEV_LVGL_CAPTURE,
  GUIEV_DOOM_CAPTURE,
  GUIEV_ERROR_MESSAGE,
  GUIEV_DRAW,

  GUIEV_AVRCP_CONNECT,
  GUIEV_AVRCP_DISC,
  GUIEV_MUSIC_INFO,
  GUIEV_STREAM_PLAY,
  GUIEV_STREAM_PAUSE,
  GUIEV_TRACK_CHANGED,
} GUIEV_CODE;

typedef struct {
  GUIEV_CODE evcode;
  uint32_t   evval0;
  void       *evarg1;
  void       *evarg2;
} GUI_EVENT;

#define OP_ERROR        1       
#define OP_START        2       
#define OP_PROGRESS     3       
#define OP_DONE         4

typedef struct {
  lv_obj_t   *screen;
  lv_group_t *ing;              // Input group
} BASE_SCREEN;

typedef struct {
  lv_obj_t *screen;
  lv_group_t *ing;              // Input group
  lv_obj_t *title;
  lv_obj_t *mbox;       /* Message box object */
  lv_obj_t *btn;
  lv_obj_t *spinner;
} START_SCREEN;

typedef struct {
  lv_obj_t *screen;
  lv_group_t *ing;              // Input group
  lv_obj_t *title;
  lv_obj_t *btn_music;
  lv_obj_t *btn_sound;
  lv_obj_t *btn_dual;
  lv_obj_t *btn_game;
  lv_obj_t *cont_bt;
  lv_obj_t *sub_scr;
  lv_obj_t *play_scr;
  lv_group_t *player_ing;       // Input group for player
} MENU_SCREEN;

typedef struct {
  lv_obj_t *screen;
  lv_group_t *ing;              // Input group
  lv_obj_t *title;
  lv_obj_t *operation;
  lv_obj_t *fname;
  lv_obj_t *mbox;       /* Message box object */
  lv_obj_t *bar;
  lv_obj_t *progress;
} COPY_SCREEN;

typedef struct {
  lv_obj_t *screen;
  lv_group_t *ing;              // Input group
  lv_obj_t *cheat_btn;
  lv_obj_t *kbd;
  lv_obj_t *ta;
  lv_obj_t *cheat_code;
  lv_obj_t *img;
  lv_obj_t *mbox;       /* Message box object */
} GAME_SCREEN;

typedef struct {
  lv_obj_t *screen;
  lv_group_t *ing;              // Input group
} SOUND_SCREEN;

typedef struct {
  lv_obj_t  *screen;
  lv_group_t *ing;              // Input group
  lv_font_t *title_font;
  lv_font_t *artist_font;
  lv_obj_t  *title_label;
  lv_obj_t  *artist_label;
  int       cover_count;
  int       cur_cover;
} A2DP_SCREEN;

typedef struct {
  lv_obj_t *scope_screen;
  lv_group_t *scope_ing;        /* Input group for scope screen */
  lv_obj_t *mlist_screen;
  lv_group_t *list_ing;		/* Input group for list screen */
  HAL_DEVICE *haldev;
  lv_obj_t *scope_image;
  lv_obj_t *scope_label;
  lv_obj_t *progress_bar;
  lv_obj_t *play_button;
  lv_obj_t *prev_button;
  lv_obj_t *next_button;
  lv_indev_t *keydev;
  uint8_t  disp_toggle;
} OSCM_SCREEN;

#define KBDEVENT_DOWN   0
#define KBDEVENT_UP     1
  
typedef struct {
  uint8_t evcode;
  uint8_t asciicode;
  uint8_t doomcode;
} KBDEVENT;

extern const GUI_LAYOUT GuiLayout;

void postMainRequest(int cmd, void *arg, int val);
void postGuiEvent(const GUI_EVENT *event);
void postGuiEventMessage(GUIEV_CODE evcode, uint32_t evval0, void *evarg1, void *evarg2);
void postMusicInfo(int code, void *ptr, int size);
void music_process_stick(int evcode, int direction);
void LoadMusicConfigs();
int ReadMusicList(char *filename);
lv_obj_t *sound_screen_create(lv_obj_t *parent, lv_group_t *ing, lv_style_t *btn_style);
lv_obj_t * _lv_demo_music_main_create(lv_obj_t * parent, lv_group_t *g, lv_style_t *btn_style);
void _lv_demo_music_list_btn_check(lv_obj_t *list, uint32_t track_id, bool state);
void _lv_demo_inter_pause_start();
void sound_process_stick(int evcode, int direction);
void app_psec_update(int tv);
void app_spectrum_update(int v);
void send_padkey(lv_indev_data_t *pdata);
void doom_send_cheat_key(char ch);
void app_screenshot();
void app_btstack_ready();
void activate_new_screen(BASE_SCREEN *base, void (*list_action)(), void *arg_ptr);

#endif
