/*
 *  scene.cpp
 *  atrace
 *
 *  Created by Alexander Strange on 6/23/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "scene.h"

//extern bool verbose_log;
#define verbose_log 0

intersectResult sphere::intersects(const ray &r, world_distance *dist, world_distance max, bool consider_close_miss) const
{ 
	intersectResult res = MISS;
	vector3 sc_v = origin - r.origin; // sphere center
	
	world_distance dist_sq = sc_v.dot_self(); // distance(r.origin, sphere.origin)
	world_distance dist_from_rad_sq = dist_sq - radSq;
	world_distance r_dist;
	world_distance closest_approach = sc_v.dot(r.dir); 
	
	if (dist_from_rad_sq > EPSILON) {
		if (closest_approach >= 0) { // ray is pointing towards sphere		
			world_distance half_cord = closest_approach*closest_approach - dist_from_rad_sq;
			if (half_cord > 0) {
				r_dist = closest_approach - sqrt(half_cord);
				if (above(r_dist, 0) && r_dist < max) {
					res = HIT;
					*dist = r_dist;
				}
			}
		}
	} else { //inside sphere		
		if (dist_from_rad_sq > -EPSILON && consider_close_miss) return MISS;
		
		world_distance half_cord = closest_approach*closest_approach - dist_from_rad_sq;
		r_dist = closest_approach + sqrt(half_cord);
		
		if (above(r_dist, 0) && r_dist < max) {
			res = HITINSIDE;
			*dist = r_dist; // far end of sphere
		}
	}
	
	return res;
}

intersectResult plane_prim::intersects(const ray &r, world_distance *cdist, world_distance max, bool consider_close_miss) const
{
	world_distance d = dot(normal, r.dir);
	
	if (!close(d, 0)) {
		world_distance col_dist = -(dot(normal, r.origin) + dist) / d;
		
		if (above(col_dist, 0) && col_dist < max) {
			*cdist = col_dist;
			return (d > 0) ? HIT : HITINSIDE;
		}
	}
	
	return MISS;
}

color4 checkerboard_texture::colorAt(world_distance u, world_distance v)
{
	int iu = lrint(u), iv = lrint(v);
	
	if ((iu+iv)&1) {
		return odd;
	} else {
		return even;
	}
}

color4 image_texture::pixelAt(size_t x, size_t y)
{
	return *pixelAddressAt(image, x, y, w);
}

static real gaussian(world_distance x, world_distance s)
{
	return exp(s ? -(x*x) / (2*s*s) : 0); // scale factor isn't correct but it has to be rescaled anyway
}

static void make_filter(real *filter, unsigned char support, world_distance pos)
{
	int i;
	world_distance p = ((1. - pos) - .5) - support;
	real sum = 0, rescale;
	
	if (verbose_log) printf("filter: p %f\n", p);
	
	for (i = -support; i <= support; i++) {
		real v = dmax(gaussian(p, (1. / sqrt(2.))), 0.);
		sum += v;
		filter[i + support] = v;
		
		p += 1.;
	}
	
	rescale = sum ? (1. / sum) : 1.;
	
	for (i = -support; i <= support; i++)
	{filter[i + support] *= rescale;if (verbose_log) printf("filter: %d %f -> %f\n", i, p, filter[i+support]);}
}

static inline color4 apply_filter_refine(image_texture *tex, size_t x, size_t y, real *filter_x, real *filter_y, uint8_t support, bool first = true, color4 previous = color4())
{
	const color Yf = color(.2126,.7152,.0722);
	color4 acc;
	world_distance luma = 0;
	
	if (!first) luma = dot(Yf, from_premultiplied(previous, NULL));
	
	for (int i = -support; i <= support; i++) {
		for (int j = -support; j <= support; j++) {
			real factor = filter_x[j + support] * filter_y[i + support];
			color4 p = tex->pixelAt(x+j, y+i);
			
			if (first) {
				acc += p * factor;
			} else {
				world_distance cY = dot(Yf, from_premultiplied(p, NULL));
				acc += ((fabs(luma - cY) < .4) ? p : previous) * factor; 
			}
		}
	}
	
	return acc;
}

static color4 apply_filter(image_texture *tex, size_t x, size_t y, real *filter_x, real *filter_y, uint8_t support, unsigned refinements = 1)
{
	color4 c = apply_filter_refine(tex, x, y, filter_x, filter_y, support);
	
	for (unsigned i = 0; i < refinements; i++) c = apply_filter_refine(tex, x, y, filter_x, filter_y, support, false, c);
	
	return c;
}

color4 image_texture::colorAt(world_distance u, world_distance v)
{
	if (verbose_log) printf("img: u %f v %f\n", u, v);
	
	if (v < 0 || v > fh || u < 0 || u > fw) {
		if (repeat) {
			bool rev_u = u < 0, rev_v = v < 0;
			u = fmod(fabs(u), fw+1.);
			v = fmod(fabs(v), fh+1.);
			if (rev_u) u = fw-u;
			if (rev_v) v = fh-v;
		} else {
			if (verbose_log) printf("img: ...is outside w %f h %f\n",fw,fh);
			return color4();
		}
	}
	
	u = fw-u;
	
	world_distance uf = floor(u), vf = floor(v);
	world_distance ud = u - uf, vd = v - vf;
	
	
	const unsigned char support = IMG_SUPPORT;
	if (verbose_log) printf("img: post-adj u %f v %f\n", u, v);

	real filter_x[support*2 + 1] = {0}, filter_y[support*2 + 1] = {0};

	make_filter(filter_x, support, ud);
	make_filter(filter_y, support, vd);
	color4 c = apply_filter(this, uf, vf, filter_x, filter_y, support);
		
	if (verbose_log) {printf("img: color res "); c.print();}
	
	return c;
}

static color4 color_of_placement(texture_placement *p, world_distance u, world_distance v)
{
	return p->tex->colorAt(u * p->uScale + p->uShift, v * p->vScale + p->vShift);
}

static color4 rec_colorOfTextureStackAt(texture_placement *textures, world_distance u, world_distance v, size_t i, color4 above)
{	
	color4 thisC = color_of_placement(&textures[i], u, v);
	
	if (verbose_log) {printf("tex recurse: i %d, this color ",i); thisC.print();}
	color4 resC = over(above, thisC);
	if (i == 0 || close(thisC.a, 1)) return resC;		
	return rec_colorOfTextureStackAt(textures, u, v, i-1, resC);
}

static color4 colorOfTextureStackAt(texture_placement *textures, world_distance u, world_distance v, size_t texcount)
{
	color4 thisC = color_of_placement(&textures[texcount-1], u, v);
	
	if (verbose_log) {printf("tex stack top: tcount %d, color ",texcount); thisC.print();}
	if (texcount == 1 || close(thisC.a, 1)) return thisC;
	return rec_colorOfTextureStackAt(textures, u, v, texcount-2, thisC);
}

color4 surface::colorAt(world_distance u, world_distance v)
{	
	return colorOfTextureStackAt(textures, u, v, texcount);
}