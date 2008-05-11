/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: gc.h,v 1.133 2008/05/11 02:31:57 mast Exp $
*/

#ifndef GC_H
#define GC_H

#include "global.h"
#include "callback.h"
#include "queue.h"
#include "threads.h"
#include "interpret.h"
#include "pike_rusage.h"

/* 1: Normal operation. 0: Disable automatic gc runs. -1: Disable
 * completely. */
extern int gc_enabled;

/* As long as the gc time is less than gc_time_ratio, aim to run the
 * gc approximately every time the ratio between the garbage and the
 * total amount of allocated things is this. */
extern double gc_garbage_ratio_low;

/* When more than this fraction of the cpu time is spent in the gc,
 * aim to minimize it as long as the garbage ratio is less than
 * gc_garbage_ratio_high. */
extern double gc_time_ratio;

/* If the garbage ratio gets up to this value, disregard gc_time_ratio
 * and start running the gc as often as it takes so that it doesn't
 * get any higher. */
extern double gc_garbage_ratio_high;

/* When predicting the next gc interval, use a decaying average with
 * this slowness factor. It should be a value less than 1.0 that
 * specifies the weight to give to the old average value. The
 * remaining weight up to 1.0 is given to the last reading. */
extern double gc_average_slowness;

/* The above are used to calculate the threshold on the number of
 * allocations since the last gc round before another is scheduled.
 * Put a cap on that threshold to avoid very small intervals. */
#define GC_MIN_ALLOC_THRESHOLD 1000

/* The upper limit on the threshold is set to avoid wrap-around only.
 * We need some space above it since num_allocs can go beyond the
 * threshold if it takes a while until the gc can run. */
#define GC_MAX_ALLOC_THRESHOLD (ALLOC_COUNT_TYPE_MAX - 10000000)

/* #define GC_MARK_DEBUG */

/* If we only have 32 bits we need to make good use of them for the
 * alloc counter since it can get high, but if we have 64 it's best to
 * stay away from unsigned since rotten compilers like MSVC haven't
 * had the energy to implement conversion from that type to floating
 * point. :P */
#ifdef INT64
#define ALLOC_COUNT_TYPE INT64
#define ALLOC_COUNT_TYPE_MAX MAX_INT64
#define PRINT_ALLOC_COUNT_TYPE "%"PRINTINT64"d"
#else
#define ALLOC_COUNT_TYPE unsigned long
#define ALLOC_COUNT_TYPE_MAX ULONG_MAX
#define PRINT_ALLOC_COUNT_TYPE "%lu"
#endif

extern INT32 num_objects;
extern ALLOC_COUNT_TYPE num_allocs, alloc_threshold;
PMOD_EXPORT extern int Pike_in_gc;
extern int gc_generation;
extern int gc_trace, gc_debug;
#ifdef CPU_TIME_MIGHT_NOT_BE_THREAD_LOCAL
extern cpu_time_t auto_gc_time;
#endif

extern struct callback *gc_evaluator_callback;
#ifdef PIKE_DEBUG
extern void *gc_svalue_location;
#endif

#ifdef DO_PIKE_CLEANUP
extern int gc_destruct_everything;
extern int gc_keep_markers;
#else
#define gc_destruct_everything 0
#define gc_keep_markers 0
#endif

#define ADD_GC_CALLBACK() do { if(!gc_evaluator_callback)  gc_evaluator_callback=add_to_callback(&evaluator_callbacks,(callback_func)do_gc,0,0); }while(0)

#define LOW_GC_ALLOC(OBJ) do {						\
 extern int d_flag;							\
 num_objects++;								\
 num_allocs++;								\
 DO_IF_DEBUG(								\
   if(d_flag) CHECK_INTERPRETER_LOCK();					\
   if(Pike_in_gc > GC_PASS_PREPARE && Pike_in_gc < GC_PASS_FREE)	\
     Pike_fatal("Allocating new objects within gc is not allowed!\n");	\
 )									\
 if (Pike_in_gc) remove_marker(OBJ);					\
} while (0)

