#ifndef TESTING
#  include "global.h"
#  include "interpret.h"
#  include "constants.h"
#  include "pike_error.h"
#  include "module.h"
#  include "stralloc.h"
#  include "pike_macros.h"
#  include "main.h"
#  include "constants.h"

RCSID("$Id: dynamic_load.c,v 1.54 2001/09/10 15:51:23 grubba Exp $");

#else /* TESTING */

#include <stdio.h>

#endif /* !TESTING */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if !defined(HAVE_DLOPEN)

#if defined(HAVE_DLD_LINK) && defined(HAVE_DLD_GET_FUNC)
#define USE_DLD
#define HAVE_SOME_DLOPEN
#define EMULATE_DLOPEN
#else
#if defined(HAVE_SHL_LOAD) && defined(HAVE_DL_H)
#define USE_HPUX_DL
#define HAVE_SOME_DLOPEN
#define EMULATE_DLOPEN
#else

#if 0
#if defined(HAVE_LOADLIBRARY) && defined(HAVE_FREELIBRARY) && \
    defined(HAVE_GETPROCADDRESS) && defined(HAVE_WINBASE_H)
#define USE_LOADLIBRARY
#define HAVE_SOME_DLOPEN
#define EMULATE_DLOPEN
#endif
#endif /* 0 */

#ifdef HAVE_MACH_O_DYLD_H
/* MacOS X... */
#define USE_DYLD
#define HAVE_SOME_DLOPEN
#define EMULATE_DLOPEN
#else /* !HAVE_MACH_O_DYLD_H */
#ifdef USE_MY_WIN32_DLOPEN
#include "pike_dlfcn.h"
#define HAVE_SOME_DLOPEN
#define HAVE_DLOPEN
#endif
#endif /* HAVE_MACH_O_DYLD_H */

#endif
#endif
#else
#define HAVE_SOME_DLOPEN
#endif


#ifdef HAVE_SOME_DLOPEN

typedef void (*modfun)(void);

#ifdef USE_LOADLIBRARY
#include <windows.h>

static TCHAR *convert_string(const char *str, ptrdiff_t len)
{
  ptrdiff_t e;
  TCHAR *ret=(TCHAR *)xalloc((len+1) * sizeof(TCHAR));
  for(e=0;e<len;e++) ret[e]=EXTRACT_UCHAR(str+e);
  ret[e]=0;
  return ret;
}

static void *dlopen(const char *foo, int how)
{
  TCHAR *tmp;
  HINSTANCE ret;
  tmp=convert_string(foo, strlen(foo));
  ret=LoadLibrary(tmp);
  free((char *)tmp);
  if(ret)
  {
    void ** psym=(void **)GetProcAddress(ret, "PikeSymbol");
    if(psym)
    {
      extern void *PikeSymbol[];
      *psym = PikeSymbol;
    }
  }
  return (void *)ret;
}

static char * dlerror(void)
{
  static char buffer[200];
  sprintf(buffer,"LoadLibrary failed with error: %d",GetLastError());
  return buffer;
}

static void *dlsym(void *module, char * function)
{
  return (void *)GetProcAddress((HMODULE)module,
				function);
}

static void dlclose(void *module)
{
  FreeLibrary((HMODULE)module);
}

#define dlinit()	1

#endif /* USE_LOADLIBRARY */


#ifdef USE_DLD
#include <dld.h>
static void *dlopen(const char *module_name, int how)
{
  dld_create_reference("pike_module_init");
  if(dld_link(module_name))
  {
    return (void *)strdup(module_name);
  }else{
    return 0;
  }
}

static char *dlerror(void)
{
  return dld_strerror(dld_errno);
}

static void *dlsym(void *module, char *function)
{
  return dld_get_func(function);
}

static void *dlclose(void *module)
{
  if(!module) return;
  dld_unlink_by_file((char *)module);
  free(module);
}

static int dlinit(void)
{
  extern char ** ARGV;
  if(dld_init(dld_find_executable(ARGV[0])))
  {
    fprintf(stderr,"Failed to init dld\n");
    return 0;
  }
  /* OK */
  return 1;
}

#endif /* USE_DLD */


#ifdef USE_HPUX_DL

