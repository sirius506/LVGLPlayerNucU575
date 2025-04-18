/**
 *  Oscilloscope Music & Lissagious Sound Player
 */
#include "DoomPlayer.h"
#include "board_if.h"
#include "fatfs.h"
#include "audio_output.h"
#include "app_setup.h"
#include <arm_math.h>

#define AUDIO_FRAME_SIZE        (192*6) /* 16bit, 192K sampling, 6ms */
#define OSC_BUF_FACTOR      4

#define	LISA_SAMPLING	192000

const float PI2 = 2.0 * PI;

#define	MIN_FREQ	100
#define	MAX_FREQ	1000

#define FREEQ_DEPTH     OSC_BUF_FACTOR
#define WAVREADQ_DEPTH  5
#define WAVEV_DEPTH     3

#define	ACTION_NEXT	0x80
#define	ACTION_PREV	0x40
#define	ACTION_NUM_MASK	0x3F

/* Definitions for WAV file format */

#define FOURCC_RIFF     0x46464952
#define FOURCC_FMT      0x20746D66
#define FOURCC_DATA     0x61746164

typedef struct {
  uint32_t FourCC;
  uint32_t size;
} CHUNK;

typedef struct {
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
  uint16_t cbSize;
} WAVEFORMATEX;

typedef struct {
  uint16_t event;
  void     *arg;
  int      option;
} WAVCONTROL_EVENT;

typedef struct {
  FSIZE_t fsize;
  int     sampleRate;
} WAVEINFO;

enum {
  READER_START = 1,
  READER_READ,
  READER_SEEK,
  READER_STOP,
};

enum {
  SOUND_START = 1,
  SOUND_NEXT,
  SOUND_STOP,
};

enum {
  WAV_PLAY,
  WAV_DATA_REQ,
  WAV_PAUSE,
  WAV_RESUME,
  WAV_PREV,
  WAV_NEXT,
  WAV_SELECT = 128,
};

enum {
  WAVP_ST_IDLE = 0,
  WAVP_ST_PLAY,
  WAVP_ST_FLASH,
  WAVP_ST_PAUSE,
};

typedef struct {
  FIL *fp;
  const char *fname;    
  int  state;
  int  nobuff;
  uint8_t  action;
  FSIZE_t fsize;
  uint32_t ppos;
  uint32_t poffset;
  uint16_t pre_val;
  uint16_t post_val;
  lv_timer_t *resume_timer;
} PLAYERINFO;

typedef struct {
  int       state;
  int       index_x;
  int       index_y;
  int       max_index_x;
  int       max_index_y;
  int       rot_x;
  float32_t fx;
  float32_t fy;
} SOUNDERINFO;

const char *MusicList[] = {
   "N-SPHERES/1 Function.wav",
   "N-SPHERES/2 Intersect.wav",
   "N-SPHERES/3 Attractor.wav",
   "N-SPHERES/4 Flux.wav",
   "N-SPHERES/5 Core.wav",
   "OscMusic/01 Dots.wav",
   "OscMusic/02 Lines.wav",
   "OscMusic/03 Blocks.wav",
   "OscMusic/04 Circles.wav",
   "OscMusic/05 Spirals.wav",
   "OscMusic/06 Planets.wav",
   "OscMusic/07 Asteroids.wav",
   "OscMusic/08 Shrooms.wav",
   "OscMusic/09 Deconstruct.wav",
   "OscMusic/10 Reconstruct.wav",
};

typedef struct {
  const char *fname;
  int  rate;
  int  secs;
} OSCMUSICINFO;

static SECTION_SRDSRAM WAVEINFO WaveInfo;
static SECTION_SRDSRAM uint8_t fmt_buffer[32];
static SECTION_SRDSRAM CHUNK ChunkBuffer;

static const AUDIO_STEREO OscSilentBuffer[AUDIO_FRAME_SIZE];

static CHUNK *read_chunk_header(FIL *fp)
{       
  int res;
  CHUNK *cp = &ChunkBuffer;
  UINT nrb;
          
  res = f_read(fp, cp, sizeof(CHUNK), &nrb);
  if (res == FR_OK && nrb == sizeof(CHUNK))
  {         
    return cp;
  }       
  return NULL;
}

static WAVEINFO *wave_open(FIL *fp)
{
  FSIZE_t flen;
  FSIZE_t cpos;
  CHUNK *hc;
  UINT nrb;
  WAVEFORMATEX *hwave;
  WAVEINFO *winfo = &WaveInfo;

  cpos = flen = 0;

  while ((hc = read_chunk_header(fp)))
  {
       cpos += sizeof(CHUNK);

       switch (hc->FourCC)
       {
       case FOURCC_RIFF:
         f_read(fp, &ChunkBuffer, 4, &nrb);
         cpos += 4;
         break;
       case FOURCC_FMT:
         f_read(fp, fmt_buffer, hc->size, &nrb);
         cpos += hc->size;
         if ((hc->size == 18) || (hc->size == 16))
         {
           hwave = (WAVEFORMATEX *)fmt_buffer;
           //debug_printf("%d channels, %d samples\n", hwave->nChannels, hwave->nSamplesPerSec);
           winfo->sampleRate = hwave->nSamplesPerSec;
         }
         else
         {
           debug_printf("hcsize %d\n", hc->size);
         }
         break;
       case FOURCC_DATA:
         winfo->fsize = hc->size;
         return winfo;
         break;
       default:
         f_lseek(fp, cpos + hc->size);
         cpos += hc->size;
         break;
       }
  }
  return winfo;
}

extern const osThreadAttr_t attributes_flacreader;
extern const osThreadAttr_t attributes_mixplayer;

static SECTION_SRDSRAM uint16_t readqBuffer[WAVREADQ_DEPTH];
static SECTION_SRDSRAM AUDIO_STEREO *playqBuffer[FREEQ_DEPTH];
static SECTION_SRDSRAM AUDIO_STEREO *freeqBuffer[FREEQ_DEPTH];
SECTION_SRDSRAM WAVCONTROL_EVENT wavevBuffer[WAVEV_DEPTH];

