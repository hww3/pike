/*
 * $Id: math_module.c,v 1.6 2001/07/15 20:20:44 per Exp $
 */

#include "global.h"
#include "config.h"
#include "program.h"

#include "math_module.h"

/* must be included last */
#include "module_magic.h"

/*** module init & exit & stuff *****************************************/

/* add other parsers here */

struct program *math_matrix_program;
struct program *math_imatrix_program;
struct program *math_fmatrix_program;
struct program *math_smatrix_program;
struct program *math_lmatrix_program;

static struct math_class
{
   char *name;
   void (*func)(void);
   struct program **pd;
} sub[] = {
   {"Matrix",init_math_matrix,&math_matrix_program},
   {"IMatrix",init_math_imatrix,&math_imatrix_program},
   {"LMatrix",init_math_lmatrix,&math_lmatrix_program},
   {"FMatrix",init_math_fmatrix,&math_fmatrix_program},
   {"SMatrix",init_math_smatrix,&math_smatrix_program},
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
}

void pike_module_init(void)
{
   int i;
   
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
}

