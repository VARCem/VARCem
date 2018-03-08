/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		A better random number generation, used for floppy weak bits
 *		and network MAC address generation.
 *
 * Version:	@(#)random.c	1.0.1	2018/02/14
 *
 * Author:	Miran Grca, <mgrca8@gmail.com>
 *
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "random.h"


uint32_t preconst = 0x6ED9EBA1;


static __inline uint32_t rotl32c (uint32_t x, uint32_t n)
{
#if 0
  assert (n<32);
#endif
#ifdef _MSC_VER
  return _rotl(x, n);
#else
  return (x<<n) | (x>>(-n&31));
#endif
}

static __inline uint32_t rotr32c (uint32_t x, uint32_t n)
{
#if 0
  assert (n<32);
#endif
#ifdef _MSC_VER
  return _rotr(x, n);
#else
  return (x>>n) | (x<<(-n&31));
#endif
}

#define ROTATE_LEFT rotl32c

#define ROTATE_RIGHT rotr32c

static __inline unsigned long long rdtsc(void)
{
    unsigned hi, lo;
#ifdef _MSC_VER
    __asm {
		rdtsc
		mov hi, edx	; EDX:EAX is already standard return!!
		mov lo, eax
    }
#else
      __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
#endif
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static uint32_t RDTSC(void)
{
	return (uint32_t) (rdtsc());
}


static void random_twist(uint32_t *val)
{
	*val = ROTATE_LEFT(*val, rand() % 32);
	*val ^= 0x5A827999;
	*val = ROTATE_RIGHT(*val, rand() % 32);
	*val ^= 0x4ED32706;
}


uint8_t random_generate(void)
{
	uint16_t r = 0;
	r = (rand() ^ ROTATE_LEFT(preconst, rand() % 32)) % 256;
	random_twist(&preconst);
	return (r & 0xff);
}


void random_init(void)
{
	uint32_t seed = RDTSC();
	srand(seed);
	return;
}
