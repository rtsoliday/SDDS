/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sddsplotLabels.c
 * 
 * part of sddsplot (plotting program for SDDS files)
 *
 * Michael Borland, 1994.
 $Log: not supported by cvs2svn $
 Revision 1.24  2009/07/28 14:01:16  borland
 Added scroll feature to -replicate option.  Greatly improved memory management for
 split and replicated datasets.  Added -nocolorbar option.

 Revision 1.23  2006/10/20 14:57:58  jiaox
 1) Added format key word for -topline and -legend options.If given with paramteter keyword (or @), the format will be applied to the parameter output. If not given, but there is format_string for the parameter in
 the SDDS file, it output will take that format_string for output.

 2) Fixed a problem that when multiple file with multiple pages are plotted with -split=page -order=Spectral and eachpage keyword is used in -graphic option, only part of the spectra is actually used in the plot.

 Revision 1.22  2005/08/29 21:31:24  shang
 added linetype to xlabel, ylabel, topline and title options

 Revision 1.21  2003/09/02 19:29:29  soliday
 Cleaned up code for Linux.

 Revision 1.20  2003/07/14 20:49:46  soliday
 Backed out of last change.

 Revision 1.19  2003/07/14 14:19:08  soliday
 label commands can now be used when the overlay command is used.

 Revision 1.18  2002/08/14 17:24:55  soliday
 Added Open License

 Revision 1.17  2002/06/05 20:29:17  shang
 replaced SDDS_GetParameter() for dataset->label[i] by SDDS_GetParameterAsString() so that
 it accepts all kinds of parameter types

 Revision 1.16  2002/01/02 17:17:05  soliday
 Fixed a problem when non string parameters were used as labels.

 Revision 1.15  2001/08/13 20:11:54  soliday
 Added thickness options for several options.

 Revision 1.14  1999/07/21 18:07:26  borland
 Eliminated many memory leaks.  Program is now suitable for running with
 -repeat option, although not all leaks are gone.

 Revision 1.13  1999/06/03 16:05:32  soliday
 Removed compiler warnings under linux.

 Revision 1.12  1999/04/19 21:56:24  borland
 Increased max label length to 160.

 Revision 1.11  1999/04/16 13:46:31  borland
 Increased maximum label length to 120 and made code a little cleaner.
 No longer prints "..." if it incounters a single long symbol on the
 label.

 Revision 1.10  1999/02/03 19:38:51  borland
 Improved operation of -range option when the center/range is undefined.
 Improved operation of time-scales when the time range is undefined.

 Revision 1.9  1998/10/01 22:48:10  borland
 Reimplemented scale and offset qualifiers for -xlabel and -ylabel.
 Implemented {xy}scaleChars qualifier for -ticksettings.
 These options allow users to control the size of characters on scale labels
 and ticks labels.

 Revision 1.8  1998/08/25 15:05:36  borland
 New version with major changes to allow multiple scales on x and y axes.

 Revision 1.7  1998/04/11 19:31:08  borland
 Added editcommand to -filenamesOnTopline option.

 Revision 1.6  1996/02/09 00:07:33  borland
 Added xtime and ytime qualifiers to -ticksettings option, along with code
 to do time-labeled axes (assumes time values are times in seconds since
 epoch).

 * Revision 1.5  1995/11/13  15:58:57  borland
 * Added long cast to strlen() in comparison to satisfy Solaris compiler.
 *
 * Revision 1.4  1995/10/18  21:56:36  borland
 * Modified data-reading routines to read multiple instances of the same file
 * only once (sddsplotRead.c).  Other routines have minor modifications.
 *
 * Revision 1.3  1995/10/11  00:18:03  borland
 * Added ylabelontopline routine to sddsplotAP.c and supporting code in
 * sddsplotLabels.c, sddsplot.h, and sddsplot.c.
 *
 * Revision 1.2  1995/09/05  21:09:56  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#include "mdb.h"
#include "graph.h"
#include "SDDS.h"
#include "sddsplot.h"
#include <ctype.h>
#include <time.h>

void determine_labels(PLOT_SPEC *plspec)
{
  long panel;
  for (panel=0; panel<plspec->panels; panel++) 
    determine_panel_labels(plspec, panel);
}

