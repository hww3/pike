/*\
||| This file is part of Pike. For copyright information see COPYRIGHT.
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
/**/
#include "global.h"
RCSID("$Id: program.c,v 1.424 2002/05/09 14:37:45 mast Exp $");
#include "program.h"
#include "object.h"
#include "dynamic_buffer.h"
#include "pike_types.h"
#include "stralloc.h"
#include "las.h"
#include "language.h"
#include "lex.h"
#include "pike_macros.h"
#include "fsort.h"
#include "pike_error.h"
#include "docode.h"
#include "interpret.h"
#include "hashtable.h"
#include "main.h"
#include "gc.h"
#include "threads.h"
#include "constants.h"
#include "operators.h"
#include "builtin_functions.h"
#include "stuff.h"
#include "mapping.h"
#include "cyclic.h"
#include "security.h"
#include "pike_types.h"
#include "opcodes.h"
#include "version.h"
#include "block_alloc.h"
#include "pikecode.h"

#include <errno.h>
#include <fcntl.h>


#undef ATTRIBUTE
#define ATTRIBUTE(X)

static void exit_program_struct(struct program *);
static size_t add_xstorage(size_t size,
			   size_t alignment,
			   ptrdiff_t modulo_orig);

#undef EXIT_BLOCK
#define EXIT_BLOCK(P) exit_program_struct( (P) )

#undef COUNT_OTHER
#define COUNT_OTHER() do{			\
  struct program *p;				\
  for(p=first_program;p;p=p->next)		\
  {						\
    size+=p->total_size;			\
  }						\
}while(0)

BLOCK_ALLOC(program, 104)


/* #define COMPILER_DEBUG */
/* #define PROGRAM_BUILD_DEBUG */

#ifdef COMPILER_DEBUG
#define CDFPRINTF(X)	fprintf X
#else /* !COMPILER_DEBUG */
#define CDFPRINTF(X)
#endif /* COMPILER_DEBUG */

/*
 * These two values should probably be fine-tuned, but doing so
 * more or less requires running a predictable 'typical' application
 * and testing different hashsizes and tresholds. I tried to do it
 * mathematically by measuring the extremes (no cache hits, 100%
 * cache hits etc.) but it seems that the processor cache becomes
 * exhausted in some of my measurements, which renders my mathematical
 * model useless.
 *
 * Further measurements seems to indicate that this cache can slow
 * things down a bit if the hit/miss rate is not fairly high.
 * For normal applications, the hitrate is most likely well over 90%,
 * but that should be verified.
 * - Holistiska Centralbyr�n (Hubbe)
 */

/* Define the size of the cache that is used for method lookup. */
/* A value of zero disables this cache */
#define FIND_FUNCTION_HASHSIZE 15013

/* Programs with less methods will not use the cache for method lookups.. */
#define FIND_FUNCTION_HASH_TRESHOLD 9


#define DECLARE
#include "compilation.h"

struct pike_string *this_program_string=0;
static struct pike_string *UNDEFINED_string=0;

char *lfun_names[] = {
  "__INIT",
  "create",
  "destroy",
  "`+",
  "`-",
  "`&",
  "`|",
  "`^",
  "`<<",
  "`>>",
  "`*",
  "`/",
  "`%",
  "`~",
  "`==",
  "`<",
  "`>",
  "__hash",
  "cast",
  "`!",
  "`[]",
  "`[]=",
  "`->",
  "`->=",
  "_sizeof",
  "_indices",
  "_values",
  "`()",
  "``+",
  "``-",
  "``&",
  "``|",
  "``^",
  "``<<",
  "``>>",
  "``*",
  "``/",
  "``%",
  "`+=",
  "_is_type",
  "_sprintf",
  "_equal",
  "_m_delete",
  "_get_iterator",
};

struct pike_string *lfun_strings[NELEM(lfun_names)];

static struct mapping *lfun_ids;

/* mapping(string:type) */
static struct mapping *lfun_types;

static char *raw_lfun_types[] = {
  tFuncV(tNone,tVoid,tVoid),	/* "__INIT", */
  tFuncV(tNone,tZero,tVoid),	/* "create", */
  tFuncV(tNone,tVoid,tVoid),	/* "destroy", */
  tFuncV(tNone,tZero,tMix),	/* "`+", */
  tFuncV(tNone,tZero,tMix),	/* "`-", */
  tFuncV(tNone,tZero,tMix),	/* "`&", */
  tFuncV(tNone,tZero,tMix),	/* "`|", */
  tFuncV(tNone,tZero,tMix),	/* "`^", */
  tFuncV(tZero,tVoid,tMix),	/* "`<<", */
  tFuncV(tZero,tVoid,tMix),	/* "`>>", */
  tFuncV(tNone,tZero,tMix),	/* "`*", */
  tFuncV(tNone,tZero,tMix),	/* "`/", */
  tFuncV(tNone,tZero,tMix),	/* "`%", */
  tFuncV(tNone,tVoid,tMix),	/* "`~", */
  tFuncV(tMix,tVoid,tInt),	/* "`==", */
  tFuncV(tMix,tVoid,tInt),	/* "`<", */
  tFuncV(tMix,tVoid,tInt),	/* "`>", */
  tFuncV(tNone,tVoid,tInt),	/* "__hash", */
  tFuncV(tString,tVoid,tMix),	/* "cast", */
  tFuncV(tNone,tVoid,tInt),	/* "`!", */
  tFuncV(tZero,tVoid,tMix),	/* "`[]", */
  tFuncV(tZero tSetvar(0,tZero),tVoid,tVar(0)),	/* "`[]=", */
  tFuncV(tStr,tVoid,tMix),	/* "`->", */
  tFuncV(tStr tSetvar(0,tZero),tVoid,tVar(0)),	/* "`->=", */
  tFuncV(tNone,tVoid,tInt),	/* "_sizeof", */
  tFuncV(tNone,tVoid,tArray),	/* "_indices", */
  tFuncV(tNone,tVoid,tArray),	/* "_values", */
  tFuncV(tNone,tZero,tMix),	/* "`()", */
  tFuncV(tNone,tZero,tMix),	/* "``+", */
  tFuncV(tNone,tZero,tMix),	/* "``-", */
  tFuncV(tNone,tZero,tMix),	/* "``&", */
  tFuncV(tNone,tZero,tMix),	/* "``|", */
  tFuncV(tNone,tZero,tMix),	/* "``^", */
  tFuncV(tZero,tVoid,tMix),	/* "``<<", */
  tFuncV(tZero,tVoid,tMix),	/* "``>>", */
  tFuncV(tNone,tZero,tMix),	/* "``*", */
  tFuncV(tNone,tZero,tMix),	/* "``/", */
  tFuncV(tNone,tZero,tMix),	/* "``%", */
  tFuncV(tZero,tVoid,tMix),	/* "`+=", */
  tFuncV(tStr,tVoid,tInt),	/* "_is_type", */
  tFuncV(tInt tOr(tMap(tStr,tInt),tVoid),tVoid,tStr),	/* "_sprintf", */
  tFuncV(tMix,tVoid,tInt),	/* "_equal", */
  tFuncV(tZero,tVoid,tMix),	/* "_m_delete", */
  tFuncV(tNone,tVoid,tObj),	/* "_get_iterator", */
};

/*! @module lfun::
 *!
 *! @endmodule
 */

/*! @decl void lfun::__INIT()
 *!
 *!   Global variable initialization.
 *!   
 *!   This function is called just before @[lfun::create()] when
 *!   an object is instanciated.
 *!
 *! @note
 *!   This function is generated automatically by the compiler,
 *!   and can not be overloaded.
 *!
 *! @seealso
 *!   @[lfun::create()]
 */

/*! @decl void lfun::create(zero ... args)
 *!
 *!   Object creation callback.
 *!   
 *!   This function is called right after @[lfun::__INIT()].
 *!   
 *!   @[args] will be the arguments passed when the program was called.
 *!
 *! @note
 *!   In Pike 7.2 and later this function can be created implicitly
 *!   by the compiler using the new syntax:
 *!     @code{
 *!       class Foo(int foo) {
 *!         int bar;
 *!       }
 *!     @}
 *!   In the above case an implicit @[lfun::create()] will be created,
 *!   and it will be equvivalent to:
 *!     @code{
 *!       class Foo {
 *!         int foo;
 *!         int bar;
 *!         static void create(int foo)
 *!         {
 *!           local::foo = foo;
 *!         }
 *!       }
 *!     @}
 *!
 *! @seealso
 *!   @[lfun::__INIT()], @[lfun::destroy()]
 */

/*! @decl void lfun::destroy()
 *!
 *!   Object destruction callback.
 *!   
 *!   This function is called by @[destruct()] right before
 *!   it will zero all the object variables, and destroy the object.
 *!
 *! @note
 *!   Note that it will also be called on implicit destruct, eg
 *!   when there are no more references to the object, or when
 *!   the garbage-collector decides to destruct the object.
 *!
 *! @seealso
 *!   @[lfun::create()], @[destruct()]
 */

/*! @decl mixed lfun::`+(zero ... args)
 *!
 *!   Left associative addition operator callback.
 *!
 *! @seealso
 *!   @[lfun::``+()], @[`+()]
 */

/*! @decl mixed lfun::`-(zero ... args)
 *!
 *!   Left associative subtraction operator callback.
 *!
 *! @seealso
 *!   @[lfun::``-()], @[`-()]
 */

/*! @decl mixed lfun::`&(zero ... args)
 *!
 *!   Left associative and operator callback.
 *!
 *! @seealso
 *!   @[lfun::``&()], @[`&()]
 */

/*! @decl mixed lfun::`|(zero ... args)
 *!
 *!   Left associative or operator callback.
 *!
 *! @seealso
 *!   @[lfun::``|()], @[`|()]
 */

/*! @decl mixed lfun::`^(zero ... args)
 *!
 *!   Left associative exclusive or operator callback.
 *!
 *! @seealso
 *!   @[lfun::``^()], @[`^()]
 */

/*! @decl mixed lfun::`<<(zero arg)
 *!
 *!   Left associative left shift operator callback.
 *!
 *! @seealso
 *!   @[lfun::``<<()], @[`<<()]
 */

/*! @decl mixed lfun::`>>(zero arg)
 *!
 *!   Left associative right shift operator callback.
 *!
 *! @seealso
 *!   @[lfun::``>>()], @[`>>()]
 */

/*! @decl mixed lfun::`*(zero ... args)
 *!
 *!   Left associative multiplication operator callback.
 *!
 *! @seealso
 *!   @[lfun::``*()], @[`*()]
 */

/*! @decl mixed lfun::`/(zero ... args)
 *!
 *!   Left associative division operator callback.
 *!
 *! @seealso
 *!   @[lfun::``/()], @[`/()]
 */

/*! @decl mixed lfun::`%(zero ... args)
 *!
 *!   Left associative modulo operator callback.
 *!
 *! @seealso
 *!   @[lfun::``%()], @[`%()]
 */

/*! @decl mixed lfun::`~()
 *!
 *!   Inversion operator callback.
 *!
 *! @seealso
 *!   @[`~()]
 */

/*! @decl int(0..1) lfun::`==(mixed arg)
 *!
 *!   Equality operator callback.
 *!
 *! @seealso
 *!   @[`==()]
 */

/*! @decl int(0..1) lfun::`<(mixed arg)
 *!
 *!   Less than operator callback.
 *!
 *! @seealso
 *!   @[`<()]
 */

/*! @decl int(0..1) lfun::`>(mixed arg)
 *!
 *!   Greater than operator callback.
 *!
 *! @seealso
 *!   @[`>()]
 */

/*! @decl int lfun::__hash()
 *!
 *!   Hashing callback.
 *!
 *!   This function gets called by various mapping operations when the
 *!   object is used as index in a mapping.
 */

/*! @decl mixed lfun::cast(string requested_type)
 *!
 *!   Cast operator callback.
 *!
 *! @param requested_type
 *!   Type to cast to.
 *!
 *! @returns
 *!   Expected to return the object value-casted (converted) to
 *!   the type described by @[requested_type].
 *!
 *! @note
 *!   The argument is currently a string with the name
 *!   of the type, but might in the future be a value of the type type.
 *!
 *! @note
 *!   Currently casting between object types is a noop.
 *!
 *! @note
 *!   If the returned value is not deemed to be of the requested type
 *!   a runtime error may be thrown.
 */

/*! @decl int lfun::`!()
 *!
 *!   Not operator callback.
 *!
 *! @returns
 *!   Returns non-zero if the object should be evaluated as false,
 *!   and @tt{0@} (zero) otherwise.
 *!
 *! @seealso
 *!   @[`!()]
 */

/*! @decl mixed lfun::`[](zero arg1, zero|void arg2)
 *!
 *!   Index/range operator callback.
 *!
 *! @seealso
 *!   @[`[]()]
 */

/*! @decl mixed lfun::`[]=(zero arg1, zero arg2)
 *!
 *!   Index assignment operator callback.
 *!
 *! @seealso
 *!   @[`[]=()], @[lfun::`->=()]
 */

/*! @decl mixed lfun::`->(string arg)
 *!
 *!   Arrow index operator callback.
 *!
 *! @seealso
 *!   @[`->()]
 */

/*! @decl mixed lfun::`->=(string arg1, zero arg2)
 *!
 *!   Arrow index assign operator callback.
 *!
 *! @seealso
 *!   @[`->=()], @[lfun::`[]=()]
 */

/*! @decl int lfun::_sizeof()
 *!
 *!   Sizeof operator callback.
 *!
 *!   Called by @[sizeof()] to determine the number of elements
 *!   in an object. If this function is not present, the number
 *!   of public symbols in the object will be returned.
 *!
 *! @returns
 *!   Expected to return the number of valid indices in the object.
 *!
 *! @seealso
 *!   @[sizeof()]
 */

/*! @decl array lfun::_indices()
 *!
 *!   Indices operator callback.
 *!
 *! @returns
 *!   Expected to return an array with the valid indices in the object.
 *!
 *! @seealso
 *!   @[indices()], @[lfun::_values()]
 */

/*! @decl array lfun::_values()
 *!
 *!   Values operator callback.
 *!
 *! @returns
 *!   Expected to return an array with the values corresponding to
 *!   the indices returned by @[lfun::_indices()].
 *!
 *! @seealso
 *!   @[values()], @[lfun::_indices()]
 */

/*! @decl mixed lfun::`()(zero ... args)
 *!
 *!   Function call operator callback.
 *!
 *! @seealso
 *!   @[`()]
 */

/*! @decl mixed lfun::``+(zero ... args)
 *!
 *!   Right associative addition operator callback.
 *!
 *! @seealso
 *!   @[lfun::`+()], @[`+()]
 */

/*! @decl mixed lfun::``-(zero ... args)
 *!
 *!   Right associative subtraction operator callback.
 *!
 *! @seealso
 *!   @[lfun::`-()], @[`-()]
 */

/*! @decl mixed lfun::``&(zero ... args)
 *!
 *!   Right associative and operator callback.
 *!
 *! @seealso
 *!   @[lfun::`&()], @[`&()]
 */

/*! @decl mixed lfun::``|(zero ... args)
 *!
 *!   Right associative or operator callback.
 *!
 *! @seealso
 *!   @[lfun::`|()], @[`|()]
 */

/*! @decl mixed lfun::``^(zero ... args)
 *!
 *!   Right associative exclusive or operator callback.
 *!
 *! @seealso
 *!   @[lfun::`^()], @[`^()]
 */

/*! @decl mixed lfun::``<<(zero arg)
 *!
 *!   Right associative left shift operator callback.
 *!
 *! @seealso
 *!   @[lfun::`<<()], @[`<<()]
 */

/*! @decl mixed lfun::``>>(zero arg)
 *!
 *!   Right associative right shift operator callback.
 *!
 *! @seealso
 *!   @[lfun::`>>()], @[`>>()]
 */

/*! @decl mixed lfun::``*(zero ... args)
 *!
 *!   Right associative multiplication operator callback.
 *!
 *! @seealso
 *!   @[lfun::`*()], @[`*()]
 */

/*! @decl mixed lfun::``/(zero ... args)
 *!
 *!   Right associative division operator callback.
 *!
 *! @seealso
 *!   @[lfun::`/()], @[`/()]
 */

/*! @decl mixed lfun::``%(zero ... args)
 *!
 *!   Right associative modulo operator callback.
 *!
 *! @seealso
 *!   @[lfun::`%()], @[`%()]
 */

/*! @decl mixed lfun::`+=(zero arg)
 *!
 *!   Self increment operator callback.
 *!
 *! @seealso
 *!   @[`+()], @[lfun::`+()]
 */

/*! @decl int(0..1) lfun::_is_type(string basic_type)
 *!
 *!   Type comparison callback.
 *!
 *!   Called by the cast operator to determine if an object
 *!   simulates a basic type.
 *!
 *! @param basic_type
 *!   One of:
 *!   @string
 *!     @value "array"
 *!     @value "float"
 *!     @value "function"
 *!     @value "int"
 *!     @value "mapping"
 *!     @value "multiset"
 *!     @value "object"
 *!     @value "program"
 *!     @value "string"
 *!     @value "type"
 *!     @value "void"
 *!     @value "zero"
 *!   @endstring
 *!
 *!   The following five shouldn't occurr, but are here for completeness:
 *!   @string
 *!     @value "lvalue"
 *!     @value "mapping_data"
 *!     @value "object storage"
 *!     @value "pike_frame"
 *!     @value "unknown"
 *!   @endstring
 *!
 *! @returns
 *!   Expected to return @tt{1@} if the object is to be regarded as a
 *!   simulation of the type specified by @[basic_type].
 *!
 *! @note
 *!   The argument is currently a string with the name
 *!   of the type, but might in the future be a value of the type type.
 */

/*! @decl string lfun::_sprintf(int conversion_type, @
 *!                             mapping(string:int)|void params)
 *!
 *!   Sprintf callback.
 *!
 *!   This method is called by @[sprintf()] to print objects. If it is
 *!   not present, printing of the object will not be supported for any
 *!   conversion-type except for the @tt{%O@}-conversion-type, which
 *!   will output @tt{"object"@}.
 *!
 *! @param conversion_type
 *!   One of:
 *!   @int
 *!     @value 'b'
 *!       Signed binary integer.
 *!     @value 'd'
 *!       Signed decimal integer.
 *!     @value 'u'
 *!       Unsigned decimal integer.
 *!     @value 'o'
 *!       Signed octal integer.
 *!     @value 'x'
 *!       Lowercase signed hexadecimal integer.
 *!     @value 'X'
 *!       Uppercase signed hexadecimal integer.
 *!     @value 'c'
 *!       Character. If a fieldsize has been specified this will output
 *!       the low-order bytes of the integer in network byte order.
 *!     @value 'f'
 *!       Float.
 *!     @value 'g'
 *!       Heuristically chosen representation of float.
 *!     @value 'G'
 *!       Like @tt{%g@}, but uses uppercase @tt{E@} for exponent.
 *!     @value 'e'
 *!       Exponential notation float.
 *!     @value 'E'
 *!       Like @tt{%e@}, but uses uppercase @tt{E@} for exponent.
 *!     @value 's'
 *!       String.
 *!     @value 'O'
 *!       Any value (debug style).
 *!     @value 't'
 *!       Type of the argument.
 *!   @endint
 *!
 *! @param params
 *!   Conversion parameters. The following parameters may be supplied:
 *!   @mapping
 *!     @member int "precision"
 *!       Precision.
 *!     @member int "width"
 *!       Field width.
 *!     @member int(1..1) "flag_left"
 *!       Indicates that the output should be left-aligned.
 *!     @member int "indent"
 *!       Indentation level in @tt{%O@}-mode.
 *!   @endmapping
 *!
 *! @returns
 *!   Is expected to return a string describing the object formatted
 *!   according to @[conversion_type].
 *!
 *! @note
 *!   @[_sprintf()] is currently not called for the following
 *!   conversion-types:
 *!   @int
 *!     @value 'F'
 *!       Binary IEEE representation of float (@tt{%4F@} gives 
 *!       single precision, @tt{%8F@} gives double precision.)
 *!   @endint
 *!
 *! @seealso
 *!    @[sprintf()]
 */

/*! @decl int lfun::_equal(mixed arg)
 *!
 *!   Equal callback.
 *!
 *! @seealso
 *!   @[equal()], @[lfun::`==()]
 */

/*! @decl mixed lfun::_m_delete(mixed arg)
 *!
 *!   Mapping delete callback.
 *!
 *! @seealso
 *!   @[m_delete()]
 */

/*! @decl object lfun::_get_iterator()
 *!
 *!   Iterator creation callback.
 *!
 *! @returns
 *!   Should return an object that implements the iterator API:
 *!   @dl
 *!   	@item
 *!   	  @[lfun::`!()] should return @tt{0@} (zero) when not at end of stream,
 *!   	  and @tt{1@} at end of stream.
 *!   	@item
 *!   	  @[lfun::`+=()] should advance the specified number of steps.
 *!   	@item
 *!   	  @[index()] should return the current index.
 *!   	@item
 *!   	  @[value()] should return the current value.
 *!   @enddl
 *!
 *! @seealso
 *!   @[Iterator], @[foreach()]
 */

struct program *first_program = 0;
static int current_program_id = PROG_DYNAMIC_ID_START;

struct program *null_program=0;

struct object *error_handler=0;
struct object *compat_handler=0;

struct program *gc_internal_program = 0;
static struct program *gc_mark_program_pos = 0;

int compilation_depth=-1;
dynamic_buffer used_modules;
static struct mapping *resolve_cache=0;

int get_small_number(char **q);

/* So what if we don't have templates? / Hubbe */

