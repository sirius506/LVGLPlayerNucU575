#ifndef APP_MUSIC_H
#define APP_MUSIC_H
#include "doominfo.h"

/* Cover file must be ARGB8888 binary format and 128x128 pixels */
//#define	COVER_MAGIC	((LV_COLOR_FORMAT_ARGB8888<<8) |LV_IMAGE_HEADER_MAGIC)
/* Cover file must be ARGB565 binary format and 128x128 pixels */
#define	COVER_MAGIC	((LV_COLOR_FORMAT_RGB565A8<<8) |LV_IMAGE_HEADER_MAGIC)

typedef struct sCover {
  char          *fname;
  uint8_t       *faddr;
  uint32_t      fsize;
  struct sCover *next;
} COVER_INFO;

typedef struct {
  char *path;
  char *title;
  char *artist;
  char *album;
  int  track;
  uint32_t samples;
  lv_group_t *main_group;	// Input group when player screen is active
  lv_group_t *list_group;	// Input group when music list is active
  lv_indev_t *kdev;
  lv_obj_t *main_cont;
} MUSIC_INFO;

typedef enum {
  MIX_ST_IDLE = 0,
  MIX_ST_PLAY,                  // Music beeing played.
  MIX_ST_PAUSE,
} mix_state;

typedef struct {
  mix_state state;
  osMessageQueueId_t mixevqId;
  osMessageQueueId_t *musicbufqId;
  uint32_t  ppos;               // Current play position in samples
  uint16_t  psec;               // Current play position in seconds
  uint16_t  idle_count;
  uint16_t  volume;             // Volume value 0..100
  uint32_t  song_len;
} MIX_INFO;

#define LV_DEMO_MUSIC_LANDSCAPE 1
#if LV_DEMO_MUSIC_LARGE
#  define LV_DEMO_MUSIC_HANDLE_SIZE  40
#else
#  define LV_DEMO_MUSIC_HANDLE_SIZE  20
#endif

void _lv_demo_music_play(uint32_t id);
void _lv_demo_music_pause(void);
void _lv_demo_music_resume(void);
uint32_t _lv_demo_music_get_track_count();
const char * _lv_demo_music_get_title(uint32_t track_id);
const char * _lv_demo_music_get_artist(uint32_t track_id);
const char * _lv_demo_music_get_genre(uint32_t track_id);
const char * _lv_demo_music_get_path(uint32_t track_id);
uint32_t _lv_demo_music_get_track_length(uint32_t track_id);

extern int register_cover_file();
extern COVER_INFO *track_cover(int track);

extern MUSIC_INFO *MusicInfo;
extern void Start_Doom_SDLMixer();
extern void Start_SDLMixer();
#endif
