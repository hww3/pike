/*
 * An access-time-based expiration policy manager.
 * by Francesco Chemolli <kinkie@roxen.com>
 *
 * $Id: Timed.pike,v 1.6 2003/01/16 14:35:58 grubba Exp $
 */

#pike __REAL_VERSION__

//TODO: use the preciousness somehow.
// idea: expire if (now-atime)*cost < ktime

#define DEFAULT_KTIME 300
private int ktime;

inherit Cache.Policy.Base;

void expire(Cache.Storage.Base storage) {
  werror("Expiring cache\n");
  int now=time(1);
  int limit=now-ktime;
  string key=storage->first();
  while (key) {
    Cache.Data got=storage->get(key,1);
    if (!objectp(got)) {
      //if got==0, there must have been some
      //dependents acting. This is kludgy... :-(
      key=storage->next();
      continue;
    }
    if (got->atime < limit ||
        (got->etime && got->etime < now) ) {
      werror("deleting %s (age: %d, now: %d, etime: %d)\n",
             key, now - got->atime, 
             now, got->etime);
      storage->delete(key);
    }
    key=storage->next();
  }
}

void create(void|int instance_ktime) {
  ktime=(instance_ktime?instance_ktime:DEFAULT_KTIME);
}
