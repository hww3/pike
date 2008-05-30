/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: svalue.h,v 1.160 2008/05/30 15:17:13 mast Exp $
*/

#ifndef SVALUE_H
#define SVALUE_H

#include "global.h"
#include "dmalloc.h"

#ifndef STRUCT_ARRAY_DECLARED
#define STRUCT_ARRAY_DECLARED
struct array;
#endif

#ifndef STRUCT_MAPPING_DECLARED
#define STRUCT_MAPPING_DECLARED
struct mapping;
#endif

#ifndef STRUCT_MULTISET_DECLARED
#define STRUCT_MULTISET_DECLARED
struct multiset;
#endif

#ifndef STRUCT_OBJECT_DECLARED
#define STRUCT_OBJECT_DECLARED
struct object;
#endif

#ifndef STRUCT_PROGRAM_DECLARED
#define STRUCT_PROGRAM_DECLARED
struct program;
#endif

#ifndef STRUCT_PIKE_STRING_DECLARED
#define STRUCT_PIKE_STRING_DECLARED
struct pike_string;
#endif

#ifndef STRUCT_CALLABLE_DECLARED
#define STRUCT_CALLABLE_DECLARED
struct callable;
#endif

#ifndef STRUCT_NODE_S_DECLARED
#define STRUCT_NODE_S_DECLARED
struct node_s;
typedef struct node_s node;
#endif

struct processing
{
  struct processing *next;
  void *pointer_a, *pointer_b;
};
   
struct ref_dummy;

/** the union of possible types in an svalue.
*/
union anything
{
  INT_TYPE integer;		/**< Union initializations assume this first. */
  struct callable *efun;
  struct array *array;
  struct mapping *mapping;
  struct multiset *multiset;
  struct object *object;
  struct program *program;
  struct pike_string *string;
  struct pike_type *type;
  INT32 *refs;
  struct ref_dummy *dummy;
  FLOAT_TYPE float_number;
  int identifier;		/**< Used with T_OBJ_INDEX. */
  struct svalue *lval;		/**< Used with T_SVALUE_PTR. */
  void *ptr;
#ifdef DEBUG_MALLOC
  char *loc;			/* Only used for free svalue debugging. */
#endif
};

#ifndef STRUCT_SVALUE_DECLARED
#define STRUCT_SVALUE_DECLARED
#endif

/* Note: At least multisets overlays the type field and uses the top 4
 * bits in it internally. */

/**
 */
struct svalue
{
  unsigned INT16 type; /**< the data type, see PIKE_T_... */
  unsigned INT16 subtype; /**< used to store the zero type, among others */
  union anything u; /**< contains the value */
};

#define PIKE_T_ARRAY 0
#define PIKE_T_MAPPING 1
#define PIKE_T_MULTISET 2
#define PIKE_T_OBJECT 3
#define PIKE_T_FUNCTION 4
#define PIKE_T_PROGRAM 5
#define PIKE_T_STRING 6
#define PIKE_T_TYPE 7
#define PIKE_T_INT 8
#define PIKE_T_FLOAT 9

/* The types above are valid types in svalues.
 * The following are only used by the internal systems.
 */

#define PIKE_T_ZERO  14	/**< Can return 0, but nothing else */


#define T_UNFINISHED 15

#define T_VOID       16 /**< Can't return any value. Also used on stack to fill out the second
 * svalue on an lvalue when it isn't used. */


#define T_MANY       17

#define PIKE_T_INT_UNTYPED  18 /* Optimization of int type size */

#define PIKE_T_GET_SET 32	/* Getter setter.
				 * Only valid in struct identifier */

/* Type to put in freed svalues. Only the type field in such svalues
 * is defined. Freeing a PIKE_T_FREE svalue is allowed and does
 * nothing. mark_free_svalue() is preferably used to set this type.
 *
 * Traditionally T_INT has been used for this without setting a proper
 * subtype; if T_INT is to be used then the subtype must be set to
 * NUMBER_NUMBER.
 *
 * PIKE_T_FREE svalues are recorded as BIT_INT in type hint fields.
 */
#define PIKE_T_FREE 237

#define PIKE_T_ATTRIBUTE 238	/* Attribute node. */
#define PIKE_T_NSTRING 239	/* Narrow string. Only for serialization. */
#define PIKE_T_RING 240
#define PIKE_T_NAME 241		/**< Named type. */
#define PIKE_T_SCOPE 243	/**< Not supported yet */
#define PIKE_T_TUPLE 244	/**< Not supported yet */
#define T_ASSIGN 245
#define T_DELETED 246
#define PIKE_T_UNKNOWN 247

/** svalue.u.identifer is an identifier index in an object. Primarily
 * used in lvalues on stack, but can also occur in arrays containing
 * lvalue pairs. */
#define T_OBJ_INDEX 248

/** svalue.u.lval points to an svalue. Primarily used in lvalues on
 * stack, but can also occur in arrays containing lvalue pairs. */
#define T_SVALUE_PTR 249

#define T_ARRAY_LVALUE 250
#define PIKE_T_MIXED 251
#define T_NOT 253
#define T_AND 254
#define T_OR 255

/* These are only used together with describe() and friends. */
#define T_STORAGE 10000
#define T_MAPPING_DATA 10001
#define T_PIKE_FRAME 10002
#define T_MULTISET_DATA 10003
#define T_STRUCT_CALLABLE 10004

#define tArr(VAL) "\000" VAL
#define tArray tArr(tMix)
#define tMap(IND,VAL) "\001" IND VAL
#define tMapping tMap(tMix,tMix)
#define tSet(IND) "\002" IND
#define tMultiset tSet(tMix)
#define tObj "\003\000\000\000\000\000"

