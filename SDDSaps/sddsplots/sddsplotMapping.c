/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sddsplotMapping.c
 * 
 * part of sddsplot (plotting program for SDDS files)
 *
 * Michael Borland, 1994.
 $Log: not supported by cvs2svn $
 Revision 1.34  2005/11/04 22:46:46  soliday
 Updated code to be compiled by a 64 bit processor.

 Revision 1.33  2003/09/02 19:29:29  soliday
 Cleaned up code for Linux.

 Revision 1.32  2003/07/22 21:07:00  soliday
 IEEE functions are always used.

 Revision 1.31  2003/01/08 23:09:40  borland
 Added new argument to binaryIndexSearch().

 Revision 1.30  2002/08/20 18:03:30  soliday
 Fixed a problem with the -thickness option causing odd behaviour

 Revision 1.29  2002/08/14 17:24:55  soliday
 Added Open License

 Revision 1.28  2001/10/12 18:27:44  soliday
 Added the -nextPage option to split requests.

 Revision 1.27  2000/10/11 21:50:41  soliday
 Changed definition of isinf so that sunmath is no longer needed.

 Revision 1.26  2000/08/10 21:17:27  soliday
 Added support for gcc on Solaris.

 Revision 1.25  2000/06/07 20:12:44  soliday
 Added missing sunmath.h include file.

 Revision 1.24  2000/05/02 18:40:38  borland
 Fixed a egregious bug in alignZeros (argument parsing) and added some diagnostics.

 Revision 1.23  2000/03/27 16:39:29  borland
 No longer adjusts plot range for datasets with no data.  Doing so resulting
 in messed up plot scales.

 Revision 1.22  2000/01/10 19:04:00  borland
 Improved function of -alignZero option.

 Revision 1.21  1999/09/18 01:11:50  borland
 Improved recalculate_small_limits.  When the range is zero but the
 average is not, the range is set to 1/5 the average, so one can at
 least see what the value is.

 Revision 1.20  1999/07/22 18:35:54  soliday
 Added WIN32 support

 Revision 1.19  1999/07/21 18:07:26  borland
 Eliminated many memory leaks.  Program is now suitable for running with
 -repeat option, although not all leaks are gone.

 Revision 1.18  1999/06/03 16:09:27  soliday
 Removed compiler warnings under linux.

 Revision 1.17  1999/02/03 19:38:52  borland
 Improved operation of -range option when the center/range is undefined.
 Improved operation of time-scales when the time range is undefined.

 Revision 1.16  1998/12/22 16:02:41  borland
 New character scaling algorithm for all labels.  Considerable changes
 to psymbol routine that draws characters and returns the size of
 strings.

 Revision 1.15  1998/09/02 22:06:52  borland
 Added device argument support for postscript plotting.  Color PS now
 supports 'onBlack' qualifier, which gives a black background.

 Revision 1.14  1998/09/02 14:09:07  borland
 Added -alignZero option.  Also added delay qualifier to -zoom option.
 Changed implementation of panel scale determination.  Also modified
 arguments to make_scale() to permit inverted scales.

 Revision 1.13  1998/08/26 14:48:29  borland
 Treatment of IEEE math function isinf is now uniform.  If on solaris
 and sunmath is missing, then modify mdb.h and Makefile.Unix.

 Revision 1.12  1998/08/25 15:05:37  borland
 New version with major changes to allow multiple scales on x and y axes.

 Revision 1.11  1998/06/19 19:48:39  borland
 Allow smaller user-coordinate ranges as a fraction of central value;
 now allow 1e-11 rather than 1e-10.  Needed for time plots with high
 resolution.

 Revision 1.10  1996/08/20 01:56:24  borland
 Changed limit for range/average from 1e-5 to 1e-10 to prevent goof-up of
 scaling for time mode.

 Revision 1.9  1996/08/16 20:51:43  borland
 Fixed bugs in recalculate_small_mapping().  Wasn't taking into account
 possibility of negative average values.

 Revision 1.8  1996/08/16 20:00:38  borland
 Modified autoscaling so that when the range of data is very small
 compared to the average, the scales are artificially increased.
 This prevents scales that are so small that the computations of
 integer coordinates get messed up.

 * Revision 1.7  1996/02/01  23:34:19  borland
 * Fixed problems with offset mode plotting when a page was column-binned.
 * Had to add xOrig0 and yOrig0 variables to PLOT_DATA structure to keep
 * track of original 0,0 values for data set prior to splitting.
 *
 * Revision 1.6  1996/01/24  05:13:21  borland
 * Fixed print statement for files with no pages; changed from error with
 * exit to warning.  Changed range determination to set range to zero if
 * there is no data (was +/- DBL_MAX).
 *
 * Revision 1.5  1996/01/16  18:49:18  borland
 * Fixed problem in -limit=autoscaling when data was transformed (have to
 * transform limits also).  Fixed bugs in enumerate scale routines that
 * resulted in misplaced ticks; fixed autoscaling of y enumerated labels.
 *
 * Revision 1.4  1995/12/08  07:08:04  borland
 * Added autoscaling qualifier to -limit option. Modified mapping routines
 * to set scales to +/- Sqrt(DBL_MAX)/2 if they are undefined; produces
 * better results when plotting multiple panels.
 *
 * Revision 1.3  1995/12/01  01:12:43  borland
 * Added global qualifier to -sameScale option; changed range determination
 * code to implement.  Added sticky feature for individual -limit fields;
 * global values for individual fields can only be overridden by local values
 * for the same field.
 *
 * Revision 1.2  1995/09/05  21:09:59  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "graph.h"
#include "SDDS.h"
#include "sddsplot.h"
#include <ctype.h>

#if defined(_WIN32)
#include <float.h>
/*#define isnan(x) _isnan(x)*/
#endif

void zoom_panel_limits(PLOT_SPEC *plspec, long panel, long last);
void align_panel_zeroes(PLOT_SPEC *plspec, long panel);


