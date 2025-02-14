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
 Revision 1.6  2002/08/14 16:29:00  soliday
 Added Open License

 Revision 1.5  1999/06/01 14:39:40  soliday
 Removed warnings when compiled under Linux.

 Revision 1.4  1998/08/11 18:27:58  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 * Revision 1.3  1996/02/06  23:24:09  borland
 * Made some format string changes to reduce the width of output lines.
 *
 * Revision 1.2  1995/09/05  21:22:37  saunders
 * First test release of the SDDS1.5 package.
 *
*/

/* prototypes for this file are in rpn_io.prot */
/* file    : rpn_io.c
 * contents: open_cominp(), open_io(), close_io()
 *           choose_format(), view(), view_top(), tsci(), viewlog(), fprf()
 *           scan(), _gets(), _puts()
 * purpose : user-callable IO routines
 *
 * Michael Borland, 1988
 */
#include "rpn_internal.h"
#include <string.h>

/* routine: open_cominp()
 * purpose: open a command input file and push the pointer onto the stack.
 *          Basically an implementation of '@'
 */

void open_cominp(void)
{
    long silent = 0;
    char *ptr;
    char *option;
    FILE *fpin;

    if ((ptr=pop_string())==NULL) {
        fputs("no filename on stack (open_cominp)\n", stderr);
        return;
        }

    if ((option=strrchr(ptr, ','))!=NULL) {
        switch (option[1]) {
            case 's':  /* silent */
                silent = 1;
                option[0] = 0;
                break;
            default:
                fprintf(stderr, "warning: unknown open_file option: %s\n", option);
                silent = 0;
                break;
            }
        }

    if ((fpin=fopen(ptr, "r"))==NULL) {
        fprintf(stderr, "error: unable to read file %s\n", ptr);
        return;
        }

    input_stack[istackptr++].fp = fpin;
    input_stack[istackptr-1].filemode  = (silent?NO_ECHO:ECHO);
    }

/* routine: open_io()
 * purpose: open a named user input or output file on a specified rpn unit.
 */

void open_io(void)
{
    char *name, *option, smode[2];
    long unit, mode;

    if ((name=pop_string())==NULL)
        return;

    mode = -1;
    if ((option=strchr(name, ','))!=NULL) {
        *option = 0;
        while (*++option) {
            switch (*option) {
                case 'r':  /* read */
                    mode = INPUT;
                    break;
                case 'w':  /* write */
                    mode = OUTPUT;
                    break;
                default:
                    fprintf(stderr, "unknown open_file option: %s\n", option);
                    rpn_set_error();
                    stop();
                    return;
                }
            }
        }
    if (mode==-1) {
        fputs("error: filename must end with mode ,w or ,r for write or read\n", stderr);
        rpn_set_error();
        stop();
        return;
        }

    unit = pop_num();
    if (unit<0 || unit>=FILESTACKSIZE) {
        fprintf(stderr, "unit number must be between %d and %d\n",
            0, FILESTACKSIZE-1);
        fputs("unit number zero is stdout\n", stderr);
        rpn_set_error();
        stop();
        return;
        }

    if (io_file[unit].fp!=NULL) {
        fprintf(stderr, "unit %ld has been opened already with file %s\n",
            unit, io_file[unit].name);
        rpn_set_error();
        stop();
        return;
        }

    if (mode==INPUT)
        strcpy(smode, "r");
    else strcpy(smode, "w");

    if ((io_file[unit].fp=fopen(name, smode))==NULL) {
        fprintf(stderr, "error: unable to open file %s for %s\n",
                name, (mode==INPUT?"reading":"writing"));
        rpn_set_error();
       stop();
       return;
       }

    io_file[unit].name = name;
    io_file[unit].mode = mode;
    }

/* routine: close_io()
 * purpose: close user IO files opened by open_io()
 */

void close_io(void)
{
    long unit;

    unit = pop_num();
    if (unit<2 || unit>=FILESTACKSIZE) {
        fprintf(stderr, "unit number out of allowed range [%d, %d]\n", 1,
                FILESTACKSIZE-1);
        rpn_set_error();
        stop();
        return;
        }

    if (io_file[unit].fp==NULL) {
        fprintf(stderr, "unit %ld is not open\n", unit);
        rpn_set_error();
        stop();
        return;
        }

    fclose(io_file[unit].fp);
    io_file[unit].fp = NULL;
    io_file[unit].name = NULL;
    io_file[unit].mode = -1;
    }

/* routine: _gets()
 * purpose: implement the user command 'gets'
 */

void rpn_gets(void)
{
    long unit;
    char s[300];

    unit = pop_num();
    if (unit<0 || unit>=FILESTACKSIZE) {
        fprintf(stderr, "unit number out of range [%d, %d]\n", 0, FILESTACKSIZE-1);
        rpn_set_error();
        stop();
        return;
        }

    if (io_file[unit].fp==NULL) {
        fprintf(stderr, "unit %ld is not open\n", unit);
        rpn_set_error();
        stop();
        return;
        }
    if (io_file[unit].mode!=INPUT) {
        fprintf(stderr, "unit %ld is not open for reading\n", unit);
        rpn_set_error();
        stop();
        return;
        }

    if (!fgets(s, 300, io_file[unit].fp)) {
        push_log(0);
        return;
        }

    push_log(1);
    s[strlen(s)-1] = 0;
    push_string(s);
    }

/* routine: scan()
 * purpose: implement the user command 'scan'
 */