char *makeNameUnitsLabel(PLOT_SPEC *plspec, long panel, long plane, long group);

void determine_panel_labels(PLOT_SPEC *plspec, long panel_index)
{
  long i, request, file, datasets;
  long j, plane, scaleCount, group, set, index, ig;
  static char buffer[SDDS_MAXLINE], s[SDDS_MAXLINE];
  PLOT_DATA **dataset;
  time_t timeValue;
  static unsigned long ticksetTime[2] = {TICKSET_XTIME, TICKSET_YTIME}, flags;
  char *newLabel, *editCommand;
  
  /* Construct labels for each scale for each plane */
  dataset = plspec->panel[panel_index].dataset;
  datasets = plspec->panel[panel_index].datasets;
#if defined(DEBUG)
  fprintf(stderr, "panel label determination: %ld datasets for panel %ld\n", 
          datasets, panel_index);
#endif
  newLabel = NULL;
  for (plane=0; plane<2; plane++) {
    scaleCount = 0;
    for (ig=0; ig<plspec->panel[panel_index].scalesUsed[plane]; ig++) {
      group = plspec->panel[panel_index].scalesGroupIndex[plane][ig];
      /* each scales group has a separate label */
      if (plspec->scaleLabelInfo[plane][group].flags!=SCALE_LABEL_USED)
          continue;
      plspec->scaleLabelInfo[plane][group].flags = 0;
      plspec->scaleLabelInfo[plane][group].scaleNumber = -1;
      plspec->scaleLabelInfo[plane][group].label = newLabel = editCommand = NULL;
      for (set=0; set<datasets; set++) {
        /* scan for datasets that belong to this group */
        if (dataset[set]->scalesGroupIndex[plane]!=group)
          continue;
        request = dataset[set]->request_index;
        /* If we get to this point, then a scale will be made for
         * this group.
         */
        if (plspec->scaleLabelInfo[plane][group].scaleNumber==-1)
          plspec->scaleLabelInfo[plane][group].scaleNumber = ++scaleCount;
        /* For each scales group, the label may have been specified by 
         * a -label option for one request.
         */
        if ((index=plspec->scalesGroupData[plane][group].labelSpecRequestIndex)>=0 &&
            index==request &&
            (flags=plspec->plot_request[index].label[plane].flags)) {
          if (flags&(LABEL_STRING_GIVEN+LABEL_PARAMETER_GIVEN+
                     LABEL_USE_NAME+LABEL_USE_SYMBOL+LABEL_USE_DESCRIPTION)) {
            if (newLabel)
              free(newLabel);
            if (!SDDS_CopyString(&newLabel, dataset[set]->label[plane]))
              SDDS_Bomb("Memory allocation failure (determine_panel_labels)");
          } else if (plspec->plot_request[request].overlay.flags)
            continue;
          if (flags&LABEL_EDITCOMMAND_GIVEN)
            editCommand = plspec->plot_request[index].label[plane].edit_command;
          if (flags&LABEL_SCALE_GIVEN) {
            plspec->scaleLabelInfo[plane][group].flags |= SCALE_LABEL_SCALEGIVEN;
            plspec->scaleLabelInfo[plane][group].scale = plspec->plot_request[index].label[plane].scale;
          }
          if (flags&LABEL_OFFSET_GIVEN) {
            plspec->scaleLabelInfo[plane][group].flags |= SCALE_LABEL_OFFSETGIVEN;
            plspec->scaleLabelInfo[plane][group].offset = plspec->plot_request[index].label[plane].offset;
          }
          if (flags&LABEL_THICKNESS_GIVEN) {
            plspec->scaleLabelInfo[plane][group].flags |= SCALE_LABEL_THICKNESSGIVEN;
            plspec->scaleLabelInfo[plane][group].thickness = plspec->plot_request[index].label[plane].thickness;
          }
          if (flags&LABEL_LINETYPE_GIVEN) {
            plspec->scaleLabelInfo[plane][group].flags |= SCALE_LABEL_LINETYPEGIVEN;
            plspec->scaleLabelInfo[plane][group].linetype = plspec->plot_request[index].label[plane].linetype;
          }
          break;
        }
      }
      if (plspec->scaleLabelInfo[plane][group].scaleNumber==-1)
        continue;

      if (!newLabel) {
        /* No explicit label was given or generated */
        if ((index=plspec->scalesGroupData[plane][group].tickSettingsRequestIndex)>=0 &&
            plspec->plot_request[index].tick_settings.flags&ticksetTime[plane]) {
          /* Use calendar time scales, per user request. */
          timeValue = plspec->scalesGroupData[plane][group].limit[0];
          if (!(newLabel = SDDS_Malloc(sizeof(*newLabel)*50)))
            SDDS_Bomb("Memory allocation failure (determine_panel_labels)");
          if (timeValue!=DBL_MAX)
            sprintf(newLabel, "Time starting %s", ctime(&timeValue));
          else
            sprintf(newLabel, "Undefined time values!");
          delete_chars(newLabel, "\n");
        } 
        else {
          /* Make a label using the names of the columns.  This is the default
           * if no commandline options are given. */
          if (!(newLabel=makeNameUnitsLabel(plspec, panel_index, plane, group))) 
            SDDS_Bomb("Unable to make label (determine_panel_labels)");
        }
      }
      if (editCommand) {
        /* Edit the label per user request. */
        strcpy_ss(buffer, newLabel);
        free(newLabel);
        edit_string(buffer, editCommand);
        if (plspec->scaleLabelInfo[plane][group].label)
          free(plspec->scaleLabelInfo[plane][group].label);
        if (!SDDS_CopyString(&plspec->scaleLabelInfo[plane][group].label, buffer))
          SDDS_Bomb("String copy problem (determine_panel_labels)");
      }
      else {
        /* Store the label for latter placement on plot. */
        if (plspec->scaleLabelInfo[plane][group].label)
          free(plspec->scaleLabelInfo[plane][group].label);
        plspec->scaleLabelInfo[plane][group].label = newLabel;
        newLabel = NULL;
      }
    }
  }
  

  /* This section creates labels for the title (0) and topline (1) */
  for (j=0; j<2; j++) {
    if (plspec->panel[panel_index].titleSpec[j].label)
      SDDS_CopyString(plspec->panel[panel_index].title+j, plspec->panel[panel_index].titleSpec[j].label);
    else {
      /* Stuff in some default labels... */
      if (j==0 &&
          plspec->plot_request[dataset[0]->request_index].description_text[dataset[0]->file_index]) 
        /* title from description text of first dataset, if there is any */
        SDDS_CopyString(plspec->panel[panel_index].title+j, 
                        plspec->plot_request[dataset[0]->request_index].
                        description_text[dataset[0]->file_index]);
      else
        SDDS_CopyString(plspec->panel[panel_index].title+j, "");
    }
    if ((editCommand=plspec->panel[panel_index].titleSpec[j].edit_command)) {
      strcpy_ss(buffer, plspec->panel[panel_index].title[j]);
      edit_string(buffer, editCommand);
      free(plspec->panel[panel_index].title[j]);
      SDDS_CopyString(plspec->panel[panel_index].title+j, buffer);
    }
  }

  if (plspec->panel[panel_index].flags&PLREQ_FNONTOPLINE) {
    /* User requested filenames on topline */
    char **filename;
    char buffer[1024];
    filename = tmalloc(sizeof(*filename)*plspec->panel[panel_index].datasets);
    for (i=0; i<plspec->panel[panel_index].datasets; i++) {
      request = plspec->panel[panel_index].dataset[i]->request_index;
      file = plspec->panel[panel_index].dataset[i]->file_index;
      filename[i] = plspec->plot_request[request].filename[file];
      if (plspec->plot_request[request].filenamesOnToplineEditCmd) {
        strcpy_ss(buffer, filename[i]);
        edit_string(buffer, plspec->plot_request[request].filenamesOnToplineEditCmd);
        SDDS_CopyString(&filename[i], buffer);
      }
    }
    qsort(filename, plspec->panel[panel_index].datasets, sizeof(*filename),
          string_cmpasc);
    i = unique(filename, plspec->panel[panel_index].datasets, sizeof(*filename),
               string_cmpasc, string_copy);
    if (plspec->panel[panel_index].title[1][0])
      sprintf(s, "%s  ", plspec->panel[panel_index].title[1]);
    else
      s[0] = 0;
    for (j=0; j<i; j++) {
      if ((strlen(s)+strlen(filename[j])+3)>1024)
        break;
      if (j)
        strcat(s, ", ");
      strcat(s, filename[j]);
    }
    SDDS_CopyString(plspec->panel[panel_index].title+1, s);
  }

  if (plspec->panel[panel_index].flags&PLREQ_YLONTOPLINE) {
    /* This is probably impossible with the multiple scales... */
  }
}

