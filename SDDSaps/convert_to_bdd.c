/**
 * @file convert_to_bdd.c
 * @brief Converts a fault tree database in SDDS format to a Binary Decision Diagram (BDD) representation.
 *
 * @details
 * This program processes a fault tree database, computes the fault probabilities of each base element
 * within the fault sub-trees, and outputs the results in a specified format. It supports various options
 * to customize the processing, including specifying known good or bad elements, piping input/output,
 * and verbose output for detailed processing information.
 *
 * @section Usage
 * ```
 * convert_to_bdd <database_file> <output_file>
 *                [-pipe=[input][,output]]
 *                [-goodElements=<list_of_base_IDs>]
 *                [-badElements=<list_of_base_IDs>]
 *                [-verbose]
 *                [-failedSubTrees=<list_of_sub_tree_IDs>]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                      |
 * |---------------------------------------|--------------------------------------------------|
 * | `-pipe`                               | Enables piping for input and/or output.         |
 * | `-goodElements`                       | Base elements with fault probability 0.         |
 * | `-badElements`                        | Base elements with fault probability 1.         |
 * | `-verbose`                            | Enables verbose output.                         |
 * | `-failedSubTrees`                     | List of sub-trees to compute (default: all).   |
 *
 * @subsection Incompatibilities
 *   - Only one of the following can be specified:
 *     - `-goodElements`
 *     - `-badElements`
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author
 * H. Shang, R. Soliday
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include <sys/types.h>

/* Enumeration for option types */
enum option_type {
  CLO_GOOD_ELEMENTS,
  CLO_BAD_ELEMENTS,
  CLO_FAILED_SUB_TREES,
  CLO_PIPE,
  CLO_VERBOSE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "goodElements",
  "badElements",
  "failedSubTrees",
  "pipe",
  "verbose"
};

static char *USAGE =
  "Usage: convert_to_bdd [<database_file>] [<output_file>]\n"
  "                      [-pipe=[input][,output]]\n"
  "                      [-goodElements=<list_of_base_IDs>]\n"
  "                      [-badElements=<list_of_base_IDs>]\n"
  "                      [-verbose]\n"
  "                      [-failedSubTrees=<list_of_sub_tree_IDs>]\n"
  "Options:\n"
  "  -pipe=<input>,<output>         Enable piping for input and/or output.\n"
  "  -goodElements=<base_IDs>       Comma-separated list of base elements with fault probability 0.\n"
  "  -badElements=<base_IDs>        Comma-separated list of base elements with fault probability 1.\n"
  "  -failedSubTrees=<sub_tree_IDs> Comma-separated list of sub-trees to compute.\n"
  "  -verbose                       Enable verbose output for detailed processing information.\n\n";

/* Structure Definitions */
typedef struct {
  long id;
  double probability, ps, pes, MIF, DIF;
  char *label;
  char *guidance, *description;
} BASE;

typedef struct {
  BASE *base;  /* First element is base */
  void *left;  /* Second element is a pointer, could be base or ITE */
  void *right; /* Third element is a pointer, could be base or ITE */
  short is_base;
  double probability, p1, p0, ps, pes;
  double MIF, DIF;
  char *description, *guidance, *label;
  long ID;
} ITE;

typedef struct {
  ITE **ite_ptr;
  long ites;
  long *tree_ID;
  int32_t ID;
  char *description, *tree_name, *typeDesc;
  short type;     /* 0 for AND, 1 for OR */
  short all_base; /* Members of sub-tree are all base elements */
  short calculated;
  double p1, p0, ps;
  ITE *CAL_ITE;
} SUB_TREE;

/* Function Prototypes */
ITE *bdd_ite_cal(ITE *ite1, ITE *ite2, short type);
ITE *bdd_base_ite_cal(BASE *base, ITE *ite, short type);
ITE *bdd_base_base_cal(BASE *base1, BASE *base2, short type);
void load_data_base(char *filename);
void print_sub_tree(ITE *ite);
short is_base(ITE *ite);
void free_cal_ite(ITE *ite);
void locate_tree_id();
double compute_ps(ITE *ite);
void SetupOutputFile(char *outputFile, SDDS_DATASET *outData);

/* Stack Definitions */
#define STACKSIZE 2000
long treeStack1[STACKSIZE];
long treeStackptr1 = 0;
long treeStack[STACKSIZE];
long treeStackptr = 0;

long push_node(long ID);
long pop_node(void);
void push_sub_tree(ITE *ite);
void push_sub_tree1(ITE *ite);
double compute_ps(ITE *ite);
void compute_sub_tree_ps(ITE *ite, SDDS_DATASET *outData, long treeIndex);
long push_node1(long ID);
long pop_node1(void);

