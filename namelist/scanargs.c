/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: scanargs.c
 * contents: scanargs(), scanargsg(), parseList(), match_string()
 * purpose: argument parsing, I/O redirection, filename "globbing"
 *
 * keyword syntax:
 *     -key[=item1[,item2...]]
 *     -key[:item1[,item2...]]
 *     -key[,item1[,item2...]]
 *     
 *
 * I/O redirection:
 *     >output_file    data writen to stdout goes to file
 *     <input_file     data read from stdin comes from file
 *    >>output_file    data writen to stdout goes to end of file
 *
 * Michael Borland, 1986, 1990, 1994, 1995
 */
#include "scan.h"
#include "mdb.h"
#if defined(_WIN32)
#include <io.h>
#endif

#define DEBUG 0
#define COMMAS_SEPARATE_FILENAMES 0

#ifdef VAX_VMS
/* check for VAX/VMS wildcards */
#define HAS_WILDCARDS(s) (strchr(s, '*')!=NULL || strchr(s, '%')!=NULL)
#endif
#ifdef UNIX
#define HAS_WILDCARDS(s) (strchr(s, '*')!=NULL || strchr(s, '?')!=NULL)
#endif

int parseList(char ***list, char *string);
long add_file_arguments(int argc, char **argv, char ***argvNew);
int parse_string(char ***list, char *string);

int scanargs(SCANNED_ARG **scanned, int argc, char **argv)
{
    SCANNED_ARG *sc_arg_ptr;
    int i, i_store, argLimit;
    char *ptr, *arg;
    char **argvNew;
    void prompt_for_arguments(int *argc, char ***argv);
#if !defined(UNIX) && !defined(_WIN32) && !defined(vxWorks)
    FILE *fp;
#endif
    argvNew = NULL;
    
#if DEBUG
    printf("%ld argv strings:\n", argc);
    for (i=0; i<argc; i++)
        printf("%ld: %s\n", i, argv[i]);
#endif

    sc_arg_ptr = tmalloc((unsigned)argc*sizeof(*sc_arg_ptr));
    *scanned = sc_arg_ptr;
    arg = tmalloc(sizeof(*arg)*(argLimit=1024));
#if 0
    ptr = 2000; /*this is useless statement to force the compiler to allocate memory for ptr.*/
#endif
    for (i=i_store=0; i<argc; i++) {
      if ((long)strlen(argv[i])>argLimit-1)
	arg = trealloc(arg, sizeof(*arg)*(argLimit=2*strlen(argv[i])));
      strcpy(arg, argv[i]);
      interpret_escapes(arg);
      if (arg[0]=='-') {
	/* it's an option or switch: "-key[=item1[,item2]...]".  / may be subsituted
	 * for -, and : or , may be substituted for = */
	sc_arg_ptr[i_store].arg_type = OPTION;
	ptr = arg;
	while (*ptr && (*ptr!='=' && *ptr!=':' && *ptr!=','))
	  ptr++;
	if (*ptr=='=' || *ptr==':' || *ptr==',') {
	  /* there's a list of items. separate list into an array
	   * with the key in the first position */
	  *ptr=',';
	  sc_arg_ptr[i_store].n_items = parseList(&(sc_arg_ptr[i_store].list), arg+1);
	}
	else if (ptr-arg>1) {
	  /* no list, just scan key */
	  sc_arg_ptr[i_store].n_items = parseList(&(sc_arg_ptr[i_store].list), arg+1);
	}
	else {
	  sc_arg_ptr[i_store].n_items = 0;
	  sc_arg_ptr[i_store].list = tmalloc(sizeof(*sc_arg_ptr[i_store].list)*1);
	  sc_arg_ptr[i_store].list[0] = tmalloc(sizeof(**sc_arg_ptr[i_store].list));
	  sc_arg_ptr[i_store].list[0][0] = 0;
	}
	i_store++;
      }
#if !defined(UNIX) && !defined(_WIN32) && !defined(vxWorks)
      /* Set up IO redirection for non-UNIX platforms.
       * Used to work on VMS, still may...
       */
      else if (arg[0]=='>') {
	/* output redirection */
	if (arg[1]=='>') {
	  if ((fp=fopen(arg+2, "a+"))==NULL) {
	    printf("unable to open %s for appending\n", arg+2);
	    exit(1);
	  }
	}
	else {
	  if ((fp=fopen(arg+1, "w"))==NULL) {
	    printf("unable to open %s for writing\n", arg+1);
	    exit(1);
	  }
	}
	/* for other systems, may need to make a different assignment */
	stdout = fp;
      }
      else if (arg[0]=='<') {
	/* input redirection */
	if ((fp=fopen(arg+1, "r"))==NULL) {
	  printf("unable to open %s for reading\n", arg+1);
	  exit(1);
	}
	/* for other systems, may need to make a different assignment */
	stdin = fp;
      }
#endif
      else {
	/* not an option or switch */
	sc_arg_ptr[i_store].arg_type = A_LIST;
#if COMMAS_SEPARATE_FILENAMES
	sc_arg_ptr[i_store].n_items = parseList(&(sc_arg_ptr[i_store].list), arg);
#else
	sc_arg_ptr[i_store].n_items = 1;
	sc_arg_ptr[i_store].list = tmalloc(sizeof(*sc_arg_ptr[i_store].list)*1);
	cp_str(&sc_arg_ptr[i_store].list[0], arg);
#endif
	i_store++;
      }
    }
    
    if (argvNew) {
      for (i=0; i<argc; i++) {
	if (argvNew[i]) free(argvNew[i]);
      }
      free(argvNew);
    }
    if (arg) free(arg);
    return i_store;
}


