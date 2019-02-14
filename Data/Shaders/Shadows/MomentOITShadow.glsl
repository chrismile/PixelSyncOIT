/**
 * This file is part of an OpenGL GLSL port of the HLSL code accompanying the paper "Moment-Based Order-Independent
 * Transparency" by Münstermann, Krumpen, Klein, and Peters (http://momentsingraphics.de/?page_id=210).
 * The original code was released in accordance to CC0 (https://creativecommons.org/publicdomain/zero/1.0/).
 *
 * This port is released under the terms of GNU General Public License v3. For more details please see the LICENSE
 * file in the root directory of this project.
 *
 * Changes for the OpenGL port: Copyright 2018 Christoph Neuhauser
 */

/*! \file
	This header provides the functionality to create the vectors of moments and
	to blend surfaces together with an appropriately reconstructed
	transmittance. It is needed for both additive passes of moment-based OIT.
*/
#ifndef MOMENT_OIT_SHADOW_GLSL
#define MOMENT_OIT_SHADOW_GLSL

// https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Block_buffer_binding
layout(std140, binding = 2) uniform MomentOITUniformDataShadow
{
	vec4 wrapping_zone_parameters;
	float overestimation;
	float moment_bias;
} MomentOITShadow;

#include "MomentMath.glsl"


uniform sampler2DArray zeroth_moment_shadow;
uniform sampler2DArray moments_shadow;
#if USE_R_RG_RGBA_FOR_MBOIT6_SHADOW
uniform sampler2DArray extra_moments_shadow;
#endif


/*! This function is to be called from the shader that composites the
	transparent fragments. It reads the moments and calls the appropriate
	function to reconstruct the transmittance at the specified depth.*/
