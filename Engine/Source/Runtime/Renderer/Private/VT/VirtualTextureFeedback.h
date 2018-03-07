// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderTargetPool.h"

/*
====================================
	Manages GPU and CPU buffers for VT feedback.
	Shared for all views, not per view.

	Should use append buffer but that requires RHI ability to copy
	a GPU structured buffer to a CPU read only version.
====================================
*/
class FVirtualTextureFeedback : public FRenderResource
{
public:
					FVirtualTextureFeedback();
					~FVirtualTextureFeedback() {}

	// FRenderResource interface
	virtual void	InitDynamicRHI() override;
	virtual void	ReleaseDynamicRHI() override;

	void			CreateResourceGPU( FRHICommandListImmediate& RHICmdList, int32 SizeX, int32 SizeY );
	void			TransferGPUToCPU( FRHICommandListImmediate& RHICmdList );

	uint32*			Map( FRHICommandListImmediate& RHICmdList, int32& Pitch );
	void			Unmap( FRHICommandListImmediate& RHICmdList );

	FIntPoint		Size;

	TRefCountPtr< IPooledRenderTarget >	FeedbackTextureGPU;
	TRefCountPtr< IPooledRenderTarget >	FeedbackTextureCPU;
};

extern TGlobalResource< FVirtualTextureFeedback > GVirtualTextureFeedback;