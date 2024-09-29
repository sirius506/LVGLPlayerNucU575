/*
 * DoomPlayer U575  Audio Output Driver
 *
 * This driver uses STM32U575 SAI as audio output device.
 *
 */
#include <string.h>
#include "bsp.h"
#include "audio_output.h"
#include "debug.h"

SECTION_SRDSRAM AUDIO_STEREO FinalAudioBuffer[BUF_FRAMES];

extern void mix_request_data(int full);

static void SAI_Audio_Init(AUDIO_CONF *aconf);
static void SAI_Audio_Start(AUDIO_CONF *aconf);
static void SAI_Audio_Stop(AUDIO_CONF *aconf);
static void SAI_Audio_MixSound(AUDIO_CONF *aconf, const AUDIO_STEREO *psrc, int num_frame);
static void SAI_Audio_SetVolume(AUDIO_CONF *conf, int vol);

static const AUDIO_OUTPUT_DRIVER sai_output_driver = {
 .Init = SAI_Audio_Init,
 .Start = SAI_Audio_Start,
 .Stop = SAI_Audio_Stop,
 .MixSound = SAI_Audio_MixSound,
 .SetVolume = SAI_Audio_SetVolume,
};

static const AUDIO_DEVCONF SAI_Audio_DevieConf = {
 .mix_mode = MIXER_FFT_ENABLE,
 .playRate = 44100,
 .numChan = 2,
 .pDriver = &sai_output_driver,
};

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
    aconf->devconf = &SAI_Audio_DevieConf;
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

void bsp_pause_audio(HAL_DEVICE *haldev)
{
  int st;

  DOOM_SAI_Handle *audio = haldev->audio_sai;
  st = HAL_SAI_DMAPause(audio->hsai);
  debug_printf("pause: %d\n", st);
}
void bsp_resume_audio(HAL_DEVICE *haldev)
{
  DOOM_SAI_Handle *audio = haldev->audio_sai;
  HAL_SAI_DMAResume(audio->hsai);
}

/**
 * @brief Called by SAI DMA half complete interrupt
 */
static void sai_half_complete(SAI_HandleTypeDef *hsai)
{
  UNUSED(hsai);

  mix_request_data(0);
}

/**
 * @brief Called by SAI DMA full complete interrupt
 */
static void sai_full_complete(SAI_HandleTypeDef *hsai)
{
  UNUSED(hsai);

  mix_request_data(1);
}

static void sai_error(SAI_HandleTypeDef *hsai)
{
  debug_printf("SAI Error: %x\n", hsai->ErrorCode);
}

/**
 * @brief Initialize SAI audio driver
 */
static void SAI_Audio_Init(AUDIO_CONF *aconf)
{
  DOOM_SAI_Handle *audio = aconf->haldev->audio_sai;

  /* Register DMA complete callbacks */
  HAL_SAI_RegisterCallback(audio->hsai, HAL_SAI_TX_HALFCOMPLETE_CB_ID, sai_half_complete);
  HAL_SAI_RegisterCallback(audio->hsai, HAL_SAI_TX_COMPLETE_CB_ID, sai_full_complete);
  HAL_SAI_RegisterCallback(audio->hsai, HAL_SAI_ERROR_CB_ID, sai_error);

  /* Initialize buffer pointers */
  aconf->sound_buffer = FinalAudioBuffer;
  aconf->sound_buffer_size = sizeof(FinalAudioBuffer);
  aconf->freebuffer_ptr = aconf->sound_buffer;
  aconf->volume = 80;
  memset(FinalAudioBuffer, 0, sizeof(FinalAudioBuffer));
}

/**
 * @brief Start SAI DMA activity
 */
static void SAI_Audio_Start(AUDIO_CONF *aconf)
{
  /* Start SAI activity */
  HAL_SAI_Transmit_DMA(aconf->haldev->audio_sai->hsai, (const uint32_t *)FinalAudioBuffer, BUF_FRAMES*2);
}

/**
 * @brief Mix Music and all sound channels
 */
static void SAI_Audio_MixSound(AUDIO_CONF *aconf, const AUDIO_STEREO *psrc, int num_frame)
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
static void SAI_Audio_Stop(AUDIO_CONF *aconf)
{
  HAL_SAI_DMAStop(aconf->haldev->audio_sai->hsai);
}

static void SAI_Audio_SetVolume(AUDIO_CONF *conf, int vol)
{
  conf->volume = vol;
}
