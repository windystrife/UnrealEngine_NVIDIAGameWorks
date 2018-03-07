// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PipelineStateCache.h: Pipeline state cache definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "EnumClassFlags.h"

// Utility flags for modifying render target behavior on a PSO
enum class EApplyRendertargetOption : int
{
	DoNothing  = 0,			// Just use the PSO from initializer's values, no checking and no modifying (faster)
	ForceApply = 1 << 0,	// Always apply the Cmd List's Render Target formats into the PSO initializer
	CheckApply = 1 << 1,	// Verify that the PSO's RT formats match the last Render Target formats set into the CmdList
};

ENUM_CLASS_FLAGS(EApplyRendertargetOption);

extern RHI_API void SetComputePipelineState(FRHICommandList& RHICmdList, FRHIComputeShader* ComputeShader);
extern RHI_API void SetGraphicsPipelineState(FRHICommandList& RHICmdList, const FGraphicsPipelineStateInitializer& Initializer, EApplyRendertargetOption ApplyFlags = EApplyRendertargetOption::CheckApply);
extern RHI_API FGraphicsPipelineState* GetAndOrCreateGraphicsPipelineState(FRHICommandList& RHICmdList, const FGraphicsPipelineStateInitializer& OriginalInitializer, EApplyRendertargetOption ApplyFlags);
extern RHI_API void ClearPipelineCache();