/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
#include "global.h"
#include "svalue.h"
#include "array.h"
#include "object.h"
#include "las.h"
#include "stralloc.h"
#include "interpret.h"
#include "language.h"
#include "error.h"
#include "pike_types.h"
#include "fsort.h"
#include "builtin_functions.h"
#include "pike_memory.h"
#include "gc.h"
#include "main.h"
#include "security.h"

RCSID("$Id: array.c,v 1.47 1999/04/12 05:27:46 hubbe Exp $");

struct array empty_array=
{
  1,                     /* Never free */
#ifdef PIKE_SECURITY
  0,
#endif
  &empty_array,          /* Next */
  &empty_array,          /* previous (circular) */
  0,                     /* Size = 0 */
  0,                     /* malloced Size = 0 */
  0,                     /* no types */
  0,			 /* no flags */
};



/* Allocate an array, this might be changed in the future to
 * allocate linked lists or something
 * NOTE: the new array have zero references
 */

struct array *low_allocate_array(INT32 size,INT32 extra_space)
{
  struct array *v;
  INT32 e;

  if(size == 0)
  {
    add_ref(&empty_array);
    return &empty_array;
  }

  GC_ALLOC();

  v=(struct array *)malloc(sizeof(struct array)+
			   (size+extra_space-1)*sizeof(struct svalue));
  if(!v)
    error("Couldn't allocate array, out of memory.\n");
  

  /* for now, we don't know what will go in here */
  v->type_field=BIT_MIXED | BIT_UNFINISHED;
  v->flags=0;

  v->malloced_size=size+extra_space;
  v->size=size;
  v->refs=1;
  v->prev=&empty_array;
  v->next=empty_array.next;
  empty_array.next=v;
  v->next->prev=v;

  INITIALIZE_PROT(v);

  for(e=0;e<v->size;e++)
  {
    ITEM(v)[e].type=T_INT;
    ITEM(v)[e].subtype=NUMBER_NUMBER;
    ITEM(v)[e].u.integer=0;
  }

  return v;
}

/*
 * Free an array without freeing the values inside it
 */
static void array_free_no_free(struct array *v)
{
  struct array *next,*prev;

  next = v->next;
  prev = v->prev;

  v->prev->next=next;
  v->next->prev=prev;

  free((char *)v);

  GC_FREE();
}

/*
 * Free an array, call this when the array has zero references
 */
void really_free_array(struct array *v)
{
#ifdef PIKE_DEBUG
  if(v == & empty_array)
    fatal("Tried to free the empty_array.\n");
#endif

#ifdef PIKE_DEBUG
  if(d_flag > 1)  array_check_type_field(v);
#endif

  add_ref(v);
  FREE_PROT(v);
  free_svalues(ITEM(v), v->size, v->type_field);
  v->refs--;
  array_free_no_free(v);
}

void do_free_array(struct array *a)
{
  free_array(a);
}

/*
 * Extract an svalue from an array
 */
void array_index_no_free(struct svalue *s,struct array *v,INT32 index)
{
#ifdef PIKE_DEBUG
  if(index<0 || index>=v->size)
    fatal("Illegal index in low level index routine.\n");
#endif

  assign_svalue_no_free(s, ITEM(v) + index);
}

/*
 * Extract an svalue from an array
 */
void array_index(struct svalue *s,struct array *v,INT32 index)
{
#ifdef PIKE_DEBUG
  if(index<0 || index>=v->size)
    fatal("Illegal index in low level index routine.\n");
#endif

  add_ref(v);
  assign_svalue(s, ITEM(v) + index);
  free_array(v);
}

void simple_array_index_no_free(struct svalue *s,
				struct array *a,struct svalue *ind)
{
  INT32 i;
  switch(ind->type)
  {
    case T_INT:
      i=ind->u.integer;
      if(i<0) i+=a->size;
      if(i<0 || i>=a->size) {
	struct svalue tmp;
	tmp.type=T_ARRAY;
	tmp.u.array=a;
	if (a->size) {
	  index_error(0,0,0,&tmp,ind,"Index %d is out of range 0 - %d.\n", i, a->size-1);
	} else {
	  index_error(0,0,0,&tmp,ind,"Attempt to index the empty array with %d.\n", i);
	}
      }
      array_index_no_free(s,a,i);
      break;

    case T_STRING:
      check_stack(4);
      ref_push_array(a);
      assign_svalue_no_free(sp++,ind);
      f_column(2);
      s[0]=sp[-1];
      sp--;
      break;
	
    default:
      {
	struct svalue tmp;
	tmp.type=T_ARRAY;
	tmp.u.array=a;
	index_error(0,0,0,&tmp,ind,"Index is not an integer.\n");
      }
  }
}

/*
 * Extract an svalue from an array
 */
void array_free_index(struct array *v,INT32 index)
{
#ifdef PIKE_DEBUG
  if(index<0 || index>=v->size)
    fatal("Illegal index in low level free index routine.\n");
#endif

  free_svalue(ITEM(v) + index);
}

/*
 * Set an index in an array
 */
void array_set_index(struct array *v,INT32 index, struct svalue *s)
{
#ifdef PIKE_DEBUG
  if(index<0 || index>v->size)
    fatal("Illegal index in low level array set routine.\n");
#endif

  add_ref(v);
  check_destructed(s);

  v->type_field = (v->type_field & ~BIT_UNFINISHED) | (1 << s->type);
  assign_svalue( ITEM(v) + index, s);
  free_array(v);
}


void simple_set_index(struct array *a,struct svalue *ind,struct svalue *s)
{
  INT32 i;
  if(ind->type != T_INT)
    error("Index is not an integer.\n");
  i=ind->u.integer;
  if(i<0) i+=a->size;
  if(i<0 || i>=a->size) {
    if (a->size) {
      error("Index %d is out of range 0 - %d.\n", i, a->size-1);
    } else {
      error("Attempt to index the empty array with %d.\n", i);
    }
  }
  array_set_index(a,i,s);
}

/*
 * Insert an svalue into an array, grow the array if nessesary
 */
struct array *array_insert(struct array *v,struct svalue *s,INT32 index)
{
#ifdef PIKE_DEBUG
  if(index<0 || index>v->size)
    fatal("Illegal index in low level insert routine.\n");
#endif

