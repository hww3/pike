#!/usr/local/bin/pike

/* $Id: export.pike,v 1.43 2002/04/07 13:25:11 nilsson Exp $ */

import Stdio;

multiset except_modules  =(<>);
string vpath;


string dirname(string dir)
{
  array tmp=dir/"/";
  if(tmp[-1]=="") tmp=tmp[..sizeof(tmp)-2];
  tmp=tmp[..sizeof(tmp)-2];
  if(sizeof(tmp)<2) return "/";
  return tmp*"/";
}

string *get_files(string path)
{
  string *files,tmp,*ret;
  files=get_dir(path);

  // if (!files) return ({});

  if(!getenv("PIKE_EXPORT_CVS_DIRS"))
    files-=({"CVS","RCS",".cvsignore"});

  ret=({});
  foreach(files,tmp)
  {
    if(tmp=="core") continue;
    if(tmp[-1]=='~') continue;
    if(tmp[0]=='#' && tmp[-1]=='#') continue;
    if(tmp[0]=='.' && tmp[1]=='#') continue;

    if(path==vpath+"/src/modules" && except_modules[tmp])
      continue;

    if(path==vpath+"/bin" && except_modules[tmp])
      continue;

    if(search(path,vpath+"/lib/modules")==0 &&
       (except_modules[tmp] || except_modules[tmp - ".pmod"]))
      continue;

    tmp=path+"/"+tmp;

    if(file_size(tmp)==-2)
    {
      ret+=get_files(tmp);
    }else{
      ret+=({tmp});
    }
  }
  return ret;
}

void fix_configure(string dir)
{
  mixed config,config_in;
  config=file_stat(dir+"/configure");
  config_in=file_stat(dir+"/configure.in");

  if(config_in)
  {
    if(!config || config_in[3] > config[3])
    {
      werror("Fixing configure in "+dir+".\n");
      Process.create_process(({"autoconf"}),(["cwd":dir]))->wait();
    }
  }
}

string getversion()
{
//  werror("FNORD: %O  %O\n",getcwd(),pike_base_name+"/src/version.h");
  string s=Stdio.read_file(pike_base_name+"/src/version.h");

  if(!s)
  {
    werror("Failed to read version.h\n");
    werror("cwd=%s  version.h=%s\n",getcwd(),pike_base_name+"/src/version.h");
    exit(1);
  }

  int maj, min, build;

  if ((!sscanf(s, "%*sPIKE_MAJOR_VERSION %d", maj)) ||
      (!sscanf(s, "%*sPIKE_MINOR_VERSION %d", min)) ||
      (!sscanf(s, "%*sPIKE_BUILD_VERSION %d", build))) {

    werror("Failed to get Pike version.\n");
    exit(1);
  }
  return sprintf("Pike v%d.%d release %d", maj, min, build);
}

void bump_version()
{
  werror("Bumping release number.\n");
  Process.create_process(({ "cvs", "update", "version.h" }),
			 ([ "cwd":pike_base_name+"/src" ]))->wait();

  string s=Stdio.read_file(pike_base_name+"/src/version.h");
  sscanf(s,"%s PIKE_BUILD_VERSION %d%s",string pre, int rel, string post);
  rel++;
  Stdio.File(pike_base_name+"/src/version.h", "wct")->
    write(pre+" PIKE_BUILD_VERSION "+rel+post);
  Process.create_process(({ "cvs", "commit", "-m",
			    "release number bumped to "+rel+" by export.pike",
			    "version.h" }),
			 ([ "cwd":pike_base_name+"/src" ]))->wait();
}

string pike_base_name;
string srcdir;
int rebuild;

