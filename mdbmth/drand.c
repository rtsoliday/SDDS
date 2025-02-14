/** 
 * @file drand.c
 * @brief Random number generation functions providing various distributions 
 *        (uniform, Gaussian) and related utilities (seeding, ordering, etc.).
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include <time.h>
#include <stdlib.h>

#if defined(_WIN32)
#  if _MSC_VER <= 1600
#    include "fdlibm.h"
#  endif
#endif
#include "f2c.h"

extern double dlaran_(integer *seed);
extern double dlaran_oag(integer *seed, long increment);

#define MAX_RAND_INT (1.0 * RAND_MAX)

/**
 * @brief Generate a uniform random float in [0,1].
 *
 * Uses the standard C rand() function. The parameter `dummy` is unused.
 *
 * @param[in] dummy Unused parameter.
 * @return A random float in [0,1].
 */
float drand(long dummy) {
  return ((float)(1.0 * rand() / MAX_RAND_INT));
}

/**
 * @brief Generate a uniform random double in [lo, hi].
 *
 * @param[in] lo The lower bound of the range.
 * @param[in] hi The upper bound of the range.
 * @return A random double in [lo, hi].
 */
double rdrand(lo, hi) double lo, hi;
{
  return (lo + ((hi - lo) * rand()) / MAX_RAND_INT);
}

/* routine: tseed()
 * purpose: seed rand() with clock time
 */

void tseed() {
  srand((int)time(NULL));
}

/**
 * @brief Generate a random point (r, θ) within an annulus defined by [r_min, r_max].
 *
 * The angle θ is chosen uniformly in [0, 2π), and r is chosen so that the area distribution is uniform.
 *
 * @param[out] r     Pointer to store the generated radius.
 * @param[out] theta Pointer to store the generated angle in radians.
 * @param[in]  r_min The inner radius of the annulus.
 * @param[in]  r_max The outer radius of the annulus.
 */
void r_theta_rand(double *r, double *theta, double r_min, double r_max) {
  double area, sqr_r_min;

  *theta = rdrand(0.0, PIx2);
  sqr_r_min = sqr(r_min);
  area = rdrand(0.0, sqr(r_max) - sqr_r_min);
  *r = sqrt(area + sqr_r_min);
}

static short inhibitPermute = 0;
/**
 * @brief Enable or disable permutation of seed bits for random number generators.
 *
 * If state >= 0, sets the inhibitPermute flag. Otherwise, returns the current state without changing it.
 *
 * @param[in] state New state for inhibition (0 = no inhibition, 1 = inhibited).
 * @return The current state of the inhibitPermute flag.
 */
short inhibitRandomSeedPermutation(short state) {
  if (state < 0)
    return inhibitPermute;
  inhibitPermute = state;
  return state;
}

/**
 * @brief Permute the bit order of a seed value to improve randomness.
 *
 * Applies a permutation of the seed bits to avoid predictable patterns.
 * If inhibition is enabled, returns the original input.
 *
 * @param[in] input0 The seed value to permute.
 * @return The permuted seed value.
 */
long permuteSeedBitOrder(long input0) {
  long offset = input0 % 1000;
  long newValue;
  long i;
  unsigned long input;
  unsigned long bitMask[32] = {
    0x00000001UL,
    0x00000002UL,
    0x00000004UL,
    0x00000008UL,
    0x00000010UL,
    0x00000020UL,
    0x00000040UL,
    0x00000080UL,
    0x00000100UL,
    0x00000200UL,
    0x00000400UL,
    0x00000800UL,
    0x00001000UL,
    0x00002000UL,
    0x00004000UL,
    0x00008000UL,
    0x00010000UL,
    0x00020000UL,
    0x00040000UL,
    0x00080000UL,
    0x00100000UL,
    0x00200000UL,
    0x00400000UL,
    0x00800000UL,
    0x01000000UL,
    0x02000000UL,
    0x04000000UL,
    0x08000000UL,
    0x10000000UL,
    0x20000000UL,
    0x40000000UL,
    0x08000000UL,
  };
  if (inhibitPermute)
    return input0;

  input = input0;
  newValue = 0;
  for (i = 0; i < 31; i++) {
    newValue += (input & bitMask[i]) ? bitMask[(i + offset) % 31] : 0;
  }
  if (newValue == input0) {
    offset += 1;
    newValue = 0;
    for (i = 0; i < 31; i++) {
      newValue += (input & bitMask[i]) ? bitMask[(i + offset) % 31] : 0;
    }
  }
  return newValue;
}

/**
 * @brief Generate a uniform random double in [0,1] using a custom seed initialization.
 *
 * Initializes the random number generator if needed, and then produces a double in [0,1].
 * Negative iseed values are used to re-initialize the sequence.
 *
 * @param[in] iseed Seed for initialization if negative, otherwise ignored after first call.
 * @return A random double in [0,1].
 */
