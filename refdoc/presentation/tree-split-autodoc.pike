/*
 * $Id: tree-split-autodoc.pike,v 1.25 2002/03/06 10:54:01 mast Exp $
 *
 */

inherit "make_html.pike";

mapping (string:Node) refs   = ([ ]);
mapping (string:int)  consts = ([ ]);

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
  n = replace(n, ([ "&gt;":">", "&lt;":"<", "&amp;":"&" ]));
  while(sscanf((string)n,"%[_a-zA-Z0-9]%c%s",string safe, int c, n)==3) {
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

string create_reference(string from, string to) {
      return "<font face='courier'><a href='" +
	"../"*max(sizeof(from/"/") - 2, 0) +
	map(to/".", cquote)*"/" + ".html'>" + to +
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
    PROFILE();
    data = get_parser()->finish( _data )->read();
    ENDPROFILE("Parsing");

    string path = replace(make_class_path(), "()->", ".");
    if(has_suffix(path, "()"))
      path = path[..sizeof(path)-3];
    refs[path] = this_object();

    sort(class_children->name, class_children);
    sort(module_children->name, module_children);
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

	foreach(children, Node c)
	  if(c->name==n) {
	    if(d) {
	      c->data = d;
	      d = 0;
	    }
	    else destruct(c);
	  }
	children -= ({ 0 });
      }
    return children;
  }

  static string parse_node(Parser.HTML p, mapping m, string c) {
    if(m->name)
      this_object()[p->tag_name()+"_children"] +=
	({ Node( p->tag_name(), m->name, c, this_object() ) });
    return "";
  }

  array(string) my_parse_docgroup(Parser.HTML p, mapping m, string c)
  {
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
	parser->add_tag("method", lambda(Parser.HTML p, mapping m) {
				    names += ({ m->name }); } );
	parser->finish(c);
	foreach(Array.uniq(names) - ({ 0, "" }), string name) {
	  method_children +=
	    ({ Node( "method", name, c, this_object() ) });
	}
	return ({ "" });
	break;

      case "constant":
      case "variable":
	string path = make_class_path();
	if(sizeof(path)) path += ".";
	if(!m["homogen-name"]) {
	  Parser.HTML()->add_tags
	    ( ([ "constant":
		 lambda(Parser.HTML p, mapping m, string c) {
		   consts[path + m->name] = 1;
		 },
		 "variable":
		 lambda(Parser.HTML p, mapping m, string c) {
		   consts[path + m->name] = 1;
		 }
	    ]) )->finish(c);
	}
	else
	  consts[path + m["homogen-name"]] = 1;
	break;

      }
    }

    return 0;
  }

  static Parser.HTML get_parser() {
    Parser.HTML parser = Parser.HTML();

    parser->case_insensitive_tag(1);
    parser->xml_tag_syntax(3);

    parser->add_container("docgroup", my_parse_docgroup);
    parser->add_container("module", parse_node);
    parser->add_container("class",  parse_node);

    return parser;
  }

  string make_faked_wrapper(string s)
  {
    if(type=="appendix")
      return "<appendix name='"+name+"'>"+s+"</appendix>";

    if(type=="method")
      s = sprintf("<docgroup homogen-type='method' homogen-name='%s'>\n"
		  "%s\n</docgroup>\n",
		  name, s);
    else
      s = sprintf("<%s name='%s'>\n%s\n</%s>\n",
		  type, name, s, type);
    if(parent)
      return parent->make_faked_wrapper(s);
    else
      return s;
  }

  string _make_filename_low;
  string make_filename_low()
  {
    if(_make_filename_low) return _make_filename_low;
    PROFILE();
    _make_filename_low = parent->make_filename_low()+"/"+cquote(name);
    ENDPROFILE("make_filename_low");
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
    if(vars->param)
      return "<font face='courier'>" + _reference + "</font>";

    if(vars->resolved && refs[vars->resolved])
      return create_reference(make_filename(), vars->resolved);

    if(vars->resolved && consts[vars->resolved])
      return "<font face='courier'>" + _reference + "</font>";

    if(!missing[vars->resolved] && !has_prefix(_reference, "lfun::") && make_class_path()!="GL")
      werror("Missed reference %O (%O) in %s\n", _reference, vars->resolved,
	     make_class_path());
    unresolved++;
    return "<font face='courier'>" + _reference + "</font>";
  }

  string _make_class_path;
  string make_class_path(void|int(0..1) header)
  {
    if(_make_class_path) return _make_class_path;
    PROFILE();
    array a = reverse(parent->get_ancestors());

    _make_class_path = "";
    foreach(a, Node n)
    {
      _make_class_path += n->name;
      if(n->type=="class")
	_make_class_path += "()->";
      else if(n->type=="module")
	_make_class_path += ".";
    }
    _make_class_path += name;

    if(type=="method")
      _make_class_path +="()";

    ENDPROFILE("make_class_path");
    return _make_class_path;
  }

  string make_navbar_really_low(array(Node) children, void|int notables)
  {
    string a="<tr><td>", b="</td></tr>\n";
    if(notables)
    {
      a=""; b="<br />\n";
    }
    string res = "";
    foreach(children, Node node)
    {
      string my_name=node->name;
      if(node->type=="method")
	my_name+="()";
      else
	my_name="<b>"+my_name+"</b>";

      if(node==this_object())
	res += sprintf("%s�&nbsp;%s%s",
		       a, my_name, b);
      else
	res += sprintf("%s�&nbsp;<a href='%s'>%s</a>%s",
		       a, make_link(node), my_name, b);
    }
    return res;
  }

  string make_hier_list(Node node)
  {
    string res="";

    if(node)
    {
      res += make_hier_list(node->parent);

      string my_class_path = (node->name!="")?node->make_class_path():"[Top]";

      if(node == this_object())
	res += sprintf("<b>%s</b><br />\n", my_class_path);
      else
	res += sprintf("<a href='%s'><b>%s</b></a><br />\n",
		       make_link(node), my_class_path);
    }
    return res;
  }

  string make_navbar_low(Node root)
  { 
    string res="";

    res += make_hier_list(root);

    res+="<table border='0' cellpadding='1' cellspacing='0' class='sidebar'>";

    res += make_navbar_really_low(root->module_children);
    if(sizeof(root->class_children))
      res += "<tr><td><br /><b>Classes</b></td></tr>\n" +
	make_navbar_really_low(root->class_children) + "<br />";

    if(root->appendix_children) {
      if(sizeof(root->appendix_children))
	res += "<tr><td><br /><b>Appendices</b></td></tr>\n"+
	  make_navbar_really_low(root->appendix_children) + "<br />";
    }
    else
      res += make_navbar_really_low(root->method_children);

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
      parent->method_children;
  }

  array(Node) find_children()
  {
    return
      class_children+
      module_children+
      method_children;
  }

  Node find_prev_node()
  {
    PROFILE();
    array(Node) siblings = find_siblings();
    int index = search( siblings, this_object() );

    Node tmp;

    if(index==0)
      return parent;

    tmp = siblings[index-1];

    while(sizeof(tmp->find_children()))
      tmp = tmp->find_children()[-1];

    ENDPROFILE("find_prev_node");
    return tmp;
  }

  Node find_next_node(void|int dont_descend)
  {
    PROFILE();
    if(!dont_descend && sizeof(find_children()))
      return find_children()[0];

    array(Node) siblings = find_siblings();
    int index = search( siblings, this_object() );

    Node tmp;
    if(index==sizeof(siblings)-1)
      tmp = parent->find_next_node(1);
    else
      tmp = siblings[index+1];
    ENDPROFILE("find_next_node");
    return tmp;
  }

  static string make_content() {
    string contents;

    string err;
    Parser.XML.Tree.Node n;
    if(err = catch( n = Parser.XML.Tree.parse_input(data)[0] )) {
      werror(err + "\n" + data);
      exit(1);
    }

    resolve_reference = my_resolve_reference;

    if(type=="appendix")
      contents = parse_appendix(n, 1);
    else {
      contents = parse_children(n, "docgroup", parse_docgroup, 1);
      contents += parse_children(n, "module", parse_module, 1);
      contents += parse_children(n, "class", parse_class, 1);
    }

    return contents;
  }

  void make_html(string template, string path)
  {
    class_children->make_html(template, path);
    module_children->make_html(template, path);
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
	"$prev_title$": prev_title,
	"$next_url$": next_url,
	"$next_title$": next_title,
	"$type$": String.capitalize(type),
	"$title$": make_class_path(1),
	"$style$": style,
	"$dotdot$": sizeof(extra_prefix)?extra_prefix:".",
	"$imagedir$":image_prefix(),
      ]));

    Stdio.mkdirhier(combine_path(path+"/"+make_filename(), "../"));
    Stdio.write_file(path+"/"+make_filename(), res);
  }
}

