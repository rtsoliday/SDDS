/**
 * @file diffeqResDescrip.c
 * @brief Provides a description string corresponding to a given differential equation result code.
 *
 * This file defines a function that, given a result code from a differential equation
 * solver, returns a human-readable description of the result. The possible codes
 * range from error conditions to successful completion indicators.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday
 */
#include "mdb.h"

#define N_DIFFEQ_RETURNS 8

static struct {
  long code;
  char *description;
} diffeq_return[N_DIFFEQ_RETURNS] = {
  {-4, "exit condition failure"},
  {-3, "zero stepsize"},
  {-2, "can't take initial step"},
  {-1, "solution stepped outside of integration interval"},
  {0, "initial independent variable value greater than desired final value"},
  {1, "differential equations solved already"},
  {2, "zero of exit-function found"},
  {3, "end of integration interval reached"}};
static char *unknown = "unknown error";

/**
 * @brief Return a descriptive string for a given differential equation result code.
 *
 * This function maps a numerical result code to a corresponding string describing
 * the outcome of a differential equation integration procedure. If the code is not
 * recognized, the function returns "unknown error".
 *
 * @param result_code The integer code representing the result of a differential equation solver.
 * @return A constant character pointer to a descriptive string for the given code.
 */
char *diffeq_result_description(long result_code) {
  int i;
  for (i = 0; i < N_DIFFEQ_RETURNS; i++) {
    if (result_code == diffeq_return[i].code)
      return (diffeq_return[i].description);
  }
  return unknown;
}
