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
#include "trig.h"

struct texture
{
	virtual ~texture() {}
	virtual color4 colorAt(world_distance u, world_distance v) = 0;
};

struct flat_texture : public texture
{
	color4 c;

	flat_texture(const color4 &c) : c(to_premultiplied(c)) {}
	color4 colorAt(world_distance u, world_distance v) {return c;}
};

struct checkerboard_texture : public texture
{
	color4 even, odd;

	checkerboard_texture(const color4 &even, const color4 &odd) : even(to_premultiplied(even)), odd(to_premultiplied(odd)) {}

	color4 colorAt(world_distance u, world_distance v);
};

#define IMG_SUPPORT 3

static inline color4 *pixelAddressAt(color4 *image, ssize_t x, ssize_t y, ssize_t w)
{
	return &image[(y + IMG_SUPPORT) * (w+IMG_SUPPORT*2) + (x + IMG_SUPPORT)];
}

struct image_texture : public texture
{
	color4 *image;
	ssize_t w, h;
	world_distance fw, fh;
	bool repeat;

	image_texture(const char *png_name, bool repeat);
	virtual ~image_texture() {if (image) delete[] image;}

	color4 pixelAt(ssize_t x, ssize_t y);
	color4 colorAt(world_distance u, world_distance v);
};

struct texture_placement
{
	texture *tex;
	world_distance uScale, vScale;
	world_distance uShift, vShift;

	texture_placement() : tex(NULL), uScale(1), vScale(1), uShift(0), vShift(0) {}
};
