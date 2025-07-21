#include "stdlib.h"
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
// #include <sys/_types/_ssize_t.h>
// #include <sys/types.h>

const char *DEFAULT_FILENAME = "index.html";

char *get_path(char *request) {
  char *path_start = strchr(request, ' ');
  if (!path_start) {
    return NULL;
  }
  path_start++;

  // skip leading "/"
  if (path_start[0] == '/') {
    path_start++;
  }

  char *path_end = strchr(path_start, ' ');
  if (!path_end) {
    return NULL;
  }

  // add a trailing "/" if it doesn't exist
  if (path_end[-1] != '/') {
    path_end[0] = '/';
    path_end++;
  }

  // exit if there is not enough space in the request string to concat filename
  if (path_end + strlen(DEFAULT_FILENAME) > request + strlen(request)) {
    return NULL;
  }
  memcpy(path_end, DEFAULT_FILENAME, strlen(DEFAULT_FILENAME) + 1);

  return path_start;
}

void print_file(const char *path) {

  // open file
  int file_desctiptor = open(path, O_RDONLY);
  if (file_desctiptor == -1) {
    printf("Error opening file %s", path);
    return;
  }

  // get file size
  struct stat metadata;
  if (fstat(file_desctiptor, &metadata) == -1) {
    printf("Error getting file stats\n");
    close(file_desctiptor);
    return;
  }

  // create a buffer
  char *file_buffer = malloc(metadata.st_size + 1);
  if (file_buffer == NULL) {
    printf("Error allocating file read buffer\n");
    free(file_buffer);
    close(file_desctiptor);
    return;
  }

  // read file contents into buffer
  ssize_t bytes_read = read(file_desctiptor, file_buffer, metadata.st_size);
  if (bytes_read == -1) {
    printf("Error reading file\n");
    close(file_desctiptor);
    free(file_buffer);
    return;
  }
  file_buffer[bytes_read] = '\0';

  // print file buffer
  printf("%s contents:\n\n%s\n", path, file_buffer);

  // close file and free buffer
  close(file_desctiptor);
  free(file_buffer);
}

int main() {
  char request[] = "GET / HTTP/1.1\nHost: example.com";
  print_file(get_path(request));

  return 0;
}
