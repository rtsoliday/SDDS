import os
import platform
import shlex
import shutil
import subprocess
import uuid
from pathlib import Path

import pytest

from sdds_test_utils import PLATFORM_ID, ROOT_DIR


LIB_DIR = ROOT_DIR / "lib" / PLATFORM_ID
REQUIRED_LIBS = [
  LIB_DIR / "libmdblib.a",
]

HARNESS_SOURCE = r"""
#define _GNU_SOURCE

#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mdb.h"
#include "non_dominated_sort.h"
#include "scan.h"
#include "SDDStypes.h"

extern char *elapsed_time();

#define THREADS 8
#define ITERATIONS 500
#define TMP_ITERATIONS 64
#define SHARED_MATCHES 512

typedef struct {
  long id;
  char search_dir[128];
  char search_file[64];
  char search_path[256];
  char error[256];
} Worker;

typedef struct {
  KEYED_EQUIVALENT **key_group;
  long key_groups;
  double search;
  long start;
  long count;
  long *results;
  char error[256];
} SharedKeyWorker;

static char tmp_names[THREADS][TMP_ITERATIONS][64];
static char mktemp_names[THREADS][TMP_ITERATIONS][64];

static char *xstrdup(const char *s)
{
  char *copy = (char *)malloc(strlen(s) + 1);
  if (!copy)
    return NULL;
  strcpy(copy, s);
  return copy;
}

static int nearly_equal(double actual, double expected)
{
  return fabs(actual - expected) < 1e-12;
}

static void set_error(Worker *worker, const char *message)
{
  snprintf(worker->error, sizeof(worker->error), "%s", message);
}

static int create_file(const char *path)
{
  FILE *fp = fopen(path, "w");
  if (!fp)
    return 0;
  fputs("mdblib thread-safety fixture\n", fp);
  fclose(fp);
  return 1;
}

static int setup_search_fixture(Worker *worker)
{
  char template_dir[128];
  int nchars;

  snprintf(template_dir, sizeof(template_dir), "/tmp/mdblib_search_%ld_XXXXXX",
           worker->id);
  if (!mktempOAG(template_dir) || !template_dir[0])
    return 0;
  if (mkdir(template_dir, 0700) != 0)
    return 0;
  snprintf(worker->search_dir, sizeof(worker->search_dir), "%s", template_dir);
  snprintf(worker->search_file, sizeof(worker->search_file),
           "worker_%ld.dat", worker->id);
  nchars = snprintf(worker->search_path, sizeof(worker->search_path), "%s/%s",
                    worker->search_dir, worker->search_file);
  if (nchars < 0 || nchars >= (int)sizeof(worker->search_path))
    return 0;
  return create_file(worker->search_path);
}

static void cleanup_search_fixture(Worker *worker)
{
  if (worker->search_path[0])
    remove(worker->search_path);
  if (worker->search_dir[0])
    rmdir(worker->search_dir);
  worker->search_path[0] = 0;
  worker->search_dir[0] = 0;
}

static void free_key_groups(KEYED_EQUIVALENT **key_group, long key_groups)
{
  long i;

  for (i = 0; i < key_groups; i++) {
    free(key_group[i]->equivalent);
    free(key_group[i]);
  }
  free(key_group);
}

static int check_row_sort(Worker *worker, long iteration)
{
  double base = worker->id * 1000.0 + iteration;
  double row0[2] = {base + 3.0, 30.0};
  double row1[2] = {base + 1.0, 10.0};
  double row2[2] = {base + 2.0, 20.0};
  double *rows[3] = {row0, row1, row2};

  set_up_row_sort(0, 2, sizeof(double), double_cmpasc);
  qsort(rows, 3, sizeof(*rows), row_compare);
  if (!nearly_equal(rows[0][0], base + 1.0) ||
      !nearly_equal(rows[1][0], base + 2.0) ||
      !nearly_equal(rows[2][0], base + 3.0)) {
    set_error(worker, "row sort comparator state interleaved");
    return 1;
  }
  return 0;
}

static int check_sort_index(Worker *worker, long iteration)
{
  double base = worker->id * 1000.0 + iteration;
  double values[6] = {
    base + 3.0,
    base + 1.0,
    base + 2.0,
    base + 1.0,
    base + 4.0,
    base + 0.0,
  };
  double expected_values[6] = {
    base + 0.0,
    base + 1.0,
    base + 1.0,
    base + 2.0,
    base + 3.0,
    base + 4.0,
  };
  long expected_index[6] = {5, 1, 3, 2, 0, 4};
  long *index;
  long i;

  index = sort_and_return_index(values, SDDS_DOUBLE, 6, 1);
  if (!index) {
    set_error(worker, "sort_and_return_index returned NULL");
    return 1;
  }
  for (i = 0; i < 6; i++) {
    if (!nearly_equal(values[i], expected_values[i]) || index[i] != expected_index[i]) {
      free(index);
      set_error(worker, "sorted key groups interleaved");
      return 1;
    }
  }
  free(index);
  return 0;
}

static int check_key_group_search(Worker *worker, long iteration)
{
  double base = worker->id * 1000.0 + iteration;
  double values[4] = {base + 2.0, base + 1.0, base + 2.0, base + 3.0};
  double search = base + 2.0;
  KEYED_EQUIVALENT **key_group;
  long key_groups;
  long first;
  long second;
  long third;

  key_group = MakeSortedKeyGroups(&key_groups, SDDS_DOUBLE, values, 4);
  if (!key_group) {
    set_error(worker, "MakeSortedKeyGroups returned NULL");
    return 1;
  }
  first = FindMatchingKeyGroup(key_group, key_groups, SDDS_DOUBLE, &search, 0);
  second = FindMatchingKeyGroup(key_group, key_groups, SDDS_DOUBLE, &search, 0);
  third = FindMatchingKeyGroup(key_group, key_groups, SDDS_DOUBLE, &search, 0);
  free_key_groups(key_group, key_groups);
  if (first != 0 || second != 2 || third != -1) {
    set_error(worker, "FindMatchingKeyGroup search state interleaved");
    return 1;
  }
  return 0;
}

static int check_scan_item_list(Worker *worker, long iteration)
{
  char alpha[64];
  char beta[64];
  char *items[5];
  unsigned long flags = 0;
  int32_t alpha_value = 0;
  double beta_value = 0;
  char code = 0;
  char *label = NULL;
  long item_count = 5;
  long expected_alpha = worker->id * 100000 + iteration;
  double expected_beta = (double)worker->id + (double)iteration / 1000.0;
  long ok;
  int failed = 0;

  snprintf(alpha, sizeof(alpha), "alpha=%ld", expected_alpha);
  snprintf(beta, sizeof(beta), "beta=%.3f", expected_beta);
  items[0] = xstrdup(alpha);
  items[1] = xstrdup(beta);
  items[2] = xstrdup("code=Z");
  items[3] = xstrdup("label=worker");
  items[4] = xstrdup("flag");
  if (!items[0] || !items[1] || !items[2] || !items[3] || !items[4]) {
    set_error(worker, "strdup failed");
    return 1;
  }

  ok = scanItemList(&flags, items, &item_count, 0,
                    "alpha", SDDS_LONG, &alpha_value, 1, 0x1u,
                    "beta", SDDS_DOUBLE, &beta_value, 1, 0x2u,
                    "code", SDDS_CHARACTER, &code, 1, 0x4u,
                    "label", SDDS_STRING, &label, 1, 0x8u,
                    "flag", -1, NULL, 0, 0x10u,
                    NULL);
  if (!ok || flags != 0x1fu || alpha_value != expected_alpha ||
      !nearly_equal(beta_value, expected_beta) || code != 'Z' ||
      !label || strcmp(label, "worker")) {
    failed = 1;
  }

  free(label);
  free(items[0]);
  free(items[1]);
  free(items[2]);
  free(items[3]);
  free(items[4]);
  if (failed) {
    set_error(worker, "scanItemList scratch state interleaved");
    return 1;
  }
  return 0;
}

static double compute_test_hypervolume(double shift)
{
  population pop;
  double column0[3] = {1.0 + shift, 2.0 + shift, 2.0 + shift};
  double column1[3] = {2.0 + shift, 1.0 + shift, 2.0 + shift};
  double column2[3] = {2.0 + shift, 2.0 + shift, 1.0 + shift};
  double *columns[3] = {column0, column1, column2};
  double reference[3] = {3.0 + shift, 3.0 + shift, 3.0 + shift};
  long maximize[3] = {0, 0, 0};
  int64_t *sorted_index;
  double hypervolume;

  fill_population(&pop, 3, 3, columns, maximize, NULL);
  sorted_index = non_dominated_sort(&pop);
  free(sorted_index);
  hypervolume = compute_hypervolume(&pop, reference);
  free_pop_mem(&pop);
  return hypervolume;
}

static int check_hypervolume(Worker *worker, long iteration)
{
  double shift = worker->id * 0.001 + iteration * 0.000001;
  double hypervolume = compute_test_hypervolume(shift);

  if (!nearly_equal(hypervolume, 4.0)) {
    set_error(worker, "hypervolume comparator state interleaved");
    return 1;
  }
  return 0;
}

static int check_search_path(Worker *worker)
{
  char tagged_file[128];
  char expected_tagged_path[320];
  char *found;

  found = findFileInSearchPath(worker->search_file);
  if (!found || strcmp(found, worker->search_path)) {
    free(found);
    set_error(worker, "search path was not visible across threads");
    return 1;
  }
  free(found);

  snprintf(tagged_file, sizeof(tagged_file), "%s=page+row", worker->search_file);
  snprintf(expected_tagged_path, sizeof(expected_tagged_path), "%s=page+row",
           worker->search_path);
  found = findFileInSearchPath(tagged_file);
  if (!found || strcmp(found, expected_tagged_path)) {
    free(found);
    set_error(worker, "search path tag handling changed");
    return 1;
  }
  free(found);
  return 0;
}

static void *search_path_reader_worker(void *arg)
{
  Worker *worker = (Worker *)arg;

  return check_search_path(worker) ? worker : NULL;
}

static int check_search_path_visible_across_threads(void)
{
  Worker worker;
  pthread_t thread;
  void *result = NULL;

  worker.id = 99;
  worker.search_dir[0] = 0;
  worker.search_file[0] = 0;
  worker.search_path[0] = 0;
  worker.error[0] = 0;
  if (!setup_search_fixture(&worker)) {
    fprintf(stderr, "failed to set up search fixture for shared search path test\n");
    return 1;
  }
  setSearchPath(worker.search_dir);
  if (pthread_create(&thread, NULL, search_path_reader_worker, &worker)) {
    fprintf(stderr, "pthread_create failed for shared search path test\n");
    setSearchPath(NULL);
    cleanup_search_fixture(&worker);
    return 1;
  }
  if (pthread_join(thread, &result)) {
    fprintf(stderr, "pthread_join failed for shared search path test\n");
    setSearchPath(NULL);
    cleanup_search_fixture(&worker);
    return 1;
  }
  setSearchPath(NULL);
  cleanup_search_fixture(&worker);
  if (result || worker.error[0]) {
    fprintf(stderr, "shared search path failed: %s\n", worker.error);
    return 1;
  }
  return 0;
}

static int check_allocators(Worker *worker, long iteration)
{
  char *buffer = (char *)tmalloc(32);

  if (!buffer) {
    set_error(worker, "tmalloc returned NULL");
    return 1;
  }
  snprintf(buffer, 32, "w%ld-%ld", worker->id, iteration);
  buffer = (char *)trealloc(buffer, 64);
  if (!buffer) {
    set_error(worker, "trealloc returned NULL");
    return 1;
  }
  if (!strstr(buffer, "w")) {
    tfree(buffer);
    set_error(worker, "trealloc content changed");
    return 1;
  }
  tfree(buffer);
  return 0;
}

static int check_scan_item_list_long(Worker *worker, long iteration)
{
  char value_item[64];
  char *items[1];
  unsigned long long flags = 0;
  int32_t value = 0;
  long item_count = 1;
  long expected = worker->id * 100000 + iteration;
  long ok;

  snprintf(value_item, sizeof(value_item), "value=%ld", expected);
  items[0] = xstrdup(value_item);
  if (!items[0]) {
    set_error(worker, "strdup failed");
    return 1;
  }
  ok = scanItemListLong(&flags, items, &item_count, 0,
                        "value", SDDS_LONG, &value, 1, 1ull << 40,
                        NULL);
  free(items[0]);
  if (!ok || flags != (1ull << 40) || value != expected) {
    set_error(worker, "scanItemListLong scratch state interleaved");
    return 1;
  }
  return 0;
}

static int check_legacy_scan_item_list(Worker *worker, long iteration)
{
  char value_item[64];
  char *items[2];
  unsigned long flags = 0;
  int32_t value = 0;
  long item_count = 2;
  long expected = worker->id * 100000 + iteration;
  long ok;

  snprintf(value_item, sizeof(value_item), "value=%ld", expected);
  items[0] = xstrdup(value_item);
  items[1] = xstrdup("unused=left");
  if (!items[0] || !items[1]) {
    set_error(worker, "strdup failed");
    return 1;
  }
  ok = scan_item_list(&flags, items, &item_count,
                      "value", SDDS_LONG, &value, 1, 0x20u,
                      NULL);
  if (!ok || flags != 0x20u || value != expected ||
      item_count != 1 || strcmp(items[0], "unused")) {
    free(items[0]);
    set_error(worker, "scan_item_list scratch state interleaved");
    return 1;
  }
  free(items[0]);
  return 0;
}

static int check_edit_string(Worker *worker, long iteration)
{
  char text[128];
  char expected[128];

  snprintf(text, sizeof(text), "worker-%ld-%ld", worker->id, iteration);
  snprintf(expected, sizeof(expected), "worker-%ld-%ld!", worker->id, iteration);
  if (!edit_string(text, "ei|!|") || strcmp(text, expected)) {
    set_error(worker, "edit_string static buffer interleaved");
    return 1;
  }
  return 0;
}

static int check_timer(Worker *worker)
{
  char *elapsed;
  double seconds;

  init_stats();
  seconds = delapsed_time();
  elapsed = elapsed_time();
  if (seconds < 0 || !elapsed || strlen(elapsed) < 8) {
    set_error(worker, "timer state invalid");
    return 1;
  }
  return 0;
}

static int valid_month_name(const char *month)
{
  static const char *const months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int i;

  for (i = 0; i < 12; i++)
    if (!strcmp(month, months[i]))
      return 1;
  return 0;
}

static int check_time_string(const char *value, int with_seconds)
{
  int day, year, hour, minute, second;
  char month[4];
  char extra;

  if (!value)
    return 0;
  month[0] = 0;
  if (with_seconds) {
    if (sscanf(value, "%d %3s %d %d:%d:%d%c",
               &day, month, &year, &hour, &minute, &second, &extra) != 6)
      return 0;
    if (second < 0 || second > 59)
      return 0;
  } else {
    if (sscanf(value, "%d %3s %d %d:%d%c",
               &day, month, &year, &hour, &minute, &extra) != 5)
      return 0;
  }

  return day >= 1 && day <= 31 &&
         valid_month_name(month) &&
         year >= 0 && year <= 99 &&
         hour >= 0 && hour <= 23 &&
         minute >= 0 && minute <= 59;
}

static int check_time_apis(Worker *worker, long iteration)
{
  char *short_time;
  char *long_time;
  double epoch;
  double hour = 12.25 + worker->id / 100.0;
  double broken_hour;
  short year, jDay, month, day;
  short input_month = (iteration % 12) + 1;
  short input_day = 15;

  short_time = mtime();
  long_time = mtimes();
  if (!check_time_string(short_time, 0) || !check_time_string(long_time, 1)) {
    free(short_time);
    free(long_time);
    set_error(worker, "mtime static buffer state interleaved");
    return 1;
  }
  free(short_time);
  free(long_time);

  if (!TimeBreakdownToEpoch(2024, 0, input_month, input_day, hour, &epoch) ||
      !TimeEpochToBreakdown(&year, &jDay, &month, &day, &broken_hour, epoch)) {
    set_error(worker, "time conversion failed");
    return 1;
  }
  if (year != 2024 || month != input_month || day != input_day ||
      jDay < 1 || jDay > 366 || !nearly_equal(broken_hour, hour)) {
    set_error(worker, "TimeEpochToBreakdown static storage interleaved");
    return 1;
  }

  return 0;
}

static void *thread_worker(void *arg)
{
  Worker *worker = (Worker *)arg;
  long i;

  for (i = 0; i < ITERATIONS; i++) {
    if (check_row_sort(worker, i) ||
        check_sort_index(worker, i) ||
        check_key_group_search(worker, i) ||
        check_scan_item_list(worker, i) ||
        check_scan_item_list_long(worker, i) ||
        check_legacy_scan_item_list(worker, i) ||
        check_edit_string(worker, i) ||
        check_timer(worker) ||
        check_time_apis(worker, i) ||
        check_hypervolume(worker, i) ||
        check_allocators(worker, i)) {
      return worker;
    }
    if (i < TMP_ITERATIONS) {
      char template_name[64] = "mdblib_XXXXXX";
      tmpname(tmp_names[worker->id][i]);
      mktempOAG(template_name);
      snprintf(mktemp_names[worker->id][i], sizeof(mktemp_names[worker->id][i]),
               "%s", template_name);
    }
  }
  return NULL;
}

static int check_unique_names(char names[THREADS][TMP_ITERATIONS][64], const char *label)
{
  long thread1, thread2, index1, index2;

  for (thread1 = 0; thread1 < THREADS; thread1++) {
    for (index1 = 0; index1 < TMP_ITERATIONS; index1++) {
      for (thread2 = thread1; thread2 < THREADS; thread2++) {
        long start_index = (thread1 == thread2) ? index1 + 1 : 0;
        for (index2 = start_index; index2 < TMP_ITERATIONS; index2++) {
          if (!strcmp(names[thread1][index1], names[thread2][index2])) {
            fprintf(stderr, "duplicate %s name: %s\n", label, names[thread1][index1]);
            return 1;
          }
        }
      }
    }
  }
  return 0;
}

static void *shared_key_worker(void *arg)
{
  SharedKeyWorker *worker = (SharedKeyWorker *)arg;
  long i;

  for (i = 0; i < worker->count; i++) {
    long row = FindMatchingKeyGroup(worker->key_group, worker->key_groups,
                                    SDDS_DOUBLE, &worker->search, 0);
    if (row < 0) {
      snprintf(worker->error, sizeof(worker->error), "unexpected exhausted key group");
      return worker;
    }
    worker->results[worker->start + i] = row;
  }
  return NULL;
}

static int check_shared_key_group_thread_safety(void)
{
  pthread_t threads[THREADS];
  SharedKeyWorker workers[THREADS];
  double values[SHARED_MATCHES];
  long results[SHARED_MATCHES];
  unsigned char seen[SHARED_MATCHES];
  KEYED_EQUIVALENT **key_group;
  long key_groups;
  double search = 42.0;
  long i, j;

  for (i = 0; i < SHARED_MATCHES; i++) {
    values[i] = search;
    results[i] = -1;
    seen[i] = 0;
  }

  key_group = MakeSortedKeyGroups(&key_groups, SDDS_DOUBLE, values, SHARED_MATCHES);
  if (!key_group) {
    fprintf(stderr, "MakeSortedKeyGroups returned NULL for shared test\n");
    return 1;
  }

  for (i = 0; i < THREADS; i++) {
    workers[i].key_group = key_group;
    workers[i].key_groups = key_groups;
    workers[i].search = search;
    workers[i].start = i * (SHARED_MATCHES / THREADS);
    workers[i].count = SHARED_MATCHES / THREADS;
    workers[i].results = results;
    workers[i].error[0] = 0;
    if (pthread_create(&threads[i], NULL, shared_key_worker, &workers[i])) {
      fprintf(stderr, "pthread_create failed for shared key test\n");
      free_key_groups(key_group, key_groups);
      return 1;
    }
  }

  for (i = 0; i < THREADS; i++) {
    void *result = NULL;
    if (pthread_join(threads[i], &result)) {
      fprintf(stderr, "pthread_join failed for shared key test\n");
      free_key_groups(key_group, key_groups);
      return 1;
    }
    if (result || workers[i].error[0]) {
      fprintf(stderr, "shared key worker %ld failed: %s\n", i, workers[i].error);
      free_key_groups(key_group, key_groups);
      return 1;
    }
  }

  for (i = 0; i < SHARED_MATCHES; i++) {
    if (results[i] < 0 || results[i] >= SHARED_MATCHES || seen[results[i]]) {
      fprintf(stderr, "duplicate or invalid shared key result: %ld\n", results[i]);
      free_key_groups(key_group, key_groups);
      return 1;
    }
    seen[results[i]] = 1;
  }
  for (j = 0; j < SHARED_MATCHES; j++) {
    if (!seen[j]) {
      fprintf(stderr, "missing shared key result: %ld\n", j);
      free_key_groups(key_group, key_groups);
      return 1;
    }
  }
  if (FindMatchingKeyGroup(key_group, key_groups, SDDS_DOUBLE, &search, 0) != -1) {
    fprintf(stderr, "shared key group was not exhausted\n");
    free_key_groups(key_group, key_groups);
    return 1;
  }

  free_key_groups(key_group, key_groups);
  return 0;
}

static int run_thread_test(const char *alloc_record_prefix)
{
  pthread_t threads[THREADS];
  Worker workers[THREADS];
  long i;

  if (alloc_record_prefix)
    keep_alloc_record((char *)alloc_record_prefix);

  for (i = 0; i < THREADS; i++) {
    workers[i].id = i;
    workers[i].search_dir[0] = 0;
    workers[i].search_file[0] = 0;
    workers[i].search_path[0] = 0;
    workers[i].error[0] = 0;
    if (!setup_search_fixture(&workers[i])) {
      fprintf(stderr, "failed to set up search fixture for worker %ld\n", i);
      while (i-- > 0)
        cleanup_search_fixture(&workers[i]);
      return 1;
    }
    if (pthread_create(&threads[i], NULL, thread_worker, &workers[i])) {
      fprintf(stderr, "pthread_create failed\n");
      cleanup_search_fixture(&workers[i]);
      return 1;
    }
  }

  for (i = 0; i < THREADS; i++) {
    void *result = NULL;
    if (pthread_join(threads[i], &result)) {
      fprintf(stderr, "pthread_join failed\n");
      cleanup_search_fixture(&workers[i]);
      return 1;
    }
    if (result || workers[i].error[0]) {
      fprintf(stderr, "worker %ld failed: %s\n", i, workers[i].error);
      cleanup_search_fixture(&workers[i]);
      return 1;
    }
    cleanup_search_fixture(&workers[i]);
  }

  if (check_unique_names(tmp_names, "tmpname") ||
      check_unique_names(mktemp_names, "mktempOAG"))
    return 1;
  if (check_shared_key_group_thread_safety())
    return 1;
  if (check_search_path_visible_across_threads())
    return 1;

  puts("thread-safe");
  return 0;
}

static int run_regression(void)
{
  double row0[2] = {3.0, 30.0};
  double row1[2] = {1.0, 10.0};
  double row2[2] = {2.0, 20.0};
  double *rows[3] = {row0, row1, row2};
  double values[4] = {3.5, 1.5, 2.5, 1.5};
  long *index;
  char *items[5];
  unsigned long flags = 0;
  int32_t alpha = 0;
  double beta = 0;
  char code = 0;
  char *label = NULL;
  long item_count = 5;
  char *long_items[1];
  unsigned long long long_flags = 0;
  int32_t long_value = 0;
  long long_item_count = 1;
  char *legacy_items[2];
  unsigned long legacy_flags = 0;
  int32_t legacy_value = 0;
  long legacy_item_count = 2;
  char text[64] = "alpha beta";
  Worker search_worker;
  char *found;
  double hypervolume;

  set_up_row_sort(0, 2, sizeof(double), double_cmpasc);
  qsort(rows, 3, sizeof(*rows), row_compare);
  printf("row_sort=%.0f,%.0f,%.0f\n", rows[0][0], rows[1][0], rows[2][0]);

  index = sort_and_return_index(values, SDDS_DOUBLE, 4, 1);
  if (!index)
    return 1;
  printf("sort_values=%.1f,%.1f,%.1f,%.1f\n",
         values[0], values[1], values[2], values[3]);
  printf("sort_index=%ld,%ld,%ld,%ld\n", index[0], index[1], index[2], index[3]);
  free(index);

  items[0] = xstrdup("alpha=42");
  items[1] = xstrdup("beta=3.5");
  items[2] = xstrdup("code=Z");
  items[3] = xstrdup("label=hello");
  items[4] = xstrdup("flag");
  if (!scanItemList(&flags, items, &item_count, 0,
                    "alpha", SDDS_LONG, &alpha, 1, 0x1u,
                    "beta", SDDS_DOUBLE, &beta, 1, 0x2u,
                    "code", SDDS_CHARACTER, &code, 1, 0x4u,
                    "label", SDDS_STRING, &label, 1, 0x8u,
                    "flag", -1, NULL, 0, 0x10u,
                    NULL))
    return 1;
  printf("scan=%lu:%ld:%.1f:%c:%s:%ld\n",
         flags, (long)alpha, beta, code, label, item_count);
  free(label);
  free(items[0]);
  free(items[1]);
  free(items[2]);
  free(items[3]);
  free(items[4]);

  long_items[0] = xstrdup("value=7");
  if (!scanItemListLong(&long_flags, long_items, &long_item_count, 0,
                        "value", SDDS_LONG, &long_value, 1, 1ull << 40,
                        NULL))
    return 1;
  printf("scan_long=%llu:%ld:%ld\n",
         long_flags, (long)long_value, long_item_count);
  free(long_items[0]);

  legacy_items[0] = xstrdup("value=9");
  legacy_items[1] = xstrdup("unused=left");
  if (!scan_item_list(&legacy_flags, legacy_items, &legacy_item_count,
                      "value", SDDS_LONG, &legacy_value, 1, 0x20u,
                      NULL))
    return 1;
  printf("legacy_scan=%lu:%ld:%ld:%s\n",
         legacy_flags, (long)legacy_value, legacy_item_count, legacy_items[0]);
  free(legacy_items[0]);

  if (!edit_string(text, "ei|!|"))
    return 1;
  printf("edit=%s\n", text);

  search_worker.id = 99;
  search_worker.search_dir[0] = 0;
  search_worker.search_file[0] = 0;
  search_worker.search_path[0] = 0;
  if (!setup_search_fixture(&search_worker))
    return 1;
  setSearchPath(search_worker.search_dir);
  found = findFileInSearchPath(search_worker.search_file);
  printf("search=%s\n",
         found && !strcmp(found, search_worker.search_path) ? "found" : "missing");
  free(found);
  cleanup_search_fixture(&search_worker);

  hypervolume = compute_test_hypervolume(0.0);
  printf("hypervolume=%.1f\n", hypervolume);
  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "threads"))
    return run_thread_test(argc > 2 ? argv[2] : NULL);
  return run_regression();
}
"""