#define tFuncV(ARGS,REST,RET) MagictFuncV(RET,REST,ARGS)
#define tFunc(ARGS,RET) MagictFunc(RET,ARGS)

#define tTuple(T1,T2)		"\364" T1 T2
#define tTriple(T1,T2,T3)	tTuple(T1, tTuple(T2, T3))
#define tQuad(T1,T2,T3,T4)	tTriple(tTuple(T1, T2), T3, T4)

/* These two magic funcions are used to make MSVC++ work
 * even if 'ARGS' is empty.
 */
#define MagictFuncV(RET,REST,ARGS) "\004" ARGS "\021" REST RET
#define MagictFunc(RET,ARGS) tFuncV(ARGS "", tVoid, RET)
#define tFunction tFuncV("" ,tOr(tZero,tVoid),tOr(tMix,tVoid))
#define tNone ""
#define tPrg(X) "\005" X
#define tProgram(X) "\005" X
#define tStr "\006"
#define tString "\006"
#define tStr0 "\357" tZero
#define tStr7 "\357" "\010\000\000\000\000\000\000\000\177"
#define tStr8 "\357" "\010\000\000\000\000\000\000\000\377"
#define tStr16 "\357" "\010\000\000\000\000\000\000\377\377"
#define tStr32 "\006"
#define tType(T) "\007" T
#define tInt "\022"
#define tInt0 "\010\000\000\000\000\000\000\000\000"
#define tInt1 "\010\000\000\000\001\000\000\000\001"
#define tInt2 "\010\000\000\000\002\000\000\000\002"
#define tInt01 "\010\000\000\000\000\000\000\000\001"
#define tInt02 "\010\000\000\000\000\000\000\000\002"
#define tInt03 "\010\000\000\000\000\000\000\000\003"
#define tInt04 "\010\000\000\000\000\000\000\000\004"
#define tInt05 "\010\000\000\000\000\000\000\000\005"
#define tInt06 "\010\000\000\000\000\000\000\000\006"
#define tInt07 "\010\000\000\000\000\000\000\000\007"
#define tInt08 "\010\000\000\000\000\000\000\000\010"
#define tInt09 "\010\000\000\000\000\000\000\000\011"
#define tIntPos "\010\000\000\000\000\177\377\377\377"
#define tInt1Plus "\010\000\000\000\001\177\377\377\377"
#define tInt2Plus "\010\000\000\000\002\177\377\377\377"
#define tInt_10 "\010\377\377\377\377\000\000\000\000"
#define tInt_11 "\010\377\377\377\377\000\000\000\001"
#define tByte "\010\000\000\000\000\000\000\000\377"
#define tFlt "\011"
#define tFloat "\011"

#define tZero "\016"
#define tVoid "\020"
#define tVar(X) #X
#define tSetvar(X,Y) "\365" #X Y
#define tScope(X,T) "\363" #X Y
#define tNot(X) "\375" X
#define tAnd(X,Y) "\376" X Y
#define tOr(X,Y) "\377" X Y
#define tOr3(X,Y,Z) tOr(X,tOr(Y,Z))
#define tOr4(X,Y,Z,A) tOr(X,tOr(Y,tOr(Z,A)))
#define tOr5(X,Y,Z,A,B) tOr(X,tOr(Y,tOr(Z,tOr(A,B))))
#define tOr6(X,Y,Z,A,B,C) tOr(X,tOr(Y,tOr(Z,tOr(A,tOr(B,C)))))
#define tOr7(X,Y,Z,A,B,C,D) tOr(X,tOr(Y,tOr(Z,tOr(A,tOr(B,tOr(C,D))))))
#define tOr8(A,B,C,D,E,F,G,H) tOr(A,tOr7(B,C,D,E,F,G,H))
#define tOr9(A,B,C,D,E,F,G,H,I) tOr(A,tOr8(B,C,D,E,F,G,H,I))
#define tMix "\373"
#define tMixed "\373"
#define tComplex tOr6(tArray,tMapping,tMultiset,tObj,tFunction,tPrg(tObj))
#define tStringIndicable tOr5(tMapping,tObj,tFunction,tPrg(tObj),tMultiset)
#define tRef tOr(tString,tComplex)
#define tIfnot(X,Y) tAnd(tNot(X),Y)
#define tAny tOr(tVoid,tMix)
#define tAttr(X,Y) "\356\0"X"\0"Y
#define tName(X,Y) "\361\0"X"\0"Y
#if PIKE_BYTEORDER == 1234
/* Little endian */
#define tAttr1(X,Y) "\356\5"X"\0\0"Y
#define tAttr2(X,Y) "\356\6"X"\0\0\0\0"Y
#define tName1(X,Y) "\361\5"X"\0\0"Y
#define tName2(X,Y) "\361\6"X"\0\0\0\0"Y
#else /* PIKE_BYTEORDER != 1234 */
/* Big endian */
#define tAttr1(X,Y) "\356\1"X"\0\0"Y
#define tAttr2(X,Y) "\356\2"X"\0\0\0\0"Y
#define tName1(X,Y) "\361\1"X"\0\0"Y
#define tName2(X,Y) "\361\2"X"\0\0\0\0"Y
#endif /* PIKE_BYTEORDER == 1234 */

/* Some convenience macros for common attributes. */
#define tSprintfFormat(X)	tAttr("sprintf_format", X)
#define tSprintfArgs(X)		tAttr("sprintf_args", X)
#define tDeprecated(X)		tAttr("deprecated", X)

#define tSimpleCallable tOr3(tArray,tFunction,tObj)
#define tCallable tOr3(tArr(tSimpleCallable),tFunction,tObj)