int scanargsg(SCANNED_ARG **scanned, int argc, char **argv)
{
    int i, j;
    int n_items;
#ifdef VAX_VMS
    int *origin;
#endif
    char **list;

    /* first get the command-line arguments and parse them into the
       SCANNED_ARG format */
    argc = scanargs(scanned, argc, argv);

    /* go through and find any non-option arguments with lists or wildcards */
    for (i=0; i<argc; i++) {
        if ((*scanned)[i].arg_type!=OPTION) {
            if ((*scanned)[i].n_items!=1) {
                /* turn each element of the list into a separate argument */
                list = (*scanned)[i].list;
                n_items = (*scanned)[i].n_items;
                *scanned = trealloc(*scanned, sizeof(**scanned)*(argc+n_items-1));
                for (j=argc-1; j>i; j--) {
                    (*scanned)[j+n_items-1].list = (*scanned)[j].list;
                    (*scanned)[j+n_items-1].n_items = (*scanned)[j].n_items;
                    (*scanned)[j+n_items-1].arg_type = (*scanned)[j].arg_type;
                    }
                for (j=0; j<n_items; j++) {
                    (*scanned)[i+j].arg_type = A_LIST;
                    (*scanned)[i+j].n_items  = 1;
                    (*scanned)[i+j].list     = tmalloc(sizeof(char**));
                    (*scanned)[i+j].list[0]  = list[j];
                    }
                argc += n_items-1;
                }
#ifdef VAX_VMS
            if (HAS_WILDCARDS((*scanned)[i].list[0])) {
                list = wild_list(&n_items, &origin, (*scanned)[i].list, 1);
                *scanned = trealloc(*scanned, sizeof(**scanned)*(argc+n_items-1));
                for (j=argc-1; j>i; j--) {
                    (*scanned)[j+n_items-1].list = (*scanned)[j].list;
                    (*scanned)[j+n_items-1].n_items = (*scanned)[j].n_items;
                    (*scanned)[j+n_items-1].arg_type = (*scanned)[j].arg_type;
                    }
                for (j=0; j<n_items; j++) {
                    (*scanned)[i+j].arg_type = A_LIST;
                    (*scanned)[i+j].n_items  = 1;
                    (*scanned)[i+j].list     = tmalloc(sizeof(char**));
                    (*scanned)[i+j].list[0]  = list[j];
                    }
                argc += n_items-1;
                i    += n_items-1;
                }
#endif
            }
        }
    return(argc);
    }

#define ITEMS_BUFSIZE 10

int parseList(char ***list, char *string)
{
    static char **items = NULL;
    char *ptr, *ptr1, *ptr2, last_char;
    int i, n_items, depth;
    static int items_max = 0;

    n_items = 0;

    if (*(ptr = string)==0) {
        *list = 0;
        return(0);
        }
    do {
        ptr1 = ptr;
#if DEBUG
        printf("ptr1 = \"%s\"\n", ptr1);
#endif
        while (*ptr1 && !(*ptr1==',' && (ptr1==ptr || *(ptr1-1)!='\\'))) {
          if ((*ptr1=='=' && *(ptr1+1)=='(') || (*ptr1=='(' && (ptr1==ptr || *(ptr1-1)!='\\'))) {
            /* if this option contains =( or (, then search for matching ), and remove the boundary parentheses */
            if (*ptr1=='=') {
              ptr2 = ptr1 + 1;
            } else {
              ptr2 = ptr1;
            }
            ptr1 = ptr2 + 1;
            depth = 1;
            while (*ptr1 && depth) {
              if (*ptr1=='(' && *(ptr1-1)!='\\') {
                depth += 1;
              } else if (*ptr1==')' && *(ptr1-1)!='\\')
                depth -= 1;
              ptr1++;
            }
            if (depth==0) {
              if (*ptr1==',' || *ptr1==0) {
                strslide(ptr1-1, -1);
                strslide(ptr2, -1);
                ptr1 -= 2;
              }
            }
          } else {
            ptr1++;
          }
        }
        
        last_char = *ptr1;
        *ptr1 = 0;
        if (n_items>=items_max)
            items = trealloc(items, sizeof(*items)*(items_max+=ITEMS_BUFSIZE));
#if DEBUG
        printf("item = \"%s\"\n", ptr);
#endif
        items[n_items++] = ptr;
        if (last_char)
            ptr = ptr1+1;
        } while (*ptr && last_char);
    if (last_char==',') 
        items[n_items++] = ptr;

    *list = tmalloc((unsigned)sizeof(ptr)*n_items);
    for (i=0; i<n_items; i++) {
        ptr = items[i];
        while (*ptr) {
            if (*ptr=='\\' && (*(ptr+1)==',' || *(ptr+1)=='"' || *(ptr+1)=='(' || *(ptr+1)==')'))
                strcpy_ss(ptr, ptr+1);
            ptr++;
            }
        *(*list+i) = tmalloc((unsigned)strlen(items[i])+1);
        strcpy(*(*list+i), items[i]);
        }
    return(n_items);
    }