MESSAGEQ_DEF(wavreadq, readqBuffer, sizeof(readqBuffer))
MESSAGEQ_DEF(freeq, freeqBuffer, sizeof(freeqBuffer))
MESSAGEQ_DEF(playq, playqBuffer, sizeof(playqBuffer))
MESSAGEQ_DEF(wavevq, wavevBuffer, sizeof(wavevBuffer))

osMessageQueueId_t wav_readqId;
osMessageQueueId_t play_bufqId;
osMessageQueueId_t free_bufqId;
osMessageQueueId_t wav_evqId;

PLAYERINFO PlayerInfo;
SOUNDERINFO SounderInfo;

static lv_style_t osc_style;

void postWaveRequest(uint16_t cmd, void *argp)
{       
  WAVCONTROL_EVENT ctrl;
      
  ctrl.event = cmd;
  ctrl.arg = argp;
  osMessageQueuePut(wav_evqId, &ctrl, 0, 0);
}   
  
void wavp_request_data(int offset)
{     
  WAVCONTROL_EVENT ctrl;

  ctrl.event = WAV_DATA_REQ;
  ctrl.option = offset;
  if (osMessageQueuePut(wav_evqId, &ctrl, 0, 0) != osOK)
    debug_printf("data req failure\n");
}

AUDIO_STEREO OscFrameBuffer[AUDIO_FRAME_SIZE*OSC_BUF_FACTOR];
AUDIO_STEREO FinalOscBuffer[AUDIO_FRAME_SIZE * 2];

static void osc_half_complete()
{
  wavp_request_data(0);
}

static void osc_full_complete()
{
  wavp_request_data(AUDIO_FRAME_SIZE);
}

const AUDIO_INIT_PARAMS oscm_audio_params = {
  .buffer = FinalOscBuffer,
  .buffer_size = sizeof(FinalOscBuffer),
  .volume = AUDIO_DEF_VOL,
  .sample_rate = 192000,
  .txhalf_comp = osc_half_complete,
  .txfull_comp = osc_full_complete,
};

const AUDIO_INIT_PARAMS lissa_audio_params = {
  .buffer = FinalOscBuffer,
  .buffer_size = sizeof(FinalOscBuffer),
  .volume = AUDIO_DEF_VOL,
  .sample_rate = LISA_SAMPLING,
  .txhalf_comp = osc_half_complete,
  .txfull_comp = osc_full_complete,
};

static int crate;

void StartWavReaderTask(void *args)
{         
  osStatus_t st;
  uint16_t cmd;
  AUDIO_STEREO *paudio;
  WAVEINFO *winfo;
  AUDIO_OUTPUT_DRIVER *pDriver;
  AUDIO_CONF *audio_config;
  PLAYERINFO *pinfo = &PlayerInfo;
  FIL *pfile = NULL;
  UINT nrb;
  HAL_DEVICE *haldev = (HAL_DEVICE *)args;
  int frames;

  crate = 0;
  memset(pinfo, 0, sizeof(PLAYERINFO));
  haldev->audio_sai->saitx_half_comp = osc_half_complete;
  haldev->audio_sai->saitx_full_comp = osc_full_complete;
  audio_config = get_audio_config(NULL);
  pDriver = (AUDIO_OUTPUT_DRIVER *)audio_config->devconf->pDriver;

  pDriver->Init(audio_config, &oscm_audio_params);

  memset(OscFrameBuffer, 0, sizeof(OscFrameBuffer));

  paudio = OscFrameBuffer;
  for (int i = 0; i < BUF_FACTOR; i++)
  {
    osMessageQueuePut(free_bufqId, &paudio, 0, 0);
    paudio += AUDIO_FRAME_SIZE;
  }

  while (1)
  {
    st = osMessageQueueGet(wav_readqId,  &cmd, 0, osWaitForever);

    if (st == osOK)
    {
      switch (cmd)
      {
      case READER_START:
        pfile = OpenMusicFile((char *)pinfo->fname);
        if (pfile)
        {
          winfo = wave_open(pfile);
        }
        if (pfile && winfo)
        {
          frames = 0;
debug_printf("%s opened.\n", pinfo->fname);
          pinfo->ppos = 0;
          pinfo->poffset = f_tell(pfile);
          postGuiEventMessage(GUIEV_OSCM_FILE, 0, (void *)pinfo->fname, NULL);
          pDriver->Stop(audio_config);

          while (osMessageQueueGet(free_bufqId, &paudio, 0, 0) == osOK)
          {
            f_read(pfile, paudio, sizeof(AUDIO_STEREO) * AUDIO_FRAME_SIZE, &nrb);
            osMessageQueuePut(play_bufqId, &paudio, 0, 0);
          }

          if (crate != winfo->sampleRate)
          {
            crate = winfo->sampleRate;
            pDriver->SetRate(audio_config, winfo->sampleRate);
          }
          pDriver->Start(audio_config);

          pinfo->fsize = winfo->fsize;
          pinfo->nobuff = 0;
          pinfo->state = WAVP_ST_PLAY;
debug_printf("Output started.\n");
        }
        break;
      case READER_READ:
        if (pinfo->state == WAVP_ST_PLAY)
        {
          while ((st = osMessageQueueGet(free_bufqId, &paudio, 0, 0)) == osOK)
          {
            f_read(pfile, paudio, sizeof(AUDIO_STEREO) * AUDIO_FRAME_SIZE, &nrb);
            if (nrb > 0)
            {
              frames++;
              osMessageQueuePut(play_bufqId, &paudio, 0, 10);
            }
            else
            {
              pinfo->state = WAVP_ST_FLASH;
              pinfo->action = ACTION_NEXT;
              osMessageQueuePut(free_bufqId, &paudio, 0, 10);
              CloseMusicFile(pfile);
              debug_printf("read finished. %d\n", pinfo->nobuff);
              break;
            }
          }
        }
        break;
      case READER_SEEK:
        pinfo->ppos &= ~3;
        f_lseek(pfile, pinfo->poffset + pinfo->ppos);

        while (osMessageQueueGet(free_bufqId, &paudio, 0, 0) == osOK)
        {
          f_read(pfile, paudio, sizeof(AUDIO_STEREO) * AUDIO_FRAME_SIZE, &nrb);
          if (nrb != sizeof(AUDIO_STEREO) * AUDIO_FRAME_SIZE)
            break;
          else
            osMessageQueuePut(play_bufqId, &paudio, 0, 0);
        }
        pinfo->state = WAVP_ST_PLAY;
        break;
      case READER_STOP:
        pinfo->state = WAVP_ST_FLASH;
        CloseMusicFile(pfile);
        break;
      default:
        break;
      }
    }
  }
}

