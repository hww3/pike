/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
#include "global.h"
RCSID("$Id: object.c,v 1.16 1997/03/17 03:04:42 hubbe Exp $");
#include "object.h"
#include "dynamic_buffer.h"
#include "interpret.h"
#include "program.h"
#include "stralloc.h"
#include "svalue.h"
#include "pike_macros.h"
#include "memory.h"
#include "error.h"
#include "main.h"
#include "array.h"
#include "gc.h"
#include "backend.h"
#include "callback.h"

struct object *master_object = 0;
struct program *master_program =0;
struct object *first_object;

struct object fake_object = { 1 }; /* start with one reference */

void setup_fake_object()
{
  fake_object.prog=&fake_program;
  fake_object.next=0;
  fake_object.refs=0xffffff;
}

struct object *low_clone(struct program *p)
{
  int e;
  struct object *o;
  struct frame frame;

  GC_ALLOC();

  o=(struct object *)xalloc(sizeof(struct object)-1+p->storage_needed);

  o->prog=p;
  p->refs++;
  o->next=first_object;
  o->prev=0;
  if(first_object)
    first_object->prev=o;
  first_object=o;
  o->refs=1;

  frame.parent_frame=fp;
  frame.current_object=o;
  frame.locals=0;
  frame.fun=-1;
  frame.pc=0;
  fp= & frame;

  frame.current_object->refs++;

  /* clear globals and call C initializers */
  for(e=p->num_inherits-1; e>=0; e--)
  {
    int d;

    frame.context=p->inherits[e];
    frame.context.prog->refs++;
    frame.current_storage=o->storage+frame.context.storage_offset;

    for(d=0;d<(int)frame.context.prog->num_identifiers;d++)
    {
      if(!IDENTIFIER_IS_VARIABLE(frame.context.prog->identifiers[d].flags))
	continue;
      
      if(frame.context.prog->identifiers[d].run_time_type == T_MIXED)
      {
	struct svalue *s;
	s=(struct svalue *)(frame.current_storage +
			    frame.context.prog->identifiers[d].func.offset);
	s->type=T_INT;
	s->u.integer=0;
	s->subtype=0;
      }else{
	union anything *u;
	u=(union anything *)(frame.current_storage +
			     frame.context.prog->identifiers[d].func.offset);
	MEMSET((char *)u,0,sizeof(*u));
      }
    }

    if(frame.context.prog->init)
      frame.context.prog->init(o);

    free_program(frame.context.prog);
  }

  free_object(frame.current_object);
  fp = frame.parent_frame;

  return o;
}

static void init_object(struct object *o, int args)
{
  apply_lfun(o,LFUN___INIT,0);
  pop_stack();
  apply_lfun(o,LFUN_CREATE,args);
  pop_stack();
}

struct object *clone_object(struct program *p, int args)
{
  struct object *o=low_clone(p);
  init_object(o,args);
  return o;
}

struct object *get_master()
{
  extern char *master_file;
  struct pike_string *master_name;
  static int inside=0;

  if(master_object && master_object->prog)
    return master_object;

  if(inside) return 0;

  if(master_object)
  {
    free_object(master_object);
    master_object=0;
  }

  inside = 1;

  if(!master_program)
  {
    master_name=make_shared_string(master_file);
    master_program=compile_file(master_name);
    free_string(master_name);
    if(!master_program) return 0;
  }
  master_object=clone_object(master_program,0);

  apply_lfun(master_object,LFUN___INIT,0);
  pop_stack();
  apply_lfun(master_object,LFUN_CREATE,0);
  pop_stack();
  
  inside = 0;
  return master_object;
}

struct object *master()
{
  struct object *o;
  o=get_master();
  if(!o) fatal("Couldn't load master object.\n");
  return o;
}