void determine_limits(PLOT_SPEC *plspec)
{
  long set, panel, i, j;
  long request;
  PLOT_REQUEST *plreq;

#ifdef DEBUG
  fprintf(stderr, "memory %10ld  after determine_limits 1\n", memory_count());
#endif

  for (set=0; set<plspec->datasets; set++) {
    request = plspec->dataset[set].request_index;
    plreq = plspec->plot_request+request;
    determine_dataset_range(plspec->dataset[set].limit, plreq->mapping, plspec->dataset+set,
                            &plreq->limit);
    if (plreq->flags&PLREQ_UNSUPPRESSX && !(plreq->mode&MODE_X_LOG))
      unsuppress_zero_limits(plspec->dataset[set].limit+0, plspec->dataset[set].limit+1);
    if (plreq->flags&PLREQ_UNSUPPRESSY && !(plreq->mode&MODE_Y_LOG))
      unsuppress_zero_limits(plspec->dataset[set].limit+2, plspec->dataset[set].limit+3);
    if (plreq->range.flags)
      impose_range_limits(plspec->dataset[set].limit, &plreq->range, plreq->mode&(MODE_X_LOG+MODE_Y_LOG));
  }
#ifdef DEBUG
  fprintf(stderr, "memory %10ld  after determine_limits 2\n", memory_count());
#endif

  if (plspec->plot_request[0].flags&PLREQ_SAMESCALEGLOBAL && plspec->plot_request[0].flags&PLREQ_SAMESCALEX)
    equalize_dataset_ranges(plspec, -1, 0);
  else {
    for (request=0; request<plspec->plot_requests; request++) {
      if (plspec->plot_request[request].flags&PLREQ_SAMESCALEX)
        equalize_dataset_ranges(plspec, request, 0);
    }
  }

#ifdef DEBUG
  fprintf(stderr, "memory %10ld  after determine_limits 3\n", memory_count());
#endif

  if (plspec->plot_request[0].flags&PLREQ_SAMESCALEGLOBAL && plspec->plot_request[0].flags&PLREQ_SAMESCALEY)
    equalize_dataset_ranges(plspec, -1, 1);
  else {
    for (request=0; request<plspec->plot_requests; request++) {
      if (plspec->plot_request[request].flags&PLREQ_SAMESCALEY)
        equalize_dataset_ranges(plspec, request, 1);
    }
  }

#ifdef DEBUG
  fprintf(stderr, "memory %10ld  after determine_limits 4\n", memory_count());
#endif

  /* determine the panel limits from the data limits */
  for (panel=0; panel<plspec->panels; panel++) 
    determine_panel_limits(plspec, panel);

#ifdef DEBUG
  fprintf(stderr, "memory %10ld  after determine_limits 5\n", memory_count());
#endif

  for (request=0; request<plspec->plot_requests; request++) {
    /* for each request, if the user gave his own scales, restore those */
    if (plspec->plot_request[request].mapping[0]!=plspec->plot_request[request].mapping[1]) 
      restore_fixed_limits(0, plspec, request);
    if (plspec->plot_request[request].mapping[2]!=plspec->plot_request[request].mapping[3]) 
      restore_fixed_limits(1, plspec, request);
  }

#ifdef DEBUG
  fprintf(stderr, "memory %10ld  after determine_limits 6\n", memory_count());
#endif

  /* do final zooms, then align zeros */
  for (panel=0; panel<plspec->panels; panel++) {
    zoom_panel_limits(plspec, panel, 1);
    align_panel_zeroes(plspec, panel);
  }
  
#ifdef DEBUG
  fprintf(stderr, "memory %10ld  after determine_limits 7\n", memory_count());
#endif

  for (i=0; i<2; i++) {
    for (j=0; j<plspec->scalesGroups[i]; j++) {
      if (isnan(plspec->scalesGroupData[i][j].limit[0]) ||
          isinf(plspec->scalesGroupData[i][j].limit[0]))
        plspec->scalesGroupData[i][j].limit[0]= -sqrt(DBL_MAX/2);
      if (isnan(plspec->scalesGroupData[i][j].limit[1]) ||
          isinf(plspec->scalesGroupData[i][j].limit[1]))
        plspec->scalesGroupData[i][j].limit[1]= sqrt(DBL_MAX/2);
    }
  }

#ifdef DEBUG
  fprintf(stderr, "memory %10ld  after determine_limits 8\n", memory_count());
#endif

}

void restore_fixed_limits(long plane, PLOT_SPEC *plspec, long request)
/* reassert "fixed" (i.e., user-supplied) limits for the plot panels originating
 * in the given request
 */
{
  long panel, set, group, offset;
  offset = 2*plane;
  for (set=0; set<plspec->datasets; set++) {
    if (plspec->dataset[set].request_index!=request || 
        (panel = plspec->dataset[set].plotpanel)==-1)
      continue;
    group = plspec->dataset[set].scalesGroupIndex[plane];
    plspec->scalesGroupData[plane][group].limit[0] = plspec->plot_request[request].mapping[offset+0];
    plspec->scalesGroupData[plane][group].limit[1] = plspec->plot_request[request].mapping[offset+1];
  }
}

void equalize_dataset_ranges(PLOT_SPEC *plspec, long request, long dimen)
/* Make equal all of the dataset limits for datasets originating from the
 * indicated request and belonging to the same scales group.  Actually, the scales group
 * qualification only comes in when request==-1, indicating global equalization was
 * requested.
 */
{
  long set, first;
  double limit[4];
  long offset, sGroup, datasets, scalesGroups;
  PLOT_DATA *dataset;

  datasets = plspec->datasets;
  dataset  = plspec->dataset;
  offset = dimen*2;
  if (request==-1)
    scalesGroups = plspec->globalScalesGroups[dimen];
  else
    scalesGroups = plspec->scalesGroups[dimen];
  for (sGroup=0; sGroup<scalesGroups; sGroup++) {
    first = 1;
    limit[0] = limit[1] = DBL_MAX;
    limit[2] = limit[3] = DBL_MAX;
    for (set=0; set<datasets; set++) {
      if (request!=-1 && dataset[set].request_index!=request)
        continue;
      if (dataset[set].globalScalesGroupIndex[dimen]!=sGroup)
        continue;
      if (dataset[set].limit[0+offset]==DBL_MAX &&
          dataset[set].limit[1+offset]==DBL_MAX )
        continue;
      if (first) {
        limit[offset  ] = dataset[set].limit[offset  ];
        limit[offset+1] = dataset[set].limit[offset+1];
        first = 0;
      }
      else {
        if (limit[offset+0]>dataset[set].limit[offset+0])
          limit[offset+0] = dataset[set].limit[offset+0];
        if (limit[offset+1]<dataset[set].limit[offset+1])
          limit[offset+1] = dataset[set].limit[offset+1];
      }
    }
    for (set=0; set<datasets; set++) {
      if (request!=-1 && dataset[set].request_index!=request)
        continue;
      if (dataset[set].globalScalesGroupIndex[dimen]!=sGroup)
        continue;
      dataset[set].limit[offset+0] = limit[offset+0];
      dataset[set].limit[offset+1] = limit[offset+1];
    }
  }
}


