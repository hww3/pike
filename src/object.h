/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/

/*
 * $Id: object.h,v 1.47 2000/06/23 06:17:58 hubbe Exp $
 */
#ifndef OBJECT_H
#define OBJECT_H

#include "global.h"
#include "svalue.h"

/* a destructed object has no program */

#ifndef STRUCT_OBJECT_DECLARED
#define STRUCT_OBJECT_DECLARED
#endif
struct object
{
  INT32 refs;                    /* Reference count, must be first. */
#ifdef PIKE_SECURITY
  struct object *prot;
#endif
  struct program *prog;
  struct object *parent;
  INT16 parent_identifier;
  struct object *next;
  struct object *prev;
#if PIKE_DEBUG
  long program_id;
#endif
  char *storage;
};

extern struct object *first_object;
extern struct object *gc_internal_object;
extern struct object *objects_to_destruct;
extern struct object *master_object;
extern struct program *master_program;
extern struct program *magic_index_program;
extern struct program *magic_set_index_program;

#define free_object(O) do{ struct object *o_=(O); debug_malloc_touch(o_); debug_malloc_touch(o_->storage); if(!--o_->refs) schedule_really_free_object(o_); }while(0)

#define LOW_GET_GLOBAL(O,I,ID) ((O)->storage+INHERIT_FROM_INT((O)->prog, (I))->storage_offset+(ID)->func.offset)
#define GET_GLOBAL(O,I) LOW_GET_GLOBAL(O,I,ID_FROM_INT((O)->prog,I))
#define GLOBAL_FROM_INT(I) GET_GLOBAL(Pike_fp->current_object, I)

#define this_object() (add_ref(Pike_fp->current_object), Pike_fp->current_object)

#include "block_alloc_h.h"
/* Prototypes begin here */
struct object *low_clone(struct program *p);
void call_c_initializers(struct object *o);
void do_free_object(struct object *o);
struct object *debug_clone_object(struct program *p, int args);
struct object *fast_clone_object(struct program *p, int args);
struct object *parent_clone_object(struct program *p,
				   struct object *parent,
				   int parent_identifier,
				   int args);
struct object *get_master(void);
struct object *debug_master(void);
struct destroy_called_mark;
PTR_HASH_ALLOC(destroy_called_mark,128)
void low_destruct(struct object *o,int do_free);
void destruct(struct object *o);
void destruct_objects_to_destruct(void);
void schedule_really_free_object(struct object *o);
void low_object_index_no_free(struct svalue *to,
			      struct object *o,
			      INT32 f);
void object_index_no_free2(struct svalue *to,
			  struct object *o,
			  struct svalue *index);
void object_index_no_free(struct svalue *to,
			   struct object *o,
			   struct svalue *index);
void object_low_set_index(struct object *o,
			  int f,
			  struct svalue *from);
void object_set_index2(struct object *o,
		      struct svalue *index,
		      struct svalue *from);
void object_set_index(struct object *o,
		       struct svalue *index,
		       struct svalue *from);
union anything *object_get_item_ptr(struct object *o,
				    struct svalue *index,
				    TYPE_T type);
int object_equal_p(struct object *a, struct object *b, struct processing *p);
void cleanup_objects(void);
struct array *object_indices(struct object *o);
struct array *object_values(struct object *o);
void gc_mark_object_as_referenced(struct object *o);
unsigned gc_touch_all_objects(void);
void gc_check_all_objects(void);
void gc_mark_all_objects(void);
void real_gc_cycle_check_object(struct object *o);
void real_gc_cycle_check_object_weak(struct object *o);
void real_gc_cycle_check_object_strong(struct object *o);
void gc_cycle_check_all_objects(void);
void gc_free_all_unreferenced_objects(void);
void count_memory_in_objects(INT32 *num_, INT32 *size_);
struct magic_index_struct;
void push_magic_index(struct program *type, int inherit_no, int parent_level);
void init_object(void);
void exit_object(void);
void check_object_context(struct object *o,
			  struct program *context_prog,
			  char *current_storage);
void check_object(struct object *o);
void check_all_objects(void);
/* Prototypes end here */

#ifdef DEBUG_MALLOC
#define clone_object(X,Y) ((struct object *)debug_malloc_pass(debug_clone_object((X),(Y))))
#else
#define clone_object debug_clone_object
#endif

#ifdef PIKE_DEBUG
#define master() ( get_master() ? get_master() : ( fatal("Couldn't load master object at %s:%d.\n",__FILE__,__LINE__), (struct object *)0) )
#else
#define master() debug_master()
#endif

#define gc_cycle_check_object(X) \
  enqueue_lifo(&gc_mark_queue, (queue_call) real_gc_cycle_check_object, (X))
#define gc_cycle_check_object_weak(X) \
  enqueue_lifo(&gc_mark_queue, (queue_call) real_gc_cycle_check_object_weak, (X))
#define gc_cycle_check_object_strong(X) \
  enqueue_lifo(&gc_mark_queue, (queue_call) real_gc_cycle_check_object_strong, (X))

#endif /* OBJECT_H */

