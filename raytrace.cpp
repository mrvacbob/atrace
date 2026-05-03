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

#include "raytrace.h"
#include <cstdio>

static constexpr bool verbose_log = false;

image::image(size_t w_, size_t h_)
{
	w = w_;
	h = h_;
	buf       = std::make_unique<f_pixel[]>(w*h);
	depth_buf = std::make_unique<f_real[]>(w*h);
}

static real fresnelR(const ray &r, const vector3 &N, real n1, real n2)
{
	world_distance nRatio = n1/n2;
	world_distance cosI = -r.dir.dot(N);
	world_distance cosTsq = 1.f - nRatio*nRatio * (1.f - cosI*cosI);

	if (cosTsq >= 0) {
		world_distance cosT = sqrtf(cosTsq);
		// Variables named Rssq/Rpsq are actually the amplitude coefficients Rs/Rp,
		// squared on the following line to get reflectance.
		world_distance n1cosI = n1*cosI, n1cosT = n1*cosT,
		               n2cosI = n2*cosI, n2cosT = n2*cosT;
		world_distance Rs = (n1cosI - n2cosT) / (n1cosI + n2cosT);
		world_distance Rp = (n1cosT - n2cosI) / (n1cosT + n2cosI);
		return (Rp*Rp + Rs*Rs) / 2.f;
	}

	return 1.f; // total internal reflection
}

bool raytracer::light_reaches(primitive *light, primitive *pi, const point3 &to, color *c, vector3 *L, const media *medium, unsigned index) const
{
	// Fast path: try center of light sphere first.
	// This handles the common unobstructed case in one trace call.
	ray p_to_L = ray_from_to(to, light->origin);
	primitive *test_p = nullptr;
	color falling_on = trace(p_to_L, nullptr, medium, nullptr, nullptr, index, &test_p);
	if (test_p) {
		if (c) *c = falling_on;
		if (L) *L = p_to_L.dir;
		return true;
	}

	// Centre missed.  For sphere lights, try 4 offset positions on the equator
	// facing the shaded point — this lets refracted caustics through the glass
	// sphere show up: some offset targets are reachable via refraction even when
	// the center isn't.  Only costs extra traces in shadow / caustic regions.
	const sphere *ls = dynamic_cast<const sphere *>(light);
	if (!ls || ls->rad == 0.f) return false;

	world_distance r = ls->rad;
	vector3 d = normalize(light->origin - to);
	vector3 t1 = normalize((fabsf(d.x) < 0.9f ? vector3(1,0,0) : vector3(0,1,0)).cross(d));
	vector3 t2 = d.cross(t1);

	static const float off[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
	color acc;
	bool any_hit = false;
	for (auto &o : off) {
		point3 target = light->origin + t1 * (r * o[0]) + t2 * (r * o[1]);
		primitive *tp = nullptr;
		color hit = trace(ray_from_to(to, target), nullptr, medium, nullptr, nullptr, index, &tp);
		if (tp) { acc += hit; if (!any_hit) { if (L) *L = ray_from_to(to, target).dir; any_hit = true; } }
	}
	if (!any_hit) return false;
	if (c) *c = acc * (1.f / 5);  // average over all 5 samples (center + 4)
	return true;
}

color raytracer::color_of_primitive_at(const ray &r, world_distance dist, primitive *pi, const media *medium, unsigned index, intersectResult res, primitive **backtracking) const
{
	color phong, reflected, refracted, dielectric_specular;
	real reflect_weight = 0, refract_weight = 0;
	ray tray = r.rayAt(dist);
	point3 p = tray.origin;
	real local_alpha;
	color local_color(from_premultiplied(pi->colorAt(p), &local_alpha));

	vector3 N = pi->normalAt(tray);

	const media *exiting_medium = medium;
	const media *entering_medium = (res == HITINSIDE) ? &sc.atmosphere : &pi->med;

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
				real phong_spec = -dot(r.dir, R);

				color diffusec, specularc;

				if (phong_diff > 0) diffusec  = light_color * local_color * phong_diff;
				if (phong_spec > 0) specularc = (pi->mat.clear_reflect ? light_color : local_color) * powf(phong_spec, pi->mat.specular_exp);

				phong += blend(diffusec, specularc, pi->mat.diffuse);
				if (pi->mat.dielectric) dielectric_specular += specularc;
			}
		}
		FOR_EACH_LIGHT_END();
	}

	if (pi->mat.dielectric) {
		reflect_weight = fresnelR(tray, N, exiting_medium->refractive_index, entering_medium->refractive_index);
		refract_weight = 1.f - reflect_weight;
	} else {
		reflect_weight = pi->mat.reflect;
		refract_weight = pi->med.transmittance;
	}

	if (verbose_log) printf("%d: R %f T %f\n", index, reflect_weight, refract_weight);

	if (reflect_weight > 0) {
		ray reflect_ray(p + N * EPSILON, r.dir.reflect(N), false);
		if (verbose_log) printf("%d: reflection\n", index);
		reflected = trace(reflect_ray, nullptr, medium, nullptr, nullptr, index, backtracking);
		if (!pi->mat.clear_reflect) reflected *= local_color;
		// Dielectric Phong highlight via reflected channel: the mirror ray rarely hits
		// the 0.2-radius light sphere, so add the Phong specular lobe here instead.
		// Gets correctly Fresnel-scaled (~4% at normal incidence, 100% at grazing)
		// so it's subtle directly but gives a bright rim; consistent in secondary views.
		// In a full path tracer this would be MIS / next-event estimation.
		if (pi->mat.dielectric) reflected += dielectric_specular;
	}

	if (refract_weight > 0) {
		if (verbose_log) printf("%d: transparency\n", index);
		world_distance nRatio = exiting_medium->refractive_index / entering_medium->refractive_index;
		world_distance cosT1 = r.dir.dot(N);
		world_distance cosT2sq = 1.f - (nRatio*nRatio)*(1.f - cosT1*cosT1);

		if (cosT2sq >= 0) {
			world_distance cosT2 = sqrtf(cosT2sq), traced_dist;
			vector3 refract_dir = (r.dir * nRatio) - N*(cosT2 + nRatio*cosT1);
			ray refract_ray(p - N * EPSILON, refract_dir, false);

			refracted = trace(refract_ray, &traced_dist, entering_medium, nullptr, nullptr, index, backtracking);

			// Per-channel Beer-Lambert: apply the *entering* medium's absorption
			// over traced_dist, which at HIT is the chord through the medium interior.
			// Using entering_medium (not exiting medium) ensures absorption is applied
			// while traveling through the glass, not over the post-exit distance.
			const color &ac = entering_medium->absorption;
			color abs_factor(powf(ac.r, traced_dist),
			                 powf(ac.g, traced_dist),
			                 powf(ac.b, traced_dist));
			refracted *= abs_factor;

			// Emission integral: em * (1 - exp(-k*L)) / k = em * (1 - abs_factor) / k
			// where k = -log(ac). Approximates single-scattering / inky self-glow —
			// center (long path) accumulates more emission than edges (short path).
			const color &em = entering_medium->emission;
			if (em.r > 0.f || em.g > 0.f || em.b > 0.f) {
				auto emit_ch = [](float e, float a, float absfac) -> float {
					if (a >= 1.f - 1e-5f) return 0.f;           // no absorption → no path-length glow
					return e * (1.f - absfac) / -logf(a);
				};
				refracted += color(emit_ch(em.r, ac.r, abs_factor.r),
				                   emit_ch(em.g, ac.g, abs_factor.g),
				                   emit_ch(em.b, ac.b, abs_factor.b));
			}

			if (verbose_log) printf("%d: traced_dist %f\n", index, traced_dist);
		}
	}

	real surface_weight = dmax(0.f, 1.f - reflect_weight - refract_weight);
	return phong * surface_weight + reflected * reflect_weight + refracted * refract_weight;
}