int main(int argc, string *argv)
{
  mixed tmp;
  int e;
  string *files;
  object cvs;
  int notag;

  foreach(Getopt.find_all_options(argv,aggregate(
    ({ "srcdir", Getopt.HAS_ARG, "--srcdir"  }),
    ({ "rebuild",Getopt.NO_ARG,  "--rebuild" }),
    ({ "notag",  Getopt.NO_ARG,  "--notag"   }),
    )),array opt)
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

	case "rebuild":
	  rebuild=1;
	  break;

        case "notag":
	  notag=1;
	  break;
      }
    }
      
  argv=Getopt.get_args(argv);

  if(!srcdir)
  {
    tmp=reverse(argv[0]/"/");
    except_modules=mkmultiset(argv[1..]);
    e=search(tmp,"pike");
    if(e==-1)
    {
      werror("Couldn't find Pike source dir.\n");
      werror("Use export.pike --srcdir=<dir> <except modules>.\n");
      exit(1);
    }
    tmp=reverse(tmp[e..]);
    cd(sizeof(tmp) ? (tmp*"/") : "/");
    werror("Sourcedir = "+tmp*"/"+"\n");
    pike_base_name=".";
  }

//  werror("pike_base_name=%s\n",pike_base_name);
//  exit(0);
  if(rebuild)
  {
    werror("Not yet finished!\n");
    exit(1);
    object autoconfig=Process.create_process(({"./run_autoconfig"}),
					     (["cwd":pike_base_name]));
    /* make depend... */
    /* And other things... */
  }

  if(!notag && file_stat(pike_base_name+"/CVS"))
  {
    bump_version();

    vpath=replace(replace(getversion()," ","-"),"-release-",".");
    string tag=replace(vpath,({"Pike-","."}),({"","_"}));

    werror("Creating tag "+tag+" in the background.\n");
    cvs=Process.create_process(({"cvs","tag","-R","-F",tag}));
  }else{
    vpath=replace(replace(getversion()," ","-"),"-release-",".");
  }

  fix_configure(pike_base_name+"/src");

  foreach(get_dir(pike_base_name+"/src/modules") - ({"CVS","RCS"}), tmp)
    if(file_size(pike_base_name+"/src/modules/"+tmp) == -2)
      fix_configure("modules/"+tmp);

  //  werror("vpath = %s  pwd = %s\n",vpath,getcwd());
  symlink(".",vpath);
  //  system("ln -s pike "+vpath);

  files=`+( ({ vpath+"/README.txt", vpath+"/ANNOUNCE",
	       vpath+"/COPYING", vpath+"/COPYRIGHT",
	       vpath+"/Makefile" }),
	   get_files(vpath+"/src"),
	   get_files(vpath+"/lib"),
	   get_files(vpath+"/bin"),
	   get_files(vpath+"/man"),
	   get_files(vpath+"/refdoc/bin"),
	   get_files(vpath+"/refdoc/not_extracted"),
	   get_files(vpath+"/refdoc/presentation"),
	   get_files(vpath+"/refdoc/src_images"),
	   get_files(vpath+"/refdoc/structure"),
	   ({ vpath+"/refdoc/Makefile",
	      vpath+"/refdoc/inlining.txt",
	      vpath+"/refdoc/keywords.txt",
	      vpath+"/refdoc/syntax.txt",
	      vpath+"/refdoc/tags.txt",
	      vpath+"/refdoc/template.xsl",	      
	      vpath+"/refdoc/xml.txt",
	      vpath+"/refdoc/.cvsignore",
	   }));

  if(notag) {
    mapping m = gmtime(time());
    vpath = sprintf("%04d%02d%02d_%02d%02d%02d", 1900+m->year, m->mon+1, m->mday,
		    m->hour, m->min, m->sec);
  }

  werror("Creating "+vpath+".tar.gz:\n");
  object o=Stdio.File();

  int first=1;
  foreach(files/50.0,files)
    {
      if(Process.create_process(({"tar",
				    first?"cvf":"rvf",
				    pike_base_name+"/"+vpath+".tar"
				    })+files)->wait())
      {
	werror("Tar file creation failed!\n");
	if(cvs) cvs->wait();
	exit(1);
      }
      first=0;
    }

  if(Process.create_process(({"gzip",
				"-9",
				pike_base_name+"/"+vpath+".tar"
})
			    )->wait())
  {
    werror("Gzip failed!\n");
    if(cvs) cvs->wait();
    exit(1);
  }
  rm(vpath);
  werror("Done.\n");

  if(cvs && !notag)
  {
    cvs->wait();
    bump_version();
  }
  return 0;
}
