/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sddsplotConvert.c
 * 
 * part of sddsplot (plotting program for SDDS files)
 *
 * Michael Borland, 1994.
 $Log: not supported by cvs2svn $
 Revision 1.51  2012/02/24 17:16:35  shang
 modified to printout warning message when the leftPoints is less than original datapoints after logarithm for y-log mode.

 Revision 1.50  2010/10/08 22:07:40  borland
 When the -factor option is given, now scales the error bar values as well.

 Revision 1.49  2009/07/28 14:01:16  borland
 Added scroll feature to -replicate option.  Greatly improved memory management for
 split and replicated datasets.  Added -nocolorbar option.

 Revision 1.48  2009/06/26 19:18:21  borland
 Removed debugging statemetn.

 Revision 1.47  2009/06/26 18:58:28  borland
 Added scrolled mode to the -replicate option.  Intended use:
 sddsplot -layout=2,2 -groupby=page,req -sep=names \
          -col=t,p -col=x,xp -col=y,yp -title=@Pass run.w1 -graph=dot -split=page \
          -column=Pass,Sdelta run.w2 -replicate=match=page,scroll -same -graph=sym,con
 where run.w1 is a phase space file and run.w2 is a turn-by-turn parameters file.

 Revision 1.46  2009/03/09 14:43:16  borland
 Some corrections to autolog mode.

 Revision 1.45  2009/03/04 23:59:54  borland
 Removed a pointless logical operation.

 Revision 1.44  2009/02/27 22:51:54  borland
 Added autolog mode.

 Revision 1.43  2008/08/27 20:19:45  soliday
 Fixed a big bug when the -split option has 0 for the width. It was reversing
 the min and max and if you looked at the intensity bar closely you would see
 the numbers in the wrong order. This also caused a problem under certain
 conditions where the plot would not even appear.

 Revision 1.42  2008/08/27 19:17:49  soliday
 Fixed a problem with the last change when there are multiple plot requests.

 Revision 1.41  2008/08/22 18:50:36  soliday
 Updated so that no additional sub-options are needed for the
 -split=columnBin=<column-name> option. It will now automatically
 bin the column data in 100 equal size bins from the min to max values.

 Revision 1.40  2008/08/20 18:37:47  soliday
 Updated so that when plotting multiple pages based on the -split columnBin
 option, it will now locate the maximum value for all the panels and the
 intensity bar will be identical for each panel. Before the maximum on the
 intensity bar on a panel was independent of the other panels.

 Revision 1.39  2007/04/20 19:39:33  borland
 Fixed serious bugs with enumerated value plotting.
 Changed order of routines in the main() procedure so that assign_enumerated_values() gets
 run properly.
 Fixed the sorting algorithm in assign_enumerated_values() so that it assigns unique values
 for unique strings, without messing up the order from the data file.

 Revision 1.38  2006/08/21 21:58:52  jiaox
 Fixed the bug that the -paramemterChange with -split option did not display the proper color codings.

 Revision 1.37  2006/03/21 17:52:00  soliday
 Fixed problems with the intesity bar with ceps and png output. Also modified
 the split by column data option to automatically find the minimum value
 for a plot request and use that as the start value if no start value is given.
 This enables multiple plots to have the same scale even if they are in
 different parts of the range.

 Revision 1.36  2004/08/02 18:43:28  borland
 Added -dither option.  It can be used to make scatter plots look more like
 density plots, particularly for digitized data.

 Revision 1.35  2003/09/02 19:29:28  soliday
 Cleaned up code for Linux.

 Revision 1.34  2003/07/22 21:07:00  soliday
 IEEE functions are always used.

 Revision 1.33  2002/12/04 00:34:50  borland
 Added fractional-deviation mode to -mode option:
         -mode=[xy]FractionalDeviation
 Added value-from-file and invert features to -factor option:
         -factor=[xy]Parameter=<name>,[xy]Invert
 Fixed invert feature for -offset option (was ignored when the offset
 was given literally but used when given as parameter).

 Revision 1.32  2002/08/14 17:24:54  soliday
 Added Open License

 Revision 1.31  2002/07/19 14:14:11  borland
 Fixed end-offset mode for y plane.

 Revision 1.30  2002/07/18 22:36:32  borland
 Added "end offset" mode.

 Revision 1.29  2002/04/22 21:24:56  soliday
 The common offset is now only common for each plot request.

 Revision 1.28  2002/04/22 20:33:45  soliday
 Added common offset mode.

 Revision 1.27  2000/11/27 15:07:02  borland
 Now properly converts error bars for log mode.

 Revision 1.26  2000/10/11 21:50:40  soliday
 Changed definition of isinf so that sunmath is no longer needed.

 Revision 1.25  2000/08/10 21:17:26  soliday
 Added support for gcc on Solaris.

 Revision 1.24  2000/06/07 20:13:00  soliday
 Added missing sunmath.h include file.

 Revision 1.23  1999/07/22 18:35:40  soliday
 Added WIN32 support

 Revision 1.22  1999/06/03 16:08:56  soliday
 Removed compiler warnings under linux.

 Revision 1.21  1999/05/18 20:34:27  borland
 Now allows width parameter for -split to be <0.

 Revision 1.20  1999/02/03 19:38:51  borland
 Improved operation of -range option when the center/range is undefined.
 Improved operation of time-scales when the time range is undefined.

 Revision 1.19  1998/08/26 14:48:27  borland
 Treatment of IEEE math function isinf is now uniform.  If on solaris
 and sunmath is missing, then modify mdb.h and Makefile.Unix.

 Revision 1.18  1998/08/25 15:05:35  borland
 New version with major changes to allow multiple scales on x and y axes.

 Revision 1.16  1997/07/29 17:08:56  borland
 Fixed bugs in handling of labels for transposition.  Now accepts user's
 labels.

 Revision 1.15  1997/04/30 16:30:38  borland
 Moved two blocks of code in perform_dataset_conversions; these blocks
 change data values according to offset info.  Put them just before
 other similar blocks.  Also added code to change -limits limit values
 accordingly.

 Revision 1.14  1996/11/11 23:58:11  borland
 Fixed bug in the way datasets were being copied during replication.
 Was overwriting existing datasets in some circumstances.

 Revision 1.13  1996/11/08 16:54:57  borland
 Implemented -replicate option.

 Revision 1.12  1996/08/06 22:28:56  borland
 Fixed bug in staggering.  Was applying staggering to all plot panels when
 request structure indicated only certain panels were affected.

 * Revision 1.11  1996/04/10  21:37:06  borland
 * Implemented -pointLabel option for labeling data points with string values
 * from data files.
 *
 * Revision 1.10  1996/02/24  00:10:25  borland
 * Fixed bug in columnbin_sddsplot_data.  Was making 1 too many bins in some
 * cases; wasn't putting the minimum-valued point in a bin.
 *
 * Revision 1.9  1996/02/09  00:07:32  borland
 * Added xtime and ytime qualifiers to -ticksettings option, along with code
 * to do time-labeled axes (assumes time values are times in seconds since
 * epoch).
 *
 * Revision 1.8  1996/02/08  00:59:15  borland
 * Enhanced -offset option to allow offseting data based on values in parameters.
 *
 * Revision 1.7  1996/02/01  23:34:18  borland
 * Fixed problems with offset mode plotting when a page was column-binned.
 * Had to add xOrig0 and yOrig0 variables to PLOT_DATA structure to keep
 * track of original 0,0 values for data set prior to splitting.
 *
 * Revision 1.6  1996/01/16  18:49:15  borland
 * Fixed problem in -limit=autoscaling when data was transformed (have to
 * transform limits also).  Fixed bugs in enumerate scale routines that
 * resulted in misplaced ticks; fixed autoscaling of y enumerated labels.
 *
 * Revision 1.5  1995/12/18  17:27:57  borland
 * Fixed bug in staggering when neither STAGGER_FILES nor STAGGER_DATANAMES is on;
 * wasn't setting last_* variables properly when moving to a new panel.
 *
 * Revision 1.4  1995/12/09  03:28:20  borland
 * Changed handling of enumerated data so that column-binning will work without
 * mixing up the data order from page to page.
 *
 * Revision 1.3  1995/09/25  16:39:22  borland
 * Changed adjust_sddsplot_overlay_data to return 0/1 if overlay not possible/
 * possible.  Added warning message to sparse_sample_clip if all data is removed
 * by winnowing.
 *
 * Revision 1.2  1995/09/05  21:09:48  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "graph.h"
#include "SDDS.h"
#include "sddsplot.h"
#include <ctype.h>
#include <time.h>
#if defined(_WIN32)
#include <float.h>
/*#define isnan(x) _isnan(x)*/
#endif