static int build_music_list(OSCMUSICINFO **pinfo)
{
  int i;
  FIL *pfile = NULL;
  WAVEINFO *winfo;
  OSCMUSICINFO *oscm;
  int num_music;
  int nums;

  num_music = sizeof(MusicList)/sizeof(char *);
  oscm = (OSCMUSICINFO *)malloc(sizeof(OSCMUSICINFO) * num_music);
  nums = 0;
  
  *pinfo = oscm;

  for (i = 0; i < num_music; i++)
  {
    pfile = OpenMusicFile((char *)MusicList[i]);
    if (pfile)
    {
      winfo = wave_open(pfile);
      oscm->fname = MusicList[i];
      oscm->secs = winfo->fsize / 4 / winfo->sampleRate;
      oscm->rate = winfo->sampleRate;
      CloseMusicFile(pfile);
//debug_printf("%s @ %dK: %d:%02d\n", oscm->fname, oscm->rate/1000, oscm->secs/ 60, oscm->secs % 60);
      oscm++;
      nums++;
    }
  }

  return nums;
}

static lv_obj_t *osc_mlist_create(OSCMUSICINFO *oscmInfo, int num_music, OSCM_SCREEN *screen);
static void mlist_btn_check(lv_obj_t *list, uint32_t mid, bool state);

extern lv_obj_t *create_reboot_mbox(char *title, char *msg);

void StartOscMusic(OSCM_SCREEN *screen)
{
  PLAYERINFO *pinfo = &PlayerInfo;
  int mid;
  osStatus_t st;
  WAVCONTROL_EVENT ctrl;
  AUDIO_STEREO *audiop, *mp;
  OSCMUSICINFO *oscmInfo;
  HAL_DEVICE *haldev = screen->haldev;
  uint16_t cmd;
  int num_music;
  int scount = 0;

  wav_readqId = osMessageQueueNew(WAVREADQ_DEPTH, sizeof(uint16_t), &attributes_wavreadq);
  free_bufqId = osMessageQueueNew(FREEQ_DEPTH, sizeof(uint16_t *), &attributes_freeq);
  play_bufqId = osMessageQueueNew(FREEQ_DEPTH, sizeof(uint16_t *), &attributes_playq);
  wav_evqId = osMessageQueueNew(WAVEV_DEPTH, sizeof(WAVCONTROL_EVENT), &attributes_wavevq);

  num_music = build_music_list(&oscmInfo);
  if (num_music == 0)
  {
    create_reboot_mbox("No music file found", "Please reboot");
    while (1) osDelay(100);
  }

  pinfo->state = WAVP_ST_IDLE;

  osc_mlist_create(oscmInfo, num_music, screen);

  osThreadNew(StartWavReaderTask, haldev, &attributes_flacreader);

  osDelay(100);

  mid = 0;
  pinfo->fname = oscmInfo->fname;
  postWaveRequest(WAV_PLAY, pinfo);

  while (1)
  {
    osMessageQueueGet(wav_evqId, &ctrl, 0, osWaitForever);

    switch (ctrl.event)
    {
    case WAV_PLAY:
      cmd = READER_START;
      scount = 0;
      osMessageQueuePut(wav_readqId, &cmd, 0, 0);
      break;
    case WAV_DATA_REQ:
      switch (pinfo->state)
      {
      case WAVP_ST_IDLE:
        audiop = FinalOscBuffer + ctrl.option;
        memcpy(audiop, OscSilentBuffer, sizeof(OscSilentBuffer));
        break;
      case WAVP_ST_PLAY:
        st = osMessageQueueGet(play_bufqId, &mp, 0, 0);
        if (st == osOK)
        {
          int progress;

          audiop = FinalOscBuffer + ctrl.option;
          memcpy(audiop, mp, sizeof(OscSilentBuffer));
          osMessageQueuePut(free_bufqId, &mp, 0, 0);
          pinfo->ppos += AUDIO_FRAME_SIZE * sizeof(AUDIO_STEREO);
          progress = (int) ((float)pinfo->ppos / (float)pinfo->fsize * 100);
          if (progress > 100)
            progress = 100;
          postGuiEventMessage(GUIEV_DRAW, AUDIO_FRAME_SIZE, (void *)mp, (void *)progress);
        }
        else  
        {   
          scount++;
          if ((scount % 10) == 0)
            debug_printf("sc: %d\n", scount);
          pinfo->nobuff++;
        }     
        if (osMessageQueueGetCount(free_bufqId) > 0)
        {
          cmd = READER_READ;
          osMessageQueuePut(wav_readqId, &cmd, 0, 0);
        }
        break;
     case WAVP_ST_FLASH:
        st = osMessageQueueGet(play_bufqId, &mp, 0, 0);
        if (st == osOK)
        {
debug_printf("in flash.\n");
          audiop = FinalOscBuffer + ctrl.option;
          memcpy(audiop, mp, sizeof(OscSilentBuffer));
          osMessageQueuePut(free_bufqId, &mp, 0, 0);
        }
        else
        {
          pinfo->state = WAVP_ST_PAUSE;
debug_printf("in pause.\n");
          postGuiEventMessage(GUIEV_DRAW, AUDIO_FRAME_SIZE, NULL, 0);
        }
        break;
     case WAVP_ST_PAUSE:
        mlist_btn_check(screen->mlist_screen, mid, false);

        switch (pinfo->action)
        {
        case ACTION_NEXT:
          mid++;
          if (mid == num_music)
            mid = 0;
          break;
        case ACTION_PREV:
          mid--;
          if (mid < 0)
            mid = num_music - 1;
          break;
        default:
          mid = pinfo->action & ACTION_NUM_MASK;
          break;
        }
        mlist_btn_check(screen->mlist_screen, mid, true);
        pinfo->state = WAVP_ST_IDLE;
        pinfo->fname = oscmInfo[mid].fname;
        postWaveRequest(WAV_PLAY, pinfo);
        break;
      default:
        break;
      }
      break;
    case WAV_PAUSE:
      pinfo->state = WAVP_ST_IDLE;
      break;
    case WAV_RESUME:
      if (pinfo->post_val != pinfo->pre_val)
      {
        pinfo->ppos = pinfo->fsize / 10 * pinfo->post_val;
        cmd = READER_SEEK;
        osMessageQueuePut(wav_readqId, &cmd, 0, 0);
      }
      else
      {
        pinfo->state = WAVP_ST_PLAY;
      }
      break;
    case WAV_PREV:
      cmd = READER_STOP;
      pinfo->action = ACTION_PREV;
      osMessageQueuePut(wav_readqId, &cmd, 0, 0);
      lv_obj_clear_state(screen->play_button, LV_STATE_CHECKED);
      break;
    case WAV_NEXT:
      cmd = READER_STOP;
      pinfo->action = ACTION_NEXT;
      osMessageQueuePut(wav_readqId, &cmd, 0, 0);
      lv_obj_clear_state(screen->play_button, LV_STATE_CHECKED);
      break;
    default:
      if (ctrl.event & WAV_SELECT)
      {
        uint8_t newid;

        newid = ctrl.event & ACTION_NUM_MASK;
        if (newid != mid)
        {
          cmd = READER_STOP;
          pinfo->action = newid;
          osMessageQueuePut(wav_readqId, &cmd, 0, 0);
          lv_obj_clear_state(screen->play_button, LV_STATE_CHECKED);
        }
      }
      break;
    }
  }
}

