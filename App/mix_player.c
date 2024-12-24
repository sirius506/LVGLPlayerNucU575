/**
 *  Sound Mixer and Music player task
 */
#include "DoomPlayer.h"
#include "audio_output.h"
#include <arm_math.h>
#include <stdlib.h>
#include <stdint.h>
#include "fft.h"
#include "fatfs.h"
#include "app_music.h"

#define DR_FLAC_IMPLEMENTATION
#define	DR_FLAC_NO_CRC
#define	DR_FLAC_NO_STDIO
#define	DR_FLAC_NO_SIMD
#define	DR_FLAC_NO_OGG

#define	IDLE_PERIOD	50

#include "dr_flac.h"

#define PLAYER_STACK_SIZE  350
#define READER_STACK_SIZE  1400

SECTION_SRDSRAM AUDIO_STEREO FinalAudioBuffer[BUF_FRAMES];

static void mix_half_comp();
static void mix_full_comp();

const AUDIO_INIT_PARAMS doom_audio_params = {
  .buffer = FinalAudioBuffer,
  .buffer_size = sizeof(FinalAudioBuffer),
  .volume = AUDIO_DEF_VOL,
  .sample_rate = 44100,
  .fft_magdiv = 3,
  .txhalf_comp = mix_half_comp,
  .txfull_comp = mix_full_comp,
};

TASK_DEF(mixplayer, PLAYER_STACK_SIZE, osPriorityAboveNormal2)
TASK_DEF(flacreader, READER_STACK_SIZE, osPriorityNormal)

#define MIXREADQ_DEPTH     3

enum {
  READER_START = 1,
  READER_READ,
  READER_STOP,
};

static uint8_t readqBuffer[MIXREADQ_DEPTH * sizeof(uint16_t)];

static AUDIO_STEREO *musicqBuffer[BUF_FACTOR];
static void mix_request_data(int full);

MESSAGEQ_DEF(flacreadq, readqBuffer, sizeof(readqBuffer))
MESSAGEQ_DEF(musicbufq, musicqBuffer, sizeof(musicqBuffer))

Mix_Music *Mix_LoadMUS(const char *file);
int Mix_PlayMusic(Mix_Music *music, int loop);

/*
 * Buffers to store music source data.
 * Decoded FLAC music data is read into MusicFrameBuffer.
 * SilentBuffer is used to generate silience.
 */
SECTION_SRDSRAM static AUDIO_STEREO MusicFrameBuffer[BUF_FRAMES];
static const AUDIO_STEREO SilentBuffer[BUF_FRAMES];

#define	NUMTAPS	31

SECTION_SRDSRAM int16_t   sInBuffer[BUF_FRAMES];
SECTION_SRDSRAM float32_t sFloatBuffer[BUF_FRAMES];
float32_t DeciStateBuffer[BUF_FRAMES+NUMTAPS-1];

float32_t audio_buffer[AUDIO_SAMPLES];	// Decimated Audio samples

/*
 * Buffers for FFT process
 * Note that result buffer contains complex numbers, but its size is same as
 * input butter, because arm_rfft_fast_f32() process takes advantage of the
 * symmetry properties of the FFT.
 */
static float32_t hamming_window[FFT_SAMPLES];
float32_t fft_real_buffer[FFT_SAMPLES];		// FFT input buffer (only real part)
float32_t fft_result_buffer[FFT_SAMPLES];	// FFT result buffer (complex number)

SECTION_DTCMRAM float32_t float_mag_buffer[FFT_SAMPLES/2];

const float32_t Coeffs[NUMTAPS] = {
  0.00156601, -0.00182149,  0.00249659, -0.00359653,  0.00510147, -0.0069667,
  0.00912439, -0.01148685,  0.01395103, -0.01640417,  0.01873013, -0.02081595,
  0.02255847, -0.02387033,  0.02468516,  0.97349755,  0.02468516, -0.02387033,
  0.02255847, -0.02081595,  0.01873013, -0.01640417,  0.01395103, -0.01148685,
  0.00912439, -0.0069667,   0.00510147, -0.00359653,  0.00249659, -0.00182149,
  0.00156601
};

typedef struct {
  float32_t *putptr;
  float32_t *getptr;
  int16_t samples;      /* Accumurated data samples */
  int16_t magdiv;
} FFTINFO;

SECTION_DTCMRAM FFTINFO FftInfo;

typedef struct {
  FIL  *pfile;
  char *comments;
  char *fname;
  int  loop_count;
  int  loop_start;
  int  loop_end;
  int  pcm_pos;
  uint32_t seek_pos;
  osMessageQueueId_t *musicbufqId;
  osMessageQueueId_t *readqId;
} FLACINFO;

FLACINFO FlacInfo;

static SECTION_DTCMRAM arm_fir_decimate_instance_f32 decimate_instance;
static SECTION_DTCMRAM arm_rfft_fast_instance_f32 fft_instance;