/* Global Variables */
static BASE **base = NULL;
static long bases = 0;
static SUB_TREE *sub_tree = NULL;
static long sub_trees = 0;
static ITE **ite = NULL;
static long ites = 0;

static ITE **ite_ptr = NULL;
static long ite_ptrs = 0;
static long verbose = 0;

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET outData;
  long i, j, alldone = 0, ready, tree_ID;
  char *inputFile = NULL, *outputFile = NULL;
  ITE *ite1, *ite2;
  char **badBase = NULL, **goodBase = NULL, **failedTree = NULL;
  long badBases = 0, goodBases = 0, failedTrees = 0, i_arg, tmpfile_used = 0, compute = 0;
  unsigned long pipeFlags = 0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "Error: Insufficient arguments provided.\n\n%s", USAGE);
    exit(EXIT_FAILURE);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_GOOD_ELEMENTS:
        goodBases = s_arg[i_arg].n_items - 1;
        goodBase = malloc(sizeof(*goodBase) * goodBases);
        if (!goodBase) {
          fprintf(stderr, "Error: Memory allocation failed for goodBase.\n");
          exit(EXIT_FAILURE);
        }
        for (i = 0; i < goodBases; i++)
          goodBase[i] = s_arg[i_arg].list[i + 1];
        break;
      case CLO_BAD_ELEMENTS:
        badBases = s_arg[i_arg].n_items - 1;
        badBase = malloc(sizeof(*badBase) * badBases);
        if (!badBase) {
          fprintf(stderr, "Error: Memory allocation failed for badBase.\n");
          exit(EXIT_FAILURE);
        }
        for (i = 0; i < badBases; i++)
          badBase[i] = s_arg[i_arg].list[i + 1];
        break;
      case CLO_FAILED_SUB_TREES:
        failedTrees = s_arg[i_arg].n_items - 1;
        failedTree = malloc(sizeof(*failedTree) * failedTrees);
        if (!failedTree) {
          fprintf(stderr, "Error: Memory allocation failed for failedTree.\n");
          exit(EXIT_FAILURE);
        }
        for (i = 0; i < failedTrees; i++)
          failedTree[i] = s_arg[i_arg].list[i + 1];
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "Error: Invalid -pipe syntax.\n");
          exit(EXIT_FAILURE);
        }
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      default:
        fprintf(stderr, "Unknown option: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    } else {
      if (!inputFile)
        inputFile = s_arg[i_arg].list[0];
      else if (!outputFile)
        outputFile = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "Error: Too many filenames provided (%s).\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    }
  }

  processFilenames("convert_to_bdd", &inputFile, &outputFile, pipeFlags, 1, &tmpfile_used);

  load_data_base(inputFile);
  locate_tree_id();

  if (goodBases) {
    for (i = 0; i < goodBases; i++) {
      for (j = 0; j < bases; j++) {
        if (strcmp(goodBase[i], base[j]->label) == 0) {
          base[j]->probability = 0.0;
          break;
        }
      }
    }
    free(goodBase);
  }

  if (badBases) {
    for (i = 0; i < badBases; i++) {
      for (j = 0; j < bases; j++) {
        if (strcmp(badBase[i], base[j]->label) == 0) {
          base[j]->probability = 1.0;
          break;
        }
      }
    }
    free(badBase);
  }

  SetupOutputFile(outputFile, &outData);

  for (i = 0; i < sub_trees; i++) {
    compute = 0;
    if (sub_tree[i].all_base) {
      if (verbose) {
        fprintf(stdout, "\nSub-tree Name: %s, ID: %d, ITE Structure:\n", sub_tree[i].tree_name, sub_tree[i].ID);
        print_sub_tree(sub_tree[i].CAL_ITE);
      }
      if (failedTrees) {
        for (j = 0; j < failedTrees; j++) {
          if (strcmp(sub_tree[i].tree_name, failedTree[j]) == 0) {
            compute = 1;
            break;
          }
        }
      } else {
        compute = 1;
      }
      if (compute)
        compute_sub_tree_ps(sub_tree[i].CAL_ITE, &outData, i);
    }
  }

  while (1) {
    alldone = 1;
    for (i = 0; i < sub_trees; i++) {
      if (sub_tree[i].calculated)
        continue;
      else {
        alldone = 0;
        ready = 1;
        for (j = 0; j < sub_tree[i].ites; j++) {
          tree_ID = sub_tree[i].tree_ID[j];
          if (tree_ID >= 0 && sub_tree[tree_ID].calculated == 0) {
            ready = 0;
            break;
          }
        }
        if (ready) {
          if (sub_tree[i].ites == 1)
            sub_tree[i].CAL_ITE = sub_tree[i].ite_ptr[0];
          else {
            if (sub_tree[i].ite_ptr[0]->base && is_base(sub_tree[i].ite_ptr[0]))
              ite1 = sub_tree[i].ite_ptr[0];
            else
              ite1 = sub_tree[sub_tree[i].tree_ID[0]].CAL_ITE;

            if (sub_tree[i].ite_ptr[1]->base && is_base(sub_tree[i].ite_ptr[1]))
              ite2 = sub_tree[i].ite_ptr[1];
            else
              ite2 = sub_tree[sub_tree[i].tree_ID[1]].CAL_ITE;

            sub_tree[i].CAL_ITE = bdd_ite_cal(ite1, ite2, sub_tree[i].type);

            for (j = 2; j < sub_tree[i].ites; j++) {
              if (sub_tree[i].ite_ptr[j]->base && is_base(sub_tree[i].ite_ptr[j]))
                ite1 = sub_tree[i].ite_ptr[j];
              else
                ite1 = sub_tree[sub_tree[i].tree_ID[j]].CAL_ITE;

              sub_tree[i].CAL_ITE = bdd_ite_cal(sub_tree[i].CAL_ITE, ite1, sub_tree[i].type);
            }

            if (verbose) {
              fprintf(stdout, "\nSub-tree Name: %s, ID: %d, ITE Structure:\n", sub_tree[i].tree_name, sub_tree[i].ID);
              print_sub_tree(sub_tree[i].CAL_ITE);
            }

            compute = 0;
            if (failedTrees) {
              for (j = 0; j < failedTrees; j++) {
                if (strcmp(sub_tree[i].tree_name, failedTree[j]) == 0) {
                  compute = 1;
                  break;
                }
              }
            } else {
              compute = 1;
            }
            if (compute)
              compute_sub_tree_ps(sub_tree[i].CAL_ITE, &outData, i);
          }
          sub_tree[i].calculated = 1;
        }
      }
    }
    if (alldone)
      break;
  }

  if (failedTrees)
    free(failedTree);

  if (!SDDS_Terminate(&outData))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  /* Freeing allocated memory */
  for (i = 0; i < bases; i++) {
    free(base[i]->guidance);
    free(base[i]->label);
    free(base[i]->description);
    free(base[i]);
  }
  free(base);

  for (i = 0; i < ites; i++) {
    if (ite[i]->left)
      free((ITE *)(ite[i]->left));
    if (ite[i]->right)
      free((ITE *)(ite[i]->right));
    free(ite[i]->description);
    free(ite[i]->guidance);
    free(ite[i]->label);
    free(ite[i]);
  }
  free(ite);

  for (i = 0; i < sub_trees; i++) {
    free(sub_tree[i].description);
    free(sub_tree[i].tree_name);
    free(sub_tree[i].typeDesc);
    free(sub_tree[i].ite_ptr);
    free(sub_tree[i].tree_ID);
  }
  free(sub_tree);

  for (i = 0; i < ite_ptrs; i++) {
    if (ite_ptr[i]->guidance)
      free(ite_ptr[i]->guidance);
    if (ite_ptr[i]->description)
      free(ite_ptr[i]->description);
    if (ite_ptr[i]->label)
      free(ite_ptr[i]->label);
    free(ite_ptr[i]);
  }
  free(ite_ptr);
  free_scanargs(&s_arg, argc);

  return EXIT_SUCCESS;
}

