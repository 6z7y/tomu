#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <dbus/dbus.h>

#include "mpris.h"
#include "backend.h"
#include "DATA.h"
#include "control.h"
#include "streaming.h"  // Add this include
#include "../shared/share_utils1.h"

void *start_playback_thread(void *arg) {
    char *path = (char*)arg;
    
    // Check if it's a URL that needs resolution
    int is_url_path = (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0);
    
    // FIRST: Get metadata from URL (before streaming starts)
    if (is_url_path) {
        printf("[playback] Getting metadata from URL...\n");
        get_metadata_from_url(path, &tctx.state.metadata);
        
        // Now resolve URL for streaming
        char *resolved = resolve_url(path);
        if (resolved && strcmp(resolved, path) != 0) {
            printf("[playback] Resolved URL: %s -> %s\n", path, resolved);
            free(path);
            path = resolved;
        }
    }
    
    // Now play the audio (local file or resolved URL)
    playback_run(path, 0, 1);
    free(path);
    
    // ... rest of the function ...
}

void start_playback(char *path) {
    char *copy = strdup(path);
    
    // Check if this path is already in the queue (only for local files)
    int exists = 0;
    int is_url_path = (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0);
    
    if (!is_url_path) {
        for (int i = 0; i < tctx.list.queue_count; i++) {
            if (strcmp(tctx.list.queue_lists[i], path) == 0) {
                exists = 1;
                tctx.list.queue_index = i;
                break;
            }
        }
    }
    
    if (!exists) {
        // Add new item to queue
        tctx.list.queue_lists = realloc(tctx.list.queue_lists, sizeof(char *) * (tctx.list.queue_count + 1));
        tctx.list.queue_lists[tctx.list.queue_count] = strdup(path);
        tctx.list.queue_index = tctx.list.queue_count;
        tctx.list.queue_count++;
    }
    
    // Reset state for new playback
    tctx.state.running = 1;
    tctx.state.paused = 0;
    tctx.state.position = 0;

    // Start playback thread
    pthread_create(&tctx.playback_thread, NULL, start_playback_thread, copy);
    pthread_detach(tctx.playback_thread);
    tctx.playback_active = 1;
}

void mpris_init(void) {
  DBusError err; // init the error object
  dbus_error_init(&err);

  // 1. connect to the bus session
  tctx.dbus_s.conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set(&err)) {
    fprintf(stderr, "tomu: dbus connect failed: %s\n", err.message);
    dbus_error_free(&err); exit(1);
  }

  // change name dbus program
  int ret = dbus_bus_request_name(tctx.dbus_s.conn, BUS_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
  if (dbus_error_is_set(&err)) {
    fprintf(stderr, "tomu: dbus name request failed: %s\n", err.message);
    dbus_error_free(&err); exit(1);
  }
  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    fprintf(stderr, "tomu: already running (another MPRIS owner exists)\n");
    exit(1);
  }

  printf("tomu mpris ready: %s\n", BUS_NAME);
}

/* URL decode a string in-place */
static void url_decode(char *str) {
    char *p = str;
    char *q = str;
    
    while (*p) {
        if (*p == '%' && isxdigit(p[1]) && isxdigit(p[2])) {
            char hex[3] = { p[1], p[2], '\0' };
            *q = (char)strtol(hex, NULL, 16);
            p += 3;
            q++;
        } else if (*p == '+') {
            *q = ' ';
            p++;
            q++;
        } else {
            *q = *p;
            p++;
            q++;
        }
    }
    *q = '\0';
}

/* ---------- tiny helpers ----------------------------------------------- */
static void var_string(DBusMessageIter *target, const char *v) {
  DBusMessageIter var;
  dbus_message_iter_open_container(target, DBUS_TYPE_VARIANT, "s", &var);
  dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
  dbus_message_iter_close_container(target, &var);
}
static void var_bool(DBusMessageIter *target, int v) {
  DBusMessageIter var; dbus_bool_t b = v ? TRUE : FALSE;
  dbus_message_iter_open_container(target, DBUS_TYPE_VARIANT, "b", &var);
  dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &b);
  dbus_message_iter_close_container(target, &var);
}
static void var_double(DBusMessageIter *target, double v) {
  DBusMessageIter var;
  dbus_message_iter_open_container(target, DBUS_TYPE_VARIANT, "d", &var);
  dbus_message_iter_append_basic(&var, DBUS_TYPE_DOUBLE, &v);
  dbus_message_iter_close_container(target, &var);
}
static void var_int64(DBusMessageIter *target, int64_t v) {
  DBusMessageIter var;
  dbus_message_iter_open_container(target, DBUS_TYPE_VARIANT, "x", &var);
  dbus_message_iter_append_basic(&var, DBUS_TYPE_INT64, &v);
  dbus_message_iter_close_container(target, &var);
}
static void var_emptystrarray(DBusMessageIter *target) {
  DBusMessageIter var, arr;
  dbus_message_iter_open_container(target, DBUS_TYPE_VARIANT, "as", &var);
  dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "s", &arr);
  dbus_message_iter_close_container(&var, &arr);
  dbus_message_iter_close_container(target, &var);
}