#ifdef ALWAYS_GC
#define GC_ALLOC(OBJ) do{						\
  LOW_GC_ALLOC(OBJ);							\
  ADD_GC_CALLBACK();							\
} while(0)
#else
#define GC_ALLOC(OBJ)  do{						\
  LOW_GC_ALLOC(OBJ);							\
  if(num_allocs >= alloc_threshold)					\
    ADD_GC_CALLBACK();							\
} while(0)
#endif

#ifdef PIKE_DEBUG

/* Use this when reallocating a block that you've used any gc_check or
 * gc_mark function on. The new block must take over all references
 * that pointed to the old block. */
#define GC_REALLOC_BLOCK(OLDPTR, NEWPTR) do {				\
  extern int d_flag;							\
  if (d_flag) CHECK_INTERPRETER_LOCK();					\
  if (Pike_in_gc > GC_PASS_PREPARE && Pike_in_gc < GC_PASS_FREE)	\
    gc_move_marker ((OLDPTR), (NEWPTR));				\
} while (0)

/* Use this when freeing blocks that you've used any gc_check or
 * gc_mark function on and that can't contain references. */
#define GC_FREE_SIMPLE_BLOCK(PTR) do {					\
  extern int d_flag;							\
  if(d_flag) CHECK_INTERPRETER_LOCK();					\
  if (Pike_in_gc == GC_PASS_CHECK)					\
    Pike_fatal("No free is allowed in this gc pass.\n");		\
  else									\
    remove_marker(PTR);							\
} while (0)

/* Use this when freeing blocks that you've used any gc_check or
 * gc_mark function on and that can contain references. */
#define GC_FREE_BLOCK(PTR) do {						\
  extern int d_flag;							\
  if(d_flag) CHECK_INTERPRETER_LOCK();					\
  if (Pike_in_gc > GC_PASS_PREPARE && Pike_in_gc < GC_PASS_FREE)	\
    Pike_fatal("Freeing objects within gc is not allowed.\n");		\
} while (0)

#else
#define GC_REALLOC_BLOCK(OLDPTR, NEWPTR) do {				\
    if (Pike_in_gc > GC_PASS_PREPARE && Pike_in_gc < GC_PASS_FREE)	\
      gc_move_marker ((OLDPTR), (NEWPTR));				\
  } while (0)
#define GC_FREE_SIMPLE_BLOCK(PTR) do {} while (0)
#define GC_FREE_BLOCK(PTR) do {} while (0)
#endif

#define GC_FREE(PTR) do {						\
  GC_FREE_BLOCK(PTR);							\
  DO_IF_DEBUG(								\
    if(num_objects < 1)							\
      Pike_fatal("Panic!! less than zero objects!\n");			\
  );									\
  num_objects-- ;							\
}while(0)

struct gc_rec_frame;

struct marker
{
  struct marker *next;
  struct gc_rec_frame *frame;	/* Pointer to the cycle check rec frame. */
  void *data;
  INT32 refs;
  /* Internal references (both weak and nonweak). Increased during
   * check pass. */
  INT32 weak_refs;
  /* Weak (implying internal) references. Increased during check pass.
   * Set to -1 during check pass if it reaches the total number of
   * references. Set to 0 during mark pass if a nonweak reference is
   * found. Decreased during zap weak pass as gc_do_weak_free() is
   * called. */
#ifdef PIKE_DEBUG
  INT32 xrefs;
  /* Known external references. Increased by gc_mark_external(). */
  INT32 saved_refs;
  /* References at beginning of gc. Set by pretouch and check passes.
   * Decreased by gc_do_weak_free() as weak references are removed. */
  unsigned INT32 flags;
#else
  unsigned INT16 flags;
#endif
};

#define GC_MARKED		0x0001
/* The thing has less internal references than references, or has been
 * visited by reference from such a thing. Set in the mark pass. */
#define GC_NOT_REFERENCED	0x0002
/* The thing has only internal references and is not referenced from a
 * marked thing. Set in the check pass and cleared in the mark
 * pass. */
#define GC_CYCLE_CHECKED	0x0004
/* The thing has been pushed in the cycle check pass. */
#define GC_LIVE			0x0008
/* The thing is a live object (i.e. not destructed and got a destroy
 * function) or is referenced from a live object. Set in the cycle
 * check pass. */
