#!/usr/local/bin/pike

/* $Id: test_pike.pike,v 1.63 2002/05/02 16:00:31 nilsson Exp $ */

import Stdio;

#if !constant(_verify_internals)
#define _verify_internals()
#endif

#if !constant(_dmalloc_set_name)
void _dmalloc_set_name(mixed ... args) {}
#endif

int foo(string opt)
{
  if(opt=="" || !opt) return 1;
  return (int)opt;
}

int istty_cache;
int istty()
{
#ifdef __NT__
  return 1;
#else
  if(!istty_cache)
  {
    istty_cache=!!Stdio.stdin->tcgetattr();
    if(!istty_cache)
    {
      istty_cache=-1;
    }else{
      switch(getenv("TERM"))
      {
	case "dumb":
	case "emacs":
	  istty_cache=-1;
      }
    }
  }
  return istty_cache>0;
#endif
}

mapping(string:int) cond_cache=([]);

#if constant(thread_create)
#define HAVE_DEBUG
#endif

void bzot(string test)
{
  int line=1;
  int tmp=strlen(test)-1;
  while(test[tmp]=='\n') tmp--;
  foreach(test[..tmp]/"\n",string s)
    werror("%3d: %s\n",line++,s);
}

array find_testsuites(string dir)
{
  array(string) ret=({});
  if(array(string) s=get_dir(dir||"."))
  {
    foreach(s, string file)
      {
	switch(file)
	{
	  case "testsuite":
	  case "module_testsuite":
	    ret+=({ combine_path(dir||"",file) });
	}
      }

    foreach(s, string file)
      {
	string name=combine_path(dir||"",file);
	if(file_size(name)==-2)
	  ret+=find_testsuites(name);
      }
  }
  return ret;
}

array(string) read_tests( string fn ) {
  string|array(string) tests = read_bytes( fn );
  if(!tests) {
    werror("Failed to read test file %O, errno=%d.\n",
	   fn, errno());
    exit(1);
  }
      
  tests = tests/"\n....\n";
  return tests[0..sizeof(tests)-2];
}


//
// Watchdog stuff
//

// 20 minutes should be enough..
#if !constant(_reset_dmalloc)
#define WATCHDOG_TIMEOUT 60*20
#else
// ... unless we're running dmalloc
#define WATCHDOG_TIMEOUT 60*80
#endif

#if constant(thread_create)
#define WATCHDOG
#define WATCHDOG_PIPE
object watchdog_pipe;
#else
#if constant(signal) && constant(signum)
#define WATCHDOG
#define WATCHDOG_SIGNAL
#endif
#endif	  

#ifdef WATCHDOG
object watchdog;
int use_watchdog=1;
int watchdog_time;

void signal_watchdog()
{
#ifdef WATCHDOG
  if(use_watchdog && time() - watchdog_time > 30)
  {
    watchdog_time=time();
#ifdef WATCHDOG_PIPE
    watchdog_pipe->write("x",1);
#endif
    
#ifdef WATCHDOG_SIGNAL
    watchdog->kill(signum("SIGQUIT"));
#endif
  }
#endif
}
#endif

