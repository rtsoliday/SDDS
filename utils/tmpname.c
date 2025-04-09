
/* Copyright 1994 by Michael Borland and Argonne National Laboratory,
 * all rights reserved.
 */
/* program: tmpname.c
 * purpose: return the name of a temporary file
 *
 * Michael Borland, 1991.
 */
#include "mdb.h"
#include "scan.h"
#ifndef _WIN32
#  include <unistd.h>
#endif

#define SET_PREFIX 0
#define SET_EXTENSION 1
#define SET_POSTFIX 2
#define SET_DIGITS 3
#define SET_PAUSE 4
#define N_OPTIONS 5

char *option[N_OPTIONS] = {
  "prefix", "extension", "postfix", "digits", "pause"};

char *USAGE = "tmpname [-prefix=string] [-extension=string] [-postfix=string] [-digits=number] [-pause]\n\n\
Returns a string suitable for use as a temporary file.  The string has the form\n\
[<prefix>][time-in-seconds][<postfix>][.<extension>]\n\
By default, the prefix is \"tmp\" and the postfix and extension are blank.\n\
The default number of digits for the time is 6.\n\
To guarantee unique names, give the -pause option to force a 1 second wait.\n\
Program by Michael Borland. ("__DATE__").";

int main(int argc, char **argv) {
  int i_arg;
  SCANNED_ARG *scanned;
  char *prefix, *postfix, *exten;
  char name[100];
  long digits, t, pause;
  double factor;

  argc = scanargs(&scanned, argc, argv);
  if (argc > N_OPTIONS)
    bomb(NULL, USAGE);

  prefix = postfix = exten = NULL;
  digits = 6;
  pause = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[i_arg].list[0],
                           option, N_OPTIONS, 0)) {
      case SET_PREFIX:
        if (scanned[i_arg].n_items != 2)
          bomb("invalid -prefix syntax", NULL);
        prefix = scanned[i_arg].list[1];
        break;
      case SET_POSTFIX:
        if (scanned[i_arg].n_items != 2)
          bomb("invalid -postfix syntax", NULL);
        postfix = scanned[i_arg].list[1];
        break;
      case SET_EXTENSION:
        if (scanned[i_arg].n_items != 2)
          bomb("invalid -extension syntax", NULL);
        exten = tmalloc(sizeof(*exten) * (strlen(scanned[i_arg].list[1]) + 2));
        sprintf(exten, ".%s", scanned[i_arg].list[1]);
        break;
      case SET_DIGITS:
        if (scanned[i_arg].n_items != 2 ||
            sscanf(scanned[i_arg].list[1], "%ld", &digits) != 1 ||
            digits <= 0)
          bomb("invalid -digits syntax", NULL);
        break;
      case SET_PAUSE:
        pause = 1;
        break;
      default:
        bomb("unknown option given", USAGE);
        break;
      }
    } else
      bomb("invalid argument", USAGE);
  }

  if (!prefix)
    cp_str(&prefix, "tmp");
  if (!postfix)
    cp_str(&postfix, "");
  if (!exten)
    cp_str(&exten, "");

  factor = ipow(10.0, digits);

  do {
    t = (long)time(NULL);
    t -= (long)factor * ((long)(t / factor));
    sprintf(name, "%s%ld%s%s", prefix, t, postfix, exten);
  } while (fexists(name));
  if (pause)
    sleep(1);
  puts(name);
  return (0);
}
