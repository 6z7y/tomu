#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "structs.h"
#include "backend.h"

#include "../../libs/miniaudio.h"

static AudioRing *g_ring = &tctx.g_ring;

// Initialize ring buffer with dynamic capacity (in samples, not bytes)
void audio_ring_init(AudioRing *r, int channels, int sample_rate)
{
    // If already initialized, destroy first
    if (r->data) {
        pthread_mutex_destroy(&r->lock);
        pthread_cond_destroy(&r->has_space);
        pthread_cond_destroy(&r->has_data);
        free(r->data);
        r->data = NULL;
    }
    
    int capacity_samples = sample_rate * channels * 1;
    r->data = malloc(capacity_samples * sizeof(float));
    r->capacity = capacity_samples;
    r->write_pos = 0;
    r->read_pos = 0;
    r->count = 0;
    r->channels = channels;
    r->stopped = 0;
    pthread_mutex_init(&r->lock, NULL);
    pthread_cond_init(&r->has_space, NULL);
    pthread_cond_init(&r->has_data, NULL);
}

// Resize ring buffer if needed
void audio_ring_resize(AudioRing *r, int new_capacity_samples)
{
    pthread_mutex_lock(&r->lock);
    
    if (new_capacity_samples > r->capacity) {
        float *new_data = malloc(new_capacity_samples * sizeof(float));
        if (new_data) {
            // Copy existing data
            if (r->count > 0) {
                if (r->read_pos < r->write_pos) {
                    memcpy(new_data, r->data + r->read_pos, r->count * sizeof(float));
                } else {
                    size_t first_part = r->capacity - r->read_pos;
                    memcpy(new_data, r->data + r->read_pos, first_part * sizeof(float));
                    memcpy(new_data + first_part, r->data, r->write_pos * sizeof(float));
                }
            }
            free(r->data);
            r->data = new_data;
            r->read_pos = 0;
            r->write_pos = r->count;
            r->capacity = new_capacity_samples;
        }
    }
    
    pthread_mutex_unlock(&r->lock);
}

// Write PCM data to ring buffer (takes float planar, converts to interleaved)
void audio_ring_write_float(AudioRing *r, float **data, int channels, int nb_samples)
{
    if (!data || nb_samples <= 0) return;
    
    pthread_mutex_lock(&r->lock);
    
    // Wait if buffer is full
    while (r->count + (nb_samples * channels) > r->capacity && !r->stopped) {
        pthread_cond_wait(&r->has_space, &r->lock);
    }
    
    if (r->stopped) {
        pthread_mutex_unlock(&r->lock);
        return;
    }
    
    // Write interleaved float samples
    for (int s = 0; s < nb_samples; s++) {
        for (int ch = 0; ch < channels; ch++) {
            r->data[r->write_pos] = data[ch][s];
            r->write_pos = (r->write_pos + 1) % r->capacity;
            r->count++;
        }
    }
    
    pthread_cond_broadcast(&r->has_data);
    pthread_mutex_unlock(&r->lock);
}

// Read PCM data from ring buffer (reads interleaved float)
int audio_ring_read_float(AudioRing *r, float *output, int samples_needed)
{
    // FIX: Check if buffer is stopped before locking
    if (r->stopped) return 0;
    
    pthread_mutex_lock(&r->lock);
    
    while (r->count == 0 && !r->stopped) {
        pthread_cond_wait(&r->has_data, &r->lock);
    }
    
    if (r->count == 0 || r->stopped) {
        pthread_mutex_unlock(&r->lock);
        return 0;
    }
    
    int samples_to_read = (samples_needed < r->count) ? samples_needed : r->count;
    
    for (int i = 0; i < samples_to_read; i++) {
        output[i] = r->data[r->read_pos];
        r->read_pos = (r->read_pos + 1) % r->capacity;
        r->count--;
    }
    
    pthread_cond_broadcast(&r->has_space);
    pthread_mutex_unlock(&r->lock);
    
    return samples_to_read;
}


// miniaudio callback - reads float samples directly
void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount)
{
    Audio_Info *inf = &tctx.inf;
    TomuStatus *state = &tctx.state;
    
    // FIX: Check if device is still valid
    if (!ma_config || !output) return;

    pthread_mutex_lock(&state->lock);
    while (state->paused && state->running)
        pthread_cond_wait(&state->wait_cond, &state->lock);
    pthread_mutex_unlock(&state->lock);

    int samples_needed = frameCount * inf->ch;
    float *out = (float*)output;
    
    // FIX: Check if ring buffer is valid before reading
    if (!g_ring || !g_ring->data || g_ring->stopped) {
        // Fill with silence
        memset(out, 0, samples_needed * sizeof(float));
        return;
    }
    
    int samples_read = audio_ring_read_float(g_ring, out, samples_needed);
    
    // Fill remaining with silence if needed
    if (samples_read < samples_needed) {
        memset(out + samples_read, 0, (samples_needed - samples_read) * sizeof(float));
    }

    // Apply volume
    pthread_mutex_lock(&state->lock);
    if (state->volume != 1.00f) {
        for (int i = 0; i < samples_needed; i++) {
            out[i] *= state->volume;
        }
    }
    pthread_mutex_unlock(&state->lock);
}

// init miniaudio config - use FLOAT format
ma_device_config init_miniaudioConfig(Audio_Info *inf)
{
    ma_device_config ma_config = ma_device_config_init(ma_device_type_playback);

    ma_config.playback.channels = inf->ch;
    ma_config.playback.format = inf->ma_fmt;
    ma_config.sampleRate = inf->sample_rate;
    ma_config.dataCallback = ma_dataCallback;
    ma_config.pUserData = &tctx;

    return ma_config;
}

Audio_Buffer *audio_buffer_init(int channels, int sample_rate)
{
    audio_ring_init(g_ring, channels, sample_rate);
    Audio_Buffer *buf = malloc(sizeof(Audio_Buffer));
    memset(buf, 0, sizeof(Audio_Buffer));
    buf->capacity = sample_rate * channels * 1; // 1 second
    return buf;
}

void audio_buffer_reset()
{
    pthread_mutex_lock(&g_ring->lock);
    g_ring->read_pos = 0;
    g_ring->write_pos = 0;
    g_ring->count = 0;
    pthread_cond_broadcast(&g_ring->has_space);
    pthread_mutex_unlock(&g_ring->lock);
}

void audio_buffer_destroy(Audio_Buffer *buf)
{
    if (!buf) return;
    
    // FIX: Check if the ring buffer is already destroyed
    if (!g_ring->data) {
        free(buf);
        return;
    }
    
    // Signal all threads to stop
    pthread_mutex_lock(&g_ring->lock);
    g_ring->stopped = 1;
    pthread_cond_broadcast(&g_ring->has_data);
    pthread_cond_broadcast(&g_ring->has_space);
    pthread_mutex_unlock(&g_ring->lock);
    
    // FIX: Wait a bit for other threads to wake up and exit
    usleep(5000);  // 5ms
    
    if (g_ring->data) {
        free(g_ring->data);
        g_ring->data = NULL;
    }
    
    // FIX: Only destroy if they were initialized
    // Check if mutex/cond are still valid (use a flag or check)
    pthread_mutex_destroy(&g_ring->lock);
    pthread_cond_destroy(&g_ring->has_space);
    pthread_cond_destroy(&g_ring->has_data);
    
    free(buf);
}
