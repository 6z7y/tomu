#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>

#define SOCKET_PATH "/tmp/tomu-sock"

int main() {
    int sock;
    char s[32];
    char buf[100];

    struct sockaddr_un addr = {
      .sun_family = AF_UNIX,
      .sun_path = SOCKET_PATH
    };

    sock = socket(AF_UNIX, SOCK_STREAM, 0); // create socket

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1){
      perror("Connect");
      return -1;
    }
    while (1){
      printf("write something to server: ");
      scanf("%s", s);

      write(sock, s, strlen(s));
      // int n = read(sock, buf, sizeof(buf));
      // buf[n] = '\0';
      // printf("server say: %s\n", buf);
    }

    close(sock);
}

