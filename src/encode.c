/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: encode.c,v 1.163 2003/01/26 16:22:50 mirar Exp $
*/

#include "global.h"
#include "stralloc.h"
#include "pike_macros.h"
#include "object.h"
#include "constants.h"
#include "interpret.h"
#include "svalue.h"
#include "mapping.h"
#include "array.h"
#include "multiset.h"
#include "dynamic_buffer.h"
#include "pike_error.h"
#include "operators.h"
#include "builtin_functions.h"
#include "module_support.h"
#include "fsort.h"
#include "threads.h"
#include "stuff.h"
#include "version.h"
#include "bignum.h"
#include "pikecode.h"

RCSID("$Id: encode.c,v 1.163 2003/01/26 16:22:50 mirar Exp $");

/* #define ENCODE_DEBUG */

/* Use the old encoding method for programs. */
#define OLD_PIKE_ENCODE_PROGRAM

#ifdef ENCODE_DEBUG
/* Pass a nonzero integer as the third arg to encode_value,
 * encode_value_canonic and decode_value to activate this debug. */
#define EDB(N,X) do { debug_malloc_touch(data); if (data->debug>=N) {X;} } while (0)
#else
#define EDB(N,X) do { debug_malloc_touch(data); } while (0)
#endif

#ifdef _AIX
#include <net/nh.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif /* HAVE_IEEEFP_H */

#include <math.h>

#ifdef PIKE_DEBUG
#define encode_value2 encode_value2_
#define decode_value2 decode_value2_
#endif


/* Tags used by encode value.
 * Currently they only differ from the PIKE_T variants by
 *   TAG_FLOAT == PIKE_T_TYPE == 7
 * and
 *   TAG_TYPE == PIKE_T_FLOAT == 9
 * These are NOT to be renumbered unless the file-format version is changed!
 */
/* Current encoding: �ik0
 *
 * +---+-+-+-------+
 * |s z|s|n|t y p e|
 * +---+-+-+-------+
 *  	sz	size/small int
 *  	s	small int indicator
 *  	n	negative (or rather inverted)
 *  	type	TAG_type
 */
#define TAG_ARRAY 0
#define TAG_MAPPING 1
#define TAG_MULTISET 2
#define TAG_OBJECT 3
#define TAG_FUNCTION 4
#define TAG_PROGRAM 5
#define TAG_STRING 6
#define TAG_FLOAT 7
#define TAG_INT 8
#define TAG_TYPE 9           /* Not supported yet */

#define TAG_AGAIN 15
#define TAG_MASK 15
#define TAG_NEG 16
#define TAG_SMALL 32
#define SIZE_SHIFT 6
#define MAX_SMALL (1<<(8-SIZE_SHIFT))
#define COUNTER_START -MAX_SMALL

/* Entries used to encode the identifier_references table. */
#define ID_ENTRY_RAW		-2
#define ID_ENTRY_EOT		-1
#define ID_ENTRY_VARIABLE	0
#define ID_ENTRY_FUNCTION	1
#define ID_ENTRY_CONSTANT	2
#define ID_ENTRY_INHERIT	3

struct encode_data
{
  int canonic;
  struct object *codec;
  struct svalue counter;
  struct mapping *encoded;
  dynamic_buffer buf;
#ifdef ENCODE_DEBUG
  int debug, depth;
#endif
};

static void encode_value2(struct svalue *val, struct encode_data *data);

#define addstr(s, l) low_my_binary_strcat((s), (l), &(data->buf))
#define addchar(t)   low_my_putchar((char)(t), &(data->buf))

/* Code a pike string */

#if BYTEORDER == 4321
#define ENCODE_DATA(S) \
   addstr( (S)->str, (S)->len << (S)->size_shift );
#else
#define ENCODE_DATA(S) 				\
    switch((S)->size_shift)			\
    {						\
      case 1:					\
        for(q=0;q<(S)->len;q++) {		\
           INT16 s=htons( STR1(S)[q] );		\
           addstr( (char *)&s, sizeof(s));	\
        }					\
        break;					\
      case 2:					\
        for(q=0;q<(S)->len;q++) {		\
           INT32 s=htonl( STR2(S)[q] );		\
           addstr( (char *)&s, sizeof(s));	\
        }					\
        break;					\
    }
#endif

#define adddata(S) do {					\
  if((S)->size_shift)					\
  {							\
    int q;                                              \
    code_entry(TAG_STRING,-1, data);			\
    code_entry((S)->size_shift, (S)->len, data);	\
    ENCODE_DATA(S);                                     \
  }else{						\
    code_entry(TAG_STRING, (S)->len, data);		\
    addstr((char *)((S)->str),(S)->len);		\
  }							\
}while(0)

/* Like adddata, but allows null pointers */

#define adddata3(S) do {			\
  if(S) {					\
    adddata(S);                                 \
  } else {					\
    code_entry(TAG_INT, 0, data);			\
  }						\
}while(0)

#define adddata2(s,l) addstr((char *)(s),(l) * sizeof((s)[0]));

/* NOTE: Fix when type encodings change. */
static int type_to_tag(int type)
{
  if (type == T_FLOAT) return TAG_FLOAT;
  if (type == T_TYPE) return TAG_TYPE;
  return type;
}
static int (*tag_to_type)(int) = type_to_tag;

/* Let's cram those bits... */
static void code_entry(int tag, INT64 num, struct encode_data *data)
{
  int t;
  EDB(5,
    fprintf(stderr,"%*sencode: code_entry(tag=%d (%s), num=%ld)\n",
	    data->depth, "", tag,
	    get_name_of_type(tag_to_type(tag)),
	    (long)num) );
  if(num<0)
  {
    tag |= TAG_NEG;
    num = ~num;
  }

  if(num < MAX_SMALL)
  {
    tag |= TAG_SMALL | (num << SIZE_SHIFT);
    addchar((char)tag);
    return;
  }else{
    num -= MAX_SMALL;
  }

  for(t = 0; (size_t)t <
#if 0
	(sizeof(INT64)-1);
#else /* !0 */
      (size_t)3;
#endif /* 0 */
      t++)
  {
    if(num >= (((INT64)256) << (t<<3)))
      num -= (((INT64)256) << (t<<3));
    else
      break;
  }

  tag |= t << SIZE_SHIFT;
  addchar((char)tag);

  switch(t)
  {
#if 0
  case 7: addchar(DO_NOT_WARN((char)((num >> 56)&0xff)));
  case 6: addchar(DO_NOT_WARN((char)((num >> 48)&0xff)));
  case 5: addchar(DO_NOT_WARN((char)((num >> 40)&0xff)));
  case 4: addchar(DO_NOT_WARN((char)((num >> 32)&0xff)));
#endif /* 0 */
  case 3: addchar(DO_NOT_WARN((char)((num >> 24)&0xff)));
  case 2: addchar(DO_NOT_WARN((char)((num >> 16)&0xff)));
  case 1: addchar(DO_NOT_WARN((char)((num >> 8)&0xff)));
  case 0: addchar(DO_NOT_WARN((char)(num&0xff)));
  }
}

static void code_number(ptrdiff_t num, struct encode_data *data)
{
  EDB(5, fprintf(stderr, "%*scode_number(%d)\n",
		 data->depth, "", num));
  code_entry(DO_NOT_WARN(num & 15),
	     num >> 4, data);
}

#ifdef _REENTRANT
static void do_enable_threads(void)
{
  exit_threads_disable(NULL);
}
#endif

/* NOTE: Take care to encode it exactly as the corresponing
 *       type string would have been encoded (cf TFUNCTION, T_MANY).
 */
static void encode_type(struct pike_type *t, struct encode_data *data)
{
 one_more_type:
  if (t->type == T_MANY) {
    addchar(T_FUNCTION);
    addchar(T_MANY);
  } else {
    addchar(t->type);
  }
  switch(t->type) {
    default:
      Pike_fatal("error in type tree: %d.\n", t->type);
      /*NOTREACHED*/

      break;

    case PIKE_T_NAME:
      {
	struct svalue sval;
	sval.type = PIKE_T_STRING;
	sval.subtype = 0;
	sval.u.string = (void *)t->car;

	encode_value2(&sval, data);
      }
      t=t->cdr;
      goto one_more_type;
    
    case T_ASSIGN:
      {
	ptrdiff_t marker = ((char *)t->car)-(char *)0;
	if ((marker < 0) || (marker > 9)) {
	  Pike_fatal("Bad assign marker: %ld\n",
		     (long)marker);
	}
	addchar('0' + marker);
	t = t->cdr;
      }
      goto one_more_type;

    case T_FUNCTION:
      while(t->type == T_FUNCTION) {
	encode_type(t->car, data);
	t = t->cdr;
      }
      addchar(T_MANY);
      /* FALL_THROUGH */
    case T_MANY:
      encode_type(t->car, data);
      t = t->cdr;
      goto one_more_type;

    case T_MAPPING:
    case T_OR:
    case T_AND:
      encode_type(t->car, data);
      t = t->cdr;
      goto one_more_type;

    case T_TYPE:
    case T_PROGRAM:
    case T_ARRAY:
    case T_MULTISET:
    case T_NOT:
      t = t->car;
      goto one_more_type;

    case T_INT:
      {
	ptrdiff_t val;

	val = ((char *)t->car)-(char *)0;
	addchar((val >> 24)&0xff);
	addchar((val >> 16)&0xff);
	addchar((val >> 8)&0xff);
	addchar(val & 0xff);
	val = ((char *)t->cdr)-(char *)0;
	addchar((val >> 24)&0xff);
	addchar((val >> 16)&0xff);
	addchar((val >> 8)&0xff);
	addchar(val & 0xff);
      }
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
    case T_ZERO:
    case T_VOID:
    case PIKE_T_UNKNOWN:
      break;

    case T_OBJECT:
    {
      addchar(((char *)t->car)-(char *)0);

      if(t->cdr)
      {
	ptrdiff_t id = ((char *)t->cdr)-(char *)0;
	if( id >= PROG_DYNAMIC_ID_START )
	{
	  struct program *p=id_to_program(id);
	  if(p)
	  {
	    ref_push_program(p);
	  }else{
	    push_int(0);
	  }
	} else
	  push_int( id );
      }else{
	push_int(0);
      }
      encode_value2(Pike_sp-1, data);
      pop_stack();
      break;
    }
  }
}

static void zap_unfinished_program(struct program *p)
{
  int e;
  debug_malloc_touch(p);
  if(p->flags & PROGRAM_FIXED) return; /* allow natural zapping */
  if(p->parent)
  {
    free_program(p->parent);
    p->parent=0;
  }
  for(e=0;e<p->num_constants;e++)
  {
    free_svalue(& p->constants[e].sval);
    p->constants[e].sval.type=T_INT;
  }
  
  for(e=0;e<p->num_inherits;e++)
  {
    if(p->inherits[e].parent)
    {
      free_object(p->inherits[e].parent);
      p->inherits[e].parent=0;
    }
    if(e)
    {
      if(p->inherits[e].prog)
      {
	free_program(p->inherits[e].prog);
	p->inherits[e].prog=0;
      }
    }
  }
}

static void encode_value2(struct svalue *val, struct encode_data *data)

#ifdef PIKE_DEBUG
#undef encode_value2
#define encode_value2(X,Y) do { struct svalue *_=Pike_sp; encode_value2_(X,Y); if(Pike_sp!=_) Pike_fatal("encode_value2 failed!\n"); } while(0)
#endif

