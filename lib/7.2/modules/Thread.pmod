#pike 7.3

//
// $Id: Thread.pmod,v 1.3 2002/12/15 00:15:45 marcus Exp $
//
// Pike 7.2 backward compatibility layer.
//

inherit Thread;

#if constant(thread_create)

class Condition
{
  inherit Thread::Condition;

  // Old-style wait().
  void wait(void|Mutex m)
  {
    if (!m) {
      m = Mutex();
      m->lock();
    }
    ::wait(m);
  }
}

#endif /* !constant(thread_create) */
