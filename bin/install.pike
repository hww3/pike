#! /usr/bin/env pike

#define USE_GTK

#if !constant(GTK.parse_rc)
#undef USE_GTK
#endif

int last_len;
int redump_all;
string pike;
array(string) files_to_delete=({});
array(string) files_to_not_delete=({});
array(string) to_dump=({});
array(string) to_export=({});

int export;
int no_gui;
int no_autodoc;

Tools.Install.ProgressBar progress_bar;

// for progress bar
int files_to_install;
int installed_files;

// the nt scripts depends on this value
// (incidentally defined elsewhere in the C code too)
#define MASTER_COOKIE __master_cookie

constant pike_upgrade_guid = "FCBB6B90-1608-4A7C-926C-69BBAB366326";

int istty_cache;
int istty()
{
  if(!istty_cache)
  {
#ifdef __NT__
    istty_cache=1;
#else
    istty_cache=!!Stdio.stdin->tcgetattr();
#endif
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
}

void status1(string fmt, mixed ... args)
{
  status_clear();
#ifdef USE_GTK
  if(label1)
  {
    label7->set_text(sprintf(fmt,@args)-"\n");
    GTK.flush();
    return;
  }
#endif

  // Ugly thing, but status_clear does not indent in non-tty mode...
  if(!istty())
    write("   ");

  write(fmt+"\n", @args);
}

string some_strerror(int err)
{
  string ret;
#if constant(strerror)
  ret=strerror(err);
#endif
  if(!ret || search("unknown error",lower_case(ret))!=-1)
    ret=sprintf("errno=%d",err);

  return ret;
}

void fail(string fmt, mixed ... args)
{
  int err=errno();
#ifdef USE_GTK
  if(label1)
  {
    status1(fmt,@args);
    hbuttonbox1->add(button1=GTK.Button("Exit")->show());
    button1->signal_connect("pressed",do_exit,0);

    label6->set_text("Click Exit to exit installation program.");

    // UGLY!!! -Hubbe
    while(1) { sleep(0.1); GTK.flush(); }
  }
#endif

  if(last_len) write("\n");
  werror("%s: %s\n",sprintf(fmt,@args),some_strerror(err));
  werror("Current directory = %s\n",getcwd());
  werror("**Installation failed..\n");
  exit(1);
}


void status(string doing, void|string file, string|void msg)
{
  if(!file) file="";
#ifdef USE_GTK
  if(label1)
  {
    last_len=1;
    label1->set_text(doing || "");
    label2->set_text(dirname(file) || "");
    label5->set_text(basename(file) || "");
    label6->set_text(msg || "");
    GTK.flush();
    return;
  }
#endif

  if(!istty()) return;

  if(progress_bar)
  {
    last_len = 75;
    progress_bar->set_current(installed_files);
    progress_bar->update(0);
    return;
  }

  file=replace(file,"\n","\\n");
  if(sizeof(file)>45)
    file=".."+file[sizeof(file)-44..];

  if(msg) file+=" "+msg;
  if(doing) file=doing+": "+file;
  string s="\r   "+file;
  int t=sizeof(s);
  if(t<last_len) s+=" "*(last_len-t);
  last_len=t;
  write(s);
}

void status_clear(void|int all)
{
    if(all)
	last_len = 75;
    status(0,"");
    status(0,"");
}

class WixNode
{
  inherit Parser.XML.Tree.SimpleElementNode;

  static void create(string name, mapping(string:string) attrs,
		     string|void text)
  {
    ::create("http://schemas.microsoft.com/wix/2003/01/wi" + name, attrs);
    mTagName = name;
    mNamespace = "http://schemas.microsoft.com/wix/2003/01/wi";
    if (text) {
      add_child(Parser.XML.Tree.SimpleTextNode(text));
    }
  }
}

class File
{
  string name;
  string source;
  string id;

  static void create(string name, string source, string id)
  {
    File::name = name;
    File::source = source;
    File::id = id;
  }

  WixNode gen_xml()
  {
    mapping(string:string) attrs = ([
      "Id":id,
      "Name":id[1..8]+"."+id[9..11],
      "LongName":name,
      "Vital":"yes",
      //      "KeyPath":"yes",
      //      "DiskId":"1",
    ]);
    if (source) {
      attrs->src = source;
#if 0
      if (search(attrs->src, "/") > 0) {
	// light doesn't like relative paths.
	attrs->src = combine_path(getcwd(), attrs->src);
      }
#endif
      if (has_prefix(attrs->src, "/home/")) {
	// KLUDGE
	attrs->src = "H:" + attrs->src[5..];
      }
      attrs->src = replace(attrs->src, "/", "\\");
    }
    return WixNode("File", attrs);
  }
}

class Directory
{
  string name;
  string id;
  Standards.UUID.UUID guid;
  string source;
  mapping(string:int) sub_sources = ([]);
  mapping(string:File) files = ([]);
  mapping(string:Directory) sub_dirs = ([]);

  static void create(string name, string parent_guid, string|void id)
  {
    guid = Standards.UUID.make_version3(name, parent_guid);
    if (!id) id = "_"+guid->str()-"-";
    Directory::name = name;
    Directory::id = id;
  }

  void add_path(string dest, string src)
  {
    if (search(dest, "/") == -1) {
      files[dest] = File(dest, src,
			 "_" +
			 Standards.UUID.make_version3(dest, guid->encode())->
			 str() - "-");
      if (has_suffix(src, "/"+dest)) {
	sub_sources[combine_path(src, "..")]++;
      }
    } else {
      array(string) path = dest/"/";
      string dname = path[0];
      Directory d = sub_dirs[dname];
      if (!d) {
	d = sub_dirs[dname] = Directory(dname, guid->encode());
      }
      d->add_path(dest[sizeof(dname)+1..], src);
    }
  }

  void set_sources()
  {
    foreach(sub_dirs; string dname; Directory d) {
      d->set_sources();
      if (d->source &&
	  has_suffix(d->source, "/" + dname)) {
	string sub_src = combine_path(d->source, "..");
	sub_sources[sub_src] += d->sub_sources[d->source] + 1;
      }
    }
    if (sizeof(sub_sources)) {
      array(int) cnt = values(sub_sources);
      array(string) srcs = indices(sub_sources);
      sort(cnt, srcs);
      source = srcs[-1];
      foreach(sub_dirs; string dname; Directory d) {
	if (d->source == source + "/" + dname) {
	  d->source = 0;
	}
      }
      foreach(files; string fname; File f) {
	if (f->source == source + "/" + fname) {
	  f->source = 0;
	}
      }
    }
  }

  WixNode gen_xml(string|void parent)
  {
    if (!parent) parent = "";
    parent += "/" + name;
    
    mapping(string:string) attrs = ([
      "Id":id,
    ]);
    // Win32 stupidity...
    if ((sizeof(name) > 11 ||
	 sizeof(name/".") > 2 ||
	 sizeof((name/".")[0]) > 8 ||
	 (sizeof(name/".") == 2 && sizeof((name/".")[1]) > 3)) &&
	(name != "SourceDir")) {
      attrs->LongName = name;
      attrs->Name = guid->str()[..7];
    } else {
      attrs->Name = name;
    }
    if (source) {
      attrs->src = source+"/";
#if 0
      if (search(attrs->src, "/") > 0) {
	// light doesn't like relative paths.
	attrs->src = combine_path(getcwd(), attrs->src);
      }
#endif
      if (has_prefix(attrs->src, "/home/")) {
	// KLUDGE.
	attrs->src = "H:"+attrs->src[5..];
      }
      attrs->src = replace(attrs->src, "/", "\\");
    }
    WixNode node = WixNode("Directory", attrs);
    foreach(sub_dirs;; Directory d) {
      node->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
      node->add_child(d->gen_xml(parent));
    }
    if (sizeof(files)) {
      node->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
      WixNode component = WixNode("Component", ([
				    "Id":"C_" + id,
				    "Guid":guid->str(),
				  ]));
      foreach(files;; File f) {
	component->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
	component->add_child(f->gen_xml());
      }
      node->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
      node->add_child(component);
    }
    node->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
    return node;
  }
}

class Dialog
{
  string id;
  string banner;

  class Control
  {
    string id;
    int x;
    int y;
    int width;
    int height;
    int disabled;
    constant type = "";
    string font = "\\VSI_MS_Sans_Serif13.0_0_0";
    string text;

    mapping(string:string) get_attrs()
    {
      mapping(string:string) res = ([
	"Type":type,
	"Id":id,
	"X":(string)x,
	"Y":(string)y,
	"Width":(string)width,
	"Height":(string)height,
      ]);
      if (disabled) {
	res->Disabled="yes";
      }
      return res;
    }

    array(WixNode) get_children()
    {
      if (text) {
	return ({
	  WixNode("Text", ([]), sprintf("{%s}%s", font, text)),
	});
      }
      return ({});
    }

    WixNode gen_xml()
    {
      mapping(string:string) attrs = get_attrs();
      foreach(attrs; string attr; string val) {
	if (!stringp(val)) {
	  error("Bad attributes for control: %O\n", attrs);
	}
      }
      WixNode res = WixNode("Control", attrs);
      foreach(get_children(), WixNode subnode) {
	res->add_child(subnode);
      }
      return res;
    }
  }

  class TextLabel
  {
    inherit Control;
    constant type = "Text";
    int x = 18;
    int height = 12;
    int transparent = 0;
    mapping(string:string) get_attrs()
    {
      mapping(string:string) res = ::get_attrs();
      if (transparent) res->Transparent = "yes";
      return res;
    }
  }

  class BannerText
  {
    inherit TextLabel;
    string id = "Banner";
    int x = 9;
    int y = 9;
    int width = 306;
    int height = 33;
    int transparent = 1;
    string font = "\\VSI_MS_Sans_Serif16.0_1_0";
    static void create()
    {
      text = banner;
    }
  }

  class BannerBmp
  {
    inherit Control;
    constant type = "Bitmap";
    string id = "BannerBmp";
    int x = 0;
    int y = 0;
    int width = 375;
    int height = 52;
    string bitmap = "Pike_banner";
    mapping(string:string) get_attrs()
    {
      mapping(string:string) res = ::get_attrs();
      res->TabSkip = "no";
      res->Text = bitmap;
      return res;
    }
  }

  class Line
  {
    inherit Control;
    constant type = "Line";
    string id = "Line";
    int x = 0;
    int y = 52;
    int width = 375;
    int height = 6;
    string text = "MsiHorizontalLine";
  }

  class Line2
  {
    inherit Line;
    string id = "Line2";
    int y = 252;
  }

  class Button
  {
    inherit Control;
    constant type = "PushButton";
    int y = 261;
    int width = 66;
    int height = 18;
  }

  class CancelButton
  {
    inherit Button;
    int x = 156;
    string text = "Cancel";
    string id = "Cancel";

    mapping(string:string) get_attrs()
    {
      mapping res = ::get_attrs();
      res->Cancel = "yes";
      return res;
    }
  }

  class PrevButton
  {
    inherit Button;
    int x = 228;
    string text = "< &Back";
    string id = "Prev";

    array(WixNode) get_children()
    {
      string prev_dialog_prop = sprintf("%s_PrevArgs", Dialog::id);
      if (disabled) {
	return ::get_children();
      }
      return ({
	@::get_children(),
	WixNode("Publish", ([
		  "Event":"NewDialog",
		  "Value":sprintf("[%s]", prev_dialog_prop),
		]),
		sprintf("%s<>\"\"", prev_dialog_prop)),
	WixNode("Condition", ([ "Action":"disable" ]),
		sprintf("%s=\"\"", prev_dialog_prop)),
	WixNode("Condition", ([ "Action":"enable" ]),
		sprintf("%s<>\"\"", prev_dialog_prop)),
      });
    }
  }

  class NextButton
  {
    inherit Button;
    int x = 300;
    string text = "&Next >";
    string id = "Next";
    int disabled = 1;

    mapping(string:string) get_attrs()
    {
      mapping(string:string) res = ::get_attrs();
      if (!disabled) {
	res->Default = "yes";
      }
      return res;
    }

    array(WixNode) get_children()
    {
      string next_dialog_prop = sprintf("%s_NextArgs", Dialog::id);
      if (disabled) {
	return ::get_children();
      }
      return ({
	@::get_children(),
	WixNode("Publish", ([
		  "Event":"NewDialog",
		  "Value":sprintf("[%s]", next_dialog_prop),
		]),
		sprintf("%s<>\"\"", next_dialog_prop)),
	WixNode("Publish", ([
		  "Event":"EndDialog",
		  "Value":"Return",
		]),
		sprintf("%s=\"\"", next_dialog_prop)),
      });
    }
  }

  class ProgressBar
  {
    inherit Control;
    constant type = "ProgressBar";
    string id = "ProgressBar";
    int x = 18;
    int y = 108;
    int width = 336;
    int height = 15;
    string text = "MsiProgressBar";
    array(WixNode) get_children()
    {
      return ({
	@::get_children(),
	@map(({ "StopServices", "DeleteServices", "StartServices",
		"WriteRegistryValues", "RemoveRegistryValues",
		"RemoveFiles", "MoveFiles", "UnmoveFiles", "InstallFiles",
	       "WriteIniValues", "InstallAdminPackage", "SetProgress",
		}), lambda(string event) {
		      return WixNode("Subscribe", ([
				       "Attribute":"Progress",
				       "Event":event,
				     ]));
		    }),
      });
    }    
  }

  int width = 373;
  int height = 287;
  string title = "[ProductName]";

  array(Control) controls;

  mapping(string:string) get_attrs()
  {
    return ([
      "Id":id,
      "Width":(string)width,
      "Height":(string)height,
      "Title":title,
    ]);
  }

  WixNode gen_xml()
  {
    WixNode res = WixNode("Dialog", get_attrs());
    foreach(controls, Control c) {
      res->add_child(c->gen_xml());
    }
    return res;
  }

  static void create()
  {
    // NOTE: __INIT must have finished before the objects are cloned.
    controls = ({
      BannerBmp(),
      BannerText(),
      Line(),
      CancelButton(),
      PrevButton(),
      NextButton(),
      Line2(),
    });
  }
}

class FolderDialog
{
  inherit Dialog;

  string id = "FolderForm";
  string banner = "Select Installation Folder";

  mapping(string:string) get_attrs()
  {
    mapping(string:string) res = ::get_attrs();
    res->TrackDiskSpace = "yes";
    return res;
  }

  class NextButton
  {
    inherit Dialog::NextButton;

    int disabled = 0;

    array(WixNode) get_children()
    {
      return ({
	@::get_children(),
	WixNode("Publish", ([
		  "Event":"SetTargetPath",
		  "Value":"TARGETDIR",
		]), "1"),
	WixNode("Publish", ([
		  "Event":"EndDialog",
		  "Value":"Return",
		]), "1"),
      });
    }
  }

  class FolderLabel
  {
    inherit TextLabel;
    int y = 114;
    int width = 348;
    string text = "&Folder:";
    string id = "FolderLabel";
    mapping(string:string) get_attrs()
    {
      mapping(string:string) res = ::get_attrs();
      res->TabSkip = "no";
      return res;
    }
  }

  class FolderEdit
  {
    inherit Control;
    constant type = "PathEdit";
    int x = 18;
    int y = 126;
    int width = 252;
    int height = 18;
    string id = "PathEdit";
    string text = "MsiPathEdit";
    
    mapping(string:string) get_attrs()
    {
      mapping(string:string) res = ::get_attrs();
      res->Sunken = "yes";
      res->Property = "TARGETDIR";
      return res;
    }
  }

  static void create()
  {
    ::create();
    controls += ({
      FolderLabel(),
      FolderEdit(),
    });
  }
}

class ProgressDialog
{
  inherit Dialog;

  string id = "ProgressDialog";
  string banner = "Installing...";

  class PrevButton
  {
    inherit Dialog::PrevButton;
    int disabled = 1;
  }

  mapping(string:string) get_attrs()
  {
    mapping(string:string) res = ::get_attrs();
    res->Modeless = "yes";
    return res;
  }

  static void create()
  {
    ::create();
    controls += ({
      ProgressBar(),
    });
  }
}

class UI
{
  array(Dialog) dialogs = ({
    FolderDialog(),
    ProgressDialog(),
  });

  WixNode gen_xml()
  {
    WixNode res = WixNode("UI", ([]))->
      add_child(WixNode("Property", ([ "Id":"DefaultUIFont" ]),
			"VsdDefaultUIFont.524F4245_5254_5341_4C45_534153783400"));
    //res->add_child(WixNode("Property", ([ "Id":"ErrorDialog" ]), "ErrorDialog"));
    foreach(dialogs, Dialog d) {
      res->add_child(d->gen_xml());
    }
    res->add_child(WixNode("TextStyle", ([
			     "Id":"VSI_MS_Sans_Serif13.0_0_0",
			     "FaceName":"MS Sans Serif",
			     "Size":"9",
			     "Red":"0",
			     "Green":"0",
			     "Blue":"0",
			   ])));
    res->add_child(WixNode("TextStyle", ([
			     "Id":"VSI_MS_Sans_Serif16.0_1_0",
			     "FaceName":"MS Sans Serif",
			     "Size":"12",
			     "Red":"0",
			     "Green":"0",
			     "Blue":"0",
			     "Bold":"yes",
			   ])));
    res->add_child(WixNode("TextStyle", ([
			     "Id":"VsdDefaultUIFont.524F4245_5254_5341_4C45_534153783400",
			     "FaceName":"MS Sans Serif",
			     "Size":"9",
			     "Red":"0",
			     "Green":"0",
			     "Blue":"0",
			   ])));
    res->add_child(WixNode("InstallUISequence", ([]))->
		   add_child(WixNode("Custom", ([
				       "After":"ValidateProductID",
				       "Action":"InitTarget",
				     ]),
				     "TARGETDIR=\"\""))->
		   add_child(WixNode("Show", ([
				       "After":"CostFinalize",
				       "Dialog":"FolderForm",
				     ])))->
		   add_child(WixNode("Show", ([
				       "After":"FolderForm",
				       "Dialog":"ProgressDialog",
				     ]))));
    return res;
  }
}

mapping already_created=([]);
int mkdirhier(string orig_dir)
{
  int tomove;
  if(export) return 1;
  string dir=orig_dir;
  if(already_created[orig_dir]) return 1;

  if(dir=="" || (sizeof(dir)==2 && dir[-1]==':')) return 1;
  dir=fakeroot(dir);

  status("Creating",dir+"/");

  mixed s=file_stat(dir);
  if(s)
  {
    if(s[1]<0)
      return already_created[orig_dir]=1;

    if(glob("*.pmod",dir))
    {
      if(!mv(dir,dir+".tmp"))
	fail("mv(%s,%s)",dir,dir+".tmp");
      tomove=1;
    }else{
      // FIXME: ask user if he wants to override
      werror("Warning: Directory '%s' already exists as a file.\n",dir);
      if(!mv(dir,dir+".old"))
	fail("mv(%s,%s)",dir,dir+".old");
    }
  }

  mkdirhier(dirname(dir));
  if(!mkdir(dir))
    fail("mkdir(%s)",dir);

  chmod(dir,0755);

  if(tomove)
    if(!mv(dir+".tmp",dir+"/module.pmod"))
      fail("mv(%s,%s)",dir+".tmp",dir+"/module.pmod");

  return already_created[orig_dir]=1;
}

int compare_files(string a,string b)
{
  mixed sa=file_stat(a);
  mixed sb=file_stat(b);
  if(sa && sb && sa[1]==sb[1])
    return Stdio.read_file(a) == Stdio.read_file(b);
  return 0;
}

int compare_to_file(string data,string a)
{
  mixed sa=file_stat(a);
  if(sa && sa[1]==sizeof(data))
    return Stdio.read_file(a) == data;
  return 0;
}

int low_install_file(string from,
		     string to,
		     void|int mode)
{
  installed_files++;
  if(export)
  {
    to_export+=({ from });
    return 1;
  }

  to=fakeroot(to);

  status("Installing",to);

  if(compare_files(from,to))
  {
    status("Installing",to,"Already installed");
    return 0;
  }
  mkdirhier(dirname(to));
  if( query_num_arg()==2 )
      mode=0755;

  string tmpfile=to+"-"+getpid()+"-"+time();
  if(!Stdio.cp(from,tmpfile))
    fail("copy(%s,%s)",from,tmpfile);

  // Chown and chgrp not implemented yet
  chmod(tmpfile,mode);

  // Need to rename the old file to .old
  if(file_stat(to))
  {
    rm(to+".old"); // Ignore errors
#if constant(hardlink)
    if( catch { hardlink(to,to+".old"); })
#endif
      mv(to,to+".old");
  }
  if(!mv(tmpfile,to))
    fail("mv(%s,%s)",tmpfile,to);

  return 1;
}

int install_file(string from,
		 string to,
		 void|int mode,
		 void|int dump)
{
  int ret;
  if(query_num_arg() == 2)
    ret=low_install_file(from,to);
  else
    ret=low_install_file(from,to,mode);

  array(string) tmp=to/".";
  string ext=tmp[-1];

  if((ret || redump_all) && dump)
  {
    switch(ext)
    {
      case "pike":
	if(glob("*/master.pike",to)) break;

      case "pmod":
	to_dump+=({to});
    }
  }

  // This magic deletes the remnants of static modules
  // when dynamic modules are installed.
  if(ret && ext == "so")
  {
    tmp[-1]="pmod";
    files_to_delete+=({ tmp*"." });
  }else{
    files_to_not_delete+=({ to });
  }
  return ret;
}

string stripslash(string s)
{
  while(sizeof(s)>1 && s[-1]=='/') s=s[..sizeof(s)-2];
  return s;
}


void install_dir(string from, string to,int dump)
{
  from=stripslash(from);
  to=stripslash(to);

  installed_files++;
  mkdirhier(to);
  foreach(get_dir(from),string file)
  {
    if(file=="CVS") continue;
    if(file=="testsuite.in") continue;
    if(file[..1]==".#") continue;
    if(file[0]=='#' && file[-1]=='#') continue;
    if(file[-1]=='~') continue;
    mixed stat=file_stat(combine_path(from,file));
    if (stat) {
      if(stat[1]==-2) {
	install_dir(combine_path(from,file),combine_path(to,file),dump);
      } else if (stat[0] & 0111) {
	// Executable
	install_file(combine_path(from,file),combine_path(to,file),0755,dump);
      } else {
	// Not executable
	install_file(combine_path(from,file),combine_path(to,file),0644,dump);
      }
    } else {
      werror(sprintf("\nstat:0, from:%O, file:%O, combined:%O\n",
		     from, file, combine_path(from, file)));
    }
  }
}

void install_header_files(string from, string to)
{
  installed_files++;
  from=stripslash(from);
  to=stripslash(to);
  mkdirhier(to);
  foreach(get_dir(from),string file)
    {
      if(file[..1]==".#") continue;
      if(file[-1]!='h' || file[-2]!='.') continue;
      install_file(combine_path(from,file),combine_path(to,file));
    }
}

mapping vars=([]);

object reg;

string regquote(string s)
{
  while(s[-1] == '/' || s[-1]=='\\') s=s[..sizeof(s)-2];
  return
    replace(s,
	    ({".","[","]","*","\\","(",")","|","+"}),
	    ({"\\.","\\[","\\]","\\*","\\\\","\\(","\\)","\\|","\\+"}) );
}

string globify(string s)
{
  if(s[-1]=='/') s=s[..sizeof(s)-2];
  return s+"*";
}

string fakeroot(string s)
{
  if(!vars->fakeroot) return s;
  if(!reg)
  {
    reg=Regexp(sprintf("^([^/])%{|(%s)%}",
		       Array.map(
				 ({
				   getcwd(),
				   vars->LIBDIR_SRC,
				   vars->SRCDIR,
				   vars->TMP_BINDIR,
				   vars->BASEDIR,
				   vars->MANDIR_SRC,
				   vars->DOCDIR_SRC,
				   vars->TMP_LIBDIR,
				   vars->fakeroot,
				 }),regquote)));
  }
  if(reg->match(s)) return s;
  return vars->fakeroot+s;
}

string export_base_name;

int mklink(string from, string to)
{
#if constant(symlink)
  catch  {
    symlink(from, to);
    return 1;
  };
#endif
  return 0;
}

string helptext=#"Usage: $TARFILE [options] [variables]

Options:
  -h, --help            Display this help and exit.
  -v, --version         Display version information and exit.
  --features            Display features and exit.
  --interactive         Interactive installation (default).
  --new-style           Install in <prefix>/pike/<ver>/{lib,include,bin}.
  --traditional         Install in <prefix>/{lib/pike,include/pike,bin}.

Variables:
  prefix=<path>         Install pike files here (/usr/local).
  pike_name=<path>      Create a symlink to pike here (<prefix>/bin/pike).
";

string translate(string filename, mapping translator)
{
  return translator[filename] ||
    combine_path(translate(dirname(filename),translator),basename(filename));
};

void tarfilter(string filename)
{
  ((program)combine_path(__FILE__, "..", "tarfilter"))()->
    main(3, ({ "tarfilter", filename, filename }));
}

#ifdef __NT__
constant tmpdir="~piketmp";
#endif /* __NT__ */

void do_export()
{
  if (export == 2) {
    status("Creating", /*export_base_name*/"Pike"+"_module.wxs");

#define TRANSLATE(X,Y) combine_path(".",X) : Y
    mapping translator = ([
      TRANSLATE(vars->BASEDIR,""),
      TRANSLATE(vars->LIBDIR_SRC,"lib"),
      TRANSLATE(vars->SRCDIR,"src"),
      TRANSLATE(vars->TMP_BINDIR,"bin"),
      TRANSLATE(vars->MANDIR_SRC,"man"),
      TRANSLATE(vars->DOCDIR_SRC,"doc"),
      TRANSLATE(vars->TMP_LIBDIR,"lib"),
      TRANSLATE(combine_path(vars->TMP_BUILDDIR, "doc_build"), "doc/src"),
      "unpack_master.pike" : "build/master.pike",
      "":"build",
    ]);

    Directory root = Directory("SourceDir",
			       Standards.UUID.new(),//UUID(pike_upgrade_guid)->encode(),
			       "PIKE_TARGETDIR");

    foreach(sort(to_export), string src) {
      root->add_path(translate(src, translator), src);
    }

    // Minimize the number of src directives.
    root->set_sources();

    // Generate the XML directory tree.
    WixNode xml_root = Parser.XML.Tree.SimpleRootNode()->
      add_child(Parser.XML.Tree.SimpleHeaderNode((["version":"1.0",
						   "encoding":"utf-8"])))->
      add_child(WixNode("Wix", ([
			  "xmlns":"http://schemas.microsoft.com/wix/2003/01/wi",
			]))->
		add_child(WixNode("Module", ([
				    "Id":"Pike",
				    "Guid":Standards.UUID.new_string(),
				    "Language":"1033",
				    "Version":sprintf("%d.%d.%d",
						      __REAL_MAJOR__,
						      __REAL_MINOR__,
						      __REAL_BUILD__),
				  ]))->
			  add_child(WixNode("Package", ([
					      "Description":"Pike dist",
					      "Comments":"Merge with this",
					      "Id":Standards.UUID.new_string(),
					      "Manufacturer":"IDA",
					      "InstallerVersion":"200",
					      "Compressed":"yes",
					    ])))->
			  add_child(root->gen_xml())));

    Stdio.write_file(/*export_base_name*/"Pike"+"_module.wxs", xml_root->render_xml());

    // Generate the banner image.
    status("Creating", "Pike_banner.bmp");

    mapping(string:Image.Image) logo =
      Image._decode(Stdio.read_bytes(combine_path(vars->BASEDIR,
						  "refdoc/src_images/pike_logo.gif")));

    Image.Image banner = Image.Image(500, 70, 255,255,255);

    banner->paste_mask(logo->img, logo->alpha,
		       490-logo->img->xsize(),
		       (70 - logo->img->ysize())/2);
    
    Stdio.write_file("Pike_banner.bmp", Image.BMP.encode(banner));

    // Generate the UserInterface

    status("Creating", /*export_base_name*/"Pike"+"_ui.wxs");

    xml_root = Parser.XML.Tree.SimpleRootNode()->
      add_child(Parser.XML.Tree.SimpleHeaderNode((["version":"1.0",
						   "encoding":"utf-8"])))->
      add_child(WixNode("Wix", ([
			  "xmlns":"http://schemas.microsoft.com/wix/2003/01/wi",
			]))->
		add_child(WixNode("Fragment", ([
				    "Id":"PikeUI",
				  ]))->
			  add_child(UI()->gen_xml())->
			  add_child(WixNode("Binary", ([
					      "Id":"Pike_banner",
					      "src":"Pike_banner.bmp"
					    ])))));

    Stdio.write_file(/*export_base_name*/"Pike"+"_ui.wxs",
		     xml_root->render_xml());

    // Generate the custom actions needed to install the master,
    // and finalize the pike binary.

    status("Creating", export_base_name+"_actions.wxs");

    string run_install =
      translate("pike", translator) +
      " -DNOT_INSTALLED" +
      " -m" + translate("unpack_master.pike", translator) + " " +
      translate(combine_path(vars->TMP_BINDIR,"install.pike"), translator);
    WixNode fragment_list =
      WixNode("Fragment", ([
		"Id":"PikeActions",
	      ]))->
      add_child(WixNode("CustomAction", ([
			  "Id":"FinalizePike",
			  "Directory":"PIKE_TARGETDIR",
			  "ExeCommand":run_install + " --finalize",
			])))->
      add_child(WixNode("CustomAction", ([
			  "Id":"InstallMaster",
			  "Directory":"PIKE_TARGETDIR",
			  "ExeCommand":run_install + " --install-master",
			])));

    xml_root = Parser.XML.Tree.SimpleRootNode()->
      add_child(Parser.XML.Tree.SimpleHeaderNode((["version":"1.0",
						   "encoding":"utf-8"])))->
      add_child(WixNode("Wix", ([
			  "xmlns":"http://schemas.microsoft.com/wix/2003/01/wi",
			]))->
		add_child(fragment_list));

    Stdio.write_file(export_base_name+"_actions.wxs", xml_root->render_xml());

    // Generate the main wxs file.

    status("Creating", export_base_name+".wxs");

    WixNode product_node = WixNode("Product", ([
				     "Name":"Pike",
				     "Language":"1033",
				     "UpgradeCode":pike_upgrade_guid,
				     "Id":Standards.UUID.new_string(),
				     "Version":sprintf("%d.%d.%d",
						       __REAL_MAJOR__,
						       __REAL_MINOR__,
						       __REAL_BUILD__),
				     "Manufacturer":"IDA",
				   ]))->
      add_child(WixNode("Package", ([
			  "Manufacturer":"IDA",
			  "Languages":"1033",
			  "InstallerVersion":"200",
			  "Platforms":"Intel",
			  "Id":Standards.UUID.new_string(),
			  "Compressed":"yes",
			  "SummaryCodepage":"1252",
			])))->
      add_child(WixNode("Media", ([
			  "Id":"1",
			  "EmbedCab":"yes",
			  "Cabinet":"Pike.cab",
			])))->
      add_child(WixNode("Directory", ([
			  "Id":"PIKE_TARGETDIR",
			  "Name":"SourceDir",
			]))->
		add_child(WixNode("Merge", ([
				    "Id":"Pike",
				    "Language":"1033",
				    "src":export_base_name+"_module.msm",
				    "DiskId":"1",
				  ]))))->
      add_child(WixNode("Feature", ([
			  "Id":"F_Pike",
			  "Title":sprintf("Pike %d.%d.%d",
					  __REAL_MAJOR__,
					  __REAL_MINOR__,
					  __REAL_BUILD__),
			  "Level":"1",
			  "ConfigurableDirectory":"PIKE_TARGETDIR",
			]))->
		add_child(WixNode("MergeRef", ([
				    "Id":"Pike",
				  ]))))->
      add_child(WixNode("Upgrade", ([
			  "Id":Standards.UUID.new_string(),
			]))->
		add_child(WixNode("UpgradeVersion", ([
				    "Minimum":sprintf("%d.0.0",
						      __REAL_MAJOR__),
				    "Property":"NEWERPRODUCTFOUND",
				    "OnlyDetect":"yes",
				    "IncludeMinimum":"yes",
				  ]))))->
      add_child(WixNode("CustomAction", ([
			  "Id":"QueryTarget",
			  "Property":"PIKE_TARGETDIR",
			  "Value":"[ProgramFilesFolder][Manufacturer]\[ProductName]",
			  "Execute":"firstSequence",
			])))
->
      add_child(WixNode("InstallExecuteSequence", ([]))->
		add_child(WixNode("Custom", ([
				    "Action":"QueryTarget",
				    "Before":"InstallFiles",
				  ]),
				  "PIKE_TARGETDIR=\"\""))
#if 0
->
		add_child(WixNode("Custom", ([
				    "Action":"FinalizePike",
				    "After":"InstallFiles",
				  ])))->
		add_child(WixNode("Custom", ([
				    "Action":"InstallMaster",
				    "After":"FinalizePike",
				  ])))
#endif /* 0 */
		)
#if 0
->
      add_child(WixNode("UI", ([]))->
		add_child(WixNode("Dialog", ([
				    "Id":"TargetDialog",
				    "Title":"[ProductName]",
				    "TrackDiskSpace":"yes",
				    "Width":"373",
				    "Height":"287",
				  ]))->
			  add_child(WixNode("Control", ([
					      "Id":"TargetEdit",
					      "Type":"PathEdit",
					      "Property":"PIKE_TARGETDIR",
					      "Sunken":"yes",
					      "Width":"258",
					      "Height":"18",
					      "X":"18",
					      "Y":"126",
					    ])))->
			  add_child(WixNode("Control", ([
					      "Id":"NextButton",
					      "Type":"PushButton",
					      "Default":"yes",
					      "Width":"66",
					      "Height":"18",
					      "X":"300",
					      "Y":"261",
					    ]))->
				    add_child(WixNode("Publish", ([
							"Event":"SetTarget",
							"Value":"PIKE_TARGETDIR",
						      ])))->
				    add_child(WixNode("Publish", ([
							"Event":"EndDialog",
							"Value":"Return"
						      ])))))->
		add_child(WixNode("InstallUISequence", ([]))->
			  add_child(WixNode("Custom", ([
					      "Action":"QueryTarget",
					      "Before":"TargetDialog",
					    ])))->
			  add_child(WixNode("Show", ([
					      "Dialog":"TargetDialog",
					      "Before":"ProgressForm",
					    ])))))
#endif /* 0 */
;

    xml_root = Parser.XML.Tree.SimpleRootNode()->
      add_child(Parser.XML.Tree.SimpleHeaderNode((["version":"1.0",
						   "encoding":"utf-8"])))->
      add_child(WixNode("Wix", ([
			  "xmlns":"http://schemas.microsoft.com/wix/2003/01/wi",
			]))->add_child(product_node));

    Stdio.write_file(export_base_name+".wxs", xml_root->render_xml());
  } else {
#ifdef __NT__
  status("Creating",export_base_name+".burk");
  Stdio.File p=Stdio.File(export_base_name+".burk","wc");
  string msg="   Loading installation script, please wait...";
  p->write("w%4c%s",sizeof(msg),msg);

#define TRANSLATE(X,Y) combine_path(".",X) : Y
  mapping translator = ([
    TRANSLATE(vars->BASEDIR,tmpdir),
    TRANSLATE(vars->LIBDIR_SRC,tmpdir+"/lib"),
    TRANSLATE(vars->SRCDIR,tmpdir+"/src"),
    TRANSLATE(vars->TMP_BINDIR,tmpdir+"/bin"),
    TRANSLATE(vars->MANDIR_SRC,tmpdir+"/man"),
    TRANSLATE(vars->DOCDIR_SRC,tmpdir+"/refdoc"),
    TRANSLATE(vars->TMP_LIBDIR,tmpdir+"/build/lib"),
    "unpack_master.pike" : tmpdir+"/build/master.pike",
    "":tmpdir+"/build",
  ]);


  array(string) translated_names = Array.map(to_export, translate, translator);
  array(string) dirs=Array.uniq(Array.map(translated_names, dirname));
  while(1)
  {
    array(string) d2=Array.map(dirs, dirname) - dirs;
    if(!sizeof(d2)) break;
    dirs+=Array.uniq(d2);
  }
  dirs-=({""});
  sort(dirs);

  foreach(dirs, string dir) p->write("d%4c%s",sizeof(dir),dir);
  foreach(Array.transpose(  ({ to_export, translated_names }) ),
	  [ string file, string file_name ])
    {
      status("Adding",file);
      if (string f=Stdio.read_file(file)) {
	p->write("f%4c%s%4c",sizeof(file_name),file_name,sizeof(f));
	p->write(f);
      } else {
	//  Huh? File could not be found.
	werror("-------------------\n"
	       "Warning: Could not add file: %s. File not found!\n"
	       "-------------------\n", file);
      }
    }

  // FIXME, support $INSTALL_SCRIPT (or similar)

#define TRVAR(X) translate(combine_path(vars->X,"."), translator)

  array(string) env=({
//    "PIKE_MODULE_PATH="+TRVAR(TMP_LIBDIR)+"/modules:"+TRVAR(LIBDIR_SRC)+"/modules",
//    "PIKE_PROGRAM_PATH=",
//    "PIKE_INCLUDE_PATH="+TRVAR(LIBDIR_SRC)+"/include",
  });

  foreach(env, string e)
    p->write("e%4c%s",sizeof(e),e);

#define RELAY(X) " " #X "=" + TRVAR(X)+

  string cmd=
    replace(translate("pike.exe", translator),"/","\\")+
    " -m"+translate("unpack_master.pike", translator)+
    " -DNOT_INSTALLED" +
    " "+translate( combine_path(vars->TMP_BINDIR,"install.pike"), translator)+
    RELAY(TMP_LIBDIR)
    RELAY(LIBDIR_SRC)
    RELAY(SRCDIR)
    RELAY(TMP_BINDIR)
    RELAY(MANDIR_SRC)
    RELAY(DOCDIR_SRC)
    RELAY(BASEDIR)
    " TMP_BUILDDIR="+translate("", translator)+
    (((vars->PIKE_MODULE_RELOC||"") != "")? " PIKE_MODULE_RELOC=1":"")+
    " $" // $ = @argv
    ;

  p->write("s%4c%s",sizeof(cmd),cmd);

  array(string) to_delete=translated_names + ({translate("pike.tmp",translator)});
  to_delete=Array.uniq(to_delete);
  to_delete+=reverse(dirs);

  // Generate cleanup
  foreach(to_delete, string del)
    p->write("D%4c%s",sizeof(del),del);

  p->write("q\0\0\0\0");
  p->close("rw");

  if(last_len)
  {
    status(0,"");
    status(0,"");
  }

#else
  export=0;

  cd("..");

  string tmpname = sprintf("PtmP%07x",random(0xfffffff));

  status("Creating","script glue");

  Stdio.write_file(tmpname+".x",
		   "#!/bin/sh\n"
#"TARFILE=\"$1\"; shift
ARGS=''

INSTALL_SCRIPT='bin/install.pike'

while [ $# != 0 ]
do
    case \"$1\" in
              -v|\\
       --version) echo \""+version()+
#" Copyright (C) 1994-2004 IDA, Link�ping University
Pike comes with ABSOLUTELY NO WARRANTY; This is free software and you
are welcome to redistribute it under certain conditions; Read the
files COPYING and COPYRIGHT in the Pike distribution for more details.
\";
		  rm -f "+tmpname+#".x
                  exit 0 ;;

              -h|\\
          --help) echo \"" + helptext + #"\"
		  rm -f "+tmpname+#".x
                  exit 0 ;;

      --features) echo \"" + Tools.Install.features()*"\n" + #"\"
		  rm -f "+tmpname+#".x
                  exit 0 ;;

    --list-files) tar xf \"$TARFILE\" "+tmpname+#".tar.gz
                  tar tfz "+tmpname+#".tar.gz
                  rm -f "+tmpname+".x "+tmpname+#".tar.gz
                  exit 0 ;;

              -s|\\
        --script) shift
                  INSTALL_SCRIPT=\"$1\" ;;

               *) ARGS=\"$ARGS '`echo \\\"$1\\\" | sed -e \\\"s/'/'\\\\\\\"'\\\\\\\"'/g\\\"`'\" ;;
    esac
    shift
