/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

/*
 * Postgres95 and PostgreSQL support for Pike 0.5 and up.
 */

#define NO_PIKE_SHORTHAND

#include "global.h"
#include "pgres_config.h"

/* Some versions of Postgres define this, and it conflicts with pike_error.h */
#undef JMP_BUF

#ifdef HAVE_POSTGRES

#include "version.h"

/* #define PGDEBUG */

/* System includes */
#include <stdlib.h>
#include <stdio.h> /* Needed or libpq-fe.h pukes :( */
#include <string.h>

/* Pike includes */
#include "svalue.h"
#include "las.h"
#include "machine.h"
#include "pike_memory.h"
#include "svalue.h"
#include "threads.h"
#include "stralloc.h"
#include "object.h"
#include "module_support.h"
#include "operators.h"

#include "pgresult.h"

#define THIS ((struct pgres_object_data *) (Pike_fp->current_storage))

/* Actual code */
#ifdef PIKE_THREADS
# ifdef PQ_THREADSAFE
#  define PQ_FETCH() PIKE_MUTEX_T *pg_mutex = &THIS->mutex;
#  define PQ_LOCK() mt_lock(pg_mutex)
#  define PQ_UNLOCK() mt_unlock(pg_mutex)
# else
PIKE_MUTEX_T pike_postgres_mutex STATIC_MUTEX_INIT;
#  define PQ_FETCH()
#  define PQ_LOCK() mt_lock(&pike_postgres_mutex)
#  define PQ_UNLOCK() mt_unlock(&pike_postgres_mutex)
# endif
#else
# define PQ_FETCH()
# define PQ_LOCK()
# define PQ_UNLOCK()
#endif

#include "pg_types.h"


#ifdef PGDEBUG
#define pgdebug printf
#else
static void pgdebug (char * a, ...) {}
#endif

struct program * postgres_program;

static void set_error (char * newerror)
{
	pgdebug("set_error(%s).\n",newerror);
	if (THIS->last_error)
		free_string(THIS->last_error);
	THIS->last_error=make_shared_string(newerror);
	return;
}

static void pgres_create (struct object * o) {
	pgdebug ("pgres_create().\n");
	THIS->dblink=NULL;
	THIS->last_error=NULL;
	mark_free_svalue (&THIS->notify_callback);
	THIS->docommit=0;
	THIS->dofetch=0;
	THIS->lastcommit=0;
        THIS->last_rows=-1;
#if defined(PIKE_THREADS) && defined(PQ_THREADSAFE)
	mt_init(&THIS->mutex);
#endif

}

static void pgres_destroy (struct object * o)
{
	PGconn * conn;
	PQ_FETCH();
	pgdebug ("pgres_destroy().\n");
	if ((conn=THIS->dblink)) {
		THREADS_ALLOW();
		PQ_LOCK();
		PQfinish(conn);
		PQ_UNLOCK();
		THREADS_DISALLOW();
		THIS->dblink=NULL;
	}
	if(THIS->last_error) {
		if (THIS->last_error)
			free_string(THIS->last_error);
		THIS->last_error=NULL;
	}
	if (THIS->notify_callback.type!=PIKE_T_FREE) {
		free_svalue(&THIS->notify_callback);
		mark_free_svalue(&THIS->notify_callback);
	}
#if defined(PIKE_THREADS) && defined(PQ_THREADSAFE)
	mt_destroy(&THIS->mutex);
#endif
}