/* ---------- dict-entry helpers ----------------------------------------- */
static void dict_add_string(DBusMessageIter *arr, const char *key, const char *val) {
  DBusMessageIter entry, var;
  dbus_message_iter_open_container(arr, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &var);
  dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &val);
  dbus_message_iter_close_container(&entry, &var);
  dbus_message_iter_close_container(arr, &entry);
}
static void dict_add_strarray1(DBusMessageIter *arr, const char *key, const char *val) {
  DBusMessageIter entry, var, astr;
  dbus_message_iter_open_container(arr, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "as", &var);
  dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "s", &astr);
  dbus_message_iter_append_basic(&astr, DBUS_TYPE_STRING, &val);
  dbus_message_iter_close_container(&var, &astr);
  dbus_message_iter_close_container(&entry, &var);
  dbus_message_iter_close_container(arr, &entry);
}
static void dict_add_int64(DBusMessageIter *arr, const char *key, int64_t val) {
  DBusMessageIter entry, var;
  dbus_message_iter_open_container(arr, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "x", &var);
  dbus_message_iter_append_basic(&var, DBUS_TYPE_INT64, &val);
  dbus_message_iter_close_container(&entry, &var);
  dbus_message_iter_close_container(arr, &entry);
}
static void dict_add_int32(DBusMessageIter *arr, const char *key, int32_t val) {
  DBusMessageIter entry, var;
  dbus_message_iter_open_container(arr, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "i", &var);
  dbus_message_iter_append_basic(&var, DBUS_TYPE_INT32, &val);
  dbus_message_iter_close_container(&entry, &var);
  dbus_message_iter_close_container(arr, &entry);
}
static void dict_add_path(DBusMessageIter *arr, const char *key, const char *val) {
  DBusMessageIter entry, var;
  dbus_message_iter_open_container(arr, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
  dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "o", &var);
  dbus_message_iter_append_basic(&var, DBUS_TYPE_OBJECT_PATH, &val);
  dbus_message_iter_close_container(&entry, &var);
  dbus_message_iter_close_container(arr, &entry);
}

/* ---------- Metadata with cover art support ---------------------------- */
// In append_metadata, the cover path should be set
static void append_metadata(DBusMessageIter *var) {
    DBusMessageIter arr;
    dbus_message_iter_open_container(var, DBUS_TYPE_ARRAY, "{sv}", &arr);
    
    dict_add_path(&arr, "mpris:trackid", "/org/mpris/MediaPlayer2/tomu/track1");
    dict_add_int64(&arr, "mpris:length", (int64_t)tctx.state.duration * 1000000);
    
    // Title
    char title_buf[256];
    if (strlen(tctx.state.metadata.title) > 0) {
        strncpy(title_buf, tctx.state.metadata.title, sizeof(title_buf) - 1);
    } else {
        strncpy(title_buf, "Unknown Track", sizeof(title_buf) - 1);
    }
    dict_add_string(&arr, "xesam:title", title_buf);
    
    // Artist
    if (strlen(tctx.state.metadata.artist) > 0) {
        dict_add_strarray1(&arr, "xesam:artist", tctx.state.metadata.artist);
    }
    
    // Cover art - use the stored cover path from metadata
    char art_url[512];
    if (strlen(tctx.state.metadata.cover_path) > 0) {
        snprintf(art_url, sizeof(art_url), "file://%s", tctx.state.metadata.cover_path);
        printf("[MPRIS] Using cover path: %s\n", art_url);
    } else {
        // Fallback
        snprintf(art_url, sizeof(art_url), "file:///tmp/tomu_cover_img/cover.jpg");
        printf("[MPRIS] Using fallback cover: %s\n", art_url);
    }
    dict_add_string(&arr, "mpris:artUrl", art_url);
    
    // Album
    if (strlen(tctx.state.metadata.album) > 0)
        dict_add_string(&arr, "xesam:album", tctx.state.metadata.album);
    
    if (strlen(tctx.state.metadata.album_artist) > 0)
        dict_add_strarray1(&arr, "xesam:albumArtist", tctx.state.metadata.album_artist);

    if (strlen(tctx.state.metadata.genre) > 0)
        dict_add_strarray1(&arr, "xesam:genre", tctx.state.metadata.genre);

    if (strlen(tctx.state.metadata.track) > 0)
        dict_add_int32(&arr, "xesam:trackNumber", (int32_t)atoi(tctx.state.metadata.track));
    
    dbus_message_iter_close_container(var, &arr);
}

