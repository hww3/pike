#include "global.h"
#include "stralloc.h"
#include "global.h"
RCSID("$Id: image_module.c,v 1.8 2000/08/16 19:44:20 grubba Exp $");
#include "pike_macros.h"
#include "interpret.h"
#include "program.h"
#include "object.h"
#include "operators.h"

#include "image.h"
#include "assembly.h"
#include "image_machine.h"

/* This must be included last! */
#include "module_magic.h"


#define IMAGE_INITER

/* #define DEBUG */

#define IMAGE_CLASS(name,init,exit,prog) \
    void init(void); void exit(void); struct program *prog;
#define IMAGE_SUBMODULE(name,init,exit)  \
    void init(void); void exit(void); 
#define IMAGE_SUBMODMAG(name,init,exit) \
    void init(void); void exit(void);
#define IMAGE_FUNCTION(name,func,def0,def1) \
    void func(INT32 args);
#include "initstuff.h"

static struct 
{
   char *name;
   void (*init)(void);
   void (*exit)(void);
   struct program **dest;
} initclass[]=
{
#undef IMAGE_CLASS
#undef IMAGE_SUBMODULE
#undef IMAGE_FUNCTION
#undef IMAGE_SUBMODMAG
#define IMAGE_SUBMODMAG(a,b,c) 
#define IMAGE_FUNCTION(a,b,c,d)
#define IMAGE_CLASS(name,init,exit,prog) { name,init,exit,&prog },
#define IMAGE_SUBMODULE(a,b,c)
#include "initstuff.h"
};

static struct 
{
   char *name;
   void (*init)(void);
   void (*exit)(void);
} initsubmodule[]=
{
#undef IMAGE_CLASS
#undef IMAGE_SUBMODULE
#define IMAGE_CLASS(name,init,exit,prog) 
#define IMAGE_SUBMODULE(name,init,exit) { name,init,exit },
#include "initstuff.h"
};

static struct 
{
   char *name;
   void (*init)(void);
   void (*exit)(void);
   struct pike_string *ps;
   struct object *o;
} submagic[]=
{
#undef IMAGE_SUBMODULE
#undef IMAGE_SUBMODMAG
#define IMAGE_SUBMODULE(a,b,c)
#define IMAGE_SUBMODMAG(name,init,exit) { name,init,exit,NULL,NULL },
#include "initstuff.h"
};

/* Avoid loss of precision warnings. */
#ifdef __ECL
static inline long TO_LONG(ptrdiff_t x)
{
  return DO_NOT_WARN((long)x);
}
#else /* !__ECL */
#define TO_LONG(x)	((long)(x))
#endif /* __ECL */

#ifdef PIKE_DEBUG
#define IMAGE_CHECK_STACK(X)	do { \
    if (save_sp != sp) { \
      fatal("%s:%d: %ld droppings on stack! previous init: %s\n", \
            __FILE__, __LINE__, TO_LONG(sp - save_sp), X); \
    } \
  } while(0)
#else
#define IMAGE_CHECK_STACK(X)
#endif /* PIKE_DEBUG */


static void image_magic_index(INT32 args)
{
   struct svalue tmp;
   int i;

   if (args!=1) 
      error("Image.`[]: Too few or too many arguments\n");
   if (sp[-1].type!=T_STRING)
      error("Image.`[]: Illegal type of argument\n");

   for (i=0; i<(int)NELEM(submagic); i++)
      if (sp[-1].u.string==submagic[i].ps)
      {
#ifdef PIKE_DEBUG
   struct svalue *save_sp;
#endif
	 pop_stack();

#ifdef PIKE_DEBUG
	 save_sp = sp;
#endif

	 if (!submagic[i].o)
	 {
	    struct program *p;
	    start_new_program();
	    (submagic[i].init)();
	    IMAGE_CHECK_STACK(submagic[i].name);
	    p=end_program();
	    submagic[i].o=clone_object(p,0);
	    free_program(p);
	 }
	 
	 ref_push_object(submagic[i].o);
	 return;
      }

   stack_dup();
   ref_push_object(fp->current_object);
   stack_swap();
   f_arrow(2);

   if (sp[-1].type==T_INT)
   {
      pop_stack();
      stack_dup();
      push_text("_Image_");
      stack_swap();
      f_add(2);
      push_int(0);
      SAFE_APPLY_MASTER("resolv",2);
   }
   if (sp[-1].type==T_INT)
   {
      pop_stack();
      stack_dup();
      push_text("_Image");
      push_int(0);
      SAFE_APPLY_MASTER("resolv",2);
      stack_swap();
      f_index(2);
   }
   stack_swap();
   pop_stack();
}

