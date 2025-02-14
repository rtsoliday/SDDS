/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution.
\*************************************************************************/

#include "mdb.h"
#include "SDDS.h"
#include "SDDSaps.h"
#include <ctype.h>
#if !defined(_WIN32)
#  include <sys/time.h>
#endif

#undef DEBUG

char *data_class_keyword[DATA_CLASS_KEYWORDS] = {
  "column",
  "parameter",
  "array",
};

static long udfCounter = 0;

typedef struct {
  char *name, *description;
  unsigned long flags;
} PROCESS_COLUMN_DATA;
#define PROCMODE_NORMAL 0x0000
#define PROCMODE_FUNCOF_REQUIRED 0x0001
#define PROCMODE_FUNCOF_UNITS 0x0002
#define PROCMODE_WEIGHT_OK 0x0004
#define PROCMODE_POSITION_OK 0x0008
#define PROCMODE_STRING_OK 0x0010

static char *process_column_name[N_PROCESS_COLUMN_MODES] = {NULL};
static PROCESS_COLUMN_DATA process_column_data[N_PROCESS_COLUMN_MODES] = {
  {"average", "average of ", PROCMODE_NORMAL | PROCMODE_WEIGHT_OK},
  {"rms", "rms of ", PROCMODE_NORMAL | PROCMODE_WEIGHT_OK},
  {"sum", "sum of ", PROCMODE_NORMAL | PROCMODE_WEIGHT_OK},
  {"standarddeviation", "standard deviation of ", PROCMODE_NORMAL | PROCMODE_WEIGHT_OK},
  {"mad", "mean absolute deviation of ", PROCMODE_NORMAL | PROCMODE_WEIGHT_OK},
  {"minimum", "minimum of ", PROCMODE_NORMAL | PROCMODE_POSITION_OK},
  {"maximum", "maximum of ", PROCMODE_NORMAL | PROCMODE_POSITION_OK},
  {"smallest", "smallest of ", PROCMODE_NORMAL | PROCMODE_POSITION_OK},
  {"largest", "largest of ", PROCMODE_NORMAL | PROCMODE_POSITION_OK},
  {"first", "first ", PROCMODE_NORMAL | PROCMODE_STRING_OK},
  {"last", "last ", PROCMODE_NORMAL | PROCMODE_STRING_OK},
  {"count", "count ", PROCMODE_NORMAL},
  {"spread", "spread in ", PROCMODE_NORMAL},
  {"median", "median of ", PROCMODE_NORMAL},
  {"baselevel", "base level of ", PROCMODE_NORMAL},
  {"toplevel", "top level of ", PROCMODE_NORMAL},
  {"amplitude", "amplitude of ", PROCMODE_NORMAL},
  {"risetime", "risetime of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"falltime", "falltime of", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"fwhm", "fwhm of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"fwtm", "fwtm of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"center", "center of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"zerocrossing", "zero crossing of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"fwha", "fwha of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
  {"fwta", "fwta of ", PROCMODE_FUNCOF_REQUIRED | PROCMODE_FUNCOF_UNITS},
};

void show_process_modes(FILE *fp) {
  long i, length, maxLength, nPerLine;
  char format[32]; // increased size to accommodate larger numbers

  maxLength = 0;
  for (i = 0; i < N_PROCESS_COLUMN_MODES; i++)
    if ((length = strlen(process_column_data[i].name)) > maxLength)
      maxLength = length;
  // Build a format string like "%<width>s%c"
  snprintf(format, sizeof(format), "%%%lds%%c", maxLength + 1);
  nPerLine = 80 / (maxLength + 2);
  fprintf(fp, "processing modes:\n");
  for (i = 0; i < N_PROCESS_COLUMN_MODES; i++)
    fprintf(fp, format, process_column_data[i].name,
            (i + 1) % nPerLine ? ' ' : '\n');
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
    bomb("invalid -ifnot syntax---column, parameter, or array keyword required", NULL);
  for (i = 1; i < names; i++)
    ifitem->type[ifitem->items + i - 1] = type;
  if (!SDDS_CopyStringArray(ifitem->name + ifitem->items, name + 1, names - 1))
    bomb("problem copying names for -ifnot or -ifis option", NULL);
  ifitem->items += names - 1;
}

long check_ifitems(SDDS_DATASET *SDDS_dataset, IFITEM_LIST *ifitem, long desired) {
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
      bomb("internal error---unknown ifitem type", NULL);
      exit(1);
      break;
    }
    if ((index >= 0) != desired) {
      if (desired) {
        fprintf(stderr, "%s %s does not exist--aborting\n",
                data_class_keyword[ifitem->type[i]], ifitem->name[i]);
        return 0;
      }
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
    bomb("memory allocation failure (process_new_filter_definition)", NULL);
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
    bomb("memory allocation failure (process_new_match_definition)", NULL);
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
    if (sscanf(argument[i++], "%lf", &(*filter)[filters].lower) != 1 ||
        sscanf(argument[i++], "%lf", &(*filter)[filters].upper) != 1 ||
        (*filter)[filters].lower > (*filter)[filters].upper)
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
    if (*(ptr + 1) == '@') {
      (*match)[matches].logic = SDDS_INDIRECT_MATCH;
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
      fprintf(stderr, "<%s>\n", match[i].string);
    else
      fprintf(stderr, "\"%s\"\n", match[i].string);
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
  long i;
  EQUATION_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    bomb("memory allocation failure (process_new_equation_definition)", NULL);
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
  defi->name = argument[1];
  equation = argument[2];
  if (arguments > 3) {
    defi->argv = tmalloc(sizeof(*defi->argv) * (arguments - 3));
    if (!SDDS_CopyStringArray(defi->argv, argument + 3, arguments - 3))
      bomb("string array copy failed (process_new_equation_definition)", NULL);
    defi->argc = arguments - 3;
  } else {
    defi->argc = 0;
    defi->argv = NULL;
  }
  for (i = 3; i < arguments; i++) {
    if (!(ptr = strchr(argument[i], '='))) {
      fprintf(stderr, "error: invalid definition-entry: %s\n", argument[i]);
      return (NULL);
    }
    *ptr = 0;
    strcat(s, argument[i]);
    strcat(s, "=\"");
    strcat(s, ptr + 1);
    strcat(s, "\", ");
  }
  if (!strstr(s, ", type="))
    strcat(s, " type=\"double\", ");
  strcat(s, "&end");
  if (!SDDS_CopyString(&defi->text, s) || !SDDS_CopyString(&defi->equation, equation))
    bomb("string copying failed (process_new_equation_definition)", NULL);
  sprintf(s, "%sUDF%ld", defi->name, udfCounter++);
  delete_chars(s, " ");
  if (!SDDS_CopyString(&defi->udf_name, s))
    bomb("string copying failed (process_new_equation_definition)", NULL);
  return (defi);
}

SCAN_DEFINITION *process_new_scan_definition(char **argument, long arguments) {
  static char s[SDDS_MAXLINE];
  char *ptr;
  long i;
  SCAN_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    bomb("memory allocation failure (process_new_scan_definition)", NULL);
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
  for (i = 4; i < arguments; i++) {
    if (!(ptr = strchr(argument[i], '=')))
      return (NULL);
    *ptr = 0;
    strcat(s, argument[i]);
    strcat(s, "=\"");
    strcat(s, ptr + 1);
    strcat(s, "\", ");
  }
  if (!strstr(s, ", type="))
    strcat(s, " type=\"double\", ");
  strcat(s, "&end");
  if (!SDDS_CopyString(&defi->text, s))
    bomb("unable to copy text of scan definition (process_new_scan_definition)", NULL);
  return (defi);
}

EDIT_DEFINITION *process_new_edit_definition(char **argument, long arguments) {
  static char s[SDDS_MAXLINE];
  char *ptr;
  long i;
  EDIT_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    bomb("memory allocation failure (process_new_equation_definition)", NULL);
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
  defi->source = argument[2];
  defi->edit_command = argument[3];
  for (i = 4; i < arguments; i++) {
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
    bomb("unable to copy text of edit definition (process_new_edit_definition)", NULL);
  return (defi);
}

long edit_column_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *edit_command) {
  char **source_value;
  long i, rows, target_index, source_index;
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
  if (SDDS_GetColumnType(SDDS_dataset, target_index) != SDDS_STRING) {
    fprintf(stderr, "error: target column %s has wrong type for editing--must be string\n", target);
    return (0);
  }

  if ((rows = SDDS_CountRowsOfInterest(SDDS_dataset)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  if (!(source_value = SDDS_GetColumn(SDDS_dataset, source))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  for (i = 0; i < rows; i++) {
    if (!source_value[i])
      s[0] = 0;
    else
      strcpy_ss(s, source_value[i]);
    if (!edit_string(s, edit_command)) {
      fprintf(stderr, "error: unable to edit source string \"%s\" to make %s\n", source_value[i], target);
      return (0);
    }
    if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, s, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (source_value[i])
      free(source_value[i]);
  }
  free(source_value);
  return (1);
}

long edit_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *edit_command) {
  char *source_value, **ptr;
  long target_index, source_index;
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
  if (SDDS_GetParameterType(SDDS_dataset, target_index) != SDDS_STRING) {
    fprintf(stderr, "error: target parameter %s has wrong type for editing--must be string\n", target);
    return (0);
  }

  if (!(ptr = SDDS_GetParameter(SDDS_dataset, source, NULL))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (!(source_value = *ptr))
    s[0] = 0;
  else
    strcpy_ss(s, source_value);
  if (!edit_string(s, edit_command)) {
    fprintf(stderr, "error: unable to edit source string \"%s\" to make %s\n", source_value, target);
    return (0);
  }
  if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, s, -1)) {
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
                             (label_parameter[j].format ? label_parameter[j].format : pardefptr->format_string),
                             output->fp, 0);
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
                             (label_parameter[j].format ? label_parameter[j].format : pardefptr->format_string),
                             output->fp, 0);
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

long process_column(SDDS_DATASET *Dataset, PROCESSING_DEFINITION *processing_ptr, double *result) {
  double *data, *indepData, *weightData;
  long i, i2, n_data, mode;
  int64_t i1, imin, imax;
  double min, max, top, base, point1, point2;
  long returnValue, count;
  short *keep;

  i1 = 0;
  mode = processing_ptr->mode;
#if DEBUG
  fprintf(stderr, "process_column: column=%s, mode=%s\n", processing_ptr->column_name,
          process_column_data[mode].name);
#endif

  if ((n_data = SDDS_CountRowsOfInterest(Dataset)) <= 0 && mode != PROCESS_COLUMN_COUNT)
    return (0);

  if (!(data = SDDS_GetColumnInDoubles(Dataset, processing_ptr->column_name)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  indepData = weightData = NULL;
  if (processing_ptr->weightBy && !(weightData = SDDS_GetColumnInDoubles(Dataset, processing_ptr->weightBy)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (processing_ptr->functionOf && !(indepData = SDDS_GetColumnInDoubles(Dataset, processing_ptr->functionOf)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  keep = NULL;
  if (processing_ptr->flags & (PROCESSING_LOLIM_GIVEN | PROCESSING_UPLIM_GIVEN | PROCESSING_TAIL_GIVEN | PROCESSING_HEAD_GIVEN |
                               PROCESSING_FTAIL_GIVEN | PROCESSING_FHEAD_GIVEN)) {
    keep = tmalloc(sizeof(*keep) * n_data);
    for (i = 0; i < n_data; i++)
      keep[i] = 1;
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
    for (i = 0; i < n_data; i++)
      if (processing_ptr->upperLimit < indepData[i])
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
    long j;
    for (i = j = 0; i < n_data; i++) {
      if (keep[i]) {
        if (i != j) {
          data[j] = data[i];
          if (indepData)
            indepData[j] = indepData[i];
          if (weightData)
            weightData[j] = weightData[i];
        }
        j++;
      }
    }
    n_data = j;
#if DEBUG
    fprintf(stderr, "%ld points kept\n", n_data);
#endif
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
      if (data[i] == 0)
        *result = indepData[i];
      else
        *result = indepData[i] + (indepData[i + 1] - indepData[i]) / (data[i + 1] - data[i]) * (-data[i]);
    } else
      returnValue = 0;
    break;
  case PROCESS_COLUMN_MEAN:
    if (weightData)
      *result = weightedAverage(data, weightData, n_data);
    else
      *result = arithmeticAverage(data, n_data);
    break;
  case PROCESS_COLUMN_RMS:
    if (weightData)
      *result = weightedRMS(data, weightData, n_data);
    else
      *result = rmsValue(data, n_data);
    break;
  case PROCESS_COLUMN_SUM:
    if (weightData) {
      double sum;
      for (i = sum = 0; i < n_data; i++)
        sum += data[i] * weightData[i];
      *result = sum;
    } else
      *result = arithmeticAverage(data, n_data) * n_data;
    break;
  case PROCESS_COLUMN_STAND_DEV:
    if (weightData)
      *result = weightedStDev(data, weightData, n_data);
    else
      *result = standardDeviation(data, n_data);
    break;
  case PROCESS_COLUMN_MAD:
    if (weightData)
      *result = weightedMAD(data, weightData, n_data);
    else
      *result = meanAbsoluteDeviation(data, n_data);
    break;
  case PROCESS_COLUMN_MINIMUM:
  case PROCESS_COLUMN_MAXIMUM:
  case PROCESS_COLUMN_SMALLEST:
  case PROCESS_COLUMN_LARGEST:
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
      if (processing_ptr->flags & PROCESSING_POSITION_GIVEN)
        *result = indepData[i1];
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
    if (compute_median(result, data, n_data) < 0)
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
    if (!findTopBaseLevels(&top, &base, data, n_data, 50, 2.0))
      returnValue = 0;
    else {
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
            (i2 = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, base + (top - base) * 0.5, -1, indepData, &point2)) < 0)
          returnValue = 0;
        else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_FWTA:
        if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.1, 1, indepData, &point1)) < 0 ||
            (i2 = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, base + (top - base) * 0.1, -1, indepData, &point2)) < 0)
          returnValue = 0;
        else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_FWHM:
        if ((i1 = findCrossingPoint(0, data, n_data, top * 0.5, 1, indepData, &point1)) < 0 ||
            (i2 = findCrossingPoint(i1, data, n_data, top * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, top * 0.5, -1, indepData, &point2)) < 0)
          returnValue = 0;
        else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_FWTM:
        if ((i1 = findCrossingPoint(0, data, n_data, top * 0.1, 1, indepData, &point1)) < 0 ||
            (i2 = findCrossingPoint(i1, data, n_data, top * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, top * 0.1, -1, indepData, &point2)) < 0)
          returnValue = 0;
        else
          *result = point2 - point1;
        break;
      case PROCESS_COLUMN_CENTER:
        if ((i1 = findCrossingPoint(0, data, n_data, base + (top - base) * 0.5, 1, indepData, &point1)) < 0 ||
            (i2 = findCrossingPoint(i1, data, n_data, base + (top - base) * 0.9, -1, NULL, NULL)) < 0 ||
            (i2 = findCrossingPoint(i2, data, n_data, base + (top - base) * 0.5, -1, indepData, &point2)) < 0)
          returnValue = 0;
        else
          *result = (point1 + point2) / 2;
        break;
      }
    }
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
  if (!returnValue) {
    fprintf(stderr, "warning: processing of %s with mode %s failed--value DBL_MAX returned\n",
            processing_ptr->column_name,
            process_column_name[processing_ptr->mode]);
    *result = DBL_MAX;
  }
  return 1;
}

char *process_string_column(SDDS_DATASET *Dataset, PROCESSING_DEFINITION *processing_ptr) {
  char **data, *result;
  long i, n_data, mode;
  long count;
  short *keep;

  mode = processing_ptr->mode;

  if ((n_data = SDDS_CountRowsOfInterest(Dataset)) <= 0 && mode != PROCESS_COLUMN_COUNT)
    return NULL;

  if (!(data = SDDS_GetColumn(Dataset, processing_ptr->column_name)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  keep = NULL;
  if (processing_ptr->flags & (PROCESSING_TAIL_GIVEN | PROCESSING_HEAD_GIVEN |
                               PROCESSING_FTAIL_GIVEN | PROCESSING_FHEAD_GIVEN)) {
    keep = tmalloc(sizeof(*keep) * n_data);
    for (i = 0; i < n_data; i++)
      keep[i] = 1;
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
    long j;
    for (i = j = 0; i < n_data; i++) {
      if (keep[i]) {
        if (i != j)
          data[j] = data[i];
        j++;
      }
    }
    n_data = j;
  }

  switch (mode) {
  case PROCESS_COLUMN_FIRST:
    result = data[0];
    break;
  case PROCESS_COLUMN_LAST:
    result = data[n_data - 1];
    break;
  default:
    bomb("invalid processing mode for string data (sddsprocess)", NULL);
    exit(1);
    break;
  }

  free(data);
  if (keep)
    free(keep);
  return result;
}

static char *PROCESSING_USAGE = "-process=<column-name>,<analysis-name>,<result-name>[,description=<string>][,symbol=<string>][,weightBy=<column-name>][,functionOf=<column-name>[,lowerLimit=<value>][,upperLimit=<value>]][,head=<number>][,tail=<number>][fhead=<fraction>][ftail=<fraction>][position][,offset=<value>][,factor=<value>]";

PROCESSING_DEFINITION *record_processing_definition(char **argument, long arguments) {
  PROCESSING_DEFINITION *pd;

  pd = tmalloc(sizeof(*pd));
  if (arguments < 3)
    bomb("invalid -process syntax--wrong number of arguments", PROCESSING_USAGE);
  pd->column_name = argument[0];
  if (process_column_name[0] == NULL) {
    long i;
    for (i = 0; i < N_PROCESS_COLUMN_MODES; i++)
      process_column_name[i] = process_column_data[i].name;
  }

  if ((pd->mode = match_string(argument[1], process_column_name, N_PROCESS_COLUMN_MODES, 0)) < 0) {
    fprintf(stderr, "invalid -process mode: %s\n", argument[1]);
    show_process_modes(stderr);
    exit(1);
  }

  pd->parameter_name = argument[2];
  argument += 3;
  arguments -= 3;
  pd->flags = 0;
  if (!scanItemList(&pd->flags, argument, &arguments, 0,
                    "functionof", SDDS_STRING, &pd->functionOf, 1, PROCESSING_FUNCOF_GIVEN,
                    "weightby", SDDS_STRING, &pd->weightBy, 1, PROCESSING_WEIGHT_GIVEN,
                    "description", SDDS_STRING, &pd->description, 1, PROCESSING_DESCRIP_GIVEN,
                    "symbol", SDDS_STRING, &pd->symbol, 1, PROCESSING_SYMBOL_GIVEN,
                    "lowerlimit", SDDS_DOUBLE, &pd->lowerLimit, 1, PROCESSING_LOLIM_GIVEN,
                    "upperlimit", SDDS_DOUBLE, &pd->upperLimit, 1, PROCESSING_UPLIM_GIVEN,
                    "head", SDDS_LONG, &pd->head, 1, PROCESSING_HEAD_GIVEN,
                    "tail", SDDS_LONG, &pd->tail, 1, PROCESSING_TAIL_GIVEN,
                    "fhead", SDDS_DOUBLE, &pd->fhead, 1, PROCESSING_FHEAD_GIVEN,
                    "ftail", SDDS_DOUBLE, &pd->ftail, 1, PROCESSING_FTAIL_GIVEN,
                    "position", -1, NULL, 0, PROCESSING_POSITION_GIVEN,
                    "offset", SDDS_DOUBLE, &pd->offset, 1, PROCESSING_OFFSET_GIVEN,
                    "factor", SDDS_DOUBLE, &pd->factor, 1, PROCESSING_FACTOR_GIVEN,
                    NULL))
    bomb("invalid -process syntax", PROCESSING_USAGE);
  if (pd->flags & PROCESSING_FACTOR_GIVEN && pd->factor == 0)
    bomb("invalid -process syntax---factor field is zero", NULL);
  if ((process_column_data[pd->mode].flags & PROCMODE_FUNCOF_REQUIRED) &&
      !(pd->flags & PROCESSING_FUNCOF_GIVEN))
    bomb("invalid -process syntax---functionOf option required for given mode", NULL);
  if (pd->flags & PROCESSING_WEIGHT_GIVEN &&
      !(process_column_data[pd->mode].flags & PROCMODE_WEIGHT_OK))
    bomb("invalid -process specification---weightBy option not available for given mode", NULL);
  if (pd->flags & PROCESSING_POSITION_GIVEN) {
    if (!(pd->flags & PROCESSING_FUNCOF_GIVEN))
      bomb("invalid -process syntax---functionOf required with position option", NULL);
    if (!(process_column_data[pd->mode].flags & PROCMODE_POSITION_OK))
      bomb("invalid -process syntax---position option not permitted for given mode", NULL);
  }
  if (pd->flags & PROCESSING_LOLIM_GIVEN && pd->flags & PROCESSING_UPLIM_GIVEN &&
      pd->lowerLimit >= pd->upperLimit)
    bomb("invalid -process syntax---lowerLimit>=upperLimit", NULL);
  if (pd->flags & PROCESSING_HEAD_GIVEN && pd->head == 0)
    bomb("invalid -process syntax---head=0", NULL);
  if (pd->flags & PROCESSING_FHEAD_GIVEN && pd->fhead == 0)
    bomb("invalid -process syntax---fhead=0", NULL);
  if (pd->flags & PROCESSING_TAIL_GIVEN && pd->tail == 0)
    bomb("invalid -process syntax---tail=0", NULL);
  if (pd->flags & PROCESSING_FTAIL_GIVEN && pd->ftail == 0)
    bomb("invalid -process syntax---ftail=0", NULL);
  if (pd->flags & (PROCESSING_LOLIM_GIVEN | PROCESSING_UPLIM_GIVEN) && !(pd->flags & PROCESSING_FUNCOF_GIVEN))
    bomb("invalid -process syntax---must give -functionOf with limit options", NULL);
  return (pd);
}

void expandProcessingDefinitions(DEFINITION **definition, long *definitions, SDDS_DATASET *SDDS_dataset) {
  PROCESSING_DEFINITION *procPtr;
  long names, idef, jdef;
  char **name;

  for (idef = 0; idef < *definitions; idef++) {
    if ((*definition)[idef].type != IS_PROCESSING_DEFINITION)
      continue;
    procPtr = (PROCESSING_DEFINITION *)(*definition)[idef].structure;
    if (!has_wildcards(procPtr->column_name))
      continue;
    if ((names = SDDS_MatchColumns(SDDS_dataset, &name, SDDS_MATCH_STRING, FIND_ANY_TYPE,
                                   procPtr->column_name, SDDS_AND | SDDS_1_PREVIOUS)) <= 0) {
      fprintf(stderr, "Error: unable to find match to column name %s\n", procPtr->column_name);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    free(procPtr->column_name);
    procPtr->column_name = name[0];
    if (names > 1) {
      *definition = SDDS_Realloc(*definition, sizeof(**definition) * (*definitions - 1 + names));
      for (jdef = *definitions - 1; jdef > idef; jdef--) {
        (*definition)[jdef + names - 1].structure = (*definition)[jdef].structure;
        (*definition)[jdef + names - 1].type = (*definition)[jdef].type;
      }
      for (jdef = names - 1; jdef > 0; jdef--) {
        procPtr = (*definition)[idef + jdef].structure =
          copyProcessingDefinition((*definition)[idef].structure);
        ((PROCESSING_DEFINITION *)(*definition)[idef + jdef].structure)->column_name = name[jdef];
        (*definition)[idef + jdef].type = IS_PROCESSING_DEFINITION;
      }
      *definitions += names - 1;
      idef += names - 1;
    }
    free(name);
  }
  /*
    fprintf(stderr, "   Final list:\n");
    for (jdef=0; jdef<*definitions; jdef++) {
    if ((*definition)[jdef].type!=IS_PROCESSING_DEFINITION)
    continue;
    fprintf(stderr, "%ld: %s -> %s\n", jdef,
    ((PROCESSING_DEFINITION*)(*definition)[jdef].structure)->column_name,
    ((PROCESSING_DEFINITION*)(*definition)[jdef].structure)->parameter_name);
    }
  */
}

PROCESSING_DEFINITION *copyProcessingDefinition(PROCESSING_DEFINITION *source) {
  PROCESSING_DEFINITION *target;
  if (!(target = (PROCESSING_DEFINITION *)calloc(1, sizeof(*target)))) {
    fputs("error: memory allocation failure (copyProcessingDefinition)\n", stderr);
    exit(1);
  }
  memcpy((char *)target, (char *)source, sizeof(*source));
  /* allocate private string memory: */
  SDDS_CopyString(&target->parameter_name, source->parameter_name);
  SDDS_CopyString(&target->description, source->description);
  SDDS_CopyString(&target->symbol, source->symbol);
  SDDS_CopyString(&target->functionOf, source->functionOf);
  SDDS_CopyString(&target->weightBy, source->weightBy);
  return target;
}

long complete_processing_definitions(PROCESSING_DEFINITION **processing_definition, long processing_definitions,
                                     SDDS_DATASET *SDDS_dataset) {
  static char s[SDDS_MAXLINE], t[SDDS_MAXLINE];
  long i, index, index1;
  PARAMETER_DEFINITION *pardef;

  index1 = 0;
  for (i = 0; i < processing_definitions; i++) {
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, processing_definition[i]->column_name)) < 0) {
      sprintf(s, "column %s does not exist", processing_definition[i]->column_name);
      SDDS_SetError(s);
      return (0);
    }
    switch (processing_definition[i]->type = SDDS_GetColumnType(SDDS_dataset, index)) {
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
    case SDDS_CHARACTER:
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
        (index1 = SDDS_GetColumnIndex(SDDS_dataset, processing_definition[i]->weightBy)) < 0) {
      sprintf(s, "column %s does not exist", processing_definition[i]->weightBy);
      SDDS_SetError(s);
      return 0;
    } else if (processing_definition[i]->flags & PROCESSING_FUNCOF_GIVEN &&
               (index1 = SDDS_GetColumnIndex(SDDS_dataset, processing_definition[i]->functionOf)) < 0) {
      sprintf(s, "column %s does not exist", processing_definition[i]->functionOf);
      SDDS_SetError(s);
      return 0;
    }
    if (processing_definition[i]->type != SDDS_STRING)
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
    } else {
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
    if (!SDDS_ProcessParameterString(SDDS_dataset, s, SDDS_WRITEONLY_DEFINITION) ||
        !(pardef = SDDS_GetParameterDefinition(SDDS_dataset, processing_definition[i]->parameter_name)))
      return (0);
    processing_definition[i]->memory_number = pardef->memory_number;
    SDDS_FreeParameterDefinition(pardef);
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

long scan_column_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *format) {
  char **source_value, char_value;
  long target_type, i, rows, target_index, source_index;
  float float_value;
  double double_value;
  long long_value;
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

  if (!(source_value = SDDS_GetColumn(SDDS_dataset, source))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  for (i = 0; i < rows; i++) {
    if (!source_value[i]) {
      if (SDDS_NUMERIC_TYPE(target_type))
        SDDS_CopyString(source_value + i, "NaN");
      else
        SDDS_CopyString(source_value + i, "\0");
    }
    switch (target_type) {
    case SDDS_DOUBLE:
      if (sscanf(source_value[i], format, &double_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", source_value[i], target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, double_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_FLOAT:
      if (sscanf(source_value[i], format, &float_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", source_value[i], target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, float_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_LONG:
      if (sscanf(source_value[i], format, &long_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", source_value[i], target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, long_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_SHORT:
      if (sscanf(source_value[i], format, &short_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", source_value[i], target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, short_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_CHARACTER:
      if (sscanf(source_value[i], format, &char_value) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", source_value[i], target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, char_value, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    case SDDS_STRING:
      if (sscanf(source_value[i], format, s) != 1) {
        fprintf(stderr, "error: unable to scan source string \"%s\" to make %s\n", source_value[i], target);
        return (0);
      }
      if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i, target_index, s, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      break;
    default:
      bomb("error: unknown type encountered--this shouldn't happen (scan_column_value)", NULL);
      break;
    }
    free(source_value[i]);
  }
  free(source_value);
  return (1);
}

long scan_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *format) {
  char *source_value, **ptr, char_value;
  long target_type, target_index, source_index;
  float float_value;
  double double_value;
  long long_value;
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
    bomb("error: unknown type encountered--this shouldn't happen (scan_parameter_value)", NULL);
    break;
  }
  free(source_value);
  return (1);
}

PRINT_DEFINITION *process_new_print_definition(char **argument, long arguments) {
  static char s[SDDS_MAXLINE];
  char *ptr;
  long i;
  PRINT_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    bomb("memory allocation failure (process_new_print_definition)", NULL);
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
    } else {
      *ptr = 0;
      if (strcmp(argument[i], "type") == 0)
        continue;
      strcat(s, argument[i]);
      strcat(s, "=\"");
      strcat(s, ptr + 1);
      strcat(s, "\", ");
    }
  }
  strcat(s, " type=string &end");
  if (!SDDS_CopyString(&defi->text, s))
    bomb("unable to copy text of print definition (process_new_print_definition)", NULL);
  return (defi);
}

long print_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char **source, long sources, char *format) {
  char *print_pos, **ptr, *fptr1, *fptr2 = NULL, *fptr3, *cptr;
  long source_type, source_index, target_index;
  long i, use_info;
  static char s[SDDS_MAXLINE];
  static char *info = NULL;

  if (!info && !(info = malloc(sizeof(double) * 2)))
    SDDS_Bomb("Allocation failure in print_parameter_value");

  if ((target_index = SDDS_GetParameterIndex(SDDS_dataset, target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (SDDS_GetParameterType(SDDS_dataset, target_index) != SDDS_STRING) {
    fprintf(stderr, "error: target parameter %s has wrong type for printing--must be string\n", target);
    return (0);
  }
  fptr1 = format;
  s[0] = 0;
  if (sources == 0)
    strcpy_ss(s, format);
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
    case SDDS_LONG:
      sprintf(print_pos, fptr1, *(long *)ptr);
      break;
    case SDDS_SHORT:
      sprintf(print_pos, fptr1, *(short *)ptr);
      break;
    case SDDS_CHARACTER:
      sprintf(print_pos, fptr1, *(char *)ptr);
      break;
    case SDDS_STRING:
      sprintf(print_pos, fptr1, *(char **)ptr);
      free(*ptr);
      break;
    default:
      bomb("error: unknown type encountered--this shouldn't happen (print_parameter_value)", NULL);
      break;
    }
    if (!(fptr1 = fptr2))
      break;
    *fptr1 = '%';
    if (!use_info)
      free(ptr);
  }
  if (!SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, target_index, s, -1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  return (1);
}

long print_column_value(SDDS_DATASET *SDDS_dataset, char *target, char **source, long sources, char *format) {
  char *print_pos, **ptr, *fptr1, *fptr2 = NULL, *fptr3;
  long source_type, source_index, target_index;
  long i, j, rows;
  static char s[SDDS_MAXLINE];

  if ((target_index = SDDS_GetColumnIndex(SDDS_dataset, target)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (SDDS_GetColumnType(SDDS_dataset, target_index) != SDDS_STRING) {
    fprintf(stderr, "error: target column %s has wrong type for printing--must be string\n", target);
    return (0);
  }
  if ((rows = SDDS_CountRowsOfInterest(SDDS_dataset)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  for (j = 0; j < rows; j++) {
    fptr1 = format;
    s[0] = 0;
    if (sources == 0)
      strcpy_ss(s, format);
    for (i = 0; i < sources; i++) {
      print_pos = s + strlen(s);
      if ((source_index = SDDS_GetColumnIndex(SDDS_dataset, source[i])) < 0 ||
          (source_type = SDDS_GetColumnType(SDDS_dataset, source_index)) < 0) {
        if ((source_index = SDDS_GetParameterIndex(SDDS_dataset, source[i])) < 0 ||
            (source_type = SDDS_GetParameterType(SDDS_dataset, source_index)) < 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (0);
        }
        if (!(ptr = SDDS_GetParameter(SDDS_dataset, source[i], NULL))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (0);
        }
      } else {
        if (!(ptr = SDDS_GetValue(SDDS_dataset, source[i], j, NULL))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (0);
        }
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
      case SDDS_LONG:
        sprintf(print_pos, fptr1, *(long *)ptr);
        break;
      case SDDS_SHORT:
        sprintf(print_pos, fptr1, *(short *)ptr);
        break;
      case SDDS_CHARACTER:
        sprintf(print_pos, fptr1, *(char *)ptr);
        break;
      case SDDS_STRING:
        sprintf(print_pos, fptr1, *(char **)ptr);
        free(*ptr);
        break;
      default:
        bomb("error: unknown type encountered--this shouldn't happen (print_column_value)", NULL);
        break;
      }
      if (!(fptr1 = fptr2))
        break;
      *fptr1 = '%';
      free(ptr);
    }
    if (!SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, target_index, s, -1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
  }
  return (1);
}

void add_definition(DEFINITION **definition, long *definitions, void *structure, long type) {
  *definition = trealloc(*definition, sizeof(**definition) * (*definitions + 1));
  (*definition)[*definitions].structure = structure;
  (*definition)[*definitions].type = type;
  *definitions += 1;
}

RPNTEST_DEFINITION *process_new_rpntest_definition(char **argument, long arguments) {
  RPNTEST_DEFINITION *defi;

  if (!(defi = tmalloc(sizeof(*defi))))
    bomb("memory allocation failure (process_new_print_definition)", NULL);
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
  defi->expression = argument[1];
  defi->autostop = 0;
  if (arguments > 2) {
    if (strncmp(argument[2], "autostop", strlen(argument[2])) == 0)
      defi->autostop = 1;
    else
      return (NULL);
  }
  return (defi);
}

RPNEXPRESSION_DEFINITION *process_new_rpnexpression_definition(char **argument, long arguments) {
  RPNEXPRESSION_DEFINITION *defi;

  if (arguments > 2)
    return (NULL);
  if (!(defi = tmalloc(sizeof(*defi))))
    bomb("memory allocation failure (process_new_rpnexpression_definition)", NULL);
  defi->expression = argument[0];
  defi->repeat = 0;
  if (arguments == 2) {
    if (strncmp(argument[1], "repeat", strlen(argument[1])) == 0)
      defi->repeat = 1;
    else
      return NULL;
  }
  return (defi);
}

CLIP_DEFINITION *process_new_clip_definition(char **argument, long arguments) {
  CLIP_DEFINITION *defi;

  if (arguments < 2 || arguments > 3)
    return (NULL);
  if (!(defi = tmalloc(sizeof(*defi))))
    bomb("memory allocation failure (process_new_clip_definition)", NULL);
  if (sscanf(argument[0], "%ld", &defi->head) != 1 ||
      sscanf(argument[1], "%ld", &defi->tail) != 1 ||
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

SPARSE_DEFINITION *process_new_sparse_definition(char **argument, long arguments) {
  SPARSE_DEFINITION *defi;

  if (arguments < 1 || arguments > 2)
    return (NULL);
  if (!(defi = tmalloc(sizeof(*defi))))
    bomb("memory allocation failure (process_new_sparse_definition)", NULL);
  if (sscanf(argument[0], "%ld", &defi->interval) != 1 || defi->interval <= 0 ||
      (arguments == 2 && (sscanf(argument[1], "%ld", &defi->offset) != 1 || defi->offset < 0)))
    return NULL;
  return defi;
}

SAMPLE_DEFINITION *process_new_sample_definition(char **argument, long arguments) {
  SAMPLE_DEFINITION *defi;

  if (arguments < 1)
    return (NULL);
  if (!(defi = tmalloc(sizeof(*defi))))
    bomb("memory allocation failure (process_new_sample_definition)", NULL);
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
    bomb("memory allocation failure (process_new_system_definition)", NULL);
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
    bomb("unable to copy text of system definition (process_new_system_definition)", NULL);
  return (defi);
}

long system_column_value(SDDS_DATASET *SDDS_dataset, char *target, char *source) {
  char **source_value, *command;
  long i, rows, target_index, source_index;
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

  if (!(source_value = SDDS_GetColumn(SDDS_dataset, source))) {
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
    if (command)
      free(command);
  }
  free(source_value);
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

long SDDS_RedefineParameter(SDDS_DATASET *SDDS_dataset, char *parameter, char **argv, long argc) {
  char *value, *keyword;
  long i;

  for (i = 0; i < argc; i++) {
    keyword = argv[i];
    if (!(value = strchr(argv[i], '=')))
      return (0);
    *value++ = 0;
    if (!SDDS_ChangeParameterInformation(SDDS_dataset, keyword, value, SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME,
                                         parameter)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
  }
  return (1);
}

long SDDS_RedefineColumn(SDDS_DATASET *SDDS_dataset, char *column, char **argv, long argc) {
  char *value, *keyword;
  long i;

  for (i = 0; i < argc; i++) {
    keyword = argv[i];
    if (!(value = strchr(argv[i], '=')))
      return (0);
    *value++ = 0;
    if (!SDDS_ChangeColumnInformation(SDDS_dataset, keyword, value, SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME,
                                      column)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
  }
  return (1);
}

long add_sddsfile_arguments(SCANNED_ARG **scanned, int argc) {
  long iarg, j, datacode, index, argc_got;
  SDDS_DATASET dataset;
  static char *choice[2] = {"column", "parameter"};
  char *dataname, *filename, **argv_got;
  SCANNED_ARG *sa_got;

  index = 0;

  argv_got = NULL;
  for (iarg = 0; iarg < argc; iarg++) {
    if (((*scanned)[iarg].arg_type != OPTION) || strcmp((*scanned)[iarg].list[0], "getargs") != 0 ||
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
        long rows;
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
