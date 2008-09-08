/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: odbc.c,v 1.50 2008/09/08 13:44:14 mast Exp $
*/

/*
 * Pike interface to ODBC compliant databases.
 *
 * Henrik Grubbstr�m
 */

/*
 * Includes
 */

#include "global.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "interpret.h"
#include "object.h"
#include "threads.h"
#include "svalue.h"
#include "stralloc.h"
#include "mapping.h"
#include "array.h"
#include "multiset.h"
#include "program.h"
#include "module_support.h"
#include "bignum.h"
#include "builtin_functions.h"

#include "precompiled_odbc.h"


#define sp Pike_sp
#define fp Pike_fp

#ifdef HAVE_ODBC

/*
 * Globals
 */

struct program *odbc_program = NULL;

SQLHENV odbc_henv = SQL_NULL_HENV;

#ifdef PIKE_THREADS
/* See f_connect_lock doc below. */
static int enable_connect_lock = 1;
static PIKE_MUTEX_T connect_mutex STATIC_MUTEX_INIT;
#endif

/*
 * Functions
 */

/*
 * Helper functions
 */

#ifdef SQL_WCHAR
struct pike_string *make_shared_binary_sqlwchar(SQLWCHAR *str,
						size_t len)
{
  int shift = 1;
  if (sizeof(SQLWCHAR) == 4) shift = 2;
  return make_shared_binary_pcharp(MKPCHARP(str, shift), len);
}

void push_sqlwchar(SQLWCHAR *str, size_t len)
{
  push_string(make_shared_binary_sqlwchar(str, len));
}
#endif /* SQL_WCHAR */

void odbc_error(const char *fun, const char *msg,
		struct precompiled_odbc *odbc, SQLHSTMT hstmt,
		RETCODE code, void (*clean)(void*), void *clean_arg);

static INLINE void odbc_check_error(const char *fun, const char *msg,
				    RETCODE code,
				    void (*clean)(void *), void *clean_arg)
{
  if ((code != SQL_SUCCESS) && (code != SQL_SUCCESS_WITH_INFO)) {
    odbc_error(fun, msg, PIKE_ODBC, SQL_NULL_HSTMT, code, clean, clean_arg);
  }
}

void odbc_error(const char *fun, const char *msg,
		struct precompiled_odbc *odbc, SQLHSTMT hstmt,
		RETCODE code, void (*clean)(void *), void *clean_arg)
{
  RETCODE _code;
#ifdef SQL_WCHAR
  SQLWCHAR errcode[256];
  SQLWCHAR errmsg[SQL_MAX_MESSAGE_LENGTH];
#else
  unsigned char errcode[256];
  unsigned char errmsg[SQL_MAX_MESSAGE_LENGTH];
#endif
  SWORD errmsg_len = 0;
  SDWORD native_error;
  HDBC hdbc = odbc->hdbc;

  ODBC_ALLOW();
  _code =
#ifdef SQL_WCHAR
    SQLErrorW
#else
    SQLError
#endif
    (odbc_henv, hdbc, hstmt, errcode, &native_error,
     errmsg, (SQL_MAX_MESSAGE_LENGTH-1), &errmsg_len);
  errmsg[errmsg_len] = '\0';
  ODBC_DISALLOW();

  if (odbc) {
    if (odbc->last_error) {
      free_string(odbc->last_error);
    }
#ifdef SQL_WCHAR
    odbc->last_error = make_shared_binary_sqlwchar(errmsg, errmsg_len);
#else
    odbc->last_error = make_shared_binary_string((char *)errmsg, errmsg_len);
#endif
  }

  if (clean) {
    clean(clean_arg);
  }
  switch(_code) {
  case SQL_SUCCESS:
  case SQL_SUCCESS_WITH_INFO:
#ifdef SQL_WCHAR
    Pike_error("%s(): %s:\n"
	  "%d:%ls:%ls\n",
	  fun, msg, code, errcode, errmsg);
#else
    Pike_error("%s(): %s:\n"
	  "%d:%s:%s\n",
	  fun, msg, code, errcode, errmsg);
#endif
    break;
  case SQL_ERROR:
    Pike_error("%s(): %s:\n"
	  "SQLError failed (%d:SQL_ERROR)\n",
	  fun, msg, code);
    break;
  case SQL_NO_DATA_FOUND:
    Pike_error("%s(): %s:\n"
	  "SQLError failed (%d:SQL_NO_DATA_FOUND)\n",
	  fun, msg, code);
    break;
  case SQL_INVALID_HANDLE:
    Pike_error("%s(): %s:\n"
	  "SQLError failed (%d:SQL_INVALID_HANDLE)\n",
	  fun, msg, code);
    break;
  default:
    Pike_error("%s(): %s:\n"
	  "SQLError failed (%d:%d)\n",
	  fun, msg, code, _code);
    break;
  }
}

