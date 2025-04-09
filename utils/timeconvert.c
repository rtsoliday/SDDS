
/* program: timeconvert
 * purpose: do conversions between time-since-epoch and calendar time.
 *
 * Michael Borland, 1996
 */
#include "mdb.h"
#include "scan.h"
#include <ctype.h>
#include <time.h>

#define SET_BREAKDOWN 0
#define SET_SECONDS 1
#define SET_NOW 2
#define SET_TEXTOUTPUT 3
#define SET_SCRIPTOUTPUT 4
#define OPTIONS 5
char *option[OPTIONS] = {
  "breakdown", "seconds", "now", "textoutput", "scriptoutput"};

static char *USAGE = "timeconvert\n\
 {-breakDown={year=<integer>,{month=<integer>,day=<integer> | julianDay=<integer>},hour=<value> | now} | \n\
  -seconds={<secondsSinceEpoch> | now} | \n\
  -now}\n\
 [-textOutput | -scriptOutput=<variableRootname>[,useEqualsSign]]\n\n\
breakDown    Take year, plus either month and day or Julian day, plus the\n\
             time of day in hours, and report the time-since-epoch in seconds.\n\
             If 'now' is given, use present time instead of time input from\n\
             options.\n\
seconds      Take time-since-epoch in seconds, and report (in order), the year,\n\
             Julian day, month, day of month, time in hours (floating point), \n\
             integer hour, integer minutes, and integer seconds.\n\
             If 'now' is given, use present time-since-epoch.\n\
now          Give the same report as -breakdown=now and -seconds=now together, all\n\
             all on one line of output.\n\
textOutput   Modifies the output for -breakDown, -seconds, and -now options, providing\n\
             a text time stamp string.\n\
scriptOutput Outputs command of the form \"set <varname> [=] <value>\".  These\n\
             can be used to set variables in a script or shell.  Variables have a common rootname, \n\
             given on the commandline.  The completions are Year, Month, Day, JulianDay, \n\
             Hours, Minutes, Seconds, Time, TimeStamp.\n\n\"\
Program by M. Borland (Version 3, June 1998).\n";

#define YEAR_GIVEN 0x0001U
#define DAY_GIVEN 0x0002U
#define MONTH_GIVEN 0x0004U
#define JULIANDAY_GIVEN 0x0008U
#define HOUR_GIVEN 0x0010U
#define NOW_GIVEN 0x0020U