short is_base(ITE *ite) {
  return (ite->left == NULL && ite->right == NULL) || ite->is_base;
}

ITE *bdd_base_base_cal(BASE *base1, BASE *base2, short type) {
  ITE *tmp = calloc(1, sizeof(*tmp));
  BASE *base_s, *base_b;

  if (!tmp) {
    fprintf(stderr, "Error: Memory allocation failed in bdd_base_base_cal.\n");
    exit(EXIT_FAILURE);
  }

  tmp->left = tmp->right = NULL;

  /* Remember allocated ITE pointers for later freeing */
  ite_ptr = SDDS_Realloc(ite_ptr, sizeof(*ite_ptr) * (ite_ptrs + 1));
  if (!ite_ptr) {
    fprintf(stderr, "Error: Memory reallocation failed in bdd_base_base_cal.\n");
    exit(EXIT_FAILURE);
  }
  ite_ptr[ite_ptrs++] = tmp;

  if (base1->id == base2->id) {
    /* a + a = a; a * a = a */
    tmp->base = base1;
    tmp->is_base = 1;
    return tmp;
  }

  if (base1->id < base2->id) {
    base_s = base1;
    base_b = base2;
  } else {
    base_s = base2;
    base_b = base1;
  }

  if (type == 0) {
    /* AND logic: ITE(base_s, base_b, 0) */
    tmp->base = base_s;
    tmp->left = calloc(1, sizeof(ITE));
    if (!tmp->left) {
      fprintf(stderr, "Error: Memory allocation failed for tmp->left in bdd_base_base_cal.\n");
      exit(EXIT_FAILURE);
    }
    ((ITE *)tmp->left)->base = base_b;
    ((ITE *)tmp->left)->left = ((ITE *)tmp->left)->right = NULL;

    tmp->right = NULL;

    ite_ptr = SDDS_Realloc(ite_ptr, sizeof(*ite_ptr) * (ite_ptrs + 1));
    if (!ite_ptr) {
      fprintf(stderr, "Error: Memory reallocation failed for tmp->left in bdd_base_base_cal.\n");
      exit(EXIT_FAILURE);
    }
    ite_ptr[ite_ptrs++] = (ITE *)tmp->left;
  } else {
    /* OR logic: ITE(base_s, 1, base_b) */
    tmp->base = base_s;
    tmp->left = NULL;
    tmp->right = calloc(1, sizeof(ITE));
    if (!tmp->right) {
      fprintf(stderr, "Error: Memory allocation failed for tmp->right in bdd_base_base_cal.\n");
      exit(EXIT_FAILURE);
    }
    ((ITE *)tmp->right)->base = base_b;
    ((ITE *)tmp->right)->left = ((ITE *)tmp->right)->right = NULL;

    ite_ptr = SDDS_Realloc(ite_ptr, sizeof(*ite_ptr) * (ite_ptrs + 1));
    if (!ite_ptr) {
      fprintf(stderr, "Error: Memory reallocation failed for tmp->right in bdd_base_base_cal.\n");
      exit(EXIT_FAILURE);
    }
    ite_ptr[ite_ptrs++] = (ITE *)tmp->right;
  }

  return tmp;
}

