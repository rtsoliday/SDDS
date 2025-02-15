/**
 * @file sdds_read_demo.c
 * @brief A program to read and process SDDS files, displaying parameters, columns, and arrays.
 *
 * This program reads an SDDS (Self Describing Data Set) file specified as a command-line argument,
 * and processes its pages, parameters, columns, and arrays, printing their values to the console.
 *
 * It demonstrates how to use the SDDS library to access different types of data within an SDDS file.
 */

#include "SDDS.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>

/**
 * @brief Main function that processes the SDDS file.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line argument strings.
 * @return Returns 0 on successful execution, non-zero otherwise.
 *
 * The main function checks the command-line arguments, initializes the SDDS dataset,
 * and iterates over the pages in the SDDS file. For each page, it processes the parameters,
 * columns, and arrays, printing their names and values to the console.
 */
int main(int argc, char **argv) {
    SDDS_DATASET SDDS_dataset; /**< SDDS dataset structure */
    int32_t page;              /**< Current page number */
    int32_t n_params;          /**< Number of parameters */
    int32_t n_columns;         /**< Number of columns */
    int32_t n_arrays;          /**< Number of arrays */
    int64_t n_rows;            /**< Number of rows in the current page */
    char **parameter_names;    /**< Array of parameter names */
    char **column_names;       /**< Array of column names */
    char **array_names;        /**< Array of array names */
    int32_t type;              /**< Data type identifier */
    void *data;                /**< Generic data pointer */

    /** Check command-line arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename.sdds\n", argv[0]);
        exit(1);
    }

    /** Initialize SDDS input */
    if (!SDDS_InitializeInput(&SDDS_dataset, argv[1])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
    }

    /** Get parameter, column, and array names */
    parameter_names = SDDS_GetParameterNames(&SDDS_dataset, &n_params);
    column_names = SDDS_GetColumnNames(&SDDS_dataset, &n_columns);
    array_names = SDDS_GetArrayNames(&SDDS_dataset, &n_arrays);

    page = 0;
    /** Iterate over pages in the SDDS file */
    while ((page = SDDS_ReadPage(&SDDS_dataset)) > 0) {
        printf("Page %d\n", page);

        n_rows = SDDS_CountRowsOfInterest(&SDDS_dataset);

        /** Process parameters */
        printf("Parameters:\n");
        for (int32_t i = 0; i < n_params; i++) {
            type = SDDS_GetParameterType(&SDDS_dataset, i);
            printf("  %s = ", parameter_names[i]);
            switch (type) {
            case SDDS_SHORT: {
                short value;
                /** Get parameter value */
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%hd\n", value);
                break;
            }
            case SDDS_USHORT: {
                unsigned short value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%hu\n", value);
                break;
            }
            case SDDS_LONG: {
                int32_t value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%" PRId32 "\n", value);
                break;
            }
            case SDDS_ULONG: {
                uint32_t value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%" PRIu32 "\n", value);
                break;
            }
            case SDDS_LONG64: {
                int64_t value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%" PRId64 "\n", value);
                break;
            }
            case SDDS_ULONG64: {
                uint64_t value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%" PRIu64 "\n", value);
                break;
            }
            case SDDS_FLOAT: {
                float value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%15.6e\n", value);
                break;
            }
            case SDDS_DOUBLE: {
                double value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%21.14e\n", value);
                break;
            }
            case SDDS_LONGDOUBLE: {
                long double value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                if (LDBL_DIG == 18) {
                    printf("%21.18Le\n", value);
                } else {
                    printf("%21.14Le\n", value);
                }
                break;
            }
            case SDDS_STRING: {
                char *value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%s\n", value);
                free(value); /**< Free the allocated string */
                break;
            }
            case SDDS_CHARACTER: {
                char value;
                if (SDDS_GetParameter(&SDDS_dataset, parameter_names[i], &value) == NULL) {
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                    continue;
                }
                printf("%c\n", value);
                break;
            }
            default:
                printf("Unknown type\n");
                break;
            }
        }

        /** Process columns */
        printf("Columns (%" PRId64 " rows):\n", n_rows);
        for (int32_t i = 0; i < n_columns; i++) {
            type = SDDS_GetColumnType(&SDDS_dataset, i);
            printf("  Column: %s\n", column_names[i]);
            data = SDDS_GetColumn(&SDDS_dataset, column_names[i]);
            if (data == NULL) {
                fprintf(stderr, "Error getting column %s\n", column_names[i]);
                continue;
            }
            switch (type) {
            case SDDS_SHORT: {
                short *values = (short *)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %hd\n", values[row]);
                }
                break;
            }
            case SDDS_USHORT: {
                unsigned short *values = (unsigned short *)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %hu\n", values[row]);
                }
                break;
            }
            case SDDS_LONG: {
                int32_t *values = (int32_t *)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %" PRId32 "\n", values[row]);
                }
                break;
            }
            case SDDS_ULONG: {
                uint32_t *values = (uint32_t *)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %" PRIu32 "\n", values[row]);
                }
                break;
            }
            case SDDS_LONG64: {
                int64_t *values = (int64_t *)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %" PRId64 "\n", values[row]);
                }
                break;
            }
            case SDDS_ULONG64: {
                uint64_t *values = (uint64_t *)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %" PRIu64 "\n", values[row]);
                }
                break;
            }
            case SDDS_FLOAT: {
                float *values = (float *)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %15.6e\n", values[row]);
                }
                break;
            }
            case SDDS_DOUBLE: {
                double *values = (double *)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %21.14e\n", values[row]);
                }
                break;
            }
            case SDDS_LONGDOUBLE: {
                long double *values = (long double *)data;
                if (LDBL_DIG == 18) {
                    for (int64_t row = 0; row < n_rows; row++) {
                        printf("    %21.18Le\n", values[row]);
                    }
                } else {
                    for (int64_t row = 0; row < n_rows; row++) {
                        printf("    %21.14Le\n", values[row]);
                    }
                }
                break;
            }
            case SDDS_STRING: {
                char **values = (char **)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %s\n", values[row]);
                    free(values[row]); /**< Free each string */
                }
                break;
            }
            case SDDS_CHARACTER: {
                char *values = (char *)data;
                for (int64_t row = 0; row < n_rows; row++) {
                    printf("    %c\n", values[row]);
                }
                break;
            }
            default:
                printf("Unknown type\n");
                break;
            }
            /** Free data */
            SDDS_Free(data);
            data = NULL;
        }

        /** Process arrays */
        printf("Arrays:\n");
        for (int32_t i = 0; i < n_arrays; i++) {
            type = SDDS_GetArrayType(&SDDS_dataset, i);
            data = SDDS_GetArray(&SDDS_dataset, array_names[i], NULL);
            if (!data) {
                fprintf(stderr, "Error getting array %s\n", array_names[i]);
                continue;
            }
            printf("  Array: %s (dimensions: ", array_names[i]);

            SDDS_ARRAY *array = (SDDS_ARRAY *)data;
            for (int32_t d = 0; d < array->definition->dimensions; d++) {
                printf("%d", array->dimension[d]);
                if (d < array->definition->dimensions - 1)
                    printf(" x ");
            }
            printf(")\n");

            switch (type) {
            case SDDS_SHORT: {
                short *values = (short *)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %hd\n", values[idx]);
                }
                break;
            }
            case SDDS_USHORT: {
                unsigned short *values = (unsigned short *)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %hu\n", values[idx]);
                }
                break;
            }
            case SDDS_LONG: {
                int32_t *values = (int32_t *)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %" PRId32 "\n", values[idx]);
                }
                break;
            }
            case SDDS_ULONG: {
                uint32_t *values = (uint32_t *)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %" PRIu32 "\n", values[idx]);
                }
                break;
            }
            case SDDS_LONG64: {
                int64_t *values = (int64_t *)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %" PRId64 "\n", values[idx]);
                }
                break;
            }
            case SDDS_ULONG64: {
                uint64_t *values = (uint64_t *)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %" PRIu64 "\n", values[idx]);
                }
                break;
            }
            case SDDS_FLOAT: {
                float *values = (float *)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %15.6e\n", values[idx]);
                }
                break;
            }
            case SDDS_DOUBLE: {
                double *values = (double *)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %21.14e\n", values[idx]);
                }
                break;
            }
            case SDDS_LONGDOUBLE: {
                long double *values = (long double *)array->data;
                if (LDBL_DIG == 18) {
                    for (int32_t idx = 0; idx < array->elements; idx++) {
                        printf("    %21.18Le\n", values[idx]);
                    }
                } else {
                    for (int32_t idx = 0; idx < array->elements; idx++) {
                        printf("    %21.14Le\n", values[idx]);
                    }
                }
                break;
            }
            case SDDS_STRING: {
                char **values = (char **)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %s\n", values[idx]);
                }
                break;
            }
            case SDDS_CHARACTER: {
                char *values = (char *)array->data;
                for (int32_t idx = 0; idx < array->elements; idx++) {
                    printf("    %c\n", values[idx]);
                }
                break;
            }
            default:
                printf("Unknown type\n");
                break;
            }
            /** Free array data */
            SDDS_FreeArray(data);
        }
    }

    /** Free parameter names */
    SDDS_FreeStringArray(parameter_names, n_params);
    SDDS_Free(parameter_names);

    /** Free column names */
    SDDS_FreeStringArray(column_names, n_columns);
    SDDS_Free(column_names);

    /** Free array names */
    SDDS_FreeStringArray(array_names, n_arrays);
    SDDS_Free(array_names);

    /** Terminate SDDS */
    SDDS_Terminate(&SDDS_dataset);
    return 0;
}