#define GC_LIVE_OBJ		0x0010
/* The thing is a live object. Set in the cycle check pass. */
#define GC_GOT_DEAD_REF		0x0040
/* 1.  The thing has lost all references but since it can't be freed
 *     inside the gc, it's been given an extra ref to stay around
 *     until the free pass. Set in the mark, cycle check or zap weak
 *     passes.
 * 2.  The thing is in a cycle and isn't GC_LIVE. It's given an
 *     extra ref so that the refcount garb doesn't recurse when the
 *     cycle is freed. Set but not cleared in the cycle check pass.
 *     Thus this flag is set on things that was dead but later marked
 *     as live, and they won't have the extra ref in that case (cf
 *     GC_GOT_EXTRA_REF). */
#define GC_FREE_VISITED		0x0080
/* The thing has been visited in the zap weak pass, which uses the
 * mark pass code to remove references to things that got nonweak
 * references but only weak external references. */
#define GC_USER_1		0x0100
#define GC_USER_2		0x0200
#define GC_USER_3		0x0400
/* Flags free for use in the gc callbacks for the specific data types.
 * E.g. multisets use these flags on the multiset_data blocks. */

#define GC_PRETOUCHED		0x4000
/* The thing has been visited by debug_gc_touch() in the pretouch pass. */
#define GC_POSTTOUCHED		0x8000
/* The thing has been visited by debug_gc_touch() in the posttouch pass. */

#ifdef PIKE_DEBUG
#define GC_IS_REFERENCED	0x00040000
/* The thing has been visited by gc_is_referenced() in the mark pass. */
#define GC_XREFERENCED		0x00080000
/* The thing has been visited by gc_mark_external() and thus has a
 * known external reference. Set in the check and locate passes. */
#define GC_DO_FREE		0x00100000
/* The thing has been visited by gc_do_free(). Set in the free pass. */
#define GC_GOT_EXTRA_REF	0x00200000
/* The thing has got an extra reference by the gc. */
#define GC_WEAK_FREED		0x00400000
/* The thing has only weak (external) references left and will be
 * freed. Set in the mark and zap weak passes. */
#define GC_CHECKED_AS_WEAK	0x00800000
/* The thing has been visited by gc_checked_as_weak(). */
#define GC_WATCHED		0x01000000
/* The thing has been set under watch by gc_watch(). */
#define GC_CLEANUP_LEAKED	0x02000000
/* The thing was reported as leaked by the cleanup code. The extra
 * unaccounted refs to it have been/will be freed there too. Thus
 * later frees of this object are logged since the "legitimate" free
 * might still take place so the thing runs out of refs too early. */
#endif

PMOD_EXPORT struct marker *pmod_get_marker (void *p);
PMOD_EXPORT struct marker *pmod_find_marker (void *p);

#define get_marker debug_get_marker
#define find_marker debug_find_marker

#include "block_alloc_h.h"
PTR_HASH_ALLOC_FIXED_FILL_PAGES(marker, n/a);

#undef get_marker
#undef find_marker

#ifdef DYNAMIC_MODULE
#define get_marker(X) ((struct marker *) debug_malloc_pass(pmod_get_marker(X)))
#define find_marker(X) ((struct marker *) debug_malloc_pass(pmod_find_marker(X)))
#else
#define get_marker(X) ((struct marker *) debug_malloc_pass(debug_get_marker(X)))
#define find_marker(X) ((struct marker *) debug_malloc_pass(debug_find_marker(X)))
#endif

extern size_t gc_ext_weak_refs;

typedef void gc_cycle_check_cb (void *data, int weak);

struct gc_frame;
struct callback *debug_add_gc_callback(callback_func call,
				 void *arg,
				 callback_func free_func);
void dump_gc_info(void);
int attempt_to_identify(void *something, void **inblock);
void describe_location(void *real_memblock,
		       int real_type,
		       void *location,
		       int indent,
		       int depth,
		       int flags);
void debug_gc_fatal(void *a, int flags, const char *fmt, ...);
void low_describe_something(void *a,
			    int t,
			    int indent,
			    int depth,
			    int flags,
			    void *inblock);