double random_1(long iseed) {
  static short initialized = 0;
  static integer seed[4] = {0, 0, 0, 0};

  if (!initialized || iseed < 0) {
    if (iseed < 0)
      iseed = -iseed;
    iseed = permuteSeedBitOrder(iseed);
    random_2(-(iseed + 2));
    random_3(-(iseed + 4));
    random_4(-(iseed + 6));
    random_5(-(iseed + 8));
    random_6(-(iseed + 10));
    iseed = (iseed / 2) * 2 + 1;
    seed[3] = (iseed & 4095);
    seed[2] = (iseed >>= 12) & 4095;
    seed[1] = (iseed >>= 12) & 4095;
    seed[0] = (iseed >>= 12) & 4095;
    initialized = 1;
  }
  if (!initialized)
    bomb("random_1 not properly initialized", NULL);

  return dlaran_(seed);
}

/**
 * @brief Similar to random_1(), provides a separate random sequence with its own seed handling.
 *
 * @param[in] iseed Seed for initialization if negative.
 * @return A random double in [0,1].
 */
double random_2(long iseed) {
  static short initialized = 0;
  static integer seed[4] = {0, 0, 0, 0};

  if (!initialized || iseed < 0) {
    if (iseed < 0)
      iseed = -iseed;
    iseed = permuteSeedBitOrder(iseed);
    seed[3] = ((iseed & 4095) / 2) * 2 + 1;
    seed[2] = (iseed >>= 12) & 4095;
    seed[1] = (iseed >>= 12) & 4095;
    seed[0] = (iseed >>= 12) & 4095;
    initialized = 1;
  }
  if (!initialized)
    bomb("random_2 not properly initialized", NULL);

  return dlaran_(seed);
}

/**
 * @brief Similar to random_2(), provides another independent random sequence.
 *
 * @param[in] iseed Seed for initialization if negative.
 * @return A random double in [0,1].
 */
double random_3(long iseed) {
  static short initialized = 0;
  static integer seed[4] = {0, 0, 0, 0};

  if (!initialized || iseed < 0) {
    if (iseed < 0)
      iseed = -iseed;
    iseed = permuteSeedBitOrder(iseed);
    seed[3] = ((iseed & 4095) / 2) * 2 + 1;
    seed[2] = (iseed >>= 12) & 4095;
    seed[1] = (iseed >>= 12) & 4095;
    seed[0] = (iseed >>= 12) & 4095;
    initialized = 1;
  }
  if (!initialized)
    bomb("random_3 not properly initialized", NULL);

  return dlaran_(seed);
}

/**
 * @brief Similar to random_3(), provides another independent random sequence.
 *
 * @param[in] iseed Seed for initialization if negative.
 * @return A random double in [0,1].
 */
double random_4(long iseed) {
  static short initialized = 0;
  static integer seed[4] = {0, 0, 0, 0};

  if (!initialized || iseed < 0) {
    if (iseed < 0)
      iseed = -iseed;
    iseed = permuteSeedBitOrder(iseed);
    seed[3] = ((iseed & 4095) / 2) * 2 + 1;
    seed[2] = (iseed >>= 12) & 4095;
    seed[1] = (iseed >>= 12) & 4095;
    seed[0] = (iseed >>= 12) & 4095;
    initialized = 1;
  }
  if (!initialized)
    bomb("random_4 not properly initialized", NULL);

  return dlaran_(seed);
}

/**
 * @brief Similar to random_4(), provides another independent random sequence.
 *
 * @param[in] iseed Seed for initialization if negative.
 * @return A random double in [0,1].
 */
double random_5(long iseed) {
  static short initialized = 0;
  static integer seed[4] = {0, 0, 0, 0};

  if (!initialized || iseed < 0) {
    if (iseed < 0)
      iseed = -iseed;
    iseed = permuteSeedBitOrder(iseed);
    seed[3] = ((iseed & 4095) / 2) * 2 + 1;
    seed[2] = (iseed >>= 12) & 4095;
    seed[1] = (iseed >>= 12) & 4095;
    seed[0] = (iseed >>= 12) & 4095;
    initialized = 1;
  }
  if (!initialized)
    bomb("random_5 not properly initialized", NULL);

  return dlaran_(seed);
}

/**
 * @brief Similar to random_5(), provides another independent random sequence.
 *
 * @param[in] iseed Seed for initialization if negative.
 * @return A random double in [0,1].
 */
