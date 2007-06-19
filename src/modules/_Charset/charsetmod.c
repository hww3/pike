/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: charsetmod.c,v 1.61 2007/06/19 17:35:48 grubba Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "global.h"
#include "program.h"
#include "interpret.h"
#include "stralloc.h"
#include "object.h"
#include "module_support.h"
#include "pike_error.h"

#include "iso2022.h"


#define sp Pike_sp
#define fp Pike_fp

#ifdef __CHAR_UNSIGNED__
#define SIGNED signed
#else
#define SIGNED
#endif

p_wchar1 *misc_charset_lookup(const char *name, int *rlo, int *rhi);

static struct program *std_cs_program = NULL, *std_rfc_program = NULL;
static struct program *utf1_program = NULL, *utf1e_program = NULL;
static struct program *utf7_program = NULL, *utf8_program = NULL;
static struct program *utf7e_program = NULL, *utf8e_program = NULL;
static struct program *utf_ebcdic_program = NULL, *utf_ebcdice_program = NULL;
static struct program *utf7_5_program = NULL, *utf7_5e_program = NULL;
static struct program *euc_program = NULL, *sjis_program = NULL;
static struct program *euce_program = NULL, *sjise_program = NULL;
static struct program *std_94_program = NULL, *std_96_program = NULL;
static struct program *std_9494_program = NULL, *std_9696_program = NULL;
static struct program *std_big5_program = NULL;
static struct program *std_8bit_program = NULL, *std_8bite_program = NULL;
static struct program *std_16bite_program = NULL;
static struct program *multichar_program = NULL, *gb18030e_program = NULL;
static struct program *gbke_program = NULL;

struct std_cs_stor { 
  struct string_builder strbuild;
  struct pike_string *retain, *replace;
  struct svalue repcb;
};

struct std_rfc_stor {
  UNICHAR const *table;
};
static size_t std_rfc_stor_offs = 0;

struct std_misc_stor {
  int lo, hi;
};
static size_t std_misc_stor_offs = 0;

struct utf7_stor {
  INT32 dat, surro;
  int shift, datbit;
};
static size_t utf7_stor_offs = 0;

struct euc_stor {
  UNICHAR const *table, *table2, *table3;
};
static size_t euc_stor_offs = 0;

struct multichar_stor {
  const struct multichar_table *table;
  int is_gb18030;
};
static size_t multichar_stor_offs = 0;

struct std8e_stor {
  p_wchar0 *revtab;
  unsigned int lowtrans, lo, hi;
};
static size_t std8e_stor_offs = 0;

struct std16e_stor {
  p_wchar1 *revtab;
  unsigned int lowtrans, lo, hi;
  int sshift;
};
static size_t std16e_stor_offs = 0;

static SIGNED char rev64t['z'-'+'+1];
static char fwd64t[64]=
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void f_create(INT32 args)
{
  struct std_cs_stor *s = (struct std_cs_stor *)fp->current_storage;

  check_all_args("create()", args, BIT_STRING|BIT_VOID|BIT_INT,
		 BIT_FUNCTION|BIT_VOID|BIT_INT, 0);

  if(args>0 && sp[-args].type == T_STRING) {
    if(s->replace != NULL)
      free_string(s->replace);
    add_ref(s->replace = sp[-args].u.string);
  }

  if(args>1 && sp[1-args].type == T_FUNCTION)
    assign_svalue(&s->repcb, &sp[1-args]);

  pop_n_elems(args);
  push_int(0);
}

static void f_set_repcb(INT32 args)
{
  struct std_cs_stor *s = (struct std_cs_stor *)fp->current_storage;

  check_all_args("set_replacement_callback()", args,
		 BIT_FUNCTION|BIT_INT, 0);

  if(args>0)
    assign_svalue(&s->repcb, &sp[-args]);

  pop_n_elems(args);
}

static int call_repcb(struct svalue *repcb, p_wchar2 ch)
{
  push_string(make_shared_binary_string2(&ch, 1));
  apply_svalue(repcb, 1);
  if(sp[-1].type == T_STRING)
    return 1;
  pop_stack();
  return 0;
}

#define REPLACE_CHAR(ch, func, ctx, pos) do {				\
    if(repcb != NULL && call_repcb(repcb, ch)) {			\
      func(ctx, sb, sp[-1].u.string, rep, NULL);			\
      pop_stack();							\
    } else if(rep != NULL)						\
      func(ctx, sb, rep, NULL, NULL);					\
    else								\
      Pike_error("Character 0x%x at position %"PRINTPTRDIFFT"d "	\
		 "unsupported by encoding.\n",				\
		 ch, (pos));						\
  } while (0)

#define MKREPCB(c) ((c).type == T_FUNCTION? &(c):NULL)

static void f_drain(INT32 args)
{
  struct std_cs_stor *s = (struct std_cs_stor *)fp->current_storage;

  pop_n_elems(args);
  push_string(finish_string_builder(&s->strbuild));
  init_string_builder(&s->strbuild, 0);
}

static void f_drain_rfc1345(INT32 args)
{
  struct std_cs_stor *s = (struct std_cs_stor *)fp->current_storage;
  UNICHAR trailer = 0;

  if (s->strbuild.s->size_shift) {
    ptrdiff_t i, len = s->strbuild.s->len;
    /* We need to check for private-space characters. */
    switch(s->strbuild.s->size_shift) {
    case 1:
      {
	p_wchar1 *s1 = STR1(s->strbuild.s);
	for (i=0; i < len; i++) {
	  if ((s1[i] & 0xff00) == 0xe300) {
	    /* Non-spacing character ==> combiner  */
	    trailer = s1[i++];
	    if (i < len) {
	      s1[i-1] = s1[i];
	      s1[i] = trailer & 0x0fff;
	      trailer = 0;
	    } else {
	      s->strbuild.s->len--;
	      break;
	    }
	  }
	}
      }
      break;
    case 2:
      {
	p_wchar2 *s2 = STR2(s->strbuild.s);
	for (i=0; i < len; i++) {
	  if ((s2[i] & 0xff00) == 0xe300) {
	    /* Non-spacing character ==> combiner */
	    trailer = s2[i++];
	    if (i < len) {
	      s2[i-1] = s2[i];
	      s2[i] = trailer & 0x0fff;
	      trailer = 0;
	    } else {
	      s->strbuild.s->len--;
	      break;
	    }
	  }
	}
      }
      break;
    }
  }
  f_drain(args);
  if (trailer) {
    /* The last character was a non-spacing character.
     * Restore it for the next pass.
     */
    string_builder_putchar(&s->strbuild, trailer);
  }
}

static void f_clear(INT32 args)
{
  struct std_cs_stor *s = (struct std_cs_stor *)fp->current_storage;

  pop_n_elems(args);

  if(s->retain != NULL) {
    free_string(s->retain);
    s->retain = NULL;
  }

  reset_string_builder(&s->strbuild);
  
  push_object(this_object());
}

static void init_stor(struct object *o)
{
  struct std_cs_stor *s = (struct std_cs_stor *)fp->current_storage;

  s->retain = NULL;
  s->replace = NULL;

  init_string_builder(&s->strbuild,0);
}

static void exit_stor(struct object *o)
{
  struct std_cs_stor *s = (struct std_cs_stor *)fp->current_storage;

  if(s->retain != NULL) {
    free_string(s->retain);
    s->retain = NULL;
  }

  if(s->replace != NULL) {
    free_string(s->replace);
    s->replace = NULL;
  }

  reset_string_builder(&s->strbuild);
  free_string(finish_string_builder(&s->strbuild));
}

static void f_std_feed(INT32 args, ptrdiff_t (*func)(const p_wchar0 *,
						     ptrdiff_t n,
						     struct std_cs_stor *))
{
  struct std_cs_stor *s = (struct std_cs_stor *)fp->current_storage;
  struct pike_string *str;
  ptrdiff_t l;

  get_all_args("feed()", args, "%W", &str);

  if(str->size_shift>0)
    Pike_error("Can't feed on wide strings!\n");

  if(s->retain != NULL) {
    str = add_shared_strings(s->retain, str);
    push_string (str);
    args++;
  }

  l = func(STR0(str), str->len, s);

  if (s->retain) {
    free_string(s->retain);
    s->retain = NULL;
  }
  if(l>0)
    s->retain = make_shared_binary_string((char *)STR0(str)+str->len-l, l);

  pop_n_elems(args);
  push_object(this_object());
}


static ptrdiff_t feed_utf8(const p_wchar0 *p, ptrdiff_t l,
			   struct std_cs_stor *s)
{
  static const int utf8cont[] = { 0, 0, 0, 0, 0, 0, 0, 0,
				  0, 0, 0, 0, 0, 0, 0, 0,
				  0, 0, 0, 0, 0, 0, 0, 0,
				  0, 0, 0, 0, 0, 0, 0, 0,
				  1, 1, 1, 1, 1, 1, 1, 1,
				  1, 1, 1, 1, 1, 1, 1, 1,
				  2, 2, 2, 2, 2, 2, 2, 2,
				  3, 3, 3, 3, 0, 0, 0, 0 };
  static const unsigned int first_char_mask[] = {0x1f, 0x0f, 0x07, 0x03, 0x01};

  const p_wchar0 *start = p;
  for (; l > 0; l--) {
    unsigned int ch = *p++;

    if (ch & 0x80) {
      int cl = utf8cont[(ch>>1) - 64], i;
      if (!cl)
	Pike_error ("Got invalid byte 0x%x at position %"PRINTPTRDIFFT"d.\n",
		    ch, p - start - 1 - (s->retain ? s->retain->len : 0));

      ch &= first_char_mask[cl - 1];

      for (i = cl >= l ? l - 1 : cl; i--;) {
	unsigned int c = *p++;
	if ((c & 0xc0) != 0x80)
	  Pike_error ("Got invalid UTF-8 sequence continuation byte 0x%x "
		      "at position %"PRINTPTRDIFFT"d.\n",
		      c, p - start - 1 - (s->retain ? s->retain->len : 0));
	ch = (ch << 6) | (c & 0x3f);
      }

      if(cl >= l)
	return l;
      l -= cl;

      switch (cl) {
	case 1: if (ch >= (1 << 7)) break;
	case 2: if (ch >= (1 << 11)) break;
	case 3: if (ch >= (1 << 16)) break;
	  {
	    ptrdiff_t errpos =
	      p - start - cl - 1 - (s->retain ? s->retain->len : 0);
	    if (errpos < 0) errpos = 0;
	    Pike_error ("Got non-shortest form of char 0x%x "
			"at position %"PRINTPTRDIFFT"d.\n",
			ch, errpos);
	  }
      }

      if ((ch >= 0xd800 && ch <= 0xdfff) || ch > 0x10ffff) {
	ptrdiff_t errpos =
	  p - start - cl - 1 - (s->retain ? s->retain->len : 0);
	if (errpos < 0) errpos = 0;
	Pike_error ("Char 0x%x at position %"PRINTPTRDIFFT"d "
		    "is outside the valid range.\n", ch, errpos);
      }
    }

    string_builder_putchar(&s->strbuild, ch);
  }