void unsuppress_zero_limits(double *lo, double *hi)
/* include zero within the range [*lo, *hi], if it isn't in the range already */
{
  if (*lo==DBL_MAX && *hi==DBL_MAX)
    return;
  if (*lo<0 && *hi>0) 
    return;
  if (*lo<0) {
    *hi = 0;
    return;
  }
  if (*hi>0) {
    *lo = 0;
    return;
  }
}

void determine_panel_limits(PLOT_SPEC *plspec, long panel) 
/* Determine the user's coordinate limits for the given panel from the limits for the datasets belonging
 * to that panel. 
 */
{
  long i, scalesGroup, scalesGroupsUsed, scalesOtherSide;
  long datasets, plane, offset;
  PLOT_DATA **dataset;
  PLOT_REQUEST *plreq;
  double *limit=NULL;
  
  datasets = plspec->panel[panel].datasets;
  dataset  = plspec->panel[panel].dataset;

  for (plane=0; plane<2; plane++) {
    offset = 2*plane;
    
    scalesGroupsUsed = scalesOtherSide = 0;
    plspec->panel[panel].scalesGroupIndex[plane] = NULL;
                  
    for (scalesGroup=0; scalesGroup<plspec->scalesGroups[plane]; scalesGroup++) {
      if (plspec->scalesGroupData[plane][scalesGroup].panel!=panel)
        continue;
      limit = plspec->scalesGroupData[plane][scalesGroup].limit;
      for (i=0; i<datasets; i++) {
        if (dataset[i]->scalesGroupIndex[plane]!=scalesGroup || 
            plspec->plot_request[dataset[i]->request_index].overlay.flags)
            continue;
        if (dataset[i]->fromOmni) 
          memcpy(dataset[i]->limit+plane*2, 
                 plspec->dataset[dataset[i]->omniSourceDataset].limit+plane*2,
                 sizeof(*dataset[i]->limit)*2);
        if (!(plspec->scalesGroupData[plane][scalesGroup].limitFlags&LIMIT0_SET))
          limit[0] = DBL_MAX;
        if (!(plspec->scalesGroupData[plane][scalesGroup].limitFlags&LIMIT1_SET))
          limit[1] = -DBL_MAX;
        if (!(plspec->scalesGroupData[plane][scalesGroup].limitFlags&LIMIT_COUNTED)) {
          scalesGroupsUsed++;
          if (plspec->scalesGroupData[plane][scalesGroup].flags&SCALESGROUP_OTHER_SIDE)
              scalesOtherSide++;
          plspec->scalesGroupData[plane][scalesGroup].limitFlags |= LIMIT_COUNTED;
          plspec->scaleLabelInfo[plane][scalesGroup].flags = SCALE_LABEL_USED;
          plspec->panel[panel].scalesGroupIndex[plane] = trealloc(plspec->panel[panel].scalesGroupIndex[plane],
                                                                 sizeof(*plspec->panel[panel].scalesGroupIndex[plane])*scalesGroupsUsed);
          plspec->panel[panel].scalesGroupIndex[plane][scalesGroupsUsed-1] = scalesGroup;
        }
        plreq = plspec->plot_request+dataset[i]->request_index;
        if (plreq->overlay.flags==0 || plreq->overlay.flags&(OVERLAY_NORMAL<<plane)) {
          if (dataset[i]->limit[0+offset]==DBL_MAX && dataset[i]->limit[1+offset]==DBL_MAX)
            continue;
          if (dataset[i]->limit[0+offset]<limit[0]) {
            limit[0] = dataset[i]->limit[0+offset];
            plspec->scalesGroupData[plane][scalesGroup].limitFlags |= LIMIT0_SET;
          }
          if (dataset[i]->limit[1+offset]>limit[1]) {
            limit[1] = dataset[i]->limit[1+offset];
            plspec->scalesGroupData[plane][scalesGroup].limitFlags |= LIMIT1_SET;
          }
        }
      }
    }
    if (!scalesGroupsUsed)
      SDDS_Bomb("No scales groups used (determine_panel_limit)");
    plspec->panel[panel].scalesUsed[plane] = scalesGroupsUsed;
    plspec->panel[panel].scalesOtherSide[plane] = scalesOtherSide;
  }
  zoom_panel_limits(plspec, panel, 0);
}

void zoom_panel_limits(PLOT_SPEC *plspec, long panel, long delay) 
{
  long i, scalesGroup, zoomed, used;
  long datasets, plane;
  PLOT_DATA **dataset;
  PLOT_REQUEST *plreq;
  double limit[2];
  
  datasets = plspec->panel[panel].datasets;
  dataset  = plspec->panel[panel].dataset;

  for (plane=0; plane<2; plane++) {
    for (scalesGroup=0; scalesGroup<plspec->scalesGroups[plane]; scalesGroup++) {
      if (plspec->scalesGroupData[plane][scalesGroup].panel!=panel)
        continue;
      zoomed = used = 0;
      memcpy(limit, plspec->scalesGroupData[plane][scalesGroup].limit, sizeof(double)*2);
      for (i=0; i<datasets; i++) {
        if (dataset[i]->scalesGroupIndex[plane]!=scalesGroup)
          continue;
        used = 1;
        plreq = plspec->plot_request+dataset[i]->request_index;
        if (!zoomed && plreq->zoom.flags&((ZOOM_MAG+ZOOM_CEN+ZOOM_UCEN)<<plane) &&
            ((plreq->zoom.flags&ZOOM_DELAY && delay) ||
             (!(plreq->zoom.flags&ZOOM_DELAY && !delay)))) 
          zoomed = determine_zoomed_limits(limit, &plreq->zoom, plane);
      }
      if (used) {
        if (!zoomed && !delay)
          expand_interval(limit, limit+1, 0.10); 
        recalculate_small_limits(limit, limit+1);
        memcpy(plspec->scalesGroupData[plane][scalesGroup].limit, limit, sizeof(double)*2);
      }
    }
  }
}


