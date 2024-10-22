/*
 * $Id$
 *
 */

inherit "make_html.pike";

mapping (string:Node) refs   = ([ ]);
string default_namespace;

string extra_prefix = "";
string image_prefix()
{
  return extra_prefix + ::image_prefix();
}

int unresolved;
mapping profiling = ([]);
#define PROFILE int profilet=gethrtime
#define ENDPROFILE(X) profiling[(X)] += gethrtime()-profilet;

string cquote(string n)
{
  string ret="";
  // n = replace(n, ([ "&gt;":">", "&lt;":"<", "&amp;":"&" ]));
  while(sscanf((string)n,"%[._a-zA-Z0-9]%c%s",string safe, int c, n)==3) {
    switch(c) {
    default:  ret += sprintf("%s_%02X",safe,c); break;
    case '+': ret += sprintf("%s_add",safe); break;
    case '`': ret += sprintf("%s_backtick",safe);  break;
    case '=': ret += sprintf("%s_eq",safe); break;
    }
  }
  ret += n;
  return ret;
}

string create_reference(string from, string to, string text) {
  array a = (to/"::");
  switch(sizeof(a)) {
  case 2:
    if (sizeof(a[1])) {
      // Split trailing module path.
      a = a[0..0] + a[1]/".";
    } else {
      // Get rid of trailing "".
      a = a[0..0];
    }
    a[0] += "::";
    break;
  case 1:
    a = a[0]/".";
    break;
  default:
    error("Bad reference: %O\n", to);
  }
  return "<font face='courier'><a href='" +
    "../"*max(sizeof(from/"/") - 2, 0) + map(a, cquote)*"/" + ".html'>" +
    text +
    "</a></font>";
}

multiset missing = (< "foreach", "catch", "throw", "sscanf", "gauge", "typeof" >);


class Node
{
  string type;
  string name;
  string data;
  array(Node) class_children  = ({ });
  array(Node) module_children = ({ });
  array(Node) enum_children = ({ });
  array(Node) method_children = ({ });

  Node parent;

  string _sprintf() {
    return sprintf("Node(%O,%O,%d)", type, name, data?sizeof(data):0);
  }

  void create(string _type, string _name, string _data, void|Node _parent)
  {
    if(!_type || !_name) throw( ({ "No type or name\n", backtrace() }) );
    type = _type;
    name = _name;
    parent = _parent;
    data = get_parser()->finish( _data )->read();

    string path = raw_class_path();
    refs[path] = this_object();

    sort(class_children->name, class_children);
    sort(module_children->name, module_children);
    sort(enum_children->name, enum_children);
    sort(method_children->name, method_children);

    method_children = check_uniq(method_children);

    /*
    foreach(method_children, Node m)
      if( (<"create","destroy">)[m->name] ) {
	method_children -= ({ m });
	string d;
	sscanf(m->data, "%*s<docgroup%s/docgroup>", d);
	if(d)
	  data += "<docgroup" + d + "/docgroup>";
      }
    */

    data = make_faked_wrapper(data);
  }

  array(Node) check_uniq(array children) {
    array names = children->name;
    foreach(Array.uniq(names), string n)
      if(sizeof(filter(names, lambda(string in) { return in==n; }))!=1) {
	string d="";
	Parser.HTML parser = Parser.HTML();
	parser->case_insensitive_tag(1);
	parser->xml_tag_syntax(3);
	parser->add_container("docgroup",
          lambda(Parser.HTML p, mapping m, string c) {
	    d += sprintf("<docgroup%{ %s='%s'%}>%s</docgroup>",
			 (array)m, c);
	    return "";
	  });

	foreach(children, Node c)
	  if(c->name==n) parser->feed(c->data);
	parser->finish();
	d = make_faked_wrapper(d);

	foreach(children; int i; Node c)
	  if(c->name==n) {
	    if(d) {
	      c->data = d;
	      d = 0;
	    }
	    else children[i] = 0;
	  }
	children -= ({ 0 });
      }
    return children;
  }

  protected string parse_node(Parser.HTML p, mapping m, string c) {
    if(!m->name) error("Unnamed %O %O\n", p->tag_name(), m);
    this_object()[p->tag_name()+"_children"] +=
      ({ Node( p->tag_name(), m->name, c, this_object() ) });
    return "";
  }

