/*\
||| This file is part of Pike. For copyright information see COPYRIGHT.
||| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
||| for more information.
||| $Id: math_module.c,v 1.10 2002/10/08 20:22:33 nilsson Exp $
\*/

#include "global.h"
#include "config.h"
#include "program.h"

#include "math_module.h"
#include "transforms.h"

/* must be included last */
#include "module_magic.h"

/*** module init & exit & stuff *****************************************/

/* add other parsers here */

struct program *math_matrix_program;
struct program *math_imatrix_program;
struct program *math_fmatrix_program;
struct program *math_smatrix_program;
#ifdef INT64
struct program *math_lmatrix_program;
#endif /* INT64 */
struct program *math_transforms_program;

static struct math_class
{
   char *name;
   void (*func)(void);
   struct program **pd;
} sub[] = {
   {"Matrix",init_math_matrix,&math_matrix_program},
   {"IMatrix",init_math_imatrix,&math_imatrix_program},
#ifdef INT64
   {"LMatrix",init_math_lmatrix,&math_lmatrix_program},
#endif /* INT64 */
   {"FMatrix",init_math_fmatrix,&math_fmatrix_program},
   {"SMatrix",init_math_smatrix,&math_smatrix_program},
   {"Transforms",init_math_transforms,&math_transforms_program},
};

void pike_module_exit(void)
{
   int i;
   for (i=0; i<(int)(sizeof(sub)/sizeof(sub[0])); i++)
      if (sub[i].pd && sub[i].pd[0])
	 free_program(sub[i].pd[0]);

   exit_math_matrix();
   exit_math_imatrix();
   exit_math_fmatrix();
   exit_math_smatrix();
   exit_math_transforms();
}

void pike_module_init(void)
{
   int i;
   DECLARE_INF
   DECLARE_NAN
   
   for (i=0; i<(int)(sizeof(sub)/sizeof(sub[0])); i++)
   {
      struct program *p;

      start_new_program();
      sub[i].func();
      p=end_program();
      add_program_constant(sub[i].name,p,0);
      if (sub[i].pd) sub[i].pd[0]=p;
      else free_program(p);
   }

   add_float_constant("pi",3.14159265358979323846  ,0);
   add_float_constant("e", 2.7182818284590452354   ,0);
   add_float_constant("inf", MAKE_INF(1), 0);
   add_float_constant("nan", MAKE_NAN(), 0);
}

