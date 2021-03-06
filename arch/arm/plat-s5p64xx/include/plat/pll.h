/* arch/arm/plat-s5p64xx/include/plat/pll.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * S5P64XX PLL code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#define XTAL_FREQ	12000000

#define S5P6440_PLL_MDIV_MASK	((1 << (25-16+1)) - 1)
#define S5P6440_PLL_PDIV_MASK	((1 << (13-8+1)) - 1)
#define S5P6440_PLL_SDIV_MASK	((1 << (2-0+1)) - 1)
#define S5P6440_PLL_MDIV_SHIFT	(16)
#define S5P6440_PLL_PDIV_SHIFT	(8)
#define S5P6440_PLL_SDIV_SHIFT	(0)

#include <asm/div64.h>

static inline unsigned long s5p6440_get_pll(unsigned long baseclk,
					    u32 pllcon)
{
	u32 mdiv, pdiv, sdiv;
	u64 fvco = baseclk;

	mdiv = (pllcon >> S5P6440_PLL_MDIV_SHIFT) & S5P6440_PLL_MDIV_MASK;
	pdiv = (pllcon >> S5P6440_PLL_PDIV_SHIFT) & S5P6440_PLL_PDIV_MASK;
	sdiv = (pllcon >> S5P6440_PLL_SDIV_SHIFT) & S5P6440_PLL_SDIV_MASK;

	fvco *= mdiv;
	do_div(fvco, (pdiv << sdiv));

	return (unsigned long)fvco;
}

#define S5P6440_EPLL_MDIV_MASK	((1 << (23-16+1)) - 1)
#define S5P6440_EPLL_PDIV_MASK	((1 << (13-8+1)) - 1)
#define S5P6440_EPLL_SDIV_MASK	((1 << (2-0+1)) - 1)
#define S5P6440_EPLL_MDIV_SHIFT	(16)
#define S5P6440_EPLL_PDIV_SHIFT	(8)
#define S5P6440_EPLL_SDIV_SHIFT	(0)
#define S5P6440_EPLL_KDIV_MASK  (0xffff)

static inline unsigned long s5p6440_get_epll(unsigned long baseclk)
{
	unsigned long result;
	u32 epll0 = __raw_readl(S3C_EPLL_CON);
	u32 epll1 = __raw_readl(S3C_EPLL_CON_K);
	u32 mdiv, pdiv, sdiv, kdiv;
	u64 tmp;

	mdiv = (epll0 >> S5P6440_EPLL_MDIV_SHIFT) & S5P6440_EPLL_MDIV_MASK;
	pdiv = (epll0 >> S5P6440_EPLL_PDIV_SHIFT) & S5P6440_EPLL_PDIV_MASK;
	sdiv = (epll0 >> S5P6440_EPLL_SDIV_SHIFT) & S5P6440_EPLL_SDIV_MASK;
	kdiv = epll1 & S5P6440_EPLL_KDIV_MASK;

	/* We need to multiple baseclk by mdiv (the integer part) and kdiv
	 * which is in 2^16ths, so shift mdiv up (does not overflow) and
	 * add kdiv before multiplying. The use of tmp is to avoid any
	 * overflows before shifting bac down into result when multipling
	 * by the mdiv and kdiv pair.
	 */

	tmp = baseclk;
	tmp *= (mdiv << 16) + kdiv;
	do_div(tmp, (pdiv << sdiv));
	result = tmp >> 16;

	return result;
}
