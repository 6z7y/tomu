#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../../libs/miniaudio.h"
#include "audio_backend.h"
#include "backend.h"

// function take from planar_value to get interleaved_value
enum AVSampleFormat get_interleaved(enum AVSampleFormat value)
{
  switch (value){
    case AV_SAMPLE_FMT_DBLP: return AV_SAMPLE_FMT_DBL;
    case AV_SAMPLE_FMT_FLTP: return AV_SAMPLE_FMT_FLT;
    case AV_SAMPLE_FMT_S64P: return AV_SAMPLE_FMT_S64;
    case AV_SAMPLE_FMT_S32P: return AV_SAMPLE_FMT_S32;
    case AV_SAMPLE_FMT_S16P: return AV_SAMPLE_FMT_S16;
    case AV_SAMPLE_FMT_U8P: return AV_SAMPLE_FMT_U8;
    default: return AV_SAMPLE_FMT_S16; // fallback
  }
}

// function take from interleaved_value get mini audio format
ma_format get_ma_format(enum AVSampleFormat value)
{
  switch (value){
    case AV_SAMPLE_FMT_DBL: return ma_format_f32;
    case AV_SAMPLE_FMT_FLT: return ma_format_f32;
    case AV_SAMPLE_FMT_S64: return ma_format_s32;
    case AV_SAMPLE_FMT_S32: return ma_format_s32;
    case AV_SAMPLE_FMT_S16: return ma_format_s16;
    case AV_SAMPLE_FMT_U8: return ma_format_u8;
    default: return ma_format_s16; // fallback
  }
}

// store information Audio file to Audio_Info structure
void store_information(int audioStream_index, enum AVSampleFormat sample_fmt )
{
  Audio_Info *inf = &tctx.inf;

  #ifdef LEGACY_LIBSWRSAMPLE
    inf->ch = tctx.codecCTX->channels,
    inf->ch_layout = tctx.codecCTX->channel_layout,
  #else
    inf->ch = tctx.codecCTX->ch_layout.nb_channels,
    inf->ch_layout = tctx.codecCTX->ch_layout,
  #endif

  inf->audioStream_index = audioStream_index;
  inf->audioStream = tctx.fmtCTX->streams[audioStream_index];
  inf->sample_rate = tctx.codecCTX->sample_rate,
  inf->sample_fmt = sample_fmt,
  inf->sample_fmt_bytes = av_get_bytes_per_sample(inf->sample_fmt),
  inf->ma_fmt = get_ma_format(sample_fmt);
}

// function for search audio stream
int get_audioStream()
{
  for (int i=0; i<tctx.fmtCTX->nb_streams; i++) { // loop by number streams
    AVStream *stream = tctx.fmtCTX->streams[i]; // select index stream between 0..nb_stream
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) { // this index is audio stream?
      return i; // (yes) return index
    }
  }
  return -1; // (no) error
}

int init_decoder(AVCodecContext *codecCTX, const AVCodecParameters *codecPAR, const AVCodec *codecTYPE)
{

  avcodec_parameters_to_context(codecCTX, codecPAR); // copy codec information to decoder

  // initialize decoder with actual codec
  if (avcodec_open2(tctx.codecCTX, codecTYPE, NULL) < 0) {
    return warn("ffmpeg: cannot open codec!");
  }

  return 0;
}

