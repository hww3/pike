#pike __REAL_VERSION__

#define error(X) throw( ({ (X), backtrace()[0..sizeof(backtrace())-2] }) )

import Stdio;

#if !constant(strerror)
#define strerror(X) ("errno="+X)
#endif

constant create_process = __builtin.create_process;

class Process
{
  inherit __builtin.create_process;

  void create( string|array(string) args, mapping m )
  {
    if( stringp( args ) )
      args = split_quoted_string( args );
    if( m )
      ::create( args, m );
    else
      ::create( args );
  }
}

int exec(string file,string ... foo)
{
  if (sizeof(file)) {
    string path;

    if(search(file,"/") >= 0)
      return exece(combine_path(getcwd(),file),foo,getenv());

    path=getenv("PATH");

    foreach(path ? path/":" : ({}) , path)
      if(file_stat(path=combine_path(path,file)))
	return exece(path, foo,getenv());
  }
  return 69;
}

static array(string) search_path_entries=0;
string search_path(string command)
{
   if (command=="" || command[0]=='/') return command;

   if (!search_path_entries) 
   {
      array(string) e=(getenv("PATH")||"")/":"-({""});

      multiset(string) filter=(<>);
      search_path_entries=({});
      foreach (e,string s)
      {
	 string t;
	 if (s[0]=='~')  // some shells allow ~-expansion in PATH
	 {
	    if (s[0..1]=="~/" && (t=getenv("HOME"))) 
	       s=t+s[1..];
	    else
	    {
	       // expand user?
	    }
	 }
	 if (!filter[s] /* && directory exist */ ) 
	 {
	    search_path_entries+=({s}); 
	    filter[s]=1;
	 }
      }
   }
   foreach (search_path_entries, string path)
   {
      string p=combine_path(path,command);
      Stdio.Stat s=file_stat(p);
      if (s && s->mode&0111) return p;
   }
   return 0;
}

string sh_quote(string s)
{
  return replace(s,
	({"\\", "'", "\"", " "}),
	({"\\\\", "\\'", "\\\"","\\ "}));
}

array(string) split_quoted_string(string s)
{
  sscanf(s,"%*[ \n\t]%s",s);
  s=replace(s,
	    ({"\"",  "'",  "\\",  " ",  "\t",  "\n"}),
	    ({"\0\"","\0'","\0\\","\0 ","\0\t","\0\n"}));
  array(string) x=s/"\0";
  array(string) ret=({x[0]});

  for(int e=1;e<sizeof(x);e++)
  {
    switch(x[e][0])
    {
      case '"':
      ret[-1]+=x[e][1..];
      while(x[++e][0]!='"')
      {
	if(strlen(x[e])==1 && x[e][0]=='\\' && x[e+1][0]=='"') e++;
	ret[-1]+=x[e];
      }
      ret[-1]+=x[e][1..];
      break;

      case '\'':
      ret[-1]+=x[e][1..];
      while(x[++e][0]!='\'') ret[-1]+=x[e];
      ret[-1]+=x[e][1..];
      break;
      
      case '\\':
      if(strlen(x[e])>1)
      {
	ret[-1]+=x[e][1..];
      }else{
	ret[-1]+=x[++e];
      }
      break;
      
      case ' ':
      case '\t':
      case '\n':
	while(strlen(x[e])==1)
	{
	  if(e+1 < sizeof(x))
	  {
	    if((<' ','\t','\n'>) [x[e+1][0]])
	      e++;
	    else
	      break;
	  }else{
	    return ret;
	  }
	}
	ret+=({x[e][1..]});
      break;

      default:
	ret[-1]+="\0"+x[e];
	break;
    }
  }
  return ret;
}

Process spawn(string s,object|void stdin,object|void stdout,object|void stderr,
	      function|void cleanup, mixed ... args)
{
#if 1
  mapping data=(["env":getenv()]);
  if(stdin) data->stdin=stdin;
  if(stdout) data->stdout=stdout;
  if(stderr) data->stderr=stderr;
#if defined(__NT__) || defined(__amigaos__)
  return create_process(split_quoted_string(s),data);
#else /* !__NT__||__amigaos__ */
  return create_process(({ "/bin/sh", "-c", s }),data);
#endif /* __NT__||__amigaos__ */
#else

  Process pid;

#if constant(fork)
  pid=fork();
#endif
  
  if(pid)
  {
    return pid;
  }else{
    if(stdin ) stdin ->dup2(File("stdin"));
    if(stdout) stdout->dup2(File("stdout"));
    if(stderr) stderr->dup2(File("stderr"));

    if(stdin ) destruct(stdin);
    if(stdout) destruct(stdout);
    if(stderr) destruct(stderr);

    if (cleanup) {
      cleanup(@args);
    }

    exec("/bin/sh","-c",s);
    exit(69);
  }
#endif
}

