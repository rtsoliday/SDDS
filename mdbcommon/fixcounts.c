/**
 * @file fixcounts.c
 * @brief Contains the implementation of the `fixcount` function to update data point counts in a file.
 *
 * This file provides functionality to adjust the number of data points recorded in a file by either counting the actual data points or setting it to a specified value.
 * It is intended for use with data files that follow a specific format, excluding SDDS files.
 */

#include "mdb.h"
#include "table.h"
#include <ctype.h>

#define LINE_LENGTH 1024

/** Function: fixcount
 * 
 * @brief Updates the data point count in a specified file.
 *
 * The `fixcount` function reads a file and updates the count of data points at a specific location within the file.
 * If `n_points` is -1, it counts the actual number of data points in the file, excluding lines that start with '!' (comment lines).
 * Otherwise, it uses the provided `n_points` value.
 * The function then writes the count back into the file at a predetermined position, ensuring the count fits within the allocated space.
 *
 * The function skips updating files that start with "SDDS" followed by a digit, returning -1 in such cases.
 *
 * @param filename The path to the file whose data point count is to be updated.
 * @param n_points The number of data points to set in the file.
 *                 If -1, the function will count the data points by reading the file.
 * @return Returns the number of data points written to the file on success.
 *         Returns 0 on failure, or if the count does not fit in the allocated space.
 *         Returns -1 if the file is identified as an SDDS file (starts with "SDDS" followed by a digit).
 */
int fixcount(char *filename, long n_points) {
  long count;
  FILE *fp;
  char s[LINE_LENGTH], t[LINE_LENGTH];
  int32_t l_count_line, posi_count_line;

  if (!(fp = fopen(filename, "r")))
    return (0);
  if (!fgets_skip(s, LINE_LENGTH, fp, '!', 0))
    return 0;
  if (strncmp(s, "SDDS", 4) == 0 && isdigit(s[4]))
    return -1;
  if (!fgets_skip(s, LINE_LENGTH, fp, '!', 0) || !fgets_skip(s, LINE_LENGTH, fp, '!', 0) || !fgets_skip(s, LINE_LENGTH, fp, '!', 0))
    return (0);
  posi_count_line = ftell(fp);
  if (!fgets_skip(s, LINE_LENGTH, fp, '!', 0))
    return (0);
  l_count_line = strlen(s) - 1;
  count = 0;
  if (n_points == -1) {
    /* count the number of points */
    while (fgets(s, LINE_LENGTH, fp))
      if (s[0] != '!')
        count++;
  } else
    count = n_points;
  fclose(fp);
  sprintf(t, "%ld", count);
  if ((long)strlen(t) <= l_count_line && (fp = fopen(filename, "r+"))) {
    pad_with_spaces(t, l_count_line - strlen(t));
    if (!(fseek(fp, posi_count_line, 0) != EOF && fputs(t, fp) != EOF)) {
      fclose(fp);
      return (0);
    } else {
      fclose(fp);
      return (count);
    }
  }
  return (0);
}
