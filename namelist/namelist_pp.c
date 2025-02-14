/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: namelist_pp
 * purpose: namelist pre-processor for C programs
 * format of namelist input:
 * #namelist namelist_name[,options...]
 *   definition_of_single_variable [={optional multiline initialization}];
 *           .
 *           .
 *           .
 * #endlist
 * Allowed types for variables are: int, short, float, double, STRING.
 *
 * Michael Borland, 1988
 */
#include "mdb.h"
#include "buffer.h"
#include "scan.h"

#define MAX_SUBSCRIPTS 100
#define SUBSCRIPT_LENGTH 20
#if defined(__TURBOC__)
#define STRING_LENGTH 256
#else
#define STRING_LENGTH 1024
#endif
#define MAX_VARIABLES 100
#define MAX_NAMELISTS 100

long get_subscripts(char subs[MAX_SUBSCRIPTS][SUBSCRIPT_LENGTH],
   long max_subs, char *variable);
char *efgets(char *s, long n, FILE *fp);
int has_semicolon(char *s);


#define USAGE "nlpp -suppressSummaryVariables inputfile[.nl] [outputfile[.h]]"
#define CLO_SUPPRESS_SUMMARY_VARS 0
#define CLO_OPTIONS 1
char *cloOption[CLO_OPTIONS] = {
  "suppressSummaryVariables",
} ;

#define SET_STATIC 0
#define SET_EXTERN 1
#define SET_STRUCT 2
#define SET_AUTO 3
#define SET_NOINIT 4
#define N_OPTIONS 5
char *option[N_OPTIONS] = {
    "static", "extern", "struct", "auto", "noinit", 
     } ;

#define BUFFER_SIZE 16384

