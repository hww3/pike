/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: block_alloc.h,v 1.87 2008/05/24 12:18:50 mast Exp $
*/

#undef PRE_INIT_BLOCK
#undef DO_PRE_INIT_BLOCK
#undef INIT_BLOCK
#undef EXIT_BLOCK
#undef BLOCK_ALLOC
#undef LOW_PTR_HASH_ALLOC
#undef PTR_HASH_ALLOC_FIXED
#undef PTR_HASH_ALLOC
#undef COUNT_BLOCK
#undef COUNT_OTHER
#undef DMALLOC_DESCRIBE_BLOCK
#undef BLOCK_ALLOC_HSIZE_SHIFT
#undef MAX_EMPTY_BLOCKS
#undef BLOCK_ALLOC_FILL_PAGES
#undef PTR_HASH_ALLOC_FILL_PAGES
#undef PTR_HASH_ALLOC_FIXED_FILL_PAGES

/* Define this to keep freed blocks around in a backlog, which can
 * help locating leftover pointers to other blocks. It can also hide
 * bugs since the blocks remain intact a while after they are freed.
 * Valgrind will immediately detect attempts to use blocks on the
 * backlog list, though. Only available with dmalloc debug. */
/* #define DMALLOC_BLOCK_BACKLOG */

/* Note: The block_alloc mutex is held while PRE_INIT_BLOCK runs. */
#define PRE_INIT_BLOCK(X)
#define INIT_BLOCK(X)
#define EXIT_BLOCK(X)
#define COUNT_BLOCK(X)
#define COUNT_OTHER()
#define DMALLOC_DESCRIBE_BLOCK(X)
#define BLOCK_ALLOC_HSIZE_SHIFT 2
#define MAX_EMPTY_BLOCKS 4	/* Must be >= 1. */

#ifndef BLOCK_ALLOC_USED
#define BLOCK_ALLOC_USED DO_IF_DMALLOC(real_used) DO_IF_NOT_DMALLOC(used)
#endif

#if defined (DMALLOC_BLOCK_BACKLOG) && defined (DEBUG_MALLOC)
#define DO_IF_BLOCK_BACKLOG(X) X
#define DO_IF_NOT_BLOCK_BACKLOG(X)
#else
#define DO_IF_BLOCK_BACKLOG(X)
#define DO_IF_NOT_BLOCK_BACKLOG(X) X
#endif

/* Invalidate the block as far as possible if running with dmalloc.
 */
#define DO_PRE_INIT_BLOCK(X)	do {				\
    DO_IF_DMALLOC(MEMSET((X), 0x55, sizeof(*(X))));		\
    PRE_INIT_BLOCK(X);						\
  } while (0)

#ifndef PIKE_HASH_T
#define PIKE_HASH_T	size_t
#endif /* !PIKE_HASH_T */

#ifdef PIKE_RUN_UNLOCKED
#include "threads.h"

/* Block Alloc UnLocked */
#define BA_UL(X) PIKE_CONCAT(X,_unlocked)
#define BA_STATIC static
#define BA_INLINE INLINE
#else
#define BA_UL(X) X
#define BA_STATIC
#define BA_INLINE
#endif

#define BLOCK_ALLOC_FILL_PAGES(DATA, PAGES)				\
  BLOCK_ALLOC(DATA,							\
              ((PIKE_MALLOC_PAGE_SIZE * (PAGES))			\
               - PIKE_MALLOC_OVERHEAD - BLOCK_HEADER_SIZE) /		\
              sizeof (struct DATA))

#define PTR_HASH_ALLOC_FILL_PAGES(DATA, PAGES)				\
  PTR_HASH_ALLOC(DATA,							\
                 ((PIKE_MALLOC_PAGE_SIZE * (PAGES))			\
                  - PIKE_MALLOC_OVERHEAD - BLOCK_HEADER_SIZE) /		\
                 sizeof (struct DATA))

#define PTR_HASH_ALLOC_FIXED_FILL_PAGES(DATA, PAGES)			\
  PTR_HASH_ALLOC_FIXED(DATA,						\
                       ((PIKE_MALLOC_PAGE_SIZE * (PAGES))		\
                        - PIKE_MALLOC_OVERHEAD - BLOCK_HEADER_SIZE) /	\
                       sizeof (struct DATA))

/* Size of the members in the block struct below that don't contain
 * the payload data (i.e. that aren't x). This can be used in BSIZE to
 * make the block fit within a page. */
#ifndef BLOCK_HEADER_SIZE
#define BLOCK_HEADER_SIZE (3 * sizeof (void *) + sizeof (INT32))
#endif

#define BLOCK_ALLOC(DATA,BSIZE)						\
									\
