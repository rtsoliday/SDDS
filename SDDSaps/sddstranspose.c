/**
 * @file sddstranspose.c
 * @brief Transposes numerical columns in an SDDS file.
 *
 * @details
 * This program reads an SDDS file with one or more pages, identifies all numerical columns,
 * and transposes them as though they were a matrix. If the file contains multiple pages,
 * additional pages are transposed only if they have the same number of rows as the first page.
 * String columns are converted to string parameters, and string parameters listed in the
 * "OldStringColumnNames" parameter become string columns in the output. If any string
 * columns exist, the data in the first string column is used as the names of the columns
 * in the output file. If no string columns exist, column names are generated from command
 * line options. The names of the input file columns are saved as a string column in the
 * output file with a name specified via the command line or the default "OldColumnName".
 *
 * @section Usage
 * ```
 * sddstranspose [<inputfile>] [<outputfile>]
 *               [-pipe=[input][,output]]
 *               [-oldColumnNames=<string>] 
 *               [-root=<string>]
 *               [-digits=<integer>]
 *               [-newColumnNames=<column>}] 
 *               [-matchColumn=<string>[,<string>,...]]
 *               [-indexColumn] 
 *               [-noOldColumnNames] 
 *               [-symbol=<string>] 
 *               [-ascii] 
 *               [-verbose] 
 *               [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Reads input from and/or writes output to a pipe.                                      |
 * | `-oldColumnNames`                     | Specifies the name for the output file string column created for the input column names. |
 * | `-root`                               | Uses the specified string to generate column names of the output file.                |
 * | `-digits`                             | Sets the minimum number of digits appended to column names. Default is 3.             |
 * | `-newColumnNames`                     | Uses the specified column as the source for new column names.                         |
 * | `-matchColumn`                        | Only transposes columns matching the specified names.                                 |
 * | `-indexColumn`                        | Adds an index column to the output file.                                              |
 * | `-noOldColumnNames`                   | Does not create a column for old column names.                                        |
 * | `-symbol`                             | Specifies the symbol field for column definitions.                                    |
 * | `-ascii`                              | Outputs the file in ASCII format. Default is binary.                                  |
 * | `-verbose`                            | Prints incidental information to stderr.                                              |
 * | `-majorOrder`                         | Specifies the output file's major order (row-major or column-major).                  |
 *
 * @subsection Incompatibilities
 * - `-root` is incompatible with:
 *   - `-newColumnNames`
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
 * M. Borland, C. Saunders, L. Emery, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"
#include "SDDSutils.h"

/* Enumeration for option types */
enum option_type {
  CLO_VERBOSE,
  CLO_COLUMNROOT,
  CLO_SYMBOL,
  CLO_ASCII,
  CLO_PIPE,
  CLO_OLDCOLUMNNAMES,
  CLO_NEWCOLUMNNAMES,
  CLO_DIGITS,
  CLO_MATCH_COLUMN,
  CLO_INDEX_COLUMN,
  CLO_NO_OLDCOLUMNNAMES,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

char *commandline_option[N_OPTIONS] = {
  "verbose",
  "root",
  "symbol",
  "ascii",
  "pipe",
  "oldColumnNames",
  "newColumnNames",
  "digits",
  "matchColumn",
  "indexColumn",
  "noOldColumnNames",
  "majorOrder",
};

static char *OLD_STRING_COLUMN_NAMES = "OldStringColumnNames";

static char *USAGE =
  "sddstranspose [<inputfile>] [<outputfile>]\n"
  "              [-pipe=[input][,output]]\n"
  "              [-oldColumnNames=<string>] \n"
  "              [-root=<string>]\n"
  "              [-digits=<integer>]\n"
  "              [-newColumnNames=<column>}] \n"
  "              [-matchColumn=<string>[,<string>,...]]\n"
  "              [-indexColumn] \n"
  "              [-noOldColumnNames] \n"
  "              [-symbol=<string>] \n"
  "              [-ascii] \n"
  "              [-verbose] \n"
  "              [-majorOrder=row|column]\n"
  "Options:\n"
  "  -pipe=[input][,output]          Reads input from and/or writes output to a pipe.\n"
  "  -oldColumnNames=<string>        Specifies the name for the output file string column created for the input file column names.\n"
  "  -root=<string>                  Uses the specified string to generate column names of the output file.\n"
  "                                  Default column names are the first string column in <inputfile>.\n"
  "                                  If no string column exists, column names are formed with the root \"Column\".\n"
  "  -digits=<integer>               Sets the minimum number of digits appended to the root part of the column names.\n"
  "                                  Default is 3.\n"
  "  -newColumnNames=<column>        Uses the specified column as the source for new column names.\n"
  "  -symbol=<string>                Uses the specified string for the symbol field in all column definitions.\n"
  "  -ascii                          Outputs the file in ASCII format. Default is binary.\n"
  "  -matchColumn=<string>[,<string>,...]  Only transposes the columns that match the specified names.\n"
  "  -indexColumn                    Adds an index column to the output file.\n"
  "  -noOldColumnNames               Does not create a new column for old column names.\n"
  "  -majorOrder=row|column          Specifies the output file's major order (row-major or column-major).\n"
  "  -verbose                        Prints incidental information to stderr.\n\n"
  "Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

char **TokenizeString(char *source, long n_items);
char *JoinStrings(char **source, long n_items, long buflen_increment);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET inputPage, outputPage;

