/*\
||| This file is part of Pike. For copyright information see COPYRIGHT.
||| Pike is distributed under GPL, LGPL and MPL. See the file COPYING
||| for more information.
||| $Id: peep.h,v 1.10 2002/10/08 20:22:24 nilsson Exp $
\*/

#ifndef PEEP_H
#define PEEP_H

#include "dynamic_buffer.h"
extern dynamic_buffer instrbuf;

struct p_instr_s
{
  short opcode;
  short line;
  struct pike_string *file;
  INT32 arg;
  INT32 arg2;
};

typedef struct p_instr_s p_instr;

/* Prototypes begin here */
void init_bytecode(void);
void exit_bytecode(void);
ptrdiff_t insert_opcode2(unsigned int f,
			 INT32 b,
			 INT32 c,
			 INT32 current_line,
			 struct pike_string *current_file);
ptrdiff_t insert_opcode1(unsigned int f,
			 INT32 b,
			 INT32 current_line,
			 struct pike_string *current_file);
ptrdiff_t insert_opcode0(int f,int current_line, struct pike_string *current_file);
void update_arg(int instr,INT32 arg);
void assemble(void);
/* Prototypes end here */

#endif