struct PIKE_CONCAT(DATA,_block)						\
{									\
  struct PIKE_CONCAT(DATA,_block) *next;				\
  struct PIKE_CONCAT(DATA,_block) *prev;				\
  struct DATA *PIKE_CONCAT3(free_,DATA,s);				\
  INT32 used;								\
  DO_IF_DMALLOC(INT32 real_used;)					\
  struct DATA x[BSIZE];							\
};									\
struct PIKE_CONCAT(DATA,_context)					\
{									\
  struct PIKE_CONCAT(DATA,_context) *next;				\
  struct PIKE_CONCAT(DATA, _block) *blocks, *free_blocks;		\
  INT32 num_empty_blocks;						\
};									\
									\
static struct PIKE_CONCAT(DATA,_context) *PIKE_CONCAT(DATA,_ctxs)=0;	\
									\
/* Points to a double linked list of the meta-blocks. */		\
/* Full meta-blocks are last on this list. */				\
static struct PIKE_CONCAT(DATA,_block) *PIKE_CONCAT(DATA,_blocks)=0;	\
									\
/* Points to the last meta-block in the DATA,_block list that isn't full. */ \
/* -1 when the block alloc isn't initialized. */			\
static struct PIKE_CONCAT(DATA,_block) *PIKE_CONCAT(DATA,_free_blocks)=	\
  (void*)-1;								\
									\
static INT32 PIKE_CONCAT3(num_empty_,DATA,_blocks)=0;			\
DO_IF_RUN_UNLOCKED(static PIKE_MUTEX_T PIKE_CONCAT(DATA,_mutex);)       \
DO_IF_BLOCK_BACKLOG (							\
  static struct DATA *PIKE_CONCAT(DATA,s_to_free)[4 * (BSIZE)];		\
  static size_t PIKE_CONCAT(DATA,s_to_free_ptr) = 0;			\
)									\
									\
void PIKE_CONCAT3(new_,DATA,_context)(void)				\
{									\
  struct PIKE_CONCAT(DATA, _context) *ctx =				\
    (struct PIKE_CONCAT(DATA, _context) *)				\
    malloc(sizeof(struct PIKE_CONCAT(DATA, _context)));			\
  if (!ctx) {								\
    fprintf(stderr, "Fatal: out of memory.\n");				\
    exit(17);								\
  }									\
  ctx->next = PIKE_CONCAT(DATA, _ctxs);					\
  PIKE_CONCAT(DATA, _ctxs) = ctx;					\
  ctx->blocks = PIKE_CONCAT(DATA,_blocks);				\
  ctx->free_blocks = PIKE_CONCAT(DATA,_free_blocks);			\
  ctx->num_empty_blocks = PIKE_CONCAT3(num_empty_,DATA,_blocks);	\
  PIKE_CONCAT(DATA,_blocks) = 0;					\
  PIKE_CONCAT(DATA,_free_blocks) = 0;					\
  PIKE_CONCAT3(num_empty_,DATA,_blocks) = 0;				\
}									\
									\
static void PIKE_CONCAT(alloc_more_,DATA)(void)				\
{                                                                       \
  struct PIKE_CONCAT(DATA,_block) *n;					\
  size_t e;								\
  n=(struct PIKE_CONCAT(DATA,_block) *)					\
     malloc(sizeof(struct PIKE_CONCAT(DATA,_block)));			\
  if(!n)								\
  {									\
    fprintf(stderr,"Fatal: out of memory.\n");				\
    exit(17);								\
  }									\
  if((n->next=PIKE_CONCAT(DATA,_blocks)))				\
    n->next->prev=n;							\
  n->prev=NULL;								\
  n->used=0;								\
  DO_IF_DMALLOC(n->real_used = 0);					\
  PIKE_CONCAT(DATA,_blocks)=n;						\
  PIKE_CONCAT(DATA,_free_blocks)=n;					\
									\
  DO_PRE_INIT_BLOCK( n->x );						\
  n->x[0].BLOCK_ALLOC_NEXT=NULL;					\
  for(e=1;e<(BSIZE);e++)						\
  {									\
    DO_PRE_INIT_BLOCK( (n->x+e) );					\
    n->x[e].BLOCK_ALLOC_NEXT=(void *)&n->x[e-1];			\
  }									\
  n->PIKE_CONCAT3(free_,DATA,s)=&n->x[(BSIZE)-1];			\
  /* Mark the new blocks as unavailable for now... */			\
  PIKE_MEM_NA(n->x);							\
}									\
									\
