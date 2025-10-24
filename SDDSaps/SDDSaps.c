/**
 * @file SDDSaps.c
 * @brief Routines used by SDDS command-line applications.
 *
 * This file contains implementations of various routines that support the 
 * functionality of SDDS (Self Describing Data Sets) command-line applications.
 * It includes functions for processing columns, filtering data, managing 
 * definitions, and handling edits and casts on SDDS datasets.
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include <ctype.h>
#include "mdb.h"
#include "SDDS.h"
#include "SDDSaps.h"
#include "rpn.h"
#if !defined(_WIN32)
#  include <sys/time.h>
#endif

#undef DEBUG

char *data_class_keyword[DATA_CLASS_KEYWORDS] = {
  "column",
  "parameter",
  "array",
};

typedef struct {
  char *name, *description;
  unsigned long flags;
} PROCESS_COLUMN_DATA;
#define PROCMODE_NORMAL 0x0000UL
#define PROCMODE_FUNCOF_REQUIRED 0x0001UL
#define PROCMODE_FUNCOF_UNITS 0x0002UL
#define PROCMODE_WEIGHT_OK 0x0004UL
#define PROCMODE_POSITION_OK 0x0008UL
#define PROCMODE_STRING_OK 0x0010UL
#define PROCMODE_YoX_UNITS 0x0020UL
#define PROCMODE_STRINGPOS_OK 0x0040UL
#define PROCMODE_YtX_UNITS 0x0080UL
#define PROCMODE_Y_UNITS 0x0100UL
#define PROCMODE_NO_UNITS x00200UL

static char *process_column_name[N_PROCESS_COLUMN_MODES] = {NULL};
static PROCESS_COLUMN_DATA process_column_data[N_PROCESS_COLUMN_MODES] = {
  {"average", "average of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_WEIGHT_OK},
  {"rms", "rms of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_WEIGHT_OK},
  {"sum", "sum of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_WEIGHT_OK},
  {"standarddeviation", "standard deviation of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_WEIGHT_OK},
  {"mad", "mean absolute deviation of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_WEIGHT_OK},
  {"minimum", "minimum of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_POSITION_OK | PROCMODE_STRINGPOS_OK},
  {"maximum", "maximum of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_POSITION_OK | PROCMODE_STRINGPOS_OK},
  {"smallest", "smallest of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_POSITION_OK | PROCMODE_STRINGPOS_OK},
  {"largest", "largest of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_POSITION_OK | PROCMODE_STRINGPOS_OK},
  {"first", "first ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_STRING_OK},
  {"last", "last ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_STRING_OK},
  {"count", "count ", PROCMODE_NORMAL},
  {"spread", "spread in ", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"median", "median of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"baselevel", "base level of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"toplevel", "top level of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"amplitude", "amplitude of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"risetime", "risetime of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"falltime", "falltime of", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"fwhm", "fwhm of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"fwtm", "fwtm of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"center", "center of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"zerocrossing", "zero crossing of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"fwha", "fwha of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"fwta", "fwta of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"sigma", "sigma of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_WEIGHT_OK},
  {"slope", "slope of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_YoX_UNITS},
  {"intercept", "intercept of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"lfsd", "linear-fit standard deviation of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"qrange", "quartile range", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"drange", "decile range", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"percentile", "percentile", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"mode", "mode of", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"integral", "integral of ", PROCMODE_NORMAL | PROCMODE_FUNCOF_REQUIRED | PROCMODE_YtX_UNITS},
  {"product", "product of ", PROCMODE_NORMAL | PROCMODE_WEIGHT_OK},
  {"prange", "percentile range", PROCMODE_NORMAL | PROCMODE_Y_UNITS},
  {"signedsmallest", "signed smallest of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_POSITION_OK | PROCMODE_STRINGPOS_OK},
  {"signedlargest", "signed largest of ", PROCMODE_NORMAL | PROCMODE_Y_UNITS | PROCMODE_POSITION_OK | PROCMODE_STRINGPOS_OK},
  {"gmintegral", "integral of ", PROCMODE_NORMAL | PROCMODE_FUNCOF_REQUIRED | PROCMODE_YtX_UNITS},
  {"correlation", "correlation coefficient", PROCMODE_FUNCOF_REQUIRED}};

/* these are used for options that support wildcard-based processing
 * using syntax like -define=column,%sFoo,"%sBar 2 *",select=x*
 */
#define SELECT_QUALIFIER 0
#define EDITSELECTION_QUALIFIER 1
#define EXCLUDE_QUALIFIER 2
#define SELECT_QUALIFIERS 3
static char *selectQualifier[SELECT_QUALIFIERS] = {
  "select", "editselection", "exclude"};

void GillMillerIntegration1(double *indepData, double *data, long n_data, double *result);

void show_process_modes(FILE *fp) {
  long i, length, maxLength, nPerLine;
  char format[30];

  maxLength = 0;
  for (i = 0; i < N_PROCESS_COLUMN_MODES; i++)
    if ((length = strlen(process_column_data[i].name)) > maxLength)
      maxLength = length;
  sprintf(format, "%%%lds%%c", maxLength + 1);
  nPerLine = 80 / (maxLength + 2);
  fprintf(fp, "processing modes:\n");
  for (i = 0; i < N_PROCESS_COLUMN_MODES; i++)
    fprintf(fp, format, process_column_data[i].name, (i + 1) % nPerLine ? ' ' : '\n');
  if (i % nPerLine)
    fputc('\n', fp);
}

void add_ifitem(IFITEM_LIST *ifitem, char **name, long names) {
  long i, type;
  if (ifitem->items == 0) {
    ifitem->name = NULL;
    ifitem->type = NULL;
  }
  ifitem->name = trealloc(ifitem->name, sizeof(*ifitem->name) * (ifitem->items + names - 1));
  ifitem->type = trealloc(ifitem->type, sizeof(*ifitem->type) * (ifitem->items + names - 1));
  if ((type = match_string(name[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) < 0)
    SDDS_Bomb("invalid -ifnot syntax---column, parameter, or array keyword required");
  for (i = 1; i < names; i++)
    ifitem->type[ifitem->items + i - 1] = type;
  if (!SDDS_CopyStringArray(ifitem->name + ifitem->items, name + 1, names - 1))
    SDDS_Bomb("problem copying names for -ifnot or -ifis option");
  ifitem->items += names - 1;
}

long check_ifitems(SDDS_DATASET *SDDS_dataset, IFITEM_LIST *ifitem, long desired, long announce) {
  long i, index;
  for (i = 0; i < ifitem->items; i++) {
    switch (ifitem->type[i]) {
    case COLUMN_BASED:
      index = SDDS_GetColumnIndex(SDDS_dataset, ifitem->name[i]);
      break;
    case PARAMETER_BASED:
      index = SDDS_GetParameterIndex(SDDS_dataset, ifitem->name[i]);
      break;
    case ARRAY_BASED:
      index = SDDS_GetArrayIndex(SDDS_dataset, ifitem->name[i]);
      break;
    default:
      SDDS_Bomb("internal error---unknown ifitem type");
      exit(1);
      break;
    }
    if ((index >= 0) != desired) {
      if (desired) {
        if (announce)
          fprintf(stderr, "%s %s does not exist--aborting\n",
                  data_class_keyword[ifitem->type[i]], ifitem->name[i]);
        return 0;
      }
      if (announce)
        fprintf(stderr, "%s %s exists--aborting\n",
                data_class_keyword[ifitem->type[i]], ifitem->name[i]);
      return 0;
    }
  }
  return 1;
}

FILTER_DEFINITION *process_new_filter_definition(char **argument, long arguments) {
  FILTER_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_filter_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for filter\n");
    return (NULL);
  }
  if (!(defi->filter_terms =
          process_filter_request(&(defi->filter_term), argument + 1, arguments - 1)))
    return (NULL);
  return (defi);
}

TIME_FILTER_DEFINITION *process_new_time_filter_definition(char **argument, long arguments) {
  TIME_FILTER_DEFINITION *defi;
  char *before, *after, *year, *month = NULL, *day = NULL, *hour, *minute, *second;
  short minute_n, second_n, hour_n;
  double hour_n1;
  minute_n = second_n = hour_n = 0;

  before = after = NULL;
  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_filter_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for filter\n");
    return (NULL);
  }
  defi->flags = 0;
  defi->before = defi->after = 0;
  hour = minute = second = 0;
  defi->name = argument[1];
  arguments -= 2;
  argument += 2;
  if (!scanItemList(&defi->flags, argument, &arguments, 0,
                    "before", SDDS_STRING, &before, 1, TIMEFILTER_BEFORE_GIVEN,
                    "after", SDDS_STRING, &after, 1, TIMEFILTER_AFTER_GIVEN,
                    "invert", -1, NULL, 0, TIMEFILTER_INVERT_GIVEN,
                    NULL))
    SDDS_Bomb("invalid -timeFilter syntax");
  if (before) {
    minute_n = second_n = hour_n = 0;
    if (!(year = get_token_tq(before, "", "/", "", "")) ||
        !(month = get_token_tq(before, "/", "/", "", "")) ||
        !(day = get_token_tq(before, "/", "@", "", "")))
      SDDS_Bomb("invalid \"before\" date given, the specified data should be YYYY/MM/DD@HH:MM:SS");
    if ((hour = get_token_tq(before, "@", ":", "", "")))
      hour_n = atol(hour);
    if ((minute = get_token_tq(before, ":", ":", "", "")))
      minute_n = atol(minute);
    if ((second = get_token_tq(before, ":", "", "", "")))
      second_n = atol(second);
    hour_n1 = hour_n * 1.0 + minute_n / 60.0 + second_n / 3600.0;
    if (!TimeBreakdownToEpoch(atol(year), 0, atol(month), atol(day), hour_n1, &defi->before))
      SDDS_Bomb("invalid -timeFilter before syntax (something wrong with time convert)");
    /*fprintf(stderr,"before: hour=%d,minute=%d,second=%d, time=%f\n",hour_n,minute_n,second_n,defi->before);*/
  }
  if (after) {
    minute_n = second_n = hour_n = 0;
    if (!(year = get_token_tq(after, "", "/", "", "")) ||
        !(month = get_token_tq(after, "/", "/", "", "")) ||
        !(day = get_token_tq(after, "/", "@", "", "")))
      SDDS_Bomb("invalid \"after\" date given, the specified data should be YYYY/MM/DD@HH:MM:SS");
    if ((hour = get_token_tq(after, "@", ":", "", "")))
      hour_n = atol(hour);
    if ((minute = get_token_tq(after, ":", ":", "", "")))
      minute_n = atol(minute);
    if ((second = get_token_tq(after, ":", "", "", "")))
      second_n = atol(second);
    hour_n1 = hour_n * 1.0 + minute_n / 60.0 + second_n / 3600.0;
    if (!TimeBreakdownToEpoch(atol(year), 0, atol(month), atol(day), hour_n1, &defi->after))
      SDDS_Bomb("invalid -timeFilter after syntax (something wrong with time convert)");
    /*  fprintf(stderr,"after: hour=%d,minute=%d,second=%d, time=%f\n",hour_n,minute_n,second_n,defi->after); */
  }
  if (before && after && defi->before < defi->after)
    SDDS_Bomb("Invalid -timeFilter syntx, before date is earlier than the after date!");
  if (!before)
    defi->before = getTimeInSecs() + 3600.0;
  /*no before time is given, accept all the time from the after date to now+3600*/
  if (!after)
    defi->after = -1; /*no after time is given, accept all the time before the before date */
  /*fprintf(stderr,"before=%f, after=%f\n",defi->before,defi->after); */
  return (defi);
}

MATCH_DEFINITION *process_new_match_definition(char **argument, long arguments) {
  MATCH_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_match_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for match\n");
    return (NULL);
  }
  if (!(defi->match_terms =
          process_match_request(&(defi->match_term), argument + 1, arguments - 1)))
    return (NULL);
  return (defi);
}

long process_filter_request(FILTER_TERM **filter, char **argument, long arguments) {
  long i, filters, max_filters;
  long need_name, need_operation;

  filters = 0;
  i = 0;
  max_filters = (arguments + 1) / 3 + 1;
  *filter = tmalloc(sizeof(**filter) * max_filters);

  need_name = 1;
  need_operation = -1;
  do {
    if (is_logic_character(argument[i][0])) {
      if (need_name)
        return (0);
      do {
        /* will come here only for &, |, or ! applying to expressions */
        if (need_name)
          return (0);
        switch (argument[i][0]) {
        case '&':
          if ((*filter)[filters].logic & SDDS_AND || (*filter)[filters].logic & SDDS_OR ||
              (*filter)[filters].logic & SDDS_NEGATE_EXPRESSION)
            return (0);
          (*filter)[filters].logic |= SDDS_AND;
          break;
        case '|':
          if ((*filter)[filters].logic & SDDS_AND || (*filter)[filters].logic & SDDS_OR ||
              (*filter)[filters].logic & SDDS_NEGATE_EXPRESSION)
            return (0);
          (*filter)[filters].logic |= SDDS_OR;
          break;
        case '!':
          if (!((*filter)[filters].logic & SDDS_AND || (*filter)[filters].logic & SDDS_OR) ||
              (*filter)[filters].logic & SDDS_NEGATE_EXPRESSION)
            return (0);
          (*filter)[filters].logic |= SDDS_NEGATE_EXPRESSION;
        }
      } while (++i < arguments && is_logic_character(argument[i][0]));
      filters++;
      if (i >= arguments)
        break;
      need_name = 1;
    } else if (need_operation > 0)
      return (0);

    /* should have a triplet (name, lower, upper) in argument list now */
    if (is_logic_character(argument[i][0]))
      return (0);
    if (arguments - i < 3)
      return (0);
    if (filters + 1 >= max_filters)
      *filter = trealloc(*filter, sizeof(**filter) * (max_filters += 2));
    (*filter)[filters].name = argument[i++];
    if (sscanf(argument[i], "%lf", &(*filter)[filters].lower) != 1)
      (*filter)[filters].lowerPar = argument[i];
    else
      (*filter)[filters].lowerPar = NULL;
    i++;
    if (sscanf(argument[i], "%lf", &(*filter)[filters].upper) != 1)
      (*filter)[filters].upperPar = argument[i];
    else
      (*filter)[filters].upperPar = NULL;
    i++;
    if ((*filter)[filters].lowerPar == NULL && (*filter)[filters].upperPar == NULL && (*filter)[filters].upper < (*filter)[filters].lower)
      return (0);
    (*filter)[filters].logic = 0;
    if (arguments - i > 0 && argument[i][0] == '!') {
      (*filter)[filters].logic = SDDS_NEGATE_MATCH;
      i++;
    }
    if (++need_operation > 0)
      need_name = 0;
    else
      filters++;
  } while (arguments > i);
  (*filter)[0].logic |= SDDS_AND;
  return (filters);
}

long process_match_request(MATCH_TERM **match, char **argument, long arguments) {
  long i, matches, max_matches;
  long need_name, need_operation;
  char *ptr;

  matches = 0;
  i = 0;
  max_matches = (arguments + 1) / 3 + 1;
  *match = tmalloc(sizeof(**match) * max_matches);

  need_name = 1;
  need_operation = -1;
  do {
    if (is_logic_character(argument[i][0])) {
      if (need_name)
        return (0);
      do {
        /* will come here only for &, |, or ! applying to expressions */
        if (need_name)
          return (0);
        switch (argument[i][0]) {
        case '&':
          if ((*match)[matches].logic & SDDS_AND || (*match)[matches].logic & SDDS_OR ||
              (*match)[matches].logic & SDDS_NEGATE_EXPRESSION)
            return (0);
          (*match)[matches].logic |= SDDS_AND;
          break;
        case '|':
          if ((*match)[matches].logic & SDDS_AND || (*match)[matches].logic & SDDS_OR ||
              (*match)[matches].logic & SDDS_NEGATE_EXPRESSION)
            return (0);
          (*match)[matches].logic |= SDDS_OR;
          break;
        case '!':
          if (!((*match)[matches].logic & SDDS_AND || (*match)[matches].logic & SDDS_OR) ||
              (*match)[matches].logic & SDDS_NEGATE_EXPRESSION)
            return (0);
          (*match)[matches].logic |= SDDS_NEGATE_EXPRESSION;
        }
      } while (++i < arguments && is_logic_character(argument[i][0]));
      matches++;
      if (i >= arguments)
        break;
      need_name = 1;
    } else if (need_operation > 0)
      return (0);

    /* should have a name=string object now */
    if (is_logic_character(argument[i][0]))
      return (0);
    if (arguments - i < 1)
      return (0);
    if (matches + 1 >= max_matches)
      *match = trealloc(*match, sizeof(**match) * (max_matches += 2));
    if (!(ptr = strchr(argument[i], '=')))
      return (0);
    *ptr = 0;
    (*match)[matches].logic = 0;
    if (*(ptr + 1) == '+') {
      (*match)[matches].logic |= SDDS_NOCASE_COMPARE;
      ptr++;
    }
    if (*(ptr + 1) == '@') {
      (*match)[matches].logic |= SDDS_INDIRECT_MATCH;
      ptr++;
    }
    (*match)[matches].name = argument[i++];
    (*match)[matches].string = ptr + 1;
    if (arguments - i > 0 && argument[i][0] == '!') {
      (*match)[matches].logic |= SDDS_NEGATE_MATCH;
      i++;
    }
    if (++need_operation > 0)
      need_name = 0;
    else
      matches++;
  } while (arguments > i);
  (*match)[0].logic |= SDDS_AND;
  return (matches);
}

void scan_label_parameter(LABEL_PARAMETER *label, char *string) {
  char *ptr;
  label->format = NULL;
  if ((ptr = strchr(string, '='))) {
    *ptr = 0;
    label->format = ptr + 1;
  }
  label->name = string;
}

void show_matches(char *type, MATCH_TERM *match, long matches) {
  long i;
  fprintf(stderr, "\n    %s matching logic:\n", type);
  for (i = 0; i < matches; i++) {
    if (match[i].logic & SDDS_NEGATE_MATCH)
      fprintf(stderr, "        <%s> != ", match[i].name);
    else
      fprintf(stderr, "        <%s> == ", match[i].name);
    if (match[i].logic & SDDS_INDIRECT_MATCH)
      fprintf(stderr, "<%s>", match[i].string);
    else
      fprintf(stderr, "\"%s\"", match[i].string);
    if (match[i].logic & SDDS_NOCASE_COMPARE)
      fprintf(stderr, "  (case insensitive)\n");
    else
      fprintf(stderr, "  (case sensitive)\n");
    if (i != 0) {
      if (match[i].logic & SDDS_AND)
        fprintf(stderr, "&& ");
      if (match[i].logic & SDDS_OR)
        fprintf(stderr, "|| ");
    }
    if (match[i].logic & SDDS_NEGATE_EXPRESSION)
      fputc('!', stderr);
    fputc('\n', stderr);
  }
}

void show_filters(char *type, FILTER_TERM *filter, long filters) {
  long i;
  fprintf(stderr, "\n    %s filtering logic:\n", type);
  for (i = 0; i < filters; i++) {
    if (filter[i].logic & SDDS_NEGATE_MATCH)
      fprintf(stderr, "        !( <%s>:[%22.15e, %22.15e] ) ", filter[i].name, filter[i].lower, filter[i].upper);
    else
      fprintf(stderr, "           <%s>:[%22.15e, %22.15e] ", filter[i].name, filter[i].lower, filter[i].upper);
    if (i != 0) {
      if (filter[i].logic & SDDS_AND)
        fprintf(stderr, "&& ");
      if (filter[i].logic & SDDS_OR)
        fprintf(stderr, "|| ");
    }
    if (filter[i].logic & SDDS_NEGATE_EXPRESSION)
      fputc('!', stderr);
    fputc('\n', stderr);
  }
}

