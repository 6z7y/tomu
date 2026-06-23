/*
 * mpris.c — tomu's MPRIS service.
 */

#include <dbus/dbus.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>

#include "DATA.h"
#include "mpris.h"
#include "backend.h"

#define BUS_NAME    "org.mpris.MediaPlayer2.tomu"
#define OBJ_PATH    "/org/mpris/MediaPlayer2"
#define IFACE_ROOT  "org.mpris.MediaPlayer2"
#define IFACE_PLAYER "org.mpris.MediaPlayer2.Player"
#define IFACE_PROPS "org.freedesktop.DBus.Properties"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DATA.h"
#include "control.h"
#include "../shared/share_utils1.h"

void *start_playback_thread(void *arg) {
    char *path = (char*)arg;
    playback_run(path, 0, 1);
    free(path);

    ctx.playback_active = 0;

    // respect skip_to_next direction (next/prev, now set by mpris.c)
    int skip = ctx.state.skip_to_next;
    ctx.state.skip_to_next = 0;

    if (skip == -1) {
        // Previous track
        if (ctx.list.queue_index > 0) {
            ctx.list.queue_index--;
        } else {
            // At beginning of queue, loop if looping is enabled
            if (ctx.state.looping && ctx.list.queue_count > 0) {
                ctx.list.queue_index = ctx.list.queue_count - 1;
            } else {
                // No previous track, stay at 0 and stop
                ctx.list.queue_index = 0;
                return NULL;
            }
        }
    } else {
        // Next track (or normal finish)
        if (ctx.list.queue_index < ctx.list.queue_count - 1) {
            ctx.list.queue_index++;
        } else {
            // At end of queue, loop if looping is enabled
            if (ctx.state.looping && ctx.list.queue_count > 0) {
                ctx.list.queue_index = 0;
            } else {
                // No more tracks, stop playing
                ctx.state.running = 0;
                ctx.list.queue_index = ctx.list.queue_count - 1; // stay at last
                return NULL;
            }
        }
    }

    // Start the next track if we have a valid index
    if (ctx.list.queue_index >= 0 && ctx.list.queue_index < ctx.list.queue_count) {
        char *next = strdup(ctx.list.queue_list[ctx.list.queue_index]);
        pthread_t t;
        pthread_create(&t, NULL, start_playback_thread, next);
        pthread_detach(t);
        ctx.playback_thread = t;
        ctx.playback_active = 1;
    }
    
    return NULL;
}

void start_playback(char *path) {
    char *copy = strdup(path);
    
    // Check if this path is already in the queue
    int exists = 0;
    for (int i = 0; i < ctx.list.queue_count; i++) {
        if (strcmp(ctx.list.queue_list[i], path) == 0) {
            exists = 1;
            ctx.list.queue_index = i;
            break;
        }
    }
    
    if (!exists) {
        // Add new file to queue
        ctx.list.queue_list = realloc(ctx.list.queue_list, sizeof(char *) * (ctx.list.queue_count + 1));
        ctx.list.queue_list[ctx.list.queue_count] = strdup(path);
        ctx.list.queue_index = ctx.list.queue_count;
        ctx.list.queue_count++;
    }
    
    // Reset state for new playback
    ctx.state.running = 1;
    ctx.state.paused = 0;
    ctx.state.position = 0;

    pthread_create(&ctx.playback_thread, NULL, start_playback_thread, copy);
    pthread_detach(ctx.playback_thread);
    ctx.playback_active = 1;
}

static DBusConnection *g_conn = NULL;

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
static void append_metadata(DBusMessageIter *var) {
    DBusMessageIter arr;
    dbus_message_iter_open_container(var, DBUS_TYPE_ARRAY, "{sv}", &arr);
    
    dict_add_path(&arr, "mpris:trackid", "/org/mpris/MediaPlayer2/tomu/track1");
    dict_add_int64(&arr, "mpris:length", (int64_t)ctx.state.duration * 1000000);
    dict_add_string(&arr, "xesam:title", ctx.state.metadata.title);
    dict_add_strarray1(&arr, "xesam:artist", ctx.state.metadata.artist);
    dict_add_string(&arr, "xesam:album", ctx.state.metadata.album);

    if (strlen(ctx.state.metadata.album_artist) > 0)
        dict_add_strarray1(&arr, "xesam:albumArtist", ctx.state.metadata.album_artist);

    if (strlen(ctx.state.metadata.genre) > 0)
        dict_add_strarray1(&arr, "xesam:genre", ctx.state.metadata.genre);

    if (strlen(ctx.state.metadata.track) > 0)
        dict_add_int32(&arr, "xesam:trackNumber", (int32_t)atoi(ctx.state.metadata.track));

    // Cover art URL for mprisence
    if (strlen(ctx.state.metadata.cover_path) > 0) {
        char art_url[512];
        snprintf(art_url, sizeof(art_url), "file://%s", ctx.state.metadata.cover_path);
        dict_add_string(&arr, "mpris:artUrl", art_url);
        printf("Added cover art URL: %s\n", art_url);
    }
    
    dbus_message_iter_close_container(var, &arr);
}