#include <dl.h>

#if defined(BIND_VERBOSE)
#define RTLD_NOW	BIND_IMMEDIATE | BIND_VERBOSE
#else
#define RTLD_NOW	BIND_IMMEDIATE
#endif /* BIND_VERBOSE */

extern int errno;

static void *dlopen(const char *libname, int how)
{
  shl_t lib;

  lib = shl_load(libname, how, 0L);

  return (void *)lib;
}

static char *dlerror(void)
{
#ifdef HAVE_STRERROR
  return strerror(errno);
#else
  return ""; /* I hope it's better than null..*/
#endif
}

static void *dlsym(void *module, char *function)
{
  void *func;
  int result;
  shl_t mod = (shl_t)module;

  result = shl_findsym(&mod, function, TYPE_UNDEFINED, &func);
  if (result == -1)
    return NULL;
  return func;
}

static void dlclose(void *module)
{
  shl_unload((shl_t)module);
}

#define dlinit()	1

#endif /* USE_HPUX_DL */

#ifdef USE_DYLD

#include <mach-o/dyld.h>

#define RTLD_NOW	NSLINKMODULE_OPTION_BINDNOW

#define dlinit()	_dyld_present()

static void *dlopen(const char *module_name, int how)
{
  NSObjectFileImageReturnCode code = 0;
  NSObjectFileImage image = NULL;

  if ((code = NSCreateObjectFileImageFromFile(module_name, &image)) !=
      NSObjectFileImageSuccess) {
    fprintf(stderr, "NSCreateObjectFileImageFromFile(\"%s\") failed with %d\n",
	    module_name, code);
    return NULL;
  }
  /* FIXME: image should be freed somewhere! */
  return NSLinkModule(image, module_name,
		      how | NSLINKMODULE_OPTION_RETURN_ON_ERROR);
}

static char *dlerror(void)
{
  NSLinkEditErrors class = 0;
  int error_number = 0;
  char *file_name = NULL;
  char *error_string = NULL;
  NSLinkEditError(&class, &error_number, &file_name, &error_string);
  return error_string;
}

static void *dlsym(void *module, char *function)
{
  return NSLookupSymbolInModule(module, function);
}

static void *dlclose(void *module)
{
  NSUnLinkModule(module, NSUNLINKMODULE_OPTION_NONE);
  return NULL;
}

#endif /* USE_DYLD */


#ifndef EMULATE_DLOPEN

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#define dlinit()	1
#endif  /* !EMULATE_DLOPEN */


#endif /* HAVE_SOME_DLOPEN */

#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif

#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0 
#endif

#ifndef TESTING

#if defined(HAVE_DLOPEN) || defined(USE_DLD) || defined(USE_HPUX_DL) || defined(USE_LOADLIBRARY)
#define USE_DYNAMIC_MODULES
#endif

#ifdef USE_DYNAMIC_MODULES

struct module_list
{
  struct module_list * next;
  void *module;
  modfun init, exit;
};

struct module_list *dynamic_module_list = 0;

#ifdef NO_CAST_TO_FUN
/* Function pointers can't be casted to scalar pointers according to
 * ISO-C (probably to support true Harward achitecture machines).
 */
static modfun CAST_TO_FUN(void *ptr)
{
  union {
    void *ptr;
    modfun fun;
  } u;
  u.ptr = ptr;
  return u.fun;
}
#else /* !NO_CAST_TO_FUN */
#define CAST_TO_FUN(X)	((modfun)X)
#endif /* NO_CAST_TO_FUN */

/*! @decl int load_module(string module_name)
 *!
 *! Load a binary module.
 *!
 *! This function loads a module written in C or some other language
 *! into Pike. The module is initialized and any programs or constants
 *! defined will immediately be available.
 *!
 *! When a module is loaded the function @tt{pike_module_init()@} will
 *! be called to initialize it. When Pike exits @tt{pike_module_exit()@}
 *! will be called. These two functions @b{must@} be available in the module.
 *!
 *! @note
 *!   The current working directory is normally not searched for
 *!   dynamic modules. Please use @tt{"./name.so"@} instead of just
 *!   @tt{"name.so"@} to load modules from the current directory.
 */
