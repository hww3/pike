#!/usr/local/bin/pike

int quiet=0;
program p; /* program being dumped */

#ifdef PIKE_FAKEROOT
string fakeroot(string s)
{
  return PIKE_FAKEROOT+combine_path(getcwd(),s);
}
#else
#define fakeroot(X) X
#endif

class ProgressBar
{
  private constant width = 45;

  private float phase_base, phase_size;
  private int max, cur;
  private string name;

  void set_current(int _cur)
  {
    cur = _cur;
  }

  void set_phase(float _phase_base, float _phase_size)
  {
    phase_base = _phase_base;
    phase_size = _phase_size;
  }
  
  void update(int increment)
  {
    cur += increment;
    cur = min(cur, max);
    
    float ratio = phase_base + ((float)cur/(float)max) * phase_size;
    if(1.0 < ratio)
      ratio = 1.0;
    
    int bar = (int)(ratio * (float)width);
    int is_full = (bar == width);
    
    write("\r   %-13s |%s%c%s%s %4.1f %%  ",
	  name+":",
	  "="*bar,
	  is_full ? '|' : ({ '\\', '|', '/', '-' })[cur & 3],
	  is_full ? "" : " "*(width-bar-1),
	  is_full ? "" : "|",
	  100.0 * ratio);
  }

  void create(string _name, int _cur, int _max,
	      float|void _phase_base, float|void _phase_size)
    /* NOTE: max must be greater than zero. */
  {
    name = _name;
    max = _max;
    cur = _cur;
    
    phase_base = _phase_base || 0.0;
    phase_size = _phase_size || 1.0 - phase_base;
  }
}

ProgressBar progress_bar;

#define error(X) throw( ({ (X), backtrace() }) )

mapping function_names=([]);

class Codec
{
  string last_id;

  string nameof(mixed x)
  {
//    if(logfile) logfile->write("%O\n",x);
//    werror("%O\n",x);
    if(p!=x)
      if(mixed tmp=function_names[x])
	return tmp;

    switch(sprintf("%t",x))
    {
      case "program":
	if(p!=x)
	{
	  if(mixed tmp=search(master()->programs,x))
	    return tmp;
#if 0
	  if(mixed tmp=search(values(_static_modules), x)!=-1)
	  {
	    return "_static_modules."+(indices(_static_modules)[tmp]);
	  }
#endif
	}
	break;

      case "object":
	if(program p=search(master()->objects,x))
	{
	  if(string tmp=search(master()->programs,p))
	  {
	    return tmp;
	  }else{
#if 0
	    werror("Completely failed to find this program:\n");
	    _describe(p);
#endif
	  }
	}
	if(object_program(x) == master()->dirnode)
	{
	  /* FIXME: this is a bit ad-hoc */
	  string dirname=x->dirname;
	  dirname-=".pmod";
	  sscanf(dirname,"%*slib/modules/%s",dirname);
	  dirname=replace(dirname,"/",".");
	  if(master()->resolv(dirname) == x)
	    return "resolv:"+dirname;
	}
	break;
    }
    return ([])[0];
  } 

  function functionof(string x)
  {
    if(sscanf(x,"efun:%s",x))
      return all_constants()[x];

    if(sscanf(x,"resolv:%s",x))
      return master()->resolv(x);

    werror("Failed to decode %s\n",x);
    return 0;
  }


  object objectof(string x)
  {
    if(sscanf(x,"efun:%s",x))
      return all_constants()[x];

    if(sscanf(x,"resolv:%s",x))
      return master()->resolv(x);

    if(object tmp=(object)x) return tmp;
    werror("Failed to decode %s\n",x);
    return 0;
    
  }

  program programof(string x)
  {
    if(sscanf(x,"efun:%s",x))
      return all_constants()[x];

    if(sscanf(x,"resolv:%s",x))
      return master()->resolv(x);

    if(sscanf(x,"_static_modules.%s",x))
    {
      return (program)_static_modules[x];
    }

    if(program tmp=(program)x) return tmp;
    werror("Failed to decode %s\n",x);
    return 0;
  }

