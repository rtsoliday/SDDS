/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 $Log: not supported by cvs2svn $
 Revision 1.8  2000/04/13 16:22:16  soliday
 Moved some WIN32 stuff to mdb.h

 Revision 1.7  1999/12/21 17:51:53  soliday
 vxWorks does not support popen

 Revision 1.6  1999/12/15 18:59:25  soliday
 Added support for vxWorks.

 Revision 1.5  1999/07/08 22:27:36  soliday
 Added definitions for sleep and popen for WIN32

 Revision 1.4  1999/06/01 14:39:26  soliday
 Removed warnings when compiled under Linux.

 Revision 1.3  1998/08/11 18:27:56  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 * Revision 1.2  1995/09/05  21:22:29  saunders
 * First test release of the SDDS1.5 package.
 *
*/

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

