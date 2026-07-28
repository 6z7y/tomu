#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <string.h>
#include <unistd.h>

#include "../../libs/miniaudio.h"
#include "output.h"
#include "errors.h"
#include "macros.h"
#include "structs.h"
#include "utils.h"

enum AVSampleFormat get_interleaved(enum AVSampleFormat value)
{
  switch (value) {
    case AV_SAMPLE_FMT_DBLP: return AV_SAMPLE_FMT_DBL;
    case AV_SAMPLE_FMT_FLTP: return AV_SAMPLE_FMT_FLT;
    case AV_SAMPLE_FMT_S64P: return AV_SAMPLE_FMT_S64;
    case AV_SAMPLE_FMT_S32P: return AV_SAMPLE_FMT_S32;
    case AV_SAMPLE_FMT_S16P: return AV_SAMPLE_FMT_S16;
    case AV_SAMPLE_FMT_U8P:  return AV_SAMPLE_FMT_U8;
    default:                 return AV_SAMPLE_FMT_S16;
  }
}

ma_format get_ma_format(enum AVSampleFormat value)
{
  switch (value) {
    case AV_SAMPLE_FMT_DBL: return ma_format_f32;
    case AV_SAMPLE_FMT_FLT: return ma_format_f32;
    case AV_SAMPLE_FMT_S64: return ma_format_s32;
    case AV_SAMPLE_FMT_S32: return ma_format_s32;
    case AV_SAMPLE_FMT_S16: return ma_format_s16;
    case AV_SAMPLE_FMT_U8:  return ma_format_u8;
    default:                return ma_format_s16;
  }
}

static void extract_title_from_path(PlayBackContext *ctx, const char *filename)
{
  const char *point = strrchr(filename, '/');
  const char *base = point ? point + 1 : filename;

  char title[256];
  strncpy(title, base, sizeof(title) - 1);
  title[sizeof(title) - 1] = '\0';

  char *dot = strrchr(title, '.');
  if (dot) *dot = '\0';

  strncpy(ctx->state.metadata.title, title, sizeof(ctx->state.metadata.title) - 1);
  ctx->state.metadata.title[sizeof(ctx->state.metadata.title) - 1] = '\0';
}

static void store_information(PlayBackContext *ctx, AVFormatContext *fmtCTX, AVCodecContext *decoderCTX, int audioStream_index, const char *filename)
{
  Audio_Info *inf = &ctx->inf;
  extract_title_from_path(ctx, filename);

  inf->audioStream_index = audioStream_index;
  inf->audioStream = fmtCTX->streams[audioStream_index];

  #ifdef LEGACY_LIBSWRSAMPLE
    inf->ch        = decoderCTX->channels;
    inf->ch_layout = decoderCTX->channel_layout;
  #else
    inf->ch        = decoderCTX->ch_layout.nb_channels;
    inf->ch_layout = decoderCTX->ch_layout;
  #endif

  enum AVSampleFormat sample_fmt = decoderCTX->sample_fmt;
  if (av_sample_fmt_is_planar(sample_fmt))
    sample_fmt = get_interleaved(sample_fmt);

  inf->sample_fmt = sample_fmt;
  inf->sample_fmt_bytes = av_get_bytes_per_sample(sample_fmt);
  inf->sample_rate = decoderCTX->sample_rate;
  inf->ma_fmt = get_ma_format(sample_fmt);
}

int get_audioStream_index(AVFormatContext *fmtCTX)
{
  for (unsigned int i = 0; i < fmtCTX->nb_streams; i++) {
    AVStream *stream = fmtCTX->streams[i];
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      return i;
  }
  return -1;
}