BA_STATIC BA_INLINE struct DATA *BA_UL(PIKE_CONCAT(alloc_,DATA))(void)	\
{									\
  struct DATA *tmp;							\
  struct PIKE_CONCAT(DATA,_block) *blk;					\
									\
  if(!(blk = PIKE_CONCAT(DATA,_free_blocks))) {				\
    PIKE_CONCAT(alloc_more_,DATA)();					\
    blk = PIKE_CONCAT(DATA,_blocks);					\
    blk->BLOCK_ALLOC_USED++;						\
  }									\
  DO_IF_DEBUG(								\
    else if (PIKE_CONCAT(DATA,_free_blocks) == (void *)-1)		\
      Pike_fatal("Block alloc " #DATA " not initialized.\n");		\
  )									\
  else if(!blk->BLOCK_ALLOC_USED++)					\
    --PIKE_CONCAT3(num_empty_,DATA,_blocks);				\
  DO_IF_DMALLOC(blk->used++);						\
									\
  tmp = blk->PIKE_CONCAT3(free_,DATA,s);				\
  /* Mark the new block as available. */				\
  PIKE_MEM_RW(*tmp);							\
  if(!(blk->PIKE_CONCAT3(free_,DATA,s) = (void *)tmp->BLOCK_ALLOC_NEXT)) \
    PIKE_CONCAT(DATA,_free_blocks) = blk->prev;				\
  DO_IF_DMALLOC(                                                        \
    dmalloc_unregister(tmp, 1);                                         \
    dmalloc_register(tmp,sizeof(struct DATA), DMALLOC_LOCATION());      \
  )                                                                     \
  /* Mark the new block as available but uninitialized. */		\
  PIKE_MEM_WO(*tmp);							\
  INIT_BLOCK(tmp);							\
  return tmp;								\
}									\
									\
DO_IF_RUN_UNLOCKED(                                                     \
struct DATA *PIKE_CONCAT(alloc_,DATA)(void)			        \
{									\
  struct DATA *ret;  							\
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));		\
  ret=PIKE_CONCAT3(alloc_,DATA,_unlocked)();  				\
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));             \
  return ret;								\
})									\
									\
DO_IF_DMALLOC(                                                          \
static void PIKE_CONCAT3(dmalloc_,DATA,_not_freed) (struct DATA *d,	\
						    const char *msg)	\
{									\
  /* Separate function to allow gdb breakpoints. */			\
  fprintf (stderr, "struct " TOSTR(DATA)				\
	   " at %p is still in use %s\n", d, msg);			\
}									\
									\
static void PIKE_CONCAT(dmalloc_late_free_,DATA) (struct DATA *d)	\
{									\
  /* Separate function to allow gdb breakpoints. */			\
  fprintf (stderr, "struct " TOSTR(DATA) " at %p freed now (too late)\n", d); \
  dmalloc_mark_as_free (d, 1);						\
  dmalloc_unregister (d, 1);						\
  PIKE_MEM_NA (*d);							\
}									\
									\
static void PIKE_CONCAT3(dmalloc_free_,DATA,_block) (			\
  struct PIKE_CONCAT(DATA,_block) *blk, const char *msg)		\
{									\
  int dont_free = 0;							\
  size_t i;								\
  for (i = 0; i < (BSIZE); i++) {					\
    if (!dmalloc_check_allocated (blk->x + i, 0))			\
      dmalloc_unregister (blk->x + i, 1);				\
    else if (dmalloc_check_allocated (blk->x + i, 1)) {			\
      PIKE_CONCAT3(dmalloc_,DATA,_not_freed) (blk->x + i, msg);		\
      DMALLOC_DESCRIBE_BLOCK ((blk->x + i));				\
      debug_malloc_dump_references (blk->x + i, 0, 2, 0);		\
      dont_free = 1;							\
    }									\
  }									\
  if (dont_free) {							\
    /* If a block still is in use we conciously leak this */		\
    /* meta-block to allow access to it later. That way we can */	\
    /* avoid fatals before the leak report. */				\
    dmalloc_accept_leak (blk);						\
  }									\
  else {								\
    /* Mark meta-block as available, since libc will mess with it. */	\
    PIKE_MEM_RW (blk->x);						\
    free ((char *) blk);						\
  }									\
}									\
)									\
									\
