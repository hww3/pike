/*
 * $Id: interpret_functions.h,v 1.88 2001/08/16 03:27:35 hubbe Exp $
 *
 * Opcode definitions for the interpreter.
 */

#include "global.h"

#undef CJUMP
#undef AUTO_BIGNUM_LOOP_TEST
#undef LOOP
#undef COMPARISON
#undef MKAPPLY
#undef DO_CALL_BUILTIN

#undef DO_IF_BIGNUM
#ifdef AUTO_BIGNUM
#define DO_IF_BIGNUM(CODE)	CODE
#else /* !AUTO_BIGNUM */
#define DO_IF_BIGNUM(CODE)
#endif /* AUTO_BIGNUM */

#undef DO_IF_ELSE_COMPUTED_GOTO
#ifdef HAVE_COMPUTED_GOTO
#define DO_IF_ELSE_COMPUTED_GOTO(A, B)	(A)
#else /* !HAVE_COMPUTED_GOTO */
#define DO_IF_ELSE_COMPUTED_GOTO(A, B)	(B)
#endif /* HAVE_COMPUTED_GOTO */

#ifdef GEN_PROTOS
/* Used to generate the interpret_protos.h file. */
#define OPCODE0(A, B, C)		OPCODE0(A, B) C
#define OPCODE1(A, B, C)		OPCODE1(A, B) C
#define OPCODE2(A, B, C)		OPCODE2(A, B) C
#define OPCODE0_TAIL(A, B, C)		OPCODE0_TAIL(A, B) C
#define OPCODE1_TAIL(A, B, C)		OPCODE1_TAIL(A, B) C
#define OPCODE2_TAIL(A, B, C)		OPCODE2_TAIL(A, B) C
#define OPCODE0_JUMP(A, B, C)		OPCODE0_JUMP(A, B) C
#define OPCODE1_JUMP(A, B, C)		OPCODE1_JUMP(A, B) C
#define OPCODE2_JUMP(A, B, C)		OPCODE2_JUMP(A, B) C
#define OPCODE0_TAILJUMP(A, B, C)	OPCODE0_TAILJUMP(A, B) C
#define OPCODE1_TAILJUMP(A, B, C)	OPCODE1_TAILJUMP(A, B) C
#define OPCODE2_TAILJUMP(A, B, C)	OPCODE2_TAILJUMP(A, B) C
#define OPCODE0_RETURN(A, B, C)		OPCODE0_RETURN(A, B) C
#define OPCODE1_RETURN(A, B, C)		OPCODE1_RETURN(A, B) C
#define OPCODE2_RETURN(A, B, C)		OPCODE2_RETURN(A, B) C
#define OPCODE0_TAILRETURN(A, B, C)	OPCODE0_TAILRETURN(A, B) C
#define OPCODE1_TAILRETURN(A, B, C)	OPCODE1_TAILRETURN(A, B) C
#define OPCODE2_TAILRETURN(A, B, C)	OPCODE2_TAILRETURN(A, B) C
#endif /* GEN_PROTOS */


#ifndef PROG_COUNTER
#define PROG_COUNTER pc
#endif

#ifndef INTER_ESCAPE_CATCH
#define INTER_ESCAPE_CATCH return -2
#endif

#ifndef INTER_RETURN
#define INTER_RETURN return -1
#endif

#ifndef OVERRIDE_JUMPS

#undef GET_JUMP
#undef SKIPJUMP
#undef DOJUMP

#ifdef PIKE_DEBUG

#define GET_JUMP() (backlog[backlogp].arg=(\
  (t_flag>3 ? sprintf(trace_buffer, "-    Target = %+ld\n", \
                      (long)LOW_GET_JUMP()), \
              write_to_stderr(trace_buffer,strlen(trace_buffer)) : 0), \
  LOW_GET_JUMP()))

#define SKIPJUMP() (GET_JUMP(), LOW_SKIPJUMP())

#else /* !PIKE_DEBUG */

#define GET_JUMP() LOW_GET_JUMP()
#define SKIPJUMP() LOW_SKIPJUMP()

#endif /* PIKE_DEBUG */

#define DOJUMP() do { \
    INT32 tmp; \
    tmp = GET_JUMP(); \
    SET_PROG_COUNTER(PROG_COUNTER + tmp); \
    FETCH; \
    if(tmp < 0) \
      fast_check_threads_etc(6); \
  } while(0)

#endif /* OVERRIDE_JUMPS */


/* WARNING:
 * The surgeon general has stated that define code blocks
 * without do{}while() can be hazardous to your health.
 * However, in these cases it is required to handle break
 * properly. -Hubbe
 */
#undef DO_JUMP_TO
#define DO_JUMP_TO(NEWPC)	{	\
  SET_PROG_COUNTER(NEWPC);		\
  FETCH;				\
  DONE;					\
}

#undef DO_DUMB_RETURN
#define DO_DUMB_RETURN {				\
  if(Pike_fp -> flags & PIKE_FRAME_RETURN_INTERNAL)	\
  {							\
    int f=Pike_fp->flags;				\
    if(f & PIKE_FRAME_RETURN_POP)			\
       low_return_pop();				\
     else						\
       low_return();					\
							\
    DO_IF_DEBUG(if (t_flag)				\
      fprintf(stderr, "Returning to 0x%p\n",		\
	      Pike_fp->pc));				\
    DO_JUMP_TO(Pike_fp->pc);				\
  }							\
  DO_IF_DEBUG(if (t_flag)				\
    fprintf(stderr, "Inter return\n"));			\
  INTER_RETURN;						\
}

#undef DO_RETURN
#ifndef PIKE_DEBUG
#define DO_RETURN DO_DUMB_RETURN
#else
#define DO_RETURN {				\
  if(d_flag>3) do_gc();				\
  if(d_flag>4) do_debug();			\
  check_threads_etc();				\
  DO_DUMB_RETURN;				\
}
#endif

#undef DO_INDEX
#define DO_INDEX do {				\
  struct svalue s;				\
  index_no_free(&s,Pike_sp-2,Pike_sp-1);	\
  pop_2_elems();				\
  *Pike_sp=s;					\
  Pike_sp++;					\
  print_return_value();				\
}while(0)


OPCODE0(F_UNDEFINED, "push UNDEFINED", {
  push_int(0);
  Pike_sp[-1].subtype=NUMBER_UNDEFINED;
});

OPCODE0(F_CONST0, "push 0", {
  push_int(0);
});

OPCODE0(F_CONST1, "push 1", {
  push_int(1);
});

OPCODE0(F_CONST_1,"push -1", {
  push_int(-1);
});

OPCODE0(F_BIGNUM, "push 0x7fffffff", {
  push_int(0x7fffffff);
});

OPCODE1(F_NUMBER, "push int", {
  push_int(arg1);
});

OPCODE1(F_NEG_NUMBER, "push -int", {
  push_int(-arg1);
});

OPCODE1(F_CONSTANT, "constant", {
  push_svalue(& Pike_fp->context.prog->constants[arg1].sval);
  print_return_value();
});

/* The rest of the basic 'push value' instructions */	

OPCODE1_TAIL(F_MARK_AND_STRING, "mark & string", {
  *(Pike_mark_sp++)=Pike_sp;

  OPCODE1(F_STRING, "string", {
    copy_shared_string(Pike_sp->u.string,Pike_fp->context.prog->strings[arg1]);
    Pike_sp->type=PIKE_T_STRING;
    Pike_sp->subtype=0;
    Pike_sp++;
    print_return_value();
  });
});


OPCODE1(F_ARROW_STRING, "->string", {
  copy_shared_string(Pike_sp->u.string,Pike_fp->context.prog->strings[arg1]);
  Pike_sp->type=PIKE_T_STRING;
  Pike_sp->subtype=1; /* Magic */
  Pike_sp++;
  print_return_value();
});

OPCODE1(F_LOOKUP_LFUN, "->lfun", {
  struct svalue tmp;
  struct object *o;
  int id;
  if ((sp[-1].type == T_OBJECT) && ((o = Pike_sp[-1].u.object)->prog) &&
      (FIND_LFUN(o->prog, LFUN_ARROW) == -1)) {
    int id = FIND_LFUN(o->prog, arg1);
    if ((id != -1) &&
	(!(o->prog->identifier_references[id].id_flags &
	   (ID_STATIC|ID_PRIVATE|ID_HIDDEN)))) {
      low_object_index_no_free(&tmp, o, id);
    } else {
      /* Not found. */
      tmp.type = T_INT;
      tmp.subtype = 1;
      tmp.u.integer = 0;
    }
  } else {
    struct svalue tmp2;
    tmp2.type = PIKE_T_STRING;
    tmp2.u.string = lfun_strings[arg1];
    tmp2.subtype = 1;
    index_no_free(&tmp, Pike_sp-1, &tmp2);
  }
  free_svalue(Pike_sp-1);
  Pike_sp[-1] = tmp;
  print_return_value();
});

OPCODE0(F_FLOAT, "push float", {
  /* FIXME, this opcode uses 'PROG_COUNTER' which is not allowed.. */
  MEMCPY((void *)&Pike_sp->u.float_number, PROG_COUNTER, sizeof(FLOAT_TYPE));
  Pike_sp->type=PIKE_T_FLOAT;
  Pike_sp++;
  DO_JUMP_TO((PIKE_OPCODE_T *)(((FLOAT_TYPE *)PROG_COUNTER) + 1));
});

OPCODE1(F_LFUN, "local function", {
  Pike_sp->u.object=Pike_fp->current_object;
  add_ref(Pike_fp->current_object);
  Pike_sp->subtype=arg1+Pike_fp->context.identifier_level;
  Pike_sp->type=PIKE_T_FUNCTION;
  Pike_sp++;
  print_return_value();
});