int get_audio_info(PlayBackContext *ctx, const char *filename)
{
  AVFormatContext *fmtCTX     = NULL;
  AVCodecContext  *decoderCTX = NULL;

  if (avformat_open_input(&fmtCTX, filename, NULL, NULL) < 0)
    return warn("ffmpeg: can't read audio source");

  if (avformat_find_stream_info(fmtCTX, NULL) < 0) {
    avformat_close_input(&fmtCTX);
    return warn("ffmpeg: cannot find stream info");
  }

  int audioStream_index = get_audioStream_index(fmtCTX);
  if (audioStream_index < 0) {
    avformat_close_input(&fmtCTX);
    return warn("can't find audioStream");
  }

  const AVCodecParameters *codecPAR = fmtCTX->streams[audioStream_index]->codecpar;

  AVCodec *codecid = avcodec_find_decoder(codecPAR->codec_id);
  if (!codecid) {
    avformat_close_input(&fmtCTX);
    return warn("ffmpeg: unsupported codec id %d", codecPAR->codec_id);
  }

  decoderCTX = avcodec_alloc_context3(codecid);
  if (!decoderCTX) {
    avformat_close_input(&fmtCTX);
    return warn("ffmpeg: failed allocate codec!");
  }

  avcodec_parameters_to_context(decoderCTX, codecPAR);

  if (avcodec_open2(decoderCTX, codecid, NULL) < 0) {
    avformat_close_input(&fmtCTX);
    avcodec_free_context(&decoderCTX);
    return warn("ffmpeg: cannot open codec!");
  }

  store_information(ctx, fmtCTX, decoderCTX, audioStream_index, filename);

  ctx->fmtCTX = fmtCTX;
  ctx->decoderCTX = decoderCTX;

  return 0;
}

int setup_sample_fmt_resampler(PlayBackContext *ctx, Audio_Info *inf, SwrContext **swrCTX)
{
  #ifdef LEGACY_LIBSWRSAMPLE
    *swrCTX = swr_alloc_set_opts(*swrCTX,
      inf->ch_layout, inf->sample_fmt, inf->sample_rate,
      inf->ch_layout, ctx->decoderCTX->sample_fmt, inf->sample_rate,
      0, NULL
    );
  #else
    swr_alloc_set_opts2(swrCTX,
      &inf->ch_layout, inf->sample_fmt, inf->sample_rate,
      &inf->ch_layout, ctx->decoderCTX->sample_fmt, inf->sample_rate,
      0, NULL
    );
  #endif

  return 1;
}

void setup_speed_resampler(PlayBackContext *ctx, Audio_Info *inf, AVFrame *frame, SwrContext **speed_swrCTX)
{
  int new_rate = (int)(inf->sample_rate / ctx->state.speed);
  enum AVSampleFormat input_fmt = frame->format;
  enum AVSampleFormat output_fmt = inf->sample_fmt;

  #ifdef LEGACY_LIBSWRSAMPLE
    uint64_t ch_layout_in = ctx->decoderCTX->channel_layout;
    if (ch_layout_in == 0)
      ch_layout_in = av_get_default_channel_layout(ctx->decoderCTX->channels);

    *speed_swrCTX = swr_alloc_set_opts(NULL,
      ch_layout_in, output_fmt, new_rate,
      ch_layout_in, input_fmt, inf->sample_rate,
      0, NULL
    );
  #else
    AVChannelLayout layout;
    av_channel_layout_default(&layout, inf->ch);

    swr_alloc_set_opts2(speed_swrCTX,
      &layout, output_fmt, new_rate,
      &layout, input_fmt, inf->sample_rate,
      0, NULL
    );
    av_channel_layout_uninit(&layout);
  #endif

  if (*speed_swrCTX && swr_init(*speed_swrCTX) < 0)
    swr_free(speed_swrCTX);
}

void init_playbackstatus(PlaybackStatus *state)
{
  state->running = 1;
  state->paused = 0;
  state->seek_request = 0;
  state->seek_target = 0;
  // state->loop = loop;
  // state->shuffle = shuffle;
  state->volume = 1.0f;
  state->speed = 1.0f;
  state->position = 0;
  state->duration = 0;
  state->skip_to_next = 0;
}


