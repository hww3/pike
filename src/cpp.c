/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/

/*
 * $Id: cpp.c,v 1.89 2001/06/14 19:28:08 grubba Exp $
 */
#include "global.h"
#include "stralloc.h"
#include "module_support.h"
#include "interpret.h"
#include "svalue.h"
#include "pike_macros.h"
#include "hashtable.h"
#include "program.h"
#include "object.h"
#include "pike_error.h"
#include "array.h"
#include "mapping.h"
#include "builtin_functions.h"
#include "operators.h"
#include "opcodes.h"
#include "constants.h"
#include "time.h"
#include "stuff.h"
#include "version.h"

#include <ctype.h>

#define CPP_NO_OUTPUT 1
#define CPP_EXPECT_ELSE 2
#define CPP_EXPECT_ENDIF 4
#define CPP_REALLY_NO_OUTPUT 8
#define CPP_END_AT_NEWLINE 16
#define CPP_DO_IF 32
#define CPP_NO_EXPAND 64

#define OUTP() (!(flags & (CPP_NO_OUTPUT | CPP_REALLY_NO_OUTPUT)))
#define PUTNL() string_builder_putchar(&this->buf, '\n')
#define GOBBLE(X) (data[pos]==(X)?++pos,1:0)
#define PUTC(C) do { \
 int c_=(C); if(OUTP() || c_=='\n') string_builder_putchar(&this->buf, c_); }while(0)

#define MAX_ARGS            255
#define DEF_ARG_STRINGIFY   0x100000
#define DEF_ARG_NOPRESPACE  0x200000
#define DEF_ARG_NOPOSTSPACE 0x400000
#define DEF_ARG_MASK        0x0fffff


struct pike_predef_s
{
  struct pike_predef_s *next;
  char *name;
  char *value;
};

static struct pike_predef_s *pike_predefs=0;
struct define_part
{
  int argument;
  struct pike_string *postfix;
};

struct define_argument {
  PCHARP arg;
  ptrdiff_t len;
};


struct cpp;
struct define;
typedef void (*magic_define_fun)(struct cpp *,
				 struct define *,
				 struct define_argument *,
				 struct string_builder *);


struct define
{
  struct hash_entry link; /* must be first */
  magic_define_fun magic;
  int args;
  ptrdiff_t num_parts;
  short inside;		/* 1 - Don't expand. 2 - In use. */
  short varargs;
  struct pike_string *first;
  struct define_part parts[1];
};

#define find_define(N) \
  (this->defines?BASEOF(hash_lookup(this->defines, N), define, link):0)

struct cpp
{
  struct hash_table *defines;
  INT32 current_line;
  INT32 compile_errors;
  struct pike_string *current_file;
  struct string_builder buf;
  struct object *handler;
  struct object *compat_handler;
  int compat_major;
  int compat_minor;
};

struct define *defined_macro =0;
struct define *constant_macro =0;

void cpp_error(struct cpp *this,char *err)
{
  this->compile_errors++;
  if(this->compile_errors > 10) return;
  if((this->handler && this->handler->prog) || get_master())
  {
    ref_push_string(this->current_file);
    push_int(this->current_line);
    push_text(err);
    safe_apply_handler("compile_error", this->handler,
		       this->compat_handler, 3);
    pop_stack();
  }else{
    (void)fprintf(stderr, "%s:%ld: %s\n",
		  this->current_file->str,
		  (long)this->current_line,
		  err);
    fflush(stderr);
  }
}

void cpp_change_compat(struct cpp *this, int major, int minor)
{
  if(this->compat_major == major &&
     this->compat_minor == minor) return;

  if(this->compat_handler)
  {
    free_object(this->compat_handler);
    this->compat_handler=0;
  }
  if((major == PIKE_MAJOR_VERSION &&
      minor == PIKE_MINOR_VERSION) || major < 0)
  {
    this->compat_major=PIKE_MAJOR_VERSION;
    this->compat_minor=PIKE_MINOR_VERSION;
    return; /* Our work here is done */
  }

  push_int(major);
  push_int(minor);
  SAFE_APPLY_MASTER("get_compilation_handler",2);
  if(sp[-1].type == T_OBJECT)
  {
    this->compat_handler=sp[-1].u.object;
    sp--;
  }
  this->compat_major=major;
  this->compat_minor=minor;
}

/* devours one reference to 'name'! */
static struct define *alloc_empty_define(struct pike_string *name,
					 ptrdiff_t parts)
{
  struct define *def;
  def=(struct define *)xalloc(sizeof(struct define)+
			      sizeof(struct define_part) * (parts -1));
  def->magic=0;
  def->args=-1;
  def->inside=0;
  def->varargs=0;
  def->num_parts=parts;
  def->first=0;
  def->link.s=name;
  debug_malloc_touch(name);
  return def;
}

static void undefine(struct cpp *this,
		     struct pike_string *name)
{
  INT32 e;
  struct define *d;

  d=find_define(name);

  if(!d) return;

  if (d->inside) {
    cpp_error(this, "Illegal to undefine a macro during its expansion.");
    return;
  }

  this->defines=hash_unlink(this->defines, & d->link);

  for(e=0;e<d->num_parts;e++)
    free_string(d->parts[e].postfix);
  free_string(d->link.s);
  if(d->first)
    free_string(d->first);
  free((char *)d);
}

