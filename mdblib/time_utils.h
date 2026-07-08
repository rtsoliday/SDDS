#ifndef MDBLIB_TIME_UTILS_H
#define MDBLIB_TIME_UTILS_H 1

#include <time.h>

int mdb_localtime_copy(const time_t *when, struct tm *result);

#endif
