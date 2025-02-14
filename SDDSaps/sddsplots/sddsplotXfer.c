/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sddsplotXfer.c
 * 
 * part of sddsplot (plotting program for SDDS files)
 *
 * Michael Borland, 1994.
 $Log: not supported by cvs2svn $
 Revision 1.11  2004/04/20 17:20:18  shang
 modified transferNamesToLists() to obtain y1names from editing ynames if
 y1name contains "y1edit="

 Revision 1.10  2002/08/14 17:24:56  soliday
 Added Open License

 Revision 1.9  2002/07/01 18:38:23  borland
 Can now use wildcards for x quantity and templates for y.

 Revision 1.8  1999/07/21 18:07:27  borland
 Eliminated many memory leaks.  Program is now suitable for running with
 -repeat option, although not all leaks are gone.

 Revision 1.7  1999/06/03 16:09:38  soliday
 Removed compiler warnings under linux.

 Revision 1.6  1999/02/10 14:27:01  borland
 Store the string "$i$iNo data$d$d" in an array so that there are no problems
 with gnu compiler when the string is passed to jxyplot.
 Also, set the character size to the default for each panel using new
 set_default_char_size() routine.

 Revision 1.5  1998/08/25 15:05:39  borland
 New version with major changes to allow multiple scales on x and y axes.

 * Revision 1.4  1996/01/21  17:57:43  borland
 * Added -namescan option to control scanning of files for matches to wildcard
 * dataname specifications; this allows the user to cut running time when there
 * are many files with identical data elements.
 *
 * Revision 1.3  1995/09/25  23:13:59  borland
 * Modified transferName(s)ToLists to properly deal with %s substitution in
 * y1 and x1 names (used to produce duplicate names); singular version now
 * calls plural version to simplify code.
 *
 * Revision 1.2  1995/09/05  21:10:06  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "graph.h"
#include "SDDS.h"
#include "sddsplot.h"
#include <ctype.h>

