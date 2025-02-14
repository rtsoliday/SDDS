/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution.
\*************************************************************************/

#include "mdb.h"
#include "scan.h"
#include <ctype.h>

char *USAGE = "\
editstring -editcommand=<string> {-stream | <string-to-edit> [<string-to-edit>] ...}\n\
editstring returns edited versions of strings, using editing\n\
sequences reminiscent of emacs.\n\
-stream    edit stream from standard input\n\
-editcommand sequences:\n\
                  <n>d   delete <n> characters\n\
                  <n>f   forward <n> characters\n\
                  <n>b   backward <n> characters\n\
                  <n>D   delete <n> words\n\
                  <n>F   forward <n> words\n\
                  <n>B   backward <n> words\n\
                     a   go to beginning of string\n\
                     e   go to end of string\n\
<n>i{delim}text{delim}   insert text <n> times\n\
r[?]{delim}text{delim}   reverse search for text, leave active position at end\n\
                         ? means that if text isn't found, stop editing\n\
R[?]{delim}text{delim}   search for text, leave active position at start\n\
                         ? means that if text isn't found, stop editing\n\
s[?]{delim}text{delim}   search for text, leave active position at end\n\
                         ? means that if text isn't found, stop editing\n\
S[?]{delim}text{delim}   search for text, leave active position at start\n\
                         ? means that if text isn't found, stop editing\n\
                  <n>k   kill <n> characters\n\
                  <n>K   kill <n> words\n\
                  z<c>   kill up to <c>\n\
               <n>Z<c>   kill up to and including <c>, <n> times\n\
   x{delim}text{delim}   kill as long as characters are in text.\n\
                         text may be a range in form [char1-char2]\n\
  x-{delim}text{delim}   kill as long as characters are not in text.\n\
                  <n>y   yank kill buffer, <n> times\n\
                     c   clear the kill buffer.\n\
 <n>%[hg]{delim}text1{delim}text2{delim}  \n\
                         replace text1 with text2 <n> times\n\
                         g means replace all occurrences (<n> is ignored).\n\
                         h means replace only if the text1 is \"here\", i.e.,\n\
                         at the active position.\n\
        <n>(<sequence>)  execute editing sequence <sequence> <n> times.\n\
\nProgram by Michael Borland.  ("__DATE__
  " "__TIME__
  ", SVN revision: " SVN_VERSION ")\n";

#define OPT_EDITCOMMAND 0
#define OPT_STREAMINPUT 1
#define N_OPTIONS 2
static char *option[N_OPTIONS] = {
  "editcommand", "stream"};

static char editbuffer[4096];

int main(int argc, char **argv) {
  SCANNED_ARG *scanned;
  long i_arg, i, strings, stream;
  char *editcommand;
  char **string;

  argc = scanargsg(&scanned, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);
  if (argc < 3)
    bomb("supply more arguments", NULL);

  string = NULL;
  strings = 0;
  editcommand = NULL;
  stream = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* process options here */
      for (i = 0; i < scanned[i_arg].n_items; i++)
        interpret_escapes(scanned[i_arg].list[i]);
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case OPT_EDITCOMMAND:
        if (scanned[i_arg].n_items != 2)
          bomb("invalid -editcommand syntax", "-editcommand=<string>");
        editcommand = scanned[i_arg].list[1];
        break;
      case OPT_STREAMINPUT:
        stream = 1;
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous keyword: %s\n", scanned[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      string = trealloc(string, sizeof(*string) * (strings + 1));
      string[strings] = scanned[i_arg].list[0];
      strings++;
    }
  }

  if (strings == 0 && !stream)
    bomb("one of -stream or strings-to-edit must be supplied", NULL);
  if (editcommand == NULL)
    bomb("-editcommand option not given", NULL);

  if (strings) {
    for (i = 0; i < strings; i++) {
      strcpy(editbuffer, string[i]);
      if (!edit_string(editbuffer, editcommand))
        fprintf(stderr, "error: problem editing string %ld\n", i);
      else
        puts(editbuffer);
    }
  } else {
    while (fgets(editbuffer, 4096, stdin)) {
      chop_nl(editbuffer);
      if (edit_string(editbuffer, editcommand))
        puts(editbuffer);
      fflush(stdout);
    }
  }
  return 0;
}
