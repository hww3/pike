/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/

/*
 * $Id: pike_types.h,v 1.56 2001/03/02 21:46:29 grubba Exp $
 */
#ifndef PIKE_TYPES_H
#define PIKE_TYPES_H

#include "svalue.h"

#ifdef USE_PIKE_TYPE
/*
 * The new type type.
 */
struct pike_type
{
  INT32 refs;
  unsigned INT32 hash;
  struct pike_type *next;
  unsigned INT32 type;
  struct pike_type *car;
  struct pike_type *cdr;
};
void free_type(struct pike_type *t);
#define copy_type(D, S)	add_ref(D = (S))
#else /* !USE_PIKE_TYPE */
/*
 * The old type type.
 */
/* Note that pike_type in this case is defined in global.h
 * to avoid circularities with svalue.h and this file.
 */
#define free_type(T)	free_string(T)
#define copy_type(D, S)	copy_shared_string(D, S)
#endif /* USE_PIKE_TYPE */

/* Also used in struct node_identifier */
union node_data
{
  struct
  {
    int number;
    struct program *prog;
  } id;
  struct svalue sval;
  struct
  {
    struct node_s *a, *b;
  } node;
  struct
  {
    struct node_identifier *a, *b;
  } node_id;
  struct
  {
    int a, b;
  } integer;
};

struct node_s
{
#if defined(SHARED_NODES)
  unsigned INT32 refs;
  size_t hash;
  struct node_s *next;
#endif /* SHARED_NODES */
#ifdef PIKE_DEBUG
  struct pike_string *current_file;
#endif
  struct pike_type *type;
  struct pike_string *name;
  struct node_s *parent;
  unsigned INT16 line_number;
  unsigned INT16 node_info;
  unsigned INT16 tree_info;
  /* The stuff from this point on is hashed. */
  unsigned INT16 token;
  union node_data u;
};

#ifdef SHARED_NODES_MK2

struct node_identifier
{
  ptrdiff_t refs;
  struct node_identifier *next;
  size_t hash;
  INT16 token;
  union node_data u;
};

#endif /* SHARED_NODES_MK2 */

#ifndef STRUCT_NODE_S_DECLARED
#define STRUCT_NODE_S_DECLARED
#endif


typedef struct node_s node;

#define PIKE_TYPE_STACK_SIZE 100000

extern unsigned char type_stack[PIKE_TYPE_STACK_SIZE];
extern unsigned char *pike_type_mark_stack[PIKE_TYPE_STACK_SIZE/4];

extern int max_correct_args;
PMOD_EXPORT extern struct pike_type *string_type_string;
PMOD_EXPORT extern struct pike_type *int_type_string;
PMOD_EXPORT extern struct pike_type *float_type_string;
PMOD_EXPORT extern struct pike_type *object_type_string;
PMOD_EXPORT extern struct pike_type *function_type_string;
PMOD_EXPORT extern struct pike_type *program_type_string;
PMOD_EXPORT extern struct pike_type *array_type_string;
PMOD_EXPORT extern struct pike_type *multiset_type_string;
PMOD_EXPORT extern struct pike_type *mapping_type_string;
PMOD_EXPORT extern struct pike_type *type_type_string;
PMOD_EXPORT extern struct pike_type *mixed_type_string;
PMOD_EXPORT extern struct pike_type *void_type_string;
PMOD_EXPORT extern struct pike_type *zero_type_string;
PMOD_EXPORT extern struct pike_type *any_type_string;
PMOD_EXPORT extern struct pike_type *weak_type_string;

#ifdef USE_PIKE_TYPE
#define CONSTTYPE(X) make_pike_type(X)
#ifdef DEBUG_MALLOC
struct pike_type_location
{
  struct pike_type *t;
  struct pike_type_location *next;
};

extern struct pike_type_location *all_pike_type_locations;

#define MAKE_CONSTANT_TYPE(T, X) do {		\
    static struct pike_type_location type_;	\
    if (!type_.t} {				\
      type_.t = CONSTTYPE(X);			\
      type_.next = all_pike_type_locations;	\
      all_pike_type_locations = &type_;		\
    }						\
    copy_type((T), type_.t);			\
  } while(0)
#else /* !DEBUG_MALLOC */
#define MAKE_CONSTANT_TYPE(T, X) do {	\
    static struct pike_type *type_;	\
    if (!type_) {			\
      type_ = CONSTTYPE(X);		\
    }					\
    copy_type((T), type_);		\
  } while(0)
#endif /* DEBUG_MALLOC */
#else /* !USE_PIKE_TYPE */
#define CONSTTYPE(X) make_shared_binary_string(X,CONSTANT_STRLEN(X))
#define MAKE_CONSTANT_TYPE(T, X)	MAKE_CONSTANT_SHARED_STRING(T, X)
#endif /* USE_PIKE_TYPE */

