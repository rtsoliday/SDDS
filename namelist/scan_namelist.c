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
 Revision 1.12  2003/12/17 03:39:31  borland
 Use popen()/pclose() instead of popen()/fclose() for commands.  Prevents problems
 with runs that have many in-namelist commands.

 Revision 1.11  2003/09/02 17:56:47  soliday
 Cleaned up code for Linux.

 Revision 1.10  2003/08/28 20:35:18  soliday
 Cleaned up the code.

 Revision 1.9  2002/12/08 02:32:55  borland
 Fixed problems with commands inside namelist items under CONDOR.
 Also, added fclose() for the popen() implementation of the command
 feature.

 Revision 1.8  2002/08/14 16:23:49  soliday
 Added Open License

 Revision 1.7  2000/02/07 18:10:12  borland
 Now properly removes escapes from { and }.

 Revision 1.6  2000/02/07 18:05:58  borland
 Fixed a bug introduced when command feature was added.  New code was
 removing escape characters, which messed up tokenization.

 Revision 1.5  2000/01/28 15:49:12  soliday
 Increased value buffer size to 16k

 Revision 1.4  2000/01/27 21:52:23  soliday
 Added the ability to run external programs to retieve dynamic data.

 Revision 1.3  1999/05/28 14:53:48  soliday
 Removed compiler warnings under Linux.

 Revision 1.2  1995/09/05 21:21:45  saunders
 First test release of the SDDS1.5 package.

*/

#include "mdb.h"
#include "namelist.h"
#include <ctype.h>

#if defined(_WIN32) && !defined(__MINGW32__)
#define popen(x,y) _popen(x,y)
#endif

/* routine: scan_namelist()
 * purpose: scan a namelist contained in a line of text
 *          List is dissected into parralel arrays of entity names,
 *          arrays of values (stored as strings), numbers of values,
 *          and arrays of repeat factors.
 *          The group name is also extracted, and the function returns the
 *          number of entities scanned.  -1 is returned on error.
 */

#define DOLLARSIGN_TOO 0

