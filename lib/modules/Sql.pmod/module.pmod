#pike __REAL_VERSION__

//! The SQL module is a unified interface between pike and all
//! its supported databases. The parts of this module that is
//! usuable for all normal uses is the @[Sql] class and the
//! @[sql_result] class.
//!
//! @example
//! string people_in_group(string group) {
//!    Sql.Sql db = Sql.Sql("mysql://localhost/testdb");
//!    return db->query("SELECT name FROM users WHERE "
//!                     "group=%s", group)->name * ",";
//! }