static void do_magic_define(struct cpp *this,
			    char *name,
			    magic_define_fun fun)
{
  struct define* def=alloc_empty_define(make_shared_string(name),0);
  def->magic=fun;
  this->defines=hash_insert(this->defines, & def->link);
}

static void simple_add_define(struct cpp *this,
			    char *name,
			    char *what)
{
  struct define* def=alloc_empty_define(make_shared_string(name),0);
  def->first=make_shared_string(what);
  this->defines=hash_insert(this->defines, & def->link);
}


/* Who needs inline functions anyway? /Hubbe */

#define FIND_END_OF_STRING() do {					\
  while(1)								\
  {									\
    if(pos>=len)							\
    {									\
      cpp_error(this,"End of file in string.");				\
      break;								\
    }									\
    switch(data[pos++])							\
    {									\
    case '\n':								\
      cpp_error(this,"Newline in string.");				\
      this->current_line++;						\
      break;								\
    case '"': break;							\
    case '\\': if(data[++pos]=='\n') this->current_line++;		\
    default: continue;							\
    }									\
   break;								\
  } } while(0)

#define FIND_END_OF_STRING2() do {					\
  while(1)								\
  {									\
    if(pos>=len)							\
    {									\
      cpp_error(this,"End of file in string.");				\
      break;								\
    }									\
    switch(data[pos++])							\
    {									\
    case '\n':								\
      this->current_line++;						\
      continue;								\
    case '"': break;							\
    case '\\': if(data[++pos]=='\n') this->current_line++;		\
    default: continue;							\
    }									\
   break;								\
  } } while(0)

#define FIND_END_OF_CHAR() do {					\
  int e=0;							\
  while(1)							\
  {								\
    if(pos>=len)						\
    {								\
      cpp_error(this,"End of file in character constant.");	\
      break;							\
    }								\
								\
    if(e++>32)							\
    {								\
      cpp_error(this,"Too long character constant.");		\
      break;							\
    }								\
								\
    switch(data[pos++])						\
    {								\
    case '\n':							\
      cpp_error(this,"Newline in char.");			\
      this->current_line++;					\
      break;							\
    case '\'': break;						\
    case '\\': if(data[++pos]=='\n') this->current_line++;	\
    default: continue;						\
    }								\
    break;							\
  } } while(0)

#define DUMPPOS(X)							\
		  fprintf(stderr,"\nSHIFT:%d, POS(%s):",SHIFT,X);	\
		  fflush(stderr);					\
		  write(2,data+pos,20<<SHIFT);				\
		  fprintf(stderr,"\n");					\
		  fflush(stderr)

#define FIND_EOL() do {				\
    while(pos < len && data[pos]!='\n') pos++;	\
  } while(0)

#define SKIPWHITE() do {					\
    if(!WC_ISSPACE(data[pos])) break;				\
    if(data[pos]=='\n') { PUTNL(); this->current_line++; }	\
    pos++;							\
  } while(1)

#define SKIPSPACE() \
  do { while(WC_ISSPACE(data[pos]) && data[pos]!='\n') pos++; \
  } while (0)

#define SKIPCOMMENT()	do{				\
  	pos++;						\
	while(data[pos]!='*' || data[pos+1]!='/')	\
	{						\
	  if(pos+2>=len)				\
	  {						\
	    cpp_error(this,"End of file in comment.");	\
	    break;					\
	  }						\
							\
	  if(data[pos]=='\n')				\
	  {						\
	    this->current_line++;			\
	    PUTNL();					\
	  }						\
							\
	  pos++;					\
	}						\
	pos+=2;						\
  }while(0)

#define READCHAR(C) do {			\
  switch(data[++pos])				\
  {						\
  case 'n': C='\n'; break;			\
  case 'r': C='\r'; break;			\
  case 'b': C='\b'; break;			\
  case 't': C='\t'; break;			\
    						\
  case '0': case '1': case '2': case '3':	\
  case '4': case '5': case '6': case '7':	\
    C=data[pos]-'0';				\
    if(data[pos+1]>='0' && data[pos+1]<='8')	\
    {						\
      C*=8;					\
      C+=data[++pos]-'0';			\
      						\
      if(data[pos+1]>='0' && data[pos+1]<='8')	\
      {						\
	C*=8;					\
	C+=data[++pos]-'0';			\
      }						\
    }						\
    break;					\
    						\
  case '\n':					\
    this->current_line++;			\
    C='\n';					\
    break;					\
    						\
  default:					\
    C = data[pos];				\
  }						\
}while (0)

/* At entry pos points to the start-quote.
 * At end pos points past the end-quote.
 */
#define READSTRING(nf)				\
while(1)					\
{						\
  pos++;					\
  if(pos>=len)					\
  {						\
    cpp_error(this,"End of file in string.");	\
    break;					\
  }						\
						\
  switch(data[pos])				\
  {						\
  case '\n':					\
    cpp_error(this,"Newline in string.");	\
    this->current_line++;			\
    break;					\
  case '"':  break;				\
  case '\\':					\
  {						\
    int tmp;					\
    if(data[pos+1]=='\n')			\
    {						\
      pos++;					\
      continue;					\
    }						\
    if(data[pos+1]=='\r' && data[pos+2]=='\n')	\
    {						\
      pos+=2;					\
      continue;					\
    }						\
    READCHAR(tmp);				\
    string_builder_putchar(&nf, tmp);		\
    continue;					\
  }						\
						\
  default:					\
    string_builder_putchar(&nf, data[pos]);	\
    continue;					\
  }						\
  pos++;					\
  break;					\
}

