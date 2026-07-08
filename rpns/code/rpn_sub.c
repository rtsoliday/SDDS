/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* prototypes for this file are in rpn_sub.prot */
/* program: rpn
 * purpose: rpn calculator--as a subroutine
 *
 * Michael Borland
 */
#include "rpn_internal.h"

static double rpn_internal_unlocked(char *expression)
{
  static RPN_THREAD_LOCAL char empty_expression[1] = "";
  struct CODE *saved_code_ptr = code_ptr;
  struct CODE *pushed_code_ptr;
  char *saved_text = saved_code_ptr ? saved_code_ptr->text : NULL;
  char *saved_token = saved_code_ptr ? saved_code_ptr->token : NULL;
  char *saved_buffer = saved_code_ptr ? saved_code_ptr->buffer : NULL;
  long saved_position = saved_code_ptr ? saved_code_ptr->position : 0;
  long saved_storage_mode = saved_code_ptr ? saved_code_ptr->storage_mode : STATIC;
  long saved_code_lev = code_lev;
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
  pushed_code_ptr = code_ptr;
  execute_code();
  if (pushed_code_ptr && pushed_code_ptr->text == expressionCopy) {
    if (saved_code_ptr && pushed_code_ptr == saved_code_ptr &&
        code_ptr == saved_code_ptr && code_lev == saved_code_lev) {
      if (!saved_buffer && code_ptr->buffer)
        saved_buffer = code_ptr->buffer;
      code_ptr->text = saved_text;
      code_ptr->position = saved_position;
      code_ptr->token = saved_token;
      code_ptr->storage_mode = saved_storage_mode;
      code_ptr->buffer = saved_buffer;
    } else {
      pushed_code_ptr->text = empty_expression;
      pushed_code_ptr->position = 0;
      pushed_code_ptr->token = NULL;
      pushed_code_ptr->storage_mode = STATIC;
    }
  }
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

double rpn_internal(char *expression)
{
  double value;

  rpn_lock();
  value = rpn_internal_unlocked(expression);
  rpn_unlock();
  return value;
}

static double rpn_unlocked(char *expression)
{
    static RPN_THREAD_LOCAL long i;
    static RPN_THREAD_LOCAL char *ptr;
    static RPN_THREAD_LOCAL char *input;
    static RPN_THREAD_LOCAL char input_buffer[CODE_LEN];
    static RPN_THREAD_LOCAL char code_buffer[LBUFFER];
    static RPN_THREAD_LOCAL long initial_call = 1;
    static long global_initial_call = 1;
    char *rpn_defns;

    if ((expression != NULL) && (strlen(expression)>CODE_LEN)) {
      fprintf(stderr, "error: expression too long (%ld characters) for RPN module. Increase CODE_LEN and recompile.\n",
	      (long)(strlen(expression)));
      abort();
    }

    if (initial_call) {
        initial_call = 0;

        /* initialize stack pointers--empty stacks */
        stackptr = 0;
        dstackptr = 0;
        sstackptr = 0;
        lstackptr = 0;
        cycle_counter = 0;
        cycle_counter_stop = 0;
        max_cycle_counter = 0;
        udf_id = NULL;

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
        input = code_ptr->text = input_buffer;
        code_ptr->position = 0;
        code_ptr->token = NULL;
        code_ptr->storage_mode = STATIC;
        code_ptr->buffer = code_buffer;
        code_ptr->pred = code_ptr->succ = NULL;
        code_lev = 1;

        /* Initialize array of IO file structures.  Element 0 is for terminal
         * input, while element 1 is for terminal output.
         */
        for (i=0; i<FILESTACKSIZE; i++)
            io_file[i].fp = NULL;
        io_file[0].fp = stdin;
        io_file[0].name = "stdin";
        io_file[0].mode = INPUT;
        io_file[1].fp = stdout;
        io_file[1].name = "stdout";
        io_file[1].mode = OUTPUT;

        /* end of initialization section */
        }
    else
        istackptr = 1;

    if (global_initial_call) {
        global_initial_call = 0;

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

        /* initialize process-global UDF, memory, and array state */
        udf_changed = num_udfs = max_udfs = 0;
        udf_list = NULL;
        udf_stackptr = 0;
        max_udf_stackptr = 0;
        udf_stack = NULL;
        udf_cond_stackptr = 0;
        max_udf_cond_stackptr = 0;
        udf_cond_stack = NULL;
        udf_unknownptr = -1;
        max_udf_unknown_counter = 0;
        udf_unknown = NULL;
        astackptr = 0;
        max_astackptr = 0;
        astack = NULL;
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
            if (strlen(rpn_defns)) {
                input_stack[istackptr].fp = fopen_e(rpn_defns, "r", 0);
                input_stack[istackptr++].filemode = NO_ECHO;
                }
            }
        expression = NULL;
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

double rpn(char *expression)
{
    double value;

    rpn_lock();
    value = rpn_unlocked(expression);
    rpn_unlock();
    return value;
    }
