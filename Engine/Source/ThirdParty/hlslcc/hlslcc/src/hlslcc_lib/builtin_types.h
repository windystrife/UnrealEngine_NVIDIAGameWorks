// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

/*
 * Copyright © 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "macros.h"

const glsl_type glsl_type::_error_type =
   glsl_type(GLSL_TYPE_ERROR, 0, (unsigned int)0, "", "");

const glsl_type glsl_type::_void_type =
   glsl_type(GLSL_TYPE_VOID, 0, (unsigned int)0, "void", "void");

const glsl_type *const glsl_type::error_type = & glsl_type::_error_type;
const glsl_type *const glsl_type::void_type = & glsl_type::_void_type;

/** \name Core built-in types
 *
 * These types exist in all versions of GLSL.
 */
/*@{*/

const glsl_type glsl_type::builtin_core_types[] =
{
   glsl_type(GLSL_TYPE_BOOL, 1, 1, "bool", "bool"),
   glsl_type(GLSL_TYPE_BOOL, 2, 1, "bvec2", "bool2"),
   glsl_type(GLSL_TYPE_BOOL, 3, 1, "bvec3", "bool3"),
   glsl_type(GLSL_TYPE_BOOL, 4, 1, "bvec4", "bool4"),
   glsl_type(GLSL_TYPE_INT, 1, 1, "int", "int"),
   glsl_type(GLSL_TYPE_INT, 2, 1, "ivec2", "int2"),
   glsl_type(GLSL_TYPE_INT, 3, 1, "ivec3", "int3"),
   glsl_type(GLSL_TYPE_INT, 4, 1, "ivec4", "int4"),
   glsl_type(GLSL_TYPE_FLOAT, 1, 1, "float", "float"),
   glsl_type(GLSL_TYPE_FLOAT, 2, 1, "vec2", "float2"),
   glsl_type(GLSL_TYPE_FLOAT, 3, 1, "vec3", "float3"),
   glsl_type(GLSL_TYPE_FLOAT, 4, 1, "vec4", "float4"),
   glsl_type(GLSL_TYPE_FLOAT, 2, 2, "mat2", "float2x2"),
   glsl_type(GLSL_TYPE_FLOAT, 3, 3, "mat3", "float3x3"),
   glsl_type(GLSL_TYPE_FLOAT, 4, 4, "mat4", "float4x4"),
   glsl_type(GLSL_TYPE_SAMPLER_STATE, 0, (unsigned int)0, "samplerState", "SamplerState"),
   glsl_type(GLSL_TYPE_SAMPLER_STATE, 0, (unsigned int)0, "samplerComparisonState", "SamplerComparisonState"),
   glsl_type(GLSL_TYPE_HALF, 1, 1, "half", "half"),
   glsl_type(GLSL_TYPE_HALF, 2, 1, "half2", "half2"),
   glsl_type(GLSL_TYPE_HALF, 3, 1, "half3", "half3"),
   glsl_type(GLSL_TYPE_HALF, 4, 1, "half4", "half4"),
   glsl_type(GLSL_TYPE_HALF, 2, 2, "half2x2", "half2x2"),
   glsl_type(GLSL_TYPE_HALF, 3, 3, "half3x3", "half3x3"),
   glsl_type(GLSL_TYPE_HALF, 4, 4, "half4x4", "half4x4"),
};

const glsl_type *const glsl_type::bool_type  = & builtin_core_types[0];
const glsl_type *const glsl_type::int_type   = & builtin_core_types[4];
const glsl_type *const glsl_type::ivec2_type = & builtin_core_types[5];
const glsl_type *const glsl_type::ivec3_type = & builtin_core_types[6];
const glsl_type *const glsl_type::ivec4_type = & builtin_core_types[7];
const glsl_type *const glsl_type::float_type = & builtin_core_types[8];
const glsl_type *const glsl_type::vec2_type = & builtin_core_types[9];
const glsl_type *const glsl_type::vec3_type = & builtin_core_types[10];
const glsl_type *const glsl_type::vec4_type = & builtin_core_types[11];
const glsl_type *const glsl_type::mat2_type = & builtin_core_types[12];
const glsl_type *const glsl_type::mat3_type = & builtin_core_types[13];
const glsl_type *const glsl_type::mat4_type = & builtin_core_types[14];
const glsl_type *const glsl_type::sampler_state_type = & builtin_core_types[15];
const glsl_type *const glsl_type::sampler_cmp_state_type = & builtin_core_types[16];
const glsl_type *const glsl_type::half_type = &builtin_core_types[17];
const glsl_type *const glsl_type::half2_type = &builtin_core_types[18];
const glsl_type *const glsl_type::half3_type = &builtin_core_types[19];
const glsl_type *const glsl_type::half4_type = &builtin_core_types[20];
const glsl_type *const glsl_type::half2x2_type = &builtin_core_types[21];
const glsl_type *const glsl_type::half3x3_type = &builtin_core_types[22];
const glsl_type *const glsl_type::half4x4_type = &builtin_core_types[23];

