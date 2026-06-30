#ifndef MPRIS_H
#define MPRIS_H

// connect to session bus, claim org.mpris.MediaPlayer2.tomu
void mpris_init(void);

// call every loop tick instead of poll() on the old socket fd
// reads+handles any pending D-Bus calls, never blocks

// call instead of broadcast_status() — tells D-Bus listeners
// (gnome/kde panels, playerctl...) that something changed
void mpris_notify_change(void);


void start_playback(char *path);
void mpris_loop();

#endif
