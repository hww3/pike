/*
 *	des - fast & portable DES encryption & decryption.
 *	Copyright (C) 1992  Dana L. How
 *	Please see the file `README' for the complete copyright notice.
 *
 * Slightly edited by Niels M�ller, 1997
 */

#ifndef DES_H_INCLUDED
#define DES_H_INCLUDED

#include "crypto_types.h"

#if 0
#include "RCSID.h"
RCSID(desCore_hRcs, "$Id: des.h,v 1.2 1997/02/12 06:16:26 nisse Exp $");
#endif

#define DES_KEYSIZE 8
#define DES_BLOCKSIZE 8
#define DES_EXPANDED_KEYLEN 32

typedef unsigned INT8 DesData[DES_BLOCKSIZE];
typedef unsigned INT32 DesKeys[DES_EXPANDED_KEYLEN];
typedef void DesFunc(unsigned INT8 *d, unsigned INT32 *r, unsigned INT8 *s);

extern int DesMethod(unsigned INT32 *method, unsigned INT8 *k);
extern void DesQuickInit(void);
extern void DesQuickDone(void);
extern DesFunc DesQuickCoreEncrypt;
extern DesFunc DesQuickFipsEncrypt;
extern DesFunc DesQuickCoreDecrypt;
extern DesFunc DesQuickFipsDecrypt;
extern DesFunc DesSmallCoreEncrypt;
extern DesFunc DesSmallFipsEncrypt;
extern DesFunc DesSmallCoreDecrypt;
extern DesFunc DesSmallFipsDecrypt;

extern DesFunc *DesCryptFuncs[2];
extern int des_key_sched(INT8 *k, INT32 *s);
extern int des_ecb_encrypt(INT8 *s, INT8 *d, INT32 *r, int e);

#endif /*  DES_H_INCLUDED */
