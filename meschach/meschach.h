/* This include file has all the Meschach include files
 * you'll ever need.
 */
#ifndef MESCHACH
#ifdef MATLIB 
#error "The MATLIB header is already included by this file, which just included meschach.h.  MATLIB and Meschach use common function names!"
#endif

#include "iter.h"
#include "matrix2.h"
#include "zmatrix.h"
#include "zmatrix2.h"
#include "sparse2.h"
#define MESCHACH 1
#endif
