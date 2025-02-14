/**
 * @file SDDS_mplsupport.c
 * @brief Functions and definitions for handling SDDS datasets and integrating with MPL (Matplotlib).
 *
 * This file provides a collection of macros, type definitions, and functions designed to manipulate
 * SDDS (Self Describing Data Sets) data structures for seamless integration with MPL (Matplotlib).
 * The functionalities include data sampling, string manipulation, match processing, column and
 * parameter handling, and writing MPL-compatible tables to SDDS files.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 *  M. Borland
 *  C. Saunders
 *  R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "table.h"
#include "match_string.h"

#define COLUMN_BASED 0
#define PARAMETER_BASED 1
#define DATA_CLASS_KEYWORDS 2
static char *data_class_keyword[DATA_CLASS_KEYWORDS] = {
  "column", "parameter"};

#define is_logic_character(c) ((c) == '|' || (c) == '&' || (c) == '!')

typedef struct
{
  char *name, *string;
  int32_t logic, last;
} MATCH_TERM;

/**
 * @brief Copies elements from the source array to the target array with sampling.
 *
 * This function copies elements from the source array to the target array, selecting
 * every `sample_interval`-th element from the source.
 *
 * @param target Pointer to the target array where sampled elements will be stored.
 * @param source Pointer to the source array from which elements are copied.
 * @param source_points Number of elements in the source array.
 * @param sample_interval Interval at which elements are sampled from the source.
 * @return The number of elements copied to the target array.
 */
int64_t copy_doubles_with_sampling(double *target, double *source, int64_t source_points, int64_t sample_interval) {
  int64_t i, j;
  for (i = j = 0; i < source_points; i += sample_interval, j++)
    target[j] = source[i];
  return (j);
}

/**
 * @brief Removes specified bounding characters from a string.
 *
 * This function trims characters specified in `t` from both the beginning and the end
 * of the string `s`. It modifies the string `s` in place and returns a pointer to it.
 *
 * @param s The string from which to remove bounding characters.
 * @param t A string containing characters to be removed from the start and end of `s`.
 * @return Pointer to the modified string `s`.
 */
char *delete_bounding_characters(char *s, char *t) {
  register char *ptr1, *ptr0, *ptrt;

  if (!s)
    return (s);
  ptr0 = s;
  while (*ptr0) {
    ptrt = t;
    while (*ptrt && *ptrt != *ptr0)
      ptrt++;
    if (*ptrt == *ptr0)
      ptr0++;
    else
      break;
  }

  ptr1 = ptr0 + strlen(ptr0) - 1;
  while (ptr1 != ptr0) {
    ptrt = t;
    while (*ptrt && *ptrt != *ptr1)
      ptrt++;
    if (*ptrt == *ptr1)
      ptr1--;
    else
      break;
  }

  *++ptr1 = 0;
  strcpy_ss(s, ptr0);
  return (s);
}

/**
 * @brief Processes match requests for columns and parameters based on an option string.
 *
 * This function parses the `option_string` to generate match criteria for columns and
 * parameters. It populates the `column_match` and `parameter_match` arrays with the
 * corresponding match terms.
 *
 * @param column_match Double pointer to an array of MATCH_TERM structures for columns.
 * @param parameter_match Double pointer to an array of MATCH_TERM structures for parameters.
 * @param option_string The string containing match options and criteria.
 * @return Returns 1 on successful processing, or 0 if an error occurs.
 */