  mixed encode_object(object x)
  {
    if(x->_encode) return x->_encode();
//    if(logfile)
//      logfile->write("Cannot encode objects yet: %s\n",master()->stupid_describe(x,100000));
#if 0
    werror("\n>>>>>>encode object was called for:<<<<<<\n");
    _describe(x);
    werror("\n");
#endif
    error("Cannot encode objects yet.\n");
//    error(sprintf("Cannot encode objects yet. %O\n",indices(x)));
  }

  mixed decode_object(program p, mixed data)
  {
    object ret=p(@data[0]);
    if(sizeof(data)>1) ret->_decode(data[1]);
    error("Cannot encode objects yet.\n");
  }
}

Stdio.File logfile;

class Handler
{
  void compile_error(string file,int line,string err)
    {
      if(!logfile) return;
      logfile->write(sprintf("%s:%d:%s\n",file,line,err));
    }

  void compile_warning(string file,int line,string err)
    {
      if(!logfile) return;
      logfile->write(sprintf("%s:%d:%s\n",file,line,err));
    }
}

void dumpit(string file)
{
  if(logfile)
    logfile->write("##%s##\n",file);

  if(!quiet)
    werror(file +": ");
  
  mixed err=catch {
    rm(file+".o"); // Make sure no old files are left
    if(mixed s=file_stat(fakeroot(file)))
    {
      if(s[1]<=0)
      {
	werror("is a directory or special file.\n");
	break;
      }
    }else{
      if(!quiet)
      werror("does not exist.\n");
      break;
    }
    if(programp(p=compile_file(file, Handler())))
    {
      string s=encode_value(p, Codec());
      p=decode_value(s,master()->Codec());
      if(programp(p))
      {
	Stdio.File(fakeroot(file) + ".o","wct")->write(s);
	switch(quiet)
	{
	  case 1: werror("."); break;
	  case 0: werror("dumped.\n");
	}
      }else{
	switch(quiet)
	{
	  case 1: werror("i"); break;
	  case 0: werror("Decode of %O failed.\n", file);
	}
      }
    }else{
      switch(quiet)
      {
	case 1: werror("!"); break;
	case 0: werror("Compilation of %O failed.\n", file);
      }
    }
  };
  if(err)
  {
#ifdef ERRORS
    err[0]="While dumping "+file+": "+err[0];
    werror(master()->describe_backtrace(err));
#else
    if(quiet)
    {
      if(quiet<2)
	werror("X");
      if(logfile)
      {
//	err[0]="While dumping "+file+": "+err[0];
//	logfile->write("================================================\n");
	logfile->write(master()->describe_backtrace(err));
      }
    }else{
      werror(err[0]);
    }
#endif
  }
}

int main(int argc, string *argv)
{
  /* Redirect all debug and error messages to a logfile. */
  Stdio.File("dumpmodule.log", "caw")->dup2(Stdio.stderr);
  
  foreach( (array)all_constants(), [string name, mixed func])
    function_names[func]="efun:"+name;

  function_names[Stdio.stdin]="resolv:Stdio.stdin";
  function_names[Stdio.stdout]="resolv:Stdio.stdout";
  function_names[Stdio.stderr]="resolv:Stdio.stderr";
  function_names[_static_modules.Builtin]="resolv:_";

  // Remove the name of the program.
  argv = argv[1..];
  
  if(argv[0]=="--quiet")
  {
    quiet=1;
    argv=argv[1..];

    // FIXME: Make this a command line option..
    // It should not be done when running a binary dist
    // installation...
    logfile=Stdio.File("dumpmodule.log","caw");
//    werror("Dumping modules ");
  }

  if(argv[0]=="--distquiet")
  {
    quiet=2;
    argv=argv[1..];
    logfile=0;
  }

  if(argv[0] == "--progress-bar")
  {
      quiet = 2;
      logfile = Stdio.File("dumpmodule.log","caw");
      
      progress_bar = ProgressBar("Precompiling",
				 @array_sscanf(argv[1], "%d,%d"),
				 0.2, 0.8);
      
      argv = argv[2..];
  }

  foreach(argv, string file)
  {
    if(progress_bar)
      progress_bar->update(1);
      
    dumpit(file);
  }

  if(quiet==1)
    werror("\n");
}
