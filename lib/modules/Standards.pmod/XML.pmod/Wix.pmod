// $Id: Wix.pmod,v 1.6 2004/11/02 15:00:11 grubba Exp $
//
// 2004-11-01 Henrik Grubbstr�m

//! Helper module for generating Windows Installer XML structures.
//!
//! @seealso
//!   @[Parser.XML.Tree.SimpleNode]

constant wix_ns = "http://schemas.microsoft.com/wix/2003/01/wi";

// FIXME: Generate deterministic output!

class WixNode
{
  inherit Parser.XML.Tree.SimpleElementNode;

  static void create(string name, mapping(string:string) attrs,
		     string|void text)
  {
    ::create(wix_ns + name, attrs);
    mTagName = name;
    mNamespace = wix_ns;
    if (text) {
      add_child(Parser.XML.Tree.SimpleTextNode(text));
    }
  }
}

class UninstallFile
{
  string id;
  string name;

  static void create(string name, string id)
  {
    UninstallFile::name = name;
    UninstallFile::id = id;
  }

  WixNode gen_xml()
  {
    mapping(string:string) attrs = ([
      "Id":id,
      "Name":name,
      "LongName":name,
      "On":"uninstall",
    ]);
    return WixNode("RemoveFile", attrs);
  }
}

class RegistryEntry
{
  string root;
  string key;
  string name;
  string value;
  string id;

  static void create(string root, string key, string name, string value,
		     string id)
  {
    RegistryEntry::root = root;
    RegistryEntry::key = key;
    RegistryEntry::name = name;
    RegistryEntry::value = value;
    RegistryEntry::id = id;
  }

  WixNode gen_xml()
  {
    mapping(string:string) attrs = ([
      "Id":id,
      "Action":"write",
      "Root":root,
      "Key":key,
      "Name":name,
      "Type":"string",
      "Value":value,
    ]);
    return WixNode("Registry", attrs);
  }
}

class Directory
{
  string name;
  string short_name;
  string id;
  Standards.UUID.UUID guid;
  string source;
  multiset(string) short_names = (<>);
  mapping(string:int) sub_sources = ([]);
  mapping(string:File) files = ([]);
  mapping(string:RegistryEntry) other_entries = ([]);
  mapping(string:Directory) sub_dirs = ([]);

  static void create(string name, string parent_guid,
		     string|void id, string|void short_name)
  {
    guid = Standards.UUID.make_version3(name, parent_guid);
    if (!id) id = "_"+guid->str()-"-";
    Directory::name = name;
    Directory::short_name = short_name;
    Directory::id = id;
  }

  string gen_8dot3(string long_name)
  {
    array(string) segs = long_name/".";
    string extension;
    string base = segs[0];
    int truncated;
    if (sizeof(segs) > 1) {
      extension = segs[-1];
      if (sizeof(segs) > 2) {
	base = segs[..sizeof(segs)-2] * "_";
	truncated = 1;
      } else {
	base = segs[0];
      }
      if (sizeof(extension) > 3) {
	extension = extension[..2];
	truncated = 1;
      }
    }
    if ((sizeof(base) > 8) || truncated) {
      int cnt;
      for (cnt = 0; cnt < 100; cnt++) {
	if (cnt < 10) {
	  if (!short_names[base = sprintf("%.6s~%.1d", base, cnt)]) {
	    short_names[base] = 1;
	    break;
	  }
	} else if (!short_names[base = sprintf("%.5s~%.2d", base, cnt)]) {
	  short_names[base] = 1;
	  break;
	}
      }
      if (cnt > 99) error("Too many like-named files: %O\n", long_name);
      //werror("gen_8dot3: %O ==> %O.%O\n", long_name, base, extension);
    }
    if (extension) return sprintf("%s.%s", base, extension);
    return base;
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
	"Name":gen_8dot3(name),
	"LongName":name,
	"Vital":"yes",
	//      "KeyPath":"yes",
	//      "DiskId":"1",
      ]);
      if (source) {
	attrs->src = replace(source, "/", "\\");
      }
      return WixNode("File", attrs);
    }
  }

  Directory low_add_path(array(string) path)
  {
    Directory d = this_object();
    foreach(path, string dir) {
      if (dir == ".") continue;
      d = (d->sub_dirs[dir] ||
	   (d = d->sub_dirs[dir] =
	    Directory(dir, d->guid->encode(), 0, d->gen_8dot3(dir))));
    }
    return d;
  }

  void low_install_file(string dest, string src, string|void id)
  {
    if (!id) {
      id = "_" +
	Standards.UUID.make_version3(dest, guid->encode())->str() -
	"-";
    }
    files[dest] = File(dest, src, id);
    if (has_suffix(src, "/"+dest)) {
      sub_sources[combine_path(src, "..")]++;
    }
  }

  void recurse_install_directory(string dest, string src)
  {
    Directory d = low_add_path(dest/"/");

    foreach(get_dir(src), string fname) {
      string fullname = combine_path(src, fname);
      Stdio.Stat stat = file_stat(fullname);
      if (stat->isdir) {
	d->recurse_install_directory(fname, fullname);
      } else {
	d->low_install_file(fname, fullname);
      }
    }
  }

  void install_file(string dest, string src, string|void id)
  {
    array(string) path = dest/"/";
    Directory d = low_add_path(path[..sizeof(path)-2]);
    d->low_install_file(path[-1], src, id);
  }

  void install_regkey(string path, string root, string key,
		      string name, string value, string id)
  {
    Directory d = low_add_path(path/"/");
    d->other_entries[root+"\\"+key+"\\:"+name] =
      RegistryEntry(root, key, name, value, id);
  }

  void low_uninstall_file(string pattern)
  {
    other_entries[pattern] =
      UninstallFile(pattern,
		    "RF_" +
		    Standards.UUID.make_version3(pattern,
						 guid->encode())->str() -
		    "-");
  }

  void uninstall_file(string pattern)
  {
    array(string) path = pattern/"/";
    Directory d = low_add_path(path[..sizeof(path)-2]);
    d->low_uninstall_file(path[-1]);
  }

  void recurse_uninstall_file(string pattern)
  {
    low_uninstall_file(pattern);
    foreach(sub_dirs;; Directory d) {
      d->recurse_uninstall_file(pattern);
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
      "Name":short_name||name,
    ]);
    if (short_name && (short_name != name)) {
      // Win32 stupidity...
      attrs->LongName = name;
    }
    if (source) {
      attrs->src = replace(source+"/", "/", "\\");
    }
    WixNode node = WixNode("Directory", attrs);
    foreach(sub_dirs;; Directory d) {
      node->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
      node->add_child(d->gen_xml(parent));
    }
    if (sizeof(files) || sizeof(other_entries)) {
      node->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
      WixNode component = WixNode("Component", ([
				    "Id":"C_" + id,
				    "Guid":guid->str(),
				  ]));
      foreach(files;; File f) {
	component->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
	component->add_child(f->gen_xml());
      }
      foreach(other_entries;; RegistryEntry r) {
	component->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
	component->add_child(r->gen_xml());
      }
      node->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
      node->add_child(component);
    }
    node->add_child(Parser.XML.Tree.SimpleTextNode("\n"));
    return node;
  }
}