  char *inputfile, *outputfile;
  char **inputColumnName, **inputStringColumnName, **inputDoubleColumnName;
  char **outputStringColumnName, **outputDoubleColumnName, **matchColumn = NULL;
  long inputDoubleColumns, inputStringColumns, indexColumn = 0, noOldColumnNamesColumn = 0;
  int64_t inputRows;
  int32_t matchColumns = 0;
  long outputRows, outputDoubleColumns, outputStringColumns;
  char **inputParameterName;
  int32_t inputParameters;
  int32_t inputColumns;
  char *inputDescription, *inputContents;
  char *outputDescription;
  long i, i_arg, col;
  char *buffer;
  char **columnOfStrings;
  long buffer_size;
#define BUFFER_SIZE_INCREMENT 16384
  MATRIX *R, *RInv;
  long OldStringColumnsDefined;
  char *inputStringRows, *outputStringRows;
  char **stringArray, *stringParameter;
  long token_length;
  long verbose;
  char format[32];
  long digits;
  char *Symbol, *Root;
  void *parameterPointer;
  long ascii;
  unsigned long pipeFlags, majorOrderFlag;
  long tmpfile_used, noWarnings;
  long ipage = 0, columnType;
  char *oldColumnNames, *newColumnNamesColumn;
  short columnMajorOrder = -1;