  return l;
}

static void f_feed_utf8(INT32 args)
{
  f_std_feed(args, feed_utf8);
}

/* From http://www.unicode.org/reports/tr16/
 *   Table 3: Byte map from UTF-EBCDIC byte-sequence to I8-sequence.
 */
static const unsigned char utf_ebcdic_to_i8_conv[] = {
  0x00, 0x01, 0x02, 0x03, 0x9c, 0x09, 0x86, 0x7f,
  0x97, 0x8d, 0x8e, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x9d, 0x0a, 0x08, 0x87,
  0x18, 0x19, 0x92, 0x8f, 0x1c, 0x1d, 0x1e, 0x1f,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x17, 0x1b,
  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x05, 0x06, 0x07,
  0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04,
  0x98, 0x99, 0x9a, 0x9b, 0x14, 0x15, 0x9e, 0x1a,
  0x20, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
  0xa7, 0xa8, 0xa9, 0x2e, 0x3c, 0x28, 0x2b, 0x7c,
  0x26, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
  0xb1, 0xb2, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x5e,
  0x2d, 0x2f, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
  0xb9, 0xba, 0xbb, 0x2c, 0x25, 0x5f, 0x3e, 0x3f,
  0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3,
  0xc4, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22,
  0xc5, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  0x68, 0x69, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb,
  0xcc, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
  0x71, 0x72, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2,
  0xd3, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0xd4, 0xd5, 0xd6, 0x5b, 0xd7, 0xd8,
  0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
  0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0x5d, 0xe6, 0xe7,
  0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed,
  0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
  0x51, 0x52, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
  0x5c, 0xf4, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x9f,
};

static ptrdiff_t feed_utf_ebcdic(const p_wchar0 *p, ptrdiff_t l,
				 struct std_cs_stor *s)
{
  static const int cont[] = { 0, 0, 0, 0, 0, 0, 0, 0,
			      0, 0, 0, 0, 0, 0, 0, 0,
			      1, 1, 1, 1, 1, 1, 1, 1,
			      1, 1, 1, 1, 1, 1, 1, 1,
			      2, 2, 2, 2, 2, 2, 2, 2,
			      3, 3, 3, 3, 4, 4, 0, 0, };
  static const unsigned int first_char_mask[] = {0x1f, 0x0f, 0x07, 0x03, 0x01};

  const p_wchar0 *start = p;
  for (; l > 0; l--) {
    unsigned int ch = utf_ebcdic_to_i8_conv[*p++];

    if ((ch & 0x80) && (ch & 0x60)) {
      int cl = cont[(ch>>1) - 80];
      int i;
      if (!cl)
	Pike_error ("Got invalid byte 0x%x at position %"PRINTPTRDIFFT"d.\n",
		    ch, p - start - 1 - (s->retain ? s->retain->len : 0));

      ch &= first_char_mask[cl - 1];

      for (i = cl >= l ? l - 1 : cl; i--;) {
	unsigned int c = utf_ebcdic_to_i8_conv[*p++];
	if ((c & 0xe0) != 0xa0)
	  Pike_error ("Got invalid UTF-EBCDIC I8-sequence continuation "
		      "byte 0x%x at position %"PRINTPTRDIFFT"d.\n",
		      c, p - start - 1 - (s->retain ? s->retain->len : 0));
	ch = (ch << 5) | (c & 0x1f);
      }

      if(cl >= l)
	return l;

      l -= cl;

#if 0
      /* FIXME: Detect non-shortest sequence. */
      switch (cl) {
	case 1: if (ch >= (1 << 7)) break;
	case 2: if (ch >= (1 << 11)) break;
	case 3: if (ch >= (1 << 16)) break;
	  {
	    ptrdiff_t errpos =
	      p - start - cl - 1 - (s->retain ? s->retain->len : 0);
	    if (errpos < 0) errpos = 0;
	    Pike_error ("Got non-shortest form of char 0x%x "
			"at position %"PRINTPTRDIFFT"d.\n",
			ch, errpos);
	  }
      }

      if ((ch >= 0xd800 && ch <= 0xdfff) || ch > 0x10ffff) {
	ptrdiff_t errpos =
	  p - start - cl - 1 - (s->retain ? s->retain->len : 0);
	if (errpos < 0) errpos = 0;
	Pike_error ("Char 0x%x at position %"PRINTPTRDIFFT"d "
		    "is outside the valid range.\n", ch, errpos);
      }
#endif /* 0 */
    }

    string_builder_putchar(&s->strbuild, ch);
  }

  return l;
}

static void f_feed_utf_ebcdic(INT32 args)
{
  f_std_feed(args, feed_utf_ebcdic);
}

static ptrdiff_t feed_utf7_5(const p_wchar0 *p, ptrdiff_t l,
			     struct std_cs_stor *s)
{
  static int utf7_5len[] = { 0, 0, 0, 0, 0, 0, 0, 0,
			    -1,-1, 1, 2,-1,-1,-1,-1, };
  static const unsigned INT32 utf7_5of[] = { 0ul, 0x28c0ul, 0xb30c0ul };
  while(l>0) {
    unsigned INT32 ch = 0;
    int cl = utf7_5len[(*p)>>4];
    if(cl>--l)
      return l+1;
    switch(cl) {
    case 2: ch += *p++; ch<<=6;
    case 1: ch += *p++; ch<<=6;
    case 0: ch += *p++;
      break;
    case -1:
      /* FIXME: Encoding error if cl < 0. */
      cl = 0;
      break;
    }
    l-=cl;
    string_builder_putchar(&s->strbuild, (ch-utf7_5of[cl])&0x7fffffffl);
  }
  return l;
}

static void f_feed_utf7_5(INT32 args)
{
  f_std_feed(args, feed_utf7_5);
}

static ptrdiff_t feed_utf7(const p_wchar0 *p, ptrdiff_t l,
			   struct std_cs_stor *s)
{
  struct utf7_stor *u7 = (struct utf7_stor *)(((char*)s)+utf7_stor_offs);
  INT32 dat = u7->dat, surro = u7->surro;
  int shift = u7->shift, datbit = u7->datbit;

  if(l<=0)
    return l;

  if(shift==2) {
    if(*p=='-') {
      string_builder_putchar(&s->strbuild, '+');
      if(--l==0) {
	u7->shift=0;
	return l;
      }
      p++;
      shift=0;
    } else
      shift=1;
  }

  for(;;)
    if(shift) {
      int c = 0, z;
      while(l-->0 && (c=(*p++)-'+')>=0 && c<=('z'-'+') && (z=rev64t[c])>=0) {
	dat = (dat<<6)|z;
	if((datbit+=6)>=16) {
	  INT32 uc = dat>>(datbit-16);
	  if((uc&0xfc00)==0xd800) {
	    if(surro)
	      string_builder_putchar(&s->strbuild, surro);
	    surro = uc;
	  } else if(surro) {
	    if((uc&0xfc00)==0xdc00)
	      string_builder_putchar(&s->strbuild, 0x00010000+
				     ((surro&0x3ff)<<10)+(uc&0x3ff));
	    else {
	      string_builder_putchar(&s->strbuild, surro);
	      string_builder_putchar(&s->strbuild, uc);
	    }
	    surro = 0;
	  } else
	    string_builder_putchar(&s->strbuild, uc);
	  datbit -= 16;
	  dat &= (1<<datbit)-1;
	}
      }
      if(l<0) {
	l++;
	break;
      }
      if(surro) {
	string_builder_putchar(&s->strbuild, surro);
	surro = 0;
      }
      /* should check that dat is 0 here. */
      shift=0;
      dat=0;
      datbit=0;
      if(c!=('-'-'+')) {
	l++;
	--p;
      } else
	if(l==0)
	  break;	
    } else {
      while(l-->0 && *p!='+')
	string_builder_putchar(&s->strbuild, *p++);
      if(l<0) {
	l++;
	break;
      }
      p++;
      if(l==0) {
	shift=2;
	break;
      }
      if(*p=='-') {
	string_builder_putchar(&s->strbuild, '+');
	if(--l==0)
	  break;
	p++;
      } else
	shift = 1;
    }

  u7->dat = dat;
  u7->surro = surro;
  u7->shift = shift;
  u7->datbit = datbit;
  return l;
}

static void f_clear_utf7(INT32 args)
{
  struct utf7_stor *u7 =
    (struct utf7_stor *)(fp->current_storage+utf7_stor_offs);

  f_clear(args);
  
  u7->dat = 0;
  u7->surro = 0;
  u7->shift = 0;
  u7->datbit = 0;
}

static void utf7_init_stor(struct object *o)
{
  struct utf7_stor *u7 =
    (struct utf7_stor *)(fp->current_storage+utf7_stor_offs);

  u7->dat = 0;
  u7->surro = 0;
  u7->shift = 0;
  u7->datbit = 0;
}

static void f_feed_utf7(INT32 args)
{
  f_std_feed(args, feed_utf7);
}

