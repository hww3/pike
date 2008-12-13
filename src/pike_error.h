/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: pike_error.h,v 1.51 2008/12/13 23:56:45 mast Exp $
*/

#ifndef PIKE_ERROR_H
#define PIKE_ERROR_H

#ifdef CONFIGURE_TEST

#include <stdio.h>

static inline void Pike_fatal (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  abort();
}

#define Pike_error Pike_fatal

#else  /* !CONFIGURE_TEST */

#include "global.h"

#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#undef HAVE_SETJMP_H
#endif

#if defined(HAVE_SIGSETJMP) && defined(HAVE_SIGLONGJMP)
#define HAVE_AND_USE_SIGSETJMP
#define LOW_JMP_BUF		sigjmp_buf
#define LOW_SETJMP(X)		sigsetjmp(X, 0)
#define LOW_LONGJMP(X, Y)	siglongjmp(X, Y)
#elif defined(HAVE__SETJMP) && defined(HAVE__LONGJMP)
#define HAVE_AND_USE__SETJMP
#define LOW_JMP_BUF		jmp_buf
#define LOW_SETJMP(X)		_setjmp(X)
#define LOW_LONGJMP(X, Y)	_longjmp(X, Y)
#else
/* Assume we have setjmp and longjmp, they are after all defined by ANSI C. */
#define HAVE_AND_USE_SETJMP
#define LOW_JMP_BUF		jmp_buf
#define LOW_SETJMP(X)		setjmp(X)
#define LOW_LONGJMP(X, Y)	longjmp(X, Y)
#endif

#if 1
PMOD_EXPORT extern const char msg_fatal_error[];
#define Pike_fatal \
 (fprintf(stderr,msg_fatal_error,__FILE__,__LINE__),debug_fatal)
#else
/* This is useful when debugging assembler code sometimes... -Hubbe */
#define Pike_fatal \
 (fprintf(stderr,"%s: Fatal error:\n",__FILE__ ":" DEFINETOSTR(__LINE__) ),debug_fatal)
#endif

#ifndef NO_PIKE_SHORTHAND
#define fatal Pike_fatal
#endif

PMOD_EXPORT DECLSPEC(noreturn) void debug_va_fatal(const char *fmt, va_list args) ATTRIBUTE((noreturn));
PMOD_EXPORT DECLSPEC(noreturn) void debug_fatal(const char *fmt, ...) ATTRIBUTE((noreturn));


#include "svalue.h"


typedef void (*error_call)(void *);

#ifndef STRUCT_FRAME_DECLARED
#define STRUCT_FRAME_DECLARED
struct pike_frame;
#endif

#define THROW_N_A 0
#define THROW_ERROR 10
#define THROW_THREAD_EXIT 20
#define THROW_THREAD_KILLED 30
#define THROW_EXIT 40
#define THROW_MAX_SEVERITY 100

/* #define ONERROR_DEBUG */

#ifdef ONERROR_DEBUG
#define OED_FPRINTF(X)	fprintf X
#else /* !ONERROR_DEBUG */
#define OED_FPRINTF(X)
#endif /* ONERROR_DEBUG */

typedef struct ONERROR
{
  struct ONERROR *previous;
  struct pike_frame *frame_pointer;
  error_call func;
  void *arg;
#ifdef PIKE_DEBUG
  const char *file;
  int line;
#endif /* PIKE_DEBUG */
} ONERROR;

typedef struct JMP_BUF
{
  struct JMP_BUF *previous;
  LOW_JMP_BUF recovery;
  struct pike_frame *frame_pointer;
  ptrdiff_t stack_pointer;
  ptrdiff_t mark_sp;
  INT32 severity;
  ONERROR *onerror;
#ifdef PIKE_DEBUG
  int line;
  char *file;
  int on_stack;
#endif
} JMP_BUF;

PMOD_EXPORT extern struct svalue throw_value;
extern int throw_severity;

#ifdef PIKE_DEBUG
PMOD_EXPORT extern const char msg_unsetjmp_nosync_1[];
PMOD_EXPORT extern const char msg_unsetjmp_nosync_2[];
#define UNSETJMP(X) do{						\
    check_recovery_context();					\
    OED_FPRINTF((stderr, "unsetjmp(%p) %s:%d\n",		\
		 &(X),  __FILE__, __LINE__));			\
    if(Pike_interpreter.recoveries != &X) {			\
      if(Pike_interpreter.recoveries)				\
	Pike_fatal(msg_unsetjmp_nosync_1,			\
		   Pike_interpreter.recoveries->file);		\
      else							\
	Pike_fatal("%s", msg_unsetjmp_nosync_2);		\
    }								\
    Pike_interpreter.recoveries=X.previous;			\
    check_recovery_context();					\
  }while (0)

