/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: stralloc.h,v 1.99 2006/07/05 02:22:43 mast Exp $
*/

#ifndef STRALLOC_H
#define STRALLOC_H
#include "global.h"

#include <stdarg.h>

#include "pike_macros.h"
#include "block_alloc_h.h"

#define STRINGS_ARE_SHARED

#ifndef STRUCT_PIKE_STRING_DECLARED
#define STRUCT_PIKE_STRING_DECLARED
#endif

#define PIKE_STRING_CONTENTS						\
  INT32 refs;								\
  INT16 flags;								\
  INT16 size_shift; /* 14 bit waste, but good for alignment... */	\
  ptrdiff_t len; /* Not counting terminating NUL. */			\
  size_t hval;								\
  struct pike_string *next 

struct pike_string
{
  PIKE_STRING_CONTENTS;
  char str[1];			/* NUL terminated. */
};

struct string_builder
{
  struct pike_string *s;
  ptrdiff_t malloced;
  INT32 known_shift;
};

/* Flags used in pike_string->flags. */
#define STRING_NOT_HASHED	1	/* Hash value is invalid. */
#define STRING_NOT_SHARED	2	/* String not shared. */
#define STRING_IS_SHORT		4	/* String is blockalloced. */

/* Flags used by string_builder_append_integer() */
#define APPEND_SIGNED		1	/* Value is signed */
/* Note: The follwoing are NOT true flags. */
#define APPEND_WIDTH_HALF	2	/* h-flag. */
#define APPEND_WIDTH_LONG	4	/* l-flag. */
#define APPEND_WIDTH_LONG_LONG	6	/* ll-flag. */
#define APPEND_WIDTH_MASK	6	/* Corresponding mask. */
/* More real flags here. */
#define APPEND_POSITIVE		8	/* Sign positive values too. */
#define APPEND_UPPER_CASE	16	/* Use upper case hex. */
#define APPEND_ZERO_PAD		32	/* Zero pad. */
#define APPEND_LEFT		64	/* Left align. */

#if SIZEOF_CHAR_P == SIZEOF_INT
#define APPEND_WIDTH_PTR	0
#elif SIZEOF_CHAR_P == SIZEOF_LONG
#define APPEND_WIDTH_PTR	APPEND_WIDTH_LONG
#elif SIZEOF_CHAR_P == SIZEOF_LONG_LONG
#define APPEND_WIDTH_PTR	APPEND_WIDTH_LONG_LONG
#else
#error "Unknown way to read pointer-wide integers."
#endif

/* Flags used by string_builder_quote_string() */
#define QUOTE_NO_STRING_CONCAT	1	/* Don't use string concat in output */
#define QUOTE_BREAK_AT_LF	2	/* Break after linefeed */

#ifdef PIKE_DEBUG
PMOD_EXPORT struct pike_string *debug_findstring(const struct pike_string *foo);
#endif

#define free_string(s) do{ \
    struct pike_string *_=(s); \
    debug_malloc_touch(_); \
    if(sub_ref(_)<=0) \
      really_free_string(_); \
  } while(0)

#define my_hash_string(X) PTR_TO_INT(X)
#define is_same_string(X,Y) ((X)==(Y))
#define string_has_null(X) (strlen((X)->str)!=(size_t)(X)->len)

#ifdef PIKE_DEBUG
#define STR0(X) ((p_wchar0 *)debug_check_size_shift((X),0)->str)
#define STR1(X) ((p_wchar1 *)debug_check_size_shift((X),1)->str)
#define STR2(X) ((p_wchar2 *)debug_check_size_shift((X),2)->str)
#else
#define STR0(X) ((p_wchar0 *)(X)->str)
#define STR1(X) ((p_wchar1 *)(X)->str)
#define STR2(X) ((p_wchar2 *)(X)->str)
#endif

