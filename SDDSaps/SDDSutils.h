/**
 * @file SDDSutils.h
 * @brief Utility functions for SDDS dataset manipulation and string array operations.
 *
 * This header file provides declarations for various utility functions used in
 * handling SDDS datasets, manipulating dynamic string arrays, performing unit
 * operations on dataset columns, comparing parameter values, and calculating
 * products of prime numbers.
 *
 * This module is essential for data manipulation and analysis tasks involving
 * SDDS datasets and related operations.
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

long appendToStringArray(char ***item, long items, char *newItem);
long expandColumnPairNames(SDDS_DATASET *SDDSin, char ***name,
                           char ***errorName, long names,
                           char **excludeName, long excludeNames,
                           long typeMode, long typeValue);

char *multiplyColumnUnits(SDDS_DATASET *SDDSin, char *name1, char *name2);
char *divideColumnUnits(SDDS_DATASET *SDDSin, char *name1, char *name2);
/* basically just composes 1/(units) */
char *makeFrequencyUnits(SDDS_DATASET *SDDSin, char *indepName);

long compareParameterValues(void *param1, void *param2, long type);
void moveToStringArray(char ***target, long *targets, char **source,
                       long sources);

int64_t greatestProductOfSmallPrimes1(int64_t rows, int64_t *primeList, int64_t nPrimes);
int64_t greatestProductOfSmallPrimes(int64_t rows);
