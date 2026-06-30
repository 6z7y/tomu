#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "DATA.h"
#include "audio_backend.h"
#include "backend_utils.h"
#include "mpris.h"
#include "streaming.h"

#include "../../libs/miniaudio.h"

// Thread to get audio info without playing
// Thread to get audio info without playing
// In get_audio_info_thread, after extracting metadata:
void *get_audio_info_thread(void *arg)
{
    (void)arg;
    printf("[info_thread] Getting audio info...\n");
    
    AVFormatContext *fmtCTX = tctx.fmtCTX;
    if (!fmtCTX) {
        fprintf(stderr, "[info_thread] No format context\n");
        return NULL;
    }

    if (avformat_find_stream_info(fmtCTX, NULL) < 0) {
        fprintf(stderr, "[info_thread] Failed to find stream info\n");
        return NULL;
    }

    int audioStream_index = -1;
    for (unsigned int i = 0; i < fmtCTX->nb_streams; i++) {
        if (fmtCTX->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream_index = i;
            break;
        }
    }

    if (audioStream_index < 0) {
        fprintf(stderr, "[info_thread] No audio stream found\n");
        return NULL;
    }

    const AVCodecParameters *codecPAR = fmtCTX->streams[audioStream_index]->codecpar;
    const AVCodec *codecTYPE = avcodec_find_decoder(codecPAR->codec_id);
    if (!codecTYPE) {
        fprintf(stderr, "[info_thread] Unsupported codec\n");
        return NULL;
    }

    AVCodecContext *codecCTX = avcodec_alloc_context3(codecTYPE);
    if (!codecCTX) {
        fprintf(stderr, "[info_thread] Failed to allocate codec context\n");
        return NULL;
    }

    avcodec_parameters_to_context(codecCTX, codecPAR);
    if (avcodec_open2(codecCTX, codecTYPE, NULL) < 0) {
        fprintf(stderr, "[info_thread] Failed to open codec\n");
        avcodec_free_context(&codecCTX);
        return NULL;
    }

    tctx.codecCTX = codecCTX;

    // Store audio info
    #ifdef LEGACY_LIBSWRSAMPLE
        tctx.inf.ch = codecCTX->channels;
        tctx.inf.ch_layout = codecCTX->channel_layout;
    #else
        tctx.inf.ch = codecCTX->ch_layout.nb_channels;
        tctx.inf.ch_layout = codecCTX->ch_layout;
    #endif

    tctx.inf.audioStream_index = audioStream_index;
    tctx.inf.audioStream = fmtCTX->streams[audioStream_index];
    tctx.inf.sample_rate = codecCTX->sample_rate;
    tctx.inf.sample_fmt = AV_SAMPLE_FMT_FLT;
    tctx.inf.sample_fmt_bytes = sizeof(float);
    tctx.inf.ma_fmt = ma_format_f32;

    int duration_sec = fmtCTX->duration / 1000000;
    tctx.state.duration = duration_sec;

    printf("[info_thread] Audio info: channels=%d, rate=%d, duration=%d\n",
           tctx.inf.ch, tctx.inf.sample_rate, duration_sec);

    // Metadata is already extracted in start_playback_thread
    printf("[info_thread] FINAL METADATA:\n");
    printf("  Title:  [%s]\n", tctx.state.metadata.title[0] ? tctx.state.metadata.title : "(empty)");
    printf("  Artist: [%s]\n", tctx.state.metadata.artist[0] ? tctx.state.metadata.artist : "(empty)");
    printf("  Cover:  [%s]\n", tctx.state.metadata.cover_path[0] ? tctx.state.metadata.cover_path : "(empty)");
    
    // Force MPRIS update
    mpris_notify_change();

    return NULL;
}