void applyOffsets(PLOT_SPEC *plspec, long request, PLOT_DATA *dataset, short yPlane) 
{
  long i;
  PLOT_REQUEST *plreq;
  plreq = plspec->plot_request+request;
  if (!yPlane) {
    if (plspec->plot_request[request].offset_flags&OFFSET_XPARAMETER_GIVEN) {
      plreq->limit.cxmin += dataset->offset[0];
      plreq->limit.cxmax += dataset->offset[0];
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->x[i] += dataset->offset[0];
    }
    if (plspec->plot_request[request].offset_flags&OFFSET_XCHANGE_GIVEN) {
      plreq->limit.cxmin += plspec->plot_request[request].offset[0];
      plreq->limit.cxmax += plspec->plot_request[request].offset[0];
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->x[i] += plspec->plot_request[request].offset[0];
    }
  } else {
    if (plspec->plot_request[request].offset_flags&OFFSET_YCHANGE_GIVEN) {
      plreq->limit.cymin += plspec->plot_request[request].offset[1];
      plreq->limit.cymax += plspec->plot_request[request].offset[1];
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->y[i] += plspec->plot_request[request].offset[1];
    }
    if (plspec->plot_request[request].offset_flags&OFFSET_YPARAMETER_GIVEN) {
      plreq->limit.cymin += dataset->offset[1];
      plreq->limit.cymax += dataset->offset[1];
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->y[i] += dataset->offset[1];
    }
  }
}

void applyFactors(PLOT_SPEC *plspec, long request, PLOT_DATA *dataset, short yPlane) 
{
  long i;
  PLOT_REQUEST *plreq;
  plreq = plspec->plot_request+request;
  if (!yPlane) {
    if (plspec->plot_request[request].factor_flags&FACTOR_XPARAMETER_GIVEN) {
      plreq->limit.cxmin *= dataset->factor[0];
      plreq->limit.cxmax *= dataset->factor[0];
      if (plreq->limit.cxmax<plreq->limit.cxmin)
        SWAP_DOUBLE(plreq->limit.cxmax, plreq->limit.cxmin);
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->x[i] *= dataset->factor[0];
      if (dataset->x1)
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->x1[i] *= dataset->factor[0];
    }
    if (plspec->plot_request[request].factor_flags&FACTOR_XMULT_GIVEN) {
      plreq->limit.cxmin *= plspec->plot_request[request].factor[0];
      plreq->limit.cxmax *= plspec->plot_request[request].factor[0];
      if (plreq->limit.cxmax<plreq->limit.cxmin)
        SWAP_DOUBLE(plreq->limit.cxmax, plreq->limit.cxmin);
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->x[i] *= plspec->plot_request[request].factor[0];
      if (dataset->x1)
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->x1[i] *= plspec->plot_request[request].factor[0];
    }
  } else {
    if (plspec->plot_request[request].factor_flags&FACTOR_YPARAMETER_GIVEN) {
      plreq->limit.cymin *= dataset->factor[1];
      plreq->limit.cymax *= dataset->factor[1];
      if (plreq->limit.cymax<plreq->limit.cymin)
        SWAP_DOUBLE(plreq->limit.cymax, plreq->limit.cymin);
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->y[i] *= dataset->factor[1];
      if (dataset->y1)
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->y1[i] *= dataset->factor[1];
    }
    if (plspec->plot_request[request].factor_flags&FACTOR_YMULT_GIVEN) {
      plreq->limit.cymin *= plspec->plot_request[request].factor[1];
      plreq->limit.cymax *= plspec->plot_request[request].factor[1];
      if (plreq->limit.cymax<plreq->limit.cymin)
        SWAP_DOUBLE(plreq->limit.cymax, plreq->limit.cymin);
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->y[i] *= plspec->plot_request[request].factor[1];
      if (dataset->y1)
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->y1[i] *= plspec->plot_request[request].factor[1];
    } 
  }
}

void sparse_sample_clip(PLOT_SPEC *plspec)
{
  long i, j, npts, dataset;
  PLOT_REQUEST *plreq;
  double *x, *y, *x1, *y1, *split;
  long random_number_seed;
  char **enumerate, **pointLabel;

  random_number_seed = (long)time((time_t*)NULL);
  random_number_seed = 2*(random_number_seed/2) + 1;
  random_1(-labs(random_number_seed));

  for (dataset=0; dataset<plspec->datasets; dataset++) {
    plreq = plspec->plot_request+plspec->dataset[dataset].request_index;
    if (!plspec->dataset[dataset].points)
      continue;
    x  = plspec->dataset[dataset].x;
    y  = plspec->dataset[dataset].y;
    x1 = plspec->dataset[dataset].x1;
    y1 = plspec->dataset[dataset].y1;
    split = plspec->dataset[dataset].split_data;
    enumerate = plspec->dataset[dataset].enumerate;
    pointLabel = plspec->dataset[dataset].pointLabel;
    
    plspec->dataset[dataset].points = remove_invalid_data(x, y, x1, y1, split, enumerate, pointLabel,
                                                          plspec->dataset[dataset].points);

    if (plreq->sparse_interval) {
      npts = plspec->dataset[dataset].points;
      j = 0;
      for (i = plreq->sparse_offset; i<npts; i+=plreq->sparse_interval, j++) {
        x[j] = x[i];
        y[j] = y[i];
        if (x1)
          x1[j] = x1[i];
        if (y1)
          y1[j] = y1[i];
        if (split)
          split[j] = split[i];
        if (enumerate)
          enumerate[j] = enumerate[i];
        if (pointLabel)
          pointLabel[j] = pointLabel[i];
      }
      plspec->dataset[dataset].points = j;
    }

    if (plreq->sample_fraction && plreq->sample_fraction!=1) {
      npts = plspec->dataset[dataset].points;
      for (i=j=0; i<npts; i++) {
        if (random_1(1)<plreq->sample_fraction) {
          x[j] = x[i];
          y[j] = y[i];
          if (x1)
            x1[j] = x1[i];
          if (y1)
            y1[j] = y1[i];
          if (split)
            split[j] = split[i];
          if (enumerate)
            enumerate[j] = enumerate[i];
          if (pointLabel)
            pointLabel[j] = pointLabel[i];
          j++;
        }
      }
      plspec->dataset[dataset].points = j;
    }

    if (plreq->limit.flags && !(plreq->limit.flags&LIMIT_AUTOSCALING)) {
      for (i=j=0; i<plspec->dataset[dataset].points; i++) {
        if ((plreq->limit.flags&LIMIT_XMIN_GIVEN && x[i]<plreq->limit.xmin) ||
            (plreq->limit.flags&LIMIT_XMAX_GIVEN && x[i]>plreq->limit.xmax) ||
            (plreq->limit.flags&LIMIT_YMIN_GIVEN && y[i]<plreq->limit.ymin) ||
            (plreq->limit.flags&LIMIT_YMAX_GIVEN && y[i]>plreq->limit.ymax))
          continue;
        x[j] = x[i];
        y[j] = y[i];
        if (x1)
          x1[j] = x1[i];
        if (y1)
          y1[j] = y1[i];
        if (split)
          split[j] = split[i];
        if (enumerate)
          enumerate[j] = enumerate[i];
        if (pointLabel)
          pointLabel[j] = pointLabel[i];
        j++;
      }
      plspec->dataset[dataset].points = j;
    }

    if (plreq->clip_head || plreq->clip_tail) {
      npts = plspec->dataset[dataset].points;
      if (plreq->flags&PLREQ_INVERTCLIP) {
        if ((j = plreq->clip_head)>npts)
          j = npts;
        if ((i = npts-plreq->clip_tail)<0)
          i = 0;
        for ( ; i<npts; i++, j++) {
          x[j] = x[i];
          y[j] = y[i];
          if (x1)
            x1[j] = x1[i];
          if (y1)
            y1[j] = y1[i];
          if (split)
            split[j] = split[i];
          if (enumerate)
            enumerate[j] = enumerate[i];
          if (pointLabel)
            pointLabel[j] = pointLabel[i];
        }
        plspec->dataset[dataset].points = j;
      }
      else {
        j = 0;
        for (i=plreq->clip_head; i<npts-plreq->clip_tail; i++, j++) {
          x[j] = x[i];
          y[j] = y[i];
          if (x1)
            x1[j] = x1[i];
          if (y1)
            y1[j] = y1[i];
          if (split)
            split[j] = split[i];
          if (enumerate)
            enumerate[j] = enumerate[i];
          if (pointLabel)
            pointLabel[j] = pointLabel[i];
        }
        plspec->dataset[dataset].points = j;
      }
    }
    if (!plspec->dataset[dataset].points) {
      fprintf(stderr, "warning: no data left after winnowing (%s, %s) from file %s\n",
              plreq->xname[plspec->dataset[dataset].dataname_index],
              plreq->yname[plspec->dataset[dataset].dataname_index],
              plreq->filename[plspec->dataset[dataset].file_index]);
    }
    /* might not be a bad idea to resize arrays here */
  }
}