#ifdef PIKE_DEBUG
#define init_type_stack() type_stack_mark()
#define exit_type_stack() do {\
    ptrdiff_t q_q_q_q = pop_stack_mark(); \
    if(q_q_q_q) fatal("Type stack out of wack! %ld\n", \
                      PTRDIFF_T_TO_LONG(q_q_q_q)); \
  } while(0)
#else
#define init_type_stack type_stack_mark
#define exit_type_stack pop_stack_mark
#endif

/* Hmm, these will cause fatals if they fail... */
#define push_type(X) do {				\
  if(Pike_compiler->type_stackp >= type_stack + sizeof(type_stack))	\
    yyerror("Type stack overflow.");			\
  else {						\
    *Pike_compiler->type_stackp=(X);					\
    Pike_compiler->type_stackp++;					\
  }							\
} while(0)

#define unsafe_push_type(X) do {			\
  *Pike_compiler->type_stackp=(X);					\
  Pike_compiler->type_stackp++;					\
} while(0)

#define type_stack_mark() do {				\
  if(Pike_compiler->pike_type_mark_stackp >= pike_type_mark_stack + NELEM(pike_type_mark_stack))	\
    fatal("Type mark stack overflow.");		\
  else {						\
    *Pike_compiler->pike_type_mark_stackp=Pike_compiler->type_stackp;				\
    Pike_compiler->pike_type_mark_stackp++;					\
  }							\
} while(0)

#define unsafe_type_stack_mark() do {				\
  *Pike_compiler->pike_type_mark_stackp=Pike_compiler->type_stackp;				\
  Pike_compiler->pike_type_mark_stackp++;					\
} while(0)

#define reset_type_stack() do {			\
   type_stack_pop_to_mark();			\
  type_stack_mark();				\
} while(0)

/* Prototypes begin here */
void check_type_string(struct pike_type *s);
void init_types(void);
ptrdiff_t pop_stack_mark(void);
void pop_type_stack(void);
void type_stack_pop_to_mark(void);
void type_stack_reverse(void);
void push_int_type(INT32 min, INT32 max);
void push_object_type(int flag, INT32 id);
void push_object_type_backwards(int flag, INT32 id);
INT32 extract_type_int(char *p);
void push_unfinished_type(char *s);
void push_finished_type(struct pike_type *type);
void push_finished_type_backwards(struct pike_type *type);
struct pike_type *debug_pop_unfinished_type(void);
struct pike_type *debug_pop_type(void);
struct pike_type *debug_compiler_pop_type(void);
struct pike_type *parse_type(char *s);
void stupid_describe_type(char *a, ptrdiff_t len);
void simple_describe_type(struct pike_type *s);
void my_describe_type(struct pike_type *type);
struct pike_string *describe_type(struct pike_type *type);
TYPE_T compile_type_to_runtime_type(struct pike_type *s);
struct pike_type *or_pike_types(struct pike_type *a,
				struct pike_type *b,
				int zero_implied);
struct pike_type *and_pike_types(struct pike_type *a,
				 struct pike_type *b);
int strict_check_call(struct pike_type *fun_type, struct pike_type *arg_type);
int check_soft_cast(struct pike_type *to, struct pike_type *from);
int match_types(struct pike_type *a,struct pike_type *b);
int pike_types_le(struct pike_type *a, struct pike_type *b);
struct pike_type *index_type(struct pike_type *type,
			     struct pike_type *index_type,
			     node *n);
struct pike_type *array_value_type(struct pike_type *array_type);
struct pike_type *key_type(struct pike_type *type, node *n);
int check_indexing(struct pike_type *type,
		   struct pike_type *index_type,
		   node *n);
int count_arguments(struct pike_type *s);
int minimum_arguments(struct pike_type *s);
struct pike_string *check_call(struct pike_type *args,
			       struct pike_type *type,
			       int strict);
INT32 get_max_args(struct pike_type *type);
struct pike_type *zzap_function_return(struct pike_type *t, INT32 id);
struct pike_type *get_type_of_svalue(struct svalue *s);
struct pike_type *object_type_to_program_type(struct pike_type *obj_t);
char *get_name_of_type(int t);
void cleanup_pike_types(void);
int type_may_overload(struct pike_type *type, int lfun);
void yyexplain_nonmatching_types(struct pike_type *type_a,
				 struct pike_type *type_b,
				 int flags);
struct pike_type *make_pike_type(char *t);
int pike_type_allow_premature_toss(struct pike_type *type);
/* Prototypes end here */

