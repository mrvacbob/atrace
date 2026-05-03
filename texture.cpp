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
#include <png.h>
#include <cstdio>
#include <cstdlib>

static constexpr bool verbose_log = false;

// --- PNG loading helpers ---

static void image_8bit_to_fp(const uint8_t *im8, color4 *imf, uint8_t channels, size_t w, size_t h)
{
	for (size_t y = 0; y < h; y++)
		for (size_t x = 0; x < w; x++) {
			real r, g, b, a = 1;
			color4 *pf = pixelAddressAt(imf, x, y, w);
			const uint8_t *p8 = &im8[(y * w * channels) + (x * channels)];
			r = colorToL(p8[0]);
			g = colorToL(p8[1]);
			b = colorToL(p8[2]);
			if (channels == 4) a = p8[3] / 255.f;
			color4 pm = to_premultiplied(color(r,g,b), a);
			*pf = pm;
		}
}

static size_t wrap(ssize_t i, ssize_t off, ssize_t m, bool modulo)
{
	i += off;
	if (modulo) {
		i = (i < 0) ? (i + m) : ((i >= m) ? (i - m) : i);
	} else i = dmin(dmax(i, (ssize_t)0), m-1);
	return static_cast<size_t>(i);
}

static void fill_edges(color4 *image, ssize_t w, ssize_t h, bool repeat)
{
	for (ssize_t y = -IMG_SUPPORT; y < (h + IMG_SUPPORT); y++)
		for (ssize_t x = -IMG_SUPPORT; x < (w + IMG_SUPPORT); x++) {
			color4 *edge = pixelAddressAt(image, x, y, w), *im = pixelAddressAt(image, wrap(x, 0, w, repeat), wrap(y, 0, h, repeat), w);
			if (edge == im) continue;
			*edge = *im;
		}
}

image_texture::image_texture(const char *name, bool repeat) : repeat(repeat)
{
	FILE *f = fopen(name, "rb");
	uint8_t sig[8];

	if (!f) {
		fprintf(stderr, "Couldn't load texture %s.\n", name);
		exit(1);
	}

	if (fread(sig, 1, 8, f) != 8 || !png_check_sig(sig, 8)) {
		fprintf(stderr, "Not a valid PNG: %s\n", name);
		fclose(f);
		return;
	}

	png_structp pngs = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop pngi = png_create_info_struct(pngs);
	if (!pngi) {
		png_destroy_read_struct(&pngs, nullptr, nullptr);
		fclose(f);
		return;
	}

	if (setjmp(png_jmpbuf(pngs))) {
		png_destroy_read_struct(&pngs, &pngi, nullptr);
		fclose(f);
		return;
	}

	png_init_io(pngs, f);
	png_set_sig_bytes(pngs, 8);
	png_set_keep_unknown_chunks(pngs, 0, nullptr, 0);
	png_read_info(pngs, pngi);

	int bit_depth, color_type;
	png_uint_32 pw, ph;

	png_get_IHDR(pngs, pngi, &pw, &ph, &bit_depth, &color_type, nullptr, nullptr, nullptr);

	w = static_cast<ssize_t>(pw);
	h = static_cast<ssize_t>(ph);

	uint8_t channels = png_get_channels(pngs, pngi);

	size_t rb8 = static_cast<size_t>(w) * channels;
	size_t rb  = static_cast<size_t>(w) + IMG_SUPPORT*2;
	auto image8 = std::make_unique<uint8_t[]>(rb8 * static_cast<size_t>(h));
	image = std::make_unique<color4[]>(rb * (static_cast<size_t>(h) + IMG_SUPPORT*2));

	{
		auto rows = std::make_unique<png_bytep[]>(static_cast<size_t>(h));
		for (ssize_t i = 0; i < h; i++)
			rows[i] = &image8[rb8 * static_cast<size_t>(i)];
		png_read_image(pngs, rows.get());
	}

	png_destroy_read_struct(&pngs, &pngi, nullptr);
	fclose(f);

	fh = static_cast<world_distance>(h - 1);
	fw = static_cast<world_distance>(w - 1);

	image_8bit_to_fp(image8.get(), image.get(), channels, static_cast<size_t>(w), static_cast<size_t>(h));
	fill_edges(image.get(), w, h, repeat);
}

// --- Texture sampling ---

color4 checkerboard_texture::colorAt(world_distance u, world_distance v) const
{
	int iu = static_cast<int>(floorf(u)), iv = static_cast<int>(floorf(v));

	return ((iu + iv) & 1) ? odd : even;
}

color4 image_texture::pixelAt(ssize_t x, ssize_t y) const
{
	return *pixelAddressAt(image.get(), x, y, w);
}

