#ifndef SHARE_DATA
#define SHARE_DATA

#define SOCKET_PATH "/tmp/tomu-sock" // path socket file
#define MAX_CLIENT 6 // limit client connection

// server info
#define TOMU_NAME "tomu"
#define TOMU_VER "1.1.0"

// client info
#define TOMUCLI_NAME "tomucli"
#define TOMUCLI_VER "1.0.0"

#define TOMUTUI_NAME "tomutui"
#define TOMUTUI_VER "X.X.X"

#define TOMUGUI_NAME "tomugui"
#define TOMUGUI_VER "X.X.X"

// Utils
#define false 0
#define true 1

typedef enum { // types
  CLIENT_CLI,
  CLIENT_TUI,
  CLIENT_GUI,

} ClientType;

#endif