OPCODE1(F_TRAMPOLINE, "trampoline", {
  struct object *o=low_clone(pike_trampoline_program);
  add_ref( ((struct pike_trampoline *)(o->storage))->frame=Pike_fp );
  ((struct pike_trampoline *)(o->storage))->func=arg1+Pike_fp->context.identifier_level;
  push_object(o);
  /* Make it look like a function. */
  Pike_sp[-1].subtype = pike_trampoline_program->lfuns[LFUN_CALL];
  Pike_sp[-1].type = T_FUNCTION;
  print_return_value();
});

/* The not so basic 'push value' instructions */

OPCODE1_TAIL(F_MARK_AND_GLOBAL, "mark & global", {
  *(Pike_mark_sp++)=Pike_sp;

  OPCODE1(F_GLOBAL, "global", {
    low_object_index_no_free(Pike_sp,
			     Pike_fp->current_object,
			     arg1 + Pike_fp->context.identifier_level);
    Pike_sp++;
    print_return_value();
  });
});

OPCODE2_TAIL(F_MARK_AND_EXTERNAL, "mark & external", {
  *(Pike_mark_sp++)=Pike_sp;

  OPCODE2(F_EXTERNAL,"external", {
    struct external_variable_context loc;

    loc.o=Pike_fp->current_object;
    if(!loc.o->prog)
      Pike_error("Cannot access parent of destructed object.\n");

    loc.parent_identifier=Pike_fp->fun;
    loc.inherit=INHERIT_FROM_INT(loc.o->prog, Pike_fp->fun);
  
    find_external_context(&loc, arg2);

    DO_IF_DEBUG({
      TRACE((5,"-   Identifier=%d Offset=%d\n",
	     arg1,
	     loc.inherit->identifier_level));
    });

    low_object_index_no_free(Pike_sp,
			     loc.o,
			     arg1 + loc.inherit->identifier_level);
    Pike_sp++;
    print_return_value();
  });
});


OPCODE2(F_EXTERNAL_LVALUE, "& external", {
  struct external_variable_context loc;

  loc.o=Pike_fp->current_object;
  if(!loc.o->prog)
    Pike_error("Cannot access parent of destructed object.\n");

  loc.parent_identifier=Pike_fp->fun;
  loc.inherit=INHERIT_FROM_INT(loc.o->prog, Pike_fp->fun);
  
  find_external_context(&loc, arg2);

  DO_IF_DEBUG({
    TRACE((5,"-   Identifier=%d Offset=%d\n",
	   arg1,
	   loc.inherit->identifier_level));
  });


  ref_push_object(loc.o);
  Pike_sp->type=T_LVALUE;
  Pike_sp->u.integer=arg1 + loc.inherit->identifier_level;
  Pike_sp++;
});

OPCODE1(F_MARK_AND_LOCAL, "mark & local", {
  *(Pike_mark_sp++) = Pike_sp;
  push_svalue( Pike_fp->locals + arg1);
  print_return_value();
});

OPCODE1(F_LOCAL, "local", {
  push_svalue( Pike_fp->locals + arg1);
  print_return_value();
});

OPCODE2(F_2_LOCALS, "2 locals", {
  push_svalue( Pike_fp->locals + arg1);
  print_return_value();
  push_svalue( Pike_fp->locals + arg2);
  print_return_value();
});

OPCODE2(F_LOCAL_2_LOCAL, "local = local", {
  assign_svalue(Pike_fp->locals + arg1, Pike_fp->locals + arg2);
});

OPCODE2(F_LOCAL_2_GLOBAL, "global = local", {
  INT32 tmp = arg1 + Pike_fp->context.identifier_level;
  struct identifier *i;

  if(!Pike_fp->current_object->prog)
    Pike_error("Cannot access global variables in destructed object.\n");

  i = ID_FROM_INT(Pike_fp->current_object->prog, tmp);
  if(!IDENTIFIER_IS_VARIABLE(i->identifier_flags))
    Pike_error("Cannot assign functions or constants.\n");
  if(i->run_time_type == PIKE_T_MIXED)
  {
    assign_svalue((struct svalue *)GLOBAL_FROM_INT(tmp),
		  Pike_fp->locals + arg2);
  }else{
    assign_to_short_svalue((union anything *)GLOBAL_FROM_INT(tmp),
			   i->run_time_type,
			   Pike_fp->locals + arg2);
  }
});

OPCODE2(F_GLOBAL_2_LOCAL, "local = global", {
  INT32 tmp = arg1 + Pike_fp->context.identifier_level;
  free_svalue(Pike_fp->locals + arg2);
  low_object_index_no_free(Pike_fp->locals + arg2,
			   Pike_fp->current_object,
			   tmp);
});

OPCODE1(F_LOCAL_LVALUE, "& local", {
  Pike_sp[0].type = T_LVALUE;
  Pike_sp[0].u.lval = Pike_fp->locals + arg1;
  Pike_sp[1].type = T_VOID;
  Pike_sp += 2;
});

OPCODE2(F_LEXICAL_LOCAL, "lexical local", {
  struct pike_frame *f=Pike_fp;
  while(arg2--)
  {
    f=f->scope;
    if(!f) Pike_error("Lexical scope error.\n");
  }
  push_svalue(f->locals + arg1);
  print_return_value();
});

OPCODE2(F_LEXICAL_LOCAL_LVALUE, "&lexical local", {
  struct pike_frame *f=Pike_fp;
  while(arg2--)
  {
    f=f->scope;
    if(!f) Pike_error("Lexical scope error.\n");
  }
  Pike_sp[0].type=T_LVALUE;
  Pike_sp[0].u.lval=f->locals+arg1;
  Pike_sp[1].type=T_VOID;
  Pike_sp+=2;
});

OPCODE1(F_ARRAY_LVALUE, "[ lvalues ]", {
  f_aggregate(arg1*2);
  Pike_sp[-1].u.array->flags |= ARRAY_LVALUE;
  Pike_sp[-1].u.array->type_field |= BIT_UNFINISHED | BIT_MIXED;
  /* FIXME: Shouldn't a ref be added here? */
  Pike_sp[0] = Pike_sp[-1];
  Pike_sp[-1].type = T_ARRAY_LVALUE;
  dmalloc_touch_svalue(Pike_sp);
  Pike_sp++;
});

OPCODE1(F_CLEAR_2_LOCAL, "clear 2 local", {
  free_mixed_svalues(Pike_fp->locals + arg1, 2);
  Pike_fp->locals[arg1].type = PIKE_T_INT;
  Pike_fp->locals[arg1].subtype = 0;
  Pike_fp->locals[arg1].u.integer = 0;
  Pike_fp->locals[arg1+1].type = PIKE_T_INT;
  Pike_fp->locals[arg1+1].subtype = 0;
  Pike_fp->locals[arg1+1].u.integer = 0;
});

OPCODE1(F_CLEAR_4_LOCAL, "clear 4 local", {
  int e;
  free_mixed_svalues(Pike_fp->locals + arg1, 4);
  for(e = 0; e < 4; e++)
  {
    Pike_fp->locals[arg1+e].type = PIKE_T_INT;
    Pike_fp->locals[arg1+e].subtype = 0;
    Pike_fp->locals[arg1+e].u.integer = 0;
  }
});

OPCODE1(F_CLEAR_LOCAL, "clear local", {
  free_svalue(Pike_fp->locals + arg1);
  Pike_fp->locals[arg1].type = PIKE_T_INT;
  Pike_fp->locals[arg1].subtype = 0;
  Pike_fp->locals[arg1].u.integer = 0;
});

OPCODE1(F_INC_LOCAL, "++local", {
  if( (Pike_fp->locals[arg1].type == PIKE_T_INT)
      DO_IF_BIGNUM(
      && (!INT_TYPE_ADD_OVERFLOW(Pike_fp->locals[arg1].u.integer, 1))
      )
      )
  {
    push_int(++(Pike_fp->locals[arg1].u.integer));
  } else {
    push_svalue(Pike_fp->locals+arg1);
    push_int(1);
    f_add(2);
    assign_svalue(Pike_fp->locals+arg1,Pike_sp-1);
  }
});

OPCODE1(F_POST_INC_LOCAL, "local++", {
  push_svalue( Pike_fp->locals + arg1);

  if( (Pike_fp->locals[arg1].type == PIKE_T_INT)
      DO_IF_BIGNUM(
      && (!INT_TYPE_ADD_OVERFLOW(Pike_fp->locals[arg1].u.integer, 1))
      )
      )
  {
    Pike_fp->locals[arg1].u.integer++;
  } else {
    push_svalue(Pike_fp->locals + arg1);
    push_int(1);
    f_add(2);
    stack_pop_to(Pike_fp->locals + arg1);
  }
});

OPCODE1(F_INC_LOCAL_AND_POP, "++local and pop", {
  if( (Pike_fp->locals[arg1].type == PIKE_T_INT)
      DO_IF_BIGNUM(
      && (!INT_TYPE_ADD_OVERFLOW(Pike_fp->locals[arg1].u.integer, 1))
      )
      )
  {
    Pike_fp->locals[arg1].u.integer++;
  } else {
    push_svalue( Pike_fp->locals + arg1);
    push_int(1);
    f_add(2);
    stack_pop_to(Pike_fp->locals + arg1);
  }
});

OPCODE1(F_DEC_LOCAL, "--local", {
  if( (Pike_fp->locals[arg1].type == PIKE_T_INT)
      DO_IF_BIGNUM(
      && (!INT_TYPE_SUB_OVERFLOW(Pike_fp->locals[arg1].u.integer, 1))
      )
      )
  {
    push_int(--(Pike_fp->locals[arg1].u.integer));
  } else {
    push_svalue(Pike_fp->locals+arg1);
    push_int(1);
    o_subtract();
    assign_svalue(Pike_fp->locals+arg1,Pike_sp-1);
  }
});

OPCODE1(F_POST_DEC_LOCAL, "local--", {
  push_svalue( Pike_fp->locals + arg1);

  if( (Pike_fp->locals[arg1].type == PIKE_T_INT)
      DO_IF_BIGNUM(
      && (!INT_TYPE_SUB_OVERFLOW(Pike_fp->locals[arg1].u.integer, 1))
      )
      )
  {
    Pike_fp->locals[arg1].u.integer--;
  } else {
    push_svalue(Pike_fp->locals + arg1);
    push_int(1);
    o_subtract();
    stack_pop_to(Pike_fp->locals + arg1);
  }
  /* Pike_fp->locals[instr].u.integer--; */
});

