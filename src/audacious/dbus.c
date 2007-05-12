/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2007 Ben Tucker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <glib.h>
#include <dbus/dbus-glib-bindings.h>
#include "dbus.h"
#include "dbus-service.h"
#include "dbus-server-bindings.h"

#include "main.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "input.h"
#include "playback.h"
#include "playlist.h"
#include "ui_playlist.h"
#include "ui_preferences.h"
#include "memorypool.h"
#include "titlestring.h"
#include "ui_jumptotrack.h"

G_DEFINE_TYPE(RemoteObject, audacious_rc, G_TYPE_OBJECT);

void audacious_rc_class_init(RemoteObjectClass *klass) {}

void audacious_rc_init(RemoteObject *object) {
    GError *error = NULL;
    DBusGProxy *driver_proxy;
    unsigned int request_ret;

    // Initialize the DBus connection
    object->connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
    if (object->connection == NULL) {
        g_warning("Unable to connect to dbus: %s", error->message);
        g_error_free(error);
        return;
    }
    
    dbus_g_object_type_install_info(audacious_rc_get_type(),
                                    &dbus_glib_audacious_rc_object_info);
    
    // Register DBUS path
    dbus_g_connection_register_g_object(object->connection,
                                        AUDACIOUS_DBUS_PATH, G_OBJECT(object));

    // Register the service name, the constants here are defined in
    // dbus-glib-bindings.h
    driver_proxy = dbus_g_proxy_new_for_name(object->connection,
                                             DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS);

    if (!org_freedesktop_DBus_request_name(driver_proxy,
        AUDACIOUS_DBUS_SERVICE, 0, &request_ret, &error)) {
        g_warning("Unable to register service: %s", error->message);
        g_error_free(error);
    }

    g_object_unref(driver_proxy);
}

void init_dbus() {
    g_type_init();
    g_object_new(audacious_rc_get_type(), NULL);
    g_message("D-Bus support has been activated");
}

// Audacious General Information
gboolean audacious_rc_version(RemoteObject *obj, gchar **version,
                                  GError **error) {
    *version = g_strdup(VERSION);
    return TRUE;
}

gboolean audacious_rc_quit(RemoteObject *obj, GError **error) {
    mainwin_quit_cb();
    return TRUE;
}

gboolean audacious_rc_eject(RemoteObject *obj, GError **error) {
    if (has_x11_connection)
        mainwin_eject_pushed();
    return TRUE;
}

gboolean audacious_rc_main_win_visible(RemoteObject *obj,
                                  gboolean *is_main_win, GError **error) {
    *is_main_win = cfg.player_visible;
    g_message("main win %s\n", (cfg.player_visible? "visible" : "hidden"));
    return TRUE;
}

gboolean audacious_rc_show_main_win(RemoteObject *obj, gboolean show,
                                        GError **error) {
    g_message("%s main win\n", (show? "showing": "hiding"));
    if (has_x11_connection)
        mainwin_show(show);
    return TRUE;
}

gboolean audacious_rc_equalizer_visible(RemoteObject *obj,
                                  gboolean *is_eq_win, GError **error) {
    *is_eq_win = cfg.equalizer_visible;
    return TRUE;
}

gboolean audacious_rc_show_equalizer(RemoteObject *obj, gboolean show,
                                         GError **error) {
    if (has_x11_connection)
        equalizerwin_show(show);
    return TRUE;
}

gboolean audacious_rc_playlist_visible(RemoteObject *obj,
                                           gboolean *is_pl_win,
                                           GError **error) {
    *is_pl_win = cfg.playlist_visible;
    return TRUE;
}

gboolean audacious_rc_show_playlist(RemoteObject *obj, gboolean show,
                                        GError **error) {
    if (has_x11_connection) {
        if (show)
            playlistwin_show();
        else
            playlistwin_hide();
    }
    return TRUE;
}

// Playback Information/Manipulation
gboolean audacious_rc_play(RemoteObject *obj, GError **error) {
    if (playback_get_paused())
        playback_pause();
    else if (playlist_get_length(playlist_get_active()))
        playback_initiate();
    else
        mainwin_eject_pushed();
    return TRUE;
}

gboolean audacious_rc_pause(RemoteObject *obj, GError **error) {
    playback_pause();
    return TRUE;
}

gboolean audacious_rc_stop(RemoteObject *obj, GError **error) {
    ip_data.stop = TRUE;
    playback_stop();
    ip_data.stop = FALSE;
    mainwin_clear_song_info();
    return TRUE;
}

gboolean audacious_rc_playing(RemoteObject *obj, gboolean *is_playing,
                                  GError **error) {
    *is_playing = playback_get_playing();
    return TRUE;
}

gboolean audacious_rc_paused(RemoteObject *obj, gboolean *is_paused,
                                 GError **error) {
    *is_paused = playback_get_paused();
    return TRUE;
}