#define INDEX_CHARP(PTR,IND,SHIFT) \
  ((SHIFT)==0?((p_wchar0 *)(PTR))[(IND)]:(SHIFT)==1?((p_wchar1 *)(PTR))[(IND)]:((p_wchar2 *)(PTR))[(IND)])

#define SET_INDEX_CHARP(PTR,IND,SHIFT,VAL) \
  ((SHIFT)==0?								\
   (((p_wchar0 *)(PTR))[(IND)] = DO_NOT_WARN ((p_wchar0) (VAL))):	\
   (SHIFT)==1?(((p_wchar1 *)(PTR))[(IND)] = DO_NOT_WARN ((p_wchar1) (VAL))): \
   (((p_wchar2 *)(PTR))[(IND)] = DO_NOT_WARN ((p_wchar2) (VAL))))


#define EXTRACT_CHARP(PTR,SHIFT) INDEX_CHARP((PTR),0,(SHIFT))
#define CHARP_ADD(PTR,X,SHIFT) (PTR)+=(X)<<(SHIFT)

#define INDEX_PCHARP(X,Y) INDEX_CHARP((X).ptr,(Y),(X).shift)
#define SET_INDEX_PCHARP(X,Y,Z) SET_INDEX_CHARP((X).ptr,(Y),(X).shift,(Z))
#define EXTRACT_PCHARP(X) INDEX_CHARP((X).ptr,(0),(X).shift)
#define INC_PCHARP(X,Y) (((X).ptr)+=(Y) << (X).shift)

#define LOW_COMPARE_PCHARP(X,CMP,Y) (((char *)((X).ptr)) CMP ((char *)((Y).ptr)))
#define LOW_SUBTRACT_PCHARP(X,Y) (LOW_COMPARE_PCHARP((X),-,(Y))>>(X).shift)

#ifdef PIKE_DEBUG
#define SUBTRACT_PCHARP(X,Y)    ((X).shift!=(Y).shift?(Pike_fatal("Subtracting different size charp!\n")),0:LOW_SUBTRACT_PCHARP((X),(Y)))
#define COMPARE_PCHARP(X,CMP,Y) ((X).shift!=(Y).shift?(Pike_fatal("Comparing different size charp!\n")),0:LOW_COMPARE_PCHARP((X),CMP,(Y)))
#else
#define SUBTRACT_PCHARP(X,Y) LOW_SUBTRACT_PCHARP((X),(Y))
#define COMPARE_PCHARP(X,CMP,Y) LOW_COMPARE_PCHARP((X),CMP,(Y))
#endif



static INLINE PCHARP MKPCHARP(const void *ptr, int shift)
{
  PCHARP tmp;
  tmp.ptr=(p_wchar0 *)ptr;
  tmp.shift=shift;
  return tmp;
}

#define MKPCHARP_OFF(PTR,SHIFT,OFF) MKPCHARP( ((char *)(PTR)) + ((OFF)<<(SHIFT)), (SHIFT))
#define MKPCHARP_STR(STR) MKPCHARP((STR)->str, (STR)->size_shift)
#define MKPCHARP_STR_OFF(STR,OFF) \
 MKPCHARP((STR)->str + ((OFF)<<(STR)->size_shift), (STR)->size_shift)
#define ADD_PCHARP(PTR,I) MKPCHARP_OFF((PTR).ptr,(PTR).shift,(I))

#define reference_shared_string(s) add_ref(s)
#define copy_shared_string(to,s) add_ref((to)=(s))

#ifdef DO_PIKE_CLEANUP

struct shared_string_location
{
  struct pike_string *s;
  struct shared_string_location *next;
};

PMOD_EXPORT extern struct shared_string_location *all_shared_string_locations;

#define MAKE_CONST_STRING(var, text) do {		\
  static struct shared_string_location str_;		\
  if(!str_.s) { 					\
    str_.s=make_shared_binary_string((text),CONSTANT_STRLEN(text)); \
    str_.next=all_shared_string_locations;		\
    all_shared_string_locations=&str_;			\
  }							\
  var = str_.s;						\
}while(0)