#define BIT_ARRAY (1<<PIKE_T_ARRAY)
#define BIT_MAPPING (1<<PIKE_T_MAPPING)
#define BIT_MULTISET (1<<PIKE_T_MULTISET)
#define BIT_OBJECT (1<<PIKE_T_OBJECT)
#define BIT_FUNCTION (1<<PIKE_T_FUNCTION)
#define BIT_PROGRAM (1<<PIKE_T_PROGRAM)
#define BIT_STRING (1<<PIKE_T_STRING)
#define BIT_TYPE (1<<PIKE_T_TYPE)
#define BIT_INT (1<<PIKE_T_INT)
#define BIT_FLOAT (1<<PIKE_T_FLOAT)

#define BIT_ZERO (1<<PIKE_T_ZERO)

/** Used to signify that the type field hasn't been set according to
 * reality. */
#define BIT_UNFINISHED (1 << T_UNFINISHED)

/** This is only used in typechecking to signify that this 
 * argument may be omitted.
 */
#define BIT_VOID (1 << T_VOID)

/** This is used in typechecking to signify that the rest of the
 * arguments has to be of this type.
 */
#define BIT_MANY (1 << T_MANY)

#define BIT_NOTHING 0
#define BIT_MIXED 0x7fff
#define BIT_BASIC (BIT_INT|BIT_FLOAT|BIT_STRING|BIT_TYPE)
#define BIT_COMPLEX (BIT_ARRAY|BIT_MULTISET|BIT_OBJECT|BIT_PROGRAM|BIT_MAPPING|BIT_FUNCTION)
#define BIT_CALLABLE (BIT_FUNCTION|BIT_PROGRAM|BIT_ARRAY|BIT_OBJECT)
#define BIT_REF_TYPES (BIT_STRING|BIT_TYPE|BIT_COMPLEX)

/* Max type which contains svalues */
#define MAX_COMPLEX PIKE_T_PROGRAM
/* Max type with ref count */
#define MAX_REF_TYPE PIKE_T_TYPE
/* Max type handled by svalue primitives */
#define MAX_TYPE PIKE_T_FLOAT

#define NUMBER_NUMBER 0
#define NUMBER_UNDEFINED 1
#define NUMBER_DESTRUCTED 2

#define FUNCTION_BUILTIN USHRT_MAX

PMOD_EXPORT const struct svalue svalue_int_zero;
#ifdef HAVE_UNION_INIT
PMOD_EXPORT const struct svalue svalue_int_one;
#else
/* The value 1 is initialized first thing in init_pike. */
PMOD_EXPORT struct svalue svalue_int_one;
#endif

#define is_gt(a,b) is_lt(b,a)
#define is_ge(a,b) is_le(b,a)

/* SAFE_IS_ZERO is compatible with the old IS_ZERO, but you should
 * consider using UNSAFE_IS_ZERO instead, since exceptions thrown from
 * `! functions will be propagated correctly then. */
#define UNSAFE_IS_ZERO(X) ((X)->type==PIKE_T_INT?(X)->u.integer==0:(1<<(X)->type)&(BIT_OBJECT|BIT_FUNCTION)?!svalue_is_true(X):0)
#define SAFE_IS_ZERO(X) ((X)->type==PIKE_T_INT?(X)->u.integer==0:(1<<(X)->type)&(BIT_OBJECT|BIT_FUNCTION)?!safe_svalue_is_true(X):0)

#define IS_UNDEFINED(X) (check_svalue (X), (X)->type==PIKE_T_INT&&(X)->subtype==1)

#define IS_DESTRUCTED(X) \
  (((X)->type == PIKE_T_OBJECT || (X)->type==PIKE_T_FUNCTION) && !(X)->u.object->prog)

#define check_destructed(S) \
do{ \
  struct svalue *_s=(S); \
  if(IS_DESTRUCTED(_s)) { \
    free_object(_s->u.object); \
    _s->type = PIKE_T_INT; \
    _s->subtype = NUMBER_DESTRUCTED ; \
    _s->u.integer = 0; \
  } \
}while(0)

/* var MUST be a variable!!! */
#define safe_check_destructed(var) do{ \
  if((var->type == PIKE_T_OBJECT || var->type==PIKE_T_FUNCTION) && !var->u.object->prog) \
    var=&svalue_int_zero; \
}while(0)

#define check_short_destructed(U,T) \
do{ \
  union anything *_u=(U); \
  if(( (1<<(T)) & (BIT_OBJECT | BIT_FUNCTION) ) && \
     _u->object && !_u->object->prog) { \
    free_object(_u->object); \
    _u->object = 0; \
  } \
}while(0)


#ifdef PIKE_RUN_UNLOCKED
#define add_ref(X) pike_atomic_inc32(&(X)->refs)
#define sub_ref(X) pike_atomic_dec_and_test32(&(X)->refs)

#if 0
#define IF_LOCAL_MUTEX(X) X
#define USE_LOCAL_MUTEX
#define pike_lock_data(X) mt_lock(&(X)->mutex)
#define pike_unlock_data(X) mt_unlock(&(X)->mutex)
#else
#define IF_LOCAL_MUTEX(X)
#define pike_lock_data(X) pike_lockmem((X))
#define pike_unlock_data(X) pike_unlockmem((X))
#endif

#else
#define IF_LOCAL_MUTEX(X)
#define add_ref(X) (void)((X)->refs++)
#define sub_ref(X) (--(X)->refs > 0)
#define pike_lock_data(X) (void)(X)
#define pike_unlock_data(X) (void)(X)
#endif


#ifdef PIKE_DEBUG
PMOD_EXPORT extern void describe(void *); /* defined in gc.c */
PMOD_EXPORT extern const char msg_type_error[];
PMOD_EXPORT extern const char msg_assign_svalue_error[];

#define IS_INVALID_TYPE(T)						\
  ((T > MAX_TYPE && T < T_OBJ_INDEX && T != T_VOID) || T > T_ARRAY_LVALUE)

