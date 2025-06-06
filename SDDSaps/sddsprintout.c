/**
 * @file sddsprintout.c
 * @brief Generate formatted printouts from SDDS data.
 *
 * This program reads SDDS data and produces formatted output based on user-specified options.
 * It supports various formats including plain text, spreadsheets, LaTeX, and HTML, providing
 * flexible data representation for scientific datasets.
 *
 * @section Usage
 * ```
 * sddsprintout [<SDDSinput>] [<outputfile>]
 *              [-pipe=[input][,output]] 
 *              [-columns[=<name-list>[,format={<string>|@<columnName>}][,label=<string>][,editLabel=<command>][,useDefaultFormat][,endsline][,blankLines=<number>]][,factor=<value>][,nounits]] 
 *              [-parameters[=<name-list>[,format={<string>|@<parameterName>}][,label=<string>][,editLabel=<command>][,useDefaultFormat][,endsline][,blankLines=<number]][,factor=<value>]] 
 *              [-array[=<name-list>[,format=<string>]] 
 *              [-fromPage=<number>] 
 *              [-toPage=<number>] 
 *              [-formatDefaults=<SDDStype>=<format-string>[,...]]
 *              [-width=<integer>] 
 *              [-pageAdvance] 
 *              [-paginate[=lines=<number>][,notitle][,nolabels]]
 *              [-noTitle] 
 *              [-title=<string>] 
 *              [-noLabels] 
 *              [-postPageLines=<number>]
 *              [-spreadsheet[=delimiter=<string>][,quotemark=<string>][,nolabels][,csv][,schfile=<filename>]]
 *              [-latexFormat[=longtable][,booktable][,sideways][,label={<string>|@<parameterName>}][,caption={<string>|@<parameterName>}][,group=<columnName>][,translate=<filename>][,justify=<codeList>][,complete][,comment=<string>]]
 *              [-htmlFormat[=caption=<string>][,translate=<filename>]]
 *              [-noWarnings]
 * ```
 *
 * @section Options
 * | Optional                            | Description                                                                      |
 * |-------------------------------------|----------------------------------------------------------------------------------|
 * | `-pipe`                             | Directs input/output via pipes.                                                  |
 * | `-columns`                          | Selects columns to include in the output with optional formatting, labels, etc.  |
 * | `-parameters`                       | Selects parameters for the output with optional formatting and labels.           |
 * | `-arrays`                           | Selects arrays for output.                                                       |
 * | `-fromPage`                         | Specifies the starting page of the dataset to process.                           |
 * | `-toPage`                           | Specifies the ending page of the dataset to process.                             |
 * | `-formatDefaults`                   | Defines default formatting strings for specified SDDS data types.                |
 * | `-width`                            | Specifies output width for text format.                                          |
 * | `-pageAdvance`                      | Forces a page break in the output between data pages.                            |
 * | `-paginate`                         | Enables pagination for plain text output.                                        |
 * | `-noTitle`                          | Suppresses title output.                                                         |
 * | `-title`                            | Adds a custom title to the output.                                               |
 * | `-noLabels`                         | Suppresses the output of column and parameter labels in the printed results      |
 * | `-postPageLines`                    | Adds a specified number of blank lines after each data page in the output        |
 * | `-spreadsheet`                      | Enables CSV or delimited file output with optional formatting.                   |
 * | `-latexFormat`                      | Configures LaTeX table formatting.                                               |
 * | `-htmlFormat`                       | Configures HTML table formatting.                                                |
 * | `-noWarnings`                       | Suppress warning messages                                                        |
 *
 * @subsection Incompatibilities
 * - `-pageAdvance` is incompatible with:
 *   - `-postPageLines`
 *   - `-paginate`
 * - `-latexFormat` cannot be combined with `-htmlFormat`.
 * - Only one of the following may be specified:
 *   - `-spreadsheet`
 *   - `-latexFormat`
 *   - `-htmlFormat`
 *
 * @subsection spec_req Specific Requirements
 * - When using `-latexFormat` with `label` or `caption`, ensure they are either literal strings or refer to parameter names using `@<parameterName>`.
 * - The `-spreadsheet=csv` option is shorthand for a combination of delimited output options, including `-nolabels`, `quotemark`, and `delimiter`.
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include <ctype.h>
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "hashtab.h"

/* Enumeration for option types */
enum option_type {
  SET_COLUMNS,
  SET_PARAMETERS,
  SET_ARRAYS,
  SET_FROMPAGE,
  SET_TOPAGE,
  SET_FORMATDEFAULTS,
  SET_WIDTH,
  SET_PIPE,
  SET_PAGEADVANCE,
  SET_NOTITLE,
  SET_TITLE,
  SET_SPREADSHEET,
  SET_PAGINATE,
  SET_NOWARNINGS,
  SET_POSTPAGELINES,
  SET_NOLABELS,
  SET_BUFFERLINES,
  SET_LATEXFORMAT,
  SET_HTMLFORMAT,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "columns",
  "parameters",
  "arrays",
  "frompage",
  "topage",
  "formatdefaults",
  "width",
  "pipe",
  "pageadvance",
  "notitle",
  "title",
  "spreadsheet",
  "paginate",
  "nowarnings",
  "postpagelines",
  "nolabels",
  "bufferlines",
  "latexformat",
  "htmlformat"
};

char *USAGE =
    "sddsprintout [<SDDSinput>] [<outputfile>]\n"
    "             [-pipe=[input][,output]] \n"
    "             [-columns[=<name-list>[,format={<string>|@<columnName>}][,label=<string>][,editLabel=<command>][,useDefaultFormat][,endsline][,blankLines=<number>]][,factor=<value>][,nounits]] \n"
    "             [-parameters[=<name-list>[,format={<string>|@<parameterName>}][,label=<string>][,editLabel=<command>][,useDefaultFormat][,endsline][,blankLines=<number]][,factor=<value>]] \n"
    "             [-array[=<name-list>[,format=<string>]] \n"
    "             [-fromPage=<number>] \n"
    "             [-toPage=<number>] \n"
    "             [-formatDefaults=<SDDStype>=<format-string>[,...]]\n"
    "             [-width=<integer>] \n"
    "             [-pageAdvance] \n"
    "             [-paginate[=lines=<number>][,notitle][,nolabels]]\n"
    "             [-noTitle] \n"
    "             [-title=<string>] \n"
    "             [-noLabels] \n"
    "             [-postPageLines=<number>]\n"
    "             [-spreadsheet[=delimiter=<string>][,quotemark=<string>][,nolabels][,csv][,schfile=<filename>]]\n"
    "             [-latexFormat[=longtable][,booktable][,sideways][,label={<string>|@<parameterName>}][,caption={<string>|@<parameterName>}][,group=<columnName>][,translate=<filename>][,justify=<codeList>][,complete][,comment=<string>]]\n"
    "             [-htmlFormat[=caption=<string>][,translate=<filename>]]\n"
    "             [-noWarnings]\n"
    // Additional explanations
    "-spreadsheet=csv is the simple way of -spreadsheet=nolabels,quote=\",delimiter=\\, -notitle \n"
    "Translation file for LaTeX mode has columns OldName and NewName, and can be used to translate symbols and units.\n\n"
    
    // Program information
    "Program by Michael Borland. (\"" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define PAGINATION_ON 0x0001U
#define PAGINATION_NOTITLE 0x0002U
#define PAGINATION_NOLABELS 0x0004U

#define LATEX_FORMAT 0x0001UL
#define LATEX_LONGTABLE 0x0002UL
#define LATEX_BOOKTABLE 0x0004UL
#define LATEX_LABEL 0x0008UL
#define LATEX_CAPTION 0x0010UL
#define LATEX_GROUP 0x0020UL
#define LATEX_TRANSLATE 0x0040UL
#define LATEX_SIDEWAYS 0x0080UL
#define LATEX_JUSTIFY 0x0100UL
#define LATEX_COMPLETE 0x0200UL
#define LATEX_COMMENT 0x0400UL
#define LATEX_LABEL_PARAM 0x0800UL
#define LATEX_CAPTION_PARAM 0x1000UL

#define HTML_FORMAT 0x0001U
#define HTML_CAPTION 0x0002U
#define HTML_TRANSLATE 0x0004U

typedef struct
{
  int32_t lines;
  long currentLine;
  unsigned long flags;
} PAGINATION;

#define ENDSLINE 0x0001U
#define USEDEFAULTFORMAT 0x0002U
#define LABEL_GIVEN 0x0004U
#define EDITLABEL_GIVEN 0x0008U
#define FACTOR_GIVEN 0x0010U
#define NO_UNITS 0x0020U

typedef struct
{
  char *name, *format, **header, *label, *editLabel;
  long fieldWidth, index, headers, type, endsLine, blankLines, useDefaultFormat, noUnits;
  double factor;
  void *data;
} PRINT_COLUMN;

typedef struct
{
  char *name, *format, *label, *editLabel;
  long fieldWidth, index, type, endsLine, blankLines, useDefaultFormat;
  double factor;
  void *data;
} PRINT_PARAMETER;

typedef struct
{
  char *name, *format, *label;
  long index;
  SDDS_ARRAY *array;
} PRINT_ARRAY;

char *makeTexSafeString(char *source);
char *makeTexExponentialString(char *text);
void copyAndPad(char *target, char *source, long sourceWidth, long targetWidth);
long makeColumnHeaders(char ***header, long *fieldWidth, char *name, char *editLabel, char *units, double factor,
                       char **format, unsigned long spreadsheetFlags, unsigned long latexFormat, htab *translationTable, unsigned long htmlFormat);
