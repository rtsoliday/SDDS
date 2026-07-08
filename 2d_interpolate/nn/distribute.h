/******************************************************************************
 *
 * File:        distribute.h        
 *
 * Created:     12/2012
 *
 * Author:      Pavel Sakov
 *              Bureau of Meteorology
 *
 * Description:
 *
 * Revisions:
 *
 *****************************************************************************/

#if !defined(_DISTRIBUTE_H)

#include "nn_thread.h"

extern int nprocesses;
extern int rank;
int* my_number_of_iterations_ptr(void);
int* my_first_iteration_ptr(void);
int* my_last_iteration_ptr(void);
int** number_of_iterations_ptr(void);
int** first_iteration_ptr(void);
int** last_iteration_ptr(void);
#ifdef NN_NO_GLOBAL_COMPAT_MACROS
extern int my_number_of_iterations;
extern int my_first_iteration;
extern int my_last_iteration;
extern int* number_of_iterations;
extern int* first_iteration;
extern int* last_iteration;
#else
#define my_number_of_iterations (*my_number_of_iterations_ptr())
#define my_first_iteration (*my_first_iteration_ptr())
#define my_last_iteration (*my_last_iteration_ptr())
#define number_of_iterations (*number_of_iterations_ptr())
#define first_iteration (*first_iteration_ptr())
#define last_iteration (*last_iteration_ptr())
#endif

void distribute_iterations(int i1, int i2, int nproc, int rank);
void distribute_free(void);

#define _DISTRIBUTE_H
#endif
