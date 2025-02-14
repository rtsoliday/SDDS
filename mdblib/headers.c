/**
 * @file headers.c
 * @brief Provides functions to manage and format table header strings.
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

void add_to_headers(
  char **header, /* array of header strings */
  long n_headers,
  char **item,    /* array of items to use for new column */
  long min_width, /* minimum acceptable width of new column */
  long format     /* index of item that is a C format specifier */
) {
  long max_strlen;
  long i, j, len, excess, width;
  char *ptr;

  max_strlen = min_width;
  for (i = 0; i < n_headers; i++) {
    if (format == i) {
      width = format_length(item[i]) + 2;
      if (width > max_strlen)
        max_strlen = width;
    } else if ((len = strlen(item[i])) > max_strlen)
      max_strlen = len;
  }

  for (i = 0; i < n_headers; i++) {
    if (i != format) {
      excess = max_strlen - strlen(item[i]);
      len = excess / 2.0 + 0.5;
      ptr = header[i] + strlen(header[i]);
      for (j = 0; j < len; j++, ptr++)
        *ptr = ' ';
      *ptr = 0;
      excess -= len;
      strcat(header[i], item[i]);
      ptr = header[i] + strlen(header[i]);
      for (j = 0; j < excess; j++, ptr++)
        *ptr = ' ';
      *ptr = 0;
    } else {
      excess = max_strlen - format_length(item[i]);
      len = excess / 2;
      ptr = header[i] + strlen(header[i]);
      for (j = 0; j < len; j++, ptr++)
        *ptr = ' ';
      *ptr = 0;
      strcat(ptr, item[i]);
      excess -= len;
      ptr = header[i] + strlen(header[i]);
      for (j = 0; j < excess; j++, ptr++)
        *ptr = ' ';
      *ptr = 0;
    }
  }
}

long format_length(char *format) {
  char *ptr;
  long width = 0;

  ptr = format;
  if (*ptr != '%' || *(ptr + 1) == '%' || 1 != sscanf(ptr + 1, "%ld", &width) || width <= 0)
    bomb("format specifier invalid", NULL);

  return (width);
}

/**
 * @brief Adds new standard headers to name, unit, and printf string headers.
 *
 * This function facilitates adding a new set of standard headers, including name, unit, and
 * format strings, to the respective header arrays. It ensures that the new headers conform
 * to the specified minimum width.
 *
 * @param name_header The header string for names.
 * @param unit_header The header string for units.
 * @param printf_string The header string for printf format specifiers.
 * @param new_name The new name to add to the name header.
 * @param new_unit The new unit to add to the unit header.
 * @param new_format The new format specifier to add to the printf string header.
 * @param min_width The minimum acceptable width for the new headers.
 */
void add_to_standard_headers(
  char *name_header,
  char *unit_header,
  char *printf_string,
  char *new_name,
  char *new_unit,
  char *new_format,
  long min_width) {
  char *header[3];
  char *item[3];

  header[0] = name_header;
  header[1] = unit_header;
  header[2] = printf_string;
  item[0] = new_name;
  item[1] = new_unit;
  item[2] = new_format;
  add_to_headers(header, 3, item, min_width, 2);
}
