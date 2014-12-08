/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.monetdb.org/Legal/MonetDBLicense
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2014 MonetDB B.V.
 * All Rights Reserved.
 */

/*
 * N.J. Nes, M. Kersten
 * 07/01/1996
 * The math module
 * This module contains the math commands. The implementation is very simply,
 * the c math library functions are called. See for documentation the
 * ANSI-C/POSIX manuals of the equaly named functions.
 *
 * NOTE: the operand itself is being modified, rather than that we produce
 * a new BAT. This to save the expensive copying.
 */
#include "monetdb_config.h"
#include "mmath.h"
#ifdef HAVE_FENV_H
#include <fenv.h>
#else
#define feclearexcept(x)
#define fetestexcept(x)		0
#endif

#ifdef _MSC_VER
# include <float.h>
#if _MSC_VER <= 1600
/* Windows spells these differently */
# define isnan(x)	_isnan(x)
#endif
# define finite(x)	_finite(x)
/* NOTE: HAVE_FPCLASS assumed... */
# define fpclass(x)	_fpclass(x)
# define FP_NINF		_FPCLASS_NINF
# define FP_PINF		_FPCLASS_PINF
#else /* !_MSC_VER */
# ifdef HAVE_IEEEFP_H
#  include <ieeefp.h>
# endif
#endif

#if defined(HAVE_FPCLASSIFY) || defined(fpclassify)
/* C99 interface: fpclassify */
# define MNisinf(x)   (fpclassify(x) == FP_INFINITE)
# define MNisnan(x)   (fpclassify(x) == FP_NAN)
# define MNfinite(x)  (!MNisinf(x) && !MNisnan(x))
#else
# define MNisnan(x)   isnan(x)
# define MNfinite(x)  finite(x)
# ifdef HAVE_ISINF
#  define MNisinf(x)  isinf(x)
# else
static int
MNisinf(double x)
{
#  ifdef HAVE_FPCLASS
	int cl = fpclass(x);

	return ((cl == FP_NINF) || (cl == FP_PINF));
#  else
	(void)x;
	return 0;		/* XXX not correct if infinite */
#  endif
}
# endif
#endif /* HAVE_FPCLASSIFY */

#define atan2_binary(x, y, z) *z = atan2(*x,*y)
#define pow_binary(x, y, z)   *z = pow(*x,*y)

#define cot(x)				(1 / tan(x))
#define radians(x)			((x) * 3.14159265358979323846 / 180.0)
#define degrees(x)			((x) * 180.0 / 3.14159265358979323846)

static str
math_unary_ISNAN(bit *res, const dbl *a)
{
#ifdef DEBUG
	printf("math_unary_ISNAN\n");
#endif
	if (*a == dbl_nil) {
		*res = bit_nil;
	} else {
		*res = MNisnan(*a);
	}
	return MAL_SUCCEED;
}

static str
math_unary_ISINF(int *res, const dbl *a)
{
#ifdef DEBUG
	printf("math_unary_ISINF\n");
#endif
	if (*a == dbl_nil) {
		*res = int_nil;
	} else {
		if (MNisinf(*a)) {
			*res = (*a < 0.0) ? -1 : 1;
		} else {
			*res = 0;
		}
	}
	return MAL_SUCCEED;
}

static str
math_unary_FINITE(bit *res, const dbl *a)
{
#ifdef DEBUG
	printf("math_unary_FINITE\n");
#endif
	if (*a == dbl_nil) {
		*res = bit_nil;
	} else {
		*res = MNfinite(*a);
	}
	return MAL_SUCCEED;
}

#define unopM5(NAME, FUNC)										\
str																\
MATHunary##NAME##dbl(dbl *res , const dbl *a)					\
{																\
	double tmp1,tmp2;											\
	str msg= MAL_SUCCEED;										\
	if (*a == dbl_nil) {										\
		*res =dbl_nil;											\
	} else {													\
		tmp1= *a;												\
		errno = 0;												\
		feclearexcept(FE_ALL_EXCEPT);							\
		tmp2 = FUNC(tmp1);										\
		if (errno != 0 ||										\
			fetestexcept(FE_INVALID | FE_DIVBYZERO |			\
						 FE_OVERFLOW | FE_UNDERFLOW) != 0)		\
			throw(MAL, "mmath." #FUNC, "Math exception: %s",	\
				  strerror(errno));								\
		*res = (dbl) tmp2;										\
	}															\
   return msg;													\
}																\
str																\
MATHunary##NAME##flt(flt *res , const flt *a)					\
{																\
	double tmp1,tmp2;											\
	str msg= MAL_SUCCEED;										\
	if (*a == flt_nil) {										\
		*res =flt_nil;											\
	} else {													\
		tmp1= *a;												\
		errno = 0;												\
		feclearexcept(FE_ALL_EXCEPT);							\
		tmp2 = FUNC(tmp1);										\
		if (errno != 0 ||										\
			fetestexcept(FE_INVALID | FE_DIVBYZERO |			\
						 FE_OVERFLOW | FE_UNDERFLOW) != 0)		\
			throw(MAL, "mmath." #FUNC, "Math exception: %s",	\
				  strerror(errno));								\
		*res = (flt) tmp2;										\
	}															\
   return msg;													\
}

