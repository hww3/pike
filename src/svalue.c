/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
#include "global.h"
#include "main.h"
#include "svalue.h"
#include "stralloc.h"
#include "array.h"
#include "mapping.h"
#include "multiset.h"
#include "object.h"
#include "program.h"
#include "constants.h"
#include "error.h"
#include "dynamic_buffer.h"
#include "interpret.h"
#include "gc.h"
#include "pike_macros.h"
#include <ctype.h>

RCSID("$Id: svalue.c,v 1.26 1998/04/08 03:31:49 hubbe Exp $");

struct svalue dest_ob_zero = { T_INT, 0 };


/*
 * This routine frees a short svalue given a pointer to it and
 * its type.
 */

void really_free_short_svalue(union anything *s, TYPE_T type)
{
  union anything tmp=*s;
  s->refs=0;
  switch(type)
  {
    case T_ARRAY:
      really_free_array(tmp.array);
      break;
      
    case T_MAPPING:
      really_free_mapping(tmp.mapping);
      break;
      
    case T_MULTISET:
      really_free_multiset(tmp.multiset);
      break;
      
    case T_OBJECT:
      really_free_object(tmp.object);
      break;
      
    case T_PROGRAM:
      really_free_program(tmp.program);
      break;
      
    case T_STRING:
      really_free_string(tmp.string);
      break;
      
#ifdef DEBUG
    default:
	fatal("Bad type in free_short_svalue.\n");
#endif
  }
}

void really_free_svalue(struct svalue *s)
{
  int tmp=s->type;
  s->type=T_INT;
  switch(tmp)
  {
  case T_ARRAY:
    really_free_array(s->u.array);
#ifdef DEBUG
    s->type = 99;
#endif
    break;
    
  case T_MAPPING:
    really_free_mapping(s->u.mapping);
#ifdef DEBUG
    s->type = 99;
#endif
    break;
    
  case T_MULTISET:
    really_free_multiset(s->u.multiset);
#ifdef DEBUG
    s->type = 99;
#endif
    break;
    
  case T_FUNCTION:
    if(s->subtype == FUNCTION_BUILTIN)
    {
      really_free_callable(s->u.efun);
      break;
    }
    /* fall through */
    
  case T_OBJECT:
    really_free_object(s->u.object);
    break;
    
  case T_PROGRAM:
    really_free_program(s->u.program);
#ifdef DEBUG
    s->type = 99;
#endif
    break;
    
  case T_STRING:
    really_free_string(s->u.string);
#ifdef DEBUG
    s->type = 99;
#endif
    break;
    
#ifdef DEBUG
  default:
    fatal("Bad type in free_svalue.\n");
#endif
  }
}

/* Free a bunch of normal svalues.
 * We put this routine here so the compiler can optimize the call
 * inside the loop if it wants to
 */
