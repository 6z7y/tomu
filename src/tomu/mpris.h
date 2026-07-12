#ifndef MPRIS_H
#define MPRIS_H

void mpris_init(void);
void *mpris_loop(void *arg);
void mpris_notify_change(void);
void mpris_dispatch(void);

#endif