static void append_property(DBusMessageIter *target, const char *iface, const char *prop) {
  (void)iface;
  if      (!strcmp(prop, "PlaybackStatus"))
    var_string(target, ctx.state.paused ? "Paused" : (ctx.state.running ? "Playing" : "Stopped"));
  else if (!strcmp(prop, "LoopStatus"))
    var_string(target, ctx.state.looping ? "Playlist" : "None");
  else if (!strcmp(prop, "Rate"))          var_double(target, ctx.state.speed);
  else if (!strcmp(prop, "MinimumRate"))   var_double(target, 1.0);
  else if (!strcmp(prop, "MaximumRate"))   var_double(target, 1.0);
  else if (!strcmp(prop, "Shuffle"))       var_bool(target, ctx.state.shuffle);
  else if (!strcmp(prop, "Volume"))        var_double(target, ctx.state.volume);
  else if (!strcmp(prop, "Position"))      var_int64(target, (int64_t)ctx.state.position * 1000000);
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

/* ---------- Introspection handler -------------------------------------- */
static void handle_introspect(DBusMessage *msg) {
    const char *introspection_xml = 
        "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
        "<node>\n"
        "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
        "    <method name=\"Introspect\">\n"
        "      <arg name=\"data\" type=\"s\" direction=\"out\"/>\n"
        "    </method>\n"
        "  </interface>\n"
        "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
        "    <method name=\"Get\">\n"
        "      <arg name=\"interface\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"property\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"value\" type=\"v\" direction=\"out\"/>\n"
        "    </method>\n"
        "    <method name=\"GetAll\">\n"
        "      <arg name=\"interface\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"properties\" type=\"a{sv}\" direction=\"out\"/>\n"
        "    </method>\n"
        "    <method name=\"Set\">\n"
        "      <arg name=\"interface\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"property\" type=\"s\" direction=\"in\"/>\n"
        "      <arg name=\"value\" type=\"v\" direction=\"in\"/>\n"
        "    </method>\n"
        "    <signal name=\"PropertiesChanged\">\n"
        "      <arg name=\"interface\" type=\"s\"/>\n"
        "      <arg name=\"changed_properties\" type=\"a{sv}\"/>\n"
        "      <arg name=\"invalidated_properties\" type=\"as\"/>\n"
        "    </signal>\n"
        "  </interface>\n"
        "  <interface name=\"org.mpris.MediaPlayer2\">\n"
        "    <method name=\"Raise\"/>\n"
        "    <method name=\"Quit\"/>\n"
        "    <property name=\"CanQuit\" type=\"b\" access=\"read\"/>\n"
        "    <property name=\"CanRaise\" type=\"b\" access=\"read\"/>\n"
        "    <property name=\"HasTrackList\" type=\"b\" access=\"read\"/>\n"
        "    <property name=\"Identity\" type=\"s\" access=\"read\"/>\n"
        "    <property name=\"DesktopEntry\" type=\"s\" access=\"read\"/>\n"
        "    <property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\"/>\n"
        "    <property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/>\n"
        "  </interface>\n"
        "  <interface name=\"org.mpris.MediaPlayer2.Player\">\n"
        "    <method name=\"Next\"/>\n"
        "    <method name=\"Previous\"/>\n"
        "    <method name=\"Pause\"/>\n"
        "    <method name=\"PlayPause\"/>\n"
        "    <method name=\"Play\"/>\n"
        "    <method name=\"Stop\"/>\n"
        "    <method name=\"Seek\">\n"
        "      <arg name=\"Offset\" type=\"x\" direction=\"in\"/>\n"
        "    </method>\n"
        "    <method name=\"OpenUri\">\n"
        "      <arg name=\"Uri\" type=\"s\" direction=\"in\"/>\n"
        "    </method>\n"
        "    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>\n"
        "    <property name=\"LoopStatus\" type=\"s\" access=\"readwrite\"/>\n"
        "    <property name=\"Rate\" type=\"d\" access=\"readwrite\"/>\n"
        "    <property name=\"Shuffle\" type=\"b\" access=\"readwrite\"/>\n"
        "    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>\n"
        "    <property name=\"Volume\" type=\"d\" access=\"readwrite\"/>\n"
        "    <property name=\"Position\" type=\"x\" access=\"read\"/>\n"
        "    <property name=\"MinimumRate\" type=\"d\" access=\"read\"/>\n"
        "    <property name=\"MaximumRate\" type=\"d\" access=\"read\"/>\n"
        "    <property name=\"CanGoNext\" type=\"b\" access=\"read\"/>\n"
        "    <property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>\n"
        "    <property name=\"CanPlay\" type=\"b\" access=\"read\"/>\n"
        "    <property name=\"CanPause\" type=\"b\" access=\"read\"/>\n"
        "    <property name=\"CanSeek\" type=\"b\" access=\"read\"/>\n"
        "    <property name=\"CanControl\" type=\"b\" access=\"read\"/>\n"
        "  </interface>\n"
        "</node>";

    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &introspection_xml);
    dbus_connection_send(g_conn, reply, NULL);
    dbus_message_unref(reply);
}

