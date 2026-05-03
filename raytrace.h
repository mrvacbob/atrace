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

#pragma once
#include "scene.h"
#include <memory>

struct image
{
	std::unique_ptr<f_pixel[]> buf;
	std::unique_ptr<f_real[]> depth_buf;
	f_real minv, maxv;
	size_t w, h;

	image(size_t w, size_t h);
	image(const image &) = delete;
	image &operator=(const image &) = delete;

	void write_to_bmp(const char *path) const;

	inline void set(size_t x, size_t y, const f_pixel p, f_real depth) {
		buf[y*w + x] = p;
		depth_buf[y*w + x] = depth;
	}

	void finish();

	f_pixel autolevel(const f_pixel &p) const {
		return p.range_fit(minv, maxv);
	}
};

struct raytracer
{
	scene sc;
	color background;

	raytracer(primitive **pr, size_t primcount) : sc(this,pr,primcount), background(0.f) {}

	bool light_reaches(primitive *light, primitive *pi, const point3 &to, color *c, vector3 *L, const media *medium, unsigned index) const;
	color color_of_primitive_at(const ray &r, world_distance dist, primitive *pi, const media *medium, unsigned index, intersectResult res, primitive **backtracking=nullptr) const;
	primitive *find_primitive_along(const ray &r, world_distance *col_dist, bool closest = true, primitive *ignore = nullptr, intersectResult *restype = nullptr, world_distance max_dist = HUGE_VAL, bool consider_close_miss=false) const;
	color trace(const ray &r, world_distance *dist_reached, const media *medium, primitive *ignore = nullptr, intersectResult *res = nullptr, unsigned index=1, primitive **backtracking=nullptr) const;

	std::unique_ptr<image> render(size_t w, size_t h, const camera &cam) const;
};

#define FOR_EACH_LIGHT() for (size_t i = 0; i < sc.primcount; i++) {primitive *light = sc.prims[i]; if (light->light) {
#define FOR_EACH_LIGHT_END() }}