double random_6(long iseed) {
  static short initialized = 0;
  static integer seed[4] = {0, 0, 0, 0};

  if (!initialized || iseed < 0) {
    if (iseed < 0)
      iseed = -iseed;
    iseed = permuteSeedBitOrder(iseed);
    seed[3] = ((iseed & 4095) / 2) * 2 + 1;
    seed[2] = (iseed >>= 12) & 4095;
    seed[1] = (iseed >>= 12) & 4095;
    seed[0] = (iseed >>= 12) & 4095;
    initialized = 1;
  }
  if (!initialized)
    bomb("random_6 not properly initialized", NULL);

  return dlaran_(seed);
}

/**
 * @brief Generate a Gaussian-distributed random number with mean 0 and sigma 1.
 *
 * Uses the given uniform random generator `urandom` to produce Gaussian deviates 
 * via the Box–Muller transform.
 *
 * @param[in]  iseed   If negative, re-initializes the uniform RNG.
 * @param[in]  urandom Pointer to a uniform random number generator function.
 * @return A Gaussian random deviate with mean 0 and sigma 1.
 */
double gauss_rn(long iseed, double (*urandom)(long iseed1)) {
  static long valueSaved = 0;
  static double savedValue;
  double urn1, urn2, sine, cosine, factor;

  if (iseed < 0)
    (*urandom)(iseed);
  if (!valueSaved) {
    urn1 = (*urandom)(0);
    urn2 = (*urandom)(0);
    factor = sqrt(-2 * log(urn1));
    cosine = cos(PIx2 * urn2);
    sine = sin(PIx2 * urn2);
    savedValue = factor * cosine;
    /* to use saved values, set this to 1 instead 
         * I've disabled this feature as it doesn't work properly with multiple
         * urandom's.
         */
    valueSaved = 0;
    return factor * sine;
  } else {
    valueSaved = 0;
    return savedValue;
  }
}

/**
 * @brief Generate a Gaussian-distributed random number with specified mean, sigma, and optional cutoff.
 *
 * If limit_in_sigmas > 0, values are regenerated until the deviate falls within ±limit_in_sigmas*sigma.
 *
 * @param[in] mean             Mean of the Gaussian distribution.
 * @param[in] sigma            Standard deviation of the Gaussian distribution.
 * @param[in] limit_in_sigmas  Cutoff in multiples of sigma (if <= 0, no cutoff).
 * @param[in] urandom          Pointer to a uniform random number generator function.
 * @return A Gaussian random deviate meeting the specified conditions.
 */
double gauss_rn_lim(
  double mean, double sigma,
  double limit_in_sigmas, /* if <= 0, ignored. Otherwise, the distribution is
                                 * cut off at +/-limit_in_sigmas*sigma from the mean */
  double (*urandom)(long iseed)) {
  double limit, value;

  if (limit_in_sigmas <= 0)
    return (mean + sigma * gauss_rn(0, urandom));

  limit = limit_in_sigmas;
  do {
    value = gauss_rn(0, urandom);
  } while (FABS(value) > limit);
  return (sigma * value + mean);
}

/**
 * @brief Convert a sequence of uniformly distributed [0,1] values into a Gaussian-distributed sequence.
 *
 * Uses the inverse error function (erf) to transform uniform data into Gaussian distributed data.
 * Values that exceed the given limit are discarded.
 *
 * @param[in,out] data   Array of input values in [0,1] to be converted.
 * @param[in]     points Number of values in the array.
 * @param[in]     limit  Upper cutoff in standard deviations (if <= 0, no cutoff).
 * @return The number of successfully converted data points.
 */
long convertSequenceToGaussianDistribution(double *data, long points, double limit) {
  double u1, u2, z = 0;
  long i, j;

  if (!points)
    return 0;
  if (!data)
    return 0;
  for (i = j = 0; i < points; i++) {
    u1 = 2 * (data[i] - 0.5);
    if (u1 < 0)
      u2 = -u1;
    else
      u2 = u1;
#if defined(vxWorks) || defined(__rtems__)
    fprintf(stderr, "erf function is not implemented on this architecture\n");
    exit(1);
#else
    z = zeroNewton(erf, u2, 0.5, 1e-6, 500, 1e-12);
#endif
    data[j] = z * SQRT2;
    if (limit <= 0 || data[j] < limit) {
      if (u1 < 0)
        data[j] = -data[j];
      j++;
    }
  }
  return j;
}

typedef struct {
  char *buffer;
  double randomValue;
} RANDOMIZATION_HOLDER;

int randomizeOrderCmp(const void *p1, const void *p2) {
  RANDOMIZATION_HOLDER *rh1, *rh2;
  double diff;
  rh1 = (RANDOMIZATION_HOLDER *)p1;
  rh2 = (RANDOMIZATION_HOLDER *)p2;
  if ((diff = rh1->randomValue - rh2->randomValue) > 0)
    return 1;
  if (diff < 0)
    return -1;
  return 0;
}