/* "Dynamic types" - use with ADD_FUNCTION_DTYPE */
#define dtStore(TYPE) {int e; for (e=0; e<CONSTANT_STRLEN(TYPE); e++) unsafe_push_type((TYPE)[e]);}
#define dtArr(VAL) {unsafe_push_type(PIKE_T_ARRAY); {VAL}}
#define dtArray dtArr(dtMix)
#define dtMap(IND,VAL) {unsafe_push_type(PIKE_T_MAPPING); {VAL} {IND}}
#define dtMapping dtMap(dtMix,dtMix)
#define dtSet(IND) {unsafe_push_type(PIKE_T_MULTISET); {IND}}
#define dtMultiset dtSet(dtMix)
#define dtObjImpl(PROGRAM) {push_object_type_backwards(0, (PROGRAM)->id);}
#define dtObjIs(PROGRAM) {push_object_type_backwards(1, (PROGRAM)->id);}
#define dtObj dtStore(tObj)
#define dtFuncV(ARGS,REST,RET) MagicdtFuncV(RET,REST,ARGS)
#define dtFunc(ARGS,RET) MagicdtFunc(RET,ARGS)
#define MagicdtFuncV(RET,REST,ARGS) {unsafe_push_type(PIKE_T_FUNCTION); {ARGS} unsafe_push_type(T_MANY); {REST} {RET}}
#define MagicdtFunc(RET,ARGS) dtFuncV(ARGS {}, dtVoid, RET)
#define dtFunction dtFuncV({},dtAny,dtAny)
#define dtNone {}
#define dtPrg {unsafe_push_type(PIKE_T_PROGRAM);}
#define dtProgram {unsafe_push_type(PIKE_T_PROGRAM);}
#define dtStr {unsafe_push_type(PIKE_T_STRING);}
#define dtString {unsafe_push_type(PIKE_T_STRING);}
#define dtType {unsafe_push_type(PIKE_T_TYPE);}
#define dtFlt {unsafe_push_type(PIKE_T_FLOAT);}
#define dtFloat {unsafe_push_type(PIKE_T_FLOAT);}
#define dtIntRange(LOW,HIGH) {unsafe_push_type(PIKE_T_INT); push_type_int_backwards(LOW); push_type_int_backwards(HIGH);}
#define dtInt dtStore(tInt)
#define dtZero {unsafe_push_type(PIKE_T_ZERO);}
#define dtVoid {unsafe_push_type(T_VOID);}
#define dtVar(X) {unsafe_push_type(X);}
#define dtSetvar(X,TYPE) {unsafe_push_type(T_ASSIGN); {TYPE}}
#define dtNot(TYPE) {unsafe_push_type(T_NOT); {TYPE}}
#define dtAnd(A,B) {unsafe_push_type(T_AND); {A} {B}}
#define dtOr(A,B) {unsafe_push_type(T_OR); {A} {B}}
#define dtOr3(A,B,C) dtOr(A,dtOr(B,C))
#define dtOr4(A,B,C,D) dtOr(A,dtOr3(B,C,D))
#define dtOr5(A,B,C,D,E) dtOr(A,dtOr4(B,C,D,E))
#define dtOr6(A,B,C,D,E,F) dtOr(A,dtOr5(B,C,D,E,F))
#define dtMix {unsafe_push_type(PIKE_T_MIXED);}
#define dtMixed {unsafe_push_type(PIKE_T_MIXED);}
#define dtComplex dtStore(tComplex)
#define dtStringIndicable dtStore(tStringIndicable)
#define dtRef dtStore(tRef)
#define dtIfnot(A,B) dtAnd(dtNot(A),B)
#define dtAny dtStore(tAny)
#define DTYPE_START do {						\
  unsafe_type_stack_mark();						\
  unsafe_type_stack_mark();						\
} while (0)
#define DTYPE_END(TYPESTR) do {						\
  if(Pike_compiler->type_stackp >= type_stack + sizeof(type_stack))			\
    fatal("Type stack overflow.");					\
  type_stack_reverse();							\
  (TYPESTR)=pop_unfinished_type();					\
} while (0)

#ifdef DEBUG_MALLOC
#define pop_type() ((struct pike_type *)debug_malloc_pass(debug_pop_type()))
#define compiler_pop_type() ((struct pike_type *)debug_malloc_pass(debug_compiler_pop_type()))
#define pop_unfinished_type() \
 ((struct pike_type *)debug_malloc_pass(debug_pop_unfinished_type()))
#else
#define pop_type debug_pop_type
#define compiler_pop_type debug_compiler_pop_type
#define pop_unfinished_type debug_pop_unfinished_type
#endif

#ifndef PIKE_DEBUG
#define check_type_string(X)
#endif

#endif
