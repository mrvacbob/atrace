/*
 *  raytrace.h
 *  atrace
 *
 *  Created by Alexander Strange on 6/23/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __raytrace_h
#define __raytrace_h
#include "scene.h"

struct image
{
	f_pixel *buf;
	f_real *depth_buf;
	f_real minv, maxv;
	size_t w, h;
	
	image(size_t w, size_t h);
	~image();
	void write_to_bmp(const char *path);
	
	inline void set(size_t x, size_t y, const f_pixel p, f_real depth) {
		buf[y*w + x] = p;
		depth_buf[y*w + x] = depth;
	}
	
	void finish();
	
	f_pixel autolevel(const f_pixel &p) {
		return p.range_fit(minv, maxv);
	}
};

struct raytracer
{
	scene sc;
	color background;
    
	raytracer(primitive **pr, size_t primcount) : sc(this,pr,primcount) {}
	
	bool light_reaches(primitive *l, primitive *pi, point3 &to, color *c, vector3 *L, media *medium, unsigned index);
	color color_of_primitive_at(const ray &r, world_distance dist, primitive *pi, media *medium, unsigned index, intersectResult res, primitive **backtracking=NULL);
	primitive *find_primitive_along(const ray &r, world_distance *col_dist, bool closest = true, primitive *ignore = NULL, intersectResult *restype = NULL, world_distance max_dist = HUGE_VAL, bool consider_close_miss=false);
	color trace(const ray &r, world_distance *dist_reached, media *medium, primitive *ignore = NULL, intersectResult *res = NULL, unsigned index=1, primitive **backtracking=NULL);
		
	image *render(size_t w, size_t h, const camera &cam);
};

#define FOR_EACH_LIGHT() for (size_t i = 0; i < sc.primcount; i++) {primitive *light = sc.prims[i]; if (light->light) {
#define FOR_EACH_LIGHT_END() }}
#endif