ITE *bdd_base_ite_cal(BASE *base, ITE *ite, short type) {
  ITE *tmp = calloc(1, sizeof(*tmp));
  ITE *left, *right;
  BASE *base1 = ite->base;

  if (!tmp) {
    fprintf(stderr, "Error: Memory allocation failed in bdd_base_ite_cal.\n");
    exit(EXIT_FAILURE);
  }

  ite_ptr = SDDS_Realloc(ite_ptr, sizeof(*ite_ptr) * (ite_ptrs + 1));
  if (!ite_ptr) {
    fprintf(stderr, "Error: Memory reallocation failed in bdd_base_ite_cal.\n");
    exit(EXIT_FAILURE);
  }
  ite_ptr[ite_ptrs++] = tmp;

  left = (ITE *)ite->left;
  right = (ITE *)ite->right;

  if (base->id < base1->id) {
    tmp->base = base;
    if (type == 1) {
      /* OR logic: ITE(base, 1, ite) */
      tmp->left = NULL;
      tmp->right = ite;
    } else {
      /* AND logic: ITE(base, ite, 0) */
      tmp->left = ite;
      tmp->right = NULL;
    }
  } else if (base->id > base1->id) {
    tmp->base = ite->base;
    if (!left) {
      if (type == 0) {
        tmp->left = calloc(1, sizeof(ITE));
        if (!tmp->left) {
          fprintf(stderr, "Error: Memory allocation failed for tmp->left in bdd_base_ite_cal.\n");
          exit(EXIT_FAILURE);
        }
        ((ITE *)tmp->left)->base = base;
        ((ITE *)tmp->left)->left = ((ITE *)tmp->left)->right = NULL;

        ite_ptr = SDDS_Realloc(ite_ptr, sizeof(*ite_ptr) * (ite_ptrs + 1));
        if (!ite_ptr) {
          fprintf(stderr, "Error: Memory reallocation failed for tmp->left in bdd_base_ite_cal.\n");
          exit(EXIT_FAILURE);
        }
        ite_ptr[ite_ptrs++] = (ITE *)tmp->left;
      } else {
        tmp->left = NULL; /* OR logic: 1 */
      }
    } else {
      tmp->left = bdd_base_ite_cal(base, (ITE *)ite->left, type);
    }

    if (!right) {
      if (type == 0)
        tmp->right = NULL; /* AND logic: 0 */
      else {
        tmp->right = calloc(1, sizeof(ITE));
        if (!tmp->right) {
          fprintf(stderr, "Error: Memory allocation failed for tmp->right in bdd_base_ite_cal.\n");
          exit(EXIT_FAILURE);
        }
        ((ITE *)tmp->right)->base = base;
        ((ITE *)tmp->right)->left = ((ITE *)tmp->right)->right = NULL;

        ite_ptr = SDDS_Realloc(ite_ptr, sizeof(*ite_ptr) * (ite_ptrs + 1));
        if (!ite_ptr) {
          fprintf(stderr, "Error: Memory reallocation failed for tmp->right in bdd_base_ite_cal.\n");
          exit(EXIT_FAILURE);
        }
        ite_ptr[ite_ptrs++] = (ITE *)tmp->right;
      }
    } else {
      tmp->right = bdd_base_ite_cal(base, (ITE *)ite->right, type);
    }
  } else {
    tmp->base = base;
    if (type == 0) {
      tmp->left = left;  /* AND logic: 1 * left */
      tmp->right = NULL; /* AND logic: 0 */
    } else {
      tmp->left = NULL;   /* OR logic: 1 */
      tmp->right = right; /* OR logic: right */
    }
  }

  return tmp;
}