  array(string) my_parse_docgroup(Parser.HTML p, mapping m, string c)
  {
    foreach(({"homogen-name", "belongs"}), string attr) {
      if (m[attr]) m[attr] = Parser.parse_html_entities(m[attr]);
    }
    if(m["homogen-type"]) {
      switch( m["homogen-type"] ) {

      case "method":
	if( m["homogen-name"] ) {
	  string name = m["homogen-name"];
	  if(m->belongs) {
	    if(m->belongs[-1]==':') name = m->belongs + name;
	    else name = m->belongs + "." + name;
	  }
	  method_children +=
	    ({ Node( "method", name, c, this_object() ) });
	  return ({ "" });
	}

	// Several different methods documented with the same blurb.
	array names = ({});
	Parser.HTML parser = Parser.HTML();
	parser->case_insensitive_tag(1);
	parser->xml_tag_syntax(0);
	parser->add_tag("method",
			lambda(Parser.HTML p, mapping m) {
			  names += ({ Parser.parse_html_entities(m->name) });
			} );
	parser->finish(c);
	foreach(Array.uniq(names) - ({ 0, "" }), string name) {
	  method_children +=
	    ({ Node( "method", name, c, this_object() ) });
	}
	return ({ "" });
	break;

      case "constant":
      case "variable":
      case "inherit":
	string path = raw_class_path();
	if(sizeof(path) && (path[-1] != ':')) path += ".";
	if(!m["homogen-name"]) {
	  Parser.HTML()->add_tags
	    ( ([ "constant":
		 lambda(Parser.HTML p, mapping m, string c) {
		   string name = Parser.parse_html_entities(m->name);
		   refs[path + name] =
		     Node( "constant", name, "", this_object());
		 },
		 "variable":
		 lambda(Parser.HTML p, mapping m, string c) {
		   string name = Parser.parse_html_entities(m->name);
		   refs[path + name] =
		     Node( "variable", name, "", this_object());
		 },
		 "inherit":
		 lambda(Parser.HTML p, mapping m, string c) {
		   if (m->name) {
		     string name = Parser.parse_html_entities(m->name);
		     refs[path + name] =
		       Node( "inherit", name, "", this_object());
		   }
		 },
	    ]) )->finish(c);
	}
	else
	  refs[path + m["homogen-name"]] =
	    Node( m["homogen-type"], m["homogen-name"], "", this_object());
	break;

      }
    }

    return 0;
  }

  protected Parser.HTML get_parser() {
    Parser.HTML parser = Parser.HTML();

    parser->case_insensitive_tag(1);
    parser->xml_tag_syntax(3);

    parser->add_container("docgroup", my_parse_docgroup);
    parser->add_container("module", parse_node);
    parser->add_container("class",  parse_node);
    parser->add_container("enum",  parse_node);

    return parser;
  }

  string make_faked_wrapper(string s)
  {
    if(type=="method")
      s = sprintf("<docgroup homogen-type='method' homogen-name='%s'>\n"
		  "%s\n</docgroup>\n",
		  Parser.encode_html_entities(name), s);
    else
      s = sprintf("<%s name='%s'>\n%s\n</%s>\n",
		  type, Parser.encode_html_entities(name), s, type);
    if(parent)
      return parent->make_faked_wrapper(s);
    else
      return s;
  }

  string _make_filename_low;
  string make_filename_low()
  {
    if(_make_filename_low) return _make_filename_low;
    if (type == "namespace") {
      _make_filename_low = parent->make_filename_low()+"/"+cquote(name+"::");
    } else {
      _make_filename_low = parent->make_filename_low()+"/"+cquote(name);
    }
    return _make_filename_low;
  }

  string make_filename()
  {
    return make_filename_low()[5..]+".html";
  }

  string make_link(Node to)
  {
    // FIXME: Optimize the length of relative links
    int num_segments = sizeof(make_filename()/"/") - 1;
    return ("../"*num_segments)+to->make_filename();
  }

  array(Node) get_ancestors()
  {
    PROFILE();
    array tmp = ({ this_object() }) + parent->get_ancestors();
    ENDPROFILE("get_ancestors");
    return tmp;
  }

