/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sddsplot_AP.c
 * 
 * argument parsing for sddsplot
 *
 * Michael Borland, 1994.
 */
#include "mdb.h"
#include "scan.h"
#include "graph.h"
#include "table.h"
#include "SDDS.h"
#include "sddsplot.h"
#include <ctype.h>
#include <string.h>
#if defined(_WIN32)
#  include <windows.h>
#else
#  include <unistd.h>
#endif
#include "hersheyfont.h"

static char *NO_REQUESTS_MESSAGE = "no plot requests";
long defaultLineThickness = 0;

extern int savedCommandlineArgc;
extern char **savedCommandlineArgv;

static const char *consumeOptionValue(int argc, char **argv, int *index, const char *option) {
  size_t optionLen;
  if (!argv || !option || !index)
    return NULL;
  optionLen = strlen(option);
  if (!strcmp(argv[*index], option)) {
    if (*index + 1 >= argc)
      return NULL;
    (*index)++;
    return argv[*index];
  }
  if (optionLen && !strncmp(argv[*index], option, optionLen) && argv[*index][optionLen] == '=')
    return argv[*index] + optionLen + 1;
  return NULL;
}

static void appendQuotedOption(char *command, size_t commandSize, const char *option, const char *value) {
  size_t len, remaining, extra = 0;
  char *escaped, *dst;
  const char *src;
  int written;
  if (!command || !option || !value || !commandSize)
    return;
  len = strlen(command);
  if (len >= commandSize - 1)
    return;
  for (src = value; *src; src++)
    if (*src == '"' || *src == '\\')
      extra++;
  escaped = tmalloc(strlen(value) + extra + 1);
  dst = escaped;
  for (src = value; *src; src++) {
    if (*src == '"' || *src == '\\')
      *dst++ = '\\';
    *dst++ = *src;
  }
  *dst = '\0';
  remaining = commandSize - len;
  written = snprintf(command + len, remaining, " %s \"%s\"", option, escaped);
  free(escaped);
  if (written < 0 || (size_t)written >= remaining)
    command[commandSize - 1] = '\0';
}

static int matchesEqualAspectOption(const char *arg) {
  size_t len;
  if (!arg)
    return 0;
  len = strlen("-equalaspect");
  if (!strncmp(arg, "-equalaspect", len))
    return arg[len] == '\0' || arg[len] == '=';
  len = strlen("-equalAspect");
  if (!strncmp(arg, "-equalAspect", len))
    return arg[len] == '\0' || arg[len] == '=';
  return 0;
}

long graphic_AP1(GRAPHIC_SPEC *graphic_spec, long element, char **item, long items);
long plotnames_AP1(PLOT_SPEC *plotspec, char **item, long items, char *plotnames_usage, long class);
void add_plot_request(PLOT_SPEC *plspec);
long plotlabel_AP(PLOT_SPEC *plotspec, long label_index, char **item, long items);
long plotExclude_AP(PLOT_SPEC *plotspec, long exclude_type, char **item, long items);
long keepnames_AP1(PLOT_SPEC *plotspec);
long keepfilenames_AP1(PLOT_SPEC *plotspec);
long add_filename(PLOT_SPEC *plotspec, char *filename);
long count_chars(char *string, char c);
void SetupFontSize(FONT_SIZE *fs);

static int extractTicksettingsOption(int argc, char **argv, int *index,
                                     const char **value) {
  const char *arg;
  const char *valueStart = NULL;
  if (!argv || !index || *index < 0 || *index >= argc)
    return 0;
  arg = argv[*index];
  if (!arg)
    return 0;
  if (!strncmp(arg, "-ticksettings", 13))
    valueStart = arg + 13;
  else if (!strncmp(arg, "-tick", 5) &&
           (arg[5] == '\0' || arg[5] == '='))
    valueStart = arg + 5;
  else
    return 0;

  if (valueStart && *valueStart == '\0') {
    if (*index + 1 < argc)
      valueStart = argv[++(*index)];
    else
      valueStart = NULL;
  } else if (valueStart && *valueStart == '=')
    valueStart++;

  if (value)
    *value = (valueStart && *valueStart) ? valueStart : NULL;
  return 1;
}

static int handle3DScatter(int argc, char **argv, const TICK_SETTINGS *tickSettings)
{
  int i;
  char *spec = NULL;
  for (i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-3d=", 4)) {
      spec = argv[i] + 4;
      break;
    }
  }
  if (!spec)
    return 0;
  char *copy = strdup(spec);
  char *type = strtok(copy, ",");
  char *xname = strtok(NULL, ",");
  char *yname = strtok(NULL, ",");
  char *zname = strtok(NULL, ",");
  char *iname = strtok(NULL, ",");
  if (!type || !xname || !yname || !zname)
    SDDS_Bomb("invalid -3d specification");
  long mode = 0;
  if (!strcmp(type, "column"))
    mode = 1;
  else if (!strcmp(type, "array"))
    mode = 2;
  else
    SDDS_Bomb("invalid -3d mode");
  char *filename = NULL;
  for (i = 1; i < argc; i++) {
    if (argv[i][0] != '-')
      filename = argv[i];
  }
  if (!filename)
    SDDS_Bomb("no input file given for -3d");
  SDDS_DATASET SDDSin;
  if (!SDDS_InitializeInput(&SDDSin, filename))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_ReadPage(&SDDSin) <= 0)
    SDDS_Bomb("unable to read page for -3d plot");
  double *xData = NULL, *yData = NULL, *zData = NULL, *iData = NULL;
  SDDS_ARRAY *xArr = NULL, *yArr = NULL, *zArr = NULL, *iArr = NULL;
  long n = 0;
  char *xUnits = NULL, *yUnits = NULL, *zUnits = NULL;
  char xLabel[256], yLabel[256], zLabel[256];
  if (mode == 1) {
    xData = SDDS_GetColumnInDoubles(&SDDSin, xname);
    yData = SDDS_GetColumnInDoubles(&SDDSin, yname);
    zData = SDDS_GetColumnInDoubles(&SDDSin, zname);
    if (!xData || !yData || !zData)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    n = SDDS_CountRowsOfInterest(&SDDSin);
    SDDS_GetColumnInformation(&SDDSin, "units", &xUnits, SDDS_GET_BY_NAME, xname);
    SDDS_GetColumnInformation(&SDDSin, "units", &yUnits, SDDS_GET_BY_NAME, yname);
    SDDS_GetColumnInformation(&SDDSin, "units", &zUnits, SDDS_GET_BY_NAME, zname);
    if (iname) {
      iData = SDDS_GetColumnInDoubles(&SDDSin, iname);
      if (!iData)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  } else {
    xArr = SDDS_GetArray(&SDDSin, xname, NULL);
    yArr = SDDS_GetArray(&SDDSin, yname, NULL);
    zArr = SDDS_GetArray(&SDDSin, zname, NULL);
    if (!xArr || !yArr || !zArr)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    n = xArr->elements;
    xData = (double *)xArr->data;
    yData = (double *)yArr->data;
    zData = (double *)zArr->data;
    SDDS_GetArrayInformation(&SDDSin, "units", &xUnits, SDDS_GET_BY_NAME, xname);
    SDDS_GetArrayInformation(&SDDSin, "units", &yUnits, SDDS_GET_BY_NAME, yname);
    SDDS_GetArrayInformation(&SDDSin, "units", &zUnits, SDDS_GET_BY_NAME, zname);
    if (iname) {
      iArr = SDDS_GetArray(&SDDSin, iname, NULL);
      if (!iArr)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      iData = (double *)iArr->data;
    }
  }
  if (xUnits && xUnits[0])
    snprintf(xLabel, sizeof(xLabel), "%s (%s)", xname, xUnits);
  else
    strncpy(xLabel, xname, sizeof(xLabel) - 1);
  xLabel[sizeof(xLabel) - 1] = '\0';
  if (yUnits && yUnits[0])
    snprintf(yLabel, sizeof(yLabel), "%s (%s)", yname, yUnits);
  else
    strncpy(yLabel, yname, sizeof(yLabel) - 1);
  yLabel[sizeof(yLabel) - 1] = '\0';
  if (zUnits && zUnits[0])
    snprintf(zLabel, sizeof(zLabel), "%s (%s)", zname, zUnits);
  else
    strncpy(zLabel, zname, sizeof(zLabel) - 1);
  zLabel[sizeof(zLabel) - 1] = '\0';
  free(xUnits);
  free(yUnits);
  free(zUnits);
#if defined(_WIN32)
  char tmpName[L_tmpnam];
  if (!tmpnam(tmpName))
    SDDS_Bomb("unable to create temporary file for 3D plot");
  FILE *fp = fopen(tmpName, "w");
  if (!fp)
    SDDS_Bomb("unable to open temporary file for 3D plot");
#else
  char tmpName[] = "sddsplot3dXXXXXX";
  int fd = mkstemp(tmpName);
  FILE *fp = NULL;
  if (fd == -1 || !(fp = fdopen(fd, "w")))
    SDDS_Bomb("unable to create temporary file for 3D plot");
#endif
  fprintf(fp, "%ld\n", n);
  for (long j = 0; j < n; j++) {
    if (iData)
      fprintf(fp, "%g %g %g %g\n", xData[j], yData[j], zData[j], iData[j]);
    else
      fprintf(fp, "%g %g %g\n", xData[j], yData[j], zData[j]);
  }
  fclose(fp);
  if (mode == 1) {
    free(xData);
    free(yData);
    free(zData);
    if (iData)
      free(iData);
  } else {
    SDDS_FreeArray(xArr);
    SDDS_FreeArray(yArr);
    SDDS_FreeArray(zArr);
    if (iArr)
      SDDS_FreeArray(iArr);
  }
  SDDS_Terminate(&SDDSin);
  char command[4096];
  snprintf(command, sizeof(command), "mpl_qt -3d=scatter %s", tmpName);
  int hasXLabel = 0, hasYLabel = 0, hasZLabel = 0;
  int hasTicksettingsArg = 0;
  for (i = 1; i < argc; i++) {
    const char *value;
    value = consumeOptionValue(argc, argv, &i, "-xlabel");
    if (value) {
      appendQuotedOption(command, sizeof(command), "-xlabel", value);
      hasXLabel = 1;
      continue;
    }
    value = consumeOptionValue(argc, argv, &i, "-ylabel");
    if (value) {
      appendQuotedOption(command, sizeof(command), "-ylabel", value);
      hasYLabel = 1;
      continue;
    }
    value = consumeOptionValue(argc, argv, &i, "-zlabel");
    if (value) {
      appendQuotedOption(command, sizeof(command), "-zlabel", value);
      hasZLabel = 1;
      continue;
    }
    value = consumeOptionValue(argc, argv, &i, "-title");
    if (value) {
      appendQuotedOption(command, sizeof(command), "-plottitle", value);
      continue;
    }
    value = consumeOptionValue(argc, argv, &i, "-topline");
    if (value) {
      appendQuotedOption(command, sizeof(command), "-topline", value);
      continue;
    }
    if (!strcmp(argv[i], "-fontsize") && i + 1 < argc) {
      strcat(command, " -fontsize ");
      strcat(command, argv[++i]);
    } else if (matchesEqualAspectOption(argv[i]))
      strcat(command, " -equalaspect");
    else if (!strcmp(argv[i], "-yflip"))
      strcat(command, " -yflip");
    else if (!strcmp(argv[i], "-noborder"))
      strcat(command, " -noborder");
    else if (!strcmp(argv[i], "-noscale"))
      strcat(command, " -noscale");
    else if (!strncmp(argv[i], "-noscales", 9)) {
      if (argv[i][9] == '=') {
        strcat(command, " ");
        strcat(command, argv[i]);
      } else {
        const char *value = NULL;
        if (!argv[i][9] && i + 1 < argc && argv[i + 1][0] != '-')
          value = argv[++i];
        if (value && value[0])
          appendQuotedOption(command, sizeof(command), "-noscales", value);
        else
          strcat(command, " -noscales");
      }
    } else if (!strcmp(argv[i], "-datestamp"))
      strcat(command, " -datestamp");
    else if (!strcmp(argv[i], "-xlog"))
      strcat(command, " -xlog");
    else if (extractTicksettingsOption(argc, argv, &i, &value)) {
      size_t len;
      hasTicksettingsArg = 1;
      len = strlen(command);
      if (value && *value)
        snprintf(command + len, sizeof(command) - len,
                 " -ticksettings=%s", value);
      else
        snprintf(command + len, sizeof(command) - len, " -ticksettings");
    } else if (!strcmp(argv[i], "-shade") && i + 1 < argc) {
      strcat(command, " -shade ");
      strcat(command, argv[++i]);
      while (i + 1 < argc && argv[i + 1][0] != '-') {
        strcat(command, " ");
        strcat(command, argv[++i]);
      }
    } else if (!strcmp(argv[i], "-mapshade") && i + 2 < argc) {
      strcat(command, " -mapshade ");
      strcat(command, argv[++i]);
      strcat(command, " ");
      strcat(command, argv[++i]);
    }
  }
  if (!hasXLabel) {
    appendQuotedOption(command, sizeof(command), "-xlabel", xLabel);
  }
  if (!hasYLabel) {
    appendQuotedOption(command, sizeof(command), "-ylabel", yLabel);
  }
  if (!hasZLabel) {
    appendQuotedOption(command, sizeof(command), "-zlabel", zLabel);
  }
  if (!hasTicksettingsArg && tickSettings &&
      (tickSettings->flags & (TICKSET_XTIME | TICKSET_YTIME))) {
    const int wantX = (tickSettings->flags & TICKSET_XTIME) != 0;
    const int wantY = (tickSettings->flags & TICKSET_YTIME) != 0;
    size_t len = strlen(command);
    if (len < sizeof(command) - 1) {
      int written = snprintf(command + len, sizeof(command) - len,
                             " -ticksettings=%s%s%s",
                             wantX ? "xtime" : "",
                             (wantX && wantY) ? "," : "",
                             wantY ? "ytime" : "");
      if (written < 0 || (size_t)written >= sizeof(command) - len)
        command[sizeof(command) - 1] = '\0';
    }
  }
  char wrapper[8192];
#if defined(_WIN32)
  snprintf(wrapper, sizeof(wrapper),
           "start /B cmd /c \"%s && del \\\"%s\\\"\"",
           command, tmpName);
#else
  snprintf(wrapper, sizeof(wrapper),
           "(%s; rm %s) &",
           command, tmpName);
#endif
  int sysret = system(wrapper);
  (void)sysret;
  free(copy);
  return 1;
}

long threeD_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  TICK_SETTINGS *tickSettings = NULL;
  if (plotspec && plotspec->plot_requests > 0)
    tickSettings = &plotspec->plot_request[plotspec->plot_requests - 1].tick_settings;
  if (handle3DScatter(savedCommandlineArgc, savedCommandlineArgv, tickSettings))
    exit(0);
  return 1;
}

long convertUnits_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  if ((items != 4) && (items != 5)) {
    return bombre("invalid -convertunits syntax", "-convertunits={column|parameter},<name>,<new-units-name>,<old-units-name>[,<factor>]", 0);
  }

  plotspec->conversion = (CONVERSION_DEFINITION**)SDDS_Realloc(plotspec->conversion, 
                                                               sizeof(CONVERSION_DEFINITION*)*(plotspec->conversions+1));
  plotspec->conversion[plotspec->conversions] = tmalloc(sizeof(CONVERSION_DEFINITION));

  plotspec->conversion[plotspec->conversions]->is_array=0;
  plotspec->conversion[plotspec->conversions]->is_column=0;
  plotspec->conversion[plotspec->conversions]->is_parameter=0;
  switch (match_string(item[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case ARRAY_BASED:
    plotspec->conversion[plotspec->conversions]->is_array=1;
    break;
  case COLUMN_BASED:
    plotspec->conversion[plotspec->conversions]->is_column=1;
    break;
  case PARAMETER_BASED:
    plotspec->conversion[plotspec->conversions]->is_parameter=1;
    break;
  default:
    return bombre("invalid -convertunits syntax", "-convertunits={column|parameter},<name>,<new-units-name>,<old-units-name>[,<factor>]", 0);
  }

  plotspec->conversion[plotspec->conversions]->name = item[1];
  plotspec->conversion[plotspec->conversions]->new_units = item[2];
  plotspec->conversion[plotspec->conversions]->old_units = item[3];
  if (items == 5) {
    if (sscanf(item[4], "%lf", &plotspec->conversion[plotspec->conversions]->factor)!=1) {
      return bombre("invalid -convertunits syntax", "-convertunits={column|parameter},<name>,<new-units-name>,<old-units-name>[,<factor>]", 0);
    }
  } else {
    plotspec->conversion[plotspec->conversions]->factor = 1;
  }

  plotspec->conversions ++;
  return 1;
}

long thickness_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (items!=1 ||
        sscanf(item[0], "%ld", 
	       &(plotspec->plot_request[plotspec->plot_requests-1].global_thickness_default))!=1 ||
        plotspec->plot_request[plotspec->plot_requests-1].global_thickness_default<=0)
        return bombre("invalid -thickness syntax", "-thickness=<integer>", 0);
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_THICKNESS;
    return 1;
}

long aspectratio_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (items!=1 ||
        sscanf(item[0], "%lf", &(plotspec->plot_request[plotspec->plot_requests-1].aspect_ratio))!=1 ||
        plotspec->plot_request[plotspec->plot_requests-1].aspect_ratio==0)
        return bombre("invalid -aspectratio syntax", "-aspectratio=<value>", 0);
    plotspec->plot_request[plotspec->plot_requests-1].aspect_ratio =
      fabs(plotspec->plot_request[plotspec->plot_requests-1].aspect_ratio);
    return 1;
    }

long rowlimit_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  long rowLimit;
  if (items!=1 || sscanf(item[0], "%ld", &rowLimit)!=1 || rowLimit<=0)
    return bombre("invalid -rowlimit value", "-rowlimit=<integer>", 0);
  SDDS_SetRowLimit(rowLimit);
  return 1;
}