int main(int argc, char **argv) {
  long i_arg, mode, textOutput, scriptOutput, useEquals = 0;
  SCANNED_ARG *scanned;
  short year, month, day, julianDay, ihour, iminute;
  time_t longTime;
  double secondsSinceEpoch, hour, minute;
  unsigned long flags;
  char *variableRootname = NULL, *timeStamp;
  double second;

  argc = scanargs(&scanned, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);

  mode = -1;
  year = month = day = julianDay = 0;
  secondsSinceEpoch = hour = 0;
  time(&longTime);
  secondsSinceEpoch = longTime;
  textOutput = scriptOutput = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[i_arg].list[0], option, OPTIONS, 0)) {
      case SET_BREAKDOWN:
        mode = SET_BREAKDOWN;
        if (--scanned[i_arg].n_items < 1 ||
            !scanItemList(&flags, scanned[i_arg].list + 1,
                          &scanned[i_arg].n_items, 0,
                          "year", SDDS_SHORT, &year, 1, YEAR_GIVEN,
                          "day", SDDS_SHORT, &day, 1, DAY_GIVEN,
                          "month", SDDS_SHORT, &month, 1, MONTH_GIVEN,
                          "julianday", SDDS_SHORT, &julianDay, 1, JULIANDAY_GIVEN,
                          "hour", SDDS_DOUBLE, &hour, 1, HOUR_GIVEN,
                          "now", -1, NULL, 0, NOW_GIVEN,
                          NULL) ||
            (!(flags & NOW_GIVEN) &&
             (!(flags & YEAR_GIVEN) || !(flags & HOUR_GIVEN) ||
              (!(flags & JULIANDAY_GIVEN) &&
               !(flags & MONTH_GIVEN && flags & DAY_GIVEN)))))
          bomb("invalid -breakDown syntax/values (timeconvert)", NULL);
        if (!(flags & NOW_GIVEN)) {
          if (hour > 24 || hour < 0)
            bomb("invalid hour given for -breakDown (timeconvert)", NULL);
          if (year < 1)
            bomb("invalid year given for -breakDown (timeconvert)", NULL);
          if (flags & JULIANDAY_GIVEN) {
            if (julianDay < 1 || julianDay > 366)
              bomb("invalid julian day given for -breakDown (timeconvert)", NULL);
          } else {
            if (day < 1 || day > 31)
              bomb("invalid day given for -breakDown (timeconvert)", NULL);
            if (month < 1 || month > 12)
              bomb("invalid month given for -breakDown (timeconvert)", NULL);
          }
        }
        break;
      case SET_SECONDS:
        mode = SET_SECONDS;
        if (scanned[i_arg].n_items != 2)
          bomb("invalid -seconds syntax/value", NULL);
        if (sscanf(scanned[i_arg].list[1], "%lf", &secondsSinceEpoch) != 1) {
          if (strncmp(scanned[i_arg].list[1],
                      "now", strlen(scanned[i_arg].list[1])) != 0)
            bomb("invalid -seconds syntax/value", NULL);
        } else {
          if (secondsSinceEpoch < 0)
            bomb("invalid seconds given for -seconds", NULL);
        }
        break;
      case SET_NOW:
        mode = SET_NOW;
        break;
      case SET_TEXTOUTPUT:
        textOutput = 1;
        break;
      case SET_SCRIPTOUTPUT:
        scriptOutput = 1;
        if (scanned[i_arg].n_items < 2 || scanned[i_arg].n_items > 3 ||
            strlen(variableRootname = scanned[i_arg].list[1]) < 1)
          bomb("invalid -scriptOutput syntax", USAGE);
        if (scanned[i_arg].n_items == 3) {
          char *ptr;
          ptr = str_tolower(scanned[i_arg].list[2]);
          if (strncmp(ptr, "useequals", strlen(ptr)) == 0)
            useEquals = 1;
          else
            bomb("invalid -scriptOutput syntax", USAGE);
        }
        break;
      default:
        bomb("unknown option given", USAGE);
        break;
      }
    } else {
      bomb("unknown argument given--appears to be filename", USAGE);
    }
  }

  if (textOutput && scriptOutput) {
    bomb("can't give -textOutput and -scriptOutput", USAGE);
  }

  if (mode == SET_BREAKDOWN) {
    if (!(flags & NOW_GIVEN))
      TimeBreakdownToEpoch(year, julianDay, month, day, hour, &secondsSinceEpoch);
    if (scriptOutput) {
      printf("set %sEpoch%s%.6f\n",
             variableRootname, useEquals ? " = " : " ", secondsSinceEpoch);
      longTime = secondsSinceEpoch;
      timeStamp = ctime(&longTime);
      timeStamp[strlen(timeStamp) - 1] = 0;
      printf("set %sTimeStamp%s{%s}\n",
             variableRootname, useEquals ? " = " : " ", timeStamp);
      exit(0);
    } else if (!textOutput)
      printf("%.6f\n", secondsSinceEpoch);
  } else {
    TimeEpochToBreakdown(&year, &julianDay, &month, &day, &hour, secondsSinceEpoch);
    ihour = hour;
    iminute = (minute = (hour - ihour) * 60);
    second = (minute - iminute) * 60;
    if (scriptOutput) {
      printf("set %sYear%s%hd\n",
             variableRootname, useEquals ? " = " : " ", year);
      printf("set %sMonth%s%hd\n",
             variableRootname, useEquals ? " = " : " ", month);
      printf("set %sDay%s%hd\n",
             variableRootname, useEquals ? " = " : " ", day);
      printf("set %sJulianDay%s%hd\n",
             variableRootname, useEquals ? " = " : " ", julianDay);
      printf("set %sHours%s%hd\n",
             variableRootname, useEquals ? " = " : " ", ihour);
      printf("set %sMinutes%s%hd\n",
             variableRootname, useEquals ? " = " : " ", iminute);
      printf("set %sSeconds%s%hd\n",
             variableRootname, useEquals ? " = " : " ", (short)second);
      printf("set %sHHMMSS%s%02hd:%02hd:%02hd\n",
             variableRootname, useEquals ? " = " : " ",
             ihour, iminute, (short)second);
      longTime = secondsSinceEpoch + 0.5;
      timeStamp = ctime(&longTime);
      timeStamp[strlen(timeStamp) - 1] = 0;
      printf("set %sTimeStamp%s{%s}\n",
             variableRootname, useEquals ? " = " : " ", timeStamp);
      if (mode == SET_NOW)
        printf("set %sEpoch%s%.6f\n",
               variableRootname, useEquals ? " = " : " ", secondsSinceEpoch);
      exit(0);
    }
    if (!textOutput) {
      printf("%hd %hd %hd %hd %.9f %hd %hd %hd", year, julianDay, month, day, hour,
             ihour, iminute, (short)second);
      if (mode == SET_NOW)
        printf(" %.6f\n", secondsSinceEpoch);
      else
        printf("\n");
    }
  }
  if (textOutput) {
    longTime = secondsSinceEpoch + 0.5;
    fputs(ctime(&longTime), stdout);
  }
  return (0);
}
