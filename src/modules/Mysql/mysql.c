/*
 * $Id: mysql.c,v 1.41 2001/01/15 22:14:34 mast Exp $
 *
 * SQL database functionality for Pike
 *
 * Henrik Grubbstr�m 1996-12-21
 */

/* Master Pike headerfile */
#include "global.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_MYSQL

/*
 * Includes
 */

#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif

/* From the mysql-dist */
/* Workaround for versions prior to 3.20.0 not beeing protected for
 * multiple inclusion.
 */
#ifndef _mysql_h
#ifdef HAVE_MYSQL_H
#include <mysql.h>
#else
#ifdef HAVE_MYSQL_MYSQL_H
#include <mysql/mysql.h>
#else
#error Need mysql.h header-file
#endif /* HAVE_MYSQL_MYSQL_H */
#endif /* HAVE_MYSQL_H */
#ifndef _mysql_h
#define _mysql_h
#endif
#endif

#ifdef HAVE_ERRMSG_H
#include <errmsg.h>
#else /* !HAVE_ERRMSG_H */
#ifdef HAVE_MYSQL_ERRMSG_H
#include <mysql/errmsg.h>
#endif /* HAVE_MYSQL_ERRMSG_H */
#endif /* HAVE_ERRMGS_H */


/* dynamic_buffer.h contains a conflicting typedef for string
 * we don't use any dynamic buffers, so we have this work-around
 */
#define DYNAMIC_BUFFER_H
typedef struct dynamic_buffer_s dynamic_buffer;

#endif /* HAVE_MYSQL */

/* From the Pike-dist */
#include "svalue.h"
#include "object.h"
#include "stralloc.h"
#include "interpret.h"
#include "port.h"
#include "pike_error.h"
#include "threads.h"
#include "program.h"
#include "operators.h"
#include "builtin_functions.h"
#include "fd_control.h"

/* System includes */
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

#include "module_magic.h"

#ifdef HAVE_MYSQL

/* Local includes */
#include "precompiled_mysql.h"

/*
 * Globals
 */

RCSID("$Id: mysql.c,v 1.41 2001/01/15 22:14:34 mast Exp $");

/*
**! module Mysql
**!
**!	This module enables access to the Mysql database from within Pike.
**!
**!	Mysql is available from www.tcx.se.
**!
**! see also: Mysql.mysql, Mysql.result, Sql.sql
**!
**! note
**!	$Id: mysql.c,v 1.41 2001/01/15 22:14:34 mast Exp $
**!
**! class mysql
**!
**!	Mysql.mysql is a pre-compiled Pike program. It enables
**!	access to the Mysql database from within Pike. Mysql.mysql
**!	is a part of the Mysql module.
**!
**!	Mysql is available from	www.tcx.se.
**!
**! see also: Mysql.result, Sql.sql
*/

struct program *mysql_program = NULL;

#if defined(HAVE_MYSQL_PORT) || defined(HAVE_MYSQL_UNIX_PORT)
#ifdef HAVE_MYSQL_PORT
extern unsigned int mysql_port;
#endif /* HAVE_MYSQL_PORT */
#ifdef HAVE_MYSQL_UNIX_PORT
extern char *mysql_unix_port;
#endif /* HAVE_MYSQL_UNIX_PORT */
#ifdef _REENTRANT
static MUTEX_T stupid_port_lock;
#define STUPID_PORT_INIT()	mt_init(&stupid_port_lock)
#define STUPID_PORT_LOCK()	mt_lock(&stupid_port_lock)
#define STUPID_PORT_UNLOCK()	mt_unlock(&stupid_port_lock)
#define STUPID_PORT_DESTROY()	mt_destroy(&stupid_port_lock)
#else /* !_REENTRANT */
#define STUPID_PORT_INIT()
#define STUPID_PORT_LOCK()
#define STUPID_PORT_UNLOCK()
#define STUPID_PORT_DESTROY()
#endif /* _REENTRANT */
#endif /* HAVE_MYSQL_PORT */

#ifdef _REENTRANT
#define MYSQL_LOCK		(&(PIKE_MYSQL->lock))
#define INIT_MYSQL_LOCK()	mt_init(MYSQL_LOCK)
#define DESTROY_MYSQL_LOCK()	mt_destroy(MYSQL_LOCK)
#define MYSQL_ALLOW()		do { MUTEX_T *__l = MYSQL_LOCK; THREADS_ALLOW(); mt_lock(__l);
#define MYSQL_DISALLOW()	mt_unlock(__l); THREADS_DISALLOW(); } while(0)
#else /* !_REENTRANT */
#define INIT_MYSQL_LOCK()
#define DESTROY_MYSQL_LOCK()
#define MYSQL_ALLOW()
#define MYSQL_DISALLOW()
#endif /* _REENTRANT */


/*
 * Functions
 */

/*
 * State maintenance
 */

static void init_mysql_struct(struct object *o)
{
  MEMSET(PIKE_MYSQL, 0, sizeof(struct precompiled_mysql));
  INIT_MYSQL_LOCK();
}

