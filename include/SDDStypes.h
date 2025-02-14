/**
 * @file SDDStypes.h
 * @brief SDDS Data Types Definitions
 *
 * This header file defines the various data types used in the SDDS (Self Describing Data Sets)
 * library. It includes type identifiers, type classification macros, and validation utilities
 * to ensure correct data handling within the SDDS framework.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license This file is distributed under the terms of the Software License Agreement
 *          found in the file LICENSE included with this distribution.
 *
 * @authors
 *  M. Borland,
 *  C. Saunders,
 *  R. Soliday,
 *  H. Shang
 *
 */

#ifndef _SDDSTYPES_
#define _SDDSTYPES_ 1

/**
 * @def SDDS_LONGDOUBLE
 * @brief Identifier for the `long double` data type.
 */
#define SDDS_LONGDOUBLE    1

/**
 * @def SDDS_DOUBLE
 * @brief Identifier for the `double` data type.
 */
#define SDDS_DOUBLE        2

/**
 * @def SDDS_FLOAT
 * @brief Identifier for the `float` data type.
 */
#define SDDS_FLOAT         3

/**
 * @def SDDS_LONG64
 * @brief Identifier for the signed 64-bit integer data type.
 */
#define SDDS_LONG64        4

/**
 * @def SDDS_ULONG64
 * @brief Identifier for the unsigned 64-bit integer data type.
 */
#define SDDS_ULONG64       5

/**
 * @def SDDS_LONG
 * @brief Identifier for the signed 32-bit integer data type.
 */
#define SDDS_LONG          6

/**
 * @def SDDS_ULONG
 * @brief Identifier for the unsigned 32-bit integer data type.
 */
#define SDDS_ULONG         7

/**
 * @def SDDS_SHORT
 * @brief Identifier for the signed short integer data type.
 */
#define SDDS_SHORT         8

/**
 * @def SDDS_USHORT
 * @brief Identifier for the unsigned short integer data type.
 */
#define SDDS_USHORT        9

/**
 * @def SDDS_STRING
 * @brief Identifier for the string data type.
 */
#define SDDS_STRING        10

/**
 * @def SDDS_CHARACTER
 * @brief Identifier for the character data type.
 */
#define SDDS_CHARACTER     11

/**
 * @def SDDS_NUM_TYPES
 * @brief Total number of defined SDDS data types.
 */
#define SDDS_NUM_TYPES     11

/**
 * @def SDDS_INTEGER_TYPE(type)
 * @brief Checks if the given type identifier corresponds to an integer type.
 *
 * This macro evaluates to a non-zero value if the `type` is one of the defined integer types:
 * `SDDS_LONG64`, `SDDS_ULONG64`, `SDDS_LONG`, `SDDS_ULONG`, `SDDS_SHORT`, or `SDDS_USHORT`.
 *
 * @param type The type identifier to check.
 * @return Non-zero if `type` is an integer type, zero otherwise.
 */
#define SDDS_INTEGER_TYPE(type) \
    (((type) == SDDS_LONG64) || ((type) == SDDS_ULONG64) || \
     ((type) == SDDS_LONG) || ((type) == SDDS_ULONG) || \
     ((type) == SDDS_SHORT) || ((type) == SDDS_USHORT))

/**
 * @def SDDS_FLOATING_TYPE(type)
 * @brief Checks if the given type identifier corresponds to a floating-point type.
 *
 * This macro evaluates to a non-zero value if the `type` is one of the defined floating-point types:
 * `SDDS_LONGDOUBLE`, `SDDS_DOUBLE`, or `SDDS_FLOAT`.
 *
 * @param type The type identifier to check.
 * @return Non-zero if `type` is a floating-point type, zero otherwise.
 */
#define SDDS_FLOATING_TYPE(type) \
    (((type) == SDDS_LONGDOUBLE) || ((type) == SDDS_DOUBLE) || \
     ((type) == SDDS_FLOAT))

/**
 * @def SDDS_NUMERIC_TYPE(type)
 * @brief Checks if the given type identifier corresponds to any numeric type.
 *
 * This macro evaluates to a non-zero value if the `type` is either an integer type
 * (`SDDS_INTEGER_TYPE`) or a floating-point type (`SDDS_FLOATING_TYPE`).
 *
 * @param type The type identifier to check.
 * @return Non-zero if `type` is a numeric type, zero otherwise.
 */
#define SDDS_NUMERIC_TYPE(type) (SDDS_INTEGER_TYPE(type) || SDDS_FLOATING_TYPE(type))

/**
 * @def SDDS_VALID_TYPE(type)
 * @brief Validates whether the given type identifier is within the defined range of SDDS types.
 *
 * This macro checks if the `type` is between `1` and `SDDS_NUM_TYPES` inclusive.
 *
 * @param type The type identifier to validate.
 * @return Non-zero if `type` is valid, zero otherwise.
 */
#define SDDS_VALID_TYPE(type) (((type) >= 1) && ((type) <= SDDS_NUM_TYPES))

/**
 * @def SDDS_ANY_NUMERIC_TYPE
 * @brief Special identifier used by SDDS_Check*() routines to accept any numeric type.
 *
 * This value extends beyond the defined types to allow functions to accept any numeric type.
 */
#define SDDS_ANY_NUMERIC_TYPE (SDDS_NUM_TYPES + 1)

/**
 * @def SDDS_ANY_FLOATING_TYPE
 * @brief Special identifier used by SDDS_Check*() routines to accept any floating-point type.
 *
 * This value extends beyond the defined types to allow functions to accept any floating-point type.
 */
#define SDDS_ANY_FLOATING_TYPE (SDDS_NUM_TYPES + 2)

/**
 * @def SDDS_ANY_INTEGER_TYPE
 * @brief Special identifier used by SDDS_Check*() routines to accept any integer type.
 *
 * This value extends beyond the defined types to allow functions to accept any integer type.
 */
#define SDDS_ANY_INTEGER_TYPE (SDDS_NUM_TYPES + 3)

/**
 * @brief Indicates that the SDDS types have been defined.
 *
 * This macro serves as an include guard to prevent multiple inclusions of the `SDDStypes.h` header file.
 */
#endif /* _SDDSTYPES_ */
