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
 Revision 1.14  2009/10/29 19:53:20  soliday
 Renamed a few global variables that conflicted with MADX.

 Revision 1.13  2003/07/09 19:17:38  soliday
 Fixed issues with the dstack and dstackptr

 Revision 1.12  2003/03/17 23:26:43  borland
 Fixed bugs in previous changes.

 Revision 1.11  2003/03/17 22:19:02  borland
 Fixed bugs in previous changes. Needs to be checked for memory leaks
 in new code.

 Revision 1.10  2003/03/17 20:10:41  borland
 Added mudf user-callable function to Make UDF from string stack.
 Added simpson user-callable function to do simpson's rule integration.
 Added rpn_internal() to allow evaluating rpn expressions from inside
 interpreter subroutines.

 Revision 1.9  2002/12/17 18:17:49  soliday
 Improved the error message when RPN_DEFNS is not set.

 Revision 1.8  2002/08/14 16:29:00  soliday
 Added Open License

 Revision 1.7  1999/07/02 16:40:56  borland
 Fixed bug with closing of files.

 Revision 1.6  1999/06/01 14:39:57  soliday
 Removed warnings when compiled under Linux.

 Revision 1.5  1998/08/11 18:27:58  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 * Revision 1.4  1996/04/02  17:02:36  borland
 * Removed obsolete check on array stack overflow.
 *
 * Revision 1.3  1995/10/07  22:45:18  borland
 * Modified UDF code to use a sorted list plus an index.  Unfortunately, this
 * doesn't seem to have improved performance.
 *
 * Revision 1.2  1995/09/05  21:22:39  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* prototypes for this file are in rpn_sub.prot */
/* program: rpn
 * purpose: rpn calculator--as a subroutine
 *
 * Michael Borland
 */
#include "rpn_internal.h"

double rpn_internal(char *expression)
{
  double value;
  long cycle_counter_stop0;
  char *expressionCopy;

  /* this is necessary to prevent UDF processing problems
   */
  cycle_counter_stop0 = cycle_counter_stop;
  cycle_counter_stop = cycle_counter;

  cp_str(&expressionCopy, expression);
#ifdef DEBUG
  fprintf(stderr, "rpn_internal: executing %s\n", expression);
#endif

  push_code(expressionCopy, STATIC);
  execute_code();
  free(expressionCopy);

#ifdef DEBUG
  fprintf(stderr, "done\n");
#endif

  value = pop_num();

#ifdef DEBUG
  fprintf(stderr, "value = %e\n", value);
#endif

  cycle_counter_stop = cycle_counter_stop0;
  return value;
}