/*! @module Postgres
 *!
 *! @class postgres
 *!
 *! This is an interface to the Postgres database server using libpq.
 *! This module may or may not be available on your Pike, depending
 *! whether the appropriate include and library files 
 *! could be found at compile-time. Note that you @b{do not@}
 *! need to have a Postgres server running on your host to use this module:
 *! you can connect to the database over a TCP/IP socket.
 *!
 *! Please notice that unless you wish to specifically connect to a Postgres
 *! server, you'd better use the @[Sql.Sql], which is a server-independent
 *! sql-server-class. The interfaces to all existing sql-classes
 *! are consistent. Using @[Sql.Sql] ensures that your Pike
 *! applications will run with any supported SQL server without changing
 *! a single line of code, at least for most common (and simple) operations.
 *!
 *! The program @[Postgres.postgres] provides the @b{raw@} interface
 *! to the database. Many functions are @b{not@} available
 *! for this program. Therefore, its use is DEPRECATED.
 *! It is included in this documentation only for completeness' sake.
 *! Use @[Sql.postgres] instead, or even better @[Sql.Sql]
 *!
 *! @note
 *! This driver is based on libpq and is DEPRECATED.  There is a newer driver
 *! called @ref{Sql.pgsql@} which is faster and more robust than this driver
 *! and does not depend on any libraries.
 *!
 *! @note
 *! There is no testsuite for this module, since to test anything would
 *! require a working Postgres server. You can try to use the included scripts
 *! in the "pike/src/modules/Postgres/extras" directory but you'll probably
 *! have to patch them to reflect your site's settings.
 *!
 *! The behavior of the Postgres C API also depends on certain environment variables
 *! defined in the environment of the pike interpreter.
 *!
 *! @string
 *!   @value "PGHOST"
 *!     Sets the name of the default host to connect to. It defaults
 *! 	to "localhost"
 *!   @value "PGOPTIONS"
 *!     Sets some extra flags for the frontend-backend connection.
 *!     DO NOT SET unless you're sure of what you're doing.
 *!   @value "PGPORT"
 *!     Sets the default port to connect to, otherwise it will use
 *!     compile-time defaults (that is: the time you compiled the postgres
 *! 	library, not the Pike driver).
 *!   @value "PGTTY"
 *!     Sets the file to be used for Postgres frontend debugging.
 *!     Do not use, unless you're sure of what you're doing.
 *!   @value "PGDATABASE"
 *!     Sets the default database to connect to.
 *!   @value "PGREALM"
 *!     Sets the default realm for Kerberos authentication. I never used
 *!   	this, so I can't help you.
 *! @endstring
 *!
 *! Refer to the libpq documentation for further details.
 *!
 *! @seealso
 *!   @[Sql.Sql], @[Sql.postgres], @[Sql.postgres_result]
 */

/*! @decl string version
 *!
 *! Should you need to report a bug to the author, please submit along with
 *! the report the driver version number, as returned by this call.
 */

/*! @decl void create()
 *! @decl void create(string host, void|string database, void|int port)
 *!
 *! With no arguments, this function initializes (reinitializes if a connection
 *! had been previously set up) a connection to the Postgres backend.
 *! Since Postgres requires a database to be selected, it will try
 *! to connect to the default database. The connection may fail however for a 
 *! variety of reasons, in this case the most likely of all is because
 *! you don't have enough authority to connect to that database.
 *! So use of this particular syntax is discouraged.
 *!
 *! The host argument allows you to connect to databases residing on different
 *! hosts. If it is 0 or "", it will try to connect to localhost.
 *!
 *! The database argument specifies the database to connect to. If 0 or "", it
 *! will try to connect to the default database.
 *!
 *! @note
 *! You need to have a database selected before using the sql-object, 
 *! otherwise you'll get exceptions when you try to query it.
 *! Also notice that this function @b{can@} raise exceptions if the db
 *! server doesn't respond, if the database doesn't exist or is not accessible
 *! by you.
 *!
 *! You don't need bothering about syncronizing the connection to the database:
 *! it is automatically closed (and the database is sync-ed) when the
 *! object is destroyed.
 *!
 *! @seealso
 *!  	@[Sql.postgres], @[Sql.Sql], @[select_db]
 */

