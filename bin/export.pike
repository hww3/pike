#! /usr/bin/env pike

/* $Id: export.pike,v 1.47 2002/04/08 21:46:08 mikael%unix.pp.se Exp $ */

multiset except_modules = (<>);
string vpath;

string dirname(string dir)
{
  array tmp=dir/"/";
  if(tmp[-1]=="") tmp=tmp[..sizeof(tmp)-2];
  tmp=tmp[..sizeof(tmp)-2];
  if(sizeof(tmp)<2) return "/";
  return tmp*"/";
}

array(string) get_files(string path)
{
  array(string) files = get_dir(path);

  if(!getenv("PIKE_EXPORT_CVS_DIRS"))
    files -= ({ "CVS", "RCS", ".cvsignore" });

  array(string) ret = ({});
  foreach(files, string fn)
  {
    if( fn=="core" ) continue;
    if( fn[-1]=='~' ) continue;
    if( fn[0]=='#' && fn[-1]=='#' ) continue;
    if( fn[0]=='.' && fn[1]=='#' ) continue;

    if( path==vpath+"/src/modules" && except_modules[fn] )
      continue;

    if( path==vpath+"/bin" && except_modules[fn] )
      continue;

    if( has_prefix(path, vpath+"/lib/modules") &&
	(except_modules[fn] || except_modules[fn - ".pmod"]))
      continue;

    fn = path+"/"+fn;

    if( Stdio.file_size(fn)==-2 )
      ret += get_files(fn);
    else
      ret += ({ fn });
  }
  return ret;
}

void fix_configure(string dir)
{
  Stdio.Stat config=file_stat(dir+"/configure");
  Stdio.Stat config_in=file_stat(dir+"/configure.in");

  if(config_in)
  {
    if(!config || config_in->mtime > config->mtime)
    {
      werror("Fixing configure in "+dir+".\n");
      Process.create_process( ({"autoconf"}),
			      (["cwd":dir]) )->wait();
    }
  }
}

array(int) getversion()
{
  string s = Stdio.read_file(pike_base_name+"/src/version.h");

  if(!s)
  {
    werror("Failed to read version.h\n");
    werror("cwd=%s  version.h=%s\n", getcwd(), pike_base_name+"/src/version.h");
    exit(1);
  }

  int maj, min, build;

  if ((!sscanf(s, "%*sPIKE_MAJOR_VERSION %d", maj)) ||
      (!sscanf(s, "%*sPIKE_MINOR_VERSION %d", min)) ||
      (!sscanf(s, "%*sPIKE_BUILD_VERSION %d", build))) {

    werror("Failed to get Pike version.\n");
    exit(1);
  }

  return ({ maj, min, build });
}

void bump_version()
{
  werror("Bumping release number.\n");
  Process.create_process( ({ "cvs", "update", "version.h" }),
			  ([ "cwd":pike_base_name+"/src" ]) )->wait();

  string s = Stdio.read_file(pike_base_name+"/src/version.h");
  sscanf(s, "%s PIKE_BUILD_VERSION %d%s", string pre, int rel, string post);
  rel++;
  Stdio.write_file( pike_base_name+"/src/version.h",
		    pre+" PIKE_BUILD_VERSION "+rel+post );
  Process.create_process( ({ "cvs", "commit", "-m",
			     "release number bumped to "+rel+" by export.pike",
			     "version.h" }),
			  ([ "cwd":pike_base_name+"/src" ]) )->wait();
}

array(string) build_file_list(string vpath, string list_file)
{
  array(string) ret=({ });
  foreach(Stdio.FILE(list_file)->line_iterator(1);;mixed line)
    {
      werror("%O\n",line);
      string name=vpath+line;
      if(file_stat(name)->isdir)
	ret += get_files(name);
      else
	ret += ({ name });
    }
  return ret;
}  


string pike_base_name;
string srcdir;
int rebuild;

