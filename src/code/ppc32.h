/*
 * $Id: ppc32.h,v 1.7 2001/08/16 00:28:30 marcus Exp $
 */

#define PIKE_OPCODE_T	unsigned INT32

#define PPC_INSTR_B_FORM(OPCD,BO,BI,BD,AA,LK)			\
      add_to_program(((OPCD)<<26)|((BO)<<21)|((BI)<<16)|	\
		     (((BD)&0x3fff)<<2)|((AA)<<1)|(LK))
#define PPC_INSTR_D_FORM(OPCD,S,A,d)					\
      add_to_program(((OPCD)<<26)|((S)<<21)|((A)<<16)|((d)&0xffff))
#define PPC_INSTR_M_FORM(OPCD,S,A,SH,MB,ME,Rc)				\
      add_to_program(((OPCD)<<26)|((S)<<21)|((A)<<16)|((SH)<<11)|	\
		     ((MB)<<6)|((ME)<<1)|(Rc))

#define BC(BO,BI,BD) PPC_INSTR_B_FORM(16,BO,BI,BD,0,0)

#define CMPLI(crfD,A,UIMM) PPC_INSTR_D_FORM(10,crfD,A,UIMM)
#define ADDIC(D,A,SIMM) PPC_INSTR_D_FORM(12,D,A,SIMM)
#define ADDI(D,A,SIMM) PPC_INSTR_D_FORM(14,D,A,SIMM)
#define ADDIS(D,A,SIMM) PPC_INSTR_D_FORM(15,D,A,SIMM)
#define ORI(D,A,UIMM) PPC_INSTR_D_FORM(24,D,A,UIMM)
#define LWZ(D,A,d) PPC_INSTR_D_FORM(32,D,A,d)
#define STW(S,A,d) PPC_INSTR_D_FORM(36,S,A,d)

#define RLWINM(S,A,SH,MB,ME) PPC_INSTR_M_FORM(21,S,A,SH,MB,ME,0)


#define LOW_GET_JUMP()	(PROG_COUNTER[0])
#define LOW_SKIPJUMP()	(SET_PROG_COUNTER(PROG_COUNTER + 1))
#define PROG_COUNTER (((INT32 **)__builtin_frame_address(1))[2])

#define SET_REG(REG, X) do {						  \
    INT32 val_ = X;							  \
    INT32 reg_ = REG;							  \
    if ((-32768 <= val_) && (val_ <= 32767)) {				  \
      /* addi reg,0,val */						  \
      ADDI(reg_, 0, val_);						  \
    } else {								  \
      /* addis reg,0,%hi(val) */					  \
      ADDIS(reg_, 0, val_ >> 16);					  \
      if (val_ & 0xffff) {						  \
	/* ori reg,reg,%lo(val) */					  \
	ORI(reg_, reg_, val_);						  \
      }									  \
    }									  \
  } while(0)

#define PPC_REG_ARG1 3
#define PPC_REG_ARG2 4


#define PPC_REG_PIKE_MARK_SP 8
#define PPC_REG_PIKE_FP 9
#define PPC_REG_PIKE_SP 10

#define PPC_REG_PIKE_INTERP 31

extern int ppc32_codegen_state;
void ppc32_flush_code_generator_state(void);
#define FLUSH_CODE_GENERATOR_STATE ppc32_flush_code_generator_state

#define PPC_CODEGEN_FP_ISSET 1
#define PPC_CODEGEN_SP_ISSET 2
#define PPC_CODEGEN_SP_NEEDSSTORE 4
#define PPC_CODEGEN_MARK_SP_ISSET 8
#define PPC_CODEGEN_MARK_SP_NEEDSSTORE 16

#define LOAD_FP_REG() do {				\
    if(!(ppc32_codegen_state & PPC_CODEGEN_FP_ISSET)) {	\
      /* lwz pike_fp,frame_pointer(pike_interpreter) */	\
      LWZ(PPC_REG_PIKE_FP, PPC_REG_PIKE_INTERP, 	\
	  OFFSETOF(Pike_interpreter, frame_pointer));	\
      ppc32_codegen_state |= PPC_CODEGEN_FP_ISSET;	\
    }							\
  } while(0)

#define LOAD_SP_REG() do {				\
    if(!(ppc32_codegen_state & PPC_CODEGEN_SP_ISSET)) {	\
      /* lwz pike_sp,stack_pointer(pike_interpreter) */	\
      LWZ(PPC_REG_PIKE_SP, PPC_REG_PIKE_INTERP,		\
	  OFFSETOF(Pike_interpreter, stack_pointer));	\
      ppc32_codegen_state |= PPC_CODEGEN_SP_ISSET;	\
    }							\
  } while(0)

