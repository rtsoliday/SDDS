/**
 * @file sddslogclient.c
 * @brief A simple TCP client for sending commands to a server.
 *
 * @details
 * This program establishes a TCP connection to a specified server and port,
 * sends commands either from the command-line arguments or interactively
 * from the user, and processes the server's responses. The client supports
 * persistent connections, allowing multiple commands to be sent over a single
 * socket until a "disconnect" command is issued.
 *
 * @section Usage
 * ```
 * sddslogclient <hostname> <port> [<command> ...]
 * ```
 *
 * @section Options
 * | Required   | Description                                                     |
 * |------------|-----------------------------------------------------------------|
 * | `hostname` | The server's hostname or IP address.                            |
 * | `port`     | The port number on which the server is listening.               |
 *
 * | Optional   | Description                                                     |
 * |------------|-----------------------------------------------------------------|
 * | `<command>`| One or more commands to send to the server initially.           |
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author
 * M. Borland, R. Soliday
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFLEN 16384

/* Error handling function */
void error_exit(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

/* Process a single command */
int process_command(char *buffer, int sockfd) {
  ssize_t n;

  n = write(sockfd, buffer, strlen(buffer));
  if (n < 0)
    error_exit("ERROR writing to socket");

  memset(buffer, 0, BUFLEN);
  n = read(sockfd, buffer, BUFLEN - 1);
  if (n < 0)
    error_exit("ERROR reading from socket");

  printf("%s\n", buffer);
  return n;
}

/* Main function */
int main(int argc, char *argv[]) {
  int sockfd, portno, arg_index;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  int persist = 1;

  char buffer[BUFLEN];

  if (argc < 3) {
    fprintf(stderr, "Usage: %s <hostname> <port> [<command> [<command> ...]]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  portno = atoi(argv[2]);
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error_exit("ERROR opening socket");

  server = gethostbyname(argv[1]);
  if (server == NULL) {
    fprintf(stderr, "ERROR, no such host\n");
    exit(EXIT_FAILURE);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(portno);

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error_exit("ERROR connecting");

  for (arg_index = 3; persist && arg_index < argc; arg_index++) {
    if (snprintf(buffer, BUFLEN, "%s\n", argv[arg_index]) >= BUFLEN) {
      fprintf(stderr, "ERROR: Command too long\n");
      continue;
    }

    if (strcmp(buffer, "disconnect\n") == 0)
      persist = 0;

    printf("Processing command: %s", buffer);
    fflush(stdout);

    process_command(buffer, sockfd);
  }

  while (persist) {
    printf("Please enter the message: ");
    fflush(stdout);

    memset(buffer, 0, BUFLEN);
    if (fgets(buffer, BUFLEN - 1, stdin) == NULL)
      break;

    if (strcmp(buffer, "disconnect\n") == 0)
      persist = 0;

    process_command(buffer, sockfd);
  }

  close(sockfd);
  return EXIT_SUCCESS;
}
