/*\
||| This file is part of Pike. For copyright information see COPYRIGHT.
||| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
||| for more information.
||| $Id: split.h,v 1.2 2002/10/08 20:22:45 nilsson Exp $
\*/

struct words
{
  unsigned int size;
  unsigned int allocated_size;
  struct word
  {
    unsigned int start;
    unsigned int size;
  } words[1];
};

void uc_words_free( struct words *w );
struct words *unicode_split_words_buffer( struct buffer *data );
int unicode_is_wordchar( int c );