/*
 * Clean-up functions
 */

static void clean_last_error(void)
{
  if (PIKE_ODBC->last_error) {
    free_string(PIKE_ODBC->last_error);
    PIKE_ODBC->last_error = NULL;
  }
}

/*
 * Glue functions
 */

static void init_odbc_struct(struct object *o)
{
  HDBC hdbc = SQL_NULL_HDBC;
  RETCODE code;

  PIKE_ODBC->affected_rows = 0;
  PIKE_ODBC->flags = 0;
  PIKE_ODBC->last_error = NULL;

  ODBC_ALLOW();
  code = SQLAllocConnect(odbc_henv, &hdbc);
  ODBC_DISALLOW();
  PIKE_ODBC->hdbc = hdbc;

  odbc_check_error("init_odbc_struct", "ODBC initialization failed",
		   code, NULL, NULL);
}

static void exit_odbc_struct(struct object *o)
{
  SQLHDBC hdbc = PIKE_ODBC->hdbc;

  if (hdbc != SQL_NULL_HDBC) {
    unsigned int flags = PIKE_ODBC->flags;
    RETCODE code;
    const char *err_msg = NULL;

    PIKE_ODBC->flags &= ~PIKE_ODBC_CONNECTED;
    ODBC_ALLOW();
    if (flags & PIKE_ODBC_CONNECTED) {
      code = SQLDisconnect(hdbc);
      if (code != SQL_SUCCESS && code != SQL_SUCCESS_WITH_INFO)
	err_msg = "Disconnecting HDBC";
    }
    if (!err_msg) {
      code = SQLFreeConnect(hdbc);
      if (code != SQL_SUCCESS && code != SQL_SUCCESS_WITH_INFO)
	err_msg = "Freeing HDBC";
      hdbc = SQL_NULL_HDBC;
    }
    ODBC_DISALLOW();
    PIKE_ODBC->hdbc = hdbc;

    if (err_msg)
      /* NOTE: Potential recursion here! */
      odbc_check_error("odbc_error", err_msg, code,
		       (void (*)(void *))exit_odbc_struct, NULL);
  }

  clean_last_error();
}

/*
 * Pike functions
 */

static void f_error(INT32 args)
{
  pop_n_elems(args);

  if (PIKE_ODBC->last_error) {
    ref_push_string(PIKE_ODBC->last_error);
  } else {
    push_int(0);
  }
}

static void f_create(INT32 args)
{
  struct pike_string *server = NULL;
  struct pike_string *database = NULL;
  struct pike_string *user = NULL;
  struct pike_string *pwd = NULL;
  HDBC hdbc = PIKE_ODBC->hdbc;
  RETCODE code;

  check_all_args("odbc->create", args,
		 BIT_STRING|BIT_INT|BIT_VOID, BIT_STRING|BIT_INT|BIT_VOID,
		 BIT_STRING|BIT_INT|BIT_VOID, BIT_STRING|BIT_VOID|BIT_INT, 0);

#define GET_ARG(VAR, ARG) do {					\
    if ((args > (ARG)) && (sp[(ARG)-args].type == T_STRING)) {	\
      VAR = sp[(ARG)-args].u.string;				\
      if (VAR->size_shift) {					\
	SIMPLE_ARG_TYPE_ERROR("create", (ARG), "string(8bit)");	\
      }								\
    }								\
  } while(0)

  GET_ARG(pwd, 3);
  GET_ARG(user, 2);
  GET_ARG(database, 1);
  GET_ARG(server, 0);