#define check_type(T) do {						\
    TYPE_T typ_ = (T);							\
    if (IS_INVALID_TYPE (typ_)) Pike_fatal(msg_type_error, typ_);	\
  } while (0)

#define check_svalue_type(S) do {					\
    const struct svalue *sval_ = (S);					\
    TYPE_T typ_ = sval_->type;						\
    if (IS_INVALID_TYPE (typ_)) debug_svalue_type_error (sval_);	\
  } while (0)

#define check_svalue(S) debug_check_svalue(dmalloc_check_svalue(S,DMALLOC_LOCATION()))

void low_thorough_check_short_svalue (const union anything *u, TYPE_T type);
#define thorough_check_short_svalue(U, T) do {				\
    union anything *anyth_ = (U);					\
    TYPE_T typ_ = (T);							\
    check_short_svalue (anyth_, typ_);					\
    if (d_flag <= 50) /* Done directly by check_svalue otherwise. */	\
      if (typ_ <= MAX_REF_TYPE)						\
	low_thorough_check_short_svalue (anyth_, typ_);			\
  } while (0)
#define thorough_check_svalue(S) do {					\
    struct svalue *sval_ = (S);						\
    check_svalue (sval_);						\
    if (d_flag <= 50) /* Done directly by check_svalue otherwise. */	\
      if (sval_->type <= MAX_REF_TYPE)					\
	low_thorough_check_short_svalue (&sval_->u, sval_->type);	\
  } while (0)

void check_short_svalue(const union anything *u, TYPE_T type);
PMOD_EXPORT void debug_svalue_type_error (const struct svalue *s);
PMOD_EXPORT void debug_check_svalue(const struct svalue *s);
void debug_check_type_hint (const struct svalue *svals, size_t num, TYPE_FIELD type_hint);
PMOD_EXPORT void real_gc_mark_external_svalues(const struct svalue *s, ptrdiff_t num,
					       const char *place);

PMOD_EXPORT extern const char msg_sval_obj_wo_refs[];
#define check_refs(S) do {\
 if((S)->type <= MAX_REF_TYPE && (!(S)->u.refs || (S)->u.refs[0] < 0)) { \
   fprintf (stderr, msg_sval_obj_wo_refs);				\
   describe((S)->u.refs);						\
   Pike_fatal(msg_sval_obj_wo_refs);					\
 } }while(0)

PMOD_EXPORT extern const char msg_ssval_obj_wo_refs[];
#define check_refs2(S,T) do { \
if((T) <= MAX_REF_TYPE && (S)->refs && (S)->refs[0] <= 0) {\
  fprintf (stderr, msg_ssval_obj_wo_refs);		   \
  describe((S)->refs);					   \
  Pike_fatal(msg_ssval_obj_wo_refs);			   \
} }while(0)

#define check_type_hint(SVALS, NUM, TYPE_HINT)				\
  debug_check_type_hint ((SVALS), (NUM), (TYPE_HINT))

#ifdef DEBUG_MALLOC
static INLINE struct svalue *dmalloc_check_svalue(struct svalue *s, char *l)
{
#if 0
  /* What's this supposed to accomplish? Dmalloc tracks memory blocks,
   * not single svalues that point to them. /mast */
  debug_malloc_update_location(s,l);
#endif
#if 1
  if(s && s->type <= MAX_REF_TYPE)
    debug_malloc_update_location(s->u.refs,l);
#endif
  return s;
}

static INLINE struct svalue *dmalloc_check_svalues(struct svalue *s, size_t num, char *l)
{
  while (num--) dmalloc_check_svalue (s + num, l);
  return s;
}

static INLINE union anything *dmalloc_check_union(union anything *u,int type, char * l)
{
#if 0
  debug_malloc_update_location(u,l);
#endif
#if 1
  if(u && type <= MAX_REF_TYPE)
    debug_malloc_update_location(u->refs,l);
#endif
  return u;
}

#undef add_ref
#undef sub_ref

#ifdef PIKE_RUN_UNLOCKED
#define add_ref(X) pike_atomic_inc32((INT32 *)debug_malloc_update_location( &((X)->refs), DMALLOC_NAMED_LOCATION(" add_ref")))
#define sub_ref(X) pike_atomic_dec_and_test32((INT32 *)debug_malloc_update_location( &((X)->refs), DMALLOC_NAMED_LOCATION(" sub_ref")))
#else
#define add_ref(X) (((INT32 *)debug_malloc_update_location( &((X)->refs), DMALLOC_NAMED_LOCATION(" add_ref")))[0]++)
#define sub_ref(X) (--((INT32 *)debug_malloc_update_location( &((X)->refs), DMALLOC_NAMED_LOCATION(" sub_ref")))[0] > 0)
#endif

#else  /* !DEBUG_MALLOC */
#define dmalloc_check_svalue(S,L) (S)
#define dmalloc_check_svalues(S,L,N) (S)
#define dmalloc_check_union(U,T,L) (U)

#endif	/* !DEBUG_MALLOC */

/* To be used for type checking in macros. */
static INLINE struct array *pass_array (struct array *a) {return a;}
static INLINE struct mapping *pass_mapping (struct mapping *m) {return m;}
static INLINE struct multiset *pass_multiset (struct multiset *l) {return l;}
static INLINE struct object *pass_object (struct object *o) {return o;}
static INLINE struct program *pass_program (struct program *p) {return p;}
static INLINE struct pike_string *pass_string (struct pike_string *s) {return s;}
static INLINE struct pike_type *pass_type (struct pike_type *t) {return t;}
static INLINE struct callable *pass_callable (struct callable *c) {return c;}

#else  /* !PIKE_DEBUG */