#define MAKE_CONST_STRING_CODE(var, code) do {				\
    static struct shared_string_location str_;				\
    if (!str_.s) {							\
      {code;}								\
      str_.s = var;							\
      DO_IF_DEBUG (							\
	if (!str_.s)							\
	  Pike_fatal ("Code at " __FILE__ ":" DEFINETOSTR (__LINE__)	\
		      " failed to produce a string in " #var ".\n");	\
      );								\
      str_.next=all_shared_string_locations;				\
      all_shared_string_locations=&str_;				\
    }									\
    else var = str_.s;							\
  } while (0)

#else

#define MAKE_CONST_STRING(var, text)						\
 do { static struct pike_string *str_;                                          \
    if(!str_) str_=make_shared_binary_string((text),CONSTANT_STRLEN(text));     \
    var = str_;									\
 }while(0)

#define MAKE_CONST_STRING_CODE(var, code) do {				\
    static struct pike_string *str_;					\
    if (!str_) {							\
      {code;}								\
      str_ = var;							\
      DO_IF_DEBUG (							\
	if (!str_)							\
	  Pike_fatal ("Code at " __FILE__ ":" DEFINETOSTR (__LINE__)	\
		      " failed to produce a string in " #var ".\n");	\
      );								\
    }									\
    else var = str_;							\
  } while (0)

#endif

#define REF_MAKE_CONST_STRING(var, text) do {				\
    MAKE_CONST_STRING(var, text);					\
    reference_shared_string(var);					\
  } while (0)

#define REF_MAKE_CONST_STRING_CODE(var, code) do {			\
    MAKE_CONST_STRING_CODE(var, code);					\
    reference_shared_string(var);					\
  } while (0)

/* Compatibility. */
#define MAKE_CONSTANT_SHARED_STRING(var, text)				\
  REF_MAKE_CONST_STRING(var, text)

#define convert_0_to_0(X,Y,Z) MEMCPY((char *)(X),(char *)(Y),(Z))
#define convert_1_to_1(X,Y,Z) MEMCPY((char *)(X),(char *)(Y),(Z)<<1)
#define convert_2_to_2(X,Y,Z) MEMCPY((char *)(X),(char *)(Y),(Z)<<2)

#define compare_0_to_0(X,Y,Z) MEMCMP((char *)(X),(char *)(Y),(Z))
#define compare_1_to_1(X,Y,Z) MEMCMP((char *)(X),(char *)(Y),(Z)<<1)
#define compare_2_to_2(X,Y,Z) MEMCMP((char *)(X),(char *)(Y),(Z)<<2)

#define CONVERT(FROM,TO) \
void PIKE_CONCAT4(convert_,FROM,_to_,TO)(PIKE_CONCAT(p_wchar,TO) *to, const PIKE_CONCAT(p_wchar,FROM) *from, ptrdiff_t len); \
INT32 PIKE_CONCAT4(compare_,FROM,_to_,TO)(const PIKE_CONCAT(p_wchar,TO) *to, const PIKE_CONCAT(p_wchar,FROM) *from, ptrdiff_t len);

PMOD_EXPORT extern struct pike_string *empty_pike_string;

/* Prototypes begin here */
PMOD_EXPORT unsigned INT32 index_shared_string(struct pike_string *s,
					       ptrdiff_t pos);
PMOD_EXPORT void low_set_index(struct pike_string *s, ptrdiff_t pos,
			       int value);
PMOD_EXPORT struct pike_string *debug_check_size_shift(struct pike_string *a,int shift);
CONVERT(0,1)
CONVERT(0,2)
CONVERT(1,0)
CONVERT(1,2)
CONVERT(2,0)
CONVERT(2,1)
PMOD_EXPORT int generic_compare_strings(const void *a, ptrdiff_t alen, int asize,
					const void *b, ptrdiff_t blen, int bsize);