/* create (host,database,username,password,port) */
static void f_create (INT32 args)
{
        /*port will be used as a string with a sprintf()*/
	char * host=NULL, *db=NULL, *user=NULL, *pass=NULL, *port=NULL;
	struct mapping *options = NULL;
	int port_no = -1;
	char port_buffer[10]; /*it's enough, we need only 6*/
	PGconn * conn;
	PQ_FETCH();

	get_all_args("postgres->create",args,
		     ".%s%s%s%s%d%G",
		     &host,
		     &db,
		     &user,
		     &pass,
		     &port_no,
		     &options);

	if (port_no > 0) {
	  if (port_no < 65536) {
	    sprintf(port = port_buffer, "%d", port_no);
	  } else {
	    SIMPLE_ARG_TYPE_ERROR("create", 5, "int(0..65535)");
	  }
	}

	conn=THIS->dblink;
	THIS->dblink = NULL;

	/* FIXME: Race-condition! */

	THREADS_ALLOW();
	PQ_LOCK();
	if (conn) {
		PQfinish(conn);
	}
#ifdef HAVE_PQSETDBLOGIN
	conn=PQsetdbLogin(host,port,NULL,NULL,db,user,pass);
#else
	conn=PQsetdb(host,port,NULL,NULL,db);
#endif
	PQ_UNLOCK();
	THREADS_DISALLOW();

	if (!conn)
		Pike_error ("Could not conneect to server\n");
	if (PQstatus(conn)!=CONNECTION_OK) {
		set_error(PQerrorMessage(conn));
		THREADS_ALLOW();
		PQ_LOCK();
		PQfinish(conn);
		PQ_UNLOCK();
		THREADS_DISALLOW();
		Pike_error("Could not connect to database. Reason: \"%S\".\n",
			   THIS->last_error);
		conn=NULL;
	}
	THIS->dblink=conn;
	pop_n_elems(args);
}


/*! @decl void select_db (string dbname)
 *!
 *! This function allows you to connect to a database. Due to restrictions
 *! of the Postgres frontend-backend protocol, you always have to be connected
 *! to a database, so in fact this function just allows you to connect
 *! to a different database on the same server.
 *!
 *! @note
 *! This function @b{can@} raise exceptions if something goes wrong (backend process
 *! not running, not enough permissions..)
 *!
 *! @seealso
 *!   create
 */

static void f_select_db (INT32 args)
{
	char *host, *port, *options, *tty, *db;
	PGconn * conn, *newconn;
	PQ_FETCH();

	get_all_args("Postgres->select_db",args,"%s", &db);
	
	if (!THIS->dblink)
	  Pike_error ("Driver error. How can you possibly not be linked to a "
		      "database already?\n");
	conn=THIS->dblink;
#if 0
	/* This is an optimization, but people may want to reset a connection
	 * by re-selecting its database.
	 */
	if (!strcmp(db, PQdb(conn))) {
		pop_n_elems(args);
		return;
	}
#endif
	THREADS_ALLOW();
	PQ_LOCK();
	host=PQhost(conn);
	port=PQport(conn);
	options=PQoptions(conn);
	tty=PQtty(conn);
	/* This could be really done calling f_create, but it's more efficient this
	 * way */
	/* using newconn is necessary or otherwise the datastructures I use
	 * as arguments get freed by PQfinish. Could be a problem under extreme
	 * situations (i.e. if the temporary use of _one_ more filedescriptor
	 * is not possible.
	 */
	newconn=PQsetdb(host,port,options,tty,db);
	PQfinish(conn);
	conn=newconn;
	PQ_UNLOCK();
	THREADS_DISALLOW();
	if (PQstatus(conn)==CONNECTION_BAD) {
		set_error(PQerrorMessage(conn));
		PQ_LOCK();
		PQfinish(conn);
		PQ_UNLOCK();
		Pike_error("Could not connect to database.\n");
		conn=NULL;
	}
	THIS->dblink=conn;
	pop_n_elems(args);
}

/*! @decl object(Sql.postgres_result) big_query (string sqlquery)
 *!
 *! This is the only provided interface which allows you to query the
 *! database. If you wish to use the simpler "query" function, you need to
 *! use the @[Sql.Sql] generic sql-object.
 *!
 *! It returns a postgres_result object (which conforms to the @[Sql.sql_result]
 *! standard interface for accessing data). I recommend using query() for
 *! simpler queries (because it is easier to handle, but stores all the result
 *! in memory), and big_query for queries you expect to return huge amounts
 *! of data (it's harder to handle, but fectches results on demand).
 *!
 *! @note
 *! This function @b{can@} raise exceptions.
 *!
 *! The program @[Sql.postgres_result] is a superset of
 *! the raw @[Postgres.postgres_result] which does not do chunking to
 *! avoid excessive memory usage in case of large resultsets.
 *!
 *! @seealso
 *!  @[Sql.Sql], @[Sql.sql_result]
 */

