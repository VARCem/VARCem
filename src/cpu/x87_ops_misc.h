/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Miscellaneous x87 FPU Instructions.
 *
 * Version:	@(#)x87_ops_misc.h	1.0.4	2020/12/11
 *
 * Authors:	Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2008-2020 Sarah Walker.
 *		Copyright 2016-2020 Miran Grca.
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

static int opFSTSW_AX(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FSTSW\n");
        AX = cpu_state.npxs;
        CLOCK_CYCLES(3);
        return 0;
}



static int opFNOP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        CLOCK_CYCLES(4);
        return 0;
}

static int opFCLEX(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        cpu_state.npxs &= 0xff00;
        CLOCK_CYCLES(4);
        return 0;
}

static int opFINIT(uint32_t fetchdat)
{
	uint64_t *p;
        FP_ENTER();
        cpu_state.pc++;
        cpu_state.npxc = 0x37F;
        cpu_state.new_npxc = (cpu_state.old_npxc & ~0xc00);
        cpu_state.npxs = 0;
	p = (uint64_t *)cpu_state.tag;
        *p = 0x0303030303030303ll;
        cpu_state.TOP = 0;
	cpu_state.ismmx = 0;
        CLOCK_CYCLES(17);
	CPU_BLOCK_END();
        return 0;
}


static int opFFREE(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FFREE\n");
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = 3;
        CLOCK_CYCLES(3);
        return 0;
}

static int opFFREEP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FFREE\n");
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = 3; if (cpu_state.abrt) return 1;
        x87_pop();
        CLOCK_CYCLES(3);
        return 0;
}

static int opFST(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FST\n");
        ST(fetchdat & 7) = ST(0);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = cpu_state.tag[cpu_state.TOP & 7];
        CLOCK_CYCLES(3);
        return 0;
}

static int opFSTP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FSTP\n");
        ST(fetchdat & 7) = ST(0);
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = cpu_state.tag[cpu_state.TOP & 7];
        x87_pop();
        CLOCK_CYCLES(3);
        return 0;
}




static int FSTOR()
{
	uint64_t *p;
        FP_ENTER();
        switch ((cr0 & 1) | (cpu_state.op32 & 0x100))
        {
                case 0x000: /*16-bit real mode*/
                case 0x001: /*16-bit protected mode*/
                cpu_state.npxc = readmemw(easeg, cpu_state.eaaddr);
                cpu_state.new_npxc = (cpu_state.old_npxc & ~0xc00) | (cpu_state.npxc & 0xc00);
                cpu_state.npxs = readmemw(easeg, cpu_state.eaaddr+2);
                x87_settag(readmemw(easeg, cpu_state.eaaddr+4));
                cpu_state.TOP = (cpu_state.npxs >> 11) & 7;
                cpu_state.eaaddr += 14;
                break;
                case 0x100: /*32-bit real mode*/
                case 0x101: /*32-bit protected mode*/
                cpu_state.npxc = readmemw(easeg, cpu_state.eaaddr);
                cpu_state.new_npxc = (cpu_state.old_npxc & ~0xc00) | (cpu_state.npxc & 0xc00);
                cpu_state.npxs = readmemw(easeg, cpu_state.eaaddr+4);
                x87_settag(readmemw(easeg, cpu_state.eaaddr+8));
                cpu_state.TOP = (cpu_state.npxs >> 11) & 7;
                cpu_state.eaaddr += 28;
                break;
        }
        x87_ld_frstor(0); cpu_state.eaaddr += 10;
        x87_ld_frstor(1); cpu_state.eaaddr += 10;
        x87_ld_frstor(2); cpu_state.eaaddr += 10;
        x87_ld_frstor(3); cpu_state.eaaddr += 10;
        x87_ld_frstor(4); cpu_state.eaaddr += 10;
        x87_ld_frstor(5); cpu_state.eaaddr += 10;
        x87_ld_frstor(6); cpu_state.eaaddr += 10;
        x87_ld_frstor(7);
        
        cpu_state.ismmx = 0;
        /*Horrible hack, but as PCem doesn't keep the FPU stack in 80-bit precision at all times
          something like this is needed*/
	p = (uint64_t *)cpu_state.tag;
        if (cpu_state.MM_w4[0] == 0xffff && cpu_state.MM_w4[1] == 0xffff && cpu_state.MM_w4[2] == 0xffff && cpu_state.MM_w4[3] == 0xffff &&
            cpu_state.MM_w4[4] == 0xffff && cpu_state.MM_w4[5] == 0xffff && cpu_state.MM_w4[6] == 0xffff && cpu_state.MM_w4[7] == 0xffff &&
            !cpu_state.TOP && !(*p))
        cpu_state.ismmx = 1;

        CLOCK_CYCLES((cr0 & 1) ? 34 : 44);
        if (fplog) DEBUG("FRSTOR %08X:%08X %i %i %04X\n", easeg, cpu_state.eaaddr, cpu_state.ismmx, cpu_state.TOP, x87_gettag());
        return cpu_state.abrt;
}
static int opFSTOR_a16(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_16(fetchdat);
        SEG_CHECK_READ(cpu_state.ea_seg);
        FSTOR();
        return cpu_state.abrt;
}
static int opFSTOR_a32(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_32(fetchdat);
        SEG_CHECK_READ(cpu_state.ea_seg);
        FSTOR();
        return cpu_state.abrt;
}

