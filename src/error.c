/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

#define NO_PIKE_SHORTHAND
#include "global.h"
#include "svalue.h"
#include "pike_macros.h"
#include "pike_error.h"
#include "interpret.h"
#include "stralloc.h"
#include "builtin_functions.h"
#include "array.h"
#include "mapping.h"
#include "object.h"
#include "main.h"
#include "builtin_functions.h"
#include "backend.h"
#include "operators.h"
#include "module_support.h"
#include "threads.h"
#include "gc.h"

/* __attribute__ only applies to function declarations, not
   definitions, so we disable them here. */
#undef ATTRIBUTE
#define ATTRIBUTE(X)

/* #define ERROR_DEBUG */

PMOD_EXPORT const char msg_fatal_error[] =
  "%s:%d: Fatal error:\n";
#ifdef PIKE_DEBUG
PMOD_EXPORT const char msg_unsetjmp_nosync_1[] =
  "UNSETJMP out of sync! (last SETJMP at %s)!\n";
PMOD_EXPORT const char msg_unsetjmp_nosync_2[] =
  "UNSETJMP out of sync! (Pike_interpreter.recoveries = 0)\n";
PMOD_EXPORT const char msg_last_setjmp[] =
  "LAST SETJMP: %s\n";
PMOD_EXPORT const char msg_unset_onerr_nosync_1[] =
  "UNSET_ONERROR out of sync (%p != %p).\n"
  "Last SET_ONERROR is from %s\n";
PMOD_EXPORT const char msg_unset_onerr_nosync_2[] =
  "UNSET_ONERROR out of sync. No Pike_interpreter.recoveries left.\n";
PMOD_EXPORT const char msg_assert_onerr[] =
  "%s ASSERT_ONERROR(%p) failed\n";
#endif
PMOD_EXPORT const char msg_bad_arg[] =
  "Bad argument %d to %s(). Expected %s.\n";
PMOD_EXPORT const char msg_bad_arg_2[] =
  "Bad argument %d to %s(). %s\n";
PMOD_EXPORT const char msg_out_of_mem[] =
  "Out of memory.\n";
PMOD_EXPORT const char msg_out_of_mem_2[] =
  "Out of memory - failed to allocate %"PRINTSIZET"d bytes.\n";
PMOD_EXPORT const char msg_div_by_zero[] =
  "Division by zero.\n";

/*
 * Recoveries handling.
 */

#ifdef PIKE_DEBUG
/* struct mapping *recovery_lookup = NULL; */

PMOD_EXPORT void check_recovery_context(void)
{
  char foo;
#define TESTILITEST ((((char *)Pike_interpreter.recoveries)-((char *)&foo))*STACK_DIRECTION)
  if(Pike_interpreter.recoveries &&
     Pike_interpreter.recoveries->on_stack &&
     TESTILITEST > 0) {
    fprintf(stderr, "Recoveries is out biking (Pike_interpreter.recoveries=%p, Pike_sp=%p, %ld)!\n",
	    Pike_interpreter.recoveries, &foo,
	    DO_NOT_WARN((long)TESTILITEST));
    fprintf(stderr, "Last recovery was added at %s:%d\n",
	    Pike_interpreter.recoveries->file,
	    Pike_interpreter.recoveries->line);
    push_int((ptrdiff_t)Pike_interpreter.recoveries);
/*     if ((s = low_mapping_lookup(recovery_lookup, Pike_sp-1))) { */
/*       fprintf(stderr, "Try also looking at %s.\n", (char*)s->u.integer); */
/*     } */
    pop_stack();
    Pike_fatal("Recoveries is out biking (Pike_interpreter.recoveries=%p, C sp=%p, %ld)!\n",
	  Pike_interpreter.recoveries, &foo,
	  DO_NOT_WARN((long)TESTILITEST));
  }

  /* Add more stuff here when required */
}
#endif

PMOD_EXPORT void pike_gdb_breakpoint(INT32 args)
{
  pop_n_elems(args);
}

PMOD_EXPORT JMP_BUF *init_recovery(JMP_BUF *r, size_t stack_pop_levels DEBUG_INIT_REC_ARGS)
{
  check_recovery_context();
#ifdef PIKE_DEBUG
  r->file=location;
  OED_FPRINTF((stderr, "init_recovery(%p) %s\n", r, location));
  r->on_stack = on_stack;
#endif
  r->frame_pointer=Pike_fp;
  r->stack_pointer=Pike_sp - stack_pop_levels - Pike_interpreter.evaluator_stack;
  r->mark_sp=Pike_mark_sp - Pike_interpreter.mark_stack;
  r->previous=Pike_interpreter.recoveries;
  r->onerror=0;
  r->severity=THROW_ERROR;
  Pike_interpreter.recoveries=r;
  check_recovery_context();
#ifdef PIKE_DEBUG
/*   if (recovery_lookup && Pike_sp) { */
/*     push_int((ptrdiff_t)r); */
/*     push_int((ptrdiff_t)location); */
/*     mapping_insert(recovery_lookup, Pike_sp-2, Pike_sp-1); */
/*     pop_n_elems(2); */
/*   } */
#endif
  return r;
}

