/*
 * $Id: sql_util.pmod,v 1.7 2001/01/09 21:18:30 marcus Exp $
 *
 * Some SQL utility functions.
 * They are kept here to avoid circular references.
 *
 * Henrik Grubbstr�m 1999-07-01
 */

#pike __REAL_VERSION__

//.
//. File:	sql_util.pmod
//. RCSID:	$Id: sql_util.pmod,v 1.7 2001/01/09 21:18:30 marcus Exp $
//. Author:	Henrik Grubbstr�m (grubba@idonex.se)
//.
//. Synopsis:	Some SQL utility functions
//.
//. +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//.
//. These functions are kept here mainly to avoid circular references.
//.

//. - quote
//.   Quote a string so that it can safely be put in a query.
//. > s - String to quote.
string quote(string s)
{
  return(replace(s, "\'", "\'\'"));
}

//. - fallback
//.   Throw an error in case an unimplemented function is called.
void fallback()
{
  throw(({ "Function not supported in this database.", backtrace() }));
}

//. - build a raw SQL query, given the cooked query and the variable bindings
//.   It's meant to be used as an emulation engine for those drivers not
//.   providing such a behaviour directly (i.e. Oracle).
//.   The raw query can contain some variables (identified by prefixing
//.   a colon to a name or a number(i.e. :var, :2). They will be
//.   replaced by the corresponding value in the mapping.
//. > query
//.   The query
//. > bindings
//.   Optional mapping containing the variable bindings. Make sure that
//.   no confusion is possible in the query. If necessary, change the
//.   variables' names
string emulate_bindings(string query, mapping(string|int:mixed)|void bindings,
                        void|object driver)
{
  array(string)k, v;
  function my_quote=(driver&&driver->quote?driver->quote:quote);
  if (!bindings)
    return query;
  v=Array.map(values(bindings),
              lambda(mixed m) {
		if(multisetp(m)) m = indices(m)[0];
		return (stringp(m)? "'"+my_quote(m)+"'" : (string)m);
	      });
  k=Array.map(indices(bindings),lambda(string s){
				  return (stringp(s)? s : ":"+s);
				});
  return replace(query,k,v);
}
