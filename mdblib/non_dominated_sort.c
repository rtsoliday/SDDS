/**
 * @file non_dominated_sort.c
 * @brief Definitions and implementations for non-dominated sorting and crowding distance routines used in multi-objective optimization.
 * 
 * This file provides the core functions for performing non-dominated sorting on a population of individuals, 
 * calculating crowding distances, and sorting the population based on dominance and crowding. It includes 
 * utility functions for managing linked lists used in the sorting process.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author H. Shang, R. Soliday
 */

#include "mdb.h"
#include "non_dominated_sort.h"

/* Insert an element X into the list at location specified by NODE */
void insert_node(list *node, long x) {
  list *temp;
  if (node == NULL) {
    printf("\n Error!! asked to enter after a NULL pointer, hence exiting \n");
    exit(1);
  }
  temp = (list *)malloc(sizeof(list));
  temp->index = x;
  temp->child = node->child;
  temp->parent = node;
  if (node->child != NULL)
    node->child->parent = temp;
  node->child = temp;
  return;
}

/* Delete the node NODE from the list */
list *del_node(list *node) {
  list *temp;
  if (node == NULL) {
    printf("\n Error!! asked to delete a NULL pointer, hence exiting \n");
    exit(1);
  }
  temp = node->parent;
  temp->child = node->child;
  if (temp->child != NULL)
    temp->child->parent = temp;
  free(node);
  return (temp);
}

/* Routine for usual non-domination checking
   It will return the following values
   1 if a dominates b
   -1 if b dominates a
   0 if both a and b are non-dominated */
long check_dominance(individual *a, individual *b, long nobj) {
  long i;
  long flag1;
  long flag2;
  flag1 = 0;
  flag2 = 0;
  if (a->constr_violation < 0 && b->constr_violation < 0) {
    if (a->constr_violation > b->constr_violation)
      return (1);
    else if (a->constr_violation < b->constr_violation)
      return (-1);
    else
      return (0);
  } else {
    if (a->constr_violation < 0 && b->constr_violation >= 0)
      return (-1);
    else if (a->constr_violation >= 0 && b->constr_violation < 0)
      return (1);
    else {
      for (i = 0; i < nobj; i++) {
        if (a->obj[i] < b->obj[i])
          flag1 = 1;
        else if (a->obj[i] > b->obj[i])
          flag2 = 1;
      }
      if (flag1 == 1 && flag2 == 0)
        return (1);
      else if (flag1 == 0 && flag2 == 1)
        return (-1);
      else
        return (0);
    }
  }
}

/* Routine to compute crowding distance based on ojbective function values when the population in in the form of a list */
void assign_crowding_distance_list(population *pop, list *lst, long front_size, long start_i, int64_t *sorted_index) {
  long **obj_array;
  long *dist;
  long i, j, nobj;
  list *temp;

  temp = lst;
  nobj = pop->nobj;
  if (front_size == 1) {
    pop->ind[lst->index].crowd_dist = INF;
    return;
  } else if (front_size == 2) {
    pop->ind[lst->index].crowd_dist = INF;
    pop->ind[lst->child->index].crowd_dist = INF;
    return;
  }
  obj_array = (long **)malloc(nobj * sizeof(long));
  dist = (long *)malloc(front_size * sizeof(long));
  for (i = 0; i < nobj; i++)
    obj_array[i] = (long *)malloc(front_size * sizeof(long));

  for (j = 0; j < front_size; j++) {
    dist[j] = temp->index;
    temp = temp->child;
  }
  assign_crowding_distance(pop, dist, obj_array, front_size, nobj);
  q_sort_dist(pop, dist, 0, front_size - 1);
  for (j = 0; j < front_size; j++)
    sorted_index[start_i + j] = dist[j];
  free(dist);
  for (i = 0; i < nobj; i++)
    free(obj_array[i]);
  free(obj_array);
  return;
}

/* Routine to compute crowding distance based on objective function values when the population in in the form of an array */
void assign_crowding_distance_indices(population *pop, long c1, long c2, long nobj) {
  long **obj_array;
  long *dist;
  long i, j;
  long front_size;
  front_size = c2 - c1 + 1;
  if (front_size == 1) {
    pop->ind[c1].crowd_dist = INF;
    return;
  } else if (front_size == 2) {
    pop->ind[c1].crowd_dist = INF;
    pop->ind[c2].crowd_dist = INF;
    return;
  }
  obj_array = (long **)malloc(nobj * sizeof(long));
  dist = (long *)malloc(front_size * sizeof(long));
  for (i = 0; i < nobj; i++)
    obj_array[i] = (long *)malloc(front_size * sizeof(long));
  for (j = 0; j < front_size; j++)
    dist[j] = c1++;
  assign_crowding_distance(pop, dist, obj_array, front_size, nobj);
  free(dist);
  for (i = 0; i < nobj; i++)
    free(obj_array[i]);
  free(obj_array);
  return;
}

/* Randomized quick sort routine to sort a population based on a 
   particular objective chosen */