typedef enum {
  MIX_DATA_REQ = 1,
  MIX_PLAY,
  MIX_PAUSE,
  MIX_RESUME,
  MIX_HALT,
  MIX_FFT_ENABLE,
  MIX_FFT_DISABLE,
  MIX_SET_VOLUME,
  /* Below events are for A2DP player */
  MIX_SET_POS,
  MIX_STREAM_START,
  MIX_STREAM_DATA,
  MIX_STREAM_STOP,
} mix_event;

typedef struct {
  mix_event event;
  void      *arg;
  int       option;
} MIXCONTROL_EVENT;

MIX_INFO MixInfo;

#define	MIX_EV_DEPTH	5

static osMutexId_t soundLockId;

static MIXCONTROL_EVENT mix_buffer[MIX_EV_DEPTH];

MESSAGEQ_DEF(mixevq, mix_buffer, sizeof(mix_buffer))

uint8_t FlacAllocSpace[DRLIB_HEAP_SIZE];

static uint8_t *flac_allocp;
static uint8_t *max_allocp;

static void *my_malloc(size_t sz, void *pUserData)
{
  UNUSED(pUserData);
  void *p;

  sz = (sz + 3) & ~3;		// Align on word boundary
// debug_printf("%s: %d, %x\n", __FUNCTION__, sz, flac_allocp);
  if (flac_allocp + sz > &FlacAllocSpace[DRLIB_HEAP_SIZE])
  {
    p = NULL;
    debug_printf("alloc failed.\n");
    osDelay(10*1000);
  }
  else
  {
    p = flac_allocp;
    flac_allocp += sz;
    if (flac_allocp > max_allocp)
    {
      max_allocp = flac_allocp;
      //debug_printf("drlib allocsize: %d\n", max_allocp - FlacAllocSpace);
    }
  }
  return p;
}

static void *my_realloc(void *p, size_t sz, void *pUserData)
{
  UNUSED(pUserData);
  void *vp;

  if (p)
  {
    vp = my_malloc(sz, NULL);
    if (vp)
      memcpy(vp, p, sz);
    return vp;
  }
// debug_printf("%s: %d\n", __FUNCTION__, sz);

  return NULL;
}

static void my_free(void *p, void *pUserData)
{
  UNUSED(p);
  UNUSED(pUserData);
  // debug_printf("my_free %x\n", p);
}

static size_t drflac__on_read_fatfs(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    UINT nread;
    FRESULT res;
    FLACINFO *piflac = (FLACINFO *)pUserData;

    res = f_read(piflac->pfile, pBufferOut, bytesToRead, &nread);
    if (res == FR_OK)
      return nread;
    return -1;
}

static drflac_bool32 drflac__on_seek_fatfs(void* pUserData, int offset, drflac_seek_origin origin)
{
    FRESULT res;
    FLACINFO *piflac = (FLACINFO *)pUserData;

    if (origin == drflac_seek_origin_current)
      res = f_lseek(piflac->pfile, f_tell(piflac->pfile) + offset);
    else
      res = f_lseek(piflac->pfile, offset);
    return (res == FR_OK);
}

/*===========================*/

static drflac_result drflac_fatopen(FIL** ppFile, const char* pFilePath)
{
    if (ppFile != NULL) {
        *ppFile = NULL;  /* Safety. */
    }

    if (pFilePath == NULL || ppFile == NULL) {
        return DRFLAC_INVALID_ARGS;
    }

    *ppFile = OpenMusicFile((char *)pFilePath);
    if (*ppFile == NULL) {
        drflac_result result = DRFLAC_DOES_NOT_EXIST;
        if (result == DRFLAC_SUCCESS) {
            result = DRFLAC_ERROR;   /* Just a safety check to make sure we never ever return success when pFile == NULL. */
        }

        return result;
    }

    return DRFLAC_SUCCESS;
}

static drflac_vorbis_comment_iterator CommentIterator;

static void drflac__on_meta(void* pUserData, drflac_metadata* pMetadata)
{
    FLACINFO *piflac = (FLACINFO *)pUserData;

    if (pMetadata->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT)
    {
      const char *cp;
      char *wp, *dp;
      drflac_uint32 clen;

      drflac_init_vorbis_comment_iterator(&CommentIterator, pMetadata->data.vorbis_comment.commentCount, pMetadata->data.vorbis_comment.pComments);
      dp = piflac->comments = (char *)my_malloc(pMetadata->rawDataSize, NULL);
      while (CommentIterator.countRemaining > 0)
      {
        cp = drflac_next_vorbis_comment(&CommentIterator, &clen);
        if (clen > 0)
        {
          memcpy(dp, cp, clen);
          wp = dp;
          dp += clen;
          *dp++ = 0;
          //debug_printf("%s\n", wp);
          if (strncmp(wp, "LOOP_START=", 11) == 0)
            piflac->loop_start = atoi(wp+11);
          else if (strncmp(wp, "LOOP_END=", 9) == 0)
            piflac->loop_end = atoi(wp+9);
        }
      }
    }
}