#ifdef PIKE_DEBUG
#define CHECK_FOO(NUMTYPE,TYPE,NAME)				\
  if(Pike_compiler->malloc_size_program-> PIKE_CONCAT(num_,NAME) < Pike_compiler->new_program-> PIKE_CONCAT(num_,NAME))	\
    fatal("Pike_compiler->new_program->num_" #NAME " is out of order\n");	\
  if(Pike_compiler->new_program->flags & PROGRAM_OPTIMIZED)			\
    fatal("Tried to reallocate fixed program.\n")

#else
#define CHECK_FOO(NUMTYPE,TYPE,NAME)
#endif

#ifndef RELOCATE_program
#define RELOCATE_program(ORIG, NEW)
#endif /* !RELOCATE_program */
#define RELOCATE_linenumbers(ORIG,NEW)
#define RELOCATE_identifier_index(ORIG,NEW)
#define RELOCATE_variable_index(ORIG,NEW)
#define RELOCATE_identifier_references(ORIG,NEW)
#define RELOCATE_strings(ORIG,NEW)
#define RELOCATE_inherits(ORIG,NEW)
#define RELOCATE_identifiers(ORIG,NEW)
#define RELOCATE_constants(ORIG,NEW)
#define RELOCATE_relocations(ORIG,NEW)

#define FOO(NUMTYPE,TYPE,NAME)						\
void PIKE_CONCAT(low_add_to_,NAME) (struct program_state *state,	\
                                    TYPE ARG) {				\
  if(state->malloc_size_program->PIKE_CONCAT(num_,NAME) ==		\
     state->new_program->PIKE_CONCAT(num_,NAME)) {			\
    TYPE *tmp;								\
    state->malloc_size_program->PIKE_CONCAT(num_,NAME) *= 2;		\
    state->malloc_size_program->PIKE_CONCAT(num_,NAME)++;		\
    tmp = realloc((void *)state->new_program->NAME,			\
		  sizeof(TYPE) *					\
		  state->malloc_size_program->				\
                  PIKE_CONCAT(num_,NAME));				\
    if(!tmp) fatal("Out of memory.\n");					\
    PIKE_CONCAT(RELOCATE_,NAME)(state->new_program, tmp);		\
    state->new_program->NAME=tmp;					\
  }									\
  state->new_program->							\
    NAME[state->new_program->PIKE_CONCAT(num_,NAME)++]=(ARG);		\
}									\
void PIKE_CONCAT(add_to_,NAME) (TYPE ARG) {				\
  CHECK_FOO(NUMTYPE,TYPE,NAME);						\
  PIKE_CONCAT(low_add_to_,NAME) ( Pike_compiler, ARG );			\
}


#include "program_areas.h"


void ins_int(INT32 i, void (*func)(char tmp))
{
  int e;
  unsigned char *p = (unsigned char *)&i;
  for(e=0;e<(long)sizeof(i);e++) {
    func(p[e]);
  }
}

void ins_short(INT16 i, void (*func)(char tmp))
{
  int e;
  unsigned char *p = (unsigned char *)&i;
  for(e=0;e<(long)sizeof(i);e++) {
    func(p[e]);
  }
}

#ifdef PIKE_DEBUG
static void debug_add_to_identifiers (struct identifier id)
{
  if (d_flag) {
    int i;
    for (i = 0; i < Pike_compiler->new_program->num_identifiers; i++)
      if (Pike_compiler->new_program->identifiers[i].name == id.name) {
	extern void dump_program_tables (struct program *p, int indent);
	dump_program_tables (Pike_compiler->new_program, 0);
	fatal ("Adding identifier twice, old at %d.\n", i);
      }
  }
  add_to_identifiers (id);
}
#else
#define debug_add_to_identifiers(ARG) add_to_identifiers(ARG)
#endif


void use_module(struct svalue *s)
{
  if( (1<<s->type) & (BIT_MAPPING | BIT_OBJECT | BIT_PROGRAM))
  {
    Pike_compiler->num_used_modules++;
    assign_svalue_no_free((struct svalue *)
			  low_make_buf_space(sizeof(struct svalue),
					     &used_modules), s);
    if(Pike_compiler->module_index_cache)
    {
      free_mapping(Pike_compiler->module_index_cache);
      Pike_compiler->module_index_cache=0;
    }
  }else{
    yyerror("Module is neither mapping nor object");
  }
}

void unuse_modules(INT32 howmany)
{
  if(!howmany) return;
#ifdef PIKE_DEBUG
  if(howmany *sizeof(struct svalue) > used_modules.s.len)
    fatal("Unusing too many modules.\n");
#endif
  Pike_compiler->num_used_modules-=howmany;
  low_make_buf_space(-sizeof(struct svalue)*howmany, &used_modules);
  free_svalues((struct svalue *)low_make_buf_space(0, &used_modules),
	       howmany,
	       BIT_MAPPING | BIT_OBJECT | BIT_PROGRAM);
  if(Pike_compiler->module_index_cache)
  {
    free_mapping(Pike_compiler->module_index_cache);
    Pike_compiler->module_index_cache=0;
  }
}

int low_find_shared_string_identifier(struct pike_string *name,
				      struct program *prog);



static struct node_s *index_modules(struct pike_string *ident,
				    struct mapping **module_index_cache,
				    int num_used_modules,
				    struct svalue *modules)
{
  if(*module_index_cache)
  {
    struct svalue *tmp=low_mapping_string_lookup(*module_index_cache,ident);
    if(tmp)
    {
      if(!(SAFE_IS_ZERO(tmp) && tmp->subtype==1))
	return mksvaluenode(tmp);
      return 0;
    }
  }

/*  fprintf(stderr,"index_module: %s\n",ident->str); */

  {
    JMP_BUF tmp;

    if(SETJMP(tmp))
    {
      struct svalue thrown = throw_value;
      throw_value.type = T_INT;

      if (!ident->size_shift) {
	my_yyerror("Couldn't index module '%s'.", ident->str);
      } else {
	yyerror("Couldn't index module.");
      }

      push_svalue(&thrown);
      low_safe_apply_handler("compile_exception", error_handler, compat_handler, 1);
      if (SAFE_IS_ZERO(sp-1)) yy_describe_exception(&thrown);
      pop_stack();
      free_svalue(&thrown);
    } else {
      int e = num_used_modules;
      struct svalue *m = modules - num_used_modules;

      while(--e>=0)
      {
	push_svalue(m+e);
	ref_push_string(ident);
	f_index(2);

	if(!IS_UNDEFINED(Pike_sp-1))
	{
	  struct node_s *ret;

	  UNSETJMP(tmp);
	  if(!*module_index_cache)
	    *module_index_cache = allocate_mapping(10);
	  mapping_string_insert(*module_index_cache, ident, Pike_sp-1);
	  ret = mksvaluenode(Pike_sp-1);
	  pop_stack();
	  return ret;
	}
	pop_stack();
      }
    }
    UNSETJMP(tmp);
  }

/*  fprintf(stderr,"***Undefined.\n"); */

  return 0;
}

struct node_s *resolve_identifier(struct pike_string *ident);

struct node_s *find_module_identifier(struct pike_string *ident,
				      int see_inherit)
{
  struct node_s *ret;

  struct svalue *modules=(struct svalue *)
    (used_modules.s.str + used_modules.s.len);

  if((ret=index_modules(ident,
			&Pike_compiler->module_index_cache,
			Pike_compiler->num_used_modules,
			modules))) return ret;
  modules-=Pike_compiler->num_used_modules;

  {
    struct program_state *p=Pike_compiler->previous;
    int n;
    for(n=0;n<compilation_depth;n++,p=p->previous)
    {
      int i;
      if(see_inherit)
      {
	i=really_low_find_shared_string_identifier(ident,
						   p->new_program,
						   SEE_STATIC);
	if(i!=-1)
	{
	  return mkexternalnode(p->new_program, i);
	}
      }
      
      if((ret=index_modules(ident,
			    &p->module_index_cache,
			    p->num_used_modules,
			    modules))) return ret;
      modules-=p->num_used_modules;
#ifdef PIKE_DEBUG
      if( ((char *)modules ) < used_modules.s.str)
	fatal("Modules out of whack!\n");
#endif
    }
  }

  /* Handle this_program */
  if (ident == this_program_string) {
    if (compilation_depth > 0)
      return mkexternalnode(Pike_compiler->previous->new_program,
			    Pike_compiler->previous->parent_identifier);
    else {
      struct svalue s;
      s.type=T_PROGRAM;
      s.u.program=Pike_compiler->new_program;
      return mkconstantsvaluenode(&s);
    }
  }
  /* Handle UNDEFINED */
  if (ident == UNDEFINED_string) {
    struct svalue s;
    s.type = T_INT;
    s.subtype = NUMBER_UNDEFINED;
    s.u.integer = 0;
    return mkconstantsvaluenode(&s);
  }

  return resolve_identifier(ident);
}

struct node_s *resolve_identifier(struct pike_string *ident)
{
  struct node_s *ret;

  if(resolve_cache)
  {
    struct svalue *tmp=low_mapping_string_lookup(resolve_cache,ident);
    if(tmp)
    {
      if(!(SAFE_IS_ZERO(tmp) && tmp->subtype==1))
	return mkconstantsvaluenode(tmp);

      return 0;
    }
  }

  if(!Pike_compiler->num_parse_error && get_master())
  {
    DECLARE_CYCLIC();
    node *ret=0;
    if(BEGIN_CYCLIC(ident, lex.current_file))
    {
      my_yyerror("Recursive module dependency in %s.",
		 ident->str);
    }else{
      int i;
      SET_CYCLIC_RET(1);

      ref_push_string(ident);
      ref_push_string(lex.current_file);
      if (error_handler) {
	ref_push_object(error_handler);
      } else {
	push_int(0);
      }

      if (safe_apply_handler("resolv", error_handler, compat_handler, 3, 0)) {
	if(!resolve_cache)
	  resolve_cache=dmalloc_touch(struct mapping *, allocate_mapping(10));
	mapping_string_insert(resolve_cache,ident,Pike_sp-1);

	if(!(SAFE_IS_ZERO(Pike_sp-1) && Pike_sp[-1].subtype==1))
	{
	  ret=mkconstantsvaluenode(Pike_sp-1);
	}
	pop_stack();
      }
      else
	if(Pike_compiler->compiler_pass==2) {
	  if (throw_value.type == T_STRING && !throw_value.u.string->size_shift) {
	    yyerror(throw_value.u.string->str);
	    free_svalue(&throw_value);
	    throw_value.type = T_INT;
	  }
	  else {
	    struct svalue thrown = throw_value;
	    throw_value.type = T_INT;

	    if (!ident->size_shift)
	      my_yyerror("Error resolving '%s'.", ident->str);
	    else
	      yyerror("Error resolving identifier.");

	    push_svalue(&thrown);
	    low_safe_apply_handler("compile_exception", error_handler, compat_handler, 1);
	    if (SAFE_IS_ZERO(sp-1)) yy_describe_exception(&thrown);
	    pop_stack();
	    free_svalue(&thrown);
	  }
	}
    }
    END_CYCLIC();
    if(ret) return ret;
  }

  return 0;
}

/* Fixme: allow level=0 to return the current level */
struct program *parent_compilation(int level)
{
  int n;
  struct program_state *p=Pike_compiler->previous;
  for(n=0;n<level;n++)
  {
    if(n>=compilation_depth) return 0;
    p=p->previous;
    if(!p) return 0;
  }
  return p->new_program;
}

#define ID_TO_PROGRAM_CACHE_SIZE 512
struct program *id_to_program_cache[ID_TO_PROGRAM_CACHE_SIZE];

struct program *id_to_program(INT32 id)
{
  struct program *p;
  INT32 h;
  if(!id) return 0;
  h=id & (ID_TO_PROGRAM_CACHE_SIZE-1);

  if((p=id_to_program_cache[h]))
    if(p->id==id)
      return p;

  for(p=first_program;p;p=p->next)
  {
    if(id==p->id)
    {
      id_to_program_cache[h]=p;
      return p;
    }
  }

  if ((id > 0) && (id < PROG_DYNAMIC_ID_START)) {
    /* Reserved id. Attempt to load the proper dynamic module
     * to resolv the id.
     */
    char *module = NULL;

    switch(id) {
    case PROG_PARSER_HTML_ID:
      module = "Parser._parser";
      break;
    case PROG_GMP_MPZ_ID:
      module = "Gmp";
      break;
    case PROG_MODULE_MIME_ID:
      module = "___MIME";
      break;
    default:
      if ((id >= 100) && (id <= 300)) {
	module = "Image";
      } else if ((id >= 1000) && (id <= 2000)) {
	module = "GTK";
      }
      break;
    }
    if (module) {
      push_text(module);
      SAFE_APPLY_MASTER("resolv", 1);
      pop_stack();

      /* Try again... */
      for(p=first_program;p;p=p->next)
      {
	if(id==p->id)
	{
	  id_to_program_cache[h]=p;
	  return p;
	}
      }
    }
  }
  return 0;
}

/* Here starts routines which are used to build new programs */

/* Re-allocate all the memory in the program in one chunk. because:
 * 1) The individual blocks are munch bigger than they need to be
 * 2) cuts down on malloc overhead (maybe)
 * 3) localizes memory access (decreases paging)
 */
void optimize_program(struct program *p)
{
  size_t size=0;
  char *data;

  /* Already done (shouldn't happen, but who knows?) */
  if(p->flags & PROGRAM_OPTIMIZED) return;

#define FOO(NUMTYPE,TYPE,NAME) \
  size=DO_ALIGN(size, ALIGNOF(TYPE)); \
  size+=p->PIKE_CONCAT(num_,NAME)*sizeof(p->NAME[0]);
#include "program_areas.h"

  data=malloc(size);
  if(!data) 
  {
    make_program_executable(p);
    return; /* We are out of memory, but we don't care! */
  }

  size=0;

#define FOO(NUMTYPE,TYPE,NAME) \
  size=DO_ALIGN(size, ALIGNOF(TYPE)); \
  MEMCPY(data+size,p->NAME,p->PIKE_CONCAT(num_,NAME)*sizeof(p->NAME[0])); \
  PIKE_CONCAT(RELOCATE_,NAME)(p, (TYPE *)(data+size)); \
  dmfree((char *)p->NAME); \
  p->NAME=(TYPE *)(data+size); \
  size+=p->PIKE_CONCAT(num_,NAME)*sizeof(p->NAME[0]);
#include "program_areas.h"

  p->total_size=size + sizeof(struct program);

  p->flags |= PROGRAM_OPTIMIZED;
  make_program_executable(p);
}

/* internal function to make the index-table */
int program_function_index_compare(const void *a,const void *b)
{
  return
    DO_NOT_WARN((int)my_order_strcmp(ID_FROM_INT(Pike_compiler->new_program,
						 *(unsigned short *)a)->name,
				     ID_FROM_INT(Pike_compiler->new_program,
						 *(unsigned short *)b)->name));
}

#ifdef PIKE_DEBUG
struct pike_string *find_program_name(struct program *p, INT32 *line)
{
  ptrdiff_t pos;
  INT32 l;
  if(!line) line=&l;

#ifdef DEBUG_MALLOC
  {
    char *tmp=dmalloc_find_name(p);
    *line=0;
    if(tmp) return make_shared_string(tmp);
  }
#endif

  return get_program_line(p, line);
}
#endif

int override_identifier (struct reference *ref, struct pike_string *name, int cur_id)
{
  int id = -1;
  /* This loop could possibly be optimized by looping over
   * each inherit and looking up 'name' in each inherit
   * and then see if should be overwritten
   * /Hubbe
   */

  for(;cur_id<Pike_compiler->new_program->num_identifier_references;cur_id++)
  {
    /* Do not zapp hidden identifiers */
    if(Pike_compiler->new_program->identifier_references[cur_id].id_flags & ID_HIDDEN)
      continue;

    /* Do not zapp inherited inline ('local') identifiers */
    if((Pike_compiler->new_program->identifier_references[z].id_flags &
	(ID_INLINE|ID_INHERITED)) == (ID_INLINE|ID_INHERITED))
      continue;

    /* Do not zapp functions with the wrong name... */
    if(ID_FROM_INT(Pike_compiler->new_program, cur_id)->name != name)
      continue;

#ifdef PROGRAM_BUILD_DEBUG
    fprintf(stderr, "%.*soverloaded reference %d (id_flags:0x%04x)\n",
	    compilation_depth, "", cur_id,
	    Pike_compiler->new_program->identifier_references[cur_id].id_flags);
#endif

    Pike_compiler->new_program->identifier_references[cur_id]=*ref;
    id = cur_id;
  }

  return id;
}

void fixate_program(void)
{
  INT32 i,e,t;
  struct program *p=Pike_compiler->new_program;

  if(p->flags & PROGRAM_FIXED) return;
#ifdef PIKE_DEBUG
  if(p->flags & PROGRAM_OPTIMIZED)
    fatal("Cannot fixate optimized program\n");
#endif

  /* Ok, sort for binsearch */
  for(e=i=0;i<(int)p->num_identifier_references;i++)
  {
    struct reference *funp;
    struct identifier *fun;
    funp=p->identifier_references+i;
    if(funp->id_flags & (ID_HIDDEN|ID_STATIC)) continue;
    fun=ID_FROM_PTR(p, funp);
    if(funp->id_flags & ID_INHERITED)
    {
      int found_better=-1;
      int funa_is_prototype;

      if(funp->id_flags & ID_PRIVATE) continue;
      funa_is_prototype = fun->func.offset == -1;
/*    if(fun->func.offset == -1) continue; * prototype */

      /* check for multiple definitions */
      for(t=i+1;t<(int)p->num_identifier_references;t++)
      {
	struct reference *funpb;
	struct identifier *funb;

	funpb=p->identifier_references+t;
	if(funpb->id_flags & (ID_HIDDEN|ID_STATIC)) continue;
	funb=ID_FROM_PTR(p,funpb);
	/* if(funb->func.offset == -1) continue; * prototype */

	if(fun->name==funb->name)
	{
	  found_better=t;

	  /* FIXME: Is this stuff needed?
	   *        It looks like it already is done by define_function().
	   */
	  if(funa_is_prototype && (funb->func.offset != -1) &&
	     !(funp->id_flags & ID_INLINE))
	  {
	    funp->inherit_offset = funpb->inherit_offset;
	    funp->identifier_offset = funpb->identifier_offset;
	  }
	  if(!funa_is_prototype && funb->func.offset == -1)
	  {
	    funpb->inherit_offset = funp->inherit_offset;
	    funpb->identifier_offset = funp->identifier_offset;
	  }
	}
      }
      if(found_better!=-1)
	continue;
    }
    if ((fun->func.offset == -1) && (funp->id_flags & ID_INLINE) &&
	IDENTIFIER_IS_PIKE_FUNCTION(fun->identifier_flags)) {
      if (!fun->name->size_shift && fun->name->len < 900) {
	my_yyerror("Missing definition for local function %s().",
		   fun->name->str);
      } else {
	yyerror("Missing definition for local function.");
      }
    }
    add_to_identifier_index(i);
  }
  fsort((void *)p->identifier_index,
	p->num_identifier_index,
	sizeof(unsigned short),(fsortfun)program_function_index_compare);


  /* Yes, it is supposed to start at 1  /Hubbe */
  for(i=1;i<NUM_LFUNS;i++) {
    p->lfuns[i] = low_find_lfun(p, i);
  }

  p->flags |= PROGRAM_FIXED;

  if(Pike_compiler->check_final)
  {
    for(i=0;i<(int)p->num_identifier_references;i++)
    {
      struct identifier *id;
      if((p->identifier_references[i].id_flags & (ID_NOMASK|ID_HIDDEN)) ==
	 ID_NOMASK)
      {
	struct pike_string *name=ID_FROM_INT(p, i)->name;

	e=find_shared_string_identifier(name,p);
	if(e == -1)
	  e=really_low_find_shared_string_identifier(name, p,
						     SEE_STATIC|SEE_PRIVATE);

	if(e != i)
	{
	  if(name->len < 1024 && !name->size_shift)
	    my_yyerror("Illegal to redefine final identifier %s",name->str);
	  else
	    my_yyerror("Illegal to redefine final identifier (unable to output name of identifier).");
	}
      }
    }
  }

#ifdef DEBUG_MALLOC
  {
#define DBSTR(X) ((X)?(X)->str:"")
    int e,v;
    INT32 line;
    struct pike_string *tmp;
    struct memory_map *m=0;;
    if(lex.current_file && 
       lex.current_file->str &&
       lex.current_file->len &&
       !strcmp(lex.current_file->str,"-"))
    {
      m=dmalloc_alloc_mmap( DBSTR(lex.current_file), lex.current_line);
    }
    else if( (tmp=find_program_name(Pike_compiler->new_program, &line)) )
    {
      m=dmalloc_alloc_mmap( tmp->str, line);
      free_string(tmp);
    }else{
      m=dmalloc_alloc_mmap( "program id", Pike_compiler->new_program->id);
    }

    for(e=0;e<Pike_compiler->new_program->num_inherits;e++)
    {
      struct inherit *i=Pike_compiler->new_program->inherits+e;
      char *tmp;
      struct pike_string *tmp2 = NULL;
      char buffer[50];

      for(v=0;v<i->prog->num_variable_index;v++)
      {
	int d=i->prog->variable_index[v];
	struct identifier *id=i->prog->identifiers+d;

	dmalloc_add_mmap_entry(m,
			       id->name->str,
			       /* OFFSETOF(object,storage) + */ i->storage_offset + id->func.offset,
			       sizeof_variable(id->run_time_type),
			       1, /* count */
			       0,0);
      }

      if(i->name)
      {
	tmp=i->name->str;
      }
      else if(!(tmp2 = find_program_name(i->prog, &line)))
      {
	sprintf(buffer,"inherit[%d]",e);
	tmp=buffer;
      } else {
	tmp = tmp2->str;
      }
      dmalloc_add_mmap_entry(m,
			     tmp,
			     /* OFFSETOF(object, storage) + */ i->storage_offset,
			     i->prog->storage_needed - i->prog->inherits[0].storage_offset,
			     1, /* count */
			     0,0);
      if (tmp2) {
	free_string(tmp2);
      }
    }
    dmalloc_set_mmap_template(Pike_compiler->new_program, m);
  }
#endif
}

struct program *low_allocate_program(void)
{
  struct program *p=alloc_program();
  MEMSET(p, 0, sizeof(struct program));
  p->flags|=PROGRAM_VIRGIN;
  p->alignment_needed=1;

  GC_ALLOC(p);
  p->id=++current_program_id;
  INIT_PIKE_MEMOBJ(p);

  DOUBLELINK(first_program, p);
  GETTIMEOFDAY(& p->timestamp);
  return p;
}

/*
 * Start building a new program
 */
void low_start_new_program(struct program *p,
			   struct pike_string *name,
			   int flags,
			   int *idp)
{
  int e,id=0;
  struct svalue tmp;

#if 0
#ifdef SHARED_NODES
  if (!node_hash.table) {
    node_hash.table = malloc(sizeof(node *)*32831);
    if (!node_hash.table) {
      fatal("Out of memory!\n");
    }
    MEMSET(node_hash.table, 0, sizeof(node *)*32831);
    node_hash.size = 32831;
  }
#endif /* SHARED_NODES */
#endif /* 0 */

  /* We don't want to change thread, but we don't want to
   * wait for the other threads to complete.
   */
  low_init_threads_disable();

  compilation_depth++;

  tmp.type=T_PROGRAM;
  if(!p)
  {
    p=low_allocate_program();
    if(name)
    {
      tmp.u.program=p;
      id=add_constant(name, &tmp, flags & ~ID_EXTERN);
      if(TEST_COMPAT(7,2) || (lex.pragmas && ID_SAVE_PARENT))
      {
	p->flags |= PROGRAM_USES_PARENT;
      }else{
	struct pike_string *tmp=findstring("__pragma_save_parent__");
	if(tmp)
	{
	  struct node_s *n=find_module_identifier(tmp, 0);
	  if(n)
	  {
	    free_node(n);
	    p->flags |= PROGRAM_USES_PARENT;
	  }
	}
      }


#if 0
      fprintf(stderr,"Compiling class %s, depth=%d\n",name->str,compilation_depth);
    }else{
      fprintf(stderr,"Compiling file %s, depth=%d\n",
	      lex.current_file ? lex.current_file->str : "-",
	      compilation_depth);
#endif
    }
    if(compilation_depth >= 1)
      add_ref(p->parent = Pike_compiler->new_program);
    e=1;
  }else{
    tmp.u.program=p;
    add_ref(p);
    if(name)
    {
      struct identifier *i;
      id=isidentifier(name);
      if (id < 0)
	fatal("Program constant disappeared in second pass.\n");
      i=ID_FROM_INT(Pike_compiler->new_program, id);
      free_type(i->type);
      i->type=get_type_of_svalue(&tmp);
    }
    e=2;
  }
  p->flags &=~ PROGRAM_VIRGIN;
  Pike_compiler->parent_identifier=id;
  if(idp) *idp=id;

  CDFPRINTF((stderr, "th(%ld) %p low_start_new_program() %s "
	     "pass=%d: threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), p, name ? name->str : "-",
	     Pike_compiler->compiler_pass,
	     threads_disabled, compilation_depth));

  init_type_stack();

#define PUSH
#include "compilation.h"

  Pike_compiler->compiler_pass=e;

  Pike_compiler->num_used_modules=0;

  if(p && (p->flags & PROGRAM_FINISHED))
  {
    yyerror("Pass2: Program already done");
    p=0;
  }

  Pike_compiler->malloc_size_program = ALLOC_STRUCT(program);
#ifdef PIKE_DEBUG
  Pike_compiler->fake_object=alloc_object();
  Pike_compiler->fake_object->storage=(char *)xalloc(256 * sizeof(struct svalue));
  /* Stipple to find illegal accesses */
  MEMSET(Pike_compiler->fake_object->storage,0x55,256*sizeof(struct svalue));
#else
  Pike_compiler->fake_object=ALLOC_STRUCT(object);
  Pike_compiler->fake_object->storage=(char *)malloc(sizeof(struct parent_info));
#endif
  /* Can't use GC_ALLOC on fake objects, but still it's good to know
   * that they never take over a stale gc marker. */
  if (Pike_in_gc) remove_marker(Pike_compiler->fake_object);

  Pike_compiler->fake_object->next=Pike_compiler->fake_object;
  Pike_compiler->fake_object->prev=Pike_compiler->fake_object;
  Pike_compiler->fake_object->refs=1;
  Pike_compiler->fake_object->prog=p;
  add_ref(p);

#ifdef PIKE_DEBUG
  Pike_compiler->fake_object->program_id=p->id;
#endif

#ifdef PIKE_SECURITY
  Pike_compiler->fake_object->prot=0;
#endif

  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);

  if(name)
  {
    /* Fake objects have parents regardless of PROGRAM_USE_PARENT  */
    if((((struct parent_info *)Pike_compiler->fake_object->storage)->parent=Pike_compiler->previous->fake_object))
      add_ref(Pike_compiler->previous->fake_object);
      ((struct parent_info *)Pike_compiler->fake_object->storage)->parent_identifier=id;
  }else{
    ((struct parent_info *)Pike_compiler->fake_object->storage)->parent=0;
    ((struct parent_info *)Pike_compiler->fake_object->storage)->parent_identifier=0;
  }

  Pike_compiler->new_program=p;

#ifdef PROGRAM_BUILD_DEBUG
  if (name) {
    fprintf (stderr, "%.*sstarting program %d (pass=%d): ",
	     compilation_depth, "                ", Pike_compiler->new_program->id, Pike_compiler->compiler_pass);
    push_string (name);
    print_svalue (stderr, --Pike_sp);
    putc ('\n', stderr);
  }
  else
    fprintf (stderr, "%.*sstarting program %d (pass=%d)\n",
	     compilation_depth, "                ", Pike_compiler->new_program->id, Pike_compiler->compiler_pass);
#endif

  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);

  if(Pike_compiler->new_program->program)
  {
#define FOO(NUMTYPE,TYPE,NAME) \
    Pike_compiler->malloc_size_program->PIKE_CONCAT(num_,NAME)=Pike_compiler->new_program->PIKE_CONCAT(num_,NAME);
#include "program_areas.h"


    {
      INT32 line=0, off=0;
      size_t len = 0;
      INT32 shift = 0;
      char *file=0;
      char *cnt=Pike_compiler->new_program->linenumbers;

      while(cnt < Pike_compiler->new_program->linenumbers +
	    Pike_compiler->new_program->num_linenumbers)
      {
	if(*cnt == 127)
	{
	  cnt++;
	  len = get_small_number(&cnt);
	  shift = *cnt;
	  file = ++cnt;
	  cnt += len<<shift;
	}
	off+=get_small_number(&cnt);
	line+=get_small_number(&cnt);
      }
      Pike_compiler->last_line=line;
      Pike_compiler->last_pc=off;
      if(file)
      {
	struct pike_string *str = begin_wide_shared_string(len, shift);
	if(Pike_compiler->last_file) free_string(Pike_compiler->last_file);
	memcpy(str->str, file, len<<shift);
	Pike_compiler->last_file = end_shared_string(str);
      }
    }

  }else{
    static struct pike_string *s;
    struct inherit i;

#define START_SIZE 64
#define FOO(NUMTYPE,TYPE,NAME) \
    Pike_compiler->malloc_size_program->PIKE_CONCAT(num_,NAME)=START_SIZE; \
    Pike_compiler->new_program->NAME=(TYPE *)xalloc(sizeof(TYPE) * START_SIZE);
#include "program_areas.h"

    i.prog=Pike_compiler->new_program;
    i.identifier_level=0;
    i.storage_offset=0;
    i.inherit_level=0;
    i.parent=0;
    i.parent_identifier=-1;
    i.parent_offset=-18;
    i.name=0;
    Pike_compiler->new_program->parent_program_id=compilation_depth>0 && Pike_compiler->previous->new_program ? Pike_compiler->previous->new_program->id : -1;
    add_to_inherits(i);
  }

  Pike_compiler->init_node=0;
  Pike_compiler->num_parse_error=0;

  push_compiler_frame(0);
  copy_pike_type(Pike_compiler->compiler_frame->current_return_type,
		 void_type_string);

  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);
}

