#pike __REAL_VERSION__
#pragma strict_types

// Pike core things that don't belong anywhere else.
//
// $Id: Pike.pmod,v 1.8 2003/04/27 02:20:40 nilsson Exp $

constant WEAK_INDICES = __builtin.PIKE_WEAK_INDICES;
constant WEAK_VALUES = __builtin.PIKE_WEAK_VALUES;
constant WEAK = WEAK_INDICES|WEAK_VALUES;
//! Flags for use together with @[set_weak_flag] and @[get_weak_flag].
//! See @[set_weak_flag] for details.

constant BacktraceFrame = __builtin.backtrace_frame;

#if constant(__builtin.security)
// This only exists if the run-time has been compiled with
// --with-security.
constant Security = __builtin.security;
#endif /* constant(__builtin.security) */

constant Backend = __builtin.Backend;
constant DefaultBackend = __builtin.__backend;

constant gc_parameters = __builtin.gc_parameters;