static void exit_mysql_struct(struct object *o)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  MYSQL *mysql = PIKE_MYSQL->mysql;
  MYSQL_RES *last_result = PIKE_MYSQL->last_result;

  PIKE_MYSQL->last_result = NULL;
  PIKE_MYSQL->socket = NULL;
  PIKE_MYSQL->mysql = NULL;

  if (PIKE_MYSQL->password) {
    free_string(PIKE_MYSQL->password);
    PIKE_MYSQL->password = NULL;
  }
  if (PIKE_MYSQL->user) {
    free_string(PIKE_MYSQL->user);
    PIKE_MYSQL->user = NULL;
  }
  if (PIKE_MYSQL->database) {
    free_string(PIKE_MYSQL->database);
    PIKE_MYSQL->database = NULL;
  }
  if (PIKE_MYSQL->host) {
    free_string(PIKE_MYSQL->host);
    PIKE_MYSQL->host = NULL;
  }

  MYSQL_ALLOW();

  if (last_result) {
    mysql_free_result(last_result);
  }
  if (socket) {
    mysql_close(socket);
  }
  if (mysql) {
    free(mysql);
  }

  MYSQL_DISALLOW();

  DESTROY_MYSQL_LOCK();
}


static void pike_mysql_reconnect(void)
{
  MYSQL *mysql = PIKE_MYSQL->mysql;
  MYSQL *socket;
  char *host = NULL;
  char *database = NULL;
  char *user = NULL;
  char *password = NULL;
  char *hostptr = NULL;
  char *portptr = NULL;
  char *saved_unix_port = NULL;
  unsigned int port = 0;
  unsigned int saved_port = 0;

  if (PIKE_MYSQL->host) {
    hostptr = strdup(PIKE_MYSQL->host->str);
    if (!hostptr) {
      Pike_error("Mysql.mysql(): Out of memory!\n");
    }
    if ((portptr = strchr(hostptr, ':')) && (*portptr == ':')) {
      *portptr = 0;
      portptr++;
      port = (unsigned int) atoi(portptr);
    }
    if (*hostptr) {
      host = hostptr;
    }
  }
  if (PIKE_MYSQL->database) {
    database = PIKE_MYSQL->database->str;
  }
  if (PIKE_MYSQL->user) {
    user = PIKE_MYSQL->user->str;
  }
  if (PIKE_MYSQL->password) {
    password = PIKE_MYSQL->password->str;
  }

  if (!mysql) {
    mysql = PIKE_MYSQL->mysql = (MYSQL *)xalloc(sizeof(MYSQL));
  }

  socket = PIKE_MYSQL->socket;
  PIKE_MYSQL->socket = NULL;

  MYSQL_ALLOW();

#if defined(HAVE_MYSQL_PORT) || defined(HAVE_MYSQL_UNIX_PORT)
  STUPID_PORT_LOCK();
#endif /* HAVE_MYSQL_PORT || HAVE_MYSQL_UNIX_PORT */

  if (socket) {
    /* Disconnect the old connection */
    mysql_close(socket);
  }

#ifdef HAVE_MYSQL_PORT
  if (port) {
    saved_port = mysql_port;
    mysql_port = port;
  }
#endif /* HAVE_MYSQL_PORT */
#ifdef HAVE_MYSQL_UNIX_PORT
  if (portptr) {
    saved_unix_port = mysql_unix_port;
    mysql_unix_port = portptr;
  }
#endif /* HAVE_MYSQL_UNIX_PORT */

  socket = mysql_connect(mysql, host, user, password);

#ifdef HAVE_MYSQL_PORT
  if (port) {
    mysql_port = saved_port;
  }
#endif /* HAVE_MYSQL_PORT */
#ifdef HAVE_MYSQL_UNIX_PORT
  if (portptr) {
    mysql_unix_port = saved_unix_port;
  }
#endif /* HAVE_MYSQL_UNIX_PORT */
#if defined(HAVE_MYSQL_PORT) || defined(HAVE_MYSQL_UNIX_PORT)
  STUPID_PORT_UNLOCK();
#endif /* HAVE_MYSQL_PORT || MAVE_MYSQL_UNIX_PORT*/

  MYSQL_DISALLOW();

  if (hostptr) {
    /* No longer needed */
    free(hostptr);
  }
  
  if (!(PIKE_MYSQL->socket = socket)) {
    Pike_error("Mysql.mysql(): Couldn't reconnect to SQL-server\n"
	  "%s\n",
	  mysql_error(PIKE_MYSQL->mysql));
  }

  if (socket->net.fd >= 0) {
    /* Make sure the fd gets closed on exec. */
    set_close_on_exec(socket->net.fd, 1);
  }

  if (database) {
    int tmp;

    MYSQL_ALLOW();

    tmp = mysql_select_db(socket, database);

    MYSQL_DISALLOW();

    if (tmp < 0) {
      PIKE_MYSQL->socket = NULL;
      MYSQL_ALLOW();

      mysql_close(socket);

      MYSQL_DISALLOW();
      if (strlen(database) < 1024) {
	Pike_error("Mysql.mysql(): Couldn't select database \"%s\"\n", database);
      } else {
	Pike_error("Mysql.mysql(): Couldn't select database\n");
      }
    }
  }
}