void describe_something(void *a, int t, int indent, int depth, int flags, void *inblock);
PMOD_EXPORT void describe(void *x);
void debug_describe_svalue(struct svalue *s);
void gc_watch(void *a);
void debug_gc_touch(void *a);
PMOD_EXPORT int real_gc_check(void *a);
PMOD_EXPORT int real_gc_check_weak(void *a);
void exit_gc(void);
void locate_references(void *a);
void debug_gc_add_extra_ref(void *a);
void debug_gc_free_extra_ref(void *a);
int debug_gc_is_referenced(void *a);
int gc_mark_external (void *a, const char *place);
void debug_really_free_gc_frame(struct gc_frame *l);
int gc_do_weak_free(void *a);
void gc_delayed_free(void *a, int type);
void debug_gc_mark_enqueue(queue_call call, void *data);
int gc_mark(void *a);
void gc_move_marker (void *old, void *new);
PMOD_EXPORT void gc_cycle_enqueue(gc_cycle_check_cb *checkfn, void *data, int weak);
void gc_cycle_run_queue(void);
int gc_cycle_push(void *x, struct marker *m, int weak);
void do_gc_recurse_svalues(struct svalue *s, int num);
void do_gc_recurse_short_svalue(union anything *u, int type);
int gc_do_free(void *a);
size_t do_gc(void *ignored, int explicit_call);
void f__gc_status(INT32 args);
void f_count_memory (INT32 args);
void cleanup_gc(void);

#if defined (PIKE_DEBUG) && defined (DEBUG_MALLOC)
#define DMALLOC_TOUCH_MARKER(X, EXPR) (get_marker(X), (EXPR))
#else
#define DMALLOC_TOUCH_MARKER(X, EXPR) (EXPR)
#endif

#define gc_check(VP) \
  DMALLOC_TOUCH_MARKER(VP, real_gc_check(debug_malloc_pass(VP)))
#define gc_check_weak(VP) \
  DMALLOC_TOUCH_MARKER(VP, real_gc_check_weak(debug_malloc_pass(VP)))

/* An object is considered live if its program has the flag
 * PROGRAM_LIVE_OBJ set. That flag gets set if:
 * o  There's a destroy LFUN,
 * o  a program event callback is set with pike_set_prog_event_callback,
 * o  an exit callback is set with set_exit_callback, or
 * o  any inherited program has PROGRAM_LIVE_OBJ set.
 * */
#define gc_object_is_live(OBJ) \
  ((OBJ)->prog && (OBJ)->prog->flags & PROGRAM_LIVE_OBJ)

#ifdef GC_MARK_DEBUG

void gc_mark_enqueue (queue_call fn, void *data);
void gc_mark_run_queue();
void gc_mark_discard_queue();

#else  /* !GC_MARK_DEBUG */

extern struct pike_queue gc_mark_queue;
#define gc_mark_enqueue(FN, DATA) enqueue (&gc_mark_queue, (FN), (DATA))
#define gc_mark_run_queue() run_queue (&gc_mark_queue)
#define gc_mark_discard_queue() discard_queue (&gc_mark_queue)

#endif	/* !GC_MARK_DEBUG */

#if defined (PIKE_DEBUG) || defined (GC_MARK_DEBUG)

extern void *gc_found_in;
extern int gc_found_in_type;
extern const char *gc_found_place;

#define GC_ENTER(THING, TYPE)						\
  do {									\
    void *orig_gc_found_in = gc_found_in;				\
    int orig_gc_found_in_type = gc_found_in_type;			\
    gc_found_in = (THING);						\
    gc_found_in_type = (TYPE);						\
    do

#define GC_LEAVE							\
    while (0);								\
    gc_found_in = orig_gc_found_in;					\
    gc_found_in_type = orig_gc_found_in_type;				\
  } while (0)

static INLINE int debug_gc_check (void *a, const char *place)
{
  int res;
  const char *orig_gc_found_place = gc_found_place;
  gc_found_place = place;
  res = gc_check (a);
  gc_found_place = orig_gc_found_place;
  return res;
}