// Main decoder thread - works like the test program
void *run_decoder(void *arg)
{
    (void)arg;
    printf("[decoder] Thread started\n");
    AVFormatContext *fmtCTX = tctx.fmtCTX;
    AVCodecContext *codecCTX = tctx.codecCTX;
    Audio_Info *inf = &tctx.inf;
    TomuStatus *state = &tctx.state;

    if (!codecCTX) {
        fprintf(stderr, "[decoder] No codec context\n");
        return NULL;
    }

    // Resampler to convert any format to float planar (FLTP)
    SwrContext *swrCTX = NULL;
    SwrContext *speed_swrCTX = NULL;

    // Check if we need resampling to FLTP
    enum AVSampleFormat decoder_fmt = codecCTX->sample_fmt;
    printf("[decoder] Decoder format: %d, target: FLTP\n", decoder_fmt);

    if (decoder_fmt != AV_SAMPLE_FMT_FLTP) {
        printf("[decoder] Setting up resampler to convert to FLTP\n");
        
        #ifdef LEGACY_LIBSWRSAMPLE
            swrCTX = swr_alloc_set_opts(NULL,
                codecCTX->channel_layout, AV_SAMPLE_FMT_FLTP, codecCTX->sample_rate,
                codecCTX->channel_layout, codecCTX->sample_fmt, codecCTX->sample_rate,
                0, NULL
            );
        #else
            AVChannelLayout out_layout = codecCTX->ch_layout;
            int ret = swr_alloc_set_opts2(&swrCTX,
                &out_layout, AV_SAMPLE_FMT_FLTP, codecCTX->sample_rate,
                &codecCTX->ch_layout, codecCTX->sample_fmt, codecCTX->sample_rate,
                0, NULL
            );
            if (ret < 0) {
                fprintf(stderr, "[decoder] Failed to set resampler options\n");
                return NULL;
            }
        #endif

        if (swrCTX) {
            if (swr_init(swrCTX) < 0) {
                fprintf(stderr, "[decoder] Failed to init resampler\n");
                swr_free(&swrCTX);
                swrCTX = NULL;
            } else {
                printf("[decoder] Resampler initialized\n");
            }
        }
    } else {
        printf("[decoder] Already FLTP, no resampler needed\n");
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    if (!packet || !frame) {
        printf("[decoder] Failed to allocate packet/frame\n");
        if (swrCTX) swr_free(&swrCTX);
        if (speed_swrCTX) swr_free(&speed_swrCTX);
        return NULL;
    }

    int64_t total_samples_played = 0;
    int duration_sec = fmtCTX->duration / 1000000;
    tctx.state.duration = duration_sec;
    float last_speed = state->speed;

    printf("[decoder] Starting decode loop\n");

decode:
    while (av_read_frame(fmtCTX, packet) >= 0) {
        if (packet->stream_index == inf->audioStream_index) {
            if (avcodec_send_packet(codecCTX, packet) < 0) {
                av_packet_unref(packet);
                continue;
            }

            while (avcodec_receive_frame(codecCTX, frame) >= 0) {
                double current_time = (double)total_samples_played / inf->sample_rate;
                tctx.state.position = (int)current_time;
                total_samples_played += frame->nb_samples;

                pthread_mutex_lock(&state->lock);
                if (state->seek_request) {
                    handle_audio_seek(&duration_sec, &total_samples_played);
                    av_packet_unref(packet);
                    av_frame_unref(frame);
                    pthread_mutex_unlock(&state->lock);
                    goto decode;
                }
                pthread_mutex_unlock(&state->lock);

                if (state->speed != last_speed) {
                    last_speed = state->speed;
                    if (speed_swrCTX) {
                        swr_free(&speed_swrCTX);
                        speed_swrCTX = NULL;
                    }
                    if (state->speed != 1.0f) {
                        setup_speed_resampler(inf, frame, &speed_swrCTX);
                    }
                }

                pthread_mutex_lock(&state->lock);
                while (state->paused)
                    pthread_cond_wait(&state->wait_cond, &state->lock);
                if (state->running == 0) {
                    pthread_mutex_unlock(&state->lock);
                    break;
                }
                pthread_mutex_unlock(&state->lock);

                // Get float planar data
                int nb_samples = frame->nb_samples;
                int channels = inf->ch;

                if (speed_swrCTX) {
                    // Speed conversion first
                    int out_samples = frame->nb_samples / state->speed;
                    if (out_samples <= 0) out_samples = 1;
                    
                    // Allocate temp buffer for speed conversion
                    float **temp_data = malloc(channels * sizeof(float*));
                    for (int i = 0; i < channels; i++) {
                        temp_data[i] = malloc(out_samples * sizeof(float));
                    }
                    
                    // Convert speed
                    int samples = swr_convert(speed_swrCTX, (uint8_t**)temp_data, out_samples,
                                             (const uint8_t**)frame->data, frame->nb_samples);
                    
                    if (samples > 0) {
                        // Now convert format if needed
                        if (swrCTX) {
                            // Need to convert from temp to FLTP
                            float **final_data = malloc(channels * sizeof(float*));
                            for (int i = 0; i < channels; i++) {
                                final_data[i] = malloc(samples * sizeof(float));
                            }
                            
                            int final_samples = swr_convert(swrCTX, (uint8_t**)final_data, samples,
                                                           (const uint8_t**)temp_data, samples);
                            
                            if (final_samples > 0) {
                                audio_ring_write_float(&tctx.g_ring, final_data, channels, final_samples);
                            }
                            
                            for (int i = 0; i < channels; i++) {
                                free(final_data[i]);
                            }
                            free(final_data);
                        } else {
                            // Just use temp data
                            audio_ring_write_float(&tctx.g_ring, temp_data, channels, samples);
                        }
                    }
                    
                    for (int i = 0; i < channels; i++) {
                        free(temp_data[i]);
                    }
                    free(temp_data);
                    
                } else if (swrCTX) {
                    // Convert to FLTP
                    float **float_out = malloc(channels * sizeof(float*));
                    for (int i = 0; i < channels; i++) {
                        float_out[i] = malloc(nb_samples * sizeof(float));
                    }
                    
                    int samples = swr_convert(swrCTX, (uint8_t**)float_out, nb_samples,
                                             (const uint8_t**)frame->data, frame->nb_samples);
                    
                    if (samples > 0) {
                        audio_ring_write_float(&tctx.g_ring, float_out, channels, samples);
                    }
                    
                    for (int i = 0; i < channels; i++) {
                        free(float_out[i]);
                    }
                    free(float_out);
                    
                } else {
                    // Already FLTP - write directly to ring buffer
                    audio_ring_write_float(&tctx.g_ring, (float**)frame->data, channels, frame->nb_samples);
                }

                av_frame_unref(frame);
            }
        }
        av_packet_unref(packet);

        if (!state->running) break;
        
        // For streaming, check for errors
        if (tctx.stream_ctx.is_streaming) {
            StreamBuf *sb = (StreamBuf *)tctx.stream_ctx.stream_buf;
            pthread_mutex_lock(&sb->lock);
            int error = sb->error;
            pthread_mutex_unlock(&sb->lock);
            
            if (error) {
                fprintf(stderr, "[decoder] Streaming error occurred\n");
                break;
            }
        }
    }

    // Looping only for local files
    if (!tctx.stream_ctx.is_streaming && state->looping && state->running) {
        av_seek_frame(fmtCTX, -1, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codecCTX);
        total_samples_played = 0;
        goto decode;
    }

    // In run_decoder() cleanup section:
    // Cleanup
    pthread_mutex_lock(&state->lock);
    state->running = 0;
    pthread_cond_broadcast(&state->wait_cond);
    pthread_mutex_unlock(&state->lock);

    // FIX: Check if buf exists before accessing its lock
    if (tctx.buf) {
        pthread_mutex_lock(&tctx.buf->lock);
        tctx.buf->stopped = 1;
        pthread_cond_broadcast(&tctx.buf->data_ready);
        pthread_mutex_unlock(&tctx.buf->lock);
    }

    pthread_mutex_lock(&tctx.buf->lock);
    tctx.buf->stopped = 1;
    pthread_cond_broadcast(&tctx.buf->data_ready);
    pthread_mutex_unlock(&tctx.buf->lock);

    if (tctx.stream_ctx.is_streaming) {
        streaming_cleanup(&tctx);
    }

    if (swrCTX) swr_free(&swrCTX);
    if (speed_swrCTX) swr_free(&speed_swrCTX);
    av_frame_free(&frame);
    av_packet_free(&packet);

    printf("[decoder] Thread finished\n");
    return NULL;
}
