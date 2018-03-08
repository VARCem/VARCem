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
 * Version:	@(#)x86_ops_mov_ctrl.h	1.0.1	2018/02/14
 *
 * Authors:	Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2008-2018 Sarah Walker.
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

static int opMOV_r_CRx_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from CRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_16(fetchdat);
        switch (cpu_reg)
        {
                case 0:
                cpu_state.regs[cpu_rm].l = cr0;
                if (is486)
                        cpu_state.regs[cpu_rm].l |= 0x10; /*ET hardwired on 486*/
                break;
                case 2:
                cpu_state.regs[cpu_rm].l = cr2;
                break;
                case 3:
                cpu_state.regs[cpu_rm].l = cr3;
                break;
                case 4:
                if (cpu_hasCR4)
                {
                        cpu_state.regs[cpu_rm].l = cr4;
                        break;
                }
                default:
                pclog("Bad read of CR%i %i\n",rmdat&7,cpu_reg);
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                break;
        }
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 0);
        return 0;
}
static int opMOV_r_CRx_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from CRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_32(fetchdat);
        switch (cpu_reg)
        {
                case 0:
                cpu_state.regs[cpu_rm].l = cr0;
                if (is486)
                        cpu_state.regs[cpu_rm].l |= 0x10; /*ET hardwired on 486*/
                break;
                case 2:
                cpu_state.regs[cpu_rm].l = cr2;
                break;
                case 3:
                cpu_state.regs[cpu_rm].l = cr3;
                break;
                case 4:
                if (cpu_hasCR4)
                {
                        cpu_state.regs[cpu_rm].l = cr4;
                        break;
                }
                default:
                pclog("Bad read of CR%i %i\n",rmdat&7,cpu_reg);
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                break;
        }
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 1);
        return 0;
}

static int opMOV_r_DRx_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from DRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_16(fetchdat);
        cpu_state.regs[cpu_rm].l = dr[cpu_reg];
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 0);
        return 0;
}
static int opMOV_r_DRx_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from DRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_32(fetchdat);
        cpu_state.regs[cpu_rm].l = dr[cpu_reg];
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 1);
        return 0;
}

static int opMOV_CRx_r_a16(uint32_t fetchdat)
{
        uint32_t old_cr0 = cr0;
        
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load CRx\n");
                x86gpf(NULL,0);
                return 1;
        }
        fetch_ea_16(fetchdat);
        switch (cpu_reg)
        {
                case 0:
                if ((cpu_state.regs[cpu_rm].l ^ cr0) & 0x80000001)
                        flushmmucache();
                cr0 = cpu_state.regs[cpu_rm].l;
                if (cpu_16bitbus)
                        cr0 |= 0x10;
                if (!(cr0 & 0x80000000))
                        mmu_perm=4;
                if (is486 && !(cr0 & (1 << 30)))
                        cpu_cache_int_enabled = 1;
                else
                        cpu_cache_int_enabled = 0;
                if (is486 && ((cr0 ^ old_cr0) & (1 << 30)))
                        cpu_update_waitstates();
                if (cr0 & 1)
                        cpu_cur_status |= CPU_STATUS_PMODE;
                else
                        cpu_cur_status &= ~CPU_STATUS_PMODE;
                break;
                case 2:
                cr2 = cpu_state.regs[cpu_rm].l;
                break;
                case 3:
                cr3 = cpu_state.regs[cpu_rm].l;
                flushmmucache();
                break;
                case 4:
                if (cpu_hasCR4)
                {
                        cr4 = cpu_state.regs[cpu_rm].l & cpu_CR4_mask;
                        break;
                }

                default:
                pclog("Bad load CR%i\n", cpu_reg);
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                break;
        }
        CLOCK_CYCLES(10);
        PREFETCH_RUN(10, 2, rmdat, 0,0,0,0, 0);
        return 0;
}
static int opMOV_CRx_r_a32(uint32_t fetchdat)
{
        uint32_t old_cr0 = cr0;
        
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load CRx\n");
                x86gpf(NULL,0);
                return 1;
        }
        fetch_ea_32(fetchdat);
        switch (cpu_reg)
        {
                case 0:
                if ((cpu_state.regs[cpu_rm].l ^ cr0) & 0x80000001)
                        flushmmucache();
                cr0 = cpu_state.regs[cpu_rm].l;
                if (cpu_16bitbus)
                        cr0 |= 0x10;
                if (!(cr0 & 0x80000000))
                        mmu_perm=4;
                if (is486 && !(cr0 & (1 << 30)))
                        cpu_cache_int_enabled = 1;
                else
                        cpu_cache_int_enabled = 0;
                if (is486 && ((cr0 ^ old_cr0) & (1 << 30)))
                        cpu_update_waitstates();
                if (cr0 & 1)
                        cpu_cur_status |= CPU_STATUS_PMODE;
                else
                        cpu_cur_status &= ~CPU_STATUS_PMODE;
                break;
                case 2:
                cr2 = cpu_state.regs[cpu_rm].l;
                break;
                case 3:
                cr3 = cpu_state.regs[cpu_rm].l;
                flushmmucache();
                break;
                case 4:
                if (cpu_hasCR4)
                {
                        cr4 = cpu_state.regs[cpu_rm].l & cpu_CR4_mask;
                        break;
                }

                default:
                pclog("Bad load CR%i\n", cpu_reg);
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                break;
        }
        CLOCK_CYCLES(10);
        PREFETCH_RUN(10, 2, rmdat, 0,0,0,0, 1);
        return 0;
}

static int opMOV_DRx_r_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load DRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_16(fetchdat);
        dr[cpu_reg] = cpu_state.regs[cpu_rm].l;
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 0);
        return 0;
}
static int opMOV_DRx_r_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load DRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_16(fetchdat);
        dr[cpu_reg] = cpu_state.regs[cpu_rm].l;
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 1);
        return 0;
}

static int opMOV_r_TRx_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from TRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_16(fetchdat);
        cpu_state.regs[cpu_rm].l = 0;
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 0);
        return 0;
}
static int opMOV_r_TRx_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load from TRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_32(fetchdat);
        cpu_state.regs[cpu_rm].l = 0;
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 1);
        return 0;
}

static int opMOV_TRx_r_a16(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load TRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_16(fetchdat);
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 0);
        return 0;
}
static int opMOV_TRx_r_a32(uint32_t fetchdat)
{
        if ((CPL || (eflags&VM_FLAG)) && (cr0&1))
        {
                pclog("Can't load TRx\n");
                x86gpf(NULL, 0);
                return 1;
        }
        fetch_ea_16(fetchdat);
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 2, rmdat, 0,0,0,0, 1);
        return 0;
}