void PIKE_CONCAT(really_free_,DATA)(struct DATA *d)			\
{									\
  struct PIKE_CONCAT(DATA,_block) *blk;					\
									\
  EXIT_BLOCK(d);							\
									\
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));               \
									\
  DO_IF_DMALLOC({							\
      blk = PIKE_CONCAT(DATA,_free_blocks);				\
      if(blk == NULL || (char *)d < (char *)blk ||			\
	 (char *)d >= (char *)(blk->x+(BSIZE))) {			\
	blk = PIKE_CONCAT(DATA,_blocks);				\
	while (blk && ((char *)d < (char *)blk ||			\
		       (char *)d >= (char *)(blk->x+(BSIZE))))		\
	  blk = blk->next;						\
      }									\
      if (blk) {							\
	if (!dmalloc_check_allocated (d, 0)) {				\
	  debug_malloc_dump_references(d, 0, 2, 0);			\
	  Pike_fatal ("really_free_" TOSTR(DATA) " called with "	\
		      "unknown pointer %p (probably already freed)\n", d); \
	}								\
	blk->used--;							\
	dmalloc_mark_as_free(d, 1);					\
	DO_IF_BLOCK_BACKLOG({						\
	    struct DATA *d2 =						\
	      PIKE_CONCAT(DATA,s_to_free)[PIKE_CONCAT(DATA,s_to_free_ptr)]; \
	    PIKE_MEM_NA(*d);						\
	    PIKE_CONCAT(DATA,s_to_free)[PIKE_CONCAT(DATA,s_to_free_ptr)] = d; \
	    PIKE_CONCAT(DATA,s_to_free_ptr) =				\
	      (PIKE_CONCAT(DATA,s_to_free_ptr) + 1) %			\
	      NELEM(PIKE_CONCAT(DATA,s_to_free));			\
	    if ((d = d2))						\
	      PIKE_MEM_WO(*d);						\
	    else							\
	      return;							\
	  });								\
      }									\
      else if (dmalloc_check_allocated (d, 0)) {			\
	PIKE_CONCAT(dmalloc_late_free_,DATA) (d);			\
	return;								\
      }									\
    });									\
									\
  blk = PIKE_CONCAT(DATA,_free_blocks);					\
  if(blk == NULL || (char *)d < (char *)blk ||				\
     (char *)d >= (char *)(blk->x+(BSIZE))) {				\
    blk = PIKE_CONCAT(DATA,_blocks);					\
    DO_IF_DEBUG (							\
      if (!blk) Pike_fatal ("really_free_" TOSTR(DATA)			\
			    " got invalid pointer %p\n", d)		\
    );									\
    if((char *)d < (char *)blk ||					\
       (char *)d >= (char *)(blk->x+(BSIZE))) {				\
      do {								\
        blk = blk->next;						\
	DO_IF_DEBUG (							\
	  if (!blk) Pike_fatal ("really_free_" TOSTR(DATA)		\
				" got invalid pointer %p\n", d)		\
	);								\
      } while((char *)d < (char *)blk ||				\
	      (char *)d >= (char *)(blk->x+(BSIZE)));			\
      if(blk == PIKE_CONCAT(DATA,_free_blocks))				\
        PIKE_CONCAT(DATA,_free_blocks) = blk->prev;			\
      blk->prev->next = blk->next;					\
      if(blk->next)							\
        blk->next->prev = blk->prev;					\
      blk->prev = NULL;							\
      blk->next = PIKE_CONCAT(DATA,_blocks);				\
      blk->next->prev = blk;						\
      PIKE_CONCAT(DATA,_blocks) = blk;					\
    }									\
    if(PIKE_CONCAT(DATA,_free_blocks) == NULL)				\
      PIKE_CONCAT(DATA,_free_blocks) = blk;				\
  }									\
									\
  DO_IF_DEBUG (								\
    if ((char *) d < (char *) &blk->x ||				\
	((char *) d - (char *) &blk->x) % sizeof (struct DATA))		\
      Pike_fatal ("really_free_" TOSTR(DATA)				\
		  " got misaligned pointer %p\n", d);			\
  );									\
									\
  DO_PRE_INIT_BLOCK(d);							\
  DO_IF_BLOCK_BACKLOG ({						\
      struct DATA *d2 = (void *)blk->PIKE_CONCAT3(free_,DATA,s);	\
      d->BLOCK_ALLOC_NEXT = NULL;					\
      if (d2) {								\
	while (d2->BLOCK_ALLOC_NEXT) {					\
	  if (PTR_TO_INT(d2->BLOCK_ALLOC_NEXT) == 0x55555555) {		\
	    debug_malloc_dump_references(d2, 0, 2, 0);			\
	    Pike_fatal("Bad next pointer in free list.\n");		\
	  }								\
	  d2 = (void *)d2->BLOCK_ALLOC_NEXT;				\
	}								\
	d2->BLOCK_ALLOC_NEXT = (void *)d;				\
      } else {								\
	blk->PIKE_CONCAT3(free_,DATA,s)=d;				\
      }									\
    });									\
  DO_IF_NOT_BLOCK_BACKLOG ({						\
      d->BLOCK_ALLOC_NEXT = (void *)blk->PIKE_CONCAT3(free_,DATA,s);	\
      blk->PIKE_CONCAT3(free_,DATA,s)=d;				\
    });									\
  /* Mark block as unavailable. */					\
  PIKE_MEM_NA(*d);							\
									\
  if(!--blk->BLOCK_ALLOC_USED &&					\
     ++PIKE_CONCAT3(num_empty_,DATA,_blocks) > MAX_EMPTY_BLOCKS) {	\
    if(blk == PIKE_CONCAT(DATA,_free_blocks)) {				\
      /* blk->prev isn't NULL because MAX_EMPTY_BLOCKS >= 1 so we */	\
      /* know there's at least one more empty block. */			\
      if((blk->prev->next = blk->next))					\
        blk->next->prev = blk->prev;					\
      PIKE_CONCAT(DATA,_free_blocks) = blk->prev;			\
    } else {								\
      PIKE_CONCAT(DATA,_blocks) = blk->next;				\
      blk->next->prev = NULL;						\
    }									\
									\
    DO_IF_DMALLOC (							\
      PIKE_CONCAT3(dmalloc_free_,DATA,_block) (				\
	blk, "in block expected to be empty")				\
    );									\
    DO_IF_NOT_DMALLOC(							\
      /* Mark meta-block as available, since libc will mess with it. */	\
      PIKE_MEM_RW(*blk);						\
      free(blk);							\
    );									\
									\
    --PIKE_CONCAT3(num_empty_,DATA,_blocks);				\
  }									\
									\
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));             \
}									\
									\
