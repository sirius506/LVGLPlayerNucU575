/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_audio_embedded.c"

/*
 *  btstack_audio_embedded.c
 *
 *  Implementation of btstack_audio.h using hal_audio.h for embedded run loop
 *
 */

#include "btstack_config.h"

// allow to compile all files in embedded folder even if there's no audio support
#ifdef HAVE_HAL_AUDIO

#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop_freertos.h"
#include "hal_audio.h"

#include <stddef.h>

#define DRIVER_POLL_INTERVAL_MS          5

// client
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples);
#if 0
static void (*recording_callback)(const int16_t * buffer, uint16_t num_samples);
#endif

// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer_sink;
#if 0
static btstack_timer_source_t  driver_timer_source;
#endif

static volatile unsigned int output_buffer_to_play;
static          unsigned int output_buffer_to_fill;
static          unsigned int output_buffer_count;
static          unsigned int output_buffer_samples;

#if 0
static volatile int       input_buffer_ready;
static volatile const int16_t * input_buffer_samples;
static volatile uint16_t  input_buffer_num_samples;
#endif

#if 0
static int source_active;
#endif
static int sink_active;

// Support for stereo-only audio hal
#ifdef HAVE_HAL_AUDIO_SINK_STEREO_ONLY
static bool output_duplicate_samples;
#endif

static void btstack_audio_audio_played(uint8_t buffer_index){
    output_buffer_to_play = (buffer_index + 1) % output_buffer_count;
}

#if 0
static void btstack_audio_audio_recorded(const int16_t * samples, uint16_t num_samples){
    input_buffer_samples     = samples;
    input_buffer_num_samples = num_samples;
    input_buffer_ready       = 1;
}
#endif

static void driver_timer_handler_sink(btstack_timer_source_t * ts){

    // playback buffer ready to fill
    if (output_buffer_to_play != output_buffer_to_fill){
        int16_t * buffer = hal_audio_sink_get_output_buffer(output_buffer_to_fill);
        (*playback_callback)(buffer, output_buffer_samples);

#ifdef HAVE_HAL_AUDIO_SINK_STEREO_ONLY
        if (output_duplicate_samples){
            unsigned int i = output_buffer_samples;
            do {
                i--;
                int16_t sample = buffer[i];
                buffer[2*i + 0] = sample;
                buffer[2*i + 1] = sample;
            } while ( i > 0 );
        }
#endif

        // next
        output_buffer_to_fill = (output_buffer_to_fill + 1 ) % output_buffer_count;
    }

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

#if 0
static void driver_timer_handler_source(btstack_timer_source_t * ts){
    // deliver samples if ready
    if (input_buffer_ready){
        (*recording_callback)((const int16_t *)input_buffer_samples, input_buffer_num_samples);
        input_buffer_ready = 0;
    }

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}
#endif


static uint32_t sink_samplerate = 0;

static int btstack_audio_embedded_sink_init(
    uint8_t channels,
    uint32_t samplerate,
    void (*playback)(int16_t * buffer, uint16_t num_samples)
){
    btstack_assert(playback != NULL);
    btstack_assert(channels != 0);

#ifdef HAVE_HAL_AUDIO_SINK_STEREO_ONLY
    // always use stereo from hal, duplicate samples if needed
    output_duplicate_samples = channels != 2;
    channels = 2;
#endif

    playback_callback  = playback;
    sink_samplerate = samplerate;
    hal_audio_sink_init(channels, samplerate, &btstack_audio_audio_played);

    return 0;
}

static uint32_t btstack_audio_embedded_sink_get_samplerate() {
    return BSP_AUDIO_OUT_GetFrequency();
}

#if 0
static uint32_t source_samplerate = 0;

static int btstack_audio_embedded_source_init(
    uint8_t channels,
    uint32_t samplerate,
    void (*recording)(const int16_t * buffer, uint16_t num_samples)
){
    if (!recording){
        log_error("No recording callback");
        return 1;
    }

    recording_callback  = recording;
    source_samplerate = samplerate;
    hal_audio_source_init(channels, samplerate, &btstack_audio_audio_recorded);

    return 0;
}

static uint32_t btstack_audio_embedded_source_get_samplerate() {
    return source_samplerate;
}
#endif

static void btstack_audio_embedded_sink_set_volume(uint8_t volume){
    UNUSED(volume);
}

#if 0
static void btstack_audio_embedded_source_set_gain(uint8_t gain){
    UNUSED(gain);
}
#endif

static void btstack_audio_embedded_sink_start_stream(void){
    output_buffer_count   = hal_audio_sink_get_num_output_buffers();
    output_buffer_samples = hal_audio_sink_get_num_output_buffer_samples();

    btstack_assert(output_buffer_samples > 0);

    // pre-fill HAL buffers
    uint16_t i;
    for (i=0;i<output_buffer_count;i++){
        int16_t * buffer = hal_audio_sink_get_output_buffer(i);
        (*playback_callback)(buffer, output_buffer_samples);
    }

    output_buffer_to_play = 0;
    output_buffer_to_fill = 0;

    // start playback
    hal_audio_sink_start();

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer_sink, &driver_timer_handler_sink);
    btstack_run_loop_set_timer(&driver_timer_sink, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_sink);

    // state
    sink_active = 1;
}