EQUATION_DEFINITION *process_new_equation_definition(char **argument, long arguments) {
  static char s[SDDS_MAXLINE];
  char *ptr, *equation;
  long i, code, algebraic = 0;
  EQUATION_DEFINITION *defi;
  char pfix[IFPF_BUF_SIZE];

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_equation_definition)");
  defi->name = defi->text = defi->equation = defi->udf_name =
    defi->editSelection = defi->select = defi->exclude = NULL;
  defi->argv = NULL;
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    sprintf(s, "&column name=\"%s\", ", argument[1]);
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    sprintf(s, "&parameter name=\"%s\", ", argument[1]);
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for definition\n");
    return (NULL);
  }
  SDDS_CopyString(&defi->name, argument[1]);

  for (i = 3; i < arguments; i++) {
    if (strncmp(argument[i], "algebraic", strlen(argument[i])) == 0) {
      algebraic = 1;
      break;
    }
  }

  defi->argc = 0;

  if (algebraic) {
    if (argument[2][0] == '@') {
      /*Add second @ symbol to signify that the parameter contains algebraic notation*/
      equation = malloc(sizeof(char) * strlen(argument[2]) + 1);
      sprintf(equation, "@%s", argument[2]);
    } else {
      ptr = addOuterParentheses(argument[2]);
      if2pf(pfix, ptr, sizeof pfix);
      free(ptr);

      if (!SDDS_CopyString(&equation, pfix)) {
        fprintf(stderr, "error: problem copying argument string\n");
        return NULL;
      }
    }
    if (arguments > 4) {
      defi->argv = tmalloc(sizeof(*defi->argv) * (arguments - 3));
    } else {
      defi->argv = tmalloc(sizeof(*defi->argv) * 1);
    }
  } else {
    equation = argument[2];
    if (arguments > 3) {
      defi->argv = tmalloc(sizeof(*defi->argv) * (arguments - 2));
    } else {
      defi->argv = tmalloc(sizeof(*defi->argv) * 1);
    }
  }
  for (i = 3; i < arguments; i++) {
    if (!(ptr = strchr(argument[i], '='))) {
      if (strncmp(argument[i], "algebraic", strlen(argument[i])) == 0)
        continue;
      fprintf(stderr, "error: invalid definition-entry: %s\n", argument[i]);
      return (NULL);
    }
    *ptr = 0;
    switch (code = match_string(argument[i], selectQualifier, SELECT_QUALIFIERS, 0)) {
    case SELECT_QUALIFIER:
      SDDS_CopyString(&defi->select, ptr + 1);
      break;
    case EDITSELECTION_QUALIFIER:
      SDDS_CopyString(&defi->editSelection, ptr + 1);
      break;
    case EXCLUDE_QUALIFIER:
      SDDS_CopyString(&defi->exclude, ptr + 1);
      break;
    default:
      break;
    }
    *ptr = '=';
    if (code >= 0)
      continue;
    if (!SDDS_CopyString(defi->argv + defi->argc, argument[i])) {
      fprintf(stderr, "error: problem copying argument string\n");
      return NULL;
    }
    defi->argc += 1;
    *ptr = 0;
    strcat(s, argument[i]);
    strcat(s, "=\"");
    strcat(s, ptr + 1);
    strcat(s, "\", ");
    *ptr = '=';
  }
  if (!strstr(s, ", type=")) {
    strcat(s, " type=\"double\", ");
    SDDS_CopyString(defi->argv + defi->argc, "type=double");
    defi->argc += 1;
  }
  strcat(s, "&end");
  if (!SDDS_CopyString(&defi->text, s) || !SDDS_CopyString(&defi->equation, equation))
    SDDS_Bomb("string copying failed (process_new_equation_definition)");
  return (defi);
}

EVALUATE_DEFINITION *process_new_evaluate_definition(char **argument, long arguments) {
  static char s[SDDS_MAXLINE];
  long i;
  EVALUATE_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_evaluate_definition)");
  defi->name = defi->text = defi->source = NULL;
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    sprintf(s, "&column name=\"%s\", ", argument[1]);
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    sprintf(s, "&parameter name=\"%s\", ", argument[1]);
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for definition\n");
    return (NULL);
  }
  SDDS_CopyString(&defi->name, argument[1]);

  defi->name = argument[1];
  defi->source = argument[2];

  for (i = 3; i < arguments; i++) {
    strcat(s, ", ");
    strcat(s, argument[i]);
  }
  if (!strstr(s, ", type=")) {
    strcat(s, " type=double, ");
  }
  strcat(s, " &end");
  if (!SDDS_CopyString(&defi->text, s))
    SDDS_Bomb("string copying failed (process_new_evaluate_definition)");
  return (defi);
}

SCAN_DEFINITION *process_new_scan_definition(char **argument, long arguments) {
  static char s[SDDS_MAXLINE];
  char *ptr;
  long i;
  SCAN_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_scan_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    sprintf(s, "&column name=\"%s\", ", argument[1]);
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    sprintf(s, "&parameter name=\"%s\", ", argument[1]);
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for scan\n");
    return (NULL);
  }
  defi->new_name = argument[1];
  defi->source = argument[2];
  defi->sscanf_string = argument[3];
  defi->edit = NULL;
  for (i = 4; i < arguments; i++) {
    if (!(ptr = strchr(argument[i], '=')))
      return (NULL);
    *ptr = 0;
    if (strncasecmp(argument[i], "edit", strlen(argument[i])) == 0) {
      /* pre-edit option */
      SDDS_CopyString(&defi->edit, ptr + 1);
    } else {
      strcat(s, argument[i]);
      strcat(s, "=\"");
      strcat(s, ptr + 1);
      strcat(s, "\", ");
    }
  }
  if (!strstr(s, ", type="))
    strcat(s, " type=\"double\", ");
  strcat(s, "&end");
  if (!SDDS_CopyString(&defi->text, s))
    SDDS_Bomb("unable to copy text of scan definition (process_new_scan_definition)");
  return (defi);
}

CAST_DEFINITION *process_new_cast_definition(char **argument, long arguments) {
  CAST_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_cast_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->isParameter = 0;
    break;
  case PARAMETER_BASED:
    defi->isParameter = 1;
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for cast\n");
    return (NULL);
  }
  defi->newName = argument[1];
  defi->source = argument[2];
  defi->newTypeName = argument[3];
  if (strcmp(defi->newName, defi->source) == 0) {
    fprintf(stderr, "error: can't cast something to the same name (sddsprocess)\n");
    return NULL;
  }
  return (defi);
}

long complete_cast_definition(SDDS_DATASET *SDDSout, CAST_DEFINITION *defi,
                              SDDS_DATASET *SDDSin) {
  char s[1024];

  if (!(defi->newType = SDDS_IdentifyType(defi->newTypeName))) {
    sprintf(s, "Unable to cast parameter %s to unknown type %s from column %s",
            defi->newName, defi->newTypeName, defi->source);
    SDDS_SetError(s);
    return 0;
  }
  if (defi->isParameter) {
    if (!SDDS_TransferParameterDefinition(SDDSout, SDDSout,
                                          defi->source, defi->newName) ||
        !SDDS_ChangeParameterInformation(SDDSout, "type", &defi->newType,
                                         SDDS_BY_NAME, defi->newName)) {
      sprintf(s, "Unable to cast parameter %s to type %s from column %s",
              defi->newName, defi->newTypeName, defi->source);
      SDDS_SetError(s);
      return 0;
    }
  } else {
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSout,
                                       defi->source, defi->newName) ||
        !SDDS_ChangeColumnInformation(SDDSout, "type", &defi->newType,
                                      SDDS_BY_NAME, defi->newName)) {
      sprintf(s, "Unable to cast column %s to type %s from column %s",
              defi->newName, defi->newTypeName, defi->source);
      SDDS_SetError(s);
      return 0;
    }
  }
  return 1;
}

NUMBERTEST_DEFINITION *process_new_numbertest_definition(char **argument, long arguments) {
  static char s[SDDS_MAXLINE];
  long i, j;

  NUMBERTEST_DEFINITION *defi;
#define NUMBERTEST_QUALIFIERS 2
  static char *numbertestQualifier[NUMBERTEST_QUALIFIERS] = {
    "invert",
    "strict",
  };
  static unsigned long numbertestFlag[NUMBERTEST_QUALIFIERS] = {
    NUMSCANFILTER_INVERT,
    NUMSCANFILTER_STRICT,
  };

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_numbertest_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    sprintf(s, "&column name=\"%s\", ", argument[1]);
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    sprintf(s, "&parameter name=\"%s\", ", argument[1]);
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for numbertest\n");
    return (NULL);
  }
  defi->name = argument[1];
  defi->flags = 0;
  for (i = 2; i < arguments; i++) {
    switch (j = match_string(argument[i], numbertestQualifier, NUMBERTEST_QUALIFIERS, 0)) {
    case -1:
      fprintf(stderr, "error: unknown qualifer to -numberTest: %s\n", argument[i]);
      exit(1);
      break;
    default:
      defi->flags |= numbertestFlag[j];
      break;
    }
  }
  return (defi);
}

EDIT_DEFINITION *process_new_edit_definition(char **argument, long arguments, short reedit) {
  static char s[SDDS_MAXLINE];
  char *ptr;
  long i, offset, hasType;
  EDIT_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_edit_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    sprintf(s, "&column name=\"%s\", ", argument[1]);
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    sprintf(s, "&parameter name=\"%s\", ", argument[1]);
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for edit\n");
    return (NULL);
  }
  defi->new_name = argument[1];
  if (!reedit) {
    defi->source = argument[2];
    defi->edit_command = argument[3];
    offset = 4;
  } else {
    defi->source = defi->new_name;
    defi->edit_command = argument[2];
    offset = 3;
  }
  if (arguments > offset) {
    defi->argv = tmalloc(sizeof(*defi->argv) * (arguments - offset));
    if (!SDDS_CopyStringArray(defi->argv, argument + offset, arguments - offset))
      SDDS_Bomb("string array copy failed (process_new_edit_definition)");
    defi->argc = arguments - offset;
  } else {
    defi->argc = 0;
    defi->argv = NULL;
  }
  hasType = 0;
  for (i = offset; i < arguments; i++) {
    if (!(ptr = strchr(argument[i], '=')))
      return (NULL);
    *ptr = 0;
    if (strcmp(argument[i], "type") == 0)
      hasType = 1;
    strcat(s, argument[i]);
    strcat(s, "=\"");
    strcat(s, ptr + 1);
    strcat(s, "\", ");
  }
  if (!hasType)
    strcat(s, " type=string &end");
  else
    strcat(s, " &end");
  if (!SDDS_CopyString(&defi->text, s))
    SDDS_Bomb("unable to copy text of edit definition (process_new_edit_definition)");
  return (defi);
}