#ifdef DMALLOC_LOCATION
#define PERR_LOCATION() DMALLOC_LOCATION()
#else
#define PERR_LOCATION() ( __FILE__ ":" DEFINETOSTR(__LINE__) )
#endif

#define DEBUG_INIT_REC_ARGS , int on_stack, char *location
#define SETJMP(X) LOW_SETJMP((init_recovery(&X, 0, 1, PERR_LOCATION())->recovery))
#define SETJMP_SP(jmp, stack_pop_levels)				\
  LOW_SETJMP((init_recovery(&jmp, stack_pop_levels, 1, PERR_LOCATION())->recovery))
#else  /* !PIKE_DEBUG */
#define DEBUG_INIT_REC_ARGS
#define SETJMP(X) LOW_SETJMP((init_recovery(&X, 0)->recovery))
#define SETJMP_SP(jmp, stack_pop_levels)				\
  LOW_SETJMP((init_recovery(&jmp, stack_pop_levels)->recovery))
#define UNSETJMP(X) Pike_interpreter.recoveries=X.previous
#endif	/* !PIKE_DEBUG */


#ifdef PIKE_DEBUG
#define SET_ONERROR(X,Y,Z)					\
  do{								\
    check_recovery_context();					\
    OED_FPRINTF((stderr, "SET_ONERROR(%p, %p, %p) %s:%d\n",	\
		 &(X), (Y), (void *)(Z), __FILE__, __LINE__));	\
    X.frame_pointer = Pike_interpreter.frame_pointer;		\
    X.func=(error_call)(Y);					\
    DO_IF_DMALLOC( if( X.func == free ) X.func=dmalloc_free);	\
    X.arg=(void *)(Z);						\
    if(!Pike_interpreter.recoveries) break;			\
    X.previous=Pike_interpreter.recoveries->onerror;		\
    X.file = __FILE__;						\
    X.line = __LINE__;						\
    Pike_interpreter.recoveries->onerror=&X;			\
  }while(0)

PMOD_EXPORT extern const char msg_last_setjmp[];
PMOD_EXPORT extern const char msg_unset_onerr_nosync_1[];
PMOD_EXPORT extern const char msg_unset_onerr_nosync_2[];
#define UNSET_ONERROR(X) do {					\
    check_recovery_context();					\
    OED_FPRINTF((stderr, "UNSET_ONERROR(%p) %s:%d\n",		\
                 &(X), __FILE__, __LINE__));			\
    if(!Pike_interpreter.recoveries) break;			\
    if(Pike_interpreter.recoveries->onerror != &(X)) {		\
      fprintf(stderr,msg_last_setjmp,				\
	      Pike_interpreter.recoveries->file);		\
      if (Pike_interpreter.recoveries->onerror) {		\
	Pike_fatal(msg_unset_onerr_nosync_1,			\
	           Pike_interpreter.recoveries->onerror, &(X),	\
                   Pike_interpreter.recoveries->onerror->file); \
      } else {							\
	Pike_fatal("%s", msg_unset_onerr_nosync_2);		\
      }								\
    }								\
    Pike_interpreter.recoveries->onerror=(X).previous;		\
  } while(0)

PMOD_EXPORT extern const char msg_assert_onerr[];
#define ASSERT_ONERROR(X) \
  do{ \
    if (!Pike_interpreter.recoveries) break; \
    if (Pike_interpreter.recoveries->onerror != &X) { \
      Pike_fatal(msg_assert_onerr, \
                 __FILE__, __LINE__, &(X)); \
    } \
  }while(0)
#else /* !PIKE_DEBUG */
#define SET_ONERROR(X,Y,Z) \
  do{ \
     X.func=(error_call)(Y); \
     X.arg=(void *)(Z); \
     X.frame_pointer = Pike_interpreter.frame_pointer; \
     if(!Pike_interpreter.recoveries) break; \
     X.previous=Pike_interpreter.recoveries->onerror; \
     Pike_interpreter.recoveries->onerror=&X; \
  }while(0)

#define UNSET_ONERROR(X) Pike_interpreter.recoveries && (Pike_interpreter.recoveries->onerror=X.previous)