static drflac* drflac_open_fatfile(const char* pFileName, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    drflac* pFlac;
    FLACINFO *piflac;
    FIL* pFile;

    flac_allocp = &FlacAllocSpace[0];

    if (drflac_fatopen(&pFile, pFileName) != DRFLAC_SUCCESS) {
        return NULL;
    }
    piflac = (FLACINFO *)pAllocationCallbacks->pUserData;
    piflac->pfile = pFile;

    pFlac = drflac_open_with_metadata(drflac__on_read_fatfs, drflac__on_seek_fatfs,  drflac__on_meta, (void*)piflac, pAllocationCallbacks);
    if (pFlac == NULL) {
        CloseMusicFile(pFile);
        return NULL;
    }

    return pFlac;
}

static void drflac_close_fatfs(drflac* pFlac)
{
    if (pFlac == NULL) {
        return;
    }

    FLACINFO *piflac;
    piflac = (FLACINFO *)pFlac->bs.pUserData;

    /*
    If we opened the file with drflac_open_file() we will want to close the file handle. We can know whether or not drflac_open_file()
    was used by looking at the callbacks.
    */
    if (pFlac->bs.onRead == drflac__on_read_fatfs) {
        CloseMusicFile(piflac->pfile);
    }

#ifndef DR_FLAC_NO_OGG
    /* Need to clean up Ogg streams a bit differently due to the way the bit streaming is chained. */
    if (pFlac->container == drflac_container_ogg) {
        drflac_oggbs* oggbs = (drflac_oggbs*)pFlac->_oggbs;
        DRFLAC_ASSERT(pFlac->bs.onRead == drflac__on_read_ogg);

        if (oggbs->onRead == drflac__on_read_fatfs) {
            CloseMusicFile((FIL*)oggbs->pUserData);
        }
    }
#endif

    drflac__free_from_callbacks(pFlac, &pFlac->allocationCallbacks);
}

#if FFT_SAMPLES == 2048
static const int frange[] = { 8, 45, 300, 600, 0 };
#else
static const int frange[] = { 5, 23, 150, 300, 0 };
#endif
static int band_val[4];

int fft_getband(int band)
{
  return band_val[band];
}

int fft_getcolor(uint8_t *p)
{
  int v;

  v = band_val[0] + band_val[1];
  v <<= 1;
  *p++ = (v > 255)? 255 : v;
  v = band_val[2];
  v <<= 1;
  *p++ = (v > 255)? 255 : v;
  v = band_val[3];
  v <<= 1;
  *p = (v > 255)? 255 : v;
  v = band_val[0] + band_val[1] + band_val[2] + band_val[3];
  return v;
}

/*
 *  FFT process.
 *   Original sampling rate is 44.1k. We reduce the rate to 1/3,
 *   resulting as 14.7k.
 */
static int process_fft(FFTINFO *fftInfo, AUDIO_STEREO *pmusic, int frames)
{
  int i;

  HAL_GPIO_TogglePin(TEST0_GPIO_Port, TEST0_Pin);
  /* Convert to mono */
  for (i = 0; i < frames; i++)
  {
    sInBuffer[i] = (pmusic->ch0 + pmusic->ch1) / 2;
    pmusic++;
  }

  /* Convert int16_t values to float, then decimate */
  arm_q15_to_float(sInBuffer, sFloatBuffer, frames);
  arm_fir_decimate_f32(&decimate_instance, sFloatBuffer, fftInfo->putptr, frames);

  fftInfo->putptr += (frames/FFT_DECIMATION_FACTOR);
  if (fftInfo->putptr > &audio_buffer[AUDIO_SAMPLES])
  {
    debug_printf("audio_buffer overflow.\n");
    osDelay(100);
  }
  if (fftInfo->putptr == &audio_buffer[AUDIO_SAMPLES])
  {
    fftInfo->putptr = audio_buffer;
  }
  fftInfo->samples += frames/FFT_DECIMATION_FACTOR;

  if (fftInfo->samples >= FFT_SAMPLES)
  {
    int room;

//    debug_printf("putptr: %x, getptr: %x, samples = %d\n", fftInfo->putptr, fftInfo->getptr, fftInfo->samples);
    room = &audio_buffer[AUDIO_SAMPLES] - fftInfo->getptr;
    if (room >= FFT_SAMPLES)
    {
          arm_mult_f32(fftInfo->getptr, hamming_window, fft_real_buffer, FFT_SAMPLES);
    }
    else
    {
          arm_mult_f32(fftInfo->getptr, hamming_window, fft_real_buffer, room);
          arm_mult_f32(audio_buffer, hamming_window + room, fft_real_buffer + room, FFT_SAMPLES - room);
    }
    arm_rfft_fast_f32(&fft_instance, fft_real_buffer, fft_result_buffer, 0);
    arm_cmplx_mag_f32(fft_result_buffer, float_mag_buffer, FFT_SAMPLES/2);

    {
      int f, f_prev;
      const int *fp;
      int *op;
      float32_t v;

      f_prev = 0;
      fp = frange;
      op = band_val;
      while (*fp)
      {
        f = *fp++;
        v = 0.0;
        for (i = f_prev; i < f; i++)
        {
          v += float_mag_buffer[i];
        }
        if (v) v = v / fftInfo->magdiv;
        if (v < 0) v = 0;
        f_prev = f;
        *op++ = (int16_t)v;
      }
    //debug_printf("BAND: %d, %d, %d, %d\n", band_val[0], band_val[1], band_val[2], band_val[3]);
    }

    fftInfo->getptr += SHIFT_SAMPLES;
    if (fftInfo->getptr >= &audio_buffer[AUDIO_SAMPLES])
      fftInfo->getptr -= AUDIO_SAMPLES;
    fftInfo->samples -= SHIFT_SAMPLES; 
    HAL_GPIO_TogglePin(TEST0_GPIO_Port, TEST0_Pin);
    HAL_GPIO_TogglePin(TEST1_GPIO_Port, TEST1_Pin);
    return 1;	/* New FFT result is ready. */
  }
  HAL_GPIO_TogglePin(TEST0_GPIO_Port, TEST0_Pin);
  return 0;	/* No FFT result available. */
}