static void apply_metadata_dict(Audio_Metadata *m, AVDictionary *dict)
{
  AVDictionaryEntry *tag = NULL;
  while ((tag = av_dict_get(dict, "", tag, AV_DICT_IGNORE_SUFFIX))) {
    if      (!strcmp(tag->key, "artist"))       strncpy(m->artist,       tag->value, sizeof(m->artist)       - 1);
    else if (!strcmp(tag->key, "album"))        strncpy(m->album,        tag->value, sizeof(m->album)        - 1);
    else if (!strcmp(tag->key, "album_artist")) strncpy(m->album_artist, tag->value, sizeof(m->album_artist) - 1);
    else if (!strcmp(tag->key, "genre"))        strncpy(m->genre,        tag->value, sizeof(m->genre)        - 1);
    else if (!strcmp(tag->key, "composer"))     strncpy(m->composer,     tag->value, sizeof(m->composer)     - 1);
    else if (!strcmp(tag->key, "disc"))         strncpy(m->disc,         tag->value, sizeof(m->disc)         - 1);
    else if (!strcmp(tag->key, "date"))         strncpy(m->date,         tag->value, sizeof(m->date)         - 1);
    else if (!strcmp(tag->key, "track"))        strncpy(m->track,        tag->value, sizeof(m->track)        - 1);
  }
}

void get_metadata(PlayBackContext *ctx, const char *filename)
{
  Audio_Metadata *m = &ctx->state.metadata;

  apply_metadata_dict(m, ctx->fmtCTX->metadata);
  int idx = ctx->inf.audioStream_index;
  if (idx >= 0 && (unsigned)idx < ctx->fmtCTX->nb_streams)
    apply_metadata_dict(m, ctx->fmtCTX->streams[idx]->metadata);

  if (!m->title[0]) {
    if (filename && strlen(filename) > 0) {
      extract_title_from_path(ctx, filename);
    } else if (ctx->stream_ctx.is_streaming) {
      // if (ctx->stream_ctx.original_url) {
      //   const char *base = strrchr(ctx->stream_ctx.original_url, '/');
      //   if (base) {
      //     base = base + 1;
      //     char *clean = strdup(base);
      //     char *q = strchr(clean, '?');
      //     if (q) *q = '\0';
      //     strncpy(m->title, clean, sizeof(m->title) - 1);
      //     free(clean);
      //   }
      // }
      if (!m->title[0])
        strncpy(m->title, "Stream", sizeof(m->title) - 1);
    }
  }

  m->title[sizeof(m->title) - 1] = '\0';
  m->artist[sizeof(m->artist) - 1] = '\0';
  m->album[sizeof(m->album) - 1] = '\0';
  m->album_artist[sizeof(m->album_artist) - 1] = '\0';
  m->genre[sizeof(m->genre) - 1] = '\0';
  m->date[sizeof(m->date) - 1] = '\0';
  m->track[sizeof(m->track) - 1] = '\0';
}

// fn for extract cover img from file audio
//
// returns:
// 0 = success
// -1 = failed, or something happend
int extract_cover(PlayBackContext *ctx)
{
  run_command("mkdir -p /tmp/tomu_cover_img 2>/dev/null");

  char *output_path = format("/tmp/tomu_cover_img/%s.jpg", ctx->state.metadata.title);

  for (unsigned int i = 0; i < ctx->fmtCTX->nb_streams; i++) {
    AVStream *stream = ctx->fmtCTX->streams[i];
    if (!stream) continue;


    for (unsigned int i = 0; i < ctx->fmtCTX->nb_streams; i++) {
        AVStream *stream = ctx->fmtCTX->streams[i];

        if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            AVPacket pkt = stream->attached_pic;

            FILE *f = fopen(output_path, "wb");
            if (!f) {
                printf("Could not create output file: %s\n", output_path);
                return 1;
            }

            fwrite(pkt.data, 1, pkt.size, f);
            fclose(f);

            printf("Cover saved: %s\n", output_path);

            strncpy(ctx->state.metadata.cover_path,
                    output_path,
                    sizeof(ctx->state.metadata.cover_path) - 1);

            return 0;
        }
    }
  }

  return -1;
}