#define ASSERT_ONERROR(X)
#endif /* PIKE_DEBUG */

#define CALL_AND_UNSET_ONERROR(X) do {		\
     X.func(X.arg);				\
     UNSET_ONERROR(X);				\
  }while(0)

#if defined(PIKE_DEBUG) && 0
/* Works, but probably not interresting for most people
 *	/grubba 1998-04-11
 */
#define PIKE_ERROR(NAME, TEXT, SP, ARGS) new_error(NAME, TEXT, SP, ARGS, __FILE__, __LINE__)
#else
#define PIKE_ERROR(NAME, TEXT, SP, ARGS) new_error(NAME, TEXT, SP, ARGS, NULL, 0)
#endif /* PIKE_DEBUG */

/* Prototypes begin here */
PMOD_EXPORT void check_recovery_context(void);
PMOD_EXPORT void pike_gdb_breakpoint(INT32 args);
PMOD_EXPORT JMP_BUF *init_recovery(JMP_BUF *r, size_t stack_pop_levels DEBUG_INIT_REC_ARGS);
PMOD_EXPORT DECLSPEC(noreturn) void pike_throw(void) ATTRIBUTE((noreturn));
PMOD_EXPORT void push_error(const char *description);
PMOD_EXPORT DECLSPEC(noreturn) void low_error(const char *buf) ATTRIBUTE((noreturn));
PMOD_EXPORT void Pike_vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
PMOD_EXPORT void va_make_error (const char *fmt, va_list args);
PMOD_EXPORT void DECLSPEC(noreturn) va_error(const char *fmt, va_list args) ATTRIBUTE((noreturn));
PMOD_EXPORT void make_error (const char *fmt, ...);
PMOD_EXPORT DECLSPEC(noreturn) void Pike_error(const char *fmt,...) ATTRIBUTE((noreturn));
PMOD_EXPORT DECLSPEC(noreturn) void new_error(const char *name, const char *text, struct svalue *oldsp,
	       INT32 args, const char *file, int line) ATTRIBUTE((noreturn));
PMOD_EXPORT void exit_on_error(const void *msg);
PMOD_EXPORT void fatal_on_error(const void *msg);
PMOD_EXPORT DECLSPEC(noreturn) void debug_fatal(const char *fmt, ...) ATTRIBUTE((noreturn));
PMOD_EXPORT DECLSPEC(noreturn) void generic_error_va(
  struct object *o, const char *func, const struct svalue *base_sp, int args,
  const char *fmt, va_list *fmt_args)
  ATTRIBUTE((noreturn));
PMOD_EXPORT DECLSPEC(noreturn) void throw_error_object(
  struct object *o,
  const char *func,
  struct svalue *base_sp,  int args,
  const char *desc, ...) ATTRIBUTE((noreturn));
PMOD_EXPORT void DECLSPEC(noreturn) generic_error(
  const char *func,
  struct svalue *base_sp,  int args,
  const char *desc, ...) ATTRIBUTE((noreturn));
PMOD_EXPORT DECLSPEC(noreturn) void index_error(
  const char *func,
  struct svalue *base_sp,  int args,
  struct svalue *val,
  struct svalue *ind,
  const char *desc, ...) ATTRIBUTE((noreturn));
PMOD_EXPORT DECLSPEC(noreturn) void bad_arg_error(
  const char *func,
  struct svalue *base_sp,  int args,
  int which_arg,
  const char *expected_type,
  struct svalue *got,
  const char *desc, ...)  ATTRIBUTE((noreturn));
PMOD_EXPORT void DECLSPEC(noreturn) math_error(
  const char *func,
  struct svalue *base_sp,  int args,
  struct svalue *number,
  const char *desc, ...) ATTRIBUTE((noreturn));
PMOD_EXPORT void DECLSPEC(noreturn) resource_error(
  const char *func,
  struct svalue *base_sp,  int args,
  const char *resource_type,
  size_t howmuch,
  const char *desc, ...) ATTRIBUTE((noreturn));
PMOD_EXPORT void DECLSPEC(noreturn) permission_error(
  const char *func,
  struct svalue *base_sp, int args,
  const char *permission_type,
  const char *desc, ...) ATTRIBUTE((noreturn));
PMOD_EXPORT void wrong_number_of_args_error(const char *name, int args, int expected)
  ATTRIBUTE((noreturn));