  inputColumnName = outputStringColumnName = outputDoubleColumnName = inputParameterName = NULL;
  outputRows = outputDoubleColumns = outputStringColumns = OldStringColumnsDefined = 0;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  inputfile = outputfile = NULL;
  verbose = 0;
  Symbol = Root = NULL;
  ascii = 0;
  digits = 3;
  pipeFlags = 0;
  tmpfile_used = 0;
  noWarnings = 0;
  oldColumnNames = NULL;
  newColumnNamesColumn = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], commandline_option, N_OPTIONS, UNIQUE_MATCH)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_MATCH_COLUMN:
        matchColumns = s_arg[i_arg].n_items - 1;
        matchColumn = s_arg[i_arg].list + 1;
        break;
      case CLO_INDEX_COLUMN:
        indexColumn = 1;
        break;
      case CLO_NO_OLDCOLUMNNAMES:
        noOldColumnNamesColumn = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_ASCII:
        ascii = 1;
        break;
      case CLO_DIGITS:
        if (!(get_long(&digits, s_arg[i_arg].list[1])))
          bomb("No integer provided for option -digits", USAGE);
        break;
      case CLO_COLUMNROOT:
        if (!(Root = s_arg[i_arg].list[1]))
          SDDS_Bomb("No root string provided with -root option");
        break;
      case CLO_SYMBOL:
        if (!(Symbol = s_arg[i_arg].list[1]))
          SDDS_Bomb("No symbol string provided with -symbol option");
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;
      case CLO_OLDCOLUMNNAMES:
        if (!(oldColumnNames = s_arg[i_arg].list[1]))
          SDDS_Bomb("No string provided for -oldColumnNames option");
        break;
      case CLO_NEWCOLUMNNAMES:
        if (s_arg[i_arg].n_items != 2 || SDDS_StringIsBlank(newColumnNamesColumn = s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid syntax or value for -newColumnNames option");
        break;
      default:
        bomb("Unrecognized option provided", USAGE);
      }
    } else {
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        bomb("Too many filenames provided", USAGE);
    }
  }

  processFilenames("sddstranspose", &inputfile, &outputfile, pipeFlags, noWarnings, &tmpfile_used);
  if (newColumnNamesColumn && Root)
    SDDS_Bomb("-root and -newColumnNames options are incompatible");

  if (!SDDS_InitializeInput(&inputPage, inputfile) ||
      !(inputParameterName = (char **)SDDS_GetParameterNames(&inputPage, &inputParameters)) ||
      !SDDS_GetDescription(&inputPage, &inputDescription, &inputContents))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (matchColumns)
    inputColumnName = getMatchingSDDSNames(&inputPage, matchColumn, matchColumns, &inputColumns, SDDS_MATCH_COLUMN);
  else {
    if (!(inputColumnName = (char **)SDDS_GetColumnNames(&inputPage, &inputColumns)))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  inputDoubleColumns = 0;
  inputStringColumns = 0;
  inputDoubleColumnName = (char **)malloc(inputColumns * sizeof(char *));
  inputStringColumnName = (char **)malloc(inputColumns * sizeof(char *));
  inputRows = 0;

  /***********
   * Read data
   ***********/
  while (0 < SDDS_ReadTable(&inputPage)) {
    ipage++;
#if defined(DEBUG)
    fprintf(stderr, "Working on page %ld\n", ipage);
#endif
    if (ipage == 1) {
      if (!SDDS_SetColumnFlags(&inputPage, 0))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

      /* Count the string and numerical columns in the input file */
      for (i = 0; i < inputColumns; i++) {
        if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&inputPage, i))) {
          inputDoubleColumnName[inputDoubleColumns] = inputColumnName[i];
          inputDoubleColumns++;
        }
      }
      for (i = 0; i < inputPage.layout.n_columns; i++) {
        if (inputPage.layout.column_definition[i].type == SDDS_STRING) {
          inputStringColumnName[inputStringColumns] = inputPage.layout.column_definition[i].name;
          inputStringColumns++;
        }
      }
      if (!(inputRows = SDDS_CountRowsOfInterest(&inputPage)))
        SDDS_Bomb("No rows in dataset.");
    } else {
      /* These statements are executed on subsequent pages */
      if (inputRows != SDDS_CountRowsOfInterest(&inputPage)) {
        SDDS_Bomb("Datasets have differing number of rows. Processing stopped before reaching end of input file.");
      }
    }

    if (inputRows > INT32_MAX) {
      SDDS_Bomb("Too many rows in dataset.");
    }

#if defined(DEBUG)
    fprintf(stderr, "Row flags set\n");