  string my_resolve_reference(string _reference, mapping vars)
  {
    array(string) resolved = vars->resolved && vars->resolved/"\0";
    if(default_namespace && has_prefix(_reference, default_namespace+"::"))
      _reference = _reference[sizeof(default_namespace)+2..];

    if(vars->param)
      return "<font face='courier'>" + _reference + "</font>";

    if (resolved) {
      foreach (resolved, string resolution) {
	Node res_obj;

	if(res_obj = refs[resolution]) {
	  while(res_obj && (<"constant","variable">)[res_obj->type]) {
	    res_obj = res_obj->parent;
	  }
	  if (!res_obj) {
	    werror("Found no page to link to for reference %O (%O)\n",
		   _reference, resolution);
	    return sprintf("<font face='courier'>" + _reference + "</font>");
	  }
	  // FIXME: Assert that the reference is correct?
	  return create_reference(make_filename(),
				  res_obj->raw_class_path(),
				  _reference);
	}

	// Might be a reference to a parameter.
	// Try cutting of the last section, and retry.
	array(string) tmp = resolution/".";
	if ((sizeof(tmp) > 1) && (res_obj = refs[tmp[..sizeof(tmp)-2]*"."])) {
	  if (res_obj == this_object()) {
	    return sprintf("<font face='courier'>" + _reference + "</font>");
	  }
	  return create_reference(make_filename(),
				  res_obj->raw_class_path(),
				  _reference);
	}

	if (!zero_type(refs[resolution])) {
	  werror("Reference %O (%O) is %O!\n",
		 _reference, resolution, refs[resolution]);
	}
      }
      if (!missing[vars->resolved] && !has_prefix(_reference, "lfun::")) {
	werror("Missed reference %O (%{%O, %}) in %s\n",
	       _reference, resolved||({}), make_class_path());
#if 0
	werror("Potential refs:%O\n",
	       sort(map(resolved,
			lambda (string resolution) {
			  return filter(indices(refs),
					glob(resolution[..sizeof(resolution)/2]+
					     "*", indices(refs)[*]));
			}) * ({})));
#endif /* 0 */
      }
      unresolved++;
    }
    return "<font face='courier'>" + _reference + "</font>";
  }

  string _make_class_path;
  string _raw_class_path;
  string make_class_path(void|int(0..1) header)
  {
    if(_make_class_path) return _make_class_path;
    array a = reverse(parent->get_ancestors());

    _make_class_path = "";
    _raw_class_path = "";
    foreach(a, Node n)
    {
      // Hide most namepaces from the class path.
      if (n->type == "namespace") {
	_raw_class_path += n->name + "::";
	if ((<"","lfun">)[n->name]) {
	  _make_class_path += n->name + "::";
	}
      } else {
	_raw_class_path += n->name + ".";
	_make_class_path += n->name;
	if(n->type=="class")
	  _make_class_path += "()->";
	else if(n->type=="module")
	  _make_class_path += ".";
      }
    }
    _make_class_path += name;
    _raw_class_path += name;

    if(type=="method") {
      _make_class_path += "()";
    } else if (type == "namespace") {
      _make_class_path += "::";
      _raw_class_path += "::";
    }

    return _make_class_path;
  }
  string raw_class_path(void|int(0..1) header)
  {
    if(_raw_class_path) return _raw_class_path;
    make_class_path(header);
    return _raw_class_path;
  }

  string make_navbar_really_low(array(Node) children, string what)
  {
    if(!sizeof(children)) return "";

    String.Buffer res = String.Buffer(3000);
    res->add("<tr><td nowrap='nowrap'><br /><b>", what, "</b></td></tr>\n");

    foreach(children, Node node)
    {
      string my_name = Parser.encode_html_entities(node->name);
      if(node->type=="method")
	my_name+="()";
      else if (node->type == "namespace") {
	my_name="<b>"+my_name+"::</b>";
      }
      else 
	my_name="<b>"+my_name+"</b>";

      res->add("<tr><td nowrap='nowrap'>&nbsp;");
      if(node==this_object())
	res->add( my_name );
      else
	res->add( "<a href='", make_link(node), "'>", my_name, "</a>" );
      res->add("</td></tr>\n");
    }
    return (string)res;
  }