  /* Can we fit it into the existing block? */
  if(v->refs<=1 && v->malloced_size > v->size)
  {
    MEMMOVE((char *)(ITEM(v)+index+1),
	    (char *)(ITEM(v)+index),
	    (v->size-index) * sizeof(struct svalue));
    ITEM(v)[index].type=T_INT;
#ifdef __CHECKER__
    ITEM(v)[index].subtype=0;
    ITEM(v)[index].u.refs=0;
#endif
    v->size++;
  }else{
    struct array *ret;

    ret=allocate_array_no_init(v->size+1, (v->size >> 3) + 1);
    ret->type_field = v->type_field;

    MEMCPY(ITEM(ret), ITEM(v), sizeof(struct svalue) * index);
    MEMCPY(ITEM(ret)+index+1, ITEM(v)+index, sizeof(struct svalue) * (v->size-index));
    ITEM(ret)[index].type=T_INT;
#ifdef __CHECKER__
    ITEM(ret)[index].subtype=0;
    ITEM(ret)[index].u.refs=0;
#endif
    v->size=0;
    free_array(v);
    v=ret;
  }

  array_set_index(v,index,s);

  return v;
}

/*
 * resize array, resize an array destructively
 */
struct array *resize_array(struct array *a, INT32 size)
{
#ifdef PIKE_DEBUG
  if(d_flag > 1)  array_check_type_field(a);
#endif

  if(a->size == size) return a;
  if(size > a->size)
  {
    /* We should grow the array */

    if(a->malloced_size >= size)
    {
      for(;a->size < size; a->size++)
      {
	ITEM(a)[a->size].type=T_INT;
	ITEM(a)[a->size].subtype=NUMBER_NUMBER;
	ITEM(a)[a->size].u.integer=0;
      }
      a->type_field |= BIT_INT;
      return a;
    }else{
      struct array *ret;
      ret=low_allocate_array(size, (size>>1) + 4);
      MEMCPY(ITEM(ret),ITEM(a),sizeof(struct svalue)*a->size);
      ret->type_field = a->type_field | BIT_INT;
      a->size=0;
      free_array(a);
      return ret;
    }
  }else{
    /* We should shrink the array */
    free_svalues(ITEM(a)+size, a->size - size, a->type_field);
    a->size = size;
    return a;
  }
}

/*
 * Shrink an array destructively
 */
struct array *array_shrink(struct array *v,INT32 size)
{
  struct array *a;

#ifdef PIKE_DEBUG
  if(v->refs>2) /* Odd, but has to be two */
    fatal("Array shrink on array with many references.\n");

  if(size > v->size)
    fatal("Illegal argument to array_shrink.\n");
#endif

  if(size*2 < v->malloced_size + 4) /* Should we realloc it? */
  {
    a=allocate_array_no_init(size,0);
    a->type_field = v->type_field;

    free_svalues(ITEM(v) + size, v->size - size, v->type_field);
    MEMCPY(ITEM(a), ITEM(v), size*sizeof(struct svalue));
    v->size=0;
    free_array(v);
    return a;
  }else{
    free_svalues(ITEM(v) + size, v->size - size, v->type_field);
    v->size=size;
    return v;
  }
}

/*
 * Remove an index from an array and shrink the array
 */
struct array *array_remove(struct array *v,INT32 index)
{
  struct array *a;

#ifdef PIKE_DEBUG
  if(v->refs>1)
    fatal("Array remove on array with many references.\n");

  if(index<0 || index >= v->size)
    fatal("Illegal argument to array_remove.\n");
#endif

  array_free_index(v, index);
  if(v->size!=1 &&
     v->size*2 + 4 < v->malloced_size ) /* Should we realloc it? */
  {
    a=allocate_array_no_init(v->size-1, 0);
    a->type_field = v->type_field;

    if(index>0)
      MEMCPY(ITEM(a), ITEM(v), index*sizeof(struct svalue));
    if(v->size-index>1)
      MEMCPY(ITEM(a)+index,
	     ITEM(v)+index+1,
	     (v->size-index-1)*sizeof(struct svalue));
    v->size=0;
    free_array(v);
    return a;
  }else{
    if(v->size-index>1)
    {
      MEMMOVE((char *)(ITEM(v)+index),
	      (char *)(ITEM(v)+index+1),
	      (v->size-index-1)*sizeof(struct svalue));
    }
    v->size--;
    return v;
  }
}

/*
 * Search for in svalue in an array.
 * return the index if found, -1 otherwise
 */
INT32 array_search(struct array *v, struct svalue *s,INT32 start)
{
  INT32 e;
#ifdef PIKE_DEBUG
  if(start<0)
    fatal("Start of find_index is less than zero.\n");
#endif

  check_destructed(s);

  /* Why search for something that is not there? */
#ifdef PIKE_DEBUG
  if(d_flag > 1)  array_check_type_field(v);
#endif
  if(v->type_field & (1 << s->type))
  {
    if(start)
    {
      for(e=start;e<v->size;e++)
	if(is_eq(ITEM(v)+e,s)) return e;
    }else{
      TYPE_FIELD t=0;
      for(e=0;e<v->size;e++)
      {
	if(is_eq(ITEM(v)+e,s)) return e;
	t |= 1<<ITEM(v)[e].type;
      }
      v->type_field=t;
    }
  }
  return -1;
}

/*
 * Slice a pice of an array (nondestructively)
 * return an array consisting of v[start..end-1]
 */
struct array *slice_array(struct array *v,INT32 start,INT32 end)
{
  struct array *a;

#ifdef PIKE_DEBUG
  if(start > end || end>v->size || start<0)
    fatal("Illegal arguments to slice_array()\n");

  if(d_flag > 1)  array_check_type_field(v);
#endif

  if(start==0 && v->refs==1)	/* Can we use the same array? */
  {
    add_ref(v);
    return array_shrink(v,end);
  }

  a=allocate_array_no_init(end-start,0);
  a->type_field = v->type_field;

  assign_svalues_no_free(ITEM(a), ITEM(v)+start, end-start, v->type_field);

  return a;
}

/*
 * Slice a pice of an array (nondestructively)
 * return an array consisting of v[start..end-1]
 */
struct array *friendly_slice_array(struct array *v,INT32 start,INT32 end)
{
  struct array *a;

#ifdef PIKE_DEBUG
  if(start > end || end>v->size || start<0)
    fatal("Illegal arguments to slice_array()\n");

  if(d_flag > 1)  array_check_type_field(v);
#endif

  a=allocate_array_no_init(end-start,0);
  a->type_field = v->type_field;