void run_watchdog(int pid) {
#ifdef WATCHDOG
  int cnt=0;
  int last_time=time();
  int exit_quietly;
#ifdef WATCHDOG_PIPE
  thread_create(lambda() {
		  object o=Stdio.File("stdin");
		  while(strlen(o->read(1) || ""))
		  {
		    last_time=time();
		  }
		  exit_quietly=1;
		});
#endif

#ifdef WATCHDOG_SIGNAL
  werror("Setting signal (1)\n");
  if(signum("SIGQUIT")>=0)
  {
    werror("Setting signal (2)\n");
    signal( signum("SIGQUIT"),
	    lambda() {
	      last_time=time();
	    });
  }
  else {
    exit(1);
  }
#endif	  

  while(1)
  {
    sleep(10);
    if(exit_quietly) _exit(0);
#ifndef __NT__
    if(!kill(pid, 0)) _exit(0);
#endif

    /* I hope 30 minutes per test is enough for everybody */
    if(time() - last_time > WATCHDOG_TIMEOUT)
    {
      werror("\n[WATCHDOG] Pike testsuite timeout, sending SIGABRT.\n");
      kill(pid, signum("SIGABRT"));
      for(int q=0;q<60;q++) if(!kill(pid,0)) _exit(0); else sleep(1);
      werror("\n"
	     "[WATCHDOG] This is your friendly watchdog again...\n"
	     "[WATCHDOG] testsuite failed to die from SIGABRT, sending SIGKILL\n");
      kill(pid, signum("SIGKILL"));
      for(int q=0;q<60;q++) if(!kill(pid,0)) _exit(0); else sleep(1);
      werror("\n"
	     "[WATCHDOG] This is your friendly watchdog AGAIN...\n"
	     "[WATCHDOG] SIGKILL, SIGKILL, SIGKILL, DIE!\n");
      kill(pid, signum("SIGKILL"));
      kill(pid, signum("SIGKILL"));
      kill(pid, signum("SIGKILL"));
      kill(pid, signum("SIGKILL"));
      for(int q=0;q<60;q++) if(!kill(pid,0)) _exit(0); else sleep(1);
      werror("\n"
	     "[WATCHDOG] Giving up, must be a device wait.. :(\n");
      _exit(0);
    }
  }
#else
  _exit(1);
#endif
}


