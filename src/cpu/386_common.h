/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Common 386 CPU code.
 *
 * Version:	@(#)386_common.h	1.0.6	2020/11/18
 *
 * Authors:	Sarah Walker, <tommowalker@tommowalker.co.uk>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2008-2020 Sarah Walker.
 *		Copyright 2016-2019 Miran Grca.
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
extern uint16_t ea_rseg;


#undef readmemb
#define readmemb(s,a) ((readlookup2[(uint32_t)((s)+(a))>>12]==(uintptr_t)-1 || \
		       (s)==0xFFFFFFFF) \
		? readmemb386l(s,a) \
		: *(uint8_t *)(readlookup2[(uint32_t)((s)+(a))>>12] + (uint32_t)((s) + (a))) )
#define readmemq(s,a) ((readlookup2[(uint32_t)((s)+(a))>>12]==(uintptr_t)-1 || \
		       (s)==0xFFFFFFFF || (((s)+(a)) & 7)) \
		? readmemql(s,a) \
		: *(uint64_t *)(readlookup2[(uint32_t)((s)+(a))>>12]+(uint32_t)((s)+(a))))

#undef writememb
#define writememb(s,a,v) if (writelookup2[(uint32_t)((s)+(a))>>12]==(uintptr_t)-1 || (s)==0xFFFFFFFF) writememb386l(s,a,v); else *(uint8_t *)(writelookup2[(uint32_t)((s) + (a)) >> 12] + (uint32_t)((s) + (a))) = v

#define writememw(s,a,v) if (writelookup2[(uint32_t)((s)+(a))>>12]==(uintptr_t)-1 || (s)==0xFFFFFFFF || (((s)+(a)) & 1)) writememwl(s,a,v); else *(uint16_t *)(writelookup2[(uint32_t)((s) + (a)) >> 12] + (uint32_t)((s) + (a))) = v
#define writememl(s,a,v) if (writelookup2[(uint32_t)((s)+(a))>>12]==(uintptr_t)-1 || (s)==0xFFFFFFFF || (((s)+(a)) & 3)) writememll(s,a,v); else *(uint32_t *)(writelookup2[(uint32_t)((s) + (a)) >> 12] + (uint32_t)((s) + (a))) = v
#define writememq(s,a,v) if (writelookup2[(uint32_t)((s)+(a))>>12]==(uintptr_t)-1 || (s)==0xFFFFFFFF || (((s)+(a)) & 7)) writememql(s,a,v); else *(uint64_t *)(writelookup2[(uint32_t)((s) + (a)) >> 12] + (uint32_t)((s) + (a))) = v


#define check_io_perm(port) \
		if (msw&1 && ((CPL > IOPL) || (eflags&VM_FLAG))) \
                { \
                        int tempi = checkio(port); \
                        if (cpu_state.abrt) return 1; \
                        if (tempi) \
                        { \
                                x86gpf("check_io_perm(): no permission",0); \
                                return 1; \
                        } \
                }

#define SEG_CHECK_READ(seg)                             \
        do                                              \
        {                                               \
                if ((seg)->base == 0xffffffff)          \
                {                                       \
                        x86gpf("Segment can't read", 0);\
                        return 1;                       \
                }                                       \
        } while (0)

#define SEG_CHECK_WRITE(seg)                    \
        do                                              \
        {                                               \
                if ((seg)->base == 0xffffffff)          \
                {                                       \
                        x86gpf("Segment can't write", 0);\
                        return 1;                       \
                }                                       \
        } while (0)
        
#define CHECK_READ(chseg, low, high) \
                if ((low < (chseg)->limit_low) || \
                    (high > (chseg)->limit_high) || \
                    ((msw & 1) && !(eflags & VM_FLAG) && \
                     (((chseg)->access & 10) == 8))) \
                { \
                        x86gpf("Limit check (READ)", 0); \
                        return 1; \
	        } \
	        if (msw&1 && !(eflags&VM_FLAG) && !((chseg)->access & 0x80)) \
	        { \
		        if ((chseg) == &_ss) \
			        x86ss(NULL,(chseg)->seg & 0xfffc); \
		        else \
			        x86np("Read from seg not present", (chseg)->seg & 0xfffc); \
		        return 1; \
                }

