#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "backend.h"
#include "backend_utils.h"
#include "control.h"
#include "utils.h"

extern PlayBackState STATE;
uint KeepPlayingDirectory = 1;

// defined here because of the extren
dirFiles DirFiles = {
  .DirLoopStop = true
};

inline void help()
{
  printf(
    "Usage: tomu [COMMAND] [PATH]\n"
    " Commands:\n\n"

    "   --loop            : loop same sound\n"
    "   --version         : show version of program\n"
    "   --help            : show help message\n"

    "\nkeys:\n"
    " (Space) = pause/resume\n"
    " (q) = quit\n"
    " (s) = shuffle toggle\n"
    " (l) = loop toggle\n"
    " (-) = decrease volume\n"
    " (+) = increase volume\n"
    " (↑/→) = audio seek forward +5s/1m\n"
    " (←/↓) = audio seek backward -5s/1m\n"
    " ([) = audio speed decrease\n"
    " (]) = audio speed increase\n"
    " (</>) = (Pervious/Next) audio\n"

    "\nExample: tomu loop [FILE.mp3]\n"
  );
}

void cleanUP(AVFormatContext *fmtCTX, AVCodecContext *codecCTX){
  if (fmtCTX ) avformat_close_input(&fmtCTX);
  if (codecCTX ) avcodec_free_context(&codecCTX);
}

void path_handle(const char *path, uint loop)
{
  struct stat st;

  if (stat(path, &st) < 0 ) goto bad_path;

  if (S_ISDIR(st.st_mode)){
    DirFiles.path = (char*)path;
    DirFiles.files = extractDir(path);
    DirFiles.DirLoopStop = false;

    shuffle(); // Set initial file

    // Keep playing files until the user quits 
    // (sets KeepPlayingDirectory = 0)
    while ((KeepPlayingDirectory || DirFiles.DirLoopStop)) {
      if (DirFiles.shuffle)
        shuffle();

      char filename[1024];
      snprintf(filename, sizeof(filename), 
         "%s/%s", DirFiles.path, DirFiles.files[DirFiles.currentFile]
      );

      // Run the player. It will block here until the song ends or 'next' is pressed.
      playback_run(filename, loop);

      // If playback_run returns, it cleaned up its own buffer/context.
      // We loop back and play the NEW DirFiles.currentFile.
    }

    // Cleanup files
    for (int i=0; i<DirFiles.totalFiles; i++) {
      free(DirFiles.files[i]);
    }
    free(DirFiles.files);
  }
  // FILE HANDLING
  else if (S_ISREG(st.st_mode)) {
    playback_run(path, loop);
  }
  else goto bad_path;

  return;

bad_path:
  die("File:");
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
