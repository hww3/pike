/*\
||| This file is part of Pike. For copyright information see COPYRIGHT.
||| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
||| for more information.
||| $Id: mpz_glue.c,v 1.110 2002/10/08 23:31:26 nilsson Exp $
\*/
/**/

#include "global.h"
RCSID("$Id: mpz_glue.c,v 1.110 2002/10/08 23:31:26 nilsson Exp $");
#include "gmp_machine.h"

#if defined(HAVE_GMP2_GMP_H) && defined(HAVE_LIBGMP2)
#define USE_GMP2
#else /* !HAVE_GMP2_GMP_H || !HAVE_LIBGMP2 */
#if defined(HAVE_GMP_H) && defined(HAVE_LIBGMP)
#define USE_GMP
#endif /* HAVE_GMP_H && HAVE_LIBGMP */
#endif /* HAVE_GMP2_GMP_H && HAVE_LIBGMP2 */

#if defined(USE_GMP) || defined(USE_GMP2)

#include "interpret.h"
#include "svalue.h"
#include "stralloc.h"
#include "array.h"
#include "pike_macros.h"
#include "program.h"
#include "stralloc.h"
#include "object.h"
#include "pike_types.h"
#include "pike_error.h"
#include "builtin_functions.h"
#include "opcodes.h"
#include "module_support.h"
#include "bignum.h"
#include "operators.h"

#include "my_gmp.h"

#include <limits.h>

#endif /* defined(USE_GMP) || defined(USE_GMP2) */

/* This must be included last! */
#include "module_magic.h"

#if defined(USE_GMP) || defined(USE_GMP2)

#define sp Pike_sp
#define fp Pike_fp

#ifdef _MSC_VER
/* No random()... provide one for gmp
 * This should possibly be a configure test
 * /Hubbe
 */
long random(void)
{
  return my_rand();
}
#endif

#undef THIS
#define THIS ((MP_INT *)(fp->current_storage))
#define THIS_PROGRAM (fp->context.prog)

struct program *mpzmod_program;
#ifdef AUTO_BIGNUM
struct program *bignum_program;
#endif

#ifdef AUTO_BIGNUM
void mpzmod_reduce(struct object *o)
{
  INT_TYPE i;

  i = mpz_get_si(OBTOMPZ(o));
  if(mpz_cmp_si(OBTOMPZ(o), i) == 0)
  {
     free_object(o);
     push_int(i);
  }
  else
  {
#ifdef BIG_PIKE_INT
#define SHIFT  16 /* suggested: CHAR_BIT*sizeof(unsigned long int) */
#define FILTER ((1<<SHIFT)-1)
     int pos=0;
     unsigned long int a;
     INT_TYPE res=0;
     mpz_t t,u;

     mpz_init_set(t,OBTOMPZ(o));
     mpz_init(u);
     while (pos<sizeof(INT_TYPE)*CHAR_BIT)
     {
        a=mpz_get_ui(t)&FILTER;
        if (!a && mpz_cmp_si(t,0)==0) 
        {
           if (mpz_get_si(OBTOMPZ(o))<0) res=-res;
	   mpz_clear(t);
	   mpz_clear(u);
           free_object(o);
           push_int(res);
           return;
        }
        res|=((INT_TYPE)a)<<pos;
        if ((res>>pos) != a) break; 
        mpz_fdiv_q_2exp(u,t,SHIFT);
	mpz_set(t,u);
	pos+=SHIFT;
     }
     mpz_clear(t);
     mpz_clear(u);
#undef SHIFT
#undef FILTER
#endif
     push_object(o);
  }
}
#define PUSH_REDUCED(o) do { struct object *reducetmp__=(o);	\
   if(THIS_PROGRAM == bignum_program)				\
     mpzmod_reduce(reducetmp__);					\
   else								\
     push_object(reducetmp__);					\
}while(0)

#else
#define PUSH_REDUCED(o) push_object(o)
#endif /* AUTO_BIGNUM */

/*! @module Gmp
 */

/*! @class mpz
 */

void get_mpz_from_digits(MP_INT *tmp,
			 struct pike_string *digits,
			 int base)
{
  if(!base || ((base >= 2) && (base <= 36)))
  {
    int offset = 0;
    int neg = 0;

    if(digits->len > 1)
    {
      if(INDEX_CHARP(digits->str, 0, digits->size_shift) == '+')
	offset += 1;
      else if(INDEX_CHARP(digits->str, 0, digits->size_shift) == '-')
      {
	offset += 1;
	neg = 1;
      }

      /* We need to fix the case with binary
	 0b101... and -0b101... numbers. */
      if(!base && digits->len > 2)
      {
	if((INDEX_CHARP(digits->str, offset, digits->size_shift) == '0') &&
	   ((INDEX_CHARP(digits->str, offset+1, digits->size_shift) == 'b') ||
	    (INDEX_CHARP(digits->str, offset+1, digits->size_shift) == 'B')))
	{
	  offset += 2;
	  base = 2;
	}
      }
    }

    if (mpz_set_str(tmp, digits->str + offset, base))
      Pike_error("invalid digits, cannot convert to mpz\n");

    if(neg)
      mpz_neg(tmp, tmp);
  }
  else if(base == 256)
  {
    int i;
    mpz_t digit;
    
    mpz_init(digit);
    mpz_set_ui(tmp, 0);
    for (i = 0; i < digits->len; i++)
    {
      mpz_set_ui(digit, EXTRACT_UCHAR(digits->str + i));
      mpz_mul_2exp(digit, digit,
		   DO_NOT_WARN((unsigned long)(digits->len - i - 1) * 8));
      mpz_ior(tmp, tmp, digit);
    }
    mpz_clear(digit);
  }
  else
  {
    Pike_error("invalid base.\n");
  }
}

