// $Id: benchmark.pike,v 1.1 2002/12/06 20:00:50 nilsson Exp $

constant help = #"
Benchmarks Pike with %d built in benchmark tests.
Arguments:

-h, --help
  Shows this help text.

-m<number>, --max-runs=<number>
  Runs a test at most <number> of times. Defaults to 25.

-s<number>, --max-seconds=<number>
  Runs a test at most <number> of seconds, rounded up to the closest
  complete test. Defaults to 5.
";

int(0..0) main(int num, array(string) args)
{
   mapping(string:Tools.Shoot.Test) tests=([]);
   array results=({});
   
   foreach (indices(Tools.Shoot);;string test)
   {
      program p;
      Tools.Shoot.Test t;
      if ((programp(p=Tools.Shoot[test])) && 
	  (t=p())->perform)
	 tests[test]=t;
   }

   int seconds_per_test = 5;
   int maximum_runs = 25;

  foreach(Getopt.find_all_options(args, ({
    ({ "help",    Getopt.NO_ARG,  "-h,--help"/"," }),
    ({ "maxrun",  Getopt.HAS_ARG, "-m,--max-runs"/"," }),
    ({ "maxsec",  Getopt.HAS_ARG, "-s,--max-seconds"/"," }),
  })), array opt)
    switch(opt[0])
    {
      case "help":
	write(help, sizeof(tests));
	return 0;
      case "maxrun":
	maximum_runs = (int)opt[1];
	break;
      case "maxsec":
	seconds_per_test = (int)opt[1];
	break;
    }

   Tools.Shoot.ExecTest("Overhead",Tools.Shoot.Overhead())
     ->run(0,1,1); // fill caches

   write("test                        total    user    mem   (runs)\n");

/* always run overhead check first */
   foreach (({"Overhead"})+(sort(indices(tests))-({"Overhead"}));;string id)
   {
      Tools.Shoot.ExecTest(id,tests[id])
	 ->run(seconds_per_test,maximum_runs);
   }
}
