/* -*- C -*- */
/*\
||| This file a part of Pike, and is copyright by Fredrik Hubinette
||| Pike is distributed as GPL (General Public License)
||| See the files COPYING and DISCLAIMER for more information.
\*/
/**/
%pure_parser

%token TOK_ARROW

/*
 * Basic value pushing
 */
%token TOK_CONSTANT TOK_FLOAT TOK_STRING
%token TOK_NUMBER

/*
 * These are the predefined functions that can be accessed from Pike.
 */

%token TOK_INC TOK_DEC
%token TOK_RETURN

%token TOK_EQ TOK_GE TOK_LE TOK_NE
%token TOK_NOT
%token TOK_LSH TOK_RSH
%token TOK_LAND TOK_LOR

%token TOK_SWITCH TOK_SSCANF TOK_CATCH
%token TOK_FOREACH

/* This is the end of file marker used by the lexer
 * to enable nicer EOF in error handling.
 */
%token TOK_LEX_EOF

%token TOK_ADD_EQ
%token TOK_AND_EQ
%token TOK_ARRAY_ID
%token TOK_BREAK
%token TOK_CASE
%token TOK_CLASS
%token TOK_COLON_COLON
%token TOK_CONTINUE 
%token TOK_DEFAULT
%token TOK_DIV_EQ
%token TOK_DO
%token TOK_DOT_DOT
%token TOK_DOT_DOT_DOT
%token TOK_ELSE
%token TOK_EXTERN
%token TOK_FLOAT_ID
%token TOK_FOR
%token TOK_FUNCTION_ID
%token TOK_GAUGE
%token TOK_IDENTIFIER
%token TOK_IF
%token TOK_IMPORT
%token TOK_INHERIT
%token TOK_INLINE
%token TOK_LOCAL_ID
%token TOK_FINAL_ID
%token TOK_INT_ID
%token TOK_LAMBDA
%token TOK_MULTISET_ID
%token TOK_MULTISET_END
%token TOK_MULTISET_START
%token TOK_LSH_EQ
%token TOK_MAPPING_ID
%token TOK_MIXED_ID
%token TOK_MOD_EQ
%token TOK_MULT_EQ
%token TOK_NO_MASK
%token TOK_OBJECT_ID
%token TOK_OR_EQ
%token TOK_PRIVATE
%token TOK_PROGRAM_ID
%token TOK_PROTECTED
%token TOK_PREDEF
%token TOK_PUBLIC
%token TOK_RSH_EQ
%token TOK_STATIC
%token TOK_STRING_ID
%token TOK_SUB_EQ
%token TOK_TYPEOF
%token TOK_VARIANT
%token TOK_VOID_ID
%token TOK_WHILE
%token TOK_XOR_EQ
%token TOK_OPTIONAL


%right '='
%right '?'
%left TOK_LOR
%left TOK_LAND
%left '|'
%left '^'
%left '&'
%left TOK_EQ TOK_NE
%left '>' TOK_GE '<' TOK_LE  /* nonassoc? */
%left TOK_LSH TOK_RSH
%left '+' '-'
%left '*' '%' '/'
%right TOK_NOT '~'
%nonassoc TOK_INC TOK_DEC

%{
/* This is the grammar definition of Pike. */

#include "global.h"
RCSID("$Id: language.yacc,v 1.222 2001/01/16 09:36:30 hubbe Exp $");
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

#include "interpret.h"
#include "array.h"
#include "object.h"
#include "stralloc.h"
#include "las.h"
#include "interpret.h"
#include "program.h"
#include "pike_types.h"
#include "constants.h"
#include "pike_macros.h"
#include "pike_error.h"
#include "docode.h"
#include "machine.h"
#include "main.h"
#include "opcodes.h"

#define YYMAXDEPTH	1000

#ifdef PIKE_DEBUG
#ifndef YYDEBUG
/* May also be defined by machine.h */
#define YYDEBUG 1
#endif /* YYDEBUG */
#endif


int add_local_name(struct pike_string *,struct pike_string *,node *);
int low_add_local_name(struct compiler_frame *,
		       struct pike_string *,struct pike_string *,node *);
static node *lexical_islocal(struct pike_string *);

static int inherit_depth;
static struct program_state *inherit_state = NULL;

/*
 * Kludge for Bison not using prototypes.
 */
#ifndef __GNUC__
#ifndef __cplusplus
static void __yy_memcpy(char *to, char *from, unsigned int count);
#endif /* !__cplusplus */
#endif /* !__GNUC__ */

%}

%union
{
  int number;
  FLOAT_TYPE fnum;
  struct node_s *n;
  char *str;
}

%{
/* Needs to be included after YYSTYPE is defined. */
#include "lex.h"
%}

%{
/* Include <stdio.h> our selves, so that we can do our magic
 * without being disturbed... */
#include <stdio.h>
int yylex(YYSTYPE *yylval);
/* Bison is stupid, and tries to optimize for space... */
#ifdef YYBISON
#define short int
#endif /* YYBISON */
%}

%type <fnum> TOK_FLOAT

%type <number> TOK_ARRAY_ID
%type <number> TOK_BREAK
%type <number> TOK_CASE
%type <number> TOK_CATCH
%type <number> TOK_CONTINUE
%type <number> TOK_DEFAULT
%type <number> TOK_DO
%type <number> TOK_ELSE
%type <number> TOK_FLOAT_ID
%type <number> TOK_FOR
%type <number> TOK_FOREACH
%type <number> TOK_FUNCTION_ID
%type <number> TOK_GAUGE
%type <number> TOK_IF
%type <number> TOK_INHERIT
%type <number> TOK_INLINE
%type <number> TOK_INT_ID
%type <number> TOK_LAMBDA
%type <number> TOK_LOCAL_ID
%type <number> TOK_MAPPING_ID
%type <number> TOK_MIXED_ID
%type <number> TOK_MULTISET_ID
%type <number> TOK_NO_MASK
%type <number> TOK_OBJECT_ID
%type <number> TOK_PREDEF
%type <number> TOK_PRIVATE
%type <number> TOK_PROGRAM_ID
%type <number> TOK_PROTECTED
%type <number> TOK_PUBLIC
%type <number> TOK_RETURN
%type <number> TOK_SSCANF
%type <number> TOK_STATIC
%type <number> TOK_STRING_ID
%type <number> TOK_SWITCH
%type <number> TOK_VOID_ID
%type <number> TOK_WHILE
%type <number> arguments
%type <number> arguments2
%type <number> func_args
%type <number> optional_create_arguments
%type <number> create_arguments
%type <number> create_arguments2
%type <number> create_arg
%type <number> assign
%type <number> modifier
%type <number> modifier_list
%type <number> modifiers
%type <number> inherit_specifier
%type <number> function_type_list
%type <number> function_type_list2
%type <number> optional_dot_dot_dot
%type <number> optional_comma
%type <number> optional_stars

%type <str> magic_identifiers
%type <str> magic_identifiers1
%type <str> magic_identifiers2
%type <str> magic_identifiers3

/* The following symbols return type information */

%type <n> number_or_minint
%type <n> number_or_maxint
%type <n> cast
%type <n> soft_cast
%type <n> simple_type
%type <n> simple_type2
%type <n> simple_identifier_type
%type <n> string_constant
%type <n> string
%type <n> TOK_STRING
%type <n> TOK_NUMBER
%type <n> optional_rename_inherit
%type <n> optional_identifier
%type <n> TOK_IDENTIFIER
%type <n> assoc_pair
%type <n> block
%type <n> optional_block
%type <n> failsafe_block
%type <n> close_paren_or_missing
%type <n> block_or_semi
%type <n> break
%type <n> case
%type <n> catch
%type <n> catch_arg
%type <n> class
%type <n> safe_comma_expr
%type <n> comma_expr
%type <n> comma_expr2
%type <n> comma_expr_or_maxint
%type <n> comma_expr_or_zero
%type <n> cond
%type <n> continue
%type <n> default
%type <n> do
%type <n> safe_expr0
%type <n> expr00
%type <n> expr01
%type <n> expr1
%type <n> expr2
%type <n> expr3 expr0
%type <n> expr4
%type <n> expr_list
%type <n> expr_list2
%type <n> for
%type <n> for_expr
%type <n> foreach
%type <n> gauge
%type <n> idents
%type <n> idents2
%type <n> labeled_statement
%type <n> lambda
%type <n> local_name_list
%type <n> local_name_list2
%type <n> low_idents
%type <n> safe_lvalue
%type <n> lvalue
%type <n> lvalue_list
%type <n> low_lvalue_list
%type <n> m_expr_list
%type <n> m_expr_list2
%type <n> new_local_name
%type <n> new_local_name2
%type <n> normal_label_statement
%type <n> optional_else_part
%type <n> optional_label
%type <n> return
%type <n> sscanf
%type <n> statement
%type <n> statements
%type <n> switch
%type <n> typeof
%type <n> unused
%type <n> unused2
%type <n> while
%type <n> optional_comma_expr
%type <n> low_program_ref
%type <n> local_function
%type <n> local_function2
%type <n> magic_identifier
%%

all: program { YYACCEPT; }
  | program TOK_LEX_EOF { YYACCEPT; }
/*  | error TOK_LEX_EOF { YYABORT; } */
  ;

program: program def
  | program ';'
  |  /* empty */
  ;

string_constant: string
  | string_constant '+' string
  {
    struct pike_string *a,*b;
    copy_shared_string(a,$1->u.sval.u.string);
    copy_shared_string(b,$3->u.sval.u.string);
    free_node($1);
    free_node($3);
    a=add_and_free_shared_strings(a,b);
    $$=mkstrnode(a);
    free_string(a);
  }
  ;

optional_rename_inherit: ':' TOK_IDENTIFIER { $$=$2; }
  | ':' bad_identifier { $$=0; }
  | ':' error { $$=0; }
  | { $$=0; }
  ;

/* NOTE: This rule pushes a string "name" on the stack in addition
 * to resolving the program reference.
 */
low_program_ref: string_constant
  {
    ref_push_string($1->u.sval.u.string);
    ref_push_string($1->u.sval.u.string);
    ref_push_string(lex.current_file);
    
    if (error_handler && error_handler->prog) {
      ref_push_object(error_handler);
      SAFE_APPLY_MASTER("handle_inherit", 3);
    } else {
      SAFE_APPLY_MASTER("handle_inherit", 2);
    }

    if(Pike_sp[-1].type != T_PROGRAM)
      my_yyerror("Couldn't cast string \"%s\" to program",
		 $1->u.sval.u.string->str);
    free_node($1);
    $$=mksvaluenode(Pike_sp-1);
    if($$->name) free_string($$->name);
    add_ref( $$->name=Pike_sp[-2].u.string );
    pop_stack();
  }
  | idents
  {
    if(Pike_compiler->last_identifier)
    {
      ref_push_string(Pike_compiler->last_identifier);
    }else{
      push_constant_text("");
    }
    $$=$1;
  }
  ;

/* NOTE: Pushes the resolved program on the stack. */
program_ref: low_program_ref
  {
    resolv_program($1);
    free_node($1);
  }
  ;
      
inheritance: modifiers TOK_INHERIT low_program_ref optional_rename_inherit ';'
  {
    if (($1 & ID_EXTERN) && (Pike_compiler->compiler_pass == 1)) {
      yywarning("Extern declared inherit.");
    }
    if(!(Pike_compiler->new_program->flags & PROGRAM_PASS_1_DONE))
    {
      struct pike_string *s=Pike_sp[-1].u.string;
      if($4) s=$4->u.sval.u.string;
      compiler_do_inherit($3,$1,s);
    }
    if($4) free_node($4);
    pop_n_elems(1);
    free_node($3);
  }
  | modifiers TOK_INHERIT low_program_ref error ';'
  {
    free_node($3); yyerrok;
  }
  | modifiers TOK_INHERIT low_program_ref error TOK_LEX_EOF
  {
    free_node($3);
    yyerror("Missing ';'.");
    yyerror("Unexpected end of file.");
  }
  | modifiers TOK_INHERIT low_program_ref error '}'
  {
    free_node($3); yyerror("Missing ';'.");
  }
  | modifiers TOK_INHERIT error ';' { yyerrok; }
  | modifiers TOK_INHERIT error TOK_LEX_EOF
  {
    yyerror("Missing ';'.");
    yyerror("Unexpected end of file.");
  }
  | modifiers TOK_INHERIT error '}' { yyerror("Missing ';'."); }
  ;

import: TOK_IMPORT idents ';'
  {
    resolv_constant($2);
    free_node($2);
    use_module(Pike_sp-1);
    pop_stack();
  }
  | TOK_IMPORT string ';'
  {
    ref_push_string($2->u.sval.u.string);
    free_node($2);
    ref_push_string(lex.current_file);
    if (error_handler && error_handler->prog) {
      ref_push_object(error_handler);
      SAFE_APPLY_MASTER("handle_import", 3);
    } else {
      SAFE_APPLY_MASTER("handle_import", 2);
    }
    use_module(Pike_sp-1);
    pop_stack();
  }
  | TOK_IMPORT error ';' { yyerrok; }
  | TOK_IMPORT error TOK_LEX_EOF
  {
    yyerror("Missing ';'.");
    yyerror("Unexpected end of file.");
  }
  | TOK_IMPORT error '}' { yyerror("Missing ';'."); }
  ;

constant_name: TOK_IDENTIFIER '=' safe_expr0
  {
    /* This can be made more lenient in the future */

    /* Ugly hack to make sure that $3 is optimized */
    {
      int tmp=Pike_compiler->compiler_pass;
      $3=mknode(F_COMMA_EXPR,$3,0);
      Pike_compiler->compiler_pass=tmp;
    }

    if ((Pike_compiler->current_modifiers & ID_EXTERN) &&
	(Pike_compiler->compiler_pass == 1)) {
      yywarning("Extern declared constant.");
    }

    if(!is_const($3))
    {
      if(Pike_compiler->compiler_pass==2)
	yyerror("Constant definition is not constant.");
      else
	add_constant($1->u.sval.u.string, 0,
		     Pike_compiler->current_modifiers & ~ID_EXTERN);
    } else {
      if(!Pike_compiler->num_parse_error)
      {
	ptrdiff_t tmp=eval_low($3);
	if(tmp < 1)
	{
	  yyerror("Error in constant definition.");
	}else{
	  pop_n_elems(DO_NOT_WARN((INT32)(tmp - 1)));
	  add_constant($1->u.sval.u.string, Pike_sp-1,
		       Pike_compiler->current_modifiers & ~ID_EXTERN);
	  pop_stack();
	}
      }
    }
    if($3) free_node($3);
    free_node($1);
  }
  | bad_identifier '=' safe_expr0 { if ($3) free_node($3); }
  | error '=' safe_expr0 { if ($3) free_node($3); }
  ;