void init_error(void);
void cleanup_error(void);
/* Prototypes end here */

/* Some useful error macros. */


/* This one should be used when the type of the argument is wrong. */
PMOD_EXPORT extern const char msg_bad_arg[];
#define SIMPLE_ARG_TYPE_ERROR(FUNC, ARG, EXPECT) \
   bad_arg_error(FUNC, Pike_sp-args, args, ARG, EXPECT, Pike_sp+ARG-1-args,\
		 msg_bad_arg, ARG, FUNC, EXPECT)
/* Less descriptive macro name kept for compatibility. */
#define SIMPLE_BAD_ARG_ERROR(FUNC, ARG, EXPECT) \
   bad_arg_error(FUNC, Pike_sp-args, args, ARG, EXPECT, Pike_sp+ARG-1-args,\
		 msg_bad_arg, ARG, FUNC, EXPECT)

/* This one should be used when there's some problem with the argument
 * other than a bogus type. PROBLEM is a full sentence without a
 * trailing newline. */
PMOD_EXPORT extern const char msg_bad_arg_2[];
#define SIMPLE_ARG_ERROR(FUNC, ARG, PROBLEM) \
  bad_arg_error (FUNC, Pike_sp-args, args, ARG, NULL, Pike_sp+ARG-1-args, \
		 msg_bad_arg_2, ARG, FUNC, PROBLEM)

#define SIMPLE_WRONG_NUM_ARGS_ERROR(FUNC, ARG) \
  wrong_number_of_args_error (FUNC, args, ARG)
/* The following is for compatibility. */
#define SIMPLE_TOO_FEW_ARGS_ERROR(FUNC, ARG) \
  wrong_number_of_args_error (FUNC, args, ARG)

PMOD_EXPORT extern const char msg_out_of_mem[];
PMOD_EXPORT extern const char msg_out_of_mem_2[];

#if 1
static INLINE void DECLSPEC(noreturn) out_of_memory_error (
  const char *func,
  struct svalue *base_sp,  int args,
  size_t amount) ATTRIBUTE((noreturn));
static INLINE void DECLSPEC(noreturn) out_of_memory_error (
  const char *func,
  struct svalue *base_sp,  int args,
  size_t amount)
{
  resource_error (func, base_sp, args, "memory", amount,
		  amount ? msg_out_of_mem_2 : msg_out_of_mem, amount);
}
#else
#define out_of_memory_error(FUNC, BASE_SP, ARGS, AMOUNT)	\
  do {								\
    size_t amount_ = (AMOUNT);					\
    int args_ = (ARGS);						\
    resource_error((FUNC), (BASE_SP), args_, "memory", amount_,	\
		   msg_out_of_mem_2, amount_);			\
  } while(0)
#endif

#define SIMPLE_OUT_OF_MEMORY_ERROR(FUNC, AMOUNT) \
   out_of_memory_error(FUNC, Pike_sp-args, args, AMOUNT)

PMOD_EXPORT extern const char msg_div_by_zero[];
#define SIMPLE_DIVISION_BY_ZERO_ERROR(FUNC) \
     math_error(FUNC, Pike_sp-args, args, 0, msg_div_by_zero)

#ifndef PIKE_DEBUG
#define check_recovery_context() ((void)0)
#endif

/* Experimental convenience exception macros. */

#define exception_try \
        do \
        { \
            int __exception_rethrow, __is_exception; \
            JMP_BUF exception; \
            __is_exception = SETJMP(exception); \
            __exception_rethrow = 0; \
            if(__is_exception) /* rethrow needs this */ \
                UNSETJMP(exception); \
            if(!__is_exception)
    
#define exception_catch_if \
            else if

#define exception_catch(e) \
            exception_catch_if(exception->severity = (e))

#define exception_catch_all \
            exception_catch_if(1)

#define exception_semicatch_all \
            exception_catch_if((__exception_rethrow = 1))

#define rethrow \
            pike_throw()

#define exception_endtry \
            else \
                __exception_rethrow = 1; \
            if(__is_exception) { \
		free_svalue(&throw_value); \
		mark_free_svalue (&throw_value); \
	    } \
	    else \
                UNSETJMP(exception); \
            if(__exception_rethrow) \
                rethrow; \
        } \
        while(0)

/* Generic error stuff */
#define ERR_EXT_DECLARE
#include "errors.h"

#endif	/* !CONFIGURE_TEST */

#endif /* PIKE_ERROR_H */