void perform_dataname_swapping(PLOT_SPEC *plspec)
{
  long request, dataname;
  PLOT_REQUEST *plreq;
  for (request=1; request<plspec->plot_requests; request++) {
    plreq = plspec->plot_request+request;
    if (!(plreq->flags&PLREQ_SWAP))
      continue;
    for (dataname=0; dataname<plreq->datanames; dataname++) {
      SWAP_PTR(plreq->xname[dataname], plreq->yname[dataname]);
      if (plreq->x1name && plreq->y1name)
        SWAP_PTR(plreq->x1name[dataname], plreq->y1name[dataname]);
    }
  }
}

typedef struct {
  char *string;
  double *indexData;
  long originalIndex;
} ENUM_SORT_DATA;

int compareEnumSortData(const void *e1, const void *e2)
{
  ENUM_SORT_DATA *es1, *es2;
  int comparison;
  
  es1 = (ENUM_SORT_DATA*)e1;
  es2 = (ENUM_SORT_DATA*)e2;
  if ((comparison=strcmp(es1->string, es2->string))==0)
    comparison = es1->originalIndex - es2->originalIndex;
  return comparison;
}

int compareEnumSortData2(const void *e1, const void *e2)
{
  ENUM_SORT_DATA *es1, *es2;
  
  es1 = (ENUM_SORT_DATA*)e1;
  es2 = (ENUM_SORT_DATA*)e2;
  return es1->originalIndex - es2->originalIndex;
}

void assign_enumerate_values(PLOT_SPEC *plspec)
/* Assigns integer values to x or y arrays to allow plotting enumerated
 * data.  Also checks for illegal requests or conditions. 
 */
{
  long group, panel, set, plane, point, enums, iEnum, dataValue;
  long *datasetsFound, *enumsFound, *enumPoints, maxGroups, datasets;
  PLOT_DATA **dataset;
  ENUM_SORT_DATA *enumData;

  maxGroups = MAX(plspec->scalesGroups[0], plspec->scalesGroups[1]);
  if (!(datasetsFound=SDDS_Malloc(sizeof(*datasetsFound)*maxGroups)) ||
      !(enumPoints=SDDS_Malloc(sizeof(*enumPoints)*maxGroups)) ||
      !(enumsFound=SDDS_Malloc(sizeof(*enumsFound)*maxGroups)) ) {
    SDDS_Bomb("Memory allocation failure (assign_enumerate_values)");
    exit(1);
  }
  for (panel=0; panel<plspec->panels; panel++) {
    /* Do the analysis on a panel basis.  This is redundant in some cases,
     * but necessary in others. 
     */
    datasets = plspec->panel[panel].datasets;
    dataset  = plspec->panel[panel].dataset;

    /* The analysis is independent for each plane. */
    for (plane=0; plane<2; plane++) {
      /* Count the number of datasets for this panel for this plane for
       * each group. Count the number of enumerated data sets.
       */
      for (group=0; group<plspec->scalesGroups[plane]; group++)
        enumsFound[group] = enumPoints[group] = datasetsFound[group] = 0;
      for (set=0; set<datasets; set++) {
        group = dataset[set]->scalesGroupIndex[plane];
        datasetsFound[group] += 1;
        if (dataset[set]->enumerate && dataset[set]->enumPlane==plane) {
          enumsFound[group] += 1;
          enumPoints[group] += dataset[set]->points;
        } else if (set && enumsFound[group])
          SDDS_Bomb("enumerated and non-enumerated values requested for same scales in same plane");
      }
        
      /* Analyze the enumerated data for each group.  Assign common indices
       * to common string values 
       */
      for (group=0; group<plspec->scalesGroups[plane]; group++) {
        if (!enumsFound[group])
          continue;
        if (!(enumData=SDDS_Malloc(sizeof(*enumData)*enumPoints[group])))
          SDDS_Bomb("Memory allocation failure (assign_enumerate_values)");
        enums = 0;
        for (set=0; set<datasets; set++) {
          /* For each enumerated data set, copy the enumerated strings, the pointer to the
           * location where the plottable value will be stored, and the original index
           * in the dataset.
           */
          if (dataset[set]->scalesGroupIndex[plane]!=group)
            continue;
          if (!dataset[set]->enumerate)
            continue;
          if ((plane && dataset[set]->enumPlane==0) || (!plane && dataset[set]->enumPlane==1))
            continue;
          for (point=0; point<dataset[set]->points; point++) {
            enumData[enums+point].string = dataset[set]->enumerate[point];
            enumData[enums+point].indexData = plane?dataset[set]->y+point:dataset[set]->x+point;
            enumData[enums+point].originalIndex = point;
          }
          enums += dataset[set]->points;
        }
        /* Sort the list of enumerated values by the strings, sub-sorting by original index */
        qsort((void*)enumData, enums, sizeof(*enumData), compareEnumSortData);
        /* Reassign the "originalIndex" value for each point to be the lowest originalIndex of the 
         * points with the identical string
         */
        for (iEnum=1; iEnum<enums; iEnum++) {
          if (strcmp(enumData[iEnum-1].string, enumData[iEnum].string)==0)
            enumData[iEnum].originalIndex = enumData[iEnum-1].originalIndex;
        }
        /* Sort by the modified index, which means we are restoring the original ordering of the
         * strings.
         */
        qsort((void*)enumData, enums, sizeof(*enumData), compareEnumSortData2);
        /* Assign successive integer values to blocks with the same "originalIndex" */
        dataValue = 0;
        for (iEnum=0; iEnum<enums; iEnum++) {
          if (iEnum && enumData[iEnum-1].originalIndex!=enumData[iEnum].originalIndex)
            dataValue++;
          *(enumData[iEnum].indexData) = dataValue;
        }
        free(enumData);
      }
    }
  }
  free(datasetsFound);
  free(enumPoints);
  free(enumsFound);
}



