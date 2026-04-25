#ifndef CLIENT_DATA_H
#define CLIENT_DATA_H

// Config Structures
typedef struct {
  char done;
  char current;
  char remaining;

} ProgressBarStyle;

// structure config for client
typedef struct {
  ProgressBarStyle progress;
} CONFIG;
//----------------------------
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
  CONFIG cfg;
  PlaybackQueue queue;
  int server_fd;
} Client_CTX;

extern Client_CTX client_ctx;

#endif
