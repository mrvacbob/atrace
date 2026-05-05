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
#include "texture.h"
#include <vector>

struct media
{
	real refractive_index;
	real transmittance;   // fraction of surface energy that refracts (used for refract_weight)
	color absorption;     // per-channel Beer-Lambert: surviving fraction per world unit
	                      // (1,1,1)=clear, (0.96,0.97,1)=slight blue tint
	color emission;       // self-emission per world unit (approximates single-scattering/Mie glow)

	media() : refractive_index(1), transmittance(0), absorption(1.f), emission(0.f) {}
};

struct surface
{
	real reflect, diffuse, specular_exp, roughness;
	bool clear_reflect;
	bool dielectric; // dielectrics' reflectivity changes depending on the viewing angle
	bool metallic;   // conductor: reflection colored by per-channel Fresnel, no diffuse
	color conductor_n, conductor_k; // complex IOR for conductor Fresnel (used when metallic)

	texture_placement textures[16];
	int texcount;

	surface() : reflect(0.f), diffuse(1), specular_exp(40.f), roughness(0.f),
	            clear_reflect(true), dielectric(false), metallic(false),
	            conductor_n(0.f), conductor_k(0.f), texcount(0) {}
	surface(const surface &) = delete;
	surface &operator=(const surface &) = delete;

	color4 colorAt(world_distance u, world_distance v) const;
};

enum intersectResult {HITINSIDE=-1, MISS=0, HIT=1};

struct primitive
{
	point3 origin;
	surface mat;
	media med;
	bool light;
	vector3 uAxis, vAxis;

	primitive() : light(false) {}
	primitive(const point3 &origin, const vector3 &uAxis, const vector3 &vAxis) : origin(origin), light(false), uAxis(uAxis), vAxis(vAxis) {}
	virtual ~primitive() {}
	virtual intersectResult intersects(const ray &r, world_distance *dist, world_distance max = HUGE_VAL, bool consider_close_miss=false) const = 0;
	virtual vector3 normalAt(const ray &r) const = 0;
	virtual const char *type() const = 0;
	virtual void uvAt(const point3 &p, world_distance *u, world_distance *v) const {*u = p.dot(uAxis); *v = p.dot(vAxis);}
	color4 colorAt(const point3 &p) const {world_distance u, v; uvAt(p, &u, &v); return mat.colorAt(u, v);}
};

struct sphere : public primitive
{
	world_distance rad, radSq;

	sphere(const point3 &origin_, world_distance radius) : primitive(origin_, vector3(1,0,0), vector3(0,1,0)), rad(radius), radSq(radius*radius) {}

	intersectResult intersects(const ray &r, world_distance *dist, world_distance max = HUGE_VAL, bool consider_close_miss=false) const override;
	vector3 normalAt(const ray &r) const override {
		return points_away_from(normalize(r.origin - origin), r.dir);
	}
	const char *type() const override {return "sphere";}
};

struct plane
{
	point3 normal;
	world_distance dist;

	plane(const point3 &normal, world_distance d) : normal(normal), dist(d) {}
};

struct plane_prim : public primitive, plane
{
	plane_prim(const point3 &normal_, world_distance d_) : plane(normalize(normal_), d_)
	{
		vector3 ref = (fabsf(normal.x) < 0.9f) ? vector3(1,0,0) : vector3(0,1,0);
		uAxis = normalize(ref - normal * normal.dot(ref));
		vAxis = normal.cross(uAxis);
	}

	intersectResult intersects(const ray &r, world_distance *dist, world_distance max = HUGE_VAL, bool consider_close_miss=false) const override;
	vector3 normalAt(const ray &r) const override {return points_away_from(normal, r.dir);}
	const char *type() const override {return "plane";}
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

struct raytracer;

struct scene
{
	std::vector<std::unique_ptr<primitive>> owned;
	std::vector<primitive *> prims;
	std::vector<primitive *> lights;
	media atmosphere;
	raytracer *parent;

	explicit scene(raytracer *rt) : parent(rt) { atmosphere.transmittance = 1; }
	scene(const scene &) = delete;
	scene &operator=(const scene &) = delete;

	void add(std::unique_ptr<primitive> p) {
		primitive *raw = p.get();
		if (raw->light) lights.push_back(raw);
		prims.push_back(raw);
		owned.push_back(std::move(p));
	}
};