long device_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (items<1)
        return bombre("invalid -device syntax", "-device=<name>[,<device-arguments>]", 0);
    SDDS_CopyString(&plotspec->device, item[0]);
    plotspec->deviceArgc = items-1;
    if (items>1) {
      if (!(plotspec->deviceArgv=(char**)malloc(sizeof(*plotspec->deviceArgv)*(items-1))) ||
          !SDDS_CopyStringArray(plotspec->deviceArgv, item+1, items-1)) {
        SDDS_Bomb("error copying device arguments");
      }
    } else
        plotspec->deviceArgv = NULL;
    return 1;
    }

#define SET_LINE_GRAPHIC 0
#define SET_SYMBOL_GRAPHIC 1
#define SET_ERRORBAR_GRAPHIC 2
#define SET_DOT_GRAPHIC 3
#define SET_IMPULSE_GRAPHIC 4
#define SET_CONTINUE_GRAPHIC 5
#define SET_BAR_GRAPHIC 6
#define SET_YIMPULSE_GRAPHIC 7
#define SET_YBAR_GRAPHIC 8
#define GRAPHIC_ELEMENTS 9
static char *graphic_element[GRAPHIC_ELEMENTS] = {
    "line", "symbol", "errorbar", "dot", "impulse", "continue", "bar",
    "yimpulse", "ybar",
    } ;

unsigned long translate_to_plotcode(GRAPHIC_SPEC graphic)
{
    unsigned long plotcode = 0;
    if (graphic.element==-1)
        bomb("graphic.element==-1 in translate_to_plotcode.  This shouldn't happen.", NULL);
    plotcode = graphic.element;
    if (graphic.connect_linetype!=-1)
        plotcode = PLOT_CSYMBOL;
    plotcode += PLOT_SIZE_MASK&(((long)graphic.scale)<<4);
    plotcode += PLOT_CODE_MASK&graphic.type;
    return(plotcode);
    }

static char *graphic_usage = "-graphic=<element>[,type=<type|@column>][,fill][,subtype={<type> | type | @column}][,thickness=<integer>][,connect[={<linetype> | type | subtype}]][,vary=type][,vary=subtype][,scale=<factor>][,modulus=<integer>][,eachfile][,eachpage][,eachrequest][,fixForName][,fixForFile][,fixForRequest]\n\
<element> is one of continue, line, symbol, errorbar, impulse, yimpulse, dot, bar, or ybar.\n";

long graphic_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    if (items<1) {
        fprintf(stderr, "error: invalid -graphic syntax\nusage: %s\n", graphic_usage);
        return 0;
        }
    
    switch (match_string(item[0], graphic_element, GRAPHIC_ELEMENTS, 0)) {
      case SET_CONTINUE_GRAPHIC:
        if (plotspec->plot_requests<2)
            return bombre("can't use -graphic=continue for first plot request", NULL, 0);
        if (items!=1)
            return bombre("invalid -graphic=continue syntax--no other keywords allowed", NULL, 0);
        plreq->graphic.element = -1;
        break;
      case SET_LINE_GRAPHIC:
        return graphic_AP1(&plreq->graphic, PLOT_LINE, item+1, items-1);
      case SET_SYMBOL_GRAPHIC:
        return graphic_AP1(&plreq->graphic, PLOT_SYMBOL, item+1, items-1);
      case SET_ERRORBAR_GRAPHIC:
        return graphic_AP1(&plreq->graphic, PLOT_ERRORBAR, item+1, items-1);
      case SET_DOT_GRAPHIC:
        if (!graphic_AP1(&plreq->graphic, PLOT_DOTS, item+1, items-1))
            return 0;
        if (plreq->graphic.connect_linetype!=-1)
            return bombre("can't connect dots with a line--the dots won't be visible!", NULL, 0);
        break;
      case SET_IMPULSE_GRAPHIC:
        return graphic_AP1(&plreq->graphic, PLOT_IMPULSE, item+1, items-1);
      case SET_YIMPULSE_GRAPHIC:
        return graphic_AP1(&plreq->graphic, PLOT_YIMPULSE, item+1, items-1);
      case SET_BAR_GRAPHIC:
        return graphic_AP1(&plreq->graphic, PLOT_BAR, item+1, items-1);
      case SET_YBAR_GRAPHIC:
        return graphic_AP1(&plreq->graphic, PLOT_YBAR, item+1, items-1);
      default:
        return bombre("invalid graphic element name", graphic_usage, 0);
        }
    return 1;
    }

#define GRAPHIC_KW_TYPE 0
#define GRAPHIC_KW_SCALE 1
#define GRAPHIC_KW_CONNECT 2
#define GRAPHIC_KW_VARY 3
#define GRAPHIC_KW_EACHPAGE 4
#define GRAPHIC_KW_EACHFILE 5
#define GRAPHIC_KW_EACHREQUEST 6
#define GRAPHIC_KW_SUBTYPE 7
#define GRAPHIC_KW_MODULUS 8
#define GRAPHIC_KW_FIXFORNAME 9
#define GRAPHIC_KW_THICKNESS 10
#define GRAPHIC_KW_FILL 11
#define GRAPHIC_KW_FIXFORFILE 12
#define GRAPHIC_KW_FIXFORREQUEST 13
#define GRAPHIC_KWS 14
static char *graphic_kw[GRAPHIC_KWS] = {
    "type", "scale", "connect", "vary", "eachpage", "eachfile", "eachrequest", "subtype", "modulus",
    "fixforname", "thickness","fill", "fixforfile", "fixforrequest",
    } ;

#define CONNECT_KW_SUBTYPE 0
#define CONNECT_KW_TYPE 1
#define CONNECT_KWS 2
static char *connect_kw[CONNECT_KWS] = {
    "subtype", "type" 
        };

long graphic_AP1(GRAPHIC_SPEC *graphic_spec, long element, char **item, long items)
/* second stage processing of -graphic options for everything by arrow elements */
{
    long i;
    char *eqptr;
    graphic_spec->element = element;
    graphic_spec->type = 0;
    graphic_spec->scale = 1;
    graphic_spec->connect_linetype = -1;
    graphic_spec->vary = 0;
    graphic_spec->flags = 0;
    graphic_spec->subtype = 0;
    graphic_spec->thickness = defaultLineThickness;
    graphic_spec->fill = 0;
    graphic_spec->type_column = NULL;
    graphic_spec->subtype_column = NULL;
    for (i=0; i<items; i++) {
        if ((eqptr=strchr(item[i], '=')))
            *eqptr = 0;
        switch (match_string(item[i], graphic_kw, GRAPHIC_KWS, 0)) {
          case GRAPHIC_KW_TYPE:
        if (!eqptr || SDDS_StringIsBlank(eqptr+1))
          return bombre("invalid type specification for -graphic", graphic_usage, 0);
        if (eqptr[1] == '@') {
          if (SDDS_StringIsBlank(eqptr+2))
            return bombre("invalid type specification for -graphic", graphic_usage, 0);
          if (!SDDS_CopyString(&graphic_spec->type_column, eqptr+2))
            SDDS_Bomb("memory allocation failure (graphic_AP1)");
        } else if (sscanf(eqptr+1, "%ld", &graphic_spec->type)!=1 || graphic_spec->type<0) {
          return bombre("invalid type specification for -graphic", graphic_usage, 0);
        }
            break;
          case GRAPHIC_KW_SUBTYPE:
            if (!eqptr || SDDS_StringIsBlank(eqptr+1))
                return bombre("invalid subtype specification for -graphic", graphic_usage, 0);
        if (eqptr[1] == '@') {
          if (SDDS_StringIsBlank(eqptr+2))
            return bombre("invalid subtype specification for -graphic", graphic_usage, 0);
          if (!SDDS_CopyString(&graphic_spec->subtype_column, eqptr+2))
            SDDS_Bomb("memory allocation failure (graphic_AP1)");
        } else if (sscanf(eqptr+1, "%ld", &graphic_spec->subtype)!=1) {
          if (strncmp(eqptr+1, "type", strlen(eqptr+1))!=0)
            return bombre("invalid subtype specification for -graphic", graphic_usage, 0);
          graphic_spec->flags |= GRAPHIC_SUBTYPE_EQ_TYPE;
          }
        else if (graphic_spec->subtype<0)
          return bombre("invalid subtype specification for -graphic", graphic_usage, 0);
            break;
          case GRAPHIC_KW_THICKNESS:
            if (!eqptr || SDDS_StringIsBlank(eqptr+1) || sscanf(eqptr+1, "%ld", &graphic_spec->thickness)!=1)
	      return bombre("invalid thickness specification for -graphic", graphic_usage, 0);
	    if (graphic_spec->thickness<=0)
	      graphic_spec->thickness = 1;
	    if (graphic_spec->thickness>9)
	      graphic_spec->thickness = 9;
            break;
          case GRAPHIC_KW_SCALE:
            if (!eqptr || SDDS_StringIsBlank(eqptr+1) || sscanf(eqptr+1, "%lf", &graphic_spec->scale)!=1 ||
                graphic_spec->scale<=0)
                return bombre("invalid scale specification for -graphic", graphic_usage, 0);
            break;
          case GRAPHIC_KW_CONNECT:
            if (eqptr) {
                if (SDDS_StringIsBlank(eqptr+1))
                    return bombre("invalid connect linetype for -graphic", graphic_usage, 0);
                if (sscanf(eqptr+1, "%ld", &graphic_spec->connect_linetype)!=1) {
                    switch (match_string(eqptr+1, connect_kw, CONNECT_KWS, 0)) {
                      case CONNECT_KW_SUBTYPE:
                        graphic_spec->flags |= GRAPHIC_CONNECT_EQ_SUBTYPE;
                        break;
                      case CONNECT_KW_TYPE:
                        graphic_spec->flags |= GRAPHIC_CONNECT_EQ_TYPE;
                        break;
                      default:
                        return bombre("invalid connect value for -graphic", graphic_usage, 0);
                        }
                    }
                else if (graphic_spec->connect_linetype<0)
                    return bombre("invalid connect linetype for -graphic", graphic_usage, 0);
                }
            else
                graphic_spec->connect_linetype = 0;
            graphic_spec->flags |= GRAPHIC_CONNECT;
            break;
          case GRAPHIC_KW_VARY:
            if (eqptr) {
                long code;
                char *varyChoice[2] = {"type", "subtype"};
                if (SDDS_StringIsBlank(eqptr+1) || 
                    (code=match_string(eqptr+1, varyChoice, 2, 0))<0)
                    return bombre("invalid -vary syntax", graphic_usage, 0);
                switch (code) {
                  case 0: graphic_spec->flags |= GRAPHIC_VARY_TYPE; break;
                  case 1: graphic_spec->flags |= GRAPHIC_VARY_SUBTYPE; break;
                    }
                }
            else
                graphic_spec->flags |= GRAPHIC_VARY_TYPE;
            graphic_spec->vary = 1;
            break;
          case GRAPHIC_KW_EACHPAGE:
            graphic_spec->flags |= GRAPHIC_VARY_EACHPAGE;
          case GRAPHIC_KW_EACHFILE:
            graphic_spec->flags |= GRAPHIC_VARY_EACHFILE;
            break;
          case GRAPHIC_KW_EACHREQUEST:
            graphic_spec->flags |= GRAPHIC_VARY_EACHREQUEST;
            break;
          case GRAPHIC_KW_FIXFORNAME:
            graphic_spec->flags |= GRAPHIC_VARY_FIXFORNAME;
            break;
          case GRAPHIC_KW_FIXFORFILE:
            graphic_spec->flags |= GRAPHIC_VARY_FIXFORFILE;
            break;
          case GRAPHIC_KW_FIXFORREQUEST:
            graphic_spec->flags |= GRAPHIC_VARY_FIXFORREQUEST;
            break;
          case GRAPHIC_KW_MODULUS:
            if (!eqptr || SDDS_StringIsBlank(eqptr+1) || sscanf(eqptr+1, "%ld", &graphic_spec->modulus)!=1 ||
                graphic_spec->modulus<=0)
                return bombre("invalid modulus specification for -graphic", graphic_usage, 0);
            break;
	  case GRAPHIC_KW_FILL:
	    graphic_spec->fill = 1;
	    break;  
	  default:
            return bombre("invalid keyword for -graphic", graphic_usage, 0);
	}
    }
    if (graphic_spec->flags&GRAPHIC_VARY_SUBTYPE &&
        graphic_spec->flags&GRAPHIC_SUBTYPE_EQ_TYPE)
      return bombre("can't vary subtype and equate it to type in -graphic", NULL, 0);
    if (graphic_spec->type_column && (graphic_spec->flags & GRAPHIC_VARY_TYPE))
      return bombre("can't vary type when using a type column in -graphic", NULL, 0);
    if (graphic_spec->subtype_column && (graphic_spec->flags & GRAPHIC_VARY_SUBTYPE))
      return bombre("can't vary subtype when using a subtype column in -graphic", NULL, 0);
    if (graphic_spec->subtype_column && (graphic_spec->flags & GRAPHIC_SUBTYPE_EQ_TYPE))
      return bombre("can't equate subtype to type when using a subtype column in -graphic", NULL, 0);
    return 1;
}

long lspace_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];

    SDDS_ZeroMemory((void*)plreq->lspace, sizeof(*plreq->lspace)*4);
    if (items<4 ||
        sscanf(item[0], "%lf", &plreq->lspace[0])!=1 ||
        sscanf(item[1], "%lf", &plreq->lspace[1])!=1 ||
        sscanf(item[2], "%lf", &plreq->lspace[2])!=1 ||
        sscanf(item[3], "%lf", &plreq->lspace[3])!=1 ||
        plreq->lspace[0]>plreq->lspace[1] || plreq->lspace[2]>plreq->lspace[3])
        return bombre("invalid -lspace syntax", "-lspace=<qmin>,<qmax>,<pmin>,<pmax>", 0);
    plreq->flags |= PLREQ_LSPACE_GIVEN;
    return 1;
    }

long mplfiles_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    add_plot_request(plotspec);
    if (!scanItemList(&plotspec->plot_request[plotspec->plot_requests-1].mplflags, item, &items, 0,
                        "notitle", -1, NULL, 0, MPLFILE_NOTITLE,
                        "notopline", -1, NULL, 0, MPLFILE_NOTOPLINE,
                        NULL))
        return bombre("invalid -mplfiles syntax", "-mplfiles[=[notitle][,notopline]] <filename> ...", 0);
    plotspec->plot_request[plotspec->plot_requests-1].mplflags |= MPLFILE;
    return 1;
    }

long outputfile_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  if (items!=1)
    return bombre("invalid -output syntax", "-output=<filename>", 0);
  SDDS_CopyString(&plotspec->outputfile, item[0]);
    return 1;
    }

long columnnames_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  if (strcmp_case_insensitive(item[0], "json")==0)
    plotspec->outputMode = PLOT_OUTPUT_JSON;
  static char *columnnames_usage = 
    "-columnnames=<xname>,<yname-list>[,{<y1name-list> | <x1name>,<y1name-list>}]";
  return plotnames_AP1(plotspec, item, items, columnnames_usage, COLUMN_DATA);
}

long toPage_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  PLOT_REQUEST *plreq;
  plreq = plotspec->plot_request+plotspec->plot_requests-1;
  if (items!=1)
    return bombre("invalid -toPage syntax",NULL,0);
  if (sscanf(item[0], "%ld", &plreq->topage)!=1)
    return bombre("invalid -toPage syntax",NULL,0);
  return 1;
}

long fromPage_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  PLOT_REQUEST *plreq;
  plreq = plotspec->plot_request+plotspec->plot_requests-1;
  if (items!=1)
    return bombre("invalid -toPage syntax",NULL,0);
  if (sscanf(item[0], "%ld", &plreq->frompage)!=1)
    return bombre("invalid -fromPage syntax",NULL,0);
  return 1;
}

/* -usePages=start=<pagenumber>,end=<pagenumber>,interval=<integer> */
long usePages_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  PLOT_REQUEST *plreq;
  unsigned long flags = 0;
  plreq = plotspec->plot_request+plotspec->plot_requests-1;

  /* Parse keywords: start, end, interval */
  if (!scanItemList(&flags, item, &items, 0,
                    "start", SDDS_LONG, &plreq->usePagesStart, 1, USEPAGES_START_GIVEN,
                    "end", SDDS_LONG, &plreq->usePagesEnd, 1, USEPAGES_END_GIVEN,
                    "interval", SDDS_LONG, &plreq->usePagesInterval, 1, USEPAGES_INTERVAL_GIVEN,
                    NULL)) {
    return bombre("invalid -usePages syntax",
                  "-usePages=start=<pagenumber>,end=<pagenumber>,interval=<integer>", 0);
  }
  /* Validate interval and bounds */
  if (!(flags & USEPAGES_INTERVAL_GIVEN) || plreq->usePagesInterval<=0)
    return bombre("invalid -usePages syntax",
                  "-usePages=start=<pagenumber>,end=<pagenumber>,interval=<integer>", 0);
  if ((flags & USEPAGES_START_GIVEN) && plreq->usePagesStart<=0)
    return bombre("invalid -usePages syntax",
                  "-usePages=start=<pagenumber>,end=<pagenumber>,interval=<integer>", 0);
  if ((flags & USEPAGES_END_GIVEN) && plreq->usePagesEnd<=0)
    return bombre("invalid -usePages syntax",
                  "-usePages=start=<pagenumber>,end=<pagenumber>,interval=<integer>", 0);
  if ((flags & USEPAGES_START_GIVEN) && (flags & USEPAGES_END_GIVEN) &&
      plreq->usePagesStart>plreq->usePagesEnd)
    return bombre("invalid -usePages syntax",
                  "-usePages=start=<pagenumber>,end=<pagenumber>,interval=<integer>", 0);

  plreq->usePagesFlags = flags;
  return 1;
}