PMOD_EXPORT DECLSPEC(noreturn) void pike_throw(void) ATTRIBUTE((noreturn))
{
#ifdef TRACE_UNFINISHED_TYPE_FIELDS
  accept_unfinished_type_fields++;
#endif

  while(Pike_interpreter.recoveries && throw_severity > Pike_interpreter.recoveries->severity)
  {
    while(Pike_interpreter.recoveries->onerror)
    {
      ONERROR *err = Pike_interpreter.recoveries->onerror;
      while(Pike_fp != err->frame_pointer)
      {
#ifdef PIKE_DEBUG
	if(!Pike_fp)
	  Pike_fatal("Popped out of stack frames.\n");
#endif
	POP_PIKE_FRAME();
      }

      Pike_interpreter.recoveries->onerror = err->previous;
      (*err->func)(err->arg);
    }

    {
      JMP_BUF *prev_rec = Pike_interpreter.recoveries->previous;
      if (Pike_interpreter.catch_ctx &&
	  (&Pike_interpreter.catch_ctx->recovery ==
	   Pike_interpreter.recoveries)) {
	struct catch_context *cc = Pike_interpreter.catch_ctx;
	Pike_interpreter.catch_ctx = cc->prev;
	really_free_catch_context (cc);
      }
      Pike_interpreter.recoveries = prev_rec;
    }
  }

  if(!Pike_interpreter.recoveries)
    Pike_fatal("No error recovery context.\n");

  /* Don't pop the stack before the onerrors have run; they might need
   * items from the stack.
   */

  while(Pike_interpreter.recoveries->onerror)
  {
    ONERROR *err = Pike_interpreter.recoveries->onerror;

    while(Pike_fp != err->frame_pointer)
    {
#ifdef PIKE_DEBUG
      if(!Pike_fp)
	Pike_fatal("Popped out of stack frames.\n");
#endif
      POP_PIKE_FRAME();
    }
    Pike_interpreter.recoveries->onerror = err->previous;
    (*err->func)(err->arg);
  }

  while(Pike_fp != Pike_interpreter.recoveries->frame_pointer)
  {
#ifdef PIKE_DEBUG
    if(!Pike_fp)
      Pike_fatal("Popped out of stack frames.\n");
#endif
    POP_PIKE_FRAME();
  }

#ifdef PIKE_DEBUG
  if(Pike_sp - Pike_interpreter.evaluator_stack < Pike_interpreter.recoveries->stack_pointer)
    Pike_fatal("Stack error in error.\n");
#endif

  pop_n_elems(Pike_sp - Pike_interpreter.evaluator_stack - Pike_interpreter.recoveries->stack_pointer);
  Pike_mark_sp = Pike_interpreter.mark_stack + Pike_interpreter.recoveries->mark_sp;

#if defined(DEBUG_MALLOC) && defined(PIKE_DEBUG)
  /* This will tell us where the value was caught (I hope) */
  if(throw_value.type <= MAX_REF_TYPE)
  {
    debug_malloc_update_location(throw_value.u.refs,
				 Pike_interpreter.recoveries->file);
    debug_malloc_touch(throw_value.u.refs);
  }
#endif

#ifdef TRACE_UNFINISHED_TYPE_FIELDS
  accept_unfinished_type_fields--;
#endif

  if (Pike_interpreter.catch_ctx &&
      &Pike_interpreter.catch_ctx->recovery == Pike_interpreter.recoveries) {
    /* This is a phony recovery made in F_CATCH that hasn't been passed
     * to LOW_SETJMP. The real jmpbuf is in catching_eval_instruction. */
#ifdef PIKE_DEBUG
    if (!Pike_interpreter.catching_eval_jmpbuf)
      Pike_fatal ("Got phony F_CATCH recovery but no catching_eval_jmpbuf.\n");
#endif
    LOW_LONGJMP (*Pike_interpreter.catching_eval_jmpbuf, 1);
  }
  else
    LOW_LONGJMP(Pike_interpreter.recoveries->recovery,1);
}

PMOD_EXPORT void push_error(const char *description)
{
  push_text(description);
  f_backtrace(0);
  f_aggregate(2);
}

PMOD_EXPORT struct svalue throw_value = SVALUE_INIT_FREE;
int throw_severity;
static const char *in_error;