done
"
		   "echo \"   Loading installation script, please wait...\"\n"
		   "tar xf \"$TARFILE\" "+tmpname+".tar.gz\n"
		   "gzip -dc "+tmpname+".tar.gz | tar xf -\n"
		   "rm -rf "+tmpname+".tar.gz\n"
		   "( cd '"+export_base_name+".dir'\n"
		   "  eval \"build/pike -mmaster.pike -DNOT_INSTALLED "
		                "\\\"$INSTALL_SCRIPT\\\" \\\n"
		   "  TMP_LIBDIR=\\\"build/lib\\\"\\\n"
		   "  LIBDIR_SRC=\\\"lib\\\"\\\n"
		   "  SRCDIR=\\\"src\\\"\\\n"
		   "  TMP_BINDIR=\\\"bin\\\"\\\n"
		   "  TMP_BUILDDIR=\\\"build\\\"\\\n"
		   "  MANDIR_SRC=\\\"man\\\"\\\n"
		   "  DOCDIR_SRC=\\\"refdoc\\\"\\\n"
		   "  PIKE_MODULE_RELOC=\\\"" + vars->PIKE_MODULE_RELOC +
		                       "\\\"\\\n"
		   "  $ARGS\"\n"
		   ")\n"
		   "rm -rf '"+export_base_name+".dir' "+tmpname+".x\n"
    );
  chmod(tmpname+".x",0755);
  string script=sprintf("#!/bin/sh\n"
			"tar xf \"$0\" %s.x\n"
			"exec ./%s.x \"$0\" \"$@\"\n", tmpname, tmpname, tmpname);
  if(sizeof(script) >= 100)
  {
    werror("Script too long!!\n");
    exit(1);
  }

  array(string) parts = script/"/";
  mkdirhier( parts[..sizeof(parts)-2]*"/");
  Stdio.write_file(script,"");

  to_export = map(to_export,
		  lambda(string s) {
		    return combine_path(export_base_name+".dir", s);
		  } );

  string tmpmsg=".";

  string tararg="cf";
  foreach(to_export/25.0, array files_to_tar)
    {
      status("Creating", tmpname+".tar", tmpmsg);
      tmpmsg+=".";
      Process.create_process(({"tar",tararg,tmpname+".tar"})+ files_to_tar)
	->wait();
      tararg="rf";
    }

  status("Filtering to root/root ownership", tmpname+".tar");
  tarfilter(tmpname+".tar");

  status("Creating", tmpname+".tar.gz");

  Process.create_process(({"gzip","-9",tmpname+".tar"}))->wait();

  to_export = ({ script, tmpname+".x", tmpname+".tar.gz" });

  status("Creating", export_base_name);

  Process.create_process( ({ "tar","cf", export_base_name }) + to_export )
    ->wait();

  status("Filtering to root/root ownership", export_base_name);
  tarfilter(export_base_name);

  chmod(export_base_name,0755);

  status("Cleaning up","");

  Process.create_process( ({ "rm","-rf",
			     export_base_name+".dir",
			     export_base_name+".x",
			     tmpname+".x",
			     tmpname+".tar.gz",
			     parts[0],
  }) ) ->wait();

