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
	
	h1 = (bmph1){0x42,0x4D,(uint32_t)(w*h*4+12+40),0,0,12+40+2};
	h2 = (bmph2){40,(uint32_t)w,(uint32_t)h,1,32,0,(uint32_t)(w*h*4),72,72,0,0};
	
	fwrite(&h1,sizeof(h1),1,img);
	fwrite(&h2,sizeof(h2),1,img);
	fwrite(argb,4,w*h,img);
	fclose(img);
	delete[] argb;
}