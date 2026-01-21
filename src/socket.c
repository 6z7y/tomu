#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>

#include "socket.h"
#include "backend.h"
#include "control.h"
#include "utils.h"

void cleanup_socket(int sig){
	unlink(SOCKET_PATH);
	die("");
}

// TODO: change the fn name 
// TODO: abstract input reading from "control.c" to be used in here as well since we have code duplication
void *run_socket(void *arg)
{
	Audio_State *state = (Audio_State*)arg;
	unlink(SOCKET_PATH);
	signal(SIGTERM, cleanup_socket);
	signal(SIGINT, cleanup_socket);

	struct sockaddr_un addr = {0}; // why zero mem at runtime????
    // memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);
	
	int sock;
	if ((sock = socket(AF_UNIX, SOCK_STREAM , 0)) <0 ){
		warn("Socket","failed: %s", strerror(errno));
	}

	if (bind(sock, (struct sockaddr*)&addr , sizeof(addr)) < 0)
		warn("Bind","failed: %s", strerror(errno));

	if (listen(sock, 10) < 0) {
		warn("Listen","failed: %s", strerror(errno));
    }

    struct pollfd pfd = {
        .fd = sock,
        .events = POLLIN
    };

    char buf[256]; // less stack movement
    // stop messing indentation!!!!!
	while (1) {
        int ret = poll(&pfd, 1, 80);
        
        if (!state->running) break;

        if (ret > 0 && (pfd.revents & POLLIN)) {
          int client = accept(sock, NULL, NULL);
          if (client < 0) continue;

          int n;
          while ((n = recv(client, buf, sizeof(buf)-1, 0)) > 0) {
            buf[n] = '\0';
            if (!strncmp(buf, "q", 1)) die("");
            if (!strncmp(buf, " ", 1)) playback_toggle(state);
          }
          close(client);
        }
        else if (ret == 0)
          continue;

        else 
          perror("[F] poll error");
    }

	close(sock);

    return NULL;
}
