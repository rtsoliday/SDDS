/******************************************************************************
 *
 * File:        distribute.c        
 *
 * Created:     12/2012
 *
 * Author:      Pavel Sakov
 *              Bureau of Meteorology
 *
 * Description: Distributes indices in the interval [i1, i2] between `nproc'
 *              processes. Process IDs are assumed to be in the interval
 *              [0, nproc-1]. The calling process has ID `rank'. The results
 *              are stored in 6 global variables, with the following relations
 *              between them:
 *                my_number_of_iterations = my_last_iteration 
 *                                                      - my_first_iteration + 1
 *                my_number_of_iterations = number_of_iterations[rank]
 *                my_first_iteration = first_iteratin[rank]
 *                my_last_iteration = last_iteration[rank]
 *
 * Revisions:   18/04/2018 PS: `nproc' was supposed to be an alias for
 *              `nprocesses'; now it can be arbitrary number such that
 *              0 < nproc < nprocesses.
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <assert.h>
#if defined(MPI)
#include <mpi.h>
#endif
#include "distribute.h"

#ifdef my_number_of_iterations
#undef my_number_of_iterations
#endif
#ifdef my_first_iteration
#undef my_first_iteration
#endif
#ifdef my_last_iteration
#undef my_last_iteration
#endif
#ifdef number_of_iterations
#undef number_of_iterations
#endif
#ifdef first_iteration
#undef first_iteration
#endif
#ifdef last_iteration
#undef last_iteration
#endif
int my_number_of_iterations = -1;
int my_first_iteration = -1;
int my_last_iteration = -1;
int* number_of_iterations = NULL;
int* first_iteration = NULL;
int* last_iteration = NULL;
static NN_THREAD_LOCAL int my_number_of_iterations_data = -1;
static NN_THREAD_LOCAL int my_first_iteration_data = -1;
static NN_THREAD_LOCAL int my_last_iteration_data = -1;
static NN_THREAD_LOCAL int* number_of_iterations_data = NULL;
static NN_THREAD_LOCAL int* first_iteration_data = NULL;
static NN_THREAD_LOCAL int* last_iteration_data = NULL;
static NN_THREAD_LOCAL int distribute_state_initialized = 0;
static MDB_THREAD_LOCK distribute_legacy_lock = MDB_THREAD_LOCK_INITIALIZER;
static int published_my_number_of_iterations = -1;
static int published_my_first_iteration = -1;
static int published_my_last_iteration = -1;
static int* published_number_of_iterations = NULL;
static int* published_first_iteration = NULL;
static int* published_last_iteration = NULL;

static void distribute_init_thread_state(void)
{
    if (!distribute_state_initialized) {
        mdb_thread_lock(&distribute_legacy_lock);
        if (my_number_of_iterations != published_my_number_of_iterations ||
            my_first_iteration != published_my_first_iteration ||
            my_last_iteration != published_my_last_iteration) {
            my_number_of_iterations_data = my_number_of_iterations;
            my_first_iteration_data = my_first_iteration;
            my_last_iteration_data = my_last_iteration;
        }
        if (number_of_iterations != published_number_of_iterations ||
            first_iteration != published_first_iteration ||
            last_iteration != published_last_iteration) {
            number_of_iterations_data = number_of_iterations;
            first_iteration_data = first_iteration;
            last_iteration_data = last_iteration;
        }
        mdb_thread_unlock(&distribute_legacy_lock);
        distribute_state_initialized = 1;
    }
}

static void distribute_publish_legacy_state(void)
{
    distribute_init_thread_state();
    mdb_thread_lock(&distribute_legacy_lock);
    my_number_of_iterations = my_number_of_iterations_data;
    my_first_iteration = my_first_iteration_data;
    my_last_iteration = my_last_iteration_data;
    number_of_iterations = number_of_iterations_data;
    first_iteration = first_iteration_data;
    last_iteration = last_iteration_data;
    published_my_number_of_iterations = my_number_of_iterations_data;
    published_my_first_iteration = my_first_iteration_data;
    published_my_last_iteration = my_last_iteration_data;
    published_number_of_iterations = number_of_iterations_data;
    published_first_iteration = first_iteration_data;
    published_last_iteration = last_iteration_data;
    mdb_thread_unlock(&distribute_legacy_lock);
}

int* my_number_of_iterations_ptr(void)
{
    distribute_init_thread_state();
    return &my_number_of_iterations_data;
}

int* my_first_iteration_ptr(void)
{
    distribute_init_thread_state();
    return &my_first_iteration_data;
}

int* my_last_iteration_ptr(void)
{
    distribute_init_thread_state();
    return &my_last_iteration_data;
}

int** number_of_iterations_ptr(void)
{
    distribute_init_thread_state();
    return &number_of_iterations_data;
}

int** first_iteration_ptr(void)
{
    distribute_init_thread_state();
    return &first_iteration_data;
}

int** last_iteration_ptr(void)
{
    distribute_init_thread_state();
    return &last_iteration_data;
}

#define my_number_of_iterations (*my_number_of_iterations_ptr())
#define my_first_iteration (*my_first_iteration_ptr())
#define my_last_iteration (*my_last_iteration_ptr())
#define number_of_iterations (*number_of_iterations_ptr())
#define first_iteration (*first_iteration_ptr())
#define last_iteration (*last_iteration_ptr())

/** Distributes indices in the interval [i1, i2] between `nproc' processes.
 * @param i1 Start of the interval
 * @param i2 End of the interval
 * @param nproc Number of processes (CPUs) to be be used
 * @param myrank ID of the process
 * @param prefix Prefix for log printing; NULL to print no log.
 * Note that `nprocesses' and `rank' are supposed to be external (global) 
 * variables.
 */
