// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureFeedback.h"

#include "ClearQuad.h"

TGlobalResource< FVirtualTextureFeedback > GVirtualTextureFeedback;

FVirtualTextureFeedback::FVirtualTextureFeedback()
	: Size( 0, 0 )
{}

void FVirtualTextureFeedback::InitDynamicRHI()
{}

void FVirtualTextureFeedback::ReleaseDynamicRHI()
{
	GRenderTargetPool.FreeUnusedResource( FeedbackTextureGPU );
	GRenderTargetPool.FreeUnusedResource( FeedbackTextureCPU );
}

void FVirtualTextureFeedback::CreateResourceGPU( FRHICommandListImmediate& RHICmdList, int32 SizeX, int32 SizeY )
{
	Size = FIntPoint( SizeX, SizeY );

	FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( Size, PF_R32_UINT, FClearValueBinding::None, TexCreate_None, TexCreate_UAV, false ) );
	GRenderTargetPool.FindFreeElement( RHICmdList, Desc, FeedbackTextureGPU, TEXT("VTFeedbackGPU") );

	// Clear to default value
	const uint32 ClearValue[4] = { ~0u, ~0u, ~0u, ~0u };
	ClearUAV( RHICmdList, FeedbackTextureGPU->GetRenderTargetItem(), ClearValue );
	RHICmdList.TransitionResource( EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToGfx, FeedbackTextureGPU->GetRenderTargetItem().UAV );
}

void FVirtualTextureFeedback::TransferGPUToCPU( FRHICommandListImmediate& RHICmdList )
{
	RHICmdList.TransitionResource( EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, FeedbackTextureGPU->GetRenderTargetItem().UAV );

	GRenderTargetPool.VisualizeTexture.SetCheckPoint( RHICmdList, FeedbackTextureGPU );
	
	FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( Size, PF_R32_UINT, FClearValueBinding::None, TexCreate_CPUReadback | TexCreate_HideInVisualizeTexture, TexCreate_None, false ) );
	GRenderTargetPool.FindFreeElement( RHICmdList, Desc, FeedbackTextureCPU, TEXT("VTFeedbackCPU") );

	// Transfer memory GPU -> CPU
	RHICmdList.CopyToResolveTarget( FeedbackTextureGPU->GetRenderTargetItem().TargetableTexture, FeedbackTextureCPU->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams() );

	GRenderTargetPool.FreeUnusedResource( FeedbackTextureGPU );
}

uint32* FVirtualTextureFeedback::Map( FRHICommandListImmediate& RHICmdList, int32& Pitch )
{
	uint32* FeedbackBuffer = nullptr;

	if( Size.X > 0 && Size.Y > 0 )
	{
		uint32 IdleStart = FPlatformTime::Cycles();

		int32 Temp;
		RHICmdList.MapStagingSurface( FeedbackTextureCPU->GetRenderTargetItem().ShaderResourceTexture, *(void**)&FeedbackBuffer, Pitch, Temp );

		// RHIMapStagingSurface will block until the results are ready (from the previous frame) so we need to consider this RT idle time
		GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
		GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;
	}

	return FeedbackBuffer;
}

void FVirtualTextureFeedback::Unmap( FRHICommandListImmediate& RHICmdList )
{
	check( Size.X > 0 && Size.Y > 0 );
	RHICmdList.UnmapStagingSurface( FeedbackTextureCPU->GetRenderTargetItem().ShaderResourceTexture );

	GRenderTargetPool.FreeUnusedResource( FeedbackTextureCPU );
	Size = FIntPoint::ZeroValue;
}