void align_panel_zeroes(PLOT_SPEC *plspec, long panel) 
{
  double meanLnRatio, factor, ratio, meanRatio;
  long scalesIncluded, scalesTested;
  long i, scalesGroup;
  long datasets, plane, alignReqIndex;
  PLOT_DATA **dataset;
  PLOT_REQUEST *plreq, *plreq1;
  double *limit, alignUnitValue=0;
  unsigned long alignRequested;
  
  datasets = plspec->panel[panel].datasets;
  dataset  = plspec->panel[panel].dataset;

#ifdef DEBUG
  fprintf(stderr, "aligning zeros for panel %ld\n", panel);
#endif

  for (plane=0; plane<2; plane++) {
    alignRequested = 0;
    for (scalesGroup=0; scalesGroup<plspec->scalesGroups[plane]; scalesGroup++) {
      if (plspec->scalesGroupData[plane][scalesGroup].panel!=panel)
        continue;
      if ((alignReqIndex = plspec->scalesGroupData[plane][scalesGroup].alignZeroRequestIndex)==-1 ||
          !(limit = plspec->scalesGroupData[plane][scalesGroup].limit))
        continue;
      plreq = plspec->plot_request+alignReqIndex;
      for (i=0; i<datasets; i++) {
        if (dataset[i]->scalesGroupIndex[plane]!=scalesGroup)
          continue;
        plreq1 =  plspec->plot_request+dataset[i]->request_index;
        if (plreq1->overlay.flags &&
            !(plreq1->overlay.flags&(OVERLAY_NORMAL<<plane)))
          continue;
#ifdef DEBUG
        fprintf(stderr, "align settings for dataset %ld: %x\n",
                i, plreq->alignSettings.flags);
#endif
        if ((alignRequested = 
            plreq->alignSettings.flags&(ALIGNZERO_XOPTS<<plane))) {
          alignUnitValue = (plreq->alignSettings.flags&(ALIGNZERO_PPIN_GIVEN<<plane))
            ? plreq->alignSettings.pinUnitSpace[plane] : 0;
#ifdef DEBUG
          fprintf(stderr, "group %ld, align (%lx) of plane %ld: limit = %le, %le\n", 
                  scalesGroup, alignRequested, plane, limit[0], limit[1]);
#endif
          if (limit[0]>0 && limit[1]>0)
            limit[0] = 0;
          if (limit[0]<0 && limit[1]<0)
            limit[1] = 0;
#ifdef DEBUG
          fprintf(stderr, "limit -> %le, %le\n", limit[0], limit[1]);
#endif
          break;
        }
      }
    }
    if (alignRequested&(ALIGNZERO_XFACTOR_GIVEN<<plane)) {
      scalesIncluded = scalesTested = 0;
      meanLnRatio = 0;
      for (scalesGroup=0; scalesGroup<plspec->scalesGroups[plane]; scalesGroup++) {
        limit = plspec->scalesGroupData[plane][scalesGroup].limit;
        scalesTested++;
        if (limit[0]==0)
          limit[0] -= (limit[1]-limit[0])*0.1;
        if (limit[1]==0)
          limit[1] += (limit[1]-limit[0])*0.1;
        if (limit[0]<0 && limit[1]>0) {
          scalesIncluded++;
          ratio = limit[1]/limit[0];
          meanLnRatio += log(fabs(ratio));
        }
      }
#ifdef DEBUG
      fprintf(stderr, "factor alignment: tested=%ld, included=%ld, meanLnRatio=%le\n",
              scalesTested, scalesIncluded, meanLnRatio);
#endif
      if (scalesTested<2)
        continue;
      if (scalesIncluded) {
        meanRatio = exp(meanLnRatio/scalesIncluded);
        for (scalesGroup=0; scalesGroup<plspec->scalesGroups[plane]; scalesGroup++) {
          limit = plspec->scalesGroupData[plane][scalesGroup].limit;
          factor = fabs((limit[1]/limit[0])/meanRatio);
          if (factor>1) 
            limit[0] = limit[0]*factor;
          else 
            limit[1] = limit[1]/factor;
#ifdef DEBUG
          fprintf(stderr, "group=%le, factor = %le, limit -> %le, %le\n",
                  scalesGroup, factor, limit[0], limit[1]);
#endif                                    
        }
      }
    }
    if (alignRequested&(ALIGNZERO_PPIN_GIVEN<<plane)) {
      for (scalesGroup=0; scalesGroup<plspec->scalesGroups[plane]; scalesGroup++) {
        limit = plspec->scalesGroupData[plane][scalesGroup].limit;
        factor = (alignUnitValue-1)/alignUnitValue;
        if (limit[0]*factor>limit[1])
          limit[1] = limit[0]*factor;
        else if (fabs(limit[1]/factor)>fabs(limit[0]))
          limit[0] = limit[1]/factor;
      }
#ifdef DEBUG
      fprintf(stderr, "pin alignment: limit-> %le, %le\n",
              limit[0], limit[1]);
#endif
    }
  }
}

void recalculate_small_limits(double *map0, double *map1)
{ 
  double average, range;
  range = *map1 - *map0;
  average = (*map1 + *map0)/2;
  if (range==0) {
    if (average==0) {
      *map0 -= 0.5;
      *map1 += 0.5;
      return;
    } else {
      *map0 = average-fabs(average)/10;
      *map1 = average+fabs(average)/10;
      range = *map1 - *map0;
    }
  }
  if (average && fabs(range/average)<1e-14) {
    if (average>0) {
      *map0 = average*(1-1e-14);
      *map1 = average*(1+1e-14);
    } else {
      *map0 = average*(1+1e-14);
      *map1 = average*(1-1e-14);
    }
  }
}

long determine_zoomed_limits(double *mapping, ZOOM_SPEC *zoom, long plane)
{
  double center, range;
  
  if (mapping[0]==DBL_MAX && mapping[1]==DBL_MAX)
    return 0;
  if (zoom->flags&(ZOOM_MAG<<plane) && zoom->magnification[plane]>0) {
    center = (mapping[0]+mapping[1])/2;
    range = (mapping[1]-mapping[0])/(2*zoom->magnification[plane]);
    if (zoom->flags&(ZOOM_CEN<<plane)) 
      center = zoom->center[plane];
    else if (zoom->flags&(ZOOM_UCEN<<plane)) 
      center = zoom->center[plane]*(mapping[1]-mapping[0]) + mapping[0];
    mapping[0] = center - range;
    mapping[1] = center + range;
    return 1;
  }
  return 0;
}

