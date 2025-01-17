/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

#ifndef PIKE_SEARCH_H
#define PIKE_SEARCH_H

#define MEMSEARCH_LINKS 512
#define BMLEN 768
#define CHARS 256
#define TUNAFISH

struct hubbe_search_link
{
  struct hubbe_search_link *next;
  ptrdiff_t offset;
  INT32 key;
};

struct hubbe_searcher
{
  void *needle;
  ptrdiff_t needlelen;

  size_t hsize, max;
  struct hubbe_search_link links[MEMSEARCH_LINKS];
  struct hubbe_search_link *set[MEMSEARCH_LINKS];
};

struct boyer_moore_hubbe_searcher
{
  void *needle;
  ptrdiff_t needlelen;

  ptrdiff_t plen;
  ptrdiff_t d1[CHARS+1];
  ptrdiff_t d2[BMLEN];
};

struct SearchMojtS;

#define FNORD(N,C) \
typedef C (* PIKE_CONCAT(SearchMojtFunc,N) )(void*, C, size_t)


FNORD(0,p_wchar0 *);
FNORD(1,p_wchar1 *);
FNORD(2,p_wchar2 *);
FNORD(N,PCHARP);

struct SearchMojtVtable
{
  SearchMojtFunc0 func0;
  SearchMojtFunc1 func1;
  SearchMojtFunc2 func2;
  SearchMojtFuncN funcN;
};

typedef struct SearchMojt
{
  const struct SearchMojtVtable *vtab;
  void *data;
  struct object *container;	/* Refcounted if non-NULL. */
} SearchMojt;

struct pike_mem_searcher
{
  SearchMojt mojt; /* Exception: mojt.container not refcounted here. */
  struct pike_string *s; /* search string */
  union memsearcher_data
  {
    struct hubbe_searcher hubbe;
    struct boyer_moore_hubbe_searcher bm;
  } data;
};


/* Prototypes begin here */
PMOD_EXPORT void pike_init_memsearch(struct pike_mem_searcher *s,
				     PCHARP needle,
				     ptrdiff_t needlelen,
				     ptrdiff_t max_haystacklen);
PMOD_EXPORT SearchMojt compile_memsearcher(PCHARP needle,
			       ptrdiff_t needlelen,
			       int max_haystacklen,
			       struct pike_string *hashkey);
PMOD_EXPORT SearchMojt simple_compile_memsearcher(struct pike_string *str);
PMOD_EXPORT char *my_memmem(char *needle,
			    size_t needlelen,
			    char *haystack,
			    size_t haystacklen);
void init_pike_searching(void);
void exit_pike_searching(void);
/* Prototypes end here */

#endif