void resolve_wildcard_datanames(PLOT_SPEC *plspec)
{
  long irequest, ifile, idataname, fileopen, names, matches = 0, origNames;
  PLOT_REQUEST *plreq;
  SDDS_TABLE table;
  char **yname, **xname, **x1name, **y1name, **newname;
  short *was_wildname;

  for (irequest=1; irequest<plspec->plot_requests; irequest++) {
    plreq = plspec->plot_request+irequest;
    names = 0;
    yname = xname = x1name = y1name = newname = NULL;
    was_wildname = NULL;
    if (plreq->filenames==0)
      bomb("plot request does not have any files", NULL);
    names = 0;
    origNames = plreq->datanames;
    for (ifile=0; ifile<plreq->filenames; ifile++) {
      fileopen = 0;
      if (plreq->nameScanFlags&NAMESCAN_FIRST && names)
	break;
      for (idataname=0; idataname<plreq->datanames; idataname++) {
	if ((plreq->xname[idataname] != NULL) && (has_wildcards(plreq->xname[idataname]))) {
	  if ((!fileopen && !SDDS_InitializeInput(&table, plreq->filename[ifile])) ||
	      (plreq->data_class==COLUMN_DATA &&
	       (matches=SDDS_MatchColumns(&table, &newname, SDDS_MATCH_STRING, FIND_ANY_TYPE,
					  plreq->xname[idataname], SDDS_0_PREVIOUS|SDDS_OR))<0) ||
	      (plreq->data_class==PARAMETER_DATA &&
	       (matches=SDDS_MatchParameters(&table, &newname, SDDS_MATCH_STRING, FIND_ANY_TYPE,
					     plreq->xname[idataname], SDDS_0_PREVIOUS|SDDS_OR))<0) ||
	      (plreq->data_class==ARRAY_DATA &&
	       (matches=SDDS_MatchArrays(&table, &newname, SDDS_MATCH_STRING, FIND_ANY_TYPE,
					 plreq->xname[idataname], SDDS_0_PREVIOUS|SDDS_OR))<0))
	    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	  fileopen = 1;
	  if (matches==0)
	    fprintf(stderr, "no match: %s\n", plreq->xname[idataname]);
	  else {
	    names = transferNamesToLists1(&yname, &xname, &x1name, &y1name, 
					 &was_wildname, 
					 names, newname, matches,
					 plreq->yname[idataname], 
					 plreq->x1name[idataname],
					 plreq->y1name[idataname], 1);
	    free(newname);
	  }
	}
	else if (has_wildcards(plreq->yname[idataname])) {
	  if ((!fileopen && !SDDS_InitializeInput(&table, plreq->filename[ifile])) ||
	      (plreq->data_class==COLUMN_DATA &&
	       (matches=SDDS_MatchColumns(&table, &newname, SDDS_MATCH_STRING, FIND_ANY_TYPE,
					  plreq->yname[idataname], SDDS_0_PREVIOUS|SDDS_OR))<0) ||
	      (plreq->data_class==PARAMETER_DATA &&
	       (matches=SDDS_MatchParameters(&table, &newname, SDDS_MATCH_STRING, FIND_ANY_TYPE,
					     plreq->yname[idataname], SDDS_0_PREVIOUS|SDDS_OR))<0) ||
	      (plreq->data_class==ARRAY_DATA &&
	       (matches=SDDS_MatchArrays(&table, &newname, SDDS_MATCH_STRING, FIND_ANY_TYPE,
					 plreq->yname[idataname], SDDS_0_PREVIOUS|SDDS_OR))<0))
	    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	  fileopen = 1;
	  if (matches==0)
	    fprintf(stderr, "no match: %s\n", plreq->yname[idataname]);
	  else {
	    names = transferNamesToLists(&yname, &xname, &x1name, &y1name, &was_wildname, 
					 names, newname, matches,
					 plreq->xname[idataname], plreq->x1name[idataname],
					 plreq->y1name[idataname], 1);
	    free(newname);
	  }
	}
	else {
	  unescape_wildcards(plreq->yname[idataname]);
	  names = transferNameToLists(&yname, &xname, &x1name, &y1name, &was_wildname, names, 
				      plreq->yname[idataname], plreq->xname[idataname], plreq->x1name[idataname],
				      plreq->y1name[idataname], 0);
	}
                
      }
      if (fileopen && !SDDS_Terminate(&table))
	SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    }
    SDDS_FreeStringArray(plreq->xname, origNames);
    free(plreq->xname);
    plreq->xname = xname;
    SDDS_FreeStringArray(plreq->yname, origNames);
    free(plreq->yname);
    plreq->yname = yname;
    if (plreq->x1name) {
      SDDS_FreeStringArray(plreq->x1name, origNames);
      free(plreq->x1name);
    }
    plreq->x1name = x1name;
    if (plreq->y1name) {
      SDDS_FreeStringArray(plreq->y1name, origNames);
      free(plreq->y1name);
    }
    plreq->y1name = y1name;
    free(plreq->was_wildname);
    plreq->was_wildname = was_wildname;
    plreq->datanames = names;
  }
}

	
long transferNamesToLists(char ***yname, char ***xname, char ***x1name, char ***y1name, 
                          short **was_wildname, long names,
                          char **newname, long newnames, char *xname1, char *x1name1, 
                          char *y1name1, short wildcode)
{
  long i, j, total;
  char *x1name1sub, *y1name1sub, *editcommand;
  static char s[SDDS_MAXLINE];

  total = names;
  *yname = trealloc(*yname, sizeof(**yname)*(names+newnames));
  *xname = trealloc(*xname, sizeof(**xname)*(names+newnames));
  *x1name = trealloc(*x1name, sizeof(**x1name)*(names+newnames));
  *y1name = trealloc(*y1name, sizeof(**y1name)*(names+newnames));
  *was_wildname = trealloc(*was_wildname, sizeof(**was_wildname)*(names+newnames));
  
  for (i=0; i<newnames; i++) {
    if (x1name1 && xname1) {
      if (strstr(x1name1, "%s")) {
        x1name1sub = malloc(sizeof(*x1name1sub)*(strlen(xname1)+strlen(x1name1)+1));
        sprintf(x1name1sub, x1name1, xname1);
      }
      else 
        SDDS_CopyString(&x1name1sub, x1name1);
    }
    else
      x1name1sub = NULL;
    if (y1name1) {
      if (strchr(y1name1,'=')!=NULL) {
        if (strstr(y1name1,"y1edit=")==NULL && strstr(y1name1,"edit=")==NULL)
          bomb("Invalid y1name with edit command, should be y1edit=<edit string> !",NULL);
        editcommand=strchr(y1name1,'=');
        strcpy_ss(s,newname[i]);
        if (!edit_string(s,editcommand))
          bomb("Can not edit string from yname to get y1name!",NULL);
        SDDS_CopyString(&y1name1sub,s);
      } else if (strstr(y1name1, "%s")) {
        y1name1sub = malloc(sizeof(*y1name1sub)*(strlen(newname[i])+strlen(y1name1)+1));
        sprintf(y1name1sub, y1name1, newname[i]);
      }
      else 
        SDDS_CopyString(&y1name1sub, y1name1);
    }
    else 
      y1name1sub = NULL;

    for (j=0; j<names; j++)  {
      if (!(strcmp((*yname)[j], newname[i])!=0 
            || (xname1 && strcmp((*xname)[j], xname1)!=0)
            || (y1name1sub && (*y1name)[j] && strcmp((*y1name)[j], y1name1sub)!=0)
            || (x1name1sub && (*x1name)[j] && strcmp((*x1name)[j], x1name1sub)!=0)
            || (y1name1sub && !(*y1name)[j]) || (!y1name1sub && (*y1name)[j])
            || (x1name1sub && !(*x1name)[j]) || (!x1name1sub && (*x1name)[j])
            ))
        break;
    }
    if (j==names) {
      (*yname)[total] = newname[i];
      SDDS_CopyString(*xname+total, xname1);
      if (x1name1)
        (*x1name)[total] =  x1name1sub;
      else 
        (*x1name)[total] = NULL;
      if (y1name1)
        (*y1name)[total] =  y1name1sub;
      else 
        (*y1name)[total] = NULL;
      (*was_wildname)[total] = wildcode;
      total++;
    }
    else
      free(newname[i]);
  }
  return total;
}

