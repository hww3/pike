/*
 * $Id: sql.pike,v 1.40 2000/09/28 03:39:10 hubbe Exp $
 *
 * Implements the generic parts of the SQL-interface
 *
 * Henrik Grubbstr�m 1996-01-09
 */

#pike __REAL_VERSION__

//.
//. File:	sql.pike
//. RCSID:	$Id: sql.pike,v 1.40 2000/09/28 03:39:10 hubbe Exp $
//. Author:	Henrik Grubbstr�m (grubba@idonex.se)
//.
//. Synopsis:	Implements the generic parts of the SQL-interface.
//.
//. +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//.
//. Implements those functions that need not be present in all SQL-modules.
//.

#define throw_error(X)	throw(({ (X), backtrace() }))

//. + master_sql
//.   Object to use for the actual SQL-queries.
object master_sql;

//. + case_convert
//.   Convert all field names in mappings to lower_case.
//.   Only relevant to databases which only implement big_query(),
//.   and use upper/mixed-case fieldnames (eg Oracle).
//.   0 - No (default)
//.   1 - Yes
int case_convert;

//. - quote
//.   Quote a string so that it can safely be put in a query.
//. > s - String to quote.
function(string:string) quote = .sql_util.quote;

//. - encode_time
//.   Converts a system time value to an appropriately formatted time
//.   spec for the database.
//. > arg 1 - Time to encode.
//. > arg 2 - If nonzero then time is taken as a "full" unix time spec
//.   (where the date part is ignored), otherwise it's converted as a
//.   seconds-since-midnight value.
function(int,void|int:string) encode_time;

//. - decode_time
//.   Converts a database time spec to a system time value.
//. > arg 1 - Time spec to decode.
//. > arg 2 - Take the date part from this system time value. If zero, a
//.   seconds-since-midnight value is returned.
function(string,void|int:int) decode_time;

//. - encode_date
//.   Converts a system time value to an appropriately formatted
//.   date-only spec for the database.
//. > arg 1 - Time to encode.
function(int:string) encode_date;

//. - decode_date
//.   Converts a database date-only spec to a system time value.
//. > arg 1 - Date spec to decode.
function(string:int) decode_date;

//. - encode_datetime
//.   Converts a system time value to an appropriately formatted
//.   date and time spec for the database.
//. > arg 1 - Time to encode.
function(int:string) encode_datetime;

//. - decode_datetime
//.   Converts a database date and time spec to a system time value.
//. > arg 1 - Date and time spec to decode.
function(string:int) decode_datetime;

