#ifndef APP_MACROS_H
#define APP_MACROS_H

#include <unistd.h>

// program info
#define TOMU_NAME "tomu"
#define TOMU_VER "1.3.5"

// D-Bus settings
#define BUS_NAME    "org.mpris.MediaPlayer2.tomu"
#define OBJ_PATH    "/org/mpris/MediaPlayer2"
#define IFACE_ROOT  "org.mpris.MediaPlayer2" // tomu
#define IFACE_PLAYER "org.mpris.MediaPlayer2.Player" // control
#define IFACE_PROPS "org.freedesktop.DBus.Properties" // write/read info

// Utils
#define F_ALSE 0
#define T_RUE 1

// colors
#define RED "\e[0;31m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"
#define MAG "\e[0;35m"
#define GRN "\e[0;32m"

// checking from msg if url
#define IS_URL(arg) (!strncmp(arg, "http://", 7)) || (!strncmp(arg, "https://", 8)) // 1=is_url, 0=not_url
#define IS_PATH(arg) (access(arg, F_OK) == 0)

// block of pthread_mutex lock/unlock
#define WITH_LOCK(mutex) for (int _once = (pthread_mutex_lock(&(mutex)), 1); _once; _once = (pthread_mutex_unlock(&(mutex)), 0)) // pthread mutex

/* for loop */
#define LEN(a) ( sizeof(a) / sizeof(a[0]) ) // get size array
#define for_each_arr(arr) for (size_t i=0; i<LEN(arr); i++) // for array
#define for_each_num(n) for (size_t i=0; i<n; i++) // for normal loop number set

// sleeping by micro sec
#define sleep_ms(n) usleep(1000*n)

/* time durations */
#define get_hour(a) ((int)a / 3600) // convert time to hour
#define get_min(a) (((int)a % 3600) / 60) // convert time to min
#define get_sec(a) ((int)a % 60) // convert time to sec

#endif
