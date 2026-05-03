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

static primitive **checkerboard_scene(size_t *pi)
{
	primitive **prims = new primitive*[6];

	// Floor: horizontal plane at y=-2, purely diffuse B&W checkerboard
	prims[0] = new plane_prim(vector3(0,1,0), 2);
	// Back wall: vertical plane at z=15, red/blue checker overlaid with tiled image
	prims[1] = new plane_prim(vector3(0,0,-1), 15);
	// Left sphere: dark blueish glass — semi-transparent, Beer-Lambert gives blue tint
	prims[2] = new sphere(vector3(-2.4f, 1.8f, 3), 2);
	// Right sphere: clear glass dielectric (ior 1.1), full refraction via Fresnel
	prims[3] = new sphere(vector3(2.4f, 1.8f, 3), 2);
	// Light: tiny bright sphere — acts as area light via backtracking caustics hack
	prims[4] = new sphere(vector3(0, 10, -1), .2f);
	// Rear sphere: gold — highly reflective, tinted reflection, minimal diffuse
	prims[5] = new sphere(vector3(0, -.2f, 7), 2);

	prims[0]->mat.textures[prims[0]->mat.texcount++].tex = std::make_unique<checkerboard_texture>(color4(.05f,.05f,.05f), color4(.9f,.9f,.9f));
	prims[0]->mat.textures[0].uScale = prims[0]->mat.textures[0].vScale = 2.f;  // 0.5-unit tiles
	prims[0]->mat.diffuse = 1;

	prims[1]->mat.textures[prims[1]->mat.texcount++].tex = std::make_unique<checkerboard_texture>(color4(1,.1f,.1f), color4(.1f,.15f,1));
	prims[1]->mat.textures[prims[1]->mat.texcount++].tex = std::make_unique<image_texture>("caro.png", true);
	prims[1]->mat.textures[1].uScale = prims[1]->mat.textures[1].vScale = 50.f;  // tile image at 50px/unit
	prims[1]->mat.textures[1].vShift = 12*50;
	prims[1]->mat.textures[1].uShift = 12*50;
	prims[1]->mat.diffuse = .7f;

	set_color(prims[2], color4(.05f,.07f,.2f));
	prims[2]->mat.reflect = 0.005f;       // very faint bubble-surface reflection
	prims[2]->mat.clear_reflect = true;
	prims[2]->med.transmittance = .8f;
	prims[2]->med.absorption = color(.167f, .275f, .684f); // exp(-k) for k=(1.79,1.29,0.38)
	prims[2]->med.emission = color(0.f, 0.0046f, 0.0575f); // inky blue glow (+15%)
	prims[2]->mat.diffuse = 1.0f;         // diffuse surface, no specular highlight

	set_color(prims[3], color4(.7f,.7f,.7f));
	prims[3]->mat.reflect = 0;
	prims[3]->med.transmittance = 1;
	prims[3]->mat.diffuse = .1f;
	prims[3]->med.refractive_index = 1.5f;
	prims[3]->mat.dielectric = true;

	set_color(prims[4], color4(5,5,5));
	prims[4]->light = true;

	set_color(prims[5], color4(.85f,.65f,.15f));  // gold: warm yellow, slightly orange
	prims[5]->mat.diffuse = .05f;
	prims[5]->mat.reflect = .9f;
	prims[5]->mat.clear_reflect = false;  // reflection tinted by gold color

	*pi = 6;
	return prims;
}

int main(int argc, char * const argv[])
{
#ifdef DEBUG_FP
	enable_fp_exceptions();
#endif
	size_t w = 1280, h = 960;
	camera cam;
	size_t primi;

	primitive **prims = checkerboard_scene(&primi);
	raytracer tr(prims, primi);
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