void prompt_for_arguments(int *argc, char ***argv)
{
    char *ptr, **cmd_line_arg;
    char *ptr1, *ptr2;
    char buffer[1024];
    int maxargs, i;

    /* copy command-line arguments into new argv array */
    cmd_line_arg = *argv;
    *argv = tmalloc(sizeof(**argv)*(maxargs=(*argc>10?*argc:10)));
    for (i=0; i<*argc; i++)
        (*argv)[i] = cmd_line_arg[i];
    tfree(cmd_line_arg);

    do {
        fgets(buffer, 1024, stdin);
        buffer[strlen(buffer)-1] = 0;
        while ((ptr=get_token_tq(buffer, " ", " ", "\"", "\""))) {
            if (*ptr=='&')
                break;
            ptr1 = ptr2 = ptr;
            while (*ptr1) {
                if (*ptr1=='"') {
                    if (*(ptr1+1)=='"') {
                        ptr1++;
                        while (*ptr1=='"')
                            ptr1++;
                        }
                    else {
                        while (*ptr1!='"')
                            *ptr2++ = *ptr1++;
                        ptr1++;
                        }
                    }
                else
                    *ptr2++ = *ptr1++;
                }
            *ptr2 = 0;
            if (*argc==maxargs)
                *argv = trealloc(*argv, sizeof(**argv)*(maxargs+=10));
            (*argv)[(*argc)++] = ptr;
            }
        } while (ptr && *ptr=='&');
    }

long processPipeOption(char **item, long items, unsigned long *flags)
{
    char *keyword[2] = {"input", "output"};
    long i;

    *flags = 0;
    if (items<1) {
        *flags = USE_STDIN+USE_STDOUT;
        return 1;
        }
    for (i=0; i<items; i++) {
        switch (match_string(item[i], keyword, 2, 0)) {
          case 0:
            *flags |= USE_STDIN;
            break;
          case 1:
            *flags |= USE_STDOUT;
            break;
          default:
            return 0;
            }
        }
    return 1;
    }

void free_scanargs(SCANNED_ARG **scanned, int argc)
{
  int i, i_store;
  if (*scanned) {
    for (i_store=0; i_store<argc; i_store++) {
      if ((*scanned)[i_store].list) {
	for (i=0; i<(*scanned)[i_store].n_items; i++) {
	  if ((*scanned)[i_store].list[i]) {
	    free((*scanned)[i_store].list[i]);
	    (*scanned)[i_store].list[i] = NULL;
	  }
	}
	free((*scanned)[i_store].list);
	(*scanned)[i_store].list = NULL;
      }
    }
    free(*scanned);
    *scanned = NULL;
  }
}

/* Simulates command line argument parseing */
int parse_string(char ***list, char *string)
{
  char *ptr=NULL, *ptr1, *ptr2, *buffer;
  int n_items=0;
  
  cp_str(&buffer, string);
  *list = NULL;
  do {
    if (ptr) free(ptr);
    ptr = NULL;
    while ((ptr=get_token_tq(buffer, " ", " ", "\"", "\""))) {
      if (*ptr=='&')
	break;
      ptr1 = ptr2 = ptr;
      while (*ptr1) {
	if (*ptr1=='"') {
	  if (*(ptr1+1)=='"') {
	    ptr1++;
	    while (*ptr1=='"')
	      ptr1++;
	  }
	  else {
	    while (*ptr1!='"')
	      *ptr2++ = *ptr1++;
	    ptr1++;
	  }
	}
	else
	  *ptr2++ = *ptr1++;
      }
      *ptr2 = 0;
      *list = trealloc(*list, sizeof(**list)*(n_items + 1));
      cp_str(&(*list)[n_items], ptr);
      /*   (*list)[n_items] = ptr;*/
      n_items++;
      if (ptr) free(ptr);
      ptr = NULL;
    }
  } while (ptr && *ptr=='&');
  if (ptr) free(ptr);
  ptr = NULL;
  if (buffer) free(buffer);
  return(n_items);
}