  assign_svalues_no_free(ITEM(a), ITEM(v)+start, end-start, v->type_field);

  return a;
}

/*
 * Copy an array
 */
struct array *copy_array(struct array *v)
{
  struct array *a;

  a=allocate_array_no_init(v->size, 0);
  a->type_field = v->type_field;

  assign_svalues_no_free(ITEM(a), ITEM(v), v->size, v->type_field);

  return a;
}

/*
 * Clean an array from destructed objects
 */
void check_array_for_destruct(struct array *v)
{
  int e;
  INT16 types;

  types = 0;
#ifdef PIKE_DEBUG
  if(d_flag > 1)  array_check_type_field(v);
#endif
  if(v->type_field & (BIT_OBJECT | BIT_FUNCTION))
  {
    for(e=0; e<v->size; e++)
    {
      if((ITEM(v)[e].type == T_OBJECT ||
	  (ITEM(v)[e].type == T_FUNCTION &&
	   ITEM(v)[e].subtype!=FUNCTION_BUILTIN)) &&
	 (!ITEM(v)[e].u.object->prog))
      {
	free_svalue(ITEM(v)+e);
	ITEM(v)[e].type=T_INT;
	ITEM(v)[e].subtype=NUMBER_DESTRUCTED;
	ITEM(v)[e].u.integer=0;

	types |= BIT_INT;
      }else{
	types |= 1<<ITEM(v)[e].type;
      }
    }
    v->type_field = types;
  }
}

/*
 * This function finds the index of any destructed object in a set
 * it could be optimized to search out the object part with a binary 
 * search lookup if the array is mixed
 */
INT32 array_find_destructed_object(struct array *v)
{
  INT32 e;
  TYPE_FIELD types;
#ifdef PIKE_DEBUG
  if(d_flag > 1)  array_check_type_field(v);
#endif
  if(v->type_field & (BIT_OBJECT | BIT_FUNCTION))
  {
    types=0;
    for(e=0; e<v->size; e++)
    {
      if((ITEM(v)[e].type == T_OBJECT ||
	  (ITEM(v)[e].type == T_FUNCTION &&
	   ITEM(v)[e].subtype!=FUNCTION_BUILTIN)) &&
	 (!ITEM(v)[e].u.object->prog))
	return e;
      types |= 1<<ITEM(v)[e].type;
    }
    v->type_field = types;
  }
#ifdef PIKE_DEBUG
  if(d_flag > 1)  array_check_type_field(v);
#endif
  return -1;
}

static int internal_cmpfun(INT32 *a,
			   INT32 *b,
			   cmpfun current_cmpfun,
			   struct svalue *current_array_p)
{
  return current_cmpfun(current_array_p + *a, current_array_p + *b);
}

#define CMP(X,Y) internal_cmpfun((X),(Y),current_cmpfun, current_array_p)
#define TYPE INT32
#define ID get_order_fsort
#define EXTRA_ARGS ,cmpfun current_cmpfun, struct svalue *current_array_p
#define XARGS ,current_cmpfun, current_array_p
#include "fsort_template.h"
#undef CMP
#undef TYPE
#undef ID
#undef EXTRA_ARGS
#undef XARGS

INT32 *get_order(struct array *v, cmpfun fun)
{
  INT32 e, *current_order;
  ONERROR tmp;

  if(!v->size) return 0;

  current_order=(INT32 *)xalloc(v->size * sizeof(INT32));
  SET_ONERROR(tmp, free, current_order);
  for(e=0; e<v->size; e++) current_order[e]=e;

  get_order_fsort(current_order,
		  current_order+v->size-1,
		  fun,
		  ITEM(v));

  UNSET_ONERROR(tmp);
  return current_order;
}

static int set_svalue_cmpfun(struct svalue *a, struct svalue *b)
{
  INT32 tmp;
  if((tmp=(a->type - b->type))) return tmp;
  switch(a->type)
  {
  case T_FLOAT:
    if(a->u.float_number < b->u.float_number) return -1;
    if(a->u.float_number > b->u.float_number) return 1;
    return 0;

  case T_FUNCTION:
    if(a->u.refs < b->u.refs) return -1;
    if(a->u.refs > b->u.refs) return 1;
    return a->subtype - b->subtype;

  case T_INT:
    if(a->u.integer < b->u.integer) return -1;
    if(a->u.integer > b->u.integer) return 1;
    return 0;

  default:
    if(a->u.refs < b->u.refs) return -1;
    if(a->u.refs > b->u.refs) return 1;
    return 0;
  }
}

static int switch_svalue_cmpfun(struct svalue *a, struct svalue *b)
{
  if(a->type != b->type) return a->type - b->type;
  switch(a->type)
  {
  case T_INT:
    if(a->u.integer < b->u.integer) return -1;
    if(a->u.integer > b->u.integer) return 1;
    return 0;

  case T_FLOAT:
    if(a->u.float_number < b->u.float_number) return -1;
    if(a->u.float_number > b->u.float_number) return 1;
    return 0;

  case T_STRING:
    return my_strcmp(a->u.string, b->u.string);
    
  default:
    return set_svalue_cmpfun(a,b);
  }
}

static int alpha_svalue_cmpfun(struct svalue *a, struct svalue *b)
{
  if(a->type == b->type)
  {
    switch(a->type)
    {
      case T_INT:
	if(a->u.integer < b->u.integer) return -1;
	if(a->u.integer > b->u.integer) return  1;
	return 0;
	
      case T_FLOAT:
	if(a->u.float_number < b->u.float_number) return -1;
	if(a->u.float_number > b->u.float_number) return  1;
	return 0;
	
      case T_STRING:
	return my_strcmp(a->u.string, b->u.string);
	
      case T_ARRAY:
	if(a==b) return 0;
	if(!a->u.array->size) return -1;
	if(!b->u.array->size) return  1;
	return alpha_svalue_cmpfun(ITEM(a->u.array), ITEM(b->u.array));
	
      default:
	return set_svalue_cmpfun(a,b);
	
      case T_OBJECT:
	break;
    }
  }else{
    if(a->type!=T_OBJECT && b->type!=T_OBJECT)
      return a->type - b->type;
  }
  return is_gt(a,b);
}

#define CMP(X,Y) alpha_svalue_cmpfun(X,Y)
#define TYPE struct svalue
#define ID low_sort_svalues
#include "fsort_template.h"
#undef CMP
#undef TYPE
#undef ID