gboolean audacious_rc_stopped(RemoteObject *obj, gboolean *is_stopped,
                                  GError **error) {
    *is_stopped = !playback_get_playing();
    return TRUE;
}

gboolean audacious_rc_status(RemoteObject *obj, gchar **status,
                                 GError **error) {
    if (playback_get_paused())
        *status = g_strdup("paused");
    else if (playback_get_playing())
        *status = g_strdup("playing");
    else
        *status = g_strdup("stopped");
    return TRUE;
}

gboolean audacious_rc_time(RemoteObject *obj, gint *time, GError **error) {
    if (playback_get_playing())
        *time = playback_get_time();
    else
        *time = 0;
    return TRUE;
}

gboolean audacious_rc_seek(RemoteObject *obj, guint pos, GError **error) {
    if (playlist_get_current_length(playlist_get_active()) > 0 &&
            pos < (guint)playlist_get_current_length(playlist_get_active()))
            playback_seek(pos / 1000);

    return TRUE;
}

gboolean audacious_rc_volume(RemoteObject *obj, gint *vl, gint *vr,
                                 GError **error) {
    input_get_volume(vl, vr);
    return TRUE;
}

gboolean audacious_rc_set_volume(RemoteObject *obj, gint vl, gint vr,
                                     GError **error) {
    if (vl > 100)
        vl = 100;
    if (vr > 100)
        vr = 100;
    input_set_volume(vl, vr);
    return TRUE;
}

gboolean audacious_rc_balance(RemoteObject *obj, gint *balance,
                                  GError **error) {
    gint vl, vr;
    input_get_volume(&vl, &vr);
    if (vl < 0 || vr < 0)
        *balance = 0;
    else if (vl > vr)
        *balance = -100 + ((vr * 100) / vl);
    else if (vr > vl)
        *balance = 100 - ((vl * 100) / vr);
    else
        *balance = 0;
    return TRUE;
}

// Playlist Information/Manipulation
gboolean audacious_rc_position(RemoteObject *obj, int *pos, GError **error)
{
    *pos = playlist_get_position(playlist_get_active());
    return TRUE;
}

gboolean audacious_rc_advance(RemoteObject *obj, GError **error) {
    playlist_next(playlist_get_active());
    return TRUE;
}

gboolean audacious_rc_reverse(RemoteObject *obj, GError **error) {
    playlist_prev(playlist_get_active());
    return TRUE;
}

gboolean audacious_rc_length(RemoteObject *obj, int *length,
                                 GError **error) {
    *length = playlist_get_length(playlist_get_active());
    return TRUE;
}

gboolean audacious_rc_song_title(RemoteObject *obj, int pos,
                                     gchar **title, GError **error) {
    *title = playlist_get_songtitle(playlist_get_active(), pos);
    return TRUE;
}

gboolean audacious_rc_song_filename(RemoteObject *obj, int pos,
                                        gchar **filename, GError **error) {
    *filename = playlist_get_filename(playlist_get_active(), pos);
    return TRUE;
}

gboolean audacious_rc_song_length(RemoteObject *obj, int pos, int *length,
                                      GError **error) {
    *length = playlist_get_songtime(playlist_get_active(), pos) / 1000;
    return TRUE;
}

gboolean audacious_rc_song_frames(RemoteObject *obj, int pos, int *length,
                                      GError **error) {
    *length = playlist_get_songtime(playlist_get_active(), pos);
    return TRUE;
}

gboolean audacious_rc_jump(RemoteObject *obj, int pos, GError **error) {
    if (pos < (guint)playlist_get_length(playlist_get_active()))
                playlist_set_position(playlist_get_active(), pos);
    return TRUE;
}

gboolean audacious_rc_add_url(RemoteObject *obj, gchar *url,
                                  GError **error) {
    playlist_add_url(playlist_get_active(), url);
    return TRUE;
}

gboolean audacious_rc_delete(RemoteObject *obj, int pos, GError **error) {
    playlist_delete_index(playlist_get_active(), pos);
    return TRUE;
}

gboolean audacious_rc_clear(RemoteObject *obj, GError **error) {
    playlist_clear(playlist_get_active());
    mainwin_clear_song_info();
    mainwin_set_info_text();
    return TRUE;
}

gboolean audacious_rc_repeating(RemoteObject *obj, gboolean *is_repeating,
                                    GError **error) {
    *is_repeating = cfg.repeat;
    return TRUE;
}

gboolean audacious_rc_repeat(RemoteObject *obj, GError **error) {
    mainwin_repeat_pushed(!cfg.repeat);
    return TRUE;
}

gboolean audacious_rc_shuffling(RemoteObject *obj, gboolean *is_shuffling,
                                    GError **error) {
    *is_shuffling = cfg.shuffle;
    return TRUE;
}

gboolean audacious_rc_shuffle(RemoteObject *obj, GError **error) {
    mainwin_shuffle_pushed(!cfg.shuffle);
    return TRUE;
}