ITE *bdd_ite_cal(ITE *ite1, ITE *ite2, short type) {
  ITE *ite = calloc(1, sizeof(*ite));
  ITE *ite_l, *ite_r;
  BASE *base1 = NULL, *base2 = NULL;

  if (!ite) {
    fprintf(stderr, "Error: Memory allocation failed in bdd_ite_cal.\n");
    exit(EXIT_FAILURE);
  }

  if (!ite1 || !ite2) {
    fprintf(stderr, "Error: Null pointer provided to bdd_ite_cal().\n");
    exit(EXIT_FAILURE);
  }

  if (is_base(ite1) && is_base(ite2))
    return bdd_base_base_cal(ite1->base, ite2->base, type);

  if (is_base(ite1))
    return bdd_base_ite_cal(ite1->base, ite2, type);
  if (is_base(ite2))
    return bdd_base_ite_cal(ite2->base, ite1, type);

  base1 = ite1->base;
  base2 = ite2->base;

  ite_ptr = SDDS_Realloc(ite_ptr, sizeof(*ite_ptr) * (ite_ptrs + 1));
  if (!ite_ptr) {
    fprintf(stderr, "Error: Memory reallocation failed in bdd_ite_cal.\n");
    exit(EXIT_FAILURE);
  }
  ite_ptr[ite_ptrs++] = ite;

  if (base1->id == base2->id) {
    ite->base = ite1->base;
    if (ite1->left && ite2->left)
      ite->left = bdd_ite_cal((ITE *)ite1->left, (ITE *)ite2->left, type);
    else {
      if (type == 1)
        ite->left = NULL; /* OR logic: 1 */
      else {
        /* AND logic: any left */
        ite->left = ite1->left ? ite1->left : ite2->left;
      }
    }

    if (ite1->right && ite2->right)
      ite->right = bdd_ite_cal((ITE *)ite1->right, (ITE *)ite2->right, type);
    else {
      if (type == 0)
        ite->right = NULL; /* AND logic: 0 */
      else {
        /* OR logic: any right */
        ite->right = ite1->right ? ite1->right : ite2->right;
      }
    }
  } else {
    if (base1->id < base2->id) {
      ite_l = ite1;
      ite_r = ite2;
    } else {
      ite_l = ite2;
      ite_r = ite1;
    }
    ite->base = ite_l->base;

    if (ite_l->left)
      ite->left = bdd_ite_cal(ite_l->left, ite_r, type);
    else {
      if (type == 0)
        ite->left = ite_r;
      else
        ite->left = NULL;
    }

    if (ite_l->right)
      ite->right = bdd_ite_cal(ite_l->right, ite_r, type);
    else {
      if (type == 0)
        ite->right = NULL;
      else
        ite->right = ite_r;
    }
  }

  return ite;
}

