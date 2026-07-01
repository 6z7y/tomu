#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <dbus/dbus.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "mpris.h"
#include "errors.h"
#include "macros.h"
#include "structs.h"
#include "control.h"
#include "file_handle.h"
#include "utils.h"

DBusError *err = &tctx.dbus_s.err;

/* URL decode a string in-place */
static void url_decode(char *str) {
    char *p = str, *q = str;
    while (*p) {
        if (*p == '%' && isxdigit(p[1]) && isxdigit(p[2])) {
            char hex[3] = { p[1], p[2], '\0' };
            *q++ = (char)strtol(hex, NULL, 16);
            p += 3;
        } else if (*p == '+') {
            *q++ = ' '; p++;
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
}

/* ---------- tiny helpers ----------------------------------------------- */
static void var_string(DBusMessageIter *t, const char *v) {
    DBusMessageIter var;
    dbus_message_iter_open_container(t, DBUS_TYPE_VARIANT, "s", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
    dbus_message_iter_close_container(t, &var);
}
static void var_bool(DBusMessageIter *t, int v) {
    DBusMessageIter var; dbus_bool_t b = v ? TRUE : FALSE;
    dbus_message_iter_open_container(t, DBUS_TYPE_VARIANT, "b", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &b);
    dbus_message_iter_close_container(t, &var);
}
static void var_double(DBusMessageIter *t, double v) {
    DBusMessageIter var;
    dbus_message_iter_open_container(t, DBUS_TYPE_VARIANT, "d", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_DOUBLE, &v);
    dbus_message_iter_close_container(t, &var);
}
static void var_int64(DBusMessageIter *t, int64_t v) {
    DBusMessageIter var;
    dbus_message_iter_open_container(t, DBUS_TYPE_VARIANT, "x", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_INT64, &v);
    dbus_message_iter_close_container(t, &var);
}
static void var_emptystrarray(DBusMessageIter *t) {
    DBusMessageIter var, arr;
    dbus_message_iter_open_container(t, DBUS_TYPE_VARIANT, "as", &var);
    dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "s", &arr);
    dbus_message_iter_close_container(&var, &arr);
    dbus_message_iter_close_container(t, &var);
}

/* ---------- dict helpers ----------------------------------------------- */
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

/* ---------- Metadata --------------------------------------------------- */
static void append_metadata(DBusMessageIter *var) {
    DBusMessageIter arr;
    dbus_message_iter_open_container(var, DBUS_TYPE_ARRAY, "{sv}", &arr);

    // length
    if (tctx.state.duration > 0) 
      dict_add_int64(&arr, "mpris:length", (int64_t)tctx.state.duration * 1000000);

    // Artust
    if (tctx.state.metadata.artist[0])
        dict_add_strarray1(&arr, "xesam:artist", tctx.state.metadata.artist);

    // Art Url (cover)
    if (tctx.state.metadata.cover_path[0])
      dict_add_string(&arr, "mpris:artUrl", tctx.state.metadata.cover_path);

    // track
    if (strlen(tctx.state.metadata.track) > 0)
        dict_add_int32(&arr, "xesam:trackNumber", (int32_t)atoi(tctx.state.metadata.track));

    // trackid
    dict_add_path(&arr, "mpris:trackid", "/org/mpris/MediaPlayer2/tomu/track1");

    // title
    if (tctx.state.metadata.title[0])
      dict_add_string(&arr, "xesam:title", tctx.state.metadata.title);

    // url
    if (tctx.state.metadata.url[0])
      dict_add_string(&arr, "xesam:url", tctx.state.metadata.url);

    // data
    if (tctx.state.metadata.date[0])
      dict_add_string(&arr, "xesam:contentCreated", tctx.state.metadata.date);

    // album artist
    if (tctx.state.metadata.album_artist[0])
      dict_add_strarray1(&arr, "xesam:albumArtist", tctx.state.metadata.album_artist);

    // album name
    if (tctx.state.metadata.album[0])
        dict_add_string(&arr, "xesam:album", tctx.state.metadata.album);

    // genre
    if (tctx.state.metadata.genre[0])
        dict_add_strarray1(&arr, "xesam:genre", tctx.state.metadata.genre);

    dbus_message_iter_close_container(var, &arr);
}

/* ---------- append_property / reply_get / reply_get_all ---------------- */
static void append_property(DBusMessageIter *target, const char *iface, const char *prop) {
    (void)iface;
    if (!strcmp(prop, "PlaybackStatus"))
        var_string(target, tctx.state.paused ? "Paused" : (tctx.state.running ? "Playing" : "Stopped"));
    else if (!strcmp(prop, "LoopStatus"))
        var_string(target, tctx.state.looping ? "Playlist" : "None");
    else if (!strcmp(prop, "Rate"))         var_double(target, tctx.state.speed);
    else if (!strcmp(prop, "MinimumRate")) var_double(target, 1.0);
    else if (!strcmp(prop, "MaximumRate")) var_double(target, 1.0);
    else if (!strcmp(prop, "Shuffle"))     var_bool(target, tctx.state.shuffle);
    else if (!strcmp(prop, "Volume"))      var_double(target, tctx.state.volume);
    else if (!strcmp(prop, "Position"))    var_int64(target, (int64_t)tctx.state.position * 1000000);
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

/* ---------- Properties.Set --------------------------------------------- */
static void handle_set(DBusMessage *msg) {
    DBusMessageIter iter, var;
    const char *iface, *prop;

    dbus_message_iter_init(msg, &iter);
    dbus_message_iter_get_basic(&iter, &iface); dbus_message_iter_next(&iter);
    dbus_message_iter_get_basic(&iter, &prop);  dbus_message_iter_next(&iter);
    dbus_message_iter_recurse(&iter, &var);
    (void)iface;

    if (!strcmp(prop, "Volume")) {
        double val;
        dbus_message_iter_get_basic(&var, &val);

        tctx.state.volume = (float)(val / 100.0);
        if (tctx.state.volume < 0.00f) tctx.state.volume = 0.00f;
        if (tctx.state.volume > 1.20f) tctx.state.volume = 1.20f;
    }
    else if (!strcmp(prop, "LoopStatus")) {
        const char *val;
        dbus_message_iter_get_basic(&var, &val);
        tctx.state.looping = (strcmp(val, "None") != 0);
    }
    else if (!strcmp(prop, "Shuffle")) {
        dbus_bool_t val;
        dbus_message_iter_get_basic(&var, &val);
        tctx.state.shuffle = (int)val;
    }

    else if (!strcmp(prop, "Rate")) {
        double val;
        dbus_message_iter_get_basic(&var, &val);
        tctx.state.speed = (float)val;
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

static void handle_message(DBusMessage *msg) {
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
        playback_toggle(&tctx.state);
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Play")) {
        tctx.state.paused = 0;
        pthread_cond_broadcast(&tctx.state.wait_cond);
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Pause")) {
        tctx.state.paused = 1;
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Stop")) {
        tctx.state.running = 0;
        pthread_cond_broadcast(&tctx.state.wait_cond);
        reply_empty(msg); mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Next")) {
        playback_next_audio(&tctx.state);
        reply_empty(msg);
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Previous")) {
        playback_prev_audio(&tctx.state);
        reply_empty(msg);
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "Seek")) {
        dbus_int64_t offset;
        dbus_message_get_args(msg, NULL, DBUS_TYPE_INT64, &offset, DBUS_TYPE_INVALID);
        WITH_LOCK(tctx.state.lock) {
            if (!tctx.state.seek_request) {
                tctx.state.seek_request = 1;
                tctx.state.seek_target = offset; // microseconds
                pthread_cond_broadcast(&tctx.state.wait_cond);
            }
        }
        reply_empty(msg);
        mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "SetPosition")) {
        const char *track_id;
        dbus_int64_t pos;
        dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &track_id,
                              DBUS_TYPE_INT64, &pos, DBUS_TYPE_INVALID);
        WITH_LOCK(tctx.state.lock) {
            if (!tctx.state.seek_request) {
                tctx.state.seek_request = 1;
                tctx.state.seek_target = pos; // microseconds, absolute
                pthread_cond_broadcast(&tctx.state.wait_cond);
            }
        }
        reply_empty(msg);
        mpris_notify_change();
    }
    else if (dbus_message_is_method_call(msg, IFACE_PLAYER, "OpenUri")) {
        const char *uri;
        dbus_error_init(err);
        if (dbus_message_get_args(msg, err, DBUS_TYPE_STRING, &uri, DBUS_TYPE_INVALID)) {
            reply_empty(msg);
            strcpy(tctx.state.metadata.url, uri);
            const char *path = uri;
            char *decoded = NULL;
            if (strncmp(uri, "file://", 7) == 0) {
                decoded = strdup(uri + 7);
                url_decode(decoded);
                path = decoded;
            }
            queue_add(path);
            free(decoded);
        } else {
            DBusMessage *error = dbus_message_new_error(msg,
                "org.freedesktop.DBus.Error.InvalidArgs", "Invalid URI argument");
            dbus_connection_send(tctx.dbus_s.conn, error, NULL);
            dbus_message_unref(error);
            dbus_error_free(err);
        }
    }
    else if (dbus_message_is_method_call(msg, IFACE_ROOT, "Raise")) {
        reply_empty(msg);
    }
    else if (dbus_message_is_method_call(msg, IFACE_ROOT, "Quit")) {
        reply_empty(msg);
        raise(SIGINT);
    }
}

// read from dbus incoming msg
void mpris_dispatch(void) {
    if (!tctx.dbus_s.conn) die("dbus:");
    dbus_connection_read_write(tctx.dbus_s.conn, 0); // read incoming msg from dbus 

    // Process each received msg
    while ((tctx.dbus_s.msg = dbus_connection_pop_message(tctx.dbus_s.conn)) != NULL) {
        handle_message(tctx.dbus_s.msg);
        dbus_message_unref(tctx.dbus_s.msg);
    }
}

// broadcast update properties
void mpris_notify_change(void) {
    if (!tctx.dbus_s.conn) die("dbus:");

    // create dbus signal
    DBusMessage *signal = dbus_message_new_signal(OBJ_PATH, IFACE_PROPS, "PropertiesChanged");
    if (!signal) return;

    DBusMessageIter iter, changed, invalid;
    dbus_message_iter_init_append(signal, &iter); // start write data into msg

    // the changed properties belong to to org.mpris.MediaPlayer2.player
    const char *iface = IFACE_PLAYER;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface);

    // start the dictionary of changed properties
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

void mpris_loop()
{
  while(T_RUE)
  {
    mpris_dispatch();
    if (tctx.state.running) mpris_notify_change();
    sleep_ms(100);
  }
}