/*@}*/

/** \name GLSL structures that have not been deprecated.
 */
/*@{*/

static const struct glsl_struct_field gl_DepthRangeParameters_fields[] = {
   glsl_struct_field(glsl_type::float_type, "near"),
   glsl_struct_field(glsl_type::float_type, "far"),
   glsl_struct_field(glsl_type::float_type, "diff"),
};

const glsl_type glsl_type::builtin_structure_types[] = {
   glsl_type(gl_DepthRangeParameters_fields,
             Elements(gl_DepthRangeParameters_fields),
             "gl_DepthRangeParameters"),
};
/*@}*/

/** \name GLSL 1.00 / 1.10 structures that are deprecated in GLSL 1.30
 */
/*@{*/

static const struct glsl_struct_field gl_PointParameters_fields[] = {
   glsl_struct_field(glsl_type::float_type, "size"),
   glsl_struct_field(glsl_type::float_type, "sizeMin"),
   glsl_struct_field(glsl_type::float_type, "sizeMax"),
   glsl_struct_field(glsl_type::float_type, "fadeThresholdSize"),
   glsl_struct_field(glsl_type::float_type, "distanceConstantAttenuation"),
   glsl_struct_field(glsl_type::float_type, "distanceLinearAttenuation"),
   glsl_struct_field(glsl_type::float_type, "distanceQuadraticAttenuation"),
};

static const struct glsl_struct_field gl_MaterialParameters_fields[] = {
   glsl_struct_field(glsl_type::vec4_type, "emission"),
   glsl_struct_field(glsl_type::vec4_type, "ambient"),
   glsl_struct_field(glsl_type::vec4_type, "diffuse"),
   glsl_struct_field(glsl_type::vec4_type, "specular"),
   glsl_struct_field(glsl_type::float_type, "shininess"),
};

static const struct glsl_struct_field gl_LightSourceParameters_fields[] = {
   glsl_struct_field(glsl_type::vec4_type, "ambient"),
   glsl_struct_field(glsl_type::vec4_type, "diffuse"),
   glsl_struct_field(glsl_type::vec4_type, "specular"),
   glsl_struct_field(glsl_type::vec4_type, "position"),
   glsl_struct_field(glsl_type::vec4_type, "halfVector"),
   glsl_struct_field(glsl_type::vec3_type, "spotDirection"),
   glsl_struct_field(glsl_type::float_type, "spotExponent"),
   glsl_struct_field(glsl_type::float_type, "spotCutoff"),
   glsl_struct_field(glsl_type::float_type, "spotCosCutoff"),
   glsl_struct_field(glsl_type::float_type, "constantAttenuation"),
   glsl_struct_field(glsl_type::float_type, "linearAttenuation"),
   glsl_struct_field(glsl_type::float_type, "quadraticAttenuation"),
};

static const struct glsl_struct_field gl_LightModelParameters_fields[] = {
   glsl_struct_field(glsl_type::vec4_type, "ambient"),
};

static const struct glsl_struct_field gl_LightModelProducts_fields[] = {
   glsl_struct_field(glsl_type::vec4_type, "sceneColor"),
};

static const struct glsl_struct_field gl_LightProducts_fields[] = {
   glsl_struct_field(glsl_type::vec4_type, "ambient"),
   glsl_struct_field(glsl_type::vec4_type, "diffuse"),
   glsl_struct_field(glsl_type::vec4_type, "specular"),
};

static const struct glsl_struct_field gl_FogParameters_fields[] = {
   glsl_struct_field(glsl_type::vec4_type, "color"),
   glsl_struct_field(glsl_type::float_type, "density"),
   glsl_struct_field(glsl_type::float_type, "start"),
   glsl_struct_field(glsl_type::float_type, "end"),
   glsl_struct_field(glsl_type::float_type, "scale"),
};

