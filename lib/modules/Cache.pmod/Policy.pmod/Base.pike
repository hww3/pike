/*
 * Sample class for the Cache.Storage stuff.
 * by Francesco Chemolli <kinkie@roxen.com>
 *
 * All Storage-related class must MUST implement this method.
 *
 * $Id: Base.pike,v 1.6 2003/11/22 15:00:54 grubba Exp $
 */

#pike __REAL_VERSION__

//! Base class for cache expiration policies.

//! Expire callback.
//!
//!   This function is called to expire parts
//!   of @[storage].
//!
//! @note
//!   All Storage.Policy classes must MUST implement this method.
void expire(Cache.Storage.Base storage) {
  throw("Override this!");
}