void f_load_module(INT32 args)
{
  void *module;
  modfun init, exit;
  struct module_list *new_module;
  const char *module_name;

  if(sp[-args].type != T_STRING)
    Pike_error("Bad argument 1 to load_module()\n");

  module_name = sp[-args].u.string->str;

  /* Removing RTLD_GLOBAL breaks some PiGTK themes - Hubbe */
  /* Using RTLD_LAZY is faster, but makes it impossible to 
   * detect linking problems at runtime..
   */
  module=dlopen(module_name, 
                RTLD_NOW |RTLD_GLOBAL  );

  if(!module)
  {
    const char *err = dlerror();
    if(!err) err = "Unknown reason";
    if (sp[-args].u.string->len < 1024) {
      Pike_error("load_module(\"%s\") failed: %s\n",
	    sp[-args].u.string->str, err);
    } else {
      Pike_error("load_module() failed: %s\n", err);
    }
  }

  init = CAST_TO_FUN(dlsym(module, "pike_module_init"));
  if (!init) {
    init = CAST_TO_FUN(dlsym(module, "_pike_module_init"));
  }
  exit = CAST_TO_FUN(dlsym(module, "pike_module_exit"));
  if (!exit) {
    exit = CAST_TO_FUN(dlsym(module, "_pike_module_exit"));
  }

  if(!init || !exit)
  {
    dlclose(module);

    if (strlen(module_name) < 1024) {
      Pike_error("Failed to initialize dynamic module \"%s\".\n", module_name);
    } else {
      Pike_error("Failed to initialize dynamic module.\n");
    }
  }

  new_module=ALLOC_STRUCT(module_list);
  new_module->next=dynamic_module_list;
  dynamic_module_list=new_module;
  new_module->module=module;
  new_module->init=init;
  new_module->exit=exit;

  start_new_program();

  {
    extern int global_callable_flags;
    global_callable_flags|=CALLABLE_DYNAMIC;
  }

#ifdef PIKE_DEBUG
  { struct svalue *save_sp=sp;
#endif
  (*(modfun)init)();
#ifdef PIKE_DEBUG
  if(sp != save_sp)
    fatal("load_module(%s) left %ld droppings on stack!\n",
	  module_name,
	  PTRDIFF_T_TO_LONG(sp - save_sp));
  }
#endif

  pop_n_elems(args);
  push_program(end_program());
}

#endif /* USE_DYNAMIC_MODULES */


void init_dynamic_load(void)
{
#ifdef USE_DYNAMIC_MODULES
  if (dlinit()) {
  
    /* function(string:program) */

    ADD_EFUN("load_module", f_load_module,
	     tFunc(tStr,tPrg(tObj)), OPT_EXTERNAL_DEPEND);
  }
#endif
}

/* Call the pike_module_exit() callbacks for the dynamic modules. */
void exit_dynamic_load(void)
{
#ifdef USE_DYNAMIC_MODULES
  struct module_list *tmp;
  for (tmp = dynamic_module_list; tmp; tmp = tmp->next)
    (*tmp->exit)();
#endif
}

/* Unload all the dynamically loaded modules. */
void free_dynamic_load(void)
{
#ifdef USE_DYNAMIC_MODULES
  while(dynamic_module_list)
  {
    struct module_list *tmp=dynamic_module_list;
    dynamic_module_list=tmp->next;
#ifndef DEBUG_MALLOC
    dlclose(tmp->module);
#endif
    free((char *)tmp);
  }
#endif
}


#else /* TESTING */
#include <stdio.h>

int main()
{
  void *module,*fun;
  dlinit();
  module=dlopen("./myconftest.so",RTLD_NOW);
  if(!module)
  {
    fprintf(stderr,"Failed to link myconftest.so: %s\n",dlerror());
    exit(1);
  }
  fun=dlsym(module,"testfunc");
  if(!fun) fun=dlsym(module,"_testfunc");
  if(!fun)
  {
    fprintf(stderr,"Failed to find function testfunc: %s\n",dlerror());
    exit(1);
  }
  fprintf(stderr,"Calling testfunc\n");
  ((void (*)(void))fun)();
  fprintf(stderr,"testfunc returned!\n");
  exit(1);
}
#endif
