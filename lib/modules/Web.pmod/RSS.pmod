// $Id: RSS.pmod,v 1.5 2003/12/02 00:29:03 nilsson Exp $

#pike __REAL_VERSION__

//! Represents a RSS (RDF Site Summary) file.

static constant ns = "http://purl.org/rss/1.0/";

static class Thing {
  static .RDF rdf;
  static mapping /* (string:string|Standards.URI) */ attributes = ([]);
  .RDF.Resource me;
  constant thing = "";

  mixed `[](string i) {
    if(!zero_type(attributes[i])) return attributes[i];
    return ::`[](i);
  }

  void `[]=(string i, mixed v) {
    if(!zero_type(attributes[i])) {
      if(!stringp(v) && !objectp(v))
	error("%s is string|Standards.URI.\n");

      attributes[i] = v;
      .RDF.Resource attr = rdf->make_resource(ns+thing);
      foreach(rdf->find_statements(me, attr, 0), array s)
	rdf->remove_statement(@s);
      rdf->add_statement(me, attr, rdf->LiteralResource(v));
    } else
      error("No %O variable.\n", i);
  }

  function `-> = `[];
  function `->= = `[]=;

  void create(.RDF _rdf, string|.RDF.Resource a, void|mapping b) {
    rdf = _rdf;
    if(b)
      create1(a,b);
    else
      create2(a);
  }

  static void create1(string about, mapping _attr) {
    me = rdf->make_resource(about);
    foreach(indices(attributes), string i) {
      string|Standards.URI v = _attr[i];
      if(!v) continue;

      attributes[i] = v;
      if(stringp(v))
	rdf->add_statement(me, rdf->make_resource(ns+thing),
			   rdf->LiteralResource(v));
      else
	rdf->add_statement(me, rdf->make_resource(ns+thing),
			   rdf->make_resource(v->raw_uri));
    }
  }

  static void create2(.RDF.Resource _me) {
    me = _me;
    foreach(rdf->find_statements(me,0,0), array r) {
      .RDF.Resource pred = r[1];
      if(pred==rdf->rdf_type) continue;
      if(pred->is_uri_resource && has_prefix(pred->get_uri(), ns)) {
	string attr = pred->get_uri()[sizeof(ns)..];
	.RDF.Resource obj = r[2];
	if(obj->is_literal_resource)
	  attributes[attr] = obj->get_literal();
	else if(obj->is_uri_resource)
	  attributes[attr] = Standards.URI( obj->get_uri() );
	else
	  error("Wrong object in statement.\n");
      }
      else
	error("Unknown stuff %O.\n", r);
    }
  }

  .RDF.Resource get_id() { return me; }
}

//!
class Image {
  inherit Thing;
  constant thing = "image";
  static mapping(string:string|Standards.URI) attributes = ([
    "title" : 0,
    "url" : 0,
    "link" : 0
  ]);
}

//!
class Item {
  inherit Thing;
  constant thing = "item";
  static mapping(string:string|Standards.URI) attributes = ([
    "title" : 0,
    "link" : 0,
    "description" : 0
  ]);
}

//!
class Textinput {
  inherit Thing;
  constant thing = "textinput";
  static mapping(string:string|Standards.URI) attributes = ([
    "title" : 0,
    "description" : 0,
    "name" : 0,
    "link" : 0,
  ]);
}

//!
class Channel {
  inherit Thing;
  static constant thing = "channel";
  static mapping attributes = ([
    "title" : 0,
    "link" : 0,
    "description" : 0,
    "image" : 0,
    "items" : 0,
    "textinput" : 0,
  ]);

  void `[]=(string i, mixed v) {
    if( i=="image" || i=="textinput" ) {
      if(objectp(v)) {
	if(v->me)
	  attributes[i] = Standards.URI(v->me->get_uri());
	else if(v->raw_uri)
	  attributes[i] = v;
	else
	  error("Wrong value type.\n");
	v = rdf->make_resource( attributes[i]->raw_uri );
      }
      else if(stringp(v)) {
	attributes[i] = v;
	v = rdf->LiteralResource(v);
      }
      else
	error("Wrong type. Expected string or Image/Textinput.\n");

      .RDF.Resource attr = rdf->make_resource(ns+i);
      foreach(rdf->find_statements(me, attr, 0), array s)
	rdf->remove_statement(@s);
      rdf->add_statement(me, attr, v);
      return;
    }
    if( i=="textinput" ) {
      if(!arrayp(i)) error("Wrong type. Expected array(Item).\n");
      // FIXME: Stuff here
      return;
    }
    ::`[]=(i, v);
  }

  //!
  void add_item(Item i) {
    if(!attributes->items)
      attributes->items = ({ i });
    else
      attributes->items += ({ i });
  }

  //!
  void remove_item(Item i) {
    attributes->items -= ({ i });
    if(!sizeof(attributes->items)) attributes->items = 0;
  }
}

//!
class Index {
  static .RDF rdf;

  array(Channel)   channels = ({});   //!
  array(Image)     images = ({});     //!
  array(Item)      items = ({});      //!
  array(Textinput) textinputs = ({}); //!

  //!
  void create(.RDF|void _rdf) {
    if(_rdf) {
      rdf = _rdf;
      foreach(rdf->find_statements(0, rdf->rdf_type,
				   rdf->make_resource(ns+"channel")),
	      array r)
	channels += ({ Channel(rdf, r[0]) });
      foreach(rdf->find_statements(0, rdf->rdf_type,
				   rdf->make_resource(ns+"image")),
	      array r)
	images += ({ Image(rdf, r[0]) });
      foreach(rdf->find_statements(0, rdf->rdf_type,
				   rdf->make_resource(ns+"item")),
	      array r)
	items += ({ Item(rdf, r[0]) });
      foreach(rdf->find_statements(0, rdf->rdf_type,
				   rdf->make_resource(ns+"textinput")),
	      array r)
	textinputs += ({ Textinput(rdf, r[0]) });
    }
    else
      rdf = .RDF();
  }
}

//! Returns an @[Index] object, populated with the rss information
//! given in the rss file @[n].
Index parse_xml(string|Parser.XML.Tree.Node n, void|string base) {
  .RDF rdf=.RDF()->parse_xml(n, base);
  return Index(rdf);
}

