/**
 * @file test_complex_weights.c
 * @brief Exercise both complex pseudoinverse weight readers without MKL.
 * @copyright Copyright (c) 2026 UChicago Argonne, LLC.
 * @license Distributed under the Software License Agreement in LICENSE.
 */
#define SDDS_COMPLEX_WEIGHTS_TEST
#include "sddscomplexpseudoinverse.c"

int main(int argc, char **argv) {
  char *rowNames[] = {"BPM1", "BPM2", "BPM3"};
  char *columnNames[] = {"C1", "C2", "C3"};
  double *weights = NULL, *corrWeights = NULL;
  long includeWeights, includeCorrWeights;

  if (argc != 6)
    return EXIT_FAILURE;
  SDDS_RegisterProgramName(argv[0]);
  includeWeights = !strcmp(argv[1], "bpm") || !strcmp(argv[1], "both");
  includeCorrWeights = !strcmp(argv[1], "corrector") || !strcmp(argv[1], "both");
  if (!includeWeights && !includeCorrWeights)
    return EXIT_FAILURE;
  readComplexWeightInputs(strtol(argv[4], NULL, 10), 0, strtol(argv[5], NULL, 10),
                          includeWeights, argv[2], "Name", "Weight",
                          includeCorrWeights, argv[3], "Name", "Weight",
                          3, rowNames, 3, columnNames, &weights, &corrWeights);
  if (weights)
    printf("bpm %.17g %.17g %.17g\n", weights[0], weights[1], weights[2]);
  if (corrWeights)
    printf("corrector %.17g %.17g %.17g\n", corrWeights[0], corrWeights[1], corrWeights[2]);
  free(weights);
  free(corrWeights);
  return EXIT_SUCCESS;
}
