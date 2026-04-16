#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../../libs/miniaudio.h"
#include "audio_backend.h"
#include "backend.h"
#include "backend_utils.h"

// function take from planar_value to get interleaved_value
inline enum AVSampleFormat get_interleaved(enum AVSampleFormat value)
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
inline ma_format get_ma_format(enum AVSampleFormat value)
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
static inline void store_information(int audioStream_index, enum AVSampleFormat sample_fmt )
{
  Audio_Info *inf = &ctx.inf;

  #ifdef LEGACY_LIBSWRSAMPLE
    inf->ch = ctx.codecCTX->channels,
    inf->ch_layout = ctx.codecCTX->channel_layout,
  #else
    inf->ch = ctx.codecCTX->ch_layout.nb_channels,
    inf->ch_layout = ctx.codecCTX->ch_layout,
  #endif

  inf->audioStream_index = audioStream_index;
  inf->audioStream = ctx.fmtCTX->streams[audioStream_index];
  inf->sample_rate = ctx.codecCTX->sample_rate,
  inf->sample_fmt = sample_fmt,
  inf->sample_fmt_bytes = av_get_bytes_per_sample(inf->sample_fmt),
  inf->ma_fmt = get_ma_format(sample_fmt);
}

// function for search audio stream
inline int get_audioStream()
{
  for_each_num(ctx.fmtCTX->nb_streams) { // loop by number streams
    AVStream *stream = ctx.fmtCTX->streams[i]; // select index stream between 0..nb_stream
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
  if (avcodec_open2(ctx.codecCTX, codecTYPE, NULL) < 0) {
    return warn("ffmpeg: cannot open codec!");
  }

  return 0;
}

// reads the file and creates a Stream Context
int get_audio_info(const char *filename)
{
  // | Container
  if (avformat_open_input(&ctx.fmtCTX, filename, NULL, NULL) < 0) {
    return warn("ffmpeg: can't read audio file");
  } // Get File Structure and store it in fmtCTX

  // || Container -> Streams
  if (avformat_find_stream_info(ctx.fmtCTX, NULL) < 0) {
    return warn("ffmpeg: can't find any streams");
  } // checking any streams in audio Structure

  int audioStream_index = get_audioStream();
  if (audioStream_index < 0) {
    return warn("can't find audioStream");
  } // Search audio Stream

  // ||| Container -> Stream[Audio_Stream] -> Codec
  const AVCodecParameters *codecPAR = ctx.fmtCTX->streams[audioStream_index]->codecpar; // selected codec in audio stream
  const AVCodec *codecTYPE = avcodec_find_decoder(codecPAR->codec_id);
  if ( !codecTYPE ) {
    return warn("ffmpeg: unsupported codec id %d");
  } // select correct decoder type

  ctx.codecCTX = avcodec_alloc_context3(codecTYPE);
  if ( !ctx.codecCTX ) {
    return warn("ffmpeg: failed allocate codec!");
  } // allocate codecCTX=Decoder

  if (init_decoder(ctx.codecCTX, codecPAR, codecTYPE) < 0) {
    return warn("ffmpeg: can't init decoder");
  } // init the decoder engin

  enum AVSampleFormat sample_fmt = ctx.codecCTX->sample_fmt;
  if (av_sample_fmt_is_planar(sample_fmt)) {
    sample_fmt = get_interleaved(sample_fmt);
  } // if sample format is planar swap it to get interleaved type

  // Store audio info to a struct audio
  store_information(audioStream_index, sample_fmt);

  return 0;
}

// Setup SWR context convert
int setup_sample_fmt_resampler(Audio_Info *inf, SwrContext **swrCTX)
{
  #ifdef LEGACY_LIBSWRSAMPLE
    *swrCTX = swr_alloc_set_opts(*swrCTX,
      inf->ch_layout, inf->sample_fmt, inf->sample_rate, // output
      inf->ch_layout, ctx.codecCTX->sample_fmt, inf->sample_rate, // input
      0, NULL
    );
  #else
    swr_alloc_set_opts2(swrCTX,
      &inf->ch_layout, inf->sample_fmt, inf->sample_rate, // output
      &inf->ch_layout, ctx.codecCTX->sample_fmt, inf->sample_rate, // input
      0, NULL
    );
  #endif

  return 1;
}

void setup_speed_resampler(Audio_Info *inf, AVFrame *frame, SwrContext **speed_swrCTX)
{
  int new_rate = (int)(inf->sample_rate / ctx.state.speed);
  enum AVSampleFormat input_fmt = frame->format;
  enum AVSampleFormat output_fmt = inf->sample_fmt;
  #ifdef LEGACY_LIBSWRSAMPLE
    uint64_t ch_layout_in = ctx.codecCTX->channel_layout;
    if (ch_layout_in == 0) {
      ch_layout_in = av_get_default_channel_layout(ctx.codecCTX->channels);
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

void init_playbackstatus(Audio_State *state, uint loop, uint shuffle)
{
  state->running = 1;
  state->paused = 0;
  state->volume = 1.00f;
  state->speed = 1.00f;
  state->looping = 0;
  state->shuffle = 1;

  state->seek_request = 0;
  state->seek_target = 0;

  state->ready = 0;  // ← Not ready until file is loaded

  pthread_mutex_init(&state->lock, NULL);
  pthread_cond_init(&state->wait_cond, NULL);
}

void handle_audio_seek(int *duration_time, int64_t *total_samples_played)
{
  Audio_Info *inf = &ctx.inf;
  Audio_State *state = &ctx.state;
  AVFormatContext *fmtCTX = ctx.fmtCTX;
  AVCodecContext *codecCTX = ctx.codecCTX;

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

void get_metadata()
{
  Audio_Metadata *metadata = &ctx.state.metadata;
  AVDictionaryEntry *tag = NULL;

  printf("File tags:\n");
  while ((tag = av_dict_get(ctx.fmtCTX->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
    printf("  %s : %s\n", tag->key, tag->value);

    if      (!strcmp(tag->key, "title")) metadata->title = strdup(tag->value);
    else if (!strcmp(tag->key, "artist")) metadata->artist = strdup(tag->value);
    else if (!strcmp(tag->key, "album")) metadata->album = strdup(tag->value);
    else if (!strcmp(tag->key, "album_artist")) metadata->album_artist = strdup(tag->value);
    else if (!strcmp(tag->key, "genre")) metadata->genre = strdup(tag->value);
    else if (!strcmp(tag->key, "date")) metadata->date = strdup(tag->value);
    else if (!strcmp(tag->key, "track")) metadata->track = strdup(tag->value);
  }
}


///////////////////////////////////////////////////// about extract cover img
int make_dir(const char *path) {
    if (mkdir(path, 0755) == 0) {
        return 0; // created
    }

    if (errno == EEXIST) {
        return 0; // already exists (this is OK)
    }

    return 1; // error
}
// helper: extract filename without path and extension
static void build_output_path(const char *input, char *out, size_t size) {
    const char *base = strrchr(input, '/');
    base = (base) ? base + 1 : input;

    char name[256];
    strncpy(name, base, sizeof(name));
    name[sizeof(name) - 1] = '\0';

    // remove extension
    char *dot = strrchr(name, '.');
    if (dot) *dot = '\0';

    snprintf(out, size, "/tmp/tomu_cover_img/%s.png", name);
}

int get_cover(AVFormatContext *fmt, const char *input) {
    char output[512];

    build_output_path(input, output, sizeof(output));

    printf("Saving to: %s\n", output);

    make_dir("/tmp/tomu_cover_img");

    // 🔥 NEW: skip if file already exists
    if (access(output, F_OK) == 0) {
        printf("File already exists, skipping...\n");
        return 0;
    }

    for (unsigned int i = 0; i < fmt->nb_streams; i++) {
        AVStream *stream = fmt->streams[i];

        if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {

            AVPacket pkt = stream->attached_pic;

            FILE *f = fopen(output, "wb");
            if (!f) {
                printf("Could not create output file\n");
                return 1;
            }

            fwrite(pkt.data, 1, pkt.size, f);
            fclose(f);

            printf("Cover saved successfully\n");
            return 0;
        }
    }

    printf("No cover found\n");
    return 1;
}

int extract_cover(const char *input) {
    if (get_cover(ctx.fmtCTX, input) == 0) return 0;

    printf("No cover found\n");
    return 1;
}