static ptrdiff_t feed_sjis(const p_wchar0 *p, ptrdiff_t l,
			   struct std_cs_stor *s)
{
  extern UNICHAR map_JIS_C6226_1983[];
  while(l>0) {
    unsigned INT32 ch = *p++;
    if(ch < 0x80) {
      if(ch == 0x5c)
	ch = 0xa5;
      else if(ch == 0x7e)
	ch = 0x203e;
      string_builder_putchar(&s->strbuild, ch);
      --l;
    } else if(ch < 0xa1 || ch >= 0xe0) {
      if(ch == 0x80 || ch == 0xa0 || ch >= 0xeb) {
	string_builder_putchar(&s->strbuild, 0xfffd);
	--l;
      } else {
	int lo;
	if(l<2)
	  return l;
	lo = *p++;
	l -= 2;
	if(ch > 0xa0)
	  ch -= 0x40;
	if(lo >= 0x40 && lo <= 0x9e && lo != 0x7f) {
	  if(lo > 0x7f)
	    --lo;
	  ch = map_JIS_C6226_1983[(ch-0x81)*188+(lo-0x40)];
	} else if(lo >= 0x9f && lo <= 0xfc)
	  ch = map_JIS_C6226_1983[(ch-0x81)*188+94+(lo-0x9f)];
	else
	  ch = 0xfffd;
	string_builder_putchar(&s->strbuild, ch);
      }
    } else {
      string_builder_putchar(&s->strbuild, ch+0xfec0);
      --l;
    }
  }
  return l;
}

static void f_feed_sjis(INT32 args)
{
  f_std_feed(args, feed_sjis);
}

static ptrdiff_t feed_euc(const p_wchar0 *p, ptrdiff_t l,
			  struct std_cs_stor *s)
{
  struct euc_stor *euc = (struct euc_stor *)(((char*)s)+euc_stor_offs);
  UNICHAR const *map = euc->table;
  UNICHAR const *map2 = euc->table2;
  UNICHAR const *map3 = euc->table3;

  while(l>0) {
    unsigned INT32 ch = *p++;
    if(ch < 0x80) {
      string_builder_putchar(&s->strbuild, ch);
      --l;
    } else if(ch > 0xa0 && ch < 0xff) {
      int lo;
      if(l<2)
	return l;
      lo = (*p++)|0x80;
      if(lo > 0xa0 && lo < 0xff)
	ch = map[(ch-0xa1)*94+(lo-0xa1)];
      else
	ch = 0xfffd;
      string_builder_putchar(&s->strbuild, ch);
      l -= 2;
    } else if(ch == 0x8e) {
      if(l<2)
	return l;
      ch = (*p++)|0x80;
      if(ch > 0xa0 && ch < 0xff)
	ch = map2[ch-0xa1];
      else
	ch = 0xfffd;
      string_builder_putchar(&s->strbuild, ch);
      l -= 2;
    } else if(ch == 0x8f) {
      int lo;
      if(l<3)
	return l;
      ch = (*p++)|0x80;
      lo = (*p++)|0x80;
      if(ch > 0xa0 && ch < 0xff && lo > 0xa0 && lo < 0xff)
	ch = map3[(ch-0xa1)*94+(lo-0xa1)];
      else
	ch = 0xfffd;
      string_builder_putchar(&s->strbuild, ch);
      l -= 3;
    } else {
      string_builder_putchar(&s->strbuild, 0xfffd);
      --l;
    }
  }
  return l;
}

static void f_feed_euc(INT32 args)
{
  f_std_feed(args, feed_euc);
}

static void f_create_euc(INT32 args)
{
  struct euc_stor *s = (struct euc_stor *)(fp->current_storage + euc_stor_offs);
  extern struct charset_def charset_map[];
  extern int num_charset_def;
  struct pike_string *str;
  int lo=0, hi=num_charset_def-1;

  check_all_args("create()", args, BIT_STRING, 0);

  str = sp[-args].u.string;

  if(str->size_shift>0)
    hi = -1;

  while(lo<=hi) {
    int c, mid = (lo+hi)>>1;
    if((c = strcmp((char *)STR0(str), charset_map[mid].name))==0) {
      if(charset_map[mid].mode == MODE_9494)
	s->table = charset_map[mid].table;
      break;
    }
    if(c<0)
      hi=mid-1;
    else
      lo=mid+1;
  }

  if(s->table == NULL)
    Pike_error("Unknown charset in EUCDec\n");

  if(s->table == iso2022_9494[2]) {
    s->table2 = iso2022_94[9];
    s->table3 = iso2022_9494[4];
  } else {
    s->table2 = NULL;
    s->table3 = NULL;
  }

  pop_n_elems(args);
  push_int(0);
}

static void f_create_multichar(INT32 args)
{
  extern struct multichar_def multichar_map[];
  char *name;
  struct multichar_def *def = multichar_map;
  struct multichar_stor *s = (struct multichar_stor *)(fp->current_storage + multichar_stor_offs);

  get_all_args("create()", args, "%s", &name);

  while(1) {
    if(def->name == 0)
      Pike_error("Unknown multichar table.\n");
    if( strcmp(name, def->name)==0 )
      break;
    def++;
  }

  s->table = def->table;
  /* NOTE: gb18030 is the first in the multichar map! */
  s->is_gb18030 = (def == multichar_map);
}

#include "gb18030.h"

/* Used for gb18030 to decode code points outside GBK. */
static ptrdiff_t feed_gb18030(const p_wchar0 *p, ptrdiff_t l,
			      struct std_cs_stor *s)
{
  p_wchar2 index = 0;
  if (l < 4) {
    return l;
  }

  /* First decode the linear offset. */

  if ((p[0] < 0x81) || (p[0] > 0xfe)) {
#if 0
    fprintf(stderr, "Illegal character: 0x%02x (expected 0x81 .. 0xfe)\n",
	    p[0]);
#endif /* 0 */
    return 0;
  }
  index = p[0] - 0x81;

  if ((p[1] < 0x30) || (p[1] > 0x39)) {
#if 0
    fprintf(stderr, "Illegal character: 0x%02x (expected 0x30 .. 0x39)\n",
	    p[0]);
#endif /* 0 */
    return 0;
  }
  index *= 10;
  index += p[1] - 0x30;

  if ((p[2] < 0x81) || (p[2] > 0xfe)) {
#if 0
    fprintf(stderr, "Illegal character: 0x%02x (expected 0x81 .. 0xfe)\n",
	    p[0]);
#endif /* 0 */
    return 0;
  }
  index *= 126;
  index += p[2] - 0x81;

  if ((p[3] < 0x30) || (p[3] > 0x39)) {
#if 0
    fprintf(stderr, "Illegal character: 0x%02x (expected 0x30 .. 0x39)\n",
	    p[0]);
#endif /* 0 */
    return 0;
  }
  index *= 10;
  index += p[3] - 0x30;

  /* Convert to Unicode. */
  string_builder_putchar(&s->strbuild, gb18030_to_unicode(index));

  /* We've used 4 bytes of input. */
  return -4;
}

static ptrdiff_t feed_multichar(const p_wchar0 *p, ptrdiff_t l,
				struct std_cs_stor *s)
{
  struct multichar_stor *m = (struct multichar_stor *)(fp->current_storage + multichar_stor_offs);
  const struct multichar_table *table = m->table;

  while(l>0) {
    unsigned INT32 ch = *p++;
    if(ch < 0x81) {
      /* FIXME: Adjust above limit to 0x80? Recent GB18030 encodes
       *        U+0080 as 0x81 0x30 0x81 0x30.
       */
      string_builder_putchar(&s->strbuild, ch);
      --l;
    }
    else {
      const struct multichar_table page = table[ ch-0x81 ];
      if(l==1) return 1;
      if(ch==0xff) {
	Pike_error("Illegal character: 0xff.\n");
      }
      ch = *p++;
      if( ch<page.lo || ch>page.hi ) {
	if (m->is_gb18030) {
	  int delta = feed_gb18030(p-2, l, s);
	  if (delta < 0) {
	    p -= delta + 2;
	    l += delta;
	    continue;
	  } else if (delta > 0) {
	    /* More characters needed. */
	    return delta;
	  }
	} 
	Pike_error("Illegal character pair: 0x%02x 0x%02x "
		   "(expected 0x%02x 0x%02x..0x%02x).\n",
		   p[-2], ch, p[-2], page.lo, page.hi);
      }
      else
	string_builder_putchar(&s->strbuild, page.table[ch-page.lo]);
      l -= 2;
    }
  }
  return 0;
}

static void f_feed_multichar(INT32 args)
{
  f_std_feed(args, feed_multichar);
}

static void feed_gb18030e(struct std_cs_stor *cs, struct string_builder *sb,
			  struct pike_string *str, struct pike_string *rep,
			  struct svalue *repcb)
{
  ptrdiff_t l = str->len;
  const struct gb18030e_info *e_info;

  switch(str->size_shift) {
  case 0:
    {
      p_wchar0 c, *p = STR0(str);
      while(l--)
	if((c=*p++)<=0x7f)
	  string_builder_putchar(sb, c);
        else if ((e_info = get_gb18030e_info(c))) {
	  if (e_info->index < 0) {
	    /* Two bytes (ie GBK). */
	    int off = ~e_info->index + (c - e_info->ulow)*2;
	    string_builder_putchar(sb, gb18030e_bytes[off]);
	    string_builder_putchar(sb, gb18030e_bytes[off+1]);
	  } else {
	    /* Four bytes */
	    int index = e_info->index + c - e_info->ulow;
	    c = index/12600;
	    string_builder_putchar(sb, 0x81 + c);
	    index -= c*12600;
	    c = index/1260;
	    string_builder_putchar(sb, 0x30 + c);
	    index -= c*1260;
	    c = index/10;
	    string_builder_putchar(sb, 0x81 + c);
	    index -= c*10;
	    string_builder_putchar(sb, 0x30 + index);
	  }
	} else {
	  REPLACE_CHAR(c, feed_gb18030e, cs, p - STR0(str) - 1);
	}
    }
    break;
  case 1:
    {
      p_wchar1 c, *p = STR1(str);
      while(l--)
	if((c=*p++)<=0x7f)
	  string_builder_putchar(sb, c);
	else if ((e_info = get_gb18030e_info(c))) {
	  if (e_info->index < 0) {
	    /* Two bytes (ie GBK). */
	    int off = ~e_info->index + (c - e_info->ulow)*2;
	    string_builder_putchar(sb, gb18030e_bytes[off]);
	    string_builder_putchar(sb, gb18030e_bytes[off+1]);
	  } else {
	    /* Four bytes */
	    int index = e_info->index + c - e_info->ulow;
	    c = index/12600;
	    string_builder_putchar(sb, 0x81 + c);
	    index -= c*12600;
	    c = index/1260;
	    string_builder_putchar(sb, 0x30 + c);
	    index -= c*1260;
	    c = index/10;
	    string_builder_putchar(sb, 0x81 + c);
	    index -= c*10;
	    string_builder_putchar(sb, 0x30 + index);
	  }
	} else {
	  REPLACE_CHAR(c, feed_gb18030e, cs, p - STR1(str) - 1);
	}
    }
    break;
  case 2:
    {
      p_wchar2 c, *p = STR2(str);
      while(l--) {
	if((c=*p++)<=0x7f) {
	  string_builder_putchar(sb, c);
	}
	else if ((e_info = get_gb18030e_info(c))) {
	  if (e_info->index < 0) {
	    /* Two bytes (ie GBK). */
	    int off = ~e_info->index + (c - e_info->ulow)*2;
	    string_builder_putchar(sb, gb18030e_bytes[off]);
	    string_builder_putchar(sb, gb18030e_bytes[off+1]);
	  } else {
	    /* Four bytes */
	    int index = e_info->index + c - e_info->ulow;
	    c = index/12600;
	    string_builder_putchar(sb, 0x81 + c);
	    index -= c*12600;
	    c = index/1260;
	    string_builder_putchar(sb, 0x30 + c);
	    index -= c*1260;
	    c = index/10;
	    string_builder_putchar(sb, 0x81 + c);
	    index -= c*10;
	    string_builder_putchar(sb, 0x30 + index);
	  }
	} else {
	  REPLACE_CHAR(c, feed_gb18030e, cs, p - STR2(str) - 1);
	}
      }
    }
    break;
  default:
    Pike_fatal("Illegal shift size!\n");
  }
}

