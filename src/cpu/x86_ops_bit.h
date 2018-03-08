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
 * Version:	@(#)x86_ops_bit.h	1.0.1	2018/02/14
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

static int opBT_w_r_a16(uint32_t fetchdat)
{
        uint16_t temp;
        
        fetch_ea_16(fetchdat);
        cpu_state.eaaddr += ((cpu_state.regs[cpu_reg].w / 16) * 2);     eal_r = 0;
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        flags_rebuild();
        if (temp & (1 << (cpu_state.regs[cpu_reg].w & 15))) flags |=  C_FLAG;
        else                                            flags &= ~C_FLAG;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, 1,0,0,0, 0);
        return 0;
}
static int opBT_w_r_a32(uint32_t fetchdat)
{
        uint16_t temp;
        
        fetch_ea_32(fetchdat);
        cpu_state.eaaddr += ((cpu_state.regs[cpu_reg].w / 16) * 2);     eal_r = 0;
        temp = geteaw();                        if (cpu_state.abrt) return 1;
        flags_rebuild();
        if (temp & (1 << (cpu_state.regs[cpu_reg].w & 15))) flags |=  C_FLAG;
        else                                            flags &= ~C_FLAG;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, 1,0,0,0, 1);
        return 0;
}
static int opBT_l_r_a16(uint32_t fetchdat)
{
        uint32_t temp;
        
        fetch_ea_16(fetchdat);
        cpu_state.eaaddr += ((cpu_state.regs[cpu_reg].l / 32) * 4);     eal_r = 0;
        temp = geteal();                        if (cpu_state.abrt) return 1;
        flags_rebuild();
        if (temp & (1 << (cpu_state.regs[cpu_reg].l & 31))) flags |=  C_FLAG;
        else                                            flags &= ~C_FLAG;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, 0,1,0,0, 0);
        return 0;
}
static int opBT_l_r_a32(uint32_t fetchdat)
{
        uint32_t temp;
        
        fetch_ea_32(fetchdat);
        cpu_state.eaaddr += ((cpu_state.regs[cpu_reg].l / 32) * 4);     eal_r = 0;
        temp = geteal();                        if (cpu_state.abrt) return 1;
        flags_rebuild();
        if (temp & (1 << (cpu_state.regs[cpu_reg].l & 31))) flags |=  C_FLAG;
        else                                            flags &= ~C_FLAG;
        
        CLOCK_CYCLES(3);
        PREFETCH_RUN(3, 2, rmdat, 0,1,0,0, 1);
        return 0;
}

