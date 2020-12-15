/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		CPU type handler.
 *
 * Version:	@(#)cpu.c	1.0.16	2021/02/12
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		leilei,
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2018-2021 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "../emu.h"
#include "cpu.h"
#include "../device.h"
#include "../io.h"
#include "x86.h"
#include "x86_ops.h"
#include "../mem.h"
#include "../devices/system/pci.h"
#ifdef USE_DYNAREC
# include "codegen.h"
#endif

uint32_t cpu_features;
int cpu_has_feature(int feature)
{
        return cpu_features & feature;
}

enum {
    CPUID_FPU = (1 << 0),
    CPUID_VME = (1 << 1),
    CPUID_PSE = (1 << 3),
    CPUID_TSC = (1 << 4),
    CPUID_MSR = (1 << 5),
    CPUID_CMPXCHG8B = (1 << 8),
    CPUID_AMDSEP = (1 << 10),
    CPUID_SEP = (1 << 11),
    CPUID_CMOV = (1 << 15),
    CPUID_MMX = (1 << 23),
    CPUID_FXSR = (1 << 24)
};

/*Addition flags returned by CPUID function 0x80000001*/
enum
{
	CPUID_3DNOW = (1 << 31)
};

const OpFn	*x86_opcodes;
const OpFn	*x86_opcodes_0f;
const OpFn	*x86_opcodes_d8_a16;
const OpFn	*x86_opcodes_d8_a32;
const OpFn	*x86_opcodes_d9_a16;
const OpFn	*x86_opcodes_d9_a32;
const OpFn	*x86_opcodes_da_a16;
const OpFn	*x86_opcodes_da_a32;
const OpFn	*x86_opcodes_db_a16;
const OpFn	*x86_opcodes_db_a32;
const OpFn	*x86_opcodes_dc_a16;
const OpFn	*x86_opcodes_dc_a32;
const OpFn	*x86_opcodes_dd_a16;
const OpFn	*x86_opcodes_dd_a32;
const OpFn	*x86_opcodes_de_a16;
const OpFn	*x86_opcodes_de_a32;
const OpFn	*x86_opcodes_df_a16;
const OpFn	*x86_opcodes_df_a32;
const OpFn	*x86_opcodes_REPE;
const OpFn	*x86_opcodes_REPNE;
const OpFn	*x86_opcodes_3DNOW;
#ifdef USE_DYNAREC
const OpFn	*x86_dynarec_opcodes;
const OpFn	*x86_dynarec_opcodes_0f;
const OpFn	*x86_dynarec_opcodes_d8_a16;
const OpFn	*x86_dynarec_opcodes_d8_a32;
const OpFn	*x86_dynarec_opcodes_d9_a16;
const OpFn	*x86_dynarec_opcodes_d9_a32;
const OpFn	*x86_dynarec_opcodes_da_a16;
const OpFn	*x86_dynarec_opcodes_da_a32;
const OpFn	*x86_dynarec_opcodes_db_a16;
const OpFn	*x86_dynarec_opcodes_db_a32;
const OpFn	*x86_dynarec_opcodes_dc_a16;
const OpFn	*x86_dynarec_opcodes_dc_a32;
const OpFn	*x86_dynarec_opcodes_dd_a16;
const OpFn	*x86_dynarec_opcodes_dd_a32;
const OpFn	*x86_dynarec_opcodes_de_a16;
const OpFn	*x86_dynarec_opcodes_de_a32;
const OpFn	*x86_dynarec_opcodes_df_a16;
const OpFn	*x86_dynarec_opcodes_df_a32;
const OpFn	*x86_dynarec_opcodes_REPE;
const OpFn	*x86_dynarec_opcodes_REPNE;
const OpFn  *x86_dynarec_opcodes_3DNOW;
#endif


int		cpu_manuf;
int		cpu_dynarec;
int		cpu_busspeed;
int		cpu_16bitbus;
int		isa_cycles;
int		cpu_cyrix_alignment;
int		CPUID;
uint64_t	cpu_CR4_mask;
int		cpu_cycles_read, cpu_cycles_read_l,
		cpu_cycles_write, cpu_cycles_write_l;
int		cpu_prefetch_cycles, cpu_prefetch_width,
		cpu_mem_prefetch_cycles, cpu_rom_prefetch_cycles;
int		cpu_cache_int_enabled, cpu_cache_ext_enabled;

int		is186,			/* 80186 */
		is286,			/* 80286 */
		is386,			/* 80386 {SX,DX} */
		is486,			/* 80486 {SX,DX} */
		is_nec,			/* NEC V20/V30 */
		is_cyrix,		/* Cyrix */
		is_rapidcad,		/* RapidCAD */
		is_pentium;		/* Pentium+ */


uint64_t	tsc = 0;
msr_t		msr;
cr0_t		CR0;

/* Variables for the 686+ processors. */
uint16_t	cs_msr = 0;
uint32_t	esp_msr = 0;
uint32_t	eip_msr = 0;
uint64_t	apic_base_msr = 0;
uint64_t	mtrr_cap_msr = 0;
uint64_t	mtrr_physbase_msr[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint64_t	mtrr_physmask_msr[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint64_t	mtrr_fix64k_8000_msr = 0;
uint64_t	mtrr_fix16k_8000_msr = 0;
uint64_t	mtrr_fix16k_a000_msr = 0;
uint64_t	mtrr_fix4k_msr[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint64_t	pat_msr = 0;
uint64_t	mtrr_deftype_msr = 0;
uint64_t	msr_ia32_pmc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint64_t	ecx17_msr = 0;
uint64_t	ecx79_msr = 0;
uint64_t	ecx8x_msr[4] = {0, 0, 0, 0};
uint64_t	ecx116_msr = 0;
uint64_t	ecx11x_msr[4] = {0, 0, 0, 0};
uint64_t	ecx11e_msr = 0;
uint64_t	ecx186_msr = 0;
uint64_t	ecx187_msr = 0;
uint64_t	ecx1e0_msr = 0;
uint64_t	ecx570_msr = 0;

#if defined(DEV_BRANCH) && defined(USE_AMD_K)
/* Variables for the AMD "K" processors. */
uint64_t	star = 0;			/* These are K6-only. */
#endif

int		timing_rr;
int		timing_mr, timing_mrl;
int		timing_rm, timing_rml;
int		timing_mm, timing_mml;
int		timing_bt, timing_bnt;
int		timing_int, timing_int_rm, timing_int_v86, timing_int_pm,
		timing_int_pm_outer;
int		timing_iret_rm, timing_iret_v86, timing_iret_pm,
		timing_iret_pm_outer;
int		timing_call_rm, timing_call_pm, timing_call_pm_gate,
		timing_call_pm_gate_inner;
int		timing_retf_rm, timing_retf_pm, timing_retf_pm_outer;
int		timing_jmp_rm, timing_jmp_pm, timing_jmp_pm_gate;
int		timing_misaligned;


static const CPU *cpu_list,
		*cpu = NULL;
static int	cpu_turbo = -1,
		cpu_effective = -1,
		cpu_waitstates,
		cpu_extfpu;
static uint32_t	cpu_speed;

#if defined(DEV_BRANCH) && defined(USE_AMD_K)
/* Variables for the AMD "K" processors. */
static uint64_t	ecx83_msr = 0;			/* AMD K5 and K6 MSR's. */
static uint64_t	sfmask = 0;
#endif

/* Variables for the Cyrix processors. */
static uint8_t	ccr0, ccr1, ccr2, ccr3, ccr4, ccr5, ccr6;
static int	cyrix_addr;


static void
cyrix_write(uint16_t addr, uint8_t val, priv_t priv)
{
    if (addr & 1) switch (cyrix_addr) {
	case 0xc0: /*CCR0*/
		ccr0 = val;
		break;

	case 0xc1: /*CCR1*/
		ccr1 = val;
		break;

	case 0xc2: /*CCR2*/
		ccr2 = val;
		break;

	case 0xc3: /*CCR3*/
		ccr3 = val;
		break;

	case 0xe8: /*CCR4*/
		if ((ccr3 & 0xf0) == 0x10) {
			ccr4 = val;
			if (cpu->type >= CPU_Cx6x86) {
				if (val & 0x80)
					CPUID = cpu->cpuid_model;
				else
					CPUID = 0;
			}
		}
		break;

	case 0xe9: /*CCR5*/
		if ((ccr3 & 0xf0) == 0x10)
			ccr5 = val;
		break;

	case 0xea: /*CCR6*/
		if ((ccr3 & 0xf0) == 0x10)
			ccr6 = val;
		break;
    } else
	cyrix_addr = val;
}


static uint8_t
cyrix_read(uint16_t addr, priv_t priv)
{
    if (addr & 1) {
	switch (cyrix_addr) {
		case 0xc0: return ccr0;
		case 0xc1: return ccr1;
		case 0xc2: return ccr2;
		case 0xc3: return ccr3;
		case 0xe8: return ((ccr3 & 0xf0) == 0x10) ? ccr4 : 0xff;
		case 0xe9: return ((ccr3 & 0xf0) == 0x10) ? ccr5 : 0xff;
		case 0xea: return ((ccr3 & 0xf0) == 0x10) ? ccr6 : 0xff;
		case 0xfe: return cpu->cyrix_id & 0xff;
		case 0xff: return cpu->cyrix_id >> 8;
	}

	if (cyrix_addr == 0x20 && cpu->type == CPU_Cx5x86)
		return 0xff;
    }

    return 0xff;
}


/*
 * Actually do the 'setup' work.
 *
 * Given the currently-selected CPU from the list the machine
 * has provided us with, perform all actions needed to make it
 * the active CPU.
 */
static void
cpu_activate(void)
{
    int hasfpu;

    /* Select the desired CPU and/or speed. */
    cpu = &cpu_list[cpu_effective];
    INFO("CPU: %s [%i] speed=%lu\n", cpu->name, cpu_effective, cpu->rspeed);

    CPUID    = cpu->cpuid_model;
    is8086   = (cpu->type > CPU_8088);
    is_nec   = (cpu->type == CPU_NEC);
    is186    = (cpu->type == CPU_186);
    is286    = (cpu->type >= CPU_286);
    is386    = (cpu->type >= CPU_386SX);
    is_rapidcad = (cpu->type == CPU_RAPIDCAD);
    is486    = ((cpu->type >= CPU_i486SX) || (cpu->type == CPU_486SLC) || \
	        (cpu->type == CPU_486DLC) || (cpu->type == CPU_RAPIDCAD));
    is_cyrix = ((cpu->type == CPU_486SLC) || (cpu->type == CPU_486DLC) || \
	        (cpu->type == CPU_Cx486S) || (cpu->type == CPU_Cx486DX) || \
	        (cpu->type == CPU_Cx5x86) || (cpu->type == CPU_Cx6x86) || \
	        (cpu->type == CPU_Cx6x86MX) || (cpu->type == CPU_Cx6x86L) || \
	        (cpu->type == CPU_CxGX1));
    is_pentium = (cpu->type >= CPU_WINCHIP);
    hasfpu   = (cpu->type >= CPU_i486DX) || (cpu->type == CPU_RAPIDCAD);
    DEBUG("CPU: manuf=%i model=%i cpuid=%08lx\n",
	cpu_manuf, cpu_effective, CPUID);
    DEBUG("     8086=%i nec=%i 186=%i 286=%i 386=%i rcad=%i cyrix=%i 486=%i pent=%i\n",
	is8086,is_nec,is186,is286,is386,is_rapidcad,is_cyrix,is486,is_pentium);
    DEBUG("     hasfpu=%i\n", hasfpu);

    cpu_16bitbus = ((cpu->type == CPU_286) || \
		    (cpu->type == CPU_386SX) || \
		    (cpu->type == CPU_486SLC));
    cpu_speed = cpu->rspeed;
    if (cpu->multi) 
	   cpu_busspeed = cpu->rspeed / cpu->multi;

    ccr0 = ccr1 = ccr2 = ccr3 = ccr4 = ccr5 = ccr6 = 0;

    if ((cpu->type == CPU_286) || (cpu->type == CPU_386SX) ||
	(cpu->type == CPU_386DX) || (cpu->type == CPU_i486SX)) {
	if (cpu_extfpu)
		hasfpu = 1;
    }

    cpu_update_waitstates();

    isa_cycles = cpu->atclk_div;

    //FIXME: will have to use new cpu_speed variable! --FvK
    if (cpu->rspeed <= 8000000)
	cpu_rom_prefetch_cycles = cpu_mem_prefetch_cycles;
    else
	cpu_rom_prefetch_cycles = cpu->rspeed / 1000000;

    /* Set the PCI bus speed(s). */
    pci_set_speed(cpu->fsb_speed);

    if (is_cyrix)
	io_sethandler(0x0022, 2,
		      cyrix_read,NULL,NULL, cyrix_write,NULL,NULL, NULL);
    else
	io_removehandler(0x0022, 2,
			 cyrix_read,NULL,NULL, cyrix_write,NULL,NULL, NULL);
	
#ifdef USE_DYNAREC
    x86_setopcodes(ops_386, ops_386_0f, dynarec_ops_386, dynarec_ops_386_0f);
#else
    x86_setopcodes(ops_386, ops_386_0f);
#endif
    x86_opcodes_REPE = ops_REPE;
    x86_opcodes_REPNE = ops_REPNE;
	x86_opcodes_3DNOW = ops_3DNOW;
#ifdef USE_DYNAREC
    x86_dynarec_opcodes_REPE = dynarec_ops_REPE;
    x86_dynarec_opcodes_REPNE = dynarec_ops_REPNE;
	x86_dynarec_opcodes_3DNOW = dynarec_ops_3DNOW;
#endif

#ifdef USE_DYNAREC
    if (hasfpu) {
	x86_dynarec_opcodes_d8_a16 = dynarec_ops_fpu_d8_a16;
	x86_dynarec_opcodes_d8_a32 = dynarec_ops_fpu_d8_a32;
	x86_dynarec_opcodes_d9_a16 = dynarec_ops_fpu_d9_a16;
	x86_dynarec_opcodes_d9_a32 = dynarec_ops_fpu_d9_a32;
	x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_da_a16;
	x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_da_a32;
	x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_db_a16;
	x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_db_a32;
	x86_dynarec_opcodes_dc_a16 = dynarec_ops_fpu_dc_a16;
	x86_dynarec_opcodes_dc_a32 = dynarec_ops_fpu_dc_a32;
	x86_dynarec_opcodes_dd_a16 = dynarec_ops_fpu_dd_a16;
	x86_dynarec_opcodes_dd_a32 = dynarec_ops_fpu_dd_a32;
	x86_dynarec_opcodes_de_a16 = dynarec_ops_fpu_de_a16;
	x86_dynarec_opcodes_de_a32 = dynarec_ops_fpu_de_a32;
	x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_df_a16;
	x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_df_a32;
    } else {
	x86_dynarec_opcodes_d8_a16 = dynarec_ops_nofpu_a16;
	x86_dynarec_opcodes_d8_a32 = dynarec_ops_nofpu_a32;
	x86_dynarec_opcodes_d9_a16 = dynarec_ops_nofpu_a16;
	x86_dynarec_opcodes_d9_a32 = dynarec_ops_nofpu_a32;
	x86_dynarec_opcodes_da_a16 = dynarec_ops_nofpu_a16;
	x86_dynarec_opcodes_da_a32 = dynarec_ops_nofpu_a32;
	x86_dynarec_opcodes_db_a16 = dynarec_ops_nofpu_a16;
	x86_dynarec_opcodes_db_a32 = dynarec_ops_nofpu_a32;
	x86_dynarec_opcodes_dc_a16 = dynarec_ops_nofpu_a16;
	x86_dynarec_opcodes_dc_a32 = dynarec_ops_nofpu_a32;
	x86_dynarec_opcodes_dd_a16 = dynarec_ops_nofpu_a16;
	x86_dynarec_opcodes_dd_a32 = dynarec_ops_nofpu_a32;
	x86_dynarec_opcodes_de_a16 = dynarec_ops_nofpu_a16;
	x86_dynarec_opcodes_de_a32 = dynarec_ops_nofpu_a32;
	x86_dynarec_opcodes_df_a16 = dynarec_ops_nofpu_a16;
	x86_dynarec_opcodes_df_a32 = dynarec_ops_nofpu_a32;
    }
    codegen_timing_set(&codegen_timing_486);
#endif

    if (hasfpu) {
	x86_opcodes_d8_a16 = ops_fpu_d8_a16;
	x86_opcodes_d8_a32 = ops_fpu_d8_a32;
	x86_opcodes_d9_a16 = ops_fpu_d9_a16;
	x86_opcodes_d9_a32 = ops_fpu_d9_a32;
	x86_opcodes_da_a16 = ops_fpu_da_a16;
	x86_opcodes_da_a32 = ops_fpu_da_a32;
	x86_opcodes_db_a16 = ops_fpu_db_a16;
	x86_opcodes_db_a32 = ops_fpu_db_a32;
	x86_opcodes_dc_a16 = ops_fpu_dc_a16;
	x86_opcodes_dc_a32 = ops_fpu_dc_a32;
	x86_opcodes_dd_a16 = ops_fpu_dd_a16;
	x86_opcodes_dd_a32 = ops_fpu_dd_a32;
	x86_opcodes_de_a16 = ops_fpu_de_a16;
	x86_opcodes_de_a32 = ops_fpu_de_a32;
	x86_opcodes_df_a16 = ops_fpu_df_a16;
	x86_opcodes_df_a32 = ops_fpu_df_a32;
    } else {
	x86_opcodes_d8_a16 = ops_nofpu_a16;
	x86_opcodes_d8_a32 = ops_nofpu_a32;
	x86_opcodes_d9_a16 = ops_nofpu_a16;
	x86_opcodes_d9_a32 = ops_nofpu_a32;
	x86_opcodes_da_a16 = ops_nofpu_a16;
	x86_opcodes_da_a32 = ops_nofpu_a32;
	x86_opcodes_db_a16 = ops_nofpu_a16;
	x86_opcodes_db_a32 = ops_nofpu_a32;
	x86_opcodes_dc_a16 = ops_nofpu_a16;
	x86_opcodes_dc_a32 = ops_nofpu_a32;
	x86_opcodes_dd_a16 = ops_nofpu_a16;
	x86_opcodes_dd_a32 = ops_nofpu_a32;
	x86_opcodes_de_a16 = ops_nofpu_a16;
	x86_opcodes_de_a32 = ops_nofpu_a32;
	x86_opcodes_df_a16 = ops_nofpu_a16;
	x86_opcodes_df_a32 = ops_nofpu_a32;
    }

    memset(&msr, 0, sizeof(msr));

    timing_misaligned = 0;
    cpu_cyrix_alignment = 0;

    switch (cpu->type) {
	case CPU_8088:
	case CPU_8086:
	case CPU_NEC:
	case CPU_186:
		break;

	case CPU_286:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_286, ops_286_0f, dynarec_ops_286, dynarec_ops_286_0f);
#else
		x86_setopcodes(ops_286, ops_286_0f);
#endif
		if (cpu_extfpu) {
#ifdef USE_DYNAREC
			x86_dynarec_opcodes_d9_a16 = dynarec_ops_fpu_287_d9_a16;
			x86_dynarec_opcodes_d9_a32 = dynarec_ops_fpu_287_d9_a32;
			x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_287_da_a16;
			x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_287_da_a32;
			x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_287_db_a16;
			x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_287_db_a32;
			x86_dynarec_opcodes_dc_a16 = dynarec_ops_fpu_287_dc_a16;
			x86_dynarec_opcodes_dc_a32 = dynarec_ops_fpu_287_dc_a32;
			x86_dynarec_opcodes_dd_a16 = dynarec_ops_fpu_287_dd_a16;
			x86_dynarec_opcodes_dd_a32 = dynarec_ops_fpu_287_dd_a32;
			x86_dynarec_opcodes_de_a16 = dynarec_ops_fpu_287_de_a16;
			x86_dynarec_opcodes_de_a32 = dynarec_ops_fpu_287_de_a32;
			x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_287_df_a16;
			x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_287_df_a32;
#endif
			x86_opcodes_d9_a16 = ops_fpu_287_d9_a16;
			x86_opcodes_d9_a32 = ops_fpu_287_d9_a32;
			x86_opcodes_da_a16 = ops_fpu_287_da_a16;
			x86_opcodes_da_a32 = ops_fpu_287_da_a32;
			x86_opcodes_db_a16 = ops_fpu_287_db_a16;
			x86_opcodes_db_a32 = ops_fpu_287_db_a32;
			x86_opcodes_dc_a16 = ops_fpu_287_dc_a16;
			x86_opcodes_dc_a32 = ops_fpu_287_dc_a32;
			x86_opcodes_dd_a16 = ops_fpu_287_dd_a16;
			x86_opcodes_dd_a32 = ops_fpu_287_dd_a32;
			x86_opcodes_de_a16 = ops_fpu_287_de_a16;
			x86_opcodes_de_a32 = ops_fpu_287_de_a32;
			x86_opcodes_df_a16 = ops_fpu_287_df_a16;
			x86_opcodes_df_a32 = ops_fpu_287_df_a32;
		}
		timing_rr  = 2;   /*register dest - register src*/
		timing_rm  = 7;   /*register dest - memory src*/
		timing_mr  = 7;   /*memory dest   - register src*/
		timing_mm  = 7;   /*memory dest   - memory src*/
		timing_rml = 9;   /*register dest - memory src long*/
		timing_mrl = 11;  /*memory dest   - register src long*/
		timing_mml = 11;  /*memory dest   - memory src*/
		timing_bt  = 7-3; /*branch taken*/
		timing_bnt = 3;   /*branch not taken*/
		timing_int = 0;
		timing_int_rm       = 23;
		timing_int_v86      = 0;
		timing_int_pm       = 40;
		timing_int_pm_outer = 78;
		timing_iret_rm       = 17;
		timing_iret_v86      = 0;
		timing_iret_pm       = 31;
		timing_iret_pm_outer = 55;
		timing_call_rm	    = 13;
		timing_call_pm	    = 26;
		timing_call_pm_gate       = 52;
		timing_call_pm_gate_inner = 82;
		timing_retf_rm       = 15;
		timing_retf_pm       = 25;
		timing_retf_pm_outer = 55;
		timing_jmp_rm      = 11;
		timing_jmp_pm      = 23;
		timing_jmp_pm_gate = 38;
		break;

	case CPU_386SX:
		timing_rr  = 2;   /*register dest - register src*/
   		timing_rm  = 6;   /*register dest - memory src*/
		timing_mr  = 7;   /*memory dest   - register src*/
		timing_mm  = 6;   /*memory dest   - memory src*/
		timing_rml = 8;   /*register dest - memory src long*/
		timing_mrl = 11;  /*memory dest   - register src long*/
		timing_mml = 10;  /*memory dest   - memory src*/
		timing_bt  = 7-3; /*branch taken*/
		timing_bnt = 3;   /*branch not taken*/
		timing_int = 0;
		timing_int_rm       = 37;
		timing_int_v86      = 59;
		timing_int_pm       = 99;
		timing_int_pm_outer = 119;
		timing_iret_rm       = 22;
		timing_iret_v86      = 60;
		timing_iret_pm       = 38;
		timing_iret_pm_outer = 82;
		timing_call_rm	    = 17;
		timing_call_pm	    = 34;
		timing_call_pm_gate       = 52;
		timing_call_pm_gate_inner = 86;
		timing_retf_rm       = 18;
		timing_retf_pm       = 32;
		timing_retf_pm_outer = 68;
		timing_jmp_rm      = 12;
		timing_jmp_pm      = 27;
		timing_jmp_pm_gate = 45;
		break;

	case CPU_386DX:
		timing_rr  = 2; /*register dest - register src*/
		timing_rm  = 6; /*register dest - memory src*/
		timing_mr  = 7; /*memory dest   - register src*/
		timing_mm  = 6; /*memory dest   - memory src*/
		timing_rml = 6; /*register dest - memory src long*/
		timing_mrl = 7; /*memory dest   - register src long*/
		timing_mml = 6; /*memory dest   - memory src*/
		timing_bt  = 7-3; /*branch taken*/
		timing_bnt = 3; /*branch not taken*/
		timing_int = 0;
		timing_int_rm       = 37;
		timing_int_v86      = 59;
		timing_int_pm       = 99;
		timing_int_pm_outer = 119;
		timing_iret_rm       = 22;
		timing_iret_v86      = 60;
		timing_iret_pm       = 38;
		timing_iret_pm_outer = 82;
		timing_call_rm	    = 17;
		timing_call_pm	    = 34;
		timing_call_pm_gate       = 52;
		timing_call_pm_gate_inner = 86;
		timing_retf_rm       = 18;
		timing_retf_pm       = 32;
		timing_retf_pm_outer = 68;
		timing_jmp_rm      = 12;
		timing_jmp_pm      = 27;
		timing_jmp_pm_gate = 45;
		break;
		
	case CPU_RAPIDCAD:
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 2; /*register dest - memory src*/
		timing_mr  = 3; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 2; /*register dest - memory src long*/
		timing_mrl = 3; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 3-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_int = 4;
		timing_int_rm       = 26;
		timing_int_v86      = 82;
		timing_int_pm       = 44;
		timing_int_pm_outer = 71;
		timing_iret_rm       = 15;
		timing_iret_v86      = 36; /*unknown*/
		timing_iret_pm       = 20;
		timing_iret_pm_outer = 36;
		timing_call_rm = 18;
		timing_call_pm = 20;
		timing_call_pm_gate = 35;
		timing_call_pm_gate_inner = 69;
		timing_retf_rm       = 13;
		timing_retf_pm       = 17;
		timing_retf_pm_outer = 35;
		timing_jmp_rm      = 17;
		timing_jmp_pm      = 19;
		timing_jmp_pm_gate = 32;
		break;
		
	case CPU_486SLC:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_486_0f, dynarec_ops_386, dynarec_ops_486_0f);
#else
		x86_setopcodes(ops_386, ops_486_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 3; /*register dest - memory src*/
		timing_mr  = 5; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 5; /*register dest - memory src long*/
		timing_mrl = 7; /*memory dest   - register src long*/
		timing_mml = 7;
		timing_bt  = 6-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		/*unknown*/
		timing_int = 4;
		timing_int_rm       = 14;
		timing_int_v86      = 82;
		timing_int_pm       = 49;
		timing_int_pm_outer = 77;
		timing_iret_rm       = 14;
		timing_iret_v86      = 66;
		timing_iret_pm       = 31;
		timing_iret_pm_outer = 66;
		timing_call_rm = 12;
		timing_call_pm = 30;
		timing_call_pm_gate = 41;
		timing_call_pm_gate_inner = 83;
		timing_retf_rm       = 13;
		timing_retf_pm       = 26;
		timing_retf_pm_outer = 61;
		timing_jmp_rm      = 9;
		timing_jmp_pm      = 26;
		timing_jmp_pm_gate = 37;
		timing_misaligned = 3;
		break;

	case CPU_486DLC:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_486_0f, dynarec_ops_386, dynarec_ops_486_0f);
#else
		x86_setopcodes(ops_386, ops_486_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 3; /*register dest - memory src*/
		timing_mr  = 3; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 3; /*register dest - memory src long*/
		timing_mrl = 3; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 6-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		/*unknown*/
		timing_int = 4;
		timing_int_rm       = 14;
		timing_int_v86      = 82;
		timing_int_pm       = 49;
		timing_int_pm_outer = 77;
		timing_iret_rm       = 14;
		timing_iret_v86      = 66;
		timing_iret_pm       = 31;
		timing_iret_pm_outer = 66;
		timing_call_rm = 12;
		timing_call_pm = 30;
		timing_call_pm_gate = 41;
		timing_call_pm_gate_inner = 83;
		timing_retf_rm       = 13;
		timing_retf_pm       = 26;
		timing_retf_pm_outer = 61;
		timing_jmp_rm      = 9;
		timing_jmp_pm      = 26;
		timing_jmp_pm_gate = 37;
		timing_misaligned = 3;
		break;

	case CPU_iDX4:
		cpu_features = CPU_FEATURE_CR4 | CPU_FEATURE_VME;
		cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_VME;
		/*FALLTHROUGH*/

	case CPU_i486SX:
	case CPU_i486DX:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_486_0f, dynarec_ops_386, dynarec_ops_486_0f);
#else
		x86_setopcodes(ops_386, ops_486_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 2; /*register dest - memory src*/
		timing_mr  = 3; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 2; /*register dest - memory src long*/
		timing_mrl = 3; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 3-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_int = 4;
		timing_int_rm       = 26;
		timing_int_v86      = 82;
		timing_int_pm       = 44;
		timing_int_pm_outer = 71;
		timing_iret_rm       = 15;
		timing_iret_v86      = 36; /*unknown*/
		timing_iret_pm       = 20;
		timing_iret_pm_outer = 36;
		timing_call_rm = 18;
		timing_call_pm = 20;
		timing_call_pm_gate = 35;
		timing_call_pm_gate_inner = 69;
		timing_retf_rm       = 13;
		timing_retf_pm       = 17;
		timing_retf_pm_outer = 35;
		timing_jmp_rm      = 17;
		timing_jmp_pm      = 19;
		timing_jmp_pm_gate = 32;
		timing_misaligned = 3;
		break;

	case CPU_Am486SX:
	case CPU_Am486DX:
		/*AMD timing identical to Intel*/
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_486_0f, dynarec_ops_386, dynarec_ops_486_0f);
#else
		x86_setopcodes(ops_386, ops_486_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 2; /*register dest - memory src*/
		timing_mr  = 3; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 2; /*register dest - memory src long*/
		timing_mrl = 3; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 3-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_int = 4;
		timing_int_rm       = 26;
		timing_int_v86      = 82;
		timing_int_pm       = 44;
		timing_int_pm_outer = 71;
		timing_iret_rm       = 15;
		timing_iret_v86      = 36; /*unknown*/
		timing_iret_pm       = 20;
		timing_iret_pm_outer = 36;
		timing_call_rm = 18;
		timing_call_pm = 20;
		timing_call_pm_gate = 35;
		timing_call_pm_gate_inner = 69;
		timing_retf_rm       = 13;
		timing_retf_pm       = 17;
		timing_retf_pm_outer = 35;
		timing_jmp_rm      = 17;
		timing_jmp_pm      = 19;
		timing_jmp_pm_gate = 32;
		timing_misaligned = 3;
		break;
		
	case CPU_Cx486S:
	case CPU_Cx486DX:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_486_0f, dynarec_ops_386, dynarec_ops_486_0f);
#else
		x86_setopcodes(ops_386, ops_486_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 3; /*register dest - memory src*/
		timing_mr  = 3; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 3; /*register dest - memory src long*/
		timing_mrl = 3; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 4-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_int = 4;
		timing_int_rm       = 14;
		timing_int_v86      = 82;
		timing_int_pm       = 49;
		timing_int_pm_outer = 77;
		timing_iret_rm       = 14;
		timing_iret_v86      = 66; /*unknown*/
		timing_iret_pm       = 31;
		timing_iret_pm_outer = 66;
		timing_call_rm = 12;
		timing_call_pm = 30;
		timing_call_pm_gate = 41;
		timing_call_pm_gate_inner = 83;
		timing_retf_rm       = 13;
		timing_retf_pm       = 26;
		timing_retf_pm_outer = 61;
		timing_jmp_rm      = 9;
		timing_jmp_pm      = 26;
		timing_jmp_pm_gate = 37;
		timing_misaligned = 3;
		break;
		
	case CPU_Cx5x86:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_486_0f, dynarec_ops_386, dynarec_ops_486_0f);
#else
		x86_setopcodes(ops_386, ops_486_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 1; /*register dest - memory src*/
		timing_mr  = 2; /*memory dest   - register src*/
		timing_mm  = 2;
		timing_rml = 1; /*register dest - memory src long*/
		timing_mrl = 2; /*memory dest   - register src long*/
		timing_mml = 2;
		timing_bt  = 5-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_int = 0;
		timing_int_rm       = 9;
		timing_int_v86      = 82; /*unknown*/
		timing_int_pm       = 21;
		timing_int_pm_outer = 32;
		timing_iret_rm       = 7;
		timing_iret_v86      = 26; /*unknown*/
		timing_iret_pm       = 10;
		timing_iret_pm_outer = 26;
		timing_call_rm = 4;
		timing_call_pm = 15;
		timing_call_pm_gate = 26;
		timing_call_pm_gate_inner = 35;
		timing_retf_rm       = 4;
		timing_retf_pm       = 7;
		timing_retf_pm_outer = 23;
		timing_jmp_rm      = 5;
		timing_jmp_pm      = 7;
		timing_jmp_pm_gate = 17;
		timing_misaligned = 2;
		cpu_cyrix_alignment = 1;
		break;

	case CPU_WINCHIP:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_winchip_0f, dynarec_ops_386, dynarec_ops_winchip_0f);
#else
		x86_setopcodes(ops_386, ops_winchip_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 2; /*register dest - memory src*/
		timing_mr  = 2; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 2; /*register dest - memory src long*/
		timing_mrl = 2; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 3-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MMX | CPU_FEATURE_MSR | CPU_FEATURE_CR4;
		msr.fcr = (1 << 8) | (1 << 16);
		cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
		/*unknown*/
		timing_int_rm       = 26;
		timing_int_v86      = 82;
		timing_int_pm       = 44;
		timing_int_pm_outer = 71;
		timing_iret_rm       = 7;
		timing_iret_v86      = 26;
		timing_iret_pm       = 10;
		timing_iret_pm_outer = 26;
		timing_call_rm = 4;
		timing_call_pm = 15;
		timing_call_pm_gate = 26;
		timing_call_pm_gate_inner = 35;
		timing_retf_rm       = 4;
		timing_retf_pm       = 7;
		timing_retf_pm_outer = 23;
		timing_jmp_rm      = 5;
		timing_jmp_pm      = 7;
		timing_jmp_pm_gate = 17;
#ifdef USE_DYNAREC
		codegen_timing_set(&codegen_timing_winchip);
#endif
		timing_misaligned = 2;
		cpu_cyrix_alignment = 1;
		break;

		case CPU_WINCHIP2:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_winchip2_0f, dynarec_ops_386, dynarec_ops_winchip2_0f);
#else
		x86_setopcodes(ops_386, ops_winchip2_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 2; /*register dest - memory src*/
		timing_mr  = 2; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 2; /*register dest - memory src long*/
		timing_mrl = 2; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 3-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MMX | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_3DNOW;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) | (1 << 16) | (1 << 18) | (1 << 19) | (1 << 20) | (1 << 21);
		cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
		/*unknown*/
		timing_int_rm       = 26;
		timing_int_v86      = 82;
		timing_int_pm       = 44;
		timing_int_pm_outer = 71;
		timing_iret_rm       = 7;
		timing_iret_v86      = 26;
		timing_iret_pm       = 10;
		timing_iret_pm_outer = 26;
		timing_call_rm = 4;
		timing_call_pm = 15;
		timing_call_pm_gate = 26;
		timing_call_pm_gate_inner = 35;
		timing_retf_rm       = 4;
		timing_retf_pm       = 7;
		timing_retf_pm_outer = 23;
		timing_jmp_rm      = 5;
		timing_jmp_pm      = 7;
		timing_jmp_pm_gate = 17;
		timing_misaligned = 2;
		cpu_cyrix_alignment = 1;
#ifdef USE_DYNAREC
		codegen_timing_set(&codegen_timing_winchip2);
#endif
		break;

	case CPU_PENTIUM:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
#else
		x86_setopcodes(ops_386, ops_pentium_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 2; /*register dest - memory src*/
		timing_mr  = 3; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 2; /*register dest - memory src long*/
		timing_mrl = 3; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 2; /*branch not taken*/
		timing_int = 6;
		timing_int_rm       = 11;
		timing_int_v86      = 54;
		timing_int_pm       = 25;
		timing_int_pm_outer = 42;
		timing_iret_rm       = 7;
		timing_iret_v86      = 27; /*unknown*/
		timing_iret_pm       = 10;
		timing_iret_pm_outer = 27;
		timing_call_rm = 4;
		timing_call_pm = 4;
		timing_call_pm_gate = 22;
		timing_call_pm_gate_inner = 44;
		timing_retf_rm       = 4;
		timing_retf_pm       = 4;
		timing_retf_pm_outer = 23;
		timing_jmp_rm      = 3;
		timing_jmp_pm      = 3;
		timing_jmp_pm_gate = 18;
		timing_misaligned = 3;
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
		cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE | CR4_PCE;
#ifdef USE_DYNAREC
		codegen_timing_set(&codegen_timing_pentium);
#endif
		break;

	case CPU_PENTIUM_MMX:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_pentiummmx_0f, dynarec_ops_386, dynarec_ops_pentiummmx_0f);
#else
		x86_setopcodes(ops_386, ops_pentiummmx_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 2; /*register dest - memory src*/
		timing_mr  = 3; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 2; /*register dest - memory src long*/
		timing_mrl = 3; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_int = 6;
		timing_int_rm       = 11;
		timing_int_v86      = 54;
		timing_int_pm       = 25;
		timing_int_pm_outer = 42;
		timing_iret_rm       = 7;
		timing_iret_v86      = 27; /*unknown*/
		timing_iret_pm       = 10;
		timing_iret_pm_outer = 27;
		timing_call_rm = 4;
		timing_call_pm = 4;
		timing_call_pm_gate = 22;
		timing_call_pm_gate_inner = 44;
		timing_retf_rm       = 4;
		timing_retf_pm       = 4;
		timing_retf_pm_outer = 23;
		timing_jmp_rm      = 3;
		timing_jmp_pm      = 3;
		timing_jmp_pm_gate = 18;
		timing_misaligned = 3;
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME | CPU_FEATURE_MMX;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
		cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE | CR4_PCE;
#ifdef USE_DYNAREC
		codegen_timing_set(&codegen_timing_pentium);
#endif
		break;

	case CPU_Cx6x86:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
#else
		x86_setopcodes(ops_386, ops_pentium_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 1; /*register dest - memory src*/
		timing_mr  = 2; /*memory dest   - register src*/
		timing_mm  = 2;
		timing_rml = 1; /*register dest - memory src long*/
		timing_mrl = 2; /*memory dest   - register src long*/
		timing_mml = 2;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 2; /*branch not taken*/
		timing_int_rm       = 9;
		timing_int_v86      = 46;
		timing_int_pm       = 21;
		timing_int_pm_outer = 32;
		timing_iret_rm       = 7;
		timing_iret_v86      = 26;
		timing_iret_pm       = 10;
		timing_iret_pm_outer = 26;
		timing_call_rm = 3;
		timing_call_pm = 4;
		timing_call_pm_gate = 15;
		timing_call_pm_gate_inner = 26;
		timing_retf_rm       = 4;
		timing_retf_pm       = 4;
		timing_retf_pm_outer = 23;
		timing_jmp_rm      = 1;
		timing_jmp_pm      = 4;
		timing_jmp_pm_gate = 14;
		timing_misaligned = 2;
		cpu_cyrix_alignment = 1;
		cpu_features = CPU_FEATURE_RDTSC;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
#ifdef USE_DYNAREC
		codegen_timing_set(&codegen_timing_686);
#endif
		CPUID = 0; /*Disabled on powerup by default*/
		break;

	case CPU_Cx6x86L:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
#else
		x86_setopcodes(ops_386, ops_pentium_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 1; /*register dest - memory src*/
		timing_mr  = 2; /*memory dest   - register src*/
		timing_mm  = 2;
		timing_rml = 1; /*register dest - memory src long*/
		timing_mrl = 2; /*memory dest   - register src long*/
		timing_mml = 2;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 2; /*branch not taken*/
		timing_int_rm       = 9;
		timing_int_v86      = 46;
		timing_int_pm       = 21;
		timing_int_pm_outer = 32;
		timing_iret_rm       = 7;
		timing_iret_v86      = 26;
		timing_iret_pm       = 10;
		timing_iret_pm_outer = 26;
		timing_call_rm = 3;
		timing_call_pm = 4;
		timing_call_pm_gate = 15;
		timing_call_pm_gate_inner = 26;
		timing_retf_rm       = 4;
		timing_retf_pm       = 4;
		timing_retf_pm_outer = 23;
		timing_jmp_rm      = 1;
		timing_jmp_pm      = 4;
		timing_jmp_pm_gate = 14;
		timing_misaligned = 2;
		cpu_cyrix_alignment = 1;
		cpu_features = CPU_FEATURE_RDTSC;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
#ifdef USE_DYNAREC
 		codegen_timing_set(&codegen_timing_686);
#endif
 		ccr4 = 0x80;
		break;

	case CPU_CxGX1:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_pentium_0f, dynarec_ops_386, dynarec_ops_pentium_0f);
#else
		x86_setopcodes(ops_386, ops_pentium_0f);
#endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 1; /*register dest - memory src*/
		timing_mr  = 2; /*memory dest   - register src*/
		timing_mm  = 2;
		timing_rml = 1; /*register dest - memory src long*/
		timing_mrl = 2; /*memory dest   - register src long*/
		timing_mml = 2;
		timing_bt  = 5-1; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_misaligned = 2;
		cpu_cyrix_alignment = 1;
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
		cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_PCE;
#ifdef USE_DYNAREC
 		codegen_timing_set(&codegen_timing_686);
#endif
		break;

	case CPU_Cx6x86MX:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_c6x86mx_0f, dynarec_ops_386, dynarec_ops_c6x86mx_0f);
		x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_686_da_a16;
		x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_686_da_a32;
		x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_686_db_a16;
		x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_686_db_a32;
		x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_686_df_a16;
		x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_686_df_a32;
#else
		x86_setopcodes(ops_386, ops_c6x86mx_0f);
#endif
		x86_opcodes_da_a16 = ops_fpu_686_da_a16;
		x86_opcodes_da_a32 = ops_fpu_686_da_a32;
		x86_opcodes_db_a16 = ops_fpu_686_db_a16;
		x86_opcodes_db_a32 = ops_fpu_686_db_a32;
		x86_opcodes_df_a16 = ops_fpu_686_df_a16;
		x86_opcodes_df_a32 = ops_fpu_686_df_a32;

		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 1; /*register dest - memory src*/
		timing_mr  = 2; /*memory dest   - register src*/
		timing_mm  = 2;
		timing_rml = 1; /*register dest - memory src long*/
		timing_mrl = 2; /*memory dest   - register src long*/
		timing_mml = 2;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 2; /*branch not taken*/
		timing_int_rm       = 9;
		timing_int_v86      = 46;
		timing_int_pm       = 21;
		timing_int_pm_outer = 32;
		timing_iret_rm       = 7;
		timing_iret_v86      = 26;
		timing_iret_pm       = 10;
		timing_iret_pm_outer = 26;
		timing_call_rm = 3;
		timing_call_pm = 4;
		timing_call_pm_gate = 15;
		timing_call_pm_gate_inner = 26;
		timing_retf_rm       = 4;
		timing_retf_pm       = 4;
		timing_retf_pm_outer = 23;
		timing_jmp_rm      = 1;
		timing_jmp_pm      = 4;
		timing_jmp_pm_gate = 14;
		timing_misaligned = 2;
		cpu_cyrix_alignment = 1;
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_MMX;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
		cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_PCE;
#ifdef USE_DYNAREC
 		codegen_timing_set(&codegen_timing_686);
#endif
 		ccr4 = 0x80;
		break;

#if defined(DEV_BRANCH) && defined(USE_AMD_K)
	case CPU_K5:
	case CPU_5K86:
# ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_k6_0f, dynarec_ops_386, dynarec_ops_k6_0f);
# else
		x86_setopcodes(ops_386, ops_k6_0f);
# endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 2; /*register dest - memory src*/
		timing_mr  = 3; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 2; /*register dest - memory src long*/
		timing_mrl = 3; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_misaligned = 3;
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME | CPU_FEATURE_MMX;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
		cpu_CR4_mask = CR4_TSD | CR4_DE | CR4_MCE | CR4_PCE;
		break;

	case CPU_K6:
# ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_k6_0f, dynarec_ops_386, dynarec_ops_k6_0f);
# else
		x86_setopcodes(ops_386, ops_k6_0f);
# endif
		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 2; /*register dest - memory src*/
		timing_mr  = 3; /*memory dest   - register src*/
		timing_mm  = 3;
		timing_rml = 2; /*register dest - memory src long*/
		timing_mrl = 3; /*memory dest   - register src long*/
		timing_mml = 3;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_misaligned = 3;
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME | CPU_FEATURE_MMX;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
		cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE | CR4_PCE;
# ifdef USE_DYNAREC
		codegen_timing_set(&codegen_timing_pentium);
# endif
		break;
#endif

	case CPU_PENTIUM_PRO:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_pentiumpro_0f, dynarec_ops_386, dynarec_ops_pentiumpro_0f);
		x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_686_da_a16;
		x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_686_da_a32;
		x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_686_db_a16;
		x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_686_db_a32;
		x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_686_df_a16;
		x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_686_df_a32;
#else
		x86_setopcodes(ops_386, ops_pentiumpro_0f);
#endif
		x86_opcodes_da_a16 = ops_fpu_686_da_a16;
		x86_opcodes_da_a32 = ops_fpu_686_da_a32;
		x86_opcodes_db_a16 = ops_fpu_686_db_a16;
		x86_opcodes_db_a32 = ops_fpu_686_db_a32;
		x86_opcodes_df_a16 = ops_fpu_686_df_a16;
		x86_opcodes_df_a32 = ops_fpu_686_df_a32;

		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 1; /*register dest - memory src*/
		timing_mr  = 1; /*memory dest   - register src*/
		timing_mm  = 1;
		timing_rml = 1; /*register dest - memory src long*/
		timing_mrl = 1; /*memory dest   - register src long*/
		timing_mml = 1;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_misaligned = 3;
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
		cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE | CR4_PCE;
#ifdef USE_DYNAREC
 		codegen_timing_set(&codegen_timing_686);
#endif
		break;

#if 1
	case CPU_PENTIUM_2:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_pentium2_0f, dynarec_ops_386, dynarec_ops_pentium2_0f);
		x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_686_da_a16;
		x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_686_da_a32;
		x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_686_db_a16;
		x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_686_db_a32;
		x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_686_df_a16;
		x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_686_df_a32;
#else
		x86_setopcodes(ops_386, ops_pentium2_0f);
#endif
		x86_opcodes_da_a16 = ops_fpu_686_da_a16;
		x86_opcodes_da_a32 = ops_fpu_686_da_a32;
		x86_opcodes_db_a16 = ops_fpu_686_db_a16;
		x86_opcodes_db_a32 = ops_fpu_686_db_a32;
		x86_opcodes_df_a16 = ops_fpu_686_df_a16;
		x86_opcodes_df_a32 = ops_fpu_686_df_a32;

		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 1; /*register dest - memory src*/
		timing_mr  = 1; /*memory dest   - register src*/
		timing_mm  = 1;
		timing_rml = 1; /*register dest - memory src long*/
		timing_mrl = 1; /*memory dest   - register src long*/
		timing_mml = 1;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_misaligned = 3;
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME | CPU_FEATURE_MMX;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
		cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE | CR4_PCE;
#ifdef USE_DYNAREC
 		codegen_timing_set(&codegen_timing_686);
#endif
		break;
#endif

	case CPU_PENTIUM_2D:
#ifdef USE_DYNAREC
		x86_setopcodes(ops_386, ops_pentium2d_0f, dynarec_ops_386, dynarec_ops_pentium2d_0f);
		x86_dynarec_opcodes_da_a16 = dynarec_ops_fpu_686_da_a16;
		x86_dynarec_opcodes_da_a32 = dynarec_ops_fpu_686_da_a32;
		x86_dynarec_opcodes_db_a16 = dynarec_ops_fpu_686_db_a16;
		x86_dynarec_opcodes_db_a32 = dynarec_ops_fpu_686_db_a32;
		x86_dynarec_opcodes_df_a16 = dynarec_ops_fpu_686_df_a16;
		x86_dynarec_opcodes_df_a32 = dynarec_ops_fpu_686_df_a32;
#else
		x86_setopcodes(ops_386, ops_pentium2d_0f);
#endif
		x86_opcodes_da_a16 = ops_fpu_686_da_a16;
		x86_opcodes_da_a32 = ops_fpu_686_da_a32;
		x86_opcodes_db_a16 = ops_fpu_686_db_a16;
		x86_opcodes_db_a32 = ops_fpu_686_db_a32;
		x86_opcodes_df_a16 = ops_fpu_686_df_a16;
		x86_opcodes_df_a32 = ops_fpu_686_df_a32;

		timing_rr  = 1; /*register dest - register src*/
		timing_rm  = 1; /*register dest - memory src*/
		timing_mr  = 1; /*memory dest   - register src*/
		timing_mm  = 1;
		timing_rml = 1; /*register dest - memory src long*/
		timing_mrl = 1; /*memory dest   - register src long*/
		timing_mml = 1;
		timing_bt  = 0; /*branch taken*/
		timing_bnt = 1; /*branch not taken*/
		timing_misaligned = 3;
		cpu_features = CPU_FEATURE_RDTSC | CPU_FEATURE_MSR | CPU_FEATURE_CR4 | CPU_FEATURE_VME | CPU_FEATURE_MMX;
		msr.fcr = (1 << 8) | (1 << 9) | (1 << 12) |  (1 << 16) | (1 << 19) | (1 << 21);
		cpu_CR4_mask = CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | CR4_MCE | CR4_PCE | CR4_OSFXSR;
#ifdef USE_DYNAREC
  		codegen_timing_set(&codegen_timing_686);
#endif
		break;

	default:
		fatal("CPU setup: unknown CPU type %i\n", cpu->type);
		/*NOTREACHED*/
    }
}


/*
 * Select the CPU to use.
 *
 * We get a pointer to the list of CPUs supported by the machine,
 * and an index to the currently-selected one. Although one usually
 * does not change a CPU on the fly (please, don't try this at home)
 * it is implemented this way because it currently is the only way
 * to change the operating speed of the selected CPU.
 *
 * We assume that the 'default' CPU we get called with, is the one
 * that runs at highest possible speed, and that index 0 will be
 * the lowest possible speed.
 *
 * In the future, this will be changed.
 */
void
cpu_set_type(const CPU *list, int manuf, int type, int fpu, int dyna)
{
    if (list == NULL) {
	fatal("CPU: invalid CPU list, aborting!\n");
	/*NOTREACHED*/
    }
    cpu_list = list;
    cpu_manuf = manuf;
    cpu_extfpu = fpu;
    cpu_dynarec = dyna;

    /* The 'turbo' speed is the one we got called with. */
    cpu_turbo = type;

    /* For now, the selected CPU/speed is the effective one. */
    cpu_effective = cpu_turbo;

    /* Activate the selected CPU. */
    cpu_activate();
}


/* Return the type of the currently-selected CPU. */
int
cpu_get_type(void)
{
    return(cpu->type);
}


/* Return the name of the currently-selected CPU. */
const char *
cpu_get_name(void)
{
    return(cpu->name);
}


/* Return the flags for the currently-selected CPU. */
int
cpu_get_flags(void)
{
    return(cpu->flags);
}


/* Return the operating speed of the currently-selected CPU. */
uint32_t
cpu_get_speed(void)
{
    return(cpu->rspeed);
}


/*
 * Select a new operating speed for the currently-selected CPU.
 *
 * As described above, for now this means actually selecting a
 * different CPU from the list, as the CPUs are listed by speed.
 *
 * This will change in the near future.
 */
int
cpu_set_speed(int new_speed)
{
    int old_speed = cpu_effective;

    if (cpu_effective == new_speed)
	return(old_speed);

    /* Update the effective CPU type. */
    cpu_effective = new_speed;

    /* Activate the selected CPU. */
    cpu_activate();

    return(old_speed);
}


void
cpu_update_waitstates(void)
{
    if (is486)
	cpu_prefetch_width = 16;
    else
	cpu_prefetch_width = cpu_16bitbus ? 2 : 4;

    if (cpu_cache_int_enabled) {
	/* Disable prefetch emulation */
	cpu_prefetch_cycles = 0;
    } else if (cpu_waitstates && (cpu->type >= CPU_286 && cpu->type <= CPU_386DX)) {
	/* Waitstates override */
	cpu_prefetch_cycles = cpu_waitstates+1;
	cpu_cycles_read = cpu_waitstates+1;
	cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * (cpu_waitstates+1);
	cpu_cycles_write = cpu_waitstates+1;
	cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * (cpu_waitstates+1);
    } else if (cpu_cache_ext_enabled) {
	/* Use cache timings */
	cpu_prefetch_cycles = cpu->cache_read_cycles;
	cpu_cycles_read = cpu->cache_read_cycles;
	cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * cpu->cache_read_cycles;
	cpu_cycles_write = cpu->cache_write_cycles;
	cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * cpu->cache_write_cycles;
    } else {
	/* Use memory timings */
	cpu_prefetch_cycles = cpu->mem_read_cycles;
	cpu_cycles_read = cpu->mem_read_cycles;
	cpu_cycles_read_l = (cpu_16bitbus ? 2 : 1) * cpu->mem_read_cycles;
	cpu_cycles_write = cpu->mem_write_cycles;
	cpu_cycles_write_l = (cpu_16bitbus ? 2 : 1) * cpu->mem_write_cycles;
    }

    if (is486)
	cpu_prefetch_cycles = (cpu_prefetch_cycles * 11) / 16;

    cpu_mem_prefetch_cycles = cpu_prefetch_cycles;

    if (cpu->rspeed <= 8000000)
	cpu_rom_prefetch_cycles = cpu_mem_prefetch_cycles;
}


void
cpu_set_edx(void)
{
    EDX = cpu->edx_reset;
}


char *
cpu_current_pc(char *bufp)
{
    static char buff[10];

    if (bufp == NULL)
	bufp = buff;

    sprintf(bufp, "%04X:%04X", CS, cpu_state.pc);

    return(bufp);
}


void
cpu_CPUID(void)
{
    switch (cpu->type) {
	case CPU_i486DX:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x756e6547;
			EDX = 0x49656e69;
			ECX = 0x6c65746e;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU; /*FPU*/
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_iDX4:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x756e6547;
			EDX = 0x49656e69;
			ECX = 0x6c65746e;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_VME;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_Am486SX:
		if (! EAX) {
			EAX = 1;
			EBX = 0x68747541;
			ECX = 0x444D4163;
			EDX = 0x69746E65;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = EDX = 0; /*No FPU*/
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_Am486DX:
		if (! EAX) {
			EAX = 1;
			EBX = 0x68747541;
			ECX = 0x444D4163;
			EDX = 0x69746E65;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU; /*FPU*/
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_WINCHIP:
		if (! EAX) {
			EAX = 1;
			if (msr.fcr2 & (1 << 14)) {
				EBX = msr.fcr3 >> 32;
				ECX = msr.fcr3 & 0xffffffff;
				EDX = msr.fcr2 >> 32;
			} else {
				EBX = 0x746e6543; /*CentaurHauls*/
				ECX = 0x736c7561;			
				EDX = 0x48727561;
			}
		} else if (EAX == 1) {
			EAX = 0x540;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR;
			if (cpu_has_feature(CPU_FEATURE_CX8))
				EDX |= CPUID_CMPXCHG8B;
			if (msr.fcr & (1 << 9))
				EDX |= CPUID_MMX;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;
	
	case CPU_WINCHIP2:
		switch(EAX) {
			case 0:
				EAX = 1;
				if (msr.fcr2 & (1 << 14)) {
					EBX = msr.fcr3 >> 32;
					ECX = msr.fcr3 & 0xffffffff;
					EDX = msr.fcr2 >> 32;
				} else {
					EBX = 0x746e6543; /*CentaurHauls*/
					ECX = 0x736c7561;			
					EDX = 0x48727561;
				}
				break;
			case 1:
				EAX = 0x580;
				EBX = ECX = 0;
				EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR;
				if (cpu_has_feature(CPU_FEATURE_CX8))
					EDX |= CPUID_CMPXCHG8B;
				if (msr.fcr & (1 << 9))
					EDX |= CPUID_MMX;
				break;
			case 0x80000000:
				EAX = 0x80000005;
				break;
			case 0x80000001:
				EAX = 0x58U;
				EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR;
				if (cpu_has_feature(CPU_FEATURE_CX8))
					EDX |= CPUID_CMPXCHG8B;
				if (msr.fcr & (1 << 9))
					EDX |= CPUID_3DNOW;
				break;

			case 0x80000002: /*Processor name string*/
				EAX = 0x20544449; /*IDT WinChip 2-3D*/
                EBX = 0x436e6957;
                ECX = 0x20706968;
                EDX = 0x44332d32;
				break;
			
			case 0x80000005: /*Cache information*/
                EBX = 0x08800880; /*TLBs*/
                ECX = 0x20040120; /*L1 data cache*/
                EDX = 0x20020120; /*L1 instruction cache*/
                break;

			default:
				EAX = EBX = ECX = EDX = 0;
			break;
		}
		break;

	case CPU_PENTIUM:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x756e6547;
			EDX = 0x49656e69;
			ECX = 0x6c65746e;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

#if defined(DEV_BRANCH) && defined(USE_AMD_K)
	case CPU_K5:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x68747541;
			EDX = 0x69746E65;
			ECX = 0x444D4163;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_5K86:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x68747541;
			EDX = 0x69746E65;
			ECX = 0x444D4163;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
		} else if (EAX == 0x80000000) {
			EAX = 0x80000005;
			EBX = ECX = EDX = 0;
		} else if (EAX == 0x80000001) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
		} else if (EAX == 0x80000002) {
			EAX = 0x2D444D41;
			EBX = 0x7428354B;
			ECX = 0x5020296D;
			EDX = 0x65636F72;
		} else if (EAX == 0x80000003) {
			EAX = 0x726F7373;
			EBX = ECX = EDX = 0;
		} else if (EAX == 0x80000004) {
			EAX = EBX = ECX = EDX = 0;
		} else if (EAX == 0x80000005) {
			EAX = 0;
			EBX = 0x04800000;
			ECX = 0x08040120;
			EDX = 0x10040120;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_K6:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x68747541;
			EDX = 0x69746E65;
			ECX = 0x444D4163;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
		} else if (EAX == 0x80000000) {
			EAX = 0x80000005;
			EBX = ECX = EDX = 0;
		} else if (EAX == 0x80000001) {
			EAX = CPUID + 0x100;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_AMDSEP;
		} else if (EAX == 0x80000002) {
			EAX = 0x2D444D41;
			EBX = 0x6D74364B;
			ECX = 0x202F7720;
			EDX = 0x746C756D;
		} else if (EAX == 0x80000003) {
			EAX = 0x64656D69;
			EBX = 0x65206169;
			ECX = 0x6E657478;
			EDX = 0x6E6F6973;
		} else if (EAX == 0x80000004) {
			EAX = 0x73;
			EBX = ECX = EDX = 0;
		} else if (EAX == 0x80000005) {
			EAX = 0;
			EBX = 0x02800140;
			ECX = 0x20020220;
			EDX = 0x20020220;
		} else if (EAX == 0x8FFFFFFF) {
			EAX = 0x4778654E;
			EBX = 0x72656E65;
			ECX = 0x6F697461;
			EDX = 0x444D416E;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;
#endif

	case CPU_PENTIUM_MMX:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x756e6547;
			EDX = 0x49656e69;
			ECX = 0x6c65746e;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_Cx6x86:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x69727943;
			EDX = 0x736e4978;
			ECX = 0x64616574;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_Cx6x86L:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x69727943;
			EDX = 0x736e4978;
			ECX = 0x64616574;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_CMPXCHG8B;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_CxGX1:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x69727943;
			EDX = 0x736e4978;
			ECX = 0x64616574;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_Cx6x86MX:
		if (! EAX) {
			EAX = 0x00000001;
			EBX = 0x69727943;
			EDX = 0x736e4978;
			ECX = 0x64616574;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_CMOV | CPUID_MMX;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

	case CPU_PENTIUM_PRO:
		if (! EAX) {
			EAX = 0x00000002;
			EBX = 0x756e6547;
			EDX = 0x49656e69;
			ECX = 0x6c65746e;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_SEP | CPUID_CMOV;
		} else if (EAX == 2) {
            EAX = 0x03020101;
            EBX = ECX = 0;
            EDX = 0x06040a42;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;

#if 1
	case CPU_PENTIUM_2:
		if (! EAX) {
			EAX = 0x00000002;
			EBX = 0x756e6547;
			EDX = 0x49656e69;
			ECX = 0x6c65746e;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX | CPUID_SEP | CPUID_CMOV;
		} else if (EAX == 2) {
			EAX = 0x03020101;
			EBX = ECX = 0;
			EDX = 0x08040C43;
		}
		else
			EAX = EBX = ECX = EDX = 0;
		break;
#endif

	case CPU_PENTIUM_2D:
		if (! EAX) {
			EAX = 0x00000002;
			EBX = 0x756e6547;
			EDX = 0x49656e69;
			ECX = 0x6c65746e;
		} else if (EAX == 1) {
			EAX = CPUID;
			EBX = ECX = 0;
			EDX = CPUID_FPU | CPUID_VME | CPUID_PSE | CPUID_TSC | CPUID_MSR | CPUID_CMPXCHG8B | CPUID_MMX | CPUID_SEP | CPUID_FXSR | CPUID_CMOV;
		} else if (EAX == 2) {
			EAX = 0x03020101;
			EBX = ECX = 0;
			EDX = 0x08040C44;
		} else
			EAX = EBX = ECX = EDX = 0;
		break;
    }
}


void
cpu_RDMSR(void)
{
    switch (cpu->type) {
	case CPU_WINCHIP: case CPU_WINCHIP2:
		EAX = EDX = 0;
		switch (ECX) {
			case 0x02:
				EAX = msr.tr1;
				break;

			case 0x0e:
				EAX = msr.tr12;
				break;

			case 0x10:
				EAX = tsc & 0xffffffff;
				EDX = tsc >> 32;
				break;

			case 0x11:
				EAX = msr.cesr;
				break;

			case 0x107:
				EAX = msr.fcr;
				break;

			case 0x108:
				EAX = msr.fcr2 & 0xffffffff;
				EDX = msr.fcr2 >> 32;
				break;

			case 0x10a:
				EAX = cpu->multi & 3;
				break;
		}
		break;

#if defined(DEV_BRANCH) && defined(USE_AMD_K)
	case CPU_K5:
	case CPU_5K86:
	case CPU_K6:
		EAX = EDX = 0;
		switch (ECX) {
			case 0x0e:
				EAX = msr.tr12;
				break;

			case 0x10:
				EAX = tsc & 0xffffffff;
				EDX = tsc >> 32;
				break;

			case 0x83:
				EAX = ecx83_msr & 0xffffffff;
				EDX = ecx83_msr >> 32;
				break;

			case 0xC0000081:
				EAX = star & 0xffffffff;
				EDX = star >> 32;
				break;

			case 0xC0000084:
				EAX = sfmask & 0xffffffff;
				EDX = sfmask >> 32;
				break;

			default:
				x86gpf(NULL, 0);
				break;
		}
		break;
#endif

	case CPU_PENTIUM:
	case CPU_PENTIUM_MMX:
		EAX = EDX = 0;
		switch (ECX) {
			case 0x10:
				EAX = tsc & 0xffffffff;
				EDX = tsc >> 32;
				break;
		}
		break;

	case CPU_Cx6x86:
	case CPU_Cx6x86L:
	case CPU_CxGX1:
	case CPU_Cx6x86MX:
		switch (ECX) {
			case 0x10:
				EAX = tsc & 0xffffffff;
				EDX = tsc >> 32;
				break;
		}
 		break;

	case CPU_PENTIUM_PRO:
	case CPU_PENTIUM_2:
	case CPU_PENTIUM_2D:
		EAX = EDX = 0;
		switch (ECX) {
			case 0x10:
				EAX = tsc & 0xffffffff;
				EDX = tsc >> 32;
				break;

			case 0x17:
				if (cpu->type != CPU_PENTIUM_2D)
					goto i686_invalid_rdmsr;
				EAX = ecx17_msr & 0xffffffff;
				EDX = ecx17_msr >> 32;
				break;

			case 0x1B:
				EAX = apic_base_msr & 0xffffffff;
				EDX = apic_base_msr >> 32;
				break;

			case 0x2A:
				EAX = 0xC5800000;
				EDX = 0;
				break;

			case 0x79:
				EAX = ecx79_msr & 0xffffffff;
				EDX = ecx79_msr >> 32;
				break;

			case 0x88: case 0x89: case 0x8A: case 0x8B:
				EAX = ecx8x_msr[ECX - 0x88] & 0xffffffff;
				EDX = ecx8x_msr[ECX - 0x88] >> 32;
				break;

			case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: case 0xC6: case 0xC7: case 0xC8:
				EAX = msr_ia32_pmc[ECX - 0xC1] & 0xffffffff;
				EDX = msr_ia32_pmc[ECX - 0xC1] >> 32;
				break;

			case 0xFE:
				EAX = mtrr_cap_msr & 0xffffffff;
				EDX = mtrr_cap_msr >> 32;
				break;

			case 0x116:
				EAX = ecx116_msr & 0xffffffff;
				EDX = ecx116_msr >> 32;
				break;

			case 0x118: case 0x119: case 0x11A: case 0x11B:
				EAX = ecx11x_msr[ECX - 0x118] & 0xffffffff;
				EDX = ecx11x_msr[ECX - 0x118] >> 32;
				break;

			case 0x11E:
				EAX = ecx11e_msr & 0xffffffff;
				EDX = ecx11e_msr >> 32;
				break;

			case 0x174:
				if (cpu->type == CPU_PENTIUM_PRO)
					goto i686_invalid_rdmsr;
				EAX &= 0xFFFF0000;
				EAX |= cs_msr;
				break;

			case 0x175:
				if (cpu->type == CPU_PENTIUM_PRO)
					goto i686_invalid_rdmsr;
				EAX = esp_msr;
				break;

			case 0x176:
				if (cpu->type == CPU_PENTIUM_PRO)
					goto i686_invalid_rdmsr;
				EAX = eip_msr;
				break;

			case 0x186:
				EAX = ecx186_msr & 0xffffffff;
				EDX = ecx186_msr >> 32;
				break;

			case 0x187:
				EAX = ecx187_msr & 0xffffffff;
				EDX = ecx187_msr >> 32;
				break;

			case 0x1E0:
				EAX = ecx1e0_msr & 0xffffffff;
				EDX = ecx1e0_msr >> 32;
				break;

			case 0x200: case 0x201: case 0x202: case 0x203: case 0x204: case 0x205: case 0x206: case 0x207:
			case 0x208: case 0x209: case 0x20A: case 0x20B: case 0x20C: case 0x20D: case 0x20E: case 0x20F:
				if (ECX & 1) {
					EAX = mtrr_physmask_msr[(ECX - 0x200) >> 1] & 0xffffffff;
					EDX = mtrr_physmask_msr[(ECX - 0x200) >> 1] >> 32;
				} else {
					EAX = mtrr_physbase_msr[(ECX - 0x200) >> 1] & 0xffffffff;
					EDX = mtrr_physbase_msr[(ECX - 0x200) >> 1] >> 32;
				}
				break;

			case 0x250:
				EAX = mtrr_fix64k_8000_msr & 0xffffffff;
				EDX = mtrr_fix64k_8000_msr >> 32;
				break;

			case 0x258:
				EAX = mtrr_fix16k_8000_msr & 0xffffffff;
				EDX = mtrr_fix16k_8000_msr >> 32;
				break;

			case 0x259:
				EAX = mtrr_fix16k_a000_msr & 0xffffffff;
				EDX = mtrr_fix16k_a000_msr >> 32;
				break;

			case 0x268: case 0x269: case 0x26A: case 0x26B: case 0x26C: case 0x26D: case 0x26E: case 0x26F:
				EAX = mtrr_fix4k_msr[ECX - 0x268] & 0xffffffff;
				EDX = mtrr_fix4k_msr[ECX - 0x268] >> 32;
				break;

			case 0x277:
				EAX = pat_msr & 0xffffffff;
				EDX = pat_msr >> 32;
				break;

			case 0x2FF:
				EAX = mtrr_deftype_msr & 0xffffffff;
				EDX = mtrr_deftype_msr >> 32;
				break;

			case 0x570:
				EAX = ecx570_msr & 0xffffffff;
				EDX = ecx570_msr >> 32;
				break;

			default:
i686_invalid_rdmsr:
				x86gpf(NULL, 0);
				break;
		}
		break;
    }
}


void
cpu_WRMSR(void)
{
    switch (cpu->type) {
	case CPU_WINCHIP: case CPU_WINCHIP2:
		switch (ECX) {
			case 0x02:
				msr.tr1 = EAX & 2;
				break;

			case 0x0e:
				msr.tr12 = EAX & 0x228;
				break;

			case 0x10:
				tsc = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x11:
				msr.cesr = EAX & 0xff00ff;
				break;

			case 0x107:
				msr.fcr = EAX;
				if (EAX & (1 << 9))
                	cpu_features |= CPU_FEATURE_MMX;
                else
                    cpu_features &= ~CPU_FEATURE_MMX;
				if (EAX & (1 << 1))
					cpu_features |= CPU_FEATURE_CX8;
				else
					cpu_features &= ~CPU_FEATURE_CX8;
				if ((EAX & (1 << 20)) && cpu->type >= CPU_WINCHIP2)
					cpu_features |= CPU_FEATURE_3DNOW;
				else
					cpu_features &= ~CPU_FEATURE_3DNOW;
				if (EAX & (1 << 29))
					CPUID = 0;
				else
					CPUID = cpu->cpuid_model;
				break;

			case 0x108:
				msr.fcr2 = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x109:
				msr.fcr3 = EAX | ((uint64_t)EDX << 32);
				break;
		}
		break;

#if defined(DEV_BRANCH) && defined(USE_AMD_K)
	case CPU_K5:
	case CPU_5K86:
	case CPU_K6:
		switch (ECX) {
			case 0x0e:
				msr.tr12 = EAX & 0x228;
				break;

			case 0x10:
				tsc = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x83:
				ecx83_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0xC0000081:
				star = EAX | ((uint64_t)EDX << 32);
				break;

			case 0xC0000084:
				sfmask = EAX | ((uint64_t)EDX << 32);
				break;
		}
		break;
#endif

	case CPU_PENTIUM:
	case CPU_PENTIUM_MMX:
		switch (ECX) {
			case 0x10:
				tsc = EAX | ((uint64_t)EDX << 32);
				break;
		}
		break;

	case CPU_Cx6x86:
	case CPU_Cx6x86L:
	case CPU_CxGX1:
	case CPU_Cx6x86MX:
		switch (ECX) {
			case 0x10:
				tsc = EAX | ((uint64_t)EDX << 32);
				break;
		}
		break;

	case CPU_PENTIUM_PRO:
	case CPU_PENTIUM_2:
	case CPU_PENTIUM_2D:
		switch (ECX) {
			case 0x10:
				tsc = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x17:
				if (cpu->type != CPU_PENTIUM_2D)
					goto i686_invalid_wrmsr;
				ecx17_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x1B:
				apic_base_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x79:
				ecx79_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x88: case 0x89: case 0x8A: case 0x8B:
				ecx8x_msr[ECX - 0x88] = EAX | ((uint64_t)EDX << 32);
				break;

			case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: case 0xC6: case 0xC7: case 0xC8:
				msr_ia32_pmc[ECX - 0xC1] = EAX | ((uint64_t)EDX << 32);
				break;

			case 0xFE:
				mtrr_cap_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x116:
				ecx116_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x118: case 0x119: case 0x11A: case 0x11B:
				ecx11x_msr[ECX - 0x118] = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x11E:
				ecx11e_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x174:
				if (cpu->type == CPU_PENTIUM_PRO)
					goto i686_invalid_wrmsr;
				cs_msr = EAX & 0xFFFF;
				break;

			case 0x175:
				if (cpu->type == CPU_PENTIUM_PRO)
					goto i686_invalid_wrmsr;
				esp_msr = EAX;
				break;

			case 0x176:
				if (cpu->type == CPU_PENTIUM_PRO)
					goto i686_invalid_wrmsr;
				eip_msr = EAX;
				break;

			case 0x186:
				ecx186_msr = EAX | ((uint64_t)EDX << 32);
				break;			

			case 0x187:
				ecx187_msr = EAX | ((uint64_t)EDX << 32);
				break;			

			case 0x1E0:
				ecx1e0_msr = EAX | ((uint64_t)EDX << 32);
				break;			

			case 0x200: case 0x201: case 0x202: case 0x203: case 0x204: case 0x205: case 0x206: case 0x207:
			case 0x208: case 0x209: case 0x20A: case 0x20B: case 0x20C: case 0x20D: case 0x20E: case 0x20F:
				if (ECX & 1)
					mtrr_physmask_msr[(ECX - 0x200) >> 1] = EAX | ((uint64_t)EDX << 32);
				else
					mtrr_physbase_msr[(ECX - 0x200) >> 1] = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x250:
				mtrr_fix64k_8000_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x258:
				mtrr_fix16k_8000_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x259:
				mtrr_fix16k_a000_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x268: case 0x269: case 0x26A: case 0x26B: case 0x26C: case 0x26D: case 0x26E: case 0x26F:
				mtrr_fix4k_msr[ECX - 0x268] = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x277:
				pat_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x2FF:
				mtrr_deftype_msr = EAX | ((uint64_t)EDX << 32);
				break;

			case 0x570:
				ecx570_msr = EAX | ((uint64_t)EDX << 32);
				break;			

			default:
i686_invalid_wrmsr:
				x86gpf(NULL, 0);
				break;
		}
		break;
    }
}


/* Run a block of code. */
void
cpu_exec(int slice)
{
    if (is386) {
#ifdef USE_DYNAREC
	if (cpu_dynarec)
		exec386_dynarec(cpu_speed/slice);
	  else
#endif
		exec386(cpu_speed/slice);
    } else if (cpu->type >= CPU_286) {
	exec386(cpu_speed/slice);
    } else {
	execx86(cpu_speed/slice);
    }
}


void
#ifdef USE_DYNAREC
x86_setopcodes(const OpFn *opcodes, const OpFn *opcodes_0f,
	       const OpFn *dynarec_opcodes, const OpFn *dynarec_opcodes_0f)
{
    x86_opcodes = opcodes;
    x86_opcodes_0f = opcodes_0f;
    x86_dynarec_opcodes = dynarec_opcodes;
    x86_dynarec_opcodes_0f = dynarec_opcodes_0f;
}
#else
x86_setopcodes(const OpFn *opcodes, const OpFn *opcodes_0f)
{
    x86_opcodes = opcodes;
    x86_opcodes_0f = opcodes_0f;
}
#endif