  string make_hier_list(Node node)
  {
    string res="";

    if(node)
    {
      if(node->type=="namespace" && node->name==default_namespace)
	node = node->parent;
      res += make_hier_list(node->parent);

      string my_class_path =
	(node->is_TopNode)?"[Top]":node->make_class_path();

      if(node == this_object())
	res += sprintf("<b>%s</b><br />\n",
		       Parser.encode_html_entities(my_class_path));
      else
	res += sprintf("<a href='%s'><b>%s</b></a><br />\n",
		       make_link(node),
		       Parser.encode_html_entities(my_class_path));
    }
    return res;
  }

  string make_navbar_low(Node root)
  {
    string res="";

    res += make_hier_list(root);

    res+="<table border='0' cellpadding='1' cellspacing='0' class='sidebar'>";

    res += make_navbar_really_low(root->module_children, "Modules");

    res += make_navbar_really_low(root->class_children, "Classes");

    if(root->is_TopNode)
      res += make_navbar_really_low(root->namespace_children, "Namespaces");
    else {
      res += make_navbar_really_low(root->enum_children, "Enums");
      res += make_navbar_really_low(root->method_children, "Methods");
    }

    return res+"</table>";
  }

  string make_navbar()
  {
    if(type=="method")
      return make_navbar_low(parent);
    else
      return make_navbar_low(this_object());
  }

  array(Node) find_siblings()
  {
    return
      parent->class_children+
      parent->module_children+
      parent->enum_children+
      parent->method_children;
  }

  array(Node) find_children()
  {
    return
      class_children+
      module_children+
      enum_children+
      method_children;
  }

  Node find_prev_node()
  {
    array(Node) siblings = find_siblings();
    int index = search( siblings, this_object() );

    Node tmp;

    if(index==0 || index == -1)
      return parent;

    tmp = siblings[index-1];

    while(sizeof(tmp->find_children()))
      tmp = tmp->find_children()[-1];

    return tmp;
  }

  Node find_next_node(void|int dont_descend)
  {
    if(!dont_descend && sizeof(find_children()))
      return find_children()[0];

    array(Node) siblings = find_siblings();
    int index = search( siblings, this_object() );

    Node tmp;
    if(index==sizeof(siblings)-1)
      tmp = parent->find_next_node(1);
    else
      tmp = siblings[index+1];
    return tmp;
  }

  protected string make_content() {
    PROFILE();
    string err;
    Parser.XML.Tree.Node n;
    if(err = catch( n = Parser.XML.Tree.parse_input(data)[0] )) {
      werror(err + "\n" + data);
      exit(1);
    }
    ENDPROFILE("XML.Tree");

    resolve_reference = my_resolve_reference;

    String.Buffer contents = String.Buffer(100000);
    resolve_class_paths(n);
    contents->add( parse_children(n, "docgroup", parse_docgroup, 1) );
    contents->add( parse_children(n, "namespace", parse_namespace, 1) );
    contents->add( parse_children(n, "module", parse_module, 1) );
    contents->add( parse_children(n, "class", parse_class, 1) );
    contents->add( parse_children(n, "enum", parse_enum, 1) );

    return (string)contents;
  }

  void make_html(string template, string path)
  {
    class_children->make_html(template, path);
    module_children->make_html(template, path);
    enum_children->make_html(template, path);
    method_children->make_html(template, path);

    int num_segments = sizeof(make_filename()/"/")-1;
    string style = ("../"*num_segments)+"style.css";
    extra_prefix = "../"*num_segments;

    Node prev = find_prev_node();
    Node next = find_next_node();
    string next_url="", next_title="", prev_url="", prev_title="";
    if(next) {
      next_title = next->make_class_path();
      next_url   = make_link(next);
    }
    if(prev) {
      prev_title = prev->make_class_path();
      prev_url   = make_link(prev);
    }

    string res = replace(template,
      (["$navbar$": make_navbar(),
	"$contents$": make_content(),
	"$prev_url$": prev_url,
	"$prev_title$": _Roxen.html_encode_string(prev_title),
	"$next_url$": next_url,
	"$next_title$": _Roxen.html_encode_string(next_title),
	"$type$": String.capitalize(type),
	"$title$": _Roxen.html_encode_string(make_class_path(1)),
	"$style$": style,
	"$dotdot$": sizeof(extra_prefix)?extra_prefix:".",
	"$imagedir$":image_prefix(),
	"$filename$": _Roxen.html_encode_string(make_filename()),
      ]));

    Stdio.mkdirhier(combine_path(path+"/"+make_filename(), "../"));
    Stdio.write_file(path+"/"+make_filename(), res);
  }
}