static INLINE int debug_gc_check_weak (void *a, const char *place)
{
  int res;
  const char *orig_gc_found_place = gc_found_place;
  gc_found_place = place;
  res = gc_check_weak (a);
  gc_found_place = orig_gc_found_place;
  return res;
}

#define debug_gc_check_svalues(S, NUM, PLACE)				\
  do {									\
    const char *orig_gc_found_place = gc_found_place;			\
    gc_found_place = (PLACE);						\
    gc_check_svalues ((S), (NUM));					\
    gc_found_place = orig_gc_found_place;				\
  } while (0)

#define debug_gc_check_weak_svalues(S, NUM, PLACE)			\
  do {									\
    const char *orig_gc_found_place = gc_found_place;			\
    gc_found_place = (PLACE);						\
    gc_check_weak_svalues ((S), (NUM));					\
    gc_found_place = orig_gc_found_place;				\
  } while (0)

#else  /* !defined (PIKE_DEBUG) && !defined (GC_MARK_DEBUG) */

#define GC_ENTER(THING, TYPE) do
#define GC_LEAVE while (0)
#define debug_gc_check(X, PLACE) gc_check (X)
#define debug_gc_check_weak(X, PLACE) gc_check_weak (X)
#define debug_gc_check_svalues(S, NUM, PLACE) gc_check_svalues ((S), (NUM))
#define debug_gc_check_weak_svalues(S, NUM, PLACE) gc_check_weak_svalues ((S), (NUM))

#endif	/* !defined (PIKE_DEBUG) && !defined (GC_MARK_DEBUG) */

#define gc_fatal \
  fprintf(stderr, "%s:%d: GC fatal:\n", __FILE__, __LINE__), debug_gc_fatal

#ifdef PIKE_DEBUG

#define gc_checked_as_weak(X) do {					\
  get_marker(X)->flags |= GC_CHECKED_AS_WEAK;				\
} while (0)
#define gc_assert_checked_as_weak(X) do {				\
  if (!(find_marker(X)->flags & GC_CHECKED_AS_WEAK))			\
    Pike_fatal("A thing was checked as nonweak but "			\
	       "marked or cycle checked as weak.\n");			\
} while (0)
#define gc_assert_checked_as_nonweak(X) do {				\
  if (find_marker(X)->flags & GC_CHECKED_AS_WEAK)			\
    Pike_fatal("A thing was checked as weak but "			\
	       "marked or cycle checked as nonweak.\n");		\
} while (0)

#else  /* !PIKE_DEBUG */

#define gc_checked_as_weak(X) do {} while (0)
#define gc_assert_checked_as_weak(X) do {} while (0)
#define gc_assert_checked_as_nonweak(X) do {} while (0)

#endif	/* !PIKE_DEBUG */

#define gc_recurse_svalues(S,N)						\
  (Pike_in_gc == GC_PASS_CYCLE ?					\
   gc_cycle_check_svalues((S), (N)) : gc_mark_svalues((S), (N)))
#define gc_recurse_short_svalue(U,T)					\
  (Pike_in_gc == GC_PASS_CYCLE ?					\
   gc_cycle_check_short_svalue((U), (T)) : gc_mark_short_svalue((U), (T)))
#define gc_recurse_weak_svalues(S,N)					\
  (Pike_in_gc == GC_PASS_CYCLE ?					\
   gc_cycle_check_weak_svalues((S), (N)) : gc_mark_weak_svalues((S), (N)))
#define gc_recurse_weak_short_svalue(U,T)				\
  (Pike_in_gc == GC_PASS_CYCLE ?					\
   gc_cycle_check_weak_short_svalue((U), (T)) : gc_mark_weak_short_svalue((U), (T)))

#define GC_RECURSE_THING(V, T)						\
  (mc_pass ?								\
   PIKE_CONCAT3 (visit_,T,_ref) (debug_malloc_pass (V), REF_TYPE_NORMAL) : \
   (DMALLOC_TOUCH_MARKER(V, Pike_in_gc == GC_PASS_CYCLE) ?		\
    PIKE_CONCAT(gc_cycle_check_, T)(V, 0) :				\
    PIKE_CONCAT3(gc_mark_, T, _as_referenced)(V)))