static int FSAVE()
{
	uint64_t *p;

        FP_ENTER();
        if (fplog) DEBUG("FSAVE %08X:%08X %i\n", easeg, cpu_state.eaaddr, cpu_state.ismmx);
        cpu_state.npxs = (cpu_state.npxs & ~(7 << 11)) | (cpu_state.TOP << 11);

        switch ((cr0 & 1) | (cpu_state.op32 & 0x100))
        {
                case 0x000: /*16-bit real mode*/
                writememw(easeg,cpu_state.eaaddr,cpu_state.npxc);
                writememw(easeg,cpu_state.eaaddr+2,cpu_state.npxs);
                writememw(easeg,cpu_state.eaaddr+4,x87_gettag());
                writememw(easeg,cpu_state.eaaddr+6,x87_pc_off);
                writememw(easeg,cpu_state.eaaddr+10,x87_op_off);
                cpu_state.eaaddr+=14;
                if (cpu_state.ismmx)
                {
                        x87_stmmx(cpu_state.MM[0]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[1]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[2]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[3]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[4]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[5]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[6]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[7]);
                }
                else
                {
                        x87_st_fsave(0); cpu_state.eaaddr+=10;
                        x87_st_fsave(1); cpu_state.eaaddr+=10;
                        x87_st_fsave(2); cpu_state.eaaddr+=10;
                        x87_st_fsave(3); cpu_state.eaaddr+=10;
                        x87_st_fsave(4); cpu_state.eaaddr+=10;
                        x87_st_fsave(5); cpu_state.eaaddr+=10;
                        x87_st_fsave(6); cpu_state.eaaddr+=10;
                        x87_st_fsave(7);
                }
                break;
                case 0x001: /*16-bit protected mode*/
                writememw(easeg,cpu_state.eaaddr,cpu_state.npxc);
                writememw(easeg,cpu_state.eaaddr+2,cpu_state.npxs);
                writememw(easeg,cpu_state.eaaddr+4,x87_gettag());
                writememw(easeg,cpu_state.eaaddr+6,x87_pc_off);
                writememw(easeg,cpu_state.eaaddr+8,x87_pc_seg);
                writememw(easeg,cpu_state.eaaddr+10,x87_op_off);
                writememw(easeg,cpu_state.eaaddr+12,x87_op_seg);
                cpu_state.eaaddr+=14;
                if (cpu_state.ismmx)
                {
                        x87_stmmx(cpu_state.MM[0]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[1]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[2]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[3]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[4]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[5]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[6]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[7]);
                }
                else
                {
                        x87_st_fsave(0); cpu_state.eaaddr+=10;
                        x87_st_fsave(1); cpu_state.eaaddr+=10;
                        x87_st_fsave(2); cpu_state.eaaddr+=10;
                        x87_st_fsave(3); cpu_state.eaaddr+=10;
                        x87_st_fsave(4); cpu_state.eaaddr+=10;
                        x87_st_fsave(5); cpu_state.eaaddr+=10;
                        x87_st_fsave(6); cpu_state.eaaddr+=10;
                        x87_st_fsave(7);
                }
                break;
                case 0x100: /*32-bit real mode*/
                writememw(easeg,cpu_state.eaaddr,cpu_state.npxc);
                writememw(easeg,cpu_state.eaaddr+4,cpu_state.npxs);
                writememw(easeg,cpu_state.eaaddr+8,x87_gettag());
                writememw(easeg,cpu_state.eaaddr+12,x87_pc_off);
                writememw(easeg,cpu_state.eaaddr+20,x87_op_off);
                writememl(easeg,cpu_state.eaaddr+24,(x87_op_off>>16)<<12);
                cpu_state.eaaddr+=28;
                if (cpu_state.ismmx)
                {
                        x87_stmmx(cpu_state.MM[0]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[1]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[2]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[3]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[4]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[5]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[6]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[7]);
                }
                else
                {
                        x87_st_fsave(0); cpu_state.eaaddr+=10;
                        x87_st_fsave(1); cpu_state.eaaddr+=10;
                        x87_st_fsave(2); cpu_state.eaaddr+=10;
                        x87_st_fsave(3); cpu_state.eaaddr+=10;
                        x87_st_fsave(4); cpu_state.eaaddr+=10;
                        x87_st_fsave(5); cpu_state.eaaddr+=10;
                        x87_st_fsave(6); cpu_state.eaaddr+=10;
                        x87_st_fsave(7);
                }
                break;
                case 0x101: /*32-bit protected mode*/
                writememw(easeg,cpu_state.eaaddr,cpu_state.npxc);
                writememw(easeg,cpu_state.eaaddr+4,cpu_state.npxs);
                writememw(easeg,cpu_state.eaaddr+8,x87_gettag());
                writememl(easeg,cpu_state.eaaddr+12,x87_pc_off);
                writememl(easeg,cpu_state.eaaddr+16,x87_pc_seg);
                writememl(easeg,cpu_state.eaaddr+20,x87_op_off);
                writememl(easeg,cpu_state.eaaddr+24,x87_op_seg);
                cpu_state.eaaddr+=28;
                if (cpu_state.ismmx)
                {
                        x87_stmmx(cpu_state.MM[0]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[1]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[2]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[3]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[4]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[5]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[6]); cpu_state.eaaddr+=10;
                        x87_stmmx(cpu_state.MM[7]);
                }
                else
                {
                        x87_st_fsave(0); cpu_state.eaaddr+=10;
                        x87_st_fsave(1); cpu_state.eaaddr+=10;
                        x87_st_fsave(2); cpu_state.eaaddr+=10;
                        x87_st_fsave(3); cpu_state.eaaddr+=10;
                        x87_st_fsave(4); cpu_state.eaaddr+=10;
                        x87_st_fsave(5); cpu_state.eaaddr+=10;
                        x87_st_fsave(6); cpu_state.eaaddr+=10;
                        x87_st_fsave(7);
                }
                break;
        }

        cpu_state.npxc = 0x37F;
        cpu_state.new_npxc = (cpu_state.old_npxc & ~0xc00);
        cpu_state.npxs = 0;
	p = (uint64_t *)cpu_state.tag;
        *p = 0x0303030303030303ll;
        cpu_state.TOP = 0;
        cpu_state.ismmx = 0;

        CLOCK_CYCLES((cr0 & 1) ? 56 : 67);
        return cpu_state.abrt;
}
static int opFSAVE_a16(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_16(fetchdat);
        SEG_CHECK_WRITE(cpu_state.ea_seg);
        FSAVE();
        return cpu_state.abrt;
}
static int opFSAVE_a32(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_32(fetchdat);
        SEG_CHECK_WRITE(cpu_state.ea_seg);
        FSAVE();
        return cpu_state.abrt;
}