void determine_dataset_range(double *mapping, double *limit, PLOT_DATA *dataset,
                             LIMIT_SETTINGS *limitSettings)
{
  long j;
  double x, y, sx, sy, xadj, yadj;
  short xAutoLimited[2], yAutoLimited[2];

  mapping[0] = mapping[2] = DBL_MAX;
  mapping[1] = mapping[3] = -DBL_MAX;
  xAutoLimited[0] = (limitSettings->flags&LIMIT_AUTOSCALING) && (limitSettings->flags&LIMIT_XMIN_GIVEN);
  yAutoLimited[0] = (limitSettings->flags&LIMIT_AUTOSCALING) && (limitSettings->flags&LIMIT_YMIN_GIVEN);
  xAutoLimited[1] = (limitSettings->flags&LIMIT_AUTOSCALING) && (limitSettings->flags&LIMIT_XMAX_GIVEN);
  yAutoLimited[1] = (limitSettings->flags&LIMIT_AUTOSCALING) && (limitSettings->flags&LIMIT_YMAX_GIVEN);
  
  for (j=0; j<dataset->points; j++) {
    x = dataset->x[j];
    y = dataset->y[j];
    sx = (dataset->x1?dataset->x1[j]:0);
    sy = (dataset->y1?dataset->y1[j]:0);
    if (limit[0]!=limit[1]) {
      if (x<limit[0] || x>limit[1])
        continue;
    }
    else {
      xadj = x;
      if (dataset->graphic.element==PLOT_ERRORBAR)
        xadj = x - sx;
      if (xadj<mapping[0] && (!xAutoLimited[0] || xadj>limitSettings->cxmin))
        mapping[0] = xadj;
      if (dataset->graphic.element==PLOT_ERRORBAR)
        xadj = x + sx;
      if (xadj>mapping[1] && (!xAutoLimited[1] || xadj<limitSettings->cxmax))
        mapping[1] = xadj;
    }
    if (limit[2]!=limit[3]) {
      if (y<limit[2] || y>limit[3])
        continue;
    }
    else {
      yadj = y;
      if (dataset->graphic.element==PLOT_ERRORBAR)
        yadj = y - sy;
      if (yadj<mapping[2] && (!yAutoLimited[0] || yadj>limitSettings->cymin))
        mapping[2] = yadj;
      if (dataset->graphic.element==PLOT_ERRORBAR)
        yadj = y + sy;
      if (yadj>mapping[3] && (!yAutoLimited[1] || yadj<limitSettings->cymax))
        mapping[3] = yadj;
    }
  }
  if (limit[0]!=limit[1]) {
    mapping[0] = limit[0];
    mapping[1] = limit[1];
  }
  if (mapping[0]==DBL_MAX && mapping[1]==-DBL_MAX)
    mapping[0] = mapping[1] = DBL_MAX;
  if (limit[2]!=limit[3]) {
    mapping[2] = limit[2];
    mapping[3] = limit[3];
  }
  if (mapping[2]==DBL_MAX && mapping[3]==-DBL_MAX)
    mapping[2] = mapping[3] = DBL_MAX;
}

void expand_interval(double *lower, double *upper, double fraction)
{
  double midpoint, range;
  midpoint = (*lower + *upper)/2;
  range    = (*upper - *lower)/2;
  *lower = midpoint - (1+fraction)*range;
  *upper = midpoint + (1+fraction)*range;
}

/* when computing the lower pspace limit for N scales, use
 * an exponential fit to these values
 */
#define PSPACE_LIMIT 0.5
#define PSPACE_1SCALE 0.15
#define PSPACE_2SCALE 0.20