void sort_array_destructively(struct array *v)
{
  if(!v->size) return;
  low_sort_svalues(ITEM(v), ITEM(v)+v->size-1);
}



/*
 * return an 'order' suitable for making mappings and multisets
 */
INT32 *get_set_order(struct array *a)
{
  return get_order(a, set_svalue_cmpfun);
}

/*
 * return an 'order' suitable for switches.
 */
INT32 *get_switch_order(struct array *a)
{
  return get_order(a, switch_svalue_cmpfun);
}


/*
 * return an 'order' suitable for sorting.
 */
INT32 *get_alpha_order(struct array *a)
{
  return get_order(a, alpha_svalue_cmpfun);
}


static INT32 low_lookup(struct array *v,
			struct svalue *s,
			cmpfun fun)
{
  INT32 a,b,c;
  int q;

  a=0;
  b=v->size;
  while(b > a)
  {
    c=(a+b)/2;
    q=fun(ITEM(v)+c,s);
    
    if(q < 0)
      a=c+1;
    else if(q > 0)
      b=c;
    else
      return c;
  }
  if(a<v->size && fun(ITEM(v)+a,s)<0) a++;
  return ~a;
}

INT32 set_lookup(struct array *a, struct svalue *s)
{
#ifdef PIKE_DEBUG
  if(d_flag > 1)  array_check_type_field(a);
#endif
  /* face it, it's not there */
  if( (((2 << s->type) -1) & a->type_field) == 0)
    return -1;

  /* face it, it's not there */
  if( ((BIT_MIXED << s->type) & BIT_MIXED & a->type_field) == 0)
    return ~a->size;

  return low_lookup(a,s,set_svalue_cmpfun);
}

INT32 switch_lookup(struct array *a, struct svalue *s)
{
  /* face it, it's not there */
#ifdef PIKE_DEBUG
  if(d_flag > 1)  array_check_type_field(a);
#endif
  if( (((2 << s->type) -1) & a->type_field) == 0)
    return -1;

  /* face it, it's not there */
  if( ((BIT_MIXED << s->type) & BIT_MIXED & a->type_field) == 0)
    return ~a->size;

  return low_lookup(a,s,switch_svalue_cmpfun);
}


/*
 * reorganize an array in the order specifyed by 'order'
 */
struct array *order_array(struct array *v, INT32 *order)
{
  reorder((char *)ITEM(v),v->size,sizeof(struct svalue),order);
  return v;
}


/*
 * copy and reorganize an array
 */
struct array *reorder_and_copy_array(struct array *v, INT32 *order)
{
  INT32 e;
  struct array *ret;
  ret=allocate_array_no_init(v->size, 0);
  ret->type_field = v->type_field;

  for(e=0;e<v->size;e++)
    assign_svalue_no_free(ITEM(ret)+e, ITEM(v)+order[e]);

  return ret;
}

/* Maybe I should have a 'clean' flag for this computation */
void array_fix_type_field(struct array *v)
{
  int e;
  TYPE_FIELD t;

  t=0;

  if(v->flags & ARRAY_LVALUE)
  {
    v->type_field=BIT_MIXED;
    return;
  }

  for(e=0; e<v->size; e++) t |= 1 << ITEM(v)[e].type;

#ifdef PIKE_DEBUG
  if(t & ~(v->type_field))
  {
    describe(v);
    fatal("Type field out of order!\n");
  }
#endif
  v->type_field = t;
}

#ifdef PIKE_DEBUG
/* Maybe I should have a 'clean' flag for this computation */
void array_check_type_field(struct array *v)
{
  int e;
  TYPE_FIELD t;

  t=0;

  if(v->flags & ARRAY_LVALUE)
    return;

  for(e=0; e<v->size; e++)
  {
    if(ITEM(v)[e].type > MAX_TYPE)
      fatal("Type is out of range.\n");
      
    t |= 1 << ITEM(v)[e].type;
  }

  if(t & ~(v->type_field))
  {
    describe(v);
    fatal("Type field out of order!\n");
  }
}
#endif

struct array *compact_array(struct array *v) { return v; }

/*
 * Get a pointer to the 'union anything' specified IF it is of the specified
 * type. The 'union anything' may be changed, but not the type.
 */
union anything *low_array_get_item_ptr(struct array *a,
				       INT32 ind,
				       TYPE_T t)
{
  if(ITEM(a)[ind].type == t) return & (ITEM(a)[ind].u);
  return 0;
}

/*
 * Get a pointer to the 'union anything' specified IF it is of the specified
 * type. The 'union anything' may be changed, but not the type.
 * The differance between this routine and the one above is that this takes
 * the index as an svalue.
 */
union anything *array_get_item_ptr(struct array *a,
				   struct svalue *ind,
				   TYPE_T t)
{
  INT32 i;
  if(ind->type != T_INT)
    error("Index is not an integer.\n");
  i=ind->u.integer;
  if(i<0) i+=a->size;
  if(i<0 || i>=a->size) {
    if (a->size) {
      error("Index %d is out of range 0 - %d.\n", i, a->size-1);
    } else {
      error("Attempt to index the empty array with %d.\n", i);
    }
  }
  return low_array_get_item_ptr(a,i,t);
}

/*
 * organize an array of INT32 to specify how to zip two arrays together
 * to maintain the order.
 * the first item in this array is the size of the result
 * the rest is n >= 0 for a[ n ]
 * or n < 0 for b[ ~n ]
 */
