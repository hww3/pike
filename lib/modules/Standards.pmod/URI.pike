//! This class implements URI parsing and resolving of relative references to
//! absolute form, as defined in RFC 2396

// Implemented by Johan Sundstr�m and Johan Sch�n.
// Copyright (c) Roxen Internet Software 2001
// $Id: URI.pike,v 1.5 2001/01/14 16:16:02 jhs Exp $

#pragma strict_types

//! Scheme component of URI
string scheme;

//! Authority component of URI (formerly called net_loc, from RFC 2396 known as authority)
string authority;

//! Path component of URI. May be empty, but not undefined.
string path;

//! Query component of URI. May be 0 if not present.
string query;

//! The fragment part of URI. May be 0 if not present.
string fragment;

//! Certain classes of URI (e.g. URL) may have these defined
string host, user, password;

//! If no port number is present in URI, but the scheme used has a default port number,
//! this number is put here.
int port;

//! The base URI object, if present
object(this_program) base_uri; 

string raw_uri;

#define error(S) throw( ({ (S), backtrace() }) )
#ifdef STANDARDS_URI_DEBUG
#define debug(X, Y) werror("Standards.URI: "+X+"\n", Y)
#define debug_n(X, Y) werror("Standards.URI: "+X+"\n", @Y)
#else
#define debug(X, Y)
#define debug_n(X, Y)
#endif

// Parse authority component (according to RFC 1738, � 3.1)
static void parse_authority()
{ 
  if(sscanf(authority, "%[^@]@%s", string auth, authority) == 2)
  {
    sscanf(auth, "%[^:]:%s", user, password); // auth info present
    debug_n("parse_authority(): user=%O, password=%O",
	      ({ user, password }));
  }
  if(scheme)
    port = [int]Protocols.Ports.tcp[scheme]; // Set a good default � la RFC 1700
  sscanf(authority, "%[^:]%*[:]%d", host, port);
  debug_n("parse_authority(): host=%O, port=%O", ({ host, port }));
}


// Inherit all properties except raw_uri and base_uri from the URI uri. :-)
static void inherit_properties(object(this_program) uri)
{ 
  authority = uri->authority;
  scheme = uri->scheme;
  user = uri->user; password = uri->password;
  host = uri->host; query = uri->query;
  port = uri->port;
  path = uri->path; fragment = uri->fragment;
}