void load_data_base(char *filename) {
  int32_t i, rows, pages, j, allbase, index;
  int32_t *id = NULL;
  double *prob = NULL;
  char **guid = NULL, **desc = NULL, **label = NULL;
  SDDS_DATASET table;

  if (!SDDS_InitializeInput(&table, filename))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  pages = 0;
  sub_tree = NULL;
  ite = NULL;

  while (SDDS_ReadPage(&table) > 0) {
    if (!(rows = SDDS_CountRowsOfInterest(&table)))
      continue;

    sub_tree = SDDS_Realloc(sub_tree, sizeof(*sub_tree) * (pages + 1));
    if (!sub_tree) {
      fprintf(stderr, "Error: Memory reallocation failed for sub_tree.\n");
      exit(EXIT_FAILURE);
    }

    sub_tree[pages].ites = rows;
    sub_tree[pages].ite_ptr = malloc(sizeof(ITE *) * rows);
    if (!sub_tree[pages].ite_ptr) {
      fprintf(stderr, "Error: Memory allocation failed for sub_tree[%" PRId32 "].ite_ptr.\n", pages);
      exit(EXIT_FAILURE);
    }

    sub_tree[pages].calculated = 0;

    if (!SDDS_GetParameter(&table, "Description", &sub_tree[pages].description) ||
        !SDDS_GetParameter(&table, "ID", &sub_tree[pages].ID) ||
        !SDDS_GetParameter(&table, "LogicalType", &sub_tree[pages].type) ||
        !SDDS_GetParameter(&table, "LogicalTypeDesc", &sub_tree[pages].typeDesc) ||
        !SDDS_GetParameter(&table, "TreeName", &sub_tree[pages].tree_name))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!(id = SDDS_GetColumn(&table, "ID")) ||
        !(prob = SDDS_GetColumn(&table, "Probability")) ||
        !(desc = SDDS_GetColumn(&table, "Description")) ||
        !(guid = SDDS_GetColumn(&table, "Guidance")) ||
        !(label = SDDS_GetColumn(&table, "Label")))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    allbase = 1;
    for (i = 0; i < rows; i++) {
      if (id[i] > 1000) {
        /* Base element */
        index = -1;
        if (bases) {
          for (j = 0; j < bases; j++) {
            if (id[i] == base[j]->id) {
              index = j;
              break;
            }
          }
        }

        if (index < 0) {
          base = SDDS_Realloc(base, sizeof(*base) * (bases + 1));
          if (!base) {
            fprintf(stderr, "Error: Memory reallocation failed for base.\n");
            exit(EXIT_FAILURE);
          }
          base[bases] = calloc(1, sizeof(**base));
          if (!base[bases]) {
            fprintf(stderr, "Error: Memory allocation failed for base[%ld].\n", bases);
            exit(EXIT_FAILURE);
          }
          base[bases]->id = id[i];
          base[bases]->probability = prob[i];
          SDDS_CopyString(&base[bases]->guidance, guid[i]);
          SDDS_CopyString(&base[bases]->label, label[i]);
          SDDS_CopyString(&base[bases]->description, desc[i]);
          index = bases;
          bases++;
        }

        sub_tree[pages].ite_ptr[i] = calloc(1, sizeof(ITE));
        if (!sub_tree[pages].ite_ptr[i]) {
          fprintf(stderr, "Error: Memory allocation failed for sub_tree[%" PRId32 "].ite_ptr[%d].\n", pages, i);
          exit(EXIT_FAILURE);
        }

        sub_tree[pages].ite_ptr[i]->base = base[index];
        sub_tree[pages].ite_ptr[i]->left = sub_tree[pages].ite_ptr[i]->right = NULL;
        sub_tree[pages].ite_ptr[i]->ID = id[i];
        sub_tree[pages].ite_ptr[i]->probability = prob[i];
        sub_tree[pages].ite_ptr[i]->is_base = 1;
        SDDS_CopyString(&sub_tree[pages].ite_ptr[i]->guidance, guid[i]);
        SDDS_CopyString(&sub_tree[pages].ite_ptr[i]->label, label[i]);
        SDDS_CopyString(&sub_tree[pages].ite_ptr[i]->description, desc[i]);

        /* Remember allocated ITE pointers for later freeing */
        ite_ptr = SDDS_Realloc(ite_ptr, sizeof(*ite_ptr) * (ite_ptrs + 1));
        if (!ite_ptr) {
          fprintf(stderr, "Error: Memory reallocation failed for ite_ptr in load_data_base.\n");
          exit(EXIT_FAILURE);
        }
        ite_ptr[ite_ptrs++] = sub_tree[pages].ite_ptr[i];
      } else {
        /* Non-base ITE */
        index = -1;
        if (ites) {
          for (j = 0; j < ites; j++) {
            if (id[i] == ite[j]->ID) {
              index = j;
              break;
            }
          }
        }

        if (index < 0) {
          ite = SDDS_Realloc(ite, sizeof(*ite) * (ites + 1));
          if (!ite) {
            fprintf(stderr, "Error: Memory reallocation failed for ite.\n");
            exit(EXIT_FAILURE);
          }
          ite[ites] = calloc(1, sizeof(ITE));
          if (!ite[ites]) {
            fprintf(stderr, "Error: Memory allocation failed for ite[%ld].\n", ites);
            exit(EXIT_FAILURE);
          }
          ite[ites]->ID = id[i];
          ite[ites]->base = NULL;
          ite[ites]->probability = prob[i];
          ite[ites]->is_base = 0;
          SDDS_CopyString(&ite[ites]->guidance, guid[i]);
          SDDS_CopyString(&ite[ites]->description, desc[i]);
          SDDS_CopyString(&ite[ites]->label, label[i]);
          index = ites;
          ites++;
        }

        sub_tree[pages].ite_ptr[i] = ite[index];
        allbase = 0;
      }
    }

    sub_tree[pages].ites = rows;
    sub_tree[pages].all_base = allbase;

    /* Compute base sub-tree */
    if (allbase) {
      if (rows == 1) {
        sub_tree[pages].CAL_ITE = sub_tree[pages].ite_ptr[0];
      } else {
        sub_tree[pages].CAL_ITE = bdd_ite_cal(sub_tree[pages].ite_ptr[0], sub_tree[pages].ite_ptr[1], sub_tree[pages].type);
        for (j = 2; j < rows; j++) {
          sub_tree[pages].CAL_ITE = bdd_ite_cal(sub_tree[pages].CAL_ITE, sub_tree[pages].ite_ptr[j], sub_tree[pages].type);
        }
      }
      sub_tree[pages].calculated = 1;
    }

    /* Free temporary arrays */
    free(id);
    free(prob);
    SDDS_FreeStringArray(guid, rows);
    SDDS_FreeStringArray(desc, rows);
    SDDS_FreeStringArray(label, rows);
    free(label);
    free(guid);
    free(desc);

    pages++;
  }

  if (verbose)
    fprintf(stdout, "Total Bases: %ld, Total ITEs: %ld\n", bases, ites);

  sub_trees = pages;

  if (!SDDS_Terminate(&table))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