/**
 * @brief FLAC file reader task
 */
static void StartFlacReaderTask(void *args)
{
  UNUSED(args);
  FLACINFO *flacInfo = &FlacInfo;
  drflac_uint64 num_read;
  drflac_allocation_callbacks allocationCallbacks;
  drflac *pflac = NULL;
  AUDIO_STEREO *pmusic;
  osStatus_t st;
  uint16_t cmd;
  int i;
  int bindex;

  allocationCallbacks.pUserData = flacInfo;
  allocationCallbacks.onMalloc = my_malloc;
  allocationCallbacks.onRealloc = my_realloc;
  allocationCallbacks.onFree = my_free;

  while (1)
  {
    st = osMessageQueueGet(flacInfo->readqId, &cmd, 0, osWaitForever);

    if (st == osOK)
    {
      switch (cmd)
      {
      case READER_START:
        memset(MusicFrameBuffer, 0, sizeof(MusicFrameBuffer));
        flacInfo->loop_start = flacInfo->loop_end = flacInfo->pcm_pos = 0;
        pflac = drflac_open_fatfile(flacInfo->fname, &allocationCallbacks);
        if (pflac)
        {
          pmusic = MusicFrameBuffer;
          /* Read two frames of music data into MusicFrameBuffer. */
          num_read = drflac_read_pcm_frames_s16(pflac, NUM_FRAMES * BUF_FACTOR, (drflac_int16 *)pmusic);
          for (i = 0; i < BUF_FACTOR; i++)
          {
            osMessageQueuePut(flacInfo->musicbufqId, &pmusic, 0, 0);
            pmusic += NUM_FRAMES;
          }
          bindex = 0;
          flacInfo->pcm_pos = num_read;
        }
debug_printf("loop_count = %d\n", flacInfo->loop_count);
        break;
      case READER_READ:
        if (pflac)
        {
          pmusic = MusicFrameBuffer;
          if (bindex)
            pmusic += NUM_FRAMES;
          num_read = drflac_read_pcm_frames_s16(pflac, NUM_FRAMES, (drflac_int16 *)pmusic);
          if ((flacInfo->loop_count != 0) && (flacInfo->pcm_pos > flacInfo->loop_end))
          {
            /* We have passed loop_end positon, fix the num_read count. */
            num_read -= (flacInfo->pcm_pos - flacInfo->loop_end);
            flacInfo->seek_pos = flacInfo->loop_start;
debug_printf("need seek.\n");
          }
          if (num_read > 0)
          {
            AUDIO_STEREO *mp;

            flacInfo->pcm_pos += num_read;
            mp = pmusic + num_read;
            /* If read data amount is less than NUM_FRAMES,
             * fill with silent data.
             */
            while (num_read < NUM_FRAMES)
            {
              mp->ch0 = 0;
              mp->ch1 = 0;
              mp++;
              num_read++;
            }
            osMessageQueuePut(flacInfo->musicbufqId, &pmusic, 0, 0);
            if (flacInfo->seek_pos)
            {
debug_printf("PCM seek %d\n", flacInfo->seek_pos);
              drflac_seek_to_pcm_frame(pflac, flacInfo->loop_start);
              flacInfo->pcm_pos = flacInfo->loop_start;
              flacInfo->seek_pos = 0;
            }
            bindex ^= 1;
          }
          else
          {
            pmusic = NULL;
            osMessageQueuePut(flacInfo->musicbufqId, &pmusic, 0, 0);
            drflac_close_fatfs(pflac);
            pflac = NULL;
          }
        }
        break;
      case READER_STOP:
#ifdef MIX_DEBUG
        debug_printf("READER_STOP\n");
#endif
        if (pflac)
        {
          drflac_close_fatfs(pflac);
          pflac = NULL;
        }
        break;
      default:
        break;
      }
    }
  }
}

static void mix_half_comp()
{
  mix_request_data(0);
}

static void mix_full_comp()
{
  mix_request_data(1);
}