PMOD_EXPORT DECLSPEC(noreturn) void low_error(const char *buf) ATTRIBUTE((noreturn))
{
  push_error(buf);
  free_svalue(& throw_value);
  move_svalue (&throw_value, --Pike_sp);
  throw_severity = THROW_ERROR;
  in_error=0;
  pike_throw();  /* Hope someone is catching, or we will be out of balls. */
}

PMOD_EXPORT void Pike_vsnprintf(char *str, size_t size,
				const char *fmt, va_list args)
{
  size--;

  do {
    if(fmt[0]=='%') {

      fmt++;

      switch( fmt++[0] ) {

      case 'O':
	{
	  dynamic_buffer buf;
	  dynbuf_string s;
	  char *ostr;
	  struct svalue t = va_arg(args, struct svalue);
	  init_buf(&buf);
	  describe_svalue(&t,0,0);
	  s=complex_free_buf(&buf);
	  ostr=s.str;

	  while( --size>0 && (s.len--)>0 )
	    str++[0]=s.str++[0];

	  free(ostr);
	}
	break;

      case 'S':
	{
	  dynamic_buffer buf;
	  dynbuf_string s;
	  char *ostr;
	  struct svalue t;

	  t.type = PIKE_T_STRING;
	  t.u.string = va_arg(args, struct pike_string *);

	  init_buf(&buf);
	  describe_svalue(&t,0,0);
	  s=complex_free_buf(&buf);
	  ostr=s.str;

	  while( --size>0 && (s.len--)>0 )
	    str++[0]=s.str++[0];

	  free(ostr);
	}
	break;

      case 's':
	{
	  char *from = va_arg(args, char *);
	  while( --size>0 && from[0]!=0 )
	    str++[0]=from++[0];
	}
	break;

      case 'c':
	{
	  int c = (char)va_arg(args, int);
	  sprintf(str, "%c", c);
	  str++;
	  size--;
	}
	break;

      case 'd':
	{
	  char buf[12];
	  int pos=0;
	  sprintf(buf, "%d", va_arg(args, int));
	  while( --size>0 && buf[pos]!=0 )
	    str++[0]=buf[pos++];
	}
	break;

      case 'x':
	{
	  char buf[12];
	  int pos=0;
	  sprintf(buf, "%x", va_arg(args, int));
	  while( --size>0 && buf[pos]!=0 )
	    str++[0]=buf[pos++];
	}
	break;

      case '%':
	str++[0]='%';
	size--;
	break;

      default:
	Pike_fatal("Unknown Pike_vsnprintf formatting char '%c'.\n",
		   (fmt-1)[0]);
      }
    }
    else {
      str++[0]=fmt++[0];
      size--;
    }

  } while(fmt[0] && size>0);

  str[0]=0;
  va_end(args);
}


PMOD_EXPORT void va_make_error (const char *fmt, va_list args)
{
  struct string_builder s;
  init_string_builder(&s, 0);
  string_builder_vsprintf(&s, fmt, args);
  push_string(finish_string_builder(&s));
  f_backtrace(0);
  f_aggregate(2);
}

PMOD_EXPORT void DECLSPEC(noreturn) va_error(const char *fmt, va_list args)
  ATTRIBUTE((noreturn))
{
  SWAP_IN_THREAD_IF_REQUIRED();
  if(in_error)
  {
    const char *tmp=in_error;
    in_error = NULL;
    Pike_fatal("Recursive Pike_error() calls, "
	       "original error: %s, new error: %s", tmp, fmt);
  }

  in_error=fmt;

  if(!Pike_interpreter.recoveries)
  {
    /* FIXME: Why not use Pike_fatal() here? */
#ifdef PIKE_DEBUG
    if (d_flag) {
      fprintf(stderr,"No error recovery context!\n%s", fmt);
      dump_backlog();
    }
#endif

    fprintf(stderr,"No error recovery context!\n%s", fmt);
    exit(99);
  }

  va_make_error (fmt, args);
  free_svalue(&throw_value);
  move_svalue(&throw_value, --Pike_sp);
  throw_severity = THROW_ERROR;
  in_error = NULL;
  pike_throw();	/* Hope someone is catching, or we will be out of balls. */
}

PMOD_EXPORT void make_error (const char *fmt, ...)
/* Creates a general error container like Pike_error but doesn't throw
 * it, instead it is left on the stack. */
{
  va_list args;
  va_start (args,fmt);
  va_make_error (fmt,args);
  va_end (args);
}

PMOD_EXPORT DECLSPEC(noreturn) void Pike_error(const char *fmt,...) ATTRIBUTE((noreturn))
{
  va_list args;
  va_start(args,fmt);
  va_error(fmt,args);
  va_end(args);
}

