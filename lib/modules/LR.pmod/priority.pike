/*
 * $Id: priority.pike,v 1.5 2001/11/19 00:44:35 nilsson Exp $
 *
 * Rule priority specification
 *
 * Henrik Grubbstr�m 1996-12-05
 */

#pike __REAL_VERSION__

//!
//! Specifies the priority and associativity of a rule.
//!

//!   Priority value
int value;

//! Associativity
//!
//! @int
//!   @value -1
//!    Left
//!   @value 0
//!    None
//!   @value 1
//!    Right
//! @endint
int assoc;

//! Create a new priority object.
//!
//! @param p
//!  Priority.
//! @param  a
//!  Associativity.
void create(int p, int a)
{
  value = p;
  assoc = a;
}