// reads the file and creates a Stream Context
int get_audio_info(const char *filename)
{
  // | Container
  if (avformat_open_input(&tctx.fmtCTX, filename, NULL, NULL) < 0) {
    return warn("ffmpeg: can't read audio file");
  } // Get File Structure and store it in fmtCTX

  // || Container -> Streams
  if (avformat_find_stream_info(tctx.fmtCTX, NULL) < 0) {
    return warn("ffmpeg: can't find any streams");
  } // checking any streams in audio Structure

  int audioStream_index;
  if ((audioStream_index = get_audioStream()) < 0) {
    return warn("can't find audioStream");
  } // Search audio Stream

  // ||| Container -> Stream[Audio_Stream] -> Codec
  const AVCodecParameters *codecPAR = tctx.fmtCTX->streams[audioStream_index]->codecpar; // selected codec in audio stream
  const AVCodec *codecTYPE = avcodec_find_decoder(codecPAR->codec_id);
  if ( !codecTYPE ) {
    return warn("ffmpeg: unsupported codec id %d");
  } // select correct decoder type

  tctx.codecCTX = avcodec_alloc_context3(codecTYPE);
  if ( !tctx.codecCTX ) {
    return warn("ffmpeg: failed allocate codec!");
  } // allocate codecCTX=Decoder

  if (init_decoder(tctx.codecCTX, codecPAR, codecTYPE) < 0) {
    return warn("ffmpeg: can't init decoder");
  } // init the decoder engin

  enum AVSampleFormat sample_fmt = tctx.codecCTX->sample_fmt;
  if (av_sample_fmt_is_planar(sample_fmt)) {
    sample_fmt = get_interleaved(sample_fmt);
  } // if sample format is planar swap it to get interleaved type

  // Store audio info to a struct audio
  store_information(audioStream_index, sample_fmt);

  return 0;
}

// Setup SWR context convert
// Setup SWR context for format conversion
// Setup SWR context - FORCE S16 interleaved
int setup_sample_fmt_resampler(Audio_Info *inf, SwrContext **swrCTX)
{
    AVCodecContext *codecCTX = tctx.codecCTX;
    
    #ifdef LEGACY_LIBSWRSAMPLE
        // Legacy FFmpeg
        *swrCTX = swr_alloc_set_opts(NULL,
            codecCTX->channel_layout, AV_SAMPLE_FMT_S16, codecCTX->sample_rate, // output: S16
            codecCTX->channel_layout, codecCTX->sample_fmt, codecCTX->sample_rate, // input: native
            0, NULL
        );
    #else
        // New FFmpeg
        AVChannelLayout out_layout;
        av_channel_layout_default(&out_layout, codecCTX->ch_layout.nb_channels);
        
        int ret = swr_alloc_set_opts2(swrCTX,
            &out_layout, AV_SAMPLE_FMT_S16, codecCTX->sample_rate, // output: S16
            &codecCTX->ch_layout, codecCTX->sample_fmt, codecCTX->sample_rate, // input: native
            0, NULL
        );
        av_channel_layout_uninit(&out_layout);
        
        if (ret < 0) {
            fprintf(stderr, "[resampler] Failed to set options: %d\n", ret);
            return 0;
        }
    #endif

    if (*swrCTX) {
        printf("[resampler] Forcing S16: input fmt=%d, output fmt=S16, channels=%d, rate=%d\n",
               codecCTX->sample_fmt, (int)codecCTX->channel_layout, codecCTX->sample_rate);
    } else {
        fprintf(stderr, "[resampler] Failed to allocate\n");
        return 0;
    }

    return 1;
}

void setup_speed_resampler(Audio_Info *inf, AVFrame *frame, SwrContext **speed_swrCTX)
{
  int new_rate = (int)(inf->sample_rate / tctx.state.speed);
  enum AVSampleFormat input_fmt = frame->format;
  enum AVSampleFormat output_fmt = inf->sample_fmt;
  #ifdef LEGACY_LIBSWRSAMPLE
    uint64_t ch_layout_in = tctx.codecCTX->channel_layout;
    if (ch_layout_in == 0) {
      ch_layout_in = av_get_default_channel_layout(tctx.codecCTX->channels);
    }
    
    *speed_swrCTX = swr_alloc_set_opts(NULL,
      ch_layout_in, output_fmt, new_rate,
      ch_layout_in, input_fmt, inf->sample_rate,
      0, NULL
    );
  #else
    AVChannelLayout layout;
    av_channel_layout_default(&layout, inf->ch);
    
    int alloc_ret = swr_alloc_set_opts2(speed_swrCTX,
      &layout, output_fmt, new_rate,
      &layout, input_fmt, inf->sample_rate,
      0, NULL
    );
    av_channel_layout_uninit(&layout);
  #endif
  
  if (speed_swrCTX && swr_init(*speed_swrCTX) < 0) {
    swr_free(speed_swrCTX);
    speed_swrCTX = NULL;
  }
}

