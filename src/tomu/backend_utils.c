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
int get_audioStream()
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

  int audioStream_index;
  if ((audioStream_index = get_audioStream()) < 0) {
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

void init_playbackstatus(TomuStatus *state, uint loop, uint shuffle)
{
  state->running = 1;
  state->paused = 0;
  state->volume = 1.00f;
  state->speed = 1.00f;
  state->looping = loop;
  state->shuffle = shuffle;

  state->seek_request = 0;
  state->seek_target = 0;


  pthread_mutex_init(&state->lock, NULL);
  pthread_cond_init(&state->wait_cond, NULL);
}

void handle_audio_seek(int *duration_time, int64_t *total_samples_played)
{
  Audio_Info *inf = &ctx.inf;
  TomuStatus *state = &ctx.state;
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
  Audio_Metadata *m = &ctx.state.metadata;
  
  // Clear arrays (no freeing needed)
  memset(m, 0, sizeof(*m));
  
  AVDictionaryEntry *tag = NULL;
  while ((tag = av_dict_get(ctx.fmtCTX->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
    // if (!strcmp(tag->key, "title"))        strncpy(m->title, tag->value, sizeof(m->title) - 1);
    if (!strcmp(tag->key, "artist"))  strncpy(m->artist, tag->value, sizeof(m->artist) - 1);
    else if (!strcmp(tag->key, "album"))   strncpy(m->album, tag->value, sizeof(m->album) - 1);
    else if (!strcmp(tag->key, "album_artist")) strncpy(m->album_artist, tag->value, sizeof(m->album_artist) - 1);
    else if (!strcmp(tag->key, "genre"))   strncpy(m->genre, tag->value, sizeof(m->genre) - 1);
    else if (!strcmp(tag->key, "date"))    strncpy(m->date, tag->value, sizeof(m->date) - 1);
    else if (!strcmp(tag->key, "track"))   strncpy(m->track, tag->value, sizeof(m->track) - 1);
  }
  
  // Ensure null termination
  // m->title[sizeof(m->title)-1] = '\0';
  m->artist[sizeof(m->artist)-1] = '\0';
  m->album[sizeof(m->album)-1] = '\0';
  m->album_artist[sizeof(m->album_artist)-1] = '\0';
  m->genre[sizeof(m->genre)-1] = '\0';
  m->date[sizeof(m->date)-1] = '\0';
  m->track[sizeof(m->track)-1] = '\0';
}

///////////////////////////////////////////////////// about extract cover img
// Helper: extract filename without path and build full output path
void build_output_path(const char *input, char *out, size_t size) {
    // Get just the filename from the full path
    const char *base = strrchr(input, '/');
    base = (base) ? base + 1 : input;
    
    // Remove extension if present (like .mp3, .flac, etc.)
    char name_without_ext[256];
    strncpy(name_without_ext, base, sizeof(name_without_ext) - 1);
    name_without_ext[sizeof(name_without_ext) - 1] = '\0';

    
    char *dot = strrchr(name_without_ext, '.');
    if (dot) *dot = '\0';

    strcpy(ctx.state.metadata.title, name_without_ext);
    printf("name: titile : %s\n", ctx.state.metadata.title);
    
    // Build full path: /tmp/tomu_cover_img/filename.jpg
    snprintf(out, size, "/tmp/tomu_cover_img/%s.jpg", name_without_ext);
}

int make_dir(const char *path) {
    if (mkdir(path, 0755) == 0) {
        return 0; // created
    }

    if (errno == EEXIST) {
        return 0; // already exists (this is OK)
    }

    return 1; // error
}

int get_cover(AVFormatContext *fmt, const char *input) {
    char output[512];
    
    // Create directory first
    make_dir("/tmp/tomu_cover_img");
    
    // Build output path (now this actually does something!)
    build_output_path(input, output, sizeof(output));
    
    printf("Looking for cover in: %s\n", input);
    printf("Will save to: %s\n", output);
    
    // Skip if file already exists
    if (access(output, F_OK) == 0) {
        printf("Cover already exists, skipping...\n");
        return 0;
    }
    
    // Search for attached picture in streams
    for (unsigned int i = 0; i < fmt->nb_streams; i++) {
        AVStream *stream = fmt->streams[i];
        
        if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            AVPacket pkt = stream->attached_pic;
            
            FILE *f = fopen(output, "wb");
            if (!f) {
                printf("Could not create output file: %s\n", output);
                return 1;
            }
            
            size_t written = fwrite(pkt.data, 1, pkt.size, f);
            fclose(f);
            
            if (written == pkt.size) {
                printf("✅ Cover saved: %s (%zu bytes)\n", output, written);
                return 0;
            } else {
                printf("⚠️ Cover partially written: %zu/%d bytes\n", written, pkt.size);
                return 1;
            }
        }
    }
    
    printf("No cover art found in file\n");
    return 1;
}

int extract_cover(const char *input) {
    if (!input || !ctx.fmtCTX) {
        printf("No file or format context\n");
        return 1;
    }
    
    printf("Extracting cover from: %s\n", input);
    return get_cover(ctx.fmtCTX, input);
}
