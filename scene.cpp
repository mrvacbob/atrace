/*
 * Copyright (c) Alex Strange
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

#include "scene.h"

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
		if (half_cord < 0) return MISS; // numerical edge: origin is on the surface within epsilon
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