static void StartMixPlayerTask(void *args)
{
  AUDIO_OUTPUT_DRIVER *pDriver;
  FFTINFO *fftInfo;
  int fft_count;
  MIX_INFO *mixInfo = &MixInfo;
  FLACINFO *flacInfo = &FlacInfo;
  GUI_EVENT guiev;
  int argval;
  uint8_t stream_toggle = 0;
  uint32_t psec;
  AUDIO_CONF *audio_config;
  HAL_DEVICE *haldev = &HalDevice;
  uint16_t cmd;
  int mix_frames;
  AUDIO_INIT_PARAMS *param = (AUDIO_INIT_PARAMS *)args;

  debug_printf("Player Started..\n");

  audio_config = get_audio_config(NULL);

  argval = audio_config->devconf->mix_mode;

  flacInfo->readqId = osMessageQueueNew(MIXREADQ_DEPTH, sizeof(uint16_t), &attributes_flacreadq);
  flacInfo->musicbufqId = osMessageQueueNew(BUF_FACTOR, sizeof(uint32_t *), &attributes_musicbufq);
  mixInfo->musicbufqId = flacInfo->musicbufqId;

  if (haldev->boot_mode == BOOTM_DOOM)
  {
    osThreadNew(StartFlacReaderTask, NULL, &attributes_flacreader);
  }

  fftInfo = &FftInfo;
  fftInfo->magdiv = param->fft_magdiv;
  fft_count = 0;
  arm_fir_decimate_init_f32(&decimate_instance, NUMTAPS, FFT_DECIMATION_FACTOR, (float32_t *)Coeffs, DeciStateBuffer, BUF_FRAMES/2);
  arm_hamming_f32(hamming_window, FFT_SAMPLES);

  pDriver = (AUDIO_OUTPUT_DRIVER *)audio_config->devconf->pDriver;
  mix_frames = NUM_FRAMES;

  mixInfo->mixevqId = osMessageQueueNew(MIX_EV_DEPTH, sizeof(MIXCONTROL_EVENT), &attributes_mixevq);
  mixInfo->volume = AUDIO_DEF_VOL;
  mixInfo->state = MIX_ST_IDLE;

  if (haldev->boot_mode != BOOTM_OSCM)
  {
    mixInfo->volume = param->volume;
    pDriver->Init(audio_config, param);
  }
  else
  {
    pDriver->SetVolume(audio_config, mixInfo->volume);
  }
  soundLockId = audio_config->soundLockId;

  /* We'll keep sending contents of FinalSoundBuffer using DMA */

  if (haldev->boot_mode == BOOTM_DOOM)
  {
    pDriver->Start(audio_config);
  }

  while (1)
  {
    MIXCONTROL_EVENT ctrl;
    AUDIO_STEREO *mp;
    osStatus_t st;

    osMessageQueueGet(mixInfo->mixevqId, &ctrl, 0, osWaitForever);

    switch (ctrl.event)
    {
    case MIX_PLAY:		// Start playing specified FLAC file
#ifdef MIX_DEBUG
      debug_printf("MIX_PLAY\n");
#endif
      if (mixInfo->state != MIX_ST_IDLE)
      {
        debug_printf("MIX_PLAY: Bad state\n");
      }

      if (haldev->boot_mode == BOOTM_DOOM)
      {
        flacInfo->fname = (char *)ctrl.arg;
        flacInfo->loop_count = ctrl.option;
        if (flacInfo->loop_count > 0)
          flacInfo->loop_count--;
      }

      mixInfo->ppos = mixInfo->psec = 0;
      fft_count = 0;

      memset(audio_buffer, 0, sizeof(audio_buffer));
      fftInfo->getptr = fftInfo->putptr = audio_buffer;
      fftInfo->samples = 0;
      arm_rfft_fast_init_f32(&fft_instance, FFT_SAMPLES);

      if (haldev->boot_mode == BOOTM_DOOM)
      {
        cmd = READER_START;
        if (osMessageQueuePut(flacInfo->readqId, &cmd, 0, 0) != osOK)
        {
          debug_printf("readq full\n");
        }
      }
      mixInfo->state = MIX_ST_PLAY;
      break;
    case MIX_DATA_REQ:
      /* Swap freebuffer_ptr value. */
      if (ctrl.option)
        audio_config->freebuffer_ptr = audio_config->sound_buffer + NUM_FRAMES;
      else
        audio_config->freebuffer_ptr = audio_config->sound_buffer;

      switch (mixInfo->state)
      {
      case MIX_ST_PLAY:
        /* Try to get FLAC decoded buffer */
        st = osMessageQueueGet(flacInfo->musicbufqId, &mp, 0, 0);
        if (st == osOK)
        {
          if (mp)
          {
            if (haldev->boot_mode == BOOTM_DOOM)
            {
              /* Got decoded buffer. Request to start decoding next block of frames. */
              cmd = READER_READ;
              if (osMessageQueuePut(flacInfo->readqId, &cmd, 0, 0) != osOK)
              {
                debug_printf("failed to request read.\n");
              }
            }

            if (argval & MIXER_FFT_ENABLE)
            {
              if (process_fft(fftInfo, mp, NUM_FRAMES))
              {
                guiev.evcode = GUIEV_FFT_UPDATE;
                guiev.evval0 = fft_count;
                guiev.evarg1 = band_val;
                postGuiEvent(&guiev);
                fft_count++;
              }
            }
            pDriver->MixSound(audio_config, mp, mix_frames);
            mixInfo->ppos += NUM_FRAMES;
          }
          else
          {
            /* Reached to EOF */
            GUI_EVENT guiev;
  
            mixInfo->state = MIX_ST_IDLE;
            mixInfo->idle_count = IDLE_PERIOD;

debug_printf("Music Finish.\n");
            if (argval & MIXER_FFT_ENABLE)
            {
              guiev.evcode = GUIEV_MUSIC_FINISH;
              postGuiEvent(&guiev);
            }
          }
        }
        else
        {
          mp = NULL;
        }
        if (mp == NULL)
        {
          /* We have no decoded music buffer. Send silent sound. */
          pDriver->MixSound(audio_config, SilentBuffer, mix_frames);
        }
        break;
      case MIX_ST_IDLE:
        if (fftInfo->samples > 0)
        {
          if (argval & MIXER_FFT_ENABLE)
          {
            if (process_fft(fftInfo, (AUDIO_STEREO *)SilentBuffer, NUM_FRAMES))
            {
              guiev.evcode = GUIEV_FFT_UPDATE;
              guiev.evval0 = fft_count;
              guiev.evarg1 = band_val;
              postGuiEvent(&guiev);
              fft_count++;
            }
          }
          if (mixInfo->idle_count > 0)
          {
             mixInfo->idle_count--;
             if (mixInfo->idle_count == 0)
               fftInfo->samples = 0;
          }
        }
        pDriver->MixSound(audio_config, SilentBuffer, mix_frames);
        break;
      default:
        debug_printf("st = %d\n", mixInfo->state);
        break;
      }
      psec = mixInfo->ppos / audio_config->devconf->playRate;
      if (psec > mixInfo->psec)
      {
        mixInfo->psec = psec;
        if (argval & MIXER_FFT_ENABLE)
        {
          postGuiEventMessage(GUIEV_PSEC_UPDATE, psec, mixInfo, NULL);
        }
      }
      break;
    case MIX_PAUSE:
#ifdef MIX_DEBUG
      debug_printf("MIX_PAUSE\n");
#endif
      mixInfo->state = MIX_ST_IDLE;
      mixInfo->idle_count = IDLE_PERIOD;
      break;
    case MIX_RESUME:
#ifdef MIX_DEBUG
      debug_printf("MIX_RESUME\n");
#endif
      mixInfo->state = MIX_ST_PLAY;
      break;
    case MIX_FFT_DISABLE:
      debug_printf("FFT_DISABLE\n");
      argval &= ~MIXER_FFT_ENABLE;
      memset(band_val, 0, sizeof(band_val));
      break;
    case MIX_FFT_ENABLE:
      argval |= MIXER_FFT_ENABLE;
      break;
    case MIX_HALT:
#ifdef MIX_DEBUG
      debug_printf("MIX_HALT\n");
#endif
      if (haldev->boot_mode == BOOTM_DOOM)
      {
        cmd = READER_STOP;
        if (osMessageQueuePut(flacInfo->readqId, &cmd, 0, 0) != osOK)
        {
          debug_printf("readq full\n");
        }
      }
      mixInfo->state = MIX_ST_IDLE;
      mixInfo->idle_count = IDLE_PERIOD;
      break;
    case MIX_SET_VOLUME:
#ifdef MIX_DEBUG
      debug_printf("SetVolume: %d\n", ctrl.arg);
#endif
      mixInfo->volume = (int) ctrl.arg;
      pDriver->SetVolume(audio_config, mixInfo->volume);
      break;
    case MIX_SET_POS:
      mixInfo->song_len = (uint32_t)ctrl.arg * 441 / 10;	// Convert to samples
      mixInfo->ppos = ctrl.option * 441 / 10;		// Convert to samples
      mixInfo->psec = mixInfo->ppos / 44100;		// Convert to secs
      postGuiEventMessage(GUIEV_PSEC_UPDATE, mixInfo->psec, mixInfo, NULL);
      break;
    case MIX_STREAM_START:
      fft_count = 0;
      fftInfo->getptr = fftInfo->putptr = audio_buffer;
      fftInfo->samples = 0;
      arm_rfft_fast_init_f32(&fft_instance, FFT_SAMPLES);
      break;
    case MIX_STREAM_DATA:
      mp = (AUDIO_STEREO *)ctrl.arg;
      if (ctrl.option != NUM_FRAMES)
        debug_printf("%d frames.\n", ctrl.option);
      if (stream_toggle && (process_fft(fftInfo, mp, ctrl.option)))
      {
        guiev.evcode = GUIEV_FFT_UPDATE;
        guiev.evval0 = fft_count;
        guiev.evarg1 = band_val;
        postGuiEvent(&guiev);
        fft_count++;
      }
      stream_toggle ^= 1;
      mixInfo->ppos += ctrl.option;
      psec = mixInfo->ppos / 44100;
      if (psec != mixInfo->psec)
      {
        mixInfo->psec = psec;
        postGuiEventMessage(GUIEV_PSEC_UPDATE, psec, mixInfo, NULL);
      }
      break;
    case MIX_STREAM_STOP:
      break;
    default:
      debug_printf("event = %x\n", ctrl.event);
      break;
    }
  }
  pDriver->Stop(audio_config);
}

