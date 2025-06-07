/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


#include "mdb.h"
#include "rpn_internal.h"

int main(int argc, char **argv) {
    static char pfix[IFPF_BUF_SIZE];
    
    if (argc != 2) {
      fprintf(stderr, "usage: if2pf <equation>\n\n");
      return(1);
    }
    if (if2pf(pfix, argv[1], sizeof pfix)) {
      return(1);
    }
    printf("Converted to: %s\n", pfix);
    return(0);
}