#define gc_recurse_array(V) GC_RECURSE_THING((V), array)
#define gc_recurse_mapping(V) GC_RECURSE_THING((V), mapping)
#define gc_recurse_multiset(V) GC_RECURSE_THING((V), multiset)
#define gc_recurse_object(V) GC_RECURSE_THING((V), object)
#define gc_recurse_program(V) GC_RECURSE_THING((V), program)

#ifdef PIKE_DEBUG
#define gc_is_referenced(X) \
  DMALLOC_TOUCH_MARKER(X, debug_gc_is_referenced(debug_malloc_pass(X)))
#else
#define gc_is_referenced(X) !(get_marker(X)->flags & GC_NOT_REFERENCED)
#endif

#define add_gc_callback(X,Y,Z) \
  dmalloc_touch(struct callback *,debug_add_gc_callback((X),(Y),(Z)))

#ifndef PIKE_DEBUG
#define gc_add_extra_ref(X) (++*(INT32 *)(X))
#define gc_free_extra_ref(X)
#else
#define gc_add_extra_ref(X) debug_gc_add_extra_ref(debug_malloc_pass(X))
#define gc_free_extra_ref(X) debug_gc_free_extra_ref(debug_malloc_pass(X))
#endif

#define GC_PASS_PREPARE		 50
#define GC_PASS_PRETOUCH	 90
#define GC_PASS_CHECK		100
#define GC_PASS_MARK		200
#define GC_PASS_CYCLE		250
#define GC_PASS_ZAP_WEAK	260
#define GC_PASS_POSTTOUCH	270
#define GC_PASS_FREE		300
#define GC_PASS_KILL		400
#define GC_PASS_DESTRUCT	500

#define GC_PASS_LOCATE -1
#define GC_PASS_DISABLED -2

#ifdef PIKE_DEBUG
extern int gc_in_cycle_check;
#endif

/* Use WEAK < 0 for strong links. The gc makes these assumptions about
 * them:
 *
 * 1.  All strong links are recursed before any other links, i.e.
 *     strong links must be passed to gc_cycle_check_* last. This also
 *     means that if following a strong link causes nonstrong links to
 *     be followed recursively then there can only be one strong link
 *     in a thing.
 * 2.  There can never be a cycle consisting of only strong links.
 *
 * Warning: This is unusable other than internally; you'll never be
 * able to guarantee the second assumption since strong links are used
 * internally for parent object and program relations. */