long push_node(long ID) {
  if (treeStackptr >= STACKSIZE) {
    fprintf(stderr, "Error: Stack overflow.\n");
    exit(EXIT_FAILURE);
  }
  treeStack[treeStackptr++] = ID;
  return 1;
}

long pop_node(void) {
  if (treeStackptr < 1) {
    fprintf(stderr, "Error: Too few items on stack.\n");
    exit(EXIT_FAILURE);
  }
  return treeStack[--treeStackptr];
}

void print_sub_tree(ITE *ite) {
  ITE *left, *right;
  if (!ite->base)
    return;
  push_node(ite->base->id);
  fprintf(stdout, "%ld\n", pop_node());
  left = (ITE *)ite->left;
  right = (ITE *)ite->right;
  if (!left)
    push_node(1);
  else
    push_node(left->base->id);
  fprintf(stdout, "%ld ", pop_node());
  if (!right)
    push_node(0);
  else
    push_node(right->base->id);
  fprintf(stdout, "%ld \n", pop_node());
  if (left)
    print_sub_tree(left);
  if (right)
    print_sub_tree(right);
}

void free_cal_ite(ITE *ite) {
  ITE *left, *right;
  if (!ite)
    return;
  left = (ITE *)(ite->left);
  right = (ITE *)(ite->right);
  if (!left && !right) {
    free(ite);
    return;
  }
  if (left) {
    if (is_base(left)) {
      free(left);
    } else {
      free_cal_ite(left);
    }
  }
  if (right) {
    if (is_base(right)) {
      free(right);
    } else {
      free_cal_ite(right);
    }
  }
}

void locate_tree_id() {
  long i, j, k;
  for (i = 0; i < sub_trees; i++) {
    sub_tree[i].tree_ID = malloc(sizeof(long) * sub_tree[i].ites);
    if (!sub_tree[i].tree_ID) {
      fprintf(stderr, "Error: Memory allocation failed for sub_tree[%ld].tree_ID.\n", i);
      exit(EXIT_FAILURE);
    }
    for (j = 0; j < sub_tree[i].ites; j++) {
      sub_tree[i].tree_ID[j] = -1;
      if (!(sub_tree[i].ite_ptr[j]->base)) {
        for (k = 0; k < sub_trees; k++) {
          if (sub_tree[i].ite_ptr[j]->ID == sub_tree[k].ID) {
            sub_tree[i].tree_ID[j] = k;
            break;
          }
        }
        if (sub_tree[i].tree_ID[j] < 0) {
          fprintf(stderr, "Error: No tree_ID found for ITE %ld of tree %ld (%s).\n",
                  j, i, sub_tree[i].tree_name);
          exit(EXIT_FAILURE);
        }
      }
    }
  }
}

void push_sub_tree(ITE *ite) {
  ITE *left, *right;
  push_node((long)ite);
  left = ite->left;
  right = ite->right;
  if (!left)
    push_node(1);
  else
    push_node((long)left);
  if (!right)
    push_node(0);
  else
    push_node((long)right);
  if (left)
    push_sub_tree(left);
  if (right)
    push_sub_tree(right);
}

double compute_ps(ITE *ite) {
  ITE *left, *right, *c_ite = NULL;
  long left_adr, right_adr;
  double p;
  push_sub_tree1(ite);
  while (treeStackptr1 > 0) {
    right_adr = pop_node1();
    left_adr = pop_node1();
    c_ite = (ITE *)pop_node1();
    p = c_ite->base->probability;
    if (left_adr == 1) {
      c_ite->p1 = 1.0;
    } else {
      left = (ITE *)left_adr;
      c_ite->p1 = left->ps;
    }
    if (right_adr == 0) {
      c_ite->p0 = 0.0;
    } else {
      right = (ITE *)right_adr;
      c_ite->p0 = right->ps;
    }
    c_ite->ps = p * c_ite->p1 + (1.0 - p) * c_ite->p0;
  }
  return c_ite->ps;
}