#undef GET_ARG

  /*
   * NOTE:
   *
   *   If no database has been specified, use the server argument.
   *   If neither has been specified, connect to the database named "default".
   */

  if (!database || !database->len) {
    if (!server || !server->len) {
      push_constant_text("default");
      database = sp[-1].u.string;
      args++;
    } else {
      database = server;
    }
  }
  if (!user) {
    push_empty_string();
    user = sp[-1].u.string;
    args++;
  }
  if (!pwd) {
    push_empty_string();
    pwd = sp[-1].u.string;
    args++;
  }
  if (PIKE_ODBC->flags & PIKE_ODBC_CONNECTED) {
    PIKE_ODBC->flags &= ~PIKE_ODBC_CONNECTED;
    ODBC_ALLOW();
    code = SQLDisconnect(hdbc);
    ODBC_DISALLOW();
    /* Disconnect old hdbc */
    odbc_check_error("odbc->create", "Disconnecting HDBC", code, NULL, NULL);
  }

  /* FIXME: Support wide strings. */

  ODBC_ALLOW();
#ifdef PIKE_THREADS
  {
    int lock_enabled = enable_connect_lock;
    if (lock_enabled) mt_lock (&connect_mutex);
#endif
    code = SQLConnect(hdbc, (unsigned char *)database->str,
		      DO_NOT_WARN((SQLSMALLINT)database->len),
		      (unsigned char *)user->str,
		      DO_NOT_WARN((SQLSMALLINT)user->len),
		      (unsigned char *)pwd->str,
		      DO_NOT_WARN((SQLSMALLINT)pwd->len));
#ifdef PIKE_THREADS
    if (lock_enabled) mt_unlock (&connect_mutex);
  }
#endif
  ODBC_DISALLOW();
  odbc_check_error("odbc->create", "Connect failed", code, NULL, NULL);
  PIKE_ODBC->flags |= PIKE_ODBC_CONNECTED;
  pop_n_elems(args);
}

static void f_create_dsn(INT32 args)
{
  struct pike_string *connectstring = NULL;

  SQLCHAR outconnectionstring[1024]; /* Smallest allowed buffer = 1024 */
  SQLSMALLINT stringlength2;

  get_all_args("odbc->create_dsn", args, "%S", &connectstring);

  if (!connectstring->len) {
    Pike_error("odbc->create_dsn connection string empty.\n");
  }
  if (PIKE_ODBC->flags & PIKE_ODBC_CONNECTED) {
    PIKE_ODBC->flags &= ~PIKE_ODBC_CONNECTED;
    /* Disconnect old hdbc */
    odbc_check_error("odbc->create_dsn", "Disconnecting HDBC",
		     SQLDisconnect(PIKE_ODBC->hdbc), NULL, NULL);
  }
/* Microsoft ODBC >= 1.0
	SQLRETURN SQLDriverConnect(
     SQLHDBC     ConnectionHandle,
     SQLHWND     WindowHandle,
     SQLCHAR *     InConnectionString,
     SQLSMALLINT     StringLength1,
     SQLCHAR *     OutConnectionString,
     SQLSMALLINT     BufferLength,
     SQLSMALLINT *     StringLength2Ptr,
     SQLUSMALLINT     DriverCompletion);
*/
  odbc_check_error("odbc->create_dsn", "Connect failed",
    SQLDriverConnect(PIKE_ODBC->hdbc, 
      NULL, 
      (unsigned char *)connectstring->str,
      DO_NOT_WARN((SQLSMALLINT)connectstring->len),
      outconnectionstring,
      DO_NOT_WARN((SQLSMALLINT)1024),
      &stringlength2,
      SQL_DRIVER_NOPROMPT),
		   NULL, NULL);
  PIKE_ODBC->flags |= PIKE_ODBC_CONNECTED;
  pop_n_elems(args);
}


static void f_affected_rows(INT32 args)
{
  pop_n_elems(args);
  push_int64(PIKE_ODBC->affected_rows);
}

