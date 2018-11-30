/**
 * Porting of the libm library to the PandA framework
 * starting from the original FDLIBM 5.3 (Freely Distributable LIBM) developed by SUN
 * plus the newlib version 1.19 from RedHat and plus uClibc version 0.9.32.1 developed by Erik Andersen.
 * The author of this port is Fabrizio Ferrandi from Politecnico di Milano.
 * The porting fall under the LGPL v2.1, see the files COPYING.LIB and COPYING.LIBM_PANDA in this directory.
 * Date: September, 11 2013.
 */
/* 2009 for Newlib:  Sun's sf_ilogb.c converted to be sf_logb.c.  */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* float logb(float x)
 * return the binary exponent of non-zero x
 * logbf(0) = -inf, raise divide-by-zero floating point exception
 * logbf(+inf|-inf) = +inf (no signal is raised)
 * logbf(NaN) = NaN (no signal is raised)
 * Per C99 recommendation, a NaN argument is returned unchanged.
 */

#include "math_privatef.h"

float logbf(float x)
{
   int hx, ix;

   GET_FLOAT_WORD(hx, x);
   hx &= 0x7fffffff;
   if(FLT_UWORD_IS_ZERO(hx))
   {
      float xx;
      /* arg==0:  return -inf and raise divide-by-zero exception */
      SET_FLOAT_WORD(xx, hx); /* +0.0 */
      return -1. / xx;        /* logbf(0) = -inf */
   }
   if(FLT_UWORD_IS_SUBNORMAL(hx))
   {
      for(ix = -126, hx <<= 8; hx > 0; hx <<= 1)
         ix -= 1;
      return (float)ix;
   }
   else if(FLT_UWORD_IS_INFINITE(hx))
      return HUGE_VALF; /* x==+|-inf */
   else if(FLT_UWORD_IS_NAN(hx))
      return x;
   else
      return (float)((hx >> 23) - 127);
}

#ifdef _DOUBLE_IS_32BITS

double logb(double x)
{
   return (double)logbf((float)x);
}

#endif /* defined(_DOUBLE_IS_32BITS) */