OPCODE1(F_DEC_LOCAL_AND_POP, "--local and pop", {
  if( (Pike_fp->locals[arg1].type == PIKE_T_INT)
      DO_IF_BIGNUM(
      && (!INT_TYPE_SUB_OVERFLOW(Pike_fp->locals[arg1].u.integer, 1))
      )
      )
  {
    Pike_fp->locals[arg1].u.integer--;
  } else {
    push_svalue(Pike_fp->locals + arg1);
    push_int(1);
    o_subtract();
    stack_pop_to(Pike_fp->locals + arg1);
  }
});

OPCODE0(F_LTOSVAL, "lvalue to svalue", {
  dmalloc_touch_svalue(Pike_sp-2);
  dmalloc_touch_svalue(Pike_sp-1);
  lvalue_to_svalue_no_free(Pike_sp, Pike_sp-2);
  Pike_sp++;
});

OPCODE0(F_LTOSVAL2, "ltosval2", {
  dmalloc_touch_svalue(Pike_sp-3);
  dmalloc_touch_svalue(Pike_sp-2);
  dmalloc_touch_svalue(Pike_sp-1);
  Pike_sp[0] = Pike_sp[-1];
  Pike_sp[-1].type = PIKE_T_INT;
  Pike_sp++;
  lvalue_to_svalue_no_free(Pike_sp-2, Pike_sp-4);

  /* this is so that foo+=bar (and similar things) will be faster, this
   * is done by freeing the old reference to foo after it has been pushed
   * on the stack. That way foo can have only 1 reference if we are lucky,
   * and then the low array/multiset/mapping manipulation routines can be
   * destructive if they like
   */
  if( (1 << Pike_sp[-2].type) &
      (BIT_ARRAY | BIT_MULTISET | BIT_MAPPING | BIT_STRING) )
  {
    struct svalue s;
    s.type = PIKE_T_INT;
    s.subtype = 0;
    s.u.integer = 0;
    assign_lvalue(Pike_sp-4, &s);
  }
});

OPCODE0(F_LTOSVAL3, "ltosval3", {
  Pike_sp[0] = Pike_sp[-1];
  Pike_sp[-1] = Pike_sp[-2];
  Pike_sp[-2].type = PIKE_T_INT;
  Pike_sp++;
  lvalue_to_svalue_no_free(Pike_sp-3, Pike_sp-5);

  /* this is so that foo=foo[x..y] (and similar things) will be faster, this
   * is done by freeing the old reference to foo after it has been pushed
   * on the stack. That way foo can have only 1 reference if we are lucky,
   * and then the low array/multiset/mapping manipulation routines can be
   * destructive if they like
   */
  if( (1 << Pike_sp[-3].type) &
      (BIT_ARRAY | BIT_MULTISET | BIT_MAPPING | BIT_STRING) )
  {
    struct svalue s;
    s.type = PIKE_T_INT;
    s.subtype = 0;
    s.u.integer = 0;
    assign_lvalue(Pike_sp-5, &s);
  }
});

OPCODE0(F_ADD_TO_AND_POP, "+= and pop", {
  Pike_sp[0]=Pike_sp[-1];
  Pike_sp[-1].type=PIKE_T_INT;
  Pike_sp++;
  lvalue_to_svalue_no_free(Pike_sp-2,Pike_sp-4);

  if( Pike_sp[-1].type == PIKE_T_INT &&
      Pike_sp[-2].type == PIKE_T_INT  )
  {
    DO_IF_BIGNUM(
    if(!INT_TYPE_ADD_OVERFLOW(Pike_sp[-1].u.integer, Pike_sp[-2].u.integer))
    )
    {
      /* Optimization for a rather common case. Makes it 30% faster. */
      Pike_sp[-1].u.integer += Pike_sp[-2].u.integer;
      assign_lvalue(Pike_sp-4,Pike_sp-1);
      Pike_sp-=2;
      pop_2_elems();
      goto add_and_pop_done;
    }
  }
  /* this is so that foo+=bar (and similar things) will be faster, this
   * is done by freeing the old reference to foo after it has been pushed
   * on the stack. That way foo can have only 1 reference if we are lucky,
   * and then the low array/multiset/mapping manipulation routines can be
   * destructive if they like
   */
  if( (1 << Pike_sp[-2].type) &
      (BIT_ARRAY | BIT_MULTISET | BIT_MAPPING | BIT_STRING) )
  {
    struct svalue s;
    s.type=PIKE_T_INT;
    s.subtype=0;
    s.u.integer=0;
    assign_lvalue(Pike_sp-4,&s);
  }
  f_add(2);
  assign_lvalue(Pike_sp-3,Pike_sp-1);
  pop_n_elems(3);
 add_and_pop_done:
   ; /* make gcc happy */
});

OPCODE1(F_GLOBAL_LVALUE, "& global", {
  struct identifier *i;
  INT32 tmp=arg1 + Pike_fp->context.identifier_level;
  if(!Pike_fp->current_object->prog)
    Pike_error("Cannot access global variables in destructed object.\n");
  i=ID_FROM_INT(Pike_fp->current_object->prog, tmp);

  if(!IDENTIFIER_IS_VARIABLE(i->identifier_flags))
    Pike_error("Cannot re-assign functions or constants.\n");

  if(i->run_time_type == PIKE_T_MIXED)
  {
    Pike_sp[0].type=T_LVALUE;
    Pike_sp[0].u.lval=(struct svalue *)GLOBAL_FROM_INT(tmp);
  }else{
    Pike_sp[0].type=T_SHORT_LVALUE;
    Pike_sp[0].u.short_lval= (union anything *)GLOBAL_FROM_INT(tmp);
    Pike_sp[0].subtype=i->run_time_type;
  }
  Pike_sp[1].type=T_VOID;
  Pike_sp+=2;
});

OPCODE0(F_INC, "++x", {
  union anything *u=get_pointer_if_this_type(Pike_sp-2, PIKE_T_INT);
  if(u
     DO_IF_BIGNUM(
     && !INT_TYPE_ADD_OVERFLOW(u->integer, 1)
     )
     )
  {
    INT32 val = ++u->integer;
    pop_2_elems();
    push_int(val);
  } else {
    lvalue_to_svalue_no_free(Pike_sp, Pike_sp-2); Pike_sp++;
    push_int(1);
    f_add(2);
    assign_lvalue(Pike_sp-3, Pike_sp-1);
    stack_unlink(2);
  }
});

OPCODE0(F_DEC, "--x", {
  union anything *u=get_pointer_if_this_type(Pike_sp-2, PIKE_T_INT);
  if(u
     DO_IF_BIGNUM(
     && !INT_TYPE_SUB_OVERFLOW(u->integer, 1)
     )
     )
  {
    INT32 val = --u->integer;
    pop_2_elems();
    push_int(val);
  } else {
    lvalue_to_svalue_no_free(Pike_sp, Pike_sp-2); Pike_sp++;
    push_int(1);
    o_subtract();
    assign_lvalue(Pike_sp-3, Pike_sp-1);
    stack_unlink(2);
  }
});

OPCODE0(F_DEC_AND_POP, "x-- and pop", {
  union anything *u=get_pointer_if_this_type(Pike_sp-2, PIKE_T_INT);
  if(u
     DO_IF_BIGNUM(
     && !INT_TYPE_SUB_OVERFLOW(u->integer, 1)
     )
)
  {
    --u->integer;
    pop_2_elems();
  }else{
    lvalue_to_svalue_no_free(Pike_sp, Pike_sp-2); Pike_sp++;
    push_int(1);
    o_subtract();
    assign_lvalue(Pike_sp-3, Pike_sp-1);
    pop_n_elems(3);
  }
});

OPCODE0(F_INC_AND_POP, "x++ and pop", {
  union anything *u=get_pointer_if_this_type(Pike_sp-2, PIKE_T_INT);
  if(u
     DO_IF_BIGNUM(
     && !INT_TYPE_ADD_OVERFLOW(u->integer, 1)
     )
     )
  {
    ++u->integer;
    pop_2_elems();
  } else {
    lvalue_to_svalue_no_free(Pike_sp, Pike_sp-2); Pike_sp++;
    push_int(1);
    f_add(2);
    assign_lvalue(Pike_sp-3, Pike_sp-1);
    pop_n_elems(3);
  }
});

OPCODE0(F_POST_INC, "x++", {
  union anything *u=get_pointer_if_this_type(Pike_sp-2, PIKE_T_INT);
  if(u
     DO_IF_BIGNUM(
     && !INT_TYPE_ADD_OVERFLOW(u->integer, 1)
     )
     )
  {
    INT32 val = u->integer++;
    pop_2_elems();
    push_int(val);
  } else {
    lvalue_to_svalue_no_free(Pike_sp, Pike_sp-2); Pike_sp++;
    stack_dup();
    push_int(1);
    f_add(2);
    assign_lvalue(Pike_sp-4, Pike_sp-1);
    pop_stack();
    stack_unlink(2);
    print_return_value();
  }
});

OPCODE0(F_POST_DEC, "x--", {
  union anything *u=get_pointer_if_this_type(Pike_sp-2, PIKE_T_INT);
  if(u
     DO_IF_BIGNUM(
     && !INT_TYPE_SUB_OVERFLOW(u->integer, 1)
     )
     )
  {
    INT32 val = u->integer--;
    pop_2_elems();
    push_int(val);
  } else {
    lvalue_to_svalue_no_free(Pike_sp, Pike_sp-2); Pike_sp++;
    stack_dup();
    push_int(1);
    o_subtract();
    assign_lvalue(Pike_sp-4, Pike_sp-1);
    pop_stack();
    stack_unlink(2);
    print_return_value();
  }
});

OPCODE1(F_ASSIGN_LOCAL, "assign local", {
  assign_svalue(Pike_fp->locals+arg1,Pike_sp-1);
});