static void f_select_db(INT32 args)
{
  /**********************************************/
}

/* Needed since free_string() can be a macro */
static void odbc_free_string(struct pike_string *s)
{
  if (s) {
    free_string(s);
  }
}

static void f_big_query(INT32 args)
{
  ONERROR ebuf;
  struct pike_string *q = NULL;
#ifdef PIKE_DEBUG
  struct svalue *save_sp = sp + 1 - args;
#endif /* PIKE_DEBUG */

  get_all_args("odbc->big_query", args, "%W", &q);

  add_ref(q);
  SET_ONERROR(ebuf, odbc_free_string, q);

  pop_n_elems(args);

  clean_last_error();

  /* Allocate the statement (result) object */
  ref_push_object(fp->current_object);
  push_object(clone_object(odbc_result_program, 1));

  UNSET_ONERROR(ebuf);

  /* Potential return value is now in place */

  PIKE_ODBC->affected_rows = 0;

  /* Do the actual query */
  push_string(q);
  apply(sp[-2].u.object, "execute", 1);
  
  if (sp[-1].type != T_INT) {
    Pike_error("odbc->big_query(): Unexpected return value from "
	  "odbc_result->execute().\n");
  }

  if (!sp[-1].u.integer) {
    pop_n_elems(2);	/* Zap the result object too */

#ifdef ENABLE_IMPLICIT_COMMIT
    /* This breaks with Free TDS. */
    {
      HDBC hdbc = PIKE_ODBC->hdbc;
      RETCODE code;
      ODBC_ALLOW();
      code = SQLTransact(odbc_henv, hdbc, SQL_COMMIT);
      ODBC_DISALLOW();
      odbc_check_error("odbc->big_query", "Couldn't commit query",
		       code, NULL, NULL);
    }
#endif /* ENABLE_IMPLICIT_COMMIT */

    push_int(0);
  } else {
    pop_stack();	/* Keep the result object */
  }
#ifdef PIKE_DEBUG
  if (sp != save_sp) {
    Pike_fatal("Stack error in odbc->big_query().\n");
  }
#endif /* PIKE_DEBUG */
}

static void f_list_tables(INT32 args)
{
#ifdef PIKE_DEBUG
  struct svalue *save_sp = sp + 1 - args;
#endif /* PIKE_DEBUG */
  ONERROR ebuf;
  struct pike_string *pattern = NULL;

  if (args) {
    if ((Pike_sp[-args].type != T_STRING) ||
	(Pike_sp[-args].u.string->size_shift)) {
      Pike_error("odbc->list_tables(): "
		 "Bad argument 1. Expected 8-bit string.\n");
    }
    copy_shared_string(pattern, Pike_sp[-args].u.string);
  }

  SET_ONERROR(ebuf, odbc_free_string, pattern);

  pop_n_elems(args);

  clean_last_error();

  /* Allocate the statement (result) object */
  ref_push_object(fp->current_object);
  push_object(clone_object(odbc_result_program, 1));

  UNSET_ONERROR(ebuf);

  /* Potential return value is now in place */

  PIKE_ODBC->affected_rows = 0;

  /* Do the actual query */
  if (pattern) {
    push_string(pattern);
    apply(sp[-1].u.object, "list_tables", 1);
  } else {
    apply(sp[-1].u.object, "list_tables", 0);
  }
  
  if (sp[-1].type != T_INT) {
    Pike_error("odbc->list_tables(): Unexpected return value from "
	       "odbc_result->list_tables().\n");
  }

  if (!sp[-1].u.integer) {
    pop_n_elems(2);	/* Zap the result object too */

    push_int(0);
  } else {
    pop_stack();	/* Keep the result object */
  }
#ifdef PIKE_DEBUG
  if (sp != save_sp) {
    Pike_fatal("Stack error in odbc->list_tables().\n");
  }
#endif /* PIKE_DEBUG */
}

static void f_create_db(INT32 args)
{
  /**************************************************/
}

static void f_drop_db(INT32 args)
{
  /**************************************************/
}

static void f_shutdown(INT32 args)
{
  /**************************************************/
}