#define check_svalue(S) 0
#define check_short_svalue(U, T) 0
#define check_type(T) do {} while (0)
#define check_svalue_type(S) do {} while (0)
#define check_refs(S) do {} while (0)
#define check_refs2(S,T) do {} while (0)
#define check_type_hint(SVALS, NUM, TYPE_HINT) 0
#define dmalloc_check_svalue(S,L) (S)
#define dmalloc_check_svalues(S,L,N) (S)
#define dmalloc_check_union(U,T,L) (U)

#define pass_array(A) (A)
#define pass_mapping(M) (M)
#define pass_multiset(L) (L)
#define pass_object(O) (O)
#define pass_program(P) (P)
#define pass_string(S) (S)
#define pass_type(T) (T)
#define pass_callable(C) (C)

#endif	/* !PIKE_DEBUG */

/* This marks an svalue as free. After this it may only be used as
 * input to the svalue free functions (which do nothing with it). Only
 * the type field is defined (see PIKE_T_FREE above). */
#define mark_free_svalue(X) do {					\
    struct svalue *_X__ = (X);						\
    DO_IF_DMALLOC (							\
      _X__->u.loc = " " __FILE__ ":" DEFINETOSTR (__LINE__);		\
      _X__->u.loc++; /* Attempt to achieve an odd address. */		\
    );									\
    PIKE_MEM_WO(*_X__);							\
    _X__->type = PIKE_T_FREE;						\
  } while (0)

/* This is a debug macro to assert that an svalue is free and
 * shouldn't be read at all until it's overwritten. As opposed to
 * mark_free_svalue, it is not valid input to the svalue free
 * functions and no field in it is defined. */
#define assert_free_svalue(X) do {					\
    DO_IF_DEBUG (							\
      struct svalue *_X__ = (X);					\
      _X__->type = PIKE_T_UNKNOWN;					\
      DO_IF_DMALLOC (							\
	_X__->u.loc = " " __FILE__ ":" DEFINETOSTR (__LINE__);		\
	_X__->u.loc++; /* Attempt to achieve an odd address. */		\
      );								\
      DO_IF_NOT_DMALLOC (_X__->u.ptr = (void *) -1);			\
      PIKE_MEM_WO (*_X__);						\
    );									\
  } while (0)

/* This define
 * should check that the svalue address (X) is on the local stack,
 * the processor stack or in a locked memory object
 *
 * Or, it could just try to make sure it's not in an unlocked memory
 * object...
 */
#define assert_svalue_locked(X)


#define swap_svalues_unlocked(X,Y)  do {		\
  struct svalue *_a=(X);				\
  struct svalue *_b=(Y);				\
  struct svalue _tmp;					\
  assert_svalue_locked(_a); assert_svalue_locked(_b);	\
  dmalloc_touch_svalue(_a);				\
  dmalloc_touch_svalue(_b);				\
  _tmp=*_a; *_a=*_b; *_b=_tmp;				\
}while(0)

/* Handles PIKE_T_FREE. */
#define free_svalue_unlocked(X) do {				\
  struct svalue *_s=(X);					\
  assert_svalue_locked(_s);					\
  DO_IF_DEBUG (							\
    if (_s->type != PIKE_T_FREE) {				\
      check_svalue_type(_s);					\
      check_refs(_s);						\
    }								\
  );								\
  if (_s->type > MAX_REF_TYPE)					\
    assert_free_svalue (_s);					\
  else {							\
    DO_IF_DEBUG (						\
      DO_IF_PIKE_CLEANUP (					\
	if (gc_external_refs_zapped)				\
	  gc_check_zapped (_s->u.ptr, _s->type, __FILE__, __LINE__))); \
    if (sub_ref(_s->u.dummy) <=0)				\
      really_free_svalue(_s);					\
    else							\
      assert_free_svalue (_s);					\
  }								\
}while(0)

#define free_short_svalue_unlocked(X,T) do {				\
  union anything *_s=(X); TYPE_T _t=(T);				\
  check_type(_t); check_refs2(_s,_t);					\
  assert_svalue_locked(_s);						\
  if(_t<=MAX_REF_TYPE && _s->refs) {					\
    DO_IF_DEBUG (							\
      DO_IF_PIKE_CLEANUP (						\
	if (gc_external_refs_zapped)					\
	  gc_check_zapped (_s->ptr, _t, __FILE__, __LINE__)));		\
    if(sub_ref(_s->dummy) <= 0) really_free_short_svalue(_s,_t);	\
  }									\
  DO_IF_DMALLOC(_s->refs=(void *)-1);					\
  PIKE_MEM_WO(_s->refs);						\
}while(0)

/* Handles PIKE_T_FREE. */
#define add_ref_svalue_unlocked(X) do {				\
  struct svalue *_tmp=(X);					\
  DO_IF_DEBUG (							\
    if (_tmp->type != PIKE_T_FREE) {				\
      check_svalue_type(_tmp);					\
      check_refs(_tmp);						\
    }								\
  );								\
  if(_tmp->type <= MAX_REF_TYPE) add_ref(_tmp->u.dummy);	\
}while(0)

/* Handles PIKE_T_FREE. */
#define assign_svalue_no_free_unlocked(X,Y) do {	\
  struct svalue *_to=(X);				\
  const struct svalue *_from=(Y);			\
  DO_IF_DEBUG (						\
    if (_from->type != PIKE_T_FREE) {			\
      check_svalue_type(_from);				\
      check_refs(_from);				\
    }							\
    if (_to == _from)					\
      Pike_fatal(msg_assign_svalue_error, _to);		\
  );							\
  *_to=*_from;						\
  if(_to->type <= MAX_REF_TYPE) add_ref(_to->u.dummy);	\
}while(0)