static void f_feed_gb18030e(INT32 args)
{
  struct pike_string *str;
  struct std_cs_stor *cs = (struct std_cs_stor *)fp->current_storage;

  get_all_args("feed()", args, "%W", &str);

  feed_gb18030e(cs, &cs->strbuild, str, cs->replace, MKREPCB(cs->repcb));

  pop_n_elems(args);
  push_object(this_object());
}

static void feed_gbke(struct std_cs_stor *cs, struct string_builder *sb,
		      struct pike_string *str, struct pike_string *rep,
		      struct svalue *repcb)
{
  ptrdiff_t l = str->len;
  const struct gb18030e_info *e_info;

  switch(str->size_shift) {
  case 0:
    {
      p_wchar0 c, *p = STR0(str);
      while(l--)
	if((c=*p++)<=0x7f)
	  string_builder_putchar(sb, c);
        else if ((e_info = get_gb18030e_info(c)) &&
		 (e_info->index < 0)) {
	  /* Two bytes (ie GBK). */
	  int off = ~e_info->index + (c - e_info->ulow)*2;
	  string_builder_putchar(sb, gb18030e_bytes[off]);
	  string_builder_putchar(sb, gb18030e_bytes[off+1]);
	} else {
	  REPLACE_CHAR(c, feed_gbke, cs, p - STR0(str) - 1);
	}
    }
    break;
  case 1:
    {
      p_wchar1 c, *p = STR1(str);
      while(l--)
	if((c=*p++)<=0x7f)
	  string_builder_putchar(sb, c);
	else if ((e_info = get_gb18030e_info(c)) &&
		 (e_info->index < 0)) {
	  /* Two bytes (ie GBK). */
	  int off = ~e_info->index + (c - e_info->ulow)*2;
	  string_builder_putchar(sb, gb18030e_bytes[off]);
	  string_builder_putchar(sb, gb18030e_bytes[off+1]);
	} else {
	  REPLACE_CHAR(c, feed_gbke, cs, p - STR1(str) - 1);
	}
    }
    break;
  case 2:
    {
      p_wchar2 c, *p = STR2(str);
      while(l--) {
	if((c=*p++)<=0x7f) {
	  string_builder_putchar(sb, c);
	}
	else if ((e_info = get_gb18030e_info(c)) &&
		 (e_info->index < 0)) {
	  /* Two bytes (ie GBK). */
	  int off = ~e_info->index + (c - e_info->ulow)*2;
	  string_builder_putchar(sb, gb18030e_bytes[off]);
	  string_builder_putchar(sb, gb18030e_bytes[off+1]);
	} else {
	  REPLACE_CHAR(c, feed_gbke, cs, p - STR2(str) - 1);
	}
      }
    }
    break;
  default:
    Pike_fatal("Illegal shift size!\n");
  }
}

static void f_feed_gbke(INT32 args)
{
  struct pike_string *str;
  struct std_cs_stor *cs = (struct std_cs_stor *)fp->current_storage;

  get_all_args("feed()", args, "%W", &str);

  feed_gbke(cs, &cs->strbuild, str, cs->replace, MKREPCB(cs->repcb));

  pop_n_elems(args);
  push_object(this_object());
}

static void f_create_sjise(INT32 args)
{
  struct std16e_stor *s = (struct std16e_stor *)(fp->current_storage + std16e_stor_offs);
  int i, j, z;
  extern UNICHAR map_JIS_C6226_1983[];

  s->lowtrans = 0x5c;
  s->lo = 0x5c;
  s->hi = 0xfffd;

  memset((s->revtab = (p_wchar1 *)xalloc((s->hi-s->lo)*sizeof(p_wchar1))), 0,
	 (s->hi-s->lo)*sizeof(p_wchar1));

  for(z=0, i=33; i<=126; i++, z+=94)
    for(j=33; j<=126; j++) {
      UNICHAR c;
      if((c=map_JIS_C6226_1983[z+j-33])!=0xfffd && c>=s->lo) {
	if(i&1)
	  s->revtab[c-s->lo]=(((i>>1)+(i<95? 113:177))<<8)|(j+(j<96? 31:32));
	else
	  s->revtab[c-s->lo]=(((i>>1)+(i<95? 112:176))<<8)|(j+126);
      }
    }

  for(i=0x5d; i<0x7e; i++)
    s->revtab[i-s->lo] = i;

  for(i=1; i<64; i++)
    s->revtab[i+0xff60-s->lo] = 0xa0+i;

  s->revtab[0xa5 - s->lo] = 0x5c;
  s->revtab[0x203e - s->lo] = 0x7e;

  f_create(args);
  push_int(0);
}

static void f_create_euce(INT32 args)
{
  struct std16e_stor *s = (struct std16e_stor *)(fp->current_storage + std16e_stor_offs);
  extern struct charset_def charset_map[];
  extern int num_charset_def;
  struct pike_string *str;
  int i, j, z, lo=0, hi=num_charset_def-1;
  UNICHAR const *table=NULL;

  check_all_args("create()", args, BIT_STRING, BIT_STRING|BIT_VOID|BIT_INT,
		 BIT_FUNCTION|BIT_VOID|BIT_INT, 0);

  str = sp[-args].u.string;

  if(str->size_shift>0)
    hi = -1;

  while(lo<=hi) {
    int c, mid = (lo+hi)>>1;
    if((c = strcmp((char *)STR0(str), charset_map[mid].name))==0) {
      if(charset_map[mid].mode == MODE_9494)
	table = charset_map[mid].table;
      break;
    }
    if(c<0)
      hi=mid-1;
    else
      lo=mid+1;
  }

  if(table == NULL)
    Pike_error("Unknown charset in EUCEnc\n");

  s->lowtrans = 128;
  s->lo = 128;
  s->hi = 128;

  memset((s->revtab = (p_wchar1 *)xalloc((65536-s->lo)*sizeof(p_wchar1))), 0,
	 (65536-s->lo)*sizeof(p_wchar1));

  for(z=0, i=33; i<=126; i++, z+=94)
    for(j=33; j<=126; j++) {
      UNICHAR c;
      if((c=table[z+j-33])!=0xfffd && c>=s->lo) {
	s->revtab[c-s->lo]=(i<<8)|j|0x8080;
	if(c>=s->hi)
	  s->hi = c+1;
      }
    }

  if(table == iso2022_9494[2]) {
    s->sshift = 1;
    table = iso2022_94[9];
    for(j=33; j<=126; j++) {
      UNICHAR c;
      if((c=table[j-33])!=0xfffd && c>=s->lo &&
	 !s->revtab[c-s->lo]) {
	s->revtab[c-s->lo]=j;
	if(c>=s->hi)
	  s->hi = c+1;
      }
    }
    table = iso2022_9494[4];
    for(z=0, i=33; i<=126; i++, z+=94)
      for(j=33; j<=126; j++) {
	UNICHAR c;
	if((c=table[z+j-33])!=0xfffd && c>=s->lo &&
	   !s->revtab[c-s->lo]) {
	  s->revtab[c-s->lo]=(i<<8)|j|0x8000;
	  if(c>=s->hi)
	    s->hi = c+1;
	}
      }
  }

  f_create(args-1);
  pop_stack();
  push_int(0);
}

static struct std8e_stor *push_std_8bite(int args, int allargs, int lo, int hi)
{
  struct std8e_stor *s8;
  push_object(clone_object(std_8bite_program, args));
  if((allargs-=args)>0) {
    struct object *o = sp[-1].u.object;
    add_ref(o);
    pop_n_elems(allargs+1);
    push_object(o);
  }
  s8 = (struct std8e_stor *)(sp[-1].u.object->storage+std8e_stor_offs);
  memset((s8->revtab = (p_wchar0 *)xalloc((hi-lo)*sizeof(p_wchar0))), 0,
	 (hi-lo)*sizeof(p_wchar0));
  s8->lo = lo;
  s8->hi = hi;
  s8->lowtrans = 0;
  return s8;
}

static struct std16e_stor *push_std_16bite(int args, int allargs, int lo, int hi)
{
  struct std16e_stor *s16;
  push_object(clone_object(std_16bite_program, args));
  if((allargs-=args)>0) {
    struct object *o = sp[-1].u.object;
    add_ref(o);
    pop_n_elems(allargs+1);
    push_object(o);
  }
  s16 = (struct std16e_stor *)(sp[-1].u.object->storage+std16e_stor_offs);
  memset((s16->revtab = (p_wchar1 *)xalloc((hi-lo)*sizeof(p_wchar1))), 0,
	 (hi-lo)*sizeof(p_wchar1));
  s16->lo = lo;
  s16->hi = hi;
  s16->lowtrans = 0;
  return s16;
}