long xexclude_columnnames_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  /*static char *exclude_columnnames_usage = 
    "-xExclude=<xname>,<xname-list> -yExclude=<yname>,<yname-list>";*/
  return plotExclude_AP(plotspec,X_EXCLUDE,item,items);
}

long yexclude_columnnames_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  /*  static char *yexclude_columnnames_usage = "-yExclude=<yname>[,<yname-list>]";*/
  return plotExclude_AP(plotspec,Y_EXCLUDE,item,items);
}

long plotExclude_AP(PLOT_SPEC *plotspec, long exclude_type, char **item, long items)
{
  EXCLUDE_SPEC *exclude_spec;
  long i;
  
  switch (exclude_type) {
  case X_EXCLUDE:
    exclude_spec = &plotspec->plot_request[plotspec->plot_requests-1].x_exclude_spec;
    break;
  case Y_EXCLUDE:
    exclude_spec = &plotspec->plot_request[plotspec->plot_requests-1].y_exclude_spec;
    break;
  default:
    return bombre("invalid exclude type in plotExclude_AP",NULL,0);
  }
  exclude_spec->excludeNames=items;
  exclude_spec->excludeName=tmalloc(sizeof(*exclude_spec->excludeName)*items);
  exclude_spec->was_wildExclude=tmalloc(sizeof(*exclude_spec->was_wildExclude)*items);
  for (i=0;i<items;i++) {
    exclude_spec->was_wildExclude[i]=has_wildcards(item[i]);
    SDDS_CopyString(&exclude_spec->excludeName[i],item[i]);
  }
  return 1;
}


long parameternames_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    static char *parameternames_usage = 
        "-parameternames=<xname>,<yname-list>[,{<y1name-list> | <x1name>,<y1name-list>}]";
    return plotnames_AP1(plotspec, item, items, parameternames_usage, PARAMETER_DATA);
    }

long arraynames_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    static char *arraynames_usage = 
        "-arraynames=<xname>,<yname-list>[,{<y1name-list> | <x1name>,<y1name-list>}]";
    return plotnames_AP1(plotspec, item, items, arraynames_usage, ARRAY_DATA);
    }

long plotnames_AP1(PLOT_SPEC *plotspec, char **item, long items, char *plotnames_usage, long class)
{
  PLOT_REQUEST *plreq;
  long i, groups, y1_index;
  char *ptry;
  char **item0, **item0Orig=NULL;
  
  plreq = &plotspec->plot_request[plotspec->plot_requests-1];
  if (plreq->filenames!=0 || plreq->data_class!=class || plotspec->plot_requests==1)
    add_plot_request(plotspec);
  plreq = &plotspec->plot_request[plotspec->plot_requests-1];
  
  if (items<1)
    return bombre("invalid syntax", plotnames_usage, 0);
  
  if (!(item0=SDDS_Malloc(sizeof(*item0)*items)) ||
      !(item0Orig=SDDS_Malloc(sizeof(*item0Orig)*items)))
    SDDS_Bomb("memory allocation failure");
  for (i=0; i<items; i++) {
    SDDS_CopyString(item0+i, item[i]);
    item0Orig[i] = item0[i];
  }
  
  y1_index = -1;
  if (items>2) 
    y1_index = items==3?2:3;

  if (items==1)
    groups = 1;
  else
    groups = count_chars(item0[1], ',')+1;
  /* if (y1_index!=-1 && count_chars(item0[y1_index], ',')!=(groups-1))
     return bombre("invalid syntax", plotnames_usage, 0); */
  
  plreq->xname = SDDS_Realloc(plreq->xname, sizeof(*plreq->xname)*(plreq->datanames+groups));
  plreq->yname = SDDS_Realloc(plreq->yname, sizeof(*plreq->yname)*(plreq->datanames+groups));
  plreq->x1name = SDDS_Realloc(plreq->x1name, sizeof(*plreq->x1name)*(plreq->datanames+groups));
  plreq->y1name = SDDS_Realloc(plreq->y1name, sizeof(*plreq->y1name)*(plreq->datanames+groups));
  plreq->was_wildname = SDDS_Realloc(plreq->was_wildname, sizeof(*plreq->was_wildname)*(plreq->datanames+groups));
  plreq->data_class = class;
  for (i=0; i<groups; i++) {
    plreq->was_wildname[i] = 0;
    
    if (items==1)
      plreq->xname[plreq->datanames+i] = NULL;
    else
      SDDS_CopyString(&plreq->xname[plreq->datanames+i], item0[0]);
    plreq->x1name[plreq->datanames+i] = plreq->y1name[plreq->datanames+i] = NULL;
    if (items==4)
      SDDS_CopyString(&plreq->x1name[plreq->datanames+i], item0[2]);
    if (items==1)
      {
        if (SDDS_StringIsBlank(item0[0]))
          return bombre("invalid syntax---too few <ynames> items", plotnames_usage, 0);
        SDDS_CopyString(&plreq->yname[plreq->datanames+i], item0[0]);
      }
    else
      {
        if (SDDS_StringIsBlank(item0[1]))
          return bombre("invalid syntax---too few <ynames> items", plotnames_usage, 0);
        if ((ptry=strchr(item0[1], ',')))
          *ptry = 0;
        if (SDDS_StringIsBlank(item0[1])) 
          return bombre("invalid syntax---too few <ynames> items", plotnames_usage, 0);
        SDDS_CopyString(&plreq->yname[plreq->datanames+i], item0[1]);
        if (ptry) 
          item0[1] = ptry+1;
        else 
          item0[1] = NULL;
        if (y1_index!=-1) {
          if (SDDS_StringIsBlank(item0[y1_index])) {
            if (i == 0) {
              return bombre("invalid syntax---too few <y1names> items", plotnames_usage, 0);
            }
            SDDS_CopyString(&plreq->y1name[plreq->datanames+i],plreq->y1name[plreq->datanames+i-1]);
          } else {
            if ((ptry=strchr(item0[y1_index], ',')))
              *ptry = 0;
            if (SDDS_StringIsBlank(item0[y1_index])) {
              if (i == 0) {
                return bombre("invalid syntax---too few <y1names> items", plotnames_usage, 0);
              }
              SDDS_CopyString(&plreq->y1name[plreq->datanames+i],plreq->y1name[plreq->datanames+i-1]);
            } else
              SDDS_CopyString(&plreq->y1name[plreq->datanames+i], item0[y1_index]);
            if (ptry)
              item0[y1_index] = ptry+1;
            else 
              item0[y1_index] = NULL;
          }
        }
      }
  }
  for (i=0; i<items; i++)
      free(item0Orig[i]);
  free(item0);
  free(item0Orig);

  plreq->datanames += groups;
  return 1;
}


long pspace_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];

    SDDS_ZeroMemory((void*)plreq->pspace, sizeof(*plreq->pspace)*4);
    if (items<4 ||
        sscanf(item[0], "%lf", &plreq->pspace[0])!=1 ||
        sscanf(item[1], "%lf", &plreq->pspace[1])!=1 ||
        sscanf(item[2], "%lf", &plreq->pspace[2])!=1 ||
        sscanf(item[3], "%lf", &plreq->pspace[3])!=1 ||
        plreq->pspace[0]>plreq->pspace[1] || plreq->pspace[2]>plreq->pspace[3])
        return bombre("invalid -pspace syntax", "-scales=<xmin>,<xmax>,<ymin>,<ymax>", 0);
    return 1;
    }

long scales_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];

    SDDS_ZeroMemory((void*)plreq->mapping, sizeof(*plreq->mapping)*4);
    if (items<4 ||
        sscanf(item[0], "%lf", &plreq->mapping[0])!=1 ||
        sscanf(item[1], "%lf", &plreq->mapping[1])!=1 ||
        sscanf(item[2], "%lf", &plreq->mapping[2])!=1 ||
        sscanf(item[3], "%lf", &plreq->mapping[3])!=1 ||
        plreq->mapping[0]>plreq->mapping[1] || plreq->mapping[2]>plreq->mapping[3])
        return bombre("invalid -scales syntax", "-scales=<xmin>,<xmax>,<ymin>,<ymax>", 0);
    return 1;
    }

long unsuppresszero_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    unsigned long bits;
    if (items>2)
        return bombre("invalid -unsuppresszero syntax", "-unsuppresszero[={x | y}]", 0);
    bits = 0;
    if (items==0)
        bits = PLREQ_UNSUPPRESSX + PLREQ_UNSUPPRESSY;
    while (items--) {
        if (item[items][0]=='x')
            bits |= PLREQ_UNSUPPRESSX;
        else if (item[items][0]=='y')
            bits |= PLREQ_UNSUPPRESSY;
        else
            return bombre("invalid -unsuppresszero syntax", "-unsuppresszero[={x | y}]", 0);
        }
    plotspec->plot_request[plotspec->plot_requests-1].flags |= bits;
    return 1;
    }

long zoom_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];

    plreq->zoom.magnification[0] = plreq->zoom.magnification[1] = 0;
    plreq->zoom.center[0] = plreq->zoom.center[1] = 0;
    if (!scanItemList(&plreq->zoom.flags, item, &items, 0,
                        "xfactor", SDDS_DOUBLE, plreq->zoom.magnification+0, 1, ZOOM_XMAG,
                        "yfactor", SDDS_DOUBLE, plreq->zoom.magnification+1, 1, ZOOM_YMAG,
                        "xcenter", SDDS_DOUBLE, plreq->zoom.center+0, 1, ZOOM_XCEN,
                        "ycenter", SDDS_DOUBLE, plreq->zoom.center+1, 1, ZOOM_YCEN,
                        "pcenter", SDDS_DOUBLE, plreq->zoom.center+0, 1, ZOOM_PCEN,
                        "qcenter", SDDS_DOUBLE, plreq->zoom.center+1, 1, ZOOM_QCEN,
                        "delay", -1, NULL, 0, ZOOM_DELAY,
                        NULL))
        return bombre("invalid -zoom syntax", "-zoom=[xfactor=<value>][,yfactor=<value>][,{xcenter=<value> | qcenter=<value>}][,{ycenter=<value> | pcenter=<value>}],[,delay]", 0);
#if defined(DEBUG)
    fprintf(stderr, "zoom: factor=%e, %e   center=%e, %e\n",
            plreq->zoom.magnification[0], plreq->zoom.magnification[1],
            plreq->zoom.center[0], plreq->zoom.center[1]);
#endif
    return 1;
    }

long nolabels_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_NOLABELS;
    return 1;
    }

long noborder_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_NOBORDER;
    return 1;
    }

long noscales_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (!items)
        plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_NOSCALESX|PLREQ_NOSCALESY;
    while (items--) {
        switch (item[items][0]) {
          case 'x': case 'X':
            plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_NOSCALESX;
            break;
          case 'y': case 'Y':
            plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_NOSCALESY;
            break;
          default:
            return bombre("invalid -noscales syntax", "-noscales[={x | y}]", 0);
            }
        }
    return 1;
    }

long equalAspect_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    (void)plotspec;
    if (!items)
        return 1;
    if (items == 1) {
        if (!strcmp(item[0], "1") || !strcmp(item[0], "-1"))
            return 1;
        return bombre("invalid -equalAspect value", "-equalAspect[={-1,1}]", 0);
        }
    return bombre("invalid -equalAspect syntax", "-equalAspect[={-1,1}]", 0);
}

long xlabel_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    return plotlabel_AP(plotspec, 0, item, items);
    }
long ylabel_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    return plotlabel_AP(plotspec, 1, item, items);
    }
long title_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    return plotlabel_AP(plotspec, 2, item, items);
    }
long topline_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    return plotlabel_AP(plotspec, 3, item, items);
    }

long plotlabel_AP(PLOT_SPEC *plotspec, long label_index, char **item, long items)
{
    LABEL_SPEC *lspec;
    long offset;
    char *useItem;
    unsigned long oldFlags;
    
    static char *usage[4] = {
        "-xlabel=[@<parameter-name> | format=<format_string> | <string> | use={name | symbol |      description}[,units]][,offset=<value>][,scale=<value>][,edit=<edit-command>][,thickness=<integer>][,linetype=<integer>]",
        "-ylabel=[@<parameter-name> | format=<format_string> | <string> | use={name | symbol | description}[,units]][,offset=<value>][,scale=<value>][,edit=<edit-command>][,thickness=<integer>][,linetype=<integer>]|[,vary]",
        "-title=[@<parameter-name> | format=<format_string> | <string> | use={name | symbol | description}[,units]][,offset=<value>][,scale=<value>][,edit=<edit-command>][,thickness=<integer>][,linetype=<integer>]",
        "-topline=[@<parameter-name> | format=<format_string> | <string> | use={name | symbol | description[,units]}][,offset=<value>][,scale=<value>][,edit=<edit-command>][,thickness=<integer>][,linetype=<integer>]",
        } ;
    if (label_index<0 || label_index>3)
        return bombre("programming error--invalid label_index in plotlabel_AP", NULL, 0);
    lspec = &plotspec->plot_request[plotspec->plot_requests-1].label[label_index];
    if (items<1)
        return bombre("invalid labeling syntax", usage[label_index], 0);
    if (!contains_keyword_phrase(item[0]))
        offset = 1;
    else 
        offset = 0;
    items -= offset;
    useItem = NULL;
    oldFlags = lspec->flags;
    lspec->linetype=0;
    if (!scanItemList(&lspec->flags, item+offset, &items, 0,
                      "use", SDDS_STRING, &useItem, 1, LABEL_USE_NAME,
                      "offset", SDDS_DOUBLE, &lspec->offset, 1, LABEL_OFFSET_GIVEN, 
                      "scale", SDDS_DOUBLE, &lspec->scale, 1, LABEL_SCALE_GIVEN,
                      "edit", SDDS_STRING, &lspec->edit_command, 1, LABEL_EDITCOMMAND_GIVEN,
                      "units", -1, NULL, 0, LABEL_INCLUDE_UNITS,
                      "thickness", SDDS_LONG, &lspec->thickness, 1, LABEL_THICKNESS_GIVEN,
                      "linetype", SDDS_LONG, &lspec->linetype, 1, LABEL_LINETYPE_GIVEN,
		      "format",SDDS_STRING,&lspec->format,1,LABEL_FORMAT_GIVEN,
                        NULL))
        return bombre("invalid labeling syntax", usage[label_index], 0);
    lspec->flags |= oldFlags;
      
    if (lspec->flags&LABEL_USE_NAME) {
        char *useChoice[3] = {"name", "symbol", "description"};
        long index;
        lspec->flags -= LABEL_USE_NAME;
        if ((index=match_string(useItem, useChoice, 3, 0))<0)
            return bombre("invalid labeling syntax--unrecognized use field", usage[label_index], 0);
        else 
            lspec->flags += LABEL_USE_NAME<<index;
        free(useItem);
        }
    if (offset) {
        lspec->label = item[0];
        if (item[0][0]=='@') {
            lspec->flags |= LABEL_PARAMETER_GIVEN;
            SDDS_CopyString(&lspec->label, item[0]+1);
            }
        else {
            SDDS_CopyString(&lspec->label, item[0]);
            lspec->flags |= LABEL_STRING_GIVEN;
          }
        }
    else {
        lspec->flags |= LABEL_USE_DEFAULT;
        lspec->label = NULL;
        }
    if (lspec->linetype<0)
      lspec->linetype = 0;
    if (lspec->thickness < 0)
      lspec->thickness = 0;
    if (lspec->thickness >= 10)
      lspec->thickness = 9;
    return 1;
    }


long string_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    STRING_LABEL_SPEC *sspec;
    long sspecs, countx, county, i;
    
    static char *usage = 
        "-string={ @<parameterName> | format=<format_string>  | <string>},{xCoordinate={<value>|@<parameterName>} | pCoordinate=<value>},{yCoordinate={<value>|@<parameterName>} | qCoordinate=<value>}[,scale=<value>][,angle=<degrees>][,linetype=<integer>][,edit=<editCommand>][,justifyMode=<modes>][,slant=<degrees>][,thickness=<integer>]";

    sspecs = plotspec->plot_request[plotspec->plot_requests-1].string_labels;
    plotspec->plot_request[plotspec->plot_requests-1].string_label =
        SDDS_Realloc(plotspec->plot_request[plotspec->plot_requests-1].string_label,
                     sizeof(*sspec)*(sspecs+1));
    sspec = plotspec->plot_request[plotspec->plot_requests-1].string_label+sspecs;
    sspec->thickness=0;
    plotspec->plot_request[plotspec->plot_requests-1].string_labels += 1;
    if (items<1)
        return bombre("invalid -string syntax", usage, 0);
    items -= 1;
    sspec->edit_command = sspec->justify_mode = NULL;
    sspec->positionParameter[0] = sspec->positionParameter[1] = NULL;
    if (!scanItemList(&sspec->flags, item+1, &items, 0,
                        "scale", SDDS_DOUBLE, &sspec->scale, 1, LABEL_SCALE_GIVEN,
                        "angle", SDDS_DOUBLE, &sspec->angle, 1, LABEL_ANGLE_GIVEN,
                        "xcoordinate", SDDS_STRING, sspec->positionParameter+0, 1, LABEL_X_GIVEN,
                        "ycoordinate", SDDS_STRING, sspec->positionParameter+1, 1, LABEL_Y_GIVEN,
                        "pcoordinate", SDDS_DOUBLE, sspec->position+0, 1, LABEL_P_GIVEN,
                        "qcoordinate", SDDS_DOUBLE, sspec->position+1, 1, LABEL_Q_GIVEN,
                        "linetype", SDDS_LONG, &sspec->linetype, 1, LABEL_LINETYPE_GIVEN,
		        "format", SDDS_STRING, &sspec->format, 1, LABEL_FORMAT_GIVEN,
                        "thickness", SDDS_LONG, &sspec->thickness, 1, LABEL_THICKNESS_GIVEN,
                        "justify", SDDS_STRING, &sspec->justify_mode, 1, LABEL_JUSTIFYMODE_GIVEN,
                        "edit", SDDS_STRING, &sspec->edit_command, 1, LABEL_EDITCOMMAND_GIVEN,
                        "slant", SDDS_DOUBLE, &sspec->slant, 1, LABEL_SLANT_GIVEN,
                        NULL))
        return bombre("invalid -string syntax", usage, 0);
    for (i=0; i<2; i++) {
      if (sspec->flags&(LABEL_X_GIVEN<<i)) {
        if (sspec->positionParameter[i][0]=='@') {
          strcpy_ss(sspec->positionParameter[i], sspec->positionParameter[i]+1);
          sspec->flags |= (LABEL_XPARAM_GIVEN<<i);
        }
        else {
          if (!sscanf(sspec->positionParameter[i], "%le", &sspec->position[i]))
            return bombre("invalid coordinate value for -string", usage, 0);
        }
      }
    }
    
    countx = ((sspec->flags&LABEL_X_GIVEN)?1:0) + ((sspec->flags&LABEL_P_GIVEN)?1:0) ;
    county = ((sspec->flags&LABEL_Y_GIVEN)?1:0) + ((sspec->flags&LABEL_Q_GIVEN)?1:0) ;
    if (countx!=1 || county!=1)
        return bombre("invalid -string syntax---specify one of (x, p) and one of (y, q)", usage, 0);
    if (item[0][0]=='@') {
        sspec->flags |= LABEL_PARAMETER_GIVEN;
        SDDS_CopyString(&sspec->string, item[0]+1);
        }
    else
        SDDS_CopyString(&sspec->string, item[0]);
    if (plotspec->plot_request[plotspec->plot_requests-1].global_thickness_default &&
        !(sspec->flags&LABEL_THICKNESS_GIVEN)) {
      sspec->thickness = plotspec->plot_request[plotspec->plot_requests-1].global_thickness_default;
      sspec->flags |= LABEL_THICKNESS_GIVEN;
    }
    if (sspec->thickness < 0)
      sspec->thickness = 0;
    if (sspec->thickness >= 10)
      sspec->thickness = 9;
    return 1;
    }

