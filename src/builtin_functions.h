/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

#ifndef BUILTIN_EFUNS_H
#define BUILTIN_EFUNS_H

#define TYPEP(ID,NAME,TYPE) PMOD_EXPORT void ID(INT32 args);

#include "callback.h"
#include "block_alloc_h.h"

/* Weak flags for arrays, multisets and mappings. 1 is avoided for
 * compatibility reasons. */
#define PIKE_WEAK_INDICES 2
#define PIKE_WEAK_VALUES 4
#define PIKE_WEAK_BOTH 6

/* Prototypes begin here */
PMOD_EXPORT void debug_f_aggregate(INT32 args);
#ifdef DEBUG_MALLOC
#define f_aggregate(X) do { debug_f_aggregate(X); debug_malloc_touch(Pike_sp[-1].u.refs); } while (0)
#else
#define f_aggregate(X) debug_f_aggregate(X)
#endif

struct replace_many_tupel
{
  int prefix;
  int is_prefix;
  struct pike_string *ind;
  struct pike_string *val;
};

struct replace_many_context
{
  struct replace_many_tupel *v;
  struct pike_string *empty_repl;
  int set_start[256];
  int set_end[256];
  int other_start;
  int num;
  int flags;
};

PMOD_EXPORT struct object *get_val_true(void);
PMOD_EXPORT struct object *get_val_false(void);
PMOD_EXPORT struct object *get_val_null(void);
PMOD_EXPORT struct program *get_sql_null_prog(void);

PMOD_EXPORT void f_equal(INT32 args);
PMOD_EXPORT void f_hash(INT32 args);
PMOD_EXPORT void f_copy_value(INT32 args);
PMOD_EXPORT void f_lower_case(INT32 args);
PMOD_EXPORT void f_upper_case(INT32 args);
PMOD_EXPORT void f_random_string (INT32 args);
PMOD_EXPORT void f_random_seed(INT32 args);
PMOD_EXPORT void f_query_num_arg(INT32 args);
PMOD_EXPORT void f_search(INT32 args);
PMOD_EXPORT void f_has_prefix(INT32 args);
PMOD_EXPORT void f_has_suffix(INT32 args);
PMOD_EXPORT void f_has_index(INT32 args);
PMOD_EXPORT void f_has_value(INT32 args);
PMOD_EXPORT void f_add_constant(INT32 args);
PMOD_EXPORT void f_combine_path_nt(INT32 args);
PMOD_EXPORT void f_combine_path_unix(INT32 args);
PMOD_EXPORT void f_zero_type(INT32 args);
PMOD_EXPORT void f_string_to_unicode(INT32 args);
PMOD_EXPORT void f_unicode_to_string(INT32 args);
PMOD_EXPORT void f_string_to_utf8(INT32 args);
PMOD_EXPORT void f_utf8_to_string(INT32 args);
PMOD_EXPORT void f_all_constants(INT32 args);
PMOD_EXPORT void f_allocate(INT32 args);
void f_this_object(INT32 args);
PMOD_EXPORT DECLSPEC(noreturn) void f_throw(INT32 args) ATTRIBUTE((noreturn));
extern int in_forked_child;
PMOD_EXPORT void f_exit(INT32 args);
void f__exit(INT32 args) ATTRIBUTE((noreturn));
PMOD_EXPORT void f_time(INT32 args);
PMOD_EXPORT void f_crypt(INT32 args);
PMOD_EXPORT void f_destruct(INT32 args);
PMOD_EXPORT void f_indices(INT32 args);
PMOD_EXPORT void f_values(INT32 args);
PMOD_EXPORT void f_next_object(INT32 args);
PMOD_EXPORT void f_object_program(INT32 args);
int find_longest_prefix(char *str,
			ptrdiff_t len,
			int size_shift,
			struct replace_many_tupel *v,
			INT32 a,
			INT32 b);
void free_replace_many_context(struct replace_many_context *ctx);
void compile_replace_many(struct replace_many_context *ctx,
			  struct array *from,
			  struct array *to,
			  int reference_strings);
struct pike_string *execute_replace_many(struct replace_many_context *ctx,
					 struct pike_string *str);
PMOD_EXPORT void f_reverse(INT32 args);
PMOD_EXPORT void f_replace(INT32 args);
PMOD_EXPORT void f_compile(INT32 args);
PMOD_EXPORT void f_objectp(INT32 args);
PMOD_EXPORT void f_functionp(INT32 args);
PMOD_EXPORT void f_callablep(INT32 args);
PMOD_EXPORT void f_sleep(INT32 args);
PMOD_EXPORT void f_delay(INT32 args);
void f_gc(INT32 args);
PMOD_EXPORT void f_programp(INT32 args);
TYPEP(f_intp, "intpp", PIKE_T_INT)
TYPEP(f_mappingp, "mappingp", PIKE_T_MAPPING)
TYPEP(f_arrayp, "arrayp", PIKE_T_ARRAY)
TYPEP(f_multisetp, "multisetp", PIKE_T_MULTISET)
TYPEP(f_stringp, "stringp", PIKE_T_STRING)
TYPEP(f_floatp, "floatp", PIKE_T_FLOAT)
PMOD_EXPORT void f_sort(INT32 args);
PMOD_EXPORT void f_rows(INT32 args);
PMOD_EXPORT void f__verify_internals(INT32 args);
PMOD_EXPORT void f__debug(INT32 args);
PMOD_EXPORT void f__optimizer_debug(INT32 args);
PMOD_EXPORT void f__assembler_debug(INT32 args);
PMOD_EXPORT void f__compiler_trace(INT32 args);
PMOD_EXPORT void f_gmtime(INT32 args);
PMOD_EXPORT void f_localtime(INT32 args);
PMOD_EXPORT void f_mktime (INT32 args);
PMOD_EXPORT void f_glob(INT32 args);
PMOD_EXPORT void f_permute(INT32 args);
struct diff_magic_link;
struct diff_magic_link_pool;
struct diff_magic_link_head;
PMOD_EXPORT void f_diff(INT32 args);
PMOD_EXPORT void f_diff_compare_table(INT32 args);
PMOD_EXPORT void f_diff_longest_sequence(INT32 args);
PMOD_EXPORT void f_diff_dyn_longest_sequence(INT32 args);
struct callback *add_memory_usage_callback(callback_func call,
					  void *arg,
					  callback_func free_func);
