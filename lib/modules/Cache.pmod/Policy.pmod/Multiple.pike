/*
 * A multiple-policies expiration policy manager.
 * by Francesco Chemolli <kinkie@roxen.com>
 *
 * $Id: Multiple.pike,v 1.4 2002/01/15 22:31:24 nilsson Exp $
 */

#pike __REAL_VERSION__

inherit Cache.Policy.Base;
private array(Cache.Policy.Base) my_policies;

void expire (Cache.Storage storage) {
  foreach(my_policies, object policy) {
    policy->expire(storage);
  }
}

void create(Cache.Policy.Base ... policies) {
  my_policies=policies;
}