long transferNamesToLists1(char ***yname, char ***xname, char ***x1name, char ***y1name, 
                          short **was_wildname, long names,
                          char **newname, long newnames, char *yname1, char *x1name1, 
                          char *y1name1, short wildcode)
{
    long i, j, total;
    char *x1name1sub, *y1name1sub, *yname1sub;

    total = names;
    *yname = trealloc(*yname, sizeof(**yname)*(names+newnames));
    *xname = trealloc(*xname, sizeof(**xname)*(names+newnames));
    *x1name = trealloc(*x1name, sizeof(**x1name)*(names+newnames));
    *y1name = trealloc(*y1name, sizeof(**y1name)*(names+newnames));
    *was_wildname = trealloc(*was_wildname, sizeof(**was_wildname)*(names+newnames));

    for (i=0; i<newnames; i++) {
        if (x1name1) {
            if (strstr(x1name1, "%s")) {
                x1name1sub = malloc(sizeof(*x1name1sub)*(strlen(newname[i])+strlen(x1name1)+1));
                sprintf(x1name1sub, x1name1, newname[i]);
                }
            else 
                SDDS_CopyString(&x1name1sub, x1name1);
            }
        else
            x1name1sub = NULL;
        if (y1name1) {
            if (strstr(y1name1, "%s")) {
                y1name1sub = malloc(sizeof(*y1name1sub)*(strlen(newname[i])+strlen(y1name1)+1));
                sprintf(y1name1sub, y1name1, newname[i]);
                }
            else 
                SDDS_CopyString(&y1name1sub, y1name1);
            }
        else 
            y1name1sub = NULL;
	if (strstr(yname1, "%s")) {
	  yname1sub = malloc(sizeof(*yname1sub)*(strlen(newname[i])+strlen(yname1)+1));
	  sprintf(yname1sub, yname1, newname[i]);
	}
	else 
	  SDDS_CopyString(&yname1sub, yname1);

        for (j=0; j<names; j++)  {
            if (!(strcmp((*xname)[j], newname[i])!=0 
                  || strcmp((*yname)[j], yname1sub)!=0
                  || (y1name1sub && (*y1name)[j] && strcmp((*y1name)[j], y1name1sub)!=0)
                  || (x1name1sub && (*x1name)[j] && strcmp((*x1name)[j], x1name1sub)!=0)
                  || (y1name1sub && !(*y1name)[j]) || (!y1name1sub && (*y1name)[j])
                  || (x1name1sub && !(*x1name)[j]) || (!x1name1sub && (*x1name)[j])
                  ))
                break;
            }
        if (j==names) {
            (*xname)[total] = newname[i];
            SDDS_CopyString(*yname+total, yname1sub);
            if (x1name1)
                (*x1name)[total] =  x1name1sub;
            else 
                (*x1name)[total] = NULL;
            if (y1name1)
                (*y1name)[total] =  y1name1sub;
            else 
                (*y1name)[total] = NULL;
            (*was_wildname)[total] = wildcode;
            total++;
            }
        else
            free(newname[i]);
        }
    return total;
    }

long transferNameToLists(char ***yname, char ***xname, char ***x1name, char ***y1name, short **was_wildname, 
                         long names, char *yname1, char *xname1, char *x1name1, char *y1name1, short wildcode)
{
    char *copy;
    SDDS_CopyString(&copy, yname1);
    return transferNamesToLists(yname, xname, x1name, y1name, was_wildname, names,
                                &copy, 1, xname1, x1name1, y1name1, wildcode);
    }