static void f_big_query(INT32 args)
{
	PGconn *conn = THIS->dblink;
	PGresult * res;
	PGnotify * notification;
        struct array *bnds = 0;
	char *query = 0;
	int lastcommit,docommit,dofetch;
        int cnt;
        int nParams = 0;
        const char** paramValues = 0;
        int* paramLengths = 0;
        int* paramFormats = 0;
        int resultFormat = 0;
        
	PQ_FETCH();
	docommit=dofetch=0;
	lastcommit=THIS->lastcommit;

	check_all_args("Postgres->big_query",args,
	  BIT_STRING,
	  BIT_VOID | BIT_ARRAY,
	  0);

	if (!conn)
		Pike_error ("Not connected.\n");

        switch(args)
        {
          default:
            if(Pike_sp[1-args].type == PIKE_T_ARRAY)
              bnds=Pike_sp[1-args].u.array;
      
          case 1:
	    query=" ";
	    if(Pike_sp[-args].u.string->len)
		query=Pike_sp[-args].u.string->str;
        }

        if(bnds && (cnt=bnds->size)) {
          int i;
	  struct svalue *item;

	  nParams=cnt;
	  
	  paramValues = xalloc(nParams*sizeof*paramValues);
	  paramLengths = xalloc(nParams*sizeof*paramLengths);
	  paramFormats = xalloc(nParams*sizeof*paramFormats);

	  for (i=0,item=bnds->item; cnt--; item++,i++) {
	    int sendbin=0;
	    switch(item->type)
	    { case PIKE_T_STRING: {
	          long len;
		  const char*p;
		  paramLengths[i] = len = item->u.string->len;
	          paramValues[i] = p = item->u.string->str;
	          if( len >= BINARYCUTOFF)
	            sendbin = 1;
	          else {
	            while(len--) {
		      switch(*p++) {
			default:
			  continue;
			case '\0':case '\\':
			  sendbin = 1;
		      }
		      break;
		    }
	          }
	        break;
	      }
	      case PIKE_T_INT:
	      case T_VOID:
	          paramValues[i] = 0;	     /* NULL */
		  paramLengths[i] = 0;
	        break;
	      default:
                  Pike_error("Expected string or UNDEFINED element, Got %d.\n",
		   item->type);
	        break;
	    }
	    paramFormats[i] = sendbin;
	  }
        }

	THREADS_ALLOW();
	PQ_LOCK();
	pgdebug("f_big_query(\"%s\")\n",query);
        /*
         * libpq implements partial reads from a SELECT through
         * a mechanism of a CURSOR incombination with FETCH.
         *
         * Cursors are only used if the query starts with "SELECT "
         * and does not end in "LIMIT 1" or "LIMIT 1;"
         * The checks are case-sensitive by design.
         *
         * Cursors can only be used within a transaction, so you'll end
         * up trying to start a transaction; if a transaction already is
         * in progress, that will be noticed and it will not be disrupted.
         */
#define SELECTSTR	"SELECT "
#define LIMIT1STR	"LIMIT 1"
#define LIMIT1STRSC	LIMIT1STR";"
#define LIMITLENSC	(sizeof(LIMIT1STRSC)-1)
#define LIMITLEN	(sizeof(LIMIT1STR)-1)
	res = 0;
	if(!strncmp(query,SELECTSTR,sizeof(SELECTSTR)-1))
	{
#define CURSORPREFIX	"DECLARE "CURSORNAME" CURSOR FOR "
#define CPREFLEN	sizeof(CURSORPREFIX)
	  unsigned int qlen=strlen(query);
	  char *nquery;
	  if(qlen>LIMITLENSC && strcmp(query+qlen-LIMITLENSC,LIMIT1STRSC)
	     && strcmp(query+qlen-LIMITLEN,LIMIT1STR)
	     &&(nquery=malloc(CPREFLEN+qlen))) {
	    strcpy(nquery,CURSORPREFIX);
	    strcpy(nquery+CPREFLEN-1,query);
	    if(lastcommit)
	      goto yupbegin;
	    res=PQexecParams(conn,nquery,
             nParams,0,paramValues,paramLengths,paramFormats,resultFormat);
	    if(PQstatus(conn) != CONNECTION_OK) {
	      PQclear(res);
	      PQreset(conn);
	      res=PQexecParams(conn,nquery,
               nParams,0,paramValues,paramLengths,paramFormats,resultFormat);
	    }
	    if(res)
	      switch(PQresultStatus(res)) {
	      case PGRES_FATAL_ERROR:
		PQclear(res);
yupbegin:       res=PQexec(conn,"BEGIN");
		if(res && PQresultStatus(res)==PGRES_COMMAND_OK) {
		  PQclear(res);
	          res=PQexecParams(conn,nquery,nParams,0,
		   paramValues,paramLengths,paramFormats,resultFormat);
		  if(res && PQresultStatus(res)==PGRES_COMMAND_OK)
		    docommit=1;
		  else {
		    PQclear(res);
		    res=PQexec(conn,"COMMIT");
		    PQclear(res);
		    res=0;
		    break;
		  }
		} else
		  break;
	      case PGRES_COMMAND_OK:
		dofetch=1;
		PQclear(res);
		res=PQexec(conn,FETCHCMD);
		break;
	      default:
		PQclear(res);
		res=0;
	      }
	    free(nquery);
	  }
	}
	lastcommit=0;
	if(!res)
	  res=PQexecParams(conn,query,
           nParams,0,paramValues,paramLengths,paramFormats,resultFormat);
	/* A dirty hack to fix the reconnect bug.
	 * we don't need to store the host/user/pass/db... etc..
	 * PQreset() does all the job.
	 *  Zsolt Varga <redax@agria.hu> 2000-apr-04
	 */
	if((PQstatus(conn) != CONNECTION_OK) ||
	   (PQresultStatus(res) == PGRES_FATAL_ERROR) ||
	   (PQresultStatus(res) == PGRES_BAD_RESPONSE)) {
	  PQclear(res);
	  PQreset(conn);
	  res=PQexecParams(conn,query,
           nParams,0,paramValues,paramLengths,paramFormats,resultFormat);
	}

	notification=PQnotifies(conn);
	PQ_UNLOCK();
	THREADS_DISALLOW();
	THIS->docommit=docommit;
	THIS->dofetch=dofetch;
	THIS->lastcommit=lastcommit;

        if (bnds) {
	  xfree(paramValues); xfree(paramLengths); xfree(paramFormats);
        }

	pop_n_elems(args);
	if (notification!=NULL) {
		pgdebug("Incoming notification: \"%s\"\n",notification->relname);
		push_text(notification->relname);
		apply_svalue(&THIS->notify_callback,1); 
		/* apply_svalue simply returns if the first argument is a PIKE_T_INT */
		free (notification);
	}
	if (!res) {
		set_error(PQerrorMessage(conn));
		if (!strncmp(THIS->last_error->str,"WARN",4)) {
			/* Sigh... woldn't a NONFATAL_ERROR be MUCH better? */
			push_int(1);
			return;
		}
		Pike_error ("Error in query.\n");
	}

	switch (PQresultStatus(res))
	{
		case PGRES_EMPTY_QUERY:
		case PGRES_COMMAND_OK:
			pgdebug("\tOk.\n");
			THIS->last_result=NULL;
                        THIS->last_rows=atoi(PQcmdTuples(res));
			PQclear(res);
			push_int(0);
			return;
		case PGRES_NONFATAL_ERROR:
			pgdebug ("Warning.\n");
			set_error(PQerrorMessage(conn));
			push_int(1);
			return;
		case PGRES_BAD_RESPONSE:
		case PGRES_FATAL_ERROR:
			pgdebug("\tBad.\n");
			set_error(PQerrorMessage(conn));
			PQclear(res);
			Pike_error ("Error in frontend-backend communications.\n");
		case PGRES_TUPLES_OK:
			pgdebug("\tResult.\n");
			THIS->last_result=res;
                        THIS->last_rows=PQntuples(res);
			push_object(this_object());
			push_object(clone_object(pgresult_program,1));
			return;
		default:
			Pike_error ("Unimplemented server feature.\n");
	}
	Pike_error ("Internal error in postgres module.\n");
}


