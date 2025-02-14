/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* 
 * $Log: not supported by cvs2svn $
 * Revision 1.3  2003/09/02 18:08:32  soliday
 * Cleaned up code for Linux.
 *
 * Revision 1.2  2002/08/14 16:49:36  soliday
 * Added Open License
 *
 * Revision 1.1  2000/04/11 19:57:59  soliday
 * Moved from elegant
 *
 * Revision 1.1  2000/04/11 15:10:23  borland
 * First version of utility for text replacements.
 *
 *
 * program: replaceText
 * purpose: replace strings in a file.
 * a simplified variant of the replace program.
 * 
 * Michael Borland, 2000
 */
#include "mdb.h"
#include "scan.h"

#define SET_ORIGINAL 0
#define SET_REPLACEMENT 1
#define SET_VERBOSE 2
#define SET_STRINGS 3
#define SET_STDIO 4
#define SET_FILTER 5
#define SET_CONFIRM 6
#define N_OPTIONS 7

char *option[N_OPTIONS] = {
  "original", "replacement", "verbose", "strings", "stdio", "filter", "confirm",
} ;

#define USAGE "replaceText {input [output] | -stdio | -strings=string[,string...]}\n\
-original=string[,string...] -replacement=string[,string...] [-confirm]\n\
[-verbose] [-filter=string[,string...]]\n\n\
-stdio        Take input from standard input and deliver output to standard output.\n\
-strings      Perform replacement on the listed strings.\n\
-original     List of strings to replace.\n\
-replacement  Parallel list of replacements for original strings.\n\
-confirm      Interactive confirmation of individual replacements.\n\
-verbose      Report the number of replacements made.\n\
-filter       Perform replacement only on lines containing one or more of the\n\
              strings in the filter list.\n\n\
Program by Michael Borland.  (This is version 2, August 2012)."

int replace_string1(char *t, char *s, char *orig, char *repl);
long passesFilters(char *s, char **filter, long filters);
char *strextract(char *s, char start, char end, long include);
void process_include_file(char *str, FILE *out, char **oirg, char **repl, int n_pairs, char **filter, int filters, int *count);

