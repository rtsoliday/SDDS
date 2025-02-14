/**
 * @file sddsdiff.c
 * @brief Compare two SDDS files for differences in definitions and data.
 *
 * @details
 * The program `sddsdiff` compares two SDDS (Self Describing Data Sets) files by analyzing their
 * definitions (columns, parameters, arrays) and data, supporting various comparison criteria such as
 * exact matches, numerical tolerances, and absolute differences.
 * It provides detailed reports on discrepancies in definitions or data values.
 *
 * @section Usage
 * ```
 * sddsdiff <file1> <file2>
 *          [-compareCommon[=column|parameter|array]] 
 *          [-columns=<col1>[,<col2>...]]
 *          [-parameters=<par1>[,<par2>...]]
 *          [-arrays=<array1>[,<array2>...]] 
 *          [-tolerance=<value>] 
 *          [-precision=<integer>]
 *          [-format=float=<string>|double=<string>|longdouble=<string>|string=<string>] 
 *          [-exact] 
 *          [-absolute] 
 *          [-rowlabel=<column-name>[,nocomparison]] 
 *          [-ignoreUnits] 
 * ```
 *
 * @section Options
 * | Option                     | Description                                                                 |
 * |----------------------------|-----------------------------------------------------------------------------|
 * | `-compareCommon`           | Compare only common columns, parameters, or arrays.                        |
 * | `-columns`                 | Specify columns to compare.                                                |
 * | `-parameters`              | Specify parameters to compare.                                             |
 * | `-arrays`                  | Specify arrays to compare.                                                 |
 * | `-tolerance`               | Set numerical comparison tolerance.                                        |
 * | `-precision`               | Set the precision for numerical comparison.                                |
 * | `-format`                  | Specify the format for printing float, double, long double, or string data.|
 * | `-exact`                   | Compare values exactly (mutually exclusive with tolerance and precision).  |
 * | `-absolute`                | Compare absolute values.                                                   |
 * | `-rowlabel`                | Use a column to label rows for comparison.                                 |
 * | `-ignoreUnits`             | Ignore units during the comparison.                                        |
 *
 * @subsection Incompatibilities
 *   - `-exact` is incompatible with:
 *     - `-tolerance`
 *     - `-precision`
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author
 * H. Shang, R. Soliday, L. Emery, M. Borland
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "SDDSutils.h"
#include <float.h>

/* Enumeration for option types */
enum option_type {
  CLO_COMPARECOMMON,
  CLO_COLUMNS,
  CLO_PARAMETERS,
  CLO_ARRAYS,
  CLO_TOLERANCE,
  CLO_PRECISION,
  CLO_FORMAT,
  CLO_EXACT,
  CLO_ABSOLUTE,
  CLO_ROWLABEL,
  CLO_IGNORE_UNITS,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "compareCommon",
  "columns",
  "parameters",
  "arrays",
  "tolerance",
  "precision",
  "format",
  "exact",
  "absolute",
  "rowlabel",
  "ignoreUnits"};

char *USAGE1 = "Usage: sddsdiff <file1> <file2>\n\
               [-compareCommon[=column|parameter|array]]\n\
               [-columns=<col1>[,<col2>...]]\n\
               [-parameters=<par1][,<par2>...]]\n\
               [-arrays=<array1>[,<array2>...]]\n\
               [-tolerance=<value>]\n\
               [-precision=<integer>]\n\
               [-format=float=<string>|double=<string>|longdouble=<string>|string=<string>]\n\
               [-exact]\n\
               [-absolute]\n\
               [-rowlabel=<column-name>[,nocomparison]]\n\
               [-ignoreUnits]\n\
Options:\n\
  -compareCommon[=column|parameter|array]   Compare only the common items.\n\
  -columns=<col1>[,<col2>...]             Specify columns to compare.\n\
  -parameters=<par1>[,<par2>...]          Specify parameters to compare.\n\
  -arrays=<array1>[,<array2>...]          Specify arrays to compare.\n\
  -tolerance=<value>                      Set tolerance for numerical comparisons.\n\
  -precision=<integer>                    Set precision for floating-point comparisons.\n\
  -format=float=<string>                  Set print format for float data.\n\
  -format=double=<string>                 Set print format for double data.\n\
  -format=longdouble=<string>             Set print format for long double data.\n\
  -format=string=<string>                 Set print format for string data.\n\
  -exact                                  Compare values exactly.\n\
  -absolute                               Compare absolute values, ignoring signs.\n\
  -rowlabel=<column-name>[,nocomparison]   Use a column to label rows.\n\
  -ignoreUnits                            Do not compare units of items.\n";

char *USAGE2 = "\n\
Description:\n\
  sddsdiff compares two SDDS files by checking their definitions and data. It reports differences in columns,\n\
  parameters, and arrays based on the specified options.\n\
\n\
Example:\n\
  sddsdiff data1.sdds data2.sdds -compareCommon=column -tolerance=1e-5 -absolute\n\
\n\
Program by Hairong Shang. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define SDDS_COLUMN_TYPE 0
#define SDDS_PARAMETER_TYPE 1
#define SDDS_ARRAY_TYPE 2
/* type=0,1,2 for Column, Parameter, Array */
long CompareDefinitions(SDDS_DATASET *dataset1, SDDS_DATASET *dataset2, char *file1, char *file2, int32_t *names, char ***name, int32_t **dataType, long type, long compareCommon, char *rowLabelColumn, long notCompareRowLabel, short ignoreUnits);
long CompareData(SDDS_DATASET *dataset1, SDDS_DATASET *dataset2, char *file1, char *file2, long names, char **name, int32_t *dataType, long type, long page, long double tolerance, long double precisionTolerance, char *floatFormat, char *doubleFormat, char *ldoubleFormat, char *stringFormat,
                 long absolute, void *rowLabel, long rowLabelType, char *labelName);
long compare_two_data(void *data1, void *data2, long index, long datatype, long first, long flags, char *name, long page, long double tolerance, long double precisionTolerance, char *floatFormat, char *doubleFormat, char *ldoubleFormat, char *stringFormat, char *longFormat, char *ulongFormat,
                      char *shortFormat, char *ushortFormat, char *charFormat, long absolute, long parameter, char *labelName);
void printTitle(long flags, char *name, long page, long absolute, char *labelName);