void destruct(struct object *o)
{
  int e;
  struct frame frame;
  struct program *p;

#ifdef DEBUG
  if(d_flag > 20) do_debug();
#endif

  if(!o || !(p=o->prog)) return; /* Object already destructed */

  o->refs++;

  if(o->prog->lfuns[LFUN_DESTROY] != -1)
  {
    safe_apply_low(o, o->prog->lfuns[LFUN_DESTROY], 0);
    pop_stack();
  }

  /* destructed in destroy() */
  if(!o->prog)
  {
    free_object(o);
    return;
  }

  o->prog=0;

  frame.parent_frame=fp;
  frame.current_object=o;  /* refs already updated */
  frame.locals=0;
  frame.fun=-1;
  frame.pc=0;
  fp= & frame;

  /* free globals and call C de-initializers */
  for(e=p->num_inherits-1; e>=0; e--)
  {
    int d;

    frame.context=p->inherits[e];
    frame.context.prog->refs++;
    frame.current_storage=o->storage+frame.context.storage_offset;

    if(frame.context.prog->exit)
      frame.context.prog->exit(o);

    for(d=0;d<(int)frame.context.prog->num_identifiers;d++)
    {
      if(!IDENTIFIER_IS_VARIABLE(frame.context.prog->identifiers[d].flags)) 
	continue;
      
      if(frame.context.prog->identifiers[d].run_time_type == T_MIXED)
      {
	struct svalue *s;
	s=(struct svalue *)(frame.current_storage +
			    frame.context.prog->identifiers[d].func.offset);
	free_svalue(s);
      }else{
	union anything *u;
	u=(union anything *)(frame.current_storage +
			     frame.context.prog->identifiers[d].func.offset);
	free_short_svalue(u, frame.context.prog->identifiers[d].run_time_type);
      }
    }
    free_program(frame.context.prog);
  }

  free_object(frame.current_object);
  fp = frame.parent_frame;

  free_program(p);
}


static struct object *objects_to_destruct = 0;
static struct callback *destruct_object_evaluator_callback =0;

/* This function destructs the objects that are scheduled to be
 * destructed by really_free_object. It links the object back into the
 * list of objects first. Adds a reference, destructs it and then frees it.
 */
void destruct_objects_to_destruct()
{
  struct object *o, *next;

  while(o=objects_to_destruct)
  {
    /* Link object back to list of objects */
    objects_to_destruct=o->next;
    
    if(first_object)
      first_object->prev=o;

    o->next=first_object;
    first_object=o;
    o->prev=0;

    o->refs++; /* Don't free me now! */

    destruct(o);

    free_object(o);
  }
  objects_to_destruct=0;
  if(destruct_object_evaluator_callback)
  {
    remove_callback(destruct_object_evaluator_callback);
    destruct_object_evaluator_callback=0;
  }
}


/* really_free_objects:
 * This function is called when an object runs out of references.
 * It frees the object if it is destructed, otherwise it moves it to
 * a separate list of objects which will be destructed later.
 */

void really_free_object(struct object *o)
{
  if(o->prev)
    o->prev->next=o->next;
  else
    first_object=o->next;

  if(o->next) o->next->prev=o->prev;

  if(o->prog)
  {
    if(!objects_to_destruct)
    {
      destruct_object_evaluator_callback=
	add_to_callback(&evaluator_callbacks,
			(callback_func)destruct_objects_to_destruct,
			0,0);
    }
    o->next=objects_to_destruct;
    o->prev=0;
    objects_to_destruct=o;
  } else {
    free((char *)o);
    GC_FREE();
  }
}


void low_object_index_no_free(struct svalue *to,
			      struct object *o,
			      INT32 f)
{
  struct identifier *i;
  struct program *p=o->prog;
  
  if(!p)
    error("Cannot access global variables in destructed object.\n");

  i=ID_FROM_INT(p, f);

  switch(i->flags & (IDENTIFIER_FUNCTION | IDENTIFIER_CONSTANT))
  {
  case IDENTIFIER_FUNCTION:
  case IDENTIFIER_C_FUNCTION:
  case IDENTIFIER_PIKE_FUNCTION:
    to->type=T_FUNCTION;
    to->subtype=f;
    to->u.object=o;
    o->refs++;
    break;

  case IDENTIFIER_CONSTANT:
    {
      struct svalue *s;
      s=PROG_FROM_INT(p,f)->constants + i->func.offset;
      check_destructed(s);
      assign_svalue_no_free(to, s);
      break;
    }

  case 0:
    if(i->run_time_type == T_MIXED)
    {
      struct svalue *s;
      s=(struct svalue *)LOW_GET_GLOBAL(o,f,i);
      check_destructed(s);
      assign_svalue_no_free(to, s);
    }
    else
    {
      union anything *u;
      u=(union anything *)LOW_GET_GLOBAL(o,f,i);
      check_short_destructed(u,i->run_time_type);
      assign_from_short_svalue_no_free(to, u, i->run_time_type);
    }
  }
}