/* Handles PIKE_T_FREE. */
#define assign_svalue_unlocked(X,Y) do {	\
  struct svalue *_to2=(X);			\
  const struct svalue *_from2=(Y);		\
  if (_to2 != _from2) {				\
    free_svalue(_to2);				\
     assign_svalue_no_free(_to2, _from2);	\
  }						\
}while(0)

/* Handles PIKE_T_FREE. */
#define move_svalue(TO, FROM) do {					\
    struct svalue *_to = (TO);						\
    struct svalue *_from = (FROM);					\
    dmalloc_touch_svalue(_from);					\
    *_to = *_from;							\
    assert_free_svalue (_from);						\
  } while (0)

extern const struct svalue dest_ob_zero;

/* Handles PIKE_T_FREE. */
#define free_mixed_svalues(X,Y) do {		\
  struct svalue *s_=(X);			\
  ptrdiff_t num_=(Y);				\
  while(num_--)					\
  {						\
    dmalloc_touch_svalue(s_);			\
    free_svalue(s_++);				\
  }						\
}while(0)

/* Handles PIKE_T_FREE. */
#ifdef DEBUG_MALLOC
#define free_svalues(X,Y,Z) debug_free_svalues((X),(Y),(Z), DMALLOC_NAMED_LOCATION(" free_svalues"));
#else
#define free_svalues(X,Y,Z) debug_free_svalues((X),(Y),(Z));
#endif

#define low_clear_svalues(X,Y,N) do {		\
  struct svalue *s_=(X);			\
  ptrdiff_t num_=(Y);				\
  for(;num_-- > 0;s_++)				\
  {						\
      s_->type=PIKE_T_INT;			\
      s_->subtype=(N);				\
      s_->u.integer=0;				\
  }						\
}while(0)

#define clear_svalues(X,Y) low_clear_svalues((X),(Y),NUMBER_NUMBER)
#define clear_svalues_undefined(X,Y) low_clear_svalues((X),(Y),NUMBER_UNDEFINED)

#define really_free_short_svalue(U, TYPE) do {				\
    union anything *any_ = (U);						\
    debug_malloc_touch (any_->ptr);					\
    really_free_short_svalue_ptr (&any_->ptr, (TYPE));			\
  } while (0)

/* Prototypes begin here */
PMOD_EXPORT void really_free_short_svalue_ptr(void **s, TYPE_T type);
PMOD_EXPORT void really_free_svalue(struct svalue *s);
PMOD_EXPORT void do_free_svalue(struct svalue *s);
PMOD_EXPORT void debug_free_svalues(struct svalue *s, size_t num, INT32 type_hint DMALLOC_LINE_ARGS);
PMOD_EXPORT void debug_free_mixed_svalues(struct svalue *s, size_t num, INT32 type_hint DMALLOC_LINE_ARGS);
PMOD_EXPORT TYPE_FIELD assign_svalues_no_free(struct svalue *to,
					      const struct svalue *from,
					      size_t num,
					      TYPE_FIELD type_hint);
PMOD_EXPORT TYPE_FIELD assign_svalues(struct svalue *to,
				      const struct svalue *from,
				      size_t num,
				      TYPE_FIELD type_hint);
PMOD_EXPORT void assign_to_short_svalue(union anything *u,
			    TYPE_T type,
			    const struct svalue *s);
PMOD_EXPORT void assign_to_short_svalue_no_free(union anything *u,
				    TYPE_T type,
				    const struct svalue *s);
PMOD_EXPORT void assign_from_short_svalue_no_free(struct svalue *s,
				      const union anything *u,
				      TYPE_T type);
PMOD_EXPORT void assign_short_svalue_no_free(union anything *to,
				 const union anything *from,
				 TYPE_T type);
PMOD_EXPORT void assign_short_svalue(union anything *to,
			 const union anything *from,
			 TYPE_T type);
PMOD_EXPORT unsigned INT32 hash_svalue(const struct svalue *s);
PMOD_EXPORT int svalue_is_true(const struct svalue *s);
PMOD_EXPORT int safe_svalue_is_true(const struct svalue *s);
PMOD_EXPORT int is_identical(const struct svalue *a, const struct svalue *b);
PMOD_EXPORT int is_eq(const struct svalue *a, const struct svalue *b);
PMOD_EXPORT int low_is_equal(const struct svalue *a,
		 const struct svalue *b,
		 struct processing *p);
PMOD_EXPORT int low_short_is_equal(const union anything *a,
		       const union anything *b,
		       TYPE_T type,
		       struct processing *p);
PMOD_EXPORT int is_equal(const struct svalue *a, const struct svalue *b);
PMOD_EXPORT int is_lt(const struct svalue *a, const struct svalue *b);
PMOD_EXPORT int is_le(const struct svalue *a, const struct svalue *b);
PMOD_EXPORT void describe_svalue(const struct svalue *s,int indent,struct processing *p);
PMOD_EXPORT void safe_describe_svalue(const struct svalue *s,int indent,struct processing *p);
PMOD_EXPORT void print_svalue (FILE *out, const struct svalue *s);
PMOD_EXPORT void safe_print_svalue (FILE *out, const struct svalue *s);
PMOD_EXPORT void print_short_svalue (FILE *out, const union anything *a, TYPE_T type);
PMOD_EXPORT void safe_print_short_svalue (FILE *out, const union anything *a, TYPE_T type);
PMOD_EXPORT void print_svalue_compact (FILE *out, const struct svalue *s);
PMOD_EXPORT void safe_print_svalue_compact (FILE *out, const struct svalue *s);
PMOD_EXPORT void print_short_svalue_compact (FILE *out, const union anything *a, TYPE_T type);
PMOD_EXPORT void safe_print_short_svalue_compact (FILE *out, const union anything *a, TYPE_T type);
PMOD_EXPORT void copy_svalues_recursively_no_free(struct svalue *to,
						  const struct svalue *from,
						  size_t num,
						  struct mapping *m);