int32_t process_match_requests(MATCH_TERM **column_match, MATCH_TERM **parameter_match, char *option_string) {
  int64_t i, matches, arguments, data_class;
  int64_t need_name, need_operation;
  char *ptr, **argument, *token;
  MATCH_TERM *match;
  char s[400], t[200];

  *column_match = *parameter_match = NULL;
  argument = NULL;

  while ((token = get_token_t(option_string, " "))) {
    if (SDDS_StringIsBlank(token))
      break;
    arguments = 0;
    argument = trealloc(argument, sizeof(*argument) * (arguments + 1));
    strcpy(t, token);
    while ((argument[arguments] = get_token_t(token, ","))) {
      arguments++;
      argument = trealloc(argument, sizeof(*argument) * (arguments + 1));
    }
    match = NULL;
    matches = 0;
    switch (data_class = match_string(argument[0], data_class_keyword, DATA_CLASS_KEYWORDS, UNIQUE_MATCH)) {
    case COLUMN_BASED:
      if (*column_match) {
        SDDS_SetError("Only one column-based match request is allowed (process_match_request)");
        return (0);
      }
      break;
    case PARAMETER_BASED:
      if (*parameter_match) {
        SDDS_SetError("Only one parameter-based match request is allowed (process_match_request)");
        return (0);
      }
      break;
    default:
      sprintf(s, "Unrecognized keyword for match-type: %s\nToken was: %s\nspecify column or parameter (process_match_request)", argument[0], t);
      SDDS_SetError(s);
      return (0);
    }

    need_name = 1;
    need_operation = -1;
    i = 1;
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
            if (match[matches].logic & SDDS_AND || match[matches].logic & SDDS_OR || match[matches].logic & SDDS_NEGATE_EXPRESSION)
              return (0);
            match[matches].logic |= SDDS_AND;
            break;
          case '|':
            if (match[matches].logic & SDDS_AND || match[matches].logic & SDDS_OR || match[matches].logic & SDDS_NEGATE_EXPRESSION)
              return (0);
            match[matches].logic |= SDDS_OR;
            break;
          case '!':
            if (!(match[matches].logic & SDDS_AND || match[matches].logic & SDDS_OR) || match[matches].logic & SDDS_NEGATE_EXPRESSION)
              return (0);
            match[matches].logic |= SDDS_NEGATE_EXPRESSION;
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
      match = trealloc(match, sizeof(*match) * (matches + 1));
      if (!(ptr = strchr(argument[i], '=')))
        return (0);
      *ptr = 0;
      match[matches].name = argument[i++];
      match[matches].string = ptr + 1;
      match[matches].logic = 0;
      match[matches].last = 1;
      if (matches)
        match[matches - 1].last = 0;
      if (arguments - i > 0 && argument[i][0] == '!') {
        match[matches].logic = SDDS_NEGATE_MATCH;
        i++;
      }
      if (++need_operation > 0)
        need_name = 0;
      else
        matches++;
    } while (arguments > i);
    match[0].logic |= SDDS_AND;
    if (data_class == COLUMN_BASED)
      *column_match = match;
    else
      *parameter_match = match;
  }
  return (1);
}

/**
 * @brief Prints a list of columns in the SDDS dataset.
 *
 * This function outputs the list of available columns in the provided SDDS dataset to the specified file pointer.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure containing dataset information.
 * @param message A message to display before listing the columns.
 * @param fp File pointer where the list of columns will be printed.
 */
void SDDS_PrintListOfColumns(SDDS_DATASET *SDDS_dataset, char *message, FILE *fp) {
  char **name;
  int32_t names, i;

  if (!(name = SDDS_GetColumnNames(SDDS_dataset, &names))) {
    SDDS_SetError("Problem getting list of column names");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  } else {
    if (!names)
      fputs("no columns defined in this file\n", fp);
    else {
      fputs(message, fp);
      for (i = 0; i < names; i++)
        fprintf(fp, "    %s\n", name[i]);
    }
  }
}

/**
 * @brief Prints a list of parameters in the SDDS dataset.
 *
 * This function outputs the list of available parameters in the provided SDDS dataset to the specified file pointer.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure containing dataset information.
 * @param message A message to display before listing the parameters.
 * @param fp File pointer where the list of parameters will be printed.
 */
void SDDS_PrintListOfParameters(SDDS_DATASET *SDDS_dataset, char *message, FILE *fp) {
  char **name;
  int32_t names, i;

  if (!(name = SDDS_GetParameterNames(SDDS_dataset, &names))) {
    SDDS_SetError("Problem getting list of parameter names");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  } else {
    if (!names)
      fputs("no parameters defined in this file\n", fp);
    else {
      fputs(message, fp);
      for (i = 0; i < names; i++)
        fprintf(fp, "    %s\n", name[i]);
    }
  }
}

/**
 * @brief Reads SDDS data into an MPL-compatible table structure.
 *
 * This function initializes an SDDS dataset from the specified file, processes the data
 * according to the provided sample interval and flags, and populates the `mpl_data` table
 * with the sampled data. It handles column and parameter-based matching as specified by
 * `SDDS_tags`.
 *
 * @param mpl_data Pointer to the TABLE structure where the SDDS data will be stored.
 * @param file The filename of the SDDS data file to read.
 * @param sample_interval The interval at which data points are sampled from the source.
 * @param mpl_flags Flags controlling the behavior of the MPL data processing.
 * @param SDDS_tags Optional tags specifying columns or parameters of interest.
 * @return Returns 1 on successful reading and processing, or 0 if an error occurs.
 */