static void PIKE_CONCAT3(free_all_,DATA,_blocks_unlocked)(void)		\
{									\
  struct PIKE_CONCAT(DATA,_block) *tmp;					\
									\
  DO_IF_DMALLOC(							\
    DO_IF_BLOCK_BACKLOG (						\
      MEMSET(PIKE_CONCAT(DATA,s_to_free), 0,				\
	     sizeof(PIKE_CONCAT(DATA,s_to_free)));			\
    );									\
    while ((tmp = PIKE_CONCAT(DATA,_blocks))) {				\
      PIKE_CONCAT(DATA,_blocks) = tmp->next;				\
      PIKE_CONCAT3(dmalloc_free_,DATA,_block) (				\
	tmp, "in free_all_" TOSTR(DATA) "_blocks");			\
    }									\
  );									\
									\
  DO_IF_NOT_DMALLOC(							\
    while((tmp=PIKE_CONCAT(DATA,_blocks)))				\
    {									\
      PIKE_CONCAT(DATA,_blocks)=tmp->next;				\
      /* Mark meta-block as available, since libc will mess with it. */	\
      PIKE_MEM_RW(tmp->x);						\
      free((char *)tmp);						\
    }									\
  );									\
									\
  if (PIKE_CONCAT(DATA,_ctxs)) {					\
    struct PIKE_CONCAT(DATA, _context) *ctx = PIKE_CONCAT(DATA,_ctxs);	\
    PIKE_CONCAT(DATA,_blocks)=ctx->blocks;				\
    PIKE_CONCAT(DATA,_free_blocks)=ctx->free_blocks;			\
    PIKE_CONCAT3(num_empty_,DATA,_blocks)=ctx->num_empty_blocks;	\
    PIKE_CONCAT(DATA,_ctxs) = ctx->next;				\
    free(ctx);								\
  } else {								\
    PIKE_CONCAT(DATA,_blocks)=0;					\
    PIKE_CONCAT(DATA,_free_blocks)=0;					\
    PIKE_CONCAT3(num_empty_,DATA,_blocks)=0;				\
  }									\
}									\
									\
void PIKE_CONCAT3(free_all_,DATA,_blocks)(void)				\
{									\
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));               \
  PIKE_CONCAT3(free_all_,DATA,_blocks_unlocked)();  			\
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));             \
}                                                                       \
									\
void PIKE_CONCAT3(count_memory_in_,DATA,s)(size_t *num_, size_t *size_)	\
{									\
  size_t num=0, size=0;							\
  struct PIKE_CONCAT(DATA,_block) *tmp;					\
  struct PIKE_CONCAT(DATA,_context) *ctx = PIKE_CONCAT(DATA,_ctxs);	\
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));               \
  for(tmp=PIKE_CONCAT(DATA,_blocks);tmp;tmp=tmp->next)			\
  {									\
    size+=sizeof(struct PIKE_CONCAT(DATA,_block));			\
    num+=tmp->used;							\
    COUNT_BLOCK(tmp);                                                   \
  }									\
  while (ctx) {								\
    for(tmp=ctx->blocks;tmp;tmp=tmp->next)				\
    {									\
      size+=sizeof(struct PIKE_CONCAT(DATA,_block));			\
      num+=tmp->used;							\
      COUNT_BLOCK(tmp);							\
    }									\
    ctx = ctx->next;							\
  }									\
  COUNT_OTHER();                                                        \
  *num_=num;								\
  *size_=size;								\
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));             \
}                                                                       \
									\
									\
void PIKE_CONCAT3(init_,DATA,_blocks)(void)				\
{                                                                       \
/*  DO_IF_RUN_UNLOCKED(mt_init_recursive(&PIKE_CONCAT(DATA,_mutex)));*/ \
  DO_IF_RUN_UNLOCKED(mt_init(&PIKE_CONCAT(DATA,_mutex)));               \
  PIKE_CONCAT(DATA,_free_blocks)=0;					\
}