int Mix_GetVolume()
{
  return MixInfo.volume;
}

/*
 * Public functions to suppoort SDL_Mixer API.
 */

static MUSIC_STATE play_state;

void Mix_FFT_Enable()
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_FFT_ENABLE;
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
}

void Mix_FFT_Disable()
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_FFT_DISABLE;
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
}

int Mix_PlayMusic(Mix_Music *music, int loop)
{
  MIXCONTROL_EVENT mixc;
  int st;

  mixc.event = MIX_PLAY;
  mixc.arg = (void *)music->fname;
  mixc.option = loop;
  play_state = music->state = MST_PLAYING;
  if (music->magic != MIX_MAGIC)
     debug_printf("Bad magic.\n");
  st = osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0);
  if (st != osOK)
    debug_printf("failed to put. %d\n", st);
  return 0;
}

int Mix_ResumeMusic(Mix_Music *music)
{
  MIXCONTROL_EVENT mixc;

  switch (music->state)
  {
  case MST_LOADED:
    Mix_PlayMusic(music, 0);
    break;
  case MST_PAUSED:
    mixc.event = MIX_RESUME;
    play_state = music->state = MST_PLAYING;
    if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
    {
      debug_printf("%s: put failed.\n", __FUNCTION__);
    }
    break;
  default:
    break;
  }
  return 0;
}

