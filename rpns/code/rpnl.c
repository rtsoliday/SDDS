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
 Revision 1.6  2003/08/28 20:49:12  soliday
 Cleaned up the code.

 Revision 1.5  2002/08/14 16:29:00  soliday
 Added Open License

 Revision 1.4  1999/06/01 14:40:10  soliday
 Removed warnings when compiled under Linux.

 Revision 1.3  1998/08/11 18:27:58  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 * Revision 1.2  1995/09/05  21:22:43  saunders
 * First test release of the SDDS1.5 package.
 *
*/

#include "mdb.h"
#include "rpn_internal.h"

int main(argc, argv)
int argc;
char **argv;
{
    double rpn(), result=0;
    char *format, *defns;

    rpn(defns=getenv("RPN_DEFNS"));

    if (!(format=getenv("RPNL_FORMAT")))
        cp_str(&format, "%.15lg");

    while (--argc>0) {
        interpret_escapes(*++argv);
        if (strncmp("-s", *argv, 2)==0) {
          char buffer[16384], token[16384];
          long spos, count;
          if (sscanf(*argv, "-s%ld", &count)!=1)
            count = -1; /* read until nothing left */
          while (count--!=0 && fgets(buffer, 16384, stdin)) {
            spos = 0;
            while (get_token_rpn(buffer, token, 16384, &spos))
              result = rpn(token);
          }
        } else
          result = rpn(*argv);
    }
    if (stackptr>0) {
      printf(format, result);
      putchar('\n');
    }
    if (rpn_check_error())
        return(1);
    else
        return(0);
    }