long filenamesontopline_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  unsigned long dummy;
  plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_FNONTOPLINE;
  plotspec->plot_request[plotspec->plot_requests-1].flags &= ~PLREQ_YLONTOPLINE;
  plotspec->plot_request[plotspec->plot_requests-1].filenamesOnToplineEditCmd = NULL;
  if (!scanItemList(&dummy, item, &items, 0,
                    "editcommand", SDDS_STRING, 
                    &plotspec->plot_request[plotspec->plot_requests-1].filenamesOnToplineEditCmd,
                    1, 0,
                    NULL)) {
    return bombre("invalid -filenamesOnTopline syntax", "-filenamesOnTopline[=editcommand=<string>]",
                  0);
  }
  return 1;
}

long ylabelontopline_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_YLONTOPLINE;
    plotspec->plot_request[plotspec->plot_requests-1].flags &= ~PLREQ_FNONTOPLINE;
    return 1;
    }

long verticalprint_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    static char *option[2] = {"up", "down"};
    long i;
    if (items!=1 || (i=match_string(item[0], option, 2, 0))<0)
        return bombre("invalid -verticalprint syntax", "-verticalprint={up | down}", 0);
    if (i)
      plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_VPRINTDOWN;
    return 1;
    }

long toptitle_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_TOPTITLE;
    return 1;
    }

long datestamp_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_DATESTAMP;
    return 1;
    }

long samescale_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    unsigned long flags;
    if (items==0) {
        plotspec->plot_request[plotspec->plot_requests-1].flags 
            |= PLREQ_SAMESCALEX+PLREQ_SAMESCALEY;
        return 1;
        }

    if (!scanItemList(&flags, item, &items, 0,
                        "x", -1, NULL, 0, PLREQ_SAMESCALEX,
                        "y", -1, NULL, 0, PLREQ_SAMESCALEY,
                        "global", -1, NULL, 0, PLREQ_SAMESCALEGLOBAL,
                        NULL))
        return bombre("invalid -samescales syntax", "-samescales[=x][,y][,global]", 0);
    if (!(flags&(PLREQ_SAMESCALEX+PLREQ_SAMESCALEY)))
        flags |= PLREQ_SAMESCALEX+PLREQ_SAMESCALEY;
    plotspec->plot_request[plotspec->plot_requests-1].flags |= flags;
    return 1;
    }

long joinscale_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    unsigned long flags = 0;
    if (items==0 ||
        !scanItemList(&flags, item, &items, 0,
                      "x", -1, NULL, 0, PLREQ_JOINSCALE_X,
                      "y", -1, NULL, 0, PLREQ_JOINSCALE_Y,
                      NULL) ||
        (flags&PLREQ_JOINSCALE_X && flags&PLREQ_JOINSCALE_Y) ||
        flags==0)
        return bombre("invalid -joinscales syntax", "-joinscales={x|y}", 0);
    plotspec->plot_request[plotspec->plot_requests-1].joinScaleFlags = flags;
    if (flags&PLREQ_JOINSCALE_X) {
      plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_SAMESCALEX;
      if (!(plotspec->plot_request[plotspec->plot_requests-1].label[1].flags&LABEL_SCALE_GIVEN)) {
        plotspec->plot_request[plotspec->plot_requests-1].label[1].scale = .9;
        plotspec->plot_request[plotspec->plot_requests-1].label[1].flags |= LABEL_SCALE_GIVEN;
      }
    } else {
      plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_SAMESCALEY;
    }
    return 1;
    }

static char *legend_usage = "-legend={{xy}symbol | {xy}description | {xy}name | filename | specified=<string> | parameter=<name>} {,format=<format_string>}{,editCommand=<edit-string>}[,units][,firstFileOnly][,scale=<value>][,thickness=<integer>][,nosubtype]";

long legend_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    LEGEND_SPEC *legend;
    unsigned long oldFlags;

    plotspec->plot_request[plotspec->plot_requests-1].legend.scale = 1;
    if (items==0) {
        plotspec->plot_request[plotspec->plot_requests-1].legend.code = LEGEND_YSYMBOL;
#if defined(DEBUG)
        fprintf(stderr, "legend defaulting to ysymbol mode\n");
#endif
        return 1;
        }
    legend = &plotspec->plot_request[plotspec->plot_requests-1].legend;
    oldFlags = legend->code;
    if (!scanItemList(&legend->code,
                        item, &items, 0,
                        "ysymbol", -1, NULL, 0, LEGEND_YSYMBOL,
                        "xsymbol", -1, NULL, 0, LEGEND_XSYMBOL,
                        "yname", -1, NULL, 0, LEGEND_YNAME,
                        "xname", -1, NULL, 0, LEGEND_XNAME,
                        "filename", -1, NULL, 0, LEGEND_FILENAME,
                        "specified", SDDS_STRING, &legend->value, 1, LEGEND_SPECIFIED, 
                        "rootname", -1, NULL, 0, LEGEND_ROOTNAME,
                        "parameter", SDDS_STRING, &legend->value, 1, LEGEND_PARAMETER,
                        "ydescription", -1, NULL, 0, LEGEND_YDESCRIPTION,
                        "xdescription", -1, NULL, 0, LEGEND_XDESCRIPTION,
                        "units", -1, NULL, 0, LEGEND_UNITS,
                        "editcommand", SDDS_STRING, &legend->edit_command, 1, LEGEND_EDIT,
                        "firstfileonly", -1, NULL, 0, LEGEND_FIRSTFILEONLY,
                        "scale", SDDS_DOUBLE, &legend->scale, 1, LEGEND_SCALEGIVEN,
                        "thickness", SDDS_LONG, &legend->thickness, 1, LEGEND_THICKNESS,
			"format", SDDS_STRING, &legend->format, 1, LEGEND_FORMAT,
                        "nosubtype", -1, NULL, 0, LEGEND_NOSUBTYPE,
                        NULL) ||
        (legend->code&LEGEND_SCALEGIVEN && legend->scale<=0))
        return bombre("invalid -legend syntax", legend_usage, 0);
    legend->code |= oldFlags;
    if (!(legend->code&LEGEND_CHOICES))
        legend->code |= LEGEND_YSYMBOL;
    if (legend->thickness < 0)
      legend->thickness = 0;
    if (legend->thickness >= 10)
      legend->thickness = 9;
#if defined(DEBUG)
    fprintf(stderr, "legend.code = %x\n", legend->code);
    if (legend->code&LEGEND_EDIT)
        fprintf(stderr, "editing legend with edit command %s\n", 
                legend->edit_command);
#endif
    return 1;
    }

#define OPT_OVERLAY_XFACTOR 0
#define OPT_OVERLAY_YFACTOR 1
#define OPT_OVERLAY_XOFFSET 2
#define OPT_OVERLAY_YOFFSET 3
#define OPT_OVERLAY_XMODE 4
#define OPT_OVERLAY_YMODE 5
#define OPT_OVERLAY_XCENTER 6
#define OPT_OVERLAY_YCENTER 7
#define OPT_OVERLAY_POFFSET 8
#define OPT_OVERLAY_QOFFSET 9
#define OPT_OVERLAY_PALIGN 10
#define OPT_OVERLAY_QALIGN 11
#define OVERLAY_OPTIONS 12
static char *overlay_option[OVERLAY_OPTIONS] = {
    "xfactor", "yfactor", "xoffset", "yoffset", "xmode", "ymode",
    "xcenter", "ycenter", "poffset", "qoffset", "palign", "qalign",
    } ;

#define OPT_OVERLAYMODE_NORMAL 0
#define OPT_OVERLAYMODE_UNIT   1
#define OVERLAYMODE_OPTIONS 2
static char *overlaymode_option[OVERLAYMODE_OPTIONS] = {
    "normal", "unit",
    } ;

static char *overlay_usage = "-overlay=[{xy}mode=<mode>][,{xy}factor=<value>][,{xy}offset=<value>][,{xy}center][,{pq}offset=<value>][,{pq}align]";

long overlay_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    char *ptr;
    unsigned long flags;
    
    plotspec->plot_request[plotspec->plot_requests-1].overlay.flags = OVERLAY_DATA;
    if (items==0) {
        return 1;
        }
    while (items--) {
        ptr = NULL;
        if ((ptr=strchr(item[items], '=')))
            *ptr++ = 0;
        switch (match_string(item[items], overlay_option, OVERLAY_OPTIONS, 0)) {
          case OPT_OVERLAY_XMODE:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags &= ~OVERLAY_XNORMAL;
            switch (match_string(ptr, overlaymode_option, OVERLAYMODE_OPTIONS, 0)) {
              case OPT_OVERLAYMODE_NORMAL:
                plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_XNORMAL;
                break;
              case OPT_OVERLAYMODE_UNIT:
                break;
              default:
                return bombre("invalid -overlay xmode syntax", overlay_usage, 0);
                }
            break;
          case OPT_OVERLAY_XFACTOR:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_XFACTOR;
            if (sscanf(ptr, "%lf",  plotspec->plot_request[plotspec->plot_requests-1].overlay.factor+0)!=1)
                return bombre("invalid -overlay xfactor syntax", overlay_usage, 0);
            break;
          case OPT_OVERLAY_XOFFSET:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_XOFFSET;
            if (sscanf(ptr, "%lf",  plotspec->plot_request[plotspec->plot_requests-1].overlay.offset+0)!=1)
                return bombre("invalid -overlay xoffset syntax", overlay_usage, 0);
            break;
          case OPT_OVERLAY_POFFSET:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_POFFSET;
            if (sscanf(ptr, "%lf",  plotspec->plot_request[plotspec->plot_requests-1].overlay.unitOffset+0)!=1)
                return bombre("invalid -overlay poffset syntax", overlay_usage, 0);
            break;
          case OPT_OVERLAY_XCENTER:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_XCENTER;
            break;
          case OPT_OVERLAY_PALIGN:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_PALIGN;
            break;
          case OPT_OVERLAY_YMODE:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags &= ~OVERLAY_YNORMAL;
            switch (match_string(ptr, overlaymode_option, OVERLAYMODE_OPTIONS, 0)) {
              case OPT_OVERLAYMODE_NORMAL:
                plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_YNORMAL;
                break;
              case OPT_OVERLAYMODE_UNIT:
                break;
              default:
                return bombre("invalid -overlay ymode syntax", overlay_usage, 0);
                }
            break;
          case OPT_OVERLAY_YFACTOR:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_YFACTOR;
            if (sscanf(ptr, "%lf",  plotspec->plot_request[plotspec->plot_requests-1].overlay.factor+1)!=1)
                return bombre("invalid -overlay yfactor syntax", overlay_usage, 0);
            break;
          case OPT_OVERLAY_YOFFSET:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_YOFFSET;
            if (sscanf(ptr, "%lf",  plotspec->plot_request[plotspec->plot_requests-1].overlay.offset+1)!=1)
                return bombre("invalid -overlay yoffset syntax", overlay_usage, 0);
            break;
          case OPT_OVERLAY_QOFFSET:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_QOFFSET;
            if (sscanf(ptr, "%lf",  plotspec->plot_request[plotspec->plot_requests-1].overlay.unitOffset+1)!=1)
                return bombre("invalid -overlay qoffset syntax", overlay_usage, 0);
            break;
          case OPT_OVERLAY_YCENTER:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_YCENTER;
            break;
          case OPT_OVERLAY_QALIGN:
            plotspec->plot_request[plotspec->plot_requests-1].overlay.flags |= OVERLAY_QALIGN;
            break;
          default:
            return bombre("unknown -overlay keyword", overlay_usage, 0);
            }
        }
    flags = plotspec->plot_request[plotspec->plot_requests-1].overlay.flags;
    if ((flags&OVERLAY_YCENTER) && (flags&OVERLAY_QALIGN))
      return bombre("give only one of ycenter or qalign for overlay", overlay_usage, 0);
    if ((flags&OVERLAY_XCENTER) && (flags&OVERLAY_PALIGN))
      return bombre("give only one of xcenter or palign for overlay", overlay_usage, 0);
    return 1;
    }

static char *separate_usage = "-separate[=[<number-to-group>][,groupsOf=<number>][,fileIndex][,fileString][,nameIndex][,nameString][,page][,request][,tag][,subpage][,inamestring]";

long separate_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    short numberSeen = 0;

    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_SEPARATE;
    plotspec->plot_request[plotspec->plot_requests-1].separate_group_size = 1;
    
    if (items<1)
        return 1;
    if (isdigit(item[0][0])) {
        if (sscanf(item[0], "%" SCNd32, &plotspec->plot_request[plotspec->plot_requests-1].separate_group_size)!=1  ||
            plotspec->plot_request[plotspec->plot_requests-1].separate_group_size<=0)
            return bombre("invalid -separate syntax", separate_usage, 0);
        item += 1;
        items -= 1;
        numberSeen = 1;
        }
    if (!scanItemList(&plotspec->plot_request[plotspec->plot_requests-1].separate_flags,
                        item, &items,  0,
                        "groupsof", SDDS_LONG, &plotspec->plot_request[plotspec->plot_requests-1].separate_group_size, 1, 
                        SEPARATE_GROUPSOF,
                        "fileindex", -1, NULL, 0, SEPARATE_FILEINDEX,
                        "nameindex", -1, NULL, 0, SEPARATE_NAMEINDEX,
                        "filestring", -1, NULL, 0, SEPARATE_FILESTRING,
                        "namestring", -1, NULL, 0, SEPARATE_NAMESTRING,
                        "inamestring", -1, NULL, 0, SEPARATE_INAMESTRING,
                        "page", -1, NULL, 0, SEPARATE_PAGE,
                        "request", -1, NULL, 0, SEPARATE_REQUEST,
                        "tag", -1, NULL, 0, SEPARATE_TAG,
                        "subpage", -1, NULL, 0, SEPARATE_SUBPAGE,
                        NULL) ||
        (plotspec->plot_request[plotspec->plot_requests-1].separate_flags&SEPARATE_GROUPSOF &&
         plotspec->plot_request[plotspec->plot_requests-1].separate_group_size<=0))
        return bombre("invalid -separate syntax", separate_usage, 0);
    if (numberSeen && !(plotspec->plot_request[plotspec->plot_requests-1].separate_flags&SEPARATE_GROUPSOF)) 
        plotspec->plot_request[plotspec->plot_requests-1].separate_flags |= SEPARATE_GROUPSOF;
    return 1;
    }

long tagrequest_AP(PLOT_SPEC *plotspec, char **item, long items)
{
   /* if (items!=1 ||
        (item[0][0]!='@' && sscanf(item[0], "%lf", &plotspec->plot_request[plotspec->plot_requests-1].user_tag)!=1))
        return bombre("invalid -tagrequest syntax", "-tagrequest={<number> | @<parameter-name>}", 0); */
  if (items!=1 ||
      (item[0][0]!='@' && !SDDS_CopyString(&plotspec->plot_request[plotspec->plot_requests-1].user_tag,item[0])))
    return bombre("invalid -tagrequest syntax", "-tagrequest={<number> | @<parameter-name>}", 0);
  if (item[0][0]=='@')
    SDDS_CopyString(&plotspec->plot_request[plotspec->plot_requests-1].tag_parameter, item[0]+1);
  return 1;
}


static char *xScalesGroup_usage = "-xScalesGroup={ID=<string> | fileIndex|fileString|nameIndex|nameString|page|request|units}[,top]";
long scalesGroup_AP(PLOT_SPEC *plotspec, char **item, long items, long plane,
                    char *errorMessage, char *usage);

long xScalesGroup_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  return scalesGroup_AP(plotspec, item, items, 0, "invalid -xScalesGroup syntax",
                        xScalesGroup_usage);
}

static char *yScalesGroup_usage = "-yScalesGroup={ID=<string> | fileIndex|fileString|nameIndex|nameString|page|request|units}[,right]";

