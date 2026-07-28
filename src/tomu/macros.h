#ifndef MACROS_H
#define MACROS_H

#include <sys/stat.h>
#include <unistd.h>

// program info
#define TOMU_NAME "tomu"
#define TOMU_VER "1.3.7"


#define STATE_FILE  "/tmp/tomu.inf"
#define CMD_FILE    "/tmp/tomu.cmd"

// Utils
#define true 1
#define false 0

// colors
#define RED "\e[0;31m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"
#define MAG "\e[0;35m"
#define GRN "\e[0;32m"

// checking from msg if url or path
#define IS_PATH_DIR(arg) ({ struct stat _st; (stat((arg), &_st) == 0) && S_ISDIR(_st.st_mode); }) //1=dir, 0=not_dir
#define IS_PATH_RAW(arg) ({ struct stat _st; (stat((arg), &_st) == 0) && S_ISREG(_st.st_mode); }) //1=file, 0=not_file
#define IS_URL_RAW(arg) ((!strncmp((arg), "http://", 7)) || (!strncmp((arg), "https://", 8))) //1=url, 0=not_url
#define IS_URL_PLAYLIST(arg) (IS_URL_RAW(arg) && strstr((arg), "list=") != NULL) //1=url_playlist, 0=not

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
