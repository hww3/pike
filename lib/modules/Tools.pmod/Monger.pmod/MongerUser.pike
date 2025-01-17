// -*- Pike -*-

// $Id$

#pike __REAL_VERSION__

constant version =
 sprintf("%d.%d.%d",(int)__REAL_VERSION__,__REAL_MINOR__,__REAL_BUILD__);
constant description = "Monger: the Pike module manger.";

string repository = "http://modules.gotpike.org:8000/xmlrpc/index.pike";
string builddir = getenv("HOME") + "/.monger";

int use_force=0;
int use_local=0;
int show_urls=0;
string my_command;
string argument;
string my_version;
string run_pike;
array(string) pike_args = ({});
string original_dir;

mapping created = (["files" : ({}), "dirs": ({})]);

int main(int argc, array(string) argv)
{
  run_pike = master()->_pike_file_name;
#ifdef NOT_INSTALLED
  pike_args += ({ "-DNOT_INSTALLED" });
#endif
#ifdef PRECOMPILED_SEARCH_MORE
  pike_args += ({ "-DPRECOMPILED_SEARCH_MORE" });
#endif
  if(master()->_master_file_name)
    pike_args += ({ "-m"+master()->_master_file_name });
  putenv("RUNPIKE", run_pike+sizeof(pike_args)?" "+pike_args*" ":"");

  if(argc==1) return do_help();

  array opts = Getopt.find_all_options(argv,aggregate(
    ({"list",Getopt.NO_ARG,({"--list"}) }),
    ({"download",Getopt.NO_ARG,({"--download"}) }),
    ({"query",Getopt.NO_ARG,({"--query"}) }),
    ({"show_urls",Getopt.NO_ARG,({"--show-urls"}) }),
    ({"repository",Getopt.HAS_ARG,({"--repository"}) }),
    ({"builddir",Getopt.HAS_ARG,({"--builddir"}) }),
    ({"install",Getopt.NO_ARG,({"--install"}) }),
    ({"local",Getopt.NO_ARG,({"--local"}) }),
    ({"version",Getopt.HAS_ARG,({"--version"}) }),
    ({"force",Getopt.NO_ARG,({"--force"}) }),
    ({"help",Getopt.NO_ARG,({"--help"}) }),
    ));

  argv=Getopt.get_args(argv);

  if(sizeof(argv)>2) 
    exit(1, "Too many extra arguments!\n");
  else if(sizeof(argv)>1)
    argument = argv[1];

  foreach(opts,array opt)
  {
    switch(opt[0])
    {
      case "repository":
        repository = opt[1];
        break;
      case "show_urls":
	show_urls = 1;
        break;
      case "builddir":
        Stdio.Stat fs = file_stat(opt[1]);
        if(fs && fs->isdir)
          builddir = opt[1];
        else
          exit(1, "Build directory " + opt[1] + " does not exist.\n");
        break;
      case "version":
        my_version = opt[1];
        break;
      case "force":
        use_force = 1;
        break;
      case "local":
        use_local = 1;
        break;
    }
  }

  // create the build directory if it doesn't exist.
  Stdio.Stat fs = file_stat(builddir);
  if(!fs)
    mkdir(builddir);
  else if (!fs->isdir)
    exit(1, "Build directory " + builddir + " is not a directory.\n");
  
  foreach(opts,array opt)
  {
    switch(opt[0])
    {
      case "help":
        return do_help();
        break;
      case "list":
        my_command=opt[0];
        if(argument && strlen(argument))
          do_list(argument);
        else
          do_list();
        break;
      case "download":
        my_command=opt[0];
        if(argument)
          do_download(argument, my_version||UNDEFINED);
        else
          exit(1, "download error: module name must be specified\n");
        break;
      case "install":
        my_command=opt[0];
        if(argument)
          do_install(argument, my_version||UNDEFINED);
        else
          exit(1, "install error: module name must be specified\n");
        break;
      case "query":
        my_command=opt[0];
        if(argument)
          do_query(argument, my_version||UNDEFINED);
        else
          exit(1, "query error: module name must be specified\n");
        break;
    }
  }

 return 0;
}

