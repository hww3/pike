/*\
||| This file is part of Pike. For copyright information see COPYRIGHT.
||| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
||| for more information.
\*/
/**/
#include "global.h"
RCSID("$Id: pike_types.c,v 1.187 2002/06/11 17:30:20 mast Exp $");
#include <ctype.h>
#include "svalue.h"
#include "pike_types.h"
#include "stralloc.h"
#include "stuff.h"
#include "array.h"
#include "program.h"
#include "constants.h"
#include "object.h"
#include "multiset.h"
#include "mapping.h"
#include "pike_macros.h"
#include "pike_error.h"
#include "las.h"
#include "language.h"
#include "lex.h"
#include "pike_memory.h"
#include "bignum.h"
#include "main.h"
#include "opcodes.h"
#include "block_alloc.h"

#ifdef PIKE_DEBUG
/* #define PIKE_TYPE_DEBUG */
#endif /* PIKE_DEBUG */

/*
 * Flags used by low_match_types().
 */
#define A_EXACT 1
#define B_EXACT 2
#define NO_MAX_ARGS 4
#define NO_SHORTCUTS 8

/*
 * Flags used by pike_types_le()
 */
#define LE_WEAK_OBJECTS	1	/* Perform weaker checking of objects. */

#ifdef USE_PIKE_TYPE
/* Number of entries in the struct pike_type hash-table. */
#define PIKE_TYPE_HASH_SIZE	32768
#endif /* USE_PIKE_TYPE */


#ifdef PIKE_TYPE_DEBUG
static int indent=0;
#endif

int max_correct_args;

PMOD_EXPORT struct pike_type *string_type_string;
PMOD_EXPORT struct pike_type *int_type_string;
PMOD_EXPORT struct pike_type *float_type_string;
PMOD_EXPORT struct pike_type *function_type_string;
PMOD_EXPORT struct pike_type *object_type_string;
PMOD_EXPORT struct pike_type *program_type_string;
PMOD_EXPORT struct pike_type *array_type_string;
PMOD_EXPORT struct pike_type *multiset_type_string;
PMOD_EXPORT struct pike_type *mapping_type_string;
PMOD_EXPORT struct pike_type *type_type_string;
PMOD_EXPORT struct pike_type *mixed_type_string;
PMOD_EXPORT struct pike_type *void_type_string;
PMOD_EXPORT struct pike_type *zero_type_string;
PMOD_EXPORT struct pike_type *any_type_string;
PMOD_EXPORT struct pike_type *weak_type_string;	/* array|mapping|multiset|function */

#if defined(USE_PIKE_TYPE) && defined(DEBUG_MALLOC)
struct pike_type_location *all_pike_type_locations = NULL;
#endif /* USE_PIKE_TYPE && DEBUG_MALLOC */

static struct pike_type *a_markers[10], *b_markers[10];

static struct program *implements_a;
static struct program *implements_b;

#ifdef PIKE_DEBUG
void TYPE_STACK_DEBUG(const char *fun)
{
#if 0
  fprintf(stderr, "%25s(): stack_depth:%ld   mark_stack_depth:%ld\n",
	  fun, (long)(Pike_compiler->type_stackp - type_stack),
	  (long)(Pike_compiler->pike_type_mark_stackp - pike_type_mark_stack));
#endif /* 0 */
}
#endif /* PIKE_DEBUG */

static void clear_markers(void)
{
  unsigned int e;
  for(e=0;e<NELEM(a_markers);e++)
  {
    if(a_markers[e])
    {
      free_type(a_markers[e]);
      a_markers[e]=0;
    }
    if(b_markers[e])
    {
      free_type(b_markers[e]);
      b_markers[e]=0;
    }
  }
}

struct pike_type *debug_pop_type(void)
{
  struct pike_type *t = pop_unfinished_type();
  TYPE_STACK_DEBUG("pop_type");
  type_stack_mark();
  return t;
}

struct pike_type *debug_compiler_pop_type(void)
{
  TYPE_STACK_DEBUG("compiler_pop_type");
  if(Pike_compiler->num_parse_error)
  {
    /* This could be fixed to check if the type
     * is correct and then return it, I just didn't feel
     * like writing the checking code today. / Hubbe
     */
    type_stack_pop_to_mark();
    type_stack_mark();
    add_ref(mixed_type_string);
    return mixed_type_string;
  }else{
    return debug_pop_type();
  }
}

char *get_name_of_type(int t)
{
  switch(t)
  {
    case T_ARRAY: return "array";
    case T_FLOAT: return "float";
    case T_FUNCTION: return "function";
    case T_INT: return "int";
    case T_LVALUE: return "lvalue";
    case T_MAPPING: return "mapping";
    case T_MULTISET: return "multiset";
    case T_OBJECT: return "object";
    case T_PROGRAM: return "program";
    case T_STRING: return "string";
    case T_TYPE: return "type";
    case T_ZERO: return "zero";
    case T_VOID: return "void";
    case T_STORAGE: return "object storage";
    case T_MAPPING_DATA: return "mapping_data";
    case T_PIKE_FRAME: return "pike_frame";
    case T_MULTISET_DATA: return "multiset_data";
    default: return "unknown";
  }
}


#define TWOT(X,Y) (((X) << 8)+(Y))

#ifdef USE_PIKE_TYPE

static int low_pike_types_le(struct pike_type *a, struct pike_type *b,
			     int array_cnt, unsigned int flags);
static int low_check_indexing(struct pike_type *type,
			      struct pike_type *index_type,
			      node *n);
static void internal_parse_type(char **s);

/*
 * New and improved type representation system.
 *
 * This representation is new in Pike 7.3.
 *
 * Node:	Car:		Cdr:
 * ---------------------------------------------
 * SCOPE	num vars (int)	type
 * ASSIGN	variable (int)	type
 * NAME		name (string)	type
 * FUNCTION	type		FUNCTION|MANY
 * MANY		many type	return type
 * RING		type		type
 * TUPLE	type		type
 * MAPPING	index type	value type
 * OR		type		type
 * AND		type		type
 * ARRAY	type		-
 * MULTISET	type		-
 * NOT		type		-
 * '0'-'9'	-		-
 * FLOAT	-		-
 * STRING	-		-
 * TYPE		type		-
 * PROGRAM	type		-
 * MIXED	-		-
 * VOID		-		-
 * ZERO		-		-
 * UNKNOWN	-		-
 * INT		min (int)	max (int)
 * OBJECT	implements/is	object id(int)
 *
 * Note that the cdr of a FUNCTION is a valid FUNCTION for the rest of
 * the arguments.
 *
 * Note also that functions that don't take any arguments, or just
 * a many argument just have a MANY node, and no FUNCTION node.
 *
 */
#define PIKE_TYPE_CHUNK	128
BLOCK_ALLOC(pike_type, PIKE_TYPE_CHUNK)

static struct pike_type **pike_type_hash = NULL;
static size_t pike_type_hash_size = 0;

void debug_free_type(struct pike_type *t)
{
 loop:
  if (!(--(((struct pike_type *)debug_malloc_pass(t))->refs))) {
    unsigned INT32 hash = t->hash % pike_type_hash_size;
    struct pike_type **t2 = pike_type_hash + hash;
    struct pike_type *car, *cdr;
    unsigned INT32 type;

    while (*t2) {
      if (*t2 == t) {
	*t2 = t->next;
	break;
      }
      t2 = &((*t2)->next);
    }

    car = t->car;
    cdr = t->cdr;
    type = t->type;

    really_free_pike_type((struct pike_type *)debug_malloc_pass(t));

    /* FIXME: Recursion: Should we use a stack? */
    switch(type) {
    case T_FUNCTION:
    case T_MANY:
    case T_TUPLE:
    case T_MAPPING:
    case T_OR:
    case T_AND:
    case PIKE_T_RING:
      /* Free car & cdr */
      free_type(car);
      t = (struct pike_type *)debug_malloc_pass(cdr);
      goto loop;

    case T_ARRAY:
    case T_MULTISET:
    case T_NOT:
    case T_TYPE:
    case T_PROGRAM:
      /* Free car */
      t = (struct pike_type *)debug_malloc_pass(car);
      goto loop;
	
    case T_SCOPE:
    case T_ASSIGN:
      /* Free cdr */
      t = (struct pike_type *)debug_malloc_pass(cdr);
      goto loop;

    case PIKE_T_NAME:
      free_string((struct pike_string *)car);
      t = (struct pike_type *)debug_malloc_pass(cdr);
      goto loop;

#ifdef PIKE_DEBUG
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case T_FLOAT:
    case T_STRING:
    case T_MIXED:
    case T_VOID:
    case T_ZERO:
    case PIKE_T_UNKNOWN:
    case T_INT:
    case T_OBJECT:
      break;

    default:
      fatal("free_type(): Unhandled type-node: %d\n", type);
      break;
#endif /* PIKE_DEBUG */
    }
  }
}

/* Flags used as flag_method: */
#define PT_COPY_CAR	1
#define PT_COPY_CDR	2
#define PT_COPY_BOTH	3
#define PT_SET_MARKER	4

static inline struct pike_type *debug_mk_type(unsigned INT32 type,
					      struct pike_type *car,
					      struct pike_type *cdr,
					      int flag_method)
{
  unsigned INT32 hash = DO_NOT_WARN((unsigned INT32)
				    ((ptrdiff_t)type*0x10204081)^
				    (0x8003*(ptrdiff_t)car)^
				    ~(0x10001*(ptrdiff_t)cdr));
  unsigned INT32 index = hash % pike_type_hash_size;
  struct pike_type *t;

  for(t = pike_type_hash[index]; t; t = t->next) {
    if ((t->hash == hash) && (t->type == type) &&
	(t->car == car) && (t->cdr == cdr)) {
      /* Free car & cdr as appropriate. */
      switch(type) {
      case T_FUNCTION:
      case T_MANY:
      case T_TUPLE:
      case T_MAPPING:
      case T_OR:
      case T_AND:
      case PIKE_T_RING:
	/* Free car & cdr */
	free_type((struct pike_type *)debug_malloc_pass(car));
	free_type((struct pike_type *)debug_malloc_pass(cdr));
	break;

      case T_ARRAY:
      case T_MULTISET:
      case T_NOT:
      case T_TYPE:
      case T_PROGRAM:
	/* Free car */
	free_type((struct pike_type *)debug_malloc_pass(car));
	break;
	
      case T_SCOPE:
      case T_ASSIGN:
	/* Free cdr */
	free_type((struct pike_type *)debug_malloc_pass(cdr));
	break;

      case PIKE_T_NAME:
	free_string((struct pike_string *)debug_malloc_pass(car));
	free_type((struct pike_type *)debug_malloc_pass(cdr));
	break;

#ifdef PIKE_DEBUG
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case T_FLOAT:
      case T_STRING:
      case T_MIXED:
      case T_VOID:
      case T_ZERO:
      case PIKE_T_UNKNOWN:
      case T_INT:
      case T_OBJECT:
	break;

      default:
	fatal("mk_type(): Unhandled type-node: %d\n", type);
	break;
#endif /* PIKE_DEBUG */
      }
      add_ref((struct pike_type *)debug_malloc_pass(t));
      return t;
    }
  }
      
  debug_malloc_pass(t = alloc_pike_type());

  t->refs = 1;
  t->type = type;
  t->flags = 0;
  t->car = car;
  t->cdr = cdr;

  t->hash = hash;
  t->next = pike_type_hash[index];
  pike_type_hash[index] = t;

  if (flag_method) {
    if (flag_method == PT_SET_MARKER) {
      t->flags = PT_FLAG_MARKER;
    } else {
      if (car && (flag_method & PT_COPY_CAR)) {
	t->flags = car->flags;
      }
      if (cdr && (flag_method & PT_COPY_CDR)) {
	t->flags |= cdr->flags;
      }
    }
  }

#ifdef DEBUG_MALLOC
  switch(type) {
  case T_FUNCTION:
  case T_MANY:
  case T_TUPLE:
  case T_MAPPING:
  case T_OR:
  case T_AND:
  case PIKE_T_RING:
    debug_malloc_pass(car);
    debug_malloc_pass(cdr);
    break;
    
  case T_ARRAY:
  case T_MULTISET:
  case T_NOT:
  case T_TYPE:
  case T_PROGRAM:
    debug_malloc_pass(car);
    break;
	
  case T_SCOPE:
  case T_ASSIGN:
    debug_malloc_pass(cdr);
    break;

  case PIKE_T_NAME:
    debug_malloc_pass(car);
    debug_malloc_pass(cdr);
    break;

  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case T_FLOAT:
  case T_STRING:
  case T_MIXED:
  case T_VOID:
  case T_ZERO:
  case PIKE_T_UNKNOWN:
  case T_INT:
  case T_OBJECT:
    break;

  default:
    fatal("mk_type(): Unhandled type-node: %d\n", type);
    break;
  }
#endif /* DEBUG_MALLOC */

  return t;
}

#ifdef DEBUG_MALLOC
#define mk_type(T,CAR,CDR,FLAG)	((struct pike_type *)debug_malloc_pass(debug_mk_type(T,CAR,CDR,FLAG)))
#else /* !DEBUG_MALLOC */
#define mk_type debug_mk_type
#endif /* DEBUG_MALLOC */

#ifdef PIKE_DEBUG
void debug_check_type_string(struct pike_type *s)
{
  /* FIXME: Add verification code here */
}

#endif /* PIKE_DEBUG */

struct pike_type *type_stack[PIKE_TYPE_STACK_SIZE];
struct pike_type **pike_type_mark_stack[PIKE_TYPE_STACK_SIZE/4];

ptrdiff_t pop_stack_mark(void)
{ 
  Pike_compiler->pike_type_mark_stackp--;
  if(Pike_compiler->pike_type_mark_stackp<pike_type_mark_stack)
    fatal("Type mark stack underflow\n");

  TYPE_STACK_DEBUG("pop_stack_mark");

  return Pike_compiler->type_stackp - *Pike_compiler->pike_type_mark_stackp;
}

void type_stack_pop_to_mark(void)
{
  pop_stack_mark();
  while(Pike_compiler->type_stackp > *Pike_compiler->pike_type_mark_stackp) {
    free_type(*(Pike_compiler->type_stackp--));
  }

  TYPE_STACK_DEBUG("type_stack_pop_to_mark");
}

struct pike_type *debug_peek_type_stack(void)
{
  return *(Pike_compiler->type_stackp);
}

void debug_push_int_type(INT32 min, INT32 max)
{
  *(++Pike_compiler->type_stackp) = mk_type(T_INT,
					    (void *)(ptrdiff_t)min,
					    (void *)(ptrdiff_t)max, 0);

  TYPE_STACK_DEBUG("push_int_type");
}

void debug_push_object_type(int flag, INT32 id)
{
  *(++Pike_compiler->type_stackp) = mk_type(T_OBJECT,
                                            (void *)(ptrdiff_t)flag,
                                            (void *)(ptrdiff_t)id, 0);

  TYPE_STACK_DEBUG("push_object_type");
}

void debug_push_object_type_backwards(int flag, INT32 id)
{
  push_object_type(flag, id);
}

void debug_push_scope_type(int level)
{
  *Pike_compiler->type_stackp = mk_type(T_SCOPE,
					(void *)(ptrdiff_t)level,
					*Pike_compiler->type_stackp,
					PT_COPY_CDR);

  TYPE_STACK_DEBUG("push_scope_type");
}

void debug_push_assign_type(int marker)
{
  marker -= '0';
#ifdef PIKE_DEBUG 
  if ((marker < 0) || (marker > 9)) {
    fatal("Bad assign marker: %ld\n", marker);
  }
#endif /* PIKE_DEBUG */

  *Pike_compiler->type_stackp = mk_type(T_ASSIGN,
					(void *)(ptrdiff_t)marker,
					*Pike_compiler->type_stackp,
					PT_COPY_CDR);
  TYPE_STACK_DEBUG("push_assign_type");
}

void debug_push_type_name(struct pike_string *name)
{
  /* fprintf(stderr, "push_type_name(\"%s\")\n", name->str); */
  add_ref(name);
  *Pike_compiler->type_stackp = mk_type(PIKE_T_NAME,
					(void *)name,
					*Pike_compiler->type_stackp,
					PT_COPY_CDR);
  TYPE_STACK_DEBUG("push_type_name");
}

void debug_push_finished_type(struct pike_type *t)
{
  copy_pike_type(*(++Pike_compiler->type_stackp), t);

  TYPE_STACK_DEBUG("push_finished_type");
}

void debug_push_type(unsigned INT16 type)
{
  /* fprintf(stderr, "push_type(%d)\n", type); */

  switch(type) {
  case T_FUNCTION:
  case T_MANY:
  case T_TUPLE:
  case T_MAPPING:
  case T_OR:
  case T_AND:
  case PIKE_T_RING:
    /* Make a new type of the top two types. */
    --Pike_compiler->type_stackp;
    *Pike_compiler->type_stackp = mk_type(type,
					  *(Pike_compiler->type_stackp+1),
					  *Pike_compiler->type_stackp,
					  PT_COPY_BOTH);
    break;

  case T_ARRAY:
  case T_MULTISET:
  case T_NOT:
  case T_TYPE:
  case T_PROGRAM:
    /* Make a new type of the top type, and put it in car. */
    *Pike_compiler->type_stackp = mk_type(type,
					  *Pike_compiler->type_stackp, NULL,
					  PT_COPY_CAR);
    break;

  case T_SCOPE:
  case T_ASSIGN:
  case T_INT:
  case T_OBJECT:
  case PIKE_T_NAME:
  default:
    /* Should not occurr. */
    fatal("Unsupported argument to push_type().\n");
    break;

  case T_FLOAT:
  case T_STRING:
  case T_MIXED:
  case T_VOID:
  case T_ZERO:
  case PIKE_T_UNKNOWN:
    /* Leaf type. */
    *(++Pike_compiler->type_stackp) = mk_type(type, NULL, NULL, 0);
    break;

  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    /* Marker. */
    *(++Pike_compiler->type_stackp) = mk_type(type, NULL, NULL, PT_SET_MARKER);
    break;
  }

  TYPE_STACK_DEBUG("push_type");
}

/* Pop one level of types. This is the inverse of push_type() */
void debug_pop_type_stack(unsigned INT16 expected)
{ 
  struct pike_type *top;
  if(Pike_compiler->type_stackp<type_stack)
    fatal("Type stack underflow\n");

  top = *(Pike_compiler->type_stackp);
  /* Special case... */
  if (top->type == T_MIXED) return;	/* Probably due to an earlier error */

  Pike_compiler->type_stackp--;
#ifdef PIKE_DEBUG
  if ((top->type != expected) && (top->type != PIKE_T_NAME)) {
    fatal("Unexpected type on stack: %d (expected %d)\n", top->type, expected);
  }
#endif /* PIKE_DEBUG */
  /* OPTIMIZE: It looks like this function is always called with
   * expected == T_ARRAY.
   */
  switch(top->type) {
  case T_FUNCTION:
  case T_MANY:
  case T_TUPLE:
  case T_MAPPING:
  case T_OR:
  case T_AND:
  case PIKE_T_RING:
    /* Both car & cdr. */
    push_finished_type(top->cdr);
    push_finished_type(top->car);
    break;
  case T_ARRAY:
  case T_MULTISET:
  case T_NOT:
  case T_TYPE:
  case T_PROGRAM:
    /* car */
    push_finished_type(top->car);
    break;
  case T_SCOPE:
  case T_ASSIGN:
    /* cdr */
    push_finished_type(top->cdr);
    break;
  case T_INT:
  case T_OBJECT:
  case T_FLOAT:
  case T_STRING:
  case T_MIXED:
  case T_VOID:
  case T_ZERO:
  case PIKE_T_UNKNOWN:
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    /* Leaf */
    break;
  case PIKE_T_NAME:
    /* Pop the name and recurse. */
    push_finished_type(top->cdr);
    pop_type_stack(expected);
    break;
  default:
    Pike_error("pop_type_stack(): Unhandled node type: %d\n", top->type);
  }
  free_type(top);

  TYPE_STACK_DEBUG("pop_type_stack");
}

void debug_push_reverse_type(unsigned INT16 type)
{
  /* fprintf(stderr, "push_reverse_type(%d)\n", type); */

  switch(type) {
  case T_FUNCTION:
  case T_MANY:
  case T_TUPLE:
  case T_MAPPING:
  case T_OR:
  case T_AND:
  case PIKE_T_RING:
    {
      /* Binary type-node. -- swap the types. */
      struct pike_type *tmp = Pike_compiler->type_stackp[0];
      Pike_compiler->type_stackp[0] = Pike_compiler->type_stackp[-1];
      Pike_compiler->type_stackp[-1] = tmp;
      break;
    }
  }
  push_type(type);

  TYPE_STACK_DEBUG("push_reverse_type");
}

void debug_push_finished_type_with_markers(struct pike_type *type,
					   struct pike_type **markers)
{
 recurse:
#if 0
  fprintf(stderr, "push_finished_type_with_markers((%d[%x]),...)...\n",
	  type->type, type->flags);
#endif /* 0 */
  if (!(type->flags & PT_FLAG_MARKER)) {
    /* No markers in this sub-tree */
#if 0
    fprintf(stderr, "No markers in this subtree.\n");
#endif /* 0 */
    push_finished_type(type);
    return;
  }
  if ((type->type >= '0') && (type->type <= '9')) {
    unsigned int m = type->type - '0';
    if (markers[m]) {
      type = markers[m];
#if 0
      fprintf(stderr, "Marker %d.\n", m);
#endif /* 0 */
      goto recurse;
    } else {
      push_type(T_ZERO);
    }
    TYPE_STACK_DEBUG("push_finished_type_with_markers");
    return;
  }
  if (type->type == T_ASSIGN) {
    /* Strip assign */
#if 0
    fprintf(stderr, "Assign to marker %d.\n", ((ptrdiff_t)type->car));
#endif /* 0 */
    type = type->cdr;
    goto recurse;
  }
  if (type->type == PIKE_T_NAME) {
    /* Strip the name, since it won't be correct anymore. */
    type = type->cdr;
    goto recurse;
  }
  /* FIXME: T_SCOPE */
  if (type->cdr) {
    push_finished_type_with_markers(type->cdr, markers);
  }
  /* In all other cases type->car will be a valid node. */
  push_finished_type_with_markers(type->car, markers);
  /* push_type has sufficient magic to recreate the type. */
  push_type(type->type);
  TYPE_STACK_DEBUG("push_finished_type_with_markers");
}

INT32 extract_type_int(char *p)
{
  int e;
  INT32 ret=0;
  for(e=0;e<(int)sizeof(INT32);e++)
    ret=(ret<<8) | EXTRACT_UCHAR(p+e);
  return ret;
}

struct pike_type *debug_pop_unfinished_type(void)
{
  ptrdiff_t len;

  len = pop_stack_mark();

  if (len != 1) {
    fatal("pop_unfinished_type(): Unexpected len: %d\n", len);
  }

  TYPE_STACK_DEBUG("pop_unfinished_type");

  return *(Pike_compiler->type_stackp--);
}

/******/