constant_list: constant_name
  | constant_list ',' constant_name
  ;

constant: modifiers TOK_CONSTANT constant_list ';' {}
  | modifiers TOK_CONSTANT error ';' { yyerrok; }
  | modifiers TOK_CONSTANT error TOK_LEX_EOF
  {
    yyerror("Missing ';'.");
    yyerror("Unexpected end of file.");
  }
  | modifiers TOK_CONSTANT error '}' { yyerror("Missing ';'."); }
  ;

block_or_semi: block
  {
    $$ = check_node_hash(mknode(F_COMMA_EXPR,$1,mknode(F_RETURN,mkintnode(0),0)));
  }
  | ';' { $$ = NULL; }
  | TOK_LEX_EOF { yyerror("Expected ';'."); $$ = NULL; }
  | error { $$ = NULL; }
  ;


type_or_error: simple_type
  {
#ifdef PIKE_DEBUG
    check_type_string(check_node_hash($1)->u.sval.u.string);
#endif /* PIKE_DEBUG */
    if(Pike_compiler->compiler_frame->current_type)
      free_string(Pike_compiler->compiler_frame->current_type); 
    copy_shared_string(Pike_compiler->compiler_frame->current_type,$1->u.sval.u.string);
    free_node($1);
  }
  ;

close_paren_or_missing: ')'
  {
    /* Used to hold line-number info */
    $$ = mkintnode(0);
  }
  | /* empty */
  {
    yyerror("Missing ')'.");
    /* Used to hold line-number info */
    $$ = mkintnode(0);
  }
  ;

close_brace_or_missing: '}'
  | /* empty */
  {
    yyerror("Missing '}'.");
  }
  ;

close_bracket_or_missing: ']'
  | /* empty */
  {
    yyerror("Missing ']'.");
  }
  ;

push_compiler_frame0: /* empty */
  {
    push_compiler_frame(SCOPE_LOCAL);

    if(!Pike_compiler->compiler_frame->previous ||
       !Pike_compiler->compiler_frame->previous->current_type)
    {
      yyerror("Internal compiler fault.");
      copy_shared_string(Pike_compiler->compiler_frame->current_type,
			 mixed_type_string);
    }else{
      copy_shared_string(Pike_compiler->compiler_frame->current_type,
			 Pike_compiler->compiler_frame->previous->current_type);
    }
  }
  ;

def: modifiers type_or_error optional_stars TOK_IDENTIFIER push_compiler_frame0
  '(' arguments close_paren_or_missing
  {
    int e;
    /* construct the function type */
    push_finished_type(Pike_compiler->compiler_frame->current_type);
    if ($3 && (Pike_compiler->compiler_pass == 2)) {
      yywarning("The *-syntax in types is obsolete. Use array instead.");
    }
    while(--$3>=0) push_type(T_ARRAY);
    
    if(Pike_compiler->compiler_frame->current_return_type)
      free_string(Pike_compiler->compiler_frame->current_return_type);
    Pike_compiler->compiler_frame->current_return_type=compiler_pop_type();
    
    push_finished_type(Pike_compiler->compiler_frame->current_return_type);
    
    e=$7-1;
    if(Pike_compiler->varargs)
    {
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
      e--;
      Pike_compiler->varargs=0;
      pop_type_stack();
    }else{
      push_type(T_VOID);
    }
    push_type(T_MANY);
    for(; e>=0; e--)
    {
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
    }
    push_type(T_FUNCTION);

    {
      struct pike_string *s=compiler_pop_type();
      int i = isidentifier($4->u.sval.u.string);

      if (Pike_compiler->compiler_pass == 1) {
	if ($1 & ID_VARIANT) {
	  /* FIXME: Lookup the type of any existing variant */
	  /* Or the types. */
	  fprintf(stderr, "Pass %d: Identifier %s:\n",
		  Pike_compiler->compiler_pass, $4->u.sval.u.string->str);

	  if (i >= 0) {
	    struct identifier *id = ID_FROM_INT(Pike_compiler->new_program, i);
	    if (id) {
	      struct pike_string *new_type;
	      fprintf(stderr, "Defined, type:\n");
#ifdef PIKE_DEBUG
	      simple_describe_type(id->type);
#endif

	      new_type = or_pike_types(s, id->type, 1);
	      free_string(s);
	      s = new_type;

	      fprintf(stderr, "Resulting type:\n");
#ifdef PIKE_DEBUG
	      simple_describe_type(s);
#endif
	    } else {
	      my_yyerror("Lost identifier %s (%d).",
			 $4->u.sval.u.string->str, i);
	    }
	  } else {
	    fprintf(stderr, "Not defined.\n");
	  }
	  fprintf(stderr, "New type:\n");
#ifdef PIKE_DEBUG
	  simple_describe_type(s);
#endif
	}
      } else {
	/* FIXME: Second pass reuses the type from the end of
	 * the first pass if this is a variant function.
	 */
	if (i >= 0) {
	  if (Pike_compiler->new_program->identifier_references[i].id_flags &
	      ID_VARIANT) {
	    struct identifier *id = ID_FROM_INT(Pike_compiler->new_program, i);
	    fprintf(stderr, "Pass %d: Identifier %s:\n",
		    Pike_compiler->compiler_pass, $4->u.sval.u.string->str);

	    free_string(s);
	    copy_shared_string(s, id->type);

	    fprintf(stderr, "Resulting type:\n");
#ifdef PIKE_DEBUG
	    simple_describe_type(s);
#endif
	  }
	} else {
	  my_yyerror("Identifier %s lost after first pass.",
		     $4->u.sval.u.string->str);
	}
      }

      $<n>$=mkstrnode(s);
      free_string(s);
    }


/*    if(Pike_compiler->compiler_pass==1) */
    {
      /* FIXME:
       * set current_function_number for local functions as well
       */
      Pike_compiler->compiler_frame->current_function_number=
	define_function(check_node_hash($4)->u.sval.u.string,
			check_node_hash($<n>$)->u.sval.u.string,
			$1 & (~ID_EXTERN),
			IDENTIFIER_PIKE_FUNCTION,
			0,
			OPT_EXTERNAL_DEPEND|OPT_SIDE_EFFECT);

      if ($1 & ID_VARIANT) {
	fprintf(stderr, "Function number: %d\n",
		Pike_compiler->compiler_frame->current_function_number);
      }
    }
  }
  block_or_semi
  {
    int e;
    if($10)
    {
      int f;
      node *check_args = NULL;
      int save_line = lex.current_line;
      int num_required_args = 0;
      struct identifier *i;
#ifdef PIKE_DEBUG
      struct pike_string *save_file = lex.current_file;
      lex.current_file = $8->current_file;
#endif /* PIKE_DEBUG */
      lex.current_line = $8->line_number;

      if (($1 & ID_EXTERN) && (Pike_compiler->compiler_pass == 1)) {
	yywarning("Extern declared function definition.");
      }

      for(e=0; e<$7; e++)
      {
	if(!Pike_compiler->compiler_frame->variable[e].name ||
	   !Pike_compiler->compiler_frame->variable[e].name->len)
	{
	  my_yyerror("Missing name for argument %d.",e);
	} else {
	  if (Pike_compiler->compiler_pass == 2) {
	    if ($1 & ID_VARIANT) {
	      struct pike_string *arg_type =
		Pike_compiler->compiler_frame->variable[e].type;

	      /* FIXME: Generate code that checks the arguments. */
	      /* If there is a bad argument, call the fallback, and return. */
	      if (! pike_types_le(void_type_string, arg_type)) {
		/* Argument my not be void.
		 * ie it's required.
		 */
		num_required_args++;
	      }
	    } else {
	      /* FIXME: Should probably use some other flag. */
	      if ((runtime_options & RUNTIME_CHECK_TYPES) &&
		  (Pike_compiler->compiler_frame->variable[e].type != mixed_type_string)) {
		node *local_node;

		/* fprintf(stderr, "Creating soft cast node for local #%d\n", e);*/

		local_node = mklocalnode(e, 0);

		/* The following is needed to go around the optimization in
		 * mksoftcastnode().
		 */
		free_string(local_node->type);
		copy_shared_string(local_node->type, mixed_type_string);

		check_args =
		  mknode(F_COMMA_EXPR, check_args,
			 mksoftcastnode(Pike_compiler->compiler_frame->variable[e].type,
					local_node));
	      }
	    }
	  }
	}
      }

      if ($1 & ID_VARIANT) {
	struct pike_string *bad_arg_str;
	MAKE_CONSTANT_SHARED_STRING(bad_arg_str,
				    "Bad number of arguments!\n");

	fprintf(stderr, "Required args: %d\n", num_required_args);

	check_args =
	  mknode('?',
		 mkopernode("`<",
			    mkefuncallnode("query_num_arg", NULL),
			    mkintnode(num_required_args)),
		 mknode(':',
			mkefuncallnode("throw",
				       mkefuncallnode("aggregate",
						      mkstrnode(bad_arg_str))),
			NULL));
	free_string(bad_arg_str);
      }

      if (check_args) {
	/* Prepend the arg checking code. */
	$10 = mknode(F_COMMA_EXPR, mknode(F_POP_VALUE, check_args, NULL), $10);
      }

      lex.current_line = save_line;
#ifdef PIKE_DEBUG
      lex.current_file = save_file;
#endif /* PIKE_DEBUG */

      f=dooptcode(check_node_hash($4)->u.sval.u.string,
		  check_node_hash($10),
		  check_node_hash($<n>9)->u.sval.u.string,
		  $1);

      i = ID_FROM_INT(Pike_compiler->new_program, f);
      i->opt_flags = Pike_compiler->compiler_frame->opt_flags;

      if ($1 & ID_VARIANT) {
	fprintf(stderr, "Function number: %d\n", f);
      }

#ifdef PIKE_DEBUG
      if(Pike_interpreter.recoveries &&
	 ((Pike_sp - Pike_interpreter.evaluator_stack) <
	  Pike_interpreter.recoveries->stack_pointer))
	fatal("Stack error (underflow)\n");

      if((Pike_compiler->compiler_pass == 1) &&
	 (f != Pike_compiler->compiler_frame->current_function_number))
	fatal("define_function screwed up! %d != %d\n",
	      f, Pike_compiler->compiler_frame->current_function_number);
#endif
    }
    pop_compiler_frame();
    free_node($4);
    free_node($8);
    free_node($<n>9);
  }
  | modifiers type_or_error optional_stars TOK_IDENTIFIER push_compiler_frame0
    error
  {
    pop_compiler_frame();
    free_node($4);
  }
  | modifiers type_or_error optional_stars bad_identifier
  {
    free_string(compiler_pop_type());
  }
    '(' arguments ')' block_or_semi
  {
    if ($9) free_node($9);
  }
  | modifiers type_or_error name_list ';' {}
  | inheritance {}
  | import {}
  | constant {}
  | class { free_node($1); }
  | error TOK_LEX_EOF
  {
    reset_type_stack();
    yyerror("Missing ';'.");
    yyerror("Unexpected end of file");
  }
  | error ';'
  {
    reset_type_stack();
    yyerrok;
/*     if(Pike_compiler->num_parse_error>5) YYACCEPT; */
  }
  | error '}'
  {
    reset_type_stack();
    yyerror("Missing ';'.");
   /* yychar = '}';	*/ /* Put the '}' back on the input stream */
  }
  | modifiers
   '{' 
    {
      $<number>$=lex.pragmas;
      lex.pragmas|=$1;
    }
      program
   '}'
    {
      lex.pragmas=$<number>3;
    }
  ;

optional_dot_dot_dot: TOK_DOT_DOT_DOT { $$=1; }
  | TOK_DOT_DOT
  {
    yyerror("Range indicator ('..') where elipsis ('...') expected.");
    $$=1;
  }
  | /* empty */ { $$=0; }
  ;

optional_identifier: TOK_IDENTIFIER
  | bad_identifier { $$=0; }
  | /* empty */ { $$=0; }
  ;

new_arg_name: type7 optional_dot_dot_dot optional_identifier
  {
    if(Pike_compiler->varargs) yyerror("Can't define more arguments after ...");

    if($2)
    {
      push_type(T_ARRAY);
      Pike_compiler->varargs=1;
    }

    if(!$3)
    {
      struct pike_string *s;
      MAKE_CONSTANT_SHARED_STRING(s,"");
      $3=mkstrnode(s);
      free_string(s);
    }

    if($3->u.sval.u.string->len &&
       islocal($3->u.sval.u.string) >= 0)
      my_yyerror("Variable '%s' appears twice in argument list.",
		 $3->u.sval.u.string->str);
    
    add_local_name($3->u.sval.u.string, compiler_pop_type(),0);
    free_node($3);
  }
  ;

func_args: '(' arguments close_paren_or_missing
  {
    free_node($3);
    $$=$2;
  }
  ;

arguments: /* empty */ optional_comma { $$=0; }
  | arguments2 optional_comma
  ;

arguments2: new_arg_name { $$ = 1; }
  | arguments2 ',' new_arg_name { $$ = $1 + 1; }
  | arguments2 ':' new_arg_name
  {
    yyerror("Unexpected ':' in argument list.");
    $$ = $1 + 1;
  }
  ;

modifier:
    TOK_NO_MASK    { $$ = ID_NOMASK; }
  | TOK_FINAL_ID   { $$ = ID_NOMASK; }
  | TOK_STATIC     { $$ = ID_STATIC; }
  | TOK_EXTERN     { $$ = ID_EXTERN; }
  | TOK_OPTIONAL   { $$ = ID_OPTIONAL; }
  | TOK_PRIVATE    { $$ = ID_PRIVATE | ID_STATIC; }
  | TOK_LOCAL_ID   { $$ = ID_INLINE; }
  | TOK_PUBLIC     { $$ = ID_PUBLIC; }
  | TOK_PROTECTED  { $$ = ID_PROTECTED; }
  | TOK_INLINE     { $$ = ID_INLINE; }
  | TOK_VARIANT    { $$ = ID_VARIANT; }
  ;

