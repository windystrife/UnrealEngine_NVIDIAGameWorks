// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Definition and helpers for debug view modes
 */

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "SceneTypes.h"

class UMaterialInterface;

/** 
 * Enumeration for different Quad Overdraw visualization mode.
 */
enum EDebugViewShaderMode
{
	DVSM_None,						// No debug view.
	DVSM_ShaderComplexity,			// Default shader complexity viewmode
	DVSM_ShaderComplexityContainedQuadOverhead,	// Show shader complexity with quad overdraw scaling the PS instruction count.
	DVSM_ShaderComplexityBleedingQuadOverhead,	// Show shader complexity with quad overdraw bleeding the PS instruction count over the quad.
	DVSM_QuadComplexity,			// Show quad overdraw only.
	DVSM_PrimitiveDistanceAccuracy,	// Visualize the accuracy of the primitive distance computed for texture streaming.
	DVSM_MeshUVDensityAccuracy,		// Visualize the accuracy of the mesh UV densities computed for texture streaming.
	DVSM_MaterialTextureScaleAccuracy, // Visualize the accuracy of the material texture scales used for texture streaming.
	DVSM_OutputMaterialTextureScales, // Outputs the material texture scales.
	DVSM_RequiredTextureResolution, // Visualize the accuracy of the material texture scales used for texture streaming.
	DVSM_MAX
};

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/** Returns true if the specified shadermode is available for the given shader platform. Called for shader compilation shader compilation. */
ENGINE_API bool AllowDebugViewPS(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform);
/** Returns true if the vertex shader (and potential hull and domain) should be compiled on the given platform. */
ENGINE_API bool AllowDebugViewVSDSHS(EShaderPlatform Platform);
/** Returns true if the shader mode can be enabled. This is only for UI elements as no shader platform is actually passed. */
ENGINE_API bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode);
#else
FORCEINLINE bool AllowDebugViewPS(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform) { return false; }
FORCEINLINE bool AllowDebugViewVSDSHS(EShaderPlatform Platform)  { return false; }
FORCEINLINE bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode) { return false; }
#endif

ENGINE_API int32 GetNumActorsInWorld(UWorld* InWorld);
ENGINE_API bool GetUsedMaterialsInWorld(UWorld* InWorld, OUT TSet<UMaterialInterface*>& OutMaterials, struct FSlowTask& Task);
ENGINE_API bool CompileDebugViewModeShaders(EDebugViewShaderMode Mode, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, bool bFullRebuild, bool bWaitForPreviousShaders, TSet<UMaterialInterface*>& Materials, FSlowTask& ProgressTask);



