/*
 * Copyright (C) 2017 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "hal_audio.c"

#include "DoomPlayer.h"
#include "hal_audio.h"
#include "btstack_debug.h"
#include "board_if.h"
#include "audio_output.h"

// output
#define OUTPUT_BUFFER_NUM_SAMPLES       NUM_FRAMES
#define NUM_OUTPUT_BUFFERS              BUF_FACTOR

static void (*audio_played_handler)(uint8_t buffer_index);
static int playback_started;
static uint32_t sink_sample_rate;

// our storage
extern AUDIO_STEREO FinalAudioBuffer[BUF_FRAMES];

int BSP_AUDIO_OUT_GetFrequency()
{
    return 44100;
}

void  btaudio_HalfTransfer_CallBack(uint32_t Instance){

    (*audio_played_handler)(0);
}

void btaudio_TransferComplete_CallBack(uint32_t Instance){

    (*audio_played_handler)(1);
}

/**
 * @brief Setup audio codec for specified samplerate and number channels
 * @param Channels
 * @param Sample rate
 * @param Buffer played callback
 * @param Buffer recorded callback (use NULL if no recording)
 */
void hal_audio_sink_init(uint8_t channels, 
                    uint32_t sample_rate,
                    void (*buffer_played_callback)  (uint8_t buffer_index)){

    // We only supports stereo playback
    if (channels == 1){
        log_error("We only supports stereo playback. Please #define HAVE_HAL_AUDIO_SINK_STEREO_ONLY");
        return;
    }

    audio_played_handler = buffer_played_callback;
    sink_sample_rate = sample_rate;
}

/**
 * @brief Get number of output buffers in HAL
 * @returns num buffers
 */
uint16_t hal_audio_sink_get_num_output_buffers(void){
    return NUM_OUTPUT_BUFFERS;
}

/**
 * @brief Get size of single output buffer in HAL
 * @returns buffer size
 */
uint16_t hal_audio_sink_get_num_output_buffer_samples(void){
    return OUTPUT_BUFFER_NUM_SAMPLES;
}

/**
 * @brief Reserve output buffer
 * @returns buffer
 */
int16_t * hal_audio_sink_get_output_buffer(uint8_t buffer_index){
    switch (buffer_index){
        case 0:
            return (int16_t *)FinalAudioBuffer;
        case 1:
            return (int16_t *)&FinalAudioBuffer[NUM_FRAMES];
        default:
            return NULL;
    }
}

/**
 * @brief Start stream
 */
void hal_audio_sink_start(void)
{
  HAL_DEVICE *haldev = &HalDevice;

  playback_started = 1;

debug_printf("%s:\n", __FUNCTION__);
#if 1
  HAL_SAI_RegisterCallback(haldev->audio_sai->hsai, HAL_SAI_TX_HALFCOMPLETE_CB_ID, btaudio_HalfTransfer_CallBack);
  HAL_SAI_RegisterCallback(haldev->audio_sai->hsai, HAL_SAI_TX_COMPLETE_CB_ID, btaudio_TransferComplete_CallBack);

  HAL_SAI_Transmit_DMA(haldev->audio_sai->hsai, (const uint32_t *)FinalAudioBuffer, BUF_FRAMES*2);
#endif
}

/**
 * @brief Stop stream
 */
void hal_audio_sink_stop(void){
  HAL_DEVICE *haldev = &HalDevice;

  playback_started = 0;
  HAL_SAI_DMAStop(haldev->audio_sai->hsai);
#ifdef XX
    BSP_AUDIO_OUT_Stop(0);
#endif
}

/**
 * @brief Close audio codec
 */
void hal_audio_sink_close(void){
    if (playback_started){
        hal_audio_sink_stop();
    }
}