#define LABEL_BUFLEN 160
char *makeNameUnitsLabel(PLOT_SPEC *plspec, long panel, long plane, long group)
{
  long set, datasets;
  PLOT_DATA **dataset;
  char **symbolUsed, buffer[LABEL_BUFLEN], *ptr;
  long symbolsUsed, first, maxLabel, request, unitsDataSet, addUnits;
  static long warned[2] = { 0,0 } ;
  long labelLimit = LABEL_BUFLEN-10;
  
  dataset = plspec->panel[panel].dataset;
  datasets = plspec->panel[panel].datasets;
  symbolsUsed = maxLabel = 0;
  symbolUsed = NULL;
  first = addUnits = 1;
  buffer[0] = 0;
  unitsDataSet = -1;
  for (set=0; set<datasets; set++) {
    if (dataset[set]->scalesGroupIndex[plane]!=group)
      continue;
    if (unitsDataSet<0)
      unitsDataSet = set;
    request = dataset[set]->request_index;
    if (plspec->plot_request[request].overlay.flags)
      continue;
    if (!first && 
        ((!dataset[set]->info[plane].units && dataset[set-1]->info[plane].units) ||
           (dataset[set]->info[plane].units && !dataset[set-1]->info[plane].units) ||
           (dataset[set]->info[plane].units && dataset[set-1]->info[plane].units &&
            strcmp(dataset[set]->info[plane].units, dataset[set-1]->info[plane].units)!=0))) {
      if (!warned[plane]) {
        fprintf(stderr, "Warning: not all %ld quantities have the same units\n",
                'x'+plane);
        warned[plane] = 1;
      }
      addUnits = 0;
    }
    if (!dataset[set]->info[plane].symbol) {
      fprintf(stderr, "internal error: info[%ld] symbol is NULL for dataset %ld (panel %ld)\n",
              plane, set, panel);
      exit(1);
    }
    if (!maxLabel && 
        (first || !symbolsUsed || 
         match_string(dataset[set]->info[plane].symbol, symbolUsed, symbolsUsed, EXACT_MATCH)<0)) {
      if (!(symbolUsed=
            SDDS_Realloc(symbolUsed, sizeof(*symbolUsed)*(symbolsUsed+1)))) 
        SDDS_Bomb("Memory allocation failure (makeNameUnitsLabel)");
      symbolUsed[symbolsUsed] = dataset[set]->info[plane].symbol;
      if ((long)(strlen(buffer)+strlen(symbolUsed[symbolsUsed]))<labelLimit) {
        if (!first)
          strcat(buffer, ", ");
        strcat(buffer, symbolUsed[symbolsUsed]);
      }
      else {
        if (first)
          strncpy(buffer, symbolUsed[symbolsUsed], labelLimit);
        buffer[labelLimit] = 0;
        maxLabel = 1;
      }
      symbolsUsed++;
    }
    first = 0;
  }
  if (symbolUsed)
    free(symbolUsed);
  if (maxLabel)
    strcat(buffer, "...");
  if (addUnits && unitsDataSet>=0 &&
      dataset[unitsDataSet]->info[plane].units &&
      strlen(dataset[unitsDataSet]->info[plane].units)) {
    strcat(buffer, " (");
    strcat(buffer, dataset[unitsDataSet]->info[plane].units);
    strcat(buffer, ")");
  }
  if (strlen(buffer)) {
    if (!SDDS_CopyString(&ptr, buffer))
      SDDS_Bomb("String copying problem (makeNameUnitsLabel)");
    return ptr;
  }
  return NULL;
}