static int opFSTSW_a16(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_16(fetchdat);
        SEG_CHECK_WRITE(cpu_state.ea_seg);
        if (fplog) DEBUG("FSTSW %08X:%08X\n", easeg, cpu_state.eaaddr);
        seteaw((cpu_state.npxs & 0xC7FF) | (cpu_state.TOP << 11));
        CLOCK_CYCLES(3);
        return cpu_state.abrt;
}
static int opFSTSW_a32(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_32(fetchdat);
        SEG_CHECK_WRITE(cpu_state.ea_seg);
        if (fplog) DEBUG("FSTSW %08X:%08X\n", easeg, cpu_state.eaaddr);
        seteaw((cpu_state.npxs & 0xC7FF) | (cpu_state.TOP << 11));
        CLOCK_CYCLES(3);
        return cpu_state.abrt;
}


static int opFLD(uint32_t fetchdat)
{
        int old_tag;
        uint64_t old_i64;
        
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FLD %f\n", ST(fetchdat & 7));
        old_tag = cpu_state.tag[(cpu_state.TOP + fetchdat) & 7];
        old_i64 = cpu_state.MM[(cpu_state.TOP + fetchdat) & 7].q;
        x87_push(ST(fetchdat&7));
        cpu_state.tag[cpu_state.TOP] = old_tag;
        cpu_state.MM[cpu_state.TOP].q = old_i64;
        CLOCK_CYCLES(4);
        return 0;
}

