// Pike core things that don't belong anywhere else.
//
// $Id: Pike.pmod,v 1.2 2001/08/14 22:05:44 nilsson Exp $

constant WEAK_INDICES = __builtin.PIKE_WEAK_INDICES;
constant WEAK_VALUES = __builtin.PIKE_WEAK_VALUES;
constant WEAK = WEAK_INDICES|WEAK_VALUES;
//! Flags for use together with @[set_weak_flag] and @[get_weak_flag].
//! See @[set_weak_flag] for details.

constant BacktraceFrame = __builtin.backtrace_frame;
