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

// ACES filmic tone mapping approximation (Stephen Hill / Narkowicz fit).
// Maps linear [0,∞) → display linear [0,1] with a pleasing highlight rolloff.
// Output is linear — the sRGB OETF is applied separately in dither().
static real aces_f(real x)
{
	return dmin(dmax((x*(2.51f*x+0.03f)) / (x*(2.43f*x+0.59f)+0.14f), 0.f), 1.f);
}

void image::finish()
{
	// Log-average luminance → auto-exposure targeting 18% grey.
	// Exclude very dark pixels (shadows, black texels) from the average —
	// including them with a small delta drags the geometric mean down and
	// causes over-exposure. Use double accumulation to reduce FP error.
	const real lum_floor = 0.01f;
	double logsum = 0;
	size_t count  = 0;
	for (size_t i = 0; i < w * h; i++) {
		real L = 0.2126f*buf[i].r + 0.7152f*buf[i].g + 0.0722f*buf[i].b;
		if (L > lum_floor) {
			logsum += (double)logf(L);
			count++;
		}
	}
	real log_avg = count ? expf((real)(logsum / (double)count)) : 1.f;
	exposure = 0.18f / log_avg;
	fprintf(stderr, "atrace: log_avg_lum=%.4f exposure_scale=%.4f\n", log_avg, exposure);
}

void image::write_to_bmp(const char *path) const
{
	FILE *img = fopen(path, "wb");
	if (!img) {
		fprintf(stderr, "Couldn't open %s for writing.\n", path);
		return;
	}

	auto argb = std::make_unique<unsigned char[]>(w * h * 4);
	unsigned char *px = argb.get();

	for (size_t y = 0; y < h; y++) {
		f_pixel error(0.f);  // reset dither error each scanline to avoid horizontal streaks
		for (size_t x = 0; x < w; x++) {
			f_pixel p = buf[(h-y-1)*w + x] * exposure;
			// Tone-map via max channel, not per-channel: preserves hue by applying
			// the same scale to R, G, B. Per-channel ACES shifts hue for saturated
			// colors (e.g. gold highlights go white). Max channel ensures no channel
			// exceeds aces_f(max) ≤ 1, preventing hard post-tone-map clipping too.
			real m = dmax(p.r, dmax(p.g, p.b));
			f_pixel mapped = p * (m > 1e-6f ? aces_f(m) / m : 0.f);
			pixel8 src = mapped.dither(error);
			*px++ = src.b;
			*px++ = src.g;
			*px++ = src.r;
			*px++ = 255;
		}
	}

	uint32_t npx = static_cast<uint32_t>(w * h * 4);
	uint32_t hdrsz = static_cast<uint32_t>(sizeof(bmph1) + sizeof(bmph2));
	bmph1 h1 = {0x42, 0x4D, npx + hdrsz, 0, 0, hdrsz};
	bmph2 h2 = {40, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1, 32, 0, npx, 72, 72, 0, 0};

	fwrite(&h1, sizeof(h1), 1, img);
	fwrite(&h2, sizeof(h2), 1, img);
	fwrite(argb.get(), 4, w*h, img);
	fclose(img);
}

void image::write_to_hdr(const char *path) const
{
	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "Couldn't open %s for writing.\n", path);
		return;
	}

	fprintf(f, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %zu +X %zu\n", h, w);

	// RGBE scratch buffer for one scanline (interleaved, then per-channel for RLE)
	auto rgbe  = std::make_unique<uint8_t[]>(w * 4);
	auto chan  = std::make_unique<uint8_t[]>(w);

	for (size_t y = 0; y < h; y++) {
		// New-style RLE scanline header
		uint8_t hdr[4] = {2, 2, (uint8_t)(w >> 8), (uint8_t)(w & 0xFF)};
		fwrite(hdr, 4, 1, f);

		// Encode pixels as RGBE (buf is stored top-down, matching HDR convention)
		for (size_t x = 0; x < w; x++) {
			f_pixel p = buf[y*w + x];
			float maxc = dmax(dmax(p.r, p.g), p.b);
			uint8_t *px = &rgbe[x * 4];
			if (maxc < 1e-32f) {
				px[0] = px[1] = px[2] = px[3] = 0;
			} else {
				int e;
				float scale = frexpf(maxc, &e) * 256.f / maxc;
				px[0] = (uint8_t)(p.r * scale);
				px[1] = (uint8_t)(p.g * scale);
				px[2] = (uint8_t)(p.b * scale);
				px[3] = (uint8_t)(e + 128);
			}
		}

		// Write each channel with uncompressed RLE (runs of ≤128 non-run bytes)
		for (int c = 0; c < 4; c++) {
			for (size_t x = 0; x < w; x++) chan[x] = rgbe[x*4 + c];
			size_t i = 0;
			while (i < w) {
				size_t cnt = (w - i > 128) ? 128 : w - i;
				uint8_t n = (uint8_t)cnt;
				fwrite(&n, 1, 1, f);
				fwrite(&chan[i], 1, cnt, f);
				i += cnt;
			}
		}
	}

	fclose(f);
}