#endif
  }
  status1("Export done");

  exit(0);
}

#ifdef USE_GTK
object window1;
object vbox1;
object label4;
object frame1;
object hbox1;
object vbox2;
object label3;
object table1;
object entry1;
object entry2;
object label1;
object label2;
object button4;
object button5;
object hbuttonbox1;
object button1;
object button2;
object label5;
object label6;
object label7;
object vbox3;

#define PS pack_start
#define AT attach

void do_abort()
{
  // FIXME
  werror("Installation aborted.\n");
  exit(1);
}

void update_entry2()
{
  entry2->set_text( combine_path( entry1 -> get_text(), "bin/pike") );
}

void set_filename(array ob, object button)
{
  object selector=ob[0];
  object entry=ob[1];
  entry->set_text(selector->get_filename());
  if(entry == entry1)
    update_entry2();
  destruct(selector);
}

void selectfile(object entry, object button)
{
  object selector;
  selector=GTK.FileSelection("Pike installation prefix");
  selector->set_filename(entry->get_text());
  selector->ok_button()->signal_connect("clicked", set_filename,
					({ selector, entry }) );
  selector->cancel_button()->signal_connect("clicked",destruct,selector);
  selector->show();
}

void do_exit()
{
  exit(0);
}

void cancel()
{
  werror("See you another time!\n");
  exit(0);
}

