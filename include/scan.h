/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* define structure for use with scanargs(), scanlist() */
#if !defined(SCAN_INCLUDED)
#define SCAN_INCLUDED 1

#undef epicsShareFuncMDBLIB
#undef epicsShareFuncMDBCOMMON
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_MDBLIB)
#define epicsShareFuncMDBLIB  __declspec(dllexport)
#else
#define epicsShareFuncMDBLIB
#endif
#if defined(EXPORT_MDBCOMMON)
#define epicsShareFuncMDBCOMMON  __declspec(dllexport)
#else
#define epicsShareFuncMDBCOMMON
#endif
#else
#define epicsShareFuncMDBLIB
#define epicsShareFuncMDBCOMMON
#endif

#ifdef __cplusplus 
extern "C" {
#endif

typedef struct {
    long arg_type;	/* type of argument */
    long n_items;	/* number of items in list */
    char **list;	/* the list */
    } SCANNED_ARG;

/* possible values for arg_type */
#define OPTION 		1
#define A_LIST 		2

epicsShareFuncMDBCOMMON extern int scanargs(SCANNED_ARG **scanned, int argc, char **argv);
  /*
epicsShareFuncMDBCOMMON extern void free_scanargs(SCANNED_ARG *scanned, int argc);
  */
epicsShareFuncMDBCOMMON extern void free_scanargs(SCANNED_ARG **scanned, int argc);
epicsShareFuncMDBCOMMON extern int scanargsg(SCANNED_ARG **scanned, int argc, char **argv);
extern int parseList(char ***list, char *string);
extern int parse_string(char ***list, char *string);
epicsShareFuncMDBCOMMON long processPipeOption(char **item, long items, unsigned long *flags);
#define USE_STDIN      0x0001UL
#define USE_STDOUT     0x0002UL
#define DEFAULT_STDIN  0x0004UL
#define DEFAULT_STDOUT 0x0008UL

epicsShareFuncMDBCOMMON void processFilenames(char *programName, char **input, char **output, unsigned long pipeFlags, long noWarnings, long *tmpOutputUsed);


#include <stdio.h>
#define UNPACK_REQUIRE_SDDS 0x00000001UL
#define UNPACK_USE_PIPE     0x00000002UL
long PackSuffixType(char *filename, char **unpackedName,  unsigned long mode);
epicsShareFuncMDBLIB FILE *UnpackFopen(char *filename, unsigned long mode, short *popenUsed, char **tmpFileUsed);

#include "SDDStypes.h"

epicsShareFuncMDBLIB extern long scanItemList(unsigned long *flags, char **item, long *items, unsigned long mode, ...);
epicsShareFuncMDBLIB extern long scanItemListLong(unsigned long long *flags, char **item, long *items, unsigned long mode, ...);
/* usage: scanItemList(&flags, item, &items, mode,
               <keyword>, <SDDS-type>, <pointer>, <number-required>, <set-flag>, etc.
               NULL)
 */
#define SCANITEMLIST_UNKNOWN_VALUE_OK    0x00000001UL
#define SCANITEMLIST_UNKNOWN_KEYVALUE_OK 0x00000002UL
#define SCANITEMLIST_REMOVE_USED_ITEMS   0x00000004UL
#define SCANITEMLIST_IGNORE_VALUELESS    0x00000008UL

epicsShareFuncMDBLIB extern long contains_keyword_phrase(char *string);
/* Obsolete: */
epicsShareFuncMDBLIB extern long scan_item_list(unsigned long *flags, char **item, long *items, ...);

/* usage: scan_item_list(&flags, item, &items, 
               <keyword>, <SDDS-type>, <pointer>, <number-required>, <set-flag>, etc.
               NULL)
 */

#ifdef __cplusplus
}
#endif

#endif