void get_new_mpz(MP_INT *tmp, struct svalue *s)
{
  switch(s->type)
  {
  case T_INT:
#if BIG_PIKE_INT
/*  INT_TYPE is bigger then long int  */
  {
     INT_TYPE x=s->u.integer;
#define SHIFT  24
#define FILTER ((1<<SHIFT)-1)
     int neg=0;
     int pos=SHIFT;
     if (x<0) neg=1,x=-x;
     mpz_set_ui(tmp,(unsigned long int)(x&FILTER));
     while ( (x>>=SHIFT) )
     {
        mpz_t t2,t1;
        mpz_init_set_ui(t2,(unsigned long int)(x&FILTER)); 
        mpz_init(t1);
        mpz_mul_2exp(t1,t2,pos);
        mpz_add(t2,tmp,t1);
        mpz_set(tmp,t2);
        mpz_clear(t1);
        mpz_clear(t2);
     }
     if (neg)
     {
        mpz_t t1;
        mpz_init_set(t1,tmp);
        mpz_neg(tmp,t1);
        mpz_clear(t1);
     }
  }
#undef SHIFT
#undef FILTER
#else
    mpz_set_si(tmp, (signed long int) s->u.integer);
#endif
    break;
    
  case T_FLOAT:
    mpz_set_d(tmp, (double) s->u.float_number);
    break;

  case T_OBJECT:
    if(s->u.object->prog == mpf_program)
    {
      mpz_set_f(tmp, OBTOMPF(s->u.object));
      break;
    }

    if(s->u.object->prog == mpq_program)
    {
      mpz_set_q(tmp, OBTOMPQ(s->u.object));
      break;
    }

    if(s->u.object->prog != mpzmod_program
#ifdef AUTO_BIGNUM
       && s->u.object->prog != bignum_program
#endif
       ) {
      if (s->u.object->prog) {
	Pike_error("Wrong type of object (id:%d), cannot convert to mpz.\n",
		   s->u.object->prog->id);
      } else {
	/* Destructed object. Use as zero. */
	mpz_set_si(tmp, 0);
      }
    } else {
      mpz_set(tmp, OBTOMPZ(s->u.object));
    }
    break;
#if 0    
  case T_STRING:
    mpz_set_str(tmp, s->u.string->str, 0);
    break;

  case T_ARRAY:   /* Experimental */
    if ( (s->u.array->size != 2)
	 || (ITEM(s->u.array)[0].type != T_STRING)
	 || (ITEM(s->u.array)[1].type != T_INT))
      Pike_error("cannot convert array to mpz.\n");
    get_mpz_from_digits(tmp, ITEM(s->u.array)[0].u.string,
			ITEM(s->u.array)[1]);
    break;
#endif
  default:
    Pike_error("cannot convert argument to mpz.\n");
  }
}

/*! @decl void create()
 *! @decl void create(string|int|float|object value)
 *! @decl void create(string value, int(2..36)|int(256..256) base)
 *!
 *!   Create and initialize a @[Gmp.mpz] object.
 *!
 *! @param value
 *!   Initial value. If no value is specified, the object will be initialized
 *!   to zero.
 *!
 *! @param base
 *!   Base the value is specified in. The default base is base 10.
 *!   The base can be either a value in the range @tt{[2..36]@} (inclusive),
 *!   in which case the numbers are taken from the ASCII range
 *!   @tt{0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ@} (case-insensitive),
 *!   or the value 256, in which case @[value] is taken to be the binary
 *!   representation in network byte order.
 *!
 *! @note
 *!   Leading zeroes in @[value] are not significant. In particular leading
 *!   NUL characters are not preserved in base 256 mode.
 */
static void mpzmod_create(INT32 args)
{
#ifdef AUTO_BIGNUM
  /* Alert bignum.c that we have been loaded /Hubbe */
  if(THIS_PROGRAM == bignum_program)
    gmp_library_loaded=1;
#endif

  switch(args)
  {
  case 1:
    if(sp[-args].type == T_STRING)
      get_mpz_from_digits(THIS, sp[-args].u.string, 0);
    else
      get_new_mpz(THIS, sp-args);
    break;

  case 2: /* Args are string of digits and integer base */
    if(sp[-args].type != T_STRING)
      Pike_error("bad argument 1 for Mpz->create()\n");

    if (sp[1-args].type != T_INT)
      Pike_error("wrong type for base in Mpz->create()\n");

    get_mpz_from_digits(THIS, sp[-args].u.string, sp[1-args].u.integer);
    break;

  default:
    Pike_error("Too many arguments to Mpz->create()\n");

  case 0:
    break;	/* Needed by AIX cc */
  }
  pop_n_elems(args);
}

static void mpzmod_get_int(INT32 args)
{
  pop_n_elems(args);
#ifdef AUTO_BIGNUM
  add_ref(fp->current_object);
  mpzmod_reduce(fp->current_object);
#else
  push_int(mpz_get_si(THIS));
#endif /* AUTO_BIGNUM */
}

static void mpzmod___hash(INT32 args)
{
  pop_n_elems(args);
  push_int(mpz_get_si(THIS));
}

static void mpzmod_get_float(INT32 args)
{
  pop_n_elems(args);
  push_float((FLOAT_TYPE)mpz_get_d(THIS));
}

struct pike_string *low_get_mpz_digits(MP_INT *mpz, int base)
{
  struct pike_string *s = 0;   /* Make gcc happy. */
  ptrdiff_t len;
  
  if ( (base >= 2) && (base <= 36))
  {
    len = mpz_sizeinbase(mpz, base) + 2;
    s = begin_shared_string(len);
    mpz_get_str(s->str, base, mpz);
    /* Find NULL character */
    len-=4;
    if (len < 0) len = 0;
    while(s->str[len]) len++;
    s=end_and_resize_shared_string(s, len);
  }
  else if (base == 256)
  {
    size_t i;
#if 0
    mpz_t tmp;
#endif

    if (mpz_sgn(mpz) < 0)
      Pike_error("only non-negative numbers can be converted to base 256.\n");
#if 0
    len = (mpz_sizeinbase(mpz, 2) + 7) / 8;
    s = begin_shared_string(len);
    mpz_init_set(tmp, mpz);
    i = len;
    while(i--)
    {
      s->str[i] = mpz_get_ui(tmp) & 0xff;
      mpz_fdiv_q_2exp(tmp, tmp, 8);
    }
    mpz_clear(tmp);
#endif

    /* lets optimize this /Mirar & Per */

    /* len = mpz->_mp_size*sizeof(mp_limb_t); */
    /* This function should not return any leading zeros. /Nisse */
    len = (mpz_sizeinbase(mpz, 2) + 7) / 8;
    s = begin_shared_string(len);

    if (!mpz->_mp_size)
    {
      /* Zero is a special case. There are no limbs at all, but
       * the size is still 1 bit, and one digit should be produced. */
      if (len != 1)
	Pike_fatal("mpz->low_get_mpz_digits: strange mpz state!\n");
      s->str[0] = 0;
    } else {
      mp_limb_t *src = mpz->_mp_d;
      unsigned char *dst = (unsigned char *)s->str+s->len;

      while (len > 0)
      {
	mp_limb_t x=*(src++);
	for (i=0; i<sizeof(mp_limb_t); i++)
	{
	  *(--dst) = DO_NOT_WARN((unsigned char)(x & 0xff));
	  x >>= 8;
	  if (!--len)
	    break;
	  
	}
      }
    }
    s = end_shared_string(s);
  }
  else
  {
    Pike_error("invalid base.\n");
    return 0; /* Make GCC happy */
  }

  return s;
}

static void mpzmod_get_string(INT32 args)
{
  pop_n_elems(args);
  push_string(low_get_mpz_digits(THIS, 10));
}

static void mpzmod_digits(INT32 args)
{
  INT32 base;
  struct pike_string *s;
  
  if (!args)
  {
    base = 10;
  }
  else
  {
    if (sp[-args].type != T_INT)
      Pike_error("Bad argument 1 for Mpz->digits().\n");
    base = sp[-args].u.integer;
  }

  s = low_get_mpz_digits(THIS, base);
  pop_n_elems(args);

  push_string(s);
}

