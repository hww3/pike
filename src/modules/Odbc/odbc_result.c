/*
 * $Id: odbc_result.c,v 1.25 2001/07/12 14:00:51 grubba Exp $
 *
 * Pike  interface to ODBC compliant databases
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

RCSID("$Id: odbc_result.c,v 1.25 2001/07/12 14:00:51 grubba Exp $");

#include "interpret.h"
#include "object.h"
#include "threads.h"
#include "stralloc.h"
#include "mapping.h"
#include "array.h"
#include "multiset.h"
#include "program.h"
#include "array.h"
#include "operators.h"
#include "builtin_functions.h"
#include "pike_memory.h"
#include "pike_macros.h"
#include "module_support.h"
#include "bignum.h"

#include "precompiled_odbc.h"

/* must be included last */
#include "module_magic.h"

#ifdef HAVE_ODBC

/* #define ODBC_DEBUG */

/*
 * Constants
 */

/* Buffer size used when retrieving BLOBs
 *
 * Allow it to be specified from the command line.
 */
#ifndef BLOB_BUFSIZ
#define BLOB_BUFSIZ	1024
#endif /* !BLOB_BUFSIZ */

/*
 * Globals
 */

struct program *odbc_result_program = NULL;

/*
 * Functions
 */

/*
 * Help functions
 */

static void clean_sql_res(void)
{
  if (PIKE_ODBC_RES->field_info) {
    free(PIKE_ODBC_RES->field_info);
    PIKE_ODBC_RES->field_info = NULL;
  }
  if (PIKE_ODBC_RES->fields) {
    free_array(PIKE_ODBC_RES->fields);
    PIKE_ODBC_RES->fields = NULL;
  }
  if (PIKE_ODBC_RES->obj) {
    free_object(PIKE_ODBC_RES->obj);
    PIKE_ODBC_RES->obj = NULL;
    PIKE_ODBC_RES->odbc = NULL;
  }
  PIKE_ODBC_RES->hstmt = SQL_NULL_HSTMT;
}

static INLINE void odbc_check_error(const char *fun, const char *msg,
				    RETCODE code, void (*clean)(void))
{
  if ((code != SQL_SUCCESS) && (code != SQL_SUCCESS_WITH_INFO)) {
    odbc_error(fun, msg, PIKE_ODBC_RES->odbc, PIKE_ODBC_RES->hstmt,
	       code, clean);
  }
}

/*
 * State maintenance
 */
 
static void init_res_struct(struct object *o)
{
  memset(PIKE_ODBC_RES, 0, sizeof(struct precompiled_odbc_result));
  PIKE_ODBC_RES->hstmt = SQL_NULL_HSTMT;
}
 
static void exit_res_struct(struct object *o)
{
  if (PIKE_ODBC_RES->hstmt != SQL_NULL_HSTMT) {
    HSTMT hstmt = PIKE_ODBC_RES->hstmt;
    PIKE_ODBC_RES->hstmt = SQL_NULL_HSTMT;
    odbc_check_error("exit_res_struct", "Freeing of HSTMT failed",
		     SQLFreeStmt(hstmt, SQL_DROP), clean_sql_res);
  }
  clean_sql_res();
}

/*
 * More help functions
 */