static void append_property(DBusMessageIter *target, const char *iface, const char *prop) {
  (void)iface;
  if      (!strcmp(prop, "PlaybackStatus"))
    var_string(target, tctx.state.paused ? "Paused" : (tctx.state.running ? "Playing" : "Stopped"));
  else if (!strcmp(prop, "LoopStatus"))
    var_string(target, tctx.state.looping ? "Playlist" : "None");
  else if (!strcmp(prop, "Rate"))          var_double(target, tctx.state.speed);
  else if (!strcmp(prop, "MinimumRate"))   var_double(target, 1.0);
  else if (!strcmp(prop, "MaximumRate"))   var_double(target, 1.0);
  else if (!strcmp(prop, "Shuffle"))       var_bool(target, tctx.state.shuffle);
  else if (!strcmp(prop, "Volume"))        var_double(target, tctx.state.volume);
  else if (!strcmp(prop, "Position"))      var_int64(target, (int64_t)tctx.state.position * 1000000);
  else if (!strcmp(prop, "Metadata")) {
    DBusMessageIter var;
    dbus_message_iter_open_container(target, DBUS_TYPE_VARIANT, "a{sv}", &var);
    append_metadata(&var);
    dbus_message_iter_close_container(target, &var);
  }
  else if (!strcmp(prop, "CanGoNext") || !strcmp(prop, "CanGoPrevious") ||
           !strcmp(prop, "CanPlay")   || !strcmp(prop, "CanPause") ||
           !strcmp(prop, "CanSeek")   || !strcmp(prop, "CanControl") ||
           !strcmp(prop, "CanQuit"))      var_bool(target, 1);
  else if (!strcmp(prop, "CanRaise") || !strcmp(prop, "HasTrackList"))
    var_bool(target, 0);
  else if (!strcmp(prop, "Identity") || !strcmp(prop, "DesktopEntry"))
    var_string(target, "tomu");
  else if (!strcmp(prop, "SupportedUriSchemes") || !strcmp(prop, "SupportedMimeTypes"))
    var_emptystrarray(target);
}

/* ---------- Properties.Get / GetAll / Set ------------------------------ */
static void reply_get(DBusMessage *msg, const char *iface, const char *prop) {
  DBusMessage *reply = dbus_message_new_method_return(msg);
  DBusMessageIter iter;
  dbus_message_iter_init_append(reply, &iter);
  append_property(&iter, iface, prop);
  dbus_connection_send(tctx.dbus_s.conn, reply, NULL);
  dbus_message_unref(reply);
}

static const char *PLAYER_PROPS[] = {
  "PlaybackStatus","LoopStatus","Rate","Shuffle","Metadata","Volume","Position",
  "MinimumRate","MaximumRate","CanGoNext","CanGoPrevious","CanPlay","CanPause",
  "CanSeek","CanControl", NULL
};
static const char *ROOT_PROPS[] = {
  "Identity","CanQuit","CanRaise","HasTrackList","DesktopEntry",
  "SupportedUriSchemes","SupportedMimeTypes", NULL
};

static void reply_get_all(DBusMessage *msg, const char *iface) {
  DBusMessage *reply = dbus_message_new_method_return(msg);
  DBusMessageIter iter, arr;
  dbus_message_iter_init_append(reply, &iter);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &arr);

  const char **list = !strcmp(iface, IFACE_PLAYER) ? PLAYER_PROPS : ROOT_PROPS;
  for (int i = 0; list[i]; i++) {
    DBusMessageIter entry;
    dbus_message_iter_open_container(&arr, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &list[i]);
    append_property(&entry, iface, list[i]);
    dbus_message_iter_close_container(&arr, &entry);
  }

  dbus_message_iter_close_container(&iter, &arr);
  dbus_connection_send(tctx.dbus_s.conn, reply, NULL);
  dbus_message_unref(reply);
}

static void handle_set(DBusMessage *msg) {
  DBusMessageIter iter, var;
  const char *iface, *prop;

  dbus_message_iter_init(msg, &iter);
  dbus_message_iter_get_basic(&iter, &iface); dbus_message_iter_next(&iter);
  dbus_message_iter_get_basic(&iter, &prop);  dbus_message_iter_next(&iter);
  dbus_message_iter_recurse(&iter, &var);
  (void)iface;

  if (!strcmp(prop, "Volume")) {
    double v; dbus_message_iter_get_basic(&var, &v); tctx.state.volume = v;
  } else if (!strcmp(prop, "Rate")) {
    double v; dbus_message_iter_get_basic(&var, &v); tctx.state.speed = v;
  } else if (!strcmp(prop, "Shuffle")) {
    dbus_bool_t v; dbus_message_iter_get_basic(&var, &v); tctx.state.shuffle = v;
  } else if (!strcmp(prop, "LoopStatus")) {
    const char *v; dbus_message_iter_get_basic(&var, &v);
    tctx.state.looping = strcmp(v, "None") != 0;
  }

  DBusMessage *reply = dbus_message_new_method_return(msg);
  dbus_connection_send(tctx.dbus_s.conn, reply, NULL);
  dbus_message_unref(reply);
  mpris_notify_change();
}

