/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

#define COMPAT_BIT   1
#define COMPOSE_BIT  2

struct pike_string *unicode_normalize( struct pike_string *source, int how );
struct buffer *unicode_decompose_buffer( struct buffer *source,	int how );
struct buffer *unicode_compose_buffer( struct buffer *source, int how );
void unicode_normalize_init();