void determine_panel_pspaces(PLOT_SPEC *plspec)
{
  long panel, dataset, request, panelsOnPage, panel0, panel1;
  double *pspace, *lspace, *workSpace, p2min, p3max;
  long ix, iy;
  long plane, scaleCount, scaleCountOtherSide, otherPlane;
  double expCoef, expFactor;
  long lastRequest = 0, endPage=0, uneven=0;
  unsigned long joinScaleFlags;
  
  expCoef = -log((PSPACE_2SCALE-PSPACE_LIMIT)/(PSPACE_1SCALE-PSPACE_LIMIT));
  expFactor = -(PSPACE_1SCALE-PSPACE_2SCALE)/(exp(-expCoef)-exp(-2*expCoef));
  ix = iy = panelsOnPage = 0;
  for (panel=0; panel<plspec->panels; panel++) {
    /* In this loop, I figure out the pspace as if this panel was alone on the plot page. */

    /* Count how many scales need to be accomodated for each plane. */
    /* Also compute the lower pspace limit for each plane. */
    for (plane=0; plane<2; plane++) {
      otherPlane = plane?0:1;
      scaleCount = plspec->panel[panel].scalesUsed[plane];
      scaleCountOtherSide = plspec->panel[panel].scalesOtherSide[plane];
      if (scaleCount<2)
        plspec->panel[panel].pspace[otherPlane*2] = PSPACE_1SCALE;
      else {
        plspec->panel[panel].pspace[otherPlane*2+0] = PSPACE_LIMIT - expFactor*exp(-expCoef*(scaleCount-scaleCountOtherSide));
        plspec->panel[panel].pspace[otherPlane*2+1] = PSPACE_LIMIT + expFactor*exp(-expCoef*(scaleCountOtherSide));
      }
    }

    /* Finalize the pspace limits, keeping the user's limit if one is given. */
    plspec->panel[panel].pspace[1] = 0.85;
    plspec->panel[panel].pspace[3] = 0.85;
    if (!(plspec->panel[panel].flags&PLREQ_TOPTITLE)) {
      /* adjust pspace so title can fit at bottom */
      plspec->panel[panel].pspace[3] += (1-0.85)/2;
      plspec->panel[panel].pspace[2] += (1-0.85)/2;
    }
    for (dataset=0; dataset<plspec->panel[panel].datasets; dataset++) {
      request = plspec->panel[panel].dataset[dataset]->request_index;
      pspace = plspec->plot_request[request].pspace;
      if (pspace[0]!=pspace[1] || pspace[2]!=pspace[3]) {
        if (pspace[0]!=pspace[1]) {
          plspec->panel[panel].pspace[0] = pspace[0];
          plspec->panel[panel].pspace[1] = pspace[1];
        }
        if (pspace[2]!=pspace[3]) {
          plspec->panel[panel].pspace[2] = pspace[2];
          plspec->panel[panel].pspace[3] = pspace[3];
        }
        break;
      }
    }

    /* Compute the legend space if it isn't given or is invalid. */
    lspace = plspec->panel[panel].lspace;
    if (!(plspec->panel[panel].flags&PLREQ_LSPACE_GIVEN) || lspace[0]>=lspace[1]) {
      lspace[0] = 1.01;
      lspace[1] = (0.99-plspec->panel[panel].pspace[1])/
        (plspec->panel[panel].pspace[1]-plspec->panel[panel].pspace[0])+1;
    }
    if (!(plspec->panel[panel].flags&PLREQ_LSPACE_GIVEN) || lspace[2]>=lspace[3]) {
      lspace[2] = 0;
      lspace[3] = 1;
    }
    
    /* Now adjust the pspace data for this panel according to the number of
     * panels that it shares the page with and its position on the
     * page.  The legend space doesn't change because it is relative to
     * the pspace.
     */
    if (panelsOnPage==plspec->maxPanelsPerPage)
      ix = iy = panelsOnPage = 0;
    request = lastRequest;
    for (dataset=0; dataset<plspec->panel[panel].datasets; dataset++) {
      request = MAX(plspec->panel[panel].dataset[dataset]->request_index, request);
    }
    if (request != lastRequest) {
      if (endPage == 1) {
	endPage = 0;
	ix = iy = panelsOnPage = 0;
      }
      if (plspec->plot_request[request].flags&PLREQ_NEXTPAGE) {
	endPage = 1;
      }
      lastRequest = request;
    }
    pspace = plspec->panel[panel].pspace;
    workSpace = plspec->panel[panel].workSpace;
    plspec->panel[panel].ix = ix;
    plspec->panel[panel].iy = iy;
    plspec->panel[panel].last_panel_on_page = 
      plspec->panel[panel].last_panel_in_row = 
        plspec->panel[panel].last_panel_in_column = 0;
    workSpace[0] = workSpace[2] = 0;
    workSpace[1] = workSpace[3] = 1;
    if (plspec->layout[0] && plspec->layout[1]) {
      workSpace[0] = (1.0*ix+0)/plspec->layout[0];
      workSpace[1] = (1.0*ix+1)/plspec->layout[0];
      workSpace[2] = (plspec->layout[1]*1.0 - 1 - iy)/plspec->layout[1];
      workSpace[3] = (plspec->layout[1]*1.0 - 0 - iy)/plspec->layout[1];
      pspace[0] = (ix + pspace[0])/plspec->layout[0];
      pspace[1] = (ix + pspace[1])/plspec->layout[0];
      pspace[2] = (plspec->layout[1] - 1 - iy + pspace[2])/plspec->layout[1];
      pspace[3] = (plspec->layout[1] - 1 - iy + pspace[3])/plspec->layout[1];
      if (plspec->panel[panel].joinScaleFlags&PLREQ_JOINSCALE_X) {
        if (++iy == plspec->layout[1]) {
          iy = 0;
          if (++ix == plspec->layout[0])
            ix = 0;
        }
        if ((panel == plspec->panels - 1) && ((ix != 0) || (iy != 0)))
          {
            ix = iy = 0;
            uneven = 1;
          }
      } else {
        if (++ix == plspec->layout[0]) {
          ix = 0;
          if (++iy == plspec->layout[1])
            iy = 0;
        }
      }
    }
    panelsOnPage ++;
    if (ix==0)
      plspec->panel[panel].last_panel_in_row = 1;
    if (iy==0)
      plspec->panel[panel].last_panel_in_column = 1;
    if (panelsOnPage==plspec->maxPanelsPerPage || (ix==0 && iy==0)) 
      plspec->panel[panel].last_panel_on_page = 1;
  }

  /* Adjust pspaces for joining of x scales */
  panel0 = 0;
  joinScaleFlags = 0;                      
  p2min = plspec->panel[0].pspace[2];
  p3max = plspec->panel[0].pspace[3];

  for (panel=1; panel<plspec->panels; panel++) {
    if (p2min>plspec->panel[panel].pspace[2])
        p2min = plspec->panel[panel].pspace[2];
    if (p3max<plspec->panel[panel].pspace[3])
      p3max = plspec->panel[panel].pspace[3];
  }
  for (panel=1; panel<plspec->panels; panel++) {
    if (plspec->panel[panel].last_panel_in_column) {
      joinScaleFlags |= plspec->panel[panel].joinScaleFlags;
      if (joinScaleFlags&PLREQ_JOINSCALE_X) {
        for (panel1=panel0; panel1<=panel; panel1++) {
          if (panel1!=panel0) {
            /* adjust upper pspace */
            plspec->panel[panel1].pspace[3] = p3max-((p3max-p2min)*plspec->panel[panel1].iy)/plspec->layout[1];
            /* no topline allowed */
            plspec->panel[panel1].joinScaleFlags |= JOINSCALE_NOTOP;
          }
          if (panel1!=panel) {
            /* adjust lower space */
            plspec->panel[panel1].pspace[2] = plspec->panel[panel1].pspace[3] - (p3max-p2min)/plspec->layout[1];
            plspec->panel[panel1].joinScaleFlags |= JOINSCALE_NOBOT;
          } else if ((panel == plspec->panels - 1) && (uneven == 1)) {
            plspec->panel[panel1].workSpace[2] += (plspec->panel[panel1].pspace[3] - (p3max-p2min)/plspec->layout[1]) - plspec->panel[panel1].pspace[2];
            plspec->panel[panel1].pspace[2] = plspec->panel[panel1].pspace[3] - (p3max-p2min)/plspec->layout[1];
          }
        }
        joinScaleFlags = 0;
      }
      panel0 = panel+1;
    }
  }
}