/*
 * Methods
 */

/*
**! method void create()
**! method void create(string host)
**! method void create(string host, string database)
**! method void create(string host, string database, string user)
**! method void create(string host, string database, string user, string password)
**!
**!	Connects to a Mysql database.
**!
**!	To access the Mysql database, you must first connect to it. This is
**!	done with the function mysql().
**!
**!	If you give no argument, or give "" as hostname it will connect with
**!	a UNIX-domain socket, which is a big performance gain.
*/

/* void create(string|void host, string|void database, string|void user, string|void password) */
static void f_create(INT32 args)
{
  if (args >= 1) {
    if (sp[-args].type != T_STRING) {
      Pike_error("Bad argument 1 to mysql()\n");
    }
    if (sp[-args].u.string->len) {
      add_ref(PIKE_MYSQL->host = sp[-args].u.string);
    }

    if (args >= 2) {
      if (sp[1-args].type != T_STRING) {
	Pike_error("Bad argument 2 to mysql()\n");
      }
      if (sp[1-args].u.string->len) {
	add_ref(PIKE_MYSQL->database = sp[1-args].u.string);
      }

      if (args >= 3) {
	if (sp[2-args].type != T_STRING) {
	  Pike_error("Bad argument 3 to mysql()\n");
	}
	if (sp[2-args].u.string->len) {
	  add_ref(PIKE_MYSQL->user = sp[2-args].u.string);
	}

	if (args >= 4) {
	  if (sp[3-args].type != T_STRING) {
	    Pike_error("Bad argument 4 to mysql()\n");
	  }
	  if (sp[3-args].u.string->len) {
	    add_ref(PIKE_MYSQL->password = sp[3-args].u.string);
	  }
	}
      }
    }
  }

  pop_n_elems(args);

  pike_mysql_reconnect();
}

/*
**! method int affected_rows()
**!
**!	Returns the number of rows affected by the last query.
**!
*/
/* int affected_rows() */
static void f_affected_rows(INT32 args)
{
  MYSQL *socket;
  int count;

  if (!PIKE_MYSQL->socket) {
    pike_mysql_reconnect();
  }
  pop_n_elems(args);
  socket = PIKE_MYSQL->socket;

  MYSQL_ALLOW();
  count = mysql_affected_rows(socket);
  MYSQL_DISALLOW();

  push_int(count);
}

/*
**! method int insert_id()
**!
**!	Returns the id of the last INSERT query into a table with
**!	an AUTO INCREMENT field.
**!
*/
/* int insert_id() */
static void f_insert_id(INT32 args)
{
  MYSQL *socket;
  int id;

  if (!PIKE_MYSQL->socket) {
    pike_mysql_reconnect();
  }
  pop_n_elems(args);

  socket = PIKE_MYSQL->socket;

  MYSQL_ALLOW();
  id = mysql_insert_id(socket);
  MYSQL_DISALLOW();

  push_int(id);
}

/*
**! method string Pike_error()
**!
**!	Returns a string describing the last error from the Mysql-server.
**!
*/
/* int|string Pike_error() */
static void f_error(INT32 args)
{
  MYSQL *socket;
  char *error_msg;

  if (!PIKE_MYSQL->socket) {
    pike_mysql_reconnect();
  }

  socket = PIKE_MYSQL->socket;

  MYSQL_ALLOW();

  error_msg = mysql_error(socket);

  MYSQL_DISALLOW();

  pop_n_elems(args);

  if (error_msg && *error_msg) {
    push_text(error_msg);
  } else {
    push_int(0);
  }
}

/*
**! method void select_db(string database)
**!
**!	The Mysql-server can hold several databases. You select which one
**!	you want to access with this function.
**!
**! see also: create, create_db, drop_db
**!
*/
/* void select_db(string database) */
static void f_select_db(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  char *database;
  int tmp = -1;

  if (!args) {
    Pike_error("Too few arguments to mysql->select_db()\n");
  }
  if (sp[-args].type != T_STRING) {
    Pike_error("Bad argument 1 to mysql->select_db()\n");
  }

  database = sp[-args].u.string->str;

  if (socket) {
    MYSQL_ALLOW();

    tmp = mysql_select_db(socket, database);

    MYSQL_DISALLOW();
  }
  if (!socket || (tmp < 0)) {
    /* The connection might have been closed. */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();

    tmp = mysql_select_db(socket, database);

    MYSQL_DISALLOW();
  }

  if (tmp < 0) {
    char *err;

    MYSQL_ALLOW();
    err = mysql_error(socket);
    MYSQL_DISALLOW();

    Pike_error("mysql->select_db(): Couldn't select database \"%s\" (%s)\n",
	  sp[-args].u.string->str, err);
  }
  if (PIKE_MYSQL->database) {
    free_string(PIKE_MYSQL->database);
  }
  add_ref(PIKE_MYSQL->database = sp[-args].u.string);

  pop_n_elems(args);
}