{
  static struct svalue dested = {
    T_INT, NUMBER_DESTRUCTED,
#ifdef HAVE_UNION_INIT
    {0}, /* Only to avoid warnings. */
#endif
  };
  INT32 i;
  struct svalue *tmp;

#ifdef ENCODE_DEBUG
  data->depth += 2;
#endif

  if((val->type == T_OBJECT ||
      (val->type==T_FUNCTION && val->subtype!=FUNCTION_BUILTIN)) &&
     !val->u.object->prog)
    val = &dested;

  if((tmp=low_mapping_lookup(data->encoded, val)))
  {
    EDB(1,fprintf(stderr, "%*sEncoding TAG_AGAIN from <%d>\n",
		data->depth, "", tmp->u.integer));
    code_entry(TAG_AGAIN, tmp->u.integer, data);
#ifdef ENCODE_DEBUG
    data->depth -= 2;
#endif
    return;
  }else if (val->type != T_TYPE) {
    EDB(1,fprintf(stderr, "%*sEncoding to <%d>: ",
		data->depth, "", data->counter.u.integer);
	if(data->debug == 1)
        {
	  fprintf(stderr,"TAG%d",val->type);
	}else{
	  print_svalue(stderr, val);
	  
	}
	fputc('\n', stderr););
    mapping_insert(data->encoded, val, &data->counter);
    data->counter.u.integer++;
  }


  switch(val->type)
  {
    case T_INT:
      /* FIXME: Doesn't encode NUMBER_UNDEFINED et al. */

#if SIZEOF_INT_TYPE > 4
    {
      INT_TYPE i=val->u.integer;
      if (i != (INT32)i)
      {
	 push_int(i);
	 convert_stack_top_to_bignum();
	 encode_value2(Pike_sp-1,data);
	 pop_stack();
	 return;
      }
      else
	 code_entry(TAG_INT, i,data);
    }
#else       
      code_entry(TAG_INT, val->u.integer,data);
#endif
      break;

    case T_STRING:
      adddata(val->u.string);
      break;

    case T_TYPE:
      /* NOTE: Types are added to the encoded mapping AFTER they have
       *       been encoded, to simplify decoding.
       */
      if (data->canonic)
	Pike_error("Canonical encoding of the type type not supported.\n");
      code_entry(TAG_TYPE, 0, data);	/* Type encoding #0 */
      encode_type(val->u.type, data);
      EDB(2,fprintf(stderr, "%*sEncoded type to <%d>: ",
		  data->depth, "", data->counter.u.integer);
	  print_svalue(stderr, val);
	  fputc('\n', stderr););
      mapping_insert(data->encoded, val, &data->counter);
      data->counter.u.integer++;
      break;

    case T_FLOAT:
    {
      double d = val->u.float_number;

#define Pike_FP_SNAN -4 /* Signal Not A Number */
#define Pike_FP_QNAN -3 /* Quiet Not A Number */
#define Pike_FP_NINF -2 /* Negative infinity */
#define Pike_FP_PINF -1 /* Positive infinity */
#define Pike_FP_ZERO  0 /* Backwards compatible zero */
#define Pike_FP_NZERO 1 /* Negative Zero */
#define Pike_FP_PZERO 0 /* Positive zero */
#define Pike_FP_UNKNOWN -4711 /* Positive zero */


#ifdef HAVE_FPCLASS
      switch(fpclass(d)) {
	case FP_SNAN:
	  code_entry(TAG_FLOAT,0,data);
	  code_entry(TAG_FLOAT,Pike_FP_SNAN,data);
	  break;

	case FP_QNAN:
	  code_entry(TAG_FLOAT,0,data);
	  code_entry(TAG_FLOAT,Pike_FP_QNAN,data);
	  break;

	case FP_NINF:
	  code_entry(TAG_FLOAT,0,data);
	  code_entry(TAG_FLOAT,Pike_FP_NINF,data);
	  break;

	case FP_PINF:
	  code_entry(TAG_FLOAT,0,data);
	  code_entry(TAG_FLOAT,Pike_FP_PINF,data);
	  break;

	case FP_NZERO:
	  code_entry(TAG_FLOAT,0,data);
	  code_entry(TAG_FLOAT,Pike_FP_NZERO,data);
	  break;

	case FP_PZERO:
	  code_entry(TAG_FLOAT,0,data);
	  code_entry(TAG_FLOAT,Pike_FP_ZERO,data); /* normal zero */
	  break;

	  /* Ugly, but switch gobbles breaks -Hubbe */
	default:
	  goto encode_normal_float;
      }
      break;
    encode_normal_float:

#else
      {
	int pike_ftype=Pike_FP_UNKNOWN;
#ifdef HAVE_ISINF
	if(isinf(d))
	  pike_ftype=Pike_FP_PINF;
	else
#endif
#ifdef HAVE_ISNAN
	  if(isnan(d)) {
	    pike_ftype=Pike_FP_SNAN;
	  } else
#endif
#ifdef HAVE_ISZERO
	    if(iszero(d))
	      pike_ftype=Pike_FP_PZERO;
	    else
#endif
#ifdef HAVE_FINITE
	      if(!finite(d))
		pike_ftype=Pike_FP_PINF;
#endif
	; /* Terminate any remaining else */
	
	if(
#ifdef HAVE_SIGNBIT
	  signbit(d)
#else
	  d<0.0
#endif
	  ) {
	  switch(pike_ftype)
	  {
	    case Pike_FP_PINF:
	      pike_ftype=Pike_FP_NINF;
	      break;
	      
	    case Pike_FP_PZERO:
	      pike_ftype=Pike_FP_NZERO;
	      break;
	  }
	}
	
	if(pike_ftype != Pike_FP_UNKNOWN)
	{
	  code_entry(TAG_FLOAT,0,data);
	  code_entry(TAG_FLOAT,pike_ftype,data);
	  break;
	}
      }
#endif

      if(d == 0.0)
      {
	code_entry(TAG_FLOAT,0,data);
	code_entry(TAG_FLOAT,0,data);
      }else{
	INT64 x;
	int y;
	double tmp;

	tmp = FREXP(d, &y);
	x = DO_NOT_WARN((INT64)((((INT64)1)<<(sizeof(INT64)*8 - 2))*tmp));
	y -= sizeof(INT64)*8 - 2;

	EDB(2,fprintf(stderr,
		    "Encoding float... tmp: %10g, x: 0x%016llx, y: %d\n",
		    tmp, x, y));

#if 0
	if (x && !(x & 0xffffffffUL)) {
#endif /* 0 */
	  x >>= 32;
	  y += 32;

	  EDB(2,fprintf(stderr,
		      "Reducing float... x: 0x%08llx, y: %d\n",
		      x, y));
#if 0
	}
#endif /* 0 */
#if 0
	while(x && y && !(x&1))
	{
	  x>>=1;
	  y++;
	}
#endif
	code_entry(TAG_FLOAT,x,data);
	code_entry(TAG_FLOAT,y,data);
      }
      break;
    }

    case T_ARRAY:
      code_entry(TAG_ARRAY, val->u.array->size, data);
      for(i=0; i<val->u.array->size; i++)
	encode_value2(ITEM(val->u.array)+i, data);
      break;

    case T_MAPPING:
      check_stack(2);
      ref_push_mapping(val->u.mapping);
      f_indices(1);

      ref_push_mapping(val->u.mapping);
      f_values(1);

      if (data->canonic) {
	INT32 *order;
	if (val->u.mapping->data->ind_types & ~(BIT_BASIC & ~BIT_TYPE)) {
	  mapping_fix_type_field(val->u.mapping);
	  if (val->u.mapping->data->ind_types & ~(BIT_BASIC & ~BIT_TYPE))
	    /* This doesn't let bignums through. That's necessary as
	     * long as they aren't handled deterministically by the
	     * sort function. */
	    /* They should be hanled deterministically now - Hubbe */
	    Pike_error("Canonical encoding requires basic types in indices.\n");
	}
	order = get_switch_order(Pike_sp[-2].u.array);
	order_array(Pike_sp[-2].u.array, order);
	order_array(Pike_sp[-1].u.array, order);
	free((char *) order);
      }

      code_entry(TAG_MAPPING, Pike_sp[-2].u.array->size,data);
      for(i=0; i<Pike_sp[-2].u.array->size; i++)
      {
	encode_value2(ITEM(Pike_sp[-2].u.array)+i, data); /* indices */
	encode_value2(ITEM(Pike_sp[-1].u.array)+i, data); /* values */
      }
      pop_n_elems(2);
      break;

    case T_MULTISET: {
      struct multiset *l = val->u.multiset;

#ifdef PIKE_NEW_MULTISETS
      if (multiset_indval (l) || multiset_get_cmp_less (l)->type != T_INT)
	Pike_error ("FIXME: Encoding of multisets with values and/or "
		    "custom sort function not yet implemented.\n");
      else {
	/* Encode valueless multisets without compare functions in a
	 * compatible way. */
#endif
	code_entry(TAG_MULTISET, multiset_sizeof (l), data);
	if (data->canonic) {
	  INT32 *order;
	  if (multiset_ind_types(l) & ~(BIT_BASIC & ~BIT_TYPE)) {
	    multiset_fix_type_field(l);
	    if (multiset_ind_types(l) & ~(BIT_BASIC & ~BIT_TYPE))
	      /* This doesn't let bignums through. That's necessary as
	       * long as they aren't handled deterministically by the
	       * sort function. */
	      Pike_error("Canonical encoding requires basic types in indices.\n");
	  }
	  check_stack(1);
#ifdef PIKE_NEW_MULTISETS
	  push_array(multiset_indices(l));
#else
	  push_array(copy_array(l->ind));
#endif
	  order = get_switch_order(Pike_sp[-1].u.array);
	  order_array(Pike_sp[-1].u.array, order);
	  free((char *) order);
	  for (i = 0; i < Pike_sp[-1].u.array->size; i++)
	    encode_value2(ITEM(Pike_sp[-1].u.array)+i, data);
	  pop_stack();
	}
	else {
#ifdef PIKE_NEW_MULTISETS
	  struct svalue ind;
	  union msnode *node = low_multiset_first (l->msd);
	  for (; node; node = low_multiset_next (node))
	    encode_value2 (low_use_multiset_index (node, ind), data);
#else
	  for(i=0; i<l->ind->size; i++)
	    encode_value2(ITEM(l->ind)+i, data);
#endif
	}
#ifdef PIKE_NEW_MULTISETS
      }
#endif
      break;
    }

    case T_OBJECT:
      check_stack(1);

#ifdef AUTO_BIGNUM
      /* This could be implemented a lot more generic,
       * but that will have to wait until next time. /Hubbe
       */
      if(is_bignum_object(val->u.object))
      {
	code_entry(TAG_OBJECT, 2, data);
	/* 256 would be better, but then negative numbers
	 * won't work... /Hubbe
	 */
	push_int(36);
	apply(val->u.object,"digits",1);
	if(Pike_sp[-1].type != T_STRING)
	  Pike_error("Gmp.mpz->digits did not return a string!\n");
	encode_value2(Pike_sp-1, data);
	pop_stack();
	break;
      }
#endif

      if (data->canonic)
	Pike_error("Canonical encoding of objects not supported.\n");
      push_svalue(val);
      apply(data->codec, "nameof", 1);
      EDB(5, fprintf(stderr, "%*s->nameof: ", data->depth, "");
	  print_svalue(stderr, Pike_sp-1);
	  fputc('\n', stderr););
      switch(Pike_sp[-1].type)
      {
	case T_INT:
	  if(Pike_sp[-1].subtype == NUMBER_UNDEFINED)
	  {
	    int to_change = data->buf.s.len;
	    struct svalue tmp=data->counter;
	    tmp.u.integer--;

	    EDB(5,fprintf(stderr, "%*s(UNDEFINED)\n", data->depth, ""));

	    /* We have to remove ourself from the cache */
	    map_delete(data->encoded, val);
	    
	    pop_stack();
	    push_svalue(val);
	    f_object_program(1);

	    /* Code the program */
	    code_entry(TAG_OBJECT, 3,data);
	    encode_value2(Pike_sp-1, data);
	    pop_stack();
	    
	    push_svalue(val);

	    /* If we do not exist in cache, use backwards-
	     * compatible method, otherwise use newfangled
	     * style=3.  -Hubbe
	     */
	    if(!low_mapping_lookup(data->encoded, val))
	    {
	      int fun;
	      EDB(1,fprintf(stderr, "%*sZapping 3 -> 1 in TAG_OBJECT\n",
			    data->depth, ""));
	      
	      /* This causes the code_entry above to
	       * become: code_entry(TAG_OBJECT, 1, data);
	       * -Hubbe
	       */
	      data->buf.s.str[to_change] = 99;

	      fun = find_identifier("encode_object", data->codec->prog);
	      if (fun < 0)
		Pike_error("Cannot encode objects without an "
			   "\"encode_object\" function in the codec.\n");
	      apply_low(data->codec,fun,1);

	      /* Put value back in cache for future reference -Hubbe */
	      mapping_insert(data->encoded, val, &tmp);
	    }
	    break;
	  }
	  /* FALL THROUGH */

	default:
	  code_entry(TAG_OBJECT, 0,data);
	  break;
      }
      encode_value2(Pike_sp-1, data);
      pop_stack();
      break;

    case T_FUNCTION:
      /* FIXME: Ought to have special treatment of trampolines. */
      if (data->canonic)
	Pike_error("Canonical encoding of functions not supported.\n");
      check_stack(1);
      push_svalue(val);
      apply(data->codec,"nameof", 1);
      if(Pike_sp[-1].type == T_INT && Pike_sp[-1].subtype==NUMBER_UNDEFINED)
      {
	if(val->subtype != FUNCTION_BUILTIN)
	{
	  if(find_shared_string_identifier(ID_FROM_INT(val->u.object->prog, val->subtype)->name,
					   val->u.object->prog)==val->subtype)
	  {
	    /* We have to remove ourself from the cache for now */
	    struct svalue tmp=data->counter;
	    tmp.u.integer--;
	    map_delete(data->encoded, val);

	    code_entry(TAG_FUNCTION, 1, data);
	    push_svalue(val);
	    Pike_sp[-1].type=T_OBJECT;
	    encode_value2(Pike_sp-1, data);
	    ref_push_string(ID_FROM_INT(val->u.object->prog, val->subtype)->name);
	    encode_value2(Pike_sp-1, data);
	    pop_n_elems(3);

	    /* Put value back in cache */
	    mapping_insert(data->encoded, val, &tmp);
#ifdef ENCODE_DEBUG
	    data->depth -= 2;
#endif
	    return;
	  }
	}
	Pike_error("Encoding of efuns is not supported yet.\n");
      }

      code_entry(TAG_FUNCTION, 0, data);
      encode_value2(Pike_sp-1, data);
      pop_stack();
      break;


    case T_PROGRAM:
    {
      int d;
      if (val->u.program->id < PROG_DYNAMIC_ID_START) {
	code_entry(TAG_PROGRAM, 3, data);
	push_int(val->u.program->id);
	encode_value2(Pike_sp-1, data);
	pop_stack();
	break;
      }
      if (data->canonic)
	Pike_error("Canonical encoding of programs not supported.\n");
      check_stack(1);
      push_svalue(val);
      apply(data->codec,"nameof", 1);
      if(Pike_sp[-1].type == val->type)
	Pike_error("Error in master()->nameof(), same type returned.\n");
      if(Pike_sp[-1].type == T_INT && Pike_sp[-1].subtype == NUMBER_UNDEFINED)
      {
	struct program *p=val->u.program;
	if( (p->flags & PROGRAM_HAS_C_METHODS) || p->event_handler )
	{
	  if(p->parent)
	  {
	    /* We have to remove ourself from the cache for now */
	    struct svalue tmp=data->counter;
	    tmp.u.integer--;
	    map_delete(data->encoded, val);

	    code_entry(TAG_PROGRAM, 2, data);
	    ref_push_program(p->parent);
	    encode_value2(Pike_sp-1,data);

	    ref_push_program(p);
	    f_function_name(1);
	    if(Pike_sp[-1].type == PIKE_T_INT)
	      Pike_error("Cannot encode C programs.\n");
	    encode_value2(Pike_sp-1, data);

	    pop_n_elems(3);

	    /* Put value back in cache */
	    mapping_insert(data->encoded, val, &tmp);
#ifdef ENCODE_DEBUG
	    data->depth -= 2;
#endif
	    return;
	  }
	  if( p->event_handler )
	    Pike_error("Cannot encode programs with event handlers.\n");
	  Pike_error("Cannot encode C programs.\n");
	}


	EDB(1,
	    fprintf(stderr, "%*sencode: encoding program\n",
		    data->depth, ""));

#ifdef OLD_PIKE_ENCODE_PROGRAM

	/* Type 1 -- Old-style encoding. */

	code_entry(TAG_PROGRAM, 1, data);
	f_version(0);
	encode_value2(Pike_sp-1,data);
	pop_stack();
	code_number(p->flags,data);
	code_number(p->storage_needed,data);
	code_number(p->xstorage,data);			/**/
	code_number(p->parent_info_storage,data);	/**/

	code_number(p->alignment_needed,data);
	code_number(p->timestamp.tv_sec,data);
	code_number(p->timestamp.tv_usec,data);

	if(p->parent)
	  ref_push_program(p->parent);
	else
	  push_int(0);
	encode_value2(Pike_sp-1,data);			/**/
	pop_stack();

#define FOO(X,Y,Z) \
	code_number( p->PIKE_CONCAT(num_,Z), data);
#include "program_areas.h"

	code_number(PIKE_BYTECODE_METHOD, data);

#ifdef ENCODE_PROGRAM
#ifdef PIKE_DEBUG
	{
	  ptrdiff_t bufpos = data->buf.s.len;
#endif /* PIKE_DEBUG */
	  ENCODE_PROGRAM(p, &(data->buf));
#ifdef PIKE_DEBUG
	  if (p->num_program * sizeof(p->program[0]) !=
	      data->buf.s.len - bufpos) {
	    Pike_fatal("ENCODE_PROGRAM() failed:\n"
		  "Encoded data len: %ld\n"
		  "Expected data len: %ld\n",
		  DO_NOT_WARN((long)(p->num_program * sizeof(p->program[0]))),
		  DO_NOT_WARN((long)(data->buf.s.len - bufpos)));
	  }
	}
#endif /* PIKE_DEBUG */
#else /* !ENCODE_PROGRAM */
	adddata2(p->program, p->num_program);
#endif /* ENCODE_PROGRAM */

	adddata2(p->relocations, p->num_relocations);

	adddata2(p->linenumbers, p->num_linenumbers);
	
	for(d=0;d<p->num_identifier_index;d++)
	  code_number(p->identifier_index[d],data);

	for(d=0;d<p->num_variable_index;d++)
	  code_number(p->variable_index[d],data);

	for(d=0;d<p->num_identifier_references;d++)
	{
	  code_number(p->identifier_references[d].inherit_offset,data);
	  code_number(p->identifier_references[d].identifier_offset,data);
	  code_number(p->identifier_references[d].id_flags,data);
	  EDB(3,fprintf(stderr,"IDREF%x > %d: { %d, %d, %d }\n",
		      p->id,d,
		      p->identifier_references[d].inherit_offset,
		      p->identifier_references[d].identifier_offset,
		      p->identifier_references[d].id_flags););
	}

	for(d=0;d<p->num_strings;d++) adddata(p->strings[d]);

	for(d=0;d<p->num_inherits;d++)
	{
	  code_number(p->inherits[d].inherit_level,data);
	  code_number(p->inherits[d].identifier_level,data);
	  code_number(p->inherits[d].parent_offset,data);
	  code_number(p->inherits[d].parent_identifier,data);
	  code_number(p->inherits[d].storage_offset,data);

	  if(p->inherits[d].parent)
	  {
	    ref_push_function(p->inherits[d].parent,
			      p->inherits[d].parent_identifier);
	    EDB(3,fprintf(stderr,"INHERIT%x coded as func { %p, %d }\n",
			p->id, p->inherits[d].parent, p->inherits[d].parent_identifier););
	  }else if(p->inherits[d].prog){
	    ref_push_program(p->inherits[d].prog);
	  }else{
	    Pike_error("Failed to encode inherit #%d\n", d);
	    push_int(0);
	  }
	  encode_value2(Pike_sp-1,data);
	  pop_stack();

          adddata3(p->inherits[d].name);

	  EDB(3,fprintf(stderr,"INHERIT%x > %d: %d id=%d\n",
		      p->id,d,
		      p->inherits[d].prog->num_identifiers,
		      p->inherits[d].prog->id););
	}

	for(d=0;d<p->num_identifiers;d++)
	{
	  adddata(p->identifiers[d].name);
	  encode_type(p->identifiers[d].type, data);
	  code_number(p->identifiers[d].identifier_flags,data);
	  code_number(p->identifiers[d].run_time_type,data);
	  code_number(p->identifiers[d].opt_flags,data);
	  if (!(p->identifiers[d].identifier_flags & IDENTIFIER_C_FUNCTION)) {
	    code_number(p->identifiers[d].func.offset,data);
	  } else {
	    Pike_error("Cannot encode functions implemented in C "
		       "(identifier='%s').\n",
		       p->identifiers[d].name->str);
	  }
	}

	for(d=0;d<NUM_LFUNS;d++)
	  code_number(p->lfuns[d], data);

	for(d=0;d<p->num_constants;d++)
	{
	  encode_value2(& p->constants[d].sval, data);
	  adddata3(p->constants[d].name);
	}

#else /* !OLD_PIKE_ENCODE_PROGRAM */

	/* Type 4 -- Portable encoding. */
	code_entry(type_to_tag(val->type), 4, data);

	/* Byte-order. */
	code_number(PIKE_BYTEORDER, data);

	/* flags */
	code_number(p->flags,data);

	/* version */
	f_version(0);
	encode_value2(Pike_sp-1, data);
	pop_stack();

	/* parent */
	if (p->parent) {
	  ref_push_program(p->parent);
	} else {
	  push_int(0);
	}
	encode_value2(Pike_sp-1, data);
	pop_stack();

	/* num_* */
#define FOO(X,Y,Z) \
        code_number( p->PIKE_CONCAT(num_,Z), data);
#include "program_areas.h"

	/* Byte-code method
	 */
	code_number(PIKE_BYTECODE_METHOD, data);

	/* program */
#ifdef ENCODE_PROGRAM
#ifdef PIKE_DEBUG
	{
	  ptrdiff_t bufpos = data->buf.s.len;
#endif /* PIKE_DEBUG */
	  ENCODE_PROGRAM(p, &(data->buf));
#ifdef PIKE_DEBUG
	  if (p->num_program * sizeof(p->program[0]) !=
	      data->buf.s.len - bufpos) {
	    Pike_fatal("ENCODE_PROGRAM() failed:\n"
		  "Encoded data len: %ld\n"
		  "Expected data len: %ld\n",
		  DO_NOT_WARN((long)(p->num_program * sizeof(p->program[0]))),
		  DO_NOT_WARN((long)(data->buf.s.len - bufpos)));
	  }
	}
#endif /* PIKE_DEBUG */
#else /* !ENCODE_PROGRAM */
	adddata2(p->program, p->num_program);
#endif /* ENCODE_PROGRAM */

	/* relocations */
	for(d=0; d<(int)p->num_relocations; d++) {
	  code_number(p->relocations[d], data);
	}

	/* linenumbers */
	adddata2(p->linenumbers, p->num_linenumbers);

	{
	  struct svalue str_sval;
	  str_sval.type = T_STRING;
	  str_sval.subtype = 0;
	  /* strings */
	  for(d=0;d<p->num_strings;d++) {
	    str_sval.u.string = p->strings[d];
	    encode_value2(&str_sval, data);
	  }
	}

	EDB(5, dump_program_tables(p, data->depth));

	/* Dump the identifiers in a portable manner... */
	{
	  int inherit_num = 1;
	  struct svalue str_sval;
	  char *id_dumped = (char *) alloca(p->num_identifiers);
	  MEMSET(id_dumped,0,p->num_identifiers);
	  str_sval.type = T_STRING;
	  str_sval.subtype = 0;

	  EDB(2,
	      fprintf(stderr, "%*sencode: encoding references\n",
		      data->depth, ""));

#ifdef ENCODE_DEBUG
	  data->depth += 2;
#endif

	  /* NOTE: d is incremented by hand inside the loop. */
	  for (d=0; d < p->num_identifier_references;)
	  {
	    int d_max = p->num_identifier_references;

	    /* Find insertion point of next inherit. */
	    if (inherit_num < p->num_inherits) {
	      d_max = p->inherits[inherit_num].identifier_ref_offset;
	    }

	    /* Fix locally defined identifiers. */
	    for (; d < d_max; d++) {
	      struct reference *ref = p->identifier_references + d;
	      struct inherit *inh = INHERIT_FROM_PTR(p, ref);
	      struct identifier *id = ID_FROM_PTR(p, ref);

	      /* Skip identifiers that haven't been overloaded. */
	      if (ref->id_flags & ID_INHERITED) continue;

	      EDB(3,
		  fprintf(stderr,
			  "%*sencoding identifier ref %d: %4x \"%s\"\n",
			  data->depth, "", d,
			  id->identifier_flags,
			  id->name->str));

#ifdef ENCODE_DEBUG
	      data->depth += 2;
#endif

	      /* Variable, constant or function. */

	      if (ref->inherit_offset || ref->id_flags & ID_HIDDEN) {
		int ref_no = -1;
		/* Explicit reference to inherited symbol. */

		EDB(3,
		    fprintf(stderr, "%*sencode: encoding raw reference\n",
			    data->depth, ""));

		code_number(ID_ENTRY_RAW, data);
		code_number(ref->id_flags, data);

		/* inherit_offset */
		code_number(ref->inherit_offset, data);

		/* identifier_offset */
		/* Find the corresponding identifier reference
		 * in the inherit. */
		{
		  struct program *p2 = p->inherits[ref->inherit_offset].prog;
		  int i;
		  for (i=0; i < p2->num_identifier_references; i++) {
		    struct reference *ref2 = p2->identifier_references + i;
		    if (!(ref2->inherit_offset) &&
			!(ref2->id_flags & ID_HIDDEN) &&
			(ref2->identifier_offset == ref->identifier_offset)) {
		      ref_no = i;
		      break;
		    }
		  }
		}
		if (ref_no == -1) {
		  Pike_error("Failed to reverse explicit reference\n");
		}
		code_number(ref_no, data);
	      } else {
		if (id_dumped[ref->identifier_offset]) {
		  EDB(3,
		      fprintf(stderr, "%*sencode: already encoded reference\n",
			      data->depth, ""));
		  goto next_identifier_ref;
		}
		id_dumped[ref->identifier_offset] = 1;

		if (IDENTIFIER_IS_CONSTANT(id->identifier_flags)) {
		  /* Constant */

		  EDB(3,
		      fprintf(stderr, "%*sencode: encoding constant\n",
			      data->depth, ""));

		  code_number(ID_ENTRY_CONSTANT, data);
		  code_number(ref->id_flags, data);

		  /* name */
		  str_sval.u.string = id->name;
		  encode_value2(&str_sval, data);

		  /* offset */
		  code_number(id->func.offset, data);

		  /* type */
		  ref_push_type_value(id->type);
		  encode_value2(Pike_sp-1, data);
		  pop_stack();

		  /* run-time type */
		  code_number(id->run_time_type, data);
		} else if (IDENTIFIER_IS_PIKE_FUNCTION(id->identifier_flags)) {
		  /* Pike function */

		  EDB(3,
		      fprintf(stderr, "%*sencode: encoding function\n",
			      data->depth, ""));

		  code_number(ID_ENTRY_FUNCTION, data);
		  code_number(ref->id_flags, data);

		  /* name */
		  str_sval.u.string = id->name;
		  encode_value2(&str_sval, data);

		  /* type */
		  ref_push_type_value(id->type);
		  encode_value2(Pike_sp-1, data);
		  pop_stack();

		  /* func_flags (aka identifier_flags) */
		  code_number(id->identifier_flags, data);

		  /* func */
		  code_number(id->func.offset, data);

		  /* opt_flags */
		  code_number(id->opt_flags, data);
		} else if (id->identifier_flags & IDENTIFIER_C_FUNCTION) {
		  /* C Function */
		  /* Not supported. */
		  Pike_error("Cannot encode functions implemented in C "
			     "(identifier='%s').\n",
			     p->identifiers[d].name->str);
		} else {
		  /* Variable */
		  EDB(3,
		      fprintf(stderr, "%*sencode: encoding variable\n",
			      data->depth, ""));

		  code_number(ID_ENTRY_VARIABLE, data);
		  code_number(ref->id_flags, data);

		  /* name */
		  str_sval.u.string = id->name;
		  encode_value2(&str_sval, data);

		  /* type */
		  ref_push_type_value(id->type);
		  encode_value2(Pike_sp-1, data);
		  pop_stack();
		}
	      }

	      /* Identifier reference number */
	      code_number(d, data);

	    next_identifier_ref:
	      ;		/* C requires a statement after lables. */
#ifdef ENCODE_DEBUG
	      data->depth -= 2;
#endif
	    }

	    /* Encode next inherit. */
	    if (inherit_num < p->num_inherits) {
	      /* Inherit */
	      INT16 inherit_flags_change = 0;
	      struct inherit *inh = p->inherits + inherit_num;
	      struct reference *ref = p->identifier_references + d;
	      int i;

	      EDB(3,
		  fprintf(stderr, "%*sencode: encoding inherit\n",
			  data->depth, ""));

#ifdef ENCODE_DEBUG
	      data->depth += 2;
#endif

	      code_number(ID_ENTRY_INHERIT, data);

	      /* Calculate id_flags */
	      for (i = 0; i < inh->prog->num_identifier_references; i++) {
		/* Ignore overloaded identifiers. */
		if (ref[i].inherit_offset) {
		  inherit_flags_change |= ref[i].id_flags ^
		    inh->prog->identifier_references[i].id_flags;
		}
	      }
	      EDB(5,
		  fprintf(stderr, "%*sraw inherit_flags: %04x\n",
			  data->depth, "", inherit_flags_change));
	      inherit_flags_change &= ~(ID_HIDDEN|ID_INHERITED);
	      code_number(inherit_flags_change, data);

	      EDB(5,
		  fprintf(stderr, "%*sinherit_flags: %04x\n",
			  data->depth, "", inherit_flags_change));

	      /* Identifier reference level at insertion. */
	      code_number(d_max, data);

	      /* name */
	      str_sval.u.string = inh->name;
	      encode_value2(&str_sval, data);

	      /* prog */
	      ref_push_program(inh->prog);
	      encode_value2(Pike_sp-1, data);
	      pop_stack();

	      /* parent */
	      if (inh->parent) {
		ref_push_object(inh->parent);
	      } else {
		push_int(0);
	      }
	      encode_value2(Pike_sp-1, data);
	      pop_stack();

	      /* parent_identifier */
	      code_number(inh->parent_identifier, data);

	      /* parent_offset */
	      code_number(inh->parent_offset, data);

	      /* Number of identifier references. */
	      code_number(inh->prog->num_identifier_references, data);

	      inherit_num += inh->prog->num_inherits;

#ifdef ENCODE_DEBUG
	      data->depth -= 2;
#endif
	    }
	  }
	  /* End-marker */
	  code_number(ID_ENTRY_EOT, data);

#ifdef ENCODE_DEBUG
	  data->depth -= 2;
#endif
	}

	/* Encode the constant values table. */
	{
	  struct svalue str_sval;
	  str_sval.type = T_STRING;
	  str_sval.subtype = 0;

	  /* constants */
	  for(d=0;d<p->num_constants;d++)
	  {
	    /* value */
	    encode_value2(&p->constants[d].sval, data);

	    /* name */
	    if (p->constants[d].name) {
	      str_sval.u.string = p->constants[d].name;
	      encode_value2(&str_sval, data);
	    } else {
	      push_int(0);
	      encode_value2(Pike_sp-1, data);
	      Pike_sp--;
	    }
	  }
	}
#endif /* OLD_PIKE_ENCODE_PROGRAM */
      }else{
	code_entry(TAG_PROGRAM, 0, data);
	encode_value2(Pike_sp-1, data);
      }
      pop_stack();
      break;
    }
  }

#ifdef ENCODE_DEBUG
  data->depth -= 2;
#endif
}

