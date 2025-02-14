/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sddsplotRead.c
 * 
 * part of sddsplot (plotting program for SDDS files)
 *
 * Michael Borland, 1994.
 $Log: not supported by cvs2svn $
 Revision 1.35  2006/11/15 16:06:33  jiaox
 In the previous version, accidently deleted lines to read parameter values
 for -string=@parameter keyword. It is back now.

 Revision 1.34  2006/10/20 14:58:34  jiaox
 1) Added format key word for -topline and -legend options.If given with paramteter keyword (or @), the format will be applied to the parameter output. If not given, but there is format_string for the parameter in
 the SDDS file, it output will take that format_string for output.

 Revision 1.33  2006/08/21 21:58:52  jiaox
 Fixed the bug that the -paramemterChange with -split option did not display the proper color codings.

 Revision 1.32  2005/11/04 22:46:46  soliday
 Updated code to be compiled by a 64 bit processor.

 Revision 1.31  2005/04/28 16:31:34  soliday
 Added the ability to plot with an intensity bar that is split based on
 parameter data.

 Revision 1.30  2005/03/07 22:41:23  shang
 modified to read split symbol, units and obtain the split minimum and maximum value
 for plotting the intensity color bar

 Revision 1.29  2004/03/26 19:47:40  shang
 modified read_sddsplot_data() to work with new options, fromPage, toPage
 and xExclude, yExclude

 Revision 1.28  2003/09/02 19:29:29  soliday
 Cleaned up code for Linux.

 Revision 1.27  2003/04/14 14:46:02  shang
 added processing time filters

 Revision 1.26  2002/12/04 18:03:32  borland
 Fixed bug in factor processing introduced in previous change.

 Revision 1.25  2002/12/04 00:34:50  borland
 Added fractional-deviation mode to -mode option:
 -mode=[xy]FractionalDeviation
 Added value-from-file and invert features to -factor option:
 -factor=[xy]Parameter=<name>,[xy]Invert
 Fixed invert feature for -offset option (was ignored when the offset
 was given literally but used when given as parameter).

 Revision 1.24  2002/10/30 17:58:36  borland
 Added to -string option the ability to take position values from
 parameters in the file.

 Revision 1.23  2002/08/14 17:24:55  soliday
 Added Open License

 Revision 1.22  2002/07/03 15:06:08  shang
 replaced SDDS_GetParameterAsDouble for getting tag values by SDDS_GetParameterAsString

 Revision 1.21  2002/06/05 20:27:13  shang
 replaced SDDS_GetParameter() for dataset->legend and dataset->string_label[i].string by
 SDDS_GetParameterAsString() so that they accept all kinds of data type besides string

 Revision 1.20  2002/05/28 07:02:29  borland
 Added some debugging statements.

 Revision 1.19  2000/10/24 01:17:27  borland
 Added support for arrays, in the sense of just treating a pair of arrays like
 any other (x, y) data.

 Revision 1.18  2000/10/20 01:34:33  borland
 Now practises read recovery on files when possible.

 Revision 1.17  1999/11/12 02:32:47  borland
 Fixed bug in reporting the names of files that cause trouble when read.

 Revision 1.16  1999/07/21 18:07:26  borland
 Eliminated many memory leaks.  Program is now suitable for running with
 -repeat option, although not all leaks are gone.

 Revision 1.15  1999/06/03 16:07:57  soliday
 Removed compiler warnings under linux.

 Revision 1.14  1998/08/25 15:05:38  borland
 New version with major changes to allow multiple scales on x and y axes.

 Revision 1.13  1997/02/05 15:59:15  borland
 Added -drawline option.

 * Revision 1.12  1996/04/10  21:37:08  borland
 * Implemented -pointLabel option for labeling data points with string values
 * from data files.
 *
 * Revision 1.11  1996/03/21  17:19:47  borland
 * Removed dependence on SDDS library (gif.trm graphics.c).
 * Added presparse option and implementation.
 *
 * Revision 1.10  1996/02/15  16:14:59  borland
 * Eliminated redundant warning messages about missing datanames.
 *
 * Revision 1.9  1996/02/14  00:59:25  borland
 * Fixed bug in read routines (wasn't resetting row flags between requests).
 * Changed argument parsing over to scanItemList form scan_item_list.
 *
 * Revision 1.8  1996/02/08  00:59:17  borland
 * Enhanced -offset option to allow offseting data based on values in parameters.
 *
 * Revision 1.7  1996/02/07  17:44:26  borland
 * Fixed bug that occured with dataname-based grouping when mpl files were
 * present (null pointer reference)e).
 *
 * Revision 1.6  1996/01/24  05:13:23  borland
 * Fixed print statement for files with no pages; changed from error with
 * exit to warning.  Changed range determination to set range to zero if
 * there is no data (was +/- DBL_MAX).
 *
 * Revision 1.5  1996/01/09  23:17:20  borland
 * Removed incorrect SDDS_Terminate call for mpl files.
 *
 * Revision 1.4  1995/11/03  23:03:14  borland
 * Modified routines for adding dataset slots to realloc in big chunks, to
 * avoid SUN realloc problem that lead to N^2 growth in memory usage; added
 * debugging statements to data reading routine to track memory usage.
 *
 * Revision 1.3  1995/10/18  21:56:38  borland
 * Modified data-reading routines to read multiple instances of the same file
 * only once (sddsplotRead.c).  Other routines have minor modifications.
 *
 * Revision 1.2  1995/09/05  21:10:02  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "graph.h"
#include "table.h"
#include "SDDS.h"
#include "sddsplot.h"
#include <ctype.h>

long CheckForMplFile(char *filename, PLOT_REQUEST *plreq, long requests);

