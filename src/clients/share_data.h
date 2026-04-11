#ifndef SHARE_DATA
#define SHARE_DATA

typedef enum {
  CLIENT_CLI,
  CLIENT_TUI,
  CLIENT_GUI,

} ClientType;


typedef struct {
  char** files;
  int totalFiles;
  int currentFile;
} Dir_File;

// Playback queue state
typedef struct {
  Dir_File dir;
  char base_path[1024];
  int has_queue;  // Whether we're in directory playback mode
} PlaybackQueue;

typedef struct {
  int server_fd;
  PlaybackQueue queue;
} Client_CTX;

extern Client_CTX client_ctx;

#endif