/*! @decl string error()
 *!
 *! This function returns the textual description of the last server-related
 *! error. Returns 0 if no error has occurred yet. It is not cleared upon
 *! reading (can be invoked multiple times, will return the same result
 *! until a new error occurs).
 *!
 *! @seealso
 *!   big_query
 */

static void f_error (INT32 args)
{
        pop_n_elems(args);

	if (THIS->last_error)
		ref_push_string(THIS->last_error);
	else
		push_int(0);
	return;
}


/*! @decl void reset()
 *!
 *! This function resets the connection to the backend. Can be used for
 *! a variety of reasons, for example to detect the status of a connection.
 *!
 *! @note
 *! This function is Postgres-specific, and thus it is not available
 *! through the generic SQL-interface.
 */

static void f_reset (INT32 args)
{
	PGconn * conn;
	PQ_FETCH();

	pop_n_elems(args);

	if (!THIS->dblink)
		Pike_error ("Not connected.\n");
	conn=THIS->dblink;
	THREADS_ALLOW();
	PQ_LOCK();
	PQreset(conn);
	PQ_UNLOCK();
	THREADS_DISALLOW();
	if (PQstatus(conn)==CONNECTION_BAD) {
		set_error(PQerrorMessage(conn));
		Pike_error("Bad connection.\n");
	}
}

