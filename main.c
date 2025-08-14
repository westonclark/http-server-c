#include "stdlib.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

const char *DEFAULT_FILENAME = "index.html";
const int DEFAULT_PORT = 8080;
const int MAX_REQUEST_BYTES = 32768;

const char *ERROR_400 = "HTTP/1.1 400 Bad Request\r\n\r\n";
const char *ERROR_404 = "HTTP/1.1 404 Not Found\r\n\r\n";
const char *ERROR_500 = "HTTP/1.1 500 Internal Server Error\r\n\r\n";

int create_server_socket(int port) {

  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == -1) {
    printf("Server socket creation failed");
    return -1;
  }

  int reuse = 1;
  int reuse_option = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse,
                                sizeof(reuse));
  if (reuse_option == -1) {
    printf("Error setting reuse socket option");
    close(server_socket);
    return -1;
  }

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(port);

  int socket_binding = bind(server_socket, (struct sockaddr *)&server_address,
                            sizeof(server_address));
  if (socket_binding == -1) {
    printf("Error binding address to socket");
    close(server_socket);
    return -1;
  }

  int listening = listen(server_socket, 4);
  if (listening == -1) {
    printf("Error listening to socket connection");
    close(server_socket);
    return -1;
  }

  printf("Server listening on port %d\n", port);

  return server_socket;
}
char *get_path(char *request, char *path_buffer, size_t buffer_size) {
  char *path_start = strchr(request, ' ');
  if (!path_start)
    return NULL;
  path_start++;

  char *path_end = strchr(path_start, ' ');
  if (!path_end)
    return NULL;

  int path_length = path_end - path_start;

  int total_size = path_length;

  if (path_length > 1 && path_start[path_length - 1] != '/') {
    total_size += 1;
  }

  total_size += strlen(DEFAULT_FILENAME) + 1;

  if (total_size > buffer_size)
    return NULL;

  memcpy(path_buffer, path_start, path_length);
  int offset = path_length;

  if (path_length > 1 && path_start[path_length - 1] != '/') {
    path_buffer[offset] = '/';
    offset++;
  }

  memcpy(path_buffer + offset, DEFAULT_FILENAME, strlen(DEFAULT_FILENAME));
  path_buffer[offset + strlen(DEFAULT_FILENAME)] = '\0';

  return path_buffer[0] == '/' ? path_buffer + 1 : path_buffer;
}

int handle_request(char *request, int request_socket) {
  char path_buffer[512];
  char *path = get_path(request, path_buffer, sizeof(path_buffer));
  if (!path) {
    write(request_socket, ERROR_400, strlen(ERROR_400));
    return -1;
  }

  int file = open(path, O_RDONLY);
  if (file == -1) {
    if (errno == ENOENT) {
      write(request_socket, ERROR_404, strlen(ERROR_404));
    } else {
      write(request_socket, ERROR_500, strlen(ERROR_500));
    }
    return -1;
  }

  struct stat metadata;
  int file_metadata = fstat(file, &metadata);
  if (file_metadata == -1) {
    write(request_socket, ERROR_500, strlen(ERROR_500));
    close(file);
    return -1;
  }

  char *file_buffer = malloc(metadata.st_size + 1);
  if (file_buffer == NULL) {
    printf("Error allocating file read buffer\n");
    close(file);
    return -1;
  }

  ssize_t bytes_read = read(file, file_buffer, metadata.st_size);
  if (bytes_read == -1) {
    printf("Error reading file\n");
    close(file);
    free(file_buffer);
    return -1;
  }
  file_buffer[bytes_read] = '\0';

  char response_header[512];
  int header_length = snprintf(response_header, sizeof(response_header),
                               "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/html\r\n"
                               "Content-Length: %ld\r\n"
                               "Connection: close\r\n"
                               "\r\n",
                               bytes_read);

  ssize_t header_write = write(request_socket, response_header, header_length);
  if (header_write == -1) {
    printf("Error writing response header\n");
    close(file);
    free(file_buffer);
    return -1;
  }

  ssize_t content_written = write(request_socket, file_buffer, bytes_read);
  if (content_written == -1) {
    printf("Error writing file content\n");
    close(file);
    free(file_buffer);
    return -1;
  }

  close(file);
  free(file_buffer);
  return 1;
}

int main() {

  struct sockaddr_in client_address;
  socklen_t client_address_length = sizeof(client_address);
  char request_buffer[MAX_REQUEST_BYTES];

  int server_socket = create_server_socket(DEFAULT_PORT);
  if (server_socket == -1) {
    return 1;
  }

  while (1) {
    int request_socket =
        accept(server_socket, (struct sockaddr *)&client_address,
               &client_address_length);
    if (request_socket == -1) {
      printf("Error opening request socket connection");
      continue;
    }

    ssize_t bytes_read =
        read(request_socket, request_buffer, MAX_REQUEST_BYTES);
    if (bytes_read <= 0) {
      write(request_socket, ERROR_400, strlen(ERROR_400));
      close(request_socket);
      continue;
    }
    request_buffer[bytes_read] = '\0';

    handle_request(request_buffer, request_socket);
    close(request_socket);
  }
  return 0;
}