int Mix_PauseMusic(Mix_Music *music)
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_PAUSE;
  play_state = music->state = MST_PAUSED;
  if (music->magic != MIX_MAGIC)
     debug_printf("Bad magic.\n");
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
  return 0;
}

void Mix_HaltMusic()
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_HALT;
  mixc.arg = NULL;
  play_state = MST_INIT;
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
}

Mix_Music *Mix_LoadMUS(const char *file)
{
  Mix_Music *music;

  music = (Mix_Music *)malloc(sizeof(Mix_Music));
  if (music)
  {
    music->magic = MIX_MAGIC;
    music->fname = file;
    play_state = music->state = MST_LOADED;
  }
  else
  {
    debug_printf("%s: no mem.\n", __FUNCTION__);
    while (1) osDelay(200);
  }
  return music;
}

int Mix_PlayingMusic()
{
  return (play_state == MST_PLAYING)? 1 : 0;
}

int Mix_VolumeMusic(int volume)
{
#if 0
  MIXCONTROL_EVENT mixc;
#endif

  if (volume < 0)
    volume = 75;

#if 0
  mixc.event = MIX_SET_VOLUME;
  mixc.arg = (void *)volume;
  if (osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0) != osOK)
  {
    debug_printf("%s: put failed.\n", __FUNCTION__);
  }
#endif

  return volume;
}

void Mix_FreeMusic(Mix_Music *music)
{
  if (music )
  {
     if (music->magic != MIX_MAGIC)
       debug_printf("BAD MAGIC %x\n", music->magic);
     music->magic = 0;
     free(music);
  }
}

static osThreadId_t mixid;

void Start_SDLMixer()
{
  if (mixid == 0)
  {
    mixid = osThreadNew(StartMixPlayerTask, (void *)&doom_audio_params, &attributes_mixplayer);
  }
}

void Start_A2DPMixer(AUDIO_INIT_PARAMS *params)
{
    osThreadNew(StartMixPlayerTask, (void *)params, &attributes_mixplayer);
}

void Start_Doom_SDLMixer()
{
  if (mixid == 0)
  {
    mixid = osThreadNew(StartMixPlayerTask, (void *)&doom_audio_params, &attributes_mixplayer);
  }
}

int Mix_Started()
{
  return (int)mixid;
}

void Stop_SDLMixer()
{
  osThreadTerminate(mixid);
  mixid = 0;
}

CHANINFO ChanInfo[NUM_CHANNELS];

#define	MIX_DEBUGx

void LockChanInfo()
{
  osMutexAcquire(soundLockId, osWaitForever);
}

void UnlockChanInfo()
{
  osMutexRelease(soundLockId);
}

/**
 * @brief Start playing given sound chunk. This function is called by DOOM i_sdlsound.c
 */
int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
#ifdef MIX_DEBUG
  debug_printf("%s: %d, %d\n", __FUNCTION__, channel, chunk->alen);
#endif
  osMutexAcquire(soundLockId, osWaitForever);
  ChanInfo[channel].chunk = chunk;
  ChanInfo[channel].loop = loops;
  ChanInfo[channel].pread = (AUDIO_STEREO *)chunk->abuf;
  ChanInfo[channel].plast = (AUDIO_STEREO *)(chunk->abuf + chunk->alen);
  ChanInfo[channel].flag |= FL_SET|FL_PLAY;
  osMutexRelease(soundLockId);
  return channel;
}

/**
 * @brief Return current sound playing sample poistion in pusedoRate.
 */