static void odbc_fix_fields(void)
{
  int i;
  SWORD *odbc_field_types = alloca(sizeof(SWORD) * PIKE_ODBC_RES->num_fields);
  size_t buf_size = 1024;
  unsigned char *buf = alloca(buf_size);

  if ((!buf)||(!odbc_field_types)) {
    Pike_error("odbc_fix_fields(): Out of memory\n");
  }

  /*
   * First build the fields-array;
   */
  for (i=0; i < PIKE_ODBC_RES->num_fields; i++) {
    int nbits;
    SWORD name_len;
    SWORD sql_type;
    SQLULEN precision;
    SWORD scale;
    SWORD nullable;

    while (1) {
      odbc_check_error("odbc_fix_fields", "Failed to fetch field info",
		       SQLDescribeCol(PIKE_ODBC_RES->hstmt, i+1,
				      buf,
				      DO_NOT_WARN((SQLSMALLINT)buf_size),
				      &name_len,
				      &sql_type, &precision, &scale, &nullable),
		       (void(*)(void))0);
      if (name_len < (ptrdiff_t)buf_size) {
	break;
      }
      do {
	buf_size *= 2;
      } while (name_len >= (ptrdiff_t)buf_size);
      if (!(buf = alloca(buf_size))) {
	Pike_error("odbc_fix_fields(): Out of memory\n");
      }
    }
#ifdef ODBC_DEBUG
    fprintf(stderr, "ODBC:odbc_fix_fields():\n"
	    "name:%s\n"
	    "sql_type:%d\n"
	    "precision:%ld\n"
	    "scale:%d\n"
	    "nullable:%d\n",
	    buf, sql_type, precision, scale, nullable);
#endif /* ODBC_DEBUG */
    /* Create the mapping */
    push_text("name");
    push_string(make_shared_binary_string((char *)buf, name_len));
    push_text("type");
    odbc_field_types[i] = SQL_C_CHAR;
    switch(sql_type) {
    case SQL_CHAR:
      push_text("char");
      break;
    case SQL_NUMERIC:
      push_text("numeric");
      break;
    case SQL_DECIMAL:
      push_text("decimal");
      break;
    case SQL_INTEGER:
      push_text("integer");
      break;
    case SQL_SMALLINT:
      push_text("short");
      break;
    case SQL_FLOAT:
      push_text("float");
      break;
    case SQL_REAL:
      push_text("real");
      break;
    case SQL_DOUBLE:
      push_text("double");
      break;
    case SQL_VARCHAR:
      push_text("var string");
      odbc_field_types[i] = SQL_C_BINARY;
      break;
    case SQL_DATE:
      push_text("date");
      break;
    case SQL_TIMESTAMP:
      push_text("time");
      break;
    case SQL_LONGVARCHAR:
      push_text("var string");
      odbc_field_types[i] = SQL_C_BINARY;
      break;
    case SQL_BINARY:
      push_text("binary");
      odbc_field_types[i] = SQL_C_BINARY;
      break;
    case SQL_VARBINARY:
      push_text("blob");
      odbc_field_types[i] = SQL_C_BINARY;
      break;
    case SQL_LONGVARBINARY:
      push_text("long blob");
      odbc_field_types[i] = SQL_C_BINARY;
      break;
    case SQL_BIGINT:
      push_text("long integer");
      break;
    case SQL_TINYINT:
      push_text("tiny integer");
      break;
    case SQL_BIT:
      push_text("bit");
      break;
    default:
      push_text("unknown");
      odbc_field_types[i] = SQL_C_BINARY;
      break;
    }
    push_text("length"); push_int64(precision);
    push_text("decimals"); push_int(scale);
    push_text("flags");
    nbits = 0;
    if (nullable == SQL_NULLABLE) {
      nbits++;
      push_text("nullable");
    }
    if ((sql_type == SQL_LONGVARCHAR) ||
	(sql_type == SQL_LONGVARBINARY)) {
      nbits++;
      push_text("blob");
    }
    f_aggregate_multiset(nbits);

    f_aggregate_mapping(5*2);

  }
  f_aggregate(PIKE_ODBC_RES->num_fields);

  add_ref(PIKE_ODBC_RES->fields = sp[-1].u.array);
  pop_stack();

  PIKE_ODBC_RES->field_info = (struct field_info *)
    xalloc(sizeof(struct field_info) * PIKE_ODBC_RES->num_fields);

  /*
   * Now it's time to bind the columns
   */
  for (i=0; i < PIKE_ODBC_RES->num_fields; i++)
    PIKE_ODBC_RES->field_info[i].type = odbc_field_types[i];
}

/*
 * Methods
 */

