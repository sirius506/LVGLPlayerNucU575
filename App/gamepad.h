#ifndef GAMEPAD_H
#define	GAMEPAD_H
#include "lvgl.h"
#include "dualsense_report.h"
#include "dualshock4_report.h"

#define	HID_MODE_LVGL	0
#define	HID_MODE_DOOM	1

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

typedef struct sGamePadInfo {
  char   *name;					// Name of Gamepad
  int    hid_mode;
  const struct sGamePadDriver *padDriver;
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
#ifdef USE_PAD_IMAGE
extern const struct sGamePadImage DualShock4Image;
extern const struct sGamePadImage DualSenseImage;
extern const struct sGamePadImage Zero2Image;
#endif

extern GAMEPAD_INFO *IsSupportedGamePad(uint16_t vid, uint16_t pid);
extern int get_bt_hid_mode();
extern void SDL_JoyStickSetButtons(uint8_t hat, uint32_t vbutton);

extern uint32_t bt_comp_crc(uint8_t *ptr, int len);
extern const PADKEY_DATA PadKeyDefs[];

extern void Display_GamePad_Info(struct gamepad_inputs *rp, uint32_t vbutton);
extern void GamepadHidMode(GAMEPAD_INFO *padInfo, int mode_bit);
extern void gamepad_reset_fusion(void);
void padtest_done();
extern lv_obj_t *padtest_create(GAMEPAD_INFO *padInfo, lv_group_t *g);
extern void padtest_update(GAMEPAD_INFO *padInfo, struct gamepad_inputs *rp, uint32_t vbutton);

#endif