void perform_dataset_conversions(PLOT_SPEC *plspec)
{
  long set, request, mode, setnumber, i, last_panel=0, last_datapage=0, last_file=0;
  PLOT_DATA *dataset;
  PLOT_REQUEST *plreq;
  double factor, center, min, max;
  long pointsLeft;
  double *xmin, *xmax, *ymin, *ymax;

  for (set=0; set<plspec->datasets; set++) {
    dataset = plspec->dataset+set;
    request = dataset->request_index;
    plreq = plspec->plot_request+request;
    plreq->xorig = INT_MAX;
    plreq->yorig = INT_MAX;
  }

  xmin=tmalloc(plspec->panels*sizeof(double));
  xmax=tmalloc(plspec->panels*sizeof(double));
  ymin=tmalloc(plspec->panels*sizeof(double));
  ymax=tmalloc(plspec->panels*sizeof(double));
  for (i=0; i<plspec->panels; i++) {
    xmin[i] = ymin[i] = DBL_MAX;
    xmax[i] = ymax[i] = -DBL_MAX;
  }
  for (set=0; set<plspec->datasets; set++) {
    dataset = plspec->dataset+set;
    if (dataset->replicant)
      continue;
    request = dataset->request_index;
    if ((mode=plspec->plot_request[request].mode)) {
      if (mode&MODE_X_ABSOLUTE) {
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->x[i] = fabs(dataset->x[i]);
      }
      if (mode&MODE_Y_ABSOLUTE) {
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->y[i] = fabs(dataset->y[i]);
      }
      if (mode&MODE_X_AUTOLOG) {
        find_min_max(&min, &max, dataset->x, dataset->pointsStored);
        if (min<xmin[dataset->plotpanel])
          xmin[dataset->plotpanel] = min;
        if (max>xmax[dataset->plotpanel])
          xmax[dataset->plotpanel] = max;
      }
      if (mode&MODE_Y_AUTOLOG) {
        find_min_max(&min, &max, dataset->y, dataset->pointsStored);
        if (min<ymin[dataset->plotpanel])
          ymin[dataset->plotpanel] = min;
        if (max>ymax[dataset->plotpanel])
          ymax[dataset->plotpanel] = max;
      }
    }
  }
  for (set=0; set<plspec->datasets; set++) {
    dataset = plspec->dataset+set;
    if (dataset->replicant)
      continue;
    request = dataset->request_index;
    plreq = plspec->plot_request+request;

    if (plspec->plot_request[request].offset_flags&OFFSET_XBEFORELOG_GIVEN)
      applyOffsets(plspec, request, dataset, 0);
    if (plspec->plot_request[request].offset_flags&OFFSET_YBEFORELOG_GIVEN)
      applyOffsets(plspec, request, dataset, 1);
    if (plspec->plot_request[request].factor_flags&FACTOR_XBEFORELOG_GIVEN)
      applyFactors(plspec, request, dataset, 0);
    if (plspec->plot_request[request].factor_flags&FACTOR_YBEFORELOG_GIVEN)
      applyFactors(plspec, request, dataset, 1);

    if ((mode=plspec->plot_request[request].mode)) {
      if (mode&MODE_X_LOG || 
          (mode&MODE_X_AUTOLOG && 
           ((xmin[dataset->plotpanel]>0 && xmax[dataset->plotpanel]/xmin[dataset->plotpanel]>15)  ||
            (xmax[dataset->plotpanel]>0 && xmin[dataset->plotpanel]==0)))) {
        if (mode&MODE_X_AUTOLOG)
          plspec->scalesGroupData[0][dataset->scalesGroupIndex[0]].mode |= MODE_X_LOG|MODE_X_SPECIALSCALES;
        pointsLeft = remove_nonpositive_data(dataset->x, dataset->y, dataset->x1, dataset->y1,
                                             dataset->split_data, dataset->enumerate, 
                                             dataset->pointLabel, dataset->pointsStored);
        if (pointsLeft!=dataset->pointsStored && dataset->scrollParent) 
          bomb("using log mode for scrolling is not possible when data has non-positive values", NULL);
        dataset->pointsStored = pointsLeft;
        if (!dataset->scrollParent)
          dataset->points = pointsLeft;
        for (i=0; i<dataset->pointsStored; i++) {
          if (dataset->x1)
            dataset->x1[i] /= dataset->x[i];
          dataset->x[i] = log10(dataset->x[i]);
        }
        if (plreq->limit.xmin>0)
          plreq->limit.cxmin = log10(plreq->limit.cxmin);
        else
          plreq->limit.cxmin = -DBL_MAX/3;
        if (plreq->limit.xmax>0)
          plreq->limit.cxmax = log10(plreq->limit.cxmax);
        else
          plreq->limit.cxmax = DBL_MAX/3;
      }
      if (mode&MODE_Y_LOG ||
          (mode&MODE_Y_AUTOLOG && 
           ((ymin[dataset->plotpanel]>0 && ymax[dataset->plotpanel]/ymin[dataset->plotpanel]>15)  ||
            (ymax[dataset->plotpanel]>0 && ymin[dataset->plotpanel]==0)))) {
        if (mode&MODE_Y_AUTOLOG)
          plspec->scalesGroupData[1][dataset->scalesGroupIndex[1]].mode |= MODE_Y_LOG|MODE_Y_SPECIALSCALES;
        pointsLeft = remove_nonpositive_data(dataset->y, dataset->x, dataset->x1, dataset->y1, 
                                                  dataset->split_data, dataset->enumerate, 
                                                  dataset->pointLabel, dataset->pointsStored);
	if (pointsLeft==0 && dataset->pointsStored) 
	  bomb("No data left after logarithm because all y data are non-positive values", NULL);
	if (pointsLeft!=dataset->pointsStored) 
	  fprintf(stdout, "Warning: some points are removed after logarithm because of non-positive y values\n");
        if (pointsLeft!=dataset->pointsStored && dataset->scrollParent) 
          bomb("using log mode for scrolling is not possible when data has non-positive values", NULL);
        dataset->pointsStored = pointsLeft;
        if (!dataset->scrollParent)
          dataset->points = pointsLeft;
        for (i=0; i<dataset->pointsStored; i++) {
          if (dataset->y1)
            dataset->y1[i] /= dataset->y[i];
          dataset->y[i] = log10(dataset->y[i]);
        }
        if (plreq->limit.ymin>0)
          plreq->limit.cymin = log10(plreq->limit.cymin);
        else
          plreq->limit.cymin = -DBL_MAX/3;
        if (plreq->limit.ymax>0)
          plreq->limit.cymax = log10(plreq->limit.cymax);
        else
          plreq->limit.cymax = DBL_MAX/3;
      }
    }
  }

  free(xmin);
  free(xmax);
  free(ymin);
  free(ymax);

  for (set=0; set<plspec->datasets; set++) {
    long xmode, ymode, xRequestMode;
    dataset = plspec->dataset+set;
    if (dataset->replicant)
      continue;
    request = dataset->request_index;
    plreq = plspec->plot_request+request;
    plreq->limit.cxmin = plreq->limit.xmin;
    plreq->limit.cxmax = plreq->limit.xmax;
    plreq->limit.cymin = plreq->limit.ymin;
    plreq->limit.cymax = plreq->limit.ymax;

    xRequestMode = plreq->mode;
    xmode = plspec->scalesGroupData[0][dataset->scalesGroupIndex[0]].mode;
    if (xmode&~MODE_X_LOG) {
      if (xRequestMode&MODE_X_OFFSET) { 
        /* Using xRequestMode now instead of xmode. I may have to change this for the other mode variables as well */
        plreq->limit.cxmin -= dataset->xOrig0;
        plreq->limit.cxmax -= dataset->xOrig0;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->x[i] -= dataset->xOrig0;
      }
      if (xmode&MODE_X_COFFSET) {
        if (plreq->xorig == INT_MAX)
          plreq->xorig = dataset->xOrig0;
        plreq->limit.cxmin -= plreq->xorig;
        plreq->limit.cxmax -= plreq->xorig;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->x[i] -= plreq->xorig;
      }
      if (xmode&MODE_X_EOFFSET) {
        plreq->limit.cxmin -= dataset->xOrig1;
        plreq->limit.cxmax -= dataset->xOrig1;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->x[i] -= dataset->xOrig1;
      }
      if (xmode&MODE_Y_EOFFSET) {
        plreq->limit.cymin -= dataset->yOrig1;
        plreq->limit.cymax -= dataset->yOrig1;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->y[i] -= dataset->yOrig1;
      }
      if (xmode&MODE_X_CENTER || xmode&MODE_X_MEANCENTER) {
        if (xmode&MODE_X_CENTER) {
          find_min_max(&min, &max, dataset->x, dataset->pointsStored);
          center = (max+min)/2;
        } else
          center = arithmeticAverage(dataset->x, dataset->pointsStored);
        plreq->limit.cxmin -= center;
        plreq->limit.cxmax -= center;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->x[i] -= center;
      }
      if (xmode&MODE_X_NORMALIZE) {
        if ((factor = perform_normalization(dataset->x, dataset->x1, dataset->pointsStored))) {
          plreq->limit.cxmax /= factor;
          plreq->limit.cxmin /= factor;
          if (plreq->limit.cxmax<plreq->limit.cxmin)
            SWAP_DOUBLE(plreq->limit.cxmax, plreq->limit.cxmin);
        }
      }
      if (xmode&MODE_X_FRACDEV) {
        center = arithmeticAverage(dataset->x, dataset->pointsStored);
        plreq->limit.cxmin = (plreq->limit.cxmin-center)/center;
        plreq->limit.cxmax = (plreq->limit.cxmax-center)/center;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->x[i] = (dataset->x[i] - center)/center;
      }
    }
    ymode = plspec->scalesGroupData[1][dataset->scalesGroupIndex[1]].mode;
    if (ymode&~MODE_Y_LOG) {
      if (ymode&MODE_Y_NORMALIZE) {
        if ((factor=perform_normalization(dataset->y, dataset->y1, dataset->pointsStored))) {
          plreq->limit.cymax /= factor;
          plreq->limit.cymin /= factor;
          if (plreq->limit.cymax<plreq->limit.cymin)
            SWAP_DOUBLE(plreq->limit.cymax, plreq->limit.cymin);
        }
      }
      if (ymode&MODE_Y_OFFSET) {
        plreq->limit.cymin -= dataset->yOrig0;
        plreq->limit.cymax -= dataset->yOrig0;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->y[i] -= dataset->yOrig0;
      }
      if (ymode&MODE_Y_COFFSET) {
        if (plreq->yorig == INT_MAX)
          plreq->yorig = dataset->yOrig0;
        plreq->limit.cymin -= plreq->yorig;
        plreq->limit.cymax -= plreq->yorig;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->y[i] -= plreq->yorig;
      }
      if (ymode&MODE_Y_CENTER || ymode&MODE_Y_MEANCENTER) {
        if (ymode&MODE_Y_CENTER) {
          find_min_max(&min, &max, dataset->y, dataset->pointsStored);
          center = (max+min)/2;
        } else
          center = arithmeticAverage(dataset->y, dataset->pointsStored);
        plreq->limit.cymin -= center;
        plreq->limit.cymax -= center;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->y[i] -= center;
      }
      if (ymode&MODE_Y_FRACDEV) {
        center = arithmeticAverage(dataset->y, dataset->pointsStored);
        plreq->limit.cymin = (plreq->limit.cymin-center)/center;
        plreq->limit.cymax = (plreq->limit.cymax-center)/center;
        for (i=dataset->pointsStored-1; i>=0; i--)
          dataset->y[i] = (dataset->y[i] - center)/center;
      }
    }
    if (!(plspec->plot_request[request].offset_flags&OFFSET_XBEFORELOG_GIVEN))
      applyOffsets(plspec, request, dataset, 0);
    if (!(plspec->plot_request[request].offset_flags&OFFSET_YBEFORELOG_GIVEN))
      applyOffsets(plspec, request, dataset, 1);
    if (!(plspec->plot_request[request].factor_flags&FACTOR_XBEFORELOG_GIVEN))
      applyFactors(plspec, request, dataset, 0);
    if (!(plspec->plot_request[request].factor_flags&FACTOR_YBEFORELOG_GIVEN))
      applyFactors(plspec, request, dataset, 1);
    if (plreq->dither[0]) {
      double range, min, max;
      find_min_max(&min, &max, dataset->x, dataset->pointsStored);
      range = (max-min)*plreq->dither[0];
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->x[i] += range*(drand(1)-0.5);
    }
    if (plspec->plot_request[request].dither[1]) {
      double range, min, max;
      find_min_max(&min, &max, dataset->y, dataset->pointsStored);
      range = (max-min)*plreq->dither[1];
      for (i=dataset->pointsStored-1; i>=0; i--)
        dataset->y[i] += range*(drand(1)-0.5);
    }
  }

  for (request=0; request<plspec->plot_requests; request++) {
    if (plspec->plot_request[request].stagger_flags&STAGGER_XINC_GIVEN ||
        plspec->plot_request[request].stagger_flags&STAGGER_YINC_GIVEN) {
      if (plspec->plot_request[request].stagger_flags&STAGGER_FILES) {
        setnumber = 0;
        for (set=1; set<plspec->datasets; set++) {
          /* apply only global stagger value, or those for the request that the data came from */
          if (plspec->dataset[set].request_index!=request)
            continue;
          if (plspec->dataset[set].file_index!=plspec->dataset[set-1].file_index)
            setnumber++;
          if (setnumber) {
            dataset = plspec->dataset+set;
            for (i=0; i<dataset->pointsStored; i++) {
              dataset->x[i] += plspec->plot_request[request].stagger_amount[0]*setnumber;
              dataset->y[i] += plspec->plot_request[request].stagger_amount[1]*setnumber;
            }
          }
        }
      }
      if (plspec->plot_request[request].stagger_flags&STAGGER_DATANAMES) {
        setnumber = 0;
        for (set=1; set<plspec->datasets; set++) {
          if (!(plspec->plot_request[plspec->dataset[set].request_index].stagger_flags&STAGGER_DATANAMES))
            continue;
          if (plspec->dataset[set].file_index==plspec->dataset[set-1].file_index &&
              plspec->dataset[set].plotpanel==plspec->dataset[set-1].plotpanel)
            setnumber++;
          else
            setnumber = 0;
          if (setnumber) {
            dataset = plspec->dataset+set;
            for (i=0; i<dataset->pointsStored; i++) {
              dataset->x[i] += plspec->plot_request[request].stagger_amount[0]*setnumber;
              dataset->y[i] += plspec->plot_request[request].stagger_amount[1]*setnumber;
            }
          }
        }
      }
      if (!(plspec->plot_request[request].stagger_flags&STAGGER_FILES) &&
          !(plspec->plot_request[request].stagger_flags&STAGGER_DATANAMES)) {
        setnumber = 0;
        for (set=0; set<plspec->datasets; set++) {
          if (plspec->dataset[set].request_index!=request)
            continue;
#if defined(DEBUG)
          fprintf(stderr, "staggering set %ld:  panel=%ld, file=%ld, datapage=%ld\n",
                  set, plspec->dataset[set].plotpanel, plspec->dataset[set].file_index,
                  plspec->dataset[set].datapage);
          fprintf(stderr, "  last:  panel=%ld, file=%ld, datapage=%ld\n",
                  last_panel, last_file, last_datapage);
#endif
          dataset = plspec->dataset+set;
          if (setnumber==0 || dataset->plotpanel!=last_panel) {
            last_panel = dataset->plotpanel;
            last_file = dataset->file_index;
            last_datapage = dataset->datapage;
            setnumber = 1;
            continue;
          }
          if (dataset->file_index==last_file && dataset->datapage==last_datapage &&
              !(plspec->plot_request[request].split.flags&SPLIT_COMPLETELY ||
                plspec->plot_request[request].flags&PLREQ_TRANSPOSE))
            setnumber--;
          last_panel = dataset->plotpanel;
          last_file = dataset->file_index;
          last_datapage = dataset->datapage;
          if (setnumber==0) {
            setnumber = 1;
            continue;
          }
          for (i=0; i<dataset->pointsStored; i++) {
            dataset->x[i] += plspec->plot_request[request].stagger_amount[0]*setnumber;
            dataset->y[i] += plspec->plot_request[request].stagger_amount[1]*setnumber;
          }
#if defined(DEBUG)
          fprintf(stderr, "stagger set number %ld done\n", setnumber);
#endif
          setnumber++;
        }
      }
    }
  }
}

