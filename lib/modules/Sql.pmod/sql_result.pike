/*
 * $Id: sql_result.pike,v 1.9 2001/09/06 20:11:00 nilsson Exp $
 *
 * Implements the generic result module of the SQL-interface
 *
 * Henrik Grubbstr�m 1996-01-09
 */

#pike __REAL_VERSION__

//! Implements the generic result of the SQL-interface.
//! Used for return results from SQL.sql->big_query().

#define throw_error(X)	throw(({ (X), backtrace() }))

//! The actual result.
object|array master_res;

//! If the result was an array, this is the current row.
int index;

//! Create a new Sql.sql_result object
//!
//! @param res
//!   Result to use as base.
void create(object|array res)
{
  if (!(master_res = res) || (!arrayp(res) && !objectp(res))) {
    throw_error("Bad arguments to Sql_result()\n");
  }
  index = 0;
}

//! Returns the number of rows in the result.
int num_rows()
{
  if (arrayp(master_res)) {
    return(sizeof(master_res));
  }
  return(master_res->num_rows());
}

//! Returns the number of fields in the result.
int num_fields()
{
  if (arrayp(master_res)) {
    return(sizeof(master_res[0]));
  }
  return(master_res->num_fields());
}

//! Returns non-zero if there are no more rows.
int eof()
{
  if (arrayp(master_res)) {
    return(index >= sizeof(master_res));
  }
  return(master_res->eof());
}

//! Return information about the available fields.
array(mapping(string:mixed)) fetch_fields()
{
  if (arrayp(master_res)) {
    /* Only supports the name field */
    array(mapping(string:mixed)) res = allocate(sizeof(master_res[0]));
    int index = 0;
    
    foreach(sort(indices(master_res[0])), string name) {
      res[index++] = ([ "name": name ]);
    }
    return(res);
  }
  return(master_res->fetch_fields());
}

//! Skip past a number of rows.
//!
//! @param skip
//!   Number of rows to skip.
void seek(int skip)
{
  if (skip < 0) {
    throw_error("seek(): Argument 1 not positive\n");
  }
  if (arrayp(master_res)) {
    index += skip;
  } else if (functionp(master_res->seek)) {
    master_res->seek(skip);
  } else {
    while (skip--) {
      master_res->fetch_row();
    }
  }
}

//! Fetch the next row from the result.
int|array(string|int) fetch_row()
{
  if (arrayp(master_res)) {
    array res;
      
    if (index >= sizeof(master_res)) {
      return 0;
    }
    sort(indices(master_res[index]), res = values(master_res[index]));
    index++;
    return(res);
  }
  return (master_res->fetch_row());
}