/* At entry pos points past the start quote.
 * At exit pos points past the end quote.
 */
#define FIXSTRING(nf,outp)	do {			\
int trailing_newlines=0;				\
if(outp) string_builder_putchar(&nf, '"');		\
while(1)						\
{							\
  if(pos>=len)						\
  {							\
    cpp_error(this,"End of file in string.");		\
    break;						\
  }							\
							\
  switch(data[pos++])					\
  {							\
  case '\n':						\
    cpp_error(this,"Newline in string.");		\
    this->current_line++;				\
    break;						\
  case '"':  break;					\
  case '\\':						\
    if(data[pos]=='\n')					\
    {							\
      pos++;						\
      trailing_newlines++;				\
      this->current_line++;				\
      continue;						\
    }							\
    if(data[pos]=='\r' && data[pos+1]=='\n')		\
    {							\
      pos+=2;						\
      trailing_newlines++;				\
      this->current_line++;				\
      continue;						\
    }							\
    if(outp) string_builder_putchar(&nf, '\\');	        \
    pos++;                                              \
							\
  default:						\
    if(outp) string_builder_putchar(&nf, data[pos-1]);	\
    continue;						\
  }							\
  break;						\
}							\
if(outp) string_builder_putchar(&nf, '"');		\
while(trailing_newlines--) PUTNL();			\
}while(0)

#define READSTRING2(nf)				\
while(1)					\
{						\
  pos++;					\
  if(pos>=len)					\
  {						\
    cpp_error(this,"End of file in string.");	\
    break;					\
  }						\
						\
  switch(data[pos])				\
  {						\
  case '"':  break;				\
  case '\\':					\
  {						\
    int tmp;					\
    if(data[pos+1]=='\n')			\
    {						\
      pos++;					\
      this->current_line++;			\
      PUTNL();                                  \
      continue;					\
    }						\
    if(data[pos+1]=='\r' && data[pos+2]=='\n')	\
    {						\
      pos+=2;					\
      this->current_line++;			\
      PUTNL();                                  \
      continue;					\
    }						\
    READCHAR(tmp);				\
    string_builder_putchar(&nf, tmp);		\
    continue;					\
  }						\
						\
  case '\r':  continue; /* ignored */	        \
  case '\n':					\
    PUTNL();					\
    this->current_line++;			\
  default:					\
    string_builder_putchar(&nf, data[pos]);	\
    continue;					\
  }						\
  pos++;					\
  break;					\
}

#define FINDTOK() 				\
  do {						\
  SKIPSPACE();					\
  if(data[pos]=='/')				\
  {						\
    ptrdiff_t tmp;				\
    switch(data[pos+1])				\
    {						\
    case '/':					\
      FIND_EOL();				\
      break;					\
      						\
    case '*':					\
      tmp=pos+2;				\
      while(1)					\
      {						\
        if(data[tmp]=='*')			\
	{					\
	  if(data[tmp+1] == '/')		\
	  {					\
	    pos=tmp+2;				\
	    break;				\
	  }					\
	  break;				\
	}					\
						\
	if(data[tmp]=='\n')			\
	  break;				\
        tmp++;					\
      }						\
    }						\
  }						\
  break;					\
  }while(1)

static struct pike_string *recode_string(struct pike_string *data)
{
  /* Observations:
   *
   * * At least a prefix of two bytes need to be 7bit in a valid
   *   Pike program.
   *
   * * NUL isn't valid in a Pike program.
   */
  /* Heuristic:
   *
   * Index 0 | Index 1 | Interpretation
   * --------+---------+------------------------------------------
   *       0 |       0 | 32bit wide string.
   *       0 |      >0 | 16bit Unicode string.
   *      >0 |       0 | 16bit Unicode string reverse byte order.
   *    0xfe |    0xff | 16bit Unicode string.
   *    0xff |    0xfe | 16bit Unicode string reverse byte order.
   *    0x7b |    0x83 | EBCDIC-US ("#c").
   *    0x7b |    0x40 | EBCDIC-US ("# ").
   *    0x7b |    0x09 | EBCDIC-US ("#\t").
   * --------+---------+------------------------------------------
   *   Other |   Other | 8bit standard string.
   *
   * Note that the tests below are more lenient than the table above.
   * This shouldn't matter, since the other cases would be erroneus
   * anyway.
   */

  /* Add an extra reference to data, since we may return it as is. */
  add_ref(data);