void proceed()
{
  pre_install(({}));
  label6->set_text("Click Ok to exit installation program.");
  hbuttonbox1->add(button1=GTK.Button("Ok")->show());
  button1->signal_connect("pressed",do_exit,0);
}

int next()
{
  vars->prefix = entry1->get_text();
  vars->pike_name = entry2->get_text();
  install_type="--new_style";

  destruct(table1);

  vbox2->PS(vbox3=GTK.Vbox(0,0)->show(),1,1,0);

  vbox3->PS(label7=GTK.Label("---head---")->show(),0,0,0);
  vbox3->PS(label1=GTK.Label("---action---")->show(),0,0,0);
  vbox3->PS(label2=GTK.Label("----dir-----")->show(),0,0,0);
  vbox3->PS(label5=GTK.Label("----file----")->show(),0,0,0);
  vbox3->PS(label6=GTK.Label("----msg----")->show(),0,0,0);
  destruct(button1);
  destruct(button2);

  call_out(proceed, 0);
  return 1;
}

void begin_wizard(array(string) argv)
{
  // FIXME:
  // We should display the GPL licence and make the user
  // click 'agree' first
  //
  GTK.setup_gtk(argv);
  window1=GTK.Window(GTK.WINDOW_TOPLEVEL)
    ->set_title(version()+" installer")
    ->add(vbox1=GTK.Vbox(0,0)
	  ->PS(label4=GTK.Label(version()+" installer")
	       ->set_justify(GTK.JUSTIFY_CENTER),0,0,10)
	  ->PS(frame1=GTK.Frame()
	       ->set_shadow_type(GTK.SHADOW_IN)
	       ->set_border_width(11)
	       ->add(hbox1=GTK.Hbox(0,0)
		     ->PS(GTK.Pixmap(GTK.Util.load_image(combine_path(vars->SRCDIR,
								      "install-welcome"))->img),0,0,0)
		     ->PS(vbox2=GTK.Vbox(0,0),1,1,0)
		       ),1,1,0)
	  ->PS(hbuttonbox1=GTK.HbuttonBox()
	       ->set_border_width(15)
	       ->add(button1=GTK.Button("Cancel"))
	       ->add(button2=GTK.Button("Install Pike >>"))
	       ,0,1,0));

  vbox2->PS(label3=GTK.Label("Welcome to the interactive "+version()+" installer.")
	    ->set_justify(GTK.JUSTIFY_CENTER),1,1,0)
    ->PS(table1=GTK.Table(3,2,0)
	 ->set_border_width(19)
	 ->AT(entry1=GTK.Entry(),
	      1,2,0,1,GTK.Fill | GTK.Expand,0,0,0)
	 ->AT(entry2=GTK.Entry(),
	      1,2,1,2,GTK.Fill | GTK.Expand,0,0,0)
	 ->AT(label1=GTK.Label("Install prefix: ")
	      ->set_justify(GTK.JUSTIFY_RIGHT),
	      0,1,0,1,GTK.Expand,0,0,0)
	 ->AT(label2=GTK.Label("Pike binary name: ")
	      ->set_justify(GTK.JUSTIFY_RIGHT),
	      0,1,1,2,GTK.Expand,0,0,0)
	 ->AT(button4=GTK.Button("Browse"),
	      2,3,1,2,0,0,0,0)
	 ->AT(button5=GTK.Button("Browse"),
	      2,3,0,1,0,0,0,0),0,0,0);

  vbox2->show_all();

  entry1->set_text(prefix);
  entry2->set_text(vars->pike_name ||
		   combine_path(vars->exec_prefix||combine_path(prefix, "bin"),
				"pike"));

  entry1->signal_connect("focus_out_event",update_entry2,0);
  button1->signal_connect("pressed",cancel,0);
  button2->signal_connect("pressed",next,0);
  button4->signal_connect("pressed",selectfile,entry2);
  button5->signal_connect("pressed",selectfile,entry1);

  window1->show_all();
}
#endif