/*
**! method object(Mysql.mysql_result) big_query(string q)
**!	Make an SQL query
**! arg string q
**!	The SQL query
**!
**!	This function sends an SQL query to the Mysql-server. The result
**!	of the query is returned as a Mysql.mysql_result object.
**!	Returns 0 if the query didn't return any result (e.g. INSERT or
**!	similar).
**!
**! see also: mysql_result
*/
/* object(mysql_result) big_query(string q) */
static void f_big_query(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  MYSQL_RES *result = NULL;
  char *query;
  int qlen;
  int tmp = -1;

  if (!args) {
    Pike_error("Too few arguments to mysql->big_query()\n");
  }
  if (sp[-args].type != T_STRING) {
    Pike_error("Bad argument 1 to mysql->big_query()\n");
  }

  query = sp[-args].u.string->str;
  qlen = sp[-args].u.string->len;

  if (socket) {
    MYSQL_ALLOW();

#ifdef HAVE_MYSQL_REAL_QUERY
    tmp = mysql_real_query(socket, query, qlen);
#else
    tmp = mysql_query(socket, query);
#endif /* HAVE_MYSQL_REAL_QUERY */

    if (tmp >= 0) {
      result = mysql_store_result(socket);
    }

    MYSQL_DISALLOW();
  }
  if (socket && (tmp < 0)) {
    /* Check if we need to reconnect. */
#if defined(CR_SERVER_GONE_ERROR) && defined(CR_UNKNOWN_ERROR)
    int eno = mysql_errno(socket);
    if ((eno == CR_SERVER_GONE_ERROR) ||
	(eno == CR_UNKNOWN_ERROR)) {
      socket = NULL;
    }
#else /* !CR_SERVER_GONE_ERROR || !CR_UNKNOWN_ERROR */
    socket = NULL;
#endif /* CR_SERVER_GONE_ERROR && CR_UNKNOWN_ERROR */
  }
  if (!socket) {
    /* The connection might have been closed. */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();

#ifdef HAVE_MYSQL_REAL_QUERY
    tmp = mysql_real_query(socket, query, qlen);
#else
    tmp = mysql_query(socket, query);
#endif /* HAVE_MYSQL_REAL_QUERY */

    if (tmp >= 0) {
      result = mysql_store_result(socket);
    }

    MYSQL_DISALLOW();
  }

  if (tmp < 0) {
    char *err;

    MYSQL_ALLOW();
    err = mysql_error(socket);
    MYSQL_DISALLOW();

    if (sp[-args].u.string->len <= 512) {
      Pike_error("mysql->big_query(): Query \"%s\" failed (%s)\n",
	    sp[-args].u.string->str, err);
    } else {
      Pike_error("mysql->big_query(): Query failed (%s)\n", err);
    }
  }

  pop_n_elems(args);

  if (!(PIKE_MYSQL->last_result = result)) {
    int err;

    MYSQL_ALLOW();
    err = (
#ifdef mysql_field_count
	   mysql_field_count(socket)
#else /* !mysql_field_count */
#ifdef mysql_num_fields
	   mysql_num_fields(socket)
#else /* !mysql_num_fields */
	   socket->field_count
#endif /* mysql_num_fields */
#endif /* mysql_field_count */
	   && mysql_error(socket)[0]);
    MYSQL_DISALLOW();

    if (err) {
      Pike_error("mysql->big_query(): Couldn't create result for query\n");
    }
    /* query was INSERT or similar - return 0 */

    push_int(0);
  } else {
    /* Return the result-object */

    ref_push_object(Pike_fp->current_object);

    push_object(clone_object(mysql_result_program, 1));
  }
}

/*
**! method void create_db(string database)
**!	Create a new database
**! arg string database
**!	Name of the database to be created
**!
**!	This function creates a new database in the Mysql-server.
**!
**! see also: select_db, drop_db
**!
*/
/* void create_db(string database) */
static void f_create_db(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  char *database;
  int tmp = -1;

  if (!args) {
    Pike_error("Too few arguments to mysql->create_db()\n");
  }
  if (sp[-args].type != T_STRING) {
    Pike_error("Bad argument 1 to mysql->create_db()\n");
  }
  if (sp[-args].u.string->len > 127) {
    if (sp[-args].u.string->len < 1024) {
      Pike_error("Database name \"%s\" is too long (max 127 characters)\n",
	    sp[-args].u.string->str);
    } else {
      Pike_error("Database name (length %d) is too long (max 127 characters)\n",
	    sp[-args].u.string->len);
    }
  }
  database = sp[-args].u.string->str;

  if (socket) {
    MYSQL_ALLOW();

    tmp = mysql_create_db(socket, database);

    MYSQL_DISALLOW();
  }
  if (!socket || (tmp < 0)) {
    /* The connection might have been closed */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();

    tmp = mysql_create_db(socket, database);

    MYSQL_DISALLOW();
  }

  if (tmp < 0) {
    Pike_error("mysql->create_db(): Creation of database \"%s\" failed\n",
	  sp[-args].u.string->str);
  }

  pop_n_elems(args);
}

