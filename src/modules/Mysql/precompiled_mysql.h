/*
 * $Id: precompiled_mysql.h,v 1.2 1997/04/20 03:55:48 grubba Exp $
 *
 * SQL database connectivity for Pike
 *
 * Henrik Grubbstr�m 1996-12-21
 */

#ifndef PRECOMPILED_MYSQL_H
#define PRECOMPILED_MYSQL_H

/*
 * Includes
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

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

/* From the Pike-dist */

/*
 * Structures
 */

struct precompiled_mysql {
  MYSQL		mysql, *socket;
  MYSQL_RES	*last_result;	/* UGLY way to pass arguments to create() */
};

struct precompiled_mysql_result {
  struct object *connection;
  MYSQL_RES	*result;
};

/*
 * Defines
 */

#define PIKE_MYSQL	((struct precompiled_mysql *)(fp->current_storage))
#define PIKE_MYSQL_RES	((struct precompiled_mysql_result *)(fp->current_storage))

/*
 * Globals
 */

extern struct program *mysql_program;
extern struct program *mysql_result_program;

/*
 * Prototypes
 */

/* From result.c */

void init_mysql_res_efuns(void);
void init_mysql_res_programs(void);
void exit_mysql_res(void);

#endif /* PRECOMPILED_MYSQL_H */
