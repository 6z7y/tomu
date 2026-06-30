#include <pthread.h>
#include <dbus/dbus.h>
#include <curl/curl.h>

#include "DATA.h"
#include "audio_backend.h"
#include "backend.h"
#include "mpris.h"
#include "streaming.h"
#include "utils1.h"
#include "utils2.h"

void *playback_queue_thread(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&tctx.list.lock);
        while (tctx.list.queue_index >= tctx.list.queue_count) {
            // int idx = get_rand() % tctx.list.queue_count;

            pthread_cond_wait(&tctx.list.item_ready, &tctx.list.lock);
        }
        if (tctx.state.shuffle) {
          char *path = strdup(tctx.list.queue_lists[tctx.list.queue_index]);

        }
        char *path = strdup(tctx.list.queue_lists[tctx.list.queue_index]);
        pthread_mutex_unlock(&tctx.list.lock);

        int is_url_path = strncmp(path, "http://", 7) == 0 ||
                          strncmp(path, "https://", 8) == 0;

        // FIX: Clear metadata BEFORE getting new metadata
        memset(&tctx.state.metadata, 0, sizeof(Audio_Metadata));
        
        if (is_url_path) {
            get_metadata_from_url(path, &tctx.state.metadata);
            char *resolved = resolve_url(path);
            if (resolved && strcmp(resolved, path) != 0) {
                free(path);
                path = resolved;
            }
        }

        tctx.state.running  = 1;
        tctx.state.paused   = 0;
        tctx.state.position = 0;
        tctx.playback_active = 1;

        playback_run(path, 0, 1);
        free(path);

        tctx.playback_active = 0;

        // check skip_to_next AFTER playback_run returns
        pthread_mutex_lock(&tctx.list.lock);
        if (tctx.state.skip_to_next == 1) {
          if (tctx.buf) {
              audio_buffer_destroy(tctx.buf);
              tctx.buf = NULL;
          }

            tctx.list.queue_index++;
        } else if (tctx.state.skip_to_next == -1) {

            if (tctx.list.queue_index > 0)
              tctx.list.queue_index--;
                if (tctx.buf) {
                  audio_buffer_destroy(tctx.buf);
                  tctx.buf = NULL;
                }
        } else {
            tctx.list.queue_index++;
        }
        tctx.state.skip_to_next = 0; // reset
        pthread_mutex_unlock(&tctx.list.lock);
    }
    return NULL;
}

// Check if argument is a URL
int is_url(const char *arg)
{
  return (!strncmp(arg, "http://", 7)) || (!strncmp(arg, "https://", 8));
}

void first_init()
{
    curl_global_init(CURL_GLOBAL_ALL);
    mpris_init();

    pthread_mutex_init(&tctx.list.lock, NULL);
    pthread_cond_init(&tctx.list.item_ready, NULL);

    pthread_t *pt = &tctx.list.pt;
    pthread_create(pt, NULL, playback_queue_thread, NULL);
    pthread_detach(*pt);
}
