/*
  --------------------------------------------------------------------
  lookupa.c, by Bob Jenkins, December 1996. Public Domain.  No warranty.
  Source is http://burtleburtle.net/bob/c/lookupa.c
  Customized for elegant for both 32 and 64 bits machines by Yusong Wang,
  May, 2007.
  --------------------------------------------------------------------
*/
#ifndef STANDARD
#  include "standard.h"
#endif
#ifndef LOOKUPA
#  include "lookupa.h"
#endif

#ifndef _X86_64_
/*
  --------------------------------------------------------------------
  mix -- mix 3 32-bit values reversibly.
  For every delta with one or two bit set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
  * If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
  * If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
  mix() was built out of 36 single-cycle latency instructions in a 
  structure that could supported 2x parallelism, like so:
  a -= b; 
  a -= c; x = (c>>13);
  b -= c; a ^= x;
  b -= a; x = (a<<8);
  c -= a; b ^= x;
  c -= b; x = (b>>13);
  ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage 
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
  --------------------------------------------------------------------
*/
#  define mix(a, b, c) \
    {                  \
      a -= b;          \
      a -= c;          \
      a ^= (c >> 13);  \
      b -= c;          \
      b -= a;          \
      b ^= (a << 8);   \
      c -= a;          \
      c -= b;          \
      c ^= (b >> 13);  \
      a -= b;          \
      a -= c;          \
      a ^= (c >> 12);  \
      b -= c;          \
      b -= a;          \
      b ^= (a << 16);  \
      c -= a;          \
      c -= b;          \
      c ^= (b >> 5);   \
      a -= b;          \
      a -= c;          \
      a ^= (c >> 3);   \
      b -= c;          \
      b -= a;          \
      b ^= (a << 10);  \
      c -= a;          \
      c -= b;          \
      c ^= (b >> 15);  \
    }

/*
  --------------------------------------------------------------------
  lookup() -- hash a variable-length key into a 32-bit value
  k     : the key (the unaligned variable-length array of bytes)
  len   : the length of the key, counting by bytes
  level : can be any 4-byte value
  Returns a 32-bit value.  Every bit of the key affects every bit of
  the return value.  Every 1-bit and 2-bit delta achieves avalanche.
  About 6len+35 instructions.

  The best hash table sizes are powers of 2.  There is no need to do
  mod a prime (mod is sooo slow!).  If you need less than 32 bits,
  use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
  In which case, the hash table should have hashsize(10) elements.

  If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = lookup( k[i], len[i], h);

  By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
  code any way you wish, private, educational, or commercial.

  See http://burtleburtle.net/bob/hash/evahash.html
  Use for hash table lookup, or anything where one collision in 2^32 is
  acceptable.  Do NOT use for cryptographic purposes.
  --------------------------------------------------------------------
*/

ub4 lookup(k, length, level) register ub1 *k; /* the key */
register ub4 length;                          /* the length of the key */
register ub4 level;                           /* the previous hash, or an arbitrary value */
{
  register ub4 a, b, c, len;

  /* Set up the internal state */
  len = length;
  a = b = 0x9e3779b9; /* the golden ratio; an arbitrary value */
  c = level;          /* the previous hash value */

  /*---------------------------------------- handle most of the key */
  while (len >= 12) {
    a += (k[0] + ((ub4)k[1] << 8) + ((ub4)k[2] << 16) + ((ub4)k[3] << 24));
    b += (k[4] + ((ub4)k[5] << 8) + ((ub4)k[6] << 16) + ((ub4)k[7] << 24));
    c += (k[8] + ((ub4)k[9] << 8) + ((ub4)k[10] << 16) + ((ub4)k[11] << 24));
    mix(a, b, c);
    k += 12;
    len -= 12;
  }

  /*------------------------------------- handle the last 11 bytes */
  c += length;
  switch (len) /* all the case statements fall through */
  {
  case 11:
    c += ((ub4)k[10] << 24);
  case 10:
    c += ((ub4)k[9] << 16);
  case 9:
    c += ((ub4)k[8] << 8);
    /* the first byte of c is reserved for the length */
  case 8:
    b += ((ub4)k[7] << 24);
  case 7:
    b += ((ub4)k[6] << 16);
  case 6:
    b += ((ub4)k[5] << 8);
  case 5:
    b += k[4];
  case 4:
    a += ((ub4)k[3] << 24);
  case 3:
    a += ((ub4)k[2] << 16);
  case 2:
    a += ((ub4)k[1] << 8);
  case 1:
    a += k[0];
    /* case 0: nothing left to add */
  }
  mix(a, b, c);
  /*-------------------------------------------- report the result */
  return c;
}

#else