void init_playbackstatus(TomuStatus *state, uint loop, uint shuffle)
{
    state->running = 1;
    state->paused = 0;
    state->seek_request = 0;
    state->seek_target = 0;
    state->looping = loop;
    state->shuffle = shuffle;
    state->volume = 1.0f;
    state->speed = 1.0f;
    state->position = 0;
    state->duration = 0;
    state->skip_to_next = 0;
    
    // Only initialize if not already initialized
    // Use a flag or just destroy before re-init
    // For now, just destroy and re-init
    pthread_mutex_destroy(&state->lock);
    pthread_cond_destroy(&state->wait_cond);
    pthread_mutex_init(&state->lock, NULL);
    pthread_cond_init(&state->wait_cond, NULL);
}

void handle_audio_seek(int *duration_time, int64_t *total_samples_played)
{
  Audio_Info *inf = &tctx.inf;
  TomuStatus *state = &tctx.state;
  AVFormatContext *fmtCTX = tctx.fmtCTX;
  AVCodecContext *codecCTX = tctx.codecCTX;

  // Get current position in seconds
  double current_sec = (double)*total_samples_played / inf->sample_rate;
  
  // Calculate new position (seek_target is in microseconds, convert to seconds)
  double new_position_seconds = current_sec + ((double)state->seek_target / 1000000);
  
  // Clamp to valid range (0 to duration)
  if (new_position_seconds < 0) new_position_seconds = 0;
  if (new_position_seconds > *duration_time) new_position_seconds = *duration_time;
  
  // Convert to stream timebase for av_seek_frame
  // av_q2d converts AVRational to double: numerator / denominator
  int64_t target_pts = (int64_t)(new_position_seconds / av_q2d(inf->audioStream->time_base));
  
  // Perform the seek (ffmpeg wants stream timebase units, not microseconds!)
  av_seek_frame(fmtCTX, inf->audioStream_index, target_pts, AVSEEK_FLAG_BACKWARD);
  avcodec_flush_buffers(codecCTX);

  // Update sample counter
  *total_samples_played = (int64_t)(new_position_seconds * inf->sample_rate);

  // clear buffer (discard old audio)
  audio_buffer_reset();

  // reset seek flag
  state->seek_request = 0;
  state->seek_target = 0;
  return;
}

// Apply one AVDictionary's tags onto m, but only fields that are still empty.
// This lets us layer multiple metadata sources (format-level, then per-stream)
// without a later, emptier source overwriting a value we already found.
static void apply_metadata_dict(Audio_Metadata *m, AVDictionary *dict)
{
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(dict, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        if (!strcmp(tag->key, "title")) {
            if (!m->title[0]) strncpy(m->title, tag->value, sizeof(m->title) - 1);
        } else if (!strcmp(tag->key, "artist")) {
            if (!m->artist[0]) strncpy(m->artist, tag->value, sizeof(m->artist) - 1);
        } else if (!strcmp(tag->key, "album")) {
            if (!m->album[0]) strncpy(m->album, tag->value, sizeof(m->album) - 1);
        } else if (!strcmp(tag->key, "album_artist")) {
            if (!m->album_artist[0]) strncpy(m->album_artist, tag->value, sizeof(m->album_artist) - 1);
        } else if (!strcmp(tag->key, "genre")) {
            if (!m->genre[0]) strncpy(m->genre, tag->value, sizeof(m->genre) - 1);
        } else if (!strcmp(tag->key, "date")) {
            if (!m->date[0]) strncpy(m->date, tag->value, sizeof(m->date) - 1);
        } else if (!strcmp(tag->key, "track")) {
            if (!m->track[0]) strncpy(m->track, tag->value, sizeof(m->track) - 1);
        }
    }
}