#define LOW_PTR_HASH_ALLOC(DATA,BSIZE)					     \
									     \
BLOCK_ALLOC(DATA,BSIZE)							     \
									     \
struct DATA **PIKE_CONCAT(DATA,_hash_table)=0;				     \
size_t PIKE_CONCAT(DATA,_hash_table_size)=0;				     \
size_t PIKE_CONCAT(DATA,_hash_table_magnitude)=0;			     \
static size_t PIKE_CONCAT(num_,DATA)=0;					     \
									     \
static INLINE struct DATA *						     \
 PIKE_CONCAT3(really_low_find_,DATA,_unlocked)(void *ptr,		     \
					       PIKE_HASH_T hval)	     \
{									     \
  struct DATA *p,**pp;							     \
  p=PIKE_CONCAT(DATA,_hash_table)[hval];                                     \
  if(!p || p->PTR_HASH_ALLOC_DATA == ptr)				     \
  {                                                                          \
    return p;                                                                \
  }                                                                          \
  while((p=*(pp=&p->BLOCK_ALLOC_NEXT)))                                      \
  {									     \
    if(p->PTR_HASH_ALLOC_DATA==ptr)					     \
    {									     \
      *pp=p->BLOCK_ALLOC_NEXT;						     \
      p->BLOCK_ALLOC_NEXT=PIKE_CONCAT(DATA,_hash_table)[hval];		     \
      PIKE_CONCAT(DATA,_hash_table)[hval]=p;				     \
      return p;								     \
    }									     \
  }									     \
  return 0;								     \
}									     \
									     \
struct DATA *PIKE_CONCAT(find_,DATA)(void *ptr)				     \
{									     \
  struct DATA *p;                                                            \
  PIKE_HASH_T hval = (PIKE_HASH_T)PTR_TO_INT(ptr);			     \
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));                    \
  if(!PIKE_CONCAT(DATA,_hash_table)) {                                       \
    DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                \
    return 0;                                                                \
  }                                                                          \
  hval %= (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);		     \
  p=PIKE_CONCAT3(really_low_find_,DATA,_unlocked)(ptr, hval);		     \
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                  \
  return p;								     \
}									     \
									     \
static INLINE struct DATA *						     \
 PIKE_CONCAT3(just_find_,DATA,_unlocked)(void *ptr,			     \
					 PIKE_HASH_T hval)		     \
{									     \
  struct DATA *p;							     \
  p=PIKE_CONCAT(DATA,_hash_table)[hval];                                     \
  if(!p || p->PTR_HASH_ALLOC_DATA == ptr)				     \
  {                                                                          \
    return p;                                                                \
  }                                                                          \
  while((p=p->BLOCK_ALLOC_NEXT)) 	                                     \
  {									     \
    if(p->PTR_HASH_ALLOC_DATA==ptr) return p;				     \
  }									     \
  return 0;								     \
}									     \
									     \
static struct DATA *PIKE_CONCAT(just_find_,DATA)(void *ptr)		     \
{									     \
  struct DATA *p;                                                            \
  PIKE_HASH_T hval = (PIKE_HASH_T)PTR_TO_INT(ptr);			     \
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));                    \
  if(!PIKE_CONCAT(DATA,_hash_table)) {                                       \
    DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                \
    return 0;                                                                \
  }                                                                          \
  hval %= (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);		     \
  p=PIKE_CONCAT3(just_find_,DATA,_unlocked)(ptr, hval);			     \
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                  \
  return p;								     \
}									     \
									     \
									     \
									     \
struct DATA *PIKE_CONCAT(make_,DATA)(void *ptr)				     \
{									     \
  struct DATA *p;							     \
  PIKE_HASH_T hval = (PIKE_HASH_T)PTR_TO_INT(ptr);			     \
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));                    \
  hval %= (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);		     \
  p=PIKE_CONCAT3(make_,DATA,_unlocked)(ptr,hval);                            \
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                  \
  return p;								     \
}									     \
									     \
struct DATA *PIKE_CONCAT(get_,DATA)(void *ptr)			 	     \
{									     \
  struct DATA *p;							     \
  PIKE_HASH_T hval = (PIKE_HASH_T)PTR_TO_INT(ptr);			     \
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));                    \
  hval %= (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);		     \
  if(!(p=PIKE_CONCAT3(really_low_find_,DATA,_unlocked)(ptr, hval)))          \
    p=PIKE_CONCAT3(make_,DATA,_unlocked)(ptr, hval);		             \
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                  \
  return p;                                                                  \
}									     \
									     \