magic_identifiers1:
    TOK_NO_MASK    { $$ = "nomask"; }
  | TOK_FINAL_ID   { $$ = "final"; }
  | TOK_STATIC     { $$ = "static"; }
  | TOK_EXTERN	   { $$ = "extern"; }
  | TOK_PRIVATE    { $$ = "private"; }
  | TOK_LOCAL_ID   { $$ = "local"; }
  | TOK_PUBLIC     { $$ = "public"; }
  | TOK_PROTECTED  { $$ = "protected"; }
  | TOK_INLINE     { $$ = "inline"; }
  | TOK_OPTIONAL   { $$ = "optional"; }
  | TOK_VARIANT    { $$ = "variant"; }
  ;

magic_identifiers2:
    TOK_VOID_ID       { $$ = "void"; }
  | TOK_MIXED_ID      { $$ = "mixed"; }
  | TOK_ARRAY_ID      { $$ = "array"; }
  | TOK_MAPPING_ID    { $$ = "mapping"; }
  | TOK_MULTISET_ID   { $$ = "multiset"; }
  | TOK_OBJECT_ID     { $$ = "object"; }
  | TOK_FUNCTION_ID   { $$ = "function"; }
  | TOK_PROGRAM_ID    { $$ = "program"; }
  | TOK_STRING_ID     { $$ = "string"; }
  | TOK_FLOAT_ID      { $$ = "float"; }
  | TOK_INT_ID        { $$ = "int"; }
  ;

magic_identifiers3:
    TOK_IF         { $$ = "if"; }
  | TOK_DO         { $$ = "do"; }
  | TOK_FOR        { $$ = "for"; }
  | TOK_WHILE      { $$ = "while"; }
  | TOK_ELSE       { $$ = "else"; }
  | TOK_FOREACH    { $$ = "foreach"; }
  | TOK_CATCH      { $$ = "catch"; }
  | TOK_GAUGE      { $$ = "gauge"; }
  | TOK_CLASS      { $$ = "class"; }
  | TOK_BREAK      { $$ = "break"; }
  | TOK_CASE       { $$ = "case"; }
  | TOK_CONSTANT   { $$ = "constant"; }
  | TOK_CONTINUE   { $$ = "continue"; }
  | TOK_DEFAULT    { $$ = "default"; }
  | TOK_IMPORT     { $$ = "import"; }
  | TOK_INHERIT    { $$ = "inherit"; }
  | TOK_LAMBDA     { $$ = "lambda"; }
  | TOK_PREDEF     { $$ = "predef"; }
  | TOK_RETURN     { $$ = "return"; }
  | TOK_SSCANF     { $$ = "sscanf"; }
  | TOK_SWITCH     { $$ = "switch"; }
  | TOK_TYPEOF     { $$ = "typeof"; }
  ;

magic_identifiers: magic_identifiers1 | magic_identifiers2 | magic_identifiers3 ;

magic_identifier: TOK_IDENTIFIER 
  | magic_identifiers
  {
    struct pike_string *tmp=make_shared_string($1);
    $$=mkstrnode(tmp);
    free_string(tmp);
  }
  ;

modifiers: modifier_list
 {
   $$=Pike_compiler->current_modifiers=$1 | (lex.pragmas & ID_MODIFIER_MASK);
 }
 ;

modifier_list: /* empty */ { $$ = 0; }
  | modifier modifier_list { $$ = $1 | $2; }
  ;

optional_stars: optional_stars '*' { $$=$1 + 1; }
  | /* empty */ { $$=0; }
  ;

cast: '(' type ')'
    {
      struct pike_string *s=compiler_pop_type();
      $$=mkstrnode(s);
      free_string(s);
    }
    ;

soft_cast: '[' type ']'
    {
      struct pike_string *s=compiler_pop_type();
      $$=mkstrnode(s);
      free_string(s);
    }
    ;

type6: type | identifier_type ;
  
type: type '*'
  {
    if (Pike_compiler->compiler_pass == 2) {
       yywarning("The *-syntax in types is obsolete. Use array instead.");
    }
    push_type(T_ARRAY);
  }
  | type2
  ;

type7: type7 '*'
  {
    if (Pike_compiler->compiler_pass == 2) {
      yywarning("The *-syntax in types is obsolete. Use array instead.");
    }
    push_type(T_ARRAY);
  }
  | type4
  ;

simple_type: type4
  {
    struct pike_string *s=compiler_pop_type();
    $$=mkstrnode(s);
#ifdef PIKE_DEBUG
    if ($$->u.sval.u.string != s) {
      fatal("mkstrnode(%p:\"%s\") created node with %p:\"%s\"\n",
	    s, s->str, $$->u.sval.u.string, $$->u.sval.u.string->str);
    }
#endif /* PIKE_DEBUG */
    free_string(s);
  }
  ;

simple_type2: type2
  {
    struct pike_string *s=compiler_pop_type();
    $$=mkstrnode(s);
#ifdef PIKE_DEBUG
    if ($$->u.sval.u.string != s) {
      fatal("mkstrnode(%p:\"%s\") created node with %p:\"%s\"\n",
	    s, s->str, $$->u.sval.u.string, $$->u.sval.u.string->str);
    }
#endif /* PIKE_DEBUG */
    free_string(s);
  }
  ;

simple_identifier_type: identifier_type
  {
    struct pike_string *s=compiler_pop_type();
    $$=mkstrnode(s);
#ifdef PIKE_DEBUG
    if ($$->u.sval.u.string != s) {
      fatal("mkstrnode(%p:\"%s\") created node with %p:\"%s\"\n",
	    s, s->str, $$->u.sval.u.string, $$->u.sval.u.string->str);
    }
#endif /* PIKE_DEBUG */
    free_string(s);
  }
  ;

identifier_type: idents
  { 
    resolv_constant($1);

    if (Pike_sp[-1].type == T_TYPE) {
      /* "typedef" */
      push_finished_type(Pike_sp[-1].u.string);
    } else {
      /* object type */
      struct program *p = NULL;

      if (Pike_sp[-1].type == T_OBJECT) {
	if(!Pike_sp[-1].u.object->prog)
	{
	  pop_stack();
	  push_int(0);
	  yyerror("Destructed object used as program identifier.");
	}else{
	  extern void f_object_program(INT32);
	  f_object_program(1);
	}
      }

      switch(Pike_sp[-1].type) {
      case T_FUNCTION:
	if((p = program_from_function(Pike_sp-1)))
	  break;
      
      default:
	if (Pike_compiler->compiler_pass!=1)
	  yyerror("Illegal program identifier.");
	pop_stack();
	push_int(0);
	break;
	
      case T_PROGRAM:
	p = Pike_sp[-1].u.program;
	break;
      }

      if(p) {
	push_type_int(p->id);
      }else{
	push_type_int(0);
      }
      push_type(0);
      push_type(T_OBJECT);
    }
    pop_stack();
    free_node($1);
  }
  ;

type4: type4 '|' type8 { push_type(T_OR); }
  | type8
  ;

type2: type2 '|' type3 { push_type(T_OR); }
  | type3 
  ;

type3: TOK_INT_ID  opt_int_range    { push_type(T_INT); }
  | TOK_FLOAT_ID    { push_type(T_FLOAT); }
  | TOK_PROGRAM_ID  { push_type(T_PROGRAM); }
  | TOK_VOID_ID     { push_type(T_VOID); }
  | TOK_MIXED_ID    { push_type(T_MIXED); }
  | TOK_STRING_ID { push_type(T_STRING); }
  | TOK_OBJECT_ID   opt_object_type { push_type(T_OBJECT); }
  | TOK_MAPPING_ID opt_mapping_type { push_type(T_MAPPING); }
  | TOK_ARRAY_ID opt_array_type { push_type(T_ARRAY); }
  | TOK_MULTISET_ID opt_array_type { push_type(T_MULTISET); }
  | TOK_FUNCTION_ID opt_function_type { push_type(T_FUNCTION); }
  ;

type8: type3 | identifier_type ;

number_or_maxint: /* Empty */
  {
    $$ = mkintnode(MAX_INT32);
  }
  | TOK_NUMBER
  | '-' TOK_NUMBER
  {
#ifdef PIKE_DEBUG
    if (($2->token != F_CONSTANT) || ($2->u.sval.type != T_INT)) {
      fatal("Unexpected number in negative int-range.\n");
    }
#endif /* PIKE_DEBUG */
    $$ = mkintnode(-($2->u.sval.u.integer));
    free_node($2);
  }
  ;

number_or_minint: /* Empty */
  {
    $$ = mkintnode(MIN_INT32);
  }
  | TOK_NUMBER
  | '-' TOK_NUMBER
  {
#ifdef PIKE_DEBUG
    if (($2->token != F_CONSTANT) || ($2->u.sval.type != T_INT)) {
      fatal("Unexpected number in negative int-range.\n");
    }
#endif /* PIKE_DEBUG */
    $$ = mkintnode(-($2->u.sval.u.integer));
    free_node($2);
  }
  ;

expected_dot_dot: TOK_DOT_DOT
  | TOK_DOT_DOT_DOT
  {
    yyerror("Elipsis ('...') where range indicator ('..') expected.");
  }
  ;

opt_int_range: /* Empty */
  {
    push_type_int(MAX_INT32);
    push_type_int(MIN_INT32);
  }
  | '(' number_or_minint expected_dot_dot number_or_maxint ')'
  {
    /* FIXME: Check that $4 is >= $2. */
    if($4->token == F_CONSTANT && $4->u.sval.type == T_INT)
    {
      push_type_int($4->u.sval.u.integer);
    }else{
      push_type_int(MAX_INT32);
    }

    if($2->token == F_CONSTANT && $2->u.sval.type == T_INT)
    {
      push_type_int($2->u.sval.u.integer);
    }else{
      push_type_int(MIN_INT32);
    }

    free_node($2);
    free_node($4);
  }
  ;

opt_object_type:  /* Empty */ { push_type_int(0); push_type(0); }
  | '(' program_ref ')'
  {
    /* NOTE: On entry, there are two items on the stack:
     *   Pike_sp-2:	Name of the program reference (string).
     *   Pike_sp-1:	The resolved program (program|function|zero).
     */
    struct program *p=program_from_svalue(Pike_sp-1);
    if(p)
    {
      push_type_int(p->id);
    }else{
      if (Pike_compiler->compiler_pass!=1) {
	if ((Pike_sp[-2].type == T_STRING) && (Pike_sp[-2].u.string->len > 0) &&
	    (Pike_sp[-2].u.string->len < 256)) {
	  my_yyerror("Not a valid program specifier: '%s'",
		     Pike_sp[-2].u.string->str);
	} else {
	  yyerror("Not a valid program specifier.");
	}
      }
      push_type_int(0);
    }
    pop_n_elems(2);
    push_type(0);
  }
  ;

opt_function_type: '('
  {
    type_stack_mark();
    type_stack_mark();
  }
  function_type_list optional_dot_dot_dot ':'
  {
    if ($4)
    {
      if ($3) {
	push_type(T_MANY);
	type_stack_reverse();
      } else {
	/* function_type_list ends with a comma, or is empty.
	 * FIXME: Should this be a syntax error or not?
	 */
	if (Pike_compiler->compiler_pass == 1) {
	  yyerror("Missing type before ... .");
	}
	type_stack_reverse();
	push_type(T_MANY);
	push_type(T_MIXED);
      }
    }else{
      type_stack_reverse();
      push_type(T_MANY);
      push_type(T_VOID);
    }
    type_stack_mark();
  }
  type7 ')'
  {
    type_stack_reverse();
    type_stack_reverse();
  }
  | /* empty */
  {
   push_type(T_MIXED);
   push_type(T_VOID);
   push_type(T_OR);

   push_type(T_ZERO);
   push_type(T_VOID);
   push_type(T_OR);

   push_type(T_MANY);
  }
  ;

function_type_list: /* Empty */ optional_comma { $$=0; }
  | function_type_list2 optional_comma { $$=!$2; }
  ;

function_type_list2: type7 { $$=1; }
  | function_type_list2 ','
  {
    type_stack_reverse();
    type_stack_mark();
  }
  type7
  ;

opt_array_type: '(' type7 ')'
  |  { push_type(T_MIXED); }
  ;

opt_mapping_type: '('
  { 
    type_stack_mark();
    type_stack_mark();
  }
  type7 ':'
  { 
    type_stack_reverse();
    type_stack_mark();
  }
  type7
  { 
    type_stack_reverse();
    type_stack_reverse();
  }
  ')'
  | /* empty */ 
  {
    push_type(T_MIXED);
    push_type(T_MIXED);
  }
  ;



name_list: new_name
  | name_list ',' new_name
  ;

new_name: optional_stars TOK_IDENTIFIER
  {
    struct pike_string *type;
    push_finished_type(Pike_compiler->compiler_frame->current_type);
    if ($1 && (Pike_compiler->compiler_pass == 2)) {
      yywarning("The *-syntax in types is obsolete. Use array instead.");
    }
    while($1--) push_type(T_ARRAY);
    type=compiler_pop_type();
    define_variable($2->u.sval.u.string, type,
		    Pike_compiler->current_modifiers);
    free_string(type);
    free_node($2);
  }
  | optional_stars bad_identifier {}
  | optional_stars TOK_IDENTIFIER '='
  {
    struct pike_string *type;
    push_finished_type(Pike_compiler->compiler_frame->current_type);
    if ($1 && (Pike_compiler->compiler_pass == 2)) {
      yywarning("The *-syntax in types is obsolete. Use array instead.");
    }
    while($1--) push_type(T_ARRAY);
    type=compiler_pop_type();
    if ((Pike_compiler->current_modifiers & ID_EXTERN) &&
	(Pike_compiler->compiler_pass == 1)) {
      yywarning("Extern declared variable has initializer.");
    }
    $<number>$=define_variable($2->u.sval.u.string, type,
			       Pike_compiler->current_modifiers & (~ID_EXTERN));
    free_string(type);
  }
  expr0
  {
    Pike_compiler->init_node=mknode(F_COMMA_EXPR,Pike_compiler->init_node,
		     mkcastnode(void_type_string,
				mknode(F_ASSIGN,$5,
				       mkidentifiernode($<number>4))));
    free_node($2);
  }
  | optional_stars TOK_IDENTIFIER '=' error
  {
    free_node($2);
  }
  | optional_stars TOK_IDENTIFIER '=' TOK_LEX_EOF
  {
    yyerror("Unexpected end of file in variable definition.");
    free_node($2);
  }
  | optional_stars bad_identifier '=' expr0
  {
    free_node($4);
  }
  ;