const glsl_type glsl_type::builtin_110_deprecated_structure_types[] = {
   glsl_type(gl_PointParameters_fields,
             Elements(gl_PointParameters_fields),
             "gl_PointParameters"),
   glsl_type(gl_MaterialParameters_fields,
             Elements(gl_MaterialParameters_fields),
             "gl_MaterialParameters"),
   glsl_type(gl_LightSourceParameters_fields,
             Elements(gl_LightSourceParameters_fields),
             "gl_LightSourceParameters"),
   glsl_type(gl_LightModelParameters_fields,
             Elements(gl_LightModelParameters_fields),
             "gl_LightModelParameters"),
   glsl_type(gl_LightModelProducts_fields,
             Elements(gl_LightModelProducts_fields),
             "gl_LightModelProducts"),
   glsl_type(gl_LightProducts_fields,
             Elements(gl_LightProducts_fields),
             "gl_LightProducts"),
   glsl_type(gl_FogParameters_fields,
             Elements(gl_FogParameters_fields),
             "gl_FogParameters"),
};
/*@}*/

/** \name Types added in GLSL 1.20
 */
/*@{*/

const glsl_type glsl_type::builtin_120_types[] =
{
   glsl_type(GLSL_TYPE_FLOAT, 3, 2, "mat2x3", "float2x3"),
   glsl_type(GLSL_TYPE_FLOAT, 4, 2, "mat2x4", "float2x4"),
   glsl_type(GLSL_TYPE_FLOAT, 2, 3, "mat3x2", "float3x2"),
   glsl_type(GLSL_TYPE_FLOAT, 4, 3, "mat3x4", "float3x4"),
   glsl_type(GLSL_TYPE_FLOAT, 2, 4, "mat4x2", "float4x2"),
   glsl_type(GLSL_TYPE_FLOAT, 3, 4, "mat4x3", "float4x3"),
   glsl_type(GLSL_TYPE_HALF, 3, 2, "half2x3", "half2x3"),
   glsl_type(GLSL_TYPE_HALF, 4, 2, "half2x4", "half2x4"),
   glsl_type(GLSL_TYPE_HALF, 2, 3, "half3x2", "half3x2"),
   glsl_type(GLSL_TYPE_HALF, 4, 3, "half3x4", "half3x4"),
   glsl_type(GLSL_TYPE_HALF, 2, 4, "half4x2", "half4x2"),
   glsl_type(GLSL_TYPE_HALF, 3, 4, "half4x3", "half4x3"),
};
const glsl_type *const glsl_type::mat2x3_type = & builtin_120_types[0];
const glsl_type *const glsl_type::mat2x4_type = & builtin_120_types[1];
const glsl_type *const glsl_type::mat3x2_type = & builtin_120_types[2];
const glsl_type *const glsl_type::mat3x4_type = & builtin_120_types[3];
const glsl_type *const glsl_type::mat4x2_type = & builtin_120_types[4];
const glsl_type *const glsl_type::mat4x3_type = & builtin_120_types[5];
const glsl_type *const glsl_type::half2x3_type = &builtin_120_types[6];
const glsl_type *const glsl_type::half2x4_type = &builtin_120_types[7];
const glsl_type *const glsl_type::half3x2_type = &builtin_120_types[8];
const glsl_type *const glsl_type::half3x4_type = &builtin_120_types[9];
const glsl_type *const glsl_type::half4x2_type = &builtin_120_types[10];
const glsl_type *const glsl_type::half4x3_type = &builtin_120_types[11];
/*@}*/

/** \name Types added in GLSL 1.30
 */
/*@{*/

const glsl_type glsl_type::builtin_130_types[] =
{
   glsl_type(GLSL_TYPE_UINT, 1, 1, "uint", "uint"),
   glsl_type(GLSL_TYPE_UINT, 2, 1, "uvec2", "uint2"),
   glsl_type(GLSL_TYPE_UINT, 3, 1, "uvec3", "uint3"),
   glsl_type(GLSL_TYPE_UINT, 4, 1, "uvec4", "uint4"),
};

const glsl_type *const glsl_type::uint_type = & builtin_130_types[0];
const glsl_type *const glsl_type::uvec2_type = & builtin_130_types[1];
const glsl_type *const glsl_type::uvec3_type = & builtin_130_types[2];
const glsl_type *const glsl_type::uvec4_type = & builtin_130_types[3];
/*@}*/