static void f_rfc1345(INT32 args)
{
  extern struct charset_def charset_map[];
  extern int num_charset_def;
  struct pike_string *str;
  int lo=0, hi=num_charset_def-1;
  p_wchar1 *tabl;

  check_all_args("rfc1345()", args, BIT_STRING, BIT_INT|BIT_VOID,
		 BIT_STRING|BIT_VOID|BIT_INT, BIT_FUNCTION|BIT_VOID|BIT_INT,
		 0);

  str = sp[-args].u.string;

  if(str->size_shift>0)
    hi = -1;

  while(lo<=hi) {
    int c, mid = (lo+hi)>>1;
    if((c = strcmp((char *)STR0(str), charset_map[mid].name))==0) {
      struct program *p = NULL;

      if(args>1 && sp[1-args].type == T_INT && sp[1-args].u.integer != 0) {
	int lowtrans = 0, i, j, lo2=0, hi2=0, z;
	unsigned int c;

	switch(charset_map[mid].mode) {
	case MODE_94: lowtrans=lo=33; hi=126; break;
	case MODE_96: lowtrans=128; lo=160; hi=255; break;
	case MODE_9494: lowtrans=lo=lo2=33; hi=hi2=126; break;
	case MODE_9696: lowtrans=32; lo=lo2=160; hi=hi2=255; break;
	case MODE_BIG5: lowtrans=32; lo=0xa1; lo2=0x40; hi=0xf9; hi2=0xfe; break;
	default:
	  Pike_fatal("Internal error in rfc1345\n");
	}
	
	if(hi2) {
	  struct std16e_stor *s16;
	  s16 = push_std_16bite((args>2? args-2:0), args, lowtrans, 65536);
	  
	  s16->lowtrans = lowtrans;
	  s16->lo = lowtrans;
	  s16->hi = lowtrans;
	  
	  for(z=0, i=lo; i<=hi; i++, z+=(hi2-lo2+1))
	    for(j=lo2; j<=hi2; j++)
	      if((c=charset_map[mid].table[z+j-lo2])!=0xfffd && c>=s16->lo) {
		s16->revtab[c-s16->lo]=(i<<8)|j;
		if(c>=s16->hi)
		  s16->hi = c+1;
	      }
	} else {
	  struct std8e_stor *s8;
	  s8 = push_std_8bite((args>2? args-2:0), args, lowtrans, 65536);
	  
	  s8->lowtrans = lowtrans;
	  s8->lo = lowtrans;
	  s8->hi = lowtrans;
	  
	  for(i=lo; i<=hi; i++)
	    if((c=charset_map[mid].table[i-lo])!=0xfffd && c>=s8->lo) {
	      s8->revtab[c-s8->lo]=i;
	      if(c>=s8->hi)
		s8->hi = c+1;
	    }
	}
	return;
      }

      pop_n_elems(args);
      switch(charset_map[mid].mode) {
      case MODE_94: p = std_94_program; break;
      case MODE_96: p = std_96_program; break;
      case MODE_9494: p = std_9494_program; break;
      case MODE_9696: p = std_9696_program; break;
      case MODE_BIG5: p = std_big5_program; break;
      default:
	Pike_fatal("Internal error in rfc1345\n");
      }
      push_object(clone_object(p, 0));
      ((struct std_rfc_stor *)(sp[-1].u.object->storage+std_rfc_stor_offs))
	->table = charset_map[mid].table;
      return;
    }
    if(c<0)
      hi=mid-1;
    else
      lo=mid+1;
  }

  if(str->size_shift==0 &&
     (tabl = misc_charset_lookup((char *)STR0(str), &lo, &hi))!=NULL) {

    if(args>1 && sp[1-args].type == T_INT && sp[1-args].u.integer != 0) {
      struct std8e_stor *s8;
      int i;
      unsigned int c;

      s8 = push_std_8bite((args>2? args-2:0), args, lo, 65536);
      s8->lowtrans = lo;
      s8->lo = lo;
      s8->hi = lo;
      for(i=lo; i<=hi; i++)
	if((c=tabl[i-lo])!=0xfffd && c>=s8->lo) {
	  s8->revtab[c-lo]=i;
	  if(c>=s8->hi)
	    s8->hi = c+1;
	}
      return;
    }

    pop_n_elems(args);
    push_object(clone_object(std_8bit_program, 0));
    ((struct std_rfc_stor *)(sp[-1].u.object->storage+std_rfc_stor_offs))
      ->table = (UNICHAR *)tabl;
    ((struct std_misc_stor *)(sp[-1].u.object->storage+std_misc_stor_offs))
      ->lo = lo;
    ((struct std_misc_stor *)(sp[-1].u.object->storage+std_misc_stor_offs))
      ->hi = hi;
    return;    
  }

  pop_n_elems(args);
  push_int(0);
}

static ptrdiff_t feed_94(const p_wchar0 *p, ptrdiff_t l, struct std_cs_stor *s)
{
  UNICHAR const *table =
    ((struct std_rfc_stor *)(((char*)s)+std_rfc_stor_offs))->table;
  while(l--) {
    p_wchar0 x = *p++;
    if(x<=0x20 || x>=0x7f)
      string_builder_putchar(&s->strbuild, x);
    else
      string_builder_putchar(&s->strbuild, table[x-0x21]);
  }
  return 0;
}

static void f_feed_94(INT32 args)
{
  f_std_feed(args, feed_94);
}

static ptrdiff_t feed_96(const p_wchar0 *p, ptrdiff_t l, struct std_cs_stor *s)
{
  UNICHAR const *table =
    ((struct std_rfc_stor *)(((char*)s)+std_rfc_stor_offs))->table;
  while(l--) {
    p_wchar0 x = *p++;
    if(x<0xa0)
      string_builder_putchar(&s->strbuild, x);
    else
      string_builder_putchar(&s->strbuild, table[x-0xa0]);
  }
  return 0;
}

static void f_feed_96(INT32 args)
{
  f_std_feed(args, feed_96);
}

static ptrdiff_t feed_9494(const p_wchar0 *p, ptrdiff_t l,
			   struct std_cs_stor *s)
{
  UNICHAR const *table =
    ((struct std_rfc_stor *)(((char*)s)+std_rfc_stor_offs))->table;
  while(l--) {
    p_wchar0 y, x = (*p++)&0x7f;
    if(x<=0x20 || x>=0x7f)
      string_builder_putchar(&s->strbuild, x);
    else if(l==0)
      return 1;
    else if((y=(*p)&0x7f)>0x20 && y<0x7f) {
      --l;
      p++;
      string_builder_putchar(&s->strbuild, table[(x-0x21)*94+(y-0x21)]);
    } else {
      string_builder_putchar(&s->strbuild, x);
    }
  }
  return 0;
}

static void f_feed_9494(INT32 args)
{
  f_std_feed(args, feed_9494);
}

static ptrdiff_t feed_9696(const p_wchar0 *p, ptrdiff_t l,
			   struct std_cs_stor *s)
{
  UNICHAR const *table =
    ((struct std_rfc_stor *)(((char*)s)+std_rfc_stor_offs))->table;
  while(l--) {
    p_wchar0 y, x = (*p++)&0x7f;
    if(x<0x20)
      string_builder_putchar(&s->strbuild, x);
    else if(l==0)
      return 1;
    else if((y=(*p)&0x7f)>=0x20) {
      --l;
      p++;
      string_builder_putchar(&s->strbuild, table[(x-0x20)*96+(y-0x20)]);
    } else {
      string_builder_putchar(&s->strbuild, x);
    }
  }
  return 0;
}

static void f_feed_9696(INT32 args)
{
  f_std_feed(args, feed_9696);
}

static ptrdiff_t feed_big5(const p_wchar0 *p, ptrdiff_t l, struct std_cs_stor *s)
{
  UNICHAR const *table =
    ((struct std_rfc_stor *)(((char*)s)+std_rfc_stor_offs))->table;
  while(l--) {
    p_wchar0 y, x = (*p++);
    if(x<0xa1 || x>0xf9 )
      string_builder_putchar(&s->strbuild, x);
    else if(l==0)
      return 1;
    else if((y=(*p))>=0x40 && y<=0xfe ) {
      --l;
      p++;
      string_builder_putchar(&s->strbuild, table[(x-0xa1 )*(0xfe -0x40 +1)+(y-0x40 )]);
    } else {
      string_builder_putchar(&s->strbuild, x);
    }
  }
  return 0;
}

static void f_feed_big5(INT32 args)
{
  f_std_feed(args, feed_big5);
}

static ptrdiff_t feed_8bit(const p_wchar0 *p, ptrdiff_t l,
			   struct std_cs_stor *s)
{
  UNICHAR const *table =
    ((struct std_rfc_stor *)(((char*)s)+std_rfc_stor_offs))->table;
  struct std_misc_stor *misc =
    ((struct std_misc_stor *)(((char*)s)+std_misc_stor_offs));
  int lo = misc->lo, hi = misc->hi;

  while(l--) {
    p_wchar0 x = *p++;
    if(x<lo || (x>0x7f && hi<=0x7f))
      string_builder_putchar(&s->strbuild, x);
    else if(x>hi)
      string_builder_putchar(&s->strbuild, DEFCHAR);
    else
      string_builder_putchar(&s->strbuild, table[x-lo]);
  }
  return 0;
}

static void f_feed_8bit(INT32 args)
{
  f_std_feed(args, feed_8bit);
}


static void feed_utf8e(struct std_cs_stor *cs, struct string_builder *sb,
		       struct pike_string *str, struct pike_string *rep,
		       struct svalue *repcb)
{
  ptrdiff_t l = str->len;