new_local_name: optional_stars TOK_IDENTIFIER
  {    
    push_finished_type($<n>0->u.sval.u.string);
    if ($1 && (Pike_compiler->compiler_pass == 2)) {
      yywarning("The *-syntax in types is obsolete. Use array instead.");
    }
    while($1--) push_type(T_ARRAY);
    add_local_name($2->u.sval.u.string, compiler_pop_type(),0);
    $$=mknode(F_ASSIGN,mkintnode(0),mklocalnode(islocal($2->u.sval.u.string),0));
    free_node($2);
  }
  | optional_stars bad_identifier { $$=0; }
  | optional_stars TOK_IDENTIFIER '=' expr0 
  {
    push_finished_type($<n>0->u.sval.u.string);
    if ($1 && (Pike_compiler->compiler_pass == 2)) {
      yywarning("The *-syntax in types is obsolete. Use array instead.");
    }
    while($1--) push_type(T_ARRAY);
    add_local_name($2->u.sval.u.string, compiler_pop_type(),0);
    $$=mknode(F_ASSIGN,$4,mklocalnode(islocal($2->u.sval.u.string),0));
    free_node($2);
  }
  | optional_stars bad_identifier '=' expr0
  {
    free_node($4);
    $$=0;
  }
  | optional_stars TOK_IDENTIFIER '=' error
  {
    free_node($2);
    /* No yyerok here since we aren't done yet. */
    $$=0;
  }
  | optional_stars TOK_IDENTIFIER '=' TOK_LEX_EOF
  {
    yyerror("Unexpected end of file in local variable definition.");
    free_node($2);
    /* No yyerok here since we aren't done yet. */
    $$=0;
  }
  ;

new_local_name2: TOK_IDENTIFIER
  {
    add_ref($<n>0->u.sval.u.string);
    add_local_name($1->u.sval.u.string, $<n>0->u.sval.u.string, 0);
    $$=mknode(F_ASSIGN,mkintnode(0),mklocalnode(islocal($1->u.sval.u.string),0));
    free_node($1);
  }
  | bad_identifier { $$=0; }
  | TOK_IDENTIFIER '=' safe_expr0
  {
    add_ref($<n>0->u.sval.u.string);
    add_local_name($1->u.sval.u.string, $<n>0->u.sval.u.string, 0);
    $$=mknode(F_ASSIGN,$3, mklocalnode(islocal($1->u.sval.u.string),0));
    free_node($1);
  }
  | bad_identifier '=' safe_expr0 { $$=$3; }
  ;

block:'{'
  {
    $<number>1=Pike_compiler->num_used_modules;
    $<number>$=Pike_compiler->compiler_frame->current_number_of_locals;
  } 
  {
    /* Trick to store more than one number on compiler stack - Hubbe */
    $<number>$=Pike_compiler->compiler_frame->last_block_level;

    if($<number>$ == -1) /* if 'first block' */
      Pike_compiler->compiler_frame->last_block_level=0; /* all variables */
    else
      Pike_compiler->compiler_frame->last_block_level=$<number>2;
  }
  statements end_block
  {
    unuse_modules(Pike_compiler->num_used_modules - $<number>1);
    pop_local_variables($<number>2);
    Pike_compiler->compiler_frame->last_block_level=$<number>3;
    $$=$4;
  }
  ;

end_block: '}'
  | TOK_LEX_EOF
  {
    yyerror("Missing '}'.");
    yyerror("Unexpected end of file.");
  }
  ;

failsafe_block: block
              | error { $$=0; }
              | TOK_LEX_EOF { yyerror("Unexpected end of file."); $$=0; }
              ;


local_name_list: new_local_name
  | local_name_list ',' { $<n>$=$<n>0; } new_local_name { $$=mknode(F_COMMA_EXPR,mkcastnode(void_type_string,$1),$4); }
  ;

local_name_list2: new_local_name2
  | local_name_list2 ',' { $<n>$=$<n>0; } new_local_name { $$=mknode(F_COMMA_EXPR,mkcastnode(void_type_string,$1),$4); }
  ;


local_constant_name: TOK_IDENTIFIER '=' safe_expr0
  {
    struct pike_string *type;

    /* Ugly hack to make sure that $3 is optimized */
    {
      int tmp=Pike_compiler->compiler_pass;
      $3=mknode(F_COMMA_EXPR,$3,0);
      Pike_compiler->compiler_pass=tmp;
      type=$3->u.node.a->type;
    }

    if(!is_const($3))
    {
      if(Pike_compiler->compiler_pass==2)
	yyerror("Constant definition is not constant.");
    }else{
      ptrdiff_t tmp=eval_low($3);
      if(tmp < 1)
      {
	yyerror("Error in constant definition.");
      }else{
	pop_n_elems(DO_NOT_WARN((INT32)(tmp - 1)));
	if($3) free_node($3);
	$3=mksvaluenode(Pike_sp-1);
	type=$3->type;
	pop_stack();
      }
    }
    if(!type) type=mixed_type_string;
    add_ref(type);
    low_add_local_name(Pike_compiler->compiler_frame, /*->previous,*/
		       $1->u.sval.u.string,
		       type, $3);
    free_node($1);
  }
  | bad_identifier '=' safe_expr0 { if ($3) free_node($3); }
  | error '=' safe_expr0 { if ($3) free_node($3); }
  ;

local_constant_list: local_constant_name
  | local_constant_list ',' local_constant_name
  ;

local_constant: TOK_CONSTANT local_constant_list ';'
  | TOK_CONSTANT error ';' { yyerrok; }
  | TOK_CONSTANT error TOK_LEX_EOF
  {
    yyerror("Missing ';'.");
    yyerror("Unexpected end of file.");
  }
  | TOK_CONSTANT error '}' { yyerror("Missing ';'."); }
  ;


statements: { $$=0; }
  | statements statement
  {
    $$=mknode(F_COMMA_EXPR,$1,mkcastnode(void_type_string,$2));
  }
  ;


normal_label_statement: unused2 ';'
  | import { $$=0; }
  | cond
  | return expected_semicolon
  | local_constant { $$=0; }
  | block
  | break expected_semicolon
  | continue expected_semicolon
  | error ';' { reset_type_stack(); $$=0; yyerrok; }
  | error TOK_LEX_EOF
  {
    reset_type_stack();
    yyerror("Missing ';'.");
    yyerror("Unexpected end of file.");
    $$=0;
  }
  | error '}'
  {
    reset_type_stack();
    yyerror("Missing ';'.");
/*    yychar = '}'; */	/* Put the '}' back on the input stream. */
    $$=0;
  }
  | ';' { $$=0; } 
  ;

statement: normal_label_statement
  {
    Pike_compiler->compiler_frame->opt_flags &= ~OPT_CUSTOM_LABELS;
  }
  | while
  | do
  | for
  | foreach
  | switch
  | case
  | default
  | labeled_statement
  ;

labeled_statement: TOK_IDENTIFIER
  {
    $<number>$ = lex.current_line;
    Pike_compiler->compiler_frame->opt_flags &= ~OPT_CUSTOM_LABELS;
  }
  ':' statement
  {
    $$ = mknode(Pike_compiler->compiler_frame->opt_flags & OPT_CUSTOM_LABELS ?
		F_CUSTOM_STMT_LABEL : F_NORMAL_STMT_LABEL,
		$1, $4);

    /* FIXME: This won't be correct if the node happens to be shared.
     * That's an issue to be solved with shared nodes in general,
     * though. */
    $$->line_number = $<number>2;
  }
  ;

optional_label: TOK_IDENTIFIER
  | /* empty */ {$$ = 0;}
  ;

break: TOK_BREAK optional_label { $$=mknode(F_BREAK,$2,0); } ;
default: TOK_DEFAULT ':'  { $$=mknode(F_DEFAULT,0,0); }
  | TOK_DEFAULT
  {
    $$=mknode(F_DEFAULT,0,0); yyerror("Expected ':' after default.");
  }
  ;

continue: TOK_CONTINUE optional_label { $$=mknode(F_CONTINUE,$2,0); } ;

push_compiler_frame1: /* empty */
  {
    push_compiler_frame(SCOPE_LOCAL);
  }
  ;

lambda: TOK_LAMBDA push_compiler_frame1
  {
    debug_malloc_touch(Pike_compiler->compiler_frame->current_return_type);
    if(Pike_compiler->compiler_frame->current_return_type)
      free_string(Pike_compiler->compiler_frame->current_return_type);
    copy_shared_string(Pike_compiler->compiler_frame->current_return_type,any_type_string);
  }
  func_args
  {
    $<number>$ = Pike_compiler->varargs;
    Pike_compiler->varargs = 0;
  }
  failsafe_block
  {
    struct pike_string *type;
    char buf[40];
    int f,e;
    struct pike_string *name;
    
    debug_malloc_touch($6);
    $6=mknode(F_COMMA_EXPR,$6,mknode(F_RETURN,mkintnode(0),0));
    type=find_return_type($6);

    if(type) {
      push_finished_type(type);
      free_string(type);
    } else
      push_type(T_MIXED);
    
    e=$4-1;
    if($<number>5)
    {
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
      e--;
      pop_type_stack();
    }else{
      push_type(T_VOID);
    }
    Pike_compiler->varargs=0;
    push_type(T_MANY);
    for(; e>=0; e--)
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
    
    push_type(T_FUNCTION);
    
    type=compiler_pop_type();

    sprintf(buf,"__lambda_%ld_%ld",
	    (long)Pike_compiler->new_program->id,
	    (long)(Pike_compiler->local_class_counter++ & 0xffffffff)); /* OSF/1 cc bug. */
    name=make_shared_string(buf);

#ifdef LAMBDA_DEBUG
    fprintf(stderr, "%d: LAMBDA: %s 0x%08lx 0x%08lx\n",
	    Pike_compiler->compiler_pass, buf, (long)Pike_compiler->new_program->id, Pike_compiler->local_class_counter-1);
#endif /* LAMBDA_DEBUG */
    
    f=dooptcode(name,
		$6,
		type,
		ID_STATIC | ID_PRIVATE | ID_INLINE);

    if(Pike_compiler->compiler_frame->lexical_scope & SCOPE_SCOPED) {
      $$ = mktrampolinenode(f);
    } else {
      $$ = mkidentifiernode(f);
    }
    free_string(name);
    free_string(type);
    pop_compiler_frame();
  }
  | TOK_LAMBDA push_compiler_frame1 error
  {
    pop_compiler_frame();
    $$ = mkintnode(0);
  }
  ;

local_function: TOK_IDENTIFIER push_compiler_frame1 func_args 
  {
    char buf[40];
    struct pike_string *name,*type;
    int id,e;
    node *n;
    struct identifier *i=0;

    debug_malloc_touch(Pike_compiler->compiler_frame->current_return_type);
    if(Pike_compiler->compiler_frame->current_return_type)
      free_string(Pike_compiler->compiler_frame->current_return_type);
    copy_shared_string(Pike_compiler->compiler_frame->current_return_type,
		       $<n>0->u.sval.u.string);


    /***/
    push_finished_type(Pike_compiler->compiler_frame->current_return_type);
    
    e=$3-1;
    if(Pike_compiler->varargs)
    {
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
      e--;
      Pike_compiler->varargs=0;
      pop_type_stack();
    }else{
      push_type(T_VOID);
    }
    push_type(T_MANY);
    for(; e>=0; e--)
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
    
    push_type(T_FUNCTION);
    
    type=compiler_pop_type();
    /***/

    sprintf(buf,"__lambda_%ld_%ld",
	    (long)Pike_compiler->new_program->id,
	    (long)(Pike_compiler->local_class_counter++ & 0xffffffff)); /* OSF/1 cc bug. */

#ifdef LAMBDA_DEBUG
    fprintf(stderr, "%d: LAMBDA: %s 0x%08lx 0x%08lx\n",
	    Pike_compiler->compiler_pass, buf, (long)Pike_compiler->new_program->id, Pike_compiler->local_class_counter-1);
#endif /* LAMBDA_DEBUG */

    name=make_shared_string(buf);

    id=define_function(name,
		       type,
		       0,
		       IDENTIFIER_PIKE_FUNCTION,
		       0,
		       OPT_SIDE_EFFECT|OPT_EXTERNAL_DEPEND);
    n=0;
#if 0
    if(Pike_compiler->compiler_pass > 1 &&
       (i=ID_FROM_INT(Pike_compiler->new_program, id)))
      if(!(i->identifier_flags & IDENTIFIER_SCOPED))
	n = mkidentifiernode(id);
#endif

    low_add_local_name(Pike_compiler->compiler_frame->previous,
		       $1->u.sval.u.string, type, n);

    $<number>$=id;
    free_string(name);
  }
  failsafe_block
  {
    int localid;
    struct identifier *i=ID_FROM_INT(Pike_compiler->new_program, $<number>4);

    $5=mknode(F_COMMA_EXPR,$5,mknode(F_RETURN,mkintnode(0),0));

    debug_malloc_touch($5);
    dooptcode(i->name,
	      $5,
	      i->type,
	      ID_STATIC | ID_PRIVATE | ID_INLINE);

    i->opt_flags = Pike_compiler->compiler_frame->opt_flags;

    pop_compiler_frame();
    free_node($1);

    /* WARNING: If the local function adds more variables we are screwed */
    /* WARNING2: if add_local_name stops adding local variables at the end,
     *           this has to be fixed.
     */

    localid=Pike_compiler->compiler_frame->current_number_of_locals-1;
    if(Pike_compiler->compiler_frame->variable[localid].def)
    {
      $$=copy_node(Pike_compiler->compiler_frame->variable[localid].def);
    }else{
      if(Pike_compiler->compiler_frame->lexical_scope & 
	 (SCOPE_SCOPE_USED | SCOPE_SCOPED))
      {
	$$ = mknode(F_ASSIGN, mktrampolinenode($<number>3),
		    mklocalnode(localid,0));
      }else{
	$$ = mknode(F_ASSIGN, mkidentifiernode($<number>3),
		    mklocalnode(localid,0));
      }
    }
  }
  | TOK_IDENTIFIER push_compiler_frame1 error
  {
    pop_compiler_frame();
    $$=mkintnode(0);
  }
  ;