PMOD_EXPORT DECLSPEC(noreturn) void new_error(const char *name,
					      const char *text,
					      struct svalue *oldsp,
					      INT32 args,
					      const char *file,
					      int line) ATTRIBUTE((noreturn))
{
  int i;

  ASSERT_THREAD_SWAPPED_IN();

  if(in_error)
  {
    const char *tmp=in_error;
    in_error=0;
    Pike_fatal("Recursive error() calls, original error: %s",tmp);
  }

  in_error=text;

  if(!Pike_interpreter.recoveries)
  {
#ifdef PIKE_DEBUG
    if (d_flag) {
      fprintf(stderr,"No error recovery context!\n%s():%s",
	      name ? name : "<unknown>", text);
      dump_backlog();
    }
#endif

    fprintf(stderr,"No error recovery context!\n%s():%s",
	    name ? name : "<unknown>", text);
    if(file)
      fprintf(stderr,"at %s:%d\n",file,line);
    exit(99);
  }

  push_text(text);

  f_backtrace(0);

  if (file) {
    push_text(file);
    push_int(line);
  } else {
    push_int(0);
    push_int(0);
  }

  if (name)
    push_text(name);
  else
    push_int(0);

  for (i=-args; i; i++) {
    if (oldsp[i].type <= PIKE_T_FLOAT) {
      push_svalue(oldsp + i);
    } else {
      char buffer[50];
      sprintf(buffer, "<Svalue:0x%04x:0x%04x:%p>",
	      oldsp[i].type, oldsp[i].subtype, oldsp[i].u.ptr);
      push_text(buffer);
    }
  }

  f_aggregate(args + 3);
  f_aggregate(1);

  f_add(2);

  f_aggregate(2);

  free_svalue(&throw_value);
  move_svalue (&throw_value, --Pike_sp);
  throw_severity=THROW_ERROR;

  in_error=0;
  pike_throw();  /* Hope someone is catching, or we will be out of balls. */
}

static int inhibit_errors = 0;

PMOD_EXPORT void exit_on_error(const void *msg)
{
  ONERROR tmp;
  SET_ONERROR(tmp,fatal_on_error,"Fatal in exit_on_error!");
  d_flag=0;
  Pike_interpreter.trace_level = 0;

  if (inhibit_errors)
    fprintf (stderr, "Got recursive error in exit_on_error: %s\n", (char *) msg);

  else {
    dynamic_buffer save_buf;
    char *s;
    struct svalue thrown;

    inhibit_errors = 1;

#ifdef PIKE_DEBUG
    if (d_flag) {
      fprintf(stderr,"%s\n",(char *)msg);
      dump_backlog();
    }
#endif

    fprintf(stderr,"%s\n",(char *)msg);

    /* We've reserved LOW_SVALUE_STACK_MARGIN and LOW_C_STACK_MARGIN
     * for this. */
    Pike_interpreter.svalue_stack_margin = 0;
    Pike_interpreter.c_stack_margin = 0;
    fprintf(stderr,"Attempting to dump raw error: (may fail)\n");
    init_buf(&save_buf);
    move_svalue (&thrown, &throw_value);
    mark_free_svalue (&throw_value);
    describe_svalue(&thrown,0,0);
    free_svalue (&thrown);
    s=simple_free_buf(&save_buf);
    fprintf(stderr,"%s\n",s);
    free(s);
  }

  exit(1);
}

#ifdef __NT__
/* Wrapper around abort() to avoid interactive requesters on NT. */
int fnordel=0;
static void do_abort()
{
  if (!d_flag && !getenv("PIKE_DEBUG")) {
    exit(-6);	/* -SIGIOT */
  }
  fnordel=999/fnordel;
}
#else /* !__NT__ */
#define do_abort()	abort()
#endif /* __NT__ */

PMOD_EXPORT void fatal_on_error(const void *msg)
{
  /* It's ok if we're exiting. */
  if (throw_severity == THROW_EXIT) return;

#ifdef PIKE_DEBUG
  if (d_flag) {
    fprintf(stderr,"%s\n",(char *)msg);
    dump_backlog();
  }
#endif
  fprintf(stderr,"%s\n",(char *)msg);
  do_abort();
}

