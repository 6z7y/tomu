#ifndef CLIENT_DATA_H
#define CLIENT_DATA_H

#include "../shared/share_utils1.h"

// Config Structures
typedef struct {
  int width;
  char done[8];
  char current[8];
  char remaining[8];
  char color[8];

} ProgressBarStyle;

// structure config for client
typedef struct {
  ProgressBarStyle progress;
} CONFIG;
//----------------------------

typedef struct {
  char **files; // array store name files
  int count;
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
  TomuStatus status;
  PlaybackQueue queue;
  int server_fd;
  int filter_files;
  int running;
} Client_CTX;

extern Client_CTX ctx;

#endif