void scan(void)
{
    char *string, *copy;
    double num;

    if ((string=pop_string())==NULL) {
        push_log(0);
        return;
        }

    cp_str(&copy, string);

    if (!get_double(&num, string)) {
        push_log(0);
        push_string(copy);
        tfree(string);
        tfree(copy);
        return;
        }

    push_num(num);
    push_string(string);
    push_log(1);
    tfree(string);
    tfree(copy);
    }

/* routine: view()
 * purpose: implement 'view' command
 */

void view(void)
{
    long i;
    if (stackptr<1) {
        fputs("stack empty\n", stderr);
        return;
        }
    fprintf(stderr, "stack: %ld items\n", stackptr);
    for (i=stackptr-1; i>=0; i--)
        fprintf(stderr, choose_format(format_flag, stack[i]), ' ', stack[i], '\n');
    }

/* routine: view_top()
 * purpose: view_top command (not used)
 */

void view_top(void)
{
    if (stackptr<1) {
        fputs("stack empty (view_top)\n", stderr);
        return;
        }
    fprintf(stderr, choose_format(format_flag, stack[stackptr-1]), ' ',
                stack[stackptr-1], '\n');
    }

/* routine: tsci()
 * purpose: user command 'tsci'
 */

void tsci(void)
{
    if (format_flag==NO_SCIENTIFIC)
        format_flag = SCIENTIFIC;
    else
        format_flag = NO_SCIENTIFIC;
    }

/* routine: viewlog()
 * purpose: implement 'vlog'
 */

void viewlog(void)
{
    long i;
    if (lstackptr<1) {
        fputs("stack empty\n", stderr);
        return;
        }
    fprintf(stderr, "logical stack: %ld items\n", lstackptr);
    for (i=lstackptr-1; i>=0; i--)
        fprintf(stderr, " %s\n", (logicstack[i]?"true":"false"));
    }

/* routine: fprf()
 * purpose: implement 'fprf' command
 */

void fprf(void)
{
    char *format;
    long unit;

    unit = pop_num();
    format = pop_string();
    if (format==NULL)
        return;

    if (io_file[unit].fp==NULL) {
        fprintf(stderr, "error: no file open on unit %ld\n", unit);
        rpn_set_error();
        stop();
        return;
        }
    if (io_file[unit].mode!=OUTPUT) {
        fprintf(stderr, "error: unit %ld not open for writing\n", unit);
        rpn_set_error();
        stop();
        return;
        }

    if (stackptr<1) {
        fputs("too few items on stack (fprf)\n", stderr);
        rpn_set_error();
        stop();
        return;
        }

    interpret_escapes(format);

    fprintf(io_file[unit].fp, format, stack[stackptr-1]);
    fflush(io_file[unit].fp);
    }

/* routine: view_str()
 * purpose: implement 'vstr' command
 */

void view_str(void)
{
    register long i;

    if (sstackptr<1) {
        fputs("stack empty\n", stderr);
        return;
        }
    for (i=sstackptr-1; i>=0; i--)
        fprintf(stderr, "\"%s\"\n",
            sstack[i]);
    }

/* routine: _puts()
 * purpose: implement 'puts' command
 */

void rpn_puts(void)
{
    char *string;
    long unit;

    unit = pop_num();
    string = pop_string();
    if (string==NULL)
        return;

    if (io_file[unit].fp==NULL) {
        fprintf(stderr, "error: no file open on unit %ld\n", unit);
        rpn_set_error();
        stop();
        return;
        }

    if (io_file[unit].mode!=OUTPUT) {
        fprintf(stderr, "error: unit %ld not open for writing\n", unit);
        rpn_set_error();
        stop();
        return;
        }

    interpret_escapes(string);
    fputs(string, io_file[unit].fp);
    fflush(io_file[unit].fp);
    }

/* routine: sprf()
 * purpose: implement 'sprf' command
 */

void sprf(void)
{
    char *format;
    static char buffer[1024];

    format = pop_string();
    if (format==NULL)
        return;

    if (stackptr<1) {
        fputs("too few items on stack (sprf)\n", stderr);
        rpn_set_error();
        stop();
        return;
        }

    interpret_escapes(format);

    sprintf(buffer, format, stack[stackptr-1]);
    push_string(buffer);
    }


/* routine: choose_format()
 * purpose: return a format string for use with printf that will
 *          print a floating point number in an appropriate format,
 *          as determined by the setting of flag
 */

/*
static char *formats[2] = {"%c %.15le%c", "%c %.15lf%c"};
*/
static char user_format[100] = "%c %.15le%c";
static char user_format0[100] = "%.15le";

void format(void)
{
    char *sformat;

    format_flag = USER_SPECIFIED;

    sformat = pop_string();
    if (sformat==NULL)
        return;

    strcpy(user_format0, sformat);
    sprintf(user_format, "%%c %s%%c", sformat);
    interpret_escapes(user_format);
    }

void get_format(void)
{
    if (format_flag==USER_SPECIFIED)
        push_string(user_format0);
    else
        push_string("%.15le");
    }


char *choose_format(long flag, double x)
{
    static char *formats[2] = {"%c %.15le%c", "%c %.15lf%c"};

    if (flag==USER_SPECIFIED)
        return(user_format);
    if (flag==SCIENTIFIC || (fabs(x)<1e-4 || fabs(x)>1e4))
        return(formats[0]);
    return(formats[1]);
    }


