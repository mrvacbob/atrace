/*
 *  scene.h
 *  atrace
 *
 *  Created by Alexander Strange on 6/23/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __scene_h
#define __scene_h
#include "trig.h"

struct texture
{
	virtual ~texture() {}
	virtual color4 colorAt(world_distance u, world_distance v) = 0;
};

struct flat_texture : public texture
{
	color4 c;
	
	flat_texture(const color4 &c) : c(to_premultiplied(c)) {}
	color4 colorAt(world_distance u, world_distance v) {return c;}
};

struct checkerboard_texture : public texture
{
	color4 even, odd;
	
	checkerboard_texture(const color4 &even, const color4 &odd) : even(to_premultiplied(even)), odd(to_premultiplied(odd)) {}
	
	color4 colorAt(world_distance u, world_distance v);
};

#define IMG_SUPPORT 3

static inline color4 *pixelAddressAt(color4 *image, ssize_t x, ssize_t y, ssize_t w)
{
	return &image[(y + IMG_SUPPORT) * (w+IMG_SUPPORT*2) + (x + IMG_SUPPORT)];
}

struct image_texture : public texture
{
	color4 *image;
	ssize_t w, h;
	world_distance fw, fh;
	bool repeat;
	
	image_texture(const char *png_name, bool repeat);
	virtual ~image_texture() {if (image) delete[] image;}
	
	color4 pixelAt(size_t x, size_t y);
	color4 colorAt(world_distance u, world_distance v);
};

struct texture_placement
{
	texture *tex;
	world_distance uScale, vScale;
	world_distance uShift, vShift;
	
	texture_placement() : tex(NULL), uScale(1), vScale(1), uShift(0), vShift(0) {}
};

struct media
{
	real refractive_index;
	real transmittance; // 1 - absorbance (this math is wrong)
	
	media() : refractive_index(1), transmittance(0) {}
};

struct surface
{
	real reflect, diffuse, specular_exp;
	real filter;
	bool clear_reflect;
	bool dielectric; // dielectrics' reflectivity changes depending on the viewing angle
	
	texture_placement textures[16];
	size_t texcount;
	
	surface() : reflect(0.),diffuse(1),specular_exp(40.),filter(1),clear_reflect(true),dielectric(false),texcount(0.) {}
	~surface() {while (texcount--) {delete textures[texcount].tex; textures[texcount].tex=NULL;}}
	color4 colorAt(world_distance u, world_distance v);
};

enum intersectResult {HITINSIDE=-1,MISS=0,HIT=1};

struct primitive
{
	point3 origin;
	surface mat;
	media med;
	bool light;
	vector3 uAxis, vAxis;
	
	primitive() : light(false) {}
	primitive(const point3 origin, vector3 uAxis, vector3 vAxis) : origin(origin), light(false), uAxis(uAxis), vAxis(vAxis) {}
	virtual ~primitive() {}
	virtual intersectResult intersects(const ray &r, world_distance *dist, world_distance max = HUGE_VAL, bool consider_close_miss=false) const = 0;
	virtual vector3 normalAt(const ray &r) const = 0; 
	virtual bool is_thin() const = 0;
	virtual const char *type() const = 0;
	virtual void uvAt(const point3 &p, world_distance *u, world_distance *v) {*u = p.dot(uAxis); *v = p.dot(vAxis);}
	color4 colorAt(const point3 &p) {world_distance u, v; uvAt(p, &u, &v); return mat.colorAt(u, v);}
};

struct sphere : public primitive
{
	world_distance rad, radSq;

	intersectResult intersects(const ray &r, world_distance *dist, world_distance max = HUGE_VAL, bool consider_close_miss=false) const;
	
	sphere(const point3 origin_, world_distance radius) : primitive(origin_, vector3(1,0,0), vector3(0,1,0)), rad(radius), radSq(radius*radius) {}
	vector3 normalAt(const ray &r) const {
		return points_away_from(normalize(r.origin - origin), r.dir);
	}
	bool is_thin() const {return false;}
	const char *type() const {return "sphere";}
};

struct plane
{
	point3 normal;
	world_distance dist;
	
	plane(const point3 normal, world_distance d) : normal(normal), dist(d) {}
	virtual ~plane() {}
};

struct plane_prim : public primitive, plane
{
	plane_prim(const point3 normal_, world_distance d_) : plane(normalize(normal_), d_) {uAxis = vector3(normal.z, normal.y, -normal.x); vAxis = uAxis.cross(normal);}
	
	intersectResult intersects(const ray &r, world_distance *dist, world_distance max = HUGE_VAL, bool consider_close_miss=false) const;
	virtual vector3 normalAt(const ray &r) const {return points_away_from(normal, r.dir);}
	bool is_thin() const {return true;}
	const char *type() const {return "plane";}
};

struct quad
{
	point3 origin;
	world_distance h, w;
	
	quad() : origin(), h(0), w(0) {}
};

struct camera
{
	point3 origin;
	quad screen;
	
	camera() : origin(), screen() {}
};

class raytracer;

struct scene
{
	primitive **prims;
	size_t primcount;
	media atmosphere;
	raytracer *parent;
	
	scene(raytracer *rt, primitive **pr, size_t primcount) : prims(pr), primcount(primcount), parent(rt) {atmosphere.transmittance = 1;}
};

#endif