int main(
    int argc,
    char **argv
    )
{
  char **orig, **repl, *input, *output, *ptr, **filter;
  char **string_list;
  int n_pairs, i_pair, filters;
  char lastLine0[1024], lastLine1[1024];
  char s[1024], t[1024], s0[1024], response[10];
  FILE *fpi=NULL, *fpo=NULL;
  SCANNED_ARG *s_arg;
  int i_arg, *count, n_strings=0, use_stdio;
  char *input_fn, *output_fn;
  int tmp_file, verbose, confirm;

  argc = scanargs(&s_arg, argc, argv);
  if (argc<2 || argc>(3+N_OPTIONS)) 
    bomb("too few or too many arguments", USAGE);

  input = output = NULL;
  orig = repl = filter =  NULL;
  n_pairs = verbose = filters = 0;
  string_list = NULL;
  use_stdio = tmp_file = confirm = 0;

  for (i_arg=1; i_arg<argc; i_arg++) {
    if (s_arg[i_arg].arg_type==OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_ORIGINAL:
        if ((i_pair=s_arg[i_arg].n_items-1)<1 ||
            (n_pairs && i_pair!=n_pairs) ||
            !(orig=s_arg[i_arg].list+1))
          bomb("invalid -original syntax", USAGE);
        n_pairs = i_pair;
        break;
      case SET_REPLACEMENT:
        if ((i_pair=s_arg[i_arg].n_items-1)<1 ||
            (n_pairs && i_pair!=n_pairs) ||
            !(repl=s_arg[i_arg].list+1))
          bomb("invalid -replacement syntax", USAGE);
        n_pairs = i_pair;
        break;
      case SET_VERBOSE:
        verbose = 1;
        break;
      case SET_STRINGS:
        if (s_arg[i_arg].n_items<2)
          bomb("invalid -strings syntax", USAGE);
        string_list = s_arg[i_arg].list+1;
        n_strings = s_arg[i_arg].n_items-1;
        break;
      case SET_STDIO:
        use_stdio = 1;
        break;
      case SET_FILTER:
        if (s_arg[i_arg].n_items<2)
          bomb("invalid -filter syntax", USAGE);
        filter = s_arg[i_arg].list+1;
        filters = s_arg[i_arg].n_items-1;
        break;
      case SET_CONFIRM:
        confirm = 1;
        break;
      default:
        bomb("unknown option given", USAGE);
        break;
      }
    }
    else {
      if (!input)
        fpi = fopen_e(input=s_arg[i_arg].list[0], "r", 0);
      else if (!output)
        fpo = fopen_e(output=s_arg[i_arg].list[0], "w", FOPEN_SAVE_IF_EXISTS);
      else
        bomb("too many filenames listed", USAGE);
    }
  }                
  
  if (!(repl && orig))
    bomb("insufficient information supplied", USAGE);
  if (!input && !string_list && !use_stdio)
    bomb("unsufficient information supplied", USAGE);

  count = tmalloc(sizeof(int)*n_pairs);
  for (i_pair=0; i_pair<n_pairs; i_pair++) {
    interpret_escapes(orig[i_pair]);
    interpret_escapes(repl[i_pair]);
    count[i_pair] = 0;
  }

  if (string_list) {
    int i_string;
    for (i_string=0; i_string<n_strings; i_string++) {
      strcpy_ss(s, string_list[i_string]);
      for (i_pair=0; i_pair<n_pairs; i_pair++) {
        count[i_pair] += replace_string1(t, s, orig[i_pair], repl[i_pair]);
        strcpy_ss(s, t);
      }
      puts(t);
    }
    exit(0);
  }

  if (input) {
    tmp_file = 0;
#ifndef VAX_VMS
    if (!output) {
      output = tmpname(NULL);
      tmp_file = 1;
      fpo = fopen_e(output, "w", 0);
    }
#else
    if (!output)
      fpo = fopen_e(output=input, "w", 0);
#endif
    
    clean_filename(input);
    cp_str(&input_fn, input);
    if ((ptr=strchr(input, '.')))
      *ptr = 0;
    
    if (output) {
      cp_str(&output_fn, output);
      clean_filename(output);
      if ((ptr=strchr(output, '.'))) 
        *ptr = 0;
    }
    else 
      output_fn = input_fn;
  }
  else {
    fpi = stdin;
    fpo = stdout;
    verbose = 0;
  }
  
  lastLine0[0] = lastLine1[0] = 0;
  while (fgets(s, 1024, fpi)) {
    strcpy_ss(lastLine1, s);
    if (!filters || passesFilters(s, filter, filters)) {
      for (i_pair=0; i_pair<n_pairs; i_pair++) {
        int changed;
        /* at the start of this loop, s is the string being edited, including previous modifications 
         * s0 is a copy of s
         */
        strcpy_ss(s0, s);
        /* edit s to replace all instances of orig[i_pair] with repl[i_pair], creating t.
         * s is corrupted !
         */
        count[i_pair] += (changed=replace_string1(t, s, orig[i_pair], repl[i_pair]));
        if (!changed) continue;
        if (confirm) {
          /* Ask user to confirm change */
          fprintf(stderr, "Old-: %s", lastLine0);
          fprintf(stderr, "Old : %s", s0);
          fprintf(stderr, "New : %s", t);
          fprintf(stderr, "Keep change? (y=yes/e=edit/<enter>=no) ");
          fgets(response, 10, stdin);
          if (response[0]=='y' || response[0]=='Y') {
            /* Update s to contain newest changes */
            strcpy_ss(s, t);
          } else if (response[0]=='e' || response[0]=='E') {
            /* Give user opportunity to change the string */
            fprintf(stderr, "Please enter new string:");
            fgets(s, 1024, stdin);
          } else {
            strcpy_ss(s, s0);
          }
        } else if (changed) {
          strcpy_ss(s, t);
        }
      }
      if (strncmp(s,"include(",strlen("include("))==0 || strncmp(s,"sinclude(",strlen("sinclude("))==0) {
	process_include_file(s, fpo, orig, repl, n_pairs, filter, filters, count);
      } else {
	if (fputs(s, fpo)==EOF) {
	  puts("error writing to file--program aborted"); 
	  exit(1);
	}
      }
    } else {
      if (fputs(s, fpo)==EOF) {
	puts("error writing to file--program aborted"); 
	exit(1);
      }
    }
    strcpy_ss(lastLine0, lastLine1);
  }
  

  if (verbose)
    for (i_pair=0; i_pair<n_pairs; i_pair++) 
      printf("%d replacements of \"%s\" by \"%s\"\n",
             count[i_pair], orig[i_pair], repl[i_pair]);

  if (tmp_file) {
    sprintf(s, "%s~", input_fn);
    rename(input_fn, s);
    rename(output_fn, input_fn);
  }
  return 0;
}