#if 0
/* This was cut (for now) because of the difficulty of obtaining
 * a valid FILE * from a fileobject.
 */
static void f_trace (INT32 args)
{
	if (args!=1)
		Pike_error ("Wrong args for trace().\n");
	if (Pike_sp[-args].type==PIKE_T_INT)
		if (Pike_sp[-args].u.integer==0)
			PQuntrace(THIS->dblink);
		else
			Pike_error ("Wrong argument for postgres->trace().\n");
	else
/* I really don't know how to check that the argument is an instance of
 * /precompiled/file... I guess there's the name stored somewhere..
 * For now let's presume that if it's an object, then it's a /precompiled/file*/
		PQtrace(THIS->dblink,
				((struct file_struct*)Pike_sp[-args].u.object->storage)->fd);
}
#endif

/*! @decl void _set_notify_callback()
 *! @decl void _set_notify_callback(function f)
 *!
 *! With Postgres you can associate events and notifications to tables.
 *! This function allows you to detect and handle such events.
 *!
 *! With no arguments, resets and removes any callback you might have
 *! put previously, and any polling cycle.
 *!
 *! With one argument, sets the notification callback (there can be only
 *! one for each sqlobject). 
 *! 
 *! The callback function must return no value, and takes a string argument,
 *! which will be the name of the table on which the notification event
 *! has occurred. In future versions, support for user-specified arguments
 *! will be added.
 *!
 *! @note
 *! The @[Sql.postgres] program adds support for automatic delivery of
 *! messages (see it for explanation on the inner workings of this feature).
 *!
 *! This function is Postgres-specific, and thus it is not available
 *! through the generic SQL-interface
 *!
 *! @seealso
 *!   Sql.postgres
 */

static void f_callback(INT32 args)
{
	check_all_args("postgres->_set_notify_callback()",
		       args, BIT_INT|BIT_FUNCTION, 0);

	if (Pike_sp[-args].type==PIKE_T_INT) {
		if (THIS->notify_callback.type!=PIKE_T_FREE) {
			free_svalue(&THIS->notify_callback);
			mark_free_svalue (&THIS->notify_callback);
		}
		pop_n_elems(args);
		return;
	}
	/*let's assume it's a function otherwise*/
	assign_svalue(&THIS->notify_callback,Pike_sp-args);
	pop_n_elems(args);
}


/*! @decl string host_info()
 *!
 *! This function returns a string describing what host are we talking to,
 *! and how (TCP/IP or UNIX sockets).
 */

static void f_host_info (INT32 args)
{
        pop_n_elems(args);

	if (PQstatus(THIS->dblink)!=CONNECTION_BAD) {
		char buf[64];
		sprintf(buf,"TCP/IP %p connection to ",THIS->dblink);
		push_text(buf);
		pgdebug("adding reason\n");
		if(PQhost(THIS->dblink))
			push_text(PQhost(THIS->dblink));
		else
			push_text("<unknown>");
		pgdebug("done\n");
		f_add(2);
		return;
	}
	set_error(PQerrorMessage(THIS->dblink));
	Pike_error ("Bad connection.\n");
}

/*! @decl int affected_rows()
 *!
 *! This function returns the number of rows affected by the last query.
 */

static void f_affected_rows (INT32 args)
{
  pop_n_elems(args);
  push_int(THIS->last_rows);
}

/*! @decl string _quote(string s)
 *!
 *! Escape a string to prevent SQL injection, using the current connection's
 *! character encoding settings.
 */