static int opFXCH(uint32_t fetchdat)
{
        double td;
        uint8_t old_tag;
        uint64_t old_i64;
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FXCH\n");
        td = ST(0);
        ST(0) = ST(fetchdat&7);
        ST(fetchdat&7) = td;
        old_tag = cpu_state.tag[cpu_state.TOP];
        cpu_state.tag[cpu_state.TOP] = cpu_state.tag[(cpu_state.TOP + fetchdat) & 7];
        cpu_state.tag[(cpu_state.TOP + fetchdat) & 7] = old_tag;
        old_i64 = cpu_state.MM[cpu_state.TOP].q;
        cpu_state.MM[cpu_state.TOP].q = cpu_state.MM[(cpu_state.TOP + fetchdat) & 7].q;
        cpu_state.MM[(cpu_state.TOP + fetchdat) & 7].q = old_i64;
        
        CLOCK_CYCLES(4);
        return 0;
}

static int opFCHS(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FCHS\n");
        ST(0) = -ST(0);
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        CLOCK_CYCLES(6);
        return 0;
}

static int opFABS(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FABS %f\n", ST(0));
        ST(0) = fabs(ST(0));
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        CLOCK_CYCLES(3);
        return 0;
}

static int opFTST(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FTST\n");
        cpu_state.npxs &= ~(C0|C2|C3);
        if (ST(0) == 0.0)     cpu_state.npxs |= C3;
        else if (ST(0) < 0.0) cpu_state.npxs |= C0;
        CLOCK_CYCLES(4);
        return 0;
}

static int opFXAM(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FXAM %i %f\n", cpu_state.tag[cpu_state.TOP&7], ST(0));
        cpu_state.npxs &= ~(C0|C1|C2|C3);
        if (cpu_state.tag[cpu_state.TOP&7] == 3)   cpu_state.npxs |= (C0|C3);
        else if (ST(0) == 0.0) cpu_state.npxs |= C3;
        else                   cpu_state.npxs |= C2;
        if (ST(0) < 0.0)       cpu_state.npxs |= C1;
        CLOCK_CYCLES(8);
        return 0;
}

static int opFLD1(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FLD1\n");
        x87_push(1.0);
        CLOCK_CYCLES(4);
        return 0;
}

static int opFLDL2T(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FLDL2T\n");
        x87_push(3.3219280948873623);
        CLOCK_CYCLES(8);
        return 0;
}

static int opFLDL2E(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FLDL2E\n");
        x87_push(1.4426950408889634);
        CLOCK_CYCLES(8);
        return 0;
}

static int opFLDPI(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FLDPI\n");
        x87_push(3.141592653589793);
        CLOCK_CYCLES(8);
        return 0;
}

static int opFLDEG2(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FLDEG2\n");
        x87_push(0.3010299956639812);
        CLOCK_CYCLES(8);
        return 0;
}

static int opFLDLN2(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FLDLN2\n");
        x87_push_u64(0x3fe62e42fefa39f0ULL);
        CLOCK_CYCLES(8);
        return 0;
}
 
static int opFLDZ(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FLDZ\n");
        x87_push(0.0);
        cpu_state.tag[cpu_state.TOP&7] = 1;
        CLOCK_CYCLES(4);
        return 0;
}