int PIKE_CONCAT3(check_,DATA,_semaphore)(void *ptr)			     \
{									     \
  PIKE_HASH_T hval = (PIKE_HASH_T)PTR_TO_INT(ptr);			     \
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));                    \
  hval %= (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);		     \
  if(PIKE_CONCAT3(really_low_find_,DATA,_unlocked)(ptr, hval))		     \
  {                                                                          \
    DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                \
    return 0;								     \
  }                                                                          \
									     \
  PIKE_CONCAT3(make_,DATA,_unlocked)(ptr, hval);			     \
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                  \
  return 1;								     \
}									     \
									     \
void PIKE_CONCAT(move_,DATA)(struct DATA *block, void *new_ptr)		     \
{									     \
  PIKE_HASH_T hval =							     \
    (PIKE_HASH_T)PTR_TO_INT(block->PTR_HASH_ALLOC_DATA);		     \
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));		     \
  hval %= (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);		     \
  if (!PIKE_CONCAT3(really_low_find_,DATA,_unlocked)(			     \
	block->PTR_HASH_ALLOC_DATA, hval))				     \
    Pike_fatal("The block to move wasn't found.\n");			     \
  DO_IF_DEBUG(								     \
    if (PIKE_CONCAT(DATA,_hash_table)[hval] != block)			     \
      Pike_fatal("Expected the block to be at the top of the hash chain.\n"); \
  );									     \
  PIKE_CONCAT(DATA,_hash_table)[hval] = block->BLOCK_ALLOC_NEXT;	     \
  block->PTR_HASH_ALLOC_DATA = new_ptr;					     \
  hval = (PIKE_HASH_T)PTR_TO_INT(new_ptr) %				     \
    (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);			     \
  block->BLOCK_ALLOC_NEXT = PIKE_CONCAT(DATA,_hash_table)[hval];	     \
  PIKE_CONCAT(DATA,_hash_table)[hval] = block;				     \
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));		     \
}									     \
									     \
int PIKE_CONCAT(remove_,DATA)(void *ptr)				     \
{									     \
  struct DATA *p;							     \
  PIKE_HASH_T hval = (PIKE_HASH_T)PTR_TO_INT(ptr);			     \
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));                    \
  if(!PIKE_CONCAT(DATA,_hash_table))                                         \
  {                                                                          \
    DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                \
    return 0;				                                     \
  }                                                                          \
  hval %= (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);		     \
  if((p=PIKE_CONCAT3(really_low_find_,DATA,_unlocked)(ptr, hval)))	     \
  {									     \
    PIKE_CONCAT(num_,DATA)--;						     \
    DO_IF_DEBUG( if(PIKE_CONCAT(DATA,_hash_table)[hval]!=p)                  \
                    Pike_fatal("GAOssdf\n"); );                       	     \
    PIKE_CONCAT(DATA,_hash_table)[hval]=p->BLOCK_ALLOC_NEXT;		     \
    BA_UL(PIKE_CONCAT(really_free_,DATA))(p);				     \
    DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                \
    return 1;								     \
  }									     \
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                  \
  return 0;								     \
}									     \
									     \
void PIKE_CONCAT3(low_init_,DATA,_hash)(size_t size)			     \
{									     \
  extern const INT32 hashprimes[32];					     \
  extern int my_log2(size_t x);					             \
  PIKE_CONCAT3(init_,DATA,_blocks)();                                        \
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));                    \
  PIKE_CONCAT(DATA,_hash_table_size)=					     \
     hashprimes[PIKE_CONCAT(DATA,_hash_table_magnitude)=my_log2(size)];	     \
									     \
  PIKE_CONCAT(DATA,_hash_table)=(struct DATA **)			     \
    malloc(sizeof(struct DATA *)*PIKE_CONCAT(DATA,_hash_table_size));	     \
  if(!PIKE_CONCAT(DATA,_hash_table))					     \
  {									     \
    fprintf(stderr,"Fatal: out of memory.\n");				     \
    exit(17);								     \
  }									     \
  MEMSET(PIKE_CONCAT(DATA,_hash_table),0,				     \
	 sizeof(struct DATA *)*PIKE_CONCAT(DATA,_hash_table_size));	     \
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                  \
}									     \
									     \
									     \
void PIKE_CONCAT3(init_,DATA,_hash)(void)				     \
{									     \
  PIKE_CONCAT3(low_init_,DATA,_hash)(BSIZE);		                     \
}									     \
									     \
void PIKE_CONCAT3(exit_,DATA,_hash)(void)				     \
{									     \
  DO_IF_RUN_UNLOCKED(mt_lock(&PIKE_CONCAT(DATA,_mutex)));                    \
  PIKE_CONCAT3(free_all_,DATA,_blocks_unlocked)();			     \
  free(PIKE_CONCAT(DATA,_hash_table));					     \
  PIKE_CONCAT(DATA,_hash_table)=0;					     \
  PIKE_CONCAT(num_,DATA)=0;						     \
  DO_IF_RUN_UNLOCKED(mt_unlock(&PIKE_CONCAT(DATA,_mutex)));                  \
}