/* void create(object(odbc)) */
static void f_create(INT32 args)
{
  HSTMT hstmt = SQL_NULL_HSTMT;

  if (!args) {
    Pike_error("Too few arguments to odbc_result()\n");
  }
  if ((sp[-args].type != T_OBJECT) ||
      (!(PIKE_ODBC_RES->odbc =
	 (struct precompiled_odbc *)get_storage(sp[-args].u.object,
						odbc_program)))) {
    Pike_error("Bad argument 1 to odbc_result()\n");
  }
  add_ref(PIKE_ODBC_RES->obj = sp[-args].u.object);

  odbc_check_error("odbc_result", "Statement allocation failed",
		   SQLAllocStmt(PIKE_ODBC_RES->odbc->hdbc, &hstmt), NULL);
  PIKE_ODBC_RES->hstmt = hstmt;
}

static void f_execute(INT32 args)
{
  struct pike_string *q = NULL;
  HSTMT hstmt = PIKE_ODBC_RES->hstmt;

  get_all_args("odbc_result->execute", args, "%S", &q);

  odbc_check_error("odbc_result->execute", "Query failed",
		   SQLExecDirect(hstmt, (unsigned char *)q->str,
				 DO_NOT_WARN((SQLINTEGER)(q->len))),
		   NULL);

  odbc_check_error("odbc_result->execute", "Couldn't get the number of fields",
		   SQLNumResultCols(hstmt, &(PIKE_ODBC_RES->num_fields)),
		   NULL);

  odbc_check_error("odbc_result->execute", "Couldn't get the number of rows",
		   SQLRowCount(hstmt, &(PIKE_ODBC_RES->num_rows)), NULL);

  PIKE_ODBC_RES->odbc->affected_rows = PIKE_ODBC_RES->num_rows;

  if (PIKE_ODBC_RES->num_fields) {
    odbc_fix_fields();
  }

  pop_n_elems(args);

  /* Result */
  push_int(PIKE_ODBC_RES->num_fields);
}
 
/* int num_rows() */
static void f_num_rows(INT32 args)
{
  pop_n_elems(args);
  push_int64(PIKE_ODBC_RES->num_rows);
}

/* int num_fields() */
static void f_num_fields(INT32 args)
{
  pop_n_elems(args);
  push_int(PIKE_ODBC_RES->num_fields);
}

/* array(int|mapping(string:mixed)) fetch_fields() */
static void f_fetch_fields(INT32 args)
{
  pop_n_elems(args);

  ref_push_array(PIKE_ODBC_RES->fields);
}
 
/* int|array(string|float|int) fetch_row() */
static void f_fetch_row(INT32 args)
{
  int i;
  RETCODE code;
 
  pop_n_elems(args);

  code = SQLFetch(PIKE_ODBC_RES->hstmt);
  
  if (code == SQL_NO_DATA_FOUND) {
    /* No rows left in result */
    push_int(0);
  } else {
    odbc_check_error("odbc->fetch_row", "Couldn't fetch row",
		     code, NULL);
 
    for (i=0; i < PIKE_ODBC_RES->num_fields; i++) {
	/* BLOB */
	int num_strings = 0;
	char buf[BLOB_BUFSIZ+1];
	SQLLEN len = 0;

	while(1) {
	  code = SQLGetData(PIKE_ODBC_RES->hstmt, (SQLUSMALLINT)(i+1),
			    PIKE_ODBC_RES->field_info[i].type,
			    buf, BLOB_BUFSIZ, &len);
	  if (code == SQL_NO_DATA_FOUND) {
#ifdef ODBC_DEBUG
	    fprintf(stderr, "ODBC:fetch_row(): NO DATA\n");
#endif /* ODBC_DEBUG */
	    if (!num_strings) {
	      num_strings++;
	      push_constant_text("");
	    }
	    break;
	  }
	  odbc_check_error("odbc->fetch_row", "SQLGetData() failed",
			   code, NULL);
	  if (len == SQL_NULL_DATA) {
#ifdef ODBC_DEBUG
	    fprintf(stderr, "ODBC:fetch_row(): NULL\n");
#endif /* ODBC_DEBUG */
	    if (!num_strings) {
	      /* NULL */
	      push_int(0);
	    }
	    break;
	  } else {
	    num_strings++;
#ifdef ODBC_DEBUG
	    fprintf(stderr, "[%d] ", num_strings);
#endif /* ODBC_DEBUG */
	    if (len < BLOB_BUFSIZ) {
	      push_string(make_shared_binary_string(buf, len));
	      break;
	    } else {
	      push_string(make_shared_binary_string(buf, BLOB_BUFSIZ));
	    }
	  }
	}
	if (num_strings > 1) {
#ifdef ODBC_DEBUG
	  fprintf(stderr, "ODBC:fetch_row(): Joining %d strings\n",
		  num_strings);
#endif /* ODBC_DEBUG */
	  f_add(num_strings);
	}
    }
    f_aggregate(PIKE_ODBC_RES->num_fields);
  }
}
 