PMOD_EXPORT DECLSPEC(noreturn) void debug_va_fatal(const char *fmt, va_list args) ATTRIBUTE((noreturn))
{
  static int in_fatal = 0;

  /* fprintf(stderr, "Raw error: %s\n", fmt); */

  /* Prevent double fatal. */
  if (in_fatal)
  {
    if (fmt) (void)VFPRINTF(stderr, fmt, args);
    do_abort();
  }

  in_fatal = 1;
#ifdef PIKE_DEBUG
  if (d_flag) {
#ifdef HAVE_VA_COPY
    if (fmt) {
      va_list a;
      va_copy (a, args);
      (void)VFPRINTF(stderr, fmt, a);
      va_end (a);
    }
#endif
    dump_backlog();
  }
#endif

  if (fmt) (void)VFPRINTF(stderr, fmt, args);

  if(Pike_in_gc)
    fprintf(stderr,"Pike was in GC stage %d when this fatal occurred.\n",Pike_in_gc);
  Pike_in_gc = GC_PASS_DISABLED;

  d_flag=Pike_interpreter.trace_level=0;

  if(Pike_sp && Pike_interpreter.evaluator_stack &&
     master_object && master_object->prog)
  {
    JMP_BUF jmp;
    struct callback_list saved_eval_cbs = evaluator_callbacks;
    /* Simulate threads_disabled to avoid thread switches or any other
     * evaluator stuff while we let the master describe the backtrace
     * below. Doing it the naughty way without going through
     * init_threads_disable etc to avoid hanging on runaway locks. */
    threads_disabled++;
    MEMSET (&evaluator_callbacks, 0, sizeof (evaluator_callbacks));
    if (SETJMP (jmp))
      fprintf(stderr,"Got exception when trying to describe backtrace.\n");
    else {
      jmp.severity = THROW_EXIT; /* Don't want normal exit code to run here. */
      push_error("Backtrace at time of fatal:\n");
      APPLY_MASTER("describe_backtrace",1);
      if(Pike_sp[-1].type==PIKE_T_STRING)
	write_to_stderr(Pike_sp[-1].u.string->str, Pike_sp[-1].u.string->len);
    }
    UNSETJMP (jmp);
    threads_disabled--;
    evaluator_callbacks = saved_eval_cbs;
  }else{
    fprintf(stderr,"No stack - no backtrace.\n");
  }
  fflush(stderr);
  do_abort();
}

PMOD_EXPORT DECLSPEC(noreturn) void debug_fatal(const char *fmt, ...) ATTRIBUTE((noreturn))
{
  va_list args;
  va_start(args,fmt);
  debug_va_fatal (fmt, args);
  va_end (args);
}

#if 1

/*! @class MasterObject
 */

/*! @decl string describe_backtrace(mixed exception)
 *!
 *!   Called by various routines to format a readable
 *!   description of an exception.
 *!
 *! @param exception
 *!   Something that was thrown. Usually an @[Error.Generic] object, or
 *!   an array with the following content:
 *!   @array
 *!     @elem string msg
 *!       Error message.
 *!     @elem array(backtrace_frame|array(mixed)) backtrace
 *!       Backtrace to the point where the exception occurred.
 *!   @endarray
 *!
 *! @returns
 *!   Returns a string describing the exeception.
 *!
 *! @note
 *!   Usually added by the initialization code the global name space
 *!   with @[add_constant()].
 *!
 *! @seealso
 *!   @[predef::describe_backtrace()]
 */

/*! @endclass
 */

/*! @module Error
 */

/*! @class Generic
 *! Class for exception objects for errors of unspecified type.
 */

/*! @decl string error_message
 *!
 *! The error message. It always ends with a newline (@expr{'\n'@})
 *! character and it might be more than one line.
 *!
 *! Code that catches and rethrows errors may extend this with more
 *! error information.
 */

/*! @decl array(backtrace_frame|array(mixed)) error_backtrace
 *!
 *! The backtrace as returned by @[backtrace] where the error
 *! occurred.
 *!
 *! Code that catches and rethrows errors should ensure that this
 *! remains the same in the rethrown error.
 */

#define GENERIC_ERROR_THIS ((struct generic_error_struct *)CURRENT_STORAGE)

#define ERR_DECLARE
#include "errors.h"

/*! @decl array cast(string type)
 *!
 *! Cast operator.
 *!
 *! @note
 *!   The only supported type to cast to is @expr{"array"@}, which
 *!   generates an old-style error @expr{({@[message](),
 *!   @[backtrace]()})@}.
 */
static void f_error_cast(INT32 args)
{
  char *s;
  get_all_args("error->cast",args,"%s",&s);
  if(!strncmp(s,"array",5))
  {
    pop_n_elems(args);
    apply_current (generic_err_message_fun, 0);
    apply_current (generic_err_backtrace_fun, 0);
    f_aggregate(2);
  }else{
    SIMPLE_BAD_ARG_ERROR("error->cast", 1, "the value \"array\"");
  }
}

/*! @decl array|string `[](int(0..1) index)
 *!
 *! Index operator.
 *!
 *! Simulates an array
 *! @array
 *!   @elem string msg
 *!     Error message as returned by @[message].
 *!   @elem array backtrace
 *!     Backtrace as returned by @[backtrace].
 *! @endarray
 *!
 *! @note
 *!   The error message is always terminated with a newline.
 *!
 *! @seealso
 *!   @[backtrace()]
 */