int image_cpuid;
#ifdef ASSEMBLY_OK
static void init_cpuidflags( )
{
  unsigned int a, b, c, d;
  char *data = alloca(20);
  MEMSET( data, 0, 20 );

  image_get_cpuid( 0, &a, &b, &c, &d );

  ((int *)data)[0] = a;
  ((int *)data)[1] = b;
  ((int *)data)[2] = c;

  if( strncmp( data, "GenuineIntel", 12 ) )
  {
    if( strncmp( data, "AuthenticAMD", 12 ) )
    {
      if( !strncmp( data, "CyrixInstead", 12 ) )
      {
        if( d != 2 )
          goto normal_test;
        image_get_cpuid( 0x80000000, &a, &b, &c, &d );
        if( d < 0x80000000 )
          goto normal_test;
        image_get_cpuid( 0x80000001, &a, &b, &c, &d );
        
        if( b & 0x00800000 ) image_cpuid |= IMAGE_MMX;
        if( b & 0x02000000 ) image_cpuid |= IMAGE_SSE;
        if( b & 0x01000000 ) image_cpuid |= IMAGE_EMMX;
        if( b & 0x80000000 ) image_cpuid |= (IMAGE_3DNOW | IMAGE_MMX);
      }   
    } else { 
      /* It's an AMD cpu. */
      image_get_cpuid( 0x80000000, &a, &b, &c, &d );
      if( d < 0x80000000 )
        goto normal_test;
      image_get_cpuid( 0x80000001, &a, &b, &c, &d );
      
      if( b & 0x00800000 ) image_cpuid |= IMAGE_MMX;
      if( b & 0x02000000 ) image_cpuid |= IMAGE_SSE;
      if( b & 0x80000000 ) image_cpuid |= (IMAGE_3DNOW | IMAGE_MMX);
    }
  } else {
  normal_test:
    /* It's an intel CPU. */
    image_get_cpuid( 1, &a, &b, &c, &d );
    if( b & 0x00800000 )
      image_cpuid |= IMAGE_MMX;
    if( b & 0x02000000 )
      image_cpuid |= IMAGE_SSE;
  }
#if 0
  fprintf(stderr, "Image CPUID == %d\n", image_cpuid );
#endif
}
#endif

void pike_module_init(void)
{
   char type_of_index[]=
      tFunc(tStr,tOr3(tObj,tPrg,""))

#undef IMAGE_FUNCTION
#undef IMAGE_SUBMODMAG
#define IMAGE_SUBMODMAG(name,init,exit) 
#define IMAGE_FUNCTION(name,func,def0,def1) tOr(def0,"")
#include "initstuff.h"
      tFunc(tStr,tMixed); /* this */

#ifdef PIKE_DEBUG
   struct svalue *save_sp = sp;
#endif

   int i;


#ifdef ASSEMBLY_OK
     init_cpuidflags( );
#endif

   for (i=0; i<(int)NELEM(initclass); i++)
   {
      start_new_program();

#ifdef DEBUG
      fprintf(stderr,"Image: initiating class \"Image.%s\"...\n",
	      initclass[i].name);
#endif

      (initclass[i].init)();
      IMAGE_CHECK_STACK(initclass[i].name);
      initclass[i].dest[0]=end_program();
      add_program_constant(initclass[i].name,initclass[i].dest[0],0);
   }

   for (i=0; i<(int)NELEM(initsubmodule); i++)
   {
      struct program *p;
      struct pike_string *s;

#ifdef DEBUG
      fprintf(stderr,"Image: initiating submodule \"Image.%s\"...\n",
	      initsubmodule[i].name);
#endif
      
      start_new_program();
      (initsubmodule[i].init)();
      IMAGE_CHECK_STACK(initsubmodule[i].name);
      p=end_program();
      push_object(clone_object(p,0));
      s=make_shared_string(initsubmodule[i].name);
      add_constant(s,sp-1,0);
      free_string(s);
      free_program(p);
      pop_stack();
   }

   for (i=0; i<(int)NELEM(submagic); i++)
      submagic[i].ps=make_shared_string(submagic[i].name);

#undef IMAGE_FUNCTION
#define IMAGE_FUNCTION(name,func,def0,def1) ADD_FUNCTION(name,func,def0,def1);
#include "initstuff.h"

   quick_add_function("`[]",3,image_magic_index,
		      type_of_index,CONSTANT_STRLEN(type_of_index),0,0);

   /* compat stuff */
   add_program_constant("font",image_font_program,0); 
   add_program_constant("image",image_program,0); 
   add_program_constant("colortable",image_colortable_program,0);
}

void pike_module_exit(void) 
{
   int i;
   for (i=0; i<(int)NELEM(initclass); i++)
   {
      (initclass[i].exit)();
      free_program(initclass[i].dest[0]);
   }
   for (i=0; i<(int)NELEM(initsubmodule); i++)
      (initsubmodule[i].exit)();
   for (i=0; i<(int)NELEM(submagic); i++) {
      if (submagic[i].o)
      {
	 (submagic[i].exit)();
	 free_object(submagic[i].o);
      }
      if (submagic[i].ps)
      {
	 free_string(submagic[i].ps);
      }
   }
}
