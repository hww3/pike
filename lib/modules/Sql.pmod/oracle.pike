/*
 * $Id$
 *
 * Glue for the Oracle-module
 */

#pike __REAL_VERSION__

// Cannot dump this since the #if constant(...) check below may depend
// on the presence of system libs at runtime.
constant dont_dump_program = 1;

#if constant(Oracle.oracle)
inherit Oracle.oracle;

string server_info()
{
  return "Oracle";
}

#else
constant this_program_does_not_exist = 1;
#endif /* constant(Oracle.oracle) */
