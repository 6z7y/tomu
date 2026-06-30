#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "DATA.h"
#include "audio_backend.h"
#include "backend.h"
#include "backend_utils.h"
#include "decoder.h"

#include "../../libs/miniaudio.h"

#include "streaming.h"
#include "utils1.h"
#include "utils2.h"

int playback_run(const char *filename, uint loop_mode, uint shuffle_mode)
{
    printf("have files: %d\n", tctx.list.queue_count);
    av_log_set_level(AV_LOG_QUIET);

    int is_streaming = is_url(filename);
    
    // First, get audio info from the file or stream
    if (is_streaming) {
        if (streaming_init_playback(&tctx, filename) < 0) {
            fprintf(stderr, "[backend] Failed to initialize streaming\n");
            // Clean up and return - don't continue
            cleanUP();
            return -1;
        }
        tctx.inf.audioStream_index = -1;
        
        printf("[backend] Getting audio info from stream...\n");
        
        pthread_t info_thread;
        if (pthread_create(&info_thread, NULL, get_audio_info_thread, NULL) != 0) {
            fprintf(stderr, "[backend] Failed to create info thread\n");
            cleanUP();
            return -1;
        }
        pthread_join(info_thread, NULL);
        
        if (tctx.inf.audioStream_index < 0) {
            fprintf(stderr, "[backend] Failed to get audio info from stream\n");
            cleanUP();
            return -1;
        }
        
        printf("[backend] Stream info: channels=%d, rate=%d\n",
               tctx.inf.ch, tctx.inf.sample_rate);
        
        // Don't call get_metadata here - it will clear what we already have
        // get_metadata(NULL);
    } else {
        if (get_audio_info(filename) < 0) {
            cleanUP();
            return -1;
        }
        get_metadata(filename);
        extract_cover(filename, tctx.fmtCTX);
        
        printf("[backend] File info: channels=%d, rate=%d\n",
               tctx.inf.ch, tctx.inf.sample_rate);
    }

    // Check if we have valid audio info before proceeding
    if (tctx.inf.sample_rate == 0 || tctx.inf.ch == 0) {
        fprintf(stderr, "[backend] Invalid audio format (sample_rate=%d, channels=%d)\n",
                tctx.inf.sample_rate, tctx.inf.ch);
        cleanUP();
        return -1;
    }

    // Initialize playback status
    init_playbackstatus(&tctx.state, loop_mode, shuffle_mode);
    tctx.state.volume = 1.00f;

    // Initialize audio buffer with actual audio info - USE FLOAT!
    tctx.inf.ma_fmt = ma_format_f32;
    tctx.inf.sample_fmt = AV_SAMPLE_FMT_FLT;
    tctx.inf.sample_fmt_bytes = sizeof(float);
    
    // Initialize ring buffer with channels and sample rate
    tctx.buf = audio_buffer_init(tctx.inf.ch, tctx.inf.sample_rate);
    printf("[backend] Buffer initialized: %d samples (%.2f seconds)\n", 
           (int)tctx.buf->capacity, (float)tctx.buf->capacity / (tctx.inf.sample_rate * tctx.inf.ch));

    // Initialize miniaudio device with FLOAT format
    ma_device device;
    ma_device_config ma_config = init_miniaudioConfig(&tctx.inf);
    
    printf("[backend] Miniaudio config: %d channels, %d Hz, format=float32\n",
           tctx.inf.ch, tctx.inf.sample_rate);

    if (ma_device_init(NULL, &ma_config, &device) != MA_SUCCESS) {
        fprintf(stderr, "[backend] Failed to initialize miniaudio\n");
        goto clean_every;
    }

    // Start decoder thread
    pthread_t decoder_thread;
    if (pthread_create(&decoder_thread, NULL, run_decoder, NULL) != 0) {
        fprintf(stderr, "[backend] Failed to create decoder thread\n");
        ma_device_uninit(&device);
        goto clean_every;
    }
    
    // Start audio playback
    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "[backend] Failed to start audio device\n");
        ma_device_uninit(&device);
        goto clean_every;
    }
    printf("[backend] Audio playback started\n");

    pthread_join(decoder_thread, NULL);
    printf("here\n");

clean_every:
    tctx.state.running = 0;

    // 1. Stop the audio device FIRST
    ma_device_stop(&device);
    ma_device_uninit(&device);
    
    // 2. Wait for any pending callbacks to finish
    // ma_device_uninit should handle this, but be safe
    usleep(10000);  // 10ms
    
    // 3. NOW destroy the buffer (safe because device is stopped)
    if (tctx.buf) {
        audio_buffer_destroy(tctx.buf);
        tctx.buf = NULL;
    }
    
    // 4. Clean up state mutex/cond
    pthread_mutex_destroy(&tctx.state.lock);
    pthread_cond_destroy(&tctx.state.wait_cond);
    
    // 5. Clean up FFmpeg contexts
    cleanUP();
    
    return 0;
}