#endif
    if (inputDoubleColumns == 0)
      SDDS_Bomb("No numerical columns in file.");

    if ((ipage == 1) && verbose) {
      fprintf(stderr, "Number of numerical columns: %ld.\n", inputDoubleColumns);
      fprintf(stderr, "Number of string columns: %ld.\n", inputStringColumns);
      fprintf(stderr, "Number of rows: %" PRId64 ".\n", inputRows);
    }

    /****************
     * Transpose data
     ****************/
    if (inputDoubleColumns) {
      if (ipage == 1) {
        m_alloc(&RInv, inputRows, inputDoubleColumns);
        m_alloc(&R, inputDoubleColumns, inputRows);
      }
      for (col = 0; col < inputDoubleColumns; col++) {
        if (!(R->a[col] = (double *)SDDS_GetColumnInDoubles(&inputPage, inputDoubleColumnName[col]))) {
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      if (verbose) {
        m_show(R, "%9.6le ", "Transpose of input matrix:\n", stdout);
      }
      m_trans(RInv, R);
    }

    /***************************
     * Determine existence of  
     * transposed string columns 
     ***************************/
    if (ipage == 1) {
      OldStringColumnsDefined = 0;
      switch (SDDS_CheckParameter(&inputPage, OLD_STRING_COLUMN_NAMES, NULL, SDDS_STRING, NULL)) {
      case SDDS_CHECK_OKAY:
        OldStringColumnsDefined = 1;
        break;
      case SDDS_CHECK_NONEXISTENT:
        break;
      case SDDS_CHECK_WRONGTYPE:
      case SDDS_CHECK_WRONGUNITS:
        fprintf(stderr, "Error: Parameter OldStringColumns has incorrect type or units.\n");
        exit(EXIT_FAILURE);
        break;
      }

      if (OldStringColumnsDefined) {
        /* Decompose OldStringColumns into names of string columns for the output file */
        if (!SDDS_GetParameter(&inputPage, OLD_STRING_COLUMN_NAMES, &inputStringRows))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

        if (verbose) {
          fprintf(stderr, "Parameter OldStringColumns: %s.\n", inputStringRows);
        }

        outputStringColumnName = (char **)malloc(sizeof(char *));
        outputStringColumns = 0;
        buffer_size = BUFFER_SIZE_INCREMENT;
        buffer = (char *)malloc(sizeof(char) * buffer_size);
        while (0 <= (token_length = SDDS_GetToken(inputStringRows, buffer, BUFFER_SIZE_INCREMENT))) {
          if (!token_length)
            SDDS_Bomb("A null string was detected in parameter OldStringColumns.");

          if (!SDDS_CopyString(&outputStringColumnName[outputStringColumns], buffer))
            SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

          if (verbose) {
            fprintf(stderr, "Output string column: %s\n", outputStringColumnName[outputStringColumns]);
          }
          outputStringColumns++;
        }
      }
    }

    /*********************
     * Define output page 
     *********************/
    if (ipage == 1) {
      outputRows = inputDoubleColumns;
      outputDoubleColumns = inputRows;

      if (inputDescription) {
        outputDescription = (char *)malloc(sizeof(char) * (strlen("Transpose of ") + strlen(inputDescription) + 1));
        strcpy(outputDescription, "Transpose of ");
        strcat(outputDescription, inputDescription);
        if (!SDDS_InitializeOutput(&outputPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, outputDescription, inputContents, outputfile))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        free(outputDescription);
      } else {
        if (!SDDS_InitializeOutput(&outputPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, NULL, NULL, outputfile))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      SDDS_DeferSavingLayout(&outputPage, 1);

      if (columnMajorOrder != -1)
        outputPage.layout.data_mode.column_major = columnMajorOrder;
      else
        outputPage.layout.data_mode.column_major = inputPage.layout.data_mode.column_major;

      /***********************************
       * Define names for numerical columns 
       ***********************************/
      if (!Root && inputStringColumns) {
        /* Use specified string column, or first string column encountered */
        if (!newColumnNamesColumn)
          /* First string column encountered */
          outputDoubleColumnName = (char **)SDDS_GetColumn(&inputPage, inputStringColumnName[0]);
        else {
          /* Use specified string column */
          if (SDDS_CheckColumn(&inputPage, newColumnNamesColumn, NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY)
            SDDS_Bomb("Column specified with -newColumnNames does not exist in input file.");
          outputDoubleColumnName = (char **)SDDS_GetColumn(&inputPage, newColumnNamesColumn);
        }

        for (i = 1; i < inputRows; i++) {
          if (match_string(outputDoubleColumnName[i - 1], outputDoubleColumnName + i, inputRows - i, EXACT_MATCH) >= 0) {
            fprintf(stderr, "Error: Duplicate column name '%s' found in input file string column '%s'. Cannot be used as output column names.\n",
                    outputDoubleColumnName[i - 1], newColumnNamesColumn ? newColumnNamesColumn : inputStringColumnName[0]);
            exit(EXIT_FAILURE);
          }
        }
      } else {
        /* Use command line options to generate column names in the output file */
        outputDoubleColumnName = (char **)malloc(outputDoubleColumns * sizeof(char *));
        digits = MAX(digits, (long)(log10(inputRows) + 1));
        if (!Root) {
          Root = (char *)malloc(sizeof(char) * (strlen("Column") + 1));
          strcpy(Root, "Column");
        }
        if (outputDoubleColumns != 1) {
          for (i = 0; i < outputDoubleColumns; i++) {
            outputDoubleColumnName[i] = (char *)malloc(sizeof(char) * (strlen(Root) + digits + 1));
            sprintf(format, "%s%%0%ldld", Root, digits);
            sprintf(outputDoubleColumnName[i], format, i);
          }
        } else { /* Only one row to transpose */
          outputDoubleColumnName[0] = (char *)malloc(sizeof(char) * (strlen(Root) + 1));
          strcpy(outputDoubleColumnName[0], Root);
        }
      }

      /*************************
       * Define string columns 
       *************************/
      if (OldStringColumnsDefined) {
        if (!SDDS_DefineSimpleColumns(&outputPage, outputStringColumns, outputStringColumnName, NULL, SDDS_STRING))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        /* By default, at least one string column should exist for old column names */
        if (!noOldColumnNamesColumn) {
          outputStringColumns = 1;
          outputStringColumnName = (char **)malloc(sizeof(char *));
          if (oldColumnNames) {
            /* Command line option specification */
            outputStringColumnName[0] = oldColumnNames;
          } else {
            outputStringColumnName[0] = (char *)malloc(sizeof(char) * (strlen("OldColumnNames") + 1));
            strcpy(outputStringColumnName[0], "OldColumnNames");
          }
          if (0 > SDDS_DefineColumn(&outputPage, outputStringColumnName[0], NULL, NULL, NULL, NULL, SDDS_STRING, 0))
            SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }

      if (indexColumn && !SDDS_DefineSimpleColumn(&outputPage, "Index", NULL, SDDS_LONG))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      /*************************
       * Define numerical columns 
       *************************/
      for (i = 0; i < outputDoubleColumns; i++) {
        if (Symbol) {
          if (0 > SDDS_DefineColumn(&outputPage, outputDoubleColumnName[i], Symbol, NULL, NULL, NULL, SDDS_DOUBLE, 0))
            SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        } else {
          if (0 > SDDS_DefineColumn(&outputPage, outputDoubleColumnName[i], NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0))
            SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }

      /********************************
       * Define string parameters      
       * i.e., transposed string columns 
       ********************************/
      if (inputStringColumns > 1) {
        if (0 > SDDS_DefineParameter(&outputPage, OLD_STRING_COLUMN_NAMES, NULL, NULL, "Transposed string columns", NULL, SDDS_STRING, NULL))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (i = 0; i < inputStringColumns; i++) {
          if (0 > SDDS_DefineParameter(&outputPage, inputStringColumnName[i], NULL, NULL, "Transposed string column data", NULL, SDDS_STRING, NULL))
            SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }

      /*************************
       * Transfer other parameters 
       *************************/
      if (inputParameters) {
        for (i = 0; i < inputParameters; i++) {
          if ((0 > match_string(inputParameterName[i], outputStringColumnName, outputStringColumns, 0) &&
               strcasecmp(inputParameterName[i], OLD_STRING_COLUMN_NAMES))) {
            if (0 > SDDS_TransferParameterDefinition(&outputPage, &inputPage, inputParameterName[i], NULL))
              SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
      }

      /***************
       * Write layout 
       ***************/
      SDDS_DeferSavingLayout(&outputPage, 0);

      /* If InputFile is not already transferred to the output file, then create it */
      switch (SDDS_CheckParameter(&outputPage, "InputFile", NULL, SDDS_STRING, NULL)) {
      case SDDS_CHECK_NONEXISTENT:
        if (0 > SDDS_DefineParameter(&outputPage, "InputFile", NULL, NULL, "Original matrix file", NULL, SDDS_STRING, NULL))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      default:
        break;
      }

      if (!SDDS_WriteLayout(&outputPage))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

#if defined(DEBUG)
    fprintf(stderr, "Table layout defined\n");
#endif

    if (!SDDS_StartTable(&outputPage, outputRows))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (ipage == 1) {
      if (!SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "InputFile", inputfile ? inputfile : "pipe", NULL))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    /***************************************
     * Assign string columns from input     
     * to string parameters in output       
     ***************************************/
    if (inputStringColumns > 1) {
      for (i = 0; i < inputStringColumns; i++) {
        columnOfStrings = (char **)SDDS_GetColumn(&inputPage, inputStringColumnName[i]);
        stringParameter = JoinStrings(columnOfStrings, inputRows, BUFFER_SIZE_INCREMENT);
        if (!SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, inputStringColumnName[i], stringParameter, NULL))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        free(columnOfStrings);
        free(stringParameter);
      }
      outputStringRows = JoinStrings(inputStringColumnName, inputStringColumns, BUFFER_SIZE_INCREMENT);
      if (!SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, OLD_STRING_COLUMN_NAMES, outputStringRows, NULL))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

#if defined(DEBUG)
    fprintf(stderr, "String parameters assigned\n");
#endif

    if (inputParameters) {
      for (i = 0; i < inputParameters; i++) {
        if ((0 > match_string(inputParameterName[i], outputStringColumnName, outputStringColumns, 0) &&
             strcasecmp(inputParameterName[i], OLD_STRING_COLUMN_NAMES))) {
          parameterPointer = (void *)SDDS_GetParameter(&inputPage, inputParameterName[i], NULL);
          if (!SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, inputParameterName[i], parameterPointer, NULL))
            SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          free(parameterPointer);
        }
      }
    }

#if defined(DEBUG)
    fprintf(stderr, "Input parameters assigned\n");
#endif

    /**********************************
     * Assign data to output table part 
     * of data set                    
     **********************************/
    if (outputRows) {
      /***************************
       * Assign string column data 
       ***************************/
      if (OldStringColumnsDefined) {
        for (i = 0; i < outputStringColumns; i++) {
          if (!SDDS_GetParameter(&inputPage, outputStringColumnName[i], &stringParameter))
            SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          stringArray = TokenizeString(stringParameter, outputRows);
          if (!SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, stringArray, outputRows, outputStringColumnName[i]))
            SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          free(stringArray);
        }
      } else {
        if (!noOldColumnNamesColumn &&
            !SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, inputDoubleColumnName, outputRows, outputStringColumnName[0]))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

#if defined(DEBUG)
      fprintf(stderr, "String data columns assigned\n");
#endif

      /***************************
       * Assign numerical column data 
       ***************************/
      for (i = 0; i < outputDoubleColumns; i++) /* i is the row index */
        if (!SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, RInv->a[i], outputRows, outputDoubleColumnName[i]))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      if (indexColumn) {
        for (i = 0; i < outputRows; i++)
          if (!SDDS_SetRowValues(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i, "Index", i, NULL))
            SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

#if defined(DEBUG)
      fprintf(stderr, "Numerical data columns assigned\n");
#endif
    }

#if defined(DEBUG)
    fprintf(stderr, "Data assigned\n");
#endif

    if (!SDDS_WriteTable(&outputPage))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

#if defined(DEBUG)
    fprintf(stderr, "Data written out\n");
#endif
  }

  if (inputDoubleColumns) {
    m_free(&RInv);
    m_free(&R);
  }
  if (inputColumnName) {
    SDDS_FreeStringArray(inputColumnName, inputColumns);
    free(inputColumnName);
  }
  if (inputStringColumns)
    free(inputStringColumnName);
  if (inputDescription)
    free(inputDescription);
  if (inputParameterName) {
    SDDS_FreeStringArray(inputParameterName, inputParameters);
    free(inputParameterName);
  }
  if (outputDoubleColumns) {
    SDDS_FreeStringArray(outputDoubleColumnName, outputDoubleColumns);
    free(outputDoubleColumnName);
  }

  if (!SDDS_Terminate(&inputPage))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (ipage > 0) {
    if (!SDDS_Terminate(&outputPage))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile))
    exit(EXIT_FAILURE);

  return EXIT_SUCCESS;
}

char **TokenizeString(char *source, long n_items) {
  char *buffer;
  long buflen;
  char *ptr;
  char **string_array;
  long i;

  if (!source)
    return NULL;
  ptr = source;
  string_array = (char **)malloc(sizeof(char *) * n_items);
  buflen = strlen(source) + 1;
  buffer = (char *)malloc(sizeof(char) * buflen);
  for (i = 0; i < n_items; i++) {
    if (SDDS_GetToken(ptr, buffer, buflen) != -1)
      SDDS_CopyString(&string_array[i], buffer);
    else
      SDDS_CopyString(&string_array[i], "");
  }
  free(buffer);
  return string_array;
}

char *JoinStrings(char **source, long n_items, long buflen_increment) {
  char *buffer;
  char *ptr;
  long buffer_size, bufferLeft, bufferUsed;
  long i, slen;
  char *sptr;

  buffer_size = buflen_increment;
  buffer = (char *)malloc(sizeof(char) * buffer_size);
  buffer[0] = '\0';
  ptr = buffer;
  bufferLeft = buffer_size - 2;
  bufferUsed = 0;
  for (i = 0; i < n_items; i++) {
    sptr = source[i];
    slen = strlen(sptr);
    while ((slen + 5) > bufferLeft) {
      buffer = trealloc(buffer, sizeof(char) * (buffer_size += buflen_increment));
      bufferLeft += buflen_increment;
      ptr = buffer + bufferUsed;
    }
    if (i) {
      slen++;
      *ptr++ = ' ';
    }
    *ptr++ = '"';
    while (*sptr)
      *ptr++ = *sptr++;
    *ptr++ = '"';
    *ptr = '\0';
    bufferLeft -= slen + 2;
    bufferUsed += slen + 2;
  }

  return buffer;
}