/* ---------- Properties.Get / GetAll / Set ------------------------------ */
static void reply_get(DBusMessage *msg, const char *iface, const char *prop) {
  DBusMessage *reply = dbus_message_new_method_return(msg);
  DBusMessageIter iter;
  dbus_message_iter_init_append(reply, &iter);
  append_property(&iter, iface, prop);
  dbus_connection_send(g_conn, reply, NULL);
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
  dbus_connection_send(g_conn, reply, NULL);
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
    double v; dbus_message_iter_get_basic(&var, &v); ctx.state.volume = v;
  } else if (!strcmp(prop, "Rate")) {
    double v; dbus_message_iter_get_basic(&var, &v); ctx.state.speed = v;
  } else if (!strcmp(prop, "Shuffle")) {
    dbus_bool_t v; dbus_message_iter_get_basic(&var, &v); ctx.state.shuffle = v;
  } else if (!strcmp(prop, "LoopStatus")) {
    const char *v; dbus_message_iter_get_basic(&var, &v);
    ctx.state.looping = strcmp(v, "None") != 0;
  }

  DBusMessage *reply = dbus_message_new_method_return(msg);
  dbus_connection_send(g_conn, reply, NULL);
  dbus_message_unref(reply);
  mpris_notify_change();
}

static void reply_empty(DBusMessage *msg) {
  DBusMessage *reply = dbus_message_new_method_return(msg);
  dbus_connection_send(g_conn, reply, NULL);
  dbus_message_unref(reply);
}

/* ---------- main dispatcher -------------------------------------------- */
static void handle_message(DBusMessage *msg) {
    // Handle Introspect first
    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        handle_introspect(msg);
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
        handle_set(msg);
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "PlayPause")) {
        ctx.state.paused = !ctx.state.paused;
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Play")) {
        ctx.state.paused = 0;
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Pause")) {
        ctx.state.paused = 1;
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Stop")) {
        ctx.state.running = 0;
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Next")) {
        ctx.state.skip_to_next = 1;
        reply_empty(msg);
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Previous")) {
        ctx.state.skip_to_next = -1;
        reply_empty(msg);
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "OpenUri")) {
        const char *uri;
        DBusError err;
        dbus_error_init(&err);
        
        if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &uri, DBUS_TYPE_INVALID)) {
            char *path;
            if (strncmp(uri, "file://", 7) == 0) {
                path = strdup(uri + 7);
            } else {
                path = strdup(uri);
            }
            
            // URL-decode the path (convert %20 to spaces, etc.)
            url_decode(path);
            
            printf("OpenUri: %s -> %s\n", uri, path);
            if (!ctx.playback_active) {
                start_playback(path);
            }
            free(path);
            reply_empty(msg);
        } else {
            DBusMessage *error = dbus_message_new_error(msg, 
                "org.freedesktop.DBus.Error.InvalidArgs",
                "Invalid URI argument");
            dbus_connection_send(g_conn, error, NULL);
            dbus_message_unref(error);
            dbus_error_free(&err);
        }
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Seek")) {
        int64_t offset_us;
        dbus_message_get_args(msg, NULL, DBUS_TYPE_INT64, &offset_us, DBUS_TYPE_INVALID);
        ctx.state.position += (int)(offset_us / 1000000);
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

/* ---------- public API ------------------------------------------------- */
void mpris_init(void) {
  DBusError err;
  dbus_error_init(&err);

  g_conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set(&err)) {
    fprintf(stderr, "tomu: dbus connect failed: %s\n", err.message);
    dbus_error_free(&err); exit(1);
  }

  int ret = dbus_bus_request_name(g_conn, BUS_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
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

void mpris_dispatch(void) {
  if (!g_conn) return;
  dbus_connection_read_write(g_conn, 0);
  DBusMessage *msg;
  while ((msg = dbus_connection_pop_message(g_conn)) != NULL) {
    handle_message(msg);
    dbus_message_unref(msg);
  }
}

void mpris_notify_change(void) {
  if (!g_conn) return;
  DBusMessage *signal = dbus_message_new_signal(OBJ_PATH, IFACE_PROPS, "PropertiesChanged");
  DBusMessageIter iter, changed, invalid;
  dbus_message_iter_init_append(signal, &iter);
  const char *iface = IFACE_PLAYER;
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &changed);
  const char *push[] = { "PlaybackStatus", "Metadata", "Volume", NULL };
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
  dbus_connection_send(g_conn, signal, NULL);
  dbus_message_unref(signal);
}
