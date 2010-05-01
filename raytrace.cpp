/*
 * Copyright (c) 2007 Alexander Strange <astrange@ithinksw.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "raytrace.h"
#include <stdio.h>


image::image(size_t w_, size_t h_)
{
	w = w_;
	h = h_;
	
	buf = new f_pixel[w*h];
	depth_buf = new f_real[w*h];
}

image::~image()
{
	delete[] buf;
	delete[] depth_buf;
}

//bool verbose_log=false;
#define verbose_log 0 

bool raytracer::light_reaches(primitive *light, primitive *pi, point3 &to, color *c, vector3 *L, media *medium, unsigned index)
{
	ray p_to_L = ray_from_to(to, light->origin);
	world_distance L_dist, test_dist=0;
	point3 from;
	
	light->intersects(p_to_L, &L_dist);
	from = p_to_L.pointAt(L_dist);
	/*
	primitive *test_p = find_primitive_along(p_to_L, &test_dist, false, NULL, NULL, L_dist);	
    if (test_p) return 0;
	if (c) *c = from_premultiplied(light->colorAt(from));
    */
    primitive *test_p = NULL;
    if (verbose_log) printf("%d: ---- lighting ------\n",index);
    color falling_on = trace(p_to_L, &test_dist, medium, NULL, NULL, index, &test_p);
     if (verbose_log) printf("%d: ---- lighting end ------\n",index);
    if (!test_p) return 0;
    if (c) *c = falling_on;
    if (L) *L = p_to_L.dir;
	return 1;
}

static real fresnelR(const ray &r, const vector3 &N, real n1, real n2)
{
	world_distance nRatio = n1/n2;
	world_distance cosI = -r.dir.dot(N);
	world_distance cosTsq = 1. - nRatio*nRatio * (1. - cosI*cosI);

	if (cosTsq >= 0) {
		world_distance cosT = sqrt(cosTsq);
		world_distance n1cosI = n1*cosI, n1cosT = n1*cosT,
					   n2cosI = n2*cosI, n2cosT = n2*cosT;
		
		world_distance Rssq = (n1cosI - n2cosT) / (n1cosI + n2cosT);
		world_distance Rpsq = (n1cosT - n2cosI) / (n1cosT + n2cosI);

		return (Rpsq*Rpsq + Rssq*Rssq)/2;
	}
	
	return 1;
}

color raytracer::color_of_primitive_at(const ray &r, world_distance dist, primitive *pi, media *medium, unsigned index, intersectResult res, primitive **backtracking)
{
	color phong, reflected, refracted;
	real reflect_weight = 0, refract_weight = 0, absorbance_factor = 1;	
	ray tray = r.rayAt(dist);
	point3 p = tray.origin;
	real local_alpha;
	color local_color(from_premultiplied(pi->colorAt(p), &local_alpha));
		
	vector3 N = pi->normalAt(tray);
	
	bool any_phong_lighting = false;
	media *exiting_medium = medium, *entering_medium = (res == HITINSIDE) ? &sc.atmosphere : &pi->med;
	
	if (verbose_log) printf("trace %d: %s %p (%d) at (%f, %f, %f), normal (%f, %f, %f), exiting RI %f entering RI %f\n", index, pi->type(), pi, res,
							p.x, p.y, p.z, N.x, N.y, N.z, exiting_medium->refractive_index, entering_medium->refractive_index);
    
    if (pi->light) {if (verbose_log) printf("%d: is a light!\n", index); if (backtracking) *backtracking = pi; return local_color;}

	if (!backtracking) {
		if (verbose_log) printf("%d: lighting\n", index);
		
		FOR_EACH_LIGHT();
		{
			vector3 L;
			color light_color;
			bool light_reached = light_reaches(light, pi, p, &light_color, &L, medium, index);
			
			if (light_reached) {
				real phong_diff = dot(N, L);
				vector3 R = L.reflect(N);
				real phong_spec = dot(r.dir, R);
				
				color diffusec;
				color specularc;
				
				if (phong_diff > 0) diffusec  = light_color * local_color * phong_diff;
				if (phong_spec > 0) specularc = (pi->mat.clear_reflect ? light_color : local_color) * pow(phong_spec, pi->mat.specular_exp);
				
				phong += blend(diffusec, specularc, pi->mat.diffuse);
				any_phong_lighting=true;
			}
		}
		FOR_EACH_LIGHT_END();
	}
									  
	if (pi->mat.dielectric) {
		reflect_weight = fresnelR(tray, N, exiting_medium->refractive_index, entering_medium->refractive_index);
		refract_weight = (1. - reflect_weight);
	} else {
		reflect_weight = pi->mat.reflect;
		refract_weight = dmax(pi->med.transmittance,1.-local_alpha); // XXX wrong
	}
	
	if (verbose_log) printf("%d: R %f T %f\n",index,reflect_weight, refract_weight);
	
	if (reflect_weight > 0) {
		ray reflect_ray(p, r.dir.reflect(N), false);
		if (verbose_log) printf("%d: reflection\n", index);
		reflected = trace(reflect_ray, NULL, medium, NULL, NULL, index, backtracking);
		if (!pi->mat.clear_reflect) reflected *= local_color;
	}
	
	if (refract_weight > 0) {
		if (verbose_log) printf("%d: transparency\n", index);
		world_distance nRatio = exiting_medium->refractive_index / entering_medium->refractive_index;
		world_distance cosT1 = r.dir.dot(N);
		world_distance cosT2sq = 1. - (nRatio*nRatio)*(1. - cosT1*cosT1);
		
		absorbance_factor = 1;

		if (cosT2sq >= 0) {
			world_distance cosT2 = sqrt(cosT2sq), traced_dist;
			vector3 refract_dir = (r.dir * nRatio) - N*(cosT2 + nRatio*cosT1);
			ray refract_ray(p, refract_dir, false);
						
			refracted = trace(refract_ray, &traced_dist, entering_medium, NULL, NULL, index, backtracking);
			
			if (medium->transmittance != 1) {
				absorbance_factor = exp(-((1.-medium->transmittance) * traced_dist)); 
				refracted *= local_color; // XXX wrong
			}
			if (verbose_log) printf("%d: traced_dist %f, transmitted result %f\n", index, traced_dist, absorbance_factor);
		}
		
	}
	
	refract_weight *= absorbance_factor;
	
	return (phong * local_alpha) + (reflected * reflect_weight) + (refracted * refract_weight);
}

