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
 * Version:	@(#)x86_ops_inc_dec.h	1.0.1	2018/02/14
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

#define INC_DEC_OP(name, reg, inc, setflags) \
        static int op ## name (uint32_t fetchdat)       \
        {                                               \
                setflags(reg, 1);                       \
                reg += inc;                             \
                CLOCK_CYCLES(timing_rr);                \
                PREFETCH_RUN(timing_rr, 1, -1, 0,0,0,0, 0); \
                return 0;                               \
        }

INC_DEC_OP(INC_AX, AX, 1, setadd16nc)
INC_DEC_OP(INC_BX, BX, 1, setadd16nc)
INC_DEC_OP(INC_CX, CX, 1, setadd16nc)
INC_DEC_OP(INC_DX, DX, 1, setadd16nc)
INC_DEC_OP(INC_SI, SI, 1, setadd16nc)
INC_DEC_OP(INC_DI, DI, 1, setadd16nc)
INC_DEC_OP(INC_BP, BP, 1, setadd16nc)
INC_DEC_OP(INC_SP, SP, 1, setadd16nc)

INC_DEC_OP(INC_EAX, EAX, 1, setadd32nc)
INC_DEC_OP(INC_EBX, EBX, 1, setadd32nc)
INC_DEC_OP(INC_ECX, ECX, 1, setadd32nc)
INC_DEC_OP(INC_EDX, EDX, 1, setadd32nc)
INC_DEC_OP(INC_ESI, ESI, 1, setadd32nc)
INC_DEC_OP(INC_EDI, EDI, 1, setadd32nc)
INC_DEC_OP(INC_EBP, EBP, 1, setadd32nc)
INC_DEC_OP(INC_ESP, ESP, 1, setadd32nc)

INC_DEC_OP(DEC_AX, AX, -1, setsub16nc)
INC_DEC_OP(DEC_BX, BX, -1, setsub16nc)
INC_DEC_OP(DEC_CX, CX, -1, setsub16nc)
INC_DEC_OP(DEC_DX, DX, -1, setsub16nc)
INC_DEC_OP(DEC_SI, SI, -1, setsub16nc)
INC_DEC_OP(DEC_DI, DI, -1, setsub16nc)
INC_DEC_OP(DEC_BP, BP, -1, setsub16nc)
INC_DEC_OP(DEC_SP, SP, -1, setsub16nc)

INC_DEC_OP(DEC_EAX, EAX, -1, setsub32nc)
INC_DEC_OP(DEC_EBX, EBX, -1, setsub32nc)
INC_DEC_OP(DEC_ECX, ECX, -1, setsub32nc)
INC_DEC_OP(DEC_EDX, EDX, -1, setsub32nc)
INC_DEC_OP(DEC_ESI, ESI, -1, setsub32nc)
INC_DEC_OP(DEC_EDI, EDI, -1, setsub32nc)
INC_DEC_OP(DEC_EBP, EBP, -1, setsub32nc)
INC_DEC_OP(DEC_ESP, ESP, -1, setsub32nc)


static int opINCDEC_b_a16(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_16(fetchdat);       
        temp=geteab();                  if (cpu_state.abrt) return 1;

        if (rmdat&0x38)
        {
                seteab(temp - 1);       if (cpu_state.abrt) return 1;
                setsub8nc(temp, 1);
        }
        else
        {
                seteab(temp + 1);       if (cpu_state.abrt) return 1;
                setadd8nc(temp, 1);
        }
        CLOCK_CYCLES((cpu_mod == 3) ? timing_rr : timing_mm);
        PREFETCH_RUN((cpu_mod == 3) ? timing_rr : timing_mm, 2, rmdat, (cpu_mod == 3) ? 0:1,0,(cpu_mod == 3) ? 0:1,0, 0);
        return 0;
}
static int opINCDEC_b_a32(uint32_t fetchdat)
{
        uint8_t temp;
        
        fetch_ea_32(fetchdat);       
        temp=geteab();                  if (cpu_state.abrt) return 1;

        if (rmdat&0x38)
        {
                seteab(temp - 1);       if (cpu_state.abrt) return 1;
                setsub8nc(temp, 1);
        }
        else
        {
                seteab(temp + 1);       if (cpu_state.abrt) return 1;
                setadd8nc(temp, 1);
        }
        CLOCK_CYCLES((cpu_mod == 3) ? timing_rr : timing_mm);
        PREFETCH_RUN((cpu_mod == 3) ? timing_rr : timing_mm, 2, rmdat, (cpu_mod == 3) ? 0:1,0,(cpu_mod == 3) ? 0:1,0, 1);
        return 0;
}
