#define NO_PIKE_SHORTHAND

#include "global.h"
#include "config.h"


#include "machine.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "fdlib.h"
#include "stralloc.h"
#include "pike_macros.h"
#include "machine.h"
#include "object.h"
#include "constants.h"
#include "interpret.h"
#include "svalue.h"
#include "mapping.h"
#include "array.h"
#include "builtin_functions.h"
#include "module_support.h"
#include "backend.h"
#include "threads.h"
#include "operators.h"


/**** CLASS HeaderParser */

#define THP ((struct header_buf *)Pike_fp->current_object->storage)
struct  header_buf
{
  char headers[8192];
  char *pnt;
  ptrdiff_t left;
};

static void f_hp_feed( INT32 args )
{
  struct pike_string *str = Pike_sp[-1].u.string;
  int slash_n = 0, cnt, num;
  char *pp,*ep;
  struct svalue *tmp;
  struct mapping *headers;
  ptrdiff_t os=0, i, j, l;
  unsigned char *in;

  if( Pike_sp[-1].type != PIKE_T_STRING )
    error("Wrong type of argument to feed()\n");

  if( str->len >= THP->left )
    error("Too many headers\n");

  MEMCPY( THP->pnt, str->str, str->len );

  for( ep=(THP->pnt+str->len),
         pp=MAXIMUM(THP->headers,THP->pnt-3); 
       pp<ep && slash_n<2; 
       pp++ )
    if( *pp == '\n' )
      slash_n++;
    else if( *pp != '\r' )
      slash_n=0;
  
  THP->left -= str->len;
  THP->pnt += str->len;
  THP->pnt[0] = 0;
  pop_n_elems( args );
  if( slash_n != 2 )
  {
    /* check for HTTP/0.9? */
    push_int( 0 );
    return;
  }

  push_string( make_shared_binary_string( pp, THP->pnt - pp ) ); /*leftovers*/
  headers = allocate_mapping( 5 );
  in = THP->headers;
  l = pp - THP->headers;

  /* find first line here */
  for( i = 0; i < l; i++ )
    if( in[i] == '\n' )
      break;

  if( in[i-1] != '\r' ) 
    i++;

  push_string( make_shared_binary_string( in, i-1 ) );
  in += i; l -= i;
  if( *in == '\n' ) (in++),(l--);

  for(i = 0; i < l; i++)
  {
    if(in[i] > 64 && in[i] < 91) in[i]+=32;
    else if( in[i] == ':' )
    {
      /* in[os..i-1] == the header */
      push_string(make_shared_binary_string((char*)in+os,i-os));
      os = i+1;
      while(in[os]==' ') os++;
      for(j=os;j<l;j++) 
        if( in[j] == '\n' || in[j]=='\r')
        { 
          break; 
        }
      push_string(make_shared_binary_string((char*)in+os,j-os));

      if((tmp = low_mapping_lookup(headers, Pike_sp-2)))
      {
        f_aggregate( 1 );
        if( tmp->type == PIKE_T_ARRAY )
        {
          tmp->u.array->refs++;
          push_array(tmp->u.array);
          map_delete(headers, Pike_sp-3);
          f_add(2);
        } else {
          tmp->u.string->refs++;
          push_string(tmp->u.string);
          f_aggregate(1);
          map_delete(headers, Pike_sp-3);
          f_add(2);
        }
      }
      mapping_insert(headers, Pike_sp-2, Pike_sp-1);
      pop_n_elems(2);
      if( in[j+1] == '\n' ) j++;
      os = j+1;
      i = j;
    }
  }
  push_mapping( headers );
  f_aggregate( 3 );             /* data, headers */
}

static void f_hp_create( INT32 args )
{
  THP->pnt = THP->headers;
  THP->left = 8192;
}
/**** END CLASS HeaderParser */

