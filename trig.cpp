/*
 *  maths.cpp
 *  atrace
 *
 *  Created by Alexander Strange on 6/23/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "trig.h"

real srgbToL[256];

static inline real linearFromSRGB(uint8_t vi)
{
	const real a = .055, v = vi/255.;
	real v_linear = (v > .04045) ? pow((v + a) / 1.055, 2.4) : (v / 12.92); 
	return v_linear;
}

static struct srgbToLSetup
{
	srgbToLSetup()
	{
		for (int i = 0; i < 256; i++)
			srgbToL[i] = linearFromSRGB(i);
	}
} srgbsetup;