long yScalesGroup_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  return scalesGroup_AP(plotspec, item, items, 1, "invalid -yScalesGroup syntax",
                        yScalesGroup_usage);
}

long scalesGroup_AP(PLOT_SPEC *plotspec, char **item, long items, long plane,
                    char *errorMessage, char *usage)
{
  PLOT_REQUEST *plreq;

  plreq = plotspec->plot_request+plotspec->plot_requests-1;
  if (!scanItemList(&plreq->scalesGroupSpec[plane].flags,
                    item, &items,  0,
                    "id", SDDS_STRING, &plreq->scalesGroupSpec[plane].ID, 1, SCALESGROUP_ID_GIVEN,
                    "fileindex", -1, NULL, 0, SCALESGROUP_USE_FILEINDEX,
                    "filestring", -1, NULL, 0, SCALESGROUP_USE_FILESTRING,
                    "nameindex", -1, NULL, 0, SCALESGROUP_USE_NAMEINDEX,
                    "namestring", -1, NULL, 0, SCALESGROUP_USE_NAMESTRING,
                    "page", -1, NULL, 0, SCALESGROUP_USE_PAGE,
                    "request", -1, NULL, 0, SCALESGROUP_USE_REQUEST,
                    "units", -1, NULL, 0, SCALESGROUP_USE_UNITS,
                    "right", -1, NULL, 0, SCALESGROUP_OTHER_SIDE,
                    "top", -1, NULL, 0, SCALESGROUP_OTHER_SIDE,
                    NULL) ||
      bitsSet(plreq->scalesGroupSpec[plane].flags&~SCALESGROUP_OTHER_SIDE)!=1)
    return bombre(errorMessage, usage, 0);

  if (plreq->scalesGroupSpec[plane].flags&SCALESGROUP_ID_GIVEN &&
      strcmp(plreq->scalesGroupSpec[plane].ID, RESERVED_SCALESGROUP_ID)==0)
    return bombre("invalid -[xy]scalesGroup syntax---reserved ID used.", usage, 0);
  return 1;
}

long newpanel_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (plotspec->plot_requests<2)
        return bombre(NO_REQUESTS_MESSAGE, "-parameterNames, -columnNames or -mplfiles must be given prior to -newpanel", 0);
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_NEWPANEL;
    return 1;
    }

long endpanel_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (plotspec->plot_requests<2)
        return bombre(NO_REQUESTS_MESSAGE, "-parameterNames, -columnNames or -mplfiles must be given prior to -endpanel", 0);
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_ENDPANEL;
    return 1;
    }

long nextpage_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (plotspec->plot_requests<2)
        return bombre(NO_REQUESTS_MESSAGE, "-parameterNames, -columnNames or -mplfiles must be given prior to -nextPage", 0);
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_NEXTPAGE;
    return 1;
    }

static char *omnipresent_usage = "-omnipresent[=first]" ;

long omnipresent_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (plotspec->plot_requests<2)
        return bombre(NO_REQUESTS_MESSAGE, "-parameterNames, -columnNames or -mplfiles must be given prior to -omnipresent", 0);
    if (items==0)
      plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_OMNIPRESENT;
    else if (items==1 && strncmp(item[0], "first", strlen(item[0]))==0) 
      plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_OMNIPRESENT+PLREQ_OMNIFIRST;
    else
      return bombre("invalid -omnipresent sytnax", omnipresent_usage, 0);
    return 1;
    }

static char *layout_usage = "-layout=<nx>,<ny>[,limitPerPage=<integer>]";

long layout_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    unsigned long flags;

    if (items<2)
        return bombre("invalid -layout syntax", layout_usage, 0);
    if (sscanf(item[0], "%ld", plotspec->layout+0)!=1 || sscanf(item[1], "%ld", plotspec->layout+1)!=1 ||
        plotspec->layout[0]<=0 || plotspec->layout[1]<=0) 
        return bombre("invalid -layout syntax", layout_usage, 0);
    item  += 2;
    plotspec->maxPanelsPerPage = plotspec->layout[0]*plotspec->layout[1];
    if ((items-=2) &&
        !scanItemList(&flags, item, &items, 0,
                        "limitperpage", SDDS_LONG, &plotspec->maxPanelsPerPage, 1, 0,
                        NULL))
        return bombre("invalid -layout syntax", layout_usage, 0);
    return 1;
    }

#define OPT_SPLIT_PARAMETERCHANGE 0
#define OPT_SPLIT_PAGES 1
#define OPT_SPLIT_POINTS 2
#define OPT_SPLIT_COLUMNCHANGE 3
#define OPT_SPLIT_NOCOLORBAR 4
#define SPLIT_OPTIONS 5
/*
static char *split_option[SPLIT_OPTIONS] = {
    "parameterchange", "pages", "points",
    "columnchange", "nocolorbar",
        } ;
*/

static char *split_usage = "-split={parameterChange=<parameter-name>[,width=<value>][,start=<value>] |\n\
 columnBin=<column-name>,width=<value>[,start=<value>][,completely] |\n\
 pages[,interval=<interval>][,nocolorbar][,reverseOrder] }\n";

long split_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    SPLIT_SPEC *split;

    if (items<1)
        return bombre("invalid -split syntax", split_usage, 0);
    split = &plotspec->plot_request[plotspec->plot_requests-1].split;
    split->flags = 0;
    if (!scanItemList(&split->flags,
                        item, &items,  0,
                        "parameterchange", SDDS_STRING, &split->name, 1, SPLIT_PARAMETERCHANGE,
                        "columnbin", SDDS_STRING, &split->name, 1, SPLIT_COLUMNBIN,
                        "pages", -1, NULL, 0, SPLIT_PAGES,
                        "interval", SDDS_LONG, &split->interval, 1, SPLIT_PAGES_INTERVAL,
                        "width", SDDS_DOUBLE, &split->width, 1, SPLIT_CHANGE_WIDTH,
                        "start", SDDS_DOUBLE, &split->start, 1, SPLIT_CHANGE_START,
                        "points", -1, NULL, 0, SPLIT_POINTS,
                        "completely", -1, NULL, 0, SPLIT_COMPLETELY,
                        "nocolorbar", -1, NULL, 0, SPLIT_NOCOLORBAR,
                        "reverseorder", -1, NULL, 0, SPLIT_REVERSE_ORDER,
                      NULL)) {
      return bombre("invalid -split syntax", split_usage, 0);
    }
    if (split->flags&(SPLIT_PARAMETERCHANGE) && (!(split->flags&SPLIT_CHANGE_WIDTH) || split->width==0)) {
      return bombre("invalid -split syntax", split_usage, 0);
    }
    if (split->flags&(SPLIT_COLUMNBIN) && (!(split->flags&SPLIT_CHANGE_WIDTH))) {
      split->width = 0;
    }
    return 1;
    }


long grid_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (!items)
        plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_GRIDX|PLREQ_GRIDY;
    while (items--) {
        switch (item[items][0]) {
          case 'x': case 'X':
            plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_GRIDX;
            break;
          case 'y': case 'Y':
            plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_GRIDY;
            break;
          default:
            return bombre("invalid -grid syntax", "-grid[={x | y}]", 0);
            }
        }
    return 1;
    }


long axes_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    unsigned long flags;
    if (!items) {
        plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_AXESX|PLREQ_AXESY;
        return 1;
        }
    if (!scanItemList(&flags, item, &items, 0,
                        "x", -1, NULL, 0, PLREQ_AXESX,
                        "y", -1, NULL, 0, PLREQ_AXESY,
                        "linetype", SDDS_LONG, 
                        &plotspec->plot_request[plotspec->plot_requests-1].axesLinetype,
                        1, 0,
                        "thickness", SDDS_LONG, 
                        &plotspec->plot_request[plotspec->plot_requests-1].axesLinethickness,
                        1, 0,
                        NULL))
        return bombre("invalid -axes syntax", "-axes[=x][,y][,linetype=<number>][,thickness=<number>]", 0);
    if (!(flags&(PLREQ_AXESX|PLREQ_AXESY)))
        flags |= PLREQ_AXESX|PLREQ_AXESY;
    plotspec->plot_request[plotspec->plot_requests-1].flags |= flags;
    if (plotspec->plot_request[plotspec->plot_requests-1].axesLinethickness < 0)
      plotspec->plot_request[plotspec->plot_requests-1].axesLinethickness = 0;
    if (plotspec->plot_request[plotspec->plot_requests-1].axesLinethickness >= 10)
      plotspec->plot_request[plotspec->plot_requests-1].axesLinethickness = 9;
    return 1;
    }

static char *subticks_usage = "-subticksettings=[{xy}divisions=<integer>][,[{xy}]grid][,[{xy}]linetype=<integer>][,[{xy}]thickness=<integer>][,{xy}size=<fraction>][,xNoLogLabel][,yNoLogLabel]";

long subticks_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    TICK_SETTINGS *tset;
    double fraction;
    int32_t linetype, thickness;

    if (items<1)
        return bombre("invalid -subticks syntax", subticks_usage, 0);
    tset = &plotspec->plot_request[plotspec->plot_requests-1].subtick_settings;
    if (isdigit(item[0][0])) {
        if (sscanf(item[0], "%" SCNd32, tset->divisions+0)!=1 ||
            (items>1 && sscanf(item[1], "%" SCNd32, tset->divisions+1)!=1) )
          return bombre("invalid -subticksettings syntax", subticks_usage, 0);
        tset->flags |= TICKSET_GIVEN|TICKSET_XDIVISIONS|TICKSET_YDIVISIONS;
        return 1;
    }
    tset->divisions[0] = tset->divisions[1] = 0;
    tset->linetype[0] = tset->linetype[1] = 0;
    tset->thickness[0] = tset->thickness[1] = 0;
    tset->fraction[0] = tset->fraction[1] = 0;
    if (!scanItemListLong(&tset->flags,
                      item, &items,  0,
                      "xdivisions", SDDS_LONG, tset->divisions+0, 1, TICKSET_XDIVISIONS,
                      "ydivisions", SDDS_LONG, tset->divisions+1, 1, TICKSET_YDIVISIONS,
                      "xgrid", -1, NULL, 0, TICKSET_XGRID,
                      "ygrid", -1, NULL, 0, TICKSET_YGRID,
                      "grid", -1, NULL, 0, TICKSET_XGRID|TICKSET_YGRID,
                      "xlinetype", SDDS_LONG, tset->linetype+0, 1, TICKSET_XLINETYPE,
                      "ylinetype", SDDS_LONG, tset->linetype+1, 1, TICKSET_YLINETYPE,
                      "linetype", SDDS_LONG, &linetype, 1, TICKSET_LINETYPE,
                      "xthickness", SDDS_LONG, tset->thickness+0, 1, TICKSET_XTHICKNESS,
                      "ythickness", SDDS_LONG, tset->thickness+1, 1, TICKSET_YTHICKNESS,
                      "thickness", SDDS_LONG, &thickness, 1, TICKSET_THICKNESS,
                      "xsize", SDDS_DOUBLE, tset->fraction+0, 1, TICKSET_XFRACTION,
                      "ysize", SDDS_DOUBLE, tset->fraction+1, 1, TICKSET_YFRACTION,
                      "size", SDDS_DOUBLE, &fraction, 1, TICKSET_FRACTION,
                      "xnologlabel", -1, NULL, 0, TICKSET_XNOLOGLABEL,
                      "ynologlabel", -1, NULL, 0, TICKSET_YNOLOGLABEL,
                        NULL))
        return bombre("invalid -subticksettings syntax", subticks_usage, 0);
    if (tset->flags&TICKSET_LINETYPE) {
        if (!(tset->flags&TICKSET_XLINETYPE))
            tset->linetype[0] = linetype;
        if (!(tset->flags&TICKSET_YLINETYPE))
            tset->linetype[1] = linetype;
        tset->flags |= TICKSET_XLINETYPE|TICKSET_YLINETYPE;
        }
    if (tset->flags&TICKSET_FRACTION) {
        if (!(tset->flags&TICKSET_XFRACTION))
            tset->fraction[0] = fraction;
        if (!(tset->flags&TICKSET_YFRACTION))
            tset->fraction[1] = fraction;
        tset->flags |= TICKSET_XFRACTION|TICKSET_YFRACTION;
        }
    if (tset->flags&TICKSET_XTHICKNESS) {
      if (tset->thickness[0] < 0)
	tset->thickness[0] = 0;
      if (tset->thickness[0] > 10)
	tset->thickness[0] = 9;
    }
    if (tset->flags&TICKSET_YTHICKNESS) {
      if (tset->thickness[1] < 0)
	tset->thickness[1] = 0;
      if (tset->thickness[1] > 10)
	tset->thickness[1] = 9;
    }
    if (tset->flags&TICKSET_THICKNESS) {
      if (thickness < 0)
	thickness = 0;
      if (thickness > 10)
	thickness = 9;
      if (!(tset->flags&TICKSET_XTHICKNESS))
	tset->thickness[0] = thickness;
      if (!(tset->flags&TICKSET_YTHICKNESS))
            tset->thickness[1] = thickness;
      tset->flags |= TICKSET_XTHICKNESS|TICKSET_YTHICKNESS;
    }
    tset->flags |= TICKSET_GIVEN;
    return 1;
  }

#define OPT_MODE_LINEAR 0
#define OPT_MODE_LOG    1
#define OPT_MODE_SPECIALSCALES 2
#define OPT_MODE_NORMALIZE 3
#define OPT_MODE_OFFSET 4
#define OPT_MODE_CENTER 5
#define OPT_MODE_MEANCENTER 6
#define OPT_MODE_COFFSET 7
#define OPT_MODE_EOFFSET 8
#define OPT_MODE_FRACDEV 9
#define OPT_MODE_AUTOLOG 10
#define OPT_MODE_ABSOLUTE 11
#define MODE_OPTIONS    12
static char *mode_option[MODE_OPTIONS] = {
    "linear", "logarithmic", "specialscales", "normalize", "offset",
    "center", "meancenter", "coffset", "eoffset", "fractionaldeviation",
    "autolog", "absolute",
        } ;

#define OPT_MODE2_LIN_LIN 0
#define OPT_MODE2_LIN_LOG 1
#define OPT_MODE2_LOG_LIN 2
#define OPT_MODE2_LOG_LOG 3
#define MODE_OPTIONS2 4
static char *mode_option2[MODE_OPTIONS2] = {
  "linlin", "linlog", "loglin", "loglog",
};

static char *mode_usage = "-mode=[{x | y} = {linear | logarithmic | specialscales | normalize | offset | coffset | eoffset | center | meanCenter | fractionalDeviation | autolog | absolute}][,...]{,linlin|linlog|loglin|loglog}";

long mode_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  char *ptr;
  long shift, i;
  if (items<1)
    return bombre("invalid -mode syntax", mode_usage, 0);
  for (i=0; i<items; i++) {
    switch (match_string(item[i], mode_option2, MODE_OPTIONS2, 0)) {
    case OPT_MODE2_LIN_LIN:
      plotspec->plot_request[plotspec->plot_requests-1].mode &= ~(MODE_X_LOG+MODE_X_SPECIALSCALES);
      plotspec->plot_request[plotspec->plot_requests-1].mode &= ~((MODE_X_LOG+MODE_X_SPECIALSCALES)<<16);
      break;
    case OPT_MODE2_LIN_LOG:
      plotspec->plot_request[plotspec->plot_requests-1].mode &= ~(MODE_X_LOG+MODE_X_SPECIALSCALES);
      plotspec->plot_request[plotspec->plot_requests-1].mode |= (MODE_X_LOG+MODE_X_SPECIALSCALES)<<16;
      break;
    case OPT_MODE2_LOG_LIN:
      plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_LOG+MODE_X_SPECIALSCALES;
      plotspec->plot_request[plotspec->plot_requests-1].mode &= ~((MODE_X_LOG+MODE_X_SPECIALSCALES)<<16);
      break;
    case OPT_MODE2_LOG_LOG:
      plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_LOG+MODE_X_SPECIALSCALES;
      plotspec->plot_request[plotspec->plot_requests-1].mode |= (MODE_X_LOG+MODE_X_SPECIALSCALES)<<16;
      break;
    default:
      if (!(ptr=strchr(item[i], '=')))
        return bombre("invalid -mode syntax", mode_usage, 0);
      *ptr++ = 0;
      shift = 0;
      switch (item[i][0]) {
      case 'x': case 'X':
        break;
      case 'y': case 'Y':
        shift = 16;
        break;
      default:
        return bombre("invalid -mode syntax", mode_usage, 0);
      }
      switch (match_string(ptr, mode_option, MODE_OPTIONS, 0)) {
      case OPT_MODE_LINEAR:
        plotspec->plot_request[plotspec->plot_requests-1].mode &= ~(MODE_X_LOG<<shift);
        break;
      case OPT_MODE_LOG:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_LOG<<shift;
        break;
      case OPT_MODE_SPECIALSCALES:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_SPECIALSCALES<<shift;
        break;
      case OPT_MODE_NORMALIZE:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_NORMALIZE<<shift;
        break;
      case OPT_MODE_OFFSET:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_OFFSET<<shift;
        break;
      case OPT_MODE_COFFSET:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_COFFSET<<shift;
        break;
      case OPT_MODE_EOFFSET:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_EOFFSET<<shift;
        break;
      case OPT_MODE_CENTER:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_CENTER<<shift;
        break;
      case OPT_MODE_MEANCENTER:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_MEANCENTER<<shift;
        break;
      case OPT_MODE_FRACDEV:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_FRACDEV<<shift;
        break;
      case OPT_MODE_ABSOLUTE:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_ABSOLUTE<<shift;
        break;
      case OPT_MODE_AUTOLOG:
        plotspec->plot_request[plotspec->plot_requests-1].mode |= MODE_X_AUTOLOG<<shift;
        break;
      default:
        return bombre("invalid -mode syntax", mode_usage, 0);
      }
    }
  }
  if ((plotspec->plot_request[plotspec->plot_requests-1].mode&MODE_X_LOG && 
       plotspec->plot_request[plotspec->plot_requests-1].mode&MODE_X_AUTOLOG) ||
      (plotspec->plot_request[plotspec->plot_requests-1].mode&MODE_Y_LOG && 
       plotspec->plot_request[plotspec->plot_requests-1].mode&MODE_Y_AUTOLOG))
    return bombre("give either log or autolog, not both, for mode for each plane", mode_usage, 0);

  return 1;
}

