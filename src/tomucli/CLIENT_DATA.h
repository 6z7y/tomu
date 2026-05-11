#ifndef CLIENT_DATA_H
#define CLIENT_DATA_H

#include "../shared/share_utils.h"

// Config Structures
typedef struct {
  int width;
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
  char **files; // array store name files
  char base_path[1024]; // for dir and random
  unsigned int rand_num; // for used for get random (% total files)
  int totalFiles;
  int currentFile;
} Dir_File;

// Playback queue state
typedef struct {
  Dir_File dir;
  int has_queue;  // Whether we're in directory playback mode
  int current_index;  // Track current position in queue
  int file_played;    // Flag to know if we've sent the current file
} PlaybackQueue;

typedef struct {
  CONFIG cfg;
  PlaybackQueue queue;
  TomuStatus status;
  int server_fd;
  int running;
} Client_CTX;

extern Client_CTX ctx;

#endif
