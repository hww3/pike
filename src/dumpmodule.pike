#!/usr/local/bin/pike
/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: dumpmodule.pike,v 1.42 2003/05/31 12:47:39 mast Exp $
*/

int quiet = 1, report_failed = 0, recursive = 0, update = 0;

program p; /* program being dumped */

#ifdef PIKE_FAKEROOT
string fakeroot(string s)
{
  return PIKE_FAKEROOT+combine_path(getcwd(),s);
}
#else
#define fakeroot(X) X
#endif

Tools.Install.ProgressBar progress_bar;

Stdio.File logfile = Stdio.stderr;

int padline = 0;
string next_file = 0;

void logstart (int one_line)
{
  if (padline) logfile->write ("\n");
  if (one_line) {
    logfile->write("#### %s: ", next_file);
    padline = 0;
  }
  else {
    logfile->write("#### %s:\n", next_file);
    padline = 1;
  }
  next_file = 0;
}

void logmsg (mixed... args)
{
  if(logfile) {
    if (next_file) logstart (1);
    if (args) logfile->write (@args);
  }
}

void logmsg_long (mixed... args)
{
  if(logfile) {
    if (next_file) logstart (0);
    if (args) logfile->write (@args);
  }
}

class MyMaster
{
  inherit "/master";

  void handle_error (mixed trace)
  {
    logmsg_long (describe_backtrace (trace));
  }
}

class Handler
{
  void compile_error(string file,int line,string err)
  {
    logmsg_long("%s:%d:%s\n",file,line,err);
  }

  void compile_warning(string file,int line,string err)
  {
    logmsg_long("%s:%d:%s\n",file,line,err);
  }

  int compile_exception (array|object trace)
  {
    if (!objectp (trace) || !trace->is_cpp_error && !trace->is_compilation_error)
      logmsg_long (describe_backtrace (trace));
    return 1;
  }
}

program compile_file(string file, object|void handler)
{
  if(master()->relocate_module) {
    string s = master()->master_read_file(file);
    return master()->compile_string(s,
				    master()->unrelocate_module ?
				    master()->unrelocate_module (file) :
				    file,
				    handler);
  } else
    return master()->compile_file(file, handler);
}

int dumpit(string file, string outfile)
{
  int ok = 0;
  next_file = file;

do_dump: {
    if(Stdio.Stat s=file_stat(fakeroot(file)))
    {
      if (update) {
	if (Stdio.Stat o = file_stat (fakeroot(outfile) + ".o"))
	  if (o->mtime >= s->mtime) {
	    if (!quiet) logmsg ("Up-to-date.\n");
	    ok = 1;
	    break do_dump;
	  }
      }
      rm(fakeroot(outfile)+".o"); // Make sure no old files are left

      if (s->isdir && recursive) {
	if (array(string) dirlist = get_dir (fakeroot (file))) {
	  if (!quiet) logmsg ("Is a directory (dumping recursively).\n");
	  foreach (dirlist, string subfile)
	    if (subfile != "CVS" &&
		(has_suffix (subfile, ".pike") || has_suffix (subfile, ".pmod") ||
		 Stdio.is_dir (file + "/" + subfile)))
	      if (!dumpit (combine_path (file, subfile),
			   combine_path (outfile, subfile)))
		return 0;
	  ok = 1;
	  break do_dump;
	}
	else {
	  logmsg ("Is an unreadable directory (not dumped recursively): %s.\n",
		  strerror (errno()));
	  break do_dump;
	}
      }
      else if (!s->isreg)
      {
	logmsg("Is a directory or special file (not dumped).\n");
	break do_dump;
      }
    }else{
      if(!quiet) logmsg("Does not exist (not dumped).\n");
      break do_dump;
    }

    mixed err;
    if(!(err = catch {
	// Kludge: Resolve the module through master()->resolv since
	// it handles cyclic references better than we do in
	// compile_file above.
	master()->resolv (master()->program_path_to_name (file));

	p=compile_file(file, Handler());

      }) && programp (p))
    {
      if(!p->dont_dump_module && !p->dont_dump_program)
      {
	string s;
	if ((err = catch {
	    s=encode_value(p, master()->Encoder(p));
	    p=decode_value(s,master()->Decoder());
	  }))
	  logmsg_long(describe_backtrace(err));

	else if(programp(p))
	{
	  string dir = combine_path (outfile, "..");
	  if (!Stdio.is_dir (fakeroot(dir)))
	    if (!Stdio.mkdirhier (fakeroot(dir))) {
	      logmsg ("Failed to create target directory %O: %s.\n",
		      dir, strerror (errno()));
	      break do_dump;
	    }

	  Stdio.File(fakeroot(outfile)+".o","wct")->write(s);
	  ok = 1;
	  if(!quiet) logmsg("Dumped.\n");
	}

	else if(!quiet)
	  logmsg("Decode of %O failed (not dumped).\n", file);
      }

      else if(!quiet)
	logmsg("Not dumping %O (not dumped).\n", file);
    }

    else {
      // This should never happen. If it does then it's not safe to
      // continue dumping since later modules might do #if constant(...)
      // on something for modifiers in this one and would then be dumped
      // incorrectly without errors.
      if (err && (!objectp (err) || !err->is_compilation_error || !err->is_cpp_error))
	// compile() should never throw any other error, but we play safe.
	logmsg_long("Compilation of %O failed (not dumped):\n%s",
		    file, describe_backtrace(err));
      else
	logmsg("Compilation of %O failed (not dumped).\n", file);
      if (report_failed)
	write ("Aborting dumping since %s didn't compile\n", file);
      return 0;
    }
  }

  if (report_failed && !ok)
    write ("Dumping failed for %s\n", file);
  return 1;
}