static void reply_empty(DBusMessage *msg) {
  DBusMessage *reply = dbus_message_new_method_return(msg);
  dbus_connection_send(tctx.dbus_s.conn, reply, NULL);
  dbus_message_unref(reply);
}

/* ---------- main dispatcher -------------------------------------------- */
static void handle_message(DBusMessage *msg) {
    // Handle Introspect first
    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        // handle_introspect(msg);
        return;
    }

    if (dbus_message_is_method_call(msg, IFACE_PROPS, "Get")) {
        const char *iface, *prop;
        dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &iface,
                               DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
        reply_get(msg, iface, prop);
    }
    else if (dbus_message_is_method_call(msg, IFACE_PROPS, "GetAll")) {
        const char *iface;
        dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &iface, DBUS_TYPE_INVALID);
        reply_get_all(msg, iface);
    }
    else if (dbus_message_is_method_call(msg, IFACE_PROPS, "Set")) {
        handle_set(tctx.dbus_s.msg);
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "PlayPause")) {
        playback_toggle(&tctx.state);
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Play")) {
        tctx.state.paused = 0;
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Pause")) {
        tctx.state.paused = 1;
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Stop")) {
        tctx.state.running = 0;
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Next")) {
        tctx.state.skip_to_next = 1;
        reply_empty(msg);
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Previous")) {
        tctx.state.skip_to_next = -1;
        reply_empty(msg);
    }
    // Open handler - now non-blocking
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "open")) {
        const char *uri;
        DBusError err;
        dbus_error_init(&err);
        
        if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &uri, DBUS_TYPE_INVALID)) {
            char *path = strdup(uri);
            
            // Reply immediately so playerctl doesn't timeout
            reply_empty(msg);
            
            printf("OpenUri: %s\n", uri);
            
            // If it's a file:// URL, strip the prefix
            if (!strncmp(uri, "file://", 7)) {
                char *decoded_path = strdup(uri + 7);
                url_decode(decoded_path);
                free(path);
                path = decoded_path;
            }
            
            // Start playback in a separate thread to avoid blocking D-Bus
            if (!tctx.playback_active) {
                start_playback(path);
            }
            free(path);
        } else {
            DBusMessage *error = dbus_message_new_error(msg, 
                "org.freedesktop.DBus.Error.InvalidArgs",
                "Invalid URI argument");
            dbus_connection_send(tctx.dbus_s.conn, error, NULL);
            dbus_message_unref(error);
            dbus_error_free(&err);
        }
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Seek")) {
        int64_t offset_us;
        dbus_message_get_args(msg, NULL, DBUS_TYPE_INT64, &offset_us, DBUS_TYPE_INVALID);
        tctx.state.position += (int)(offset_us / 1000000);
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_ROOT, "Raise")) {
        reply_empty(msg);
    }
    else if (dbus_message_is_method_call(msg, IFACE_ROOT, "Quit")) {
        reply_empty(msg);
        raise(SIGINT);
    }
}

void mpris_dispatch(void) {
  if (!tctx.dbus_s.conn) die("something happend:");
  dbus_connection_read_write(tctx.dbus_s.conn, 0);
  while ((tctx.dbus_s.msg = dbus_connection_pop_message(tctx.dbus_s.conn)) != NULL) {
    handle_message(tctx.dbus_s.msg);
    dbus_message_unref(tctx.dbus_s.msg);
  }
}

void mpris_notify_change(void) {
    if (!tctx.dbus_s.conn) return;
    
    DBusMessage *signal = dbus_message_new_signal(OBJ_PATH, IFACE_PROPS, "PropertiesChanged");
    if (!signal) return;
    
    DBusMessageIter iter, changed, invalid;
    dbus_message_iter_init_append(signal, &iter);
    
    const char *iface = IFACE_PLAYER;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface);
    
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &changed);
    
    const char *push[] = { "PlaybackStatus", "Metadata", "Volume", "Position", NULL };
    for (int i = 0; push[i]; i++) {
        DBusMessageIter entry;
        dbus_message_iter_open_container(&changed, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &push[i]);
        append_property(&entry, IFACE_PLAYER, push[i]);
        dbus_message_iter_close_container(&changed, &entry);
    }
    
    dbus_message_iter_close_container(&iter, &changed);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &invalid);
    dbus_message_iter_close_container(&iter, &invalid);
    
    dbus_connection_send(tctx.dbus_s.conn, signal, NULL);
    dbus_message_unref(signal);
}