void object_index_no_free2(struct svalue *to,
			  struct object *o,
			  struct svalue *index)
{
  struct program *p;
  int f;

  if(!o || !(p=o->prog))
  {
    error("Lookup in destructed object.\n");
    return; /* make gcc happy */
  }

  if(index->type != T_STRING)
    error("Lookup on non-string value.\n");

  f=find_shared_string_identifier(index->u.string, p);
  if(f < 0)
  {
    to->type=T_INT;
    to->subtype=NUMBER_UNDEFINED;
    to->u.integer=0;
  }else{
    low_object_index_no_free(to, o, f);
  }
}

#define ARROW_INDEX_P(X) ((X)->type==T_STRING && (X)->subtype)

void object_index_no_free(struct svalue *to,
			   struct object *o,
			   struct svalue *index)
{
  struct program *p;
  int lfun;

  if(!o || !(p=o->prog))
  {
    error("Lookup in destructed object.\n");
    return; /* make gcc happy */
  }
  lfun=ARROW_INDEX_P(index) ? LFUN_ARROW : LFUN_INDEX;

  if(p->lfuns[lfun] != -1)
  {
    push_svalue(index);
    apply_lfun(o,lfun,1);
    to=sp;
    sp--;
  } else {
    object_index_no_free2(to,o,index);
  }
}


static void object_low_set_index(struct object *o,
				 int f,
				 struct svalue *from)
{
  struct identifier *i;
  struct program *p;

  if(!o || !(p=o->prog))
  {
    error("Lookup in destructed object.\n");
    return; /* make gcc happy */
  }

  check_destructed(from);

  i=ID_FROM_INT(p, f);

  if(!IDENTIFIER_IS_VARIABLE(i->flags))
  {
    error("Cannot assign functions or constants.\n");
  }
  else if(i->run_time_type == T_MIXED)
  {
    assign_svalue((struct svalue *)LOW_GET_GLOBAL(o,f,i),from);
  }
  else
  {
    assign_to_short_svalue((union anything *) 
			   LOW_GET_GLOBAL(o,f,i),
			   i->run_time_type,
			   from);
  }
}

void object_set_index2(struct object *o,
		      struct svalue *index,
		      struct svalue *from)
{
  struct program *p;
  int f;

  if(!o || !(p=o->prog))
  {
    error("Lookup in destructed object.\n");
    return; /* make gcc happy */
  }

  if(index->type != T_STRING)
    error("Lookup on non-string value.\n");

  f=find_shared_string_identifier(index->u.string, p);
  if(f < 0)
  {
    error("No such variable (%s) in object.\n", index->u.string->str);
  }else{
    object_low_set_index(o, f, from);
  }
}

void object_set_index(struct object *o,
		       struct svalue *index,
		       struct svalue *from)
{
  struct program *p;
  int lfun;

  if(!o || !(p=o->prog))
  {
    error("Lookup in destructed object.\n");
    return; /* make gcc happy */
  }

  lfun=ARROW_INDEX_P(index) ? LFUN_ASSIGN_ARROW : LFUN_ASSIGN_INDEX;

  if(p->lfuns[lfun] != -1)
  {
    push_svalue(index);
    push_svalue(from);
    apply_lfun(o,lfun,2);
    pop_stack();
  } else {
    object_set_index2(o,index,from);
  }
}

static union anything *object_low_get_item_ptr(struct object *o,
					       int f,
					       TYPE_T type)
{
  struct identifier *i;
  struct program *p;

  if(!o || !(p=o->prog))
  {
    error("Lookup in destructed object.\n");
    return 0; /* make gcc happy */
  }

  i=ID_FROM_INT(p, f);

  if(!IDENTIFIER_IS_VARIABLE(i->flags))
  {
    error("Cannot assign functions or constants.\n");
  }
  else if(i->run_time_type == T_MIXED)
  {
    struct svalue *s;
    s=(struct svalue *)LOW_GET_GLOBAL(o,f,i);
    if(s->type == type) return & s->u;
  }
  else if(i->run_time_type == type)
  {
    return (union anything *) LOW_GET_GLOBAL(o,f,i);
  }
  return 0;
}