void perform_dataset_transpositions(PLOT_SPEC *plspec)
{
  long ireq, points, datapages, i, j, inew, ifile;
  long i1fset, i2fset, i1rset, i2rset, iset, datasets;
  PLOT_REQUEST *plreq;
  PLOT_DATA *new_dataset, *new_dataset1, *dataset;
  static char buffer[SDDS_MAXLINE];

  for (ireq=0; ireq<plspec->plot_requests; ireq++) {
    plreq = plspec->plot_request + ireq;
    if (!(plreq->flags&PLREQ_TRANSPOSE)) 
      continue;
    for (i1rset=0; i1rset<plspec->datasets; i1rset++)
      if (plspec->dataset[i1rset].request_index==ireq)
        break;
    for (i2rset=i1rset+1; i2rset<plspec->datasets; i2rset++)
      if (plspec->dataset[i2rset].request_index!=ireq)
        break;
    for (ifile=0; ifile<plreq->filenames; ifile++) {
      for (i1fset=i1rset; i1fset<i2rset; i1fset++)
        if (plspec->dataset[i1fset].file_index==ifile)
          break;
      for (i2fset=i1fset+1; i2fset<i2rset; i2fset++)
        if (plspec->dataset[i2fset].file_index!=ifile)
          break;
      points = plspec->dataset[i1fset].points;
      datasets = i2fset-i1fset;
#if defined(DEBUG)
      fprintf(stderr, "%ld data sets (%ld->%ld) belong to request %ld, file %ld\n",
              datasets, i1fset, i2fset-1, ireq, ifile);
#endif
      datapages = datasets/plreq->datanames;
#if defined(DEBUG)
      fprintf(stderr, "%ld data pages inferred for %ld data names\n", datapages, plreq->datanames);
#endif
      if (plreq->datanames*datapages!=datasets)
        bomb("problem doing transposition--counting mismatch\n", NULL);
      new_dataset = tmalloc(sizeof(*new_dataset)*datapages*points);
      inew = 0;
      for (iset=i1fset; iset<i2fset; iset+=plreq->datanames, inew++) {
        dataset = plspec->dataset+iset;
        for (i=0; i<points; i++) {
          new_dataset1 = new_dataset+inew*points+i;
          *new_dataset1 = dataset[0];
          SDDS_CopyString(&new_dataset1->info[0].symbol, "Index");
          new_dataset1->info[0].description = new_dataset1->info[0].units = NULL;
          new_dataset1->x = tmalloc(sizeof(*new_dataset1->x)*plreq->datanames);
          new_dataset1->y = tmalloc(sizeof(*new_dataset1->y)*plreq->datanames);
          new_dataset1->points = plreq->datanames;
          plreq = plspec->plot_request + dataset[0].request_index;
          if (!plreq->label[3].flags && !plreq->label[3].label) {
            sprintf(buffer, "%s: %g %s",
                    dataset->info[0].symbol, dataset->x[i], 
                    dataset->info[0].units?dataset->info[0].units:"");
            SDDS_CopyString(new_dataset1->label+3, buffer);
          }
          if (!plreq->label[1].flags && !plreq->label[1].label) {
            sprintf(buffer, "%s", 
                    dataset->info[1].units?dataset->info[1].units:" ");
            SDDS_CopyString(new_dataset1->label+1, buffer);
          }
          for (j=0; j<plreq->datanames; j++) {
            new_dataset1->x[j] = j;
            new_dataset1->y[j] = dataset[j].y[i];
          }
          new_dataset1->subpage = i;
        }
      }
      j = datapages*points-(i2fset-i1fset);
      if (j>0) 
        plspec->dataset = trealloc(plspec->dataset, sizeof(*plspec->dataset)*(plspec->datasets+j));
#if defined(DEBUG)
      fprintf(stderr, "moving %ld data sets from slots %ld->%ld to %ld->%ld\n",
              plspec->datasets-i2fset, i2fset, plspec->datasets-1,
              i2fset+j, plspec->datasets-1+j);
#endif
      for (iset=plspec->datasets-1; iset>=i2fset; iset--)
        plspec->dataset[iset+j] = plspec->dataset[iset];
      if (j<0)
        plspec->dataset = trealloc(plspec->dataset, sizeof(*plspec->dataset)*(plspec->datasets+j));
      plspec->datasets = plspec->datasets + j;
      i1rset += j;
      i2rset += j;
#if defined(DEBUG)
      fprintf(stderr, "copying %ld data sets into slots %ld to %ld\n",
              datapages*points, i1fset, i1fset+datapages*points-1);
#endif
      for (iset=i1fset; iset<i1fset+datapages*points; iset++)
        plspec->dataset[iset] = new_dataset[iset-i1fset];
    }
  }
}

