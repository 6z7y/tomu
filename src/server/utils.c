#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "backend.h"
#include "backend_utils.h"
#include "utils.h"

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
    " (Backspace) = reset playback speed\n"
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

int is_audio(const char *file)
{
    int len = strlen(file);

    return 
      !strcmp(file + len - 4, ".mp3") ||
      !strcmp(file + len - 5, ".flac") ||
      !strcmp(file + len - 5, ".opus") ||
      !strcmp(file + len - 4, ".wav") ||
      !strcmp(file + len - 4, ".ogg") ||
      !strcmp(file + len - 4, ".aac") ||
      !strcmp(file + len - 4, ".wma") ||
      !strcmp(file + len - 5, ".aiff") ||
      !strcmp(file + len - 5, ".m4a");
}

void cleanUP(){
  if (ctx.fmtCTX ) avformat_close_input(&ctx.fmtCTX);
  if (ctx.codecCTX ) avcodec_free_context(&ctx.codecCTX);
}

void path_handle(const char *path, unsigned int loop_mode, unsigned int shuffle_mode)
{
    struct stat st;
    if (stat(path, &st) < 0) {
        die("File:");
        return;
    }

    // If path is a directory
    if (S_ISDIR(st.st_mode)) {
        DirFiles.path = (char*)path;
        DirFiles.files = extractDir(path); // assume this returns char** array
        DirFiles.DirLoopStop = false;

        int currentFile = 0;

        while (KeepPlayingDirectory || DirFiles.DirLoopStop) {
            if (currentFile >= DirFiles.totalFiles) {
                if (DirFiles.DirLoopStop)
                    currentFile = 0; // loop
                else
                    break;
            }

            char filename[1024];
            snprintf(filename, sizeof(filename), "%s/%s", DirFiles.path, DirFiles.files[currentFile]);

            // Skip non-audio files
            if (!is_audio(filename)) {
                printf("%s is not audio, skipping\n", filename);
                currentFile++;
                continue;
            }

            // Play audio
            playback_run(filename, loop_mode, shuffle_mode);

            currentFile++; // move to next
        }

        // Cleanup
        for (int i = 0; i < DirFiles.totalFiles; i++)
            free(DirFiles.files[i]);
        free(DirFiles.files);
    }

    // If path is a regular file
    else if (S_ISREG(st.st_mode)) {
        if (!is_audio(path)) {
            printf("'%s' is not audio\n", path);
            return;
        }
        playback_run(path, loop_mode, shuffle_mode);
    }

    else {
        die("File:");
    }
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