local_function2: optional_stars TOK_IDENTIFIER push_compiler_frame1 func_args 
  {
    char buf[40];
    struct pike_string *name,*type;
    int id,e;
    node *n;
    struct identifier *i=0;

    /***/
    debug_malloc_touch(Pike_compiler->compiler_frame->current_return_type);
    
    push_finished_type($<n>0->u.sval.u.string);
    if ($1 && (Pike_compiler->compiler_pass == 2)) {
      yywarning("The *-syntax in types is obsolete. Use array instead.");
    }
    while($1--) push_type(T_ARRAY);

    if(Pike_compiler->compiler_frame->current_return_type)
      free_string(Pike_compiler->compiler_frame->current_return_type);
    Pike_compiler->compiler_frame->current_return_type=compiler_pop_type();

    /***/
    push_finished_type(Pike_compiler->compiler_frame->current_return_type);
    
    e=$4-1;
    if(Pike_compiler->varargs)
    {
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
      e--;
      Pike_compiler->varargs=0;
      pop_type_stack();
    }else{
      push_type(T_VOID);
    }
    push_type(T_MANY);
    for(; e>=0; e--)
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
    
    push_type(T_FUNCTION);
    
    type=compiler_pop_type();
    /***/


    sprintf(buf,"__lambda_%ld_%ld",
	    (long)Pike_compiler->new_program->id,
	    (long)(Pike_compiler->local_class_counter++ & 0xffffffff)); /* OSF/1 cc bug. */

#ifdef LAMBDA_DEBUG
    fprintf(stderr, "%d: LAMBDA: %s 0x%08lx 0x%08lx\n",
	    Pike_compiler->compiler_pass, buf, (long)Pike_compiler->new_program->id, Pike_compiler->local_class_counter-1);
#endif /* LAMBDA_DEBUG */

    name=make_shared_string(buf);


    id=define_function(name,
		       type,
		       0,
		       IDENTIFIER_PIKE_FUNCTION,
		       0,
		       OPT_SIDE_EFFECT|OPT_EXTERNAL_DEPEND);
    n=0;
#if 0
    if(Pike_compiler->compiler_pass > 1 &&
       (i=ID_FROM_INT(Pike_compiler->new_program, id)))
      if(!(i->identifier_flags & IDENTIFIER_SCOPED))
	n = mkidentifiernode(id);
#endif

    low_add_local_name(Pike_compiler->compiler_frame->previous,
		       $2->u.sval.u.string, type, n);
    $<number>$=id;
    free_string(name);
  }
  failsafe_block
  {
    int localid;
    struct identifier *i=ID_FROM_INT(Pike_compiler->new_program, $<number>5);

    debug_malloc_touch($6);
    $6=mknode(F_COMMA_EXPR,$6,mknode(F_RETURN,mkintnode(0),0));


    debug_malloc_touch($6);
    dooptcode(i->name,
	      $6,
	      i->type,
	      ID_STATIC | ID_PRIVATE | ID_INLINE);

    i->opt_flags = Pike_compiler->compiler_frame->opt_flags;

    pop_compiler_frame();
    free_node($2);

    /* WARNING: If the local function adds more variables we are screwed */
    /* WARNING2: if add_local_name stops adding local variables at the end,
     *           this has to be fixed.
     */

    localid=Pike_compiler->compiler_frame->current_number_of_locals-1;
    if(Pike_compiler->compiler_frame->variable[localid].def)
    {
      $$=copy_node(Pike_compiler->compiler_frame->variable[localid].def);
    }else{
      if(Pike_compiler->compiler_frame->lexical_scope & 
	 (SCOPE_SCOPE_USED | SCOPE_SCOPED))
      {
        $$ = mknode(F_ASSIGN, mktrampolinenode($<number>5),
	  mklocalnode(localid,0));
      }else{
        $$ = mknode(F_ASSIGN, mkidentifiernode($<number>5),
	  mklocalnode(localid,0));
      }
    }
  }
  | optional_stars TOK_IDENTIFIER push_compiler_frame1 error
  {
    pop_compiler_frame();
    free_node($2);
    $$=mkintnode(0);
  }
  ;

create_arg: modifiers type_or_error optional_stars TOK_IDENTIFIER
  {
    struct pike_string *type;

    push_finished_type(Pike_compiler->compiler_frame->current_type);
    if ($3 && (Pike_compiler->compiler_pass == 2)) {
      yywarning("The *-syntax in types is obsolete. Use array instead.");
    }
    while($3--) push_type(T_ARRAY);
    type=compiler_pop_type();

    if(islocal($4->u.sval.u.string) >= 0)
      my_yyerror("Variable '%s' appears twice in create argument list.",
		 $4->u.sval.u.string->str);

    /* Add the identifier both globally and locally. */
    define_variable($4->u.sval.u.string, type,
		    Pike_compiler->current_modifiers);
    add_local_name($4->u.sval.u.string, type, 0);

    /* free_string(type); */
    free_node($4);
    $$=0;
  }
  | modifiers type_or_error optional_stars bad_identifier { $$=0; }
  ;

create_arguments2: create_arg { $$ = 1; }
  | create_arguments2 ',' create_arg { $$ = $1 + 1; }
  | create_arguments2 ':' create_arg
  {
    yyerror("Unexpected ':' in create argument list.");
    $$ = $1 + 1;
  }
  ;

create_arguments: /* empty */ optional_comma { $$=0; }
  | create_arguments2 optional_comma
  ;

push_compiler_frame01: /* empty */
  {
    push_compiler_frame(SCOPE_LOCAL);
  }
  ;

optional_create_arguments: /* empty */ { $$ = 0; }
  | '(' push_compiler_frame01 create_arguments close_paren_or_missing
  {
    int e;
    node *create_code = NULL;
    struct pike_string *type = NULL;
    struct pike_string *create_string = NULL;
    int f;

    MAKE_CONSTANT_SHARED_STRING(create_string, "create");

    /* First: Deduce the type for the create() function. */

    push_type(T_VOID); /* Return type. */
    push_type(T_VOID); /* Not varargs. */
    push_type(T_MANY);
    for(e = $3-1; e>=0; e--)
    {
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
    }
    push_type(T_FUNCTION);

    type = compiler_pop_type();

    /* Second: Declare the function. */

    Pike_compiler->compiler_frame->current_function_number=
      define_function(create_string, type,
		      ID_STATIC, IDENTIFIER_PIKE_FUNCTION, 0,
		      OPT_SIDE_EFFECT);

    /* Third: Generate the initialization code.
     *
     * global_arg = [type]local_arg;
     * [,..]
     */

    for(e=0; e<$3; e++)
    {
      if(!Pike_compiler->compiler_frame->variable[e].name ||
	 !Pike_compiler->compiler_frame->variable[e].name->len)
      {
	my_yyerror("Missing name for argument %d.",e);
      } else {
	node *local_node = mklocalnode(e, 0);

	/* FIXME: Should probably use some other flag. */
	if ((runtime_options & RUNTIME_CHECK_TYPES) &&
	    (Pike_compiler->compiler_pass == 2) &&
	    (Pike_compiler->compiler_frame->variable[e].type !=
	     mixed_type_string)) {
	  /* fprintf(stderr, "Creating soft cast node for local #%d\n", e);*/

	  /* The following is needed to go around the optimization in
	   * mksoftcastnode().
	   */
	  free_string(local_node->type);
	  copy_shared_string(local_node->type, mixed_type_string);
	  
	  local_node = mksoftcastnode(Pike_compiler->compiler_frame->
				      variable[e].type, local_node);
	}
	create_code =
	  mknode(F_COMMA_EXPR, create_code,
		 mknode(F_ASSIGN, local_node,
			mkidentifiernode(isidentifier(Pike_compiler->
						      compiler_frame->
						      variable[e].name))));
      }
    }

    /* Fourth: Add a return 0; at the end. */

    create_code = mknode(F_COMMA_EXPR,
			 mknode(F_POP_VALUE, create_code, NULL),
			 mknode(F_RETURN, mkintnode(0), NULL));

    /* Fifth: Define the function. */

    f=dooptcode(create_string, check_node_hash(create_code),
		type, ID_STATIC);

#ifdef PIKE_DEBUG
    if(Pike_interpreter.recoveries &&
       Pike_sp-Pike_interpreter.evaluator_stack < Pike_interpreter.recoveries->stack_pointer)
      fatal("Stack error (underflow)\n");

    if(Pike_compiler->compiler_pass == 1 &&
       f!=Pike_compiler->compiler_frame->current_function_number)
      fatal("define_function screwed up! %d != %d\n",
	    f, Pike_compiler->compiler_frame->current_function_number);
#endif

    /* Done. */

    pop_compiler_frame();
    free_node($4);
    free_string(type);
    free_string(create_string);
  }
  ;

failsafe_program: '{' program end_block
                | error { yyerrok; }
                | TOK_LEX_EOF
                {
		  yyerror("End of file where program definition expected.");
		}
                ;

class: modifiers TOK_CLASS optional_identifier
  {
    extern int num_parse_error;
    int num_errors=Pike_compiler->num_parse_error;
    if(!$3)
    {
      struct pike_string *s;
      char buffer[42];
      sprintf(buffer,"__class_%ld_%ld",(long)Pike_compiler->new_program->id,
	      (long)Pike_compiler->local_class_counter++);
      s=make_shared_string(buffer);
      $3=mkstrnode(s);
      free_string(s);
      $1|=ID_STATIC | ID_PRIVATE | ID_INLINE;
    }
    /* fprintf(stderr, "LANGUAGE.YACC: CLASS start\n"); */
    if(Pike_compiler->compiler_pass==1)
    {
      if ($1 & ID_EXTERN) {
	yywarning("Extern declared class definition.");
      }
      low_start_new_program(0, $3->u.sval.u.string,
			    $1,
			    &$<number>$);

      /* fprintf(stderr, "Pass 1: Program %s has id %d\n",
	 $3->u.sval.u.string->str, Pike_compiler->new_program->id); */

      if(lex.current_file)
      {
	store_linenumber(lex.current_line, lex.current_file);
	debug_malloc_name(Pike_compiler->new_program, lex.current_file->str,
			  lex.current_line);
      }
    }else{
      int i;
      struct program *p;
      struct identifier *id;
      int tmp=Pike_compiler->compiler_pass;
      i=isidentifier($3->u.sval.u.string);
      if(i<0)
      {
	low_start_new_program(Pike_compiler->new_program,0,
			      $1,
			      &$<number>$);
	yyerror("Pass 2: program not defined!");
      }else{
	id=ID_FROM_INT(Pike_compiler->new_program, i);
	if(IDENTIFIER_IS_CONSTANT(id->identifier_flags))
	{
	  struct svalue *s;
	  s=&PROG_FROM_INT(Pike_compiler->new_program,i)->constants[id->func.offset].sval;
	  if(s->type==T_PROGRAM)
	  {
	    low_start_new_program(s->u.program,
				  $3->u.sval.u.string,
				  $1,
				  &$<number>$);

	    /* fprintf(stderr, "Pass 2: Program %s has id %d\n",
	       $3->u.sval.u.string->str, Pike_compiler->new_program->id); */

	  }else{
	    yyerror("Pass 2: constant redefined!");
	    low_start_new_program(Pike_compiler->new_program, 0,
				  $1,
				  &$<number>$);
	  }
	}else{
	  yyerror("Pass 2: class constant no longer constant!");
	  low_start_new_program(Pike_compiler->new_program, 0,
				$1,
				&$<number>$);
	}
      }
      Pike_compiler->compiler_pass=tmp;
    }
    Pike_compiler->num_parse_error=num_errors; /* Kluge to prevent gazillion error messages */
  }
  optional_create_arguments failsafe_program
  {
    struct program *p;
    if(Pike_compiler->compiler_pass == 1)
      p=end_first_pass(0);
    else
      p=end_first_pass(1);

    /* fprintf(stderr, "LANGUAGE.YACC: CLASS end\n"); */

    if(!p) {
      yyerror("Class definition failed.");
    }else{
      free_program(p);
    }

    $$=mkidentifiernode($<number>4);

    free_node($3);
    check_tree($$,0);
  }
  ;

cond: TOK_IF
  {
    $<number>$=Pike_compiler->compiler_frame->current_number_of_locals;
  }
  {
    /* Trick to store more than one number on compiler stack - Hubbe */
    $<number>$=Pike_compiler->compiler_frame->last_block_level;
    Pike_compiler->compiler_frame->last_block_level=$<number>2;
  }
  '(' safe_comma_expr end_cond statement optional_else_part
  {
    $$=mknode('?',$5,mknode(':',$7,$8));
    $$->line_number=$1;
    $$=mkcastnode(void_type_string,$$);
    $$->line_number=$1;
    pop_local_variables($<number>2);
    Pike_compiler->compiler_frame->last_block_level=$<number>3;
  }
  ;

end_cond: ')'
  | '}' { yyerror("Missing ')'."); }
  | TOK_LEX_EOF
  {
    yyerror("Missing ')'.");
    yyerror("Unexpected end of file.");
  }
  ;

optional_else_part: { $$=0; }
  | TOK_ELSE statement { $$=$2; }
  ;      

safe_lvalue: lvalue
  | error { $$=0; }
  ;

safe_expr0: expr0
  | TOK_LEX_EOF { yyerror("Unexpected end of file."); $$=0; }
  | error { $$=0; }
  ;