  if ((!((unsigned char *)data->str)[0]) ||
      (((unsigned char *)data->str)[0] == 0xfe) ||
      (((unsigned char *)data->str)[0] == 0xff) ||
      (!((unsigned char *)data->str)[1])) {
    /* Unicode */
    if ((!((unsigned char *)data->str)[0]) &&
	(!((unsigned char *)data->str)[1])) {
      /* 32bit Unicode (UCS4) */
      struct pike_string *new_str;
      ptrdiff_t len;
      ptrdiff_t i;
      ptrdiff_t j;
      p_wchar0 *orig = STR0(data);
      p_wchar2 *dest;

      if (data->len & 3) {
	/* String len is not a multiple of 4 */
	return data;
      }
      len = data->len/4;
      new_str = begin_wide_shared_string(len, 2);

      dest = STR2(new_str);

      j = 0;
      for(i=0; i<len; i++) {
	dest[i] = (orig[j]<<24) | (orig[j+1]<<16) | (orig[j+2]<<8) | orig[j+3];
	j += 4;
      }

      free_string(data);
      return(end_shared_string(new_str));
    } else {
      /* 16bit Unicode (UCS2) */
      if (data->len & 1) {
	/* String len is not a multiple of 2 */
	return data;
      }
      if ((!((unsigned char *)data->str)[1]) ||
	  (((unsigned char *)data->str)[1] == 0xfe)) {
	/* Reverse Byte-order */
	struct pike_string *new_str = begin_shared_string(data->len);
	int i;
	for(i=0; i<data->len; i++) {
	  new_str->str[i^1] = data->str[i];
	}
	free_string(data);
	data = end_shared_string(new_str);
      }
      /* Note: We lose the extra reference to data here. */
      push_string(data);
      f_unicode_to_string(1);
      add_ref(data = sp[-1].u.string);
      pop_stack();
      return data;
    }
  } else if (data->str[0] == '{') {
    /* EBCDIC */
    /* Notes on EBCDIC:
     *
     * * EBCDIC conversion needs to first convert the first line
     *   according to EBCDIC-US, and then the rest of the string
     *   according to the encoding specified by the first line.
     *
     * * It's an error for a program written in EBCDIC not to
     *   start with a #charset directive.
     *
     * Obfuscation note:
     *
     * * This still allows the rest of the file to be written in
     *   another encoding than EBCDIC.
     */

    /* First split out the first line.
     *
     * Note that codes 0x00 - 0x1f are the same in ASCII and EBCDIC.
     */
    struct pike_string *new_str;
    char *p = strchr(data->str, '\n');
    char *p2;
    size_t len;

    if (!p) {
      return data;
    }

    len = p - data->str;

    if (len < CONSTANT_STRLEN("#charset ")) {
      return data;
    }

    new_str = begin_shared_string(len);

    MEMCPY(new_str->str, data->str, len);

    push_string(end_shared_string(new_str));

    push_constant_text("ebcdic-us");

    SAFE_APPLY_MASTER("decode_charset", 2);

    /* Various consistency checks. */
    if ((sp[-1].type != T_STRING) || (sp[-1].u.string->size_shift) ||
	(((size_t)sp[-1].u.string->len) < CONSTANT_STRLEN("#charset")) ||
	(sp[-1].u.string->str[0] != '#')) {
      pop_stack();
      return data;
    }

    /* At this point the decoded first line is on the stack. */

    /* Extract the charset name */

    p = sp[-1].u.string->str + 1;
    while (*p && isspace(*((unsigned char *)p))) {
      p++;
    }

    if (strncmp(p, "charset", CONSTANT_STRLEN("charset")) ||
	!isspace(((unsigned char *)p)[CONSTANT_STRLEN("charset")])) {
      pop_stack();
      return data;
    }

    p += CONSTANT_STRLEN("charset") + 1;

    while (*p && isspace(*((unsigned char *)p))) {
      p++;
    }

    if (!*p) {
      pop_stack();
      return data;
    }

    /* Build a string of the trailing data
     * NOTE:
     *   Keep the newline, so the linenumber info stays correct.
     */

    new_str = begin_shared_string(data->len - len);

    MEMCPY(new_str->str, data->str + len, data->len - len);

    push_string(end_shared_string(new_str));

    stack_swap();

    /* Build a string of the charset name */

    p2 = p;
    while(*p2 && !isspace(*((unsigned char *)p2))) {
      p2++;
    }

    len = p2 - p;

    new_str = begin_shared_string(len);

    MEMCPY(new_str->str, p, len);

    pop_stack();
    push_string(end_shared_string(new_str));
		
    /* Decode the string */

    SAFE_APPLY_MASTER("decode_charset", 2);

    if (sp[-1].type != T_STRING) {
      pop_stack();
      return data;
    }

    /* Accept the new string */

    free_string(data);
    add_ref(data = sp[-1].u.string);
    pop_stack();
  }
  return data;
}

static struct pike_string *filter_bom(struct pike_string *data)
{
  /* More notes:
   *
   * * Character 0xfeff (ZERO WIDTH NO-BREAK SPACE = BYTE ORDER MARK = BOM)
   *   needs to be filtered away before processing continues.
   */
  ptrdiff_t i;
  ptrdiff_t j = 0;
  ptrdiff_t len = data->len;
  struct string_builder buf;

  /* Add an extra reference to data here, since we may return it as is. */
  add_ref(data);

  if (!data->size_shift) {
    return(data);
  }
  
  init_string_builder(&buf, data->size_shift);
  if (data->size_shift == 1) {
    /* 16 bit string */
    p_wchar1 *ptr = STR1(data);
    for(i = 0; i<len; i++) {
      if (ptr[i] == 0xfeff) {
	if (i != j) {
	  string_builder_append(&buf, MKPCHARP(ptr + j, 1), i - j);
	  j = i+1;
	}
      }
    }
    if ((j) && (i != j)) {
      /* Add the trailing string */
      string_builder_append(&buf, MKPCHARP(ptr + j, 1), i - j);
      free_string(data);
      data = finish_string_builder(&buf);
    } else {
      /* String didn't contain 0xfeff */
      free_string_builder(&buf);
    }
  } else {
    /* 32 bit string */
    p_wchar2 *ptr = STR2(data);
    for(i = 0; i<len; i++) {
      if (ptr[i] == 0xfeff) {
	if (i != j) {
	  string_builder_append(&buf, MKPCHARP(ptr + j, 2), i - j);
	  j = i+1;
	}
      }
    }
    if ((j) && (i != j)) {
      /* Add the trailing string */
      string_builder_append(&buf, MKPCHARP(ptr + j, 2), i - j);
      free_string(data);
      data = finish_string_builder(&buf);
    } else {
      /* String didn't contain 0xfeff */
      free_string_builder(&buf);
    }
  }
  return(data);
}