primitive *raytracer::find_primitive_along(const ray &r, world_distance *col_dist, bool closest, primitive *ignore, intersectResult *restype, world_distance max_dist, bool consider_close_miss) const
{
	primitive *res = nullptr;
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
			continue;
		}

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

color raytracer::trace(const ray &r, world_distance *dist, const media *medium, primitive *ignore, intersectResult *res, unsigned index, primitive **backtracking) const
{
	if (index == 0 || index > static_cast<unsigned>(TRACE_DEPTH)) {if (dist) *dist = HUGE_VAL; return color(0.f);}
	world_distance col_dist;
	intersectResult ires;
	color c = backtracking ? color(0.f) : background;

	primitive *p = find_primitive_along(r, &col_dist, true, ignore, &ires);

	if (p) {
		if (dist) *dist = col_dist;
		if (res) *res = ires;
		c = color_of_primitive_at(r, col_dist, p, medium, index+1, ires, backtracking);
	} else if (dist) *dist = HUGE_VAL;

	return c;
}

std::unique_ptr<image> raytracer::render(size_t w, size_t h, const camera &cam) const
{
	auto target = std::make_unique<image>(w, h);
	world_distance dw = static_cast<world_distance>(w) - 1;
	world_distance dh = static_cast<world_distance>(h) - 1;
	world_distance dy = -(cam.screen.h/dh), dx = cam.screen.w/dw;
	point3 screen_ul = cam.screen.origin + point3(-(cam.screen.w/2.f), cam.screen.h/2.f, 0);

	// Gaussian-weighted supersampling (5 rays per pixel).
	// Centre ray at (x+0.5, y+0.5): weight 1.0.
	// Four quadrant rays at offsets (0.25,0.25),(0.25,0.75),(0.75,0.25),(0.75,0.75): weight w_q
	//   (d² = 0.25²+0.25² = 0.125, sigma = 0.35 → w_q = exp(-0.125/(2·0.35²)) ≈ 0.601).

	static constexpr real sigma2 = 2.f * 0.35f * 0.35f;   // 2σ²
	const real w_q     = expf(-0.125f / sigma2);           // ≈ 0.601
	const real w_total = 1.f + 4.f * w_q;

	static const world_distance offsets[2] = {0.25f, 0.75f};

	for (size_t y = 0; y < h; y++) {
		for (size_t x = 0; x < w; x++) {
			world_distance dist_ctr;
			point3 eye_ctr = screen_ul + point3(dx * (x + 0.5f), dy * (y + 0.5f), 0);
			color acc = trace(ray_from_to(cam.origin, eye_ctr), &dist_ctr, &sc.atmosphere);
			world_distance dist_acc = dist_ctr;

			for (world_distance oy : offsets) {
				for (world_distance ox : offsets) {
					world_distance dist;
					point3 eye = screen_ul + point3(dx * (x + ox), dy * (y + oy), 0);
					acc      += trace(ray_from_to(cam.origin, eye), &dist, &sc.atmosphere) * w_q;
					dist_acc += dist * w_q;
				}
			}
			acc      = acc      * (1.f / w_total);
			dist_acc = dist_acc * (1.f / w_total);
			target->set(x, y, f_pixel(acc.r, acc.g, acc.b), dist_acc);
		}
	}

	target->finish();
	return target;
}