#define COMPARE_COMMON_COLUMN 0x0001U
#define COMPARE_COMMON_PARAMETER 0x0002U
#define COMPARE_COMMON_ARRAY 0x0004U

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET table1, table2;
  char *file1, *file2;
  long different = 0, i, pages1, pages2, pagediff = 0, i_arg, absolute = 0;
  int32_t *columnDataType, *parDataType, *arrayDataType;
  int32_t columns, parameters, arrays, columnMatches, parameterMatches, arrayMatches;
  int64_t rows1, rows2;
  char **columnName, **parameterName, **arrayName, **columnMatch, **parameterMatch, **arrayMatch;
  long column_provided, parameter_provided, array_provided, precision, labelFromSecondFile = 0, rowLabelType = 0, rowLabelIndex = -1, notCompareRowLabel = 0;
  long double tolerance = 0.0L, precisionTolerance = 0.0L;
  unsigned long compareCommonFlags = 0, dummyFlags = 0;
  char *floatFormat, *doubleFormat, *ldoubleFormat, *stringFormat, *rowLabelColumn;
  void *rowLabel;
  short ignoreUnits = 0;

  rowLabelColumn = NULL;
  rowLabel = NULL;
  floatFormat = doubleFormat = ldoubleFormat = stringFormat = NULL;
  precision = 0;
  columnDataType = parDataType = arrayDataType = NULL;
  columnName = parameterName = arrayName = NULL;
  columnMatch = parameterMatch = arrayMatch = NULL;
  columnMatches = parameterMatches = arrayMatches = 0;
  columns = parameters = arrays = 0;
  file1 = file2 = NULL;
  pages1 = pages2 = 0;
  column_provided = parameter_provided = array_provided = 0;
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s%s", USAGE1, USAGE2);
    exit(EXIT_FAILURE);
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_ROWLABEL:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -rowlabel syntax");
        rowLabelColumn = s_arg[i_arg].list[1];
        if (s_arg[i_arg].n_items > 2 &&
            strncmp_case_insensitive("nocomparison", s_arg[i_arg].list[2], strlen(s_arg[i_arg].list[2])) == 0)
          notCompareRowLabel = 1;
        break;
      case CLO_EXACT:
        tolerance = -1.0L;
        break;
      case CLO_ABSOLUTE:
        absolute = 1;
        break;
      case CLO_TOLERANCE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -tolerance syntax");
        if (!get_longdouble(&tolerance, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -tolerance syntax (not a number given)");
        break;
      case CLO_PRECISION:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -precision syntax");
        if (!get_long(&precision, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -precision syntax (not a number given)");
        if (precision < 0)
          precision = 0;
        break;
      case CLO_FORMAT:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -format syntax.");
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "float", SDDS_STRING, &floatFormat, 1, 0,
                          "double", SDDS_STRING, &doubleFormat, 1, 0,
                          "longdouble", SDDS_STRING, &ldoubleFormat, 1, 0,
                          "string", SDDS_STRING, &stringFormat, 1, 0, NULL))
          SDDS_Bomb("Invalid -format syntax");
        s_arg[i_arg].n_items++;
        if (floatFormat && !SDDS_VerifyPrintfFormat(floatFormat, SDDS_FLOAT)) {
          fprintf(stderr, "Error: Given print format (\"%s\") for float data is invalid.\n", floatFormat);
          exit(EXIT_FAILURE);
        }
        if (doubleFormat && !SDDS_VerifyPrintfFormat(doubleFormat, SDDS_DOUBLE)) {
          fprintf(stderr, "Error: Given print format (\"%s\") for double data is invalid.\n", doubleFormat);
          exit(EXIT_FAILURE);
        }
        if (ldoubleFormat && !SDDS_VerifyPrintfFormat(ldoubleFormat, SDDS_LONGDOUBLE)) {
          fprintf(stderr, "Error: Given print format (\"%s\") for long double data is invalid.\n", ldoubleFormat);
          exit(EXIT_FAILURE);
        }
        if (stringFormat && !SDDS_VerifyPrintfFormat(stringFormat, SDDS_STRING)) {
          fprintf(stderr, "Error: Given print format (\"%s\") for string data is invalid.\n", stringFormat);
          exit(EXIT_FAILURE);
        }
        break;
      case CLO_COMPARECOMMON:
        if (s_arg[i_arg].n_items == 1)
          compareCommonFlags |= COMPARE_COMMON_COLUMN | COMPARE_COMMON_PARAMETER | COMPARE_COMMON_ARRAY;
        else {
          s_arg[i_arg].n_items--;
          if (!scanItemList(&compareCommonFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                            "column", -1, NULL, 0, COMPARE_COMMON_COLUMN,
                            "parameter", -1, NULL, 0, COMPARE_COMMON_PARAMETER,
                            "array", -1, NULL, 0, COMPARE_COMMON_ARRAY, NULL))
            SDDS_Bomb("Invalid -compareCommon syntax");
          s_arg[i_arg].n_items++;
        }
        break;
      case CLO_COLUMNS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -columns syntax");
        columnMatch = tmalloc(sizeof(*columnMatch) * (columnMatches = s_arg[i_arg].n_items - 1));
        for (i = 0; i < columnMatches; i++)
          columnMatch[i] = s_arg[i_arg].list[i + 1];
        column_provided = 1;
        break;
      case CLO_PARAMETERS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -parameters syntax");
        parameterMatch = tmalloc(sizeof(*parameterMatch) * (parameterMatches = s_arg[i_arg].n_items - 1));
        for (i = 0; i < parameterMatches; i++)
          parameterMatch[i] = s_arg[i_arg].list[i + 1];
        parameter_provided = 1;
        break;
      case CLO_ARRAYS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -arrays syntax");
        arrayMatch = tmalloc(sizeof(*arrayMatch) * (arrayMatches = s_arg[i_arg].n_items - 1));
        for (i = 0; i < arrayMatches; i++)
          arrayMatch[i] = s_arg[i_arg].list[i + 1];
        array_provided = 1;
        break;
      case CLO_IGNORE_UNITS:
        ignoreUnits = 1;
        break;
      default:
        fprintf(stderr, "Unknown option given (sddsdiff): %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (!file1)
        file1 = s_arg[i_arg].list[0];
      else if (!file2)
        file2 = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many files given.");
    }
  }
  if (!floatFormat)
    SDDS_CopyString(&floatFormat, "%25.8e");
  if (!doubleFormat)
    SDDS_CopyString(&doubleFormat, "%25.16e");
  if (!ldoubleFormat)
    SDDS_CopyString(&ldoubleFormat, "%26.18Le");
  if (!stringFormat)
    SDDS_CopyString(&stringFormat, "%25s");

  if (tolerance && precision > 0) {
    SDDS_Bomb("Tolerance and precision options are not compatible. Only one of tolerance, precision, or exact may be given.");
  }
  if (!file1 || !file2) {
    fprintf(stderr, "Error: Two files must be provided for comparison.\n");
    exit(EXIT_FAILURE);
  }
  if (strcmp(file1, file2) == 0) {
    printf("\"%s\" and \"%s\" are identical.\n", file1, file2);
    return EXIT_SUCCESS;
  }
  if (!SDDS_InitializeInput(&table1, file1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!SDDS_InitializeInput(&table2, file2)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (rowLabelColumn) {
    if ((rowLabelIndex = SDDS_GetColumnIndex(&table1, rowLabelColumn)) < 0) {
      if ((rowLabelIndex = SDDS_GetColumnIndex(&table2, rowLabelColumn)) < 0) {
        fprintf(stdout, "Warning: Row label column \"%s\" does not exist in the input files. The number of rows will be labeled instead.\n", rowLabelColumn);
        rowLabelColumn = NULL;
      } else {
        labelFromSecondFile = 1;
        notCompareRowLabel = 1;
      }
    } else {
      if (SDDS_GetColumnIndex(&table2, rowLabelColumn) < 0)
        notCompareRowLabel = 1;
    }

    if (rowLabelColumn) {
      if (labelFromSecondFile)
        rowLabelType = SDDS_GetColumnType(&table2, rowLabelIndex);
      else
        rowLabelType = SDDS_GetColumnType(&table1, rowLabelIndex);
    }
  }
  if (!precision) {
    precisionTolerance = powl(10L, -1L * fabsl(log10l(LDBL_EPSILON)));
  } else {
    precisionTolerance = powl(10L, -1L * precision);
  }
  if (column_provided) {
    columnName = getMatchingSDDSNames(&table1, columnMatch, columnMatches, &columns, SDDS_MATCH_COLUMN);
    if (CompareDefinitions(&table1, &table2, file1, file2, &columns, &columnName, &columnDataType, SDDS_COLUMN_TYPE, compareCommonFlags & COMPARE_COMMON_COLUMN, rowLabelColumn, notCompareRowLabel, ignoreUnits))
      different = 1;
  }
  if (parameter_provided) {
    parameterName = getMatchingSDDSNames(&table1, parameterMatch, parameterMatches, &parameters, SDDS_MATCH_PARAMETER);

    if (CompareDefinitions(&table1, &table2, file1, file2, &parameters, &parameterName, &parDataType, SDDS_PARAMETER_TYPE, compareCommonFlags & COMPARE_COMMON_PARAMETER, NULL, 1, ignoreUnits))
      different = 1;
  }
  if (array_provided) {
    arrayName = getMatchingSDDSNames(&table1, arrayMatch, arrayMatches, &arrays, SDDS_MATCH_ARRAY);
    if (CompareDefinitions(&table1, &table2, file1, file2, &arrays, &arrayName, &arrayDataType, SDDS_ARRAY_TYPE, compareCommonFlags & COMPARE_COMMON_ARRAY, NULL, 1, ignoreUnits))
      different = 1;
  }
  if (!columns && !parameters && !arrays) {
    if (!compareCommonFlags || compareCommonFlags & COMPARE_COMMON_COLUMN)
      different += CompareDefinitions(&table1, &table2, file1, file2, &columns, &columnName, &columnDataType, SDDS_COLUMN_TYPE, compareCommonFlags & COMPARE_COMMON_COLUMN, rowLabelColumn, notCompareRowLabel, ignoreUnits);
    if (!compareCommonFlags || compareCommonFlags & COMPARE_COMMON_PARAMETER)
      different += CompareDefinitions(&table1, &table2, file1, file2, &parameters, &parameterName, &parDataType, SDDS_PARAMETER_TYPE, compareCommonFlags & COMPARE_COMMON_PARAMETER, NULL, 1, ignoreUnits);
    if (!compareCommonFlags || compareCommonFlags & COMPARE_COMMON_ARRAY)
      different += CompareDefinitions(&table1, &table2, file1, file2, &arrays, &arrayName, &arrayDataType, SDDS_ARRAY_TYPE, compareCommonFlags & COMPARE_COMMON_ARRAY, NULL, 1, ignoreUnits);
  }
  if (!different) {
    if (!columns && !parameters && !arrays) {
      fprintf(stderr, "There are no common columns, parameters, or arrays in the two files.\n");
      different = 1;
    } else {
      /* Definitions are the same, now compare the data */
      while (1) {
        pagediff = 0;
        pages1 = SDDS_ReadPage(&table1);
        pages2 = SDDS_ReadPage(&table2);
        if (pages1 > 0 && pages2 > 0) {
          /* Compare data */
          rows1 = SDDS_CountRowsOfInterest(&table1);
          rows2 = SDDS_CountRowsOfInterest(&table2);
          if (rows1 != rows2) {
            pagediff = 1;
            different = 1;
            fprintf(stderr, "The two files have different numbers of rows on page %ld: \"%s\" has %" PRId64 " rows, while \"%s\" has %" PRId64 " rows.\n",
                    pages1, file1, rows1, file2, rows2);
            break;
          } else {
            if (parameters)
              pagediff += CompareData(&table1, &table2, file1, file2, parameters, parameterName, parDataType, SDDS_PARAMETER_TYPE, pages1, tolerance, precisionTolerance, floatFormat, doubleFormat, ldoubleFormat, stringFormat, absolute, NULL, rowLabelType, NULL);
            if (columns && rows1) {
              if (rowLabelColumn) {
                if (labelFromSecondFile) {
                  if (!(rowLabel = SDDS_GetColumn(&table2, rowLabelColumn)))
                    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                } else {
                  if (!(rowLabel = SDDS_GetColumn(&table1, rowLabelColumn)))
                    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              }
              pagediff += CompareData(&table1, &table2, file1, file2, columns, columnName, columnDataType, SDDS_COLUMN_TYPE, pages1, tolerance, precisionTolerance, floatFormat, doubleFormat, ldoubleFormat, stringFormat, absolute, rowLabel, rowLabelType, rowLabelColumn);
              if (rowLabelColumn) {
                if (rowLabelType == SDDS_STRING)
                  SDDS_FreeStringArray((char **)rowLabel, rows1);
                else
                  free(rowLabel);
                rowLabel = NULL;
              }
            }
            if (arrays)
              pagediff += CompareData(&table1, &table2, file1, file2, arrays, arrayName, arrayDataType, SDDS_ARRAY_TYPE, pages1, tolerance, precisionTolerance, floatFormat, doubleFormat, ldoubleFormat, stringFormat, absolute, NULL, rowLabelType, NULL);
            different += pagediff;
          }
        } else if (pages1 > 0 && pages2 <= 0) {
          fprintf(stderr, "\"%s\" has fewer pages than \"%s\".\n", file2, file1);
          different = 1;
          break;
        } else if (pages1 < 0 && pages2 > 0) {
          different = 1;
          fprintf(stderr, "\"%s\" has fewer pages than \"%s\".\n", file1, file2);
          break;
        } else {
          break;
        }
      }
    }
  } else {
    different = 1;
  }
  if (!different)
    printf("\"%s\" and \"%s\" are identical.\n", file1, file2);
  else
    fprintf(stderr, "\"%s\" and \"%s\" are different.\n", file1, file2);

  if (columns) {
    for (i = 0; i < columns; i++)
      free(columnName[i]);
    free(columnName);
    free(columnDataType);
    free(columnMatch);
  }
  if (parameters) {
    for (i = 0; i < parameters; i++)
      free(parameterName[i]);
    free(parameterName);
    free(parDataType);
    free(parameterMatch);
  }
  if (arrays) {
    for (i = 0; i < arrays; i++)
      free(arrayName[i]);
    free(arrayName);
    free(arrayDataType);
    free(arrayMatch);
  }
  free_scanargs(&s_arg, argc);
  free(stringFormat);
  free(floatFormat);
  free(doubleFormat);
  free(ldoubleFormat);

  if (!SDDS_Terminate(&table1) || !SDDS_Terminate(&table2)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}

long CompareDefinitions(SDDS_DATASET *dataset1, SDDS_DATASET *dataset2, char *file1, char *file2, int32_t *names, char ***name, int32_t **dataType, long type, long compareCommon, char *rowLabelColumn, long notCompareRowLabel, short ignoreUnits) {
  size_t same_items;
  long sames, free_same_name = 0;
  int32_t *same, *datatype;
  int32_t names1, names2;
  char **name1, **name2, *def, **same_name;
  long type1, type2, i, returnValue = 0, first = 1;
  int32_t index1, index2;
  char *units1, *units2;
  units1 = units2 = NULL;
  name1 = name2 = same_name = NULL;
  names1 = names2 = 0;
  def = NULL;
  same_items = 0;
  same = datatype = NULL;

  type1 = type2 = -1;

  switch (type) {
  case SDDS_COLUMN_TYPE:
    name1 = SDDS_GetColumnNames(dataset1, &names1);
    name2 = SDDS_GetColumnNames(dataset2, &names2);
    SDDS_CopyString(&def, "column");
    break;
  case SDDS_PARAMETER_TYPE:
    name1 = SDDS_GetParameterNames(dataset1, &names1);
    name2 = SDDS_GetParameterNames(dataset2, &names2);
    SDDS_CopyString(&def, "parameter");
    break;
  case SDDS_ARRAY_TYPE:
    name1 = SDDS_GetArrayNames(dataset1, &names1);
    name2 = SDDS_GetArrayNames(dataset2, &names2);
    SDDS_CopyString(&def, "array");
    break;
  default:
    fprintf(stderr, "Unknown type given for CompareDefinitions().\n");
    return 1;
  }
  if (names1 == 0 && names2 == 0)
    return 0;
  if (*names) {
    if (!names1 || !names2) {
      fprintf(stderr, "Error: One of the files does not have any %s.\n", def);
      returnValue = 1;
    }
    if (!returnValue) {
      for (i = 0; i < *names; i++) {
        if (rowLabelColumn && notCompareRowLabel && strcmp((*name)[i], rowLabelColumn) == 0)
          continue;
        if (-1 == match_string((*name)[i], name1, names1, EXACT_MATCH)) {
          fprintf(stderr, "Error: File \"%s\" does not have %s \"%s\".\n", file1, def, (*name)[i]);
          returnValue = 1;
          break;
        }
        if (-1 == match_string((*name)[i], name2, names2, EXACT_MATCH)) {
          fprintf(stderr, "Error: File \"%s\" does not have %s \"%s\".\n", file2, def, (*name)[i]);
          returnValue = 1;
          break;
        }
      }
    }
    if (returnValue) {
      for (i = 0; i < names1; i++)
        free(name1[i]);
      free(name1);
      for (i = 0; i < names2; i++)
        free(name2[i]);
      free(name2);
      free(def);
      return returnValue;
    }
    same_items = *names;
    same_name = *name;
    compareCommon = 0;
  } else {
    if (!names1 && !names2) {
      free(def);
      return 0;
    }
    if (compareCommon && (!names1 || !names2)) {
      if (names1) {
        for (i = 0; i < names1; i++)
          free(name1[i]);
        free(name1);
      }
      if (names2) {
        for (i = 0; i < names2; i++)
          free(name2[i]);
        free(name2);
      }
      *names = 0;
      return 0;
    }
    if (names1 != names2 && !compareCommon && !notCompareRowLabel)
      returnValue = 1;
    if (returnValue) {
      fprintf(stderr, "Error: Two files have different numbers of %ss:\n    \"%s\" has %" PRId32 " %ss while \"%s\" has %" PRId32 " %ss.\n", def, file1, names1, def, file2, names2, def);
      for (i = 0; i < names2; i++)
        free(name2[i]);
      free(name2);
      for (i = 0; i < names1; i++)
        free(name1[i]);
      free(name1);
      free(def);
      return returnValue;
    }
    same_items = 0;
    for (i = 0; i < names1; i++) {
      if (rowLabelColumn && notCompareRowLabel && strcmp(rowLabelColumn, name1[i]) == 0)
        continue;
      if (-1 == match_string(name1[i], name2, names2, EXACT_MATCH)) {
        if (!compareCommon) {
          if (first) {
            fprintf(stderr, "    Following %ss of \"%s\" are not in \"%s\":\n", def, file1, file2);
            first = 0;
          }
          fprintf(stderr, "      %s\n", name1[i]);
          returnValue++;
        }
      } else {
        same_name = (char **)SDDS_Realloc(same_name, sizeof(*same_name) * (same_items + 1));
        same_name[same_items] = name1[i];
        same_items++;
      }
    }
    if (!compareCommon) {
      if (!first)
        fprintf(stderr, "\n");
      first = 1;
      for (i = 0; i < names2; i++) {
        if (rowLabelColumn && notCompareRowLabel && strcmp(rowLabelColumn, name2[i]) == 0)
          continue;
        if (-1 == match_string(name2[i], name1, names1, EXACT_MATCH)) {
          if (first) {
            fprintf(stderr, "    Following %ss of \"%s\" are not in \"%s\":\n", def, file2, file1);
            first = 0;
          }
          fprintf(stderr, "      %s\n", name2[i]);
          returnValue++;
        }
      }
      if (!first)
        fprintf(stderr, "\n");
    }
    if (same_items)
      free_same_name = 1;
  }
  sames = same_items;
  if (same_items) {
    datatype = (int32_t *)malloc(sizeof(*datatype) * same_items);
    same = (int32_t *)malloc(sizeof(*same) * same_items);
    /* Check the units and type */
    for (i = 0; i < same_items; i++) {
      same[i] = 1;
      switch (type) {
      case SDDS_COLUMN_TYPE:
        index1 = SDDS_GetColumnIndex(dataset1, same_name[i]);
        index2 = SDDS_GetColumnIndex(dataset2, same_name[i]);
        type1 = SDDS_GetColumnType(dataset1, index1);
        type2 = SDDS_GetColumnType(dataset2, index2);
        if (!ignoreUnits) {
          if (SDDS_GetColumnInformation(dataset1, "units", &units1, SDDS_GET_BY_INDEX, index1) != SDDS_STRING) {
            SDDS_SetError("Units field of column has wrong data type!");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          if (SDDS_GetColumnInformation(dataset2, "units", &units2, SDDS_GET_BY_INDEX, index2) != SDDS_STRING) {
            SDDS_SetError("Units field of column has wrong data type!");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        }
        break;
      case SDDS_PARAMETER_TYPE:
        index1 = SDDS_GetParameterIndex(dataset1, same_name[i]);
        index2 = SDDS_GetParameterIndex(dataset2, same_name[i]);
        type1 = SDDS_GetParameterType(dataset1, index1);
        type2 = SDDS_GetParameterType(dataset2, index2);
        if (!ignoreUnits) {
          if (SDDS_GetParameterInformation(dataset1, "units", &units1, SDDS_GET_BY_INDEX, index1) != SDDS_STRING) {
            SDDS_SetError("Units field of parameter has wrong data type!");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          if (SDDS_GetParameterInformation(dataset2, "units", &units2, SDDS_GET_BY_INDEX, index2) != SDDS_STRING) {
            SDDS_SetError("Units field of parameter has wrong data type!");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        }
        break;
      case SDDS_ARRAY_TYPE:
        index1 = SDDS_GetArrayIndex(dataset1, same_name[i]);
        index2 = SDDS_GetArrayIndex(dataset2, same_name[i]);
        type1 = SDDS_GetArrayType(dataset1, index1);
        type2 = SDDS_GetArrayType(dataset2, index2);
        if (!ignoreUnits) {
          if (SDDS_GetArrayInformation(dataset1, "units", &units1, SDDS_GET_BY_INDEX, index1) != SDDS_STRING) {
            SDDS_SetError("Units field of array has wrong data type!");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          if (SDDS_GetArrayInformation(dataset2, "units", &units2, SDDS_GET_BY_INDEX, index2) != SDDS_STRING) {
            SDDS_SetError("Units field of array has wrong data type!");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        }
        break;
      }
      datatype[i] = type1;
      if (type1 != type2) {
        if (first && !compareCommon) {
          fprintf(stderr, "The type of the following %ss do not match in the two files:\n", def);
          fprintf(stderr, "%20s\t%20s\t%20s\n", "Name", file1, file2);
          first = 0;
        }
        if (!compareCommon) {
          fprintf(stderr, "%20s\t%20s\t%20s\n", same_name[i], SDDS_type_name[type1 - 1], SDDS_type_name[type2 - 1]);
          returnValue++;
        }
        sames--;
        same[i] = 0;
      } else if ((units1 && units2 && strcasecmp(units1, units2) != 0) || (units1 && !units2) || (!units1 && units2)) {
        if (first && !compareCommon) {
          fprintf(stderr, "The units of the following %ss do not match in the two files:\n", def);
          fprintf(stderr, "%20s\t%20s\t%20s\n", "Name", file1, file2);
          first = 0;
        }
        if (!compareCommon) {
          if (units1 && units2)
            fprintf(stderr, "%20s\t%20s\t%20s\n", same_name[i], units1, units2);
          else if (units1)
            fprintf(stderr, "%20s\t%20s\t%20s\n", same_name[i], units1, "   ");
          else if (units2)
            fprintf(stderr, "%20s\t%20s\t%20s\n", same_name[i], "   ", units2);
          returnValue++;
        }
        sames--;
        same[i] = 0;
      }
      free(units1);
      free(units2);
      units1 = units2 = NULL;
      if (returnValue && !compareCommon)
        break;
    }
    if (!compareCommon && returnValue) {
      sames = 0;
    }
  }
  if (sames) {
    if (!(*names)) {
      for (i = 0; i < same_items; i++) {
        if (same[i]) {
          *name = (char **)SDDS_Realloc(*name, sizeof(**name) * (*names + 1));
          *dataType = (int32_t *)SDDS_Realloc(*dataType, sizeof(**dataType) * (*names + 1));
          SDDS_CopyString(*name + *names, same_name[i]);
          (*dataType)[*names] = datatype[i];
          (*names)++;
        }
      }
      free(datatype);
    } else
      *dataType = datatype;
  }

  for (i = 0; i < names2; i++)
    free(name2[i]);
  free(name2);
  for (i = 0; i < names1; i++)
    free(name1[i]);
  free(name1);
  if (same)
    free(same);
  if (free_same_name)
    free(same_name);
  free(def);
  return compareCommon ? 0 : returnValue;
}

long CompareData(SDDS_DATASET *dataset1, SDDS_DATASET *dataset2, char *file1, char *file2,
                 long names, char **name, int32_t *dataType, long type, long page, long double tolerance,
                 long double precisionTolerance, char *floatFormat, char *doubleFormat, char *ldoubleFormat,
                 char *stringFormat, long absolute, void *rowLabel, long rowLabelType, char *rowLabelColumn) {
  long diff = 0, i, first = 1;
  int64_t rows, j;
  char fFormat[2048], dFormat[2048], ldFormat[2048], strFormat[2048], lFormat[2048], ulFormat[2048], ushortFormat[2048], shortFormat[2048], cFormat[2048], labelFormat[1024];
  SDDS_ARRAY *array1, *array2;
  void *data1, *data2;

  array1 = array2 = NULL;
  data1 = data2 = NULL;

  if (!rowLabel) {
    snprintf(fFormat, sizeof(fFormat), "%%20ld%s%s%s\n", floatFormat, floatFormat, floatFormat);
    snprintf(dFormat, sizeof(dFormat), "%%20ld%s%s%s\n", doubleFormat, doubleFormat, doubleFormat);
    snprintf(ldFormat, sizeof(ldFormat), "%%20ld%s%s%s\n", ldoubleFormat, ldoubleFormat, ldoubleFormat);
    snprintf(strFormat, sizeof(strFormat), "%%20ld%s%s%%25ld\n", stringFormat, stringFormat);
    snprintf(lFormat, sizeof(lFormat), "%s", "%20ld%25ld%25ld\n");
    snprintf(ulFormat, sizeof(ulFormat), "%s", "%20ld%25lu%25lu%25ld\n");
    snprintf(shortFormat, sizeof(shortFormat), "%s", "%20ld%25hd%25hd%25hd\n");
    snprintf(ushortFormat, sizeof(ushortFormat), "%s", "%20ld%25hu%25hu%25hd\n");
    snprintf(cFormat, sizeof(cFormat), "%s", "%20ld%25c%25c%25d\n");
  }
  switch (type) {
  case SDDS_COLUMN_TYPE:
    rows = SDDS_CountRowsOfInterest(dataset1);
    if (!rows)
      break;
    for (i = 0; i < names; i++) {
      first = 1;
      if (!(data1 = SDDS_GetColumn(dataset1, name[i])) || !(data2 = SDDS_GetColumn(dataset2, name[i]))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < rows; j++) {
        if (rowLabel) {
          switch (rowLabelType) {
          case SDDS_STRING:
            snprintf(labelFormat, sizeof(labelFormat), "%20s", ((char **)rowLabel)[j]);
            break;
          case SDDS_LONGDOUBLE:
            snprintf(labelFormat, sizeof(labelFormat), "%20.15Le", ((long double *)rowLabel)[j]);
            break;
          case SDDS_DOUBLE:
            snprintf(labelFormat, sizeof(labelFormat), "%20.10e", ((double *)rowLabel)[j]);
            break;
          case SDDS_FLOAT:
            snprintf(labelFormat, sizeof(labelFormat), "%20.5e", ((float *)rowLabel)[j]);
            break;
          case SDDS_ULONG64:
            snprintf(labelFormat, sizeof(labelFormat), "%20" PRIu64, ((uint64_t *)rowLabel)[j]);
            break;
          case SDDS_LONG64:
            snprintf(labelFormat, sizeof(labelFormat), "%20" PRId64, ((int64_t *)rowLabel)[j]);
            break;
          case SDDS_ULONG:
            snprintf(labelFormat, sizeof(labelFormat), "%20" PRIu32, ((uint32_t *)rowLabel)[j]);
            break;
          case SDDS_LONG:
            snprintf(labelFormat, sizeof(labelFormat), "%20" PRId32, ((int32_t *)rowLabel)[j]);
            break;
          case SDDS_USHORT:
            snprintf(labelFormat, sizeof(labelFormat), "%20hu", ((unsigned short *)rowLabel)[j]);
            break;
          case SDDS_SHORT:
            snprintf(labelFormat, sizeof(labelFormat), "%20hd", ((short *)rowLabel)[j]);
            break;
          case SDDS_CHARACTER:
            snprintf(labelFormat, sizeof(labelFormat), "%20c", ((char *)rowLabel)[j]);
            break;
          default:
            fprintf(stderr, "Unknown data type for rowlabel.\n");
            exit(EXIT_FAILURE);
          }
          snprintf(fFormat, sizeof(fFormat), "%s%s%s%s\n", labelFormat, floatFormat, floatFormat, floatFormat);
          snprintf(dFormat, sizeof(dFormat), "%s%s%s%s\n", labelFormat, doubleFormat, doubleFormat, doubleFormat);
          snprintf(ldFormat, sizeof(ldFormat), "%s%s%s%s\n", labelFormat, ldoubleFormat, ldoubleFormat, ldoubleFormat);
          snprintf(strFormat, sizeof(strFormat), "%s%s%s%%25ld\n", labelFormat, stringFormat, stringFormat);
          snprintf(lFormat, sizeof(lFormat), "%s%%25ld%%25ld%%25ld\n", labelFormat);
          snprintf(ulFormat, sizeof(ulFormat), "%s%%25lu%%25lu%%25ld\n", labelFormat);
          snprintf(shortFormat, sizeof(shortFormat), "%s%%25hd%%25hd%%25hd\n", labelFormat);
          snprintf(ushortFormat, sizeof(ushortFormat), "%s%%25hu%%25hu%%25hd\n", labelFormat);
          snprintf(cFormat, sizeof(cFormat), "%s%%25c%%25c%%25d\n", labelFormat);
        }
        if (compare_two_data(data1, data2, j, dataType[i], first, SDDS_COLUMN_TYPE, name[i], page, tolerance, precisionTolerance, fFormat, dFormat, ldFormat, strFormat, lFormat, ulFormat, shortFormat, ushortFormat, cFormat, absolute, 0, rowLabelColumn) != 0) {
          diff++;
          if (first)
            first = 0;
        }
        if (dataType[i] == SDDS_STRING) {
          free((char *)((char **)data1)[j]);
          free((char *)((char **)data2)[j]);
        }
      }
      free((char **)data1);
      free((char **)data2);
      data1 = data2 = NULL;
    }
    break;
  case SDDS_PARAMETER_TYPE:
    for (i = 0; i < names; i++) {
      first = 1;
      if (!(data1 = SDDS_GetParameter(dataset1, name[i], NULL)) || !(data2 = SDDS_GetParameter(dataset2, name[i], NULL))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      if (compare_two_data(data1, data2, 0, dataType[i], first, SDDS_PARAMETER_TYPE, name[i], page, tolerance, precisionTolerance, fFormat, dFormat, ldFormat, strFormat, lFormat, ulFormat, shortFormat, ushortFormat, cFormat, absolute, 1, NULL) != 0) {
        diff++;
        if (first)
          first = 0;
      }
      if (dataType[i] == SDDS_STRING) {
        free(*((char **)data1));
        free(*((char **)data2));
      }
      free(data1);
      free(data2);
      data1 = data2 = NULL;
    }
    break;
  case SDDS_ARRAY_TYPE:
    for (i = 0; i < names; i++) {
      first = 1;
      if (!(array1 = SDDS_GetArray(dataset1, name[i], NULL)) || !(array2 = SDDS_GetArray(dataset2, name[i], NULL))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      if (array1->elements != array2->elements) {
        fprintf(stderr, "Array \"%s\" has %" PRId32 " elements in \"%s\", but %" PRId32 " elements in \"%s\".\n", name[i], array1->elements, file1, array2->elements, file2);
        diff++;
      } else {
        for (j = 0; j < array1->elements; j++) {
          if (compare_two_data(array1->data, array2->data, j, dataType[i], first, SDDS_ARRAY_TYPE, name[i], page, tolerance, precisionTolerance, fFormat, dFormat, ldFormat, strFormat, lFormat, ulFormat, shortFormat, ushortFormat, cFormat, absolute, 0, NULL) != 0) {
            diff++;
            if (first)
              first = 0;
          }
        }
      }
      SDDS_FreeArray(array1);
      SDDS_FreeArray(array2);
      array1 = array2 = NULL;
    }
    break;
  }
  return diff;
}

void printTitle(long flags, char *name, long page, long absolute, char *labelName) {
  char *type = NULL;
  char *element = NULL;

  switch (flags) {
  case SDDS_COLUMN_TYPE:
    SDDS_CopyString(&type, "column");
    if (labelName)
      SDDS_CopyString(&element, labelName);
    else
      SDDS_CopyString(&element, "row");
    break;
  case SDDS_PARAMETER_TYPE:
    SDDS_CopyString(&type, "parameter");
    SDDS_CopyString(&element, "page number");
    break;
  case SDDS_ARRAY_TYPE:
    SDDS_CopyString(&type, "array");
    SDDS_CopyString(&element, "element number");
    break;
  }
  if (type) {
    fprintf(stdout, "\nDifferences found in %s \"%s\" on page %ld:\n", type, name, page);
    if (absolute)
      fprintf(stdout, "%20s%25s%25s%25s\n", element, "Value in file1", "Value in file2", "Difference (abs)");
    else
      fprintf(stdout, "%20s%25s%25s%25s\n", element, "Value in file1", "Value in file2", "Difference (file1 - file2)");
    free(type);
    free(element);
  }
}

long compare_two_data(void *data1, void *data2, long index, long datatype,
                      long first, long flags, char *name, long page,
                      long double tolerance, long double precisionTolerance,
                      char *floatFormat, char *doubleFormat, char *ldoubleFormat,
                      char *stringFormat, char *longFormat, char *ulongFormat,
                      char *shortFormat, char *ushortFormat, char *charFormat,
                      long absolute, long parameter, char *labelName) {
  char *str1, *str2;
  long double ldval1, ldval2, ldenominator, ldabs1, ldabs2, lddiff;
  double dval1, dval2, denominator, dabs1, dabs2, ddiff;
  float fval1, fval2, fabs1, fabs2, fdenominator, fdiff;
  int32_t lval1, lval2, labs1, labs2, ldiff, uldiff;
  uint32_t ulval1, ulval2;
  int64_t llval1, llval2, llabs1, llabs2, lldiff, ulldiff;
  uint64_t ullval1, ullval2;
  short sval1, sval2, sabs1, sabs2, sdiff, usdiff;
  unsigned short usval1, usval2;
  char cval1, cval2;
  long returnValue = 0, printIndex;
  long double tol;

  printIndex = index + 1;
  if (parameter)
    printIndex = page;

  if (tolerance < 0)
    tol = 0L;
  else
    tol = tolerance;

  switch (datatype) {
  case SDDS_STRING:
    str1 = *((char **)data1 + index);
    str2 = *((char **)data2 + index);
    returnValue = strcmp(trim_spaces(str1), trim_spaces(str2));
    if (returnValue != 0) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName) {
        fprintf(stdout, stringFormat, str1, str2, returnValue);
      } else
        fprintf(stdout, stringFormat, printIndex, str1, str2, returnValue);
    }
    break;
  case SDDS_LONGDOUBLE:
    ldval1 = *((long double *)data1 + index);
    ldval2 = *((long double *)data2 + index);
    if (absolute) {
      ldabs1 = fabsl(ldval1);
      ldabs2 = fabsl(ldval2);
    } else {
      ldabs1 = ldval1;
      ldabs2 = ldval2;
    }
    lddiff = ldabs1 - ldabs2;
    if ((isnan(ldval1) && !isnan(ldval2)) || (isinf(ldval1) && !isinf(ldval2)))
      returnValue = 1;
    else if (ldabs1 != ldabs2) {
      if (tolerance) {
        if (fabsl(lddiff) > tol)
          returnValue = 1;
      } else {
        if (ldabs1 == 0L || ldabs2 == 0L) {
          if (fabsl(ldabs1 - ldabs2) > precisionTolerance)
            returnValue = 1;
        } else {
          ldabs1 = fabsl(ldval1);
          ldabs2 = fabsl(ldval2);
          ldenominator = (ldabs1 < ldabs2) ? ldabs1 : ldabs2;
          if (fabsl(ldval1 - ldval2) / ldenominator > precisionTolerance)
            returnValue = 1;
        }
      }
    }
    if (returnValue) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, ldoubleFormat, ldval1, ldval2, lddiff);
      else
        fprintf(stdout, ldoubleFormat, printIndex, ldval1, ldval2, lddiff);
    }
    break;
  case SDDS_DOUBLE:
    dval1 = *((double *)data1 + index);
    dval2 = *((double *)data2 + index);
    if (absolute) {
      dabs1 = fabs(dval1);
      dabs2 = fabs(dval2);
    } else {
      dabs1 = dval1;
      dabs2 = dval2;
    }
    ddiff = dabs1 - dabs2;
    if ((isnan(dval1) && !isnan(dval2)) || (isinf(dval1) && !isinf(dval2)))
      returnValue = 1;
    else if (dabs1 != dabs2) {
      if (tolerance) {
        if (fabs(ddiff) > tol)
          returnValue = 1;
      } else {
        if (dabs1 == 0 || dabs2 == 0) {
          if (fabs(dabs1 - dabs2) > precisionTolerance)
            returnValue = 1;
        } else {
          dabs1 = fabs(dval1);
          dabs2 = fabs(dval2);
          denominator = (dabs1 < dabs2) ? dabs1 : dabs2;
          if (fabs(dval1 - dval2) / denominator > precisionTolerance)
            returnValue = 1;
        }
      }
    }
    if (returnValue) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, doubleFormat, dval1, dval2, ddiff);
      else
        fprintf(stdout, doubleFormat, printIndex, dval1, dval2, ddiff);
    }
    break;
  case SDDS_FLOAT:
    fval1 = *((float *)data1 + index);
    fval2 = *((float *)data2 + index);
    if (absolute) {
      fabs1 = fabs(fval1);
      fabs2 = fabs(fval2);
    } else {
      fabs1 = fval1;
      fabs2 = fval2;
    }
    fdiff = fabs1 - fabs2;
    if ((isnan(fval1) && !isnan(fval2)) || (isinf(fval1) && !isinf(fval2)))
      returnValue = 1;
    else if (fabs1 != fabs2) {
      if (tolerance) {
        if (fabs(fdiff) > tol)
          returnValue = 1;
      } else {
        if (fabs1 == 0 || fabs2 == 0) {
          if (fabs(fabs1 - fabs2) > precisionTolerance)
            returnValue = 1;
        } else {
          fabs1 = fabs(fval1);
          fabs2 = fabs(fval2);
          fdenominator = (fabs1 < fabs2) ? fabs1 : fabs2;
          if (fabs(fval1 - fval2) / fdenominator > precisionTolerance)
            returnValue = 1;
        }
      }
    }
    if (returnValue) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, floatFormat, fval1, fval2, fdiff);
      else
        fprintf(stdout, floatFormat, printIndex, fval1, fval2, fdiff);
    }
    break;
  case SDDS_ULONG64:
    ullval1 = *((uint64_t *)data1 + index);
    ullval2 = *((uint64_t *)data2 + index);
    ulldiff = ullval1 - ullval2;
    if (labs(ulldiff) > tol)
      returnValue = 1;
    if (returnValue) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, ulongFormat, ullval1, ullval2, ulldiff);
      else
        fprintf(stdout, ulongFormat, printIndex, ullval1, ullval2, ulldiff);
    }
    break;
  case SDDS_LONG64:
    llval1 = *((int64_t *)data1 + index);
    llval2 = *((int64_t *)data2 + index);
    if (absolute) {
      llabs1 = labs(llval1);
      llabs2 = labs(llval2);
    } else {
      llabs1 = llval1;
      llabs2 = llval2;
    }
    lldiff = llabs1 - llabs2;
    if (llabs(lldiff) > tol)
      returnValue = 1;
    if (returnValue) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, longFormat, llval1, llval2, lldiff);
      else
        fprintf(stdout, longFormat, printIndex, llval1, llval2, lldiff);
    }
    break;
  case SDDS_ULONG:
    ulval1 = *((uint32_t *)data1 + index);
    ulval2 = *((uint32_t *)data2 + index);
    uldiff = ulval1 - ulval2;
    if (labs(uldiff) > tol)
      returnValue = 1;
    if (returnValue) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, ulongFormat, ulval1, ulval2, uldiff);
      else
        fprintf(stdout, ulongFormat, printIndex, ulval1, ulval2, uldiff);
    }
    break;
  case SDDS_LONG:
    lval1 = *((int32_t *)data1 + index);
    lval2 = *((int32_t *)data2 + index);
    if (absolute) {
      labs1 = abs(lval1);
      labs2 = abs(lval2);
    } else {
      labs1 = lval1;
      labs2 = lval2;
    }
    ldiff = labs1 - labs2;
    if (labs(ldiff) > tol)
      returnValue = 1;
    if (returnValue) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, longFormat, lval1, lval2, ldiff);
      else
        fprintf(stdout, longFormat, printIndex, lval1, lval2, ldiff);
    }
    break;
  case SDDS_SHORT:
    sval1 = *((short *)data1 + index);
    sval2 = *((short *)data2 + index);
    if (absolute) {
      sabs1 = abs(sval1);
      sabs2 = abs(sval2);
    } else {
      sabs1 = sval1;
      sabs2 = sval2;
    }
    sdiff = sabs1 - sabs2;
    if (abs(sdiff) > tol)
      returnValue = 1;
    if (returnValue) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, shortFormat, sval1, sval2, sdiff);
      else
        fprintf(stdout, shortFormat, printIndex, sval1, sval2, sdiff);
    }
    break;
  case SDDS_USHORT:
    usval1 = *((unsigned short *)data1 + index);
    usval2 = *((unsigned short *)data2 + index);
    usdiff = usval1 - usval2;
    if (abs(usdiff) > tol)
      returnValue = 1;
    if (returnValue) {
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, ushortFormat, usval1, usval2, usdiff);
      else
        fprintf(stdout, ushortFormat, printIndex, usval1, usval2, usdiff);
    }
    break;
  case SDDS_CHARACTER:
    cval1 = *((char *)data1 + index);
    cval2 = *((char *)data2 + index);
    if (cval1 != cval2) {
      returnValue = 1;
      if (first)
        printTitle(flags, name, page, absolute, labelName);
      if (labelName)
        fprintf(stdout, charFormat, cval1, cval2, cval1 - cval2);
      else
        fprintf(stdout, charFormat, printIndex, cval1, cval2, cval1 - cval2);
    }
    break;
  default:
    fprintf(stderr, "Unknown data type %ld.\n", datatype);
    exit(EXIT_FAILURE);
  }
  return returnValue;
}