void free_one_define(struct hash_entry *h)
{
  int e;
  struct define *d=BASEOF(h, define, link);

  for(e=0;e<d->num_parts;e++)
    free_string(d->parts[e].postfix);
  if(d->first)
    free_string(d->first);
  free((char *)d);
}

static ptrdiff_t low_cpp(struct cpp *this, void *data, ptrdiff_t len,
			 int shift, int flags, int auto_convert,
			 struct pike_string *charset);

#define SHIFT 0
#include "preprocessor.h"
#undef SHIFT

#define SHIFT 1
#include "preprocessor.h"
#undef SHIFT

#define SHIFT 2
#include "preprocessor.h"
#undef SHIFT

static ptrdiff_t low_cpp(struct cpp *this, void *data, ptrdiff_t len,
			 int shift, int flags, int auto_convert,
			 struct pike_string *charset)
{
  switch(shift) {
  case 0:
    return lower_cpp0(this, (p_wchar0 *)data, len,
		      flags, auto_convert, charset);
  case 1:
    return lower_cpp1(this, (p_wchar1 *)data, len,
		      flags, auto_convert, charset);
  case 2:
    return lower_cpp2(this, (p_wchar2 *)data, len,
		      flags, auto_convert, charset);
  default:
    fatal("low_cpp(): Bad shift: %d\n", shift);
  }
  /* NOT_REACHED */
  return 0;
}

/*** Magic defines ***/
static void insert_current_line(struct cpp *this,
				struct define *def,
				struct define_argument *args,
				struct string_builder *tmp)
{
  char buf[20];
  sprintf(buf," %ld ",(long)this->current_line);
  string_builder_binary_strcat(tmp, buf, strlen(buf));
}

static void insert_current_file_as_string(struct cpp *this,
					  struct define *def,
					  struct define_argument *args,
					  struct string_builder *tmp)
{
  PUSH_STRING_SHIFT(this->current_file->str, this->current_file->len,
		    this->current_file->size_shift, tmp);
}

static void insert_current_time_as_string(struct cpp *this,
					  struct define *def,
					  struct define_argument *args,
					  struct string_builder *tmp)
{
  /* FIXME: Is this code safe? */
  time_t tmp2;
  char *buf;
  time(&tmp2);
  buf=ctime(&tmp2);

  PUSH_STRING0((p_wchar0 *)buf+11, 8, tmp);
}

static void insert_current_date_as_string(struct cpp *this,
					  struct define *def,
					  struct define_argument *args,
					  struct string_builder *tmp)
{
  /* FIXME: Is this code safe? */
  time_t tmp2;
  char *buf;
  time(&tmp2);
  buf=ctime(&tmp2);

  PUSH_STRING0((p_wchar0 *)buf+4, 6, tmp);
  PUSH_STRING0((p_wchar0 *)buf+19, 5, tmp);
}

static void insert_current_version(struct cpp *this,
				   struct define *def,
				   struct define_argument *args,
				   struct string_builder *tmp)
{
  char buf[50];
  sprintf(buf," %d.%d ",this->compat_major, this->compat_minor);
  string_builder_binary_strcat(tmp, buf, strlen(buf));
}


static void insert_current_minor(struct cpp *this,
				 struct define *def,
				 struct define_argument *args,
				 struct string_builder *tmp)
{
  char buf[20];
  sprintf(buf," %d ",this->compat_minor);
  string_builder_binary_strcat(tmp, buf, strlen(buf));
}


static void insert_current_major(struct cpp *this,
				 struct define *def,
				 struct define_argument *args,
				 struct string_builder *tmp)
{
  char buf[20];
  sprintf(buf," %d ",this->compat_major);
  string_builder_binary_strcat(tmp, buf, strlen(buf));
}


static void check_defined(struct cpp *this,
			  struct define *def,
			  struct define_argument *args,
			  struct string_builder *tmp)
{
  struct pike_string *s = NULL;
  switch(args[0].arg.shift) {
  case 0:
    s=binary_findstring((char *)args[0].arg.ptr, args[0].len);
    break;
  case 1:
    s=binary_findstring1((p_wchar1 *)args[0].arg.ptr, args[0].len);
    break;
  case 2:
    s=binary_findstring2((p_wchar2 *)args[0].arg.ptr, args[0].len);
    break;
  default:
    fatal("cpp(): Symbol has unsupported shift: %d\n", args[0].arg.shift);
    break;
  }
  if(s && find_define(s))
  {
    string_builder_binary_strcat(tmp, " 1 ", 3);
  }else{
    string_builder_binary_strcat(tmp, " 0 ", 3);
  }
}

static void dumpdef(struct cpp *this,
		    struct define *def,
		    struct define_argument *args,
		    struct string_builder *tmp)
{
  struct pike_string *s = NULL;
  struct define *d;

