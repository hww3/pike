/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: dmalloc.h,v 1.40 2002/11/25 00:52:18 mast Exp $
*/

PMOD_EXPORT extern void *debug_xalloc(size_t);
PMOD_EXPORT extern void debug_xfree(void *);
PMOD_EXPORT extern void *debug_xmalloc(size_t);
PMOD_EXPORT extern void *debug_xcalloc(size_t,size_t);
PMOD_EXPORT extern void *debug_xrealloc(void *,size_t);

#define DMALLOC_LOCATION() (("NS"  __FILE__ ":" DEFINETOSTR(__LINE__) )+1)

#ifdef DMALLOC_TRACE
#define DMALLOC_TRACELOGSIZE 131072

extern char *dmalloc_tracelog[DMALLOC_TRACELOGSIZE];
extern size_t dmalloc_tracelogptr;

#define DMALLOC_TRACE_LOG(X)  (dmalloc_tracelog[ dmalloc_tracelogptr = (dmalloc_tracelogptr +1 )%DMALLOC_TRACELOGSIZE ] = (X))

#endif /* DMALLOC_TRACE */

#ifdef DEBUG_MALLOC
struct memhdr;

void dump_memhdr_locations(struct memhdr *from,
			   struct memhdr *notfrom,
			   int indent);
struct memhdr *alloc_memhdr(void);
void really_free_memhdr(struct memhdr *mh);
void add_marks_to_memhdr(struct memhdr *to,void *ptr);

extern int verbose_debug_malloc;
extern int verbose_debug_exit;
extern void dmalloc_register(void *, int, char *);
extern int dmalloc_unregister(void *, int);
extern void *debug_malloc(size_t,  char *);
extern void *debug_calloc(size_t, size_t,  char *);
extern void *debug_realloc(void *, size_t,  char *);
extern void debug_free(void *,  char *,int);
extern char *debug_strdup(const char *, char *);
extern void reset_debug_malloc(void);
extern void dmalloc_free(void *p);
extern int debug_malloc_touch_fd(int,  char *);
extern int debug_malloc_register_fd(int,  char *);
extern int debug_malloc_close_fd(int,  char *);
extern int dmalloc_mark_as_free(void*,int);

void *debug_malloc_update_location(void *, char *);
void *debug_malloc_update_location_ptr(void *, ptrdiff_t, char *);
void search_all_memheaders_for_references(void);
void cleanup_memhdrs(void);
void cleanup_debug_malloc(void);

/* Beware! names of named memory regions are never ever freed!! /Hubbe */
void *debug_malloc_name(void *p, const char *fn, int line);
int debug_malloc_copy_names(void *p, void *p2);
char *dmalloc_find_name(void *p);

/* glibc 2.1 defines this as a macro. */
#ifdef strdup
#undef strdup
#endif

#define malloc(x) debug_malloc((x), DMALLOC_LOCATION())
#define calloc(x, y) debug_calloc((x), (y), DMALLOC_LOCATION())
#define realloc(x, y) debug_realloc((x), (y), DMALLOC_LOCATION())
#define free(x) debug_free((x), DMALLOC_LOCATION(),0)
#define dmfree(x) debug_free((x),DMALLOC_LOCATION(),1)
#define strdup(x) debug_strdup((x), DMALLOC_LOCATION())
#define DO_IF_DMALLOC(X) X
#define debug_malloc_touch(X) debug_malloc_update_location((X),DMALLOC_LOCATION())
#define debug_malloc_pass(X) debug_malloc_update_location((X),DMALLOC_LOCATION())
#define dmalloc_touch_struct_ptr(TYPE,X,MEMBER) ((TYPE)debug_malloc_update_location_ptr((X), ((ptrdiff_t)& (((TYPE)0)->MEMBER)), DMALLOC_LOCATION()))

#define xalloc(X) ((void *)debug_malloc_pass(debug_xalloc(X)))
#define xfree(X) debug_xfree(debug_malloc_pass((X)))
void debug_malloc_dump_references(void *x, int indent, int depth, int flags);
#define dmalloc_touch(TYPE,X) ((TYPE)debug_malloc_update_location((X),DMALLOC_LOCATION()))
void debug_malloc_dump_fd(int fd);
#define dmalloc_touch_svalue(X) do { struct svalue *_tmp = (X); if ((X)->type <= MAX_REF_TYPE) { debug_malloc_touch(_tmp->u.refs); } } while(0)