//. - create
//.   Create a new generic SQL object.
//. > host
//.   object - Use this object to access the SQL-database.
//.   string - Connect to the server specified.
//.            The string should be on the format:
//.              [dbtype://][user[:password]@]hostname[:port][/database]
//.            If dbtype isn't specified, use any available database server
//.            on the specified host.
//.            If the hostname is "", access through a UNIX-domain socket or
//.            similar.
//.   zero   - Access through a UNIX-domain socket or similar.
//. > database
//.   Select this database.
//. > user
//.   User name to access the database as.
//. > password
//.   Password to access the database.
void create(void|string|object host, void|string db,
	    void|string user, void|string password)
{
  if (objectp(host)) {
    master_sql = host;
    if ((user && user != "") || (password && password != "")) {
      throw_error("Sql.sql(): Only the database argument is supported when "
		  "first argument is an object\n");
    }
    if (db && db != "") {
      master_sql->select_db(db);
    }
  }
  else {
    if (db == "") {
      db = 0;
    }
    if (user == "") {
      user = 0;
    }
    if (password == "") {
      password = 0;
    }

    array(string) program_names;
    int throw_errors = 1;

    if (host && (host != replace(host, ({ ":", "/", "@" }), ({ "", "", "" })))) {

      // The hostname is on the format:
      //
      // [dbtype://][user[:password]@]hostname[:port][/database]

      array(string) arr = host/"://";
      if ((sizeof(arr) > 1) && (arr[0] != "")) {
	if (sizeof(arr[0]/".pike") > 1) {
	  program_names = ({ (arr[0]/".pike")[0] });
	} else {
	  program_names = ({ arr[0] });
	}
	host = arr[1..] * "://";
      }
      arr = host/"@";
      if (sizeof(arr) > 1) {
	// User and/or password specified
	host = arr[-1];
	arr = (arr[0..sizeof(arr)-2]*"@")/":";
	if (!user && sizeof(arr[0])) {
	  user = arr[0];
	}
	if (!password && (sizeof(arr) > 1)) {
	  password = arr[1..]*":";
	  if (password == "") {
	    password = 0;
	  }
	}
      }
      arr = host/"/";
      if (sizeof(arr) > 1) {
	host = arr[..sizeof(arr)-2]*"/";
	if (!db) {
	  db = arr[-1];
	}
      }
    }

    if (host == "") {
      host = 0;
    }

    if (!program_names) {
#ifdef PIKE_SQL_DEBUG
      program_names = indices(Sql);
#else /* !PIKE_SQL_DEBUG */
      // Ignore compiler errors for the various sql-modules,
      // since we might not have some.
      // This is NOT a nice way to do it, but...
      // It's nicer now, since it's a thread-local variable,
      // but not by much.
      mixed old_inhib = master()->get_inhibit_compile_errors();
      master()->set_inhibit_compiler_errors(lambda(){});
      program_names = indices(Sql);
      // Restore compiler errors mode to whatever it was before.
      master()->set_inhibit_compile_errors(old_inhib);
#endif /* PIKE_SQL_DEBUG */

      throw_errors = 0;
    }

    foreach(program_names, string program_name) {
      if ((sizeof(program_name / "_result") == 1) &&
	  (program_name[..2] != "sql")) {
	/* Don't call ourselves... */
	array(mixed) err;
      
	err = catch {
	  program p;

	  err = catch {p = Sql[program_name];};

	  if (err) {
#ifdef PIKE_SQL_DEBUG
	    Stdio.stderr->write(sprintf("Sql.sql(): Failed to compile module Sql.%s (%s)\n",
					program_name, err[0]));
#endif /* PIKE_SQL_DEBUG */
	    if (throw_errors) {
	      throw(err);
	    } else {
	      throw(0);
	    }
	  }

	  if (p) {
	    err = catch {
	      if (password) {
		master_sql = p(host||"", db||"", user||"", password);
	      } else if (user) {
		master_sql = p(host||"", db||"", user);
	      } else if (db) {
		master_sql = p(host||"", db);
	      } else if (host) {
		master_sql = p(host);
	      } else {
		master_sql = p();
	      }
	    };
	    if (err) {
	      if (throw_errors) {
		throw(err);
	      }
#ifdef PIKE_SQL_DEBUG
	      Stdio.stderr->write(sprintf("Sql.sql(): Failed to connect using module Sql.%s (%s)\n",
					  program_name, err[0]));
#endif /* PIKE_SQL_DEBUG */
	    }
	  } else {
	    if (throw_errors) {
	      throw(({ sprintf("Sql.sql(): Failed to index module Sql.%s\n",
			       program_name), backtrace() }));
	    }
#ifdef PIKE_SQL_DEBUG
	    Stdio.stderr->write(sprintf("Sql.sql(): Failed to index module Sql.%s\n",
					program_name));
#endif /* PIKE_SQL_DEBUG */
	  }
	};
	if(master_sql)
	  break;
	if (err && throw_errors) {
	  throw(err);
	}
      }
    }

    if (!master_sql)
      if (!throw_errors) {
	throw_error("Sql.sql(): Couldn't connect using any of the databases\n");
      } else {
	throw_error("Sql.sql(): Couldn't connect using the " +
		    (program_names[0]/".pike")[0] + " database\n");
      }
  }

  if (master_sql->quote) quote = master_sql->quote;
  encode_time = master_sql->encode_time || .sql_util.fallback;
  decode_time = master_sql->decode_time || .sql_util.fallback;
  encode_date = master_sql->encode_date || .sql_util.fallback;
  decode_date = master_sql->decode_date || .sql_util.fallback;
  encode_datetime = master_sql->encode_datetime || .sql_util.fallback;
  decode_datetime = master_sql->decode_datetime || .sql_util.fallback;
}