void quicksort_front_obj(population *pop, long objcount, long obj_array[], long obj_array_size) {
  q_sort_front_obj(pop, objcount, obj_array, 0, obj_array_size - 1);
  return;
}

/* Actual implementation of the randomized quick sort used to sort 
   a population based on a particular objective chosen */
void q_sort_front_obj(population *pop, long objcount, long obj_array[], long left, long right) {
  long index;
  long temp;
  long i, j;
  double pivot;
  if (left < right) {
    index = (left + right) / 2;
    temp = obj_array[right];
    obj_array[right] = obj_array[index];
    obj_array[index] = temp;
    pivot = pop->ind[obj_array[right]].obj[objcount];
    i = left - 1;
    for (j = left; j < right; j++) {
      if (pop->ind[obj_array[j]].obj[objcount] <= pivot) {
        i += 1;
        temp = obj_array[j];
        obj_array[j] = obj_array[i];
        obj_array[i] = temp;
      }
    }
    index = i + 1;
    temp = obj_array[index];
    obj_array[index] = obj_array[right];
    obj_array[right] = temp;
    q_sort_front_obj(pop, objcount, obj_array, left, index - 1);
    q_sort_front_obj(pop, objcount, obj_array, index + 1, right);
  }
  return;
}

/* Randomized quick sort routine to sort a population based on crowding distance */
void quicksort_dist(population *pop, long *dist, long front_size) {
  q_sort_dist(pop, dist, 0, front_size - 1);
  return;
}

/* Actual implementation of the randomized quick sort used to sort a population based on crowding distance */
void q_sort_dist(population *pop, long *dist, long left, long right) {
  long index;
  long temp;
  long i, j;
  double pivot;
  if (left < right) {
    index = (left + right) / 2;
    temp = dist[right];
    dist[right] = dist[index];
    dist[index] = temp;
    pivot = pop->ind[dist[right]].crowd_dist;
    i = left - 1;
    for (j = left; j < right; j++) {
      if (pop->ind[dist[j]].crowd_dist > pivot) {
        i += 1;
        temp = dist[j];
        dist[j] = dist[i];
        dist[i] = temp;
      }
    }
    index = i + 1;
    temp = dist[index];
    dist[index] = dist[right];
    dist[right] = temp;
    q_sort_dist(pop, dist, left, index - 1);
    q_sort_dist(pop, dist, index + 1, right);
  }
  return;
}

/* Routine to compute crowding distances */
void assign_crowding_distance(population *pop, long *dist, long **obj_array, long front_size, long nobj) {
  long i, j;
  for (i = 0; i < nobj; i++) {
    for (j = 0; j < front_size; j++)
      obj_array[i][j] = dist[j];
    quicksort_front_obj(pop, i, obj_array[i], front_size);
  }
  for (j = 0; j < front_size; j++)
    pop->ind[dist[j]].crowd_dist = 0.0;
  for (i = 0; i < nobj; i++)
    pop->ind[obj_array[i][0]].crowd_dist = INF;
  for (i = 0; i < nobj; i++) {
    for (j = 1; j < front_size - 1; j++) {
      if (pop->ind[obj_array[i][j]].crowd_dist != INF) {
        if (pop->ind[obj_array[i][front_size - 1]].obj[i] == pop->ind[obj_array[i][0]].obj[i])
          pop->ind[obj_array[i][j]].crowd_dist += 0.0;
        else
          pop->ind[obj_array[i][j]].crowd_dist += (pop->ind[obj_array[i][j + 1]].obj[i] - pop->ind[obj_array[i][j - 1]].obj[i]) / (pop->ind[obj_array[i][front_size - 1]].obj[i] - pop->ind[obj_array[i][0]].obj[i]);
      }
    }
  }
  for (j = 0; j < front_size; j++) {
    if (pop->ind[dist[j]].crowd_dist != INF)
      pop->ind[dist[j]].crowd_dist = (pop->ind[dist[j]].crowd_dist) / nobj;
  }
  return;
}

/**
 * @brief Performs non-dominated sorting on a population and assigns ranks and crowding distances.
 *
 * This function sorts the population into different fronts based on non-domination, assigns a rank to each individual
 * indicating its front, and computes the crowding distance to maintain diversity within each front.
 *
 * @param new_pop Pointer to the population structure to be sorted.
 * @return Array of sorted indices representing the population ordered by rank and crowding distance.
 */