  switch(args[0].arg.shift) {
  case 0:
    s=binary_findstring((char *)args[0].arg.ptr, args[0].len);
    break;
  case 1:
    s=binary_findstring1((p_wchar1 *)args[0].arg.ptr, args[0].len);
    break;
  case 2:
    s=binary_findstring2((p_wchar2 *)args[0].arg.ptr, args[0].len);
    break;
  default:
    fatal("cpp(): Bad shift in macroname: %d\n", args[0].arg.shift);
    break;
  }
  if(s && (d=find_define(s)))
  {
    int e;
    char buffer[42];
    PUSH_STRING_SHIFT(d->link.s->str, d->link.s->len,
		      d->link.s->size_shift, tmp);
    if(d->magic)
    {
      string_builder_binary_strcat(tmp, " defined magically ", 19);
    }else{
      string_builder_binary_strcat(tmp, " defined as ", 12);
      
      if(d->first)
	PUSH_STRING_SHIFT(d->first->str, d->first->len,
			  d->first->size_shift, tmp);
      for(e=0;e<d->num_parts;e++)
      {
	if(!(d->parts[e].argument & DEF_ARG_NOPRESPACE))
	  string_builder_putchar(tmp, ' ');
	
	if(d->parts[e].argument & DEF_ARG_STRINGIFY)
	  string_builder_putchar(tmp, '#');
	
	sprintf(buffer,"%ld",(long)(d->parts[e].argument & DEF_ARG_MASK));
	string_builder_binary_strcat(tmp, buffer, strlen(buffer));
	
	if(!(d->parts[e].argument & DEF_ARG_NOPOSTSPACE))
	  string_builder_putchar(tmp, ' ');
	
	PUSH_STRING_SHIFT(d->parts[e].postfix->str, d->parts[e].postfix->len,
			  d->parts[e].postfix->size_shift, tmp);
      } 
    }
  }else{
    string_builder_binary_strcat(tmp, " 0 ", 3);
  }
}

static int do_safe_index_call(struct pike_string *s);

static void check_constant(struct cpp *this,
			  struct define *def,
			  struct define_argument *args,
			  struct string_builder *tmp)
{
  struct svalue *save_stack=sp;
  struct svalue *sv;
  PCHARP data=args[0].arg;
  int res;
  ptrdiff_t dlen,len=args[0].len;
  struct pike_string *s;
  int c;

  while(len && ((c = EXTRACT_PCHARP(data))< 256) && isspace(c)) {
    INC_PCHARP(data, 1);
    len--;
  }

  if(!len)
    cpp_error(this,"#if constant() with empty argument.\n");

  for(dlen=0;dlen<len;dlen++)
    if(!isidchar(INDEX_PCHARP(data, dlen)))
      break;

  if(dlen)
  {
    s = begin_wide_shared_string(dlen, data.shift);
    MEMCPY(s->str, data.ptr, dlen<<data.shift);
    push_string(end_shared_string(s));
#ifdef PIKE_DEBUG
    s = NULL;
#endif /* PIKE_DEBUG */
    if((sv=low_mapping_string_lookup(get_builtin_constants(),
				     sp[-1].u.string)))
    {
      pop_stack();
      push_svalue(sv);
      res=1;
    }else if(get_master()) {
      int i;
      ref_push_string(this->current_file);
      if (this->handler) {
	ref_push_object(this->handler);
      } else {
	push_int(0);
      }

      safe_apply_handler("resolv", this->handler, this->compat_handler, 3);
      
      res=(throw_value.type!=T_STRING) &&
	(!(IS_ZERO(sp-1) && sp[-1].subtype == NUMBER_UNDEFINED));
    }else{
      res=0;
    }
  }else{
    /* Handle contant(.foo) */
    push_text(".");
    ref_push_string(this->current_file);

    if (this->handler) {
      ref_push_object(this->handler);
    } else {
      push_int(0);
    }

    safe_apply_handler("handle_import", this->handler,
		       this->compat_handler, 3);

    res=(throw_value.type!=T_STRING) &&
      (!(IS_ZERO(sp-1) && sp[-1].subtype == NUMBER_UNDEFINED));
  }

  while(1)
  {
    INC_PCHARP(data, dlen);
    len-=dlen;
  
    while(len && ((c = EXTRACT_PCHARP(data)) < 256) && isspace(c)) {
      INC_PCHARP(data, 1);
      len--;
    }

    if(!len) break;

    if(EXTRACT_PCHARP(data) == '.')
    {
      INC_PCHARP(data, 1);
      len--;
      
      while(len && ((c = EXTRACT_PCHARP(data)) < 256) && isspace(c)) {
	INC_PCHARP(data, 1);
	len--;
      }

      if (INDEX_PCHARP(data, 0) == '`') {
	/* LFUN */
	for(dlen=0; dlen<len; dlen++) {
	  int c;
	  if ((c = INDEX_PCHARP(data, dlen)) != '`') {
	    switch(c) {
	    case '/': case '%': case '*': case '&':
	    case '|': case '^': case '~':
	      dlen++;
	      break;
	    case '+':
	      if ((++dlen < len) && INDEX_PCHARP(data, dlen) == '=')
		dlen++;
	      break;
	    case '-':
	      if ((++dlen < len) && INDEX_PCHARP(data, dlen) == '>') {
		dlen++;
	      }
	      break;
	    case '>': case '<': case '!': case '=':
	      {
		/* We get a false match for the operator `!!, but that's ok. */
		int c2;
		if ((++dlen < len) &&
		    (((c2 = INDEX_PCHARP(data, dlen)) == c) || (c2 == '='))) {
		  dlen++;
		}
	      }
	      break;
	    case '(':
	      if ((++dlen < len) && INDEX_PCHARP(data, dlen) == ')') {
		dlen++;
	      } else {
		cpp_error(this, "Expected `().\n");
	      }
	      break;
	    case '[':
	      if ((++dlen < len) && INDEX_PCHARP(data, ++dlen) == ']') {
		dlen++;
	      } else {
		cpp_error(this, "Expected `[].\n");
	      }
	      break;
	    default:
	      cpp_error(this, "Unrecognized ` identifier.\n");
	      break;
	    }
	    break;
	  }
	}
      } else {
  	for(dlen=0; dlen<len; dlen++)
  	  if(!isidchar(INDEX_PCHARP(data, dlen)))
  	    break;
      }

      if(res)
      {
        struct pike_string *s = begin_wide_shared_string(dlen, data.shift);
	MEMCPY(s->str, data.ptr, dlen<<data.shift);
	s = end_shared_string(s);
	res=do_safe_index_call(s);
        free_string(s);
      }
    }else{
      cpp_error(this, "Garbage characters in constant()\n");
    }
  }