void free_svalues(struct svalue *s,INT32 num, INT32 type_hint)
{
  switch(type_hint)
  {
  case 0:
  case BIT_INT:
  case BIT_FLOAT:
  case BIT_FLOAT | BIT_INT:
    return;

#define DOTYPE(X,Y,Z) case X:while(--num>=0) { Y(s->u.Z); s++; }return
    DOTYPE(BIT_STRING, free_string, string);
    DOTYPE(BIT_ARRAY, free_array, array);
    DOTYPE(BIT_MAPPING, free_mapping, mapping);
    DOTYPE(BIT_MULTISET, free_multiset, multiset);
    DOTYPE(BIT_OBJECT, free_object, object);
    DOTYPE(BIT_PROGRAM, free_program, program);

#define COMBINE9(A) case A:

#define COMBINE8(A,B) \
  COMBINE9(A|B)

#define COMBINE7(A,B,C) \
  COMBINE8(A|B,C) \
  COMBINE9(A|C)

#define COMBINE6(A,B,C,D) \
  COMBINE7(A|B,C,D) \
  COMBINE8(A|C,D) \
  COMBINE9(A|D)

#define COMBINE5(A,B,C,D,E) \
  COMBINE6(A|B,C,D,E) \
  COMBINE7(A|C,D,E) \
  COMBINE8(A|D,E) \
  COMBINE9(A|E)

#define COMBINE4(A,B,C,D,E,F) \
  COMBINE5(A|B,C,D,E,F) \
  COMBINE6(A|C,D,E,F) \
  COMBINE7(A|D,E,F) \
  COMBINE8(A|E,F) \
  COMBINE9(A|F)

#define COMBINE3(A,B,C,D,E,F,G) \
  COMBINE4(A|B,C,D,E,F,G) \
  COMBINE5(A|C,D,E,F,G) \
  COMBINE6(A|D,E,F,G) \
  COMBINE7(A|E,F,G) \
  COMBINE8(A|F,G) \
  COMBINE9(A|G)

#define COMBINE(A,B,C,D,E,F,G) \
  COMBINE3(A,B,C,D,E,F,G) \
  COMBINE4(B,C,D,E,F,G) \
  COMBINE5(C,D,E,F,G) \
  COMBINE6(D,E,F,G) \
  COMBINE7(E,F,G) \
  COMBINE8(F,G)


  COMBINE(BIT_STRING, BIT_ARRAY, BIT_MAPPING, BIT_MULTISET, BIT_OBJECT, BIT_PROGRAM, BIT_FUNCTION);
    while(--num>=0)
    {
      if(--s->u.refs[0]<=0)
	really_free_svalue(s);
      s++;
    }
    break;

  case BIT_FUNCTION:
    while(--num>=0)
    {
      if(--s->u.refs[0] <= 0)
      {
	if(s->subtype == FUNCTION_BUILTIN)
	  really_free_callable(s->u.efun);
	else
	  really_free_object(s->u.object);
      }
      s++;
    }
    return;

#undef DOTYPE
  default:
    while(--num >= 0) free_svalue(s++);
  }
}

void assign_svalues_no_free(struct svalue *to,
			    struct svalue *from,
			    INT32 num,
			    INT32 type_hint)
{
#ifdef DEBUG
  if(d_flag)
  {
    INT32 e,t;
    for(t=e=0;e<num;e++) t|=1<<from[e].type;
    if(t & ~type_hint)
      fatal("Type hint lies!\n");
  }
#endif
  if((type_hint & ((2<<MAX_REF_TYPE)-1)) == 0)
  {
    MEMCPY((char *)to, (char *)from, sizeof(struct svalue) * num);
    return;
  }

  if((type_hint & ((2<<MAX_REF_TYPE)-1)) == type_hint)
  {
    while(--num >= 0)
    {
      struct svalue tmp;
      tmp=*(from++);
      *(to++)=tmp;
      tmp.u.refs[0]++;
    }
    return;
  }

  while(--num >= 0) assign_svalue_no_free(to++,from++);
}

void assign_svalues(struct svalue *to,
		    struct svalue *from,
		    INT32 num,
		    TYPE_FIELD types)
{
  free_svalues(to,num,BIT_MIXED);
  assign_svalues_no_free(to,from,num,types);
}

void assign_to_short_svalue(union anything *u,
			    TYPE_T type,
			    struct svalue *s)
{
  check_type(s->type);
  check_refs(s);

  if(s->type == type)
  {
    INT32 *tmp;
    switch(type)
    {
      case T_INT: u->integer=s->u.integer; break;
      case T_FLOAT: u->float_number=s->u.float_number; break;
      default:
	if(u->refs && --*(u->refs) <= 0) really_free_short_svalue(u,type);
	u->refs = tmp = s->u.refs;
	tmp[0]++;
    }
  }else if(type<=MAX_REF_TYPE && IS_ZERO(s)){
    if(u->refs && --*(u->refs) <= 0) really_free_short_svalue(u,type);
    u->refs=0;
  }else{
    error("Wrong type in assignment.\n");
  }
}

void assign_to_short_svalue_no_free(union anything *u,
				    TYPE_T type,
				    struct svalue *s)
{
  check_type(s->type);
  check_refs(s);

  if(s->type == type)
  {
    INT32 *tmp;
    switch(type)
    {
      case T_INT: u->integer=s->u.integer; break;
      case T_FLOAT: u->float_number=s->u.float_number; break;
      default:
	u->refs = tmp = s->u.refs;
	tmp[0]++;
    }
  }else if(type<=MAX_REF_TYPE && IS_ZERO(s)){
    u->refs=0;
  }else{
    error("Wrong type in assignment.\n");
  }
}


