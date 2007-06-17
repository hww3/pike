#ifndef TEST_NORMAL

#define PRE "connecttest (closed): "

void fail()
{ 
// can't connect to socket - this is what we expect
   write(PRE "everything ok\n");
   exit(0); // ok
}

void ok()
{
   if (f->write("hej")==-1)
      werror(PRE "succeeded to connect to closed socket"
	     " (port %d)\n",z);
   else
      werror(PRE "socket still open (??)"
	     " (port %d)\n",z);

   exit(1);      
}

#else

#define PRE "connecttest (open): "

void fail()
{ 
   werror(PRE "can't connect to open port; failure reported\n");
   exit(1); // fail
}

void ok()
{
// can connect to socket - this is what we expect
   if (f->write("hej")==-1)
   {
      werror(PRE "connected ok, but socket closed"
	     " (port %d)\n",z);
      exit (1);
   }
   else
   {
      write(PRE "everything ok"
	     " (port %d)\n",z);
      exit (0);
   }
}

#endif

void rcb(){}

void timeout()
{
   werror(PRE "timeout - connection neither succeded "
	  "nor failed\n");
   exit(1);
}

object f=Stdio.File();
int z;
object p=Stdio.Port();

int main()
{
   if (!p->bind(0)) {
     werror(PRE "failed to bind a port: %s.\n", strerror(p->errno()));
     exit(1);
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
   //       error callback.
   f->set_nonblocking(rcb,ok,fail);
   int ok;
   if (catch { ok = f->connect("127.0.0.1",z); } &&
       catch { ok = f->connect("localhost",z); })
   {
      write(PRE "failed to connect "
	     "to neither \"localhost\" nor \"127.0.0.1\"\n");
      write(PRE "reporting ok\n");
      return 0;
   } else if (!ok) {
#ifdef TEST_NORMAL
     werror(PRE "connect() failed with errno %d: %s\n",
	    f->errno(), strerror(f->errno()));
     werror(PRE "reporting failure\n");
     return 1;
#else
     write(PRE "connect() failed with errno %d: %s\n",
	   f->errno(), strerror(f->errno()));
     write(PRE "reporting ok\n");
     return 0;
#endif
   }
   call_out(timeout, 10);
   return -1;
}