PMOD_EXPORT void debug_start_new_program(PROGRAM_LINE_ARGS)
{
  CDFPRINTF((stderr,
	     "th(%ld) start_new_program(): threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(),threads_disabled, compilation_depth));

  low_start_new_program(0,0,0,0);
#ifdef PIKE_DEBUG
  {
    struct pike_string *s=make_shared_string(file);
    store_linenumber(line,s);
    free_string(s);
    debug_malloc_name(Pike_compiler->new_program, file, line);
  }
#endif
}


static void exit_program_struct(struct program *p)
{
  unsigned INT16 e;

  if(p->parent)
  {
    /* Make sure to break the circularity... */
    struct program *parent = p->parent;
    p->parent = NULL;
    free_program(parent);
  }


  if(id_to_program_cache[p->id & (ID_TO_PROGRAM_CACHE_SIZE-1)]==p)
    id_to_program_cache[p->id & (ID_TO_PROGRAM_CACHE_SIZE-1)]=0;

  if(p->strings)
    for(e=0; e<p->num_strings; e++)
      if(p->strings[e])
	free_string(p->strings[e]);

  if(p->identifiers)
  {
    for(e=0; e<p->num_identifiers; e++)
    {
      if(p->identifiers[e].name)
	free_string(p->identifiers[e].name);
      if(p->identifiers[e].type)
	free_type(p->identifiers[e].type);
    }
  }

  if(p->constants)
  {
    for(e=0;e<p->num_constants;e++)
    {
      free_svalue(& p->constants[e].sval);
      if(p->constants[e].name) free_string(p->constants[e].name);
    }
  }

  if(p->inherits)
    for(e=0; e<p->num_inherits; e++)
    {
      if(p->inherits[e].name)
	free_string(p->inherits[e].name);
      if(e)
      {
	if(p->inherits[e].prog)
	  free_program(p->inherits[e].prog);
      }
      if(p->inherits[e].parent)
	free_object(p->inherits[e].parent);
    }

  DOUBLEUNLINK(first_program, p);

  if(p->flags & PROGRAM_OPTIMIZED)
  {
    if(p->program)
      dmfree(p->program);
#define FOO(NUMTYPE,TYPE,NAME) p->NAME=0;
#include "program_areas.h"
  }else{
#define FOO(NUMTYPE,TYPE,NAME) \
    if(p->NAME) { dmfree((char *)p->NAME); p->NAME=0; }
#include "program_areas.h"
  }

  EXIT_PIKE_MEMOBJ(p);

  GC_FREE(p);
}

#ifdef PIKE_DEBUG
void dump_program_desc(struct program *p)
{
  int e,d,q;
/*  fprintf(stderr,"Program '%s':\n",p->name->str); */

  fprintf(stderr,"All inherits:\n");
  for(e=0;e<p->num_inherits;e++)
  {
    for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"%3d:\n",e);

    for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"inherited program: %d\n",p->inherits[e].prog->id);

    if(p->inherits[e].name)
    {
      for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
      fprintf(stderr,"name  : %s\n",p->inherits[e].name->str);
    }

    for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"inherit_level: %d\n",p->inherits[e].inherit_level);

    for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"identifier_level: %d\n",p->inherits[e].identifier_level);

    for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"parent_identifier: %d\n",p->inherits[e].parent_identifier);

    for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"parent_offset: %d\n",p->inherits[e].parent_offset);

    for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"storage_offset: %ld\n",
	    DO_NOT_WARN((long)p->inherits[e].storage_offset));

    for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"parent: %p\n",p->inherits[e].parent);

    if(p->inherits[e].parent &&
      p->inherits[e].parent->prog &&
      p->inherits[e].parent->prog->num_linenumbers>1)
    {
      for(d=0;d<p->inherits[e].inherit_level;d++) fprintf(stderr,"  ");
      fprintf(stderr,"parent: %s\n",p->inherits[e].parent->prog->linenumbers+1);
    }
  }

  fprintf(stderr,"All identifiers:\n");
  for(e=0;e<(int)p->num_identifier_references;e++)
  {
    fprintf(stderr,"%3d:",e);
    for(d=0;d<INHERIT_FROM_INT(p,e)->inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"%s;\n",ID_FROM_INT(p,e)->name->str);
  }
  fprintf(stderr,"All sorted identifiers:\n");
  for(q=0;q<(int)p->num_identifier_index;q++)
  {
    e=p->identifier_index[q];
    fprintf(stderr,"%3d (%3d):",e,q);
    for(d=0;d<INHERIT_FROM_INT(p,e)->inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"%s;\n", ID_FROM_INT(p,e)->name->str);
  }
}
#endif

static void toss_compilation_resources(void)
{
  if(Pike_compiler->fake_object)
  {
    if( ((struct parent_info *)Pike_compiler->fake_object->storage)->parent )
    {
      free_object(((struct parent_info *)Pike_compiler->fake_object->storage)->parent);

      ((struct parent_info *)Pike_compiler->fake_object->storage)->parent=0;
    }

    free_program(Pike_compiler->fake_object->prog);
    Pike_compiler->fake_object->prog=0;
    free_object(Pike_compiler->fake_object);
    Pike_compiler->fake_object=0;
  }

  free_program(Pike_compiler->new_program);
  Pike_compiler->new_program=0;

  if(Pike_compiler->malloc_size_program)
    {
      dmfree((char *)Pike_compiler->malloc_size_program);
      Pike_compiler->malloc_size_program=0;
    }

  if(Pike_compiler->module_index_cache)
  {
    free_mapping(Pike_compiler->module_index_cache);
    Pike_compiler->module_index_cache=0;
  }

  while(Pike_compiler->compiler_frame)
    pop_compiler_frame();

  if(Pike_compiler->last_identifier)
  {
    free_string(Pike_compiler->last_identifier);
    Pike_compiler->last_identifier=0;
  }

  if(Pike_compiler->last_file)
  {
    free_string(Pike_compiler->last_file);
    Pike_compiler->last_file=0;
  }

  unuse_modules(Pike_compiler->num_used_modules);
}

int sizeof_variable(int run_time_type)
{
  switch(run_time_type)
  {
    case T_FUNCTION:
    case T_MIXED: return sizeof(struct svalue);
    case T_FLOAT: return sizeof(FLOAT_TYPE);
    case T_INT: return sizeof(INT_TYPE);
    default: return sizeof(void *);
  }
}

static ptrdiff_t alignof_variable(int run_time_type)
{
  switch(run_time_type)
  {
    case T_FUNCTION:
    case T_MIXED: return ALIGNOF(struct svalue);
    case T_FLOAT: return ALIGNOF(FLOAT_TYPE);
    case T_INT: return ALIGNOF(INT_TYPE);
    default: return ALIGNOF(void *);
  }
}

#ifdef PIKE_DEBUG

void dump_program_tables (struct program *p, int indent)
{
  int d;

  fprintf(stderr,
	  "%*sProgram flags: 0x%04x\n\n",
	  indent, "", p->flags);

  fprintf(stderr,
	  "%*sReference table:\n"
	  "%*s  ####: Flags Inherit Identifier\n",
	  indent, "", indent, "");
  for (d=0; d < p->num_identifier_references; d++) {
    struct reference *ref = p->identifier_references + d;

    fprintf(stderr, "%*s  %4d: %5x %7d %10d  %s\n",
	    indent, "",
	    d, ref->id_flags, ref->inherit_offset,
	    ref->identifier_offset,
	    ID_FROM_PTR(p,ref)->name->size_shift ? "(wide)" :
	    ID_FROM_PTR(p,ref)->name->str);
    if (IDENTIFIER_IS_PIKE_FUNCTION(ID_FROM_PTR(p,ref)->identifier_flags)) {
      INT32 line;
      struct program *inh_p = INHERIT_FROM_PTR(p,ref)->prog;
      char *file = get_line (ID_FROM_PTR(p,ref)->func.offset + inh_p->program,
			     inh_p, &line);
      fprintf (stderr, "%*s                                  %s:%d\n",
	       indent, "", file, line);
    }
  }
  fprintf(stderr, "\n"
	  "%*sInherit table:\n"
	  "%*s  ####: Level id_level offset ref_offset\n",
	  indent, "", indent, "");
  for (d=0; d < p->num_inherits; d++) {
    struct inherit *inh = p->inherits + d;

    fprintf(stderr, "%*s  %4d: %5d %8d %6d %10d\n",
	    indent, "",
	    d, inh->inherit_level, inh->identifier_level,
	    inh->storage_offset, inh->identifier_ref_offset);
  }
  fprintf(stderr, "\n"
	  "%*sIdentifier table:\n"
	  "%*s  ####: Flags Offset Type Name\n",
	  indent, "", indent, "");
  for (d=0; d < p->num_identifiers; d++) {
    struct identifier *id = p->identifiers + d;

    fprintf(stderr, "%*s  %4d: %5x %6d %4d \"%s\"\n",
	    indent, "",
	    d, id->identifier_flags, id->func.offset,
	    id->run_time_type, id->name->str);
  }
  fprintf(stderr, "\n"
	  "%*sVariable table:\n"
	  "%*s  ####: Index\n",
	  indent, "", indent, "");
  for (d = 0; d < p->num_variable_index; d++) {
    fprintf(stderr, "%*s  %4d: %5d\n",
	    indent, "",
	    d, p->variable_index[d]);
  }
  fprintf(stderr, "\n"
	  "%*sConstant table:\n"
	  "%*s  ####: Type Name\n",
	  indent, "", indent, "");
  for (d = 0; d < p->num_constants; d++) {
    struct program_constant *c = p->constants + d;
    fprintf(stderr, "%*s  %4d: %4d %s%s%s\n",
	    indent, "",
	    d, c->sval.type,
	    c->name?"\"":"",c->name?c->name->str:"NULL",c->name?"\"":"");
  }
  fprintf(stderr, "\n");
}

void check_program(struct program *p)
{
  INT32 size;
  unsigned INT32 checksum, e;
  int variable_positions[1024];

  if(p->flags & PROGRAM_AVOID_CHECK) return;

  for(e=0;e<NELEM(variable_positions);e++)
    variable_positions[e]=-1;

  if(p->id > current_program_id)
    fatal("Program id is out of sync! (p->id=%d, current_program_id=%d)\n",p->id,current_program_id);

  if(p->refs <=0)
    fatal("Program has zero refs.\n");

  if(p->next && p->next->prev != p)
    fatal("Program ->next->prev != program.\n");

  if(p->prev)
  {
    if(p->prev->next != p)
      fatal("Program ->prev->next != program.\n");
  }else{
    if(first_program != p)
      fatal("Program ->prev == 0 but first_program != program.\n");
  }

  if(p->id > current_program_id || p->id <= 0)
    fatal("Program id is wrong.\n");

  if(p->storage_needed < 0)
    fatal("Program->storage_needed < 0.\n");

  if(p->num_identifier_index > p->num_identifier_references)
    fatal("Too many identifier index entries in program!\n");

  for(e=0;e<p->num_constants;e++)
  {
    check_svalue(& p->constants[e].sval);
    if(p->constants[e].name) check_string(p->constants[e].name);
  }

  for(e=0;e<p->num_strings;e++)
    check_string(p->strings[e]);

  for(e=0;e<p->num_inherits;e++)
  {
    if(!p->inherits[e].prog) 
    {
      /* This inherit is not yet initialized, ignore rest of tests.. */
      return;
    }

    if(p->inherits[e].storage_offset < 0)
      fatal("Inherit->storage_offset is wrong.\n");

    if(p->inherits[e].prog &&
       p->inherits[e].storage_offset + STORAGE_NEEDED(p->inherits[e].prog) >
       p->storage_needed)
      fatal("Not enough room allocated by inherit!\n");

    if(e)
    {
      if(p->inherits[e-1].storage_offset >
	 p->inherits[e].storage_offset)
	fatal("Overlapping inherits! (1)\n");

      if(p->inherits[e-1].prog &&
	 p->inherits[e-1].inherit_level >= p->inherits[e].inherit_level &&
	 ( p->inherits[e-1].storage_offset +
	   STORAGE_NEEDED(p->inherits[e-1].prog)) >
	 p->inherits[e].storage_offset)
	fatal("Overlapping inherits! (3)\n");
    }
  }


  if(p->flags & PROGRAM_FINISHED)
  for(e=0;e<p->num_identifiers;e++)
  {
    check_string(p->identifiers[e].name);
    check_type_string(p->identifiers[e].type);

    if(p->identifiers[e].identifier_flags & ~IDENTIFIER_MASK)
      fatal("Unknown flags in identifier flag field.\n");

    if(p->identifiers[e].run_time_type!=T_MIXED)
      check_type(p->identifiers[e].run_time_type);

    if(IDENTIFIER_IS_VARIABLE(p->identifiers[e].identifier_flags))
    {
      if( (p->identifiers[e].func.offset /* + OFFSETOF(object,storage)*/ ) &
	 (alignof_variable(p->identifiers[e].run_time_type)-1))
      {
	fatal("Variable %s offset is not properly aligned (%ld).\n",
	      p->identifiers[e].name->str,
	      PTRDIFF_T_TO_LONG(p->identifiers[e].func.offset));
      }
    }
  }

  for(e=0;e<p->num_identifier_references;e++)
  {
    struct identifier *i;
    if(p->identifier_references[e].inherit_offset > p->num_inherits)
      fatal("Inherit offset is wrong!\n");

    if(!p->inherits[p->identifier_references[e].inherit_offset].prog)
    {
      if(!(p->flags & PROGRAM_FINISHED))
	continue;

      fatal("p->inherit[%d].prog = NULL!\n",p->identifier_references[e].inherit_offset);
    }

    if(p->identifier_references[e].identifier_offset >
       p->inherits[p->identifier_references[e].inherit_offset].prog->num_identifiers)
      fatal("Identifier offset %d is wrong! %d > %d\n",
	    e,
	    p->identifier_references[e].identifier_offset,
	    p->inherits[p->identifier_references[e].inherit_offset].prog->num_identifiers);

    i=ID_FROM_INT(p, e);

    if( !(i->identifier_flags & (IDENTIFIER_FUNCTION | IDENTIFIER_CONSTANT)))
    {
      size_t q, size;
      /* Variable */
      ptrdiff_t offset = INHERIT_FROM_INT(p, e)->storage_offset+i->func.offset;
      size=sizeof_variable(i->run_time_type);

      if((offset+size > (size_t)p->storage_needed) || offset<0)
	fatal("Variable outside storage! (%s)\n",i->name->str);

      for(q=0;q<size;q++)
      {
	if(offset+q >= NELEM(variable_positions)) break;

	if(variable_positions[offset+q] != -1)
	{
	  if(ID_FROM_INT(p,variable_positions[offset+q])->run_time_type !=
	     i->run_time_type)
	  {
	    fprintf(stderr, "Storage offset: 0x%08x vs 0x%08x\n"
		    "Func offset: 0x%08x vs 0x%08x\n"
		    "Type: %s vs %s\n",
		    INHERIT_FROM_INT(p, variable_positions[offset+q])->
		    storage_offset,
		    INHERIT_FROM_INT(p, e)->storage_offset,
		    ID_FROM_INT(p, variable_positions[offset+q])->func.offset,
		    i->func.offset,
		    get_name_of_type(ID_FROM_INT(p,variable_positions[offset+q]
						 )->run_time_type),
		    get_name_of_type(i->run_time_type));
	    if (i->name) {
	      fatal("Variable '%s' and '%s' overlap\n"
		    "Offset 0x%08x - 0x%08x overlaps with 0x%08x - 0x%08x\n",
		    ID_FROM_INT(p, variable_positions[offset+q])->name->str,
		    i->name->str,
		    INHERIT_FROM_INT(p, variable_positions[offset+q])->
		    storage_offset +
		    ID_FROM_INT(p, variable_positions[offset+q])->func.offset,
		    INHERIT_FROM_INT(p, variable_positions[offset+q])->
		    storage_offset +
		    ID_FROM_INT(p, variable_positions[offset+q])->func.offset +
		    sizeof_variable(ID_FROM_INT(p, variable_positions[offset+q]
						)->run_time_type)-1,
		    offset, offset+size-1);
	    } else {
	      fatal("Variable '%s' and anonymous variable (%d) overlap\n"
		    "Offset 0x%08x - 0x%08x overlaps with 0x%08x - 0x%08x\n",
		    ID_FROM_INT(p, variable_positions[offset+q])->name->str,
		    e,
		    INHERIT_FROM_INT(p, variable_positions[offset+q])->
		    storage_offset +
		    ID_FROM_INT(p, variable_positions[offset+q])->func.offset,
		    INHERIT_FROM_INT(p, variable_positions[offset+q])->
		    storage_offset +
		    ID_FROM_INT(p, variable_positions[offset+q])->func.offset +
		    sizeof_variable(ID_FROM_INT(p, variable_positions[offset+q]
						)->run_time_type)-1,
		    offset, offset+size-1);
	    }
	  }
	}
	variable_positions[offset+q]=e;
      }
    }
  }

  for(e=0;e<p->num_identifier_index;e++)
  {
    if(p->identifier_index[e] > p->num_identifier_references)
      fatal("Program->identifier_indexes[%ld] is wrong\n",(long)e);
  }

}
#endif

/* Note: This function is misnamed, since it's run after both passes. /mast */
struct program *end_first_pass(int finish)
{
  int e;
  struct program *prog;
  struct pike_string *s;

  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);

  MAKE_CONSTANT_SHARED_STRING(s,"__INIT");


  /* Collect references to inherited __INIT functions */
  for(e=Pike_compiler->new_program->num_inherits-1;e;e--)
  {
    int id;
    if(Pike_compiler->new_program->inherits[e].inherit_level!=1) continue;
    id=low_reference_inherited_identifier(0, e, s, SEE_STATIC);
    if(id!=-1)
    {
      Pike_compiler->init_node=mknode(F_COMMA_EXPR,
		       mkcastnode(void_type_string,
				  mkapplynode(mkidentifiernode(id),0)),
		       Pike_compiler->init_node);
    }
  }

  /*
   * Define the __INIT function, but only if there was any code
   * to initialize.
   */

  if(Pike_compiler->init_node)
  {
    e=dooptcode(s,
		mknode(F_COMMA_EXPR,
		       Pike_compiler->init_node,mknode(F_RETURN,mkintnode(0),0)),
		function_type_string,
		ID_STATIC);
    Pike_compiler->init_node=0;
  }else{
    e=-1;
  }
  Pike_compiler->new_program->lfuns[LFUN___INIT]=e;

  free_string(s);

  pop_compiler_frame(); /* Pop __INIT local variables */

  if(Pike_compiler->num_parse_error > 0)
  {
    prog=0;
  }else{
    prog=Pike_compiler->new_program;
    add_ref(prog);

#ifdef PIKE_DEBUG
    check_program(prog);
    if(l_flag)
      dump_program_desc(prog);
#endif

    Pike_compiler->new_program->flags |= PROGRAM_PASS_1_DONE;

    if(finish)
    {
      if(Pike_compiler->new_program->flags & PROGRAM_USES_PARENT)
      {
	Pike_compiler->new_program->parent_info_storage =
	  add_xstorage(sizeof(struct parent_info),
		       ALIGNOF(struct parent_info),
		       0);
      }else{
	/* Cause errors if used hopefully */
	Pike_compiler->new_program->parent_info_storage=-1;
      }

      fixate_program();
      if(Pike_compiler->num_parse_error)
      {
	free_program(prog);
	prog=0;
      }else{
	optimize_program(Pike_compiler->new_program);
	Pike_compiler->new_program->flags |= PROGRAM_FINISHED;
      }
    }

  }

#ifdef PROGRAM_BUILD_DEBUG
  fprintf (stderr, "%.*sfinishing program %d (pass=%d)\n",
	   compilation_depth, "                ", Pike_compiler->new_program->id, Pike_compiler->compiler_pass);
#endif

  toss_compilation_resources();

#if 0
  CDFPRINTF((stderr,
	     "th(%ld) end_first_pass(): "
	     "compilation_depth:%d, Pike_compiler->compiler_pass:%d\n",
	     (long)th_self(), compilation_depth, Pike_compiler->compiler_pass));
#endif

  if(!Pike_compiler->compiler_frame && (Pike_compiler->compiler_pass==2 || !prog) && resolve_cache)
  {
    free_mapping(dmalloc_touch(struct mapping *, resolve_cache));
    resolve_cache=0;
  }

#ifdef SHARED_NODES
  /* free(node_hash.table); */
#endif /* SHARED_NODES */

#define POP
#include "compilation.h"

  exit_type_stack();

  CDFPRINTF((stderr,
	     "th(%ld) %p end_first_pass(%d): "
	     "threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), prog, finish,
	     threads_disabled, compilation_depth));

  compilation_depth--;

  exit_threads_disable(NULL);

  free_all_nodes();

  return prog;
}

/*
 * Finish this program, returning the newly built program
 */
PMOD_EXPORT struct program *debug_end_program(void)
{
  Pike_compiler->compiler_pass = 2;
  return end_first_pass(1);
}


/*
 * Allocate needed for this program in the object structure.
 * An offset to the data is returned.
 */
PMOD_EXPORT size_t low_add_storage(size_t size, size_t alignment,
				   ptrdiff_t modulo_orig)
{
  ptrdiff_t offset;
  ptrdiff_t modulo;

  if(!size) return Pike_compiler->new_program->storage_needed;

#ifdef PIKE_DEBUG
  if(alignment <=0 || (alignment & (alignment-1)) || alignment > 256)
    fatal("Alignment must be 1,2,4,8,16,32,64,128 or 256 not %ld\n",
	  PTRDIFF_T_TO_LONG(alignment));
#endif
  modulo=( modulo_orig /* +OFFSETOF(object,storage) */ ) % alignment;

  offset=DO_ALIGN(Pike_compiler->new_program->storage_needed-modulo,alignment)+modulo;

  if(!Pike_compiler->new_program->storage_needed) {
    /* Shouldn't Pike_compiler->new_program->storage_needed be set here?
     * Otherwise the debug code below ought to be trigged.
     * But since it isn't, I guess this is dead code?
     *	/grubba 1999-09-28
     *
     * No, the below offset represents the storage in the beginning
     * of obj->storage which is not used because of alignment constraints.
     * However, for historical reasons, prog->storage_offset needs to
     * contain this unused space as well. This means that the real
     * space used by all variables in an object is really:
     * o->prog->storage_needed - o->prog->inherits[0].storage_offset,
     * This can also be written as STORAGE_NEEDED(o->prog)
     * STORAGE_NEEDED() is defined in program.h.
     * /Hubbe 1999-09-29
     *
     * Oops, seems I read the test below the wrong way around.
     *	/grubba 1999-09-29
     */
    Pike_compiler->new_program->inherits[0].storage_offset=offset;
  }

  if(Pike_compiler->new_program->alignment_needed<alignment)
    Pike_compiler->new_program->alignment_needed =
      DO_NOT_WARN((unsigned INT8)alignment);

#ifdef PIKE_DEBUG
  if(offset < Pike_compiler->new_program->storage_needed)
    fatal("add_storage failed horribly!\n");

  if( (offset /* + OFFSETOF(object,storage) */ - modulo_orig ) % alignment )
    fatal("add_storage failed horribly(2) %ld %ld %ld %ld!\n",
	  DO_NOT_WARN((long)offset),
	  (long)0 /* + OFFSETOF(object,storage) */,
	  DO_NOT_WARN((long)modulo_orig),
	  DO_NOT_WARN((long)alignment));

#endif

  Pike_compiler->new_program->storage_needed = offset + size;

  return (size_t) offset;
}