long edit_column_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *edit_command) {
  char **source_value;
  long target_type, target_index, source_index;
  int64_t i, rows;
  static char s[SDDS_MAXLINE];

  if ((source_index = SDDS_GetColumnIndex(SDDS_dataset, source)) < 0 ||
      (target_index = SDDS_GetColumnIndex(SDDS_dataset, target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (SDDS_GetColumnType(SDDS_dataset, source_index) != SDDS_STRING) {
    fprintf(stderr, "error: source column %s has wrong type for editing--must be string\n", source);
    return (0);
  }
  if ((target_type = SDDS_GetColumnType(SDDS_dataset, target_index)) != SDDS_STRING &&
      target_type != SDDS_CHARACTER) {
    fprintf(stderr, "error: target column %s has wrong type for editing--must be string or character\n", target);
    return (0);
  }

  if ((rows = SDDS_CountRowsOfInterest(SDDS_dataset)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  if (!(source_value = SDDS_GetInternalColumn(SDDS_dataset, source))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  for (i = 0; i < rows; i++) {
    if (!source_value[i])
      s[0] = 0;
    else
      strcpy(s, source_value[i]);
    if (!edit_string(s, edit_command)) {
      fprintf(stderr, "error: unable to edit source string \"%s\" to make %s\n", source_value[i], target);
      return (0);
    }
    if ((target_type == SDDS_STRING &&
         !SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, s, -1)) ||
        (target_type == SDDS_CHARACTER &&
         !SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, s[0], -1))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    /*
      if (source_value[i])
      free(source_value[i]);
      */
  }
  /*
    free(source_value);
    */
  return (1);
}

long edit_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *edit_command) {
  char *source_value, **ptr;
  long target_type, target_index, source_index;
  static char s[SDDS_MAXLINE];

  if ((source_index = SDDS_GetParameterIndex(SDDS_dataset, source)) < 0 ||
      (target_index = SDDS_GetParameterIndex(SDDS_dataset, target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (SDDS_GetParameterType(SDDS_dataset, source_index) != SDDS_STRING) {
    fprintf(stderr, "error: source parameter %s has wrong type for editing--must be string\n", source);
    return (0);
  }
  if ((target_type = SDDS_GetParameterType(SDDS_dataset, target_index)) != SDDS_STRING &&
      target_type != SDDS_CHARACTER) {
    fprintf(stderr, "error: target parameter %s has wrong type for editing--must be string or character\n", target);
    return (0);
  }

  if (!(ptr = SDDS_GetParameter(SDDS_dataset, source, NULL))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (!(source_value = *ptr))
    s[0] = 0;
  else
    strcpy(s, source_value);
  if (!edit_string(s, edit_command)) {
    fprintf(stderr, "error: unable to edit source string \"%s\" to make %s\n", source_value, target);
    return (0);
  }
  if ((target_type == SDDS_STRING &&
       !SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, s, -1)) ||
      (target_type == SDDS_CHARACTER &&
       !SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, s[0], -1))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (source_value)
    free(source_value);
  free(ptr);
  return (1);
}

OUTPUT_REQUEST *process_output_request(char **argument, long arguments, OUTPUT_REQUEST *last_request) {
  OUTPUT_REQUEST *request;
  request = tmalloc(sizeof(*request));
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    request->parameter_output = 0;
    break;
  case PARAMETER_BASED:
    request->parameter_output = 1;
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for output\n");
    return (NULL);
  }
  argument++;
  arguments--;

  request->fp = NULL;
  request->points = 0;
  request->counter = 0;

  if (!(request->item[0] = determine_item_name(argument, last_request, 0)))
    return (NULL);

  if (!(request->item[1] = determine_item_name(argument, last_request, 1)))
    return (NULL);

  request->item[2] = request->item[3] = NULL;
  request->columns = 2;

  if (arguments < 3)
    return (request);

  request->columns = 3;
  if (!(request->item[2] = determine_item_name(argument, last_request, 2)))
    return (NULL);

  if (arguments < 4)
    return (request);

  request->columns = 4;
  if (!(request->item[3] = determine_item_name(argument, last_request, 3)))
    return (NULL);

  return (request);
}

char *determine_item_name(char **argument, OUTPUT_REQUEST *last_request, long index) {
  if (strcmp(argument[index], ".") == 0) {
    if (!last_request)
      return (NULL);
    return (last_request->item[index]);
  }
  return (argument[index]);
}

void set_up_output(char *filename, OUTPUT_REQUEST *output, LABEL_PARAMETER *label_parameter, long label_parameters,
                   long separate_pages, long announce_openings, SDDS_DATASET *SDDS_dataset) {
  COLUMN_DEFINITION **coldef;
  PARAMETER_DEFINITION **pardef, *pardefptr;
  long j;
  double param;
  char *mplTitle, *mplTopline;

  output->fp = fopen_e(filename, "w", 0);
  if (announce_openings)
    fprintf(stderr, "file opened: %s\n", filename);
  if (!output->parameter_output) {
    coldef = (COLUMN_DEFINITION **)(output->definitions = tmalloc(sizeof(*coldef) * 4));
    coldef[0] = coldef[1] = coldef[2] = coldef[3] = NULL;
    if (!(coldef[0] = SDDS_GetColumnDefinition(SDDS_dataset, output->item[0])) ||
        !(coldef[1] = SDDS_GetColumnDefinition(SDDS_dataset, output->item[1])) ||
        (output->item[2] && !(coldef[2] = SDDS_GetColumnDefinition(SDDS_dataset, output->item[2]))) ||
        (output->item[3] && !(coldef[3] = SDDS_GetColumnDefinition(SDDS_dataset, output->item[3])))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    fputs(coldef[0]->symbol ? coldef[0]->symbol : output->item[0], output->fp);
    if (coldef[0]->units && !is_blank(coldef[0]->units))
      fprintf(output->fp, " (%s)", coldef[0]->units);
    fprintf(output->fp, "\n%s", coldef[1]->symbol ? coldef[1]->symbol : output->item[1]);
    if (coldef[1]->units && !is_blank(coldef[1]->units))
      fprintf(output->fp, " (%s)", coldef[1]->units);
    if (label_parameters) {
      fputc('\n', output->fp);
      for (j = 0; j < label_parameters; j++) {
        if (!(pardefptr = SDDS_GetParameterDefinition(SDDS_dataset, label_parameter[j].name))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
        if (!SDDS_GetParameter(SDDS_dataset, label_parameter[j].name, &param)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
        fprintf(output->fp, "%s=", pardefptr->symbol ? pardefptr->symbol : label_parameter[j].name);
        SDDS_PrintTypedValue((void *)&param, 0, pardefptr->type,
                             (label_parameter[j].format ? label_parameter[j].format : pardefptr->format_string), output->fp,
                             0);
        if (pardefptr->units) {
          if (isdigit(pardefptr->units[0]))
            fputc(' ', output->fp);
          fprintf(output->fp, "%s%s", pardefptr->units, j == label_parameters - 1 ? "\n" : "   ");
        } else
          fprintf(output->fp, "%s", j == label_parameters - 1 ? "\n" : "  ");
      }
    } else {
      if (SDDS_GetParameterIndex(SDDS_dataset, "mplTitle") < 0)
        fprintf(output->fp, "\n%s vs %s\n",
                coldef[1]->description ? coldef[1]->description : (coldef[1]->symbol ? coldef[1]->symbol : output->item[1]),
                coldef[0]->description ? coldef[0]->description : (coldef[0]->symbol ? coldef[0]->symbol : output->item[0]));
      else {
        if (!SDDS_GetParameter(SDDS_dataset, "mplTitle", &mplTitle)) {
          SDDS_SetError("Unable to get value of parameter mplTitle");
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        fprintf(output->fp, "\n%s\n", mplTitle);
        free(mplTitle);
      }
    }
  } else {
    pardef = (PARAMETER_DEFINITION **)(output->definitions = tmalloc(sizeof(*pardef) * 4));
    pardef[0] = pardef[1] = pardef[2] = pardef[3] = NULL;
    if (!(pardef[0] = SDDS_GetParameterDefinition(SDDS_dataset, output->item[0])) ||
        !(pardef[1] = SDDS_GetParameterDefinition(SDDS_dataset, output->item[1])) ||
        (output->item[2] && !(pardef[2] = SDDS_GetParameterDefinition(SDDS_dataset, output->item[2]))) ||
        (output->item[3] && !(pardef[3] = SDDS_GetParameterDefinition(SDDS_dataset, output->item[3])))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    fputs(pardef[0]->symbol ? pardef[0]->symbol : output->item[0], output->fp);
    if (pardef[0]->units && !is_blank(pardef[0]->units))
      fprintf(output->fp, " (%s)", pardef[0]->units);
    fprintf(output->fp, "\n%s", pardef[1]->symbol ? pardef[1]->symbol : output->item[1]);
    if (pardef[1]->units && !is_blank(pardef[1]->units))
      fprintf(output->fp, " (%s)", pardef[1]->units);
    if (label_parameters) {
      fputc('\n', output->fp);
      for (j = 0; j < label_parameters; j++) {
        if (!(pardefptr = SDDS_GetParameterDefinition(SDDS_dataset, label_parameter[j].name))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
        if (!SDDS_GetParameter(SDDS_dataset, label_parameter[j].name, &param)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
        fprintf(output->fp, "%s=", pardefptr->symbol ? pardefptr->symbol : label_parameter[j].name);
        SDDS_PrintTypedValue((void *)&param, 0, pardefptr->type,
                             (label_parameter[j].format ? label_parameter[j].format : pardefptr->format_string), output->fp,
                             0);
        if (pardefptr->units) {
          if (isdigit(pardefptr->units[0]))
            fputc(' ', output->fp);
          fprintf(output->fp, "%s%s", pardefptr->units, j == label_parameters - 1 ? "\n" : "   ");
        } else
          fprintf(output->fp, "%s", j == label_parameters - 1 ? "\n" : "  ");
      }
    } else {
      if (SDDS_GetParameterIndex(SDDS_dataset, "mplTitle") < 0)
        fprintf(output->fp, "\n%s vs %s\n",
                pardef[1]->description ? pardef[1]->description : (pardef[1]->symbol ? pardef[1]->symbol : output->item[1]),
                pardef[0]->description ? pardef[0]->description : (pardef[0]->symbol ? pardef[0]->symbol : output->item[0]));
      else {
        if (!SDDS_GetParameter(SDDS_dataset, "mplTitle", &mplTitle)) {
          SDDS_SetError("Unable to get value of parameter mplTitle");
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        fprintf(output->fp, "\n%s\n", mplTitle);
        free(mplTitle);
      }
    }
  }
  if (SDDS_GetParameterIndex(SDDS_dataset, "mplTopline") < 0) {
    fprintf(output->fp, "Data extracted from SDDS file %s", SDDS_dataset->layout.filename);
    if (separate_pages)
      fprintf(output->fp, ", page %" PRId32, SDDS_dataset->page_number);
  } else {
    if (!SDDS_GetParameter(SDDS_dataset, "mplTopline", &mplTopline)) {
      SDDS_SetError("Unable to get value of parameter mplTopline");
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    fprintf(output->fp, "%s", mplTopline);
    free(mplTopline);
  }
  fprintf(output->fp, "\n%-10" PRId64 "\n", SDDS_dataset->n_rows);
}

#define DEBUG 0

long process_column(SDDS_DATASET *Dataset, PROCESSING_DEFINITION *processing_ptr,
                    double *result, char **stringResult, long warnings, int threads) {
  double *data, *indepData, *weightData;
  long i1, i2 = 0, i2save, mode;
  int64_t imin, imax;
  int64_t i, n_data, count;
  double min, max, top, base, point1, point2;
  double slope, intercept, variance;
  long returnValue, matchfound;
  char **stringData, **matchData;
  short *keep;
  double quartilePoint[2] = {25.0, 75.0}, quartileResult[2];
  double decilePoint[2] = {10.0, 90.0}, decileResult[2];
  double percentilePoint[2], percentileResult[2];
  char temp_str[256];

  i1 = i2save = 0;
  matchfound = 0;
  mode = processing_ptr->mode;
#if DEBUG
  fprintf(stderr, "process_column: column=%s, mode=%s\n", processing_ptr->column_name,
          process_column_data[mode].name);
#endif

  *stringResult = NULL;
  if ((n_data = SDDS_CountRowsOfInterest(Dataset)) <= 0) {
    if (mode != PROCESS_COLUMN_COUNT)
      *result = processing_ptr->defaultValue;
    else
      *result = n_data;
    return 1;
  }
  if (processing_ptr->lower_par && !SDDS_GetParameterAsDouble(Dataset, processing_ptr->lower_par,
                                                              &processing_ptr->lowerLimit))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (processing_ptr->upper_par && !SDDS_GetParameterAsDouble(Dataset, processing_ptr->upper_par,
                                                              &processing_ptr->upperLimit))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  
  if (processing_ptr->head_par && !SDDS_GetParameterAsLong(Dataset, processing_ptr->head_par,
							   &processing_ptr->head))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (processing_ptr->tail_par && !SDDS_GetParameterAsLong(Dataset, processing_ptr->tail_par,
							   &processing_ptr->tail))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (processing_ptr->fhead_par && !SDDS_GetParameterAsDouble(Dataset, processing_ptr->fhead_par,
							   &processing_ptr->fhead))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (processing_ptr->ftail_par && !SDDS_GetParameterAsDouble(Dataset, processing_ptr->ftail_par,
							   &processing_ptr->ftail))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (processing_ptr->offset_par) {
    if (wild_match(processing_ptr->offset_par, "%s*")) {
      sprintf(temp_str, processing_ptr->offset_par, processing_ptr->column_name);
      if (!SDDS_GetParameterAsDouble(Dataset, temp_str,
				     &processing_ptr->offset))
	SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (!SDDS_GetParameterAsDouble(Dataset, processing_ptr->offset_par,
					  &processing_ptr->offset))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (processing_ptr->factor_par) {
    if (wild_match(processing_ptr->factor_par, "%s*")) {
      sprintf(temp_str, processing_ptr->factor_par, processing_ptr->column_name);
      if (!SDDS_GetParameterAsDouble(Dataset, temp_str,
				     &processing_ptr->factor))
	SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (!SDDS_GetParameterAsDouble(Dataset, processing_ptr->factor_par,
					  &processing_ptr->factor))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if ((processing_ptr->flags & PROCESSING_INVERT_OFFSET) && (processing_ptr->flags & PROCESSING_OFFSET_GIVEN))
    processing_ptr->offset *=-1;
  if ((processing_ptr->flags & PROCESSING_FACTOR_GIVEN) && (processing_ptr->flags & PROCESSING_INVERT_FACTOR))
      processing_ptr->factor = 1/processing_ptr->factor;
  
  if (!(data = SDDS_GetColumnInDoubles(Dataset, processing_ptr->column_name)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  indepData = weightData = NULL;
  stringData = matchData = NULL;

  if (processing_ptr->weightBy && !(weightData = SDDS_GetColumnInDoubles(Dataset, processing_ptr->weightBy)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (processing_ptr->functionOf &&
      processing_ptr->outputType != SDDS_STRING &&
      !(indepData = SDDS_GetColumnInDoubles(Dataset, processing_ptr->functionOf)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (processing_ptr->functionOf &&
      processing_ptr->outputType == SDDS_STRING &&
      !(stringData = SDDS_GetColumn(Dataset, processing_ptr->functionOf)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  keep = NULL;
  if (processing_ptr->flags & (PROCESSING_LOLIM_GIVEN | PROCESSING_UPLIM_GIVEN | PROCESSING_TAIL_GIVEN | PROCESSING_HEAD_GIVEN | PROCESSING_FTAIL_GIVEN | PROCESSING_FHEAD_GIVEN | PROCESSING_MATCHCOLUMN_GIVEN | PROCESSING_TOPLIM_GIVEN | PROCESSING_BOTLIM_GIVEN)) {
    keep = tmalloc(sizeof(*keep) * n_data);
    for (i = 0; i < n_data; i++)
      keep[i] = 1;
  }
  if (processing_ptr->flags & PROCESSING_MATCHCOLUMN_GIVEN) {
    if (!(matchData = SDDS_GetColumn(Dataset, processing_ptr->match_column)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < n_data; i++)
      keep[i] = 0;
    for (i = 0; i < n_data; i++) {
      if (wild_match(matchData[i], processing_ptr->match_value)) {
        matchfound = 1;
	keep[i] = 1;
      }
    }
    for (i = 0; i < n_data; i++)
      free(matchData[i]);
    free(matchData);
    if (!matchfound) {
      if (warnings)
        fprintf(stderr, "Warning: no values in column %s match %s.\n",
                processing_ptr->match_column, processing_ptr->match_value);
      free(keep);
      free(data);
      *result = processing_ptr->defaultValue;
      return 1;
    }
  }
  if (processing_ptr->flags & PROCESSING_LOLIM_GIVEN && indepData) {
#if DEBUG
    fprintf(stderr, "lower limit processing: value %e\n", processing_ptr->lowerLimit);
#endif
    for (i = 0; i < n_data; i++)
      if (processing_ptr->lowerLimit > indepData[i])
	keep[i] = 0;
  }

  if (processing_ptr->flags & PROCESSING_UPLIM_GIVEN && indepData) {
#if DEBUG
    fprintf(stderr, "upper limit processing: value %e\n", processing_ptr->upperLimit);
#endif
    for (i = 0; i < n_data; i++) {
      if (processing_ptr->upperLimit < indepData[i])
	keep[i] = 0;
      if (isinf(indepData[i]) || isnan(indepData[i]))
        keep[i] = 0;
    }
  }

  if (processing_ptr->flags & PROCESSING_TOPLIM_GIVEN) {
    for (i = 0; i < n_data; i++) {
      if (processing_ptr->topLimit < data[i])
	keep[i] = 0;
      if (isinf(data[i]) || isnan(data[i]))
        keep[i] = 0;
    }
  }

  if (processing_ptr->flags & PROCESSING_BOTLIM_GIVEN) {
    for (i = 0; i < n_data; i++)
      if (processing_ptr->bottomLimit > data[i])
	keep[i] = 0;
  }

  if (processing_ptr->flags & PROCESSING_HEAD_GIVEN) {
    count = 0;
#if DEBUG
    fprintf(stderr, "head processing: %ld points\n", processing_ptr->head);
#endif
    if (processing_ptr->head > 0) {
      /* keep first head points */
      for (i = 0; i < n_data && count < processing_ptr->head; i++)
        if (keep[i])
          count++;
      for (; i < n_data; i++)
	keep[i] = 0;
    } else {
      /* kill first -head points */
      for (i = 0; i < n_data && count < -processing_ptr->head; i++)
        if (keep[i]) {
          count++;
	  keep[i] = 0;
        }
    }
  }

  if (processing_ptr->flags & PROCESSING_FHEAD_GIVEN) {
    long head;
    count = 0;
    head = fabs(n_data * processing_ptr->fhead + 0.5);
    if (processing_ptr->fhead > 0) {
      for (i = 0; i < n_data && count < head; i++)
        if (keep[i])
          count++;
      for (; i < n_data; i++)
        keep[i] = 0;
    } else
      for (i = 0; i < n_data && count < head; i++)
        if (keep[i]) {
          count++;
          keep[i] = 0;
        }
  }

  if (processing_ptr->flags & PROCESSING_TAIL_GIVEN) {
    count = 0;
#if DEBUG
    fprintf(stderr, "tail processing: %ld points\n", processing_ptr->tail);
#endif
    if (processing_ptr->tail > 0) {
      for (i = n_data - 1; i >= 0 && count < processing_ptr->tail; i--)
        if (keep[i])
          count++;
      for (; i >= 0; i--)
        keep[i] = 0;
    } else
      for (i = n_data - 1; i >= 0 && count < -processing_ptr->tail; i--)
        if (keep[i]) {
          count++;
          keep[i] = 0;
        }
  }
  
  if (processing_ptr->flags & PROCESSING_FTAIL_GIVEN) {
    long tail;
    count = 0;
    tail = fabs(processing_ptr->ftail * n_data + 0.5);
    if (processing_ptr->ftail > 0) {
      for (i = n_data - 1; i >= 0 && count < tail; i--)
        if (keep[i])
          count++;
      for (; i >= 0; i--)
        keep[i] = 0;
    } else
      for (i = n_data - 1; i >= 0 && count < tail; i--)
        if (keep[i]) {
          count++;
          keep[i] = 0;
        }
  }

  if (keep) {
    int64_t j;
    for (i = j = 0; i < n_data; i++) {
      if (keep[i]) {
        if (i != j) {
          data[j] = data[i];
          if (indepData)
            indepData[j] = indepData[i];
          if (stringData) {
            free(stringData[j]);
            stringData[j] = stringData[i];
            stringData[i] = NULL;
          }
          if (weightData)
            weightData[j] = weightData[i];
        }
        j++;
      }
    }
    n_data = j;
#if DEBUG
    fprintf(stderr, "%" PRId64 " points kept\n", n_data);
#endif
  }
  if (n_data == 0) {
    free(data);
    if (indepData)
      free(indepData);
    if (weightData)
      free(weightData);
    if (keep)
      free(keep);
    if (warnings)
      fprintf(stderr, "warning: no matches found");
    if (mode != PROCESS_COLUMN_COUNT) {
      *result = processing_ptr->defaultValue;
      *stringResult = NULL;
      return 1;
    } else {
      *result = 0;
      *stringResult = NULL;
      return 1;
    }
  }

  if (processing_ptr->flags & PROCESSING_OFFSET_GIVEN) {
#if DEBUG
    fprintf(stderr, "offsetting by %f\n", processing_ptr->offset);
#endif
    for (i = 0; i < n_data; i++)
      data[i] += processing_ptr->offset;
  }

  if (processing_ptr->flags & PROCESSING_FACTOR_GIVEN) {
#if DEBUG
    fprintf(stderr, "multiplying by %f\n", processing_ptr->factor);
#endif
    for (i = 0; i < n_data; i++)
      data[i] *= processing_ptr->factor;
  }

#if DEBUG
  fprintf(stderr, "data points:\n");
  for (i = 0; i < n_data; i++)
    fprintf(stderr, "%e%c", data[i], (i + 1) % 8 == 0 ? '\n' : ' ');
  fputc('\n', stderr);
#endif

  *result = 0;
  returnValue = 1;
  switch (mode) {
  case PROCESS_COLUMN_ZEROCROSSING:
    for (i = 0; i < n_data - 1; i++) {
      if (data[i] == 0)
        break;
      if ((data[i] < 0 && data[i + 1] >= 0) || (data[i] > 0 && data[i + 1] <= 0))
        break;
    }
    if (i != n_data - 1) {
      if (indepData) {
        if (data[i] == 0)
          *result = indepData[i];
        else
          *result = indepData[i] + (indepData[i + 1] - indepData[i]) / (data[i + 1] - data[i]) * (-data[i]);
      }
      if (stringData) {
        *stringResult = stringData[i];
        stringData[i] = NULL;
      }
    } else
      returnValue = 0;
    break;
  case PROCESS_COLUMN_MEAN:
    if (weightData)
      *result = weightedAverageThreaded(data, weightData, n_data, threads);
    else
      *result = arithmeticAverageThreaded(data, n_data, threads);
    break;
  case PROCESS_COLUMN_RMS:
    if (weightData)
      *result = weightedRMSThreaded(data, weightData, n_data, threads);
    else
      *result = rmsValueThreaded(data, n_data, threads);
    break;
  case PROCESS_COLUMN_SUM:
    if (weightData) {
      double sum;
      for (i = sum = 0; i < n_data; i++)
        sum += data[i] * weightData[i];
      *result = sum;
    } else
      *result = arithmeticAverageThreaded(data, n_data, threads) * n_data;
    break;
  case PROCESS_COLUMN_PRODUCT:
    if (weightData) {
      double product = 1;
      for (i = 0; i < n_data; i++)
        product *= data[i] * weightData[i];
      *result = product;
    } else {
      double product = 1;
      for (i = 0; i < n_data; i++)
        product *= data[i];
      *result = product;
    }
    break;
  case PROCESS_COLUMN_STAND_DEV:
    if (weightData)
      *result = weightedStDevThreaded(data, weightData, n_data, threads);
    else
      *result = standardDeviationThreaded(data, n_data, threads);
    break;
  case PROCESS_COLUMN_SIGMA:
    if (weightData)
      *result = weightedStDevThreaded(data, weightData, n_data, threads);
    else
      *result = standardDeviationThreaded(data, n_data, threads);
    *result /= sqrt(1.0 * n_data);
    break;
  case PROCESS_COLUMN_MAD:
    if (weightData)
      *result = weightedMADThreaded(data, weightData, n_data, threads);
    else
      *result = meanAbsoluteDeviationThreaded(data, n_data, threads);
    break;
  case PROCESS_COLUMN_SIGNEDSMALLEST:
    imin = 0;
    for (i = 1; i < n_data; i++)
      if (fabs(data[imin]) > fabs(data[i]))
        imin = i;
    *result = data[imin];
    if (processing_ptr->flags & PROCESSING_POSITION_GIVEN) {
      if (indepData)
        *result = indepData[imin];
      if (stringData) {
        *stringResult = stringData[imin];
        stringData[imin] = NULL;
      }
    }
    break;
  case PROCESS_COLUMN_SIGNEDLARGEST:
    imax = 0;
    for (i = 1; i < n_data; i++)
      if (fabs(data[imax]) < fabs(data[i]))
        imax = i;
    *result = data[imax];
    if (processing_ptr->flags & PROCESSING_POSITION_GIVEN) {
      if (indepData)
        *result = indepData[imax];
      if (stringData) {
        *stringResult = stringData[imax];
        stringData[imax] = NULL;
      }
    }
    break;
  case PROCESS_COLUMN_SMALLEST:
  case PROCESS_COLUMN_LARGEST:
    for (i = 0; i < n_data; i++)
      data[i] = fabs(data[i]);
    if (!index_min_max(&imin, &imax, data, n_data))
      returnValue = 0;
    else {
      switch (mode) {
      case PROCESS_COLUMN_SMALLEST:
        *result = data[i1 = imin];
        break;
      case PROCESS_COLUMN_LARGEST:
        *result = data[i1 = imax];
        break;
      }
      if (processing_ptr->flags & PROCESSING_POSITION_GIVEN) {
        if (indepData)
          *result = indepData[i1];
        if (stringData) {
          *stringResult = stringData[i1];
          stringData[i1] = NULL;
        }
      }
    }
    break;
  case PROCESS_COLUMN_MINIMUM:
  case PROCESS_COLUMN_MAXIMUM:
  case PROCESS_COLUMN_SPREAD:
    if (!index_min_max(&imin, &imax, data, n_data))
      returnValue = 0;
    else {
      min = data[imin];
      max = data[imax];
      switch (mode) {
      case PROCESS_COLUMN_MINIMUM:
        i1 = imin;
        *result = min;
        break;
      case PROCESS_COLUMN_MAXIMUM:
        i1 = imax;
        *result = max;
        break;
      case PROCESS_COLUMN_SMALLEST:
        if (fabs(min) < fabs(max)) {
          i1 = imin;
          *result = fabs(min);
        } else {
          i1 = imax;
          *result = fabs(max);
        }
        break;
      case PROCESS_COLUMN_LARGEST:
        if (fabs(min) > fabs(max)) {
          i1 = imin;
          *result = fabs(min);
        } else {
          i1 = imax;
          *result = fabs(max);
        }
        break;
      case PROCESS_COLUMN_SPREAD:
        *result = max - min;
        break;
      }
      if (processing_ptr->flags & PROCESSING_POSITION_GIVEN) {
        if (indepData)
          *result = indepData[i1];
        if (stringData) {
          *stringResult = stringData[i1];
          stringData[i1] = NULL;
        }
      }
    }
    break;
  case PROCESS_COLUMN_FIRST:
    *result = data[0];
    break;
  case PROCESS_COLUMN_LAST:
    *result = data[n_data - 1];
    break;
  case PROCESS_COLUMN_COUNT:
    *result = n_data;
    break;
  case PROCESS_COLUMN_MEDIAN:
    if (compute_median(result, data, n_data) <= 0)
      returnValue = 0;
    break;
  case PROCESS_COLUMN_MODE:
    if (processing_ptr->flags & PROCESSING_BINSIZE_GIVEN) {
      if (computeMode(result, data, n_data, processing_ptr->binSize, 0) <= 0)
        returnValue = 0;
    } else if (computeMode(result, data, n_data, 0, 100) <= 0)
      returnValue = 0;
    break;
  case PROCESS_COLUMN_BASELEVEL:
  case PROCESS_COLUMN_TOPLEVEL:
  case PROCESS_COLUMN_AMPLITUDE:
  case PROCESS_COLUMN_RISETIME:
  case PROCESS_COLUMN_FALLTIME:
  case PROCESS_COLUMN_FWHA:
  case PROCESS_COLUMN_FWTA:
  case PROCESS_COLUMN_CENTER:
  case PROCESS_COLUMN_FWHM:
  case PROCESS_COLUMN_FWTM:
    if (!findTopBaseLevels(&top, &base, data, n_data, 50, 2.0) ||
        !index_min_max(&imin, &imax, data, n_data))
      returnValue = 0;
    else {
      if ((top - base) < 0.75 * (data[imax] - base))
        top = data[imax];
      switch (mode) {
      case PROCESS_COLUMN_BASELEVEL:
        *result = base;
        break;
      case PROCESS_COLUMN_TOPLEVEL:
        *result = top;
        break;
      case PROCESS_COLUMN_AMPLITUDE:
        *result = top - base;
        break;
      case PROCESS_COLUMN_RISETIME:
        if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.1, 1, indepData, &point1)) < 0 ||
            (i2 = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, 1, indepData, &point2)) < 0)
          returnValue = 0;
        else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_FALLTIME:
        if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.9, -1, indepData, &point1)) < 0 ||
            (i2 = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.1, -1, indepData, &point2)) < 0)
          returnValue = 0;
        else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_FWHA:
        if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.5, 1, indepData, &point1)) < 0 ||
            (i2 = i2save = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, base + (top - base) * 0.5, -1, indepData, &point2)) < 0) {
          if (warnings) {
            fprintf(stderr, "warning: couldn't find crossing point for FWHA of %s\n",
                    processing_ptr->column_name);
            fprintf(stderr, "top, base = %e, %e   min, max = %e, %e\n", top, base,
                    data[imin], data[imax]);
            if (i1 > 0)
              fprintf(stderr, "#1: %ld, (%e, %e)\n",
                      i1, indepData[i1], data[i1]);
            if (i2save > 0)
              fprintf(stderr, "#2: %ld, (%e, %e)\n",
                      i2save, indepData[i2save], data[i2save]);
            if (i2 > 0)
              fprintf(stderr, "#2a: %ld, (%e, %e)\n",
                      i2, indepData[i2], data[i2]);
          }
          if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.5, 1, indepData, &point1)) < 0 ||
              (i2 = i2save = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, 1, NULL, NULL)) < 0 ||
              (i2 = findCrossingPoint(i2, data, n_data, base + (top - base) * 0.5, -1, indepData, &point2)) < 0) {
            returnValue = 0;
          } else {
            *result = point2 - point1;
          }
        } else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_FWTA:
        if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.1, 1, indepData, &point1)) < 0 ||
            (i2 = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, base + (top - base) * 0.1, -1, indepData, &point2)) < 0) {
          if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.1, 1, indepData, &point1)) < 0 ||
              (i2 = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, 1, NULL, NULL)) < 0 ||
              (i2 = findCrossingPoint(i2, data, n_data, base + (top - base) * 0.1, -1, indepData, &point2)) < 0) {
            returnValue = 0;
          } else {
            *result = point2 - point1;
          }
        } else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_FWHM:
        if ((i1 = findCrossingPoint(0, data, n_data, top * 0.5, 1, indepData, &point1)) < 0 ||
            (i2 = i2save = findCrossingPoint(i1, data, n_data, top * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, top * 0.5, -1, indepData, &point2)) < 0) {
          if (warnings) {
            fprintf(stderr, "warning: couldn't find crossing point for FWHM of %s\n",
                    processing_ptr->column_name);
            fprintf(stderr, "top, base = %e, %e\n", top, base);
            if (i1 > 0)
              fprintf(stderr, "#1: %ld, (%e, %e)\n",
                      i1, indepData[i1], data[i1]);
            if (i2save > 0)
              fprintf(stderr, "#2: %ld, (%e, %e)\n",
                      i2save, indepData[i2save], data[i2save]);
          }
          if ((i1 = findCrossingPoint(0, data, n_data, top * 0.5, 1, indepData, &point1)) < 0 ||
              (i2 = i2save = findCrossingPoint(i1, data, n_data, top * 0.9, 1, NULL, NULL)) < 0 ||
              (i2 = findCrossingPoint(i2, data, n_data, top * 0.5, -1, indepData, &point2)) < 0) {
            returnValue = 0;
          } else {
            *result = point2 - point1;
          }
        } else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_FWTM:
        if ((i1 = findCrossingPoint(0, data, n_data, top * 0.1, 1, indepData, &point1)) < 0 ||
            (i2 = findCrossingPoint(i1, data, n_data, top * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, top * 0.1, -1, indepData, &point2)) < 0) {
          if ((i1 = findCrossingPoint(0, data, n_data, top * 0.1, 1, indepData, &point1)) < 0 ||
              (i2 = findCrossingPoint(i1, data, n_data, top * 0.9, 1, NULL, NULL)) < 0 ||
              (i2 = findCrossingPoint(i2, data, n_data, top * 0.1, -1, indepData, &point2)) < 0) {
            returnValue = 0;
          } else {
            *result = point2 - point1;
          }
        } else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_CENTER:
        if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.5, 1, indepData, &point1)) < 0 ||
            (i2 = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, base + (top - base) * 0.5, -1, indepData, &point2)) < 0) {
          if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.5, 1, indepData, &point1)) < 0 ||
              (i2 = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, 1, NULL, NULL)) < 0 ||
              (i2 = findCrossingPoint(i2, data, n_data, base + (top - base) * 0.5, -1, indepData, &point2)) < 0) {
            returnValue = 0;
          } else {
            *result = (point1 + point2) / 2;
          }
        } else
          *result = (point1 + point2) / 2;
        break;
      }
    }
    break;
  case PROCESS_COLUMN_SLOPE:
  case PROCESS_COLUMN_INTERCEPT:
  case PROCESS_COLUMN_LFSD:
    if (!unweightedLinearFit(indepData, data, n_data, &slope, &intercept, &variance))
      returnValue = 0;
    else
      switch (mode) {
      case PROCESS_COLUMN_SLOPE:
        *result = slope;
        break;
      case PROCESS_COLUMN_INTERCEPT:
        *result = intercept;
        break;
      case PROCESS_COLUMN_LFSD:
        *result = sqrt(variance);
        break;
      }
    break;
  case PROCESS_COLUMN_INTEGRAL:
    trapazoidIntegration(indepData, data, n_data, result);
    break;
  case PROCESS_COLUMN_GMINTEGRAL:
    GillMillerIntegration1(indepData, data, n_data, result);
    break;
  case PROCESS_COLUMN_QRANGE:
    if (!compute_percentiles(quartileResult, quartilePoint, 2, data, n_data))
      returnValue = 0;
    else
      *result = quartileResult[1] - quartileResult[0];
    break;
  case PROCESS_COLUMN_DRANGE:
    if (!compute_percentiles(decileResult, decilePoint, 2, data, n_data))
      returnValue = 0;
    else
      *result = decileResult[1] - decileResult[0];
    break;
  case PROCESS_COLUMN_PERCENTILE:
    if (!compute_percentiles(percentileResult, &processing_ptr->percentileLevel, 1, data, n_data))
      returnValue = 0;
    else {
      *result = percentileResult[0];
      returnValue = 1;
    }
    break;
  case PROCESS_COLUMN_PRANGE:
    percentilePoint[1] = 50 + processing_ptr->percentileLevel / 2.0;
    percentilePoint[0] = 50 - processing_ptr->percentileLevel / 2.0;
    if (!compute_percentiles(percentileResult, percentilePoint, 2, data, n_data))
      returnValue = 0;
    else {
      *result = percentileResult[1] - percentileResult[0];
      returnValue = 1;
    }
    break;
  case PROCESS_COLUMN_CORRELATION:
    *result = linearCorrelationCoefficient(data, indepData, NULL, NULL, n_data, NULL);
    returnValue = 1;
    break;
  default:
    returnValue = 0;
    break;
  }

  free(data);
  if (indepData)
    free(indepData);
  if (weightData)
    free(weightData);
  if (keep)
    free(keep);
  if (stringData) {
    for (i = 0; i < n_data; i++)
      if (stringData[i])
        free(stringData[i]);
    free(stringData);
  }
  if (!returnValue) {
    if (warnings)
      fprintf(stderr, "warning: processing of %s with mode %s failed--value %e returned\n",
              processing_ptr->column_name,
              process_column_name[processing_ptr->mode],
              processing_ptr->defaultValue);
    *result = processing_ptr->defaultValue;
    *stringResult = NULL;
  }
  return 1;
}

