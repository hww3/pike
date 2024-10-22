/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

#ifndef PIKE_TYPES_H
#define PIKE_TYPES_H

#include "svalue.h"

#define PIKE_TYPE_STACK_SIZE 100000

#ifdef PIKE_DEBUG
void TYPE_STACK_DEBUG(const char *fun);
#else /* !PIKE_DEBUG */
#define TYPE_STACK_DEBUG(X)
#endif /* PIKE_DEBUG */

/*
 * The new type type.
 */
struct pike_type
{
  INT32 refs;
  unsigned INT32 hash;
  struct pike_type *next;
  unsigned INT32 flags;
  unsigned INT16 type;
  struct pike_type *car;
  struct pike_type *cdr;
};

extern struct pike_type **pike_type_hash;
extern size_t pike_type_hash_size;

#define CAR_TO_INT(TYPE) ((char *) (TYPE)->car - (char *) 0)
#define CDR_TO_INT(TYPE) ((char *) (TYPE)->cdr - (char *) 0)

#include "block_alloc_h.h"
BLOCK_ALLOC(pike_type, n/a);

/*
 * pike_type flags:
 */
#define PT_FLAG_MARKER_0	0x000001	/* The subtree holds a '0'. */
#define PT_FLAG_MARKER_1	0x000002	/* The subtree holds a '1'. */
#define PT_FLAG_MARKER_2	0x000004	/* The subtree holds a '2'. */
#define PT_FLAG_MARKER_3	0x000008	/* The subtree holds a '3'. */
#define PT_FLAG_MARKER_4	0x000010	/* The subtree holds a '4'. */
#define PT_FLAG_MARKER_5	0x000020	/* The subtree holds a '5'. */
#define PT_FLAG_MARKER_6	0x000040	/* The subtree holds a '6'. */
#define PT_FLAG_MARKER_7	0x000080	/* The subtree holds a '7'. */
#define PT_FLAG_MARKER_8	0x000100	/* The subtree holds a '8'. */
#define PT_FLAG_MARKER_9	0x000200	/* The subtree holds a '9'. */
#define PT_FLAG_MARKER		0x0003ff	/* The subtree holds markers. */
#define PT_ASSIGN_SHIFT		12		/* Number of bits to shift. */
#define PT_FLAG_ASSIGN_0	0x001000	/* The subtree assigns '0'. */
#define PT_FLAG_ASSIGN_1	0x002000	/* The subtree assigns '1'. */
#define PT_FLAG_ASSIGN_2	0x004000	/* The subtree assigns '2'. */
#define PT_FLAG_ASSIGN_3	0x008000	/* The subtree assigns '3'. */
#define PT_FLAG_ASSIGN_4	0x010000	/* The subtree assigns '4'. */
#define PT_FLAG_ASSIGN_5	0x020000	/* The subtree assigns '5'. */
#define PT_FLAG_ASSIGN_6	0x040000	/* The subtree assigns '6'. */
#define PT_FLAG_ASSIGN_7	0x080000	/* The subtree assigns '7'. */
#define PT_FLAG_ASSIGN_8	0x100000	/* The subtree assigns '8'. */
#define PT_FLAG_ASSIGN_9	0x200000	/* The subtree assigns '9'. */
#define PT_FLAG_ASSIGN		0x3ff000	/* The subtree holds assigns. */

#define PT_FLAG_MARK_ASSIGN	0x3ff3ff	/* Assigns AND Markers. */

/*
 * new_check_call() and check_splice_call() flags
 */
#define CALL_STRICT		0x0001	/* Strict checking. */
#define CALL_LAST_ARG		0x0002	/* This is the last argument. */
#define CALL_7_6		0x0004	/* Pike 7.6 compatibility mode. */
#define CALL_WEAK_VOID		0x0008	/* Allow promotion of void to zero. */
#define CALL_ARG_LVALUE		0x0010	/* Argument is lvalue (sscanf). */
#define CALL_INHIBIT_WARNINGS	0x0020	/* Inhibit warnings. */