/*
 * Internal function.
 * Adds object storage that will *not* be inherited.
 */
static size_t add_xstorage(size_t size,
			   size_t alignment,
			   ptrdiff_t modulo_orig)
{
  ptrdiff_t offset, modulo, available;
  int e;

  if(!size) return Pike_compiler->new_program->xstorage;

  modulo=( modulo_orig /* +OFFSETOF(object,storage) */ ) % alignment;

  offset=DO_ALIGN(Pike_compiler->new_program->xstorage-modulo,alignment)+modulo;

  Pike_compiler->new_program->xstorage = offset + size;

  /* Move all inherits to make room */
  available = Pike_compiler->new_program->inherits[0].storage_offset;
  if(available < (ptrdiff_t)(offset+size))
  {
    available=
      DO_ALIGN( ((offset + size) - available), 
		Pike_compiler->new_program->alignment_needed);
    
    for(e=0;e<Pike_compiler->new_program->num_inherits;e++)
      Pike_compiler->new_program->inherits[e].storage_offset+=available;

    Pike_compiler->new_program->storage_needed+=available;
  }
  
  return (size_t) offset;
}

typedef void (*oldhandlertype)(struct object *);
static void compat_event_handler(int e)
{
  oldhandlertype handler;
  debug_malloc_touch(Pike_fp->current_object);
  handler=((oldhandlertype *)Pike_fp->context.prog->program)[e];
  if(handler) handler(Pike_fp->current_object);
  debug_malloc_touch(Pike_fp->current_object);
}

static void add_compat_event_handler(void)
{
  if(Pike_compiler->new_program->event_handler != compat_event_handler)
  {
    unsigned int e,d;
    unsigned char *tmp=(unsigned char *)&Pike_compiler->new_program->event_handler;

    for(d=0;d<NUM_PROG_EVENTS;d++) {
      /* FIXME: This looks like it might be broken. */
      /* Broken how? -Hubbe */
#ifdef HAVE_COMPUTED_GOTO
      add_to_program(Pike_compiler->new_program->event_handler);
#else /* !HAVE_COMPUTED_GOTO */
      for(e=0;e<sizeof(Pike_compiler->new_program->event_handler);e++)
	add_to_program(tmp[e]);
#endif /* HAVE_COMPUTED_GOTO */
    }    
    Pike_compiler->new_program->event_handler=compat_event_handler;
  }
}

/*
 * set a callback used to initialize clones of this program
 * the init function is called at clone time
 * This function is obsolete, use pike_set_prog_event_callback instead.
 */
PMOD_EXPORT void set_init_callback(void (*init)(struct object *))
{
  add_compat_event_handler();
  ((oldhandlertype *)Pike_compiler->new_program->program)[PROG_EVENT_INIT]=init;
}

/*
 * set a callback used to de-initialize clones of this program
 * the exit function is called at destruct
 * This function is obsolete, use pike_set_prog_event_callback instead.
 */
PMOD_EXPORT void set_exit_callback(void (*exit)(struct object *))
{
  add_compat_event_handler();
  ((oldhandlertype *)Pike_compiler->new_program->program)[PROG_EVENT_EXIT]=exit;
}

/*
 * This callback is used by the gc to traverse all references to
 * objects. It should call some gc_recurse_* function exactly once for
 * each reference that the pike internals doesn't know about.
 *
 * If a reference is shared between objects, it should be traversed
 * once for every instance sharing it.
 *
 * The callback might be called more than once for the same instance
 * during a gc pass. The gc assumes that the references are enumerated
 * in the same order in that case.
 *
 * This function is obsolete, use pike_set_prog_event_callback instead.
 */
PMOD_EXPORT void set_gc_recurse_callback(void (*m)(struct object *))
{
  add_compat_event_handler();
  ((oldhandlertype *)Pike_compiler->new_program->program)[PROG_EVENT_GC_RECURSE]=m;
}

/*
 * This callback is used by the gc to count all references to objects.
 * It should call gc_check, gc_check_(weak_)svalues or
 * gc_check_(weak_)short_svalue exactly once for each reference that
 * the pike internals doesn't know about.
 *
 * If a reference is shared between objects, it should be counted once
 * for all shared instances. The return value from gc_check is useful
 * to ensure this; it's zero when called the first time for its
 * argument.
 *
 * This function is obsolete, use pike_set_prog_event_callback instead.
 */
PMOD_EXPORT void set_gc_check_callback(void (*m)(struct object *))
{
  add_compat_event_handler();
  ((oldhandlertype *)Pike_compiler->new_program->program)[PROG_EVENT_GC_CHECK]=m;
}

void pike_set_prog_event_callback(void (*cb)(int))
{
#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->event_handler)
    fatal("Program already has an event handler!\n");
#endif
  Pike_compiler->new_program->event_handler=cb;
}

void pike_set_prog_optimize_callback(node *(*opt)(node *))
{
#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->optimize)
    fatal("Program already has an optimize handler!\n");
#endif
  Pike_compiler->new_program->optimize = opt;
}

int low_reference_inherited_identifier(struct program_state *q,
				       int e,
				       struct pike_string *name,
				       int flags)
{
  struct program *np=(q?q:Pike_compiler)->new_program;
  struct reference funp;
  struct program *p;
  int i,d;

  p=np->inherits[e].prog;
  i=find_shared_string_identifier(name,p);
  if(i==-1)
  {
    i=really_low_find_shared_string_identifier(name,p, flags);
    if(i==-1) return -1;
  }

  if(p->identifier_references[i].id_flags & ID_HIDDEN)
    return -1;

  if(p->identifier_references[i].id_flags & ID_PRIVATE)
    if(!(flags & SEE_PRIVATE))
      return -1;

  funp=p->identifier_references[i];
  funp.inherit_offset+=e;
  funp.id_flags = (funp.id_flags & ~ID_INHERITED) | ID_INLINE|ID_HIDDEN;

  for(d=0;d<(int)np->num_identifier_references;d++)
  {
    struct reference *refp;
    refp=np->identifier_references+d;

    if(!MEMCMP((char *)refp,(char *)&funp,sizeof funp)) return d;
  }

  if(q)
    low_add_to_identifier_references(q,funp);
  else
    add_to_identifier_references(funp);
  return np->num_identifier_references -1;
}

int find_inherit(struct program *p, struct pike_string *name)
{
  int e;

#if 0
  fprintf(stderr, "find_inherit(0x%08lx, \"%s\")...\n",
	  (unsigned long)p, name->str);
#endif /* 0 */
  for(e = p->num_inherits-1; e>0; e--) {
#if 0
    fprintf(stderr, "  %04d: %04d %s\n",
	    e, p->inherits[e].inherit_level,
	    p->inherits[e].name?p->inherits[e].name->str:"NULL");
#endif /* 0 */
    if (p->inherits[e].inherit_level > 1) continue;
    if (name == p->inherits[e].name) return e;
  }
  return 0;
}

node *reference_inherited_identifier(struct pike_string *super_name,
				     struct pike_string *function_name)
{
  int n,e,id;
  struct program_state *state=Pike_compiler->previous;

  struct program *p;


#ifdef PIKE_DEBUG
  if(function_name!=debug_findstring(function_name))
    fatal("reference_inherited_function on nonshared string.\n");
#endif

  p=Pike_compiler->new_program;

  for(e=p->num_inherits-1;e>0;e--)
  {
    if(p->inherits[e].inherit_level!=1) continue;
    if(!p->inherits[e].name) continue;

    if(super_name)
      if(super_name != p->inherits[e].name)
	continue;

    id=low_reference_inherited_identifier(0,
					  e,
					  function_name,
					  SEE_STATIC);

    if(id!=-1)
      return mkidentifiernode(id);

    if(ISCONSTSTR(function_name,"`->") ||
       ISCONSTSTR(function_name,"`[]"))
    {
      return mknode(F_MAGIC_INDEX,mknewintnode(e),mknewintnode(0));
    }

    if(ISCONSTSTR(function_name,"`->=") ||
       ISCONSTSTR(function_name,"`[]="))
    {
      return mknode(F_MAGIC_SET_INDEX,mknewintnode(e),mknewintnode(0));
    }

    if(ISCONSTSTR(function_name,"_indices"))
    {
      return mknode(F_MAGIC_INDICES,mknewintnode(e),mknewintnode(0));
    }

    if(ISCONSTSTR(function_name,"_values"))
    {
      return mknode(F_MAGIC_VALUES,mknewintnode(e),mknewintnode(0));
    }
  }


  for(n=0;n<compilation_depth;n++,state=state->previous)
  {
    struct program *p=state->new_program;

    for(e=p->num_inherits-1;e>0;e--)
    {
      if(p->inherits[e].inherit_level!=1) continue;
      if(!p->inherits[e].name) continue;

      if(super_name)
	if(super_name != p->inherits[e].name)
	  continue;

      id=low_reference_inherited_identifier(state,e,function_name,SEE_STATIC);

      if(id!=-1)
	return mkexternalnode(p, id);

      if(ISCONSTSTR(function_name,"`->") ||
	 ISCONSTSTR(function_name,"`[]"))
      {
	return mknode(F_MAGIC_INDEX,
		      mknewintnode(e),mknewintnode(n+1));
      }

      if(ISCONSTSTR(function_name,"`->=") ||
	 ISCONSTSTR(function_name,"`[]="))
      {
	return mknode(F_MAGIC_SET_INDEX,
		      mknewintnode(e),mknewintnode(n+1));
      }

      if(ISCONSTSTR(function_name,"_indices"))
      {
	return mknode(F_MAGIC_INDICES,
		      mknewintnode(e),mknewintnode(n+1));
      }

      if(ISCONSTSTR(function_name,"_values"))
      {
	return mknode(F_MAGIC_VALUES,
		      mknewintnode(e),mknewintnode(n+1));
      }
    }
  }

  return 0;
}

void rename_last_inherit(struct pike_string *n)
{
  if(Pike_compiler->new_program->inherits[Pike_compiler->new_program->num_inherits].name)
    free_string(Pike_compiler->new_program->inherits[Pike_compiler->new_program->num_inherits].name);
  copy_shared_string(Pike_compiler->new_program->inherits[Pike_compiler->new_program->num_inherits].name,
		     n);
}

static int locate_parent_state(struct program_state **state,
			       struct inherit **i,
			       int *parent_identifier,
			       int depth)
{
  int result=1;
  if(depth<=0) return depth;
  while(depth-->0)
  {
    if( (*i)->parent_offset != -17)
    {
      int tmp=(*i)->parent_identifier;
      if( (*i)->parent_offset > 0)
      {
	int po=(*i)->parent_offset;
	*parent_identifier = (*state)->parent_identifier;
	*state = (*state)->previous;
	result++;
	fprintf(stderr,"INHERIT: state=state->previous (po=%d)\n",po);

	if(po>1)
	{
	  *i=INHERIT_FROM_INT( (*state)->new_program,
			       *parent_identifier);

	  result+=locate_parent_state(state,i,parent_identifier, po-1);
	}
      }

      if(tmp != -1)
      {
	if( *parent_identifier == -4711)
	{
	  *parent_identifier = tmp;
	}else{
	  *parent_identifier = tmp + INHERIT_FROM_INT( (*state)->new_program,
						      *parent_identifier)->identifier_level;
	}
      }
    }else{
      fprintf(stderr,"INHERIT: Bailout!\n");
      return result+depth+1;
    }
    *i = INHERIT_FROM_INT( (*state)->new_program, *parent_identifier);
  }
  return result;
}


static int find_depth(struct program_state *state,
		      struct inherit *i,
		      int parent_identifier,
		      int depth)
{
#if 0
  int e;
  struct inherit *oi;
  for(e=0;e<=parent_offset;e++) state=state->previous;
  oi=INHERIT_FROM_INT(state->new_program, parent_identifier);
  parent_offset+=i->parent_offset;
#endif

  return locate_parent_state(&state,
			     &i,
			     &parent_identifier,
			     depth);
}

/*
 * make this program inherit another program
 */
void low_inherit(struct program *p,
		 struct object *parent,
		 int parent_identifier,
		 int parent_offset,
		 INT32 flags,
		 struct pike_string *name)
{
  int e;
  ptrdiff_t inherit_offset, storage_offset;
  struct inherit inherit;

#if 0
  fprintf(stderr,"%p low_inherit(pid=%d, parent=%p, parid=%d, "
	  "paroff=%d, flags=0x%x, name=%s);\n",
	  Pike_compiler->new_program,
	  p ? p->id : 0,
	  parent,
	  parent_identifier,
	  parent_offset,
	  flags,
	  name?  name->str : "");
#endif
  CDFPRINTF((stderr, "th(%ld) %p inherit %p\n",
	     (long) th_self(), Pike_compiler->new_program, p));
	
  if(!p)
  {
    yyerror("Illegal program pointer.");
    return;
  }

#ifdef PIKE_DEBUG
  if (p == placeholder_program)
    fatal("Trying to inherit placeholder_program.\n");
#endif

  if(p->flags & PROGRAM_NEEDS_PARENT)
  {
    struct program_state *state=Pike_compiler;

    if(!parent && !parent_offset)
    {
      yyerror("Parent pointer lost, cannot inherit!");
      /* We inherit it anyway, to avoid causing more errors */
    }

#if 0
    /* FIXME: we don't really need to set this flag on ALL
     * previous compilations, but I'm too lazy to figure out
     * exactly how deep down we need to go...
     */
    for(e=0;e<compilation_depth;e++,state=state->previous)
      state->new_program->flags |= PROGRAM_USES_PARENT;
#endif
  }

 /* parent offset was increased by 42 for above test.. */
  if(parent_offset)
    parent_offset-=42;


  if(!(p->flags & (PROGRAM_FINISHED | PROGRAM_PASS_1_DONE)))
  {
    yyerror("Cannot inherit program which is not fully compiled yet.");
    return;
  }

  inherit_offset = Pike_compiler->new_program->num_inherits;

  /* alignment magic */
  storage_offset=p->inherits[0].storage_offset % p->alignment_needed;
  storage_offset=low_add_storage(STORAGE_NEEDED(p),
				 p->alignment_needed,
				 storage_offset);

  /* Without this, the inherit becomes skewed */
  storage_offset-=p->inherits[0].storage_offset;

  for(e=0; e<(int)p->num_inherits; e++)
  {
    inherit=p->inherits[e];
    add_ref(inherit.prog);
    inherit.identifier_level += Pike_compiler->new_program->num_identifier_references;
    inherit.storage_offset += storage_offset;
    inherit.inherit_level ++;


    if(!e)
    {
      if(parent)
      {
	if(parent->next == parent)
	{
#if 0
	  struct object *o;
	  inherit.parent_offset=0;
	  for(o=Pike_compiler->fake_object;o!=parent;o=o->parent)
	  {
#ifdef PIKE_DEBUG
	    if(!o) fatal("low_inherit with odd Pike_compiler->fake_object as parent!\n");
#endif
	    inherit.parent_offset++;
	  }
#else
          struct program_state *state=Pike_compiler;
	  inherit.parent_offset=0;
	  for(;state->fake_object!=parent;state=state->previous)
	  {
#ifdef PIKE_DEBUG
	    if(!state->fake_object)
	      fatal("low_inherit with odd Pike_compiler->fake_object as parent!\n");
#endif
	    inherit.parent_offset++;
	  }
#endif
	}else{
	  inherit.parent=parent;
	  inherit.parent_identifier=parent_identifier;
	  inherit.parent_offset=-17;
	}
      }else{
	inherit.parent_offset=parent_offset;
	inherit.parent_identifier=parent_identifier;
      }
    }else{
      if(!inherit.parent)
      {
	if(parent && parent->next != parent && inherit.parent_offset)
	{
	  /* Fake object */
	  struct object *par=parent;
	  int e,pid=parent_identifier;

	  for(e=1;e<inherit.parent_offset;e++)
	  {
	    struct inherit *in;
	    if(!par->prog)
	    {
	      par=0;
	      pid=-1;
	      break;
	    }

	    in=INHERIT_FROM_INT(par->prog, pid);
	    switch(in->parent_offset)
	    {
	      default:
	      {
		struct external_variable_context tmp;
		struct inherit *in2=in;
		while(in2->identifier_level >= in->identifier_level) in2--;
		tmp.o=par;
		tmp.inherit=in2;
		tmp.parent_identifier=pid;
		find_external_context(&tmp, in->parent_offset);
		par = tmp.o;
		pid = tmp.parent_identifier;
	      }
	      break;

	      case -17:
		pid = in->parent_identifier;
		par = in->parent;
		break;

	      case -18:
		if(par->prog->flags & PROGRAM_USES_PARENT)
		{
		  pid = PARENT_INFO(par)->parent_identifier;
		  par = PARENT_INFO(par)->parent;
		}else{
		  pid=-1;
		  par=0;
		}
	    }
	  }

	  inherit.parent=par;
	  inherit.parent_offset=-17;
	}
      }
    }
    if(inherit.parent) add_ref(inherit.parent);

    if(name)
    {
      if(e==0)
      {
	copy_shared_string(inherit.name,name);
      }
      else if(inherit.name)
      {
	struct pike_string *s;
	s=begin_shared_string(inherit.name->len + name->len + 2);
	MEMCPY(s->str,name->str,name->len);
	MEMCPY(s->str+name->len,"::",2);
	MEMCPY(s->str+name->len+2,inherit.name->str,inherit.name->len);
	inherit.name=end_shared_string(s);
      }
      else
      {
	inherit.name=0;
      }
    }else{
      inherit.name=0;
    }
    add_to_inherits(inherit);
  }

  for (e=0; e < (int)p->num_identifier_references; e++)
  {
    struct reference fun;
    struct pike_string *name;

    fun = p->identifier_references[e]; /* Make a copy */

    name=ID_FROM_PTR(p,&fun)->name;
    fun.inherit_offset += inherit_offset;

    if (fun.id_flags & ID_NOMASK)
    {
      Pike_compiler->check_final++;
    }
    
    if(fun.id_flags & ID_PRIVATE) fun.id_flags|=ID_HIDDEN;

    if (fun.id_flags & ID_PUBLIC)
      fun.id_flags |= flags & ~ID_PRIVATE;
    else
      fun.id_flags |= flags;

    fun.id_flags |= ID_INHERITED;
    add_to_identifier_references(fun);
  }
}

PMOD_EXPORT void do_inherit(struct svalue *s,
		INT32 flags,
		struct pike_string *name)
{
  struct program *p=program_from_svalue(s);
  low_inherit(p,
	      s->type == T_FUNCTION ? s->u.object : 0,
	      s->type == T_FUNCTION ? s->subtype : -1,
	      0,
	      flags,
	      name);
}

void compiler_do_inherit(node *n,
			 INT32 flags,
			 struct pike_string *name)
{
  struct program *p;
  struct identifier *i;
  INT32 numid=-1, offset=0;

  if(!n)
  {
    yyerror("Unable to inherit");
    return;
  }
  switch(n->token)
  {
    case F_IDENTIFIER:
      p=Pike_compiler->new_program;
      offset=0;
      numid=n->u.id.number;
      goto continue_inherit;

    case F_EXTERNAL:
      {
	struct program_state *state = Pike_compiler;

	offset = 0;
	while (state && (state->new_program->id != n->u.integer.a)) {
	  state = state->previous;
	  offset++;
	}
	if (!state) {
	  yyerror("Failed to resolv external constant.\n");
	  return;
	}
	p = state->new_program;
	numid = n->u.integer.b;
      }

  continue_inherit:

      i=ID_FROM_INT(p, numid);

      if(IDENTIFIER_IS_CONSTANT(i->identifier_flags))
      {
	struct svalue *s=&PROG_FROM_INT(p, numid)->
	  constants[i->func.offset].sval;
	if(s->type != T_PROGRAM)
	{
	  do_inherit(s,flags,name);
	  return;
	}else{
	  p=s->u.program;
	}
      }else{
	yyerror("Inherit identifier is not a constant program");
	return;
      }

      low_inherit(p,
		  0,
		  numid,
		  offset+42,
		  flags,
		  name);
      break;

    default:
      resolv_class(n);
      do_inherit(Pike_sp-1, flags, name);
      pop_stack();
  }
}

int call_handle_inherit(struct pike_string *s)
{
  int args;

  reference_shared_string(s);
  push_string(s);
  ref_push_string(lex.current_file);
  if (error_handler && error_handler->prog) {
    ref_push_object(error_handler);
    args = 3;
  }
  else args = 2;

  if (safe_apply_handler("handle_inherit", error_handler, compat_handler,
			 args, BIT_PROGRAM|BIT_FUNCTION|BIT_ZERO))
    if (Pike_sp[-1].type != T_INT)
      return 1;
    else {
      pop_stack();
      if (!s->size_shift)
	my_yyerror("Couldn't find program: %s",s->str);
      else
	yyerror("Couldn't find program");
    }
  else {
    struct svalue thrown = throw_value;
    throw_value.type = T_INT;
    my_yyerror("Error finding program");
    push_svalue(&thrown);
    low_safe_apply_handler("compile_exception", error_handler, compat_handler, 1);
    if (SAFE_IS_ZERO(sp-1)) yy_describe_exception(&thrown);
    pop_stack();
    free_svalue(&thrown);
  }

  return 0;
}

void simple_do_inherit(struct pike_string *s,
		       INT32 flags,
		       struct pike_string *name)
{
  if (!call_handle_inherit(s)) return;

  if(name)
  {
    free_string(s);
    s=name;
  }
  do_inherit(Pike_sp-1, flags, s);
  free_string(s);
  pop_stack();
}

/*
 * Return the index of the identifier found, otherwise -1.
 */
int isidentifier(struct pike_string *s)
{
  INT32 e;
  return really_low_find_shared_string_identifier(s,
						  Pike_compiler->new_program,
						  SEE_STATIC|SEE_PRIVATE);
}

/* argument must be a shared string */
int low_define_variable(struct pike_string *name,
			struct pike_type *type,
			INT32 flags,
			size_t offset,
			INT32 run_time_type)
{
  int n;

  struct identifier dummy;
  struct reference ref;

#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->flags & (PROGRAM_FIXED | PROGRAM_OPTIMIZED))
    fatal("Attempting to add variable to fixed program\n");

  if(Pike_compiler->compiler_pass==2)
    fatal("Internal error: Not allowed to add more identifiers during second compiler pass.\n"
	  "Added identifier: \"%s\"\n", name->str);
#endif

  copy_shared_string(dummy.name, name);
  copy_pike_type(dummy.type, type);
  if (flags & ID_ALIAS) {
    dummy.identifier_flags = IDENTIFIER_ALIAS;
  } else {
    dummy.identifier_flags = 0;
  }
  dummy.run_time_type=run_time_type;
  dummy.func.offset=offset - Pike_compiler->new_program->inherits[0].storage_offset;
#ifdef PROFILING
  dummy.self_time=0;
  dummy.num_calls=0;
  dummy.total_time=0;
#endif

  ref.id_flags=flags;
  ref.identifier_offset=Pike_compiler->new_program->num_identifiers;
  ref.inherit_offset=0;

  add_to_variable_index(ref.identifier_offset);

  debug_add_to_identifiers(dummy);

  n=Pike_compiler->new_program->num_identifier_references;
  add_to_identifier_references(ref);

  return n;
}

PMOD_EXPORT int map_variable(char *name,
		 char *type,
		 INT32 flags,
		 size_t offset,
		 INT32 run_time_type)
{
  int ret;
  struct pike_string *n;
  struct pike_type *t;

#ifdef PROGRAM_BUILD_DEBUG
  fprintf (stderr, "%.*sdefining variable (pass=%d): %s %s\n",
	   compilation_depth, "                ", Pike_compiler->compiler_pass, type, name);
#endif

  n=make_shared_string(name);
  t=parse_type(type);
  ret=low_define_variable(n,t,flags,offset,run_time_type);
  free_string(n);
  free_type(t);
  return ret;
}

PMOD_EXPORT int quick_map_variable(char *name,
		       int name_length,
		       size_t offset,
		       char *type,
		       int type_length,
		       INT32 run_time_type,
		       INT32 flags)
{
  int ret;
  struct pike_string *n;
  struct pike_type *t;

  n = make_shared_binary_string(name, name_length);
#ifdef USE_PIKE_TYPE
  t = make_pike_type(type);
#else /* !USE_PIKE_TYPE */
  t = make_shared_binary_string(type, type_length);
#endif /* USE_PIKE_TYPE */

#ifdef PROGRAM_BUILD_DEBUG
  {
    struct pike_string *d = describe_type (t);
    fprintf (stderr, "%.*sdefining variable (pass=%d): %s ",
	     compilation_depth, "                ", Pike_compiler->compiler_pass, d->str);
    free_string (d);
    push_string (n);
    print_svalue (stderr, --Pike_sp);
    putc ('\n', stderr);
  }
#endif

  ret=low_define_variable(n,t,flags,offset,run_time_type);
  free_string(n);
  free_type(t);
  return ret;
}

