/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

#include "global.h"
#include "program.h"
#include "object.h"
#include "dynamic_buffer.h"
#include "pike_types.h"
#include "stralloc.h"
#include "las.h"
#include "lex.h"
#include "pike_macros.h"
#include "fsort.h"
#include "pike_error.h"
#include "docode.h"
#include "interpret.h"
#include "hashtable.h"
#include "main.h"
#include "pike_memory.h"
#include "gc.h"
#include "threads.h"
#include "constants.h"
#include "operators.h"
#include "builtin_functions.h"
#include "stuff.h"
#include "mapping.h"
#include "cyclic.h"
#include "pike_security.h"
#include "pike_types.h"
#include "opcodes.h"
#include "version.h"
#include "block_alloc.h"
#include "pikecode.h"
#include "pike_compiler.h"
#include "module_support.h"

#include <errno.h>
#include <fcntl.h>

#define sp Pike_sp

#undef ATTRIBUTE
#define ATTRIBUTE(X)

static void low_enter_compiler(struct object *ce, int inherit);
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
    size+=p->total_size - sizeof (struct program); \
  }						\
}while(0)

BLOCK_ALLOC_FILL_PAGES(program, 4)


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

struct pike_string *this_program_string;
static struct pike_string *this_string, *this_function_string;
static struct pike_string *UNDEFINED_string;

/* Common compiler subsystems */
struct pike_string *parser_system_string;
struct pike_string *type_check_system_string;

const char *const lfun_names[]  = {
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
  "`[..]",
  /* NOTE: After this point there are only fake lfuns. */
  "_search",
};

struct pike_string *lfun_strings[NELEM(lfun_names)];

static struct mapping *lfun_ids;

/* mapping(string:type) */
static struct mapping *lfun_types;

static const char *const raw_lfun_types[] = {
  tFuncV(tNone,tVoid,tVoid),	/* "__INIT", */
  tFuncV(tNone,tZero,tVoid),	/* "create", */
  tFuncV(tOr(tVoid,tInt),tVoid,tVoid), /* "destroy", */
  tFuncV(tZero,tZero,tMix),	/* "`+", */
  tFunc(tOr(tVoid,tZero),tMix),	/* "`-", */
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
  tFuncV(tZero,tZero,tMix),	/* "``+", */
  tFuncV(tZero,tVoid,tMix),	/* "``-", */
  tFuncV(tNone,tZero,tMix),	/* "``&", */
  tFuncV(tNone,tZero,tMix),	/* "``|", */
  tFuncV(tNone,tZero,tMix),	/* "``^", */
  tFuncV(tZero,tVoid,tMix),	/* "``<<", */
  tFuncV(tZero,tVoid,tMix),	/* "``>>", */
  tFuncV(tNone,tZero,tMix),	/* "``*", */
  tFuncV(tNone,tZero,tMix),	/* "``/", */
  tFuncV(tNone,tZero,tMix),	/* "``%", */
  tFuncV(tZero,tZero,tMix),	/* "`+=", */
  tFuncV(tStr,tVoid,tInt),	/* "_is_type", */
  tFuncV(tInt tOr(tMap(tStr,tInt),tVoid),tVoid,tStr),	/* "_sprintf", */
  tFuncV(tMix,tVoid,tInt),	/* "_equal", */
  tFuncV(tZero,tVoid,tMix),	/* "_m_delete", */
  tFuncV(tNone,tVoid,tObj),	/* "_get_iterator", */
  tFuncV(tZero tRangeBound tZero tRangeBound, tVoid, tMix), /* "`[..]" */
  /* NOTE: After this point there are only fake lfuns. */
  tFuncV(tZero tOr(tZero, tVoid), tVoid, tMix), /* "_search", */
};

/* These two are not true LFUNs! */
static struct pike_type *lfun_getter_type_string = NULL;
static struct pike_type *lfun_setter_type_string = NULL;

/*! @namespace lfun::
 *!
 *! Callback functions used to overload various builtin functions.
 *!
 *! The functions can be grouped into a few sets:
 *!
 *! @ul
 *!   @item
 *!     Object initialization and destruction.
 *!
 *!     @[__INIT()], @[create()], @[destroy()]
 *!
 *!   @item
 *!     Unary operator overloading.
 *!
 *!     @[`~()], @[`!()],
 *!     @[_values()], @[cast()],
 *!     @[_sizeof()], @[_indices()],
 *!     @[__hash()]
 *!
 *!   @item
 *!     Binary asymmetric operator overloading.
 *!
 *!     @[`+()], @[``+()],
 *!     @[`-()], @[``-()],
 *!     @[`&()], @[``&()],
 *!     @[`|()], @[``|()],
 *!     @[`^()], @[``^()],
 *!     @[`<<()], @[``<<()],
 *!     @[`>>()], @[``>>()],
 *!     @[`*()], @[``*()],
 *!     @[`/()], @[``/()],
 *!     @[`%()], @[``%()]
 *!
 *!   @item
 *!     Binary symmetric operator overloading.
 *!
 *!     The optimizer will make assumptions about the relations
 *!     between these functions.
 *!
 *!     @[`==()], @[_equal()], @[`<()], @[`>()]
 *!
 *!   @item
 *!     Other binary operator overloading.
 *!
 *!     @[`[]()], @[`[]=()], @[`->()],
 *!     @[`->=()], @[`+=()], @[`()()]
 *!
 *!   @item
 *!     Overloading of other builtin functions.
 *!     
 *!     @[_is_type()], @[_sprintf()], @[_m_delete()],
 *!     @[_get_iterator()], @[_search()]
 *! @endul
 *!
 *! @note
 *!   Although these functions are called from outside the object they
 *!   exist in, they will still be used even if they are declared
 *!   @expr{protected@}. It is in fact recommended to declare them
 *!   @expr{protected@}, since that will hinder them being used for
 *!   other purposes.
 *!
 *! @seealso
 *!   @[::]
 */

/*! @decl void lfun::__INIT()
 *!
 *!   Inherit and variable initialization.
 *!
 *!   This function is generated automatically by the compiler. It's
 *!   called just before @[lfun::create()] when an object is
 *!   instantiated.
 *!
 *!   It first calls any @expr{__INIT@} functions in inherited classes
 *!   (regardless of modifiers on the inherits). It then executes all
 *!   the variable initialization expressions in this class, in the
 *!   order they occur.
 *!
 *! @note
 *!   This function can not be overloaded or blocked from executing.
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
 *!   @[args] are the arguments passed when the program was called.
 *!
 *! @note
 *!   In Pike 7.2 and later this function can be created implicitly
 *!   by the compiler using the new syntax:
 *! @code
 *! class Foo(int foo) {
 *!   int bar;
 *! }
 *! @endcode
 *!   In the above case an implicit @[lfun::create()] is created, and
 *!   it's equvivalent to:
 *! @code
 *! class Foo {
 *!   int foo;
 *!   int bar;
 *!   protected void create(int foo)
 *!   {
 *!     local::foo = foo;
 *!   }
 *! }
 *! @endcode
 *!
 *! @seealso
 *!   @[lfun::__INIT()], @[lfun::destroy()]
 */

/*! @decl void lfun::destroy (void|int reason)
 *!
 *!   Object destruction callback.
 *!   
 *!   This function is called right before the object is destructed.
 *!   That can happen either through a call to @[predef::destruct()],
 *!   when there are no more references to the object, or when the
 *!   garbage collector discovers that it's part of a cyclic data
 *!   structure that has become garbage.
 *!
 *! @param reason
 *!   A flag that tells why the object is destructed:
 *!
 *!   @int
 *!     @value Object.DESTRUCT_EXPLICIT
 *!       Destructed explicitly by @[predef::destruct].
 *!     @value Object.DESTRUCT_NO_REFS
 *!       Destructed due to running out of references.
 *!     @value Object.DESTRUCT_GC
 *!       Destructed by the garbage collector.
 *!     @value Object.DESTRUCT_CLEANUP
 *!       Destructed as part of the cleanup when the pike process
 *!       exits. Occurs only if Pike has been compiled with the
 *!       configure option @tt{--with-cleanup-on-exit@}. See note
 *!       below.
 *!   @endint
 *!
 *! @note
 *! Objects are normally not destructed when a process exits, so
 *! @expr{destroy@} functions aren't called then. Use @[atexit] to get
 *! called when the process exits.
 *!
 *! @note
 *! Regarding destruction order during garbage collection:
 *! 
 *! If an object is destructed by the garbage collector, it's part of
 *! a reference cycle with other things but with no external
 *! references. If there are other objects with @expr{destroy@}
 *! functions in the same cycle, it becomes a problem which to call
 *! first.
 *!
 *! E.g. if this object has a variable with another object which
 *! (directly or indirectly) points back to this one, you might find
 *! that the other object already has been destructed and the variable
 *! thus contains zero.
 *! 
 *! The garbage collector tries to minimize such problems by defining
 *! an order as far as possible:
 *! 
 *! @ul
 *! @item
 *!   If an object A contains an @[lfun::destroy] and an object B does
 *!   not, then A is destructed before B.
 *! @item
 *!   If A references B single way, then A is destructed before B.
 *! @item
 *!   If A and B are in a cycle, and there is a reference somewhere
 *!   from B to A that is weaker than any reference from A to B, then
 *!   A is destructed before B.
 *! @item
 *!   If a cycle is resolved according to the rule above by ignoring a
 *!   weaker reference, and there is another ambiguous cycle that
 *!   would get resolved by ignoring the same reference, then the
 *!   latter cycle will be resolved by ignoring that reference.
 *! @item
 *!   Weak references (e.g. set with @[predef::set_weak_flag()]) are
 *!   considered weaker than normal references, and both are
 *!   considered weaker than strong references.
 *! @item
 *!   Strong references are those from objects to the objects of their
 *!   lexically surrounding classes. There can never be a cycle
 *!   consisting only of strong references. (This means the gc never
 *!   destructs a parent object before all children have been
 *!   destructed.)
 *! @endul
 *! 
 *! An example with well defined destruct order due to strong
 *! references:
 *!
 *! @code
 *! class Super {
 *!   class Sub {
 *!     protected void destroy() {
 *!       if (!Super::this)
 *!         error ("My parent has been destructed!\n");
 *!     }
 *!   }
 *!   Sub sub = Sub();
 *!   protected void destroy() {
 *!     if (!sub)
 *!       werror ("sub already destructed.\n");
 *!   }
 *! }
 *! @endcode
 *!
 *! The garbage collector ensures that these objects are destructed in
 *! an order so that @expr{werror@} in @expr{Super@} is called and not
 *! @expr{error@} in @expr{Sub@}.
 *!
 *! @note
 *! When the garbage collector calls @[lfun::destroy], all accessible
 *! non-objects and objects without @expr{destroy@} functions are
 *! still intact. They are not freed if the @expr{destroy@} function
 *! adds external references to them. However, all objects with
 *! @[lfun::destroy] in the cycle are already scheduled for
 *! destruction and will therefore be destroyed even if external
 *! references are added to them.
 *!
 *! @note
 *! The garbage collector had completely random destruct order in
 *! versions prior to 7.2.
 *!
 *! @seealso
 *!   @[lfun::create()], @[predef::destruct()]
 */

/*! @decl mixed lfun::`+(zero arg, zero ... rest)
 *!
 *!   Left side addition/concatenation callback.
 *!
 *!   This is used by @[predef::`+]. It's called with any arguments
 *!   that follow this object in the argument list of the call to
 *!   @[predef::`+]. The returned value should be a new instance that
 *!   represents the addition/concatenation between this object and
 *!   the arguments in the order they are given.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``+()], @[lfun::`+=()], @[predef::`+()]
 */

/*! @decl this_program lfun::`+=(zero arg, zero ... rest)
 *!
 *!   Destructive addition/concatenation callback.
 *!
 *!   This is used by @[predef::`+]. It's called with any arguments
 *!   that follow this object in the argument list of the call to
 *!   @[predef::`+]. It should update this object to represent the
 *!   addition/concatenation between it and the arguments in the order
 *!   they are given. It should always return this object.
 *!
 *! @note
 *!   This function should only be implemented if @[lfun::`+()] also
 *!   is. It should only work as a more optimized alternative to that
 *!   one, for the case when it's safe to change the object
 *!   destructively and use it directly as the result.
 *!
 *! @note
 *!   This function is not an lfun for the @expr{+=@} operator. It's
 *!   only whether or not it's safe to do a destructive change that
 *!   decides if this function or @[lfun::`+()] is called; both the
 *!   @expr{+@} operator and the @expr{+=@} operator can call either
 *!   one.
 *!
 *! @seealso
 *!   @[lfun::`+()], @[predef::`+()]
 */

/*! @decl mixed lfun::``+(zero arg, zero ... rest)
 *!
 *!   Right side addition/concatenation callback.
 *!
 *!   This is used by @[predef::`+]. It's called with any arguments
 *!   that precede this object in the argument list of the call to
 *!   @[predef::`+]. The returned value should be a new instance that
 *!   represents the addition/concatenation between the arguments in
 *!   the order they are given and this object.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`+()], @[predef::`+()]
 */

/*! @decl mixed lfun::`-(void|zero arg)
 *!
 *!   Negation and left side subtraction/set difference callback.
 *!
 *!   This is used by @[predef::`-]. When called without an argument
 *!   the result should be a new instance that represents the negation
 *!   of this object, otherwise the result should be a new instance
 *!   that represents the difference between this object and @[arg].
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``-()], @[predef::`-()]
 */

/*! @decl mixed lfun::``-(zero arg)
 *!
 *!   Right side subtraction/set difference callback.
 *!
 *!   This is used by @[predef::`-]. The result should be a new
 *!   instance that represents the difference between @[arg] and this
 *!   object.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`-()], @[predef::`-()]
 */

/*! @decl mixed lfun::`&(zero ... args)
 *!
 *!   Left side bitwise and/intersection callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``&()], @[predef::`&()]
 */

/*! @decl mixed lfun::``&(zero ... args)
 *!
 *!   Right side bitwise and/intersection callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`&()], @[predef::`&()]
 */

/*! @decl mixed lfun::`|(zero ... args)
 *!
 *!   Left side bitwise or/union callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``|()], @[predef::`|()]
 */

/*! @decl mixed lfun::``|(zero ... args)
 *!
 *!   Right side bitwise or/union callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`|()], @[predef::`|()]
 */

/*! @decl mixed lfun::`^(zero ... args)
 *!
 *!   Left side exclusive or callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``^()], @[predef::`^()]
 */

/*! @decl mixed lfun::``^(zero ... args)
 *!
 *!   Right side exclusive or callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`^()], @[predef::`^()]
 */

/*! @decl mixed lfun::`<<(zero arg)
 *!
 *!   Left side left shift callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``<<()], @[predef::`<<()]
 */

/*! @decl mixed lfun::``<<(zero arg)
 *!
 *!   Right side left shift callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`<<()], @[predef::`<<()]
 */

/*! @decl mixed lfun::`>>(zero arg)
 *!
 *!   Left side right shift callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``>>()], @[predef::`>>()]
 */

/*! @decl mixed lfun::``>>(zero arg)
 *!
 *!   Right side right shift callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`>>()], @[predef::`>>()]
 */

/*! @decl mixed lfun::`*(zero ... args)
 *!
 *!   Left side multiplication/repetition/implosion callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``*()], @[predef::`*()]
 */

/*! @decl mixed lfun::``*(zero ... args)
 *!
 *!   Right side multiplication/repetition/implosion callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`*()], @[predef::`*()]
 */

/*! @decl mixed lfun::`/(zero ... args)
 *!
 *!   Left side division/split callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``/()], @[predef::`/()]
 */

/*! @decl mixed lfun::``/(zero ... args)
 *!
 *!   Right side division/split callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`/()], @[predef::`/()]
 */

/*! @decl mixed lfun::`%(zero ... args)
 *!
 *!   Left side modulo callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::``%()], @[predef::`%()]
 */

/*! @decl mixed lfun::``%(zero ... args)
 *!
 *!   Right side modulo callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`%()], @[predef::`%()]
 */

/*! @decl int lfun::`!()
 *!
 *!   Logical not callback.
 *!
 *! @returns
 *!   Returns non-zero if the object should be evaluated as false,
 *!   and @expr{0@} (zero) otherwise.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::`!()]
 */

/*! @decl mixed lfun::`~()
 *!
 *!   Complement/inversion callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::`~()]
 */

/*! @decl int(0..1) lfun::`==(mixed arg)
 *!
 *!   Equality test callback.
 *!
 *! @note
 *!   If this is implemented it might be necessary to implement
 *!   @[lfun::__hash] too. Otherwise mappings might hold several
 *!   objects as indices which are duplicates according to this
 *!   function. Various other functions that use hashing also might
 *!   not work correctly, e.g. @[predef::Array.uniq].
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::`==()], @[lfun::__hash]
 */

/*! @decl int(0..1) lfun::`<(mixed arg)
 *!
 *!   Less than test callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::`<()]
 */

/*! @decl int(0..1) lfun::`>(mixed arg)
 *!
 *!   Greater than test callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::`>()]
 */

/*! @decl int lfun::__hash()
 *!
 *!   Hashing callback.
 *!
 *!   The main caller of this function is @[predef::hash_value()]
 *!   or the low-level equvivalent, which get called by various
 *!   mapping operations when the object is used as index in a mapping.
 *!
 *! @returns
 *!   It should return an integer that corresponds to the object
 *!   in such a way that all values which @[lfun::`==] considers
 *!   equal to the object get the same hash value.
 *!
 *! @note
 *!   The function @[predef::hash] does not return hash values that
 *!   are compatible with this one.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[lfun::`==], @[predef::hash_value()]
 */

/*! @decl mixed lfun::cast(string requested_type)
 *!
 *!   Value cast callback.
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
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 */

/*! @decl mixed lfun::`[..](zero low, int low_bound_type, @
 *!                         zero high, int high_bound_type)
 *!
 *!   Subrange callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::`[..]]
 */

/*! @decl mixed lfun::`[](zero arg1, zero|void arg2)
 *!
 *!   Indexing callback.
 *!
 *!   For compatibility, this is also called to do subranges unless
 *!   there is a @[`[..]] in the class. See @[predef::`[..]] for
 *!   details.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::`[]()], @[predef::`[..]]
 */

/*! @decl mixed lfun::`[]=(zero arg1, zero arg2)
 *!
 *!   Index assignment callback.
 *!
 *! @seealso
 *!   @[predef::`[]=()], @[lfun::`->=()]
 */

/*! @decl mixed lfun::`->(string arg)
 *!
 *!   Arrow index callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::`->()]
 */

/*! @decl mixed lfun::`->=(string arg1, zero arg2)
 *!
 *!   Arrow index assignment callback.
 *!
 *! @seealso
 *!   @[predef::`->=()], @[lfun::`[]=()]
 */

/*! @decl int lfun::_sizeof()
 *!
 *!   Size query callback.
 *!
 *!   Called by @[predef::sizeof()] to determine the number of elements
 *!   in an object. If this function is not present, the number
 *!   of public symbols in the object will be returned.
 *!
 *! @returns
 *!   Expected to return the number of valid indices in the object.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::sizeof()]
 */

/*! @decl array lfun::_indices()
 *!
 *!   List indices callback.
 *!
 *! @returns
 *!   Expected to return an array with the valid indices in the object.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::indices()], @[lfun::_values()]
 */

/*! @decl array lfun::_values()
 *!
 *!   List values callback.
 *!
 *! @returns
 *!   Expected to return an array with the values corresponding to
 *!   the indices returned by @[lfun::_indices()].
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::values()], @[lfun::_indices()]
 */

/*! @decl mixed lfun::`()(zero ... args)
 *!
 *!   Apply callback.
 *!
 *! @seealso
 *!   @[predef::`()]
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
 *!   Expected to return @expr{1@} if the object is to be regarded as a
 *!   simulation of the type specified by @[basic_type].
 *!
 *! @note
 *!   The argument is currently a string with the name
 *!   of the type, but might in the future be a value of the type type.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 */

/*! @decl string lfun::_sprintf(int conversion_type, @
 *!                             mapping(string:int)|void params)
 *!
 *!   Sprintf callback.
 *!
 *!   This method is called by @[predef::sprintf()] to print objects. If it is
 *!   not present, printing of the object will not be supported for any
 *!   conversion-type except for the @tt{%O@}-conversion-type, which
 *!   will output @expr{"object"@}.
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
 *! @note
 *!   This function might be called at odd times, e.g. before
 *!   @[lfun::create] has been called or when an error has occurred.
 *!   The reason is typically that it gets called when a backtrace is
 *!   being formatted to report an error. It should therefore be very
 *!   robust and not make any assumptions about its own internal
 *!   state, at least not when @[conversion_type] is @expr{'O'@}.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!    @[predef::sprintf()]
 */

/*! @decl int lfun::_equal(mixed arg)
 *!
 *!   Recursive equality callback.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::equal()], @[lfun::`==()]
 */

/*! @decl mixed lfun::_m_delete(mixed arg)
 *!
 *!   Delete index callback.
 *!
 *! @seealso
 *!   @[predef::m_delete()]
 */

/*! @decl predef::Iterator lfun::_get_iterator()
 *!
 *!   Iterator creation callback.
 *!
 *!   The returned @[predef::Iterator] instance works as a cursor that
 *!   references a specific item contained (in some arbitrary sense)
 *!   in this one.
 *!
 *! @note
 *!   It's assumed that this function is side-effect free.
 *!
 *! @seealso
 *!   @[predef::Iterator], @[predef::get_iterator], @[predef::foreach()]
 */

/*! @decl mixed lfun::_search(mixed needle, mixed|void start)
 *!
 *!   Search callback.
 *!
 *! @seealso
 *!   @[predef::search()]
 */

/*! @decl mixed lfun::`symbol()
 *! @decl mixed lfun::`->symbol()
 *!
 *!   Variable retrieval callback (aka "getter").
 *!
 *! @note
 *!   Note that the @expr{symbol@} in the name can be any symbol.
 *!
 *! @note
 *!   This is not a true LFUN, since it is even more low level!
 *!
 *! @note
 *!   This function WILL be called even by inheriting programs
 *!   when they attempt to access the variable named @expr{symbol@}.
 *!
 *! @seealso
 *!   @[lfun::`->symbol=()], @[lfun::`->()]
 */

/*! @decl void lfun::`symbol=(zero value)
 *! @decl void lfun::`->symbol=(zero value)
 *!
 *!   Variable assignment callback (aka "setter").
 *!
 *! @note
 *!   Note that the @expr{symbol@} in the name can be any symbol.
 *!
 *! @note
 *!   This is not a true LFUN, since it is even more low level!
 *!
 *! @note
 *!   This function WILL be called even by inheriting programs
 *!   when they attempt to set the variable named @expr{symbol@}.
 *!
 *! @seealso
 *!   @[lfun::`->symbol()], @[lfun::`->=()]
 */

/*! @endnamespace
 */

/*! @class MasterObject
 */

/*! @decl void unregister(program p)
 *!
 *!   Unregister a program that was only partially compiled.
 *!
 *!   Called by @[compile()] to clean up references to partially compiled
 *!   programs.
 *!
 *! @param p
 *!   Partially compiled program that should no longer be referenced.
 *!
 *! @fixme
 *!   Shouldn't this function be in the compilation handler?
 */

/*! @endclass
 */

/*! @class CompilationHandler
 */

/*! @decl mapping(string:mixed)|object get_default_module()
 *!
 *!   Returns the default module from which global symbols will
 *!   be fetched.
 *!
 *! @returns
 *!   Returns the default module, or @expr{0@} (zero).
 *!
 *!   If @expr{0@} (zero) is returned the compiler use the mapping
 *!   returned by @[all_constants()] as fallback.
 *!
 *! @seealso
 *!   @[get_predefines()]
 */

/*! @decl void compile_warning(string filename, int line, string msg)
 *!
 *!   Called by @[compile()] to report warnings.
 *!
 *! @param filename
 *!   File which triggered the warning.
 *!
 *! @param line
 *!   Line which triggered the warning.
 *!
 *! @param msg
 *!   Warning message.
 *!
 *! @seealso
 *!   @[compile_error()]
 */

/*! @endclass
 */

struct program *first_program = 0;
static int current_program_id = PROG_DYNAMIC_ID_START;

struct program *null_program=0;

struct program *compilation_program = 0;
struct program *compilation_env_program = 0;
struct object *compilation_environment = NULL;

struct program *gc_internal_program = 0;
static struct program *gc_mark_program_pos = 0;

static struct mapping *resolve_cache=0;

#ifdef PIKE_DEBUG
#define CHECK_FILE_ENTRY(PROG, POS, LEN, SHIFT)				\
  do {									\
    if (SHIFT < 0 || SHIFT > 2 ||					\
	POS + (LEN << SHIFT) > PROG->linenumbers + PROG->num_linenumbers) \
      Pike_fatal ("Invalid file entry in linenumber info.\n");		\
  } while (0)
#else
#define CHECK_FILE_ENTRY(PROG, POS, LEN, SHIFT) do {} while (0)
#endif

int get_small_number(char **q);

PMOD_EXPORT void do_free_program (struct program *p)
{
  if (p)
    free_program(p);
}

/* So what if we don't have templates? / Hubbe */