/*
 * soft_cast() flags
 */
#define SOFT_WEAKER	0x0001	/* Soft cast to a weaker type. */

void debug_free_type(struct pike_type *t);
#ifdef DEBUG_MALLOC
#define copy_pike_type(D, S) add_ref((struct pike_type *)debug_malloc_pass(D = (S)))
#else /* !DEBUG_MALLOC */
#define copy_pike_type(D, S)	add_ref(D = (S))
#endif /* DEBUG_MALLOC */
#define CONSTTYPE(X) make_pike_type(X)

extern struct pike_type *type_stack[PIKE_TYPE_STACK_SIZE];
extern struct pike_type **pike_type_mark_stack[PIKE_TYPE_STACK_SIZE/4];

#ifdef DEBUG_MALLOC
#define check_type_string(T) debug_check_type_string((struct pike_type *)debug_malloc_pass_named(T, "check_type_string"))
#elif defined (PIKE_DEBUG)
#define check_type_string debug_check_type_string
#endif /* PIKE_DEBUG */

#define debug_free_type_preamble(T) do {				\
    debug_malloc_touch_named (T, "free_type");				\
    DO_IF_DEBUG (							\
      DO_IF_PIKE_CLEANUP (						\
	if (gc_external_refs_zapped)					\
	  gc_check_zapped (T, PIKE_T_TYPE, __FILE__, __LINE__)));	\
  } while (0)

#define free_type(T) do {						\
    struct pike_type *t_ = (T);						\
    debug_free_type_preamble (t_);					\
    debug_free_type (t_);						\
  } while (0)

#define free_pike_type free_type

extern int max_correct_args;
PMOD_EXPORT extern struct pike_type *string0_type_string;
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
extern struct pike_type *sscanf_type_string;
extern struct pike_type *sscanf_76_type_string;

#define CONSTTYPE(X) make_pike_type(X)

#ifdef DO_PIKE_CLEANUP
struct pike_type_location
{
  struct pike_type *t;
  struct pike_type_location *next;
};

extern struct pike_type_location *all_pike_type_locations;

#define MAKE_CONSTANT_TYPE(T, X) do {		\
    static struct pike_type_location type_;	\
    if (!type_.t) {				\
      type_.t = CONSTTYPE(X);			\
      type_.next = all_pike_type_locations;	\
      all_pike_type_locations = &type_;		\
    }						\
    copy_pike_type((T), type_.t);		\
  } while(0)
#else /* !DO_PIKE_CLEANUP */
#define MAKE_CONSTANT_TYPE(T, X) do {	\
    static struct pike_type *type_;	\
    if (!type_) {			\
      type_ = CONSTTYPE(X);		\
    }					\
    copy_pike_type((T), type_);		\
  } while(0)
#endif /* DO_PIKE_CLEANUP */

#ifdef PIKE_DEBUG
#define init_type_stack() type_stack_mark()
#define exit_type_stack() do {\
    ptrdiff_t q_q_q_q = pop_stack_mark(); \
    if(q_q_q_q) Pike_fatal("Type stack out of wack! %ld\n", \
                      PTRDIFF_T_TO_LONG(q_q_q_q)); \
  } while(0)
#else
#define init_type_stack type_stack_mark
#define exit_type_stack pop_stack_mark
#endif

void debug_push_type(unsigned int type);
void debug_push_reverse_type(unsigned int type);
#ifdef DEBUG_MALLOC
#define push_type(T) do { debug_push_type(T); debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_reverse_type(T) do { debug_push_reverse_type(T); debug_malloc_pass(debug_peek_type_stack()); } while(0)
#else /* !DEBUG_MALLOC */
#define push_type debug_push_type
#define push_reverse_type debug_push_reverse_type
#endif /* DEBUG_MALLOC */