#define GC_CYCLE_ENTER(X, TYPE, WEAK) do {				\
  void *_thing_ = (X);							\
  struct marker *_m_ = get_marker(_thing_);				\
  if (!(_m_->flags & GC_MARKED)) {					\
    DO_IF_DEBUG(							\
      if (gc_in_cycle_check)						\
	Pike_fatal("Recursing immediately in gc cycle check.\n");	\
      gc_in_cycle_check = 1;						\
    );									\
    if (gc_cycle_push(_thing_, _m_, (WEAK))) {

#define GC_CYCLE_ENTER_OBJECT(X, WEAK) do {				\
  struct object *_thing_ = (X);						\
  struct marker *_m_ = get_marker(_thing_);				\
  if (!(_m_->flags & GC_MARKED)) {					\
    if (gc_object_is_live (_thing_))					\
      _m_->flags |= GC_LIVE|GC_LIVE_OBJ;				\
    DO_IF_DEBUG(							\
      if (gc_in_cycle_check)						\
	Pike_fatal("Recursing immediately in gc cycle check.\n");	\
      gc_in_cycle_check = 1;						\
    );									\
    if (gc_cycle_push(_thing_, _m_, (WEAK))) {

#define GC_CYCLE_LEAVE							\
    }									\
    DO_IF_DEBUG(gc_in_cycle_check = 0);					\
  }									\
} while (0)

/* Generic API to follow refs in arbitrary structures (experimental).
 *
 * The check/mark/cycle check callbacks in the gc might be converted
 * to this. Global state is fair play by design; the intention is to
 * use thread local storage when the time comes. */

/* A visit_thing_fn is made for every type of refcounted block. An
 * INT32 refcounter is assumed to be first in every block. The
 * visit_thing_fn should call visit_ref exactly once for every
 * refcounted ref inside src_thing, in a stable order. dst_thing is
 * then the target of the ref, ref_type describes the type of the ref
 * itself (REF_TYPE_*), visit_dst is the visit_thing_fn for the target
 * block, and extra is an arbitrary value passed along to visit_dst.
 *
 * action identifies some action that the visit_thing_fn should take
 * (VISIT_*). Also, visit_ref_cb is likely to get a return value that
 * identifies an action that should be taken on the ref.
 *
 * visit_thing_fn's must be reentrant. visit_dst might be called
 * immediately, queued and called later, or not called at all. */

typedef void visit_thing_fn (void *src_thing, int action, void *extra);
typedef void visit_ref_cb (void *dst_thing, int ref_type,
			   visit_thing_fn *visit_dst, void *extra);
PMOD_EXPORT extern visit_ref_cb *visit_ref;

#define REF_TYPE_STRENGTH 0x03	/* Bits for normal/weak/strong. */
#define REF_TYPE_NORMAL	0x00	/* Normal (nonweak and nonstrong) ref. */
#define REF_TYPE_WEAK	0x01	/* Weak ref. */
#define REF_TYPE_STRONG	0x02	/* Strong ref. Note restrictions above. */

#define REF_TYPE_INTERNAL 0x04	/* "Internal" ref. */
/* An "internal" ref is one that should be considered internal within
 * a structure that is treated as one unit on the pike level. E.g. the
 * refs from mappings and multisets to their data blocks are internal.
 * In general, if the target block isn't one of the eight referenced
 * pike types then the ref is internal. Internal refs must not make up
 * a cycle. They don't count towards the lookahead distance in
 * f_count_memory. */

#define VISIT_NORMAL 0
/* Alias for zero which indicates normal (no) action: Visit all refs
 * to all (refcounted) blocks and do nothing else. */

#define VISIT_COMPLEX_ONLY 0x01
/* Bit flag. If this is set, only follow refs to blocks that might
 * contain more refs. */

#define VISIT_COUNT_BYTES 0x02
/* Add the number of bytes allocated for the block to
 * mc_counted_bytes, then visit the refs. Never combined with
 * VISIT_COMPLEX_ONLY. */

/* Map between type and visit function for the standard ref types. */
PMOD_EXPORT extern visit_thing_fn *const visit_fn_from_type[MAX_REF_TYPE + 1];
PMOD_EXPORT TYPE_T type_from_visit_fn (visit_thing_fn *fn);

static INLINE void real_visit_short_svalue (const union anything *u, TYPE_T t,
					    int ref_type)
{
  check_short_svalue (u, t);
  if (t <= MAX_REF_TYPE)
    visit_ref (u->ptr, ref_type, visit_fn_from_type[t], NULL);
}
#define visit_short_svalue(U, T, REF_TYPE) \
  (real_visit_short_svalue (debug_malloc_pass ((U)->ptr), (T), (REF_TYPE)))

#ifdef DEBUG_MALLOC
static INLINE void dmalloc_visit_svalue (struct svalue *s,
					 int ref_type, char *l)
{
  int t = s->type;
  check_svalue (s);
  dmalloc_check_svalue (s, l);
  if (t <= MAX_REF_TYPE) {
    if (t == PIKE_T_FUNCTION) visit_function (s, ref_type);
    else visit_ref (s->u.ptr, ref_type, visit_fn_from_type[t], NULL);
  }
}
#define visit_svalue(S, REF_TYPE) \
  dmalloc_visit_svalue ((S), (REF_TYPE), DMALLOC_LOCATION())
#else
static INLINE void visit_svalue (struct svalue *s, int ref_type)
{
  int t = s->type;
  check_svalue (s);
  if (t <= MAX_REF_TYPE) {
    if (t == PIKE_T_FUNCTION) visit_function (s, ref_type);
    else visit_ref (s->u.ptr, ref_type, visit_fn_from_type[t], NULL);
  }
}
#endif

/* Memory counting */

PMOD_EXPORT extern int mc_pass;
PMOD_EXPORT extern size_t mc_counted_bytes;
PMOD_EXPORT int mc_count_bytes (void *thing);

#endif