union anything *object_get_item_ptr(struct object *o,
				    struct svalue *index,
				    TYPE_T type)
{

  struct program *p;
  int f;

  if(!o || !(p=o->prog))
  {
    error("Lookup in destructed object.\n");
    return 0; /* make gcc happy */
  }

  f=ARROW_INDEX_P(index) ? LFUN_ASSIGN_ARROW : LFUN_ASSIGN_INDEX;
  if(p->lfuns[f] != -1)
    error("Cannot do incremental operations on overloaded index (yet).\n");

  if(index->type != T_STRING)
    error("Lookup on non-string value.\n");

  f=find_shared_string_identifier(index->u.string, p);
  if(f < 0)
  {
    error("No such variable in object.\n");
  }else{
    return object_low_get_item_ptr(o, f, type);
  }
  return 0;
}

#ifdef DEBUG
void verify_all_objects()
{
  struct object *o;
  struct frame frame;

  for(o=first_object;o;o=o->next)
  {
    if(o->next && o->next->prev !=o)
      fatal("Object check: o->next->prev != o\n");

    if(o->prev)
    {
      if(o->prev->next != o)
	fatal("Object check: o->prev->next != o\n");

      if(o == first_object)
	fatal("Object check: o->prev !=0 && first_object == o\n");
    } else {
      if(first_object != o)
	fatal("Object check: o->prev ==0 && first_object != o\n");
    }

    if(o->refs <= 0)
      fatal("Object refs <= zero.\n");

    if(o->prog)
    {
      extern struct program *first_program;
      struct program *p;
      int e;

      for(p=first_program;p!=o->prog;p=p->next)
	if(!p)
	  fatal("Object's program not in program list.\n");

      for(e=0;e<(int)o->prog->num_identifiers;e++)
      {
	struct identifier *i;
	i=ID_FROM_INT(o->prog, e);
	if(!IDENTIFIER_IS_VARIABLE(i->flags))
	  continue;

	if(i->run_time_type == T_MIXED)
	{
	  check_svalue((struct svalue *)LOW_GET_GLOBAL(o,e,i));
	}else{
	  check_short_svalue((union anything *)LOW_GET_GLOBAL(o,e,i),
			     i->run_time_type);
	}
      }

      frame.parent_frame=fp;
      frame.current_object=o;
      frame.locals=0;
      frame.fun=-1;
      frame.pc=0;
      fp= & frame;

      frame.current_object->refs++;

      for(e=0;e<(int)o->prog->num_inherits;e++)
      {
	frame.context=o->prog->inherits[e];
	frame.context.prog->refs++;
	frame.current_storage=o->storage+frame.context.storage_offset;
      }

      free_object(frame.current_object);
      fp = frame.parent_frame;
    }
  }
}
#endif

int object_equal_p(struct object *a, struct object *b, struct processing *p)
{
  struct processing curr;

  if(a == b) return 1;
  if(a->prog != b->prog) return 0;

  curr.pointer_a = a;
  curr.pointer_b = b;
  curr.next = p;

  for( ;p ;p=p->next)
    if(p->pointer_a == (void *)a && p->pointer_b == (void *)b)
      return 1;


  if(a->prog)
  {
    int e;
    for(e=0;e<(int)a->prog->num_identifiers;e++)
    {
      struct identifier *i;
      i=ID_FROM_INT(a->prog, e);
      if(!IDENTIFIER_IS_VARIABLE(i->flags))
	continue;

      if(i->run_time_type == T_MIXED)
      {
	if(!low_is_equal((struct svalue *)LOW_GET_GLOBAL(a,e,i),
			 (struct svalue *)LOW_GET_GLOBAL(b,e,i),
			 &curr))
	  return 0;
      }else{
	if(!low_short_is_equal((union anything *)LOW_GET_GLOBAL(a,e,i),
			       (union anything *)LOW_GET_GLOBAL(b,e,i),
			       i->run_time_type,
			       &curr))
	  return 0;
      }
    }
  }

  return 1;
}

void cleanup_objects()
{
  struct object *o, *next;
  for(o=first_object;o;o=next)
  {
    o->refs++;
    destruct(o);
    next=o->next;
    free_object(o);
  }
  destruct_objects_to_destruct();

  free_object(master_object);
  master_object=0;
  free_program(master_program);
  master_program=0;
}

