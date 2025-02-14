/** 
 * @file pointInsideContour.c
 * @brief Provides functions for determining the relative position of points 
 *         with respect to a polygonal contour, including whether a point lies inside it.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, R. Soliday
 */
/* This code found on multiple internet sources, translated to C */
#include "mdb.h"

double pointIsLeftOfLine(int64_t i1, int64_t i2, /* indices of vertex points to test */
                         double *x, double *y,   /* (x, y) of vertices */
                         double x0, double y0    /* (x, y) of point to test */
) {
  double result;
  result = (x[i2] - x[i1]) * (y0 - y[i1]) - (x0 - x[i1]) * (y[i2] - y[i1]);
#ifdef DEBUG
  printf("pointIsLeftOfLine(%ld, %ld, %le, %le) = %le\n",
         i1, i2, x0, y0, result);
  fflush(stdout);
#endif

  return result;
}

/**
 * @brief Determine if a given point (x0, y0) is inside a specified polygonal contour.
 *
 * This function uses the winding number method to check if the point (x0, y0)
 * is inside the polygon defined by arrays x and y. If a rotation is specified 
 * (by a non-zero theta and a center), the point is rotated about the given center 
 * before the inside test is performed.
 *
 * @param[in] x0    The x-coordinate of the point to test.
 * @param[in] y0    The y-coordinate of the point to test.
 * @param[in] x     Array of x-coordinates of the polygon vertices.
 * @param[in] y     Array of y-coordinates of the polygon vertices.
 * @param[in] n     The number of vertices in the polygon.
 * @param[in] center Pointer to an array representing the (x,y) center of rotation.
 * @param[in] theta  The clockwise rotation angle (in radians) to apply to the polygon.
 * @return Non-zero if the point is inside the polygon, zero otherwise.
 */
int pointIsInsideContour(
  double x0, double y0, /* point to check */
  double *x, double *y, /* points on contour */
  int64_t n,            /* points in contour */
  double *center,       /* center of rotation of contour */
  double theta          /* clockwise angle to rotation of contour */
) {
  int64_t winding_number = 0;
  int64_t i1, i2;

  if (theta != 0 && center) {
    double x1, y1, ct, st;
    /* rotate (x0, y0) by theta about center */
    x0 -= center[0];
    y0 -= center[1];
    ct = cos(theta);
    st = sin(theta);
    x1 = x0 * ct - y0 * st + center[0];
    y1 = x0 * st + y0 * ct + center[1];
    x0 = x1;
    y0 = y1;
  }

  for (i1 = 0; i1 < n; i1++) {
    if (i1 == (n - 1))
      /* wrap */
      i2 = 0;
    else
      i2 = i1 + 1;

    if (y[i1] <= y0) {
      /* start y <= y0 */
      if (y[i2] > y0) {
        /* upward crossing */
#ifdef DEBUG
        printf("upward crossing\n");
        fflush(stdout);
#endif
        if (pointIsLeftOfLine(i1, i2, x, y, x0, y0) > 0) {
          /* Point left of edge */
          ++winding_number;
        }
      }
    } else {
      /* start y > y0 */
      if (y[i2] <= y0) {
        /* downward crossing */
#ifdef DEBUG
        printf("downward crossing\n");
        fflush(stdout);
#endif
        if (pointIsLeftOfLine(i1, i2, x, y, x0, y0) < 0) {
          /* Point right of edge */
          --winding_number;
        }
      }
    }
#ifdef DEBUG
    printf("i1 = %" PRId64 ", winding_number = %" PRId64 "\n", i1, winding_number);
    fflush(stdout);
#endif
  }

#ifdef DEBUG
  printf("winding_number = %" PRId64 "\n", winding_number);
  fflush(stdout);
#endif

  return (winding_number != 0);
}