foreach: TOK_FOREACH
  {
    $<number>$=Pike_compiler->compiler_frame->current_number_of_locals;
  }
  {
    /* Trick to store more than one number on compiler stack - Hubbe */
    $<number>$=Pike_compiler->compiler_frame->last_block_level;
    Pike_compiler->compiler_frame->last_block_level=$<number>2;
  }
  '(' expr0 ',' safe_lvalue end_cond statement
  {
    if ($7) {
      $$=mknode(F_FOREACH, mknode(F_VAL_LVAL,$5,$7),$9);
      $$->line_number=$1;
    } else {
      /* Error in lvalue */
      free_node($5);
      $$=$9;
    }
    pop_local_variables($<number>2);
    Pike_compiler->compiler_frame->last_block_level=$<number>3;
    Pike_compiler->compiler_frame->opt_flags |= OPT_CUSTOM_LABELS;
  }
  ;

do: TOK_DO statement TOK_WHILE '(' safe_comma_expr end_cond expected_semicolon
  {
    $$=mknode(F_DO,$2,$5);
    $$->line_number=$1;
    Pike_compiler->compiler_frame->opt_flags |= OPT_CUSTOM_LABELS;
  }
  | TOK_DO statement TOK_WHILE TOK_LEX_EOF
  {
    $$=0;
    yyerror("Missing '(' in do-while loop.");
    yyerror("Unexpected end of file.");
  }
  | TOK_DO statement TOK_LEX_EOF
  {
    $$=0;
    yyerror("Missing 'while' in do-while loop.");
    yyerror("Unexpected end of file.");
  }
  ;

expected_semicolon: ';'
  | TOK_LEX_EOF
  {
    yyerror("Missing ';'.");
    yyerror("Unexpected end of file.");
  }
  ;

for: TOK_FOR
  {
    $<number>$=Pike_compiler->compiler_frame->current_number_of_locals;
  }
  {
    /* Trick to store more than one number on compiler stack - Hubbe */
    $<number>$=Pike_compiler->compiler_frame->last_block_level;
    Pike_compiler->compiler_frame->last_block_level=$<number>2;
  }
  '(' unused expected_semicolon for_expr expected_semicolon unused end_cond
  statement
  {
    int i=lex.current_line;
    lex.current_line=$1;
    $$=mknode(F_COMMA_EXPR, mkcastnode(void_type_string,$5),
	      mknode(F_FOR,$7,mknode(':',$11,$9)));
    lex.current_line=i;
    pop_local_variables($<number>2);
    Pike_compiler->compiler_frame->last_block_level=$<number>3;
    Pike_compiler->compiler_frame->opt_flags |= OPT_CUSTOM_LABELS;
  }
  ;


while:  TOK_WHILE
  {
    $<number>$=Pike_compiler->compiler_frame->current_number_of_locals;
  }
  {
    /* Trick to store more than one number on compiler stack - Hubbe */
    $<number>$=Pike_compiler->compiler_frame->last_block_level;
    Pike_compiler->compiler_frame->last_block_level=$<number>2;
  }
  '(' safe_comma_expr end_cond statement
  {
    int i=lex.current_line;
    lex.current_line=$1;
    $$=mknode(F_FOR,$5,mknode(':',$7,NULL));
    lex.current_line=i;
    pop_local_variables($<number>2);
    Pike_compiler->compiler_frame->last_block_level=$<number>3;
    Pike_compiler->compiler_frame->opt_flags |= OPT_CUSTOM_LABELS;
  }
  ;

for_expr: /* EMPTY */ { $$=mkintnode(1); }
  | safe_comma_expr
  ;

switch:	TOK_SWITCH
  {
    $<number>$=Pike_compiler->compiler_frame->current_number_of_locals;
  }
  {
    /* Trick to store more than one number on compiler stack - Hubbe */
    $<number>$=Pike_compiler->compiler_frame->last_block_level;
    Pike_compiler->compiler_frame->last_block_level=$<number>2;
  }
  '(' safe_comma_expr end_cond statement
  {
    $$=mknode(F_SWITCH,$5,$7);
    $$->line_number=$1;
    pop_local_variables($<number>2);
    Pike_compiler->compiler_frame->last_block_level=$<number>3;
  }
  ;

case: TOK_CASE safe_comma_expr expected_colon
  {
    $$=mknode(F_CASE,$2,0);
  }
  | TOK_CASE safe_comma_expr expected_dot_dot optional_comma_expr expected_colon
  {
     $$=mknode(F_CASE,$4?$2:0,$4?$4:$2);
  }
  ;

expected_colon: ':'
  | ';'
  {
    yyerror("Missing ':'.");
  }
  | '}'
  {
    yyerror("Missing ':'.");
  }
  | TOK_LEX_EOF
  {
    yyerror("Missing ':'.");
    yyerror("Unexpected end of file.");
  }
  ;

return: TOK_RETURN
  {
    if(!TEST_COMPAT(0,6) &&
       !match_types(Pike_compiler->compiler_frame->current_return_type,
		    void_type_string))
    {
      yyerror("Must return a value for a non-void function.");
    }
    $$=mknode(F_RETURN,mkintnode(0),0);
  }
  | TOK_RETURN safe_comma_expr
  {
    $$=mknode(F_RETURN,$2,0);
  }
  ;
	
unused: { $$=0; }
  | safe_comma_expr { $$=mkcastnode(void_type_string,$1);  }
  ;

unused2: comma_expr { $$=mkcastnode(void_type_string,$1);  } ;

optional_comma_expr: { $$=0; }
  | safe_comma_expr
  ;

safe_comma_expr: comma_expr
  | error { $$=0; }
  ;

comma_expr: comma_expr2
  | simple_type2 local_name_list { $$=$2; free_node($1); }
  | simple_identifier_type local_name_list2 { $$=$2; free_node($1); }
  | simple_identifier_type local_function { $$=$2; free_node($1); }
  | simple_type2 local_function2 { $$=$2; free_node($1); }
  ;
          

comma_expr2: expr0
  | comma_expr2 ',' expr0
  {
    $$ = mknode(F_COMMA_EXPR,mkcastnode(void_type_string,$1),$3); 
  }
  ;

expr00: expr0
      | '@' expr0 { $$=mknode(F_PUSH_ARRAY,$2,0); };

expr0: expr01
  | expr4 '=' expr0  { $$=mknode(F_ASSIGN,$3,$1); }
  | expr4 '=' error { $$=$1; reset_type_stack(); yyerrok; }
  | bad_expr_ident '=' expr0 { $$=$3; }
  | '[' low_lvalue_list ']' '=' expr0  { $$=mknode(F_ASSIGN,$5,mknode(F_ARRAY_LVALUE,$2,0)); }
  | expr4 assign expr0  { $$=mknode($2,$1,$3); }
  | expr4 assign error { $$=$1; reset_type_stack(); yyerrok; }
  | bad_expr_ident assign expr0 { $$=$3; }
  | '[' low_lvalue_list ']' assign expr0  { $$=mknode($4,mknode(F_ARRAY_LVALUE,$2,0),$5); }
  | '[' low_lvalue_list ']' error { $$=$2; reset_type_stack(); yyerrok; }
/*  | error { $$=0; reset_type_stack(); } */
  ;

expr01: expr1
  | expr1 '?' expr01 ':' expr01 { $$=mknode('?',$1,mknode(':',$3,$5)); }
  ;

assign: TOK_AND_EQ { $$=F_AND_EQ; }
  | TOK_OR_EQ  { $$=F_OR_EQ; }
  | TOK_XOR_EQ { $$=F_XOR_EQ; }
  | TOK_LSH_EQ { $$=F_LSH_EQ; }
  | TOK_RSH_EQ { $$=F_RSH_EQ; }
  | TOK_ADD_EQ { $$=F_ADD_EQ; }
  | TOK_SUB_EQ { $$=F_SUB_EQ; }
  | TOK_MULT_EQ{ $$=F_MULT_EQ; }
  | TOK_MOD_EQ { $$=F_MOD_EQ; }
  | TOK_DIV_EQ { $$=F_DIV_EQ; }
  ;

optional_comma: { $$=0; } | ',' { $$=1; };

expr_list: { $$=0; }
  | expr_list2 optional_comma
  ;
         

expr_list2: expr00
  | expr_list2 ',' expr00 { $$=mknode(F_ARG_LIST,$1,$3); }
  ;

m_expr_list: { $$=0; }
  | m_expr_list2 optional_comma
  ;

m_expr_list2: assoc_pair
  | m_expr_list2 ',' assoc_pair
  {
    if ($3) {
      $$=mknode(F_ARG_LIST,$1,$3);
    } else {
      /* Error in assoc_pair */
      $$=$1;
    }
  }
  | m_expr_list2 ',' error
  ;

assoc_pair:  expr0 expected_colon expr0 { $$=mknode(F_ARG_LIST,$1,$3); }
  | expr0 expected_colon error { free_node($1); $$=0; }
  ;

expr1: expr2
  | expr1 TOK_LOR expr1  { $$=mknode(F_LOR,$1,$3); }
  | expr1 TOK_LAND expr1 { $$=mknode(F_LAND,$1,$3); }
  | expr1 '|' expr1    { $$=mkopernode("`|",$1,$3); }
  | expr1 '^' expr1    { $$=mkopernode("`^",$1,$3); }
  | expr1 '&' expr1    { $$=mkopernode("`&",$1,$3); }
  | expr1 TOK_EQ expr1   { $$=mkopernode("`==",$1,$3); }
  | expr1 TOK_NE expr1   { $$=mkopernode("`!=",$1,$3); }
  | expr1 '>' expr1    { $$=mkopernode("`>",$1,$3); }
  | expr1 TOK_GE expr1   { $$=mkopernode("`>=",$1,$3); }
  | expr1 '<' expr1    { $$=mkopernode("`<",$1,$3); }
  | expr1 TOK_LE expr1   { $$=mkopernode("`<=",$1,$3); }
  | expr1 TOK_LSH expr1  { $$=mkopernode("`<<",$1,$3); }
  | expr1 TOK_RSH expr1  { $$=mkopernode("`>>",$1,$3); }
  | expr1 '+' expr1    { $$=mkopernode("`+",$1,$3); }
  | expr1 '-' expr1    { $$=mkopernode("`-",$1,$3); }
  | expr1 '*' expr1    { $$=mkopernode("`*",$1,$3); }
  | expr1 '%' expr1    { $$=mkopernode("`%",$1,$3); }
  | expr1 '/' expr1    { $$=mkopernode("`/",$1,$3); }
  | expr1 TOK_LOR error 
  | expr1 TOK_LAND error
  | expr1 '|' error   
  | expr1 '^' error   
  | expr1 '&' error   
  | expr1 TOK_EQ error  
  | expr1 TOK_NE error  
  | expr1 '>' error   
  | expr1 TOK_GE error  
  | expr1 '<' error   
  | expr1 TOK_LE error  
  | expr1 TOK_LSH error 
  | expr1 TOK_RSH error 
  | expr1 '+' error   
  | expr1 '-' error   
  | expr1 '*' error   
  | expr1 '%' error   
  | expr1 '/' error
  ;

expr2: expr3
  | cast expr2
  {
    $$=mkcastnode($1->u.sval.u.string,$2);
    free_node($1);
  }
  | soft_cast expr2
  {
    $$=mksoftcastnode($1->u.sval.u.string,$2);
    free_node($1);
  }
  | TOK_INC expr4       { $$=mknode(F_INC,$2,0); }
  | TOK_DEC expr4       { $$=mknode(F_DEC,$2,0); }
  | TOK_NOT expr2        { $$=mkopernode("`!",$2,0); }
  | '~' expr2          { $$=mkopernode("`~",$2,0); }
  | '-' expr2          { $$=mkopernode("`-",$2,0); }
  ;

expr3: expr4
  | expr4 TOK_INC       { $$=mknode(F_POST_INC,$1,0); }
  | expr4 TOK_DEC       { $$=mknode(F_POST_DEC,$1,0); }
  ;

/* FIXMEs
 * It would be nice if 'return' would exit from
 * the surrounding function rather than from the
 * implicit lambda. (I think) So beware that the
 * behaviour of 'return' might change some day.
 * -Hubbe
 *
 * It would also be nice if it was possible to send
 * arguments to the implicit function, but it would
 * require using ugly implicit variables or extending
 * the syntax, and if you extend the syntax you might
 * as well use lambda() instead.
 * -Hubbe
 *
 * We might want to allow having more than block after
 * a function ( ie. func(args) {} {} {} {} )
 * -Hubbe
 */

optional_block: /* EMPTY */ { $$=0; }
  | '{' push_compiler_frame0
  {
    debug_malloc_touch(Pike_compiler->compiler_frame->current_return_type);
    if(Pike_compiler->compiler_frame->current_return_type)
      free_string(Pike_compiler->compiler_frame->current_return_type);
    copy_shared_string(Pike_compiler->compiler_frame->current_return_type,any_type_string);

    /* block code */
    $<number>1=Pike_compiler->num_used_modules;
    $<number>3=Pike_compiler->compiler_frame->current_number_of_locals;
  }
  statements end_block
  {
    struct pike_string *type;
    char buf[40];
    int f,e;
    struct pike_string *name;

    /* block code */
    unuse_modules(Pike_compiler->num_used_modules - $<number>1);
    pop_local_variables($<number>3);
    
    debug_malloc_touch($4);
    $4=mknode(F_COMMA_EXPR,$4,mknode(F_RETURN,mkintnode(0),0));
    type=find_return_type($4);

    if(type) {
      push_finished_type(type);
      free_string(type);
    } else
      push_type(T_MIXED);
    
    push_type(T_VOID);
    push_type(T_MANY);
/*
    e=$4-1;
    for(; e>=0; e--)
      push_finished_type(Pike_compiler->compiler_frame->variable[e].type);
*/
    
    push_type(T_FUNCTION);
    type=compiler_pop_type();

    sprintf(buf,"__lambda_%ld_%ld",
	    (long)Pike_compiler->new_program->id,
	    (long)(Pike_compiler->local_class_counter++ & 0xffffffff)); /* OSF/1 cc bug. */
    name=make_shared_string(buf);

#ifdef LAMBDA_DEBUG
    fprintf(stderr, "%d: IMPLICIT LAMBDA: %s 0x%08lx 0x%08lx\n",
	    Pike_compiler->compiler_pass, buf, (long)Pike_compiler->new_program->id, Pike_compiler->local_class_counter-1);
#endif /* LAMBDA_DEBUG */
    
    f=dooptcode(name,
		$4,
		type,
		ID_STATIC | ID_PRIVATE | ID_INLINE);

    if(Pike_compiler->compiler_frame->lexical_scope & SCOPE_SCOPED) {
      $$ = mktrampolinenode(f);
    } else {
      $$ = mkidentifiernode(f);
    }
    free_string(name);
    free_string(type);
    pop_compiler_frame();
  }
  ;