void assign_from_short_svalue_no_free(struct svalue *s,
					     union anything *u,
					     TYPE_T type)
{
  check_type(type);
  check_refs2(u,type);

  switch(type)
  {
    case T_FLOAT: s->type=T_FLOAT; s->u.float_number=u->float_number; break;
    case T_INT: s->type=T_INT; s->u.integer=u->integer; break;
    default:
      if((s->u.refs=u->refs))
      {
	u->refs[0]++;
	s->type=type;
      }else{
	s->type=T_INT;
	s->subtype=NUMBER_NUMBER;
	s->u.integer=0;
      }
  }
}

void assign_short_svalue_no_free(union anything *to,
				 union anything *from,
				 TYPE_T type)
{
  INT32 *tmp;
  check_type(type);
  check_refs2(from,type);

  switch(type)
  {
    case T_INT: to->integer=from->integer; break;
    case T_FLOAT: to->float_number=from->float_number; break;
    default:
      to->refs = tmp = from->refs;
      if(tmp) tmp[0]++;
  }
}

void assign_short_svalue(union anything *to,
			 union anything *from,
			 TYPE_T type)
{
  INT32 *tmp;
  check_type(type);
  check_refs2(from,type);

  switch(type)
  {
    case T_INT: to->integer=from->integer; break;
    case T_FLOAT: to->float_number=from->float_number; break;
    default:
      if(to->refs && --*(to->refs) <= 0) really_free_short_svalue(to,type);
      to->refs = tmp = from->refs;
      if(tmp) tmp[0]++;
  }
}

unsigned INT32 hash_svalue(struct svalue *s)
{
  unsigned INT32 q;

  check_type(s->type);
  check_refs(s);

  switch(s->type)
  {
  case T_OBJECT:
    if(!s->u.object->prog)
    {
      q=0;
      break;
    }

    if(FIND_LFUN(s->u.object->prog,LFUN___HASH) != -1)
    {
      safe_apply_low(s->u.object, FIND_LFUN(s->u.object->prog,LFUN___HASH), 0);
      if(sp[-1].type == T_INT)
      {
	q=sp[-1].u.integer;
      }else{
	q=0;
      }
      pop_stack();
      break;
    }

  default:      q=(unsigned INT32)((long)s->u.refs >> 2);
  case T_INT:   q=s->u.integer; break;
  case T_FLOAT: q=(unsigned INT32)(s->u.float_number * 16843009.731757771173); break;
  }
  q+=q % 997;
  q+=((q + s->type) * 9248339);
  
  return q;
}

int svalue_is_true(struct svalue *s)
{
  unsigned INT32 q;
  check_type(s->type);
  check_refs(s);

  switch(s->type)
  {
  case T_INT:
    if(s->u.integer) return 1;
    return 0;

  case T_FUNCTION:
    if(!s->u.object->prog) return 0;
    return 1;

  case T_OBJECT:
    if(!s->u.object->prog) return 0;

    if(FIND_LFUN(s->u.object->prog,LFUN_NOT)!=-1)
    {
      safe_apply_low(s->u.object,FIND_LFUN(s->u.object->prog,LFUN_NOT),0);
      if(sp[-1].type == T_INT && sp[-1].u.integer == 0)
      {
	pop_stack();
	return 1;
      } else {
	pop_stack();
	return 0;
      }
    }

  default:
    return 1;
  }
    
}

#define TWO_TYPES(X,Y) (((X)<<8)|(Y))

