void exit_test (int failure)
{
  Tools.Testsuite.report_result (1 - !!failure, !!failure);
  exit (failure);
}

#ifndef TEST_NORMAL

#define PRE "connecttest (closed): "

void fail()
{ 
// can't connect to socket - this is what we expect
   write(PRE "everything ok\n");
   exit_test(0); // ok
}

void ok()
{
   if (f->write("hej")==-1)
      write(PRE "succeeded to connect to closed socket"
	     " (port %d)\n",z);
   else
      write(PRE "socket still open (??)"
	     " (port %d)\n",z);

   exit_test(1);
}

#else

#define PRE "connecttest (open): "

void fail()
{ 
   write(PRE "can't connect to open port; failure reported\n");
   exit_test(1); // fail
}

void ok()
{
// can connect to socket - this is what we expect
   if (f->write("hej")==-1)
   {
      write(PRE "connected ok, but socket closed"
	     " (port %d)\n",z);
      exit_test (1);
   }
   else
   {
      write(PRE "everything ok"
	     " (port %d)\n",z);
      exit_test (0);
   }
}

#endif

void rcb(){}

void timeout()
{
   write(PRE "timeout - connection neither succeded "
	  "nor failed\n");
   exit_test(1);
}

object f=Stdio.File();
int z;
object p=Stdio.Port();

int main()
{
   if (!p->bind(0)) {
     write(PRE "failed to bind a port: %s.\n", strerror(p->errno()));
     exit_test(1);
   }
   z = (int)(p->query_address()/" ")[-1];
//     write("port: %d\n",z);
#ifndef TEST_NORMAL
   p->close();
   destruct(p); // this port can't be connected to now
   p = 0;
#endif

   write(PRE "using port %d\n",z);

   sleep(0.1);
   
   f->open_socket();
   // NOTE: Some OS's (NT) signal connection failure on the
   //       error callback (aka read_oob callback).
   f->set_nonblocking(rcb,ok,fail,fail,0);
   int ok;
   if (catch { ok = f->connect("127.0.0.1",z); } &&
       catch { ok = f->connect("localhost",z); })
   {
      write(PRE "failed to connect "
	     "to neither \"localhost\" nor \"127.0.0.1\"\n");
      write(PRE "reporting ok\n");
      exit_test (0);
   } else if (!ok) {
     write(PRE "connect() failed with errno %d: %s\n",
	    f->errno(), strerror(f->errno()));
#ifdef TEST_NORMAL
     write(PRE "reporting failure\n");
     exit_test (1);
#else
     write(PRE "reporting ok\n");
     exit_test (0);
#endif
   }
   call_out(timeout, 10);
   return -1;
}