INT32 * merge(struct array *a,struct array *b,INT32 opcode)
{
  INT32 ap,bp,i,*ret,*ptr;
  
  ap=bp=0;
#ifdef PIKE_DEBUG
  if(d_flag > 1)
  {
    array_check_type_field(a);
    array_check_type_field(b);
  }
#endif
  if(!(a->type_field & b->type_field))
  {
    /* do smart optimizations */
    switch(opcode)
    {
    case PIKE_ARRAY_OP_AND:
      ret=(INT32 *)xalloc(sizeof(INT32));
      *ret=0;
      return ret;

    case PIKE_ARRAY_OP_SUB:
      ptr=ret=(INT32 *)xalloc(sizeof(INT32)*(a->size+1));
      *(ptr++)=a->size;
      for(i=0;i<a->size;i++) *(ptr++)=i;
      return ret;
    }
  }

  ptr=ret=(INT32 *)xalloc(sizeof(INT32)*(a->size + b->size + 1));
  ptr++;

  while(ap < a->size && bp < b->size)
  {
    i=set_svalue_cmpfun(ITEM(a)+ap,ITEM(b)+bp);
    if(i < 0)
      i=opcode >> 8;
    else if(i > 0)
      i=opcode;
    else
      i=opcode >> 4;
    
    if(i & PIKE_ARRAY_OP_A) *(ptr++)=ap;
    if(i & PIKE_ARRAY_OP_B) *(ptr++)=~bp;
    if(i & PIKE_ARRAY_OP_SKIP_A) ap++;
    if(i & PIKE_ARRAY_OP_SKIP_B) bp++;
  }

  if((opcode >> 8) & PIKE_ARRAY_OP_A) while(ap<a->size) *(ptr++)=ap++;
  if(opcode & PIKE_ARRAY_OP_B) while(bp<b->size) *(ptr++)=~(bp++);

  *ret=(ptr-ret-1);

  return ret;
}

/*
 * This routine merges two arrays in the order specified by 'zipper'
 * zipper normally produced by merge() above
 */
struct array *array_zip(struct array *a, struct array *b,INT32 *zipper)
{
  INT32 size,e;
  struct array *ret;
  size=zipper[0];
  zipper++;

  ret=allocate_array_no_init(size,0);
  for(e=0; e<size; e++)
  {
    if(*zipper >= 0)
      assign_svalue_no_free(ITEM(ret)+e, ITEM(a)+*zipper);
    else
      assign_svalue_no_free(ITEM(ret)+e, ITEM(b)+~*zipper);
    zipper++;
  }
  ret->type_field = a->type_field | b->type_field;
  return ret;
}

struct array *add_arrays(struct svalue *argp, INT32 args)
{
  INT32 e, size;
  struct array *v;

  for(size=e=0;e<args;e++)
    size+=argp[e].u.array->size;

  if(args && argp[0].u.array->refs==1)
  {
    e=argp[0].u.array->size;
    v=resize_array(argp[0].u.array, size);
    argp[0].type=T_INT;
    size=e;
    e=1;
  }else{
    v=allocate_array_no_init(size, 0);
    v->type_field=0;
    e=size=0;
  }

  for(; e<args; e++)
  {
    v->type_field|=argp[e].u.array->type_field;
    assign_svalues_no_free(ITEM(v)+size,
			   ITEM(argp[e].u.array),
			   argp[e].u.array->size,
			   argp[e].u.array->type_field);
    size+=argp[e].u.array->size;
  }

  return v;
}

int array_equal_p(struct array *a, struct array *b, struct processing *p)
{
  struct processing curr;
  INT32 e;

  if(a == b) return 1;
  if(a->size != b->size) return 0;
  if(!a->size) return 1;

#ifdef PIKE_DEBUG
  if(d_flag > 1)
  {
    array_check_type_field(a);
    array_check_type_field(b);
  }
#endif

  /* This could be done much better if I KNEW that
   * the type fields didn't contain types that
   * really aren't in the array
   */
  if(!(a->type_field & b->type_field) &&
     !( (a->type_field | b->type_field) & BIT_OBJECT ))
    return 0;

  curr.pointer_a = a;
  curr.pointer_b = b;
  curr.next = p;

  for( ;p ;p=p->next)
    if(p->pointer_a == (void *)a && p->pointer_b == (void *)b)
      return 1;

  for(e=0; e<a->size; e++)
    if(!low_is_equal(ITEM(a)+e, ITEM(b)+e, &curr))
      return 0;

  return 1;
}

static INT32 *ordera=0, *orderb=0;
/*
 * this is used to rearrange the zipper so that the order is retained
 * as it was before (check merge_array_with_order below)
 */
static int array_merge_fun(INT32 *a, INT32 *b)
{
  if(*a<0)
  {
    if(*b<0)
    {
      return orderb[~*a] - orderb[~*b];
    }else{
      return -1;
    }
  }else{
    if(*b<0)
    {
      return 1;
    }else{
      return ordera[*a] - ordera[*b];
    }
  }
}



/*
 * merge two arrays and retain their order, this is done by arranging them
 * into ordered sets, merging them as sets and then rearranging the zipper
 * before zipping the sets together. 
 */
struct array *merge_array_with_order(struct array *a, struct array *b,INT32 op)
{
  INT32 *zipper;
  struct array *tmpa,*tmpb,*ret;

  if(ordera) { free((char *)ordera); ordera=0; }
  if(orderb) { free((char *)orderb); orderb=0; }

  ordera=get_set_order(a);
  tmpa=reorder_and_copy_array(a,ordera);

  orderb=get_set_order(b);
  tmpb=reorder_and_copy_array(b,orderb);

  zipper=merge(tmpa,tmpb,op);

  fsort((char *)(zipper+1),*zipper,sizeof(INT32),(fsortfun)array_merge_fun);
  free((char *)ordera);
  free((char *)orderb);
  orderb=ordera=0;
  ret=array_zip(tmpa,tmpb,zipper);
  free_array(tmpa);
  free_array(tmpb);
  free((char *)zipper);
  return ret;
}


#define CMP(X,Y) set_svalue_cmpfun(X,Y)
#define TYPE struct svalue
#define ID set_sort_svalues
#include "fsort_template.h"
#undef CMP
#undef TYPE
#undef ID


/*
 * merge two arrays and retain their order, this is done by arranging them
 * into ordered sets, merging them as sets and then rearranging the zipper
 * before zipping the sets together. 
 */
struct array *merge_array_without_order2(struct array *a, struct array *b,INT32 op)
{
  INT32 ap,bp,i;
  struct svalue *arra,*arrb;
  struct array *ret;

#ifdef PIKE_DEBUG
  if(d_flag > 1)
  {
    array_check_type_field(a);
    array_check_type_field(b);
  }
#endif

  if(a->refs==1 || !a->size)
  {
    arra=ITEM(a);
  }else{
    arra=(struct svalue *)xalloc(a->size*sizeof(struct svalue));
    MEMCPY(arra,ITEM(a),a->size*sizeof(struct svalue));
  }

  if(b->refs==1 || !b->size)
  {
    arrb=ITEM(b);
  }else{
    arrb=(struct svalue *)xalloc(b->size*sizeof(struct svalue));
    MEMCPY(arrb,ITEM(b),b->size*sizeof(struct svalue));
  }

