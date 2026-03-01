#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "backend.h"
#include "backend_utils.h"
#include "utils.h"

static const char *filter_formats[] = {
  ".mp3", ".opus", ".flac", ".wav",
  ".ogg", ".aac", ".wma", ".aiff",
  ".m4a"
};

void cleanUP(){
  if (ctx.fmtCTX ) avformat_close_input(&ctx.fmtCTX);
  if (ctx.codecCTX ) avcodec_free_context(&ctx.codecCTX);
}

int is_audio(const char *file)
{
    int len = strlen(file);
    int limit = (sizeof(filter_formats) / sizeof(filter_formats[0]));

    for (int i = 0; i < limit; i++){
      int ext_len = strlen(filter_formats[i]);
      if (!strcmp(file + len - ext_len, filter_formats[i]))
        return 1; // is a file audio
    }
    return 0; // it's not a file audio
}

void path_handle(const char *path, uint loop_mode, uint shuffle_mode, uint skip_fmt_mode)
{
    Dir_File *dir = &ctx.dir;

    struct stat st;
    if (stat(path, &st) < 0) die("File:");

    // If path is a directory
    if (S_ISDIR(st.st_mode)) {
        dir->files = extractDir(path, dir); // assume this returns char** array

        srand(time(NULL));

        dir->currentFile = shuffle_mode ? rand() % dir->totalFiles : 0;

        while (1){
          if (dir->currentFile < 0) dir->currentFile = dir->totalFiles - 1;
          if (dir->currentFile >= dir->totalFiles) dir->currentFile = 0;

          char filename[1024];
          snprintf(filename, sizeof(filename), "%s/%s", path, dir->files[dir->currentFile]);

          // Skip non-audio files
          if (!skip_fmt_mode){
            if (!is_audio(filename)) {
              printf("'%s' is not audio file, skipping\n", filename);
              dir->currentFile++;
              continue;
            }
          }

          /* reset skip flag before playing */
          ctx.state.skip_to_next = 1;

          // Play audio
          playback_run(filename, loop_mode, shuffle_mode);

          /* q was pressed — fully quit */
          if (ctx.state.skip_to_next == 0) break;

          /* next / prev / shuffle */
          if (ctx.state.shuffle)
              dir->currentFile = rand() % dir->totalFiles;
          else
              dir->currentFile += ctx.state.skip_to_next; /* +1 or -1 */
        }

        // Cleanup
        for (int i = 0; i < dir->totalFiles; i++)
            free(dir->files[i]);
        free(dir->files);
    }

    // If path is a regular file
    else if (S_ISREG(st.st_mode)) {

        if (!skip_fmt_mode){
          if (!is_audio(path)) {
              printf("'%s' is not audio file\n", path);
              return;
          }
        }
        playback_run(path, loop_mode, shuffle_mode);
    }

    else die("File:");
}



void verr(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}
}

void warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verr(fmt, ap);
  va_end(ap);
}

void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
  verr(fmt, ap);
	va_end(ap);
	exit(-1);
}