/*
**! method void drop_db(string database)
**!	Drop a database
**! arg string database
**!	Name of database to be dropped
**!
**!	This function drops a database from a Mysql-server.
**!
**! see also: create_db, select_db
**!
*/
/* void drop_db(string database) */
static void f_drop_db(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  char *database;
  int tmp = -1;

  if (!args) {
    Pike_error("Too few arguments to mysql->drop_db()\n");
  }
  if (sp[-args].type != T_STRING) {
    Pike_error("Bad argument 1 to mysql->drop_db()\n");
  }
  if (sp[-args].u.string->len > 127) {
    if (sp[-args].u.string->len < 1024) {
      Pike_error("Database name \"%s\" is too long (max 127 characters)\n",
	    sp[-args].u.string->str);
    } else {
      Pike_error("Database name (length %d) is too long (max 127 characters)\n",
	    sp[-args].u.string->len);
    }
  }
  database = sp[-args].u.string->str;

  if (socket) {
    MYSQL_ALLOW();

    tmp = mysql_drop_db(socket, database);

    MYSQL_DISALLOW();
  }
  if (!socket || (tmp < 0)) {
    /* The connection might have been closed */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();

    tmp = mysql_drop_db(socket, database);

    MYSQL_DISALLOW();
  }    

  if (tmp < 0) {
    Pike_error("mysql->drop_db(): Drop of database \"%s\" failed\n",
	  sp[-args].u.string->str);
  }

  pop_n_elems(args);
}

/*
**! method void shutdown()
**!	Shutdown the Mysql-server
**!
**!	This function shuts down a running Mysql-server.
**!
**! see also:
**!	reload
**!
*/
/* void shutdown() */
static void f_shutdown(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  int tmp = -1;

  if (socket) {
    MYSQL_ALLOW();
  
    tmp = mysql_shutdown(socket);

    MYSQL_DISALLOW();
  }
  if (!socket || (tmp < 0)) {
    /* The connection might have been closed */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();
  
    tmp = mysql_shutdown(socket);

    MYSQL_DISALLOW();
  }

  if (tmp < 0) {
    Pike_error("mysql->shutdown(): Shutdown failed\n");
  }

  pop_n_elems(args);
}

/*
**! method void reload()
**!	Reload security tables
**!
**!	This function causes the Mysql-server to reload its access tables.
**!
**! see also: shutdown
**!
*/
/* void reload() */
static void f_reload(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  int tmp = -1;

  if (socket) {
    MYSQL_ALLOW();

    tmp = mysql_reload(socket);

    MYSQL_DISALLOW();
  }
  if (!socket || (tmp < 0)) {
    /* The connection might have been closed */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();

    tmp = mysql_reload(socket);

    MYSQL_DISALLOW();
  }

  if (tmp < 0) {
    Pike_error("mysql->reload(): Reload failed\n");
  }

  pop_n_elems(args);
}

/*
**! method string statistics()
**!	Some Mysql-server statistics
**!
**!	This function returns some server statistics.
**!
**! 	Example:
**!	<pre>
**!	int main()
**!	{
**!	  write(mysql()->statistics());
**!	  return(0);
**!	}
**!	</pre>
**! see also: server_info, host_info, protocol_info
**!
*/
/* string statistics() */
static void f_statistics(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  char *stats;

  if (!socket) {
    pike_mysql_reconnect();
    socket = PIKE_MYSQL->socket;
  }

  pop_n_elems(args);

  MYSQL_ALLOW();

  stats = mysql_stat(socket);

  MYSQL_DISALLOW();

  push_text(stats);
}

/*
**! method string server_info()
**!	Give the version number of the Mysql-server
**!
**!	This function returns the version number of the Mysql-server.
**!
**! see also: statistics, host_info, protocol_info
**!
*/
/* string server_info() */
static void f_server_info(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  char *info;

  if (!socket) {
    pike_mysql_reconnect();
    socket = PIKE_MYSQL->socket;
  }

  pop_n_elems(args);

  push_text("mysql/");

  MYSQL_ALLOW();

  info = mysql_get_server_info(socket);

  MYSQL_DISALLOW();

  push_text(info);
  f_add(2);
}

/*
**! method string host_info()
**!	Give information about the Mysql-server connection
**!
**!	This function returns a string describing the connection to
**!	the Mysql-server.
**!
**! see also: statistics, server_info, protocol_info
**!
*/
/* string host_info() */
static void f_host_info(INT32 args)
{
  MYSQL *socket;
  char *info;

  if (!PIKE_MYSQL->socket) {
    pike_mysql_reconnect();
  }

  socket = PIKE_MYSQL->socket;

  pop_n_elems(args);

  MYSQL_ALLOW();

  info = mysql_get_host_info(socket);

  MYSQL_DISALLOW();

  push_text(info);
}