OPCODE0(F_ASSIGN, "assign", {
  assign_lvalue(Pike_sp-3,Pike_sp-1);
  free_svalue(Pike_sp-3);
  free_svalue(Pike_sp-2);
  Pike_sp[-3]=Pike_sp[-1];
  Pike_sp-=2;
});

OPCODE2(F_APPLY_ASSIGN_LOCAL_AND_POP, "apply, assign local and pop", {
  apply_svalue(&((Pike_fp->context.prog->constants + arg1)->sval),
	       DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp)));
  free_svalue(Pike_fp->locals+arg2);
  Pike_fp->locals[arg2]=Pike_sp[-1];
  Pike_sp--;
});

OPCODE2(F_APPLY_ASSIGN_LOCAL, "apply, assign local", {
  apply_svalue(&((Pike_fp->context.prog->constants + arg1)->sval),
	       DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp)));
  assign_svalue(Pike_fp->locals+arg2, Pike_sp-1);
});

OPCODE0(F_ASSIGN_AND_POP, "assign and pop", {
  assign_lvalue(Pike_sp-3, Pike_sp-1);
  pop_n_elems(3);
});

OPCODE1(F_ASSIGN_LOCAL_AND_POP, "assign local and pop", {
  free_svalue(Pike_fp->locals + arg1);
  Pike_fp->locals[arg1] = Pike_sp[-1];
  Pike_sp--;
});

OPCODE1(F_ASSIGN_GLOBAL, "assign global", {
  struct identifier *i;
  INT32 tmp=arg1 + Pike_fp->context.identifier_level;
  if(!Pike_fp->current_object->prog)
    Pike_error("Cannot access global variables in destructed object.\n");

  i=ID_FROM_INT(Pike_fp->current_object->prog, tmp);
  if(!IDENTIFIER_IS_VARIABLE(i->identifier_flags))
    Pike_error("Cannot assign functions or constants.\n");
  if(i->run_time_type == PIKE_T_MIXED)
  {
    assign_svalue((struct svalue *)GLOBAL_FROM_INT(tmp), Pike_sp-1);
  }else{
    assign_to_short_svalue((union anything *)GLOBAL_FROM_INT(tmp),
			   i->run_time_type,
			   Pike_sp-1);
  }
});

OPCODE1(F_ASSIGN_GLOBAL_AND_POP, "assign global and pop", {
  struct identifier *i;
  INT32 tmp=arg1 + Pike_fp->context.identifier_level;
  if(!Pike_fp->current_object->prog)
    Pike_error("Cannot access global variables in destructed object.\n");

  i=ID_FROM_INT(Pike_fp->current_object->prog, tmp);
  if(!IDENTIFIER_IS_VARIABLE(i->identifier_flags))
    Pike_error("Cannot assign functions or constants.\n");

  if(i->run_time_type == PIKE_T_MIXED)
  {
    struct svalue *s=(struct svalue *)GLOBAL_FROM_INT(tmp);
    free_svalue(s);
    Pike_sp--;
    *s=*Pike_sp;
  }else{
    assign_to_short_svalue((union anything *)GLOBAL_FROM_INT(tmp),
			   i->run_time_type,
			   Pike_sp-1);
    pop_stack();
  }
});


/* Stack machine stuff */

OPCODE0(F_POP_VALUE, "pop", {
  pop_stack();
});

OPCODE1(F_POP_N_ELEMS, "pop_n_elems", {
  pop_n_elems(arg1);
});

OPCODE0_TAIL(F_MARK2, "mark mark", {
  *(Pike_mark_sp++)=Pike_sp;

/* This opcode is only used when running with -d. Identical to F_MARK,
 * but with a different name to make the debug printouts more clear. */
  OPCODE0_TAIL(F_SYNCH_MARK, "synch mark", {

    OPCODE0(F_MARK, "mark", {
      *(Pike_mark_sp++)=Pike_sp;
    });
  });
});

OPCODE1(F_MARK_X, "mark Pike_sp-X", {
  *(Pike_mark_sp++)=Pike_sp-arg1;
});

OPCODE0(F_POP_MARK, "pop mark", {
  --Pike_mark_sp;
});

OPCODE0(F_POP_TO_MARK, "pop to mark", {
  pop_n_elems(Pike_sp - *--Pike_mark_sp);
});

/* These opcodes are only used when running with -d. The reason for
 * the two aliases is mainly to keep the indentation in asm debug
 * output. */
OPCODE0_TAIL(F_CLEANUP_SYNCH_MARK, "cleanup synch mark", {
  OPCODE0(F_POP_SYNCH_MARK, "pop synch mark", {
    if (d_flag) {
      if (Pike_mark_sp <= Pike_interpreter.mark_stack) {
	fatal("Mark stack out of synch - 0x%08x <= 0x%08x.\n",
	      DO_NOT_WARN((unsigned long)Pike_mark_sp),
	      DO_NOT_WARN((unsigned long)Pike_interpreter.mark_stack));
      } else if (*--Pike_mark_sp != Pike_sp) {
	ptrdiff_t should = *Pike_mark_sp - Pike_interpreter.evaluator_stack;
	ptrdiff_t is = Pike_sp - Pike_interpreter.evaluator_stack;
	if (Pike_sp - *Pike_mark_sp > 0) /* not always same as Pike_sp > *Pike_mark_sp */
	/* Some attempt to recover, just to be able to report the backtrace. */
	  pop_n_elems(Pike_sp - *Pike_mark_sp);
	fatal("Stack out of synch - should be %ld, is %ld.\n",
	      DO_NOT_WARN((long)should), DO_NOT_WARN((long)is));
      }
    }
  });
});

OPCODE0(F_CLEAR_STRING_SUBTYPE, "clear string subtype", {
  if(Pike_sp[-1].type==PIKE_T_STRING) Pike_sp[-1].subtype=0;
});

      /* Jumps */
OPCODE0_JUMP(F_BRANCH, "branch", {
  DOJUMP();
});

OPCODE2_JUMP(F_BRANCH_IF_NOT_LOCAL_ARROW, "branch if !local->x", {
  struct svalue tmp;
  tmp.type=PIKE_T_STRING;
  tmp.u.string=Pike_fp->context.prog->strings[arg1];
  tmp.subtype=1;
  Pike_sp->type=PIKE_T_INT;	
  Pike_sp++;
  index_no_free(Pike_sp-1,Pike_fp->locals+arg2, &tmp);
  print_return_value();

  /* Fall through */

  OPCODE0_TAILJUMP(F_BRANCH_WHEN_ZERO, "branch if zero", {
    if(!IS_ZERO(Pike_sp-1))
    {
      SKIPJUMP();
    }else{
      DOJUMP();
    }
    pop_stack();
  });
});

      
OPCODE0_JUMP(F_BRANCH_WHEN_NON_ZERO, "branch if not zero", {
  if(IS_ZERO(Pike_sp-1))
  {
    SKIPJUMP();
  }else{
    DOJUMP();
  }
  pop_stack();
});

OPCODE1_JUMP(F_BRANCH_IF_TYPE_IS_NOT, "branch if type is !=", {
/*  fprintf(stderr,"******BRANCH IF TYPE IS NOT***** %s\n",get_name_of_type(arg1)); */
  if(Pike_sp[-1].type == T_OBJECT &&
     Pike_sp[-1].u.object->prog)
  {
    int fun=FIND_LFUN(Pike_sp[-1].u.object->prog, LFUN__IS_TYPE);
    if(fun != -1)
    {
/*      fprintf(stderr,"******OBJECT OVERLOAD IN TYPEP***** %s\n",get_name_of_type(arg1)); */
      push_text(get_name_of_type(arg1));
      apply_low(Pike_sp[-2].u.object, fun, 1);
      arg1=IS_ZERO(Pike_sp-1) ? T_FLOAT : T_OBJECT ;
      pop_stack();
    }
  }
  if(Pike_sp[-1].type == arg1)
  {
    SKIPJUMP();
  }else{
    DOJUMP();
  }
  pop_stack();
});

OPCODE1_JUMP(F_BRANCH_IF_LOCAL, "branch if local", {
  if(IS_ZERO(Pike_fp->locals + arg1))
  {
    SKIPJUMP();
  }else{
    DOJUMP();
  }
});

OPCODE1_JUMP(F_BRANCH_IF_NOT_LOCAL, "branch if !local", {
  if(!IS_ZERO(Pike_fp->locals + arg1))
  {
    SKIPJUMP();
  }else{
    DOJUMP();
  }
});

#define CJUMP(X, DESC, Y) \
  OPCODE0_JUMP(X, DESC, { \
    if(Y(Pike_sp-2,Pike_sp-1)) { \
      DOJUMP(); \
    }else{ \
      SKIPJUMP(); \
    } \
    pop_2_elems(); \
  })

CJUMP(F_BRANCH_WHEN_EQ, "branch if ==", is_eq);
CJUMP(F_BRANCH_WHEN_NE, "branch if !=", !is_eq);
CJUMP(F_BRANCH_WHEN_LT, "branch if <", is_lt);
CJUMP(F_BRANCH_WHEN_LE, "branch if <=", !is_gt);
CJUMP(F_BRANCH_WHEN_GT, "branch if >", is_gt);
CJUMP(F_BRANCH_WHEN_GE, "branch if >=", !is_lt);

OPCODE0_JUMP(F_BRANCH_AND_POP_WHEN_ZERO, "branch & pop if zero", {
  if(!IS_ZERO(Pike_sp-1))
  {
    SKIPJUMP();
  }else{
    DOJUMP();
    pop_stack();
  }
});

OPCODE0_JUMP(F_BRANCH_AND_POP_WHEN_NON_ZERO, "branch & pop if !zero", {
  if(IS_ZERO(Pike_sp-1))
  {
    SKIPJUMP();
  }else{
    DOJUMP();
    pop_stack();
  }
});

OPCODE0_JUMP(F_LAND, "&&", {
  if(!IS_ZERO(Pike_sp-1))
  {
    SKIPJUMP();
    pop_stack();
  }else{
    DOJUMP();
  }
});

