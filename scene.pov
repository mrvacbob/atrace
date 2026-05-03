// POV-Ray 3.7 scene matching atrace checkerboard_scene
// Render with: povray +W1280 +H960 +A0.3 +AM2 scene.pov -D
#version 3.7;
global_settings {
    assumed_gamma 1.0
    // Photons: caustics from glass spheres onto floor/wall
    photons { spacing 0.008 gather 30, 80 max_trace_level 8 }
    // Radiosity: diffuse GI — bounce light from bright floor tiles, soften shadows
    radiosity {
        pretrace_start 0.08
        pretrace_end   0.02
        count          80
        nearest_count  8
        error_bound    0.15
        recursion_limit 1
        low_error_factor 0.5
        brightness     0.05  // very subtle GI — keep shadow edges sharp
        adc_bailout    0.005
    }
}

background { color rgb <0.28, 0.30, 0.35> }

// Camera: origin (0,0,-5), looking toward +z through a screen at z=0
// Screen is 8×6 units → vertical FOV = 2·atan(3/5) = 61.9°, horizontal = 77.3°
camera {
    location  <0, 0, -5>
    direction z * 0.75
    right     x * (4/3)
    up        y
    look_at   <0, 0, 0>
}

// Area light sized to match the emitter sphere (radius 0.2 → 0.4-unit square).
// Gives soft shadows; better matches atrace where the sphere acts as a light source.
light_source {
    <0, 10, -1>
    color rgb <1.3, 1.3, 1.3>
    area_light <0.2, 0, 0>, <0, 0.2, 0>, 4, 4
    adaptive 1
    jitter
    circular orient
    photons { reflection on refraction on }
}

// Visible emitter sphere — decorative
sphere { <0, 10, -1>, 0.2
    texture {
        pigment { color rgb <1, 1, 1> }
        finish { ambient 5 diffuse 0 }
    }
    no_shadow
}

// Floor: horizontal plane at y=-2, B&W checkerboard (0.5-unit tiles)
plane { y, -2
    texture {
        pigment {
            checker
            color rgb <0.05, 0.05, 0.05>
            color rgb <0.9,  0.9,  0.9>
            scale 0.5
        }
        finish { diffuse 1 ambient 0 specular 0 }
    }
}

// Back wall: z=15, tiled caro.png overlay on red/blue checker
// caro.png: 533×600 px at 50 px/unit → 10.66×12.0 world-unit tiles
plane { -z, -15
    texture {
        pigment {
            checker
            color rgb <1, 0.1, 0.1>
            color rgb <0.1, 0.15, 1>
            scale 2
        }
        finish { diffuse 0.7 ambient 0 }
    }
    texture {
        pigment {
            image_map { png "caro.png" map_type 0 interpolate 2 }
            scale <10.66, 12.0, 1>
        }
        finish { diffuse 0.85 ambient 0 }
    }
}

// Left sphere: translucent inky blue blob. No specular, no reflection — looks
// like a volume of dark blue ink/gas. Light passes through but is heavily
// absorbed in red/green so only blue survives, with a depth gradient (center
// darker than rim). Surface is matte/non-reflective; the colour comes almost
// entirely from the interior media.
sphere { <-2.4, 1.8, 3>, 2
    texture {
        pigment { color rgbft <0, 0, 0.5, 0.1, 0.8> }  // f+t=0.9 (physical); blue filter for shadow
        finish { diffuse 0 ambient 0 specular 0 }
    }
    interior {
        ior 1.01                         // near-gas: essentially no refraction
        media {
            absorption <1.79, 1.29, 0.38> // matches atrace k=(1.79,1.29,0.38) → exp(-k*d) survival
            emission  <0.0, 0.0, 0.004>  // barely-there self-glow
            intervals 10
            samples 6, 6
        }
    }
    hollow
    photons { target refraction off reflection off collect off }
}

// Right sphere: clear glass dielectric, ior=1.5 (real glass)
// Add Fresnel reflection so the sphere surface reads as glass (bright edges).
sphere { <2.4, 1.8, 3>, 2
    texture {
        pigment { color rgbf <1.0, 1.0, 1.0, 1.0> }
        finish {
            diffuse 0.0
            ambient 0
            specular 0.4
            roughness 0.001
            reflection { 0.05, 1.0 fresnel on }
            conserve_energy
        }
    }
    interior {
        ior 1.5
        fade_distance 4
        fade_power 1001        // realistic exponential falloff (no actual fade for clear glass)
        dispersion 1.03          // slight chromatic aberration at sphere edges
        dispersion_samples 7
    }
    hollow
    photons { target refraction on reflection on }
}

// Rear sphere: gold
sphere { <0, -0.2, 7>, 2
    texture {
        pigment { color rgb <0.85, 0.65, 0.15> }
        finish {
            diffuse 0.05
            ambient 0
            metallic
            specular 0.8
            roughness 0.003
            reflection { 0.9 metallic }
        }
    }
    photons { reflection off refraction off collect off }
}