/* argument must be a shared string */
int define_variable(struct pike_string *name,
		    struct pike_type *type,
		    INT32 flags)
{
  int n, run_time_type;

#ifdef PIKE_DEBUG
  if(name!=debug_findstring(name))
    fatal("define_variable on nonshared string.\n");
#endif

#ifdef PROGRAM_BUILD_DEBUG
  {
    struct pike_string *d = describe_type (type);
    fprintf (stderr, "%.*sdefining variable (pass=%d): %s ",
	     compilation_depth, "                ", Pike_compiler->compiler_pass, d->str);
    free_string (d);
    push_string (name);
    print_svalue (stderr, --Pike_sp);
    putc ('\n', stderr);
  }
#endif

  if(type == void_type_string)
    yyerror("Variables can't be of type void");

  n = isidentifier(name);

  if(Pike_compiler->new_program->flags & PROGRAM_PASS_1_DONE)
  {
    if(n==-1)
      yyerror("Pass2: Variable disappeared!");
    else {
      struct identifier *id=ID_FROM_INT(Pike_compiler->new_program,n);
      free_type(id->type);
      copy_pike_type(id->type, type);
      return n;
    }
  }

#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->flags & (PROGRAM_FIXED | PROGRAM_OPTIMIZED))
    fatal("Attempting to add variable to fixed program\n");
#endif

  if(n != -1)
  {
    /* not inherited */
    if(Pike_compiler->new_program->identifier_references[n].inherit_offset == 0)
    {
      if (!((IDENTIFIERP(n)->id_flags | flags) & ID_EXTERN)) {
	my_yyerror("Identifier '%s' defined twice.",name->str);
	return n;
      }
      if (flags & ID_EXTERN) {
	/* FIXME: Check type */
	return n;
      }
    }

    if (!(IDENTIFIERP(n)->id_flags & ID_EXTERN)) {
      if (IDENTIFIERP(n)->id_flags & ID_NOMASK)
	my_yyerror("Illegal to redefine 'nomask/final' "
		   "variable/functions \"%s\"", name->str);

      if(!(IDENTIFIERP(n)->id_flags & ID_INLINE) || Pike_compiler->compiler_pass!=1)
      {
	int n2;

 	if(ID_FROM_INT(Pike_compiler->new_program, n)->type != type &&
	   !pike_types_le(type,
			  ID_FROM_INT(Pike_compiler->new_program, n)->type)) {
	  if (!match_types(ID_FROM_INT(Pike_compiler->new_program, n)->type,
			   type)) {
	    my_yyerror("Illegal to redefine inherited variable '%s' "
		       "with different type.", name->str);
	    return n;
	  } else {
	    yywarning("Redefining inherited variable '%s' "
		      "with different type.", name->str);
	  }
	}
	

	if(!IDENTIFIER_IS_VARIABLE(ID_FROM_INT(Pike_compiler->new_program, n)->
				   identifier_flags))
	{
	  my_yyerror("Illegal to redefine inherited variable "
		     "with different type.");
	  return n;
	}

	if ((ID_FROM_INT(Pike_compiler->new_program, n)->run_time_type !=
	     PIKE_T_MIXED) &&
	    (ID_FROM_INT(Pike_compiler->new_program, n)->run_time_type !=
	     compile_type_to_runtime_type(type))) {
	  my_yyerror("Illegal to redefine inherited variable "
		     "with different type.");
	  return n;
	}

	/* Copy the variable reference, so that we can change the
	 * compile-time type. */
	n2 = low_define_variable(name, type,
				 (flags | ID_ALIAS) & ~ID_EXTERN,
				 ID_FROM_INT(Pike_compiler->new_program, n)->
				 func.offset +
				 INHERIT_FROM_INT(Pike_compiler->new_program,
						  n)->storage_offset,
				 ID_FROM_INT(Pike_compiler->new_program, n)->
				 run_time_type);
	/* Hide the old variable. */
	Pike_compiler->new_program->identifier_references[n].id_flags |=
	  ID_STATIC|ID_PRIVATE;
	return n2;
      }
    }
  }

  run_time_type=compile_type_to_runtime_type(type);

  switch(run_time_type)
  {
#ifdef AUTO_BIGNUM
#if 0
    case T_OBJECT:
      /* This is to allow room for integers in variables declared as
       * 'object', however, this could be changed in the future to only
       * make room for integers if the variable was declared as
       * 'object(Gmp.mpz)'                                     /Hubbe
       */
#endif
    case T_INT:
#endif
    case T_FUNCTION:
    case T_PROGRAM:
      run_time_type = T_MIXED;
  }

  n=low_define_variable(name,type,flags,
			low_add_storage(sizeof_variable(run_time_type),
					alignof_variable(run_time_type),0),
			run_time_type);


  return n;
}

PMOD_EXPORT int simple_add_variable(char *name,
			char *type,
			INT32 flags)
{
  INT32 ret;
  struct pike_string *name_s;
  struct pike_type *type_s;
  name_s = make_shared_string(name);
  type_s = parse_type(type);

  ret=define_variable(name_s, type_s, flags);
  free_string(name_s);
  free_type(type_s);
  return ret;
}

PMOD_EXPORT int add_constant(struct pike_string *name,
		 struct svalue *c,
		 INT32 flags)
{
  int n;
  struct identifier dummy;
  struct reference ref;
  struct svalue zero;

  if (!c) {
    zero.type = T_INT;
    zero.subtype = 0;
    zero.u.integer = 0;
    c = &zero;
  }

#ifdef PROGRAM_BUILD_DEBUG
  {
    if (c) {
      struct pike_type *t = get_type_of_svalue(c);
      struct pike_string *d = describe_type (t);
      fprintf (stderr, "%.*sdefining constant (pass=%d): %s ",
	       compilation_depth, "                ",
	       Pike_compiler->compiler_pass, d->str);
      free_type(t);
      free_string (d);
    }
    else
      fprintf (stderr, "%.*sdeclaring constant (pass=%d): ",
	       compilation_depth, "                ",
	       Pike_compiler->compiler_pass);
    push_string (name);
    print_svalue (stderr, --Pike_sp);
    putc ('\n', stderr);
  }
#endif

#ifdef PIKE_DEBUG
  if(name!=debug_findstring(name))
    fatal("define_constant on nonshared string.\n");
#endif

  do {
    if(c &&
       c->type == T_FUNCTION &&
       c->subtype != FUNCTION_BUILTIN &&
       c->u.object->prog)
    {
      struct identifier *id=ID_FROM_INT(c->u.object->prog, c->subtype);
      if(c->u.object->prog == Pike_compiler->new_program &&
	 !c->u.object->prog->identifier_references[c->subtype].inherit_offset)
      {
	if(id->identifier_flags & IDENTIFIER_FUNCTION)
	{
	  return define_function(name,
				 id->type,
				 flags,
				 id->identifier_flags | IDENTIFIER_ALIAS,
				 & id->func,
				 id->opt_flags);
	  
	}
	else if((id->identifier_flags & IDENTIFIER_CONSTANT) &&
		id->func.offset != -1)
	{
	  c=& Pike_compiler->new_program->constants[id->func.offset].sval;
	}
      }
      else
      {
	if((id->identifier_flags & IDENTIFIER_CONSTANT) &&
	   id->func.offset != -1 &&
	   INHERIT_FROM_INT(c->u.object->prog, c->subtype)->prog->
	   constants[id->func.offset].sval.type == T_PROGRAM)
	{
	  /* In this one case we allow fake objects to enter the
	   * mainstream...
	   */
	  break;
	}
      }
    }
    
    if(c && !svalues_are_constant(c,1,BIT_MIXED,0))
      yyerror("Constant values may not have references this_object()");
    
  }while(0);

  n = isidentifier(name);

  if(Pike_compiler->new_program->flags & PROGRAM_PASS_1_DONE)
  {
    if(n==-1)
    {
      yyerror("Pass2: Constant disappeared!");
    }else{
      struct identifier *id;
      id=ID_FROM_INT(Pike_compiler->new_program,n);
      if(id->func.offset>=0)
      {
	struct pike_type *s;
	struct svalue *c=&PROG_FROM_INT(Pike_compiler->new_program,n)->
	  constants[id->func.offset].sval;
	s=get_type_of_svalue(c);
	free_type(id->type);
	id->type=s;
      }
      else {
#ifdef PIKE_DEBUG
	if (!c) fatal("Can't declare constant during second compiler pass\n");
#endif
	free_type(id->type);
	id->type = get_type_of_svalue(c);
	id->run_time_type = c->type;
	id->func.offset = store_constant(c, 0, 0);
      }
      return n;
    }
  }

#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->flags & (PROGRAM_FIXED | PROGRAM_OPTIMIZED))
    fatal("Attempting to add constant to fixed program\n");

  if(Pike_compiler->compiler_pass==2)
    fatal("Internal error: Not allowed to add more identifiers during second compiler pass.\n");
#endif

  copy_shared_string(dummy.name, name);
  dummy.identifier_flags = IDENTIFIER_CONSTANT;

  if (c) {
    dummy.type = get_type_of_svalue(c);
    dummy.run_time_type=c->type;
    dummy.func.offset=store_constant(c, 0, 0);
    dummy.opt_flags=OPT_SIDE_EFFECT | OPT_EXTERNAL_DEPEND;
    if(c->type == PIKE_T_PROGRAM && (c->u.program->flags & PROGRAM_CONSTANT))
       dummy.opt_flags=0;
  }
  else {
    copy_pike_type(dummy.type, mixed_type_string);
    dummy.run_time_type=T_MIXED;
    dummy.func.offset=-1;
    dummy.opt_flags=0;
  }

  ref.id_flags=flags;
  ref.identifier_offset=Pike_compiler->new_program->num_identifiers;
  ref.inherit_offset=0;

#ifdef PROFILING
  dummy.self_time=0;
  dummy.num_calls=0;
  dummy.total_time=0;
#endif

  debug_add_to_identifiers(dummy);

  if(n != -1)
  {
    int overridden;

    if(IDENTIFIERP(n)->id_flags & ID_NOMASK)
      my_yyerror("Illegal to redefine 'nomask' identifier \"%s\"", name->str);

    if(!TEST_COMPAT(7,2) &&
       IDENTIFIER_IS_VARIABLE(ID_FROM_INT(Pike_compiler->new_program,
					  n)->identifier_flags))
    {
      my_yyerror("Illegal to redefine variable \"%s\" as constant.",
		 name->str);
    }

    /* not inherited */
    if(Pike_compiler->new_program->identifier_references[n].inherit_offset == 0)
    {
      my_yyerror("Identifier '%s' defined twice.",name->str);
      return n;
    }

    /* override */
    if ((overridden = override_identifier (&ref, name, 0)) >= 0) {
#ifdef PIKE_DEBUG
      if(MEMCMP(Pike_compiler->new_program->identifier_references+n, &ref,sizeof(ref)))
	fatal("New constant overriding algorithm failed!\n");
#endif
      return overridden;
    }
  }
  n=Pike_compiler->new_program->num_identifier_references;
  add_to_identifier_references(ref);

  return n;
}

PMOD_EXPORT int simple_add_constant(char *name,
			struct svalue *c,
			INT32 flags)
{
  INT32 ret;
  struct pike_string *id;
  id=make_shared_string(name);
  ret=add_constant(id, c, flags);
  free_string(id);
  return ret;
}

PMOD_EXPORT int add_integer_constant(char *name,
			 INT32 i,
			 INT32 flags)
{
  struct svalue tmp;
  tmp.u.integer=i;
  tmp.type=T_INT;
  tmp.subtype=NUMBER_NUMBER;
  return simple_add_constant(name, &tmp, flags);
}

PMOD_EXPORT int quick_add_integer_constant(char *name,
			       int name_length,
			       INT32 i,
			       INT32 flags)
{
  struct svalue tmp;
  struct pike_string *id;
  INT32 ret;

  tmp.u.integer=i;
  tmp.type=T_INT;
  tmp.subtype=NUMBER_NUMBER;
  id=make_shared_binary_string(name,name_length);
  ret=add_constant(id, &tmp, flags);
  free_string(id);
  return ret;
}

PMOD_EXPORT int add_float_constant(char *name,
			 double f,
			 INT32 flags)
{
  struct svalue tmp;
  tmp.type=T_FLOAT;
  tmp.u.float_number = DO_NOT_WARN((FLOAT_TYPE)f);
  tmp.subtype=0;
  return simple_add_constant(name, &tmp, flags);
}

PMOD_EXPORT int add_string_constant(char *name,
			char *str,
			INT32 flags)
{
  INT32 ret;
  struct svalue tmp;
  tmp.type=T_STRING;
  tmp.subtype=0;
  tmp.u.string=make_shared_string(str);
  ret=simple_add_constant(name, &tmp, flags);
  free_svalue(&tmp);
  return ret;
}

PMOD_EXPORT int add_program_constant(char *name,
			 struct program *p,
			 INT32 flags)
{
  INT32 ret;
  struct svalue tmp;
  tmp.type=T_PROGRAM;
  tmp.subtype=0;
  tmp.u.program=p;
  ret=simple_add_constant(name, &tmp, flags);
  return ret;
}

PMOD_EXPORT int add_object_constant(char *name,
			struct object *o,
			INT32 flags)
{
  INT32 ret;
  struct svalue tmp;
  tmp.type=T_OBJECT;
  tmp.subtype=0;
  tmp.u.object=o;
  ret=simple_add_constant(name, &tmp, flags);
  return ret;
}

PMOD_EXPORT int add_function_constant(char *name, void (*cfun)(INT32), char * type, INT16 flags)
{
  struct svalue s;
  INT32 ret;

  s.type=T_FUNCTION;
  s.subtype=FUNCTION_BUILTIN;
  s.u.efun=make_callable(cfun, name, type, flags, 0, 0);
  ret=simple_add_constant(name, &s, 0);
  free_svalue(&s);
  return ret;
}


PMOD_EXPORT int debug_end_class(char *name, ptrdiff_t namelen, INT32 flags)
{
  INT32 ret;
  struct svalue tmp;
  struct pike_string *id;

  tmp.type=T_PROGRAM;
  tmp.subtype=0;
  tmp.u.program=end_program();
  if(!tmp.u.program)
    fatal("Failed to initialize class '%s'\n",name);

  id=make_shared_binary_string(name,namelen);
  ret=add_constant(id, &tmp, flags);
  free_string(id);
  free_svalue(&tmp);
  return ret;
}

/*
 * define a new function
 * if func isn't given, it is supposed to be a prototype.
 */
INT32 define_function(struct pike_string *name,
		      struct pike_type *type,
		      unsigned INT16 flags,
		      unsigned INT8 function_flags,
		      union idptr *func,
		      unsigned INT16 opt_flags)
{
  struct identifier *funp,fun;
  struct reference ref;
  struct svalue *lfun_type;
  INT32 i;

#ifdef PROGRAM_BUILD_DEBUG
  {
    struct pike_string *d = describe_type (type);
    fprintf (stderr, "%.*sdefining function (pass=%d): %s ",
	     compilation_depth, "                ", Pike_compiler->compiler_pass, d->str);
    free_string (d);
    push_string (name);
    print_svalue (stderr, --Pike_sp);
    putc ('\n', stderr);
  }
#endif

#ifdef PROFILING
  fun.self_time=0;
  fun.num_calls=0;
  fun.total_time=0;
#endif

  /* If this is an lfun, match against the predefined type. */
  if ((lfun_type = low_mapping_string_lookup(lfun_types, name))) {
#ifdef PIKE_DEBUG
    if (lfun_type->type != T_TYPE) {
      fatal("Bad entry in lfun_types for key \"%s\"\n", name->str);
    }
#endif /* PIKE_DEBUG */
    if (!pike_types_le(type, lfun_type->u.type)) {
      if (!match_types(type, lfun_type->u.type)) {
	yytype_error("Function type mismatch", lfun_type->u.type, type, 0);
      } else if (lex.pragmas & ID_STRICT_TYPES) {
	yytype_error("Function type mismatch", lfun_type->u.type, type,
		     YYTE_IS_WARNING);
      }
    }
  }

  if(function_flags & IDENTIFIER_C_FUNCTION)
    Pike_compiler->new_program->flags |= PROGRAM_HAS_C_METHODS;

  i=isidentifier(name);

#ifdef PROGRAM_BUILD_DEBUG
  fprintf(stderr, "%.*sexisted as identifier #%d\n", compilation_depth, "", i);
#endif

  if(i >= 0)
  {
    /* already defined */

    funp=ID_FROM_INT(Pike_compiler->new_program, i);
    ref=Pike_compiler->new_program->identifier_references[i];

    if(!(ref.id_flags & ID_INHERITED)) /* not inherited */
    {

      if( !( IDENTIFIER_IS_FUNCTION(funp->identifier_flags) &&
	     ( (!func || func->offset == -1) || (funp->func.offset == -1))))
      {
	my_yyerror("Identifier '%s' defined twice.",name->str);
	return i;
      }

      /* match types against earlier prototype or vice versa */
      if(!match_types(type, funp->type))
      {
	if (!(flags & ID_VARIANT)) {
	  my_yyerror("Prototype doesn't match for function %s.",name->str);
	}
      }
    }

    /* We modify the old definition if it is in this program */

    if(!(ref.id_flags & ID_INHERITED))
    {
      if(func)
	funp->func = *func;
#if 0 /* prototypes does not override non-prototypes, ok? */
      else
	funp->func.offset = -1;
#endif

      funp->identifier_flags=function_flags;

      funp->opt_flags &= opt_flags;

      free_type(funp->type);
      copy_pike_type(funp->type, type);
    }else{
#ifdef PROGRAM_BUILD_DEBUG
      fprintf(stderr, "%.*sidentifier was inherited\n", compilation_depth, "");
#endif

      if((ref.id_flags & ID_NOMASK)
#if 0
	 && !(funp->func.offset == -1)
#endif
	)
      {
	my_yyerror("Illegal to redefine 'nomask' function %s.",name->str);
      }


      if(ref.id_flags & ID_INLINE)
      {
#ifdef PROGRAM_BUILD_DEBUG
	fprintf(stderr, "%.*sidentifier is local\n", compilation_depth, "");
#endif

	goto make_a_new_def;
      }

      /* Otherwise we alter the existing definition */
#ifdef PROGRAM_BUILD_DEBUG
      fprintf(stderr, "%.*saltering the existing definition\n",
	      compilation_depth, "");
#endif

      copy_shared_string(fun.name, name);
      copy_pike_type(fun.type, type);

      fun.run_time_type=T_FUNCTION;

      fun.identifier_flags=function_flags;
      if(function_flags & IDENTIFIER_C_FUNCTION)
	Pike_compiler->new_program->flags |= PROGRAM_HAS_C_METHODS;

      if(func)
	fun.func = *func;
      else
	fun.func.offset = -1;

      fun.opt_flags = opt_flags;

      ref.identifier_offset=Pike_compiler->new_program->num_identifiers;
      debug_add_to_identifiers(fun);
    }

    ref.inherit_offset = 0;
    ref.id_flags = flags;
    override_identifier (%ref, name, 0);
#ifdef PIKE_DEBUG
    if(MEMCMP(Pike_compiler->new_program->identifier_references+i, &ref,sizeof(ref)))
      fatal("New function overloading algorithm failed!\n");
#endif
    }
#endif
    return i;
  }
make_a_new_def:

#ifdef PROGRAM_BUILD_DEBUG
  fprintf(stderr, "%.*smaking a new definition\n", compilation_depth, "");
#endif

#ifdef PIKE_DEBUG
  if(Pike_compiler->compiler_pass==2)
    fatal("Internal error: Not allowed to add more identifiers during second compiler pass.\n");
#endif

  /* define a new function */

  copy_shared_string(fun.name, name);
  copy_pike_type(fun.type, type);

  fun.identifier_flags=function_flags;
  fun.run_time_type=T_FUNCTION;

  if(func)
    fun.func = *func;
  else
    fun.func.offset = -1;

  fun.opt_flags = opt_flags;

  i=Pike_compiler->new_program->num_identifiers;

  debug_add_to_identifiers(fun);

  ref.id_flags = flags;
  ref.identifier_offset = i;
  ref.inherit_offset = 0;

  i=Pike_compiler->new_program->num_identifier_references;
  add_to_identifier_references(ref);

  return i;
}

/* Identifier lookup
 *
 * The search algorithm has changed several times during Pike 7.3.
 *
 * It now (Pike 7.3.33 and later) looks up the most recent definition
 * in the most recent inherit.
 *
 * In Pike 7.3.23 -- 7.3.32 it looked up the most recent definition
 * with the least inherit depth.
 *
 * In Pike 7.3.22 and prior, it looked up the last definition regardless
 * of inherit depth, unless there was a definition in the current program.
 *
 * Example:
 *
 * class A {
 *   int foo() {}
 * }
 *
 * class B {
 *   int foo() {}
 *   inherit A;
 * }
 *
 * class C {
 *   inherit B;
 * }
 *
 * class D {
 *   inherit B;
 *   inherit C;
 * }
 *
 * Lookup of identifier "foo" in D():
 *
 * D-+-B-+-foo		Pike 7.3.23 --- Pike 7.3.32
 *   |   |
 *   |   +-A---foo
 *   |
 *   +-C---B-+-foo	Pike 7.3.33 ---
 *           |
 *           +-A---foo	            --- Pike 7.3.22
 *
 * Lookup of identifier "foo" in C():
 *
 * C---B-+-foo		Pike 7.3.23 ---
 *       |
 *       +-A---foo	            --- Pike 7.3.22
 *
 * Lookup of identifier "foo" in B():
 *
 * B-+-foo		All versions of Pike
 *   |
 *   +-A---foo
 */
int really_low_find_shared_string_identifier(struct pike_string *name,
					     struct program *prog,
					     int flags)
{
  struct reference *funp;
  struct identifier *fun;
  int id, i, depth, last_inh;

#if 0
  CDFPRINTF((stderr,"th(%ld) Trying to find %s flags=%d\n",
	     (long)th_self(),name->str, flags));
#endif

#ifdef PIKE_DEBUG
  if (!prog) {
    fatal("really_low_find_shared_string_identifier(\"%s\", NULL, %d)\n"
	  "prog is NULL!\n", name->str, flags);
  }
#endif /* PIKE_DEBUG */

  id = -1;
  depth = 0;
  last_inh = prog->num_inherits;
  i = (int)prog->num_identifier_references;
  while(i--)
  {
    funp = prog->identifier_references + i;
    if(funp->id_flags & ID_HIDDEN) continue;
    if(funp->id_flags & ID_STATIC)
      if(!(flags & SEE_STATIC))
	continue;
    fun = ID_FROM_PTR(prog, funp);
    /* if(fun->func.offset == -1) continue; * Prototype */
    if(!is_same_string(fun->name,name)) continue;
    if(funp->id_flags & ID_INHERITED)
    {
      struct inherit *inh = INHERIT_FROM_PTR(prog, funp);
      if ((funp->id_flags & ID_PRIVATE) && !(flags & SEE_PRIVATE)) continue;
      if (!depth || (depth > inh->inherit_level)) {
	if (id != -1) {
	  int j;
	  int min_level = depth;
	  for (j=last_inh-1; j > funp->inherit_offset; j--) {
	    struct inherit *inh2 = prog->inherits + j;
	    if (inh2->inherit_level >= min_level) {
	      /* Got deeper in the inherit graph */
	      continue;
	    }
	    min_level = inh2->inherit_level;
	  }
	  if (!(inh->inherit_level < min_level)) {
	    continue;
	  }
	  /* Found new identifier on the path from the old identifier to
	   * the root.
	   */
	}
	last_inh = funp->inherit_offset;
	depth = inh->inherit_level;
	id = i;
      }
    } else {
      return i;
    }
  }
  return id;
}

int low_find_lfun(struct program *p, ptrdiff_t lfun)
{
  struct pike_string *lfun_name = lfun_strings[lfun];
  unsigned int flags = 0;
  return
    really_low_find_shared_string_identifier(lfun_name,
					     dmalloc_touch(struct program *,
							   p),
					     SEE_STATIC);
}

int lfun_lookup_id(struct pike_string *lfun_name)
{
  struct svalue *id = low_mapping_string_lookup(lfun_ids, lfun_name);
  if (!id) return -1;
  if (id->type == T_INT) return id->u.integer;
  my_yyerror("Bad entry in lfun lookup table for %s.", lfun_name->str);
  return -1;
}

/*
 * lookup the number of a function in a program given the name in
 * a shared_string
 */
int low_find_shared_string_identifier(struct pike_string *name,
				      struct program *prog)
{
  int max,min,tst;
  struct identifier *fun;

  if(prog->flags & PROGRAM_FIXED)
  {
    unsigned short *funindex = prog->identifier_index;

#ifdef PIKE_DEBUG
    if(!funindex)
      fatal("No funindex in fixed program\n");
#endif

    max = prog->num_identifier_index;
    min = 0;
    while(max != min)
    {
      tst=(max + min) >> 1;
      fun = ID_FROM_INT(prog, funindex[tst]);
      if(is_same_string(fun->name,name)) return funindex[tst];
      if(my_order_strcmp(fun->name, name) > 0)
	max=tst;
      else
	min=tst+1;
    }
  }else{
    return really_low_find_shared_string_identifier(name,prog,0);
  }
  return -1;
}