static void mpzmod__sprintf(INT32 args)
{
  INT_TYPE precision, width, width_undecided, base = 0, mask_shift = 0;
  struct pike_string *s = 0;
  INT_TYPE flag_left, method;

  debug_malloc_touch(Pike_fp->current_object);
  
  if(args < 1 || sp[-args].type != T_INT)
    Pike_error("Bad argument 1 for Mpz->_sprintf().\n");
  if(args < 2 || sp[1-args].type != T_MAPPING)
    Pike_error("Bad argument 2 for Mpz->_sprintf().\n");

  push_svalue(&sp[1-args]);
  push_constant_text("precision");
  f_index(2);
  if(sp[-1].type != T_INT)
    Pike_error("\"precision\" argument to Mpz->_sprintf() is not an integer.\n");
  precision = (--sp)->u.integer;
  
  push_svalue(&sp[1-args]);
  push_constant_text("width");
  f_index(2);
  if(sp[-1].type != T_INT)
    Pike_error("\"width\" argument to Mpz->_sprintf() is not an integer.\n");
  width_undecided = ((sp-1)->subtype != NUMBER_NUMBER);
  width = (--sp)->u.integer;

  push_svalue(&sp[1-args]);
  push_constant_text("flag_left");
  f_index(2);
  if(sp[-1].type != T_INT)
    Pike_error("\"flag_left\" argument to Mpz->_sprintf() is not an integer.\n");
  flag_left=sp[-1].u.integer;
  pop_stack();

  debug_malloc_touch(Pike_fp->current_object);

  switch(method = sp[-args].u.integer)
  {
#ifdef AUTO_BIGNUM
    case 't':
      pop_n_elems(args);
      if(THIS_PROGRAM == bignum_program)
	push_constant_text("int");
      else
	push_constant_text("object");
      return;
#endif      
  case 'O':
  case 'u': /* Note: 'u' is not really supported. */
  case 'd':
    s = low_get_mpz_digits(THIS, 10);
    break;

  case 'x':
  case 'X':
    base += 8;
    mask_shift += 1;
    /* Fall-through. */
  case 'o':
    base += 6;
    mask_shift += 2;
    /* Fall-through. */
  case 'b':
    base += 2;
    mask_shift += 1;

    if(precision > 0)
    {
      mpz_t mask;

      mpz_init_set_ui(mask, 1);
      mpz_mul_2exp(mask, mask, precision * mask_shift);
      mpz_sub_ui(mask, mask, 1);
      mpz_and(mask, mask, THIS);
      s = low_get_mpz_digits(mask, base);
      mpz_clear(mask);
    }
    else
      s = low_get_mpz_digits(THIS, base);
    break;
    
  case 'c':
  {
    INT_TYPE length, neg = 0;
    unsigned char *dst;
    mp_limb_t *src;
    mpz_t tmp;
    MP_INT *n;
    INT_TYPE i;

    length = THIS->_mp_size;

    if(width_undecided)
    {
      p_wchar2 ch = mpz_get_ui(THIS);
      if(length<0)
	ch = (~ch)+1;
      s = make_shared_binary_string2(&ch, 1);
      break;
    }
    
    if(length < 0)
    {
      mpz_init_set(tmp, THIS);
      mpz_add_ui(tmp, tmp, 1);
      length = -tmp->_mp_size;
      n = tmp;
      neg = 1;
    }
    else
      n = THIS;

    if(width < 1)
      width = 1;
    
    s = begin_shared_string(width);
    
    if (!flag_left)
       dst = (unsigned char *)STR0(s) + width;
    else
       dst = (unsigned char *)STR0(s);
       
    src = n->_mp_d;

    while(width > 0)
    {
      mp_limb_t x = (length-->0? *(src++) : 0);

      if (!flag_left)
	 for(i = 0; i < (INT_TYPE)sizeof(mp_limb_t); i++)
	 {
	    *(--dst) = DO_NOT_WARN((unsigned char)((neg ? ~x : x) & 0xff));
	    x >>= 8;
	    if(!--width)
	       break;
	 }
      else
	 for(i = 0; i < (INT_TYPE)sizeof(mp_limb_t); i++)
	 {
	    *(dst++) = DO_NOT_WARN((unsigned char)((neg ? ~x : x) & 0xff));
	    x >>= 8;
	    if(!--width)
	       break;
	 }
    }
    
    if(neg)
    {
      mpz_clear(tmp);
    }
    
    s = end_shared_string(s);
  }
  break;
  }

  debug_malloc_touch(Pike_fp->current_object);

  pop_n_elems(args);

  if(s) {
    push_string(s);
    if (method == 'X') {
      f_upper_case(1);
    }
  } else {
    push_int(0);   /* Push false? */
    sp[-1].subtype = 1;
  }
}

static void mpzmod__is_type(INT32 args)
{
  if(args < 1 || sp[-args].type != T_STRING)
    Pike_error("Bad argument 1 for Mpz->_is_type().\n");

  pop_n_elems(args-1);
  push_constant_text("int");
  f_eq(2);
}

static void mpzmod_size(INT32 args)
{
  int base;
  if (!args)
  {
    /* Default is number of bits */
    base = 2;
  }
  else
  {
    if (sp[-args].type != T_INT)
      Pike_error("bad argument 1 for Mpz->size()\n");
    base = sp[-args].u.integer;
    if ((base != 256) && ((base < 2) || (base > 36)))
      Pike_error("invalid base\n");
  }
  pop_n_elems(args);

  if (base == 256)
    push_int(DO_NOT_WARN((INT32)((mpz_sizeinbase(THIS, 2) + 7) / 8)));
  else
    push_int(DO_NOT_WARN((INT32)(mpz_sizeinbase(THIS, base))));
}

static void mpzmod_cast(INT32 args)
{
  struct pike_string *s;

  if(args < 1)
    Pike_error("mpz->cast() called without arguments.\n");
  if(sp[-args].type != T_STRING)
    Pike_error("Bad argument 1 to mpz->cast().\n");

  s = sp[-args].u.string;
  add_ref(s);

  pop_n_elems(args);

  switch(s->str[0])
  {
  case 'i':
    if(!strncmp(s->str, "int", 3))
    {
      free_string(s);
      mpzmod_get_int(0);
      return;
    }
    break;

  case 's':
    if(!strcmp(s->str, "string"))
    {
      free_string(s);
      mpzmod_get_string(0);
      return;
    }
    break;

  case 'f':
    if(!strcmp(s->str, "float"))
    {
      free_string(s);
      mpzmod_get_float(0);
      return;
    }
    break;

  case 'o':
    if(!strcmp(s->str, "object"))
    {
      push_object(this_object());
    }
    break;

  case 'm':
    if(!strcmp(s->str, "mixed"))
    {
      push_object(this_object());
    }
    break;
    
  }

  push_string(s);	/* To get it freed when Pike_error() pops the stack. */

  Pike_error("mpz->cast() to \"%s\" is other type than string, int or float.\n",
	s->str);
}

