/*
 * $Id: md4.h,v 1.1 2002/02/26 16:54:18 marcus Exp $
 */

#include "crypto_types.h"

#define MD4_DATASIZE    64
#define MD4_DATALEN     16
#define MD4_DIGESTSIZE  16
#define MD4_DIGESTLEN    4

struct md4_ctx {
  unsigned INT32 digest[MD4_DIGESTLEN]; /* Digest */
  unsigned INT32 count_l, count_h;      /* Block count */
  unsigned INT8 block[MD4_DATASIZE];   /* One block buffer */
  int index;                            /* index into buffer */
};

void md4_init(struct md4_ctx *ctx);
void md4_update(struct md4_ctx *ctx, unsigned INT8 *buffer, unsigned INT32 len);
void md4_final(struct md4_ctx *ctx);
void md4_digest(struct md4_ctx *ctx, INT8 *s);
void md4_copy(struct md4_ctx *dest, struct md4_ctx *src);
