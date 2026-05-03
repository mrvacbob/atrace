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

	prims[0] = new plane_prim(vector3(0,1,0), 2);
	prims[1] = new plane_prim(vector3(0,0,-1), 15);
	prims[2] = new sphere(vector3(-2.4f, 1.8f, 3), 2);
	prims[3] = new sphere(vector3(2.4f, 1.8f, 3), 2);
	prims[4] = new sphere(vector3(0, 10, -1), .2f);
	prims[5] = new sphere(vector3(0, -.2f, 7), 2);

	set_color(prims[0], color4(.7f,.7f,.7f));
	prims[0]->mat.diffuse = 1;

	prims[1]->mat.textures[prims[1]->mat.texcount++].tex = std::make_unique<checkerboard_texture>(color4(1,.1f,.1f), color4(.1f,.15f,1));
	prims[1]->mat.textures[prims[1]->mat.texcount++].tex = std::make_unique<image_texture>("caro.png", true);
	prims[1]->mat.textures[1].uScale = prims[1]->mat.textures[1].vScale = 50.f;
	prims[1]->mat.textures[1].vShift = 12*50;
	prims[1]->mat.textures[1].uShift = 12*50;
	prims[1]->mat.diffuse = .7f;

	set_color(prims[2], color4(.7f,.9f,.7f));
	prims[2]->mat.reflect = 0;
	prims[2]->med.transmittance = .8f;
	prims[2]->mat.diffuse = .1f;

	set_color(prims[3], color4(.7f,.7f,.7f));
	prims[3]->mat.reflect = 0;
	prims[3]->med.transmittance = 1;
	prims[3]->mat.diffuse = .1f;
	prims[3]->med.refractive_index = 1.1f;
	prims[3]->mat.dielectric = true;

	set_color(prims[4], color4(5,5,5));
	prims[4]->light = true;

	set_color(prims[5], color4(.75f,.75f,.4f));
	prims[5]->mat.diffuse = .1f;
	prims[5]->mat.reflect = .5f;
	prims[5]->mat.clear_reflect = false;

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

	cam.origin = point3(0, 0, -5);
	cam.screen.origin = point3(0, 0, 0);
	cam.screen.w = 8;
	cam.screen.h = 6;

	//run_tests();

	tr.render(w, h, cam)->write_to_bmp("scene.bmp");

	return 0;
}
