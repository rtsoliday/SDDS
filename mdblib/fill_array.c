/**
 * @file fill_array.c
 * @brief Provides functions to fill arrays of various data types with a specified value.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author C. Saunders, M. Borland, R. Soliday
 */

#include "mdb.h"

/**
 * @brief Fills a double array with the specified value.
 *
 * Assigns the given value to each element of the provided double array.
 *
 * @param array Pointer to the double array to be filled.
 * @param n Number of elements in the array.
 * @param value The double value to assign to each array element.
 */
void fill_double_array(double *array, long n, double value) {
  while (--n >= 0)
    array[n] = value;
}

/**
 * @brief Fills a float array with the specified value.
 *
 * Assigns the given value to each element of the provided float array.
 *
 * @param array Pointer to the float array to be filled.
 * @param n Number of elements in the array.
 * @param value The float value to assign to each array element.
 */
void fill_float_array(float *array, long n, float value) {
  while (--n >= 0)
    array[n] = value;
}

/**
 * @brief Fills a long array with the specified value.
 *
 * Assigns the given value to each element of the provided long array.
 *
 * @param array Pointer to the long array to be filled.
 * @param n Number of elements in the array.
 * @param value The long value to assign to each array element.
 */
void fill_long_array(long *array, long n, long value) {
  while (--n >= 0)
    array[n] = value;
}

/**
 * @brief Fills an int array with the specified value.
 *
 * Assigns the given value to each element of the provided int array.
 *
 * @param array Pointer to the int array to be filled.
 * @param n Number of elements in the array.
 * @param value The int value to assign to each array element.
 */
void fill_int_array(int *array, long n, int value) {
  while (--n >= 0)
    array[n] = value;
}

/**
 * @brief Fills a short array with the specified value.
 *
 * Assigns the given value to each element of the provided short array.
 *
 * @param array Pointer to the short array to be filled.
 * @param n Number of elements in the array.
 * @param value The short value to assign to each array element.
 */
void fill_short_array(short *array, long n, short value) {
  while (--n >= 0)
    array[n] = value;
}
