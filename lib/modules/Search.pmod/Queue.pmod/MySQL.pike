inherit .Base;

Sql.Sql db;
string url, table;

Web.Crawler.Stats stats;
Web.Crawler.Policy policy;
Web.Crawler.RuleSet allow, deny;

inherit Web.Crawler.Queue;

static string to_md5(string url)
{
  object md5 = Crypto.md5();
  md5->update(string_to_utf8(url));
  return Crypto.string_to_hex(md5->digest());
}

void create( Web.Crawler.Stats _stats,
	     Web.Crawler.Policy _policy,
	     
	     string _url, string _table,
	     
	     void|Web.Crawler.RuleSet _allow,
	     void|Web.Crawler.RuleSet _deny)
{
  stats = _stats;   policy = _policy;
  allow=_allow;     deny=_deny;
  table = _table;

  db = Sql.Sql( _url );
  perhaps_create_table(  );
}

static void perhaps_create_table(  )
{
  db->query(
#"
    create table IF NOT EXISTS "+table+#" (
        uri        blob not null,
        uri_md5    char(32) not null default '',
	template   varchar(255) not null default '',
	md5        char(32) not null default '',
	recurse    tinyint not null,
	stage      tinyint not null,
	UNIQUE(uri_md5),
	INDEX stage   (stage)
	)
    ");
}
  
mapping hascache = ([]);
static int has_uri( string|Standards.URI uri )
{
  uri = (string)uri;
  if( sizeof(hascache) > 100000 )  hascache = ([]);
  return hascache[uri]||
    (hascache[uri]=
     sizeof(db->query("select stage from "+table+" where uri_md5=%s",
		      to_md5(uri))));
}

void add_uri( Standards.URI uri, int recurse, string template, void|int force )
{
  // The language is encoded in the fragment.
  Standards.URI r = Standards.URI( (string)uri );
  if( r->query )         r->query = normalize_query( r->query );
  if(r->query && !strlen(r->query))  r->query = 0;

    // Remove any trailing index filename
    
  string rpath=reverse(r->path);
  // FIXME: Make these configurable?
  foreach( ({"index.xml", "index.html", "index.htm"}),
	   string index)
    if(search(rpath,reverse(index))==0)
      rpath=rpath[sizeof(index)..];
  r->path=reverse(rpath);
    
  if( force || check_link(uri, allow, deny) )
  {
    if(has_uri(r))
    {
      int stage = get_stage(r);
      if(stage!=1 && stage!=2 && stage!=3 && stage!=4)
	set_stage(r, 0);
    }
    else
      db->query( "insert into "+table+
		 " (uri,uri_md5,recurse,template) values (%s,%s,%d,%s)",
		 string_to_utf8((string)r),
		 to_md5((string)r), recurse, (template||"") );
  }
}

void set_md5( Standards.URI uri, string md5 )
{
  db->query( "update "+table+
	     " set md5=%s WHERE uri_md5=%s", md5, to_md5((string)uri) );
}

mapping(string:mapping(string:string)) extra_data = ([]);
mapping get_extra( Standards.URI uri )
{
  if( extra_data[(string)uri] )
    return extra_data[(string)uri];
  array r = db->query( "SELECT md5,recurse,stage,template "
		       "FROM "+table+" WHERE uri_md5=%s", to_md5((string)uri) );
  if( sizeof( r ) )
    return r[0];
  
}

static int empty_count;
static int retry_count;
  
// cache, for performance reasons.
static array possible=({});
static int p_c;
  
int|Standards.URI get()
{
  if(stats->concurrent_fetchers() > policy->max_concurrent_fetchers)
  {
    return -1;
  }

  if( sizeof( possible ) <= p_c )
  {
    p_c = 0;
    possible = db->query( "select * from "+table+" where stage=0 limit 20" );
    extra_data = mkmapping( map(possible->uri,utf8_to_string), possible );
    possible = map(possible->uri,utf8_to_string);
  }

  while( sizeof( possible ) > p_c )
  {
    empty_count=0;
    if( possible[ p_c ] )
    {
      Standards.URI ur = Standards.URI( possible[p_c++] );

      if( stats->concurrent_fetchers( ur->host ) >
	  policy->max_concurrent_fetchers_per_host )
      {
	retry_count++;
	continue; // not this host..
      }
      possible[p_c-1] = 0;
      retry_count=0;
      set_stage( ur, 1 );
      return ur;
    }
    p_c++;
    continue;
  }

  if( stats->concurrent_fetchers() )
  {
    return -1;
  }
  // delay for (quite) a while.
  if( empty_count++ > 40 )
  {
    if( num_with_stage( 2 ) || num_with_stage( 3 ) )
    {
      empty_count=0;
      return -1;
    }
    return 0;
  }

  return -1;
}

void put(string|array(string)|Standards.URI|array(Standards.URI) uri)
{
  if(arrayp(uri))
  {
    foreach(uri, string|object _uri)
      put(_uri);
    return;
  }
  if(!objectp(uri))
    uri=Standards.URI(uri);
    
  add_uri( uri, 1, 0 );
}

void done( Standards.URI uri,
	   int called )
{
  if( called )
    set_stage( uri, 2 );
  else
    set_stage( uri, 5 );
}

void clear()
{
  hascache = ([ ]);
  db->query("delete from "+table);
}


void clear_stage( int ... stages )
{
  foreach( stages, int s )
    db->query( "update "+table+" set stage=0 where stage=%d", s );
}

void clear_md5( int ... stages )
{
  foreach( stages, int s )
    db->query( "update "+table+" set md5='' where stage=%d", s );
}

int num_with_stage( int ... stage )
{
  return (int)
    db->query( "select COUNT(*) as c from "+table+" where stage IN (%s)",
	       ((array(string))stage)*"," )[ 0 ]->c;
}

void set_stage( Standards.URI uri,
		int stage )
{
  db->query( "update "+table+" set stage=%d where uri_md5=%s",stage,
	     to_md5((string)uri));
}

int get_stage( Standards.URI uri )
{
  array a = db->query( "select stage from "+table+" where uri_md5=%s", to_md5((string)uri));
  if(sizeof(a))
    return (int)a[0]->stage;
  else
    return -1;
}
