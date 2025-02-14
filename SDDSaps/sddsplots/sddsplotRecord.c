/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sddsplotRecord.c
 * 
 * part of sddsplot (plotting program for SDDS files)
 *
 * Michael Borland, 1994.
 $Log: not supported by cvs2svn $
 Revision 1.3  1998/08/25 15:05:39  borland
 New version with major changes to allow multiple scales on x and y axes.

 * Revision 1.2  1995/09/05  21:10:04  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "graph.h"
#include "SDDS.h"
#include "sddsplot.h"
#include <ctype.h>

void record_column_information(DATA_INFO *info, SDDS_TABLE *table, char *name)
{
  if (name != NULL)
    {
      if (SDDS_GetColumnInformation(table, "symbol", &info->symbol, SDDS_GET_BY_NAME, name)!=SDDS_STRING ||
          SDDS_GetColumnInformation(table, "units", &info->units, SDDS_GET_BY_NAME, name)!=SDDS_STRING ||
          SDDS_GetColumnInformation(table, "description", &info->description, SDDS_GET_BY_NAME, name)!=SDDS_STRING ) 
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      if (!info->symbol || SDDS_StringIsBlank(info->symbol))
        SDDS_CopyString(&info->symbol, name);
    }
  else
    {
      SDDS_CopyString(&info->symbol, "Index");
    }
  if (info->units==NULL)
    SDDS_CopyString(&info->units, "");
#if defined(DEBUG)
  fprintf(stderr, "column info recorded: name=%s   symbol=%s, units=%s, descrip=%s\n",
          name, info->symbol, info->units, info->description);
#endif
}

void record_parameter_information(DATA_INFO *info, SDDS_TABLE *table, char *name)
{
  if (name != NULL)
    {
      if (SDDS_GetParameterInformation(table, "symbol", &info->symbol, SDDS_GET_BY_NAME, name)!=SDDS_STRING ||
          SDDS_GetParameterInformation(table, "units", &info->units, SDDS_GET_BY_NAME, name)!=SDDS_STRING ||
          SDDS_GetParameterInformation(table, "description", &info->description, SDDS_GET_BY_NAME, name)!=SDDS_STRING )
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      if (!info->symbol || SDDS_StringIsBlank(info->symbol))
        SDDS_CopyString(&info->symbol, name);
    }
  else
    {
      SDDS_CopyString(&info->symbol, "Index");
    }
  if (info->units==NULL)
    SDDS_CopyString(&info->units, "");
}

void record_array_information(DATA_INFO *info, SDDS_TABLE *table, char *name)
{
  if (name != NULL)
    {
      if (SDDS_GetArrayInformation(table, "symbol", &info->symbol, SDDS_GET_BY_NAME, name)!=SDDS_STRING ||
          SDDS_GetArrayInformation(table, "units", &info->units, SDDS_GET_BY_NAME, name)!=SDDS_STRING ||
          SDDS_GetArrayInformation(table, "description", &info->description, SDDS_GET_BY_NAME, name)!=SDDS_STRING )
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      if (!info->symbol || SDDS_StringIsBlank(info->symbol))
        SDDS_CopyString(&info->symbol, name);
    }
  else
    {
      SDDS_CopyString(&info->symbol, "Index");
    }
  if (info->units==NULL)
    SDDS_CopyString(&info->units, "");
}



