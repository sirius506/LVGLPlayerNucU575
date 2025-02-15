#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H
#include "SDL_mixer.h"

#define NUM_CHANNELS    16

#define	AUDIO_DEF_VOL	60

typedef struct {
  int16_t ch0;
  int16_t ch1;
} AUDIO_STEREO;

typedef struct {
  int flag;
  Mix_Chunk *chunk;
  int loop;
  uint8_t   vol_left, vol_right;
  AUDIO_STEREO   *pread;
  AUDIO_STEREO   *plast;
} CHANINFO;

#define	FL_ALLOCED	(1<<0)	/* Sound channel allocated */
#define FL_SET          (1<<1)	/* Sound PCM loaded */
#define FL_PLAY         (1<<2)	/* Sound is playing */

#define BUF_MSEC        10      /* Hold 10ms worth PCM samples in one frame */
#define NUM_FRAMES      441     /* 10ms worth PCM samples */
#define BUF_FACTOR      2       /* Two for double buffering */
#define BUF_FRAMES      (NUM_FRAMES*BUF_FACTOR)

typedef enum {
  AUDIO_ST_INIT = 0,
  AUDIO_ST_IDLE,
  AUDIO_ST_PLAY,
  AUDIO_ST_PAUSE,
  AUDIO_ST_STOP,
} AUDIO_STATUS;

typedef struct {
  const struct s_adevconf   *devconf;
  HAL_DEVICE      *haldev;
  AUDIO_STATUS    status;
  AUDIO_STEREO    *sound_buffer;
  int             sound_buffer_size;
  AUDIO_STEREO    *freebuffer_ptr;
  AUDIO_STEREO    *playbuffer_ptr;
  uint16_t        volume;		/* Current volume value */
  uint32_t        sample_rate;		/* Sampling rate */
  osMutexId_t     soundLockId;
} AUDIO_CONF;

typedef struct {
  AUDIO_STEREO *buffer;
  int      buffer_size;
  int      volume;
  uint32_t sample_rate;
  uint32_t fft_magdiv;
  void   (*txhalf_comp)();
  void   (*txfull_comp)();
} AUDIO_INIT_PARAMS;

typedef struct {
  void    (*Init)(AUDIO_CONF *audio_conf, const AUDIO_INIT_PARAMS *parm);
  void    (*Start)(AUDIO_CONF *audio_conf);
  void    (*Stop)(AUDIO_CONF *audio_conf);
  void    (*Pause)(AUDIO_CONF *audio_conf);
  void    (*Resume)(AUDIO_CONF *audio_conf);
  void    (*MixSound)(AUDIO_CONF *audio_conf, const AUDIO_STEREO *psrc, int num_frame);
  void    (*SetVolume)(AUDIO_CONF *audio_conf, int vol);
  int     (*GetVolume)(AUDIO_CONF *audio_conf);
  void    (*SetRate)(AUDIO_CONF *audio_conf, uint32_t rate);
  uint32_t (*GetRate)(AUDIO_CONF *audio_conf);
} AUDIO_OUTPUT_DRIVER;

typedef struct s_adevconf {
  uint16_t mix_mode;
  uint16_t playRate;
  uint16_t numChan;
  const AUDIO_OUTPUT_DRIVER *pDriver;
} AUDIO_DEVCONF;

AUDIO_CONF *get_audio_config(HAL_DEVICE *haldev);
AUDIO_OUTPUT_DRIVER *get_audio_driver();
extern CHANINFO ChanInfo[NUM_CHANNELS];

extern void bsp_pause_audio(HAL_DEVICE *haldev);
extern void bsp_resume_audio(HAL_DEVICE *haldev);

#endif