static void f_reload(INT32 args)
{
  /**************************************************/
}

static void f_list_dbs(INT32 args)
{
#ifdef SQL_WCHAR
  static SQLWCHAR buf[SQL_MAX_DSN_LENGTH+1];
  static SQLWCHAR descr[256];
#else
  static UCHAR buf[SQL_MAX_DSN_LENGTH+1];
  static UCHAR descr[256];
#endif
  SQLSMALLINT buf_len = 0;
  SQLSMALLINT descr_len = 0;
  int cnt = 0;
  RETCODE ret;

  pop_n_elems(args);

  ODBC_ALLOW();
  ret =
#ifdef SQL_WCHAR
    SQLDataSourcesW
#else
    SQLDataSources
#endif
    (odbc_henv, SQL_FETCH_FIRST,
     buf, SQL_MAX_DSN_LENGTH, &buf_len,
     descr, 255, &descr_len);
  ODBC_DISALLOW();

  while ((ret == SQL_SUCCESS) || (ret == SQL_SUCCESS_WITH_INFO)) {
#ifdef SQL_WCHAR
    push_sqlwchar(buf, buf_len);
#else
    push_string(make_shared_binary_string(buf, buf_len));
#endif
    cnt++;

    ODBC_ALLOW();
    ret =
#ifdef SQL_WCHAR
      SQLDataSourcesW
#else
      SQLDataSources
#endif
      (odbc_henv, SQL_FETCH_NEXT,
       buf, SQL_MAX_DSN_LENGTH, &buf_len,
       descr, 255, &descr_len);
    ODBC_DISALLOW();
  }

  f_aggregate(cnt);
}

#ifdef PIKE_THREADS
/*! @decl int(0..1) connect_lock (void|int enable)
 *!
 *! Enable or disable a mutex that serializes all ODBC SQLConnect
 *! calls (i.e. when ODBC connections are created). This lock might be
 *! necessary to work around bugs in ODBC drivers.
 *!
 *! @param enable
 *!   Enables the mutex if nonzero, disables it otherwise. The state
 *!   is not changed if this argument is left out.
 *!
 *! @returns
 *!   The old state of the flag.
 *!
 *! @note
 *! This is currently enabled by default due to bugs in the current
 *! FreeTDS library (version 0.63), but that might change if the
 *! demand for this kludge ceases in the future. Therefore, if this
 *! setting is important to you then always set it explicitly.
 *! Hopefully most users don't need to bother with it.
 */
static void f_connect_lock (INT32 args)
{
  int old = enable_connect_lock;
  if (args) {
    enable_connect_lock = !UNSAFE_IS_ZERO (Pike_sp - args);
    pop_n_elems (args);
  }
  push_int (old);
}
#endif

#endif /* HAVE_ODBC */

/*
 * Module linkage
 */