  set_sort_svalues(arra,arra+a->size-1);
  set_sort_svalues(arrb,arrb+b->size-1);

  ret=low_allocate_array(0,32);
  ap=bp=0;

  while(ap < a->size && bp < b->size)
  {
    i=set_svalue_cmpfun(arra+ap,arrb+bp);
    if(i < 0)
      i=op >> 8;
    else if(i > 0)
      i=op;
    else
      i=op >> 4;
    
    if(i & PIKE_ARRAY_OP_A) ret=append_array(ret,arra+ap);
    if(i & PIKE_ARRAY_OP_B) ret=append_array(ret,arrb+bp);
    if(i & PIKE_ARRAY_OP_SKIP_A) ap++;
    if(i & PIKE_ARRAY_OP_SKIP_B) bp++;
  }

  if((op >> 8) & PIKE_ARRAY_OP_A)
    while(ap<a->size)
      ret=append_array(ret,arra + ap++);

  if(op & PIKE_ARRAY_OP_B)
    while(bp<b->size)
      ret=append_array(ret,arrb + bp++);

  if(arra != ITEM(a)) free((char *)arra);
  if(arrb != ITEM(b)) free((char *)arrb);

  free_array(a);
  free_array(b);

  return ret;
}


/* merge two arrays without paying attention to the order
 * the elements has presently
 */
struct array *merge_array_without_order(struct array *a,
					struct array *b,
					INT32 op)
{
#if 0
  INT32 *zipper;
  struct array *tmpa,*tmpb,*ret;

  if(ordera) { free((char *)ordera); ordera=0; }
  if(orderb) { free((char *)orderb); orderb=0; }

  ordera=get_set_order(a);
  tmpa=reorder_and_copy_array(a,ordera);
  free((char *)ordera);
  ordera=0;

  orderb=get_set_order(b);
  tmpb=reorder_and_copy_array(b,orderb);
  free((char *)orderb);
  orderb=0;

  zipper=merge(tmpa,tmpb,op);
  ret=array_zip(tmpa,tmpb,zipper);
  free_array(tmpa);
  free_array(tmpb);
  free((char *)zipper);
  return ret;

#else
  add_ref(a);
  add_ref(b);
  return merge_array_without_order2(a,b,op);
#endif
}

/* subtract an array from another */
struct array *subtract_arrays(struct array *a, struct array *b)
{
#ifdef PIKE_DEBUG
  if(d_flag > 1)
  {
    array_check_type_field(b);
  }
#endif
  check_array_for_destruct(a);

  if(a->type_field & b->type_field)
  {
    return merge_array_with_order(a, b, PIKE_ARRAY_OP_SUB);
  }else{
    if(a->refs == 1)
    {
      add_ref(a);
      return a;
    }
    return slice_array(a,0,a->size);
  }
}

/* and two arrays */
struct array *and_arrays(struct array *a, struct array *b)
{
#ifdef PIKE_DEBUG
  if(d_flag > 1)
  {
    array_check_type_field(b);
  }
#endif
  check_array_for_destruct(a);

  if(a->type_field & b->type_field)
  {
    return merge_array_without_order(a, b, PIKE_ARRAY_OP_AND);
  }else{
    return allocate_array_no_init(0,0);
  }
}

int check_that_array_is_constant(struct array *a)
{
  array_fix_type_field(a);
  if(a->type_field & (BIT_FUNCTION | BIT_OBJECT))
    return 0;
  return 1;
}

node *make_node_from_array(struct array *a)
{
  struct svalue s;
  INT32 e;

  array_fix_type_field(a);
  if(a->type_field == BIT_INT)
  {
    for(e=0; e<a->size; e++)
      if(ITEM(a)[e].u.integer != 0)
	break;
    if(e == a->size)
    {
      return mkefuncallnode("allocate",mkintnode(a->size));
    }
  }
  if(check_that_array_is_constant(a))
  {
    s.type=T_ARRAY;
    s.subtype=0;
    s.u.array=a;
    return mkconstantsvaluenode(&s);
  }else{
    node *ret=0;
    for(e=0; e<a->size; e++)
      ret=mknode(F_ARG_LIST,ret,mksvaluenode(ITEM(a)+e));
    return mkefuncallnode("aggregate",ret);
  }
}

void push_array_items(struct array *a)
{
  check_stack(a->size);
  check_array_for_destruct(a);
  if(a->refs == 1)
  {
    MEMCPY(sp,ITEM(a),sizeof(struct svalue)*a->size);
    sp += a->size;
    a->size=0;
    free_array(a);
  }else{
    assign_svalues_no_free(sp, ITEM(a), a->size, a->type_field);
    sp += a->size;
    free_array(a);
  }
}

void describe_array_low(struct array *a, struct processing *p, int indent)
{
  INT32 e,d;
  indent += 2;

  for(e=0; e<a->size; e++)
  {
    if(e) my_strcat(",\n");
    for(d=0; d<indent; d++) my_putchar(' ');
    describe_svalue(ITEM(a)+e,indent,p);
  }
}

void simple_describe_array(struct array *a)
{
  char *s;
  init_buf();
  describe_array_low(a,0,0);
  s=simple_free_buf();
  fprintf(stderr,"({\n%s\n})\n",s);
  free(s);
}

void describe_index(struct array *a,
		    int e,
		    struct processing *p,
		    int indent)
{
  describe_svalue(ITEM(a)+e, indent, p);
}


void describe_array(struct array *a,struct processing *p,int indent)
{
  struct processing doing;
  INT32 e;
  char buf[60];
  if(! a->size)
  {
    my_strcat("({ })");
    return;
  }

  doing.next=p;
  doing.pointer_a=(void *)a;
  for(e=0;p;e++,p=p->next)
  {
    if(p->pointer_a == (void *)a)
    {
      sprintf(buf,"@%ld",(long)e);
      my_strcat(buf);
      return;
    }
  }
  
  sprintf(buf,"({ /* %ld elements */\n",(long)a->size);
  my_strcat(buf);
  describe_array_low(a,&doing,indent);
  my_putchar('\n');
  for(e=2; e<indent; e++) my_putchar(' ');
  my_strcat("})");
}

struct array *aggregate_array(INT32 args)
{
  struct array *a;

  a=allocate_array_no_init(args,0);
  MEMCPY((char *)ITEM(a),(char *)(sp-args),args*sizeof(struct svalue));
  a->type_field=BIT_MIXED;
  sp-=args;
  return a;
}