int traditional;
string prefix;
string exec_prefix;
string lib_prefix;
string include_prefix;
string doc_prefix;
string man_prefix;
string lnk;
string old_exec_prefix;
object interactive;
string install_type="--interactive";


int pre_install(array(string) argv)
{
  if( vars->prefix )
    prefix = vars->prefix;
  else {
#ifdef __NT__
    prefix = RegGetValue(HKEY_LOCAL_MACHINE,
			 "SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
			 "ProgramFilesDir");
#else
    prefix = "/usr/local";
#endif
  }

  if(!vars->TMP_BINDIR)
    vars->TMP_BINDIR=combine_path(vars->SRCDIR,"../bin");

  if(!vars->TMP_BUILDDIR) vars->TMP_BUILDDIR=".";

  while(1)
  {
    // werror("install_type: %O...\r\n", install_type);
  switch(install_type)
  {
    case "--traditional":
      exec_prefix=vars->exec_prefix||(prefix+"/bin/");
      lib_prefix=vars->lib_prefix||(prefix+"/lib/pike/");
      include_prefix=combine_path(prefix,"include","pike");
      doc_prefix=combine_path(prefix, "doc", "pike");
      man_prefix=vars->man_prefix||(prefix+"/man/");
      break;

    case "--interactive":

#ifdef USE_GTK
      catch  {
	if(!no_gui)
	{
#ifndef __NT__ // We are using GTK on Win32!! no DISPLAY required
	  if(getenv("DISPLAY"))
#endif
	  {
	    begin_wizard(argv);
	    return -1;
	  }
	}
      };
#endif

      status1("");

      interactive=Tools.Install.Readline();
      interactive->set_cwd("../");

      write("   Welcome to the interactive "+version()+
	    " installation script.\n"
	    "\n" +
	    (interactive->get_input_controller()->dumb ?
	     "   The script will guide you through the installation process by asking\n"
	     "   a few questions. You will be able to confirm your settings before\n"
	     "   the installation begins.\n"
	     :
	     "   The script will guide you through the installation process by asking\n"
	     "   a few questions. Whenever you input a path or a filename, you may use\n"
	     "   the <tab> key to perform filename completion. You will be able to\n"
	     "   confirm your settings before the installation begins.\n")
	    );

      string confirm, bin_path = vars->pike_name;
      do {
	write("\n");

	prefix = interactive->edit_directory(prefix,"Install prefix: ");
	prefix = interactive->absolute_path(prefix);

	if(!vars->pike_name)
	{
#if constant(symlink)
	  bin_path=interactive->edit_filename
		   (combine_path(vars->exec_prefix ||
				 combine_path(prefix, "bin"),
				 "pike"), "Pike binary name: ");
#else
	  bin_path=combine_path(
#ifdef __NT__
				"\\",
#else
				"/",
#endif
				getcwd(),prefix,"pike",
				replace(version()-"Pike v"," release ","."),
				"bin","pike");
#endif
	}

	bin_path = interactive->absolute_path(bin_path);

	write("\n");
	confirm =
	  lower_case(interactive->
		     edit("", "Are the settings above correct [Y/n/quit]? "));
	if(confirm == "quit")
	{
	  // Maybe clean up?
	  destruct(interactive);
	  exit(0);
	}

      } while(!(confirm == "" || confirm == "y"));

      write("\n");

      vars->pike_name = bin_path;

      destruct(interactive);
      install_type="--new-style";
      continue;

    case "--wix":
      export = 2;
    case "--export":
      export = export || 1;
      string ver = replace( version(), ([ " ":"-", " release ":"." ]) );
#if constant(uname)
      mapping(string:string) u = uname();
      if( u->sysname=="AIX" )
      {
	export_base_name = sprintf("%s-%s-%s.%s",
				   ver,
				   u->sysname,
				   u->version,
				   u->release);
      }
      else {
	export_base_name = sprintf("%s-%s-%s-%s",
				   ver,
				   u->sysname,
				   u->release,
				   u->machine);
      }
      export_base_name = replace(export_base_name, ([ "/":"-", " ":"-" ]));
#else
      export_base_name = ver;
#endif

      status1("Building export %s\n", export_base_name);

#ifndef __NT__
      if (export == 1) {
	if (!mkdir(export_base_name+".dir")) {
	  error("Failed to create directory %O: %s\n",
		export_base_name+".dir", strerror(errno()));
	}

	mklink(vars->LIBDIR_SRC,export_base_name+".dir/lib");
	mklink(vars->SRCDIR,export_base_name+".dir/src");
	mklink(getcwd(),export_base_name+".dir/build");
	mklink(vars->TMP_BINDIR,export_base_name+".dir/bin");
	mklink(vars->MANDIR_SRC,export_base_name+".dir/man");
	mklink(vars->DOCDIR_SRC,export_base_name+".dir/refdoc");

	cd(export_base_name+".dir");

	vars->TMP_LIBDIR="build/lib";
	vars->LIBDIR_SRC="lib";
	vars->SRCDIR="src";
	vars->TMP_BINDIR="bin";
	vars->MANDIR_SRC="man";
	vars->DOCDIR_SRC="refdoc";
	vars->TMP_BUILDDIR="build";
      }
#endif
      to_export+=({ combine_path(vars->TMP_BINDIR,"install.pike") });

    case "":
    default:
    case "--new-style":
      if(!(lnk=vars->pike_name) || !sizeof(lnk)) {
	lnk = combine_path(vars->exec_prefix || combine_path(vars->prefix, "bin"),
			   "pike");
	old_exec_prefix=vars->exec_prefix; // to make the directory for pike link
      }
      prefix = combine_path("/", getcwd(), prefix, "pike",
			    replace(version()-"Pike v"," release ","."));
      exec_prefix=combine_path(prefix,"bin");
      lib_prefix=combine_path(prefix,"lib");
      doc_prefix=combine_path(prefix,"doc");
      include_prefix=combine_path(prefix,"include","pike");
      man_prefix=combine_path(prefix,"man");
      break;
  case "--finalize":
    prefix = getcwd();
    exec_prefix = combine_path(prefix, "bin");
    lib_prefix = combine_path(prefix, "lib");
    vars->TMP_BUILDDIR = "build";
    finalize_pike();
    return 0;
  case "--install-master":
    prefix = getcwd();
    exec_prefix = combine_path(prefix, "bin");
    lib_prefix = combine_path(prefix, "lib");
    include_prefix = combine_path(prefix,"include","pike");
    make_master("lib/master.pike", "lib/master.pike.in",
		lib_prefix, include_prefix);
    return 0;
  }
  break;
  }

  do_install();
  return 0;
}

