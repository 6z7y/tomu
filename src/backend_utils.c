#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "../libs/miniaudio.h"

#include "backend.h"
#include "backend_utils.h"

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

// fn to search correct stream you want
int get_stream(int type)
{
  for (int i = 0; i < ctx.fmtCTX->nb_streams; i++){
    AVStream *stream = ctx.fmtCTX->streams[i];
    if (stream->codecpar->codec_type == type )
      return i;
  }
  return -1;
}

// store information Audio file to Audio_Info structure
void store_information(int audioStream_index, enum AVSampleFormat output_sample_fmt )
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
  inf->sample_fmt = output_sample_fmt,
  inf->sample_fmt_bytes = av_get_bytes_per_sample(inf->sample_fmt),
  inf->ma_fmt = get_ma_format(output_sample_fmt);
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
  // int new_rate = (int)(inf->sample_rate * ctx->state->speed);
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
  state->looping = loop;

  state->seek_request = 0;
  state->seek_target = 0;

  pthread_mutex_init(&state->lock, NULL);
  pthread_cond_init(&state->wait_cond, NULL);
}

void print_metadata(AVDictionary *metadata)
{
  AVDictionaryEntry *tag = NULL;

  printf("File tags:\n");
  while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
    printf("  %s : %s\n", tag->key, tag->value);
  }
}

// init miniaudio config before using
ma_device_config init_miniaudioConfig(Audio_Info *inf)
{
  ma_device_config ma_config = ma_device_config_init(ma_device_type_playback);

  ma_config.playback.channels = inf->ch;
  ma_config.playback.format = inf->ma_fmt;
  ma_config.sampleRate = inf->sample_rate;
  ma_config.dataCallback = ma_dataCallback;
  ma_config.pUserData = &ctx;

  return ma_config;
}

Audio_Buffer *audio_buffer_init(int capacity)
{
  Audio_Buffer *buf = malloc(sizeof(Audio_Buffer));

  buf->pcm_data = malloc(capacity);
  buf->capacity = capacity;
  buf->write_pos = 0;     // Start writing at beginning
  buf->read_pos = 0;      // Start reading from beginning
  buf->filled = 0;        // Buffer starts empty

  pthread_mutex_init(&buf->lock, NULL);
  pthread_cond_init(&buf->data_ready, NULL);
  pthread_cond_init(&buf->space_free, NULL);
  return buf;
}

// Reset audio buffer to empty state (used after seeking to discard old audio)
void audio_buffer_reset()
{
  pthread_mutex_lock(&ctx.buf->lock);

    ctx.buf->filled = 0;
    ctx.buf->read_pos = 0;
    ctx.buf->write_pos = 0;
    pthread_cond_broadcast(&ctx.buf->space_free);

  pthread_mutex_unlock(&ctx.buf->lock);
}

void audio_buffer_destroy(Audio_Buffer *buf)
{
  if (buf ){
    free(buf->pcm_data);
    pthread_mutex_destroy(&buf->lock);
    pthread_cond_destroy(&buf->data_ready);
    pthread_cond_destroy(&buf->space_free);
    free (ctx.buf);
  }
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

inline void progress(Audio_State *state, double current_time, int duration_time)
{
  int bar_width = 30;

  int pos = (current_time / duration_time) * bar_width;

  printf("\0337");
  printf("\033[0J");
  printf("\r%s[", ctx.state.paused ? " (Paused) " : "");
  for (int i = 0; i < bar_width; i++){

    if (i < pos)
      printf("=");

    else if (i == pos)
      printf(">");

    else
      printf(".");
  }

    printf("] %d:%02d:%02d / %d:%02d:%02d (~%.00f%%) | %.2fx v: %.0f%%, s:%d, l:%d\r",
    get_hour(current_time), get_min(current_time), get_sec(current_time), 
    get_hour(duration_time), get_min(duration_time), get_sec(duration_time),
    (current_time / duration_time) * 100.0, state->speed,
    state->volume * 100.0f, state->shuffle, state->looping
  );
  printf("\0338");

  fflush(stdout);
}

// Read all the files in dir and return them 
char** extractDir(const char* path){
  // why do i realloc ? because i want O(n) 
  // its better than count then add all the files it will be O(n^2)

  // TODO make it only extract audio files
  // TODO if there isn't any file close the program 

  DIR *dir = opendir(path);
  int capacity = 10;
  char **files = malloc(capacity * sizeof(char*));

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) // remove (".", "..")
      continue;

    // Grow array if needed
    if (DirFiles.totalFiles == capacity) {
      capacity *= 2;
      char **tmp = realloc(files, capacity * sizeof(char *));
      if (!tmp) {
        // cleanup on failure
        for (int i = 0; i < DirFiles.totalFiles; i++)
          free(files[i]);
        free(files);
        closedir(dir);
        return NULL;
      }
      files = tmp;

    }
    files[DirFiles.totalFiles++] = strdup(entry->d_name);
  }
  return files;
}
