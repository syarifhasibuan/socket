#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>

#define BACKLOG      10
#define DEFAULT_IP   "127.0.0.1"
#define DEFAULT_PORT "12345"
#define MAX_CLIENTS  10
#define TIMEOUT      2500
#define BUFFER_SIZE  256

#define LENGTH(X)   (sizeof X / sizeof X[0])

int main(int argc, char *argv[])
{
  char *ip, *port;

  if (argc > 3) {
    printf("Usage: %s <IP Address> <Port>\n", argv[0]);
    exit(EXIT_SUCCESS);
  } else if (argc == 3) {
    ip   = strdup(argv[1]);
    port = strdup(argv[2]);
  } else if (argc == 2) {
    ip   = strdup(argv[1]);
    port = strdup(DEFAULT_PORT);
  } else {
    ip   = strdup(DEFAULT_IP);
    port = strdup(DEFAULT_PORT);
  }

  int status;
  struct addrinfo hints, *servinfo;
  int sockfd;

  memset(&hints, 0, sizeof hints);

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_PASSIVE;

  /* TODO: check multiple servinfo */
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

  if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  freeaddrinfo(servinfo);
  free(ip);
  free(port);

  if (listen(sockfd, BACKLOG) == -1) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  const int base_pollfd = 2; /* stdin and sockfd */
  const int max_pollfd = MAX_CLIENTS + base_pollfd;
  struct pollfd fds[max_pollfd];
  int client_count = 0;
  int new_fd;

  fds[0].fd = 0; /* stdin */
  fds[0].events = POLLIN;
  fds[1].fd = sockfd;
  fds[1].events = POLLIN;

  while (1) {
    int ret = poll(fds, base_pollfd+client_count, TIMEOUT);

    if (ret == -1) {
      perror("Poll failed");
      continue;
    } else if (ret == 0) {
      continue;
    }

    /* Read from stdin and broadcast message from server */
    if (fds[0].revents & POLLIN) {
      char buffer[BUFFER_SIZE] = {0};

      ret = read(0, buffer, BUFFER_SIZE - 1);

      if (ret == -1) {
        perror("read from stdin");
        continue;
      } else if (ret > 0) {
        buffer[ret] = '\0';  // Null-terminate the string
      }

      for (int i = base_pollfd; i < client_count + base_pollfd; i++) {
        send(fds[i].fd, buffer, ret, 0);
      }

    }

    /* Check if there is new connection */
    if (fds[1].revents & POLLIN) {
      struct sockaddr_storage incoming_addr;
      socklen_t addr_size = sizeof incoming_addr;
      new_fd = accept(sockfd, (struct sockaddr *)&incoming_addr, &addr_size);
      if (new_fd == -1) {
        perror("Accept failed");
        continue;
      }

      if (client_count >= MAX_CLIENTS) {
        char buffer[53] = {0};
        printf("Max clients has been reached.\n");
        printf("Rejecting connection...\n");
        snprintf(buffer, 53, "Max clients has been reached. Closing connection...\n");
        send(new_fd, buffer, 53, 0);
        close(new_fd);
        continue;
      }

      fds[client_count + base_pollfd].fd = new_fd;
      fds[client_count + base_pollfd].events = POLLIN;
      client_count++;

      printf("New client connected: %d\n", new_fd);
    }

    /* Check for any data in the client */
    for (int i = base_pollfd; i < client_count + base_pollfd; i++) {

      if (fds[i].revents & POLLIN) {
        char buffer[BUFFER_SIZE] = {0};
        int bytes_received = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received < 0) {
          perror("Recv failed");
        } else if (bytes_received == 0) {
          printf("Client %d disconnected\n", fds[i].fd);
          close(fds[i].fd);
          client_count--;
          fds[i] = fds[base_pollfd + client_count]; /* Reorder fds */
        } else {
          printf("Client fd %d: %s\n", fds[i].fd, buffer);
        }

      }
    }
  }

  close(sockfd);

  return 0;
}