static char *stagger_usage = "-stagger=[xIncrement=<value>][,yIncrement=<value>][,files][,datanames]";

long stagger_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    double dummy;
    plreq = plotspec->plot_request+plotspec->plot_requests-1;
    if (items && sscanf(item[0], "%lf", &dummy)==1) {
        if (items!=2)
            return bombre("invalid -stagger syntax", stagger_usage, 0);
        if (sscanf(item[0], "%lf", plreq->stagger_amount+0)!=1 ||
            sscanf(item[1], "%lf", plreq->stagger_amount+1)!=1 )
            return bombre("invalid -stagger syntax", stagger_usage, 0);
        plreq->stagger_flags = 
            (plreq->stagger_amount[0]?STAGGER_XINC_GIVEN:0) +
                (plreq->stagger_amount[1]?STAGGER_YINC_GIVEN:0) ;
        }
    else {
        if (!scanItemList(&plreq->stagger_flags,
                            item, &items,  0,
                            "xincrement", SDDS_DOUBLE, plreq->stagger_amount+0, 1, STAGGER_XINC_GIVEN,
                            "yincrement", SDDS_DOUBLE, plreq->stagger_amount+1, 1, STAGGER_YINC_GIVEN,
                            "files", -1, NULL, 0, STAGGER_FILES,
                            "columns", -1, NULL, 0, STAGGER_DATANAMES,  /* for backward compatibility */
                            "datanames", -1, NULL, 0, STAGGER_DATANAMES,
                            NULL)) 
            return bombre("invalid -stagger syntax", stagger_usage, 0);
        }
    return 1;
    }

static char *factor_usage = "-factor=[{xy}Multiplier=<value>][,{xy}Parameter=<value>][,{xy}Invert][,{xy}BeforeLog]";

long factor_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    long i;
    
    plreq = plotspec->plot_request+plotspec->plot_requests-1;
    if (!scanItemList(&plreq->factor_flags,
                        item, &items,  0,
                        "xmultiplier", SDDS_DOUBLE, plreq->factor+0, 1, FACTOR_XMULT_GIVEN,
                        "ymultiplier", SDDS_DOUBLE, plreq->factor+1, 1, FACTOR_YMULT_GIVEN,
                        "xparameter", SDDS_STRING, plreq->factor_parameter+0, 1, FACTOR_XPARAMETER_GIVEN,
                        "yparameter", SDDS_STRING, plreq->factor_parameter+1, 1, FACTOR_YPARAMETER_GIVEN,
                        "xinvert", -1, NULL, 0, FACTOR_XINVERT_GIVEN,
                        "yinvert", -1, NULL, 0, FACTOR_YINVERT_GIVEN,
                        "xbeforelog", -1, NULL, 0, FACTOR_XBEFORELOG_GIVEN,
                        "ybeforelog", -1, NULL, 0, FACTOR_YBEFORELOG_GIVEN,
                        NULL) ||
        ((plreq->factor_flags&FACTOR_XMULT_GIVEN) && (plreq->factor_flags&FACTOR_XPARAMETER_GIVEN)) ||
        ((plreq->factor_flags&FACTOR_YMULT_GIVEN) && (plreq->factor_flags&FACTOR_YPARAMETER_GIVEN)))
        return bombre("invalid -factor syntax", factor_usage, 0);
    for (i=0; i<2; i++) {
      if ((plreq->factor_flags&(FACTOR_XINVERT_GIVEN<<i)) &&
          (plreq->factor_flags&(FACTOR_XMULT_GIVEN<<i)))
        plreq->factor[i] = 1/plreq->factor[i];
    }
    return 1;
    }

static char *offset_usage = "-offset=[{x|y}change={value>][,{x|y}parameter=<name>][,{x|y}invert][,{x|y}beforelog]";

long offset_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    long i;
    
    plreq = plotspec->plot_request+plotspec->plot_requests-1;
    if (!scanItemList(&plreq->offset_flags,
                        item, &items,  0,
                        "xchange", SDDS_DOUBLE, plreq->offset+0, 1, OFFSET_XCHANGE_GIVEN,
                        "ychange", SDDS_DOUBLE, plreq->offset+1, 1, OFFSET_YCHANGE_GIVEN,
                        "xparameter", SDDS_STRING, plreq->offset_parameter+0, 1, OFFSET_XPARAMETER_GIVEN,
                        "yparameter", SDDS_STRING, plreq->offset_parameter+1, 1, OFFSET_YPARAMETER_GIVEN,
                        "xinvert", -1, NULL, 0, OFFSET_XINVERT_GIVEN,
                        "yinvert", -1, NULL, 0, OFFSET_YINVERT_GIVEN,
                        "xbeforelog", -1, NULL, 0, OFFSET_XBEFORELOG_GIVEN,
                        "ybeforelog", -1, NULL, 0, OFFSET_YBEFORELOG_GIVEN,
                        NULL))
        return bombre("invalid -offset syntax", offset_usage, 0);
    for (i=0; i<2; i++) {
      if ((plreq->offset_flags&(OFFSET_XINVERT_GIVEN<<i)) &&
          (plreq->offset_flags&(OFFSET_XCHANGE_GIVEN<<i)))
        plreq->offset[i] *= -1;
    }
    return 1;
  }

static char *dither_usage = "-dither=[{x|y}range=<fraction>]";

long dither_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    unsigned long flags;
    
    plreq = plotspec->plot_request+plotspec->plot_requests-1;
    if (!scanItemList(&flags,
                        item, &items,  0,
                        "xrange", SDDS_DOUBLE, plreq->dither+0, 1, 0,
                        "yrange", SDDS_DOUBLE, plreq->dither+1, 1, 0,
                        NULL))
        return bombre("invalid -dither syntax", dither_usage, 0);
    return 1;
  }

static char *sever_usage = "-sever[={xgap=<value> | ygap=<value>}]";

long sever_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    unsigned long flags;
    PLOT_REQUEST *plreq;

    plreq = plotspec->plot_request+plotspec->plot_requests-1;
    if (items==0) 
        flags = PLREQ_SEVER;
    else if (!scanItemList(&flags, item, &items, 0,
                             "xgap", SDDS_DOUBLE, &plreq->xgap, 1, PLREQ_XGAP,
                             "ygap", SDDS_DOUBLE, &plreq->ygap, 1, PLREQ_YGAP,
                             NULL) ||
             (flags&PLREQ_XGAP && flags&PLREQ_YGAP))
        return bombre("invalid -sever syntax", sever_usage, 0);
    plreq->flags |= flags;
    return 1;
    }

static char *sparse_usage = "-sparse=<interval>[,<offset>]";

long sparse_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (items!=1 && items!=2)
        return bombre("invalid -sparse syntax", sparse_usage, 0);
    if ((sscanf(item[0], "%ld", &plotspec->plot_request[plotspec->plot_requests-1].sparse_interval)!=1) ||
        (plotspec->plot_request[plotspec->plot_requests-1].sparse_interval<=0) ||
        ((items==2) && ((sscanf(item[1], "%ld", &plotspec->plot_request[plotspec->plot_requests-1].sparse_offset)!=1) ||
         (plotspec->plot_request[plotspec->plot_requests-1].sparse_offset<0))))
        return bombre("invalid -sparse syntax", sparse_usage, 0);
    return 1;
    }

static char *presparse_usage = "-presparse=<interval>[,<offset>]";

long presparse_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (items!=1 && items!=2)
        return bombre("invalid -presparse syntax", sparse_usage, 0);
    if ((sscanf(item[0], "%ld", &plotspec->plot_request[plotspec->plot_requests-1].presparse_interval)!=1) ||
        (plotspec->plot_request[plotspec->plot_requests-1].presparse_interval<=0) ||
        ((items==2) && ((sscanf(item[1], "%ld", &plotspec->plot_request[plotspec->plot_requests-1].presparse_offset)!=1) ||
         (plotspec->plot_request[plotspec->plot_requests-1].presparse_offset<0))))
        return bombre("invalid -presparse syntax", presparse_usage, 0);
    return 1;
    }

static char *sample_usage = "-sample=<fraction>";

long sample_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (items!=1)
        return bombre("invalid -sample syntax", sample_usage, 0);
    if (sscanf(item[0], "%lf", &plotspec->plot_request[plotspec->plot_requests-1].sample_fraction)!=1 ||
        plotspec->plot_request[plotspec->plot_requests-1].sample_fraction<=0 )
        return bombre("invalid -sample syntax", sample_usage, 0);
    return 1;
    }

static char *clip_usage = "-clip=<head>,<tail>[,invert]";

long clip_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (items<2 || items>3)
        return bombre("invalid -clip syntax", clip_usage, 0);
    if (sscanf(item[0], "%ld", &plotspec->plot_request[plotspec->plot_requests-1].clip_head)!=1 ||
        plotspec->plot_request[plotspec->plot_requests-1].clip_head<0 ||
        sscanf(item[1], "%ld", &plotspec->plot_request[plotspec->plot_requests-1].clip_tail)!=1 ||
        plotspec->plot_request[plotspec->plot_requests-1].clip_tail<0 )
        return bombre("invalid -clip syntax", clip_usage, 0);
    if (items==3) {
        if (strncmp(item[2], "invert", strlen(item[2]))==0)
            plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_INVERTCLIP;
        else
            return bombre("invalid -clip syntax", clip_usage, 0);
        }
    return 1;
    }

#define OPT_KEEP_NAMES 0
#define OPT_KEEP_FILES 1
#define KEEP_OPTIONS 2
static char *keep_option[KEEP_OPTIONS] = {
    "names", "files",
    } ;
static char *keep_usage = "-keep[={names | files}]";

long keep_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (plotspec->plot_requests<2 ||
        plotspec->plot_request[plotspec->plot_requests-1].mplflags&MPLFILE)
        return bombre("-parameterNames or -columnNames must be given prior to -keep", NULL, 0);
    add_plot_request(plotspec);
    if (items==0) {
        keepnames_AP1(plotspec);
        keepfilenames_AP1(plotspec);
        }
    else {
        while (items--) {
            switch (match_string(item[items], keep_option, KEEP_OPTIONS, 0)) {
              case OPT_KEEP_NAMES:
                keepnames_AP1(plotspec);
                break;
              case OPT_KEEP_FILES:
                keepfilenames_AP1(plotspec);
                break;
              default:
                return bombre("invalid -keep keyword", keep_usage, 0);
                }
            }
        }
    return 1;
    }

long keepnames_AP1(PLOT_SPEC *plotspec)
{
    PLOT_REQUEST *plreq, *pllreq;
    long i;

    pllreq = &plotspec->plot_request[plotspec->plot_requests-2];
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    plreq->datanames = pllreq->datanames;
    plreq->xname = SDDS_Realloc(plreq->xname, sizeof(*plreq->xname)*plreq->datanames);
    plreq->yname = SDDS_Realloc(plreq->yname, sizeof(*plreq->yname)*plreq->datanames);
    plreq->x1name = SDDS_Realloc(plreq->x1name, sizeof(*plreq->x1name)*plreq->datanames);
    plreq->y1name = SDDS_Realloc(plreq->y1name, sizeof(*plreq->y1name)*plreq->datanames);
    plreq->was_wildname = SDDS_Realloc(plreq->was_wildname, sizeof(*plreq->was_wildname)*plreq->datanames);

    for (i=0; i<plreq->datanames; i++) {
        if (plreq->xname[i])
          free(plreq->xname[i]);
        SDDS_CopyString(plreq->xname+i, pllreq->xname[i]);
        if (plreq->yname[i])
          free(plreq->yname[i]);
        SDDS_CopyString(plreq->yname+i, pllreq->yname[i]);
        if (plreq->x1name[i])
          free(plreq->x1name[i]);
        if (plreq->x1name[i]) 
            SDDS_CopyString(plreq->x1name+i, pllreq->x1name[i]);
        if (plreq->y1name[i])
          free(plreq->y1name[i]);
        if (plreq->y1name[i])
            SDDS_CopyString(plreq->y1name+i, pllreq->y1name[i]);
        plreq->was_wildname[i] = 0;
        }
    return 1;
    }

long keepfilenames_AP1(PLOT_SPEC *plotspec)
{
    PLOT_REQUEST *plreq, *pllreq;
    long i;

    pllreq = &plotspec->plot_request[plotspec->plot_requests-2];
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    plreq->filename = tmalloc(sizeof(*plreq->filename)*pllreq->filenames);
    for (i=0; i<pllreq->filenames; i++) 
        SDDS_CopyString(plreq->filename+i, pllreq->filename[i]);
    plreq->filenames = pllreq->filenames;
    return 1;
    }

static char *filter_usage = " -filter={`column' | `parameter'},<range-spec>[,<range-spec>[,<logic-operation>...]\n\
A <logic-operation> is one of & (logical and) or | (logical or), optionally followed by a ! to \n\
logically negate the value of the expression.\n\
A <range-spec> is of the form <name>,<lower-value>,<upper-value>[,!].\n";

long filter_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    plreq->filter = (FILTER_DEFINITION**)SDDS_Realloc(plreq->filter, 
                                                      sizeof(*plreq->filter)*(plreq->filters+1));
    if (!(plreq->filter[plreq->filters]=process_new_filter_definition(item, items)))
        return bombre("invalid -filter syntax", filter_usage, 0);
    plreq->filters ++;
    return 1;
    }

static char *time_filter_usage = " -timeFilter={`column' | `parameter'},<name>[,before=YYYY/MM/DD@HH:MM:SS][,after=YYYY/MM/DD@HH:MM:SS][,invert]";

long time_filter_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  PLOT_REQUEST *plreq;
  plreq = &plotspec->plot_request[plotspec->plot_requests-1];
  plreq->time_filter = (TIME_FILTER_DEFINITION**)SDDS_Realloc(plreq->time_filter, 
                                                    sizeof(*plreq->time_filter)*(plreq->time_filters+1));
  if (!(plreq->time_filter[plreq->time_filters]=process_new_time_filter_definition(item, items)))
    return bombre("invalid -timeFilter syntax", time_filter_usage, 0);
  plreq->time_filters ++;
  return 1;
}

static char *match_usage = " -match={`column' | `parameter'},<match-test>[,<match-test>[,<logic-operation>...]\n\
A <match-test> is of the form <name>=<matching-string>[,!], where ! signifies logical negation.\n\
A <logic-operation> is one of & (logical and) or | (logical or), optionally followed by a ! to \n\
logically negate the value of the expression.\n";

long match_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    plreq->match = (MATCH_DEFINITION**)SDDS_Realloc(plreq->match, 
                                                    sizeof(*plreq->match)*(plreq->matches+1));
    if (!(plreq->match[plreq->matches]=process_new_match_definition(item, items)))
        return bombre("invalid -match syntax", match_usage, 0);
    plreq->matches ++;
    return 1;
    }