static void f_error_index(INT32 args)
{
  INT_TYPE ind;
  get_all_args("error->`[]",args,"%i",&ind);

  switch(ind)
  {
    case 0:
      pop_n_elems(args);
      apply_current (generic_err_message_fun, 0);
      break;
    case 1:
      pop_n_elems(args);
      apply_current (generic_err_backtrace_fun, 0);
      break;
    default:
      index_error("error->`[]", Pike_sp-args, args, NULL, Pike_sp-args,
		  "Index %"PRINTPIKEINT"d is out of range 0..1.\n", ind);
      break;
  }
}

/*! @decl string describe()
 *!
 *! Return a readable error report that includes the backtrace.
 */
static void f_error_describe(INT32 args)
{
  pop_n_elems(args);
  ref_push_object(Pike_fp->current_object);
  APPLY_MASTER("describe_backtrace",1);
}

/*! @decl string message()
 *!
 *! Return a readable message describing the error. Normally simply
 *! returns @[error_message].
 *!
 *! If you override this function then you should ensure that
 *! @[error_message] is included in the returned message, since there
 *! might be code that catches your error objects, extends
 *! @[error_message] with more info, and rethrows the error.
 */
static void f_error_message(INT32 args)
{
  pop_n_elems(args);
  if (GENERIC_ERROR_THIS->error_message)
    ref_push_string (GENERIC_ERROR_THIS->error_message);
  else
    push_int (0);
}

/*! @decl array backtrace()
 *!
 *! Return the backtrace where the error occurred. Normally simply
 *! returns @[error_backtrace].
 *!
 *! @seealso
 *!   @[predef::backtrace()]
 */
static void f_error_backtrace(INT32 args)
{
  pop_n_elems(args);
  if(GENERIC_ERROR_THIS->error_backtrace)
    ref_push_array(GENERIC_ERROR_THIS->error_backtrace);
  else
    push_int(0);
}

/*! @decl string _sprintf()
 */
static void f_error__sprintf(INT32 args)
{
  int mode = 0;

  if(args>0 && Pike_sp[-args].type == PIKE_T_INT)
    mode = Pike_sp[-args].u.integer;
  pop_n_elems(args);

  if(mode != 'O') {
    push_undefined();
    return;
  }

  {
    struct program *p = Pike_fp->current_object->prog;
    int i;
    struct pike_string *error_name_str;
    /* error_name is the name the error program wants in printouts
     * like this. To still let inheriting programs fall back to
     * describe_program if they don't provide it, we make it private
     * in the base error classes and allow us to see private
     * identifiers here. */
    MAKE_CONST_STRING (error_name_str, "error_name");
    i = really_low_find_shared_string_identifier (error_name_str, p,
						  SEE_PRIVATE);
    if (i != -1)
      push_svalue (&PROG_FROM_INT (p, i)->
		   constants[ID_FROM_INT (p, i)->func.offset].sval);
    else {
      ref_push_program (p);
      SAFE_APPLY_MASTER ("describe_program", 1);
    }
  }

  push_constant_text("(%O)");
  if(GENERIC_ERROR_THIS->error_message)
    ref_push_string(GENERIC_ERROR_THIS->error_message);
  else
    push_int(0);
  f_sprintf(2);
  f_add(2);
}

/*! @decl int(0..1) _is_type(string t)
 *!
 *! Claims that the error object is an array, for compatibility with
 *! old style error handling code.
 */
static void f_error__is_type(INT32 args)
{
  struct pike_string *array_string;
  int ret;
  MAKE_CONST_STRING(array_string, "array");
  if (args < 0) SIMPLE_TOO_FEW_ARGS_ERROR("_is_type", 1);
  if (args > 1) SIMPLE_WRONG_NUM_ARGS_ERROR("_is_type", 1);
  if (Pike_sp[-args].type != PIKE_T_STRING)
    SIMPLE_ARG_TYPE_ERROR("_is_type", 1, "string");
  ret = Pike_sp[-args].u.string == array_string;
  pop_n_elems(args);
  push_int(ret);
}

/*! @decl void create(string message, @
 *!                   void|array(backtrace_frame|array(mixed)) backtrace)
 */
static void f_error_create(INT32 args)
{
  struct pike_string *msg;
  struct array *bt = NULL;

  get_all_args("create", args, "%W.%A", &msg, &bt);

  do_free_string(GENERIC_ERROR_THIS->error_message);
  copy_shared_string(GENERIC_ERROR_THIS->error_message, msg);

  if (bt) {
    if (GENERIC_ERROR_THIS->error_backtrace)
      free_array (GENERIC_ERROR_THIS->error_backtrace);
    add_ref (GENERIC_ERROR_THIS->error_backtrace = bt);
  }
  else {
    f_backtrace(0);
    push_int (1);
    o_range2 (RANGE_LOW_OPEN|RANGE_HIGH_FROM_END);
    assign_to_short_svalue ((union anything *)&GENERIC_ERROR_THIS->error_backtrace,
			    PIKE_T_ARRAY, Pike_sp-1);
    pop_stack();
  }

  pop_n_elems(args);
}

