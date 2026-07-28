// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <pthread.h>
// #include <curl/curl.h>
// #include <libavformat/avformat.h>
// #include <libavcodec/avcodec.h>
// #include <unistd.h>
// #include <sys/stat.h>
// #include <time.h>
//
// #include "player_utils.h"
// #include "stream.h"
// #include "errors.h"
// #include "macros.h"
// #include "structs.h"
// #include "playlist.h"
//
// // fn for protect the command from edit for get request
// static char *shell_quote(const char *s) {
//   size_t len = strlen(s);
//   char *out = malloc(len * 2 + 4);
//   if (!out) return NULL;
//     char *p = out;
//     *p++ = '\''; // first '
//
//     for (size_t i = 0; i < len; i++) {
//         if (s[i] == '\'') {
//             *p++ = '\'';
//             *p++ = '\\';
//             *p++ = '\'';
//             *p++ = '\'';
//         } else {
//             *p++ = s[i];
//         }
//     }
//
//     *p++ = '\''; // end '
//     *p = '\0';
//   return out;
// }
//
// int avio_read_packet(void *opaque, uint8_t *buf, int want) {
//   StreamBuf *sb = (StreamBuf *)opaque;
//   pthread_mutex_lock(&sb->lock);
//
//   while (sb->read_pos >= sb->size && !sb->done) {
//     if (sb->error) {
//       pthread_mutex_unlock(&sb->lock);
//       return AVERROR(EIO);
//     }
//     pthread_cond_wait(&sb->more_data, &sb->lock);
//   }
//
//   size_t available = sb->size - sb->read_pos;
//   if (available == 0) {
//     pthread_mutex_unlock(&sb->lock);
//     return AVERROR_EOF;
//   }
//
//   size_t give = (available < (size_t)want) ? available : (size_t)want;
//   memcpy(buf, sb->data + sb->read_pos, give);
//   sb->read_pos += give;
//
//   pthread_mutex_unlock(&sb->lock);
//   return (int)give;
// }
//
// int64_t avio_seek_packet(void *opaque, int64_t offset, int whence) {
//   StreamBuf *sb = (StreamBuf *)opaque;
//   pthread_mutex_lock(&sb->lock);
//
//   if (whence == AVSEEK_SIZE || whence == SEEK_END) {
//     pthread_mutex_unlock(&sb->lock);
//     return -1;
//   }
//
//   int64_t new_pos;
//   if (whence == SEEK_SET)
//     new_pos = offset;
//   else if (whence == SEEK_CUR)
//     new_pos = (int64_t)sb->read_pos + offset;
//   else {
//     pthread_mutex_unlock(&sb->lock);
//     return -1;
//   }
//
//   if (new_pos < 0) new_pos = 0;
//
//   while ((size_t)new_pos > sb->size && !sb->done) {
//     if (sb->error) {
//       pthread_mutex_unlock(&sb->lock);
//       return -1;
//     }
//     pthread_cond_wait(&sb->more_data, &sb->lock);
//   }
//
//   sb->read_pos = (size_t)new_pos;
//   pthread_mutex_unlock(&sb->lock);
//   return new_pos;
// }
//
// static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
//   StreamBuf *sb = (StreamBuf *)userdata;
//   size_t bytes = size * nmemb;
//
//   pthread_mutex_lock(&sb->lock);
//
//   if (sb->size + bytes > sb->capacity) {
//     size_t new_capacity = sb->capacity;
//     while (sb->size + bytes > new_capacity)
//       new_capacity *= 2;
//     uint8_t *new_data = realloc(sb->data, new_capacity);
//     if (!new_data) {
//       sb->error = 1;
//       pthread_mutex_unlock(&sb->lock);
//       return 0;
//     }
//     sb->data = new_data;
//     sb->capacity = new_capacity;
//   }
//
//   memcpy(sb->data + sb->size, ptr, bytes);
//   sb->size += bytes;
//
//   pthread_cond_broadcast(&sb->more_data);
//   pthread_mutex_unlock(&sb->lock);
//   return bytes;
// }
//
// typedef struct {
//   char *url;
//   StreamBuf *sb;
// } CurlArgs;
//
// void *curl_thread_fn(void *arg) {
//   CurlArgs *ca = (CurlArgs *)arg;
//   CURL *curl = curl_easy_init();
//
//   if (!curl) {
//     pthread_mutex_lock(&ca->sb->lock);
//     ca->sb->error = 1;
//     ca->sb->done = 1;
//     pthread_cond_broadcast(&ca->sb->more_data);
//     pthread_mutex_unlock(&ca->sb->lock);
//     free(ca);
//     return NULL;
//   }
//
//   curl_easy_setopt(curl, CURLOPT_URL, ca->url);
//   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
//   curl_easy_setopt(curl, CURLOPT_WRITEDATA, ca->sb);
//   curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
//   curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (compatible; tomu/1.0)");
//   curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
//   curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 100L);
//   curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
//   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
//   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
//   curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 16384L);
//
//   CURLcode res = curl_easy_perform(curl);
//
//   pthread_mutex_lock(&ca->sb->lock);
//   if (res != CURLE_OK) {
//     fprintf(stderr, "[curl] error: %s\n", curl_easy_strerror(res));
//     ca->sb->error = 1;
//   }
//   ca->sb->done = 1;
//   pthread_cond_broadcast(&ca->sb->more_data);
//   pthread_mutex_unlock(&ca->sb->lock);
//
//   curl_easy_cleanup(curl);
//   free(ca->url);
//   free(ca);
//   return NULL;
// }
//
// int extract_playlist_url(PlayBackContext *ctx, const char *url)
// {
//   char *quoted = shell_quote(url);
//   if (!quoted) return -1;
//   char cmd[4096];
//   snprintf(cmd, sizeof(cmd),
//     "yt-dlp --flat-playlist --print url %s 2>/dev/null", quoted);
//   free(quoted);
//
//   FILE *fp = popen(cmd, "r");
//   if (!fp) {
//     return warn("tomu: popen failed\n");
//   }
//
//   int count = 0;
//   char line[256];
//   while (fgets(line, sizeof(line), fp)) {
//     size_t len = strlen(line);
//     while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
//       line[--len] = '\0';
//     if (len == 0) continue;
//
//     printf("[playlist] +%d: %s\n", count + 1, line);
//     src_handle(ctx, line);
//     count++;
//   }
//   pclose(fp);
//
//   printf("[playlist] Added %d tracks\n", count);
//   return count;
// }
//
// char *resolve_url(const char *url) {
//   const char *is_direct[] = {
//     ".mp3", ".ogg", ".opus", ".m4a",
//     ".flac", ".wav", ".webm", "googlevideo.com",
//     "cf-hls", "sndcdn.com/media"
//   };
//
//   for_each_arr(is_direct) {
//     if (strstr(url, is_direct[i])) {
//       printf("[url] direct audio URL detected\n");
//       return strdup(url);
//     }
//   }
//
//   char *quoted = shell_quote(url);
//   if (!quoted) return strdup(url);
//   char cmd[4096];
//   snprintf(cmd, sizeof(cmd),
//     "yt-dlp -f bestaudio/best --no-playlist -g %s 2>/dev/null | head -1", quoted);
//   free(quoted);
//
//   FILE *fp = popen(cmd, "r");
//   if (!fp) {
//     fprintf(stderr, "[yt-dlp] popen failed\n");
//     return strdup(url);
//   }
//   printf("[yt-dlp] Running: %s\n", cmd);
//
//   char resolved[8192] = {0};
//   if (!fgets(resolved, sizeof(resolved), fp)) {
//     fprintf(stderr, "[yt-dlp] no output - is yt-dlp installed?\n");
//     pclose(fp);
//     return strdup(url);
//   }
//   pclose(fp);
//
//   size_t len = strlen(resolved);
//   while (len > 0 && (resolved[len-1] == '\n' || resolved[len-1] == '\r'))
//     resolved[--len] = '\0';
//
//   if (len == 0) {
//     fprintf(stderr, "[yt-dlp] empty result\n");
//     return strdup(url);
//   }
//
//   printf("[yt-dlp] resolved -> %s\n", resolved);
//   return strdup(resolved);
// }
//
// void fetch_url_metadata(const char *url, Audio_Metadata *m) {
//   char *quoted = shell_quote(url);
//   if (!quoted) return;
//   char cmd[4096];
//   char line[512];
//
//   // title
//   snprintf(cmd, sizeof(cmd),
//     "yt-dlp --print title --no-playlist %s 2>/dev/null | head -1", quoted);
//   FILE *fp = popen(cmd, "r");
//   if (fp) {
//     if (fgets(line, sizeof(line), fp)) {
//       size_t len = strlen(line);
//       while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
//       if (len > 0 && strcmp(line, "NA") != 0)
//         strncpy(m->title, line, sizeof(m->title) - 1);
//     }
//     pclose(fp);
//   }
//
//   // artist / uploader
//   snprintf(cmd, sizeof(cmd),
//     "yt-dlp --print uploader --no-playlist %s 2>/dev/null | head -1", quoted);
//   fp = popen(cmd, "r");
//   if (fp) {
//     if (fgets(line, sizeof(line), fp)) {
//       size_t len = strlen(line);
//       while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
//       if (len > 0 && strcmp(line, "NA") != 0)
//         strncpy(m->artist, line, sizeof(m->artist) - 1);
//     }
//     pclose(fp);
//   }
//
//   // thumbnail URL -> store directly, no download
//   snprintf(cmd, sizeof(cmd),
//     "yt-dlp --print thumbnail --no-playlist %s 2>/dev/null | head -1", quoted);
//   fp = popen(cmd, "r");
//   if (fp) {
//     if (fgets(line, sizeof(line), fp)) {
//       size_t len = strlen(line);
//       while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
//       if (len > 0 && strcmp(line, "NA") != 0)
//         strncpy(m->cover_path, line, sizeof(m->cover_path) - 1);
//     }
//     pclose(fp);
//   }
//
//   free(quoted);
//
//   m->title[sizeof(m->title) - 1] = '\0';
//   m->artist[sizeof(m->artist) - 1] = '\0';
//   m->cover_path[sizeof(m->cover_path) - 1] = '\0';
// }
//
// void streaming_cleanup(StreamContext *stream) {
//   if (!stream) return;
//
//   if (stream->stream_buf) {
//     StreamBuf *sb = (StreamBuf *)stream->stream_buf;
//     if (sb->data) free(sb->data);
//     pthread_mutex_destroy(&sb->lock);
//     pthread_cond_destroy(&sb->more_data);
//     free(sb);
//     stream->stream_buf = NULL;
//   }
//
//   if (stream->stream_url) {
//     free(stream->stream_url);
//     stream->stream_url = NULL;
//   }
//
//   stream->is_streaming = 0;
// }
//
// int streaming_start(PlayBackContext *ctx, const char *url) {
//   streaming_cleanup(&ctx->stream_ctx);   // clean up any prior stream first
//
//   char *stream_url = resolve_url(url);
//   if (!stream_url) return -1;
//
//   StreamBuf *sb = malloc(sizeof(StreamBuf));
//   if (!sb) { free(stream_url); return -1; }
//   memset(sb, 0, sizeof(StreamBuf));
//   ctx->stream_ctx.stream_buf = sb;
//
//   sb->data = malloc(1024 * 1024); // allocate 1mb storage
//   if (!sb->data) { free(sb); free(stream_url); return -1; }
//   // exit(0);
//
//   sb->capacity = 1024 * 1024; // 1mb storage
//   sb->size = 0; // current 0 bytes
//   sb->read_pos = 0; // we're reading now
//   sb->done = 0; // tell me Not done downloading
//   sb->error = 0; // No error yet
//   pthread_mutex_init(&sb->lock, NULL);
//   pthread_cond_init(&sb->more_data, NULL);
//
//   // ctx->stream_ctx.stream_buf = sb;
//   ctx->stream_ctx.stream_url = stream_url;
//   ctx->stream_ctx.is_streaming = 1;
//
//   CurlArgs *ca = malloc(sizeof(CurlArgs));
//   if (!ca) { streaming_cleanup(&ctx->stream_ctx); return -1; }
//
//   ca->url = stream_url;
//   ca->sb = sb;
//
//   pthread_create(&ctx->stream_ctx.stream_thread, NULL, curl_thread_fn, ca);
//   pthread_detach(ctx->stream_ctx.stream_thread);
//
//   return 0;
// }
//
// void streaming_stop(PlayBackContext *ctx) {
//   if (!ctx || !ctx->stream_ctx.is_streaming) return;
//
//   if (ctx->stream_ctx.stream_buf) {
//     StreamBuf *sb = (StreamBuf *)ctx->stream_ctx.stream_buf;
//     pthread_mutex_lock(&sb->lock);
//     sb->done = 1;
//     pthread_cond_broadcast(&sb->more_data);
//     pthread_mutex_unlock(&sb->lock);
//   }
//
//   streaming_cleanup(&ctx->stream_ctx);
//   printf("[stream] Stopped streaming\n");
// }
//
// int streaming_is_active(PlayBackContext *ctx) {
//   return ctx->stream_ctx.is_streaming;
// }
//
// int streaming_init_playback(PlayBackContext *ctx, const char *url) {
//   memset(&ctx->stream_ctx, 0, sizeof(StreamContext)); // reinit
//
//   if (streaming_start(ctx, url) < 0) {
//     fprintf(stderr, "Failed to start streaming\n");
//     return -1;
//   }
//
//   StreamBuf *sb = ctx->stream_ctx.stream_buf;
//
//   fetch_url_metadata(url, &ctx->state.metadata);
//
//   pthread_mutex_lock(&sb->lock);
//   int timeout = 300;
//   while (sb->size < 64 * 1024 && !sb->done && timeout > 0) {
//     printf("Buffering... %zu / %d bytes\n", sb->size, 64 * 1024);
//     struct timespec ts;
//     clock_gettime(CLOCK_REALTIME, &ts);
//     ts.tv_sec += 1;
//     pthread_cond_timedwait(&sb->more_data, &sb->lock, &ts);
//     timeout--;
//   }
//
//   if (sb->error) {
//     pthread_mutex_unlock(&sb->lock);
//     streaming_stop(ctx);
//     return -1;
//   }
//
//   if (sb->size == 0 && sb->done) {
//     pthread_mutex_unlock(&sb->lock);
//     fprintf(stderr, "No data received from URL\n");
//     streaming_stop(ctx);
//     return -1;
//   }
//   pthread_mutex_unlock(&sb->lock);
//
//   uint8_t *avio_buf = (uint8_t *)av_malloc(64 * 1024);
//   if (!avio_buf) { streaming_stop(ctx); return -1; }
//
//   AVIOContext *avio = avio_alloc_context(
//     avio_buf, 64 * 1024, 0, sb,
//     avio_read_packet, NULL, avio_seek_packet
//   );
//
//   if (!avio) { av_free(avio_buf); streaming_stop(ctx); return -1; }
//
//   ctx->stream_ctx.avio = avio; // NEW: store so streaming_stop() can free it
//
//   ctx->fmctx->= avformat_alloc_context();
//   if (!ctx->fmctx-> {
//     streaming_stop(ctx); // now frees avio too
//     return -1;
//   }
//
//   ctx->fmctx->>pb = avio;
//   ctx->fmctx->>flags |= AVFMT_FLAG_CUSTOM_IO;
//   ctx->fmctx->>probesize = 64 * 1024;
//   ctx->fmctx->>max_analyze_duration = 3 * AV_TIME_BASE;
//
//   if (avformat_open_input(&ctx->fmctx-> NULL, NULL, NULL) < 0) {
//     ctx->fmctx->= NULL; // avformat_open_input already freed it on failure
//     streaming_stop(ctx); // frees avio via stream_ctx.avio
//     return -1;
//   }
//
//   // ---- find stream info + open codec (mirrors get_audio_info() for local files) ----
//   if (avformat_find_stream_info(ctx->fmctx-> NULL) < 0) {
//     fprintf(stderr, "[stream] Failed to find stream info\n");
//     streaming_stop(ctx);
//     return -1;
//   }
//
//   int stream_idx = av_find_best_stream(ctx->fmctx-> AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
//   if (stream_idx < 0) {
//     fprintf(stderr, "[stream] No audio stream found\n");
//     streaming_stop(ctx);
//     return -1;
//   }
//
//   AVStream *st = ctx->fmctx->>streams[stream_idx];
//   const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
//   if (!codec) {
//     fprintf(stderr, "[stream] Unsupported codec\n");
//     streaming_stop(ctx);
//     return -1;
//   }
//
//   ctx->decoderCTX = avcodec_alloc_context3(codec);
//   if (!ctx->decoderCTX) {
//     fprintf(stderr, "[stream] Failed to alloc codec context\n");
//     streaming_stop(ctx);
//     return -1;
//   }
//
//   if (avcodec_parameters_to_context(ctx->decoderCTX, st->codecpar) < 0) {
//     fprintf(stderr, "[stream] Failed to copy codec params\n");
//     streaming_stop(ctx);
//     return -1;
//   }
//
//   if (avcodec_open2(ctx->decoderCTX, codec, NULL) < 0) {
//     fprintf(stderr, "[stream] Failed to open codec\n");
//     streaming_stop(ctx);
//     return -1;
//   }
//
//   ctx->inf.audioStream_index = stream_idx;
//     ctx->inf.audioStream = st;
//     ctx->inf.sample_rate = ctx->decoderCTX->sample_rate;
//
//     enum AVSampleFormat sample_fmt = ctx->decoderCTX->sample_fmt;
//     if (av_sample_fmt_is_planar(sample_fmt))
//       sample_fmt = get_interleaved(sample_fmt);
//
//     ctx->inf.sample_fmt = sample_fmt;
//     ctx->inf.sample_fmt_bytes = av_get_bytes_per_sample(sample_fmt);
//     ctx->inf.ma_fmt = get_ma_format(sample_fmt);   // <-- ADDED
//
// #ifdef LEGACY_LIBSWRSAMPLE
//     ctx->inf.ch = ctx->decoderCTX->channels;
//     ctx->inf.ch_layout = ctx->decoderCTX->channel_layout;
// #else
//     ctx->inf.ch = ctx->decoderCTX->ch_layout.nb_channels;
//     av_channel_layout_copy(&ctx->inf.ch_layout, &ctx->decoderCTX->ch_layout);
// #endif
//   // ---- end new code ----
//
//   return 0;
// }
//
//
// // first init global for curl
// void init_curl()
// {
//   curl_global_init(CURL_GLOBAL_ALL);
// }