int is_eq(struct svalue *a, struct svalue *b)
{
  check_type(a->type);
  check_type(b->type);
  check_refs(a);
  check_refs(b);

  safe_check_destructed(a);
  safe_check_destructed(b);

  if (a->type != b->type)
  {
    switch(TWO_TYPES((1<<a->type),(1<<b->type)))
    {
    case TWO_TYPES(BIT_FUNCTION,BIT_PROGRAM):
      return program_from_function(a) == b->u.program;
    
    case TWO_TYPES(BIT_PROGRAM,BIT_FUNCTION):
      return program_from_function(b) == a->u.program;

    case TWO_TYPES(BIT_OBJECT, BIT_ARRAY):
    case TWO_TYPES(BIT_OBJECT, BIT_MAPPING):
    case TWO_TYPES(BIT_OBJECT, BIT_MULTISET):
    case TWO_TYPES(BIT_OBJECT, BIT_OBJECT):
    case TWO_TYPES(BIT_OBJECT, BIT_FUNCTION):
    case TWO_TYPES(BIT_OBJECT, BIT_PROGRAM):
    case TWO_TYPES(BIT_OBJECT, BIT_STRING):
    case TWO_TYPES(BIT_OBJECT, BIT_INT):
    case TWO_TYPES(BIT_OBJECT, BIT_FLOAT):
      if(FIND_LFUN(a->u.object->prog,LFUN_EQ) != -1)
      {
      a_is_obj:
	assign_svalue_no_free(sp, b);
	sp++;
	apply_lfun(a->u.object, LFUN_EQ, 1);
	if(IS_ZERO(sp-1))
	{
	  pop_stack();
	  return 0;
	}else{
	  pop_stack();
	  return 1;
	}
      }
    if(b->type != T_OBJECT) return 0;

    case TWO_TYPES(BIT_ARRAY,BIT_OBJECT):
    case TWO_TYPES(BIT_MAPPING,BIT_OBJECT):
    case TWO_TYPES(BIT_MULTISET,BIT_OBJECT):
    case TWO_TYPES(BIT_FUNCTION,BIT_OBJECT):
    case TWO_TYPES(BIT_PROGRAM,BIT_OBJECT):
    case TWO_TYPES(BIT_STRING,BIT_OBJECT):
    case TWO_TYPES(BIT_INT,BIT_OBJECT):
    case TWO_TYPES(BIT_FLOAT,BIT_OBJECT):
      if(FIND_LFUN(b->u.object->prog,LFUN_EQ) != -1)
      {
      b_is_obj:
	assign_svalue_no_free(sp, a);
	sp++;
	apply_lfun(b->u.object, LFUN_EQ, 1);
	if(IS_ZERO(sp-1))
	{
	  pop_stack();
	  return 0;
	}else{
	  pop_stack();
	  return 1;
	}
      }
    }

    return 0;
  }
  switch(a->type)
  {
  case T_OBJECT:
    if(FIND_LFUN(a->u.object->prog,LFUN_EQ) != -1)
      goto a_is_obj;

    if(FIND_LFUN(b->u.object->prog,LFUN_EQ) != -1)
      goto b_is_obj;

  case T_MULTISET:
  case T_PROGRAM:
  case T_ARRAY:
  case T_MAPPING:
    return a->u.refs == b->u.refs;

  case T_INT:
    return a->u.integer == b->u.integer;

  case T_STRING:
    return is_same_string(a->u.string,b->u.string);

  case T_FUNCTION:
    return (a->subtype == b->subtype && a->u.object == b->u.object);
      
  case T_FLOAT:
    return a->u.float_number == b->u.float_number;

  default:
    fatal("Unknown type %x\n",a->type);
    return 0; /* make gcc happy */
  }
}

int low_is_equal(struct svalue *a,
		 struct svalue *b,
		 struct processing *p)
{
  check_type(a->type);
  check_type(b->type);
  check_refs(a);
  check_refs(b);

  if(is_eq(a,b)) return 1;

  if(a->type != b->type) return 0;

  switch(a->type)
  {
    case T_INT:
    case T_STRING:
    case T_FLOAT:
    case T_FUNCTION:
    case T_PROGRAM:
      return 0;

    case T_OBJECT:
      return object_equal_p(a->u.object, b->u.object, p);

    case T_ARRAY:
      check_array_for_destruct(a->u.array);
      check_array_for_destruct(b->u.array);
      return array_equal_p(a->u.array, b->u.array, p);

    case T_MAPPING:
      return mapping_equal_p(a->u.mapping, b->u.mapping, p);

    case T_MULTISET:
      return multiset_equal_p(a->u.multiset, b->u.multiset, p);
      
    default:
      fatal("Unknown type in is_equal.\n");
  }
  return 1; /* survived */
}

int low_short_is_equal(const union anything *a,
		       const union anything *b,
		       TYPE_T type,
		       struct processing *p)
{
  struct svalue sa,sb;

  check_type(type);
  check_refs2(a,type);
  check_refs2(b,type);