static real gaussian(world_distance x, world_distance s)
{
	return expf(s ? -(x*x) / (2*s*s) : 0); // scale factor isn't correct but it has to be rescaled anyway
}

static void make_filter(real *filter, int support, world_distance pos)
{
	world_distance p = ((1.f - pos) - .5f) - support;
	real sum = 0, rescale;

	if (verbose_log) printf("filter: p %f\n", p);

	for (int i = -support; i <= support; i++) {
		real v = dmax(gaussian(p, 1.f / sqrtf(2.f)), 0.f);
		sum += v;
		filter[i + support] = v;
		p += 1.f;
	}

	rescale = sum ? (1.f / sum) : 1.f;

	for (int i = -support; i <= support; i++)
	{filter[i + support] *= rescale; if (verbose_log) printf("filter: %d %f -> %f\n", i, p, filter[i+support]);}
}

static inline color4 apply_filter_refine(const image_texture *tex, ssize_t x, ssize_t y, real *filter_x, real *filter_y, int support, bool first = true, color4 previous = color4())
{
	const color Yf = color(.2126f,.7152f,.0722f);
	color4 acc;
	world_distance luma = 0;

	if (!first) luma = dot(Yf, from_premultiplied(previous));

	for (int i = -support; i <= support; i++) {
		for (int j = -support; j <= support; j++) {
			real factor = filter_x[j + support] * filter_y[i + support];
			color4 p = tex->pixelAt(x+j, y+i);

			if (first) {
				acc += p * factor;
			} else {
				world_distance cY = dot(Yf, from_premultiplied(p));
				acc += ((fabsf(luma - cY) < .4f) ? p : previous) * factor;
			}
		}
	}

	return acc;
}

static color4 apply_filter(const image_texture *tex, ssize_t x, ssize_t y, real *filter_x, real *filter_y, int support, unsigned refinements = 1)
{
	color4 c = apply_filter_refine(tex, x, y, filter_x, filter_y, support);
	for (unsigned i = 0; i < refinements; i++)
		c = apply_filter_refine(tex, x, y, filter_x, filter_y, support, false, c);
	return c;
}

color4 image_texture::colorAt(world_distance u, world_distance v) const
{
	if (verbose_log) printf("img: u %f v %f\n", u, v);

	if (v < 0 || v > fh || u < 0 || u > fw) {
		if (repeat) {
			bool rev_u = u < 0, rev_v = v < 0;
			u = fmodf(fabsf(u), fw+1.f);
			v = fmodf(fabsf(v), fh+1.f);
			if (rev_u) u = fw-u;
			if (rev_v) v = fh-v;
		} else {
			if (verbose_log) printf("img: ...is outside w %f h %f\n",fw,fh);
			return color4();
		}
	}

	u = fw-u;

	world_distance uf = floorf(u), vf = floorf(v);
	world_distance ud = u - uf, vd = v - vf;

	const int support = IMG_SUPPORT;
	if (verbose_log) printf("img: post-adj u %f v %f\n", u, v);

	real filter_x[support*2 + 1] = {0}, filter_y[support*2 + 1] = {0};

	make_filter(filter_x, support, ud);
	make_filter(filter_y, support, vd);
	color4 c = apply_filter(this, static_cast<ssize_t>(uf), static_cast<ssize_t>(vf), filter_x, filter_y, support);

	if (verbose_log) {printf("img: color res "); c.print();}

	return c;
}

// --- Texture compositing ---

static color4 color_of_placement(const texture_placement &p, world_distance u, world_distance v)
{
	return p.tex->colorAt(u * p.uScale + p.uShift, v * p.vScale + p.vShift);
}

static color4 rec_colorOfTextureStackAt(const texture_placement *textures, world_distance u, world_distance v, size_t i, color4 above)
{
	color4 thisC = color_of_placement(textures[i], u, v);

	if (verbose_log) {printf("tex recurse: i %lu, this color ", i); thisC.print();}
	color4 resC = over(above, thisC);
	if (i == 0 || close(thisC.a, 1)) return resC;
	return rec_colorOfTextureStackAt(textures, u, v, i-1, resC);
}

static color4 colorOfTextureStackAt(const texture_placement *textures, world_distance u, world_distance v, int texcount)
{
	color4 thisC = color_of_placement(textures[texcount-1], u, v);

	if (verbose_log) {printf("tex stack top: tcount %d, color ", texcount); thisC.print();}
	if (texcount == 1 || close(thisC.a, 1)) return thisC;
	return rec_colorOfTextureStackAt(textures, u, v, static_cast<size_t>(texcount-2), thisC);
}

color4 surface::colorAt(world_distance u, world_distance v) const
{
	if (texcount == 0) return color4(.5f, .5f, .5f, 1.f);
	return colorOfTextureStackAt(textures, u, v, texcount);
}