#define CHECK_WRITE(chseg, low, high)  \
                if ((low < (chseg)->limit_low) || \
                    (high > (chseg)->limit_high) || \
                    !((chseg)->access & 2) || ((msw & 1) && \
                    !(eflags & VM_FLAG) && ((chseg)->access & 8))) \
                { \
                        x86gpf("Limit check (WRITE)", 0); \
                        return 1; \
	        } \
	        if (msw&1 && !(eflags&VM_FLAG) && !((chseg)->access & 0x80)) \
	        { \
		        if ((chseg) == &_ss) \
			        x86ss(NULL,(chseg)->seg & 0xfffc); \
		        else \
			        x86np("Write to seg not present", (chseg)->seg & 0xfffc); \
		        return 1; \
                }

#define CHECK_WRITE_REP(chseg, low, high) \
                if ((low < (chseg)->limit_low) || \
                    ((unsigned)high > (chseg)->limit_high)) \
                { \
                        x86gpf("Limit check (WRITE REP)", 0); \
                        break; \
	        } \
	        if (msw&1 && !(eflags&VM_FLAG) && !((chseg)->access & 0x80)) \
	        { \
		        if ((chseg) == &_ss) \
			        x86ss(NULL,(chseg)->seg & 0xfffc); \
		        else \
			        x86np("Write (REP) to seg not present", (chseg)->seg & 0xfffc); \
		        break; \
                }


#define NOTRM   if (!(msw & 1) || (eflags & VM_FLAG)) \
                { \
                        x86_int(6); \
                        return 1; \
                }



static INLINE uint8_t fastreadb(uint32_t a)
{
        uint8_t *t;
        
        if ((a >> 12) == pccache) 
                return *((uint8_t *)&pccache2[a]);
        t = getpccache(a);
        if (cpu_state.abrt)
                return 0;
        pccache = a >> 12;
        pccache2 = t;
        return *((uint8_t *)&pccache2[a]);
}

static INLINE uint16_t fastreadw(uint32_t a)
{
        uint8_t *t;
        uint16_t val;
        if ((a&0xFFF)>0xFFE)
        {
                val = fastreadb(a);
                val |= (fastreadb(a + 1) << 8);
                return val;
        }
        if ((a>>12)==pccache) return *((uint16_t *)&pccache2[a]);
        t = getpccache(a);
        if (cpu_state.abrt)
                return 0;

        pccache = a >> 12;
        pccache2 = t;
        return *((uint16_t *)&pccache2[a]);
}

static INLINE uint32_t fastreadl(uint32_t a)
{
        uint8_t *t;
        uint32_t val;
        if ((a&0xFFF)<0xFFD)
        {
                if ((a>>12)!=pccache)
                {
                        t = getpccache(a);
                        if (cpu_state.abrt)
                                return 0;
                        pccache2 = t;
                        pccache=a>>12;
                        /* return *((uint32_t *)&pccache2[a]); */
                }
                return *((uint32_t *)&pccache2[a]);
        }
        val  = fastreadw(a);
        val |= (fastreadw(a + 2) << 16);
        return val;
}

static INLINE uint8_t getbyte()
{
        cpu_state.pc++;
        return fastreadb(cs + (cpu_state.pc - 1));
}

static INLINE uint16_t getword()
{
        cpu_state.pc+=2;
        return fastreadw(cs+(cpu_state.pc-2));
}

static INLINE uint32_t getlong()
{
        cpu_state.pc+=4;
        return fastreadl(cs+(cpu_state.pc-4));
}

static INLINE uint64_t getquad()
{
        cpu_state.pc+=8;
        return fastreadl(cs+(cpu_state.pc-8)) | ((uint64_t)fastreadl(cs+(cpu_state.pc-4)) << 32);
}



