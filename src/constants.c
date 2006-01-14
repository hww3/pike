/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: constants.c,v 1.56 2006/01/14 18:18:45 nilsson Exp $
*/

#include "global.h"
#include "constants.h"
#include "pike_macros.h"
#include "program.h"
#include "pike_types.h"
#include "stralloc.h"
#include "pike_memory.h"
#include "interpret.h"
#include "mapping.h"
#include "pike_error.h"
#include "pike_security.h"
#include "block_alloc.h"

struct mapping *builtin_constants = 0;

#ifdef PIKE_DEBUG
struct callable *first_callable = NULL;
#endif

PMOD_EXPORT struct mapping *get_builtin_constants(void)
{
  return builtin_constants;
}

void low_add_efun(struct pike_string *name, struct svalue *fun)
{
  struct svalue s;

  s.type=T_STRING;
  s.subtype=0;
  s.u.string=name;

  if(fun)
  {
    mapping_insert(builtin_constants, &s, fun);
  }else{
    map_delete(builtin_constants, &s);
  }
}

void low_add_constant(const char *name, struct svalue *fun)
{
  struct pike_string *p;
  p=make_shared_string(name);
  low_add_efun(p, fun);
  free_string(p);
}

void add_pike_string_constant(const char *name, const char *str, int len)
{
  struct pike_string *key = make_shared_string(name);
  struct pike_string *val = make_shared_binary_string(str, len);
  mapping_string_insert_string(builtin_constants, key, val);
  free_string(val);
  free_string(key);
}

PMOD_EXPORT void add_global_program(const char *name, struct program *p)
{
  struct svalue s;
  s.type=T_PROGRAM;
  s.subtype=0;
  s.u.program=p;
  low_add_constant(name, &s);
}

#undef INIT_BLOCK
#define INIT_BLOCK(X) do {						\
    DO_IF_DEBUG (DOUBLELINK (first_callable, X));			\
  } while (0)

#undef EXIT_BLOCK
#define EXIT_BLOCK(X) do {		\
  DO_IF_DEBUG (DOUBLEUNLINK (first_callable, X)); \
  free_type(X->type);			\
  free_string(X->name);			\
  X->name=0;				\
  EXIT_PIKE_MEMOBJ(X);                  \
}while(0)

BLOCK_ALLOC_FILL_PAGES(callable,2)

int global_callable_flags=0;

/* Eats one ref to 'type' and 'name' */
PMOD_EXPORT struct callable *low_make_callable(c_fun fun,
				   struct pike_string *name,
				   struct pike_type *type,
				   int flags,
				   optimize_fun optimize,
				   docode_fun docode)
{
  struct callable *f=alloc_callable();
  INIT_PIKE_MEMOBJ(f);
  f->function=fun;
  f->name=name;
  f->type=type;
  f->prog=Pike_compiler->new_program;
  f->flags=flags;
  f->docode=docode;
  f->optimize=optimize;
  f->internal_flags = global_callable_flags;
#ifdef PIKE_DEBUG
  {
    struct pike_type *z = check_call(function_type_string, type, 0);
    f->may_return_void = (z == void_type_string);
    if(!z) Pike_fatal("Function has no valid return type.\n");
    free_type(z);
  }
  f->runs=0;
  f->compiles=0;
#endif
  return f;
}

PMOD_EXPORT struct callable *make_callable(c_fun fun,
			       const char *name,
			       const char *type,
			       int flags,
			       optimize_fun optimize,
			       docode_fun docode)
{
  return low_make_callable(fun, make_shared_string(name), parse_type(type),
			   flags, optimize, docode);
}

PMOD_EXPORT struct callable *add_efun2(const char *name,
			    c_fun fun,
			    const char *type,
			    int flags,
			    optimize_fun optimize,
			    docode_fun docode)
{
  struct svalue s;
  struct pike_string *n;
  struct callable *ret;

  n=make_shared_string(name);
  s.type=T_FUNCTION;
  s.subtype=FUNCTION_BUILTIN;
  ret=s.u.efun=make_callable(fun, name, type, flags, optimize, docode);
  low_add_efun(n, &s);
  free_svalue(&s);
  free_string(n);
  return ret;
}

PMOD_EXPORT struct callable *add_efun(const char *name, c_fun fun, const char *type, int flags)
{
  return add_efun2(name,fun,type,flags,0,0);
}

PMOD_EXPORT struct callable *quick_add_efun(const char *name, ptrdiff_t name_length,
					    c_fun fun,
					    const char *type, ptrdiff_t type_length,
					    int flags,
					    optimize_fun optimize,
					    docode_fun docode)
{
  struct svalue s;
  struct pike_string *n;
  struct pike_type *t;
  struct callable *ret;

#ifdef PIKE_DEBUG
  if(simple_mapping_string_lookup(builtin_constants, name))
    Pike_fatal("%s added as efun more than once.\n", name);
#endif

  n = make_shared_binary_string(name, name_length);
  t = make_pike_type(type);
#ifdef DEBUG
  check_type_string(t);
#endif
  s.type=T_FUNCTION;
  s.subtype=FUNCTION_BUILTIN;
  add_ref(n);
  ret=s.u.efun=low_make_callable(fun, n, t, flags, optimize, docode);
  mapping_string_insert(builtin_constants, n, &s);
  free_svalue(&s);
  free_string(n);
  return ret;
}

#ifdef PIKE_DEBUG
void present_constant_profiling(void)
{
  struct callable_block *b;
  size_t e;
  for(b=callable_blocks;b;b=b->next)
  {
    for(e=0;e<NELEM(b->x);e++)
    {
      if(b->x[e].name)
      {
	fprintf(stderr,"%010ld @E@: %s\n",b->x[e].runs, b->x[e].name->str);
      }
    }
  }
}
#endif

void init_builtin_constants(void)
{
  builtin_constants = allocate_mapping(300);
}

void exit_builtin_constants(void)
{
#ifdef DO_PIKE_CLEANUP
  if(builtin_constants)
  {
    free_mapping(builtin_constants);
    builtin_constants=0;
  }
#endif
}
