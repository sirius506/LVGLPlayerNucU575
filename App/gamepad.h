#ifndef GAMEPAD_H
#define	GAMEPAD_H
#include "lvgl.h"
#include "dualsense_report.h"
#include "dualshock4_report.h"
#ifdef USE_FUSION
#include "Fusion.h"
#endif

#define	HID_MODE_LVGL	0
#define	HID_MODE_TEST	1
#define	HID_MODE_DOOM	2

#define	VID_SONY	0x054C
#define	PID_DUALSHOCK	0x09CC
#define	PID_DUALSENSE	0x0CE6
#define	PID_ZERO2	0x05C4

/*
 * Vertual Button bitmask definitions
 */
#define	VBMASK_SQUARE	(1<<0)
#define	VBMASK_CROSS	(1<<1)
#define	VBMASK_CIRCLE	(1<<2)
#define	VBMASK_TRIANGLE	(1<<3)
#define	VBMASK_L1	(1<<4)
#define	VBMASK_R1	(1<<5)
#define	VBMASK_L2	(1<<6)
#define	VBMASK_R2	(1<<7)
#define	VBMASK_SHARE	(1<<8)
#define	VBMASK_OPTION	(1<<9)
#define	VBMASK_L3	(1<<10)
#define	VBMASK_R3	(1<<11)
#define	VBMASK_PS	(1<<12)
#define VBMASK_TOUCH    (1<<13)
#define VBMASK_MUTE     (1<<14)
#define VBMASK_UP       (1<<15)
#define VBMASK_LEFT     (1<<16)
#define VBMASK_RIGHT    (1<<17)
#define VBMASK_DOWN     (1<<18)

#define	NUM_VBUTTONS	15	// Number of virtual buttons exclude direciton keys
#define	NUM_PAD_BUTTONS	19	// include direction keys

typedef struct {
  uint8_t  *ptr;
  uint16_t len;
  uint8_t  hid_mode;
} HID_REPORT;

struct sGamePadDriver {
  void (*DecodeInputReport)(HID_REPORT *report);
  void (*btSetup)(uint16_t cid);
  void (*btProcessGetReport)(const uint8_t *report, int len);
  void (*ResetFusion)(void);
  void (*btDisconnect)(void);
};

#define	PADIMG_IMU	(1<<0)
#define	PADIMG_STICK	(1<<1)
#define	PADIMG_TPAD	(1<<2)

/**
 *   GamePad Image Information
 */
struct sGamePadImage {
  char       *ibin_name;	// Name of Image file
  uint16_t   ibin_width;	// Width of Image bitmap
  uint16_t   ibin_height;	// Height of Image bitmap
  uint8_t    image_flag;
  uint16_t   tpad_hor_res;	// Touchpad horizonatal resolution
  uint16_t   tpad_ver_res;	// Touchpad vertical resolution
  uint16_t   tpad_width;	// Touchpad image width
  uint16_t   tpad_height;	// Touchpad image height
  uint16_t   tpad_xpos;
  uint16_t   tpad_ypos;
  uint16_t   lstick_x;		// Left stick xpos
  uint16_t   rstick_x;		// Right stick xpos
  uint16_t   stick_y;		// Left/Right stick ypos
  lv_point_t ButtonPositions[NUM_PAD_BUTTONS];
};

typedef struct sGamePadInfo {
  char   *name;					// Name of Gamepad
  int    hid_mode;
  const struct sGamePadDriver *padDriver;
  const struct sGamePadImage  *padImage;
  lv_obj_t *img;	/* Game Pad Image object */
  lv_obj_t *pad1;	/* LED object for two pad touch positions */
  lv_obj_t *pad2;
  lv_obj_t *yaw;
  lv_obj_t *pitch;
  lv_obj_t *roll_bar;
  lv_obj_t *joyleft;
  lv_obj_t *joyright;
  lv_obj_t *ButtonLeds[NUM_PAD_BUTTONS];
} GAMEPAD_INFO;

typedef struct {
  uint32_t mask;
  lv_key_t lvkey;
} PADKEY_DATA;

struct sGamePad {
  uint16_t  vid;
  uint16_t  pid;
  char      *name;
  const struct sGamePadDriver *padDriver;
  const struct sGamePadImage  *padImage;
};

struct gamepad_touch_point {
  uint8_t  contact;
  uint16_t xpos;
  uint16_t ypos;
};

struct gamepad_inputs {
  uint8_t x, y;
  uint8_t rx, ry;
  uint8_t z, rz;
  uint32_t vbutton;
  /* Motion sensors */
  int16_t gyro[3];
  int16_t accel[3];
  struct  gamepad_touch_point points[2];
  uint8_t Temperature;
  uint8_t battery_level;
};


typedef struct {
  int16_t roll;
  int16_t pitch;
  int16_t yaw;
} FUSION_ANGLE;

extern FUSION_ANGLE ImuAngle;

#define	CALIB_BUF_SIZE	(DS_FEATURE_REPORT_CALIBRATION_SIZE+3)
#define	INREP_SIZE	sizeof(struct dualsense_input_report)
#define	OUTREP_SIZE	sizeof(struct dualsense_btout_report)

typedef struct {
  uint8_t InputReportBuffer[INREP_SIZE];
  uint8_t OutputReportBuffer[OUTREP_SIZE*2];
  uint8_t out_toggle;
} GAMEPAD_BUFFERS;

extern GAMEPAD_BUFFERS *GetGamePadBuffer();

extern const struct sGamePadDriver DualShockDriver;
extern const struct sGamePadDriver DualSenseDriver;
extern const struct sGamePadDriver Zero2Driver;
extern const struct sGamePadImage DualShock4Image;
extern const struct sGamePadImage DualSenseImage;
extern const struct sGamePadImage Zero2Image;

extern GAMEPAD_INFO *IsSupportedGamePad(uint16_t vid, uint16_t pid);
extern int get_bt_hid_mode();
extern void SDL_JoyStickSetButtons(uint8_t hat, uint32_t vbutton);

extern uint32_t bt_comp_crc(uint8_t *ptr, int len);
extern const PADKEY_DATA PadKeyDefs[];

#ifdef USE_FUSUION
extern void setup_fusion(int sample_rate, const FusionAhrsSettings *psettings);
extern void gamepad_process_fusion(float sample_period, FusionVector gyroscope, FusionVector accelerometer);
#endif

extern void Display_GamePad_Info(struct gamepad_inputs *rp, uint32_t vbutton);
extern void GamepadHidMode(GAMEPAD_INFO *padInfo, int mode_bit);
extern void gamepad_reset_fusion(void);
void padtest_done();
extern lv_obj_t *padtest_create(GAMEPAD_INFO *padInfo, lv_group_t *g);
extern void padtest_update(GAMEPAD_INFO *padInfo, struct gamepad_inputs *rp, uint32_t vbutton);

#endif