int Mix_PlayPosition(int channel)
{
  Mix_Chunk *chunk;
  int pos;

  osMutexAcquire(soundLockId, osWaitForever);
  chunk = ChanInfo[channel].chunk;
  pos = ChanInfo[channel].pread - (AUDIO_STEREO *)chunk->abuf;
  osMutexRelease(soundLockId);

  return pos;
}

int Mix_LoadChannel(int channel, Mix_Chunk *chunk, int loops)
{
#ifdef MIX_DEBUG
  debug_printf("%s: %d, %d\n", __FUNCTION__, channel, chunk->alen);
#endif
  osMutexAcquire(soundLockId, osWaitForever);
  ChanInfo[channel].chunk = chunk;
  ChanInfo[channel].loop = loops;
  ChanInfo[channel].flag = FL_SET;
  ChanInfo[channel].pread = (AUDIO_STEREO *)chunk->abuf;
  ChanInfo[channel].plast = (AUDIO_STEREO *)(chunk->abuf + chunk->alen);
  ChanInfo[channel].vol_left = MIX_MAX_VOLUME;
  ChanInfo[channel].vol_right = MIX_MAX_VOLUME;
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_ResumeChannel(int channel)
{
  osMutexAcquire(soundLockId, osWaitForever);
#if 1
  ChanInfo[channel].flag |= FL_SET|FL_PLAY;
#else
  ChanInfo[channel].flag |= FL_PLAY;
#endif
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_PauseChannel(int channel)
{
  osMutexAcquire(soundLockId, osWaitForever);
#if 0
  ChanInfo[channel].flag &= ~FL_SET;
#else
  ChanInfo[channel].flag &= ~FL_PLAY;
#endif
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_HaltChannel(int channel)
{
#ifdef MIX_DEBUG
  debug_printf("%s: %d\n", __FUNCTION__, channel);
#endif

  osMutexAcquire(soundLockId, osWaitForever);
  if (channel >= 0)
  {
    ChanInfo[channel].flag &= ~(FL_PLAY|FL_SET);
  }
  else	/* Close all channels */
  {
    for (channel = 0; channel < NUM_CHANNELS; channel++)
      ChanInfo[channel].flag &= ~(FL_PLAY|FL_SET);
  }
  osMutexRelease(soundLockId);
  return 0;
}

/*
 * @brief See if given sound chanel is busy. Called by i_sdlsound.c
 */
int Mix_Playing(int channel)
{
  //debug_printf("%s %d:\n", __FUNCTION__, channel);
  if ((ChanInfo[channel].flag & (FL_SET|FL_PLAY)) == (FL_SET|FL_PLAY))
    return 1;
  return 0;
}

int Mix_SetPanning(int channel, uint8_t left, uint8_t right)
{
  CHANINFO *cinfo = &ChanInfo[channel];
  int res = 0;

#ifdef MIX_DEBUG
  debug_printf("%s: %d (%d, %d)\n", __FUNCTION__, channel, left, right);
#endif
  osMutexAcquire(soundLockId, osWaitForever);
  if (cinfo->pread < cinfo->plast)
  {
    cinfo->vol_left = left;
    cinfo->vol_right = right;
    //cinfo->flag |= FL_PLAY;
    res = 1;
  }
  osMutexRelease(soundLockId);
  return res;
}

void Mix_CloseAudio()
{
  debug_printf("%ss:\n", __FUNCTION__);
}

int Mix_AllocateChannels(int chans)
{
  int i;

  osMutexAcquire(soundLockId, osWaitForever);
  for (i = 0; i < chans; i++)
    ChanInfo[i].flag = FL_ALLOCED;
  osMutexRelease(soundLockId);
  return 1;
}

static void mix_request_data(int full)
{
  MIXCONTROL_EVENT evcode;
  int st;

  evcode.event = MIX_DATA_REQ;
  evcode.option = full;
  st = osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
  if (st != osOK)
    debug_printf("mix request failed (%d)\n", st);
}

int Mix_QueryFreq()
{
  AUDIO_CONF *aconf;

  aconf = get_audio_config(NULL);
  return aconf->devconf->playRate;
}

void Mix_Set_Position(int length, int pos)
{
  MIXCONTROL_EVENT evcode;

  evcode.event = MIX_SET_POS;
  evcode.arg = (void *)length;
  evcode.option = pos;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}

static AUDIO_STEREO *audio_bufp;

void Mix_Stream_Data(uint8_t *ptr, int num_frames)
{
  MIXCONTROL_EVENT evcode;

  memcpy(audio_bufp, ptr, num_frames * sizeof(AUDIO_STEREO));
  evcode.event = MIX_STREAM_DATA;
  evcode.arg = (void *)audio_bufp;
  audio_bufp += num_frames;
  if (audio_bufp >= &MusicFrameBuffer[BUF_FRAMES])
    audio_bufp = MusicFrameBuffer;

  evcode.option = num_frames;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}

void Mix_Stream_Start()
{
  MIXCONTROL_EVENT evcode;

  audio_bufp = MusicFrameBuffer;
  evcode.event = MIX_STREAM_START;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}

void Mix_Stream_Stop()
{
  MIXCONTROL_EVENT evcode;

  evcode.event = MIX_STREAM_STOP;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}