string popen(string s)
{
  object p;
  string t;
  object f = File();

  if (!f) error("Popen failed. (couldn't create pipe)\n");

  p=f->pipe(Stdio.PROP_IPC);
  if(!p) error("Popen failed. (couldn't create pipe)\n");
  spawn(s,0,p,0, destruct, f);
  p->close();
  destruct(p);

  t=f->read(0x7fffffff);
  if(!t)
  {
    int e;
    e=f->errno();
    f->close();
    destruct(f);
    error("Popen failed with error "+e+".\n");
  } else {
    f->close();
    destruct(f);
  }
  return t;
}

int system(string s)
{
  return spawn(s)->wait();
}

#ifndef __NT__
#if constant(fork)
constant fork = predef::fork;
#endif

#if constant(exece)
constant exece = predef::exece;
#endif

#if constant(fork)
class Spawn
{
   object stdin;
   object stdout;
   object stderr;
   array(object) fd;

   object pid;

   private object low_spawn(array(void|object(Stdio.File)) fdp,
			    array(void|object(Stdio.File)) fd_to_close,
			    string cmd, void|array(string) args, 
			    void|mapping(string:string) env, 
			    string|void cwd)
   {
      object(Stdio.File) pie,pied; /* interprocess communication */
      object pid;

      pie=Stdio.File();
      pied=pie->pipe();

      if(!(pid=fork()))
      {
	 mixed err=catch
	 {
	    if(cwd && !cd(cwd))
	    {
	       throw(({"pike: cannot change cwd to "+cwd+
		       ": "+strerror(errno())+"\n",
		       backtrace()}));
	    }

	    if (sizeof(fdp)>0 && fdp[0]) fdp[0]->dup2(Stdio.File("stdin"));
	    if (sizeof(fdp)>1 && fdp[1]) fdp[1]->dup2(Stdio.File("stdout"));
	    if (sizeof(fdp)>2 && fdp[2]) fdp[2]->dup2(Stdio.File("stderr"));
	    /* dup2 fdd[3..] here FIXME FIXME */
	    foreach (fd_to_close,object f) 
	       if (objectp(f)) { f->close(); destruct(f); }
	    pie->close();
	    destruct(pie);
	   
	    pied->set_close_on_exec(1);

	    if (env) 
	       exece(cmd,args||({}),env);
	    else 
	       exece(cmd,args||({}));

	    throw(({"pike: failed to exece "+cmd+
		    ": "+strerror(errno())+"\n",
		    backtrace()}));
	 };

	 pied->write(encode_value(err));
	 exit(1);
      }

      foreach (fdp,object f) if (objectp(f)) { f->close(); destruct(f); }

      pied->close();
      destruct(pied);

      mixed err=pie->read();
      if (err && err!="") throw(decode_value(err));

      pie->close();
      destruct(pie);

      return pid;
   }

   void create(string cmd,
	       void|array(string) args,
	       void|mapping(string:string) env,
	       string|void cwd,
	       void|array(object(Stdio.File)|void) ownpipes,
	       void|array(object(Stdio.File)|void) fds_to_close)
   {
      if (!ownpipes)
      {
	 stdin=Stdio.File();
	 stdout=Stdio.File();
	 stderr=Stdio.File();
	 fd=({stdin->pipe(),stdout->pipe(),stderr->pipe()});
	 fds_to_close=({stdin,stdout,stderr});
      }
      else
      {
	 fd=ownpipes;
	 if (sizeof(fd)>0) stdin=fd[0]; else stdin=0;
	 if (sizeof(fd)>1) stdout=fd[1]; else stdout=0;
	 if (sizeof(fd)>2) stderr=fd[2]; else stderr=0;
      }
      pid=low_spawn(fd,fds_to_close||({}),cmd,args,env,cwd);
   }

#if constant(kill)
   int kill(int signal) 
   { 
      return predef::kill(pid,signal); 
   }
#endif

   int wait()
   {
     return pid->wait();
   }

   // void set_done_callback(function foo,mixed ... args);
   // int result();
   // array rusage();
}
#endif
#endif
