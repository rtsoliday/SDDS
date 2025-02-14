/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: buffer.h
 *       proto-types for string-buffering routines
 *
 * Michael Borland, 1990
 $Log: not supported by cvs2svn $
 Revision 1.5  2002/08/14 15:40:13  soliday
 Added Open License

 Revision 1.4  1999/09/14 18:03:40  soliday
 Added export commands for WIN32 dll files.

 Revision 1.3  1999/04/14 13:58:47  borland
 Fixed some function types and returns.

 Revision 1.2  1995/09/05 21:14:52  saunders
 First test release of the SDDS1.5 package.

 */
#include <stdio.h>
#undef epicsShareFuncMDBLIB
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_MDBLIB)
#define epicsShareFuncMDBLIB  __declspec(dllexport)
#else
#define epicsShareFuncMDBLIB
#endif
#else
#define epicsShareFuncMDBLIB
#endif

epicsShareFuncMDBLIB void create_buffer(char *name, int increment);
epicsShareFuncMDBLIB void add_to_buffer(char *name, char *string);
void fputs_buffer(char *name, FILE *fp);
epicsShareFuncMDBLIB void fprintf_buffer(FILE *fp, char *format, char *name);
struct buffer_struct *find_buffer(char *name);
epicsShareFuncMDBLIB int buffer_exists(char *name);
epicsShareFuncMDBLIB void clear_buffer(char *name);


