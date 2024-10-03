/*
 * DoomPlayer U575  Audio Output Driver
 *
 * This driver uses STM32U575 SAI as audio output device.
 *
 */
#include <string.h>
#include "bsp.h"
#include "rtosdef.h"
#include "board_if.h"
#include "audio_output.h"
#include "debug.h"

static void Player_Audio_Init(AUDIO_CONF *aconf, const AUDIO_INIT_PARAMS *param);
static void Player_Audio_Start(AUDIO_CONF *aconf);
static void Player_Audio_Stop(AUDIO_CONF *aconf);
static void Player_Audio_Pause(AUDIO_CONF *aconf);
static void Player_Audio_Resume(AUDIO_CONF *aconf);
static void Player_Audio_MixSound(AUDIO_CONF *aconf, const AUDIO_STEREO *psrc, int num_frame);
static void Player_Audio_SetVolume(AUDIO_CONF *conf, int vol);
static int Player_Audio_GetVolume(AUDIO_CONF *conf);
static void Player_Audio_SetRate(AUDIO_CONF *conf, uint32_t rate);
static uint32_t Player_Audio_GetRate(AUDIO_CONF *conf);

static const AUDIO_OUTPUT_DRIVER sai_output_driver = {
 .Init = Player_Audio_Init,
 .Start = Player_Audio_Start,
 .Stop = Player_Audio_Stop,
 .Pause = Player_Audio_Pause,
 .Resume = Player_Audio_Resume,
 .MixSound = Player_Audio_MixSound,
 .SetVolume = Player_Audio_SetVolume,
 .GetVolume = Player_Audio_GetVolume,
 .SetRate = Player_Audio_SetRate,
 .GetRate = Player_Audio_GetRate,
};

static const AUDIO_DEVCONF Player_Audio_DevieConf = {
 .mix_mode = MIXER_FFT_ENABLE,
 .playRate = 44100,
 .numChan = 2,
 .pDriver = &sai_output_driver,
};

MUTEX_DEF(sound_lock);

static AUDIO_CONF Audio_Conf;

/**
 * @brief Register/Get audio configuration
 * 
 * @param haldev: HAL device pointer
 * @return Audio configuration pointer
 */
AUDIO_CONF *get_audio_config(HAL_DEVICE *haldev)
{
  AUDIO_CONF *aconf = &Audio_Conf;

  if (haldev)
  {
    aconf->haldev = haldev;
    aconf->devconf = &Player_Audio_DevieConf;
  }
  return aconf;
}

/**
 * @brief Mix all sound channels
 */
static void MixSoundChannels(AUDIO_STEREO *pdst, int num_frame)
{
  CHANINFO *chanInfo = ChanInfo;
  int c;

  if (num_frame > NUM_FRAMES)
  {
    debug_printf("%s: %d > NUM_FRAMES\n", __FUNCTION__, num_frame);
    return;
  }

  memset(pdst, 0, sizeof(AUDIO_STEREO) * num_frame);

  for (c = 0; c < NUM_CHANNELS; c++)
  {
    if ((chanInfo->flag & (FL_SET|FL_PLAY)) == (FL_SET|FL_PLAY))
    {     
      int len = num_frame;
      AUDIO_STEREO *soundp = pdst;

      while (len > 0 && chanInfo->pread < chanInfo->plast)
      {
        soundp->ch0 += (chanInfo->pread->ch0 * chanInfo->vol_left) / MIX_MAX_VOLUME;
        soundp->ch1 += (chanInfo->pread->ch1 * chanInfo->vol_right) / MIX_MAX_VOLUME;
        soundp++;
        chanInfo->pread++;
        if (chanInfo->pread >= chanInfo->plast)
        {
          chanInfo->pread = (AUDIO_STEREO *)chanInfo->chunk->abuf;
          chanInfo->flag &= ~(FL_PLAY|FL_SET);
          break;
        }
        len--;
      }
    }
    chanInfo++;
  }
}

static void sai_half_comp(SAI_HandleTypeDef *hsai)
{
  DOOM_SAI_Handle *sai_audio = (DOOM_SAI_Handle *)hsai->UserPointer;
  (*sai_audio->saitx_half_comp)();
}

static void sai_full_comp(SAI_HandleTypeDef *hsai)
{
  DOOM_SAI_Handle *sai_audio = (DOOM_SAI_Handle *)hsai->UserPointer;
  (*sai_audio->saitx_full_comp)();
}

/**
 * @brief Initialize SAI audio driver
 */
