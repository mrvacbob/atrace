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

#include "scene.h"
#include "raytrace.h"
#include <png.h>

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
			if (channels == 4) a = p8[3] / 255.;
			color4 pm = to_premultiplied(color(r,g,b), a);
			*pf = pm;
		}
}
/*
static void dump_px(FILE *ex, real *p)
{
	unsigned char p8;
	for (int i = 0; i < 3; i++) {
		p8 = *p++ * 255.;
		fwrite(&p8, 1, 1, ex);
	}
}*/

static void fill_edges(color4 *image, ssize_t w, ssize_t h, bool repeat)
{
	//FILE *ex = fopen("edges.raw", "wb");
	for (ssize_t y = -IMG_SUPPORT; y < (h + IMG_SUPPORT); y++)
		for (ssize_t x = -IMG_SUPPORT; x < (w + IMG_SUPPORT); x++) {
			color4 *edge = pixelAddressAt(image, x, y, w), *im = pixelAddressAt(image, wrap(x, 0, w, repeat), wrap(y, 0, h, repeat), w);
			if (edge == im) continue;

			*edge = *im;
			///dump_px(ex,(real*)edge);
		}
	
 	//fclose(ex);
}

image_texture::image_texture(const char *name, bool repeat) : repeat(repeat)
{
	FILE *f = fopen(name, "rb");
	uint8_t sig[8];
	
    if (!f) {
        fprintf(stderr, "Couldn't load texture %s.\n", name);
        exit(1);
    }

	fread(sig, 1, 8, f);
	if (!png_check_sig(sig, 8)) {printf("nyoro~n\n"); return;}
	
	png_structp pngs = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop pngi = png_create_info_struct(pngs);
	
	if (setjmp(pngs->jmpbuf)) {
		png_destroy_read_struct(&pngs, &pngi, NULL);
		fclose(f);
		return;
	}
	
	png_init_io(pngs, f);
	png_set_sig_bytes(pngs, 8);
	png_set_keep_unknown_chunks(pngs, 0, NULL, 0);
	png_read_info(pngs, pngi);
	
	int bit_depth, color_type;
	png_uint_32 pw, ph;
	
	png_get_IHDR(pngs, pngi, &pw, &ph, &bit_depth, &color_type, NULL, NULL, NULL);
	
	w = pw;
	h = ph;
	
	uint8_t channels = png_get_channels(pngs, pngi);
	
	size_t rb8 = w*channels, rb = (w + IMG_SUPPORT*2);
	uint8_t *image8 = new uint8_t[rb8 * h];
	image = new color4[rb * (h + IMG_SUPPORT*2)];
	
	{
		png_bytep *rows = new png_bytep[h];
		
		for (ssize_t i = 0; i < h; i++) 
			rows[i] = &image8[rb8*i];
		
		png_read_image(pngs, rows);
        delete[] rows;
	}
	
	png_destroy_read_struct(&pngs, &pngi, NULL);
	fclose(f);
	
	fh = h-1;
	fw = w-1;
	
	image_8bit_to_fp(image8, image, channels, w, h);
	fill_edges(image, w, h, repeat);
	
	delete[] image8;
}

struct bmph1 {
	uint8_t hh, hl;
	uint32_t size;
	uint16_t r1, r2;
	uint32_t off;
} __attribute__((packed));

struct bmph2 {
	uint32_t hsize;
	uint32_t width, height;
	uint16_t planes, bpp;
	uint32_t cm, rsize, ppih, ppiv, cc, icc;
} __attribute__((packed));

void image::finish()
{
	f_pixel minp(0.), maxp(1.);
	
	for (size_t y=0; y < h; y++) {
		for (size_t x=0; x < w; x++) {
			minp.set_min(buf[y*w+x]);
			maxp.set_max(buf[y*w+x]);
		}
	}
	
	minv = dmin(dmin(minp.r,minp.g),minp.b);
	maxv = dmax(dmax(maxp.r,maxp.g),maxp.b);
	
	//printf("minv %f maxv %f\n",minv,maxv);
}

void image::write_to_bmp(const char *path)
{
	bmph1 h1;
	bmph2 h2;
	unsigned char *argb = new unsigned char[w * h * 4];
	unsigned char *px = argb;
	f_pixel error(0.);
	
	FILE *img = fopen(path, "wb");
	
	for (size_t y=0; y < h; y++) {
		for (size_t x=0; x < w; x++)
		{
			pixel8 src = autolevel(buf[(h-y-1)*w + x]).dither(error);
			
			*px++ = src.b;
			*px++ = src.g;
			*px++ = src.r;
			*px++ = 255;
		}
	}
	
	h1 = (bmph1){0x42,0x4D,w*h*4+12+40,0,0,12+40+2};
	h2 = (bmph2){40,w,h,1,32,0,w*h*4,72,72,0,0};
	
	fwrite(&h1,sizeof(h1),1,img);
	fwrite(&h2,sizeof(h2),1,img);
	fwrite(argb,4,w*h,img);
	fclose(img);
	delete[] argb;
}