double rpn(char *expression)
{
    static long i;
    static char *ptr;
    static char *input, *rpn_defns;
    static long initial_call = 1;

    if ((expression != NULL) && (strlen(expression)>CODE_LEN)) {
      fprintf(stderr, "error: expression too long (%ld characters) for RPN module. Increase CODE_LEN and recompile.\n",
	      (long)(strlen(expression)));
      abort();
    }

    if (initial_call) {
        initial_call = 0;

#ifdef VAX_VMS
        /* initialize collection of computer usage statistics--required by
         * user-callable function 'rs'
         */
        init_stats();
#endif

#ifdef USE_GSL
        gsl_set_error_handler_off();
#endif

        /* sort the command table for faster access */
        qsort(funcRPN, NFUNCS, sizeof(struct FUNCTION), func_compare);

        /* initialize stack pointers--empty stacks */
        stackptr = 0;
        dstackptr = 0;
        sstackptr = 0;
        lstackptr = 0;
        astackptr = 0;
	    udf_stackptr = 0;
	    max_udf_stackptr = 0;
        astack = NULL;
	    udf_stack = NULL;
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

        /* If there was an argument (filename), push it onto the input stack
         * so that it will be run to set up the program.
         */
        if (expression) {
          if ((input_stack[istackptr].fp = fopen_e(expression, "r", 1))==NULL) {
            fprintf(stderr, "ensure the RPN_DEFNS environment variable is set\n");
            exit(1);
          }
          input_stack[istackptr++].filemode = NO_ECHO;
        }
        else if ((rpn_defns=getenv("RPN_DEFNS"))) {
            /* check environment variable RPN_DEFNS for setup file */
            cp_str(&rpn_defns, getenv("RPN_DEFNS"));
            if (strlen(rpn_defns)) {
                input_stack[istackptr].fp = fopen_e(rpn_defns, "r", 0);
                input_stack[istackptr++].filemode = NO_ECHO;
                }
            }
        expression = NULL;

        /* end of initialization section */
        }
    else
        istackptr = 1;

    /* check the stacks for overflows */

    if (stackptr>=STACKSIZE-1) {
        fprintf(stderr, "error: numeric stack size overflow (rpn).\n");
        abort();
        }
/*
    if (astackptr>=STACKSIZE-1) {
        fprintf(stderr, "error: array stack size overflow (rpn).\n");
        abort();
        }
*/
    if (sstackptr>=STACKSIZE-1) {
        fprintf(stderr, "error: string stack size overflow (rpn).\n");
        abort();
        }
    if (lstackptr>=LOGICSTACKSIZE-1) {
        fprintf(stderr, "error: logic stack size overflow (rpn).\n");
        abort();
        }


    /* This is the main loop. Code is read in and executed here. */
    while (istackptr!=0) {
        /* istackptr-1 gives index of most recently pushed input file. */
        /* This loop implements the command input file stacking. */
        while (istackptr>0 &&
               (ptr=((istackptr-1)?fgets((code_ptr->text=input), CODE_LEN,
                                     input_stack[istackptr-1].fp)
                                 :(expression?strcpy(code_ptr->text,expression):NULL) )) ) {
            /* Loop while there's still data in the (istackptr-1)th file. *
             * istackptr=1 corresponds to the expression passed.          *
             * The data is put in the code list.                          */

            /* If we are at the terminal input level and a UDF has been changed
             * or a memory added, relink the udfs to get any references to the
             * new udf or memory translated into 'pcode'.
             */
            if ((istackptr==1 && udf_changed) || memory_added) {
                link_udfs();
                udf_changed = memory_added = 0;
                }
            code_ptr->position = 0;

            /* Get rid of new-lines in data from files */
            if (istackptr!=1 && ptr!=NULL) {
                chop_nl(ptr);
                }

            /* Check for and ignore comment lines. */
            if (strncmp(ptr, "/*", 2)==0)
                continue;

            /* Finally, push input line onto the code stack & execute it.   */
            execute_code();
            if (code_lev!=1) {
                fputs("error: code level on return from execute_code is not 1\n\n", stderr);
                exit(1);
                }
            /* Reset pointers in the current code structure to indicate that the
             * stuff has been executed.
             */
            *(code_ptr->text) = 0;
            code_ptr->position = 0;

            expression = NULL;
            }

        /* Close the current input file and go to the one below it on the *
         * stack.  This constitutes popping the command input stack.      *
         */
        if (istackptr>1)
            fclose(input_stack[--istackptr].fp);
        else
            istackptr--;
        }

    /* check the stacks for overflows */
    if (stackptr>=STACKSIZE-1) {
        fprintf(stderr, "error: numeric stack size overflow (rpn).\n");
        abort();
        }
/*
    if (astackptr>=STACKSIZE-1) {
        fprintf(stderr, "error: array stack size overflow (rpn).\n");
        abort();
        }
*/
    if (sstackptr>=STACKSIZE-1) {
        fprintf(stderr, "error: string stack size overflow (rpn).\n");
        abort();
        }
    if (lstackptr>=LOGICSTACKSIZE-1) {
        fprintf(stderr, "error: logic stack size overflow (rpn).\n");
        abort();
        }

    if (stackptr>0)
        return(stack[stackptr-1]);
    return(0.0);
    }