#ifdef PIKE_DEBUG
#define CHECK_FOO(NUMTYPE,TYPE,NAME)				              \
  if(Pike_compiler->malloc_size_program-> PIKE_CONCAT(num_,NAME) <            \
       Pike_compiler->new_program-> PIKE_CONCAT(num_,NAME))	              \
    Pike_fatal("Pike_compiler->new_program->num_" #NAME " is out of order\n");\
  if(Pike_compiler->new_program->flags & PROGRAM_OPTIMIZED)		      \
    Pike_fatal("Tried to reallocate fixed program.\n")

#else
#define CHECK_FOO(NUMTYPE,TYPE,NAME)
#endif

#ifndef RELOCATE_program
#define RELOCATE_program(ORIG, NEW)
#endif /* !RELOCATE_program */
#define RELOCATE_identifier_cache(ORIG,NEW)
#define RELOCATE_linenumbers(ORIG,NEW)
#define RELOCATE_identifier_index(ORIG,NEW)
#define RELOCATE_variable_index(ORIG,NEW)
#define RELOCATE_identifier_references(ORIG,NEW)
#define RELOCATE_strings(ORIG,NEW)
#define RELOCATE_inherits(ORIG,NEW)
#define RELOCATE_identifiers(ORIG,NEW)
#define RELOCATE_constants(ORIG,NEW)
#define RELOCATE_relocations(ORIG,NEW)

#if SIZEOF_LONG_LONG == 8
/* we have 8 byte ints, hopefully this constant works on all these systems */
#define MAXVARS(NUMTYPE)						\
   (NUMTYPE)(sizeof(NUMTYPE)==1?254:					\
    (sizeof(NUMTYPE)==2?65534:						\
     (sizeof(NUMTYPE)==4?4294967294U:18446744073709551614ULL)))
#else
#define MAXVARS(NUMTYPE) \
   (NUMTYPE)(sizeof(NUMTYPE)==1?254: (sizeof(NUMTYPE)==2?65534:4294967294U))
#endif

#ifdef PIKE_USE_MACHINE_CODE
/* Special cases for low_add_to_program and add_to_program since
 * many OSes require us to use mmap to allocate memory for our
 * machine code. For decoding efficiency we also want a multi copy
 * variant to be used by decode().
 */
#define BAR(NUMTYPE,TYPE,ARGTYPE,NAME)					\
void PIKE_CONCAT(low_add_to_,NAME) (struct program_state *state,	\
                                    TYPE ARG) {				\
  NUMTYPE m = state->malloc_size_program->PIKE_CONCAT(num_,NAME);	\
  CHECK_FOO(NUMTYPE,TYPE,NAME);						\
  if(m == state->new_program->PIKE_CONCAT(num_,NAME)) {			\
    TYPE *tmp;								\
    if(m==MAXVARS(NUMTYPE)) {						\
      yyerror("Too many " #NAME ".");					\
      return;								\
    }									\
    m = MINIMUM(m*2+1,MAXVARS(NUMTYPE));				\
    tmp = mexec_realloc((void *)state->new_program->NAME,		\
			sizeof(TYPE) * m);				\
    if(!tmp) Pike_fatal("Out of memory.\n");				\
    PIKE_CONCAT(RELOCATE_,NAME)(state->new_program, tmp);		\
    state->malloc_size_program->PIKE_CONCAT(num_,NAME)=m;		\
    state->new_program->NAME=tmp;					\
  }									\
  state->new_program->							\
    NAME[state->new_program->PIKE_CONCAT(num_,NAME)++]=(ARG);		\
}									\
void PIKE_CONCAT(low_add_many_to_,NAME) (struct program_state *state,	\
					 TYPE *ARG, NUMTYPE cnt) {	\
  NUMTYPE m = state->malloc_size_program->PIKE_CONCAT(num_,NAME);	\
  CHECK_FOO(NUMTYPE,TYPE,NAME);						\
  if((m + cnt) > state->new_program->PIKE_CONCAT(num_,NAME)) {		\
    TYPE *tmp;								\
    NUMTYPE n = m;							\
    do {								\
      if(n==MAXVARS(NUMTYPE)) {						\
	yyerror("Too many " #NAME ".");					\
	return;								\
      }									\
      n = MINIMUM(n*2+1,MAXVARS(NUMTYPE));				\
    } while (m + cnt > n);						\
    m = n;								\
    tmp = mexec_realloc((void *)state->new_program->NAME,		\
			sizeof(TYPE) * m);				\
    if(!tmp) Pike_fatal("Out of memory.\n");				\
    PIKE_CONCAT(RELOCATE_,NAME)(state->new_program, tmp);		\
    state->malloc_size_program->PIKE_CONCAT(num_,NAME)=m;		\
    state->new_program->NAME=tmp;					\
  }									\
  MEMCPY(state->new_program->NAME +					\
	 state->new_program->PIKE_CONCAT(num_,NAME),			\
	 ARG, sizeof(TYPE) * cnt);					\
  state->new_program->PIKE_CONCAT(num_,NAME) += cnt;			\
}									\
void PIKE_CONCAT(add_to_,NAME) (ARGTYPE ARG) {				\
  PIKE_CONCAT(low_add_to_,NAME) ( Pike_compiler, ARG );			\
}
#else /* !PIKE_USE_MACHINE_CODE */
#define BAR(NUMTYPE,TYPE,ARGTYPE,NAME)					\
  FOO(NUMTYPE,TYPE,ARGTYPE,NAME)					\
  void PIKE_CONCAT(low_add_many_to_,NAME) (struct program_state *state,	\
					   TYPE *ARG, NUMTYPE cnt) {	\
  NUMTYPE m = state->malloc_size_program->PIKE_CONCAT(num_,NAME);	\
  CHECK_FOO(NUMTYPE,TYPE,NAME);						\
  if((m + cnt) > state->new_program->PIKE_CONCAT(num_,NAME)) {		\
    TYPE *tmp;								\
    NUMTYPE n = m;							\
    do {								\
      if(n==MAXVARS(NUMTYPE)) {						\
	yyerror("Too many " #NAME ".");					\
	return;								\
      }									\
      n = MINIMUM(n*2+1,MAXVARS(NUMTYPE));				\
    } while (m + cnt > n);						\
    m = n;								\
    tmp = realloc((void *)state->new_program->NAME,			\
		  sizeof(TYPE) * m);					\
    if(!tmp) Pike_fatal("Out of memory.\n");				\
    PIKE_CONCAT(RELOCATE_,NAME)(state->new_program, tmp);		\
    state->malloc_size_program->PIKE_CONCAT(num_,NAME)=m;		\
    state->new_program->NAME=tmp;					\
  }									\
  MEMCPY(state->new_program->NAME +					\
	 state->new_program->PIKE_CONCAT(num_,NAME),			\
	 ARG, sizeof(TYPE) * cnt);					\
  state->new_program->PIKE_CONCAT(num_,NAME) += cnt;			\
}
#endif /* PIKE_USE_MACHINE_CODE */

/* Funny guys use the uppermost value for nonexistant variables and
   the like. Hence -2 and not -1. Y2K. */
#define FOO(NUMTYPE,TYPE,ARGTYPE,NAME)					\
void PIKE_CONCAT(low_add_to_,NAME) (struct program_state *state,	\
                                    TYPE ARG) {				\
  NUMTYPE m = state->malloc_size_program->PIKE_CONCAT(num_,NAME);	\
  CHECK_FOO(NUMTYPE,TYPE,NAME);						\
  if(m == state->new_program->PIKE_CONCAT(num_,NAME)) {			\
    TYPE *tmp;								\
    if(m==MAXVARS(NUMTYPE)) {						\
      yyerror("Too many " #NAME ".");					\
      return;								\
    }									\
    m = MINIMUM(m*2+1,MAXVARS(NUMTYPE));				\
    tmp = realloc((void *)state->new_program->NAME,			\
		  sizeof(TYPE) * m);					\
    if(!tmp) Pike_fatal("Out of memory.\n");				\
    PIKE_CONCAT(RELOCATE_,NAME)(state->new_program, tmp);		\
    state->malloc_size_program->PIKE_CONCAT(num_,NAME)=m;		\
    state->new_program->NAME=tmp;					\
  }									\
  state->new_program->							\
    NAME[state->new_program->PIKE_CONCAT(num_,NAME)++]=(ARG);		\
}									\
void PIKE_CONCAT(add_to_,NAME) (ARGTYPE ARG) {				\
  PIKE_CONCAT(low_add_to_,NAME) ( Pike_compiler, ARG );			\
}

/* Funny guys use the uppermost value for nonexistant variables and
   the like. Hence -2 and not -1. Y2K. */
#define PASS1ONLY(NUMTYPE,TYPE,ARGTYPE,NAME)				\
void PIKE_CONCAT(low_add_to_,NAME) (struct program_state *state,	\
                                    TYPE ARG) {				\
  NUMTYPE m = state->malloc_size_program->PIKE_CONCAT(num_,NAME);	\
  CHECK_FOO(NUMTYPE,TYPE,NAME);						\
  DO_IF_DEBUG(if (state->compiler_pass != 1) {				\
		Pike_fatal("Adding " TOSTR(NAME) " in pass %d.\n",	\
			   state->compiler_pass);			\
	      });							\
  if(m == state->new_program->PIKE_CONCAT(num_,NAME)) {			\
    TYPE *tmp;								\
    if(m==MAXVARS(NUMTYPE)) {						\
      yyerror("Too many " #NAME ".");					\
      return;								\
    }									\
    m = MINIMUM(m*2+1,MAXVARS(NUMTYPE));				\
    tmp = realloc((void *)state->new_program->NAME,			\
		  sizeof(TYPE) * m);					\
    if(!tmp) Pike_fatal("Out of memory.\n");				\
    PIKE_CONCAT(RELOCATE_,NAME)(state->new_program, tmp);		\
    state->malloc_size_program->PIKE_CONCAT(num_,NAME)=m;		\
    state->new_program->NAME=tmp;					\
  }									\
  state->new_program->							\
    NAME[state->new_program->PIKE_CONCAT(num_,NAME)++]=(ARG);		\
}									\
void PIKE_CONCAT(add_to_,NAME) (ARGTYPE ARG) {				\
  PIKE_CONCAT(low_add_to_,NAME) ( Pike_compiler, ARG );			\
}


#include "program_areas.h"


#define add_to_program(ARG)	do {				\
    debug_malloc_touch(Pike_compiler->new_program->program);	\
    add_to_program(ARG);					\
  } while(0)

void ins_int(INT32 i, void (*func)(char tmp))
{
  int e;
  unsigned char *p = (unsigned char *)&i;
  for(e=0;e<(long)sizeof(i);e++) {
    func(p[e]);
  }
}

void ins_short(int i, void (*func)(char tmp))
{
  int e;
  unsigned char *p = (unsigned char *)&i;
  for(e=0;e<(long)sizeof(i);e++) {
    func(p[e]);
  }
}

#if 0
/* This check is not possible to do since the identifier is added
 * before checking for duplicates in add_constant. */
static void debug_add_to_identifiers (struct identifier id)
{
  if (d_flag) {
    int i;
    for (i = 0; i < Pike_compiler->new_program->num_identifiers; i++)
      if (Pike_compiler->new_program->identifiers[i].name == id.name) {
	dump_program_tables (Pike_compiler->new_program, 0);
	Pike_fatal ("Adding identifier twice, old at %s:%d #%d.\n",
		    Pike_compiler->new_program->identifiers[i].filename?
		    Pike_compiler->new_program->identifiers[i].filename:"-",
		    Pike_compiler->new_program->identifiers[i].linenumber,
		    i);
      }
  }
  add_to_identifiers (id);
}
#else
#define debug_add_to_identifiers(ARG) add_to_identifiers(ARG)
#endif

void add_relocated_int_to_program(INT32 i)
{
  add_to_relocations(Pike_compiler->new_program->num_program);
  ins_int(i, (void (*)(char))add_to_program);
}

void use_module(struct svalue *s)
{
  struct compilation *c = THIS_COMPILATION;
  if( (1<<s->type) & (BIT_MAPPING | BIT_OBJECT | BIT_PROGRAM))
  {
    c->num_used_modules++;
    Pike_compiler->num_used_modules++;
    assign_svalue_no_free((struct svalue *)
			  low_make_buf_space(sizeof(struct svalue),
					     &c->used_modules), s);
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
  struct compilation *c = THIS_COMPILATION;
  if(!howmany) return;
#ifdef PIKE_DEBUG
  if(howmany *sizeof(struct svalue) > c->used_modules.s.len)
    Pike_fatal("Unusing too many modules.\n");
#endif
  c->num_used_modules -= howmany;
  Pike_compiler->num_used_modules-=howmany;
  low_make_buf_space((ptrdiff_t) sizeof(struct svalue) * -howmany,
		     &c->used_modules);
  free_svalues((struct svalue *)low_make_buf_space(0, &c->used_modules),
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
      handle_compile_exception ("Couldn't index a module with %S.", ident);
    else {
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

	  if (Pike_compiler->compiler_pass == 2 &&
	      ((Pike_sp[-1].type == T_OBJECT &&
		Pike_sp[-1].u.object == placeholder_object) ||
	       (Pike_sp[-1].type == T_PROGRAM &&
		Pike_sp[-1].u.program == placeholder_program))) {
	    my_yyerror("Got placeholder %s (resolver problem) "
		       "when indexing a module with %S.",
		       get_name_of_type (Pike_sp[-1].type), ident);
	    ret = 0;
	  }
	  else {
	    if(!*module_index_cache)
	      *module_index_cache = allocate_mapping(10);
	    mapping_string_insert(*module_index_cache, ident, Pike_sp-1);
	    ret = mksvaluenode(Pike_sp-1);
#if 0 && defined (COMPILER_DEBUG)
	    safe_pike_fprintf (stderr, "Index %S: %O\n", ident, Pike_sp - 1);
#endif
	  }
	  pop_stack();
	  return ret;
	}
	pop_stack();
      }
    }
    UNSETJMP(tmp);
  }

#if 0 && defined (COMPILER_DEBUG)
  safe_pike_fprintf (stderr, "Index %S: undefined\n", ident);
#endif

  return 0;
}

struct node_s *resolve_identifier(struct pike_string *ident);

struct node_s *find_module_identifier(struct pike_string *ident,
				      int see_inherit)
{
  struct compilation *c = THIS_COMPILATION;
  struct node_s *ret;

  struct svalue *modules=(struct svalue *)
    (c->used_modules.s.str + c->used_modules.s.len);

  {
    struct program_state *p=Pike_compiler;
    int n;
    for(n=0;n<=c->compilation_depth;n++,p=p->previous)
    {
      int i;
      if(see_inherit)
      {
	i=really_low_find_shared_string_identifier(ident,
						   p->new_program,
						   SEE_PROTECTED|SEE_PRIVATE);
	if(i!=-1)
	{
	  if ((p->flags & COMPILATION_FORCE_RESOLVE) &&
	      (p->compiler_pass == 2) &&
	      ((p->num_inherits + 1) < p->new_program->num_inherits) &&
	      (PTR_FROM_INT(p->new_program, i)->inherit_offset >
	       p->num_inherits)) {
	    /* Don't look up symbols inherited later, since we need to get
	     * the same symbol in both passes in the force_resolve mode.
	     */
	    continue;
	  }
	  return p == Pike_compiler ?
	    mkidentifiernode(i) :
	    mkexternalnode(p->new_program, i);
	}
      }
      
      if((ret=index_modules(ident,
			    &p->module_index_cache,
			    p->num_used_modules,
			    modules))) return ret;
      modules-=p->num_used_modules;
#ifdef PIKE_DEBUG
      if( ((char *)modules ) < c->used_modules.s.str)
	Pike_fatal("Modules out of whack!\n");
#endif
    }
  }

  return resolve_identifier(ident);
}

/*! @decl constant UNDEFINED
 *!
 *! The undefined value; ie a zero for which @[zero_type()] returns 1.
 */

struct node_s *resolve_identifier(struct pike_string *ident)
{
  struct compilation *c = THIS_COMPILATION;
  node *ret = NULL;

  /* Handle UNDEFINED */
  if (ident == UNDEFINED_string) {
    return mkconstantsvaluenode(&svalue_undefined);
  }

  if(resolve_cache)
  {
    struct svalue *tmp=low_mapping_string_lookup(resolve_cache,ident);
    if(tmp)
    {
      if(!IS_UNDEFINED (tmp))
	return mkconstantsvaluenode(tmp);

      return 0;
    }
  }

  CHECK_COMPILER();

  ref_push_string(ident);
  ref_push_string(c->lex.current_file);
  if (c->handler) {
    ref_push_object(c->handler);
  } else {
    push_int(0);
  }
  if (!safe_apply_current2(PC_RESOLV_FUN_NUM, 3, NULL))
    handle_compile_exception ("Error resolving '%S'.", ident);

  if (Pike_compiler->compiler_pass != 2) {
    /* If we get a program that hasn't gone through pass 1 yet then we
     * have to register a dependency now in our pass 1 so that our
     * pass 2 gets delayed. Otherwise the other program might still be
     * just as unfinished when we come back here in pass 2. */
    struct program *p = NULL;
    if (Pike_sp[-1].type == T_PROGRAM)
      p = Pike_sp[-1].u.program;
    else if (Pike_sp[-1].type == T_OBJECT ||
	     (Pike_sp[-1].type == T_FUNCTION &&
	      Pike_sp[-1].subtype != FUNCTION_BUILTIN))
      p = Pike_sp[-1].u.object->prog;
    if (p && !(p->flags & PROGRAM_PASS_1_DONE))
      report_compiler_dependency (p);
  }

  if (Pike_compiler->compiler_pass == 2 &&
      ((Pike_sp[-1].type == T_OBJECT &&
	Pike_sp[-1].u.object == placeholder_object) ||
       (Pike_sp[-1].type == T_PROGRAM &&
	Pike_sp[-1].u.program == placeholder_program))) {
    my_yyerror("Got placeholder %s (resolver problem) "
	       "when resolving '%S'.",
	       get_name_of_type (Pike_sp[-1].type), ident);
  } else {
    if(!resolve_cache)
      resolve_cache=dmalloc_touch(struct mapping *, allocate_mapping(10));
    mapping_string_insert(resolve_cache,ident,Pike_sp-1);

    if(!IS_UNDEFINED (Pike_sp-1))
    {
      ret=mkconstantsvaluenode(Pike_sp-1);
    }
  }
  pop_stack();

  return ret;
}

/*! @decl constant this
 *!
 *! Builtin read only variable that evaluates to the current object.
 *!
 *! @seealso
 *!   @[this_program], @[this_object()]
 */

/*! @decl constant this_program
 *!
 *! Builtin constant that evaluates to the current program.
 *!
 *! @seealso
 *!   @[this], @[this_object()]
 */

/* If the identifier is recognized as one of the magic identifiers,
 * like "this", "this_program" or "`->" when preceded by ::, then a
 * suitable node is returned, NULL otherwise.
 *
 * inherit_num is -1 when no specific inherit has been specified; ie
 * either when the identifier has no prefix (colon_colon_ref == 0) or
 * when the identifier has the prefix :: without any preceding identifier
 * (colon_colon_ref == 1).
 */
struct node_s *program_magic_identifier (struct program_state *state,
					 int state_depth, int inherit_num,
					 struct pike_string *ident,
					 int colon_colon_ref)
{
#if 0
  fprintf (stderr, "magic_identifier (state, %d, %d, %s, %d)\n",
	   state_depth, inherit_num, ident->str, colon_colon_ref);
#endif

  if (!inherit_num && TEST_COMPAT(7,6)) {
    /* Pike 7.6 and earlier couldn't refer to the current inherit. */
    inherit_num = -1;
  }

  if ((inherit_num == -1) || (!TEST_COMPAT(7,6) && (inherit_num >= 0))) {
    if (ident == this_string) {
      /* Handle this. */
      return mkthisnode(state->new_program, inherit_num);
    }

    /* Handle this_program */
    if (ident == this_program_string) {
      node *n = mkefuncallnode("object_program",
			       mkthisnode(state->new_program, inherit_num));
      /* We know this expression is constant. */
      n->node_info &= ~OPT_NOT_CONST;
      n->tree_info &= ~OPT_NOT_CONST;
      return n;
    }

    /* Handle this_function */
    if (ident == this_function_string) {
      int i;
      if ((i = Pike_compiler->compiler_frame->current_function_number) >= 0) {
	struct identifier *id;
	id = ID_FROM_INT(Pike_compiler->new_program, i);
	if (id->identifier_flags & IDENTIFIER_SCOPED) {
	  return mktrampolinenode(i, Pike_compiler->compiler_frame->previous);
	} else {
	  return mkidentifiernode(i);
	}
      } else {
	/* FIXME: Fall back to __INIT? */
      }
    }
  }

  if (colon_colon_ref) {
    /* These are only recognized when prefixed with the :: operator. */

    if (inherit_num < 0) inherit_num = 0;
    if(ident == lfun_strings[LFUN_ARROW] ||
       ident == lfun_strings[LFUN_INDEX]) {
      return mknode(F_MAGIC_INDEX, mknewintnode(inherit_num),
		    mknewintnode(state_depth));
    } else if(ident == lfun_strings[LFUN_ASSIGN_ARROW] ||
	      ident == lfun_strings[LFUN_ASSIGN_INDEX]) {
      return mknode(F_MAGIC_SET_INDEX, mknewintnode(inherit_num),
		    mknewintnode(state_depth));
    } else if(ident == lfun_strings[LFUN__INDICES]) {
      return mknode(F_MAGIC_INDICES, mknewintnode(inherit_num),
		    mknewintnode(state_depth));
    } else if(ident == lfun_strings[LFUN__VALUES]) {
      return mknode(F_MAGIC_VALUES, mknewintnode(inherit_num),
		    mknewintnode(state_depth));
    }
  }

  return NULL;
}

/* Fixme: allow level=0 to return the current level */
struct program *parent_compilation(int level)
{
  struct compilation *c = THIS_COMPILATION;
  int n;
  struct program_state *p=Pike_compiler->previous;
  for(n=0;n<level;n++)
  {
    if(n>=c->compilation_depth) return 0;
    p=p->previous;
    if(!p) return 0;
  }
  return p->new_program;
}

#define ID_TO_PROGRAM_CACHE_SIZE 512
struct program *id_to_program_cache[ID_TO_PROGRAM_CACHE_SIZE];

struct program *id_to_program(INT32 id)
{
  struct program_state *state;
  struct program *p;
  INT32 h;
  if(!id) return 0;

  /* fprintf(stderr, "id_to_program(%d)... ", id); */

  h=id & (ID_TO_PROGRAM_CACHE_SIZE-1);

  if((p=id_to_program_cache[h]))
    if(p->id==id) {
      /* fprintf(stderr, "cached: %p\n", p); */
      return p;
    }

  for(p=first_program;p;p=p->next)
  {
    if(id==p->id)
    {
      id_to_program_cache[h]=p;
      /* fprintf(stderr, "found: %p\n", p); */
      return p;
    }
  }

  /* Check if it is a program being compiled right now. */
  for (state = Pike_compiler; state; state = state->previous) {
    if (state->new_program && state->new_program->id == id) {
      return state->new_program;
    }
  }

  if ((id > 0) && (id < PROG_DYNAMIC_ID_START)) {
    /* Reserved id. Attempt to load the proper dynamic module
     * to resolv the id.
     */
    char *module = NULL;

    /* fprintf(stderr, "reserved "); */

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
      } else if ((id >= 1000) && (id < 2000)) {
	module = "___GTK";
      } else if ((id >= 2000) && (id < 3000)) {
	module = "___GTK2";
      }
      break;
    }
    if (module && get_master()) {
      /* fprintf(stderr, "%s... ", module); */
      push_text(module);
      SAFE_APPLY_MASTER("resolv", 1);
      pop_stack();

      /* Try again... */
      for(p=first_program;p;p=p->next)
      {
	if(id==p->id)
	{
	  id_to_program_cache[h]=p;
	  /* fprintf(stderr, "found: %p\n", p); */
	  return p;
	}
      }
    }
  }
  /* fprintf(stderr, "not found\n"); */
  return 0;
}

/* Here starts routines which are used to build new programs */

/*
 * A typical program goes through the following steps:
 *
 * allocate_program  ==> PROGRAM_VIRGIN
 *
 * start_program     ==> !PROGRAM_VIRGIN
 *
 * end_first_pass    ==> PROGRAM_PASS_1_DONE
 *
 * fixate_program    ==> PROGRAM_FIXED
 *
 * optimize_program  ==> PROGRAM_OPTIMIZED 
 *
 * end_first_pass(1) ==> PROGRAM_FINISHED
 */

/* Re-allocate all the memory in the program in one chunk. because:
 * 1) The individual blocks are much bigger than they need to be
 * 2) cuts down on malloc overhead (maybe)
 * 3) localizes memory access (decreases paging)
 */
void optimize_program(struct program *p)
{
  size_t size=0;
  char *data;

  /* Already done (shouldn't happen, but who knows?) */
  if(p->flags & PROGRAM_OPTIMIZED) return;

#ifdef PIKE_USE_MACHINE_CODE
  /* Don't move our mexec-allocated memory into the malloc... */
#define BAR(NUMTYPE,TYPE,ARGTYPE,NAME)
#endif /* PIKE_USE_MACHINE_CODE */ 

#define FOO(NUMTYPE,TYPE,ARGTYPE,NAME) \
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

#ifdef PIKE_USE_MACHINE_CODE
  /* As above. */
#define BAR(NUMTYPE,TYPE,ARGTYPE,NAME)
#endif /* PIKE_USE_MACHINE_CODE */ 

#define FOO(NUMTYPE,TYPE,ARGTYPE,NAME) \
  size=DO_ALIGN(size, ALIGNOF(TYPE)); \
  if (p->PIKE_CONCAT (num_, NAME))					\
    MEMCPY(data+size,p->NAME,p->PIKE_CONCAT(num_,NAME)*sizeof(p->NAME[0])); \
  PIKE_CONCAT(RELOCATE_,NAME)(p, (TYPE *)(data+size)); \
  dmfree(p->NAME); \
  p->NAME=(TYPE *)(data+size); \
  size+=p->PIKE_CONCAT(num_,NAME)*sizeof(p->NAME[0]);
#include "program_areas.h"

  p->total_size=size + sizeof(struct program);

  p->flags |= PROGRAM_OPTIMIZED;
  make_program_executable(p);
}

/* internal function to make the index-table */
static int program_identifier_index_compare(int a, int b,
					    const struct program *p)
{
  size_t val_a = PTR_TO_INT (ID_FROM_INT(p, a)->name);
  size_t val_b = PTR_TO_INT (ID_FROM_INT(p, b)->name);
  return val_a < val_b ? -1 : (val_a == val_b ? (a < b? -1:(a != b)) : 1);
}

#define CMP(X,Y)	program_identifier_index_compare(*(X), *(Y), prog)
#define EXTRA_ARGS	, struct program *prog
#define XARGS		, prog
#define ID		fsort_program_identifier_index
#define TYPE		unsigned short
#include "fsort_template.h"
#undef TYPE
#undef ID
#undef XARGS
#undef EXTRA_ARGS
#undef CMP

#ifdef PIKE_DEBUG
struct pike_string *find_program_name(struct program *p, INT32 *line)
{
  INT32 l;
  if(!line) line=&l;

#ifdef DEBUG_MALLOC
  {
    char *tmp=dmalloc_find_name(p);
    if (tmp) {
      char *p = STRCHR (tmp, ':');
      if (p) {
	char *pp;
	while ((pp = STRCHR (p + 1, ':'))) p = pp;
	*line = atoi (p + 1);
	return make_shared_binary_string (tmp, p - tmp);
      }
      else {
	*line=0;
	return make_shared_string(tmp);
      }
    }
  }
#endif

  return get_program_line(p, line);
}
#endif

int override_identifier (struct reference *new_ref, struct pike_string *name)
{
  struct compilation *c = THIS_COMPILATION;
  int id = -1, cur_id = 0, is_used = 0;

  int new_is_variable =
    IDENTIFIER_IS_VARIABLE(ID_FROM_PTR(Pike_compiler->new_program,
				       new_ref)->identifier_flags);

  /* This loop could possibly be optimized by looping over
   * each inherit and looking up 'name' in each inherit
   * and then see if should be overwritten
   * /Hubbe
   */

  CHECK_COMPILER();

  for(;cur_id<Pike_compiler->new_program->num_identifier_references;cur_id++)
  {
    struct reference *ref =
      Pike_compiler->new_program->identifier_references + cur_id;
    struct identifier *i;

    /* No need to do anything for ourselves. */
    if (ref == new_ref) continue;

    /* Do not zapp hidden identifiers */
    if(ref->id_flags & ID_HIDDEN) continue;

    /* Do not zapp functions with the wrong name... */
    if((i = ID_FROM_PTR(Pike_compiler->new_program, ref))->name != name)
      continue;

    /* Do not zapp inherited inline ('local') identifiers,
     * or inherited externals with new externals,
     * since this makes it hard to identify in encode_value().
     */
    if((ref->id_flags & (ID_INLINE|ID_INHERITED)) == (ID_INLINE|ID_INHERITED)
       || (ref->id_flags & new_ref->id_flags & ID_EXTERN)) {
      /* But we still need to hide them, since we shadow them... */
      ref->id_flags |= ID_HIDDEN;
      continue;
    }

#ifdef PROGRAM_BUILD_DEBUG
    fprintf(stderr, "%.*soverloaded reference %d (id_flags:0x%04x)\n",
	    c->compilation_depth, "", cur_id, ref->id_flags);
#endif

    if (!new_is_variable && IDENTIFIER_IS_VARIABLE(i->identifier_flags)) {
      /* Overloading a variable with a constant or a function.
       * This is generally a bad idea.
       */
      ref->id_flags |= ID_INLINE|ID_HIDDEN;
      yywarning("Attempt to override a non local variable %S "
		"with a non-variable.", name);
      continue;
    }

    if ((ref->id_flags & (ID_INHERITED|ID_USED)) == (ID_INHERITED|ID_USED)) {
      struct inherit *inh = INHERIT_FROM_PTR(Pike_compiler->new_program, ref);
      struct reference *sub_ref;

      /* Find the inherit one level away. */
      while (inh->inherit_level > 1) inh--;

#ifdef PIKE_DEBUG
      if (!inh->inherit_level) {
	Pike_fatal("Inherit without intermediate levels.\n");
      }
#endif

      sub_ref = PTR_FROM_INT(inh->prog, cur_id - inh->identifier_level);

      /* Check if the symbol was used before it was inherited. */
      if ((c->lex.pragmas & ID_STRICT_TYPES) &&
	  (sub_ref->id_flags & ID_USED)) {
	struct identifier *sub_id = ID_FROM_PTR(inh->prog, sub_ref);
	if (IDENTIFIER_IS_FUNCTION(sub_id->identifier_flags)) {
	  if ((Pike_compiler->compiler_pass == 2) &&
	      !pike_types_le(ID_FROM_PTR(Pike_compiler->new_program,
					 new_ref)->type, sub_id->type)) {
	    yytype_report(REPORT_WARNING,
			  NULL, 0, sub_id->type,
			  NULL, 0, ID_FROM_PTR(Pike_compiler->new_program,
					       new_ref)->type,
			  0, "Type mismatch when overloading function %S.",
			  name);
	  }
	} else {
	  struct identifier *new_id;
	  /* Variable or constant.
	   *
	   * Note: Use weaker check for integers (especially for constants).
	   */
	  if (!pike_types_le(sub_id->type,
			     (new_id = ID_FROM_PTR(Pike_compiler->new_program,
						   new_ref))->type) &&
	      !((i->run_time_type == PIKE_T_INT) &&
		(new_id->run_time_type == PIKE_T_INT) &&
		(IDENTIFIER_IS_CONSTANT(sub_id->identifier_flags) ||
		 match_types(sub_id->type, new_id->type)))) {
	    yytype_report(REPORT_WARNING,
			  NULL, 0, sub_id->type,
			  NULL, 0, new_id->type,
			  0, "Type mismatch when overloading %S.", name);
	  }
	}
      }
    }
    is_used = ref->id_flags & ID_USED;

    *ref=*new_ref;
    ref->id_flags |= is_used;
    id = cur_id;
  }

  return id;
}

void fixate_program(void)
{
  struct compilation *c = THIS_COMPILATION;
  INT32 i,e,t;
  struct program *p=Pike_compiler->new_program;

  CHECK_COMPILER();

  if(p->flags & PROGRAM_FIXED) return;
#ifdef PIKE_DEBUG
  if(p->flags & PROGRAM_OPTIMIZED)
    Pike_fatal("Cannot fixate optimized program\n");
#endif

  /* Fixup the runtime type for functions.
   * (T_MIXED is used as the tentative type marker in pass 1).
   */
  for (i=0; i < p->num_identifiers; i++) {
    if (IDENTIFIER_IS_FUNCTION(p->identifiers[i].identifier_flags) &&
	(p->identifiers[i].run_time_type == T_MIXED)) {
      /* Get rid of the remaining tentative type marker. */
      /* FIXME: Should probably never be reachable.
       *        Consider this a fatal?
       */
      p->identifiers[i].run_time_type = T_FUNCTION;
    }
  }

  /* Fixup identifier overrides. */
  for (i = 0; i < p->num_identifier_references; i++) {
    struct reference *ref = p->identifier_references + i;
    if (ref->id_flags & ID_HIDDEN) continue;
    if (ref->inherit_offset != 0) continue;
    override_identifier (ref, ID_FROM_PTR (p, ref)->name);

    if ((ref->id_flags & (ID_HIDDEN|ID_PRIVATE|ID_USED)) == ID_PRIVATE) {
      yywarning("%S is private but not used anywhere.",
		ID_FROM_PTR(p, ref)->name);
    }
  }

  /* Ok, sort for binsearch */
  for(e=i=0;i<(int)p->num_identifier_references;i++)
  {
    struct reference *funp;
    struct identifier *fun;
    funp=p->identifier_references+i;
    if(funp->id_flags & ID_HIDDEN) continue;
    fun=ID_FROM_PTR(p, funp);
    if(funp->id_flags & ID_INHERITED)
    {
      int found_better=-1;
      int funa_is_prototype;

      /* NOTE: Mixin is currently not supported for PRIVATE symbols. */
      if(funp->id_flags & ID_PRIVATE) continue;
      funa_is_prototype = fun->func.offset == -1;
/*    if(fun->func.offset == -1) continue; * prototype */

      /* check for multiple definitions */
      for(t=i+1;t<(int)p->num_identifier_references;t++)
      {
	struct reference *funpb;
	struct identifier *funb;

	funpb=p->identifier_references+t;
	if (funpb->id_flags & ID_HIDDEN) continue;
	funb=ID_FROM_PTR(p,funpb);
	/* if(funb->func.offset == -1) continue; * prototype */

	if(fun->name==funb->name)
	{
	  if (!(funpb->id_flags & ID_PROTECTED)) {
	    /* Only regard this symbol as better if it
	     * will end up in the index further below.
	     */
	    found_better=t;
	  }

	  /* FIXME: Is this stuff needed?
	   *        It looks like it already is done by define_function().
	   *
	   *        Yes -- It's needed in case of mixin.
	   */
	  if(funa_is_prototype && (funb->func.offset != -1) &&
	     !(funp->id_flags & ID_INLINE))
	  {
	    if ((c->lex.pragmas & ID_STRICT_TYPES) &&
		(funp->id_flags & ID_USED)) {
	      /* Verify that the types are compatible. */
	      if (!pike_types_le(funb->type, fun->type)) {
		yytype_report(REPORT_WARNING, NULL, 0, fun->type,
			      NULL, 0, funb->type,
			      0, "Type mismatch when overloading %S.",
			      fun->name);
	      }
	    }
	    funp->inherit_offset = funpb->inherit_offset;
	    funp->identifier_offset = funpb->identifier_offset;
	  }
	  if(!funa_is_prototype && funb->func.offset == -1)
	  {
	    if ((c->lex.pragmas & ID_STRICT_TYPES) &&
		(funpb->id_flags & ID_USED)) {
	      /* Verify that the types are compatible. */
	      if (!pike_types_le(fun->type, funb->type)) {
		yytype_report(REPORT_WARNING, NULL, 0, funb->type,
			      NULL, 0, fun->type,
			      0, "Type mismatch when overloading %S.",
			      fun->name);
	      }
	    }
	    funpb->inherit_offset = funp->inherit_offset;
	    funpb->identifier_offset = funp->identifier_offset;
	  }
	}
      }
      if(found_better!=-1)
	continue;
    }
    if (IDENTIFIER_IS_PIKE_FUNCTION(fun->identifier_flags) &&
	(fun->func.offset == -1) && (funp->id_flags & ID_INLINE)) {
      my_yyerror("Missing definition for local function %S.",
		 fun->name);
    }
    if (funp->id_flags & ID_PROTECTED) continue;
    add_to_identifier_index(i);
  }
  fsort_program_identifier_index(p->identifier_index,
				 p->identifier_index +
				 p->num_identifier_index - 1,
				 p);

  /* Take care of duplicates in the identifier index table;
   * this can happen eg when the overloading definition is
   * before an inherit being overloaded. This happens for
   * eg the default master object.
   */
  if (p->num_identifier_index) {
    struct identifier *id = ID_FROM_INT(p, p->identifier_index[0]);
    for (e = i = 1; e < p->num_identifier_index; e++) {
      struct identifier *probe = ID_FROM_INT(p, p->identifier_index[e]);
      if (probe == id) {
	/* Duplicate. */
	continue;
      }
      p->identifier_index[i++] = p->identifier_index[e];
      id = probe;
    }
    p->num_identifier_index = i;
  }

  p->flags |= PROGRAM_FIXED;

  /* Yes, it is supposed to start at 1  /Hubbe */
  for(i=1;i<NUM_LFUNS;i++) {
    int id = p->lfuns[i] = low_find_lfun(p, i);
  }

  /* Set the PROGRAM_LIVE_OBJ flag by looking for destroy() and
   * inherited PROGRAM_LIVE_OBJ flags. This is done at fixation time
   * to allow the user to set and clear that flag while the program is
   * being built. */
  if (!(p->flags & PROGRAM_LIVE_OBJ)) {
    int e, destroy = p->lfuns[LFUN_DESTROY];
    if (destroy > -1) {
      struct identifier *id = ID_FROM_INT (p, destroy);
      if (!IDENTIFIER_IS_PIKE_FUNCTION (id->identifier_flags) ||
	  id->func.offset != -1) {
	/* Got a destroy function that isn't a prototype. */
	p->flags |= PROGRAM_LIVE_OBJ;
	goto program_live_obj_set;
      }
    }

    for (e = p->num_inherits - 1; e >= 0; e--)
      if (p->inherits[e].prog->flags & PROGRAM_LIVE_OBJ) {
	p->flags |= PROGRAM_LIVE_OBJ;
	break;
      }

  program_live_obj_set:;
  }

  if(Pike_compiler->flags & COMPILATION_CHECK_FINAL)
  {
    for(i=0;i<(int)p->num_identifier_references;i++)
    {
      if((p->identifier_references[i].id_flags & (ID_FINAL|ID_HIDDEN)) ==
	 ID_FINAL)
      {
	struct pike_string *name=ID_FROM_INT(p, i)->name;

	e=find_shared_string_identifier(name,p);
	if(e == -1)
	  e=really_low_find_shared_string_identifier(name, p,
						     SEE_PROTECTED|SEE_PRIVATE);

	if((e != i) && (e != -1))
	{
	  my_yyerror("Illegal to redefine final identifier %S", name);
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
    if(c->lex.current_file && 
       c->lex.current_file->str &&
       c->lex.current_file->len &&
       !strcmp(c->lex.current_file->str,"-"))
    {
      m=dmalloc_alloc_mmap( DBSTR(c->lex.current_file), c->lex.current_line);
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
			   int pass,
			   struct pike_string *name,
			   int flags,
			   int *idp)
{
  struct compilation *c = THIS_COMPILATION;
  int id=0;
  struct svalue tmp;

  CHECK_COMPILER();

#ifdef WITH_FACETS
  if(Pike_compiler->compiler_pass == 1 && p) {
    p->facet_index = -1;
    p->facet_group = NULL;
  }
#endif

  /* We don't want to change thread, but we don't want to
   * wait for the other threads to complete either.
   */
  low_init_threads_disable();

  c->compilation_depth++;

  if (!Pike_compiler->compiler_frame) {
    new_node_s_context();
  }

  tmp.type=T_PROGRAM;
  if(!p)
  {
    p=low_allocate_program();
    if(name)
    {
      tmp.u.program=p;
      id=add_constant(name, &tmp, flags & ~ID_EXTERN);
#if 0
      fprintf(stderr,"Compiling class %s, depth=%d\n",
	      name->str, c->compilation_depth);
    }else{
      fprintf(stderr,"Compiling file %s, depth=%d\n",
	      c->lex.current_file ? c->lex.current_file->str : "-",
	      c->compilation_depth);
#endif
    }
  }else{
    tmp.u.program=p;
    add_ref(p);
    if((pass == 2) && name)
    {
      struct identifier *i;
      id=isidentifier(name);
      if (id < 0)
	Pike_fatal("Program constant disappeared in second pass.\n");
      i=ID_FROM_INT(Pike_compiler->new_program, id);
      free_type(i->type);
      i->type=get_type_of_svalue(&tmp);
    }
  }
  if (pass == 1) {
    if(c->compilation_depth >= 1) {
      add_ref(p->parent = Pike_compiler->new_program);
      debug_malloc_touch (p);
    }
  }
  p->flags &=~ PROGRAM_VIRGIN;
  if(idp) *idp=id;

  CDFPRINTF((stderr, "th(%ld) %p low_start_new_program() %s "
	     "pass=%d: threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), p, name ? name->str : "-",
	     Pike_compiler->compiler_pass,
	     threads_disabled, c->compilation_depth));

  init_type_stack();

#define PUSH
#include "compilation.h"

  Pike_compiler->parent_identifier=id;
  Pike_compiler->compiler_pass = pass;

  Pike_compiler->num_used_modules=0;	/* FIXME: Duplicate? */

  if(p->flags & PROGRAM_FINISHED)
  {
    yyerror("Pass2: Program already done");
  }

  Pike_compiler->malloc_size_program = ALLOC_STRUCT(program);
  Pike_compiler->fake_object=alloc_object();

#ifdef PIKE_DEBUG
  Pike_compiler->fake_object->storage=(char *)malloc(256 * sizeof(struct svalue));
  if (Pike_compiler->fake_object->storage) {
    /* Stipple to find illegal accesses */
    MEMSET(Pike_compiler->fake_object->storage,0x55,256*sizeof(struct svalue));
    PIKE_MEM_WO_RANGE (Pike_compiler->fake_object->storage,
		       256 * sizeof (struct svalue));
  }
#else
  Pike_compiler->fake_object->storage=(char *)malloc(sizeof(struct parent_info));
#endif
  if (!Pike_compiler->fake_object->storage) {
    yyerror("Out of memory when allocating object storage.");
  }
  /* Can't use GC_ALLOC on fake objects, but still it's good to know
   * that they never take over a stale gc marker. */
  if (Pike_in_gc) remove_marker(Pike_compiler->fake_object);

  Pike_compiler->fake_object->next=Pike_compiler->fake_object;
  Pike_compiler->fake_object->prev=Pike_compiler->fake_object;
  Pike_compiler->fake_object->refs=0;
  add_ref(Pike_compiler->fake_object);	/* For DMALLOC... */
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

  if (Pike_compiler->fake_object->storage) {
    if(name)
    {
      /* Fake objects have parents regardless of PROGRAM_USES_PARENT */
      if((((struct parent_info *)Pike_compiler->fake_object->storage)->parent=Pike_compiler->previous->fake_object))
	add_ref(Pike_compiler->previous->fake_object);
      ((struct parent_info *)Pike_compiler->fake_object->storage)->parent_identifier=id;
    }else{
      ((struct parent_info *)Pike_compiler->fake_object->storage)->parent=0;
      ((struct parent_info *)Pike_compiler->fake_object->storage)->parent_identifier=0;
    }
  }

  Pike_compiler->new_program=p;

#ifdef PROGRAM_BUILD_DEBUG
  if (name) {
    fprintf (stderr, "%.*sstarting program %d (pass=%d): ",
	     c->compilation_depth, "",
	     Pike_compiler->new_program->id, Pike_compiler->compiler_pass);
    push_string (name);
    print_svalue (stderr, --Pike_sp);
    putc ('\n', stderr);
  }
  else
    fprintf (stderr, "%.*sstarting program %d (pass=%d)\n",
	     c->compilation_depth, "",
	     Pike_compiler->new_program->id, Pike_compiler->compiler_pass);
#endif

  if (c->compilation_depth >= 1) {
    if(TEST_COMPAT(7,2) || (c->lex.pragmas & ID_SAVE_PARENT))
    {
      p->flags |= PROGRAM_USES_PARENT;
    }else if (!(c->lex.pragmas & ID_DONT_SAVE_PARENT)) {
      struct pike_string *tmp=findstring("__pragma_save_parent__");
      if(tmp)
      {
	struct node_s *n=find_module_identifier(tmp, 0);
	if(n)
	{
	  int do_save_parent = !node_is_false(n); /* Default to true. */
	  free_node(n);
	  if (do_save_parent) p->flags |= PROGRAM_USES_PARENT;
	}
      }
    }
  }

  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);

  if(Pike_compiler->new_program->program)
  {
#define FOO(NUMTYPE,TYPE,ARGTYPE,NAME)					\
    Pike_compiler->malloc_size_program->PIKE_CONCAT(num_,NAME) =	\
      Pike_compiler->new_program->PIKE_CONCAT(num_,NAME);
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
	  CHECK_FILE_ENTRY (Pike_compiler->new_program, cnt, len, shift);
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
#ifdef PIKE_USE_MACHINE_CODE
#define BAR(NUMTYPE,TYPE,ARGTYPE,NAME)	\
    if (Pike_compiler->new_program->NAME) {				\
      mexec_free(Pike_compiler->new_program->NAME);			\
      Pike_compiler->new_program->PIKE_CONCAT(num_,NAME) = 0;		\
    }									\
    Pike_compiler->malloc_size_program->PIKE_CONCAT(num_,NAME) =	\
      START_SIZE;							\
    Pike_compiler->new_program->NAME =					\
      (TYPE *)mexec_alloc(sizeof(TYPE) * START_SIZE);
#endif /* PIKE_USE_MACHINE_CODE */ 
#define FOO(NUMTYPE,TYPE,ARGTYPE,NAME)					\
    if (Pike_compiler->new_program->NAME) {				\
      free (Pike_compiler->new_program->NAME);				\
      Pike_compiler->new_program->PIKE_CONCAT(num_,NAME) = 0;		\
    }									\
    Pike_compiler->malloc_size_program->PIKE_CONCAT(num_,NAME) =	\
      START_SIZE;							\
    Pike_compiler->new_program->NAME =					\
      (TYPE *)xalloc(sizeof(TYPE) * START_SIZE);
#include "program_areas.h"

    i.prog=Pike_compiler->new_program;
    i.identifier_level=0;
    i.storage_offset=0;
    i.inherit_level=0;
    i.identifier_ref_offset=0;
    i.parent=0;
    i.parent_identifier=-1;
    i.parent_offset=OBJECT_PARENT;
    i.name=0;
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

PMOD_EXPORT void debug_start_new_program(int line, const char *file)
{
  struct pike_string *save_file;
  int save_line;
  struct compilation *c;

  CHECK_COMPILER();
  c = THIS_COMPILATION;

  save_file = dmalloc_touch(struct pike_string *, c->lex.current_file);
  save_line = c->lex.current_line;

  { /* Trim off the leading path of the compilation environment. */
    const char *p = DEFINETOSTR(PIKE_SRC_ROOT), *f = file;
    while (*p && *p == *f) p++, f++;
    while (*f == '/' || *f == '\\') f++;

    c->lex.current_file = make_shared_string(f);
    c->lex.current_line = line;
  }

  CDFPRINTF((stderr,
	     "th(%ld) start_new_program(%d, %s): "
	     "threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), line, file,
	     threads_disabled, c->compilation_depth));

  low_start_new_program(0,1,0,0,0);
  store_linenumber(line,c->lex.current_file);
  debug_malloc_name(Pike_compiler->new_program, file, line);

  free_string(c->lex.current_file);
  c->lex.current_file = dmalloc_touch(struct pike_string *, save_file);
  c->lex.current_line = save_line;
}


static void exit_program_struct(struct program *p)
{
  unsigned e;

#ifdef PIKE_DEBUG
  if (p->refs) {
#ifdef DEBUG_MALLOC
    fprintf (stderr, "Program to be freed still got %d references:\n", p->refs);
    describe_something(p, T_PROGRAM, 0,2,0, NULL);
#endif
    Pike_fatal("Program to be freed still got %d references.\n", p->refs);
  }
#endif

  if(p->parent)
  {
    /* Make sure to break the circularity... */
    struct program *parent = p->parent;
    p->parent = NULL;
#ifdef PIKE_DEBUG
    if (!parent->refs) {
      dump_program_tables(p, 2);
      fprintf(stderr, "Dead parent:\n");
      dump_program_tables(parent, 2);
      Pike_fatal("Program parent is dead.\n");
    }
#endif
    free_program(parent);
  }

  /* fprintf(stderr, "Exiting program: %p, id:%d\n", p, p->id); */

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
#if 0
      if(p->constants[e].name) free_string(p->constants[e].name);
#endif /* 0 */
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

#ifdef WITH_FACETS
  if(p->facet_group)
  {
    free_object(p->facet_group);
  }
#endif

  if(p->flags & PROGRAM_OPTIMIZED)
  {
#ifdef PIKE_USE_MACHINE_CODE
    do {
      /* NOTE: Assumes all BAR's are before any FOO. */
#define BAR(NUMTYPE,TYPE,ARGTYPE,NAME)		\
      if (p->NAME) mexec_free(p->NAME);
#define FOO(NUMTYPE,TYPE,ARGTYPE,NAME)		\
      if (p->NAME) {				\
	dmfree(p->NAME);			\
	break;					\
      }
#include "program_areas.h"
    } while(0);
#else /* PIKE_USE_MACHINE_CODE */
    if(p->program) {
      dmfree(p->program);
    }
#endif /* PIKE_USE_MACHINE_CODE */
#define FOO(NUMTYPE,TYPE,ARGTYPE,NAME)	do {			     \
      p->NAME=0;						     \
    } while(0);
    
#include "program_areas.h"
  }else{
#ifdef PIKE_USE_MACHINE_CODE
#define BAR(NUMTYPE,TYPE,ARGTYPE,NAME)				\
    if(p->NAME) {						\
      mexec_free((char *)p->NAME); p->NAME=0;			\
    }
#endif /* PIKE_USE_MACHINE_CODE */
#define FOO(NUMTYPE,TYPE,ARGTYPE,NAME)			\
    if(p->NAME) {					\
      dmfree((char *)p->NAME); p->NAME=0;		\
    }
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

  fprintf(stderr,"$$$$$ dump_program_desc for %p\n", p);

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

  fprintf(stderr,"All identifier references:\n");
  for(e=0;e<(int)p->num_identifier_references;e++)
  {
    struct inherit *inh = INHERIT_FROM_INT(p,e);
    fprintf(stderr,"%3d: ",e);
    for(d=0;d<inh->inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"%-40s  flags 0x%x",ID_FROM_INT(p,e)->name->str,
	    p->identifier_references[e].id_flags);
    for (q = 0; q < p->num_inherits; q++)
      if (p->inherits + q == inh) {
	fprintf(stderr,
		"  inherit %d\n"
		"       type: ",
		q);
	simple_describe_type(ID_FROM_INT(p, e)->type);
	fprintf(stderr, "\n");
	goto inherit_found;
      }
    fprintf (stderr, "  inherit not found!\n");
  inherit_found:;
  }
  fprintf(stderr,"All sorted identifiers:\n");
  for(q=0;q<(int)p->num_identifier_index;q++)
  {
    e=p->identifier_index[q];
    fprintf(stderr,"%3d (%3d):",e,q);
    for(d=0;d<INHERIT_FROM_INT(p,e)->inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"%s;\n", ID_FROM_INT(p,e)->name->str);
  }

  fprintf(stderr,"$$$$$ dump_program_desc for %p done\n", p);
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
    case PIKE_T_FREE:
    case PIKE_T_GET_SET: return 0;
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
    case PIKE_T_FREE:
    case PIKE_T_GET_SET: return 1;
    default: return ALIGNOF(void *);
  }
}

#ifdef PIKE_DEBUG

void dump_program_tables (const struct program *p, int indent)
{
  int d;

  if (!p) {
    fprintf(stderr, "%*sProgram: NULL\n\n", indent, "");
    return;
  }

  fprintf(stderr,
	  "%*sProgram flags: 0x%04x\n\n",
	  indent, "", p->flags);

  fprintf(stderr,
	  "%*sReference table:\n"
	  "%*s  ####: Flags Inherit Identifier\n",
	  indent, "", indent, "");
  for (d=0; d < p->num_identifier_references; d++) {
    struct reference *ref = p->identifier_references + d;
    struct identifier *id = ID_FROM_PTR(p, ref);

    fprintf(stderr,
	    "%*s  %4d: %5x %7d %10d  %s\n"
	    "%*s        %s:%d\n",
	    indent, "",
	    d, ref->id_flags, ref->inherit_offset,
	    ref->identifier_offset,
	    id->name->size_shift ? "(wide)" : id->name->str,
	    indent, "",
	    p->num_strings?p->strings[id->filename_strno]->str:"-", id->linenumber);
    if (IDENTIFIER_IS_ALIAS(id->identifier_flags)) {
      fprintf (stderr, "%*s                                  Alias for %d:%d\n",
	       indent, "", id->func.ext_ref.depth, id->func.ext_ref.id);
    } else if (IDENTIFIER_IS_CONSTANT(id->identifier_flags)) {
      fprintf (stderr, "%*s                                  Constant #%ld\n",
	       indent, "", (long)id->func.offset);
    } else if (IDENTIFIER_IS_VARIABLE(id->identifier_flags)) {
      fprintf (stderr, "%*s                                  Offset: 0x%08lx\n",
	       indent, "", (long)id->func.offset);
    } else if (IDENTIFIER_IS_PIKE_FUNCTION(id->identifier_flags)) {
      INT32 line;
      struct program *inh_p = INHERIT_FROM_PTR(p,ref)->prog;
      struct pike_string *file =
	get_line (ID_FROM_PTR(p,ref)->func.offset + inh_p->program, inh_p, &line);
      if (!file->size_shift)
	fprintf (stderr, "%*s                                  %s:%d\n",
		 indent, "", file->str, line);
      free_string (file);
    } else {
      fprintf (stderr, "%*s                                  Cfun: %p\n",
	       indent, "", id->func.c_fun);
    }
  }

  fprintf(stderr, "\n"
	  "%*sIdentifier index table:\n"
	  "%*s  ####: Index\tName\n",
	  indent, "", indent, "");
  for (d = 0; d < p->num_identifier_index; d++) {
    struct identifier *id = ID_FROM_INT(p, p->identifier_index[d]);
    fprintf(stderr, "%*s  %4d: %5d\t%s\n",
	    indent, "",
	    d, p->identifier_index[d],
	    id->name->size_shift ? "(wide)" : id->name->str);
  }

  fprintf(stderr, "\n"
	  "%*sInherit table:\n"
	  "%*s  ####: Level prog_id id_level storage_offs "
	  "par_id par_offs par_obj_id id_ref_offs\n",
	  indent, "", indent, "");
  for (d=0; d < p->num_inherits; d++) {
    struct inherit *inh = p->inherits + d;

    fprintf(stderr, "%*s  %4d: %5d %7d %8d %12"PRINTPTRDIFFT"d %6d %8d %10d %11"PRINTSIZET"d\n",
	    indent, "",
	    d, inh->inherit_level,
	    inh->prog ? inh->prog->id : -1,
	    inh->identifier_level, inh->storage_offset,
	    inh->parent_identifier, inh->parent_offset,
	    inh->parent ? inh->parent->program_id : -1,
	    inh->identifier_ref_offset);
  }
  fprintf(stderr, "\n"
	  "%*sIdentifier table:\n"
	  "%*s  ####: Flags Offset Type Name\n",
	  indent, "", indent, "");
  for (d=0; d < p->num_identifiers; d++) {
    struct identifier *id = p->identifiers + d;

    fprintf(stderr,
	    "%*s  %4d: %5x %6"PRINTPTRDIFFT"d %4d \"%s\"\n"
	    "%*s        %s:%d\n",
	    indent, "",
	    d, id->identifier_flags, id->func.offset,
	    id->run_time_type, id->name->str,
	    indent, "",
	    p->num_strings?p->strings[id->filename_strno]->str:"-", id->linenumber);
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
	  "%*s  ####: Type            Raw\n",
	  indent, "", indent, "");
  for (d = 0; d < p->num_constants; d++) {
    struct program_constant *c = p->constants + d;
#if 1
    fprintf(stderr, "%*s  %4d: %-15s %p\n",
	    indent, "",
	    d, get_name_of_type (c->sval.type),
	    c->sval.u.ptr);
#else /* !0 */
    fprintf(stderr, "%*s  %4d: %-15s %"PRINTPTRDIFFT"d\n",
	    indent, "",
	    d, get_name_of_type (c->sval.type),
	    c->offset);
#endif /* 0 */
  }

  fprintf(stderr, "\n"
	  "%*sString table:\n"
	  "%*s  ####: Value\n",
	  indent, "", indent, "");
  for (d = 0; d < p->num_strings; d++) {
    fprintf(stderr, "%*s  %4d: [%p]\"%s\"(%"PRINTPTRDIFFT"d characters)\n",
	    indent, "", (int)d, p->strings[d], p->strings[d]->str, p->strings[d]->len);
  }

  fprintf(stderr, "\n"
	  "%*sLFUN table:\n"
	  "%*s  LFUN  Ref# Name\n",
	  indent, "", indent, "");
  for (d = 0; d < NUM_LFUNS; d++) {
    if (p->lfuns[d] != -1) {
      fprintf(stderr, "%*s  %4d: %04d %s\n",
	      indent, "", d, p->lfuns[d], lfun_names[d]);
    }
  }

  fprintf(stderr, "\n"
	  "%*sLinenumber table:\n",
	  indent, "");
  {
    INT32 off = 0, line = 0;
    char *cnt = p->linenumbers;

    while (cnt < p->linenumbers + p->num_linenumbers) {
      if (*cnt == 127) {
	int len, shift;
	char *file;
	cnt++;
	len = get_small_number(&cnt);
	shift = *cnt;
	file = ++cnt;
	CHECK_FILE_ENTRY (p, cnt, len, shift);
	cnt += len << shift;
	if (!shift) {
	  fprintf(stderr, "%*s  Filename: \"%s\"\n", indent, "", file);
	} else {
	  fprintf(stderr, "%*s  Filename: len:%d, shift:%d\n", indent, "", len, shift);
	}
      }
      off += get_small_number(&cnt);
      line += get_small_number(&cnt);
      fprintf(stderr, "%*s    %8d:%8d\n", indent, "", off, line);
    }
  }

  fprintf(stderr, "\n");
}

void check_program(struct program *p)
{
  unsigned INT32 e;
  int variable_positions[1024];

  if(p->flags & PROGRAM_AVOID_CHECK) return;

  for(e=0;e<NELEM(variable_positions);e++)
    variable_positions[e]=-1;

  if(p->id > current_program_id)
    Pike_fatal("Program id is out of sync! (p->id=%d, current_program_id=%d)\n",p->id,current_program_id);

  if(p->refs <=0)
    Pike_fatal("Program has zero refs.\n");

  if(p->next && p->next->prev != p)
    Pike_fatal("Program ->next->prev != program.\n");

  if(p->prev)
  {
    if(p->prev->next != p)
      Pike_fatal("Program ->prev->next != program.\n");
  }else{
    if(first_program != p)
      Pike_fatal("Program ->prev == 0 but first_program != program.\n");
  }

  if(p->id > current_program_id || p->id <= 0)
    Pike_fatal("Program id is wrong.\n");

  if(p->storage_needed < 0)
    Pike_fatal("Program->storage_needed < 0.\n");

  if(p->num_identifier_index > p->num_identifier_references)
    Pike_fatal("Too many identifier index entries in program!\n");

  for(e=0;e<p->num_constants;e++)
  {
    struct svalue *s = & p->constants[e].sval;
    check_svalue(s);
    if (p->flags & PROGRAM_FINISHED && s->type == T_OBJECT &&
	s->u.object->next == s->u.object)
      Pike_fatal ("Got fake object in constant in finished program.\n");
#if 0
    if(p->constants[e].name) check_string(p->constants[e].name);
#else /* ! 0 */
    if (p->constants[e].offset >= p->num_identifiers) {
      Pike_fatal("Constant initializer outside num_identifiers (%d >= %d).\n",
		 p->constants[e].offset, p->num_identifiers);
    }
#endif /* 0 */
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
      Pike_fatal("Inherit->storage_offset is wrong.\n");

    if(p->inherits[e].prog &&
       p->inherits[e].storage_offset + STORAGE_NEEDED(p->inherits[e].prog) >
       p->storage_needed)
      Pike_fatal("Not enough room allocated by inherit!\n");

    if (p->inherits[e].inherit_level == 1 &&
	p->inherits[e].identifier_level != (INT32) p->inherits[e].identifier_ref_offset) {
      dump_program_tables (p, 0);
      Pike_fatal ("Unexpected difference between identifier_level "
		  "and identifier_ref_offset in inherit %d.\n", e);
    }

    if(e)
    {
      if(p->inherits[e-1].storage_offset >
	 p->inherits[e].storage_offset)
	Pike_fatal("Overlapping inherits! (1)\n");

      if(p->inherits[e-1].prog &&
	 p->inherits[e-1].inherit_level >= p->inherits[e].inherit_level &&
	 ( p->inherits[e-1].storage_offset +
	   STORAGE_NEEDED(p->inherits[e-1].prog)) >
	 p->inherits[e].storage_offset)
	Pike_fatal("Overlapping inherits! (3)\n");
    }
  }


  if(p->flags & PROGRAM_FINISHED)
  for(e=0;e<p->num_identifiers;e++)
  {
    check_string(p->identifiers[e].name);
    check_type_string(p->identifiers[e].type);

    switch (p->identifiers[e].identifier_flags & IDENTIFIER_TYPE_MASK) {
      case IDENTIFIER_VARIABLE:
      case IDENTIFIER_PIKE_FUNCTION:
      case IDENTIFIER_C_FUNCTION:
      case IDENTIFIER_CONSTANT:
	break;

      default:
	Pike_fatal("Invalid identifier type.\n");
    }

    if(p->identifiers[e].identifier_flags & ~IDENTIFIER_MASK)
      Pike_fatal("Unknown flags in identifier flag field.\n");

    if((p->identifiers[e].run_time_type!=T_MIXED) &&
       (p->identifiers[e].run_time_type!=PIKE_T_FREE) &&
       (p->identifiers[e].run_time_type!=PIKE_T_GET_SET))
      check_type(p->identifiers[e].run_time_type);

    if (!IDENTIFIER_IS_ALIAS(p->identifiers[e].identifier_flags)) {
      if(IDENTIFIER_IS_VARIABLE(p->identifiers[e].identifier_flags))
      {
	if((p->identifiers[e].func.offset /* + OFFSETOF(object,storage)*/ ) &
	    (alignof_variable(p->identifiers[e].run_time_type)-1))
	{
	  dump_program_tables(p, 0);
	  Pike_fatal("Variable %s offset is not properly aligned (%ld).\n",
		     p->identifiers[e].name->str,
		     PTRDIFF_T_TO_LONG(p->identifiers[e].func.offset));
	}
      }
    } else {
      /* FIXME: Check that ext_ref.depth and ext_ref.id are valid and
       *        have matching identifier_flags.
       */
      if (p->identifiers[e].func.ext_ref.depth &&
	  !(p->flags & PROGRAM_USES_PARENT)) {
	Pike_fatal("Identifier %d is an external reference, but "
		   "PROGRAM_USES_PARENT hasn't been set.\n",
		   e);
      }
    }
  }

  for(e=0;e<p->num_identifier_references;e++)
  {
    struct identifier *i;
    if(p->identifier_references[e].inherit_offset > p->num_inherits)
      Pike_fatal("Inherit offset is wrong!\n");

    if(!p->inherits[p->identifier_references[e].inherit_offset].prog)
    {
      if(!(p->flags & PROGRAM_FINISHED))
	continue;

      Pike_fatal("p->inherit[%d].prog = NULL!\n",p->identifier_references[e].inherit_offset);
    }

    if(p->identifier_references[e].identifier_offset >
       p->inherits[p->identifier_references[e].inherit_offset].prog->num_identifiers)
      Pike_fatal("Identifier offset %d is wrong! %d > %d\n",
	    e,
	    p->identifier_references[e].identifier_offset,
	    p->inherits[p->identifier_references[e].inherit_offset].prog->num_identifiers);

    i=ID_FROM_INT(p, e);

    if (IDENTIFIER_IS_ALIAS(i->identifier_flags)) {
      if ((!i->func.ext_ref.depth) && (i->func.ext_ref.id == e)) {
	dump_program_tables(p, 0);
	Pike_fatal("Circular alias for reference %d!\n", e);
      }
    } else if (IDENTIFIER_IS_VARIABLE(i->identifier_flags)) {
      size_t q, size;
      /* Variable */
      ptrdiff_t offset = INHERIT_FROM_INT(p, e)->storage_offset+i->func.offset;
      if (i->run_time_type == PIKE_T_GET_SET) {
	struct reference *ref = PTR_FROM_INT(p, e);
	if (!ref->inherit_offset) {
	  if (i->func.gs_info.getter >= p->num_identifier_references) {
	    Pike_fatal("Getter outside references.\n");
	  }
	  if (i->func.gs_info.setter >= p->num_identifier_references) {
	    Pike_fatal("Setter outside references.\n");
	  }
	}
	continue;
      }
      size=sizeof_variable(i->run_time_type);

      if(size && ((offset+size > (size_t)p->storage_needed) || offset<0))
	Pike_fatal("Variable outside storage! (%s)\n",i->name->str);

      for(q=0;q<size;q++)
      {
	if(offset+q >= NELEM(variable_positions)) break;

	if(variable_positions[offset+q] != -1)
	{
	  if(ID_FROM_INT(p,variable_positions[offset+q])->run_time_type !=
	     i->run_time_type)
	  {
	    fprintf(stderr, "Storage offset: "
		    "0x%08"PRINTPTRDIFFT"x vs 0x%08"PRINTPTRDIFFT"x\n"
		    "Func offset: 0x%08"PRINTPTRDIFFT"x vs 0x%08"PRINTPTRDIFFT"x\n"
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
	      Pike_fatal("Variable '%s' and '%s' overlap\n"
			 "Offset 0x%08"PRINTPTRDIFFT"x - 0x%08"PRINTPTRDIFFT"x "
			 "overlaps with "
			 "0x%08"PRINTPTRDIFFT"x - 0x%08"PRINTPTRDIFFT"x\n",
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
	      Pike_fatal("Variable '%s' and anonymous variable (%d) overlap\n"
			 "Offset 0x%08"PRINTPTRDIFFT"x - 0x%08"PRINTPTRDIFFT"x "
			 "overlaps with "
			 "0x%08"PRINTPTRDIFFT"x - 0x%08"PRINTPTRDIFFT"x\n",
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
      Pike_fatal("Program->identifier_indexes[%ld] is wrong\n",(long)e);
    if (e && (program_identifier_index_compare(p->identifier_index[e-1],
					       p->identifier_index[e],
					       p) > 0)) {
      Pike_fatal("Program->identifier_index[%ld] > "
		 "Program->identifier_index[%ld]\n",
		 (long)(e-1), (long)e);
    }
  }

}
#endif

/* Note: This function is misnamed, since it's run after both passes. /mast */
/* finish-states:
 *
 *   0: First pass.
 *   1: Last pass.
 *   2: Called from decode_value().
 */
struct program *end_first_pass(int finish)
{
  struct compilation *c = THIS_COMPILATION;
  int e;
  struct program *prog;
  struct pike_string *s;

  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);

  MAKE_CONST_STRING(s,"__INIT");


  /* Collect references to inherited __INIT functions */
  if (!(Pike_compiler->new_program->flags & PROGRAM_AVOID_CHECK)) {
    for(e=Pike_compiler->new_program->num_inherits-1;e;e--)
    {
      int id;
      if(Pike_compiler->new_program->inherits[e].inherit_level!=1) continue;
      id=low_reference_inherited_identifier(0, e, s, SEE_PROTECTED);
      if(id!=-1)
      {
	Pike_compiler->init_node=mknode(F_COMMA_EXPR,
		         mkcastnode(void_type_string,
				    mkapplynode(mkidentifiernode(id),0)),
		         Pike_compiler->init_node);
      }
    }
  }

  /*
   * Define the __INIT function, but only if there was any code
   * to initialize.
   */

  if(Pike_compiler->init_node)
  {
    Pike_compiler->compiler_frame->current_function_number = -2;
    e=dooptcode(s,
		mknode(F_COMMA_EXPR,
		       Pike_compiler->init_node,
		       mknode(F_RETURN,mkintnode(0),0)),
		function_type_string,
		ID_PROTECTED);
    Pike_compiler->init_node=0;
  } else if (finish == 2) {
    /* Called from decode_value(). */
    e = low_find_lfun(Pike_compiler->new_program, LFUN___INIT);
  }else{
    e=-1;
  }
  Pike_compiler->new_program->lfuns[LFUN___INIT]=e;

  pop_compiler_frame(); /* Pop __INIT local variables */

  if(Pike_compiler->num_parse_error > 0)
  {
    CDFPRINTF((stderr, "th(%ld) %p Compilation errors (%d).\n",
	       (long)th_self(), Pike_compiler->new_program,
	       Pike_compiler->num_parse_error));
    prog=0;
  }else{
    prog=Pike_compiler->new_program;
    add_ref(prog);

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

#ifdef PIKE_DEBUG
    if (prog) {
      check_program(prog);
      if(l_flag)
	dump_program_desc(prog);
    }
#endif
  }

#ifdef PROGRAM_BUILD_DEBUG
  fprintf (stderr, "%.*sfinishing program %d (pass=%d)\n",
	   c->compilation_depth, "",
	   Pike_compiler->new_program->id, Pike_compiler->compiler_pass);
#endif

  toss_compilation_resources();

#if 0
  CDFPRINTF((stderr,
	     "th(%ld) end_first_pass(): "
	     "%p compilation_depth:%d, Pike_compiler->compiler_pass:%d\n",
	     (long)th_self(), prog,
	     c->compilation_depth, Pike_compiler->compiler_pass));
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

  free_all_nodes();

  CDFPRINTF((stderr,
	     "th(%ld) %p end_first_pass(%d): "
	     "threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), prog, finish,
	     threads_disabled, c->compilation_depth));

  c->compilation_depth--;

  exit_threads_disable(NULL);

#if 0
#ifdef PIKE_USE_MACHINE_CODE
  if (prog &&
      (((unsigned long long *)prog->program)[-1] != 0xdeadfeedf00dfaddLL)) {
    Pike_fatal("Bad mexec magic!\n");
  }
#endif /* PIKE_USE_MACHINE_CODE */
#endif /* 0 */
  return prog;
}

/*
 * Finish this program, returning the newly built program
 */
PMOD_EXPORT struct program *debug_end_program(void)
{
  if (!Pike_compiler->new_program->num_identifier_references &&
      Pike_compiler->new_program->event_handler) {
    /* Program with no symbols, but with event handler.
     * eg _disable_threads.
     * Add a symbol to avoid call_c_initializers() creating broken frames.
     * Workaround for [bug 6156].
     */
    add_integer_constant("__EVENT_HANDLER__", 0,
			 ID_PRIVATE|ID_PROTECTED|ID_USED);
  }

  Pike_compiler->compiler_pass = 2;
  return end_first_pass(1);
}


/*
 * Allocate space needed for this program in the object structure.
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
    Pike_fatal("Alignment must be 1,2,4,8,16,32,64,128 or 256 not %ld\n",
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
    Pike_fatal("add_storage failed horribly!\n");

  if( (offset /* + OFFSETOF(object,storage) */ - modulo_orig ) % alignment )
    Pike_fatal("add_storage failed horribly(2) %ld %ld %ld %ld!\n",
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
  handler=((oldhandlertype *)Pike_fp->context->prog->program)[e];
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
 * Set a callback to be called when this program is cloned.
 *
 * This function is obsolete; see pike_set_prog_event_callback for
 * details.
 */
PMOD_EXPORT void set_init_callback(void (*init)(struct object *))
{
  add_compat_event_handler();
  ((oldhandlertype *)Pike_compiler->new_program->program)[PROG_EVENT_INIT]=init;
}

/*
 * Set a callback to be called when clones of this program are
 * destructed.
 *
 * This function is obsolete; see pike_set_prog_event_callback for
 * details.
 *
 * Note: If the callback only does very trivial stuff, like freeing or
 * clearing a few pointers in the object storage, you can do
 *
 *   Pike_compiler->new_program->flags &= ~PROGRAM_LIVE_OBJ;
 *
 * after the set_exit_callback call. That allows the gc to operate
 * more efficiently, but the effect is that the callback might be
 * called out of order for PROG_EVENT_EXIT events; see the docs for
 * PROGRAM_LIVE_OBJ in program.h for further details.
 */
PMOD_EXPORT void set_exit_callback(void (*exit)(struct object *))
{
  add_compat_event_handler();
  ((oldhandlertype *)Pike_compiler->new_program->program)[PROG_EVENT_EXIT]=exit;
  Pike_compiler->new_program->flags |= PROGRAM_LIVE_OBJ;
}

/*
 * This callback is used by the gc to traverse all references to
 * things in memory. It should call some gc_recurse_* function exactly
 * once for each reference that the pike internals doesn't know about.
 *
 * If a reference is shared between objects, it should be traversed
 * once for every instance sharing it.
 *
 * The callback might be called more than once for the same instance
 * during a gc pass. The gc assumes that the references are enumerated
 * in the same order in that case.
 *
 * The callback is called after any mapped variables on the object
 * have been recursed (and possibly freed).
 *
 * If there are pointers to allocated memory that you keep track of on
 * the C level then you should add something like this to the recurse
 * callback so that Pike.count_memory remains accurate:
 *
 *   if (mc_count_bytes (Pike_fp->current_object))
 *     mc_counted_bytes += <size of the allocated memory block(s)>
 *
 * If the allocated memory is shared between objects then it gets more
 * complicated and you need to write visit_thing_fn callbacks. See
 * e.g. visit_mapping and visit_mapping_data for how to do that.
 *
 * This function is obsolete; see pike_set_prog_event_callback for
 * details.
 */
PMOD_EXPORT void set_gc_recurse_callback(void (*m)(struct object *))
{
  add_compat_event_handler();
  ((oldhandlertype *)Pike_compiler->new_program->program)[PROG_EVENT_GC_RECURSE]=m;
}

/*
 * This callback is used by the gc to count all references to things
 * in memory. It should call gc_check, gc_check_(weak_)svalues or
 * gc_check_(weak_)short_svalue exactly once for each reference that
 * the pike internals don't know about.
 *
 * If a reference is shared between objects, it should be counted once
 * for all shared instances. The return value from gc_check is useful
 * to ensure this; it's zero when called the first time for its
 * argument (it is perfectly fine to use gc_check on things that the
 * pike core doesn't know about, but they must have an INT32 refcount
 * first).
 *
 * This function is obsolete; see pike_set_prog_event_callback for
 * details.
 */
PMOD_EXPORT void set_gc_check_callback(void (*m)(struct object *))
{
  add_compat_event_handler();
  ((oldhandlertype *)Pike_compiler->new_program->program)[PROG_EVENT_GC_CHECK]=m;
}

/*
 * Set a callback to be called when any of the special program events
 * occur. The event type is sent as an integer argument. The events
 * include, but might not be limited to, the following:
 *
 * PROG_EVENT_INIT
 *   An object is being cloned from the program. See set_init_callback
 *   for details.
 * PROG_EVENT_EXIT
 *   An object is being destructed. See set_exit_callback for details.
 * PROG_EVENT_GC_RECURSE
 *   An object is being recursed by the gc. See
 *   set_gc_recurse_callback for details.
 * PROG_EVENT_GC_CHECK
 *   An object is being checked by the gc. See set_gc_check_callback
 *   for details.
 *
 * Note that installing an event callback will set the
 * PROGRAM_LIVE_OBJ flag since the callback might act on the
 * PROG_EVENT_EXIT event. If the callback won't do anything for that
 * event (or if it only does something very trivial for it), you
 * should do
 *
 *   Pike_compiler->new_program->flags &= ~PROGRAM_LIVE_OBJ;
 *
 * afterwards to clear it again. That allows the gc to operate more
 * efficiently, but the effect is that the callback might be called
 * out of order for PROG_EVENT_EXIT events; see the docs for
 * PROGRAM_LIVE_OBJ in program.h for further details.
 */
PMOD_EXPORT void pike_set_prog_event_callback(void (*cb)(int))
{
#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->event_handler)
    Pike_fatal("Program already has an event handler!\n");
#endif
  Pike_compiler->new_program->event_handler=cb;
  Pike_compiler->new_program->flags |= PROGRAM_LIVE_OBJ;
}

PMOD_EXPORT void pike_set_prog_optimize_callback(node *(*opt)(node *))
{
#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->optimize)
    Pike_fatal("Program already has an optimize handler!\n");
#endif
  Pike_compiler->new_program->optimize = opt;
}

int really_low_reference_inherited_identifier(struct program_state *q,
					      int e,
					      int i)
{
  struct program *np=(q?q:Pike_compiler)->new_program;
  struct reference funp;
  struct program *p;
  int d, num_id_refs;

  if(i==-1) return -1;

  p = np->inherits[e].prog;

  funp = p->identifier_references[i];
  funp.inherit_offset += e;
  funp.id_flags = (funp.id_flags & ~ID_INHERITED) | ID_INLINE|ID_HIDDEN;

  num_id_refs = np->num_identifier_references;

  for(d = 0; d < num_id_refs; d++)
  {
    struct reference *refp;
    refp = np->identifier_references + d;

    if ((refp->inherit_offset == funp.inherit_offset) &&
	(refp->identifier_offset == funp.identifier_offset) &&
	((refp->id_flags | ID_USED) == (funp.id_flags | ID_USED)))
      return d;
  }

  if(q)
    low_add_to_identifier_references(q,funp);
  else
    add_to_identifier_references(funp);
  /* NOTE: np->num_identifier_references has been increased by one by
   *       {low_,}add_to_identifier_references().
   */
#ifdef PIKE_DEBUG
  if (num_id_refs != np->num_identifier_references-1) {
    fatal("Unexpected number of identifier references: %d != %d\n",
	  num_id_refs, np->num_identifier_references-1);
  }
#endif /* PIKE_DEBUG */
  return num_id_refs; /* aka np->num_identifier_references - 1 */
}

int low_reference_inherited_identifier(struct program_state *q,
				       int e,
				       struct pike_string *name,
				       int flags)
{
  struct program *np=(q?q:Pike_compiler)->new_program;
  struct program *p;
  int i;

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

  return really_low_reference_inherited_identifier(q, e, i);
}

int find_inherit(struct program *p, struct pike_string *name)
{
  int e;

#if 0
  fprintf(stderr, "find_inherit(0x%08lx, \"%s\")...\n",
	  (unsigned long)p, name->str);
#endif /* 0 */
  /* FIXME: This loop could be optimized by advancing by the number
   *        of inherits in the inherit. But in that case the loop
   *        would have to go the other way.
   */
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

/* Reference the symbol super_name::function_name */
node *reference_inherited_identifier(struct pike_string *super_name,
				     struct pike_string *function_name)
{
  int n,e,id;
  struct compilation *c = THIS_COMPILATION;
  struct program_state *state=Pike_compiler->previous;

  struct program *p;


#ifdef PIKE_DEBUG
  if(function_name!=debug_findstring(function_name))
    Pike_fatal("reference_inherited_function on nonshared string.\n");
#endif

  p=Pike_compiler->new_program;

  /* FIXME: This loop could be optimized by advancing by the number
   *        of inherits in the inherit. But in that case the loop
   *        would have to go the other way.
   */
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
					  SEE_PROTECTED);

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


  for(n=0;n<c->compilation_depth;n++,state=state->previous)
  {
    struct program *p=state->new_program;

    /* FIXME: This loop could be optimized by advancing by the number
     *        of inherits in the inherit. But in that case the loop
     *        would have to go the other way.
     */
    for(e=p->num_inherits-1;e>0;e--)
    {
      if(p->inherits[e].inherit_level!=1) continue;
      if(!p->inherits[e].name) continue;

      if(super_name)
	if(super_name != p->inherits[e].name)
	  continue;

      id=low_reference_inherited_identifier(state,e,function_name,SEE_PROTECTED);

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

/* FIXME: This function probably doesn't do what it is intended to do
 *        if the last inherit had inherits of its own. Consider removal.
 */
void rename_last_inherit(struct pike_string *n)
{
  if(Pike_compiler->new_program->inherits[Pike_compiler->new_program->num_inherits].name)
    free_string(Pike_compiler->new_program->inherits[Pike_compiler->new_program->num_inherits].name);
  copy_shared_string(Pike_compiler->new_program->inherits[Pike_compiler->new_program->num_inherits].name,
		     n);
}

#if 0
static int locate_parent_state(struct program_state **state,
			       struct inherit **i,
			       int *parent_identifier,
			       int depth)
{
  int result=1;
  if(depth<=0) return depth;
  while(depth-->0)
  {
    if( (*i)->parent_offset != INHERIT_PARENT)
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
#endif

#ifdef WITH_FACETS
void check_for_facet_inherit(struct program *p)
{
  /* If the inherit statement comes before the facet keyword in the
   * class declaration the class will be temporarily marked as a
   * product-class, but this will be taken care of when the facet
   * keyword is found. */
  if (!p) return;
  if (Pike_compiler->new_program->facet_group &&
      p->facet_group != Pike_compiler->new_program->facet_group)
    yyerror("A class can not belong to two facet-groups.");
  if (p->flags & PROGRAM_IS_FACET) {
    if (Pike_compiler->new_program->flags & PROGRAM_IS_FACET) {
      if(Pike_compiler->new_program->facet_index != p->facet_index)
	yyerror("Facet class can't inherit from class in different facet.");
    }
    /* Otherwise this is a product class */
    else {
      if( !Pike_compiler->new_program->facet_group ) {
	Pike_compiler->new_program->flags |= PROGRAM_IS_PRODUCT;
	add_ref(p->facet_group);
	Pike_compiler->new_program->facet_group = p->facet_group;
      }
      push_int(Pike_compiler->new_program->id);
      push_int(p->facet_index);
      push_int(p->id);
      safe_apply(p->facet_group, "add_product_class", 3);
      pop_stack();
    }
  }
  /* The inherited class is not a facet class */
  else if (p->flags & PROGRAM_IS_PRODUCT) {
    if (Pike_compiler->new_program->flags & PROGRAM_IS_FACET) {
      yyerror("Facet class can't inherit from product class.");
    }
    else if(Pike_compiler->new_program->flags & PROGRAM_IS_PRODUCT){
      yyerror("Product class can't inherit from other product class.");
    }
    /* A class that inherits from a product class is also a product class */
    else {
      Pike_compiler->new_program->flags |= PROGRAM_IS_PRODUCT;
      add_ref(p->facet_group);
      Pike_compiler->new_program->facet_group = p->facet_group;
    }
  }
}
#endif


/*
 * make this program inherit another program
 */
PMOD_EXPORT void low_inherit(struct program *p,
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

  if (Pike_compiler->compiler_pass == 2) {
    struct program *old_p =
      Pike_compiler->new_program->inherits[Pike_compiler->num_inherits+1].prog;
    Pike_compiler->num_inherits += old_p->num_inherits;

    if (old_p != p) {
      yyerror("Got different program for inherit in second pass "
	      "(resolver problem).");
    }

    if (!(p->flags & PROGRAM_FINISHED)) {
      /* Require that the inherited program really is finished in pass
       * 2. Otherwise we might not have all definitions when we
       * fixate our program.
       *
       * FIXME: Maybe this can be relaxed by registering a dependency
       * and delaying compilation here?
       */
      yyerror ("Cannot inherit program in pass 2 "
	       "which is not fully compiled yet.");
      yyerror ("(You probably have a cyclic symbol dependency that the "
	       "compiler cannot handle.)");
    }


    if(p->flags & PROGRAM_NEEDS_PARENT)
    {
      struct program_state *state=Pike_compiler;

      if(!parent && !parent_offset)
      {
	yyerror("Parent pointer lost, cannot inherit!");
	/* We've inherited it anyway, to avoid causing more errors */
      }

#if 0
      /* FIXME: we don't really need to set this flag on ALL
       * previous compilations, but I'm too lazy to figure out
       * exactly how deep down we need to go...
       */
      for(e=0;e<c->compilation_depth;e++,state=state->previous)
	state->new_program->flags |= PROGRAM_USES_PARENT;
#endif
    }

    return;
  }

  if (!(p->flags & (PROGRAM_FINISHED | PROGRAM_PASS_1_DONE))) {
    yyerror ("Cannot inherit program in pass 1 "
	     "which is only a placeholder.");
    yyerror ("(You probably have a cyclic symbol dependency that the "
	     "compiler cannot handle.)");
    return;
  }

#ifdef WITH_FACETS
  /* Check if inherit is a facet inherit. */
  check_for_facet_inherit(p);
#endif

  if (p == placeholder_program) {
    yyerror("Trying to inherit placeholder program (resolver problem).");
    return;
  }

 /* parent offset was increased by 42 for above test.. */
  if(parent_offset)
    parent_offset-=42;

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
	    if(!o) Pike_fatal("low_inherit with odd Pike_compiler->fake_object as parent!\n");
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
	      Pike_fatal("low_inherit with odd Pike_compiler->fake_object as parent!\n");
#endif
	    inherit.parent_offset++;
	  }
#endif
	}else{
	  inherit.parent=parent;
	  inherit.parent_identifier=parent_identifier;
	  inherit.parent_offset=INHERIT_PARENT;
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
	  struct object *par=parent;
	  int e,pid=parent_identifier;

	  for(e=1;e<inherit.parent_offset;e++)
	  {
	    struct inherit *in;
	    if(!par || !par->prog)
	    {
	      par = NULL;
	      pid = -1;
	      break;
	    }

	    in=INHERIT_FROM_INT(par->prog, pid);
	    switch(in->parent_offset)
	    {
	      default:
	      {
		struct external_variable_context tmp;
		struct inherit *in2=in;
		while(in2->inherit_level >= in->inherit_level) in2--;
		tmp.o=par;
		tmp.inherit=in2;
		tmp.parent_identifier=pid;
		find_external_context(&tmp, in->parent_offset);
		par = tmp.o;
		pid = tmp.parent_identifier;
	      }
	      break;

	      case INHERIT_PARENT:
		pid = in->parent_identifier;
		par = in->parent;
		break;

	      case OBJECT_PARENT:
		/* Ponder: Can we be sure that PROGRAM_USES_PARENT
		 * doesn't get set later? /mast */
		if(par->prog->flags & PROGRAM_USES_PARENT)
		{
		  pid = PARENT_INFO(par)->parent_identifier;
		  par = PARENT_INFO(par)->parent;
		}else{
		  pid = -1;
		  par = NULL;
		}
	    }
	  }

	  inherit.parent=par;
	  inherit.parent_offset=INHERIT_PARENT;
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
	/* FIXME: Wide string handling. */
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

  /* This value is used by encode_value() to reverse the inherit operation. */
  Pike_compiler->new_program->inherits[inherit_offset].identifier_ref_offset =
    Pike_compiler->new_program->num_identifier_references;

  for (e=0; e < (int)p->num_identifier_references; e++)
  {
    struct reference fun;

    fun = p->identifier_references[e]; /* Make a copy */

    fun.inherit_offset += inherit_offset;

    if (fun.id_flags & ID_FINAL)
    {
      Pike_compiler->flags |= COMPILATION_CHECK_FINAL;
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

      /* FIXME: Support external constants. */
      if(numid != IDREF_MAGIC_THIS &&
	 (IDENTIFIER_IS_CONSTANT((i=ID_FROM_INT(p, numid))->
				 identifier_flags)) &&
	 (i->func.offset != -1))
      {
	struct svalue *s=&PROG_FROM_INT(p, numid)->
	  constants[i->func.offset].sval;
	if(s->type != T_PROGRAM)
	{
	  do_inherit(s,flags,name);
	  return;
	}else{
	  low_inherit(s->u.program,
		      0,
		      numid,
		      offset+42,
		      flags,
		      name);
	}
	if (n->token == F_EXTERNAL) {
	  struct program *p=program_from_svalue(s);
	  if (p->flags & (PROGRAM_NEEDS_PARENT|PROGRAM_NEEDS_PARENT)) {
	    /* We'll need the parent pointer as well... */
	    struct program_state *state = Pike_compiler;

	    while (state && (state->new_program->id != n->u.integer.a)) {
	      state->new_program->flags |= PROGRAM_NEEDS_PARENT|PROGRAM_USES_PARENT;
	      state = state->previous;
	    }
	  }
	}
      }else{
	yyerror("Inherit identifier is not a constant program");
	return;
      }
      break;

    default:
      resolv_class(n);
      do_inherit(Pike_sp-1, flags, name);
      pop_stack();
  }
}

int call_handle_inherit(struct pike_string *s)
{
  struct compilation *c = THIS_COMPILATION;

  CHECK_COMPILER();

  ref_push_string(s);
  if (!TEST_COMPAT(7,6)) {
    /* In Pike 7.7 and later filenames belonging to Pike are assumed
     * to be encoded according to UTF-8.
     */
    f_string_to_utf8(1);
  }

  if (safe_apply_current2(PC_HANDLE_INHERIT_FUN_NUM, 1, NULL))
    if (Pike_sp[-1].type != T_INT)
      return 1;
    else {
      my_yyerror("Couldn't find program %S", s);
    }
  else {
    handle_compile_exception ("Error finding program");
  }
  pop_stack();

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
  return really_low_find_shared_string_identifier(s,
						  Pike_compiler->new_program,
						  SEE_PROTECTED|SEE_PRIVATE);
}

/*
 * Definition of identifiers.
 *
 * Pike has three plus one classes of identifiers:
 *
 *   IDENTIFIER_VARIABLE - A value stored in object->storage
 *   IDENTIFIER_CONSTANT - A value stored in program->constants
 *   IDENTIFIER_FUNCTION - Either a C function or a Pike function
 * and
 *   IDENTIFIER_ALIAS    - An alias for a different identifier.
 *
 */


/* Define an alias for a (possibly extern) identifier.
 *
 * Note that both type and name may be NULL. If they are NULL
 * they will be defaulted to the values from the aliased identifier.
 */
int low_define_alias(struct pike_string *name, struct pike_type *type,
		     int flags, int depth, int refno)
{
  int n;
  int e;

  struct compilation *c = THIS_COMPILATION;
  struct program_state *state = Pike_compiler;
  struct identifier dummy, *id;
  struct reference ref;

#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->flags & (PROGRAM_FIXED | PROGRAM_OPTIMIZED))
    Pike_fatal("Attempting to add variable to fixed program\n");

  if(Pike_compiler->compiler_pass==2)
    Pike_fatal("Internal error: Not allowed to add more identifiers during second compiler pass.\n"
	  "Added identifier: \"%s\"\n", name->str);
#endif

  for(e = 0; state && (e < depth); e++) {
    state = state->previous;
  }

#ifdef PIKE_DEBUG
  if (!state) {
    Pike_fatal("Internal error: External symbol buried too deep.\n");
  }
  if (state->new_program->num_identifier_references <= refno) {
    Pike_fatal("Internal error: Reference out of bounds: %d (max: %d).\n",
	       refno, state->new_program->num_identifier_references);
  }
#endif

  id = ID_FROM_INT(state->new_program, refno);

  if (name) {
    copy_shared_string(dummy.name, name);
  } else {
    copy_shared_string(dummy.name, id->name);
  }
  if (type) {
    copy_pike_type(dummy.type, type);
  } else {
    copy_pike_type(dummy.type, id->type);
  }
  dummy.filename_strno = store_prog_string(c->lex.current_file);
  dummy.linenumber = c->lex.current_line;
  dummy.identifier_flags = id->identifier_flags | IDENTIFIER_ALIAS;
  dummy.run_time_type = id->run_time_type;	/* Not actually used. */
  dummy.func.ext_ref.depth = depth;
  dummy.func.ext_ref.id = refno;
  dummy.opt_flags = 0;
#ifdef PROFILING
  dummy.self_time=0;
  dummy.num_calls=0;
  dummy.recur_depth=0;
  dummy.total_time=0;
#endif

  if (flags & ID_PRIVATE) flags |= ID_INLINE;

  ref.id_flags=flags;
  ref.identifier_offset=Pike_compiler->new_program->num_identifiers;
  ref.inherit_offset=0;

  debug_add_to_identifiers(dummy);

  n = Pike_compiler->new_program->num_identifier_references;
  add_to_identifier_references(ref);

  return n;
}

PMOD_EXPORT int define_alias(struct pike_string *name, struct pike_type *type,
			     int flags, int depth, int refno)
{
  /* FIXME: Support NULL name and type. */
  int n = isidentifier(name);

  if(Pike_compiler->new_program->flags & PROGRAM_PASS_1_DONE)
  {
    if(n==-1)
      yyerror("Pass2: Alias disappeared!");
    else {
      struct identifier *id = ID_FROM_INT(Pike_compiler->new_program, n);
      if (!IDENTIFIER_IS_ALIAS(id->identifier_flags)) {
	if (IDENTIFIER_IS_CONSTANT(id->identifier_flags)) {
	  /* Convert a placeholder constant into an alias. */
	  struct program_state *state = Pike_compiler;
	  int e;

	  for(e = 0; state && (e < depth); e++) {
	    state = state->previous;
	  }

#ifdef PIKE_DEBUG
	  if (!state) {
	    Pike_fatal("Internal error: External symbol buried too deep.\n");
	  }
	  if (state->new_program->num_identifier_references <= refno) {
	    Pike_fatal("Internal error: Reference out of bounds: %d (max: %d).\n",
		       refno, state->new_program->num_identifier_references);
	  }
#endif

	  id->identifier_flags = IDENTIFIER_ALIAS |
	    ID_FROM_INT(state->new_program, refno)->identifier_flags;
	} else {
	  Pike_fatal("Replacing non alias with an alias in second pass!\n");
	}
      }

      free_type(id->type);
      copy_pike_type(id->type, type);
      id->func.ext_ref.depth = depth;
      id->func.ext_ref.id = refno;
      return n;
    }
  }

#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->flags & (PROGRAM_FIXED | PROGRAM_OPTIMIZED))
    Pike_fatal("Attempting to add variable to fixed program\n");
#endif

  if(n != -1)
  {
    /* not inherited */
    if(Pike_compiler->new_program->identifier_references[n].inherit_offset == 0)
    {
      if (!((IDENTIFIERP(n)->id_flags | flags) & ID_EXTERN)) {
	my_yyerror("Identifier %S defined twice.",name);
	return n;
      }
      if (flags & ID_EXTERN) {
	/* FIXME: Check type */
	return n;
      }
    }

    if (!(IDENTIFIERP(n)->id_flags & ID_EXTERN)) {
      if (IDENTIFIERP(n)->id_flags & ID_FINAL)
	my_yyerror("Illegal to redefine 'final' "
		   "variable/functions %S", name);

      /* FIXME: More. */
    }
  }

  return low_define_alias(name, type, flags, depth, refno);
}

/* argument must be a shared string */
int low_define_variable(struct pike_string *name,
			struct pike_type *type,
			INT32 flags,
			size_t offset,
			INT32 run_time_type)
{
  int n;

  struct compilation *c = THIS_COMPILATION;
  struct identifier dummy;
  struct reference ref;

#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->flags & (PROGRAM_FIXED | PROGRAM_OPTIMIZED))
    Pike_fatal("Attempting to add variable to fixed program\n");

  if(Pike_compiler->compiler_pass==2)
    Pike_fatal("Internal error: Not allowed to add more identifiers during second compiler pass.\n"
	  "Added identifier: \"%s\"\n", name->str);
#endif

  copy_shared_string(dummy.name, name);
  copy_pike_type(dummy.type, type);
  dummy.filename_strno = store_prog_string(c->lex.current_file);
  dummy.linenumber = c->lex.current_line;
  dummy.identifier_flags = IDENTIFIER_VARIABLE;
  dummy.run_time_type=run_time_type;
  dummy.func.offset=offset - Pike_compiler->new_program->inherits[0].storage_offset;
  dummy.opt_flags = 0;
#ifdef PROFILING
  dummy.self_time=0;
  dummy.num_calls=0;
  dummy.recur_depth=0;
  dummy.total_time=0;
#endif

  if (run_time_type == PIKE_T_FREE) dummy.func.offset = -1;

  if (flags & ID_PRIVATE) flags |= ID_INLINE;

  ref.id_flags=flags;
  ref.identifier_offset=Pike_compiler->new_program->num_identifiers;
  ref.inherit_offset=0;

  add_to_variable_index(ref.identifier_offset);

  debug_add_to_identifiers(dummy);

  n=Pike_compiler->new_program->num_identifier_references;
  add_to_identifier_references(ref);

  return n;
}

/* type is a textual type */
PMOD_EXPORT int map_variable(const char *name,
		 const char *type,
		 INT32 flags,
		 size_t offset,
		 INT32 run_time_type)
{
  int ret;
  struct pike_string *n;
  struct pike_type *t;

#ifdef PROGRAM_BUILD_DEBUG
  struct compilation *c = THIS_COMPILATION;
  fprintf (stderr, "%.*sdefining variable (pass=%d): %s %s\n",
	   c->compilation_depth, "",
	   Pike_compiler->compiler_pass, type, name);
#endif

  n=make_shared_string(name);
  t=parse_type(type);
  ret=low_define_variable(n,t,flags|ID_USED,offset,run_time_type);
  free_string(n);
  free_type(t);
  return ret;
}

/* type is a serialized tokenized type. */
PMOD_EXPORT int quick_map_variable(const char *name,
		       int name_length,
		       size_t offset,
		       const char *type,
		       int type_length,
		       INT32 run_time_type,
		       INT32 flags)
{
  int ret;
  struct pike_string *n;
  struct pike_type *t;

  n = make_shared_binary_string(name, name_length);
  t = make_pike_type(type);

#ifdef PROGRAM_BUILD_DEBUG
  {
    struct compilation *c = THIS_COMPILATION;
    struct pike_string *d = describe_type (t);
    fprintf (stderr, "%.*sdefining variable (pass=%d): %s ",
	     c->compilation_depth, "", Pike_compiler->compiler_pass, d->str);
    free_string (d);
    push_string (n);
    print_svalue (stderr, --Pike_sp);
    putc ('\n', stderr);
  }
#endif

  ret=low_define_variable(n,t,flags|ID_USED,offset,run_time_type);
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
    Pike_fatal("define_variable on nonshared string.\n");
#endif

#ifdef PROGRAM_BUILD_DEBUG
  {
    struct compilation *c = THIS_COMPILATION;
    struct pike_string *d = describe_type (type);
    fprintf (stderr, "%.*sdefining variable (pass=%d): %s ",
	     c->compilation_depth, "", Pike_compiler->compiler_pass, d->str);
    free_string (d);
    push_string (name);
    print_svalue (stderr, --Pike_sp);
    putc ('\n', stderr);
  }
#endif

  if(type == void_type_string)
    yyerror("Variables can't be of type void.");

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
    Pike_fatal("Attempting to add variable to fixed program\n");
#endif

  if(n != -1)
  {
    /* not inherited */
    if(Pike_compiler->new_program->identifier_references[n].inherit_offset == 0)
    {
      if (!((IDENTIFIERP(n)->id_flags | flags) & ID_EXTERN)) {
	my_yyerror("Identifier %S defined twice.",name);
	return n;
      }
      if (flags & ID_EXTERN) {
	/* FIXME: Check type */
	return n;
      }
    }

    if (!(IDENTIFIERP(n)->id_flags & ID_EXTERN)) {
      if (IDENTIFIERP(n)->id_flags & ID_FINAL)
	my_yyerror("Illegal to redefine 'final' "
		   "variable/functions %S", name);

      if(!(IDENTIFIERP(n)->id_flags & ID_INLINE) ||
	 Pike_compiler->compiler_pass!=1)
      {
	int n2;

 	if(ID_FROM_INT(Pike_compiler->new_program, n)->type != type &&
	   !pike_types_le(type,
			  ID_FROM_INT(Pike_compiler->new_program, n)->type)) {
	  int level = REPORT_WARNING;
	  if (!match_types(ID_FROM_INT(Pike_compiler->new_program, n)->type,
			   type)) {
	    level = REPORT_ERROR;
	  }
	  yytype_report(level, NULL, 0,
			ID_FROM_INT(Pike_compiler->new_program, n)->type,
			NULL, 0, type, 0,
			"Illegal to redefine inherited variable %S "
			"with different type.", name);
	}
	

	if(!IDENTIFIER_IS_VARIABLE(ID_FROM_INT(Pike_compiler->new_program, n)->
				   identifier_flags))
	{
	  my_yyerror("Illegal to redefine inherited symbol %S "
		     "as a variable.", name);
	  return n;
	}

	if ((ID_FROM_INT(Pike_compiler->new_program, n)->run_time_type !=
	     PIKE_T_MIXED) &&
	    (ID_FROM_INT(Pike_compiler->new_program, n)->run_time_type !=
	     compile_type_to_runtime_type(type))) {
	  yytype_report(REPORT_ERROR, NULL, 0,
			ID_FROM_INT(Pike_compiler->new_program, n)->type,
			NULL, 0, type, 0,
			"Illegal to redefine inherited variable %S "
			"with different type.", name);
	  return n;
	}

	/* Create an alias for the old variable reference, so that we
	 * can change the compile-time type. */
	n2 = define_alias(name, type, flags & ~ID_EXTERN, 0, n);

	/* Hide the old variable. */
	Pike_compiler->new_program->identifier_references[n].id_flags |=
	  ID_HIDDEN;
	return n2;
      } else if ((IDENTIFIERP(n)->id_flags & (ID_INLINE|ID_INHERITED)) ==
		 (ID_INLINE|ID_INHERITED)) {
	/* Hide the overloaded inherited symbol. */
	IDENTIFIERP(n)->id_flags |= ID_HIDDEN;
      }
    } else if ((IDENTIFIERP(n)->id_flags & (ID_EXTERN|ID_INHERITED)) ==
	       (ID_EXTERN|ID_INHERITED)) {
      /* Hide the overloaded inherited symbol. */
      IDENTIFIERP(n)->id_flags |= ID_STATIC|ID_PRIVATE|ID_USED;
    }
  }

  if (flags & ID_EXTERN) {
    run_time_type = PIKE_T_FREE;
  } else {
    run_time_type=compile_type_to_runtime_type(type);

    switch(run_time_type)
    {
#ifdef AUTO_BIGNUM
    case T_INT:
#endif
    case T_OBJECT:
      /* Make place for the object subtype. */
    case T_FUNCTION:
    case T_PROGRAM:
      run_time_type = T_MIXED;
    }
  }

  n=low_define_variable(name,type,flags,
			low_add_storage(sizeof_variable(run_time_type),
					alignof_variable(run_time_type),0),
			run_time_type);

  ID_FROM_INT(Pike_compiler->new_program, n)->identifier_flags |= IDENTIFIER_NO_THIS_REF;

  return n;
}

PMOD_EXPORT int simple_add_variable(const char *name,
				    const char *type,
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
			     const struct svalue *c,
			     INT32 flags)
{
  int n;
  struct compilation *cc = THIS_COMPILATION;
  struct identifier dummy;
  struct reference ref;

#ifdef PROGRAM_BUILD_DEBUG
  {
    if (c) {
      struct pike_type *t = get_type_of_svalue(c);
      struct pike_string *d = describe_type (t);
      fprintf (stderr, "%.*sdefining constant (pass=%d): %s ",
	       cc->compilation_depth, "",
	       Pike_compiler->compiler_pass, d->str);
      free_type(t);
      free_string (d);
      push_string (name);
      print_svalue (stderr, --Pike_sp);
      fputs (" = ", stderr);
      print_svalue (stderr, c);
    }
    else {
      fprintf (stderr, "%.*sdeclaring constant (pass=%d): ",
	       cc->compilation_depth, "",
	       Pike_compiler->compiler_pass);
      push_string (name);
      print_svalue (stderr, --Pike_sp);
    }
    putc ('\n', stderr);
  }
#endif

#if 0
  if (!c) {
    c = &svalue_int_zero;
  }
#endif

#ifdef PIKE_DEBUG
  if(name!=debug_findstring(name))
    Pike_fatal("define_constant on nonshared string.\n");
  if (c) {
    check_svalue (c);
    if (c->type > MAX_TYPE)
      /* check_svalue allows some things like T_SVALUE_PTR. */
      Pike_fatal ("Invalid type in svalue: %d\n", c->type);
  }
#endif

  n = isidentifier(name);

  if(
#if 1
    c &&
#endif
    c->type == T_FUNCTION &&
    c->subtype != FUNCTION_BUILTIN &&
    c->u.object->prog)
  {
    struct program_state *state = Pike_compiler;
    struct reference *idref = PTR_FROM_INT(c->u.object->prog, c->subtype);
    struct program *p = PROG_FROM_PTR(c->u.object->prog, idref);
    struct identifier *id = p->identifiers + idref->identifier_offset;
    int depth = 0;
    while (state && (c->u.object->prog != state->new_program)) {
      depth++;
      state = state->previous;
    }
    if(state) {
      /* Alias for a symbol in the current or surrounding programs.
       */
      if(IDENTIFIER_IS_CONSTANT(id->identifier_flags) &&
	 (id->func.offset != -1) &&
	 (state == Pike_compiler)) {
	c=& p->constants[id->func.offset].sval;
      } else if (IDENTIFIER_IS_VARIABLE(id->identifier_flags) &&
		 (state == Pike_compiler)) {
	my_yyerror("Attempt to make a constant %S of a variable.",
		   name);
	c = NULL;
      } else {
	/* Alias for a function or a variable or constant in a surrounding
	 * scope.
	 */
	int n = c->subtype;
	struct reference *remote_ref = PTR_FROM_INT(state->new_program, n);
	if (!(remote_ref->id_flags & (ID_INLINE|ID_HIDDEN))) {
	  /* We need to get a suitable reference. */
	  n = really_low_reference_inherited_identifier(state, 0, n);
	}
	return define_alias(name, id->type, flags, depth, n);
      }
    }
  }
    
  if(
#if 1
    c &&
#endif
    !svalues_are_constant(c,1,BIT_MIXED,0))
    my_yyerror("Constant value %S has a reference to this.", name);

  if(Pike_compiler->new_program->flags & PROGRAM_PASS_1_DONE)
  {
    if(n==-1
#if 1
       || !c
#endif
       )
    {
      yyerror("Pass2: Constant disappeared!");
    }else{
      struct identifier *id;
      id=ID_FROM_INT(Pike_compiler->new_program,n);
      if(id->func.offset>=0) {
	/* Update the stored constant. */
	assign_svalue (&PROG_FROM_INT(Pike_compiler->new_program,n)->
		       constants[id->func.offset].sval, c);
      } else {
	id->run_time_type = (unsigned char) c->type;
	id->func.offset = store_constant(c, 0, 0);
      }
      free_type(id->type);
      if ((c->type == T_INT) && !(flags & ID_INLINE)) {
	if (c->u.integer) {
	  copy_pike_type(id->type, int_type_string);
	} else {
	  copy_pike_type(id->type, zero_type_string);
	}
      } else {
	id->type = get_type_of_svalue(c);
      }
#ifdef PROGRAM_BUILD_DEBUG
      fprintf (stderr, "%.*sstored constant #%d at %d\n",
	       cc->compilation_depth, "",
	       n, id->func.offset);
#endif
    }
    return n;
  }

#ifdef PIKE_DEBUG
  if(Pike_compiler->new_program->flags & (PROGRAM_FIXED | PROGRAM_OPTIMIZED))
    Pike_fatal("Attempting to add constant to fixed program\n");

  if(Pike_compiler->compiler_pass==2) {
    dump_program_tables(Pike_compiler->new_program, 2);
    Pike_fatal("Internal error: Not allowed to add more identifiers during second compiler pass.\n"
	       "                Attempted to add the identifier \"%s\"\n",
	       name->str);
  }
#endif

  copy_shared_string(dummy.name, name);
  dummy.identifier_flags = IDENTIFIER_CONSTANT;
  dummy.filename_strno = store_prog_string(cc->lex.current_file);
  dummy.linenumber = cc->lex.current_line;

#if 1
  if (c) {
#endif
    if ((c->type == T_INT) && !(flags & ID_INLINE)) {
      if (c->u.integer) {
	copy_pike_type(dummy.type, int_type_string);
      } else {
	copy_pike_type(dummy.type, zero_type_string);
      }
    } else {
      dummy.type = get_type_of_svalue(c);
    }
    dummy.run_time_type = (unsigned char) c->type;
    dummy.func.offset=store_constant(c, 0, 0);
    dummy.opt_flags=OPT_SIDE_EFFECT | OPT_EXTERNAL_DEPEND;
    if(c->type == PIKE_T_PROGRAM && (c->u.program->flags & PROGRAM_CONSTANT))
       dummy.opt_flags=0;
#if 1
  }
  else {
    copy_pike_type(dummy.type, mixed_type_string);
    dummy.run_time_type=T_MIXED;
    dummy.func.offset=-1;
    dummy.opt_flags=0;
  }
#endif

  if (flags & ID_PRIVATE) flags |= ID_INLINE;

  ref.id_flags=flags;
  ref.identifier_offset=Pike_compiler->new_program->num_identifiers;
  ref.inherit_offset=0;

#ifdef PROFILING
  dummy.self_time=0;
  dummy.num_calls=0;
  dummy.recur_depth=0;
  dummy.total_time=0;
#endif

  debug_add_to_identifiers(dummy);

  if(n != -1)
  {
    int overridden;

    if(IDENTIFIERP(n)->id_flags & ID_FINAL)
      my_yyerror("Illegal to redefine 'final' identifier %S", name);

    if(!TEST_COMPAT(7,2) &&
       IDENTIFIER_IS_VARIABLE(ID_FROM_INT(Pike_compiler->new_program,
					  n)->identifier_flags))
    {
      my_yyerror("Illegal to redefine variable %S as constant.", name);
    }

    /* not inherited */
    if(Pike_compiler->new_program->identifier_references[n].inherit_offset == 0)
    {
      my_yyerror("Identifier %S defined twice.", name);
      return n;
    }

    /* override */
    if ((overridden = override_identifier (&ref, name)) >= 0) {
#ifdef PIKE_DEBUG
      struct reference *oref =
	Pike_compiler->new_program->identifier_references+overridden;
      if((oref->inherit_offset != ref.inherit_offset) ||
	 (oref->identifier_offset != ref.identifier_offset) ||
	 ((oref->id_flags | ID_USED) != (ref.id_flags | ID_USED))) {
	Pike_fatal("New constant overriding algorithm failed!\n");
      }
#endif
      return overridden;
    }
  }
  n=Pike_compiler->new_program->num_identifier_references;
  add_to_identifier_references(ref);

  return n;
}

PMOD_EXPORT int simple_add_constant(const char *name,
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

PMOD_EXPORT int add_integer_constant(const char *name,
				     INT_ARG_TYPE i,
				     INT32 flags)
{
  struct svalue tmp;
  tmp.u.integer=i;
  tmp.type=T_INT;
  tmp.subtype=NUMBER_NUMBER;
  return simple_add_constant(name, &tmp, flags);
}

PMOD_EXPORT int quick_add_integer_constant(const char *name,
					   int name_length,
					   INT_ARG_TYPE i,
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

PMOD_EXPORT int add_float_constant(const char *name,
				   FLOAT_ARG_TYPE f,
				   INT32 flags)
{
  struct svalue tmp;
  tmp.type=T_FLOAT;
  tmp.u.float_number = (FLOAT_TYPE) f;
  tmp.subtype=0;
  return simple_add_constant(name, &tmp, flags);
}

PMOD_EXPORT int quick_add_float_constant(const char *name,
					 int name_length,
					 FLOAT_ARG_TYPE f,
					 INT32 flags)
{
  struct svalue tmp;
  struct pike_string *id;
  INT32 ret;

  tmp.u.float_number = (FLOAT_TYPE) f;
  tmp.type=T_FLOAT;
  tmp.subtype=0;
  id=make_shared_binary_string(name,name_length);
  ret=add_constant(id, &tmp, flags);
  free_string(id);
  return ret;
}

PMOD_EXPORT int add_string_constant(const char *name,
				    const char *str,
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

PMOD_EXPORT int add_program_constant(const char *name,
				     struct program *p,
				     INT32 flags)
{
  INT32 ret;
  struct svalue tmp;
  if (p) {
    tmp.type=T_PROGRAM;
    tmp.subtype=0;
    tmp.u.program=p;
  } else {
    /* Probable compilation error in a C-module. */
    tmp.type = T_INT;
    tmp.subtype = NUMBER_UNDEFINED;
    tmp.u.integer = 0;
    my_yyerror("Program constant \"%s\" is NULL.", name);
  }
  ret=simple_add_constant(name, &tmp, flags);
  return ret;
}

PMOD_EXPORT int add_object_constant(const char *name,
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

PMOD_EXPORT int add_function_constant(const char *name, void (*cfun)(INT32),
				      const char * type, int flags)
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


PMOD_EXPORT int debug_end_class(const char *name, ptrdiff_t namelen, INT32 flags)
{
  INT32 ret;
  struct svalue tmp;
  struct pike_string *id;

  tmp.type=T_PROGRAM;
  tmp.subtype=0;
  tmp.u.program=end_program();
  if(!tmp.u.program)
    Pike_fatal("Failed to initialize class '%s'\n",name);

  id=make_shared_binary_string(name,namelen);
  ret=add_constant(id, &tmp, flags);
  /* The following is not really true, but it helps encode_value()... */
  Pike_compiler->new_program->flags |=
    tmp.u.program->flags & PROGRAM_HAS_C_METHODS;
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
		      unsigned flags,
		      unsigned function_flags,
		      union idptr *func,
		      unsigned opt_flags)
{
  struct compilation *c = THIS_COMPILATION;
  struct identifier *funp,fun;
  struct reference ref;
  struct svalue *lfun_type;
  int run_time_type = T_FUNCTION;
  INT32 i;
  int getter_setter = -1;
  int is_setter = 0;

  CHECK_COMPILER();

#ifdef PROGRAM_BUILD_DEBUG
  {
    struct pike_string *d = describe_type (type);
    fprintf (stderr, "%.*sdefining function (pass=%d): %s ",
	     c->compilation_depth, "", Pike_compiler->compiler_pass, d->str);
    free_string (d);
    push_string (name);
    print_svalue (stderr, --Pike_sp);
    putc ('\n', stderr);
  }
#endif

#ifdef PROFILING
  fun.self_time=0;
  fun.num_calls=0;
  fun.recur_depth=0;
  fun.total_time=0;
#endif

  /* If this is an lfun, match against the predefined type. */
  if ((lfun_type = low_mapping_string_lookup(lfun_types, name))) {
#ifdef PIKE_DEBUG
    if (lfun_type->type != T_TYPE) {
      Pike_fatal("Bad entry in lfun_types for key \"%s\"\n", name->str);
    }
#endif /* PIKE_DEBUG */
    if (!pike_types_le(type, lfun_type->u.type)) {
      int level = REPORT_NOTICE;
      if (!match_types(type, lfun_type->u.type)) {
	level = REPORT_ERROR;
      } else if (c->lex.pragmas & ID_STRICT_TYPES) {
	level = REPORT_WARNING;
      }
      if (level != REPORT_NOTICE) {
	yytype_report(level, NULL, 0, lfun_type->u.type,
		      NULL, 0, type, 0,
		      "Type mismatch for callback function %S:", name);
      }
    }
  } else if (((name->len > 3) &&
	      (index_shared_string(name, 0) == '`') &&
	      (index_shared_string(name, 1) == '-') &&
	      (index_shared_string(name, 2) == '>')) ||
	     ((name->len > 1) &&
	      (index_shared_string(name, 0) == '`') &&
	      ((index_shared_string(name, 1) >= 256) ||
	       isidchar(index_shared_string(name, 1))))) {
    /* Getter setter. */
    struct pike_string *symbol = NULL;
    struct pike_type *symbol_type = NULL;
    struct pike_type *gs_type = NULL;
    int delta = 1;	/* new-style */
    if (index_shared_string(name, 1) == '-') {
      /* Getter setter (old-style). */
      delta = 3;
    }
    if (index_shared_string(name, name->len-1) != '=') {
      /* fprintf(stderr, "Got getter: %s\n", name->str); */
      gs_type = lfun_getter_type_string;
      symbol = string_slice(name, delta, name->len-delta);
      symbol_type = get_argument_type(type, -1);
    } else if (name->len > delta+1) {
      /* fprintf(stderr, "Got setter: %s\n", name->str); */
      gs_type = lfun_setter_type_string;
      is_setter = 1;
      symbol = string_slice(name, delta, name->len-(delta+1));
      symbol_type = get_argument_type(type, 0);
    }

    if (symbol) {
      /* We got a getter or a setter. */
      struct reference *ref;
      if (!pike_types_le(type, gs_type)) {
	int level = REPORT_NOTICE;
	if (!match_types(type, gs_type)) {
	  level = REPORT_ERROR;
	} else if (c->lex.pragmas & ID_STRICT_TYPES) {
	  level = REPORT_WARNING;
	}
	yytype_report(level, NULL, 0, gs_type,
		      NULL, 0, type, 0,
		       "Type mismatch for callback function %S:", name);
      }
      i = isidentifier(symbol);
      if ((i >= 0) && 
	  !((ref = PTR_FROM_INT(Pike_compiler->new_program, i))->
	    id_flags & ID_INHERITED)) {
	/* Not an inherited symbol. */
	struct identifier *id = ID_FROM_INT(Pike_compiler->new_program, i);
	if (!IDENTIFIER_IS_VARIABLE(id->identifier_flags)) {
	  my_yyerror("Illegal to redefine function %S with variable.", symbol);
	} else if (id->run_time_type != PIKE_T_GET_SET) {
	  my_yyerror("Illegal to redefine a current variable with a getter/setter: %S.", symbol);
	} else {
	  if ((ref->id_flags | ID_USED) != (flags | ID_USED)) {
	    if (Pike_compiler->compiler_pass == 1) {
	      yywarning("Modifier mismatch for variable %S.", symbol);
	    }
	    ref->id_flags &= flags | ID_USED;
	  }
	  getter_setter = i;
	}
	/* FIXME: Update id->type here. */
      } else {
	struct identifier *id;
	i = low_define_variable(symbol, symbol_type, flags,
				~0, PIKE_T_GET_SET);
	id = ID_FROM_INT(Pike_compiler->new_program, i);

	/* Paranoia. */
	id->func.gs_info.getter = -1;
	id->func.gs_info.setter = -1;

	getter_setter = i;
      }
      /* NOTE: The function needs to have the same PRIVATE/INLINE
       *       behaviour as the variable for overloading to behave
       *       as expected.
       *
       * FIXME: Force PRIVATE?
       */
      flags |= ID_PROTECTED /* | ID_PRIVATE | ID_INLINE | ID_USED */;
      free_type(symbol_type);
      free_string(symbol);
    }
  }

  if(IDENTIFIER_IS_C_FUNCTION(function_flags))
    Pike_compiler->new_program->flags |= PROGRAM_HAS_C_METHODS;

  if (Pike_compiler->compiler_pass == 1) {
    /* Mark the type as tentative by setting the runtime-type
     * to T_MIXED.
     *
     * NOTE: This should be reset to T_FUNCTION in pass 2.
     */
    run_time_type = T_MIXED;
  }

  i=isidentifier(name);

  if(i >= 0)
  {
    int overridden;

    /* already defined */

#ifdef PROGRAM_BUILD_DEBUG
    fprintf(stderr, "%.*sexisted as identifier #%d\n",
	  c->compilation_depth, "", i);
#endif

    funp=ID_FROM_INT(Pike_compiler->new_program, i);
    ref=Pike_compiler->new_program->identifier_references[i];

    if (funp->identifier_flags & IDENTIFIER_HAS_BODY)
      /* Keep this flag. */
      function_flags |= IDENTIFIER_HAS_BODY;

    if(!(ref.id_flags & ID_INHERITED)) /* not inherited */
    {

      if( !( IDENTIFIER_IS_FUNCTION(funp->identifier_flags) &&
	     ( (!func || func->offset == -1) || (funp->func.offset == -1))))
      {
	my_yyerror("Identifier %S defined twice.", name);

	if (getter_setter != -1) {
	  struct identifier *id = ID_FROM_INT(Pike_compiler->new_program,
					      getter_setter);
	  (&id->func.gs_info.getter)[is_setter] = i;
	}
	return i;
      }

      /* Note: The type from pass 1 may be incompatible with the one from
       *       pass 2. Only do this in pass 2, and only if the previous
       *       type isn't from pass 1.
       */
      if ((Pike_compiler->compiler_pass == 2) &&
	  (funp->run_time_type == T_FUNCTION)) {
	/* match types against earlier prototype or vice versa */
	if(!match_types(type, funp->type))
	{
	  if (!(flags & ID_VARIANT)) {
	    yytype_report(REPORT_ERROR, NULL, 0,
			  funp->type,
			  NULL, 0, type, 0,
			 "Prototype doesn't match for function %S.", name);
	  }
	}
      }

      if(func)
	funp->func = *func;
#if 0 /* prototypes does not override non-prototypes, ok? */
      else
	funp->func.offset = -1;
#endif

      funp->identifier_flags=function_flags;
      funp->run_time_type = run_time_type;

      funp->opt_flags &= opt_flags;

      free_type(funp->type);
      copy_pike_type(funp->type, type);
    }else{
#ifdef PROGRAM_BUILD_DEBUG
      fprintf(stderr, "%.*sidentifier was inherited\n",
	      c->compilation_depth, "");
#endif

      if((ref.id_flags & ID_FINAL)
#if 0
	 && !(funp->func.offset == -1)
#endif
	)
      {
	my_yyerror("Illegal to redefine 'final' function %S.", name);
      }


      if(ref.id_flags & ID_INLINE)
      {
#ifdef PROGRAM_BUILD_DEBUG
	fprintf(stderr, "%.*sidentifier is local\n",
		c->compilation_depth, "");
#endif
	/* Hide the previous definition, and make a new definition. */
	Pike_compiler->new_program->identifier_references[i].id_flags |=
	  ID_PROTECTED;
	goto make_a_new_def;
      }

      /* Otherwise we alter the existing definition */
#ifdef PROGRAM_BUILD_DEBUG
      fprintf(stderr, "%.*saltering the existing definition\n",
	      c->compilation_depth, "");
#endif

      copy_shared_string(fun.name, name);
      copy_pike_type(fun.type, type);
      fun.filename_strno = store_prog_string(c->lex.current_file);
      fun.linenumber = c->lex.current_line;

      fun.run_time_type = run_time_type;

      fun.identifier_flags=function_flags;

      if(func)
	fun.func = *func;
      else
	fun.func.offset = -1;

      fun.opt_flags = opt_flags;

      ref.identifier_offset=Pike_compiler->new_program->num_identifiers;
      debug_add_to_identifiers(fun);
    }

    if (flags & ID_PRIVATE) flags |= ID_INLINE;

    ref.inherit_offset = 0;
    ref.id_flags = flags;
    if ((overridden = override_identifier (&ref, name)) >= 0) {
#ifdef PIKE_DEBUG
      struct reference *oref =
	Pike_compiler->new_program->identifier_references+overridden;
      if((oref->inherit_offset != ref.inherit_offset) ||
	 (oref->identifier_offset != ref.identifier_offset) ||
	 ((oref->id_flags | ID_USED) != (ref.id_flags | ID_USED))) {
	fprintf(stderr,
		"ref: %d:%d 0x%04x\n"
		"got: %d:%d 0x%04x (%d)\n",
		ref.inherit_offset, ref.identifier_offset,
		ref.id_flags,
		oref->inherit_offset,
		oref->identifier_offset,
		oref->id_flags,
		overridden);
	Pike_fatal("New function overloading algorithm failed!\n");
      }
#endif

      if (getter_setter != -1) {
	struct identifier *id = ID_FROM_INT(Pike_compiler->new_program,
					    getter_setter);
	INT32 old_i = (&id->func.gs_info.getter)[is_setter];
	if ((old_i >= 0) && (old_i != overridden)) {
	  my_yyerror("Multiple definitions for %S.", name);
	}
	(&id->func.gs_info.getter)[is_setter] = overridden;
      }
      return overridden;
    }
    /* NOTE: At this point we already have the identifier in the
     *       new program, and just need to add the reference.
     */
  } else {
  make_a_new_def:

#ifdef PIKE_DEBUG
    if(Pike_compiler->compiler_pass==2)
      Pike_fatal("Internal error: Not allowed to add more identifiers during second compiler pass.\n");
#endif

    /* Define a new function */

    copy_shared_string(fun.name, name);
    copy_pike_type(fun.type, type);
    fun.filename_strno = store_prog_string(c->lex.current_file);
    fun.linenumber = c->lex.current_line;

    fun.identifier_flags=function_flags;
    fun.run_time_type = run_time_type;

    if(func)
      fun.func = *func;
    else
      fun.func.offset = -1;

    fun.opt_flags = opt_flags;

#ifdef PIKE_DEBUG
    if (a_flag > 5) {
      fprintf(stderr, 
	      "Adding new function #%d: '%s'\n"
	      "  identifier_flags:0x%02x opt_flags:0x%04x\n",
	      Pike_compiler->new_program->num_identifiers,
	      fun.name->str,
	      fun.identifier_flags, fun.opt_flags);
    }
#endif /* PIKE_DEBUG */

    i=Pike_compiler->new_program->num_identifiers;

    debug_add_to_identifiers(fun);

    if (flags & ID_PRIVATE) flags |= ID_INLINE;

    ref.id_flags = flags;
    ref.identifier_offset = i;
    ref.inherit_offset = 0;
  }

  /* Add the reference. */

  i=Pike_compiler->new_program->num_identifier_references;
  add_to_identifier_references(ref);

#ifdef PROGRAM_BUILD_DEBUG
  fprintf(stderr, "%.*sadded new definition #%d\n",
	  c->compilation_depth, "", i);
#endif

  if (getter_setter != -1) {
    struct identifier *id = ID_FROM_INT(Pike_compiler->new_program,
					getter_setter);
    INT32 old_i = (&id->func.gs_info.getter)[is_setter];
    if (old_i >= 0) {
      my_yyerror("Multiple definitions for %S.", name);
    }
    (&id->func.gs_info.getter)[is_setter] = i;
  }

  return i;
}

#if 0

int add_ext_ref(struct program_state *state, struct program *target, int i)
{
  struct reference ref, *r;
  int j;
  if (state->new_program == target) return i;
  i = add_ext_ref(state->previous, target, i);
  for (r = state->new_program->identifier_references, j = 0;
       j < state->new_program->num_identifier_references;
       j++, r++) {
    if (((r->id_flags & (ID_PARENT_REF|ID_PROTECTED|ID_PRIVATE|ID_HIDDEN)) ==
	 ID_PARENT_REF|ID_PROTECTED|ID_PRIVATE|ID_HIDDEN) &&
	(r->identifier_offset == i) &&
	(!(r->inherit_offset))) {
      return j;
    }
  }
  ref.id_flags = ID_PARENT_REF|ID_PROTECTED|ID_PRIVATE|ID_HIDDEN|ID_INLINE;
  ref.identifier_offset = i;
  ref.inherit_offset = 0;
  add_to_identifier_references(ref);
  state->new_program->flags |= PROGRAM_USES_PARENT | PROGRAM_NEEDS_PARENT;
  return j;
}

#endif /* 0 */

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
 * Note also that the changed handling of mixin's in Pike 7.7.40 affects
 * the behaviour when using static inherit. See F below.
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
 * class E {
 *   int foo() {}
 * }
 *
 * class F {
 *   inherit A;
 *   static inherit E;
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
 *
 * External lookup of identifier "foo" in E():
 *
 * F-+-A---foo		            --- Pike 7.7.33
 *   |
 *   +-E---foo		Pike 7.7.34 ---
 */
int really_low_find_shared_string_identifier(struct pike_string *name,
					     struct program *prog,
					     int flags)
{
  struct reference *funp;
  struct identifier *fun;
  int id, i, depth, last_inh;

#if 0
  CDFPRINTF((stderr,"th(%ld) %p Trying to find %s flags=%d\n",
	     (long)th_self(), prog, name->str, flags));
#endif

#ifdef PIKE_DEBUG
  if (!prog) {
    Pike_fatal("really_low_find_shared_string_identifier(\"%s\", NULL, %d)\n"
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
    if(funp->id_flags & ID_PROTECTED)
      if(!(flags & SEE_PROTECTED))
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

PMOD_EXPORT int low_find_lfun(struct program *p, ptrdiff_t lfun)
{
  struct pike_string *lfun_name = lfun_strings[lfun];
  unsigned int flags = 0;
#if 0
  struct identifier *id;
#endif
  int i =
    really_low_find_shared_string_identifier(lfun_name,
					     dmalloc_touch(struct program *,
							   p),
					     SEE_PROTECTED);
#if 0
  if (i < 0 || !(p->flags & PROGRAM_FIXED)) return i;
  id = ID_FROM_INT(p, i);
  if (IDENTIFIER_IS_PIKE_FUNCTION(id->identifier_flags) &&
      (id->func.offset == -1)) {
    /* Function prototype. */
    return -1;
  }
#endif /* 0 */
  return i;
}

int lfun_lookup_id(struct pike_string *lfun_name)
{
  struct svalue *id = low_mapping_string_lookup(lfun_ids, lfun_name);
  if (!id) return -1;
  if (id->type == T_INT) return id->u.integer;
  my_yyerror("Bad entry in lfun lookup table for %S.", lfun_name);
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
    size_t val_n = PTR_TO_INT (name);

#ifdef PIKE_DEBUG
    if(!funindex)
      Pike_fatal("No funindex in fixed program\n");
#endif

    max = prog->num_identifier_index;
    min = 0;
    while(max != min)
    {
      size_t val_t;

      tst=(max + min) >> 1;
      fun = ID_FROM_INT(prog, funindex[tst]);
      if(is_same_string(fun->name,name)) return funindex[tst];
      val_t = PTR_TO_INT (fun->name);
      if (val_n < val_t) {
	max = tst;
      } else {
	min = tst+1;
      }
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
    Pike_fatal("find_shared_string_identifier(): No program!\n"
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

/* NOTE: O(n�)! */
int store_constant(const struct svalue *foo,
		   int equal,
		   struct pike_string *constant_name)
{
  struct program_constant tmp;
  volatile unsigned int e;

  JMP_BUF jmp;
  if (SETJMP(jmp)) {
    handle_compile_exception ("Error comparing constants.");
    /* Assume that if `==() throws an error, the svalues aren't equal. */
    e = Pike_compiler->new_program->num_constants;
  } else {
    for(e=0;e<Pike_compiler->new_program->num_constants;e++)
    {
      struct program_constant *c = Pike_compiler->new_program->constants+e;

      if (foo->type == c->sval.type) {
	/* Make sure only to compare within the same basic type. */
	if (foo->type == T_OBJECT) {
	  /* Special case objects -- We don't want strange LFUN effects... */
	  if ((foo->u.object == c->sval.u.object) &&
	      (foo->subtype == c->sval.subtype)) {
	    UNSETJMP(jmp);
	    return e;
	  }
	} else if (foo->type == T_INT) {
	  if (foo->u.integer == c->sval.u.integer) {
	    /* Make sure UNDEFINED is kept (but not in compat mode). */
	    if (foo->u.integer || (foo->subtype == c->sval.subtype) ||
		TEST_COMPAT(7, 6)) {
	      UNSETJMP(jmp);
	      return e;
	    }
	  }
	} else if(equal ? is_equal(& c->sval,foo) : is_eq(& c->sval,foo)) {
	  UNSETJMP(jmp);
	  return e;
	}
      }
    }
  }
  UNSETJMP(jmp);
  assign_svalue_no_free(&tmp.sval,foo);
#if 0
  if((tmp.name=constant_name)) add_ref(constant_name);
#else /* !0 */
  tmp.offset = -1;
#endif /* 0 */

  add_to_constants(tmp);

  return e;
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
    if (p->identifier_references[e].id_flags &
	(ID_HIDDEN|ID_PROTECTED|ID_PRIVATE)) {
      continue;
    }
    id = ID_FROM_INT(p, e);
    if (IDENTIFIER_IS_ALIAS(id->identifier_flags)) {
      /* FIXME!
       */
      continue;
    } else if (IDENTIFIER_IS_CONSTANT(id->identifier_flags)) {
      if (id->func.offset >= 0) {
	struct program *p2 = PROG_FROM_INT(p, e);
	struct svalue *val = &p2->constants[id->func.offset].sval;
	if ((val->type != T_PROGRAM) ||
	    !(val->u.program->flags & PROGRAM_USES_PARENT)) {
	  ref_push_string(ID_FROM_INT(p, e)->name);
	  n++;
	}
      } else {
	/* Prototype constant. */
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
    if (p->identifier_references[e].id_flags &
	(ID_HIDDEN|ID_PROTECTED|ID_PRIVATE)) {
      continue;
    }
    id = ID_FROM_INT(p, e);
    if (IDENTIFIER_IS_ALIAS(id->identifier_flags)) {
      /* FIXME!
       */
      continue;
    } else if (IDENTIFIER_IS_CONSTANT(id->identifier_flags)) {
      if (id->func.offset >= 0) {
	struct program *p2 = PROG_FROM_INT(p, e);
	struct svalue *val = &p2->constants[id->func.offset].sval;
	if ((val->type != T_PROGRAM) ||
	    !(val->u.program->flags & PROGRAM_USES_PARENT)) {
	  push_svalue(val);
	  n++;
	}
      } else {
	/* Prototype constant. */
	push_int(0);
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

int program_index_no_free(struct svalue *to, struct svalue *what,
			  struct svalue *ind)
{
  int e;
  struct pike_string *s;
  struct object *parent = NULL;
  struct svalue *sub = NULL;
  struct program *p;
  struct identifier *id;
  int parent_identifier = -1;

  if (what->type == T_PROGRAM) {
    p = what->u.program;
  } else if ((what->type == T_FUNCTION) &&
	     (what->subtype != FUNCTION_BUILTIN) &&
	     ((parent = what->u.object)->prog) &&
	     IDENTIFIER_IS_CONSTANT((id = ID_FROM_INT(parent->prog,
						      what->subtype))->identifier_flags) &&
	     (id->func.offset != -1) &&
	     ((sub = &PROG_FROM_INT(parent->prog, what->subtype)->constants[id->func.offset].sval)->type == T_PROGRAM)) {
    p = sub->u.program;
    parent_identifier = what->subtype;
  } else {
    /* Not a program. */
    return 0;
  }
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

    if (IDENTIFIER_IS_ALIAS(id->identifier_flags)) {
      struct external_variable_context loc;
      struct object fake_object;
      struct parent_info parent_info;
      int refid;

      if (!parent) goto fail;

      parent_info.parent = parent;
      parent_info.parent_identifier = parent_identifier;
      fake_object.prog = p;
      fake_object.refs = 1;
      fake_object.next = fake_object.prev = NULL;
      fake_object.storage = ((char *)&parent_info) - p->parent_info_storage;
#ifdef PIKE_DEBUG
      fake_object.program_id = p->id;
#endif

      loc.o = &fake_object;
      loc.inherit = INHERIT_FROM_INT(p, e);
      loc.parent_identifier = 0;

      do {
	find_external_context(&loc, id->func.ext_ref.depth);
	refid = id->func.ext_ref.id;
	id = ID_FROM_INT(loc.o->prog, refid);
      } while (IDENTIFIER_IS_ALIAS(id->identifier_flags));

      if (fake_object.refs != 1) {
	Pike_fatal("Lost track of fake object! refs: %d\n",
		   fake_object.refs);
      }

      if (loc.o != &fake_object) {
	low_object_index_no_free(to, loc.o, refid);
#if 0 && defined (COMPILER_DEBUG)
	safe_pike_fprintf (stderr, "program_index_no_free1 %O->%S: %O\n",
			   what, s, to);
#endif
	return 1;
      }
    }

    if (IDENTIFIER_IS_CONSTANT(id->identifier_flags)) {
      if (id->func.offset >= 0) {
	struct program *p2 = PROG_FROM_INT(p, e);
	struct svalue *val = &p2->constants[id->func.offset].sval;
	assign_svalue_no_free(to, val);
      } else {
	/* Prototype constant. */
	to->type = T_INT;
	to->subtype = NUMBER_NUMBER;
	to->u.integer = 0;
      }
#if 0 && defined (COMPILER_DEBUG)
      safe_pike_fprintf (stderr, "program_index_no_free2 %O->%S: %O\n",
			 what, s, to);
#endif
      return 1;
    }
  }
 fail:

  to->type=T_INT;
  to->subtype=NUMBER_UNDEFINED;
  to->u.integer=0;
  return 1;
}

/*
 * Line number support routines, now also tells what file we are in.
 *
 * FIXME: Consider storing the filenames in strings (like what is now done
 *        for identifiers).
 */

/* program.linenumbers format:
 *
 * Filename entry:
 *   1. char		127 (marker).
 *   2. small number	Filename string length.
 *   3. char		Filename string size shift.
 *   4. string data	(Possibly wide) filename string without null
 *                      termination.
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
 *     2. short		The 16-bit signed number stored in big endian order.
 *   Else if n < -0x80000000 or n > 0x7fffffff in Pike 7.9 and later:
 *     1. char		-127 (marker).
 *     2. short		Zero (64-bit marker).
 *     3. INT_TYPE	The 64-bit signed number stored in big endian order.
 *   Else:
 *     1. char		-128 (marker).
 *     2. int		The 32-bit signed number stored in big endian order.
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
  unsigned char *addr = (unsigned char *)*q;
  int ret = *((signed char *)addr);
  ret=*(signed char *)*q;
  addr++;
  switch(ret)
  {
  case -127:
    ret = (((signed char *)addr)[0]<<8) | addr[1];
    addr += 2;
    if (ret) break;

    /* Forward compat with Pike 7.9. */
    addr += 4;	/* Ignore the high 32-bits. */

    /* FALL_THROUGH */
  case -128:
    ret = (((signed char *)addr)[0]<<24) | (addr[1]<<16) |
      (addr[2]<<8) | addr[3];
    addr += 4;
    break;

#ifdef PIKE_DEBUG
  case 127:
    Pike_fatal("get_small_number used on filename entry\n");
#endif
  }
  *q = (char *)addr;
  return ret;
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
    add_to_linenumbers(a>>8);
    add_to_linenumbers(a);
  }else{
    add_to_linenumbers(-128);
    add_to_linenumbers(a>>24);
    add_to_linenumbers(a>>16);
    add_to_linenumbers(a>>8);
    add_to_linenumbers(a);
  }
#ifdef PIKE_DEBUG
  {
    char *tmp = Pike_compiler->new_program->linenumbers + start;
    INT32 res = get_small_number(&tmp);
    if (a != res) {
      tmp = Pike_compiler->new_program->linenumbers + start;
      fprintf(stderr, "0x%p: %02x %02x %02x %02x %02x\n",
	      tmp, (unsigned char) tmp[0], (unsigned char) tmp[1],
	      (unsigned char) tmp[2], (unsigned char) tmp[3], (unsigned char) tmp[4]);
      Pike_fatal("insert_small_number failed: %d (0x%08x) != %d (0x%08x)\n",
	    a, a, res, res);
    }
  }
#endif /* PIKE_DEBUG */
}

static void ext_insert_small_number (char **ptr, INT32 a)
{
  if(a>-127 && a<127)
  {
    *(*ptr)++ = a;
  }else if(a>=-32768 && a<32768){
    *(*ptr)++ = -127;
    *(*ptr)++ = a>>8;
    *(*ptr)++ = a;
  }else{
    *(*ptr)++ = -128;
    *(*ptr)++ = a>>24;
    *(*ptr)++ = a>>16;
    *(*ptr)++ = a>>8;
    *(*ptr)++ = a;
  }
}

void ext_store_program_line (struct program *prog, INT32 line, struct pike_string *file)
{
  char *ptr;

#ifdef PIKE_DEBUG
  if (prog->linenumbers)
    Pike_fatal ("Program already got linenumber info.\n");
  if (Pike_compiler->new_program == prog)
    Pike_fatal ("Use store_linenumber instead when the program is compiled.\n");
#endif

  ptr = prog->linenumbers = xalloc (1 + 5 + 1 + (file->len << file->size_shift) + 5 + 5);
  *ptr++ = 127;
  ext_insert_small_number (&ptr, file->len);
  *ptr++ = (char) file->size_shift;
  MEMCPY (ptr, file->str, file->len << file->size_shift);
  ptr += file->len << file->size_shift;
  *ptr++ = 0;			/* PC */
  ext_insert_small_number (&ptr, line);
  prog->num_linenumbers = ptr - prog->linenumbers;
}

void store_linenumber(INT32 current_line, struct pike_string *current_file)
{
/*  if(!store_linenumbers)  Pike_fatal("Fnord.\n"); */
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
	CHECK_FILE_ENTRY (Pike_compiler->new_program, cnt, len, shift);
	cnt += len<<shift;
	if (a_flag > 100) {
	  fprintf(stderr, "Filename entry:\n"
		  "  len: %"PRINTSIZET"d, shift: %d\n",
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
	MEMCMP(Pike_compiler->last_file->str, file, len<<shift)))
    {
      Pike_fatal("Line numbering out of whack\n"
	    "    (line : %d ?= %d)!\n"
	    "    (  pc : %d ?= %d)!\n"
	    "    (shift: %d ?= %d)!\n"
	    "    (len  : %"PRINTSIZET"d ?= %"PRINTSIZET"d)!\n"
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

#define FIND_PROGRAM_LINE(prog, file, len, shift, line) do {		\
    char *pos = prog->linenumbers;					\
    len = 0;								\
    shift = 0;								\
    file = NULL;							\
									\
    if (pos < prog->linenumbers + prog->num_linenumbers) {		\
      if (*pos == 127) {						\
	pos++;								\
	len = get_small_number(&pos);					\
	shift = *pos;							\
	file = ++pos;							\
	CHECK_FILE_ENTRY (prog, pos, len, shift);			\
	pos += len<<shift;						\
      }									\
      get_small_number(&pos);	/* Ignore the offset */			\
      line = get_small_number(&pos);					\
    }									\
  } while (0)

PMOD_EXPORT struct pike_string *low_get_program_line (struct program *prog,
						      INT32 *linep)
{
  *linep = 0;

  if (prog->linenumbers) {
    size_t len;
    INT32 shift;
    char *file;

    FIND_PROGRAM_LINE (prog, file, len, shift, (*linep));

    if (file) {
      struct pike_string *str = begin_wide_shared_string(len, shift);
      memcpy(str->str, file, len<<shift);
      return end_shared_string(str);
    }
  }

  return NULL;
}

static char *make_plain_file (char *file, size_t len, INT32 shift, int malloced)
{
  static char buf[1000];
  if(shift)
  {
    size_t bufsize;
    char *buffer;
    PCHARP from=MKPCHARP(file, shift);
    size_t ptr=0;

    if (malloced) {
      bufsize = len + 1;
      buffer = malloc (bufsize);
    }
    else {
      bufsize = NELEM(buf) - 1;
      buffer = buf;
    }

    for (; len--; INC_PCHARP(from, 1))
    {
      size_t space;
      int chr = EXTRACT_PCHARP(from);
      space = chr > 255 ? 20 : 1;

      if (ptr + space > bufsize) {
	if (malloced) {
	  bufsize = (bufsize << 1) + space + 1;
	  buffer = realloc (buffer, bufsize);
	}
	else
	  break;
      }

      if(chr > 255)
      {
	sprintf(buffer+ptr,"\\u%04X",chr);
	ptr+=strlen(buffer+ptr);
      }else{
	buffer[ptr++]=chr;
      }
    }

    buffer[ptr]=0;
    return buffer;
  }

  else{
    char *buffer;
    if (malloced)
      buffer = malloc (len + 1);
    else {
      buffer = buf;
      if (len > NELEM (buf) - 1)
	len = NELEM (buf) - 1;
    }
    MEMCPY (buffer, file, len);
    buffer[len] = 0;
    return buffer;
  }
}

/* Same as low_get_program_line but returns a plain char *. It's
 * malloced if the malloced flag is set, otherwise it's a pointer to a
 * static buffer which might be clobbered by later calls.
 *
 * This function is useful when the shared string table has been freed
 * and in sensitive parts of the gc where the shared string structures
 * can't be touched. It also converts wide strings to ordinary ones
 * with escaping.
 */
PMOD_EXPORT char *low_get_program_line_plain(struct program *prog, INT32 *linep,
					     int malloced)
{
  *linep = 0;

  if (prog->linenumbers) {
    char *file;
    size_t len;
    INT32 shift;
    FIND_PROGRAM_LINE (prog, file, len, shift, (*linep));
    if (file) return make_plain_file (file, len, shift, malloced);
  }

  return NULL;
}

/* Returns the file where the program is defined. The line of the
 * class start is written to linep, or 0 if the program is the top
 * level of the file. */
PMOD_EXPORT struct pike_string *get_program_line(struct program *prog,
						 INT32 *linep)
{
  struct pike_string *res = low_get_program_line(prog, linep);
  if (!res) {
    struct pike_string *dash;
    REF_MAKE_CONST_STRING(dash, "-");
    return dash;
  }
  return res;
}

PMOD_EXPORT struct pike_string *low_get_line (PIKE_OPCODE_T *pc,
					      struct program *prog, INT32 *linep)
{
  linep[0] = 0;

  if (prog->program && prog->linenumbers) {
    ptrdiff_t offset = pc - prog->program;
    if ((offset < (ptrdiff_t)prog->num_program) && (offset >= 0)) {
      static char *file = NULL;
      static char *base, *cnt;
      static INT32 off,line,pid;
      static size_t len;
      static INT32 shift;

      if(prog->linenumbers != base || prog->id != pid || offset < off)
      {
	base = cnt = prog->linenumbers;
	off=line=0;
	pid=prog->id;
	file = 0;
      }else{
	if (cnt < prog->linenumbers + prog->num_linenumbers)
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
	  CHECK_FILE_ENTRY (prog, cnt, len, shift);
	  cnt += len<<shift;
	}
	off+=get_small_number(&cnt);
      fromold:
	if(off > offset) break;
	line+=get_small_number(&cnt);
      }
      linep[0]=line;
      if (file) {
	struct pike_string *res = begin_wide_shared_string(len, shift);
	memcpy(res->str, file, len<<shift);
	return end_shared_string(res);
      }
    } else {
      fprintf(stderr, "Bad offset: pc:%p program:%p (%p)\n",
	      pc, prog->program, (void *)prog->num_program);
    }
  } else {
    fprintf(stderr, "No program of linenumbers program:%p linenumbers:%p\n",
	    prog->program, prog->linenumbers);
  }

  return NULL;
}

/* This is to low_get_line as low_get_program_line_plain is to
 * low_get_program_line. */
PMOD_EXPORT char *low_get_line_plain (PIKE_OPCODE_T *pc, struct program *prog,
				      INT32 *linep, int malloced)
{
  linep[0] = 0;

  if (prog->program && prog->linenumbers) {
    ptrdiff_t offset = pc - prog->program;

    if ((offset < (ptrdiff_t)prog->num_program) && (offset >= 0)) {
      char *cnt = prog->linenumbers;
      INT32 off = 0, line = 0;
      char *file = NULL;
      size_t len = 0;
      INT32 shift = 0;

      while(cnt < prog->linenumbers + prog->num_linenumbers)
      {
	if(*cnt == 127)
	{
	  cnt++;
	  len = get_small_number(&cnt);
	  shift = *cnt;
	  file = ++cnt;
	  CHECK_FILE_ENTRY (prog, cnt, len, shift);
	  cnt += len<<shift;
	}
	off+=get_small_number(&cnt);
	if(off > offset) break;
	line+=get_small_number(&cnt);
      }
      linep[0]=line;

      if (file) return make_plain_file (file, len, shift, malloced);
    }
  }

  return NULL;
}

#ifdef PIKE_DEBUG
/* Variants for convenient use from a debugger. */

void gdb_program_line (struct program *prog)
{
  INT32 line;
  char *file = low_get_program_line_plain (prog, &line, 0);
  fprintf (stderr, "%s:%d\n", file, line);
}

void gdb_get_line (PIKE_OPCODE_T *pc, struct program *prog)
{
  INT32 line;
  char *file = low_get_line_plain (pc, prog, &line, 0);
  fprintf (stderr, "%s:%d\n", file, line);
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
  struct pike_string *res;

  if (prog == 0) {
    struct pike_string *unknown_program;
    REF_MAKE_CONST_STRING(unknown_program, "Unknown program");
    linep[0] = 0;
    return unknown_program;
  }

  res = low_get_line(pc, prog, linep);
  if (!res) {
    struct pike_string *not_found;
    REF_MAKE_CONST_STRING(not_found, "Line not found");
    return not_found;
  }
  return res;
}

PMOD_EXPORT struct pike_string *low_get_function_line (struct object *o,
						       int fun, INT32 *linep)
{
  if (o->prog) {
    struct program *p;
    struct identifier *id;
    struct pike_string *ret;
    while (1) {
      struct external_variable_context loc;
      struct reference *idref = o->prog->identifier_references + fun;
      p = PROG_FROM_PTR(o->prog, idref);
      id = p->identifiers + idref->identifier_offset;
      if (!IDENTIFIER_IS_ALIAS(id->identifier_flags)) break;
      loc.o = o;
      loc.inherit = INHERIT_FROM_INT(o->prog, fun);
      loc.parent_identifier = fun;
      find_external_context(&loc, id->func.ext_ref.depth);
      fun = id->func.ext_ref.id + loc.inherit->identifier_level;
      o = loc.o;
    }
    if (IDENTIFIER_IS_PIKE_FUNCTION(id->identifier_flags) &&
	(id->func.offset != -1))
      return low_get_line (p->program + id->func.offset, p, linep);
    if ((ret = get_identifier_line(o->prog, fun, linep))) {
      add_ref(ret);
      return ret;
    }
    return low_get_program_line(o->prog, linep);
  }
  *linep = 0;
  return NULL;
}

/* Return the file and line where the identifier with reference number
 * fun was defined.
 *
 * Note: Unlike the other get*line() functions, this one does not
 *       add a reference to the returned string.
 */
PMOD_EXPORT struct pike_string *get_identifier_line(struct program *p,
						    int fun, INT32 *linep)
{
  struct reference *ref = PTR_FROM_INT(p, fun);
  struct identifier *id = ID_FROM_PTR(p, ref);
  p = PROG_FROM_PTR(p, ref);
  if (id->filename_strno >= p->num_strings) return NULL;
  if (linep) *linep = id->linenumber;
  return p->strings[id->filename_strno];
}

/* Main entry point for compiler messages originating from
 * C-code.
 *
 * Sends the message along to PikeCompiler()->report().
 *
 * NOTE: The format string fmt (and vargs) is only formatted with
 *       string_builder_vsprintf() if the number of extra
 *       Pike stack arguments (args) is zero.
 *
 * NOTE: This function may be called from functions that sometimes
 *       execute outside of the compilation context, eg by
 *       __handle_sprintf_format(), which can be called directly
 *       by Pike-code, in which case it is a NO-OP.
 */
PMOD_EXPORT void va_yyreport(int severity_level,
			     struct pike_string *file, INT32 line,
			     struct pike_string *system, INT32 args,
			     const char *fmt, va_list vargs)
{
  struct compilation *c = MAYBE_THIS_COMPILATION;
  struct string_builder s;

  if (!c) {
    /* No compiler context. */
    pop_n_elems(args);
    return;
  }

  STACK_LEVEL_START(args);

#ifdef PIKE_DEBUG
  if(Pike_interpreter.recoveries && Pike_sp-Pike_interpreter.evaluator_stack < Pike_interpreter.recoveries->stack_pointer)
    Pike_fatal("Stack error (underflow)\n");
#endif

  /* Convert type errors to warnings in non-strict compat mode. */
  if ((system == type_check_system_string) &&
      (severity_level == REPORT_ERROR) &&
      (Pike_compiler->compat_major != -1) &&
      !(c->lex.pragmas & ID_STRICT_TYPES) &&
      ((Pike_compiler->compat_major < PIKE_MAJOR_VERSION) ||
       ((Pike_compiler->compat_major == PIKE_MAJOR_VERSION) &&
	(Pike_compiler->compat_minor < PIKE_MINOR_VERSION)))) {
    severity_level = REPORT_WARNING;
  }

  /* If we have parse errors we might get erroneous warnings,
   * so don't print them.
   * This has the additional benefit of making it easier to
   * visually locate the actual error message.
   */
  if ((severity_level <= REPORT_WARNING) &&
      Pike_compiler->num_parse_error) {
    pop_n_elems(args);
    return;
  }

  if (severity_level >= REPORT_ERROR) {
    if (Pike_compiler->num_parse_error > 20) {
      pop_n_elems(args);
      return;
    }
    Pike_compiler->num_parse_error++;
    cumulative_parse_error++;
  }

  push_int(severity_level);
  ref_push_string(file?file:c->lex.current_file);
  push_int(line?line:c->lex.current_line);
  ref_push_string(system);
  if (args) {
    int i = args;
    push_text(fmt);
    /* Copy the arguments. */
    while (i--) {
      push_svalue(Pike_sp-(args+5));
    }
  } else {
    init_string_builder(&s, 0);
    string_builder_vsprintf(&s, fmt, vargs);
    push_string(finish_string_builder(&s));
  }

  safe_apply_current(PC_REPORT_FUN_NUM, args + 5);
  pop_stack();
  if (args) pop_n_elems(args);
  STACK_LEVEL_DONE(0);
}

PMOD_EXPORT void low_yyreport(int severity_level,
			      struct pike_string *file, INT32 line,
			      struct pike_string *system,
			      INT32 args, const char *fmt, ...)
{
  va_list vargs;

  va_start(vargs,fmt);
  va_yyreport(severity_level, file, line, system, args, fmt, vargs);
  va_end(vargs);
}

PMOD_EXPORT void yyreport(int severity_level, struct pike_string *system,
			  INT32 args, const char *fmt, ...)
{
  va_list vargs;

  va_start(vargs,fmt);
  va_yyreport(severity_level, NULL, 0, system, args, fmt, vargs);
  va_end(vargs);
}

PMOD_EXPORT void yywarning(char *fmt, ...)
{
  va_list vargs;

  va_start(vargs,fmt);
  va_yyreport(REPORT_WARNING, NULL, 0, parser_system_string, 0, fmt, vargs);
  va_end(vargs);
}

/* FIXME: Consider converting these to using va_yyreport(). */

PMOD_EXPORT void my_yyerror(const char *fmt,...)
{
  va_list vargs;
  va_start(vargs,fmt);
  va_yyreport(REPORT_ERROR, NULL, 0, parser_system_string, 0, fmt, vargs);
  va_end(vargs);
}

PMOD_EXPORT void yyerror(const char *str)
{
  my_yyerror("%s", str);
}

/* Main entry point for errors from the type-checking subsystems.
 *
 * The message (if any) will be formatted for the same source
 * position as the got_t.
 *
 * The expect_t will be formatted for the same position as the got_t
 * if there's no expected_file/expected_line.
 *
 * The got_t will be formatted for the current lex position if there's
 * no got_file/got_line.
 */
void yytype_report(int severity_level,
		   struct pike_string *expected_file, INT32 expected_line, 
		   struct pike_type *expected_t,
		   struct pike_string *got_file, INT32 got_line,
		   struct pike_type *got_t,
		   INT32 args, const char *fmt, ...)
{
  if (fmt)
  {
    va_list vargs;
    va_start(vargs, fmt);
    va_yyreport(severity_level, got_file, got_line, type_check_system_string,
		args, fmt, vargs);
    va_end(vargs);
  }

  if (expected_t && got_t) {
    yyexplain_nonmatching_types(severity_level,
				expected_file?expected_file:got_file,
				expected_line?expected_line:got_line,
				expected_t,
				got_file, got_line, got_t);
  } else if (expected_t) {
    ref_push_type_value(expected_t);
    low_yyreport(severity_level,
		 expected_file?expected_file:got_file,
		 expected_line?expected_line:got_line,
		 type_check_system_string,
		 1, "Expected: %O.");
  } else if (got_t) {
    ref_push_type_value(got_t);
    low_yyreport(severity_level, got_file, got_line, type_check_system_string,
		 1, "Got     : %O.");
  }
}

void yytype_error(const char *msg, struct pike_type *expected_t,
		  struct pike_type *got_t, unsigned int flags)
{
  yytype_report((flags & YYTE_IS_WARNING)?REPORT_WARNING:REPORT_ERROR,
		NULL, 0, expected_t, NULL, 0, got_t, 0, "%s", msg);
}

struct pike_string *format_exception_for_error_msg (struct svalue *thrown)
{
  struct pike_string *s = NULL;

  push_svalue (thrown);
  SAFE_APPLY_MASTER ("describe_error", 1);

  if (sp[-1].type == T_STRING) {
    f_string_trim_all_whites(1);
    push_constant_text("\n");
    push_constant_text(" ");
    f_replace(3);
    return (--sp)->u.string;
  }
  else {
    pop_stack();
    return NULL;
  }
}

void handle_compile_exception (const char *yyerror_fmt, ...)
{
  struct svalue thrown;
  struct compilation *c = THIS_COMPILATION;

  CHECK_COMPILER();

  move_svalue (&thrown, &throw_value);
  mark_free_svalue (&throw_value);

  if (yyerror_fmt) {
    va_list args;
    va_start (args, yyerror_fmt);
    va_yyreport(REPORT_ERROR, NULL, 0, parser_system_string, 0,
		yyerror_fmt, args);
    va_end (args);
  }

  push_svalue(&thrown);
  /* safe_apply_current(PC_FILTER_EXCEPTION_FUN_NUM, 1); */
  low_safe_apply_handler("compile_exception", c->handler, c->compat_handler, 1);

  if (SAFE_IS_ZERO(sp-1)) {
    struct pike_string *s = format_exception_for_error_msg (&thrown);
    if (s) {
      push_string(s);
      yyreport(REPORT_ERROR, parser_system_string, 1, "%s");
    }
  }

  pop_stack();
  free_svalue(&thrown);
}

extern void yyparse(void);

#ifdef PIKE_DEBUG
#define do_yyparse() do {				\
  struct svalue *save_sp=Pike_sp;			\
  yyparse();  /* Parse da program */			\
  if(save_sp != Pike_sp) {				\
    Pike_fatal("yyparse() left %"PRINTPTRDIFFT"d droppings on the stack!\n", \
	  Pike_sp - save_sp);				\
  }							\
}while(0)
#else
#define do_yyparse() yyparse()
#endif

struct Supporter *current_supporter=0;


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
    Pike_fatal("This is not a supporter (addr=%p, magic=%x)!\n",s,s->magic);
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
    Pike_fatal("Que, numbers out of whack1\n");

  if(s->depends_on && SNUM(s->depends_on) <= m->level)
    Pike_fatal("Que, numbers out of whack2\n");

  for(ss=s->dependants;ss;ss=ss->next_dependant) {
    if (ss->depends_on != s)
      Pike_fatal("Dependant hasn't got depends_on set properly.\n");
    if(SNUM(ss) >= m->level)
      Pike_fatal("Que, numbers out of whack3\n");
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
  CDFPRINTF((stderr, "th(%ld) init_supporter() supporter=%p data=%p.\n",
	     (long) th_self(), s, data));
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
    Pike_fatal("Previous unlink failed.\n");
#endif
  debug_malloc_touch(c);
  verify_supporters();
  if(c->depends_on)
  {
#ifdef PIKE_DEBUG
    struct Supporter *s;
    for (s = c->depends_on->dependants; s; s = s->next_dependant)
      if (s == c) Pike_fatal("Dependant already linked in.\n");
#endif
    ret++;
    c->next_dependant = c->depends_on->dependants;
    c->depends_on->dependants=c;
    add_ref(c->self);
    CDFPRINTF((stderr, "th(%ld) unlink_current_supporter() "
	       "supporter=%p (prog %p) depends on %p (prog %p).\n",
	       (long) th_self(), c, c->prog,
	       c->depends_on, c->depends_on->prog));
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
  CDFPRINTF((stderr, "th(%ld) call_dependants() supporter=%p (prog %p) "
	     "finish=%d.\n", (long) th_self(), s, s->prog, finish));
  verify_supporters();
  while((tmp=s->dependants))
  {
    CDFPRINTF((stderr, "th(%ld) dependant: %p (prog %p) (data:%p).\n",
	       (long) th_self(), tmp, tmp->prog, tmp->data));
    s->dependants=tmp->next_dependant;
#ifdef PIKE_DEBUG
    tmp->next_dependant=0;
#endif
    verify_supporters();
    if (!tmp->fun(tmp->data, finish)) ok = 0;
    verify_supporters();
    free_object(tmp->self);
  }
  return ok;
}

int report_compiler_dependency(struct program *p)
{
  int ret=0;
  struct Supporter *c,*cc;

  if (p == Pike_compiler->new_program) {
    /* Depends on self... */
    return 0;
  }

  CDFPRINTF((stderr, "th(%ld) compiler dependency on %p from %p\n",
	     (long)th_self(), p, Pike_compiler->new_program));

  verify_supporters();
  if (Pike_compiler->flags & COMPILATION_FORCE_RESOLVE)
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
	  CDFPRINTF ((stderr, "th(%ld) supporter %p (prog %p) "
		      "now depends on %p (prog %p)\n",
		      (long) th_self(), cc, cc->prog, c, c->prog));
	  verify_supporters();
	  ret++; /* dependency registred */
	}
      }
    }
  }
  verify_supporters();
  return ret;
}

/*! @module DefaultCompilerEnvironment
 *!
 *!   The @[CompilerEnvironment] object that is used
 *!   for loading C-modules and by @[predef::compile()].
 *!
 *! @note
 *!   @[predef::compile()] is essentially an alias for the
 *!   @[CompilerEnvironment()->compile()] in this object.
 *!
 *! @seealso
 *!   @[CompilerEnvironment], @[predef::compile()]
 */

/*! @endmodule
 */

/*! @class CompilerEnvironment
 *!
 *!   The compiler environment.
 *!
 *!   By inheriting this class and overloading the functions,
 *!   it is possible to make a custom Pike compiler.
 *!
 *! @note
 *!   Prior to Pike 7.8 this sort of customization has to be done
 *!   either via custom master objects, or via @[CompilationHandler]s.
 *!
 *! @seealso
 *!   @[CompilationHandler], @[MasterObject], @[master()], @[replace_master()]
 */

/*! @decl enum SeverityLevel
 *!   Message severity level.
 *! { NOTICE, WARNING, ERROR, FATAL }
 *!
 *! @constant NOTICE
 *! @constant WARNING
 *! @constant ERROR
 *! @constant FATAL
 *!
 *! @seealso
 *!   @[report()]
 */

/*! @decl void report(SeverityLevel severity, @
 *!                   string filename, int linenumber, @
 *!                   string subsystem, @
 *!                   string message, mixed ... extra_args)
 *!
 *!   Report a diagnostic from the compiler.
 *!
 *! @param severity
 *!   The severity of the diagnostic.
 *!
 *!   The default implementation does the following depending on @[severity]:
 *!   @int
 *!     @value NOTICE
 *!       Ignored.
 *!     @value WARNING
 *!       Calls @[MasterObject()->compile_warning()].
 *!     @value ERROR
 *!     @value FATAL
 *!       Calls @[MasterObject()->compile_error()].
 *!   @endint
 *!
 *!   If there's no master object yet, the diagnostic is output to
 *!   @[Stdio.stderr].
 *!
 *! @param filename
 *! @param linenumber
 *!   Location which triggered the diagnostic.
 *!
 *! @param subsystem
 *!   Compiler subsystem that generated the diagnostic.
 *!
 *! @param message
 *!   @[sprintf()]-style formatting string with the diagnostic message.
 *!
 *! @param extra_args
 *!   Extra arguments to @[sprintf()].
 *!
 *! @seealso
 *!   @[PikeCompiler()->report()]
 */
static void f_compilation_env_report(INT32 args)
{
  int level;
  struct pike_string *filename;
  INT_TYPE linenumber;
  struct pike_string *subsystem;
  struct pike_string *message;

  if (args > 5) {
    f_sprintf(args - 4);
    args = 5;
  }
  get_all_args("report", args, "%d%W%i%W%W",
	       &level, &filename, &linenumber, &subsystem, &message);

  /* Ignore informational level messages */
  if (level >= REPORT_WARNING) {
    if (get_master()) {
      ref_push_string(filename);
      push_int(linenumber);
      ref_push_string(message);
      if (level >= REPORT_ERROR) {
	APPLY_MASTER("compile_error", 3);
	args++;
      } else {
	APPLY_MASTER("compile_warning", 3);
	args++;
      }
    } else {
      /* We hope that errors from compiling the master
       * won't contain wide-strings... */
      if (level >= REPORT_ERROR) {
	fprintf(stderr, "%s:%ld: %s\n",
		filename->str, linenumber, message->str);
      } else {
	fprintf(stderr, "%s:%ld: Warning: %s\n",
		filename->str, linenumber, message->str);
      }
      fflush(stderr);
    }
  }
  pop_n_elems(args);
  push_int(0);
}

/*! @decl program compile(string source, CompilationHandler|void handler, @
 *!                       int|void major, int|void minor,@
 *!                       program|void target, object|void placeholder)
 *!
 *!   Compile a string to a program.
 *!
 *!   This function takes a piece of Pike code as a string and
 *!   compiles it into a clonable program.
 *!
 *!   The optional argument @[handler] is used to specify an alternative
 *!   error handler. If it is not specified the current master object will
 *!   be used.
 *!
 *!   The optional arguments @[major] and @[minor] are used to tell the
 *!   compiler to attempt to be compatible with Pike @[major].@[minor].
 *!
 *! @note
 *!   This function essentially performs
 *!   @code
 *!     program compile(mixed ... args)
 *!     {
 *!       return PikeCompiler(@@args)->compile();
 *!     }
 *!   @endcode
 *!
 *! @note
 *!   Note that @[source] must contain the complete source for a program.
 *!   It is not possible to compile a single expression or statement.
 *!
 *!   Also note that @[compile()] does not preprocess the program.
 *!   To preprocess the program you can use @[compile_string()] or
 *!   call the preprocessor manually by calling @[cpp()].
 *!
 *! @seealso
 *!   @[compile_string()], @[compile_file()], @[cpp()], @[master()],
 *!   @[CompilationHandler]
 */
static void f_compilation_env_compile(INT32 args)
{
  apply_current(CE_PIKE_COMPILER_FUN_NUM, args);
  args = 1;
  if (Pike_sp[-1].type != T_OBJECT) {
    Pike_error("Bad return value from PikeCompiler().\n");
  }
  apply(Pike_sp[-1].u.object, "compile", 0);
  stack_pop_n_elems_keep_top(args);
}

/*! @decl mixed resolv(string identifier, string filename, @
 *!		       object|void handler)
 */
static void f_compilation_env_resolv(INT32 args)
{
  struct pike_string *ident;
  struct pike_string *filename;
  struct object *handler = NULL;

  get_all_args("resolv", args, "%W%W.%O",
	       &ident, &filename, &handler);

  if(get_master())
  {
    DECLARE_CYCLIC();
    if(BEGIN_CYCLIC(ident, filename))
    {
      my_yyerror("Recursive module dependency in %S.", ident);
    }else{
      SET_CYCLIC_RET(1);

      APPLY_MASTER("resolv", args);
    }
    END_CYCLIC();
  } else {
    pop_n_elems(args);
    push_undefined();
  }
}

/*! @decl object get_compilation_handler(int major, int minor)
 *!
 *!   Get compatibility handler for Pike @[major].@[minor].
 *!
 *!   The default implementation calls the corresponding
 *!   function in the master object.
 *!
 *! @note
 *!   This function is typically called by
 *!   @[PikeCompiler()->get_compilation_handler()].
 *!
 *! @seealso
 *!   @[MasterObject()->get_compilation_handler()].
 */
static void f_compilation_env_get_compilation_handler(INT32 args)
{
  if(get_master())
  {
    APPLY_MASTER("get_compilation_handler", args);
  } else {
    pop_n_elems(args);
    push_undefined();
  }
}

/*! @decl mapping(string:mixed)|object get_default_module()
 *!
 *!   Get the default module for the current compatibility level
 *!   (ie typically the value returned by @[predef::all_constants()]).
 *!
 *!   The default implementation calls the corresponding function
 *!   in the master object.
 *!
 *! @returns
 *!   @mixed
 *!     @type mapping(string:mixed)|object
 *!       Constant table to use.
 *!
 *!     @type int(0..0)
 *!       Use the builtin constant table.
 *!   @endmixed
 *!
 *! @note
 *!   This function is typically called by 
 *!   @[Pike_compiler()->get_default_module()].
 *!
 *! @seealso
 *!   @[MasterObject()->get_default_module()].
 */
static void f_compilation_env_get_default_module(INT32 args)
{
  if(get_master())
  {
    APPLY_MASTER("get_default_module", args);
  } else {
    pop_n_elems(args);
    push_undefined();
  }
}

/*! @decl program handle_inherit(string inh, string current_file, @
 *!                              object|void handler)
 *!
 *!   Look up an inherit @[inh].
 *!
 *!   The default implementation calls the corresponding function
 *!   in the master object.
 *!
 *! @seealso
 *!   @[MasterObject()->handle_inherit()].
 */
static void f_compilation_env_handle_inherit(INT32 args)
{
  if(get_master())
  {
    APPLY_MASTER("handle_inherit", args);
  } else {
    pop_n_elems(args);
    push_undefined();
  }
}

/*! @decl int filter_exception(SeverityLevel level, mixed err)
 *!
 *!   The default implementation calls
 *!   @[MasterObject()->compile_exception()] for @[level] @[ERROR]
 *!   and @[FATAL].
 *!
 *! @note
 *!   This function is not implemented in Pike 7.8.
 *!
 *! @seealso
 *!   @[MasterObject()->compile_exception()].
 */
static void f_compilation_env_filter_exception(INT32 args)
{
  int level;
  struct svalue *err;

  get_all_args("filter_exception", args, "%d%*", &level, &err);
  if (args > 2) {
    pop_n_elems(args-2);
    args = 2;
  }

#if 0
  if (level >= REPORT_WARNING) {
    if (level >= REPORT_ERROR) {
      APPLY_MASTER("compile_exception", 1);
      /* FIXME! */
    } else {
      push_int(level);
      push_string(format_exception_for_error_msg(err));
      /* FIXME! */
    }
  }
#endif

  pop_n_elems(args);
  push_undefined();
  return;
}

/*! @class PikeCompiler
 *!
 *!   The Pike compiler.
 *!
 *!   An object of this class compiles a single string
 *!   of Pike code.
 */

static void free_compilation(struct compilation *c)
{
  debug_malloc_touch(c);
  if (c->prog) {
    free_string(c->prog);
    c->prog = NULL;
  }
  if(c->handler) {
    free_object(c->handler);
    c->handler = NULL;
  }
  if(c->compat_handler) {
    free_object(c->compat_handler);
    c->compat_handler = NULL;
  }
  if(c->target) {
    free_program(c->target);
    c->target = NULL;
  }
  if(c->p) {
    free_program(c->p);
    c->p = NULL;
  }
  if(c->placeholder) {
    free_object(c->placeholder);
    c->placeholder = NULL;
  }
  if(c->lex.current_file) {
    free_string(c->lex.current_file);
    c->lex.current_file = NULL;
  }
  free_svalue(& c->default_module);
  c->default_module.type = T_INT;
  c->default_module.subtype = NUMBER_NUMBER;
  free_supporter(&c->supporter);
  verify_supporters();
}

static void run_init(struct compilation *c)
{
  debug_malloc_touch(c);

  if (c->compat_handler) free_object(c->compat_handler);
  c->compat_handler=0;

  c->resolve_cache_save = resolve_cache;
  resolve_cache = 0;

  c->lex.current_line=1;
  free_string(c->lex.current_file);
  c->lex.current_file=make_shared_string("-");

  if (runtime_options & RUNTIME_STRICT_TYPES)
  {
    c->lex.pragmas = ID_STRICT_TYPES;
  } else {
    c->lex.pragmas = 0;
  }

  c->lex.end = c->prog->str + (c->prog->len << c->prog->size_shift);

  switch(c->prog->size_shift)
  {
    case 0: c->lex.current_lexer = yylex0; break;
    case 1: c->lex.current_lexer = yylex1; break;
    case 2: c->lex.current_lexer = yylex2; break;
#ifdef PIKE_DEBUG
    default:
      Pike_fatal("Program has bad shift %d!\n", c->prog->size_shift);
      break;
#endif
  }

  c->lex.pos=c->prog->str;
}

static void run_init2(struct compilation *c)
{
#if 0
  int i;
  struct program *p;
  struct reference *refs;
#endif /* 0 */
  debug_malloc_touch(c);
  Pike_compiler->compiler = c;

  /* Get the proper default module. */
  safe_apply_current2(PC_GET_DEFAULT_MODULE_FUN_NUM, 0, NULL);
  if(Pike_sp[-1].type == T_INT)
  {
    pop_stack();
    ref_push_mapping(get_builtin_constants());
  }
  assign_svalue(&c->default_module, Pike_sp-1);
  pop_stack();

  use_module(& c->default_module);

  Pike_compiler->compat_major=PIKE_MAJOR_VERSION;
  Pike_compiler->compat_minor=PIKE_MINOR_VERSION;

  if(c->major>=0)
    change_compiler_compatibility(c->major, c->minor);

#if 0
  /* Make all inherited private symbols that weren't overloaded
   * in the first pass local.
   */
  p = c->new_program;
  i = p->num_identifier_references;
  refs = p->identifier_references;
  while (i--) {
    if (refs[i].id_flags & ID_PRIVATE) refs[i].id_flags |= ID_INLINE;
  }
#endif /* 0 */
}

static void run_exit(struct compilation *c)
{
  debug_malloc_touch(c);

#ifdef PIKE_DEBUG
  if(c->num_used_modules)
    Pike_fatal("Failed to pop modules properly.\n");
#endif

#ifdef PIKE_DEBUG
  if (c->compilation_depth != -1) {
    fprintf(stderr, "compile(): compilation_depth is %d\n",
	    c->compilation_depth);
  }
#endif /* PIKE_DEBUG */

  if (resolve_cache)
    free_mapping(resolve_cache);
  resolve_cache = c->resolve_cache_save;

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

/* NOTE: Must not throw errors! */
static int run_pass1(struct compilation *c)
{
  int ret=0;

  debug_malloc_touch(c);
  run_init(c);

#if 0
  CDFPRINTF((stderr, "th(%ld) compile() starting compilation_depth=%d\n",
	     (long)th_self(), c->compilation_depth));
#endif

  if(c->placeholder && c->placeholder->prog != null_program) {
    yyerror("Placeholder object is not a null_program clone!");
    return 0;
  }
  debug_malloc_touch(c->placeholder);

  if(c->target && !(c->target->flags & PROGRAM_VIRGIN)) {
    yyerror("Placeholder program is not virgin!");
    return 0;
  }

  low_start_new_program(c->target,1,0,0,0);
  c->supporter.prog = Pike_compiler->new_program;

  CDFPRINTF((stderr,
	     "th(%ld) %p run_pass1() start: "
	     "threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), Pike_compiler->new_program,
	     threads_disabled, c->compilation_depth));

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
  CDFPRINTF((stderr, "th(%ld) %p compile(): First pass\n",
	     (long)th_self(), Pike_compiler->new_program));
#endif

  do_yyparse();  /* Parse da program */

  if (!Pike_compiler->new_program->num_linenumbers) {
    /* The lexer didn't write an initial entry. */
    store_linenumber(0, c->lex.current_file);
#ifdef DEBUG_MALLOC
    if(strcmp(c->lex.current_file->str,"-"))
      debug_malloc_name(Pike_compiler->new_program, c->lex.current_file->str, 0);
#endif
  }

  CDFPRINTF((stderr, "th(%ld) %p run_pass1() done for %s\n",
	     (long)th_self(), Pike_compiler->new_program,
	     c->lex.current_file->str));

  ret=unlink_current_supporter(& c->supporter);

  c->p=debug_malloc_pass(end_first_pass(0));

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
	Pike_fatal("Placeholder object got wrong program after first pass.\n");
#endif
      debug_malloc_touch(c->placeholder);
      c->placeholder->storage=c->p->storage_needed ?
	(char *)xcalloc(c->p->storage_needed, 1) :
	(char *)NULL;
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

  if (!c->p) {
    c->flags &= ~(COMPILER_BUSY);
    c->flags |= COMPILER_DONE;
    return;
  }

  run_init(c);
  low_start_new_program(c->p,2,0,0,0);
  free_program(c->p);
  c->p=0;

  run_init2(c);

  CDFPRINTF((stderr,
	     "th(%ld) %p run_pass2() start: "
	     "threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), Pike_compiler->new_program,
	     threads_disabled, c->compilation_depth));

  verify_supporters();

  do_yyparse();  /* Parse da program */

  CDFPRINTF((stderr, "th(%ld) %p run_pass2() done for %s\n",
	     (long)th_self(), Pike_compiler->new_program,
	     c->lex.current_file->str));

  verify_supporters();

  c->p=debug_malloc_pass(end_program());

  run_exit(c);
}

static void run_cleanup(struct compilation *c, int delayed)
{
  debug_malloc_touch(c);
  debug_malloc_touch(c->placeholder);
#if 0 /* FIXME */
  if (threads_disabled != c->saved_threads_disabled) {
    Pike_fatal("compile(): threads_disabled:%d saved_threads_disabled:%d\n",
	  threads_disabled, c->saved_threads_disabled);
  }
#endif /* PIKE_DEBUG */

  exit_threads_disable(NULL);

  CDFPRINTF((stderr,
	     "th(%ld) %p run_cleanup(): "
	     "threads_disabled:%d, compilation_depth:%d\n",
	     (long)th_self(), c->target,
	     threads_disabled, c->compilation_depth));
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
	  p->constants[i].sval.subtype = NUMBER_NUMBER;
	}
      }

      /* We have to notify the master object that
       * a previous compile() actually failed, even
       * if we did not know it at the time
       */
      CDFPRINTF((stderr, "th(%ld) %p unregistering failed delayed compile.\n",
		 (long) th_self(), p));
      ref_push_program(p);
      /* FIXME: Shouldn't the compilation handler be used here? */
      SAFE_APPLY_MASTER("unregister",1);
      pop_stack();

      {
#ifdef PIKE_DEBUG
	int refs = p->refs;
#endif

	/* Free the target here to avoid false alarms in the debug
	 * check below. */
	free_program (c->target);
	c->target = NULL;

#ifdef PIKE_DEBUG
	if (refs > 1) {
	  /* Other programs can have indexed out constants from p, which
	   * might be broken themselves and/or keep references to p
	   * through the parent pointer. We should find all those other
	   * programs and invalidate them too, but how can that be done?
	   * The whole delayed compilation thingie is icky icky icky... :P
	   * /mast */
	  fprintf(stderr, "Warning: Program %p still got %d "
		  "external refs after unregister:\n", p, p->refs);
	  locate_references(p);
	  fprintf (stderr, "Describing program:\n");
	  describe_something (p, T_PROGRAM, 0, 0, 0, NULL);
	}
#endif
      }
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
	  Pike_fatal("Placeholder object got wrong program after second pass.\n");
#endif
	if(SETJMP(rec))
	{
	  handle_compile_exception (NULL);
	  debug_malloc_touch(c->placeholder);
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
  c->flags &= ~(COMPILER_BUSY);
  c->flags |= COMPILER_DONE;
}

static int call_delayed_pass2(struct compilation *cc, int finish)
{
  int ok = 0;
  debug_malloc_touch(cc);

  debug_malloc_touch(cc->p);

  CDFPRINTF((stderr, "th(%ld) %p %s delayed compile.\n",
	     (long) th_self(), cc->p, finish ? "continuing" : "cleaning up"));

  /* Reenter the delayed compilation. */
  add_ref(cc->supporter.self);
  low_enter_compiler(cc->supporter.self, cc->compilation_inherit);

  if(finish && cc->p) run_pass2(cc);
  run_cleanup(cc,1);

  exit_compiler();
  
  debug_malloc_touch(cc);

#ifdef PIKE_DEBUG
  if(cc->supporter.dependants)
    Pike_fatal("Que???\n");
#endif
  if(cc->p) {
    ok = finish;
    free_program(cc->p); /* later */
    cc->p = NULL;
  }

  CDFPRINTF((stderr, "th(%ld) %p delayed compile %s.\n",
	     (long) th_self(), cc->target, ok ? "done" : "failed"));

  verify_supporters();

  return ok;
}

static void compilation_event_handler(int e)
{
  struct compilation *c = THIS_COMPILATION;

  switch (e) {
  case PROG_EVENT_INIT:
    CDFPRINTF((stderr, "th(%ld) compilation: INIT(%p).\n",
	       (long) th_self(), c));
    MEMSET(c, 0, sizeof(*c));
    c->supporter.self = Pike_fp->current_object; /* NOTE: Not ref-counted! */
    c->compilation_inherit =
      Pike_fp->context - Pike_fp->current_object->prog->inherits;
    initialize_buf(&c->used_modules);
    add_ref(c->default_module.u.mapping = get_builtin_constants());
    c->default_module.type = T_MAPPING;
    c->major = -1;
    c->minor = -1;
    c->lex.current_line = 1;
    c->lex.current_file = make_shared_string("-");
    c->compilation_depth = -1;
    break;
  case PROG_EVENT_EXIT:
    CDFPRINTF((stderr, "th(%ld) compilation: EXIT(%p).\n",
	       (long) th_self(), c));
    toss_buffer(&c->used_modules);
    free_compilation(c);
    break;
  }
}

/*! @decl void report(SeverityLevel severity, @
 *!                   string filename, int linenumber, @
 *!                   string subsystem, @
 *!                   string message, mixed ... extra_args)
 *!
 *!   Report a diagnostic from the compiler.
 *!
 *!   The default implementation calls the corresponding function
 *!   in the active handlers, and otherwise falls back to
 *!   @[CompilerEnvironment()->report()] in the parent object.
 */
static void f_compilation_report(INT32 args)
{
  struct compilation *c = THIS_COMPILATION;
  int level;
  struct pike_string *filename;
  INT_TYPE linenumber;
  struct pike_string *subsystem;
  struct pike_string *message;
  struct object *handler = NULL;
  int fun = -1;

  /* FIXME: get_all_args() ought to have a marker
   *        indicating that we accept more arguments...
   */
  get_all_args("report", args, "%d", &level);

  if ((c->handler || c->compat_handler) &&
      (level >= REPORT_WARNING)) {
    /* Ignore informational level messages */
    const char *fun_name = "compile_warning";

    if (level >= REPORT_ERROR) fun_name = "compile_error";

    if((handler = c->handler) && handler->prog) {
      if ((fun = find_identifier(fun_name, handler->prog)) !=
	  -1) {
	goto apply_handler;
      }
    }
    if ((handler = c->compat_handler) && handler->prog) {
      if ((fun = find_identifier(fun_name, handler->prog)) !=
	  -1) {
	goto apply_handler;
      }
    }
  }
  /* Nothing apropriate in any handlers.
   * Call the report() in our parent.
   */
  apply_external(1, CE_REPORT_FUN_NUM, args);
  return;

 apply_handler:
  if (args > 5) {
    f_sprintf(args - 4);
    args = 5;
  }
  get_all_args("report", args, "%d%W%i%W%W",
	       &level, &filename, &linenumber,
	       &subsystem, &message);

  ref_push_string(filename);
  push_int(linenumber);
  ref_push_string(message);
  apply_low(handler, fun, 3);
  stack_pop_n_elems_keep_top(args);
}

/*! @decl void create(string|void source, @
 *!                   CompilationHandler|void handler, @
 *!                   int|void major, int|void minor,@
 *!                   program|void target, object|void placeholder)
 *!
 *!   Create a PikeCompiler object for a source string.
 *!
 *!   This function takes a piece of Pike code as a string and
 *!   initializes a compiler object accordingly.
 *!
 *! @param source
 *!   Source code to compile.
 *!
 *! @param handler
 *!   The optional argument @[handler] is used to specify an alternative
 *!   error handler. If it is not specified the current master object
 *!   at compile time will be used.
 *!
 *! @param major
 *! @param minor
 *!   The optional arguments @[major] and @[minor] are used to tell the
 *!   compiler to attempt to be compatible with Pike @[major].@[minor].
 *!
 *! @param target
 *!   @[__empty_program()] program to fill in. The virgin program
 *!   returned by @[__empty_program()] will be modified and returned
 *!   by @[compile()] on success.
 *!
 *! @param placeholder
 *!   @[__null_program()] placeholder object to fill in. The object
 *!   will be modified into an instance of the resulting program
 *!   on successfull compile. Note that @[lfun::create()] in the
 *!   program will be called without any arguments.
 *!
 *! @note
 *!   Note that @[source] must contain the complete source for a program.
 *!   It is not possible to compile a single expression or statement.
 *!
 *!   Also note that no preprocessing is performed.
 *!   To preprocess the program you can use @[compile_string()] or
 *!   call the preprocessor manually by calling @[cpp()].
 *!
 *! @note
 *!   Note that all references to @[target] and @[placeholder] should
 *!   removed if @[compile()] failes. On failure the @[placeholder]
 *!   object will be destructed.
 *!
 *! @seealso
 *!   @[compile_string()], @[compile_file()], @[cpp()], @[master()],
 *!   @[CompilationHandler]
 */
static void f_compilation_create(INT32 args)
{
  struct pike_string *aprog = NULL;
  struct object *ahandler = NULL;/* error handler */
  int amajor = -1;
  int aminor = -1;
  struct program *atarget = NULL;
  struct object *aplaceholder = NULL;
  int dependants_ok = 1;
  struct compilation *c = THIS_COMPILATION;

  if (c->flags & COMPILER_BUSY) {
    Pike_error("PikeCompiler object is in use.\n");
  }

  STACK_LEVEL_START(args);

  get_all_args("create", args, ".%W%O%d%d%P%O",
	       &aprog, &ahandler,
	       &amajor, &aminor,
	       &atarget, &aplaceholder);

  if (args == 3) {
    SIMPLE_BAD_ARG_ERROR("create", 4, "int");
  }

  check_c_stack(65536);

  CDFPRINTF((stderr, "th(%ld) %p compilation create() enter, placeholder=%p\n",
	     (long) th_self(), atarget, aplaceholder));

  debug_malloc_touch(c);

  verify_supporters();

  c->flags &= ~COMPILER_DONE;

  if (c->p) free_program(c->p);
  c->p = NULL;

  if (c->prog) free_string(c->prog);
  if ((c->prog=aprog)) add_ref(aprog);

  if (c->handler) free_object(c->handler);
  if ((c->handler=ahandler)) add_ref(ahandler);

  if (c->target) free_program(c->target);
  if ((c->target=atarget)) add_ref(atarget);

  if (c->placeholder) free_object(c->placeholder);
  if ((c->placeholder=aplaceholder)) add_ref(aplaceholder);

  c->major = amajor?amajor:-1;
  c->minor = aminor?aminor:-1;

  STACK_LEVEL_DONE(args);
  pop_n_elems(args);

  push_int(0);
}

/*! @decl program compile()
 *!
 *!   Compile the current source into a program.
 *!
 *!   This function compiles the current Pike source code
 *!   into a clonable program.
 *!
 *! @seealso
 *!   @[compile_string()], @[compile_file()], @[cpp()], @[master()],
 *!   @[CompilationHandler], @[create()]
 */
static void f_compilation_compile(INT32 args)
{
  int delay, dependants_ok = 1;
  struct program *ret;
#ifdef PIKE_DEBUG
  ONERROR tmp;
#endif
  struct compilation *c = THIS_COMPILATION;

  if (c->flags & COMPILER_BUSY) {
    Pike_error("PikeCompiler in use.\n");
  }

  get_all_args("compile", args, "");

  check_c_stack(65536);

  CDFPRINTF((stderr, "th(%ld) %p f_compilation_compile() enter, "
	     "placeholder=%p\n", (long) th_self(), c->target, c->placeholder));

  debug_malloc_touch(c);

  verify_supporters();

  if (c->flags & COMPILER_DONE) {
    /* Already compiled. */
    pop_n_elems(args);
    if (c->p) ref_push_program(c->p);
    else push_int(0);
    return;
  }

  if (!c->prog) {
    /* No program text. */
    low_start_new_program(c->target, 1, NULL, 0, NULL);
    c->p = end_program();
    c->flags |= COMPILER_DONE;
    pop_n_elems(args);
    ref_push_program(c->p);
    return;
  }

#ifdef PIKE_DEBUG
  SET_ONERROR(tmp, fatal_on_error,"Compiler exited with longjump!\n");
#endif

  c->flags |= COMPILER_BUSY;

  low_init_threads_disable();
  c->saved_threads_disabled = threads_disabled;

  init_supporter(& c->supporter,
		 (supporter_callback *) call_delayed_pass2,
		 (void *)c);

  delay=run_pass1(c);
  dependants_ok = call_dependants(& c->supporter, !!c->p );
#ifdef PIKE_DEBUG
  /* FIXME */
  UNSET_ONERROR(tmp);
#endif

  if(delay)
  {
    CDFPRINTF((stderr, "th(%ld) %p f_compilation_compile() finish later, "
	       "placeholder=%p.\n",
	       (long) th_self(), c->target, c->placeholder));
    /* finish later */
    verify_supporters();
    /* We're hanging in the supporter. */
    ret = debug_malloc_pass(c->p);
  }else{
    CDFPRINTF((stderr, "th(%ld) %p f_compilation_compile() finish now.\n",
	       (long) th_self(), c->target));
    /* finish now */
    run_pass2(c);
    debug_malloc_touch(c);
    run_cleanup(c,0);
    
    ret = debug_malloc_pass(c->p);

    debug_malloc_touch(c);

    if (!dependants_ok) {
      CDFPRINTF((stderr, "th(%ld) %p f_compilation_compile() reporting failure "
		 "since a dependant failed.\n",
		 (long) th_self(), c->target));
      throw_error_object(fast_clone_object(compilation_error_program), 0, 0, 0,
			 "Compilation failed.\n");
    }
    if(!ret) {
      CDFPRINTF((stderr, "th(%ld) %p f_compilation_compile() failed.\n",
		 (long) th_self(), c->target));
      throw_error_object(fast_clone_object(compilation_error_program), 0, 0, 0,
			 "Compilation failed.\n");
    }
    debug_malloc_touch(ret);
#ifdef PIKE_DEBUG
    if (a_flag > 2) {
      dump_program_tables(ret, 0);
    }
#endif /* PIKE_DEBUG */
    verify_supporters();
  }
  pop_n_elems(args);
  if (ret)
    ref_push_program(ret);
  else
    push_int(0);
}

/*! @decl mixed resolv(string identifier, string filename, @
 *!                    object handler)
 *!
 *!   Resolve the symbol @[identifier].
 *!
 *!   The default implementation calls the corresponding function
 *!   in any active handler, and otherwise falls back to
 *!   @[CompilerEnvironment()->resolv()] in the parent object.
 */
static void f_compilation_resolv(INT32 args)
{
  struct compilation *c = THIS_COMPILATION;
  struct object *handler;
  int fun = -1;

  if (((handler = c->handler) && handler->prog &&
       ((fun = find_identifier("resolv", handler->prog)) != -1)) ||
      ((handler = c->compat_handler) && handler->prog &&
       ((fun = find_identifier("resolv", handler->prog)) != -1))) {
    apply_low(handler, fun, args);
  } else {
    apply_external(1, CE_RESOLV_FUN_NUM, args);
  }
}

/*! @decl object get_compilation_handler(int major, int minor)
 *!
 *!   Get compatibility handler for Pike @[major].@[minor].
 *!
 *! @note
 *!   This function is called by @[change_compiler_compatibility()].
 */
static void f_compilation_get_compilation_handler(INT32 args)
{
  struct compilation *c = THIS_COMPILATION;
  struct object *handler;
  int fun = -1;

  if (((handler = c->handler) && handler->prog &&
       ((fun = find_identifier("get_compilation_handler", handler->prog)) != -1)) ||
      ((handler = c->compat_handler) && handler->prog &&
       ((fun = find_identifier("get_compilation_handler", handler->prog)) != -1))) {
    apply_low(handler, fun, args);
  } else {
    apply_external(1, CE_GET_COMPILATION_HANDLER_FUN_NUM, args);
  }
}

/*! @decl mapping(string:mixed)|object get_default_module()
 *!
 *!   Get the default module for the current compatibility level
 *!   (ie typically the value returned by @[predef::all_constants()]).
 *!
 *!   The default implementation calls the corresponding function
 *!   in the current handler, the current compatibility handler
 *!   or in the parent @[CompilerEnvironment] in that order.
 *!
 *! @returns
 *!   @mixed
 *!     @type mapping(string:mixed)|object
 *!       Constant table to use.
 *!
 *!     @type int(0..0)
 *!       Use the builtin constant table.
 *!   @endmixed
 *!
 *! @note
 *!   This function is called by @[change_compiler_compatibility()].
 */
static void f_compilation_get_default_module(INT32 args)
{
  struct compilation *c = THIS_COMPILATION;
  struct object *handler;
  int fun = -1;

  if (((handler = c->handler) && handler->prog &&
       ((fun = find_identifier("get_default_module", handler->prog)) != -1)) ||
      ((handler = c->compat_handler) && handler->prog &&
       ((fun = find_identifier("get_default_module", handler->prog)) != -1))) {
    apply_low(handler, fun, args);
  } else {
    apply_external(1, CE_GET_DEFAULT_MODULE_FUN_NUM, args);
  }
}

/*! @decl void change_compiler_compatibility(int major, int minor)
 *!
 *!   Change compiler to attempt to be compatible with Pike @[major].@[minor].
 */
static void f_compilation_change_compiler_compatibility(INT32 args)
{
  struct compilation *c = THIS_COMPILATION;
  int major = -1;
  int minor = -1;

  STACK_LEVEL_START(args);

  get_all_args("change_compiler_compatibility", args, "%d%d",
	       &major, &minor);

  if ((major == -1) && (minor == -1)) {
    major = PIKE_MAJOR_VERSION;
    minor = PIKE_MINOR_VERSION;
  }

  if ((major == Pike_compiler->compat_major) &&
      (minor == Pike_compiler->compat_minor)) {
    /* Optimization: Already at this compat level. */
    pop_n_elems(args);
    push_int(0);
    return;
  }

  Pike_compiler->compat_major=major;
  Pike_compiler->compat_minor=minor;

  /* Optimization: The up to date compiler shouldn't need a compat handler. */
  if((major != PIKE_MAJOR_VERSION) || (minor != PIKE_MINOR_VERSION))
  {
    apply_current(PC_GET_COMPILATION_HANDLER_FUN_NUM, args);

    if((Pike_sp[-1].type == T_OBJECT) && (Pike_sp[-1].u.object->prog))
    {
      if (Pike_sp[-1].subtype) {
	/* FIXME: */
	Pike_error("Subtyped compat handlers are not supported yet.\n");
      }
      if (c->compat_handler == Pike_sp[-1].u.object) {
	/* Still at the same compat level. */
	pop_stack();
	push_int(0);
	return;
      } else {
	if(c->compat_handler) free_object(c->compat_handler);
	c->compat_handler = Pike_sp[-1].u.object;
	dmalloc_touch_svalue(Pike_sp-1);
	Pike_sp--;
      }
    } else {
      pop_stack();
      if(c->compat_handler) {
	free_object(c->compat_handler);
	c->compat_handler = NULL;
      } else {
	/* No change in compat handler. */
	push_int(0);
	return;
      }
    }
  } else {
    pop_n_elems(args);
    if (c->compat_handler) {
      free_object(c->compat_handler);
      c->compat_handler = NULL;    
    } else {
      /* No change in compat handler. */
      push_int(0);
      return;
    }
  }

  STACK_LEVEL_CHECK(0);

  Pike_fp->args = 0;	/* Clean up the stack frame. */

  apply_current(PC_GET_DEFAULT_MODULE_FUN_NUM, 0);

  if(Pike_sp[-1].type == T_INT)
  {
    pop_stack();
    ref_push_mapping(get_builtin_constants());
  }

  STACK_LEVEL_CHECK(1);

  assign_svalue(&c->default_module, Pike_sp-1);

  /* Replace the implicit import of all_constants() with
   * the new value.
   */
  if(c->num_used_modules)
  {
    free_svalue( (struct svalue *)c->used_modules.s.str );
    ((struct svalue *)c->used_modules.s.str)[0]=sp[-1];
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

  STACK_LEVEL_DONE(0);
  push_int(0);
}

/*! @decl program handle_inherit(string inh)
 *!
 *!   Look up an inherit @[inh] in the current program.
 */
static void f_compilation_handle_inherit(INT32 args)
{
  struct compilation *c = THIS_COMPILATION;
  struct object *handler;
  int fun = -1;

  if (args > 1) pop_n_elems(args-1);

  ref_push_string(c->lex.current_file);
  if (c->handler && c->handler->prog) {
    ref_push_object(c->handler);
    args = 3;
  }
  else args = 2;

  if (((handler = c->handler) && handler->prog &&
       ((fun = find_identifier("handle_inherit", handler->prog)) != -1)) ||
      ((handler = c->compat_handler) && handler->prog &&
       ((fun = find_identifier("handle_inherit", handler->prog)) != -1))) {
    apply_low(handler, fun, args);
  } else {
    apply_external(1, CE_HANDLE_INHERIT_FUN_NUM, args);
  }
}

/*! @decl int(0..1) pop_type_attribute(string attribute, type a, type b)
 *!
 *!   Type attribute handler.
 *!
 *!   Called during type checking when @expr{a <= b@} and
 *!   @[a] had the type attribute @[attribute] before the
 *!   comparison.
 *!
 *!   The default implementation implements the "deprecated"
 *!   attribute.
 *!
 *! @returns
 *!   Returns @expr{1@} if the type check should be allowed
 *!   (ie @expr{__attribute__(attribute, a) <= b@}), and
 *!   @expr{0@} (zero) otherwise.
 *!
 *! @seealso
 *!   @[push_type_attribute()]
 */
static void f_compilation_pop_type_attribute(INT32 args)
{
  struct pike_string *attr;
  struct svalue *a, *b;
  struct compilation *c = THIS_COMPILATION;
  struct pike_string *deprecated_string;

  get_all_args("pop_type_attribute", args, "%W%*%*", &attr, &a, &b);

  if (Pike_compiler->compiler_pass == 2) {
    MAKE_CONST_STRING(deprecated_string, "deprecated");
    if ((attr == deprecated_string) &&
	!(c->lex.pragmas & ID_NO_DEPRECATION_WARNINGS)) {
      push_svalue(a);
      yytype_report(REPORT_WARNING, NULL, 0, NULL,
		    NULL, 0, NULL,
		    1, "Using deprecated %O value.");
    }
  }
  pop_n_elems(args);
  push_int(1);
}

/*! @decl int(0..1) push_type_attribute(string attribute, type a, type b)
 *!
 *!   Type attribute handler.
 *!
 *!   Called during type checking when @expr{a <= b@} and
 *!   @[b] had the type attribute @[attribute] before the
 *!   comparison.
 *!
 *!   The default implementation implements the "deprecated"
 *!   attribute.
 *!
 *! @returns
 *!   Returns @expr{1@} if the type check should be allowed
 *!   (ie @expr{a <= __attribute__(attribute, b)@}), and
 *!   @expr{0@} (zero) otherwise.
 *!
 *! @seealso
 *!   @[pop_type_attribute()]
 */
static void f_compilation_push_type_attribute(INT32 args)
{
  struct pike_string *attr;
  struct svalue *a, *b;
  struct compilation *c = THIS_COMPILATION;
  struct pike_string *deprecated_string;

  get_all_args("push_type_attribute", args, "%W%*%*", &attr, &a, &b);

  if (Pike_compiler->compiler_pass == 2) {
    MAKE_CONST_STRING(deprecated_string, "deprecated");
    if ((attr == deprecated_string) &&
	!(c->lex.pragmas & ID_NO_DEPRECATION_WARNINGS) &&
	!((a->type == PIKE_T_TYPE) && (a->u.type == zero_type_string))) {
      /* Don't warn about setting deprecated values to zero. */
      push_svalue(b);
      yytype_report(REPORT_WARNING, NULL, 0, NULL,
		    NULL, 0, NULL,
		    1, "Using deprecated %O value.");
    }
  }
  pop_n_elems(args);
  push_int(1);
}

/*! @decl int(0..1) apply_type_attribute(string attribute, @
 *!                                      type a, type|void b)
 *!
 *!   Type attribute handler.
 *!
 *! @param attribute
 *!   Attribute that @[a] had.
 *!
 *! @param a
 *!   Type of the value being called.
 *!
 *! @param b
 *!   Type of the first argument in the call, or
 *!   @[UNDEFINED] if no more arguments.
 *!
 *!   Called during type checking when @[a] has been successfully
 *!   had a partial evaluation with the argument @[b] and
 *!   @[a] had the type attribute @[attribute] before the
 *!   evaluation.
 *!
 *!   The default implementation implements the "deprecated"
 *!   attribute.
 *!
 *! @returns
 *!   Returns @expr{1@} if the type check should be allowed
 *!   (ie @expr{__attribute__(attribute, a)(b)@}) is valid,
 *!   and @expr{0@} (zero) otherwise.
 *!
 *! @seealso
 *!   @[pop_type_attribute()], @[push_type_attribute()]
 */
static void f_compilation_apply_type_attribute(INT32 args)
{
  struct pike_string *attr;
  struct svalue *a, *b = NULL;
  struct compilation *c = THIS_COMPILATION;
  struct pike_string *deprecated_string;

  get_all_args("apply_type_attribute", args, "%W%*.%*", &attr, &a, &b);

  if (Pike_compiler->compiler_pass == 2) {
    MAKE_CONST_STRING(deprecated_string, "deprecated");
    if ((attr == deprecated_string) &&
	!(c->lex.pragmas & ID_NO_DEPRECATION_WARNINGS) &&
	(!b ||
	 ((b->type == T_INT) && (b->subtype == NUMBER_UNDEFINED) &&
	  (!b->u.integer)))) {
      /* push_svalue(a); */
      yytype_report(REPORT_WARNING, NULL, 0, NULL,
		    NULL, 0, NULL,
		    0, "Calling a deprecated value.");
    }
  }
  pop_n_elems(args);
  push_int(1);
}

/*! @decl type(mixed) apply_attribute_constant(string attr, @
 *!                                            mixed value, @
 *!                                            type arg_type, @
 *!                                            void cont_type)
 *!
 *!   Handle constant arguments to attributed function argument types.
 *!
 *! @param attr
 *!   Attribute that @[arg_type] had.
 *!
 *! @param value
 *!   Constant value sent as parameter.
 *!
 *! @param arg_type
 *!   Declared type of the function argument.
 *!
 *! @param cont_type
 *!   Continuation function type after the current argument.
 *!
 *!   This function is called when a function is called
 *!   with the constant value @[value] and it has been
 *!   successfully matched against @[arg_type],
 *!   and @[arg_type] had the type attribute @[attr].
 *!
 *!   This function is typically used to perform specialized
 *!   argument checking and to allow for a strengthening
 *!   of the function type based on @[value].
 *!
 *!   The default implementation implements the @expr{"sprintf_format"@},
 *!   @expr{"sscanf_format"@} and @expr{"sscanf_76_format"@} attributes.
 *!
 *! @returns
 *!   Returns a continuation type if it succeeded in strengthening the type.
 *!
 *!   Returns @tt{UNDEFINED@} otherwise (this is not an error indication).
 *!
 *! @seealso
 *!   @[pop_type_attribute()], @[push_type_attribute()]
 */
static void f_compilation_apply_attribute_constant(INT32 args)
{
  struct compilation *c = THIS_COMPILATION;
  struct pike_string *attribute;
  struct pike_string *test;
  struct svalue *sval;
  get_all_args("apply_attribute_constant", args, "%S%*", &attribute, &sval);

  if ((sval->type == T_INT) && !sval->u.integer) {
    pop_n_elems(args);
    push_undefined();
    return;
  }

  MAKE_CONST_STRING(test, "sprintf_format");
  if (attribute == test) {
    f___handle_sprintf_format(args);
    return;
  }
  MAKE_CONST_STRING(test, "strict_sprintf_format");
  if (attribute == test) {
    f___handle_sprintf_format(args);
    return;
  }
  MAKE_CONST_STRING(test, "sscanf_format");
  if (attribute == test) {
    f___handle_sscanf_format(args);
    return;
  }
  MAKE_CONST_STRING(test, "sscanf_76_format");
  if (attribute == test) {
    f___handle_sscanf_format(args);
    return;
  }
  pop_n_elems(args);
  push_undefined();
}

static void f_compilation__sprintf(INT32 args)
{
  struct compilation *c = THIS_COMPILATION;
  struct string_builder buf;
  init_string_builder_alloc(&buf, 50, 0);
  string_builder_strcat(&buf, "PikeCompiler(");
  if (c->prog) {
    string_builder_strcat(&buf, "\"\", ");
  } else {
    string_builder_strcat(&buf, "UNDEFINED, ");
  }
  if (c->handler) {
    ref_push_object(c->handler);
    string_builder_sprintf(&buf, "%O, ", Pike_sp-1);
    pop_stack();
  } else {
    string_builder_strcat(&buf, "UNDEFINED, ");
  }
  string_builder_sprintf(&buf, "%d, %d, %s, %s)",
			 c->major, c->minor,
			 c->target?"target":"UNDEFINED",
			 c->placeholder?"placeholder":"UNDEFINED");
  pop_n_elems(args);
  push_string(finish_string_builder(&buf));
}

/* Fake being called via PikeCompiler()->compile()
 *
 * This function is used to set up the environment for
 * compiling C efuns and modules.
 *
 * Note: Since this is a stack frame, it will be cleaned up
 *       automatically on error, so no need to use ONERROR().
 *
 * Note: Steals a reference from ce.
 */
static void low_enter_compiler(struct object *ce, int inherit)
{
  struct pike_frame *new_frame = alloc_pike_frame();
#ifdef PROFILING
  new_frame->children_base = Pike_interpreter.accounted_time;
  new_frame->start_time = get_cpu_time() - Pike_interpreter.unlocked_time;
  new_frame->ident = PC_COMPILE_FUN_NUM;	/* Fake call of compile(). */
#endif /* PROFILING */
  new_frame->next = Pike_fp;
  new_frame->current_object = ce;
  /* Note: The compilation environment object hangs on this frame,
   *       so that it will be freed when the frame dies.
   */
  new_frame->current_program = ce->prog;
  add_ref(new_frame->current_program);
  new_frame->context = compilation_program->inherits + inherit;
  new_frame->current_storage = ce->storage + new_frame->context->storage_offset;
#ifdef PIKE_DEBUG
  if (new_frame->context->prog != compilation_program) {
    Pike_fatal("Invalid inherit for compilation context (%p != %p).\n",
	       new_frame->context->prog, compilation_program);
  }
#endif /* PIKE_DEBUG */
  new_frame->fun = new_frame->context->identifier_level + PC_COMPILE_FUN_NUM;
  new_frame->expendible = Pike_sp;
  new_frame->locals = Pike_sp;
  new_frame->save_sp = Pike_sp;
  new_frame->save_mark_sp = Pike_mark_sp;
  new_frame->mark_sp_base = Pike_mark_sp;
  new_frame->args = 0;
  new_frame->num_locals = 0;
  new_frame->pc = 0;
  new_frame->return_addr = 0;
  new_frame->scope = 0;
  new_frame->save_sp = Pike_sp;
  Pike_fp = new_frame;
}

PMOD_EXPORT void enter_compiler(struct pike_string *filename, int linenumber)
{
  struct object *ce = parent_clone_object(compilation_program,
					  compilation_environment,
					  CE_PIKE_COMPILER_FUN_NUM, 0);
  struct compilation *c;

  low_enter_compiler(ce, 0);

  c = THIS_COMPILATION;
  if (filename) {
    free_string(c->lex.current_file);
    copy_shared_string(c->lex.current_file, filename);
  }
  if (linenumber) {
    c->lex.current_line = linenumber;
  }
}

/* Reverse the effect of enter_compiler().
 */
PMOD_EXPORT void exit_compiler(void)
{
#ifdef PIKE_DEBUG
  if ((Pike_fp->current_program != compilation_program) ||
      (Pike_fp->fun != PC_COMPILE_FUN_NUM)) {
    Pike_fatal("exit_compiler(): Frame stack out of whack!\n");
  }
#endif /* PIKE_DEBUG */
  POP_PIKE_FRAME();
}

/*! @class CompilerState
 *!
 *!   Keeps the state of a single program/class during compilation.
 *!
 *! @note
 *!   Not in use yet!
 */

#define THIS_PROGRAM_STATE  ((struct program_state *)(Pike_fp->current_storage))

static void program_state_event_handler(int event)
{
#if 0
  struct program_state *c = THIS_PROGRAM_STATE;
  switch (event) {
  case PROG_EVENT_INIT:
#define INIT
#include "compilation.h"
    break;
  case PROG_EVENT_EXIT:
#define EXIT
#include "compilation.h"
    break;
  }
#endif /* 0 */
}

/*! @endclass
 */

/*! @endclass
 */

/*! @endclass
 */

/* Strap the compiler by creating the compilation program by hand. */
static void compile_compiler(void)
{
  struct program *p = low_allocate_program();
  struct program *p2 = compilation_program = low_allocate_program();
  struct object *co;
  struct inherit *inh;

  p->parent_info_storage = -1;
  /* p->event_handler = compilation_env_event_handler; */
  p->flags |= PROGRAM_HAS_C_METHODS;

#if 0
  /* ADD_STORAGE(struct compilation_env); */
  p->alignment_needed = ALIGNOF(struct compilation_env);
  p->storage_needed = p->xstorage + sizeof(struct compilation_env);
#endif /* 0 */

  /* Add the initial inherit, this is needed for clone_object()
   * to actually call the event handler, and for low_enter_compiler()
   * to find the storage and context. */
  p->inherits = inh = xalloc(sizeof(struct inherit));
  inh->prog = p;
  inh->inherit_level = 0;
  inh->identifier_level = 0;
  inh->parent_identifier = -1;
  inh->parent_offset = OBJECT_PARENT;
  inh->identifier_ref_offset = 0;
  inh->storage_offset = p->xstorage;
  inh->parent = NULL;
  inh->name = NULL;
  p->num_inherits = 1;

  /* Force clone_object() to accept the program...
   */
  p->flags |= PROGRAM_PASS_1_DONE;
  compilation_environment = clone_object(p, 0);
  p->flags &= ~PROGRAM_PASS_1_DONE;

  /* Once more, this time for p2...
   */

  p2->parent_info_storage = 0;
  p2->xstorage = sizeof(struct parent_info);
  p2->event_handler = compilation_event_handler;
  p2->flags |= PROGRAM_NEEDS_PARENT|PROGRAM_USES_PARENT|PROGRAM_HAS_C_METHODS;

  /* ADD_STORAGE(struct compilation); */
  p2->alignment_needed = ALIGNOF(struct compilation);
  p2->storage_needed = p2->xstorage + sizeof(struct compilation);

  p2->inherits = inh = xalloc(sizeof(struct inherit));
  inh->prog = p2;
  inh->inherit_level = 0;
  inh->identifier_level = 0;
  inh->parent_identifier = CE_PIKE_COMPILER_FUN_NUM;
  inh->parent_offset = OBJECT_PARENT;
  inh->identifier_ref_offset = 0;
  inh->storage_offset = p2->xstorage;
  inh->parent = NULL;
  inh->name = NULL;
  p2->num_inherits = 1;

  p2->flags |= PROGRAM_PASS_1_DONE;
  co = parent_clone_object(p2, compilation_environment,
			   CE_PIKE_COMPILER_FUN_NUM, 0);
  p2->flags &= ~PROGRAM_PASS_1_DONE;

  low_enter_compiler(co, 0);

  low_start_new_program(p, 1, NULL, 0, NULL);
  free_program(p);	/* Remove the extra ref we just got... */

  ADD_FUNCTION("report", f_compilation_env_report, 
	       tFuncV(tName("SeverityLevel", tInt03) tStr tIntPos
		      tStr tStr, tMix, tVoid),0);

  ADD_FUNCTION("compile", f_compilation_env_compile,
	       tFunc(tOr(tStr, tVoid) tOr(tObj, tVoid)
		     tOr(tInt, tVoid) tOr(tInt, tVoid)
		     tOr(tPrg(tObj), tVoid) tOr(tObj, tVoid),
		     tPrg(tObj)), 0);

  ADD_FUNCTION("resolv", f_compilation_env_resolv,
	       tFunc(tStr tStr tObj, tMix), 0);

  low_start_new_program(p2, 1, NULL, 0, NULL);

  /* low_start_new_program() has zapped the inherit we created
   * for p2 above, so we need to repair the frame pointer.
   */
  Pike_fp->context = p2->inherits;

  /* MAGIC! We're now executing inside the object being compiled,
   * and have done sufficient stuff to be able to call and use
   * the normal program building functions.
   */

  /* NOTE: The order of these identifiers is hard-coded in
   *       the PC_*_FUN_NUM definitions in "pike_compiler.h".
   */

  ADD_FUNCTION("report", f_compilation_report, 
	       tFuncV(tName("SeverityLevel", tInt03) tStr tIntPos
		      tStr tStr, tMix, tVoid),0);

  ADD_FUNCTION("compile", f_compilation_compile,
	       tFunc(tNone, tPrg(tObj)), 0);

  ADD_FUNCTION("resolv", f_compilation_resolv,
	       tFunc(tStr tStr tObj, tMix), 0);

  ADD_FUNCTION("create", f_compilation_create,
	       tFunc(tOr(tStr, tVoid) tOr(tObj, tVoid)
		     tOr(tInt, tVoid) tOr(tInt, tVoid)
		     tOr(tPrg(tObj), tVoid) tOr(tObj, tVoid), tVoid),
	       ID_PROTECTED);

  ADD_FUNCTION("get_compilation_handler",
	       f_compilation_get_compilation_handler,
	       tFunc(tInt tInt, tObj), 0);
  
  ADD_FUNCTION("get_default_module", f_compilation_get_default_module,
	       tFunc(tNone, tOr(tMap(tStr, tMix), tObj)), 0);

  ADD_FUNCTION("change_compiler_compatibility",
	       f_compilation_change_compiler_compatibility,
	       tFunc(tInt tInt, tVoid), 0);

  ADD_FUNCTION("handle_inherit", f_compilation_handle_inherit,
	       tFunc(tStr, tPrg(tObj)), 0);

  ADD_FUNCTION("pop_type_attribute", f_compilation_pop_type_attribute,
	       tFunc(tStr tType(tMix) tType(tMix), tInt01), 0);

  ADD_FUNCTION("push_type_attribute", f_compilation_push_type_attribute,
	       tFunc(tStr tType(tMix) tType(tMix), tInt01), 0);

  ADD_FUNCTION("apply_type_attribute", f_compilation_apply_type_attribute,
	       tFunc(tStr tType(tMix) tOr(tType(tMix), tVoid), tInt01), 0);

  ADD_FUNCTION("apply_attribute_constant",
	       f_compilation_apply_attribute_constant,
	       tFunc(tStr tMix tType(tMix) tType(tFunction),
		     tType(tFunction)), 0);

  ADD_FUNCTION("_sprintf", f_compilation__sprintf,
	       tFunc(tInt tOr(tMap(tStr, tMix), tVoid), tStr), ID_PROTECTED);

  start_new_program();

  ADD_STORAGE(struct program_state);
  Pike_compiler->new_program->event_handler = program_state_event_handler;
  Pike_compiler->new_program->flags |=
    PROGRAM_NEEDS_PARENT|PROGRAM_USES_PARENT|PROGRAM_HAS_C_METHODS;

  /* Alias for report above. */
  low_define_alias(NULL, NULL, 0, 1, PC_REPORT_FUN_NUM);

  end_class("CompilerState", 0);

  /* Map some of our variables so that the gc can find them. */
  PIKE_MAP_VARIABLE("prog", OFFSETOF(compilation, prog),
		    tStr, PIKE_T_STRING, ID_HIDDEN);
  PIKE_MAP_VARIABLE("handler", OFFSETOF(compilation, handler),
		    tObj, PIKE_T_OBJECT, 0);
  PIKE_MAP_VARIABLE("compat_handler", OFFSETOF(compilation, compat_handler),
		    tObj, PIKE_T_OBJECT, 0);
  PIKE_MAP_VARIABLE("target", OFFSETOF(compilation, target),
		    tPrg(tObj), PIKE_T_PROGRAM, ID_HIDDEN);
  PIKE_MAP_VARIABLE("placeholder", OFFSETOF(compilation, placeholder),
		    tObj, PIKE_T_OBJECT, ID_HIDDEN);
  PIKE_MAP_VARIABLE("p", OFFSETOF(compilation, p),
		    tPrg(tObj), PIKE_T_PROGRAM, ID_HIDDEN);
  PIKE_MAP_VARIABLE("current_file", OFFSETOF(compilation, lex.current_file),
		    tStr, PIKE_T_STRING, ID_HIDDEN);
  PIKE_MAP_VARIABLE("default_module", OFFSETOF(compilation, default_module),
		    tOr(tMap(tStr,tMix),tObj), PIKE_T_MIXED, 0);

  /* end_class()/end_program() adds the parent_info storage once more.
   * Remove the one we added above, so that we don't get it double.
   */
  p2->xstorage = 0;
  
  end_class("PikeCompiler", 0);
  /* end_class()/end_program() has zapped the inherit once again,
   * so we need to repair the frame pointer.
   */
  Pike_fp->context = compilation_program->inherits;

  ADD_FUNCTION("get_compilation_handler",
	       f_compilation_env_get_compilation_handler,
	       tFunc(tInt tInt, tObj), 0);
  
  ADD_FUNCTION("get_default_module",
	       f_compilation_env_get_default_module,
	       tFunc(tNone, tOr(tMap(tStr, tMix), tObj)), 0);

  ADD_FUNCTION("handle_inherit", f_compilation_env_handle_inherit,
	       tFunc(tStr tStr tOr(tObj, tVoid), tPrg(tObj)), 0);

  {
    struct svalue type_value;

    /* enum SeverityLevel { NOTICE, WARNING, ERROR, FATAL } */
    type_value.type = PIKE_T_TYPE;
    type_value.subtype = 0;
    type_value.u.type = CONSTTYPE(tName("SeverityLevel", tInt03));
    simple_add_constant("SeverityLevel", &type_value, 0);
    free_svalue(&type_value);

    add_integer_constant("NOTICE",  REPORT_NOTICE, 0);
    add_integer_constant("WARNING", REPORT_WARNING, 0);
    add_integer_constant("ERROR",   REPORT_ERROR, 0);
    add_integer_constant("FATAL",   REPORT_FATAL, 0);
  }

  compilation_env_program = end_program();

  add_global_program("CompilerEnvironment", compilation_env_program);

  exit_compiler();

  ref_push_object(compilation_environment);
  low_add_constant("DefaultCompilerEnvironment", Pike_sp-1);
  pop_stack();
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
  struct object *ce;
  struct compilation *c;

  /* FIXME! */

  Pike_fatal("Old C-level compile() function called!\n");

  CDFPRINTF((stderr, "th(%ld) %p compile() enter, placeholder=%p\n",
	     (long) th_self(), atarget, aplaceholder));

  ce = clone_object(compilation_program, 0);
  c = (struct compilation *)ce->storage;

  debug_malloc_touch(c);

  verify_supporters();

  c->p = NULL;
  add_ref(c->prog=aprog);
  if((c->handler=ahandler)) add_ref(ahandler);
  c->major=amajor;
  c->minor=aminor;
  if((c->target=atarget)) add_ref(atarget);
  if((c->placeholder=aplaceholder)) add_ref(aplaceholder);
  c->default_module.type=T_INT;
  c->default_module.subtype = NUMBER_NUMBER;

  if (c->handler)
  {
    if (safe_apply_handler ("get_default_module", c->handler, NULL,
			    0, BIT_MAPPING|BIT_OBJECT|BIT_ZERO)) {
      if(SAFE_IS_ZERO(Pike_sp-1))
      {
	pop_stack();
	ref_push_mapping(get_builtin_constants());
      }
    } else {
      ref_push_mapping(get_builtin_constants());
    }
  }else{
    ref_push_mapping(get_builtin_constants());
  }
  free_svalue(& c->default_module);
  move_svalue (&c->default_module, --Pike_sp);

#ifdef PIKE_DEBUG
  SET_ONERROR(tmp, fatal_on_error,"Compiler exited with longjump!\n");
#endif

  low_init_threads_disable();
  c->saved_threads_disabled = threads_disabled;

  init_supporter(& c->supporter,
		 (supporter_callback *) call_delayed_pass2,
		 (void *)c);

  delay=run_pass1(c);
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
    CDFPRINTF((stderr, "th(%ld) %p compile() finish now\n",
	       (long) th_self(), c->target));
    /* finish now */
    if(c->p) run_pass2(c);
    debug_malloc_touch(c);
    run_cleanup(c,0);
    
    ret=c->p;
    /* FIXME: Looks like ret should get an extra ref here, but I'm not
     * sure. Besides, this function isn't used anymore. /mast */

    debug_malloc_touch(c);
    free_object(ce);

    if (!dependants_ok) {
      CDFPRINTF((stderr, "th(%ld) %p compile() reporting failure "
		 "since a dependant failed.\n",
		 (long) th_self(), c->target));
      if (ret) free_program(ret);
      throw_error_object(fast_clone_object(compilation_error_program), 0, 0, 0,
			 "Compilation failed.\n");
    }
    if(!ret) {
      CDFPRINTF((stderr, "th(%ld) %p compile() failed.\n",
		 (long) th_self(), c->target));
      throw_error_object(fast_clone_object(compilation_error_program), 0, 0, 0,
			 "Compilation failed.\n");
    }
    debug_malloc_touch(ret);
#ifdef PIKE_DEBUG
    if (a_flag > 2) {
      dump_program_tables(ret, 0);
    }
#endif /* PIKE_DEBUG */
    verify_supporters();
    return ret;
  }
}

PMOD_EXPORT int pike_add_function2(const char *name, void (*cfun)(INT32),
				   const char *type, unsigned flags,
				   unsigned opt_flags)
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

PMOD_EXPORT int quick_add_function(const char *name,
				   int name_length,
				   void (*cfun)(INT32),
				   const char *type,
				   int type_length,
				   unsigned flags,
				   unsigned opt_flags)
{
  int ret;
  struct pike_string *name_tmp;
  struct pike_type *type_tmp;
  union idptr tmp;
/*  fprintf(stderr,"ADD_FUNC: %s\n",name); */
  name_tmp = make_shared_binary_string(name, name_length);
  type_tmp = make_pike_type(type);

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
	  Pike_fatal("Error in find_function_cache[%ld].id\n",(long)e);

	if(cache[e].fun < -1 || cache[e].fun > 65536)
	  Pike_fatal("Error in find_function_cache[%ld].fun\n",(long)e);
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

static void sprintf_trampoline (INT32 args)
{
  dynamic_buffer save_buf;
  dynbuf_string str;

  if (!args || sp[-args].type != T_INT || sp[-args].u.integer != 'O' ||
      !THIS->frame || !THIS->frame->current_object) {
    pop_n_elems (args);
    push_int (0);
    return;
  }
  pop_n_elems (args);

  ref_push_function (THIS->frame->current_object, THIS->func);
  init_buf(&save_buf);
  describe_svalue (sp - 1, 0, 0);
  str = complex_free_buf(&save_buf);
  pop_stack();
  push_string (make_shared_binary_string (str.str, str.len));
  free (str.str);
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
      debug_gc_check (f->current_object, " as current_object in trampoline frame");
    if(f->current_program)
      debug_gc_check (f->current_program, " as current_program in trampoline frame");
    debug_gc_check_svalues (f->locals, f->num_locals, " in locals of trampoline frame");
    if(f->scope && !debug_gc_check (f->scope, " as scope frame of trampoline frame"))
      gc_check_frame(f->scope);
  }
}

static void gc_check_trampoline(struct object *o)
{
  if (THIS->frame &&
      !debug_gc_check (THIS->frame, " as trampoline frame"))
    gc_check_frame(THIS->frame);
}

static void gc_recurse_frame(struct pike_frame *f)
{
  if(f->current_object) gc_recurse_object(f->current_object);
  if(f->current_program) gc_recurse_program(f->current_program);
  if(f->flags & PIKE_FRAME_MALLOCED_LOCALS)
    gc_recurse_svalues(f->locals,f->num_locals);
  if(f->scope)          gc_recurse_frame(f->scope);
}

static void gc_recurse_trampoline(struct object *o)
{
  if (THIS->frame) gc_recurse_frame(THIS->frame);
}


/* This placeholder should be used
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

static void placeholder_sprintf (INT32 args)
{
  struct pike_string *s;

  if (!args || sp[-args].type != T_INT || sp[-args].u.integer != 'O') {
    pop_n_elems (args);
    push_int (0);
    return;
  }

  pop_n_elems (args);
  MAKE_CONST_STRING (s, "__placeholder_object");
  ref_push_string (s);
}

void init_program(void)
{
  size_t i;
  struct svalue key;
  struct svalue val;
  struct svalue id;
  init_program_blocks();

  MAKE_CONST_STRING(this_function_string,"this_function");
  MAKE_CONST_STRING(this_program_string,"this_program");
  MAKE_CONST_STRING(this_string,"this");
  MAKE_CONST_STRING(UNDEFINED_string,"UNDEFINED");

  MAKE_CONST_STRING(parser_system_string, "parser");
  MAKE_CONST_STRING(type_check_system_string, "type_check");

  lfun_ids = allocate_mapping(NUM_LFUNS);
  lfun_types = allocate_mapping(NUM_LFUNS);
  for (i=0; i < NELEM(lfun_names); i++) {
    lfun_strings[i] = make_shared_string(lfun_names[i]);

    id.type = T_INT;
    id.subtype = NUMBER_NUMBER;
    id.u.integer = i;
    key.type = T_STRING;
    key.u.string = lfun_strings[i];
    mapping_insert(lfun_ids, &key, &id);

    val.type = T_TYPE;
    val.u.type = make_pike_type(raw_lfun_types[i]);
    mapping_insert(lfun_types, &key, &val);
    free_type(val.u.type);
  }

  lfun_getter_type_string = make_pike_type(tFuncV(tNone, tVoid, tMix));
  lfun_setter_type_string = make_pike_type(tFuncV(tZero, tVoid, tVoid));

  compile_compiler();

  enter_compiler(NULL, 0);

  start_new_program();
  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);
  ADD_STORAGE(struct pike_trampoline);
  ADD_FUNCTION("`()",apply_trampoline,tFunction,0);
  ADD_FUNCTION("`!",not_trampoline,tFunc(tVoid,tInt),0);
  ADD_FUNCTION("_sprintf", sprintf_trampoline,
	       tFunc(tInt tOr(tMapping,tVoid),tStr), 0);
  set_init_callback(init_trampoline);
  set_exit_callback(exit_trampoline);
  set_gc_check_callback(gc_check_trampoline);
  set_gc_recurse_callback(gc_recurse_trampoline);
  debug_malloc_touch(Pike_compiler->fake_object);
  debug_malloc_touch(Pike_compiler->fake_object->storage);
  pike_trampoline_program=end_program();

  /*! @decl constant __null_program
   *!
   *! Program used internally by the compiler to create objects
   *! that are later modified into instances of the compiled program
   *! by the compiler.
   *!
   *! @seealso
   *!   @[__placeholder_object]
   */
  {
    struct svalue s;
    debug_start_new_program(0, "__null_program");
    null_program=end_program();
    s.type=T_PROGRAM;
    s.u.program=null_program;
    low_add_constant("__null_program",&s);
    debug_malloc_touch(null_program);
  }

  /*! @decl constant __placeholder_object
   *!
   *! Object used internally by the compiler.
   *!
   *! @seealso
   *!   @[__null_program]
   */
  {
    struct svalue s;
    start_new_program();
    ADD_FUNCTION("`()", placeholder_index, tFuncV(tNone,tMix,tObj), 0);
    ADD_FUNCTION("`[]", placeholder_index, tFunc(tMix,tObj), 0);
    ADD_FUNCTION("_sprintf", placeholder_sprintf,
		 tFunc(tInt tOr(tMapping,tVoid),tStr), 0);
    placeholder_program=end_program();
    placeholder_object=fast_clone_object(placeholder_program);

    s.type=T_OBJECT;
    s.subtype = 0;
    s.u.object=placeholder_object;
    low_add_constant("__placeholder_object",&s);
    debug_malloc_touch(placeholder_object);
  }

  exit_compiler();
}

void cleanup_program(void)
{
  size_t e;

  free_type(lfun_setter_type_string);
  free_type(lfun_getter_type_string);
  free_mapping(lfun_types);
  free_mapping(lfun_ids);
  for (e=0; e < NELEM(lfun_names); e++) {
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

  if (compilation_program) {
    free_program(compilation_program);
    compilation_program = 0;
  }
  if (compilation_environment) {
    free_object(compilation_environment);
    compilation_environment = 0;
  }
  if (compilation_env_program) {
    free_program(compilation_env_program);
    compilation_env_program = 0;
  }
}


PMOD_EXPORT void visit_program (struct program *p, int action)
{
  switch (action) {
#ifdef PIKE_DEBUG
    default:
      Pike_fatal ("Unknown visit action %d.\n", action);
    case VISIT_NORMAL:
    case VISIT_COMPLEX_ONLY:
      break;
#endif
    case VISIT_COUNT_BYTES:
      mc_counted_bytes += p->total_size;
      break;
  }

  if (!(p->flags & PROGRAM_AVOID_CHECK)) {
    int e;
    struct program_constant *consts = p->constants;
    struct inherit *inh = p->inherits;

    for (e = p->num_constants - 1; e >= 0; e--)
      visit_svalue (&consts[e].sval, REF_TYPE_NORMAL);

    for (e = p->num_inherits - 1; e >= 0; e--) {
      if (inh[e].parent)
	visit_object_ref (inh[e].parent, REF_TYPE_NORMAL);

      if (e && inh[e].prog)
	visit_program_ref (inh[e].prog, REF_TYPE_NORMAL);
    }

    if (!(action & VISIT_COMPLEX_ONLY)) {
      struct identifier *ids = p->identifiers;
      struct pike_string **strs = p->strings;

      for (e = p->num_inherits - 1; e >= 0; e--) {
	if (inh[e].name)
	  visit_string_ref (inh[e].name, REF_TYPE_NORMAL);
      }

      for (e = p->num_identifiers - 1; e >= 0; e--) {
	struct identifier *id = ids + e;
	visit_string_ref (id->name, REF_TYPE_NORMAL);
	visit_type_ref (id->type, REF_TYPE_NORMAL);
      }

      for (e = p->num_strings - 1; e >= 0; e--)
	visit_string_ref (strs[e], REF_TYPE_NORMAL);
    }

    /* Strong ref follows. It must be last. */
    if (p->parent)
      visit_program_ref (p->parent, REF_TYPE_STRONG);
  }
}

static void gc_check_program(struct program *p);

void gc_mark_program_as_referenced(struct program *p)
{
  debug_malloc_touch(p);

  if (p->flags & PROGRAM_AVOID_CHECK) {
    /* Program is in an inconsistent state.
     * don't look closer at it.
     */
    debug_malloc_touch(p);

    if (gc_mark(p, T_PROGRAM)) {
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
  
  if(gc_mark(p, T_PROGRAM))
    GC_ENTER (p, T_PROGRAM) {
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

      for(e = p->num_constants - 1; e >= 0; e--)
	gc_mark_svalues(& p->constants[e].sval, 1);

      for(e = p->num_inherits - 1; e >= 0; e--)
      {
	if(p->inherits[e].parent)
	  gc_mark_object_as_referenced(p->inherits[e].parent);

	if(e && p->inherits[e].prog)
	  gc_mark_program_as_referenced(p->inherits[e].prog);
      }

#if defined (PIKE_DEBUG) || defined (DO_PIKE_CLEANUP)
      for (e = p->num_identifiers - 1; e >= 0; e--)
	gc_mark_type_as_referenced (p->identifiers[e].type);
#endif

    } GC_LEAVE;
}

void real_gc_cycle_check_program(struct program *p, int weak)
{
  GC_CYCLE_ENTER(p, T_PROGRAM, weak) {
    int e;

    if (!(p->flags & PROGRAM_AVOID_CHECK))
    {
      for(e = p->num_constants - 1; e >= 0; e--)
	gc_cycle_check_svalues(& p->constants[e].sval, 1);
      
      for(e = p->num_inherits - 1; e >= 0; e--)
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

  debug_malloc_touch(p);

  if (p->flags & PROGRAM_AVOID_CHECK) {
    /* Program is in an inconsistent state.
     * don't look closer at it.
     */
    debug_malloc_touch(p);
    return;
  }

  GC_ENTER (p, T_PROGRAM) {
    if(p->parent)
      debug_gc_check (p->parent, " as parent program of a program");
  
    for(e = p->num_constants - 1; e >= 0; e--) {
      debug_gc_check_svalues (&p->constants[e].sval, 1, " as program constant");
    }
  
    for(e = p->num_inherits - 1; e >= 0; e--)
    {
      if(p->inherits[e].parent)
      {
	debug_gc_check (p->inherits[e].parent,
			" as inherited parent object of a program");
      }

#ifdef PIKE_DEBUG
      if (Pike_in_gc == GC_PASS_LOCATE && p->inherits[e].name)
	debug_gc_check (p->inherits[e].name, " as inherit name");
#endif

      if(e && p->inherits[e].prog)
	debug_gc_check (p->inherits[e].prog, " as inherited program of a program");
    }

#if defined (PIKE_DEBUG) || defined (DO_PIKE_CLEANUP)
    if (gc_keep_markers || Pike_in_gc == GC_PASS_LOCATE)
    {
      for(e = p->num_strings - 1; e >= 0; e--)
	debug_gc_check (p->strings[e], " in the string storage of a program");
      for(e = p->num_identifiers - 1; e >= 0; e--)
	debug_gc_check (p->identifiers[e].name,
			" as identifier name in a program");
    }

    for(e = p->num_identifiers - 1; e >= 0; e--)
      debug_gc_check (p->identifiers[e].type,
		      " as identifier type in a program");
#endif

  } GC_LEAVE;
}

unsigned gc_touch_all_programs(void)
{
  unsigned n = 0;
  struct program *p;
  if (first_program && first_program->prev)
    Pike_fatal("Error in program link list.\n");
  for (p = first_program; p; p = p->next) {
    debug_gc_touch(p);
    n++;
    if (p->next && p->next->prev != p)
      Pike_fatal("Error in program link list.\n");
  }
  return n;
}

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
  gc_mark_discard_queue();
}

size_t gc_free_all_unreferenced_programs(void)
{
  struct program *p,*next;
  size_t unreferenced = 0;
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
	p->constants[e].sval.subtype = NUMBER_NUMBER;
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
    unreferenced++;
  }

#ifdef PIKE_DEBUG
  if (gc_debug)
    for (p = first_program; p != gc_internal_program; p = p->next) {
      int e,tmp=0;
      if (!p)
	Pike_fatal("gc_internal_program was bogus.\n");
      for(e=0;e<p->num_constants;e++)
      {
	if(p->constants[e].sval.type == T_PROGRAM && p->constants[e].sval.u.program == p)
	  tmp++;
      }
      if(tmp >= p->refs)
	gc_fatal(p, 1 ,"garbage collector failed to free program!!!\n");
    }
#endif

  return unreferenced;
}


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
  struct compilation *c = THIS_COMPILATION;
  while(Pike_compiler->compiler_frame->current_number_of_locals > level)
  {
    int e;
    e=--(Pike_compiler->compiler_frame->current_number_of_locals);
    if ((Pike_compiler->compiler_pass == 2) &&
	!(Pike_compiler->compiler_frame->variable[e].flags &
	  LOCAL_VAR_IS_USED)) {
      ref_push_string(Pike_compiler->compiler_frame->variable[e].name);
      low_yyreport(REPORT_WARNING,
		   Pike_compiler->compiler_frame->variable[e].file,
		   Pike_compiler->compiler_frame->variable[e].line,
		   parser_system_string,
		   1, "Unused local variable %s.");
    }
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
  struct compilation *c = THIS_COMPILATION;
  /* We need to save the variables Kuppo (but not their names) */
  if(level < Pike_compiler->compiler_frame->min_number_of_locals)
  {
    /* FIXME: Consider using flags to indicate whether a local variable
     *        actually is used from a nested scope. */
    for(;level<Pike_compiler->compiler_frame->min_number_of_locals;level++)
    {
      if ((Pike_compiler->compiler_pass == 2) &&
	  !(Pike_compiler->compiler_frame->variable[level].flags &
	    LOCAL_VAR_IS_USED)) {
	ref_push_string(Pike_compiler->compiler_frame->variable[level].name);
	low_yyreport(REPORT_WARNING,
		     Pike_compiler->compiler_frame->variable[level].file,
		     Pike_compiler->compiler_frame->variable[level].line,
		     parser_system_string,
		     1, "Unused local variable %s.");
	/* Make sure we only warn once... */
	Pike_compiler->compiler_frame->variable[level].flags |=
	  LOCAL_VAR_IS_USED;
      }
      free_string(Pike_compiler->compiler_frame->variable[level].name);
      copy_shared_string(Pike_compiler->compiler_frame->variable[level].name,
			 empty_pike_string);
      /* FIXME: Do we need to keep the filenames? */
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
    Pike_fatal("Popping out of compiler frames\n");
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

PMOD_EXPORT ptrdiff_t low_get_storage(struct program *o, struct program *p)
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
    Pike_fatal("hval>GET_STORAGE_CACHE_SIZE\n");
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

struct program *low_program_from_function(struct object *o, INT32 i)
{
  struct svalue *f;
  struct program *p;
  struct identifier *id;
  while(1) {
    struct external_variable_context loc;
    p = o->prog;

    if(!p) return 0;

    id = ID_FROM_INT(p, i);
    if(!IDENTIFIER_IS_ALIAS(id->identifier_flags)) break;

    loc.o = o;
    loc.inherit = INHERIT_FROM_INT(p, i);
    loc.parent_identifier = i;
    find_external_context(&loc, id->func.ext_ref.depth);
    i = id->func.ext_ref.id + loc.inherit->identifier_level;
    o = loc.o;
  }
  if(!IDENTIFIER_IS_CONSTANT(id->identifier_flags)) return 0;
  if(id->func.offset==-1) return 0;
  f = &PROG_FROM_INT(p,i)->constants[id->func.offset].sval;
  if(f->type!=T_PROGRAM) return 0;
  return f->u.program;
}

PMOD_EXPORT struct program *program_from_function(const struct svalue *f)
{
  if(f->type != T_FUNCTION) return 0;
  if(f->subtype == FUNCTION_BUILTIN) return 0;
  return low_program_from_function(f->u.object, f->subtype);
}

/* NOTE: Does not add references to the return value! */
PMOD_EXPORT struct program *program_from_svalue(const struct svalue *s)
{
  switch(s->type)
  {
    case T_OBJECT:
    {
      struct program *p = s->u.object->prog;
#if 0
      int call_fun;
#endif

      if (!p) return 0;

#if 0
      p = p->inherits[s->subtype].prog;
      if ((call_fun = FIND_LFUN(p, LFUN_CALL)) >= 0) {
	/* Get the program from the return type. */
	struct identifier *id = ID_FROM_INT(p, call_fun);
	/* FIXME: do it. */
	return 0;
      }
#endif
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

#if 0
static struct find_child_cache_s find_child_cache[FIND_CHILD_HASHSIZE];

int find_child(struct program *parent, struct program *child)
{
  unsigned INT32 h=(parent->id  * 9248339 + child->id);
  h= h % FIND_CHILD_HASHSIZE;
#ifdef PIKE_DEBUG
  if(h>=FIND_CHILD_HASHSIZE)
    Pike_fatal("find_child failed to hash within boundaries.\n");
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
#endif /* 0 */



/* returns 1 if a implements b */
static int low_implements(struct program *a, struct program *b)
{
  int e;
  struct pike_string *s=findstring("__INIT");
  for(e=0;e<b->num_identifier_references;e++)
  {
    struct identifier *bid;
    int i;
    if (b->identifier_references[e].id_flags & (ID_PROTECTED|ID_HIDDEN))
      continue;		/* Skip protected & hidden */
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
    Pike_fatal("Implements_cache failed!\n");
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
    if (b->identifier_references[e].id_flags & (ID_PROTECTED|ID_HIDDEN))
      continue;		/* Skip protected & hidden */

    /* FIXME: What if they aren't protected & hidden in a? */

    bid = ID_FROM_INT(b,e);
    if(s == bid->name) continue;	/* Skip __INIT */
    i = find_shared_string_identifier(bid->name,a);
    if (i == -1) {
      continue;		/* It's ok... */
    }

    /* Note: Uses weaker check for constant integers. */
    if(((bid->run_time_type != PIKE_T_INT) ||
	(ID_FROM_INT(a, i)->run_time_type != PIKE_T_INT)) &&
       !match_types(ID_FROM_INT(a,i)->type, bid->type)) {
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
    Pike_fatal("Implements_cache failed!\n");
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
    Pike_fatal("Implements_cache failed!\n");
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

/* Explains why a is not compatible with b */
void yyexplain_not_compatible(int severity_level,
			      struct program *a, struct program *b)
{
  int e;
  struct pike_string *s=findstring("__INIT");
  int res = 1;
  INT32 a_line = 0;
  INT32 b_line = 0;
  struct pike_string *a_file;
  struct pike_string *b_file;
  DECLARE_CYCLIC();

  /* Optimize the loop somewhat */
  if (a->num_identifier_references < b->num_identifier_references) {
    struct program *tmp = a;
    a = b;
    b = tmp;
  }

  if (BEGIN_CYCLIC(a, b)) {
    END_CYCLIC();
    return;
  }
  SET_CYCLIC_RET(1);

  a_file = get_program_line(a, &a_line);
  b_file = get_program_line(b, &b_line);

  for(e=0;e<b->num_identifier_references;e++)
  {
    struct identifier *bid;
    int i;
    if (b->identifier_references[e].id_flags & (ID_PROTECTED|ID_HIDDEN))
      continue;		/* Skip protected & hidden */

    /* FIXME: What if they aren't protected & hidden in a? */

    bid = ID_FROM_INT(b,e);
    if(s == bid->name) continue;	/* Skip __INIT */
    i = find_shared_string_identifier(bid->name,a);
    if (i == -1) {
      continue;		/* It's ok... */
    }

    /* Note: Uses weaker check for constant integers. */
    if(((bid->run_time_type != PIKE_T_INT) ||
	(ID_FROM_INT(a, i)->run_time_type != PIKE_T_INT)) &&
       !match_types(ID_FROM_INT(a,i)->type, bid->type)) {
      INT32 aid_line = a_line;
      INT32 bid_line = b_line;
      struct pike_string *aid_file = get_identifier_line(a, i, &aid_line);
      struct pike_string *bid_file = get_identifier_line(b, e, &bid_line);
      if (!aid_file) aid_file = a_file;
      if (!bid_file) bid_file = b_file;
      ref_push_string(bid->name);
      ref_push_program(a);
      ref_push_program(b);
      yytype_report(severity_level,
		    aid_file, aid_line, ID_FROM_INT(a, i)->type,
		    bid_file, bid_line, bid->type, 3,
		    "Identifier %s in %O is incompatible with the same in %O.");
    }
  }
  free_string(b_file);
  free_string(a_file);
  END_CYCLIC();
  return;
}

/* Explains why a does not implement b */
void yyexplain_not_implements(int severity_level,
			      struct program *a, struct program *b)
{
  int e;
  struct pike_string *s=findstring("__INIT");
  INT32 a_line = 0;
  INT32 b_line = 0;
  struct pike_string *a_file;
  struct pike_string *b_file;
  DECLARE_CYCLIC();

  if (BEGIN_CYCLIC(a, b)) {
    END_CYCLIC();
    return;
  }
  SET_CYCLIC_RET(1);

  a_file = get_program_line(a, &a_line);
  b_file = get_program_line(b, &b_line);

  for(e=0;e<b->num_identifier_references;e++)
  {
    struct identifier *bid;
    int i;
    if (b->identifier_references[e].id_flags & (ID_PROTECTED|ID_HIDDEN))
      continue;		/* Skip protected & hidden */
    bid = ID_FROM_INT(b,e);
    if(s == bid->name) continue;	/* Skip __INIT */
    i = find_shared_string_identifier(bid->name,a);
    if (i == -1) {
      INT32 bid_line = b_line;
      struct pike_string *bid_file;
      if (b->identifier_references[e].id_flags & (ID_OPTIONAL))
	continue;		/* It's ok... */
      bid_file = get_identifier_line(b, e, &bid_line);
      if (!bid_file) bid_file = b_file;
      yytype_report(severity_level,
		    bid_file, bid_line, bid->type,
		    a_file, a_line, NULL,
		    0, "Missing identifier %S.", bid->name);
      continue;
    }

    if (!pike_types_le(bid->type, ID_FROM_INT(a, i)->type)) {
      INT32 aid_line = a_line;
      INT32 bid_line = b_line;
      struct pike_string *aid_file = get_identifier_line(a, i, &aid_line);
      struct pike_string *bid_file = get_identifier_line(b, e, &bid_line);
      if (!aid_file) aid_file = a_file;
      if (!bid_file) bid_file = b_file;
      if(!match_types(ID_FROM_INT(a,i)->type, bid->type)) {
	yytype_report(severity_level,
		      bid_file, bid_line, bid->type,
		      aid_file, aid_line, ID_FROM_INT(a, i)->type,
		      0, "Type of identifier %S does not match.", bid->name);
      } else {
	yytype_report((severity_level < REPORT_WARNING)?
		      severity_level : REPORT_WARNING,
		      bid_file, bid_line, bid->type,
		      aid_file, aid_line, ID_FROM_INT(a, i)->type,
		      0, "Type of identifier %S is not strictly compatible.",
		      bid->name);
      }
      continue;
    }
  }
  free_string(b_file);
  free_string(a_file);
  END_CYCLIC();
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

  if (!loc.o->prog)
    Pike_error ("Cannot access storage of destructed parent object.\n");

  return loc.o->storage + loc.inherit->storage_offset;
}

PMOD_EXPORT void change_compiler_compatibility(int major, int minor)
{
  CHECK_COMPILER();

  push_int(major);
  push_int(minor);

  safe_apply_current2(PC_CHANGE_COMPILER_COMPATIBILITY_FUN_NUM, 2,
		      "change_compiler_compatibility");
  pop_stack();
}

#ifdef PIKE_USE_MACHINE_CODE

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

void make_area_executable (char *start, size_t len)
{
#ifndef USE_MY_MEXEC_ALLOC
  {
    /* Perform page alignment. */
    void *addr = (void *)(((size_t)start) & ~(page_size-1));
    size_t l = ((start + len) - ((char *)addr) +
		(page_size - 1)) & ~(page_size-1);

    if (mprotect(addr, l, PROT_EXEC | PROT_READ | PROT_WRITE) < 0) {
#if 0
      fprintf(stderr, "%p:%d: mprotect(%p, %lu, 0x%04x): errno: %d\n",
	      start, len, addr, l, PROT_EXEC | PROT_READ | PROT_WRITE, errno);
#endif /* 0 */
    }
  }
#endif /* !USE_MY_MEXEC_ALLOC */

#ifdef HAVE_SYNC_INSTRUCTION_MEMORY
  sync_instruction_memory(start, len);
#elif defined(FLUSH_INSTRUCTION_CACHE)
  FLUSH_INSTRUCTION_CACHE(start, len);
#endif /* HAVE_SYNC_INSTRUCTION_MEMORY || FLUSH_INSTRUCTION_CACHE */
}

void make_program_executable(struct program *p)
{
  if (!p->num_program) return;
  if ((p->event_handler == compat_event_handler) &&
      ((p->num_program * sizeof(p->program[0]) <=
	(NUM_PROG_EVENTS * sizeof(p->event_handler))))) {
    /* Only event handlers. */
    return;
  }

  make_area_executable ((char *) p->program,
			p->num_program * sizeof (p->program[0]));
}
#endif
