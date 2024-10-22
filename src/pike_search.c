/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

/* New memory searcher functions */

#include "global.h"
#include "stuff.h"
#include "mapping.h"
#include "object.h"
#include "pike_memory.h"
#include "stralloc.h"
#include "interpret.h"
#include "pike_error.h"
#include "module_support.h"
#include "pike_macros.h"
#include "pike_search.h"
#include "bignum.h"

#include <assert.h>

ptrdiff_t pike_search_struct_offset;
#define OB2MSEARCH(O) ((struct pike_mem_searcher *)((O)->storage+pike_search_struct_offset))
#define THIS_MSEARCH ((struct pike_mem_searcher *)(Pike_fp->current_storage))

static struct mapping *memsearch_cache;
static struct program *pike_search_program;


static void *nil_search(void *no_data,
			void *haystack,
			ptrdiff_t haystacklen)
{
  return haystack;
}

/* Needed on architectures where struct returns have
 * incompatible calling conventions (sparc v8).
 */
static PCHARP nil_searchN(void *no_data,
			  PCHARP haystack,
			  ptrdiff_t haystacklen)
{
  return haystack;
}

static const struct SearchMojtVtable nil_search_vtable = {
  (SearchMojtFunc0) nil_search,
  (SearchMojtFunc1) nil_search,
  (SearchMojtFunc2) nil_search,
  (SearchMojtFuncN) nil_searchN,
};

/* magic stuff for hubbesearch */
/* NOTE: GENERIC_GET4_CHARS(PTR) must be compatible with
 *       the GET_4_{,UN}ALIGNED_CHARS0() variants!
 */
#if PIKE_BYTEORDER == 4321
#define GENERIC_GET4_CHARS(PTR) \
 ( ((PTR)[0] << 24) + ( (PTR)[1] << 16 ) +( (PTR)[2] << 8 ) +  (PTR)[3] )
#else /* PIKE_BYTEORDER != 4321 */
#define GENERIC_GET4_CHARS(PTR) \
 ( ((PTR)[3] << 24) + ( (PTR)[2] << 16 ) +( (PTR)[1] << 8 ) +  (PTR)[0] )
#endif /* PIKE_BYTEORDER == 4321 */

#define HUBBE_ALIGN0(q) q=(p_wchar0 *)(PTR_TO_INT(q) & ~(sizeof(INT32) - 1))
#define GET_4_ALIGNED_CHARS0(PTR)  (*(INT32 *)(PTR))
#define GET_4_UNALIGNED_CHARS0(PTR)  EXTRACT_INT(PTR)

#define HUBBE_ALIGN1(q) 
#define GET_4_ALIGNED_CHARS1 GENERIC_GET4_CHARS
#define GET_4_UNALIGNED_CHARS1 GENERIC_GET4_CHARS

#define HUBBE_ALIGN2(q) 
#define GET_4_ALIGNED_CHARS2 GENERIC_GET4_CHARS
#define GET_4_UNALIGNED_CHARS2 GENERIC_GET4_CHARS


#define PxC(X,Y) PIKE_CONCAT(X,Y)
#define PxC2(X,Y) PIKE_CONCAT(X,Y)
#define PxC3(X,Y,Z) PIKE_CONCAT3(X,Y,Z)
#define PxC4(X,Y,Z,Q) PIKE_CONCAT4(X,Y,Z,Q)
#define NameN(X) PxC2(X,NSHIFT)
#define NameH(X) PxC2(X,HSHIFT)
#define NameNH(X) PxC3(X,NSHIFT,HSHIFT)

#define BMHASH00(X) (X)
#define BMHASH01(X) ((X)>CHARS ? CHARS : (X))
#define BMHASH02    BMHASH01

#define BMHASH10(X) (((X)*997)&(CHARS-1))
#define BMHASH11 BMHASH10
#define BMHASH12 BMHASH10

#define BMHASH20 BMHASH10
#define BMHASH21 BMHASH20
#define BMHASH22 BMHASH20

#define NCHAR NameN(p_wchar)
#define HCHAR NameH(p_wchar)

#define NEEDLE ((NCHAR *)(s->needle))
#define NEEDLELEN s->needlelen

#define NSHIFT 0
#include "pike_search_engine.c"
#undef NSHIFT

#define NSHIFT 1
#include "pike_search_engine.c"
#undef NSHIFT

#define NSHIFT 2
#include "pike_search_engine.c"
#undef NSHIFT


#define NSHIFT 0



PMOD_EXPORT void pike_init_memsearch(struct pike_mem_searcher *s,
				     PCHARP needle,
				     ptrdiff_t needlelen,
				     ptrdiff_t max_haystacklen)
{
  switch(needle.shift)
  {
    case 0:
      init_memsearch0(s,(p_wchar0*)needle.ptr, needlelen, max_haystacklen);
      return;
    case 1:
      init_memsearch1(s,(p_wchar1*)needle.ptr, needlelen, max_haystacklen);
      return;
    case 2:
      init_memsearch2(s,(p_wchar2*)needle.ptr, needlelen, max_haystacklen);
      return;
  }
#ifdef PIKE_DEBUG
  Pike_fatal("Illegal shift\n");
#endif
}

PMOD_EXPORT SearchMojt compile_memsearcher(PCHARP needle,
					   ptrdiff_t needlelen,
					   int max_haystacklen,
					   struct pike_string *hashkey)
{
  switch(needle.shift)
  {
    default:
#ifdef PIKE_DEBUG
      Pike_fatal("Illegal shift\n");
    case 0:
#endif
      return compile_memsearcher0((p_wchar0*)needle.ptr, needlelen, max_haystacklen,hashkey);
    case 1:
      return compile_memsearcher1((p_wchar1*)needle.ptr, needlelen, max_haystacklen,hashkey);
    case 2:
      return compile_memsearcher2((p_wchar2*)needle.ptr, needlelen, max_haystacklen,hashkey);
  }
  /* NOTREACHED */
}

PMOD_EXPORT SearchMojt simple_compile_memsearcher(struct pike_string *str)
{
  return compile_memsearcher(MKPCHARP_STR(str),
			     str->len,
			     0x7fffffff,
			     str);
}

/* This function is thread safe, most other functions in this
 * file are not thread safe.
 */
PMOD_EXPORT char *my_memmem(char *needle,
			    size_t needlelen,
			    char *haystack,
			    size_t haystacklen)
{
  struct pike_mem_searcher tmp;
  init_memsearch0(&tmp, (p_wchar0 *) needle, (ptrdiff_t)needlelen,
		  (ptrdiff_t)haystacklen);
  return (char *) tmp.mojt.vtab->func0(tmp.mojt.data, (p_wchar0 *) haystack,
				       (ptrdiff_t)haystacklen);
  /* No free required - Hubbe */
}

void init_pike_searching(void)
{
  start_new_program();
  pike_search_struct_offset=ADD_STORAGE(struct pike_mem_searcher);
  MAP_VARIABLE("__s", tStr, 0,
	       pike_search_struct_offset + OFFSETOF(pike_mem_searcher,s),
	       PIKE_T_STRING);
  pike_search_program=end_program();
  add_program_constant("Search",pike_search_program,ID_PROTECTED);

  memsearch_cache=allocate_mapping(10);
  memsearch_cache->data->flags |= MAPPING_FLAG_WEAK;
}

void exit_pike_searching(void)
{
  if(pike_search_program)
  {
    free_program(pike_search_program);
    pike_search_program=0;
  }
  if(memsearch_cache)
  {
    free_mapping(memsearch_cache);
    memsearch_cache=0;
  }
}