// Create a master.pike with the correct lib_prefix
void make_master(string dest, string master, string lib_prefix,
		 string include_prefix, string|void share_prefix)
{
  status("Finalizing",master);
  string master_data=Stdio.read_file(master);
  if (!master_data) {
    error("Failed to read master template file %O\n", master);
  }
  master_data=replace(master_data,
		      ({"�lib_prefix�","�include_prefix�","�share_prefix�"}),
		      ({replace(lib_prefix,"\\","\\\\"),
			replace(include_prefix,"\\","\\\\"),
			replace(share_prefix||"�share_prefix�", "\\", "\\\\"),
		      }));
  if((vars->PIKE_MODULE_RELOC||"") != "")
    master_data = replace(master_data, "#undef PIKE_MODULE_RELOC",
			  "#define PIKE_MODULE_RELOC 1");
  if(compare_to_file(master_data, dest)) {
    status("Finalizing",dest,"Already finalized");
    return;
  }
  Stdio.write_file(dest,master_data);
  status("Finalizing",master,"done");
}

// Install file while fixing CC= w.r.t. smartlink
void fix_smartlink(string src, string dest, string include_prefix)
{
  status("Finalizing",src);
  string data=Stdio.read_file(src);
  data = map(data/"\n", lambda(string s) {
			  string cc;
			  if(2==sscanf(s, "CC=%*s/smartlink %s", cc))
			    return "CC="+include_prefix+"/smartlink "+cc;
			  else
			    return s;
			})*"\n";
  if(compare_to_file(data, dest)) {
    status("Finalizing",dest,"Already finalized");
    return;
  }
  Stdio.write_file(fakeroot(dest),data);
  status("Finalizing",fakeroot(dest),"done");
}

