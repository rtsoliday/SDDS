/*
------------------------------------------------------------------------------
By Bob Jenkins, September 1996.
lookupa.h, a hash function for table lookup, same function as lookup.c.
Use this code in any way you wish.  Public Domain.  It has no warranty.
Source is http://burtleburtle.net/bob/c/lookupa.h
------------------------------------------------------------------------------
*/

#ifndef STANDARD
#include "standard.h"
#endif

#ifndef LOOKUPA
#define LOOKUPA

#define CHECKSTATE 8
#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

#ifndef _X86_64_
ub4 lookup(ub1 *k, ub4 length, ub4 level);
#else
ub8 lookup(ub1 *k, ub8 length, ub8 level);
#endif

#endif /* LOOKUPA */