  switch(str->size_shift) {
  case 0:
    {
      p_wchar0 c, *p = STR0(str);
      while(l--)
	if((c=*p++)<=0x7f)
	  string_builder_putchar(sb, c);
        else {
	  string_builder_putchar(sb, 0xc0|(c>>6));
	  string_builder_putchar(sb, 0x80|(c&0x3f));	
	}
    }
    break;
  case 1:
    {
      p_wchar1 c, *p = STR1(str);
      while(l--)
	if((c=*p++)<=0x7f)
	  string_builder_putchar(sb, c);
	else if(c<=0x7ff) {
	  string_builder_putchar(sb, 0xc0|(c>>6));
	  string_builder_putchar(sb, 0x80|(c&0x3f));	
	} else if (c <= 0xd7ff || c >= 0xe000) {
      	  string_builder_putchar(sb, 0xe0|(c>>12));
	  string_builder_putchar(sb, 0x80|((c>>6)&0x3f));
	  string_builder_putchar(sb, 0x80|(c&0x3f));	
	} else
	  REPLACE_CHAR(c, feed_utf8e, cs, p - STR1(str) - 1);
    }
    break;
  case 2:
    {
      p_wchar2 c, *p = STR2(str);
      while(l--) {
	if((c=*p++)<=0x7f) {
	  string_builder_putchar(sb, c);
	  continue;
	}
	else if(c<=0x7ff) {
	  string_builder_putchar(sb, 0xc0|(c>>6));
	  string_builder_putchar(sb, 0x80|(c&0x3f));	
	  continue;
	} else if(c<=0xffff) {
	  if (c <= 0xd7ff || c >= 0xe000) {
	    string_builder_putchar(sb, 0xe0|(c>>12));
	    string_builder_putchar(sb, 0x80|((c>>6)&0x3f));
	    string_builder_putchar(sb, 0x80|(c&0x3f));
	    continue;
	  }
	} else if(c<=0x10ffff) {
	  string_builder_putchar(sb, 0xf0|(c>>18));
	  string_builder_putchar(sb, 0x80|((c>>12)&0x3f));
	  string_builder_putchar(sb, 0x80|((c>>6)&0x3f));
	  string_builder_putchar(sb, 0x80|(c&0x3f));	
	  continue;
	}
	REPLACE_CHAR(c, feed_utf8e, cs, p - STR2(str) - 1);
      }
    }
    break;
  default:
    Pike_fatal("Illegal shift size!\n");
  }
}

static void f_feed_utf8e(INT32 args)
{
  struct pike_string *str;
  struct std_cs_stor *cs = (struct std_cs_stor *)fp->current_storage;

  get_all_args("feed()", args, "%W", &str);

  feed_utf8e(cs, &cs->strbuild, str, cs->replace, MKREPCB(cs->repcb));

  pop_n_elems(args);
  push_object(this_object());
}

/* From http://www.unicode.org/reports/tr16/
 *   Table 2: Byte map from I8-sequence to UTF-EBCDIC byte sequence
 */
static const unsigned char i8_to_utf_ebcdic_conv[] = {
  0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f,
  0x16, 0x05, 0x15, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26,
  0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f,
  0x40, 0x5a, 0x7f, 0x7b, 0x5b, 0x6c, 0x50, 0x7d,
  0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
  0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,
  0x7c, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
  0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
  0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
  0xe7, 0xe8, 0xe9, 0xad, 0xe0, 0xbd, 0x5f, 0x6d,
  0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
  0xa7, 0xa8, 0xa9, 0xc0, 0x4f, 0xd0, 0xa1, 0x07,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x06, 0x17,
  0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x09, 0x0a, 0x1b,
  0x30, 0x31, 0x1a, 0x33, 0x34, 0x35, 0x36, 0x08,
  0x38, 0x39, 0x3a, 0x3b, 0x04, 0x14, 0x3e, 0xff,
  0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
  0x57, 0x58, 0x59, 0x62, 0x63, 0x64, 0x65, 0x66,
  0x67, 0x68, 0x69, 0x6a, 0x70, 0x71, 0x72, 0x73,
  0x74, 0x75, 0x76, 0x77, 0x78, 0x80, 0x8a, 0x8b,
  0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x9a, 0x9b, 0x9c,
  0x9d, 0x9e, 0x9f, 0xa0, 0xaa, 0xab, 0xac, 0xae,
  0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
  0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbe, 0xbf,
  0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xda, 0xdb,
  0xdc, 0xdd, 0xde, 0xdf, 0xe1, 0xea, 0xeb, 0xec,
  0xed, 0xee, 0xef, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
};

static void feed_utf_ebcdice(struct std_cs_stor *cs, struct string_builder *sb,
			     struct pike_string *str, struct pike_string *rep,
			     struct svalue *repcb)
{
  ptrdiff_t l = str->len;

  switch(str->size_shift) {
  case 0:
    {
      p_wchar0 c, *p = STR0(str);
      while(l--)
	if((c=*p++)<=0x9f)
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[c]);
        else {
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xc0|(c>>5)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|(c&0x1f)]);
	}
    }
    break;
  case 1:
    {
      p_wchar1 c, *p = STR1(str);
      while(l--)
	if((c=*p++)<=0x9f)
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[c]);
	else if(c<=0x3ff) {
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xc0|(c>>5)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|(c&0x1f)]);
	} else if (c <= 0x3fff) {
      	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xe0|(c>>10)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|((c>>5)&0x1f)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|(c&0x1f)]);
	} else if (c <= 0xd7ff || c >= 0xe000) {
      	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xf0|(c>>15)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|((c>>10)&0x1f)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|((c>>5)&0x1f)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|(c&0x1f)]);
	} else
	  REPLACE_CHAR(c, feed_utf_ebcdice, cs, p - STR1(str) - 1);
    }
    break;
  case 2:
    {
      p_wchar2 c, *p = STR2(str);
      while(l--) {
	if((c=*p++)<=0x9f) {
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[c]);
	  continue;
	}
	else if(c<=0x3ff) {
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xc0|(c>>5)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|(c&0x1f)]);
	  continue;
	} else if(c<=0x3fff) {
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xe0|(c>>10)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|((c>>5)&0x1f)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|(c&0x1f)]);
	  continue;
	} else if(c<=0x3ffff) {
	  if (c <= 0xd7ff || c >= 0xe000) {
	    string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xe0|(c>>15)]);
	    string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|((c>>10)&0x1f)]);
	    string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|((c>>5)&0x1f)]);
	    string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|(c&0x1f)]);
	    continue;
	  }
	} else if(c<=0x10ffff) {
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xf0|(c>>20)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|((c>>15)&0x1f)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|((c>>10)&0x1f)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|((c>>5)&0x1f)]);
	  string_builder_putchar(sb, i8_to_utf_ebcdic_conv[0xa0|(c&0x1f)]);
	  continue;
	}
	REPLACE_CHAR(c, feed_utf_ebcdice, cs, p - STR2(str) - 1);
      }
    }
    break;
  default:
    Pike_fatal("Illegal shift size!\n");
  }
}

static void f_feed_utf_ebcdice(INT32 args)
{
  struct pike_string *str;
  struct std_cs_stor *cs = (struct std_cs_stor *)fp->current_storage;

  get_all_args("feed()", args, "%W", &str);

  feed_utf_ebcdice(cs, &cs->strbuild, str, cs->replace, MKREPCB(cs->repcb));

  pop_n_elems(args);
  push_object(this_object());
}

static void feed_utf7_5e(struct std_cs_stor *cs, struct string_builder *sb,
			 struct pike_string *str, struct pike_string *rep,
			 struct svalue *repcb)
{
  ptrdiff_t l = str->len;

  switch(str->size_shift) {
  case 0:
    {
      p_wchar0 c, *p = STR0(str);
      while(l--)
	if((c=*p++)<=0x7f)
	  string_builder_putchar(sb, c);
        else {
	  string_builder_putchar(sb, 0xa0|(c>>6));
	  string_builder_putchar(sb, 0xc0|(c&0x3f));	
	}
    }
    break;
  case 1:
    {
      p_wchar1 c, *p = STR1(str);
      while(l--)
	if((c=*p++)<=0x7f)
	  string_builder_putchar(sb, c);
	else if(c<=0x3ff) {
	  string_builder_putchar(sb, 0xa0|(c>>6));
	  string_builder_putchar(sb, 0xc0|(c&0x3f));	
	} else {
      	  string_builder_putchar(sb, 0xb0|(c>>12));
	  string_builder_putchar(sb, 0xc0|((c>>6)&0x3f));
	  string_builder_putchar(sb, 0xc0|(c&0x3f));	
	}
    }
    break;
  case 2:
    {
      p_wchar2 c, *p = STR2(str);
      while(l--)
	if((c=*p++)<=0x7f)
	  string_builder_putchar(sb, c);
	else if(c<=0x3ff) {
	  string_builder_putchar(sb, 0xa0|(c>>6));
	  string_builder_putchar(sb, 0xc0|(c&0x3f));	
	} else if(c<=0xffff) {
	  string_builder_putchar(sb, 0xb0|(c>>12));
	  string_builder_putchar(sb, 0xc0|((c>>6)&0x3f));
	  string_builder_putchar(sb, 0xc0|(c&0x3f));	
	} else
	  REPLACE_CHAR(c, feed_utf7_5e, cs, p - STR2(str) - 1);
      /* FIXME: Encode using surrogates? */
    }
    break;
  default:
    Pike_fatal("Illegal shift size!\n");
  }
}

static void f_feed_utf7_5e(INT32 args)
{
  struct pike_string *str;
  struct std_cs_stor *cs = (struct std_cs_stor *)fp->current_storage;

  get_all_args("feed()", args, "%W", &str);

  feed_utf7_5e(cs, &cs->strbuild, str, cs->replace, MKREPCB(cs->repcb));

  pop_n_elems(args);
  push_object(this_object());
}

static void feed_utf7e(struct utf7_stor *u7, struct string_builder *sb,
		       struct pike_string *str, struct pike_string *rep,
		       struct svalue *repcb)
{
  ptrdiff_t l = str->len;
  INT32 dat = u7->dat;
  int shift = u7->shift, datbit = u7->datbit;