  switch(type)
  {
    case T_INT: return a->integer == b->integer;
    case T_FLOAT: return a->float_number == b->float_number;
  }

  if((sa.u.refs=a->refs))
  {
    sa.type=type;
  }else{
    sa.type=T_INT;
    sa.u.integer=0;
  }

  if((sb.u.refs=a->refs))
  {
    sb.type=type;
  }else{
    sb.type=T_INT;
    sa.u.integer=0;
  }

  return low_is_equal(&sa,&sb,p);
}

int is_equal(struct svalue *a,struct svalue *b)
{
  return low_is_equal(a,b,0);
}

int is_lt(struct svalue *a,struct svalue *b)
{
  check_type(a->type);
  check_type(b->type);
  check_refs(a);
  check_refs(b);

  safe_check_destructed(a);
  safe_check_destructed(b);

  if (a->type != b->type)
  {
    if(a->type == T_FLOAT && b->type==T_INT)
      return a->u.float_number < (FLOAT_TYPE)b->u.integer;

    if(a->type == T_INT && b->type==T_FLOAT)
      return (FLOAT_TYPE)a->u.integer < b->u.float_number;

    if(a->type == T_OBJECT)
    {
    a_is_object:
      if(!a->u.object->prog)
	error("Comparison on destructed object.\n");
      if(FIND_LFUN(a->u.object->prog,LFUN_LT) == -1)
	error("Object lacks '<\n");
      assign_svalue_no_free(sp, b);
      sp++;
      apply_lfun(a->u.object, LFUN_LT, 1);
      if(IS_ZERO(sp-1))
      {
	pop_stack();
	return 0;
      }else{
	pop_stack();
	return 1;
      }
    }

    if(b->type == T_OBJECT)
    {
      if(!b->u.object->prog)
	error("Comparison on destructed object.\n");
      if(FIND_LFUN(b->u.object->prog,LFUN_GT) == -1)
	error("Object lacks '>\n");
      assign_svalue_no_free(sp, a);
      sp++;
      apply_lfun(b->u.object, LFUN_GT, 1);
      if(IS_ZERO(sp-1))
      {
	pop_stack();
	return 0;
      }else{
	pop_stack();
	return 1;
      }
    }
    
    error("Cannot compare different types.\n");
  }
  switch(a->type)
  {
  case T_OBJECT:
    goto a_is_object;

  default:
    error("Bad type to comparison.\n");

  case T_INT:
    return a->u.integer < b->u.integer;

  case T_STRING:
    return my_strcmp(a->u.string, b->u.string) < 0;

  case T_FLOAT:
    return a->u.float_number < b->u.float_number;

  }
}

void describe_svalue(struct svalue *s,int indent,struct processing *p)
{
  char buf[40];

  check_type(s->type);
  check_refs(s);

  indent+=2;
  switch(s->type)
  {
    case T_LVALUE:
      my_strcat("lvalue");
      break;

    case T_INT:
      sprintf(buf,"%ld",(long)s->u.integer);
      my_strcat(buf);
      break;

    case T_STRING:
      {
	int i;
        my_putchar('"');
	for(i=0; i < s->u.string->len; i++)
        {
          switch(s->u.string->str[i])
          {
	  case '\n':
	    my_putchar('\\');
	    my_putchar('n');
	    break;

	  case '\t':
	    my_putchar('\\');
	    my_putchar('t');
	    break;

	  case '\b':
	    my_putchar('\\');
	    my_putchar('b');
	    break;

	  case '\r':
	    my_putchar('\\');
	    my_putchar('r');
	    break;


            case '"':
            case '\\':
              my_putchar('\\');
              my_putchar(s->u.string->str[i]);
	      break;

            default:
	      if(is8bitalnum(((unsigned char *)s->u.string->str)[i]) ||
		 isprint(((unsigned char *)s->u.string->str)[i]))
	      {
		my_putchar(s->u.string->str[i]);
		break;
	      }

	    case 0: case 1: case 2: case 3: case 4:
	    case 5: case 6: case 7:
	      my_putchar('\\');
	      my_putchar('0');
	      my_putchar('0');
	      my_putchar('0' + s->u.string->str[i]);
	      break;
          } 
        }
        my_putchar('"');
      }
      break;


    case T_FUNCTION:
      if(s->subtype == FUNCTION_BUILTIN)
      {
	my_binary_strcat(s->u.efun->name->str,s->u.efun->name->len);
      }else{
	if(s->u.object->prog)
	{
	  struct pike_string *name;
	  name=ID_FROM_INT(s->u.object->prog,s->subtype)->name;
	  my_binary_strcat(name->str,name->len);
	}else{
	  my_strcat("0");
	}
      }
      break;

    case T_OBJECT:
      my_strcat("object");
      break;

    case T_PROGRAM:
      my_strcat("program");
      break;

    case T_FLOAT:
      sprintf(buf,"%f",(double)s->u.float_number);
      my_strcat(buf);
      break;

    case T_ARRAY:
      describe_array(s->u.array, p, indent);
      break;

    case T_MULTISET:
      describe_multiset(s->u.multiset, p, indent);
      break;

    case T_MAPPING:
      describe_mapping(s->u.mapping, p, indent);
      break;

    default:
      sprintf(buf,"<Unknown %d>",s->type);
      my_strcat(buf);
  }
}

