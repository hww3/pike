/*
 * $Id: top.c,v 1.4 2000/12/01 08:10:40 hubbe Exp $
 *
 */

#include "global.h"

#include "config.h"

RCSID("$Id: top.c,v 1.4 2000/12/01 08:10:40 hubbe Exp $");
#include "stralloc.h"
#include "pike_macros.h"
#include "object.h"
#include "program.h"
#include "interpret.h"
#include "builtin_functions.h"
#include "pike_error.h"

#include "module_magic.h"

#ifdef HAVE_LIBGLUT
#ifdef HAVE_GL_GLUT_H
#define GLUT_API_VERSION 4
#include <GL/glut.h>
#endif
#endif


void pike_module_init( void )
{
#ifdef HAVE_LIBGLUT
#ifdef HAVE_GL_GLUT_H
  extern void add_auto_funcs_glut(void);
  add_auto_funcs_glut();
#endif
#endif
}


void pike_module_exit( void )
{
}