PMOD_EXPORT void generic_memcpy(PCHARP to,
				PCHARP from,
				ptrdiff_t len);
PMOD_EXPORT void pike_string_cpy(PCHARP to, struct pike_string *from);
PMOD_EXPORT struct pike_string *binary_findstring(const char *foo, ptrdiff_t l);
PMOD_EXPORT struct pike_string *findstring(const char *foo);
struct short_pike_string0;
struct short_pike_string1;
struct short_pike_string2;
BLOCK_ALLOC(short_pike_string0, SHORT_STRING_BLOCK);
BLOCK_ALLOC(short_pike_string1, SHORT_STRING_BLOCK);
BLOCK_ALLOC(short_pike_string2, SHORT_STRING_BLOCK);




PMOD_EXPORT struct pike_string *debug_begin_shared_string(size_t len);
PMOD_EXPORT struct pike_string *debug_begin_wide_shared_string(size_t len, int shift);
PMOD_EXPORT struct pike_string *low_end_shared_string(struct pike_string *s);
PMOD_EXPORT struct pike_string *end_shared_string(struct pike_string *s);
PMOD_EXPORT struct pike_string *end_and_resize_shared_string(struct pike_string *str, ptrdiff_t len);
PMOD_EXPORT struct pike_string * debug_make_shared_binary_string(const char *str,size_t len);
PMOD_EXPORT struct pike_string * debug_make_shared_binary_pcharp(const PCHARP str,size_t len);
PMOD_EXPORT struct pike_string * debug_make_shared_pcharp(const PCHARP str);
PMOD_EXPORT struct pike_string * debug_make_shared_binary_string0(const p_wchar0 *str,size_t len);
PMOD_EXPORT struct pike_string * debug_make_shared_binary_string1(const p_wchar1 *str,size_t len);
PMOD_EXPORT struct pike_string * debug_make_shared_binary_string2(const p_wchar2 *str,size_t len);
PMOD_EXPORT struct pike_string *debug_make_shared_string(const char *str);
PMOD_EXPORT struct pike_string *debug_make_shared_string0(const p_wchar0 *str);
PMOD_EXPORT struct pike_string *debug_make_shared_string1(const p_wchar1 *str);
PMOD_EXPORT struct pike_string *debug_make_shared_string2(const p_wchar2 *str);
PMOD_EXPORT void unlink_pike_string(struct pike_string *s);
PMOD_EXPORT void do_free_string(struct pike_string *s);
PMOD_EXPORT void do_really_free_string(struct pike_string *s);
PMOD_EXPORT void do_really_free_pike_string(struct pike_string *s);
PMOD_EXPORT void really_free_string(struct pike_string *s);
PMOD_EXPORT void debug_free_string(struct pike_string *s);
struct pike_string *add_string_status(int verbose);
PMOD_EXPORT void check_string(struct pike_string *s);
PMOD_EXPORT void verify_shared_strings_tables(void);
PMOD_EXPORT int safe_debug_findstring(struct pike_string *foo);
PMOD_EXPORT struct pike_string *debug_findstring(const struct pike_string *foo);
PMOD_EXPORT void debug_dump_pike_string(struct pike_string *s, INT32 max);
void dump_stralloc_strings(void);
PMOD_EXPORT int low_quick_binary_strcmp(char *a, ptrdiff_t alen,
					char *b, ptrdiff_t blen);
PMOD_EXPORT ptrdiff_t generic_quick_binary_strcmp(const char *a,
						  ptrdiff_t alen, int asize,
						  const char *b,
						  ptrdiff_t blen, int bsize);
PMOD_EXPORT ptrdiff_t generic_find_binary_prefix(const char *a,
						 ptrdiff_t alen, int asize,
						 const char *b,
						 ptrdiff_t blen, int bsize);
PMOD_EXPORT int c_compare_string(struct pike_string *s, char *foo, int len);
PMOD_EXPORT ptrdiff_t my_quick_strcmp(struct pike_string *a,
				      struct pike_string *b);
