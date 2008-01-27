/*
 *  vector_template.h
 *  atrace
 *
 *  Created by Alexander Strange on 11/25/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#if N == 4
#define N4(x...) x 
#else
#define N4(x...) 
#endif

template <typename T> struct vectorX<T, N>
{
	typedef vectorX<T, N> V;
	
	union {
		T val[N];
		struct {T x, y, z N4(,w);};
		struct {T r, g, b N4(,a);};
	};
	
	vectorX(const T v[N]) : x(v[0]), y(v[1]), z(v[2]) N4(, w(v[3])) {}
	vectorX(const T a, const T b, const T c N4(, const T d = 1)) : x(a), y(b), z(c) N4(, w(d)) {}
	vectorX(const T v) : x(v), y(v), z(v) N4(, w(v)) {}
	vectorX() : x(0), y(0), z(0) N4(, w(0)) {}
	
	vectorX(const V &t) : x(t.x), y(t.y), z(t.z) N4(, w(t.w)) {}
	
	T dot(const V &t) const {return x*t.x + y*t.y + z*t.z N4(+ w*t.w);}
	T dot_self() const {return dot(*this);}

#if N != 4
	V cross(const V &t) const {return V(y*t.z - z*t.y, z*t.x - x*t.z, x*t.y - y*t.x);}
#endif
	
#define arithmetic_op(op) \
	V &operator op##=(const V &t) { \
		x op##= t.x; y op##= t.y; z op##= t.z; N4(w op##= t.w;) \
		return *this;\
	}\
	V operator op(const V &t) const { \
		return V(*this) op##= t;\
	}\
	V &operator op##=(const T t) { \
		x op##= t; y op##= t; z op##= t; N4(w op##= t;)\
		return *this;\
	}\
	V operator op(const T t) const __attribute__((always_inline)) { \
		return V(*this) op##= t;\
	}\

	arithmetic_op(*);
	arithmetic_op(/);
	arithmetic_op(-);
	arithmetic_op(+);
	
#undef arithmetic_op
	
	V operator-() const {return V(-x,-y,-z N4(,-w));}
	
	void normalize() {		
		T factor = sqrt(dot_self());
		factor = factor ? factor : 1;
		operator/=(factor);
	}
	
	V reflect(const V &normal) const {T cosI = dot(normal); return *this - (normal * (cosI * 2));}
	
	void set_min(const V &t) {x = dmin(x,t.x); y = dmin(y,t.y); z = dmin(z,t.z); N4(w = dmin(w, t.w);)}
	void set_max(const V &t) {x = dmax(x,t.x); y = dmax(y,t.y); z = dmax(z,t.z); N4(w = dmax(w, t.w);)}
	
	V range_fit(const V &minv, const V &maxv) const {return (*this - minv) / (maxv - minv);}
	V range_fit(const T &minv, const T &maxv) const {return (*this - minv) / (maxv - minv);}
	
	V &operator=(const V &t) {x=t.x;y=t.y;z=t.z; N4(w=t.w;) return *this;}
		
	vectorX<uint8_t, N> dither(V &error)
	{
		return vectorX<uint8_t, N>(dithered_fromL(r,&error.r), dithered_fromL(g,&error.g), dithered_fromL(b,&error.b) N4(, dithered_fromL(a, &error.a)));
	}
	
	friend T dot(const V &a, const V &b) {return a.dot(b);}
	friend V cross(const V &a, const V &b) {return a.cross(b);}
	friend V blend(const V &a, const V &b, T weight) {		
		return (a * weight) + (b * (1. - weight));
	}
	
	friend T distance_between(const V &a, const V &b)
	{
		return sqrt((a - b).dot_self());
	}	
	
	friend V normalize(const V &t) {V tmp(t); tmp.normalize(); return tmp;}
	
	friend V points_away_from(const V &normal, const V &dir)
	{
		return (dir.dot(normal) < 0) ? normal : -normal;
	}
	
#if N == 4
	void print() const {printf("x %f y %f z %f w %f\n",x,y,z,w);}	
#else
	void print() const {printf("x %f y %f z %f\n",x,y,z);}	
#endif
};

#ifdef __SSSE3__
#include <pmmintrin.h>

template<> struct vectorX<float, N>
{
	typedef float T;
	typedef vectorX<float, N> V;
	
	union {
		__m128 s;
		struct {T x, y, z, w;};
		struct {T r, g, b, a;};
	};
	
	void zero_w() {s=zero_w_sse(s);}
	vectorX(T a_, T b, T c, T d = 1) : s(_mm_setr_ps(a_,b,c,d)) {}
	vectorX(T v) : s(_mm_set1_ps(v)) {if (N!=4) zero_w();}
	vectorX(__m128 s) : s(s) {}
	vectorX() : s(_mm_set1_ps(0)) {}
	
	vectorX(const V &t) : s(t.s) {}
	
	T dot(const V &t) const {
		T ret;
		__m128 tmp = _mm_mul_ps(s, t.s);
		if (N!=4) tmp = zero_w_sse(tmp);
		tmp = _mm_hadd_ps(tmp, tmp);
		tmp = _mm_hadd_ps(tmp, tmp);
		_mm_store_ss(&ret, tmp);
		return ret;
	}
	T dot_self() const {return dot(*this);}
	
	V cross(const V &t) const {return V(y*t.z - z*t.y, z*t.x - x*t.z, x*t.y - y*t.x);}
	
#define arithmetic_op(op, sse) \
	V &operator op##=(const V &t) { \
		s = sse(s, t.s);\
		return *this;\
	}\
	V operator op(const V &t) const { \
		return V(*this) op##= t;\
	}\
	V &operator op##=(const T t) { \
		s = sse(s, _mm_set1_ps(t));\
		return *this;\
	}\
	inline V operator op(const T t) const __attribute__((always_inline)) { \
		return V(*this) op##= t;\
	}
	
	arithmetic_op(*, _mm_mul_ps);
	arithmetic_op(/, _mm_div_ps);
	arithmetic_op(-, _mm_sub_ps);
	arithmetic_op(+, _mm_add_ps);
	
#undef arithmetic_op
	
	V operator-() const {return V(_mm_xor_ps(s, _mm_set1_ps(-0.)));}
	
	void normalize() {		
		T factor = sqrtf(dot_self());
		factor = factor ? factor : 1.;
		operator/=(factor);
	}
	
	inline V reflect(const V &normal) const __attribute__((always_inline)) {T cosI = dot(normal); return *this - (normal * (cosI * 2.));}
	
	void set_min(const V &t) {s = _mm_min_ps(s, t.s);}
	void set_max(const V &t) {s = _mm_max_ps(s, t.s);}
	
	V range_fit(const V &minv, const V &maxv) const {return (*this - minv) / (maxv - minv);}
	V range_fit(const T &minv, const T &maxv) const {return (*this - minv) / (maxv - minv);}
	
	friend V normalize(const V &t) {V tmp(t); tmp.normalize(); return tmp;}
	
	friend T dot(const V &a, const V &b) {return a.dot(b);}
	friend V cross(const V &a, const V &b) {return a.cross(b);}
	friend V blend(const V &a, const V &b, T weight) {		
		return (a * weight) + (b * (1. - weight));
	}
	
	V &operator=(const V &t) {s=t.s; return *this;}
	
	friend V points_away_from(const V &normal, const V &dir)
	{
		return (dir.dot(normal) < 0) ? normal : -normal;
	}
	
	friend T distance_between(const V &a, const V &b)
	{
		return sqrtf((a - b).dot_self());
	}
	
	vectorX<uint8_t, N> dither(V &error)
	{
#if N != 4
		return vectorX<uint8_t, 3>(dithered_fromL(r,&error.r), dithered_fromL(g,&error.g), dithered_fromL(b,&error.b));
#else
		return vectorX<uint8_t, 4>(dithered_fromL(r,&error.r), dithered_fromL(g,&error.g), dithered_fromL(b,&error.b), dithered_fromL(a, &error.a));
#endif
	}
	
	void print() const {if (N!=4) printf("x %f y %f z %f\n",x,y,z);
	else printf("x %f y %f z %f w %f\n",x,y,z,w);}
} __attribute__((aligned));
#endif

#undef N4
#undef N