void clear_svalues(struct svalue *s, INT32 num)
{
  struct svalue dum;
  dum.type=T_INT;
  dum.subtype=NUMBER_NUMBER;
  dum.u.refs=0;
  dum.u.integer=0;
  while(--num >= 0) *(s++)=dum;
}

void copy_svalues_recursively_no_free(struct svalue *to,
				      struct svalue *from,
				      INT32 num,
				      struct processing *p)
{
  while(--num >= 0)
  {
    check_type(from->type);
    check_refs(from);

    switch(from->type)
    {
    default:
      *to=*from;
      if(from->type <= MAX_REF_TYPE) from->u.refs[0]++;
      break;

    case T_ARRAY:
      to->u.array=copy_array_recursively(from->u.array,p);
      to->type=T_ARRAY;
      break;

    case T_MAPPING:
      to->u.mapping=copy_mapping_recursively(from->u.mapping,p);
      to->type=T_MAPPING;
      break;

    case T_MULTISET:
      to->u.multiset=copy_multiset_recursively(from->u.multiset,p);
      to->type=T_MULTISET;
      break;
    }
    to++;
    from++;
  }
}

#ifdef DEBUG
void check_short_svalue(union anything *u,TYPE_T type)
{
  static int inside=0;

  check_type(type);
  check_refs2(u,type);
  if(!u->refs) return;

  switch(type)
  {
  case T_STRING:
    if(!debug_findstring(u->string))
      fatal("Shared string not shared!\n");
    break;

  default:
    if(d_flag > 50)
    {
      if(inside) return;
      inside=1;

      switch(type)
      {
      case T_MAPPING: check_mapping(u->mapping); break;
      case T_ARRAY: check_array(u->array); break;
      case T_PROGRAM: check_program(u->program); break;
/*      case T_OBJECT: check_object(u->object); break; */
/*      case T_MULTISET: check_multiset(u->multiset); break; */
      }
      inside=0;
    }
  }
}

void check_svalue(struct svalue *s)
{
  check_refs(s);
  check_short_svalue(& s->u, s->type);
}

#endif

TYPE_FIELD gc_check_svalues(struct svalue *s, int num)
{
  INT32 e;
  TYPE_FIELD f;
  f=0;

  for(e=0;e<num;e++,s++)
  {
    check_type(s->type);
    check_refs(s);

#ifdef DEBUG
    gc_svalue_location=(void *)s;
#endif
    
    switch(s->type)
    {
    case T_FUNCTION:
      if(s->subtype == FUNCTION_BUILTIN)
      {
	if(d_flag)
	{
	  if(!gc_check(s->u.efun))
	  {
	    gc_check(s->u.efun->name);
	    gc_check(s->u.efun->type);
	  }
	}
	break;
      }

    case T_OBJECT:
      if(s->u.object->prog)
      {
#ifdef DEBUG
	if(gc_check(s->u.object) == -2)
	  fprintf(stderr,"(in svalue at %lx, type = %d)\n",(long)s,s->type);
#else	  
	  gc_check(s->u.object);
#endif
      }else{
	free_svalue(s);
	s->type=T_INT;
	s->u.integer=0;
      }
      break;

    case T_STRING:
      if(!d_flag) break;
    case T_PROGRAM:
    case T_ARRAY:
    case T_MULTISET:
    case T_MAPPING:
      gc_check(s->u.refs);
      break;

    }
    f|= 1 << s->type;
  }

#ifdef DEBUG
  gc_svalue_location=0;
#endif

  return f;
}

