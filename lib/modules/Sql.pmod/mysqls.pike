/*
 * $Id$
 *
 * Glue for the Mysql-module using SSL
 */

//! Implements SQL-urls for
//!   @tt{mysqls://[user[:password]@@][hostname][:port][/database]@}
//!
//! Sets the connection to SSL-mode, and sets the default configuration
//! file to @expr{"/etc/my.cnf"@}.
//!
//! @fixme
//!   Ought to load a suitable default configuration file for Win32 too.
//!
//! @note
//!   This connection method only exists if the Mysql-module has been
//!   compiled with SSL-support.

#pike __REAL_VERSION__

// Cannot dump this since the #if constant(...) check below may depend
// on the presence of system libs at runtime.
constant dont_dump_program = 1;

#if constant(Mysql.mysql.CLIENT_SSL)

inherit Sql.mysql;

void create(string host,
	    string db,
	    string user,
	    string password,
	    mapping(string:mixed)|void options)
{
  if (!mappingp(options))
    options = ([ ]);

  options->connect_options |= CLIENT_SSL;

  if (!options->mysql_config_file)
    options->mysql_config_file = "/etc/my.cnf";

  ::create(host||"", db||"", user||"", password||"", options);
}

#else
constant this_program_does_not_exist = 1;
#endif