int main(argc, argv)
int argc;
char **argv;
{
    char s[STRING_LENGTH], *ptr, *ptr1;
    char variable_name[MAX_VARIABLES][STRING_LENGTH];
    char type_name[MAX_VARIABLES][STRING_LENGTH];
    char namelist_name[MAX_NAMELISTS][STRING_LENGTH];
    long n_subscripts[MAX_SUBSCRIPTS];
    char subscript[MAX_SUBSCRIPTS][SUBSCRIPT_LENGTH];
    char input[BUFFER_SIZE], output[BUFFER_SIZE], tmp_buf[BUFFER_SIZE];
    FILE *fpi, *fpo;
    long n_lines, n_variables, n_namelists;
    long i, j, use_struct, no_init;
    char definition_prefix[BUFFER_SIZE], struct_name[BUFFER_SIZE];
    long i_arg;
    SCANNED_ARG *s_arg;
    char suppressSummaryVariables = 0;
    
    input[0] = output[0] = 0;
    suppressSummaryVariables = 0;
    
    if (argc<2 || argc>4)
        bomb(NULL, USAGE);

    argc = scanargs(&s_arg, argc, argv);
    for (i_arg=1; i_arg<argc; i_arg++) {
      if (s_arg[i_arg].arg_type==OPTION) {
        switch (match_string(s_arg[i_arg].list[0], cloOption, CLO_OPTIONS, 0)) {
        case CLO_SUPPRESS_SUMMARY_VARS:
          suppressSummaryVariables = 1;
          break;
        default:
          bomb("unknown option", USAGE);
          break;
        }
      } else {
        if (input[0]==0)
          strcpy_ss(input, s_arg[i_arg].list[0]);
        else if (output[0]==0)
          strcpy_ss(output, s_arg[i_arg].list[0]);
        else
          bomb("too many filenames", USAGE);
      }
    }

    if (input[0]==0)
      bomb("too few filenames", USAGE);

    if (output[0]==0) {
      if ((ptr=strchr(input, '.'))!=NULL) {
        *ptr = 0;
        sprintf(output, "%s.h", input);
        *ptr = '.';
      } else {
        sprintf(output, "%s.h", input);
      }
    } else {
      if ((ptr=strchr(output, '.'))==NULL)
        strcat(output, ".h");
    }
    
    fpi = fopen_e(input , "r", 0);
    fpo = fopen_e(output, "w", 0);

    n_lines = n_namelists = 0;
    while (fgets(s, STRING_LENGTH, fpi)) {
        n_lines++;
        if (s[0]!='#' || strncmp(s+1, "namelist", 8)!=0) {
            fputs(s, fpo);
            continue;
            }
        strcpy_ss(s, s+9);
        if ((ptr=get_token_buf(s, namelist_name[n_namelists], STRING_LENGTH))==NULL) {
            printf("error on line %ld: no name for namelist\n", n_lines);
            exit(1);
            }
        use_struct = no_init = 0;
        strcpy_ss(definition_prefix, "static ");
        while ((ptr=get_token_buf(s, tmp_buf, BUFFER_SIZE))) {
            if ((ptr1=strchr(ptr, '/')))
                *ptr1++ = 0;
            switch (match_string(ptr, option, N_OPTIONS, 0)) {
              case SET_STATIC:
                strcpy_ss(definition_prefix, "static ");
                break;
              case SET_EXTERN:
                strcpy_ss(definition_prefix, "extern ");
                break;
              case SET_STRUCT:
                use_struct = 1;
                if (ptr1) {
                    strcpy_ss(struct_name, ptr1);
                    if (strcmp(struct_name, namelist_name[n_namelists])==0)
                        bomb("structure name can't be the same as the namelist name", NULL);
                    }
                else
                    sprintf(struct_name, "%s_struct", namelist_name[n_namelists]);
                break;
              case SET_AUTO:
                strcpy_ss(definition_prefix, "auto ");
                break;
              default:
                printf("error: unknown namelist option: %s\n", ptr);
                exit(1);
                break;
                }
            }
        n_variables = 0;
        if (use_struct) {
            fprintf(fpo, "typedef struct {\n");
            if (!buffer_exists("dimensions"))
                create_buffer("dimensions", 10);
            else
                clear_buffer("dimensions");
            if (!buffer_exists("initializations"))
                create_buffer("initializations", 10);
            else
                clear_buffer("initializations");
            }
        while (efgets(s, STRING_LENGTH, fpi) && s[0]!='#') {
            n_lines++;
            if (get_token_buf(s, type_name[n_variables], STRING_LENGTH)==NULL) {
                printf("error on line %ld: no type name\n", n_lines);
                exit(1);
                }
            if (get_token_buf(s, variable_name[n_variables], STRING_LENGTH)==NULL) {
                printf("error on line %ld: no variable name\n", n_lines);
                exit(1);
                }
            if (!use_struct) {
                fprintf(fpo, "%s%s %s%s;\n", definition_prefix,
                        type_name[n_variables], variable_name[n_variables], s);
                if ((ptr=strchr(variable_name[n_variables], '['))) {
                    *ptr = 0;
                    fprintf(fpo, "%s%s %s_default[%s%s;\n", definition_prefix,
                            type_name[n_variables], variable_name[n_variables], ptr+1, s);
                    *ptr = '[';
                    }
                else {
                    fprintf(fpo, "%s%s %s_default%s;\n", definition_prefix,
                            type_name[n_variables], variable_name[n_variables], s);
                    }
                n_subscripts[n_variables] =
                    get_subscripts(subscript, MAX_SUBSCRIPTS, variable_name[n_variables]);
                if (n_subscripts[n_variables]!=0) {
                    fprintf(fpo, "%s long %s_dimen[%ld] = { ", definition_prefix, variable_name[n_variables],
                            n_subscripts[n_variables]);
                   for (i=0; i<n_subscripts[n_variables]-1; i++)
                        fprintf(fpo, "%s, ", subscript[i]);
                    fprintf(fpo, "%s };\n", subscript[i]);
                    }
                str_toupper(type_name[n_variables]);
                }
            else {
                fprintf(fpo, "    %s %s;\n", type_name[n_variables], variable_name[n_variables]);
                if ((ptr=strchr(s, '='))) {
                    while (*++ptr==' ')
                        ;
                    strcpy_ss(s, ptr);
                    }
                add_to_buffer("initializations", s);
                str_toupper(type_name[n_variables]);
                n_subscripts[n_variables] =
                    get_subscripts(subscript, MAX_SUBSCRIPTS, variable_name[n_variables]);
                if (n_subscripts[n_variables]!=0) {
                  snprintf(s, sizeof(s), "%.500s long %.500s_dimen[%ld] = { ", definition_prefix, variable_name[n_variables],
                            n_subscripts[n_variables]);
                    for (i=0; i<n_subscripts[n_variables]-1; i++)
                      snprintf(s+strlen(s), sizeof(s)-strlen(s), "%.1015s, ", subscript[i]);
                    snprintf(s+strlen(s), sizeof(s)-strlen(s), "%.1015s };\n", subscript[i]);
                    add_to_buffer("dimensions", s);
                    }
                }
            n_variables++;
            }
        if (use_struct) {
            if (no_init) {
                strcpy_ss(s, struct_name);
                str_toupper(s);
                fprintf(fpo, "    } %s;\n%s %s;\n", s, s, struct_name);
                }
            else {
                strcpy_ss(s, struct_name);
                str_toupper(s);
                fprintf(fpo, "    } %s;\n%s%s %s = {\n", s, 
                        definition_prefix, s, struct_name);
                fprintf_buffer(fpo, "    %s,\n", "initializations");
                fprintf(fpo, "    } ;\n");
                fprintf(fpo, "static %s %s_default = {\n", s, struct_name);
                fprintf_buffer(fpo, "    %s,\n", "initializations");
                fprintf(fpo, "    } ;\n");
                fprintf_buffer(fpo, "%s", "dimensions");
                }
            }
        fprintf(fpo, "\n%s ITEM %s_item_list[%ld] = {\n",
                definition_prefix, namelist_name[n_namelists], n_variables);
        for (i=0; i<n_variables; i++) {
            fprintf(fpo, "    { (char*)\"%s\", TYPE_%s, %ld, ",
                    variable_name[i], type_name[i], n_subscripts[i]);
            if (n_subscripts[i]==0)
                fprintf(fpo, "NULL, ");
            else
                fprintf(fpo, "%s_dimen, ", variable_name[i]);
            if (!use_struct)
                fprintf(fpo, "(char*)&%s", variable_name[i]);
            else
                fprintf(fpo, "(char*)&%s.%s",
                        struct_name, variable_name[i]);
            for (j=0; j<n_subscripts[i]; j++)
                fputs("[0]", fpo);
            fputs(", ", fpo);
            if (!use_struct)
                fprintf(fpo, "(char*)&%s_default", variable_name[i]);
            else
                fprintf(fpo, "(char*)&%s_default.%s",
                        struct_name, variable_name[i]);
            for (j=0; j<n_subscripts[i]; j++)
                fprintf(fpo, "[0]");
            if (!use_struct)
                fprintf(fpo, ", sizeof(%s)", variable_name[i]);
            else
                fprintf(fpo, ", sizeof(%s.%s)", 
                        struct_name, variable_name[i]);
            fprintf(fpo, " }%c\n", (i==n_variables-1?' ':','));
            }
        fprintf(fpo, "    } ;\n\n");
        fprintf(fpo, "%s NAMELIST %s = {\n    %s_item_list, \n",
                definition_prefix, namelist_name[n_namelists], namelist_name[n_namelists]);
        fprintf(fpo, "    %ld,\n    (char*)\"%s\"\n    };\n\n",
                n_variables, namelist_name[n_namelists]);
        n_namelists++;
        }
    if (n_namelists!=0 && !suppressSummaryVariables) {
        fprintf(fpo, "%s long n_namelists = %ld;\n\n", definition_prefix, n_namelists);
        fprintf(fpo, "%s NAMELIST *namelist_pointer[%ld] = {\n", definition_prefix, n_namelists);
        for (i=0; i<n_namelists-1; i++)
            fprintf(fpo, "    &%s,\n", namelist_name[i]);
        fprintf(fpo, "    &%s\n    } ;\n\n", namelist_name[i]);
        fprintf(fpo, "%s char *namelist_name[%ld] = {\n", definition_prefix, n_namelists);
        for (i=0; i<n_namelists-1; i++)
            fprintf(fpo, "    (char*)\"%s\",\n", namelist_name[i]);
        fprintf(fpo, "    (char*)\"%s\"\n    } ;\n", namelist_name[i]);
        fprintf(fpo, "\n%s NAMELIST_TEXT namelist_text;\n", definition_prefix);
        for (i=0; i<n_namelists; i++) {
            strcpy_ss(s, namelist_name[i]);
            str_toupper(s);
            fprintf(fpo, "#define %s_NAMELIST %ld\n", s, i);
            }
        }

    return(0);
    }