PMOD_EXPORT ptrdiff_t my_strcmp(struct pike_string *a,struct pike_string *b);
PMOD_EXPORT struct pike_string *realloc_unlinked_string(struct pike_string *a,
							ptrdiff_t size);
PMOD_EXPORT struct pike_string *realloc_shared_string(struct pike_string *a,
						      ptrdiff_t size);
PMOD_EXPORT struct pike_string *new_realloc_shared_string(struct pike_string *a, INT32 size, int shift);
PMOD_EXPORT struct pike_string *modify_shared_string(struct pike_string *a,
					 INT32 position,
					 INT32 c);
PMOD_EXPORT struct pike_string *add_shared_strings(struct pike_string *a,
					 struct pike_string *b);
PMOD_EXPORT struct pike_string *add_and_free_shared_strings(struct pike_string *a,
						struct pike_string *b);
PMOD_EXPORT ptrdiff_t string_search(struct pike_string *haystack,
				    struct pike_string *needle,
				    ptrdiff_t start);
PMOD_EXPORT struct pike_string *string_slice(struct pike_string *s,
					     ptrdiff_t start,
					     ptrdiff_t len);
PMOD_EXPORT struct pike_string *string_replace(struct pike_string *str,
				   struct pike_string *del,
				   struct pike_string *to);
void init_shared_string_table(void);
void cleanup_shared_string_table(void);
void count_memory_in_strings(INT32 *num, INT32 *size);
unsigned gc_touch_all_strings(void);
void gc_mark_all_strings(void);
struct pike_string *next_pike_string (struct pike_string *s);
PMOD_EXPORT void init_string_builder(struct string_builder *s, int mag);
PMOD_EXPORT void init_string_builder_alloc(struct string_builder *s, ptrdiff_t length, int mag);
PMOD_EXPORT void init_string_builder_copy(struct string_builder *to,
					  struct string_builder *from);
PMOD_EXPORT int init_string_builder_with_string (struct string_builder *s,
						 struct pike_string *str);
PMOD_EXPORT void string_build_mkspace(struct string_builder *s,
				      ptrdiff_t chars, int mag);
PMOD_EXPORT void *string_builder_allocate(struct string_builder *s, ptrdiff_t chars, int mag);
PMOD_EXPORT void string_builder_putchar(struct string_builder *s, int ch);
PMOD_EXPORT void string_builder_binary_strcat0(struct string_builder *s,
					       const p_wchar0 *str, ptrdiff_t len);
PMOD_EXPORT void string_builder_binary_strcat1(struct string_builder *s,
					       const p_wchar1 *str, ptrdiff_t len);
PMOD_EXPORT void string_builder_binary_strcat2(struct string_builder *s,
					       const p_wchar2 *str, ptrdiff_t len);
PMOD_EXPORT void string_builder_append(struct string_builder *s,
				       PCHARP from,
				       ptrdiff_t len);
PMOD_EXPORT void string_builder_fill(struct string_builder *s,
				     ptrdiff_t howmany,
				     PCHARP from,
				     ptrdiff_t len,
				     ptrdiff_t offset);
PMOD_EXPORT void string_builder_strcat(struct string_builder *s, char *str);
PMOD_EXPORT void string_builder_shared_strcat(struct string_builder *s, struct pike_string *str);
PMOD_EXPORT ptrdiff_t string_builder_quote_string(struct string_builder *buf,
						  struct pike_string *str,
						  ptrdiff_t start,
						  ptrdiff_t max_len,
						  int flags);
PMOD_EXPORT void string_builder_append_integer(struct string_builder *s,
					       LONGEST val,
					       unsigned int base,
					       int flags,
					       size_t min_width,
					       size_t precision);
PMOD_EXPORT void string_builder_vsprintf(struct string_builder *s,
					 const char *fmt,
					 va_list args);
PMOD_EXPORT void string_builder_sprintf(struct string_builder *s,
					const char *fmt, ...);
