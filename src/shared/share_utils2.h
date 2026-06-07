#ifndef SHARED_UTILS2_H
#define SHARED_UTILS2_H

/* time durations */
#define get_hour(a) ((int)a / 3600) // convert time to hour
#define get_min(a) (((int)a % 3600) / 60) // convert time to min
#define get_sec(a) ((int)a % 60) // convert time to sec

#endif
