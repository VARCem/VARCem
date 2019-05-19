/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the CPU module.
 *
 * Version:	@(#)cpu.h	1.0.14	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		leilei,
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
 *		Copyright 2016-2018 leilei.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free  Software  Foundation; either  version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is  distributed in the hope that it will be useful, but
 * WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
 * MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE. See  the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *   Free Software Foundation, Inc.
 *   59 Temple Place - Suite 330
 *   Boston, MA 02111-1307
 *   USA.
 */
#ifndef EMU_CPU_H
# define EMU_CPU_H


/* Supported CPU manufacturers. */
#define MANU_INTEL	0
#define MANU_AMD	1
#define MANU_CYRIX	2
#define MANU_IDT	3
#define MANU_NEC	4

/*
 * Supported CPU types.
 * FIXME: We should encode the CPU class in the ID's.
 */
#define CPU_8088	0		/* 808x class CPUs */
#define CPU_8086	1
#define CPU_NEC		2		/* NEC V20 or V30 */
#define CPU_186		3		/* 186 class CPUs */
#define CPU_286		4		/* 286 class CPUs */
#define CPU_386SX	5		/* 386 class CPUs */
#define CPU_386DX	6
#define CPU_RAPIDCAD	7
#define CPU_486SLC	8
#define CPU_486DLC	9
#define CPU_i486SX	10		/* 486 class CPUs */
#define CPU_Am486SX	11
#define CPU_Cx486S	12
#define CPU_i486DX	13
#define CPU_Am486DX	14
#define CPU_Cx486DX	15
#define CPU_iDX4	16
#define CPU_Cx5x86	17
#define CPU_WINCHIP	18		/* 586 class CPUs */
#define CPU_PENTIUM	19
#define CPU_PENTIUM_MMX	20
#define CPU_Cx6x86 	21
#define CPU_Cx6x86MX 	22
#define CPU_Cx6x86L 	23
#define CPU_CxGX1 	24
#define CPU_K5		25
#define CPU_5K86	26
#define CPU_K6		27
#define CPU_PENTIUM_PRO	28		/* 686 class CPUs */
#define CPU_PENTIUM_2	29
#define CPU_PENTIUM_2D	30

#define CPU_SUPPORTS_DYNAREC 1
#define CPU_REQUIRES_DYNAREC 2
#define CPU_ALTERNATE_XTAL   4


typedef struct {
    const char	*name;
    int		type;

    uint32_t	rspeed;
    int		multi;
    int		fsb_speed;

    uint32_t	edx_reset;
    uint32_t	cpuid_model;
    uint16_t	cyrix_id;

    int		flags;

    int		mem_read_cycles,
		mem_write_cycles;
    int		cache_read_cycles,
		cache_write_cycles;

    int		atclk_div;
} CPU;


extern const CPU	cpus_8088[];
extern const CPU	cpus_8086[];
extern const CPU	cpus_nec[];
extern const CPU	cpus_186[];
extern const CPU	cpus_286[];
extern const CPU	cpus_i386SX[];
extern const CPU	cpus_i386DX[];
extern const CPU	cpus_Am386SX[];
extern const CPU	cpus_Am386DX[];
extern const CPU	cpus_486SLC[];
extern const CPU	cpus_486DLC[];
extern const CPU	cpus_i486[];
extern const CPU	cpus_Am486[];
extern const CPU	cpus_Cx486[];
extern const CPU	cpus_WinChip[];
extern const CPU	cpus_Pentium5V[];
extern const CPU	cpus_Pentium5V50[];
extern const CPU	cpus_PentiumS5[];
#if defined(DEV_BRANCH) && defined(USE_AMD_K)
extern const CPU	cpus_K5[];
extern const CPU	cpus_K56[];
# define CPU_AMD_K5	{"AMD",cpus_K5}
# define CPU_AMD_K56	{"AMD",cpus_K56}
#else
# define CPU_AMD_K5	{"",NULL}
# define CPU_AMD_K56	{"",NULL}
#endif
extern const CPU	cpus_Pentium[];
extern const CPU	cpus_6x86[];
extern const CPU	cpus_PentiumPro[];
extern const CPU	cpus_Pentium2[];
extern const CPU	cpus_Pentium2D[];


#define C_FLAG		0x0001
#define P_FLAG		0x0004
#define A_FLAG		0x0010
#define Z_FLAG		0x0040
#define N_FLAG		0x0080
#define T_FLAG		0x0100
#define I_FLAG		0x0200
#define D_FLAG		0x0400
#define V_FLAG		0x0800
#define NT_FLAG		0x4000

#define VM_FLAG		0x0002			/* in EFLAGS */
#define VIF_FLAG	0x0008			/* in EFLAGS */
#define VIP_FLAG	0x0010			/* in EFLAGS */

#define WP_FLAG		0x10000			/* in CR0 */
#define CR4_VME		(1 << 0)
#define CR4_PVI		(1 << 1)
#define CR4_PSE		(1 << 4)

#define CPL		((_cs.access>>5)&3)
#define IOPL		((flags>>12)&3)
#define IOPLp		((!(msw&1)) || (CPL<=IOPL))


typedef union {
    uint32_t	l;
    uint16_t	w;
    struct {
	uint8_t	l,
		h;
    }		b;
} x86reg;

typedef struct {
    uint32_t	base;
    uint32_t	limit;
    uint8_t	access;
    uint16_t	seg;
    uint32_t	limit_low,
		limit_high;
    int		checked; /*Non-zero if selector is known to be valid*/
} x86seg;

typedef union {
    uint64_t	q;
    int64_t	sq;
    uint32_t	l[2];
    int32_t	sl[2];
    uint16_t	w[4];
    int16_t	sw[4];
    uint8_t	b[8];
    int8_t	sb[8];
} MMX_REG;

typedef struct {
    uint32_t	tr1, tr12;
    uint32_t	cesr;
    uint32_t	fcr;
    uint64_t	fcr2, fcr3;
} msr_t;

typedef union {
    uint32_t l;
    uint16_t w;
} cr0_t;


typedef struct {
    x86reg	regs[8];

    uint8_t	tag[8];

    x86seg	*ea_seg;
    uint32_t	eaaddr;

    int		flags_op;
    uint32_t	flags_res;
    uint32_t	flags_op1,
		flags_op2;

    uint32_t	pc;
    uint32_t	oldpc;
    uint32_t	op32;  

    int		TOP;

    union {
	struct {
	    int8_t	rm,
			mod,
			reg;
	} rm_mod_reg;
	int32_t		rm_mod_reg_data;
    }		rm_data;

    int8_t	ssegs;
    int8_t	ismmx;
    int8_t	abrt;

    int		_cycles;
    int		cpu_recomp_ins;

    uint16_t	npxs,
		npxc;

    double	ST[8];

    uint16_t	MM_w4[8];

    MMX_REG	MM[8];

    uint16_t	old_npxc,
		new_npxc;
    uint32_t	last_ea;
} cpu_state_t;

extern cpu_state_t cpu_state;

/*The flags below must match in both cpu_cur_status and block->status for a block
  to be valid*/
#define CPU_STATUS_USE32   (1 << 0)
#define CPU_STATUS_STACK32 (1 << 1)
#define CPU_STATUS_PMODE   (1 << 2)
#define CPU_STATUS_V86     (1 << 3)
#define CPU_STATUS_FLAGS 0xffff

/*If the flags below are set in cpu_cur_status, they must be set in block->status.
  Otherwise they are ignored*/
#define CPU_STATUS_NOTFLATDS  (1 << 16)
#define CPU_STATUS_NOTFLATSS  (1 << 17)
#define CPU_STATUS_MASK 0xffff0000

/* FIXME: why not use #if sizeof(cpu_state) <= 128 #error?  --FvK */
#ifdef _MSC_VER
# define COMPILE_TIME_ASSERT(expr)	/*nada*/
#else
# define COMPILE_TIME_ASSERT(expr) typedef char COMP_TIME_ASSERT[(expr) ? 1 : 0];
#endif

COMPILE_TIME_ASSERT(sizeof(cpu_state) <= 128)

#define cpu_state_offset(MEMBER) ((uint8_t)((uintptr_t)&cpu_state.MEMBER - (uintptr_t)&cpu_state - 128))

#define EAX	cpu_state.regs[0].l
#define AX	cpu_state.regs[0].w
#define AL	cpu_state.regs[0].b.l
#define AH	cpu_state.regs[0].b.h
#define ECX	cpu_state.regs[1].l
#define CX	cpu_state.regs[1].w
#define CL	cpu_state.regs[1].b.l
#define CH	cpu_state.regs[1].b.h
#define EDX	cpu_state.regs[2].l
#define DX	cpu_state.regs[2].w
#define DL	cpu_state.regs[2].b.l
#define DH	cpu_state.regs[2].b.h
#define EBX	cpu_state.regs[3].l
#define BX	cpu_state.regs[3].w
#define BL	cpu_state.regs[3].b.l
#define BH	cpu_state.regs[3].b.h
#define ESP	cpu_state.regs[4].l
#define EBP	cpu_state.regs[5].l
#define ESI	cpu_state.regs[6].l
#define EDI	cpu_state.regs[7].l
#define SP	cpu_state.regs[4].w
#define BP	cpu_state.regs[5].w
#define SI	cpu_state.regs[6].w
#define DI	cpu_state.regs[7].w

#define cycles	cpu_state._cycles

#define cpu_rm	cpu_state.rm_data.rm_mod_reg.rm
#define cpu_mod	cpu_state.rm_data.rm_mod_reg.mod
#define cpu_reg	cpu_state.rm_data.rm_mod_reg.reg

#define CR4_TSD  (1 << 2)
#define CR4_DE   (1 << 3)
#define CR4_MCE  (1 << 6)
#define CR4_PCE  (1 << 8)
#define CR4_OSFXSR  (1 << 9)


/* Global variables. */
extern int		cpu_manuf;		/* cpu manufacturer */
extern int		cpu_dynarec;		/* dynamic recompiler enabled */
extern int		cpu_busspeed;
extern int		cpu_16bitbus;
extern int		xt_cpu_multi;
extern int		cpu_cyrix_alignment;	/*Cyrix 5x86/6x86 only has data misalignment
					  penalties when crossing 8-byte boundaries*/

extern int		is8086,	is186, is286, is386, is486;
extern int		is_nec, is_rapidcad, is_cyrix, is_pentium;
extern int		cpu_hasrdtsc;
extern int		cpu_hasMSR;
extern int		cpu_hasMMX;
extern int		cpu_hasCR4;
extern int		cpu_hasVME;

extern uint32_t		cpu_cur_status;
extern uint64_t		cpu_CR4_mask;
extern uint64_t		tsc;
extern msr_t		msr;
extern uint8_t		opcode;
extern int		fpucount;
extern int		cgate16;
extern int		cpl_override;
extern int		CPUID;
extern int		isa_cycles;

extern uint16_t		flags,eflags;
extern uint32_t		oldds,oldss,olddslimit,oldsslimit,olddslimitw,oldsslimitw;
extern int		ins;		// FIXME: get rid of this!

extern float		bus_timing;
extern uint16_t		cs_msr;
extern uint32_t		esp_msr;
extern uint32_t		eip_msr;

/* For the AMD K6. */
extern uint64_t		star;

#define FPU_CW_Reserved_Bits (0xe0c0)

extern cr0_t		CR0;
#define cr0		CR0.l
#define msw		CR0.w
extern uint32_t		cr2, cr3, cr4;
extern uint32_t		dr[8];


/*Segments -
  _cs,_ds,_es,_ss are the segment structures
  CS,DS,ES,SS is the 16-bit data
  cs,ds,es,ss are defines to the bases*/
extern x86seg	gdt,ldt,idt,tr;
extern x86seg	_cs,_ds,_es,_ss,_fs,_gs;
extern x86seg	_oldds;
#define CS	_cs.seg
#define DS	_ds.seg
#define ES	_es.seg
#define SS	_ss.seg
#define FS	_fs.seg
#define GS	_gs.seg
#define cs	_cs.base
#define ds	_ds.base
#define es	_es.base
#define ss	_ss.base
#define seg_fs	_fs.base
#define gs	_gs.base


#if 1
# define ISA_CYCLES_SHIFT 6
# define ISA_CYCLES(x)    ((x * isa_cycles) >> ISA_CYCLES_SHIFT)
#else
# define ISA_CYCLES(x)    (x * isa_cycles)
#endif


extern int	cpu_cycles_read, cpu_cycles_read_l,
		cpu_cycles_write, cpu_cycles_write_l;
extern int	cpu_prefetch_cycles, cpu_prefetch_width,
		cpu_mem_prefetch_cycles, cpu_rom_prefetch_cycles;
extern int	cpu_cache_int_enabled, cpu_cache_ext_enabled;

extern int	timing_rr;
extern int	timing_mr, timing_mrl;
extern int	timing_rm, timing_rml;
extern int	timing_mm, timing_mml;
extern int	timing_bt, timing_bnt;
extern int	timing_int, timing_int_rm, timing_int_v86, timing_int_pm;
extern int	timing_int_pm_outer, timing_iret_rm, timing_iret_v86, timing_iret_pm;
extern int	timing_iret_pm_outer, timing_call_rm, timing_call_pm;
extern int	timing_call_pm_gate, timing_call_pm_gate_inner;
extern int	timing_retf_rm, timing_retf_pm, timing_retf_pm_outer;
extern int	timing_jmp_rm, timing_jmp_pm, timing_jmp_pm_gate;
extern int	timing_misaligned;


/* Functions. */
extern void	loadseg(uint16_t seg, x86seg *s);
extern void	loadcs(uint16_t seg);

extern void	cpu_set_type(const CPU *list,int manuf,int type,int fpu,int dyna);
extern int	cpu_get_type(void);
extern const char *cpu_get_name(void);
extern int	cpu_set_speed(int new_speed);
extern uint32_t	cpu_get_speed(void);
extern int	cpu_get_flags(void);
extern void	cpu_update_waitstates(void);
extern char	*cpu_current_pc(char *bufp);
extern void	cpu_set_edx(void);
extern void	cpu_reset(int hard);
extern void	cpu_dumpregs(int __force);

extern void	execx86(int cycs);
extern void	exec386(int cycs);
extern void	exec386_dynarec(int cycs);

extern void	cpu_CPUID(void);
extern void	cpu_RDMSR(void);
extern void	cpu_WRMSR(void);

extern int	checkio(uint32_t port);
extern void	codegen_block_end(void);
extern void	codegen_reset(void);
extern int	divl(uint32_t val);
extern int	idivl(int32_t val);
extern void	loadcscall(uint16_t seg);
extern void	loadcsjmp(uint16_t seg, uint32_t oxpc);
extern void	pmodeint(int num, int soft);
extern void	pmoderetf(int is32, uint16_t off);
extern void	pmodeiret(int is32);
extern void	resetmcr(void);
extern void	refreshread(void);
extern void	resetreadlookup(void);
extern void	x86_int_sw(uint32_t num);
extern int	x86_int_sw_rm(int num);
extern void	x86gpf(char *s, uint16_t error);
extern void	x86np(char *s, uint16_t error);
extern void	x86ss(char *s, uint16_t error);
extern void	x86ts(char *s, uint16_t error);
extern void	x87_dumpregs(void);
extern void	x87_reset(void);


#endif	/*EMU_CPU_H*/