expr4: string
  | TOK_NUMBER 
  | TOK_FLOAT { $$=mkfloatnode((FLOAT_TYPE)$1); }
  | catch
  | gauge
  | typeof
  | sscanf
  | lambda
  | class
  | idents2
  | expr4 '(' expr_list  ')' optional_block
    {
      if($5) $3=mknode(F_ARG_LIST, $3, $5);
      $$=mkapplynode($1,$3);
    }
  | expr4 '(' error ')' { $$=mkapplynode($1, NULL); yyerrok; }
  | expr4 '(' error TOK_LEX_EOF
  {
    yyerror("Missing ')'."); $$=mkapplynode($1, NULL);
    yyerror("Unexpected end of file.");
  }
  | expr4 '(' error ';' { yyerror("Missing ')'."); $$=mkapplynode($1, NULL); }
  | expr4 '(' error '}' { yyerror("Missing ')'."); $$=mkapplynode($1, NULL); }
  | expr4 '[' expr0 ']' { $$=mknode(F_INDEX,$1,$3); }
  | expr4 '['  comma_expr_or_zero expected_dot_dot comma_expr_or_maxint ']'
  {
    $$=mknode(F_RANGE,$1,mknode(F_ARG_LIST,$3,$5));
  }
  | expr4 '[' error ']' { $$=$1; yyerrok; }
  | expr4 '[' error TOK_LEX_EOF
  {
    $$=$1; yyerror("Missing ']'.");
    yyerror("Unexpected end of file.");
  }
  | expr4 '[' error ';' { $$=$1; yyerror("Missing ']'."); }
  | expr4 '[' error '}' { $$=$1; yyerror("Missing ']'."); }
  | expr4 '[' error ')' { $$=$1; yyerror("Missing ']'."); }
  | '(' comma_expr2 ')' { $$=$2; }
  | '(' '{' expr_list close_brace_or_missing ')'
    { $$=mkefuncallnode("aggregate",$3); }
  | '(' '[' m_expr_list close_bracket_or_missing ')'
    { $$=mkefuncallnode("aggregate_mapping",$3); }
  | TOK_MULTISET_START expr_list TOK_MULTISET_END
    { $$=mkefuncallnode("aggregate_multiset",$2); }
  | TOK_MULTISET_START expr_list ')'
    {
      yyerror("Missing '>'.");
      $$=mkefuncallnode("aggregate_multiset",$2);
    }
  | '(' error ')' { $$=0; yyerrok; }
  | '(' error TOK_LEX_EOF
  {
    $$=0; yyerror("Missing ')'.");
    yyerror("Unexpected end of file.");
  }
  | '(' error ';' { $$=0; yyerror("Missing ')'."); }
  | '(' error '}' { $$=0; yyerror("Missing ')'."); }
  | TOK_MULTISET_START error TOK_MULTISET_END { $$=0; yyerrok; }
  | TOK_MULTISET_START error ')' {
    yyerror("Missing '>'.");
    $$=0; yyerrok;
  }
  | TOK_MULTISET_START error TOK_LEX_EOF
  {
    $$=0; yyerror("Missing '>)'.");
    yyerror("Unexpected end of file.");
  }
  | TOK_MULTISET_START error ';' { $$=0; yyerror("Missing '>)'."); }
  | TOK_MULTISET_START error '}' { $$=0; yyerror("Missing '>)'."); }
  | expr4 TOK_ARROW magic_identifier
  {
    $$=mknode(F_ARROW,$1,$3);
  }
  | expr4 TOK_ARROW error {}
  ;

idents2: idents
  | TOK_LOCAL_ID TOK_COLON_COLON TOK_IDENTIFIER
  {
    int i;

    if(Pike_compiler->last_identifier) free_string(Pike_compiler->last_identifier);
    copy_shared_string(Pike_compiler->last_identifier, $3->u.sval.u.string);

    if (((i = find_shared_string_identifier(Pike_compiler->last_identifier,
					    Pike_compiler->new_program)) >= 0) ||
	((i = really_low_find_shared_string_identifier(Pike_compiler->last_identifier,
						       Pike_compiler->new_program,
						       SEE_STATIC|
						       SEE_PRIVATE)) >= 0)) {
      if (!(Pike_compiler->new_program->identifier_references[i].id_flags & ID_HIDDEN)) {
	/* We need to generate a new reference. */
	int d;
	struct reference funp = Pike_compiler->new_program->identifier_references[i];
	funp.id_flags |= ID_HIDDEN;
	i = -1;
	for(d = 0; d < (int)Pike_compiler->new_program->num_identifier_references; d++) {
	  struct reference *refp;
	  refp = Pike_compiler->new_program->identifier_references + d;

	  if(!MEMCMP((char *)refp,(char *)&funp,sizeof funp)) {
	    i = d;
	    break;
	  }
	}
	if (i < 0) {
	  add_to_identifier_references(funp);
	  i = Pike_compiler->new_program->num_identifier_references - 1;
	}
      }
      $$ = mkidentifiernode(i);
    } else {
      if (!Pike_compiler->num_parse_error) {
	if (Pike_compiler->compiler_pass == 2) {
	  my_yyerror("'%s' not defined in local scope.", Pike_compiler->last_identifier->str);
	  $$ = 0;
	} else {
	  $$ = mknode(F_UNDEFINED, 0, 0);
	}
      } else {
	$$ = mkintnode(0);
      }
    }

    free_node($3);
  }
  | TOK_LOCAL_ID TOK_COLON_COLON bad_identifier
  {
    $$=0;
  }
  ;

idents: low_idents
  | idents '.' TOK_IDENTIFIER
  {
    $$=index_node($1, Pike_compiler->last_identifier?Pike_compiler->last_identifier->str:NULL,
		  $3->u.sval.u.string);
    free_node($1);
    if(Pike_compiler->last_identifier) free_string(Pike_compiler->last_identifier);
    copy_shared_string(Pike_compiler->last_identifier, $3->u.sval.u.string);
    free_node($3);
  }
  | '.' TOK_IDENTIFIER
  {
    node *tmp;
    push_text(".");
    ref_push_string(lex.current_file);
    if (error_handler && error_handler->prog) {
      ref_push_object(error_handler);
      SAFE_APPLY_MASTER("handle_import", 3);
    } else {
      SAFE_APPLY_MASTER("handle_import", 2);
    }
    tmp=mkconstantsvaluenode(Pike_sp-1);
    pop_stack();
    $$=index_node(tmp, ".", $2->u.sval.u.string);
    free_node(tmp);
    if(Pike_compiler->last_identifier) free_string(Pike_compiler->last_identifier);
    copy_shared_string(Pike_compiler->last_identifier, $2->u.sval.u.string);
    free_node($2);
  }
  | idents '.' bad_identifier {}
  | idents '.' error {}
  ;

inherit_specifier: TOK_IDENTIFIER TOK_COLON_COLON
  {
    int e = 0;

    inherit_state = Pike_compiler;

    for (inherit_depth = -1; inherit_depth < compilation_depth;
	 inherit_depth++, inherit_state = inherit_state->previous) {
      if ((e = find_inherit(inherit_state->new_program, $1->u.sval.u.string)))
	break;
    }
    if (!e) {
      my_yyerror("No such inherit %s.", $1->u.sval.u.string->str);
    }
    free_node($1);
    $$ = e;
  }
  | inherit_specifier TOK_IDENTIFIER TOK_COLON_COLON
  {
    if ($1) {
      int e = 0;
#if 0
      /* FIXME: The inherit modifiers aren't kept. */
      if (!(inherit_state->new_program->inherits[$1].flags & ID_PRIVATE)) {
#endif /* 0 */
	e = find_inherit(inherit_state->new_program->inherits[$1].prog,
			 $2->u.sval.u.string);
#if 0
      }
#endif /* 0 */
      if (!e) {
	if (inherit_state->new_program->inherits[$1].name) {
	  my_yyerror("No such inherit %s::%s.",
		     inherit_state->new_program->inherits[$1].name->str,
		     $2->u.sval.u.string->str);
	} else {
	  my_yyerror("No such inherit %s.", $2->u.sval.u.string->str);
	}
	$$ = 0;
      } else {
	/* We know stuff about the inherit structure... */
	$$ = e + $1;
      }
    }
    free_node($2);
  }
  | inherit_specifier bad_identifier TOK_COLON_COLON { $$ = 0; }
  ;

low_idents: TOK_IDENTIFIER
  {
    int i;
    struct efun *f;
    if(Pike_compiler->last_identifier) free_string(Pike_compiler->last_identifier);
    copy_shared_string(Pike_compiler->last_identifier, $1->u.sval.u.string);

    if(($$=lexical_islocal(Pike_compiler->last_identifier)))
    {
      /* done, nothing to do here */
    }else if((i=isidentifier(Pike_compiler->last_identifier))>=0){
      $$=mkidentifiernode(i);
    }else if(!($$=find_module_identifier(Pike_compiler->last_identifier,1))){
      if(!Pike_compiler->num_parse_error)
      {
	if(Pike_compiler->compiler_pass==2)
	{
	  my_yyerror("'%s' undefined.", Pike_compiler->last_identifier->str);
	  $$=0;
	}else{
	  $$=mknode(F_UNDEFINED,0,0);
	}
      }else{
	$$=mkintnode(0);
      }
    }
    free_node($1);
  }
  | TOK_PREDEF TOK_COLON_COLON TOK_IDENTIFIER
  {
    node *tmp2;
    extern dynamic_buffer used_modules;

    if(Pike_compiler->last_identifier) free_string(Pike_compiler->last_identifier);
    copy_shared_string(Pike_compiler->last_identifier, $3->u.sval.u.string);

    tmp2=mkconstantsvaluenode((struct svalue *) used_modules.s.str );
    $$=index_node(tmp2, "predef", $3->u.sval.u.string);
    if(!$$->name)
      add_ref( $$->name=$3->u.sval.u.string );
    free_node(tmp2);
    free_node($3);
  }
  | TOK_PREDEF TOK_COLON_COLON bad_identifier
  {
    $$=0;
  }
  | inherit_specifier TOK_IDENTIFIER
  {
    if ($1) {
      int id;

      if(Pike_compiler->last_identifier) free_string(Pike_compiler->last_identifier);
      copy_shared_string(Pike_compiler->last_identifier, $2->u.sval.u.string);

      id = low_reference_inherited_identifier(inherit_state,
					      $1,
					      Pike_compiler->last_identifier,
					      SEE_STATIC);

      if (id != -1) {
	if (inherit_depth >= 0) {
	  $$ = mkexternalnode(inherit_state->new_program, id);
	} else {
	  $$ = mkidentifiernode(id);
	}
      } else if(ISCONSTSTR(Pike_compiler->last_identifier, "`->") ||
		ISCONSTSTR(Pike_compiler->last_identifier, "`[]")) {
	$$ = mknode(F_MAGIC_INDEX, mknewintnode($1),
		    mknewintnode(inherit_depth+1));
      } else if(ISCONSTSTR(Pike_compiler->last_identifier, "`->=") ||
		ISCONSTSTR(Pike_compiler->last_identifier, "`[]=")) {
	$$ = mknode(F_MAGIC_SET_INDEX, mknewintnode($1),
		    mknewintnode(inherit_depth+1));
      } else {
	if (inherit_state->new_program->inherits[$1].name) {
	  my_yyerror("Undefined identifier %s::%s.",
		     inherit_state->new_program->inherits[$1].name->str,
		     Pike_compiler->last_identifier->str);
	} else {
	  my_yyerror("Undefined identifier %s.", Pike_compiler->last_identifier->str);
	}
	$$=0;
      }
    } else {
      $$=0;
    }

    free_node($2);
  }
  | inherit_specifier bad_identifier { $$=0; }
  | inherit_specifier error { $$=0; }
  | TOK_COLON_COLON TOK_IDENTIFIER
  {
    int e,i;

    if(Pike_compiler->last_identifier) free_string(Pike_compiler->last_identifier);
    copy_shared_string(Pike_compiler->last_identifier, $2->u.sval.u.string);

    $$=0;
    for(e=1;e<(int)Pike_compiler->new_program->num_inherits;e++)
    {
      if(Pike_compiler->new_program->inherits[e].inherit_level!=1) continue;
      i=low_reference_inherited_identifier(0,e,$2->u.sval.u.string,SEE_STATIC);
      if(i==-1) continue;
      if($$)
      {
	$$=mknode(F_ARG_LIST,$$,mkidentifiernode(i));
      }else{
	$$=mkidentifiernode(i);
      }
    }
    if(!$$)
    {
	if(ISCONSTSTR($2->u.sval.u.string,"`->") ||
	   ISCONSTSTR($2->u.sval.u.string,"`[]") )
	{
	  $$=mknode(F_MAGIC_INDEX,mknewintnode(0),mknewintnode(0));
	}
	else if(ISCONSTSTR($2->u.sval.u.string,"`->=") ||
		ISCONSTSTR($2->u.sval.u.string,"`[]=") )
	{
	  $$=mknode(F_MAGIC_SET_INDEX,mknewintnode(0),mknewintnode(0));
	}
	else
	{
	  $$=mkintnode(0);
	}
    }else{
      if($$->token==F_ARG_LIST) $$=mkefuncallnode("aggregate",$$);
    }
    free_node($2);
  }
  | TOK_COLON_COLON bad_identifier
  {
    $$=0;
  }
  ;

comma_expr_or_zero: /* empty */ { $$=mkintnode(0); }
  | comma_expr
  | TOK_LEX_EOF { yyerror("Unexpected end of file."); $$=0; }
  ;

comma_expr_or_maxint: /* empty */ { $$=mkintnode(0x7fffffff); }
  | comma_expr
  | TOK_LEX_EOF { yyerror("Unexpected end of file."); $$=mkintnode(0x7fffffff); }
  ;