void compute_sub_tree_ps(ITE *ite, SDDS_DATASET *outData, long treeIndex) {
  ITE *t_ite;
  long this_adr;
  long t_bases = 0, i, index;
  BASE **t_base = NULL, *base1;
  double prob, PS;

  /* First compute system PS */
  PS = compute_ps(ite);

  push_sub_tree(ite);
  while (treeStackptr > 0) {
    this_adr = pop_node();
    if (this_adr != 1 && this_adr != 0) {
      t_ite = (ITE *)this_adr;
      base1 = t_ite->base;
      index = -1;
      if (!t_bases) {
        t_base = SDDS_Realloc(t_base, sizeof(*t_base) * (t_bases + 1));
        if (!t_base) {
          fprintf(stderr, "Error: Memory reallocation failed for t_base in compute_sub_tree_ps.\n");
          exit(EXIT_FAILURE);
        }
        t_base[t_bases] = base1;
        t_bases++;
      } else {
        for (i = 0; i < t_bases; i++) {
          if (base1->id == t_base[i]->id) {
            index = i;
            break;
          }
        }
        if (index < 0) {
          t_base = SDDS_Realloc(t_base, sizeof(*t_base) * (t_bases + 1));
          if (!t_base) {
            fprintf(stderr, "Error: Memory reallocation failed for t_base in compute_sub_tree_ps.\n");
            exit(EXIT_FAILURE);
          }
          t_base[t_bases] = base1;
          t_bases++;
        }
      }
      if (index >= 0)
        continue;

      prob = base1->probability; /* Remember original probability */

      /* Compute ps when probability = 1 */
      base1->probability = 1.0;
      base1->ps = compute_ps(ite);

      /* Compute pes when probability = 0 */
      base1->probability = 0.0;
      base1->pes = compute_ps(ite);

      /* Restore original probability */
      base1->probability = prob;

      base1->MIF = base1->ps - base1->pes;
      base1->DIF = prob + prob * (1.0 - prob) * base1->MIF / PS;

      if (verbose)
        fprintf(stdout, "Base %ld: prob=%.6f, ps=%.6f, pes=%.6f, MIF=%.6f, DIF=%.6f\n",
                base1->id, base1->probability, base1->ps, base1->pes, base1->MIF, base1->DIF);
    }
  }

  if (!SDDS_StartPage(outData, t_bases))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_SetParameters(outData, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "TreeName", sub_tree[treeIndex].tree_name,
                          "Description", sub_tree[treeIndex].description,
                          "LogicalType", sub_tree[treeIndex].type,
                          "LogicalTypeDesc", sub_tree[treeIndex].typeDesc,
                          "ID", sub_tree[treeIndex].ID, NULL))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  for (i = 0; i < t_bases; i++) {
    if (!SDDS_SetRowValues(outData, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                           "BaseID", t_base[i]->id,
                           "Label", t_base[i]->label,
                           "Probability", t_base[i]->probability,
                           "DIF", t_base[i]->DIF,
                           "MIF", t_base[i]->MIF,
                           "PS", t_base[i]->ps,
                           "PES", t_base[i]->pes,
                           "Description", t_base[i]->description,
                           "Guidance", t_base[i]->guidance, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_WritePage(outData))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (t_base)
    free(t_base);
}

long push_node1(long ID) {
  if (treeStackptr1 >= STACKSIZE) {
    fprintf(stderr, "Error: Stack overflow in push_node1.\n");
    exit(EXIT_FAILURE);
  }
  treeStack1[treeStackptr1++] = ID;
  return 1;
}

long pop_node1(void) {
  if (treeStackptr1 < 1) {
    fprintf(stderr, "Error: Too few items on stack in pop_node1.\n");
    exit(EXIT_FAILURE);
  }
  return treeStack1[--treeStackptr1];
}

void push_sub_tree1(ITE *ite) {
  ITE *left, *right;
  push_node1((long)ite);
  left = ite->left;
  right = ite->right;
  if (!left)
    push_node1(1);
  else
    push_node1((long)left);
  if (!right)
    push_node1(0);
  else
    push_node1((long)right);
  if (left)
    push_sub_tree1(left);
  if (right)
    push_sub_tree1(right);
}

void SetupOutputFile(char *outputFile, SDDS_DATASET *outData) {
  if (!SDDS_InitializeOutput(outData, SDDS_ASCII, 1, NULL, NULL, outputFile))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_DefineSimpleParameter(outData, "TreeName", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(outData, "Description", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(outData, "LogicalType", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleParameter(outData, "LogicalTypeDesc", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(outData, "ID", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleColumn(outData, "BaseID", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleColumn(outData, "Label", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleColumn(outData, "Probability", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleColumn(outData, "DIF", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleColumn(outData, "MIF", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleColumn(outData, "PS", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleColumn(outData, "PES", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleColumn(outData, "Description", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleColumn(outData, "Guidance", NULL, SDDS_STRING))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_WriteLayout(outData))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}
