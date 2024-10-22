/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

#ifndef PIKE_COMPILER_H
#define PIKE_COMPILER_H

#include "lex.h"
#include "program.h"

struct compilation
{
  struct Supporter supporter;
  struct pike_string *prog;		/* String to compile. */
  struct object *handler;		/* error_handler */
  struct object *compat_handler;	/* compat_handler */
  int major, minor;			/* Base compat version */
  struct program *target;		/* Program being compiled. */
  struct object *placeholder;
  int flags;
  
  struct program *p;			/* Compiled program or NULL. */
  struct lex lex;
  int compilation_inherit;		/* Inherit in supporter->self containing
					 * compilation_program. */

  struct svalue default_module;		/* predef:: */
  dynamic_buffer used_modules;		/* Stack of svalues with imported
					 * modules. */
  INT32 num_used_modules;		/* Number of entries on the stack. */

  int compilation_depth;		/* Current class nesting depth. */

  int saved_threads_disabled;
  struct mapping *resolve_cache_save;

};

#ifdef PIKE_DEBUG
#define CHECK_COMPILER()	do {				\
    if (!Pike_fp || !compilation_program ||			\
	Pike_fp->context->prog != compilation_program) {	\
      Pike_fatal("Invalid compilation context!\n");		\
    }								\
  } while(0)
#else
#define CHECK_COMPILER()
#endif
#define THIS_COMPILATION  ((struct compilation *)(Pike_fp->current_storage))
#define MAYBE_THIS_COMPILATION  ((Pike_fp && compilation_program && (Pike_fp->context->prog == compilation_program))?THIS_COMPILATION:NULL)

/* Flags. */
#define COMPILER_BUSY	1	/* The compiler is busy compiling. */
#define COMPILER_DONE	2	/* The compiler is finished compiling. */

/* CompilerEnvironment function numbers. */
#define CE_REPORT_FUN_NUM				0
#define CE_COMPILE_FUN_NUM				1
#define CE_RESOLV_FUN_NUM				2
#define CE_PIKE_COMPILER_FUN_NUM			3
#define CE_GET_COMPILATION_HANDLER_FUN_NUM		4
#define CE_GET_DEFAULT_MODULE_FUN_NUM			5
#define CE_HANDLE_INHERIT_FUN_NUM			6

/* PikeCompiler function numbers. */
#define PC_REPORT_FUN_NUM				0
#define PC_COMPILE_FUN_NUM				1
#define PC_RESOLV_FUN_NUM				2
#define PC_CREATE_FUN_NUM				3
#define PC_GET_COMPILATION_HANDLER_FUN_NUM		4
#define PC_GET_DEFAULT_MODULE_FUN_NUM			5
#define PC_CHANGE_COMPILER_COMPATIBILITY_FUN_NUM	6
#define PC_HANDLE_INHERIT_FUN_NUM			7
#define PC_POP_TYPE_ATTRIBUTE_FUN_NUM			8
#define PC_PUSH_TYPE_ATTRIBUTE_FUN_NUM			9
#define PC_APPLY_TYPE_ATTRIBUTE_FUN_NUM			10
#define PC_APPLY_ATTRIBUTE_CONSTANT_FUN_NUM		11

#endif	/* !PIKE_COMPILER_H */