int main(int argc, array(string) argv)
{
  int e, verbose, prompt, successes, errors, t, check;
  int skipped, quiet;
  array(string) tests;
  program testprogram;
  int start, fail, mem;
  int loop=1;
  int end=0x7fffffff;
  string extra_info="";
  int shift;

#if constant(signal) && constant(signum)
  if(signum("SIGQUIT")>=0)
  {
    signal(signum("SIGQUIT"),lambda()
	   {
	     master()->handle_error( ({"\nSIGQUIT recived, printing backtrace and continuing.\n",backtrace() }) );

	     mapping x=_memory_usage();
	     foreach(sort(indices(x)),string p)
	     {
	       if(sscanf(p,"%s_bytes",p))
	       {
		 werror("%20ss:  %8d   %8d Kb\n",p,
			x["num_"+p+"s"],
			x[p+"_bytes"]/1024);
	       }
	     }
	   });
  }
#endif

  array(string) args=backtrace()[0][3];
  array(string) testsuites=({});
  args=args[..sizeof(args)-1-argc];
  add_constant("RUNPIKE",Array.map(args,Process.sh_quote)*" ");

  foreach(Getopt.find_all_options(argv,aggregate(
    ({"no-watchdog",Getopt.NO_ARG,({"--no-watchdog"})}),
    ({"watchdog",Getopt.HAS_ARG,({"--watchdog"})}),
    ({"help",Getopt.NO_ARG,({"-h","--help"})}),
    ({"verbose",Getopt.MAY_HAVE_ARG,({"-v","--verbose"})}),
    ({"prompt",Getopt.NO_ARG,({"-p","--prompt"})}),
    ({"quiet",Getopt.NO_ARG,({"-q","--quiet"})}),
    ({"start",Getopt.HAS_ARG,({"-s","--start-test"})}),
    ({"end",Getopt.HAS_ARG,({"-e","--end-after"})}),
    ({"fail",Getopt.NO_ARG,({"-f","--fail"})}),
    ({"loop",Getopt.HAS_ARG,({"-l","--loop"})}),
    ({"trace",Getopt.HAS_ARG,({"-t","--trace"})}),
    ({"check",Getopt.MAY_HAVE_ARG,({"-c","--check"})}),
    ({"mem",Getopt.NO_ARG,({"-m","--mem","--memory"})}),
    ({"auto",Getopt.NO_ARG,({"-a","--auto"})}),
    ({"notty",Getopt.NO_ARG,({"-T","--notty"})}),
#ifdef HAVE_DEBUG
    ({"debug",Getopt.MAY_HAVE_ARG,({"-d","--debug"})}),
#endif
    )),array opt)
    {
      switch(opt[0])
      {
	case "no-watchdog":
	  use_watchdog=0;
	  break;

	case "watchdog":
	  run_watchdog( (int)opt[1] );
	  break;
	  
	case "notty":
	  istty_cache=-1;
	  break;

	case "help":
	  write(doc);
	  return 0;

	case "verbose": verbose+=foo(opt[1]); break;
	case "prompt": prompt+=foo(opt[1]); break;
        case "quiet": quiet=1; istty_cache=-1; break;
	case "start": start=foo(opt[1]); start--; break;
	case "end": end=foo(opt[1]); break;
	case "fail": fail=1; break;
	case "loop": loop=foo(opt[1]); break;
	case "trace": t+=foo(opt[1]); break;
	case "check": check+=foo(opt[1]); break;
	case "mem": mem=1; break;

	case "auto":
	  testsuites=find_testsuites(".");
	  break;

#ifdef HAVE_DEBUG
	case "debug":
	{
	  object p=Stdio.Port();
	  p->bind(0);
	  werror("Debug port is: %s\n",p->query_address());
	  sscanf(p->query_address(),"%*s %d",int portno);
	  extra_info+=sprintf(" dport:%d",portno);
	  thread_create(lambda(object p){
	    while(p)
	    {
	      if(object o=p->accept())
	      {
		object q=Stdio.FILE();
		q->assign(o);
		destruct(o);
		Tools.Hilfe.GenericHilfe(q,q);
	      }
	    }
	  },p);
	}
#endif
      }
    }

#ifdef WATCHDOG
  int watchdog_time=time();

  if(use_watchdog)
  {
#ifdef WATCHDOG_PIPE
    object watchdog_tmp=Stdio.File();
    watchdog_pipe=watchdog_tmp->pipe(Stdio.PROP_IPC);
    watchdog=Process.create_process(
      backtrace()[0][3] + ({  "--watchdog="+getpid() }),
      (["stdin":watchdog_tmp ]));
    destruct(watchdog_tmp);
#endif
    
#ifdef WATCHDOG_SIGNAL
    watchdog=Process.create_process(
      backtrace()[0][3] + ({  "--watchdog="+getpid() }) );
#endif
  }
  add_constant("__signal_watchdog",signal_watchdog);
#else
  add_constant("__signal_watchdog",lambda(){});
#endif

  testsuites += Getopt.get_args(argv, 1)[1..];
  if(!sizeof(testsuites))
  {
    werror("No tests found. Use --help for more information.\n");
    exit(1);
  }

  while(loop--)
  {
    successes=errors=0;
  testloop:
    foreach(testsuites, string testsuite)
    {
      tests = read_tests( testsuite );

      werror("Doing tests in %s (%d tests)\n", testsuite, sizeof(tests));
      int qmade, qskipped, qmadep, qskipp;
      
      for(e=start;e<sizeof(tests);e++)
      {
	signal_watchdog();

	int skip=0;
	string test,condition;
	string|int type;
	object o;
	mixed a,b;
	
	if(check)
        {
	  if(check>0 || (e % -check)==0 )
	      _verify_internals();
	}
	if(check>3) {
	  gc();
	  _verify_internals();
	}
	
	test=tests[e];
	if( sscanf(test, "COND %s\n%s", condition, test)==2 )
        {
	  int tmp;
	  if(!(tmp=cond_cache[condition]))
	  {
	    mixed err = catch {
	      tmp=!!(compile_string("mixed c() { return "+condition+"; }",
				    "Cond "+(e+1))()->c());
	    };

	    if(err) {
	      werror( "\nConditional %d failed:\n"
		      "%s\n", e+1, describe_backtrace(err) );
	      errors++;
	      tmp = -1;
	    }

	    if(!tmp) tmp=-1;
	    cond_cache[condition]=tmp;
	  }
	  
	  if(tmp==-1)
	  {
	    if(verbose)
	      werror("Not doing test "+(e+1)+"\n");
	    successes++;
	    skipped++;
	    skip=1;
	  }
	}

	if(istty())
        {
	  if(!verbose)
	    werror("%6d\r",e+1);
	}
	else if(quiet){
	  if(skip) {
	    if(qmade) werror(" Made %d test%s.\n", qmade, qmade==1?"":"s");
	    qmade=0;
	    qskipp=1;
	    qskipped++;
	  }
	  else {
	    if(qskipped) werror(" Skipped %d test%s.\n", qskipped, qskipped==1?"":"s");
	    qskipped=0;
	    qmadep=1;
	    qmade++;
	  }
	}
	else {
	  /* Use + instead of . so that sendmail and
	   * cron will not cut us off... :(
	   */
	  switch( (e-start) % 50)
	  {
	  case 0:
	    werror("%5d: ",e);
	    break;

	  case 9:
	  case 19:
	  case 29:
	  case 39:
	    werror(skip?"- ":"+ ");
	    break;
		
	  default:
	    werror(skip?"-":"+");
	    break;
		
	  case 49:
	    werror(skip?"-\n":"+\n");
	  }
	}
	if(skip) continue;

	int testno;
	sscanf(test, "%s\n%s", type, test);
	sscanf(type, "test %d, expected result: %s", testno, type);

	if (prompt) {
	  if (Stdio.Readline()->
	      read(sprintf("About to run test: %d. [<RETURN>/'quit']: ",
			   testno)) == "quit") {
	    break testloop;
	  }
	}

	if(verbose)
	{
	  werror("Doing test %d (%d total)%s\n", testno, successes+errors+1, extra_info);
	  if(verbose>1) bzot(test);
	}

	if(check > 1) _verify_internals();
	
	shift++;
	string fname = testsuite + ": Test " + testno +
	  " (shift " + (shift%3) + ")";

	string widener = ([ 0:"",
			    1:"\nint \x30c6\x30b9\x30c8=0;\n",
			    2:"\nint \x10001=0;\n" ])[shift%3];

	// widener += "#pragma strict_types\n";

	if(test[-1]!='\n') test+="\n";

	int computed_line=sizeof(test/"\n");
	array gnapp= test/"#";
	for(int e=1;e<sizeof(gnapp);e++)
	{
	  if(sscanf(gnapp[e],"%*d"))
	  {
	    computed_line=0;
	    break;
	  }
	}
	string linetester="int __cpp_line=__LINE__; int __rtl_line=[int]backtrace()[-1][1];\n";

	string to_compile = test + linetester + widener;

	if((shift/6)&1)
	{
	  if(search("don't save parent",to_compile) != -1)
	  {
	    fname+=" (save parent)";
	    to_compile=
	      "#pragma save_parent\n"
	      "# 1\n"
	      +to_compile;
	  }
	}

	if((shift/3)&1)
	{
	  fname+=" (CRNL)";
	  to_compile=replace(to_compile,"\n","\r\n");
	}

	// _optimizer_debug(5);
	   
	if(verbose>9) bzot(to_compile);
	switch(type)
        {
	  mixed at,bt;
	  mixed err;
	case "COMPILE":
	  _dmalloc_set_name(fname,0);
	  if(catch(compile_string(to_compile, fname)))
	  {
	    _dmalloc_set_name();
	    werror("\n" + fname + " failed.\n");
	    bzot(test);
	    errors++;
	  }
	  else {
	    _dmalloc_set_name();
	    successes++;
	  }
	  break;
	    
	case "COMPILE_ERROR":
	  master()->set_inhibit_compile_errors(1);
	  _dmalloc_set_name(fname,0);
	  if(catch(compile_string(to_compile, fname)))
	  {
	    _dmalloc_set_name();
	    successes++;
	  }
	  else {
	    _dmalloc_set_name();
	    werror("\n" + fname + " failed (expected compile error).\n");
	    bzot(test);
	    errors++;
	  }
	  master()->set_inhibit_compile_errors(0);
	  break;
	    
	case "EVAL_ERROR":
	  master()->set_inhibit_compile_errors(1);
	  _dmalloc_set_name(fname,0);

	  at = gauge {
	    err=catch {
	      clone(compile_string(to_compile, fname))->a();
	    };
	  };
	  if(err)
	  {
	    _dmalloc_set_name();
	    successes++;
	    if(verbose>2)
	      werror("Time in a(): %f\n",at);
	  }
	  else {
	    _dmalloc_set_name();
	    werror("\n" + fname + " failed (expected eval error).\n");
	    bzot(test);
	    errors++;
	  }
	  master()->set_inhibit_compile_errors(0);
	  break;
	    
	default:
	  if (err = catch{
	    _dmalloc_set_name(fname,0);
	    o=clone(compile_string(to_compile,fname));
	    _dmalloc_set_name();
	    
	    if(check > 1) _verify_internals();
	    
	    a=b=0;
	    if(t) trace(t);
	    _dmalloc_set_name(fname,1);
	    if(functionp(o->a))
	    {
	      // trace(10);
	      at = gauge { a=o->a(); };
	      // trace(0);
	    }

	    if(functionp(o->b))
	    {
	      bt = gauge { b=o->b(); };
	    }
		  
	    _dmalloc_set_name();

	    if(t) trace(0);
	    if(check > 1) _verify_internals();

	  }) {
	    // trace(0);
	    werror("\n" + fname + " failed.\n");
	    bzot(test);
	    if (arrayp(err) && sizeof(err) && stringp(err[0])) {
	      werror("Error: " + master()->describe_backtrace(err));
	    }
	    if (objectp(err)) {
	      werror("Error: " + master()->describe_backtrace(err));
	    }
	    errors++;
	    break;
	  }

	  if( o->__cpp_line != o->__rtl_line ||
	      ( computed_line && computed_line!=o->__cpp_line))
	    {
	      werror("\n" + fname + " Line numbering failed.\n");
	      bzot(test + linetester);
	      werror("   CPP lines: %d\n",o->__cpp_line);
	      werror("   RTL lines: %d\n",o->__rtl_line);
	      if(computed_line)
		werror("Actual lines: %d\n",computed_line);
	      errors++;
	    }

	  if(verbose>2)
	    werror("Time in a(): %f, Time in b(): %O\n",at,bt);
	    
	  switch(type)
	  {
	  case "FALSE":
	    if(a)
	    {
	      werror("\n" + fname + " failed.\n");
	      bzot(test);
	      werror(sprintf("o->a(): %O\n",a));
	      errors++;
	    }
	    else {
	      successes++;
	    }
	    break;
		
	  case "TRUE":
	    if(!a)
	    {
	      werror("\n" + fname + " failed.\n");
	      bzot(test);
	      werror(sprintf("o->a(): %O\n",a));
	      errors++;
	    }
	    else {
	      successes++;
	    }
	    break;
		
	  case "RUN":
	    successes++;
	    break;

	  case "RUNCT":
	    if(!a || !arrayp(a) || sizeof(a)!=2 || !intp(a[0]) || !intp(a[1])) {
	      werror("\n" + fname + " failed to return proper results.\n");
	      bzot(test);
	      werror(sprintf("o->a(): %O\n",a));
	      errors++;
	    }
	    else {
	      successes += a[0];
	      errors += a[1];
	      if(a[1])
		werror("%d/%d tests failed.\n", a[1], a[0]+a[1]);
	      else
		werror("Did %d tests in %s.\n", a[0], fname);
	    }
	    break;

	  case "EQ":
	    if(a!=b)
	    {
	      werror("\n" + fname + " failed.\n");
	      bzot(test);
	      werror(sprintf("o->a(): %O\n",a));
	      werror(sprintf("o->b(): %O\n",b));
	      errors++;
	    }
	    else {
	      successes++;
	    }
	    break;
		
	  case "EQUAL":
	    if(!equal(a,b))
	    {
	      werror("\n" + fname + " failed.\n");
	      bzot(test);
	      werror(sprintf("o->a(): %O\n",a));
	      werror(sprintf("o->b(): %O\n",b));
	      errors++;
	    }
	    else {
	      successes++;
	    }
	    break;
		
	  default:
	    werror(sprintf("\n%s: Unknown test type (%O).\n", fname, type));
	    errors++;
	  }
	}
	
	if(check > 2) _verify_internals();
	
	if(fail && errors)
	  exit(1);

	if(successes+errors > end)
	{
	  break testloop;
	}
	
	a=b=0;
      }

      if(istty())
      {
	werror("             \r");
      }
      else if(quiet) {
	if(!qskipp && !qmadep);
	else if(!qskipp) werror("Made all tests\n");
	else if(!qmadep) werror("Skipped all tests\n");
	else if(qmade) werror(" Made %d test%s.\n", qmade, qmade==1?"":"s");
	else if(qskipped) werror(" Skipped %d test%s.\n", qskipped, qskipped==1?"":"s");
      }
      else {
	werror("\n");
      }
    }

    if(mem)
    {
      int total;
      tests=0;
      gc();
      mapping tmp=_memory_usage();
      write(sprintf("%-10s: %6s %10s\n","Category","num","bytes"));
      foreach(sort(indices(tmp)),string foo)
      {
	if(sscanf(foo,"%s_bytes",foo))
	{
	  write(sprintf("%-10s: %6d %10d\n",
			foo+"s",
			tmp["num_"+foo+"s"],
			tmp[foo+"_bytes"]));
	  total+=tmp[foo+"_bytes"];
	}
      }
      write( "%-10s: %6s %10d\n",
	     "Total", "", total );
    }
  }
  if(errors || verbose)
  {
    werror("Failed tests: "+errors+".\n");
  }
      
  werror("Total tests: %d  (%d tests skipped)\n",successes+errors,skipped);

#ifdef WATCHDOG_SIGNAL
  if(use_watchdog)
  {
    watchdog->kill(signum("SIGKILL"));
    watchdog->wait();
  }
#endif

#ifdef WATCHDOG_PIPE
  if(use_watchdog)
  {
    destruct(watchdog_pipe);
#if constant(signum)
    catch { watchdog->kill(signum("SIGKILL")); };
#endif
    watchdog->wait();
  }
#endif

  return errors;
}