#if defined(HAVE_PQESCAPESTRINGCONN) || defined(HAVE_PQESCAPESTRING)
static void f_quote(INT32 args)
{
	int err;
	int len;
	struct pike_string *ret;
	struct pike_string *s;
	char *err_msg;

	get_all_args("Postgres->_quote", args, "%S", &s);

	ret = begin_shared_string(s->len * 2 + 1);
#ifdef HAVE_PQESCAPESTRINGCONN
	len = PQescapeStringConn(THIS->dblink, ret->str, s->str, s->len, &err);
	if (err != 0) {
		err_msg = PQerrorMessage(THIS->dblink);
		set_error(err_msg);
		Pike_error(err_msg);
	}
#else
	len = PQescapeString(ret->str, s->str, s->len);
#endif
	pop_n_elems(args);
	push_string(end_and_resize_shared_string(ret, len));
}
#endif /* HAVE_PQESCAPESTRINGCONN || HAVE_PQESCAPESTRING */

/*! @endclass
 *!
 *! @endmodule
 */

PIKE_MODULE_INIT
{
  start_new_program();
  ADD_STORAGE(struct pgres_object_data);
  set_init_callback(pgres_create);
  set_exit_callback(pgres_destroy);

  /* sql-interface compliant functions */
  /* function(void|string,void|string,void|string,void|string,int|void:void) */
  ADD_FUNCTION("create", f_create,
	       tFunc(tOr(tVoid,tStr) tOr(tVoid,tStr) tOr(tVoid,tStr)
		     tOr(tVoid,tStr) tOr(tInt,tVoid), tVoid), 0);

  /* That is: create(hostname,database,port)
   * It depends on the environment variables:
   * PGHOST, PGOPTIONS, PGPORT, PGTTY(don't use!), PGDATABASE
   * Notice: Postgres _requires_ a database to be selected upon connection
   */
  /* function(string:void) */
  ADD_FUNCTION("select_db", f_select_db, tFunc(tStr,tVoid), 0);

  /* function(string:int|object) */
  ADD_FUNCTION("big_query", f_big_query,
       tFunc(tStr tOr(tVoid,tArr(tOr3(tInt,tStr,tVoid))), tOr(tInt,tObj)), 0);

  /* function(void:string) */
  ADD_FUNCTION("error", f_error, tFunc(tVoid,tStr), 0);

  /* function(void:string) */
  ADD_FUNCTION("host_info", f_host_info, tFunc(tVoid,tStr), 0);

  /* function(void:int) */
  ADD_FUNCTION("affected_rows", f_affected_rows, tFunc(tVoid,tInt), 0);

  /* function(string:string) */
#if defined(HAVE_PQESCAPESTRINGCONN) || defined(HAVE_PQESCAPESTRING)
  ADD_FUNCTION("_quote", f_quote, tFunc(tStr,tStr), 0);
#endif


  /* postgres-specific functions */
  /* function(void:void) */
  ADD_FUNCTION("reset", f_reset, tFunc(tVoid,tVoid), 0);

#if 0
  /* function(object|int:void) */
  ADD_FUNCTION("trace", f_trace, tFunc(tOr(tObj,tInt),tVoid), 0);

  /* If given a clone of /precompiled/file, traces to that file.
   * If given 0, stops tracing.
   * See note on the implementation.
   */
#endif
  /* function(int|function(string:void):void) */
  ADD_FUNCTION("_set_notify_callback", f_callback,
	       tFunc(tOr(tInt,tFunc(tStr,tVoid)),tVoid), 0);

  postgres_program = end_program();
  add_program_constant("postgres",postgres_program,0);

  add_string_constant("version",PGSQL_VERSION,0);

  pgresult_init(); 
}

PIKE_MODULE_EXIT
{
  extern struct program * pgresult_program;

  if(postgres_program)
  {
    free_program(postgres_program);
    postgres_program=0;
  }

  if(pgresult_program)
  {
    free_program(pgresult_program);
    pgresult_program=0;
  }
}

#else /* HAVE_POSTGRES */
#include "program.h"
#include "module.h"
#include "module_support.h"
PIKE_MODULE_INIT {
  if(!TEST_COMPAT(7,6))
    HIDE_MODULE();
}
PIKE_MODULE_EXIT {}
#endif /* HAVE_POSTGRES */