char *makeParameterLabel(long *fieldWidth, char *name, char *editLabel, char *units, double factor, char *format);
long changeDefaultFormats(char **argv, long argc, long noWarnings);
void setDefaultFormats(void);
long processPrintColumns(PRINT_COLUMN **printRequestPtr, long printRequests, SDDS_DATASET *inTable, long noWarnings,
                         unsigned long spreadsheetFlags, long csv, unsigned long latexformat, htab *translationTable, unsigned long htmlformat);
void printColumnHeaders(FILE *fpOut, PRINT_COLUMN *printColumn, long printColumns, long width, PAGINATION *pagination,
                        long latexFormat, char *latexTitle, long htmlFormat, char *htmlTitle);
long processPrintParameters(PRINT_PARAMETER **printRequestPtr, long printRequests, SDDS_DATASET *inTable,
                            long noWarnings, long noLabels, long csv);
long processPrintArrays(PRINT_ARRAY **printRequestPtr, long printRequests, SDDS_DATASET *inTable);
long getFormatFieldLength(char *format, long *extraChars);
char **makeListOfNames(char *string, long *names);
void doPrintParameters(SDDS_DATASET *inTable, PRINT_PARAMETER *printParameter, long printParameters, long width,
                       FILE *fpOut, unsigned long spreadsheetFlags, char *spreadsheetDelimiter,
                       char *spreadsheetQuoteMark, PAGINATION *pagination, char *title, long noLabels);
long characterCount(char *string, char c);
long printPageTitle(FILE *fpOut, char *title);
void doPrintColumns(SDDS_DATASET *inTable, PRINT_COLUMN *printColumn, long printColumns,
                    long width, FILE *fpOut, unsigned long spreadsheetFlags, char *spreadsheetDelimiter, char *spreadsheetQuoteMark,
                    long latexFormat, char *latexTitle, char *latexLabel, char *latexGroupColumn,
                    long htmlFormat, char *htmlTitle,
                    PAGINATION *pagination, char *title, long noLabels);
long checkPagination(FILE *fpOut, PAGINATION *pagination, char *title);
void replaceFormatWidth(char *buffer, char *format, long width);
void CreateSCHFile(char *output, char *input, unsigned long flags, char *delimiter, char *quote,
                   PRINT_COLUMN *printColumn, long printColumns);
htab *readTranslationTable(char *TranslationFile);
char *findTranslation(htab *ht, char *key);
char *modifyUnitsWithFactor(char *units0, double factor, unsigned long latexFormat);

static char **defaultFormat = NULL;
static char **csvFormat = NULL;
static char *latexJustify = NULL;

