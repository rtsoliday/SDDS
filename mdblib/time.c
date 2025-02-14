/**
 * @file time.c
 * @brief Contains time-related functions: mtime(), convert_date_time(), mtimes().
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
#include <ctype.h>
#include <time.h>

#ifdef VAX_VMS

static char *month[12] = {
  "january", "february", "march",
  "april", "may", "june", "july",
  "august", "september", "october",
  "november", "december"};

/* routine: convert_date_time()
 * purpose: convert time string, as from mtime(), to DATE_TIME structure.
 * assumed format of input string: day month year hours:minutes
 * The month must be written out (e.g., "June", not 6).
 * Michael Borland, 1987
 */

convert_date_time(dt, ct0)
  DATE_TIME *dt;
char *ct0;
{
  register char *ptr, *ct;
  register long i, l;

  /* copy the string */
  cp_str(&ct, ct0);

  /* shift everything to lower case */
  ptr = ct;
  while (*ptr) {
    *ptr = tolower(*ptr);
    ptr++;
  }

  /* scan the day */
  if (!get_long(&(dt->day), ct))
    return (0);

  /* scan the month */
  ptr = get_token(ct);
  l = strlen(ptr);
  for (i = 0; i < 12; i++)
    if (strncmp(ptr, month[i], l) == 0)
      break;
  if (i == 12)
    return (0);
  dt->month = i + 1;

  if (!get_long(&(dt->year), ct))
    return (0);
  if (dt->year < 100)
    dt->year += 1900;

  dt->hrs = 0;
  dt->mins = 0;
  dt->secs = 0;
  while (*ct == ' ' && *ct)
    ct++;
  if (sscanf(ct, "%ld:%ld", &(dt->hrs), &(dt->mins)) != 2)
    return (NO_TIME);

  return (1);
}
#endif

/**
 * @brief Generates a formatted time string.
 *
 * This function returns a more user-friendly time string compared to `ctime()`.
 * 
 * **Format Comparison:**
 * - `ctime`: "wkd mmm dd hh:mm:ss 19yy\n"
 * - `mtime`: "dd mmm yy hh:mm"
 *
 * @return A pointer to the newly allocated formatted time string.
 */
char *mtime(void) {
  char *ct, *mt;
  char *month, *day, *t, *ptr;
  time_t i;
  time_t time();

  while ((mt = tmalloc((unsigned)30 * sizeof(*mt))) == NULL)
    puts("allocation failure in mtime()");
  time(&i);
  ct = ctime(&i) + 4;
  *(ct + strlen(ct) - 1) = 0;

  month = ct;
  ct = strchr(ct, ' ');
  while (*ct == ' ')
    *ct++ = 0;

  day = ct;
  ct = strchr(ct, ' ');
  while (*ct == ' ')
    *ct++ = 0;

  t = ct;
  ct = strchr(ct, ' ');
  while (*ct == ' ')
    *ct++ = 0;
  ptr = strrchr(t, ':');
  *ptr = 0;

  sprintf(mt, "%s %s %s %s", day, month, ct + 2, t);
  return (mt);
}

/**
 * @brief Generates a detailed formatted time string.
 *
 * This function returns a more detailed and user-friendly time string compared to `ctime()`.
 * 
 * **Format Comparison:**
 * - `ctime`: "wkd mmm dd hh:mm:ss 19yy\n"
 * - `mtimes`: "dd mmm yy hh:mm:ss"
 *
 * @return A pointer to the newly allocated detailed formatted time string.
 */
char *mtimes(void) {
  char *ct, *mt;
  char *month, *day, *t;
  time_t i;
  time_t time();

  while ((mt = tmalloc((unsigned)30 * sizeof(*mt))) == NULL)
    puts("allocation failure in mtime()");
  time(&i);
  ct = ctime(&i) + 4;
  *(ct + strlen(ct) - 1) = 0;

  month = ct;
  ct = strchr(ct, ' ');
  while (*ct == ' ')
    *ct++ = 0;

  day = ct;
  ct = strchr(ct, ' ');
  while (*ct == ' ')
    *ct++ = 0;

  t = ct;
  ct = strchr(ct, ' ');
  while (*ct == ' ')
    *ct++ = 0;

  sprintf(mt, "%s %s %s %s", day, month, ct + 2, t);
  return (mt);
}
