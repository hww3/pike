/*
|| This file is part of Pike. For copyright information see COPYRIGHT.
|| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
|| for more information.
|| $Id: top.c,v 1.10 2004/10/07 22:19:09 nilsson Exp $
*/

#include "global.h"

#include "config.h"

#include "stralloc.h"
#include "pike_macros.h"
#include "object.h"
#include "program.h"
#include "interpret.h"
#include "builtin_functions.h"
#include "pike_error.h"
#include "module.h"

#define GLUT_API_VERSION 4

#ifdef HAVE_LIBGLUT
#ifdef HAVE_GL_GLUT_H
#include <GL/glut.h>
#else
#ifdef HAVE_GLUT_GLUT_H
#include <GLUT/glut.h>
#endif
#endif
#endif


PIKE_MODULE_INIT
{
#ifdef HAVE_LIBGLUT
#if defined(HAVE_GL_GLUT_H) || defined(HAVE_GLUT_GLUT_H)
  extern void add_auto_funcs_glut(void);
  add_auto_funcs_glut();
#endif
#endif
}


PIKE_MODULE_EXIT
{
}