static void internal_parse_typeA(char **_s)
{
  char buf[80];
  unsigned int len;
  unsigned char **s = (unsigned char **)_s;
  
  while(ISSPACE(**s)) ++*s;

  len=0;
  for(len=0;isidchar(EXTRACT_UCHAR(s[0]+len));len++)
  {
    if(len>=sizeof(buf)) Pike_error("Buffer overflow in parse_type\n");
    buf[len] = s[0][len];
  }
  buf[len]=0;
  *s += len;
  
  switch(buf[0])
  {
    case 'z':
      if(!strcmp(buf,"zero")) { push_type(T_ZERO); break; }
      goto bad_type;

    case 'i':
      if(!strcmp(buf,"int"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s=='(')
	{
	  INT32 min,max;
	  ++*s;
	  while(ISSPACE(**s)) ++*s;
	  min=STRTOL((char *)*s,(char **)s,0);
	  while(ISSPACE(**s)) ++*s;
	  if(s[0][0]=='.' && s[0][1]=='.')
	    s[0]+=2;
	  else
	    Pike_error("Missing .. in integer type.\n");
	  
	  while(ISSPACE(**s)) ++*s;
	  max=STRTOL((char *)*s,(char **)s,0);
	  while(ISSPACE(**s)) ++*s;

	  if(**s != ')') Pike_error("Missing ')' in integer range.\n");
	  ++*s;
	  push_int_type(min, max);
	}else{
	  push_int_type(MIN_INT32, MAX_INT32);
	}
	break;
      }
      goto bad_type;

    case 'f':
      if(!strcmp(buf,"function"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  int nargs = 0;
	  ++*s;
	  while(ISSPACE(**s)) ++*s;
	  while(1)
	  {
	    if(**s == ':')
	    {
	      push_type(T_VOID);
	      break;
	    }
	    internal_parse_type(_s);
	    if(**s==',')
	    {
	      nargs++;
	      ++*s;
	      while(ISSPACE(**s)) ++*s;
	    }
	    else if(s[0][0]=='.' && s[0][1]=='.' && s[0][2]=='.')
	    {
	      *s+=3;
	      while(ISSPACE(**s)) ++*s;
	      if(**s != ':')
		Pike_error("Missing ':' after ... in function type.\n");
	      break;
	    } else {
	      nargs++;
	    }
	  }
	  ++*s;
	  internal_parse_type(_s);  /* return type */
	  push_reverse_type(T_MANY);

	  while (nargs-- > 0) {
	    push_reverse_type(T_FUNCTION);
	  }

	  if(**s != ')') Pike_error("Missing ')' in function type.\n");
	  ++*s;
	}else{
	  push_type(T_VOID);
	  push_type(T_MIXED);
	  push_type(T_OR);
	  push_type(T_VOID);
	  push_type(T_ZERO);
	  push_type(T_OR);
	  push_type(T_MANY);
	}
	break;
      }
      if(!strcmp(buf,"float")) { push_type(T_FLOAT); break; }
      goto bad_type;

    case 'o':
      if(!strcmp(buf,"object"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(') /* object({is,implements} id) */
	{
	  int is = 1, id;
	  ++*s;
	  if( **s != 'i' )
	    goto bad_type;
	  ++*s;
	  if( **s == 'm' )
	    is = 0;
	  while( isidchar( **s ) ) ++*s;
	  while( ISSPACE(**s) )      ++*s;
	  if( !**s )
	    goto bad_type;
	  id = atoi( *s );	
	  while( **s >= '0' && **s <= '9' )
	    ++*s;
	  while(ISSPACE(**s)) ++*s;
	  if( !**s || **s != ')' )
	    goto bad_type;
	  ++*s;
	  push_object_type(is, id);
	}
	else
	  push_object_type(0, 0);
	break;
      }
      goto bad_type;


    case 'p':
      if(!strcmp(buf,"program")) {
	push_object_type(0, 0);
	push_type(T_PROGRAM);
	break;
      }
      goto bad_type;


    case 's':
      if(!strcmp(buf,"string")) { push_type(T_STRING); break; }
      goto bad_type;

    case 'v':
      if(!strcmp(buf,"void")) { push_type(T_VOID); break; }
      goto bad_type;

    case 't':
      if (!strcmp(buf,"tuple"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  ++*s;
	  internal_parse_type(_s);
	  if(**s != ',') Pike_error("Expecting ','.\n");
	  ++*s;
	  internal_parse_type(_s);
	  if(**s != ')') Pike_error("Expecting ')'.\n");
	  ++*s;
	}else{
	  push_type(T_MIXED);
	  push_type(T_MIXED);
	}
	push_reverse_type(T_TUPLE);
	break;
      }
      /* FIXME: Handle type(T) */
      if(!strcmp(buf,"type")) { push_type(T_MIXED); push_type(T_TYPE); break; }
      goto bad_type;

    case 'm':
      if(!strcmp(buf,"mixed")) { push_type(T_MIXED); break; }
      if(!strcmp(buf,"mapping"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  ++*s;
	  internal_parse_type(_s);
	  if(**s != ':') Pike_error("Expecting ':'.\n");
	  ++*s;
	  internal_parse_type(_s);
	  if(**s != ')') Pike_error("Expecting ')'.\n");
	  ++*s;
	}else{
	  push_type(T_MIXED);
	  push_type(T_MIXED);
	}
	push_reverse_type(T_MAPPING);
	break;
      }
      if(!strcmp(buf,"multiset"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  ++*s;
	  internal_parse_type(_s);
	  if(**s != ')') Pike_error("Expecting ')'.\n");
	  ++*s;
	}else{
	  push_type(T_MIXED);
	}
	push_type(T_MULTISET);
	break;
      }
      goto bad_type;

    case 'u':
      if(!strcmp(buf,"unknown")) { push_type(PIKE_T_UNKNOWN); break; }
      goto bad_type;

    case 'a':
      if(!strcmp(buf,"array"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  ++*s;
	  internal_parse_type(_s);
	  if(**s != ')') Pike_error("Expecting ')'.\n");
	  ++*s;
	}else{
	  push_type(T_MIXED);
	}
	push_type(T_ARRAY);
	break;
      }
      goto bad_type;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if(atoi(buf)<10)
      {
	while(ISSPACE(**s)) ++*s;
	if(**s=='=')
	{
	  ++*s;
	  internal_parse_type(_s);
	  push_assign_type(buf[0]);
	}else{
	  push_type(buf[0]);
	}
	break;
      }

    default:
  bad_type:
      Pike_error("Couldn't parse type. (%s)\n",buf);
  }

  while(ISSPACE(**s)) ++*s;
}


static void internal_parse_typeB(char **s)
{
  while(ISSPACE(**((unsigned char **)s))) ++*s;
  switch(**s)
  {
  case '!':
    ++*s;
    internal_parse_typeB(s);
    push_type(T_NOT);
    break;

  case '(':
    ++*s;
    internal_parse_type(s);
    while(ISSPACE(**((unsigned char **)s))) ++*s;
    if(**s != ')') Pike_error("Expecting ')'.\n");
    ++*s;
    break;
    
  default:

    internal_parse_typeA(s);
  }
}

static void internal_parse_typeCC(char **s)
{
  internal_parse_typeB(s);

  while(ISSPACE(**((unsigned char **)s))) ++*s;
  
  while(**s == '*')
  {
    ++*s;
    while(ISSPACE(**((unsigned char **)s))) ++*s;
    push_type(T_ARRAY);
  }
}

static void internal_parse_typeC(char **s)
{
  internal_parse_typeCC(s);

  if(**s == '&')
  {
    ++*s;
    internal_parse_typeC(s);
    push_reverse_type(T_AND);
  }
}

static void internal_parse_type(char **s)
{
  internal_parse_typeC(s);

  while(**s == '|')
  {
    ++*s;
    internal_parse_typeC(s);
    push_type(T_OR);
  }
}

/* This function is used when adding simul efuns so that
 * the types for the functions can be easily stored in strings.
 * It takes a string on the exact same format as Pike and returns a type
 * struct.
 */
struct pike_type *parse_type(char *s)
{
  struct pike_type *ret;
#ifdef PIKE_DEBUG
  struct pike_type **ts=Pike_compiler->type_stackp;
  struct pike_type ***ptms=Pike_compiler->pike_type_mark_stackp;
#endif

  /* fprintf(stderr, "parse_type(\"%s\")...\n", s); */

  TYPE_STACK_DEBUG("parse_type");

  type_stack_mark();
  internal_parse_type(&s);

  if( *s )
    fatal("Extra junk at end of type definition.\n");

  ret=pop_unfinished_type();

#ifdef PIKE_DEBUG
  if(ts!=Pike_compiler->type_stackp || ptms!=Pike_compiler->pike_type_mark_stackp)
    fatal("Type stack whacked in parse_type.\n");
#endif

  return ret;
}

#ifdef PIKE_DEBUG
/* FIXME: */
void stupid_describe_type_string(char *a, ptrdiff_t len)
{
  ptrdiff_t e;
  for(e=0;e<len;e++)
  {
    if(e) fprintf(stderr, " ");
    switch(EXTRACT_UCHAR(a+e))
    {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	fprintf(stderr, "%c",EXTRACT_UCHAR(a+e));
	break;

      case T_SCOPE: fprintf(stderr, "scope"); break;
      case T_TUPLE: fprintf(stderr, "tuple"); break;
      case T_ASSIGN: fprintf(stderr, "="); break;
      case T_INT:
	{
	  INT32 min=extract_type_int(a+e+1);
	  INT32 max=extract_type_int(a+e+1+sizeof(INT32));
	  fprintf(stderr, "int");
	  if(min!=MIN_INT32 || max!=MAX_INT32)
	    fprintf(stderr, "(%ld..%ld)",(long)min,(long)max);
	  e+=sizeof(INT32)*2;
	  break;
	}
      case T_FLOAT: fprintf(stderr, "float"); break;
      case T_STRING: fprintf(stderr, "string"); break;
      case T_TYPE: fprintf(stderr, "type"); break;
      case T_PROGRAM: fprintf(stderr, "program"); break;
      case T_OBJECT:
	fprintf(stderr, "object(%s %ld)",
	       EXTRACT_UCHAR(a+e+1)?"is":"implements",
	       (long)extract_type_int(a+e+2));
	e+=sizeof(INT32)+1;
	break;
      case T_FUNCTION: fprintf(stderr, "function"); break;
      case T_ARRAY: fprintf(stderr, "array"); break;
      case T_MAPPING: fprintf(stderr, "mapping"); break;
      case T_MULTISET: fprintf(stderr, "multiset"); break;
	
      case PIKE_T_UNKNOWN: fprintf(stderr, "unknown"); break;
      case T_MANY: fprintf(stderr, "many"); break;
      case T_OR: fprintf(stderr, "or"); break;
      case T_AND: fprintf(stderr, "and"); break;
      case T_NOT: fprintf(stderr, "not"); break;
      case T_VOID: fprintf(stderr, "void"); break;
      case T_ZERO: fprintf(stderr, "zero"); break;
      case T_MIXED: fprintf(stderr, "mixed"); break;
	
      default: fprintf(stderr, "%d",EXTRACT_UCHAR(a+e)); break;
    }
  }
  fprintf(stderr, "\n");
}

void simple_describe_type(struct pike_type *s)
{
  if (s) {
    switch(s->type) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	fprintf(stderr, "%d", s->type-'0');
	break;

      case PIKE_T_NAME:
	fprintf(stderr, "{ %s = ", ((struct pike_string *)s->car)->str);
	simple_describe_type(s->cdr);
	fprintf(stderr, " }");
	break;

      case T_SCOPE:
	fprintf(stderr, "scope(%ld, ", (long)(ptrdiff_t)s->car);
	simple_describe_type(s->cdr);
	fprintf(stderr, ")");
	break;
      case T_TUPLE:
	fprintf(stderr, "tuple(");
	simple_describe_type(s->car);
	fprintf(stderr, ", ");
	simple_describe_type(s->cdr);
	fprintf(stderr, ")");	
	break;
      case T_ASSIGN:
	fprintf(stderr, "%ld = ", (long)(ptrdiff_t)s->car);
	simple_describe_type(s->cdr);
	break;
      case T_INT:
	{
	  INT32 min = (ptrdiff_t)s->car;
	  INT32 max = (ptrdiff_t)s->cdr;
	  fprintf(stderr, "int");
	  if(min!=MIN_INT32 || max!=MAX_INT32)
	    fprintf(stderr, "(%ld..%ld)",(long)min,(long)max);
	  break;
	}
      case T_FLOAT: fprintf(stderr, "float"); break;
      case T_STRING: fprintf(stderr, "string"); break;
      case T_TYPE:
	fprintf(stderr, "type(");
	simple_describe_type(s->car);
	fprintf(stderr, ")");
	break;
      case T_PROGRAM:
	fprintf(stderr, "program(");
	simple_describe_type(s->car);
	fprintf(stderr, ")");
	break;
      case T_OBJECT:
	fprintf(stderr, "object(%s %ld)",
	       s->car?"is":"implements",
	       (long)(ptrdiff_t)s->cdr);
	break;
      case T_FUNCTION:
      case T_MANY:
	fprintf(stderr, "function(");
	while(s->type == T_FUNCTION) {
	  simple_describe_type(s->car);
	  s = s->cdr;
	  if ((s->type == T_FUNCTION) ||
	      (s->car->type != T_VOID)) {
	    fprintf(stderr, ", ");
	  }
	}
	if (s->car->type != T_VOID) {
	  simple_describe_type(s->car);
	  fprintf(stderr, "...");
	}
	fprintf(stderr, ":");
	simple_describe_type(s->cdr);
	fprintf(stderr, ")");
	break;
      case T_ARRAY:
	fprintf(stderr, "array(");
	simple_describe_type(s->car);
	fprintf(stderr, ")");
	break;
      case T_MAPPING:
	fprintf(stderr, "mapping(");
	simple_describe_type(s->car);
	fprintf(stderr, ":");
	simple_describe_type(s->cdr);
	fprintf(stderr, ")");
	break;
      case T_MULTISET:
	fprintf(stderr, "multiset(");
	simple_describe_type(s->car);
	fprintf(stderr, ")");
	break;
	
      case PIKE_T_UNKNOWN: fprintf(stderr, "unknown"); break;
      case PIKE_T_RING:
	fprintf(stderr, "ring(");
	simple_describe_type(s->car);
	fprintf(stderr, "�");
	simple_describe_type(s->cdr);
	fprintf(stderr, ")");
	break;
      case T_OR:
	fprintf(stderr, "or(");
	simple_describe_type(s->car);
	fprintf(stderr, "|");
	simple_describe_type(s->cdr);
	fprintf(stderr, ")");
	break;
      case T_AND:
	fprintf(stderr, "and(");
	simple_describe_type(s->car);
	fprintf(stderr, "&");
	simple_describe_type(s->cdr);
	fprintf(stderr, ")");
	break;
      case T_NOT:
	fprintf(stderr, "not(");
	simple_describe_type(s->car);
	fprintf(stderr, ")");
	break;
      case T_VOID: fprintf(stderr, "void"); break;
      case T_ZERO: fprintf(stderr, "zero"); break;
      case T_MIXED: fprintf(stderr, "mixed"); break;
	
      default:
	fprintf(stderr, "Unknown type node: %d, %p:%p",
	       s->type, s->car, s->cdr);
	break;
    }
  } else {
    fprintf(stderr, "NULL");
  }
}

#ifdef DEBUG_MALLOC
void describe_all_types(void)
{
  unsigned INT32 index;

  for(index = 0; index < pike_type_hash_size; index++) {
    struct pike_type *t;
    for (t = pike_type_hash[index]; t; t = t->next) {
      if (t->refs) {
	fprintf(stderr, "Type at 0x%p: ", t);
	simple_describe_type(t);
	fprintf(stderr, " (refs:%ld)\n", (long)t->refs);
      }
    }
  }
}
#endif /* DEBUG_MALLOC */
#endif

static void low_describe_type(struct pike_type *t)
{
  /**** FIXME: ****/
  switch(t->type)
  {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      my_putchar(t->type);
      break;
      
    case T_ASSIGN:
      my_putchar('(');
      my_putchar('0' + (ptrdiff_t)t->car);
      my_putchar('=');
      my_describe_type(t->cdr);
      my_putchar(')');
      break;

    case T_SCOPE:
      my_putchar('{');
      my_putchar((ptrdiff_t)t->car);
      my_putchar(',');
      my_describe_type(t->cdr);
      my_putchar('}');
      break;

    case T_TUPLE:
      my_putchar('[');
      my_describe_type(t->car);
      my_putchar(',');
      my_describe_type(t->cdr);
      my_putchar(']');
      break;

    case T_VOID: my_strcat("void"); break;
    case T_ZERO: my_strcat("zero"); break;
    case T_MIXED: my_strcat("mixed"); break;
    case PIKE_T_UNKNOWN: my_strcat("unknown"); break;
    case T_INT:
    {
      INT32 min=(ptrdiff_t)t->car;
      INT32 max=(ptrdiff_t)t->cdr;
      my_strcat("int");
      
      if(min!=MIN_INT32 || max!=MAX_INT32)
      {
	char buffer[100];
	sprintf(buffer,"(%ld..%ld)",(long)min,(long)max);
	my_strcat(buffer);
      }
      break;
    }
    case T_FLOAT: my_strcat("float"); break;
    case T_PROGRAM:
      my_strcat("program(");
      my_describe_type(t->car);
      my_strcat(")");
      break;
    case T_OBJECT:
      if (t->cdr)
      {
	char buffer[100];
	sprintf(buffer,"object(%s %ld)",
		t->car?"is":"implements",
		(long)(ptrdiff_t)t->cdr);
	my_strcat(buffer);
      }else{
	my_strcat("object");
      }
      break;

    case T_STRING: my_strcat("string"); break;
    case T_TYPE:
      my_strcat("type(");
      my_describe_type(t->car);
      my_strcat(")");
      break;

    case PIKE_T_NAME:
      if (!((struct pike_string *)t->car)->size_shift) {
	my_strcat("{ ");
	my_binary_strcat(((struct pike_string *)t->car)->str,
			 ((struct pike_string *)t->car)->len);
	my_strcat(" = ");
	my_describe_type(t->cdr);
	my_strcat(" }");
      } else {
	my_describe_type(t->cdr);
      }
      break;
      
    case T_FUNCTION:
    case T_MANY:
    {
      int s;
      my_strcat("function");
      if(t->type == T_MANY &&
	 t->cdr->type == T_OR &&
	 ((t->cdr->car->type == T_MIXED && t->cdr->cdr->type == T_VOID) ||
	  (t->cdr->cdr->type == T_MIXED && t->cdr->car->type == T_VOID)) &&
	 (t->car->type == T_ZERO ||
	  (t->car->type == T_OR &&
	   ((t->car->car->type == T_ZERO && t->car->cdr->type == T_VOID) ||
	    (t->car->cdr->type == T_ZERO && t->car->car->type == T_VOID)))))
      {
	/* function == function(zero...:mixed|void) or
	 *             function(zero|void...:mixed|void)
	 */
	/* done */
      } else {
	my_strcat("(");
	s=0;
	while(t->type != T_MANY)
	{
	  if(s++) my_strcat(", ");
	  my_describe_type(t->car);
	  t = t->cdr;
	}
	if(t->car->type != T_VOID)
	{
	  if(s++) my_strcat(", ");
	  my_describe_type(t->car);
	  my_strcat(" ...");
	}
	my_strcat(" : ");
	my_describe_type(t->cdr);
	my_strcat(")");
      }
      break;
    }
    
    case T_ARRAY:
      my_strcat("array");
      if(t->car->type != T_MIXED) {
	my_strcat("(");
	my_describe_type(t->car);
	my_strcat(")");
      }
      break;
      
    case T_MULTISET:
      my_strcat("multiset");
      if(t->car->type != T_MIXED) {
	my_strcat("(");
	my_describe_type(t->car);
	my_strcat(")");
      }
      break;
      
    case T_NOT:
      my_strcat("!");
      if (t->car->type > T_NOT) {
	my_strcat("(");
	my_describe_type(t->car);
	my_strcat(")");
      } else {
	my_describe_type(t->car);
      }
      break;

    case PIKE_T_RING:
      /* FIXME: Should be renumbered for correct parenthesing. */
      my_strcat("(");
      my_describe_type(t->car);
      my_strcat(")�(");
      my_describe_type(t->cdr);
      my_strcat(")");
      break;
      
    case T_OR:
      if (t->car->type > T_OR) {
	my_strcat("(");
	my_describe_type(t->car);
	my_strcat(")");
      } else {
	my_describe_type(t->car);
      }
      my_strcat(" | ");
      if (t->cdr->type > T_OR) {
	my_strcat("(");
	my_describe_type(t->cdr);
	my_strcat(")");
      } else {
	my_describe_type(t->cdr);
      }
      break;
      
    case T_AND:
      if (t->car->type > T_AND) {
	my_strcat("(");
	my_describe_type(t->car);
	my_strcat(")");
      } else {
	my_describe_type(t->car);
      }
      my_strcat(" & ");
      if (t->cdr->type > T_AND) {
	my_strcat("(");
	my_describe_type(t->cdr);
	my_strcat(")");
      } else {
	my_describe_type(t->cdr);
      }
      break;
      
    case T_MAPPING:
      my_strcat("mapping");
      if(t->car->type != T_MIXED || t->cdr->type != T_MIXED) {
	my_strcat("(");
	my_describe_type(t->car);
	my_strcat(":");
	my_describe_type(t->cdr);
	my_strcat(")");
      }
      break;
    default:
      {
	char buf[20];
	my_strcat("unknown code(");
	sprintf(buf, "%d", t->type);
	my_strcat(buf);
	my_strcat(")");
	break;
      }
  }
}

void my_describe_type(struct pike_type *type)
{
  low_describe_type(type);
}

struct pike_string *describe_type(struct pike_type *type)
{
  check_type_string(type);
  if(!type) return make_shared_string("mixed");
  init_buf();
  low_describe_type(type);
  return free_buf();
}


/******/

static int low_is_same_type(struct pike_type *a, struct pike_type *b)
{
  return a == b;
}

TYPE_T compile_type_to_runtime_type(struct pike_type *t)
{
  switch(t->type)
  {
  case PIKE_T_RING:
    return compile_type_to_runtime_type(t->car);

  case T_OR:
    {
      TYPE_T tmp = compile_type_to_runtime_type(t->car);
      if(tmp == compile_type_to_runtime_type(t->cdr))
	return tmp;

      /* FALL_THROUGH */
    }
  case T_TUPLE:
    /* FIXME: Shouldn't occur/should be converted to array? */
    /* FALL_THROUGH */
  default:
    return T_MIXED;

  case T_ZERO:
    return T_INT;

  case T_SCOPE:
  case PIKE_T_NAME:
    return compile_type_to_runtime_type(t->cdr);

  case T_MANY:
    return T_FUNCTION;

  case T_ARRAY:
  case T_MAPPING:
  case T_MULTISET:

  case T_OBJECT:
  case T_PROGRAM:
  case T_FUNCTION:
    
  case T_STRING:
  case T_TYPE:
  case T_INT:
  case T_FLOAT:
    return t->type;
  }
}


static int low_find_exact_type_match(struct pike_type *needle,
				     struct pike_type *haystack,
				     unsigned int separator)
{
  while(haystack->type == separator)
  {
    if(low_find_exact_type_match(needle, haystack->car, separator))
      return 1;
    haystack = haystack->cdr;
  }
  return low_is_same_type(needle, haystack);
}

static void very_low_or_pike_types(struct pike_type *to_push,
				   struct pike_type *not_push)
{
  while(to_push->type == T_OR)
  {
    very_low_or_pike_types(to_push->car, not_push);
    to_push = to_push->cdr;
  }
  /* FIXME:
   * this might use the 'le' operator
   */

  if(!low_find_exact_type_match(to_push, not_push, T_OR))
  {
    push_finished_type(to_push);
    push_type(T_OR);
  }
}

static void low_or_pike_types(struct pike_type *t1,
			      struct pike_type *t2,
			      int zero_implied)
{
  if(!t1)
  {
    if(!t2)
      push_type(T_VOID);
    else
      push_finished_type(t2);
  }
  else if((!t2)
	  || (t2->type == T_ZERO && zero_implied)

    )
  {
    push_finished_type(t1);
  }
  else if (t1->type == T_ZERO && zero_implied)
  {
    push_finished_type(t2);
  }
  else if(t1->type == T_MIXED || t2->type == T_MIXED)
  {
    push_type(T_MIXED);
  }
  else if(t1->type == T_INT && t2->type == T_INT)
  {
    INT32 min, max;

    if ((((ptrdiff_t)t1->cdr) + 1 < (ptrdiff_t)t2->car) ||
	(((ptrdiff_t)t2->cdr) + 1 < (ptrdiff_t)t1->car)) {
      /* No overlap. */
      push_finished_type(t1);
      push_finished_type(t2);
      push_type(T_OR);
    } else {
      /* Overlap */
      min = MINIMUM((ptrdiff_t)t1->car, (ptrdiff_t)t2->car);
      max = MAXIMUM((ptrdiff_t)t1->cdr, (ptrdiff_t)t2->cdr);

      push_int_type(min, max);
    }
  }
  else if (t1->type == T_SCOPE)
  {
    if (t2->type == T_SCOPE) {
      low_or_pike_types(t1->cdr, t2->cdr, zero_implied);
      if (t1->car > t2->car)
	push_scope_type((ptrdiff_t)t1->car);
      else
	push_scope_type((ptrdiff_t)t2->car);
    } else {
      low_or_pike_types(t1->cdr, t2, zero_implied);
      push_scope_type((ptrdiff_t)t1->car);
    }
  }
  else if (t2->type == T_SCOPE)
  {
    low_or_pike_types(t1, t2->cdr, zero_implied);
    push_scope_type((ptrdiff_t)t2->car);
    push_type(T_SCOPE);
  }
  else
  {
    push_finished_type(t1);
    very_low_or_pike_types(t2,t1);
  }
}

struct pike_type *or_pike_types(struct pike_type *a,
				struct pike_type *b,
				int zero_implied)
{
  type_stack_mark();
  low_or_pike_types(a,b,1 /*zero_implied*/);
  return pop_unfinished_type();
}

static void very_low_and_pike_types(struct pike_type *to_push,
				    struct pike_type *not_push)
{
  while(to_push->type == T_AND)
  {
    very_low_and_pike_types(to_push->car, not_push);
    to_push = to_push->cdr;
  }
  if(!low_find_exact_type_match(to_push, not_push, T_AND))
  {
    push_finished_type(to_push);
    push_type(T_AND);
  }
}

static void even_lower_and_pike_types(struct pike_type *t1,
				      struct pike_type *t2)
{
  while(t2->type == T_OR)
  {
    even_lower_and_pike_types(t1, t2->car);
    t2 = t2->cdr;
  }
  if (t1->type == t2->type) {
    if (t1->type == T_INT) {
      INT32 i1, i2;
      INT32 upper_bound, lower_bound;
      i1 = (ptrdiff_t)t1->cdr;
      i2 = (ptrdiff_t)t2->cdr;
      upper_bound = MINIMUM(i1,i2);

      i1 = (ptrdiff_t)t1->car;
      i2 = (ptrdiff_t)t2->car;
      lower_bound = MAXIMUM(i1,i2);

      if (upper_bound >= lower_bound) {
	push_int_type(lower_bound, upper_bound);
	push_type(T_OR);
      }
    } else {
      push_finished_type(t1);
      push_type(T_OR);
    }
  }
}

static int lower_and_pike_types(struct pike_type *t1, struct pike_type *t2)
{
  int is_complex = 0;
  while(t1->type == T_OR)
  {
    is_complex |= lower_and_pike_types(t1->car, t2);
    t1 = t1->cdr;
  }
  switch(t1->type) {
  case T_ZERO:
  case T_VOID:
    break;
  case T_STRING:
  case T_FLOAT:
  case T_INT:
    even_lower_and_pike_types(t1, t2);
    break;
  default:
    return 1;
  }
  return is_complex;
}

static int low_and_push_complex_pike_type(struct pike_type *type)
{
  int is_complex = 0;
  while(type->type == T_OR)
  {
    int new_complex;
    new_complex = low_and_push_complex_pike_type(type->car);
    if (new_complex) {
      if (is_complex) {
	push_type(T_OR);
      } else {
	is_complex = 1;
      }
    }
    type = type->cdr;
  }
  switch(type->type) {
  case T_VOID:
  case T_ZERO:
  case T_STRING:
  case T_FLOAT:
  case T_INT:
    /* Simple type. Already handled. */
    break;
  default:
    push_finished_type(type);
    if (is_complex) {
      push_type(T_OR);
    }
    return 1;
  }
  return is_complex;
}

static void low_and_pike_types(struct pike_type *t1,
			       struct pike_type *t2)
{
  if(!t1 || t1->type == T_VOID ||
     !t2 || t2->type == T_VOID)
  {
    push_type(T_VOID);
  }
  else if(t1->type == T_ZERO ||
	  t2->type == T_ZERO)
  {
    push_type(T_ZERO);
  }
  else if(t1->type == T_MIXED)
  {
    push_finished_type(t2);
  }
  else if(t2->type == T_MIXED)
  {
    push_finished_type(t1);
  }
  else if(t1->type == T_INT && t2->type == T_INT)
  {
    INT32 i1,i2;
    INT32 upper_bound, lower_bound;
    i1 = (ptrdiff_t)t1->cdr;
    i2 = (ptrdiff_t)t2->cdr;
    upper_bound = MINIMUM(i1,i2);

    i1 = (ptrdiff_t)t1->car;
    i2 = (ptrdiff_t)t2->car;
    lower_bound = MAXIMUM(i1,i2);

    if (upper_bound >= lower_bound) {
      push_int_type(lower_bound, upper_bound);
    } else {
      /* No overlap! */
      /* FIXME: Warn? */
      push_type(T_VOID);
    }
  }
  else if (t1->type == T_SCOPE)
  {
    if (t2->type == T_SCOPE) {
      low_and_pike_types(t1->cdr, t2->cdr);
      if (t1->car > t2->car)
	push_scope_type((ptrdiff_t)t1->car);
      else
	push_scope_type((ptrdiff_t)t2->car);
    } else {
      low_and_pike_types(t1->cdr, t2);
      push_scope_type((ptrdiff_t)t1->car);
    }
  }
  else if (t2->type == T_SCOPE)
  {
    low_and_pike_types(t1, t2->cdr);
    push_scope_type((ptrdiff_t)t2->car);
  }
  else if((t1->type == t2->type) &&
	  ((t1->type == T_STRING) ||
	   (t1->type == T_FLOAT)))
  {
    push_finished_type(t1);
  }
  else if(low_pike_types_le(t1, t2, 0, 0))
  {
    push_finished_type(t1);
  }
  else if(low_pike_types_le(t2, t1, 0, 0))
  {
    push_finished_type(t2);
  }
  else
  {
    push_type(T_ZERO);

    if (lower_and_pike_types(t1, t2)) {
      /* t1 contains complex types. */
      if (low_and_push_complex_pike_type(t2)) {
	/* t2 also contains complex types. */
	low_and_push_complex_pike_type(t1);
	push_type(T_AND);
	push_type(T_OR);
      }
    }
    /*     push_finished_type(t1); */
    /*     very_low_and_pike_types(t2,t1); */
  }
}

struct pike_type *and_pike_types(struct pike_type *a,
				 struct pike_type *b)
{
  type_stack_mark();
  low_and_pike_types(a, b);
  return pop_unfinished_type();
}

static struct pike_type *low_object_lfun_type(struct pike_type *t, short lfun)
{
  struct program *p;
  int i;
  p = id_to_program((ptrdiff_t)t->cdr);
  if(!p) return 0;
  i=FIND_LFUN(p, lfun);
  if(i==-1) return 0;
  return ID_FROM_INT(p, i)->type;
}



/******/

/*
 * match two type strings, return zero if they don't match, and return
 * the part of 'a' that _did_ match if it did.
 */
static struct pike_type *low_match_types(struct pike_type *a,
					 struct pike_type *b,
					 int flags)
#ifdef PIKE_TYPE_DEBUG
{
  int e;
  char *s;
  static struct pike_type *low_match_types2(struct pike_type *a,
					    struct pike_type *b,
					    int flags);

  if (l_flag>2) {
    init_buf();
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("low_match_types(");
    my_describe_type(a);
    my_strcat(",\n");
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("                ");
    my_describe_type(b);
    my_strcat(",\n");
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("                ");

    if (flags) {
      int f = 0;
      if (flags & A_EXACT) {
	my_strcat("A_EXACT");
	f = 1;
      }
      if (flags & B_EXACT) {
	if (f) {
	  my_strcat(" | ");
	}
	my_strcat("B_EXACT");
	f = 1;
      }
      if (flags & NO_MAX_ARGS) {
	if (f) {
	  my_strcat(" | ");
	}
	my_strcat("NO_MAX_ARGS");
	f = 1;
      }
      if (flags & NO_SHORTCUTS) {
	if (f) {
	  my_strcat(" | ");
	}
	my_strcat("NO_SHORTCUTS");
	f = 1;
      }
    } else {
      my_strcat("0");
    }
    my_strcat(");\n");
    fprintf(stderr,"%s",(s=simple_free_buf()));
    free(s);
    indent++;
  }

  a = low_match_types2(a, b, flags);

  if (l_flag>2) {
    indent--;
    init_buf();
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("= ");
    if(a)
      my_describe_type(a);
    else
      my_strcat("NULL");
    my_strcat("\n");
    fprintf(stderr,"%s",(s=simple_free_buf()));
    free(s);
  }
  return a;
}

static struct pike_type *low_match_types2(struct pike_type *a,
					  struct pike_type *b,
					  int flags)
#endif

{
  int correct_args;
  struct pike_type *ret;
  if(a == b) return a;

  switch(a->type)
  {
  case T_AND:
    ret = low_match_types(a->car, b, flags);
    if(!ret) return 0;
    return low_match_types(a->cdr, b, flags);

  case T_OR:
    ret = low_match_types(a->car, b, flags);
    if(ret && !(flags & NO_SHORTCUTS)) return ret;
    if(ret)
    {
      low_match_types(a->cdr, b, flags);
      return ret;
    }else{
      return low_match_types(a->cdr, b, flags);
    }

  case PIKE_T_RING:
    return low_match_types(a->car, b, flags);

  case PIKE_T_NAME:
    return low_match_types(a->cdr, b, flags);

  case T_NOT:
    if(low_match_types(a->car, b, (flags ^ B_EXACT ) | NO_MAX_ARGS))
      return 0;
    return a;

    case T_ASSIGN:
      ret = low_match_types(a->cdr, b, flags);
      if(ret && (b->type != T_VOID))
      {
	int m = (ptrdiff_t)a->car;
	struct pike_type *tmp;

#ifdef PIKE_DEBUG
	if ((m < 0) || (m > 9)) {
	  fatal("marker out of range: %d\n", m);
	}
#endif /* PIKE_DEBUG */

	type_stack_mark();
	push_finished_type_with_markers(b, b_markers);
	tmp = pop_unfinished_type();

	type_stack_mark();
	low_or_pike_types(a_markers[m], tmp, 0);
	if(a_markers[m]) free_type(a_markers[m]);
	free_type(tmp);
	a_markers[m] = pop_unfinished_type();

#ifdef PIKE_TYPE_DEBUG
	if (l_flag>2) {
	  char *s;
	  int e;
	  init_buf();
	  for(e=0;e<indent;e++) my_strcat("  ");
	  my_strcat("a_markers[");
	  my_putchar((char)(m+'0'));
	  my_strcat("]=");
	  my_describe_type(a_markers[m]);
	  my_strcat("\n");
	  fprintf(stderr,"%s",(s=simple_free_buf()));
	  free(s);
	}
#endif
#ifdef PIKE_DEBUG
	if((ptrdiff_t)a_markers[m]->type == m+'0')
	  fatal("Cyclic type!\n");
#endif
      }
      return ret;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    {
      int m = a->type - '0';
      if(a_markers[m])
      {
#ifdef PIKE_DEBUG
	if(a_markers[m]->type == a->type)
	  fatal("Cyclic type!\n");
	if(a_markers[m]->type == T_OR &&
	   a_markers[m]->car->type == a->type)
	  fatal("Cyclic type!\n");
#endif
	return low_match_types(a_markers[m], b, flags);
      }
      else
	return low_match_types(mixed_type_string, b, flags);
    }
  }

  switch(b->type)
  {
  case T_AND:
    ret = low_match_types(a, b->car, flags);
    if(!ret) return 0;
    return low_match_types(a, b->cdr, flags);

  case T_OR:
    ret = low_match_types(a, b->car, flags);
    if(ret && !(flags & NO_SHORTCUTS)) return ret;
    if(ret)
    {
      low_match_types(a, b->cdr, flags);
      return ret;
    }else{
      return low_match_types(a, b->cdr, flags);
    }

  case PIKE_T_RING:
    return low_match_types(a, b->car, flags);

  case PIKE_T_NAME:
    return low_match_types(a, b->cdr, flags);

  case T_NOT:
    if(low_match_types(a, b->car, (flags ^ A_EXACT ) | NO_MAX_ARGS))
      return 0;
    return a;

    case T_ASSIGN:
      ret = low_match_types(a, b->cdr, flags);
      if(ret && (a->type != T_VOID))
      {
	int m = (ptrdiff_t)b->car;
	struct pike_type *tmp;
	type_stack_mark();
	push_finished_type_with_markers(a, a_markers);
	tmp=pop_unfinished_type();

	type_stack_mark();
	low_or_pike_types(b_markers[m], tmp, 0);
	if(b_markers[m]) free_type(b_markers[m]);
	free_type(tmp);
	b_markers[m] = pop_unfinished_type();
#ifdef PIKE_TYPE_DEBUG
	if (l_flag>2) {
	  char *s;
	  int e;
	  init_buf();
	  for(e=0;e<indent;e++) my_strcat("  ");
	  my_strcat("b_markers[");
	  my_putchar((char)(m+'0'));
	  my_strcat("]=");
	  my_describe_type(b_markers[m]);
	  my_strcat("\n");
	  fprintf(stderr,"%s",(s=simple_free_buf()));
	  free(s);
	}
#endif
#ifdef PIKE_DEBUG
	if((ptrdiff_t)b_markers[m]->type == m+'0')
	  fatal("Cyclic type!\n");
#endif
      }
      return ret;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    {
      int m = b->type - '0';
      if(b_markers[m])
      {
#ifdef PIKE_DEBUG
	if(b_markers[m]->type == b->type)
	  fatal("Cyclic type!\n");
#endif
	return low_match_types(a, b_markers[m], flags);
      }
      else
	return low_match_types(a, mixed_type_string, flags);
    }
  }

  /* 'mixed' matches anything */

  if((a->type == T_ZERO || a->type == T_MIXED) &&
     !(flags & (A_EXACT|B_EXACT)) &&
     (b->type != T_VOID))
  {
#if 1
    switch(b->type)
    {
      /* These types can contain sub-types */
      case T_ARRAY:
	low_match_types(array_type_string, b, flags);
	break;
      case T_MAPPING:
	low_match_types(mapping_type_string, b, flags);
	break;
      case T_FUNCTION:
      case T_MANY:
	low_match_types(function_type_string, b, flags);
	break;
      case T_MULTISET:
	low_match_types(multiset_type_string, b, flags);
	break;
    }
#endif
    return a;
  }

  if((b->type == T_ZERO || b->type == T_MIXED) &&
     !(flags & (A_EXACT|B_EXACT)) &&
     (a->type != T_VOID))
  {
#if 1
    switch(a->type)
    {
      /* These types can contain sub-types */
      case T_ARRAY:
	low_match_types(a, array_type_string, flags);
	break;
      case T_MAPPING:
	low_match_types(a, mapping_type_string, flags);
	break;
      case T_FUNCTION:
      case T_MANY:
	low_match_types(a, function_type_string, flags);
	break;
      case T_MULTISET:
	low_match_types(a, multiset_type_string, flags);
	break;
    }
#endif
    return a;
  }

  /* Special cases (tm) */
  switch(TWOT(a->type, b->type))
  {
  case TWOT(T_PROGRAM, T_FUNCTION):
  case TWOT(T_FUNCTION, T_PROGRAM):
  case TWOT(T_PROGRAM, T_MANY):
  case TWOT(T_MANY, T_PROGRAM):
    /* FIXME: Should look at the sub-type of the program
     * to determine the prototype to use.
     */
    return a;

  case TWOT(T_OBJECT, T_FUNCTION):
  case TWOT(T_OBJECT, T_MANY):
  {
    struct pike_type *s;
    if((s = low_object_lfun_type(a, LFUN_CALL)))
       return low_match_types(s, b, flags);
    if (flags & B_EXACT) {
      /* A function isn't an object */
      return 0;
    }
    return a;
  }

  case TWOT(T_FUNCTION, T_OBJECT):
  case TWOT(T_MANY, T_OBJECT):
  {
    struct pike_type *s;
    if((s=low_object_lfun_type(b, LFUN_CALL)))
       return low_match_types(a, s, flags);
    if (flags & A_EXACT) {
      /* A function isn't an object */
      return 0;
    }
    return a;
  }
  case TWOT(T_INT, T_ZERO):
  {
    if ((((ptrdiff_t)a->car) > 0) ||
	(((ptrdiff_t)a->cdr) < 0)) {
      return 0;
    }
    return a;
  }
  case TWOT(T_ZERO, T_INT):
  {
    if ((((ptrdiff_t)b->car) > 0) ||
	(((ptrdiff_t)b->cdr) < 0)) {
      return 0;
    }
    return a;
  }
  case TWOT(T_FUNCTION, T_FUNCTION):
  case TWOT(T_FUNCTION, T_MANY):
  case TWOT(T_MANY, T_FUNCTION):
  case TWOT(T_MANY, T_MANY):
    ret = a;
    correct_args=0;
    while ((a->type != T_MANY) || (b->type != T_MANY))
    {
      struct pike_type *a_tmp, *b_tmp;

      a_tmp = a->car;
      if (a->type == T_FUNCTION)
      {
	a = a->cdr;
      }

      b_tmp = b->car;
      if (b->type == T_FUNCTION)
      {
	b = b->cdr;
      }

      if(!low_match_types(a_tmp, b_tmp,
			  (flags | NO_MAX_ARGS) & ~(A_EXACT|B_EXACT)))
	return 0;
      if(++correct_args > max_correct_args)
	if(!(flags & NO_MAX_ARGS))
	  max_correct_args=correct_args;
    }
    /* check the 'many' type */
    if(b->car->type != T_VOID &&
       a->car->type != T_VOID)
    {
      if(!low_match_types(a->car, b->car,
			  (flags | NO_MAX_ARGS) & ~(A_EXACT|B_EXACT)))
	return 0;
    }
    if(!(flags & NO_MAX_ARGS))
       max_correct_args=0x7fffffff;
    /* check the returntype */
    a = a->cdr;
    b = b->cdr;
    if ((b->type == T_VOID) && (a->type != T_VOID)) {
      /* Promote b to a function returning zero. */
      if (!low_match_types(a, zero_type_string, flags & ~(A_EXACT|B_EXACT)))
	return 0;
    } else if ((a->type == T_VOID) && (b->type != T_VOID)) {
      /* Promote a to a function returning zero. */
      if(!low_match_types(zero_type_string, b, flags & ~(A_EXACT|B_EXACT)))
	return 0;
    } else if(!low_match_types(a, b, flags & ~(A_EXACT|B_EXACT))) return 0;
    return ret;
  }

  if(a->type != b->type) return 0;

  ret=a;
  switch(a->type)
  {
  case T_MAPPING:
    if(!low_match_types(a->car, b->car,
			flags & ~(A_EXACT|B_EXACT))) return 0;
    if(!low_match_types(a->cdr, b->cdr,
			flags & ~(A_EXACT|B_EXACT))) return 0;
    break;

  case T_OBJECT:
#if 0
    if(a->cdr || b->cdr)
    {
      fprintf(stderr,"Type match1: ");
      stupid_describe_type(a,type_length(a));
      fprintf(stderr,"Type match2: ");
      stupid_describe_type(b,type_length(b));
    }
#endif

    /* object(* 0) matches any object */
    if(!a->cdr || !b->cdr) break;

    /* object(x *) =? object(x *) */
    if(a->car == b->car)
    {
      /* x? */
      if(a->car)
      {
	/* object(1 x) =? object(1 x) */
	if(a->cdr != b->cdr) return 0;
      }else{
	/* object(0 *) =? object(0 *) */
	/* FIXME: Ought to check the implements relation */
	break;
      }
    }

    {
      struct program *ap,*bp;
      ap = id_to_program((ptrdiff_t)a->cdr);
      bp = id_to_program((ptrdiff_t)b->cdr);

      if(!ap || !bp) break;

#if 0
      /* FIXME: Temporary kludge.
       * match_types() currently seems to need to be symetric.
       */
      if (!implements(ap,bp) && !implements(bp,ap))
	return 0;
#else /* !1 */
      if(a->car)
      {
	if(!implements(implements_a=ap,implements_b=bp))
	  return 0;
      }else{
	if(!implements(implements_a=bp,implements_b=ap))
	  return 0;
      }
#endif /* 1 */
    }
    
    break;

  case T_INT:
  {
    INT32 amin = (ptrdiff_t)a->car;
    INT32 amax = (ptrdiff_t)a->cdr;

    INT32 bmin = (ptrdiff_t)b->car;
    INT32 bmax = (ptrdiff_t)b->cdr;
    
    if(amin > bmax || bmin > amax) return 0;
    break;
  }
    

  case T_PROGRAM:
  case T_TYPE:
  case T_MULTISET:
  case T_ARRAY:
    if(!low_match_types(a->car, b->car,
			flags & ~(A_EXACT|B_EXACT))) return 0;

  case T_FLOAT:
  case T_STRING:
  case T_ZERO:
  case T_VOID:
  case T_MIXED:
    break;

  default:
    fatal("Error in type string.\n");
  }
  return ret;
}

/*
 * Check the partial ordering relation.
 *
 *                 mixed
 *
 * int float string program function object
 *
 *                 zero
 *
 *                 void
 *
 * Note that non-destructive operations are assumed.
 * ie it's assumed that calling a function(mapping(string|int:string|int):void)
 * with a mapping(int:int) won't change the type of the mapping after the
 * operation.
 */
static int low_pike_types_le(struct pike_type *a, struct pike_type *b,
			     int array_cnt, unsigned int flags)
#ifdef PIKE_TYPE_DEBUG
{
  int e;
  char *s;
  static int low_pike_types_le2(struct pike_type *a, struct pike_type *b,
				int array_cnt, unsigned int flags);
  int res;
  char buf[50];

  if (l_flag>2) {
    init_buf();
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("low_pike_types_le(");
    my_describe_type(a);
    my_strcat(",\n");
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("                ");
    my_describe_type(b);
    my_strcat(",\n");
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("                ");
    sprintf(buf, "%d", array_cnt);
    my_strcat(buf);
    my_strcat(", ");
    sprintf(buf, "0x%08x", flags);
    my_strcat(buf);
    my_strcat(");\n");
    fprintf(stderr,"%s",(s=simple_free_buf()));
    free(s);
    indent++;
  }

  res = low_pike_types_le2(a, b, array_cnt, flags);

  if (l_flag>2) {
    indent--;

    for(e=0;e<indent;e++) fprintf(stderr, "  ");
    fprintf(stderr, "= %d\n", res);
  }
  return res;
}

static int low_pike_types_le2(struct pike_type *a, struct pike_type *b,
			      int array_cnt, unsigned int flags)
#endif /* PIKE_TYPE_DEBUG */

{
  int ret;

 recurse:
#if 0
  fprintf(stderr, "low_pike_types_le(%d, %d, %d, 0x%08x)\n",
	  a->type, b->type, array_cnt, flags);
#endif /* 0 */

  if(a == b) return 1;

  switch(a->type)
  {
  case T_AND:
    /* OK if either of the parts is a subset. */
    /* FIXME: What if b also contains an AND? */
    ret = low_pike_types_le(a->car, b, array_cnt, flags);
    if(ret) return ret;
    a = a->cdr;
    goto recurse;

  case T_OR:
    /* OK, if both of the parts are a subset */
    if (a->car->type == T_VOID) {
      /* Special case for T_VOID */
      /* FIXME: Should probably be handled as T_ZERO. */
      a = a->cdr;
      goto recurse;
    } else {
      ret = low_pike_types_le(a->car, b, array_cnt, flags);
      if (!ret) return 0;
      if (a->cdr->type == T_VOID) {
	/* Special case for T_VOID */
	/* FIXME: Should probably be handled as T_ZERO. */
	return 1;
      } else {
	a = a->cdr;
	goto recurse;
      }
    }

  case PIKE_T_RING:
    a = a->car;
    goto recurse;

  case PIKE_T_NAME:
    a = a->cdr;
    goto recurse;

  case T_NOT:
    if (b->type == T_NOT) {
      struct pike_type *tmp = a->car;
      a = b->car;
      b = tmp;
      array_cnt = -array_cnt;
      goto recurse;
    }
    if (a->car->type == T_NOT) {
      a = a->car->car;
      goto recurse;
    }
    if (low_pike_types_le(a->car, b, array_cnt, flags)) {
      return 0;
    }
    /* FIXME: This is wrong... */
    return !low_pike_types_le(b, a->car, -array_cnt, flags);

  case T_ASSIGN:
    ret = low_pike_types_le(a->cdr, b, array_cnt, flags);
    if(ret && (b->type != T_VOID))
    {
      int m = (ptrdiff_t)a->car;
      struct pike_type *tmp;
      int i;
      type_stack_mark();
      push_finished_type_with_markers(b, b_markers);
      for(i=array_cnt; i > 0; i--)
	push_type(T_ARRAY);
      tmp=pop_unfinished_type();
      
      type_stack_mark();
      low_or_pike_types(a_markers[m], tmp, 0);
      if(a_markers[m]) free_type(a_markers[m]);
      free_type(tmp);
      a_markers[m] = pop_unfinished_type();
#ifdef PIKE_TYPE_DEBUG
      if (l_flag>2) {
	char *s;
	int e;
	init_buf();
	for(e=0;e<indent;e++) my_strcat("  ");
	my_strcat("a_markers[");
	my_putchar((char)(m+'0'));
	my_strcat("]=");
	my_describe_type(a_markers[m]);
	my_strcat("\n");
	fprintf(stderr,"%s",(s=simple_free_buf()));
	free(s);
      }
#endif
    }
    return ret;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    {
      int m = a->type - '0';
      if(a_markers[m]) {
	a = a_markers[m];
      } else {
	a = mixed_type_string;
      }
      goto recurse;
    }
  }

  switch(b->type)
  {
  case T_AND:
    /* OK, if a is a subset of both parts. */
    ret = low_pike_types_le(a, b->car, array_cnt, flags);
    if(!ret) return 0;
    b = b->cdr;
    goto recurse;

  case T_OR:
    /* OK if a is a subset of either of the parts. */
    ret=low_pike_types_le(a, b->car, array_cnt, flags);
    if (ret) return ret;
    b = b->cdr;
    goto recurse;

  case PIKE_T_RING:
    b = b->car;
    goto recurse;

  case PIKE_T_NAME:
    b = b->cdr;
    goto recurse;

  case T_NOT:
    if (b->car->type == T_NOT) {
      b = b->car->car;
      goto recurse;
    }
    if (low_pike_types_le(a, b->car, array_cnt, flags)) {
      return 0;
    }
    /* FIXME: This is wrong... */
    return !low_pike_types_le(b->car, a, -array_cnt, flags);

  case T_ASSIGN:
    ret = low_pike_types_le(a, b->cdr, array_cnt, flags);
    if(ret && (a->type != T_VOID))
    {
      int m = (ptrdiff_t)b->car;
      struct pike_type *tmp;
      int i;
      type_stack_mark();
      push_finished_type_with_markers(a, a_markers);
      for(i = array_cnt; i < 0; i++)
	push_type(T_ARRAY);
      tmp=pop_unfinished_type();
      
      type_stack_mark();
      low_or_pike_types(b_markers[m], tmp, 0);
      if(b_markers[m]) free_type(b_markers[m]);
      free_type(tmp);
      b_markers[m]=pop_unfinished_type();
#ifdef PIKE_TYPE_DEBUG
      if (l_flag>2) {
	char *s;
	int e;
	init_buf();
	for(e=0;e<indent;e++) my_strcat("  ");
	my_strcat("b_markers[");
	my_putchar((char)(m+'0'));
	my_strcat("]=");
	my_describe_type(b_markers[m]);
	my_strcat("\n");
	fprintf(stderr,"%s",(s=simple_free_buf()));
	free(s);
      }
#endif
    }
    return ret;

  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    {
      int m =  b->type - '0';
      if(b_markers[m]) {
	b = b_markers[m];
      } else {
	b = mixed_type_string;
      }
      goto recurse;
    }
  }

  if ((array_cnt < 0) && (b->type == T_ARRAY)) {
    while (b->type == T_ARRAY) {
      b = b->car;
      if (!++array_cnt) break;
    }
    goto recurse;
  } else if ((array_cnt > 0) && (a->type == T_ARRAY)) {
    while (a->type == T_ARRAY) {
      a = a->car;
      if (!--array_cnt) break;
    }
    goto recurse;
  }

  /* NOTE: void only matches void. */
  if (a->type == T_VOID) {
    /* void <= any_type */
    if (array_cnt >= 0) {
      /* !array(void) */
      if (!array_cnt && (b->type == T_VOID)) {
	return 1;
      }
      return 0;
    }
  }

  if (b->type == T_VOID) {
    if (array_cnt <= 0) {
      /* !array(void) */
      return 0;
    }
  }

  if (b->type == T_MIXED) {
    /* any_type <= 'mixed' */
    if (array_cnt <= 0) {
      /* !array(mixed) */
      return 1;
    }
  }

  if (a->type == T_MIXED) {
    if (array_cnt >= 0) {
      /* !array(mixed) */
      return 0;
    }
  }

  if (a->type == T_ZERO) {
    /* void <= zero <= any_type */
    if (array_cnt >= 0) {
      /* !array(zero) */
      return 1;
    }
  }

  if (b->type == T_ZERO) {
    if (array_cnt <= 0) {
      /* !array(zero) */
      return 0;
    }
  }

  /* Special cases (tm) */
  switch(TWOT(a->type, b->type))
  {
  case TWOT(T_PROGRAM, T_FUNCTION):
  case TWOT(T_FUNCTION, T_PROGRAM):
  case TWOT(T_PROGRAM, T_MANY):
  case TWOT(T_MANY, T_PROGRAM):
    /* FIXME: Not really... Should check the return value. */
    /* FIXME: Should also look at the subtype of the program. */
    return 1;

  case TWOT(T_OBJECT, T_FUNCTION):
  case TWOT(T_OBJECT, T_MANY):
    {
      if((a = low_object_lfun_type(a, LFUN_CALL))) {
	goto recurse;
      }
      return 1;
    }

  case TWOT(T_FUNCTION, T_OBJECT):
  case TWOT(T_MANY, T_OBJECT):
    {
      if((b=low_object_lfun_type(b, LFUN_CALL))) {
	goto recurse;
      }
      return 1;
    }

  case TWOT(T_FUNCTION, T_ARRAY):
  case TWOT(T_MANY, T_ARRAY):
    {
      while (b->type == T_ARRAY) {
	b = b->car;
	array_cnt++;
      }
      goto recurse;
    }

  case TWOT(T_ARRAY, T_FUNCTION):
  case TWOT(T_ARRAY, T_MANY):
    {
      while (a->type == T_ARRAY) {
	a = a->car;
	array_cnt--;
      }
      goto recurse;
    }

  case TWOT(T_FUNCTION, T_FUNCTION):
  case TWOT(T_FUNCTION, T_MANY):
  case TWOT(T_MANY, T_FUNCTION):
    /*
     * function(A...:B) <= function(C...:D)	iff C <= A && B <= D
     */
    /*
     * function(:int) <= function(int:int)
     * function(int|string:int) <= function(int:int)
     * function(:int) <= function(:void)
     * function(string:int) != function(int:int)
     * function(int:int) != function(:int)
     *
     * FIXME: Enforcing of all required arguments?
     */
    while((a->type != T_MANY) || (b->type != T_MANY))
    {
      struct pike_type *a_tmp, *b_tmp;

      a_tmp = a->car;
      if (a->type == T_FUNCTION)
      {
	a = a->cdr;
      }

      b_tmp = b->car;
      if (b->type == T_FUNCTION)
      {
	b = b->cdr;
      }

      if (a_tmp->type != T_VOID) {
	if (!low_pike_types_le(b_tmp, a_tmp, 0, flags)) {
	  return 0;
	}
      }
    }
    /* FALL_THROUGH */
  case TWOT(T_MANY, T_MANY):
    /* check the 'many' type */
    if ((a->car->type != T_VOID) && (b->car->type != T_VOID)) {
      if (!low_pike_types_le(b->car, a->car, 0, flags)) {
	return 0;
      }
    }

    a = a->cdr;
    b = b->cdr;

    /* check the returntype */
    /* FIXME: Check array_cnt */
    if ((b->type != T_VOID) && (a->type != T_VOID)) {
      if(!low_pike_types_le(a, b, array_cnt, flags)) return 0;
    }
    return 1;
  }

  if(a->type != b->type) return 0;

  if (array_cnt) return 0;

  switch(a->type)
  {
  case T_MAPPING:
    /*
     *  mapping(A:B) <= mapping(C:D)   iff A <= C && B <= D.
     */
    if(!low_pike_types_le(a->car, b->car, 0, flags)) return 0;
    array_cnt = 0;
    a = a->cdr;
    b = b->cdr;
    goto recurse;

  case T_OBJECT:
#if 0
    if(a->cdr || b->cdr)
    {
      fprintf(stderr,"Type match1: ");
      stupid_describe_type(a,type_length(a));
      fprintf(stderr,"Type match2: ");
      stupid_describe_type(b,type_length(b));
    }
#endif

    /*
     * object(0|1 x) <= object(0|1 0)
     * object(0|1 0) <=! object(0|1 !0)
     * object(1 x) <= object(0|1 x)
     * object(1 x) <= object(1 y)	iff x == y
     * object(1 x) <= object(0 y)	iff x implements y
     * Not WEAK_OBJECTS:
     *   object(0 x) <= object(0 y)	iff x implements y
     * WEAK_OBJECTS:
     *   object(0 x) <= object(0 y)	iff x is_compatible y
     */

    /* object(* 0) matches any object */
    if(!b->cdr)
      return 1;

    if(!a->cdr)
      return 0;
    
    if ((a->car || !b->car) &&
	(a->cdr == b->cdr))
      return 1;

    if (b->car) {
      return 0;
    }

    {
      struct program *ap = id_to_program((ptrdiff_t)a->cdr);
      struct program *bp = id_to_program((ptrdiff_t)b->cdr);

      if (!ap || !bp) {
	/* Shouldn't happen... */
	return 0;
      }
      if ((flags & LE_WEAK_OBJECTS) && (!a->car)) {
	return is_compatible(implements_a=ap, implements_b=bp);
      }
      return implements(implements_a=ap, implements_b=bp);
    }
    break;

  case T_INT:
  {
    INT32 amin = (ptrdiff_t)a->car;
    INT32 amax = (ptrdiff_t)a->cdr;

    INT32 bmin = (ptrdiff_t)b->car;
    INT32 bmax = (ptrdiff_t)b->cdr;
    
    if(amin < bmin || amax > bmax) return 0;
    break;
  }
    

  case T_TYPE:
  case T_PROGRAM:
  case T_MULTISET:
  case T_ARRAY:
    a = a->car;
    b = b->car;
    array_cnt = 0;
    goto recurse;

  case T_FLOAT:
  case T_STRING:
  case T_ZERO:
  case T_VOID:
  case T_MIXED:
    break;

  default:
    fatal("Error in type string.\n");
  }
  return 1;
}

/*
 * Check the function parameters.
 * Note: The difference between this function, and pike_types_le()
 *       is the more lenient check for T_OR, and the handling of T_ARRAY.
 */
int strict_check_call(struct pike_type *fun_type,
		      struct pike_type *arg_type)
{
  while ((fun_type->type == T_OR) ||
	 (fun_type->type == T_ARRAY)) {
    if (fun_type->type == T_OR) {
      int res = strict_check_call(fun_type->car, arg_type);
      if (res) return res;
      fun_type = fun_type->cdr;
    } else {
      fun_type = fun_type->car;
    }
  }
  return low_pike_types_le(fun_type, arg_type, 0, 0);
}

/*
 * Check validity of soft-cast.
 * Note: This uses a weaker check of function arguments, since
 *       people get confused otherwise.
 */
int check_soft_cast(struct pike_type *to, struct pike_type *from)
{
  return low_pike_types_le(to, from, 0, LE_WEAK_OBJECTS);
}

/*
 * Return the return type from a function call.
 */
static int low_get_return_type(struct pike_type *a, struct pike_type *b)
{
  int tmp;
  switch(a->type)
  {
  case T_OR:
    {
      struct pike_type *o1, *o2;
      o1=o2=0;

      type_stack_mark();
      if(low_get_return_type(a->car, b)) 
      {
	o1=pop_unfinished_type();
	type_stack_mark();
      }

      if(low_get_return_type(a->cdr, b))
	o2=pop_unfinished_type();
      else
	pop_stack_mark();

      if(!o1 && !o2) return 0;

      low_or_pike_types(o1, o2, 0);

      if(o1) free_type(o1);
      if(o2) free_type(o2);

      return 1;
    }

  case T_AND:
    type_stack_mark();
    tmp = low_get_return_type(a->car, b);
    type_stack_pop_to_mark();
    if(!tmp) return 0;
    return low_get_return_type(a->cdr, b);

  case PIKE_T_RING:
    return low_get_return_type(a->car, b);

  case PIKE_T_NAME:
    return low_get_return_type(a->cdr, b);

  case T_ARRAY:
    tmp = low_get_return_type(a->car, b);
    if(!tmp) return 0;
    push_type(T_ARRAY);
    return 1;
  }

  a = low_match_types(a, b, NO_SHORTCUTS);
  if(a)
  {
#if 0
    if ((lex.pragmas & ID_STRICT_TYPES) &&
	!low_pike_types_le(a, b, 0, 0)) {
      yywarning("Type mismatch");
    }
#endif /* 0 */
    switch(a->type)
    {
    case T_FUNCTION:
      a = a->cdr;
      while(a->type == T_FUNCTION) {
	a = a->cdr;
      }
      /* FALL_THROUGH */
    case T_MANY:
      a = a->cdr;
      push_finished_type_with_markers(a, a_markers );
      return 1;

    case T_PROGRAM:
      push_finished_type(a->car);
      return 1;

    default:
      push_type(T_MIXED);
      return 1;
    }
  }
  return 0;
}


int match_types(struct pike_type *a, struct pike_type *b)
{
  check_type_string(a);
  check_type_string(b);
  clear_markers();
  return !!low_match_types(a, b, 0);
}

int pike_types_le(struct pike_type *a,struct pike_type *b)
{
  check_type_string(a);
  check_type_string(b);
  clear_markers();
  return low_pike_types_le(a, b, 0, 0);
}


#ifdef DEBUG_MALLOC
#define low_index_type(X,Y,Z) ((struct pike_type *)debug_malloc_pass(debug_low_index_type((X),(Y),(Z))))
#else
#define low_index_type debug_low_index_type
#endif

/* FIXME, add the index */
static struct pike_type *debug_low_index_type(struct pike_type *t,
					      struct pike_type *index_type,
					      node *n)
{
  struct pike_type *tmp;
  struct program *p;

  switch(low_check_indexing(t, index_type, n))
  {
    case 0: return 0;
    case -1:
      add_ref(zero_type_string);
      return zero_type_string;
  }

  while(t->type == PIKE_T_NAME) {
    t = t->cdr;
  }
  while(index_type->type == PIKE_T_NAME) {
    index_type = index_type->cdr;
  }

  switch(t->type)
  {
  case T_OBJECT:
  {
    p = id_to_program((ptrdiff_t)t->cdr);

  comefrom_int_index:
    if(p && n)
    {
      INT32 i;
      if(n->token == F_ARROW)
      {
	/* FIXME: make this stricter */
	if((i=FIND_LFUN(p,LFUN_ARROW))!=-1)
	{
	  /* FIXME: function_type_string should be replaced with something
	   * derived from type_string
	   */
	  if(i!=-1 &&
	     (tmp=check_call(function_type_string, ID_FROM_INT(p, i)->type,
			     0)))
	    return tmp;

	  add_ref(mixed_type_string);
	  return mixed_type_string;
	}
      }else{
	if((i=FIND_LFUN(p,LFUN_INDEX)) != -1)
	{
	  /* FIXME: function_type_string should be replaced with something
	   * derived from type_string
	   */
	  if(i!=-1 &&
	     (tmp=check_call(function_type_string, ID_FROM_INT(p, i)->type,
			     0)))
	    return tmp;

	  add_ref(mixed_type_string);
	  return mixed_type_string;
	}
      }
      if(CDR(n)->token == F_CONSTANT && CDR(n)->u.sval.type==T_STRING)
      {
	i = find_shared_string_identifier(CDR(n)->u.sval.u.string, p);
	if(i==-1)
	{
	  add_ref(mixed_type_string);
	  return mixed_type_string;
	}else{
#if 0
	  if(t->car ||
	     (p->identifier_references[i].id_flags & ID_NOMASK) ||
	    (ID_FROM_INT(p, i)->identifier_flags & IDENTIFIER_PROTOTYPED))
	  {
#endif
	    add_ref(ID_FROM_INT(p, i)->type);
	    return ID_FROM_INT(p, i)->type;
#if 0
	  }else{
	    add_ref(mixed_type_string);
	    return mixed_type_string;
	  }
#endif
	}	   
      }
    }
  }
  default:
    add_ref(mixed_type_string);
    return mixed_type_string;

  case T_MIXED:
    if (lex.pragmas & ID_STRICT_TYPES) {
      yywarning("Indexing mixed.");
    }
    add_ref(mixed_type_string);
    return mixed_type_string;    

    case T_INT:
#ifdef AUTO_BIGNUM
      /* Don't force Gmp.mpz to be loaded here since this function
       * is called long before the master object is compiled...
       * /Hubbe
       */
      p=get_auto_bignum_program_or_zero();
      goto comefrom_int_index;
#endif
    case T_ZERO:
    case T_TYPE:
    case PIKE_T_RING:
    case T_VOID:
    case T_FLOAT:
      return 0;

  case T_OR:
  {
    struct pike_type *a,*b;
    a = low_index_type(t->car, index_type, n);
    b = low_index_type(t->cdr, index_type,n);
    if(!b) return a;
    if(!a) return b;
    type_stack_mark();
    low_or_pike_types(a,b,1);
    free_type(a);
    free_type(b);
    return pop_unfinished_type();
  }

  case T_AND:
    /* FIXME: Shouldn't both branches be looked at? */
    return low_index_type(t->cdr, index_type, n);

  case T_STRING: /* always int */
  case T_MULTISET: /* always int */
    add_ref(int_type_string);
    return int_type_string;

  case T_MAPPING:
    add_ref(t = t->cdr);
    return t;

  case T_ARRAY:
    {
      struct pike_type *a;

      if(low_pike_types_le(string_type_string, index_type, 0, 0) &&
	 (a = low_index_type(t->car, string_type_string, n))) {
	/* Possible to index the array with a string. */
	type_stack_mark();
	push_finished_type(a);
	free_type(a);
	push_type(T_ARRAY);

	if (low_match_types(int_type_string, index_type, 0)) {
	  /* Also possible to index the array with an int. */
	  push_finished_type(t->car);
	  push_type(T_OR);
	}
	return pop_unfinished_type();
      }
      if (low_match_types(int_type_string, index_type, 0)) {
	/* Possible to index the array with an int. */
	add_ref(t->car);
	return t->car;
      }
      /* Bad index type. */
      return 0;
    }
  }
}

struct pike_type *index_type(struct pike_type *type,
			     struct pike_type *index_type,
			     node *n)
{
  struct pike_type *t;
  clear_markers();
  t = low_index_type(type, index_type, n);
  if(!t) {
    copy_pike_type(t, mixed_type_string);
  }
  return t;
}

#ifdef DEBUG_MALLOC
#define low_range_type(X,Y,Z) ((struct pike_type *)debug_malloc_pass(debug_low_range_type((X),(Y),(Z))))
#else
#define low_range_type debug_low_range_type
#endif

/* FIXME, add the index
 *
 * FIXME: Is the above fixme valid for this function too?
 */
static struct pike_type *debug_low_range_type(struct pike_type *t,
					      struct pike_type *index1_type,
					      struct pike_type *index2_type)
{
  struct pike_type *tmp;
  struct program *p;

  while(t->type == PIKE_T_NAME) {
    t = t->cdr;
  }
  while(index1_type->type == PIKE_T_NAME) {
    index1_type = index1_type->cdr;
  }
  while(index2_type->type == PIKE_T_NAME) {
    index2_type = index2_type->cdr;
  }

  switch(t->type)
  {
  case T_OBJECT:
  {
    p = id_to_program((ptrdiff_t)t->cdr);

    if(p)
    {
      INT32 i;
      if((i = FIND_LFUN(p, LFUN_INDEX)) != -1)
      {
	struct pike_type *call_type = NULL;
	/* FIXME: function_type_string should be replaced with something
	 * derived from type_string
	 */
	type_stack_mark();
	push_finished_type(mixed_type_string);
	push_finished_type(void_type_string);
	push_type(T_OR);			/* Return type */
	push_finished_type(void_type_string);	/* Many type */
	push_type(T_MANY);
	push_finished_type(index2_type);	/* arg2 type */
	push_type(T_FUNCTION);
	push_finished_type(index1_type);	/* arg1 type */
	push_type(T_FUNCTION);
	call_type = pop_unfinished_type();
	
	if((tmp = check_call(call_type, ID_FROM_INT(p, i)->type, 0))) {
	  free_type(call_type);
	  return tmp;
	}

	add_ref(mixed_type_string);
	return mixed_type_string;
      }
      yywarning("Ranging object without index operator.");
      return 0;
    }
    if (lex.pragmas & ID_STRICT_TYPES) {
      yywarning("Ranging generic object.");
    }
    add_ref(mixed_type_string);
    return mixed_type_string;    
  }

  case T_MIXED:
    if (lex.pragmas & ID_STRICT_TYPES) {
      yywarning("Ranging mixed.");
    }
    add_ref(mixed_type_string);
    return mixed_type_string;    

  case T_INT:
  case T_ZERO:
  case T_TYPE:
  case PIKE_T_RING:
  case T_VOID:
  case T_FLOAT:
  case T_MULTISET:
  case T_MAPPING:
    /* Illegal range operation. */
    /* FIXME: Strict type warning. */
    return 0;

  case T_ARRAY:
  case T_STRING:
    /* Check that the index types are compatible with int. */
    {
      struct pike_type *a;

      if (!low_match_types(int_type_string, index1_type, 0)) {
	struct pike_string *s = describe_type(t);
	yywarning("Bad argument 1 to range operator on %s.",
		  s->str);
	free_string(s);
	yyexplain_nonmatching_types(int_type_string, index1_type,
				    YYTE_IS_WARNING);
	/* Bad index1 type. */
	return 0;
      }
      if (!low_match_types(int_type_string, index2_type, 0)) {
	struct pike_string *s = describe_type(t);
	yywarning("Bad argument 2 to range operator on %s.",
		  s->str);
	free_string(s);
	yyexplain_nonmatching_types(int_type_string, index2_type,
				    YYTE_IS_WARNING);
	/* Bad index2 type. */
	return 0;
      }
    }
    /* FALLTHROUGH */
  default:
    /* Identity. */
    add_ref(t);
    return t;

  case T_OR:
  {
    struct pike_type *a,*b;
    a = low_range_type(t->car, index1_type, index2_type);
    b = low_range_type(t->cdr, index1_type, index2_type);
    if(!b) return a;
    if(!a) return b;
    type_stack_mark();
    low_or_pike_types(a,b,1);
    free_type(a);
    free_type(b);
    return pop_unfinished_type();
  }

  case T_AND:
    /* FIXME: Shouldn't both branches be looked at? */
    return low_range_type(t->cdr, index1_type, index2_type);
  }
}

struct pike_type *range_type(struct pike_type *type,
			     struct pike_type *index1_type,
			     struct pike_type *index2_type)
{
  struct pike_type *t;
  clear_markers();
  t = low_range_type(type, index1_type, index2_type);
  if(!t) {
    yyerror("Invalid range operation.");
    copy_pike_type(t, type);
  }
  return t;
}


static struct pike_type *low_array_value_type(struct pike_type *arr_t)
{
  struct pike_type *res = NULL;
  struct pike_type *sub_t;

  while (arr_t->type == T_OR) {
    sub_t = low_array_value_type(arr_t->car);
    arr_t = arr_t->cdr;
    if (sub_t) {
      if (res) {
	struct pike_type *new = or_pike_types(res, sub_t, 1);
	free_type(res);
	free_type(sub_t);
	res = new;
      } else {
	res = sub_t;
      }
    }
  }
  if (arr_t->type != T_ARRAY)
    return res;

  copy_pike_type(sub_t, arr_t->car);

  if (res) {
    struct pike_type *new = or_pike_types(res, sub_t, 1);
    free_type(res);
    free_type(sub_t);
    return new;
  }
  return sub_t;
}

struct pike_type *array_value_type(struct pike_type *array_type)
{
  struct pike_type *t = low_array_value_type(array_type);
  if (!t) {
    copy_pike_type(t, mixed_type_string);
  }
  return t;
}


#ifdef DEBUG_MALLOC
#define low_key_type(X,Y) ((struct pike_type *)debug_malloc_pass(debug_low_key_type((X),(Y))))
#else
#define low_key_type debug_low_key_type
#endif

/* FIXME, add the index */
static struct pike_type *debug_low_key_type(struct pike_type *t, node *n)
{
  switch(t->type)
  {
  case T_OBJECT:
  {
    struct program *p = id_to_program((ptrdiff_t)t->cdr);
    if(p && n)
    {
      if(n->token == F_ARROW)
      {
	if(FIND_LFUN(p,LFUN_ARROW)!=-1 || FIND_LFUN(p,LFUN_ASSIGN_ARROW)!=-1)
	{
	  add_ref(mixed_type_string);
	  return mixed_type_string;
	}
      }else{
	if(FIND_LFUN(p,LFUN_INDEX) != -1 || FIND_LFUN(p,LFUN_ASSIGN_INDEX) != -1)
	{
	  add_ref(mixed_type_string);
	  return mixed_type_string;
	}
      }
    }
    add_ref(string_type_string);
    return string_type_string;
  }
  default:
    add_ref(mixed_type_string);
    return mixed_type_string;

    case T_VOID:
    case T_ZERO:
    case T_TYPE:
    case PIKE_T_RING:
    case T_FLOAT:
    case T_INT:
      return 0;

  case T_OR:
  {
    struct pike_type *a,*b;
    a = low_key_type(t->car, n);
    b = low_key_type(t->cdr, n);
    if(!b) return a;
    if(!a) return b;
    type_stack_mark();
    low_or_pike_types(a,b,1);
    free_type(a);
    free_type(b);
    return pop_unfinished_type();
  }

  case T_AND:
    /* FIXME: Shouldn't this look at both branches? */
    return low_key_type(t->cdr, n);

  case PIKE_T_NAME:
    return low_key_type(t->cdr, n);

  case T_ARRAY:
  case T_STRING: /* always int */
    add_ref(int_type_string);
    return int_type_string;

  case T_MAPPING:
  case T_MULTISET:
    copy_pike_type(t, t->car);
    return t;
  }
}

struct pike_type *key_type(struct pike_type *type, node *n)
{
  struct pike_type *t;
  clear_markers();
  t = low_key_type(type,n);
  if(!t) {
    copy_pike_type(t, mixed_type_string);
  }
  return t;
}



static int low_check_indexing(struct pike_type *type,
			      struct pike_type *index_type,
			      node *n)
{
  switch(type->type)
  {
  case T_OR:
    return low_check_indexing(type->car, index_type, n) ||
      low_check_indexing(type->cdr, index_type, n);

  case T_AND:
    return low_check_indexing(type->car, index_type, n) &&
      low_check_indexing(type->cdr, index_type, n);

  case T_NOT:
    return low_check_indexing(type->car, index_type, n) != 1;

  case PIKE_T_NAME:
    return low_check_indexing(type->cdr, index_type, n);

  case T_ARRAY:
    if(low_match_types(string_type_string, index_type, 0) &&
       low_check_indexing(type->car, index_type, n))
      return 1;

  case T_STRING:
    return !!low_match_types(int_type_string, index_type, 0);

  case T_OBJECT:
  {
    struct program *p = id_to_program((ptrdiff_t)type->cdr);
    if(p)
    {
      if(n->token == F_ARROW)
      {
	if(FIND_LFUN(p,LFUN_ARROW)!=-1 || FIND_LFUN(p,LFUN_ASSIGN_ARROW)!=-1)
	  return 1;
      }else{
	if(FIND_LFUN(p,LFUN_INDEX)!=-1 || FIND_LFUN(p,LFUN_ASSIGN_INDEX)!=-1)
	  return 1;
      }
      return !!low_match_types(string_type_string, index_type,0);
    }else{
      return 1;
    }
  }

  case T_MULTISET:
  case T_MAPPING:
    /* FIXME: Why -1 and not 0? */
    return low_match_types(type->car, index_type, 0) ? 1 : -1;

#ifdef AUTO_BIGNUM
    case T_INT:
#endif
    case T_PROGRAM:
      return !!low_match_types(string_type_string, index_type, 0);

  case T_MIXED:
    return 1;
    
  default:
    return 0;
  }
}
				 
int check_indexing(struct pike_type *type,
		   struct pike_type *index_type,
		   node *n)
{
  check_type_string(type);
  check_type_string(index_type);

  return low_check_indexing(type, index_type, n);
}

static int low_count_arguments(struct pike_type *q)
{
  int num=0, num2;

  switch(q->type)
  {
    case T_OR:
      num = low_count_arguments(q->car);
      num2 = low_count_arguments(q->cdr);
      if(num<0 && num2>0) return num;
      if(num2<0 && num>0) return num2;
      if(num2<0 && num<0) return ~num>~num2?num:num2;
      return num>num2?num:num2;

    case T_AND:
      num = low_count_arguments(q->car);
      num2 = low_count_arguments(q->cdr);
      if(num<0 && num2>0) return num2;
      if(num2<0 && num>0) return num;
      if(num2<0 && num<0) return ~num<~num2?num:num2;
      return num<num2?num:num2;

    case PIKE_T_NAME:
      return low_count_arguments(q->cdr);

    default: return 0x7fffffff;

    case T_FUNCTION:
      while(q->type == T_FUNCTION)
      {
	num++;
	q = q->cdr;
      }
      /* FALL_THROUGH */
    case T_MANY:
      q = q->car;
      if(q->type != T_VOID) return ~num;
      return num;
  }
}

/* Count the number of arguments for a funciton type.
 * return -1-n if the function can take number of arguments
 * >= n  (varargs)
 */
int count_arguments(struct pike_type *s)
{
  check_type_string(s);

  return low_count_arguments(s);
}


static int low_minimum_arguments(struct pike_type *q)
{
  int num;

  switch(q->type)
  {
    case T_OR:
    case T_AND:
      return MAXIMUM(low_count_arguments(q->car),
		     low_count_arguments(q->cdr));

    default: return 0;

    case PIKE_T_NAME:
      return low_minimum_arguments(q->cdr);

    case T_FUNCTION:
      num = 0;
      while(q->type == T_FUNCTION)
      {
	if(low_match_types(void_type_string, q->car, B_EXACT))
	  return num;

	num++;
	q = q->cdr;
      }
      return num;
    case T_MANY:
      return 0;
  }
}

/* Count the minimum number of arguments for a funciton type.
 */
int minimum_arguments(struct pike_type *s)
{
  int ret;
  check_type_string(s);

  ret = low_minimum_arguments(s);

#if 0
  fprintf(stderr,"minimum_arguments(");
  simple_describe_type(s);
  fprintf(stderr," ) -> %d\n",ret);
#endif

  return ret;
}

struct pike_type *check_call(struct pike_type *args,
			     struct pike_type *type,
			     int strict)
{
  check_type_string(args);
  check_type_string(type);
  clear_markers();
  type_stack_mark();
  max_correct_args=0;
  
  if(low_get_return_type(type, args))
  {
    if (strict) {
      if (!strict_check_call(type, args)) {
	struct pike_string *type_t = describe_type(type);
	struct pike_type *func_zero_type;

	MAKE_CONSTANT_TYPE(func_zero_type, tFuncV(tNone,tZero,tMix));

	if (!low_pike_types_le(type, func_zero_type, 0, 0)) {
	  yywarning("Calling non-function value.");
	  yywarning("Type called: %s", type_t->str);
	} else {
	  struct pike_string *arg_t = describe_type(args);
	  yywarning("Arguments not strictly compatible.");
	  yywarning("Expected: %s", type_t->str);
	  yywarning("Got     : %s", arg_t->str);
	  free_string(arg_t);
	}

	free_type(func_zero_type);
	free_string(type_t);
      }
    }
    return pop_unfinished_type();
  }else{
    pop_stack_mark();
    return 0;
  }
}

INT32 get_max_args(struct pike_type *type)
{
  INT32 ret,tmp=max_correct_args;
  check_type(type->type);
  clear_markers();
  type = check_call(function_type_string, type, 0);
  if(type) free_type(type);
  ret=max_correct_args;
  max_correct_args=tmp;
  return tmp;
}

/* NOTE: type loses a reference. */
struct pike_type *new_check_call(node *fun, int *argno,
				 struct pike_type *type, node *args)
{
  struct pike_type *tmp_type = NULL;

  while (args && (args->token == F_ARG_LIST)) {
    type = new_check_call(fun, argno, type, CAR(args));
    args = CDR(args);
  }
  if (!args) {
    return type;
  }

  switch(type->type) {
  case T_NOT:
    break;

  case T_FUNCTION:
    if (!pike_types_le(args->type, type->car)) {
      if (!match_types(args->type, type->car)) {
	/* Bad arg */
      } else {
	/* Not strict arg */
      }
    }
    copy_pike_type(tmp_type, type->cdr);
    free_type(type);
    type = tmp_type;
    (*argno)++;
    break;

  case T_MANY:
    if (!pike_types_le(args->type, type->car)) {
      if (!match_types(args->type, type->car)) {
	/* Bad arg */
      } else {
	/* Not strict arg */
      }
    }
    (*argno)++;
    break;
  }

  return type;
}

struct pike_type *zzap_function_return(struct pike_type *a, INT32 id)
{
  switch(a->type)
  {
    case T_OR:
    {
      struct pike_type *ar, *br, *ret=0;
      ar = zzap_function_return(a->car, id);
      br = zzap_function_return(a->cdr, id);
      if(ar && br) ret = or_pike_types(ar, br, 0);
      if(ar) free_type(ar);
      if(br) free_type(br);
      return ret;
    }
      
    case T_FUNCTION:
    case T_MANY:
    {
      int nargs=0;
      type_stack_mark();
      
      while(a->type == T_FUNCTION)
      {
	push_finished_type(a->car);
	nargs++;
	a = a->cdr;
      }
      push_finished_type(a->car);
      push_object_type(1, id);
      push_reverse_type(T_MANY);
      while(nargs-- > 0) {
	push_reverse_type(T_FUNCTION);
      }
      return pop_unfinished_type();
    }

    case T_ARRAY:
      return zzap_function_return(a->car, id);

    case PIKE_T_NAME:
      return zzap_function_return(a->cdr, id);

    case T_MIXED:
      /* I wonder when this occurrs, but apparently it does... */
      /* FIXME: */
      type_stack_mark();
      push_object_type(1, id);
      push_type(T_VOID);
      push_type(T_MIXED);
      push_type(T_OR);
      push_type(T_MANY);
      return pop_unfinished_type();
  }
/* This error is bogus /Hubbe
  fatal("zzap_function_return() called with unexpected value: %d\n",
	EXTRACT_UCHAR(a));
*/
  return NULL;
}

struct pike_type *get_type_of_svalue(struct svalue *s)
{
  struct pike_type *ret;
  switch(s->type)
  {
  case T_FUNCTION:
    if(s->subtype == FUNCTION_BUILTIN)
    {
      copy_pike_type(ret, s->u.efun->type);
    }else{
      struct program *p;

      p=s->u.object->prog;
      if(!p)
      {
	copy_pike_type(ret, zero_type_string);
      }else{
	copy_pike_type(ret, ID_FROM_INT(p,s->subtype)->type);
      }
    }
    return ret;
       
  case T_ARRAY:
    {
      struct pike_type *arg_type;
      struct array *a = s->u.array;
#if 0
      int i;

      /* FIXME: Circular structures? */
      copy_pike_type(arg_type, zero_type_string);
      for (i = 0; i < a->size; i++) {
	struct pike_type *tmp1 = get_type_of_svalue(a->item+i);
	struct pike_type *tmp2 = or_pike_types(arg_type, tmp1, 1);
	free_type(arg_type);
	free_type(tmp1);
	arg_type = tmp2;
      }
#else /* !0 */
      if (a->size)
	copy_pike_type(arg_type, mixed_type_string);
      else
	copy_pike_type(arg_type, zero_type_string);
#endif /* 0 */
      type_stack_mark();
      push_finished_type(arg_type);
      free_type(arg_type);
      push_type(s->type);
      return pop_unfinished_type();
    }

  case T_MULTISET:
    type_stack_mark();
    if (multiset_sizeof(s->u.multiset)) {
      push_type(T_MIXED);
    }
    else {
      push_type(T_ZERO);
    }
    push_type(T_MULTISET);
    return pop_unfinished_type();

  case T_MAPPING:
    type_stack_mark();
    if (m_sizeof(s->u.mapping)) {
      push_type(T_MIXED);
      push_type(T_MIXED);
    }
    else {
      push_type(T_ZERO);
      push_type(T_ZERO);
    }
    push_type(T_MAPPING);
    return pop_unfinished_type();

  case T_OBJECT:
    type_stack_mark();
    if(s->u.object->prog)
    {
#ifdef AUTO_BIGNUM
      if(is_bignum_object(s->u.object))
      {
	push_int_type(MIN_INT32, MAX_INT32);
      }
      else
#endif
      {
	push_object_type(1, s->u.object->prog->id);
      }
    }else{
      /* Destructed object */
      push_type(T_ZERO);
    }
    return pop_unfinished_type();

  case T_INT:
    if(s->u.integer)
    {
      type_stack_mark();
      /* Fixme, check that the integer is in range of MIN_INT32 .. MAX_INT32!
       */
      push_int_type(s->u.integer, s->u.integer);
      return pop_unfinished_type();
    }else{
      copy_pike_type(ret, zero_type_string);
      return ret;
    }

  case T_PROGRAM:
  {
    /* FIXME: An alternative would be to push program(object(1,p->id)). */
    struct pike_type *a;
    struct pike_string *tmp;
    int id;

    if(s->u.program->identifiers)
    {
      id=FIND_LFUN(s->u.program,LFUN_CREATE);
      if(id>=0)
      {
	a = ID_FROM_INT(s->u.program, id)->type;
	if((a = zzap_function_return(a, s->u.program->id)))
	  return a;
	tmp=describe_type(ID_FROM_INT(s->u.program, id)->type);
	/* yywarning("Failed to zzap function return for type: %s.", tmp->str);*/
	free_string(tmp);
      }
    } else {
      if((a = zzap_function_return(function_type_string, s->u.program->id)))
	return a;
    }

    type_stack_mark();
    push_object_type(1, s->u.program->id);
    push_type(T_VOID);
    push_type(T_MANY);
    return pop_unfinished_type();
  }

  case T_TYPE:
    type_stack_mark();
    push_finished_type(s->u.type);
    push_type(T_TYPE);
    return pop_unfinished_type();

  default:
    type_stack_mark();
    push_type(s->type);
    return pop_unfinished_type();
  }
}


static struct pike_type *low_object_type_to_program_type(struct pike_type *obj_t)
{
  struct pike_type *res = NULL;
  struct pike_type *sub;
  struct svalue sval;
  int id;

  while(obj_t->type == T_OR) {
    sub = low_object_type_to_program_type(obj_t->car);
    if (!sub) {
      if (res) {
	free_type(res);
      }
      return NULL;
    }
    if (res) {
      struct pike_type *tmp = or_pike_types(res, sub, 1);
      free_type(res);
      free_type(sub);
      res = tmp;
    } else {
      res = sub;
    }
    obj_t = obj_t->cdr;
  }
  sval.type = T_PROGRAM;
  if ((obj_t->type != T_OBJECT) ||
      (!(id = (ptrdiff_t)obj_t->cdr)) ||
      (!(sval.u.program = id_to_program(id))) ||
      (!(sub = get_type_of_svalue(&sval)))) {
    if (res) {
      free_type(res);
    }
    return NULL;
  }
  /* FIXME: obj_t->car should propagate to the return-type in sub. */
  if (res) {
    struct pike_type *tmp = or_pike_types(res, sub, 1);
    free_type(res);
    free_type(sub);
    return tmp;
  }
  return sub;
}

/* Used by fix_object_program_type() */
struct pike_type *object_type_to_program_type(struct pike_type *obj_t)
{
  return low_object_type_to_program_type(obj_t);
}



int type_may_overload(struct pike_type *type, int lfun)
{
  switch(type->type)
  {
    case T_ASSIGN:
      return type_may_overload(type->cdr, lfun);
      
    case T_FUNCTION:
    case T_MANY:
    case T_ARRAY:
      /* might want to check for `() */
      
    default:
      return 0;

    case PIKE_T_NAME:
      return type_may_overload(type->cdr, lfun);

    case PIKE_T_RING:
      return type_may_overload(type->car, lfun);

    case T_OR:
      return type_may_overload(type->car, lfun) ||
	type_may_overload(type->cdr, lfun);
      
    case T_AND:
      return type_may_overload(type->car, lfun) &&
	type_may_overload(type->cdr, lfun);
      
    case T_NOT:
      return !type_may_overload(type->car, lfun);

    case T_MIXED:
      return 1;
      
    case T_OBJECT:
    {
      struct program *p = id_to_program((ptrdiff_t)type->cdr);
      if(!p) return 1;
      return FIND_LFUN(p, lfun)!=-1;
    }
  }
}


void yyexplain_nonmatching_types(struct pike_type *type_a,
				 struct pike_type *type_b,
				 int flags)
{
  implements_a=0;
  implements_b=0;

  match_types(type_a, type_b);

#if 0
  if(!(implements_a && implements_b &&
       type_a->str[0]==T_OBJECT &&
       type_b->str[0]==T_OBJECT))
#endif /* 0 */
  {
    struct pike_string *s1, *s2;
    s1 = describe_type(type_a);
    s2 = describe_type(type_b);
    if(flags & YYTE_IS_WARNING)
    {
      yywarning("Expected: %s",s1->str);
      yywarning("Got     : %s",s2->str);
    }else{
      my_yyerror("Expected: %s",s1->str);
      my_yyerror("Got     : %s",s2->str);
    }
    free_string(s1);
    free_string(s2);
  }

  if(implements_a && implements_b)
    yyexplain_not_implements(implements_a,implements_b,flags);
}


/******/

#ifdef DEBUG_MALLOC
#define low_make_pike_type(T,C) ((struct pike_type *)debug_malloc_pass(debug_low_make_pike_type(T,C)))
#define low_make_function_type(T,C) ((struct pike_type *)debug_malloc_pass(debug_low_make_function_type(T,C)))
#else /* !DEBUG_MALLOC */
#define low_make_pike_type debug_low_make_pike_type
#define low_make_function_type debug_low_make_function_type
#endif /* DEBUG_MALLOC */
  
static struct pike_type *debug_low_make_pike_type(unsigned char *type_string,
						  unsigned char **cont);

static struct pike_type *debug_low_make_function_type(unsigned char *type_string,
						      unsigned char **cont)
{
  struct pike_type *tmp;

  if (*type_string == T_MANY) {
    tmp = low_make_pike_type(type_string+1, cont);
    return mk_type(T_MANY, tmp,
		   low_make_pike_type(*cont, cont), PT_COPY_BOTH);
  }
  tmp = low_make_pike_type(type_string, cont);
  return mk_type(T_FUNCTION, tmp,
		 low_make_function_type(*cont, cont), PT_COPY_BOTH);
}

static struct pike_type *debug_low_make_pike_type(unsigned char *type_string,
						  unsigned char **cont)
{
  unsigned INT32 type;
  struct pike_type *tmp;

  switch(type = *type_string) {
  case T_SCOPE:
  case T_ASSIGN:
    if ((type_string[1] < '0') || (type_string[1] > '9')) {
      fatal("low_make_pike_type(): Bad marker: %d\n", type_string[1]);
    }
    return mk_type(type, (void *)(ptrdiff_t)(type_string[1] - '0'),
		   low_make_pike_type(type_string+2, cont), PT_COPY_CDR);
  case T_FUNCTION:
    /* Multiple ordered values. */
    /* FIXME: */
    return low_make_function_type(type_string+1, cont);
  case T_TUPLE:
  case T_MAPPING:
  case PIKE_T_RING:
    /* Order dependant */
    tmp = low_make_pike_type(type_string+1, cont);
    return mk_type(type, tmp,
		   low_make_pike_type(*cont, cont), PT_COPY_BOTH);
  case T_OR:
  case T_AND:
    /* Order independant */
    /* FIXME: */
    tmp = low_make_pike_type(type_string+1, cont);
    return mk_type(type, tmp,
		   low_make_pike_type(*cont, cont), PT_COPY_BOTH);
  case T_ARRAY:
  case T_MULTISET:
  case T_TYPE:
  case T_NOT:
  case T_PROGRAM:
    /* Single type */
    return mk_type(type, low_make_pike_type(type_string+1, cont), NULL,
		   PT_COPY_CAR);
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    /* Marker type */
    *cont = type_string+1;
    return mk_type(type, NULL, NULL, PT_SET_MARKER);

  case T_FLOAT:
  case T_STRING:
  case T_MIXED:
  case T_VOID:
  case T_ZERO:
  case PIKE_T_UNKNOWN:
    /* Leaf type */
    *cont = type_string+1;
    return mk_type(type, NULL, NULL, 0);

  case T_INT:
    *cont = type_string + 9;	/* 2*sizeof(INT32) + 1 */
    return mk_type(T_INT,
		   (void *)(ptrdiff_t)extract_type_int(type_string+1),
		   (void *)(ptrdiff_t)extract_type_int(type_string+5), 0);
  case T_OBJECT:
    *cont = type_string + 6;	/* 1 + sizeof(INT32) + 1 */
    return mk_type(T_OBJECT, (void *)(ptrdiff_t)(type_string[1]),
		   (void *)(ptrdiff_t)extract_type_int(type_string+2), 0);
  default:
    fatal("compile_type_string(): Error in type string %d.\n", type);
    /* NOT_REACHED */
    break;
  }
  /* NOT_REACHED */
  return NULL;
}

/* Make a pike-type from a serialized (old-style) type. */
struct pike_type *debug_make_pike_type(const char *serialized_type)
{
  return low_make_pike_type((unsigned char *)serialized_type,
			    (unsigned char **)&serialized_type);
}

int pike_type_allow_premature_toss(struct pike_type *type)
{
 again:
#if 0
  fprintf(stderr, "pike_type_allow_premature_toss(): Type: %d\n",
	  type->type);
#endif /* 0 */
  switch(type->type)
  {
    case T_NOT:
      return !pike_type_allow_premature_toss(type->car);

    case T_OBJECT:
    case T_MIXED:
    case T_FUNCTION:
    case T_MANY:
      return 0;

    case PIKE_T_NAME:
    case T_SCOPE:
    case T_ASSIGN:
      type = type->cdr;
      goto again;

    case PIKE_T_RING:
      type = type->car;
      goto again;

    case T_OR:
    case T_MAPPING:
      if(!pike_type_allow_premature_toss(type->car)) return 0;
      type = type->cdr;
      goto again;

    case T_AND:
      /* FIXME: Should probably look at both branches. */
      type = type->cdr;
      goto again;

    case T_ARRAY:
    case T_MULTISET:
      type = type->car;
      goto again;

    case T_PROGRAM:
    case T_TYPE:
    case T_INT:
    case T_FLOAT:
    case T_STRING:
    case T_VOID:
      return 1;
  default:
    fatal("pike_type_allow_premature_toss: Unknown type code (%d)\n",
	  ((unsigned char *)type)[-1]);
    /* NOT_REACHED */
    return 0;
  }
}

static void low_type_to_string(struct pike_type *t)
{
 recurse:
  switch(t->type) {
  case T_ARRAY:
  case T_MULTISET:
  case T_TYPE:
  case T_NOT:
  case T_PROGRAM:
    my_putchar(t->type);
    /* FALL_THROUGH */
  case PIKE_T_NAME:
    t = t->car;
    goto recurse;

  case PIKE_T_RING:
  case T_TUPLE:
  case T_MAPPING:
  case T_OR:
  case T_AND:
    my_putchar(t->type);
    low_type_to_string(t->car);
    t = t->cdr;
    goto recurse;

  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case T_STRING:
  case T_FLOAT:
  case T_ZERO:
  case T_VOID:
  case T_MIXED:
    my_putchar(t->type);
    break;

  case T_OBJECT:
    {
      INT32 i;
      my_putchar(T_OBJECT);
      i = (INT32)(ptrdiff_t)t->car;
      my_putchar( i );
      i = (INT32)(ptrdiff_t)t->cdr;

      if( i > 65535 )  i = 0; /* Not constant between recompilations */

      my_putchar((i >> 24) & 0xff);
      my_putchar((i >> 16) & 0xff);
      my_putchar((i >> 8)  & 0xff);
      my_putchar(i & 0xff);
    }
    break;

  case T_INT:
    {
      INT32 i;
      my_putchar(T_INT);
      i = (INT32)(ptrdiff_t)t->car;
      my_putchar((i >> 24) & 0xff);
      my_putchar((i >> 16) & 0xff);
      my_putchar((i >> 8) & 0xff);
      my_putchar(i & 0xff);
      i = (INT32)(ptrdiff_t)t->cdr;
      my_putchar((i >> 24) & 0xff);
      my_putchar((i >> 16) & 0xff);
      my_putchar((i >> 8) & 0xff);
      my_putchar(i & 0xff);
    }
    break;

  case T_FUNCTION:
  case T_MANY:
    my_putchar(T_FUNCTION);
    while(t->type == T_FUNCTION) {
      low_type_to_string(t->car);
      t = t->cdr;
    }
    my_putchar(T_MANY);
    low_type_to_string(t->car);
    t = t->cdr;
    goto recurse;

  case T_SCOPE:
  case T_ASSIGN:
    my_putchar(t->type);
    my_putchar('0' + (ptrdiff_t)t->car);
    t = t->cdr;
    goto recurse;

  default:
    Pike_error("low_type_to_string(): Unsupported node: %d\n", t->type);
    break;
  }
}

#else /* !USE_PIKE_TYPE */

static void internal_parse_type(char **s);
static ptrdiff_t type_length(const char *t);
static int low_pike_types_le(char *a, char *b,
			     int array_cnt, unsigned int flags);
static int low_check_indexing(char *type, char *index_type, node *n);

#define EXTRACT_TWOT(X,Y) TWOT(EXTRACT_UCHAR(X), EXTRACT_UCHAR(Y))

/*
 * basic types are represented by just their value in a string
 * basic type are string, type, int, float, object and program
 * arrays are coded like by the value T_ARRAY followed by the
 * data type, if the type is not known it is T_MIXED, ie:
 * T_ARRAY <data type>
 * mappings are followed by two arguments, the first is the type
 * for the indices, and the second is the type of the data, ie:
 * T_MAPPING <indice type> <data type>
 * multiset works similarly to arrays.
 * functions are _very_ special:
 * they are coded like this:
 * T_FUNCTION <arg type> <arg type> ... <arg type> T_MANY <arg type> <return type>
 * note that the type after T_MANY can be T_VOID
 * T_MIXED matches anything except T_VOID
 * PIKE_T_UNKNOWN only matches T_MIXED and PIKE_T_UNKNOWN
 * objects are coded thus:
 * T_OBJECT <0/1> <program_id>
 *           ^
 *           0 means 'implements'
 *           1 means 'is' (aka 'clone of')
 * Integers are encoded as:
 * T_INT <min> <max>
 * Everything except T_VOID matches T_ZERO.
 */

#ifdef PIKE_DEBUG
void check_type_string(struct pike_type *s)
{
  if(debug_findstring(s) != s)
    fatal("Type string not shared.\n");

  if(type_length(s->str) != s->len)
  {
    simple_describe_type(s);
    fatal("Length of type is wrong. (should be %ld, is %ld)\n",
	  PTRDIFF_T_TO_LONG(type_length(s->str)),
	  PTRDIFF_T_TO_LONG(s->len));
  }
}
#endif

static ptrdiff_t type_length(const char *t)
{
  const char *q=t;
one_more_type:
  switch(EXTRACT_UCHAR(t++))
  {
    default:
      fatal("Error in type string %d.\n",EXTRACT_UCHAR(t-1));
      /*NOTREACHED*/
      
      break;

    case T_SCOPE:
    case T_ASSIGN:
      t++;
      goto one_more_type;
      
    case T_FUNCTION:
      while(EXTRACT_UCHAR(t)!=T_MANY) t+=type_length(t); /* skip arguments */
      t++;
      
    case T_TUPLE:
    case T_MAPPING:
    case PIKE_T_RING:
    case T_OR:
    case T_AND:
      t+=type_length(t);
      
    case T_ARRAY:
    case T_MULTISET:
    case T_TYPE:
    case T_NOT:
    case T_PROGRAM:
      goto one_more_type;
      
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case T_FLOAT:
    case T_STRING:
    case T_MIXED:
    case T_VOID:
    case T_ZERO:
    case PIKE_T_UNKNOWN:
      break;

    case T_INT:
      t+=sizeof(INT32)*2;
      break;
      
    case T_OBJECT:
      t++;
      t+=sizeof(INT32);
      break;
  }
  return t-q;
}


unsigned char type_stack[PIKE_TYPE_STACK_SIZE];
unsigned char *pike_type_mark_stack[PIKE_TYPE_STACK_SIZE/4];


ptrdiff_t pop_stack_mark(void)
{ 
  Pike_compiler->pike_type_mark_stackp--;
  if(Pike_compiler->pike_type_mark_stackp<pike_type_mark_stack)
    fatal("Type mark stack underflow\n");

  return Pike_compiler->type_stackp - *Pike_compiler->pike_type_mark_stackp;
}

void debug_pop_type_stack(unsigned INT16 expected)
{ 
#ifdef PIKE_DEBUG
  if (Pike_compiler->type_stackp[0] != expected) {
    fatal("Unexpected type node: %d (expected: %d)\n",
	  Pike_compiler->type_stackp[0], expected);
  }
#endif /* PIKE_DEBUG */
  Pike_compiler->type_stackp--;
  if(Pike_compiler->type_stackp<type_stack)
    fatal("Type stack underflow\n");
}


void type_stack_pop_to_mark(void)
{
  Pike_compiler->type_stackp-=pop_stack_mark();
#ifdef PIKE_DEBUG
  if(Pike_compiler->type_stackp<type_stack)
    fatal("Type stack underflow\n");
#endif
}

void type_stack_reverse(void)
{
  ptrdiff_t a;
  a=pop_stack_mark();
  reverse((char *)(Pike_compiler->type_stackp-a),a,1);
}

static void push_type_int(INT32 i)
{
  ptrdiff_t e;
  for(e = 0; e < (ptrdiff_t)sizeof(i); e++)
    push_type(DO_NOT_WARN((unsigned char)((i>>(e*8)) & 0xff)));
}

static void push_type_int_backwards(INT32 i)
{
  int e;
  for(e=(int)sizeof(i);e-->0;)
    push_type( (i>>(e*8)) & 0xff );
}

void debug_push_int_type(INT32 min, INT32 max)
{
  push_type_int(max);
  push_type_int(min);
  push_type(T_INT);
}

void debug_push_assign_type(int marker)
{
  push_type(marker);
  push_type(T_ASSIGN);
}

void debug_push_object_type(int flag, INT32 id)
{
  push_type_int(id);
  push_type(flag);
  push_type(T_OBJECT);
}

void debug_push_object_type_backwards(int flag, INT32 id)
{
  unsafe_push_type(T_OBJECT);
  unsafe_push_type(flag);
  push_type_int_backwards(id);
}

INT32 extract_type_int(char *p)
{
  int e;
  INT32 ret=0;
  for(e=0;e<(int)sizeof(INT32);e++)
    ret=(ret<<8) | EXTRACT_UCHAR(p+e);
  return ret;
}

void debug_push_unfinished_type(char *s)
{
  ptrdiff_t e;
  e=type_length(s);
  for(e--;e>=0;e--) push_type(s[e]);
}

static void push_unfinished_type_with_markers(char *s,
					      struct pike_type **am)
{
  int d,e,c;
  ptrdiff_t len=type_length(s);

  type_stack_mark();
  for(e=0;e<len;e++)
  {
    switch(c=EXTRACT_UCHAR(s+e))
    {
#if 1
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	if(am[c-'0'])
	{
	  push_finished_type_backwards(am[c-'0']);
	}else{
	  push_type(T_ZERO);
	}
	break;
#endif
      case T_ASSIGN:
#if 1
	e++;
#else
	push_type(c);
	push_type(EXTRACT_UCHAR(s+ ++e));
#endif
	break;

      case T_INT:
	push_type(c);
	for(d=0;d<(int)sizeof(INT32)*2;d++)
	  push_type(EXTRACT_UCHAR(s+ ++e));
	break;
	
      case T_OBJECT:
	push_type(c);
	for(d=0;d<(int)sizeof(INT32)+1;d++) push_type(EXTRACT_UCHAR(s+ ++e));
	break;
	
      default:
	push_type(c);
    }
  }
  type_stack_reverse();
}

void debug_push_finished_type(struct pike_type *type)
{
  ptrdiff_t e;
  check_type_string(type);
  for(e=type->len-1;e>=0;e--) push_type(type->str[e]);
}

void debug_push_finished_type_backwards(struct pike_type *type)
{
  int e;
  check_type_string(type);
  MEMCPY(Pike_compiler->type_stackp, type->str, type->len);
  Pike_compiler->type_stackp+=type->len;
}

struct pike_type *debug_pop_unfinished_type(void)
{
  ptrdiff_t len, e;
  struct pike_type *s;
  len=pop_stack_mark();
  s=begin_shared_string(len);
  Pike_compiler->type_stackp-=len;
  MEMCPY(s->str, Pike_compiler->type_stackp, len);
  reverse(s->str, len, 1);
  s=end_shared_string(s);
  check_type_string(s);
  return s;
}

static void internal_parse_typeA(char **_s)
{
  char buf[80];
  unsigned int len;
  unsigned char **s = (unsigned char **)_s;
  
  while(ISSPACE(**s)) ++*s;

  len=0;
  for(len=0;isidchar(EXTRACT_UCHAR(s[0]+len));len++)
  {
    if(len>=sizeof(buf)) Pike_error("Buffer overflow in parse_type\n");
    buf[len] = s[0][len];
  }
  buf[len]=0;
  *s += len;
  
  switch(buf[0])
  {
    case 'z':
      if(!strcmp(buf,"zero")) { push_type(T_ZERO); break; }
      goto bad_type;

    case 'i':
      if(!strcmp(buf,"int"))
      {
	INT32 min = MIN_INT32, max = MAX_INT32;

	while(ISSPACE(**s)) ++*s;
	if(**s=='(')
	{
	  ++*s;
	  while(ISSPACE(**s)) ++*s;
	  min=STRTOL((char *)*s,(char **)s,0);
	  while(ISSPACE(**s)) ++*s;
	  if(s[0][0]=='.' && s[0][1]=='.')
	    s[0]+=2;
	  else
	    Pike_error("Missing .. in integer type.\n");
	  
	  while(ISSPACE(**s)) ++*s;
	  max=STRTOL((char *)*s,(char **)s,0);
	  while(ISSPACE(**s)) ++*s;

	  if(**s != ')') Pike_error("Missing ')' in integer range.\n");
	  ++*s;
	}
	push_int_type(min, max);
	break;
      }
      goto bad_type;

    case 'f':
      if(!strcmp(buf,"function"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  ++*s;
	  while(ISSPACE(**s)) ++*s;
	  type_stack_mark();
	  while(1)
	  {
	    if(**s == ':')
	    {
	      push_type(T_MANY);
	      push_type(T_VOID);
	      break;
	    }
	    
	    type_stack_mark();
	    type_stack_mark();
	    type_stack_mark();
	    internal_parse_type(_s);
	    type_stack_reverse();
	    if(**s==',')
	    {
	      ++*s;
	      while(ISSPACE(**s)) ++*s;
	    }
	    else if(s[0][0]=='.' && s[0][1]=='.' && s[0][2]=='.')
	    {
	      type_stack_reverse();
	      push_type(T_MANY);
	      type_stack_reverse();
	      *s+=3;
	      while(ISSPACE(**s)) ++*s;
	      if(**s != ':') Pike_error("Missing ':' after ... in function type.\n");
	      break;
	    }
	    pop_stack_mark();
	    pop_stack_mark();
	  }
	  ++*s;
	  type_stack_mark();
	  internal_parse_type(_s);  /* return type */
	  type_stack_reverse();
	  if(**s != ')') Pike_error("Missing ')' in function type.\n");
	  ++*s;
	  type_stack_reverse(); 
	}else{
	  push_type(T_VOID);
	  push_type(T_MIXED);
	  push_type(T_OR);
	  push_type(T_VOID);
	  push_type(T_ZERO);
	  push_type(T_OR);
	  push_type(T_MANY);
	}
	push_type(T_FUNCTION);
	break;
      }
      if(!strcmp(buf,"float")) { push_type(T_FLOAT); break; }
      goto bad_type;

    case 'o':
      if(!strcmp(buf,"object"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(') /* object({is,implements} id) */
	{
	  int is = 1, id;
	  ++*s;
	  if( **s != 'i' )
	    goto bad_type;
	  ++*s;
	  if( **s == 'm' )
	    is = 0;
	  while( isidchar( **s ) ) ++*s;
	  while( ISSPACE(**s) )      ++*s;
	  if( !**s )
	    goto bad_type;
	  id = atoi( *s );	
	  while( **s >= '0' && **s <= '9' )
	    ++*s;
	  while(ISSPACE(**s)) ++*s;
	  if( !**s || **s != ')' )
	    goto bad_type;
	  ++*s;
	  push_object_type(is, id);
	}
	else
	  push_object_type(0, 0);
	break;
      }
      goto bad_type;

    case 'p':
      if(!strcmp(buf,"program")) {
	push_object_type(0, 0);
	push_type(T_PROGRAM);
	break;
      }
      goto bad_type;


    case 's':
      if(!strcmp(buf,"string")) { push_type(T_STRING); break; }
      goto bad_type;

    case 'v':
      if(!strcmp(buf,"void")) { push_type(T_VOID); break; }
      goto bad_type;

    case 't':
      if (!strcmp(buf,"tuple"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  type_stack_mark();
	  ++*s;
	  type_stack_mark();
	  internal_parse_type(_s);
	  type_stack_reverse();
	  if(**s != ',') Pike_error("Expecting ','.\n");
	  ++*s;
	  type_stack_mark();
	  internal_parse_type(_s);
	  type_stack_reverse();
	  if(**s != ')') Pike_error("Expecting ')'.\n");
	  ++*s;
	  type_stack_reverse();
	}else{
	  push_type(T_MIXED);
	  push_type(T_MIXED);
	}
	push_type(T_TUPLE);
	break;
      }
      /* FIXME: Support for type(T). */
      if(!strcmp(buf,"type")) { push_type(T_MIXED); push_type(T_TYPE); break; }
      goto bad_type;

    case 'm':
      if(!strcmp(buf,"mixed")) { push_type(T_MIXED); break; }
      if(!strcmp(buf,"mapping"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  type_stack_mark();
	  ++*s;
	  type_stack_mark();
	  internal_parse_type(_s);
	  type_stack_reverse();
	  if(**s != ':') Pike_error("Expecting ':'.\n");
	  ++*s;
	  type_stack_mark();
	  internal_parse_type(_s);
	  type_stack_reverse();
	  if(**s != ')') Pike_error("Expecting ')'.\n");
	  ++*s;
	  type_stack_reverse();
	}else{
	  push_type(T_MIXED);
	  push_type(T_MIXED);
	}
	push_type(T_MAPPING);
	break;
      }
      if(!strcmp(buf,"multiset"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  ++*s;
	  internal_parse_type(_s);
	  if(**s != ')') Pike_error("Expecting ')'.\n");
	  ++*s;
	}else{
	  push_type(T_MIXED);
	}
	push_type(T_MULTISET);
	break;
      }
      goto bad_type;

    case 'u':
      if(!strcmp(buf,"unknown")) { push_type(PIKE_T_UNKNOWN); break; }
      goto bad_type;

    case 'a':
      if(!strcmp(buf,"array"))
      {
	while(ISSPACE(**s)) ++*s;
	if(**s == '(')
	{
	  ++*s;
	  internal_parse_type(_s);
	  if(**s != ')') Pike_error("Expecting ')'.\n");
	  ++*s;
	}else{
	  push_type(T_MIXED);
	}
	push_type(T_ARRAY);
	break;
      }
      goto bad_type;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if(atoi(buf)<10)
      {
	while(ISSPACE(**s)) ++*s;
	if(**s=='=')
	{
	  ++*s;
	  internal_parse_type(_s);
	  push_type(buf[0]);
	  push_type(T_ASSIGN);
	}else{
	  push_type(buf[0]);
	}
	break;
      }

    default:
  bad_type:
      Pike_error("Couldn't parse type. (%s)\n",buf);
  }

  while(ISSPACE(**s)) ++*s;
}


static void internal_parse_typeB(char **s)
{
  while(ISSPACE(**((unsigned char **)s))) ++*s;
  switch(**s)
  {
  case '!':
    ++*s;
    internal_parse_typeB(s);
    push_type(T_NOT);
    break;

  case '(':
    ++*s;
    internal_parse_type(s);
    while(ISSPACE(**((unsigned char **)s))) ++*s;
    if(**s != ')') Pike_error("Expecting ')'.\n");
    ++*s;
    break;
    
  default:

    internal_parse_typeA(s);
  }
}

static void internal_parse_typeCC(char **s)
{
  internal_parse_typeB(s);

  while(ISSPACE(**((unsigned char **)s))) ++*s;
  
  while(**s == '*')
  {
    ++*s;
    while(ISSPACE(**((unsigned char **)s))) ++*s;
    push_type(T_ARRAY);
  }
}

static void internal_parse_typeC(char **s)
{
  type_stack_mark();

  type_stack_mark();
  internal_parse_typeCC(s);
  type_stack_reverse();

  while(ISSPACE(**((unsigned char **)s))) ++*s;
  
  if(**s == '&')
  {
    ++*s;
    type_stack_mark();
    internal_parse_typeC(s);
    type_stack_reverse();
    type_stack_reverse();
    push_type(T_AND);
  }else{
    type_stack_reverse();
  }
}

static void internal_parse_type(char **s)
{
  internal_parse_typeC(s);

  while(ISSPACE(**((unsigned char **)s))) ++*s;
  
  while(**s == '|')
  {
    ++*s;
    internal_parse_typeC(s);
    push_type(T_OR);
  }
}

/* This function is used when adding simul efuns so that
 * the types for the functions can be easily stored in strings.
 * It takes a string on the exact same format as Pike and returns a type
 * struct.
 */
struct pike_type *parse_type(char *s)
{
  struct pike_type *ret;
#ifdef PIKE_DEBUG
  unsigned char *ts=Pike_compiler->type_stackp;
  unsigned char **ptms=Pike_compiler->pike_type_mark_stackp;
#endif
  type_stack_mark();
  internal_parse_type(&s);

  if( *s )
    fatal("Extra junk at end of type definition.\n");

  ret=pop_unfinished_type();

#ifdef PIKE_DEBUG
  if(ts!=Pike_compiler->type_stackp || ptms!=Pike_compiler->pike_type_mark_stackp)
    fatal("Type stack whacked in parse_type.\n");
#endif

  return ret;
}

#ifdef PIKE_DEBUG
void stupid_describe_type(char *a, ptrdiff_t len)
{
  ptrdiff_t e;
  for(e=0;e<len;e++)
  {
    if(e) fputs(" ", stderr);
    switch(EXTRACT_UCHAR(a+e))
    {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	fprintf(stderr, "%c",EXTRACT_UCHAR(a+e));
	break;

      case T_SCOPE: fputs("scope", stderr); break;
      case T_TUPLE: fputs("tuple", stderr); break;
      case T_ASSIGN: fputs("=", stderr); break;
      case T_INT:
	{
	  INT32 min=extract_type_int(a+e+1);
	  INT32 max=extract_type_int(a+e+1+sizeof(INT32));
	  fprintf(stderr, "int");
	  if(min!=MIN_INT32 || max!=MAX_INT32)
	    fprintf(stderr, "(%ld..%ld)",(long)min,(long)max);
	  e+=sizeof(INT32)*2;
	  break;
	}
      case T_FLOAT: fputs("float", stderr); break;
      case T_STRING: fputs("string", stderr); break;
      case T_TYPE: fputs("type", stderr); break;
      case T_PROGRAM: fputs("program", stderr); break;
      case T_OBJECT:
	fprintf(stderr, "object(%s %ld)",
		EXTRACT_UCHAR(a+e+1)?"is":"implements",
		(long)extract_type_int(a+e+2));
	e+=sizeof(INT32)+1;
	break;
      case T_FUNCTION: fputs("function", stderr); break;
      case T_ARRAY: fputs("array", stderr); break;
      case T_MAPPING: fputs("mapping", stderr); break;
      case T_MULTISET: fputs("multiset", stderr); break;
	
      case PIKE_T_UNKNOWN: fputs("unknown", stderr); break;
      case T_MANY: fputs("many", stderr); break;
      case PIKE_T_RING: fputs("ring", stderr); break;
      case T_OR: fputs("or", stderr); break;
      case T_AND: fputs("and", stderr); break;
      case T_NOT: fputs("not", stderr); break;
      case T_VOID: fputs("void", stderr); break;
      case T_ZERO: fputs("zero", stderr); break;
      case T_MIXED: fputs("mixed", stderr); break;
	
      default: fprintf(stderr, "%d",EXTRACT_UCHAR(a+e)); break;
    }
  }
}

void simple_describe_type(struct pike_type *s)
{
  stupid_describe_type(s->str,s->len);
  fputs("\n", stderr);
}
#endif

static char *low_describe_type(char *t)
{
  switch(EXTRACT_UCHAR(t++))
  {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      my_putchar(EXTRACT_UCHAR(t-1));
      break;
      
    case T_ASSIGN:
      my_putchar('(');
      my_putchar(EXTRACT_UCHAR(t++));
      my_putchar('=');
      t=low_describe_type(t);
      my_putchar(')');
      break;

    case T_SCOPE:
      my_putchar('{');
      my_putchar(EXTRACT_UCHAR(t++));
      my_putchar(',');
      t = low_describe_type(t);
      my_putchar('}');
      break;

    case T_TUPLE:
      my_putchar('[');
      t = low_describe_type(t);
      my_putchar(',');
      t = low_describe_type(t);
      my_putchar(']');
      break;

    case T_VOID: my_strcat("void"); break;
    case T_ZERO: my_strcat("zero"); break;
    case T_MIXED: my_strcat("mixed"); break;
    case PIKE_T_UNKNOWN: my_strcat("unknown"); break;
    case T_INT:
    {
      INT32 min=extract_type_int(t);
      INT32 max=extract_type_int(t+sizeof(INT32));
      my_strcat("int");
      
      if(min!=MIN_INT32 || max!=MAX_INT32)
      {
	char buffer[100];
	sprintf(buffer,"(%ld..%ld)",(long)min,(long)max);
	my_strcat(buffer);
      }
      t+=sizeof(INT32)*2;
      
      break;
    }
    case T_FLOAT: my_strcat("float"); break;
    case T_PROGRAM:
      my_strcat("program(");
      t = low_describe_type(t);
      my_strcat(")");
      break;
    case T_OBJECT:
      if(extract_type_int(t+1))
      {
	char buffer[100];
	sprintf(buffer,"object(%s %ld)",*t?"is":"implements",
		(long)extract_type_int(t+1));
	my_strcat(buffer);
      }else{
	my_strcat("object");
      }
      
      t+=sizeof(INT32)+1;
      /* Prog id */
      break;
    case T_STRING: my_strcat("string"); break;
    case T_TYPE:
      my_strcat("type(");
      t = low_describe_type(t);
      my_strcat(")");
      break;
      
    case T_FUNCTION:
    {
      int s;
      my_strcat("function");
      if(EXTRACT_UCHAR(t) == T_MANY &&
	 ((EXTRACT_UCHAR(t+1) == T_ZERO &&
	   EXTRACT_UCHAR(t+2) == T_OR &&
	   ((EXTRACT_UCHAR(t+3) == T_MIXED && EXTRACT_UCHAR(t+4) == T_VOID) ||
	    (EXTRACT_UCHAR(t+4) == T_MIXED && EXTRACT_UCHAR(t+3) == T_VOID)))
	  ||
	  (EXTRACT_UCHAR(t+1) == T_OR
	   &&
	   ((EXTRACT_UCHAR(t+2) == T_ZERO && EXTRACT_UCHAR(t+3) == T_VOID) ||
	    (EXTRACT_UCHAR(t+3) == T_ZERO && EXTRACT_UCHAR(t+2) == T_VOID))
	   &&
	   EXTRACT_UCHAR(t+4) == T_OR
	   &&
	   ((EXTRACT_UCHAR(t+5) == T_MIXED && EXTRACT_UCHAR(t+6) == T_VOID) ||
	    (EXTRACT_UCHAR(t+6) == T_MIXED && EXTRACT_UCHAR(t+5) == T_VOID)))))
      {
	/* done */
	if (EXTRACT_UCHAR(t+1) == T_ZERO) {
	  /* function(zero...:mixed|void) */
	  t += 5;
	} else {
	  /* function(zero|void...mixed|void) */
	  t += 7;
	}
      } else {
	my_strcat("(");
	s=0;
	while(EXTRACT_UCHAR(t) != T_MANY)
	{
	  if(s++) my_strcat(", ");
	  t=low_describe_type(t);
	}
	t++;
	if(EXTRACT_UCHAR(t) == T_VOID)
	{
	  t++;
	}else{
	  if(s++) my_strcat(", ");
	  t=low_describe_type(t);
	  my_strcat(" ...");
	}
	my_strcat(" : ");
	t=low_describe_type(t);
	my_strcat(")");
      }
      break;
    }
    
    case T_ARRAY:
      my_strcat("array");
      if(EXTRACT_UCHAR(t)==T_MIXED)
      {
	t++;
      }else{
	my_strcat("(");
	t=low_describe_type(t);
	my_strcat(")");
      }
      break;
      
    case T_MULTISET:
      my_strcat("multiset");
      if(EXTRACT_UCHAR(t)!=T_MIXED)
      {
	my_strcat("(");
	t=low_describe_type(t);
	my_strcat(")");
      }else{
	t++;
      }
      break;
      
    case T_NOT:
      my_strcat("!");
      if (EXTRACT_UCHAR(t) > T_NOT) {
	my_strcat("(");
	t=low_describe_type(t);
	my_strcat(")");
      } else {
	t=low_describe_type(t);
      }
      break;

    case PIKE_T_RING:
      my_strcat("(");
      t = low_describe_type(t);
      my_strcat(")�(");
      t = low_describe_type(t);
      my_strcat(")");
      break;
      
    case T_OR:
      if (EXTRACT_UCHAR(t) > T_OR) {
	my_strcat("(");
	t=low_describe_type(t);
	my_strcat(")");
      } else {
	t=low_describe_type(t);
      }
      my_strcat(" | ");
      if (EXTRACT_UCHAR(t) > T_OR) {
	my_strcat("(");
	t=low_describe_type(t);
	my_strcat(")");
      } else {
	t=low_describe_type(t);
      }
      break;
      
    case T_AND:
      if (EXTRACT_UCHAR(t) > T_AND) {
	my_strcat("(");
	t=low_describe_type(t);
	my_strcat(")");
      } else {
	t=low_describe_type(t);
      }
      my_strcat(" & ");
      if (EXTRACT_UCHAR(t) > T_AND) {
	my_strcat("(");
	t=low_describe_type(t);
	my_strcat(")");
      } else {
	t=low_describe_type(t);
      }
      break;
      
    case T_MAPPING:
      my_strcat("mapping");
      if(EXTRACT_UCHAR(t)==T_MIXED && EXTRACT_UCHAR(t+1)==T_MIXED)
      {
	t+=2;
      }else{
	my_strcat("(");
	t=low_describe_type(t);
	my_strcat(":");
	t=low_describe_type(t);
	my_strcat(")");
      }
      break;
    default:
      {
	char buf[20];
	my_strcat("unknown code(");
	sprintf(buf, "%d", EXTRACT_UCHAR(t-1));
	my_strcat(buf);
	my_strcat(")");
	break;
      }
  }
  return t;
}

void my_describe_type(struct pike_type *type)
{
  low_describe_type(type->str);
}

struct pike_string *describe_type(struct pike_type *type)
{
  check_type_string(type);
  if(!type) return make_shared_string("mixed");
  init_buf();
  my_describe_type(type);
  return free_buf();
}

static int low_is_same_type(char *a, char *b)
{
  if(type_length(a) != type_length(b)) return 0;
  return !MEMCMP(a,b,type_length(a));
}

static TYPE_T low_compile_type_to_runtime_type(char *t)
{
  TYPE_T tmp;
  switch(EXTRACT_UCHAR(t))
  {
  case T_OR:
    t++;
    tmp=low_compile_type_to_runtime_type(t);
    if(tmp == low_compile_type_to_runtime_type(t+type_length(t)))
      return tmp;

    case T_TUPLE:
      /* FIXME: Shouldn't occur/should be converted to array? */
      /* FALL_THROUGH */
    default:
      return T_MIXED;

  case T_ZERO:
    return T_INT;

  case PIKE_T_RING:
    return low_compile_type_to_runtime_type(t+1);

    case T_SCOPE:
      return low_compile_type_to_runtime_type(t+2);

    case T_ARRAY:
    case T_MAPPING:
    case T_MULTISET:

    case T_OBJECT:
    case T_PROGRAM:
    case T_FUNCTION:

    case T_STRING:
    case T_TYPE:
    case T_INT:
    case T_FLOAT:
      return EXTRACT_UCHAR(t);
  }
}

TYPE_T compile_type_to_runtime_type(struct pike_type *s)
{
  return low_compile_type_to_runtime_type(s->str);
}


static int low_find_exact_type_match(char *needle, char *haystack,
				     int separator)
{
  while(EXTRACT_UCHAR(haystack) == separator)
  {
    haystack++;
    if(low_find_exact_type_match(needle, haystack, separator))
      return 1;
    haystack+=type_length(haystack);
  }
  return low_is_same_type(needle, haystack);
}

static void very_low_or_pike_types(char *to_push, char *not_push)
{
  while(EXTRACT_UCHAR(to_push)==T_OR)
  {
    to_push++;
    very_low_or_pike_types(to_push, not_push);
    to_push+=type_length(to_push);
  }
  /* FIXME:
   * this might use the 'le' operator
   */

  if(!low_find_exact_type_match(to_push, not_push, T_OR))
  {
    push_unfinished_type(to_push);
    push_type(T_OR);
  }
}

static void low_or_pike_types(char *t1, char *t2, int zero_implied)
{
  if(!t1)
  {
    if(!t2)
      push_type(T_VOID);
    else
      push_unfinished_type(t2);
  }
  else if((!t2)
	  || (EXTRACT_UCHAR(t2) == T_ZERO && zero_implied)

    )
  {
    push_unfinished_type(t1);
  }
  else if (EXTRACT_UCHAR(t1) == T_ZERO && zero_implied)
  {
    push_unfinished_type(t2);
  }
  else if(EXTRACT_UCHAR(t1)==T_MIXED || EXTRACT_UCHAR(t2)==T_MIXED)
  {
    push_type(T_MIXED);
  }
  else if(EXTRACT_UCHAR(t1)==T_INT && EXTRACT_UCHAR(t2)==T_INT)
  {
    /* FIXME:
     * This should only be done if the ranges are
     * overlapping or adjecant to each other. /Hubbe
     */
    INT32 i1,i2;
    i1=extract_type_int(t1+1+sizeof(INT32));
    i2=extract_type_int(t2+1+sizeof(INT32));
    push_type_int(MAXIMUM(i1,i2));

    i1=extract_type_int(t1+1);
    i2=extract_type_int(t2+1);
    push_type_int(MINIMUM(i1,i2));

    push_type(T_INT);
  }
  else if (EXTRACT_UCHAR(t1) == T_SCOPE)
  {
    if (EXTRACT_UCHAR(t2) == T_SCOPE) {
      low_or_pike_types(t1+2, t2+2, zero_implied);
      if (EXTRACT_UCHAR(t1+1) > EXTRACT_UCHAR(t2+1))
	push_type(EXTRACT_UCHAR(t1+1));
      else
	push_type(EXTRACT_UCHAR(t2+1));
    } else {
      low_or_pike_types(t1+2, t2, zero_implied);
      push_type(EXTRACT_UCHAR(t1+1));
    }
    push_type(T_SCOPE);
  }
  else if (EXTRACT_UCHAR(t2) == T_SCOPE)
  {
    low_or_pike_types(t1, t2+2, zero_implied);
    push_type(EXTRACT_UCHAR(t2+1));
    push_type(T_SCOPE);
  }
  else
  {
    push_unfinished_type(t1);
    very_low_or_pike_types(t2,t1);
  }
}

static void medium_or_pike_types(struct pike_type *a,
				 struct pike_type *b,
				 int zero_implied)
{
  low_or_pike_types( a ? a->str : 0 , b ? b->str : 0 , zero_implied);
}

struct pike_type *or_pike_types(struct pike_type *a,
				struct pike_type *b,
				int zero_implied)
{
  type_stack_mark();
  medium_or_pike_types(a,b,1 /*zero_implied*/);
  return pop_unfinished_type();
}

static void very_low_and_pike_types(char *to_push, char *not_push)
{
  while(EXTRACT_UCHAR(to_push)==T_AND)
  {
    to_push++;
    very_low_and_pike_types(to_push, not_push);
    to_push+=type_length(to_push);
  }
  if(!low_find_exact_type_match(to_push, not_push, T_AND))
  {
    push_unfinished_type(to_push);
    push_type(T_AND);
  }
}

static void even_lower_and_pike_types(char *t1, char *t2)
{
  while(EXTRACT_UCHAR(t2) == T_OR)
  {
    t2++;
    even_lower_and_pike_types(t1, t2);
    t2 += type_length(t2);
  }
  if (EXTRACT_UCHAR(t1) == EXTRACT_UCHAR(t2)) {
    if (EXTRACT_UCHAR(t1) == T_INT) {
      INT32 i1,i2;
      INT32 upper_bound,lower_bound;
      i1=extract_type_int(t1+1+sizeof(INT32));
      i2=extract_type_int(t2+1+sizeof(INT32));
      upper_bound = MINIMUM(i1,i2);

      i1=extract_type_int(t1+1);
      i2=extract_type_int(t2+1);
      lower_bound = MAXIMUM(i1,i2);

      if (upper_bound >= lower_bound) {
	push_type_int(upper_bound);
	push_type_int(lower_bound);
	push_type(T_INT);
	push_type(T_OR);
      }
    } else {
      push_unfinished_type(t1);
      push_type(T_OR);
    }
  }
}

static int lower_and_pike_types(char *t1, char *t2)
{
  int is_complex = 0;
  while(EXTRACT_UCHAR(t1)==T_OR)
  {
    t1++;
    is_complex |= lower_and_pike_types(t1, t2);
    t1 += type_length(t1);
  }
  switch(EXTRACT_UCHAR(t1)) {
  case T_ZERO:
  case T_VOID:
    break;
  case T_STRING:
  case T_FLOAT:
  case T_INT:
    even_lower_and_pike_types(t1, t2);
    break;
  default:
    return 1;
  }
  return is_complex;
}

static int low_and_push_complex_pike_type(char *type)
{
  int is_complex = 0;
  while(EXTRACT_UCHAR(type) == T_OR)
  {
    int new_complex;
    type++;
    new_complex = low_and_push_complex_pike_type(type);
    if (new_complex) {
      if (is_complex) {
	push_type(T_OR);
      } else {
	is_complex = 1;
      }
    }
    type += type_length(type);
  }
  switch(EXTRACT_UCHAR(type)) {
  case T_VOID:
  case T_ZERO:
  case T_STRING:
  case T_FLOAT:
  case T_INT:
    /* Simple type. Already handled. */
    break;
  default:
    push_unfinished_type(type);
    if (is_complex) {
      push_type(T_OR);
    }
    return 1;
  }
  return is_complex;
}

static void low_and_pike_types(char *t1, char *t2)
{
  if(!t1 || EXTRACT_UCHAR(t1) == T_VOID ||
     !t2 || EXTRACT_UCHAR(t2) == T_VOID)
  {
    push_type(T_VOID);
  }
  else if(EXTRACT_UCHAR(t1) == T_ZERO ||
	  EXTRACT_UCHAR(t2) == T_ZERO)
  {
    push_type(T_ZERO);
  }
  else if(EXTRACT_UCHAR(t1)==T_MIXED)
  {
    push_unfinished_type(t2);
  }
  else if(EXTRACT_UCHAR(t2)==T_MIXED)
  {
    push_unfinished_type(t1);
  }
  else if(EXTRACT_UCHAR(t1)==T_INT && EXTRACT_UCHAR(t2)==T_INT)
  {
    INT32 i1,i2;
    INT32 upper_bound,lower_bound;
    i1=extract_type_int(t1+1+sizeof(INT32));
    i2=extract_type_int(t2+1+sizeof(INT32));
    upper_bound = MINIMUM(i1,i2);

    i1=extract_type_int(t1+1);
    i2=extract_type_int(t2+1);
    lower_bound = MAXIMUM(i1,i2);

    if (upper_bound >= lower_bound) {
      push_type_int(upper_bound);
      push_type_int(lower_bound);
      push_type(T_INT);
    } else {
      /* No overlap! */
      /* FIXME: Warn? */
      push_type(T_VOID);
    }
  }
  else if (EXTRACT_UCHAR(t1) == T_SCOPE)
  {
    if (EXTRACT_UCHAR(t2) == T_SCOPE) {
      low_and_pike_types(t1+2, t2+2);
      if (EXTRACT_UCHAR(t1+1) > EXTRACT_UCHAR(t2+1))
	push_type(EXTRACT_UCHAR(t1+1));
      else
	push_type(EXTRACT_UCHAR(t2+1));
    } else {
      low_and_pike_types(t1+2, t2);
      push_type(EXTRACT_UCHAR(t1+1));
    }
    push_type(T_SCOPE);
  }
  else if (EXTRACT_UCHAR(t2) == T_SCOPE)
  {
    low_and_pike_types(t1, t2+2);
    push_type(EXTRACT_UCHAR(t2+1));
    push_type(T_SCOPE);
  }
  else if((EXTRACT_UCHAR(t1)==T_STRING && EXTRACT_UCHAR(t2)==T_STRING) ||
	  (EXTRACT_UCHAR(t1)==T_FLOAT && EXTRACT_UCHAR(t2)==T_FLOAT))
  {
    push_unfinished_type(t1);
  }
  else if(low_pike_types_le(t1, t2, 0, 0))
  {
    push_unfinished_type(t1);
  }
  else if(low_pike_types_le(t2, t1, 0, 0))
  {
    push_unfinished_type(t2);
  }
  else
  {
    push_type(T_ZERO);

    if (lower_and_pike_types(t1, t2)) {
      /* t1 contains complex types. */
      if (low_and_push_complex_pike_type(t2)) {
	/* t2 also contains complex types. */
	low_and_push_complex_pike_type(t1);
	push_type(T_AND);
	push_type(T_OR);
      }
    }
    /*     push_unfinished_type(t1); */
    /*     very_low_and_pike_types(t2,t1); */
  }
}

static void medium_and_pike_types(struct pike_type *a,
				  struct pike_type *b)
{
  low_and_pike_types( a ? a->str : 0 , b ? b->str : 0 );
}

struct pike_type *and_pike_types(struct pike_type *a,
				 struct pike_type *b)
{
  type_stack_mark();
  medium_and_pike_types(a,b);
  return pop_unfinished_type();
}

static struct pike_type *low_object_lfun_type(char *t, short lfun)
{
  struct program *p;
  int i;
  p=id_to_program(extract_type_int(t+2));
  if(!p) return 0;
  i=FIND_LFUN(p, lfun);
  if(i==-1) return 0;
  return ID_FROM_INT(p, i)->type;
}

/*
 * match two type strings, return zero if they don't match, and return
 * the part of 'a' that _did_ match if it did.
 */
static char *low_match_types(char *a,char *b, int flags)
#ifdef PIKE_TYPE_DEBUG
{
  int e;
  char *s;
  static char *low_match_types2(char *a,char *b, int flags);

  if (l_flag>2) {
    init_buf();
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("low_match_types(");
    low_describe_type(a);
    if(type_length(a) + type_length(b) > 10)
    {
      my_strcat(",\n");
      for(e=0;e<indent;e++) my_strcat("  ");
      my_strcat("                ");
      low_describe_type(b);
      my_strcat(",\n");
      for(e=0;e<indent;e++) my_strcat("  ");
      my_strcat("                ");
    }else{
      my_strcat(", ");
      low_describe_type(b);
      my_strcat(", ");
    }
    if (flags) {
      int f = 0;
      if (flags & A_EXACT) {
	my_strcat("A_EXACT");
	f = 1;
      }
      if (flags & B_EXACT) {
	if (f) {
	  my_strcat(" | ");
	}
	my_strcat("B_EXACT");
	f = 1;
      }
      if (flags & NO_MAX_ARGS) {
	if (f) {
	  my_strcat(" | ");
	}
	my_strcat("NO_MAX_ARGS");
	f = 1;
      }
      if (flags & NO_SHORTCUTS) {
	if (f) {
	  my_strcat(" | ");
	}
	my_strcat("NO_SHORTCUTS");
	f = 1;
      }
    } else {
      my_strcat("0");
    }
    my_strcat(");\n");
    fprintf(stderr,"%s",(s=simple_free_buf()));
    free(s);
    indent++;
  }

  a=low_match_types2(a,b,flags);

  if (l_flag>2) {
    indent--;
    init_buf();
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("= ");
    if(a)
      low_describe_type(a);
    else
      my_strcat("NULL");
    my_strcat("\n");
    fprintf(stderr,"%s",(s=simple_free_buf()));
    free(s);
  }
  return a;
}

static char *low_match_types2(char *a,char *b, int flags)
#endif

{
  int correct_args;
  char *ret;
  if(a == b) return a;

  switch(EXTRACT_UCHAR(a))
  {
  case T_AND:
    a++;
    ret=low_match_types(a,b,flags);
    if(!ret) return 0;
    a+=type_length(a);
    return low_match_types(a,b,flags);

  case T_OR:
    a++;
    ret=low_match_types(a,b,flags);
    if(ret && !(flags & NO_SHORTCUTS)) return ret;
    a+=type_length(a);
    if(ret)
    {
      low_match_types(a,b,flags);
      return ret;
    }else{
      return low_match_types(a,b,flags);
    }

  case PIKE_T_RING:
    return low_match_types(a+1, b, flags);

  case T_NOT:
    if(low_match_types(a+1,b,(flags ^ B_EXACT ) | NO_MAX_ARGS))
      return 0;
    return a;

    case T_ASSIGN:
      ret=low_match_types(a+2,b,flags);
      if(ret && EXTRACT_UCHAR(b)!=T_VOID)
      {
	int m=EXTRACT_UCHAR(a+1)-'0';
	struct pike_type *tmp;
	type_stack_mark();
	push_unfinished_type_with_markers(b, b_markers);
	tmp=pop_unfinished_type();

	type_stack_mark();
	medium_or_pike_types(a_markers[m], tmp, 0);
	if(a_markers[m]) free_type(a_markers[m]);
	free_type(tmp);
	a_markers[m]=pop_unfinished_type();

#ifdef PIKE_TYPE_DEBUG
	if (l_flag>2) {
	  char *s;
	  int e;
	  init_buf();
	  for(e=0;e<indent;e++) my_strcat("  ");
	  my_strcat("a_markers[");
	  my_putchar((char)(m+'0'));
	  my_strcat("]=");
	  low_describe_type(a_markers[m]->str);
	  my_strcat("\n");
	  fprintf(stderr,"%s",(s=simple_free_buf()));
	  free(s);
	}
#endif
#ifdef PIKE_DEBUG
	if(a_markers[m]->str[0] == m+'0')
	  fatal("Cyclic type!\n");
#endif
      }
      return ret;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    {
      int m=EXTRACT_UCHAR(a)-'0';
      if(a_markers[m])
      {
#ifdef PIKE_DEBUG
	if(a_markers[m]->str[0] == EXTRACT_UCHAR(a))
	  fatal("Cyclic type!\n");
	if(EXTRACT_UCHAR(a_markers[m]->str) == T_OR &&
	  a_markers[m]->str[1] == EXTRACT_UCHAR(a))
	  fatal("Cyclic type!\n");
#endif
	return low_match_types(a_markers[m]->str, b, flags);
      }
      else
	return low_match_types(mixed_type_string->str, b, flags);
    }
  }

  switch(EXTRACT_UCHAR(b))
  {
  case T_AND:
    b++;
    ret=low_match_types(a,b,flags);
    if(!ret) return 0;
    b+=type_length(b);
    return low_match_types(a,b,flags);

  case T_OR:
    b++;
    ret=low_match_types(a,b,flags);
    if(ret && !(flags & NO_SHORTCUTS)) return ret;
    b+=type_length(b);
    if(ret)
    {
      low_match_types(a,b,flags);
      return ret;
    }else{
      return low_match_types(a,b,flags);
    }

  case PIKE_T_RING:
    return low_match_types(a, b+1, flags);

  case T_NOT:
    if(low_match_types(a,b+1, (flags ^ A_EXACT ) | NO_MAX_ARGS))
      return 0;
    return a;

    case T_ASSIGN:
      ret=low_match_types(a,b+2,flags);
      if(ret && EXTRACT_UCHAR(a)!=T_VOID)
      {
	int m=EXTRACT_UCHAR(b+1)-'0';
	struct pike_type *tmp;
	type_stack_mark();
	push_unfinished_type_with_markers(a, a_markers);
	tmp=pop_unfinished_type();

	type_stack_mark();
	medium_or_pike_types(b_markers[m], tmp, 0);
	if(b_markers[m]) free_type(b_markers[m]);
	free_type(tmp);
	b_markers[m]=pop_unfinished_type();
#ifdef PIKE_TYPE_DEBUG
	if (l_flag>2) {
	  char *s;
	  int e;
	  init_buf();
	  for(e=0;e<indent;e++) my_strcat("  ");
	  my_strcat("b_markers[");
	  my_putchar((char)(m+'0'));
	  my_strcat("]=");
	  low_describe_type(b_markers[m]->str);
	  my_strcat("\n");
	  fprintf(stderr,"%s",(s=simple_free_buf()));
	  free(s);
	}
#endif
#ifdef PIKE_DEBUG
	if(b_markers[m]->str[0] == m+'0')
	  fatal("Cyclic type!\n");
#endif
      }
      return ret;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    {
      int m=EXTRACT_UCHAR(b)-'0';
      if(b_markers[m])
      {
#ifdef PIKE_DEBUG
	if(b_markers[m]->str[0] == EXTRACT_UCHAR(b))
	  fatal("Cyclic type!\n");
#endif
	return low_match_types(a, b_markers[m]->str, flags);
      }
      else
	return low_match_types(a, mixed_type_string->str, flags);
    }
  }

  /* 'mixed' matches anything */

  if(((EXTRACT_UCHAR(a) == T_ZERO || EXTRACT_UCHAR(a) == T_MIXED) &&
      !(flags & (A_EXACT|B_EXACT)) &&
      EXTRACT_UCHAR(b) != T_VOID))
  {
#if 1
    switch(EXTRACT_UCHAR(b))
    {
      /* These types can contain sub-types */
      case T_ARRAY:
	low_match_types(array_type_string->str,b , flags);
	break;
      case T_MAPPING:
	low_match_types(mapping_type_string->str,b, flags);
	break;
      case T_FUNCTION:
	low_match_types(function_type_string->str,b, flags);
	break;
      case T_MULTISET:
	low_match_types(multiset_type_string->str,b, flags);
	break;
    }
#endif
    return a;
  }

  if((( EXTRACT_UCHAR(b) == T_ZERO || EXTRACT_UCHAR(b) == T_MIXED) &&
      !(flags & (A_EXACT|B_EXACT)) &&
      EXTRACT_UCHAR(a) != T_VOID))
  {
#if 1
    switch(EXTRACT_UCHAR(a))
    {
      /* These types can contain sub-types */
      case T_ARRAY:
	low_match_types(a , array_type_string->str, flags);
	break;
      case T_MAPPING:
	low_match_types(a , mapping_type_string->str, flags);
	break;
      case T_FUNCTION:
	low_match_types(a , function_type_string->str, flags);
	break;
      case T_MULTISET:
	low_match_types(a , multiset_type_string->str, flags);
	break;
    }
#endif
    return a;
  }

  /* Convert zero to int(0..0). */
  if (EXTRACT_UCHAR(a) == T_ZERO)
    a = tInt0;
  if (EXTRACT_UCHAR(b) == T_ZERO)
    b = tInt0;

  /* Special cases (tm) */
  switch(EXTRACT_TWOT(a,b))
  {
  case TWOT(T_PROGRAM, T_FUNCTION):
  case TWOT(T_FUNCTION, T_PROGRAM):
    /* FIXME: Should look at the program subtype. */
    return a;

  case TWOT(T_OBJECT, T_FUNCTION):
  {
    struct pike_type *s;
    if((s=low_object_lfun_type(a, LFUN_CALL)))
       return low_match_types(s->str,b,flags);
    if (flags & B_EXACT) {
      /* A function isn't an object */
      return 0;
    }
    return a;
  }

  case TWOT(T_FUNCTION, T_OBJECT):
  {
    struct pike_type *s;
    if((s=low_object_lfun_type(b, LFUN_CALL)))
       return low_match_types(a,s->str,flags);
    if (flags & A_EXACT) {
      /* A function isn't an object */
      return 0;
    }
    return a;
  }
  }

  if(EXTRACT_UCHAR(a) != EXTRACT_UCHAR(b)) return 0;

  ret=a;
  switch(EXTRACT_UCHAR(a))
  {
  case T_FUNCTION:
    correct_args=0;
    a++;
    b++;
    while(EXTRACT_UCHAR(a)!=T_MANY || EXTRACT_UCHAR(b)!=T_MANY)
    {
      char *a_tmp,*b_tmp;
      if(EXTRACT_UCHAR(a)==T_MANY)
      {
	a_tmp=a+1;
      }else{
	a_tmp=a;
	a+=type_length(a);
      }

      if(EXTRACT_UCHAR(b)==T_MANY)
      {
	b_tmp=b+1;
      }else{
	b_tmp=b;
	b+=type_length(b);
      }

      if(!low_match_types(a_tmp, b_tmp,
			  (flags | NO_MAX_ARGS) & ~(A_EXACT|B_EXACT)))
	return 0;
      if(++correct_args > max_correct_args)
	if(!(flags & NO_MAX_ARGS))
	  max_correct_args=correct_args;
    }
    /* check the 'many' type */
    a++;
    b++;
    if(EXTRACT_UCHAR(b)==T_VOID || EXTRACT_UCHAR(a)==T_VOID)
    {
      a+=type_length(a);
      b+=type_length(b);
    }else{
      if(!low_match_types(a,b, (flags | NO_MAX_ARGS) & ~(A_EXACT|B_EXACT)))
	return 0;
    }
    if(!(flags & NO_MAX_ARGS))
       max_correct_args=0x7fffffff;
    /* check the returntype */
    if ((EXTRACT_UCHAR(b) == T_VOID) && (EXTRACT_UCHAR(a) != T_VOID)) {
      /* Promote b to a function returning zero. */
      if (!low_match_types(a, tZero, flags & ~(A_EXACT|B_EXACT))) return 0;
    } else if ((EXTRACT_UCHAR(a) == T_VOID) && (EXTRACT_UCHAR(b) != T_VOID)) {
      /* Promote a to a function returning zero. */
      if(!low_match_types(tZero,b,flags & ~(A_EXACT|B_EXACT))) return 0;
    } else if(!low_match_types(a,b,flags & ~(A_EXACT|B_EXACT))) return 0;
    break;

  case T_MAPPING:
    if(!low_match_types(++a,++b,flags & ~(A_EXACT|B_EXACT))) return 0;
    if(!low_match_types(a+type_length(a),b+type_length(b),
			flags & ~(A_EXACT|B_EXACT))) return 0;
    break;

  case T_OBJECT:
#if 0
    if(extract_type_int(a+2) || extract_type_int(b+2))
    {
      fprintf(stderr,"Type match1: ");
      stupid_describe_type(a,type_length(a));
      fprintf(stderr,"\nType match2: ");
      stupid_describe_type(b,type_length(b));
      fputc('\n', stderr);
    }
#endif

    /* object(* 0) matches any object */
    if(!extract_type_int(a+2) || !extract_type_int(b+2)) break;

    /* object(x *) =? object(x *) */
    if(EXTRACT_UCHAR(a+1) == EXTRACT_UCHAR(b+1))
    {
      /* x? */
      if(EXTRACT_UCHAR(a+1))
      {
	/* object(1 x) =? object(1 x) */
	if(extract_type_int(a+2) != extract_type_int(b+2)) return 0;
      }else{
	/* object(0 *) =? object(0 *) */
	/* FIXME: Ought to check the implements relation */
	break;
      }
    }

    {
      struct program *ap,*bp;
      ap=id_to_program(extract_type_int(a+2));
      bp=id_to_program(extract_type_int(b+2));

      if(!ap || !bp) break;

#if 0
      /* FIXME: Temporary kludge.
       * match_types() currently seems to need to be symetric.
       */
      if (!implements(ap,bp) && !implements(bp,ap))
	return 0;
#else /* !1 */
      if(EXTRACT_UCHAR(a+1))
      {
	if(!implements(implements_a=ap,implements_b=bp))
	  return 0;
      }else{
	if(!implements(implements_a=bp,implements_b=ap))
	  return 0;
      }
#endif /* 1 */
    }
    
    break;

  case T_INT:
  {
    INT32 amin=extract_type_int(a+1);
    INT32 amax=extract_type_int(a+1+sizeof(INT32));

    INT32 bmin=extract_type_int(b+1);
    INT32 bmax=extract_type_int(b+1+sizeof(INT32));
    
    if(amin > bmax || bmin > amax) return 0;
    break;
  }
    

  case T_TYPE:
  case T_MULTISET:
  case T_ARRAY:
  case T_PROGRAM:
    if(!low_match_types(++a,++b,flags & ~(A_EXACT|B_EXACT))) return 0;

  case T_FLOAT:
  case T_STRING:
  case T_ZERO:
  case T_VOID:
  case T_MIXED:
    break;

  default:
    fatal("Error in type string.\n");
  }
  return ret;
}

/*
 * Check the partial ordering relation.
 *
 *                 mixed
 *
 * int float string program function object
 *
 *                 zero
 *
 *                 void
 *
 * Note that non-destructive operations are assumed.
 * ie it's assumed that calling a function(mapping(string|int:string|int):void)
 * with a mapping(int:int) won't change the type of the mapping after the
 * operation.
 */
static int low_pike_types_le(char *a, char *b,
			     int array_cnt, unsigned int flags)
#ifdef PIKE_TYPE_DEBUG
{
  int e;
  char *s;
  static int low_pike_types_le2(char *a, char *b,
				int array_cnt, unsigned int flags);
  int res;
  char buf[50];

  if (l_flag>2) {
    init_buf();
    for(e=0;e<indent;e++) my_strcat("  ");
    my_strcat("low_pike_types_le(");
    low_describe_type(a);
    if(type_length(a) + type_length(b) > 10)
    {
      my_strcat(",\n");
      for(e=0;e<indent;e++) my_strcat("  ");
      my_strcat("                ");
      low_describe_type(b);
    }else{
      my_strcat(", ");
      low_describe_type(b);
    }
    if(type_length(a) + type_length(b) > 10)
    {
      my_strcat(",\n");
      for(e=0;e<indent;e++) my_strcat("  ");
      my_strcat("                ");
    }else{
      my_strcat(", ");
    }
    sprintf(buf, "%d", array_cnt);
    my_strcat(buf);
    my_strcat(", ");
    sprintf(buf, "0x%08x", flags);
    my_strcat(buf);
    my_strcat(");\n");
    fprintf(stderr,"%s",(s=simple_free_buf()));
    free(s);
    indent++;
  }

  res=low_pike_types_le2(a, b, array_cnt, flags);

  if (l_flag>2) {
    indent--;

    for(e=0;e<indent;e++) fprintf(stderr, "  ");
    fprintf(stderr, "= %d\n", res);
  }
  return res;
}

static int low_pike_types_le2(char *a, char *b,
			      int array_cnt, unsigned int flags)
#endif /* PIKE_TYPE_DEBUG */

{
  int ret;
  if(a == b) return 1;

  switch(EXTRACT_UCHAR(a))
  {
  case T_AND:
    /* OK if either of the parts is a subset. */
    /* FIXME: What if b also contains an AND? */
    a++;
    ret = low_pike_types_le(a, b, array_cnt, flags);
    if(ret) return ret;
    a += type_length(a);
    return low_pike_types_le(a, b, array_cnt, flags);

  case T_OR:
    /* OK, if both of the parts are a subset */
    a++;
    if (EXTRACT_UCHAR(a) == T_VOID) {
      /* Special case for T_VOID */
      /* FIXME: Should probably be handled as T_ZERO. */
      a += type_length(a);
      return low_pike_types_le(a, b, array_cnt, flags);
    } else {
      ret=low_pike_types_le(a, b, array_cnt, flags);
      if (!ret) return 0;
      a+=type_length(a);
      if (EXTRACT_UCHAR(a) == T_VOID) {
	/* Special case for T_VOID */
	/* FIXME: Should probably be handled as T_ZERO. */
	return 1;
      } else {
	return low_pike_types_le(a, b, array_cnt, flags);
      }
    }

  case PIKE_T_RING:
    return low_pike_types_le(a+1, b, array_cnt, flags);

  case T_NOT:
    if (EXTRACT_UCHAR(b) == T_NOT) {
      return low_pike_types_le(b+1, a+1, -array_cnt, flags);
    }
    if (EXTRACT_UCHAR(a+1) == T_NOT) {
      return low_pike_types_le(a+2, b, array_cnt, flags);
    }
    if (low_pike_types_le(a+1, b, array_cnt, flags)) {
      return 0;
    }
    /* FIXME: This is wrong... */
    return !low_pike_types_le(b, a+1, -array_cnt, flags);

  case T_ASSIGN:
    ret=low_pike_types_le(a+2, b, array_cnt, flags);
    if(ret && EXTRACT_UCHAR(b)!=T_VOID)
    {
      int m=EXTRACT_UCHAR(a+1)-'0';
      struct pike_type *tmp;
      int i;
      type_stack_mark();
      push_unfinished_type_with_markers(b, b_markers);
      for(i=array_cnt; i > 0; i--)
	push_type(T_ARRAY);
      tmp=pop_unfinished_type();
      
      type_stack_mark();
      medium_or_pike_types(a_markers[m], tmp, 0);
      if(a_markers[m]) free_type(a_markers[m]);
      free_type(tmp);
      a_markers[m]=pop_unfinished_type();
#ifdef PIKE_TYPE_DEBUG
      if (l_flag>2) {
	char *s;
	int e;
	init_buf();
	for(e=0;e<indent;e++) my_strcat("  ");
	my_strcat("a_markers[");
	my_putchar((char)(m+'0'));
	my_strcat("]=");
	low_describe_type(a_markers[m]->str);
	my_strcat("\n");
	fprintf(stderr,"%s",(s=simple_free_buf()));
	free(s);
      }
#endif
    }
    return ret;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    {
      int m=EXTRACT_UCHAR(a)-'0';
      if(a_markers[m])
	return low_pike_types_le(a_markers[m]->str, b, array_cnt, flags);
      else
	return low_pike_types_le(mixed_type_string->str, b, array_cnt, flags);
    }
  }

  switch(EXTRACT_UCHAR(b))
  {
  case T_AND:
    /* OK, if a is a subset of both parts. */
    b++;
    ret = low_pike_types_le(a, b, array_cnt, flags);
    if(!ret) return 0;
    b+=type_length(b);
    return low_pike_types_le(a, b, array_cnt, flags);

  case T_OR:
    /* OK if a is a subset of either of the parts. */
    b++;
    ret=low_pike_types_le(a, b, array_cnt, flags);
    if (ret) return ret;
    b+=type_length(b);
    return low_pike_types_le(a, b, array_cnt, flags);

  case PIKE_T_RING:
    return low_pike_types_le(a, b+1, array_cnt, flags);

  case T_NOT:
    if (EXTRACT_UCHAR(b+1) == T_NOT) {
      return low_pike_types_le(a, b+2, array_cnt, flags);
    }
    if (low_pike_types_le(a, b+1, array_cnt, flags)) {
      return 0;
    }
    /* FIXME: This is wrong... */
    return !low_pike_types_le(b+1, a, -array_cnt, flags);

  case T_ASSIGN:
    ret=low_pike_types_le(a, b+2, array_cnt, flags);
    if(ret && EXTRACT_UCHAR(a)!=T_VOID)
    {
      int m=EXTRACT_UCHAR(b+1)-'0';
      struct pike_type *tmp;
      int i;
      type_stack_mark();
      push_unfinished_type_with_markers(a, a_markers);
      for(i = array_cnt; i < 0; i++)
	push_type(T_ARRAY);
      tmp=pop_unfinished_type();
      
      type_stack_mark();
      medium_or_pike_types(b_markers[m], tmp, 0);
      if(b_markers[m]) free_type(b_markers[m]);
      free_type(tmp);
      b_markers[m]=pop_unfinished_type();
#ifdef PIKE_TYPE_DEBUG
      if (l_flag>2) {
	char *s;
	int e;
	init_buf();
	for(e=0;e<indent;e++) my_strcat("  ");
	my_strcat("b_markers[");
	my_putchar((char)(m+'0'));
	my_strcat("]=");
	low_describe_type(b_markers[m]->str);
	my_strcat("\n");
	fprintf(stderr,"%s",(s=simple_free_buf()));
	free(s);
      }
#endif
    }
    return ret;

  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    {
      int m=EXTRACT_UCHAR(b)-'0';
      if(b_markers[m])
	return low_pike_types_le(a, b_markers[m]->str, array_cnt, flags);
      else
	return low_pike_types_le(a, mixed_type_string->str, array_cnt, flags);
    }
  }

  if ((array_cnt < 0) && (EXTRACT_UCHAR(b) == T_ARRAY)) {
    while (EXTRACT_UCHAR(b) == T_ARRAY) {
      b++;
      if (!++array_cnt) break;
    }
    return low_pike_types_le(a, b, array_cnt, flags);
  } else if ((array_cnt > 0) && (EXTRACT_UCHAR(a) == T_ARRAY)) {
    while (EXTRACT_UCHAR(a) == T_ARRAY) {
      a++;
      if (!--array_cnt) break;
    }
    return low_pike_types_le(a, b, array_cnt, flags);
  }

  /* NOTE: void only matches void. */
  if (EXTRACT_UCHAR(a) == T_VOID) {
    /* void <= any_type */
    if (array_cnt >= 0) {
      /* !array(void) */
      if (!array_cnt && (EXTRACT_UCHAR(b) == T_VOID)) {
	return 1;
      }
      return 0;
    }
  }

  if (EXTRACT_UCHAR(b) == T_VOID) {
    if (array_cnt <= 0) {
      /* !array(void) */
      return 0;
    }
  }

  if (EXTRACT_UCHAR(b) == T_MIXED) {
    /* any_type <= 'mixed' */
    if (array_cnt <= 0) {
      /* !array(mixed) */
      return 1;
    }
  }

  if (EXTRACT_UCHAR(a) == T_MIXED) {
    if (array_cnt >= 0) {
      /* !array(mixed) */
      return 0;
    }
  }

  if (EXTRACT_UCHAR(a) == T_ZERO) {
    /* void <= zero <= any_type */
    if (array_cnt >= 0) {
      /* !array(zero) */
      return 1;
    }
  }

  if (EXTRACT_UCHAR(b) == T_ZERO) {
    if (array_cnt <= 0) {
      /* !array(zero) */
      return 0;
    }
  }

  /* Special cases (tm) */
  switch(EXTRACT_TWOT(a,b))
  {
  case TWOT(T_PROGRAM, T_FUNCTION):
  case TWOT(T_FUNCTION, T_PROGRAM):
    /* FIXME: Not really... Should check the return value. */
    /* FIXME: Should look at the program subtype. */
    return 1;

  case TWOT(T_OBJECT, T_FUNCTION):
    {
      struct pike_type *s;
      if((s=low_object_lfun_type(a, LFUN_CALL)))
	return low_pike_types_le(s->str, b, array_cnt, flags);
      return 1;
    }

  case TWOT(T_FUNCTION, T_OBJECT):
    {
      struct pike_type *s;
      if((s=low_object_lfun_type(b, LFUN_CALL)))
	return low_pike_types_le(a, s->str, array_cnt, flags);
      return 1;
    }

  case TWOT(T_FUNCTION, T_ARRAY):
    {
      while (EXTRACT_UCHAR(b) == T_ARRAY) {
	b++;
	array_cnt++;
      }
      return low_pike_types_le(a, b, array_cnt, flags);
    }

  case TWOT(T_ARRAY, T_FUNCTION):
    {
      while (EXTRACT_UCHAR(a) == T_ARRAY) {
	a++;
	array_cnt--;
      }
      return low_pike_types_le(a, b, array_cnt, flags);
    }
  }

  if(EXTRACT_UCHAR(a) != EXTRACT_UCHAR(b)) return 0;

  if (EXTRACT_UCHAR(a) == T_FUNCTION) {
    /*
     * function(A...:B) <= function(C...:D)	iff C <= A && B <= D
     */
    /*
     * function(:int) <= function(int:int)
     * function(int|string:int) <= function(int:int)
     * function(:int) <= function(:void)
     * function(string:int) != function(int:int)
     * function(int:int) != function(:int)
     *
     * FIXME: Enforcing of all required arguments?
     */
    a++;
    b++;
    while(EXTRACT_UCHAR(a)!=T_MANY || EXTRACT_UCHAR(b)!=T_MANY)
    {
      char *a_tmp,*b_tmp;
      if(EXTRACT_UCHAR(a)==T_MANY)
      {
	a_tmp=a+1;
      }else{
	a_tmp=a;
	a+=type_length(a);
      }

      if(EXTRACT_UCHAR(b)==T_MANY)
      {
	b_tmp=b+1;
      }else{
	b_tmp=b;
	b+=type_length(b);
      }

      if (EXTRACT_UCHAR(a_tmp) != T_VOID) {
	if (!low_pike_types_le(b_tmp, a_tmp, 0, flags)) {
	  return 0;
	}
      }
    }
    /* check the 'many' type */
    a++;
    b++;
    if ((EXTRACT_UCHAR(a) != T_VOID) && (EXTRACT_UCHAR(b) != T_VOID)) {
      if (!low_pike_types_le(b, a, 0, flags)) {
	return 0;
      }
    }

    a+=type_length(a);
    b+=type_length(b);

    /* check the returntype */
    /* FIXME: Check array_cnt */
    if ((EXTRACT_UCHAR(b) != T_VOID) && (EXTRACT_UCHAR(a) != T_VOID)) {
      if(!low_pike_types_le(a, b, array_cnt, flags)) return 0;
    }
    return 1;
  }

  if (array_cnt) return 0;

  switch(EXTRACT_UCHAR(a))
  {
  case T_MAPPING:
    /*
     *  mapping(A:B) <= mapping(C:D)   iff A <= C && B <= D.
     */
    if(!low_pike_types_le(++a, ++b, 0, flags)) return 0;
    return low_pike_types_le(a+type_length(a), b+type_length(b), 0, flags);

  case T_OBJECT:
#if 0
    if(extract_type_int(a+2) || extract_type_int(b+2))
    {
      fprintf(stderr,"Type match1: ");
      stupid_describe_type(a,type_length(a));
      fprintf(stderr,"\nType match2: ");
      stupid_describe_type(b,type_length(b));
      fputc('\n', stderr);
    }
#endif

    /*
     * object(0|1 x) <= object(0|1 0)
     * object(0|1 0) <=! object(0|1 !0)
     * object(1 x) <= object(0|1 x)
     * object(1 x) <= object(1 y)	iff x == y
     * object(1 x) <= object(0 y)	iff x implements y
     * Not WEAK_OBJECTS:
     *   object(0 x) <= object(0 y)	iff x implements y
     * WEAK_OBJECTS:
     *   object(0 x) <= object(0 y)	iff x is_compatible y
     */

    /* object(* 0) matches any object */
    if(!extract_type_int(b+2))
      return 1;

    if(!extract_type_int(a+2))
      return 0;
    
    if ((EXTRACT_UCHAR(a+1) || !EXTRACT_UCHAR(b+1)) &&
	(extract_type_int(a+2) == extract_type_int(b+2)))
      return 1;

    if (EXTRACT_UCHAR(b+1)) {
      return 0;
    }

    {
      struct program *ap = id_to_program(extract_type_int(a+2));
      struct program *bp = id_to_program(extract_type_int(b+2));

      if (!ap || !bp) {
	/* Shouldn't happen... */
	return 0;
      }
      if ((flags & LE_WEAK_OBJECTS) && (!EXTRACT_UCHAR(a+1))) {
	return is_compatible(implements_a=ap, implements_b=bp);
      }
      return implements(implements_a=ap, implements_b=bp);
    }
    break;

  case T_INT:
  {
    INT32 amin=extract_type_int(a+1);
    INT32 amax=extract_type_int(a+1+sizeof(INT32));

    INT32 bmin=extract_type_int(b+1);
    INT32 bmax=extract_type_int(b+1+sizeof(INT32));
    
    if(amin < bmin || amax > bmax) return 0;
    break;
  }
    

  case T_TYPE:
  case T_MULTISET:
  case T_ARRAY:
  case T_PROGRAM:
    if(!low_pike_types_le(++a, ++b, 0, flags)) return 0;

  case T_FLOAT:
  case T_STRING:
  case T_ZERO:
  case T_VOID:
  case T_MIXED:
    break;

  default:
    fatal("Error in type string.\n");
  }
  return 1;
}

/*
 * Check the function parameters.
 * Note: The difference between this function, and pike_types_le()
 *       is the more lenient check for T_OR, and the handling of T_ARRAY.
 */
static int low_strict_check_call(char *fun_type, char *arg_type)
{
  while ((EXTRACT_UCHAR(fun_type) == T_OR) ||
	 (EXTRACT_UCHAR(fun_type) == T_ARRAY)) {
    if (EXTRACT_UCHAR(fun_type++) == T_OR) {
      int res = low_strict_check_call(fun_type, arg_type);
      if (res) return res;
      fun_type += type_length(fun_type);
    }
  }
  return low_pike_types_le(fun_type, arg_type, 0, 0);
}

int strict_check_call(struct pike_type *fun_type, struct pike_type *arg_type)
{
  return low_strict_check_call(fun_type->str, arg_type->str);
}

/*
 * Check validity of soft-cast.
 * Note: This uses a weaker check of function arguments, since
 *       people get confused otherwise.
 */
int check_soft_cast(struct pike_type *to, struct pike_type *from)
{
  return low_pike_types_le(to->str, from->str, 0, LE_WEAK_OBJECTS);
}

/*
 * Return the return type from a function call.
 */
static int low_get_return_type(char *a,char *b)
{
  int tmp;
  switch(EXTRACT_UCHAR(a))
  {
  case T_OR:
    {
      struct pike_type *o1, *o2;
      a++;
      o1=o2=0;

      type_stack_mark();
      if(low_get_return_type(a,b)) 
      {
	o1=pop_unfinished_type();
	type_stack_mark();
      }

      if(low_get_return_type(a+type_length(a),b))
	o2=pop_unfinished_type();
      else
	pop_stack_mark();

      if(!o1 && !o2) return 0;

      medium_or_pike_types(o1,o2, 0);

      if(o1) free_type(o1);
      if(o2) free_type(o2);

      return 1;
    }

  case T_AND:
    a++;
    type_stack_mark();
    tmp=low_get_return_type(a,b);
    type_stack_pop_to_mark();
    if(!tmp) return 0;
    return low_get_return_type(a+type_length(a),b);

  case T_ARRAY:
    a++;
    tmp=low_get_return_type(a,b);
    if(!tmp) return 0;
    push_type(T_ARRAY);
    return 1;
  }

  a=low_match_types(a,b,NO_SHORTCUTS);
  if(a)
  {
#if 0
    if ((lex.pragmas & ID_STRICT_TYPES) &&
	!low_pike_types_le(a, b, 0, 0)) {
      yywarning("Type mismatch");
    }
#endif /* 0 */
    switch(EXTRACT_UCHAR(a))
    {
    case T_FUNCTION:
      a++;
      while(EXTRACT_UCHAR(a)!=T_MANY) a+=type_length(a);
      a++;
      a+=type_length(a);
      push_unfinished_type_with_markers(a, a_markers );
      return 1;

    case T_PROGRAM:
      push_unfinished_type(a+1);
      return 1;

    default:
      push_type(T_MIXED);
      return 1;
    }
  }
  return 0;
}


int match_types(struct pike_type *a, struct pike_type *b)
{
  check_type_string(a);
  check_type_string(b);
  clear_markers();
  return 0!=low_match_types(a->str, b->str,0);
}

int pike_types_le(struct pike_type *a, struct pike_type *b)
{
  check_type_string(a);
  check_type_string(b);
  clear_markers();
  return low_pike_types_le(a->str, b->str, 0, 0);
}


#ifdef DEBUG_MALLOC
#define low_index_type(X,Y,Z) ((struct pike_type *)debug_malloc_pass(debug_low_index_type((X),(Y),(Z))))
#else
#define low_index_type debug_low_index_type
#endif

/* FIXME, add the index */
static struct pike_type *debug_low_index_type(char *t,
					      char *index_type,
					      node *n)
{
  struct pike_type *tmp;
  struct program *p;

  switch(low_check_indexing(t, index_type, n))
  {
    case 0: return 0;
    case -1:
      reference_shared_string(zero_type_string);
      return zero_type_string;
  }

  switch(EXTRACT_UCHAR(t++))
  {
  case T_OBJECT:
  {
    p=id_to_program(extract_type_int(t+1));

  comefrom_int_index:
    if(p && n)
    {
      INT32 i;
      if(n->token == F_ARROW)
      {
	/* FIXME: make this stricter */
	if((i=FIND_LFUN(p,LFUN_ARROW))!=-1)
	{
	  /* FIXME: function_type_string should be replaced with something
	   * derived from type_string
	   */
	  if(i!=-1 &&
	     (tmp=check_call(function_type_string, ID_FROM_INT(p, i)->type,
			     0)))
	    return tmp;

	  reference_shared_string(mixed_type_string);
	  return mixed_type_string;
	}
      }else{
	if((i=FIND_LFUN(p,LFUN_INDEX)) != -1)
	{
	  /* FIXME: function_type_string should be replaced with something
	   * derived from type_string
	   */
	  if(i!=-1 &&
	     (tmp=check_call(function_type_string, ID_FROM_INT(p, i)->type,
			     0)))
	    return tmp;

	  reference_shared_string(mixed_type_string);
	  return mixed_type_string;
	}
      }
      if(CDR(n)->token == F_CONSTANT && CDR(n)->u.sval.type==T_STRING)
      {
	i=find_shared_string_identifier(CDR(n)->u.sval.u.string, p);
	if(i==-1)
	{
	  reference_shared_string(mixed_type_string);
	  return mixed_type_string;
	}else{
#if 0
	  if(EXTRACT_UCHAR(t) ||
	     (p->identifier_references[i].id_flags & ID_NOMASK) ||
	    (ID_FROM_INT(p, i)->identifier_flags & IDENTIFIER_PROTOTYPED))
	  {
#endif
	    reference_shared_string(ID_FROM_INT(p, i)->type);
	    return ID_FROM_INT(p, i)->type;
#if 0
	  }else{
	    reference_shared_string(mixed_type_string);
	    return mixed_type_string;
	  }
#endif
	}	   
      }
    }
  }
  default:
    reference_shared_string(mixed_type_string);
    return mixed_type_string;

  case T_MIXED:
    if (lex.pragmas & ID_STRICT_TYPES) {
      yywarning("Indexing mixed.");
    }
    reference_shared_string(mixed_type_string);
    return mixed_type_string;    

    case T_INT:
#ifdef AUTO_BIGNUM
      /* Don't force Gmp.mpz to be loaded here since this function
       * is called long before the master object is compiled...
       * /Hubbe
       */
      p=get_auto_bignum_program_or_zero();
      goto comefrom_int_index;
#endif
    case T_ZERO:
    case T_TYPE:
    case T_VOID:
    case T_FLOAT:
    case PIKE_T_RING:
      return 0;

  case T_OR:
  {
    struct pike_type *a, *b;
    a=low_index_type(t,index_type,n);
    t+=type_length(t);
    b=low_index_type(t,index_type,n);
    if(!b) return a;
    if(!a) return b;
    type_stack_mark();
    medium_or_pike_types(a,b,1);
    free_type(a);
    free_type(b);
    return pop_unfinished_type();
  }

  case T_AND:
    return low_index_type(t+type_length(t),index_type,n);

  case T_STRING: /* always int */
  case T_MULTISET: /* always int */
    reference_shared_string(int_type_string);
    return int_type_string;

  case T_MAPPING:
    t+=type_length(t);
    return make_shared_binary_string(t, type_length(t));

  case T_ARRAY:
    {
      struct pike_type *a;

      if(low_pike_types_le(tString, index_type, 0, 0) &&
	 (a = low_index_type(t, tString, n))) {
	/* Possible to index the array with a string. */
	type_stack_mark();
	push_finished_type(a);
	free_type(a);
	push_type(T_ARRAY);

	if (low_match_types(tInt, index_type, 0)) {
	  /* Also possible to index the array with an int. */
	  push_unfinished_type(t);
	  push_type(T_OR);
	}
	return pop_unfinished_type();
      }
      if (low_match_types(tInt, index_type, 0)) {
	/* Possible to index the array with an int. */
	return make_shared_binary_string(t, type_length(t));
      }
      /* Bad index type. */
      return 0;
    }
  }
}

struct pike_type *index_type(struct pike_type *type,
			     struct pike_type *index_type,
			     node *n)
{
  struct pike_type *t;
  clear_markers();
  t=low_index_type(type->str,index_type->str,n);
  if(!t) copy_shared_string(t,mixed_type_string);
  return t;
}


static struct pike_type *low_array_value_type(char *arr_t)
{
  struct pike_type *res = NULL;
  struct pike_type *sub_t;

  while (EXTRACT_UCHAR(arr_t) == T_OR) {
    arr_t++;
    sub_t = low_array_value_type(arr_t);
    arr_t += type_length(arr_t);
    if (sub_t) {
      if (res) {
	struct pike_type *new = or_pike_types(res, sub_t, 1);
	free_type(res);
	free_type(sub_t);
	res = new;
      } else {
	res = sub_t;
      }
    }
  }
  if (EXTRACT_UCHAR(arr_t) != T_ARRAY)
    return res;
  arr_t++;
  sub_t = make_shared_binary_string(arr_t, type_length(arr_t));
  if (res) {
    struct pike_type *new = or_pike_types(res, sub_t, 1);
    free_type(res);
    free_type(sub_t);
    return new;
  }
  return sub_t;
}

struct pike_type *array_value_type(struct pike_type *array_type)
{
  struct pike_type *t = low_array_value_type(array_type->str);
  if (!t) copy_shared_string(t, mixed_type_string);
  return t;
}


#ifdef DEBUG_MALLOC
#define low_key_type(X,Y) ((struct pike_type *)debug_malloc_pass(debug_low_key_type((X),(Y))))
#else
#define low_key_type debug_low_key_type
#endif

/* FIXME, add the index */
static struct pike_type *debug_low_key_type(char *t, node *n)
{
  switch(EXTRACT_UCHAR(t++))
  {
  case T_OBJECT:
  {
    struct program *p=id_to_program(extract_type_int(t+1));
    if(p && n)
    {
      if(n->token == F_ARROW)
      {
	if(FIND_LFUN(p,LFUN_ARROW)!=-1 || FIND_LFUN(p,LFUN_ASSIGN_ARROW)!=-1)
	{
	  reference_shared_string(mixed_type_string);
	  return mixed_type_string;
	}
      }else{
	if(FIND_LFUN(p,LFUN_INDEX) != -1 || FIND_LFUN(p,LFUN_ASSIGN_INDEX) != -1)
	{
	  reference_shared_string(mixed_type_string);
	  return mixed_type_string;
	}
      }
    }
    reference_shared_string(string_type_string);
    return string_type_string;
  }
  default:
    reference_shared_string(mixed_type_string);
    return mixed_type_string;

    case T_VOID:
    case T_ZERO:
    case T_TYPE:
    case T_FLOAT:
    case T_INT:
    case PIKE_T_RING:
      return 0;

  case T_OR:
  {
    struct pike_type *a, *b;
    a=low_key_type(t,n);
    t+=type_length(t);
    b=low_key_type(t,n);
    if(!b) return a;
    if(!a) return b;
    type_stack_mark();
    medium_or_pike_types(a,b,1);
    free_type(a);
    free_type(b);
    return pop_unfinished_type();
  }

  case T_AND:
    return low_key_type(t+type_length(t),n);

  case T_ARRAY:
  case T_STRING: /* always int */
    reference_shared_string(int_type_string);
    return int_type_string;

  case T_MAPPING:
  case T_MULTISET:
    return make_shared_binary_string(t, type_length(t));
  }
}

struct pike_type *key_type(struct pike_type *type, node *n)
{
  struct pike_type *t;
  clear_markers();
  t=low_key_type(type->str,n);
  if(!t) copy_shared_string(t,mixed_type_string);
  return t;
}



static int low_check_indexing(char *type, char *index_type, node *n)
{
  switch(EXTRACT_UCHAR(type++))
  {
  case T_OR:
    return low_check_indexing(type,index_type,n) ||
      low_check_indexing(type+type_length(type),index_type,n);

  case T_AND:
    return low_check_indexing(type,index_type,n) &&
      low_check_indexing(type+type_length(type),index_type,n);

  case T_NOT:
    return low_check_indexing(type,index_type,n)!=1;

  case T_ARRAY:
    if(low_match_types(string_type_string->str, index_type,0) &&
       low_check_indexing(type, index_type,n))
      return 1;

  case T_STRING:
    return !!low_match_types(int_type_string->str, index_type,0);

  case T_OBJECT:
  {
    struct program *p=id_to_program(extract_type_int(type+1));
    if(p)
    {
      if(n->token == F_ARROW)
      {
	if(FIND_LFUN(p,LFUN_ARROW)!=-1 || FIND_LFUN(p,LFUN_ASSIGN_ARROW)!=-1)
	  return 1;
      }else{
	if(FIND_LFUN(p,LFUN_INDEX)!=-1 || FIND_LFUN(p,LFUN_ASSIGN_INDEX)!=-1)
	  return 1;
      }
      return !!low_match_types(string_type_string->str, index_type,0);
    }else{
      return 1;
    }
  }

  case T_MULTISET:
  case T_MAPPING:
    return low_match_types(type,index_type,0) ? 1 : -1;

#ifdef AUTO_BIGNUM
    case T_INT:
#endif
    case T_PROGRAM:
      return !!low_match_types(string_type_string->str, index_type,0);

  case T_MIXED:
    return 1;
    
  default:
    return 0;
  }
}
				 
int check_indexing(struct pike_type *type,
		   struct pike_type *index_type,
		   node *n)
{
  check_type_string(type);
  check_type_string(index_type);

  return low_check_indexing(type->str, index_type->str, n);
}

static int low_count_arguments(char *q)
{
  int num,num2;

  switch(EXTRACT_UCHAR(q++))
  {
    case T_OR:
      num=low_count_arguments(q);
      num2=low_count_arguments(q+type_length(q));
      if(num<0 && num2>0) return num;
      if(num2<0 && num>0) return num2;
      if(num2<0 && num<0) return ~num>~num2?num:num2;
      return num>num2?num:num2;

    case T_AND:
      num=low_count_arguments(q);
      num2=low_count_arguments(q+type_length(q));
      if(num<0 && num2>0) return num2;
      if(num2<0 && num>0) return num;
      if(num2<0 && num<0) return ~num<~num2?num:num2;
      return num<num2?num:num2;

    default: return 0x7fffffff;

    case T_FUNCTION:
      num=0;
      while(EXTRACT_UCHAR(q)!=T_MANY)
      {
	num++;
	q+=type_length(q);
      }
      q++;
      if(EXTRACT_UCHAR(q)!=T_VOID) return ~num;
      return num;
  }
}

/* Count the number of arguments for a funciton type.
 * return -1-n if the function can take number of arguments
 * >= n  (varargs)
 */
int count_arguments(struct pike_type *s)
{
  check_type_string(s);

  return low_count_arguments(s->str);
}


static int low_minimum_arguments(char *q)
{
  int num;

  switch(EXTRACT_UCHAR(q++))
  {
    case T_OR:
    case T_AND:
      return MAXIMUM(low_count_arguments(q),
		     low_count_arguments(q+type_length(q)));

    default: return 0;

    case T_FUNCTION:
      num=0;
      while(EXTRACT_UCHAR(q)!=T_MANY)
      {
	if(low_match_types(void_type_string->str, q, B_EXACT))
	  return num;

	num++;
	q+=type_length(q);
      }
      return num;
  }
}

/* Count the minimum number of arguments for a funciton type.
 */
int minimum_arguments(struct pike_type *s)
{
  int ret;
  check_type_string(s);

  ret=low_minimum_arguments(s->str);

#if 0
  fprintf(stderr,"minimum_arguments(");
  simple_describe_type(s);
  fprintf(stderr," ) -> %d\n",ret);
#endif

  return ret;
}

struct pike_type *check_call(struct pike_type *args,
			     struct pike_type *type,
			     int strict)
{
  check_type_string(args);
  check_type_string(type);
  clear_markers();
  type_stack_mark();
  max_correct_args=0;
  
  if(low_get_return_type(type->str,args->str))
  {
    if (strict) {
      if (!strict_check_call(type, args)) {
	struct pike_string *type_t = describe_type(type);

	if (!low_pike_types_le(type->str, tFuncV(tNone,tZero,tMix), 0, 0)) {
	  yywarning("Calling non-function value.");
	  yywarning("Type called: %s", type_t->str);
	} else {
	  struct pike_string *arg_t = describe_type(args);
	  yywarning("Arguments not strictly compatible.");
	  yywarning("Expected: %s", type_t->str);
	  yywarning("Got     : %s", arg_t->str);
	  free_string(arg_t);
	}

	free_string(type_t);
      }
    }
    return pop_unfinished_type();
  }else{
    pop_stack_mark();
    return 0;
  }
}

INT32 get_max_args(struct pike_type *type)
{
  INT32 ret,tmp=max_correct_args;
  check_type_string(type);
  clear_markers();
  type = check_call(function_type_string, type, 0);
  if(type) free_type(type);
  ret=max_correct_args;
  max_correct_args=tmp;
  return tmp;
}


static struct pike_type *low_zzap_function_return(char *a, INT32 id)
{
  switch(EXTRACT_UCHAR(a))
  {
    case T_OR:
    {
      struct pike_type *ar, *br, *ret=0;
      a++;
      ar = low_zzap_function_return(a,id);
      br = low_zzap_function_return(a+type_length(a),id);
      if(ar && br) ret=or_pike_types(ar,br,0);
      if(ar) free_type(ar);
      if(br) free_type(br);
      return ret;
    }
      
    case T_FUNCTION:
      type_stack_mark();
      push_type_int(id);
      push_type(1);
      push_type(T_OBJECT);
      
      type_stack_mark();
      a++;
      while(EXTRACT_UCHAR(a)!=T_MANY)
      {
	type_stack_mark();
	push_unfinished_type(a);
	type_stack_reverse();
	a+=type_length(a);
      }
      a++;
      push_type(T_MANY);
      type_stack_mark();
      push_unfinished_type(a);
      type_stack_reverse();
      type_stack_reverse();
      push_type(T_FUNCTION);
      return pop_unfinished_type();

    case T_ARRAY:
      return low_zzap_function_return(a+1,id);

    case T_MIXED:
      /* I wonder when this occurrs, but apparently it does... */
      return low_zzap_function_return(tFuncV(tVoid,tOr(tMix,tVoid),tObj), id);
  }
/* This error is bogus /Hubbe
  fatal("low_zzap_function_return() called with unexpected value: %d\n",
	EXTRACT_UCHAR(a));
*/
  return NULL;
}

struct pike_type *zzap_function_return(struct pike_type *t, INT32 id)
{
  return low_zzap_function_return(t->str, id);
}

struct pike_type *get_type_of_svalue(struct svalue *s)
{
  struct pike_type *ret;
  switch(s->type)
  {
  case T_FUNCTION:
    if(s->subtype == FUNCTION_BUILTIN)
    {
      copy_pike_type(ret, s->u.efun->type);
    }else{
      struct program *p;

      p=s->u.object->prog;
      if(!p)
      {
	copy_pike_type(ret, zero_type_string);
      }else{
	copy_pike_type(ret, ID_FROM_INT(p,s->subtype)->type);
      }
    }
    return ret;
       
  case T_MULTISET:
  case T_ARRAY:
    {
      struct pike_type *arg_type;
      struct array *a;

      if (s->type == T_MULTISET) {
	a = s->u.multiset->ind;
      } else {
	a = s->u.array;
      }
#if 0
      int i;

      /* FIXME: Circular structures? */
      copy_shared_string(arg_type, zero_type_string);
      for (i = 0; i < a->size; i++) {
	struct pike_type *tmp1 = get_type_of_svalue(a->item+i);
	struct pike_type *tmp2 = or_pike_types(arg_type, tmp1, 1);
	free_type(arg_type);
	free_type(tmp1);
	arg_type = tmp2;
      }
#else /* !0 */
      if (a->size)
	copy_pike_type(arg_type, mixed_type_string);
      else
	copy_pike_type(arg_type, zero_type_string);
#endif /* 0 */
      type_stack_mark();
      push_finished_type(arg_type);
      free_type(arg_type);
      push_type(s->type);
      return pop_unfinished_type();
    }


  case T_MAPPING:
    type_stack_mark();
    if (m_sizeof(s->u.mapping)) {
      push_type(T_MIXED);
      push_type(T_MIXED);
    }
    else {
      push_type(T_ZERO);
      push_type(T_ZERO);
    }
    push_type(T_MAPPING);
    return pop_unfinished_type();

  case T_OBJECT:
    type_stack_mark();
    if(s->u.object->prog)
    {
#ifdef AUTO_BIGNUM
      if(is_bignum_object(s->u.object))
      {
	push_int_type(MIN_INT32, MAX_INT32);
      }
      else
#endif
      {
	push_type_int(s->u.object->prog->id);
	push_type(1);
	push_type(T_OBJECT);
      }
    }else{
      /* Destructed object */
      push_type(T_ZERO);
    }
    return pop_unfinished_type();

  case T_INT:
    if(s->u.integer)
    {
      type_stack_mark();
      /* Fixme, check that the integer is in range of MIN_INT32 .. MAX_INT32!
       */
      push_int_type(s->u.integer, s->u.integer);
      return pop_unfinished_type();
    }else{
      copy_pike_type(ret, zero_type_string);
    }
    return ret;

  case T_PROGRAM:
  {
    /* FIXME: An alternative is to push PROGRAM(OBJECT(1, p->id)) */
    struct pike_type *t;
    int id;

    if(s->u.program->identifiers)
    {
      id=FIND_LFUN(s->u.program,LFUN_CREATE);
      if(id>=0)
      {
	struct pike_string *tmp;
	t = ID_FROM_INT(s->u.program, id)->type;
	if((t=zzap_function_return(t, s->u.program->id)))
	  return t;
	tmp = describe_type(ID_FROM_INT(s->u.program, id)->type);
	/* yywarning("Failed to zzap function return for type: %s.", tmp->str);*/
	free_string(tmp);
      }
    } else {
      t = function_type_string;
      if((t = zzap_function_return(t, s->u.program->id)))
	return t;
    }

    type_stack_mark();
    push_object_type(1, s->u.program->id);
    push_type(T_VOID);
    push_type(T_MANY);
    push_type(T_FUNCTION);

    return pop_unfinished_type();
  }

  case T_TYPE:
    type_stack_mark();
    push_finished_type(s->u.type);
    push_type(T_TYPE);
    return pop_unfinished_type();

  default:
    type_stack_mark();
    push_type(s->type);
    return pop_unfinished_type();
  }
}

static struct pike_type *low_object_type_to_program_type(char *obj_t)
{
  struct pike_type *res = NULL;
  struct pike_type *sub;
  struct svalue sval;
  int id;

  while(EXTRACT_UCHAR(obj_t) == T_OR) {
    obj_t++;
    sub = low_object_type_to_program_type(obj_t);
    if (!sub) {
      if (res) {
	free_type(res);
      }
      return NULL;
    }
    if (res) {
      struct pike_type *tmp = or_pike_types(res, sub, 1);
      free_type(res);
      free_type(sub);
      res = tmp;
    } else {
      res = sub;
    }
    obj_t += type_length(obj_t);
  }
  sval.type = T_PROGRAM;
  if ((EXTRACT_UCHAR(obj_t) != T_OBJECT) ||
      (!(id = extract_type_int(obj_t + 2))) ||
      (!(sval.u.program = id_to_program(id))) ||
      (!(sub = get_type_of_svalue(&sval)))) {
    if (res) {
      free_type(res);
    }
    return NULL;
  }
  /* FIXME: obj_t + 1 should propagate to the return-type in sub. */
  if (res) {
    struct pike_type *tmp = or_pike_types(res, sub, 1);
    free_type(res);
    free_type(sub);
    return tmp;
  }
  return sub;
}

/* Used by fix_object_program_type() */
struct pike_type *object_type_to_program_type(struct pike_type *obj_t)
{
  return low_object_type_to_program_type(obj_t->str);
}

static int low_type_may_overload(char *type, int lfun)
{
  switch(EXTRACT_UCHAR(type++))
  {
    case T_ASSIGN:
      return low_type_may_overload(type+1, lfun);
      
    case T_FUNCTION:
    case T_ARRAY:
      /* might want to check for `() */
      
    default:
      return 0;

    case T_OR:
      return low_type_may_overload(type, lfun) ||
	low_type_may_overload(type+type_length(type), lfun);
      
    case T_AND:
      return low_type_may_overload(type, lfun) &&
	low_type_may_overload(type+type_length(type), lfun);
      
    case T_NOT:
      return !low_type_may_overload(type, lfun);

    case T_MIXED:
      return 1;
      
    case T_OBJECT:
    {
      struct program *p=id_to_program(extract_type_int(type+1));
      if(!p) return 1;
      return FIND_LFUN(p, lfun)!=-1;
    }
  }
}

int type_may_overload(struct pike_type *type, int lfun)
{
  return low_type_may_overload(type->str, lfun);
}

void yyexplain_nonmatching_types(struct pike_type *type_a,
				 struct pike_type *type_b,
				 int flags)
{
  implements_a=0;
  implements_b=0;

  match_types(type_a,type_b);

#if 0
  if(!(implements_a && implements_b &&
       type_a->str[0]==T_OBJECT &&
       type_b->str[0]==T_OBJECT))
#endif /* 0 */
  {
    struct pike_string *s1,*s2;
    s1=describe_type(type_a);
    s2=describe_type(type_b);
    if(flags & YYTE_IS_WARNING)
    {
      yywarning("Expected: %s",s1->str);
      yywarning("Got     : %s",s2->str);
    }else{
      my_yyerror("Expected: %s",s1->str);
      my_yyerror("Got     : %s",s2->str);
    }
    free_string(s1);
    free_string(s2);
  }

  if(implements_a && implements_b)
    yyexplain_not_implements(implements_a,implements_b,flags);
}


struct pike_type *debug_make_pike_type(const char *t)
{
  return make_shared_binary_string(t, type_length(t));
}


static int low_pike_type_allow_premature_toss(char *type)
{
 again:
  switch(EXTRACT_UCHAR(type++))
  {
    case T_NOT:
      return !low_pike_type_allow_premature_toss(type);

    case T_OBJECT:
    case T_MIXED:
    case T_FUNCTION:
      return 0;

    case PIKE_T_RING:
    case T_SCOPE:
    case T_ASSIGN:
      type++;
      goto again;

    case T_OR:
    case T_MAPPING:
      if(!low_pike_type_allow_premature_toss(type)) return 0;
    case T_AND:
      type+=type_length(type);
    case T_ARRAY:
    case T_MULTISET:
      goto again;

    case T_PROGRAM:
    case T_TYPE:
    case T_INT:
    case T_FLOAT:
    case T_STRING:
      return 1;
  }
  /* NOT_REACHED */
  return 0;
}

int pike_type_allow_premature_toss(struct pike_type *type)
{
  return low_pike_type_allow_premature_toss(type->str);
}

#endif /* USE_PIKE_TYPE */

struct pike_string *type_to_string(struct pike_type *t)
{
#ifdef USE_PIKE_TYPE
  init_buf();
  low_type_to_string(t);
  return free_buf();
#else /* !USE_PIKE_TYPE */
  add_ref(t);
  return t;
#endif /* USE_PIKE_TYPE */
}

void init_types(void)
{
#ifdef USE_PIKE_TYPE
  /* Initialize hashtable here. */
  pike_type_hash = (struct pike_type **)xalloc(sizeof(struct pike_type *) *
					       PIKE_TYPE_HASH_SIZE);
  MEMSET(pike_type_hash, 0, sizeof(struct pike_type *) * PIKE_TYPE_HASH_SIZE);
  pike_type_hash_size = PIKE_TYPE_HASH_SIZE;
  init_pike_type_blocks();
#endif /* USE_PIKE_TYPE */
  string_type_string = CONSTTYPE(tString);
  int_type_string = CONSTTYPE(tInt);
  object_type_string = CONSTTYPE(tObj);
  program_type_string = CONSTTYPE(tPrg(tObj));
  float_type_string = CONSTTYPE(tFloat);
  mixed_type_string = CONSTTYPE(tMix);
  array_type_string = CONSTTYPE(tArray);
  multiset_type_string = CONSTTYPE(tMultiset);
  mapping_type_string = CONSTTYPE(tMapping);
  function_type_string = CONSTTYPE(tFunction);
  type_type_string = CONSTTYPE(tType(tMix));
  void_type_string = CONSTTYPE(tVoid);
  zero_type_string = CONSTTYPE(tZero);
  any_type_string = CONSTTYPE(tOr(tVoid,tMix));
  weak_type_string = CONSTTYPE(tOr4(tArray,tMultiset,tMapping,
				    tFuncV(tNone,tZero,tOr(tMix,tVoid))));
}

void cleanup_pike_types(void)
{
#if defined(USE_PIKE_TYPE) && defined(DEBUG_MALLOC)
  struct pike_type_location *t = all_pike_type_locations;

  while(t) {
    free_type(t->t);
    t = t->next;
  }
#endif /* USE_PIKE_TYPE && DEBUG_MALLOC */

  free_type(string_type_string);
  free_type(int_type_string);
  free_type(float_type_string);
  free_type(function_type_string);
  free_type(object_type_string);
  free_type(program_type_string);
  free_type(array_type_string);
  free_type(multiset_type_string);
  free_type(mapping_type_string);
  free_type(type_type_string);
  free_type(mixed_type_string);
  free_type(void_type_string);
  free_type(zero_type_string);
  free_type(any_type_string);
  free_type(weak_type_string);
}

void cleanup_pike_type_table(void)
{
#ifdef USE_PIKE_TYPE
  /* Free the hashtable here. */
  if (pike_type_hash) {
    free(pike_type_hash);
    /* Don't do this, it messes up stuff... */
    /* pike_type_hash = NULL; */
  }
  /* Don't do this, it messes up stuff... */
  /* pike_type_hash_size = 0; */
#ifdef DEBUG_MALLOC
  free_all_pike_type_blocks();
#endif /* DEBUG_MALLOC */
#endif /* USE_PIKE_TYPE */
}