PMOD_EXPORT void real_gc_check_svalues(const struct svalue *s, size_t num);
void gc_check_weak_svalues(const struct svalue *s, size_t num);
PMOD_EXPORT void real_gc_check_short_svalue(const union anything *u, TYPE_T type);
void gc_check_weak_short_svalue(const union anything *u, TYPE_T type);
PMOD_EXPORT TYPE_FIELD real_gc_mark_svalues(struct svalue *s, size_t num);
TYPE_FIELD gc_mark_weak_svalues(struct svalue *s, size_t num);
int real_gc_mark_short_svalue(union anything *u, TYPE_T type);
int gc_mark_weak_short_svalue(union anything *u, TYPE_T type);
int gc_mark_without_recurse(struct svalue *s);
int gc_mark_weak_without_recurse(struct svalue *s);
PMOD_EXPORT TYPE_FIELD real_gc_cycle_check_svalues(struct svalue *s, size_t num);
TYPE_FIELD gc_cycle_check_weak_svalues(struct svalue *s, size_t num);
PMOD_EXPORT int real_gc_cycle_check_short_svalue(union anything *u, TYPE_T type);
int gc_cycle_check_weak_short_svalue(union anything *u, TYPE_T type);
void real_gc_free_svalue(struct svalue *s);
void real_gc_free_short_svalue(union anything *u, TYPE_T type);
PMOD_EXPORT INT32 pike_sizeof(const struct svalue *s);
int svalues_are_constant(struct svalue *s,
			 INT32 num,
			 TYPE_FIELD hint,
			 struct processing *p);

#define gc_cycle_check_without_recurse gc_mark_without_recurse
#define gc_cycle_check_weak_without_recurse gc_mark_without_recurse

#define gc_mark_external_svalues(S, NUM, PLACE) do {			\
    size_t num__ = (NUM);						\
    real_gc_mark_external_svalues (					\
      dmalloc_check_svalues ((S), num__, DMALLOC_LOCATION()), num__, (PLACE)); \
  } while (0)
#define gc_check_svalues(S, NUM) do {					\
    size_t num__ = (NUM);						\
    real_gc_check_svalues (						\
      dmalloc_check_svalues ((S), num__, DMALLOC_LOCATION()), num__);	\
  } while (0)

#ifdef DEBUG_MALLOC
static INLINE TYPE_FIELD dmalloc_gc_mark_svalues (struct svalue *s, size_t num, char *l)
  {return real_gc_mark_svalues (dmalloc_check_svalues (s, num, l), num);}
#define gc_mark_svalues(S, NUM) dmalloc_gc_mark_svalues ((S), (NUM), DMALLOC_LOCATION())
static INLINE TYPE_FIELD dmalloc_gc_cycle_check_svalues (struct svalue *s, size_t num, char *l)
  {return real_gc_cycle_check_svalues (dmalloc_check_svalues (s, num, l), num);}
#define gc_cycle_check_svalues(S, NUM) dmalloc_gc_cycle_check_svalues ((S), (NUM), DMALLOC_LOCATION())
#else
#define gc_mark_svalues real_gc_mark_svalues
#define gc_cycle_check_svalues real_gc_cycle_check_svalues
#endif

#define gc_check_short_svalue(U,T) real_gc_check_short_svalue(dmalloc_check_union((U),(T),DMALLOC_LOCATION()),T)
#define gc_mark_short_svalue(U,T) real_gc_mark_short_svalue(dmalloc_check_union((U),(T),DMALLOC_LOCATION()),T)
#define gc_cycle_check_short_svalue(U,T) real_gc_cycle_check_short_svalue(dmalloc_check_union((U),(T),DMALLOC_LOCATION()),(T))
#define gc_free_svalue(S) real_gc_free_svalue(dmalloc_check_svalue(S,DMALLOC_LOCATION()))
#define gc_free_short_svalue(U,T) real_gc_free_short_svalue(dmalloc_check_union((U),(T),DMALLOC_LOCATION()),(T))

#ifndef NO_PIKE_SHORTHAND

#define T_ARRAY    PIKE_T_ARRAY
#define T_MAPPING  PIKE_T_MAPPING
#define T_MULTISET PIKE_T_MULTISET
#define T_OBJECT   PIKE_T_OBJECT
#define T_FUNCTION PIKE_T_FUNCTION
#define T_PROGRAM  PIKE_T_PROGRAM
#define T_STRING   PIKE_T_STRING
#define T_TYPE     PIKE_T_TYPE
#define T_FLOAT    PIKE_T_FLOAT
#define T_INT      PIKE_T_INT

#define T_ZERO	   PIKE_T_ZERO

#define T_TUPLE	   PIKE_T_TUPLE
#define T_SCOPE	   PIKE_T_SCOPE
#define T_MIXED    PIKE_T_MIXED

#endif /* !NO_PIKE_SHORTHAND */

#if 0 /* PIKE_RUN_UNLOCKED */

#include "pike_error.h"

/*#define swap_svalues swap_svalues*/
/*#define free_svalue free_svalue_unlocked*/
/*#define free_short_svalue free_short_svalue_unlocked */
/*#define add_ref_svalue add_ref_svalue_unlocked*/

/*
 * These don't work right now - Hubbe
 */
#define assign_svalue_no_free assign_svalue_no_free_unlocked
#define assign_svalue assign_svalue_unlocked

/* FIXME:
 * These routines assumes that pointers are 32 bit
 * and svalues 64 bit!!!!! - Hubbe
 */

#ifndef swap_svalues 
#define swap_svalues swap_svalues_unlocked
#endif

