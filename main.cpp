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
#include "tests.h"
#ifdef DEBUG_FP
#include <cfenv>
static void enable_fp_exceptions()
{
	fenv_t fenv;
	fegetenv(&fenv);
#if defined(__x86_64__)
	fenv.__control &= ~(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
	fenv.__mxcsr   &= ~((FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW) << 7);
#elif defined(__aarch64__)
	fenv.__fpcr    |= (1 << 8) | (1 << 9) | (1 << 10); // IOE | DZE | OFE
#endif
	fesetenv(&fenv);
}
#endif

static void set_color(primitive *p, const color4 &c)
{
	p->mat.textures[0].tex = std::make_unique<flat_texture>(c);
	p->mat.texcount = 1;
}

static void checkerboard_scene(scene &sc)
{
	// Floor: horizontal plane at y=-2, purely diffuse B&W checkerboard
	auto p0 = std::make_unique<plane_prim>(vector3(0,1,0), 2);
	p0->mat.textures[p0->mat.texcount++].tex = std::make_unique<checkerboard_texture>(color4(.05f,.05f,.05f), color4(.9f,.9f,.9f));
	p0->mat.textures[0].uScale = p0->mat.textures[0].vScale = 2.f;  // 0.5-unit tiles
	p0->mat.diffuse = 1;
	sc.add(std::move(p0));

	// Back wall: vertical plane at z=15, red/blue checker overlaid with tiled image
	auto p1 = std::make_unique<plane_prim>(vector3(0,0,-1), 15);
	p1->mat.textures[p1->mat.texcount++].tex = std::make_unique<checkerboard_texture>(color4(1,.1f,.1f), color4(.1f,.15f,1));
	p1->mat.textures[p1->mat.texcount++].tex = std::make_unique<image_texture>("caro.png", true);
	p1->mat.textures[1].uScale = p1->mat.textures[1].vScale = 50.f;  // tile image at 50px/unit
	p1->mat.textures[1].vShift = 12*50;
	p1->mat.textures[1].uShift = 12*50;
	p1->mat.diffuse = .7f;
	sc.add(std::move(p1));

	// Left sphere: dark blueish glass — semi-transparent, Beer-Lambert gives blue tint
	auto p2 = std::make_unique<sphere>(vector3(-2.4f, 1.8f, 3), 2);
	set_color(p2.get(), color4(.05f,.07f,.2f));
	p2->mat.reflect = 0.005f;       // very faint bubble-surface reflection
	p2->mat.clear_reflect = true;
	p2->med.transmittance = .8f;
	p2->med.absorption = color(.167f, .275f, .684f); // exp(-k) for k=(1.79,1.29,0.38)
	p2->med.emission = color(0.f, 0.0046f, 0.0575f); // inky blue glow (+15%)
	p2->mat.diffuse = 1.0f;         // diffuse surface, no specular highlight
	sc.add(std::move(p2));

	// Right sphere: clear glass dielectric (ior 1.5), full refraction via Fresnel
	auto p3 = std::make_unique<sphere>(vector3(2.4f, 1.8f, 3), 2);
	set_color(p3.get(), color4(.7f,.7f,.7f));
	p3->mat.reflect = 0;
	p3->med.transmittance = 1;
	p3->mat.diffuse = .1f;
	p3->med.refractive_index = 1.5f;
	p3->mat.dielectric = true;
	sc.add(std::move(p3));

	// Light: tiny bright sphere — acts as area light via backtracking caustics hack
	auto p4 = std::make_unique<sphere>(vector3(0, 10, -1), .2f);
	set_color(p4.get(), color4(5,5,5));
	p4->light = true;
	sc.add(std::move(p4));

	// Rear sphere: gold — highly reflective, tinted reflection, minimal diffuse
	auto p5 = std::make_unique<sphere>(vector3(0, -.2f, 7), 2);
	set_color(p5.get(), color4(.85f,.65f,.15f));  // gold: warm yellow, slightly orange
	p5->mat.diffuse = .05f;
	p5->mat.reflect = .9f;
	p5->mat.clear_reflect = false;  // reflection tinted by gold color
	p5->mat.roughness = 0.03f;      // 3% roughness — slightly blurred reflections
	sc.add(std::move(p5));
}

int main(int argc, char * const argv[])
{
#ifdef DEBUG_FP
	enable_fp_exceptions();
#endif
	size_t w = 1280, h = 960;
	camera cam;

	raytracer tr;
	checkerboard_scene(tr.sc);
	tr.background = color(0.28f, 0.30f, 0.35f);  // cool overcast sky grey

	cam.origin = point3(0, 0, -5);
	cam.screen.origin = point3(0, 0, 0);
	cam.screen.w = 8;
	cam.screen.h = 6;

	//run_tests();

	auto img = tr.render(w, h, cam);
	img->write_to_bmp("scene.bmp");
	img->write_to_hdr("scene.hdr");

	return 0;
}