//! Compare this URI to something, in a canonical way.
//! @param something
//!   Compare the URI to this
int `==(mixed something)
{
  return
    _sprintf('t') == sprintf("%t", something) &&
    _sprintf('x') == sprintf("%x", something);
}

string combine_uri_path(string base, string rel)
{
  string buf;
  
  // RFC 2396, �5.2.6:
  // a) All but the last segment of the base URI's path component is
  //    copied to the buffer.  In other words, any characters after the
  //    last (right-most) slash character, if any, are excluded.
  array segments=base/"/";
  if(has_value(base, "/"))
    buf=segments[..sizeof(segments)-2]*"/"+"/";
  else
    buf=base;
  
  // b) The reference's path component is appended to the buffer string.
  buf+=rel;
  segments = buf / "/";
  
  // c) All occurrences of "./", where "." is a complete path segment,
  //    are removed from the buffer string.
  for(int i=0; i<sizeof(segments)-1; i++)
    if(segments[i]==".")
      segments[i]=0;
	  
  segments -= ({0}); 
  
  // d) If the buffer string ends with "." as a complete path segment,
  //    that "." is removed.
  if(segments[-1]==".")
    segments=segments[..sizeof(segments)-2]+({""});

  // e) All occurrences of "<segment>/../", where <segment> is a
  //    complete path segment not equal to "..", are removed from the
  //    buffer string.  Removal of these path segments is performed
  //    iteratively, removing the leftmost matching pattern on each
  //    iteration, until no matching pattern remains.
  int found_pattern;
  do
  {
    found_pattern=0;
    if(sizeof(segments)<3)
      continue;
    for(int i=0; i<sizeof(segments)-2; i++)
    {
      if(segments[i]!=".." && segments[i]!="" && segments[i+1]=="..")
      {
	segments = segments[..i-1]+segments[i+2..];
	found_pattern=1;
	continue;
      }
    }

  } while(found_pattern);

  // f) If the buffer string ends with "<segment>/..", where <segment>
  //    is a complete path segment not equal to "..", that
  //    "<segment>/.." is removed.
  if(sizeof(segments)>=2)
    if(segments[-2]!=".." && segments[-1]=="..")
      segments=segments[..sizeof(segments)-3]+({""});

  // g) If the resulting buffer string still begins with one or more
  //    complete path segments of "..", then the reference is
  //    considered to be in error.  Implementations may handle this
  //    error by retaining these components in the resolved path (i.e.,
  //    treating them as part of the final URI), by removing them from
  //    the resolved path (i.e., discarding relative levels above the
  //    root), or by avoiding traversal of the reference.
  return segments * "/";
}


//! @decl void reparse_uri()
//! @decl void reparse_uri(URI base_uri)
//! @decl void reparse_uri(string base_uri)
//! Reparse the URI with respect to a new base URI. If
//! no base_uri was supplied, the old base_uri is thrown away.
//! The resolving is performed according to the guidelines
//! outlined by RFC 2396, Uniform Resource Identifiers (URI): Generic Syntax.
//! @param base_uri
//!   Set the new base URI to this.  
void reparse_uri(object(this_program)|string|void base_uri)
{
  string uri = raw_uri;
  
  if(stringp(base_uri))
  {
    debug("cloning base URI %O", base_uri);
    local::base_uri = object_program(this_object())(base_uri); // create a new URI object
  }
  else
    local::base_uri = [object(this_program)]base_uri;

  // RFC 2396, �5.2:
  // 1) The URI reference is parsed into the potential four components and
  //    fragment identifier, as described in Section 4.3.


  // 2) If the path component is empty and the scheme, authority, and
  //    query components are undefined, then it is a reference to the
  //    current document and we are done.  Otherwise, the reference URI's
  //    query and fragment components are defined as found (or not found)
  //    within the URI reference and not inherited from the base URI.
  //    (Doing this at once saves us some useless parsing efforts.)
  if(!raw_uri || raw_uri == "")
  { 
    debug("Path is empty -- Inherit entire base URI as per RFC 2396, �5.2 step 2. Done!", 0);
    inherit_properties(local::base_uri);
    return;
  }

  if(raw_uri[0] == '#')
  { 
    debug("Fragment only. Using entire base URI, except fragment.", 0);
    inherit_properties(local::base_uri);
    fragment=raw_uri[1..];
    return;
  }
  
  // Parse fragment identifier
  sscanf(uri, "%s#%s", uri, fragment);     
  debug("Found fragment %O", fragment);

  // Parse scheme
  if(sscanf(uri, "%[A-Za-z0-9+.-]:%s", scheme, uri) < 2)
  {
    scheme = 0;
    if(!local::base_uri)
	error("Standards.URI: got a relative URI (no scheme) lacking a base_uri!\n");
  }
  debug("Found scheme %O", scheme);

  // Parse authority/login
  if(sscanf(uri, "//%[^/]%s", authority, uri))		
  {
    debug("Found authority %O", authority);
  }

  // Parse query information
  sscanf(uri, "%s?%s", uri, query);
  debug("Found query %O", query);

  // Parse path:
  path = uri;						
  debug("Found path %O", path);

  // 3) If the scheme component is defined, indicating that the reference
  //    starts with a scheme name, then the reference is interpreted as an
  //    absolute URI and we are done.  Otherwise, the reference URI's
  //    scheme is inherited from the base URI's scheme component.
  if(scheme)
  {					
    if(authority)
      parse_authority();

    debug("Scheme found! RFC 2396, �5.2, step 3 says we're absolute. Done!", 0);
    return;		
  }
  scheme = local::base_uri->scheme;			
  debug("Inherited scheme %O from base URI", scheme);

  if(authority)
    parse_authority();

  
  // 4) If the authority component is defined, then the reference is a
  //	network-path and we skip to step 7.  Otherwise, the reference
  //	URI's authority is inherited from the base URI's authority
  //	component, which will also be undefined if the URI scheme does not
  //	use an authority component.
  if(!authority || !sizeof(authority))			
  {
    authority = local::base_uri->authority;
    debug("Inherited authority %O from base URI", authority);
    parse_authority();

    // 5) If the path component begins with a slash character ("/"), then
    //    the reference is an absolute-path and we skip to step 7.
    if(!sscanf(path, "/%*s"))
    {

      // 6) If this step is reached, then we are resolving a relative-path
      //    reference.  The relative path needs to be merged with the base
      //    URI's path.  Although there are many ways to do this, we will
      //    describe a simple method using a separate string buffer.

      debug_n("Combining base path %O with path %O => %O",
	      ({ local::base_uri->path, path,
		 combine_uri_path(local::base_uri->path, path) }));
      path = combine_uri_path(local::base_uri->path, path); 
      
    }
  }

  // 7) The resulting URI components, including any inherited from the
  //    base URI, are recombined to give the absolute form of the URI reference.
  //    (Reassembly is done at cast-to-string/sprintf() time)
}


//! @decl void create(URI uri)
//! @decl void create(URI uri, URI base_uri)
//! @decl void create(URI uri, string base_uri)
//! @decl void create(string uri)
//! @decl void create(string uri, URI base_uri)
//! @decl void create(string uri, string base_uri)
//! @param base_uri
//!   When supplied, will root the URI a the given location. This is
//!   needed to correctly verify relative URIs, but may be left out otherwise.
//!   If left out, and uri is a relative URI, an error is thrown.
//! @param uri
//!   When uri is another URI object, the created
//!   URI will inherit all properties of the supplied uri
//!   except, of course, for its base_uri.
void create(object(this_program)|string uri,
	    object(this_program)|string|void base_uri)
{
  debug_n("Create(%O, %O) called!", ({ uri, base_uri }));
  if(stringp(uri))
    raw_uri = [string]uri; // Keep for future runs of reparse_uri after a base_uri change
  else // if(objectp(uri)) (implied)
  {
    raw_uri = uri->raw_uri;
    inherit_properties([object(this_program)]uri);
  }
  
  reparse_uri(base_uri);
}


//! Assign a new value to a property of URI
//! @param property
//!   When any of the following properties are used, properties that
//!   depend on them are recalculated: user, password, host, port, authority, base_uri.
//! @param value
//!   The value to assign to @[property]
mixed `[]=(string property, mixed value)
{
  debug_n("`[]=(%O, %O)", ({ property, value }));
  switch(property)
  {
    case "user":
    case "password":
    case "host":
    case "port":
      ::`[]=(property, value);
      authority = (user ? user + (password ? ":" + password : "") + "@"
		   : "") +
		  (host || "") +
		  (port != Protocols.Ports.tcp[scheme] ? ":" + port
						       : "");
	return value;

    case "authority":
      authority = [string]value;
      parse_authority(); // Set user, password, host and port accordingly
      return value;

    case "base_uri":
      reparse_uri([object(this_program)|string]value);
      return base_uri;

    default:
      return ::`[]=(property, value); // Set and return the new value
  }
}

