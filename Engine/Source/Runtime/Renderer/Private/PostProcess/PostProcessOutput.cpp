// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PostProcess/PostProcessOutput.h"

FRCPassPostProcessOutput::FRCPassPostProcessOutput(TRefCountPtr<IPooledRenderTarget>* InExternalRenderTarget)
	: ExternalRenderTarget(InExternalRenderTarget)
{
}

void FRCPassPostProcessOutput::Process(FRenderingCompositePassContext& Context)
{
	const FRenderingCompositeOutputRef* Input = GetInput(ePId_Input0);

	if(!Input)
	{
		// input is not hooked up correctly
		return;
	}

	// pass through
	PassOutputs[0].PooledRenderTarget = Input->GetOutput()->PooledRenderTarget;

	check(ExternalRenderTarget);
	*ExternalRenderTarget = PassOutputs[0].PooledRenderTarget;
}

FPooledRenderTargetDesc FRCPassPostProcessOutput::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.DebugName = TEXT("PostProcessOutput");

	return Ret;
}
