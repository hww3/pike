/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/

/*
 * $Id: pike_error.h,v 1.7 2001/01/04 02:16:15 hubbe Exp $
 */
#ifndef ERROR_H
#define ERROR_H

#include "machine.h"

#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#undef HAVE_SETJMP_H
#endif

#include <stdarg.h>

#include "svalue.h"


typedef void (*error_call)(void *);

#ifndef STRUCT_FRAME_DECLARED
#define STRUCT_FRAME_DECLARED
struct pike_frame;
#endif

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
  jmp_buf recovery;
  struct pike_frame *frame_pointer;
  ptrdiff_t stack_pointer;
  ptrdiff_t mark_sp;
  INT32 severity;
  ONERROR *onerror;
#ifdef PIKE_DEBUG
  int line;
  char *file;
#endif
} JMP_BUF;

PMOD_EXPORT extern struct svalue throw_value;
extern int throw_severity;

#ifdef PIKE_DEBUG
#define UNSETJMP(X) do{ \
   check_recovery_context(); \
   OED_FPRINTF((stderr, "unsetjmp(%p) %s:%d\n", \
                &(X),  __FILE__, __LINE__)); \
  if(Pike_interpreter.recoveries != &X) { \
    if(Pike_interpreter.recoveries) \
      fatal("UNSETJMP out of sync! (last SETJMP at %s:%d)!\n",Pike_interpreter.recoveries->file,Pike_interpreter.recoveries->line); \
    else \
      fatal("UNSETJMP out of sync! (Pike_interpreter.recoveries = 0)\n"); \
    } \
    Pike_interpreter.recoveries=X.previous; \
   check_recovery_context(); \
  }while (0)
#define DEBUG_LINE_ARGS ,int line, char *file
#define SETJMP(X) setjmp((init_recovery(&X,__LINE__,__FILE__)->recovery))
#else
#define DEBUG_LINE_ARGS 
#define SETJMP(X) setjmp((init_recovery(&X)->recovery))
#define UNSETJMP(X) Pike_interpreter.recoveries=X.previous
#endif


#ifdef PIKE_DEBUG
#define SET_ONERROR(X,Y,Z) \
  do{ \
     check_recovery_context(); \
     OED_FPRINTF((stderr, "SET_ONERROR(%p, %p, %p) %s:%d\n", \
                  &(X), (Y), (void *)(Z), __FILE__, __LINE__)); \
     X.func=(error_call)(Y); \
     DO_IF_DMALLOC( if( X.func == free ) X.func=dmalloc_free;) \
     X.arg=(void *)(Z); \
     if(!Pike_interpreter.recoveries) break; \
     X.previous=Pike_interpreter.recoveries->onerror; \
     X.file = __FILE__; \
     X.line = __LINE__; \
     Pike_interpreter.recoveries->onerror=&X; \
  }while(0)

#define UNSET_ONERROR(X) do {\
    check_recovery_context(); \
    OED_FPRINTF((stderr, "UNSET_ONERROR(%p) %s:%d\n", \
                 &(X), __FILE__, __LINE__)); \
    if(!Pike_interpreter.recoveries) break; \
    if(Pike_interpreter.recoveries->onerror != &(X)) { \
      fprintf(stderr,"LAST SETJMP: %s:%d\n",Pike_interpreter.recoveries->file,Pike_interpreter.recoveries->line); \
      if (Pike_interpreter.recoveries->onerror) { \
        fatal("UNSET_ONERROR out of sync (%p != %p).\n" \
              "Last SET_ONERROR is from %s:%d\n",\
              Pike_interpreter.recoveries->onerror, &(X), \
              Pike_interpreter.recoveries->onerror->file, Pike_interpreter.recoveries->onerror->line ); \
      } else { \
        fatal("UNSET_ONERROR out of sync. No Pike_interpreter.recoveries left.\n"); \
      } \
    } \
    Pike_interpreter.recoveries->onerror=(X).previous; \
  } while(0)

#define ASSERT_ONERROR(X) \
  do{ \
    if (!Pike_interpreter.recoveries) break; \
    if (Pike_interpreter.recoveries->onerror != &X) { \
      fatal("%s:%d ASSERT_ONERROR(%p) failed\n", \
            __FILE__, __LINE__, &(X)); \
    } \
  }while(0)