PMOD_EXPORT void reset_string_builder(struct string_builder *s);
PMOD_EXPORT void free_string_builder(struct string_builder *s);
PMOD_EXPORT struct pike_string *finish_string_builder(struct string_builder *s);
PMOD_EXPORT PCHARP MEMCHR_PCHARP(PCHARP ptr, int chr, ptrdiff_t len);
PMOD_EXPORT long STRTOL_PCHARP(PCHARP str, PCHARP *ptr, int base);
PMOD_EXPORT int string_to_svalue_inumber(struct svalue *r,
			     char * str,
			     char **ptr,
			     int base,
			     int maxlength);
PMOD_EXPORT int wide_string_to_svalue_inumber(struct svalue *r,
					      void * str,
					      void *ptr,
					      int base,
					      ptrdiff_t maxlength,
					      int shift);
PMOD_EXPORT int pcharp_to_svalue_inumber(struct svalue *r,
					 PCHARP str,
					 PCHARP *ptr,
					 int base,
					 ptrdiff_t maxlength);
PMOD_EXPORT int convert_stack_top_string_to_inumber(int base);
PMOD_EXPORT double STRTOD_PCHARP(PCHARP nptr, PCHARP *endptr);
PMOD_EXPORT p_wchar0 *require_wstring0(struct pike_string *s,
			   char **to_free);
PMOD_EXPORT p_wchar1 *require_wstring1(struct pike_string *s,
			   char **to_free);
PMOD_EXPORT p_wchar2 *require_wstring2(struct pike_string *s,
			   char **to_free);
/* Prototypes end here */

static INLINE void string_builder_binary_strcat(struct string_builder *s,
						const char *str, ptrdiff_t len)
{
  string_builder_binary_strcat0 (s, (const p_wchar0 *) str, len);
}

#define ISCONSTSTR(X,Y) c_compare_string((X),Y,sizeof(Y)-sizeof(""))

#ifdef DEBUG_MALLOC
#define make_shared_string(X) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_string(X)))
#define make_shared_binary_string(X,Y) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_binary_string((X),(Y))))

#define make_shared_string0(X) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_string0(X)))
#define make_shared_binary_string0(X,Y) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_binary_string0((X),(Y))))

#define make_shared_string1(X) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_string1(X)))
#define make_shared_binary_string1(X,Y) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_binary_string1((X),(Y))))

#define make_shared_string2(X) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_string2(X)))
#define make_shared_binary_string2(X,Y) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_binary_string2((X),(Y))))

#define begin_shared_string(X) \
 ((struct pike_string *)debug_malloc_pass(debug_begin_shared_string(X)))
#define begin_wide_shared_string(X,Y) \
 ((struct pike_string *)debug_malloc_pass(debug_begin_wide_shared_string((X),(Y))))

#define make_shared_pcharp(X) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_pcharp(X)))
#define make_shared_binary_pcharp(X,Y) \
 ((struct pike_string *)debug_malloc_pass(debug_make_shared_binary_pcharp((X),(Y))))

#else
#define make_shared_string debug_make_shared_string
#define make_shared_binary_string debug_make_shared_binary_string

#define make_shared_string0 debug_make_shared_string0
#define make_shared_binary_string0 debug_make_shared_binary_string0

#define make_shared_string1 debug_make_shared_string1
#define make_shared_binary_string1 debug_make_shared_binary_string1

#define make_shared_string2 debug_make_shared_string2
#define make_shared_binary_string2 debug_make_shared_binary_string2

#define begin_shared_string debug_begin_shared_string
#define begin_wide_shared_string debug_begin_wide_shared_string

#define make_shared_pcharp debug_make_shared_pcharp
#define make_shared_binary_pcharp debug_make_shared_binary_pcharp

#endif

#undef CONVERT

PMOD_PROTO extern void f_sprintf(INT32 num_arg);

#endif /* STRALLOC_H */