#ifdef DEBUG
void gc_xmark_svalues(struct svalue *s, int num)
{
  INT32 e;

  if (!s) {
    return;
  }

  for(e=0;e<num;e++,s++)
  {
    check_type(s->type);
    check_refs(s);
    
#ifdef DEBUG
    gc_svalue_location=(void *)s;
#endif

    if(s->type <= MAX_REF_TYPE)
      gc_external_mark(s->u.refs);
  }
#ifdef DEBUG
  gc_svalue_location=0;
#endif
}
#endif

void gc_check_short_svalue(union anything *u, TYPE_T type)
{
#ifdef DEBUG
  gc_svalue_location=(void *)u;
#endif
  switch(type)
  {
  case T_FUNCTION:
    fatal("Cannot have a function in a short svalue.\n");

  case T_OBJECT:
    if(!u->refs) return;
    if(u->object->prog)
    {
      gc_check(u->object);
    }else{
      free_short_svalue(u,T_OBJECT);
      u->object=0;
    }
    break;

  case T_STRING:
    if(!d_flag) break;
  case T_ARRAY:
  case T_MULTISET:
  case T_MAPPING:
  case T_PROGRAM: 
    if(!u->refs) return;
    gc_check(u->refs);
    break;
  }
#ifdef DEBUG
  gc_svalue_location=0;
#endif
}

void gc_mark_svalues(struct svalue *s, int num)
{
  INT32 e;
  for(e=0;e<num;e++,s++)
  {
    switch(s->type)
    {
    case T_ARRAY:   gc_mark_array_as_referenced(s->u.array);     break;
    case T_MULTISET:    gc_mark_multiset_as_referenced(s->u.multiset);       break;
    case T_MAPPING: gc_mark_mapping_as_referenced(s->u.mapping); break;
    case T_PROGRAM: gc_mark_program_as_referenced(s->u.program); break;

    case T_FUNCTION:
      if(s->subtype == FUNCTION_BUILTIN) break;

    case T_OBJECT:
      if(s->u.object->prog)
      {
	gc_mark_object_as_referenced(s->u.object);
      }else{
	free_svalue(s);
	s->type=T_INT;
	s->u.integer=0;
      }
      break;
    }
  }
}

void gc_mark_short_svalue(union anything *u, TYPE_T type)
{
  if(!u->refs) return;
  switch(type)
  {
  case T_ARRAY:   gc_mark_array_as_referenced(u->array);     break;
  case T_MULTISET:    gc_mark_multiset_as_referenced(u->multiset);       break;
  case T_MAPPING: gc_mark_mapping_as_referenced(u->mapping); break;
  case T_PROGRAM: gc_mark_program_as_referenced(u->program); break;

  case T_OBJECT:
    if(u->object->prog)
    {
      gc_mark_object_as_referenced(u->object);
    }else{
      free_short_svalue(u,T_OBJECT);
      u->object=0;
    }
    break;
  }
}

INT32 pike_sizeof(struct svalue *s)
{
  switch(s->type)
  {
  case T_STRING: return s->u.string->len;
  case T_ARRAY: return s->u.array->size;
  case T_MAPPING: return m_sizeof(s->u.mapping);
  case T_MULTISET: return l_sizeof(s->u.multiset);
  case T_OBJECT:
    if(!s->u.object->prog)
      error("sizeof() on destructed object.\n");
    if(FIND_LFUN(s->u.object->prog,LFUN__SIZEOF) == -1)
    {
      return s->u.object->prog->num_identifier_index;
    }else{
      apply_lfun(s->u.object, LFUN__SIZEOF, 0);
      if(sp[-1].type != T_INT)
	error("Bad return type from o->_sizeof() (not int)\n");
      sp--;
      return sp->u.integer;
    }
  default:
    error("Bad argument 1 to sizeof().\n");
    return 0; /* make apcc happy */
  }
}