#if 0
static void btstack_audio_embedded_source_start_stream(void){
    // just started, no data ready
    input_buffer_ready = 0;

    // start recording
    hal_audio_source_start();

    // start timer
#if 0
    btstack_run_loop_set_timer_handler(&driver_timer_source, &driver_timer_handler_source);
#endif
    btstack_run_loop_set_timer(&driver_timer_source, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_source);

    // state
    source_active = 1;
}
#endif

static void btstack_audio_embedded_sink_stop_stream(void){
    // stop stream
    hal_audio_sink_stop();
    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_sink);
    // state
    sink_active = 0;
}

#if 0
static void btstack_audio_embedded_source_stop_stream(void){
    // stop stream
    hal_audio_source_stop();
    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_source);
    // state
    source_active = 0;
}
#endif

static void btstack_audio_embedded_sink_close(void){
    // stop stream if needed
    if (sink_active){
        btstack_audio_embedded_sink_stop_stream();
    }
    // close HAL
    hal_audio_sink_close();
}

#if 0
static void btstack_audio_embedded_source_close(void){
    // stop stream if needed
    if (source_active){
        btstack_audio_embedded_source_stop_stream();
    }
    // close HAL
    hal_audio_source_close();
}
#endif

static const btstack_audio_sink_t btstack_audio_embedded_sink = {
        .init           = &btstack_audio_embedded_sink_init,
        .get_samplerate = &btstack_audio_embedded_sink_get_samplerate,
        .set_volume     = &btstack_audio_embedded_sink_set_volume,
        .start_stream   = &btstack_audio_embedded_sink_start_stream,
        .stop_stream    = &btstack_audio_embedded_sink_stop_stream,
        .close          = &btstack_audio_embedded_sink_close
};

#if 0
static const btstack_audio_source_t btstack_audio_embedded_source = {
        .init           = &btstack_audio_embedded_source_init,
        .get_samplerate = &btstack_audio_embedded_source_get_samplerate,
        .set_gain       = &btstack_audio_embedded_source_set_gain,
        .start_stream   = &btstack_audio_embedded_source_start_stream,
        .stop_stream    = &btstack_audio_embedded_source_stop_stream,
        .close          = &btstack_audio_embedded_source_close
};
#endif

const btstack_audio_sink_t * btstack_audio_embedded_sink_get_instance(void){
    return &btstack_audio_embedded_sink;
}

#if 0
const btstack_audio_source_t * btstack_audio_embedded_source_get_instance(void){
    return &btstack_audio_embedded_source;
}
#endif

#endif 