#define type_stack_mark() do {				\
  if(Pike_compiler->pike_type_mark_stackp >= pike_type_mark_stack + NELEM(pike_type_mark_stack))	\
    Pike_fatal("Type mark stack overflow.\n");		\
  else {						\
    *Pike_compiler->pike_type_mark_stackp=Pike_compiler->type_stackp;				\
    Pike_compiler->pike_type_mark_stackp++;					\
  }							\
  TYPE_STACK_DEBUG("type_stack_mark");			\
} while(0)

#define reset_type_stack() do {			\
   type_stack_pop_to_mark();			\
  type_stack_mark();				\
} while(0)

/* Prototypes begin here */
void debug_check_type_string(struct pike_type *s);
void init_types(void);
ptrdiff_t pop_stack_mark(void);
void debug_pop_type_stack(unsigned int expected);
void type_stack_pop_to_mark(void);
void type_stack_reverse(void);
struct pike_type *debug_peek_type_stack(void);
void debug_push_int_type(INT_TYPE min, INT_TYPE max);
void debug_push_object_type(int flag, INT32 id);
void debug_push_object_type_backwards(int flag, INT32 id);
void debug_push_type_attribute(struct pike_string *attr);
void debug_push_type_name(struct pike_string *name);
INT32 extract_type_int(char *p);
void debug_push_unfinished_type(char *s);
void debug_push_assign_type(int marker);
void debug_push_finished_type(struct pike_type *type);
void debug_push_finished_type_backwards(struct pike_type *type);
void debug_push_scope_type(int level);
struct pike_type *debug_pop_unfinished_type(void);
void compiler_discard_type (void);
struct pike_type *debug_pop_type(void);
struct pike_type *debug_compiler_pop_type(void);
struct pike_type *parse_type(const char *s);
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
			     struct pike_type *type_of_index,
			     node *n);
struct pike_type *range_type(struct pike_type *type,
			     struct pike_type *index1_type,
			     struct pike_type *index2_type);
struct pike_type *array_value_type(struct pike_type *array_type);
struct pike_type *key_type(struct pike_type *type, node *n);
int check_indexing(struct pike_type *type,
		   struct pike_type *type_of_index,
		   node *n);
int count_arguments(struct pike_type *s);
int minimum_arguments(struct pike_type *s);
struct pike_type *check_call(struct pike_type *args,
			     struct pike_type *type,
			     int strict);
struct pike_type *get_argument_type(struct pike_type *fun, int arg_no);
struct pike_type *soft_cast(struct pike_type *soft_type,
			    struct pike_type *orig_type,
			    int flags);
struct pike_type *low_new_check_call(struct pike_type *fun_type,
				     struct pike_type *arg_type,
				     INT32 flags,
				     struct svalue *sval);
struct pike_type *new_get_return_type(struct pike_type *fun_type,
				      INT32 flags);
struct pike_type *get_first_arg_type(struct pike_type *fun_type,
				     INT32 flags);
struct pike_type *check_splice_call(struct pike_string *fun_name,
				    struct pike_type *fun_type,
				    INT32 argno,
				    struct pike_type *arg_type,
				    struct svalue *sval,
				    INT32 flags);
struct pike_type *new_check_call(struct pike_string *fun_name,
				 struct pike_type *fun_type,
				 node *args, INT32 *argno, INT32 flags);
struct pike_type *zzap_function_return(struct pike_type *t,
				       struct pike_type *fun_ret);
struct pike_type *get_type_of_svalue(const struct svalue *s);
struct pike_type *object_type_to_program_type(struct pike_type *obj_t);
PMOD_EXPORT char *get_name_of_type(TYPE_T t);
void cleanup_pike_types(void);
void cleanup_pike_type_table(void);
PMOD_EXPORT void visit_type (struct pike_type *t, int action);
void gc_mark_type_as_referenced(struct pike_type *t);
void gc_check_type (struct pike_type *t);
void gc_check_all_types (void);
int type_may_overload(struct pike_type *type, int lfun);
void yyexplain_nonmatching_types(int severity_level,
				 struct pike_string *a_file,
				 INT32 a_line,
				 struct pike_type *type_a,
				 struct pike_string *b_file,
				 INT32 b_line,
				 struct pike_type *type_b);
