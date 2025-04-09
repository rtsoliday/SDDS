
/* Copyright 1994 by Michael Borland and Argonne National Laboratory,
 * all rights reserved.
 */
/* program: token
 * purpose: read lines from the standard input and write the
 *          ith token of each line to the standard output
 *
 * Michael Borland, 1989
 */
#include "mdb.h"
#include "scan.h"
#include <ctype.h>

#define SET_NUMBER 0
#define SET_LAST 1
#define SET_HELP 2
#define OPTIONS 3
char *option[OPTIONS] = {
  "number", "last", "help"};
static char *USAGE = "token [{-number=<integer> | -last}] [-help]\ntoken is a filter that extracts tokens.\n\
M. Borland, February 1994\n";

#define MAXLINE 1024 * 1024

void truncate_token(char *ptr);
char *next_token(char *ptr);

int main(int argc, char **argv) {
  long i_arg, number, last, i;
  SCANNED_ARG *scanned;
  char s[MAXLINE], *ptr, *ptr1;

  argc = scanargs(&scanned, argc, argv);
  if (argc < 1)
    bomb(NULL, USAGE);

  number = last = 0;
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[i_arg].list[0], option, OPTIONS, 0)) {
      case SET_NUMBER:
        if (scanned[i_arg].n_items != 2 || sscanf(scanned[i_arg].list[1], "%ld", &number) != 1 || number < 0)
          bomb("invalid -number syntax", USAGE);
        break;
      case SET_LAST:
        last = 1;
        break;
      case SET_HELP:
        printf("usage: %s\n", USAGE);
        exit(1);
        break;
      default:
        bomb("unknown option given", USAGE);
        break;
      }
    } else {
      bomb("unknown argument given--appears to be filename", USAGE);
    }
  }

  if (!number && !last)
    number = 1;
  while (fgets(s, MAXLINE, stdin)) {
    ptr = s;
    while (isspace(*ptr))
      ptr++;
    if (number) {
      for (i = 1; i < number; i++) {
        if (!(ptr = next_token(ptr)))
          break;
      }
      if (ptr) {
        truncate_token(ptr);
        puts(ptr);
      }
    } else {
      ptr1 = ptr;
      while ((ptr = next_token(ptr)))
        ptr1 = ptr;
      ptr = ptr1;
      truncate_token(ptr);
      puts(ptr);
    }
  }
  return (0);
}

char *next_token(char *ptr) {
  while (*ptr && !isspace(*ptr))
    ptr++;
  while (isspace(*ptr))
    ptr++;
  if (*ptr)
    return (ptr);
  return (NULL);
}

void truncate_token(char *ptr) {
  while (*ptr && !isspace(*ptr))
    ptr++;
  *ptr = 0;
}