#define LOAD_MARK_SP_REG() do {						\
    if(!(ppc32_codegen_state & PPC_CODEGEN_MARK_SP_ISSET)) {		\
      /* lwz pike_mark_sp,mark_stack_pointer(pike_interpreter) */	\
      LWZ(PPC_REG_PIKE_MARK_SP, PPC_REG_PIKE_INTERP,			\
	  OFFSETOF(Pike_interpreter, mark_stack_pointer));		\
      ppc32_codegen_state |= PPC_CODEGEN_MARK_SP_ISSET;			\
    }									\
  } while(0)

#define INCR_SP_REG(n) do {				\
      /* addi pike_sp,pike_sp,n */			\
      ADDI(PPC_REG_PIKE_SP, PPC_REG_PIKE_SP, n);	\
      ppc32_codegen_state |= PPC_CODEGEN_SP_NEEDSSTORE;	\
    } while(0)

#define INCR_MARK_SP_REG(n) do {				\
      /* addi pike_mark_sp,pike_mark_sp,n */			\
      ADDI(PPC_REG_PIKE_MARK_SP, PPC_REG_PIKE_MARK_SP, n);	\
      ppc32_codegen_state |= PPC_CODEGEN_MARK_SP_NEEDSSTORE;	\
    } while(0)

#define UPDATE_PC() do {				\
    INT32 tmp = PIKE_PC;				\
    LOAD_FP_REG();					\
    SET_REG(0, tmp);					\
    /* stw r0,pc(pike_fp) */				\
    STW(0, PPC_REG_PIKE_FP, OFFSETOF(pike_frame, pc));	\
  } while(0)

#define ins_pointer(PTR)  add_to_program((INT32)(PTR))
#define read_pointer(OFF) (Pike_compiler->new_program->program[(INT32)(OFF)])
#define upd_pointer(OFF,PTR) (Pike_compiler->new_program->program[(INT32)(OFF)] = (INT32)(PTR))
#define ins_align(ALIGN)
#define ins_byte(VAL)	  add_to_program((INT32)(VAL))
#define ins_data(VAL)	  add_to_program((INT32)(VAL))

#define READ_INCR_BYTE(PC)	(((PC)++)[0])

#if 0
#define RELOCATE_program(P, NEW)	do {			\
    PIKE_OPCODE_T *op_ = NEW;					\
    struct program *p_ = P;					\
    size_t rel_ = p_->num_relocations;				\
    INT32 delta_ = p_->program - op_;				\
    while (rel_--) {						\
      DO_IF_DEBUG(						\
        if ((op_[p_->relocations[rel_]] & 0xfc000002) !=	\
	    0x48000000) {					\
          fatal("Bad relocation: %d, off:%d, opcode: 0x%08x\n",	\
		rel_, p_->relocations[rel_],			\
		op_[p_->relocations[rel_]]);			\
	}							\
      );							\
      op_[p_->relocations[rel_]] = 0x48000000 |			\
	((op_[p_->relocations[rel_]] + (delta_<<2)) &		\
	 0x03ffffff);						\
    }								\
  } while(0)
#endif

extern void ppc32_flush_instruction_cache(void *addr, size_t len);
#define FLUSH_INSTRUCTION_CACHE ppc32_flush_instruction_cache

/*
struct dynamic_buffer_s;

void ppc32_encode_program(struct program *p, struct dynamic_buffer_s *buf);
void ppc32_decode_program(struct program *p);

#define ENCODE_PROGRAM(P, BUF)	ppc32_encode_program(P, BUF)
#define DECODE_PROGRAM(P)	ppc32_decode_program(p)
*/

#ifdef PIKE_CPU_REG_PREFIX
#define PPC_REGNAME(n) PIKE_CPU_REG_PREFIX #n
#else
#define PPC_REGNAME(n) #n
#endif

#define CALL_MACHINE_CODE(pc)						    \
  __asm__ __volatile__( "	mtctr %0\n"				    \
			"	mr "PPC_REGNAME(31)",%1\n"		    \
			"	bctr"					    \
			:						    \
			: "r" (pc), "r" (&Pike_interpreter)		    \
			: "ctr", "lr", "cc", "memory", "r31", "r0",	    \
			  "r3", "r4", "r5", "r6", "r7", "r8", "r9",	    \
			  "r10", "r11", "r12")