#define	I1_INDEX_SIZE	8

static const uint8_t index_data[I1_INDEX_SIZE] = {
  0x00,0x00,0x00,0xff,0x80,0xE0,0x00,0xFF,
};

uint8_t oscImage_map1[I1_INDEX_SIZE+32*256];
uint8_t oscImage_map2[I1_INDEX_SIZE+32*256];

const lv_image_dsc_t oscImage1 = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.flags = 0,
  .header.w = 256,
  .header.h = 256,
  .header.stride = 32,
  .data_size = sizeof(oscImage_map1),
  .data = oscImage_map1,
}; 

const lv_image_dsc_t oscImage2 = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.flags = 0,
  .header.w = 256,
  .header.h = 256,
  .header.stride = 32,
  .data_size = sizeof(oscImage_map2),
  .data = oscImage_map2,
}; 

static void resume_timer_cb(lv_timer_t *timer)
{
  PLAYERINFO *pinfo = (PLAYERINFO *)timer->user_data;

  postWaveRequest(WAV_RESUME, pinfo);
}

/**
 * Called when play button has pressed.
 */
static void pb_handler(lv_event_t *e)
{
  lv_obj_t * obj = lv_event_get_target(e);
  OSCM_SCREEN *screen = (OSCM_SCREEN *)lv_event_get_user_data(e);
  PLAYERINFO *pinfo = &PlayerInfo;
  int progress;


  if(lv_obj_has_state(obj, LV_STATE_CHECKED)) {
    lv_obj_clear_state(screen->play_button, LV_STATE_CHECKED);
    lv_obj_clear_flag(screen->scope_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(screen->progress_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screen->slider, LV_OBJ_FLAG_HIDDEN);

    pinfo->post_val = lv_slider_get_value(screen->slider);
    if (pinfo->post_val != pinfo->pre_val)
    {
      lv_bar_set_value(screen->progress_bar, pinfo->post_val * 10, LV_ANIM_OFF);
    }
    /* Insert some delay to issue WAV_RESUME request */
    pinfo->resume_timer = lv_timer_create(resume_timer_cb, 600, pinfo);
    lv_timer_set_repeat_count(pinfo->resume_timer, 1);
  }
  else {
    progress = (int) ((float)pinfo->ppos / (float)pinfo->fsize * 10);
    if (progress > 10)
      progress = 10;
    pinfo->pre_val = lv_slider_get_value(screen->slider);

    lv_obj_add_state(screen->play_button, LV_STATE_CHECKED);
    lv_obj_add_flag(screen->scope_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screen->progress_bar, LV_OBJ_FLAG_HIDDEN);
    lv_slider_set_value(screen->slider, progress, LV_ANIM_OFF);
    lv_obj_clear_flag(screen->slider, LV_OBJ_FLAG_HIDDEN);
    postWaveRequest(WAV_PAUSE, pinfo);
  }
}

static const lv_style_prop_t trans_props[] = { LV_STYLE_IMAGE_OPA, 0 };

static void list_proc(OSCM_SCREEN *screen)
{
  if (screen->mlist_screen)
  {
    lv_screen_load_anim(screen->mlist_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
    lv_indev_set_group(screen->keydev, screen->list_ing);
  }
}

void KickOscMusic(HAL_DEVICE *haldev, OSCM_SCREEN *screen)
{
  lv_obj_t *cs;
  LV_IMG_DECLARE(img_lv_demo_music_btn_playlarge);
  LV_IMG_DECLARE(img_lv_demo_music_btn_pauselarge);
  static lv_style_t style_kfocus;

  cs = screen->scope_screen;
  lv_obj_set_size(cs, 480, 320);
  lv_obj_set_style_bg_color(cs, lv_color_black(), LV_PART_MAIN);

  static lv_style_transition_dsc_t tr;
  lv_style_transition_dsc_init(&tr, trans_props, lv_anim_path_linear, 500, 20, NULL);

  static lv_style_t style_def, style_pr, style_focus;
  lv_style_init(&style_def);
  lv_style_init(&style_pr);
  lv_style_set_image_opa(&style_def, LV_OPA_0);
  lv_style_set_image_opa(&style_pr, LV_OPA_100);
  lv_style_set_transition(&style_def, &tr);
  lv_style_init(&style_focus);
  lv_style_set_img_recolor_opa(&style_focus, LV_OPA_10);
  lv_style_set_img_recolor(&style_focus, lv_color_black());

  memcpy(oscImage_map1, index_data, I1_INDEX_SIZE);	/* Copy Index data */
  memcpy(oscImage_map2, index_data, I1_INDEX_SIZE);	/* Copy Index data */
  screen->disp_toggle = 0;
  lv_style_init(&osc_style);
  lv_style_set_text_color(&osc_style, lv_palette_main(LV_PALETTE_LIME));

  screen->scope_image = lv_image_create(cs);
  lv_image_set_src(screen->scope_image, &oscImage1);
  lv_obj_align(screen->scope_image, LV_ALIGN_TOP_MID, 0, 20);
  screen->scope_label = lv_label_create(cs);
  lv_obj_add_style(screen->scope_label, &osc_style, 0);
  lv_label_set_text(screen->scope_label, "");
  lv_obj_align(screen->scope_label, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_set_height(screen->scope_label, 20);

  screen->progress_bar = lv_bar_create(cs);
  lv_obj_set_size(screen->progress_bar, lv_obj_get_width(screen->scope_label), 6);
  lv_obj_align_to(screen->progress_bar, screen->scope_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 1);
  lv_bar_set_value(screen->progress_bar, 80, LV_ANIM_OFF);

  screen->slider = lv_slider_create(cs);
  lv_slider_set_range(screen->slider, 0, 10);
  lv_obj_set_size(screen->slider, lv_obj_get_width(screen->scope_label), 20);
  lv_obj_align(screen->slider, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_flag(screen->slider, LV_OBJ_FLAG_HIDDEN);
  lv_style_init(&style_kfocus);
  lv_style_set_outline_color(&style_kfocus, lv_palette_lighten(LV_PALETTE_YELLOW, 1));
  lv_style_set_outline_width(&style_kfocus, 3);
  lv_style_set_outline_opa(&style_kfocus, LV_OPA_50);
  lv_obj_add_style(screen->slider, &style_kfocus, LV_STATE_FOCUS_KEY);

  screen->play_button = lv_imagebutton_create(screen->scope_image);
  lv_imagebutton_set_src(screen->play_button, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, &img_lv_demo_music_btn_playlarge, NULL);
  lv_imagebutton_set_src(screen->play_button, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &img_lv_demo_music_btn_pauselarge, NULL);
  lv_obj_add_event_cb(screen->play_button, pb_handler, LV_EVENT_PRESSED, screen);
  lv_obj_center(screen->play_button);
  lv_obj_add_style(screen->play_button, &style_focus, LV_STATE_FOCUS_KEY);
  lv_group_add_obj(screen->scope_ing, screen->play_button);
  lv_group_add_obj(screen->scope_ing, screen->slider);

  lv_obj_add_style(screen->play_button, &style_def, 0);
  lv_obj_add_style(screen->play_button, &style_pr, LV_STATE_CHECKED);

  activate_new_screen((BASE_SCREEN *)screen, list_proc, screen);
  screen->haldev = haldev;

  osThreadNew((osThreadFunc_t)StartOscMusic, screen, &attributes_mixplayer);
}

static inline void plotDot(int left, int right)
{
  oscImage_map1[right * 32 + left / 8 + I1_INDEX_SIZE] |= (0x80 >> (left & 7));
  oscImage_map2[right * 32 + left / 8 + I1_INDEX_SIZE] |= (0x80 >> (left & 7));
}

#ifdef DRAW_LINE
static void hline(int ypos, int px, int cx)
{
  if (px < cx)
  {
    do
    {
      plotDot(px, ypos);
      px++;
    } while (px < cx);
  }
  else
  {
    do
    {
      plotDot(cx, ypos);
      cx++;
    } while (px > cx);
  }
}

static void vline(int xpos, int py, int cy)
{
  if (py < cy)
  {
    do
    {
      plotDot(xpos, py);
      py++;
    } while (py < cy);
  }
  else
  {
    do
    {
      plotDot(xpos, cy);
      cy++;
    } while (py > cy);
  }
}
#endif

int oscDraw(OSCM_SCREEN *screen, AUDIO_STEREO *mp, int progress)
{
  int i;
  int left, right;
#ifdef DRAW_LINE
  int d;
  static int px, py;
#endif
  int retval = 0;
  static int dcount;
  int modc;

  dcount++;

  if (mp == NULL)
  {
    memset(oscImage_map2 + I1_INDEX_SIZE, 0, sizeof(oscImage_map2) - I1_INDEX_SIZE);
    memset(oscImage_map1 + I1_INDEX_SIZE, 0, sizeof(oscImage_map1) - I1_INDEX_SIZE);
#ifdef DRAW_LINE
    px = py = 128;
#endif
    lv_image_set_src(screen->scope_image, &oscImage1);
    return 1;
  }

  for (i = 0; i < AUDIO_FRAME_SIZE; i++)
  {
    /* Shift audio value 8bits right. Then, value range becomes [-128..127] */
    left = mp->ch0 >> 8;
    right = mp->ch1 >> 8;
    right = -right;
    right--;

    /* Add 128, range is [0..255] */
    left += 128;
    right += 128;

#ifdef DRAW_LINE
    d = ((left - px) ^ 2) + ((right - py) ^ 2);

    if ((d > 10) && (d < 30))
    {
       if (left == px)
         vline(left, py, right);
       else if (right == py)
         hline(right, px, left);
       else
         plotDot(left, right);
    }
    else
    {
      plotDot(left, right);
    }
    px = left; py = right;
#else
    plotDot(left, right);
#endif
    mp++;
  }

  if (crate == 96000)
    modc = 3;
  else if (crate == 192000)
    modc = 6;
  else
    modc = 20;

  switch (dcount % modc)
  {
  case 0:
      lv_image_set_src(screen->scope_image,
                       (screen->disp_toggle & 1)? &oscImage2 : &oscImage1);
      if (crate != 0)
      {
        lv_bar_set_value(screen->progress_bar, progress, LV_ANIM_OFF);
      }
      retval = 1;
      break;
  case 1:
      if (screen->disp_toggle & 1)
        memset(oscImage_map1 + I1_INDEX_SIZE, 0, sizeof(oscImage_map2) - I1_INDEX_SIZE);
      else
        memset(oscImage_map2 + I1_INDEX_SIZE, 0, sizeof(oscImage_map1) - I1_INDEX_SIZE);
      screen->disp_toggle ^= 1;
      if (crate == 0)
      {
        SOUNDERINFO *sinfo = &SounderInfo;

        SounderInfo.index_x += SounderInfo.rot_x;
        if (sinfo->index_x == sinfo->max_index_x) sinfo->index_x = 0;
        if (sinfo->index_y == sinfo->max_index_y) sinfo->index_y = 0;
      }
      break;
  default:
      break;
  }
  return retval;
}

static lv_style_t style_scrollbar;
static lv_style_t style_btn;
static lv_style_t style_button_pr;
static lv_style_t style_button_chk;
static lv_style_t style_button_def;
static lv_style_t style_title;
static lv_style_t style_time;

LV_IMG_DECLARE(img_lv_demo_music_btn_list_play);
LV_IMG_DECLARE(img_lv_demo_music_btn_list_pause);
LV_IMG_DECLARE(img_lv_demo_music_list_border);

static void mlist_btn_check(lv_obj_t *list, uint32_t mid, bool state)
{
    lv_obj_t * btn = lv_obj_get_child(list, mid);
    lv_obj_t * icon = lv_obj_get_child(btn, 0);

    if(state) {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        lv_image_set_src(icon, &img_lv_demo_music_btn_list_pause);
        lv_obj_scroll_to_view(btn, LV_ANIM_ON);
        lv_gridnav_set_focused(list, btn, LV_ANIM_OFF);
    }
    else {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
        lv_image_set_src(icon, &img_lv_demo_music_btn_list_play);
    }
}

static void btn_click_event_cb(lv_event_t *e)
{
  lv_obj_t * btn = lv_event_get_target(e);
  OSCM_SCREEN *screen = (OSCM_SCREEN *)lv_event_get_user_data(e);

  uint32_t idx = lv_obj_get_index(btn);

  lv_obj_clear_state(screen->play_button, LV_STATE_CHECKED);
  lv_obj_clear_flag(screen->scope_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(screen->progress_bar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(screen->slider, LV_OBJ_FLAG_HIDDEN);

  postWaveRequest(WAV_SELECT + idx, &PlayerInfo);
  activate_new_screen((BASE_SCREEN *)screen, list_proc, screen);
}

static lv_obj_t *add_mlist_btn(lv_obj_t *parent, OSCMUSICINFO *mi, OSCM_SCREEN *screen)
{
    lv_obj_t * btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, lv_pct(100), 50);

    lv_obj_add_style(btn, &style_btn, 0);
    lv_obj_add_style(btn, &style_button_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &style_button_chk, LV_STATE_CHECKED);
    //lv_obj_add_style(btn, &style_button_def, LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &style_button_def, LV_STATE_FOCUSED);
    lv_obj_add_state(btn, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn, btn_click_event_cb, LV_EVENT_CLICKED, screen);
lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * icon = lv_image_create(btn);
    lv_image_set_src(icon, &img_lv_demo_music_btn_list_play);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * title_label = lv_label_create(btn);
    lv_label_set_text(title_label, mi->fname);
    lv_obj_set_grid_cell(title_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_style(title_label, &style_title, 0);

    lv_obj_t * time_label = lv_label_create(btn);
    lv_label_set_text_fmt(time_label, "%d:%02d", mi->secs / 60, mi->secs % 60);
    lv_obj_add_style(time_label, &style_time, 0);
    lv_obj_set_grid_cell(time_label, LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t * border = lv_image_create(btn);
    lv_image_set_src(border, &img_lv_demo_music_list_border);
    lv_image_set_inner_align(border, LV_IMAGE_ALIGN_TILE);
    lv_obj_set_width(border, lv_pct(120));
    lv_obj_align(border, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(border, LV_OBJ_FLAG_IGNORE_LAYOUT);

    return btn;
}

lv_obj_t *osc_mlist_create(OSCMUSICINFO *mlist, int num_music, OSCM_SCREEN *screen)
{
  const lv_font_t *font_medium;

  font_medium = &lv_font_montserrat_16;

  lv_style_init(&style_scrollbar);
  lv_style_set_width(&style_scrollbar,  4);
  lv_style_set_bg_opa(&style_scrollbar, LV_OPA_COVER);
  lv_style_set_bg_color(&style_scrollbar, lv_color_hex3(0xeee));
  lv_style_set_radius(&style_scrollbar, LV_RADIUS_CIRCLE);
  lv_style_set_pad_right(&style_scrollbar, 4);

  static const lv_coord_t grid_cols[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static const lv_coord_t grid_rows[] = {40, LV_GRID_TEMPLATE_LAST};
    lv_style_init(&style_btn);
    //lv_style_set_bg_opa(&style_btn, LV_OPA_TRANSP);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
  lv_style_set_bg_color(&style_btn, lv_color_hex3(0x101));
    lv_style_set_grid_column_dsc_array(&style_btn, grid_cols);
    lv_style_set_grid_row_dsc_array(&style_btn, grid_rows);
    lv_style_set_grid_row_align(&style_btn, LV_GRID_ALIGN_CENTER);
    lv_style_set_layout(&style_btn, LV_LAYOUT_GRID);
    lv_style_set_pad_right(&style_btn, 20);
    lv_style_init(&style_button_pr);
    lv_style_set_bg_opa(&style_button_pr, LV_OPA_COVER);
    lv_style_set_bg_color(&style_button_pr,  lv_color_hex(0x4c4965));

    lv_style_init(&style_button_chk);
    lv_style_set_bg_opa(&style_button_chk, LV_OPA_COVER);
    lv_style_set_bg_color(&style_button_chk, lv_color_hex(0x4c4965));

    lv_style_init(&style_button_def);
    lv_style_set_text_opa(&style_button_def, LV_OPA_80);
    lv_style_set_image_opa(&style_button_def, LV_OPA_80);
 
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, font_medium);
    lv_style_set_text_color(&style_title, lv_color_hex(0xffffff));
 
    lv_style_init(&style_time);
    lv_style_set_text_font(&style_time, font_medium);
    lv_style_set_text_color(&style_time, lv_color_hex(0xffffff));

  /*Create an empty transparent container*/
  lv_obj_t *list;

  list = lv_obj_create(NULL);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, LV_HOR_RES, LV_VER_RES - 20);
  lv_obj_set_y(list, 20);
  lv_obj_add_style(list, &style_scrollbar, LV_PART_SCROLLBAR);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLL_ELASTIC);

  lv_gridnav_add(list, LV_GRIDNAV_CTRL_ROLLOVER);

  for (int i = 0; i < num_music; i++)
  {
    add_mlist_btn(list, mlist++, screen);
  }

  lv_group_add_obj(screen->list_ing, list);
  screen->mlist_screen = list;

  mlist_btn_check(list, 0, true);

  return list;
}

void oscm_process_stick(OSCM_SCREEN *screen, int evcode, int direction, int cflag)
{
  if (evcode == GUIEV_RIGHT_YDIR)
  {
    if (direction > 0 && cflag)
    {
      lv_screen_load_anim(screen->scope_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, false);
      lv_indev_set_group(screen->keydev, screen->scope_ing);
    }
    else if ((direction < 0) && (cflag == 0))
    {
      lv_screen_load_anim(screen->mlist_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
      lv_indev_set_group(screen->keydev, screen->list_ing);
    }
  }
}

void fill_wave(AUDIO_STEREO *paudio, SOUNDERINFO *sinfo)
{
  int i;

  for (i = 0; i < AUDIO_FRAME_SIZE; i++)
  {
    int32_t val;

    val = arm_sin_f32(PI2*sinfo->fx*sinfo->index_x/LISA_SAMPLING) * 32768.0;
    if (val > 32767) val = 32767;
    paudio->ch0 = val;
    val = arm_sin_f32(PI2*sinfo->fy*sinfo->index_y/LISA_SAMPLING) * 32768.0;
    if (val > 32767) val = 32767;
    paudio->ch1 = val;
    sinfo->index_x++;
    sinfo->index_y++;
    if (sinfo->index_x == sinfo->max_index_x) sinfo->index_x = 0;
    if (sinfo->index_y == sinfo->max_index_y) sinfo->index_y = 0;
    paudio++;
  }
}

void StartSoundGeneratorTask(void *args)
{
  osStatus_t st;
  uint16_t cmd;
  AUDIO_OUTPUT_DRIVER *pDriver;
  AUDIO_CONF *audio_config;
  HAL_DEVICE *haldev = (HAL_DEVICE *)args;
  SOUNDERINFO *sinfo = &SounderInfo;
  AUDIO_STEREO *paudio;

  crate = 0;
  haldev->audio_sai->saitx_half_comp = osc_half_complete;
  haldev->audio_sai->saitx_full_comp = osc_full_complete;
  audio_config = get_audio_config(NULL);
  pDriver = (AUDIO_OUTPUT_DRIVER *)audio_config->devconf->pDriver;

  pDriver->Init(audio_config, &lissa_audio_params);

  memset(OscFrameBuffer, 0, sizeof(OscFrameBuffer));

  paudio = OscFrameBuffer;
  for (int i = 0; i < BUF_FACTOR; i++)
  {
    osMessageQueuePut(free_bufqId, &paudio, 0, 0);
    paudio += AUDIO_FRAME_SIZE;
  }

  while (1)
  {
    st = osMessageQueueGet(wav_readqId,  &cmd, 0, osWaitForever);

    if (st == osOK)
    {
      switch (cmd)
      {
      case SOUND_START:
        sinfo->state = WAVP_ST_PLAY;

        sinfo->max_index_x = LISA_SAMPLING * sinfo->fx;
        sinfo->max_index_y = LISA_SAMPLING * sinfo->fy;
        sinfo->index_x = 0;
        sinfo->index_y = 0;

        while ((st = osMessageQueueGet(free_bufqId, &paudio, 0, 0)) == osOK)
        {
          fill_wave(paudio, sinfo);
          osMessageQueuePut(play_bufqId, &paudio, 0, 0);
        }
        pDriver->Start(audio_config);
        break;
      case SOUND_NEXT:
        while ((st = osMessageQueueGet(free_bufqId, &paudio, 0, 0)) == osOK)
        {
          fill_wave(paudio, sinfo);
          osMessageQueuePut(play_bufqId, &paudio, 0, 0);
        }
        break;
      case SOUND_STOP:
        break;
      default:
        break;
      }
    }
  }
}

void StartLissajous(OSCM_SCREEN *screen)
{
  HAL_DEVICE *haldev = screen->haldev;
  SOUNDERINFO *sinfo = &SounderInfo;
  AUDIO_STEREO *audiop, *mp;
  uint16_t cmd;
  int scount;
  osStatus_t st;
  WAVCONTROL_EVENT ctrl;

  wav_readqId = osMessageQueueNew(WAVREADQ_DEPTH, sizeof(uint16_t), &attributes_wavreadq);
  free_bufqId = osMessageQueueNew(FREEQ_DEPTH, sizeof(uint16_t *), &attributes_freeq);
  play_bufqId = osMessageQueueNew(FREEQ_DEPTH, sizeof(uint16_t *), &attributes_playq);
  wav_evqId = osMessageQueueNew(WAVEV_DEPTH, sizeof(WAVCONTROL_EVENT), &attributes_wavevq);
  memset(sinfo, 0, sizeof(SOUNDERINFO));

  osThreadNew(StartSoundGeneratorTask, haldev, &attributes_flacreader);

  sinfo->state = WAVP_ST_IDLE;
  sinfo->fx = 100;
  sinfo->fy = 100;

  postWaveRequest(WAV_PLAY, sinfo);
  scount = 0;

  while (1)
  {
    osMessageQueueGet(wav_evqId, &ctrl, 0, osWaitForever);

    switch (ctrl.event)
    {
    case WAV_PLAY:
      cmd = SOUND_START;
      scount = 0;
      osMessageQueuePut(wav_readqId, &cmd, 0, 0);
      break;
    case WAV_DATA_REQ:
      switch (sinfo->state)
      {
      case WAVP_ST_IDLE:
        audiop = FinalOscBuffer + ctrl.option;
        memcpy(audiop, OscSilentBuffer, sizeof(OscSilentBuffer));
        break;
      case WAVP_ST_PLAY:
        st = osMessageQueueGet(play_bufqId, &mp, 0, 0);
        if (st == osOK)
        {
          audiop = FinalOscBuffer + ctrl.option;
          memcpy(audiop, mp, sizeof(OscSilentBuffer));
          osMessageQueuePut(free_bufqId, &mp, 0, 0);
          postGuiEventMessage(GUIEV_DRAW, AUDIO_FRAME_SIZE, (void *)mp, (void *)0);
        }
        else
        {
          scount++;
          if ((scount % 10) == 0)
            debug_printf("sc: %d\n", scount);
        }
        if (osMessageQueueGetCount(free_bufqId) > 0)
        {
          cmd = SOUND_NEXT;
          osMessageQueuePut(wav_readqId, &cmd, 0, 0);
        }
        break;
     case WAVP_ST_PAUSE:
        break;
      default:
        break;
      }
      break;
    }
  }
}

#ifdef USE_LISSAJOUS
static const char *roller_option = "100\n200\n300\n400\n500\n600\n700\n800\n900";

static void roller_event_handler(lv_event_t *ev)
{
  lv_obj_t *obj;
  char tbuff[5];
  OSCM_SCREEN *screen;
  int val;
  SOUNDERINFO *sinfo = &SounderInfo;

  obj = lv_event_get_target_obj(ev);
  screen = (OSCM_SCREEN *)lv_event_get_user_data(ev);
  lv_roller_get_selected_str(obj, tbuff, sizeof(tbuff));

  val = atoi(tbuff);
  if (obj == screen->roller_left)
  {
    if (val != sinfo->fx)
    {
      sinfo->fx = val;
      postWaveRequest(WAV_PLAY, sinfo);
    }
  }
  else
  {
    if (val != sinfo->fy)
    {
      sinfo->fy = val;
      postWaveRequest(WAV_PLAY, sinfo);
    }
  }
}

static void rot_handler(lv_event_t *ev)
{
  lv_obj_t * obj = lv_event_get_target(ev);

  if (lv_obj_has_state(obj, LV_STATE_CHECKED))
  {
    SounderInfo.rot_x = 0;
  }
  else
  {
    SounderInfo.rot_x = 2;
  }
}

void KickLissajous(HAL_DEVICE *haldev, OSCM_SCREEN *screen)
{
  lv_obj_t *cs;
  lv_obj_t *label;
  static lv_style_t style_kfocus;

  cs = screen->scope_screen;
  lv_obj_set_size(cs, 480, 320);
  lv_obj_set_style_bg_color(cs, lv_color_black(), LV_PART_MAIN);

  static lv_style_t style_def, style_pr, style_focus;
  lv_style_init(&style_def);
  lv_style_init(&style_pr);
  lv_style_set_image_opa(&style_def, LV_OPA_0);
  lv_style_set_image_opa(&style_pr, LV_OPA_100);
  lv_style_init(&style_focus);
  lv_style_set_img_recolor_opa(&style_focus, LV_OPA_10);
  lv_style_set_img_recolor(&style_focus, lv_color_black());

  screen->roller_left = lv_roller_create(cs);
  lv_roller_set_options(screen->roller_left, roller_option, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(screen->roller_left, 3);
  lv_obj_align(screen->roller_left, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_add_event_cb(screen->roller_left, roller_event_handler, LV_EVENT_VALUE_CHANGED, screen);

  screen->roller_right = lv_roller_create(cs);
  lv_roller_set_options(screen->roller_right, roller_option, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(screen->roller_right, 3);
  lv_obj_align(screen->roller_right, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_obj_add_event_cb(screen->roller_right, roller_event_handler, LV_EVENT_VALUE_CHANGED, screen);


  memcpy(oscImage_map1, index_data, I1_INDEX_SIZE);	/* Copy Index data */
  memcpy(oscImage_map2, index_data, I1_INDEX_SIZE);	/* Copy Index data */
  screen->disp_toggle = 0;

  lv_style_init(&osc_style);
  lv_style_set_text_color(&osc_style, lv_palette_main(LV_PALETTE_LIME));

  screen->scope_image = lv_image_create(cs);
  lv_image_set_src(screen->scope_image, &oscImage1);
  lv_obj_align(screen->scope_image, LV_ALIGN_TOP_MID, 0, 20);

  screen->play_button = lv_button_create(cs);
  lv_obj_add_event_cb(screen->play_button, rot_handler, LV_EVENT_PRESSED, NULL);
  lv_obj_align(screen->play_button, LV_ALIGN_BOTTOM_LEFT, 5, -20);
  lv_obj_add_flag(screen->play_button, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_height(screen->play_button, LV_SIZE_CONTENT);
  label = lv_label_create(screen->play_button);
  lv_label_set_text(label, "Rot");
  lv_obj_center(label);

  lv_group_add_obj(screen->scope_ing, screen->roller_left);
  lv_group_add_obj(screen->scope_ing, screen->roller_right);
  lv_group_add_obj(screen->scope_ing, screen->play_button);

  lv_style_init(&style_kfocus);
#if 1
  lv_style_set_outline_color(&style_kfocus, lv_palette_lighten(LV_PALETTE_YELLOW, 1));
#else
  lv_style_set_outline_color(&style_kfocus, lv_palette_main(LV_PALETTE_YELLOW));
#endif
  lv_style_set_outline_width(&style_kfocus, 4);
  lv_style_set_outline_opa(&style_kfocus, LV_OPA_50);
  lv_obj_add_style(screen->roller_left, &style_kfocus, LV_STATE_FOCUS_KEY);
  lv_obj_add_style(screen->roller_right, &style_kfocus, LV_STATE_FOCUS_KEY);
  lv_obj_add_style(screen->play_button, &style_kfocus, LV_STATE_FOCUS_KEY);

  activate_new_screen((BASE_SCREEN *)screen, NULL, NULL);
  screen->haldev = haldev;

  osThreadNew((osThreadFunc_t)StartLissajous, screen, &attributes_mixplayer);
}
#endif