#define DMALLOC_LINE_ARGS ,char * dmalloc_location
#define DMALLOC_POS ,DMALLOC_LOCATION()
#define DMALLOC_PROXY_ARGS ,dmalloc_location
void dmalloc_accept_leak(void *);
#define dmalloc_touch_fd(X) debug_malloc_touch_fd((X),DMALLOC_LOCATION())
#define dmalloc_register_fd(X) debug_malloc_register_fd((X),DMALLOC_LOCATION())
#define dmalloc_close_fd(X) debug_malloc_close_fd((X),DMALLOC_LOCATION())

/* Beware, these do not exist without DMALLOC */
struct memory_map;
void dmalloc_set_mmap(void *ptr, struct memory_map *m);
void dmalloc_set_mmap_template(void *ptr, struct memory_map *m);
void dmalloc_set_mmap_from_template(void *p, void *p2);
void dmalloc_describe_location(void *p, int offset, int indent);
struct memory_map *dmalloc_alloc_mmap(char *name, int line);
void dmalloc_add_mmap_entry(struct memory_map *m,
			    char *name,
			    int offset,
			    int size,
			    int count,
			    struct memory_map *recur,
			    int recur_offset);
int dmalloc_is_invalid_memory_block(void *block);


#else /* DEBUG_MALLOC */

#define dmalloc_touch_fd(X) (X)
#define dmalloc_register_fd(X) (X)
#define dmalloc_close_fd(X) (X)
#define dmfree(X) free((X))
#define dmalloc_accept_leak(X) (void)(X)
#define DMALLOC_LINE_ARGS 
#define DMALLOC_POS 
#define DMALLOC_PROXY_ARGS
#define debug_malloc_dump_references(X,x,y,z)
#define debug_malloc_dump_fd(fd)
#define xalloc debug_xalloc

#if defined(DYNAMIC_MODULE) && defined(__NT__)
#define xmalloc debug_xmalloc
#define xcalloc debug_xcalloc
#define xrealloc debug_xrealloc
#define xfree debug_xfree
#else /* defined(DYNAMIC_MODULE) && defined(__NT__) */
#define xmalloc malloc
#define xcalloc calloc
#define xrealloc realloc
#define xfree free
#endif /* !(defined(DYNAMIC_MODULE) && defined(__NT__)) */

#define dbm_main main
#define DO_IF_DMALLOC(X)
#define dmalloc_register(X,Y,Z)
#define dmalloc_unregister(X,Y)
#define debug_free(X,Y,Z) free((X))
#define debug_malloc_name(P,FN,LINE)
#define debug_malloc_copy_names(p,p2) 0
#define search_all_memheaders_for_references()
#define dmalloc_find_name(X) "unknown (no dmalloc)"
#define dmalloc_touch_struct_ptr(TYPE,X,MEMBER) (X)

#ifdef DMALLOC_TRACE
#define debug_malloc_update_location(X,Y) (DMALLOC_TRACE_LOG(DMALLOC_LOCATION()),(X))
#define dmalloc_touch_svalue(X) DMALLOC_TRACE_LOG(DMALLOC_LOCATION())
#define debug_malloc_touch(X) DMALLOC_TRACE_LOG(DMALLOC_LOCATION())
#define debug_malloc_pass(X) (DMALLOC_TRACE_LOG(DMALLOC_LOCATION()),(X))
#define dmalloc_touch(TYPE,X) (DMALLOC_TRACE_LOG(DMALLOC_LOCATION()),(X))
#else /* DMALLOC_TRACE */
#define debug_malloc_update_location(X,Y) (X)
#define dmalloc_touch_svalue(X)
#define debug_malloc_touch(X)
#define debug_malloc_pass(X) (X)
#define dmalloc_touch(TYPE,X) (X)
#endif /* !MALLOC_TRACE */

#endif /* !DEBUG_MALLOC */