long drawline_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  DRAW_LINE_SPEC *drawLineSpec;
  long drawLineSpecs;
  static char *drawlineUsage = "-drawLine=\
{x0value=<value> | p0value=<value> | x0parameter=<name> | p0parameter=<name>},\
{x1value=<value> | p1value=<value> | x1parameter=<name> | p1parameter=<name>},\
{y0value=<value> | q0value=<value> | y0parameter=<name> | q0parameter=<name>},\
{y1value=<value> | q1value=<value> | y1parameter=<name> | q1parameter=<name>}\
[,linetype=<integer>][,thickness=<integer>][,clip]";
  
  drawLineSpec = plotspec->plot_request[plotspec->plot_requests-1].drawLineSpec;
  drawLineSpecs = plotspec->plot_request[plotspec->plot_requests-1].drawLineSpecs;
  if (!(drawLineSpec=SDDS_Realloc(drawLineSpec, sizeof(*drawLineSpec)*(drawLineSpecs+1)))) {
    return bombre("allocation failure in drawline_AP", NULL, 0);
  }
  drawLineSpec[drawLineSpecs].linethickness = 0;
  if (!scanItemList(&drawLineSpec[drawLineSpecs].flags, item, &items, 0,
		    "x0value", SDDS_DOUBLE, &drawLineSpec[drawLineSpecs].x0, 1, DRAW_LINE_X0GIVEN,
                    "y0value", SDDS_DOUBLE, &drawLineSpec[drawLineSpecs].y0, 1, DRAW_LINE_Y0GIVEN,
                    "p0value", SDDS_DOUBLE, &drawLineSpec[drawLineSpecs].p0, 1, DRAW_LINE_P0GIVEN,
		    "q0value", SDDS_DOUBLE, &drawLineSpec[drawLineSpecs].q0, 1, DRAW_LINE_Q0GIVEN,
                    "x1value", SDDS_DOUBLE, &drawLineSpec[drawLineSpecs].x1, 1, DRAW_LINE_X1GIVEN,
                    "y1value", SDDS_DOUBLE, &drawLineSpec[drawLineSpecs].y1, 1, DRAW_LINE_Y1GIVEN,
                    "p1value", SDDS_DOUBLE, &drawLineSpec[drawLineSpecs].p1, 1, DRAW_LINE_P1GIVEN,
                    "q1value", SDDS_DOUBLE, &drawLineSpec[drawLineSpecs].q1, 1, DRAW_LINE_Q1GIVEN,
                    "x0parameter", SDDS_STRING, &drawLineSpec[drawLineSpecs].x0Param, 1, DRAW_LINE_X0PARAM,
                    "y0parameter", SDDS_STRING, &drawLineSpec[drawLineSpecs].y0Param, 1, DRAW_LINE_Y0PARAM,
                    "p0parameter", SDDS_STRING, &drawLineSpec[drawLineSpecs].p0Param, 1, DRAW_LINE_P0PARAM,
                    "q0parameter", SDDS_STRING, &drawLineSpec[drawLineSpecs].q0Param, 1, DRAW_LINE_Q0PARAM,
                    "x1parameter", SDDS_STRING, &drawLineSpec[drawLineSpecs].x1Param, 1, DRAW_LINE_X1PARAM,
                    "y1parameter", SDDS_STRING, &drawLineSpec[drawLineSpecs].y1Param, 1, DRAW_LINE_Y1PARAM,
                    "p1parameter", SDDS_STRING, &drawLineSpec[drawLineSpecs].p1Param, 1, DRAW_LINE_P1PARAM,
                    "q1parameter", SDDS_STRING, &drawLineSpec[drawLineSpecs].q1Param, 1, DRAW_LINE_Q1PARAM,
                    "linetype", SDDS_LONG, &drawLineSpec[drawLineSpecs].linetype, 1, DRAW_LINE_LINETYPEGIVEN, 
                    "thickness", SDDS_LONG, &drawLineSpec[drawLineSpecs].linethickness, 1, 0, 
                    "clip", -1, NULL, 0, DRAW_LINE_CLIPGIVEN,
                    NULL)) 
    return bombre("invalid -drawline syntax", drawlineUsage, 0);
  if (bitsSet(drawLineSpec[drawLineSpecs].flags&
              (DRAW_LINE_X0GIVEN+DRAW_LINE_P0GIVEN+DRAW_LINE_X0PARAM+DRAW_LINE_P0PARAM))!=1 ||
      bitsSet(drawLineSpec[drawLineSpecs].flags&
              (DRAW_LINE_Y0GIVEN+DRAW_LINE_Q0GIVEN+DRAW_LINE_Y0PARAM+DRAW_LINE_Q0PARAM))!=1 ||
      bitsSet(drawLineSpec[drawLineSpecs].flags&
              (DRAW_LINE_X1GIVEN+DRAW_LINE_P1GIVEN+DRAW_LINE_X1PARAM+DRAW_LINE_P1PARAM))!=1 ||
      bitsSet(drawLineSpec[drawLineSpecs].flags&
              (DRAW_LINE_Y1GIVEN+DRAW_LINE_Q1GIVEN+DRAW_LINE_Y1PARAM+DRAW_LINE_Q1PARAM))!=1)
    return bombre("invalid -drawline syntax", drawlineUsage, 0);
  plotspec->plot_request[plotspec->plot_requests-1].drawLineSpec = drawLineSpec;
  plotspec->plot_request[plotspec->plot_requests-1].drawLineSpecs += 1;
  if (drawLineSpec[drawLineSpecs].linethickness < 0)
    drawLineSpec[drawLineSpecs].linethickness = 0;
  if (drawLineSpec[drawLineSpecs].linethickness >= 10)
    drawLineSpec[drawLineSpecs].linethickness = 9;
  return 1;
}


long swap_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (plotspec->plot_requests<1)
        return bombre(NO_REQUESTS_MESSAGE, "-parameterNames, -columnNames or -mplfiles must be given prior to -swap", 0);
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_SWAP;
    return 1;
    }

long showlinkdate_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    void put_link_date(FILE *fp);
    put_link_date(stderr);
    return 0;
    }

long transpose_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (plotspec->plot_requests<1)
        return bombre(NO_REQUESTS_MESSAGE, "-parameterNames, -columnNames or -mplfiles must be given prior to -transpose", 0);
    plotspec->plot_request[plotspec->plot_requests-1].flags |= PLREQ_TRANSPOSE;
    return 1;
    }

#define LINETYPEDEFAULT_KW_THICKNESS 0
#define LINETYPEDEFAULT_KWS 1
static char *linetypedefault_kw[LINETYPEDEFAULT_KWS] = {
  "thickness"
};
long linetypedefault_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    int i;
    char *eqptr;
    PLOT_REQUEST *plreq;
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    if ((items < 1) ||
	(sscanf(item[0], "%ld", &plreq->linetype_default)!=1) ||
	(plreq->linetype_default<0))
        return bombre("invalid -linetypedefault syntax", "-linetypedefault=<integer>[,thickness=<value>]", 0);
    for (i=1; i<items; i++) {
      if ((eqptr=strchr(item[i], '=')))
	*eqptr = 0;
      switch (match_string(item[i], linetypedefault_kw, LINETYPEDEFAULT_KWS, 0)) {
      case LINETYPEDEFAULT_KW_THICKNESS:
	if (!eqptr || SDDS_StringIsBlank(eqptr+1) || sscanf(eqptr+1, "%ld", &plreq->linethickness_default)!=1)
	  return bombre("invalid -linetypedefault syntax", "-linetypedefault=<integer>[,thickness=<value>]", 0);
	if (plreq->linethickness_default <= 0)
	  plreq->linethickness_default = 1;
	if (plreq->linethickness_default >= 10)
	  plreq->linethickness_default = 9;
	defaultLineThickness = plreq->linethickness_default;
	break;
      default:
	return bombre("invalid -linetypedefault syntax", "-linetypedefault=<integer>[,thickness=<value>]", 0);
      }
    }
    return 1;
    }


static char *ticksettings_usage = "-ticksettings=[{xy}spacing=<value>][,[{xy}]grid][,[{xy}]linetype=<integer>][,[{xy}]thickness=<integer>][,{xy}size=<fraction>][{xy}modulus=<value>][,[{xy}]logarithmic][,{xy}factor=<value>][,{xy}offset=<value>][,{xy}time][,{xy}nonExponentialLabels][,{xy}invert][,{xy}scaleChars=<value>][,[{xy}]labelThickness=<integer>]\n";

long ticksettings_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    TICK_SETTINGS *tset;
    double fraction, scaleChar=0;
    int32_t linetype;
    int32_t thickness;
    int32_t labelThickness=0;
    unsigned long long oldFlags;
    
    if (items<1)
        return bombre("invalid -ticksettings syntax", ticksettings_usage, 0);

    tset = &plotspec->plot_request[plotspec->plot_requests-1].tick_settings;
    if (isdigit(item[0][0])) {
        if (items!=2 ||
            sscanf(item[0], "%lf", tset->spacing+0)!=1  ||
            sscanf(item[1], "%lf", tset->spacing+1)!=1 )
            return bombre("invalid -ticksettings syntax", ticksettings_usage, 0);
        tset->flags = TICKSET_GIVEN|TICKSET_XSPACING|TICKSET_YSPACING;
        return 1;
        }
    tset->spacing[0] = tset->spacing[1] = 0;
    tset->modulus[0] = tset->modulus[1] = 0;
    tset->linetype[0] = tset->linetype[1] = 0;
    tset->factor[0] = tset->factor[1] = 1;
    tset->fraction[0] = 0.02;
    tset->fraction[1] = 0.01;
    oldFlags = tset->flags;
    if (!scanItemListLong(&tset->flags,
                        item, &items,  0,
                        "xspacing", SDDS_DOUBLE, tset->spacing+0, 1, TICKSET_XSPACING,
                        "yspacing", SDDS_DOUBLE, tset->spacing+1, 1, TICKSET_YSPACING,
                        "xmodulus", SDDS_DOUBLE, tset->modulus+0, 1, TICKSET_XMODULUS,
                        "ymodulus", SDDS_DOUBLE, tset->modulus+1, 1, TICKSET_YMODULUS,
                        "xgrid", -1, NULL, 0, TICKSET_XGRID,
                        "ygrid", -1, NULL, 0, TICKSET_YGRID,
                        "grid", -1, NULL, 0, TICKSET_XGRID|TICKSET_YGRID,
                        "xlogarithmic", -1, NULL, 0, TICKSET_XLOGARITHMIC,
                        "ylogarithmic", -1, NULL, 0, TICKSET_YLOGARITHMIC,
                        "logarithmic", -1, NULL, 0, TICKSET_XLOGARITHMIC|TICKSET_YLOGARITHMIC,
                        "xlinetype", SDDS_LONG, tset->linetype+0, 1, TICKSET_XLINETYPE,
                        "ylinetype", SDDS_LONG, tset->linetype+1, 1, TICKSET_YLINETYPE,
                        "linetype", SDDS_LONG, &linetype, 1, TICKSET_LINETYPE,
                        "xthickness", SDDS_LONG, tset->thickness+0, 1, TICKSET_XTHICKNESS,
                        "ythickness", SDDS_LONG, tset->thickness+1, 1, TICKSET_YTHICKNESS,
                        "thickness", SDDS_LONG, &thickness, 1, TICKSET_THICKNESS,
                        "xsize", SDDS_DOUBLE, tset->fraction+0, 1, TICKSET_XFRACTION,
                        "ysize", SDDS_DOUBLE, tset->fraction+1, 1, TICKSET_YFRACTION,
                        "size", SDDS_DOUBLE, &fraction, 1, TICKSET_FRACTION,
                        "xfactor", SDDS_DOUBLE, tset->factor+0, 1, TICKSET_XFACTOR,
                        "yfactor", SDDS_DOUBLE, tset->factor+1, 1, TICKSET_YFACTOR,
                        "xoffset", SDDS_DOUBLE, tset->offset+0, 1, TICKSET_XOFFSET,
                        "yoffset", SDDS_DOUBLE, tset->offset+1, 1, TICKSET_YOFFSET,
                        "xtime", -1, NULL, 0, TICKSET_XTIME,
                        "ytime", -1, NULL, 0, TICKSET_YTIME,
                        "xnonexponentiallabels", -1, NULL, 0, TICKSET_XNONEXPLABELS,
                        "ynonexponentiallabels", -1, NULL, 0, TICKSET_YNONEXPLABELS,
                        "xinvert", -1, NULL, 0, TICKSET_XINVERT,
                        "yinvert", -1, NULL, 0, TICKSET_YINVERT,
                        "xscalechars", SDDS_DOUBLE, tset->scaleChar+0, 1, TICKSET_XSCALECHAR,
                        "yscalechars", SDDS_DOUBLE, tset->scaleChar+1, 1, TICKSET_YSCALECHAR,
                        "scalechars", SDDS_DOUBLE, &scaleChar, 1, 0ULL,
		        "ylabelthickness", SDDS_LONG, tset->labelThickness+1, 1, 0ULL,
		        "xlabelthickness", SDDS_LONG, tset->labelThickness+0, 1, 0ULL,
		        "labelthickness", SDDS_LONG, &labelThickness, 1, 0ULL,
                        NULL) ||
        (tset->flags&TICKSET_XMODULUS && tset->modulus[0]<=0) ||
        (tset->flags&TICKSET_YMODULUS && tset->modulus[1]<=0) ||
        (tset->flags&TICKSET_XFACTOR && tset->factor[0]<=0) ||
        (tset->flags&TICKSET_YFACTOR && tset->factor[1]<=0) ||
        (tset->flags&TICKSET_XSCALECHAR && tset->scaleChar[0]<0) ||
        (tset->flags&TICKSET_YSCALECHAR && tset->scaleChar[1]<0))
        return bombre("invalid -ticksettings syntax", ticksettings_usage, 0);
    tset->flags |= oldFlags;
    if (scaleChar>0) {
      if (!(tset->flags&TICKSET_XSCALECHAR))
	tset->scaleChar[0] = scaleChar;
      if (!(tset->flags&TICKSET_YSCALECHAR))
	tset->scaleChar[1] = scaleChar;
      tset->flags |= TICKSET_XSCALECHAR+TICKSET_YSCALECHAR;
    }
    if (tset->flags&TICKSET_LINETYPE) {
        if (!(tset->flags&TICKSET_XLINETYPE))
            tset->linetype[0] = linetype;
        if (!(tset->flags&TICKSET_YLINETYPE))
            tset->linetype[1] = linetype;
        tset->flags |= TICKSET_XLINETYPE|TICKSET_YLINETYPE;
        }
    if (tset->flags&TICKSET_XTHICKNESS) {
      if (tset->thickness[0] < 0)
	tset->thickness[0] = 0;
      if (tset->thickness[0] > 10)
	tset->thickness[0] = 9;
    }
    if (tset->flags&TICKSET_YTHICKNESS) {
      if (tset->thickness[1] < 0)
	tset->thickness[1] = 0;
      if (tset->thickness[1] > 10)
	tset->thickness[1] = 9;
    }
    if (tset->flags&TICKSET_THICKNESS) {
      if (thickness < 0)
	thickness = 0;
      if (thickness > 10)
	thickness = 9;
      if (!(tset->flags&TICKSET_XTHICKNESS))
	tset->thickness[0] = thickness;
      if (!(tset->flags&TICKSET_YTHICKNESS))
            tset->thickness[1] = thickness;
      tset->flags |= TICKSET_XTHICKNESS|TICKSET_YTHICKNESS;
    }
    if (tset->labelThickness[0] < 0)
      tset->labelThickness[0] = 0;
    if (tset->labelThickness[0] > 10)
      tset->labelThickness[0] = 9;
    if (tset->labelThickness[1] < 0)
      tset->labelThickness[1] = 0;
    if (tset->labelThickness[1] > 10)
      tset->labelThickness[1] = 9;
    if (labelThickness < 0)
      labelThickness = 0;
    if (labelThickness > 10)
      labelThickness = 9;
    if ((labelThickness != 0) && (tset->labelThickness[0] == 0))
      tset->labelThickness[0] = labelThickness;
    if ((labelThickness != 0) && (tset->labelThickness[1] == 0))
      tset->labelThickness[1] = labelThickness;
    if (tset->flags&TICKSET_FRACTION) {
        if (!(tset->flags&TICKSET_XFRACTION))
            tset->fraction[0] = fraction;
        if (!(tset->flags&TICKSET_YFRACTION))
            tset->fraction[1] = fraction;
        tset->flags |= TICKSET_XFRACTION|TICKSET_YFRACTION;
        }
    tset->flags |= TICKSET_GIVEN;
    return 1;
    }

long labelsize_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    if (items!=1 || sscanf(item[0], "%lf", &plreq->labelsize_fraction)!=1 ||
        plreq->labelsize_fraction<=0)
        return bombre("invalid -labelsize syntax", "-labelsize=<fraction>", 0);
    return 1;
    }


long enumeratedscales_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;

    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    if (items<1 ||
        !scanItemList(&plreq->enumerate_settings.flags,
                        item, &items,  0,
                        "rotate", -1, NULL, 0, ENUM_ROTATE,
                        "scale", SDDS_DOUBLE, &plreq->enumerate_settings.scale, 1, ENUM_SCALEGIVEN,
                        "interval", SDDS_LONG, &plreq->enumerate_settings.interval, 1, ENUM_INTERVALGIVEN,
                        "limit", SDDS_LONG, &plreq->enumerate_settings.limit, 1, ENUM_LIMITGIVEN,
                        "allticks", -1, NULL, 0, ENUM_ALLTICKS,
                        "editcommand", SDDS_STRING, &plreq->enumerate_settings.editcommand, 1, ENUM_EDITCOMMANDGIVEN,
                        NULL) ||
        (plreq->enumerate_settings.flags&ENUM_INTERVALGIVEN && plreq->enumerate_settings.interval<=0) ||
        (plreq->enumerate_settings.flags&ENUM_LIMITGIVEN && plreq->enumerate_settings.limit<=0) ||
        (plreq->enumerate_settings.flags&ENUM_SCALEGIVEN && plreq->enumerate_settings.scale<=0) )
        return bombre("invalid -enumeratedscales syntax",
                      "-enumeratedscales=[interval=<integer>][,limit=<integer>][,scale=<factor>][,allTicks][,rotate][,editcommand=<string>]", 0);
    return 1;
    }


char *arrowsettings_usage = "-arrowsettings=[scale=<value>][,barblength=<value>][,barbangle=<deg>][,linetype=<number>][,centered][,cartesiandata[,endpoints]][,polardata][,scalardata][,singlebarb][,autoscale][,thickness=<number>]";

long arrowsettings_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    ARROW_SETTINGS *arrow;

    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    arrow = &plreq->graphic.arrow;
    plreq->graphic.element = PLOT_ARROW;
    arrow->linetype = plreq->graphic.type;

    /*
      if (plreq->datanames>1) 
      bomb("can't have multiple datanames for arrow plotting", NULL);
      */

    if (items<1 ||
        !scanItemList(&arrow->flags,
                        item, &items,  0,
                        "centered", -1, NULL, 0, ARROW_CENTERED,
                        "scale", SDDS_DOUBLE, &arrow->scale, 1, ARROW_SCALE_GIVEN,
                        "barblength", SDDS_DOUBLE, &arrow->barbLength, 1, ARROW_BARBLENGTH_GIVEN,
                        "barbangle", SDDS_DOUBLE, &arrow->barbAngle, 1, ARROW_BARBANGLE_GIVEN,
                        "linetype", SDDS_LONG, &arrow->linetype, 1, ARROW_LINETYPE_GIVEN,
                        "thickness", SDDS_LONG, &arrow->thickness, 1, ARROW_THICKNESS_GIVEN,
                        "cartesiandata", -1, NULL, 0, ARROW_CARTESIAN_DATA,
                        "polardata", -1, NULL, 0, ARROW_POLAR_DATA,
                        "scalardata", -1, NULL, 0, ARROW_SCALAR_DATA,
                        "singlebarb", -1, NULL, 0, ARROW_SINGLEBARB,
                        "autoscale", -1, NULL, 0, ARROW_AUTOSCALE,
                        "endpoints", -1, NULL, 0, ARROW_ENDPOINTS,
                        NULL))
        return bombre("invalid -arrowsettings syntax", arrowsettings_usage, 0);
    if (arrow->flags&ARROW_ENDPOINTS) {
        if (arrow->flags&ARROW_POLAR_DATA || arrow->flags&ARROW_SCALAR_DATA)
            return bombre("invalid -arrowsettings syntax---endpoints keyword not compatible with polar or scalar keywords",
                          NULL, 0);
        if (arrow->flags&ARROW_AUTOSCALE)
            return bombre("invalid -arrowsettings syntax---endpoints keyword not compatible with autoscale keyword", NULL, 0);
        arrow->flags |= ARROW_CARTESIAN_DATA;
        }
    return 1;
    }