#define opBT(name, operation)                                                   \
        static int opBT ## name ## _w_r_a16(uint32_t fetchdat)                  \
        {                                                                       \
                int tempc;                                                      \
                uint16_t temp;                                                  \
                                                                                \
                fetch_ea_16(fetchdat);                                          \
                cpu_state.eaaddr += ((cpu_state.regs[cpu_reg].w / 16) * 2);     eal_r = eal_w = 0;      \
                temp = geteaw();                        if (cpu_state.abrt) return 1;     \
                tempc = (temp & (1 << (cpu_state.regs[cpu_reg].w & 15))) ? 1 : 0;   \
                temp operation (1 << (cpu_state.regs[cpu_reg].w & 15));             \
                seteaw(temp);                           if (cpu_state.abrt) return 1;     \
                flags_rebuild();                                                \
                if (tempc) flags |=  C_FLAG;                                    \
                else       flags &= ~C_FLAG;                                    \
                                                                                \
                CLOCK_CYCLES(6);                                                \
                PREFETCH_RUN(6, 2, rmdat, 1,0,1,0, 0);                          \
                return 0;                                                       \
        }                                                                       \
        static int opBT ## name ## _w_r_a32(uint32_t fetchdat)                  \
        {                                                                       \
                int tempc;                                                      \
                uint16_t temp;                                                  \
                                                                                \
                fetch_ea_32(fetchdat);                                          \
                cpu_state.eaaddr += ((cpu_state.regs[cpu_reg].w / 16) * 2);     eal_r = eal_w = 0;      \
                temp = geteaw();                        if (cpu_state.abrt) return 1;     \
                tempc = (temp & (1 << (cpu_state.regs[cpu_reg].w & 15))) ? 1 : 0;   \
                temp operation (1 << (cpu_state.regs[cpu_reg].w & 15));             \
                seteaw(temp);                           if (cpu_state.abrt) return 1;     \
                flags_rebuild();                                                \
                if (tempc) flags |=  C_FLAG;                                    \
                else       flags &= ~C_FLAG;                                    \
                                                                                \
                CLOCK_CYCLES(6);                                                \
                PREFETCH_RUN(6, 2, rmdat, 1,0,1,0, 1);                          \
                return 0;                                                       \
        }                                                                       \
        static int opBT ## name ## _l_r_a16(uint32_t fetchdat)                  \
        {                                                                       \
                int tempc;                                                      \
                uint32_t temp;                                                  \
                                                                                \
                fetch_ea_16(fetchdat);                                          \
                cpu_state.eaaddr += ((cpu_state.regs[cpu_reg].l / 32) * 4);     eal_r = eal_w = 0;      \
                temp = geteal();                        if (cpu_state.abrt) return 1;     \
                tempc = (temp & (1 << (cpu_state.regs[cpu_reg].l & 31))) ? 1 : 0;   \
                temp operation (1 << (cpu_state.regs[cpu_reg].l & 31));             \
                seteal(temp);                           if (cpu_state.abrt) return 1;     \
                flags_rebuild();                                                \
                if (tempc) flags |=  C_FLAG;                                    \
                else       flags &= ~C_FLAG;                                    \
                                                                                \
                CLOCK_CYCLES(6);                                                \
                PREFETCH_RUN(6, 2, rmdat, 0,1,0,1, 0);                          \
                return 0;                                                       \
        }                                                                       \
        static int opBT ## name ## _l_r_a32(uint32_t fetchdat)                  \
        {                                                                       \
                int tempc;                                                      \
                uint32_t temp;                                                  \
                                                                                \
                fetch_ea_32(fetchdat);                                          \
                cpu_state.eaaddr += ((cpu_state.regs[cpu_reg].l / 32) * 4);     eal_r = eal_w = 0;      \
                temp = geteal();                        if (cpu_state.abrt) return 1;     \
                tempc = (temp & (1 << (cpu_state.regs[cpu_reg].l & 31))) ? 1 : 0;   \
                temp operation (1 << (cpu_state.regs[cpu_reg].l & 31));             \
                seteal(temp);                           if (cpu_state.abrt) return 1;     \
                flags_rebuild();                                                \
                if (tempc) flags |=  C_FLAG;                                    \
                else       flags &= ~C_FLAG;                                    \
                                                                                \
                CLOCK_CYCLES(6);                                                \
                PREFETCH_RUN(6, 2, rmdat, 0,1,0,1, 1);                          \
                return 0;                                                       \
        }

opBT(C, ^=)
opBT(R, &=~)
opBT(S, |=)

static int opBA_w_a16(uint32_t fetchdat)
{
        int tempc, count;
        uint16_t temp;

        fetch_ea_16(fetchdat);
        
        temp = geteaw();
        count = getbyte();                      if (cpu_state.abrt) return 1;
        tempc = temp & (1 << count);
        flags_rebuild();
        switch (rmdat & 0x38)
        {
                case 0x20: /*BT w,imm*/
                if (tempc) flags |=  C_FLAG;
                else       flags &= ~C_FLAG;
                CLOCK_CYCLES(3);
                PREFETCH_RUN(3, 3, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 0);
                return 0;
                case 0x28: /*BTS w,imm*/
                temp |=  (1 << count);
                break;
                case 0x30: /*BTR w,imm*/
                temp &= ~(1 << count);
                break;
                case 0x38: /*BTC w,imm*/
                temp ^=  (1 << count);
                break;

                default:
                pclog("Bad 0F BA opcode %02X\n", rmdat & 0x38);
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                break;
        }
        seteaw(temp);                           if (cpu_state.abrt) return 1;
        if (tempc) flags |=  C_FLAG;
        else       flags &= ~C_FLAG;
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 3, rmdat, (cpu_mod == 3) ? 0:1,0,(cpu_mod == 3) ? 0:1,0, 0);
        return 0;
}
static int opBA_w_a32(uint32_t fetchdat)
{
        int tempc, count;
        uint16_t temp;

        fetch_ea_32(fetchdat);
        
        temp = geteaw();
        count = getbyte();                      if (cpu_state.abrt) return 1;
        tempc = temp & (1 << count);
        flags_rebuild();
        switch (rmdat & 0x38)
        {
                case 0x20: /*BT w,imm*/
                if (tempc) flags |=  C_FLAG;
                else       flags &= ~C_FLAG;
                CLOCK_CYCLES(3);
                PREFETCH_RUN(3, 3, rmdat, (cpu_mod == 3) ? 0:1,0,0,0, 1);
                return 0;
                case 0x28: /*BTS w,imm*/
                temp |=  (1 << count);
                break;
                case 0x30: /*BTR w,imm*/
                temp &= ~(1 << count);
                break;
                case 0x38: /*BTC w,imm*/
                temp ^=  (1 << count);
                break;

                default:
                pclog("Bad 0F BA opcode %02X\n", rmdat & 0x38);
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                break;
        }
        seteaw(temp);                           if (cpu_state.abrt) return 1;
        if (tempc) flags |=  C_FLAG;
        else       flags &= ~C_FLAG;
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 3, rmdat, (cpu_mod == 3) ? 0:1,0,(cpu_mod == 3) ? 0:1,0, 0);
        return 0;
}