long remove_nonpositive_data(double *data1, double *data2, double *data3, double *data4, double *data5, 
                             char **enumerate, char **pointLabel, long n)
{
  long i, j;
  static char *keep = NULL;
  keep = trealloc(keep, sizeof(*keep)*n);
  for (i=0; i<n; i++) {
    if (data1[i]<=0)
      keep[i] = 0;
    else
      keep[i] = 1;
  }
  for (i=j=0; i<n; i++) {
    if (keep[i]) {
      if (i!=j) {
        data1[j] = data1[i];
        data2[j] = data2[i];
        if (data3)
          data3[j] = data3[i];
        if (data4)
          data4[j] = data4[i];
        if (data5)
          data5[j] = data5[i];
        if (enumerate)
          enumerate[j] = enumerate[i];
        if (pointLabel)
          pointLabel[j] = pointLabel[i];
      }
      j++;
    }
  }
  return j;
}

long remove_invalid_data(double *data1, double *data2, double *data3, double *data4, double *data5, 
                         char **enumerate, char **pointLabel, long n)
{
  long i, j;
  static char *keep = NULL;
  keep = trealloc(keep, sizeof(*keep)*n);
  for (i=0; i<n; i++) {
    if (isnan(data1[i]) || isinf(data1[i]) ||
        isnan(data2[i]) || isinf(data2[i]) ||
        (data3 && (isnan(data3[i]) || isinf(data3[i]))) ||
        (data4 && (isnan(data4[i]) || isinf(data4[i]))) )
      keep[i] = 0;
    else
      keep[i] = 1;
  }
  for (i=j=0; i<n; i++) {
    if (keep[i]) {
      if (i!=j) {
        data1[j] = data1[i];
        data2[j] = data2[i];
        if (data3)
          data3[j] = data3[i];
        if (data4)
          data4[j] = data4[i];
        if (data5)
          data5[j] = data5[i];
        if (enumerate)
          enumerate[j] = enumerate[i];
        if (pointLabel)
          pointLabel[j] = pointLabel[i];
      }
      j++;
    }
  }
  return j;
}

void columnbin_sddsplot_data(PLOT_SPEC *plspec)
{
  long iset, jset, i, j, bins, *keep, keepers_max, keepers;
  PLOT_DATA dataset;
  PLOT_REQUEST *plreq;
  double min, max, start, end, width, xOrig0, yOrig0, xOrig1, yOrig1;
  
  for (iset=0; iset<plspec->datasets; iset++) {
    dataset = plspec->dataset[iset];
    plreq = plspec->plot_request+dataset.request_index;
    if (plreq->split.flags&SPLIT_COLUMNBIN || plreq->split.flags&SPLIT_PARAMETERCHANGE) {
      if (!(plreq->split.flags&SPLIT_CHANGE_START)) {
        if (plreq->split.width>=0) {
          plreq->split.start = DBL_MAX;
          plreq->split.end = -DBL_MAX;
        } else {
          plreq->split.start = -DBL_MAX;
          plreq->split.end = DBL_MAX;
        }
      }
    }
  }
  for (iset=0; iset<plspec->datasets; iset++) {
    dataset = plspec->dataset[iset];
    plreq = plspec->plot_request+dataset.request_index;
    if (plreq->split.flags&SPLIT_COLUMNBIN || plreq->split.flags&SPLIT_PARAMETERCHANGE) {
      min = max = 0;
      find_min_max(&min, &max, dataset.split_data, dataset.points);
      if (plreq->split.width>=0) {
        if (!(plreq->split.flags&SPLIT_CHANGE_START)) {
          if (min < plreq->split.start) {
            plreq->split.start = min;
          }
        }
        if (max > plreq->split.end) {
          plreq->split.end = max;
        }
      } else {
        if (!(plreq->split.flags&SPLIT_CHANGE_START)) {
          if (max > plreq->split.start) {
            plreq->split.start = max;
          }
        }
        if (min < plreq->split.end) {
          plreq->split.end = min;
        }
      }
    }
  }
  for (iset=0; iset<plspec->datasets; iset++) {
    dataset = plspec->dataset[iset];
    plreq = plspec->plot_request+dataset.request_index;
    if (plreq->split.flags&SPLIT_COLUMNBIN && (plreq->split.width == 0)) {
      plreq->split.width = (plreq->split.end - plreq->split.start) / 100.0;
    }
  }
  keep = NULL;
  keepers_max = 0;
  for (iset=0; iset<plspec->datasets; iset++) {
    dataset = plspec->dataset[iset];
    plspec->dataset[iset].xOrig0 = xOrig0 = (dataset.points?dataset.x[0]:0);
    plspec->dataset[iset].yOrig0 = yOrig0 = (dataset.points?dataset.y[0]:0);
    plspec->dataset[iset].xOrig1 = xOrig1 = (dataset.points?dataset.x[dataset.points-1]:0);
    plspec->dataset[iset].yOrig1 = yOrig1 = (dataset.points?dataset.y[dataset.points-1]:0);
    plreq = plspec->plot_request+dataset.request_index;
    if (plreq->split.flags&SPLIT_COLUMNBIN || plreq->split.flags&SPLIT_PARAMETERCHANGE) {
      min = max = 0;
      find_min_max(&min, &max, dataset.split_data, dataset.points);
#if defined(DEBUG)
      fprintf(stderr, "column-binning dataset %ld by column %s.  %ld data points on [%e, %e]\n",
              iset, plreq->split.name, dataset.points, min, max);
#endif
      if (plreq->split.width>0) {
        min = start = plreq->split.start;
      } else {
        max = start = plreq->split.start;
      }
      /*
      if (plreq->split.width>0) {
        if (plreq->split.flags&SPLIT_CHANGE_START)
          min = start = plreq->split.start;
        else 
          start = min;
      } else {
        if (plreq->split.flags&SPLIT_CHANGE_START)
          max = start = plreq->split.start;
        else 
          start = max;
      }
      */
      if ((bins = fabs((max-min)/(width=plreq->split.width)) + 1.0)>1) {
        max = min+bins*width;
#if defined(DEBUG)
        fprintf(stderr, "%ld bins will be used\n", bins);
#endif
        if (keepers_max<dataset.points)
          keep = trealloc(keep, sizeof(*keep)*(keepers_max = dataset.points));
        plspec->dataset = trealloc(plspec->dataset, sizeof(*plspec->dataset)*(plspec->datasets+bins));
        for (jset=plspec->datasets-1; jset>=iset+1; jset--) {
#if defined(DEBUG) 
          fprintf(stderr, "copying from slot %ld to %ld\n", jset, jset+bins-1);
#endif
          plspec->dataset[jset+bins-1] = plspec->dataset[jset];
        }
        for (jset=iset; jset<iset+bins; jset++) {
#if defined(DEBUG)
          fprintf(stderr, "working on bin %ld\n", jset-iset);
#endif
          end = start+width;
          plspec->dataset[jset] = dataset;
          plspec->dataset[jset].split_data = NULL;
          plspec->dataset[jset].subpage = jset-iset;
	  
          for (i=keepers=0; i<dataset.points; i++) {
            if ((width>0 && (dataset.split_data[i]>=end || dataset.split_data[i]<start)) ||
                (width<0 && (dataset.split_data[i]>=start || dataset.split_data[i]<end)) )
              keep[i] = 0;
            else {
              keep[i] = 1;
              keepers++;
            }
          }
          plspec->dataset[jset].x = tmalloc(sizeof(*plspec->dataset[jset].x)*keepers);
          plspec->dataset[jset].y = tmalloc(sizeof(*plspec->dataset[jset].y)*keepers);
          if (dataset.x1)
            plspec->dataset[jset].x1 = tmalloc(sizeof(*plspec->dataset[jset].x1)*keepers);
          if (dataset.y1)
            plspec->dataset[jset].y1 = tmalloc(sizeof(*plspec->dataset[jset].y1)*keepers);
          if (dataset.enumerate)
            plspec->dataset[jset].enumerate = tmalloc(sizeof(*plspec->dataset[jset].enumerate)*keepers);
          if (dataset.pointLabel)
            plspec->dataset[jset].pointLabel = tmalloc(sizeof(*plspec->dataset[jset].pointLabel)*keepers);
          for (i=j=0; i<dataset.points; i++) {
            if (keep[i]) {
              plspec->dataset[jset].x[j] = dataset.x[i];
              plspec->dataset[jset].y[j] = dataset.y[i];
              if (dataset.x1)
                plspec->dataset[jset].x1[j] = dataset.x1[i];
              if (dataset.y1)
                plspec->dataset[jset].y1[j] = dataset.y1[i];
              if (dataset.enumerate)
                plspec->dataset[jset].enumerate[j] = dataset.enumerate[i];
              if (dataset.pointLabel)
                plspec->dataset[jset].pointLabel[j] = dataset.pointLabel[i];
              j++;
            }
          }
          plspec->dataset[jset].xOrig0 = xOrig0;
          plspec->dataset[jset].yOrig0 = yOrig0;
          plspec->dataset[jset].xOrig1 = xOrig1;
          plspec->dataset[jset].yOrig1 = yOrig1;
          if (plreq->split.flags&SPLIT_COMPLETELY && j) {
            plspec->dataset[jset].xOrig0 = plspec->dataset[jset].x[0];
            plspec->dataset[jset].yOrig0 = plspec->dataset[jset].y[0];
            plspec->dataset[jset].xOrig1 = plspec->dataset[jset].x[keepers-1];
            plspec->dataset[jset].yOrig1 = plspec->dataset[jset].y[keepers-1];
          }
          plspec->dataset[jset].points = keepers;
          start = end;
        }
        iset += bins-1;
        plspec->datasets += bins-1;
      }
    }
  }
}

