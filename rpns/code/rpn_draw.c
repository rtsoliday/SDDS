/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* file: rpn_draw.c
 * purpose: create and manage a synchronized draw subprocess
 *
 * M. Borland, 1993
 */

#if !defined(_WIN32)
#include <unistd.h>
#endif
#include "rpn_internal.h"

#if defined(vxWorks)
#include <time.h>
#endif

void rpn_draw()
{
    static char s[1024];
    static FILE *fp = NULL;
    long n_numbers, n_strings, i;
#if defined(vxWorks)
    struct timespec rqtp;
    rqtp.tv_sec = 2;
    rqtp.tv_nsec = 0;
#endif

    if (!fp) {
        /* open a pipe and start csh, then run draw */
#if defined(vxWorks)
      fprintf(stderr, "popen is not supported in vxWorks\n");
      exit(1);
#else
      fp = popen("csh", "w");
      fprintf(fp, "draw\n");
      fflush(fp);
#endif
#if defined(vxWorks)
      nanosleep(&rqtp,NULL);
#else
      sleep(2);
#endif
    }
    
    n_numbers = 0;
    if (stackptr>=1)
        n_numbers = stack[--stackptr];
    n_strings = 1;
    if (stackptr>=1)
        n_strings += stack[--stackptr];

    s[0] = 0;
    if (n_strings>sstackptr) {
        fprintf(stderr, "error: requested number of items not present on string stack (rpn_draw)\n");
        rpn_set_error();
        stop();
        return;
        }
    for (i=0; i<n_strings; i++) {
        strcat(s, sstack[sstackptr-i-1]);
        strcat(s, " ");
        }
    sstackptr -= n_strings;

    if (n_numbers>stackptr) {
        fprintf(stderr, "error: requested number of items not present on numeric stack\n");
        rpn_set_error();
        stop();
        return;
        }
    for (i=n_numbers-1; i>=0; i--) {
        sprintf(s+strlen(s), choose_format(USER_SPECIFIED, stack[stackptr-i-1]), ' ', stack[stackptr-i-1], ' ');
        }
    stackptr -= n_numbers;

    fprintf(fp, "%s\n", s);
    fflush(fp);
    }

