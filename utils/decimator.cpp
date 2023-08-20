#
/*
 *    Copyright (C) 2010, 2011, 2012
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the jan's software
 *
 *    the software is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    the software is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with the software; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include	"decimator.h"
#define  _USE_MATH_DEFINES
#include <math.h>

//
//	decimationFilter.
//	Useful for e.g. reduction of the samplerate with dabsticks
//	Two tastes are within the same class: the lowpass and
//	the bandpass variant.

	decimator::decimator (int16_t firSize,
	                      int32_t low,
	                      int32_t fs,
	                      int16_t Dm):
	                          filterKernel (firSize),
	                          buffer       (firSize) {

float	*tmp 	= (float *)_malloca (firSize * sizeof (float));
float	f	= (float)low / fs;
float	sum	= 0.0;

	filterSize		= firSize;
	decimationFactor	= Dm;
	decimationCounter	= 0;

	for (int i = 0; i < filterSize; i ++) {
	   if (i == filterSize / 2)
	      tmp [i] = 2 * M_PI * f;
	   else 
	      tmp [i] = sin (2 * M_PI * f * (i - filterSize/2))/ (i - filterSize/ 2);

	   tmp [i]  *= (0.42 -
		    0.5 * cos (2 * M_PI * (float)i / (float)filterSize) +
		    0.08 * cos (4 * M_PI * (float)i / (float)filterSize));

	   sum += tmp [i];
	}

	for (int i = 0; i < filterSize; i ++)
	   filterKernel [i] = std::complex<float> (tmp [i] / sum, 0);
	for (int i = 0; i < filterSize; i ++)
	   buffer [i] = std::complex<float> (0, 0);
}

	decimator::~decimator () {
}
//
//	The real cpu killer: this function is called once for every
//	sample that comes from the dongle. So, it really should be
//	optimized.
bool	decimator::Pass (std::complex<float> z,
	                                 std::complex<float> *z_out) {
int16_t		i;
std::complex<float>	tmp	= 0;
int16_t		index;

	buffer [ip] = z;
	if (++decimationCounter < decimationFactor) {
	   ip =  (ip + 1) % filterSize;
	   return false;
	}

	decimationCounter = 0;
//
//	we are working with a circular buffer, we take two steps
//	we move from ip - 1 .. 0 with i going from 0 .. ip -1
	for (i = 0; i <= ip; i ++) {
	   index =  ip - i;
	   tmp 	+= buffer [index] * filterKernel [i];
	}
//	and then we take the rest
	for (i = ip + 1; i < filterSize; i ++) {
	   index =  filterSize + ip - i;
	   tmp 	+= buffer [index] * filterKernel [i];
	}

	ip = (ip + 1) % filterSize;
	*z_out = tmp;
	return true;
}