/*! @endclass
 */

/*! @endmodule
 */

#ifdef ERROR_DEBUG
#define DWERROR(X)	fprintf X
#else /* !ERROR_DEBUG */
#define DWERROR(X)
#endif /* ERROR_DEBUG */

#define INIT_ERROR(FEL)\
  va_list foo; \
  struct object *o; \
  va_start(foo,desc); \
  ASSERT_THREAD_SWAPPED_IN(); \
  DWERROR((stderr, "%s(): Throwing a " #FEL " error\n", func)); \
  o=fast_clone_object(PIKE_CONCAT(FEL,_error_program))

#define ERROR_DONE(FOO) \
  PIKE_CONCAT(FOO,_error_va(o,func, \
			      base_sp, args, \
			      desc, &foo)); \
  va_end(foo)

#define ERROR_STRUCT(STRUCT,O) \
 ((struct PIKE_CONCAT(STRUCT,_error_struct) *)((O)->storage + PIKE_CONCAT(STRUCT,_error_offset)))

#define ERROR_COPY(STRUCT,X) \
  ERROR_STRUCT(STRUCT,o)->X=X

#define ERROR_COPY_SVALUE(STRUCT,X) do { \
    if (X) { \
      assign_svalue_no_free( & ERROR_STRUCT(STRUCT,o)->X, X); \
    } else { \
      ERROR_STRUCT(STRUCT, o)->X.type = PIKE_T_INT; \
      ERROR_STRUCT(STRUCT, o)->X.subtype = NUMBER_UNDEFINED; \
      ERROR_STRUCT(STRUCT, o)->X.u.integer = 0; \
    } \
  } while (0)


#define ERROR_COPY_REF(STRUCT,X) \
  add_ref( ERROR_STRUCT(STRUCT,o)->X=X )


/* This prepares the passed object o, which is assumed to inherit
 * generic_error_program, and throws it:
 *
 * o  A backtrace is assigned to error_backtrace.
 *
 * o  If func is specified, a frame is constructed for at the end of
 *    backtrace using it as function name and base_sp[0..args-1] as
 *    arguments.
 *
 * o  If fmt is specified, an error message is created from it and
 *    fmt_args using string_builder_vsprintf. (fmt_args is passed as a
 *    va_list pointer to be able to pass NULL if fmt is NULL.)
 */
PMOD_EXPORT DECLSPEC(noreturn) void generic_error_va(
  struct object *o, const char *func, const struct svalue *base_sp, int args,
  const char *fmt, va_list *fmt_args)
{
  struct generic_error_struct *err =
    (struct generic_error_struct *) get_storage (o, generic_error_program);

#ifdef PIKE_DEBUG
  if (!err)
    Pike_fatal ("Got an object which doesn't inherit generic_error_program.\n");
#endif

  if(in_error)
  {
    const char *tmp=in_error;
    in_error=0;
    Pike_fatal("Recursive error() calls, original error: %s", tmp);
  }
  in_error = fmt ? fmt : "no error message";

  if (fmt) {
    struct string_builder s;
    init_string_builder(&s, 0);
    string_builder_vsprintf(&s, fmt, *fmt_args);

#if 0
    if (!master_program) {
      fprintf(stderr, "ERROR: %s\n", s.s->str);
    }
#endif

    if (err->error_message) free_string(err->error_message);
    err->error_message = finish_string_builder(&s);
  }

  f_backtrace(0);

  if(func)
  {
    int i;
    push_int(0);
    push_int(0);
    push_text(func);

    for (i=0;i<args;i++)
      push_svalue(base_sp + i);
    f_aggregate(args + 3);
    f_aggregate(1);
    f_add(2);
  }

  if(Pike_sp[-1].type!=PIKE_T_ARRAY)
    Pike_fatal("f_backtrace failed to generate a backtrace!\n");

  if (err->error_backtrace) free_array(err->error_backtrace);
  err->error_backtrace=Pike_sp[-1].u.array;
  Pike_sp--;
  dmalloc_touch_svalue(Pike_sp);

  free_svalue(& throw_value);
  throw_value.type=PIKE_T_OBJECT;
  throw_value.subtype = 0;
  throw_value.u.object=o;
  throw_severity = THROW_ERROR;
  in_error=0;

  dmalloc_touch_svalue(& throw_value);

  pike_throw();  /* Hope someone is catching, or we will be out of balls. */
}


/* Throw a preallocated error object.
 *
 * NOTE: The object MUST NOT have been cloned by a plain low_clone()!
 *       At least fast_clone_object() MUST have been used, or the object
 *       data must have been properly initialized in some other way!
 */
PMOD_EXPORT DECLSPEC(noreturn) void throw_error_object(
  struct object *o,
  const char *func,
  struct svalue *base_sp,  int args,
  const char *desc, ...) ATTRIBUTE((noreturn))
{
  va_list foo;
  va_start(foo,desc);
  ASSERT_THREAD_SWAPPED_IN();
  DWERROR((stderr, "%s(): Throwing an error object\n", func));
  ERROR_DONE(generic);
}

PMOD_EXPORT DECLSPEC(noreturn) void generic_error(
  const char *func,
  struct svalue *base_sp,  int args,
  const char *desc, ...) ATTRIBUTE((noreturn))
{
  INIT_ERROR(generic);
  ERROR_DONE(generic);
}

PMOD_EXPORT DECLSPEC(noreturn) void index_error(
  const char *func,
  struct svalue *base_sp,  int args,
  struct svalue *value,
  struct svalue *index,
  const char *desc, ...) ATTRIBUTE((noreturn))
{
  INIT_ERROR(index);
  ERROR_COPY_SVALUE(index, value);
  ERROR_COPY_SVALUE(index, index);
  ERROR_DONE(generic);
}

PMOD_EXPORT DECLSPEC(noreturn) void bad_arg_error(
  const char *func,
  struct svalue *base_sp,  int args,
  int which_argument,
  const char *expected_type,
  struct svalue *got_value,
  const char *desc, ...)  ATTRIBUTE((noreturn))
{
  INIT_ERROR(bad_argument);
  ERROR_COPY(bad_argument, which_argument);
  if (expected_type)
    ERROR_STRUCT(bad_argument,o)->expected_type=make_shared_string(expected_type);
  else
    ERROR_STRUCT(bad_argument,o)->expected_type = NULL;
  ERROR_COPY_SVALUE(bad_argument, got_value);
  DWERROR((stderr, "%s():Bad arg %d (expected %s)\n",
	   func, which_argument, expected_type));
  ERROR_DONE(generic);
}

PMOD_EXPORT DECLSPEC(noreturn) void math_error(
  const char *func,
  struct svalue *base_sp,  int args,
  struct svalue *number,
  const char *desc, ...) ATTRIBUTE((noreturn))
{
  INIT_ERROR(math);
  if(number)
  {
    ERROR_COPY_SVALUE(math, number);
  }else{
    ERROR_STRUCT(math,o)->number.type=PIKE_T_INT;
    ERROR_STRUCT(math,o)->number.subtype=NUMBER_UNDEFINED;
    ERROR_STRUCT(math,o)->number.u.integer=0;
  }
  ERROR_DONE(generic);
}

PMOD_EXPORT DECLSPEC(noreturn) void resource_error(
  const char *func,
  struct svalue *base_sp,  int args,
  const char *resource_type,
  size_t howmuch_,
  const char *desc, ...) ATTRIBUTE((noreturn))
{
  INT_TYPE howmuch = DO_NOT_WARN((INT_TYPE)howmuch_);
  INIT_ERROR(resource);
  ERROR_COPY(resource, howmuch);
  ERROR_STRUCT(resource,o)->resource_type=make_shared_string(resource_type);
  ERROR_DONE(generic);
}

PMOD_EXPORT DECLSPEC(noreturn) void permission_error(
  const char *func,
  struct svalue *base_sp, int args,
  const char *permission_type,
  const char *desc, ...) ATTRIBUTE((noreturn))
{
  INIT_ERROR(permission);
  ERROR_STRUCT(permission,o)->permission_type=
    make_shared_string(permission_type);
  ERROR_DONE(generic);
}

PMOD_EXPORT void wrong_number_of_args_error(const char *name, int args, int expected)
{
  if(expected>args)
  {
    bad_arg_error (name, Pike_sp-args, args, expected, NULL, NULL,
		   "Too few arguments to %s(). Expected at least %d (got %d).\n",
		   name, expected, args);
  }else {
    bad_arg_error (name, Pike_sp-args, args, expected, NULL, NULL,
		   "Too many arguments to %s(). Expected at most %d (got %d).\n",
		   name, expected, args);
  }
}

#ifdef PIKE_DEBUG
static void gc_check_throw_value(struct callback *foo, void *bar, void *gazonk)
{
  gc_mark_external_svalues(&throw_value,1," in the throw value");
}
#endif

void init_error(void)
{
#define ERR_SETUP
#include "errors.h"

#ifdef PIKE_DEBUG
  dmalloc_accept_leak(add_gc_callback(gc_check_throw_value,0,0));

/*   recovery_lookup = allocate_mapping(100); */
#endif
}

void cleanup_error(void)
{
#ifdef PIKE_DEBUG
/*   free_mapping(recovery_lookup); */
#endif
#define ERR_CLEANUP
#include "errors.h"
}
#endif