void get_metadata(const char *filename)
{
    Audio_Metadata *m = &tctx.state.metadata;

    // ONLY clear if we don't already have metadata from yt-dlp
    if (m->title[0] == 0 && m->artist[0] == 0 && m->cover_path[0] == 0) {
        memset(m, 0, sizeof(*m));
    }

    // Only apply FFmpeg metadata if we don't have yt-dlp metadata
    if (tctx.fmtCTX) {
        if (!m->title[0]) apply_metadata_dict(m, tctx.fmtCTX->metadata);
        int idx = tctx.inf.audioStream_index;
        if (idx >= 0 && (unsigned)idx < tctx.fmtCTX->nb_streams) {
            if (!m->title[0]) apply_metadata_dict(m, tctx.fmtCTX->streams[idx]->metadata);
        }
    }

    // Last resort fallback
    if (!m->title[0]) {
        if (filename && strlen(filename) > 0) {
            const char *base = strrchr(filename, '/');
            base = base ? base + 1 : filename;
            strncpy(m->title, base, sizeof(m->title) - 1);
            char *dot = strrchr(m->title, '.');
            if (dot) *dot = '\0';
        } else if (tctx.stream_ctx.is_streaming) {
            if (tctx.stream_ctx.original_url) {
                const char *base = strrchr(tctx.stream_ctx.original_url, '/');
                if (base) {
                    base = base + 1;
                    char *clean = strdup(base);
                    char *q = strchr(clean, '?');
                    if (q) *q = '\0';
                    strncpy(m->title, clean, sizeof(m->title) - 1);
                    free(clean);
                }
            }
            if (!m->title[0]) {
                strncpy(m->title, "Stream", sizeof(m->title) - 1);
            }
        }
    }

    m->title[sizeof(m->title)-1] = '\0';
    m->artist[sizeof(m->artist)-1] = '\0';
    m->album[sizeof(m->album)-1] = '\0';
    m->album_artist[sizeof(m->album_artist)-1] = '\0';
    m->genre[sizeof(m->genre)-1] = '\0';
    m->date[sizeof(m->date)-1] = '\0';
    m->track[sizeof(m->track)-1] = '\0';
}

///////////////////////////////////////////////////// about extract cover img
// Tiny FNV-1a hash so we can key the cover cache off the FULL input path,
// not just the basename — avoids two different files that happen to share
// a filename (e.g. "01.flac" in two different album folders) colliding on
// the same /tmp cache entry and showing each other's stale cover art.
static unsigned long fnv1a_hash(const char *str) {
    unsigned long hash = 2166136261UL;
    while (*str) {
        hash ^= (unsigned char)(*str++);
        hash *= 16777619UL;
    }
    return hash;
}

// Helper: extract filename without path and build full output path
void build_output_path(const char *input, char *out, size_t size) {
    // Get just the filename from the full path (for a readable suffix only)
    const char *base = strrchr(input, '/');
    base = (base) ? base + 1 : input;

    char name_without_ext[256];
    strncpy(name_without_ext, base, sizeof(name_without_ext) - 1);
    name_without_ext[sizeof(name_without_ext) - 1] = '\0';

    char *dot = strrchr(name_without_ext, '.');
    if (dot) *dot = '\0';

    // Build full path: /tmp/tomu_cover_img/<hash-of-full-path>_<filename>.jpg
    // The hash of the FULL input path is what guarantees uniqueness; the
    // filename suffix is just there to make the cache dir human-readable.
    snprintf(out, size, "/tmp/tomu_cover_img/%lx_%s.jpg", fnv1a_hash(input), name_without_ext);
}

