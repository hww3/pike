#!/usr/local/bin/pike

/* $Id: sendfiletest.pike,v 1.13 2010/07/11 11:48:42 mast Exp $ */

final constant TEST_SIZE = 16384;

string testdata = random_string(TEST_SIZE);

int testno;

object(Stdio.Port) loopback = Stdio.Port();
int loopbackport;

/*
 * Some helper functions.
 */

void exit_test (int failure)
{
  Tools.Testsuite.report_result (max (testno - !!failure, 0), !!failure);
  exit (failure);
}

object(Stdio.File) From(string f)
{
  object(Stdio.File) from = Stdio.File();

  if (!from->open(f, "r")) {
    write("Failed to open %O for reading.\n", f);
    exit_test(1);
  }
  return from;
}

object(Stdio.File) To(string f)
{
  object(Stdio.File) to = Stdio.File();

  if (!to->open(f, "cwt")) {
    write("Failed to open %O for writing.\n", f);
    exit_test(1);
  }
  return to;
}

array(object(Stdio.File)) SocketPair()
{
  object(Stdio.File) sock1, sock2;
  sock1 = Stdio.File();
  sock1->connect("127.0.0.1", loopbackport);
  sock2 = loopback->accept();
  if(!sock2)
  {
    write("Accept returned 0\n");
    exit_test(1);
  }
  return ({ sock1, sock2 });
}

void Verify()
{
  array(string) data = From("conftest.dst")->read()/TEST_SIZE;
  int i;
  for(i=0; i < sizeof(data); i++) {
    if (data[i] != testdata) {
      write("Segment %d corrupted!\n", i);
      int j;
      for (j=0; j < TEST_SIZE; j++) {
	if (data[i][j] != testdata[j]) {
	  write("First corrupt byte at segment offset %d: 0x%02x != 0x%02x\n",
		 j, data[i][j], testdata[j]);
	  exit_test(1);
	}
      }
      write("Corrupt byte not found!\n");
      exit_test(1);
    }
  }
}

/*
 * The driver function.
 */

void next()
{
  testno++;

  function test;
  if (!(test = this_object()["test"+testno])) exit_test(0);
  mixed err;
  if (err = catch {
    write("Sendfile test: %d\n", testno);
    test();
  }) {
    catch {
      write("Test %d failed!\n"
	     "%s\n",
	     testno,
	     describe_backtrace(err));
    };
    exit_test(1);
  }
}

void done(int sent, int expected)
{
  if (sent != expected) {
    write(sprintf("Test %d failed: %d != %d\n", testno, sent, expected));
    exit_test(1);
  }
  call_out(next, 0);
}

/*
 * The actual tests.
 */

void test1()
{
  /* First try sending out testdata to a plain file. */

  if (!Stdio.sendfile(testdata/1024, 0, 0, -1, 0,
		      To("conftest.src"), done, TEST_SIZE)) {
    write("Stdio,sendfile() failed!\n");
    exit_test(1);
  }
}

void test2()
{
  /* Then try copying it to another file. */

  if (!Stdio.sendfile(0, From("conftest.src"), 0, -1, 0,
		      To("conftest.dst"), done, TEST_SIZE)) {
    write("Stdio.sendfile() failed!\n");
    exit_test(1);
  }
}

void test3()
{
  Verify();

  /* Try with a headers + file + trailers combo. */

  if (!Stdio.sendfile(testdata/4096, From("conftest.src"), 0, -1,
		      testdata/512, To("conftest.dst"), done, TEST_SIZE*3)) {
    write("Stdio.sendfile() failed!\n");
    exit_test(1);
  }
}

void test4()
{
  Verify();

  /* Try a loopback test. */

  array(object) pair = SocketPair();

  if (!Stdio.sendfile(testdata/4096, From("conftest.src"), 0, -1,
		      testdata/512, pair[0], done, TEST_SIZE*3)) {
    write("Stdio.sendfile() failed!\n");
    exit_test(1);
  }

  if (!Stdio.sendfile(testdata/4096, pair[1], 0, -1,
		      testdata/512, To("conftest.dst"), done, TEST_SIZE*5)) {
    write("Stdio.sendfile() failed!\n");
    exit_test(1);
  }
}

void test5()
{
  /* Dummy (test 4 will call done twice). */
}

void test6()
{
  Verify();

  next();
}

void test7()
{
  /* Clean up. */
  rm("conftest.src");
  rm("conftest.dst");
  next();
}

/*
 * Start the backend.
 */

int main(int argc, array(string) argv)
{
#if constant(alarm)
  alarm(5*60);	// 5 minutes should be sufficient for this test.
#endif
  write("\n");
  loopback->bind(0);
  loopbackport = (int)((loopback->query_address()/" ")[1]);
  call_out(next, 0);
  return -1;
}