int replace_string1(char *t, char *s, char *orig, char *repl)
{
  char *ptr0, *ptr1;
  int count, lorig;

  ptr0 = s; 
  t[0] = 0;
  count = 0;
  lorig = strlen(orig);

  while ( ((ptr1=ptr0) && orig[0]=='^' && (orig[1]==0 || strncmp(ptr0, orig+1, strlen(orig+1))==0)) || 
         (orig[0]!='^' && (ptr1=str_in(ptr0, orig))) || 
         orig[lorig-1]=='$') {
    if (orig[lorig-1]=='$') {
      if (((long)strlen(ptr0))<=(lorig-1))
        break;
      if ((ptr1=ptr0+strlen(ptr0)-(lorig-1))) {
        if (strncmp(ptr1, orig, lorig-1)!=0)
          break;
      }
      else if (lorig!=1)
        break;
    }
    if (orig[0]=='^') {
      strcat(t, repl);
      strcat(t, ptr0+lorig-1);
      return(1);
    }
    if (orig[lorig-1]=='$') {
      *ptr1 = 0;
      strcat(t, ptr0);
      strcat(t, repl);
      return(1);
    }
    count++;
    *ptr1 = 0;
    strcat(t, ptr0);
    strcat(t, repl);
    ptr0 = ptr1+lorig;
  }
  if (strlen(ptr0))
    strcat(t, ptr0);
  return(count);
}

long passesFilters(char *s, char **filter, long filters)
{
  while (filters--) {
    if (strstr(s, *filter++))
      return 1;
  }
  return 0;
}

char *strextract(char *s, char start, char end, long include)
{
  int  i, count, startCount;
  char *ptr=NULL;
  
  ptr = malloc(sizeof(*ptr)*(1+strlen(s)));
  count = startCount = 0;
  for (i=0; i<strlen(s); i++) {
    if (s[i]==start) {
      startCount =1;
      if (include) 
	ptr[count++] = s[i];
    } else if (s[i]==end) {
      if (include) 
	ptr[count++] = s[i];
      break;
    } else if (startCount)
      ptr[count++] = s[i];
  }
  ptr[count]='\0';
  return ptr;
}

void process_include_file(char *str, FILE *fpo, char **orig, char **repl, int  n_pairs, char **filter, int filters, int *count)
{
  long i_pair;
  char s[1024], t[1024], *filename=NULL;
  FILE *finclude=NULL;
  
  filename = strextract(str, '(', ')', 0);
  if (!(finclude = fopen(filename, "r"))) {
    if (strstr(str,"sinclude(")) {
      return;
    } else {
      fprintf(stderr,"%s file does not exist, can not be included.\n", filename);
      exit(1);
    }
  }
  free(filename);
  while (fgets(s, 1024, finclude)) {
    if (!filters || passesFilters(s, filter, filters)) {
      for (i_pair=0; i_pair<n_pairs; i_pair++) {
	count[i_pair] += replace_string1(t, s, orig[i_pair], repl[i_pair]);
	strcpy_ss(s, t);
      }
    }
    if (fputs(s, fpo)==EOF) {
      puts("error writing to file--program aborted"); 
      exit(1);
    }
  }
  fclose(finclude);
}