int get_cover(AVFormatContext *fmt, const char *input) {
    // Create directory properly
    system("mkdir -p /tmp/tomu_cover_img 2>/dev/null");

    // Build output path keyed off the full input path (collision-proof),
    // see build_output_path() / fnv1a_hash() above for why.
    char output_path[512];
    build_output_path(input, output_path, sizeof(output_path));
    
    printf("Looking for cover in: %s\n", input);
    printf("Will save to: %s\n", output_path);
    
    // Search for attached picture in streams
    for (unsigned int i = 0; i < fmt->nb_streams; i++) {
        AVStream *stream = fmt->streams[i];
        
        if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            AVPacket pkt = stream->attached_pic;
            
            FILE *f = fopen(output_path, "wb");
            if (!f) {
                printf("Could not create output file: %s\n", output_path);
                return 1;
            }
            
            size_t written = fwrite(pkt.data, 1, pkt.size, f);
            fclose(f);
            
            if (written == pkt.size) {
                printf("✅ Cover saved: %s (%zu bytes)\n", output_path, written);
                // Store cover path in metadata for MPRIS
                strncpy(tctx.state.metadata.cover_path, output_path, sizeof(tctx.state.metadata.cover_path) - 1);
                return 0;
            }
        }
    }
    
    printf("No cover art found in file\n");
    return 1;
}

// Add this to backend_utils.c after get_cover():
int extract_cover(const char *input, AVFormatContext *fmt_tctx) {
    printf("Extracting cover from: %s\n", input);
    return get_cover(fmt_tctx, input);
}

// Add this function to backend_utils.c - extracts cover art from any format context
// Add this function to backend_utils.c - extracts cover art from any format context
int extract_cover_from_stream(AVFormatContext *fmt) {
    if (!fmt) return 1;
    
    // If we already have a cover from yt-dlp, don't override it
    if (strlen(tctx.state.metadata.cover_path) > 0) {
        printf("[cover] Already have cover from yt-dlp: %s\n", tctx.state.metadata.cover_path);
        return 0;
    }
    
    // Create directory
    system("mkdir -p /tmp/tomu_cover_img 2>/dev/null");
    
    char output_path[512];
    
    // Use title if available, otherwise use timestamp
    if (strlen(tctx.state.metadata.title) > 0) {
        char clean_title[256];
        strncpy(clean_title, tctx.state.metadata.title, sizeof(clean_title) - 1);
        // Sanitize
        for (int i = 0; clean_title[i]; i++) {
            if (clean_title[i] == '/' || clean_title[i] == '\\' || 
                clean_title[i] == ':' || clean_title[i] == '*' || 
                clean_title[i] == '?' || clean_title[i] == '"' || 
                clean_title[i] == '<' || clean_title[i] == '>' || 
                clean_title[i] == '|' || clean_title[i] == ' ') {
                clean_title[i] = '_';
            }
        }
        snprintf(output_path, sizeof(output_path), "/tmp/tomu_cover_img/%s.jpg", clean_title);
    } else {
        snprintf(output_path, sizeof(output_path), "/tmp/tomu_cover_img/stream_cover_%ld.jpg", time(NULL));
    }
    
    printf("[cover] Looking for cover art in stream...\n");
    printf("[cover] Will save to: %s\n", output_path);
    
    // Search for attached picture in streams
    for (unsigned int i = 0; i < fmt->nb_streams; i++) {
        AVStream *stream = fmt->streams[i];
        
        if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            AVPacket pkt = stream->attached_pic;
            
            FILE *f = fopen(output_path, "wb");
            if (!f) {
                printf("[cover] Could not create output file: %s\n", output_path);
                return 1;
            }
            
            size_t written = fwrite(pkt.data, 1, pkt.size, f);
            fclose(f);
            
            if (written == pkt.size) {
                printf("[cover] ✅ Cover saved: %s (%zu bytes)\n", output_path, written);
                strncpy(tctx.state.metadata.cover_path, output_path, sizeof(tctx.state.metadata.cover_path) - 1);
                return 0;
            }
        }
    }
    
    printf("[cover] No cover art found in stream\n");
    return 1;
}