static void free_encode_data(struct encode_data *data)
{
  toss_buffer(& data->buf);
  free_mapping(data->encoded);
}

/*! @decl string encode_value(mixed value, object|void codec)
 *!
 *! Code a value into a string.
 *!
 *! This function takes a value, and converts it to a string. This string
 *! can then be saved, sent to another Pike process, packed or used in
 *! any way you like. When you want your value back you simply send this
 *! string to @[decode_value()] and it will return the value you encoded.
 *!
 *! Almost any value can be coded, mappings, floats, arrays, circular
 *! structures etc.
 *!
 *! To encode objects, programs and functions, a codec object must be
 *! provided.
 *!
 *! @note
 *!
 *! When only simple types like int, floats, strings, mappings,
 *! multisets and arrays are encoded, the produced string is very
 *! portable between pike versions. It can at least be read by any
 *! later version.
 *!
 *! The portability when objects, programs and functions are involved
 *! depends mostly on the codec. If the byte code is encoded, i.e.
 *! when Pike programs are actually dumped in full, then the string
 *! can probably only be read by the same pike version.
 *!
 *! @seealso
 *!   @[decode_value()], @[sprintf()], @[encode_value_canonic()]
 */
void f_encode_value(INT32 args)
{
  ONERROR tmp;
  struct encode_data d, *data;
  data=&d;

  check_all_args("encode_value", args, BIT_MIXED, BIT_VOID | BIT_OBJECT,
#ifdef ENCODE_DEBUG
		 /* This argument is only an internal debug helper.
		  * It's intentionally not part of the function
		  * prototype, to keep the argument position free for
		  * other uses in the future. */
		 BIT_VOID | BIT_INT,
#endif
		 0);

  initialize_buf(&data->buf);
  data->canonic = 0;
  data->encoded=allocate_mapping(128);
  data->counter.type=T_INT;
  data->counter.u.integer=COUNTER_START;
  if(args > 1)
  {
    data->codec=Pike_sp[1-args].u.object;
  }else{
    data->codec=get_master();
  }
#ifdef ENCODE_DEBUG
  data->debug = args > 2 ? Pike_sp[2-args].u.integer : 0;
  data->depth = -2;
#endif

  SET_ONERROR(tmp, free_encode_data, data);
  addstr("\266ke0", 4);
  encode_value2(Pike_sp-args, data);
  UNSET_ONERROR(tmp);

  free_mapping(data->encoded);

  pop_n_elems(args);
  push_string(low_free_buf(&data->buf));
}

