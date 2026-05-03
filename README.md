![Reference render](reference.png)

**[Interactive WebGL preview](https://htmlpreview.github.io/?https://github.com/mrvacbob/atrace/blob/master/scene.html)**

A Whitted-style CPU raytracer. Written to learn the math; never got to use
any 3D matrices so I still don't know them.

## Building

Requires libpng and pkg-config (e.g. from MacPorts).

```
make          # optimized build
make debug    # FP exception traps, no -ffast-math
make check    # render and assert exposure is in expected range
```

Does not work with `-ffast-math` (breaks infinity sentinel used for
max-distance tracking).

Outputs `scene.bmp` (tone-mapped 8-bit) and `scene.hdr` (Radiance HDR,
raw linear via RGBE encoding).

## Neat parts

- Proper sRGB ↔ linear pipeline with noise-shaped dithering on output
- ACES filmic tone mapping via max-channel (preserves hue in highlights)
- Blackman-windowed sinc texture reconstruction (sharper than bilinear)
- Fresnel equations (unpolarized Rs/Rp, TIR handled)
- Per-channel Beer-Lambert absorption for coloured glass interiors
- Emission integral for volumetric self-glow (inky depth gradient)
- Backtracking shadow hack: caustics through glass without forward tracing
- Area light sampling (4 extra shadow rays for soft shadows/penumbras)
- SIMD-accelerated vector class (SSE on x86 via `-march=native`)
- POV-Ray parallel reference scene (photons + radiosity)
- Three.js WebGL preview

## Limitations

- No BVH or spatial acceleration — fine for 6 primitives
- Spheres and planes only
- No scene file format
- Texture minification needs mipmaps/ray differentials (current sinc filter
  is a fixed support, looks bad if textures are effectively downsized)
- Whitted-style transport: no path tracing, no proper GI