int64_t *non_dominated_sort(population *new_pop) {
  long flag, popsize, i, end, front_size, rank = 1, j;
  int64_t *sorted_index = NULL;
  list *orig;
  list *cur;
  list *temp1, *temp2;
  orig = (list *)malloc(sizeof(list));
  cur = (list *)malloc(sizeof(list));
  front_size = 0;
  orig->index = -1;
  orig->parent = NULL;
  orig->child = NULL;
  cur->index = -1;
  cur->parent = NULL;
  cur->child = NULL;
  temp1 = orig;
  popsize = new_pop->popsize;
  sorted_index = (int64_t *)malloc(sizeof(*sorted_index) * popsize);
  for (i = 0; i < popsize; i++) {
    sorted_index[i] = i;
    insert_node(temp1, i);
    temp1 = temp1->child;
  }
  i = j = 0;
  do {
    if (orig->child->child == NULL) {
      new_pop->ind[orig->child->index].rank = rank;
      new_pop->ind[orig->child->index].crowd_dist = INF;
      sorted_index[i++] = orig->child->index;
      free(orig->child);
      break;
    }
    temp1 = orig->child;
    insert_node(cur, temp1->index);
    front_size = 1;
    temp2 = cur->child;
    temp1 = del_node(temp1);
    temp1 = temp1->child;
    do {
      temp2 = cur->child;
      do {
        end = 0;
        flag = check_dominance(&(new_pop->ind[temp1->index]),
                               &(new_pop->ind[temp2->index]), new_pop->nobj);
        if (flag == 1) {
          insert_node(orig, temp2->index);
          temp2 = del_node(temp2);
          front_size--;
          temp2 = temp2->child;
        }
        if (flag == 0)
          temp2 = temp2->child;
        else if (flag == -1)
          end = 1;
      } while (end != 1 && temp2 != NULL);
      if (flag == 0 || flag == 1) {
        insert_node(cur, temp1->index);
        front_size++;
        temp1 = del_node(temp1);
      }
      temp1 = temp1->child;
    } while (temp1 != NULL);
    temp2 = cur->child;
    do {
      new_pop->ind[temp2->index].rank = rank;
      sorted_index[i++] = temp2->index;
      temp2 = temp2->child;
    } while (temp2 != NULL);
    assign_crowding_distance_list(new_pop, cur->child, front_size, j, sorted_index);
    temp2 = cur->child;
    do {
      j++;
      temp2 = del_node(temp2);
      temp2 = temp2->child;
    } while (cur->child != NULL);
    rank += 1;
  } while (orig->child != NULL);
  free(orig);
  free(cur);
  return sorted_index;
}

/**
 * @brief Initializes and fills the population structure with individuals and their objective values.
 *
 * This function allocates memory for the population's individuals, sets up their objective values based on the
 * provided column values, and initializes constraint violations if provided.
 *
 * @param pop Pointer to the population structure to be filled.
 * @param rows The number of individuals in the population.
 * @param columns The number of objective functions.
 * @param columnValue 2D array containing the objective values for each individual and objective.
 * @param maximize Array indicating which objectives should be maximized (non-zero value) or minimized.
 * @param const_violation Array containing constraint violation values for each individual, or NULL if no constraints.
 */
void fill_population(population *pop, long rows, long columns, double **columnValue, long *maximize, double *const_violation) {
  long i, j;
  if (!pop) {
    fprintf(stderr, "Null pointer passed to fill_population.\n");
    exit(1);
  }
  pop->ind = (individual *)malloc(rows * sizeof(individual));
  pop->popsize = rows;
  pop->nbin = pop->ncons = pop->nreal = 0;
  pop->nbits = NULL;
  pop->nobj = columns;
  for (i = 0; i < rows; i++) {
    pop->ind[i].xreal = NULL;
    pop->ind[i].xbin = NULL;
    pop->ind[i].gene = NULL;
    pop->ind[i].obj = (double *)malloc(sizeof(double) * pop->nobj);
    if (const_violation)
      pop->ind[i].constr_violation = const_violation[i];
    else
      pop->ind[i].constr_violation = 0;
    pop->ind[i].constr = NULL;
    pop->ind[i].rank = 0;
    pop->ind[i].crowd_dist = 0;
    for (j = 0; j < columns; j++) {
      if (maximize[j])
        pop->ind[i].obj[j] = -1.0 * columnValue[j][i];
      else
        pop->ind[i].obj[j] = columnValue[j][i];
    }
  }
  return;
}

/**
 * @brief Frees all dynamically allocated memory associated with a population.
 *
 * This function deallocates memory for all individuals in the population, including their objective values, binary variables,
 * constraints, genes, and other dynamically allocated fields.
 *
 * @param pop Pointer to the population structure whose memory is to be freed.
 */
void free_pop_mem(population *pop) {
  long i, j;
  for (i = 0; i < pop->popsize; i++) {
    if (pop->ind[i].xreal)
      free(pop->ind[i].xreal);
    if (pop->ind[i].obj)
      free(pop->ind[i].obj);
    if (pop->ind[i].xbin)
      free(pop->ind[i].xbin);
    if (pop->ind[i].constr)
      free(pop->ind[i].constr);
    for (j = 0; j < pop->nbin; j++)
      if (pop->ind[i].gene && pop->ind[i].gene[j])
        free(pop->ind[i].gene[j]);
    if (pop->ind[i].gene)
      free(pop->ind[i].gene);
  }
  if (pop->ind)
    free(pop->ind);
  if (pop->nbits)
    free(pop->nbits);
}