/*! @decl string encode_value_canonic(mixed value, object|void codec)
 *!
 *! Code a value into a string on canonical form.
 *!
 *! Takes a value and converts it to a string on canonical form, much like
 *! @[encode_value()]. The canonical form means that if an identical value is
 *! encoded, it will produce exactly the same string again, even if it's
 *! done at a later time and/or in another Pike process. The produced
 *! string is compatible with @[decode_value()].
 *!
 *! @note
 *!   Note that this function is more restrictive than @[encode_value()] with
 *!   respect to the types of values it can encode. It will throw an error
 *!   if it can't encode to a canonical form.
 *!
 *! @seealso
 *!   @[encode_value()], @[decode_value()]
 */
void f_encode_value_canonic(INT32 args)
{
  ONERROR tmp;
  struct encode_data d, *data;
  data=&d;

  check_all_args("encode_value_canonic", args, BIT_MIXED, BIT_VOID | BIT_OBJECT,
#ifdef ENCODE_DEBUG
		 /* This argument is only an internal debug helper.
		  * It's intentionally not part of the function
		  * prototype, to keep the argument position free for
		  * other uses in the future. */
		 BIT_VOID | BIT_INT,
#endif
		 0);

  initialize_buf(&data->buf);
  data->canonic = 1;
  data->encoded=allocate_mapping(128);
  data->counter.type=T_INT;
  data->counter.u.integer=COUNTER_START;
  if(args > 1)
  {
    data->codec=Pike_sp[1-args].u.object;
  }else{
    data->codec=get_master();
  }
#ifdef ENCODE_DEBUG
  data->debug = args > 2 ? Pike_sp[2-args].u.integer : 0;
  data->depth = -2;
#endif

  SET_ONERROR(tmp, free_encode_data, data);
  addstr("\266ke0", 4);
  encode_value2(Pike_sp-args, data);
  UNSET_ONERROR(tmp);

  free_mapping(data->encoded);

  pop_n_elems(args);
  push_string(low_free_buf(&data->buf));
}


struct unfinished_prog_link
{
  struct unfinished_prog_link *next;
  struct program *prog;
};

struct unfinished_obj_link
{
  struct unfinished_obj_link *next;
  struct object *o;
};

struct decode_data
{
  unsigned char *data;
  ptrdiff_t len;
  ptrdiff_t ptr;
  struct mapping *decoded;
  struct unfinished_prog_link *unfinished_programs;
  struct unfinished_obj_link *unfinished_objects;
  struct svalue counter;
  struct object *codec;
  int pickyness;
  int pass;
  struct pike_string *raw;
  struct decode_data *next;
#ifdef PIKE_THREADS
  struct object *thread_id;
#endif
#ifdef ENCODE_DEBUG
  int debug, depth;
#endif
  struct Supporter supporter;
};

static void decode_value2(struct decode_data *data);

static void fallback_codec(void)
{
  size_t x;
  push_constant_text(".");
  f_divide(2);
  f_reverse(1);
  Pike_sp--;
  x=Pike_sp->u.array->size;
  push_array_items(Pike_sp->u.array);
  ref_push_mapping(get_builtin_constants());
  while(x--)
  {
    stack_swap();
    f_arrow(2);
  }
}

static int my_extract_char(struct decode_data *data)
{
  if(data->ptr >= data->len)
    Pike_error("Format error, not enough data in string.\n");
  return data->data [ data->ptr++ ];
}

#define GETC() my_extract_char(data)

#define DECODE(Z) do {					\
  EDB(5,						\
    fprintf(stderr,"%*sdecode(%s) at %d: ",		\
	    data->depth,"",(Z),__LINE__));		\
  what=GETC();						\
  e=what>>SIZE_SHIFT;					\
  numh=0;						\
  if(what & TAG_SMALL) {				\
     num=e;						\
  } else {						\
     INT32 numl;					\
     num=0;						\
     while(e > 4) {					\
       numh = (numh<<8) + (GETC()+1);			\
       e--;						\
     }							\
     while(e-->=0) num=(num<<8) + (GETC()+1);		\
     numl = num + MAX_SMALL - 1;			\
     if (numl < num) numh++;				\
     num = numl;					\
  }							\
  if(what & TAG_NEG) {					\
    num = ~num;						\
    numh = ~numh;					\
  }							\
  EDB(5,						\
    fprintf(stderr,"type=%d (%s), num=%ld\n",	\
	    (what & TAG_MASK),				\
	    get_name_of_type(tag_to_type(what & TAG_MASK)),		\
	    (long)num) ); 					\
} while (0)



#define decode_entry(X,Y,Z)					\
  do {								\
    INT32 what, e, num, numh;					\
    DECODE("decode_entry");					\
    if((what & TAG_MASK) != (X))				\
      Pike_error("Failed to decode, wrong bits (%d).\n", what & TAG_MASK); \
    (Y)=num;							\
  } while(0);

#define getdata2(S,L) do {						\
      if(data->ptr + (ptrdiff_t)(sizeof(S[0])*(L)) > data->len)		\
	Pike_error("Failed to decode string. (string range error)\n");	\
      MEMCPY((S),(data->data + data->ptr), sizeof(S[0])*(L));		\
      data->ptr+=sizeof(S[0])*(L);					\
  }while(0)

#if BYTEORDER == 4123
#define BITFLIP(S)
#else
#define BITFLIP(S)						\
   switch(what)							\
   {								\
     case 1: for(e=0;e<num;e++) STR1(S)[e]=ntohs(STR1(S)[e]); break;	\
     case 2: for(e=0;e<num;e++) STR2(S)[e]=ntohl(STR2(S)[e]); break;    \
   }
#endif

#define get_string_data(STR,LEN, data) do {				    \
  if((LEN) == -1)							    \
  {									    \
    INT32 what, e, num, numh;						    \
    DECODE("get_string_data");						    \
    what &= TAG_MASK;							    \
    if(data->ptr + num > data->len || num <0)				    \
      Pike_error("Failed to decode string. (string range error)\n");		    \
    if(what<0 || what>2)						    \
      Pike_error("Failed to decode string. (Illegal size shift)\n");		    \
    STR=begin_wide_shared_string(num, what);				    \
    MEMCPY(STR->str, data->data + data->ptr, num << what);		    \
    data->ptr+=(num << what);						    \
    BITFLIP(STR);							    \
    STR=end_shared_string(STR);                                             \
  }else{								    \
    if(data->ptr + (LEN) > data->len || (LEN) <0)			    \
      Pike_error("Failed to decode string. (string range error)\n");		    \
    STR=make_shared_binary_string((char *)(data->data + data->ptr), (LEN)); \
    data->ptr+=(LEN);							    \
  }									    \
}while(0)

#define getdata(X) do {				\
   long length;					\
   decode_entry(TAG_STRING, length,data);	\
   if(data->pass == 1)				\
     get_string_data(X, length, data);		\
   else						\
     data->ptr+=length;				\
  }while(0)

#define getdata3(X) do {						     \
  INT32 what, e, num, numh;						     \
  DECODE("getdata3");							     \
  switch(what & TAG_MASK)						     \
  {									     \
    case TAG_INT:							     \
      X=0;								     \
      break;								     \
									     \
    case TAG_STRING:							     \
      get_string_data(X,num,data);                                           \
      break;								     \
									     \
    default:								     \
      Pike_error("Failed to decode string, tag is wrong: %d\n",		     \
            what & TAG_MASK);						     \
    }									     \
}while(0)

#define decode_number(X,data) do {	\
   INT32 what, e, num, numh;		\
   DECODE("decode_number");			\
   X=(what & TAG_MASK) | (num<<4);		\
   EDB(5, fprintf(stderr, "%*s  ==>%d\n",	\
                  data->depth, "", X));		\
  }while(0)					\


static void restore_type_stack(struct pike_type **old_stackp)
{
#if 0
  fprintf(stderr, "Restoring type-stack: %p => %p\n",
	  Pike_compiler->type_stackp, old_stackp);
#endif /* 0 */
#ifdef PIKE_DEBUG
  if (old_stackp > Pike_compiler->type_stackp) {
    Pike_fatal("type stack out of sync!\n");
  }
#endif /* PIKE_DEBUG */
  while(Pike_compiler->type_stackp > old_stackp) {
    free_type(*(Pike_compiler->type_stackp--));
  }
}

static void restore_type_mark(struct pike_type ***old_type_mark_stackp)
{
#if 0
  fprintf(stderr, "Restoring type-mark: %p => %p\n",
	  Pike_compiler->pike_type_mark_stackp, old_type_mark_stackp);
#endif /* 0 */
#ifdef PIKE_DEBUG
  if (old_type_mark_stackp > Pike_compiler->pike_type_mark_stackp) {
    Pike_fatal("type Pike_interpreter.mark_stack out of sync!\n");
  }
#endif /* PIKE_DEBUG */
  Pike_compiler->pike_type_mark_stackp = old_type_mark_stackp;
}

static void low_decode_type(struct decode_data *data)
{
  /* FIXME: Probably ought to use the tag encodings too. */

  int tmp;
  ONERROR err1;
  ONERROR err2;

  SET_ONERROR(err1, restore_type_stack, Pike_compiler->type_stackp);
  SET_ONERROR(err2, restore_type_mark, Pike_compiler->pike_type_mark_stackp);

  tmp = GETC();
  switch(tmp)
  {
    default:
      Pike_error("decode_value(): Error in type string (%d).\n", tmp);
      /*NOTREACHED*/
      break;

    case T_ASSIGN:
      tmp = GETC();
      if ((tmp < '0') || (tmp > '9')) {
	Pike_error("decode_value(): Bad marker in type string (%d).\n", tmp);
      }
      low_decode_type(data);
      push_assign_type(tmp);	/* Actually reverse, but they're the same */
      break;

    case T_FUNCTION:
      {
	int narg = 0;

	while (GETC() != T_MANY) {
	  data->ptr--;
	  low_decode_type(data);
	  narg++;
	}
	low_decode_type(data);	/* Many */
	low_decode_type(data);	/* Return */
	push_reverse_type(T_MANY);
	while(narg-- > 0) {
	  push_reverse_type(T_FUNCTION);
	}
      }
      break;

    case T_MAPPING:
    case T_OR:
    case T_AND:
      low_decode_type(data);
      low_decode_type(data);
      push_reverse_type(tmp);
      break;

    case T_TYPE:
    case T_PROGRAM:
    case T_ARRAY:
    case T_MULTISET:
    case T_NOT:
      low_decode_type(data);
      push_type(tmp);
      break;

    case T_INT:
      {
	INT32 min=0, max=0;
	min = GETC();
	min = (min<<8)|GETC();
	min = (min<<8)|GETC();
	min = (min<<8)|GETC();
	max = GETC();
	max = (max<<8)|GETC();
	max = (max<<8)|GETC();
	max = (max<<8)|GETC();
	push_int_type(min, max);
      }
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
    case T_ZERO:
    case T_VOID:
    case PIKE_T_UNKNOWN:
      push_type(tmp);
      break;

    case PIKE_T_NAME:
      decode_value2(data);

      if (Pike_sp[-1].type != PIKE_T_STRING) {
	Pike_error("decode_value(): Type name is not a string (%s)\n",
		   get_name_of_type(Pike_sp[-1].type));
      }
      low_decode_type(data);
      push_type_name(Pike_sp[-1].u.string);
      pop_stack();
      break;

    case T_OBJECT:
    {
      int flag = GETC();

      decode_value2(data);
      switch(Pike_sp[-1].type)
      {
	case T_INT:
	  push_object_type_backwards(flag, Pike_sp[-1].u.integer );
	  break;

	case T_PROGRAM:
	  push_object_type_backwards(flag, Pike_sp[-1].u.program->id);
	  break;

        case T_FUNCTION:
	  {
	    struct program *prog;
	    if (Pike_sp[-1].subtype == FUNCTION_BUILTIN) {
	      Pike_error("Failed to decode object type.\n");
	    }
	    prog = program_from_svalue(Pike_sp-1);
	    if (!prog) {
	      Pike_error("Failed to decode object type.\n");
	    }
	    push_object_type_backwards(flag, prog->id);
	  }
	  break;

	default:
	  Pike_error("Failed to decode type "
		"(object(%s), expected object(zero|program)).\n",
		get_name_of_type(Pike_sp[-1].type));
      }
      pop_stack();
    }
  }

  UNSET_ONERROR(err2);
  UNSET_ONERROR(err1);
}


static void zap_placeholder(struct object *placeholder)
{
  /* fprintf(stderr, "Destructing placeholder.\n"); */
  if (placeholder->storage) {
    debug_malloc_touch(placeholder);
    destruct(placeholder);
  } else {
    free_program(placeholder->prog);
    placeholder->prog = NULL;
    debug_malloc_touch(placeholder);
  }
  free_object(placeholder);
}

static int init_placeholder(struct object *placeholder);


#define SETUP_DECODE_MEMOBJ(TYPE, U, VAR, ALLOCATE,SCOUR) do {		\
  struct svalue *tmpptr;						\
  if(data->pass > 1 &&							\
     (tmpptr=low_mapping_lookup(data->decoded, & data->counter)))	\
  {									\
    tmp=*tmpptr;							\
    VAR=tmp.u.U;							\
    SCOUR;                                                              \
  }else{								\
    tmp.type=TYPE;							\
    tmp.u.U=VAR=ALLOCATE;						\
    mapping_insert(data->decoded, & data->counter, &tmp);		\
  /* Since a reference to the object is stored in the mapping, we can	\
   * safely decrease this reference here. Thus it will be automatically	\
   * freed if something goes wrong.					\
   */									\
    VAR->refs--;							\
  }									\
  data->counter.u.integer++;						\
}while(0)

