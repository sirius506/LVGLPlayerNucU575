/**
 *  Oscilloscope Music Player
 */
#include "DoomPlayer.h"
#include "fatfs.h"
#include "audio_output.h"

#define AUDIO_FRAME_SIZE        (192*6) /* 16bit, 192K sampling, 6ms */
#define OSC_BUF_FACTOR      4

#define FREEQ_DEPTH     OSC_BUF_FACTOR
#define WAVREADQ_DEPTH  5
#define WAVEV_DEPTH     3

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
  FSIZE_t fsize;
} PLAYERINFO;

const char *MusicList[] = {
#if 0
   "OscMusic/01 Dots.wav",
   "OscMusic/02 Lines.wav",
   "OscMusic/03 Blocks.wav",
   "OscMusic/04 Circles.wav",
#endif
   "OscMusic/05 Spirals.wav",
   "OscMusic/06 Planets.wav",
   "OscMusic/07 Asteroids.wav",
   "OscMusic/08 Shrooms.wav",
   "OscMusic/09 Deconstruct.wav",
   "OscMusic/10 Reconstruct.wav",
};

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
           debug_printf("%d channels, %d samples\n", hwave->nChannels, hwave->nSamplesPerSec);
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

static void osc_half_complete(SAI_HandleTypeDef *hsai)
{
  UNUSED(hsai);
  wavp_request_data(0);
}

static void osc_full_complete(SAI_HandleTypeDef *hsai)
{
  UNUSED(hsai);
  wavp_request_data(AUDIO_FRAME_SIZE);
}

static void osc_error(SAI_HandleTypeDef *hsai)
{
  debug_printf("SAI Error: %x\n", hsai->ErrorCode);
}

/**
  * @brief  SAI1 clock Config.
  * @param  hsai SAI handle.
  * @param  SampleRate Audio sample rate used to play the audio stream.
  * @note   The SAI PLL configuration done within this function assumes that
  *         the SAI PLL input is MSI clock and that MSI is already enabled at 4 MHz.
  * @retval HAL status.
  */
__weak HAL_StatusTypeDef MX_SAI1_ClockConfig(const SAI_HandleTypeDef *hsai, uint32_t SampleRate)
{   
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hsai);
  UNUSED(SampleRate);
    
  HAL_StatusTypeDef ret = HAL_OK;
  RCC_PeriphCLKInitTypeDef rcc_ex_clk_init_struct;
    
  rcc_ex_clk_init_struct.PLL3.PLL3Source = RCC_PLLSOURCE_HSI;
  rcc_ex_clk_init_struct.PLL3.PLL3RGE = 0;
  rcc_ex_clk_init_struct.PLL3.PLL3FRACN = 0;
  rcc_ex_clk_init_struct.PLL3.PLL3ClockOut = RCC_PLL3_DIVP;
  rcc_ex_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
  rcc_ex_clk_init_struct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL3;
  rcc_ex_clk_init_struct.PLL3.PLL3Q = 2;
  rcc_ex_clk_init_struct.PLL3.PLL3R = 2;

    /* SAI clock config:
    PLL3_VCO Input = HSI_16Mhz/PLL3M = 16 Mhz
    PLL3_VCO Output = PLL3_VCO Input * PLL3N = 384 Mhz
    SAI_CLK_x = PLL3_VCO Output/PLL3P = 384/8 = 48 Mhz */
    rcc_ex_clk_init_struct.PLL3.PLL3M = 1;
    rcc_ex_clk_init_struct.PLL3.PLL3N = 24;
    rcc_ex_clk_init_struct.PLL3.PLL3P = 8;
    rcc_ex_clk_init_struct.PLL3.PLL3FRACN = 4700;

  if (HAL_RCCEx_PeriphCLKConfig(&rcc_ex_clk_init_struct) != HAL_OK)
  {
    ret = HAL_ERROR;
  }
  __HAL_RCC_SAI1_CLK_ENABLE();

  return ret;
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
  SAI_HandleTypeDef *hsai;
  int frames;

  hsai = haldev->audio_sai->hsai;

#if 1
  MX_SAI1_ClockConfig(hsai, 192000);

  crate = 192000;
#else
  crate = 0;
#endif
  bsp_codec_init(haldev->codec_i2c, crate);
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
        pfile = OpenFATFile((char *)pinfo->fname);
        if (pfile)
        {
          winfo = wave_open(pfile);
        }
        if (pfile && winfo)
        {
          frames = 0;
debug_printf("%s opened.\n", pinfo->fname);
          postGuiEventMessage(GUIEV_OSCM_FILE, 0, (void *)pinfo->fname, NULL);
          HAL_SAI_DeInit(hsai);
          while (osMessageQueueGet(free_bufqId, &paudio, 0, 0) == osOK)
          {
            f_read(pfile, paudio, sizeof(AUDIO_STEREO) * AUDIO_FRAME_SIZE, &nrb);
            osMessageQueuePut(play_bufqId, &paudio, 0, 0);
          }
          hsai->Init.AudioFrequency = winfo->sampleRate;
          debug_printf("rate change: %d\n", winfo->sampleRate);
          if (crate != winfo->sampleRate)
          {
            crate = winfo->sampleRate;
            bsp_codec_init(haldev->codec_i2c, crate);
          }
          HAL_SAI_Init(hsai);
          HAL_SAI_RegisterCallback(hsai, HAL_SAI_TX_HALFCOMPLETE_CB_ID, osc_half_complete);
          HAL_SAI_RegisterCallback(hsai, HAL_SAI_TX_COMPLETE_CB_ID, osc_full_complete);
          HAL_SAI_RegisterCallback(hsai, HAL_SAI_ERROR_CB_ID, osc_error);

          HAL_SAI_Transmit_DMA(hsai, (uint8_t *)FinalOscBuffer, AUDIO_FRAME_SIZE * 4);

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
              osMessageQueuePut(free_bufqId, &paudio, 0, 10);
              CloseFATFile(pfile);
              debug_printf("read finished. %d\n", pinfo->nobuff);
              break;
            }
          }
        }
        break;
      case READER_STOP:
        break;
      default:
        break;
      }
    }
  }
}

