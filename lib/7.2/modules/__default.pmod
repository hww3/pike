// Compatibility module
// $Id: __default.pmod,v 1.10 2002/11/29 02:21:48 nilsson Exp $

#pike 7.3

//! Pike 7.2 efun compatibility module.
//!
//! The methods in this module will appear as efuns in
//! programs that use @tt{#pike 7.2@} or lower.

string dirname(string x)
{
  array(string) tmp=explode_path(x);
  return tmp[..sizeof(tmp)-2]*"/";
}

//!   High-resolution sleep (Pike 7.2 compatibility).
//!
//!   Sleep @[t] seconds. This function was renamed to @[delay()]
//!   in Pike 7.3.
//!
//! @note
//!   This function will busy-wait if the sleep-interval is short.
//!
//! @deprecated delay
//!
//! @seealso
//!   @[predef::sleep()], @[delay()]
void sleep(float|int t, void|int abort)
{
  delay(t, abort);
}

//!   Get the default YP domain (Pike 7.2 compatibility).
//!   This function was removed in Pike 7.3, use
//!   @[Yp.default_domain()] instead.
//!
//! @deprecated Yp.default_domain
//!
//! @seealso
//!   @[YP.default_domain()]
string default_yp_domain() {
  return Yp.default_domain();
}

//!   Instantiate a program (Pike 7.2 compatibility).
//!
//!   A new instance of the class @[prog] will be created.
//!   All global variables in the new object be initialized, and
//!   then @[lfun::create()] will be called with @[args] as arguments.
//!
//!   This function was removed in Pike 7.3, use
//!   @code{((program)@[prog])(@@@[args])@}
//!   instead.
//!
//! @deprecated
//!
//! @seealso
//!   @[destruct()], @[compile_string()], @[compile_file()], @[clone()]
//!
object new(string|program prog, mixed ... args)
{
  if(stringp(prog))
  {
    if(program p=(program)(prog, backtrace()[-2][0]))
      return p(@args);
    else
      error("Failed to find program %s.\n", prog);
  }
  return prog(@args);
}

//! @decl object clone(string|program prog, mixed ... args)
//!
//!   Alternate name for the function @[new()] (Pike 7.2 compatibility).
//!
//!   This function was removed in Pike 7.3, use
//!   @code{((program)@[prog])(@@@[args])@}
//!   instead.
//!
//! @deprecated
//!
//! @seealso
//!   @[destruct()], @[compile_string()], @[compile_file()], @[new()]

function(string|program, mixed ... : object) clone = new;

// spider
#define SPIDER(X) constant X = spider.##X
//! @ignore
SPIDER(_low_program_name);
SPIDER(set_start_quote);
SPIDER(set_end_quote);
SPIDER(parse_accessed_database);
SPIDER(_dump_obj_table);
SPIDER(parse_html);
SPIDER(parse_html_lines);
SPIDER(discdate);
SPIDER(stardate);
SPIDER(get_all_active_fd);
SPIDER(fd_info);
//! @endignore

mapping(string:mixed) all_constants()
{
  mapping(string:mixed) ret=predef::all_constants()+([]);

#define ADD(X) ret->##X=X

  ADD(all_constants);
  ADD(dirname);
  ADD(default_yp_domain);
  ADD(new);
  ADD(clone);
  ADD(_low_program_name);
  ADD(set_start_quote);
  ADD(set_end_quote);
  ADD(parse_accessed_database);
  ADD(_dump_obj_table);
  ADD(parse_html);
  ADD(parse_html_lines);
  ADD(discdate);
  ADD(stardate);
  ADD(get_all_active_fd);
  ADD(fd_info);

  return ret;
}