static int opF2XM1(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("F2XM1\n");
        ST(0) = pow(2.0, ST(0)) - 1.0;
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        CLOCK_CYCLES(200);
        return 0;
}

static int opFYL2X(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FYL2X\n");
        ST(1) = ST(1) * (log(ST(0)) / log(2.0));
        cpu_state.tag[(cpu_state.TOP + 1) & 7] &= ~TAG_UINT64;
        x87_pop();
        CLOCK_CYCLES(250);
        return 0;
}

static int opFYL2XP1(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FYL2XP1\n");
        ST(1) = ST(1) * (log1p(ST(0)) / log(2.0));
        cpu_state.tag[(cpu_state.TOP + 1) & 7] &= ~TAG_UINT64;
        x87_pop();
        CLOCK_CYCLES(250);
        return 0;
}

static int opFPTAN(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FPTAN\n");
        ST(0) = tan(ST(0));
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        x87_push(1.0);
        cpu_state.npxs &= ~C2;
        CLOCK_CYCLES(235);
        return 0;
}

static int opFPATAN(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FPATAN\n");
        ST(1) = atan2(ST(1), ST(0));
        cpu_state.tag[(cpu_state.TOP + 1) & 7] &= ~TAG_UINT64;
        x87_pop();
        CLOCK_CYCLES(250);
        return 0;
}

static int opFDECSTP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FDECSTP\n");
        cpu_state.TOP = (cpu_state.TOP - 1) & 7;
        CLOCK_CYCLES(4);
        return 0;
}

static int opFINCSTP(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FDECSTP\n");
        cpu_state.TOP = (cpu_state.TOP + 1) & 7;
        CLOCK_CYCLES(4);
        return 0;
}

static int opFPREM(uint32_t fetchdat)
{
        int64_t temp64;
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FPREM %f %f  ", ST(0), ST(1));
        temp64 = (int64_t)(ST(0) / ST(1));
        ST(0) = ST(0) - (ST(1) * (double)temp64);
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        if (fplog) DEBUG("%f\n", ST(0));
        cpu_state.npxs &= ~(C0|C1|C2|C3);
        if (temp64 & 4) cpu_state.npxs|=C0;
        if (temp64 & 2) cpu_state.npxs|=C3;
        if (temp64 & 1) cpu_state.npxs|=C1;
        CLOCK_CYCLES(100);
        return 0;
}
static int opFPREM1(uint32_t fetchdat)
{
        int64_t temp64;
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FPREM1 %f %f  ", ST(0), ST(1));
        temp64 = (int64_t)(ST(0) / ST(1));
        ST(0) = ST(0) - (ST(1) * (double)temp64);
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        if (fplog) DEBUG("%f\n", ST(0));
        cpu_state.npxs &= ~(C0|C1|C2|C3);
        if (temp64 & 4) cpu_state.npxs|=C0;
        if (temp64 & 2) cpu_state.npxs|=C3;
        if (temp64 & 1) cpu_state.npxs|=C1;
        CLOCK_CYCLES(100);
        return 0;
}

static int opFSQRT(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FSQRT\n");
        ST(0) = sqrt(ST(0));
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        CLOCK_CYCLES(83);
        return 0;
}

static int opFSINCOS(uint32_t fetchdat)
{
        double td;
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FSINCOS\n");
        td = ST(0);
        ST(0) = sin(td);
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        x87_push(cos(td));
        cpu_state.npxs &= ~C2;
        CLOCK_CYCLES(330);
        return 0;
}

static int opFRNDINT(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FRNDINT %g ", ST(0));
        ST(0) = (double)x87_fround(ST(0));
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        if (fplog) DEBUG("%g\n", ST(0));
        CLOCK_CYCLES(21);
        return 0;
}

static int opFSCALE(uint32_t fetchdat)
{
        int64_t temp64;
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FSCALE\n");
        temp64 = (int64_t)ST(1);
        ST(0) = ST(0) * pow(2.0, (double)temp64);
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        CLOCK_CYCLES(30);
        return 0;
}

static int opFSIN(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FSIN\n");
        ST(0) = sin(ST(0));
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        cpu_state.npxs &= ~C2;
        CLOCK_CYCLES(300);
        return 0;
}