/*
**! method int protocol_info()
**!	Give the Mysql protocol version
**!
**!	This function returns the version number of the protocol the
**!	Mysql-server uses.
**!
**! see also: statistics, server_info, host_info
**!
*/
/* int protocol_info() */
static void f_protocol_info(INT32 args)
{
  MYSQL *socket;
  int prot;

  if (!PIKE_MYSQL->socket) {
    pike_mysql_reconnect();
  }

  pop_n_elems(args);

  socket = PIKE_MYSQL->socket;

  MYSQL_ALLOW();
  prot = mysql_get_proto_info(socket);
  MYSQL_DISALLOW();

  push_int(prot);
}

/*
**! method object(Mysql.mysql_result) list_dbs()
**! method object(Mysql.mysql_result) list_dbs(string wild)
**!	List databases
**!
**!	Returns a table containing the names of all databases in the
**!	Mysql-server. If an argument is specified, only those matching
**!	wild are returned.
**!
**! see also: list_tables, list_fields, list_processes, Mysql.mysql_result
**!
*/
/* object(mysql_res) list_dbs(void|string wild) */
static void f_list_dbs(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  MYSQL_RES *result = NULL;
  char *wild = NULL;

  if (args) {
    if (sp[-args].type != T_STRING) {
      Pike_error("Bad argument 1 to mysql->list_dbs()\n");
    }
    if (sp[-args].u.string->len > 80) {
      if (sp[-args].u.string->len < 1024) {
	Pike_error("Wildcard \"%s\" is too long (max 80 characters)\n",
	      sp[-args].u.string->str);
      } else {
	Pike_error("Wildcard (length %d) is too long (max 80 characters)\n",
	      sp[-args].u.string->len);
      }
    }
    wild = sp[-args].u.string->str;
  }

  if (socket) {
    MYSQL_ALLOW();

    result = mysql_list_dbs(socket, wild);

    MYSQL_DISALLOW();
  }
  if (!socket || !result) {
    /* The connection might have been closed */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();

    result = mysql_list_dbs(socket, wild);

    MYSQL_DISALLOW();
  }

  if (!(PIKE_MYSQL->last_result = result)) {
    char *err;

    MYSQL_ALLOW();

    err = mysql_error(socket);

    MYSQL_DISALLOW();

    Pike_error("mysql->list_dbs(): Cannot list databases: %s\n", err);
  }

  pop_n_elems(args);

  ref_push_object(Pike_fp->current_object);

  push_object(clone_object(mysql_result_program, 1));
}

/*
**! method object(Mysql.mysql_result) list_tables()
**! method object(Mysql.mysql_result) list_tables(string wild)
**!	List tables in the current database
**! arg string wild
**!	Wildcard to filter with.
**!
**!	Returns a table containing the names of all tables in the current
**!	database. If an argument is given, only those matching wild are
**!	returned.
**!
**! see also: list_dbs, list_fields, list_processes, Mysql.mysql_result
**!
*/
/* object(mysql_res) list_tables(void|string wild) */
static void f_list_tables(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  MYSQL_RES *result = NULL;
  char *wild = NULL;

  if (args) {
    if (sp[-args].type != T_STRING) {
      Pike_error("Bad argument 1 to mysql->list_tables()\n");
    }
    if (sp[-args].u.string->len > 80) {
      if (sp[-args].u.string->len < 1024) {
	Pike_error("Wildcard \"%s\" is too long (max 80 characters)\n",
	      sp[-args].u.string->str);
      } else {
	Pike_error("Wildcard (length %d) is too long (max 80 characters)\n",
	      sp[-args].u.string->len);
      }
    }
    wild = sp[-args].u.string->str;
  }

  if (socket) {
    MYSQL_ALLOW();

    result = mysql_list_tables(socket, wild);

    MYSQL_DISALLOW();
  }
  if (!socket || !result) {
    /* The connection might have been closed */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();

    result = mysql_list_tables(socket, wild);

    MYSQL_DISALLOW();
  }

  if (!(PIKE_MYSQL->last_result = result)) {
    char *err;

    MYSQL_ALLOW();

    err =  mysql_error(socket);

    MYSQL_DISALLOW();

    Pike_error("mysql->list_tables(): Cannot list databases: %s\n", err);
  }

  pop_n_elems(args);

  ref_push_object(Pike_fp->current_object);

  push_object(clone_object(mysql_result_program, 1));
}