  switch(str->size_shift) {
  case 0:
    {
      p_wchar0 c, *p = STR0(str);
      while(l--)
	if(((c=*p++)>=32 && c<=125 && c!=43 && c!=92)
	   || c==9 || c==10 || c==13) {
	  if(shift) {
	    if(datbit) {
	      string_builder_putchar(sb, fwd64t[dat<<(6-datbit)]);
	      dat=0;
	      datbit=0;
	    }
	    if(c>='+' && c<='z' && rev64t[c-'+']>=0)
	      string_builder_putchar(sb, '-');
	    shift = 0;  
	  }
	  string_builder_putchar(sb, c);
	} else if(c==43 && !shift) {
	  string_builder_putchar(sb, '+');
	  string_builder_putchar(sb, '-');
	} else {
	  if(!shift) {
	    string_builder_putchar(sb, '+');
	    shift = 1;
	  }
	  dat=(dat<<16)|c;
	  string_builder_putchar(sb, fwd64t[dat>>(datbit+10)]);
	  string_builder_putchar(sb, fwd64t[(dat>>(datbit+4))&0x3f]);
	  if((datbit+=4)>=6) {
	    string_builder_putchar(sb, fwd64t[(dat>>(datbit-6))&0x3f]);
	    datbit-=6;
	  }
	  dat&=(1<<datbit)-1;
	}
    }
    break;
  case 1:
    {
      p_wchar1 c, *p = STR1(str);
      while(l--)
	if(((c=*p++)>=32 && c<=125 && c!=43 && c!=92)
	   || c==9 || c==10 || c==13) {
	  if(shift) {
	    if(datbit) {
	      string_builder_putchar(sb, fwd64t[dat<<(6-datbit)]);
	      dat=0;
	      datbit=0;
	    }
	    if(c>='+' && c<='z' && rev64t[c-'+']>=0)
	      string_builder_putchar(sb, '-');
	    shift = 0;  
	  }
	  string_builder_putchar(sb, c);
	} else if(c==43 && !shift) {
	  string_builder_putchar(sb, '+');
	  string_builder_putchar(sb, '-');
	} else {
	  if(!shift) {
	    string_builder_putchar(sb, '+');
	    shift = 1;
	  }
	  dat=(dat<<16)|c;
	  string_builder_putchar(sb, fwd64t[dat>>(datbit+10)]);
	  string_builder_putchar(sb, fwd64t[(dat>>(datbit+4))&0x3f]);
	  if((datbit+=4)>=6) {
	    string_builder_putchar(sb, fwd64t[(dat>>(datbit-6))&0x3f]);
	    datbit-=6;
	  }
	  dat&=(1<<datbit)-1;
	}
    }
    break;
  case 2:
    {
      p_wchar2 c, *p = STR2(str);
      while(l--)
	if(((c=*p++)>=32 && c<=125 && c!=43 && c!=92)
	   || c==9 || c==10 || c==13) {
	  if(shift) {
	    if(datbit) {
	      string_builder_putchar(sb, fwd64t[dat<<(6-datbit)]);
	      dat=0;
	      datbit=0;
	    }
	    if(c>='+' && c<='z' && rev64t[c-'+']>=0)
	      string_builder_putchar(sb, '-');
	    shift = 0;  
	  }
	  string_builder_putchar(sb, c);
	} else if(c==43 && !shift) {
	  string_builder_putchar(sb, '+');
	  string_builder_putchar(sb, '-');
	} else if(c>0x10ffff) {
	  u7->dat = dat;
	  u7->shift = shift;
	  u7->datbit = datbit;
	  REPLACE_CHAR(c, feed_utf7e, u7, p - STR2(str) - 1);
	  dat = u7->dat;
	  shift = u7->shift;
	  datbit = u7->datbit;
	} else {
	  if(!shift) {
	    string_builder_putchar(sb, '+');
	    shift = 1;
	  }
	  if(c>0xffff) {
	    dat=(dat<<16)|(0xd800+(c>>10)-64);
	    string_builder_putchar(sb, fwd64t[dat>>(datbit+10)]);
	    string_builder_putchar(sb, fwd64t[(dat>>(datbit+4))&0x3f]);
	    if((datbit+=4)>=6) {
	      string_builder_putchar(sb, fwd64t[(dat>>(datbit-6))&0x3f]);
	      datbit-=6;
	    }
	    dat&=(1<<datbit)-1;
	    c=0xdc00+(c&1023);
	  }
	  dat=(dat<<16)|c;
	  string_builder_putchar(sb, fwd64t[dat>>(datbit+10)]);
	  string_builder_putchar(sb, fwd64t[(dat>>(datbit+4))&0x3f]);
	  if((datbit+=4)>=6) {
	    string_builder_putchar(sb, fwd64t[(dat>>(datbit-6))&0x3f]);
	    datbit-=6;
	  }
	  dat&=(1<<datbit)-1;
	}
    }
    break;
  default:
    Pike_fatal("Illegal shift size!\n");
  }

  u7->dat = dat;
  u7->shift = shift;
  u7->datbit = datbit;
}

static void f_feed_utf7e(INT32 args)
{
  struct pike_string *str;
  struct std_cs_stor *cs = (struct std_cs_stor *)fp->current_storage;

  get_all_args("feed()", args, "%W", &str);

  feed_utf7e((struct utf7_stor *)(((char*)fp->current_storage)+utf7_stor_offs),
	     &cs->strbuild, str, cs->replace, MKREPCB(cs->repcb));

  pop_n_elems(args);
  push_object(this_object());
}

static void f_drain_utf7e(INT32 args)
{
  struct std_cs_stor *cs = (struct std_cs_stor *)fp->current_storage;
  struct utf7_stor *u7 =
    (struct utf7_stor *)(fp->current_storage+utf7_stor_offs);

  if(u7->shift) {
    if(u7->datbit) {
      string_builder_putchar(&cs->strbuild, fwd64t[u7->dat<<(6-u7->datbit)]);
      u7->dat=0;
      u7->datbit=0;
    }
    string_builder_putchar(&cs->strbuild, '-');
    u7->shift = 0;  
  }
  f_drain(args);
}

static void std_8bite_init_stor(struct object *o)
{
  struct std8e_stor *s8 =
    (struct std8e_stor *)(fp->current_storage+std8e_stor_offs);

  s8->revtab = NULL;
  s8->lowtrans = 32;
  s8->lo = 0;
  s8->hi = 0;
}

static void std_8bite_exit_stor(struct object *o)
{
  struct std8e_stor *s8 =
    (struct std8e_stor *)(fp->current_storage+std8e_stor_offs);

  if(s8->revtab != NULL)
    free(s8->revtab);
}

static void feed_std8e(struct std8e_stor *s8, struct string_builder *sb,
		       struct pike_string *str, struct pike_string *rep,
		       struct svalue *repcb)
{
  ptrdiff_t l = str->len;
  p_wchar0 *tab = s8->revtab;
  unsigned int lowtrans = s8->lowtrans, lo = s8->lo, hi = s8->hi;
  p_wchar0 ch;

  switch(str->size_shift) {
  case 0:
    {
      p_wchar0 c, *p = STR0(str);
      while(l--)
	if((c=*p++)<lowtrans)
	  string_builder_putchar(sb, c);
	else if(c>=lo && c<hi && (ch=tab[c-lo])!=0)
	  string_builder_putchar(sb, ch);
	else
	  REPLACE_CHAR(c, feed_std8e, s8, p - STR0(str) - 1);
    }
    break;
  case 1:
    {
      p_wchar1 c, *p = STR1(str);
      while(l--)
	if((c=*p++)<lowtrans)
	  string_builder_putchar(sb, c);
	else if(c>=lo && c<hi && (ch=tab[c-lo])!=0)
	  string_builder_putchar(sb, ch);
	else
	  REPLACE_CHAR(c, feed_std8e, s8, p - STR1(str) - 1);
    }
    break;
  case 2:
    {
      p_wchar2 c, *p = STR2(str);
      while(l--)
	if((c=*p++)<lowtrans)
	  string_builder_putchar(sb, c);
	else if(c>=lo && c<hi && (ch=tab[c-lo])!=0)
	  string_builder_putchar(sb, ch);
	else
	  REPLACE_CHAR(c, feed_std8e, s8, p - STR2(str) - 1);
    }
    break;
  default:
    Pike_fatal("Illegal shift size!\n");
  }
}

static void f_feed_std8e(INT32 args)
{
  struct pike_string *str;
  struct std_cs_stor *cs = (struct std_cs_stor *)fp->current_storage;

  get_all_args("feed()", args, "%W", &str);

  feed_std8e((struct std8e_stor *)(((char*)fp->current_storage)+
				   std8e_stor_offs),
	     &cs->strbuild, str, cs->replace, MKREPCB(cs->repcb));

  pop_n_elems(args);
  push_object(this_object());
}

static void std_16bite_init_stor(struct object *o)
{
  struct std16e_stor *s16 =
    (struct std16e_stor *)(fp->current_storage+std16e_stor_offs);

  s16->revtab = NULL;
  s16->lowtrans = 32;
  s16->lo = 0;
  s16->hi = 0;
  s16->sshift = 0;
}

static void std_16bite_exit_stor(struct object *o)
{
  struct std16e_stor *s16 =
    (struct std16e_stor *)(fp->current_storage+std16e_stor_offs);

  if(s16->revtab != NULL)
    free(s16->revtab);
}

static void feed_std16e(struct std16e_stor *s16, struct string_builder *sb,
			struct pike_string *str, struct pike_string *rep,
			struct svalue *repcb)
{
  ptrdiff_t l = str->len;
  p_wchar1 *tab = s16->revtab;
  unsigned int lowtrans = s16->lowtrans, lo = s16->lo, hi = s16->hi;
  int sshift = s16->sshift;
  p_wchar1 ch;

