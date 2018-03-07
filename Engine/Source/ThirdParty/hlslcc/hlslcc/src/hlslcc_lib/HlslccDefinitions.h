// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Enums used in both hlslcc and the common shader compiler modules in the Engine.

#pragma once

enum EHlslShaderFrequency
{
	HSF_VertexShader,
	HSF_PixelShader,
	HSF_GeometryShader,
	HSF_HullShader,
	HSF_DomainShader,
	HSF_ComputeShader,
	HSF_FrequencyCount,
	HSF_InvalidFrequency = -1
};

/**
 * Compilation flags. See PackUniformBuffers.h for details on Grouping/Packing uniforms.
 */
enum EHlslCompileFlag
{
	/** Disables validation of the IR. */
	HLSLCC_NoValidation = 0x1,
	/** Disabled preprocessing. */
	HLSLCC_NoPreprocess = 0x2,
	/** Pack uniforms into typed arrays. */
	HLSLCC_PackUniforms = 0x4,
	/** Assume that input shaders output into DX11 clip space,
	 * and adjust them for OpenGL clip space. */
	HLSLCC_DX11ClipSpace = 0x8,
	/** Print AST for debug purposes. */
	HLSLCC_PrintAST = 0x10,
	// Removed any structures embedded on uniform buffers flattens them into elements of the uniform buffer (Mostly for ES 2: this implies PackUniforms).
	HLSLCC_FlattenUniformBufferStructures = 0x20 | HLSLCC_PackUniforms,
	// Removes uniform buffers and flattens them into globals (Mostly for ES 2: this implies PackUniforms & Flatten Structures).
	HLSLCC_FlattenUniformBuffers = 0x40 | HLSLCC_PackUniforms | HLSLCC_FlattenUniformBufferStructures,
	// Groups flattened uniform buffers per uniform buffer source/precision (Implies Flatten UBs)
	HLSLCC_GroupFlattenedUniformBuffers = 0x80 | HLSLCC_FlattenUniformBuffers,
	// Remove redundant subexpressions [including texture fetches] (to workaround certain drivers who can't optimize redundant texture fetches)
	HLSLCC_ApplyCommonSubexpressionElimination = 0x100,
	// Expand subexpressions/obfuscate (to workaround certain drivers who can't deal with long nested expressions)
	HLSLCC_ExpandSubexpressions = 0x200,
	// Generate shaders compatible with the separate_shader_objects extension
	HLSLCC_SeparateShaderObjects = 0x400,
	// Finds variables being used as atomics and changes all references to use atomic reads/writes
	HLSLCC_FixAtomicReferences = 0x800,
	// Packs global uniforms & flattens structures, and makes each packed array its own uniform buffer
	HLSLCC_PackUniformsIntoUniformBuffers = 0x1000 | HLSLCC_PackUniforms,
	// Set default precision to highp in a pixel shader (default is mediump on ES2 platforms)
	HLSLCC_UseFullPrecisionInPS = 0x2000,
	// Tried to keep global variables' original order from source (default is to sort them in reverse declaration order) 
	HLSCC_KeepGlobalVariableOrder = 0x4000,
	// Do not replace names for samplers and textures (like ps0, ci2)
	HLSLCC_KeepSamplerAndImageNames = 0x8000,
};

/**
 * Cross compilation target.
 */
enum EHlslCompileTarget
{
	HCT_FeatureLevelSM4,	// Equivalent to GLSL 1.50
	HCT_FeatureLevelES3_1Ext,// Equivalent to GLSL ES 310 + extensions
	HCT_FeatureLevelSM5,	// Equivalent to GLSL 4.3
	HCT_FeatureLevelES2,	// Equivalent to GLSL ES2 1.00
	HCT_FeatureLevelES3_1,	// Equivalent to GLSL ES 3.1

	/** Invalid target sentinel. */
	HCT_InvalidTarget,
};
