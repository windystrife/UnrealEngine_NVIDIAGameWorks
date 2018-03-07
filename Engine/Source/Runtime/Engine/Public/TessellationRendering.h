// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TessellationRendering.h: Tessellation rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"

class FVertexFactoryType;
class UMaterialInterface;

/** Returns true if the Material and Vertex Factory combination require adjacency information.
  * Game thread version that looks at the material settings. Will not change answer during a shader compile */
ENGINE_API bool MaterialSettingsRequireAdjacencyInformation_GameThread(UMaterialInterface* Material, const FVertexFactoryType* VertexFactoryType, ERHIFeatureLevel::Type InFeatureLevel);

/** Returns true if the Material and Vertex Factory combination require adjacency information.
  * Rendering thread version that looks at the current shader that will be used. **Will change answer during a shader compile** */
// NVCHANGE_BEGIN: Add VXGI
ENGINE_API bool MaterialRenderingRequiresAdjacencyInformation_RenderingThread(UMaterialInterface* Material, const FVertexFactoryType* VertexFactoryType, ERHIFeatureLevel::Type InFeatureLevel, bool bIsVxgiVoxelization = false);
// NVCHANGE_END: Add VXGI

/** Returns true if the Material and Vertex Factory combination require adjacency information.
  * Returns different information depending on whether it is called on the rendering thread or game thread -
  * On the game thread, it looks at the material settings. Will not change answer during a shader compile
  * On the rendering thread, it looks at the current shader that will be used. **Will change answer during a shader compile**
  *
  * WARNING: In single-threaded mode as the game thread will return the rendering thread information
  * Please use the explicit game/render thread functions above instead */
ENGINE_API bool RequiresAdjacencyInformation(UMaterialInterface* Material, const FVertexFactoryType* VertexFactoryType, ERHIFeatureLevel::Type InFeatureLevel);