char *process_string_column(SDDS_DATASET *Dataset, PROCESSING_DEFINITION *processing_ptr, long warnings) {
  char **data, *result, **matchData;
  long mode, matchfound;
  int64_t i, n_data, count;
  short *keep;

  mode = processing_ptr->mode;
  matchData = NULL;
  matchfound = 0;

  if ((n_data = SDDS_CountRowsOfInterest(Dataset)) <= 0 && mode != PROCESS_COLUMN_COUNT)
    return NULL;

  if (!(data = SDDS_GetColumn(Dataset, processing_ptr->column_name)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  keep = NULL;
  if (processing_ptr->flags & (PROCESSING_TAIL_GIVEN | PROCESSING_HEAD_GIVEN |
                               PROCESSING_FTAIL_GIVEN | PROCESSING_FHEAD_GIVEN |
                               PROCESSING_MATCHCOLUMN_GIVEN)) {
    keep = tmalloc(sizeof(*keep) * n_data);
    for (i = 0; i < n_data; i++)
      keep[i] = 1;
  }

  if (processing_ptr->flags & PROCESSING_MATCHCOLUMN_GIVEN) {
    if (!(matchData = SDDS_GetColumn(Dataset, processing_ptr->match_column)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < n_data; i++)
      keep[i] = 0;
    for (i = 0; i < n_data; i++) {
      if (wild_match(matchData[i], processing_ptr->match_value)) {
        matchfound = 1;
        keep[i] = 1;
      }
    }
    for (i = 0; i < n_data; i++)
      free(matchData[i]);
    free(matchData);
    if (!matchfound) {
      if (warnings)
        fprintf(stderr, "Warning: no values in column %s match %s.\n",
                processing_ptr->match_column, processing_ptr->match_value);
      free(keep);
      for (i = 0; i < n_data; i++)
        free(data[i]);
      free(data);
      SDDS_CopyString(&result, "_NoResultFound_");
      return result;
    }
  }
  if (processing_ptr->flags & PROCESSING_HEAD_GIVEN) {
    count = 0;
    for (i = 0; i < n_data && count < processing_ptr->head; i++)
      if (keep[i])
        count++;
    for (; i < n_data; i++)
      keep[i] = 0;
  }

  if (processing_ptr->flags & PROCESSING_FHEAD_GIVEN) {
    long head;
    count = 0;
    head = n_data * processing_ptr->fhead + 0.5;
    for (i = 0; i < n_data && count < head; i++)
      if (keep[i])
        count++;
    for (; i < n_data; i++)
      keep[i] = 0;
  }

  if (processing_ptr->flags & PROCESSING_TAIL_GIVEN) {
    count = 0;
    for (i = n_data - 1; i >= 0 && count < processing_ptr->tail; i--)
      if (keep[i])
        count++;
    for (; i >= 0; i--)
      keep[i] = 0;
  }

  if (processing_ptr->flags & PROCESSING_FTAIL_GIVEN) {
    long tail;
    count = 0;
    tail = processing_ptr->ftail * n_data + 0.5;
    for (i = n_data - 1; i >= 0 && count < tail; i--)
      if (keep[i])
        count++;
    for (; i >= 0; i--)
      keep[i] = 0;
  }

  if (keep) {
    int64_t j;
    for (i = j = 0; i < n_data; i++) {
      if (keep[i]) {
        if (i != j)
          data[j] = data[i];
        j++;
      } else
        free(data[i]);
    }
    n_data = j;
  }
  if (n_data > 0) {
    switch (mode) {
    case PROCESS_COLUMN_FIRST:
      SDDS_CopyString(&result, data[0]);
      break;
    case PROCESS_COLUMN_LAST:
      SDDS_CopyString(&result, data[n_data - 1]);
      break;
    default:
      SDDS_Bomb("invalid processing mode for string data (sddsprocess)");
      exit(1);
      break;
    }
  } else {
    if (warnings)
      fprintf(stderr, "warning, no matches found.");
    SDDS_CopyString(&result, "_NoResultFound_");
  }
  for (i = 0; i < n_data; i++)
    free(data[i]);
  free(data);
  if (keep)
    free(keep);
  return result;
}

static char *PROCESSING_USAGE =
  " [-process=<column-name>,<analysis-name>,<result-name>[,description=<string>][,symbol=<string>][,weightBy=<column-name>]\n\
    [,match=<column-name>,value=<match-string>]\n\
    [,head=<number>][,tail=<number>][fhead=<fraction>][ftail=<fraction>][,topLimit=<value>][,bottomLimit=<value>]\n\
    [,functionOf=<column-name>[,lowerLimit=<value>|@parameter_name][,upperLimit=<value>|@<parameter_name>][,position]]\n\
    [,offset=<value>][,factor=<value>][match=<column-name>,value=<string>][,overwrite],[default=<value>]]\n\
    [,binSize=<value>]\n";

PROCESSING_DEFINITION *record_processing_definition(char **argument, long arguments) {
  PROCESSING_DEFINITION *pd;
  char *lower_str = NULL, *upper_str = NULL;
  char *head_str=NULL, *tail_str=NULL, *fhead_str=NULL, *ftail_str=NULL, *offset_str=NULL, *factor_str=NULL;
  char *invert_str=NULL;
  pd = tmalloc(sizeof(*pd));
  if (arguments < 3)
    bomb("invalid -process syntax--wrong number of arguments", PROCESSING_USAGE);
  SDDS_CopyString(&pd->column_name, argument[0]);
  if (process_column_name[0] == NULL) {
    long i;
    for (i = 0; i < N_PROCESS_COLUMN_MODES; i++)
      process_column_name[i] = process_column_data[i].name;
  }
  pd->lower_par = pd->upper_par = pd->head_par = pd->tail_par = pd->fhead_par = pd->ftail_par = pd->offset_par=pd->factor_par=NULL;
  if ((pd->mode = match_string(argument[1], process_column_name, N_PROCESS_COLUMN_MODES, 0)) < 0) {
    fprintf(stderr, "invalid -process mode: %s\n", argument[1]);
    show_process_modes(stderr);
    exit(1);
  }

  SDDS_CopyString(&pd->parameter_name, argument[2]);
  argument += 3;
  arguments -= 3;
  pd->defaultValue = DBL_MAX;
  if (!scanItemList(&pd->flags, argument, &arguments, 0,
                    "functionof", SDDS_STRING, &pd->functionOf, 1, PROCESSING_FUNCOF_GIVEN,
                    "weightby", SDDS_STRING, &pd->weightBy, 1, PROCESSING_WEIGHT_GIVEN,
                    "description", SDDS_STRING, &pd->description, 1, PROCESSING_DESCRIP_GIVEN,
                    "symbol", SDDS_STRING, &pd->symbol, 1, PROCESSING_SYMBOL_GIVEN,
                    "toplimit", SDDS_DOUBLE, &pd->topLimit, 1, PROCESSING_TOPLIM_GIVEN,
                    "bottomlimit", SDDS_DOUBLE, &pd->bottomLimit, 1, PROCESSING_BOTLIM_GIVEN,
                    "lowerlimit", SDDS_STRING, &lower_str, 1, PROCESSING_LOLIM_GIVEN,
                    "upperlimit", SDDS_STRING, &upper_str, 1, PROCESSING_UPLIM_GIVEN,
		    "head", SDDS_STRING, &head_str, 1, PROCESSING_HEAD_GIVEN,
                    "tail", SDDS_STRING, &tail_str, 1, PROCESSING_TAIL_GIVEN,
                    "fhead", SDDS_STRING, &fhead_str, 1, PROCESSING_FHEAD_GIVEN,
                    "ftail", SDDS_STRING, &ftail_str, 1, PROCESSING_FTAIL_GIVEN,
                    "position", -1, NULL, 0, PROCESSING_POSITION_GIVEN,
		    "offset", SDDS_STRING, &offset_str, 1, PROCESSING_OFFSET_GIVEN,
                    "factor", SDDS_STRING, &factor_str, 1, PROCESSING_FACTOR_GIVEN,
                    "percentlevel", SDDS_DOUBLE, &pd->percentileLevel, 1, PROCESSING_PERCLEVEL_GIVEN,
                    "binsize", SDDS_DOUBLE, &pd->binSize, 1, PROCESSING_BINSIZE_GIVEN,
                    "match", SDDS_STRING, &pd->match_column, 1, PROCESSING_MATCHCOLUMN_GIVEN,
                    "value", SDDS_STRING, &pd->match_value, 1, PROCESSING_MATCHVALUE_GIVEN,
                    "overwrite", -1, NULL, 0, PROCESSING_OVERWRITE_GIVEN,
		    "invert", SDDS_STRING, &invert_str, 1, 0,
                    "default", SDDS_DOUBLE, &pd->defaultValue, 1, PROCESSING_DEFAULTVALUE_GIVEN,
                    NULL))
    bomb("invalid -process syntax", PROCESSING_USAGE);
  if (invert_str) {
    if (strncmp_case_insensitive(invert_str, "offset", strlen(invert_str))==0)
      pd->flags |= PROCESSING_INVERT_OFFSET;
    else if (strncmp_case_insensitive(invert_str, "factor", strlen(invert_str))==0) 
      pd->flags |= PROCESSING_INVERT_FACTOR;
    else if (strncmp_case_insensitive(invert_str, "both", strlen(invert_str))==0) {
      pd->flags |= PROCESSING_INVERT_OFFSET;
      pd->flags |= PROCESSING_INVERT_FACTOR;
    } else {
      fprintf(stdout, "Warning, invalid invert %s provided for process.\n", invert_str);
    }
    free(invert_str);
  }
  if (lower_str) {
    if (wild_match(lower_str, "@*")) {
      SDDS_CopyString(&pd->lower_par, lower_str + 1);
      free(lower_str);
    } else if (!get_double(&pd->lowerLimit, lower_str)) {
      bomb("invalid lower limit provided for -process option", PROCESSING_USAGE);
    }
  }
  if (upper_str) {
    if (wild_match(upper_str, "@*")) {
      SDDS_CopyString(&pd->upper_par, upper_str + 1);
      free(upper_str);
    } else if (!get_double(&pd->upperLimit, upper_str)) {
      bomb("invalid upper limit provided for -process option", PROCESSING_USAGE);
    }
  }
  if (head_str) {
    if (wild_match(head_str, "@*")) {
      SDDS_CopyString(&pd->head_par, head_str + 1);
      free(head_str);
    } else if (!get_int(&pd->head, head_str)) {
      bomb("invalid head provided for -process option", PROCESSING_USAGE);
    }
  }
  if (tail_str) {
    if (wild_match(tail_str, "@*")) {
      SDDS_CopyString(&pd->tail_par, tail_str + 1);
      free(tail_str);
    } else if (!get_int(&pd->tail, tail_str)) {
      bomb("invalid tail provided for -process option", PROCESSING_USAGE);
    }
  }
  if (fhead_str) {
    if (wild_match(fhead_str, "@*")) {
      SDDS_CopyString(&pd->fhead_par, fhead_str + 1);
      free(fhead_str);
    } else if (!get_double(&pd->fhead, fhead_str)) {
      bomb("invalid fhead provided for -process option", PROCESSING_USAGE);
    }
  }
  if (ftail_str) {
    if (wild_match(ftail_str, "@*")) {
      SDDS_CopyString(&pd->ftail_par, ftail_str + 1);
      free(ftail_str);
    } else if (!get_double(&pd->ftail, ftail_str)) {
      bomb("invalid ftail provided for -process option", PROCESSING_USAGE);
    }
  }
  if (offset_str) {
    if (wild_match(offset_str, "@*")) {
      SDDS_CopyString(&pd->offset_par, offset_str + 1);
      free(offset_str);
    } else if (wild_match(offset_str, "%s*")) {
      SDDS_CopyString(&pd->offset_par, offset_str);
      free(ftail_str);
    } else if (!get_double(&pd->offset, offset_str)) {
      bomb("invalid offset provided for -process option", PROCESSING_USAGE);
    }
  }
  if (factor_str) {
    if (wild_match(factor_str, "@*")) {
      SDDS_CopyString(&pd->factor_par, factor_str + 1);
      free(factor_str);
    } else if (wild_match(factor_str, "%s*")) {
      SDDS_CopyString(&pd->factor_par, factor_str);
      free(factor_str);
    } else if (!get_double(&pd->factor, factor_str)) {
      bomb("invalid factor provided for -process option", PROCESSING_USAGE);
    }
  }

  if (pd->flags & PROCESSING_BINSIZE_GIVEN && pd->binSize <= 0)
    SDDS_Bomb("invalid -process syntax---bin size is zero");
  if (pd->flags & PROCESSING_FACTOR_GIVEN && pd->factor == 0)
    SDDS_Bomb("invalid -process syntax---factor field is zero");
  if ((process_column_data[pd->mode].flags & PROCMODE_FUNCOF_REQUIRED) &&
      !(pd->flags & PROCESSING_FUNCOF_GIVEN))
    SDDS_Bomb("invalid -process syntax---functionOf option required for given mode");
  if (pd->flags & PROCESSING_WEIGHT_GIVEN &&
      !(process_column_data[pd->mode].flags & PROCMODE_WEIGHT_OK))
    SDDS_Bomb("invalid -process specification---weightBy option not available for given mode");
  if (pd->flags & PROCESSING_POSITION_GIVEN) {
    if (!(pd->flags & PROCESSING_FUNCOF_GIVEN))
      SDDS_Bomb("invalid -process syntax---functionOf required with position option");
    if (!(process_column_data[pd->mode].flags & PROCMODE_POSITION_OK))
      SDDS_Bomb("invalid -process syntax---position option not permitted for given mode");
  }
  if (pd->flags & PROCESSING_LOLIM_GIVEN && pd->flags & PROCESSING_UPLIM_GIVEN &&
      pd->lowerLimit > pd->upperLimit)
    SDDS_Bomb("invalid -process syntax---lowerLimit>upperLimit");
  if (pd->flags & PROCESSING_TOPLIM_GIVEN && pd->flags & PROCESSING_BOTLIM_GIVEN && pd->topLimit < pd->bottomLimit)
    SDDS_Bomb("invalid -process syntax---bottomLimit>topLimit");
  if (pd->flags & PROCESSING_HEAD_GIVEN && !pd->head_par &&  pd->head == 0)
    SDDS_Bomb("invalid -process syntax---head=0");
  if (pd->flags & PROCESSING_FHEAD_GIVEN && !pd->fhead_par && pd->fhead == 0)
    SDDS_Bomb("invalid -process syntax---fhead=0");
  if (pd->flags & PROCESSING_TAIL_GIVEN && !pd->tail_par &&  pd->tail == 0)
    SDDS_Bomb("invalid -process syntax---tail=0");
  if (pd->flags & PROCESSING_FTAIL_GIVEN && !pd->ftail_par &&  pd->ftail == 0)
    SDDS_Bomb("invalid -process syntax---ftail=0");
  if (pd->flags & (PROCESSING_LOLIM_GIVEN | PROCESSING_UPLIM_GIVEN) && !(pd->flags & PROCESSING_FUNCOF_GIVEN))
    SDDS_Bomb("invalid -process syntax---must give -functionOf with limit options");
  if (pd->mode == PROCESS_COLUMN_PERCENTILE && !(pd->flags & PROCESSING_PERCLEVEL_GIVEN))
    SDDS_Bomb("invalid -process syntax---must give percentLevel with percentile processing");
  if (pd->flags & PROCESSING_MATCHCOLUMN_GIVEN && !(pd->flags & PROCESSING_MATCHVALUE_GIVEN))
    SDDS_Bomb("invalid -process syntax---must give value for match column");
  return (pd);
}

SDDS_ENUM_PAIR typeEnumPair_local[SDDS_NUM_TYPES + 1] = {
  {"longdouble", SDDS_LONGDOUBLE},
  {"double", SDDS_DOUBLE},
  {"float", SDDS_FLOAT},
  {"short", SDDS_SHORT},
  {"ushort", SDDS_USHORT},
  {"long", SDDS_LONG},
  {"ulong", SDDS_ULONG},
  {"long64", SDDS_LONG64},
  {"ulong64", SDDS_ULONG64},
  {"character", SDDS_CHARACTER},
  {"string", SDDS_STRING},
  {NULL, 0}};

long complete_processing_definitions(PROCESSING_DEFINITION **processing_definition,
                                     long processing_definitions,
                                     SDDS_DATASET *SDDS_dataset) {
  static char s[SDDS_MAXLINE], t[SDDS_MAXLINE];
  long i, index, index1;
  PARAMETER_DEFINITION *pardef = NULL, pardef1;
  SDDS_FIELD_INFORMATION SDDS_ParameterFieldInformation_local[SDDS_PARAMETER_FIELDS] = {
    {"name", offsetof(PARAMETER_DEFINITION, name), SDDS_STRING},
    {"symbol", offsetof(PARAMETER_DEFINITION, symbol), SDDS_STRING},
    {"units", offsetof(PARAMETER_DEFINITION, units), SDDS_STRING},
    {"description", offsetof(PARAMETER_DEFINITION, description), SDDS_STRING},
    {"format_string", offsetof(PARAMETER_DEFINITION, format_string), SDDS_STRING},
    {"type", offsetof(PARAMETER_DEFINITION, type), SDDS_LONG, typeEnumPair_local},
    {"fixed_value", offsetof(PARAMETER_DEFINITION, fixed_value), SDDS_STRING},
  };

  index1 = 0;

  for (i = 0; i < processing_definitions; i++) {
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, processing_definition[i]->column_name)) < 0) {
      sprintf(s, "column %s does not exist", processing_definition[i]->column_name);
      SDDS_SetError(s);
      return (0);
    }
    switch (processing_definition[i]->type =
              processing_definition[i]->outputType =
                SDDS_GetColumnType(SDDS_dataset, index)) {
    case SDDS_STRING:
      if (!(process_column_data[processing_definition[i]->mode].flags & PROCMODE_STRING_OK)) {
        fprintf(stderr, "column %s has the wrong type for processing (sddsprocess)\n",
                processing_definition[i]->column_name);
        exit(1);
      }
      break;
    case SDDS_LONG:
    case SDDS_SHORT:
    case SDDS_FLOAT:
    case SDDS_DOUBLE:
    case SDDS_LONG64:
    case SDDS_CHARACTER:
    case SDDS_ULONG64:
    case SDDS_ULONG:
    case SDDS_USHORT:
    case SDDS_LONGDOUBLE:
      break;
    default:
      fprintf(stderr, "column %s has the wrong type for processing (sddsprocess)\n",
              processing_definition[i]->column_name);
      exit(1);
      break;
    }
    if (processing_definition[i]->flags & PROCESSING_DESCRIP_GIVEN) {
      if (strstr(processing_definition[i]->description, "%s")) {
        sprintf(s, processing_definition[i]->description,
                processing_definition[i]->column_name);
        free(processing_definition[i]->description);
        SDDS_CopyString(&processing_definition[i]->description, s);
      }
    } else {
      sprintf(s, "%s%s", process_column_data[processing_definition[i]->mode].description,
              processing_definition[i]->column_name);
      SDDS_CopyString(&processing_definition[i]->description, s);
    }
    if (strstr(processing_definition[i]->parameter_name, "%s")) {
      sprintf(s, processing_definition[i]->parameter_name, processing_definition[i]->column_name);
      free(processing_definition[i]->parameter_name);
      SDDS_CopyString(&processing_definition[i]->parameter_name, s);
    }
    if (processing_definition[i]->flags & PROCESSING_WEIGHT_GIVEN &&
        SDDS_GetColumnIndex(SDDS_dataset, processing_definition[i]->weightBy) < 0) {
      sprintf(s, "column %s does not exist", processing_definition[i]->weightBy);
      SDDS_SetError(s);
      return 0;
    }
    if (processing_definition[i]->flags & PROCESSING_MATCHCOLUMN_GIVEN &&
        SDDS_GetColumnIndex(SDDS_dataset, processing_definition[i]->match_column) < 0) {
      sprintf(s, "column %s does not exist", processing_definition[i]->match_column);
      SDDS_SetError(s);
      return 0;
    }
    if (processing_definition[i]->flags & PROCESSING_FUNCOF_GIVEN) {
      if ((index1 = SDDS_GetColumnIndex(SDDS_dataset, processing_definition[i]->functionOf)) < 0) {
        sprintf(s, "column %s does not exist", processing_definition[i]->functionOf);
        SDDS_SetError(s);
        return 0;
      }
      if (processing_definition[i]->flags & PROCESSING_POSITION_GIVEN &&
          (processing_definition[i]->outputType = SDDS_GetColumnType(SDDS_dataset, index1)) == SDDS_STRING &&
          !(process_column_data[processing_definition[i]->mode].flags & PROCMODE_STRINGPOS_OK)) {
        sprintf(s, "Can't have string column for position data with %s processing",
                process_column_data[processing_definition[i]->mode].name);
        SDDS_SetError(s);
        return 0;
      }
    }
    if (processing_definition[i]->outputType != SDDS_STRING)
      sprintf(s, "&parameter name=\"%s\", type=double, description=\"%s\", ",
              processing_definition[i]->parameter_name, processing_definition[i]->description);
    else
      sprintf(s, "&parameter name=\"%s\", type=string, description=\"%s\", ",
              processing_definition[i]->parameter_name, processing_definition[i]->description);
    if (process_column_data[processing_definition[i]->mode].flags & PROCMODE_FUNCOF_UNITS ||
        (processing_definition[i]->flags & PROCESSING_FUNCOF_GIVEN &&
         processing_definition[i]->flags & PROCESSING_POSITION_GIVEN)) {
      if (SDDS_dataset->layout.column_definition[index1].units) {
        sprintf(t, "units=\"%s\", ", SDDS_dataset->layout.column_definition[index1].units);
        strcat(s, t);
      }
    } else if (process_column_data[processing_definition[i]->mode].flags & PROCMODE_YoX_UNITS) {
      t[0] = 0;
      if (!SDDS_StringIsBlank(SDDS_dataset->layout.column_definition[index].units) &&
          !SDDS_StringIsBlank(SDDS_dataset->layout.column_definition[index1].units))
        sprintf(t, "units=\"(%s)/(%s)\", ",
                SDDS_dataset->layout.column_definition[index].units,
                SDDS_dataset->layout.column_definition[index1].units);
      else if (!SDDS_StringIsBlank(SDDS_dataset->layout.column_definition[index].units))
        sprintf(t, "units=\"%s\", ",
                SDDS_dataset->layout.column_definition[index].units);
      else if (!SDDS_StringIsBlank(SDDS_dataset->layout.column_definition[index1].units))
        sprintf(t, "units=\"1/(%s)\", ",
                SDDS_dataset->layout.column_definition[index1].units);
      strcat(s, t);
    } else if (process_column_data[processing_definition[i]->mode].flags & PROCMODE_YtX_UNITS) {
      t[0] = 0;
      if (!SDDS_StringIsBlank(SDDS_dataset->layout.column_definition[index].units) &&
          !SDDS_StringIsBlank(SDDS_dataset->layout.column_definition[index1].units))
        sprintf(t, "units=\"%s*%s\", ",
                SDDS_dataset->layout.column_definition[index].units,
                SDDS_dataset->layout.column_definition[index1].units);
      else if (!SDDS_StringIsBlank(SDDS_dataset->layout.column_definition[index].units))
        sprintf(t, "units=\"%s\", ",
                SDDS_dataset->layout.column_definition[index].units);
      else if (!SDDS_StringIsBlank(SDDS_dataset->layout.column_definition[index1].units))
        sprintf(t, "units=\"%s\", ",
                SDDS_dataset->layout.column_definition[index1].units);
      strcat(s, t);
    } else if (process_column_data[processing_definition[i]->mode].flags & PROCMODE_Y_UNITS) {
      if (SDDS_dataset->layout.column_definition[index].units) {
        sprintf(t, "units=\"%s\", ", SDDS_dataset->layout.column_definition[index].units);
        strcat(s, t);
      }
    }
    if (processing_definition[i]->symbol) {
      sprintf(t, "symbol=\"%s\", ", processing_definition[i]->symbol);
      strcat(s, t);
    }
    strcat(s, "&end");
    if ((index = SDDS_GetParameterIndex(SDDS_dataset, processing_definition[i]->parameter_name)) >= 0 &&
        processing_definition[i]->flags & PROCESSING_OVERWRITE_GIVEN) {
      pardef1.name = pardef1.symbol = pardef1.units = pardef1.description =
        pardef1.format_string = pardef1.fixed_value = NULL;
      pardef1.type = -1;
      if (!SDDS_ParseNamelist((void *)&pardef1, SDDS_ParameterFieldInformation_local,
                              SDDS_PARAMETER_FIELDS, s)) {
        SDDS_SetError("Problem parsing parameter namelist");
        return 0;
      }
      if ((pardef1.symbol && !SDDS_ChangeParameterInformation(SDDS_dataset, "symbol", &pardef1.symbol, SDDS_BY_INDEX, index)) ||
          (pardef1.units && !SDDS_ChangeParameterInformation(SDDS_dataset, "units", &pardef1.units, SDDS_BY_INDEX, index)) ||
          (pardef1.description && !SDDS_ChangeParameterInformation(SDDS_dataset, "description", &pardef1.description, SDDS_BY_INDEX, index)) ||
          (pardef1.format_string && !SDDS_ChangeParameterInformation(SDDS_dataset, "format_string", &pardef1.format_string, SDDS_BY_INDEX, index)) ||
          (pardef1.fixed_value && !SDDS_ChangeParameterInformation(SDDS_dataset, "fixed_value", &pardef1.fixed_value, SDDS_BY_INDEX, index)) ||
          (pardef1.type >= 0 && !SDDS_ChangeParameterInformation(SDDS_dataset, "type", &pardef1.type, SDDS_BY_INDEX, index))) {
        SDDS_SetError("Problem changing parameter definitions");
        return 0;
      }
    } else {
      if (!SDDS_ProcessParameterString(SDDS_dataset, s, SDDS_WRITEONLY_DEFINITION) ||
          !(pardef = SDDS_GetParameterDefinition(SDDS_dataset, processing_definition[i]->parameter_name)))
        return (0);
      processing_definition[i]->memory_number = pardef->memory_number;
      SDDS_FreeParameterDefinition(pardef);
    }
  }
  return (1);
}

