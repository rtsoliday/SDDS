
/* Copyright 1994 by Michael Borland and Argonne National Laboratory,
 * all rights reserved.
 */
/* program: echo
 * purpose: echo arguments, with interpretation of escape sequences
 *
 * Michael Borland, 1989
 */
#include "mdb.h"

int main(argc, argv)
     int argc;
     char **argv;
{
  while (--argc) {
    interpret_escapes(*++argv);
    fputs(*argv, stdout);
  }
  return (0);
}
