/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Miscellaneous x86 CPU Instructions.
 *
 * Version:	@(#)x86_ops_prefix.h	1.0.2	2020/11/24
 *
 * Authors:	Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2008-2020 Sarah Walker.
 *		Copyright 2016-2018 Miran Grca.
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

#define op_seg(name, seg, opcode_table, normal_opcode_table)    \
static int op ## name ## _w_a16(uint32_t fetchdat)              \
{                                                               \
        fetchdat = fastreadl(cs + cpu_state.pc);                \
        if (cpu_state.abrt) return 1;                                     \
        cpu_state.pc++;                                         \
                                                                \
        cpu_state.ea_seg = &seg;                                          \
        cpu_state.ssegs = 1;                                              \
        CLOCK_CYCLES(4);                                        \
        PREFETCH_PREFIX();                                      \
                                                                \
        if (opcode_table[fetchdat & 0xff])                      \
                return opcode_table[fetchdat & 0xff](fetchdat >> 8);    \
        return normal_opcode_table[fetchdat & 0xff](fetchdat >> 8); \
}                                                               \
                                                                \
static int op ## name ## _l_a16(uint32_t fetchdat)              \
{                                                               \
        fetchdat = fastreadl(cs + cpu_state.pc);                \
        if (cpu_state.abrt) return 1;                                     \
        cpu_state.pc++;                                         \
                                                                \
        cpu_state.ea_seg = &seg;                                          \
        cpu_state.ssegs = 1;                                              \
        CLOCK_CYCLES(4);                                        \
        PREFETCH_PREFIX();                                      \
                                                                \
        if (opcode_table[(fetchdat & 0xff) | 0x100])            \
                return opcode_table[(fetchdat & 0xff) | 0x100](fetchdat >> 8);      \
        return normal_opcode_table[(fetchdat & 0xff) | 0x100](fetchdat >> 8); \
}                                                               \
                                                                \
static int op ## name ## _w_a32(uint32_t fetchdat)              \
{                                                               \
        fetchdat = fastreadl(cs + cpu_state.pc);                \
        if (cpu_state.abrt) return 1;                                     \
        cpu_state.pc++;                                         \
                                                                \
        cpu_state.ea_seg = &seg;                                          \
        cpu_state.ssegs = 1;                                              \
        CLOCK_CYCLES(4);                                        \
        PREFETCH_PREFIX();                                      \
                                                                \
        if (opcode_table[(fetchdat & 0xff) | 0x200])            \
                return opcode_table[(fetchdat & 0xff) | 0x200](fetchdat >> 8);      \
        return normal_opcode_table[(fetchdat & 0xff) | 0x200](fetchdat >> 8); \
}                                                               \
                                                                \
static int op ## name ## _l_a32(uint32_t fetchdat)              \
{                                                               \
        fetchdat = fastreadl(cs + cpu_state.pc);                \
        if (cpu_state.abrt) return 1;                                     \
        cpu_state.pc++;                                         \
                                                                \
        cpu_state.ea_seg = &seg;                                          \
        cpu_state.ssegs = 1;                                              \
        CLOCK_CYCLES(4);                                        \
        PREFETCH_PREFIX();                                      \
                                                                \
        if (opcode_table[(fetchdat & 0xff) | 0x300])            \
                return opcode_table[(fetchdat & 0xff) | 0x300](fetchdat >> 8);      \
        return normal_opcode_table[(fetchdat & 0xff) | 0x300](fetchdat >> 8); \
}

op_seg(CS, cpu_state.seg_cs, x86_opcodes, x86_opcodes)
op_seg(DS, cpu_state.seg_ds, x86_opcodes, x86_opcodes)
op_seg(ES, cpu_state.seg_es, x86_opcodes, x86_opcodes)
op_seg(FS, cpu_state.seg_fs, x86_opcodes, x86_opcodes)
op_seg(GS, cpu_state.seg_gs, x86_opcodes, x86_opcodes)
op_seg(SS, cpu_state.seg_ss, x86_opcodes, x86_opcodes)

op_seg(CS_REPE, cpu_state.seg_cs, x86_opcodes_REPE, x86_opcodes)
op_seg(DS_REPE, cpu_state.seg_ds, x86_opcodes_REPE, x86_opcodes)
op_seg(ES_REPE, cpu_state.seg_es, x86_opcodes_REPE, x86_opcodes)
op_seg(FS_REPE, cpu_state.seg_fs, x86_opcodes_REPE, x86_opcodes)
op_seg(GS_REPE, cpu_state.seg_gs, x86_opcodes_REPE, x86_opcodes)
op_seg(SS_REPE, cpu_state.seg_ss, x86_opcodes_REPE, x86_opcodes)

op_seg(CS_REPNE, cpu_state.seg_cs, x86_opcodes_REPNE, x86_opcodes)
op_seg(DS_REPNE, cpu_state.seg_ds, x86_opcodes_REPNE, x86_opcodes)
op_seg(ES_REPNE, cpu_state.seg_es, x86_opcodes_REPNE, x86_opcodes)
op_seg(FS_REPNE, cpu_state.seg_fs, x86_opcodes_REPNE, x86_opcodes)
op_seg(GS_REPNE, cpu_state.seg_gs, x86_opcodes_REPNE, x86_opcodes)
op_seg(SS_REPNE, cpu_state.seg_ss, x86_opcodes_REPNE, x86_opcodes)

static int op_66(uint32_t fetchdat) /*Data size select*/
{
        fetchdat = fastreadl(cs + cpu_state.pc);
        if (cpu_state.abrt) return 1;
        cpu_state.pc++;

        cpu_state.op32 = ((use32 & 0x100) ^ 0x100) | (cpu_state.op32 & 0x200);
        CLOCK_CYCLES(2);
        PREFETCH_PREFIX();
        return x86_opcodes[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);
}
static int op_67(uint32_t fetchdat) /*Address size select*/
{
        fetchdat = fastreadl(cs + cpu_state.pc);
        if (cpu_state.abrt) return 1;
        cpu_state.pc++;

        cpu_state.op32 = ((use32 & 0x200) ^ 0x200) | (cpu_state.op32 & 0x100);
        CLOCK_CYCLES(2);
        PREFETCH_PREFIX();
        return x86_opcodes[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);
}

static int op_66_REPE(uint32_t fetchdat) /*Data size select*/
{
        fetchdat = fastreadl(cs + cpu_state.pc);
        if (cpu_state.abrt) return 1;
        cpu_state.pc++;

        cpu_state.op32 = ((use32 & 0x100) ^ 0x100) | (cpu_state.op32 & 0x200);
        CLOCK_CYCLES(2);
        PREFETCH_PREFIX();
        if (x86_opcodes_REPE[(fetchdat & 0xff) | cpu_state.op32])
                return x86_opcodes_REPE[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);
        return x86_opcodes[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);        
}
static int op_67_REPE(uint32_t fetchdat) /*Address size select*/
{
        fetchdat = fastreadl(cs + cpu_state.pc);
        if (cpu_state.abrt) return 1;
        cpu_state.pc++;

        cpu_state.op32 = ((use32 & 0x200) ^ 0x200) | (cpu_state.op32 & 0x100);
        CLOCK_CYCLES(2);
        PREFETCH_PREFIX();
        if (x86_opcodes_REPE[(fetchdat & 0xff) | cpu_state.op32])
                return x86_opcodes_REPE[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);
        return x86_opcodes[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);        
}
static int op_66_REPNE(uint32_t fetchdat) /*Data size select*/
{
        fetchdat = fastreadl(cs + cpu_state.pc);
        if (cpu_state.abrt) return 1;
        cpu_state.pc++;

        cpu_state.op32 = ((use32 & 0x100) ^ 0x100) | (cpu_state.op32 & 0x200);
        CLOCK_CYCLES(2);
        PREFETCH_PREFIX();
        if (x86_opcodes_REPNE[(fetchdat & 0xff) | cpu_state.op32])
                return x86_opcodes_REPNE[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);
        return x86_opcodes[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);        
}
static int op_67_REPNE(uint32_t fetchdat) /*Address size select*/
{
        fetchdat = fastreadl(cs + cpu_state.pc);
        if (cpu_state.abrt) return 1;
        cpu_state.pc++;

        cpu_state.op32 = ((use32 & 0x200) ^ 0x200) | (cpu_state.op32 & 0x100);
        CLOCK_CYCLES(2);
        PREFETCH_PREFIX();
        if (x86_opcodes_REPNE[(fetchdat & 0xff) | cpu_state.op32])
                return x86_opcodes_REPNE[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);
        return x86_opcodes[(fetchdat & 0xff) | cpu_state.op32](fetchdat >> 8);        
}