/*
**! method array(int|mapping(string:mixed)) list_fields(string table)
**! method array(int|mapping(string:mixed)) list_fields(string table, string wild)
**!	List all fields.
**!
**!	Returns an array of mappings with information about the fields in the
**!	specified table.
**!
**!	The mappings contain the following entries:
**!	<pre>
**!	 "name":	string	The name of the field.
**!	 "table":	string	The name of the table.
**!	 "default":	string	The default value for the field.
**!	 "type":	string	The type of the field.
**!	 "length":	int	The length of the field.
**!	 "max_length":	int	The length of the longest element in this field.
**!	 "flags":	multiset(string)	Some flags.
**!	 "decimals":	int	The number of decimalplaces.
**!	</pre>
**!
**!	The type of the field can be any of:
**!	"decimal", "char", "short", "long", "float", "double", "null",
**!	"time", "longlong", "int24", "tiny blob", "medium blob",
**!	"long blob", "var string", "string" or "unknown".
**!
**!	The flags multiset can contain any of
**!	 "primary_key":	This field is part of the primary key for this table.
**!	 "not_null":	This field may not be NULL.
**!	 "blob":	This field is a blob field.
**!
**! arg string table
**!	Name of table to list the fields of
**! arg string wild
**!	Wildcard to filter the result with
**!
**! note
**!	Michael Widenius recomends usage of the following query instead:
**!	<pre>
**!	 show fields in 'table' like "wild"
**!	</pre>
**!
**! see also: list_dbs, list_tables, list_processes, fetch_fields
**!
*/
/* array(int|mapping(string:mixed)) list_fields(string table, void|string wild) */
static void f_list_fields(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  MYSQL_RES *result = NULL;
  MYSQL_FIELD *field;
  int i = 0;
  char *table;
  char *wild = NULL;

  if (!args) {
    Pike_error("Too few arguments to mysql->list_fields()\n");
  }
  
  if (sp[-args].type != T_STRING) {
    Pike_error("Bad argument 1 to mysql->list_fields()\n");
  }
  if (sp[-args].u.string->len > 125) {
    if (sp[-args].u.string->len < 1024) {
      Pike_error("Table name \"%s\" is too long (max 125 characters)\n",
	    sp[-args].u.string->str);
    } else {
      Pike_error("Table name (length %d) is too long (max 125 characters)\n",
	    sp[-args].u.string->len);
    }
  }
  table = sp[-args].u.string->str;
  if (args > 1) {
    if (sp[-args+1].type != T_STRING) {
      Pike_error("Bad argument 2 to mysql->list_fields()\n");
    }
    if (sp[-args+1].u.string->len + sp[-args].u.string->len > 125) {
      /* The length of the table name has already been checked. */
      if (sp[-args+1].u.string->len < 1024) {
	Pike_error("Wildcard \"%s\" + table name \"%s\" is too long "
	      "(max 125 characters)\n",
	      sp[-args+1].u.string->str, sp[-args].u.string->str);
      } else {
	Pike_error("Wildcard (length %d) + table name \"%s\" is too long "
	      "(max 125 characters)\n",
	      sp[-args+1].u.string->len, sp[-args].u.string->str);
      }
    }
    wild = sp[-args+1].u.string->str;
  }

  if (socket) {
    MYSQL_ALLOW();

    result = mysql_list_fields(socket, table, wild);

    MYSQL_DISALLOW();
  }
  if (!socket || !result) {
    /* The connection might have been closed */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();

    result = mysql_list_fields(socket, table, wild);

    MYSQL_DISALLOW();
  }

  if (!result) {
    char *err;

    MYSQL_ALLOW();

    err = mysql_error(socket);

    MYSQL_DISALLOW();

    Pike_error("mysql->list_fields(): Cannot list databases: %s\n", err);
  }

  pop_n_elems(args);

  /* FIXME: Should have MYSQL_{DIS,}ALLOW() here */

  while ((field = mysql_fetch_field(result))) {
    mysqlmod_parse_field(field, 1);
    i++;
  }
  f_aggregate(i);
}

/*
**! method object(Mysql.mysql_result) list_processes()
**!	List all processes in the Mysql-server
**!
**!	Returns a table containing the names of all processes in the
**!	Mysql-server.
**!
**! see also: list_dbs, list_tables, list_fields, Mysql.mysql_result
**!
*/
/* object(mysql_res) list_processes() */
static void f_list_processes(INT32 args)
{
  MYSQL *socket = PIKE_MYSQL->socket;
  MYSQL_RES *result = NULL;

  pop_n_elems(args);

  if (socket) {
    MYSQL_ALLOW();

    result = mysql_list_processes(socket);

    MYSQL_DISALLOW();
  }
  if (!socket || !result) {
    /* The connection might have been closed */
    pike_mysql_reconnect();

    socket = PIKE_MYSQL->socket;

    MYSQL_ALLOW();

    result = mysql_list_processes(socket);

    MYSQL_DISALLOW();
  }

  if (!(PIKE_MYSQL->last_result = result)) {
    char *err;

    MYSQL_ALLOW();

    err = mysql_error(socket);

    MYSQL_DISALLOW();

    Pike_error("mysql->list_processes(): Cannot list databases: %s\n", err);
  }

  ref_push_object(Pike_fp->current_object);

  push_object(clone_object(mysql_result_program, 1));
}