// dump modules (and master)
void dump_modules()
{
  string master=combine_path(lib_prefix,"master.pike");
  Stdio.Stat s1=file_stat(master);
  Stdio.Stat s2=file_stat(master+".o");
  mapping(string:mapping(string:string)) options = ([
    "env":getenv()-([
      "PIKE_PROGRAM_PATH":"",
      "PIKE_MODULE_PATH":"",
      "PIKE_INCLUDE_PATH":"",
      "PIKE_MASTER":"",
      ]) ]);


  if(!s2 || s1->mtime>=s2->mtime || redump_all)
  {
    int retcode;
    mixed error = catch {
      if(file_stat(fakeroot(pike))) {
	object p=
	  Process.create_process( ({fakeroot(pike),"-m",
	    combine_path(vars->SRCDIR,"dumpmaster.pike"),
	    @(vars->fakeroot?({"--fakeroot="+vars->fakeroot}):({})),
	    master}), options);
	retcode=p->wait();
      }
      else
	werror("Pike binary %O could not be found.\n"
	       "Dumping of master.pike failed (not fatal).\n",
	       fakeroot(pike));
    };
    if(error)
      werror("Dumping of master.pike failed (not fatal)\n%s\n",
	     describe_backtrace(error));
    if(retcode)
      werror("Dumping of master.pike failed (not fatal) (0x%08x)\n",
	     retcode);
  }

  if(!sizeof(to_dump)) return;

  rm("dumpmodule.log");

  foreach(to_dump, string mod)
    if (file_stat(mod+".o"))
      rm(mod+".o");

  array cmd=({ fakeroot(pike) });

  if(vars->fakeroot)
    cmd+=({
      sprintf("-DPIKE_FAKEROOT=%O",vars->fakeroot),
      sprintf("-DPIKE_FAKEROOT_OMIT=%O",
	      map( ({
		getcwd(),
		vars->LIBDIR_SRC,
		vars->SRCDIR,
		vars->TMP_BINDIR,
		vars->MANDIR_SRC,
		vars->DOCDIR_SRC,
		vars->TMP_LIBDIR,
		vars->BASEDIR,
		vars->fakeroot,
	      }), globify)*":"),
      "-m",combine_path(vars->TMP_LIBDIR,"master.pike")
    });

  cmd+=({ "-x", "dump",
	  "--log-file",	// --distquiet below might override this.
#ifdef USE_GTK
	  label1?"--distquiet":
#endif
	  "--quiet"});

  // Dump 25 modules at a time as to not confuse systems with
  // very short memory for application arguments.

  int offset = 1;
  foreach(to_dump/25.0, array delta_dump)
  {
    mixed err = catch {
      object p=
	Process.create_process(cmd +
			       ( istty() ?
				 ({
				   sprintf("--progress-bar=%d,%d",
					   offset, sizeof(to_dump))
				 }) : ({}) ) +
			       delta_dump, options);
      int retcode=p->wait();
      if (retcode)
	werror("Dumping of some modules failed (not fatal) (0x%08x)\n",
	       retcode);
    };
    if (err) {
      werror("Failed to spawn module dumper (not fatal):\n"
	       "%s\n", describe_backtrace(err));
    }

    offset += sizeof(delta_dump);
  }

  if(progress_bar)
    // The last files copied does not really count (should
    // really be a third phase)...
    progress_bar->set_phase(1.0, 0.0);

  status_clear(1);
}

void finalize_pike()
{
  pike=combine_path(exec_prefix,"pike");
  // Ugly way to detect NT installation
  string pike_bin_file=combine_path(vars->TMP_BUILDDIR,"pike");
  if(file_stat(pike_bin_file+".exe"))
  {
    pike_bin_file+=".exe";
    pike+=".exe";
  }

  if(export)
    to_export += ({ pike_bin_file });
  else {
    status("Finalizing",pike_bin_file);
    string pike_bin=Stdio.read_file(pike_bin_file);

    if (!pike_bin) {
      // Failed to read bin file, most likely Cygwin.

      status("Finalizing",pike_bin_file,"FAILED");
      if (!istty()) {
	werror("Finalizing of %O failed!\n", pike_bin_file);
	werror("Not found in %s.\n"
	       "%O\n", getcwd(), get_dir("."));
	werror("BUILDDIR: %O\n"
	       "exe-stat: %O\n",
	       vars->TMP_BUILDDIR, file_stat(pike_bin_file+".exe"));
      }
      exit(1);
    }

    int pos=search(pike_bin, MASTER_COOKIE);

    if(pos>=0)
    {
      status("Finalizing",pike_bin_file,"...");
      pike_bin_file=combine_path(vars->TMP_BUILDDIR,"pike.tmp");
      Stdio.write_file(pike_bin_file, pike_bin);
      Stdio.File f=Stdio.File(pike_bin_file,"rw");
      f->seek(pos+sizeof(MASTER_COOKIE));
      f->write(combine_path(lib_prefix,"master.pike"));
      f->close();
      status("Finalizing",pike_bin_file,"done");
    }
    else
      werror("Warning! Failed to finalize master location!\n");

    if(install_file(pike_bin_file,pike)) redump_all=1;
  }
}

