/**
 *  Oscilloscope Music Player
 */
#include "DoomPlayer.h"
#include "board_if.h"
#include "fatfs.h"
#include "audio_output.h"
#include "app_setup.h"

#define DOUBLE_IMAGE		/* Use double image buffering */

#define AUDIO_FRAME_SIZE        (192*6) /* 16bit, 192K sampling, 6ms */
#define OSC_BUF_FACTOR      4

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
  READER_STOP,
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
} PLAYERINFO;

const char *MusicList[] = {
   "N-SPHERES/Function.wav",
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

static int crate;

void StartWavReaderTask(void *args)
{         
  osStatus_t st;
  uint16_t cmd;
  AUDIO_STEREO *paudio;
  WAVEINFO *winfo;
  PLAYERINFO *pinfo = &PlayerInfo;
  FIL *pfile = NULL;
  UINT nrb;
  HAL_DEVICE *haldev = (HAL_DEVICE *)args;
  int frames;

  crate = 0;
  haldev->audio_sai->saitx_half_comp = osc_half_complete;
  haldev->audio_sai->saitx_full_comp = osc_full_complete;
  Board_SAI_ClockConfig(haldev, crate);

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
          postGuiEventMessage(GUIEV_OSCM_FILE, 0, (void *)pinfo->fname, NULL);
          Board_SAI_DeInit(haldev);
          while (osMessageQueueGet(free_bufqId, &paudio, 0, 0) == osOK)
          {
            f_read(pfile, paudio, sizeof(AUDIO_STEREO) * AUDIO_FRAME_SIZE, &nrb);
            osMessageQueuePut(play_bufqId, &paudio, 0, 0);
          }

          Board_SAI_Init(haldev,  winfo->sampleRate);
          if (crate != winfo->sampleRate)
          {
            crate = winfo->sampleRate;
            bsp_codec_init(haldev->codec_i2c, crate);
          }
          Board_SAI_Start(haldev, (uint8_t *)FinalOscBuffer, AUDIO_FRAME_SIZE * 4);

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

  num_music = sizeof(MusicList)/sizeof(char *);
  oscm = (OSCMUSICINFO *)malloc(sizeof(OSCMUSICINFO) * num_music);
  
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
    }
  }

  return num_music;
}