static INLINE uint8_t geteab()
{
        if (cpu_mod == 3)
                return (cpu_rm & 4) ? cpu_state.regs[cpu_rm & 3].b.h : cpu_state.regs[cpu_rm&3].b.l;
        if (eal_r)
                return *(uint8_t *)eal_r;
        return readmemb(easeg, cpu_state.eaaddr);
}

static INLINE uint16_t geteaw()
{
        if (cpu_mod == 3)
                return cpu_state.regs[cpu_rm].w;
        /* cycles-=3; */
        if (eal_r)
                return *(uint16_t *)eal_r;
        return readmemw(easeg, cpu_state.eaaddr);
}

static INLINE uint32_t geteal()
{
        if (cpu_mod == 3)
                return cpu_state.regs[cpu_rm].l;
        /* cycles-=3; */
        if (eal_r)
                return *eal_r;
        return readmeml(easeg, cpu_state.eaaddr);
}

static INLINE uint64_t geteaq()
{
        return readmemq(easeg, cpu_state.eaaddr);
}

static INLINE uint8_t geteab_mem()
{
        if (eal_r) return *(uint8_t *)eal_r;
        return readmemb(easeg,cpu_state.eaaddr);
}
static INLINE uint16_t geteaw_mem()
{
        if (eal_r) return *(uint16_t *)eal_r;
        return readmemw(easeg,cpu_state.eaaddr);
}
static INLINE uint32_t geteal_mem()
{
        if (eal_r) return *eal_r;
        return readmeml(easeg,cpu_state.eaaddr);
}

static INLINE void seteaq(uint64_t v)
{
        writememql(easeg, cpu_state.eaaddr, v);
}

#if 0
# define seteab(v) if (cpu_mod!=3) { if (eal_w) *(uint8_t *)eal_w=v;  else writememb386l(easeg,cpu_state.eaaddr,v); } else if (cpu_rm&4) cpu_state.regs[cpu_rm&3].b.h=v; else cpu_state.regs[cpu_rm].b.l=v
# define seteaw(v) if (cpu_mod!=3) { if (eal_w) *(uint16_t *)eal_w=v; else writememwl(easeg,cpu_state.eaaddr,v);    } else cpu_state.regs[cpu_rm].w=v
# define seteal(v) if (cpu_mod!=3) { if (eal_w) *eal_w=v;             else writememll(easeg,cpu_state.eaaddr,v);    } else cpu_state.regs[cpu_rm].l=v
#else
# define seteab(v) if (cpu_mod!=3) { CHECK_WRITE(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr); if (eal_w) *(uint8_t *)eal_w=v;  else { writememb386l(easeg,cpu_state.eaaddr,v); } } else if (cpu_rm&4) cpu_state.regs[cpu_rm&3].b.h=v; else cpu_state.regs[cpu_rm].b.l=v
# define seteaw(v) if (cpu_mod!=3) { CHECK_WRITE(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 1); if (eal_w) *(uint16_t *)eal_w=v; else { writememwl(easeg,cpu_state.eaaddr,v); } } else cpu_state.regs[cpu_rm].w=v
# define seteal(v) if (cpu_mod!=3) { CHECK_WRITE(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 3); if (eal_w) *eal_w=v;             else { writememll(easeg,cpu_state.eaaddr,v); } } else cpu_state.regs[cpu_rm].l=v
#endif
 
#define seteab_mem(v) if (eal_w) *(uint8_t *)eal_w=v;  else writememb386l(easeg,cpu_state.eaaddr,v);
#define seteaw_mem(v) if (eal_w) *(uint16_t *)eal_w=v; else writememwl(easeg,cpu_state.eaaddr,v);
#define seteal_mem(v) if (eal_w) *eal_w=v;             else writememll(easeg,cpu_state.eaaddr,v);
 
#define getbytef() ((uint8_t)(fetchdat)); cpu_state.pc++
#define getwordf() ((uint16_t)(fetchdat)); cpu_state.pc+=2
#define getbyte2f() ((uint8_t)(fetchdat>>8)); cpu_state.pc++
#define getword2f() ((uint16_t)(fetchdat>>8)); cpu_state.pc+=2


#define rmdat rmdat32
#define fetchdat rmdat32

void x86_int(uint32_t num);