/* int eof() */
static void f_eof(INT32 args)
{
  Pike_error("odbc->eof(): Not implemented yet!\n");
}

/* void seek() */
static void f_seek(INT32 args)
{
  Pike_error("odbc->seek(): Not implemented yet!\n");
}
 
/*
 * Module linkage
 */
 
void init_odbc_res_programs(void)
{
  /*
   * start_new_program();
   *
   * add_storage();
   *
   * add_function();
   * add_function();
   * ...
   *
   * set_init_callback();
   * set_exit_callback();
   *
   * program = end_c_program();
   * program->refs++;
   *
   */
 
  start_new_program();
  ADD_STORAGE(struct precompiled_odbc_result);

  map_variable("_odbc", "object", 0,
	       OFFSETOF(precompiled_odbc_result, obj), T_OBJECT);
  map_variable("_fields", "array(mapping(string:mixed))", 0,
	       OFFSETOF(precompiled_odbc_result, fields), T_ARRAY);
 
  /* function(object:void) */
  ADD_FUNCTION("create", f_create,tFunc(tObj,tVoid), ID_PUBLIC);
  /* function(string:int) */
  ADD_FUNCTION("execute", f_execute,tFunc(tStr,tInt), ID_PUBLIC);
  /* function(void:int) */
  ADD_FUNCTION("num_rows", f_num_rows,tFunc(tVoid,tInt), ID_PUBLIC);
  /* function(void:int) */
  ADD_FUNCTION("num_fields", f_num_fields,tFunc(tVoid,tInt), ID_PUBLIC);
#ifdef SUPPORT_FIELD_SEEK
  /* function(int:void) */
  ADD_FUNCTION("field_seek", f_field_seek,tFunc(tInt,tVoid), ID_PUBLIC);
#endif /* SUPPORT_FIELD_SEEK */
  /* function(void:int) */
  ADD_FUNCTION("eof", f_eof,tFunc(tVoid,tInt), ID_PUBLIC);
#ifdef SUPPORT_FIELD_SEEK
  /* function(void:int|mapping(string:mixed)) */
  ADD_FUNCTION("fetch_field", f_fetch_field,tFunc(tVoid,tOr(tInt,tMap(tStr,tMix))), ID_PUBLIC);
#endif /* SUPPORT_FIELD_SEEK */
  /* function(void:array(int|mapping(string:mixed))) */
  ADD_FUNCTION("fetch_fields", f_fetch_fields,tFunc(tVoid,tArr(tOr(tInt,tMap(tStr,tMix)))), ID_PUBLIC);
  /* function(int:void) */
  ADD_FUNCTION("seek", f_seek,tFunc(tInt,tVoid), ID_PUBLIC);
  /* function(void:int|array(string|int|float)) */
  ADD_FUNCTION("fetch_row", f_fetch_row,tFunc(tVoid,tOr(tInt,tArr(tOr3(tStr,tInt,tFlt)))), ID_PUBLIC);
 
  set_init_callback(init_res_struct);
  set_exit_callback(exit_res_struct);
 
  odbc_result_program = end_program();
  add_program_constant("odbc_result",odbc_result_program, 0);
}
 
void exit_odbc_res(void)
{
  if (odbc_result_program) {
    free_program(odbc_result_program);
    odbc_result_program = NULL;
  }
}

#else
static int place_holder;	/* Keep the compiler happy */
#endif /* HAVE_ODBC */
