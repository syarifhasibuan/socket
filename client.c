#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>

#define TIMEOUT     2500
#define BUFFER_SIZE 256
#define LENGTH(X)   (sizeof X / sizeof X[0])

int main(int argc, char* argv[])
{
  char *ip, *port;

  if (argc > 3) {
    printf("Usage: %s <IP Address> <Port>\n", argv[0]);
    exit(EXIT_SUCCESS);
  }

  ip   = strdup(argv[1]);
  port = strdup(argv[2]);

  int status;
  struct addrinfo hints, *servinfo;
  int sockfd;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(EXIT_FAILURE);
  }

  if (servinfo == NULL) {
    fprintf(stderr, "No server info obtained");
    exit(EXIT_FAILURE);
  }

  if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
          servinfo->ai_protocol)) == -1) {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  if ((status = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)) == -1) {
    perror("connect to server failed");
    exit(EXIT_FAILURE);
  }

  struct pollfd fds[2]; /* stdin and sockfd */
  fds[0].fd = 0; /* stdin */
  fds[0].events = POLLIN;
  fds[1].fd = sockfd;
  fds[1].events = POLLIN;

  while (1) {
    int ret = poll(fds, 2, TIMEOUT);

    if (ret == -1) {
      perror("Poll failed");
      continue;
    } else if (ret == 0) {
      continue;
    }

    if (fds[0].revents & POLLIN) {
      char buffer[BUFFER_SIZE] = {0};

      ret = read(0, buffer, BUFFER_SIZE - 1);

      if (ret == -1) {
        perror("read from stdin");
        continue;
      } else if (ret > 0) {
        buffer[ret] = '\0';  // Null-terminate the string
      }

      send(fds[1].fd, buffer, ret, 0);
    }

    if (fds[1].revents & POLLIN) {
      char buffer[BUFFER_SIZE] = {0};
      int bytes_received = recv(fds[1].fd, buffer, sizeof buffer - 1, 0);

      if (bytes_received < 0) {
        perror("Recv failed");
      } else if (bytes_received == 0) {
        printf("Connection closed by foreign host.\n");
        break;
      } else {
        printf("Server: %s\n", buffer);
      }
    }
  }

  close(sockfd);

  return 0;
}
