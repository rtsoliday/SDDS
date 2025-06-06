/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: non_dominated_sort.h
 * purpose: definitions for non-dominated sort routines
 * Hairong Shang, May 2005
*/

#ifndef _NON_DOMINATED_SORT_INCLUDED_
#define _NON_DOMINATED_SORT_INCLUDED_ 1
# define INF 1.0e14
#ifdef __cplusplus
extern "C" {
#endif

#undef epicsShareFuncMDBLIB
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_MDBLIB)
#define epicsShareFuncMDBLIB  __declspec(dllexport)
#else
#define epicsShareFuncMDBLIB
#endif
#else
#define epicsShareFuncMDBLIB
#endif


typedef struct lists {
  long index;
  struct lists *parent;
  struct lists *child;
} list;

typedef struct {
  long rank;
  double constr_violation;
  double *xreal;
  long **gene;
  double *xbin;
  double *obj;
  double *constr;
  double crowd_dist;
} individual;

typedef struct {
  individual *ind;
  long popsize;
  long nreal, nbin, ncons, nobj;
  long *nbits;
} population;

void insert_node(list *node, long x);
list* del_node(list *node);

long check_dominance (individual *a, individual *b, long nobj);

void assign_crowding_distance_list (population *pop, list *lst, long front_size, long start_i, int64_t *sorted_index);
void assign_crowding_distance_indices (population *pop, long c1, long c2, long nobj);
void quicksort_front_obj(population *pop, long objcount, long obj_array[], long obj_array_size);
void q_sort_front_obj(population *pop, long objcount, long obj_array[], long left, long right);
void quicksort_dist(population *pop, long *dist, long front_size);
void q_sort_dist(population *pop, long *dist, long left, long right);
/* Routine to compute crowding distances */
void assign_crowding_distance (population *pop, long *dist, long **obj_array, long front_size, long nobj);
epicsShareFuncMDBLIB int64_t *non_dominated_sort (population *pop);
  epicsShareFuncMDBLIB void fill_population(population *pop, long rows, long columns, double **columnValue, long *maximize, double *const_violation);
epicsShareFuncMDBLIB void free_pop_mem(population *pop);

#ifdef __cplusplus
}
#endif 

#endif