#ifdef FIND_FUNCTION_HASHSIZE
#if FIND_FUNCTION_HASHSIZE == 0
#undef FIND_FUNCTION_HASHSIZE
#endif
#endif

#ifdef FIND_FUNCTION_HASHSIZE
struct ff_hash
{
  struct pike_string *name;
  int id;
  int fun;
};

static struct ff_hash cache[FIND_FUNCTION_HASHSIZE];
#endif

int find_shared_string_identifier(struct pike_string *name,
				  struct program *prog)
{
#ifdef PIKE_DEBUG
  if (!prog) {
    fatal("find_shared_string_identifier(): No program!\n"
	  "Identifier: %s%s%s\n",
	  name?"\"":"", name?name->str:"NULL", name?"\"":"");
  }
#endif /* PIKE_DEBUG */
#ifdef FIND_FUNCTION_HASHSIZE
  if(prog -> flags & PROGRAM_FIXED
#if FIND_FUNCTION_HASH_TRESHOLD - 0
     && prog->num_identifier_index >= FIND_FUNCTION_HASH_TRESHOLD
#endif
    )
  {
    size_t hashval;
    hashval = my_hash_string(name);
    hashval += prog->id;
    hashval %= FIND_FUNCTION_HASHSIZE;
    if(is_same_string(cache[hashval].name,name) &&
       cache[hashval].id==prog->id)
      return cache[hashval].fun;

    if(cache[hashval].name) free_string(cache[hashval].name);
    copy_shared_string(cache[hashval].name,name);
    cache[hashval].id=prog->id;
    return cache[hashval].fun=low_find_shared_string_identifier(name,prog);
  }
#endif /* FIND_FUNCTION_HASHSIZE */

  return low_find_shared_string_identifier(name,prog);
}

PMOD_EXPORT int find_identifier(const char *name,struct program *prog)
{
  struct pike_string *n;
  if(!prog) {
    if (strlen(name) < 1024) {
      Pike_error("Lookup of identifier %s in destructed object.\n", name);
    } else {
      Pike_error("Lookup of long identifier in destructed object.\n");
    }
  }
  n=findstring(name);
  if(!n) return -1;
  return find_shared_string_identifier(n,prog);
}

int store_prog_string(struct pike_string *str)
{
  unsigned int i;

  for (i=0;i<Pike_compiler->new_program->num_strings;i++)
    if (Pike_compiler->new_program->strings[i] == str)
      return i;

  reference_shared_string(str);
  add_to_strings(str);
  return i;
}

int store_constant(struct svalue *foo,
		   int equal,
		   struct pike_string *constant_name)
{
  struct program_constant tmp;
  unsigned int e;
  JMP_BUF tmp2;

  if(SETJMP(tmp2))
  {
    struct svalue zero, thrown = throw_value;
    throw_value.type = T_INT;

    yyerror("Couldn't store constant.");

    push_svalue(&thrown);
    low_safe_apply_handler("compile_exception", error_handler, compat_handler, 1);
    if (SAFE_IS_ZERO(sp-1)) yy_describe_exception(&thrown);
    pop_stack();
    free_svalue(&thrown);

    zero.type = T_INT;
    zero.subtype = NUMBER_NUMBER;
    zero.u.integer=0;

    UNSETJMP(tmp2);
    return store_constant(&zero, equal, constant_name);
  }else{
    for(e=0;e<Pike_compiler->new_program->num_constants;e++)
    {
      JMP_BUF tmp1;
      /* Assume that if `==() throws an error, the svalues aren't equal. */
      if (!SETJMP(tmp1)) {
	struct program_constant *c= Pike_compiler->new_program->constants+e;
	if((equal ? is_equal(& c->sval,foo) : is_eq(& c->sval,foo)) &&
	   c->name == constant_name)
	{
	  UNSETJMP(tmp1);
	  UNSETJMP(tmp2);
	  return e;
	}
      }
      UNSETJMP(tmp1);
    }
    assign_svalue_no_free(&tmp.sval,foo);
    if((tmp.name=constant_name)) add_ref(constant_name);

    add_to_constants(tmp);

    UNSETJMP(tmp2);
    return e;
  }
}

/*
 * program examination functions available from Pike.
 */

struct array *program_indices(struct program *p)
{
  int e;
  int n = 0;
  struct array *res;
  for (e = p->num_identifier_references; e--; ) {
    struct identifier *id;
    if (p->identifier_references[e].id_flags & ID_HIDDEN) {
      continue;
    }
    id = ID_FROM_INT(p, e);
    if (IDENTIFIER_IS_CONSTANT(id->identifier_flags)) {
      struct program *p2 = PROG_FROM_INT(p, e);
      struct svalue *val = &p2->constants[id->func.offset].sval;
      if ((val->type != T_PROGRAM) ||
	  !(val->u.program->flags & PROGRAM_USES_PARENT)) {
	ref_push_string(ID_FROM_INT(p, e)->name);
	n++;
      }
    }
  }
  f_aggregate(n);
  res = Pike_sp[-1].u.array;
  add_ref(res);
  pop_stack();
  return(res);
}

struct array *program_values(struct program *p)
{
  int e;
  int n = 0;
  struct array *res;
  for(e = p->num_identifier_references; e--; ) {
    struct identifier *id;
    if (p->identifier_references[e].id_flags & ID_HIDDEN) {
      continue;
    }
    id = ID_FROM_INT(p, e);
    if (IDENTIFIER_IS_CONSTANT(id->identifier_flags)) {
      struct program *p2 = PROG_FROM_INT(p, e);
      struct svalue *val = &p2->constants[id->func.offset].sval;
      if ((val->type != T_PROGRAM) ||
	  !(val->u.program->flags & PROGRAM_USES_PARENT)) {
	push_svalue(val);
	n++;
      }
    }
  }
  f_aggregate(n);
  res = Pike_sp[-1].u.array;
  add_ref(res);
  pop_stack();
  return(res);
}

void program_index_no_free(struct svalue *to, struct program *p,
			   struct svalue *ind)
{
  int e;
  struct pike_string *s;

  if (ind->type != T_STRING) {
    Pike_error("Can't index a program with a %s (expected string)\n",
	  get_name_of_type(ind->type));
  }
  s = ind->u.string;
  e=find_shared_string_identifier(s, p);
  if(e!=-1)
  {
    struct identifier *id;
    id=ID_FROM_INT(p, e);
    if (IDENTIFIER_IS_CONSTANT(id->identifier_flags)) {
      struct program *p2 = PROG_FROM_INT(p, e);
      struct svalue *val = &p2->constants[id->func.offset].sval;
      assign_svalue_no_free(to, val);
      return;
    }
  }

  to->type=T_INT;
  to->subtype=NUMBER_UNDEFINED;
  to->u.integer=0;
}

/*
 * Line number support routines, now also tells what file we are in
 */

/* program.linenumbers format:
 *
 * Filename entry:
 *   1. char		127 (marker).
 *   2. small number	Filename string length.
 *   3. char		Filename string size shift.
 *   4. string data	(Possibly wide) filename string without null termination.
 * 			Each character is stored in native byte order.
 * 
 * Line number entry:
 *   1. small number	Index in program.program (pc).
 * 			Stored as the difference from the pc in the
 * 			closest previous line number entry. The first
 * 			stored entry is absolute.
 *   2. small number	Line number. Stored in the same way as the pc.
 * 
 * Small number:
 *   If -127 < n < 127:
 *     1. char		The number.
 *   Else if -32768 <= n < 32768:
 *     1. char		-127 (marker).
 *     2. short		The number stored in native byte order.
 *   Else:
 *     1. char		-128 (marker).
 *     2. int		The number stored in native byte order.
 *
 * Whenever the filename changes, a filename entry followed by a line
 * number entry is stored. If only the line number changes, a line
 * number entry is stored. The first stored entry (at pc 0) is the
 * file and line where the program is defined, if they are known. The
 * definition line for a top level program is set to 0.
 */

int get_small_number(char **q)
{
  /* This is a workaround for buggy cc & Tru64 */
  int ret;
  ret=*(signed char *)*q;
  (*q)++;
  switch(ret)
  {
  case -127:
    ret=EXTRACT_WORD((unsigned char*)*q);
    *q+=2;
    return ret;

  case -128:
    ret=EXTRACT_INT((unsigned char*)*q);
    *q+=4;
    return ret;

#ifdef PIKE_DEBUG
  case 127:
    fatal("get_small_number used on filename entry\n");
#endif

  default:
    return ret;
  }
}

void start_line_numbering(void)
{
  if(Pike_compiler->last_file)
  {
    free_string(Pike_compiler->last_file);
    Pike_compiler->last_file=0;
  }
  Pike_compiler->last_pc=Pike_compiler->last_line=0;
}

static void insert_small_number(INT32 a)
{
#ifdef PIKE_DEBUG
  int start = Pike_compiler->new_program->num_linenumbers;
#endif /* PIKE_DEBUG */
  if(a>-127 && a<127)
  {
    add_to_linenumbers(a);
  }else if(a>=-32768 && a<32768){
    add_to_linenumbers(-127);
    ins_short(a, add_to_linenumbers);
  }else{
    add_to_linenumbers(-128);
    ins_int(a, add_to_linenumbers);
  }
#ifdef PIKE_DEBUG
  {
    unsigned char *tmp = Pike_compiler->new_program->linenumbers + start;
    INT32 res = get_small_number((char **)&tmp);
    if (a != res) {
      tmp = Pike_compiler->new_program->linenumbers + start;
      fprintf(stderr, "0x%p: %02x %02x %02x %02x %02x\n",
	      tmp, tmp[0], tmp[1], tmp[2], tmp[3], tmp[4]);	      
      fatal("insert_small_number failed: %d (0x%08x) != %d (0x%08x)\n",
	    a, a, res, res);
    }
  }
#endif /* PIKE_DEBUG */
}

void store_linenumber(INT32 current_line, struct pike_string *current_file)
{
/*  if(!store_linenumbers)  fatal("Fnord.\n"); */
#ifdef PIKE_DEBUG
  if(a_flag)
  {
    INT32 line=0, off=0;
    size_t len = 0;
    INT32 shift = 0;
    char *file=0;
    char *cnt=Pike_compiler->new_program->linenumbers;

    if (a_flag > 50) {
      fprintf(stderr, "store_linenumber(%d, \"%s\") at pc %d\n",
	      current_line, current_file->str,
	      (INT32) PIKE_PC);
      fprintf(stderr, "  last_line:%d last_file:\"%s\"\n",
	      Pike_compiler->last_line,
	      Pike_compiler->last_file?Pike_compiler->last_file->str:"");
    }

    while(cnt < Pike_compiler->new_program->linenumbers +
	  Pike_compiler->new_program->num_linenumbers)
    {
      char *start = cnt;
      if(*cnt == 127)
      {
	cnt++;
	len = get_small_number(&cnt);
	shift = *cnt;
	file = ++cnt;
	cnt += len<<shift;
	if (a_flag > 10) {
	  fprintf(stderr, "Filename entry:\n"
		  "  len: %d, shift: %d\n",
		  len, shift);
	}
      }
      off+=get_small_number(&cnt);
      line+=get_small_number(&cnt);
      if (a_flag > 100) {
	fprintf(stderr, "  off: %d, line: %d\n"
		"  raw: ",
		off, line);
	for (;start < cnt; start++) {
	  fprintf(stderr, "%02x ", *((unsigned char *)start));
	}
	fprintf(stderr, "\n");
      }
    }

    if(Pike_compiler->last_line != line ||
       Pike_compiler->last_pc != off ||
       (Pike_compiler->last_file && file &&
	memcmp(Pike_compiler->last_file->str, file, len<<shift)))
    {
      fatal("Line numbering out of whack\n"
	    "    (line : %d ?= %d)!\n"
	    "    (  pc : %d ?= %d)!\n"
	    "    (shift: %d ?= %d)!\n"
	    "    (len  : %d ?= %d)!\n"
	    "    (file : %s ?= %s)!\n",
	    Pike_compiler->last_line, line,
	    Pike_compiler->last_pc, off,
	    Pike_compiler->last_file?Pike_compiler->last_file->size_shift:0,
	    shift,
	    Pike_compiler->last_file?Pike_compiler->last_file->len:0, len,
	    Pike_compiler->last_file?Pike_compiler->last_file->str:"N/A",
	    file?file:"N/A");
    }
  }
#endif
  if(Pike_compiler->last_line != current_line ||
     Pike_compiler->last_file != current_file)
  {
    if(Pike_compiler->last_file != current_file)
    {
      char *tmp;
      INT32 remain = DO_NOT_WARN((INT32)current_file->len)<<
	current_file->size_shift;

      if(Pike_compiler->last_file) free_string(Pike_compiler->last_file);
      add_to_linenumbers(127);
      insert_small_number(DO_NOT_WARN((INT32)current_file->len));
      add_to_linenumbers(current_file->size_shift);
      for(tmp=current_file->str; remain-- > 0; tmp++)
	add_to_linenumbers(*tmp);
      copy_shared_string(Pike_compiler->last_file, current_file);
    }
    insert_small_number(DO_NOT_WARN((INT32)(PIKE_PC-Pike_compiler->last_pc)));
    insert_small_number(current_line-Pike_compiler->last_line);
    Pike_compiler->last_line = current_line;
    Pike_compiler->last_pc = DO_NOT_WARN((INT32)PIKE_PC);
  }
}

/* Returns the file where the program is defined. The line of the
 * class start is written to linep, or 0 if the program is the top
 * level of the file. */
PMOD_EXPORT struct pike_string *get_program_line(struct program *prog,
						 INT32 *linep)
{
  char *cnt;
  size_t len = 0;
  INT32 shift = 0;
  char *file = NULL;

  if(prog == Pike_compiler->new_program)
  {
    struct pike_string *optimizer;
    linep[0]=0;
    MAKE_CONSTANT_SHARED_STRING(optimizer, "Optimizer");
    return optimizer;
  }

  cnt = prog->linenumbers;
  if (cnt < prog->linenumbers + prog->num_linenumbers) {
    if (*cnt == 127) {
      cnt++;
      len = get_small_number(&cnt);
      shift = *cnt;
      file = ++cnt;
      cnt += len<<shift;
    }
    get_small_number(&cnt);	/* Ignore the offset */
    *linep = get_small_number(&cnt);
  }
  else *linep = 0;
  if (file) {
    struct pike_string *str = begin_wide_shared_string(len, shift);
    memcpy(str->str, file, len<<shift);
    return end_shared_string(str);
  } else {
    struct pike_string *dash;
    MAKE_CONSTANT_SHARED_STRING(dash, "-");
    return dash;
  }
}

#ifdef PIKE_DEBUG
/* Same as get_program_line but only used for debugging,
 * returns a char* 
 * This is important because this function may be called
 * after the shared string table has expired.
 */
char *debug_get_program_line(struct program *prog,
		       INT32 *linep)
{
  char *cnt;
  size_t len = 0;
  INT32 shift = 0;
  char *file = NULL;
  static char buffer[1025];

  if(prog == Pike_compiler->new_program)
  {
    linep[0]=0;
    return "optimizer";
  }

  cnt = prog->linenumbers;
  if (cnt < prog->linenumbers + prog->num_linenumbers) {
    if (*cnt == 127) {
      cnt++;
      len = get_small_number(&cnt);
      shift = *cnt;
      file = ++cnt;
      cnt += len<<shift;
    }
    get_small_number(&cnt);	/* Ignore the offset */
    *linep = get_small_number(&cnt);
  }
  else *linep = 0;
  if (file) {
    if(shift)
    {
      PCHARP from=MKPCHARP(file, shift);
      size_t ptr=0;
      while(ptr < NELEM(buffer)-20 &&
	    EXTRACT_PCHARP(from))
      {
	if(EXTRACT_PCHARP(from) > 255)
	{
	  sprintf(buffer+ptr,"\\0x%x",EXTRACT_PCHARP(from));
	  ptr+=strlen(buffer+ptr);
	}else{
	  buffer[ptr++]=EXTRACT_PCHARP(from);
	}
	INC_PCHARP(from, 1);
      }
      buffer[ptr]=0;
      return buffer;
    }else{
      return file;
    }
  } else {
    return "-";
  }
}
#endif

/*
 * return the file in which we were executing. pc should be the
 * program counter (i.e. the address in prog->program), prog the
 * current program, and line will be initialized to the line in that
 * file.
 */
PMOD_EXPORT struct pike_string *get_line(PIKE_OPCODE_T *pc,
					 struct program *prog, INT32 *linep)
{
  static char *file = NULL;
  static char *cnt;
  static INT32 off,line,pid;
  static size_t len;
  static INT32 shift;
  ptrdiff_t offset;

  linep[0] = 0;

  if (prog == 0) {
    struct pike_string *unknown_program;
    MAKE_CONSTANT_SHARED_STRING(unknown_program, "Unknown program");
    return unknown_program;
  }

  if(prog == Pike_compiler->new_program)
  {
    struct pike_string *optimizer;
    MAKE_CONSTANT_SHARED_STRING(optimizer, "Optimizer");
    return optimizer;
  }

  offset = pc - prog->program;

  if ((offset > (ptrdiff_t)prog->num_program) || (offset < 0)) {
    struct pike_string *not_found;
    MAKE_CONSTANT_SHARED_STRING(not_found, "Line not found");
    return not_found;
  }

  if(prog->id != pid || offset < off)
  {
    cnt=prog->linenumbers;
    off=line=0;
    pid=prog->id;
    file = 0;
  }else{
    goto fromold;
  }

  while(cnt < prog->linenumbers + prog->num_linenumbers)
  {
    if(*cnt == 127)
    {
      cnt++;
      len = get_small_number(&cnt);
      shift = *cnt;
      file = ++cnt;
      cnt += len<<shift;
    }
    off+=get_small_number(&cnt);
    if(off > offset) break;
  fromold:
    line+=get_small_number(&cnt);
  }
  linep[0]=line;
  if (!file) {
    struct pike_string *not_found;
    MAKE_CONSTANT_SHARED_STRING(not_found, "Line not found");
    return not_found;
  } else {
    struct pike_string *res = begin_wide_shared_string(len, shift);
    memcpy(res->str, file, len<<shift);
    return end_shared_string(res);
  }
}

void my_yyerror(char *fmt,...)  ATTRIBUTE((format(printf,1,2)))
{
  va_list args;
  char buf[8192];

  va_start(args,fmt);

#ifdef HAVE_VSNPRINTF
  vsnprintf(buf, 8190, fmt, args);
#else /* !HAVE_VSNPRINTF */
  VSPRINTF(buf, fmt, args);
#endif /* HAVE_VSNPRINTF */

  if((size_t)strlen(buf) >= (size_t)sizeof(buf))
    fatal("Buffer overflow in my_yyerror.\n");

  yyerror(buf);
  va_end(args);
}

void yy_describe_exception(struct svalue *thrown)
{
  /* FIXME: Doesn't handle wide string error messages. */
  struct pike_string *s = 0;

  if ((thrown->type == T_ARRAY) && thrown->u.array->size &&
      (thrown->u.array->item[0].type == T_STRING)) {
    /* Old-style backtrace */
    s = thrown->u.array->item[0].u.string;
  } else if (thrown->type == T_OBJECT) {
    struct generic_error_struct *ge;
    if ((ge = (struct generic_error_struct *)
	 get_storage(thrown->u.object, generic_error_program))) {
      s = ge->desc;
    }
  }

  if (s && !s->size_shift) {
    extern void f_string_trim_all_whites(INT32 args);
    ref_push_string(s);
    f_string_trim_all_whites(1);
    push_constant_text("\n");
    push_constant_text(" ");
    f_replace(3);
    my_yyerror(sp[-1].u.string->str);
    pop_stack();
  }
}

extern void yyparse(void);

#ifdef PIKE_DEBUG
#define do_yyparse() do {				\
  struct svalue *save_sp=Pike_sp;			\
  yyparse();  /* Parse da program */			\
  if(save_sp != Pike_sp)				\
    fatal("yyparse() left %"PRINTPTRDIFFT"d droppings on the stack!\n",	\
	  Pike_sp - save_sp);						\
}while(0)
#else
#define do_yyparse() yyparse()
#endif

struct Supporter *current_supporter=0;
int force_resolve = 0;


#ifdef PIKE_DEBUG

struct supporter_marker
{
  struct supporter_marker *next;
  void *data;
  int level, verified;
};

#undef EXIT_BLOCK
#define EXIT_BLOCK(P)
#undef COUNT_OTHER
#define COUNT_OTHER()

#undef INIT_BLOCK
#define INIT_BLOCK(X) do { (X)->level = (X)->verified = 0; }while(0)
PTR_HASH_ALLOC(supporter_marker, 128);

static int supnum;

#define SNUM(X) (get_supporter_marker((X))->level)

static void mark_supporters(struct Supporter *s)
{
  struct supporter_marker *m;

  if(!s) return;
  debug_malloc_touch(s);
  m=get_supporter_marker(s);

  if(m->level) return;
  m->level = -1;

  if(s->magic != 0x500b0127)
  {
#ifdef DEBUG_MALLOC
    describe(s);
#endif
    fatal("This is not a supporter (addr=%p, magic=%x)!\n",s,s->magic);
  }

  mark_supporters(s->dependants);
  mark_supporters(s->next_dependant);

  m->level=supnum++;

  mark_supporters(s->previous);
  mark_supporters(s->depends_on);
}

static void low_verify_supporters(struct Supporter *s)
{
  struct Supporter *ss;
  struct supporter_marker *m;

  if(!s) return;
  debug_malloc_touch(s);
  m=get_supporter_marker(s);

  if(m->verified) return;
  m->verified = 1;

  low_verify_supporters(s->dependants);
  low_verify_supporters(s->next_dependant);

#if 0
  fprintf(stderr, "low_verify_supporters %p%s, level %d: "
	  "previous %p, depends_on %p, dependants %p, next_dependant %p\n",
	  s, s == current_supporter ? " == current_supporter" : "",
	  m->level, s->previous, s->depends_on, s->dependants, s->next_dependant);
#endif

  if(s->previous && SNUM(s->previous) <= m->level)
    fatal("Que, numbers out of whack1\n");

  if(s->depends_on && SNUM(s->depends_on) <= m->level)
    fatal("Que, numbers out of whack2\n");

  for(ss=s->dependants;ss;ss=ss->next_dependant) {
    if (ss->depends_on != s)
      fatal("Dependant hasn't got depends_on set properly.\n");
    if(SNUM(ss) >= m->level)
      fatal("Que, numbers out of whack3\n");
  }

  low_verify_supporters(s->previous);
  low_verify_supporters(s->depends_on);
}

void verify_supporters()
{
  if(d_flag)
  {
    supnum=1;
    init_supporter_marker_hash();

#if 0
    fprintf(stderr, "verify_supporters start\n");
#endif

    mark_supporters(current_supporter);
    low_verify_supporters(current_supporter);
#ifdef DO_PIKE_CLEANUP
    {
      size_t e=0;
      struct supporter_marker *h;
      for(e=0;e<supporter_marker_hash_table_size;e++)
	while(supporter_marker_hash_table[e])
	  remove_supporter_marker(supporter_marker_hash_table[e]->data);
    }
#endif
    exit_supporter_marker_hash();

#if 0
    fprintf(stderr, "verify_supporters end\n");
#endif
  }
}
#else
#define verify_supporters();
#endif

void init_supporter(struct Supporter *s,
		    supporter_callback *fun,
		    void *data)
{
  verify_supporters();
#ifdef PIKE_DEBUG
  s->magic = 0x500b0127;
#endif
  s->previous=current_supporter;
  current_supporter=s;

  s->depends_on=0;
  s->dependants=0;
  s->next_dependant=0;
  s->fun=fun;
  s->data=data;
  s->prog=0;
  verify_supporters();
}

int unlink_current_supporter(struct Supporter *c)
{
  int ret=0;
#ifdef PIKE_DEBUG
  if(c != current_supporter)
    fatal("Previous unlink failed.\n");
#endif
  debug_malloc_touch(c);
  verify_supporters();
  if(c->depends_on)
  {
#ifdef PIKE_DEBUG
    struct Supporter *s;
    for (s = c->depends_on->dependants; s; s = s->next_dependant)
      if (s == c) fatal("Dependant already linked in.\n");
#endif
    ret++;
    c->next_dependant = c->depends_on->dependants;
    c->depends_on->dependants=c;
  }
  current_supporter=c->previous;
  verify_supporters();
  return ret;
}

void free_supporter(struct Supporter *c)
{
  verify_supporters();
  if (c->depends_on) {
    struct Supporter **s;
    for (s = &c->depends_on->dependants; *s; s = &(*s)->next_dependant)
      if (*s == c) {*s = c->next_dependant; break;}
    c->depends_on = 0;
  }
  verify_supporters();
}

int call_dependants(struct Supporter *s, int finish)
{
  int ok = 1;
  struct Supporter *tmp;
  verify_supporters();
  while((tmp=s->dependants))
  {
    s->dependants=tmp->next_dependant;
#ifdef PIKE_DEBUG
    tmp->next_dependant=0;
#endif
    verify_supporters();
    if (!tmp->fun(tmp->data, finish)) ok = 0;
    verify_supporters();
  }
  return ok;
}