void perform_arrow_autoscaling(PLOT_SPEC *plspec)
/* also checks for proper data for arrow plotting */
{
  long irequest, ifile, iset, ipoint, iname; 
  PLOT_REQUEST *request;
  double dxAve = 0, dyAve = 0, dx, dy, maxLength = 0, length, factor;
  long dxAvePoints = 0, dyAvePoints = 0;
  PLOT_DATA *dataset;
  
  for (irequest=1; irequest<plspec->plot_requests; irequest++) {
    request = plspec->plot_request+irequest;
    if (request->graphic.element!=PLOT_ARROW)
      continue;
    for (ifile=0; ifile<request->filenames; ifile++) {
      dxAve = dyAve = maxLength = 0;
      dxAvePoints = dyAvePoints = 0;
      for (iname=0; iname<request->datanames; iname++) {
        for (iset=0; iset<plspec->datasets; iset++) {
          if (plspec->dataset[iset].request_index!=irequest || plspec->dataset[iset].file_index!=ifile
              || plspec->dataset[iset].dataname_index!=iname) 
            continue;
          dataset = plspec->dataset+iset;
          if (!dataset->y1 || (!(request->graphic.arrow.flags&ARROW_SCALAR_DATA) && !dataset->x1))
            bomb("arrow plotting requested with insufficient data specified", NULL);
          if (request->graphic.arrow.flags&ARROW_ENDPOINTS) {
            /* convert x1 and y1 to change range than endpoint values */
            for (ipoint=0; ipoint<dataset->points; ipoint++) {
              dataset->x1[ipoint] -= dataset->x[ipoint];
              dataset->y1[ipoint] -= dataset->y[ipoint];
            }
            continue;
          }
          if (!(request->graphic.arrow.flags&ARROW_AUTOSCALE))
            continue;
          for (ipoint=0; ipoint<dataset->points; ipoint++) {
            if (ipoint) {
              dx = fabs(dataset->x[ipoint] - dataset->x[ipoint-1]);
              dy = fabs(dataset->y[ipoint] - dataset->y[ipoint-1]);
              if (dx) {
                dxAve += dx;
                dxAvePoints ++;
              }
              if (dy) {
                dyAve += dy;
                dyAvePoints ++;
              }
            }
            if (request->graphic.arrow.flags&ARROW_SCALAR_DATA)
              length = fabs(dataset->y1[ipoint]);
            else if (request->graphic.arrow.flags&ARROW_POLAR_DATA)
              length = fabs(dataset->x1[ipoint]);
            else
              length = sqrt(sqr(dataset->x1[ipoint])+sqr(dataset->y1[ipoint]));
            if (length > maxLength)
              maxLength = length;
          }
        }
      }
    }
    if (!maxLength)
      continue;
    if (dxAvePoints)
      dxAve /= dxAvePoints;
    if (dyAvePoints)
      dyAve /= dyAvePoints;
    if (dxAve && dyAve)
      dxAve = dxAve<dyAve ? dxAve : dyAve;
    else if (dyAve)
      dxAve = dyAve;
    factor = dxAve/maxLength;
    for (ifile=0; ifile<request->filenames; ifile++) {
      for (iname=0; iname<request->datanames; iname++) {
        for (iset=0; iset<plspec->datasets; iset++) {
          if (plspec->dataset[iset].request_index!=irequest || plspec->dataset[iset].file_index!=ifile
              || plspec->dataset[iset].dataname_index!=iname) 
            continue;
          dataset = plspec->dataset+iset;
          for (ipoint=0; ipoint<dataset->points; ipoint++) {
            if (request->graphic.arrow.flags&ARROW_SCALAR_DATA)
              dataset->y1[ipoint] *= factor;
            else if (request->graphic.arrow.flags&ARROW_POLAR_DATA)
              dataset->x1[ipoint] *= factor;
            else {
              dataset->x1[ipoint] *= factor;
              dataset->y1[ipoint] *= factor;
            }
          }
        }
      }
    }
  }
}