struct array *append_array(struct array *a, struct svalue *s)
{
  a=resize_array(a,a->size+1);
  array_set_index(a, a->size-1, s);
  return a;
}

struct array *explode(struct pike_string *str,
		       struct pike_string *del)
{
  INT32 e;
  struct array *ret;
  char *s, *end, *tmp;

#if 0
  if(!str->len)
  {
    return allocate_array_no_init(0,0);
  }
#endif
  if(!del->len)
  {
    ret=allocate_array_no_init(str->len,0);
    for(e=0;e<str->len;e++)
    {
      ITEM(ret)[e].type=T_STRING;
      ITEM(ret)[e].u.string=string_slice(str,e,1);
    }
  }else{
    struct generic_mem_searcher searcher;
    
    s=str->str;
    end=s+(str->len << str->size_shift);

    ret=allocate_array(10);
    ret->size=0;
    
    init_generic_memsearcher(&searcher,
			     del->str,
			     del->len,
			     del->size_shift,
			     str->len,
			     str->size_shift);
    
    while((tmp=(char *)generic_memory_search(&searcher,
					     s,
					     (end-s)>>str->size_shift,
					     str->size_shift)))
    {
      if(ret->size == ret->malloced_size)
      {
	e=ret->size;
	ret=resize_array(ret, e * 2);
	ret->size=e;
      }

      ITEM(ret)[ret->size].u.string=string_slice(str,
						 (s-str->str)>>str->size_shift,
						 (tmp-s)>>str->size_shift);
      ITEM(ret)[ret->size].type=T_STRING;
      ret->size++;

      s=tmp+(del->len << str->size_shift);
    }

    if(ret->size == ret->malloced_size)
    {
      e=ret->size;
      ret=resize_array(ret, e * 2);
      ret->size=e;
    }

    ITEM(ret)[ret->size].u.string=string_slice(str,
					       (s-str->str)>>str->size_shift,
					       (end-s)>>str->size_shift);

    ITEM(ret)[ret->size].type=T_STRING;
    ret->size++;
  }
  ret->type_field=BIT_STRING;
  return ret;
}

struct pike_string *implode(struct array *a,struct pike_string *del)
{
  INT32 len,e, inited;
  PCHARP r;
  struct pike_string *ret,*tmp;
  int max_shift=0;

  len=0;

  for(e=0;e<a->size;e++)
  {
    if(ITEM(a)[e].type==T_STRING)
    {
      len+=ITEM(a)[e].u.string->len + del->len;
      if(ITEM(a)[e].u.string->size_shift > max_shift)
	max_shift=ITEM(a)[e].u.string->size_shift;
    }
  }
  if(del->size_shift > max_shift) max_shift=del->size_shift;
  if(len) len-=del->len;
  
  ret=begin_wide_shared_string(len,max_shift);
  r=MKPCHARP_STR(ret);
  inited=0;
  for(e=0;e<a->size;e++)
  {
    if(ITEM(a)[e].type==T_STRING)
    {
      if(inited)
      {
	pike_string_cpy(r,del);
	INC_PCHARP(r,del->len);
      }
      inited=1;
      tmp=ITEM(a)[e].u.string;
      pike_string_cpy(r,tmp);
      INC_PCHARP(r,tmp->len);
      len++;
    }
  }
  return low_end_shared_string(ret);
}

struct array *copy_array_recursively(struct array *a,struct processing *p)
{
  struct processing doing;
  struct array *ret;

#ifdef PIKE_DEBUG
  if(d_flag > 1)  array_check_type_field(a);
#endif

  doing.next=p;
  doing.pointer_a=(void *)a;
  for(;p;p=p->next)
  {
    if(p->pointer_a == (void *)a)
    {
      ret=(struct array *)p->pointer_b;
      add_ref(ret);
      return ret;
    }
  }

  ret=allocate_array_no_init(a->size,0);
  doing.pointer_b=(void *)ret;

  copy_svalues_recursively_no_free(ITEM(ret),ITEM(a),a->size,&doing);

  ret->type_field=a->type_field;
  return ret;
}

void apply_array(struct array *a, INT32 args)
{
  INT32 e;
  struct array *ret;
  INT32 argp;

  argp=sp-args - evaluator_stack;

  check_stack(a->size + args + 1);

  for(e=0;e<a->size;e++)
  {
    assign_svalues_no_free(sp,evaluator_stack+argp,args,BIT_MIXED);
    sp+=args;
    apply_svalue(ITEM(a)+e,args);
  }
  ret=aggregate_array(a->size);
  pop_n_elems(args);
  push_array(ret);
}

struct array *reverse_array(struct array *a)
{
  INT32 e;
  struct array *ret;

  /* FIXME: Check refs so we might optimize */
  ret=allocate_array_no_init(a->size,0);
  for(e=0;e<a->size;e++)
    assign_svalue_no_free(ITEM(ret)+e,ITEM(a)+a->size+~e);
  return ret;
}

void array_replace(struct array *a,
		   struct svalue *from,
		   struct svalue *to)
{
  INT32 i = -1;

  while((i=array_search(a,from,i+1)) >= 0) array_set_index(a,i,to);
}

#ifdef PIKE_DEBUG
void check_array(struct array *a)
{
  INT32 e;

  if(a->next->prev != a)
    fatal("Array check: a->next->prev != a\n");

  if(a->size > a->malloced_size)
    fatal("Array is larger than malloced block!\n");

  if(a->refs <=0 )
    fatal("Array has zero refs.\n");

  for(e=0;e<a->size;e++)
  {
    if(! ( (1 << ITEM(a)[e].type) & (a->type_field) ) && ITEM(a)[e].type<16)
      fatal("Type field lies.\n");
    
    check_svalue(ITEM(a)+e);
  }
}

void check_all_arrays(void)
{
  struct array *a;

  a=&empty_array;
  do
  {
    check_array(a);

    a=a->next;
    if(!a)
      fatal("Null pointer in array list.\n");
  } while (a != & empty_array);
}
#endif /* PIKE_DEBUG */


void gc_mark_array_as_referenced(struct array *a)
{
  if(gc_mark(a) && !(a->flags & ARRAY_WEAK_FLAG))
    if(a->type_field & BIT_COMPLEX)
      gc_mark_svalues(ITEM(a), a->size);
}