PIKE_MODULE_INIT
{
#ifdef HAVE_ODBC
  RETCODE err = SQLAllocEnv(&odbc_henv);

  if (err != SQL_SUCCESS) {
    odbc_henv = SQL_NULL_HENV;
    return;
    /*    Pike_error("odbc_module_init(): SQLAllocEnv() failed with code %08x\n", err); */
  }

#if defined (PIKE_THREADS) && !defined (HAS_STATIC_MUTEX_INIT)
  mt_init (&connect_mutex);
#endif

  start_new_program();
  ADD_STORAGE(struct precompiled_odbc);

  /* function(void:int|string) */
  ADD_FUNCTION("error", f_error,tFunc(tVoid,tOr(tInt,tStr)), ID_PUBLIC);
  /* function(string|void, string|void, string|void, string|void:void) */
  ADD_FUNCTION("create", f_create,tFunc(tOr(tStr,tVoid) tOr(tStr,tVoid) tOr(tStr,tVoid) tOr(tStr,tVoid),tVoid), ID_PUBLIC);
  /* function(string:void) */
  ADD_FUNCTION("create_dsn", f_create_dsn,tFunc(tStr,tVoid), ID_PUBLIC);
  /* function(string:void) */
  ADD_FUNCTION("select_db", f_select_db,tFunc(tStr,tVoid), ID_PUBLIC);
  /* function(string:int|object) */
  ADD_FUNCTION("big_query", f_big_query,tFunc(tStr,tOr(tInt,tObj)), ID_PUBLIC);
  /* function(void:int) */
  ADD_FUNCTION("affected_rows", f_affected_rows,tFunc(tVoid,tInt), ID_PUBLIC);
  /* function(void|string:object) */
  ADD_FUNCTION("list_tables", f_list_tables,tFunc(tOr(tVoid,tStr),tObj), ID_PUBLIC);
  /* NOOP's: */
  /* function(string:void) */
  ADD_FUNCTION("create_db", f_create_db,tFunc(tStr,tVoid), ID_PUBLIC);
  /* function(string:void) */
  ADD_FUNCTION("drop_db", f_drop_db,tFunc(tStr,tVoid), ID_PUBLIC);
  /* function(void:void) */
  ADD_FUNCTION("shutdown", f_shutdown,tFunc(tVoid,tVoid), ID_PUBLIC);
  /* function(void:void) */
  ADD_FUNCTION("reload", f_reload,tFunc(tVoid,tVoid), ID_PUBLIC);
#if 0
  /* function(void:int) */
  ADD_FUNCTION("insert_id", f_insert_id,tFunc(tVoid,tInt), ID_PUBLIC);
  /* function(void:string) */
  ADD_FUNCTION("statistics", f_statistics,tFunc(tVoid,tStr), ID_PUBLIC);
  /* function(void:string) */
  ADD_FUNCTION("server_info", f_server_info,tFunc(tVoid,tStr), ID_PUBLIC);
  /* function(void:string) */
  ADD_FUNCTION("host_info", f_host_info,tFunc(tVoid,tStr), ID_PUBLIC);
  /* function(void:int) */
  ADD_FUNCTION("protocol_info", f_protocol_info,tFunc(tVoid,tInt), ID_PUBLIC);
  /* function(string, void|string:array(int|mapping(string:mixed))) */
  ADD_FUNCTION("list_fields", f_list_fields,tFunc(tStr tOr(tVoid,tStr),tArr(tOr(tInt,tMap(tStr,tMix)))), ID_PUBLIC);
  /* function(void|string:object) */
  ADD_FUNCTION("list_processes", f_list_processes,tFunc(tOr(tVoid,tStr),tObj), ID_PUBLIC);
 
  /* function(void:int) */
  ADD_FUNCTION("binary_data", f_binary_data,tFunc(tVoid,tInt), ID_PUBLIC);
#endif /* 0 */
 
  set_init_callback(init_odbc_struct);
  set_exit_callback(exit_odbc_struct);
 
  odbc_program = end_program();
  add_program_constant("odbc", odbc_program, 0);
 
  /* function(void|string:array(string)) */
  ADD_FUNCTION("list_dbs", f_list_dbs,tFunc(tOr(tVoid,tStr),tArr(tStr)), ID_PUBLIC);

  ADD_FUNCTION ("connect_lock", f_connect_lock, tFunc(tOr(tVoid,tInt),tInt01), ID_PUBLIC);

  init_odbc_res_programs();

#else
  if(!TEST_COMPAT(7,6))
    HIDE_MODULE();
#endif /* HAVE_ODBC */
}

PIKE_MODULE_EXIT
{
#ifdef HAVE_ODBC
  exit_odbc_res();

#if defined (PIKE_THREADS) && !defined (HAS_STATIC_MUTEX_INIT)
  mt_destroy (&connect_mutex);
#endif
 
  if (odbc_program) {
    free_program(odbc_program);
    odbc_program = NULL;
  }

  if (odbc_henv != SQL_NULL_HENV) {
    RETCODE err = SQLFreeEnv(odbc_henv);
    odbc_henv = SQL_NULL_HENV;

    if ((err != SQL_SUCCESS) && (err != SQL_SUCCESS_WITH_INFO)) {
      Pike_error("odbc_module_exit(): SQLFreeEnv() failed with code %08x\n", err);
    }
  }
#endif /* HAVE_ODBC */
}