OPCODE0_JUMP(F_LOR, "||", {
  if(IS_ZERO(Pike_sp-1))
  {
    SKIPJUMP();
    pop_stack();
  }else{
    DOJUMP();
  }
});

OPCODE0_JUMP(F_EQ_OR, "==||", {
  if(!is_eq(Pike_sp-2,Pike_sp-1))
  {
    pop_2_elems();
    SKIPJUMP();
  }else{
    pop_2_elems();
    push_int(1);
    DOJUMP();
  }
});

OPCODE0_JUMP(F_EQ_AND, "==&&", {
  if(is_eq(Pike_sp-2,Pike_sp-1))
  {
    pop_2_elems();
    SKIPJUMP();
  }else{
    pop_2_elems();
    push_int(0);
    DOJUMP();
  }
});

OPCODE0_JUMP(F_CATCH, "catch", {
  check_c_stack(8192);
  switch (o_catch((PIKE_OPCODE_T *)(((INT32 *)PROG_COUNTER)+1)))
  {
  case 1:
    /* There was a return inside the evaluated code */
    DO_DUMB_RETURN;
  case 2:
    DO_JUMP_TO(Pike_fp->pc);
    break;
  default:
    DO_JUMP_TO(PROG_COUNTER + GET_JUMP());
  }
  /* NOT_REACHED */
});

OPCODE0_RETURN(F_ESCAPE_CATCH, "escape catch", {
  Pike_fp->pc = PROG_COUNTER;
  INTER_ESCAPE_CATCH;
});

OPCODE0(F_THROW_ZERO, "throw(0)", {
  push_int(0);
  f_throw(1);
});

OPCODE1(F_SWITCH, "switch", {
  INT32 tmp;
  PIKE_OPCODE_T *addr = PROG_COUNTER;
  tmp=switch_lookup(Pike_fp->context.prog->
		    constants[arg1].sval.u.array,Pike_sp-1);
  addr = DO_IF_ELSE_COMPUTED_GOTO(addr, (PIKE_OPCODE_T *)
				  DO_ALIGN(addr,((ptrdiff_t)sizeof(INT32))));
  addr = (PIKE_OPCODE_T *)(((INT32 *)addr) + (tmp>=0 ? 1+tmp*2 : 2*~tmp));
  if(*(INT32*)addr < 0) fast_check_threads_etc(7);
  pop_stack();
  DO_JUMP_TO(addr + *(INT32*)addr);
});

OPCODE1(F_SWITCH_ON_INDEX, "switch on index", {
  INT32 tmp;
  struct svalue s;
  PIKE_OPCODE_T *addr = PROG_COUNTER;
  index_no_free(&s,Pike_sp-2,Pike_sp-1);
  Pike_sp++[0]=s;

  tmp=switch_lookup(Pike_fp->context.prog->
		    constants[arg1].sval.u.array,Pike_sp-1);
  pop_n_elems(3);
  addr = DO_IF_ELSE_COMPUTED_GOTO(addr, (PIKE_OPCODE_T *)
				  DO_ALIGN(addr,((ptrdiff_t)sizeof(INT32))));
  addr = (PIKE_OPCODE_T *)(((INT32 *)addr) + (tmp>=0 ? 1+tmp*2 : 2*~tmp));
  if(*(INT32*)addr < 0) fast_check_threads_etc(7);
  DO_JUMP_TO(addr + *(INT32*)addr);
});

OPCODE2(F_SWITCH_ON_LOCAL, "switch on local", {
  INT32 tmp;
  PIKE_OPCODE_T *addr = PROG_COUNTER;
  tmp=switch_lookup(Pike_fp->context.prog->
		    constants[arg2].sval.u.array,Pike_fp->locals + arg1);
  addr = DO_IF_ELSE_COMPUTED_GOTO(addr, (PIKE_OPCODE_T *)
				  DO_ALIGN(addr,((ptrdiff_t)sizeof(INT32))));
  addr = (PIKE_OPCODE_T *)(((INT32 *)addr) + (tmp>=0 ? 1+tmp*2 : 2*~tmp));
  if(*(INT32*)addr < 0) fast_check_threads_etc(7);
  DO_JUMP_TO(addr + *(INT32*)addr);
});


#ifdef AUTO_BIGNUM
#define AUTO_BIGNUM_LOOP_TEST(X,Y) INT_TYPE_ADD_OVERFLOW(X,Y)
#else
#define AUTO_BIGNUM_LOOP_TEST(X,Y) 0
#endif

      /* FIXME: Does this need bignum tests? /Fixed - Hubbe */
      /* LOOP(OPCODE, INCREMENT, OPERATOR, IS_OPERATOR) */
#define LOOP(ID, DESC, INC, OP2, OP4)					\
  OPCODE0_JUMP(ID, DESC, {						\
    union anything *i=get_pointer_if_this_type(Pike_sp-2, T_INT);	\
    if(i && !AUTO_BIGNUM_LOOP_TEST(i->integer,INC))			\
    {									\
      i->integer += INC;						\
      if(i->integer OP2 Pike_sp[-3].u.integer)				\
      {									\
  	DOJUMP();							\
      }else{								\
  	SKIPJUMP();							\
      }									\
    }else{								\
      lvalue_to_svalue_no_free(Pike_sp,Pike_sp-2); Pike_sp++;		\
      push_int(INC);							\
      f_add(2);								\
      assign_lvalue(Pike_sp-3,Pike_sp-1);				\
      if(OP4 ( Pike_sp-1, Pike_sp-4 ))					\
      {									\
  	DOJUMP();							\
      }else{								\
  	SKIPJUMP();							\
      }									\
      pop_stack();							\
    }									\
  })

LOOP(F_INC_LOOP, "++Loop", 1, <, is_lt);
LOOP(F_DEC_LOOP, "--Loop", -1, >, is_gt);
LOOP(F_INC_NEQ_LOOP, "++Loop!=", 1, !=, !is_eq);
LOOP(F_DEC_NEQ_LOOP, "--Loop!=", -1, !=, !is_eq);

/* Use like:
 *
 * push(loopcnt)
 * branch(l2)
 * l1:
 *   sync_mark
 *     code
 *   pop_sync_mark
 * l2:
 * loop(l1)
 */
OPCODE0_JUMP(F_LOOP, "loop", { /* loopcnt */
  /* Use >= and 1 to be able to reuse the 1 for the subtraction. */
  push_int(1);
  if (!is_lt(sp-2, sp-1)) {
    o_subtract();
    DOJUMP();
  } else {
    pop_2_elems();
    SKIPJUMP();
  }
});

OPCODE0_JUMP(F_FOREACH, "foreach", { /* array, lvalue, X, i */
  if(Pike_sp[-4].type != PIKE_T_ARRAY)
    PIKE_ERROR("foreach", "Bad argument 1.\n", Pike_sp-3, 1);
  if(Pike_sp[-1].u.integer < Pike_sp[-4].u.array->size)
  {
    if(Pike_sp[-1].u.integer < 0)
      Pike_error("Foreach loop variable is negative!\n");
    assign_lvalue(Pike_sp-3, Pike_sp[-4].u.array->item + Pike_sp[-1].u.integer);
    DOJUMP();
    Pike_sp[-1].u.integer++;
  }else{
    SKIPJUMP();
  }
});

OPCODE0(F_MAKE_ITERATOR, "Iterator", {
  extern void f_Iterator(INT32);
  f_Iterator(1);
});

OPCODE0_JUMP(F_NEW_FOREACH, "foreach++", { /* iterator, lvalue, lvalue */
  extern int foreach_iterate(struct object *o);

  if(Pike_sp[-5].type != PIKE_T_OBJECT)
    PIKE_ERROR("foreach", "Bad argument 1.\n", Pike_sp-3, 1);
  if(foreach_iterate(Pike_sp[-5].u.object))
  {
    DOJUMP();
  }else{
    SKIPJUMP();
  }
});


OPCODE1_RETURN(F_RETURN_LOCAL,"return local",{
  DO_IF_DEBUG(
    /* special case! Pike_interpreter.mark_stack may be invalid at the time we
     * call return -1, so we must call the callbacks here to
     * prevent false alarms! /Hubbe
     */
    if(d_flag>3) do_gc();
    if(d_flag>4) do_debug();
    check_threads_etc();
    );
  if(Pike_fp->expendible <= Pike_fp->locals + arg1)
  {
    pop_n_elems(Pike_sp-1 - (Pike_fp->locals + arg1));
  }else{
    push_svalue(Pike_fp->locals + arg1);
  }
  print_return_value();
  DO_DUMB_RETURN;
});


OPCODE0_RETURN(F_RETURN_IF_TRUE,"return if true",{
  if(!IS_ZERO(Pike_sp-1)) DO_RETURN;
  pop_stack();
});

OPCODE0_RETURN(F_RETURN_1,"return 1",{
  push_int(1);
  DO_RETURN;
});

OPCODE0_RETURN(F_RETURN_0,"return 0",{
  push_int(0);
  DO_RETURN;
});

OPCODE0_RETURN(F_RETURN, "return", {
  DO_RETURN;
});

OPCODE0_RETURN(F_DUMB_RETURN,"dumb return", {
  DO_DUMB_RETURN;
});

OPCODE0(F_NEGATE, "unary minus", {
  if(Pike_sp[-1].type == PIKE_T_INT)
  {
    DO_IF_BIGNUM(
      if(INT_TYPE_NEG_OVERFLOW(Pike_sp[-1].u.integer))
      {
	convert_stack_top_to_bignum();
	o_negate();
      }
      else
      )
      Pike_sp[-1].u.integer =- Pike_sp[-1].u.integer;
  }
  else if(Pike_sp[-1].type == PIKE_T_FLOAT)
  {
    Pike_sp[-1].u.float_number =- Pike_sp[-1].u.float_number;
  }else{
    o_negate();
  }
});

OPCODE0(F_COMPL, "~", {
  o_compl();
});