def compiler_command():
  command = shlex.split(os.environ.get("CC", "cc"))
  if not command or not shutil.which(command[0]):
    pytest.skip("C compiler not available")
  return command


@pytest.fixture(scope="module")
def mdblib_harness(tmp_path_factory):
  if not all(path.exists() for path in REQUIRED_LIBS):
    pytest.skip("mdblib library not built")

  tmp_path = tmp_path_factory.mktemp("mdblib_thread_safety")
  source = tmp_path / "mdblib_thread_safety.c"
  executable = tmp_path / "mdblib_thread_safety"
  source.write_text(HARNESS_SOURCE, encoding="ascii")

  command = [
    *compiler_command(),
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I",
    str(ROOT_DIR / "include"),
    str(source),
    "-L",
    str(LIB_DIR),
    "-lmdblib",
    "-lm",
    "-lpthread",
  ]
  if platform.system() == "Linux":
    command.extend(["-lrt", "-ldl", "-lgcc"])
  command.extend(["-o", str(executable)])

  subprocess.run(command, check=True)
  return executable


def run_harness(executable, *args):
  return subprocess.run(
    [str(executable), *args],
    check=True,
    capture_output=True,
    text=True,
  )


def test_mdblib_regression_outputs(mdblib_harness):
  result = run_harness(mdblib_harness)
  assert result.stderr == ""
  assert result.stdout == (
    "row_sort=1,2,3\n"
    "sort_values=1.5,1.5,2.5,3.5\n"
    "sort_index=1,3,2,0\n"
    "scan=31:42:3.5:Z:hello:5\n"
    "scan_long=1099511627776:7:1\n"
    "legacy_scan=32:9:1:unused\n"
    "edit=alpha beta!\n"
    "search=found\n"
    "hypervolume=4.0\n"
  )


def test_mdblib_is_thread_safe(mdblib_harness):
  alloc_prefix = Path("/tmp") / f"mdbrec_{uuid.uuid4().hex}"
  result = run_harness(mdblib_harness, "threads", str(alloc_prefix))
  assert result.stderr == ""
  assert result.stdout == "thread-safe\n"
  for suffix in (".tmalloc", ".trealloc", ".tfree"):
    path = Path(f"{alloc_prefix}{suffix}")
    try:
      assert path.stat().st_size > 0
    finally:
      try:
        path.unlink()
      except FileNotFoundError:
        pass