/*
  --------------------------------------------------------------------
  mix -- mix 3 64-bit values reversibly.
  mix() takes 48 machine instructions, but only 24 cycles on a superscalar
  machine (like Intel's new MMX architecture).  It requires 4 64-bit
  registers for 4::2 parallelism.
  All 1-bit deltas, all 2-bit deltas, all deltas composed of top bits of
  (a,b,c), and all deltas of bottom bits were tested.  All deltas were
  tested both on random keys and on keys that were nearly all zero.
  These deltas all cause every bit of c to change between 1/3 and 2/3
  of the time (well, only 113/400 to 287/400 of the time for some
  2-bit delta).  These deltas all cause at least 80 bits to change
  among (a,b,c) when the mix is run either forward or backward (yes it
  is reversible).
  This implies that a hash using mix64 has no funnels.  There may be
  characteristics with 3-bit deltas or bigger, I didn't test for
  those.
  --------------------------------------------------------------------
*/
#  define mix64(a, b, c) \
    {                    \
      a -= b;            \
      a -= c;            \
      a ^= (c >> 43);    \
      b -= c;            \
      b -= a;            \
      b ^= (a << 9);     \
      c -= a;            \
      c -= b;            \
      c ^= (b >> 8);     \
      a -= b;            \
      a -= c;            \
      a ^= (c >> 38);    \
      b -= c;            \
      b -= a;            \
      b ^= (a << 23);    \
      c -= a;            \
      c -= b;            \
      c ^= (b >> 5);     \
      a -= b;            \
      a -= c;            \
      a ^= (c >> 35);    \
      b -= c;            \
      b -= a;            \
      b ^= (a << 49);    \
      c -= a;            \
      c -= b;            \
      c ^= (b >> 11);    \
      a -= b;            \
      a -= c;            \
      a ^= (c >> 12);    \
      b -= c;            \
      b -= a;            \
      b ^= (a << 18);    \
      c -= a;            \
      c -= b;            \
      c ^= (b >> 22);    \
    }

/*
  --------------------------------------------------------------------
  lookup() -- hash a variable-length key into a 64-bit value
  k     : the key (the unaligned variable-length array of bytes)
  len   : the length of the key, counting by bytes
  level : can be any 8-byte value
  Returns a 64-bit value.  Every bit of the key affects every bit of
  the return value.  No funnels.  Every 1-bit and 2-bit delta achieves
  avalanche.  About 41+5len instructions.

  The best hash table sizes are powers of 2.  There is no need to do
  mod a prime (mod is sooo slow!).  If you need less than 64 bits,
  use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
  In which case, the hash table should have hashsize(10) elements.

  If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = lookup( k[i], len[i], h);

  By Bob Jenkins, Jan 4 1997.  bob_jenkins@burtleburtle.net.  You may
  use this code any way you wish, private, educational, or commercial,
  but I would appreciate if you give me credit.

  See http://burtleburtle.net/bob/hash/evahash.html
  Use for hash table lookup, or anything where one collision in 2^^64
  is acceptable.  Do NOT use for cryptographic purposes.
  --------------------------------------------------------------------
*/

ub8 lookup(k, length, level) register ub1 *k; /* the key */
register ub8 length;                          /* the length of the key */
register ub8 level;                           /* the previous hash, or an arbitrary value */
{
  register ub8 a, b, c, len;

  /* Set up the internal state */
  len = length;
  a = b = level;            /* the previous hash value */
  c = 0x9e3779b97f4a7c13LL; /* the golden ratio; an arbitrary value */

  /*---------------------------------------- handle most of the key */
  while (len >= 24) {
    a += (k[0] + ((ub8)k[1] << 8) + ((ub8)k[2] << 16) + ((ub8)k[3] << 24) + ((ub8)k[4] << 32) + ((ub8)k[5] << 40) + ((ub8)k[6] << 48) + ((ub8)k[7] << 56));
    b += (k[8] + ((ub8)k[9] << 8) + ((ub8)k[10] << 16) + ((ub8)k[11] << 24) + ((ub8)k[12] << 32) + ((ub8)k[13] << 40) + ((ub8)k[14] << 48) + ((ub8)k[15] << 56));
    c += (k[16] + ((ub8)k[17] << 8) + ((ub8)k[18] << 16) + ((ub8)k[19] << 24) + ((ub8)k[20] << 32) + ((ub8)k[21] << 40) + ((ub8)k[22] << 48) + ((ub8)k[23] << 56));
    mix64(a, b, c);
    k += 24;
    len -= 24;
  }

  /*------------------------------------- handle the last 23 bytes */
  c += length;
  switch (len) /* all the case statements fall through */
  {
  case 23:
    c += ((ub8)k[22] << 56);
  case 22:
    c += ((ub8)k[21] << 48);
  case 21:
    c += ((ub8)k[20] << 40);
  case 20:
    c += ((ub8)k[19] << 32);
  case 19:
    c += ((ub8)k[18] << 24);
  case 18:
    c += ((ub8)k[17] << 16);
  case 17:
    c += ((ub8)k[16] << 8);
    /* the first byte of c is reserved for the length */
  case 16:
    b += ((ub8)k[15] << 56);
  case 15:
    b += ((ub8)k[14] << 48);
  case 14:
    b += ((ub8)k[13] << 40);
  case 13:
    b += ((ub8)k[12] << 32);
  case 12:
    b += ((ub8)k[11] << 24);
  case 11:
    b += ((ub8)k[10] << 16);
  case 10:
    b += ((ub8)k[9] << 8);
  case 9:
    b += ((ub8)k[8]);
  case 8:
    a += ((ub8)k[7] << 56);
  case 7:
    a += ((ub8)k[6] << 48);
  case 6:
    a += ((ub8)k[5] << 40);
  case 5:
    a += ((ub8)k[4] << 32);
  case 4:
    a += ((ub8)k[3] << 24);
  case 3:
    a += ((ub8)k[2] << 16);
  case 2:
    a += ((ub8)k[1] << 8);
  case 1:
    a += ((ub8)k[0]);
    /* case 0: nothing left to add */
  }
  mix64(a, b, c);
  /*-------------------------------------------- report the result */
  return c;
}
#endif