OPCODE0(F_NOT, "!", {
  switch(Pike_sp[-1].type)
  {
  case PIKE_T_INT:
    Pike_sp[-1].u.integer =! Pike_sp[-1].u.integer;
    break;

  case PIKE_T_FUNCTION:
  case PIKE_T_OBJECT:
    if(IS_ZERO(Pike_sp-1))
    {
      pop_stack();
      push_int(1);
    }else{
      pop_stack();
      push_int(0);
    }
    break;

  default:
    free_svalue(Pike_sp-1);
    Pike_sp[-1].type=PIKE_T_INT;
    Pike_sp[-1].u.integer=0;
  }
});

OPCODE0(F_LSH, "<<", {
  o_lsh();
});

OPCODE0(F_RSH, ">>", {
  o_rsh();
});

#define COMPARISON(ID,DESC,EXPR)	\
  OPCODE0(ID, DESC, {			\
    INT32 val = EXPR;			\
    pop_2_elems();			\
    push_int(val);			\
  })

COMPARISON(F_EQ, "==", is_eq(Pike_sp-2,Pike_sp-1));
COMPARISON(F_NE, "!=", !is_eq(Pike_sp-2,Pike_sp-1));
COMPARISON(F_GT, ">", is_gt(Pike_sp-2,Pike_sp-1));
COMPARISON(F_GE, ">=", !is_lt(Pike_sp-2,Pike_sp-1));
COMPARISON(F_LT, "<", is_lt(Pike_sp-2,Pike_sp-1));
COMPARISON(F_LE, "<=", !is_gt(Pike_sp-2,Pike_sp-1));

OPCODE0(F_ADD, "+", {
  f_add(2);
});

OPCODE0(F_ADD_INTS, "int+int", {
  if(Pike_sp[-1].type == T_INT && Pike_sp[-2].type == T_INT 
     DO_IF_BIGNUM(
      && (!INT_TYPE_ADD_OVERFLOW(Pike_sp[-1].u.integer, Pike_sp[-2].u.integer))
      )
    )
  {
    Pike_sp[-2].u.integer+=Pike_sp[-1].u.integer;
    Pike_sp--;
  }else{
    f_add(2);
  }
});

OPCODE0(F_ADD_FLOATS, "float+float", {
  if(Pike_sp[-1].type == T_FLOAT && Pike_sp[-2].type == T_FLOAT)
  {
    Pike_sp[-2].u.float_number+=Pike_sp[-1].u.float_number;
    Pike_sp--;
  }else{
    f_add(2);
  }
});

OPCODE0(F_SUBTRACT, "-", {
  o_subtract();
});

OPCODE0(F_AND, "&", {
  o_and();
});

OPCODE0(F_OR, "|", {
  o_or();
});

OPCODE0(F_XOR, "^", {
  o_xor();
});

OPCODE0(F_MULTIPLY, "*", {
  o_multiply();
});

OPCODE0(F_DIVIDE, "/", {
  o_divide();
});

OPCODE0(F_MOD, "%", {
  o_mod();
});

OPCODE1(F_ADD_INT, "add integer", {
  if(Pike_sp[-1].type == T_INT
     DO_IF_BIGNUM(
      && (!INT_TYPE_ADD_OVERFLOW(Pike_sp[-1].u.integer, arg1))
      )
     )
  {
    Pike_sp[-1].u.integer+=arg1;
  }else{
    push_int(arg1);
    f_add(2);
  }
});

OPCODE1(F_ADD_NEG_INT, "add -integer", {
  if(Pike_sp[-1].type == T_INT
     DO_IF_BIGNUM(
      && (!INT_TYPE_ADD_OVERFLOW(Pike_sp[-1].u.integer, -arg1))
      )
     )
  {
    Pike_sp[-1].u.integer-=arg1;
  }else{
    push_int(-arg1);
    f_add(2);
  }
});

OPCODE0(F_PUSH_ARRAY, "@", {
  switch(Pike_sp[-1].type)
  {
  default:
    PIKE_ERROR("@", "Bad argument.\n", Pike_sp, 1);
    
  case PIKE_T_OBJECT:
    if(!Pike_sp[-1].u.object->prog ||
       FIND_LFUN(Pike_sp[-1].u.object->prog,LFUN__VALUES) == -1)
      PIKE_ERROR("@", "Bad argument.\n", Pike_sp, 1);

    apply_lfun(Pike_sp[-1].u.object, LFUN__VALUES, 0);
    if(Pike_sp[-1].type != PIKE_T_ARRAY)
      Pike_error("Bad return type from o->_values() in @\n");
    free_svalue(Pike_sp-2);
    Pike_sp[-2]=Pike_sp[-1];
    Pike_sp--;
    break;

  case PIKE_T_ARRAY: break;
  }
  Pike_sp--;
  push_array_items(Pike_sp->u.array);
});

OPCODE2(F_LOCAL_LOCAL_INDEX, "local[local]", {
  struct svalue *s=Pike_fp->locals+arg1;
  if(s->type == PIKE_T_STRING) s->subtype=0;
  Pike_sp++->type=PIKE_T_INT;
  index_no_free(Pike_sp-1,Pike_fp->locals+arg2,s);
});

OPCODE1(F_LOCAL_INDEX, "local index", {
  struct svalue tmp;
  struct svalue *s = Pike_fp->locals+arg1;
  if(s->type == PIKE_T_STRING) s->subtype=0;
  index_no_free(&tmp,Pike_sp-1,s);
  free_svalue(Pike_sp-1);
  Pike_sp[-1]=tmp;
});

OPCODE2(F_GLOBAL_LOCAL_INDEX, "global[local]", {
  struct svalue tmp;
  struct svalue *s;
  low_object_index_no_free(Pike_sp,
			   Pike_fp->current_object,
			   arg1 + Pike_fp->context.identifier_level);
  Pike_sp++;
  s=Pike_fp->locals+arg2;
  if(s->type == PIKE_T_STRING) s->subtype=0;
  index_no_free(&tmp,Pike_sp-1,s);
  free_svalue(Pike_sp-1);
  Pike_sp[-1]=tmp;
});

OPCODE2(F_LOCAL_ARROW, "local->x", {
  struct svalue tmp;
  tmp.type=PIKE_T_STRING;
  tmp.u.string=Pike_fp->context.prog->strings[arg1];
  tmp.subtype=1;
  Pike_sp->type=PIKE_T_INT;	
  Pike_sp++;
  index_no_free(Pike_sp-1,Pike_fp->locals+arg2, &tmp);
  print_return_value();
});

OPCODE1(F_ARROW, "->x", {
  struct svalue tmp;
  struct svalue tmp2;
  tmp.type=PIKE_T_STRING;
  tmp.u.string=Pike_fp->context.prog->strings[arg1];
  tmp.subtype=1;
  index_no_free(&tmp2, Pike_sp-1, &tmp);
  free_svalue(Pike_sp-1);
  Pike_sp[-1]=tmp2;
  print_return_value();
});

OPCODE1(F_STRING_INDEX, "string index", {
  struct svalue tmp;
  struct svalue tmp2;
  tmp.type=PIKE_T_STRING;
  tmp.u.string=Pike_fp->context.prog->strings[arg1];
  tmp.subtype=0;
  index_no_free(&tmp2, Pike_sp-1, &tmp);
  free_svalue(Pike_sp-1);
  Pike_sp[-1]=tmp2;
  print_return_value();
});

OPCODE1(F_POS_INT_INDEX, "int index", {
  push_int(arg1);
  print_return_value();
  DO_INDEX;
});

OPCODE1(F_NEG_INT_INDEX, "-int index", {
  push_int(-(ptrdiff_t)arg1);
  print_return_value();
  DO_INDEX;
});

OPCODE0(F_INDEX, "index", {
  DO_INDEX;
});

OPCODE2(F_MAGIC_INDEX, "::`[]", {
  push_magic_index(magic_index_program, arg2, arg1);
});

OPCODE2(F_MAGIC_SET_INDEX, "::`[]=", {
  push_magic_index(magic_set_index_program, arg2, arg1);
});

OPCODE0(F_CAST, "cast", {
  f_cast();
});

OPCODE0(F_CAST_TO_INT, "cast_to_int", {
  o_cast_to_int();
});

OPCODE0(F_CAST_TO_STRING, "cast_to_string", {
  o_cast_to_string();
});

OPCODE0(F_SOFT_CAST, "soft cast", {
  /* Stack: type_string, value */
  DO_IF_DEBUG({
    if (Pike_sp[-2].type != T_TYPE) {
      fatal("Argument 1 to soft_cast isn't a type!\n");
    }
  });
  if (runtime_options & RUNTIME_CHECK_TYPES) {
    struct pike_type *sval_type = get_type_of_svalue(Pike_sp-1);
    if (!pike_types_le(sval_type, Pike_sp[-2].u.type)) {
      /* get_type_from_svalue() doesn't return a fully specified type
       * for array, mapping and multiset, so we perform a more lenient
       * check for them.
       */
      if (!pike_types_le(sval_type, weak_type_string) ||
	  !match_types(sval_type, Pike_sp[-2].u.type)) {
	struct pike_string *t1;
	struct pike_string *t2;
	char *fname = "__soft-cast";
	ONERROR tmp1;
	ONERROR tmp2;

	if (Pike_fp->current_object && Pike_fp->context.prog &&
	    Pike_fp->current_object->prog) {
	  /* Look up the function-name */
	  struct pike_string *name =
	    ID_FROM_INT(Pike_fp->current_object->prog, Pike_fp->fun)->name;
	  if ((!name->size_shift) && (name->len < 100))
	    fname = name->str;
	}

	t1 = describe_type(Pike_sp[-2].u.type);
	SET_ONERROR(tmp1, do_free_string, t1);
	  
	t2 = describe_type(sval_type);
	SET_ONERROR(tmp2, do_free_string, t2);
	  
	free_type(sval_type);

	bad_arg_error(NULL, Pike_sp-1, 1, 1, t1->str, Pike_sp-1,
		      "%s(): Soft cast failed. Expected %s, got %s\n",
		      fname, t1->str, t2->str);
	/* NOT_REACHED */
	UNSET_ONERROR(tmp2);
	UNSET_ONERROR(tmp1);
	free_string(t2);
	free_string(t1);
      }
    }
    free_type(sval_type);

    DO_IF_DEBUG({
      if (d_flag > 2) {
	struct pike_string *t = describe_type(Pike_sp[-2].u.type);
	fprintf(stderr, "Soft cast to %s\n", t->str);
	free_string(t);
      }
    });
  }
  stack_swap();
  pop_stack();
});