gauge: TOK_GAUGE catch_arg
  {
#ifdef HAVE_GETHRVTIME
    $$=mkefuncallnode("abs",
		  mkopernode("`/", 
			     mkopernode("`-", mkefuncallnode("gethrvtime",0),
					mknode(F_COMMA_EXPR,$2,
					       mkefuncallnode("gethrvtime",0))),
			     mkfloatnode((FLOAT_TYPE)1000000.0)));
#else
  $$=mkefuncallnode("abs",
	mkopernode("`/", 
		mkopernode("`-",
			 mknode(F_INDEX,mkefuncallnode("rusage",0),
				mkintnode(GAUGE_RUSAGE_INDEX)),
		         mknode(F_COMMA_EXPR,$2,
				mknode(F_INDEX,mkefuncallnode("rusage",0),
				       mkintnode(GAUGE_RUSAGE_INDEX)))),
		mkfloatnode((FLOAT_TYPE)1000.0)));
#endif
  };

typeof: TOK_TYPEOF '(' expr0 ')'
  {
    struct pike_string *s;
    node *tmp;

    /* FIXME: Why build the node at all? */
    /* Because the optimizer cannot optimize the root node of the
     * tree properly -Hubbe
     */
    tmp=mknode(F_COMMA_EXPR, $3, 0);

    s=(tmp && CAR(tmp) && CAR(tmp)->type ? CAR(tmp)->type : mixed_type_string);
    if(TEST_COMPAT(7,0))
    {
      s=describe_type(s);
      $$ = mkstrnode(s);
      free_string(s);
    }else{
      $$ = mktypenode(s);
    }
    free_node(tmp);
  } 
  | TOK_TYPEOF '(' error ')' { $$=0; yyerrok; }
  | TOK_TYPEOF '(' error '}' { $$=0; yyerror("Missing ')'."); }
  | TOK_TYPEOF '(' error TOK_LEX_EOF
  {
    $$=0; yyerror("Missing ')'.");
    yyerror("Unexpected end of file.");
  }
  | TOK_TYPEOF '(' error ';' { $$=0; yyerror("Missing ')'."); }
  ;
 
catch_arg: '(' comma_expr ')'  { $$=$2; }
  | '(' error ')' { $$=0; yyerrok; }
  | '(' error TOK_LEX_EOF
  {
    $$=0; yyerror("Missing ')'.");
    yyerror("Unexpected end of file.");
  }
  | '(' error '}' { $$=0; yyerror("Missing ')'."); }
  | '(' error ';' { $$=0; yyerror("Missing ')'."); }
  | block
  | error { $$=0; yyerror("Bad expression for catch."); }
  ; 

catch: TOK_CATCH
     {
       Pike_compiler->catch_level++;
     }
     catch_arg
     {
       $$=mknode(F_CATCH,$3,NULL);
       Pike_compiler->catch_level--;
     }
     ;

sscanf: TOK_SSCANF '(' expr0 ',' expr0 lvalue_list ')'
  {
    $$=mknode(F_SSCANF,mknode(F_ARG_LIST,$3,$5),$6);
  }
  | TOK_SSCANF '(' expr0 ',' expr0 error ')'
  {
    $$=0;
    free_node($3);
    free_node($5);
    yyerrok;
  }
  | TOK_SSCANF '(' expr0 ',' expr0 error TOK_LEX_EOF
  {
    $$=0;
    free_node($3);
    free_node($5);
    yyerror("Missing ')'.");
    yyerror("Unexpected end of file.");
  }
  | TOK_SSCANF '(' expr0 ',' expr0 error '}'
  {
    $$=0;
    free_node($3);
    free_node($5);
    yyerror("Missing ')'.");
  }
  | TOK_SSCANF '(' expr0 ',' expr0 error ';'
  {
    $$=0;
    free_node($3);
    free_node($5);
    yyerror("Missing ')'.");
  }
  | TOK_SSCANF '(' expr0 error ')'
  {
    $$=0;
    free_node($3);
    yyerrok;
  }
  | TOK_SSCANF '(' expr0 error TOK_LEX_EOF
  {
    $$=0;
    free_node($3);
    yyerror("Missing ')'.");
    yyerror("Unexpected end of file.");
  }
  | TOK_SSCANF '(' expr0 error '}'
  {
    $$=0;
    free_node($3);
    yyerror("Missing ')'.");
  }
  | TOK_SSCANF '(' expr0 error ';'
  {
    $$=0;
    free_node($3);
    yyerror("Missing ')'.");
  }
  | TOK_SSCANF '(' error ')' { $$=0; yyerrok; }
  | TOK_SSCANF '(' error TOK_LEX_EOF
  {
    $$=0; yyerror("Missing ')'.");
    yyerror("Unexpected end of file.");
  }
  | TOK_SSCANF '(' error '}' { $$=0; yyerror("Missing ')'."); }
  | TOK_SSCANF '(' error ';' { $$=0; yyerror("Missing ')'."); }
  ;

lvalue: expr4
  | '[' low_lvalue_list ']' { $$=mknode(F_ARRAY_LVALUE, $2,0); }
  | type6 TOK_IDENTIFIER
  {
    add_local_name($2->u.sval.u.string,compiler_pop_type(),0);
    $$=mklocalnode(islocal($2->u.sval.u.string),0);
    free_node($2);
  }
  | bad_expr_ident
  { $$=mknewintnode(0); }
  ;
low_lvalue_list: lvalue lvalue_list { $$=mknode(F_LVALUE_LIST,$1,$2); }
  ;

lvalue_list: /* empty */ { $$ = 0; }
  | ',' lvalue lvalue_list { $$ = mknode(F_LVALUE_LIST,$2,$3); }
  ;

string: TOK_STRING 
  | string TOK_STRING
  {
    struct pike_string *a,*b;
    copy_shared_string(a,$1->u.sval.u.string);
    copy_shared_string(b,$2->u.sval.u.string);
    free_node($1);
    free_node($2);
    a=add_and_free_shared_strings(a,b);
    $$=mkstrnode(a);
    free_string(a);
  }
  ;

/*
 * Some error-handling
 */

/* FIXME: Should probably set Pike_compiler->last_identifier. */
bad_identifier: bad_expr_ident
  | TOK_CLASS
  { yyerror("class is a reserved word."); }
  | TOK_ARRAY_ID
  { yyerror("array is a reserved word."); }
  | TOK_FLOAT_ID
  { yyerror("float is a reserved word.");}
  | TOK_FUNCTION_ID
  { yyerror("function is a reserved word.");}
  | TOK_INT_ID
  { yyerror("int is a reserved word."); }
  | TOK_MAPPING_ID
  { yyerror("mapping is a reserved word."); }
  | TOK_MIXED_ID
  { yyerror("mixed is a reserved word."); }
  | TOK_MULTISET_ID
  { yyerror("multiset is a reserved word."); }
  | TOK_OBJECT_ID
  { yyerror("object is a reserved word."); }
  | TOK_PROGRAM_ID
  { yyerror("program is a reserved word."); }
  | TOK_STRING_ID
  { yyerror("string is a reserved word."); }
  | TOK_VOID_ID
  { yyerror("void is a reserved word."); }
  ;

bad_expr_ident:
    TOK_INLINE
  { yyerror("inline is a reserved word."); }
  | TOK_LOCAL_ID
  { yyerror("local is a reserved word."); }
  | TOK_NO_MASK
  { yyerror("nomask is a reserved word."); }
  | TOK_PREDEF
  { yyerror("predef is a reserved word."); }
  | TOK_PRIVATE
  { yyerror("private is a reserved word."); }
  | TOK_PROTECTED
  { yyerror("protected is a reserved word."); }
  | TOK_PUBLIC
  { yyerror("public is a reserved word."); }
  | TOK_OPTIONAL
  { yyerror("optional is a reserved word."); }
  | TOK_VARIANT
  { yyerror("variant is a reserved word."); }
  | TOK_STATIC
  { yyerror("static is a reserved word."); }
  | TOK_EXTERN
  { yyerror("extern is a reserved word."); }
  | TOK_FINAL_ID
  { yyerror("final is a reserved word.");}
  | TOK_DO
  { yyerror("do is a reserved word."); }
  | TOK_ELSE
  { yyerror("else without if."); }
  | TOK_RETURN
  { yyerror("return is a reserved word."); }
  | TOK_IMPORT
  { yyerror("import is a reserved word."); }
  | TOK_INHERIT
  { yyerror("inherit is a reserved word."); }
  | TOK_CATCH
  { yyerror("catch is a reserved word."); }
  | TOK_GAUGE
  { yyerror("gauge is a reserved word."); }
  | TOK_LAMBDA
  { yyerror("lambda is a reserved word."); }
  | TOK_SSCANF
  { yyerror("sscanf is a reserved word."); }
  | TOK_SWITCH
  { yyerror("switch is a reserved word."); }
  | TOK_TYPEOF
  { yyerror("typeof is a reserved word."); }
  | TOK_BREAK
  { yyerror("break is a reserved word."); }
  | TOK_CASE
  { yyerror("case is a reserved word."); }
  | TOK_CONTINUE
  { yyerror("continue is a reserved word."); }
  | TOK_DEFAULT
  { yyerror("default is a reserved word."); }
  | TOK_FOR
  { yyerror("for is a reserved word."); }
  | TOK_FOREACH
  { yyerror("foreach is a reserved word."); }
  | TOK_IF
  { yyerror("if is a reserved word."); }
  ;



%%

void yyerror(char *str)
{
  extern int num_parse_error;
  extern int cumulative_parse_error;

#ifdef PIKE_DEBUG
  if(Pike_interpreter.recoveries && Pike_sp-Pike_interpreter.evaluator_stack < Pike_interpreter.recoveries->stack_pointer)
    fatal("Stack error (underflow)\n");
#endif

  if (Pike_compiler->num_parse_error > 10) return;
  Pike_compiler->num_parse_error++;
  cumulative_parse_error++;

  if ((error_handler && error_handler->prog) || get_master())
  {
    if (lex.current_file) {
      ref_push_string(lex.current_file);
    } else {
      /* yyerror() can be called from define_function(), which
       * can be called by the C module initialization code.
       */
      push_constant_text("");
    }
    push_int(lex.current_line);
    push_text(str);
    if (error_handler && error_handler->prog) {
      safe_apply(error_handler, "compile_error", 3);
    } else {
      SAFE_APPLY_MASTER("compile_error", 3);
    }
    pop_stack();
  }else{
    if (lex.current_file) {
      (void)fprintf(stderr, "%s:%ld: %s\n",
		    lex.current_file->str,
		    (long)lex.current_line,
		    str);
    } else {
      (void)fprintf(stderr, "NULL:%ld: %s\n",
		    (long)lex.current_line,
		    str);
    }
    fflush(stderr);
  }
}

static int low_islocal(struct compiler_frame *f,
		       struct pike_string *str)
{
  int e;
  for(e=f->current_number_of_locals-1;e>=0;e--)
    if(f->variable[e].name==str)
      return e;
  return -1;
}



int low_add_local_name(struct compiler_frame *frame,
			struct pike_string *str,
			struct pike_string *type,
			node *def)
{

  if (str->len && !TEST_COMPAT(7,0)) {
    int tmp=low_islocal(frame,str);
    if(tmp>=0 && tmp >= frame->last_block_level)
    {
      if(str->size_shift)
	my_yyerror("Duplicate local variable, "
		   "previous declaration on line %d\n",
		   frame->variable[tmp].line);
      else
	my_yyerror("Duplicate local variable '%s', "
		   "previous declaration on line %d\n",
		   STR0(str), frame->variable[tmp].line);
    }

    if(type == void_type_string)
    {
      if(str->size_shift)
	my_yyerror("Local variables cannot be of type of 'void'.\n");
      else
	my_yyerror("Local variable '%s' is void.\n",STR0(str));
    }
  }

  debug_malloc_touch(def);
  debug_malloc_touch(type);
  debug_malloc_touch(str);
  reference_shared_string(str);
  if (frame->current_number_of_locals == MAX_LOCAL)
  {
    yyerror("Too many local variables.");
    return 0;
  }else {
#ifdef PIKE_DEBUG
    check_type_string(type);
#endif /* PIKE_DEBUG */
    if (pike_types_le(type, void_type_string)) {
      if (Pike_compiler->compiler_pass != 1) {
	yywarning("Declaring local variable with type void "
		  "(converted to type zero).");
      }
      free_string(type);
      copy_shared_string(type, zero_type_string);
    }
    frame->variable[frame->current_number_of_locals].type = type;
    frame->variable[frame->current_number_of_locals].name = str;
    frame->variable[frame->current_number_of_locals].def = def;

    frame->variable[frame->current_number_of_locals].line=lex.current_line;
    frame->variable[frame->current_number_of_locals].file=lex.current_file;
    add_ref(lex.current_file);

    frame->current_number_of_locals++;
    if(frame->current_number_of_locals > 
       frame->max_number_of_locals)
    {
      frame->max_number_of_locals=
	frame->current_number_of_locals;
    }

    return frame->current_number_of_locals-1;
  }
}


/* argument must be a shared string */
/* Note that this function eats a reference to 'type' */
/* If def is nonzero, it also eats a ref to def */
int add_local_name(struct pike_string *str,
		   struct pike_string *type,
		   node *def)
{
  return low_add_local_name(Pike_compiler->compiler_frame,
			    str,
			    type,
			    def);
}



int islocal(struct pike_string *str)
{
  return low_islocal(Pike_compiler->compiler_frame, str);
}

/* argument must be a shared string */
static node *lexical_islocal(struct pike_string *str)
{
  int e,depth=0;
  struct compiler_frame *f=Pike_compiler->compiler_frame;
  
  while(1)
  {
    for(e=f->current_number_of_locals-1;e>=0;e--)
    {
      if(f->variable[e].name==str)
      {
	struct compiler_frame *q=Pike_compiler->compiler_frame;
	if(f->variable[e].def)
	  return copy_node(f->variable[e].def);
	while(q!=f) 
	{
	  q->lexical_scope|=SCOPE_SCOPED;
	  q=q->previous;
	}

	if(depth)
	  q->lexical_scope|=SCOPE_SCOPE_USED;

	return mklocalnode(e,depth);
      }
    }
    if(!(f->lexical_scope & SCOPE_LOCAL)) return 0;
    depth++;
    f=f->previous;
  }
}

void cleanup_compiler(void)
{
}