static int opFCOS(uint32_t fetchdat)
{
        FP_ENTER();
        cpu_state.pc++;
        if (fplog) DEBUG("FCOS\n");
        ST(0) = cos(ST(0));
        cpu_state.tag[cpu_state.TOP] &= ~TAG_UINT64;
        cpu_state.npxs &= ~C2;
        CLOCK_CYCLES(300);
        return 0;
}


static int FLDENV()
{
        FP_ENTER();
        if (fplog) DEBUG("FLDENV %08X:%08X\n", easeg, cpu_state.eaaddr);
        switch ((cr0 & 1) | (cpu_state.op32 & 0x100))
        {
                case 0x000: /*16-bit real mode*/
                case 0x001: /*16-bit protected mode*/
                cpu_state.npxc = readmemw(easeg, cpu_state.eaaddr);
                cpu_state.new_npxc = (cpu_state.old_npxc & ~0xc00) | (cpu_state.npxc & 0xc00);
                cpu_state.npxs = readmemw(easeg, cpu_state.eaaddr+2);
                x87_settag(readmemw(easeg, cpu_state.eaaddr+4));
                cpu_state.TOP = (cpu_state.npxs >> 11) & 7;
                break;
                case 0x100: /*32-bit real mode*/
                case 0x101: /*32-bit protected mode*/
                cpu_state.npxc = readmemw(easeg, cpu_state.eaaddr);
                cpu_state.new_npxc = (cpu_state.old_npxc & ~0xc00) | (cpu_state.npxc & 0xc00);
                cpu_state.npxs = readmemw(easeg, cpu_state.eaaddr+4);
                x87_settag(readmemw(easeg, cpu_state.eaaddr+8));
                cpu_state.TOP = (cpu_state.npxs >> 11) & 7;
                break;
        }
        CLOCK_CYCLES((cr0 & 1) ? 34 : 44);
        return cpu_state.abrt;
}

static int opFLDENV_a16(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_16(fetchdat);
        SEG_CHECK_READ(cpu_state.ea_seg);
        FLDENV();
        return cpu_state.abrt;
}
static int opFLDENV_a32(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_32(fetchdat);
        SEG_CHECK_READ(cpu_state.ea_seg);
        FLDENV();
        return cpu_state.abrt;
}

static int opFLDCW_a16(uint32_t fetchdat)
{
        uint16_t tempw;
        FP_ENTER();
        fetch_ea_16(fetchdat);
        SEG_CHECK_READ(cpu_state.ea_seg);
        if (fplog) DEBUG("FLDCW %08X:%08X\n", easeg, cpu_state.eaaddr);                        
        tempw = geteaw();
        if (cpu_state.abrt) return 1;
        cpu_state.npxc = tempw;
        cpu_state.new_npxc = (cpu_state.old_npxc & ~0xc00) | (cpu_state.npxc & 0xc00);
        CLOCK_CYCLES(4);
        return 0;
}
static int opFLDCW_a32(uint32_t fetchdat)
{
        uint16_t tempw;
        FP_ENTER();
        fetch_ea_32(fetchdat);
        SEG_CHECK_READ(cpu_state.ea_seg);
        if (fplog) DEBUG("FLDCW %08X:%08X\n", easeg, cpu_state.eaaddr);                        
        tempw = geteaw();
        if (cpu_state.abrt) return 1;
        cpu_state.npxc = tempw;
        cpu_state.new_npxc = (cpu_state.old_npxc & ~0xc00) | (cpu_state.npxc & 0xc00);
        CLOCK_CYCLES(4);
        return 0;
}

