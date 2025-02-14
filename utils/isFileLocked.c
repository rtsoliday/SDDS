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


#define USAGE "isFileLocked <filename>\n"


int main(int argc, char **argv) {
  char *filename=NULL;
  SCANNED_ARG *s_arg;
  int i_arg;
#if defined(F_TEST)
  FILE *fp;
#endif

  argc = scanargs(&s_arg, argc, argv);
  if (argc!=2) 
    bomb("too few or too many arguments", USAGE);

  for (i_arg=1; i_arg<argc; i_arg++) {
    if (!filename)
      filename=s_arg[i_arg].list[0];
    else
      bomb("too many filenames listed", USAGE);
  }                

#if defined(F_TEST)
  if (!(fp = fopen(filename, "rb"))) {
    printf("Unable to open file.\n");
    return(0);
  }
  if (lockf(fileno(fp), F_TEST, 0)==-1) {
    fclose(fp);
    printf("Yes\n");
    return(0);
  }
  fclose(fp);
  printf("No\n");
  return(0);
#else
  printf("Unable to test file locking on this operating system.\n");
  return(0);
#endif
}
