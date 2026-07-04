#include <fcntl.h>
#include <libavformat/avformat.h>
#include <pthread.h>
#include <dbus/dbus.h>
#include <curl/curl.h>
#include <libavcodec/codec.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "macros.h"
#include "structs.h"
#include "audio_backend.h"
#include "backend.h"
#include "errors.h"
#include "mpris.h"
#include "streaming.h"


// build string and return it
char *format(const char *fmt, ...)
{
  // read args
  static char buf[256];
  va_list args;
  va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  return buf;
}

// get rand num from linux kernel
unsigned int get_rand()
{
  // 1. init varibles
  int fd;
  unsigned int n; // number without negative

  // 2. open file
  if ((fd = open("/dev/urandom", O_RDONLY)) < 0) die("can't get rand num");

  // 3. read stream
  if (read(fd, &n, sizeof(n)) != sizeof(n)) die("rand:");

  // 4. close it and return it
  close(fd);
  return n;
}

// function run command
void run_command(char *cmd)
{
  FILE *p;

  if (!(p = popen(cmd, "r"))) { 
    warn("cmd:");
  }

  pclose(p);
}

// clean playback ctx
void cleanUP(){
  // First, clean up streaming if active
  if (tctx.stream_ctx.is_streaming) {
    streaming_cleanup(&tctx.stream_ctx);
  }
  
  // Then clean up FFmpeg contexts
  if (tctx.fmtCTX) {
    avformat_close_input(&tctx.fmtCTX);
    tctx.fmtCTX = NULL;
  }
  if (tctx.codecCTX) {
    avcodec_free_context(&tctx.codecCTX);
    tctx.codecCTX = NULL;
  }
}

void signal_handle(int sig)
{
  (void)sig;

  printf("exit..\n");
  // Clean up streaming if active
  if (tctx.stream_ctx.is_streaming) {
    streaming_stop(&tctx);
  }
  for (int i = 0; i < tctx.list.queue_count; i++) free(tctx.list.queue_lists[i]);
  free(tctx.list.queue_lists);
  curl_global_cleanup();

  cleanUP();
  _exit(0);
}

void *playback_queue_thread(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&tctx.list.lock);
        while (tctx.list.queue_index >= tctx.list.queue_count) {

            pthread_cond_wait(&tctx.list.item_ready, &tctx.list.lock);
        }
        if (tctx.state.shuffle) {
          int idx = get_rand() % tctx.list.queue_count;
          strdup(tctx.list.queue_lists[idx]);

        }
        char *path = strdup(tctx.list.queue_lists[tctx.list.queue_index]);
        pthread_mutex_unlock(&tctx.list.lock);

        // FIX: Clear metadata BEFORE getting new metadata
        memset(&tctx.state.metadata, 0, sizeof(Audio_Metadata));
        
        if (IS_URL_RAW(path)) {
            get_metadata_from_url(path, &tctx.state.metadata);
            char *resolved = resolve_url(path);
            if (resolved && strcmp(resolved, path) != 0) {
                free(path);
                path = resolved;
            }
        }

        playback_run(path, tctx.state.loop, 1);
        free(path);

        tctx.playback_active = 0;

        // check skip_to_next AFTER playback_run returns
        pthread_mutex_lock(&tctx.list.lock);
        if (tctx.state.skip_to_next == 1) {
          if (tctx.buf) {
              audio_buffer_destroy(tctx.buf);
              tctx.buf = NULL;
          }

          if (tctx.state.loop == LOOP_PLAYLIST){
            tctx.list.queue_index = (tctx.list.queue_index + 1) % tctx.list.queue_count;
          }
          else 
            tctx.list.queue_index++;

        } else if (tctx.state.skip_to_next == -1) {

          if (tctx.buf) {
            audio_buffer_destroy(tctx.buf);
            tctx.buf = NULL;
          }

          if (tctx.state.loop == LOOP_PLAYLIST)
            tctx.list.queue_index = (tctx.list.queue_index - 1 + tctx.list.queue_count) % tctx.list.queue_count;
          else  {
            if (tctx.list.queue_index > 0)
              tctx.list.queue_index--;
          }

        } else {

          if (tctx.state.loop == LOOP_PLAYLIST) {
            printf("jhhh\n");
            if (!tctx.state.running)
              tctx.list.queue_index = (tctx.list.queue_index + 1) % tctx.list.queue_count;
            printf("222\n");
          }

          else 
            tctx.list.queue_index++;
        }
        tctx.state.skip_to_next = 0; // reset
        pthread_mutex_unlock(&tctx.list.lock);
    }
    return NULL;
}

// init mpris connection
void mpris_init(void) {
  DBusError err;
  dbus_error_init(&err);

  // 1. connect to dbus session
  tctx.dbus_s.conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  dbus_err_handle(&err, format("dbus connect failed: %s\n", &err.message));

  // 2. change name dbus session for support mpris
  int ret = dbus_bus_request_name(tctx.dbus_s.conn, BUS_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
  dbus_err_handle(&err, format("dbus name request failed: %s\n", err.message));

  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) die("tomu: already running\n");
}

void first_init()
{
  mpris_init();
    curl_global_init(CURL_GLOBAL_ALL);
    tctx.list.filter_files = 1;

    pthread_mutex_init(&tctx.list.lock, NULL);
    pthread_cond_init(&tctx.list.item_ready, NULL);

    pthread_t *pt = &tctx.list.pt;
    pthread_create(pt, NULL, playback_queue_thread, NULL);
    pthread_detach(*pt);
}