long listDevices_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  list_terms(stderr);
  exit(1);
  return(1);
}


long limit_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    LIMIT_SETTINGS defaultLimits;
    
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    defaultLimits = plreq->limit;
    plreq->limit.flags = 0;
    if (items<1 ||
        !scanItemList(&plreq->limit.flags, item, &items, 0,
                        "xminimum", SDDS_DOUBLE, &plreq->limit.xmin, 1, LIMIT_XMIN_GIVEN,
                        "yminimum", SDDS_DOUBLE, &plreq->limit.ymin, 1, LIMIT_YMIN_GIVEN,
                        "xmaximum", SDDS_DOUBLE, &plreq->limit.xmax, 1, LIMIT_XMAX_GIVEN,
                        "ymaximum", SDDS_DOUBLE, &plreq->limit.ymax, 1, LIMIT_YMAX_GIVEN,
                        "autoscaling", -1, NULL, 0, LIMIT_AUTOSCALING,
                        NULL))
        return bombre("invalid -limit syntax", "-limit=[{x| y}{min | max}imum=<value>][,autoscaling]", 0);
    if (defaultLimits.flags&LIMIT_XMIN_GIVEN && !(plreq->limit.flags&LIMIT_XMIN_GIVEN)) {
        plreq->limit.flags |= LIMIT_XMIN_GIVEN;
        plreq->limit.xmin = defaultLimits.xmin;
        }
    if (defaultLimits.flags&LIMIT_XMAX_GIVEN && !(plreq->limit.flags&LIMIT_XMAX_GIVEN)) {
        plreq->limit.flags |= LIMIT_XMAX_GIVEN;
        plreq->limit.xmax = defaultLimits.xmax;
        }
    if (defaultLimits.flags&LIMIT_YMIN_GIVEN && !(plreq->limit.flags&LIMIT_YMIN_GIVEN)) {
        plreq->limit.flags |= LIMIT_YMIN_GIVEN;
        plreq->limit.ymin = defaultLimits.ymin;
        }    
    if (defaultLimits.flags&LIMIT_YMAX_GIVEN && !(plreq->limit.flags&LIMIT_YMAX_GIVEN)) {
        plreq->limit.flags |= LIMIT_YMAX_GIVEN;
        plreq->limit.ymax = defaultLimits.ymax;
        }
    return 1;
    }

long intensityBar_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  PLOT_REQUEST *plreq;

  plreq = &plotspec->plot_request[plotspec->plot_requests-1];
  plreq->intensityBar_settings.flags = 0;
  if (items<1 ||
      !scanItemList(&plreq->intensityBar_settings.flags, item, &items, 0,
		    "text", SDDS_STRING, &plreq->intensityBar_settings.text, 1, INTENSITYBAR_TEXT_GIVEN,
                    "labelsize", SDDS_DOUBLE, &plreq->intensityBar_settings.labelsize, 1, INTENSITYBAR_LABELSIZE_GIVEN,
                    "unitsize", SDDS_DOUBLE, &plreq->intensityBar_settings.unitsize, 1, INTENSITYBAR_UNITSIZE_GIVEN,
                    "xadjust", SDDS_DOUBLE, &plreq->intensityBar_settings.xadjust, 1, INTENSITYBAR_XADJUST_GIVEN,
                    NULL))
    return bombre("invalid -intensityBar syntax", "-intensityBar=[labelsize=<value>][,unitsize=<value>][,xadjust=<value>]\nThe defaults are -intensityBar=labelsize=1,unitsize=1,xadjust=0", 0);
  
  return 1;
}


long range_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    plreq->range.flags = 0;
    if (items<1 ||
        !scanItemList(&plreq->range.flags, item, &items, 0,
                        "xminimum", SDDS_DOUBLE, plreq->range.minimum+0, 1, XRANGE_MINIMUM,
                        "xmaximum", SDDS_DOUBLE, plreq->range.maximum+0, 1, XRANGE_MAXIMUM,
                        "xcenter" , SDDS_DOUBLE, plreq->range.center+0 , 1, XRANGE_CENTER,
                        "yminimum", SDDS_DOUBLE, plreq->range.minimum+1, 1, YRANGE_MINIMUM,
                        "ymaximum", SDDS_DOUBLE, plreq->range.maximum+1, 1, YRANGE_MAXIMUM,
                        "ycenter" , SDDS_DOUBLE, plreq->range.center+1 , 1, YRANGE_CENTER,
                        NULL))
        return bombre("invalid -range syntax", "-range=[{x| y}{minimum | maximum | center}=<value>]", 0);
    return 1;
    }


long namescan_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    plreq->nameScanFlags = 0;
    if (items<1 ||
        !scanItemList(&plreq->nameScanFlags, item, &items, 0,
			"all", -1, NULL, 0, NAMESCAN_ALL,
			"first", -1, NULL, 0, NAMESCAN_FIRST,
                        NULL) ||
	!plreq->nameScanFlags || 
	(plreq->nameScanFlags&NAMESCAN_ALL && plreq->nameScanFlags&NAMESCAN_FIRST))
        return bombre("invalid -namescan syntax", "-namescan={all | first}", 0);
    return 1;
    }

static char *pointLabel_usage = "-pointlabel=<name>[,edit=<editCommand>][,scale=<number>][,justifyMode={rcl}{bct}][,thickness=<integer>[,lineType=<integer>]][,vertical]";

long pointlabel_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    PLOT_REQUEST *plreq;
    
    if (items<1) 
      return bombre("invalid -pointLabel syntax", pointLabel_usage, 0);
    
    plreq = &plotspec->plot_request[plotspec->plot_requests-1];
    SDDS_CopyString(&plreq->pointLabelSettings.name, item[0]);
    item++;
    items--;
    plreq->pointLabelSettings.editCommand = plreq->pointLabelSettings.justifyMode = NULL;
    plreq->pointLabelSettings.scale = 1;
    plreq->pointLabelSettings.thickness = 1;
    
    if (!scanItemList(&plreq->pointLabelSettings.flags, item, &items, 0,
                      "edit", SDDS_STRING, &plreq->pointLabelSettings.editCommand, 1, 0,
                      "scale", SDDS_DOUBLE, &plreq->pointLabelSettings.scale, 1, 0,
                      "thickness", SDDS_LONG, &plreq->pointLabelSettings.thickness, 1, 0,
                      "justify", SDDS_STRING, &plreq->pointLabelSettings.justifyMode, 1, 0,
                      "linetype",SDDS_LONG, &plreq->pointLabelSettings.linetype, 1, POINTLABEL_LINETYPE_GIVEN,
		      "vertical", -1, NULL, 1, POINTLABEL_VERTICAL,
                      NULL) ||
        (plreq->pointLabelSettings.justifyMode && strlen(plreq->pointLabelSettings.justifyMode)!=2))
      return bombre("invalid -pointLabel syntax", pointLabel_usage, 0);
    return 1;
    }

static char *replicate_usage = "-replicate={number=<integer> | match={names | pages | requests | files}}[,scroll]";

long replicate_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  PLOT_REQUEST *plreq;
  char *matchMode;
  static char *matchName[4] = {"names", "pages", "requests", "files" };
  
  if (items<1)
    return bombre("invalid -replicate syntax", replicate_usage, 0);
  
  plreq = &plotspec->plot_request[plotspec->plot_requests-1];
  if (!scanItemList(&plreq->replicateSettings.flags, item, &items, 0, 
                    "number", SDDS_LONG, &plreq->replicateSettings.number, 1, REPLICATE_NUMBER,
                    "match", SDDS_STRING, &matchMode, 1, REPLICATE_MATCH,
                    "scroll", -1, NULL, 0, REPLICATE_SCROLL_MODE,
                    NULL) || !plreq->replicateSettings.flags ||
      (plreq->replicateSettings.flags&REPLICATE_NUMBER &&
      plreq->replicateSettings.flags&REPLICATE_MATCH ))
    return bombre("invalid -replicate syntax", replicate_usage, 0);
  
  if (plreq->replicateSettings.flags&REPLICATE_MATCH) {
    switch (match_string(matchMode, matchName, 4, 0)) {
    case 0:
      plreq->replicateSettings.flags |= REPLICATE_MATCH_NAMES;
      break;
    case 1:
      plreq->replicateSettings.flags |= REPLICATE_MATCH_PAGES;
      break;
    case 2:
      plreq->replicateSettings.flags |= REPLICATE_MATCH_REQUESTS;
      break;
    case 3:
      plreq->replicateSettings.flags |= REPLICATE_MATCH_FILES;
      break;
    default:
      return bombre("invalid -replicate sytnax: unknown/ambiguous match mode", replicate_usage, 0);
    }
  }
  free(matchMode);
  return 1;
}

long alignzero_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  PLOT_REQUEST *plreq;
  unsigned long flags;
  long plane;
  
  plreq = &plotspec->plot_request[plotspec->plot_requests-1];
  
  if (!scanItemList(&flags, item, &items, 0,
                    "xcenter", -1, NULL, 0, ALIGNZERO_XCENTER_GIVEN,
                    "ycenter", -1, NULL, 0, ALIGNZERO_YCENTER_GIVEN,
                    "xfactor", -1, NULL, 0, ALIGNZERO_XFACTOR_GIVEN,
                    "yfactor", -1, NULL, 0, ALIGNZERO_YFACTOR_GIVEN,
                    "ppin", SDDS_DOUBLE, &plreq->alignSettings.pinUnitSpace[0], 1, ALIGNZERO_PPIN_GIVEN, 
                    "qpin", SDDS_DOUBLE, &plreq->alignSettings.pinUnitSpace[1], 1, ALIGNZERO_QPIN_GIVEN, 
                    NULL) || !flags)
    return bombre("invalid -alignZero syntax", "-alignZero[={xcenter|xfactor|pPin=<value>}][,{ycenter|yfactor|qPin=<value>}]", 0);
  for (plane=0; plane<2; plane++) {
    if (flags&(ALIGNZERO_XCENTER_GIVEN<<plane)) {
      plreq->alignSettings.pinUnitSpace[0] = 0.5;
      flags &= ~((ALIGNZERO_XCENTER_GIVEN<<plane));
      flags |= (ALIGNZERO_PPIN_GIVEN<<plane);
    }
  }
  plreq->alignSettings.flags = flags;
  
  return 1;
}

static char *orderColors_usage = "-orderColors={temperature|rtemperature|spectral|rspectral|start=(<red>,<green>,<blue>){[,finish=(<red>,<green>,<blue>)]|[,increment=(<red>,<green>,<blue>)]}}\n\
All colors range from 0 to 65535\n";

#define ORDERCOLORS_KW_START 0
#define ORDERCOLORS_KW_FINISH 1
#define ORDERCOLORS_KW_INCREMENT 2
#define ORDERCOLORS_KW_SPECTRAL 3
#define ORDERCOLORS_KW_RSPECTRAL 4
#define ORDERCOLORS_KW_TEMPERATURE 5
#define ORDERCOLORS_KW_RTEMPERATURE 6
#define ORDERCOLORS_KWS 7
static char *ordercolors_kw[ORDERCOLORS_KWS] = {
  "start", "finish", "increment", "spectral", "rspectral", "temperature", "rtemperature"
};
long orderColors_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  int i;
  char *eqptr, *commaptr;
  PLOT_REQUEST *plreq;
  plreq = &plotspec->plot_request[plotspec->plot_requests-1];

  if (items < 1) {
    fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
    return 0;
  }
  for (i=0; i<items; i++) {
    if ((eqptr=strchr(item[i], '=')))
      *eqptr = 0;
    switch (match_string(item[i], ordercolors_kw, ORDERCOLORS_KWS, 0)) {
    case ORDERCOLORS_KW_START:
      if (eqptr == NULL) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if ((commaptr=strchr(eqptr+1, ','))) {
	*commaptr = 0;
      } else {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if (!eqptr || SDDS_StringIsBlank(eqptr+1)) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      plreq->color_settings.red[0] = (unsigned short)atol(eqptr+1);
      eqptr = commaptr;
      if ((commaptr=strchr(eqptr+1, ','))) {
	*commaptr = 0;
      } else {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if (!eqptr || SDDS_StringIsBlank(eqptr+1)) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      plreq->color_settings.green[0] = (unsigned short)atol(eqptr+1);
      eqptr = commaptr;
      if ((commaptr=strchr(eqptr+1, ','))) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if (!eqptr || SDDS_StringIsBlank(eqptr+1)) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      plreq->color_settings.blue[0] = (unsigned short)atol(eqptr+1);
      plreq->color_settings.flags |= COLORSET_START;
      break;
    case ORDERCOLORS_KW_FINISH:
      if (eqptr == NULL) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if ((commaptr=strchr(eqptr+1, ','))) {
	*commaptr = 0;
      } else {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if (!eqptr || SDDS_StringIsBlank(eqptr+1)) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      plreq->color_settings.red[1] = (unsigned short)atol(eqptr+1);
      eqptr = commaptr;
      if ((commaptr=strchr(eqptr+1, ','))) {
	*commaptr = 0;
      } else {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if (!eqptr || SDDS_StringIsBlank(eqptr+1)) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      plreq->color_settings.green[1] = (unsigned short)atol(eqptr+1);
      eqptr = commaptr;
      if ((commaptr=strchr(eqptr+1, ','))) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if (!eqptr || SDDS_StringIsBlank(eqptr+1)) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      plreq->color_settings.blue[1] = (unsigned short)atol(eqptr+1);
      plreq->color_settings.flags |= COLORSET_FINISH;
      break;
    case ORDERCOLORS_KW_INCREMENT:
      if (eqptr == NULL) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if ((commaptr=strchr(eqptr+1, ','))) {
	*commaptr = 0;
      } else {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if (!eqptr || SDDS_StringIsBlank(eqptr+1)) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      plreq->color_settings.increment[0] = atol(eqptr+1);
      eqptr = commaptr;
      if ((commaptr=strchr(eqptr+1, ','))) {
	*commaptr = 0;
      } else {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if (!eqptr || SDDS_StringIsBlank(eqptr+1)) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      plreq->color_settings.increment[1] = atol(eqptr+1);
      eqptr = commaptr;
      if ((commaptr=strchr(eqptr+1, ','))) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      if (!eqptr || SDDS_StringIsBlank(eqptr+1)) {
	fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
	return 0;
      }
      plreq->color_settings.increment[2] = atol(eqptr+1);
      plreq->color_settings.flags |= COLORSET_INCREMENT;
      break;
    case ORDERCOLORS_KW_SPECTRAL:
      plreq->color_settings.flags |= COLORSET_SPECTRAL;
      break;
    case ORDERCOLORS_KW_RSPECTRAL:
      plreq->color_settings.flags |= COLORSET_RSPECTRAL;
      break;
    case ORDERCOLORS_KW_TEMPERATURE:
      plreq->color_settings.flags |= COLORSET_TEMPERATURE;
      break;
    case ORDERCOLORS_KW_RTEMPERATURE:
      plreq->color_settings.flags |= COLORSET_RTEMPERATURE;
      break;
    default:
      fprintf (stderr, "error: invalid -orderColors syntax\nusage: %s\n", orderColors_usage);
      return 0;
    }
  }
  return 1;
}

extern int dataBehind;
long dataBehind_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  dataBehind = 1;
  return 1;
}

long font_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    if (items!=1)
        return bombre("invalid -font syntax", "-font=<name>", 0);
    SDDS_CopyString(&plotspec->font, item[0]);
    return 1;
}
long listFonts_AP(PLOT_SPEC *plotspec, char **item, long items)
{
  hershey_font_list();
  exit(1);
  return(1);
}
long fixfontsize_AP(PLOT_SPEC *plotspec, char **item, long items)
{
    unsigned long flags;
    plotspec->fontsize[0].autosize = 0;
    plotspec->fontsize[0].all = -1;
    plotspec->fontsize[0].legend = -1;
    plotspec->fontsize[0].xlabel = -1;
    plotspec->fontsize[0].ylabel = -1;
    plotspec->fontsize[0].xticks = -1;
    plotspec->fontsize[0].yticks = -1;
    plotspec->fontsize[0].title = -1;
    plotspec->fontsize[0].topline = -1;
    if (items==0) {
      plotspec->fontsize[0].all = .02;
      SetupFontSize(&(plotspec->fontsize[0]));
      return 1;
    }

    if (!scanItemList(&flags,
                      item, &items,  0,
                      "all", SDDS_DOUBLE, &(plotspec->fontsize[0].all), 1, 0,
                      "legend", SDDS_DOUBLE, &(plotspec->fontsize[0].legend), 1, 0,
                      "xlabel", SDDS_DOUBLE, &(plotspec->fontsize[0].xlabel), 1, 0,
                      "ylabel", SDDS_DOUBLE, &(plotspec->fontsize[0].ylabel), 1, 0,
                      "xticks", SDDS_DOUBLE, &(plotspec->fontsize[0].xticks), 1, 0,
                      "yticks", SDDS_DOUBLE, &(plotspec->fontsize[0].yticks), 1, 0,
                      "title", SDDS_DOUBLE, &(plotspec->fontsize[0].title), 1, 0,
                      "topline", SDDS_DOUBLE, &(plotspec->fontsize[0].topline), 1, 0,
                      NULL)) {
      return bombre("invalid -fixfontsize syntax", "-fixfontsize=[all=.02][,legend=.015][,<x|y>xlabel=<value>][,<x|y>ticks=<value>][,title=<value>][,topline=<value>]", 0);
    }
    SetupFontSize(&(plotspec->fontsize[0]));
    return 1;
}