/*
**! method int binary_data()
**!	Inform if this version of mysql supports binary data
**!
**!	This function returns non-zero if binary data can be reliably stored
**!	and retreived with this version of the mysql-module.
**!
**!	Usually, there is no problem storing binary data in mysql-tables,
**!	but data containing '\0' (NUL) couldn't be fetched with old
**!	versions (prior to 3.20.5) of the mysql-library.
**!
*/
/*
 * Support for binary data in tables
 */
static void f_binary_data(INT32 args)
{
  pop_n_elems(args);
#ifdef HAVE_MYSQL_FETCH_LENGTHS
  push_int(1);
#else
  push_int(0);
#endif /* HAVE_MYSQL_FETCH_LENGTHS */
}

#endif /* HAVE_MYSQL */

/*
 * Module linkage
 */


void pike_module_init(void)
{
#ifdef HAVE_MYSQL
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
  ADD_STORAGE(struct precompiled_mysql);

  /* function(void:int|string) */
  ADD_FUNCTION("error", f_error,tFunc(tVoid,tOr(tInt,tStr)), ID_PUBLIC);
  /* function(string|void, string|void, string|void, string|void:void) */
  ADD_FUNCTION("create", f_create,tFunc(tOr(tStr,tVoid) tOr(tStr,tVoid) tOr(tStr,tVoid) tOr(tStr,tVoid),tVoid), ID_PUBLIC);
  /* function(void:int) */
  ADD_FUNCTION("affected_rows", f_affected_rows,tFunc(tVoid,tInt), ID_PUBLIC);
  /* function(void:int) */
  ADD_FUNCTION("insert_id", f_insert_id,tFunc(tVoid,tInt), ID_PUBLIC);
  /* function(string:void) */
  ADD_FUNCTION("select_db", f_select_db,tFunc(tStr,tVoid), ID_PUBLIC);
  /* function(string:int|object) */
  ADD_FUNCTION("big_query", f_big_query,tFunc(tStr,tOr(tInt,tObj)), ID_PUBLIC);
  /* function(string:void) */
  ADD_FUNCTION("create_db", f_create_db,tFunc(tStr,tVoid), ID_PUBLIC);
  /* function(string:void) */
  ADD_FUNCTION("drop_db", f_drop_db,tFunc(tStr,tVoid), ID_PUBLIC);
  /* function(void:void) */
  ADD_FUNCTION("shutdown", f_shutdown,tFunc(tVoid,tVoid), ID_PUBLIC);
  /* function(void:void) */
  ADD_FUNCTION("reload", f_reload,tFunc(tVoid,tVoid), ID_PUBLIC);
  /* function(void:string) */
  ADD_FUNCTION("statistics", f_statistics,tFunc(tVoid,tStr), ID_PUBLIC);
  /* function(void:string) */
  ADD_FUNCTION("server_info", f_server_info,tFunc(tVoid,tStr), ID_PUBLIC);
  /* function(void:string) */
  ADD_FUNCTION("host_info", f_host_info,tFunc(tVoid,tStr), ID_PUBLIC);
  /* function(void:int) */
  ADD_FUNCTION("protocol_info", f_protocol_info,tFunc(tVoid,tInt), ID_PUBLIC);
  /* function(void|string:object) */
  ADD_FUNCTION("list_dbs", f_list_dbs,tFunc(tOr(tVoid,tStr),tObj), ID_PUBLIC);
  /* function(void|string:object) */
  ADD_FUNCTION("list_tables", f_list_tables,tFunc(tOr(tVoid,tStr),tObj), ID_PUBLIC);
  /* function(string, void|string:array(int|mapping(string:mixed))) */
  ADD_FUNCTION("list_fields", f_list_fields,tFunc(tStr tOr(tVoid,tStr),tArr(tOr(tInt,tMap(tStr,tMix)))), ID_PUBLIC);
  /* function(void|string:object) */
  ADD_FUNCTION("list_processes", f_list_processes,tFunc(tOr(tVoid,tStr),tObj), ID_PUBLIC);

  /* function(void:int) */
  ADD_FUNCTION("binary_data", f_binary_data,tFunc(tVoid,tInt), ID_PUBLIC);

  set_init_callback(init_mysql_struct);
  set_exit_callback(exit_mysql_struct);

  mysql_program = end_program();
  add_program_constant("mysql", mysql_program, 0);

#ifdef HAVE_MYSQL_PORT
  STUPID_PORT_INIT();
#endif /* HAVE_MYSQL_PORT */

  init_mysql_res_programs();
#endif /* HAVE_MYSQL */
}

void pike_module_exit(void)
{
#ifdef HAVE_MYSQL
  exit_mysql_res();

#ifdef HAVE_MYSQL_PORT
  STUPID_PORT_DESTROY();
#endif /* HAVE_MYSQL_PORT */

  if (mysql_program) {
    free_program(mysql_program);
    mysql_program = NULL;
  }
#endif /* HAVE_MYSQL */
}

