/**
 * @file report_stats.c
 * @brief Reports elapsed time, CPU time, BIO/DIO counts, page faults, and memory usage to a file.
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
long bio_count();
long dio_count();
/**
 * @brief Reports statistics to a specified file.
 *
 * This function formats and writes various statistics such as elapsed time, CPU time,
 * BIO/DIO counts, page faults, and memory usage to the specified file.
 *
 * @param fp Pointer to the file where the statistics will be written.
 * @param label A label string to prepend to the statistics output.
 */
void report_stats(FILE *fp, char *label) {
  char s[200];
  extern char *elapsed_time();
  //long cpu_time(), bio_count(), dio_count(), page_faults(), memory_count();

  sprintf(s, "ET:%13s CP:%8.2f BIO:%ld DIO:%ld PF:%ld MEM:%ld", elapsed_time(),
          cpu_time() / 100.0, bio_count(), dio_count(), page_faults(), memory_count());

  fprintf(fp, "%s   %s\n", label, s);
  fflush(fp);
}