/**
 * @brief Randomize the order of an array of elements.
 *
 * Shuffles the elements of an array using a provided uniform random generator.
 *
 * @param[in,out] ptr     Pointer to the array to randomize.
 * @param[in]     size    Size of each element in bytes.
 * @param[in]     length  Number of elements in the array.
 * @param[in]     iseed   Seed for initialization if negative.
 * @param[in]     urandom Pointer to a uniform random number generator function.
 * @return Non-zero if successful, zero otherwise.
 */
long randomizeOrder(char *ptr, long size, long length, long iseed, double (*urandom)(long iseed1)) {
  RANDOMIZATION_HOLDER *rh;
  long i;
  if (length < 2)
    return 1;
  if (!ptr)
    return 0;
  if (!(rh = malloc(sizeof(*rh) * length)))
    return 0;
  if (!urandom)
    return 0;
  if (iseed < 0)
    (*urandom)(iseed);
  for (i = 0; i < length; i++) {
    if (!(rh[i].buffer = malloc(size)))
      return 0;
    memcpy(rh[i].buffer, ptr + i * size, size);
    rh[i].randomValue = (*urandom)(0);
  }
  qsort((void *)rh, length, sizeof(*rh), randomizeOrderCmp);

  for (i = 0; i < length; i++) {
    memcpy(ptr + i * size, rh[i].buffer, size);
    free(rh[i].buffer);
  }
  free(rh);
  return 1;
}

/**
 * @brief Generate a uniform random double in [0,1] using a seed and increment, optimized for certain applications.
 *
 * Uses a custom random number generator implemented in Fortran (dlaran_oag).
 *
 * @param[in] iseed     Seed for initialization if negative.
 * @param[in] increment Increment to apply for each call.
 * @return A random double in [0,1].
 */
double random_oag(long iseed, long increment) {
  static short initialized = 0;
  static integer seed[4] = {0, 0, 0, 0};

  if (!initialized || iseed < 0) {
    if (iseed < 0)
      iseed = -iseed;
    seed[3] = ((iseed & 4095) / 2) * 2 + 1;
    seed[2] = (iseed >>= 12) & 4095;
    seed[1] = (iseed >>= 12) & 4095;
    seed[0] = (iseed >>= 12) & 4095;
    initialized = 1;
  }
  if (!initialized) {
    fprintf(stderr, "random_oag not properly initialized\n");
    exit(1);
  }

  return dlaran_oag(seed, increment);
}

/**
 * @brief Generate a Gaussian-distributed random number using the `random_oag` approach.
 *
 * Uses a modified Box–Muller method to generate Gaussian deviates from the `urandom` function provided.
 *
 * @param[in] iseed     Seed for initialization if negative.
 * @param[in] increment Increment step for random number generation.
 * @param[in] urandom   Pointer to an `oag`-style uniform random number generator function.
 * @return A Gaussian random deviate with mean 0 and sigma 1.
 */
double gauss_rn_oag(long iseed, long increment, double (*urandom)(long iseed1, long increment)) {
  double urn1, urn2, sine, factor;

  if (increment < 1)
    increment = 1;
  increment = ((increment - 1) * 2) + 1;
  urn1 = (*urandom)(iseed, increment);
  urn2 = (*urandom)(0, 1);
  factor = sqrt(-2 * log(urn1));
  sine = sin(PIx2 * urn2);
  return factor * sine;
}

/**
 * @brief Generate a Gaussian-distributed random number with mean, sigma, and optional cutoff using `oag` RNG.
 *
 * If limit_in_sigmas > 0, values are regenerated until they fall within the cutoff range.
 *
 * @param[in] mean             Mean of the Gaussian distribution.
 * @param[in] sigma            Standard deviation of the Gaussian distribution.
 * @param[in] limit_in_sigmas  Cutoff in multiples of sigma (if <= 0, no cutoff).
 * @param[in] increment        Increment step for random number generation.
 * @param[in] urandom          Pointer to an `oag`-style uniform random number generator function.
 * @return A Gaussian random deviate meeting the specified conditions.
 */
double gauss_rn_lim_oag(
  double mean, double sigma,
  double limit_in_sigmas, /* if <= 0, ignored. Otherwise, the distribution is
                                 * cut off at +/-limit_in_sigmas*sigma from the mean */
  long increment,
  double (*urandom)(long iseed, long increment)) {
  double limit, value;
  long i;

  if (limit_in_sigmas <= 0)
    return (mean + sigma * gauss_rn_oag(0, increment, urandom));

  limit = limit_in_sigmas;
  i = 0;

  do {
    value = gauss_rn_oag(0, 1, urandom);
    if (FABS(value) <= limit)
      i++;
  } while ((FABS(value) > limit) || (i < increment));

  return (sigma * value + mean);
}