/* Converts an svalue, located on the stack, to an mpz object */
MP_INT *debug_get_mpz(struct svalue *s, int throw_error)
{
#define MPZ_ERROR(x) if (throw_error) Pike_error(x)
  struct object *o;
  switch(s->type)
  {
  default:
    MPZ_ERROR("Wrong type of value, cannot convert to mpz.\n");
    return 0;

  case T_INT:
  case T_FLOAT:
#if 0
  case T_STRING:
  case T_ARRAY:
#endif
  use_as_int:
    o=clone_object(mpzmod_program,0);
    get_new_mpz(OBTOMPZ(o), s);
    free_svalue(s);
    s->u.object=o;
    s->type=T_OBJECT;
    return (MP_INT *)o->storage;
    
  case T_OBJECT:
    if(s->u.object->prog != mpzmod_program
#ifdef AUTO_BIGNUM
       && s->u.object->prog != bignum_program
#endif
      )
    {
      if (s->u.object->prog) {
	if(throw_error)
	  Pike_error("Wrong type of object (id:%d), cannot convert to mpz.\n",
		     s->u.object->prog->id);
      } else {
	/* Destructed object. Use as zero. */
	goto use_as_int;
      }
      /* NOT_REACHED */
      return 0;
    }
    return (MP_INT *)s->u.object->storage;
  }
#undef ERROR
}

/* Non-reentrant */
#if 0
/* These two functions are here so we can allocate temporary
 * objects without having to worry about them leaking in
 * case of errors..
 */
static struct object *temporary;
MP_INT *get_tmp(void)
{
  if(!temporary)
    temporary=clone_object(mpzmod_program,0);

  return (MP_INT *)temporary->storage;
}

static void return_temporary(INT32 args)
{
  pop_n_elems(args);
  push_object(temporary);
  temporary=0;
}
#endif

#ifdef AUTO_BIGNUM

#define DO_IF_AUTO_BIGNUM(X) X
double double_from_sval(struct svalue *s)
{
  switch(s->type)
  {
    case T_INT: return (double)s->u.integer;
    case T_FLOAT: return (double)s->u.float_number;
    case T_OBJECT: 
      if(s->u.object->prog == mpzmod_program ||
	 s->u.object->prog == bignum_program)
	return mpz_get_d(OBTOMPZ(s->u.object));
    default:
      Pike_error("Bad argument, expected a number of some sort.\n");
  }
  /* NOT_REACHED */
  return (double)0.0;	/* Keep the compiler happy. */
}

#else
#define DO_IF_AUTO_BIGNUM(X)
#endif

#ifdef BIG_PIKE_INT
#define TOOBIGTEST || sp[e-args].u.integer>MAX_INT32
#else
#define TOOBIGTEST 
#endif

#define BINFUN2(name, fun, OP)						\
static void name(INT32 args)						\
{									\
  INT32 e;								\
  struct object *res;							\
  DO_IF_AUTO_BIGNUM(                                                    \
  if(THIS_PROGRAM == bignum_program)					\
  {									\
    double ret;								\
    for(e=0; e<args; e++)						\
    {									\
      switch(sp[e-args].type)						\
      {									\
       case T_FLOAT:                                                    \
	ret=mpz_get_d(THIS);						\
	for(e=0; e<args; e++)						\
	  ret = ret OP double_from_sval(sp-args);		        \
									\
	pop_n_elems(args);						\
	push_float( (FLOAT_TYPE)ret );					\
	return;								\
 STRINGCONV(                                                            \
       case T_STRING:                                                   \
        MEMMOVE(sp-args+1, sp-args, sizeof(struct svalue)*args);        \
        sp++; args++;                                                   \
        sp[-args].type=T_INT;                                           \
        sp[-args].u.string=low_get_mpz_digits(THIS, 10);                \
        sp[-args].type=T_STRING;                                        \
        f_add(args);                                                    \
	return; )							\
      }									\
    }									\
  } )									\
  for(e=0; e<args; e++)							\
   if(sp[e-args].type != T_INT || sp[e-args].u.integer<=0 TOOBIGTEST)	\
    get_mpz(sp+e-args, 1);						\
  res = fast_clone_object(THIS_PROGRAM, 0);				\
  mpz_set(OBTOMPZ(res), THIS);						\
  for(e=0;e<args;e++)							\
    if(sp[e-args].type != T_INT)					\
      fun(OBTOMPZ(res), OBTOMPZ(res), OBTOMPZ(sp[e-args].u.object));	\
    else								\
      PIKE_CONCAT(fun,_ui)(OBTOMPZ(res), OBTOMPZ(res),			\
                           sp[e-args].u.integer);			\
									\
  pop_n_elems(args);                                                    \
  PUSH_REDUCED(res);                                                    \
}									\
                                                                        \
STRINGCONV(                                                             \
static void PIKE_CONCAT(name,_rhs)(INT32 args)				\
{									\
  INT32 e;								\
  struct object *res;							\
  DO_IF_AUTO_BIGNUM(                                                    \
  if(THIS_PROGRAM == bignum_program)					\
  {									\
    double ret;								\
    for(e=0; e<args; e++)						\
    {									\
      switch(sp[e-args].type)						\
      {									\
       case T_FLOAT:                                                    \
	ret=mpz_get_d(THIS);						\
	for(e=0; e<args; e++)						\
	  ret = ret OP double_from_sval(sp-args);		        \
									\
	pop_n_elems(args);						\
	push_float( (FLOAT_TYPE)ret );					\
	return;								\
       case T_STRING:                                                   \
        push_string(low_get_mpz_digits(THIS, 10));                      \
        f_add(args+1);                                                  \
	return; 							\
      }									\
    }									\
  } )									\
  for(e=0; e<args; e++)							\
   if(sp[e-args].type != T_INT || sp[e-args].u.integer<=0)		\
    get_mpz(sp+e-args, 1);						\
  res = fast_clone_object(THIS_PROGRAM, 0);				\
  mpz_set(OBTOMPZ(res), THIS);						\
  for(e=0;e<args;e++)							\
    if(sp[e-args].type != T_INT)					\
      fun(OBTOMPZ(res), OBTOMPZ(res), OBTOMPZ(sp[e-args].u.object));	\
    else								\
      PIKE_CONCAT(fun,_ui)(OBTOMPZ(res), OBTOMPZ(res),			\
                           sp[e-args].u.integer);			\
									\
  pop_n_elems(args);							\
  PUSH_REDUCED(res);							\
}									\
)									\
									\