  switch(str->size_shift) {
  case 0:
    {
      p_wchar0 c, *p = STR0(str);
      while(l--)
	if((c=*p++)<lowtrans)
	  string_builder_putchar(sb, c);
	else if(c>=lo && c<hi && (ch=tab[c-lo])!=0) {
	  if(sshift && !(ch & 0x80)) {
	    ch |= 0x80;
	    string_builder_putchar(sb, (ch > 0xff? 0x8f : 0x8e));
	  }
	  if(ch > 0xff)
	    string_builder_putchar(sb, (ch>>8)&0xff);
	  string_builder_putchar(sb, ch&0xff);
	} else
	  REPLACE_CHAR(c, feed_std16e, s16, p - STR0(str) - 1);
    }
    break;
  case 1:
    {
      p_wchar1 c, *p = STR1(str);
      while(l--)
	if((c=*p++)<lowtrans)
	  string_builder_putchar(sb, c);
	else if(c>=lo && c<hi && (ch=tab[c-lo])!=0) {
	  if(sshift && !(ch & 0x80)) {
	    ch |= 0x80;
	    string_builder_putchar(sb, (ch > 0xff? 0x8f : 0x8e));
	  }
	  if(ch > 0xff)
	    string_builder_putchar(sb, (ch>>8)&0xff);
	  string_builder_putchar(sb, ch&0xff);
	} else
	  REPLACE_CHAR(c, feed_std16e, s16, p - STR1(str) - 1);
    }
    break;
  case 2:
    {
      p_wchar2 c, *p = STR2(str);
      while(l--)
	if((c=*p++)<lowtrans)
	  string_builder_putchar(sb, c);
	else if(c>=lo && c<hi && (ch=tab[c-lo])!=0) {
	  if(sshift && !(ch & 0x80)) {
	    ch |= 0x80;
	    string_builder_putchar(sb, (ch > 0xff? 0x8f : 0x8e));
	  }
	  if(ch > 0xff)
	    string_builder_putchar(sb, (ch>>8)&0xff);
	  string_builder_putchar(sb, ch&0xff);
	} else
	  REPLACE_CHAR(c, feed_std16e, s16, p - STR2(str) - 1);
    }
    break;
  default:
    Pike_fatal("Illegal shift size!\n");
  }
}

static void f_feed_std16e(INT32 args)
{
  struct pike_string *str;
  struct std_cs_stor *cs = (struct std_cs_stor *)fp->current_storage;

  get_all_args("feed()", args, "%W", &str);

  feed_std16e((struct std16e_stor *)(((char*)fp->current_storage)+
				     std16e_stor_offs),
	      &cs->strbuild, str, cs->replace, MKREPCB(cs->repcb));

  pop_n_elems(args);
  push_object(this_object());
}


PIKE_MODULE_INIT
{
  int i;
  struct svalue prog;
  extern void iso2022_init(void);

  iso2022_init();

  start_new_program();
  ADD_STORAGE(struct std_cs_stor);
  /* function(:string) */
  ADD_FUNCTION("drain", f_drain,tFunc(tNone,tStr), 0);
  /* function(:object) */
  ADD_FUNCTION("clear", f_clear,tFunc(tNone,tObj), 0);
  /* function(string|void,function(string:string)|void:void) */
  ADD_FUNCTION("create", f_create,tFunc(tOr(tStr,tVoid) tOr(tFunc(tStr,tStr),tVoid),tVoid), 0);
  /* function(function(string:string):void) */
  ADD_FUNCTION("set_replacement_callback", f_set_repcb,tFunc(tFunc(tStr,tStr),tVoid), 0);
  map_variable("_repcb", "function(string:string)", ID_STATIC,
	       OFFSETOF(std_cs_stor, repcb), T_MIXED);
  set_init_callback(init_stor);
  set_exit_callback(exit_stor);
  std_cs_program = end_program();

  prog.type = T_PROGRAM;
  prog.subtype = 0;
  prog.u.program = std_cs_program;

  memset(rev64t, -1, sizeof(rev64t));
  for(i=0; i<64; i++)
    rev64t[fwd64t[i]-'+']=i;

  start_new_program();
  do_inherit(&prog, 0, NULL);
  utf7_stor_offs = ADD_STORAGE(struct utf7_stor);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_utf7,tFunc(tStr,tObj), 0);
  /* function(:object) */
  ADD_FUNCTION("clear", f_clear_utf7,tFunc(tNone,tObj), 0);
  set_init_callback(utf7_init_stor);
  add_program_constant("UTF7dec", utf7_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_utf8,tFunc(tStr,tObj), 0);
  add_program_constant("UTF8dec", utf8_program = end_program(), ID_STATIC|ID_NOMASK);

  prog.u.program = utf7_program;
  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_utf7e,tFunc(tStr,tObj), 0);
  /* function(:string) */
  ADD_FUNCTION("drain", f_drain_utf7e,tFunc(tNone,tStr), 0);
  add_program_constant("UTF7enc", utf7e_program = end_program(), ID_STATIC|ID_NOMASK);
  prog.u.program = std_cs_program;

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_utf8e,tFunc(tStr,tObj), 0);
  add_program_constant("UTF8enc", utf8e_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_utf_ebcdic,tFunc(tStr,tObj), 0);
  add_program_constant("UTF_EBCDICdec", utf_ebcdic_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_utf_ebcdice,tFunc(tStr,tObj), 0);
  add_program_constant("UTF_EBCDICenc", utf_ebcdice_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_utf7_5,tFunc(tStr,tObj), 0);
  add_program_constant("UTF7_5dec", utf7_5_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_utf7_5e,tFunc(tStr,tObj), 0);
  add_program_constant("UTF7_5enc", utf7_5e_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  euc_stor_offs = ADD_STORAGE(struct euc_stor);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_euc,tFunc(tStr,tObj), 0);
  /* function(string:) */
  ADD_FUNCTION("create", f_create_euc,tFunc(tStr,tVoid), ID_STATIC);
  add_program_constant("EUCDec", euc_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  multichar_stor_offs = ADD_STORAGE(struct multichar_stor);
  ADD_FUNCTION("create", f_create_multichar,tFunc(tStr,tVoid), ID_STATIC);
  ADD_FUNCTION("feed", f_feed_multichar,tFunc(tStr,tObj), 0);
  add_program_constant("MulticharDec", multichar_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_gb18030e,tFunc(tStr,tObj), 0);
  add_program_constant("GB18030Enc", gb18030e_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_gbke,tFunc(tStr,tObj), 0);
  add_program_constant("GBKenc", gbke_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_sjis,tFunc(tStr,tObj), 0);
  add_program_constant("ShiftJisDec", sjis_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  std8e_stor_offs = ADD_STORAGE(struct std8e_stor);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_std8e,tFunc(tStr,tObj), 0);
  set_init_callback(std_8bite_init_stor);
  set_exit_callback(std_8bite_exit_stor);
  std_8bite_program = end_program();

  start_new_program();
  do_inherit(&prog, 0, NULL);
  std16e_stor_offs = ADD_STORAGE(struct std16e_stor);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_std16e,tFunc(tStr,tObj), 0);
  set_init_callback(std_16bite_init_stor);
  set_exit_callback(std_16bite_exit_stor);
  std_16bite_program = end_program();

  start_new_program();
  do_inherit(&prog, 0, NULL);
  std_rfc_stor_offs = ADD_STORAGE(struct std_rfc_stor);
  ADD_FUNCTION("drain", f_drain_rfc1345, tFunc(tNone,tStr), 0);
  std_rfc_program = end_program();

  prog.u.program = std_16bite_program;

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string,string|void,function(string:string)|void:void) */
  ADD_FUNCTION("create", f_create_euce,tFunc(tStr tOr(tStr,tVoid) tOr(tFunc(tStr,tStr),tVoid),tVoid), 0);
  add_program_constant("EUCEnc", euce_program = end_program(), ID_STATIC|ID_NOMASK);

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string|void,function(string:string)|void:void) */
  ADD_FUNCTION("create", f_create_sjise,tFunc(tOr(tStr,tVoid) tOr(tFunc(tStr,tStr),tVoid),tVoid), 0);
  add_program_constant("ShiftJisEnc", sjise_program = end_program(), ID_STATIC|ID_NOMASK);

  prog.u.program = std_rfc_program;

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_94,tFunc(tStr,tObj), 0);
  std_94_program = end_program();

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_96,tFunc(tStr,tObj), 0);
  std_96_program = end_program();

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_9494,tFunc(tStr,tObj), 0);
  std_9494_program = end_program();

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_9696,tFunc(tStr,tObj), 0);
  std_9696_program = end_program();

  start_new_program();
  do_inherit(&prog, 0, NULL);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_big5,tFunc(tStr,tObj), 0);
  std_big5_program = end_program();

  start_new_program();
  do_inherit(&prog, 0, NULL);
  std_misc_stor_offs = ADD_STORAGE(struct std_misc_stor);
  /* function(string:object) */
  ADD_FUNCTION("feed", f_feed_8bit,tFunc(tStr,tObj), 0);
  std_8bit_program = end_program();

  add_function_constant("rfc1345", f_rfc1345,
			"function(string,int|void,string|void,"
			"function(string:string)|void:object)", 0);
}

PIKE_MODULE_EXIT
{
  extern void iso2022_exit(void);

  if(utf7e_program != NULL)
    free_program(utf7e_program);

  if(utf8e_program != NULL)
    free_program(utf8e_program);

  if(utf7_program != NULL)
    free_program(utf7_program);

  if(utf8_program != NULL)
    free_program(utf8_program);

  if(utf_ebcdic_program != NULL)
    free_program(utf_ebcdic_program);

  if(utf_ebcdice_program != NULL)
    free_program(utf_ebcdice_program);

  if(utf7_5_program != NULL)
    free_program(utf7_5_program);

  if(utf7_5e_program != NULL)
    free_program(utf7_5e_program);

  if(euc_program != NULL)
    free_program(euc_program);

  if(sjis_program != NULL)
    free_program(sjis_program);

  if(euce_program != NULL)
    free_program(euce_program);

  if(sjise_program != NULL)
    free_program(sjise_program);

  if(std_94_program != NULL)
    free_program(std_94_program);

  if(std_96_program != NULL)
    free_program(std_96_program);

  if(std_9494_program != NULL)
    free_program(std_9494_program);

  if(std_9696_program != NULL)
    free_program(std_9696_program);
  
  if(std_big5_program != NULL)
    free_program(std_big5_program);

  if(std_8bit_program != NULL)
    free_program(std_8bit_program);

  if(std_8bite_program != NULL)
    free_program(std_8bite_program);

  if(std_16bite_program != NULL)
    free_program(std_16bite_program);

  if(std_rfc_program != NULL)
    free_program(std_rfc_program);

  if(std_cs_program != NULL)
    free_program(std_cs_program);

  if(gb18030e_program != NULL)
    free_program(gb18030e_program);

  if(gbke_program != NULL)
    free_program(gbke_program);

  if(multichar_program != NULL)
    free_program(multichar_program);

  iso2022_exit();
}