static void Player_Audio_Init(AUDIO_CONF *aconf, const AUDIO_INIT_PARAMS *param)
{
  DOOM_SAI_Handle *audio = aconf->haldev->audio_sai;
  RCC_PeriphCLKInitTypeDef rcc_ex_clk_init_struct = { 0};

  if (aconf->soundLockId == 0)
  {
    aconf->soundLockId = osMutexNew(&attributes_sound_lock);
  }

  osMutexAcquire(aconf->soundLockId, osWaitForever);
  rcc_ex_clk_init_struct.PLL3.PLL3Source = RCC_PLLSOURCE_HSI;
  rcc_ex_clk_init_struct.PLL3.PLL3RGE = 0;
  rcc_ex_clk_init_struct.PLL3.PLL3FRACN = 0;
  rcc_ex_clk_init_struct.PLL3.PLL3ClockOut = RCC_PLL3_DIVP;
  rcc_ex_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
  rcc_ex_clk_init_struct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL3;
  rcc_ex_clk_init_struct.PLL3.PLL3Q = 2;
  rcc_ex_clk_init_struct.PLL3.PLL3R = 2;
  rcc_ex_clk_init_struct.PLL3.PLL3M = 1;

  if (param->sample_rate == 44100)
  {
    rcc_ex_clk_init_struct.PLL3.PLL3N = 21;
    rcc_ex_clk_init_struct.PLL3.PLL3P = 30;
    rcc_ex_clk_init_struct.PLL3.PLL3FRACN = 1380;
  }
  else
  {
    /* SAI clock config:
      PLL3_VCO Input = HSI_16Mhz/PLL3M = 16 Mhz
      PLL3_VCO Output = PLL3_VCO Input * PLL3N = 384 Mhz
      SAI_CLK_x = PLL3_VCO Output/PLL3P = 384/8 = 48 Mhz */
    rcc_ex_clk_init_struct.PLL3.PLL3N = 24;
    rcc_ex_clk_init_struct.PLL3.PLL3P = 8;
    rcc_ex_clk_init_struct.PLL3.PLL3FRACN = 4700;
  }

  HAL_RCCEx_PeriphCLKConfig(&rcc_ex_clk_init_struct);

  __HAL_RCC_SAI1_CLK_ENABLE();

  HAL_SAI_DeInit(audio->hsai);

  audio->saitx_half_comp = param->txhalf_comp;
  audio->saitx_full_comp = param->txfull_comp;

  audio->hsai->Init.AudioFrequency = param->sample_rate;
  HAL_SAI_Init(audio->hsai);
  HAL_SAI_RegisterCallback(audio->hsai, HAL_SAI_TX_HALFCOMPLETE_CB_ID, sai_half_comp);
  HAL_SAI_RegisterCallback(audio->hsai, HAL_SAI_TX_COMPLETE_CB_ID, sai_full_comp);

  aconf->sound_buffer = param->buffer;
  aconf->sound_buffer_size = param->buffer_size;
  bsp_codec_init(aconf->haldev->codec_i2c, param->volume, param->sample_rate);
  aconf->sample_rate = param->sample_rate;
  aconf->volume = param->volume;

  aconf->status = AUDIO_ST_INIT;

  osMutexRelease(aconf->soundLockId);
}

/**
 * @brief Start SAI DMA activity
 */
static void Player_Audio_Start(AUDIO_CONF *aconf)
{
  osMutexAcquire(aconf->soundLockId, osWaitForever);

  /* Initialize buffer pointers */
  aconf->freebuffer_ptr = aconf->sound_buffer;
  memset(aconf->sound_buffer, 0, aconf->sound_buffer_size);
  HAL_SAI_Transmit_DMA(aconf->haldev->audio_sai->hsai, (uint8_t *)(aconf->sound_buffer), aconf->sound_buffer_size/2);
  aconf->status = AUDIO_ST_PLAY;

  osMutexRelease(aconf->soundLockId);
}

/**
 * @brief Mix Music and all sound channels
 */
static void Player_Audio_MixSound(AUDIO_CONF *aconf, const AUDIO_STEREO *psrc, int num_frame)
{
  int i;
  AUDIO_STEREO *pdst;

  osMutexAcquire(aconf->soundLockId, osWaitForever);
  pdst = (AUDIO_STEREO *)(aconf->freebuffer_ptr);

  MixSoundChannels(pdst, num_frame);		/* Mix sound channels */

  /* Mix music and sounds */
  for (i = 0; i < num_frame; i++)
  {         
      pdst->ch0 += psrc->ch0;
      pdst->ch1 += psrc->ch1;
      psrc++;
      pdst++;
  }
  aconf->freebuffer_ptr += NUM_FRAMES * sizeof(AUDIO_STEREO);
  if (aconf->freebuffer_ptr >= aconf->sound_buffer + aconf->sound_buffer_size)
      aconf->freebuffer_ptr = aconf->sound_buffer;

  osMutexRelease(aconf->soundLockId);
}

/**
 * @brief Stop SAI activity
 */
static void Player_Audio_Stop(AUDIO_CONF *aconf)
{
  osMutexAcquire(aconf->soundLockId, osWaitForever);
  HAL_SAI_DMAStop(aconf->haldev->audio_sai->hsai);
  aconf->status = AUDIO_ST_STOP;
  osMutexRelease(aconf->soundLockId);
}

static void Player_Audio_Pause(AUDIO_CONF *aconf)
{
  if (aconf->status == AUDIO_ST_PLAY)
  {
    osMutexAcquire(aconf->soundLockId, osWaitForever);
    HAL_SAI_DMAPause(aconf->haldev->audio_sai->hsai);
    aconf->status = AUDIO_ST_PAUSE;
    osMutexRelease(aconf->soundLockId);
  }
}

static void Player_Audio_Resume(AUDIO_CONF *aconf)
{
  if (aconf->status == AUDIO_ST_PAUSE)
  {
    osMutexAcquire(aconf->soundLockId, osWaitForever);
    HAL_SAI_DMAResume(aconf->haldev->audio_sai->hsai);
    aconf->status = AUDIO_ST_PLAY;
    osMutexRelease(aconf->soundLockId);
  }
}

static void Player_Audio_SetVolume(AUDIO_CONF *conf, int vol)
{
  conf->volume = vol;
}

static int Player_Audio_GetVolume(AUDIO_CONF *conf)
{
  return conf->volume;
}

static void Player_Audio_SetRate(AUDIO_CONF *conf, uint32_t rate)
{
  if (conf->sample_rate != rate)
  {
    conf->sample_rate = rate;
    bsp_codec_init(conf->haldev->codec_i2c, conf->volume, rate);
  }
}

static uint32_t Player_Audio_GetRate(AUDIO_CONF *conf)
{
  return conf->sample_rate;
}