void read_sddsplot_data(PLOT_SPEC *plspec)
{
  long ireq, ifile, idata, iset, points, datapage = 1, pagesRead, i, j;
  long datanames, datanames_absent, inewdata, file, maxFiles;
  SDDS_TABLE table;
  PLOT_REQUEST *plreq = NULL;
  double *x, *y=NULL, *x1, *y1, *split;
  double xparam, yparam, x1param, y1param, splitparam;
  short *dataname_absent, mplFile;
  char **enumerate, **pointLabel;
  long *xtype, *ytype, files;
  char edit_buffer[SDDS_MAXLINE];
  char **filename;
  typedef struct {
    long iset, newset, pagesAccepted, datasets;
    PLOT_DATA *dataset;
  } REQUESTDATA;
  REQUESTDATA *requestData;
  long justAddedSlots, presparseOffset, presparseInterval;
  int32_t nx, ny, nx1, ny1;
  long matches=0;
  char **matchname=NULL;
  
  plspec->dataset = NULL;
  plspec->datasets = iset = 0;
  dataname_absent = NULL;
  xtype = ytype = NULL;
  filename = NULL;
  files = maxFiles = 0;
  requestData = (REQUESTDATA *)calloc(sizeof(*requestData), plspec->plot_requests);
  for (ireq=1; ireq<plspec->plot_requests; ireq++) {
    plreq = plspec->plot_request+ireq;
    plreq->description_text = (char **)calloc(plreq->filenames, sizeof(*plreq->description_text));
    plreq->split.value_valid = 0;
    plreq->split.min = DBL_MAX;
    plreq->split.max = -DBL_MAX;
    for (ifile=0; ifile<plreq->filenames; ifile++) {
      for (file=0; file<files; file++) {
        if (strcmp(filename[file], plreq->filename[ifile])==0) 
          break;
      }
      if (file==files) {
        if (files>=maxFiles)
          filename = SDDS_Realloc(filename, sizeof(*filename)*(maxFiles+=10));
        SDDS_CopyString(filename+files, plreq->filename[ifile]);
        files++;
      }
    }
  }
#if defined(DEBUG)
  fprintf(stderr, "done making list of unique filenames\n");
#endif
  
  for (file=0; file<files; file++) {
    if (!(mplFile=CheckForMplFile(filename[file], plspec->plot_request, plspec->plot_requests)) &&
        !SDDS_InitializeInput(&table, filename[file]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    for (i=0; i<plspec->conversions; i++) {
      if (plspec->conversion[i]->is_array) {
        if (has_wildcards(plspec->conversion[i]->name)) {
          matches = SDDS_MatchArrays(&table, &matchname, SDDS_MATCH_STRING, FIND_ANY_TYPE, plspec->conversion[i]->name, SDDS_0_PREVIOUS|SDDS_OR);
          for (j=0 ; j<matches; j++) {
            if (SDDS_SetArrayUnitsConversion(&table, matchname[j], plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
              fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
            }
            free(matchname[j]);
          }
          if (matchname) free(matchname);
          matchname = NULL;
        } else {
          if (SDDS_SetArrayUnitsConversion(&table, plspec->conversion[i]->name, plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
            fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
          }
        }
      }
      if (plspec->conversion[i]->is_column) {
        if (has_wildcards(plspec->conversion[i]->name)) {
          matches = SDDS_MatchColumns(&table, &matchname, SDDS_MATCH_STRING, FIND_ANY_TYPE, plspec->conversion[i]->name, SDDS_0_PREVIOUS|SDDS_OR);
          for (j=0 ; j<matches; j++) {
            if (SDDS_SetColumnUnitsConversion(&table, matchname[j], plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
              fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
            }
            free(matchname[j]);
          }
          if (matchname) free(matchname);
          matchname = NULL;
        } else {
          if (SDDS_SetColumnUnitsConversion(&table, plspec->conversion[i]->name, plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
            fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
          }
        }
      }
      if (plspec->conversion[i]->is_parameter) {
        if (has_wildcards(plspec->conversion[i]->name)) {
          matches = SDDS_MatchParameters(&table, &matchname, SDDS_MATCH_STRING, FIND_ANY_TYPE, plspec->conversion[i]->name, SDDS_0_PREVIOUS|SDDS_OR);
          for (j=0 ; j<matches; j++) {
            if (SDDS_SetParameterUnitsConversion(&table, matchname[j], plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
              fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
            }
            free(matchname[j]);
          }
          if (matchname) free(matchname);
          matchname = NULL;
        } else {
          if (SDDS_SetParameterUnitsConversion(&table, plspec->conversion[i]->name, plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
            fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
          }
        }
      }
    }
    presparseInterval = 1;
    presparseOffset = 0;
    for (ireq=1; ireq<plspec->plot_requests; ireq++) {
      plreq = plspec->plot_request+ireq;
      plreq->split.value_valid = 0;
      requestData[ireq].iset = -1;
      if (plreq->presparse_interval>1) {
        for (ifile=0; ifile<plreq->filenames; ifile++) {
          if (strcmp(filename[file], plreq->filename[ifile])==0) {
            presparseInterval = plreq->presparse_interval;
            presparseOffset = plreq->presparse_offset;
            break;
          }
        }
      }
    }
    
#if defined(DEBUG)
    fprintf(stderr, "memory: %ld\n", memory_count());
    fprintf(stderr, "working on file %s  %ld datasets  mplfile = %hd\n", filename[file], 
            plspec->datasets, mplFile);
    fprintf(stderr, "file pointer = %x\n", table.layout.fp);
#endif
    pagesRead = 0;
    while (mplFile || (datapage=SDDS_ReadPageSparse(&table, 0, presparseInterval, presparseOffset, 0))>=0) {
      /* if data from this page can be recovered, then use it; otherwise, skip it */
      if (datapage==0 && !SDDS_ReadRecoveryPossible(&table))
        break;
      pagesRead++;
#if defined(DEBUG)
      fprintf(stderr, "memory: %ld\n", memory_count());
      fprintf(stderr, "page %ld read\n", datapage);
#endif
      for (i=0; i<plspec->conversions; i++) {
        if (plspec->conversion[i]->is_array) {
          if (has_wildcards(plspec->conversion[i]->name)) {
            matches = SDDS_MatchArrays(&table, &matchname, SDDS_MATCH_STRING, FIND_ANY_TYPE, plspec->conversion[i]->name, SDDS_0_PREVIOUS|SDDS_OR);
            for (j=0 ; j<matches; j++) {
              if (SDDS_SetArrayUnitsConversion(&table, matchname[j], plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
                fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
              }
              free(matchname[j]);
            }
            if (matchname) free(matchname);
            matchname = NULL;
          } else {
            if (SDDS_SetArrayUnitsConversion(&table, plspec->conversion[i]->name, plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
              fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
            }
          }
        }
        if (plspec->conversion[i]->is_column) {
          if (has_wildcards(plspec->conversion[i]->name)) {
            matches = SDDS_MatchColumns(&table, &matchname, SDDS_MATCH_STRING, FIND_ANY_TYPE, plspec->conversion[i]->name, SDDS_0_PREVIOUS|SDDS_OR);
            for (j=0 ; j<matches; j++) {
              if (SDDS_SetColumnUnitsConversion(&table, matchname[j], plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
                fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
              }
              free(matchname[j]);
            }
            if (matchname) free(matchname);
            matchname = NULL;
          } else {
            if (SDDS_SetColumnUnitsConversion(&table, plspec->conversion[i]->name, plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
              fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
            }
          }
        }
        if (plspec->conversion[i]->is_parameter) {
          if (has_wildcards(plspec->conversion[i]->name)) {
            matches = SDDS_MatchParameters(&table, &matchname, SDDS_MATCH_STRING, FIND_ANY_TYPE, plspec->conversion[i]->name, SDDS_0_PREVIOUS|SDDS_OR);
            for (j=0 ; j<matches; j++) {
              if (SDDS_SetParameterUnitsConversion(&table, matchname[j], plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
                fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
              }
              free(matchname[j]);
            }
            if (matchname) free(matchname);
            matchname = NULL;
          } else {
            if (SDDS_SetParameterUnitsConversion(&table, plspec->conversion[i]->name, plspec->conversion[i]->new_units, plspec->conversion[i]->old_units, plspec->conversion[i]->factor) == 0) {
              fprintf(stderr, "Problem with units conversion (read_sddsplot_data)\n");
            }
          }
        }
      }
      for (ireq=1; ireq<plspec->plot_requests; ireq++) {
        plreq = plspec->plot_request+ireq;
        for (ifile=0; ifile<plreq->filenames; ifile++) {
          if (strcmp(filename[file], plreq->filename[ifile])!=0)
            continue;
          if (plreq->frompage && datapage<plreq->frompage)
            continue;
          if (plreq->topage && datapage>plreq->topage)
            continue;
#if defined(DEBUG)
          fprintf(stderr, "memory: %ld\n", memory_count());
          fprintf(stderr, "taking data for file %ld of request %ld\n", ifile, ireq);
#endif
          if (mplFile) {
            requestData[ireq].dataset
              = add_dataset_slots(requestData[ireq].dataset, requestData[ireq].datasets, 1);
            read_mpl_dataset(requestData[ireq].dataset+requestData[ireq].datasets,
                             plreq->filename[ifile], plreq->sparse_interval, ireq, ifile, plreq->mplflags);
            requestData[ireq].datasets++;
            continue;
          }
          if ((!plreq->description_text[ifile] &&
               !SDDS_GetDescription(&table, plreq->description_text+ifile, NULL)) ||
              !SDDS_SetRowFlags(&table, 1))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

          if (!find_datanames_absent(&table, &dataname_absent, &datanames_absent, 
                                     plreq->yname, plreq->y1name,
                                     plreq->xname, plreq->x1name, 
                                     plreq->datanames, plreq->data_class,
                                     plreq->x_exclude_spec.excludeName, 
                                     plreq->x_exclude_spec.was_wildExclude,plreq->x_exclude_spec.excludeNames,
                                     plreq->y_exclude_spec.excludeName, 
                                     plreq->y_exclude_spec.was_wildExclude,plreq->y_exclude_spec.excludeNames)) {
            for (idata=0; idata<plreq->datanames; idata++)  {
              if (plreq->was_wildname[idata])
                continue;
              if (datapage==1) {
                switch (dataname_absent[idata]) {
                case 1:
                case 2: 
                case 3:
                  fprintf(stderr, "warning: (%s, %s) does not appear in %s\n",
                          plreq->xname[idata], plreq->yname[idata],
                          plreq->filename[ifile]);
                  break;
                case 4:
                case 5:
                  fprintf(stderr, "warning: (%s, %s) was excluded from plot.\n",
                          plreq->xname[idata], plreq->yname[idata]);
                  break;
                default:
                  break;
                }
              }
            }
          }
          if (datapage==1 && datanames_absent)
            fprintf(stderr, "%ld of %ld datanames absent from file %s\n",
                    datanames_absent, plreq->datanames, plreq->filename[ifile]);
          datanames = plreq->datanames - datanames_absent;
          if (datanames==0 && datapage==1)  {
            fprintf(stderr, "warning: no datanames in request found for file %s\n",
                    plreq->filename[ifile]);
            continue;
          }

          xtype = SDDS_Realloc(xtype, sizeof(*xtype)*plreq->datanames);
          ytype = SDDS_Realloc(ytype, sizeof(*ytype)*plreq->datanames);
          for (idata=0; idata<plreq->datanames; idata++) {
            if (dataname_absent[idata])
              continue;
            switch (plreq->data_class) {
            case COLUMN_DATA:
              if (plreq->xname[idata] == NULL)
                {
                  xtype[idata] = SDDS_DOUBLE;
                }
              else
                {
                  if (!(xtype[idata] = SDDS_GetNamedColumnType(&table, plreq->xname[idata]))) {
                    fprintf(stderr, "Error: unable to get type for %s for file %s\n", plreq->xname[idata],
                            plreq->filename[ifile]);
                    exit(1);
                  }
                }
              if (!(ytype[idata] = SDDS_GetNamedColumnType(&table, plreq->yname[idata]))) {
                fprintf(stderr, "Error: unable to get type for %s for file %s\n", plreq->yname[idata],
                        plreq->filename[ifile]);
                exit(1);
              }
              break;
            case PARAMETER_DATA:
              if (plreq->xname[idata] == NULL)
                {
                  xtype[idata] = SDDS_DOUBLE;
                }
              else
                {
                  xtype[idata] = SDDS_GetNamedParameterType(&table, plreq->xname[idata]);
                }
              ytype[idata] = SDDS_GetNamedParameterType(&table, plreq->yname[idata]);
              break;
            case ARRAY_DATA:
              if (plreq->xname[idata] == NULL)
                {
                  xtype[idata] = SDDS_DOUBLE;
                }
              else
                {
                  xtype[idata] = SDDS_GetNamedArrayType(&table, plreq->xname[idata]);
                }
              ytype[idata] = SDDS_GetNamedArrayType(&table, plreq->yname[idata]);
              break;
            }
            if (xtype[idata]==SDDS_STRING && ytype[idata]==SDDS_STRING)
              bomb("x and y data cannot both be string type", NULL);
          }

          if (!perform_sddsplot_filtering(&table, plreq->filter, plreq->filters) ||
              !perform_sddsplot_time_filtering(&table,plreq->time_filter,plreq->time_filters) || 
              !perform_sddsplot_matching(&table, plreq->match, plreq->matches))
            continue;
          requestData[ireq].pagesAccepted++;
          points = SDDS_CountRowsOfInterest(&table);
#if defined(DEBUG)
          fprintf(stderr, "memory: %ld\n", memory_count());
          fprintf(stderr, "%ld points of interest\n", points);
#endif
          justAddedSlots = 0;
          if ((iset = requestData[ireq].iset)==-1) {
#if defined(DEBUG)
            fprintf(stderr, "memory: %ld\n", memory_count());
            fprintf(stderr, "adding %ld slots for datasets\n", datanames);
#endif
            requestData[ireq].dataset 
              = add_dataset_slots(requestData[ireq].dataset, requestData[ireq].datasets, datanames);
            iset = requestData[ireq].iset = requestData[ireq].datasets;
            requestData[ireq].datasets += datanames;
            justAddedSlots = 1;
          }
#if defined(DEBUG)
          fprintf(stderr, "memory: %ld\n", memory_count());
          fprintf(stderr, "adding to slot %ld of dataset list\n", iset);
#endif


          if (plreq->split.flags) 
            requestData[ireq].newset = check_for_split(&table, &plreq->split, datapage,plreq->data_class);
	   
#if defined(DEBUG)
          fprintf(stderr, "memory: %ld\n", memory_count());
          fprintf(stderr, "newset = %ld\n", requestData[ireq].newset);
#endif

          if (requestData[ireq].newset && !justAddedSlots) {
            requestData[ireq].dataset 
              = add_dataset_slots(requestData[ireq].dataset, requestData[ireq].datasets, datanames);
            requestData[ireq].iset = iset = requestData[ireq].datasets;
            requestData[ireq].datasets += datanames;
            justAddedSlots = 1;
#if defined(DEBUG)
            fprintf(stderr, "memory: %ld\n", memory_count());
            fprintf(stderr, "new slots added for total of %ld.  iset=%ld\n", requestData[ireq].datasets, 
                    iset);
#endif
          }
          if (justAddedSlots) {
	    
            for (idata=inewdata=0; idata<plreq->datanames; idata++) {
              if (dataname_absent[idata])
                continue;
              requestData[ireq].dataset[iset+inewdata].split_min=DBL_MAX;
              requestData[ireq].dataset[iset+inewdata].split_max=-DBL_MAX;
              switch (plreq->data_class) {
              case COLUMN_DATA:
                record_column_information(&requestData[ireq].dataset[iset+inewdata].info[0], &table, plreq->xname[idata]);
                record_column_information(&requestData[ireq].dataset[iset+inewdata].info[1], &table, plreq->yname[idata]);
                /*
                  for (i=0;i<plreq->conversions;i++) {
                  if (plreq->conversion[i]->is_parameter == 0) {
                  if (plreq->xname[idata] != NULL)
                  {
                  if (wild_match(plreq->xname[idata],plreq->conversion[i]->name)) {
                  SDDS_CopyString(&(&requestData[ireq].dataset[iset+inewdata].info[0])->units, plreq->conversion[i]->new_units);
                  }
                  }
                  if (wild_match(plreq->yname[idata],plreq->conversion[i]->name)) {
                  SDDS_CopyString(&(&requestData[ireq].dataset[iset+inewdata].info[1])->units, plreq->conversion[i]->new_units);
                  }
                  }
                  }
                */
                break;
              case PARAMETER_DATA:
                record_parameter_information(&requestData[ireq].dataset[iset+inewdata].info[0], &table, plreq->xname[idata]);
                record_parameter_information(&requestData[ireq].dataset[iset+inewdata].info[1], &table, plreq->yname[idata]);
                /*
                  for (i=0;i<plreq->conversions;i++) {
                  if (plreq->conversion[i]->is_parameter == 1) {
                  if (wild_match(plreq->xname[idata],plreq->conversion[i]->name)) {
                  SDDS_CopyString(&(&requestData[ireq].dataset[iset+inewdata].info[0])->units, plreq->conversion[i]->new_units);
                  }
                  if (wild_match(plreq->yname[idata],plreq->conversion[i]->name)) {
                  SDDS_CopyString(&(&requestData[ireq].dataset[iset+inewdata].info[1])->units, plreq->conversion[i]->new_units);
                  }
                  }
                  }
                */
                break;
              case ARRAY_DATA:
                record_array_information(&requestData[ireq].dataset[iset+inewdata].info[0], &table, plreq->xname[idata]);
                record_array_information(&requestData[ireq].dataset[iset+inewdata].info[1], &table, plreq->yname[idata]);
                break;
              }
              requestData[ireq].dataset[iset+inewdata].request_index = ireq;
              requestData[ireq].dataset[iset+inewdata].file_index = ifile;
              requestData[ireq].dataset[iset+inewdata].dataname_index = idata;
              requestData[ireq].dataset[iset+inewdata].datapage = datapage;
#if defined(DEBUG)
              fprintf(stderr, "memory: %ld\n", memory_count());
              fprintf(stderr, "datapage %ld from file %ld and dataname %ld of request %ld stored in slot %ld/%ld\n",
                      requestData[ireq].dataset[iset+inewdata].datapage, 
                      requestData[ireq].dataset[iset+inewdata].file_index,
                      requestData[ireq].dataset[iset+inewdata].dataname_index,
                      requestData[ireq].dataset[iset+inewdata].request_index, 
                      requestData[ireq].dataset[iset+inewdata].request_index, 
                      iset+inewdata);
#endif
              determine_dataset_labels(plspec, &table, requestData[ireq].dataset+iset+inewdata);
              determine_dataset_legends(plspec, &table, requestData[ireq].dataset+iset+inewdata, ifile==0);
              determine_dataset_strings(plspec, &table, requestData[ireq].dataset+iset+inewdata);
              determine_dataset_tag(plspec, &table, requestData[ireq].dataset+iset+inewdata);
              determine_dataset_offsets(plspec, &table, requestData[ireq].dataset+iset+inewdata);
              determine_dataset_drawlines(plspec, &table, requestData[ireq].dataset+iset+inewdata);
              inewdata++;
            }
          }
          for (idata=inewdata=0; idata<plreq->datanames; idata++) {
            if (dataname_absent[idata])
              continue;
            x1 = y1 = split = NULL;
            switch (plreq->data_class) {
            case COLUMN_DATA:
              if (points==0)
                continue;
              enumerate = pointLabel = NULL;
              x = y = NULL;
              if (plreq->xname[idata] == NULL)
                {
                  x = (double*)malloc(sizeof(double) * points);
                  for (j=0;j<points;j++) {
                    x[j] = j;
                  }
                }
              else
                {
                  if ((xtype[idata]==SDDS_STRING && !(enumerate=SDDS_GetColumn(&table, plreq->xname[idata]))) ||
                      (xtype[idata]!=SDDS_STRING && !(x=SDDS_GetColumnInDoubles(&table, plreq->xname[idata]))))
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
                }
              if ((ytype[idata]==SDDS_STRING && !(enumerate=SDDS_GetColumn(&table, plreq->yname[idata]))) ||
                  (ytype[idata]!=SDDS_STRING && !(y=SDDS_GetColumnInDoubles(&table, plreq->yname[idata]))))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

              /* add factor here */
              /*
                for (i=0;i<plreq->conversions;i++) {
                if (plreq->conversion[i]->is_parameter == 0) {
                if (plreq->xname[idata] != NULL)
                {
                if (wild_match(plreq->xname[idata],plreq->conversion[i]->name)) {
                if (xtype[idata]==SDDS_STRING) {
                fprintf(stderr, "Warning: cannot convert %s units with factor because it is not a numerical column\n", 
                plreq->xname[idata]);
                } else {
                for (j=0;j<points;j++) {
                x[j] = x[j] * plreq->conversion[i]->factor;
                }
                }
                }
                }
                if (wild_match(plreq->yname[idata],plreq->conversion[i]->name)) {
                if (ytype[idata]==SDDS_STRING) {
                fprintf(stderr, "Warning: cannot convert %s units with factor because it is not a numerical column\n", 
                plreq->yname[idata]);
                } else {
                for (j=0;j<points;j++) {
                y[j] = y[j] * plreq->conversion[i]->factor;
                }
                }
                }
                }
                }
              */
              if ((plreq->x1name[idata] && !(x1=SDDS_GetColumnInDoubles(&table, plreq->x1name[idata]))) ||
                  (plreq->y1name[idata] && !(y1=SDDS_GetColumnInDoubles(&table, plreq->y1name[idata]))) ||
                  (plreq->split.flags&SPLIT_COLUMNBIN && !(split=SDDS_GetColumnInDoubles(&table, plreq->split.name))) ||
                  (plreq->split.flags&SPLIT_PARAMETERCHANGE && !SDDS_GetParameterAsDouble(&table, plreq->split.name,&splitparam)))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
              
              if (plreq->pointLabelSettings.name &&
                  !(pointLabel=SDDS_GetColumnInString(&table, plreq->pointLabelSettings.name)))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
              if (plreq->pointLabelSettings.name && plreq->pointLabelSettings.editCommand)
                edit_strings(pointLabel, points, edit_buffer, plreq->pointLabelSettings.editCommand);
              if (enumerate && plreq->enumerate_settings.flags&ENUM_EDITCOMMANDGIVEN)
                edit_strings(enumerate, points, edit_buffer, plreq->enumerate_settings.editcommand);
              if (plreq->split.flags&SPLIT_PARAMETERCHANGE)
                {
                  split = (double*)malloc(sizeof(double) * points);
                  for (i=0;i<points;i++) {
                    split[i] = splitparam;
                  }
                }
              append_to_dataset(requestData[ireq].dataset+iset+inewdata, x, enumerate, y, x1, y1, split, 
                                pointLabel, points);
              				
              if (x)
                free(x);
              if (enumerate)
                free(enumerate);
              free(y);
              if (x1)
                free(x1);
              if (y1)
                free(y1);
              if (split)
                free(split);
              if (pointLabel)
                free(pointLabel);
              break;
            case PARAMETER_DATA:
	    
              if (plreq->xname[idata] == NULL)
                {
                  xparam = datapage;
                }
              else
                {
                  if (!SDDS_GetParameterAsDouble(&table, plreq->xname[idata], &xparam))
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
                }

              if (!SDDS_GetParameterAsDouble(&table, plreq->yname[idata], &yparam) ||
                  (plreq->y1name[idata] && !SDDS_GetParameterAsDouble(&table, plreq->y1name[idata], &y1param)) ||
                  (plreq->split.flags&SPLIT_PARAMETERCHANGE && !SDDS_GetParameterAsDouble(&table, plreq->split.name,&splitparam)))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	      
              /* add factor here */
              /*
                for (i=0;i<plreq->conversions;i++) {
                if (plreq->conversion[i]->is_parameter == 1) {
                if (wild_match(plreq->xname[idata],plreq->conversion[i]->name)) {
                xparam = xparam * plreq->conversion[i]->factor;
                }
                if (wild_match(plreq->yname[idata],plreq->conversion[i]->name)) {
                yparam = yparam * plreq->conversion[i]->factor;
                }
                }
                }
              */

              append_to_dataset(requestData[ireq].dataset+iset+inewdata, &xparam, NULL, &yparam, 
                                (plreq->x1name[idata]?&x1param:NULL),
                                (plreq->y1name[idata]?&y1param:NULL), ((plreq->split.flags&SPLIT_PARAMETERCHANGE)?&splitparam:NULL), NULL, 1);
              break;
            case ARRAY_DATA:
              
              if (!(y=SDDS_GetArrayInDoubles(&table, plreq->yname[idata], &ny)) ||
                  (plreq->x1name[idata] && !(x1=SDDS_GetArrayInDoubles(&table, plreq->x1name[idata], &nx1))) ||
                  (plreq->y1name[idata] && !(y1=SDDS_GetArrayInDoubles(&table, plreq->y1name[idata], &ny1))))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
/*
              if (!(x=SDDS_GetArrayInDoubles(&table, plreq->xname[idata], &nx)) ||
                  !(y=SDDS_GetArrayInDoubles(&table, plreq->yname[idata], &ny)) ||
                  (plreq->x1name[idata] && !(x1=SDDS_GetArrayInDoubles(&table, plreq->x1name[idata], &nx1))) ||
                  (plreq->y1name[idata] && !(y1=SDDS_GetArrayInDoubles(&table, plreq->y1name[idata], &ny1))))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
*/
              if (plreq->xname[idata] == NULL)
                {
                  nx = ny;
                  x = (double*)malloc(sizeof(double) * nx);
                  for (j=0;j<nx;j++) {
                    x[j] = j;
                  }
                }
              else
                {
                  if (!(x=SDDS_GetArrayInDoubles(&table, plreq->xname[idata], &nx)))
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
                }

              if (nx!=ny || (plreq->x1name[idata] && nx!=nx1) || (plreq->y1name[idata] && ny!=ny1)) {
                char s[16384];
                sprintf(s, "arrays have different numbers of elements: %s, %s", plreq->xname[idata],
                        plreq->yname[idata]);
                if (plreq->x1name[idata]) {
                  strcat(s, ", ");
                  strcat(s, plreq->x1name[idata]);
                }
                if (plreq->y1name[idata]) {
                  strcat(s, ", ");
                  strcat(s, plreq->y1name[idata]);
                }
                SDDS_Bomb(s);
              }
              append_to_dataset(requestData[ireq].dataset+iset+inewdata, x, NULL, y, x1, y1, NULL,
                                NULL, nx);
              break;
            }
            inewdata++;
          }
        }
      }
      if (mplFile) 
        break;
#if defined(DEBUG)
      fprintf(stderr, "Done processing page\n");
#endif
    }
#if defined(DEBUG)
    fprintf(stderr, "Done reading file\n");
#endif
    if (pagesRead==0) {
      fprintf(stderr, "warning: problem reading data from file %s\n",
              filename[file]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    if (!mplFile && !SDDS_Terminate(&table)) 
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  }
#if defined(DEBUG)
  fprintf(stderr, "done reading data from files\n");
  fprintf(stderr, "memory: %ld\n", memory_count());
#endif
  plspec->datasets = 0;
  for (ireq=1; ireq<plspec->plot_requests; ireq++) {
    plspec->datasets += requestData[ireq].datasets;
  }
  if (!(plspec->dataset = (PLOT_DATA*)calloc(plspec->datasets, sizeof(*plspec->dataset)))) 
    SDDS_Bomb("allocation failure (read_sddsplot_data)");
  iset = 0;
  for (ireq=1; ireq<plspec->plot_requests; ireq++) {
    memcpy((char*)(plspec->dataset+iset), (char*)(requestData[ireq].dataset), 
           sizeof(*plspec->dataset)*requestData[ireq].datasets);
    iset += requestData[ireq].datasets;
    free(requestData[ireq].dataset);
  }
  for (file=0; file<files; file++)
    free(filename[file]);
  free(xtype);
  free(ytype);
  free(filename);
  free(requestData);
  free(dataname_absent);
  
}

void read_mpl_dataset(PLOT_DATA *dataset, char *file, long sample_interval, long ireq, long ifile, long flags)
{
  TABLE data;
  if (!sample_interval)
    sample_interval = 1;
  if (!get_table(&data, file, sample_interval, 0)) {
    fprintf(stderr, "error: unable to read mpl file %s\n", file);
    exit(1);
  }
  dataset->request_index = ireq;
  dataset->file_index = ifile;
  dataset->datapage = 0;
  dataset->x = data.c1;
  dataset->y = data.c2;
  dataset->points = data.n_data;
  dataset->x1 = dataset->y1 = NULL;
  if (data.flags&SIGMA_X_PRESENT)
    dataset->x1 = data.s1;
  if (data.flags&SIGMA_Y_PRESENT)
    dataset->y1 = data.s2;
  dataset->split_data = NULL;
  extract_name_and_units(&dataset->info[0], data.xlab);
  extract_name_and_units(&dataset->info[1], data.ylab);
  dataset->label[0] = dataset->label[1] = NULL;
  if (!(flags&MPLFILE_NOTITLE))
    dataset->label[2] = data.title;
  else
    dataset->label[2] = NULL;
  if (!(flags&MPLFILE_NOTOPLINE))
    dataset->label[3] = data.topline;
  else
    dataset->label[3] = NULL;
}

long extract_name_and_units(DATA_INFO *info, char *label)
{
  static char buffer[SDDS_MAXLINE];
  char *ptr, *uptr;

  info->symbol = info->units = NULL;
  SDDS_CopyString(&info->description, label);
  strcpy_ss(buffer, label);
  if ((uptr=strchr(buffer, '('))) {
    *uptr++ = 0;
    if ((ptr=strchr(uptr, ')')))
      *ptr = 0;
    SDDS_CopyString(&info->units, uptr);
  }
  else
    info->units = NULL;
  if (strlen(buffer)) {
    ptr = buffer + strlen(buffer)-1;
    while (ptr!=buffer 
           && *ptr==' ')
      *ptr-- = 0;
  }
  if (strlen(buffer)!=0)
    SDDS_CopyString(&info->symbol, buffer);
  else
    SDDS_CopyString(&info->symbol, "?");
  return 0;
}

long check_for_split(SDDS_TABLE *table, SPLIT_SPEC *split, long datapage,long dataclass)
{
  long index;
  char *string_value;
  double number_value;
  if (split->data_type==0) {
    if (split->flags&SPLIT_PARAMETERCHANGE) {
      if ((index=SDDS_GetParameterIndex(table, split->name))<0 ||
          !(split->data_type=SDDS_GetParameterType(table, index))) {
        fprintf(stderr, "error: problem splitting with parameter %s\n", split->name);
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
      }
    }
    else if (split->flags&SPLIT_COLUMNBIN) {
      if ((index=SDDS_GetColumnIndex(table, split->name))<0 ||
          !(split->data_type=SDDS_GetColumnType(table, index))) {
        fprintf(stderr, "error: problem splitting with column %s\n", split->name);
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
      }
      if (!SDDS_NUMERIC_TYPE(split->data_type)) {
        fprintf(stderr, "error: problem splitting with column %s--not numerical\n", split->name);
        exit(1);
      }
    }
  }
  if (split->flags&SPLIT_PAGES) {
    if (datapage!=1 &&
        (!(split->flags&SPLIT_PAGES_INTERVAL) || datapage%split->interval==0))
      return 1;
    return 0;
  }
  if (split->flags&SPLIT_PARAMETERCHANGE && dataclass !=PARAMETER_DATA) {
         
    if ((split->data_type==SDDS_STRING && 
         !SDDS_GetParameter(table, split->name, &string_value)) ||
        !SDDS_GetParameterAsDouble(table, split->name, &number_value)) {
      fprintf(stderr, "error: unable to get value for parameter %s\n",
              split->name);
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
    }
    if (!split->value_valid) {
      if (split->data_type==SDDS_STRING)
        SDDS_CopyString(&split->ref_string, string_value);
      else {
        if (!(split->flags&SPLIT_CHANGE_START))
          split->start = number_value;
        split->ref_value = number_value;
      }
      if (split->min > split->ref_value)
        split->min=split->ref_value;
      if (split->max < split->ref_value)
        split->max=split->ref_value;
      split->value_valid = 1;
      
    }
    else {
      if (split->data_type==SDDS_STRING) {
        if (strcmp(string_value, split->ref_string)==0)
          return 0;
        if (split->ref_string)
          free(split->ref_string);
        split->ref_string = string_value;
        string_value = NULL;
        if (split->min > split->ref_value)
          split->min=split->ref_value;
        if (split->max < split->ref_value)
          split->max=split->ref_value;
        return datapage!=1;
	
      }
      else {
        if (!(split->flags&SPLIT_CHANGE_WIDTH)) {
          if (split->ref_value==number_value)
            return 0;
          split->ref_value = number_value;
          return datapage!=1;
	  
        }
        if ( ((long)((number_value-split->start)/split->width)) ==
             ((long)((split->ref_value-split->start)/split->width)) )
          return 0;
        split->ref_value = number_value;
        if (split->min > split->ref_value)
          split->min=split->ref_value;
        if (split->max < split->ref_value)
          split->max=split->ref_value;
        return datapage!=1;
	  
      }
    }
  }
  
  if (split->flags&SPLIT_PARAMETERCHANGE) {
    if (SDDS_GetParameterInformation(table, "symbol",&split->symbol,SDDS_GET_BY_NAME, split->name)!=SDDS_STRING ||
        SDDS_GetParameterInformation(table, "units", &split->units, SDDS_GET_BY_NAME, split->name)!=SDDS_STRING ) 
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  }     
  if (split->flags&SPLIT_COLUMNBIN) {
    if (SDDS_GetColumnInformation(table, "symbol",&split->symbol,SDDS_GET_BY_NAME, split->name)!=SDDS_STRING ||
        SDDS_GetColumnInformation(table, "units", &split->units, SDDS_GET_BY_NAME, split->name)!=SDDS_STRING ) 
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  }
  if (!(split->symbol))
    split->symbol=split->name;
  return 0;
}

void determine_dataset_legends(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset, long first_file)
{
  PLOT_REQUEST *request;
  static char buffer[SDDS_MAXLINE];

  request = plspec->plot_request+dataset->request_index;
  dataset->legend = NULL;
  if (request->legend.code&LEGEND_FIRSTFILEONLY && !first_file)
    return;
  if (request->legend.code&LEGEND_YSYMBOL) {
    if (request->legend.code&LEGEND_UNITS && dataset->info[1].units && !SDDS_StringIsBlank(dataset->info[1].units)) 
      sprintf(buffer, "%s (%s)", dataset->info[1].symbol, dataset->info[1].units);
    else 
      sprintf(buffer, "%s", dataset->info[1].symbol);
    SDDS_CopyString(&dataset->legend, buffer);
  }
  else if (request->legend.code&LEGEND_XSYMBOL) {
    if (request->legend.code&LEGEND_UNITS && dataset->info[0].units && !SDDS_StringIsBlank(dataset->info[1].units)) 
      sprintf(buffer, "%s (%s)", dataset->info[0].symbol, dataset->info[0].units);
    else 
      sprintf(buffer, "%s", dataset->info[0].symbol);
    SDDS_CopyString(&dataset->legend, buffer);
  }
  else if (request->legend.code&LEGEND_YNAME) {
    if (request->legend.code&LEGEND_UNITS && dataset->info[1].units && !SDDS_StringIsBlank(dataset->info[1].units)) 
      sprintf(buffer, "%s (%s)", request->yname[dataset->dataname_index], dataset->info[1].units);
    else 
      sprintf(buffer, "%s", request->yname[dataset->dataname_index]);
    SDDS_CopyString(&dataset->legend, buffer);
  }
  else if (request->legend.code&LEGEND_XNAME) {
    if (request->legend.code&LEGEND_UNITS && dataset->info[0].units && !SDDS_StringIsBlank(dataset->info[0].units)) 
      sprintf(buffer, "%s (%s)", request->xname[dataset->dataname_index], dataset->info[0].units);
    else 
      sprintf(buffer, "%s", request->xname[dataset->dataname_index]);
    SDDS_CopyString(&dataset->legend, buffer);
  }
  else if (request->legend.code&LEGEND_YDESCRIPTION)
    SDDS_CopyString(&dataset->legend, dataset->info[1].description);
  else if (request->legend.code&LEGEND_XDESCRIPTION)
    SDDS_CopyString(&dataset->legend, dataset->info[0].description);
  else if (request->legend.code&LEGEND_FILENAME) 
    SDDS_CopyString(&dataset->legend, request->filename[dataset->file_index]);
  else if (request->legend.code&LEGEND_SPECIFIED)
    SDDS_CopyString(&dataset->legend, request->legend.value);
  else if (request->legend.code&LEGEND_ROOTNAME) {
    SDDS_CopyString(&dataset->legend, request->filename[dataset->file_index]);
    shorten_filename(dataset->legend);
  }
  else if (request->legend.code&LEGEND_PARAMETER) {
    /* if (!table) {
       fprintf(stderr, "error: unable to get value of parameter %s from file %s---not a SDDS file\n",
       request->legend.value, request->filename[dataset->file_index]);
       exit(1);
       }
       if (!SDDS_GetParameter(table, request->legend.value, &dataset->legend)) {
       fprintf(stderr, "error: unable to get value of parameter %s from file %s\n", 
       request->legend.value, request->filename[dataset->file_index]);
       SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
       exit(1);
       } */
    long type;
    char *dataBuffer = NULL;
    char *format=NULL;
      
    if ( !(dataBuffer = malloc(sizeof(double)*4)))
      SDDS_Bomb("Allocation failure in determin_dataset_labels");
    if (!SDDS_GetParameter(table,request->legend.value, dataBuffer))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);  
    if(!(type=SDDS_GetParameterType(table,SDDS_GetParameterIndex(table,request->legend.value))))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);	 
    if(SDDS_GetParameterInformation(table,"format_string",&format,SDDS_GET_BY_NAME,request->legend.value) !=SDDS_STRING)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);  	   
      
    if(request->legend.code&LEGEND_FORMAT) {
      if(!SDDS_VerifyPrintfFormat(request->legend.format,type)) {
        fprintf(stderr, "error: given format (\"%s\") for parameter %s is invalid\n",
                request->legend.format, request->legend.value);
        exit(1);
      }   
      if(!(SDDS_SprintTypedValue((void*)dataBuffer,0,type,request->legend.format,buffer,SDDS_PRINT_NOQUOTES)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      SDDS_CopyString(&dataset->legend,buffer); 
    }	else {
      if(!SDDS_StringIsBlank(format)) {
        if(!(SDDS_SprintTypedValue((void*)dataBuffer,0,type,format,buffer,SDDS_PRINT_NOQUOTES)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        SDDS_CopyString(&dataset->legend,buffer);  
      } else {   
        if (!(dataset->legend=SDDS_GetParameterAsString(table,request->legend.value,NULL)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      }      
    }
    free(dataBuffer);
    free(format);	    
  }
  else
    return;
  if (request->legend.code&LEGEND_EDIT && request->legend.edit_command) {
#if defined(DEBUG)
    fprintf(stderr, "editing legend %s with edit command %s\n",
            dataset->legend, request->legend.edit_command);
#endif
    strcpy_ss(buffer, dataset->legend); 
    edit_string(buffer, request->legend.edit_command);
    free(dataset->legend);
    SDDS_CopyString(&dataset->legend, buffer);
  }
}

void determine_dataset_strings(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset)
{
  PLOT_REQUEST *request;
  static char buffer[SDDS_MAXLINE];
  long i, j;

  request = plspec->plot_request+dataset->request_index;
  dataset->string_labels = request->string_labels;
  dataset->string_label = tmalloc(sizeof(*dataset->string_label)*dataset->string_labels);
  for (i=0; i<dataset->string_labels; i++) {
    dataset->string_label[i] = request->string_label[i];
    dataset->string_label[i].string = dataset->string_label[i].edit_command = 
      dataset->string_label[i].justify_mode = NULL;
#if defined(DEBUG)
    fprintf(stderr, "string label %ld for request: %s\n",
            i, request->string_label[i].string);
    fprintf(stderr, "  position=%e, %e  flags=%x  jmode=%s  scale=%e\n",
            request->string_label[i].position[0], request->string_label[i].position[1],
            request->string_label[i].flags, request->string_label[i].justify_mode,
            request->string_label[i].scale);
#endif
    if (request->string_label[i].flags&LABEL_JUSTIFYMODE_GIVEN)
      SDDS_CopyString(&dataset->string_label[i].justify_mode,
                      request->string_label[i].justify_mode);
    for (j=0; j<2; j++) {
      if (request->string_label[i].flags&(LABEL_XPARAM_GIVEN<<j)) {
        if (!SDDS_GetParameterAsDouble(table, request->string_label[i].positionParameter[j],
                                       &dataset->string_label[i].position[j])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        }
      }
    }
       
	if (request->string_label[i].flags&LABEL_PARAMETER_GIVEN) {
      if (request->string_label[i].flags&LABEL_FORMAT_GIVEN) {
        if (!(dataset->string_label[i].string=SDDS_GetParameterAsFormattedString(table, request->string_label[i].string, NULL, request->string_label[i].format))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        }
      } else {
        if (!(dataset->string_label[i].string=SDDS_GetParameterAsFormattedString(table, request->string_label[i].string, NULL, NULL))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        }
      }
    } else {
      if (!SDDS_CopyString(&dataset->string_label[i].string, request->string_label[i].string)) {
        fprintf(stderr, "error: unable to copy label string\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
	}	
	
#if defined(DEBUG)
    fprintf(stderr, "string label %ld for dataset: %s\n",
            i, dataset->string_label[i].string);
    fprintf(stderr, "  position=%e, %e  flags=%x  jmode=%s  scale=%e\n\n",
            dataset->string_label[i].position[0], dataset->string_label[i].position[1],
            dataset->string_label[i].flags, dataset->string_label[i].justify_mode,
            dataset->string_label[i].scale);
#endif
    if (request->string_label[i].flags&LABEL_EDITCOMMAND_GIVEN &&
        request->string_label[i].edit_command) {
      strcpy_ss(buffer, dataset->string_label[i].string); 
      edit_string(buffer, request->string_label[i].edit_command);
      free(dataset->string_label[i].string);
      SDDS_CopyString(&dataset->string_label[i].string, buffer);
    }
  }
}

void determine_dataset_drawlines(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset)
{
  PLOT_REQUEST *request;
  long i, j;
  double *positionPtr;
  unsigned long flagMask, flagSubs;
  char **namePtr;
  
  request = plspec->plot_request+dataset->request_index;
  dataset->drawLineSpecs = request->drawLineSpecs;
  dataset->drawLineSpec = tmalloc(sizeof(*dataset->drawLineSpec)*dataset->drawLineSpecs);
  for (i=0; i<dataset->drawLineSpecs; i++) {
    dataset->drawLineSpec[i] = request->drawLineSpec[i];
    dataset->drawLineSpec[i].flags &= 
      ~(DRAW_LINE_X0PARAM+DRAW_LINE_Y0PARAM+DRAW_LINE_P0PARAM+DRAW_LINE_Q0PARAM+
        DRAW_LINE_X1PARAM+DRAW_LINE_Y1PARAM+DRAW_LINE_P1PARAM+DRAW_LINE_Q1PARAM);
    dataset->drawLineSpec[i].x0Param = 
      dataset->drawLineSpec[i].y0Param = 
      dataset->drawLineSpec[i].p0Param = 
      dataset->drawLineSpec[i].q0Param = 
      dataset->drawLineSpec[i].x1Param = 
      dataset->drawLineSpec[i].y1Param = 
      dataset->drawLineSpec[i].p1Param = 
      dataset->drawLineSpec[i].q1Param = NULL;
    flagMask = DRAW_LINE_X0PARAM;
    flagSubs = DRAW_LINE_X0GIVEN;
    positionPtr = &(dataset->drawLineSpec[i].x0);
    namePtr = &(request->drawLineSpec[i].x0Param);
    for (j=0; j<8; j++) {
      if (!(request->drawLineSpec[i].flags&flagMask)) {
        flagMask = flagMask << 1;
        flagSubs = flagSubs << 1;
        positionPtr += 1;
        namePtr += 1;
        continue;
      }
      if (!SDDS_GetParameterAsDouble(table, *namePtr, positionPtr)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      }
      dataset->drawLineSpec[i].flags |= flagSubs;
      flagMask = flagMask << 1;
      flagSubs = flagSubs << 1;
      positionPtr += 1;
      namePtr += 1;
    }
  }
}


void determine_dataset_offsets(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset)
{
  PLOT_REQUEST *request;
  long i;

  request = plspec->plot_request+dataset->request_index;
  for (i=0; i<2; i++) {
    dataset->offset[i] = 0;
    if (request->offset_flags&(OFFSET_XPARAMETER_GIVEN<<i)) {
      if (!SDDS_GetParameterAsDouble(table, request->offset_parameter[i], dataset->offset+i))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    }
    if (request->offset_flags&(OFFSET_XINVERT_GIVEN<<i)) 
      dataset->offset[i] *= -1;
  }
  for (i=0; i<2; i++) {
    dataset->factor[i] = 0;
    if (request->factor_flags&(FACTOR_XPARAMETER_GIVEN<<i)) {
      if (!SDDS_GetParameterAsDouble(table, request->factor_parameter[i], dataset->factor+i))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    }
    if (request->factor_flags&(FACTOR_XINVERT_GIVEN<<i))
      dataset->factor[i] = 1/dataset->factor[i];
  }
}


void determine_dataset_tag(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset)
{
  PLOT_REQUEST *request;

  request = plspec->plot_request+dataset->request_index;
  if (!request->tag_parameter) {
    SDDS_CopyString(&dataset->tag, request->user_tag);
    return;
  }
  if (!(dataset->tag=SDDS_GetParameterAsString(table, request->tag_parameter,NULL)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  /*if (!SDDS_GetParameterAsDouble(table, request->tag_parameter, &dataset->tag))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors); */
}

long CheckForMplFile(char *filename, PLOT_REQUEST *plreq, long requests)
{
  long ifile, ireq;
  for (ireq=1; ireq<requests; ireq++) {
    for (ifile=0; ifile<plreq[ireq].filenames; ifile++) 
      if (plreq[ireq].mplflags&MPLFILE && strcmp(plreq[ireq].filename[ifile], filename)==0)
        return 1;
  }
  return 0;
}

                
