#include "global.h"
#include "stralloc.h"
#include "global.h"
RCSID("$Id: blobs.c,v 1.5 2001/07/04 22:02:26 per Exp $");
#include "pike_macros.h"
#include "interpret.h"
#include "program.h"
#include "program_id.h"
#include "object.h"
#include "array.h"
#include "operators.h"
#include "module_support.h"

#include "config.h"

#include "whitefish.h"
#include "resultset.h"
#include "blob.h"
#include "buffer.h"
#include "blobs.h"

/* must be included last */
#include "module_magic.h"
static void exit_blobs_struct( );

#define HSIZE 4711
#define THIS ((struct blobs *)Pike_fp->current_storage)
#define HASH(X) (((long)(X)>>3) % HSIZE)

extern struct program *blob_program;

struct hash
{
  struct object *bl;
  struct hash *next;
  struct pike_string *id;
};

struct blobs
{
  int next_ind;
  struct hash *next_h;
  struct hash *hash[HSIZE];
};


static struct hash *new_hash( struct pike_string *id )
{
  struct hash *res =  malloc( sizeof( struct hash ) );
  res->id = id;
  id->refs++;
  res->next = 0;
  res->bl = clone_object( blob_program,0 );
  return res;
}

static void insert_hash( struct blobs *d, struct hash *h )
{
  int r = HASH(h->id);
  h->next = d->hash[ r ];
  d->hash[ r ] = h;
}

static void free_hash( struct hash *h )
{
  while( h )
  {
    struct hash *n = h->next;
    if( h->bl ) free_object( h->bl );
    if( h->id ) free_string( h->id );
    free( h );
    h = n;
  }
}

static struct hash *find_hash( struct blobs *d, struct pike_string *id )
{
  int r = HASH(id);
  struct hash *h = d->hash[ r ];
  while( h )
  {
    if( h->id == id )
      return h;
    h = h->next;
  }
  h = new_hash( id );
  insert_hash( d, h );
  return h;
}

static void f_blobs_add_words( INT32 args )
/*
 *! @decl void add_words( int docid, array(string) words, int field_id,
 *!                       int link_hash )
 *!
 *! Add all the words in the 'words' array to the blobs
 */ 
{
  INT_TYPE docid;
  struct array *words;
  INT_TYPE field_id;
  INT_TYPE link_hash;

  int i;
  struct blobs *blbl = THIS;
  
  get_all_args( "add_words", args, "%d%a%d%d",
		&docid, &words, &field_id, &link_hash );

  for( i = 0; i<words->size; i++ )
    if( words->item[i].type != PIKE_T_STRING )
      Pike_error("Illegal element %d in words array\n", i );
    else
      wf_blob_low_add( find_hash( blbl, words->item[i].u.string )->bl,
		       docid, field_id, link_hash, i );

  pop_n_elems( args );
  push_int(0);
}

static void f_blobs_memsize( INT32 args )
/*
 *! @decl int memsize()
 *!
 *! Returns the in-memory size of the blobs
 */
{
  int size = HSIZE*sizeof(void *); /* htable.. */
  int i;
  struct hash *h;
  struct blobs *bl = THIS;
  
  for( i = 0; i<HSIZE; i++ )
  {
    h = bl->hash[i];
    while( h )
    {
      if( h->id )
	size += (wf_blob_low_memsize( h->bl )        /* blob */
		 + ((sizeof( struct hash )+7)/8)*8+4 /* the container (aprox)*/
		 + (h->id->len<<h->id->size_shift)    /* and the string */
		 + ((int)((struct pike_string *)0)->str));
      else
	size += ((sizeof( struct hash )+7)/8)*8+4;
      h = h->next;
    }
  }
  pop_n_elems( args );
  push_int( size );
}


static void f_blobs_read( INT32 args )
/*
 *! @decl array read();
 *!
 *! returns ({ string word_id, @[Blob] b }) or ({0,0}) As a side-effect,
 *! this function frees the blob and the word_id, so you can only read
 *! the blobs struct once. Also, once you have called @[read],
 *! @[add_words] will no longer work as expected.
 */
{
  struct blobs *t = THIS;
  struct array *a = allocate_array( 2 );
  while( !t->next_h )
  {
    if( t->next_ind >= HSIZE )
    {
      pop_n_elems( args );
      a->item[0].type = PIKE_T_INT;
      a->item[0].u.integer = 0;
      a->item[1].type = PIKE_T_INT;
      a->item[1].u.integer = 0;
      push_array( a );
      return;
    }
    t->next_h = t->hash[ t->next_ind ];
    t->next_ind++;
  }

  pop_n_elems( args );
  a->item[0].type = PIKE_T_STRING;
  a->item[0].u.string = t->next_h->id;
  a->item[1].type = PIKE_T_OBJECT;
  a->item[1].u.object = t->next_h->bl;

  t->next_h->id = 0;
  t->next_h->bl = 0;

  push_array( a );

  t->next_h = THIS->next_h->next;
}

static void init_blobs_struct( )
{
  MEMSET( THIS, 0, sizeof( struct blobs ) );
}

static void exit_blobs_struct( )
{
  int i;
  for( i = 0; i<HSIZE; i++ )
    if( THIS->hash[i] )
      free_hash( THIS->hash[i] );
  init_blobs_struct();
}

static struct program *blobs_program;
void init_blobs_program()
{
  start_new_program();
  ADD_STORAGE( struct blobs );
  add_function("add_words",f_blobs_add_words,
	       "function(int,array,int,int:void)",0 );
  add_function("memsize", f_blobs_memsize, "function(void:int)", 0 );
  add_function("read", f_blobs_read, "function(void:array(string|object))", 0);
  set_init_callback( init_blobs_struct );
  set_exit_callback( exit_blobs_struct );
  blobs_program = end_program( );
  add_program_constant( "Blobs", blobs_program, 0 );
}

void exit_blobs_program()
{
  free_program( blobs_program );
}
