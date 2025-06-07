/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "mdb.h"

main(int argc, char **argv)
{
    FILE *fp;
    char s[100];

    fp = fopen_e("x11test.out", "w", 0);
    while (fgets(s, 100, stdin))
        fputs(s, fp);
    }

