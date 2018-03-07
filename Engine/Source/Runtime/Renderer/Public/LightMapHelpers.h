// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "DrawingPolicy.h"

class FPrimitiveSceneProxy;

class LightMapHelpers
{
public:
	 static RENDERER_API FUniformBufferRHIRef CreateDummyPrecomputedLightingUniformBuffer(
		EUniformBufferUsage BufferUsage,
		ERHIFeatureLevel::Type FeatureLevel,
		const FLightCacheInterface* LCI = nullptr
	);
};

