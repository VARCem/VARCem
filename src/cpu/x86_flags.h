/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Flag handling for the X86 architecture.
 *
 * Version:	@(#)x86_flags.h	1.0.1	2018/02/14
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

enum
{
        FLAGS_UNKNOWN,
        
        FLAGS_ZN8,
        FLAGS_ZN16,
        FLAGS_ZN32,
        
        FLAGS_ADD8,
        FLAGS_ADD16,
        FLAGS_ADD32,
        
        FLAGS_SUB8,
        FLAGS_SUB16,
        FLAGS_SUB32,
        
        FLAGS_SHL8,
        FLAGS_SHL16,
        FLAGS_SHL32,

        FLAGS_SHR8,
        FLAGS_SHR16,
        FLAGS_SHR32,

        FLAGS_SAR8,
        FLAGS_SAR16,
        FLAGS_SAR32,

        FLAGS_INC8,
        FLAGS_INC16,
        FLAGS_INC32,
        
        FLAGS_DEC8,
        FLAGS_DEC16,
        FLAGS_DEC32,
	
	FLAGS_ADC8,
	FLAGS_ADC16,
	FLAGS_ADC32,
	
	FLAGS_SBC8,
	FLAGS_SBC16,
	FLAGS_SBC32
};

static INLINE int ZF_SET()
{
        switch (cpu_state.flags_op)
        {
                case FLAGS_ZN8:   case FLAGS_ZN16:   case FLAGS_ZN32:
                case FLAGS_ADD8:  case FLAGS_ADD16:  case FLAGS_ADD32:
                case FLAGS_SUB8:  case FLAGS_SUB16:  case FLAGS_SUB32:
                case FLAGS_SHL8:  case FLAGS_SHL16:  case FLAGS_SHL32:
                case FLAGS_SHR8:  case FLAGS_SHR16:  case FLAGS_SHR32:
                case FLAGS_SAR8:  case FLAGS_SAR16:  case FLAGS_SAR32:
                case FLAGS_INC8:  case FLAGS_INC16:  case FLAGS_INC32:
                case FLAGS_DEC8:  case FLAGS_DEC16:  case FLAGS_DEC32:
		case FLAGS_ADC8:  case FLAGS_ADC16:  case FLAGS_ADC32:
		case FLAGS_SBC8:  case FLAGS_SBC16:  case FLAGS_SBC32:
                return !cpu_state.flags_res;
                
                case FLAGS_UNKNOWN:
                return flags & Z_FLAG;

		default:
		return 0;
        }
}

static INLINE int NF_SET()
{
        switch (cpu_state.flags_op)
        {
                case FLAGS_ZN8:   case FLAGS_ADD8:  case FLAGS_SUB8:
                case FLAGS_SHL8:  case FLAGS_SHR8:  case FLAGS_SAR8:
                case FLAGS_INC8:  case FLAGS_DEC8:  case FLAGS_ADC8:
		case FLAGS_SBC8:
                return cpu_state.flags_res & 0x80;
                
                case FLAGS_ZN16:  case FLAGS_ADD16: case FLAGS_SUB16:
                case FLAGS_SHL16: case FLAGS_SHR16: case FLAGS_SAR16:
                case FLAGS_INC16: case FLAGS_DEC16: case FLAGS_ADC16:
		case FLAGS_SBC16:
                return cpu_state.flags_res & 0x8000;
                
                case FLAGS_ZN32:  case FLAGS_ADD32: case FLAGS_SUB32:
                case FLAGS_SHL32: case FLAGS_SHR32: case FLAGS_SAR32:
                case FLAGS_INC32: case FLAGS_DEC32: case FLAGS_ADC32:
		case FLAGS_SBC32:
                return cpu_state.flags_res & 0x80000000;
                
                case FLAGS_UNKNOWN:
                return flags & N_FLAG;

		default:
		return 0;
        }
}

static INLINE int PF_SET()
{
        switch (cpu_state.flags_op)
        {
                case FLAGS_ZN8:   case FLAGS_ZN16:   case FLAGS_ZN32:
                case FLAGS_ADD8:  case FLAGS_ADD16:  case FLAGS_ADD32:
                case FLAGS_SUB8:  case FLAGS_SUB16:  case FLAGS_SUB32:
                case FLAGS_SHL8:  case FLAGS_SHL16:  case FLAGS_SHL32:
                case FLAGS_SHR8:  case FLAGS_SHR16:  case FLAGS_SHR32:
                case FLAGS_SAR8:  case FLAGS_SAR16:  case FLAGS_SAR32:
                case FLAGS_INC8:  case FLAGS_INC16:  case FLAGS_INC32:
                case FLAGS_DEC8:  case FLAGS_DEC16:  case FLAGS_DEC32:
		case FLAGS_ADC8:  case FLAGS_ADC16:  case FLAGS_ADC32:
		case FLAGS_SBC8:  case FLAGS_SBC16:  case FLAGS_SBC32:
                return znptable8[cpu_state.flags_res & 0xff] & P_FLAG;
                
                case FLAGS_UNKNOWN:
                return flags & P_FLAG;

		default:
		return 0;
        }
}

static INLINE int VF_SET()
{
        switch (cpu_state.flags_op)
        {
                case FLAGS_ZN8:  case FLAGS_ZN16:  case FLAGS_ZN32:
                case FLAGS_SAR8: case FLAGS_SAR16: case FLAGS_SAR32:
                return 0;
                
                case FLAGS_ADD8:  case FLAGS_INC8:  case FLAGS_ADC8:
                return !((cpu_state.flags_op1 ^ cpu_state.flags_op2) & 0x80) && ((cpu_state.flags_op1 ^ cpu_state.flags_res) & 0x80);
                case FLAGS_ADD16: case FLAGS_INC16: case FLAGS_ADC16:
                return !((cpu_state.flags_op1 ^ cpu_state.flags_op2) & 0x8000) && ((cpu_state.flags_op1 ^ cpu_state.flags_res) & 0x8000);
                case FLAGS_ADD32: case FLAGS_INC32: case FLAGS_ADC32:
                return !((cpu_state.flags_op1 ^ cpu_state.flags_op2) & 0x80000000) && ((cpu_state.flags_op1 ^ cpu_state.flags_res) & 0x80000000);
                                
                case FLAGS_SUB8:  case FLAGS_DEC8: case FLAGS_SBC8:
                return ((cpu_state.flags_op1 ^ cpu_state.flags_op2) & (cpu_state.flags_op1 ^ cpu_state.flags_res) & 0x80);
                case FLAGS_SUB16: case FLAGS_DEC16: case FLAGS_SBC16:
                return ((cpu_state.flags_op1 ^ cpu_state.flags_op2) & (cpu_state.flags_op1 ^ cpu_state.flags_res) & 0x8000);
                case FLAGS_SUB32: case FLAGS_DEC32: case FLAGS_SBC32:
                return ((cpu_state.flags_op1 ^ cpu_state.flags_op2) & (cpu_state.flags_op1 ^ cpu_state.flags_res) & 0x80000000);

                case FLAGS_SHL8:
                return (((cpu_state.flags_op1 << cpu_state.flags_op2) ^ (cpu_state.flags_op1 << (cpu_state.flags_op2 - 1))) & 0x80);
                case FLAGS_SHL16:
                return (((cpu_state.flags_op1 << cpu_state.flags_op2) ^ (cpu_state.flags_op1 << (cpu_state.flags_op2 - 1))) & 0x8000);
                case FLAGS_SHL32:
                return (((cpu_state.flags_op1 << cpu_state.flags_op2) ^ (cpu_state.flags_op1 << (cpu_state.flags_op2 - 1))) & 0x80000000);
                
                case FLAGS_SHR8:
                return ((cpu_state.flags_op2 == 1) && (cpu_state.flags_op1 & 0x80));
                case FLAGS_SHR16:
                return ((cpu_state.flags_op2 == 1) && (cpu_state.flags_op1 & 0x8000));
                case FLAGS_SHR32:
                return ((cpu_state.flags_op2 == 1) && (cpu_state.flags_op1 & 0x80000000));
                
                case FLAGS_UNKNOWN:
                return flags & V_FLAG;

		default:
		return 0;
        }
}

static INLINE int AF_SET()
{
        switch (cpu_state.flags_op)
        {
                case FLAGS_ZN8:  case FLAGS_ZN16:  case FLAGS_ZN32:
                case FLAGS_SHL8: case FLAGS_SHL16: case FLAGS_SHL32:
                case FLAGS_SHR8: case FLAGS_SHR16: case FLAGS_SHR32:
                case FLAGS_SAR8: case FLAGS_SAR16: case FLAGS_SAR32:
                return 0;
                
                case FLAGS_ADD8:   case FLAGS_ADD16:  case FLAGS_ADD32:
                case FLAGS_INC8:   case FLAGS_INC16:  case FLAGS_INC32:
                return ((cpu_state.flags_op1 & 0xF) + (cpu_state.flags_op2 & 0xF)) & 0x10;

                case FLAGS_ADC8:
		return ((cpu_state.flags_res & 0xf) < (cpu_state.flags_op1 & 0xf)) ||
			((cpu_state.flags_res & 0xf) == (cpu_state.flags_op1 & 0xf) && cpu_state.flags_op2 == 0xff);
		case FLAGS_ADC16:
		return ((cpu_state.flags_res & 0xf) < (cpu_state.flags_op1 & 0xf)) ||
			((cpu_state.flags_res & 0xf) == (cpu_state.flags_op1 & 0xf) && cpu_state.flags_op2 == 0xffff);
		case FLAGS_ADC32:
		return ((cpu_state.flags_res & 0xf) < (cpu_state.flags_op1 & 0xf)) ||
			((cpu_state.flags_res & 0xf) == (cpu_state.flags_op1 & 0xf) && cpu_state.flags_op2 == 0xffffffff);
		
		case FLAGS_SBC8:  case FLAGS_SBC16:  case FLAGS_SBC32:
		return ((cpu_state.flags_op1 & 0xf) < (cpu_state.flags_op2 & 0xf)) ||
			((cpu_state.flags_op1 & 0xf) == (cpu_state.flags_op2 & 0xf) && (cpu_state.flags_res & 0xf) != 0);
			
		case FLAGS_SUB8:  case FLAGS_SUB16:  case FLAGS_SUB32:
                case FLAGS_DEC8:  case FLAGS_DEC16:  case FLAGS_DEC32:
                return ((cpu_state.flags_op1 & 0xF) - (cpu_state.flags_op2 & 0xF)) & 0x10;
                
                case FLAGS_UNKNOWN:
                return flags & A_FLAG;

		default:
		return 0;
        }
}

static INLINE int CF_SET()
{
        switch (cpu_state.flags_op)
        {
                case FLAGS_ADD8:
                return (cpu_state.flags_op1 + cpu_state.flags_op2) & 0x100;
                case FLAGS_ADD16:
                return (cpu_state.flags_op1 + cpu_state.flags_op2) & 0x10000;
                case FLAGS_ADD32:
                return (cpu_state.flags_res < cpu_state.flags_op1);

                case FLAGS_ADC8:
		return (cpu_state.flags_res < cpu_state.flags_op1) ||
			(cpu_state.flags_res == cpu_state.flags_op1 && cpu_state.flags_op2 == 0xff);
                case FLAGS_ADC16:
		return (cpu_state.flags_res < cpu_state.flags_op1) ||
			(cpu_state.flags_res == cpu_state.flags_op1 && cpu_state.flags_op2 == 0xffff);
                case FLAGS_ADC32:
		return (cpu_state.flags_res < cpu_state.flags_op1) ||
			(cpu_state.flags_res == cpu_state.flags_op1 && cpu_state.flags_op2 == 0xffffffff);
			
		case FLAGS_SUB8:  case FLAGS_SUB16:  case FLAGS_SUB32:
                return (cpu_state.flags_op1 < cpu_state.flags_op2);
                
                case FLAGS_SBC8:  case FLAGS_SBC16:  case FLAGS_SBC32:
		return (cpu_state.flags_op1 < cpu_state.flags_op2) ||
			(cpu_state.flags_op1 == cpu_state.flags_op2 && cpu_state.flags_res != 0);
		
		case FLAGS_SHL8:
                return (cpu_state.flags_op1 << (cpu_state.flags_op2 - 1)) & 0x80;
                case FLAGS_SHL16:
                return (cpu_state.flags_op1 << (cpu_state.flags_op2 - 1)) & 0x8000;
                case FLAGS_SHL32:
                return (cpu_state.flags_op1 << (cpu_state.flags_op2 - 1)) & 0x80000000;

                case FLAGS_SHR8:
                case FLAGS_SHR16:
                case FLAGS_SHR32:
                return (cpu_state.flags_op1 >> (cpu_state.flags_op2 - 1)) & 1;

                case FLAGS_SAR8:
                return ((int8_t)cpu_state.flags_op1 >> (cpu_state.flags_op2 - 1)) & 1;
                case FLAGS_SAR16:
                return ((int16_t)cpu_state.flags_op1 >> (cpu_state.flags_op2 - 1)) & 1;
                case FLAGS_SAR32:
                return ((int32_t)cpu_state.flags_op1 >> (cpu_state.flags_op2 - 1)) & 1;

                case FLAGS_ZN8: 
                case FLAGS_ZN16:
                case FLAGS_ZN32:
                return 0;
                
                case FLAGS_DEC8:
                case FLAGS_DEC16:
                case FLAGS_DEC32:
                case FLAGS_INC8:
                case FLAGS_INC16:
                case FLAGS_INC32:
                case FLAGS_UNKNOWN:
                return flags & C_FLAG;

		default:
		return 0;
        }
}

static INLINE void flags_rebuild()
{
        if (cpu_state.flags_op != FLAGS_UNKNOWN)
        {
                uint16_t tempf = 0;
                if (CF_SET()) tempf |= C_FLAG;
                if (PF_SET()) tempf |= P_FLAG;
                if (AF_SET()) tempf |= A_FLAG;
                if (ZF_SET()) tempf |= Z_FLAG;                                
                if (NF_SET()) tempf |= N_FLAG;
                if (VF_SET()) tempf |= V_FLAG;
                flags = (flags & ~0x8d5) | tempf;
                cpu_state.flags_op = FLAGS_UNKNOWN;
        }
}

static INLINE void flags_extract()
{
        cpu_state.flags_op = FLAGS_UNKNOWN;
}

static INLINE void flags_rebuild_c()
{
        if (cpu_state.flags_op != FLAGS_UNKNOWN)
        {
                if (CF_SET())
                   flags |=  C_FLAG;
                else
                   flags &= ~C_FLAG;
        }                
}

static INLINE void setznp8(uint8_t val)
{
        cpu_state.flags_op = FLAGS_ZN8;
        cpu_state.flags_res = val;
}
static INLINE void setznp16(uint16_t val)
{
        cpu_state.flags_op = FLAGS_ZN16;
        cpu_state.flags_res = val;
}
static INLINE void setznp32(uint32_t val)
{
        cpu_state.flags_op = FLAGS_ZN32;
        cpu_state.flags_res = val;
}

#define set_flags_shift(op, orig, shift, res) \
        cpu_state.flags_op = op;                  \
        cpu_state.flags_res = res;                \
        cpu_state.flags_op1 = orig;               \
        cpu_state.flags_op2 = shift;

static INLINE void setadd8(uint8_t a, uint8_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = (a + b) & 0xff;
        cpu_state.flags_op = FLAGS_ADD8;
}
static INLINE void setadd16(uint16_t a, uint16_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = (a + b) & 0xffff;
        cpu_state.flags_op = FLAGS_ADD16;
}
static INLINE void setadd32(uint32_t a, uint32_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = a + b;
        cpu_state.flags_op = FLAGS_ADD32;
}
static INLINE void setadd8nc(uint8_t a, uint8_t b)
{
        flags_rebuild_c();
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = (a + b) & 0xff;
        cpu_state.flags_op = FLAGS_INC8;
}
static INLINE void setadd16nc(uint16_t a, uint16_t b)
{
        flags_rebuild_c();
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = (a + b) & 0xffff;
        cpu_state.flags_op = FLAGS_INC16;
}
static INLINE void setadd32nc(uint32_t a, uint32_t b)
{
        flags_rebuild_c();
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = a + b;
        cpu_state.flags_op = FLAGS_INC32;
}

static INLINE void setsub8(uint8_t a, uint8_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = (a - b) & 0xff;
        cpu_state.flags_op = FLAGS_SUB8;
}
static INLINE void setsub16(uint16_t a, uint16_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = (a - b) & 0xffff;
        cpu_state.flags_op = FLAGS_SUB16;
}
static INLINE void setsub32(uint32_t a, uint32_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = a - b;
        cpu_state.flags_op = FLAGS_SUB32;
}

static INLINE void setsub8nc(uint8_t a, uint8_t b)
{
        flags_rebuild_c();
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = (a - b) & 0xff;
        cpu_state.flags_op = FLAGS_DEC8;
}
static INLINE void setsub16nc(uint16_t a, uint16_t b)
{
        flags_rebuild_c();
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = (a - b) & 0xffff;
        cpu_state.flags_op = FLAGS_DEC16;
}
static INLINE void setsub32nc(uint32_t a, uint32_t b)
{
        flags_rebuild_c();
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
        cpu_state.flags_res = a - b;
        cpu_state.flags_op = FLAGS_DEC32;
}

static INLINE void setadc8(uint8_t a, uint8_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
	cpu_state.flags_res = (a + b + tempc) & 0xff;
        cpu_state.flags_op = FLAGS_ADC8;
}
static INLINE void setadc16(uint16_t a, uint16_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
	cpu_state.flags_res = (a + b + tempc) & 0xffff;
        cpu_state.flags_op = FLAGS_ADC16;
}
static INLINE void setadc32(uint32_t a, uint32_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
	cpu_state.flags_res = a + b + tempc;
        cpu_state.flags_op = FLAGS_ADC32;
}

static INLINE void setsbc8(uint8_t a, uint8_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
	cpu_state.flags_res = (a - (b + tempc)) & 0xff;
        cpu_state.flags_op = FLAGS_SBC8;
}
static INLINE void setsbc16(uint16_t a, uint16_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
	cpu_state.flags_res = (a - (b + tempc)) & 0xffff;
        cpu_state.flags_op = FLAGS_SBC16;
}
static INLINE void setsbc32(uint32_t a, uint32_t b)
{
        cpu_state.flags_op1 = a;
        cpu_state.flags_op2 = b;
	cpu_state.flags_res = a - (b + tempc);
        cpu_state.flags_op = FLAGS_SBC32;
}

