/*
 * $Id: idea.c,v 1.14 2000/07/28 07:15:16 hubbe Exp $
 *
 * IDEA crypto module for Pike
 *
 * /precompiled/crypto/idea
 *
 * Henrik Grubbstr�m 1996-11-03, Niels M�ller 1996-11-21
 */

/*
 * Includes
 */

/* From the Pike distribution */
#include "global.h"
#include "stralloc.h"
#include "interpret.h"
#include "svalue.h"
#include "object.h"
#include "error.h"
#include "las.h"

/* Backend includes */
#include <idea.h>

/* THIS MUST BE INCLUDED LAST */
#include "module_magic.h"

#undef THIS
#define THIS ((unsigned INT16 *)(fp->current_storage))
#define OBTOCTX(o) ((unsigned INT16 *)(o->storage))

/*
 * Globals
 */

struct program *pike_crypto_idea_program;

/*
 * Functions
 */

void init_pike_crypto_idea(struct object *o)
{
  MEMSET(THIS, 0, sizeof(INT16[IDEA_KEYLEN]));
}

void exit_pike_crypto_idea(struct object *o)
{
  MEMSET(THIS, 0, sizeof(INT16[IDEA_KEYLEN]));
}

/*
 * efuns and the like
 */

/* string name(void) */
static void f_name(INT32 args)
{
  if (args) {
    error("Too many arguments to idea->name()\n");
  }
  push_string(make_shared_string("IDEA"));
}

/* int query_block_size(void) */
static void f_query_block_size(INT32 args)
{
  if (args) {
    error("Too many arguments to idea->query_block_size()\n");
  }
  push_int(IDEA_BLOCKSIZE);
}

/* int query_key_length(void) */
static void f_query_key_length(INT32 args)
{
  if (args) {
    error("Too many arguments to idea->query_key_length()\n");
  }
  push_int(IDEA_KEYSIZE);
}

/* void set_encrypt_key(string) */
static void f_set_encrypt_key(INT32 args)
{
  if (args != 1) {
    error("Wrong number of args to idea->set_encrypt_key()\n");
  }
  if (sp[-1].type != T_STRING) {
    error("Bad argument 1 to idea->set_encrypt_key()\n");
  }
  if (sp[-1].u.string->len != IDEA_KEYSIZE) {
    error("idea->set_encrypt_key(): Invalid key length\n");
  }
  idea_expand(THIS, (unsigned char *)sp[-1].u.string->str);
  
  pop_n_elems(args);
  push_object(this_object());
}

/* void set_decrypt_key(string) */
static void f_set_decrypt_key(INT32 args)
{
  f_set_encrypt_key(args);
  idea_invert(THIS, THIS);
}

/* string crypt_block(string) */
static void f_crypt_block(INT32 args)
{
  int len;
  struct pike_string *s;
  INT32 i;
  
  if (args != 1) {
    error("Wrong number of arguemnts to idea->crypt()\n");
  }
  if (sp[-1].type != T_STRING) {
    error("Bad argument 1 to idea->crypt()\n");
  }

  len = sp[-1].u.string->len;
  if (len % IDEA_BLOCKSIZE) {
    error("Bad length of argument 1 to idea->crypt()\n");
  }

  s = begin_shared_string(len);
  for(i = 0; i < len; i += IDEA_BLOCKSIZE)
    idea_crypt(THIS,
	       (unsigned INT8 *) s->str + i,
	       (unsigned INT8 *) sp[-1].u.string->str + i);
  
  pop_n_elems(args);

  push_string(end_shared_string(s));
}

/*
 * Module linkage
 */

void pike_idea_init(void)
{
  /*
   * start_new_program();
   *
   * add_storage();
   *
   * add_function();
   * add_function();
   * ...
   *
   * set_init_callback();
   * set_exit_callback();
   *
   * program = end_c_program();
   * program->refs++;
   *
   */

  start_new_program();
  low_add_storage(sizeof(INT16[IDEA_KEYLEN]),ALIGNOF(INT16),0);

  /* function(void:string) */
  ADD_FUNCTION("name", f_name,tFunc(tVoid,tStr), 0);
  /* function(void:int) */
  ADD_FUNCTION("query_block_size", f_query_block_size,tFunc(tVoid,tInt), 0);
  /* function(void:int) */
  ADD_FUNCTION("query_key_length", f_query_key_length,tFunc(tVoid,tInt), 0);
  /* function(string:object) */
  ADD_FUNCTION("set_encrypt_key", f_set_encrypt_key,tFunc(tStr,tObj), 0);
  /* function(string:object) */
  ADD_FUNCTION("set_decrypt_key", f_set_decrypt_key,tFunc(tStr,tObj), 0);
  /* function(string:string) */
  ADD_FUNCTION("crypt_block", f_crypt_block,tFunc(tStr,tStr), 0);

  set_init_callback(init_pike_crypto_idea);
  set_exit_callback(exit_pike_crypto_idea);

  end_class("idea", 0);
}

void pike_idea_exit(void)
{
}
