import ".";

constant MAXIMUM_REQUEST_SIZE=1000000;

Stdio.File my_fd;
Port server_port;
HeaderParser headerparser;

string buf="";    // content buffer

string request_raw;
string request_type;
string full_query;
string not_query;
string query;
string protocol;

mapping(string:string|array(string)) headers=([]);

mapping(string:string|array(string)) variables=([]);

function(this_program:void) request_callback;

void attach_fd(Stdio.File _fd,Port server,
	       function(this_program:void) _request_callback)
{
   my_fd=_fd;
   server_port=server;
   headerparser=HeaderParser();
   request_callback=_request_callback;

   my_fd->set_nonblocking(read_cb,0,close_cb);
}

static void read_cb(mixed dummy,string s)
{
   array v=headerparser->feed(s);
   if (v)
   {
      destruct(headerparser);
      headerparser=0;

      buf=v[0];
      headers=v[2];
      request_raw=v[1];
      parse_request();

      if (parse_variables())
      {
	 my_fd->set_blocking();
	 request_callback(this_object());
      }
   }
}

static void parse_request()
{
   array v=request_raw/" ";
   switch (sizeof(v))
   {
      case 0:
	 request_type="GET";
	 protocol="HTTP/0.9";
	 full_query="";
	 break;
      case 1:
	 request_type="GET";
	 protocol="HTTP/0.9";
	 full_query=v[0];
	 break;
      default:
	 if (v[-1][..3]=="HTTP")
	 {
	    request_type=v[0];
	    protocol=v[-1];
	    full_query=v[1..sizeof(v)-2]*" ";
	    break;
	 }
      case 2:
	 request_type=v[0];
	 protocol="HTTP/0.9";
	 full_query=v[1..]*" ";
	 break;
   }
   query="";
   sscanf(not_query=full_query,"%s?%s",not_query,query);
}

static int parse_variables()
{
   if (query!="")
      http_decode_urlencoded_query(query,variables);

   if (request_type=="POST" &&
       headers["content-type"]=="application/x-www-form-urlencoded")
   {
      if ((int)headers["content-length"]<=strlen(buf))
      {
	 parse_post();
	 return 1;
      }
	  
      my_fd->set_read_callback(read_cb_post);
      return 0; // delay
   }
   return 1;
}

static void parse_post()
{
   int n=(int)headers["content-length"];
   string s=buf[..n-1];
   buf=buf[n..];

   http_decode_urlencoded_query(s,variables);
}

static void read_cb_post(mixed dummy,string s)
{
   buf+=strlen(s);
   if (strlen(buf)<=(int)headers["content-length"] ||
       strlen(buf)>MAXIMUM_REQUEST_SIZE)
   {
      my_fd->set_blocking();
      parse_post();
      request_callback(this_object());
   }
}

static void close_cb()
{
// closed by peer before request read
}

string _sprintf(int t)
{
   switch (t)
   {
      case 'O':
	 return sprintf("HTTP.Server.Request(%s %s)",request_type,full_query);
      default:
	 return 0;
   }
}

// ----------------------------------------------------------------

string make_response_header(mapping m)
{
   array(string) res=({});

   switch (m->error)
   {
      case 0:
      case 200: 
	 res+=({"HTTP/1.0 200 OK"}); // HTTP/1.1 when supported
	 break;
      default:
   // better error names?
	 res+=({"HTTP/1.0 "+m->error+" ERROR"});
	 break;
   }

   if (!m->type)
      m->type=filename_to_type(not_query);

   res+=({"Content-Type: "+m->type});
   
   res+=({"Server: "+(m->server||http_serverid)});

   string http_now=http_date(time());
   res+=({"Date: "+http_now});

   if (!m->stat && m->file)
      m->stat=m->file->stat();

   if (m->modified)
      res+=({"Last-Modified: "+http_date(m->modified)});
   else if (m->stat)
      res+=({"Last-Modified: "+http_date(m->stat->mtime)});
   else
      res+=({"Last-Modified: "+http_now});

// FIXME: insert extra headers and cookies here

   if (zero_type(m->size))
      if (m->data)
	 m->size=m->data;
      else if (m->stat)
	 m->size=m->stat->size;
      else 
	 m->size=-1;

   if (m->size!=-1)
      res+=({"Content-Length: "+m->size});

   return res*"\r\n"+"\r\n\r\n";
}

void response_and_finish(mapping m)
{
// insert HTTP 1.1 stuff here

   string header=make_response_header(m);

   if (m->file && 
       m->size!=-1 && 
       m->size+strlen(header)<4096) // fit in buffer
   {
      m->data=m->file->read(m->size);
      m->file->close();
      m->file=0;
   }

   if (request_type=="HEAD")
   {
      send_buf=header;
      if (m->file) m->file->close(),m->file=0;
   }
   else if (m->data)
      send_buf=header+m->data;
   else
      send_buf=header;

   if (sizeof(send_buf)<4096 &&
       !m->file)
   {
      my_fd->write(send_buf);
      send_buf="";

      finish();
      return;
   }

   send_pos=0;
   my_fd->set_nonblocking(0,send_write,send_close);

   m->file->read(1);

   if (m->file)
   {
      send_fd=m->file;
      send_buf+=send_fd->read(8192); // start read-ahead if possible
   }
   else
      send_fd=0;
}

void finish()
{
   if (my_fd) my_fd->close();
}

string send_buf="";
int send_pos;
Stdio.File send_fd=0;

void send_write()
{
   if (strlen(send_buf)-send_pos<8192 &&
       send_fd)
   {
      string q=send_fd->read(131072);
      if (!q || q=="")
      {
	 send_fd->close();
	 send_fd=0;
      }
      else
      {
	 send_buf=send_buf[send_pos..]+q;
	 send_pos=0;
      }
   }
   else if (send_pos==strlen(send_buf) && !send_fd)
   {
      finish();
      return;
   }
   
   int n=my_fd->write(send_buf[send_pos..]);
   send_pos+=n;
}

void send_close()
{
/* socket closed by peer */
   finish();
}