long get_subscripts(
                   char subs[MAX_SUBSCRIPTS][SUBSCRIPT_LENGTH],
                   long max_subs,
                   char *variable
                   )
{
    register char *ptr, *ptr1;
    register long n_subs;

    ptr = variable;
    n_subs = 0;
    while (*ptr) {
        if (*ptr!='[') {
            ptr++;
            continue;
            }
        *ptr++ = 0;
        ptr1 = ptr;
        while (*ptr1 && *ptr1!=']')
            ptr1++;
        *ptr1 = 0;
        strcpy_ss(subs[n_subs++], ptr);
        ptr = ptr1+1;
        if (n_subs>=max_subs) {
            printf("error: variable %s uses too many subscripts\n",
                   variable);
            exit(1);
            }
        }
    return(n_subs);
    }

char *efgets(char *s, long n, FILE *fp)
{
    register char *sr;

    sr = s;
    if (!fgets(sr, n, fp))
        return(NULL);
    if (sr[0]=='#')
        return(s);
    while (!has_semicolon(sr)) {
        sr = s + strlen(s);
        fgets(sr, n, fp);
        }
    return(s);
    }

int has_semicolon(char *s)
{
    register char *ptr;

    ptr = s;
    while (*ptr && *ptr!=';') {
        if (*ptr=='"') {
            ptr++;
            while (*ptr!='"')
                ptr++;
            ptr++;
            }
        else if (*ptr=='\'') {
            ptr++;
            while (*ptr!='\'')
                ptr++;
            ptr++;
            }
        else
            ptr++;
        }
    if (*ptr==';') {
        *ptr = 0;
        return(1);
        }
    return(0);
    }