void do_install()
{
  if(!export)
  {
    status1("Installing Pike in %s, please wait...\n", fakeroot(prefix));
    catch {
      files_to_install = (int)Stdio.read_file
	(combine_path(vars->TMP_BUILDDIR, "num_files_to_install"));

      if(files_to_install)
	progress_bar =
	  Tools.Install.ProgressBar("Installing", 0,
				    files_to_install, 0.0, 0.2);
    };
  }

  mixed err = catch {

      finalize_pike();

#ifdef __NT__
    // Copy needed DLL files (like libmySQL.dll if available).
    foreach(glob("*.dll", get_dir(vars->TMP_BUILDDIR)), string dll_name)
      install_file(combine_path(vars->TMP_BUILDDIR, dll_name),
		   combine_path(exec_prefix, dll_name));
    // Copy the Program Database (debuginfo)
    if(file_stat(combine_path(vars->TMP_BUILDDIR, "pike.pdb")))
      install_file(combine_path(vars->TMP_BUILDDIR, "pike.pdb"),
		   combine_path(exec_prefix, "pike.pdb"));
#endif

#ifndef __NT__
    install_file(combine_path(vars->TMP_BUILDDIR,"rsif"),
		 combine_path(exec_prefix,"rsif"));
    install_file(combine_path(vars->TMP_BUILDDIR,"hilfe"),
		 combine_path(exec_prefix,"hilfe"));
#endif
    install_file(combine_path(vars->TMP_BUILDDIR,"pike.syms"),
		 pike+".syms");

    string master_src=combine_path(vars->LIBDIR_SRC,"master.pike.in");

    if(export)
    {
      string unpack_master = "master.pike";
      if (export == 1) {
#ifdef __NT__
	// We don't want to overwrite the main master...
	// This is undone by the translator.
	unpack_master = "unpack_master.pike";
	make_master(unpack_master, master_src,
		    tmpdir+"/build/lib", tmpdir+"/build", tmpdir+"/lib");
#else
	make_master(unpack_master, master_src, "build/lib", "build", "lib");
#endif
      } else {
	unpack_master = "unpack_master.pike";
	make_master(unpack_master, master_src, "build/lib", "build", "lib");
      }

      to_export+=({
	unpack_master,
	combine_path(vars->TMP_BUILDDIR,"specs"),
	combine_path(vars->TMP_BUILDDIR,
		     "modules/dynamic_module_makefile"),
	combine_path(vars->SRCDIR,"install-welcome"),
	combine_path(vars->SRCDIR,"dumpmaster.pike"),
      });

      void basefile(string x) {
	string from = combine_path(vars->BASEDIR,x);
	if(!Stdio.cp(from, x))
	  werror("Could not copy %s to %s.\n", from ,x);
	to_export += ({ x });
      };

      basefile("ANNOUNCE");
      basefile("COPYING");
      basefile("COPYRIGHT");
    }
    else
      make_master(combine_path(vars->TMP_LIBDIR,"master.pike"), master_src,
		  lib_prefix, include_prefix);

    install_dir(vars->TMP_LIBDIR,lib_prefix,1);
    install_dir(vars->LIBDIR_SRC,lib_prefix,1);

    install_header_files(vars->SRCDIR,include_prefix);
    install_header_files(combine_path(vars->SRCDIR,"code"),
			 combine_path(include_prefix,"code"));
    install_header_files(vars->TMP_BUILDDIR,include_prefix);

    install_file(combine_path(vars->SRCDIR,"make_variables.in"),
		 combine_path(include_prefix,"make_variables.in"));
    install_file(combine_path(vars->SRCDIR,"aclocal.m4"),
		 combine_path(include_prefix,"aclocal.m4"));
    install_file(combine_path(vars->SRCDIR,"run_autoconfig"),
		 combine_path(include_prefix,"run_autoconfig"));
    install_file(combine_path(vars->SRCDIR,"precompile2.sh"),
		 combine_path(include_prefix,"precompile.sh"));

    if (!no_autodoc) {
      // install the core extracted autodocs
      install_file(combine_path(vars->TMP_BUILDDIR, "autodoc.xml"),
		   combine_path(doc_prefix, "src", "core_autodoc.xml"));

      // create a directory for extracted module documentation
      if(!export)
	mkdirhier(combine_path(doc_prefix, "src", "extracted"));

      install_dir(combine_path(vars->TMP_BUILDDIR, "doc_build", "images"),
		  combine_path(doc_prefix, "src", "images"), 0);
      install_dir(combine_path(vars->DOCDIR_SRC, "presentation"),
		  combine_path(doc_prefix, "src", "presentation"), 0);
      install_dir(combine_path(vars->DOCDIR_SRC, "src_images"),
		  combine_path(doc_prefix, "src", "src_images"), 0);
      install_dir(combine_path(vars->DOCDIR_SRC, "structure"),
		  combine_path(doc_prefix, "src", "structure"), 0);
      install_file(combine_path(vars->DOCDIR_SRC,"Makefile"),
		   combine_path(doc_prefix, "src", "Makefile"));
    }
    else if(!export) {
      mkdirhier(combine_path(doc_prefix, "src", "extracted"));
      mkdirhier(combine_path(doc_prefix, "src", "images"));
      mkdirhier(combine_path(doc_prefix, "src", "presentation"));
      mkdirhier(combine_path(doc_prefix, "src", "src_images"));
      mkdirhier(combine_path(doc_prefix, "src", "structure"));
    }

    foreach(({"install_module", "precompile.pike", "smartlink",
	      "fixdepends.sh", "mktestsuite", "test_pike.pike"}), string f)
      install_file(combine_path(vars->TMP_BINDIR,f),
		   combine_path(include_prefix,f));

    if(!export) {
      fix_smartlink(combine_path(vars->TMP_BUILDDIR,
				 "modules/dynamic_module_makefile"),
		    combine_path(include_prefix,"dynamic_module_makefile"),
		    include_prefix);
      fix_smartlink(combine_path(vars->TMP_BUILDDIR,"specs"),
		    combine_path(include_prefix,"specs"), include_prefix);
    }

    if(file_stat(vars->MANDIR_SRC))
    {
      install_dir(vars->MANDIR_SRC,combine_path(man_prefix,"man1"),0);
    }
  };


  status_clear();

  if(err) throw(err);

  catch {
    Stdio.write_file(combine_path(vars->TMP_BUILDDIR,"num_files_to_install"),
		     sprintf("%d\n",installed_files));
    to_export+=({ combine_path(vars->TMP_BUILDDIR,"num_files_to_install") });
  };

  files_to_install=0;

  if(export)
  {
    do_export();
  }else{
    dump_modules();

    // Delete any .pmod files that would shadow the .so
    // files that we just installed. For a new installation
    // this never does anything. -Hubbe
    Array.map(files_to_delete - files_to_not_delete,rm);

#if constant(symlink)
    if(lnk)
    {
      status("Creating",lnk);
      mixed s=file_stat(fakeroot(lnk),1);
      if(s)
      {
	if(!mv(fakeroot(lnk),fakeroot(lnk+".old")))
	{
	  werror("Failed to move %s\n",lnk);
	  exit(1);
	}
      }
      if (old_exec_prefix) {
	mkdirhier(fakeroot(old_exec_prefix));
      }
      mkdirhier(fakeroot(dirname(lnk)));
      symlink(pike,fakeroot(lnk));
      catch {
	rm(fakeroot(lnk)+__MAJOR__+__MINOR__);
	symlink(pike,fakeroot(lnk)+__MAJOR__+__MINOR__);
      };
      status("Creating",lnk,"done");
    }
#endif
  }

  progress_bar = 0;
  status1("Pike installation completed successfully.");
}

int main(int argc, array(string) argv)
{
  foreach(Getopt.find_all_options(argv, ({
    ({"help",Getopt.NO_ARG,({"-h","--help"})}),
    ({"notty",Getopt.NO_ARG,({"-t","--notty"})}),
    ({"--interactive",Getopt.NO_ARG,({"-i","--interactive"})}),
    ({"--new-style",Getopt.NO_ARG,({"--new-style"})}),
    ({"--finalize",Getopt.NO_ARG,({"--finalize"})}),
    ({"--make-master",Getopt.NO_ARG,({"--make-master"})}),
    ({"no-autodoc",Getopt.NO_ARG,({"--no-autodoc","--no-refdoc"})}),
    ({"no-gui",Getopt.NO_ARG,({"--no-gui","--no-x"})}),
    ({"--export",Getopt.NO_ARG,({"--export"})}),
    ({"--wix", Getopt.NO_ARG, ({ "--wix" })}),
    ({"--traditional",Getopt.NO_ARG,({"--traditional"})}),
    }) ),array opt)
    {
      switch(opt[0])
      {
        case "no-autodoc":
	  no_autodoc = 1;
	  break;

	case "no-gui":
	  no_gui=1;
	  break;

	case "help":
	  write(helptext);
	  exit(0);

	case "notty":
	  istty_cache=-1;
	  break;

	default:
	  install_type=opt[0];
      }
    }

  argv=Getopt.get_args(argv);

  foreach(argv[1..], string foo)
    if(sscanf(foo,"%s=%s",string var, string value)==2)
      vars[var]=value;

  if(vars->BASEDIR) {
    if(vars->BASEDIR[-1]!='/') vars->BASEDIR += "/";
    if(!vars->LIBDIR_SRC) vars->LIBDIR_SRC=vars->BASEDIR+"lib";
    if(!vars->MANDIR_SRC) vars->MANDIR_SRC=vars->BASEDIR+"man";
    if(!vars->DOCDIR_SRC) vars->DOCDIR_SRC=vars->BASEDIR+"refdoc";
    if(!vars->SRCDIR) vars->SRCDIR=vars->BASEDIR+"src";
  }
  else if(vars->SRCDIR) {
    // Do some guessing...
    array split = vars->SRCDIR/"/";
    vars->BASEDIR = split[..sizeof(split)-2]*"/"+"/";
  }
  else {
    werror("No BASEDIR.\n");
    exit(1);
  }

  // Some magic for the fakeroot stuff
  string tmp = m_delete(vars, "fakeroot");
  if(tmp && tmp!="")
  {
    if(tmp[-1]=='/' || tmp[-1]=='\\')
      tmp=tmp[..sizeof(tmp)-2];

    // Create the fakeroot if it doesn't exist
    // This must be done with fakeroot unset since
    // it would create fakeroot/fakeroot otherwise
    mkdirhier(tmp);
    vars->fakeroot=tmp;
  }
  return pre_install(argv);
}