#define PTR_HASH_ALLOC_FIXED(DATA,BSIZE)				     \
struct DATA *PIKE_CONCAT3(make_,DATA,_unlocked)(void *ptr, PIKE_HASH_T hval);\
LOW_PTR_HASH_ALLOC(DATA,BSIZE)                                               \
									     \
struct DATA *PIKE_CONCAT3(make_,DATA,_unlocked)(void *ptr, PIKE_HASH_T hval) \
{									     \
  struct DATA *p;							     \
									     \
  DO_IF_DEBUG( if(!PIKE_CONCAT(DATA,_hash_table))			     \
    Pike_fatal("Hash table error!\n"); )				     \
  PIKE_CONCAT(num_,DATA)++;						     \
									     \
  p=BA_UL(PIKE_CONCAT(alloc_,DATA))();					     \
  p->PTR_HASH_ALLOC_DATA=ptr;						     \
  p->BLOCK_ALLOC_NEXT=PIKE_CONCAT(DATA,_hash_table)[hval];		     \
  PIKE_CONCAT(DATA,_hash_table)[hval]=p;				     \
  return p;								     \
}


#define PTR_HASH_ALLOC(DATA,BSIZE)					     \
struct DATA *PIKE_CONCAT3(make_,DATA,_unlocked)(void *ptr,		     \
						PIKE_HASH_T hval);	     \
LOW_PTR_HASH_ALLOC(DATA,BSIZE)                                               \
                                                                             \
static void PIKE_CONCAT(DATA,_rehash)(void)				     \
{									     \
  /* Time to re-hash */							     \
  extern const INT32 hashprimes[32];					     \
  struct DATA **old_hash;	                                 	     \
  struct DATA *p;							     \
  PIKE_HASH_T hval;							     \
  size_t e;								     \
                                                                             \
  old_hash= PIKE_CONCAT(DATA,_hash_table);		                     \
  e=PIKE_CONCAT(DATA,_hash_table_size);			                     \
									     \
  PIKE_CONCAT(DATA,_hash_table_magnitude)++;				     \
  PIKE_CONCAT(DATA,_hash_table_size)=					     \
    hashprimes[PIKE_CONCAT(DATA,_hash_table_magnitude)];		     \
  if((PIKE_CONCAT(DATA,_hash_table)=(struct DATA **)			     \
      malloc(PIKE_CONCAT(DATA,_hash_table_size)*			     \
	     sizeof(struct DATA *))))					     \
  {									     \
    MEMSET(PIKE_CONCAT(DATA,_hash_table),0,				     \
	   sizeof(struct DATA *)*PIKE_CONCAT(DATA,_hash_table_size));	     \
    while(e-- > 0)							     \
    {									     \
      while((p=old_hash[e]))						     \
      {									     \
	old_hash[e]=p->BLOCK_ALLOC_NEXT;				     \
	hval = (PIKE_HASH_T)PTR_TO_INT(p->PTR_HASH_ALLOC_DATA);		     \
	hval %= (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);	     \
	p->BLOCK_ALLOC_NEXT=PIKE_CONCAT(DATA,_hash_table)[hval];	     \
	PIKE_CONCAT(DATA,_hash_table)[hval]=p;				     \
      }									     \
    }									     \
    free((char *)old_hash);						     \
  }else{								     \
    PIKE_CONCAT(DATA,_hash_table)=old_hash;				     \
    PIKE_CONCAT(DATA,_hash_table_size)=e;				     \
  }									     \
}									     \
									     \
struct DATA *PIKE_CONCAT3(make_,DATA,_unlocked)(void *ptr,		     \
						PIKE_HASH_T hval)	     \
{									     \
  struct DATA *p;							     \
									     \
  DO_IF_DEBUG( if(!PIKE_CONCAT(DATA,_hash_table))			     \
    Pike_fatal("Hash table error!\n"); )				     \
  PIKE_CONCAT(num_,DATA)++;						     \
									     \
  if(( PIKE_CONCAT(num_,DATA)>>BLOCK_ALLOC_HSIZE_SHIFT ) >=		     \
     PIKE_CONCAT(DATA,_hash_table_size))				     \
  {									     \
    PIKE_CONCAT(DATA,_rehash)();					     \
    hval = (PIKE_HASH_T)PTR_TO_INT(ptr);				     \
    hval %= (PIKE_HASH_T)PIKE_CONCAT(DATA,_hash_table_size);		     \
  }									     \
									     \
  p=BA_UL(PIKE_CONCAT(alloc_,DATA))();					     \
  p->PTR_HASH_ALLOC_DATA=ptr;						     \
  p->BLOCK_ALLOC_NEXT=PIKE_CONCAT(DATA,_hash_table)[hval];		     \
  PIKE_CONCAT(DATA,_hash_table)[hval]=p;				     \
  return p;								     \
}
