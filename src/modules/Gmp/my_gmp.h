/* my_gmp.h
 *
 * These functions or something similar will hopefully be included
 * with Gmp-2.1 .
 */

#ifndef MY_GMP_H_INCLUDED
#define MY_GMP_H_INCLUDED

unsigned long mpz_small_factor(mpz_t n, int limit);

void mpz_next_prime(mpz_t p, mpz_t n, int count, int prime_limit);

#endif MY_GMP_H_INCLUDED
