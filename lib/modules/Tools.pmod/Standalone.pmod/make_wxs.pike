/*
 * $Id: make_wxs.pike,v 1.2 2004/11/02 15:10:40 grubba Exp $
 *
 * Make a Wix modules source XML file from an existing directory.
 *
 * 2004-11-02 Henrik Grubbstr�m
 */

int main(int argc, array(string) argv)
{
  string base_guid = Standards.UUID.new_string();
  string version_str = "1.0";
  string property = "TARGETDIR";
  string id;
  string descr;
  string manufacturer;
  string comments;

  foreach(Getopt.find_all_options(argv, ({
    ({"--guid", Getopt.HAS_ARG, ({"-g", "--guid"})}),
    ({"--version", Getopt.MAY_HAVE_ARG, ({"-v", "--version"})}),
    ({"--property", Getopt.HAS_ARG, ({"-p", "--prop", "--property"})}),
    ({"--id", Getopt.HAS_ARG, ({"-i", "--id", "--identifier"})}),
    ({"--description", Getopt.HAS_ARG, ({"-d", "--descr", "--description"})}),
    ({"--manufacturer", Getopt.HAS_ARG, ({"-m", "--manufacturer"})}),
    ({"--comments", Getopt.HAS_ARG, ({"-c", "--comment", "--comments"})}),
  })), array(string) opt) {
    switch(opt[0]) {
    case "--guid":
      base_guid = Standards.UUID.UUID(opt[1])->str();
      break;
    case "--version":
      if (stringp(opt[1])) {
	version_str = opt[1];
      } else {
	write("$Revision: 1.2 $\n");
	exit(0);
      }
      break;
    case "--property":
      property = opt[1];
      break;
    case "--id":
      id = opt[1];
      break;
    case "--description":
      descr = opt[1];
      break;
    case "--manufacturer":
      manufacturer = opt[1];
      break;
    case "--comments":
      comments = opt[1];
      break;
    }
  }

  if (!id) {
    werror("No identifier specified.\n");
    exit(1);
  }

  argv = Getopt.get_args(argv);

  string version_guid =
    Standards.UUID.make_version3(base_guid, version_str)->str();
  Standards.XML.Wix.Directory root =
    Standards.XML.Wix.Directory("SourceDir",
				Standards.UUID.UUID(version_guid)->encode(),
				property);

  foreach(argv[1..], string srcdir) {
    array(string) seg;
    if (sizeof(seg = (srcdir/":")) > 1) {
      root->recurse_install_directory(seg[0], seg[1..]*":");
    } else {
      root->recurse_install_directory(".", srcdir);
    }
  }

  root->set_sources();

  write(Standards.XML.Wix.get_module_xml(root, id, version_str,
					 manufacturer, descr, version_guid,
					 comments)->render_xml());
}