/* $Id: my_mpz_xor.c,v 1.3 2000/07/28 07:12:07 hubbe Exp $
 *
 * since xor isn't implemented by gmp (for some odd reason)
 */

#include "global.h"

RCSID("$Id: my_mpz_xor.c,v 1.3 2000/07/28 07:12:07 hubbe Exp $");

#include "gmp_machine.h"

#if defined(HAVE_GMP2_GMP_H) && defined(HAVE_LIBGMP2)
#define USE_GMP2
#else /* !HAVE_GMP2_GMP_H || !HAVE_LIBGMP2 */
#if defined(HAVE_GMP_H) && defined(HAVE_LIBGMP)
#define USE_GMP
#endif /* HAVE_GMP_H && HAVE_LIBGMP */
#endif /* HAVE_GMP2_GMP_H && HAVE_LIBGMP2 */

#if defined(USE_GMP) || defined(USE_GMP2)

#include "my_gmp.h"

/* This must be included last! */
#include "module_magic.h"

void my_mpz_xor (mpz_ptr res, mpz_srcptr a, mpz_srcptr b)
{
   /* (a&~b)|(~a&b) for now */

   mpz_t t1;
   mpz_t t2;
   mpz_t not;
   
   mpz_init(t1);
   mpz_init(t2);
   mpz_init(not);
   
   /* t1=(a&~b) */
   mpz_com(not,b);
   mpz_and(t1,a,not);

   /* t2=(~a&b) */
   mpz_com(not,a);
   mpz_and(t2,not,b);

   /* res=t1|t2 */
   mpz_ior(res,t1,t2);

   /* bye bye */
   mpz_clear(t1);
   mpz_clear(t2);
   mpz_clear(not);
}


#endif