struct array *object_indices(struct object *o)
{
  struct program *p;
  struct array *a;
  int e;

  p=o->prog;
  if(!p)
    error("indices() on destructed object.\n");

  if(p->lfuns[LFUN__INDICES]==-1)
  {
    a=allocate_array_no_init(p->num_identifier_indexes,0);
    for(e=0;e<(int)p->num_identifier_indexes;e++)
    {
      copy_shared_string(ITEM(a)[e].u.string,
			 ID_FROM_INT(p,p->identifier_index[e])->name);
      ITEM(a)[e].type=T_STRING;
    }
  }else{
    apply_lfun(o, LFUN__INDICES, 0);
    if(sp[-1].type != T_ARRAY)
      error("Bad return type from o->_indices()\n");
    a=sp[-1].u.array;
    sp--;
  }
  return a;
}

struct array *object_values(struct object *o)
{
  struct program *p;
  struct array *a;
  int e;
  
  p=o->prog;
  if(!p)
    error("values() on destructed object.\n");

  if(p->lfuns[LFUN__INDICES]==-1)
  {
    a=allocate_array_no_init(p->num_identifier_indexes,0);
    for(e=0;e<(int)p->num_identifier_indexes;e++)
    {
      low_object_index_no_free(ITEM(a)+e, o, p->identifier_index[e]);
    }
  }else{
    apply_lfun(o, LFUN__VALUES, 0);
    if(sp[-1].type != T_ARRAY)
      error("Bad return type from o->_values()\n");
    a=sp[-1].u.array;
    sp--;
  }
  return a;
}

#ifdef GC2


void gc_mark_object_as_referenced(struct object *o)
{
  if(gc_mark(o))
  {
    if(o->prog)
    {
      INT32 e;
      
      for(e=0;e<(int)o->prog->num_identifier_indexes;e++)
      {
	struct identifier *i;
	
	i=ID_FROM_INT(o->prog, e);
	
	if(!IDENTIFIER_IS_VARIABLE(i->flags)) continue;
	
	if(i->run_time_type == T_MIXED)
	{
	  gc_mark_svalues((struct svalue *)LOW_GET_GLOBAL(o,e,i),1);
	}else{
	  gc_mark_short_svalue((union anything *)LOW_GET_GLOBAL(o,e,i),
			       i->run_time_type);
	}
      }
    }
  }
}

void gc_check_all_objects()
{
  struct object *o;
  for(o=first_object;o;o=o->next)
  {
    if(o->prog)
    {
      INT32 e;

      for(e=0;e<(int)o->prog->num_identifier_indexes;e++)
      {
	struct identifier *i;
	
	i=ID_FROM_INT(o->prog, e);
	
	if(!IDENTIFIER_IS_VARIABLE(i->flags)) continue;
	
	if(i->run_time_type == T_MIXED)
	{
	  gc_check_svalues((struct svalue *)LOW_GET_GLOBAL(o,e,i),1);
	}else{
	  gc_check_short_svalue((union anything *)LOW_GET_GLOBAL(o,e,i),
				i->run_time_type);
	}
      }
    }
  }
}

void gc_mark_all_objects()
{
  struct object *o;
  for(o=first_object;o;o=o->next)
    if(gc_is_referenced(o))
      gc_mark_object_as_referenced(o);
}

void gc_free_all_unreferenced_objects()
{
  struct object *o,*next;

  for(o=first_object;o;o=next)
  {
    if(gc_do_free(o))
    {
      o->refs++;
      destruct(o);
      next=o->next;
      free_object(o);
    }else{
      next=o->next;
    }
  }
}

#endif /* GC2 */

void count_memory_in_objects(INT32 *num_, INT32 *size_)
{
  INT32 num=0, size=0;
  struct object *o;
  for(o=first_object;o;o=o->next)
  {
    num++;
    if(o->prog)
    {
      size+=sizeof(struct object)-1+o->prog->storage_needed;
    }else{
      size+=sizeof(struct object);
    }
  }
  for(o=objects_to_destruct;o;o=o->next)
  {
    num++;
    if(o->prog)
    {
      size+=sizeof(struct object)-1+o->prog->storage_needed;
    }else{
      size+=sizeof(struct object);
    }
  }
  *num_=num;
  *size_=size;
}