static lv_obj_t *osc_mlist_create(OSCMUSICINFO *oscmInfo, int num_music, OSCM_SCREEN *screen);
static void mlist_btn_check(lv_obj_t *list, uint32_t mid, bool state);

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
          audiop = FinalOscBuffer + ctrl.option;
          memcpy(audiop, mp, sizeof(OscSilentBuffer));
          osMessageQueuePut(free_bufqId, &mp, 0, 0);
          postGuiEventMessage(GUIEV_DRAW, AUDIO_FRAME_SIZE, (void *)mp, NULL);
        }
        else  
        {   
          scount++;
          if ((scount % 100) == 0)
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
debug_printf("mid = %d, action = %d\n", mid, pinfo->action);
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
      pinfo->state = WAVP_ST_PLAY;
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

#ifdef DOUBLE_IMAGE
uint8_t oscImage_map1[40*320+8];
uint8_t oscImage_map2[40*320+8];

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
#else
uint8_t oscImage_map[40*320+8];

const lv_image_dsc_t oscImage = {
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.flags = 0,
  .header.w = 256,
  .header.h = 256,
  .header.stride = 32,
  .data_size = sizeof(oscImage_map),
  .data = oscImage_map,
}; 
#endif

/**
 * Called when play button status has changed
 */
static void style_handler(lv_event_t *e)
{
  lv_obj_t * obj = lv_event_get_target(e);
  OSCM_SCREEN *screen = (OSCM_SCREEN *)lv_event_get_user_data(e);

  if(lv_obj_has_state(obj, LV_STATE_CHECKED))
  {
    lv_obj_remove_flag(screen->prev_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(screen->next_button, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_add_flag(screen->prev_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screen->next_button, LV_OBJ_FLAG_HIDDEN);
  }
}

static void list_handler(lv_event_t *e)
{
  OSCM_SCREEN *screen = (OSCM_SCREEN *)lv_event_get_user_data(e);

  if (screen->mlist_screen)
  {
    lv_screen_load(screen->mlist_screen);
  }
}

/**
 * Called when play button has pressed.
 */
static void pb_handler(lv_event_t *e)
{
  lv_obj_t * obj = lv_event_get_target(e);
  OSCM_SCREEN *screen = (OSCM_SCREEN *)lv_event_get_user_data(e);

  if(lv_obj_has_state(obj, LV_STATE_CHECKED)) {
    lv_obj_clear_state(screen->play_button, LV_STATE_CHECKED);
    postWaveRequest(WAV_RESUME, &PlayerInfo);
  }
  else {
    lv_obj_add_state(screen->play_button, LV_STATE_CHECKED);
    postWaveRequest(WAV_PAUSE, &PlayerInfo);
  }
}

static void prev_handler(lv_event_t *e)
{
  UNUSED(e);
  postWaveRequest(WAV_PREV, &PlayerInfo);
}

static void next_handler(lv_event_t *e)
{
  UNUSED(e);
  postWaveRequest(WAV_NEXT, &PlayerInfo);
}

static const lv_style_prop_t trans_props[] = { LV_STYLE_IMAGE_OPA, 0 };

void KickOscMusic(HAL_DEVICE *haldev, OSCM_SCREEN *screen)
{
  lv_obj_t *cs;
  LV_IMG_DECLARE(img_lv_demo_music_btn_playlarge);
  LV_IMG_DECLARE(img_lv_demo_music_btn_pauselarge);
  LV_IMG_DECLARE(img_lv_demo_music_btn_prevlarge);
  LV_IMG_DECLARE(img_lv_demo_music_btn_nextlarge);

  screen->scope_screen = cs = lv_obj_create(NULL);
  lv_screen_load(cs);
  activate_screen(cs);
  lv_obj_set_size(cs, 480, 320);
  lv_obj_set_style_bg_color(cs, lv_color_black(), LV_PART_MAIN);

  static lv_style_transition_dsc_t tr;
  lv_style_transition_dsc_init(&tr, trans_props, lv_anim_path_linear, 500, 20, NULL);

  static lv_style_t style_def, style_pr;
  lv_style_init(&style_def);
  lv_style_init(&style_pr);
  lv_style_set_image_opa(&style_def, LV_OPA_0);
  lv_style_set_image_opa(&style_pr, LV_OPA_100);
  lv_style_set_transition(&style_def, &tr);

#ifdef DOUBLE_IMAGE
  memcpy(oscImage_map1, index_data, I1_INDEX_SIZE);	/* Copy Index data */
  memcpy(oscImage_map2, index_data, I1_INDEX_SIZE);	/* Copy Index data */
  screen->disp_toggle = 0;
#else
  memcpy(oscImage_map, index_data, I1_INDEX_SIZE);	/* Copy Index data */
#endif
  lv_style_init(&osc_style);
  lv_style_set_text_color(&osc_style, lv_palette_main(LV_PALETTE_LIME));

  screen->scope_image = lv_image_create(cs);
#ifdef DOUBLE_IMAGE
  lv_image_set_src(screen->scope_image, &oscImage1);
#else
  lv_image_set_src(screen->scope_image, &oscImage);
#endif
  lv_obj_center(screen->scope_image);
  screen->scope_label = lv_label_create(cs);
  lv_obj_add_style(screen->scope_label, &osc_style, 0);
  lv_label_set_text(screen->scope_label, "");
  lv_obj_add_flag(screen->scope_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(screen->scope_label, LV_ALIGN_BOTTOM_MID, 0, -15);
  lv_obj_add_event_cb(screen->scope_label, list_handler, LV_EVENT_PRESSED, screen);

  screen->play_button = lv_imagebutton_create(screen->scope_image);
  lv_imagebutton_set_src(screen->play_button, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, &img_lv_demo_music_btn_playlarge, NULL);
  lv_imagebutton_set_src(screen->play_button, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &img_lv_demo_music_btn_pauselarge, NULL);
  lv_obj_add_event_cb(screen->play_button, pb_handler, LV_EVENT_PRESSED, screen);
  lv_obj_center(screen->play_button);

  screen->prev_button = lv_image_create(cs);
  lv_image_set_src(screen->prev_button, &img_lv_demo_music_btn_prevlarge);
  lv_obj_align(screen->prev_button, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_flag(screen->prev_button, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(screen->prev_button, LV_OBJ_FLAG_CLICKABLE);

  screen->next_button = lv_image_create(cs);
  lv_image_set_src(screen->next_button, &img_lv_demo_music_btn_nextlarge);
  lv_obj_align(screen->next_button, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_flag(screen->next_button, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(screen->next_button, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_event_cb(screen->prev_button, prev_handler, LV_EVENT_CLICKED, screen);
  lv_obj_add_event_cb(screen->next_button, next_handler, LV_EVENT_CLICKED, screen);

  lv_obj_add_style(screen->play_button, &style_def, 0);
  lv_obj_add_style(screen->play_button, &style_pr, LV_STATE_CHECKED);

  lv_obj_add_event_cb(screen->play_button, style_handler, LV_EVENT_STYLE_CHANGED, screen);

  screen->haldev = haldev;

  osThreadNew((osThreadFunc_t)StartOscMusic, screen, &attributes_mixplayer);
}

void oscDraw(OSCM_SCREEN *screen, AUDIO_STEREO *mp)
{
  int i;
  int left, right;
  static int dcount;

  dcount++;

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

#ifdef DOUBLE_IMAGE
    oscImage_map1[right * 32 + left / 8 + I1_INDEX_SIZE] |= (0x80 >> (left & 7));
    oscImage_map2[right * 32 + left / 8 + I1_INDEX_SIZE] |= (0x80 >> (left & 7));
#else
    oscImage_map[right * 32 + left / 8 + I1_INDEX_SIZE] |= (0x80 >> (left & 7));
#endif
    mp++;
  }
  if (crate == 96000)
  {
    /* Refresh screen every four frames */
    switch (dcount % 4)
    {
    case 0:
#ifdef DOUBLE_IMAGE
      lv_image_set_src(screen->scope_image,
                       (screen->disp_toggle & 1)? &oscImage2 : & oscImage1);
#else
      lv_obj_invalidate(screen->scope_image);
#endif
      lv_timer_handler();
      break;
    case 1:
#ifdef DOUBLE_IMAGE
      if (screen->disp_toggle & 1)
        memset(oscImage_map2 + I1_INDEX_SIZE, 0, sizeof(oscImage_map2) - I1_INDEX_SIZE);
      else
        memset(oscImage_map1 + I1_INDEX_SIZE, 0, sizeof(oscImage_map1) - I1_INDEX_SIZE);
      screen->disp_toggle ^= 1;
#else
      memset(oscImage_map + I1_INDEX_SIZE, 0, sizeof(oscImage_map) - I1_INDEX_SIZE);
#endif
      break;
    default:
      break;
    }
  }
  else
  {
    /* Refresh screen every six frames */
    switch (dcount % 6)
    {
    case 0:
#ifdef DOUBLE_IMAGE
      lv_image_set_src(screen->scope_image,
                       (screen->disp_toggle & 1)? &oscImage2 : & oscImage1);
#else
      lv_obj_invalidate(screen->scope_image);
#endif
      lv_timer_handler();
      break;
    case 2:
#ifdef DOUBLE_IMAGE
      if (screen->disp_toggle & 1)
        memset(oscImage_map2 + I1_INDEX_SIZE, 0, sizeof(oscImage_map2) - I1_INDEX_SIZE);
      else
        memset(oscImage_map1 + I1_INDEX_SIZE, 0, sizeof(oscImage_map1) - I1_INDEX_SIZE);
      screen->disp_toggle ^= 1;
#else
      memset(oscImage_map + I1_INDEX_SIZE, 0, sizeof(oscImage_map) - I1_INDEX_SIZE);
#endif
      break;
    default:
      break;
    }
  }
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

  postWaveRequest(WAV_SELECT + idx, &PlayerInfo);
  lv_screen_load(screen->scope_screen);
}

static lv_obj_t *add_mlist_btn(lv_obj_t *parent, OSCMUSICINFO *mi, OSCM_SCREEN *screen)
{
    lv_obj_t * btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, lv_pct(100), 60);

    lv_obj_add_style(btn, &style_btn, 0);
    lv_obj_add_style(btn, &style_button_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &style_button_chk, LV_STATE_CHECKED);
    //lv_obj_add_style(btn, &style_button_def, LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &style_button_def, LV_STATE_FOCUSED);
    lv_obj_add_state(btn, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn, btn_click_event_cb, LV_EVENT_CLICKED, screen);

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
  lv_obj_set_size(list, LV_HOR_RES, LV_VER_RES-10);
  lv_obj_set_y(list, 10);
  lv_obj_add_style(list, &style_scrollbar, LV_PART_SCROLLBAR);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLL_ELASTIC);

  lv_gridnav_add(list, LV_GRIDNAV_CTRL_ROLLOVER);

  for (int i = 0; i < num_music; i++)
  {
    add_mlist_btn(list, mlist++, screen);
  }

  screen->mlist_screen = list;

  mlist_btn_check(list, 0, true);

  return list;
}