WixNode get_module_xml(Directory dir, string id, string version,
		       string|void manufacturer, string|void description,
		       string|void guid, string|void comments)
{
  guid = guid || Standards.UUID.new_string();
  mapping(string:string) package_attrs = ([
    "Id":guid,
    "InstallerVersion":"200",
    "Compressed":"yes",
  ]);
  if (manufacturer) {
    package_attrs->Manufacturer = manufacturer;
  }
  if (description) {
    package_attrs->Description = description;
  }
  if (comments) {
    package_attrs->Comments = comments;
  }
  return Parser.XML.Tree.SimpleRootNode()->
    add_child(Parser.XML.Tree.SimpleHeaderNode((["version":"1.0",
						 "encoding":"utf-8"])))->
    add_child(WixNode("Wix", ([	"xmlns":wix_ns ]))->
	      add_child(WixNode("Module", ([
				  "Id":id,
				  "Guid":guid,
				  "Language":"1033",
				  "Version":version,
				]))->
			add_child(WixNode("Package", package_attrs))->
			add_child(dir->gen_xml())));
}

#if 0
string manufacturer;
string name;
string upgrade_code = "FCBB6B90-1608-4A7C-926C-69BBAB366326";
string id = "B50D7CE5-60C6-4FDD-B7EC-9A686FA9FB80";
string package_id = "BC86625F-3A92-4828-A66C-2C3CBAA53AFB";

Directory targetdir = Directory("SourceDir", 0, "TARGETDIR");

void add_file(string path, string source)
{
  string fname = (path/"/")[-1];
  targetdir->add_path(path, File(fname, source));
}

Parser.XML.Tree.SimpleRootNode get_xml_node()
{
  Parser.XML.Tree.SimpleElementNode node;
  Parser.XML.Tree.SimpleRootNode root = Parser.XML.Tree.SimpleRootNode()->
    add_child(Parser.XML.Tree.SimpleHeaderNode((["version": "1.0",
						 "encoding": "utf-8"])))->
    add_child(WixNode("Wix", (["xmlns":wix_ns]))->
	      add_child(WixNode("Product", ([
				  "Manufacturer":manufacturer,
				  "Name":name,
				  "Language":"1033",
				  "UpgradeCode":upgrade_code,
				  "Id":id,
				  "Version":"1.0.0",
				]))->
			add_child(node = WixNode("Package", ([
						   "Manufacturer":manufacturer,
						   "Languages":"1033",
						   "Compressed":"yes",
						   "InstallerVersion":"200",
						   "Platforms":"Intel",
						   "SummaryCodepage":"1252",
						   "Id":product_id,
						 ])))));
  node->add_child(targetdir->get_xml_node());

  return root;
}

#endif /* 0 */
