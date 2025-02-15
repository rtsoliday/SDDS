/**
 * @file sdds_write_demo.c
 * @brief Example program demonstrating how to write data to an SDDS file using various data types.
 *
 * This program writes an ASCII SDDS file with parameters, arrays, and columns with different data types,
 * writes two pages of data, and then terminates the SDDS file.
 */

#include <stdio.h>
#include "SDDS.h"

/**
 * @brief Main function to write data to an SDDS file.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Returns 0 on success, 1 on failure.
 */
int main(int argc, char **argv) {
    SDDS_TABLE SDDS_table; /**< SDDS table structure for data output. */

    /** Initialize SDDS output */
    if (!SDDS_InitializeOutput(&SDDS_table, SDDS_ASCII, 1, "Example SDDS Output",
                               "SDDS Example", "example.sdds")) {
        fprintf(stderr, "Error initializing SDDS output.\n");
        return 1;
    }

    /** 
     * Define parameters for the SDDS table.
     *
     * Parameters are scalar values associated with each page.
     */
    if (
        SDDS_DefineParameter(&SDDS_table, "shortParam",      NULL, NULL, NULL, NULL, SDDS_SHORT,      NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "ushortParam",     NULL, NULL, NULL, NULL, SDDS_USHORT,     NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "longParam",       NULL, NULL, NULL, NULL, SDDS_LONG,       NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "ulongParam",      NULL, NULL, NULL, NULL, SDDS_ULONG,      NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "long64Param",     NULL, NULL, NULL, NULL, SDDS_LONG64,     NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "ulong64Param",    NULL, NULL, NULL, NULL, SDDS_ULONG64,    NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "floatParam",      NULL, NULL, NULL, NULL, SDDS_FLOAT,      NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "doubleParam",     NULL, NULL, NULL, NULL, SDDS_DOUBLE,     NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "longdoubleParam", NULL, NULL, NULL, NULL, SDDS_LONGDOUBLE, NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "stringParam",     NULL, NULL, NULL, NULL, SDDS_STRING,     NULL) == -1 ||
        SDDS_DefineParameter(&SDDS_table, "charParam",       NULL, NULL, NULL, NULL, SDDS_CHARACTER,  NULL) == -1
    ) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** 
     * Define arrays for the SDDS table.
     *
     * Arrays are multidimensional data structures associated with each page.
     */
    if (
        SDDS_DefineArray(&SDDS_table, "shortArray",      NULL, NULL, NULL, NULL, SDDS_SHORT,      0, 1, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "ushortArray",     NULL, NULL, NULL, NULL, SDDS_USHORT,     0, 1, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "longArray",       NULL, NULL, NULL, NULL, SDDS_LONG,       0, 1, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "ulongArray",      NULL, NULL, NULL, NULL, SDDS_ULONG,      0, 1, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "long64Array",     NULL, NULL, NULL, NULL, SDDS_LONG64,     0, 2, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "ulong64Array",    NULL, NULL, NULL, NULL, SDDS_ULONG64,    0, 2, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "floatArray",      NULL, NULL, NULL, NULL, SDDS_FLOAT,      0, 2, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "doubleArray",     NULL, NULL, NULL, NULL, SDDS_DOUBLE,     0, 2, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "longdoubleArray", NULL, NULL, NULL, NULL, SDDS_LONGDOUBLE, 0, 2, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "stringArray",     NULL, NULL, NULL, NULL, SDDS_STRING,     0, 2, NULL) == -1 ||
        SDDS_DefineArray(&SDDS_table, "charArray",       NULL, NULL, NULL, NULL, SDDS_CHARACTER,  0, 2, NULL) == -1
    ) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** 
     * Define columns for the SDDS table.
     *
     * Columns are data structures associated with each row of data.
     */
    if (
        SDDS_DefineColumn(&SDDS_table, "shortCol",      NULL, NULL, NULL, NULL, SDDS_SHORT,      0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "ushortCol",     NULL, NULL, NULL, NULL, SDDS_USHORT,     0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "longCol",       NULL, NULL, NULL, NULL, SDDS_LONG,       0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "ulongCol",      NULL, NULL, NULL, NULL, SDDS_ULONG,      0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "long64Col",     NULL, NULL, NULL, NULL, SDDS_LONG64,     0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "ulong64Col",    NULL, NULL, NULL, NULL, SDDS_ULONG64,    0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "floatCol",      NULL, NULL, NULL, NULL, SDDS_FLOAT,      0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "doubleCol",     NULL, NULL, NULL, NULL, SDDS_DOUBLE,     0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "longdoubleCol", NULL, NULL, NULL, NULL, SDDS_LONGDOUBLE, 0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "stringCol",     NULL, NULL, NULL, NULL, SDDS_STRING,     0) == -1 ||
        SDDS_DefineColumn(&SDDS_table, "charCol",       NULL, NULL, NULL, NULL, SDDS_CHARACTER,  0) == -1
    ) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** Write the layout (definitions) to the SDDS file */
    if (!SDDS_WriteLayout(&SDDS_table)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** Start the first page with 5 rows */
    if (!SDDS_StartPage(&SDDS_table, 5)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** 
     * Set parameters for the first page.
     *
     * Parameters are scalar values that apply to the entire page.
     */
    if (!SDDS_SetParameters(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "shortParam", (short)10,
                            "ushortParam", (unsigned short)11,
                            "longParam", (int32_t)1000,
                            "ulongParam", (uint32_t)1001,
                            "long64Param", (int64_t)1002,
                            "ulong64Param", (uint64_t)1003,
                            "floatParam", (float)3.14f,
                            "doubleParam", (double)2.71828,
                            "longdoubleParam", (long double)1.1L,
                            "stringParam", "FirstPage",
                            "charParam", 'A',
                            NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** 
     * Data for arrays on the first page.
     *
     * Arrays can have multiple dimensions and are associated with each page.
     */
    int32_t dimension[1]  = {3}; /**< Dimensions for 1D arrays */
    int32_t dimensionB[2] = {4, 2}; /**< Dimensions for 2D arrays */
    short shortArrayData[3]            = {1, 2, 3};
    unsigned short ushortArrayData[3]  = {4, 5, 6};
    int32_t longArrayData[3]           = {1000, 2000, 3000};
    uint32_t ulongArrayData[3]         = {1001, 2001, 3001};
    int64_t long64ArrayData[8]         = {1002, 2002, 3002, 4002, 5002, 6002, 7002, 8002};
    uint64_t ulong64ArrayData[8]       = {1003, 2003, 3003, 4003, 5003, 6003, 7003, 8003};
    float floatArrayData[8]            = {1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f};
    double doubleArrayData[8]          = {1.2, 2.2, 3.2, 4.2, 5.2, 6.2, 7.2, 8.2};
    long double longdoubleArrayData[8] = {1.3L, 2.3L, 3.3L, 4.3L, 5.3L, 6.3L, 7.3L, 8.3L};
    char *stringArrayData[8]           = {"one", "two", "three", "four", "five", "six", "seven", "eight"};
    char charArrayData[8]              = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};

    /** Set arrays for the first page */
    if (
        !SDDS_SetArray(&SDDS_table, "shortArray",      SDDS_CONTIGUOUS_DATA, shortArrayData,      dimension) ||
        !SDDS_SetArray(&SDDS_table, "ushortArray",     SDDS_CONTIGUOUS_DATA, ushortArrayData,     dimension) ||
        !SDDS_SetArray(&SDDS_table, "longArray",       SDDS_CONTIGUOUS_DATA, longArrayData,       dimension) ||
        !SDDS_SetArray(&SDDS_table, "ulongArray",      SDDS_CONTIGUOUS_DATA, ulongArrayData,      dimension) ||
        !SDDS_SetArray(&SDDS_table, "long64Array",     SDDS_CONTIGUOUS_DATA, long64ArrayData,     dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "ulong64Array",    SDDS_CONTIGUOUS_DATA, ulong64ArrayData,    dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "floatArray",      SDDS_CONTIGUOUS_DATA, floatArrayData,      dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "doubleArray",     SDDS_CONTIGUOUS_DATA, doubleArrayData,     dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "longdoubleArray", SDDS_CONTIGUOUS_DATA, longdoubleArrayData, dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "stringArray",     SDDS_CONTIGUOUS_DATA, stringArrayData,     dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "charArray",       SDDS_CONTIGUOUS_DATA, charArrayData,       dimensionB)
    ) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** 
     * Data for columns on the first page.
     *
     * Columns represent data for each row.
     */
    int64_t rows = 5; /**< Number of rows in the first page */
    short shortColData[5]            = {1, 2, 3, 4, 5};
    unsigned short ushortColData[5]  = {1, 2, 3, 4, 5};
    int32_t longColData[5]           = {100, 200, 300, 400, 500};
    uint32_t ulongColData[5]         = {100, 200, 300, 400, 500};
    int64_t long64ColData[5]         = {100, 200, 300, 400, 500};
    uint64_t ulong64ColData[5]       = {100, 200, 300, 400, 500};
    float floatColData[5]            = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
    double doubleColData[5]          = {10.01, 20.02, 30.03, 40.04, 50.05};
    long double longdoubleColData[5] = {10.01L, 20.02L, 30.03L, 40.04L, 50.05L};
    char *stringColData[5]           = {"one", "two", "three", "four", "five"};
    char charColData[5]              = {'a', 'b', 'c', 'd', 'e'};

    /** Set columns for the first page */
    if (
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, shortColData,      rows, "shortCol")      ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, ushortColData,     rows, "ushortCol")     ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, longColData,       rows, "longCol")       ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, ulongColData,      rows, "ulongCol")      ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, long64ColData,     rows, "long64Col")     ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, ulong64ColData,    rows, "ulong64Col")    ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, floatColData,      rows, "floatCol")      ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, doubleColData,     rows, "doubleCol")     ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, longdoubleColData, rows, "longdoubleCol") ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, stringColData,     rows, "stringCol")     ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, charColData,       rows, "charCol")
    ) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** Write the first page to the SDDS file */
    if (!SDDS_WritePage(&SDDS_table)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** Start the second page with 3 rows */
    if (!SDDS_StartPage(&SDDS_table, 3)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** 
     * Set parameters for the second page.
     */
    if (!SDDS_SetParameters(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "shortParam", (short)20,
                            "ushortParam", (unsigned short)21,
                            "longParam", (int32_t)2000,
                            "ulongParam", (uint32_t)2001,
                            "long64Param", (int64_t)2002,
                            "ulong64Param", (uint64_t)2003,
                            "floatParam", (float)6.28f,
                            "doubleParam", (double)1.41421,
                            "longdoubleParam", (long double)2.2L,
                            "stringParam", "SecondPage",
                            "charParam", 'B',
                            NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** 
     * Data for arrays on the second page.
     */
    dimension[0]  = 2;      /**< Update dimensions for 1D arrays */
    dimensionB[0] = 2;      /**< Update dimensions for 2D arrays */
    dimensionB[1] = 2;
    short shortArrayData2[2]            = {7, 8};
    unsigned short ushortArrayData2[2]  = {9, 10};
    int32_t longArrayData2[2]           = {4000, 5000};
    uint32_t ulongArrayData2[2]         = {4001, 5001};
    int64_t long64ArrayData2[4]         = {4002, 5002, 6002, 7002};
    uint64_t ulong64ArrayData2[4]       = {4003, 5003, 6003, 7003};
    float floatArrayData2[4]            = {11.11f, 22.22f, 33.33f, 44.44f};
    double doubleArrayData2[4]          = {33.33, 44.44, 55.55, 66.66};
    long double longdoubleArrayData2[4] = {55.55L, 66.66L, 77.77L, 88.88L};
    char *stringArrayData2[4]           = {"blue", "red", "yellow", "gold"};
    char charArrayData2[4]              = {'W', 'X', 'Y', 'Z'};

    /** Set arrays for the second page */
    if (
        !SDDS_SetArray(&SDDS_table, "shortArray",      SDDS_CONTIGUOUS_DATA, shortArrayData2,      dimension) ||
        !SDDS_SetArray(&SDDS_table, "ushortArray",     SDDS_CONTIGUOUS_DATA, ushortArrayData2,     dimension) ||
        !SDDS_SetArray(&SDDS_table, "longArray",       SDDS_CONTIGUOUS_DATA, longArrayData2,       dimension) ||
        !SDDS_SetArray(&SDDS_table, "ulongArray",      SDDS_CONTIGUOUS_DATA, ulongArrayData2,      dimension) ||
        !SDDS_SetArray(&SDDS_table, "long64Array",     SDDS_CONTIGUOUS_DATA, long64ArrayData2,     dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "ulong64Array",    SDDS_CONTIGUOUS_DATA, ulong64ArrayData2,    dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "floatArray",      SDDS_CONTIGUOUS_DATA, floatArrayData2,      dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "doubleArray",     SDDS_CONTIGUOUS_DATA, doubleArrayData2,     dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "longdoubleArray", SDDS_CONTIGUOUS_DATA, longdoubleArrayData2, dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "stringArray",     SDDS_CONTIGUOUS_DATA, stringArrayData2,     dimensionB) ||
        !SDDS_SetArray(&SDDS_table, "charArray",       SDDS_CONTIGUOUS_DATA, charArrayData2,       dimensionB)
    ) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** 
     * Data for columns on the second page.
     */
    rows = 3; /**< Number of rows in the second page */
    short shortColData2[3]            = {6, 7, 8};
    unsigned short ushortColData2[3]  = {6, 7, 8};
    int32_t longColData2[3]           = {600, 700, 800};
    uint32_t ulongColData2[3]         = {600, 700, 800};
    int64_t long64ColData2[3]         = {600, 700, 800};
    uint64_t ulong64ColData2[3]       = {600, 700, 800};
    float floatColData2[3]            = {6.6f, 7.7f, 8.8f};
    double doubleColData2[3]          = {60.06, 70.07, 80.08};
    long double longdoubleColData2[3] = {60.06L, 70.07L, 80.08L};
    char *stringColData2[3]           = {"six", "seven", "eight"};
    char charColData2[3]              = {'f', 'g', 'h'};

    /** Set columns for the second page */
    if (
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, shortColData2,      rows, "shortCol")      ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, ushortColData2,     rows, "ushortCol")     ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, longColData2,       rows, "longCol")       ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, ulongColData2,      rows, "ulongCol")      ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, long64ColData2,     rows, "long64Col")     ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, ulong64ColData2,    rows, "ulong64Col")    ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, floatColData2,      rows, "floatCol")      ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, doubleColData2,     rows, "doubleCol")     ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, longdoubleColData2, rows, "longdoubleCol") ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, stringColData2,     rows, "stringCol")     ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, charColData2,       rows, "charCol")
    ) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** Write the second page to the SDDS file */
    if (!SDDS_WritePage(&SDDS_table)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
    }

    /** Terminate the SDDS table */
    SDDS_Terminate(&SDDS_table);

    return 0;
}
