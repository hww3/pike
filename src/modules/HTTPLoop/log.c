#include "config.h"
#include "global.h"
	  
#include "array.h"
#include "backend.h"
#include "machine.h"
#include "mapping.h"
#include "module_support.h"
#include "multiset.h"
#include "object.h"
#include "operators.h"
#include "pike_memory.h"
#include "program.h"
#include "stralloc.h"
#include "svalue.h"
#include "threads.h"
#include "fdlib.h"
#include "builtin_functions.h"

#ifdef _REENTRANT
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "accept_and_parse.h"
#include "log.h"
#include "requestobject.h"
#include "util.h"

struct program *aap_log_object_program;
struct log *aap_first_log;

static void push_log_entry(struct log_entry *le)
{
  struct object *o = clone_object( aap_log_object_program, 0 );
  struct log_object *lo = (struct log_object*)o->storage;
  lo->time = le->t;
  lo->sent_bytes = le->sent_bytes;
  lo->reply = le->reply;
  lo->received_bytes = le->received_bytes;
  lo->raw = make_shared_binary_string(le->raw.str, le->raw.len);
  lo->url = make_shared_binary_string(le->url.str, le->url.len);
  lo->method = make_shared_binary_string(le->method.str, le->method.len);
  lo->protocol = le->protocol;
  le->protocol->refs++;
  lo->from = make_shared_string( inet_ntoa(le->from.sin_addr) );
  push_object( o );
}

void f_aap_log_as_array(INT32 args)
{
  struct log_entry *le;
  struct log *l = LTHIS->log;
  int n = 0;
  pop_n_elems(args);
  
  mt_lock( &l->log_lock );
  le = l->log_head;
  l->log_head = l->log_tail = 0;
  mt_unlock( &l->log_lock );
  
  while(le)
  {
    struct log_entry *l;
    n++;
    push_log_entry(le);
    l = le->next;
    free(le);
    le = l;
  }
  {
    f_aggregate(n);
  }
}

void f_aap_log_exists(INT32 args)
{
  if(LTHIS->log->log_head) 
    push_int(1);
  else 
    push_int(0);
}

void f_aap_log_size(INT32 args)
{
  int n=1;
  struct log *l = LTHIS->log;
  struct log_entry *le;
  if(!l) {
    push_int(0);
    return;
  }
  mt_lock( &l->log_lock );
  le = l->log_head; 
  while((le = le->next))
    n++;
  mt_unlock( &l->log_lock );
  push_int(n);
}

void f_aap_log_as_commonlog_to_file(INT32 args)
{
  struct log_entry *le;
  struct log *l = LTHIS->log;
  int n = 0;
  int mfd, ot=0;
  struct object *f;
  struct tm tm;
  FILE *foo;
  static char *month[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Oct", "Sep", "Nov", "Dec",
  };

  get_all_args("log_as_commonlog_to_file", args, "%o", &f);
  f->refs++;

  pop_n_elems(args);
  apply(f, "query_fd", 0);
  mfd = dup(sp[-1].u.integer);
  if(mfd < 1)error("Bad fileobject to ->log_as_commonlog_to_file\n");
  pop_stack();

  foo = fdopen( mfd, "w" );
  if(!foo)
    error("Bad fileobject to ->log_as_commonlog_to_file\n");

  THREADS_ALLOW();

  mt_lock( &l->log_lock );
  le = l->log_head; 
  l->log_head = l->log_tail = 0;
  mt_unlock( &l->log_lock );

  while(le)
  {
    int i;
    struct log_entry *l = le->next;
    /* remotehost rfc931 authuser [date] "request" status bytes */
    if(le->t != ot)
    {
      time_t t = (time_t)le->t;
#ifdef HAVE_GMTIME_R
      gmtime_r( &t, &tm );
#else
#ifdef HAVE_GMTIME
      tm = *gmtime( &t ); /* This will break if two threads run
			    gmtime() at once. */
#else
#ifdef HAVE_LOCALTIME
      tm = *localtime( &t ); /* This will break if two threads run
			       localtime() at once. */
#endif
#endif
#endif
      ot = le->t;
    }

    /* date format:  [03/Feb/1998:23:08:20 +0000]  */

    /* GET [URL] HTTP/1.0 */
    for(i=13; i<le->raw.len; i++)
      if(le->raw.str[i] == '\r')
      {
	le->raw.str[i] = 0;
	break;
      }

    fprintf(foo, 
    "%d.%d.%d.%d - %s [%02d/%s/%d:%02d:%02d:%02d +0000] \"%s\" %d %d\n",
	    ((unsigned char *)&le->from.sin_addr)[ 0 ],
	    ((unsigned char *)&le->from.sin_addr)[ 1 ],
	    ((unsigned char *)&le->from.sin_addr)[ 2 ],
	    ((unsigned char *)&le->from.sin_addr)[ 3 ], /* hostname */
	    "-",                          /* remote-user */
	    tm.tm_mday, month[tm.tm_mon], tm.tm_year+1900,
	    tm.tm_hour, tm.tm_min, tm.tm_sec, /* date */
	    le->raw.str, /* request line */
	    le->reply, /* reply code */
	    le->sent_bytes); /* bytes transfered */
    free(le);
    n++;
    le = l;
  }
  fclose(foo);
  close(mfd);
  THREADS_DISALLOW();
  push_int(n);
}

void aap_log_append(int sent, struct args *arg, int reply)
{
  struct log *l = arg->log;
  /* we do not incude the body, only the headers et al.. */
  struct log_entry *le=malloc(sizeof(struct log_entry)+arg->res.body_start-3);
  char *data_to=((char *)le)+sizeof(struct log_entry);

fprintf(stderr, "log_append()\n");
  
  le->t = aap_get_time();
  le->sent_bytes = sent;
  le->reply = reply;
  le->received_bytes = arg->res.body_start + arg->res.content_len;
  MEMCPY(data_to, arg->res.data, arg->res.body_start-4);
  le->raw.str = data_to;
  le->raw.len = arg->res.body_start-4;
  le->url.str = (data_to + (int)(arg->res.url-arg->res.data));
  le->url.len = arg->res.url_len;
  le->from = arg->from;
  le->method.str = data_to;
  le->method.len = arg->res.method_len;
  le->protocol = arg->res.protocol;
  le->next = 0;

  mt_lock( &l->log_lock );
  if(l->log_head)
  {
    l->log_tail->next = le;
    l->log_tail = le;
  }
  else
  {
    l->log_head = le;
    l->log_tail = le;
  }
  mt_unlock( &l->log_lock );
}

#endif