static int FSTENV()
{
        FP_ENTER();
        if (fplog) DEBUG("FSTENV %08X:%08X\n", easeg, cpu_state.eaaddr);
        switch ((cr0 & 1) | (cpu_state.op32 & 0x100))
        {
                case 0x000: /*16-bit real mode*/
                writememw(easeg,cpu_state.eaaddr,cpu_state.npxc);
                writememw(easeg,cpu_state.eaaddr+2,cpu_state.npxs);
                writememw(easeg,cpu_state.eaaddr+4,x87_gettag());
                writememw(easeg,cpu_state.eaaddr+6,x87_pc_off);
                writememw(easeg,cpu_state.eaaddr+10,x87_op_off);
                break;
                case 0x001: /*16-bit protected mode*/
                writememw(easeg,cpu_state.eaaddr,cpu_state.npxc);
                writememw(easeg,cpu_state.eaaddr+2,cpu_state.npxs);
                writememw(easeg,cpu_state.eaaddr+4,x87_gettag());
                writememw(easeg,cpu_state.eaaddr+6,x87_pc_off);
                writememw(easeg,cpu_state.eaaddr+8,x87_pc_seg);
                writememw(easeg,cpu_state.eaaddr+10,x87_op_off);
                writememw(easeg,cpu_state.eaaddr+12,x87_op_seg);
                break;
                case 0x100: /*32-bit real mode*/
                writememw(easeg,cpu_state.eaaddr,cpu_state.npxc);
                writememw(easeg,cpu_state.eaaddr+4,cpu_state.npxs);
                writememw(easeg,cpu_state.eaaddr+8,x87_gettag());
                writememw(easeg,cpu_state.eaaddr+12,x87_pc_off);
                writememw(easeg,cpu_state.eaaddr+20,x87_op_off);
                writememl(easeg,cpu_state.eaaddr+24,(x87_op_off>>16)<<12);
                break;
                case 0x101: /*32-bit protected mode*/
                writememw(easeg,cpu_state.eaaddr,cpu_state.npxc);
                writememw(easeg,cpu_state.eaaddr+4,cpu_state.npxs);
                writememw(easeg,cpu_state.eaaddr+8,x87_gettag());
                writememl(easeg,cpu_state.eaaddr+12,x87_pc_off);
                writememl(easeg,cpu_state.eaaddr+16,x87_pc_seg);
                writememl(easeg,cpu_state.eaaddr+20,x87_op_off);
                writememl(easeg,cpu_state.eaaddr+24,x87_op_seg);
                break;
        }
        CLOCK_CYCLES((cr0 & 1) ? 56 : 67);
        return cpu_state.abrt;
}

static int opFSTENV_a16(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_16(fetchdat);
        SEG_CHECK_WRITE(cpu_state.ea_seg);
        FSTENV();
        return cpu_state.abrt;
}
static int opFSTENV_a32(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_32(fetchdat);
        SEG_CHECK_WRITE(cpu_state.ea_seg);
        FSTENV();
        return cpu_state.abrt;
}

static int opFSTCW_a16(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_16(fetchdat);
        SEG_CHECK_WRITE(cpu_state.ea_seg);
        if (fplog) DEBUG("FSTCW %08X:%08X\n", easeg, cpu_state.eaaddr);
        seteaw(cpu_state.npxc);
        CLOCK_CYCLES(3);
        return cpu_state.abrt;
}
static int opFSTCW_a32(uint32_t fetchdat)
{
        FP_ENTER();
        fetch_ea_32(fetchdat);
        SEG_CHECK_WRITE(cpu_state.ea_seg);
        if (fplog) DEBUG("FSTCW %08X:%08X\n", easeg, cpu_state.eaaddr);
        seteaw(cpu_state.npxc);
        CLOCK_CYCLES(3);
        return cpu_state.abrt;
}

#define opFCMOV(condition)                                                              \
        static int opFCMOV ## condition(uint32_t fetchdat)                              \
        {                                                                               \
                FP_ENTER();                                                             \
                cpu_state.pc++;                                                         \
                if (fplog) DEBUG("FCMOV %f\n", ST(fetchdat & 7));                       \
                if (cond_ ## condition)                                                 \
                {                                                                       \
                        cpu_state.tag[cpu_state.TOP] = cpu_state.tag[(cpu_state.TOP + fetchdat) & 7];                           \
                        cpu_state.MM[cpu_state.TOP].q = cpu_state.MM[(cpu_state.TOP + fetchdat) & 7].q;                     \
                        ST(0) = ST(fetchdat & 7);                                       \
                }                                                                       \
                CLOCK_CYCLES(4);                                                        \
                return 0;                                                               \
        }

#define cond_U   ( PF_SET())
#define cond_NU  (!PF_SET())

opFCMOV(B)
opFCMOV(E)
opFCMOV(BE)
opFCMOV(U)
opFCMOV(NB)
opFCMOV(NE)
opFCMOV(NBE)
opFCMOV(NU)
