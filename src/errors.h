/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: errors.h,v 1.36 2006/09/20 16:15:55 mast Exp $
*/

#ifdef ERR_DECLARE
#define DECLARE_ERROR(NAME, SCNAME, INHERIT, DECL) \
PMOD_EXPORT struct program *PIKE_CONCAT(NAME,_error_program); \
ptrdiff_t PIKE_CONCAT(NAME,_error_offset); \
DECL

#define ERR_FUNC_SAVE_ID(VAR, NAME, FUNC, TYPE, FLAGS) static int VAR;

#endif

#ifdef ERR_EXT_DECLARE
#define DECLARE_ERROR(NAME, SCNAME, INHERIT, DECL) \
PMOD_EXPORT extern struct program *PIKE_CONCAT(NAME,_error_program); \
extern ptrdiff_t PIKE_CONCAT(NAME,_error_offset); \
struct PIKE_CONCAT(NAME,_error_struct) { \
  DECL \
};

#define ERR_VAR(TYPE,CTYPE,RUNTYPE,NAME) TYPE NAME ;

/* Some compilers (eg cl) don't like empty structs... */
#define EMPTY ERR_VAR(INT_TYPE, int, PIKE_T_INT, ignored__)

#endif /* ERR_EXT_DECLARE */

#ifdef ERR_SETUP
#define DECLARE_ERROR(NAME, SCNAME, INHERIT, DECL) do{\
  ptrdiff_t current_offset=0; \
  struct PIKE_CONCAT(NAME,_error_struct) foo; \
  start_new_program(); \
  INHERIT \
  current_offset = PIKE_CONCAT(NAME,_error_offset) = \
    ADD_STORAGE(struct PIKE_CONCAT(NAME,_error_struct));\
  add_string_constant("error_type", #SCNAME, 0); \
  add_integer_constant("is_" #NAME "_error",1,0); \
  DECL \
  PIKE_CONCAT(NAME,_error_program)=end_program(); \
  add_program_constant( #SCNAME "Error",PIKE_CONCAT(NAME,_error_program),0); \
}while(0);

#define ERR_VAR(TYPE,CTYPE,RUNTYPE,NAME2) \
  MAP_VARIABLE(#NAME2, CTYPE, 0, \
	       current_offset + (((char *)&(foo.NAME2))-((char *)&foo)), RUNTYPE);

/* Reference foo just to avoid warning. */
#define EMPTY (void) &foo;

#define ERR_INHERIT(NAME) \
  low_inherit(PIKE_CONCAT(NAME,_error_program),0,0,0,0,0);

#define ERR_FUNC(NAME,FUNC,TYPE,FLAGS) \
  ADD_FUNCTION(NAME,FUNC,TYPE,FLAGS);

#define ERR_FUNC_SAVE_ID(VAR, NAME, FUNC, TYPE, FLAGS) \
  VAR = ADD_FUNCTION(NAME,FUNC,TYPE,FLAGS);

#endif

#ifdef ERR_CLEANUP
#define DECLARE_ERROR(NAME, SCNAME, INHERIT, DECL) \
  if(PIKE_CONCAT(NAME,_error_program)) {\
    free_program(PIKE_CONCAT(NAME,_error_program)); \
    PIKE_CONCAT(NAME,_error_program)=0;\
  }
#endif

#ifndef EMPTY
#define EMPTY
#endif

#ifndef ERR_INHERIT
#define ERR_INHERIT(NAME)
#endif

#ifndef ERR_VAR
#define ERR_VAR(TYPE,CTYPE,RUNTYPE,NAME)
#endif

#ifndef ERR_FUNC
#define ERR_FUNC(NAME,FUNC,TYPE,FLAGS)
#endif

#ifndef ERR_FUNC_SAVE_ID
#define ERR_FUNC_SAVE_ID(VAR, NAME, FUNC, TYPE, FLAGS)
#endif

DECLARE_ERROR(generic, Generic, EMPTY ,
  ERR_VAR(struct pike_string *,tStr,PIKE_T_STRING,error_message)
  ERR_VAR(struct array *,tArray,PIKE_T_ARRAY,error_backtrace)
  ERR_FUNC("cast",f_error_cast,tFunc(tString,tArray),ID_STATIC)
  ERR_FUNC("`[]",f_error_index,tFunc(tInt01,tMixed),ID_STATIC)
  ERR_FUNC("describe",f_error_describe,tFunc(tVoid,tString),0)
  ERR_FUNC_SAVE_ID (generic_err_message_fun, "message", f_error_message,
		    tFunc(tVoid,tString), 0)
  ERR_FUNC_SAVE_ID (generic_err_backtrace_fun, "backtrace", f_error_backtrace,
		    tFunc(tVoid,tArr(tMixed)),0)
  ERR_FUNC("_sprintf", f_error__sprintf,
	   tFunc(tOr(tInt,tVoid) tOr(tMapping,tVoid),tString), ID_STATIC)
  ERR_FUNC("_is_type", f_error__is_type, tFunc(tString, tInt01), ID_STATIC)
  ERR_FUNC("create",f_error_create,tFunc(tStr tOr(tVoid,tArr(tMixed)),tVoid),ID_STATIC)
)

#define GENERIC_ERROR_THIS ((struct generic_error_struct *)CURRENT_STORAGE)

DECLARE_ERROR(index, Index,
	      ERR_INHERIT(generic),
  ERR_VAR(struct svalue, tMix, PIKE_T_MIXED, value)
  ERR_VAR(struct svalue, tMix, PIKE_T_MIXED, index)
)

DECLARE_ERROR(bad_argument, BadArgument,
	      ERR_INHERIT(generic),
  ERR_VAR(INT_TYPE, tInt, PIKE_T_INT, which_argument)
  ERR_VAR(struct pike_string *, tStr,PIKE_T_STRING,expected_type)
  ERR_VAR(struct svalue, tMix, PIKE_T_MIXED, got_value)
)

DECLARE_ERROR(math, Math,
	      ERR_INHERIT(generic),
  ERR_VAR(struct svalue, tMix, PIKE_T_MIXED, number)
)

DECLARE_ERROR(resource, Resource,
	      ERR_INHERIT(generic),
  ERR_VAR(struct pike_string *, tStr, PIKE_T_STRING,resource_type)
  ERR_VAR(INT_TYPE, tInt, PIKE_T_INT, howmuch)
)

DECLARE_ERROR(permission, Permission,
	      ERR_INHERIT(generic),
  ERR_VAR(struct pike_string *, tStr, PIKE_T_STRING,permission_type)
)

DECLARE_ERROR(cpp, Cpp, ERR_INHERIT(generic), EMPTY)

DECLARE_ERROR(compilation, Compilation, ERR_INHERIT(generic), EMPTY)

DECLARE_ERROR(master_load, MasterLoad, ERR_INHERIT (generic), EMPTY)

DECLARE_ERROR (module_load, ModuleLoad,
	       ERR_INHERIT (generic),
  ERR_VAR (struct pike_string *, tStr, PIKE_T_STRING, path)
  ERR_VAR (struct pike_string *, tStr, PIKE_T_STRING, reason)
)

#undef DECLARE_ERROR
#undef ERR_INHERIT
#undef ERR_VAR
#undef EMPTY
#undef ERR_FUNC
#undef ERR_FUNC_SAVE_ID

#undef ERR_DECLARE
#undef ERR_EXT_DECLARE
#undef ERR_SETUP
#undef ERR_CLEANUP
