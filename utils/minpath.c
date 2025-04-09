
/* Copyright 1994 by Michael Borland and Argonne National Laboratory,
 * all rights reserved.
 */
/* program: minpath
 * purpose: take a UNIX path and eliminate duplicates
 * M. Borland, 1993
 */
#include "mdb.h"
#include "match_string.h"

int main(int argc, char **argv) {
  char **element = NULL;
  long n_elements = 0, i;
  char *path, *null, *ptr;
  char *newpath;

  if (argc == 1) {
    if (!(path = getenv("PATH")))
      bomb("couldn't get path", NULL);
  } else if (argc == 2) {
    path = getenv(argv[1]);
  } else {
    fputs("usage: minpath [<path>]\nUses PATH environment variable by default.\n", stderr);
    exit(1);
  }

  newpath = tmalloc(sizeof(*newpath) * (strlen(path) + 1));
  null = path;
#ifdef _WIN32
  while ((ptr = strtok(null, ";"))) {
#else
    while ((ptr = strtok(null, ":"))) {
#endif
      null = NULL;
      if (n_elements == 0 || match_string(ptr, element, n_elements, EXACT_MATCH) < 0) {
        element = trealloc(element, sizeof(*element) * (n_elements + 1));
        cp_str(element + n_elements, ptr);
        n_elements++;
      }
    }

    for (i = 0; i < n_elements; i++) {
      strcat(newpath, element[i]);
#ifdef _WIN32
      strcat(newpath, ";");
#else
      strcat(newpath, ":");
#endif
    }
    puts(newpath);
    return (0);
  }