CONVERSION_DEFINITION *process_conversion_definition(char **argument, long arguments) {
  CONVERSION_DEFINITION *request;
  request = tmalloc(sizeof(*request));
  if (arguments != 5)
    return (NULL);
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    request->is_parameter = 0;
    break;
  case PARAMETER_BASED:
    request->is_parameter = 1;
    break;
  default:
    return (NULL);
  }
  argument++;
  arguments--;

  request->name = argument[0];
  request->new_units = argument[1];
  request->old_units = argument[2];
  if (sscanf(argument[3], "%lf", &request->factor) != 1)
    return (NULL);

  return (request);
}

#include <stddef.h>

char *substituteString(char *template, char *pattern, char *replacement) {
  char *ptr, *ptr0, *buffer, c;
  long count = 0, patternLength, newLength;

  if (!(patternLength = strlen(pattern)))
    return NULL;
  if (!(ptr = template))
    return NULL;
  while ((ptr = strstr(ptr, pattern))) {
    count++;
    ptr += patternLength;
  }
  if ((newLength = strlen(template) - count * patternLength + count * strlen(replacement) + 1) <= 0 ||
      !(buffer = malloc(newLength * sizeof(*buffer))))
    return NULL;

  ptr0 = template;
  *buffer = 0;
  while (ptr0 && *ptr0 && (ptr = strstr(ptr0, pattern))) {
    c = *ptr;
    *ptr = 0;
    strcat(buffer, ptr0);
    strcat(buffer, replacement);
    *ptr = c;
    ptr0 = ptr = ptr + patternLength;
  }
  if (ptr0 && *ptr0)
    strcat(buffer, ptr0);
  return buffer;
}