int32_t SDDS_ReadIntoMplTable(TABLE *mpl_data, char *file, int64_t sample_interval, int32_t mpl_flags, char *SDDS_tags) {
  SDDS_DATASET SDDS_dataset;
  int32_t first;
  int64_t i, new_points, n_rows;
  SDDS_LAYOUT *layout;
  char *xname, *yname, *sxname, *syname, *option_string, *ptr;
  COLUMN_DEFINITION *xdef, *ydef, *sxdef, *sydef;
  PARAMETER_DEFINITION *xpdef, *ypdef, *sxpdef, *sypdef;
  PARAMETER_DEFINITION *titledef, *toplinedef, *pardef;
  char s[SDDS_MAXLINE];
  double *data;
  MATCH_TERM *column_match, *parameter_match;
  int32_t accept = 1;

  xname = yname = sxname = syname = NULL;
  xdef = ydef = sxdef = sydef = NULL;
  xpdef = ypdef = sxpdef = sypdef = titledef = toplinedef = NULL;
  n_rows = 0;

  if (!SDDS_InitializeInput(&SDDS_dataset, file)) {
    SDDS_PrintErrors(stderr, 1);
    return (0);
  }
  layout = &SDDS_dataset.layout;
  if (SDDS_dataset.layout.n_columns < 1 && SDDS_dataset.layout.n_parameters < 1)
    return (0);

  first = 1;
  while (SDDS_ReadPage(&SDDS_dataset) > 0) {
    if (first) {
      first = 0;
#if 0
	  if ((pardef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplTitle")) && pardef->type == SDDS_STRING)
	    mpl_data->title = SDDS_GetParameter(&SDDS_dataset, "mplTitle", NULL);
	  else
	    SDDS_CopyString(&mpl_data->title, "");
	  if ((pardef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplTopline")) && pardef->type == SDDS_STRING)
	    mpl_data->topline = SDDS_GetParameter(&SDDS_dataset, "mplTopline", NULL);
	  else
	    SDDS_CopyString(&mpl_data->topline, "");
#endif

      xname = yname = sxname = syname = option_string = NULL;
      xdef = ydef = sxdef = sydef = NULL;
      xpdef = ypdef = sxpdef = sypdef = NULL;
      if (SDDS_tags) {
        /* expect columns-of-interest to be specified in this string as
	       * <xname>+<yname>[+<syname>][,<options>] or
	       * <xname>+<yname>[+<sxname>+<syname>][,<options>]
	       */
        xname = SDDS_tags;
        if ((yname = strchr(xname, '+'))) {
          ptr = yname;
          while ((option_string = strchr(ptr, ','))) {
            if (option_string != ptr && *(option_string - 1) == '\\')
              ptr = option_string + 1;
            else {
              *option_string++ = 0;
              break;
            }
          }
          *yname++ = 0;
          if ((sxname = strchr(yname, '+'))) {
            *sxname++ = 0;
            if ((syname = strchr(sxname, '+'))) {
              *syname++ = 0;
            } else {
              syname = sxname;
              sxname = NULL;
            }
          }
        }
        delete_bounding_characters(xname, "\"'");
        delete_bounding_characters(yname, "\"'");
        if (sxname)
          delete_bounding_characters(sxname, "\"'");
        if (syname)
          delete_bounding_characters(syname, "\"'");
      }

      if (!xname || !yname) {
        xname = yname = NULL;
        /* No columns-of-interest found in SDDS_tags.  Check for parameters mplxName and mplyName */
        if ((pardef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplxName")) && pardef->type == SDDS_STRING && (pardef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplyName")) && pardef->type == SDDS_STRING) {
          xname = SDDS_GetParameter(&SDDS_dataset, "mplxName", NULL);
          yname = SDDS_GetParameter(&SDDS_dataset, "mplyName", NULL);
          if (xname && yname) {
            /* check for mplSigmaxName and mplSigmayName */
            if ((pardef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplSigmayName")) && pardef->type == SDDS_STRING)
              syname = SDDS_GetParameter(&SDDS_dataset, "mplSigmayName", NULL);
            if ((pardef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplSigmaxName")) && pardef->type == SDDS_STRING)
              sxname = SDDS_GetParameter(&SDDS_dataset, "mplSigmaxName", NULL);
          }
        }
      }

      if (!xname || !yname) {
        /* No columns-of-interest found in SDDS_tags or parameters. Just take the first two numeric
	       * column definitions in order 
	       */
        for (i = 0; i < layout->n_columns; i++) {
          if (SDDS_NUMERIC_TYPE(layout->column_definition[i].type)) {
            if (!xname)
              xname = layout->column_definition[i].name;
            else if (!yname)
              yname = layout->column_definition[i].name;
            else
              break;
          }
        }
        if (sxname && !syname) {
          syname = sxname;
          sxname = NULL;
        }
      }

      if (!xname || !yname)
        return (0);

      if (!((xdef = SDDS_GetColumnDefinition(&SDDS_dataset, xname)) && SDDS_NUMERIC_TYPE(xdef->type)) && !((xpdef = SDDS_GetParameterDefinition(&SDDS_dataset, xname)) && SDDS_NUMERIC_TYPE(xpdef->type))) {
        fprintf(stderr, "error: column (or parameter) %s does not exist or is non-numeric\n", xname);
        SDDS_PrintListOfColumns(&SDDS_dataset, "Valid columns are:\n", stderr);
        SDDS_PrintListOfParameters(&SDDS_dataset, "Valid parameters are:\n", stderr);
        return (0);
      }
      if (xdef) {
        if (!((ydef = SDDS_GetColumnDefinition(&SDDS_dataset, yname)) && SDDS_NUMERIC_TYPE(ydef->type))) {
          fprintf(stderr, "error: column %s does not exist or is non-numeric\n", yname);
          SDDS_PrintListOfColumns(&SDDS_dataset, "Valid columns are:\n", stderr);
          return (0);
        }
        if (sxname && !((sxdef = SDDS_GetColumnDefinition(&SDDS_dataset, sxname)) && SDDS_NUMERIC_TYPE(sxdef->type))) {
          fprintf(stderr, "error: column %s does not exist or is non-numeric\n", sxname);
          SDDS_PrintListOfColumns(&SDDS_dataset, "Valid columns are:\n", stderr);
          return (0);
        }
        if (syname && !((sydef = SDDS_GetColumnDefinition(&SDDS_dataset, syname)) && SDDS_NUMERIC_TYPE(sydef->type))) {
          fprintf(stderr, "error: column %s does not exist or is non-numeric\n", syname);
          SDDS_PrintListOfColumns(&SDDS_dataset, "Valid columns are:\n", stderr);
          return (0);
        }
        if (xdef->symbol && !(mpl_flags & SDDS_NOCOMPRESS_NAMES))
          delete_chars(xdef->symbol, " ");
        if (ydef->symbol && !(mpl_flags & SDDS_NOCOMPRESS_NAMES))
          delete_chars(ydef->symbol, " ");
        if (sxdef && sxdef->symbol && !(mpl_flags & SDDS_NOCOMPRESS_NAMES))
          delete_chars(sxdef->symbol, " ");
        if (sydef && sydef->symbol && !(mpl_flags & SDDS_NOCOMPRESS_NAMES))
          delete_chars(sydef->symbol, " ");

        if (xdef->units && !SDDS_StringIsBlank(xdef->units))
          sprintf(s, "%s (%s)", xdef->symbol ? xdef->symbol : xdef->name, xdef->units);
        else
          sprintf(s, "%s", xdef->symbol ? xdef->symbol : xdef->name);
        SDDS_CopyString(&mpl_data->xlab, s);
        if (ydef->units && !SDDS_StringIsBlank(ydef->units))
          sprintf(s, "%s (%s)", ydef->symbol ? ydef->symbol : ydef->name, ydef->units);
        else
          sprintf(s, "%s", ydef->symbol ? ydef->symbol : ydef->name);
        SDDS_CopyString(&mpl_data->ylab, s);
        toplinedef = titledef = NULL;
        if ((toplinedef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplTopline")) && (toplinedef->type != SDDS_STRING || !SDDS_GetParameter(&SDDS_dataset, toplinedef->name, &mpl_data->topline))) {
          SDDS_FreeParameterDefinition(toplinedef);
          toplinedef = NULL;
        }
        if (!toplinedef) {
          if (layout->description)
            SDDS_CopyString(&mpl_data->topline, layout->description);
          else
            SDDS_CopyString(&mpl_data->topline, "");
        }
        if ((titledef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplTitle"))) {
          if (titledef->type != SDDS_STRING || !SDDS_GetParameter(&SDDS_dataset, titledef->name, &mpl_data->title))
            titledef = NULL;
        }
        if (!titledef) {
          if (!option_string)
            sprintf(s, "%s vs %s", ydef->description ? ydef->description : (ydef->symbol ? ydef->symbol : yname), xdef->description ? xdef->description : (xdef->symbol ? xdef->symbol : xname));
          else
            sprintf(s, "%s vs %s : %s", ydef->description ? ydef->description : (ydef->symbol ? ydef->symbol : yname), xdef->description ? xdef->description : (xdef->symbol ? xdef->symbol : xname), option_string);
          SDDS_CopyString(&mpl_data->title, s);
        }
      } else {
        if (!((ypdef = SDDS_GetParameterDefinition(&SDDS_dataset, yname)) && SDDS_NUMERIC_TYPE(ypdef->type))) {
          fprintf(stderr, "error: parameter %s does not exist or is non-numeric\n", yname);
          SDDS_PrintListOfParameters(&SDDS_dataset, "Valid parameters are:\n", stderr);
          return (0);
        }
        if (sxname && !((sxpdef = SDDS_GetParameterDefinition(&SDDS_dataset, sxname)) && SDDS_NUMERIC_TYPE(sxpdef->type))) {
          fprintf(stderr, "error: parameter %s does not exist or is non-numeric\n", sxname);
          SDDS_PrintListOfParameters(&SDDS_dataset, "Valid parameters are:\n", stderr);
          return (0);
        }
        if (syname && !((sypdef = SDDS_GetParameterDefinition(&SDDS_dataset, syname)) && SDDS_NUMERIC_TYPE(sypdef->type))) {
          fprintf(stderr, "error: parameter %s does not exist or is non-numeric\n", syname);
          SDDS_PrintListOfParameters(&SDDS_dataset, "Valid parameters are:\n", stderr);
          return (0);
        }

        if (xpdef->units && !SDDS_StringIsBlank(xpdef->units))
          sprintf(s, "%s (%s)", xpdef->symbol ? delete_chars(xpdef->symbol, " ") : xpdef->name, xpdef->units);
        else
          sprintf(s, "%s", xpdef->symbol ? delete_chars(xpdef->symbol, " ") : xpdef->name);
        SDDS_CopyString(&mpl_data->xlab, s);
        if (ypdef->units && !SDDS_StringIsBlank(ypdef->units))
          sprintf(s, "%s (%s)", ypdef->symbol ? delete_chars(ypdef->symbol, " ") : ypdef->name, ypdef->units);
        else
          sprintf(s, "%s", ypdef->symbol ? delete_chars(ypdef->symbol, " ") : ypdef->name);
        SDDS_CopyString(&mpl_data->ylab, s);
        toplinedef = titledef = NULL;
        if ((toplinedef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplTopline")) && (toplinedef->type != SDDS_STRING || !SDDS_GetParameter(&SDDS_dataset, toplinedef->name, &mpl_data->topline))) {
          SDDS_FreeParameterDefinition(toplinedef);
          toplinedef = NULL;
        }
        if (!toplinedef) {
          if (layout->description)
            SDDS_CopyString(&mpl_data->topline, layout->description);
          else
            SDDS_CopyString(&mpl_data->topline, "");
        }
        if ((titledef = SDDS_GetParameterDefinition(&SDDS_dataset, "mplTitle"))) {
          if (titledef->type != SDDS_STRING || !SDDS_GetParameter(&SDDS_dataset, titledef->name, &mpl_data->title))
            titledef = NULL;
        }
        if (!titledef) {
          if (!option_string)
            sprintf(s, "%s vs %s", ypdef->description ? ypdef->description : (ypdef->symbol ? ypdef->symbol : yname), xpdef->description ? xpdef->description : (xpdef->symbol ? xpdef->symbol : xname));
          else
            sprintf(s, "%s vs %s : %s", ypdef->description ? ypdef->description : (ypdef->symbol ? ypdef->symbol : yname), xpdef->description ? xpdef->description : (xpdef->symbol ? xpdef->symbol : xname), option_string);
          SDDS_CopyString(&mpl_data->title, s);
        }
      }

      mpl_data->c1 = mpl_data->c2 = mpl_data->s1 = mpl_data->s2 = NULL;
      mpl_data->n_data = 0;
      mpl_data->flags = (sxname ? SIGMA_X_PRESENT : 0) + (syname ? SIGMA_Y_PRESENT : 0);

      column_match = parameter_match = NULL;
      if (option_string && !process_match_requests(&column_match, &parameter_match, option_string))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } /* end of block for first page */
    if (!SDDS_dataset.n_rows && xdef)
      continue;

    accept = 1;
    if (parameter_match) {
      i = -1;
      do {
        i++;
        if (!(pardef = SDDS_GetParameterDefinition(&SDDS_dataset, parameter_match[i].name)) || !(pardef->type == SDDS_STRING || pardef->type == SDDS_CHARACTER)) {
          fprintf(stderr, "error: unknown or numeric parameter %s given for match\n", parameter_match[i].name);
          exit(1);
        }
        if (pardef->type == SDDS_STRING) {
          char **ppc;
          ppc = SDDS_GetParameter(&SDDS_dataset, parameter_match[i].name, NULL);
          strcpy(s, *ppc);
        } else {
          char *pc;
          pc = SDDS_GetParameter(&SDDS_dataset, parameter_match[i].name, NULL);
          sprintf(s, "%c", *pc);
        }
        accept = SDDS_Logic(accept, wild_match(s, parameter_match[i].string), parameter_match[i].logic);
      } while (!parameter_match[i].last);
    }
    if (!accept)
      continue;

    if (xdef) {
      if (!SDDS_SetColumnFlags(&SDDS_dataset, 1) ||
          (sxname && syname &&
           !SDDS_SetColumnsOfInterest(&SDDS_dataset, SDDS_NAME_STRINGS, xname, yname, sxname, syname, NULL)) ||
          (syname && !sxname && !SDDS_SetColumnsOfInterest(&SDDS_dataset, SDDS_NAME_STRINGS, xname, yname, syname, NULL)) || !SDDS_SetColumnsOfInterest(&SDDS_dataset, SDDS_NAME_STRINGS, xname, yname, NULL))
        return (0);

      if (column_match) {
        i = -1;
        do {
          i++;
          if (SDDS_MatchRowsOfInterest(&SDDS_dataset, column_match[i].name, column_match[i].string, column_match[i].logic) < 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        } while (!column_match[i].last);
      }

      if ((n_rows = SDDS_CountRowsOfInterest(&SDDS_dataset)) <= 0)
        continue;

      if (!(new_points = ceil((1.0 * n_rows) / sample_interval)))
        new_points = 1;
    } else
      new_points = 1;

    /* allocate all four arrays, since that is the expected behavior from get_table() */
    if (!(mpl_data->c1 = SDDS_Realloc(mpl_data->c1, sizeof(*mpl_data->c1) * (mpl_data->n_data + new_points))) ||
        !(mpl_data->c2 = SDDS_Realloc(mpl_data->c2, sizeof(*mpl_data->c2) * (mpl_data->n_data + new_points))) ||
        !(mpl_data->s1 = SDDS_Realloc(mpl_data->s1, sizeof(*mpl_data->s1) * (mpl_data->n_data + new_points))) || !(mpl_data->s2 = SDDS_Realloc(mpl_data->s2, sizeof(*mpl_data->s2) * (mpl_data->n_data + new_points)))) {
      SDDS_SetError("Allocation failure creating mpl-compatible structure");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    for (i = 0; i < new_points; i++)
      mpl_data->c1[i + mpl_data->n_data] = mpl_data->c2[i + mpl_data->n_data] = mpl_data->s1[i + mpl_data->n_data] = mpl_data->s2[i + mpl_data->n_data] = 0;

    if (xdef) {
      if (!(data = SDDS_GetColumnInDoubles(&SDDS_dataset, xdef->name)) || (i = copy_doubles_with_sampling((mpl_flags & SWAP ? mpl_data->c2 : mpl_data->c1) + mpl_data->n_data, data, n_rows, sample_interval)) != new_points) {
        sprintf(s, "Sampling problem: %" PRId64 " rows created, %" PRId64 " expected", i, new_points);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      free(data);
      if (!(data = SDDS_GetColumnInDoubles(&SDDS_dataset, ydef->name)) || (i = copy_doubles_with_sampling((mpl_flags & SWAP ? mpl_data->c1 : mpl_data->c2) + mpl_data->n_data, data, n_rows, sample_interval)) != new_points) {
        sprintf(s, "Sampling problem: %" PRId64 " rows created, %" PRId64 " expected", i, new_points);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
      free(data);
      if (sxdef) {
        if (!(data = SDDS_GetColumnInDoubles(&SDDS_dataset, sxdef->name)) || (i = copy_doubles_with_sampling((mpl_flags & SWAP ? mpl_data->s2 : mpl_data->s1) + mpl_data->n_data, data, n_rows, sample_interval)) != new_points) {
          sprintf(s, "Sampling problem: %" PRId64 " rows created, %" PRId64 " expected", i, new_points);
          SDDS_SetError(s);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (0);
        }
        free(data);
      }
      if (sydef) {
        if (!(data = SDDS_GetColumnInDoubles(&SDDS_dataset, sydef->name)) || (i = copy_doubles_with_sampling((mpl_flags & SWAP ? mpl_data->s1 : mpl_data->s2) + mpl_data->n_data, data, n_rows, sample_interval)) != new_points) {
          sprintf(s, "Sampling problem: %" PRId64 " rows created, %" PRId64 " expected", i, new_points);
          SDDS_SetError(s);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (0);
        }
        free(data);
      }
    } else {
      int32_t buffer[16];
      SDDS_GetParameter(&SDDS_dataset, xpdef->name, buffer);
      mpl_data->c1[mpl_data->n_data] = SDDS_ConvertToDouble(xpdef->type, buffer, 0);
      SDDS_GetParameter(&SDDS_dataset, ypdef->name, buffer);
      mpl_data->c2[mpl_data->n_data] = SDDS_ConvertToDouble(ypdef->type, buffer, 0);
      if (sxpdef) {
        SDDS_GetParameter(&SDDS_dataset, sxpdef->name, buffer);
        mpl_data->s1[mpl_data->n_data] = SDDS_ConvertToDouble(sxpdef->type, buffer, 0);
      }
      if (sypdef) {
        SDDS_GetParameter(&SDDS_dataset, sypdef->name, buffer);
        mpl_data->s2[mpl_data->n_data] = SDDS_ConvertToDouble(sypdef->type, buffer, 0);
      }
      if (SDDS_NumberOfErrors()) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
    }
    mpl_data->n_data += new_points;
  }
  if (xdef)
    SDDS_FreeColumnDefinition(xdef);
  if (ydef)
    SDDS_FreeColumnDefinition(ydef);
  if (sxdef)
    SDDS_FreeColumnDefinition(sxdef);
  if (sydef)
    SDDS_FreeColumnDefinition(sydef);
  if (titledef)
    SDDS_FreeParameterDefinition(titledef);
  if (toplinedef)
    SDDS_FreeParameterDefinition(toplinedef);
  if (first || !SDDS_Terminate(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  return (1);
}

/**
 * @brief Cleans and fixes an MPL-compatible name by removing specific sequences and extra spaces.
 *
 * This function iterates through the provided `name` string and removes any occurrence of a
 * dollar sign ('$') followed by specific characters (`'a'`, `'b'`, `'n'`, `'g'`, `'r'`,
 * `'s'`, `'e'`, `'d'`, `'i'`, `'v'`, `'u'`). Additionally, it removes any extra spaces within
 * the string, ensuring that only single spaces remain between words.
 *
 * @param name A mutable string representing the name to be fixed. The string is modified in place.
 *
 * @note This function assumes that the `name` string is mutable and has sufficient space to handle in-place modifications.
 */
void SDDS_FixMplName(char *name) {
  char *ptr, *ptr1;
  ptr = name;
  while ((ptr = strchr(ptr, '$'))) {
    switch (*(ptr + 1)) {
    case 'a':
    case 'b':
    case 'n':
    case 'g':
    case 'r':
    case 's':
    case 'e':
    case 'd':
    case 'i':
    case 'v':
    case 'u':
      strcpy(ptr, ptr + 2);
      break;
    default:
      ptr += 1;
      break;
    }
  }
  ptr = name;
  while ((ptr = strchr(ptr, ' '))) {
    ptr1 = ptr;
    while (*ptr1 == ' ')
      ptr1++;
    strcpy(ptr, ptr1);
  }
}

/**
 * @brief Extracts the name and unit from a labeled string.
 *
 * This function parses a label string to separate the name and its associated unit. It looks for
 * the pattern " (unit)" within the label. If found, it splits the label into the `name` and `unit`
 * components. The function also trims any trailing spaces from the `name`.
 *
 * @param name  Double pointer to a string where the extracted name will be stored. Memory is allocated internally.
 * @param unit  Double pointer to a string where the extracted unit will be stored. Memory is allocated internally.
 *              If no unit is found, `*unit` is set to `NULL`.
 * @param label The input label string containing the name and optionally the unit in the format "Name (Unit)".
 *
 * @note The caller is responsible for freeing the memory allocated for `*name` and `*unit` if they are not `NULL`.
 */
void SDDS_ExtractNameAndUnit(char **name, char **unit, char *label) {
  char *ptr, *uptr;

  if ((uptr = strstr(label, " ("))) {
    *uptr = 0;
    uptr += 2;
    if ((ptr = strchr(uptr, ')')))
      *ptr = 0;
    SDDS_CopyString(unit, uptr);
  } else
    *unit = NULL;
  ptr = label + strlen(label) - 1;
  while (ptr != label && *ptr == ' ')
    *ptr-- = 0;
  SDDS_CopyString(name, label);
}

/**
 * @brief Adds an MPL-compatible column definition to an SDDS dataset.
 *
 * This function defines a new column in the provided `SDDS_dataset` based on the given label, suffix,
 * and default name. It handles the extraction of the symbol and unit from the label, ensures that
 * the column name is unique within the dataset, and appends the suffix to create the final column name.
 *
 * @param SDDS_dataset  Pointer to the SDDS_DATASET structure where the column will be added.
 * @param label         The label string containing the name and optionally the unit in the format "Name (Unit)".
 * @param suffix        An optional suffix to append to the column name. Can be `NULL` if no suffix is needed.
 * @param default_name  The default name to use if the `label` is blank.
 * @param filename      The name of the file being processed, used for error reporting.
 *
 * @return Returns the index of the newly added column on success, or `-1` if an error occurs.
 *
 * @see SDDS_DefineColumn
 */
int32_t SDDS_AddMplDefinition(SDDS_DATASET *SDDS_dataset, char *label, char *suffix, char *default_name, char *filename) {
  char *symbol, *name, *unit;
  int32_t index;

  unit = NULL;
  if (SDDS_StringIsBlank(label)) {
    name = SDDS_Malloc(sizeof(*name) * (strlen(default_name) + (suffix ? strlen(suffix) : 0) + 1));
    sprintf(name, "%s%s", default_name, suffix ? suffix : "");
    SDDS_CopyString(&symbol, name);
  } else {
    SDDS_ExtractNameAndUnit(&symbol, &unit, label);
    name = SDDS_Malloc(sizeof(*name) * (strlen(symbol) + (suffix ? strlen(suffix) : 0) + 1));
    sprintf(name, "%s%s", symbol, suffix ? suffix : "");
    SDDS_FixMplName(name);
  }
  if (SDDS_GetColumnIndex(SDDS_dataset, name) >= 0) {
    fprintf(stderr, "error: column name %s already exists in file %s\n", name, filename);
    return -1;
  }
  if ((index = SDDS_DefineColumn(SDDS_dataset, name, symbol, unit, NULL, NULL, SDDS_DOUBLE, 0)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return -1;
  }
  free(name);
  free(symbol);
  if (unit)
    free(unit);
  return index;
}

/**
 * @brief Writes an MPL-compatible table to an SDDS file.
 *
 * This function initializes an SDDS output file, defines necessary columns based on the provided
 * `mpl_data`, and writes the data to the file. It handles optional sigma columns, ensures that
 * the output file does not overwrite existing files by renaming them if necessary, and respects
 * environment variables for output configuration.
 *
 * @param mpl_data Pointer to the TABLE structure containing MPL data to be written.
 * @param file     The filename of the SDDS file where the data will be written.
 *
 * @return Returns `1` on successful writing of the MPL table, or `0` if an error occurs.
 *
 * @note This function relies on several SDDS utility functions and assumes that the SDDS library
 *       is properly initialized and configured.
 *
 * @see SDDS_DefineColumn
 * @see SDDS_WritePage
 */
int32_t SDDS_WriteMplTable(TABLE *mpl_data, char *file) {
  SDDS_DATASET page;
  char *mplSDDSOutput = NULL;
  int32_t mplSDDS_datamode = SDDS_BINARY, mplSDDS_disable = 1;
  int32_t sx_index, sy_index;

  if (!mpl_data) {
    fprintf(stderr, "error: NULL mpl TABLE passed (SDDS_WriteMplTable)\n");
    return 0;
  }
  if (!file) {
    fprintf(stderr, "error: NULL filename passed (SDDS_WriteMplTable)\n");
    return 0;
  }
  if (!mplSDDSOutput) {
    if (!(mplSDDSOutput = getenv("mplSDDSOutput")))
      SDDS_CopyString(&mplSDDSOutput, "");
  }
  if (mplSDDSOutput[0]) {
    if (strstr(mplSDDSOutput, "enable"))
      mplSDDS_disable = 0;
    if (strstr(mplSDDSOutput, "ascii"))
      mplSDDS_datamode = SDDS_ASCII;
  }

  if (mplSDDS_disable)
    return 0;

  if (fexists(file)) {
    char *buffer;
    buffer = SDDS_Malloc(sizeof(*file) * (strlen(file) + 2));
    sprintf(buffer, "%s~", file);
    if (rename(file, buffer) != 0) {
      SDDS_SetError("Cannot save previous version of output file (SDDS_WriteMplTable)");
      free(buffer);
      return 0;
    }
    free(buffer);
  }

  if (!SDDS_InitializeOutput(&page, mplSDDS_datamode, 1, NULL, NULL, file) ||
      (mpl_data->topline && !SDDS_StringIsBlank(mpl_data->topline) && SDDS_DefineParameter(&page, "mplTopline", NULL, NULL, NULL, NULL, SDDS_STRING, mpl_data->topline) < 0) || (mpl_data->title && !SDDS_StringIsBlank(mpl_data->title) && SDDS_DefineParameter(&page, "mplTitle", NULL, NULL, NULL, NULL, SDDS_STRING, mpl_data->title) < 0)) {
    SDDS_SetError("Problem initializing SDDS output of mpl page data (SDDS_WriteMplTable)");
    return 0;
  }
  if (SDDS_AddMplDefinition(&page, mpl_data->xlab, NULL, "x", file) < 0 || SDDS_AddMplDefinition(&page, mpl_data->ylab, NULL, "y", file) < 0) {
    SDDS_SetError("Unable to define primary mpl columns for SDDS output (SDDS_WriteMplTable)");
    return 0;
  }
  sx_index = sy_index = -1;
  if (mpl_data->flags & SIGMA_X_PRESENT && (sx_index = SDDS_AddMplDefinition(&page, mpl_data->xlab, "Sigma", "x", file)) < 0) {
    SDDS_SetError("Unable to define sigma-x column for SDDS output (SDDS_WriteMplTable)");
    return 0;
  }
  if (mpl_data->flags & SIGMA_Y_PRESENT && (sy_index = SDDS_AddMplDefinition(&page, mpl_data->ylab, "Sigma", "y", file)) < 0) {
    SDDS_SetError("Unable to define sigma-y column for SDDS output (SDDS_WriteMplTable)");
    return 0;
  }
  if (!SDDS_WriteLayout(&page)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (!SDDS_StartPage(&page, mpl_data->n_data)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (!SDDS_SetColumn(&page, SDDS_SET_BY_INDEX, mpl_data->c1, mpl_data->n_data, 0) ||
      !SDDS_SetColumn(&page, SDDS_SET_BY_INDEX, mpl_data->c2, mpl_data->n_data, 1) || (sx_index >= 0 && !SDDS_SetColumn(&page, SDDS_SET_BY_INDEX, mpl_data->s1, mpl_data->n_data, sx_index)) || (sy_index >= 0 && !SDDS_SetColumn(&page, SDDS_SET_BY_INDEX, mpl_data->s2, mpl_data->n_data, sy_index))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (!SDDS_WritePage(&page) || !SDDS_Terminate(&page)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  return 1;
}