#ifndef free_svalue
static INLINE void free_svalue(struct svalue *s)
{
  INT64 tmp;
  struct svalue zero;
  zero.type=PIKE_T_INT;
  zero.subtype = NUMBER_NUMBER;
  tmp=pike_atomic_swap64((INT64 *)s, *(INT64 *)&zero);
  free_svalue_unlocked((struct svalue *)&tmp);
}
#endif

#ifndef free_short_svalue
static INLINE void free_short_svalue(union anything *s, int t)
{
  if(t <= MAX_REF_TYPE)
  {
    INT32 tmp;
    tmp=pike_atomic_swap32((INT32 *)s, 0);
    free_short_svalue_unlocked((union anything *)&tmp, t);
  }
}
#endif

#ifndef add_ref_svalue
static INLINE void add_ref_svalue(struct svalue *s)
{
  INT64 sv;
  sv=pike_atomic_get64((INT64 *)s);
  add_ref_svalue_unlocked((struct svalue *)&sv);
}
#endif

#ifndef assign_svalue_no_free
void assign_svalue_no_free(struct svalue *to, const struct svalue *from)
{
  INT64 tmp, sv;
  sv=pike_atomic_get64((INT64 *)from);
#ifdef PIKE_DEBUG
  if(sv != *(INT64*)from)
  {
    fprintf(stderr,"pike_atomic_get64() is broken %llx != %llx (%08x%08x)!\n",
	    sv,
	    *(INT64*)from,
	    ((INT32*)from)[1], ((INT32*)from)[0]);
    abort();
  }
#endif
  add_ref_svalue_unlocked((struct svalue *)&sv);
  pike_atomic_set64((INT64 *)to, sv);
#ifdef PIKE_DEBUG
  if(*(INT64*)to != *(INT64*)from)
  {
    fprintf(stderr,"pike_atomic_set64() is broken!\n");
    abort();
  }
#endif
}
#endif

#ifndef assign_svalue
static INLINE void assign_svalue(struct svalue *to, const struct svalue *from)
{
  INT64 tmp, sv;
  if(to != from)
  {
    sv=pike_atomic_get64((INT64 *)from);
    add_ref_svalue_unlocked((struct svalue *)&sv);
    tmp=pike_atomic_swap64((INT64 *)to, sv);
    free_svalue_unlocked((struct svalue *)&tmp);
  }
}
#endif

#else /* FOO_PIKE_RUN_UNLOCKED */
#define swap_svalues swap_svalues
#define free_svalue free_svalue_unlocked
#define free_short_svalue free_short_svalue_unlocked 
#define add_ref_svalue add_ref_svalue_unlocked
#define assign_svalue_no_free assign_svalue_no_free_unlocked
#define assign_svalue assign_svalue_unlocked
#endif /* FOO_PIKE_RUN_UNLOCKED */

#ifdef PIKE_RUN_UNLOCKED
#include "pike_threadlib.h"
#endif

/* 
 * Note to self:
 * It might be better to use a static array of mutexes instead
 * and just lock mutex ptr % array_size instead.
 * That way I wouldn't need a mutex in each memory object,
 * but it would cost a couple of cycles in every lock/unlock
 * operation instead.
 */
#define PIKE_MEMORY_OBJECT_MEMBERS \
  INT32 refs \
  DO_IF_SECURITY(; struct object *prot) \
  IF_LOCAL_MUTEX(; PIKE_MUTEX_T mutex)

#ifdef PIKE_SECURITY
#ifdef USE_LOCAL_MUTEX
#define PIKE_CONSTANT_MEMOBJ_INIT(refs) refs, 0, PTHREAD_MUTEX_INITIALIZER
#else
#define PIKE_CONSTANT_MEMOBJ_INIT(refs) refs, 0
#endif
#else
#ifdef USE_LOCAL_MUTEX
#define PIKE_CONSTANT_MEMOBJ_INIT(refs) refs, PTHREAD_MUTEX_INITIALIZER
#else
#define PIKE_CONSTANT_MEMOBJ_INIT(refs) refs
#endif
#endif

#define INIT_PIKE_MEMOBJ(X) do {			\
  struct ref_dummy *v_=(struct ref_dummy *)(X);		\
  v_->refs=0;						\
  add_ref(v_); /* For DMALLOC... */			\
  DO_IF_SECURITY( INITIALIZE_PROT(v_) );		\
  IF_LOCAL_MUTEX(mt_init_recursive(&(v_->mutex)));	\
}while(0)

#define EXIT_PIKE_MEMOBJ(X) do {		\
  struct ref_dummy *v_=(struct ref_dummy *)(X);		\
  DO_IF_SECURITY( FREE_PROT(v_) );		\
  IF_LOCAL_MUTEX(mt_destroy(&(v_->mutex)));	\
}while(0)


struct ref_dummy
{
  PIKE_MEMORY_OBJECT_MEMBERS;
};

/* The following macro is useful to initialize static svalues. Note
 * that the value isn't always set. */
#ifdef HAVE_UNION_INIT
#define SVALUE_INIT(TYPE, SUBTYPE, VAL) {TYPE, SUBTYPE, {VAL}}
#define SVALUE_INIT_INT(VAL) {T_INT, NUMBER_NUMBER, {VAL}}
#define SVALUE_INIT_FREE {PIKE_T_FREE, NUMBER_NUMBER, {0}}
#else
#define SVALUE_INIT(TYPE, SUBTYPE, VAL) {TYPE, SUBTYPE}
#define SVALUE_INIT_INT(VAL) {T_INT, NUMBER_NUMBER}
#define SVALUE_INIT_FREE {PIKE_T_FREE, NUMBER_NUMBER}
#endif

#endif /* !SVALUE_H */