OPCODE0(F_RANGE, "range", {
  o_range();
});

OPCODE0(F_COPY_VALUE, "copy_value", {
  struct svalue tmp;
  copy_svalues_recursively_no_free(&tmp,Pike_sp-1,1,0);
  free_svalue(Pike_sp-1);
  Pike_sp[-1]=tmp;
});

OPCODE0(F_INDIRECT, "indirect", {
  struct svalue s;
  lvalue_to_svalue_no_free(&s,Pike_sp-2);
  if(s.type != PIKE_T_STRING)
  {
    pop_2_elems();
    *Pike_sp=s;
    Pike_sp++;
  }else{
    struct object *o;
    o=low_clone(string_assignment_program);
    ((struct string_assignment_storage *)o->storage)->lval[0]=Pike_sp[-2];
    ((struct string_assignment_storage *)o->storage)->lval[1]=Pike_sp[-1];
    ((struct string_assignment_storage *)o->storage)->s=s.u.string;
    Pike_sp-=2;
    push_object(o);
  }
  print_return_value();
});
      
OPCODE0(F_SIZEOF, "sizeof", {
  INT32 val = pike_sizeof(Pike_sp-1);
  pop_stack();
  push_int(val);
});

OPCODE1(F_SIZEOF_LOCAL, "sizeof local", {
  push_int(pike_sizeof(Pike_fp->locals+arg1));
});

OPCODE1(F_SSCANF, "sscanf", {
  o_sscanf(arg1);
});

#define MKAPPLY(OP,OPCODE,NAME,TYPE,  ARG2, ARG3)			   \
OP(PIKE_CONCAT(F_,OPCODE),NAME, {					   \
if(low_mega_apply(TYPE,DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp)),	   \
		  ARG2, ARG3))						   \
{									   \
  Pike_fp->next->pc=PROG_COUNTER;					   \
  Pike_fp->flags |= PIKE_FRAME_RETURN_INTERNAL;				   \
  DO_JUMP_TO(Pike_fp->pc);						   \
}									   \
});									   \
									   \
OP(PIKE_CONCAT3(F_,OPCODE,_AND_POP),NAME " & pop", {			   \
  if(low_mega_apply(TYPE, DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp)), \
		    ARG2, ARG3))					   \
  {									   \
    Pike_fp->next->pc=PROG_COUNTER;					   \
    Pike_fp->flags |= PIKE_FRAME_RETURN_INTERNAL | PIKE_FRAME_RETURN_POP;  \
    DO_JUMP_TO(Pike_fp->pc);						   \
  }else{								   \
    pop_stack();							   \
  }									   \
});									   \
									   \
PIKE_CONCAT(OP,_RETURN)(PIKE_CONCAT3(F_,OPCODE,_AND_RETURN),		   \
			NAME " & return", {				   \
  if(low_mega_apply(TYPE,DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp)),  \
		    ARG2,ARG3))						   \
  {									   \
    PIKE_OPCODE_T *addr = Pike_fp->pc;					   \
    DO_IF_DEBUG(Pike_fp->next->pc=0);					   \
    unlink_previous_frame();						   \
    DO_JUMP_TO(addr);							   \
  }else{								   \
    DO_DUMB_RETURN;							   \
  }									   \
});									   \


#define MKAPPLY2(OP,OPCODE,NAME,TYPE,  ARG2, ARG3)			   \
									   \
MKAPPLY(OP,OPCODE,NAME,TYPE,  ARG2, ARG3)			           \
									   \
OP(PIKE_CONCAT(F_MARK_,OPCODE),"mark, " NAME, {				   \
  if(low_mega_apply(TYPE,0,						   \
		    ARG2, ARG3))					   \
  {									   \
    Pike_fp->next->pc=PROG_COUNTER;					   \
    Pike_fp->flags |= PIKE_FRAME_RETURN_INTERNAL;			   \
    DO_JUMP_TO(Pike_fp->pc);						   \
  }									   \
});									   \
									   \
OP(PIKE_CONCAT3(F_MARK_,OPCODE,_AND_POP),"mark, " NAME " & pop", {	   \
  if(low_mega_apply(TYPE, 0,						   \
		    ARG2, ARG3))					   \
  {									   \
    Pike_fp->next->pc=PROG_COUNTER;					   \
    Pike_fp->flags |= PIKE_FRAME_RETURN_INTERNAL | PIKE_FRAME_RETURN_POP;  \
    DO_JUMP_TO(Pike_fp->pc);						   \
  }else{								   \
    pop_stack();							   \
  }									   \
});									   \
									   \
PIKE_CONCAT(OP,_RETURN)(PIKE_CONCAT3(F_MARK_,OPCODE,_AND_RETURN),	   \
			"mark, " NAME " & return", {			   \
  if(low_mega_apply(TYPE,0,						   \
		    ARG2,ARG3))						   \
  {									   \
    PIKE_OPCODE_T *addr = Pike_fp->pc;					   \
    DO_IF_DEBUG(Pike_fp->next->pc=0);					   \
    unlink_previous_frame();						   \
    DO_JUMP_TO(addr);							   \
  }else{								   \
    DO_DUMB_RETURN;							   \
  }									   \
})


MKAPPLY2(OPCODE1,CALL_LFUN,"call lfun",APPLY_LOW,
	 Pike_fp->current_object,
	 (void *)(arg1+Pike_fp->context.identifier_level));

MKAPPLY2(OPCODE1,APPLY,"apply",APPLY_SVALUE_STRICT,
	 &((Pike_fp->context.prog->constants + arg1)->sval),0);

MKAPPLY(OPCODE0,CALL_FUNCTION,"call function",APPLY_STACK, 0,0);

OPCODE1(F_CALL_OTHER,"call other", {
  INT32 args=DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp));
  struct svalue *s=Pike_sp-args;
  if(s->type == T_OBJECT)
  {
    struct object *o=s->u.object;
    struct program *p;
    if((p=o->prog))
    {
      if(FIND_LFUN(p, LFUN_ARROW) == -1)
      {
	int fun;
	fun=find_shared_string_identifier(Pike_fp->context.prog->strings[arg1],
					  p);
	if(fun >= 0)
	{
	  if(low_mega_apply(APPLY_LOW, args-1, o, (void *)fun))
	  {
	    Pike_fp->save_sp--;
	    Pike_fp->next->pc=PROG_COUNTER;
	    Pike_fp->flags |= PIKE_FRAME_RETURN_INTERNAL;
	    DO_JUMP_TO(Pike_fp->pc);
	  }
	  stack_unlink(1);
	  DONE;
	}
      }
    }
  }

  {
    struct svalue tmp;
    struct svalue tmp2;

    tmp.type=PIKE_T_STRING;
    tmp.u.string=Pike_fp->context.prog->strings[arg1];
    tmp.subtype=1;

    index_no_free(&tmp2, s, &tmp);
    free_svalue(s);
    *s=tmp2;
    print_return_value();

    if(low_mega_apply(APPLY_STACK, args, 0, 0))
    {
      Pike_fp->next->pc=PROG_COUNTER;
      Pike_fp->flags |= PIKE_FRAME_RETURN_INTERNAL;
      DO_JUMP_TO(Pike_fp->pc);
    }
    DONE;
  }
});

OPCODE1(F_CALL_OTHER_AND_POP,"call other & pop", {
  INT32 args=DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp));
  struct svalue *s=Pike_sp-args;
  if(s->type == T_OBJECT)
  {
    struct object *o=s->u.object;
    struct program *p;
    if((p=o->prog))
    {
      if(FIND_LFUN(p, LFUN_ARROW) == -1)
      {
	int fun;
	fun=find_shared_string_identifier(Pike_fp->context.prog->strings[arg1],
					  p);
	if(fun >= 0)
	{
	  if(low_mega_apply(APPLY_LOW, args-1, o, (void *)fun))
	  {
	    Pike_fp->save_sp--;
	    Pike_fp->next->pc=PROG_COUNTER;
	    Pike_fp->flags |= 
	      PIKE_FRAME_RETURN_INTERNAL |
	      PIKE_FRAME_RETURN_POP;
	    DO_JUMP_TO(Pike_fp->pc);
	  }
	  pop_2_elems();
	  DONE;
	}
      }
    }
  }

  {
    struct svalue tmp;
    struct svalue tmp2;

    tmp.type=PIKE_T_STRING;
    tmp.u.string=Pike_fp->context.prog->strings[arg1];
    tmp.subtype=1;

    index_no_free(&tmp2, s, &tmp);
    free_svalue(s);
    *s=tmp2;
    print_return_value();

    if(low_mega_apply(APPLY_STACK, args, 0, 0))
    {
      Pike_fp->next->pc=PROG_COUNTER;
      Pike_fp->flags |= PIKE_FRAME_RETURN_INTERNAL | PIKE_FRAME_RETURN_POP;
      DO_JUMP_TO(Pike_fp->pc);
    }
    pop_stack();
  }
});

