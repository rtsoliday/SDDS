/**
 * @file test_complex_names.c
 * @brief Exercise production complex-matrix naming and reconstruction helpers without MKL.
 * @copyright Copyright (c) 2026 UChicago Argonne, LLC.
 * @license Distributed under the Software License Agreement in LICENSE.
 */
#define SDDS_COMPLEX_NAMES_TEST
#include "sddscomplexpseudoinverse.c"

int main(int argc, char **argv) {
  SDDS_DATASET input, output, reconstruction;
  char **names = NULL, *automatic = NULL, *requested, *root;
  double *real, *imag;
  int32_t i, j, page, count = 0;

  if (argc != 6)
    return EXIT_FAILURE;
  SDDS_RegisterProgramName(argv[0]);
  requested = strcmp(argv[4], "-") ? argv[4] : NULL;
  root = strcmp(argv[5], "-") ? argv[5] : NULL;
  if (requested && root)
    SDDS_Bomb("-root and -newColumnNames are incompatible");
  if (!SDDS_InitializeInput(&input, argv[1]))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (requested && !checkComplexNameColumn(&input, requested))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < input.layout.n_columns; i++) {
    if (input.layout.column_definition[i].type == SDDS_STRING) {
      automatic = input.layout.column_definition[i].name;
      break;
    }
  }
  while ((page = SDDS_ReadPage(&input)) > 0) {
    int64_t rows = SDDS_CountRowsOfInterest(&input);
    if (rows <= 0 || rows > INT32_MAX)
      SDDS_Bomb("No rows in dataset or too many rows.");
    if (page == 1) {
      count = (int32_t)rows;
      if (!(names = getComplexOutputNames(&input, root, automatic, requested, 3)) ||
          !initializeComplexReconstruction(&reconstruction, &input, argv[3]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (rows != count)
      SDDS_Bomb("Datasets don't have the same number of rows.");
    /* The production caller replaces only numeric values after this copy. */
    if (!copyComplexReconstructionPage(&reconstruction, &input, 2, "") ||
        !SDDS_WritePage(&reconstruction))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (page == 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!names)
    SDDS_Bomb("No data pages in dataset.");
  /* Prove that decoded names are owned independently of the source dictionary. */
  if (!SDDS_Terminate(&input) || !SDDS_Terminate(&reconstruction) ||
      !SDDS_InitializeOutput(&output, SDDS_ASCII, 1, NULL, NULL, argv[2]) ||
      !SDDS_DefineSimpleColumn(&output, "RowName", NULL, SDDS_STRING) ||
      !defineComplexNamedColumns(&output, names, count, "matrix") ||
      !SDDS_WriteLayout(&output) || !SDDS_StartPage(&output, count) ||
      !SDDS_SetColumn(&output, SDDS_SET_BY_NAME, names, count, "RowName"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  real = SDDS_Malloc(sizeof(*real) * count);
  imag = SDDS_Malloc(sizeof(*imag) * count);
  if (!real || !imag)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < count; i++) {
    for (j = 0; j < count; j++) {
      real[j] = (i + 1) * 10 + j;
      imag[j] = -real[j];
    }
    if (!setComplexNamedColumn(&output, names[i], count, real, imag))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_WritePage(&output) || !SDDS_Terminate(&output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(real);
  free(imag);
  SDDS_FreeStringArray(names, count);
  free(names);
  return EXIT_SUCCESS;
}