/* This really needs to disable threads.... */
#define decode_type(X,data)  do {		\
  type_stack_mark();				\
  low_decode_type(data);			\
  (X)=pop_unfinished_type();			\
} while(0)

static void decode_value2(struct decode_data *data)

#ifdef PIKE_DEBUG
#undef decode_value2
#define decode_value2(X) do { struct svalue *_=Pike_sp; decode_value2_(X); if(Pike_sp!=_+1) Pike_fatal("decode_value2 failed!\n"); } while(0)
#endif


{
  INT32 what, e, num, numh;
  struct svalue tmp, *tmp2;

#ifdef ENCODE_DEBUG
  data->depth += 2;
#endif

  DECODE("decode_value2");

#ifdef ENCODE_DEBUG
  if(data->debug)
  {
    if((what & TAG_MASK ) == TAG_AGAIN)
      fprintf(stderr, "%*sDecoding TAG_AGAIN from <%d>\n",
	      data->depth, "", num);
    
    else
      if(data->debug > 1)
	fprintf(stderr, "%*sDecoding to <%d>: TAG%d (%d)\n",
		data->depth, "", data->counter.u.integer ,
		what & TAG_MASK, num);
  }
#endif

  check_stack(1);

  switch(what & TAG_MASK)
  {
    case TAG_AGAIN:
      tmp.type=T_INT;
      tmp.subtype=0;
      tmp.u.integer=num;
      if((tmp2=low_mapping_lookup(data->decoded, &tmp)))
      {
	push_svalue(tmp2);
      }else{
	Pike_error("Failed to decode TAG_AGAIN entry <%d>.\n", num);
      }
#ifdef ENCODE_DEBUG
      data->depth -= 2;
#endif
      return;

    case TAG_INT:
      tmp.type = T_INT;
      tmp=data->counter;
      data->counter.u.integer++;
      push_int(num);
      break;

    case TAG_STRING:
    {
      struct pike_string *str;
      tmp.type = T_INT;
      tmp=data->counter;
      data->counter.u.integer++;
      get_string_data(str, num, data);
      push_string(str);
      break;
    }

    case TAG_FLOAT:
    {
      double res;

      EDB(2,fprintf(stderr, "Decoding float... numh:0x%08x, num:0x%08x\n",
		  numh, num));

      res = LDEXP((double)numh, 32) + (double)(unsigned INT32)num;

      EDB(2,fprintf(stderr, "Mantissa: %10g\n", res));

      tmp = data->counter;
      data->counter.u.integer++;

      DECODE("float");

      EDB(2,fprintf(stderr, "Exponent: %d\n", num));

      if(!res)
      {
	DECLARE_INF
	DECLARE_NAN

	switch(num)
	{
	  case Pike_FP_SNAN: /* Signal Not A Number */
	  case Pike_FP_QNAN: /* Quiet Not A Number */
	    push_float(DO_NOT_WARN((FLOAT_TYPE)MAKE_NAN()));
	    break;
		       
	  case Pike_FP_NINF: /* Negative infinity */
	    push_float(DO_NOT_WARN((FLOAT_TYPE)MAKE_INF(-1)));
	    break;

	  case Pike_FP_PINF: /* Positive infinity */
	    push_float(DO_NOT_WARN((FLOAT_TYPE)MAKE_INF(1)));
	    break;

	  case Pike_FP_NZERO: /* Negative Zero */
	    push_float(-0.0); /* Does this do what we want? */
	    break;

	  default:
	    push_float(DO_NOT_WARN((FLOAT_TYPE)LDEXP(res, num)));
	    break;
	}
	break;
      }

      push_float(DO_NOT_WARN((FLOAT_TYPE)LDEXP(res, num)));
      break;
    }

    case TAG_TYPE:
    {
      struct pike_type *t;

      decode_type(t, data);
      check_type_string(t);
      push_type_value(t);

      tmp.type = T_INT;
      tmp = data->counter;
      data->counter.u.integer++;
    }
    break;

    case TAG_ARRAY:
    {
      struct array *a;
      if(num < 0)
	Pike_error("Failed to decode array. (array size is negative)\n");

      /* Heruetical */
      if(data->ptr + num > data->len)
	Pike_error("Failed to decode array. (not enough data)\n");

      EDB(2,fprintf(stderr, "%*sDecoding array of size %d to <%d>\n",
		  data->depth, "", num, data->counter.u.integer));

      SETUP_DECODE_MEMOBJ(T_ARRAY, array, a, allocate_array(num),
			  free_svalues(ITEM(a), a->size, a->type_field));

      for(e=0;e<num;e++)
      {
	decode_value2(data);
	ITEM(a)[e]=Pike_sp[-1];
	Pike_sp--;
	dmalloc_touch_svalue(Pike_sp);
      }
      ref_push_array(a);
#ifdef ENCODE_DEBUG
      data->depth -= 2;
#endif
      return;
    }

    case TAG_MAPPING:
    {
      struct mapping *m;
      if(num<0)
	Pike_error("Failed to decode mapping. (mapping size is negative)\n");

      /* Heruetical */
      if(data->ptr + num > data->len)
	Pike_error("Failed to decode mapping. (not enough data)\n");

      EDB(2,fprintf(stderr, "%*sDecoding mapping of size %d to <%d>\n",
		  data->depth, "", num, data->counter.u.integer));

      SETUP_DECODE_MEMOBJ(T_MAPPING, mapping, m, allocate_mapping(num), ; );

      for(e=0;e<num;e++)
      {
	decode_value2(data);
	decode_value2(data);
	mapping_insert(m, Pike_sp-2, Pike_sp-1);
	pop_n_elems(2);
      }
      ref_push_mapping(m);
#ifdef ENCODE_DEBUG
      data->depth -= 2;
#endif
      return;
    }

    case TAG_MULTISET:
    {
      struct multiset *m;
      struct array *a;
      if(num<0)
	Pike_error("Failed to decode multiset. (multiset size is negative)\n");

      /* Heruetical */
      if(data->ptr + num > data->len)
	Pike_error("Failed to decode multiset. (not enough data)\n");

      /* NOTE: This code knows stuff about the implementation of multisets...*/

      EDB(2,fprintf(stderr, "%*sDecoding multiset of size %d to <%d>\n",
		  data->depth, "", num, data->counter.u.integer));
#ifdef PIKE_NEW_MULTISETS
      SETUP_DECODE_MEMOBJ (T_MULTISET, multiset, m,
			   allocate_multiset (0, 0, NULL), ;);
      /* FIXME: This array could be avoided by building the multiset directly. */
      a = low_allocate_array (num, 0);
#else
      SETUP_DECODE_MEMOBJ(T_MULTISET, multiset, m,
			  allocate_multiset(low_allocate_array(num, 0)), ;);
      a=m->ind;
#endif

      for(e=0;e<num;e++)
      {
	decode_value2(data);
	assign_svalue(a->item+e , Pike_sp-1);
	pop_stack();
	dmalloc_touch_svalue(Pike_sp);
      }
      array_fix_type_field(a);
#ifdef PIKE_NEW_MULTISETS
      {
	struct multiset *l = mkmultiset (a);
	free_array (a);
	/* This special case is handled efficiently by merge_multisets. */
	merge_multisets (m, l, PIKE_MERGE_DESTR_A | PIKE_ARRAY_OP_ADD);
	free_multiset (l);
      }
#else
      order_multiset(m);
#endif
      ref_push_multiset(m);
#ifdef ENCODE_DEBUG
      data->depth -= 2;
#endif
      return;
    }

    case TAG_OBJECT:
    {
      tmp=data->counter;
      data->counter.u.integer++;

      decode_value2(data);

      switch(num)
      {
	case 0:
	  if(data->codec)
	  {
	    apply(data->codec,"objectof", 1);
	  }else{
	    fallback_codec();
	  }
	  break;

	case 1:
	  if(UNSAFE_IS_ZERO(Pike_sp-1))
	  {
	    EDB(1,fprintf(stderr, "%*sDecoded a failed object to <%d>: ",
			data->depth, "", tmp.u.integer);
		print_svalue(stderr, Pike_sp-1);
		fputc('\n', stderr););
	    decode_value2(data);
	    pop_stack();
	  }else{
	    int fun;
	    struct object *o;
	    /* decode_value_clone_object does not call __INIT, so
	     * we want to do that ourselves...
	     */
	    o=decode_value_clone_object(Pike_sp-1);
	    debug_malloc_touch(o);
	    pop_stack();
	    push_object(o);

	    if(o->prog)
	    {
	      if(o->prog->flags & PROGRAM_FINISHED)
	      {
		apply_lfun(o, LFUN___INIT, 0);
		pop_stack();
		/* FIXME: Should call LFUN_CREATE here in <= 7.2
		 * compatibility mode. */
	      }else{
		struct unfinished_obj_link *ol=ALLOC_STRUCT(unfinished_obj_link);
		ol->o=o;
		ol->next=data->unfinished_objects;
		data->unfinished_objects=ol;
	      }
	    }

	    EDB(2,fprintf(stderr, "%*sDecoded an object to <%d>: ",
			data->depth, "", tmp.u.integer);
		print_svalue(stderr, Pike_sp-1);
		fputc('\n', stderr););

	    ref_push_object(o);
	    decode_value2(data);
	    if(!data->codec)
	      Pike_error("Failed to decode object (no codec)\n");

	    fun = find_identifier("decode_object", data->codec->prog);
	    if (fun < 0)
	      Pike_error("Cannot decode objects without a "
			 "\"decode_object\" function in the codec.\n");
	    apply_low(data->codec,fun,2);
	    pop_stack();
	  }

	  break;

#ifdef AUTO_BIGNUM
	  /* It is possible that we should do this even without
	   * AUTO_BIGNUM /Hubbe
	   * However, that requires that some of the bignum functions
	   * are always available...
	   */
	case 2:
	{
	  check_stack(2);
	  /* 256 would be better, but then negative numbers
	   * doesn't work... /Hubbe
	   */
	  push_int(36);
	  convert_stack_top_with_base_to_bignum();
	  break;
	}

#endif
	case 3:
	  pop_stack();
	  decode_value2(data);
	  break;

	default:
	  Pike_error("Object coding not compatible.\n");
	  break;
      }
      if(Pike_sp[-1].type != T_OBJECT)
	if(data->pickyness)
	  Pike_error("Failed to decode object.\n");

      break;
    }

    case TAG_FUNCTION:
      tmp=data->counter;
      data->counter.u.integer++;
      decode_value2(data);

      switch(num)
      {
	case 0:
	  if(data->codec)
	  {
	    apply(data->codec,"functionof", 1);
	  }else{
	    fallback_codec();
	  }
	  break;

	case 1: {
	  struct program *p;
	  decode_value2(data);
	  if (Pike_sp[-2].type == T_OBJECT &&
	      Pike_sp[-1].type == T_STRING &&
	      (p = Pike_sp[-2].u.object->prog)) {
	    int f = find_shared_string_identifier(Pike_sp[-1].u.string, p);
	    if (f >= 0) {
	      struct svalue func;
	      low_object_index_no_free(&func, Pike_sp[-2].u.object, f);
#ifdef PIKE_SECURITY
	      /* FIXME: Check access to the function. */
#endif
	      pop_n_elems(2);
	      *Pike_sp++ = func;
	      break;
	    }
	  }
	  pop_stack();
	  break;
	}

	default:
	  Pike_error("Function coding not compatible.\n");
	  break;
      }
      if(data->pickyness && Pike_sp[-1].type != T_FUNCTION)
	Pike_error("Failed to decode function.\n");
      break;


    case TAG_PROGRAM:
      EDB(3,
	  fprintf(stderr, "%*s  TAG_PROGRAM(%d)\n",
		  data->depth, "", num));
      switch(num)
      {
	case 0:
	{
	  struct svalue *prog_code;
	  struct program *p;

	  tmp=data->counter;
	  data->counter.u.integer++;
	  decode_value2(data);

	  /* Keep the value so that we can make a good error-message. */
	  prog_code = Pike_sp-1;
	  stack_dup();

	  if(data->codec)
	  {
	    apply(data->codec,"programof", 1);
	  }else{
	    fallback_codec();
	  }

	  p = program_from_svalue(Pike_sp-1);

	  if (!p) {
	    if(data->pickyness) {
	      if ((prog_code->type == T_STRING) &&
		  (prog_code->u.string->len < 128) &&
		  (!prog_code->u.string->size_shift)) {
		Pike_error("Failed to decode program \"%s\".\n",
			   prog_code->u.string->str);
	      }
	      Pike_error("Failed to decode program.\n");
	    }
	    pop_n_elems(2);
	    push_undefined();
	    break;
	  }
	  /* Remove the extra entry from the stack. */
	  ref_push_program(p);
	  stack_pop_n_elems_keep_top(2);
	  break;
	}

	case 1:
	{
	  int d, in;
	  size_t size=0;
	  char *dat=0;
	  struct program *p;
	  struct object *placeholder=0;
	  ONERROR err1, err2, err3, err4;

#ifdef _REENTRANT
	  ONERROR err;
	  low_init_threads_disable();
	  SET_ONERROR(err, do_enable_threads, 0);
#endif

	  EDB(2,fprintf(stderr, "%*sDecoding a program to <%d>: ",
		      data->depth, "", data->counter.u.integer);
	      print_svalue(stderr, &tmp);
	      fputc('\n', stderr););

	  SETUP_DECODE_MEMOBJ(T_PROGRAM, program, p, low_allocate_program(),;);

	  SET_ONERROR(err3, zap_unfinished_program, p);
	  
	  if(data->pass == 1)
	  {
	    if(! data->supporter.prog)
	      data->supporter.prog = p;

	    debug_malloc_touch(p);
	    if(data->codec)
	    {
	      ref_push_program(p);
	      apply(data->codec, "__register_new_program", 1);
	      
	      /* return a placeholder */
	      if(Pike_sp[-1].type == T_OBJECT)
	      {
		placeholder=Pike_sp[-1].u.object;
		if(placeholder->prog != null_program)
		  Pike_error("Placeholder object is not a null_program clone!\n");
		Pike_sp--;
	      }else{
		pop_stack();
	      }
	    }
	  }

	  if(placeholder)
	    SET_ONERROR(err4, zap_placeholder, placeholder);

	  decode_value2(data);
	  f_version(0);
	  if(!is_eq(Pike_sp-1,Pike_sp-2))
	    Pike_error("Cannot decode programs encoded with other pike version.\n");
	  pop_n_elems(2);

	  debug_malloc_touch(p);
	  decode_number(p->flags,data);

	  if(data->pass == 1)
	  {
	    p->flags &= ~(PROGRAM_FINISHED | PROGRAM_OPTIMIZED |
			  PROGRAM_FIXED | PROGRAM_PASS_1_DONE);
	    p->flags |= PROGRAM_AVOID_CHECK;
	  }
	  decode_number(p->storage_needed,data);
	  decode_number(p->xstorage,data);
	  decode_number(p->parent_info_storage,data);
	  decode_number(p->alignment_needed,data);
	  decode_number(p->timestamp.tv_sec,data);
	  decode_number(p->timestamp.tv_usec,data);

	  if(data->pass && p->parent)
	  {
	    free_program(p->parent);
	    p->parent=0;
	  }

	  debug_malloc_touch(p);
	  decode_value2(data);
	  switch(Pike_sp[-1].type)
	  {
	    case T_INT:
	      p->parent=0;
	      break;
	    case T_PROGRAM:
	      p->parent=Pike_sp[-1].u.program;
	      break;
	    case T_FUNCTION:
	      p->parent=program_from_svalue(Pike_sp-1);
	      break;
	    default:
	      Pike_error("Program decode failed!\n");
	  }
	  if(p->parent) {
	    add_ref(p->parent);
	  }
	  pop_stack();

	  debug_malloc_touch(p);

#define FOO(X,Y,Z) \
	  decode_number( p->num_##Z, data);
#include "program_areas.h"


	  if(data->pass == 1)
	  {
#define FOO(NUMTYPE,TYPE,NAME) \
          size=DO_ALIGN(size, ALIGNOF(TYPE)); \
          size+=p->PIKE_CONCAT(num_,NAME)*sizeof(p->NAME[0]);
#include "program_areas.h"

	    dat=xalloc(size);
	    debug_malloc_touch(dat);
	    MEMSET(dat,0,size);
	    size=0;
#define FOO(NUMTYPE,TYPE,NAME) \
	  size=DO_ALIGN(size, ALIGNOF(TYPE)); \
          p->NAME=(TYPE *)(dat+size); \
          size+=p->PIKE_CONCAT(num_,NAME)*sizeof(p->NAME[0]);
#include "program_areas.h"

	    for(e=0;e<p->num_constants;e++)
	      p->constants[e].sval.type=T_INT;

	    debug_malloc_touch(dat);
	    debug_malloc_touch(p);
	    
	    p->total_size=size + sizeof(struct program);
	    
	    p->flags |= PROGRAM_OPTIMIZED;
	  }

	  {
	    INT32 bytecode_method = 0;
	    decode_number(bytecode_method, data);
	    if (bytecode_method != PIKE_BYTECODE_METHOD) {
	      Pike_error("Unsupported bytecode method: %d. Expected %d\n",
			 bytecode_method, PIKE_BYTECODE_METHOD);
	    }
	  }

	  getdata2(p->program, p->num_program);
	  getdata2(p->relocations, p->num_relocations);

#ifdef DECODE_PROGRAM
	  {
	    int byteorder = PIKE_BYTEORDER;	/* FIXME: Used by bytecode.h */
	    DECODE_PROGRAM(p);
	  }
#endif /* DECODE_PROGRAM */
	  make_program_executable(p);

	  getdata2(p->linenumbers, p->num_linenumbers);

#ifdef DEBUG_MALLOC
	  if(p->num_linenumbers && p->linenumbers &&
	     EXTRACT_UCHAR(p->linenumbers)==127)
	  {
	    char *foo = p->linenumbers + 1;
	    int len = get_small_number(&foo);
	    int shift = *foo;
	    char *fname = ++foo;
	    foo += len << shift;
	    get_small_number(&foo); /* pc offset */
	    /* FIXME: Dmalloc doesn't support wide filenames. */
	    debug_malloc_name(p, fname, get_small_number(&foo));
	  }
#endif


	  debug_malloc_touch(p);
	  for(d=0;d<p->num_identifier_index;d++)
	  {
	    decode_number(p->identifier_index[d],data);
	    if(p->identifier_index[d] > p->num_identifier_references)
	    {
	      p->identifier_index[d]=0;
	      Pike_error("Malformed program in decode.\n");
	    }
	  }

	  debug_malloc_touch(p);
	  for(d=0;d<p->num_variable_index;d++)
	  {
	    decode_number(p->variable_index[d],data);
	    if(p->variable_index[d] > p->num_identifiers)
	    {
	      p->variable_index[d]=0;
	      Pike_error("Malformed program in decode.\n");
	    }
	  }

	  debug_malloc_touch(p);
	  for(d=0;d<p->num_identifier_references;d++)
	  {
	    decode_number(p->identifier_references[d].inherit_offset,data);
	    if(p->identifier_references[d].inherit_offset > p->num_inherits)
	    {
	      p->identifier_references[d].inherit_offset=0;
	      Pike_error("Malformed program in decode.\n");
	    }
	    decode_number(p->identifier_references[d].identifier_offset,data);
	    decode_number(p->identifier_references[d].id_flags,data);

	    EDB(3,fprintf(stderr,"IDREF%x < %d: { %d, %d, %d }\n",
			p->id,d,
			p->identifier_references[d].inherit_offset,
			p->identifier_references[d].identifier_offset,
			p->identifier_references[d].id_flags); );
	  }

	  debug_malloc_touch(p);
	  for(d=0;d<p->num_strings;d++)
	    getdata(p->strings[d]);

	  debug_malloc_touch(p);
	  debug_malloc_touch(dat);

	  data->pickyness++;


/*	  p->inherits[0].prog=p;
	  p->inherits[0].parent_offset=1;
*/

	  if(placeholder && data->pass==1)
	  {
	    if(placeholder->prog != null_program)
	    {
	      debug_malloc_touch(placeholder);
	      Pike_error("Placeholder argument is not a null_program clone!");
	    }else{
	      free_program(placeholder->prog);
	      add_ref(placeholder->prog = p);
	      debug_malloc_touch(placeholder);
	    }
	  }

	  debug_malloc_touch(p);

	  in=p->num_inherits;
	  for(d=0;d<in;d++)
	  {
	    decode_number(p->inherits[d].inherit_level,data);
	    decode_number(p->inherits[d].identifier_level,data);
	    decode_number(p->inherits[d].parent_offset,data);
	    decode_number(p->inherits[d].parent_identifier,data);
	    decode_number(p->inherits[d].storage_offset,data);

	    decode_value2(data);
	    if(d==0)
	    {
	      if(Pike_sp[-1].type != T_PROGRAM ||
		 Pike_sp[-1].u.program != p)
		Pike_error("Program decode failed!\n");
	      p->refs--;
	    }

	    if(data->pass > 1)
	    {
	      if(p->inherits[d].prog)
	      {
		free_program(p->inherits[d].prog);
		p->inherits[d].prog=0;
	      }

	      if(p->inherits[d].parent)
	      {
		free_object(p->inherits[d].parent);
		p->inherits[d].parent=0;
	      }
	    }

	    switch(Pike_sp[-1].type)
	    {
	      case T_FUNCTION:
		if(Pike_sp[-1].subtype == FUNCTION_BUILTIN)
		  Pike_error("Failed to decode parent.\n");

		EDB(3, fprintf(stderr,"INHERIT%x = func { %p, %d} \n",p->id,Pike_sp[-1].u.object, Pike_sp[-1].subtype); );

		p->inherits[d].parent_identifier=Pike_sp[-1].subtype;
		p->inherits[d].prog=program_from_svalue(Pike_sp-1);
		if(!p->inherits[d].prog)
		  Pike_error("Failed to decode parent.\n");
		add_ref(p->inherits[d].prog);
		p->inherits[d].parent=Pike_sp[-1].u.object;
		Pike_sp--;
		dmalloc_touch_svalue(Pike_sp);
		break;

	      case T_PROGRAM:
		EDB(3, fprintf(stderr,"INHERIT%x = prog\n",p->id); );
		p->inherits[d].prog=Pike_sp[-1].u.program;
		Pike_sp--;
		dmalloc_touch_svalue(Pike_sp);
		break;
	      default:
		Pike_error("Failed to decode inheritance.\n");
	    }

	    p->num_inherits=d+1;

	    getdata3(p->inherits[d].name);

	    EDB(3, fprintf(stderr,"INHERIT%x < %d: %d id=%d\n",
			 p->id,d,
			 p->inherits[d].prog->num_identifiers,
			 p->inherits[d].prog->id); );
	  }

	  debug_malloc_touch(dat);


	  SET_ONERROR(err1, restore_type_stack, Pike_compiler->type_stackp);
	  SET_ONERROR(err2, restore_type_mark, Pike_compiler->pike_type_mark_stackp);

	  debug_malloc_touch(p);
	  for(d=0;d<p->num_identifiers;d++)
	  {
	    getdata(p->identifiers[d].name);
	    decode_type(p->identifiers[d].type,data);
	    decode_number(p->identifiers[d].identifier_flags,data);
	    decode_number(p->identifiers[d].run_time_type,data);
	    decode_number(p->identifiers[d].opt_flags,data);
	    if (!(p->identifiers[d].identifier_flags & IDENTIFIER_C_FUNCTION))
	    {
	      decode_number(p->identifiers[d].func.offset,data);
	    } else {
	      Pike_error("Cannot decode functions implemented in C "
			 "(identifier='%s').\n",
			 p->identifiers[d].name->str);
	    }
	  }


	  UNSET_ONERROR(err2);
	  UNSET_ONERROR(err1);

	  debug_malloc_touch(dat);

	  debug_malloc_touch(p);
	  for(d=0;d<NUM_LFUNS;d++)
	    decode_number(p->lfuns[d],data);

	  debug_malloc_touch(dat);

	  debug_malloc_touch(p);
	  {
	    struct program *new_program_save=Pike_compiler->new_program;
	    Pike_compiler->new_program=p;
	    fsort((void *)p->identifier_index,
		  p->num_identifier_index,
		  sizeof(unsigned short),(fsortfun)program_function_index_compare);
	    Pike_compiler->new_program=new_program_save;
	  }

	  debug_malloc_touch(dat);
	  debug_malloc_touch(p);

	  p->flags |= PROGRAM_PASS_1_DONE | PROGRAM_FIXED;
	  for(d=0;d<p->num_constants;d++)
	  {
	    decode_value2(data);
	    if(data->pass > 1)
	    {
	      assign_svalue(& p->constants[d].sval , Pike_sp -1 );
	      pop_stack();
	    }else{
	      p->constants[d].sval=*--Pike_sp;
	    }
	    dmalloc_touch_svalue(Pike_sp);
	    getdata3(p->constants[d].name);
	  }

#ifdef PIKE_DEBUG	  
	  {
	    int q;
	    for(q=0;q<p->num_inherits;q++)
	      if(!p->inherits[q].prog)
		Pike_fatal("FOOBAR!@!!!\n");
	  }
#endif

	  if(placeholder && data->pass == 1)
	  {
	    if(!p || (placeholder->storage))
	    {
	      Pike_error("Placeholder already has storage!\n");
	    } else {
	      placeholder->storage=p->storage_needed ?
		(char *)xalloc(p->storage_needed) :
		(char *)0;
	      call_c_initializers(placeholder);
	    }
	  }
	  
	  data->pickyness--;

	  if(placeholder)
	  {
	    free_object(placeholder);
	    UNSET_ONERROR(err4);
	  }
	  UNSET_ONERROR(err3);

	  ref_push_program(p);

	  if(!(p->flags & PROGRAM_FINISHED) &&
	     !data->supporter.depends_on)
	  {
	    /* Logic for the PROGRAM_FINISHED flag:
	     * The purpose of this code is to make sure that the PROGRAM_FINISHED
	     * flat is not set on the program until all inherited programs also
	     * have that flag. -Hubbe
	     */
	    for(d=1;d<p->num_inherits;d++)
	      if(! (p->inherits[d].prog->flags & PROGRAM_FINISHED))
		break;
	    
	    if(d == p->num_inherits)
	    {
	      p->flags &=~ PROGRAM_AVOID_CHECK;
	      p->flags |= PROGRAM_FINISHED;

	      if (placeholder)
	      {
		if(!init_placeholder(placeholder))
		  placeholder=0;
	      }
	      
	      /* Go through the linked list of unfinished programs
	       * to see what programs are now finished.
	       */
	      {
		struct unfinished_prog_link *l, **ptr;
		
#ifdef PIKE_DEBUG
		check_program(p);
#endif /* PIKE_DEBUG */
		
		/* It is possible that we need to restart loop
		 * in some cases... /Hubbe
		 */
		for(ptr= &data->unfinished_programs ; (l=*ptr);)
		{
		  struct program *pp=l->prog;
		  for(d=1;d<pp->num_inherits;d++)
		    if(! (pp->inherits[d].prog->flags & PROGRAM_FINISHED))
		      break;
		  
		  if(d == pp->num_inherits)
		  {
		    pp->flags &=~ PROGRAM_AVOID_CHECK;
		    pp->flags |= PROGRAM_FINISHED;
		    
#ifdef PIKE_DEBUG
		    check_program(pp);
#endif /* PIKE_DEBUG */
		    
		    *ptr = l->next;
		    free((char *)l);
		  }else{
		    ptr=&l->next;
		  }
		}
	      }
	      
	      /* Go through the linked list of unfinished objects
	       * to see what objects are now finished.
	       */
	      {
		struct unfinished_obj_link *l, **ptr;
		for(ptr= &data->unfinished_objects ; (l=*ptr);)
		{
		  struct object *o=l->o;
		  if(o->prog)
		  {
		    if(o->prog->flags & PROGRAM_FINISHED)
		    {
		      apply_lfun(o, LFUN___INIT, 0);
		      pop_stack();
		      /* FIXME: Should call LFUN_CREATE here in <= 7.2
		       * compatibility mode. */
		    }else{
		      ptr=&l->next;
		      continue;
		    }
		  }
		  *ptr = l->next;
		  free((char *)l);
		}
	      }
	    }else{
	      struct unfinished_prog_link *l;
	      l=ALLOC_STRUCT(unfinished_prog_link);
	      l->prog=p;
	      l->next=data->unfinished_programs;
	      data->unfinished_programs=l;
	    }
	  }

#ifdef _REENTRANT
	  UNSET_ONERROR(err);
	  exit_threads_disable(NULL);
#endif
#ifdef ENCODE_DEBUG
	  data->depth -= 2;
#endif
	  return;
	}

	case 2:
	  tmp=data->counter;
	  data->counter.u.integer++;
	  decode_value2(data);
	  decode_value2(data);
	  if(Pike_sp[-2].type==T_INT)
	  {
	    pop_stack();
	  }else{
	    f_arrow(2);
	  }
	  if(data->pickyness && Pike_sp[-1].type != T_PROGRAM)
	    Pike_error("Failed to decode program.\n");
	  break;

        case 3:
	  tmp=data->counter;
	  data->counter.u.integer++;
	  decode_value2(data);
	  if ((Pike_sp[-1].type == T_INT) &&
	      (Pike_sp[-1].u.integer < PROG_DYNAMIC_ID_START) &&
	      (Pike_sp[-1].u.integer > 0)) {
	    struct program *p = id_to_program(Pike_sp[-1].u.integer);
	    if (!p) {
	      Pike_error("Failed to decode program %"PRINTPIKEINT"d\n",
			 Pike_sp[-1].u.integer);
	    }
	    pop_stack();
	    ref_push_program(p);
	  } else {
	    Pike_error("Failed to decode program.\n");
	  }
	  break;

        case 4:
	{
	  struct program *p;
	  ONERROR err;
	  int byteorder;
	  int bytecode_method;
	  int entry_type;
	  INT16 id_flags;
	  INT16 p_flags;
#define FOO(NUMTYPE,Y,NAME)   \
          NUMTYPE PIKE_CONCAT(local_num_, NAME) = 0;
#include "program_areas.h"

#ifdef ENCODE_DEBUG
	  data->depth += 2;
#endif

	  /* Decode byte-order. */
	  decode_number(byteorder, data);

	  EDB(4,
	      fprintf(stderr, "%*sByte order:%d\n",
		      data->depth, "", byteorder));

	  if ((byteorder != PIKE_BYTEORDER)
#if (PIKE_BYTEORDER == 1234)
	      && (byteorder != 4321)
#else
#if (PIKE_BYTEORDER == 4321)
	      && (byteorder != 1234)
#endif
#endif
	      ) {
	    Pike_error("Unsupported byte-order. Native:%d Encoded:%d\n",
		       PIKE_BYTEORDER, byteorder);
	  }

	  /* Decode flags. */
	  decode_number(p_flags,data);
	  p_flags &= ~(PROGRAM_FINISHED | PROGRAM_OPTIMIZED |
		       PROGRAM_FIXED | PROGRAM_PASS_1_DONE);
	  p_flags |= PROGRAM_AVOID_CHECK;

	  /* Start the new program. */
	  low_start_new_program(NULL, NULL, 0, NULL);
	  p = Pike_compiler->new_program;

	  p->flags = p_flags;

	  /* Kludge to get end_first_pass() to free the program. */
	  Pike_compiler->num_parse_error++;

	  SET_ONERROR(err, end_first_pass, 0);

	  debug_malloc_touch(p);

	  tmp.type=T_PROGRAM;
	  tmp.u.program=p;
	  EDB(2,fprintf(stderr, "%*sDecoding a program to <%d>: ",
			data->depth, "", data->counter.u.integer);
	      print_svalue(stderr, &tmp);
	      fputc('\n', stderr););
	  mapping_insert(data->decoded, & data->counter, &tmp);
	  tmp = data->counter;
	  data->counter.u.integer++;

	  debug_malloc_touch(p);

	  /* Check the version. */
	  decode_value2(data);
	  f_version(0);
	  if(!is_eq(Pike_sp-1,Pike_sp-2))
	    Pike_error("Cannot decode programs encoded with other pike version.\n");
	  pop_n_elems(2);

	  debug_malloc_touch(p);

	  /* parent */
	  decode_value2(data);
	  if (Pike_sp[-1].type == T_PROGRAM) {
	    p->parent = Pike_sp[-1].u.program;
	  } else if ((Pike_sp[-1].type == T_INT) &&
		     (!Pike_sp[-1].u.integer)) {
	    p->parent = NULL;
	  } else {
	    Pike_error("Bad type for parent program (%s)\n",
		       get_name_of_type(Pike_sp[-1].type));
	  }
	  Pike_sp--;

	  /* Decode lengths. */
#define FOO(X,Y,NAME) decode_number(PIKE_CONCAT(local_num_, NAME), data);
#include "program_areas.h"

	  /* Byte-code method */
	  decode_number(bytecode_method, data);
	  if (bytecode_method != PIKE_BYTECODE_METHOD) {
	    Pike_error("Unsupported byte-code method: %d\n", bytecode_method);
	  }

	  /* Decode program */
	  if (data->ptr + (int)local_num_program >= data->len) {
	    Pike_error("Failed to decode program. (string too short)\n");
	  }
	  for (e=0; e<(int)local_num_program; e++) {
	    PIKE_OPCODE_T opcode;
	    MEMCPY(&opcode, data->data + data->ptr, sizeof(PIKE_OPCODE_T));
	    data->ptr += sizeof(PIKE_OPCODE_T);
	    add_to_program(opcode);
	  }

	  /* Decode relocations */
	  for (e=0; e<(int)local_num_relocations; e++) {
	    size_t reloc;
	    decode_number(reloc, data);
	    CHECK_RELOC(reloc, local_num_program);
	    add_to_relocations(reloc);
	  }

	  /* Perform relocation. */
#ifdef DECODE_PROGRAM
	  DECODE_PROGRAM(p);
#endif /* DECODE_PROGRAM */
	  make_program_executable(p);

	  /* Decode linenumbers */
	  if (data->ptr + (int)local_num_linenumbers >= data->len) {
	    Pike_error("Failed to decode linenumbers. (string too short)\n");
	  }
	  for (e=0; e<(int)local_num_linenumbers; e++) {
	    char lineno_info;
	    lineno_info = *(data->data + data->ptr++);
	    add_to_linenumbers(lineno_info);
	  }

	  /* identifier_index & variable_index are created by
	   * fixate_program() and optimize_program().
	   */

	  /* Decode strings */
	  for (e=0; e<local_num_strings; e++) {
	    decode_value2(data);
	    if (Pike_sp[-1].type != T_STRING) {
	      Pike_error("Non strings in string table.\n");
	    }
	    add_to_strings(Pike_sp[-1].u.string);
	    Pike_sp--;
	  }

	  /* Place holder constants.
	   *
	   * These will be replaced later on.
	   */
	  {
	    struct program_constant constant;
	    constant.name = NULL;
	    constant.sval.type = T_INT;
	    constant.sval.subtype = NUMBER_UNDEFINED;
	    constant.sval.u.integer = 0;

	    for(e=0;e<local_num_constants;e++) {
	      add_to_constants(constant);
	    }
	  }

	  /* Decode identifier_references, inherits and identifiers. */
	  decode_number(entry_type, data);
	  EDB(4,
	      fprintf(stderr, "%*sDecoding identifier references.\n",
		      data->depth, ""));
#ifdef ENCODE_DEBUG
	  data->depth+=2;
#endif
	  while (entry_type != ID_ENTRY_EOT) {
	    decode_number(id_flags, data);
	    switch(entry_type) {
	    case ID_ENTRY_RAW:
	      {
		int no;
		int ref_no;
		struct reference ref;

		/* id_flags */
		ref.id_flags = id_flags;

		/* inherit_offset */
		decode_number(ref.inherit_offset, data);

		/* identifier_offset */
		/* Actually the id ref number from the inherited program */
		decode_number(ref_no, data);
		ref.identifier_offset = p->inherits[ref.inherit_offset].prog->
		  identifier_references[ref_no].identifier_offset;

		/* Expected identifier reference number */
		decode_number(no, data);

		if (no > p->num_identifier_references) {
		  EDB (3, dump_program_tables (p, data->depth));
		  Pike_error("Bad identifier reference offset: %d != %d\n", no,
			     Pike_compiler->new_program->
			     num_identifier_references);
		} else if (no == p->num_identifier_references) {
		  add_to_identifier_references(ref);
		}
		else {
		  p->identifier_references[no] = ref;
		}
	      }
	      break;

	    case ID_ENTRY_VARIABLE:
	      {
		int no;

		/* name */
		decode_value2(data);
		if (Pike_sp[-1].type != T_STRING) {
		  Pike_error("Bad variable name (not a string)\n");
		}

		/* type */
		decode_value2(data);
		if (Pike_sp[-1].type != T_TYPE) {
		  Pike_error("Bad variable type (not a type)\n");
		}

		/* Expected identifier offset */
		decode_number(no, data);

		EDB(5,
		    fprintf(stderr,
			    "%*sdefine_variable(\"%s\", X, 0x%04x)\n",
			    data->depth, "",
			    Pike_sp[-2].u.string->str, id_flags));

		/* Alters
		 *
		 * storage, variable_index, identifiers and
		 * identifier_references
		 */
		if (no != define_variable(Pike_sp[-2].u.string,
					  Pike_sp[-1].u.type,
					  id_flags)) {
		  Pike_error("Bad variable identifier offset: %d\n", no);
		}

		pop_n_elems(2);
	      }
	      break;
	    case ID_ENTRY_FUNCTION:
	      {
		union idptr func;
		unsigned INT8 func_flags;
		unsigned INT16 opt_flags;
		int no;
		int n;

		/* name */
		decode_value2(data);
		if (Pike_sp[-1].type != T_STRING) {
		  Pike_error("Bad function name (not a string)\n");
		}

		/* type */
		decode_value2(data);
		if (Pike_sp[-1].type != T_TYPE) {
		  Pike_error("Bad function type (not a type)\n");
		}

		/* func_flags (aka identifier_flags) */
		decode_number(func_flags, data);

		/* func */
		decode_number(func.offset, data);

		/* opt_flags */
		decode_number(opt_flags, data);

		/* FIXME:
		 *   Verify validity of func_flags, func.offset & opt_flags
		 */

		/* Expected identifier offset */
		decode_number(no, data);

		EDB(5, {
		  INT32 line;
		  struct pike_string *file =
		    get_line(func.offset + p->program, p, &line);
		  fprintf(stderr,
			  "%*sdefine_function(\"%s\", X, 0x%04x, 0x%04x,\n"
			  "%*s                0x%04x, 0x%04x)\n"
			  "%*s    @ %s:%d\n",
			  data->depth, "",
			  Pike_sp[-2].u.string->str, id_flags, func_flags,
			  data->depth, "",
			  func.offset, opt_flags,
			  data->depth, "",
			  file->str, line);
		});

		/* Alters
		 *
		 * identifiers, identifier_references
		 */
		n = define_function(Pike_sp[-2].u.string,
				    Pike_sp[-1].u.type,
				    id_flags, func_flags,
				    &func, opt_flags);
		if (no != n &&
		    (p->identifier_references[no].id_flags != id_flags ||
		     p->identifier_references[no].identifier_offset !=
		     p->identifier_references[n].identifier_offset ||
		     p->identifier_references[no].inherit_offset != 0)) {
		  Pike_error("Bad function identifier offset: %d\n", no);
		}

		pop_n_elems(2);
	      }
	      break;
	    case ID_ENTRY_CONSTANT:
	      {
		struct identifier id;
		struct reference ref;
		int no;
		int n;

		/* name */
		decode_value2(data);
		if (Pike_sp[-1].type != T_STRING) {
		  Pike_error("Bad function name (not a string)\n");
		}
		id.name = Pike_sp[-1].u.string;

		/* identifier_flags */
		id.identifier_flags = IDENTIFIER_CONSTANT;

		/* offset */
		decode_number(id.func.offset, data);

		/* FIXME:
		 *   Verify validity of func.offset
		 */

		/* type */
		decode_value2(data);
		if (Pike_sp[-1].type != T_TYPE) {
		  Pike_error("Bad function type (not a type)\n");
		}
		id.type = Pike_sp[-1].u.type;

		/* run_time_type */
		decode_number(id.run_time_type, data);

		/* Expected identifier number. */
		decode_number(no, data);

		n = isidentifier(id.name);

#ifdef PROFILING
		id.self_time=0;
		id.num_calls=0;
		id.total_time=0;
#endif

		/* id_flags */
		ref.id_flags = id_flags;

		/* identifier_offset */
		ref.identifier_offset =
		  Pike_compiler->new_program->num_identifiers;

		/* ref.inherit_offset */
		ref.inherit_offset = 0;

		EDB(5,
		    fprintf(stderr,
			    "%*sdefining constant(\"%s\", X, 0x%04x)\n",
			    data->depth, "",
			    Pike_sp[-2].u.string->str, id_flags));

		/* Alters
		 *
		 * identifiers, identifier_references
		 */

		if (n < 0 || (n = override_identifier (&ref, id.name)) < 0) {
		  n = p->num_identifier_references;
		  add_to_identifier_references(ref);
		}

		if (no != n) {
		  if (id.name->size_shift)
		    Pike_error("Bad constant identifier offset: %d\n", no);
		  else
		    Pike_error("Bad constant identifier offset %d for %s\n",
			       no, id.name->str);
		}

		add_to_identifiers(id);
		Pike_sp -= 2;
	      }
	      break;
	    case ID_ENTRY_INHERIT:
	      {
		struct program *prog;
		struct object *parent = NULL;
		int parent_identifier;
		int parent_offset;
		struct pike_string *name = NULL;
		int no;

		decode_number(no, data);
		if (no !=
		    Pike_compiler->new_program->num_identifier_references) {
		  Pike_error("Bad inherit identifier offset: %d\n", no);
		}

		/* name */
		decode_value2(data);
		if (Pike_sp[-1].type == T_STRING) {
		  name = Pike_sp[-1].u.string;
		} else if ((Pike_sp[-1].type != T_INT) ||
			   Pike_sp[-1].u.integer) {
		  Pike_error("Bad function name (not a string)\n");
		}

		/* prog */
		decode_value2(data);
		if (Pike_sp[-1].type != T_PROGRAM) {
		  Pike_error("Bad inherit: Expected program, got %s\n",
			     get_name_of_type(Pike_sp[-1].type));
		}
		prog = Pike_sp[-1].u.program;

		/* parent */
		decode_value2(data);
		if (Pike_sp[-1].type == T_OBJECT) {
		  parent = Pike_sp[-1].u.object;
		} else if ((Pike_sp[-1].type != T_INT) ||
			   Pike_sp[-1].u.integer) {
		  Pike_error("Bad inherit: Parent isn't an object.\n");
		}

		/* parent_identifier */
		decode_number(parent_identifier, data);

		/* parent_offset */
		decode_number(parent_offset, data);

		/* Expected number of identifier references. */
		decode_number(no, data);

		if (prog->num_identifier_references != no) {
		  Pike_error("Bad number of identifiers in inherit: %d\n", no);
		}

		EDB(5,
		    fprintf(stderr,
			    "%*slow_inherit(..., \"%s\")\n",
			    data->depth, "",
			    name?name->str:"NULL"));

		/* Alters
		 *
		 * storage, inherits and identifier_references
		 */
		low_inherit(prog, parent, parent_identifier,
			    parent_offset + 42, id_flags, name);

		pop_n_elems(3);
	      }
	      break;
	    default:
	      Pike_error("Unsupported id entry type: %d\n", entry_type);
	    }
	    decode_number(entry_type, data);
	  }

#ifdef ENCODE_DEBUG
	  data->depth-=2;
#endif

	  UNSET_ONERROR(err);

	  /* De-kludge to get end_first_pass() to free the program. */
	  Pike_compiler->num_parse_error--;

	  p->flags |= PROGRAM_PASS_1_DONE;

	  EDB(5, dump_program_tables(p, data->depth));

	  /* Fixate & optimize
	   *
	   * lfuns and identifier_index
	   */
	  if (!(p = end_first_pass(2))) {
	    Pike_error("Failed to decode program.\n");
	  }
	  push_program(p);

	  /* Verify... */
#define FOO(NUMTYPE,Y,NAME)                                                  \
          if (PIKE_CONCAT(local_num_, NAME) != p->PIKE_CONCAT(num_,NAME)) {  \
            Pike_error("Value mismatch for num_" TOSTR(NAME) ": %d != %d\n", \
                       PIKE_CONCAT(local_num_, NAME),                        \
                       p->PIKE_CONCAT(num_, NAME));                          \
          }
#include "program_areas.h"

	  /* Decode the actual constants
	   *
	   * This must be done after the program has been ended.
	   */
	  for (e=0; e<local_num_constants; e++) {
	    struct program_constant *constant = p->constants+e;
	    /* value */
	    decode_value2(data);
	    /* name */
	    decode_value2(data);
	    if (Pike_sp[-1].type == T_STRING) {
	      constant->name = Pike_sp[-1].u.string;
	    } else if ((Pike_sp[-1].type == T_INT) &&
		       !Pike_sp[-1].u.integer) {
	      constant->name = NULL;
	    } else {
	      Pike_error("Name of constant is not a string.\n");
	    }
	    constant->sval = Pike_sp[-2];
	    Pike_sp -= 2;
	  }

	  /* The program should be consistent now. */
	  p->flags &= ~PROGRAM_AVOID_CHECK;

	  EDB(5, fprintf(stderr, "%*sProgram flags: 0x%04x\n",
			 data->depth, "", p->flags));

#ifdef ENCODE_DEBUG
	  data->depth -= 2;
#endif
	}
	break;

	default:
	  Pike_error("Cannot decode program encoding type %d\n",num);
      }
      break;

  default:
    Pike_error("Failed to restore string. (Illegal type)\n");
  }

  EDB(2,fprintf(stderr, "%*sDecoded to <%d>: ", data->depth, "", tmp.u.integer);
      print_svalue(stderr, Pike_sp-1);
      fputc('\n', stderr););
  mapping_insert(data->decoded, & tmp, Pike_sp-1);
#ifdef ENCODE_DEBUG
  data->depth -= 2;
#endif
}

/* Placed after to prevent inlining */
static int init_placeholder(struct object *placeholder)
{
  JMP_BUF rec;
  /* Initialize the placeholder. */
  if(SETJMP(rec))
  {
    dmalloc_touch_svalue(&throw_value);
    call_handle_error();
    zap_placeholder(placeholder);
    UNSETJMP(rec);
    return 1;
  }else{
    call_pike_initializers(placeholder,0);
    UNSETJMP(rec);
    return 0;
  }
}



static struct decode_data *current_decode = NULL;

static void free_decode_data(struct decode_data *data)
{
  int delay;

  debug_malloc_touch(data);

  if (current_decode == data) {
    current_decode = data->next;
  } else {
    struct decode_data *d;
    for (d = current_decode; d; d=d->next) {
      if (d->next == data) {
	d->next = d->next->next;
	break;
      }
    }
#ifdef PIKE_DEBUG
    if (!d) {
      Pike_fatal("Decode data fell off the stack!\n");
    }
#endif /* PIKE_DEBUG */
  }

  
  delay=unlink_current_supporter(&data->supporter);
  call_dependants(& data->supporter, 1);

  if(delay)
  {
    debug_malloc_touch(data);
    /* We have been delayed */
    return;
  }

  free_mapping(data->decoded);

#ifdef PIKE_DEBUG
  if(data->unfinished_programs)
    Pike_fatal("We have unfinished programs left in decode()!\n");
  if(data->unfinished_objects)
    Pike_fatal("We have unfinished objects left in decode()!\n");
#endif

  while(data->unfinished_programs)
  {
    struct unfinished_prog_link *tmp=data->unfinished_programs;
    data->unfinished_programs=tmp->next;
    free((char *)tmp);
  }

  while(data->unfinished_objects)
  {
    struct unfinished_obj_link *tmp=data->unfinished_objects;
    data->unfinished_objects=tmp->next;
    free((char *)tmp);
  }
#ifdef PIKE_THREADS
  free_object(data->thread_id);
#endif

  free( (char *) data);
}

/* Run pass2 */
int re_decode(struct decode_data *data, int ignored)
{
  ONERROR err;
  SET_ONERROR(err, free_decode_data, data);
  data->next = current_decode;
  current_decode = data;

  decode_value2(data);

  UNSET_ONERROR(err);

  free_decode_data(data);
  return 1;
}

static INT32 my_decode(struct pike_string *tmp,
		       struct object *codec
#ifdef ENCODE_DEBUG
		       , int debug
#endif
		      )
{
  ONERROR err;
  struct decode_data *data;

  /* Attempt to avoid infinite recursion on circular structures. */
  for (data = current_decode; data; data=data->next) {
    if (data->raw == tmp && data->codec == codec
#ifdef PIKE_THREADS
	&& data->thread_id == Pike_interpreter.thread_id
#endif
       ) {
      struct svalue *res;
      struct svalue val = {
	T_INT, NUMBER_NUMBER,
#ifdef HAVE_UNION_INIT
	{0},	/* Only to avoid warnings. */
#endif /* HAVE_UNION_INIT */
      };
      val.u.integer = COUNTER_START;
      if ((res = low_mapping_lookup(data->decoded, &val))) {
	push_svalue(res);
	return 1;
      }
      /* Possible recursion detected. */
      /* return 0; */
    }
  }

  data=ALLOC_STRUCT(decode_data);
  data->counter.type=T_INT;
  data->counter.u.integer=COUNTER_START;
  data->data=(unsigned char *)tmp->str;
  data->len=tmp->len;
  data->ptr=0;
  data->codec=codec;
  data->pickyness=0;
  data->pass=1;
  data->unfinished_programs=0;
  data->unfinished_objects=0;
  data->raw = tmp;
  data->next = current_decode;
#ifdef PIKE_THREADS
  data->thread_id = Pike_interpreter.thread_id;
#endif
#ifdef ENCODE_DEBUG
  data->debug = debug;
  data->depth = -2;
#endif

  if (tmp->size_shift ||
      data->len < 5 ||
      GETC() != 182 ||
      GETC() != 'k' ||
      GETC() != 'e' ||
      GETC() != '0')
  {
    free( (char *) data);
    return 0;
  }

#ifdef PIKE_THREADS
  add_ref(Pike_interpreter.thread_id);
#endif

  data->decoded=allocate_mapping(128);

  current_decode = data;

  SET_ONERROR(err, free_decode_data, data);

  init_supporter(& data->supporter,
		 (supporter_callback *) re_decode,
		 (void *)data);

  decode_value2(data);

  CALL_AND_UNSET_ONERROR(err);
  return 1;
}

/* Compatibilidy decoder */

static unsigned char extract_char(char **v, ptrdiff_t *l)
{
  if(!*l) Pike_error("Format error, not enough place for char.\n");
  else (*l)--;
  (*v)++;
  return ((unsigned char *)(*v))[-1];
}

static ptrdiff_t extract_int(char **v, ptrdiff_t *l)
{
  INT32 j;
  ptrdiff_t i;

  j=extract_char(v,l);
  if(j & 0x80) return (j & 0x7f);

  if((j & ~8) > 4)
    Pike_error("Format error: Error in format string, invalid integer.\n");
  i=0;
  while(j & 7) { i=(i<<8) | extract_char(v,l); j--; }
  if(j & 8) return -i;
  return i;
}

/*! @class MasterObject
 */

/*! @decl inherit Codec
 *!
 *!   The master object is used as a fallback codec by @[encode_value()]
 *!   and @[decode_value()] if no codec was given.
 *!
 *!   It will also be used as a codec if @[decode_value()] encounters
 *!   old-style @[encode_value()]'ed data.
 */

/*! @endclass
 */

/*! @class Codec
 *!
 *!   Codec objects are used by @[encode_value()] and @[decode_value()]
 *!   to encode and decode objects, functions and programs.
 *!
 *! @note
 *!   @[encode_value()] and @[decode_value()] will use the current
 *!   master object as fallback codec object if no codec was specified.
 */

/*! @decl mixed nameof(object|function|program x)
 *!
 *!   Called by @[encode_value()] to encode objects, functions and programs.
 *!
 *! @returns
 *!   Returns something encodable on success, typically a string.
 *!   The returned value will be passed to the corresponding
 *!   @[objectof()], @[functionof()] or @[programof()] by
 *!   @[decode_value()].
 *!
 *!   Returns @[UNDEFINED] on failure.
 *!
 *! @note
 *!   @[encode_value()] has fallbacks for some classes of objects,
 *!   functions and programs.
 *!
 *! @seealso
 *!   @[objectof()], @[functionof()], @[objectof()]
 */

/*! @decl object objectof(string data)
 *!
 *!   Decode object encoded in @[data].
 *!
 *!   This function is called by @[decode_value()] when it encounters
 *!   encoded objects.
 *!
 *! @param data
 *!   Encoding of some object as specified by @[nameof()].
 *!
 *! @param minor
 *!   Minor version.
 *!
 *! @returns
 *!   Returns the decoded object.
 *!
 *! @seealso
 *!   @[functionof()], @[programof()]
 */

/*! @decl function functionof(string data)
 *!
 *!   Decode function encoded in @[data].
 *!
 *!   This function is called by @[decode_value()] when it encounters
 *!   encoded functions.
 *!
 *! @param data
 *!   Encoding of some function as specified by @[nameof()].
 *!
 *! @param minor
 *!   Minor version.
 *!
 *! @returns
 *!   Returns the decoded function.
 *!
 *! @seealso
 *!   @[objectof()], @[programof()]
 */

/*! @decl program programof(string data)
 *!
 *!   Decode program encoded in @[data].
 *!
 *!   This function is called by @[decode_value()] when it encounters
 *!   encoded programs.
 *!
 *! @param data
 *!   Encoding of some program as specified by @[nameof()].
 *!
 *! @param minor
 *!   Minor version.
 *!
 *! @returns
 *!   Returns the decoded program.
 *!
 *! @seealso
 *!   @[functionof()], @[objectof()]
 */

/*! @decl program __register_new_program(program p)
 *!
 *!   Called by @[decode_value()] to register a program that is
 *!   being decoded.
 *!
 *! @returns
 *!   Returns a program os a place-holder program.
 */

/*! @endclass
 */

static void rec_restore_value(char **v, ptrdiff_t *l)
{
  ptrdiff_t t, i;

  i = extract_int(v,l);
  t = extract_int(v,l);
  switch(i)
  {
  case TAG_INT:
    push_int(DO_NOT_WARN(t));
    return;

  case TAG_FLOAT:
    if(sizeof(ptrdiff_t) < sizeof(FLOAT_TYPE))  /* FIXME FIXME FIXME FIXME */
      Pike_error("Float architecture not supported.\n");
    push_int(DO_NOT_WARN(t)); /* WARNING! */
    Pike_sp[-1].type = T_FLOAT;
    return;

  case TAG_TYPE:
    {
      Pike_error("Format error: TAG_TYPE not supported yet.\n");
    }
    return;

  case TAG_STRING:
    if(t<0) Pike_error("Format error: length of string is negative.\n");
    if(*l < t) Pike_error("Format error: string to short\n");
    push_string(make_shared_binary_string(*v, t));
    (*l)-= t;
    (*v)+= t;
    return;

  case TAG_ARRAY:
    if(t<0) Pike_error("Format error: length of array is negative.\n");
    check_stack(t);
    for(i=0;i<t;i++) rec_restore_value(v,l);
    f_aggregate(DO_NOT_WARN(t)); /* FIXME: Unbounded stack consumption. */
    return;

  case TAG_MULTISET:
    if(t<0) Pike_error("Format error: length of multiset is negative.\n");
    check_stack(t);
    for(i=0;i<t;i++) rec_restore_value(v,l);
    f_aggregate_multiset(DO_NOT_WARN(t)); /* FIXME: Unbounded stack consumption. */
    return;

  case TAG_MAPPING:
    if(t<0) Pike_error("Format error: length of mapping is negative.\n");
    check_stack(t*2);
    for(i=0;i<t;i++)
    {
      rec_restore_value(v,l);
      rec_restore_value(v,l);
    }
    f_aggregate_mapping(DO_NOT_WARN(t*2)); /* FIXME: Unbounded stack consumption. */
    return;

  case TAG_OBJECT:
    if(t<0) Pike_error("Format error: length of object is negative.\n");
    if(*l < t) Pike_error("Format error: string to short\n");
    push_string(make_shared_binary_string(*v, t));
    (*l) -= t; (*v) += t;
    APPLY_MASTER("objectof", 1);
    return;

  case TAG_FUNCTION:
    if(t<0) Pike_error("Format error: length of function is negative.\n");
    if(*l < t) Pike_error("Format error: string to short\n");
    push_string(make_shared_binary_string(*v, t));
    (*l) -= t; (*v) += t;
    APPLY_MASTER("functionof", 1);
    return;

  case TAG_PROGRAM:
    if(t<0) Pike_error("Format error: length of program is negative.\n");
    if(*l < t) Pike_error("Format error: string to short\n");
    push_string(make_shared_binary_string(*v, t));
    (*l) -= t; (*v) += t;
    APPLY_MASTER("programof", 1);
    return;

  default:
    Pike_error("Format error: Unknown type tag %ld:%ld\n",
	  PTRDIFF_T_TO_LONG(i), PTRDIFF_T_TO_LONG(t));
  }
}

/*! @decl mixed decode_value(string coded_value, object|void codec)
 *!
 *! Decode a value from a string.
 *!
 *! This function takes a string created with @[encode_value()] or
 *! @[encode_value_canonic()] and converts it back to the value that was
 *! coded.
 *!
 *! If no codec is specified, the current master object will be used as codec.
 *!
 *! @seealso
 *!   @[encode_value()], @[encode_value_canonic()]
 */
void f_decode_value(INT32 args)
{
  struct pike_string *s;
  struct object *codec;

#ifdef ENCODE_DEBUG
  int debug;
#endif /* ENCODE_DEBUG */

  check_all_args("decode_value", args,
		 BIT_STRING, BIT_VOID | BIT_OBJECT | BIT_INT,
#ifdef ENCODE_DEBUG
		 /* This argument is only an internal debug helper.
		  * It's intentionally not part of the function
		  * prototype, to keep the argument position free for
		  * other uses in the future. */
		 BIT_VOID | BIT_INT,
#endif
		 0);

#ifdef ENCODE_DEBUG
  debug = args > 2 ? Pike_sp[2-args].u.integer : 0;
#endif /* ENCODE_DEBUG */

  s = Pike_sp[-args].u.string;
  if(args<2)
  {
    codec=get_master();
  }
  else if(Pike_sp[1-args].type == T_OBJECT)
  {
    codec=Pike_sp[1-args].u.object;
  }
  else
  {
    codec=0;
  }

  if(!my_decode(s, codec
#ifdef ENCODE_DEBUG
		, debug
#endif
	       ))
  {
    char *v=s->str;
    ptrdiff_t l=s->len;
    rec_restore_value(&v, &l);
  }
  assign_svalue(Pike_sp-args-1, Pike_sp-1);
  pop_n_elems(args);
}