  pop_n_elems(sp-save_stack);

  string_builder_binary_strcat(tmp, res?" 1 ":" 0 ", 3);
}


static int do_safe_index_call(struct pike_string *s)
{
  int res;
  JMP_BUF recovery;
  if(!s) return 0;
  
  if (SETJMP(recovery)) {
    res = 0;
  } else {
    ref_push_string(s);
    f_index(2);
    
    res=!(IS_ZERO(sp-1) && sp[-1].subtype == NUMBER_UNDEFINED);
  }
  UNSETJMP(recovery);
  return res;
}


/*! @decl string cpp(string data, string|void current_file, @
 *!                  int|string|void charset, object|void handler, @
 *!                  void|int compat_major, void|int compat_minor)
 *!
 *! Run a string through the preprocessor.
 *!
 *! Preprocesses the string @[data] with Pike's builtin ANSI-C look-alike
 *! preprocessor. If the @[current_file] argument has not been specified,
 *! it will default to @tt{"-"@}. @[charset] defaults to @tt{"ISO-10646"@}.
 *!
 *! @seealso
 *!   @[compile()]
 */
void f_cpp(INT32 args)
{
  struct pike_string *data;
  struct pike_predef_s *tmpf;
  struct svalue *save_sp = sp - args;
  struct cpp this;
  int auto_convert = 0;
  struct pike_string *charset = NULL;

#ifdef PIKE_DEBUG
  ONERROR tmp;
#endif /* PIKE_DEBUG */

  if(args<1)
    Pike_error("Too few arguments to cpp()\n");

  if(sp[-args].type != T_STRING)
    Pike_error("Bad argument 1 to cpp()\n");

  data = sp[-args].u.string;

  add_ref(data);

  this.handler = NULL;
  this.compat_handler = NULL;

  if(args>1)
  {
    if(args > 5)
    {
      if(sp[4-args].type != T_INT)
	Pike_error("Bad argument 5 to cpp()\n");
      if(sp[5-args].type != T_INT)
	Pike_error("Bad argument 6 to cpp()\n");
    }
    if(sp[1-args].type != T_STRING) {
      free_string(data);
      Pike_error("Bad argument 2 to cpp()\n");
    }
    copy_shared_string(this.current_file, sp[1-args].u.string);

    if (args > 2) {
      if (sp[2-args].type == T_STRING) {
	charset = sp[2 - args].u.string;
	push_string(data);
	ref_push_string(charset);
	SAFE_APPLY_MASTER("decode_charset", 2);
	if (sp[-1].type != T_STRING) {
	  free_string(this.current_file);
	  Pike_error("Unknown charset\n");
	}
	data = sp[-1].u.string;
	sp--;
	dmalloc_touch_svalue(sp);
      } else if (sp[2-args].type == T_INT) {
	auto_convert = sp[2-args].u.integer;
      } else {
	free_string(data);
	Pike_error("Bad argument 3 to cpp()\n");
      }
      if (args > 3) {
	if (sp[3-args].type == T_OBJECT) {
	  if ((this.handler = sp[3-args].u.object)) {
	    add_ref(this.handler);
	  }
	} else if (sp[3-args].type != T_INT) {
	  free_string(data);
	  free_string(this.current_file);
	  Pike_error("Bad argument 4 to cpp()\n");
	}
      }
    }
  }else{
    this.current_file=make_shared_string("-");
  }

  if (auto_convert && (!data->size_shift) && (data->len > 1)) {
    /* Try to determine if we need to recode the string */
    struct pike_string *new_data = recode_string(data);
    free_string(data);
    data = new_data;
  }
  if (data->size_shift) {
    /* Get rid of any byte order marks (0xfeff) */
    struct pike_string *new_data = filter_bom(data);
    free_string(data);
    data = new_data;
  }

  init_string_builder(&this.buf, 0);
  this.current_line=1;
  this.compile_errors=0;
  this.defines=0;
  this.compat_major=PIKE_MAJOR_VERSION;
  this.compat_minor=PIKE_MINOR_VERSION;

  do_magic_define(&this,"__LINE__",insert_current_line);
  do_magic_define(&this,"__FILE__",insert_current_file_as_string);
  do_magic_define(&this,"__DATE__",insert_current_date_as_string);
  do_magic_define(&this,"__TIME__",insert_current_time_as_string);
  do_magic_define(&this,"__VERSION__",insert_current_version);
  do_magic_define(&this,"__MAJOR__",insert_current_major);
  do_magic_define(&this,"__MINOR__",insert_current_minor);

  {
    struct define* def=alloc_empty_define(make_shared_string("__dumpdef"),0);
    def->magic=dumpdef;
    def->args=1;
    this.defines=hash_insert(this.defines, & def->link);
  }

  {
    char buffer[128];

    simple_add_define(&this, "__PIKE__", " 1 ");

    simple_add_define(&this, "__REAL_VERSION__",
		      " " DEFINETOSTR(PIKE_MAJOR_VERSION) "."
		      DEFINETOSTR(PIKE_MINOR_VERSION) " ");
    simple_add_define(&this, "__REAL_MAJOR__",
		      " " DEFINETOSTR(PIKE_MAJOR_VERSION) " ");
    simple_add_define(&this, "__REAL_MINOR__",
		      " " DEFINETOSTR(PIKE_MINOR_VERSION) " ");
    simple_add_define(&this, "__BUILD__",
		      " " DEFINETOSTR(PIKE_BUILD_VERSION) " ");
    simple_add_define(&this, "__REAL_BUILD__",
		      " " DEFINETOSTR(PIKE_BUILD_VERSION) " ");
#ifdef AUTO_BIGNUM
    simple_add_define(&this, "__AUTO_BIGNUM__", " 1 ");
#endif
#ifdef __NT__
    simple_add_define(&this, "__NT__", " 1 ");
#endif
#ifdef __amigaos__
    simple_add_define(&this, "__amigaos__", " 1 ");
#endif
  }

  for (tmpf=pike_predefs; tmpf; tmpf=tmpf->next)
    simple_add_define(&this, tmpf->name, tmpf->value);

  string_builder_binary_strcat(&this.buf, "# 1 ", 4);
  PUSH_STRING_SHIFT(this.current_file->str, this.current_file->len,
		    this.current_file->size_shift, &this.buf);
  string_builder_putchar(&this.buf, '\n');

#ifdef PIKE_DEBUG
  SET_ONERROR(tmp, fatal_on_error, "Preprocessor exited with longjump!\n");
#endif /* PIKE_DEBUG */

  if(args > 5)
  {
    cpp_change_compat(&this, sp[4-args].u.integer, sp[5-args].u.integer);
  }


  low_cpp(&this, data->str, data->len, data->size_shift,
	  0, auto_convert, charset);

#ifdef PIKE_DEBUG
  UNSET_ONERROR(tmp);
#endif /* PIKE_DEBUG */

  if(this.defines)
    free_hashtable(this.defines, free_one_define);

  if(this.compat_handler)
  {
    free_object(this.compat_handler);
    this.compat_handler=0;
  }

  if (this.handler)
  {
    free_object(this.handler);
    this.handler = 0;
  }

  free_string(this.current_file);

  free_string(data);

  if(this.compile_errors)
  {
    free_string_builder(&this.buf);
    Pike_error("Cpp() failed\n");
  }else{
    pop_n_elems(sp - save_sp);
    push_string(finish_string_builder(&this.buf));
  }
}