int(0..0) do_help()
{
  write(
#"This is Monger, the manager for Fresh Pike.

Usage: pike -x monger [options] modulename

--list               list known modules in the repository, optionally
                       limited to those whose name contains the last 
                       argument in the argument list (modulename)
--query              retrieves information about module modulename
--show-urls          display download and other urls in query view
--download           download the module modulename
--install            install the module modulename
--repository=url     sets the repository source to url
--builddir=path      sets the build directory to path, default is 
                       the $HOME/.monger
--force              force the performance of an action that might 
                       otherwise generate a warning
--local              perform installation in a user's local rather than 
                       system directory
--version=ver        work with the specified version of the module
");
  return 0;
}


void do_query(string name, string|void version)
{
  mapping vi = get_module_action_data(name, version);
  if( !vi ) return;

  write("%s: %s\n", vi->name, vi->description);
  write("Author/Owner: %s\n", vi->owner);
  write("Version: %s (%s)\t", vi->version, vi->version_type);
  write("License: %s\n", vi->license);
  write("Changes: %s\n\n", vi->changes);
  if(vi->download && show_urls)
  {
    if(stringp(vi->download))
      write("Download URL: %s\n\n", vi->download);
    else if(arrayp(vi->download))
      foreach(vi->download;;string u)
          write("Download URL: %s\n\n", u);
      
  }
  if(vi->download)
    write("This module is available for automated installation.\n");

  catch 
  {
    string local_ver = master()->resolv(name)["__version"];

    if(local_ver) write("Version %s of this module is currently installed.\n", 
                        local_ver);
  };
}

mapping get_module_action_data(string name, string|void version)
{
  int module_id;
  string dv;
  mixed err;

  string pike_version = 
    sprintf("%d.%d.%d", (int)__REAL_MAJOR__, 
      (int)__REAL_MINOR__, (int)__REAL_BUILD__);

  object x = xmlrpc_handler(repository);

  err = catch {
    module_id = x->get_module_id(name);
  };
  if(err)
  {
    werror("An error occurred: %s", err[0]);
    return 0;
  }

  string v;

  mapping info = x->get_module_info((int)module_id);

  err = catch {
    v = x->get_recommended_module_version((int)module_id, pike_version);
  };

  if(err)
    write("an error occurred while getting the recommended version.\n");

  info->version_type="recommended";

  if(version && version != v)
    info->version_type="not recommended";

  if(version && use_force)
  {
    dv=my_version;
  }
  else if(version && version!=v)
  {
    write("Requested version %s is not the recommended version.\n"
          "use --force to force %s of this version.\n", 
          version, my_command);
    exit(1);
  }
  else if(version)
  {
    write("Selecting requested and recommended version %s for %s.\n", 
          v, my_command);
    dv=version;
  }
  else if(v)
  {
    write("Selecting recommended version %s for %s.\n", v, my_command);
    dv=v;
  }
  else
    exit(1, "repository error: no recommended version to %s.\n"
	 "use --force --version=ver to force %s of a particular version.\n",
         my_command, my_command);

  mapping vi = x->get_module_version_info((int)module_id, dv);

  return vi + info;
}

void do_download(string name, string|void version)
{
  mapping vi = get_module_action_data(name, version);

  if(vi->download)
  {
    write("beginning download of version %s...\n", vi->version);
    array rq;
    if(arrayp(vi->download))
      foreach(vi->download;; string u)
      {
        rq = Protocols.HTTP.get_url_nice(u);
        if(rq) break;  
      }
    else
      rq = Protocols.HTTP.get_url_nice(vi->download);
    if(!rq) 
      exit(1, "download error: unable to access download url\n");
    else
    {
      Stdio.write_file(vi->filename, rq[1]);
      write("wrote module to file %s (%d bytes)\n", vi->filename, sizeof(rq[1]));
    }
  }
  else 
    exit(1, "download error: no download available for this module version.\n");
}

void do_install(string name, string|void version)
{
  int res;

  mapping vi = get_module_action_data(name, version);

  if(vi->download)
  {
    original_dir = getcwd();
    cd(builddir);

    write("beginning download of version %s...\n", vi->version);
    array rq;
    if(arrayp(vi->download))
      foreach(vi->download;; string u)
      {
        rq = Protocols.HTTP.get_url_nice(u);
        if(rq) break;  
      }
    else
      rq = Protocols.HTTP.get_url_nice(vi->download);
    if(!rq) 
      exit(1, "download error: unable to access download url\n");
    else
    {
      Stdio.write_file(vi->filename, rq[1]);
      write("wrote module to file %s (%d bytes)\n", vi->filename, sizeof(rq[1]));
    }
  }
  else 
    exit(1, "install error: no download available for this module version.\n");

  // now we should uncompress the file.
  string fn;

  if((vi->filename)[sizeof(vi->filename)-3..] == ".gz")
  {
    fn = (vi->filename)[0.. sizeof(vi->filename)-4];
    write("uncompressing...%s\n", vi->filename);
    if(!Process.search_path("gzip"))
      exit(1, "install error: no gzip found in PATH.\n");
    else
      res = Process.system("gzip -f -d " + vi->filename);

    if(res)
      exit(1, "install error: uncompress failed.\n");

  }
  else fn = vi->filename;

  created->file += ({ fn });

  werror("working with tar file " + fn + "\n");

  if(!Process.search_path("tar"))
    exit(1, "install error: no tar found in PATH.\n");
  else
    res = Process.system("tar xvf " + fn);

  if(res)
    exit(1, "install error: untar failed.\n");
  else
    created->dirs += ({fn[0..sizeof(fn)-5]});  


  // change directory to the module
  cd(combine_path(builddir, fn[0..sizeof(fn)-5]));

  // now, build the module.

  array jobs = ({"", "verify", (use_local?"local_":"") + "install"});

  object builder;

  foreach(jobs, string j)
  {
    if(j == "install")
    {
      uninstall(name, 0);
    }
    else if(j=="local_install")
    {
      uninstall(name, 1);
    }

    write("\nRunning %O in %O\n\n", run_pike+" "+pike_args*" "+" -x module "+j,
	  getcwd());
    builder = Process.create_process(
      ({run_pike}) + pike_args + ({"-x", "module"}) + (({ j }) - ({""})));
    res = builder->wait();
  
    if(res)
    {
      werror("install error: make %s failed.\n\n", j);

      werror("the following files have been preserved in %s:\n\n%s\n\n", 
             builddir, created->file * "\n");

      werror("the following directories have been preserved in %s:\n\n%s\n\n", 
             builddir, created->dirs * "\n");

      exit(1);
    }
    else
    {
       werror("make %s successful.\n", j);
    }
  }

  // now we should clean up our mess.
  foreach(created->file, string file)
  {
    write("cleaning up %s\n", file);
    rm(file);
  }

  foreach(created->dirs, string dir)
  {
    write("removing directory %s\n", dir);
    Stdio.recursive_rm(dir);
  }

  cd(original_dir);
}

void do_list(string|void name)
{
  array results;

  object x = xmlrpc_handler(repository);

  if(name && strlen(name))
  {
    results=x->get_modules_match(name);
  }
  else
    results=x->get_modules();

#if 0
  if(name && strlen(name))
    write("Found %d modules in the repository:\n\n", 
      sizeof(results[0]));
  else
    write("Found %d modules with a name containing %s:\n\n", 
      sizeof(results[0]), name);

#endif

  foreach(results, string r)
  {
    write("%s\n", r);
  }
}

array find_components(string name, int|void _local)
{
  string local_ver;
  array components;

  catch
  {
    local_ver = master()->resolv(name)["__version"];

    if(local_ver) write("Version %s of this module is currently installed.\n",
                        local_ver);

    components = master()->resolv(name)["__components"];
  };

  return ({local_ver, components||({})});
}

int uninstall(string name, int|void _local)
{
  string local_ver;
  array components;

  [local_ver, components] = find_components(name, _local);

  if(!local_ver) return 0;
  if(!components)
  {
    werror("no components element found for this module. Unable to reliably uninstall.\n");
    return 0;
  }
  
  low_uninstall(components, _local);
  
  return 1;
}

void low_uninstall(array components, int _local)
{    
  string dir;

  if(_local)
  {
    dir = combine_path(getenv("HOME"), "lib/pike/modules");
  }
  else
  {
    dir = master()->system_module_path[-1];
  }

  array elems = reverse(Array.sort_array(components));
  
  foreach(elems;; string comp)
  {
    object s = file_stat(Stdio.append_path(dir, comp));
    if(!s)
    {
      werror("warning: %s does not exist.\n", comp);
      continue;
    }
    string path = Stdio.append_path(dir, comp);
    
    werror("deleting: " + path + " [%s]\n", (s->isdir?"dir":"file"));
    rm(path);
  }
}

class xmlrpc_handler
{
  Protocols.XMLRPC.Client x;

  void create(string loc)
  {
    x = Protocols.XMLRPC.Client(loc);
  }
 

  protected class _caller (string n){

    mixed `()(mixed ... args)
    {
      array|Protocols.XMLRPC.Fault r;
      if(args)
	r = x[n](@args);
      else
	r = x[n]();
      if(objectp(r)) // we have an error, throw it.
        error(r->fault_string);
      else return r[0];
    }

  }

  function `->(string n, mixed ... args)
  {
    return _caller(n);
  }

}
