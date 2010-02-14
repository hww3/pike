//
// This file needs to support old pikes that e.g. don't understand
// "#pike". Some of them fail when they see an unknown cpp directive.
//
// #pike __REAL_VERSION__
//
// $Id: Pike.pmod,v 1.42 2010/02/14 01:09:11 nilsson Exp $

//! This module parses and tokenizes Pike source code.

inherit "C.pmod";

array(string) low_split(string data, void|mapping state)
{
  if(state && state->remains)
    data = m_delete(state, "remains") + data;
  array ret;
  string rem;
  [ret, rem] = Parser._parser._Pike.tokenize(data);
  if(sizeof(rem)) {
    if(rem[0]=='"')
      throw(UnterminatedStringError(ret*"", rem));
    if(state) state->remains=rem;
  }
  return ret;
}

//! Splits the @[data] string into an array of tokens. An additional
//! element with a newline will be added to the resulting array of
//! tokens. If the optional argument @[state] is provided the split
//! function is able to pause and resume splitting inside #"" and
//! /**/ tokens. The @[state] argument should be an initially empty
//! mapping, in which split will store its state between successive
//! calls.
array(string) split(string data, void|mapping state) {
  array r = low_split(data, state);

  array new = ({});
  for(int i; i<sizeof(r); i++)
    if(r[i][..1]=="//" && r[i][-1]=='\n')
      new += ({ r[i][..<1], "\n" });
    else
      new += ({ r[i] });

  if(sizeof(new) && (< "\n", " " >)[new[-1]])
    new[-1] += "\n";
  else
    new += ({ "\n" });
  return new;
}

class UnterminatedStringError
//! Error thrown when an unterminated string token is encountered.
{
  inherit Error.Generic;
  constant error_type = "unterminated_string";
  constant is_unterminated_string_error = 1;

  string err_str;
  //! The string that failed to be tokenized

  protected void create(string pre, string post)
  { 
    int line = String.count(pre, "\n")+1;
    err_str = pre+post;
    if( sizeof(post) > 100 )
      ::create(sprintf("Unterminated string %O[%d] at line %d\n",
                       post[..100], sizeof(post)-100, line));
    else
      ::create(sprintf("Unterminated string %O at line %d\n",
                       post, line));
  }
}