static void PIKE_CONCAT(name,_eq)(INT32 args)				\
{									\
  INT32 e;								\
  DO_IF_AUTO_BIGNUM(                                                    \
  if(THIS_PROGRAM == bignum_program)					\
  {									\
    double ret;								\
    for(e=0; e<args; e++)						\
    {									\
      switch(sp[e-args].type)						\
      {									\
       case T_FLOAT:                                                    \
	ret=mpz_get_d(THIS);						\
	for(e=0; e<args; e++)						\
	  ret = ret OP double_from_sval(sp-args);	        	\
									\
	pop_n_elems(args);						\
	push_float( (FLOAT_TYPE)ret );					\
	return;								\
       case T_STRING:                                                   \
        MEMMOVE(sp-args+1, sp-args, sizeof(struct svalue)*args);        \
        sp++; args++;                                                   \
        sp[-args].type=T_INT;                                           \
        sp[-args].u.string=low_get_mpz_digits(THIS, 10);                \
        sp[-args].type=T_STRING;                                        \
        f_add(args);                                                    \
	return;								\
      }									\
    }									\
  } )									\
  for(e=0; e<args; e++)							\
   if(sp[e-args].type != T_INT || sp[e-args].u.integer<=0)		\
    get_mpz(sp+e-args, 1);						\
  for(e=0;e<args;e++)							\
    if(sp[e-args].type != T_INT)					\
      fun(THIS, THIS, OBTOMPZ(sp[e-args].u.object));			\
    else								\
      PIKE_CONCAT(fun,_ui)(THIS,THIS, sp[e-args].u.integer);		\
  add_ref(fp->current_object);						\
  PUSH_REDUCED(fp->current_object);					\
}

#define STRINGCONV(X) X
BINFUN2(mpzmod_add,mpz_add,+)

#undef STRINGCONV
#define STRINGCONV(X)
BINFUN2(mpzmod_mul,mpz_mul,*)