struct pike_type *debug_make_pike_type(const char *t);
struct pike_string *type_to_string(struct pike_type *t);
int pike_type_allow_premature_toss(struct pike_type *type);
void register_attribute_handler(struct pike_string *attr,
				struct svalue *handler);
/* Prototypes end here */

#define visit_type_ref(T, REF_TYPE)				\
  visit_ref (pass_type (T), (REF_TYPE),				\
	     (visit_thing_fn *) &visit_type, NULL)

#if 0 /* FIXME: Not supported under USE_PIKE_TYPE yet. */
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
  if(Pike_compiler->type_stackp >= type_stack + sizeof(type_stack))	\
    Pike_fatal("Type stack overflow.\n");				\
  type_stack_reverse();							\
  (TYPESTR)=pop_unfinished_type();					\
} while (0)
#endif /* 0 */

#ifdef DEBUG_MALLOC
#define pop_type() ((struct pike_type *)debug_malloc_pass(debug_pop_type()))
#define compiler_pop_type() ((struct pike_type *)debug_malloc_pass(debug_compiler_pop_type()))
#define pop_unfinished_type() \
 ((struct pike_type *)debug_malloc_pass(debug_pop_unfinished_type()))
#define make_pike_type(X) \
 ((struct pike_type *)debug_malloc_pass(debug_make_pike_type(X)))
#define peek_type_stack() ((struct pike_type *)debug_malloc_pass(debug_peek_type_stack()))
#define pop_type_stack(E) do { debug_malloc_pass(debug_peek_type_stack()); debug_pop_type_stack(E); } while(0)
#define push_int_type(MIN,MAX) do { debug_push_int_type(MIN,MAX);debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_object_type(FLAG,ID) do { debug_push_object_type(FLAG,ID);debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_object_type_backwards(FLAG,ID) do { debug_push_object_type_backwards(FLAG,ID);debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_scope_type(LEVEL) do { debug_push_scope_type(LEVEL);debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_type_attribute(ATTR) do { debug_push_type_attribute((struct pike_string *)debug_malloc_pass(ATTR));debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_type_name(NAME) do { debug_push_type_name((struct pike_string *)debug_malloc_pass(NAME));debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_unfinished_type(S) ERROR
#define push_assign_type(MARKER) do { debug_push_assign_type(MARKER);debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_finished_type(T) do { debug_push_finished_type((struct pike_type *)debug_malloc_pass(T));debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_finished_type_with_markers(T,M,MS) do { debug_push_finished_type_with_markers((struct pike_type *)debug_malloc_pass(T),M,MS);debug_malloc_pass(debug_peek_type_stack()); } while(0)
#define push_finished_type_backwards(T) ERROR
#else
#define make_pike_type debug_make_pike_type
#define pop_type debug_pop_type
#define compiler_pop_type debug_compiler_pop_type
#define pop_unfinished_type debug_pop_unfinished_type
#define peek_type_stack debug_peek_type_stack
#define pop_type_stack debug_pop_type_stack
#define push_int_type debug_push_int_type
#define push_object_type debug_push_object_type
#define push_object_type_backwards debug_push_object_type_backwards
#define push_scope_type debug_push_scope_type
#define push_type_attribute debug_push_type_attribute
#define push_type_name debug_push_type_name
#define push_unfinished_type debug_push_unfinished_type
#define push_assign_type debug_push_assign_type
#define push_finished_type debug_push_finished_type
#define push_finished_type_with_markers debug_push_finished_type_with_markers
#define push_finished_type_backwards debug_push_finished_type_backwards
#endif

#ifndef PIKE_DEBUG
#define check_type_string(X)
#endif

#endif