constant doc = #"
Usage: test_pike [args] [testfiles]

--no-watchdog       Watchdog will not be used.
--watchdog=pid      Run only the watchdog and monitor the process with the given pid.
-h, --help          Prints this message.
-v[level],
--verbose[=level]   Select the level of verbosity. Every verbose level includes the
                    printouts from the levels below.
                    0  No extra printouts.
                    1  Some extra information about test that will or won't be run.
                    2  Every test is printed out.
                    3  Time spent in individual tests are printed out.
                    10 The actual pike code compiled, including wrappers, is printed.
-p, --prompt        The user will be asked before every test is run.
-q, --quiet         Less outputs than normal.
-sX, --start=X      Where in the testsuite testing should start, e.g. ignores X tests
                    in every testsuite.
-eX, --end=X        How many tests should be run.
-f, --fail          If set, the testsuite always fails.
-lX, --loop=X       The number of times the testsuite should be run. Default is 1.
-tX, --trace=X      Run tests with trace level X.
-c[X], --check[=X]  The level of extra pike consistency checks performed.
                    1   _verify_internals is run before every test.
                    2   _verify_internals is run after every compilation.
                    3   _verify_internals is run after every test.
                    4   An extra gc and _verify_internals is run before
                        every test.
                    X<0 For values below zero, _verify_internals will be run before
                        every n:th test, where n=abs(X).
-m, --mem, --memory Print out memory allocations after the tests.
-a, --auto          Let the test program find the testsuits self.
-T, --notty         Format output for non-tty.
-d, --debug         Opens a debug port.
";