//! Works similar to @[`[]=(string property, mixed value)]
mixed `->=(string property, mixed value) { return `[]=(property, value); }


//! When cast to string, return the URI (in a canonicalized form).
//! When cast to mapping, return a mapping with scheme, authority, user, password, host,
//! port, path, query, fragment, raw_uri, base_uri as documented above.
string|mapping cast(string to)
{
  switch(to)
  {
    case "string":
      return _sprintf('s');
    case "mapping":
      array(string) i = ({ "scheme", "authority", "user", "password", "host", "port",
			   "path", "query", "fragment",
			   "raw_uri", "base_uri",  });
      return mkmapping(i, rows(this_object(), i));
  }
}

string _sprintf(int how, mapping|void args)
{
  string look, _scheme = scheme, _host = host, getstring;
  switch(how)
  {
    case 't':
      return "Standards.URI";

    case 'x': // A case-mangling version, especially suited for readable hash values
      if(_host) _host = lower_case(_host);
      if(_scheme) _scheme = lower_case(_scheme);
    case 's':
    case 'O':
      getstring = (path||"") + 
	          (query ? "?" + query : "");
      look = _scheme + ":" + (authority
			      ? "//" + (user
					? user + (password
						  ? ":" + password
						  : "") + "@"
					: "") +
			      (_host || "") +
			      (port != Protocols.Ports.tcp[scheme]
			       ? ":" + port
			       : "")
			      : ("")) +
	getstring + (fragment ? "#" + fragment : "");
      break;
  }

  if(how == 'O')
    return "URI(\"" + look + "\")";
  else
    if(args && args->flag_left)
      return getstring;
    else
      return look;
}

// Not used yet.
string quote(string s)
{
  return replace(s,
		 ({ "\000", "\001", "\002", "\003", "\004", "\005", "\006",
		    "\007", "\010", "\011", "\012", "\013", "\014", "\015",
		    "\016", "\017", "\020", "\021", "\022", "\023", "\024",
		    "\025", "\026", "\027", "\030", "\031", "\032", "\033",
		    "\034", "\035", "\036", "\037", "\200", "\201", "\202",
		    "\203", "\204", "\205", "\206", "\207", "\210", "\211",
		    "\212", "\213", "\214", "\215", "\216", "\217", "\220",
		    "\221", "\222", "\223", "\224", "\225", "\226", "\227",
		    "\230", "\231", "\232", "\233", "\234", "\235", "\236",
		    "\237", " ", "%", "'", "\"" }),
		 ({ "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
		    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
		    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
		    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
		    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
		    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
		    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
		    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
		    "%20", "%25", "%27", "%22"}));
}

