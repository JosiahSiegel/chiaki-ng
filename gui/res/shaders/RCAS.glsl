// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// FidelityFX FSR v1.0.2 RCAS (Robust Contrast Adaptive Sharpening) by AMD.
//
// chiaki-ng runs this after the FSRCNNX spatial upscaler, which upscales luma 2x and is
// then resampled to the window size; that last resample softens the result. Unlike the
// RCAS pass in FSR.glsl (luma only, at the upscaler's resolution) this pass works on RGB
// at the OUTPUT stage: after scaling and color management, at the final resolution and in
// display (gamma or PQ) encoding, which is where AMD intends RCAS to run.
// Strength is the `sharpness` parameter, set by the renderer from the user's setting.

//!PARAM sharpness
//!DESC RCAS strength in stops of reduction: 0.0 is the strongest, 2.0 the weakest
//!TYPE float
//!MINIMUM 0.0
//!MAXIMUM 2.0
0.5

//!HOOK OUTPUT
//!BIND HOOKED
//!DESC FidelityFX RCAS (sharpening after spatial upscaling)

// The limit of providing natural-looking results
#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))

float rcas_max3(float x, float y, float z) { return max(x, max(y, z)); }
float rcas_min3(float x, float y, float z) { return min(x, min(y, z)); }
// Luma times 2, as in AMD's FsrRcasF
float rcas_luma2(vec3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }

vec4 hook() {
	// Minimal 3x3 neighborhood (a plus shape):
	//    b
	//  d e f
	//    h
	vec4 center = HOOKED_tex(HOOKED_pos);
	vec3 e = center.rgb;
	vec3 b = HOOKED_texOff(vec2( 0.0, -1.0)).rgb;
	vec3 d = HOOKED_texOff(vec2(-1.0,  0.0)).rgb;
	vec3 f = HOOKED_texOff(vec2( 1.0,  0.0)).rgb;
	vec3 h = HOOKED_texOff(vec2( 0.0,  1.0)).rgb;

	// Min and max of the ring
	vec3 mn4 = min(min(b, d), min(f, h));
	vec3 mx4 = max(max(b, d), max(f, h));

	// Limiters: the strongest negative lobe that keeps every channel in range
	const vec2 peakC = vec2(1.0, -4.0);
	vec3 hitMin = min(mn4, e) / max(4.0 * mx4, vec3(1e-5));
	vec3 hitMax = (peakC.x - max(mx4, e)) / min(4.0 * mn4 + peakC.y, vec3(-1e-5));
	vec3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-FSR_RCAS_LIMIT, min(rcas_max3(lobeRGB.r, lobeRGB.g, lobeRGB.b), 0.0))
		* exp2(-clamp(sharpness, 0.0, 2.0));

	// Noise detection: sharpen less where the neighborhood looks like noise
	float bL = rcas_luma2(b);
	float dL = rcas_luma2(d);
	float eL = rcas_luma2(e);
	float fL = rcas_luma2(f);
	float hL = rcas_luma2(h);
	float nz = 0.25 * bL + 0.25 * dL + 0.25 * fL + 0.25 * hL - eL;
	float range = rcas_max3(rcas_max3(bL, dL, eL), fL, hL) - rcas_min3(rcas_min3(bL, dL, eL), fL, hL);
	nz = clamp(abs(nz) / max(range, 1e-5), 0.0, 1.0);
	lobe *= -0.5 * nz + 1.0;

	// Resolve; 4 * lobe + 1 >= 0.25 because lobe >= -FSR_RCAS_LIMIT
	vec3 c = (lobe * (b + d + f + h) + e) / (4.0 * lobe + 1.0);
	return vec4(c, center.a);
}