static void mpzmod_gcd(INT32 args)
{
  INT32 e;
  struct object *res;
  for(e=0; e<args; e++)
   if(sp[e-args].type != T_INT || sp[e-args].u.integer<=0)
    get_mpz(sp+e-args, 1);
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_set(OBTOMPZ(res), THIS);
  for(e=0;e<args;e++)
    if(sp[e-args].type != T_INT)
     mpz_gcd(OBTOMPZ(res), OBTOMPZ(res), OBTOMPZ(sp[e-args].u.object));
    else
      mpz_gcd_ui(OBTOMPZ(res), OBTOMPZ(res),sp[e-args].u.integer);

  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_sub(INT32 args)
{
  INT32 e;
  struct object *res;

  if (args)
    for (e = 0; e<args; e++)
      get_mpz(sp + e - args, 1);
  
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_set(OBTOMPZ(res), THIS);

  if(args)
  {
    for(e=0;e<args;e++)
      mpz_sub(OBTOMPZ(res), OBTOMPZ(res), OBTOMPZ(sp[e-args].u.object));
  }else{
    mpz_neg(OBTOMPZ(res), OBTOMPZ(res));
  }
  pop_n_elems(args);
  debug_malloc_touch(res);
  PUSH_REDUCED(res);
}

static void mpzmod_rsub(INT32 args)
{
  struct object *res = NULL;
  MP_INT *a;
  
  if(args!=1)
    Pike_error("Gmp.mpz->``- called with more or less than one argument.\n");
  
  a=get_mpz(sp-1,1);
  
  res = fast_clone_object(THIS_PROGRAM, 0);

  mpz_sub(OBTOMPZ(res), a, THIS);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_div(INT32 args)
{
  INT32 e;
  struct object *res;
  
  for(e=0;e<args;e++)
  {
    if(sp[e-args].type != T_INT || sp[e-args].u.integer<=0)
      if (!mpz_sgn(get_mpz(sp+e-args, 1)))
	Pike_error("Division by zero.\n");	
  }
  
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_set(OBTOMPZ(res), THIS);
  for(e=0;e<args;e++)	
  {
    if(sp[e-args].type == T_INT)
      mpz_fdiv_q_ui(OBTOMPZ(res), OBTOMPZ(res), sp[e-args].u.integer);
   else
      mpz_fdiv_q(OBTOMPZ(res), OBTOMPZ(res), OBTOMPZ(sp[e-args].u.object));
  }

  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rdiv(INT32 args)
{
  MP_INT *a;
  struct object *res = NULL;
  if(!mpz_sgn(THIS))
    Pike_error("Division by zero.\n");

  if(args!=1)
    Pike_error("Gmp.mpz->``/() called with more than one argument.\n");

  a=get_mpz(sp-1,1);
  
  res=fast_clone_object(THIS_PROGRAM,0);
  mpz_fdiv_q(OBTOMPZ(res), a, THIS);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_mod(INT32 args)
{
  INT32 e;
  struct object *res;
  
  for(e=0;e<args;e++)
    if (!mpz_sgn(get_mpz(sp+e-args, 1)))
      Pike_error("Division by zero.\n");	
  
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_set(OBTOMPZ(res), THIS);
  for(e=0;e<args;e++)	
    mpz_fdiv_r(OBTOMPZ(res), OBTOMPZ(res), OBTOMPZ(sp[e-args].u.object));

  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rmod(INT32 args)
{
  MP_INT *a;
  struct object *res = NULL;
  if(!mpz_sgn(THIS))
    Pike_error("Modulo by zero.\n");

  if(args!=1)
    Pike_error("Gmp.mpz->``%%() called with more than one argument.\n");

  a=get_mpz(sp-1,1);
  
  res=fast_clone_object(THIS_PROGRAM,0);
  mpz_fdiv_r(OBTOMPZ(res), a, THIS);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}


static void mpzmod_gcdext(INT32 args)
{
  struct object *g, *s, *t;
  MP_INT *a;

  if (args != 1)
    Pike_error("Gmp.mpz->gcdext: Wrong number of arguments.\n");

  a = get_mpz(sp-1, 1);
  
  g = fast_clone_object(THIS_PROGRAM, 0);
  s = fast_clone_object(THIS_PROGRAM, 0);
  t = fast_clone_object(THIS_PROGRAM, 0);

  mpz_gcdext(OBTOMPZ(g), OBTOMPZ(s), OBTOMPZ(t), THIS, a);
  pop_n_elems(args);
  PUSH_REDUCED(g); PUSH_REDUCED(s); PUSH_REDUCED(t);
  f_aggregate(3);
}

static void mpzmod_gcdext2(INT32 args)
{
  struct object *g, *s;
  MP_INT *a;

  if (args != 1)
    Pike_error("Gmp.mpz->gcdext: Wrong number of arguments.\n");

  a = get_mpz(sp-args, 1);
  
  g = fast_clone_object(THIS_PROGRAM, 0);
  s = fast_clone_object(THIS_PROGRAM, 0);

  mpz_gcdext(OBTOMPZ(g), OBTOMPZ(s), NULL, THIS, a);
  pop_n_elems(args);
  PUSH_REDUCED(g); PUSH_REDUCED(s); 
  f_aggregate(2);
}

static void mpzmod_invert(INT32 args)
{
  MP_INT *modulo;
  struct object *res;

  if (args != 1)
    Pike_error("Gmp.mpz->invert: wrong number of arguments.\n");
  modulo = get_mpz(sp-args, 1);
  if (!mpz_sgn(modulo))
    Pike_error("divide by zero\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  if (mpz_invert(OBTOMPZ(res), THIS, modulo) == 0)
  {
    free_object(res);
    Pike_error("Gmp.mpz->invert: not invertible\n");
  }
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

#define BINFUN(name, fun)				\
static void name(INT32 args)				\
{							\
  INT32 e;						\
  struct object *res;					\
  for(e=0; e<args; e++)					\
    get_mpz(sp+e-args, 1);				\
  res = fast_clone_object(THIS_PROGRAM, 0);		\
  mpz_set(OBTOMPZ(res), THIS);				\
  for(e=0;e<args;e++)					\
    fun(OBTOMPZ(res), OBTOMPZ(res),			\
	OBTOMPZ(sp[e-args].u.object));			\
  pop_n_elems(args);					\
  PUSH_REDUCED(res);					\
}

BINFUN(mpzmod_and,mpz_and)
BINFUN(mpzmod_or,mpz_ior)
BINFUN(mpzmod_xor,my_mpz_xor)

static void mpzmod_compl(INT32 args)
{
  struct object *o;
  pop_n_elems(args);
  o=fast_clone_object(THIS_PROGRAM,0);
  mpz_com(OBTOMPZ(o), THIS);
  PUSH_REDUCED(o);
}

#define CMPEQU(name,cmp,default)			\
static void name(INT32 args)				\
{							\
  INT32 i;						\
  MP_INT *arg;						\
  if(!args) Pike_error("Comparison with one argument?\n");	\
  if (!(arg = get_mpz(sp-args, 0)))			\
    do { default; }while(0);				\
  else							\
    i=mpz_cmp(THIS, arg) cmp 0;				\
  pop_n_elems(args);					\
  push_int(i);						\
}

#define RET_UNDEFINED do{pop_n_elems(args);push_undefined();return;}while(0)

CMPEQU(mpzmod_gt, >, RET_UNDEFINED)
CMPEQU(mpzmod_lt, <, RET_UNDEFINED)
CMPEQU(mpzmod_ge, >=, RET_UNDEFINED)
CMPEQU(mpzmod_le, <=, RET_UNDEFINED)

CMPEQU(mpzmod_eq, ==, RET_UNDEFINED)
CMPEQU(mpzmod_nq, !=, i=1)

static void mpzmod_probably_prime_p(INT32 args)
{
  INT_TYPE count;
  if (args)
  {
    get_all_args("Gmp.mpz->probably_prime_p", args, "%i", &count);
    count = sp[-1].u.integer;
    if (count <= 0)
      Pike_error("Gmp.mpz->probably_prime_p: count argument must be positive.\n");
  } else
    count = 25;
  pop_n_elems(args);
  push_int(mpz_probab_prime_p(THIS, count));
}

static void mpzmod_small_factor(INT32 args)
{
  INT_TYPE limit;

  if (args)
    {
      get_all_args("Gmp.mpz->small_factor", args, "%i", &limit);
      if (limit < 1)
	Pike_error("Gmp.mpz->small_factor: limit argument must be at least 1.\n");
    }
  else
    limit = INT_MAX;
  pop_n_elems(args);
  push_int(mpz_small_factor(THIS, limit));
}

static void mpzmod_next_prime(INT32 args)
{
  INT_TYPE count = 25;
  INT_TYPE limit = INT_MAX;
  struct object *o;

  switch(args)
  {
  case 0:
    break;
  case 1:
    get_all_args("Gmp.mpz->next_prime", args, "%i", &count);
    break;
  default:
    get_all_args("Gmp.mpz->next_prime", args, "%i%i", &count, &limit);
    break;
  }
  pop_n_elems(args);
  
  o = fast_clone_object(THIS_PROGRAM, 0);
  mpz_next_prime(OBTOMPZ(o), THIS, count, limit);
  
  PUSH_REDUCED(o);
}

static void mpzmod_sgn(INT32 args)
{
  pop_n_elems(args);
  push_int(mpz_sgn(THIS));
}


static void mpzmod_sqrt(INT32 args)
{
  struct object *o = 0;   /* Make gcc happy. */
  pop_n_elems(args);
  if(mpz_sgn(THIS)<0)
    Pike_error("mpz->sqrt() on negative number.\n");

  o=fast_clone_object(THIS_PROGRAM,0);
  mpz_sqrt(OBTOMPZ(o), THIS);
  PUSH_REDUCED(o);
}

static void mpzmod_sqrtrem(INT32 args)
{
  struct object *root = 0, *rem = 0;   /* Make gcc happy. */
  
  pop_n_elems(args);
  if(mpz_sgn(THIS)<0)
    Pike_error("mpz->sqrtrem() on negative number.\n");

  root = fast_clone_object(THIS_PROGRAM,0);
  rem = fast_clone_object(THIS_PROGRAM,0);
  mpz_sqrtrem(OBTOMPZ(root), OBTOMPZ(rem), THIS);
  PUSH_REDUCED(root); PUSH_REDUCED(rem);
  f_aggregate(2);
}

static void mpzmod_lsh(INT32 args)
{
  struct object *res = NULL;
  if (args != 1)
    Pike_error("Wrong number of arguments to Gmp.mpz->`<<.\n");
  ref_push_type_value(int_type_string);
  stack_swap();
  f_cast();
  if(sp[-1].u.integer < 0)
    Pike_error("mpz->lsh on negative number.\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_mul_2exp(OBTOMPZ(res), THIS, sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rsh(INT32 args)
{
  struct object *res = NULL;
  if (args != 1)
    Pike_error("Wrong number of arguments to Gmp.mpz->`>>.\n");
  ref_push_type_value(int_type_string);
  stack_swap();
  f_cast();
  if (sp[-1].u.integer < 0)
    Pike_error("Gmp.mpz->rsh: Shift count must be positive.\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_fdiv_q_2exp(OBTOMPZ(res), THIS, sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rlsh(INT32 args)
{
  struct object *res = NULL;
  INT32 i;
  if (args != 1)
    Pike_error("Wrong number of arguments to Gmp.mpz->``<<.\n");
  get_mpz(sp-1,1);
  i=mpz_get_si(THIS);
  if(i < 0)
    Pike_error("mpz->``<< on negative number.\n");

  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_mul_2exp(OBTOMPZ(res), OBTOMPZ(sp[-1].u.object), i);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_rrsh(INT32 args)
{
  struct object *res = NULL;
  INT32 i;
  if (args != 1)
    Pike_error("Wrong number of arguments to Gmp.mpz->``>>.\n");
  get_mpz(sp-1,1);
  i=mpz_get_si(THIS);
  if(i < 0)
    Pike_error("mpz->``>> on negative number.\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_fdiv_q_2exp(OBTOMPZ(res), OBTOMPZ(sp[-1].u.object), i);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_powm(INT32 args)
{
  struct object *res = NULL;
  MP_INT *n;
  
  if(args != 2)
    Pike_error("Wrong number of arguments to Gmp.mpz->powm()\n");

  n = get_mpz(sp - 1, 1);
  if (!mpz_sgn(n))
    Pike_error("Gmp.mpz->powm: Divide by zero\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_powm(OBTOMPZ(res), THIS, get_mpz(sp - 2, 1), n);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_pow(INT32 args)
{
  struct object *res = NULL;
  
  if (args != 1)
    Pike_error("Gmp.mpz->pow: Wrong number of arguments.\n");
  if (sp[-1].type != T_INT)
    Pike_error("Gmp.mpz->pow: Non int exponent.\n");
  if (sp[-1].u.integer < 0)
    Pike_error("Gmp.mpz->pow: Negative exponent.\n");
  res = fast_clone_object(THIS_PROGRAM, 0);
  mpz_pow_ui(OBTOMPZ(res), THIS, sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_not(INT32 args)
{
  pop_n_elems(args);
  push_int(!mpz_sgn(THIS));
}

static void mpzmod_popcount(INT32 args)
{
  pop_n_elems(args);
#ifdef HAVE_MPZ_POPCOUNT
  push_int(mpz_popcount(THIS));  
#else
  switch (mpz_sgn(THIS))
  {
  case 0:
    push_int(0);
    break;
  case -1:
    push_int(-1);
    break;
  case 1:
    push_int(mpn_popcount(THIS->_mp_d, THIS->_mp_size));
    break;
  default:
    Pike_fatal("Gmp.mpz->popcount: Unexpected sign!\n");
  }
#endif
}

static void gmp_pow(INT32 args)
{
  struct object *res = NULL;
  if (args != 2)
    Pike_error("Gmp.pow: Wrong number of arguments");
  if ( (sp[-2].type != T_INT) || (sp[-2].u.integer < 0)
       || (sp[-1].type != T_INT) || (sp[-1].u.integer < 0))
    Pike_error("Gmp.pow: Negative arguments");
  res = fast_clone_object(mpzmod_program, 0);
  mpz_ui_pow_ui(OBTOMPZ(res), sp[-2].u.integer, sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void gmp_fac(INT32 args)
{
  struct object *res = NULL;
  if (args != 1)
    Pike_error("Gmp.fac: Wrong number of arguments.\n");
  if (sp[-1].type != T_INT)
    Pike_error("Gmp.fac: Non int argument.\n");
  if (sp[-1].u.integer < 0)
    Pike_error("Gmp.mpz->pow: Negative exponent.\n");
  res = fast_clone_object(mpzmod_program, 0);
  mpz_fac_ui(OBTOMPZ(res), sp[-1].u.integer);
  pop_n_elems(args);
  PUSH_REDUCED(res);
}

static void mpzmod_random(INT32 args)
{
  struct object *res = 0;   /* Make gcc happy. */
  pop_n_elems(args);
  if(mpz_sgn(THIS) <= 0)
    Pike_error("random on negative number.\n");

  res=fast_clone_object(THIS_PROGRAM,0);
  /* We add two to assure reasonably uniform randomness */
  mpz_random(OBTOMPZ(res), DO_NOT_WARN((int)(mpz_size(THIS) + 2)));
  mpz_fdiv_r(OBTOMPZ(res), OBTOMPZ(res), THIS); /* modulo */
  PUSH_REDUCED(res);
}
  
static void init_mpz_glue(struct object *o)
{
#ifdef PIKE_DEBUG
  if(!fp) Pike_fatal("ZERO FP\n");
  if(!THIS) Pike_fatal("ZERO THIS\n");
#endif
  mpz_init(THIS);
}

static void exit_mpz_glue(struct object *o)
{
#ifdef PIKE_DEBUG
  if(!fp) Pike_fatal("ZERO FP\n");
  if(!THIS) Pike_fatal("ZERO THIS\n");
#endif
  mpz_clear(THIS);
}
#endif

void pike_module_exit(void)
{
  pike_exit_mpf_module();
  pike_exit_mpq_module();
#if defined(USE_GMP) || defined(USE_GMP2)
  if(mpzmod_program)
  {
    free_program(mpzmod_program);
    mpzmod_program=0;
  }
#ifdef AUTO_BIGNUM
  if(bignum_program)
  {
    free_program(bignum_program);
    bignum_program=0;
  }
#endif
#endif
}


#define MPZ_ARG_TYPE "int|float|object"
#define MPZ_RET_TYPE "object"
#define MPZ_SHIFT_TYPE "function(int|float|object:" MPZ_RET_TYPE")"
#define MPZ_BINOP_TYPE ("function(" MPZ_ARG_TYPE "...:"MPZ_RET_TYPE")")
#define MPZ_CMPOP_TYPE ("function(mixed:int)")

#define tMpz_arg tOr3(tInt,tFloat,tObj)
#define tMpz_ret tObj
#define tMpz_int tInt
#define tMpz_shift_type tFunc(tMpz_arg,tMpz_ret)
#define tMpz_binop_type tFuncV(tNone, tMpz_arg, tMpz_ret)
#define tMpz_cmpop_type tFunc(tMixed, tMpz_ret)

#define MPZ_DEFS()							\
  ADD_STORAGE(MP_INT);							\
  									\
  /* function(void|string|int|float|object:void)" */			\
  /* "|function(string,int:void) */					\
  ADD_FUNCTION("create", mpzmod_create,					\
	       tOr(tFunc(tOr5(tVoid,tStr,tInt,tFlt,			\
			      tObj),tVoid),				\
		   tFunc(tStr tInt,tVoid)), ID_STATIC);			\
									\
  ADD_FUNCTION("`+",mpzmod_add,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("`+=",mpzmod_add_eq,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("``+",mpzmod_add_rhs,tMpz_binop_type, ID_STATIC);	\
  ADD_FUNCTION("`-",mpzmod_sub,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("``-",mpzmod_rsub,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("`*",mpzmod_mul,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("``*",mpzmod_mul,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("`*=",mpzmod_mul_eq,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("`/",mpzmod_div,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("``/",mpzmod_rdiv,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("`%",mpzmod_mod,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("``%",mpzmod_rmod,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("`&",mpzmod_and,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("``&",mpzmod_and,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("`|",mpzmod_or,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("``|",mpzmod_or,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("`^",mpzmod_xor,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("``^",mpzmod_xor,tMpz_binop_type, ID_STATIC);		\
  ADD_FUNCTION("`~",mpzmod_compl,tFunc(tNone,tObj), ID_STATIC);		\
									\
  add_function("`<<",mpzmod_lsh,MPZ_SHIFT_TYPE, ID_STATIC);		\
  add_function("`>>",mpzmod_rsh,MPZ_SHIFT_TYPE, ID_STATIC);		\
  add_function("``<<",mpzmod_rlsh,MPZ_SHIFT_TYPE, ID_STATIC);		\
  add_function("``>>",mpzmod_rrsh,MPZ_SHIFT_TYPE, ID_STATIC);		\
									\
  add_function("`>", mpzmod_gt,MPZ_CMPOP_TYPE, ID_STATIC);		\
  add_function("`<", mpzmod_lt,MPZ_CMPOP_TYPE, ID_STATIC);		\
  add_function("`>=",mpzmod_ge,MPZ_CMPOP_TYPE, ID_STATIC);		\
  add_function("`<=",mpzmod_le,MPZ_CMPOP_TYPE, ID_STATIC);		\
									\
  add_function("`==",mpzmod_eq,MPZ_CMPOP_TYPE, ID_STATIC);		\
  add_function("`!=",mpzmod_nq,MPZ_CMPOP_TYPE, ID_STATIC);		\
									\
  ADD_FUNCTION("`!",mpzmod_not,tFunc(tNone,tInt), ID_STATIC);		\
									\
  ADD_FUNCTION("__hash",mpzmod___hash,tFunc(tNone,tInt), ID_STATIC);	\
  ADD_FUNCTION("cast",mpzmod_cast,tFunc(tStr,tMix), ID_STATIC);		\
									\
  ADD_FUNCTION("_is_type", mpzmod__is_type, tFunc(tStr,tInt), ID_STATIC);\
  									\
  ADD_FUNCTION("digits", mpzmod_digits,tFunc(tOr(tVoid,tInt),tStr), 0);	\
  ADD_FUNCTION("_sprintf", mpzmod__sprintf, tFunc(tInt,tStr), ID_STATIC);\
  ADD_FUNCTION("size", mpzmod_size,tFunc(tOr(tVoid,tInt),tInt), 0);	\
									\
  ADD_FUNCTION("cast_to_int",mpzmod_get_int,tFunc(tNone,tMpz_int),0);	\
  ADD_FUNCTION("cast_to_string",mpzmod_get_string,tFunc(tNone,tStr),0);	\
  ADD_FUNCTION("cast_to_float",mpzmod_get_float,tFunc(tNone,tFlt),0);	\
									\
  ADD_FUNCTION("probably_prime_p",mpzmod_probably_prime_p,		\
	       tFunc(tNone,tInt),0);					\
  ADD_FUNCTION("small_factor", mpzmod_small_factor,			\
	       tFunc(tOr(tInt,tVoid),tInt), 0);				\
  ADD_FUNCTION("next_prime", mpzmod_next_prime,				\
	       tFunc(tOr(tInt,tVoid) tOr(tInt,tVoid),tMpz_ret), 0);	\
  									\
  ADD_FUNCTION("gcd",mpzmod_gcd, tMpz_binop_type, 0);			\
  ADD_FUNCTION("gcdext",mpzmod_gcdext,tFunc(tMpz_arg,tArr(tMpz_ret)),0);\
  ADD_FUNCTION("gcdext2",mpzmod_gcdext2,tFunc(tMpz_arg,tArr(tMpz_ret)),0);\
  ADD_FUNCTION("invert", mpzmod_invert,tFunc(tMpz_arg,tMpz_ret),0);	\
									\
  ADD_FUNCTION("sqrt", mpzmod_sqrt,tFunc(tNone,tMpz_ret),0);		\
  ADD_FUNCTION("_sqrt", mpzmod_sqrt,tFunc(tNone,tMpz_ret),0);		\
  ADD_FUNCTION("sqrtrem",mpzmod_sqrtrem,tFunc(tNone,tArr(tMpz_ret)),0);\
  ADD_FUNCTION("powm",mpzmod_powm,tFunc(tMpz_arg tMpz_arg,tMpz_ret),0);	\
  ADD_FUNCTION("pow", mpzmod_pow,tFunc(tInt,tMpz_ret), 0);		\
									\
  ADD_FUNCTION("popcount", mpzmod_popcount,tFunc(tVoid,tInt), 0);	\
									\
  ADD_FUNCTION("_random",mpzmod_random,tFunc(tNone,tMpz_ret),0);	\
  									\
  set_init_callback(init_mpz_glue);					\
  set_exit_callback(exit_mpz_glue);


void pike_module_init(void)
{
#if defined(USE_GMP) || defined(USE_GMP2)
  start_new_program();

  MPZ_DEFS();

#if 0
  /* These are not implemented yet */
  /* function(:int) */
  ADD_FUNCTION("squarep", mpzmod_squarep,tFunc(tNone,tInt), 0);
  add_function("divmod", mpzmod_divmod, "function(" MPZ_ARG_TYPE ":array(object))", 0);
  add_function("divm", mpzmod_divm, "function(" MPZ_ARG_TYPE ","
	       MPZ_ARG_TYPE ":object)", 0);
#endif

  mpzmod_program=end_program();
  mpzmod_program->id = PROG_GMP_MPZ_ID;
  add_program_constant("mpz", mpzmod_program, 0);

  /* function(int, int:object) */
  ADD_FUNCTION("pow", gmp_pow,tFunc(tInt tInt,tObj), 0);
  /* function(int:object) */
  ADD_FUNCTION("fac", gmp_fac,tFunc(tInt,tObj), 0);

#ifdef AUTO_BIGNUM
  {
    int id;

    /* This program autoconverts to integers, Gmp.mpz does not!!
     * magic? no, just an if statement :)              /Hubbe
     */
    start_new_program();

#undef tMpz_ret
#undef tMpz_int
#define tMpz_ret tInt
#define tMpz_int tInt

    /* I first tried to just do an inherit here, but it becomes too hard
     * to tell the programs apart when I do that..          /Hubbe
     */
    MPZ_DEFS();

    id=add_program_constant("bignum", bignum_program=end_program(), 0);
    bignum_program->flags |= 
      PROGRAM_NO_WEAK_FREE |
      PROGRAM_NO_EXPLICIT_DESTRUCT |
      PROGRAM_CONSTANT ;
    
#if 0
    /* magic /Hubbe
     * This seems to break more than it fixes though... /Hubbe
     */
    free_type(ID_FROM_INT(Pike_compiler->new_program, id)->type);
    ID_FROM_INT(Pike_compiler->new_program, id)->type=CONSTTYPE(tOr(tFunc(tOr5(tVoid,tStr,tInt,tFlt,tObj),tInt),tFunc(tStr tInt,tInt)));
#endif
  }
#endif

#endif

  pike_init_mpq_module();
  pike_init_mpf_module();
}

/*! @endclass
 */

/*! @endmodule
 */