static int opBA_l_a16(uint32_t fetchdat)
{
        int tempc, count;
        uint32_t temp;

        fetch_ea_16(fetchdat);
        
        temp = geteal();
        count = getbyte();                      if (cpu_state.abrt) return 1;
        tempc = temp & (1 << count);
        flags_rebuild();
        switch (rmdat & 0x38)
        {
                case 0x20: /*BT w,imm*/
                if (tempc) flags |=  C_FLAG;
                else       flags &= ~C_FLAG;
                CLOCK_CYCLES(3);
                PREFETCH_RUN(3, 3, rmdat, 0,(cpu_mod == 3) ? 0:1,0,0, 0);
                return 0;
                case 0x28: /*BTS w,imm*/
                temp |=  (1 << count);
                break;
                case 0x30: /*BTR w,imm*/
                temp &= ~(1 << count);
                break;
                case 0x38: /*BTC w,imm*/
                temp ^=  (1 << count);
                break;

                default:
                pclog("Bad 0F BA opcode %02X\n", rmdat & 0x38);
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                break;
        }
        seteal(temp);                           if (cpu_state.abrt) return 1;
        if (tempc) flags |=  C_FLAG;
        else       flags &= ~C_FLAG;
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 3, rmdat, 0,(cpu_mod == 3) ? 0:1,0,(cpu_mod == 3) ? 0:1, 0);
        return 0;
}
static int opBA_l_a32(uint32_t fetchdat)
{
        int tempc, count;
        uint32_t temp;

        fetch_ea_32(fetchdat);
        
        temp = geteal();
        count = getbyte();                      if (cpu_state.abrt) return 1;
        tempc = temp & (1 << count);
        flags_rebuild();
        switch (rmdat & 0x38)
        {
                case 0x20: /*BT w,imm*/
                if (tempc) flags |=  C_FLAG;
                else       flags &= ~C_FLAG;
                CLOCK_CYCLES(3);
                PREFETCH_RUN(3, 3, rmdat, 0,(cpu_mod == 3) ? 0:1,0,0, 1);
                return 0;
                case 0x28: /*BTS w,imm*/
                temp |=  (1 << count);
                break;
                case 0x30: /*BTR w,imm*/
                temp &= ~(1 << count);
                break;
                case 0x38: /*BTC w,imm*/
                temp ^=  (1 << count);
                break;

                default:
                pclog("Bad 0F BA opcode %02X\n", rmdat & 0x38);
                cpu_state.pc = cpu_state.oldpc;
                x86illegal();
                break;
        }
        seteal(temp);                           if (cpu_state.abrt) return 1;
        if (tempc) flags |=  C_FLAG;
        else       flags &= ~C_FLAG;
        CLOCK_CYCLES(6);
        PREFETCH_RUN(6, 3, rmdat, 0,(cpu_mod == 3) ? 0:1,0,(cpu_mod == 3) ? 0:1, 1);
        return 0;
}