int main(int argc, array(string) argv)
{
  array(string) files;
  string export_list, filename;
  object cvs;
  int tag, snapshot;

  foreach(Getopt.find_all_options(argv, ({
    ({ "srcdir",    Getopt.HAS_ARG, "--srcdir"     }),
    ({ "rebuild",   Getopt.NO_ARG,  "--rebuild"    }),
    ({ "tag",       Getopt.NO_ARG,  "--tag"        }),
    ({ "help",      Getopt.NO_ARG,  "--help"       }),
    ({ "exportlist",Getopt.HAS_ARG, "--exportlist" }),
    ({ "filename",  Getopt.HAS_ARG, "--name"       })
  }) ),array opt)
    {
      switch(opt[0])
      {
	case "srcdir":
	  srcdir=opt[1];
	  if(basename(srcdir)=="src")
	    srcdir=dirname(srcdir);
	  pike_base_name=".";
	  
	  cd(srcdir);
	  break;
	  
        case "exportlist":
	  export_list=opt[1];
	  break;
	  
        case "filename":
	  filename=opt[1];
	  break;

	case "rebuild":
	  rebuild=1;
	  break;

        case "tag":
	  tag=1;
	  break;
	  
        case "help":
	  write(documentation);
	  return 0;
      }
    }
      

  argv -= ({ 0 });
  except_modules = (multiset)argv[1..];
  if(!srcdir || !export_list || !filename) {
    werror(documentation);
    return 1;
  }

  export_list=srcdir+"/"+export_list;

  if(rebuild)
  {
    werror("Not yet finished!\n");
    exit(1);
    object autoconfig=Process.create_process(({"./run_autoconfig"}),
					     (["cwd":pike_base_name]));
    /* make depend... */
    /* And other things... */
  }

  if(tag && file_stat(pike_base_name+"/CVS"))
  {
    bump_version();

    array(int) version = getversion();
    vpath = sprintf("Pike-v%d.%d.%d", @version);
    string tag = sprintf("v%d_%d_%d", @version);

    werror("Creating tag "+tag+" in the background.\n");
    cvs = Process.create_process( ({"cvs", "tag", "-R", "-F", tag}) );
  }

  mapping m = gmtime(time());
  array(int) version = getversion();
  mapping symbols=([
    "%maj":(string) version[0],
    "%min":(string) version[1],
    "%bld":(string) version[2],
    "%Y":sprintf("%04d",1900+m->year),
    "%M":sprintf("%02d",1+m->mon),
    "%D":sprintf("%02d",m->mday),
    "%h":sprintf("%02d",m->hour),
    "%m":sprintf("%02d",m->min),
    "%s":sprintf("%02d",m->sec)
  ]);
    
  vpath=replace(filename,symbols);

  fix_configure(pike_base_name+"/src");

  foreach(get_dir(pike_base_name+"/src/modules") - ({"CVS","RCS"}), string fn)
    if(Stdio.file_size(pike_base_name+"/src/modules/"+fn) == -2)
      fix_configure("modules/"+fn);

  symlink(".", vpath);

  files=build_file_list(vpath,export_list);

  werror("Creating "+vpath+".tar.gz:\n");

  int first = 1;
  foreach(files/25.0, files)
    {
      if(Process.create_process
	 ( ({"tar",
	     first?"cvf":"rvf",
	     pike_base_name+"/"+vpath+".tar" }) +
	   files)->wait())
      {
	werror("Tar file creation failed!\n");
	if(cvs) cvs->wait();
	rm(vpath);
	exit(1);
      }
      first = 0;
    }

  if(Process.create_process
     ( ({"gzip",
	 "-9",
	 pike_base_name+"/"+vpath+".tar"
     }) )->wait())
    {
      werror("Gzip failed!\n");
      if(cvs) cvs->wait();
      rm(vpath);
      exit(1);
    }

  rm(vpath);
  werror("Done.\n");

  if(cvs && tag)
  {
    cvs->wait();
    bump_version();
  }

  return 0;
}

constant documentation = #"
Usage: export.pike --srcdir=<src> --name=<filename> 
                   --exportlist=<exportlistfile> <except modules>

Creates a pike distribution. Optional arguments:

 rebuild    not implemented
 tag        bump the version and tag the tree
 name       name of exported file (%maj, %min, %bld, %Y, %M, %D, %h, %m, %s
            are replaced with apropiate values).
 exportlist file with list of files and directories to export.
 help       show this text
";