void gc_check_all_arrays(void)
{
  struct array *a;
  a=&empty_array;
  do
  {
#ifdef PIKE_DEBUG
    if(d_flag > 1)  array_check_type_field(a);
#endif
    if(a->type_field & BIT_COMPLEX)
    {
      TYPE_FIELD t;
      t=debug_gc_check_svalues(ITEM(a), a->size, T_ARRAY, a);

      /* Ugly, but we are not allowed to change type_field
       * at the same time as the array is being built...
       * Actually we just need beter primitives for building arrays.
       */
      if(!(a->type_field & BIT_UNFINISHED) || a->refs!=1)
	a->type_field = t;
      else
	a->type_field |= t;
    }

    a=a->next;
  } while (a != & empty_array);
}


void gc_mark_all_arrays(void)
{
  struct array *a;

  a=&empty_array;
  do
  {
    if(gc_is_referenced(a))
      gc_mark_array_as_referenced(a);
            
    a=a->next;
  } while (a != & empty_array);
}

void gc_free_all_unreferenced_arrays(void)
{
  struct array *a,*next;

  a=&empty_array;
  do
  {
    if(gc_do_free(a))
    {
      add_ref(a);
      free_svalues(ITEM(a), a->size, a->type_field);
      a->size=0;

      if(!(next=a->next))
	fatal("Null pointer in array list.\n");

      free_array(a);
      a=next;
    }
    else if(a->flags & ARRAY_WEAK_FLAG)
    {
      int e;
      add_ref(a);

      if(a->flags & ARRAY_WEAK_SHRINK)
      {
	int d=0;
	for(e=0;e<a->size;e++)
	{
	  if(a->item[e].type <= MAX_COMPLEX && gc_do_free(a->item[e].u.refs))
	    free_svalue(a->item+e);
	  else
	    a->item[d++]=a->item[e];
	}
	a->size=d;
      }else{
	for(e=0;e<a->size;e++)
	{
	  if(a->item[e].type <= MAX_COMPLEX && gc_do_free(a->item[e].u.refs))
	  {
	    free_svalue(a->item+e);
	    a->item[e].type=T_INT;
	    a->item[e].u.integer=0;
	    a->item[e].subtype=NUMBER_DESTRUCTED;
	    a->type_field |= BIT_INT;
	  }
	}
      }
	  
      if(!(next=a->next))
	fatal("Null pointer in array list.\n");

      free_array(a);
      a=next;
    }
    else
    {
      a=a->next;
    }
  } while (a != & empty_array);
}


#ifdef PIKE_DEBUG

void debug_dump_type_field(TYPE_FIELD t)
{
  int e;
  for(e=0;e<=MAX_TYPE;e++)
    if(t & (1<<e))
      fprintf(stderr," %s",get_name_of_type(e));

  for(;e<16;e++)
    if(t & (1<<e))
      fprintf(stderr," <%d>",e);
}

void debug_dump_array(struct array *a)
{
  fprintf(stderr,"Location=%p Refs=%d, next=%p, prev=%p, size=%d, malloced_size=%d\n",
	  a,
	  a->refs,
	  a->next,
	  a->prev,
	  a->size,
	  a->malloced_size);
  fprintf(stderr,"Type field = ");
  debug_dump_type_field(a->type_field);
  fprintf(stderr,"\n");
  simple_describe_array(a);
}
#endif


void zap_all_arrays(void)
{
  struct array *a,*next;

  a=&empty_array;
  do
  {

#if defined(PIKE_DEBUG) && defined(DEBUG_MALLOC)
    if(verbose_debug_exit && a!=&empty_array)
      describe(a);
#endif
    
    add_ref(a);
    free_svalues(ITEM(a), a->size, a->type_field);
    a->size=0;
    
    if(!(next=a->next))
      fatal("Null pointer in array list.\n");
    
    free_array(a);
    a=next;
  } while (a != & empty_array);
}


void count_memory_in_arrays(INT32 *num_, INT32 *size_)
{
  INT32 num=0, size=0;
  struct array *m;
  for(m=empty_array.next;m!=&empty_array;m=m->next)
  {
    num++;
    size+=sizeof(struct array)+
      sizeof(struct svalue) *  (m->malloced_size - 1);
  }
  *num_=num;
  *size_=size;
}

struct array *explode_array(struct array *a, struct array *b)
{
  INT32 e,d,q,start;
  struct array *tmp;

  q=start=0;
#if 0
  if(!a->size)
  {
    return allocate_array_no_init(0,0);
  }
#endif
  if(b->size)
  {
    for(e=0;e<=a->size - b->size;e++)
    {
      for(d=0;d<b->size;d++)
      {
	if(!is_eq(ITEM(a)+(e+d),ITEM(b)+d))
	  break;
      }
      if(d==b->size)
      {
	check_stack(1);
	push_array(friendly_slice_array(a, start, e));
	q++;
	e+=b->size-1;
	start=e+1;
      }
    }
    check_stack(1);
    push_array(friendly_slice_array(a, start, a->size));
    q++;
  }else{
    check_stack(a->size);
    for(e=0;e<a->size;e++) push_array(friendly_slice_array(a, e, e+1));
    q=a->size;
  }
  tmp=aggregate_array(q);
  if(tmp->size) tmp->type_field=BIT_ARRAY;
  return tmp;
}

struct array *implode_array(struct array *a, struct array *b)
{
  INT32 e,size;
  struct array *ret;
  size=0;
  for(e=0;e<a->size;e++)
  {
    if(ITEM(a)[e].type!=T_ARRAY)
      error("Implode array contains non-arrays.\n");
    size+=ITEM(a)[e].u.array->size;
  }

  ret=allocate_array((a->size -1) * b->size + size);
  size=0;
  ret->type_field=0;
  for(e=0;e<a->size;e++)
  {
    if(e)
    {
      ret->type_field|=b->type_field;
      assign_svalues_no_free(ITEM(ret)+size,
			     ITEM(b),
			     b->size,
			     b->type_field);
      size+=b->size;
    }
    ret->type_field|=ITEM(a)[e].u.array->type_field;
    assign_svalues_no_free(ITEM(ret)+size,
			   ITEM(ITEM(a)[e].u.array),
			   ITEM(a)[e].u.array->size,
			   ITEM(a)[e].u.array->type_field);
    size+=ITEM(a)[e].u.array->size;
  }
#ifdef PIKE_DEBUG
  if(size != ret->size)
    fatal("Implode_array failed miserably\n");
#endif
  return ret;
}