class TopNode {
  inherit Node;

  array(Node) appendix_children = ({ });

  void create(string _data) {
    Parser.HTML parser = Parser.HTML();
    parser->case_insensitive_tag(1);
    parser->xml_tag_syntax(3);
    parser->add_container("module", lambda(Parser.HTML p, mapping args, string c) {
                                      return ({ c }); });
    _data = parser->finish(_data)->read();
    ::create("module", "", _data);
    sort(appendix_children->name, appendix_children);
    type = "";
  }

  Parser.HTML get_parser() {
    Parser.HTML parser = ::get_parser();
    parser->add_container("appendix", parse_node);
    return parser;
  }

  string make_filename_low() { return "__index"; }
  string make_filename() { return "index.html"; }
  array(Node) get_ancestors() { return ({ }); }
  int(0..0) find_prev_node() { return 0; }
  int(0..0) find_next_node() { return 0; }
  string make_class_path(void|int(0..1) header) {
    if(header && sizeof(method_children))
      return "Top level methods";
    return "";
  }

  string make_content() {
    resolve_reference = my_resolve_reference;
    if(!sizeof(method_children)) return "";

    string contents = "<table class='sidebar'><tr>";
    foreach(method_children/( sizeof(method_children)/4.0 ),
            array(Node) children)
      contents += "<td>" + make_navbar_really_low(children, 1) + "</td>";

    contents += "</tr><tr><td colspan='4'>" +
      parse_children(Parser.XML.Tree.parse_input(data),
		     "docgroup", parse_docgroup, 1) +
      "</td></tr></table>";

    return contents;
  }

  void make_html(string template, string path) {
    appendix_children->make_html(template, path);
    ::make_html(template, path);
  }
}

int main(int argc, array(string) argv)
{
  PROFILE();
  if(argc!=4) {
    werror("Too few arguments. (in-file, template, out-dir)\n");
    return 1;
  }

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
  template = replace(template, ([ "$version$":version(),
				  "$date$":sprintf("%4d-%02d-%02d", m->year+1900, m->mon+1, m->mday),
  ]) );

  werror("Splitting to destination directory %s...\n", argv[3]);
  TopNode top = TopNode(doc);
  top->make_html(template, argv[3]);
  ENDPROFILE("main");

  foreach(sort(indices(profiling)), string f)
    werror("%s: %.1f\n", f, profiling[f]/1000000.0);
  werror("%d unresolved references.\n", unresolved);
  werror("%d documented functions/classes/modules.\n", sizeof(refs));
  werror("%d documented constants/variables.\n", sizeof(consts));

  return 0;
}
