/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id$
*/

/*
 * Compilator state push / pop operator construction file
 *
 * (Can you tell I like macros?)
 */

/*
 * IMEMBER: do not reset this member when pushing
 * ZMEMBER: reset this member to zero when pushing
 * STACKMEMBER: Like IMEMBER, but is not allowed to become more when popping
 *
 * defining STRUCT defines the structures
 * defining DECLARE creates global vars for saving linked list
 *                  of these lists and the start sentinel.
 * defining PUSH pushes the selected state(s) on the stack(s)
 * defining POP pops the selected state(s) from the stack(s)
 *
 * define PROGRAM_STATE to select the program state
 */

#ifdef PIKE_DEBUG
#define DO_DEBUG_CODE(X) X
#else
#define DO_DEBUG_CODE(X)
#endif

#ifdef STRUCT
#define IMEMBER(TYPE, FIELD, VALUE)	TYPE FIELD;
#define STACKMEMBER(TYPE, FIELD, VALUE)	TYPE FIELD;
#define ZMEMBER(TYPE, FIELD, VALUE)	TYPE FIELD;
#define SNAME(STRUCT_TAG, VAR_NAME)			\
  struct STRUCT_TAG { struct STRUCT_TAG *previous;
#define SEND };
#endif

#ifdef EXTERN
#define IMEMBER(X,Y,Z)
#define STACKMEMBER(X,Y,z)
#define ZMEMBER(X,Y,Z)
#define SNAME(X,Y) PMOD_EXPORT extern struct X * Y;
#define SEND
#endif

#ifdef DECLARE
#define IMEMBER(X,Y,Z) Z,
#define STACKMEMBER(X,Y,Z) Z,
#define ZMEMBER(X,Y,Z) Z,
#define SNAME(X,Y) \
  extern struct X PIKE_CONCAT(Y,_base); \
  struct X * Y = & PIKE_CONCAT(Y,_base); \
  struct X PIKE_CONCAT(Y,_base) = { 0, 
#define SEND };
#endif

#ifdef PUSH
#define IMEMBER(X,Y,Z) (nEw->Y=Pike_compiler->Y);
#define STACKMEMBER(X,Y,Z) (nEw->Y=Pike_compiler->Y);
#define ZMEMBER(X,Y,Z) /* Zapped by the MEMSET in SNAME() below. */;
#define SNAME(X,Y) { \
      struct X *nEw; \
      nEw=ALLOC_STRUCT(X); \
      MEMSET((char *)nEw, 0, sizeof(struct X)); \
      nEw->previous=Pike_compiler;
#define SEND \
      Pike_compiler=nEw; \
      }

#endif


#ifdef POP
#define IMEMBER(X,Y,Z) 
#define ZMEMBER(X,Y,Z) 

#define STACKMEMBER(X,Y,Z) DO_DEBUG_CODE( \
    if(Pike_compiler->Y < oLd->Y) \
      Pike_fatal("Stack " #Y " shrunk %ld steps compilation, currently: %p.\n", \
            PTRDIFF_T_TO_LONG(oLd->Y - Pike_compiler->Y), Pike_compiler->Y); )

#define SNAME(X,Y) { \
      struct X *oLd=Pike_compiler->previous;

#define SEND \
     free((char *)Pike_compiler); \
     Pike_compiler=oLd; \
    }

#define PCODE(X) X
#else
#define PCODE(X)
#endif


#ifdef INIT
#define IMEMBER(X,Y,Z) (c->Y=Pike_compiler->Y);
#define STACKMEMBER(X,Y,Z) (c->Y=Pike_compiler->Y);
#define ZMEMBER(X,Y,Z) /* Zapped by the MEMSET in SNAME() below. */;
#define SNAME(X,Y) { \
      MEMSET(c, 0, sizeof(struct X));		\
      c->previous = Pike_compiler;
#define SEND \
      Pike_compiler = c; \
      }

#endif


#ifdef EXIT
#define IMEMBER(X,Y,Z) 
#define ZMEMBER(X,Y,Z) 

#define STACKMEMBER(X,Y,Z) DO_DEBUG_CODE( \
    if(c->Y < oLd->Y) \
      Pike_fatal("Stack " #Y " shrunk %ld steps compilation, currently: %p.\n", \
            PTRDIFF_T_TO_LONG(oLd->Y - c->Y), c->Y); )

#define SNAME(X,Y) { \
    struct X *oLd = c->previous;

#define SEND							\
    if (Pike_compiler == c) {					\
      Pike_compiler=oLd;					\
    } else {							\
      struct program_state *tmp = Pike_compiler;		\
      while (tmp && (tmp->previous != c))			\
        tmp = tmp->previous;					\
      if (tmp) tmp->previous = oLd;				\
      else Pike_fatal("Lost track of compiler_state %p\n", c);	\
    }								\
  }
#undef PCODE
#define PCODE(X) X
#endif


#ifdef PIKE_DEBUG
#define STRMEMBER(X,Y) \
  PCODE(if(Pike_compiler->X) Pike_fatal("Variable " #X " not deallocated properly.\n");) \
  ZMEMBER(struct pike_string *,X,Y)
#else
#define STRMEMBER(X,Y) \
  ZMEMBER(struct pike_string *,X,Y)
#endif

  SNAME(program_state,Pike_compiler)
  ZMEMBER(INT32,last_line,0)
  STRMEMBER(last_file,0)
  ZMEMBER(struct object *,fake_object,0)
  ZMEMBER(struct program *,new_program,0)
  ZMEMBER(struct program *,malloc_size_program,0)
  ZMEMBER(node *,init_node,0)
  ZMEMBER(INT32,last_pc,0)
  ZMEMBER(int,num_parse_error,0)
  ZMEMBER(struct compiler_frame *,compiler_frame,0)
  ZMEMBER(INT32,num_used_modules,0)
  IMEMBER(int,compiler_pass,0)
  ZMEMBER(int,local_class_counter,0)
  ZMEMBER(int,catch_level,0)
  ZMEMBER(INT32,current_modifiers,0)
  ZMEMBER(int,varargs,0)
  ZMEMBER(int, num_create_args, 0)
  ZMEMBER(int, num_inherits, 0)	/* Used during second pass. */
  STRMEMBER(last_identifier,0)
  ZMEMBER(struct mapping *,module_index_cache,0)
  STACKMEMBER(struct pike_type **,type_stackp,type_stack)
  STACKMEMBER(struct pike_type ***,pike_type_mark_stackp,pike_type_mark_stack)
  ZMEMBER(INT32,parent_identifier,0)
  IMEMBER(int, compat_major, PIKE_MAJOR_VERSION)
  IMEMBER(int, compat_minor, PIKE_MINOR_VERSION)
  ZMEMBER(int, flags, 0)
  ZMEMBER(struct compilation *,compiler,0)
  SEND

#undef PCODE
#undef STRMEMBER
#undef IMEMBER
#undef ZMEMBER
#undef SNAME
#undef SEND
#undef STACKMEMBER

#undef EXTERN
#undef STRUCT
#undef EXIT
#undef INIT
#undef PUSH
#undef POP
#undef DECLARE
#undef DO_DEBUG_CODE