#else /* !PIKE_DEBUG */
#define SET_ONERROR(X,Y,Z) \
  do{ \
     X.func=(error_call)(Y); \
     X.arg=(void *)(Z); \
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
#define PIKE_ERROR(NAME, TEXT, SP, ARGS)	new_error(NAME, TEXT, SP, ARGS, __FILE__, __LINE__);
#else
#define PIKE_ERROR(NAME, TEXT, SP, ARGS)	new_error(NAME, TEXT, SP, ARGS, NULL, 0);
#endif /* PIKE_DEBUG */

/* Prototypes begin here */
PMOD_EXPORT void check_recovery_context(void);
PMOD_EXPORT void pike_gdb_breakpoint(void);
PMOD_EXPORT JMP_BUF *init_recovery(JMP_BUF *r DEBUG_LINE_ARGS);
PMOD_EXPORT DECLSPEC(noreturn) void pike_throw(void) ATTRIBUTE((noreturn));
PMOD_EXPORT void push_error(char *description);
PMOD_EXPORT DECLSPEC(noreturn) void low_error(const char *buf) ATTRIBUTE((noreturn));
void va_error(const char *fmt, va_list args) ATTRIBUTE((noreturn));
PMOD_EXPORT DECLSPEC(noreturn) void new_error(const char *name, const char *text, struct svalue *oldsp,
	       INT32 args, const char *file, int line) ATTRIBUTE((noreturn));
PMOD_EXPORT void exit_on_error(void *msg);
PMOD_EXPORT void fatal_on_error(void *msg);
PMOD_EXPORT DECLSPEC(noreturn) void Pike_error(const char *fmt,...) ATTRIBUTE((noreturn,format (printf, 1, 2)));
PMOD_EXPORT DECLSPEC(noreturn) void debug_fatal(const char *fmt, ...) ATTRIBUTE((noreturn,format (printf, 1, 2)));
void f_error_cast(INT32 args);
void f_error_index(INT32 args);
void f_error_describe(INT32 args);
void f_error_backtrace(INT32 args);
void DECLSPEC(noreturn) generic_error_va(struct object *o,
		      char *func,
		      struct svalue *base_sp,  int args,
		      char *fmt,
		      va_list foo)
  ATTRIBUTE((noreturn));
PMOD_EXPORT void DECLSPEC(noreturn) generic_error(
  char *func,
  struct svalue *base_sp,  int args,
  char *desc, ...) ATTRIBUTE((noreturn,format (printf, 4, 5)));
PMOD_EXPORT DECLSPEC(noreturn) void index_error(
  char *func,
  struct svalue *base_sp,  int args,
  struct svalue *val,
  struct svalue *ind,
  char *desc, ...) ATTRIBUTE((noreturn,format (printf, 6, 7)));
PMOD_EXPORT DECLSPEC(noreturn) void bad_arg_error(
  char *func,
  struct svalue *base_sp,  int args,
  int which_arg,
  char *expected_type,
  struct svalue *got,
  char *desc, ...)  ATTRIBUTE((noreturn,format (printf, 7, 8)));
PMOD_EXPORT void DECLSPEC(noreturn) math_error(
  char *func,
  struct svalue *base_sp,  int args,
  struct svalue *number,
  char *desc, ...) ATTRIBUTE((noreturn,format (printf, 5, 6)));
PMOD_EXPORT void DECLSPEC(noreturn) resource_error(
  char *func,
  struct svalue *base_sp,  int args,
  char *resource_type,
  size_t howmuch,
  char *desc, ...) ATTRIBUTE((noreturn,format (printf, 6, 7)));
PMOD_EXPORT void DECLSPEC(noreturn) permission_error(
  char *func,
  struct svalue *base_sp, int args,
  char *permission_type,
  char *desc, ...) ATTRIBUTE((noreturn, format(printf, 5, 6)));
PMOD_EXPORT void wrong_number_of_args_error(char *name, int args, int expected);
void init_error(void);
void cleanup_error(void);
/* Prototypes end here */

#if 1
#define fatal \
 fprintf(stderr,"%s:%d: Fatal error:\n",__FILE__,__LINE__),debug_fatal
#else
/* This is useful when debugging assembler code sometimes... -Hubbe */
#define fatal \
 fprintf(stderr,"%s: Fatal error:\n",__FILE__ ":" DEFINETOSTR(__LINE__) ),debug_fatal
#endif

/* Some useful error macros. */


#define SIMPLE_BAD_ARG_ERROR(FUNC, ARG, EXPECT) \
   bad_arg_error(FUNC, Pike_sp-args, args, ARG, EXPECT, Pike_sp+ARG-1-args,\
                 "Bad argument %d to %s(). Expected %s\n", \
                  ARG, FUNC, EXPECT)

#define SIMPLE_TOO_FEW_ARGS_ERROR(FUNC, ARG) \
   bad_arg_error(FUNC, Pike_sp-args, args, ARG, "void", 0,\
                 "Too few arguments to %s().\n",FUNC)

#define SIMPLE_OUT_OF_MEMORY_ERROR(FUNC, AMOUNT) \
   resource_error(FUNC, Pike_sp-args, args, "memory", AMOUNT, "Out of memory.\n")

#define SIMPLE_DIVISION_BY_ZERO_ERROR(FUNC) \
     math_error(FUNC, Pike_sp-args, args, 0, "Division by zero.\n")

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
            if(!__is_exception) \
                UNSETJMP(exception); \
            if(__exception_rethrow) \
                rethrow; \
        } \
        while(0)

/* Generic error stuff */
#define ERR_EXT_DECLARE
#include "errors.h"

#endif
