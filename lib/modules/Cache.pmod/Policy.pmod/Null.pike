/*
 * Null policy-manager for the generic Caching system
 * by Francesco Chemolli <kinkie@roxen.com>
 * (C) 2000 Roxen IS
 *
 * $Id: Null.pike,v 1.3 2000/09/28 03:38:29 hubbe Exp $
 *
 * This is a policy manager that doesn't actually expire anything.
 * It is useful in multilevel and/or network-based caches.
 */

#pike __REAL_VERSION__

void expire (Cache.Storage storage) {
  /* empty */
}