class TopNode {
  inherit Node;

  constant is_TopNode = 1;
  array(Node) namespace_children = ({ });

  void create(string _data) {
    PROFILE();
    Parser.HTML parser = Parser.HTML();
    parser->case_insensitive_tag(1);
    parser->xml_tag_syntax(3);
    parser->add_container("autodoc",
			  lambda(Parser.HTML p, mapping args, string c) {
			    return ({ c }); });

    _data = parser->finish(_data)->read();
    ::create("autodoc", "", _data);
    sort(namespace_children->name, namespace_children);
    foreach(namespace_children, Node x)
      if(x->type=="namespace" && x->name==default_namespace) {
	//	namespace_children -= ({ x });
	class_children += x->class_children;
	module_children += x->module_children;
	enum_children += x->enum_children;
	method_children += x->method_children;
      }
    type = "autodoc";
    ENDPROFILE("top_create");
  }

  Parser.HTML get_parser() {
    Parser.HTML parser = ::get_parser();
    parser->add_container("namespace", parse_node);
    return parser;
  }

  string make_filename_low() { return "__index"; }
  string make_filename() { return "index.html"; }
  array(Node) get_ancestors() { return ({ }); }
  int(0..0) find_prev_node() { return 0; }
  int(0..0) find_next_node() { return 0; }
  string make_class_path(void|int(0..1) header) {
    if(header && sizeof(method_children)) {
      if(default_namespace)
	return "namespace "+default_namespace;
      else
	return "Namespaces";
    }
    return "";
  }
  string raw_class_path(void|int(0..1) header) {
    return "";
  }

  string make_method_page(array(Node) children)
  {
    String.Buffer res = String.Buffer(3500);
    foreach(children, Node node)
      res->add("&nbsp;<a href='", make_link(node), "'>",
	       Parser.encode_html_entities(node->name),
	       "()</a><br />\n");
    return (string)res;
  }

  string make_content() {
    resolve_reference = my_resolve_reference;
    if(!sizeof(method_children)) return "";

    string contents = "<table class='sidebar'><tr>";
    foreach(method_children/( sizeof(method_children)/4.0 ),
            array(Node) children)
      contents += "<td nowrap='nowrap' valign='top'>" +
	make_method_page(children) + "</td>";

    contents += "</tr><tr><td colspan='4' nowrap='nowrap'>" +
      parse_children(Parser.XML.Tree.parse_input(data),
		     "docgroup", parse_docgroup, 1) +
      "</td></tr></table>";

    return contents;
  }

  void make_html(string template, string path) {
    PROFILE();
    namespace_children->make_html(template, path);
    ::make_html(template, path);
    ENDPROFILE("top_make_html");
  }
}

int main(int argc, array(string) argv)
{
  PROFILE();
  if(argc<4) {
    werror("Too few arguments. (in-file, template, out-dir (, namespace))\n");
    return 1;
  }
  if(argc>4) default_namespace=argv[4];

  werror("Reading refdoc blob %s...\n", argv[1]);
  string doc = Stdio.read_file(argv[1]);
  if(!doc) {
    werror("Failed to load refdoc blob %s.\n", argv[1]);
    return 1;
  }

  werror("Reading template file %s...\n", argv[2]);
  string template = Stdio.read_file(argv[2]);
  if(!template) {
    werror("Failed to load template %s.\n", argv[2]);
    return 1;
  }
  mapping m = localtime(time());
  template = replace(template,
		     ([ "$version$":version(),
			"$date$":sprintf("%4d-%02d-%02d",
					 m->year+1900, m->mon+1, m->mday),
		     ]) );

  werror("Splitting to destination directory %s...\n", argv[3]);
  TopNode top = TopNode(doc);
  top->make_html(template, argv[3]);
  ENDPROFILE("main");

  foreach(sort(indices(profiling)), string f)
    werror("%s: %.1f\n", f, profiling[f]/1000000.0);
  werror("%d unresolved references.\n", unresolved);
  werror("%d documented constants/variables/functions/classes/modules.\n",
	 sizeof(refs));
  return 0;
}