void StartOscMusic(HAL_DEVICE *haldev)
{
  PLAYERINFO *pinfo = &PlayerInfo;
  int mid;
  osStatus_t st;
  WAVCONTROL_EVENT ctrl;
  AUDIO_STEREO *audiop, *mp;
  uint16_t cmd;
  int scount = 0;

  wav_readqId = osMessageQueueNew(WAVREADQ_DEPTH, sizeof(uint16_t), &attributes_wavreadq);
  free_bufqId = osMessageQueueNew(FREEQ_DEPTH, sizeof(uint16_t *), &attributes_freeq);
  play_bufqId = osMessageQueueNew(FREEQ_DEPTH, sizeof(uint16_t *), &attributes_playq);
  wav_evqId = osMessageQueueNew(WAVEV_DEPTH, sizeof(WAVCONTROL_EVENT), &attributes_wavevq);

  pinfo->state = WAVP_ST_IDLE;

  osThreadNew(StartWavReaderTask, haldev, &attributes_flacreader);

  osDelay(100);

  mid = 0;
  pinfo->fname = MusicList[mid++];
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
        pinfo->state = WAVP_ST_IDLE;
        pinfo->fname = MusicList[mid++];
        postWaveRequest(WAV_PLAY, pinfo);
        if (mid == sizeof(MusicList) / sizeof(char *)) mid = 0;
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
    default:
      break;
    }
  }
}

#define	I1_INDEX_SIZE	8

static const uint8_t index_data[I1_INDEX_SIZE] = {
  0x00,0x00,0x00,0xff,0x80,0xE0,0x00,0xFF,
};

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

static const lv_style_prop_t trans_props[] = { LV_STYLE_IMAGE_OPA, 0 };

void KickOscMusic(HAL_DEVICE *haldev, OSCM_SCREEN *screen)
{
  lv_obj_t *cs;
  LV_IMG_DECLARE(img_lv_demo_music_btn_playlarge);
  LV_IMG_DECLARE(img_lv_demo_music_btn_pauselarge);

  cs = lv_screen_active();
  lv_obj_set_style_bg_color(cs, lv_color_black(), LV_PART_MAIN);

  static lv_style_transition_dsc_t tr;
  lv_style_transition_dsc_init(&tr, trans_props, lv_anim_path_linear, 500, 20, NULL);

  static lv_style_t style_def, style_pr;
  lv_style_init(&style_def);
  lv_style_init(&style_pr);
  lv_style_set_image_opa(&style_def, LV_OPA_0);
  lv_style_set_image_opa(&style_pr, LV_OPA_100);
  lv_style_set_transition(&style_def, &tr);

  memcpy(oscImage_map, index_data, I1_INDEX_SIZE);	/* Copy Index data */
  lv_style_init(&osc_style);
  lv_style_set_text_color(&osc_style, lv_palette_main(LV_PALETTE_LIME));

  screen->scope_image = lv_image_create(cs);
  lv_image_set_src(screen->scope_image, &oscImage);
  lv_obj_center(screen->scope_image);
  screen->scope_label = lv_label_create(cs);
  lv_obj_add_style(screen->scope_label, &osc_style, 0);
  lv_label_set_text(screen->scope_label, "");
  lv_obj_align(screen->scope_label, LV_ALIGN_BOTTOM_MID, 0, -8);

  screen->play_button = lv_imagebutton_create(screen->scope_image);
  lv_imagebutton_set_src(screen->play_button, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, &img_lv_demo_music_btn_playlarge, NULL);
  lv_imagebutton_set_src(screen->play_button, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &img_lv_demo_music_btn_pauselarge, NULL);
  lv_obj_add_event_cb(screen->play_button, pb_handler, LV_EVENT_CLICKED, screen);
  lv_obj_center(screen->play_button);

  lv_obj_add_style(screen->play_button, &style_def, 0);
  lv_obj_add_style(screen->play_button, &style_pr, LV_STATE_CHECKED);

  osThreadNew(StartOscMusic, haldev, &attributes_mixplayer);
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

    oscImage_map[right * 32 + left / 8 + I1_INDEX_SIZE] |= (0x80 >> (left & 7));
    mp++;
  }
  if (crate == 96000)
  {
    switch (dcount % 4)
    {
    case 0:
      lv_obj_invalidate(screen->scope_image);
      lv_timer_handler();
      break;
    case 1:
      memset(oscImage_map + I1_INDEX_SIZE, 0, sizeof(oscImage_map) - I1_INDEX_SIZE);
      break;
    default:
      break;
    }
  }
  else
  {
    switch (dcount % 6)
    {
    case 0:
      lv_obj_invalidate(screen->scope_image);
      lv_timer_handler();
      break;
    case 2:
      memset(oscImage_map + I1_INDEX_SIZE, 0, sizeof(oscImage_map) - I1_INDEX_SIZE);
      break;
    default:
      break;
    }
  }
}