primitive *raytracer::find_primitive_along(const ray &r, world_distance *col_dist, bool closest, primitive *ignore, intersectResult *restype, world_distance max_dist, bool consider_close_miss)
{
	primitive *res = NULL;
	world_distance res_dist = max_dist;
	
	if (restype) *restype = MISS;
	if (verbose_log) {
		printf("find_primitive_along ray from (%f,%f,%f) dir (%f,%f,%f), max_dist %f, finding closest %d\n", r.origin.x, r.origin.y, r.origin.z, 
			   r.dir.x, r.dir.y, r.dir.z, max_dist, closest);
	}
	
	for (size_t i = 0; i < sc.primcount; i++) {
		primitive *p = sc.prims[i];
				
		if (p == ignore) {
			if (verbose_log) printf("\tignoring %s %p at (%f,%f,%f)\n", p->type(), p, p->origin.x, p->origin.y, p->origin.z);
			continue;}
		
		world_distance tdist = res_dist;		
		intersectResult ires = p->intersects(r, &tdist, res_dist, consider_close_miss);

		if (ires != MISS) {
			if (verbose_log) printf("\thit (%d) %s %p at (%f, %f, %f) dist %f\n", ires, p->type(), p, p->origin.x, p->origin.y, p->origin.z, tdist);
			res = p; res_dist = tdist;
			if (restype) *restype = ires;
			if (!closest) break;
		}
	}
	
	if (verbose_log) {
		if (!res) printf("no hit\n\n");
		else printf("\n");
	}
	
	if (res && col_dist) *col_dist = res_dist;

	return res;
}

color raytracer::trace(const ray &r, world_distance *dist, media *medium, primitive *ignore, intersectResult *res, unsigned index, primitive **backtracking)
{
	if (index == 0 || index > TRACE_DEPTH) {if (dist) *dist = HUGE_VAL; return color(0.);}
	world_distance col_dist;
	intersectResult ires;
	color c(background);
	
	primitive *p = find_primitive_along(r, &col_dist, true, ignore, &ires);	
	
	if (p) {
		if (dist) *dist = col_dist;
		if (res) *res = ires;
				
		c = color_of_primitive_at(r, col_dist, p, medium, index+1, ires, backtracking);
	} else if (dist) *dist = HUGE_VAL;
	
	return c;
}

image *raytracer::render(size_t w, size_t h, const camera &cam)
{
	image *target = new image(w,h);
	world_distance dw = w-1, dh = h-1;
	world_distance dy = -(cam.screen.h/dh), dx = cam.screen.w/dw;
	point3 screen_ul = cam.screen.origin + point3(-(cam.screen.w/2.),cam.screen.h/2.,0);
	
	#pragma omp parallel for
	for (size_t y=0; y < h; y++) {
		for (size_t x=0; x < w; x++) {
			//if ((x == 841 && y == 313)) verbose_log=true;
			world_distance dist = 0;//,mdist=0;
			/*world_distance gauss_side = exp(-.5 * (.5*.5 + .5*.5)), gauss_mid = exp(0.);
			int aa=0;
			color mc;
			
			for (int ay=0; ay < 2; ay++) {
				for (int ax=0; ax < 2; ax++) {
					world_distance adist;
					point3 eye = screen_ul + point3(dx * (x+ax), dy * (y+ay), 0);
					ray r = ray_from_to(cam.origin,eye);
					color ac = trace(r, &adist, &sc.atmosphere);
					mc += ac*gauss_side;
					mdist += adist*gauss_side;
					aa++;
				}
			}
			*/
			point3 eye = screen_ul + point3(dx * (x+.5), dy * (y+.5),0);
			ray r = ray_from_to(cam.origin,eye);
			color c = trace(r, &dist, &sc.atmosphere);
/*
			c += mc;
			dist += mdist;
			
			c /= gauss_mid + (gauss_side*6.);
			dist /= gauss_mid + (gauss_side*6.);
*/			
			f_pixel outc(c.r,c.g,c.b); 
			target->set(x,y, outc, dist);
            //verbose_log=false;
		}
	}
	
	target->finish();
	return target;
}