void distribute_iterations(int i1, int i2, int nproc, int myrank)
{
    int n, npp, i;

#if defined(MPI)
    fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    assert(i2 >= i1);

    if (number_of_iterations == NULL) {
        number_of_iterations = malloc(nprocesses * sizeof(int));
        first_iteration = malloc(nprocesses * sizeof(int));
        last_iteration = malloc(nprocesses * sizeof(int));
    }
#if defined(MPI)
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    assert(nproc > 0 && nproc <= nprocesses);

    n = i2 - i1 + 1;
    npp = n / nproc;
    if (n % nproc == 0) {
        for (i = 0; i < nproc; ++i)
            number_of_iterations[i] = npp;
    } else {
        int j = n - nproc * npp;

        for (i = 0; i < j; ++i)
            number_of_iterations[i] = npp + 1;
        for (i = j; i < nproc; ++i)
            number_of_iterations[i] = npp;
    }
    for (i = nproc; i < nprocesses; ++i)
        number_of_iterations[i] = 0;
#if defined(MPI)
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    first_iteration[0] = i1;
    last_iteration[0] = i1 + number_of_iterations[0] - 1;
    for (i = 1; i < nproc; ++i) {
        first_iteration[i] = last_iteration[i - 1] + 1;
        last_iteration[i] = first_iteration[i] + number_of_iterations[i] - 1;
    }
    for (i = nproc; i < nprocesses; ++i) {
        first_iteration[i] = last_iteration[i - 1] + 1;
        last_iteration[i] = first_iteration[i] + number_of_iterations[i] - 1;
    }

    my_first_iteration = first_iteration[myrank];
    my_last_iteration = last_iteration[myrank];
    my_number_of_iterations = number_of_iterations[myrank];
    distribute_publish_legacy_state();
}

/**
 */
void distribute_free(void)
{
    int* old_number_of_iterations = number_of_iterations;
    int* old_first_iteration = first_iteration;
    int* old_last_iteration = last_iteration;

    if (old_number_of_iterations == NULL)
        return;
    number_of_iterations = NULL;
    first_iteration = NULL;
    last_iteration = NULL;
    my_number_of_iterations = -1;
    my_first_iteration = -1;
    my_last_iteration = -1;
    distribute_publish_legacy_state();
    free(old_number_of_iterations);
    free(old_first_iteration);
    free(old_last_iteration);
}