void init_cpp()
{
  defined_macro=alloc_empty_define(make_shared_string("defined"),0);
  defined_macro->magic=check_defined;
  defined_macro->args=1;

  constant_macro=alloc_empty_define(make_shared_string("constant"),0);
  constant_macro->magic=check_constant;
  constant_macro->args=1;

  
/* function(string, string|void, int|string|void, object|void:string) */
  ADD_EFUN("cpp", f_cpp, tFunc(tStr tOr(tStr,tVoid)
			       tOr(tInt,tOr(tStr,tVoid))
			       tOr(tObj,tVoid)
			       tOr(tInt,tVoid)
			       tOr(tInt,tVoid)
			       , tStr), OPT_EXTERNAL_DEPEND);
}


void add_predefine(char *s)
{
  struct pike_predef_s *tmp=ALLOC_STRUCT(pike_predef_s);
  char * pos=STRCHR(s,'=');
  if(pos)
  {
    tmp->name=(char *)xalloc(pos-s+1);
    MEMCPY(tmp->name,s,pos-s);
    tmp->name[pos-s]=0;

    tmp->value=(char *)xalloc(s+strlen(s)-pos);
    MEMCPY(tmp->value,pos+1,s+strlen(s)-pos);
  }else{
    tmp->name=(char *)xalloc(strlen(s)+1);
    MEMCPY(tmp->name,s,strlen(s)+1);

    tmp->value=(char *)xalloc(4);
    MEMCPY(tmp->value," 1 ",4);
  }
  tmp->next=pike_predefs;
  pike_predefs=tmp;
}

void exit_cpp(void)
{
#ifdef DO_PIKE_CLEANUP
  struct pike_predef_s *tmp;
  while((tmp=pike_predefs))
  {
    free(tmp->name);
    free(tmp->value);
    pike_predefs=tmp->next;
    free((char *)tmp);
  }
  free_string(defined_macro->link.s);
  free((char *)defined_macro);

  free_string(constant_macro->link.s);
  free((char *)constant_macro);
#endif
}