void impose_range_limits(double *mapping0, RANGE_SPEC *range, unsigned long logMode)
{
  long plane;
  double center0, range0, range1, *mapping;
  unsigned long logFlag[2] = {MODE_X_LOG, MODE_Y_LOG};
    
  for (plane=0; plane<2; plane++) {
    mapping = mapping0+2*plane;
    if (mapping[0]==DBL_MAX && mapping[1]==DBL_MAX)
      continue;
    center0 = (mapping[1] + mapping[0])/2;
    range0  = (mapping[1] - mapping[0]);
    if (logFlag[plane]&logMode) {
      if (mapping[0]==DBL_MAX || mapping[1]==DBL_MAX)
        center0 = 0;
      if (range0==0)
        range0 = 1;
    }
    if (range->flags&(RANGE_CENTER<<plane)) {
      center0 = range->center[plane];
      range0 = 2.2*fabs(mapping[0]-center0);
      range1 = 2.2*fabs(mapping[1]-center0);
      if (range1>range0)
        range0 = range1;
    }
    if (range->flags&(RANGE_MAXIMUM<<plane) && range0>range->maximum[plane])
      range0 = range->maximum[plane];
    if (range->flags&(RANGE_MINIMUM<<plane) && range0<range->minimum[plane])
      range0 = range->minimum[plane];
    mapping[0] = center0 - range0/2;
    mapping[1] = center0 + range0/2;
  }
}

int compareScalesGroupData(const void *d1, const void *d2)
{
  const SCALES_GROUP_DATA *d1sg, *d2sg;
  long diffPanel;
  d1sg = d1;
  d2sg = d2;
  diffPanel = d1sg->panel - d2sg->panel;
  if (diffPanel)
    return diffPanel;
  return strcmp(d1sg->ID, d2sg->ID);
}

char *determineScalesGroupID(long plane, PLOT_REQUEST *plreq, PLOT_DATA *dataset);
void assign_scales_groups1(PLOT_SPEC *plspec, long global);

void assign_scales_groups(PLOT_SPEC *plspec) 
{
  /* assign "local" scales group, which is specific to each panel 
   * I.e., two scales on different panels may have the same local scales
   * group index
   */
  assign_scales_groups1(plspec, 0);
  /* assign "global" scales group */
  assign_scales_groups1(plspec, 1);
}


void assign_scales_groups1(PLOT_SPEC *plspec, long global)
{
  long set, panel, index, i, plane;
  int32_t duplicate;
  long requestIndex, scalesGroups, maxScalesGroups;
  PLOT_REQUEST *plreq;
  SCALES_GROUP_DATA **scalesGroup, *newGroup, keyGroup;
  unsigned long long tickOptions;
  
  maxScalesGroups = 0;
  scalesGroup = NULL; 
  newGroup = NULL;
  tickOptions = TICKSET_XOPTS;
  for (plane=0; plane<2; plane++) {
    scalesGroups = 0;
    /* Sort the datasets by the scalesGroup ID and panel, putting
     * the data into a new structure.
     */
    for (panel=0; panel<plspec->panels; panel++) {
      for (set=0; set<plspec->panel[panel].datasets; set++) {
        plreq = plspec->plot_request + 
          (requestIndex=plspec->panel[panel].dataset[set]->request_index);
        if ((scalesGroups+1)>maxScalesGroups &&
            !(scalesGroup=SDDS_Realloc
              (scalesGroup, sizeof(*scalesGroup)*(maxScalesGroups+=10))))
          SDDS_Bomb("Memory allocation failure (assign_scales_groups)");
        /* add this to the index */
        if (scalesGroups) {
          if (!(newGroup=calloc(1, sizeof(*newGroup)))) 
            SDDS_Bomb("Memory allocation failure (assign_scales_groups)");
          if (!(newGroup->ID = determineScalesGroupID(plane, plreq, 
                                                      plspec->panel[panel].dataset[set]))) {
            fprintf(stderr, "Couldn't determine scales group ID for plane %ld, dataset %ld\n",
                    plane, set);
            exit(1);
          }
          newGroup->panel = global?0:panel;
          index = binaryInsert((void**)scalesGroup, scalesGroups, 
                               newGroup, compareScalesGroupData, &duplicate);
          if (duplicate) {
            free(newGroup);
            newGroup = scalesGroup[index];
          }
        }
        else {
          if (!(scalesGroup[0] = calloc(1, sizeof(**scalesGroup)))) 
            SDDS_Bomb("Memory allocation failure (assign_scales_groups)");
          if (!(scalesGroup[0]->ID = determineScalesGroupID(plane, plreq, 
                                                            plspec->panel[panel].dataset[set]))) {
            fprintf(stderr, "Couldn't determine scales group ID for plane %ld, dataset %ld\n",
                    plane, set);
            exit(1);
          }
          scalesGroup[0]->panel = global?0:panel;
          duplicate = 0;
          newGroup = scalesGroup[0];
        }
        if (!duplicate) {
          scalesGroups++;
          newGroup->tickSettingsRequestIndex =
            newGroup->subtickSettingsRequestIndex = 
              newGroup->enumSettingsRequestIndex =
                newGroup->labelSpecRequestIndex = 
                  newGroup->alignZeroRequestIndex = -1;
          newGroup->mode = newGroup->flags = 0;
        }
        newGroup->mode |= plreq->mode;
        newGroup->flags |= plreq->flags;;
        if (newGroup->tickSettingsRequestIndex==-1 ||
            (newGroup->tickSettingsRequestIndex<requestIndex
             && plreq->tick_settings.flags&tickOptions))
          newGroup->tickSettingsRequestIndex = requestIndex;
        if (newGroup->subtickSettingsRequestIndex==-1 ||
            (newGroup->subtickSettingsRequestIndex<requestIndex
             && plreq->subtick_settings.flags&tickOptions))
          newGroup->subtickSettingsRequestIndex = requestIndex;
        if (newGroup->enumSettingsRequestIndex==-1 ||
            (newGroup->enumSettingsRequestIndex<requestIndex &&
             plreq->enumerate_settings.flags&ENUM_GIVEN))
          newGroup->enumSettingsRequestIndex = requestIndex;
        if (newGroup->labelSpecRequestIndex==-1 ||
            (newGroup->labelSpecRequestIndex<requestIndex &&
             plreq->label[plane].flags &&
	     plreq->label[plane].flags != LABEL_THICKNESS_GIVEN))
          newGroup->labelSpecRequestIndex = requestIndex;
        if (newGroup->alignZeroRequestIndex==-1 ||
            (newGroup->alignZeroRequestIndex<requestIndex &&
             plreq->label[plane].flags))
          newGroup->alignZeroRequestIndex = requestIndex;
      }
    }
    
    if (!scalesGroups)
      SDDS_Bomb("No scales groups (assign_scales_groups)");
    
    /* For each dataset, record its index in the scales group data list.
     */

    for (panel=0; panel<plspec->panels; panel++) {
      for (set=0; set<plspec->panel[panel].datasets; set++) {
        plreq = plspec->plot_request + 
          (requestIndex=plspec->panel[panel].dataset[set]->request_index);
        plreq = plspec->plot_request + (requestIndex=plspec->panel[panel].dataset[set]->request_index);
        keyGroup.ID = plspec->panel[panel].dataset[set]->scalesGroupID[plane];
        keyGroup.panel = global?0:panel;
        if ((index = 
             binaryIndexSearch((void**)scalesGroup, scalesGroups,
                               &keyGroup, compareScalesGroupData, 0))<0) {
          fprintf(stderr, "Error searching for scales group \"%s\" for panel %ld and plane %ld\n",
                  keyGroup.ID, keyGroup.panel, plane);
          SDDS_Bomb("Internal error (assign_scales_groups): scales group not in sorted list after construction.");
        }
        if (global)
          plspec->panel[panel].dataset[set]->globalScalesGroupIndex[plane] = 
            index;
        else 
          plspec->panel[panel].dataset[set]->scalesGroupIndex[plane] =
            index;
      }
    }
    

    /* Copy the scales group data */
    if (!global) {
      plspec->scalesGroups[plane] = scalesGroups;
      if (!(plspec->scalesGroupData[plane] = 
            SDDS_Malloc(sizeof(*plspec->scalesGroupData[plane])*scalesGroups)) ||
          !(plspec->scaleLabelInfo[plane] = 
            SDDS_Malloc(sizeof(*plspec->scaleLabelInfo[plane])*scalesGroups))) {
        SDDS_Bomb("Memory allocation failure (assign_scales_groups)");
      }
      for (i=0; i<scalesGroups; i++) {
        plspec->scalesGroupData[plane][i] = *scalesGroup[i];
        plspec->scalesGroupData[plane][i].limit[0] = plspec->scalesGroupData[plane][i].limit[1] = 0;
        plspec->scalesGroupData[plane][i].limitFlags = 0;
        memset(&plspec->scaleLabelInfo[plane][i], 1, sizeof(plspec->scaleLabelInfo[plane][i]));
        free(scalesGroup[i]);
        scalesGroup[i] = NULL;
      }
    }
    else {
      plspec->globalScalesGroups[plane] = scalesGroups;
      if (plspec->globalScalesGroupData[plane])
        free(plspec->globalScalesGroupData[plane]);
      if (!(plspec->globalScalesGroupData[plane] = 
            SDDS_Malloc(sizeof(*plspec->globalScalesGroupData[plane])*scalesGroups))) {
        SDDS_Bomb("Memory allocation failure (assign_scales_groups)");
      }
      for (i=0; i<scalesGroups; i++) {
        plspec->globalScalesGroupData[plane][i] = *scalesGroup[i];
        plspec->globalScalesGroupData[plane][i].limit[0] = plspec->globalScalesGroupData[plane][i].limit[1] = 0;
        plspec->globalScalesGroupData[plane][i].limitFlags = 0;
        free(scalesGroup[i]);
        scalesGroup[i] = NULL;
      }
    }
    
    tickOptions = TICKSET_YOPTS;
  }
  free(scalesGroup);
}

