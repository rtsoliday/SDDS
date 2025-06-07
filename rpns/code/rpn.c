/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* prototypes for this file are in rpn2.prot */
/* program: rpn
 * purpose: rpn calculator
 *
 * Michael Borland
 */
#include "rpn_internal.h"

#ifdef VAX_VMS
#include <unixlib.h>
#endif

int main(int argc, char **argv)
{
    long i, return_code;
    char *ptr;
    static char *input;
    static char *rpn_defns;

#ifdef VAX_VMS
    /* initialize collection of computer usage statistics--required by
     * user-callable function 'rs'
     */
    init_stats();
#endif

    puts("Welcome to rpn version 6, by Michael Borland and Robert Soliday (June 1999).");

    /* sort the command table for faster access */
    /*qsort(func, NFUNCS, sizeof(struct FUNCTION), func_compare); */
    qsort(funcRPN, sizeof(funcRPN)/sizeof(funcRPN[0]), sizeof(struct FUNCTION), func_compare);
    /* initialize stack pointers--empty stacks */
    stackptr = 0;
    sstackptr = 0;
    lstackptr = 0;
    astackptr = 0;
    dstackptr = 0;
    astack = NULL;
    udf_stackptr = 0;
    max_udf_stackptr = 0;
    udf_stack = NULL;
    udf_cond_stackptr = 0;
    max_udf_cond_stackptr = 0;
    udf_cond_stack = NULL;
    udf_id = NULL;	
    udf_unknown = NULL;

    /* The first item on the command input stack is the standard input.
     * Input from this source is echoed to the screen. */
    istackptr = 1;
    input_stack[0].fp = stdin;
    input_stack[0].filemode = ECHO;

    /* Initialize variables use in keeping track of what 'code' is being
     * executed.  code_ptr is a global pointer to the currently used
     * code structure.  The code is kept track of in a linked list of
     * code structures.
     */
    code_ptr = &code;
    input = code_ptr->text = tmalloc(sizeof(*(code_ptr->text))*CODE_LEN);
    code_ptr->position = 0;
    code_ptr->token = NULL;
    code_ptr->storage_mode = STATIC;
    code_ptr->buffer = tmalloc(sizeof(*(code_ptr->buffer))*LBUFFER);
    code_ptr->pred = code_ptr->succ = NULL;
    code_lev = 1;

    /* Initialize array of IO file structures.  Element 0 is for terminal
     * input, while element 1 is for terminal output.
     */
    for (i=0; i<FILESTACKSIZE; i++)
        io_file[i].fp = NULL;
    io_file[0].fp = stdin;
    cp_str(&(io_file[0].name), "stdin");
    io_file[0].mode = INPUT;
    io_file[1].fp = stdout;
    cp_str(&(io_file[1].name), "stdout");
    io_file[1].mode = OUTPUT;

    /* initialize variables for UDF storage */
    udf_changed = num_udfs = max_udfs = 0;
    udf_list = NULL;

    /* Initialize flags for user memories */
    n_memories = memory_added = 0;

    /* If there are arguments push them onto the input stack
     * so that it will be run to set up the program.
     */
    while (argc-- >= 2) {
        input_stack[istackptr].fp = fopen_e(argv[argc], "r", 0);
        input_stack[istackptr++].filemode = NO_ECHO;
        }

    if ((rpn_defns=getenv("RPN_DEFNS")) && (long)strlen(rpn_defns)>0 ) {
        /* push rpn definitions file onto top of the stack */
        input_stack[istackptr].fp = fopen_e(rpn_defns, "r", 0);
        input_stack[istackptr++].filemode = NO_ECHO;
        }

    /* This is the main loop. Code is read in and executed here. */
    while (istackptr!=0) {
        /* istackptr-1 gives index of most recently pushed input file. */
        /* This loop implements the command input file stacking. */
#ifdef DEBUG
        fprintf(stderr, "istackptr = %ld\n", istackptr);
#endif
        while (prompt("rpn> ", !(istackptr-1)),
                ptr=fgets((code_ptr->text=input), CODE_LEN,
                input_stack[istackptr-1].fp)) {
            /* Loop while there's still data in the (istackptr-1)th file. *
             * The data is put in the code list.                          */
#ifdef DEBUG
            fprintf(stderr, "input string: >%s<\n", ptr);
#endif
            /* If we are at the terminal input level and a UDF has been changed
             * or a memory added, relink the udfs to get any references to the
             * new udf or memory translated into 'pcode'.
             */
            if ((udf_changed) || memory_added) {
#ifdef DEBUG
                fputs("re-linking udfs", stderr);
#endif
                link_udfs();
                udf_changed = memory_added = 0;
                }
            code_ptr->position = 0;

            /* Get rid of new-lines in data from files, and echo data to  *
             * screen if appropriate.                                     */
            if (istackptr!=1 && ptr!=NULL) {
#ifdef DEBUG
                fputs("truncating input line", stderr);
#endif
                chop_nl(ptr);
                if (input_stack[istackptr-1].filemode==ECHO)
                    puts(ptr);
                }

            /* Check for and ignore comment lines. */
#ifdef DEBUG
            fputs("checking for comment line", stderr);
#endif
            if (strncmp(ptr, "/*", 2)==0)
                continue;

            /* Finally, push input line onto the code stack & execute it.   */
#ifdef DEBUG
            fputs("pushing onto stack and executing", stderr);
#endif
            return_code = execute_code();
	    cycle_counter = 0;

            if (code_lev!=1) {
                fputs("error: code level on return from execute_code is not 1\n", stderr);
                exit(1);
                }
            /* Reset pointers in the current code structure to indicate that the
             * stuff has been executed.
             */
#ifdef DEBUG
            fputs("reseting pointers", stderr);
#endif
            *(code_ptr->text) = 0;
            code_ptr->position = 0;

            /* If it's appropriate to print the top of the numeric or logical *
             * stacks, do so here.                                            */
            if (stackptr>=1 && return_code==NUMERIC_FUNC )
                printf(choose_format(format_flag, stack[stackptr-1]),
                                    ' ', stack[stackptr-1], '\n');
            if (lstackptr>=1 && return_code==LOGICAL_FUNC)
                printf("%s\n", (logicstack[lstackptr-1])?"true":"false");
            }

        /* Close the current input file and go to the one below it on the *
         * stack.  This constitutes popping the command input stack.      *
         */
#ifdef DEBUG
        fputs("closing input file", stderr);
#endif
        fclose(input_stack[--istackptr].fp);
        }
    return(0);
    }

