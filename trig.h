/*
 *  maths.h
 *  atrace
 *
 *  Created by Alexander Strange on 6/23/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __trig_h
#define __trig_h
#include "atrace.h"
#include <math.h>
#include <stdio.h>

#ifdef M_PI
#define PI M_PI
#else
#define PI 3.14159265358979323846
#endif
#define TWOPI 6.28318530717958647692

#define LOWPRECISION
#ifdef LOWPRECISION
#ifdef __SSSE3__
#define SSEVEC
#endif
#define EPSILON ((world_distance)(1./4096.))

typedef float world_distance;
typedef world_distance angle;
typedef float real;
#define pow powf
#define exp expf
#define sqrt sqrtf
#define fabs fabsf
#define fmod fmodf
#else
#define EPSILON ((world_distance)(1./1099511627776.))

typedef double world_distance;
typedef world_distance angle;
typedef double real;
#endif
typedef float f_real;

template<typename T, typename T2> T dmin(T a, T2 b) {return (a <= b) ? a : b;}
template<typename T, typename T2> T dmax(T a, T2 b) {return (a >= b) ? a : b;}

static inline bool close(world_distance a, world_distance b) {return fabs(a-b) <= EPSILON;}
static inline bool above(world_distance a, world_distance b) {return (a - b) >= EPSILON;}

template <typename T> void swap(T &a, T &b)
{
	T c = a;
	a = b;
	b = c;
}

static size_t wrap(ssize_t i, ssize_t off, ssize_t m, bool modulo)
{
	i += off;
	
	if (modulo) {
		i = (i < 0) ? (i + m) : ((i >= m) ? (i - m) : i);
	} else i = dmin(dmax(i, 0), m-1);
	
	return i;
}

extern real srgbToL[256];

static inline real colorToL(uint8_t vi)
{
	return srgbToL[vi];
}

static inline uint8_t colorFromL(f_real v)
{
	const f_real a = .055f;
	f_real v_srgb = (v > .0031308f) ? ((a + 1.f) * powf(v, 1.f/2.4f) - a) : (v * 12.92f);
		
	return dmin(dmax((int)lrintf(v_srgb * 255.f), 0), 255);
}

static inline uint8_t dithered_fromL(f_real v, f_real *error)
{
	f_real er = *error, v_er = v+er;
	
	uint8_t o = colorFromL(v_er);
	f_real v2 = colorToL(o);
	
	*error = v_er-v2;
	return o;
}

template <typename T, int N> struct vectorX
{
};

#ifdef __SSSE3__
#include <pmmintrin.h>
static inline __m128 sse_1000() {return (__m128)_mm_set_epi32(-1,0,0,0);}
static inline __m128 zero_w_sse(__m128 s) {return _mm_andnot_ps(sse_1000(), s);}
#endif

#define N 3
#include "vector_template.h"
#define N 4
#include "vector_template.h"

typedef vectorX<world_distance, 3> vector3;
typedef vectorX<world_distance, 4> vector4;

typedef vectorX<real, 3> color3;
typedef vectorX<real, 4> color4;

typedef color3 color;
typedef vector3 point3;

typedef vectorX<f_real, 3> f_pixel;
typedef vectorX<uint8_t, 3> pixel8;

static inline color4 over(color4 above, color4 below)
{
	return above + below*(1.-above.a);
}

static inline color4 c3to4(color c)
{
#ifdef SSEVEC
	color4 res(c.s);
	res.a=1;
	return res;
#else
	return color4(c.r,c.g,c.b);
#endif
}

static inline color c4to3(color4 c)
{
#ifdef SSEVEC
	return color(c.s);
#else
	return color(c.r,c.g,c.b);
#endif
}

static inline color4 to_premultiplied(color c, real a)
{
	color4 res = c3to4(c);
	
	return res * a;
}

static inline color4 to_premultiplied(color4 cs) {return to_premultiplied(c4to3(cs), cs.a);}

static inline color from_premultiplied(color4 c, real *a = NULL)
{
	real resa = c.a;
	
	if (a) *a = resa;
	
	return c4to3(c * resa);
}

static inline color strip_alpha(color4 c)
{
	return c4to3(c*c.a);
}

struct ray
{
	point3 origin;
	vector3 dir;
	
	ray(const point3 &o, const vector3 &d, bool should_normalize = true) __attribute__((always_inline)) : origin(o), dir(should_normalize ? normalize(d) : d) {}
	
	point3 pointAt(world_distance dist) const {
		return dir * dist + origin;
	}
	
	ray rayAt(world_distance dist) const {
		return ray(pointAt(dist), dir, false);
	}
};

static inline ray ray_from_to(const point3 &origin, const point3 &destination)
{
	vector3 from_to = destination - origin;
	return ray(origin, from_to);
}
#endif