void expandDefinitions(DEFINITION **definition, long *definitions, SDDS_DATASET *SDDS_dataset) {
  long names, idef, jdef, is_parameter, templates, itemplate, definitionType, i, j;
  char **name, **wildcardName, *editSelection, **target, **excludeWildcardName = NULL;
  size_t structSize;
  void *defStruct;
  char editBuffer[SDDS_MAXLINE];
  long maxTemplates = 10, requireWildcard;
  struct {
    long offset;
    short isPointer, isBase;
    size_t arraySize;
    long index;
    char *template;
  } * targetData;

  is_parameter = 0;
  structSize = 0;

  if (!(targetData = SDDS_Malloc(sizeof(*targetData) * maxTemplates)))
    SDDS_Bomb("Memory allocation failure");

  for (idef = 0; idef < *definitions; idef++) {
    editSelection = NULL;
    requireWildcard = 0; /* If nonzero, don't try to process an expansion unless the
                           * name being expanded has wildcards.
                           */
    for (i = 0; i < maxTemplates; i++) {
      targetData[i].isPointer = 0;
    }
    switch (definitionType = (*definition)[idef].type) {
    case IS_EQUATION_DEFINITION:
      /* e.g., -define=column,%sDiff,"%sSet %sRead -",select=Current*Set,edit=%/Set// */
      wildcardName = &((EQUATION_DEFINITION *)(*definition)[idef].structure)->select;
      excludeWildcardName = &((EQUATION_DEFINITION *)(*definition)[idef].structure)->exclude;
      is_parameter = ((EQUATION_DEFINITION *)(*definition)[idef].structure)->is_parameter;
      editSelection = ((EQUATION_DEFINITION *)(*definition)[idef].structure)->editSelection;
      structSize = sizeof(EQUATION_DEFINITION);
      targetData[0].offset = offsetof(EQUATION_DEFINITION, name);
      targetData[1].offset = offsetof(EQUATION_DEFINITION, equation);
      targetData[2].offset = offsetof(EQUATION_DEFINITION, text);
      SDDS_CopyString(&targetData[0].template,
                      ((EQUATION_DEFINITION *)(*definition)[idef].structure)->name);
      SDDS_CopyString(&targetData[1].template,
                      ((EQUATION_DEFINITION *)(*definition)[idef].structure)->equation);
      SDDS_CopyString(&targetData[2].template,
                      ((EQUATION_DEFINITION *)(*definition)[idef].structure)->text);
      templates = 3;
      break;
    case IS_PRINT_DEFINITION:
      /* e.g., -print=column,%sLabel,"%s = %f%s",%s.name,%s,%s.units,select=Current* */
      wildcardName = &((PRINT_DEFINITION *)(*definition)[idef].structure)->select;
      excludeWildcardName = &((PRINT_DEFINITION *)(*definition)[idef].structure)->exclude;
      is_parameter = ((PRINT_DEFINITION *)(*definition)[idef].structure)->is_parameter;
      editSelection = ((PRINT_DEFINITION *)(*definition)[idef].structure)->editSelection;
      structSize = sizeof(PRINT_DEFINITION);
      targetData[0].offset = offsetof(PRINT_DEFINITION, new_name);
      targetData[1].offset = offsetof(PRINT_DEFINITION, text);
      SDDS_CopyString(&targetData[0].template,
                      ((PRINT_DEFINITION *)(*definition)[idef].structure)->new_name);
      SDDS_CopyString(&targetData[1].template,
                      ((PRINT_DEFINITION *)(*definition)[idef].structure)->text);
      if (((PRINT_DEFINITION *)(*definition)[idef].structure)->sources + 2 > maxTemplates) {
        maxTemplates = 2 * ((PRINT_DEFINITION *)(*definition)[idef].structure)->sources + 2;
        if (!(targetData = SDDS_Realloc(targetData, sizeof(*targetData) * maxTemplates)))
          SDDS_Bomb("Memory allocation failure");
      }
      for (i = 0; i < ((PRINT_DEFINITION *)(*definition)[idef].structure)->sources; i++) {
        targetData[i + 2].isPointer = 1;
        if (i == 0) {
          targetData[i + 2].isBase = 1;
          targetData[i + 2].arraySize = sizeof(*(((PRINT_DEFINITION *)(*definition)[idef].structure)->source)) *
                                        ((PRINT_DEFINITION *)(*definition)[idef].structure)->sources;
        } else
          targetData[i + 2].isBase = 0;
        targetData[i + 2].index = i;
        targetData[i + 2].offset = offsetof(PRINT_DEFINITION, source);
        SDDS_CopyString(&targetData[i + 2].template,
                        ((PRINT_DEFINITION *)(*definition)[idef].structure)->source[i]);
      }
      templates = ((PRINT_DEFINITION *)(*definition)[idef].structure)->sources + 2;
      break;
    case IS_SCAN_DEFINITION:
      /* e.g., -scan=column,%sNum,Current*,"%f" */
      wildcardName = &((SCAN_DEFINITION *)(*definition)[idef].structure)->source;
      requireWildcard = 1;
      is_parameter = ((SCAN_DEFINITION *)(*definition)[idef].structure)->is_parameter;
      structSize = sizeof(SCAN_DEFINITION);
      targetData[0].offset = offsetof(SCAN_DEFINITION, new_name);
      targetData[1].offset = offsetof(SCAN_DEFINITION, text);
      targetData[2].offset = offsetof(SCAN_DEFINITION, source);
      SDDS_CopyString(&targetData[0].template,
                      ((SCAN_DEFINITION *)(*definition)[idef].structure)->new_name);
      SDDS_CopyString(&targetData[1].template,
                      ((SCAN_DEFINITION *)(*definition)[idef].structure)->text);
      SDDS_CopyString(&targetData[2].template, "%s");
      templates = 3;
      break;
    case IS_EDIT_DEFINITION:
      /* e.g., -edit=column,%sEdited,String*,%/Fred/Sally/ */
      wildcardName = &((EDIT_DEFINITION *)(*definition)[idef].structure)->source;
      requireWildcard = 1;
      is_parameter = ((EDIT_DEFINITION *)(*definition)[idef].structure)->is_parameter;
      structSize = sizeof(EDIT_DEFINITION);
      targetData[0].offset = offsetof(EDIT_DEFINITION, new_name);
      targetData[1].offset = offsetof(EDIT_DEFINITION, text);
      targetData[2].offset = offsetof(EDIT_DEFINITION, source);
      SDDS_CopyString(&targetData[0].template,
                      ((EDIT_DEFINITION *)(*definition)[idef].structure)->new_name);
      SDDS_CopyString(&targetData[1].template,
                      ((EDIT_DEFINITION *)(*definition)[idef].structure)->text);
      SDDS_CopyString(&targetData[2].template, "%s");
      templates = 3;
      break;
    case IS_PROCESSING_DEFINITION:
      /* e.g., -process=Current*,average,%sMean */
      wildcardName = &((PROCESSING_DEFINITION *)(*definition)[idef].structure)->column_name;
      requireWildcard = 1;
      is_parameter = 0;
      structSize = sizeof(PROCESSING_DEFINITION);
      targetData[0].offset = offsetof(PROCESSING_DEFINITION, parameter_name);
      targetData[1].offset = offsetof(PROCESSING_DEFINITION, column_name);
      targetData[2].offset = offsetof(PROCESSING_DEFINITION, functionOf);
      targetData[3].offset = offsetof(PROCESSING_DEFINITION, weightBy);
      targetData[4].offset = offsetof(PROCESSING_DEFINITION, description);
      targetData[5].offset = offsetof(PROCESSING_DEFINITION, symbol);
      SDDS_CopyString(&targetData[0].template,
                      ((PROCESSING_DEFINITION *)(*definition)[idef].structure)->parameter_name);
      SDDS_CopyString(&targetData[1].template, "%s");
      SDDS_CopyString(&targetData[2].template,
                      ((PROCESSING_DEFINITION *)(*definition)[idef].structure)->functionOf);
      SDDS_CopyString(&targetData[3].template,
                      ((PROCESSING_DEFINITION *)(*definition)[idef].structure)->weightBy);
      SDDS_CopyString(&targetData[4].template,
                      ((PROCESSING_DEFINITION *)(*definition)[idef].structure)->description);
      SDDS_CopyString(&targetData[5].template,
                      ((PROCESSING_DEFINITION *)(*definition)[idef].structure)->symbol);
      templates = 6;
      break;
    case IS_CONVERSION_DEFINITION:
      /* e.g., -convert=column,Current*,mA,A,1000 */
      wildcardName = &((CONVERSION_DEFINITION *)(*definition)[idef].structure)->name;
      requireWildcard = 1;
      is_parameter = ((CONVERSION_DEFINITION *)(*definition)[idef].structure)->is_parameter;
      structSize = sizeof(CONVERSION_DEFINITION);
      targetData[0].offset = offsetof(CONVERSION_DEFINITION, name);
      SDDS_CopyString(&targetData[0].template, "%s");
      templates = 1;
      break;
    case IS_SYSTEM_DEFINITION:
      /* e.g., -system=column,SysResult%s,Command* */
      wildcardName = &((SYSTEM_DEFINITION *)(*definition)[idef].structure)->source;
      requireWildcard = 1;
      is_parameter = ((SYSTEM_DEFINITION *)(*definition)[idef].structure)->is_parameter;
      structSize = sizeof(SYSTEM_DEFINITION);
      targetData[0].offset = offsetof(SYSTEM_DEFINITION, new_name);
      targetData[1].offset = offsetof(SYSTEM_DEFINITION, text);
      targetData[2].offset = offsetof(SYSTEM_DEFINITION, source);
      SDDS_CopyString(&targetData[0].template,
                      ((SYSTEM_DEFINITION *)(*definition)[idef].structure)->new_name);
      SDDS_CopyString(&targetData[1].template,
                      ((SYSTEM_DEFINITION *)(*definition)[idef].structure)->text);
      SDDS_CopyString(&targetData[2].template, "%s");
      templates = 3;
      break;
    case IS_CAST_DEFINITION:
      /* e.g., -cast=column,%sLong,Current*,long */
      wildcardName = &((CAST_DEFINITION *)(*definition)[idef].structure)->source;
      requireWildcard = 1;
      is_parameter = ((CAST_DEFINITION *)(*definition)[idef].structure)->isParameter;
      structSize = sizeof(CAST_DEFINITION);
      targetData[0].offset = offsetof(CAST_DEFINITION, newName);
      targetData[1].offset = offsetof(CAST_DEFINITION, source);
      SDDS_CopyString(&targetData[0].template,
                      ((CAST_DEFINITION *)(*definition)[idef].structure)->newName);
      SDDS_CopyString(&targetData[1].template, "%s");
      templates = 2;
      break;
    default:
      wildcardName = NULL;
      templates = 0;
      break;
    }
    if ((!wildcardName || !*wildcardName) || (requireWildcard && !has_wildcards(*wildcardName))) {
      if (wildcardName && *wildcardName)
        unescape_wildcards(*wildcardName);
      continue;
    }

    if (is_parameter) {
      if ((excludeWildcardName == NULL) || (*excludeWildcardName == NULL)) {
        if ((names = SDDS_MatchParameters(SDDS_dataset, &name, SDDS_MATCH_STRING, FIND_ANY_TYPE,
                                          *wildcardName, SDDS_AND | SDDS_1_PREVIOUS)) <= 0) {
          fprintf(stderr, "Error (expandDefinitions): unable to find match to parameter name %s\n", *wildcardName);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
      } else {
        if ((names = SDDS_MatchParameters(SDDS_dataset, &name, SDDS_MATCH_EXCLUDE_STRING, FIND_ANY_TYPE,
                                          *wildcardName, *excludeWildcardName,
                                          SDDS_AND | SDDS_1_PREVIOUS)) <= 0) {
          fprintf(stderr, "Error (expandDefinitions): unable to find match to parameter name %s\n", *wildcardName);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
      }
    } else {
      if ((excludeWildcardName == NULL) || (*excludeWildcardName == NULL)) {
        if ((names = SDDS_MatchColumns(SDDS_dataset, &name, SDDS_MATCH_STRING, FIND_ANY_TYPE,
                                       *wildcardName, SDDS_AND | SDDS_1_PREVIOUS)) <= 0) {
          fprintf(stderr, "Error (expandDefinitions): unable to find match to column name %s\n", *wildcardName);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
      } else {
        if ((names = SDDS_MatchColumns(SDDS_dataset, &name, SDDS_MATCH_EXCLUDE_STRING, FIND_ANY_TYPE,
                                       *wildcardName, *excludeWildcardName,
                                       SDDS_AND | SDDS_1_PREVIOUS)) <= 0) {
          fprintf(stderr, "Error (expandDefinitions): unable to find match to column name %s\n", *wildcardName);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
      }
    }
    if (editSelection)
      edit_strings(name, names, editBuffer, editSelection);
    *definition = SDDS_Realloc(*definition, sizeof(**definition) * (*definitions - 1 + names));
    for (jdef = *definitions - 1; jdef > idef; jdef--) {
      (*definition)[jdef + names - 1].structure = (*definition)[jdef].structure;
      (*definition)[jdef + names - 1].type = (*definition)[jdef].type;
    }
    for (jdef = names - 1; jdef >= 0; jdef--) {
      if (jdef) {
        if (!(defStruct = (*definition)[idef + jdef].structure = calloc(1, structSize)))
          SDDS_Bomb("memory allocation failure (expandDefinitions)");
        memcpy((char *)defStruct, (char *)(*definition)[idef].structure, structSize);
      } else
        defStruct = (*definition)[idef + jdef].structure;
      (*definition)[idef + jdef].type = definitionType;
      for (itemplate = 0; itemplate < templates; itemplate++) {
        if (targetData[itemplate].isPointer) {
          char **ptr;
          if (targetData[itemplate].isBase) {
            *(char ***)((char *)defStruct + targetData[itemplate].offset) =
              SDDS_Malloc(targetData[itemplate].arraySize);
          }
          ptr = *(char ***)((char *)defStruct + targetData[itemplate].offset);
          target = ptr + targetData[itemplate].index;
        } else
          target = (char **)((char *)defStruct + targetData[itemplate].offset);
        if (!(*target =
                substituteString(targetData[itemplate].template, "%s", name[jdef])) &&
            targetData[itemplate].template) {
          fputs("Error: problem doing pattern substitution (expandDefinitions)\n", stderr);
          fprintf(stderr, "template is \"%s\", instance is \"%s\"\n",
                  targetData[itemplate].template, name[jdef]);
          exit(1);
        }
      }
    }
    *definitions += names - 1;
    idef += names - 1;
    for (j = 0; j < names; j++)
      free(name[j]);
    free(name);
  }
  free(targetData);
}

long scan_column_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *format,
                       char *edit_command) {
  char **source_value, char_value, *sourcePtr;
  long target_type, target_index, source_index;
  int64_t i, rows;
  float float_value;
  double double_value;
  int64_t long64_value;
  int32_t long_value;
  short short_value;
  static char s[SDDS_MAXLINE];

  if ((source_index = SDDS_GetColumnIndex(SDDS_dataset, source)) < 0 ||
      (target_index = SDDS_GetColumnIndex(SDDS_dataset, target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (SDDS_GetColumnType(SDDS_dataset, source_index) != SDDS_STRING) {
    fprintf(stderr, "error: source column %s has wrong type for scanning--must be string\n", source);
    return (0);
  }
  target_type = SDDS_GetColumnType(SDDS_dataset, target_index);

  if ((rows = SDDS_CountRowsOfInterest(SDDS_dataset)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  if (!(source_value = SDDS_GetInternalColumn(SDDS_dataset, source))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  for (i = 0; i < rows; i++) {
    sourcePtr = source_value[i];
    if (!source_value[i]) {
      if (SDDS_NUMERIC_TYPE(target_type))
        strcpy(s, "NaN");
      else
        strcpy(s, "\0");
      sourcePtr = s;
    } else if (edit_command && strlen(edit_command)) {
      strncpy(s, source_value[i], SDDS_MAXLINE - 1);
      sourcePtr = s;
      if (!edit_string(s, edit_command)) {
        fprintf(stderr, "error: unable to preedit source string \"%s\" to scan into %s\n", source_value[i], target);
        return (0);
      }
    }
    switch (target_type) {
    case SDDS_DOUBLE:
      if (sscanf(sourcePtr, format, &double_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", sourcePtr, target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, double_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_FLOAT:
      if (sscanf(sourcePtr, format, &float_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", sourcePtr, target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, float_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_LONG64:
      if (sscanf(sourcePtr, format, &long64_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", sourcePtr, target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, long64_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_LONG:
      if (sscanf(sourcePtr, format, &long_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", sourcePtr, target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, long_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_SHORT:
      if (sscanf(sourcePtr, format, &short_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", sourcePtr, target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, short_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_CHARACTER:
      if (sscanf(sourcePtr, format, &char_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", sourcePtr, target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, char_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_STRING:
      if (sscanf(sourcePtr, format, s) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", sourcePtr, target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, s, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    default:
      SDDS_Bomb("error: unknown type encountered--this shouldn't happen (scan_column_value)");
      break;
    }
  }
  return (1);
}

long scan_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *format,
                          char *edit_command) {
  char *source_value, **ptr, char_value;
  long target_type, target_index, source_index;
  float float_value;
  double double_value;
  int64_t long64_value;
  int32_t long_value;
  short short_value;
  static char s[SDDS_MAXLINE];

  if ((source_index = SDDS_GetParameterIndex(SDDS_dataset, source)) < 0 ||
      (target_index = SDDS_GetParameterIndex(SDDS_dataset, target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (SDDS_GetParameterType(SDDS_dataset, source_index) != SDDS_STRING) {
    fprintf(stderr, "error: source parameter %s has wrong type for scanning--must be string\n", source);
    return (0);
  }
  target_type = SDDS_GetParameterType(SDDS_dataset, target_index);

  if (!(ptr = SDDS_GetParameter(SDDS_dataset, source, NULL))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  source_value = *ptr;
  if (!source_value) {
    if (SDDS_NUMERIC_TYPE(target_type))
      SDDS_CopyString(&source_value, "NaN");
    else
      SDDS_CopyString(&source_value, "\0");
  } else if (edit_command) {
    strncpy(s, source_value, SDDS_MAXLINE - 1);
    if (!edit_string(s, edit_command)) {
      fprintf(stderr, "error: unable to preedit source string \"%s\" for scanning into %s\n", source_value, target);
      return (0);
    }
    free(source_value);
    SDDS_CopyString(&source_value, s);
  }
  switch (target_type) {
  case SDDS_DOUBLE:
    if (sscanf(source_value, format, &double_value) != 1) {
      fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\nsscanf string was:", source_value, target);
      fputs(format, stderr);
      fputc('\n', stderr);
      return (0);
    }
    if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, double_value, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    break;
  case SDDS_FLOAT:
    if (sscanf(source_value, format, &float_value) != 1) {
      fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\nsscanf string was:", source_value, target);
      fputs(format, stderr);
      fputc('\n', stderr);
      return (0);
    }
    if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, float_value, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    break;
  case SDDS_LONG64:
    if (sscanf(source_value, format, &long64_value) != 1) {
      fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\nsscanf string was:", source_value, target);
      fputs(format, stderr);
      fputc('\n', stderr);
      return (0);
    }
    if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, long64_value, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    break;
  case SDDS_LONG:
    if (sscanf(source_value, format, &long_value) != 1) {
      fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\nsscanf string was:", source_value, target);
      fputs(format, stderr);
      fputc('\n', stderr);
      return (0);
    }
    if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, long_value, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    break;
  case SDDS_SHORT:
    if (sscanf(source_value, format, &short_value) != 1) {
      fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\nsscanf string was:", source_value, target);
      fputs(format, stderr);
      fputc('\n', stderr);
      return (0);
    }
    if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, short_value, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    break;
  case SDDS_CHARACTER:
    if (sscanf(source_value, format, &char_value) != 1) {
      fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\nsscanf string was:", source_value, target);
      fputs(format, stderr);
      fputc('\n', stderr);
      return (0);
    }
    if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, char_value, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    break;
  case SDDS_STRING:
    if (sscanf(source_value, format, s) != 1) {
      fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\nsscanf string was:", source_value, target);
      fputs(format, stderr);
      fputc('\n', stderr);
      return (0);
    }
    if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, s, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    break;
  default:
    SDDS_Bomb("error: unknown type encountered--this shouldn't happen (scan_parameter_value)");
    break;
  }
  free(source_value);
  return (1);
}

PRINT_DEFINITION *process_new_print_definition(char **argument, long arguments) {
  static char s[SDDS_MAXLINE];
  char *ptr;
  long i, hasType, code;
  PRINT_DEFINITION *defi;

  hasType = 0;
  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_print_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    sprintf(s, "&column name=\"%s\", ", argument[1]);
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    sprintf(s, "&parameter name=\"%s\", ", argument[1]);
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for print\n");
    return (NULL);
  }
  defi->new_name = argument[1];
  defi->source = NULL;
  defi->sources = 0;
  defi->printf_string = argument[2];

  for (i = 3; i < arguments; i++) {
    if (!(ptr = strchr(argument[i], '='))) {
      /* source entry */
      defi->source = trealloc(defi->source, sizeof(*defi->source) * (defi->sources + 1));
      defi->source[defi->sources++] = argument[i];
    } else
      break;
  }
  for (; i < arguments; i++) {
    /* should only have qualifiers from here on */
    if (!(ptr = strchr(argument[i], '='))) {
      fprintf(stderr, "error: invalid definition-entry: %s\n", argument[i]);
      return (NULL);
    }
    *ptr = 0;
    switch (code = match_string(argument[i], selectQualifier, SELECT_QUALIFIERS, 0)) {
    case SELECT_QUALIFIER:
      defi->select = ptr + 1;
      break;
    case EDITSELECTION_QUALIFIER:
      defi->editSelection = ptr + 1;
      break;
    case EXCLUDE_QUALIFIER:
      defi->exclude = ptr + 1;
      break;
    default:
      break;
    }
    *ptr = '=';
    if (code >= 0)
      continue;
    *ptr = 0;
    strcat(s, argument[i]);
    if (strcmp(argument[i], "type") == 0)
      hasType = 1;
    strcat(s, "=\"");
    strcat(s, ptr + 1);
    strcat(s, "\", ");
    *ptr = '=';
  }
  if (!hasType)
    strcat(s, " type=string &end");
  else
    strcat(s, " &end");
  if (!SDDS_CopyString(&defi->text, s))
    SDDS_Bomb("unable to copy text of print definition (process_new_print_definition)");
  return (defi);
}

long print_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char **source, long sources, char *format) {
  char *print_pos, **ptr, *fptr1, *fptr2 = NULL, *fptr3, *cptr;
  long source_type, source_index, target_index, target_type;
  long i, use_info;
  static char s[SDDS_MAXLINE];
  static char *info = NULL;

  if (!info && !(info = malloc(sizeof(double) * 2)))
    SDDS_Bomb("Allocation failure in print_parameter_value");

  if ((target_index = SDDS_GetParameterIndex(SDDS_dataset, target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if ((target_type = SDDS_GetParameterType(SDDS_dataset, target_index)) != SDDS_STRING &&
      target_type != SDDS_CHARACTER) {
    fprintf(stderr, "error: target parameter %s has wrong type for printing--must be string or character\n", target);
    return (0);
  }
  fptr1 = format;
  s[0] = 0;
  if (sources == 0)
    strcpy(s, format);
  for (i = 0; i < sources; i++) {
    print_pos = s + strlen(s);
    use_info = 0;
    if ((source_index = SDDS_GetParameterIndex(SDDS_dataset, source[i])) < 0 ||
        (source_type = SDDS_GetParameterType(SDDS_dataset, source_index)) < 0) {
      if ((cptr = strrchr(source[i], '.'))) {
        *cptr = 0;
        if (!(source_type = SDDS_GetParameterInformation(SDDS_dataset, cptr + 1, info, SDDS_GET_BY_NAME, source[i])) &&
            !(source_type = SDDS_GetColumnInformation(SDDS_dataset, cptr + 1, info, SDDS_GET_BY_NAME, source[i]))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (0);
        }
        if (strcmp(cptr + 1, "type") == 0) {
          if (source_type != SDDS_LONG)
            return bombre("internal error: type of data-type information is not long", NULL, 0);
          SDDS_CopyString((char **)info, SDDS_type_name[*(long *)info - 1]);
          source_type = SDDS_STRING;
        }
        *cptr = '.';
        use_info = 1;
        if (source_type == SDDS_STRING && *(char **)info == NULL)
          SDDS_CopyString((char **)info, "");
      } else {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
    }
    if (use_info)
      ptr = (char **)info;
    else if (!(ptr = SDDS_GetParameter(SDDS_dataset, source[i], NULL))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }

    fptr3 = fptr1;
    while (*fptr3) {
      if (!(fptr2 = strchr(fptr3, '%'))) {
        fprintf(stderr, "Error: invalid sprintf string\n");
        return (0);
      }
      if (*(fptr2 + 1) == '%')
        fptr3 = fptr2 + 2;
      else if (fptr2 != format && *(fptr2 - 1) == '\\')
        fptr3 = fptr2 + 1;
      else
        break;
    }
    if (*fptr3 == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if ((fptr2 = strchr(fptr2 + 1, '%')) && i != (sources - 1))
      *fptr2 = 0;
    else
      fptr2 = NULL;
    switch (source_type) {
    case SDDS_DOUBLE:
      sprintf(print_pos, fptr1, *(double *)ptr);
      break;
    case SDDS_FLOAT:
      sprintf(print_pos, fptr1, *(float *)ptr);
      break;
    case SDDS_LONG64:
      sprintf(print_pos, fptr1, *(int64_t *)ptr);
      break;
    case SDDS_ULONG64:
      sprintf(print_pos, fptr1, *(uint64_t *)ptr);
      break;
    case SDDS_LONG:
      sprintf(print_pos, fptr1, *(int32_t *)ptr);
      break;
    case SDDS_ULONG:
      sprintf(print_pos, fptr1, *(uint32_t *)ptr);
      break;
    case SDDS_SHORT:
      sprintf(print_pos, fptr1, *(short *)ptr);
      break;
    case SDDS_USHORT:
      sprintf(print_pos, fptr1, *(unsigned short *)ptr);
      break;
    case SDDS_CHARACTER:
      sprintf(print_pos, fptr1, *(char *)ptr);
      break;
    case SDDS_STRING:
      sprintf(print_pos, fptr1, *(char **)ptr);
      free(*ptr);
      break;
    default:
      SDDS_Bomb("error: unknown type encountered--this shouldn't happen (print_parameter_value)");
      break;
    }
    if (!(fptr1 = fptr2))
      break;
    *fptr1 = '%';
    if (!use_info)
      free(ptr);
  }
  if ((target_type == SDDS_STRING &&
       !SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, s, -1)) ||
      (target_type == SDDS_CHARACTER &&
       !SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, s[0], -1))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  return (1);
}

long print_column_value(SDDS_DATASET *SDDS_dataset, char *target, char **source, long sources, char *format) {
  char *print_pos, **ptr, *fptr1, *fptr2 = NULL, *fptr3, *cptr;
  long target_index, target_type;
  long i;
  int64_t j, rows;
  static char s[SDDS_MAXLINE];
  static long firstCall = 1;
  struct {
    long source_type, source_index;
    short is_parameter, is_column, use_info;
    char *info;
  } * sourceData;

  ptr = NULL;
  if ((target_index = SDDS_GetColumnIndex(SDDS_dataset, target)) < 0) {
    SDDS_SetError("Unable to find print target");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if ((target_type = SDDS_GetColumnType(SDDS_dataset, target_index)) != SDDS_STRING &&
      target_type != SDDS_CHARACTER) {
    fprintf(stderr, "error: target column %s has wrong type for printing--must be string or character\n", target);
    return (0);
  }
  if ((rows = SDDS_CountRowsOfInterest(SDDS_dataset)) < 0) {
    SDDS_SetError("Error counting rows to print.");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  if (!(sourceData = SDDS_Malloc(sizeof(*sourceData) * sources)))
    SDDS_Bomb("Memory allocation failure doing printing.");
  for (i = 0; i < sources; i++) {
    print_pos = s + strlen(s);
    sourceData[i].is_column = sourceData[i].is_parameter =
      sourceData[i].use_info = 0;
    if (((sourceData[i].source_index = SDDS_GetColumnIndex(SDDS_dataset, source[i])) >= 0) &&
        ((sourceData[i].source_type = SDDS_GetColumnType(SDDS_dataset, sourceData[i].source_index)) >= 0))
      sourceData[i].is_column = 1;
    else if ((sourceData[i].source_index = SDDS_GetParameterIndex(SDDS_dataset, source[i])) >= 0 &&
             (sourceData[i].source_type = SDDS_GetParameterType(SDDS_dataset, sourceData[i].source_index)) >= 0)
      sourceData[i].is_parameter = 1;
    else if ((cptr = strrchr(source[i], '.'))) {
      *cptr = 0;
      if ((sourceData[i].source_type =
             SDDS_GetParameterInformation(SDDS_dataset, cptr + 1, &sourceData[i].info, SDDS_GET_BY_NAME, source[i])))
        sourceData[i].is_parameter = 1;
      else if ((sourceData[i].source_type =
                  SDDS_GetColumnInformation(SDDS_dataset, cptr + 1, &sourceData[i].info, SDDS_GET_BY_NAME, source[i])))
        sourceData[i].is_column = 1;
      else {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      if (strcmp(cptr + 1, "type") == 0) {
        if (sourceData[i].source_type != SDDS_LONG)
          return bombre("internal error: type of data-type information is not long", NULL, 0);
        SDDS_CopyString((char **)sourceData[i].info, SDDS_type_name[*(long *)sourceData[i].info - 1]);
        sourceData[i].source_type = SDDS_STRING;
      }
      *cptr = '.';
      sourceData[i].use_info = 1;
      if (sourceData[i].source_type == SDDS_STRING && sourceData[i].info == NULL)
        SDDS_CopyString(&sourceData[i].info, "");
    } else {
      sprintf(s, "Unable to find print source %s.", source[i]);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
  }

  for (j = 0; j < rows; j++) {
    fptr1 = format;
    s[0] = 0;
    if (sources == 0)
      strcpy(s, format);
    for (i = 0; i < sources; i++) {
      print_pos = s + strlen(s);
      if (sourceData[i].is_column && !sourceData[i].use_info) {
        if (!(ptr = SDDS_GetValue(SDDS_dataset, source[i], j, NULL))) {
          SDDS_SetError("Unable to get column value for printing");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (0);
        }
        if (!j && firstCall && SDDS_GetParameterIndex(SDDS_dataset, source[i]) >= 0) {
          if (*ptr == NULL) {
            fprintf(stderr, "error: -print option for column %s uses source %s which is defined\n",
                    target, source[i]);
            fprintf(stderr, "as both a column and a parameter\n");
            return (0);
          } else {
            fprintf(stderr, "warning: -print option for column %s uses source %s which is defined\n",
                    target, source[i]);
            fprintf(stderr, "as both a column and a parameter---column values are used!\n");
          }
        }
      }
      if (sourceData[i].is_parameter && !sourceData[i].use_info) {
        if (!(ptr = SDDS_GetParameter(SDDS_dataset, source[i], NULL))) {
          SDDS_SetError("Unable to get print source parameter value.");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (0);
        }
      }
      if (sourceData[i].use_info)
        ptr = &sourceData[i].info;
      fptr3 = fptr1;
      while (*fptr3) {
        if (!(fptr2 = strchr(fptr3, '%'))) {
          fprintf(stderr, "Error: invalid sprintf string\n");
          return (0);
        }
        if (*(fptr2 + 1) == '%')
          fptr3 = fptr2 + 2;
        else if (fptr2 != format && *(fptr2 - 1) == '\\')
          fptr3 = fptr2 + 1;
        else
          break;
      }
      if (*fptr3 == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      if ((fptr2 = strchr(fptr2 + 1, '%')) && i != (sources - 1))
        *fptr2 = 0;
      else
        fptr2 = NULL;
      switch (sourceData[i].source_type) {
      case SDDS_DOUBLE:
        sprintf(print_pos, fptr1, *(double *)ptr);
        break;
      case SDDS_FLOAT:
        sprintf(print_pos, fptr1, *(float *)ptr);
        break;
      case SDDS_LONG64:
        sprintf(print_pos, fptr1, *(int64_t *)ptr);
        break;
      case SDDS_ULONG64:
        sprintf(print_pos, fptr1, *(uint64_t *)ptr);
        break;
      case SDDS_LONG:
        sprintf(print_pos, fptr1, *(int32_t *)ptr);
        break;
      case SDDS_ULONG:
        sprintf(print_pos, fptr1, *(uint32_t *)ptr);
        break;
      case SDDS_SHORT:
        sprintf(print_pos, fptr1, *(short *)ptr);
        break;
      case SDDS_USHORT:
        sprintf(print_pos, fptr1, *(unsigned short *)ptr);
        break;
      case SDDS_CHARACTER:
        sprintf(print_pos, fptr1, *(char *)ptr);
        break;
      case SDDS_STRING:
        sprintf(print_pos, fptr1, *(char **)ptr);
        if (!sourceData[i].use_info)
          free(*ptr);
        break;
      default:
        SDDS_Bomb("error: unknown type encountered--this shouldn't happen (print_column_value)");
        break;
      }
      if (!sourceData[i].use_info)
        free(ptr);
      if (!(fptr1 = fptr2))
        break;
      *fptr1 = '%';
    }
    if ((target_type == SDDS_STRING &&
         !SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, target_index, s, -1)) ||
        (target_type == SDDS_CHARACTER &&
         !SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, target_index, s[0],
                            -1))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
  }
  firstCall = 0;
  SDDS_Free(sourceData);
  return (1);
}

FORMAT_DEFINITION *process_new_format_definition(char **argument, long arguments) {
  FORMAT_DEFINITION *defi;
  unsigned long flags;

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_format_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for format\n");
    return (NULL);
  }
  defi->target = argument[1];
  defi->source = argument[2];
  defi->stringFormat = NULL;
  defi->doubleFormat = NULL;
  defi->longFormat = NULL;

  arguments -= 3;
  argument += 3;
  if (!scanItemList(&flags, argument, &arguments, 0,
                    "stringformat", SDDS_STRING, &defi->stringFormat, 1, 0,
                    "doubleformat", SDDS_STRING, &defi->doubleFormat, 1, 0,
                    "longformat", SDDS_STRING, &defi->longFormat, 1, 0,
                    NULL)) {
    bomb("invalid -format syntax", NULL);
  }
  return (defi);
}

long format_parameter_value(SDDS_DATASET *SDDS_dataset, FORMAT_DEFINITION *definition) {
  static char *buffer = NULL;
  static long bufferSize = 0;
  char *ptr, *doubleFormat, *longFormat, *stringFormat;
  long target_index, source_index, bufferRequired;

  if ((source_index = SDDS_GetParameterIndex(SDDS_dataset, definition->source)) < 0 || (target_index = SDDS_GetParameterIndex(SDDS_dataset, definition->target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (SDDS_GetParameterType(SDDS_dataset, source_index) != SDDS_STRING) {
    fprintf(stderr, "error: target parameter %s has wrong type for formatting--must be string\n", definition->source);
    return 0;
  }
  if (SDDS_GetParameterType(SDDS_dataset, target_index) != SDDS_STRING) {
    fprintf(stderr, "error: target parameter %s has wrong type for formatting--must be string\n", definition->target);
    return 0;
  }
  if (!(ptr = SDDS_GetParameter(SDDS_dataset, definition->source, NULL))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if ((bufferRequired = 2 * strlen(ptr) + 1024) > bufferSize) {
    /* guessing that string won't more than double in length */
    bufferSize = bufferRequired;
    if (!(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * bufferRequired))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
  }

  if (!(doubleFormat = definition->doubleFormat))
    doubleFormat = "%21.15e";
  if (!(longFormat = definition->longFormat))
    longFormat = "%ld";
  if (!(stringFormat = definition->stringFormat))
    stringFormat = "%s";

  if (!reformatString(buffer, bufferSize, ptr, stringFormat, doubleFormat, longFormat))
    return 0;

  if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, buffer, -1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  return 1;
}

long reformatString(char *buffer, long bufferSize, char *string, char *stringFormat,
                    char *doubleFormat, char *longFormat) {
  long bufferUseLimit, length, lValue;
  double dValue;
  char *bufferPosition, *token0, *token1;

  bufferPosition = buffer;
  *buffer = 0;
  bufferUseLimit = bufferSize * 0.9 + 1;

  token1 = string;
  while ((token0 = strtok(token1, " "))) {
    token1 = NULL;

    if (tokenIsInteger(token0)) {
      if (sscanf(token0, "%ld", &lValue) == 1)
        sprintf(bufferPosition, longFormat, lValue);
      else {
        fprintf(stderr, "Error: problem formatting token %s as long integer.\n", token0);
        return 0;
      }
    } else if (tokenIsNumber(token0)) {
      if (sscanf(token0, "%lf", &dValue) == 1)
        sprintf(bufferPosition, doubleFormat, dValue);
      else {
        fprintf(stderr, "Error: problem formatting token %s as double.\n", token0);
        return 0;
      }
    } else
      sprintf(bufferPosition, stringFormat, token0);
    if ((length = strlen(buffer)) > bufferUseLimit) {
      fprintf(stderr, "Error: format buffer overflow (sddsprocess).\n");
      return 0;
    }
    bufferPosition = buffer + length;
    *bufferPosition = ' ';
    *++bufferPosition = 0;
  }
  if ((--bufferPosition) >= buffer)
    *bufferPosition = 0;
  return 1;
}

long format_column_value(SDDS_DATASET *SDDS_dataset, FORMAT_DEFINITION *definition) {
  static char *buffer = NULL;
  static long bufferSize = 0;
  char **ptr, *longFormat, *doubleFormat, *stringFormat;
  long source_index, target_index, bufferRequired;
  int64_t row, rows;

  if ((source_index = SDDS_GetColumnIndex(SDDS_dataset, definition->source)) < 0 ||
      (target_index = SDDS_GetColumnIndex(SDDS_dataset, definition->target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (SDDS_GetColumnType(SDDS_dataset, source_index) != SDDS_STRING) {
    fprintf(stderr, "error: target column %s has wrong type for formatting--must be string\n", definition->source);
    return 0;
  }
  if (SDDS_GetColumnType(SDDS_dataset, target_index) != SDDS_STRING) {
    fprintf(stderr, "error: target column %s has wrong type for formatting--must be string\n", definition->target);
    return 0;
  }
  if ((rows = SDDS_CountRowsOfInterest(SDDS_dataset)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (!(ptr = SDDS_GetColumn(SDDS_dataset, definition->source))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }

  if (!(doubleFormat = definition->doubleFormat))
    doubleFormat = "%21.15e";
  if (!(longFormat = definition->longFormat))
    longFormat = "%ld";
  if (!(stringFormat = definition->stringFormat))
    stringFormat = "%s";

  for (row = 0; row < rows; row++) {
    if ((bufferRequired = 2 * strlen(ptr[row]) + 1024) > bufferSize) {
      bufferSize = bufferRequired;
      if (!(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * bufferRequired))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 0;
      }
    }
    if (!reformatString(buffer, bufferSize, ptr[row], stringFormat, doubleFormat, longFormat)) {
      fprintf(stderr, "Error: problem formatting string >%s< (sddsprocess).\n",
              ptr[row]);
      return 0;
    }
    if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, row, target_index, buffer, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
  }
  return 1;
}

void add_definition(DEFINITION **definition, long *definitions, void *structure, long type) {
  *definition = trealloc(*definition, sizeof(**definition) * (*definitions + 1));
  (*definition)[*definitions].structure = structure;
  (*definition)[*definitions].type = type;
  *definitions += 1;
}

RPNTEST_DEFINITION *process_new_rpntest_definition(char **argument, long arguments) {
  RPNTEST_DEFINITION *defi;
  int i, algebraic = 0;
  char pfix[IFPF_BUF_SIZE];

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_print_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for rpntest\n");
    return (NULL);
  }
  defi->autostop = 0;

  for (i = 2; i < arguments; i++) {
    if (strncmp(argument[i], "autostop", strlen(argument[i])) == 0)
      defi->autostop = 1;
    else if (strncmp(argument[i], "algebraic", strlen(argument[i])) == 0)
      algebraic = 1;
    else
      return (NULL);
  }
  if (algebraic) {
    char *ptr;
    ptr = addOuterParentheses(argument[1]);
    if2pf(pfix, ptr, sizeof pfix);
    free(ptr);
    if (!SDDS_CopyString(&defi->expression, pfix)) {
      fprintf(stderr, "error: problem copying argument string\n");
      return NULL;
    }
  } else
    defi->expression = argument[1];
  return (defi);
}

RPNEXPRESSION_DEFINITION *process_new_rpnexpression_definition(char **argument, long arguments) {
  RPNEXPRESSION_DEFINITION *defi;
  int i, algebraic = 0;
  char pfix[IFPF_BUF_SIZE];

  if (arguments > 2)
    return (NULL);
  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_rpnexpression_definition)");
  defi->repeat = 0;

  for (i = 1; i < arguments; i++) {
    if (strncmp(argument[i], "repeat", strlen(argument[i])) == 0)
      defi->repeat = 1;
    else if (strncmp(argument[i], "algebraic", strlen(argument[i])) == 0)
      algebraic = 1;
    else
      return NULL;
  }
  if (algebraic) {
    char *ptr;
    ptr = addOuterParentheses(argument[0]);
    if2pf(pfix, ptr, sizeof pfix);
    free(ptr);
    if (!SDDS_CopyString(&defi->expression, pfix)) {
      fprintf(stderr, "error: problem copying argument string\n");
      return NULL;
    }
  } else
    defi->expression = argument[0];
  return (defi);
}

CLIP_DEFINITION *process_new_clip_definition(char **argument, long arguments) {
  CLIP_DEFINITION *defi;

  if (arguments < 2 || arguments > 3)
    return (NULL);
  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_clip_definition)");
  if (sscanf(argument[0], "%" SCNd64, &defi->head) != 1 ||
      sscanf(argument[1], "%" SCNd64, &defi->tail) != 1 ||
      defi->head < 0 || defi->tail < 0)
    return NULL;

  defi->invert = 0;
  if (arguments == 3) {
    if (strncmp(argument[2], "invert", strlen(argument[2])) != 0)
      return NULL;
    defi->invert = 1;
  }
  return (defi);
}
FCLIP_DEFINITION *process_new_fclip_definition(char **argument, long arguments) {
  FCLIP_DEFINITION *defi;

  if (arguments < 2 || arguments > 3)
    return (NULL);
  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_clip_definition)");
  if (sscanf(argument[0], "%le", &defi->fhead) != 1 ||
      sscanf(argument[1], "%le", &defi->ftail) != 1 ||
      defi->fhead < 0 || defi->ftail < 0)
    return NULL;

  defi->invert = 0;
  if (arguments == 3) {
    if (strncmp(argument[2], "invert", strlen(argument[2])) != 0)
      return NULL;
    defi->invert = 1;
  }
  return (defi);
}
SPARSE_DEFINITION *process_new_sparse_definition(char **argument, long arguments) {
  SPARSE_DEFINITION *defi;

  if (arguments < 1 || arguments > 2)
    return (NULL);
  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_sparse_definition)");
  if (sscanf(argument[0], "%" SCNd64, &defi->interval) != 1 || defi->interval <= 0 ||
      (arguments == 2 && (sscanf(argument[1], "%" SCNd64, &defi->offset) != 1 || defi->offset < 0)))
    return NULL;
  return defi;
}

SAMPLE_DEFINITION *process_new_sample_definition(char **argument, long arguments) {
  SAMPLE_DEFINITION *defi;

  if (arguments < 1)
    return (NULL);
  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_sample_definition)");
  if (sscanf(argument[0], "%lf", &defi->fraction) != 1)
    return NULL;
  return defi;
}

SYSTEM_DEFINITION *process_new_system_definition(char **argument, long arguments) {
  static char s[SDDS_MAXLINE];
  char *ptr;
  long i;
  SYSTEM_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    SDDS_Bomb("memory allocation failure (process_new_system_definition)");
  switch (match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
  case COLUMN_BASED:
    defi->is_parameter = 0;
    sprintf(s, "&column name=\"%s\", ", argument[1]);
    break;
  case PARAMETER_BASED:
    defi->is_parameter = 1;
    sprintf(s, "&parameter name=\"%s\", ", argument[1]);
    break;
  default:
    fprintf(stderr, "error: column or parameter must be specified for system\n");
    return (NULL);
  }
  defi->new_name = argument[1];
  defi->source = argument[2];
  for (i = 3; i < arguments; i++) {
    if (!(ptr = strchr(argument[i], '=')))
      return (NULL);
    *ptr = 0;
    if (strcmp(argument[i], "type") == 0)
      continue;
    strcat(s, argument[i]);
    strcat(s, "=\"");
    strcat(s, ptr + 1);
    strcat(s, "\", ");
  }
  strcat(s, " type=string &end");
  if (!SDDS_CopyString(&defi->text, s))
    SDDS_Bomb("unable to copy text of system definition (process_new_system_definition)");
  return (defi);
}

long system_column_value(SDDS_DATASET *SDDS_dataset, char *target, char *source) {
  char **source_value, *command;
  int64_t i, rows;
  long target_index, source_index;
  static char s[SDDS_MAXLINE];

  if ((source_index = SDDS_GetColumnIndex(SDDS_dataset, source)) < 0 ||
      (target_index = SDDS_GetColumnIndex(SDDS_dataset, target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (SDDS_GetColumnType(SDDS_dataset, source_index) != SDDS_STRING) {
    fprintf(stderr, "error: source column %s has wrong type for system call--must be string\n", source);
    return (0);
  }
  if (SDDS_GetColumnType(SDDS_dataset, target_index) != SDDS_STRING) {
    fprintf(stderr, "error: target column %s has wrong type for system call--must be string\n", target);
    return (0);
  }

  if ((rows = SDDS_CountRowsOfInterest(SDDS_dataset)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  if (!(source_value = SDDS_GetInternalColumn(SDDS_dataset, source))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  for (i = 0; i < rows; i++) {
    s[0] = 0;
    if ((command = source_value[i]) && !run_on_pipe(command, s, SDDS_MAXLINE)) {
      sprintf(s, "Problem with subprocess call (system_column_value)---command was\n%s\n",
              command);
      SDDS_SetError(s);
      return (0);
    }
    if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, s, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
  }
  return (1);
}

long system_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char *source) {
  char *command, **ptr;
  long target_index, source_index;
  static char s[SDDS_MAXLINE];

  if ((source_index = SDDS_GetParameterIndex(SDDS_dataset, source)) < 0 ||
      (target_index = SDDS_GetParameterIndex(SDDS_dataset, target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (SDDS_GetParameterType(SDDS_dataset, source_index) != SDDS_STRING) {
    fprintf(stderr, "error: source parameter %s has wrong type for system call--must be string\n", source);
    return (0);
  }
  if (SDDS_GetParameterType(SDDS_dataset, target_index) != SDDS_STRING) {
    fprintf(stderr, "error: target parameter %s has wrong type for system call--must be string\n", target);
    return (0);
  }

  if (!(ptr = SDDS_GetParameter(SDDS_dataset, source, NULL))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  s[0] = 0;
  if ((command = *ptr) && !run_on_pipe(command, s, SDDS_MAXLINE)) {
    sprintf(s, "Problem with subprocess call (system_parameter_value)---command was\n%s\n",
            command);
    SDDS_SetError(s);
    return (0);
  }
  if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, s, -1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (command)
    free(command);
  free(ptr);
  return (1);
}

long run_on_pipe(char *command, char *buffer, long buffer_length) {
  FILE *fp;
  long length;

  if (!(fp = popen(command, "r")))
    return (0);
  buffer[0] = 0;
  if (!fgets(buffer, buffer_length, fp)) {
    pclose(fp);
    return (0);
  }
  if (buffer[(length = strlen(buffer)) - 1] == '\n')
    buffer[length - 1] = 0;
  pclose(fp);
  return (1);
}

/* used for redefining parameters using sddsprocess-style commandline arguments */
long SDDS_RedefineParameterCL(SDDS_DATASET *SDDS_dataset, char *parameter, char **argv, long argc) {
  char *value, *keyword;
  long i;

  for (i = 0; i < argc; i++) {
    keyword = argv[i];
    if (!(value = strchr(argv[i], '='))) {
      fprintf(stderr, "error: missing = in qualifier string for redefine: %s\n", argv[i]);
      return (0);
    }
    *value++ = 0;
    if (!SDDS_ChangeParameterInformation(SDDS_dataset, keyword, value, SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME,
                                         parameter)) {
      fprintf(stderr, "Problem redefining %s for %s\n",
              keyword, parameter);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    *--value = '=';
  }
  return (1);
}

/* used for redefining columns using sddsprocess-style commandline arguments */
long SDDS_RedefineColumnCL(SDDS_DATASET *SDDS_dataset, char *column, char **argv, long argc) {
  char *value, *keyword;
  long i;

  for (i = 0; i < argc; i++) {
    keyword = argv[i];
    if (!(value = strchr(argv[i], '='))) {
      fprintf(stderr, "error: missing = in qualifier string for redefine: %s\n", argv[i]);
      return (0);
    }
    *value++ = 0;
    if (!SDDS_ChangeColumnInformation(SDDS_dataset, keyword, value, SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME,
                                      column)) {
      fprintf(stderr, "Problem redefining %s for %s\n",
              keyword, column);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    *--value = '=';
  }
  return (1);
}

long add_sddsfile_arguments(SCANNED_ARG **scanned, int argc) {
  long iarg, datacode, index, argc_got;
  int64_t j;
  SDDS_DATASET dataset;
  static char *choice[2] = {"column", "parameter"};
  char *dataname, *filename, **argv_got;
  SCANNED_ARG *sa_got;

  index = 0;

  argv_got = NULL;
  for (iarg = 0; iarg < argc; iarg++) {
    if (!((*scanned)[iarg].arg_type == OPTION) ||
        strcmp((*scanned)[iarg].list[0], "getargs") != 0 ||
        (*scanned)[iarg].n_items != 3 ||
        !(dataname = strchr((*scanned)[iarg].list[2], '=')) || (*dataname++ = 0) ||
        ((datacode = match_string((*scanned)[iarg].list[2], choice, 2, 0)) < 0))
      continue;
    filename = (*scanned)[iarg].list[1];
    if (!SDDS_InitializeInput(&dataset, filename))
      continue;
    if ((datacode == 0 && (index = SDDS_GetColumnIndex(&dataset, dataname)) < 0) ||
        (datacode == 1 && (index = SDDS_GetParameterIndex(&dataset, dataname)) < 0)) {
      fprintf(stderr, "error: %s %s not found in file %s--can't process -getargs request\n",
              choice[datacode], dataname, filename);
      /*            SDDS_Terminate(&dataset); */
      exit(1);
    }
    if ((datacode == 0 && SDDS_GetColumnType(&dataset, index) != SDDS_STRING) ||
        (datacode == 1 && SDDS_GetParameterType(&dataset, index) != SDDS_STRING)) {
      fprintf(stderr, "error: %s %s from file %s does not have string type--can't use for -getargs\n",
              choice[datacode], dataname, filename);
      /*            SDDS_Terminate(&dataset); */
      exit(1);
    }

    argc_got = 0;
    switch (datacode) {
    case 0:
      while (SDDS_ReadPage(&dataset) > 0) {
        char **column;
        int64_t rows;
        if ((rows = SDDS_CountRowsOfInterest(&dataset)) <= 0)
          continue;
        if (!(column = SDDS_GetColumn(&dataset, dataname)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        argv_got = trealloc(argv_got, sizeof(*argv_got) * (rows + argc_got));
        for (j = 0; j < rows; j++) {
          delete_chars(column[j], "\"");
          argv_got[argc_got + j] = column[j];
        }
        free(column);
        argc_got += j;
      }
      break;
    case 1:
      while (SDDS_ReadPage(&dataset) > 0) {
        char *parameter;
        if (!SDDS_GetParameter(&dataset, dataname, &parameter))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        argv_got = trealloc(argv_got, sizeof(*argv_got) * (argc_got + 1));
        argv_got[argc_got] = parameter;
        argc_got += 1;
      }
      break;
    default:
      break;
    }
    SDDS_Terminate(&dataset);
    argc_got = scanargsg(&sa_got, argc_got, argv_got);
    *scanned = trealloc(*scanned, sizeof(**scanned) * (argc + argc_got));
    if (argc_got > 1)
      for (j = argc - 1; j > iarg; j--)
        (*scanned)[j + argc_got - 1] = (*scanned)[j];
    for (j = 0; j < argc_got; j++)
      (*scanned)[j + iarg] = sa_got[j];
    argc += argc_got - 1;
    iarg += argc_got - 1;
  }
  return argc;
}

long ParameterScansAsNumber(SDDS_DATASET *dataset, char *name, short invert) {
  char **ptr;
  double value;
  if (SDDS_CheckParameter(dataset, name, NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY) {
    fprintf(stderr, "Error: parameter %s nonexistent or not string type---can't use with -numberTest\n",
            name);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (!(ptr = SDDS_GetParameter(dataset, name, NULL))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (sscanf(*ptr, "%lf", &value) != 1) {
    free(*ptr);
    return invert;
  }
  free(*ptr);
  return !invert;
}

long cast_column_value(SDDS_DATASET *SDDS_dataset, CAST_DEFINITION *cast) {
  char **source_value, **target_value;
  long target_index, source_index, size;
  int64_t i, rows;
  long source_type, target_type;
  char s[1024];

  if ((source_index = SDDS_GetColumnIndex(SDDS_dataset, cast->source)) < 0 ||
      (target_index = SDDS_GetColumnIndex(SDDS_dataset, cast->newName)) < 0) {
    sprintf(s, "Unable to cast %s to %s--column not found", cast->source,
            cast->newName);
    SDDS_SetError(s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  source_type = SDDS_GetColumnType(SDDS_dataset, source_index);
  if (!SDDS_NUMERIC_TYPE(source_type)) {
    sprintf(s, "source column %s has wrong type for casting--must be number\n",
            cast->source);
    SDDS_SetError(s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  target_type = SDDS_GetColumnType(SDDS_dataset, target_index);
  if (!SDDS_NUMERIC_TYPE(target_type)) {
    sprintf(s, "target column %s has wrong type for casting--must be number\n",
            cast->newName);
    SDDS_SetError(s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  if ((rows = SDDS_CountRowsOfInterest(SDDS_dataset)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  if (!(source_value = SDDS_GetInternalColumn(SDDS_dataset, cast->source))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (!(target_value = SDDS_GetInternalColumn(SDDS_dataset, cast->newName))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  size = SDDS_GetTypeSize(target_type);
  for (i = 0; i < rows; i++) {
    if (!SDDS_CastValue(source_value, i, source_type, target_type,
                        (void *)((char *)target_value + size * i))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
  }
  return (1);
}

long cast_parameter_value(SDDS_DATASET *SDDS_dataset, CAST_DEFINITION *cast) {
  double doubleValue;
  int32_t targetIndex, sourceIndex;
  static char s[SDDS_MAXLINE];

  if ((sourceIndex = SDDS_GetParameterIndex(SDDS_dataset, cast->source)) < 0 ||
      (targetIndex = SDDS_GetParameterIndex(SDDS_dataset, cast->newName)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (!SDDS_NUMERIC_TYPE(SDDS_GetParameterType(SDDS_dataset, sourceIndex))) {
    sprintf(s, "error: source parameter %s has wrong type for casting--must be number\n",
            cast->source);
    SDDS_SetError(s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (!SDDS_NUMERIC_TYPE(SDDS_GetParameterType(SDDS_dataset, targetIndex))) {
    fprintf(stderr, "error: target parameter %s has wrong type for casting--must be number\n",
            cast->newName);
    return (0);
  }

  if (!SDDS_GetParameterAsDouble(SDDS_dataset, cast->source, &doubleValue)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  if (!SDDS_SetParametersFromDoubles(SDDS_dataset,
                                     SDDS_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     targetIndex, doubleValue, -1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  return (1);
}

char *addOuterParentheses(char *arg) {
  char *ptr;
  ptr = tmalloc(sizeof(*ptr) * (strlen(arg) + 2));
  sprintf(ptr, "(%s)", arg);
  return ptr;
}

void GillMillerIntegration1(double *indepData, double *data, long n_data, double *result) {
  double *integral;
  integral = tmalloc(sizeof(*integral) * n_data);
  GillMillerIntegration(integral, NULL, data, indepData, n_data);
  *result = integral[n_data - 1];
  free(integral);
}

/* ========================================================================== */
/* Hash table support for -hashLookup (moved from sddsxref.c into SDDSaps)    */
/* Shared implementations for string and numeric key lookup.                  */
/* ========================================================================== */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Internal helpers */
static unsigned long SDDS_hash_string(const char *s) {
  unsigned long h = 5381;
  int c;
  while (s && (c = *s++))
    h = ((h << 5) + h) + (unsigned long)c; /* djb2 */
  return h;
}

static uint64_t SDDS_hash_double_key(uint64_t key) {
  /* Mix 64-bit key using FNV-1a style for decent distribution */
  uint64_t h = 1469598103934665603ULL;
  for (int i = 0; i < 8; i++) {
    uint8_t b = (key >> (i * 8)) & 0xFF;
    h ^= b;
    h *= 1099511628211ULL;
  }
  return h;
}

/* Build a hash table mapping string keys to row indices (supports duplicates) */
StrHash *SDDS_BuildStrHash(char **keys, int64_t nrows) {
  const int buckets = 8192;
  StrHash *ht = (StrHash *)malloc(sizeof(StrHash));
  if (!ht) return NULL;
  ht->bucketCount = buckets;
  ht->buckets = (StrNode **)calloc(buckets, sizeof(StrNode *));
  if (!ht->buckets) {
    free(ht);
    return NULL;
  }
  for (int64_t r = 0; r < nrows; r++) {
    char *key = keys[r];
    if (!key)
      continue;
    unsigned long h = SDDS_hash_string(key) % (unsigned long)buckets;
    StrNode *node = ht->buckets[h];
    while (node && strcmp(node->key, key) != 0)
      node = node->next;
    if (!node) {
      node = (StrNode *)malloc(sizeof(StrNode));
      if (!node) continue;
      node->key = key; /* borrow pointer (lifetime managed by caller) */
      node->list.rows = (int64_t *)malloc(sizeof(int64_t));
      if (!node->list.rows) { free(node); continue; }
      node->list.rows[0] = r;
      node->list.count = 1;
      node->list.next = 0;
      node->next = ht->buckets[h];
      ht->buckets[h] = node;
    } else {
      node->list.rows = (int64_t *)realloc(node->list.rows, sizeof(int64_t) * (node->list.count + 1));
      if (!node->list.rows) {
        node->list.count = 0;
        continue;
      }
      node->list.rows[node->list.count++] = r;
    }
  }
  return ht;
}

int64_t SDDS_LookupStr(StrHash *ht, const char *key, int reuse) {
  if (!ht || !key)
    return -1;
  unsigned long h = SDDS_hash_string(key) % (unsigned long)ht->bucketCount;
  StrNode *node = ht->buckets[h];
  while (node && strcmp(node->key, key) != 0)
    node = node->next;
  if (!node || node->list.count == 0)
    return -1;
  if (reuse) {
    /* allow reuse: return first row */
    return node->list.rows[0];
  } else {
    /* no reuse: consume next available */
    if (node->list.next >= node->list.count)
      return -1;
    return node->list.rows[node->list.next++];
  }
}

void SDDS_FreeStrHash(StrHash *ht) {
  if (!ht) return;
  for (int i = 0; i < ht->bucketCount; i++) {
    StrNode *cur = ht->buckets[i];
    while (cur) {
      StrNode *n = cur->next;
      if (cur->list.rows)
        free(cur->list.rows);
      free(cur);
      cur = n;
    }
  }
  free(ht->buckets);
  free(ht);
}

/* Build a hash table mapping double keys (bit pattern) to row indices */
NumHash *SDDS_BuildNumHash(double *keys, int64_t nrows) {
  const int buckets = 8192;
  NumHash *ht = (NumHash *)malloc(sizeof(NumHash));
  if (!ht) return NULL;
  ht->bucketCount = buckets;
  ht->buckets = (NumNode **)calloc(buckets, sizeof(NumNode *));
  if (!ht->buckets) {
    free(ht);
    return NULL;
  }
  for (int64_t r = 0; r < nrows; r++) {
    double dv = keys[r];
    uint64_t k;
    memcpy(&k, &dv, sizeof(uint64_t));
    uint64_t hv = SDDS_hash_double_key(k);
    int idx = (int)(hv % (uint64_t)buckets);
    NumNode *node = ht->buckets[idx];
    while (node && node->key != k)
      node = node->next;
    if (!node) {
      node = (NumNode *)malloc(sizeof(NumNode));
      if (!node) continue;
      node->key = k;
      node->list.rows = (int64_t *)malloc(sizeof(int64_t));
      if (!node->list.rows) { free(node); continue; }
      node->list.rows[0] = r;
      node->list.count = 1;
      node->list.next = 0;
      node->next = ht->buckets[idx];
      ht->buckets[idx] = node;
    } else {
      node->list.rows = (int64_t *)realloc(node->list.rows, sizeof(int64_t) * (node->list.count + 1));
      if (!node->list.rows) {
        node->list.count = 0;
        continue;
      }
      node->list.rows[node->list.count++] = r;
    }
  }
  return ht;
}

int64_t SDDS_LookupNum(NumHash *ht, double keyVal, int reuse) {
  if (!ht)
    return -1;
  uint64_t k;
  memcpy(&k, &keyVal, sizeof(uint64_t));
  uint64_t hv = SDDS_hash_double_key(k);
  int idx = (int)(hv % (uint64_t)ht->bucketCount);
  NumNode *node = ht->buckets[idx];
  while (node && node->key != k)
    node = node->next;
  if (!node || node->list.count == 0)
    return -1;
  if (reuse) {
    return node->list.rows[0];
  } else {
    if (node->list.next >= node->list.count)
      return -1;
    return node->list.rows[node->list.next++];
  }
}

void SDDS_FreeNumHash(NumHash *ht) {
  if (!ht) return;
  for (int i = 0; i < ht->bucketCount; i++) {
    NumNode *cur = ht->buckets[i];
    while (cur) {
      NumNode *n = cur->next;
      if (cur->list.rows)
        free(cur->list.rows);
      free(cur);
      cur = n;
    }
  }
  free(ht->buckets);
  free(ht);
}