int report_compiler_dependency(struct program *p)
{
  int ret=0;
  struct Supporter *c,*cc;
  verify_supporters();
  if (force_resolve)
    return 0;
  for(cc=current_supporter;cc;cc=cc->previous)
  {
    if(cc->prog &&
       !(cc->prog->flags & PROGRAM_PASS_1_DONE))
    {
      c=cc->depends_on;
      if(!c) c=cc->previous;
      for(;c;c=c->previous)
      {
	if(c->prog == p)
	{
	  cc->depends_on=c;
	  verify_supporters();
	  ret++; /* dependency registred */
	}
      }
    }
  }
  verify_supporters();
  return ret;
}


struct compilation
{
  struct Supporter supporter;
  struct pike_string *prog;
  struct object *handler;
  int major, minor;
  struct program *target;
  struct object *placeholder;
  
  struct program *p;
  struct lex save_lex;
  int save_depth;
  int saved_threads_disabled;
  struct object *saved_handler;
  struct object *saved_compat_handler;
  dynamic_buffer used_modules_save;
  INT32 num_used_modules_save;
  struct mapping *resolve_cache_save;

  struct svalue default_module;
};

static void free_compilation(struct compilation *c)
{
  debug_malloc_touch(c);
  free_string(c->prog);
  if(c->handler) free_object(c->handler);
  if(c->target) free_program(c->target);
  if(c->placeholder) free_object(c->placeholder);
  free_svalue(& c->default_module);
  free_supporter(&c->supporter);
  free((char *)c);
  verify_supporters();
}

static void run_init(struct compilation *c)
{
  debug_malloc_touch(c);
  c->save_depth=compilation_depth;
  compilation_depth=-1;

  c->saved_handler = error_handler;
  if((error_handler = c->handler))
    add_ref(error_handler);

  c->saved_compat_handler = compat_handler;
  compat_handler=0;

  c->used_modules_save = used_modules;
  c->num_used_modules_save = Pike_compiler->num_used_modules;
  Pike_compiler->num_used_modules=0;

  c->resolve_cache_save = resolve_cache;
  resolve_cache = 0;

  c->save_lex=lex;

  lex.current_line=1;
  lex.current_file=make_shared_string("-");

  if (runtime_options & RUNTIME_STRICT_TYPES)
  {
    lex.pragmas = ID_STRICT_TYPES;
  } else {
    lex.pragmas = 0;
  }

  lex.end = c->prog->str + (c->prog->len << c->prog->size_shift);

  switch(c->prog->size_shift)
  {
    case 0: lex.current_lexer = yylex0; break;
    case 1: lex.current_lexer = yylex1; break;
    case 2: lex.current_lexer = yylex2; break;
    default:
      fatal("Program has bad shift %d!\n", c->prog->size_shift);
      break;
  }

  lex.pos=c->prog->str;
}

static void run_init2(struct compilation *c)
{
  debug_malloc_touch(c);
  Pike_compiler->compiler = c;

  initialize_buf(&used_modules);
  use_module(& c->default_module);

  Pike_compiler->compat_major=PIKE_MAJOR_VERSION;
  Pike_compiler->compat_minor=PIKE_MINOR_VERSION;

  if(c->major>=0)
    change_compiler_compatibility(c->major, c->minor);
}

static void run_exit(struct compilation *c)
{
  debug_malloc_touch(c);
  toss_buffer(&used_modules);
  used_modules = c->used_modules_save;

#ifdef PIKE_DEBUG
  if(Pike_compiler->num_used_modules)
    fatal("Failed to pop modules properly.\n");
#endif
  Pike_compiler->num_used_modules = c->num_used_modules_save ;

#ifdef PIKE_DEBUG
  if (compilation_depth != -1) {
    fprintf(stderr, "compile(): compilation_depth is %d\n",
	    compilation_depth);
  }
#endif /* PIKE_DEBUG */
  compilation_depth=c->save_depth;

#ifdef PIKE_DEBUG
  if (resolve_cache)
    free_mapping(resolve_cache);
#endif
  resolve_cache = c->resolve_cache_save;

  if (error_handler) free_object(error_handler);
  error_handler = c->saved_handler;

  if (compat_handler)  free_object(compat_handler);
  compat_handler = c->saved_compat_handler;

  free_string(lex.current_file);
  lex=c->save_lex;
  verify_supporters();
}

static void zap_placeholder(struct compilation *c)
{
  /* fprintf(stderr, "Destructing placeholder.\n"); */
  if (c->placeholder->storage) {
    yyerror("Placeholder already has storage!");
#if 0
    fprintf(stderr, "Placeholder already has storage!\n"
	    "placeholder: %p, storage: %p, prog: %p\n",
	    c->placeholder, c->placeholder->storage, c->placeholder->prog);
#endif
    debug_malloc_touch(c->placeholder);
    destruct(c->placeholder);
  } else {
    /* FIXME: Is this correct? */
    /* It would probably be nicer if it was possible to just call
     * destruct on the object, but this works too. -Hubbe
     */
    free_program(c->placeholder->prog);
    c->placeholder->prog = NULL;
    debug_malloc_touch(c->placeholder);
  }
  free_object(c->placeholder);
  c->placeholder=0;
  verify_supporters();
}

static int run_pass1(struct compilation *c)
{
  int ret=0;

  debug_malloc_touch(c);
  run_init(c);

#if 0
  CDFPRINTF((stderr, "th(%ld) compile() starting compilation_depth=%d\n",
	     (long)th_self(),compilation_depth));
#endif

  if(c->placeholder && c->placeholder->prog != null_program)
    Pike_error("Placeholder object is not a null_program clone!\n");
  debug_malloc_touch(c->placeholder);

  if(c->target && !(c->target->flags & PROGRAM_VIRGIN))
    Pike_error("Placeholder program is not virgin!\n");

  low_start_new_program(c->target,0,0,0);
  c->supporter.prog = Pike_compiler->new_program;

  CDFPRINTF((stderr,
	     "th(%ld) %p run_pass1() start: "
	     "threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), Pike_compiler->new_program,
	     threads_disabled, compilation_depth));

  Pike_compiler->compiler_pass=1;
  run_init2(c);

  if(c->placeholder)
  {
    if(c->placeholder->prog != null_program)
    {
      yyerror("Placeholder argument is not a null_program clone!");
      c->placeholder=0;
      debug_malloc_touch(c->placeholder);
    }else{
      free_program(c->placeholder->prog);
      add_ref(c->placeholder->prog=Pike_compiler->new_program);
      debug_malloc_touch(c->placeholder);
    }
  }

#if 0
  CDFPRINTF((stderr, "th(%ld)   compile(): First pass\n",
	     (long)th_self()));
#endif

  do_yyparse();  /* Parse da program */

  if (!Pike_compiler->new_program->num_linenumbers) {
    /* The lexer didn't write an initial entry. */
    store_linenumber(0, lex.current_file);
#ifdef DEBUG_MALLOC
    if(strcmp(lex.current_file->str,"-"))
      debug_malloc_name(Pike_compiler->new_program, lex.current_file->str, 0);
#endif
  }

  CDFPRINTF((stderr, "th(%ld) %p run_pass1() done for %s\n",
	     (long)th_self(), Pike_compiler->new_program, lex.current_file->str));

  ret=unlink_current_supporter(& c->supporter);

  c->p=end_first_pass(0);

  run_exit(c);

  if(c->placeholder)
  {
    if(!c->p || (c->placeholder->storage))
    {
      debug_malloc_touch(c->placeholder);
      zap_placeholder(c);
    } else {
#ifdef PIKE_DEBUG
      if (c->placeholder->prog != c->p)
	fatal("Placeholder object got wrong program after first pass.\n");
#endif
      debug_malloc_touch(c->placeholder);
      c->placeholder->storage=c->p->storage_needed ?
	(char *)xalloc(c->p->storage_needed) :
	(char *)0;
      call_c_initializers(c->placeholder);
    }
  }

  verify_supporters();
  return ret;
}

void run_pass2(struct compilation *c)
{
  debug_malloc_touch(c);
  debug_malloc_touch(c->placeholder);

  run_init(c);
  low_start_new_program(c->p,0,0,0);
  free_program(c->p);
  c->p=0;

  Pike_compiler->compiler_pass=2;

  run_init2(c);

  CDFPRINTF((stderr,
	     "th(%ld) %p run_pass2() start: "
	     "threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), Pike_compiler->new_program,
	     threads_disabled, compilation_depth));

  verify_supporters();

  do_yyparse();  /* Parse da program */

  CDFPRINTF((stderr, "th(%ld) %p run_pass2() done for %s\n",
	     (long)th_self(), Pike_compiler->new_program, lex.current_file->str));

  verify_supporters();

  c->p=end_program();

  run_exit(c);
}

static void run_cleanup(struct compilation *c, int delayed)
{
  debug_malloc_touch(c);
  debug_malloc_touch(c->placeholder);
#if 0 /* FIXME */
  if (threads_disabled != c->saved_threads_disabled) {
    fatal("compile(): threads_disabled:%d saved_threads_disabled:%d\n",
	  threads_disabled, c->saved_threads_disabled);
  }
#endif /* PIKE_DEBUG */

  exit_threads_disable(NULL);

  CDFPRINTF((stderr,
	     "th(%ld) %p run_cleanup(): threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), c->target, threads_disabled, compilation_depth));
  if (!c->p)
  {
    /* fprintf(stderr, "Destructing placeholder.\n"); */
    if(c->placeholder) {
      debug_malloc_touch(c->placeholder);
      zap_placeholder(c);
    }

    if(delayed && c->target)
    {
      struct program *p = c->target;
      /* Free the constants in the failed program, to untangle the
       * cyclic references we might have to this program, typically
       * in parent pointers in nested classes. */
      if (p->constants) {
	int i;
	for (i = 0; i < p->num_constants; i++) {
	  free_svalue(&p->constants[i].sval);
	  p->constants[i].sval.type = T_INT;
	}
      }

      /* We have to notify the master object that
       * a previous compile() actually failed, even
       * if we did not know it at the time
       */
      CDFPRINTF((stderr, "th(%ld) %p unregistering failed delayed compile.\n",
		 (long) th_self(), p));
      ref_push_program(p);
      SAFE_APPLY_MASTER("unregister",1);
      pop_stack();

#ifdef PIKE_DEBUG
      if (p->refs > 1) {
	/* Other programs can have indexed out constants from p, which
	 * might be broken themselves and/or keep references to p
	 * through the parent pointer. We should find all those other
	 * programs and invalidate them too, but how can that be done?
	 * The whole delayed compilation thingie is icky icky icky... :P
	 * /mast */
	fprintf(stderr, "Warning: Program still got %d "
		"external refs after unregister:\n", p->refs - 1);
	locate_references(p);
      }
#endif
    }
  }
  else
  {
    if (c->placeholder)
    {
      if (c->target->flags & PROGRAM_FINISHED) {
	JMP_BUF rec;
	/* Initialize the placeholder. */
#ifdef PIKE_DEBUG
	if (c->placeholder->prog != c->p)
	  fatal("Placeholder object got wrong program after second pass.\n");
#endif
	if(SETJMP(rec))
	{
	  struct svalue thrown = throw_value;
	  debug_malloc_touch(c->placeholder);
	  throw_value.type = T_INT;
	  push_svalue(&thrown);
	  low_safe_apply_handler("compile_exception", error_handler, compat_handler, 1);
	  if (SAFE_IS_ZERO(sp-1)) yy_describe_exception(&thrown);
	  pop_stack();
	  free_svalue(&thrown);
	  zap_placeholder(c);
	}else{
	  debug_malloc_touch(c->placeholder);
	  call_pike_initializers(c->placeholder,0);
	}
	UNSETJMP(rec);
      }
      else {
	debug_malloc_touch(c->placeholder);
	zap_placeholder(c);
      }
    }
  }
  verify_supporters();
}

static int call_delayed_pass2(struct compilation *cc, int finish)
{
  int ok = 0;
  debug_malloc_touch(cc);

  CDFPRINTF((stderr, "th(%ld) %p %s delayed compile.\n",
	     (long) th_self(), cc->p, finish ? "continuing" : "cleaning up"));

  if(finish && cc->p) run_pass2(cc);
  run_cleanup(cc,1);
  
  debug_malloc_touch(cc);

#ifdef PIKE_DEBUG
  if(cc->supporter.dependants)
    fatal("Que???\n");
#endif
  if(cc->p) {
    ok = finish;
    free_program(cc->p); /* later */
  }

  CDFPRINTF((stderr, "th(%ld) %p delayed compile %s.\n",
	     (long) th_self(), cc->target, ok ? "done" : "failed"));

  free_compilation(cc);
  verify_supporters();

  return ok;
}

struct program *compile(struct pike_string *aprog,
			struct object *ahandler,/* error handler */
			int amajor, int aminor,
			struct program *atarget,
			struct object *aplaceholder)
{
  int delay, dependants_ok = 1;
  struct program *ret;
#ifdef PIKE_DEBUG
  ONERROR tmp;
#endif
  struct compilation *c=ALLOC_STRUCT(compilation);

  verify_supporters();

  CDFPRINTF((stderr, "th(%ld) %p compile() enter, placeholder=%p\n",
	     (long) th_self(), atarget, aplaceholder));

  debug_malloc_touch(c);
  add_ref(c->prog=aprog);
  if((c->handler=ahandler)) add_ref(ahandler);
  c->major=amajor;
  c->minor=aminor;
  if((c->target=atarget)) add_ref(atarget);
  if((c->placeholder=aplaceholder)) add_ref(aplaceholder);
  c->default_module.type=T_INT;

  if(c->handler)
  {
    safe_apply(c->handler,"get_default_module",0);
    if(SAFE_IS_ZERO(Pike_sp-1))
    {
      pop_stack();
      ref_push_mapping(get_builtin_constants());
    }
  }else{
    ref_push_mapping(get_builtin_constants());
  }
  free_svalue(& c->default_module);
  c->default_module=Pike_sp[-1];
  Pike_sp--;

#ifdef PIKE_DEBUG
  SET_ONERROR(tmp, fatal_on_error,"Compiler exited with longjump!\n");
#endif

  low_init_threads_disable();
  c->saved_threads_disabled = threads_disabled;

  init_supporter(& c->supporter,
		 (supporter_callback *) call_delayed_pass2,
		 (void *)c);

  delay=run_pass1(c) && c->p;
  dependants_ok = call_dependants(& c->supporter, !!c->p );
#ifdef PIKE_DEBUG
  /* FIXME */
  UNSET_ONERROR(tmp);
#endif

  if(delay)
  {
    CDFPRINTF((stderr, "th(%ld) %p compile() finish later, placeholder=%p.\n",
	       (long) th_self(), c->target, c->placeholder));
    /* finish later */
    add_ref(c->p);
    verify_supporters();
    return c->p; /* freed later */
  }else{
    /* finish now */
    if(c->p) run_pass2(c);
    debug_malloc_touch(c);
    run_cleanup(c,0);
    
    ret=c->p;

    debug_malloc_touch(c);
    free_compilation(c);

    if (!dependants_ok) {
      CDFPRINTF((stderr, "th(%ld) %p compile() reporting failure "
		 "since a dependant failed.\n",
		 (long) th_self(), c->target));
      if (ret) free_program(ret);
      throw_error_object(low_clone(compilation_error_program), 0, 0, 0,
			 "Compilation failed.\n");
    }
    if(!ret) {
      CDFPRINTF((stderr, "th(%ld) %p compile() failed.\n",
		 (long) th_self(), c->target));
      throw_error_object(low_clone(compilation_error_program), 0, 0, 0,
			 "Compilation failed.\n");
    }
    debug_malloc_touch(ret);
    verify_supporters();
    return ret;
  }
}

PMOD_EXPORT int pike_add_function2(char *name, void (*cfun)(INT32),
				   char *type, unsigned INT8 flags,
				   unsigned INT16 opt_flags)
{
  int ret;
  struct pike_string *name_tmp;
  struct pike_type *type_tmp;
  union idptr tmp;

  name_tmp=make_shared_string(name);
  type_tmp=parse_type(type);

  if(cfun)
  {
    tmp.c_fun=cfun;
    ret=define_function(name_tmp,
			type_tmp,
			flags,
			IDENTIFIER_C_FUNCTION,
			&tmp,
			opt_flags);
  }else{
    ret=define_function(name_tmp,
			type_tmp,
			flags,
			IDENTIFIER_C_FUNCTION,
			0,
			opt_flags);
  }
  free_string(name_tmp);
  free_type(type_tmp);
  return ret;
}

PMOD_EXPORT int quick_add_function(char *name,
				   int name_length,
				   void (*cfun)(INT32),
				   char *type,
				   int type_length,
				   unsigned INT8 flags,
				   unsigned INT16 opt_flags)
{
  int ret;
  struct pike_string *name_tmp;
  struct pike_type *type_tmp;
  union idptr tmp;
/*  fprintf(stderr,"ADD_FUNC: %s\n",name); */
  name_tmp = make_shared_binary_string(name, name_length);
#ifdef USE_PIKE_TYPE
  type_tmp = make_pike_type(type);
#else /* !USE_PIKE_TYPE */
  type_tmp = make_shared_binary_string(type, type_length);
#endif /* USE_PIKE_TYPE */

  if(cfun)
  {
    tmp.c_fun=cfun;
    ret=define_function(name_tmp,
			type_tmp,
			flags,
			IDENTIFIER_C_FUNCTION,
			&tmp,
			opt_flags);
  }else{
    ret=define_function(name_tmp,
			type_tmp,
			flags,
			IDENTIFIER_C_FUNCTION,
			0,
			opt_flags);
  }
  free_string(name_tmp);
  free_type(type_tmp);
  return ret;
}

#ifdef PIKE_DEBUG
void check_all_programs(void)
{
  struct program *p;
  for(p=first_program;p;p=p->next)
    check_program(p);

#ifdef FIND_FUNCTION_HASHSIZE
  {
    unsigned long e;
    for(e=0;e<FIND_FUNCTION_HASHSIZE;e++)
    {
      if(cache[e].name)
      {
	check_string(cache[e].name);
	if(cache[e].id<0 || cache[e].id > current_program_id)
	  fatal("Error in find_function_cache[%ld].id\n",(long)e);

	if(cache[e].fun < -1 || cache[e].fun > 65536)
	  fatal("Error in find_function_cache[%ld].fun\n",(long)e);
      }
    }
  }
#endif

}
#endif

#undef THIS
#define THIS ((struct pike_trampoline *)(CURRENT_STORAGE))
struct program *pike_trampoline_program=0;

static void apply_trampoline(INT32 args)
{
  Pike_error("Internal error: Trampoline magic failed!\n");
}

static void not_trampoline(INT32 args)
{
  pop_n_elems(args);
  if (!THIS->frame || !THIS->frame->current_object ||
      !THIS->frame->current_object->prog) {
    push_int(1);
  } else {
    push_int(0);
  }
}

static void init_trampoline(struct object *o)
{
  THIS->frame=0;
}

static void exit_trampoline(struct object *o)
{
  if(THIS->frame)
  {
    free_pike_scope(THIS->frame);
    THIS->frame=0;
  }
}

static void gc_check_frame(struct pike_frame *f)
{
  if(f->flags & PIKE_FRAME_MALLOCED_LOCALS)
  {
    if(f->current_object)
      debug_gc_check2(f->current_object, T_PIKE_FRAME, f,
		      " as current_object in trampoline frame");
    if(f->context.prog)
      debug_gc_check2(f->context.prog, T_PIKE_FRAME, f,
		      " as context.prog in trampoline frame");
    if(f->context.parent)
      debug_gc_check2(f->context.parent, T_PIKE_FRAME, f,
		      " as context.parent in trampoline frame");
    if(f->flags & PIKE_FRAME_MALLOCED_LOCALS)
      debug_gc_check_svalues2(f->locals, f->num_locals, T_PIKE_FRAME, f,
			      " in locals of trampoline frame");
    if(f->scope && !debug_gc_check2(f->scope, T_PIKE_FRAME, f,
				    " as scope frame of trampoline frame"))
      gc_check_frame(f->scope);
  }
}

static void gc_check_trampoline(struct object *o)
{
  if (THIS->frame &&
      !debug_gc_check2(THIS->frame, T_OBJECT, o, " as trampoline frame"))
    gc_check_frame(THIS->frame);
}

static void gc_recurse_frame(struct pike_frame *f)
{
  if(f->current_object) gc_recurse_object(f->current_object);
  if(f->context.prog)   gc_recurse_program(f->context.prog);
  if(f->context.parent) gc_recurse_object(f->context.parent);
  if(f->flags & PIKE_FRAME_MALLOCED_LOCALS)
    gc_recurse_svalues(f->locals,f->num_locals);
  if(f->scope)          gc_recurse_frame(f->scope);
}

static void gc_recurse_trampoline(struct object *o)
{
  if (THIS->frame) gc_recurse_frame(THIS->frame);
}


/* This placeholder should is used
 * in the first compiler pass to take the place
 * of unknown things
 */
struct program *placeholder_program;
struct object *placeholder_object;

void placeholder_index(INT32 args)
{
  pop_n_elems(args);
  ref_push_object(Pike_fp->current_object);
}

void init_program(void)
{
  int i;
  struct svalue key;
  struct svalue val;
  struct svalue id;
  init_program_blocks();

  MAKE_CONSTANT_SHARED_STRING(this_program_string,"this_program");
  MAKE_CONSTANT_SHARED_STRING(UNDEFINED_string,"UNDEFINED");

  lfun_ids = allocate_mapping(NUM_LFUNS);
  lfun_types = allocate_mapping(NUM_LFUNS);
  for (i=0; i < NUM_LFUNS; i++) {
    lfun_strings[i] = make_shared_string(lfun_names[i]);

    id.type = T_INT;
    id.u.integer = i;
    key.type = T_STRING;
    key.u.string = lfun_strings[i];
    mapping_insert(lfun_ids, &key, &id);

    val.type = T_TYPE;
    val.u.type = make_pike_type(raw_lfun_types[i]);
    mapping_insert(lfun_types, &key, &val);
    free_type(val.u.type);
  }
  start_new_program();
  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);
  ADD_STORAGE(struct pike_trampoline);
  add_function("`()",apply_trampoline,"function(mixed...:mixed)",0);
  add_function("`!",not_trampoline,"function(:int)",0);
  set_init_callback(init_trampoline);
  set_exit_callback(exit_trampoline);
  set_gc_check_callback(gc_check_trampoline);
  set_gc_recurse_callback(gc_recurse_trampoline);
  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);
  pike_trampoline_program=end_program();

  {
    struct svalue s;
    start_new_program();
    null_program=end_program();
    s.type=T_PROGRAM;
    s.u.program=null_program;
    low_add_constant("__null_program",&s);
    debug_malloc_touch(null_program);
  }

  {
    struct svalue s;
    start_new_program();
    add_function("`()",placeholder_index,"function(mixed...:object)",0);
    add_function("`[]",placeholder_index,"function(mixed:object)",0);
    placeholder_program=end_program();
    placeholder_object=fast_clone_object(placeholder_program,0);

    s.type=T_OBJECT;
    s.u.object=placeholder_object;
    low_add_constant("__placeholder_object",&s);
    debug_malloc_touch(placeholder_object);
  }
}

void cleanup_program(void)
{
  int e;

  free_string(UNDEFINED_string);
  free_string(this_program_string);

  free_mapping(lfun_types);
  free_mapping(lfun_ids);
  for (e=0; e < NUM_LFUNS; e++) {
    free_string(lfun_strings[e]);
  }
#ifdef FIND_FUNCTION_HASHSIZE
  for(e=0;e<FIND_FUNCTION_HASHSIZE;e++)
  {
    if(cache[e].name)
    {
      free_string(cache[e].name);
      cache[e].name=0;
    }
  }
#endif

#ifdef DO_PIKE_CLEANUP
  if(resolve_cache)
  {
    free_mapping(dmalloc_touch (struct mapping *, resolve_cache));
    resolve_cache=0;
  }

  if(pike_trampoline_program)
  {
    free_program(pike_trampoline_program);
    pike_trampoline_program=0;
  }

  if(null_program)
  {
    free_program(null_program);
    null_program=0;
  }

  if(placeholder_object)
  {
    free_object(placeholder_object);
    placeholder_object=0;
  }

  if(placeholder_program)
  {
    free_program(placeholder_program);
    placeholder_program=0;
  }
#endif
}

#ifdef GC2

