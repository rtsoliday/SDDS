/**
 * @file timeconvert.c
 * @brief Provides functions for converting and manipulating time representations, including leap year calculations, Julian day conversions, and epoch time breakdowns.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, R. Soliday
 */

#include "mdb.h"
#include <time.h>

short IsLeapYear(short year) {
  if (year < 0)
    return -1;
  year = year < 100 ? (year > 95 ? year + 1900 : year + 2000) : year;
  if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
    return 1;
  return 0;
}

/* days in each month for leap years, nonleap years */
static short DaysInMonths[2][12] = {
  {
    31,
    28,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31,
  },
  {
    31,
    29,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31,
  },
};

short JulianDayFromMonthDay(short month, short day, short year, short *julianDay) {
  short leapYear, jday, i, daysInMonth;

  if (year <= 0 || month < 1 || month > 12 || day < 1 || !julianDay)
    return 0;

  leapYear = IsLeapYear(year);
  daysInMonth = DaysInMonths[leapYear][month - 1];
  if (day > daysInMonth)
    return 0;

  jday = day;
  for (i = 1; i < month; i++)
    jday += DaysInMonths[leapYear][i - 1];
  *julianDay = jday;
  return 1;
}

short MonthDayFromJulianDay(short julianDay, short year, short *month, short *day) {
  short leapYear, sum, i, days;

  if (julianDay < 0 || julianDay > 366 || year <= 0 || !month || !day)
    return 0;
  leapYear = IsLeapYear(year);
  if ((leapYear == 0 && julianDay >= 365) || julianDay >= 366) {
    *month = 12;
    *day = 31;
    return 1;
  }

  sum = 0;
  for (i = 1; i <= 12; i++) {
    days = DaysInMonths[leapYear][i - 1];
    if (sum + days < julianDay) {
      sum += days;
    } else
      break;
  }
  *month = i;
  *day = julianDay - sum;
  return 1;
}

#include <time.h>
/**
 * @brief Breaks down epoch time into its constituent components.
 *
 * This function decomposes epoch time into year, Julian day, month, day, and fractional hour components.
 *
 * @param year Pointer to store the year.
 * @param jDay Pointer to store the Julian day.
 * @param month Pointer to store the month.
 * @param day Pointer to store the day.
 * @param hour Pointer to store the fractional hour.
 * @param epochTime The epoch time to be broken down.
 * @return Returns 1 on success, 0 on failure.
 */
short TimeEpochToBreakdown(short *year, short *jDay, short *month, short *day, double *hour, double epochTime) {
  struct tm *timeBreakdown;
  double dayStartTime;
  short lyear, ljDay, lhour;
  time_t theTime;
  theTime = epochTime;
  if (!(timeBreakdown = localtime(&theTime)))
    return 0;
  lyear = timeBreakdown->tm_year + 1900;
  ljDay = timeBreakdown->tm_yday + 1;
  lhour = timeBreakdown->tm_hour;
  if (year)
    *year = lyear;
  if (jDay)
    *jDay = ljDay;
  if (month)
    *month = timeBreakdown->tm_mon + 1;
  if (day)
    *day = timeBreakdown->tm_mday;
  if (hour) {
    /* go through some contortions to preserve fractional seconds */
    TimeBreakdownToEpoch(lyear, ljDay, (short)0, (short)0, (double)0.0, &dayStartTime);
    *hour = (epochTime - dayStartTime) / 3600;
    if (((short)*hour) != lhour) {
      /* daylight savings time problem? */
      *hour = *hour + lhour - ((short)*hour);
    }
  }
  return 1;
}

/**
 * @brief Converts epoch time to a formatted text string.
 *
 * This function formats epoch time into a human-readable string in the format "YYYY/MM/DD HH:MM:SS.FFFF".
 *
 * @param text Buffer to store the formatted time string.
 * @param epochTime The epoch time to be converted.
 * @return Returns 1 on success, 0 on failure.
 */
short TimeEpochToText(char *text, double epochTime) {
  short year, jDay, month, day, hr, min;
  double dayTime, sec;
  if (!TimeEpochToBreakdown(&year, &jDay, &month, &day, &dayTime, epochTime))
    return 0;
  hr = dayTime;
  min = 60 * (dayTime - hr);
  sec = 3600.0 * dayTime - (3600.0 * hr + 60.0 * min);
  sprintf(text, "%04hd/%02hd/%02hd %02hd:%02hd:%07.4f",
          year, month, day, hr, min, sec);
  return 1;
}

/**
 * @brief Converts a broken-down time into epoch time.
 *
 * This function takes individual time components and converts them into epoch time.
 * It handles both Julian day and standard month/day formats based on the input.
 *
 * @param year The year.
 * @param jDay The Julian day number.
 * @param month The month.
 * @param day The day of the month.
 * @param hour The fractional hour.
 * @param epochTime Pointer to store the resulting epoch time.
 * @return Returns 1 on success, 0 on failure.
 */
short TimeBreakdownToEpoch(short year, short jDay, short month, short day, double hour, double *epochTime) {
  struct tm timeBreakdown;
  short imin, ihour;
  double fsec, fmin;

  if (!epochTime)
    return 0;
  memset((char *)&timeBreakdown, 0, sizeof(timeBreakdown));
  if (year > 100)
    timeBreakdown.tm_year = year - 1900;
  else
    timeBreakdown.tm_year = year;
  if (jDay) {
    short iday, imonth;
    if (!MonthDayFromJulianDay(jDay, year, &imonth, &iday)) {
      return 0;
    }
    timeBreakdown.tm_mday = iday;
    timeBreakdown.tm_mon = imonth - 1;
  } else {
    timeBreakdown.tm_mday = day;
    timeBreakdown.tm_mon = month - 1;
  }
  /* Break floating-point hours into integer H:M:S plus fractional seconds */
  ihour = timeBreakdown.tm_hour = hour;
  imin = timeBreakdown.tm_min = fmin = 60 * (hour - ihour);
  timeBreakdown.tm_sec = fsec = 60 * (fmin - imin);
  fsec -= timeBreakdown.tm_sec;
  timeBreakdown.tm_isdst = -1;
#if defined(SUNOS4)
  *epochTime = timelocal(&timeBreakdown) + fsec;
#else
  *epochTime = mktime(&timeBreakdown) + fsec;
#endif
  return 1;
}