#define binopbaseM5(NAME,X2,X3)							\
str														\
MATHbinary##NAME##X3(X3 *res, const X3 *a, const X3 *b)	\
{														\
   if (*a == X3##_nil || *b == X3##_nil) {				\
		*res = X3##_nil;								\
   } else {												\
		dbl r1 ,a1 = *a, b1 = *b;						\
		X2##_binary( &a1, &b1, &r1);					\
		*res= (X3) r1;									\
   }													\
   return MAL_SUCCEED;									\
}

#define binopM5(X,Y)							\
  binopbaseM5(X,Y,dbl)							\
  binopbaseM5(X,Y,flt)

#define roundM5(X1)											\
str															\
MATHbinary_ROUND##X1(X1 *res, const X1 *x, const int *y)	\
{															\
  if(*x == X1##_nil || *y == int_nil) {						\
	*res = X1##_nil;										\
  } else {													\
	dbl factor = pow(10,*y), integral;						\
	dbl tmp = *y>0?modf(*x,&integral):*x;					\
															\
	tmp *= factor;											\
	if(tmp>=0)												\
	  tmp = floor(tmp+0.5);									\
	else													\
	  tmp = ceil(tmp-0.5);									\
	tmp /= factor;											\
															\
	if(*y>0)												\
	  tmp += integral;										\
															\
	*res = (X1) tmp;										\
  }															\
  return MAL_SUCCEED;										\
}


unopM5(_ACOS,acos)
unopM5(_ASIN,asin)
unopM5(_ATAN,atan)
binopM5(_ATAN2,atan2)
unopM5(_COS,cos)
unopM5(_SIN,sin)
unopM5(_TAN,tan)
unopM5(_COT,cot)
unopM5(_RADIANS,radians)
unopM5(_DEGREES,degrees)

unopM5(_COSH,cosh)
unopM5(_SINH,sinh)
unopM5(_TANH,tanh)

unopM5(_EXP,exp)
unopM5(_LOG,log)
unopM5(_LOG10,log10)

binopM5(_POW,pow)
unopM5(_SQRT,sqrt)

unopM5(_CEIL,ceil)
unopM5(_FLOOR,floor)

str
MATHunary_FABSdbl(dbl *res , const dbl *a)
{
	*res = *a == dbl_nil ? dbl_nil : fabs(*a);
	return MAL_SUCCEED;
}

roundM5(dbl)
roundM5(flt)

str
MATHunary_ISNAN(bit *res, const dbl *a)
{
	return math_unary_ISNAN(res, a);
}

str
MATHunary_ISINF(int *res, const dbl *a)
{
	return math_unary_ISINF(res, a);
}

str
MATHunary_FINITE(bit *res, const dbl *a)
{
	return math_unary_FINITE(res, a);
}

str
MATHrandint(int *res)
{
#ifdef STATIC_CODE_ANALYSIS
	*res = 0;
#else
	*res = rand();
#endif
	return MAL_SUCCEED;
}

str
MATHrandintarg(int *res, const int *dummy)
{
	(void) dummy;
#ifdef STATIC_CODE_ANALYSIS
	*res = 0;
#else
	*res = rand();
#endif
	return MAL_SUCCEED;
}

str
MATHsrandint(void *ret, const int *seed)
{
	(void) ret;
	srand(*seed);
	return MAL_SUCCEED;
}

str
MATHsqlrandint(int *res, const int *seed)
{
	srand(*seed);
#ifdef STATIC_CODE_ANALYSIS
	*res = 0;
#else
	*res = rand();
#endif
	return MAL_SUCCEED;
}

str
MATHpi(dbl *pi)
{
	*pi = 3.14159265358979323846;
	return MAL_SUCCEED;
}

