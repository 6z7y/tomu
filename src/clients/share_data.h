#ifndef SHARE_DATA
#define SHARE_DATA

// struct for data of the files in dir
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

#endif