static private array(mapping(string:mixed)) res_obj_to_array(object res_obj)
{
  if (res_obj) 
  {
    /* Not very efficient, but sufficient */
    array(mapping(string:mixed)) res = ({});
    array(string) fieldnames;
    array(mixed) row;
    array(mapping) fields = res_obj->fetch_fields();

    fieldnames = (Array.map(fields,
                            lambda (mapping(string:mixed) m) {
                              return((m->table||"") + "." + m->name);
                            }) +
                  fields->name);

    if (case_convert)
      fieldnames = Array.map(fieldnames, lower_case);


    while (row = res_obj->fetch_row())
      res += ({ mkmapping(fieldnames, row + row) });

    return(res);
  }
  return 0;
}

//. - error
//.   Return last error message.  
int|string error()
{
  if (functionp (master_sql->error))
    return master_sql->error();
  return "Unknown error";
}

//. - select_db
//.   Select database to access.
void select_db(string db)
{
  master_sql->select_db(db);
}

//. - compile_query
//.   Compiles the query (if possible). Otherwise returns it as is.
//.   The resulting object can be used multiple times in query() and
//.   big_query().
//. > q
//.   SQL-query to compile.
string|object compile_query(string q)
{
  if (functionp(master_sql->compile_query)) {
    return(master_sql->compile_query(q));
  }
  return(q);
}

//. - handle_extraargs
//.   Handle sprintf-based quoted arguments
private string handle_extraargs(string query, array(mixed) extraargs) {
  return sprintf(query,@Array.map(extraargs,lambda(mixed s)
                                            {return quote((string)s);}));
}

//. - query
//.   Send an SQL query to the underlying SQL-server. The result is returned
//.   as an array of mappings indexed on the name of the columns.
//.   Returns 0 if the query didn't return any result (e.g. INSERT or similar).
//. > q
//.   Query to send to the SQL-server. This can either be a string with the
//.   query, or a previously compiled query (see compile_query()).
//. > extraargs
//.   This parameter, if specified, can be in two forms:
//.   1) a mapping containing bindings of variables used in the query.
//.   A variable is identified by a colon (:) followed by a name or number.
//.   Each index in the mapping corresponds to one such variable, and the
//.   value for that index is substituted (quoted) into the query wherever
//.   the variable is used.
//.   (i.e. query("select foo from bar where gazonk=':baz'",
//.         (["baz":"value"])) )
//.   Binary values (BLOBs) may need to be placed in multisets. 
//.   2) arguments as you would use in sprintf. They are automatically 
//.   quoted.
//.   (i.e. query("select foo from bar where gazonk='%s'","value") )
array(mapping(string:mixed)) query(object|string q,
                                   mixed ... extraargs)
{
  mapping(string|int:mixed) bindings=0;
  if (extraargs && sizeof(extraargs)) {
    if (mappingp(extraargs[0])) {
      bindings=extraargs[0];
    } else {
      q=handle_extraargs(q,extraargs);
    }
  }
  if (functionp(master_sql->query)) {
    if (bindings) {
      return(master_sql->query(q, bindings));
    } else {
      return(master_sql->query(q));
    }
  }
  if (bindings) {
    return(res_obj_to_array(master_sql->big_query(q, bindings)));
  } else {
    return(res_obj_to_array(master_sql->big_query(q)));
  }
}

//. - big_query
//.   Send an SQL query to the underlying SQL-server. The result is returned
//.   as a Sql.sql_result object. This allows for having results larger than
//.   the available memory, and returning some more info about the result.
//.   Returns 0 if the query didn't return any result (e.g. INSERT or similar).
//.   For the other arguments, they are the same as the query() function.
int|object big_query(object|string q, mixed ... extraargs)
{
  object|array(mapping) pre_res;
  mapping(string|int:mixed) bindings=0;
  
  if (extraargs && sizeof(extraargs)) {
    if (mappingp(extraargs[0])) {
      bindings=extraargs[0];
    } else {
      q=handle_extraargs(q,extraargs);
    }
  }  

  if (functionp(master_sql->big_query)) {
    if (bindings) {
      pre_res = master_sql->big_query(q, bindings);
    } else {
      pre_res = master_sql->big_query(q);
    }
  } else if (bindings) {
    pre_res = master_sql->query(q, bindings);
  } else {
    pre_res = master_sql->query(q);
  }
  return(pre_res && Sql.sql_result(pre_res));
}