void replicate_datasets(PLOT_SPEC *plspec) 
{
  long idataset, irequest, j;
  long maxPages, maxNames, maxFiles, replications, scrollDelta;
  PLOT_REQUEST *request;
  
  for (idataset=maxPages=0; idataset<plspec->datasets; idataset++) {
    if (plspec->dataset[idataset].datapage>maxPages)
      maxPages = plspec->dataset[idataset].datapage;
  }

  for (irequest=maxNames=maxFiles=1; irequest<plspec->plot_requests; irequest++) {
    if (plspec->plot_request[irequest].datanames>maxNames)
      maxNames = plspec->plot_request[irequest].datanames;
    if (plspec->plot_request[irequest].filenames>maxFiles)
      maxFiles = plspec->plot_request[irequest].filenames;
  }

#if defined(DEBUG)
  fprintf(stderr, "%ld datasets before replication\n", plspec->datasets);
  for (j=0; j<plspec->datasets; j++) {
    fprintf(stderr, "dataset %ld:  replicant = %hd\n", j, plspec->dataset[j].replicant);
    fprintf(stderr, "  real: request = %ld, file = %ld, dataname = %ld\n",
            plspec->dataset[j].request_index, plspec->dataset[j].file_index,
            plspec->dataset[j].dataname_index);
    fprintf(stderr, "  virt: request = %ld, file = %ld, dataname = %ld\n",
            plspec->dataset[j].virtual_request_index, plspec->dataset[j].virtual_file_index,
            plspec->dataset[j].virtual_dataname_index);
    fprintf(stderr, "  datapage = %ld\n", plspec->dataset[j].datapage);
  }
#endif
  for (idataset=0; idataset<plspec->datasets; idataset++) {
    plspec->dataset[idataset].scrollParent = plspec->dataset[idataset].replicant = 0;
    plspec->dataset[idataset].pointsStored = plspec->dataset[idataset].points;
    plspec->dataset[idataset].virtual_request_index = plspec->dataset[idataset].request_index;
    plspec->dataset[idataset].virtual_dataname_index = plspec->dataset[idataset].dataname_index;
    plspec->dataset[idataset].virtual_file_index = plspec->dataset[idataset].file_index;
    request = plspec->plot_request + plspec->dataset[idataset].request_index;
    if (!request->replicateSettings.flags) {
#if defined(DEBUG)
      fprintf(stderr, "dataset %ld not being replicated\n", idataset);
#endif
      continue;
    }
#if defined(DEBUG)
    fprintf(stderr, "dataset %ld being replicated\n", idataset);
#endif
    if (request->replicateSettings.flags&REPLICATE_MATCH_NAMES) {
      replications = maxNames-1;
    } else if (request->replicateSettings.flags&REPLICATE_MATCH_PAGES) {
      replications = maxPages-1;
    } else if (request->replicateSettings.flags&REPLICATE_MATCH_REQUESTS) {
      replications = plspec->plot_requests-2;  /* don't count dummy 0th request */
    } else if (request->replicateSettings.flags&REPLICATE_NUMBER) {
      replications = request->replicateSettings.number;
    } else if (request->replicateSettings.flags&REPLICATE_MATCH_FILES) {
      replications = maxFiles - 1;
    } else
      continue;
    if (replications<1)
      continue;
    if (!(plspec->dataset=SDDS_Realloc(plspec->dataset, 
                                       sizeof(*plspec->dataset)*(plspec->datasets+replications))))
      SDDS_Bomb("memory allocation failure");
    /* move existing datasets and make copies of dataset being replicated */
    for (j=plspec->datasets-1; j>idataset; j--) {
      plspec->dataset[j+replications] = plspec->dataset[j];
#if defined(DEBUG)
      fprintf(stderr, "** moved dataset %ld:  replicant = %hd\n", j, plspec->dataset[j].replicant);
      fprintf(stderr, "  real: request = %ld, file = %ld, dataname = %ld\n",
              plspec->dataset[j].request_index, plspec->dataset[j].file_index,
              plspec->dataset[j].dataname_index);
      fprintf(stderr, "  virt: request = %ld, file = %ld, dataname = %ld\n",
              plspec->dataset[j].virtual_request_index, plspec->dataset[j].virtual_file_index,
              plspec->dataset[j].virtual_dataname_index);
      fprintf(stderr, "  datapage = %ld\n", plspec->dataset[j].datapage);
      fprintf(stderr, "to dataset %ld:  replicant = %hd\n", j+replications, plspec->dataset[j+replications].replicant);
      fprintf(stderr, "  real: request = %ld, file = %ld, dataname = %ld\n",
              plspec->dataset[j+replications].request_index, plspec->dataset[j+replications].file_index,
              plspec->dataset[j+replications].dataname_index);
      fprintf(stderr, "  virt: request = %ld, file = %ld, dataname = %ld\n",
              plspec->dataset[j+replications].virtual_request_index, plspec->dataset[j+replications].virtual_file_index,
              plspec->dataset[j+replications].virtual_dataname_index);
      fprintf(stderr, "  datapage = %ld\n", plspec->dataset[j+replications].datapage);
#endif 
    }

    scrollDelta = (plspec->dataset[idataset].points-1)/replications;
    if (scrollDelta<1)
      scrollDelta = 1;
    for (j=1; j<=replications; j++) {
      ReplicateDataset(plspec->dataset+idataset+j, plspec->dataset+idataset, request->replicateSettings.flags&REPLICATE_SCROLL_MODE ? j*scrollDelta+1 : 0);
      plspec->dataset[idataset+j].replicant = 1;
      plspec->dataset[idataset+j].pointsStored = 0;
      plspec->dataset[idataset+j].virtual_dataname_index = plspec->dataset[idataset].dataname_index +
        (request->replicateSettings.flags&(REPLICATE_MATCH_NAMES+REPLICATE_NUMBER) ? j : 0);
      plspec->dataset[idataset+j].datapage = plspec->dataset[idataset].datapage + 
        (request->replicateSettings.flags&REPLICATE_MATCH_PAGES ? j : 0);
      plspec->dataset[idataset+j].virtual_request_index = plspec->dataset[idataset].request_index +
        (request->replicateSettings.flags&REPLICATE_MATCH_REQUESTS ? j : 0);
      plspec->dataset[idataset+j].virtual_file_index = plspec->dataset[idataset].file_index +
        (request->replicateSettings.flags&REPLICATE_MATCH_FILES ? j : 0);
    }
    if (request->replicateSettings.flags&REPLICATE_SCROLL_MODE) {
      plspec->dataset[idataset].scrollParent = 1;
      plspec->dataset[idataset].pointsStored = plspec->dataset[idataset].points;
      plspec->dataset[idataset].points = 1;
    }
    plspec->datasets += replications;
    idataset += replications;
#if defined(DEBUG)
    fprintf(stderr, "%ld datasets after replication\n", plspec->datasets);
    for (j=0; j<plspec->datasets; j++) {
      fprintf(stderr, "dataset %ld:  replicant = %hd\n", j, plspec->dataset[j].replicant);
      fprintf(stderr, "  real: request = %ld, file = %ld, dataname = %ld\n",
              plspec->dataset[j].request_index, plspec->dataset[j].file_index,
              plspec->dataset[j].dataname_index);
      fprintf(stderr, "  virt: request = %ld, file = %ld, dataname = %ld\n",
              plspec->dataset[j].virtual_request_index, plspec->dataset[j].virtual_file_index,
              plspec->dataset[j].virtual_dataname_index);
      fprintf(stderr, "  datapage = %ld\n", plspec->dataset[j].datapage);
    }
#endif
  }
}

void ReplicateDataset(PLOT_DATA *target, PLOT_DATA *source, long offset)
{
  long i, points;

  points = source->points;
  if (offset && (points = offset)>source->points)
    points = source->points;
  
  *target = *source;
  target->points = points;
  
  if (0) {
    if (source->x) {
      if (!(target->x=malloc(sizeof(*target->x)*points)))
        SDDS_Bomb("memory allocation failure (ReplicateDataset)");
      memcpy(target->x, source->x, sizeof(*target->x)*points);
    }
    if (source->y) {
      if (!(target->y=malloc(sizeof(*target->y)*points)))
        SDDS_Bomb("memory allocation failure (ReplicateDataset)");
      memcpy(target->y, source->y, sizeof(*target->y)*points);
    }
    if (source->x1) {
      if (!(target->x1=malloc(sizeof(*target->x1)*points)))
        SDDS_Bomb("memory allocation failure (ReplicateDataset)");
      memcpy(target->x1, source->x1, sizeof(*target->x1)*points);
    }
    if (source->y1) {
      if (!(target->y1=malloc(sizeof(*target->y1)*points)))
        SDDS_Bomb("memory allocation failure (ReplicateDataset)");
      memcpy(target->y1, source->y1, sizeof(*target->y1)*points);
    }
    if (source->split_data) {
      if (!(target->split_data=malloc(sizeof(*target->split_data)*points)))
        SDDS_Bomb("memory allocation failure (ReplicateDataset)");
      memcpy(target->split_data, source->split_data, sizeof(*target->split_data)*points);
    }
    if (source->enumerate) {
      if (!(target->enumerate=malloc(sizeof(*target->enumerate)*points)))
        SDDS_Bomb("memory allocation failure (ReplicateDataset)");
      if (!SDDS_CopyStringArray(target->enumerate, source->enumerate, points))
        SDDS_Bomb("memory allocation failure (ReplicateDataset)");
    }
    if (source->pointLabel) {
      if (!(target->pointLabel=malloc(sizeof(*target->pointLabel)*points)))
        SDDS_Bomb("memory allocation failure (ReplicateDataset)");
      if (!SDDS_CopyStringArray(target->pointLabel, source->pointLabel, points))
        SDDS_Bomb("memory allocation failure (ReplicateDataset)");
    }
  }    

  if (source->legend && !SDDS_CopyString(&target->legend, source->legend))
    SDDS_Bomb("memory allocation failure (ReplicateDataset)");
  for (i=0; i<4; i++)
    if (source->label[i] && !SDDS_CopyString(&target->label[i], source->label[i]))
      SDDS_Bomb("memory allocation failure (ReplicateDataset)");
  
}