char *determineScalesGroupID
  (long plane,
  PLOT_REQUEST *plreq,
  PLOT_DATA *dataset
)
{
  char buffer[1024], *ptr;
  ptr = NULL;
  if (!plreq->scalesGroupSpec[plane].flags || 
      plreq->scalesGroupSpec[plane].flags&SCALESGROUP_ID_GIVEN) {
    SDDS_CopyString(&ptr, plreq->scalesGroupSpec[plane].ID);
  } else {
    if (plreq->scalesGroupSpec[plane].flags&SCALESGROUP_USE_FILEINDEX)
      sprintf(buffer, "FI%ld\001\002\003\004", dataset->file_index);
    else if (plreq->scalesGroupSpec[plane].flags&SCALESGROUP_USE_FILESTRING)
      sprintf(buffer, "FS%s\001\002\003\004", plreq->filename[dataset->file_index]);
    else if (plreq->scalesGroupSpec[plane].flags&SCALESGROUP_USE_NAMEINDEX)
      sprintf(buffer, "NI%ld\001\002\003\004", dataset->dataname_index);
    else if (plreq->scalesGroupSpec[plane].flags&SCALESGROUP_USE_NAMESTRING)
      sprintf(buffer, "NS%s\001\002\003\004", 
              plane?plreq->yname[dataset->dataname_index]:plreq->xname[dataset->dataname_index]);
    else if (plreq->scalesGroupSpec[plane].flags&SCALESGROUP_USE_PAGE)
      sprintf(buffer, "PG%ld\001\002\003\004", dataset->datapage);
    else if (plreq->scalesGroupSpec[plane].flags&SCALESGROUP_USE_REQUEST)
      sprintf(buffer, "RQ%ld\001\002\003\004", dataset->request_index);
    else if (plreq->scalesGroupSpec[plane].flags&SCALESGROUP_USE_UNITS)
      sprintf(buffer, "UN%s\001\002\003\004", dataset->info[plane].units);
    else if (plreq->scalesGroupSpec[plane].flags&SCALESGROUP_USE_INAMESTRING) {
      sprintf(buffer, "INS%s\001\002\003\004", 
              plane?plreq->yname[dataset->dataname_index]:plreq->xname[dataset->dataname_index]);
      str_tolower(buffer);
    }
    SDDS_CopyString(&ptr, buffer);
  }
  if (dataset->scalesGroupID[plane])
    free(dataset->scalesGroupID[plane]);
  dataset->scalesGroupID[plane] = ptr;
  return ptr;
}