//. - create_db
//.   Create a new database.
//. > db
//.   Name of database to create.
void create_db(string db)
{
  master_sql->create_db(db);
}

//. - drop_db
//.   Drop database
//. > db
//.   Name of database to drop.
void drop_db(string db)
{
  master_sql->drop_db(db);
}

//. - shutdown
//.   Shutdown a database server.
void shutdown()
{
  if (functionp(master_sql->shutdown)) {
    master_sql->shutdown();
  } else {
    throw_error("sql->shutdown(): Not supported by this database\n");
  }
}

//. - reload
//.   Reload the tables.
void reload()
{
  if (functionp(master_sql->reload)) {
    master_sql->reload();
  } else {
    /* Probably safe to make this a NOOP */
  }
}

//. - server_info
//.   Return info about the current SQL-server.
string server_info()
{
  if (functionp(master_sql->server_info)) {
    return(master_sql->server_info());
  }
  return("Unknown SQL-server");
}

//. - host_info
//.   Return info about the connection to the SQL-server.
string host_info()
{
  if (functionp(master_sql->host_info)) {
    return(master_sql->host_info());
  } 
  return("Unknown connection to host");
}

//. - list_dbs
//.   List available databases on this SQL-server.
//. > wild
//.   Optional wildcard to match against.
array(string) list_dbs(string|void wild)
{
  array(string)|array(mapping(string:mixed))|object res;
  
  if (functionp(master_sql->list_dbs)) {
    if (objectp(res = master_sql->list_dbs())) {
      res = res_obj_to_array(res);
    }
  } else {
    res = query("show databases");
  }
  if (sizeof(res) && mappingp(res[0])) {
    res = Array.map(res, lambda (mapping m) {
      return(values(m)[0]);	/* Hope that there's only one field */
    } );
  }
  if (wild) {
    res = Simulate.map_regexp(res,
			      replace(wild, ({ "%", "_" }), ({ ".*", "." }) ));
  }
  return(res);
}

//. - list_tables
//.   List tables available in the current database.
//. > wild
//.   Optional wildcard to match against.
array(string) list_tables(string|void wild)
{
  array(string)|array(mapping(string:mixed))|object res;
  
  if (functionp(master_sql->list_tables)) {
    if (objectp(res = master_sql->list_tables())) {
      res = res_obj_to_array(res);
    }
  } else {
    res = query("show tables");
  }
  if (sizeof(res) && mappingp(res[0])) {
    res = Array.map(res, lambda (mapping m) {
      return(values(m)[0]);	/* Hope that there's only one field */
    } );
  }
  if (wild) {
    res = Simulate.map_regexp(res,
			      replace(wild, ({ "%", "_" }), ({ ".*", "." }) ));
  }
  return(res);
}

//. - list_fields
//.   List fields available in the specified table
//. > table
//.   Table to list the fields of.
//. > wild
//.   Optional wildcard to match against.
array(mapping(string:mixed)) list_fields(string table, string|void wild)
{
  array(mapping(string:mixed))|object res;

  if (functionp(master_sql->list_fields)) {
    if (objectp(res = master_sql->list_fields(table))) {
      res = res_obj_to_array(res);
    }
    if (wild) {
      /* Not very efficient, but... */
      res = Array.filter(res, lambda (mapping m, string re) {
	return(sizeof(Simulate.map_regexp( ({ m->name }), re)));
      }, replace(wild, ({ "%", "_" }), ({ ".*", "." }) ) );
    }
    return(res);
  }
  if (wild) {
    res = query("show fields from \'" + table +
		"\' like \'" + wild + "\'");
  } else {
    res = query("show fields from \'" + table + "\'");
  }
  res = Array.map(res, lambda (mapping m, string table) {
    foreach(indices(m), string str) {
      /* Add the lower case variants */
      string low_str = lower_case(str);
      if (low_str != str && !m[low_str]) {
	m[low_str] = m[str];
	m_delete(m, str);	/* Remove duplicate */
      }
    }
    if ((!m->name) && m->field) {
      m["name"] = m->field;
      m_delete(m, "field");	/* Remove duplicate */
    }
    if (!m->table) {
      m["table"] = table;
    }
    return(m);
  }, table);
  return(res);
}