void determine_dataset_labels(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset)
{
  PLOT_REQUEST *request;
  long i;
  DATA_INFO *info;
  char printBuffer[SDDS_MAXLINE];
  request = plspec->plot_request+dataset->request_index;
  for (i=0; i<4; i++) {
    dataset->label[i] = NULL;
    if (request->label[i].flags&LABEL_STRING_GIVEN) {
      if (!SDDS_CopyString(&dataset->label[i], request->label[i].label))
        SDDS_Bomb("String copy error (determine_dataset_labels)");
    }      
    else if (request->label[i].flags&LABEL_PARAMETER_GIVEN) {
      long type;
      char *dataBuffer = NULL;
      char *format =NULL;
      
      if ( !(dataBuffer = malloc(sizeof(double)*4)))
          SDDS_Bomb("Allocation failure in determin_dataset_labels");
      if (!SDDS_GetParameter(table,request->label[i].label, dataBuffer))
         SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);  
      if(!(type=SDDS_GetParameterType(table,SDDS_GetParameterIndex(table,request->label[i].label))))
	     SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);	 
      if(SDDS_GetParameterInformation(table,"format_string",&format,SDDS_GET_BY_NAME,request->label[i].label) !=SDDS_STRING)
         SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);  	  
      
	 	 
      if ( request->label[i].flags&LABEL_FORMAT_GIVEN) {
          if(!SDDS_VerifyPrintfFormat(request->label[i].format,type)) {
	     fprintf(stderr, "error: given format (\"%s\") for parameter %s is invalid\n",
                        request->label[i].format, request->label[i].label);
             exit(1);
	  }   
	  
	  
	  if(!(SDDS_SprintTypedValue((void*)dataBuffer,0,type,request->label[i].format,printBuffer,SDDS_PRINT_NOQUOTES)))
	      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	  SDDS_CopyString(&dataset->label[i],printBuffer);
      }  else {
         if(!SDDS_StringIsBlank(format)) {
	      if(!(SDDS_SprintTypedValue((void*)dataBuffer,0,type,format,printBuffer,SDDS_PRINT_NOQUOTES)))
	          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	     SDDS_CopyString(&dataset->label[i],printBuffer);  
	 } else {	  
              if (!(dataset->label[i]=SDDS_GetParameterAsString(table,request->label[i].label,NULL))) 
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
	 }
      }	 	  
      free(format);
      free(dataBuffer);
      
      /*if (SDDS_GetParameterType(table, SDDS_GetParameterIndex(table, request->label[i].label)) != SDDS_STRING) {
	fprintf(stderr, "labels taken from parameters must be strings\n");
	exit(1);
      }
      if (!SDDS_GetParameter(table, request->label[i].label, &s))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      dataset->label[i] = s; */
    }
    else if (request->label[i].flags&(LABEL_USE_NAME+LABEL_USE_SYMBOL+LABEL_USE_DESCRIPTION)) {
      if (i==0)
        info = &dataset->info[0];
      else
        info = &dataset->info[1];
      if (request->label[i].flags&LABEL_USE_NAME)
        SDDS_CopyString(&dataset->label[i],
                        i==0?request->xname[dataset->dataname_index]:request->yname[dataset->dataname_index]);
      else if (request->label[i].flags&LABEL_USE_SYMBOL && info->symbol)
        SDDS_CopyString(&dataset->label[i], info->symbol);
      else if (request->label[i].flags&LABEL_USE_DESCRIPTION && info->description)
        SDDS_CopyString(&dataset->label[i], info->description);
      if (dataset->label[i] && request->label[i].flags&LABEL_INCLUDE_UNITS && info->units &&
          !SDDS_StringIsBlank(info->units)) {
        if (!(dataset->label[i] = SDDS_Realloc(dataset->label[i], 
                                               sizeof(*dataset->label[i])*(strlen(dataset->label[i])+strlen(info->units)+4))))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        strcat(dataset->label[i], " (");
        strcat(dataset->label[i], info->units);
        strcat(dataset->label[i], ")");
      }
    }
  }
}