void resolveMomentsShadow(out float transmittance_at_depth, out float total_transmittance, float depth, vec2 sv_pos)
{
	vec3 idx0 = vec3(sv_pos, 0);
	vec3 idx1 = vec3(idx0.xy, 1);

	transmittance_at_depth = 1;
	total_transmittance = 1;

	float b_0 = texture(zeroth_moment_shadow, idx0).x;
	if (b_0 < 0.00100050033f) {
        return;
    }
	total_transmittance = exp(-b_0);

#if NUM_MOMENTS_SHADOW == 4
#if TRIGONOMETRIC_SHADOW
	vec4 b_tmp = texture(moments_shadow, idx0);
	vec2 trig_b[2];
	trig_b[0] = b_tmp.xy;
	trig_b[1] = b_tmp.zw;
#if SINGLE_PRECISION_SHADOW
	trig_b[0] /= b_0;
	trig_b[1] /= b_0;
#else
	trig_b[0] = fma(trig_b[0], vec2(2.0), vec2(-1.0));
	trig_b[1] = fma(trig_b[1], vec2(2.0), vec2(-1.0));
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom2TrigonometricMoments(b_0, trig_b, depth,
	        MomentOITShadow.moment_bias, MomentOITShadow.overestimation, MomentOITShadow.wrapping_zone_parameters);
#else
	vec4 b_1234 = texture(moments_shadow, idx0).xyzw;
#if SINGLE_PRECISION_SHADOW
	vec2 b_even = b_1234.yw;
	vec2 b_odd = b_1234.xz;

	b_even /= b_0;
	b_odd /= b_0;

	const vec4 bias_vector = vec4(0, 0.375, 0, 0.375);
#else
	vec2 b_even_q = b_1234.yw;
	vec2 b_odd_q = b_1234.xz;

	// Dequantize the moments
	vec2 b_even;
	vec2 b_odd;
	offsetAndDequantizeMoments(b_even, b_odd, b_even_q, b_odd_q);
	const vec4 bias_vector = vec4(0, 0.628, 0, 0.628);
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom4PowerMoments(b_0, b_even, b_odd, depth,
	        MomentOITShadow.moment_bias, MomentOITShadow.overestimation, bias_vector);
#endif
#elif NUM_MOMENTS_SHADOW == 6
	ivec3 idx2 = ivec3(idx0.xy, 2);
#if TRIGONOMETRIC_SHADOW
	vec2 trig_b[3];
	trig_b[0] = texture(moments_shadow, idx0).xy;
#if USE_R_RG_RGBA_FOR_MBOIT6_SHADOW
	vec4 tmp = texture(extra_moments_shadow, idx0);
	trig_b[1] = tmp.xy;
	trig_b[2] = tmp.zw;
#else
	trig_b[1] = texture(moments_shadow, idx1).xy;
	trig_b[2] = texture(moments_shadow, idx2).xy;
#endif
#if SINGLE_PRECISION_SHADOW
	trig_b[0] /= b_0;
	trig_b[1] /= b_0;
	trig_b[2] /= b_0;
#else
	trig_b[0] = fma(trig_b[0], vec2(2.0), vec2(-1.0));
	trig_b[1] = fma(trig_b[1], vec2(2.0), vec2(-1.0));
	trig_b[2] = fma(trig_b[2], vec2(2.0), vec2(-1.0));
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom3TrigonometricMoments(b_0, trig_b, depth,
	        MomentOITShadow.moment_bias, MomentOITShadow.overestimation, MomentOITShadow.wrapping_zone_parameters);
#else
	vec2 b_12 = texture(moments_shadow, idx0).xy;
#if USE_R_RG_RGBA_FOR_MBOIT6_SHADOW
	vec4 tmp = texture(extra_moments_shadow, idx0);
	vec2 b_34 = tmp.xy;
	vec2 b_56 = tmp.zw;
#else
	vec2 b_34 = texture(moments_shadow, idx1).xy;
	vec2 b_56 = texture(moments_shadow, idx2).xy;
#endif
#if SINGLE_PRECISION_SHADOW
	vec3 b_even = vec3(b_12.y, b_34.y, b_56.y);
	vec3 b_odd = vec3(b_12.x, b_34.x, b_56.x);

	b_even /= b_0;
	b_odd /= b_0;

	const float bias_vector[6] = { 0, 0.48, 0, 0.451, 0, 0.45 };
#else
	vec3 b_even_q = vec3(b_12.y, b_34.y, b_56.y);
	vec3 b_odd_q = vec3(b_12.x, b_34.x, b_56.x);
	// Dequantize b_0 and the other moments
	vec3 b_even;
	vec3 b_odd;
	offsetAndDequantizeMoments(b_even, b_odd, b_even_q, b_odd_q);

	const float bias_vector[6] = { 0, 0.5566, 0, 0.489, 0, 0.47869382 };
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom6PowerMoments(b_0, b_even, b_odd, depth,
	        MomentOITShadow.moment_bias, MomentOITShadow.overestimation, bias_vector);
#endif
#elif NUM_MOMENTS_SHADOW == 8
#if TRIGONOMETRIC
	vec4 b_tmp = texture(moments_shadow, idx0);
	vec4 b_tmp2 = texture(moments_shadow, idx1);
#if SINGLE_PRECISION_SHADOW
	vec2 trig_b[4] = {
		b_tmp2.xy / b_0,
		b_tmp.xy / b_0,
		b_tmp2.zw / b_0,
		b_tmp.zw / b_0
	};
#else
	vec2 trig_b[4] = {
		fma(b_tmp2.xy, vec2(2.0), vec2(-1.0)),
		fma(b_tmp.xy, vec2(2.0), vec2(-1.0)),
		fma(b_tmp2.zw, vec2(2.0), vec2(-1.0)),
		fma(b_tmp.zw, vec2(2.0), vec2(-1.0))
	};
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom4TrigonometricMoments(b_0, trig_b, depth,
	        MomentOITShadow.moment_bias, MomentOITShadow.overestimation, MomentOITShadow.wrapping_zone_parameters);
#else
#if SINGLE_PRECISION_SHADOW
	vec4 b_even = texture(moments_shadow, idx0);
	vec4 b_odd = texture(moments_shadow, idx1);

	b_even /= b_0;
	b_odd /= b_0;
	const float bias_vector[8] = { 0, 0.75, 0, 0.67666666666666664, 0, 0.63, 0, 0.60030303030303034 };
#else
	vec4 b_even_q = texture(moments_shadow, idx0);
	vec4 b_odd_q = texture(moments_shadow, idx1);

	// Dequantize the moments
	vec4 b_even;
	vec4 b_odd;
	offsetAndDequantizeMoments(b_even, b_odd, b_even_q, b_odd_q);
	const float bias_vector[8] = { 0, 0.42474916387959866, 0, 0.22407802675585284, 0, 0.15369230769230768, 0, 0.12900440529089119 };
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom8PowerMoments(b_0, b_even, b_odd, depth,
	        MomentOITShadow.moment_bias, MomentOITShadow.overestimation, bias_vector);
#endif
#endif

}
#endif