void gc_mark_program_as_referenced(struct program *p)
{
  debug_malloc_touch(p);

  if (p->flags & PROGRAM_AVOID_CHECK) {
    /* Program is in an inconsistent state.
     * don't look closer at it.
     */
    debug_malloc_touch(p);

    if (gc_mark(p)) {
      if (p == gc_mark_program_pos)
	gc_mark_program_pos = p->next;
      if (p == gc_internal_program)
	gc_internal_program = p->next;
      else {
	DOUBLEUNLINK(first_program, p);
	DOUBLELINK(first_program, p); /* Linked in first. */
      }
    }

    return;
  }
  
  if(gc_mark(p))
  {
    int e;

    if (p == gc_mark_program_pos)
      gc_mark_program_pos = p->next;
    if (p == gc_internal_program)
      gc_internal_program = p->next;
    else {
      DOUBLEUNLINK(first_program, p);
      DOUBLELINK(first_program, p); /* Linked in first. */
    }

    if(p->parent)
      gc_mark_program_as_referenced(p->parent);

    for(e=0;e<p->num_constants;e++)
      gc_mark_svalues(& p->constants[e].sval, 1);

    for(e=0;e<p->num_inherits;e++)
    {
      if(p->inherits[e].parent)
	gc_mark_object_as_referenced(p->inherits[e].parent);

      if(e && p->inherits[e].prog)
	gc_mark_program_as_referenced(p->inherits[e].prog);
    }

  }
}

void real_gc_cycle_check_program(struct program *p, int weak)
{
  GC_CYCLE_ENTER(p, weak) {
    int e;

    if (!(p->flags & PROGRAM_AVOID_CHECK))
    {
      for(e=0;e<p->num_constants;e++)
	gc_cycle_check_svalues(& p->constants[e].sval, 1);
      
      for(e=0;e<p->num_inherits;e++)
      {
	if(p->inherits[e].parent)
	  gc_cycle_check_object(p->inherits[e].parent, 0);
	
	if(e && p->inherits[e].prog)
	  gc_cycle_check_program(p->inherits[e].prog, 0);
      }
      
      /* Strong ref follows. It must be last. */
      if(p->parent)
	gc_cycle_check_program(p->parent, 0);
    }
  } GC_CYCLE_LEAVE;
}

static void gc_check_program(struct program *p)
{
  int e;
#ifdef PIKE_DEBUG
  extern void * check_for;
#endif
  
  debug_malloc_touch(p);

  if (p->flags & PROGRAM_AVOID_CHECK) {
    /* Program is in an inconsistent state.
     * don't look closer at it.
     */
    debug_malloc_touch(p);
    return;
  }

  if(p->parent)
    debug_gc_check2(p->parent, T_PROGRAM, p, " as parent program of a program");
  
  for(e=0;e<p->num_constants;e++) {
    debug_gc_check_svalues(& p->constants[e].sval, 1, T_PROGRAM, p);
  }
  
  for(e=0;e<p->num_inherits;e++)
  {
    if(p->inherits[e].parent)
    {
      debug_gc_check2(p->inherits[e].parent, T_PROGRAM, p,
		      " as inherited parent object of a program");
    }

#ifdef PIKE_DEBUG
    if(d_flag && p->inherits[e].name && check_for != (void *)(ptrdiff_t)1)
      debug_gc_check(p->inherits[e].name, T_PROGRAM, p);
#endif

    if(e && p->inherits[e].prog)
      debug_gc_check2(p->inherits[e].prog, T_PROGRAM, p,
		      " as inherited program of a program");
  }

#ifdef PIKE_DEBUG
  if(d_flag && check_for != (void *)(ptrdiff_t)1)
  {
    int e;
    for(e=0;e<(int)p->num_strings;e++)
      debug_gc_check(p->strings[e], T_PROGRAM, p);

    for(e=0;e<(int)p->num_identifiers;e++)
    {
      debug_gc_check(p->identifiers[e].name, T_PROGRAM, p);
      debug_gc_check(p->identifiers[e].type, T_PROGRAM, p);
    }
  }
#endif
}

#ifdef PIKE_DEBUG
unsigned gc_touch_all_programs(void)
{
  unsigned n = 0;
  struct program *p;
  struct program_state *ps;
  if (first_program && first_program->prev)
    fatal("Error in program link list.\n");
  for (p = first_program; p; p = p->next) {
    debug_gc_touch(p);
    n++;
    if (p->next && p->next->prev != p)
      fatal("Error in program link list.\n");
  }
  return n;
}
#endif

void gc_check_all_programs(void)
{
  struct program *p;
  for(p=first_program;p;p=p->next) gc_check_program(p);
}

void gc_mark_all_programs(void)
{
  gc_mark_program_pos = gc_internal_program;
  while (gc_mark_program_pos) {
    struct program *p = gc_mark_program_pos;
    gc_mark_program_pos = p->next;
    if(gc_is_referenced(p)) {
      gc_mark_program_as_referenced(debug_malloc_pass(p));
    }
  }
}

void gc_cycle_check_all_programs(void)
{
  struct program *p;
  for (p = gc_internal_program; p; p = p->next) {
    real_gc_cycle_check_program(p, 0);
    gc_cycle_run_queue();
  }
}

void gc_zap_ext_weak_refs_in_programs(void)
{
  gc_mark_program_pos = first_program;
  while (gc_mark_program_pos != gc_internal_program && gc_ext_weak_refs) {
    struct program *p = gc_mark_program_pos;
    gc_mark_program_pos = p->next;
    gc_mark_program_as_referenced(p);
  }
  discard_queue(&gc_mark_queue);
}

void gc_free_all_unreferenced_programs(void)
{
  struct program *p,*next;
#ifdef PIKE_DEBUG
  int first = 1;
#endif

  for(p=gc_internal_program;p;p=next)
  {
    debug_malloc_touch(p);

    if(gc_do_free(p))
    {
      /* Got an extra ref from gc_cycle_pop_object(). */
      int e;
      if(p->parent)
      {
	free_program(p->parent);
	p->parent=0;
      }
      for(e=0;e<p->num_constants;e++)
      {
	free_svalue(& p->constants[e].sval);
	p->constants[e].sval.type=T_INT;
	DO_IF_DMALLOC(p->constants[e].sval.u.refs=(void *)-1);
      }

      for(e=0;e<p->num_inherits;e++)
      {
	if(p->inherits[e].parent)
	{
	  free_object(p->inherits[e].parent);
	  p->inherits[e].parent=0;
	}
	if(e && p->inherits[e].prog)
	{
	  free_program(p->inherits[e].prog);
	  p->inherits[e].prog=0;
	}
      }

      gc_free_extra_ref(p);
      SET_NEXT_AND_FREE(p, free_program);
#ifdef PIKE_DEBUG
      if (first) gc_internal_program = next;
#endif
    }else{
      next=p->next;
#ifdef PIKE_DEBUG
      first = 0;
#endif
    }
  }

#ifdef PIKE_DEBUG
  if (gc_debug)
    for (p = first_program; p != gc_internal_program; p = p->next) {
      int e,tmp=0;
      if (!p)
	fatal("gc_internal_program was bogus.\n");
      for(e=0;e<p->num_constants;e++)
      {
	if(p->constants[e].sval.type == T_PROGRAM && p->constants[e].sval.u.program == p)
	  tmp++;
      }
      if(tmp >= p->refs)
	gc_fatal(p, 1 ,"garbage collector failed to free program!!!\n");
    }
#endif
}

#endif /* GC2 */


void push_compiler_frame(int lexical_scope)
{
  struct compiler_frame *f;
  f=ALLOC_STRUCT(compiler_frame);
  f->previous=Pike_compiler->compiler_frame;
  f->lexical_scope=lexical_scope;
  f->current_type=0;
  f->current_return_type=0;

  f->current_number_of_locals=0;
  f->max_number_of_locals=0;
  f->min_number_of_locals=0;
  f->last_block_level=-1;

  f->current_function_number=-2; /* no function */
  f->recur_label=-1;
  f->is_inline=0;
  f->num_args=-1;
  f->opt_flags = OPT_SIDE_EFFECT|OPT_EXTERNAL_DEPEND; /* FIXME: Should be 0. */
  Pike_compiler->compiler_frame=f;
}

void low_pop_local_variables(int level)
{
  while(Pike_compiler->compiler_frame->current_number_of_locals > level)
  {
    int e;
    e=--(Pike_compiler->compiler_frame->current_number_of_locals);
    free_string(Pike_compiler->compiler_frame->variable[e].name);
    free_type(Pike_compiler->compiler_frame->variable[e].type);
    if(Pike_compiler->compiler_frame->variable[e].def)
      free_node(Pike_compiler->compiler_frame->variable[e].def);

    free_string(Pike_compiler->compiler_frame->variable[e].file);
  }
}

void pop_local_variables(int level)
{
#if 1
  /* We need to save the variables Kuppo (but not their names) */
  if(level < Pike_compiler->compiler_frame->min_number_of_locals)
  {
    for(;level<Pike_compiler->compiler_frame->min_number_of_locals;level++)
    {
      free_string(Pike_compiler->compiler_frame->variable[level].name);
      MAKE_CONSTANT_SHARED_STRING(Pike_compiler->compiler_frame->variable[level].name,"");
    }
  }
#endif
  low_pop_local_variables(level);
}


void pop_compiler_frame(void)
{
  struct compiler_frame *f;

  f=Pike_compiler->compiler_frame;
#ifdef PIKE_DEBUG
  if(!f)
    fatal("Popping out of compiler frames\n");
#endif

  low_pop_local_variables(0);
  if(f->current_type)
    free_type(f->current_type);

  if(f->current_return_type)
    free_type(f->current_return_type);

  Pike_compiler->compiler_frame=f->previous;
  dmfree((char *)f);
}


#define GET_STORAGE_CACHE_SIZE 1024
static struct get_storage_cache
{
  INT32 oid, pid;
  ptrdiff_t offset;
} get_storage_cache[GET_STORAGE_CACHE_SIZE];

ptrdiff_t low_get_storage(struct program *o, struct program *p)
{
  INT32 oid, pid;
  ptrdiff_t offset;
  unsigned INT32 hval;

  if(!o) return -1;
  oid=o->id;
  pid=p->id;
  hval=oid*9248339 + pid;
  hval%=GET_STORAGE_CACHE_SIZE;
#ifdef PIKE_DEBUG
  if(hval>GET_STORAGE_CACHE_SIZE)
    fatal("hval>GET_STORAGE_CACHE_SIZE");
#endif
  if(get_storage_cache[hval].oid == oid &&
     get_storage_cache[hval].pid == pid)
  {
    offset=get_storage_cache[hval].offset;
  }else{
    INT32 e;
    offset=-1;
    for(e=0;e<o->num_inherits;e++)
    {
      if(o->inherits[e].prog==p)
      {
	offset=o->inherits[e].storage_offset;
	break;
      }
    }

    get_storage_cache[hval].oid=oid;
    get_storage_cache[hval].pid=pid;
    get_storage_cache[hval].offset=offset;
  }

  return offset;
}

PMOD_EXPORT char *get_storage(struct object *o, struct program *p)
{
  ptrdiff_t offset;

#ifdef _REENTRANT
  if(d_flag) CHECK_INTERPRETER_LOCK();
#endif

  offset= low_get_storage(o->prog, p);
  if(offset == -1) return 0;
  return o->storage + offset;
}

struct program *low_program_from_function(struct program *p,
					  INT32 i)
{
  struct svalue *f;
  struct identifier *id=ID_FROM_INT(p, i);
  if(!IDENTIFIER_IS_CONSTANT(id->identifier_flags)) return 0;
  if(id->func.offset==-1) return 0;
  f=& PROG_FROM_INT(p,i)->constants[id->func.offset].sval;
  if(f->type!=T_PROGRAM) return 0;
  return f->u.program;
}

PMOD_EXPORT struct program *program_from_function(const struct svalue *f)
{
  if(f->type != T_FUNCTION) return 0;
  if(f->subtype == FUNCTION_BUILTIN) return 0;
  if(!f->u.object->prog) return 0;
  return low_program_from_function(f->u.object->prog, f->subtype);
}

PMOD_EXPORT struct program *program_from_svalue(const struct svalue *s)
{
  switch(s->type)
  {
    case T_OBJECT:
    {
      struct program *p = s->u.object->prog;
      int call_fun;

      if (!p) return 0;

      if ((call_fun = FIND_LFUN(p, LFUN_CALL)) >= 0) {
	/* Get the program from the return type. */
	struct identifier *id = ID_FROM_INT(p, call_fun);
	/* FIXME: do it. */
	/* fprintf(stderr, "Object type has `()().\n"); */
	return 0;
      }
      push_svalue(s);
      f_object_program(1);
      p=program_from_svalue(Pike_sp-1);
      pop_stack();
      return p; /* We trust that there is a reference somewhere... */
    }

  case T_FUNCTION:
    return program_from_function(s);
  case T_PROGRAM:
    return s->u.program;
  default:
    return 0;
  }
}

#define FIND_CHILD_HASHSIZE 5003
struct find_child_cache_s
{
  INT32 pid,cid,id;
};

static struct find_child_cache_s find_child_cache[FIND_CHILD_HASHSIZE];

int find_child(struct program *parent, struct program *child)
{
  unsigned INT32 h=(parent->id  * 9248339 + child->id);
  h= h % FIND_CHILD_HASHSIZE;
#ifdef PIKE_DEBUG
  if(h>=FIND_CHILD_HASHSIZE)
    fatal("find_child failed to hash within boundaries.\n");
#endif
  if(find_child_cache[h].pid == parent->id &&
     find_child_cache[h].cid == child->id)
  {
    return find_child_cache[h].id;
  }else{
    INT32 i;
    for(i=0;i<parent->num_identifier_references;i++)
    {
      if(low_program_from_function(parent, i)==child)
      {
	find_child_cache[h].pid=parent->id;
	find_child_cache[h].cid=child->id;
	find_child_cache[h].id=i;
	return i;
      }
    }
  }
  return -1;
}

void yywarning(char *fmt, ...) ATTRIBUTE((format(printf,1,2)))
{
  char buf[4711];
  va_list args;

  /* If we have parse errors we might get erroneous warnings,
   * so don't print them.
   * This has the additional benefit of making it easier to
   * visually locate the actual error message.
   */
  if (Pike_compiler->num_parse_error) return;

  va_start(args,fmt);
  VSPRINTF(buf, fmt, args);
  va_end(args);

  if(strlen(buf)>sizeof(buf))
    fatal("Buffer overflow in yywarning!\n");

  if ((error_handler && error_handler->prog) || get_master()) {
    ref_push_string(lex.current_file);
    push_int(lex.current_line);
    push_text(buf);

    low_safe_apply_handler("compile_warning", error_handler, compat_handler, 3);
    pop_stack();
  }
}



/* returns 1 if a implements b */
static int low_implements(struct program *a, struct program *b)
{
  int e;
  struct pike_string *s=findstring("__INIT");
  for(e=0;e<b->num_identifier_references;e++)
  {
    struct identifier *bid;
    int i;
    if (b->identifier_references[e].id_flags & (ID_STATIC|ID_HIDDEN))
      continue;		/* Skip static & hidden */
    bid = ID_FROM_INT(b,e);
    if(s == bid->name) continue;	/* Skip __INIT */
    i = find_shared_string_identifier(bid->name,a);
    if (i == -1) {
      if (b->identifier_references[e].id_flags & (ID_OPTIONAL))
	continue;		/* It's ok... */
#if 0
      fprintf(stderr, "Missing identifier \"%s\"\n", bid->name->str);
#endif /* 0 */
      return 0;
    }

    if (!pike_types_le(bid->type, ID_FROM_INT(a, i)->type)) {
      if(!match_types(ID_FROM_INT(a,i)->type, bid->type)) {
#if 0
	fprintf(stderr, "Identifier \"%s\" is incompatible.\n",
		bid->name->str);
#endif /* 0 */
	return 0;
      } else {
#if 0
	fprintf(stderr, "Identifier \"%s\" is not strictly compatible.\n",
		bid->name->str);
#endif /* 0 */
      }
    }
  }
  return 1;
}

#define IMPLEMENTS_CACHE_SIZE 4711
struct implements_cache_s { INT32 aid, bid, ret; };
static struct implements_cache_s implements_cache[IMPLEMENTS_CACHE_SIZE];

/* returns 1 if a implements b, but faster */
PMOD_EXPORT int implements(struct program *a, struct program *b)
{
  unsigned long hval;
  if(!a || !b) return -1;
  if(a==b) return 1;

  hval = a->id*9248339 + b->id;
  hval %= IMPLEMENTS_CACHE_SIZE;
#ifdef PIKE_DEBUG
  if(hval >= IMPLEMENTS_CACHE_SIZE)
    fatal("Implements_cache failed!\n");
#endif
  if(implements_cache[hval].aid==a->id && implements_cache[hval].bid==b->id)
  {
    return implements_cache[hval].ret;
  }
  /* Do it the tedious way */
  implements_cache[hval].aid=a->id;
  implements_cache[hval].bid=b->id;
  implements_cache[hval].ret = 1;	/* Tentatively compatible. */
  implements_cache[hval].ret = low_implements(a,b);
  /* NOTE: If low_implements() returns 0, the cache may have received
   *       some false positives. Those should be cleared.
   */
  return implements_cache[hval].ret;
}

/* Returns 1 if a is compatible with b */
static int low_is_compatible(struct program *a, struct program *b)
{
  int e;
  struct pike_string *s=findstring("__INIT");

  /* Optimize the loop somewhat */
  if (a->num_identifier_references < b->num_identifier_references) {
    struct program *tmp = a;
    a = b;
    b = tmp;
  }

  for(e=0;e<b->num_identifier_references;e++)
  {
    struct identifier *bid;
    int i;
    if (b->identifier_references[e].id_flags & (ID_STATIC|ID_HIDDEN))
      continue;		/* Skip static & hidden */

    /* FIXME: What if they aren't static & hidden in a? */

    bid = ID_FROM_INT(b,e);
    if(s == bid->name) continue;	/* Skip __INIT */
    i = find_shared_string_identifier(bid->name,a);
    if (i == -1) {
      continue;		/* It's ok... */
    }

    if(!match_types(ID_FROM_INT(a,i)->type, bid->type)) {
#if 0
      fprintf(stderr, "Identifier \"%s\" is incompatible.\n",
	      bid->name->str);
#endif /* 0 */
      return 0;
    }
  }
  return 1;
}

static struct implements_cache_s is_compatible_cache[IMPLEMENTS_CACHE_SIZE];
/* Returns 1 if a is compatible with b
 * ie it's possible to write a hypothetical c that implements both.
 */
PMOD_EXPORT int is_compatible(struct program *a, struct program *b)
{
  unsigned long hval;
  unsigned long rhval;
  int aid, bid;
  if(!a || !b) return -1;
  if(a==b) return 1;

  /* Order the id's so we don't need double entries in the cache. */
  aid = a->id;
  bid = b->id;
  if (aid > bid) {
    int tmp = aid;
    aid = bid;
    bid = tmp;
  }

  hval = aid*9248339 + bid;
  hval %= IMPLEMENTS_CACHE_SIZE;
#ifdef PIKE_DEBUG
  if(hval >= IMPLEMENTS_CACHE_SIZE)
    fatal("Implements_cache failed!\n");
#endif
  if(is_compatible_cache[hval].aid==aid &&
     is_compatible_cache[hval].bid==bid)
  {
    return is_compatible_cache[hval].ret;
  }
  if(implements_cache[hval].aid==aid &&
     implements_cache[hval].bid==bid &&
     implements_cache[hval].ret)
  {
    /* a implements b */
    return 1;
  }
  rhval = bid*9248339 + aid;
  rhval %= IMPLEMENTS_CACHE_SIZE;
#ifdef PIKE_DEBUG
  if(rhval >= IMPLEMENTS_CACHE_SIZE)
    fatal("Implements_cache failed!\n");
#endif
  if(implements_cache[rhval].aid==bid &&
     implements_cache[rhval].bid==aid &&
     implements_cache[rhval].ret)
  {
    /* b implements a */
    return 1;
  }
  /* Do it the tedious way */
  is_compatible_cache[hval].aid=aid;
  is_compatible_cache[hval].bid=bid;
  is_compatible_cache[hval].ret = 1;	/* Tentatively compatible. */
  is_compatible_cache[hval].ret = low_is_compatible(a,b);
  /* NOTE: If low_is compatible() returns 0, the cache may have received
   *       some false positives. Those should be cleared.
   */
  return is_compatible_cache[hval].ret;
}

/* returns 1 if a implements b */
int yyexplain_not_implements(struct program *a, struct program *b, int flags)
{
  int e;
  struct pike_string *s=findstring("__INIT");
  for(e=0;e<b->num_identifier_references;e++)
  {
    struct identifier *bid;
    int i;
    if (b->identifier_references[e].id_flags & (ID_STATIC|ID_HIDDEN))
      continue;		/* Skip static & hidden */
    bid = ID_FROM_INT(b,e);
    if(s == bid->name) continue;	/* Skip __INIT */
    i = find_shared_string_identifier(bid->name,a);
    if (i == -1) {
      if (b->identifier_references[e].id_flags & (ID_OPTIONAL))
	continue;		/* It's ok... */
      if(flags & YYTE_IS_WARNING)
	yywarning("Missing identifier \"%s\".", bid->name->str);
      else
	my_yyerror("Missing identifier \"%s\".", bid->name->str);
      return 0;
    }

    if (!pike_types_le(bid->type, ID_FROM_INT(a, i)->type)) {
      struct pike_string *s1,*s2;
      s1=describe_type(ID_FROM_INT(a,i)->type);
      s2=describe_type(bid->type);
      if(!match_types(ID_FROM_INT(a,i)->type, bid->type)) {
	my_yyerror("Type of identifier \"%s\" does not match.",
		   bid->name->str);
	my_yyerror("Expected: %s",s1->str);
	my_yyerror("Got     : %s",s2->str);
      } else {
	yywarning("Type of identifier \"%s\" is not strictly compatible.",
		  bid->name->str);
	yywarning("Expected: %s",s1->str);
	yywarning("Got     : %s",s2->str);
      }
      free_string(s1);
      free_string(s2);
      return 0;
    }
  }
  return 1;
}

PMOD_EXPORT void *parent_storage(int depth)
{
  struct external_variable_context loc;
  struct program *p;


  loc.o=Pike_fp->current_object;
  p=loc.o->prog;
  if(!p)
  {
    /* magic fallback */
    p=get_program_for_object_being_destructed(loc.o);
    if(!p)
    {
      Pike_error("Cannot access parent of destructed object.\n");
    }
  }

  if((Pike_fp->fun & 0xffff) == 0xffff)
    Pike_error("Cannot access parent storage!\n");

  loc.parent_identifier=Pike_fp->fun;
  loc.inherit=INHERIT_FROM_INT(p, Pike_fp->fun);
  
  find_external_context(&loc, depth);

  return loc.o->storage + loc.inherit->storage_offset;
}


PMOD_EXPORT void change_compiler_compatibility(int major, int minor)
{
  if(major == PIKE_MAJOR_VERSION && minor == PIKE_MINOR_VERSION)
  {
    push_int(0); /* optimization */
  } else {
    if(major == Pike_compiler->compat_major &&
       minor == Pike_compiler->compat_minor) {
      /* Optimization -- reuse the current compat handler. */
      if (compat_handler) {
	ref_push_object(compat_handler);
      } else {
	push_int(0);
      }
    } else {
      push_int(major);
      push_int(minor);
      SAFE_APPLY_MASTER("get_compilation_handler",2);
    }
  }

  if(compat_handler)
  {
    free_object(compat_handler);
    compat_handler=0;
  }
  
  if(sp[-1].type == T_OBJECT)
  {
    compat_handler = dmalloc_touch(struct object *, sp[-1].u.object);
    sp--;

    if (error_handler) {
      safe_apply(error_handler, "get_default_module", 0);
    } else {
      push_int(0);
    }
    if (Pike_sp[-1].type == T_INT) {
      pop_stack();
      safe_apply(compat_handler, "get_default_module", 0);
    
      if(Pike_sp[-1].type == T_INT)
      {
	pop_stack();
	ref_push_mapping(get_builtin_constants());
      }
    }
  }else{
    pop_stack();
    if (error_handler) {
      safe_apply(error_handler, "get_default_module", 0);
    } else {
      push_int(0);
    }
    if (Pike_sp[-1].type == T_INT) {
      pop_stack();
      ref_push_mapping(get_builtin_constants());
    }
  }


  if(Pike_compiler->num_used_modules)
  {
    free_svalue( (struct svalue *)used_modules.s.str );
    ((struct svalue *)used_modules.s.str)[0]=sp[-1];
    sp--;
    dmalloc_touch_svalue(sp);
    if(Pike_compiler->module_index_cache)
    {
      free_mapping(Pike_compiler->module_index_cache);
      Pike_compiler->module_index_cache=0;
    }
  }else{
    use_module(sp-1);
    pop_stack();
  }

  Pike_compiler->compat_major=major;
  Pike_compiler->compat_minor=minor;
}

#ifdef PIKE_USE_MACHINE_CODE

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

void make_program_executable(struct program *p)
{
  mprotect((void *)p->program, p->num_program*sizeof(p->program[0]),
	   PROT_EXEC | PROT_READ | PROT_WRITE);
#ifdef FLUSH_INSTRUCTION_CACHE
  FLUSH_INSTRUCTION_CACHE(p->program,
			  p->num_program*sizeof(p->program[0]));
#endif /* FLUSH_INSTRUCTION_CACHE */
}
#endif