#define SPREADSHEET_ON 0x0001U
#define SPREADSHEET_DELIMITER 0x0002U
#define SPREADSHEET_QUOTEMARK 0x0004U
#define SPREADSHEET_NOLABELS 0x0008U
#define SPREADSHEET_CSV 0x0010UL

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_dataset;
  long i, i_arg, pageNumber, width;
  SCANNED_ARG *s_arg;
  char *input, *output, **name, *format, *title, *label, *editLabel, *schFile;
  PRINT_COLUMN *printColumn;
  PRINT_PARAMETER *printParameter;
  PRINT_ARRAY *printArray;
  long printColumns, printParameters, printArrays, firstPage, bufferLines;
  long fromPage, toPage, names, pageAdvance, noTitle, noLabels, noWarnings;
  int32_t blankLines;
  unsigned long latexFormat, htmlFormat;
  FILE *fpOut;
  unsigned long flags, pipeFlags, spreadsheetFlags, dummyFlags, postPageLines, CSV = 0;
  char *spreadsheetDelimiter, *spreadsheetQuoteMark;
  PAGINATION pagination;
  char **formatDefaultArg;
  long formatDefaultArgs;
  static char formatbuffer[100], formatbuffer2[100];
  char *latexLabel, *latexCaption = NULL, *latexGroup, *latexTranslationFile, *latexComment = NULL;
  char *latexCaptionBuffer = NULL, *latexLabelBuffer = NULL;
  htab *TranslationTable;
  char *htmlCaption = NULL, *htmlTranslationFile;
  double factor;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  input = output = NULL;
  fromPage = toPage = 0;
  printColumn = NULL;
  printParameter = NULL;
  printArray = NULL;
  width = 130;
  setDefaultFormats();
  printColumns = printParameters = printArrays = pipeFlags = flags = 0;
  postPageLines = pageAdvance = noTitle = noLabels = 0;
  title = NULL;
  spreadsheetFlags = 0;
  pagination.flags = pagination.currentLine = 0;
  noWarnings = 0;
  formatDefaultArgs = 0;
  schFile = NULL;
  formatDefaultArg = NULL;
  bufferLines = 0;
  latexFormat = 0;
  TranslationTable = NULL;
  htmlFormat = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_COLUMNS:
        if (s_arg[i_arg].n_items < 2) {
          s_arg[i_arg].n_items = 2;
          s_arg[i_arg].list = SDDS_Realloc(s_arg[i_arg].list, sizeof(*s_arg[i_arg].list) * 2);
          SDDS_CopyString(&s_arg[i_arg].list[1], "*");
        }
        name = makeListOfNames(s_arg[i_arg].list[1], &names);
        printColumn = SDDS_Realloc(printColumn, sizeof(*printColumn) * (names + printColumns));
        s_arg[i_arg].n_items -= 2;
        format = NULL;
        blankLines = 0;
        factor = 1;
        if (!scanItemList(&flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "format", SDDS_STRING, &format, 1, 0,
                          "endsline", -1, NULL, 0, ENDSLINE,
                          "usedefaultformat", -1, NULL, 0, USEDEFAULTFORMAT,
                          "blanklines", SDDS_LONG, &blankLines, 1, 0,
                          "editlabel", SDDS_STRING, &editLabel, 1, EDITLABEL_GIVEN,
                          "label", SDDS_STRING, &label, 1, LABEL_GIVEN,
                          "nounits", -1, NULL, 0, NO_UNITS,
                          "factor", SDDS_DOUBLE, &factor, 1, FACTOR_GIVEN, NULL))
          SDDS_Bomb("invalid -columns syntax");
        for (i = 0; i < names; i++) {
          SDDS_ZeroMemory(printColumn + i + printColumns, sizeof(*printColumn));
          printColumn[i + printColumns].name = name[i];
          if (format) {
            replaceString(formatbuffer, format, "ld", PRId32, -1, 0);
            replaceString(formatbuffer2, formatbuffer, "lu", PRIu32, -1, 0);
            SDDS_CopyString(&printColumn[i + printColumns].format, formatbuffer2);
          }
          if (flags & ENDSLINE && (i == names - 1))
            printColumn[i + printColumns].endsLine = 1;
          if (flags & USEDEFAULTFORMAT)
            printColumn[i + printColumns].useDefaultFormat = 1;
          printColumn[i + printColumns].label = printColumn[i + printColumns].editLabel = NULL;
          if (flags & LABEL_GIVEN)
            printColumn[i + printColumns].label = label;
          if (flags & EDITLABEL_GIVEN)
            printColumn[i + printColumns].editLabel = editLabel;
          printColumn[i + printColumns].noUnits = flags & NO_UNITS ? 1 : 0;
          printColumn[i + printColumns].factor = factor;
          printColumn[i + printColumns].blankLines = blankLines;
        }
        free(name);
        printColumns += names;
        break;
      case SET_PARAMETERS:
        if (s_arg[i_arg].n_items < 2) {
          s_arg[i_arg].n_items = 2;
          s_arg[i_arg].list = SDDS_Realloc(s_arg[i_arg].list, sizeof(*s_arg[i_arg].list) * 2);
          SDDS_CopyString(&s_arg[i_arg].list[1], "*");
        }
        name = makeListOfNames(s_arg[i_arg].list[1], &names);
        printParameter = SDDS_Realloc(printParameter, sizeof(*printParameter) * (names + printParameters));
        s_arg[i_arg].n_items -= 2;
        format = NULL;
        blankLines = 0;
        factor = 1;
        if (!scanItemList(&flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "format", SDDS_STRING, &format, 1, 0,
                          "endsline", -1, NULL, 0, ENDSLINE,
                          "usedefaultformat", -1, NULL, 0, USEDEFAULTFORMAT,
                          "blanklines", SDDS_LONG, &blankLines, 1, 0,
                          "editlabel", SDDS_STRING, &editLabel, 1, EDITLABEL_GIVEN,
                          "label", SDDS_STRING, &label, 1, LABEL_GIVEN,
                          "factor", SDDS_DOUBLE, &factor, 1, FACTOR_GIVEN, NULL))
          SDDS_Bomb("invalid -parameters syntax");
        for (i = 0; i < names; i++) {
          SDDS_ZeroMemory(printParameter + i + printParameters, sizeof(*printParameter));
          printParameter[i + printParameters].name = name[i];
          if (format) {
            replaceString(formatbuffer, format, "ld", PRId32, -1, 0);
            replaceString(formatbuffer2, formatbuffer, "lu", PRIu32, -1, 0);
            SDDS_CopyString(&printParameter[i + printParameters].format, formatbuffer2);
          }
          if (flags & ENDSLINE && (i == names - 1))
            printParameter[i + printParameters].endsLine = 1;
          if (flags & USEDEFAULTFORMAT)
            printParameter[i + printParameters].useDefaultFormat = 1;
          printParameter[i + printParameters].label = printParameter[i + printParameters].editLabel = NULL;
          if (flags & LABEL_GIVEN)
            printParameter[i + printParameters].label = label;
          if (flags & EDITLABEL_GIVEN)
            printParameter[i + printParameters].editLabel = editLabel;
          printParameter[i + printParameters].factor = factor;
          printParameter[i + printParameters].blankLines = blankLines;
        }
        free(name);
        printParameters += names;
        break;
      case SET_ARRAYS:
        if (s_arg[i_arg].n_items < 2) {
          s_arg[i_arg].n_items = 2;
          s_arg[i_arg].list = SDDS_Realloc(s_arg[i_arg].list, sizeof(*s_arg[i_arg].list) * 2);
          SDDS_CopyString(&s_arg[i_arg].list[1], "*");
        }
        name = makeListOfNames(s_arg[i_arg].list[1], &names);
        printArray = SDDS_Realloc(printArray, sizeof(*printArray) * (names + printArrays));
        s_arg[i_arg].n_items -= 2;
        format = NULL;
        if (!scanItemList(&flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "format", SDDS_STRING, &format, 1, 0, NULL))
          SDDS_Bomb("invalid -arrays syntax");
        for (i = 0; i < names; i++) {
          printArray[i + printArrays].name = name[i];
          if (format) {
            replaceString(formatbuffer, format, "ld", PRId32, -1, 0);
            replaceString(formatbuffer2, formatbuffer, "lu", PRIu32, -1, 0);
            SDDS_CopyString(&printArray[i + printArrays].format, formatbuffer2);
          }
        }
        free(name);
        printArrays += names;
        break;
      case SET_FROMPAGE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -fromPage syntax");
        if (fromPage != 0)
          SDDS_Bomb("invalid syntax: specify -fromPage once only");
        if (sscanf(s_arg[i_arg].list[1], "%ld", &fromPage) != 1 || fromPage <= 0)
          SDDS_Bomb("invalid -fromPage syntax or value");
        break;
      case SET_TOPAGE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -toPage syntax");
        if (toPage != 0)
          SDDS_Bomb("invalid syntax: specify -toPage once only");
        if (sscanf(s_arg[i_arg].list[1], "%ld", &toPage) != 1 || toPage <= 0)
          SDDS_Bomb("invalid -toPage syntax or value");
        break;
      case SET_FORMATDEFAULTS:
        formatDefaultArg = s_arg[i_arg].list + 1;
        formatDefaultArgs = s_arg[i_arg].n_items - 1;
        break;
      case SET_WIDTH:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &width) != 1 || (width < 40 && width))
          SDDS_Bomb("invalid -width syntax or value");
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_PAGEADVANCE:
        pageAdvance = 1;
        break;
      case SET_NOTITLE:
        noTitle = 1;
        break;
      case SET_TITLE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -title syntax");
        title = s_arg[i_arg].list[1];
        break;
      case SET_SPREADSHEET:
        s_arg[i_arg].n_items--;
        spreadsheetDelimiter = NULL;
        spreadsheetQuoteMark = "";
        if (!scanItemList(&spreadsheetFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "delimiter", SDDS_STRING, &spreadsheetDelimiter, 1, SPREADSHEET_DELIMITER,
                          "quotemark", SDDS_STRING, &spreadsheetQuoteMark, 1, SPREADSHEET_QUOTEMARK,
                          "nolabels", -1, NULL, 0, SPREADSHEET_NOLABELS, "csv", -1, NULL, 0, SPREADSHEET_CSV,
                          "schfile", SDDS_STRING, &schFile, 1, 0, NULL))
          SDDS_Bomb("invalid -spreadsheet syntax");
        if (!spreadsheetDelimiter || !(spreadsheetFlags & SPREADSHEET_DELIMITER))
          spreadsheetDelimiter = "\t";
        if (spreadsheetFlags & SPREADSHEET_CSV) {
          spreadsheetDelimiter = ",";
          spreadsheetFlags |= SPREADSHEET_DELIMITER;
          spreadsheetFlags |= SPREADSHEET_QUOTEMARK;
          spreadsheetQuoteMark = "\"";
          noTitle = 1;
          CSV = 1;
        }
        spreadsheetFlags |= SPREADSHEET_ON;
        width = 0;
        break;
      case SET_PAGINATE:
        s_arg[i_arg].n_items--;
        pagination.lines = 66;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "lines", SDDS_LONG, &pagination.lines, 1, 0,
                          "notitle", -1, NULL, 0, PAGINATION_NOTITLE,
                          "nolabels", -1, NULL, 0, PAGINATION_NOLABELS, NULL) ||
            pagination.lines <= 3)
          SDDS_Bomb("invalid -paginate syntax/values");
        pagination.flags |= PAGINATION_ON;
        break;
      case SET_NOWARNINGS:
        noWarnings = 1;
        break;
      case SET_POSTPAGELINES:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &postPageLines) != 1)
          SDDS_Bomb("invalid -postPageLines syntax/values");
        break;
      case SET_NOLABELS:
        noLabels = 1;
        break;
      case SET_BUFFERLINES:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -bufferLines syntax");
        if (sscanf(s_arg[i_arg].list[1], "%ld", &bufferLines) != 1 || bufferLines < 0)
          SDDS_Bomb("invalid -bufferLines syntax or value");
        break;
      case SET_LATEXFORMAT:
        s_arg[i_arg].n_items--;
        latexLabel = NULL;
        latexCaption = NULL;
        latexComment = NULL;
        if (!scanItemList(&latexFormat, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "longtable", -1, NULL, 0, LATEX_LONGTABLE,
                          "booktable", -1, NULL, 0, LATEX_BOOKTABLE,
                          "sideways", -1, NULL, 0, LATEX_SIDEWAYS,
                          "label", SDDS_STRING, &latexLabel, 1, LATEX_LABEL,
                          "caption", SDDS_STRING, &latexCaption, 1, LATEX_CAPTION,
                          "comment", SDDS_STRING, &latexComment, 1, LATEX_COMMENT,
                          "group", SDDS_STRING, &latexGroup, 1, LATEX_GROUP,
                          "translate", SDDS_STRING, &latexTranslationFile, 1, LATEX_TRANSLATE,
                          "justify", SDDS_STRING, &latexJustify, 1, LATEX_JUSTIFY,
                          "complete", -1, NULL, 0, LATEX_COMPLETE, NULL))
          SDDS_Bomb("invalid -latexFormat syntax/values");
        latexFormat |= LATEX_FORMAT;
        if (latexFormat & LATEX_LONGTABLE && latexFormat & LATEX_SIDEWAYS)
          SDDS_Bomb("invalid -latexFormat syntax/values: give only one of longtable and sideways");
        break;
      case SET_HTMLFORMAT:
        s_arg[i_arg].n_items--;
        htmlCaption = NULL;
        if (!scanItemList(&htmlFormat, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "caption", SDDS_STRING, &htmlCaption, 1, HTML_CAPTION,
                          "translate", SDDS_STRING, &htmlTranslationFile, 1, HTML_TRANSLATE, NULL))
          SDDS_Bomb("invalid -htmlFormat syntax/values");
        htmlFormat |= HTML_FORMAT;
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        SDDS_Bomb(NULL);
        break;
      }
    } else {
      if (!input)
        input = s_arg[i_arg].list[0];
      else if (!output)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  pipeFlags |= DEFAULT_STDOUT;
  processFilenames("sddsprintout", &input, &output, pipeFlags, noWarnings, NULL);

  if (formatDefaultArgs)
    changeDefaultFormats(formatDefaultArg, formatDefaultArgs, noWarnings);

  if (pageAdvance) {
    if (postPageLines)
      SDDS_Bomb("-pageAdvance and -postPageLines are incompatible");
    if (pagination.flags & PAGINATION_ON)
      SDDS_Bomb("-pageAdvance and -paginate are incompatible");
  }
  if (pagination.flags & PAGINATION_ON && postPageLines)
    SDDS_Bomb("-postPageLines and -paginate are incompatible");

  if (!printColumns && !printParameters && !printArrays)
    SDDS_Bomb("you must specify at least one of -columns, -parameters, or -arrays");
  if (fromPage && toPage && fromPage > toPage)
    SDDS_Bomb("invalid -fromPage and -toPage");
  if (latexFormat && htmlFormat)
    SDDS_Bomb("-latexFormat and -htmlFormat are incompatible");

  if (latexFormat & LATEX_TRANSLATE)
    TranslationTable = readTranslationTable(latexTranslationFile);
  if (htmlFormat & HTML_TRANSLATE)
    TranslationTable = readTranslationTable(htmlTranslationFile);

  if (!SDDS_InitializeInput(&SDDS_dataset, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  fpOut = stdout;
  if (output && !(fpOut = fopen(output, "w")))
    SDDS_Bomb("unable to open output file");

  printColumns = processPrintColumns(&printColumn, printColumns, &SDDS_dataset, noWarnings,
                                     spreadsheetFlags, CSV, latexFormat, TranslationTable, htmlFormat);
  printParameters = processPrintParameters(&printParameter, printParameters, &SDDS_dataset, noWarnings, noLabels, CSV);
  /*    printArrays = processPrintArrays(&printArray, printArrays, &SDDS_dataset); */

  SDDS_SetTerminateMode(TERMINATE_DONT_FREE_TABLE_STRINGS + TERMINATE_DONT_FREE_ARRAY_STRINGS);

  firstPage = 1;
  pagination.flags |= noTitle ? PAGINATION_NOTITLE : 0;
  if (!title) {
    title = tmalloc(sizeof(*title) * ((input ? strlen(input) : 10) + 100));
    if (htmlFormat)
      sprintf(title, "Printout for SDDS file %s", input ? input : "stdin");
    else
      sprintf(title, "Printout for SDDS file %s%s", input ? input : "stdin", latexFormat ? "" : "\n");
  }

  if (schFile) {
    if (printArrays || printParameters || !printColumns)
      SDDS_Bomb("Can't create schFile except for pure column printout.");
    CreateSCHFile(schFile, input, spreadsheetFlags, spreadsheetDelimiter, spreadsheetQuoteMark, printColumn, printColumns);
  }
  if (latexFormat) {
    noTitle = 1;
    if (latexFormat & LATEX_LABEL && latexLabel[0] == '@') {
      memmove(latexLabel, latexLabel + 1, strlen(latexLabel));
      if (SDDS_CheckParameter(&SDDS_dataset, latexLabel, NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK) {
        fprintf(stderr, "sddsprintout: error: parameter %s not found in input file\n", latexLabel);
        exit(EXIT_FAILURE);
      }
      latexFormat |= LATEX_LABEL_PARAM;
    }
    if (latexFormat & LATEX_CAPTION && latexCaption[0] == '@') {
      memmove(latexCaption, latexCaption + 1, strlen(latexCaption));
      if (SDDS_CheckParameter(&SDDS_dataset, latexCaption, NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK) {
        fprintf(stderr, "sddsprintout: error: parameter %s not found in input file\n", latexCaption);
        exit(EXIT_FAILURE);
      }
      latexFormat |= LATEX_CAPTION_PARAM;
    }
    if (latexFormat & LATEX_COMPLETE) {
      fprintf(fpOut, "\\documentclass{report}\n\\pagestyle{empty}\n");
      if (latexFormat & LATEX_BOOKTABLE)
        fprintf(fpOut, "\\usepackage{booktabs}\n");
      if (latexFormat & LATEX_SIDEWAYS)
        fprintf(fpOut, "\\usepackage{rotating}\n");
      fprintf(fpOut, "\\begin{document}\n");
    }
    if (latexComment)
      fprintf(fpOut, "%% %s\n", latexComment);
  }
  if (htmlFormat) {
    noTitle = 1;
  }

  while ((pageNumber = SDDS_ReadPageSparse(&SDDS_dataset, 0, (printColumns || SDDS_dataset.layout.data_mode.column_major) ? 1 : 1000000, 0, 0)) > 0) {
    if ((fromPage && pageNumber < fromPage) || (toPage && pageNumber > toPage))
      continue;
    if (pagination.flags & PAGINATION_ON) {
      if (!firstPage) {
        fputc('\014', fpOut);
        pagination.currentLine = 1;
      }
      if (!noTitle)
        pagination.currentLine += printPageTitle(fpOut, title);
    } else {
      if (firstPage) {
        if (!noTitle)
          printPageTitle(fpOut, title);
      } else if (pageAdvance)
        fputc('\014', fpOut);
      else if (postPageLines > 0) {
        long line = postPageLines;
        while (line--)
          fputc('\n', fpOut);
      }
    }
    if (!latexFormat && !htmlFormat)
      doPrintParameters(&SDDS_dataset, printParameter, printParameters, width, fpOut, spreadsheetFlags,
                        spreadsheetDelimiter, spreadsheetQuoteMark, &pagination, title, noLabels);
    /*        doPrintArrays(&SDDS_dataset, printArray, printArrays, width, fpOut, &pagination); */
    latexCaptionBuffer = latexLabelBuffer = NULL;
    if (latexFormat & LATEX_CAPTION_PARAM && !(latexCaptionBuffer = SDDS_GetParameterAsString(&SDDS_dataset, latexCaption, NULL))) {
      char buffer[1024];
      snprintf(buffer, 1024, "Error: can't read latex caption parameter %s\n", latexCaption);
      SDDS_Bomb(buffer);
    }
    if (latexFormat & LATEX_LABEL_PARAM && !(latexLabelBuffer = SDDS_GetParameterAsString(&SDDS_dataset, latexLabel, NULL))) {
      char buffer[1024];
      snprintf(buffer, 1024, "Error: can't read latex label parameter %s\n", latexLabel);
      SDDS_Bomb(buffer);
    }
    doPrintColumns(&SDDS_dataset, printColumn, printColumns, width, fpOut, spreadsheetFlags, spreadsheetDelimiter,
                   spreadsheetQuoteMark, latexFormat,
                   latexFormat & LATEX_CAPTION_PARAM ? latexCaptionBuffer : latexCaption,
                   latexFormat & LATEX_LABEL_PARAM ? latexLabelBuffer : latexLabel,
                   latexGroup, htmlFormat, htmlCaption,
                   &pagination, title, noLabels);
    if (latexFormat & LATEX_CAPTION_PARAM && latexCaptionBuffer)
      free(latexCaptionBuffer);
    if (latexFormat & LATEX_LABEL_PARAM && latexLabelBuffer)
      free(latexLabelBuffer);
    if (bufferLines) {
      for (i = 0; i < bufferLines; i++)
        fputc('\n', fpOut);
    }
    firstPage = 0;
  }

  if (!SDDS_Terminate(&SDDS_dataset))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < printColumns; i++) {
    if (printColumn[i].headers) {
      for (long j = 0; j < printColumn[i].headers; j++) {
        free(printColumn[i].header[j]);
      }
      free(printColumn[i].header);
    }
  }
  if (latexFormat & LATEX_COMPLETE)
    fprintf(fpOut, "\\end{document}\n");

  free(printColumn);
  free_scanargs(&s_arg, argc);
  return EXIT_SUCCESS;
}

long processPrintColumns(PRINT_COLUMN **printRequestPtr, long printRequests, SDDS_DATASET *inTable, long noWarnings,
                         unsigned long spreadsheetFlags, long csv, unsigned long latexFormat, htab *TranslationTable, unsigned long htmlFormat) {
  PRINT_COLUMN *printColumn, *printRequest;
  long i, irequest, iname, printColumns, names, *columnUsed, columnLimit;
  int64_t index;
  char **name, *units, *format;

  if (printRequests < 1 || !(printRequest = *printRequestPtr))
    return 0;
  if ((columnLimit = SDDS_ColumnCount(inTable)) < 0) {
    if (!noWarnings)
      fprintf(stderr, "warning: no column data in input file\n");
    return 0;
  }
  columnUsed = tmalloc(sizeof(*columnUsed) * columnLimit);
  for (i = 0; i < columnLimit; i++)
    columnUsed[i] = 0;
  printColumn = tmalloc(sizeof(*printColumn) * columnLimit);
  printColumns = 0;
  name = NULL;

  for (irequest = 0; irequest < printRequests; irequest++) {
    if ((names = SDDS_MatchColumns(inTable, &name, SDDS_MATCH_STRING, FIND_ANY_TYPE, printRequest[irequest].name, SDDS_0_PREVIOUS | SDDS_OR)) > 0) {
      for (iname = 0; iname < names; iname++) {
        if ((index = SDDS_GetColumnIndex(inTable, name[iname])) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!columnUsed[index]) {
          if (SDDS_GetColumnInformation(inTable, "units", &units, SDDS_GET_BY_INDEX, index) != SDDS_STRING)
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          SDDS_CopyString(&printColumn[printColumns].name, name[iname]);
          printColumn[printColumns].index = index;
          if (iname == names - 1)
            printColumn[printColumns].endsLine = printRequest[irequest].endsLine;
          else
            printColumn[printColumns].endsLine = 0;
          printColumn[printColumns].type = SDDS_GetColumnType(inTable, index);
          printColumn[printColumns].blankLines = printRequest[irequest].blankLines;
          if (SDDS_GetColumnInformation(inTable, "format_string", &format, SDDS_GET_BY_INDEX, index) != SDDS_STRING)
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

          if (csv) {
            SDDS_CopyString(&printColumn[printColumns].format, csvFormat[printColumn[printColumns].type - 1]);
          } else {
            if (printRequest[irequest].format) {
              if (printRequest[irequest].format[0] != '@') {
                if (!SDDS_VerifyPrintfFormat(printRequest[irequest].format, printColumn[printColumns].type)) {
                  fprintf(stderr, "error: given format (\"%s\") for column %s is invalid\n",
                          printRequest[irequest].format, name[iname]);
                  exit(EXIT_FAILURE);
                }
              } else {
                long formatIndex;
                if ((formatIndex = SDDS_GetColumnIndex(inTable, printRequest[irequest].format + 1)) < 0 ||
                    SDDS_GetColumnType(inTable, formatIndex) != SDDS_STRING) {
                  fprintf(stderr, "error: given format column (\"%s\") for column %s is absent or not string type\n",
                          printRequest[irequest].format + 1, name[iname]);
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                  exit(EXIT_FAILURE);
                }
              }
              SDDS_CopyString(&printColumn[printColumns].format, printRequest[irequest].format);
            } else if (SDDS_StringIsBlank(format) || printRequest[irequest].useDefaultFormat)
              SDDS_CopyString(&printColumn[printColumns].format, defaultFormat[printColumn[printColumns].type - 1]);
            else
              SDDS_CopyString(&printColumn[printColumns].format, format);
          }
          printColumn[printColumns].headers =
            makeColumnHeaders(&printColumn[printColumns].header, &printColumn[printColumns].fieldWidth,
                              printRequest[irequest].label ? printRequest[irequest].label : name[iname],
                              printRequest[irequest].editLabel,
                              printRequest[irequest].noUnits ? NULL : units, printRequest[irequest].factor,
                              &printColumn[printColumns].format, spreadsheetFlags, latexFormat, TranslationTable, htmlFormat);
#if defined(DEBUG)
          fprintf(stderr, "%s has format >%s<\n",
                  printRequest[irequest].label ? printRequest[irequest].label : name[iname], printColumn[printColumns].format);
#endif
          printColumn[printColumns].data = NULL;
          printColumn[printColumns].factor = printRequest[irequest].factor;
          printColumns++;
          columnUsed[index] = 1;
          if (units)
            free(units);
        }
        free(name[iname]);
      }
      free(name);
    } else if (names < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    else if (!noWarnings)
      fprintf(stderr, "warning: no column matches %s\n", printRequest[irequest].name);
    free(printRequest[irequest].name);
    if (printRequest[irequest].format)
      free(printRequest[irequest].format);
  }
  free(printRequest);
  *printRequestPtr = printColumn;
  return printColumns;
}

long processPrintParameters(PRINT_PARAMETER **printRequestPtr, long printRequests, SDDS_DATASET *inTable,
                            long noWarnings, long noLabels, long csv) {
  PRINT_PARAMETER *printParameter, *printRequest;
  long i, irequest, iname, printParameters, names, *parameterUsed, parameterLimit, maxFieldWidth, index;
  char **name, *units, *format;

  if (printRequests < 1 || !(printRequest = *printRequestPtr))
    return 0;
  if ((parameterLimit = SDDS_ParameterCount(inTable)) < 0) {
    if (!noWarnings)
      fprintf(stderr, "warning: no parameter data in input file\n");
    return 0;
  }
  parameterUsed = tmalloc(sizeof(*parameterUsed) * parameterLimit);
  for (i = 0; i < parameterLimit; i++)
    parameterUsed[i] = 0;
  printParameter = tmalloc(sizeof(*printParameter) * parameterLimit);
  printParameters = 0;
  name = NULL;
  maxFieldWidth = 0;
  for (irequest = 0; irequest < printRequests; irequest++) {
    if ((names = SDDS_MatchParameters(inTable, &name, SDDS_MATCH_STRING, FIND_ANY_TYPE, printRequest[irequest].name, SDDS_0_PREVIOUS | SDDS_OR)) > 0) {
      for (iname = 0; iname < names; iname++) {
        if ((index = SDDS_GetParameterIndex(inTable, name[iname])) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!parameterUsed[index]) {
          if (SDDS_GetParameterInformation(inTable, "units", &units, SDDS_GET_BY_INDEX, index) != SDDS_STRING)
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          SDDS_CopyString(&printParameter[printParameters].name, name[iname]);
          printParameter[printParameters].index = index;
          printParameter[printParameters].type = SDDS_GetParameterType(inTable, index);
          if (iname == names - 1)
            printParameter[printParameters].endsLine = printRequest[irequest].endsLine;
          printParameter[printParameters].blankLines = printRequest[irequest].blankLines;
          if (SDDS_GetParameterInformation(inTable, "format_string", &format, SDDS_GET_BY_INDEX, index) != SDDS_STRING)
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

          if (csv) {
            SDDS_CopyString(&printParameter[printParameters].format, csvFormat[printParameter[printParameters].type - 1]);
          } else {
            if (printRequest[irequest].format) {
              if (!SDDS_VerifyPrintfFormat(printRequest[irequest].format, printParameter[printParameters].type)) {
                fprintf(stderr, "error: given format (\"%s\") for parameter %s is invalid\n", printRequest[irequest].format, name[iname]);
                exit(EXIT_FAILURE);
              }
              SDDS_CopyString(&printParameter[printParameters].format, printRequest[irequest].format);
            } else if (SDDS_StringIsBlank(format) || printRequest[irequest].useDefaultFormat)
              SDDS_CopyString(&printParameter[printParameters].format, defaultFormat[printParameter[printParameters].type - 1]);
            else
              SDDS_CopyString(&printParameter[printParameters].format, format);
          }

          if (!noLabels) {
            printParameter[printParameters].label =
              makeParameterLabel(&printParameter[printParameters].fieldWidth, printRequest[irequest].label ? printRequest[irequest].label : name[iname], printRequest[irequest].editLabel, units, printRequest[irequest].factor, printParameter[printParameters].format);
          } else {
            printParameter[printParameters].label = "";
          }
          if (printParameter[printParameters].fieldWidth > maxFieldWidth)
            maxFieldWidth = printParameter[printParameters].fieldWidth;
          printParameter[printParameters].data = NULL;
          printParameter[printParameters].factor = printRequest[irequest].factor;
          printParameters++;
          parameterUsed[index] = 1;
          if (units)
            free(units);
        }
        free(name[iname]);
      }
      free(name);
    } else if (names < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    else if (!noWarnings)
      fprintf(stderr, "warning: no parameter matches %s\n", printRequest[irequest].name);
    free(printRequest[irequest].name);
    if (printRequest[irequest].format)
      free(printRequest[irequest].format);
  }
  for (i = 0; i < printParameters; i++) {
    char *ptr;
    if (printParameter[i].fieldWidth < maxFieldWidth) {
      ptr = tmalloc(sizeof(*ptr) * (maxFieldWidth + 1));
      strcpy(ptr, printParameter[i].label);
      pad_with_spaces(ptr, maxFieldWidth - printParameter[i].fieldWidth);
      free(printParameter[i].label);
      printParameter[i].label = ptr;
      printParameter[i].fieldWidth = maxFieldWidth;
    }
  }
  free(printRequest);
  *printRequestPtr = printParameter;
  return printParameters;
}

#if SDDS_NUM_TYPES != 11
#  error "number of SDDS types is not 11 as expected"
#endif

void setDefaultFormats(void) {
  defaultFormat = tmalloc(sizeof(*defaultFormat) * SDDS_NUM_TYPES);
  SDDS_CopyString(defaultFormat + SDDS_LONGDOUBLE - 1, "%13.6e");
  SDDS_CopyString(defaultFormat + SDDS_DOUBLE - 1, "%13.6e");
  SDDS_CopyString(defaultFormat + SDDS_FLOAT - 1, "%13.6e");
  SDDS_CopyString(defaultFormat + SDDS_LONG64 - 1, "%10" PRId64);
  SDDS_CopyString(defaultFormat + SDDS_ULONG64 - 1, "%10" PRIu64);
  SDDS_CopyString(defaultFormat + SDDS_LONG - 1, "%10" PRId32);
  SDDS_CopyString(defaultFormat + SDDS_ULONG - 1, "%10" PRIu32);
  SDDS_CopyString(defaultFormat + SDDS_SHORT - 1, "%5hd");
  SDDS_CopyString(defaultFormat + SDDS_USHORT - 1, "%5hu");
  SDDS_CopyString(defaultFormat + SDDS_CHARACTER - 1, "%c");
  SDDS_CopyString(defaultFormat + SDDS_STRING - 1, "%16s");
  csvFormat = tmalloc(sizeof(*csvFormat) * SDDS_NUM_TYPES);
  SDDS_CopyString(csvFormat + SDDS_LONGDOUBLE - 1, "%13.6e");
  SDDS_CopyString(csvFormat + SDDS_DOUBLE - 1, "%13.6e");
  SDDS_CopyString(csvFormat + SDDS_FLOAT - 1, "%13.3e");
  SDDS_CopyString(csvFormat + SDDS_LONG64 - 1, "%" PRId64);
  SDDS_CopyString(csvFormat + SDDS_ULONG64 - 1, "%" PRIu64);
  SDDS_CopyString(csvFormat + SDDS_LONG - 1, "%" PRId32);
  SDDS_CopyString(csvFormat + SDDS_ULONG - 1, "%" PRIu32);
  SDDS_CopyString(csvFormat + SDDS_SHORT - 1, "%hd");
  SDDS_CopyString(csvFormat + SDDS_USHORT - 1, "%hu");
  SDDS_CopyString(csvFormat + SDDS_CHARACTER - 1, "%c");
  SDDS_CopyString(csvFormat + SDDS_STRING - 1, "%s");
}

long changeDefaultFormats(char **argv, long argc, long noWarnings) {
  long i, type;
  char *format;
  static char formatbuffer[100], formatbuffer2[100];

  for (i = 0; i < argc; i++) {
    if (!(format = strchr(argv[i], '='))) {
      fprintf(stderr, "-formatDefault syntax error with keyword \"%s\"\n", argv[i]);
      return 0;
    }
    *format++ = 0;
    if (!(type = SDDS_IdentifyType(argv[i]))) {
      fprintf(stderr, "-formatDefault error: unknown type \"%s\"\n", argv[i]);
      return 0;
    }
    if (!SDDS_VerifyPrintfFormat(format, type)) {
      fprintf(stderr, "-formatDefault error: invalid format string \"%s\" for type \"%s\"\n", format, argv[i]);
      return 0;
    }
    free(defaultFormat[type - 1]);

    if (SDDS_LONG == type) {
      replaceString(formatbuffer, format, "ld", PRId32, -1, 0);
      replaceString(formatbuffer2, formatbuffer, "lu", PRIu32, -1, 0);
      SDDS_CopyString(defaultFormat + type - 1, formatbuffer2);
    } else {
      SDDS_CopyString(defaultFormat + type - 1, format);
    }
    if (!getFormatFieldLength(format, NULL) && !noWarnings)
      fprintf(stderr, "warning: no field length for default format \"%s\"---this will produce poor results\n", defaultFormat[type - 1]);
  }
  return 1;
}

long makeColumnHeaders(char ***header, long *fieldWidth, char *name, char *editLabel, char *units, double factor,
                       char **format, unsigned long spreadsheetHeaders, unsigned long latexFormat, htab *TranslationTable, unsigned long htmlFormat) {
  long formatWidth, unitsWidth, nameWidth, headers, i, formatPadding, formatExtraChars;
  static char buffer[1024], buffer2[1026];
  static char nameBuffer[1024];

  formatWidth = 0;

  if (latexFormat & LATEX_TRANSLATE) {
    name = findTranslation(TranslationTable, name);
    units = findTranslation(TranslationTable, units);
  }
  if (htmlFormat & HTML_TRANSLATE) {
    name = findTranslation(TranslationTable, name);
    units = findTranslation(TranslationTable, units);
  }
  if (factor != 1)
    units = modifyUnitsWithFactor(units, 1. / factor, latexFormat);

  if (editLabel) {
    strcpy(nameBuffer, name);
    if (!edit_string(nameBuffer, editLabel))
      SDDS_Bomb("Problem editing column label");
    name = nameBuffer;
  }

#if defined(DEBUG)
  fprintf(stderr, "name: >%s<  format: >%s<\n", name, *format ? *format : "NULL");
#endif

  /* the field width is the large of the units width and the column name width */
  unitsWidth = units ? strlen(units) : 0;
  nameWidth = strlen(name);
  *fieldWidth = unitsWidth > nameWidth ? unitsWidth : nameWidth;

  if (!(spreadsheetHeaders & SPREADSHEET_ON)) {
    /* compare the field width to the width in the format string, if any.
       field width needs to be at least equal to the format width + 2 to
       provide buffer spaces
    */
    if (!*format || (*format && (*format)[0] != '@')) {
      if (*format) {
        formatWidth = getFormatFieldLength(*format, &formatExtraChars);
        if ((formatWidth + formatExtraChars) > *fieldWidth)
          *fieldWidth = formatWidth + formatExtraChars;
      }
      *fieldWidth += 2;
      if (*fieldWidth < 2)
        *fieldWidth = 2;
#if defined(DEBUG)
      fprintf(stderr, "fieldWidth = %ld, formatWidth = %ld, formatExtra = %ld\n", *fieldWidth, formatWidth, formatExtraChars);
#endif
      if (*format && (formatPadding = (*fieldWidth - 2 - formatExtraChars) - formatWidth) > 0) {
        /* come here if the format width is smaller than the field width less
           the extra char and buffer space.  Change the format width to fill up the
           available space
        */
        formatWidth = *fieldWidth - 2 - formatExtraChars;
        replaceFormatWidth(buffer, *format, formatWidth);
#if defined(DEBUG)
        fprintf(stderr, "Adjusted format width to %ld: >%s<\n", formatWidth, buffer);
#endif
        free(*format);
        sprintf(buffer2, " %s ", buffer);
        SDDS_CopyString(format, buffer2);
      } else {
        sprintf(buffer, " %s ", *format);
        free(*format);
        SDDS_CopyString(format, buffer);
      }
    }
  }

  *header = tmalloc(sizeof(**header) * (headers = 2));
  for (i = 0; i < headers; i++)
    (*header)[i] = tmalloc(sizeof(***header) * (*fieldWidth + 1));

  /* first header is column name */
  copyAndPad((*header)[0], name, nameWidth, *fieldWidth);
  /* second header is column units */
  copyAndPad((*header)[1], units, unitsWidth, *fieldWidth);
#if defined(DEBUG)
  fprintf(stderr, "header0 padded to %ld: >%s<\n", *fieldWidth, (*header)[0]);
  (*header)[0][0] = '+';
  (*header)[0][strlen((*header)[0]) - 1] = '-';
  fprintf(stderr, "header1 padded to %ld: >%s<\n", *fieldWidth, (*header)[1]);
  (*header)[1][0] = '+';
  (*header)[1][strlen((*header)[1]) - 1] = '-';
#endif

  return headers;
}

void copyAndPad(char *target, char *source, long sourceWidth, long targetWidth) {
  long i, head, tail, excess;
  if ((excess = targetWidth - sourceWidth) < 0)
    SDDS_Bomb("negative excess in copyAndPad()---programming error");
  head = tail = excess / 2;
  if ((i = excess - head - tail))
    head += i;
  for (i = 0; i < head; i++)
    target[i] = ' ';
  target[i] = 0;
  if (source)
    strcat(target, source);
  for (i = strlen(target); i < targetWidth; i++)
    target[i] = ' ';
  target[i] = 0;
}

/*
  void copyAndPad(char *target, char *source, long sourceWidth, long targetWidth) {
  long i, head, tail, excess;
  if (source && *source!='@') {
  if ((excess = targetWidth - sourceWidth) < 0)
  SDDS_Bomb("negative excess in copyAndPad()---programming error");
  head = tail = excess / 2;
  if ((i = excess - head - tail))
  head += i;
  for (i = 0; i < head; i++)
  target[i] = ' ';
  target[i] = 0;
  if (source)
  strcat(target, source);
  for (i = strlen(target); i < targetWidth; i++)
  target[i] = ' ';
  target[i] = 0;
  } else if (source)
  strcat(target, source);
  }
*/

char *makeParameterLabel(long *fieldWidth, char *name, char *editLabel, char *units, double factor, char *format) {
  long formatWidth, labelWidth, extraFormatChars;
  char *label;
  static char buffer[1024];

  if (editLabel) {
    strcpy(buffer, name);
    if (!edit_string(buffer, editLabel))
      SDDS_Bomb("Problem editing parameter label");
    name = buffer;
  }

  *fieldWidth = (labelWidth = strlen(name) + 4 + (units && !SDDS_StringIsBlank(units) ? strlen(units) + 3 : 0)) + (formatWidth = getFormatFieldLength(format, &extraFormatChars));
  *fieldWidth += extraFormatChars;
  if (*fieldWidth < 2)
    *fieldWidth = 2;
  label = tmalloc(sizeof(*label) * (labelWidth + 1));
  if (factor != 1)
    units = modifyUnitsWithFactor(units, 1. / factor, 0);
  if (units && !SDDS_StringIsBlank(units))
    sprintf(label, "%s (%s) = ", name, units);
  else
    sprintf(label, "%s = ", name);
  return label;
}

long getFormatFieldLength(char *format, long *extraChars) {
  long width;
  char *format0;

  if (*format == '@')
    return 10;
  format0 = format;
  while (*format && *format != '%')
    format++;
  if (*format != '%') {
    fprintf(stderr, "Bad format string: %s\n", format0);
    exit(EXIT_FAILURE);
  }
  if (extraChars)
    *extraChars = format - format0;
  while (*format && !isdigit(*format))
    if (*format == '.')
      break;
    else
      format++;
  if (!*format || !isdigit(*format))
    width = 0;
  else {
    width = atoi(format);
  }
  while (isdigit(*format) || *format == '.')
    format++;
  if (*format == 'l' || *format == 'h')
    format++;
  if (*format)
    format++;
  if (extraChars)
    *extraChars += strlen(format);

  return width;
}

void doPrintParameters(SDDS_DATASET *inTable, PRINT_PARAMETER *printParameter, long printParameters, long width,
                       FILE *fpOut, unsigned long spreadsheetFlags, char *spreadsheetDelimiter,
                       char *spreadsheetQuoteMark, PAGINATION *pagination, char *title, long noLabels) {
  long parameter, outputRow, length;
  char printBuffer[2 * SDDS_MAXLINE];
  static void **dataBuffer = NULL;
  char *format;

  if (!printParameters)
    return;
  if (!dataBuffer && !(dataBuffer = malloc(sizeof(void *) * 4)))
    SDDS_Bomb("Allocation failure in doPrintParameters");

  outputRow = 0;
  for (parameter = 0; parameter < printParameters; parameter++) {
    format = NULL;
    if (printParameter[parameter].format && printParameter[parameter].format[0] == '@' &&
        !(format = SDDS_GetParameterAsString(inTable, printParameter[parameter].format + 1, NULL)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_GetParameter(inTable, printParameter[parameter].name, &dataBuffer[0]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_SprintTypedValueFactor(dataBuffer, 0, printParameter[parameter].type,
                                     format ? format : printParameter[parameter].format, printBuffer,
                                     SDDS_PRINT_NOQUOTES, printParameter[parameter].factor))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (spreadsheetFlags & SPREADSHEET_ON) {
      if (!(spreadsheetFlags & SPREADSHEET_NOLABELS))
        fprintf(fpOut, "%s%s", printParameter[parameter].name, spreadsheetDelimiter);
      fprintf(fpOut, "%s%s%s\n", spreadsheetQuoteMark, printBuffer, spreadsheetQuoteMark);
      continue;
    }
    length = strlen(printParameter[parameter].label) + strlen(printBuffer) + (parameter ? 2 : 0);
    if ((parameter && printParameter[parameter - 1].endsLine) || (outputRow && (outputRow + length) > width)) {
      fputc('\n', fpOut);
      checkPagination(fpOut, pagination, title);
      outputRow = length - 2;
      if (parameter && printParameter[parameter - 1].blankLines && printParameter[parameter - 1].endsLine) {
        long i;
        for (i = 1; i < printParameter[parameter - 1].blankLines; i++) {
          checkPagination(fpOut, pagination, title);
          fputc('\n', fpOut);
        }
      }
    } else {
      if (parameter)
        fputs("  ", fpOut);
      outputRow += length;
    }
    fputs(printParameter[parameter].label, fpOut);
    fputs(printBuffer, fpOut);
    if (printParameter[parameter].type == SDDS_STRING && dataBuffer[0])
      free((char *)dataBuffer[0]);
  }
  if (!(spreadsheetFlags & SPREADSHEET_ON)) {
    checkPagination(fpOut, pagination, title);
    fputc('\n', fpOut);
    checkPagination(fpOut, pagination, title);
    fputc('\n', fpOut);
  }
}

char **makeListOfNames(char *string, long *names) {
  char **name, *ptr;
  long iName;

  *names = characterCount(string, ',') + 1;
  name = tmalloc(sizeof(*name) * *names);
  for (iName = 0; string && iName < *names; iName++) {
    if ((ptr = strchr(string, ',')))
      *ptr++ = 0;
    SDDS_CopyString(&name[iName], string);
    string = ptr;
  }
  if (iName != *names)
    SDDS_Bomb("problem occurred scanning list of names");
  return name;
}

long characterCount(char *string, char c) {
  char *ptr;
  long count = 0;
  while ((ptr = strchr(string, c))) {
    count++;
    string = ptr + 1;
  }
  return count;
}

long printPageTitle(FILE *fpOut, char *title) {
  long lines;
  char *ptr;

  fprintf(fpOut, "%s\n", title);
  ptr = title;
  lines = 1;
  while ((ptr = strchr(ptr, '\n'))) {
    ptr++;
    lines++;
  }
  return lines;
}

long checkPagination(FILE *fpOut, PAGINATION *pagination, char *title) {
  if (!(pagination->flags & PAGINATION_ON))
    return 0;
  if ((pagination->currentLine += 1) >= pagination->lines) {
    fputc('\014', fpOut);
    pagination->currentLine = 0;
    if (!(pagination->flags & PAGINATION_NOTITLE))
      pagination->currentLine += printPageTitle(fpOut, title);
    return 1;
  }
  return 0;
}

void printColumnHeaders(FILE *fpOut, PRINT_COLUMN *printColumn, long printColumns, long width,
                        PAGINATION *pagination, long latexFormat, char *latexTitle, long htmlFormat, char *htmlTitle) {
  char *label;
  long row, column, header, outputRow, maxOutputRow, length, noUnitsLine;

  if (latexFormat) {
    if (latexFormat & LATEX_LONGTABLE) {
      fprintf(fpOut, "\\begin{longtable}{");
      if (latexFormat & LATEX_JUSTIFY) {
        fprintf(fpOut, "%s", latexJustify);
      } else {
        for (column = 0; column < printColumns; column++)
          fprintf(fpOut, "%s", latexFormat & LATEX_BOOKTABLE ? "c" : (column == 0 ? "|c|" : "c|"));
      }
      fprintf(fpOut, "}\n");
      if (latexTitle && strlen(latexTitle)) {
        fprintf(fpOut, "\\caption{%s}\\\\\n", latexTitle ? makeTexSafeString(latexTitle) : "No caption");
      }
    } else {
      fprintf(fpOut, "\\begin{%s}[htb]", latexFormat & LATEX_SIDEWAYS ? "sidewaystable" : "table");
      if (latexTitle && strlen(latexTitle)) {
        fprintf(fpOut, "\\caption{%s}\n", latexTitle ? makeTexSafeString(latexTitle) : "No caption");
      } else
        fprintf(fpOut, "\n");
      fprintf(fpOut, "\\begin{center}\n");
      fprintf(fpOut, "\\begin{tabular}{");
      if (latexFormat & LATEX_JUSTIFY) {
        fprintf(fpOut, "%s", latexJustify);
      } else {
        for (column = 0; column < printColumns; column++)
          fprintf(fpOut, "%s", latexFormat & LATEX_BOOKTABLE ? "c" : (column == 0 ? "|c|" : "c|"));
      }
      fprintf(fpOut, "}\n");
    }

    if (latexFormat & LATEX_BOOKTABLE)
      fprintf(fpOut, "\\toprule\n");
    else
      fprintf(fpOut, "\\hline\n");
  }
  if (htmlFormat) {
    fprintf(fpOut, "<table style=\"width:100%%\">\n");
    if (htmlTitle && strlen(htmlTitle)) {
      fprintf(fpOut, "  <caption>%s</caption>\n", htmlTitle);
    }
  }
  noUnitsLine = 1;
  header = 1;
  for (column = 0; column < printColumns; column++) {
    if (!SDDS_StringIsBlank(printColumn[column].header[header]))
      break;
  }
  if (column != printColumns)
    noUnitsLine = 0;

  maxOutputRow = 0;
  for (header = 0; header < printColumn[0].headers; header++) {
    outputRow = 0;
    if (noUnitsLine && header == 1)
      continue;
    if (htmlFormat) {
      fprintf(fpOut, "  <tr>\n");
    }
    for (column = 0; column < printColumns; column++) {
      label = printColumn[column].header[header];
      length = strlen(label);
      if (!latexFormat && !htmlFormat && ((column && printColumn[column - 1].endsLine) || (width && outputRow && (outputRow + length) > width))) {
        printColumn[column - 1].endsLine = 1;
        fputc('\n', fpOut);
        pagination->currentLine++;
        outputRow = 0;
      }
      if (latexFormat) {
        if (latexFormat & LATEX_TRANSLATE && strchr(label, '$'))
          fputs(label, fpOut);
        else if (strpbrk(label, "_^{}\\"))
          fprintf(fpOut, "$%s$", label);
        else
          fputs(makeTexSafeString(printColumn[column].header[header]), fpOut);
        fputs(column == printColumns - 1 ? (latexFormat & LATEX_BOOKTABLE ? " \\\\ \n" : " \\\\ \\hline") : " & ", fpOut);
      } else if (htmlFormat) {
        fprintf(fpOut, "    <th>%s</th>\n", trim_spaces(label));
      } else {
        fputs(label, fpOut);
        outputRow += length;
        if (outputRow > maxOutputRow)
          maxOutputRow = outputRow;
      }
    }
    if (htmlFormat) {
      fprintf(fpOut, "  </tr>\n");
    }
    fputc('\n', fpOut);
    pagination->currentLine++;
  }
  if (!latexFormat && !htmlFormat) {
    for (row = 0; row < maxOutputRow; row++)
      fputc('-', fpOut);
    fputc('\n', fpOut);
  } else if (latexFormat & LATEX_BOOKTABLE) {
    fputs("\\midrule\n", fpOut);
  }
  pagination->currentLine++;
}

void doPrintColumns(SDDS_DATASET *inTable, PRINT_COLUMN *printColumn, long printColumns,
                    long width, FILE *fpOut, unsigned long spreadsheetFlags, char *spreadsheetDelimiter, char *spreadsheetQuoteMark,
                    long latexFormat, char *latexTitle, char *latexLabel, char *latexGroupColumn,
                    long htmlFormat, char *htmlTitle,
                    PAGINATION *pagination, char *title, long noLabels) {
  long column;
  int64_t row, rows, nGroups, maxGroupLength, groupLength;
  void **data;
  char **groupData = NULL;
  char ***format = NULL;
  char printBuffer[16 * SDDS_MAXLINE];
  char *t;
  if (!printColumns)
    return;

  if ((rows = SDDS_CountRowsOfInterest(inTable)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  else if (!rows)
    return;

  if ((latexTitle != NULL) && (latexTitle[0] == '@')) {
    latexTitle = latexTitle + 1;
    t = SDDS_GetParameterAsString(inTable, latexTitle, &t);
    latexTitle = t;
  }
  if ((htmlTitle != NULL) && (htmlTitle[0] == '@')) {
    htmlTitle = htmlTitle + 1;
    t = SDDS_GetParameterAsString(inTable, htmlTitle, &t);
    htmlTitle = t;
  }
  if (!(spreadsheetFlags & SPREADSHEET_ON) && !noLabels)
    printColumnHeaders(fpOut, printColumn, printColumns, width, pagination,
                       latexFormat, latexTitle ? latexTitle : title,
                       htmlFormat, htmlTitle ? htmlTitle : title);
  else if (!(spreadsheetFlags & SPREADSHEET_NOLABELS) && !noLabels) {
    for (column = 0; column < printColumns; column++)
      fprintf(fpOut, "%s%s", printColumn[column].name, column != printColumns - 1 ? spreadsheetDelimiter : "\n");
  }

  nGroups = 1;
  maxGroupLength = groupLength = 1;
  if (latexFormat & LATEX_GROUP) {
    if (!(groupData = SDDS_GetColumn(inTable, latexGroupColumn))) {
      fprintf(stderr, "Error: unable to get data for column %s\n", latexGroupColumn);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    for (row = 1; row < rows; row++) {
      if (strcmp(groupData[row], groupData[row - 1])) {
        if (groupLength > maxGroupLength)
          maxGroupLength = groupLength;
        groupLength = 1;
        nGroups++;
      } else {
        groupLength++;
      }
    }
    if (groupLength > maxGroupLength)
      maxGroupLength = groupLength;
    if (latexFormat)
      fprintf(fpOut, "%% nGroups = %" PRId64 ", maxGroupLength = %" PRId64 "\n", nGroups, maxGroupLength);
  }

  data = tmalloc(sizeof(*data) * printColumns);
  format = tmalloc(sizeof(*format) * printColumns);
  for (column = 0; column < printColumns; column++) {
    if (!(data[column] = SDDS_GetColumn(inTable, printColumn[column].name))) {
      fprintf(stderr, "Error: unable to get data for column %s\n", printColumn[column].name);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    format[column] = NULL;
    if (printColumn[column].format && printColumn[column].format[0] == '@') {
      if (!(format[column] = SDDS_GetColumn(inTable, printColumn[column].format + 1))) {
        fprintf(stderr, "Error: unable to get format data for column %s from %s\n",
                printColumn[column].name, printColumn[column].format + 1);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }
  for (row = 0; row < rows; row++) {
    if (htmlFormat) {
      if (row % 2 == 0)
        fprintf(fpOut, "  <tr bgcolor=\"#ddd\">\n");
      else
        fprintf(fpOut, "  <tr>\n");
    }
    if (latexFormat && nGroups > 1) {
      if (row == 0 || strcmp(groupData[row], groupData[row - 1])) {
        if (row != 0 && latexFormat & LATEX_BOOKTABLE)
          fprintf(fpOut, "\\midrule\n");
        fprintf(fpOut, "\\multicolumn{%ld}{l}{\\bf %s} \\\\ \n", printColumns, groupData[row]);
      }
    }
    for (column = 0; column < printColumns; column++) {
      if (htmlFormat) {
        fprintf(fpOut, "    <td style=\"text-align:center\">");
      }
      if (printColumn[column].type == SDDS_STRING && SDDS_StringIsBlank(*((char **)data[column] + row))) {
        sprintf(printBuffer, format[column] ? format[column][row] : printColumn[column].format, " ");
      } else {
        if (!SDDS_SprintTypedValueFactor(data[column], row, printColumn[column].type,
                                         format[column] ? format[column][row] : printColumn[column].format,
                                         printBuffer, SDDS_PRINT_NOQUOTES,
                                         printColumn[column].factor)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      if (spreadsheetFlags & SPREADSHEET_ON) {
        fprintf(fpOut, "%s%s%s%s", spreadsheetQuoteMark, printBuffer, spreadsheetQuoteMark, column != printColumns - 1 ? spreadsheetDelimiter : "\n");
        continue;
      }
      if (latexFormat) {
        if (SDDS_FLOATING_TYPE(printColumn[column].type))
          fputs(makeTexExponentialString(printBuffer), fpOut);
        else
          fputs(makeTexSafeString(printBuffer), fpOut);
        fputs(column != printColumns - 1 ? " & " : (latexFormat & LATEX_BOOKTABLE ? " \\\\ " : " \\\\ \\hline"), fpOut);
        continue;
      }
      if (htmlFormat) {
        fputs(trim_spaces(printBuffer), fpOut);
        fprintf(fpOut, "</td>\n");
        continue;
      }
      fputs(printBuffer, fpOut);
      if (printColumn[column].endsLine) {
        long i;
        fputc('\n', fpOut);
        if (checkPagination(fpOut, pagination, title))
          printColumnHeaders(fpOut, printColumn, printColumns, width, pagination, latexFormat, latexTitle ? latexTitle : title, htmlFormat, htmlTitle ? htmlTitle : title);
        for (i = 0; i < printColumn[column].blankLines; i++) {
          fputc('\n', fpOut);
          if (checkPagination(fpOut, pagination, title))
            printColumnHeaders(fpOut, printColumn, printColumns, width, pagination, latexFormat, latexTitle ? latexTitle : title, htmlFormat, htmlTitle ? htmlTitle : title);
        }
      }
    }
    if (htmlFormat) {
      fprintf(fpOut, "  </tr>\n");
    } else {
      if (!(spreadsheetFlags & SPREADSHEET_ON) && !printColumn[column - 1].endsLine) {
        fputc('\n', fpOut);
        if (!latexFormat && !htmlFormat && checkPagination(fpOut, pagination, title))
          printColumnHeaders(fpOut, printColumn, printColumns, width, pagination, latexFormat, latexTitle ? latexTitle : title, htmlFormat, htmlTitle ? htmlTitle : title);
      }
    }
  }
  if (latexFormat) {
    if (latexFormat & LATEX_BOOKTABLE)
      fputs("\\bottomrule\n", fpOut);
    if (latexFormat & LATEX_LONGTABLE) {
      if (latexLabel)
        fprintf(fpOut, "\\label{%s}\n", latexLabel);
      else
        fprintf(fpOut, "%%\\label{%s}\n", "tab:labelHere");
      fprintf(fpOut, "\\end{longtable}\n");
    } else {
      fprintf(fpOut, "\\end{tabular}\n");
      fprintf(fpOut, "\\end{center}\n");
      if (latexLabel)
        fprintf(fpOut, "\\label{%s}\n", latexLabel);
      else
        fprintf(fpOut, "%%\\label{%s}\n", "tab:labelHere");
      fprintf(fpOut, "\\end{%s}\n", latexFormat & LATEX_SIDEWAYS ? "sidewaystable" : "table");
    }
  }
  if (htmlFormat) {
    fprintf(fpOut, "</table><br>\n");
  }
  for (column = 0; column < printColumns; column++) {
    free(data[column]);
    if (format[column])
      free(format[column]);
  }
  free(data);
  free(format);
}

void replaceFormatWidth(char *buffer, char *format, long width) {
  char *ptr;
  ptr = format;
  while (*ptr) {
    if (*ptr == '%' && *(ptr + 1) != '%')
      break;
    ptr++;
  }
  if (*ptr) {
    *ptr = 0;
    if (*(ptr + 1) == '-') {
      sprintf(buffer, "%s%%-%ld", format, width);
      ptr++;
    } else
      sprintf(buffer, "%s%%%ld", format, width);
    ptr++;
    while (*ptr && isdigit(*ptr))
      ptr++;
    strcat(buffer, ptr);
  } else {
    strcpy(buffer, format);
  }
}

void CreateSCHFile(char *output, char *input, unsigned long flags, char *delimiter,
                   char *quote, PRINT_COLUMN *printColumn, long printColumns) {
  FILE *fp;
  long i;

  if (!(fp = fopen(output, "w")))
    SDDS_Bomb("Couldn't open SCHFile for writing.");
  fprintf(fp, "[%s]\nFiletype=Delimited\nDelimiter=%s\nSeparator=%s\nCharSet=ascii\n", input ? input : "NULL", quote, delimiter);
  for (i = 0; i < printColumns; i++) {
    printColumn[i].useDefaultFormat = 1;
    fprintf(fp, "Field%ld=%s,%s,00,00,00\n\n", i + 1, printColumn[i].name, SDDS_NUMERIC_TYPE(printColumn[i].type) ? "Float" : "Char");
  }
}

char *makeTexSafeString(char *source) {
  static char *buffer = NULL;
  static long buflen = 0;
  long index = 0, length, inMath = 0;
  if (!source)
    return source;
  length = strlen(source);
  if (length > (buflen - 2)) {
    buflen = length * 2 + 2;
    if (!(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * buflen)))
      SDDS_Bomb("memory allocation failure");
  }
  buffer[0] = 0;
  while (*source) {
    if (*source == '_' || *source == '^' || *source == '{' || *source == '}' || *source == '%' || *source == '#') {
      if (!inMath)
        buffer[index++] = '\\';
      buffer[index++] = *source++;
    } else if (*source == '<' || *source == '>' || *source == '|') {
      if (!inMath)
        buffer[index++] = '$';
      buffer[index++] = *source++;
      if (!inMath)
        buffer[index++] = '$';
    } else {
      if (*source == '$')
        inMath = !inMath;
      buffer[index++] = *source++;
    }
    if (index >= (buflen - 1)) {
      buflen = index * 2;
      if (!(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * buflen)))
        SDDS_Bomb("memory allocation failure");
    }
  }
  buffer[index] = 0;
  return buffer;
}

char *makeTexExponentialString(char *text) {
  char *ptr1, *ptr2;
  char buffer[100];
  long exponent;
  if ((ptr1 = ptr2 = strchr(text, 'e')) || (ptr1 = ptr2 = strchr(text, 'E'))) {
    trim_spaces(ptr2);
    while (*ptr2 && !(isdigit(*ptr2) || *ptr2 == '-'))
      ptr2++;
    if (!*ptr2)
      return text;
    if (strlen(ptr2) >= 100)
      SDDS_Bomb("buffer overflow in makeTexExponentialString");
    sscanf(ptr2, "%ld", &exponent);
    *ptr1 = 0;
    if (exponent != 0) {
      sprintf(buffer, "%ld", exponent);
      sprintf(ptr1, "$\\times 10^{%s}$", buffer);
    }
  }
  return text;
}

htab *readTranslationTable(char *TranslationFile) {
#if defined(_WIN32)
  fprintf(stderr, "The latex and html options in sddsprintout are not available on Windows\n");
  exit(EXIT_FAILURE);
#else
  htab *ht;
  SDDS_DATASET SDDSin;
  char **oldName, **newName;
  int64_t rows;
  ht = hcreate(12);
  if (!SDDS_InitializeInput(&SDDSin, TranslationFile) ||
      SDDS_ReadPage(&SDDSin) < 0 ||
      (rows = SDDS_RowCount(&SDDSin)) <= 0 ||
      !(oldName = SDDS_GetColumn(&SDDSin, "OldName")) ||
      !(newName = SDDS_GetColumn(&SDDSin, "NewName"))) {
    SDDS_Bomb("Problem with translation file");
  } else {
    while (--rows >= 0)
      hadd(ht, (ub1 *)oldName[rows], (ub4)strlen(oldName[rows]), newName[rows]);
  }
  return ht;
#endif
}

char *findTranslation(htab *ht, char *key) {
#if defined(_WIN32)
  SDDS_Bomb("The latex and html options in sddsprintout are not available on Windows");
#else
  if (key && ht && hfind(ht, (ub1 *)key, (ub4)strlen(key)) == TRUE)
    return (char *)hstuff(ht);
#endif
  return key;
}

char *modifyUnitsWithFactor(char *units0, double factor, unsigned long latexFormat) {
  char *units, buffer[100];

  units = tmalloc(sizeof(*units) * ((units0 ? strlen(units0) : 0) + 100));

  if (latexFormat) {
    sprintf(buffer, "%.2g", factor);
    sprintf(units, "%s%s", makeTexExponentialString(buffer), units0 ? units0 : "");
  } else {
    sprintf(units, "%.2g%s", factor, units0 ? units0 : "");
  }
  return units;
}