PMOD_EXPORT void f__memory_usage(INT32 args);
PMOD_EXPORT void f__next(INT32 args);
PMOD_EXPORT void f__prev(INT32 args);
PMOD_EXPORT void f__refs(INT32 args);
PMOD_EXPORT void f__leak(INT32 args);
PMOD_EXPORT void f__typeof(INT32 args);
PMOD_EXPORT void f_replace_master(INT32 args);
PMOD_EXPORT void f_master(INT32 args);
PMOD_EXPORT void f_gethrvtime(INT32 args);
PMOD_EXPORT void f_gethrtime(INT32 args);
PMOD_EXPORT void f_object_variablep(INT32 args);
PMOD_EXPORT void f_uniq_array(INT32 args);
PMOD_EXPORT void f_splice(INT32 args);
PMOD_EXPORT void f_everynth(INT32 args);
PMOD_EXPORT void f_transpose(INT32 args);
PMOD_EXPORT void f__reset_dmalloc(INT32 args);
PMOD_EXPORT void f__dmalloc_set_name(INT32 args);
PMOD_EXPORT void f__list_open_fds(INT32 args);
PMOD_EXPORT void f__locate_references(INT32 args);
PMOD_EXPORT void f__describe(INT32 args);
PMOD_EXPORT void f__gc_set_watch(INT32 args);
PMOD_EXPORT void f__dump_backlog(INT32 args);
PMOD_EXPORT void f_map(INT32 args);
PMOD_EXPORT void f_filter(INT32 args);
PMOD_EXPORT void f_inherit_list(INT32 args);
PMOD_EXPORT void f_function_defined(INT32 args);
void init_builtin_efuns(void);
void exit_builtin_efuns(void);

/* From iterators.cmod. */
PMOD_EXPORT void f_get_iterator(INT32 args);
int foreach_iterate(struct object *o, int do_step);

/* From builtin.cmod. */
PMOD_EXPORT void f_cq__describe_program(INT32 args);
PMOD_EXPORT void f_basetype(INT32 args);
PMOD_EXPORT void f_int2char(INT32 args);
PMOD_EXPORT void f_int2hex(INT32 args);
PMOD_EXPORT void f_string2hex(INT32 args);
PMOD_EXPORT void f_hex2string(INT32 args);
PMOD_EXPORT void f_column(INT32 args);
PMOD_EXPORT void f_mkmultiset(INT32 args);
PMOD_EXPORT void f_trace(INT32 args);
PMOD_EXPORT void f_gc_parameters(INT32 args);
PMOD_EXPORT void f_ctime(INT32 args);
PMOD_EXPORT void f_mkmapping(INT32 args);
PMOD_EXPORT void f_string_count(INT32 args);
PMOD_EXPORT void f_string_trim_whites(INT32 args);
PMOD_EXPORT void f_string_normalize_space(INT32 args);
PMOD_EXPORT void f_string_trim_all_whites(INT32 args);
PMOD_EXPORT void f_program_implements(INT32 args);
PMOD_EXPORT void f_program_inherits(INT32 args);
PMOD_EXPORT void f_program_defined(INT32 args);
PMOD_EXPORT void f_string_width(INT32 args);
PMOD_EXPORT void f_m_delete(INT32 args);
PMOD_EXPORT void f_get_weak_flag(INT32 args);
PMOD_EXPORT void f_function_name(INT32 args);
PMOD_EXPORT void f_function_object(INT32 args);
PMOD_EXPORT void f_function_program(INT32 args);
PMOD_EXPORT void f_random(INT32 args);
PMOD_EXPORT void f_backtrace(INT32 args);

struct pike_list_node
{
  /* NOTE: Unusual order of elements due to use of sentinels. */
  struct pike_list_node *next;
  INT32 refs;
  struct pike_list_node *prev;
  struct svalue val;
};
BLOCK_ALLOC_FILL_PAGES(pike_list_node, 4);
PMOD_EXPORT void free_list_node(struct pike_list_node *node);
PMOD_EXPORT void unlink_list_node(struct pike_list_node *n);
PMOD_EXPORT void prepend_list_node(struct pike_list_node *node,
				   struct pike_list_node *new_node);
PMOD_EXPORT void append_list_node(struct pike_list_node *node,
				  struct pike_list_node *new_node);
void init_builtin(void);
void exit_builtin(void);

/* From modules/files */
void f_werror (INT32 args);

/* Prototypes end here */

#endif
