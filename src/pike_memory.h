/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/

/*
 * $Id: pike_memory.h,v 1.26 2000/12/01 20:11:19 grubba Exp $
 */
#ifndef MEMORY_H
#define MEMORY_H

#include "global.h"
#include "stralloc.h"



#define MEMSEARCH_LINKS 512

struct link
{
  struct link *next;
  INT32 key;
  ptrdiff_t offset;
};

enum methods {
  no_search,
  use_memchr,
  memchr_and_memcmp,
  hubbe_search,
  boyer_moore
};

struct mem_searcher
{
  enum methods method;
  char *needle;
  size_t needlelen;
  size_t hsize, max;
  struct link links[MEMSEARCH_LINKS];
  struct link *set[MEMSEARCH_LINKS];
};


#if 1
/* use new searching stuff */

#include "pike_search.h"

#else
struct generic_mem_searcher
{
  char needle_shift;
  char haystack_shift;
  union data_u
  {
    struct mem_searcher eightbit;
    struct other_search_s
    {
      enum methods method;
      void *needle;
      size_t needlelen;
      int first_char;
    } other;
  } data;
};

#endif

#include "block_alloc_h.h"
#define MEMCHR0 MEMCHR

/* Note to self: Prototypes must be updated manually /Hubbe */
PMOD_EXPORT ptrdiff_t pcharp_memcmp(PCHARP a, PCHARP b, int sz);
PMOD_EXPORT long pcharp_strlen(PCHARP a);
PMOD_EXPORT INLINE p_wchar1 *MEMCHR1(p_wchar1 *p, p_wchar2 c, ptrdiff_t e);
PMOD_EXPORT INLINE p_wchar2 *MEMCHR2(p_wchar2 *p, p_wchar2 c, ptrdiff_t e);
PMOD_EXPORT void swap(char *a, char *b, size_t size);
PMOD_EXPORT void reverse(char *memory, size_t nitems, size_t size);
PMOD_EXPORT void reorder(char *memory, INT32 nitems, INT32 size,INT32 *order);
PMOD_EXPORT size_t hashmem(const unsigned char *a, size_t len, size_t mlen);
PMOD_EXPORT size_t hashstr(const unsigned char *str, ptrdiff_t maxn);
PMOD_EXPORT size_t simple_hashmem(const unsigned char *str, ptrdiff_t len, ptrdiff_t maxn);
PMOD_EXPORT void init_memsearch(struct mem_searcher *s,
		    char *needle,
		    size_t needlelen,
		    size_t max_haystacklen);
PMOD_EXPORT char *memory_search(struct mem_searcher *s,
		    char *haystack,
		    size_t haystacklen);
PMOD_EXPORT void init_generic_memsearcher(struct generic_mem_searcher *s,
			      void *needle,
			      size_t needlelen,
			      char needle_shift,
			      size_t estimated_haystack,
			      char haystack_shift);
PMOD_EXPORT void *generic_memory_search(struct generic_mem_searcher *s,
			    void *haystack,
			    size_t haystacklen,
			    char haystack_shift);
PMOD_EXPORT char *my_memmem(char *needle,
		size_t needlelen,
		char *haystack,
		size_t haystacklen);
PMOD_EXPORT void memfill(char *to,
	     INT32 tolen,
	     char *from,
	     INT32 fromlen,
	     INT32 offset);
PMOD_EXPORT char *debug_xalloc(size_t size);

#undef BLOCK_ALLOC

#ifdef HANDLES_UNALIGNED_MEMORY_ACCESS
#define DO_IF_ELSE_UNALIGNED_MEMORY_ACCESS(IF, ELSE)	IF
#else /* !HANDLES_UNALIGNED_MEMORY_ACCESS */
#define DO_IF_ELSE_UNALIGNED_MEMORY_ACCESS(IF, ELSE)	ELSE
#endif /* HANDLES_UNALIGNED_MEMORY_ACCESS */

#if SIZEOF_CHAR_P == 4
#define DIVIDE_BY_2_CHAR_P(X)	(X >>= 3)
#else /* sizeof(char *) != 4 */
#if SIZEOF_CHAR_P == 8
#define DIVIDE_BY_2_CHAR_P(X)	(X >>= 4)
#else /* sizeof(char *) != 8 */
#define DIVIDE_BY_2_CHAR_P(X)	(X /= 2*sizeof(size_t))
#endif /* sizeof(char *) == 8 */
#endif /* sizeof(char *) == 4 */

#define DO_HASHMEM(A, LEN, MLEN)			\
  do {							\
    const unsigned char *a = A;				\
    size_t len = LEN;					\
    size_t mlen = MLEN;					\
    size_t ret;						\
  							\
    ret = 9248339*len;					\
    if(len<mlen)					\
      mlen=len;						\
    else						\
    {							\
      switch(len-mlen)					\
      {							\
  	default: ret^=(ret<<6) + a[len-7];		\
  	case 7:						\
  	case 6: ret^=(ret<<7) + a[len-5];		\
  	case 5:						\
  	case 4: ret^=(ret<<4) + a[len-4];		\
  	case 3: ret^=(ret<<3) + a[len-3];		\
  	case 2: ret^=(ret<<3) + a[len-2];		\
  	case 1: ret^=(ret<<3) + a[len-1];		\
      }							\
    }							\
    a += mlen & 7;					\
    switch(mlen&7)					\
    {							\
      case 7: ret^=a[-7];				\
      case 6: ret^=(ret<<4)+a[-6];			\
      case 5: ret^=(ret<<7)+a[-5];			\
      case 4: ret^=(ret<<6)+a[-4];			\
      case 3: ret^=(ret<<3)+a[-3];			\
      case 2: ret^=(ret<<7)+a[-2];			\
      case 1: ret^=(ret<<5)+a[-1];			\
    }							\
  							\
    DO_IF_ELSE_UNALIGNED_MEMORY_ACCESS(			\
      {							\
  	size_t *b;					\
  	b=(size_t *)a;					\
    							\
  	for(DIVIDE_BY_2_CHAR_P(mlen);mlen--;)		\
  	{						\
  	  ret^=(ret<<7)+*(b++);				\
  	  ret^=(ret>>6)+*(b++);				\
  	}						\
      }							\
    ,							\
      for(mlen >>= 3; mlen--;)				\
      {							\
  	register size_t t1,t2;				\
  	t1= a[0];					\
  	t2= a[1];					\
  	t1=(t1<<5) + a[2];				\
  	t2=(t2<<4) + a[3];				\
  	t1=(t1<<7) + a[4];				\
  	t2=(t2<<5) + a[5];				\
  	t1=(t1<<3) + a[6];				\
  	t2=(t2<<4) + a[7];				\
  	a += 8;						\
  	ret^=(ret<<7) + (ret>>6) + t1 + (t2<<6);	\
      }							\
    )							\
  							\
    return ret;						\
  } while(0)

#endif
