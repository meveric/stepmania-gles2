/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtomcrypt.com
 */
#include "tomcrypt.h"

/** 
  @file pkcs_1_os2ip.c
  Octet to Integer OS2IP, Tom St Denis 
*/
#ifdef PKCS_1

/**
  Read a binary string into an mp_int
  @param n          [out] The mp_int destination
  @param in         The binary string to read
  @param inlen      The length of the binary string
  @return CRYPT_OK if successful
*/
int pkcs_1_os2ip(void *n, unsigned char *in, unsigned long inlen)
{
   return mp_read_unsigned_bin(n, in, inlen);
}

#endif /* PKCS_1 */


/* $Source$ */
/* $Revision: 24838 $ */
/* $Date: 2007-01-23 23:16:57 -0600 (Tue, 23 Jan 2007) $ */