static void f_make_http_headers( INT32 args )
{
  int total_len = 0, e;
  char *pnt;
  struct mapping *m;
  struct keypair *k;
  struct pike_string *res;
  if( Pike_sp[-1].type != PIKE_T_MAPPING )
    error("Wrong argument type to make_http_headers(mapping heads)\n");

  m = Pike_sp[-1].u.mapping;
  /* loop to check len */
  NEW_MAPPING_LOOP( m->data )
  {
    if( k->ind.type != PIKE_T_STRING || k->ind.u.string->size_shift )
      error("Wrong argument type to make_http_headers("
            "mapping(string(8bit):string(8bit)|array(string(8bit))) heads)\n");
    if( k->val.type == PIKE_T_STRING )
      total_len +=  k->val.u.string->len + 2 + k->ind.u.string->len + 2;
    else if( k->val.type == PIKE_T_ARRAY )
    {
      struct array *a = k->val.u.array;
      ptrdiff_t i, kl = k->ind.u.string->len + 2 ;
      for( i = 0; i<a->size; i++ )
        if( a->item[i].type != PIKE_T_STRING||a->item[i].u.string->size_shift )
          error("Wrong argument type to make_http_headers("
                "mapping(string(8bit):string(8bit)|"
                "array(string(8bit))) heads)\n");
        else
          total_len += kl + a->item[i].u.string->len + 2;
    } else
      error("Wrong argument type to make_http_headers("
            "mapping(string(8bit):string(8bit)|"
            "array(string(8bit))) heads)\n");
  }
  total_len += 2;

  res = begin_shared_string( total_len );
  pnt = (char *)res->str;
#define STRADD(X)\
    for( l=X.u.string->len,s=X.u.string->str,c=0; c<l; c++ )\
      *(pnt++)=*(s++);

  NEW_MAPPING_LOOP( m->data )
  {
    char *s;
    ptrdiff_t l, c;
    if( k->val.type == PIKE_T_STRING )
    {
      STRADD( k->ind ); *(pnt++) = ':'; *(pnt++) = ' ';
      STRADD( k->val ); *(pnt++) = '\r'; *(pnt++) = '\n';
    }
    else
    {
      struct array *a = k->val.u.array;
      ptrdiff_t i, kl = k->ind.u.string->len + 2;
      for( i = 0; i<a->size; i++ )
      {
        STRADD( k->ind );    *(pnt++) = ':'; *(pnt++) = ' ';
        STRADD( a->item[i] );*(pnt++) = '\r';*(pnt++) = '\n';
      }
    }
  }
  *(pnt++) = '\r';
  *(pnt++) = '\n';

  pop_n_elems( args );
  push_string( end_shared_string( res ) );
}

static void f_http_decode_string(INT32 args)
{
   int proc;
   char *foo,*bar,*end;
   struct pike_string *newstr;

   if (!args || Pike_sp[-args].type != PIKE_T_STRING)
     error("Invalid argument to http_decode_string(STRING);\n");

   foo=bar=Pike_sp[-args].u.string->str;
   end=foo+Pike_sp[-args].u.string->len;

   /* count '%' characters */
   for (proc=0; foo<end; ) if (*foo=='%') { proc++; foo+=3; } else foo++;

   if (!proc) { pop_n_elems(args-1); return; }

   /* new string len is (foo-bar)-proc*2 */
   newstr=begin_shared_string((foo-bar)-proc*2);
   foo=newstr->str;
   for (proc=0; bar<end; foo++)
      if (*bar=='%')
      {
        if (bar<end-2)
          *foo=(((bar[1]<'A')?(bar[1]&15):((bar[1]+9)&15))<<4)|
            ((bar[2]<'A')?(bar[2]&15):((bar[2]+9)&15));
        else
          *foo=0;
        bar+=3;
      }
      else { *foo=*(bar++); }
   pop_n_elems(args);
   push_string(end_shared_string(newstr));
}


void pike_module_init()
{
  pike_add_function("make_http_headers", f_make_http_headers,
               "function(mapping(string:string|array(string)):string)", 0 );

  pike_add_function("http_decode_string", f_http_decode_string,
               "function(string:string)", 0 );

  start_new_program();
  ADD_STORAGE( struct header_buf  );
  pike_add_function( "feed", f_hp_feed, "function(string:array(string|mapping))",0 );
  pike_add_function( "create", f_hp_create, "function(void:void)", 0 );
  end_class( "HeaderParser", 0 );
}

void pike_module_exit()
{
}