OPCODE1(F_CALL_OTHER_AND_RETURN,"call other & return", {
  INT32 args=DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp));
  struct svalue *s=Pike_sp-args;
  if(s->type == T_OBJECT)
  {
    struct object *o=s->u.object;
    struct program *p;
    if((p=o->prog))
    {
      if(FIND_LFUN(p, LFUN_ARROW) == -1)
      {
	int fun;
	fun=find_shared_string_identifier(Pike_fp->context.prog->strings[arg1],
					  p);
	if(fun >= 0)
	{
	  if(low_mega_apply(APPLY_LOW, args-1, o, (void *)fun))
	  {
	    PIKE_OPCODE_T *addr = Pike_fp->pc;
	    Pike_fp->save_sp--;
	    DO_IF_DEBUG(Pike_fp->next->pc=0);
	    unlink_previous_frame();
	    DO_JUMP_TO(addr);
	  }
	  stack_unlink(1);
	  DO_DUMB_RETURN;
	}
      }
    }
  }

  {
    struct svalue tmp;
    struct svalue tmp2;

    tmp.type=PIKE_T_STRING;
    tmp.u.string=Pike_fp->context.prog->strings[arg1];
    tmp.subtype=1;

    index_no_free(&tmp2, s, &tmp);
    free_svalue(s);
    *s=tmp2;
    print_return_value();

    if(low_mega_apply(APPLY_STACK, args, 0, 0))
    {
      PIKE_OPCODE_T *addr = Pike_fp->pc;
      DO_IF_DEBUG(Pike_fp->next->pc=0);
      unlink_previous_frame();
      DO_JUMP_TO(addr);
    }
    DO_DUMB_RETURN;
  }
});

#undef DO_CALL_BUILTIN
#ifdef PIKE_DEBUG
#define DO_CALL_BUILTIN(ARGS) do {					 \
  int args=(ARGS);							 \
  struct svalue *expected_stack=Pike_sp-args;				 \
    struct svalue *s=&Pike_fp->context.prog->constants[arg1].sval;	 \
  if(t_flag>1)								 \
  {									 \
    init_buf();								 \
    describe_svalue(s, 0,0);						 \
    do_trace_call(args);						 \
  }									 \
  (*(s->u.efun->function))(args);					 \
  s->u.efun->runs++;                                                     \
  if(Pike_sp != expected_stack + !s->u.efun->may_return_void)		 \
  {									 \
    if(Pike_sp < expected_stack)					 \
      fatal("Function popped too many arguments: %s\n",			 \
	    s->u.efun->name->str);					 \
    if(Pike_sp>expected_stack+1)					 \
      fatal("Function left %d droppings on stack: %s\n",		 \
           Pike_sp-(expected_stack+1),					 \
	    s->u.efun->name->str);					 \
    if(Pike_sp == expected_stack && !s->u.efun->may_return_void)	 \
      fatal("Non-void function returned without return value "		 \
	    "on stack: %s %d\n",					 \
	    s->u.efun->name->str,s->u.efun->may_return_void);		 \
    if(Pike_sp==expected_stack+1 && s->u.efun->may_return_void)		 \
      fatal("Void function returned with a value on the stack: %s %d\n", \
	    s->u.efun->name->str, s->u.efun->may_return_void);		 \
  }									 \
  if(t_flag>1 && Pike_sp>expected_stack) trace_return_value();		 \
}while(0)
#else
#define DO_CALL_BUILTIN(ARGS) \
(*(Pike_fp->context.prog->constants[arg1].sval.u.efun->function))(ARGS)
#endif

OPCODE1(F_CALL_BUILTIN, "call builtin", {
  DO_CALL_BUILTIN(DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp)));
});

OPCODE1(F_CALL_BUILTIN_AND_POP,"call builtin & pop", {
  DO_CALL_BUILTIN(DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp)));
  pop_stack();
});

OPCODE1_RETURN(F_CALL_BUILTIN_AND_RETURN,"call builtin & return", {
  DO_CALL_BUILTIN(DO_NOT_WARN((INT32)(Pike_sp - *--Pike_mark_sp)));
  DO_DUMB_RETURN;
});


OPCODE1(F_MARK_CALL_BUILTIN, "mark, call builtin", {
  DO_CALL_BUILTIN(0);
});

OPCODE1(F_MARK_CALL_BUILTIN_AND_POP, "mark, call builtin & pop", {
  DO_CALL_BUILTIN(0);
  pop_stack();
});

OPCODE1_RETURN(F_MARK_CALL_BUILTIN_AND_RETURN, "mark, call builtin & return", {
  DO_CALL_BUILTIN(0);
  DO_DUMB_RETURN;
});


OPCODE1(F_CALL_BUILTIN1, "call builtin 1", {
  DO_CALL_BUILTIN(1);
});

OPCODE1(F_CALL_BUILTIN1_AND_POP, "call builtin1 & pop", {
  DO_CALL_BUILTIN(1);
  pop_stack();
});

#define DO_RECUR(XFLAGS) do{						   \
  PIKE_OPCODE_T *addr;							   \
  register struct pike_frame *new_frame;				   \
  ptrdiff_t args;							   \
									   \
  fast_check_threads_etc(6);						   \
  check_stack(256);							   \
									   \
  new_frame=alloc_pike_frame();						   \
									   \
  new_frame->refs=1;							   \
  new_frame->next=Pike_fp;						   \
									   \
  Pike_fp->pc = (PIKE_OPCODE_T *)(((INT32 *)PROG_COUNTER) + 1);		   \
  addr = PROG_COUNTER+GET_JUMP();					   \
  args=addr[-1];							   \
									   \
  new_frame->num_args = new_frame->args = args;				   \
  new_frame->locals=new_frame->save_sp=new_frame->expendible=Pike_sp-args; \
  new_frame->save_mark_sp = new_frame->mark_sp_base = Pike_mark_sp;	   \
                                                                           \
  push_zeroes((new_frame->num_locals = (ptrdiff_t)addr[-2]) - args);       \
                                                                           \
  DO_IF_DEBUG({								   \
    if(t_flag > 3)							   \
      fprintf(stderr,"-    Allocating %d extra locals.\n",		   \
	      new_frame->num_locals - new_frame->num_args);		   \
  });									   \
									   \
                                                                           \
  new_frame->fun=Pike_fp->fun;						   \
  new_frame->ident=Pike_fp->ident;					   \
  new_frame->current_storage=Pike_fp->current_storage;                     \
  if(Pike_fp->scope) add_ref(new_frame->scope=Pike_fp->scope);		   \
  add_ref(new_frame->current_object=Pike_fp->current_object);		   \
  new_frame->context=Pike_fp->context;                                     \
  add_ref(new_frame->context.prog);					   \
  if(new_frame->context.parent)						   \
    add_ref(new_frame->context.parent);					   \
  Pike_fp=new_frame;							   \
  new_frame->flags=PIKE_FRAME_RETURN_INTERNAL | XFLAGS;			   \
									   \
  DO_JUMP_TO(addr);							   \
}while(0)


/* Assume that the number of arguments is correct */
OPCODE1_JUMP(F_COND_RECUR, "recur if not overloaded", {
  /* FIXME:
   * this test should actually test if this function is
   * overloaded or not. Currently it only tests if
   * this context is inherited or not.
   */
  if(Pike_fp->current_object->prog != Pike_fp->context.prog)
  {
    PIKE_OPCODE_T *addr = (PIKE_OPCODE_T *)(((INT32 *)PROG_COUNTER) + 1);
    PIKE_OPCODE_T *faddr = PROG_COUNTER+GET_JUMP();
    ptrdiff_t args=faddr[-1];
      
    if(low_mega_apply(APPLY_LOW,
		      args,
		      Pike_fp->current_object,
		      (void *)(arg1+Pike_fp->context.identifier_level)))
    {
      Pike_fp->next->pc=addr;
      Pike_fp->flags |= PIKE_FRAME_RETURN_INTERNAL;
      addr = Pike_fp->pc;
    }
    DO_JUMP_TO(addr);
  }

  /* FALL THROUGH */

  /* Assume that the number of arguments is correct */

  OPCODE0_TAILJUMP(F_RECUR, "recur", {
    DO_RECUR(0);
  });
});

/* Ugly code duplication */
OPCODE0_JUMP(F_RECUR_AND_POP, "recur & pop", {
  DO_RECUR(PIKE_FRAME_RETURN_POP);
});


/* Assume that the number of arguments is correct */
/* FIXME: adjust Pike_mark_sp */
OPCODE0_JUMP(F_TAIL_RECUR, "tail recursion", {
  int x;
  INT32 num_locals;
  PIKE_OPCODE_T *addr;
  INT32 args;

  fast_check_threads_etc(6);

  addr=PROG_COUNTER+GET_JUMP();
  args=addr[-1];
  num_locals=addr[-2];


  DO_IF_DEBUG({
    if(args != EXTRACT_UCHAR(addr-1))
      fatal("Wrong number of arguments in F_TAIL_RECUR %d != %d\n",
	    args, EXTRACT_UCHAR(addr-1));
  });

  if(Pike_sp-args != Pike_fp->locals)
  {
    DO_IF_DEBUG({
      if (Pike_sp < Pike_fp->locals + args)
	fatal("Pike_sp (%p) < Pike_fp->locals (%p) + args (%d)\n",
	      Pike_sp, Pike_fp->locals, args);
    });
    assign_svalues(Pike_fp->locals, Pike_sp-args, args, BIT_MIXED);
    pop_n_elems(Pike_sp - (Pike_fp->locals + args));
  }

  push_zeroes(num_locals - args);

  DO_IF_DEBUG({
    if(Pike_sp != Pike_fp->locals + Pike_fp->num_locals)
      fatal("Sp whacked!\n");
  });

  DO_JUMP_TO(addr);
});

OPCODE0(F_BREAKPOINT, "breakpoint", {
  extern void o_breakpoint(void);
  o_breakpoint();
  DO_JUMP_TO(PROG_COUNTER-1);
});

OPCODE0(F_THIS_OBJECT, "this_object", {
  if(Pike_fp)
  {
    ref_push_object(Pike_fp->current_object);
  }else{
    push_int(0);
  }
});

OPCODE0(F_ZERO_TYPE, "zero_type", {
  if(Pike_sp[-1].type != T_INT)
  {
    pop_stack();
    push_int(0);
  }else{
    Pike_sp[-1].u.integer=Pike_sp[-1].subtype;
    Pike_sp[-1].subtype=NUMBER_NUMBER;
  }
});

#undef PROG_COUNTER