int main(int argc, array(string) argv)
{
  string target_dir = 0;
  string update_stamp = 0;

  replace_master (MyMaster());

  foreach (Getopt.find_all_options (argv, ({
    ({"log-file", Getopt.MAY_HAVE_ARG, ({"-l", "--log-file"})}),
    ({"verbose", Getopt.NO_ARG, ({"-v", "--verbose"})}),
    ({"quiet", Getopt.NO_ARG, ({"-q", "--quiet"})}), // The default.
    ({"distquiet", Getopt.NO_ARG, ({"--distquiet"})}),
    ({"progress-bar", Getopt.HAS_ARG, ({"--progress-bar"})}),
    ({"report-failed", Getopt.NO_ARG, ({"--report-failed"})}),
    ({"recursive", Getopt.NO_ARG, ({"-r", "--recursive"})}),
    ({"target-dir", Getopt.HAS_ARG, ({"-t", "--target-dir"})}),
    ({"update-only", Getopt.MAY_HAVE_ARG, ({"-u", "--update-only"})}),
  })), array opt)
    switch (opt[0]) {

      case "log-file":
	logfile = Stdio.File(stringp (opt[1]) && opt[1] || "dumpmodule.log","caw");
	/* Redirect all debug and error messages to the logfile. */
	logfile->dup2(Stdio.stderr);
	break;

      case "verbose":
	quiet = 0;
	break;

      case "quiet":
	quiet=1;
	break;

      case "distquiet":
	quiet=1;
	logfile=0;
	break;

      case "progress-bar":
	quiet = 1;

	progress_bar = Tools.Install.ProgressBar("Precompiling",
						 @array_sscanf(opt[1], "%d,%d"),
						 0.2, 0.8);
	break;

      case "report-failed":
	report_failed = 1;
	break;

      case "recursive":
	recursive = 1;
	break;

      case "target-dir":
	target_dir = opt[1];
	break;

      case "update-only":
	if (stringp (opt[1])) {
	  update_stamp = opt[1];
	  update = Stdio.read_file (update_stamp) == version();
	}
	else
	  update = 1;
	break;
    }

  // Remove the name of the program.
  argv = argv[1..];

  argv=Getopt.get_args(argv);

#if 0
  // Hack to get Calendar files to compile in correct order.
  object tmp = Calendar;
#endif

  foreach(argv, string file)
  {
    if(progress_bar)
      progress_bar->update(1);

    string outfile = file;
    if (target_dir) {
#ifdef __NT__
      outfile = replace (outfile, "\\", "/");
#endif
      outfile = combine_path (target_dir, ((outfile / "/") - ({""}))[-1]);
    }

    if (!dumpit(file, outfile)) break;
  }

  if (update_stamp)
    Stdio.write_file (update_stamp, version());
}
