/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* definitions for use with namelist scanning routines */
/* Michael Borland, 1988 
*/

#ifndef _NAMELIST_

#ifdef __cplusplus
extern "C" {
#endif

#define _NAMELIST_ 1

typedef struct {
    long n_entities;
    char *group_name;
    char **entity;
    long *n_subscripts;
    long **subscript;
    long *n_values;
    char ***value;
    long **repeat;
    } NAMELIST_TEXT;

typedef struct {
    char *name;
    long type;
    long n_subscripts;
    long *dimensions;
    char *root;
    char *def_root;
    long data_size;
    } ITEM;

typedef struct {
    ITEM *item_list;
    long n_items;
    char *name;
    } NAMELIST;

/* If more types are added here, the list in show_namelist_fields.c must also be updated */
#define TYPE_SHORT 1
#define TYPE_INT 2
#define TYPE_INT32_T 3
#define TYPE_LONG 4
#define TYPE_FLOAT 5
#define TYPE_DOUBLE 6
#define TYPE_STRING 7
#define TYPE_CHAR 8


long scan_namelist(NAMELIST_TEXT *nl, char *line);
char *get_namelist(char *s, long n, FILE *fp);
char *get_namelist_e(char *s, long n, FILE *fp, long *errorCode);
void free_namelist_text(NAMELIST_TEXT *nl);
long count_occurences(char *s, char c, char *end);
void un_quote(char *s);
void show_namelist(FILE *fp, NAMELIST_TEXT *nl);
void zero_namelist(NAMELIST_TEXT *nl);
void free_namelist(NAMELIST *nl);
long extract_subscripts(char *name, long **subscript);
long is_quoted(char *string, char *position, char quotation_mark);
char *next_unquoted_char(char *ptr, char c, char quote_mark);
void show_namelist_fields(FILE *fp, NAMELIST *nl, char *nl_name);
void show_namelists_fields(FILE *fp, NAMELIST **nl, char **nl_name, long n_nl);

#define STICKY_NAMELIST_DEFAULTS 0x0001
void set_namelist_processing_flags(long flags);
long process_namelists(NAMELIST **nl, char **nl_name, long n_nl, 
    NAMELIST_TEXT *nl_t);
long process_namelist(NAMELIST *nl, NAMELIST_TEXT *nl_t);

#define NAMELIST_ERROR -1
long processNamelist(NAMELIST *nl, NAMELIST_TEXT *nl_t);
long process_entity(ITEM *item, char **item_name, long n_items, 
    NAMELIST_TEXT *nl_t, long i_entity);
char *get_address(char *root, long n_subs, long *subscript, long *dimension,
    unsigned long size);
void reset_namelist_values(NAMELIST *nl);

#define PRINT_NAMELIST_NODEFAULTS 1
#define PRINT_NAMELIST_COMPACT 2
void set_print_namelist_flags(long flags);
void print_namelist(FILE *fp, NAMELIST *nl);
char *escape_quotes(char *s);

typedef char *STRING;

#define NAMELIST_NO_ERROR 0
#define NAMELIST_BUFFER_TOO_SMALL 1
#define NAMELIST_IMPROPER_CONSTRUCTION 2

#ifdef __cplusplus
}
#endif

#endif
