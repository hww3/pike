/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: acconfig.h,v 1.12 2003/03/28 15:31:42 marcus Exp $
*/

/*
 * Config-file for the Pike mySQL-module.
 *
 * Henrik Grubbstr�m 1997-01-30
 */

#ifndef PIKE_MYSQL_CONFIG_H
#define PIKE_MYSQL_CONFIG_H

@TOP@
@BOTTOM@

/* Define if you have mySQL */
#undef HAVE_MYSQL

/* Define if you have SSL support in MySQL */
#undef HAVE_MYSQL_SSL

/* Return type of mysql_fetch_lengths(). Usually unsigned long. */
#undef FETCH_LENGTHS_TYPE

/* Define if you have mysql_fetch_lengths(). */
#undef HAVE_MYSQL_FETCH_LENGTHS

/* Define if you have mysql_real_query(). */
#undef HAVE_MYSQL_REAL_QUERY

/* Define if you have mysql_options(). */
#undef HAVE_MYSQL_OPTIONS

/* Define if you have the mysql_port variable. */
#undef HAVE_MYSQL_PORT

/* Define if you have the mysql_unix_port variable. */
#undef HAVE_MYSQL_UNIX_PORT

/* Define if your mysql.h defines MYSQL_OPT_CONNECT_TIMEOUT */
#undef HAVE_MYSQL_OPT_CONNECT_TIMEOUT

/* Define if your mysql.h defines MYSQL_OPT_COMPRESS */
#undef HAVE_MYSQL_OPT_COMPRESS

/* Define if your mysql.h defines MYSQL_OPT_NAMED_PIPE */
#undef HAVE_MYSQL_OPT_NAMED_PIPE

/* Define if your mysql.h defines MYSQL_INIT_COMMAND */
#undef HAVE_MYSQL_INIT_COMMAND

/* Define if your mysql.h defines MYSQL_READ_DEFAULT_FILE */
#undef HAVE_MYSQL_READ_DEFAULT_FILE

/* Define if your mysql.h defines MYSQL_READ_DEFAULT_GROUP */
#undef HAVE_MYSQL_READ_DEFAULT_GROUP

/* Define if your mysql.h defines MYSQL_SET_CHARSET_DIR */
#undef HAVE_MYSQL_SET_CHARSET_DIR

/* Define if your mysql.h defines MYSQL_SET_CHARSET_NAME */
#undef HAVE_MYSQL_SET_CHARSET_NAME

/* Define if your mysql.h defines MYSQL_OPT_LOCAL_INFILE */
#undef HAVE_MYSQL_OPT_LOCAL_INFILE

/* Define if you have the `CreateSemaphoreA' function. */
#undef HAVE_CREATESEMAPHOREA

#endif /* PIKE_MYSQL_CONFIG_H */