long scan_namelist(NAMELIST_TEXT *nl, char *line)
{
    register char *ptr, *ptr_e;
    char *ptr_p, *ptr_to_free ;
    long n_entities, i, j, length_values, length;
    char *values;
    static char *buffer = NULL;
    static long bufsize = 0;
#define BIGBUFSIZE 16384
#define INDEX_LIMIT 16380
#if !defined(vxWorks)
    long k, m, n, len, insideCommand;
    char psBuffer[16384], tempptr[16384], command[16384];
    FILE *fID;
#ifdef CONDOR_COMPILE
    char tmpName[1024];
#endif
#endif

    values = NULL;
    length_values = 0;
    if (!buffer)
        buffer = tmalloc(sizeof(*buffer)*(bufsize = 128));

    nl->n_entities = 0;
    nl->group_name = NULL;
    nl->entity   = NULL;
    nl->value    = NULL;
    nl->n_values = nl->n_subscripts = NULL;
    nl->subscript = nl->repeat = NULL;

#ifdef DEBUG
    printf("scan_namelist: line = <%s>\n", line);
#endif
    cp_str(&ptr_to_free, line);
    ptr = ptr_to_free;

    /* Find group name--it's a token starting with '$' or '&'. */
#if DOLLARSIGN_TOO
    if (!(ptr_p=next_unquoted_char(ptr, '$', '"')) &&
        !(ptr_p=next_unquoted_char(ptr, '&', '"')) ) {
        free(ptr_to_free);
        fprintf(stderr, "error: namelist scanning problem---missing group name:\n%s\n", ptr);
        return(-1);
        }
#else
    if (!(ptr_p=next_unquoted_char(ptr, '&', '"'))) {
        free(ptr_to_free);
        fprintf(stderr, "error: namelist scanning problem---missing group name:\n%s\n", ptr);
        return(-1);
        }
#endif
    ptr_p++;
    if ((nl->group_name=get_token(ptr_p))==NULL) {
        free(ptr_to_free);
        fprintf(stderr, "error: namelist scanning problem---missing group name:\n%s\n", ptr);
        return(-1);
        }
#ifdef DEBUG
    printf("scan_namelist: nl->group_name = <%s>\n", nl->group_name);
#endif

    /* Count entities--each entity is associated with an equals sign,
     * since these are all assignments. */
#if DOLLARSIGN_TOO
    n_entities = count_occurences(ptr_p, '=', "$&");
#else
    n_entities = count_occurences(ptr_p, '=', "&");
#endif
    if (n_entities==0) {
        free(ptr_to_free);
        return(nl->n_entities=0);
        }
#ifdef DEBUG
    printf("scan_namelist: n_entities = %ld\n", n_entities);
#endif

    /* allocate memory for parallel arrays representing the namelist:
     *   nl->entity is an array of entity names
     *   nl->value is an array of arrays of values assigned
     *   nl->repeat is an array of arrays of repeat factors applied to values
     *   nl->n_values is an array of numbers of values
     *   nl->n_subscripts is an array of numbers of subscripts
     *   nl->subscript is an array of arrays of subscripts
     */
    nl->entity   = tmalloc(sizeof(*(nl->entity  ))*n_entities);
    nl->value    = tmalloc(sizeof(*(nl->value   ))*n_entities);
    nl->repeat   = tmalloc(sizeof(*(nl->repeat  ))*n_entities);
    nl->n_values = tmalloc(sizeof(*(nl->n_values))*n_entities);
    nl->n_subscripts = tmalloc(sizeof(*(nl->n_subscripts))*n_entities);
    nl->subscript    = tmalloc(sizeof(*(nl->subscript))*n_entities);

#ifdef DEBUG
    printf("scan_namelist: allocation successful\n");
#endif

    /* scan individual items--look for entity names and successively extract
     * strings containing assignments for single entities.  For each string,
     * extract a list of values and repeat factors.
     */
    for (i=0; i<n_entities; i++) {
        /* Find next entity name, which terminates with '='.  ptr_p
         * points to the section of the string being parsed.  I.e., everything
         * before ptr_p has been parsed already. */
        if (!(ptr_e = next_unquoted_char(ptr_p, '=', '"'))) {
            fprintf(stderr, "scan_namelist: no '=' in string >%s<\n", ptr_p);
            exit(1);
            }

        ptr = ptr_e;
        while (isspace(*(ptr-1)))
            ptr--;
        *ptr = 0;                /* end entity name string at '=' or space */

        while (isspace(*ptr_p))    /* advance to start of name      */
            ptr_p++;
        cp_str(nl->entity+i, ptr_p);   /* copy the entity name */
        nl->n_subscripts[i] = extract_subscripts(nl->entity[i], nl->subscript+i);

#ifdef DEBUG
        printf("scan_namelist: nl->entity[%ld] = <%s>\n", i, nl->entity[i]);
        printf("remainder: <%s>\n", ptr_e+1);
#endif

        /* Set ptr to point to start of values list, which follows the '='
         * that was just found and deleted. */
        ptr = ptr_e + 1;

        /* Find next entity name, if there is one, and insert a 0 before it
         * to mark end of current values list. */
        if ((ptr_e = next_unquoted_char(ptr, '=', '"'))) {
            /* set ptr_e to point to end of values list for current entity,
             * which is at the comma or space preceeding the next entity name
             */
#ifdef DEBUG
            printf("scan_namelist: next list is <%s>\n", ptr_e);
#endif
            /* Skip whitespace and get to the entity name. */
            ptr_e--;
            while (isspace(*ptr_e) && ptr_e!=ptr)
                ptr_e--;
            /* Skip over the entity name. */
            while (*ptr_e==')' || *ptr_e=='}') {
                ptr_e--;
                while (*ptr_e!='(' && *ptr_e!='{' && ptr_e!=ptr)
                    ptr_e--;
                }
            while (!(isspace(*ptr_e) || *ptr_e==',') && ptr_e!=ptr)
                ptr_e--;
            ptr_p = ptr_e+1;      /* Save start of next entity. */
            /* Skip trailing whitespace and commas in the values list. */
            while ((isspace(*ptr_e) || *ptr_e==',') && ptr_e!=ptr)
                ptr_e--;
            /* If there's nothing left, there was no value given. */
            if (++ptr_e==ptr)
                bomb("missing value in namelist", NULL);
            /* Mark end of values with a NUL, and set ptr_p to the point
             * where processing begins for the next entity. */
            *ptr_e = 0;
            }
        else {
            /* There is no following entity, so there should be a '$end'
             * '&end', or just '$' or '&'.   */
            /* set ptr_e to $end of string, check for error */
#ifdef DEBUG
            printf("scan_namelist: last item is <%s>\n", ptr);
#endif
#if DOLLARSIGN_TOO
            if (!(ptr_e=next_unquoted_char(ptr, '&', '"')) && !(ptr_e=next_unquoted_char(ptr, '$', '"')) ) {
                fprintf(stderr, "error: namelist improperly terminated\n");
                return(-1);
                }
#else
            if (!(ptr_e=next_unquoted_char(ptr, '&', '"'))) {
                fprintf(stderr, "error: namelist improperly terminated\n");
                return(-1);
                }
#endif
            *ptr_e = 0;           /* Delete the symbol. */

            /* If the number of entities doesn't match what was counted before,
             * something is really goofy. */
            if (i!=n_entities-1) {
#ifdef DEBUG
                printf("scan_namelist: fatal error: n_entites=%ld, i=%ld\n", n_entities,
                      i);
#endif
                return(-1);
                }

            /* Scan backwards to the first token. Make sure there are some
             * values listed. */
            ptr_e--;
            while ((isspace(*ptr_e) || *ptr_e==',') && ptr_e!=ptr)
                ptr_e--;
            if (++ptr_e==ptr) {
#ifdef DEBUG
            printf("scan_namelist: fatal error: ptr_e = ptr\n");
            printf("scan_namelist: ptr = <%s>\n", ptr);
#endif
                fprintf(stderr, "error: no values listed for namelist field %s\n",
                        nl->entity[i]);
                return(-1);     /* No values listed. */
                }
            *ptr_e = 0;
            ptr_p = ptr_e;
            }
        /* ptr now points to the beginning of the values list, which is
         * NUL terminated.  ptr_p points to the beginning of the rest of
         * what remains to be parsed. */
#ifdef DEBUG
        printf("scan_namelist: values list = <%s>\n", ptr);
        printf("scan_namelist: remainder = <%s>\n", ptr_p);
#endif
#if !defined(vxWorks)
	length = strlen(ptr);
	if (length == 0) {
            fprintf(stderr, "error: missing values for namelist field %s\n",
                    nl->entity[i]);
            return -1;
	}
	n = 0;
	j = 0;
	k = 0;
	insideCommand = 0;
	while (n<length) {
	  if (j>INDEX_LIMIT) {
	      fprintf(stderr, "error: values for namelist field %s is too long\n",
		      nl->entity[i]);
	      return -1;
	  }
	  if (ptr[n]=='\\') {
            if (ptr[n+1]!='}' && ptr[n+1]!='{')
              tempptr[j++] = ptr[n++];
            else
              n++;
	    if (n<length) {
	      if (insideCommand) {
		command[k] = ptr[n];
		k++;
		n++;		
	      } else {
		tempptr[j] = ptr[n];
		j++;
		n++;
	      }
	    }
	  } else if (ptr[n]=='{') {
	    if (insideCommand) {
	      fprintf(stderr, "error: values for namelist field %s has invalid command brackets\n",
		      nl->entity[i]);
	      return -1;
	    }
	    insideCommand = 1;
	    k = 0;
	    n++;
	  } else if (ptr[n]=='}') {
	    if (!insideCommand) {
	      fprintf(stderr, "error: values for namelist field %s has invalid command brackets\n",
		      nl->entity[i]);
	      return -1;
	    }
	    insideCommand = 0;
	    command[k] = 0;
	    n++;
#ifndef CONDOR_COMPILE
	    if ((fID = popen(command, "r")) == NULL) {
	      fprintf(stderr, "error: invalid command for namelist field %s\n",
		      nl->entity[i]);
              fprintf(stderr, "command was %s\n", command);
	      return -1;
	    }
	    if (feof(fID)) {
	      fprintf(stderr, "error: command for namelist field %s returns EOF\n",
		      nl->entity[i]);
              fprintf(stderr, "command was %s\n", command);
	      return -1;
	    }
	    if (fgets(psBuffer, 128, fID) == NULL) {
	      fprintf(stderr, "error: command for namelist field %s returns NULL\n",
		      nl->entity[i]);
              fprintf(stderr, "command was %s\n", command);
	      return -1;
	    }
            pclose(fID);
#else
            tmpnam(tmpName);
            sprintf(psBuffer, "%s > %s", command, tmpName);
            system(psBuffer);
            if (!(fID = fopen(tmpName, "r"))) {
	      fprintf(stderr, "error: command for namelist field %s failed\n",
		      nl->entity[i]);
              fprintf(stderr, "command was %s\n", command);
	      return -1;
            }
            if (fgets(psBuffer, 128, fID) == NULL) {
	      fprintf(stderr, "error: command for namelist field %s returns NULL\n",
		      nl->entity[i]);
              fprintf(stderr, "command was %s\n", command);
	      return -1;
            }
            fclose(fID);
            remove(tmpName);
 #endif
	    len = strlen(psBuffer)-1;
	    if ((len > 0) && (psBuffer[len] == '\n')) {
	      psBuffer[len] = 0;
	    }
	    m = 0;
	    len = strlen(psBuffer);
	    if (j+len>INDEX_LIMIT) {
	      fprintf(stderr, "error: values for namelist field %s is too long\n",
		      nl->entity[i]);
	      return -1;
	    }
	    while (m < len) {
	      tempptr[j] = psBuffer[m];
	      j++;
	      m++;
	    } 
	  } else {
	    if (insideCommand) {
	      command[k] = ptr[n];
	      n++;
	      k++;
	    } else {
	      tempptr[j] = ptr[n];
	      n++;
	      j++;
	    }
	  }
	}
	tempptr[j] = 0;
	
	ptr = tempptr;
#endif
        /* Process string of values.  Values must be separated by commas. */
        length = strlen(ptr)+1;
        if (length==1) {
            fprintf(stderr, "error: missing values for namelist field %s\n",
                    nl->entity[i]);
            return -1;
            }
        if (values==NULL) {
            values = tmalloc(length*sizeof(*values));
            length_values = length;
            }
        else if (length>length_values) {
            tfree(values);
            values = tmalloc(length*sizeof(*values));
            }
        strcpy_ss(values, ptr);
        nl->n_values[i] = count_occurences(values, ',', "")+1;
        nl->value[i]    = tmalloc(sizeof(*(nl->value[i]))*nl->n_values[i]);
        nl->repeat[i]   = tmalloc(sizeof(*(nl->repeat[i]))*nl->n_values[i]);
#ifdef DEBUG
        printf("scan_namelist: allocation successful--nl->n_values[%ld]=%ld\n",
            i, nl->n_values[i]);
#endif

        for (j=0; j<nl->n_values[i]; j++) {
#ifdef DEBUG
          printf("scan_namelist: scanning tokens from >%s<\n",
                 values);
#endif
            ptr = nl->value[i][j] =
                    get_token_tq(values, ", ", ", ", "\"'", "\"'");
#ifdef DEBUG
            printf("scan_namelist: looping over values--j=%ld\n", j);
            printf("scan_namelist: token is <%s>\n", ptr);
#endif 
            if (!ptr) {
                fprintf(stderr, "error: missing values for namelist field %s\n",
                        nl->entity[i]);
                return -1;
                }
            if (!isdigit(*ptr)) {
                nl->repeat[i][j] = 1;
                un_quote(ptr);
                }
            else {
                ptr_e = ptr;
                while (isdigit(*ptr))
                    ptr++;
                if (*ptr!='*') {
                    nl->repeat[i][j] = 1;
                    continue;
                    }
                *ptr = 0;
                if ((length=strlen(ptr_e))-1>bufsize)
                    buffer = trealloc(buffer, sizeof(*buffer)*(bufsize=length+1024));
                strcpy_ss(buffer, ptr_e);
                strcpy_ss(nl->value[i][j], ptr+1);
                un_quote(nl->value[i][j]);
#ifdef DEBUG
                printf("doing repeat scan: ptr_e = <%s>  ptr+1 = <%s>\nbuffer = <%s>\n",
                    ptr_e, ptr+1, buffer);
#endif
                if (sscanf(buffer, "%ld", nl->repeat[i]+j)!=1)
                    bomb("bad repeat specifier in namelist", NULL);
                }
#ifdef DEBUG
            printf("scan_namelist: nl->value[%ld][%ld] = %ld*<%s>\n",
                i, j, nl->repeat[i][j], nl->value[i][j]);
            printf("remainder: <%s>\n\n", values);
#endif
            }
        }

    free(values);
    free(ptr_to_free);
#ifdef DEBUG
    nl->n_entities = n_entities;
    show_namelist(stdout, nl);
#endif
    return(nl->n_entities=n_entities);
    }

