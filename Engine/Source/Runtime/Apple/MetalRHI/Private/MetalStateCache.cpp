// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalStateCache.h"
#include "MetalProfiler.h"
#include "ShaderCache.h"

static MTLTriangleFillMode TranslateFillMode(ERasterizerFillMode FillMode)
{
	switch (FillMode)
	{
		case FM_Wireframe:	return MTLTriangleFillModeLines;
		case FM_Point:		return MTLTriangleFillModeFill;
		default:			return MTLTriangleFillModeFill;
	};
}

static MTLCullMode TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch (CullMode)
	{
		case CM_CCW:	return MTLCullModeFront;
		case CM_CW:		return MTLCullModeBack;
		default:		return MTLCullModeNone;
	}
}

FORCEINLINE MTLStoreAction GetMetalRTStoreAction(ERenderTargetStoreAction StoreAction)
{
	switch(StoreAction)
	{
		case ERenderTargetStoreAction::ENoAction: return MTLStoreActionDontCare;
		case ERenderTargetStoreAction::EStore: return MTLStoreActionStore;
		//default store action in the desktop renderers needs to be MTLStoreActionStoreAndMultisampleResolve.  Trying to express the renderer by the requested maxrhishaderplatform
        //because we may render to the same MSAA target twice in two separate passes.  BasePass, then some stuff, then translucency for example and we need to not lose the prior MSAA contents to do this properly.
		case ERenderTargetStoreAction::EMultisampleResolve: 
			return	FMetalCommandQueue::SupportsFeature(EMetalFeatures::EMetalFeaturesMSAAStoreAndResolve) && (GMaxRHIShaderPlatform == SP_METAL_MRT || GMaxRHIShaderPlatform == SP_METAL_SM5 || GMaxRHIShaderPlatform == SP_METAL_MRT_MAC) ?
					MTLStoreActionStoreAndMultisampleResolve : MTLStoreActionMultisampleResolve;
		default: return MTLStoreActionDontCare;
	}
}

FORCEINLINE MTLStoreAction GetConditionalMetalRTStoreAction(bool bMSAATarget)
{
	if (bMSAATarget)
	{
		//this func should only be getting called when an encoder had to abnormally break.  In this case we 'must' do StoreAndResolve because the encoder will be restarted later
		//with the original MSAA rendertarget and the original data must still be there to continue the render properly.
		check(FMetalCommandQueue::SupportsFeature(EMetalFeatures::EMetalFeaturesMSAAStoreAndResolve));
		return MTLStoreActionStoreAndMultisampleResolve;
	}
	else
	{
		return MTLStoreActionStore;
	}	
}

FMetalStateCache::FMetalStateCache(bool const bInImmediate)
: DepthStore(MTLStoreActionUnknown)
, StencilStore(MTLStoreActionUnknown)
, VisibilityResults(nil)
, VisibilityMode(MTLVisibilityResultModeDisabled)
, VisibilityOffset(0)
, DepthStencilState(nullptr)
, RasterizerState(nullptr)
, StencilRef(0)
, BlendFactor(FLinearColor::Transparent)
, FrameBufferSize(CGSizeMake(0.0, 0.0))
, RenderTargetArraySize(1)
, RenderPassDesc(nil)
, RasterBits(0)
, bIsRenderTargetActive(false)
, bHasValidRenderTarget(false)
, bHasValidColorTarget(false)
, bScissorRectEnabled(false)
, bUsingTessellation(false)
, bCanRestartRenderPass(false)
, bImmediate(bInImmediate)
, bFallbackDepthStencilBound(false)
{
	FMemory::Memzero(Viewport);
	FMemory::Memzero(Scissor);
	
	ActiveViewports = 0;
	ActiveScissors = 0;
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		ColorStore[i] = MTLStoreActionUnknown;
	}
	
	FMemory::Memzero(VertexBuffers, sizeof(VertexBuffers));
	FMemory::Memzero(RenderTargetsInfo);	
	FMemory::Memzero(DirtyUniformBuffers);
	
	FMemory::Memzero(ShaderBuffers, sizeof(ShaderBuffers));
	FMemory::Memzero(ShaderTextures, sizeof(ShaderTextures));
	
	for (uint32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
	{
		ShaderSamplers[Frequency].Bound = 0;
		for (uint32 i = 0; i < ML_MaxSamplers; i++)
		{
			ShaderSamplers[Frequency].Samplers[i].SafeRelease();
		}
	}
}

FMetalStateCache::~FMetalStateCache()
{
	[RenderPassDesc release];
	RenderPassDesc = nil;
}

void FMetalStateCache::Reset(void)
{
	for (uint32 i = 0; i < CrossCompiler::NUM_SHADER_STAGES; i++)
	{
		ShaderParameters[i].MarkAllDirty();
	}
	
	SetStateDirty();
	
	IndexType = EMetalIndexType_None;
	SampleCount = 0;
	
	FMemory::Memzero(Viewport);
	FMemory::Memzero(Scissor);
	
	ActiveViewports = 0;
	ActiveScissors = 0;
	
	FMemory::Memzero(RenderTargetsInfo);
	bIsRenderTargetActive = false;
	bHasValidRenderTarget = false;
	bHasValidColorTarget = false;
	bScissorRectEnabled = false;
	
	FMemory::Memzero(DirtyUniformBuffers);
	
	FMemory::Memzero(VertexBuffers, sizeof(VertexBuffers));
	FMemory::Memzero(ShaderBuffers, sizeof(ShaderBuffers));
	FMemory::Memzero(ShaderTextures, sizeof(ShaderTextures));
	
	for (uint32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
	{
		ShaderSamplers[Frequency].Bound = 0;
		for (uint32 i = 0; i < ML_MaxSamplers; i++)
		{
			ShaderSamplers[Frequency].Samplers[i].SafeRelease();
		}
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			BoundUniformBuffers[Frequency][i].SafeRelease();
		}
	}
	
	VisibilityResults = nil;
	VisibilityMode = MTLVisibilityResultModeDisabled;
	VisibilityOffset = 0;
	
	DepthStencilState.SafeRelease();
	RasterizerState.SafeRelease();
	GraphicsPSO.SafeRelease();
	ComputeShader.SafeRelease();
	DepthStencilSurface.SafeRelease();
	StencilRef = 0;
	
	[RenderPassDesc release];
	RenderPassDesc = nil;
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		ColorStore[i] = MTLStoreActionUnknown;
	}
	DepthStore = MTLStoreActionUnknown;
	StencilStore = MTLStoreActionUnknown;
	
	BlendFactor = FLinearColor::Transparent;
	FrameBufferSize = CGSizeMake(0.0, 0.0);
	RenderTargetArraySize = 0;
    bUsingTessellation = false;
    bCanRestartRenderPass = false;
}

static bool MTLScissorRectEqual(MTLScissorRect const& Left, MTLScissorRect const& Right)
{
	return Left.x == Right.x && Left.y == Right.y && Left.width == Right.width && Left.height == Right.height;
}

void FMetalStateCache::SetScissorRect(bool const bEnable, MTLScissorRect const& Rect)
{
	if (bScissorRectEnabled != bEnable || !MTLScissorRectEqual(Scissor[0], Rect))
	{
		bScissorRectEnabled = bEnable;
		if (bEnable)
		{
			Scissor[0] = Rect;
		}
		else
		{
			Scissor[0].x = Viewport[0].originX;
			Scissor[0].y = Viewport[0].originY;
			Scissor[0].width = Viewport[0].width;
			Scissor[0].height = Viewport[0].height;
		}
		
		// Clamp to framebuffer size - Metal doesn't allow scissor to be larger.
		Scissor[0].x = Scissor[0].x;
		Scissor[0].y = Scissor[0].y;
		Scissor[0].width = FMath::Max((Scissor[0].x + Scissor[0].width <= FMath::RoundToInt(FrameBufferSize.width)) ? Scissor[0].width : FMath::RoundToInt(FrameBufferSize.width) - Scissor[0].x, (NSUInteger)1u);
		Scissor[0].height = FMath::Max((Scissor[0].y + Scissor[0].height <= FMath::RoundToInt(FrameBufferSize.height)) ? Scissor[0].height : FMath::RoundToInt(FrameBufferSize.height) - Scissor[0].y, (NSUInteger)1u);
		
		RasterBits |= EMetalRenderFlagScissorRect;
	}
	
	ActiveScissors = 1;
}

void FMetalStateCache::SetBlendFactor(FLinearColor const& InBlendFactor)
{
	if(BlendFactor != InBlendFactor) // @todo zebra
	{
		BlendFactor = InBlendFactor;
		RasterBits |= EMetalRenderFlagBlendColor;
	}
}

void FMetalStateCache::SetStencilRef(uint32 const InStencilRef)
{
	if(StencilRef != InStencilRef) // @todo zebra
	{
		StencilRef = InStencilRef;
		RasterBits |= EMetalRenderFlagStencilReferenceValue;
	}
}

void FMetalStateCache::SetDepthStencilState(FMetalDepthStencilState* InDepthStencilState)
{
	if(DepthStencilState != InDepthStencilState) // @todo zebra
	{
		DepthStencilState = InDepthStencilState;
		RasterBits |= EMetalRenderFlagDepthStencilState;
	}
}

void FMetalStateCache::SetRasterizerState(FMetalRasterizerState* InRasterizerState)
{
	if(RasterizerState != InRasterizerState) // @todo zebra
	{
		RasterizerState = InRasterizerState;
		RasterBits |= EMetalRenderFlagFrontFacingWinding|EMetalRenderFlagCullMode|EMetalRenderFlagDepthBias|EMetalRenderFlagTriangleFillMode;
	}
}

void FMetalStateCache::SetComputeShader(FMetalComputeShader* InComputeShader)
{
	if(ComputeShader != InComputeShader) // @todo zebra
	{
		ComputeShader = InComputeShader;
		
		bUsingTessellation = false;
		
		DirtyUniformBuffers[SF_Compute] = 0xffffffff;

		for (const auto& PackedGlobalArray : InComputeShader->Bindings.PackedGlobalArrays)
		{
			ShaderParameters[CrossCompiler::SHADER_STAGE_COMPUTE].PrepareGlobalUniforms(PackedGlobalArray.TypeIndex, PackedGlobalArray.Size);
		}
	}
}

bool FMetalStateCache::SetRenderTargetsInfo(FRHISetRenderTargetsInfo const& InRenderTargets, id<MTLBuffer> const QueryBuffer, bool const bRestart)
{
	bool bNeedsSet = false;
	
	// see if our new Info matches our previous Info
	if (NeedsToSetRenderTarget(InRenderTargets) || QueryBuffer != VisibilityResults)
	{
		bool bNeedsClear = false;
		
		// Deferred store actions make life a bit easier...
		static bool bSupportsDeferredStore = GetMetalDeviceContext().GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
		
		//Create local store action states if we support deferred store
		MTLStoreAction NewColorStore[MaxSimultaneousRenderTargets];
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
		{
			NewColorStore[i] = MTLStoreActionUnknown;
		}
		
		MTLStoreAction NewDepthStore = MTLStoreActionUnknown;
		MTLStoreAction NewStencilStore = MTLStoreActionUnknown;
		
		// back this up for next frame
		RenderTargetsInfo = InRenderTargets;
		
		// at this point, we need to fully set up an encoder/command buffer, so make a new one (autoreleased)
		MTLRenderPassDescriptor* RenderPass = [MTLRenderPassDescriptor renderPassDescriptor];
		TRACK_OBJECT(STAT_MetalRenderPassDescriptorCount, RenderPass);
	
		// if we need to do queries, write to the supplied query buffer
		if (IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::SM4))
		{
			VisibilityResults = QueryBuffer;
			RenderPass.visibilityResultBuffer = QueryBuffer;
		}
		else
		{
			VisibilityResults = NULL;
			RenderPass.visibilityResultBuffer = NULL;
		}
	
		// default to non-msaa
	    int32 OldCount = SampleCount;
		SampleCount = 0;
	
		bIsRenderTargetActive = false;
		bHasValidRenderTarget = false;
		bHasValidColorTarget = false;
		
		bFallbackDepthStencilBound = false;
		
		uint8 ArrayTargets = 0;
		uint8 BoundTargets = 0;
		uint32 ArrayRenderLayers = UINT_MAX;
		
		bool bFramebufferSizeSet = false;
		FrameBufferSize = CGSizeMake(0.f, 0.f);
		
		bCanRestartRenderPass = true;
		
		for (uint32 RenderTargetIndex = 0; RenderTargetIndex < MaxSimultaneousRenderTargets; RenderTargetIndex++)
		{
			// default to invalid
			uint8 FormatKey = 0;
			// only try to set it if it was one that was set (ie less than RenderTargetsInfo.NumColorRenderTargets)
			if (RenderTargetIndex < RenderTargetsInfo.NumColorRenderTargets && RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex].Texture != nullptr)
			{
				const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
				ColorTargets[RenderTargetIndex] = RenderTargetView.Texture;
				
				FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);
				FormatKey = Surface.FormatKey;
				
				uint32 Width = FMath::Max((uint32)(Surface.SizeX >> RenderTargetView.MipIndex), (uint32)1);
				uint32 Height = FMath::Max((uint32)(Surface.SizeY >> RenderTargetView.MipIndex), (uint32)1);
				if(!bFramebufferSizeSet)
				{
					bFramebufferSizeSet = true;
					FrameBufferSize.width = Width;
					FrameBufferSize.height = Height;
				}
				else
				{
					FrameBufferSize.width = FMath::Min(FrameBufferSize.width, (CGFloat)Width);
					FrameBufferSize.height = FMath::Min(FrameBufferSize.height, (CGFloat)Height);
				}
	
				// if this is the back buffer, make sure we have a usable drawable
				ConditionalUpdateBackBuffer(Surface);
	
				BoundTargets |= 1 << RenderTargetIndex;
            
#if !PLATFORM_MAC
                if (Surface.Texture == nil)
                {
                    SampleCount = OldCount;
                    bCanRestartRenderPass &= (OldCount <= 1);
                    return true;
                }
#endif
				
				// The surface cannot be nil - we have to have a valid render-target array after this call.
				check (Surface.Texture != nil);
	
				// user code generally passes -1 as a default, but we need 0
				uint32 ArraySliceIndex = RenderTargetView.ArraySliceIndex == 0xFFFFFFFF ? 0 : RenderTargetView.ArraySliceIndex;
				if (Surface.bIsCubemap)
				{
					ArraySliceIndex = GetMetalCubeFace((ECubeFace)ArraySliceIndex);
				}
				
				switch(Surface.Type)
				{
					case RRT_Texture2DArray:
					case RRT_Texture3D:
					case RRT_TextureCube:
						if(RenderTargetView.ArraySliceIndex == 0xFFFFFFFF)
						{
							ArrayTargets |= (1 << RenderTargetIndex);
							ArrayRenderLayers = FMath::Min(ArrayRenderLayers, Surface.GetNumFaces());
						}
						else
						{
							ArrayRenderLayers = 1;
						}
						break;
					default:
						ArrayRenderLayers = 1;
						break;
				}
	
				MTLRenderPassColorAttachmentDescriptor* ColorAttachment = [MTLRenderPassColorAttachmentDescriptor new];
				TRACK_OBJECT(STAT_MetalRenderPassColorAttachmentDescriptorCount, ColorAttachment);
	
				if (Surface.MSAATexture != nil)
				{
					// set up an MSAA attachment
					ColorAttachment.texture = Surface.MSAATexture;
					NewColorStore[RenderTargetIndex] = GetMetalRTStoreAction(ERenderTargetStoreAction::EMultisampleResolve);
					ColorAttachment.storeAction = bSupportsDeferredStore ? MTLStoreActionUnknown : NewColorStore[RenderTargetIndex];
					ColorAttachment.resolveTexture = Surface.MSAAResolveTexture ? Surface.MSAAResolveTexture : Surface.Texture;
					SampleCount = Surface.MSAATexture.sampleCount;
                    
					// only allow one MRT with msaa
					checkf(RenderTargetsInfo.NumColorRenderTargets == 1, TEXT("Only expected one MRT when using MSAA"));
				}
				else
				{
					// set up non-MSAA attachment
					ColorAttachment.texture = Surface.Texture;
					NewColorStore[RenderTargetIndex] = GetMetalRTStoreAction(RenderTargetView.StoreAction);
					ColorAttachment.storeAction = bSupportsDeferredStore ? MTLStoreActionUnknown : NewColorStore[RenderTargetIndex];
                    SampleCount = 1;
				}
				
				ColorAttachment.level = RenderTargetView.MipIndex;
				if(Surface.Type == RRT_Texture3D)
				{
					ColorAttachment.depthPlane = ArraySliceIndex;
				}
				else
				{
					ColorAttachment.slice = ArraySliceIndex;
				}
				
				ColorAttachment.loadAction = (Surface.Written || !bImmediate || bRestart) ? GetMetalRTLoadAction(RenderTargetView.LoadAction) : MTLLoadActionClear;
				FPlatformAtomics::InterlockedExchange(&Surface.Written, 1);
				
				bNeedsClear |= (ColorAttachment.loadAction == MTLLoadActionClear);
				
				const FClearValueBinding& ClearValue = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex].Texture->GetClearBinding();
				if (ClearValue.ColorBinding == EClearBinding::EColorBound)
				{
					const FLinearColor& ClearColor = ClearValue.GetClearColor();
					ColorAttachment.clearColor = MTLClearColorMake(ClearColor.R, ClearColor.G, ClearColor.B, ClearColor.A);
				}

				// assign the attachment to the slot
				[RenderPass.colorAttachments setObject:ColorAttachment atIndexedSubscript:RenderTargetIndex];
				
				bCanRestartRenderPass &= (SampleCount <= 1) && (ColorAttachment.loadAction == MTLLoadActionLoad) && (RenderTargetView.StoreAction == ERenderTargetStoreAction::EStore);
	
				UNTRACK_OBJECT(STAT_MetalRenderPassColorAttachmentDescriptorCount, ColorAttachment);
				[ColorAttachment release];
	
				bHasValidRenderTarget = true;
				bHasValidColorTarget = true;
			}
			else
			{
				ColorTargets[RenderTargetIndex].SafeRelease();
			}
		}
		
		RenderTargetArraySize = 1;
		
		if(ArrayTargets)
		{
			if (!GetMetalDeviceContext().SupportsFeature(EMetalFeaturesLayeredRendering))
			{
				if (ArrayRenderLayers != 1)
				{
					UE_LOG(LogMetal, Fatal, TEXT("Layered rendering is unsupported on this device."));
				}
			}
#if PLATFORM_MAC
			else
			{
				if (ArrayTargets == BoundTargets)
				{
					RenderTargetArraySize = ArrayRenderLayers;
					RenderPass.renderTargetArrayLength = ArrayRenderLayers;
				}
				else
				{
					UE_LOG(LogMetal, Fatal, TEXT("All color render targets must be layered when performing multi-layered rendering under Metal."));
				}
			}
#endif
		}
	
		// default to invalid
		uint8 DepthFormatKey = 0;
		uint8 StencilFormatKey = 0;
		
		// setup depth and/or stencil
		if (RenderTargetsInfo.DepthStencilRenderTarget.Texture != nullptr)
		{
			FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderTargetsInfo.DepthStencilRenderTarget.Texture);
			
			switch(Surface.Type)
			{
				case RRT_Texture2DArray:
				case RRT_Texture3D:
				case RRT_TextureCube:
					ArrayRenderLayers = Surface.GetNumFaces();
					break;
				default:
					ArrayRenderLayers = 1;
					break;
			}
			if(!ArrayTargets && ArrayRenderLayers > 1)
			{
				if (!GetMetalDeviceContext().SupportsFeature(EMetalFeaturesLayeredRendering))
				{
					UE_LOG(LogMetal, Fatal, TEXT("Layered rendering is unsupported on this device."));
				}
#if PLATFORM_MAC
				else
				{
					RenderTargetArraySize = ArrayRenderLayers;
					RenderPass.renderTargetArrayLength = ArrayRenderLayers;
				}
#endif
			}
			
			if(!bFramebufferSizeSet)
			{
				bFramebufferSizeSet = true;
				FrameBufferSize.width = Surface.SizeX;
				FrameBufferSize.height = Surface.SizeY;
			}
			else
			{
				FrameBufferSize.width = FMath::Min(FrameBufferSize.width, (CGFloat)Surface.SizeX);
				FrameBufferSize.height = FMath::Min(FrameBufferSize.height, (CGFloat)Surface.SizeY);
			}
			
			EPixelFormat DepthStencilPixelFormat = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetFormat();
			
			id<MTLTexture> DepthTexture = nil;
			id<MTLTexture> StencilTexture = nil;
			
            const bool bSupportSeparateMSAAResolve = FMetalCommandQueue::SupportsSeparateMSAAAndResolveTarget();
			uint32 DepthSampleCount = (Surface.MSAATexture ? Surface.MSAATexture.sampleCount : Surface.Texture.sampleCount);
            bool bDepthStencilSampleCountMismatchFixup = false;
            DepthTexture = Surface.MSAATexture ? Surface.MSAATexture : Surface.Texture;
			if (SampleCount == 0)
			{
				SampleCount = DepthSampleCount;
			}
			else if (SampleCount != DepthSampleCount)
            {
                //in the case of NOT support separate MSAA resolve the high level may legitimately cause a mismatch which we need to handle by binding the resolved target which we normally wouldn't do.
                checkf(!bSupportSeparateMSAAResolve, TEXT("If we support separate targets the high level should always give us matching counts"));
                DepthTexture = Surface.Texture;
                bDepthStencilSampleCountMismatchFixup = true;
				DepthSampleCount = 1;
            }

			switch (DepthStencilPixelFormat)
			{
				case PF_X24_G8:
				case PF_DepthStencil:
				case PF_D24:
				{
					MTLPixelFormat DepthStencilFormat = Surface.Texture ? Surface.Texture.pixelFormat : MTLPixelFormatInvalid;
					
					switch(DepthStencilFormat)
					{
						case MTLPixelFormatDepth32Float:
#if !PLATFORM_MAC
							StencilTexture = (DepthStencilPixelFormat == PF_DepthStencil) ? Surface.StencilTexture : nil;
#endif
							break;
						case MTLPixelFormatStencil8:
							StencilTexture = DepthTexture;
							break;
						case MTLPixelFormatDepth32Float_Stencil8:
							StencilTexture = DepthTexture;
							break;
#if PLATFORM_MAC
						case MTLPixelFormatDepth24Unorm_Stencil8:
							StencilTexture = DepthTexture;
							break;
#endif
						default:
							break;
					}
					
					break;
				}
				case PF_ShadowDepth:
				{
					break;
				}
				default:
					break;
			}
			
			float DepthClearValue = 0.0f;
			uint32 StencilClearValue = 0;
			const FClearValueBinding& ClearValue = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetClearBinding();
			if (ClearValue.ColorBinding == EClearBinding::EDepthStencilBound)
			{
				ClearValue.GetDepthStencil(DepthClearValue, StencilClearValue);
			}
			else if(!ArrayTargets && ArrayRenderLayers > 1)
			{
				DepthClearValue = 1.0f;
			}

			static bool const bUsingValidation = FMetalCommandQueue::SupportsFeature(EMetalFeaturesValidation) && !FParse::Param(FCommandLine::Get(),TEXT("metalbinddepthstencilseparately"));
			
			bool const bCombinedDepthStencilUsingStencil = (DepthTexture && DepthTexture.pixelFormat != MTLPixelFormatDepth32Float && RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsUsingStencil());
			
			bool const bUsingDepth = (RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsUsingDepth() || (bUsingValidation && bCombinedDepthStencilUsingStencil));
			if (DepthTexture && bUsingDepth)
			{
				MTLRenderPassDepthAttachmentDescriptor* DepthAttachment = [[MTLRenderPassDepthAttachmentDescriptor alloc] init];
				TRACK_OBJECT(STAT_MetalRenderPassDepthAttachmentDescriptorCount, DepthAttachment);
				
				DepthFormatKey = Surface.FormatKey;
	
				// set up the depth attachment
				DepthAttachment.texture = DepthTexture;
				DepthAttachment.loadAction = GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction);
				
				bNeedsClear |= (DepthAttachment.loadAction == MTLLoadActionClear);
				
				ERenderTargetStoreAction HighLevelStoreAction = (Surface.MSAATexture && !bDepthStencilSampleCountMismatchFixup) ? ERenderTargetStoreAction::EMultisampleResolve : RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction;
				if (bUsingDepth && (HighLevelStoreAction == ERenderTargetStoreAction::ENoAction || bDepthStencilSampleCountMismatchFixup))
				{
					if (DepthSampleCount > 1)
					{
						HighLevelStoreAction = ERenderTargetStoreAction::EMultisampleResolve;
					}
					else
					{
						HighLevelStoreAction = ERenderTargetStoreAction::EStore;
					}
				}
				
                //needed to quiet the metal validation that runs when you end renderpass. (it requires some kind of 'resolve' for an msaa target)
				//But with deferredstore we don't set the real one until submit time.
                NewDepthStore = GetMetalRTStoreAction(HighLevelStoreAction);
				DepthAttachment.storeAction = bSupportsDeferredStore ? MTLStoreActionUnknown : NewDepthStore;
				DepthAttachment.clearDepth = DepthClearValue;
				check(SampleCount > 0);

				const bool bSupportsMSAADepthResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAADepthResolve);
				if (Surface.MSAATexture && bSupportsMSAADepthResolve)
				{
                    if (bDepthStencilSampleCountMismatchFixup)
                    {
                        DepthAttachment.resolveTexture = nil;
                    }
                    else
                    {
                        DepthAttachment.resolveTexture = Surface.MSAAResolveTexture ? Surface.MSAAResolveTexture : Surface.Texture;
                    }
#if PLATFORM_MAC
					//would like to assert and do manual custom resolve, but that is causing some kind of weird corruption.
					//checkf(false, TEXT("Depth resolves need to do 'max' for correctness.  MacOS does not expose this yet unless the spec changed."));
#else
					DepthAttachment.depthResolveFilter = MTLMultisampleDepthResolveFilterMax;
#endif
				}
				
				bHasValidRenderTarget = true;
				bFallbackDepthStencilBound = (RenderTargetsInfo.DepthStencilRenderTarget.Texture == FallbackDepthStencilSurface);

				bCanRestartRenderPass &= (SampleCount <= 1) && ((RenderTargetsInfo.DepthStencilRenderTarget.Texture == FallbackDepthStencilSurface) || ((DepthAttachment.loadAction == MTLLoadActionLoad) && (!RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsDepthWrite() || (RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction == ERenderTargetStoreAction::EStore))));
				
				// and assign it
				RenderPass.depthAttachment = DepthAttachment;
				
				UNTRACK_OBJECT(STAT_MetalRenderPassDepthAttachmentDescriptorCount, DepthAttachment);
				[DepthAttachment release];
			}
	
            //if we're dealing with a samplecount mismatch we just bail on stencil entirely as stencil
            //doesn't have an autoresolve target to use.
			
			bool const bCombinedDepthStencilUsingDepth = (StencilTexture && StencilTexture.pixelFormat != MTLPixelFormatStencil8 && RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsUsingDepth());
			bool const bUsingStencil = RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsUsingStencil() || (bUsingValidation && bCombinedDepthStencilUsingDepth);
			if (StencilTexture && bUsingStencil && (FMetalCommandQueue::SupportsFeature(EMetalFeaturesCombinedDepthStencil) || !bDepthStencilSampleCountMismatchFixup))
			{
                if (!FMetalCommandQueue::SupportsFeature(EMetalFeaturesCombinedDepthStencil) && bDepthStencilSampleCountMismatchFixup)
                {
                    checkf(!RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsStencilWrite(), TEXT("Stencil write not allowed as we don't have a proper stencil to use."));
                }
                else
                {
                    MTLRenderPassStencilAttachmentDescriptor* StencilAttachment = [[MTLRenderPassStencilAttachmentDescriptor alloc] init];
                    TRACK_OBJECT(STAT_MetalRenderPassStencilAttachmentDescriptorCount, StencilAttachment);
                    
                    StencilFormatKey = Surface.FormatKey;
        
                    // set up the stencil attachment
                    StencilAttachment.texture = StencilTexture;
                    StencilAttachment.loadAction = GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction);
                    
                    bNeedsClear |= (StencilAttachment.loadAction == MTLLoadActionClear);
					
					ERenderTargetStoreAction HighLevelStoreAction = RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction();
					if (bUsingStencil && (HighLevelStoreAction == ERenderTargetStoreAction::ENoAction || bDepthStencilSampleCountMismatchFixup))
					{
						HighLevelStoreAction = ERenderTargetStoreAction::EStore;
					}
					
					// For the case where Depth+Stencil is MSAA we can't Resolve depth and Store stencil - we can only Resolve + DontCare or StoreResolve + Store (on newer H/W and iOS).
					// We only allow use of StoreResolve in the Desktop renderers as the mobile renderer does not and should not assume hardware support for it.
					NewStencilStore = (StencilTexture.sampleCount == 1  || GetMetalRTStoreAction(ERenderTargetStoreAction::EMultisampleResolve) == MTLStoreActionStoreAndMultisampleResolve) ? GetMetalRTStoreAction(HighLevelStoreAction) : MTLStoreActionDontCare;
                    StencilAttachment.storeAction = bSupportsDeferredStore ? MTLStoreActionUnknown : NewStencilStore;
                    StencilAttachment.clearStencil = StencilClearValue;

                    if (SampleCount == 0)
                    {
                        SampleCount = StencilAttachment.texture.sampleCount;
                    }
                    
                    bHasValidRenderTarget = true;
                    
                    // @todo Stencil writes that need to persist must use ERenderTargetStoreAction::EStore on iOS.
                    // We should probably be using deferred store actions so that we can safely lazily instantiate encoders.
                    bCanRestartRenderPass &= (SampleCount <= 1) && ((RenderTargetsInfo.DepthStencilRenderTarget.Texture == FallbackDepthStencilSurface) || ((StencilAttachment.loadAction == MTLLoadActionLoad) && (1 || !RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsStencilWrite() || (RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction() == ERenderTargetStoreAction::EStore))));
                    
                    // and assign it
                    RenderPass.stencilAttachment = StencilAttachment;
                    
                    UNTRACK_OBJECT(STAT_MetalRenderPassStencilAttachmentDescriptorCount, StencilAttachment);
                    [StencilAttachment release];
                }
			}
		}
		
		//Update deferred store states if required otherwise they're already set directly on the Metal Attachement Descriptors
		if (bSupportsDeferredStore)
		{
			for (uint32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
			{
				ColorStore[i] = NewColorStore[i];
			}
			DepthStore = NewDepthStore;
			StencilStore = NewStencilStore;
		}
		
		bHasValidRenderTarget |= (InRenderTargets.NumUAVs > 0);
		if (SampleCount == 0)
		{
			SampleCount = 1;
		}
		
		bIsRenderTargetActive = bHasValidRenderTarget;
		
		// Only start encoding if the render target state is valid
		if (bHasValidRenderTarget)
		{
			// Retain and/or release the depth-stencil surface in case it is a temporary surface for a draw call that writes to depth without a depth/stencil buffer bound.
			DepthStencilSurface = RenderTargetsInfo.DepthStencilRenderTarget.Texture;
		}
		else
		{
			DepthStencilSurface.SafeRelease();
		}
		
		[RenderPassDesc release];
		RenderPassDesc = nil;
		RenderPassDesc = [RenderPass retain];
		
		bNeedsSet = true;
	}

	return bNeedsSet;
}

void FMetalStateCache::InvalidateRenderTargets(void)
{
	bHasValidRenderTarget = false;
	bHasValidColorTarget = false;
	bIsRenderTargetActive = false;
}

void FMetalStateCache::SetRenderTargetsActive(bool const bActive)
{
	bIsRenderTargetActive = bActive;
}

static bool MTLViewportEqual(MTLViewport const& Left, MTLViewport const& Right)
{
	return FMath::IsNearlyEqual(Left.originX, Right.originX) &&
			FMath::IsNearlyEqual(Left.originY, Right.originY) &&
			FMath::IsNearlyEqual(Left.width, Right.width) &&
			FMath::IsNearlyEqual(Left.height, Right.height) &&
			FMath::IsNearlyEqual(Left.znear, Right.znear) &&
			FMath::IsNearlyEqual(Left.zfar, Right.zfar);
}

void FMetalStateCache::SetViewport(const MTLViewport& InViewport)
{
	if (!MTLViewportEqual(Viewport[0], InViewport))
	{
		Viewport[0] = InViewport;
	
		RasterBits |= EMetalRenderFlagViewport;
	}
	
	ActiveViewports = 1;
	
	if (!bScissorRectEnabled)
	{
		MTLScissorRect Rect;
		Rect.x = InViewport.originX;
		Rect.y = InViewport.originY;
		Rect.width = InViewport.width;
		Rect.height = InViewport.height;
		SetScissorRect(false, Rect);
	}
}

void FMetalStateCache::SetViewport(uint32 Index, const MTLViewport& InViewport)
{
	check(Index < ML_MaxViewports);
	
	if (!MTLViewportEqual(Viewport[Index], InViewport))
	{
		Viewport[Index] = InViewport;
		
		RasterBits |= EMetalRenderFlagViewport;
	}
	
	// There may not be gaps in the viewport array.
	ActiveViewports = Index + 1;
	
	// This always sets the scissor rect because the RHI doesn't bother to expose proper scissor states for multiple viewports.
	// This will have to change if we want to guarantee correctness in the mid to long term.
	{
		MTLScissorRect Rect;
		Rect.x = InViewport.originX;
		Rect.y = InViewport.originY;
		Rect.width = InViewport.width;
		Rect.height = InViewport.height;
		SetScissorRect(Index, false, Rect);
	}
}

void FMetalStateCache::SetScissorRect(uint32 Index, bool const bEnable, MTLScissorRect const& Rect)
{
	check(Index < ML_MaxViewports);
	if (!MTLScissorRectEqual(Scissor[Index], Rect))
	{
		// There's no way we can setup the bounds correctly - that must be done by the caller or incorrect rendering & crashes will ensue.
		Scissor[Index] = Rect;
		RasterBits |= EMetalRenderFlagScissorRect;
	}
	
	ActiveScissors = Index + 1;
}

void FMetalStateCache::SetViewports(const MTLViewport InViewport[], uint32 Count)
{
	check(Count >= 1 && Count < ML_MaxViewports);
	
	// Check if the count has changed first & if so mark for a rebind
	if (ActiveViewports != Count)
	{
		RasterBits |= EMetalRenderFlagViewport;
		RasterBits |= EMetalRenderFlagScissorRect;
	}
	
	for (uint32 i = 0; i < Count; i++)
	{
		SetViewport(i, InViewport[i]);
	}
	
	ActiveViewports = Count;
}

void FMetalStateCache::SetVertexStream(uint32 const Index, id<MTLBuffer> Buffer, FMetalBufferData* Bytes, uint32 const Offset, uint32 const Length)
{
	check(Index < MaxVertexElementCount);
	check(UNREAL_TO_METAL_BUFFER_INDEX(Index) < MaxMetalStreams);

	VertexBuffers[Index].Buffer = Buffer;
	VertexBuffers[Index].Offset = 0;
	VertexBuffers[Index].Bytes = Bytes;
	VertexBuffers[Index].Length = Length;
	
	SetShaderBuffer(SF_Vertex, Buffer, Bytes, Offset, Length, UNREAL_TO_METAL_BUFFER_INDEX(Index));
}

uint32 FMetalStateCache::GetVertexBufferSize(uint32 const Index)
{
	check(Index < MaxVertexElementCount);
	check(UNREAL_TO_METAL_BUFFER_INDEX(Index) < MaxMetalStreams);
	return VertexBuffers[Index].Length;
}

void FMetalStateCache::SetGraphicsPipelineState(FMetalGraphicsPipelineState* State)
{
	if (GraphicsPSO != State)
	{
		GraphicsPSO = State;
		
		bool bNewUsingTessellation = (State && State->GetPipeline(IndexType).TessellationPipelineDesc);
		if (bNewUsingTessellation != bUsingTessellation)
		{
			for (uint32 i = 0; i < SF_NumFrequencies; i++)
			{
				ShaderBuffers[i].Bound = UINT32_MAX;
#if PLATFORM_MAC
#ifndef UINT128_MAX
#define UINT128_MAX (((__uint128_t)1 << 127) - (__uint128_t)1 + ((__uint128_t)1 << 127))
#endif
				ShaderTextures[i].Bound = UINT128_MAX;
#else
				ShaderTextures[i].Bound = UINT32_MAX;
#endif
				ShaderSamplers[i].Bound = UINT16_MAX;
			}
		}
		// Whenever the pipeline changes & a Hull shader is bound clear the Hull shader bindings, otherwise the Hull resources from a
		// previous pipeline with different binding table will overwrite the vertex shader bindings for the current pipeline.
		if (bNewUsingTessellation)
		{
			ShaderBuffers[SF_Hull].Bound = UINT32_MAX;
#if PLATFORM_MAC
			ShaderTextures[SF_Hull].Bound = UINT128_MAX;
#else
			ShaderTextures[SF_Hull].Bound = UINT32_MAX;
#endif
			ShaderSamplers[SF_Hull].Bound = UINT16_MAX;
			FMemory::Memzero(ShaderBuffers[SF_Hull].Buffers, sizeof(ShaderBuffers[SF_Hull].Buffers));
			FMemory::Memzero(ShaderTextures[SF_Hull].Textures, sizeof(ShaderTextures[SF_Hull].Textures));
			for (uint32 i = 0; i < ML_MaxSamplers; i++)
			{
				ShaderSamplers[SF_Hull].Samplers[i].SafeRelease();
			}

			for (const auto& PackedGlobalArray : State->HullShader->Bindings.PackedGlobalArrays)
			{
				ShaderParameters[CrossCompiler::SHADER_STAGE_HULL].PrepareGlobalUniforms(PackedGlobalArray.TypeIndex, PackedGlobalArray.Size);
			}

			for (const auto& PackedGlobalArray : State->DomainShader->Bindings.PackedGlobalArrays)
			{
				ShaderParameters[CrossCompiler::SHADER_STAGE_DOMAIN].PrepareGlobalUniforms(PackedGlobalArray.TypeIndex, PackedGlobalArray.Size);
			}
		}
		bUsingTessellation = bNewUsingTessellation;
		
		DirtyUniformBuffers[SF_Vertex] = 0xffffffff;
		DirtyUniformBuffers[SF_Pixel] = 0xffffffff;
		DirtyUniformBuffers[SF_Hull] = 0xffffffff;
		DirtyUniformBuffers[SF_Domain] = 0xffffffff;
		DirtyUniformBuffers[SF_Geometry] = 0xffffffff;
		
		RasterBits |= EMetalRenderFlagPipelineState;
		
		SetDepthStencilState(State->DepthStencilState);
		SetRasterizerState(State->RasterizerState);

		for (const auto& PackedGlobalArray : State->VertexShader->Bindings.PackedGlobalArrays)
		{
			ShaderParameters[CrossCompiler::SHADER_STAGE_VERTEX].PrepareGlobalUniforms(PackedGlobalArray.TypeIndex, PackedGlobalArray.Size);
		}

		if (State->PixelShader)
		{
			for (const auto& PackedGlobalArray : State->PixelShader->Bindings.PackedGlobalArrays)
			{
				ShaderParameters[CrossCompiler::SHADER_STAGE_PIXEL].PrepareGlobalUniforms(PackedGlobalArray.TypeIndex, PackedGlobalArray.Size);
			}
		}
	}
}

void FMetalStateCache::SetIndexType(EMetalIndexType InIndexType)
{
	if (IndexType != InIndexType)
	{
		IndexType = InIndexType;
		
		RasterBits |= EMetalRenderFlagPipelineState;
	}
}

void FMetalStateCache::BindUniformBuffer(EShaderFrequency const Freq, uint32 const BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	check(BufferIndex < ML_MaxBuffers);
	if (BoundUniformBuffers[Freq][BufferIndex] != BufferRHI)
	{
		BoundUniformBuffers[Freq][BufferIndex] = BufferRHI;
		DirtyUniformBuffers[Freq] |= 1 << BufferIndex;
	}
}

void FMetalStateCache::SetDirtyUniformBuffers(EShaderFrequency const Freq, uint32 const Dirty)
{
	DirtyUniformBuffers[Freq] = Dirty;
}

void FMetalStateCache::SetVisibilityResultMode(MTLVisibilityResultMode const Mode, NSUInteger const Offset)
{
	if (VisibilityMode != Mode || VisibilityOffset != Offset)
	{
		VisibilityMode = Mode;
		VisibilityOffset = Offset;
		
		RasterBits |= EMetalRenderFlagVisibilityResultMode;
	}
}

void FMetalStateCache::ConditionalUpdateBackBuffer(FMetalSurface& Surface)
{
	// are we setting the back buffer? if so, make sure we have the drawable
	if ((Surface.Flags & TexCreate_Presentable))
	{
		// update the back buffer texture the first time used this frame
		if (Surface.Texture == nil)
		{
			// set the texture into the backbuffer
			Surface.GetDrawableTexture();
		}
#if PLATFORM_MAC
		check (Surface.Texture);
#endif
	}
}

bool FMetalStateCache::NeedsToSetRenderTarget(const FRHISetRenderTargetsInfo& InRenderTargetsInfo)
{
	// see if our new Info matches our previous Info
	
	// basic checks
	bool bAllChecksPassed = GetHasValidRenderTarget() && bIsRenderTargetActive && InRenderTargetsInfo.NumColorRenderTargets == RenderTargetsInfo.NumColorRenderTargets && InRenderTargetsInfo.NumUAVs == RenderTargetsInfo.NumUAVs &&
		(InRenderTargetsInfo.DepthStencilRenderTarget.Texture == RenderTargetsInfo.DepthStencilRenderTarget.Texture);

	// now check each color target if the basic tests passe
	if (bAllChecksPassed)
	{
		for (int32 RenderTargetIndex = 0; RenderTargetIndex < InRenderTargetsInfo.NumColorRenderTargets; RenderTargetIndex++)
		{
			const FRHIRenderTargetView& RenderTargetView = InRenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
			const FRHIRenderTargetView& PreviousRenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];

			// handle simple case of switching textures or mip/slice
			if (RenderTargetView.Texture != PreviousRenderTargetView.Texture ||
				RenderTargetView.MipIndex != PreviousRenderTargetView.MipIndex ||
				RenderTargetView.ArraySliceIndex != PreviousRenderTargetView.ArraySliceIndex)
			{
				bAllChecksPassed = false;
				break;
			}
			
			// it's non-trivial when we need to switch based on load/store action:
			// LoadAction - it only matters what we are switching to in the new one
			//    If we switch to Load, no need to switch as we can re-use what we already have
			//    If we switch to Clear, we have to always switch to a new RT to force the clear
			//    If we switch to DontCare, there's definitely no need to switch
			//    If we switch *from* Clear then we must change target as we *don't* want to clear again.
            if (RenderTargetView.LoadAction == ERenderTargetLoadAction::EClear)
            {
                bAllChecksPassed = false;
                break;
            }
            // StoreAction - this matters what the previous one was **In Spirit**
            //    If we come from Store, we need to switch to a new RT to force the store
            //    If we come from DontCare, then there's no need to switch
            //    @todo metal: However, we basically only use Store now, and don't
            //        care about intermediate results, only final, so we don't currently check the value
            //			if (PreviousRenderTargetView.StoreAction == ERenderTTargetStoreAction::EStore)
            //			{
            //				bAllChecksPassed = false;
            //				break;
            //			}
        }
        
        if (InRenderTargetsInfo.DepthStencilRenderTarget.Texture && (InRenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction == ERenderTargetLoadAction::EClear || InRenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction == ERenderTargetLoadAction::EClear))
        {
            bAllChecksPassed = false;
		}
		
		if (InRenderTargetsInfo.DepthStencilRenderTarget.Texture && (InRenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction > RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction || InRenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction() > RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction()))
		{
			// Don't break the encoder if we can just change the store actions.
			if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesDeferredStoreActions))
			{
				MTLStoreAction NewDepthStore = DepthStore;
				MTLStoreAction NewStencilStore = StencilStore;
				if (InRenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction > RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction)
				{
					if (RenderPassDesc.depthAttachment.texture != nil)
					{
						FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderTargetsInfo.DepthStencilRenderTarget.Texture);
						
						const uint32 DepthSampleCount = (Surface.MSAATexture ? Surface.MSAATexture.sampleCount : Surface.Texture.sampleCount);
						bool const bDepthStencilSampleCountMismatchFixup = (SampleCount != DepthSampleCount);

						ERenderTargetStoreAction HighLevelStoreAction = (Surface.MSAATexture && !bDepthStencilSampleCountMismatchFixup) ? ERenderTargetStoreAction::EMultisampleResolve : RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction;
						
						NewDepthStore = GetMetalRTStoreAction(HighLevelStoreAction);
					}
					else
					{
						bAllChecksPassed = false;
					}
				}
				
				if (InRenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction() > RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction())
				{
					if (RenderPassDesc.stencilAttachment.texture != nil)
					{
						NewStencilStore = GetMetalRTStoreAction(RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction());
					}
					else
					{
						bAllChecksPassed = false;
					}
				}
				
				if (bAllChecksPassed)
				{
					DepthStore = NewDepthStore;
					StencilStore = NewStencilStore;
				}
			}
			else
			{
				bAllChecksPassed = false;
			}
		}
	}

	// if we are setting them to nothing, then this is probably end of frame, and we can't make a framebuffer
	// with nothng, so just abort this (only need to check on single MRT case)
	if (InRenderTargetsInfo.NumColorRenderTargets == 1 && InRenderTargetsInfo.ColorRenderTarget[0].Texture == nullptr &&
		InRenderTargetsInfo.DepthStencilRenderTarget.Texture == nullptr)
	{
		bAllChecksPassed = true;
	}

	return bAllChecksPassed == false;
}

void FMetalStateCache::SetShaderBuffer(EShaderFrequency const Frequency, id<MTLBuffer> const Buffer, FMetalBufferData* const Bytes, NSUInteger const Offset, NSUInteger const Length, NSUInteger const Index, EPixelFormat const Format)
{
	check(Frequency < SF_NumFrequencies);
	check(Index < ML_MaxBuffers);
	
	if (ShaderBuffers[Frequency].Buffers[Index].Buffer != Buffer ||
		ShaderBuffers[Frequency].Buffers[Index].Bytes != Bytes ||
		ShaderBuffers[Frequency].Buffers[Index].Offset != Offset ||
		ShaderBuffers[Frequency].Buffers[Index].Length != Length ||
		ShaderBuffers[Frequency].Buffers[Index].Type != Format)
	{
		ShaderBuffers[Frequency].Buffers[Index].Buffer = Buffer;
		ShaderBuffers[Frequency].Buffers[Index].Bytes = Bytes;
		ShaderBuffers[Frequency].Buffers[Index].Offset = Offset;
		ShaderBuffers[Frequency].Buffers[Index].Length = Length;
		ShaderBuffers[Frequency].Buffers[Index].Type = Format;
		
		if (Buffer || Bytes)
		{
			ShaderBuffers[Frequency].Bound |= (1 << Index);
		}
		else
		{
			ShaderBuffers[Frequency].Bound &= ~(1 << Index);
		}
	}
}

void FMetalStateCache::SetShaderTexture(EShaderFrequency const Frequency, id<MTLTexture> Texture, NSUInteger const Index)
{
	check(Frequency < SF_NumFrequencies);
	check(Index < ML_MaxTextures);
	
	if (ShaderTextures[Frequency].Textures[Index] != Texture)
	{
		ShaderTextures[Frequency].Textures[Index] = Texture;
		
		if (Texture)
		{
			ShaderTextures[Frequency].Bound |= (1 << Index);
		}
		else
		{
			ShaderTextures[Frequency].Bound &= ~(1 << Index);
		}
	}
}

void FMetalStateCache::SetShaderSamplerState(EShaderFrequency const Frequency, FMetalSamplerState* const Sampler, NSUInteger const Index)
{
	check(Frequency < SF_NumFrequencies);
	check(Index < ML_MaxSamplers);
	
	if (ShaderSamplers[Frequency].Samplers[Index] != Sampler)
	{
		ShaderSamplers[Frequency].Samplers[Index] = Sampler;

		if (Sampler)
		{
			ShaderSamplers[Frequency].Bound |= (1 << Index);
		}
		else
		{
			ShaderSamplers[Frequency].Bound &= ~(1 << Index);
		}
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FRHITexture* RESTRICT TextureRHI, float CurrentTime)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	id<MTLTexture> Texture = nil;
	if (Surface != nullptr)
	{
		TextureRHI->SetLastRenderTime(CurrentTime);
		Texture = Surface->Texture;
	}
	
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderTexture(SF_Pixel, Texture, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderTexture(SF_Vertex, Texture, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderTexture(SF_Compute, Texture, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_HULL:
			SetShaderTexture(SF_Hull, Texture, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_DOMAIN:
			SetShaderTexture(SF_Domain, Texture, BindIndex);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalStateCache::SetShaderResourceView(FMetalContext* Context, EShaderFrequency ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV)
{
	if (SRV)
	{
		FRHITexture* Texture = SRV->SourceTexture.GetReference();
		FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
		FMetalIndexBuffer* IB = SRV->SourceIndexBuffer.GetReference();
		FMetalStructuredBuffer* SB = SRV->SourceStructuredBuffer.GetReference();
		if (Texture)
		{
			FMetalSurface* Surface = SRV->TextureView;
			if (Surface != nullptr)
			{
				SetShaderTexture(ShaderStage, Surface->Texture, BindIndex);
			}
			else
			{
				SetShaderTexture(ShaderStage, nil, BindIndex);
			}
		}
		else if (SRV->GetLinearTexture(false))
		{
			SetShaderTexture(ShaderStage, SRV->GetLinearTexture(false), BindIndex);
		}
		else if (VB)
		{
			SetShaderBuffer(ShaderStage, VB->Buffer, VB->Data, 0, VB->GetSize(), BindIndex, (EPixelFormat)SRV->Format);
		}
		else if (IB)
		{
			SetShaderBuffer(ShaderStage, IB->Buffer, nil, 0, IB->GetSize(), BindIndex, (EPixelFormat)SRV->Format);
		}
		else if (SB)
		{
			SetShaderBuffer(ShaderStage, SB->Buffer, nil, 0, SB->GetSize(), BindIndex);
		}
	}
}

bool FMetalStateCache::IsAtomicUAV(EShaderFrequency ShaderStage, uint32 BindIndex)
{
	check(BindIndex < 8);
	switch (ShaderStage)
	{
		case SF_Vertex:
		{
			return (GraphicsPSO->VertexShader->Bindings.AtomicUAVs & (1 << BindIndex)) != 0;
			break;
		}
		case SF_Pixel:
		{
			return (GraphicsPSO->PixelShader->Bindings.AtomicUAVs & (1 << BindIndex)) != 0;
			break;
		}
		case SF_Hull:
		{
			return (GraphicsPSO->HullShader->Bindings.AtomicUAVs & (1 << BindIndex)) != 0;
			break;
		}
		case SF_Domain:
		{
			return (GraphicsPSO->DomainShader->Bindings.AtomicUAVs & (1 << BindIndex)) != 0;
			break;
		}
		case SF_Compute:
		{
			return (ComputeShader->Bindings.AtomicUAVs & (1 << BindIndex)) != 0;
		}
		default:
		{
			check(false);
			return false;
		}
	}
}

void FMetalStateCache::SetShaderUnorderedAccessView(EShaderFrequency ShaderStage, uint32 BindIndex, FMetalUnorderedAccessView* RESTRICT UAV)
{
	if (UAV)
	{
		// figure out which one of the resources we need to set
		FMetalStructuredBuffer* StructuredBuffer = UAV->SourceView->SourceStructuredBuffer.GetReference();
		FMetalVertexBuffer* VertexBuffer = UAV->SourceView->SourceVertexBuffer.GetReference();
		FRHITexture* Texture = UAV->SourceView->SourceTexture.GetReference();
		FMetalSurface* Surface = UAV->SourceView->TextureView;
		if (StructuredBuffer)
		{
			SetShaderBuffer(ShaderStage, StructuredBuffer->Buffer, nil, 0, StructuredBuffer->GetSize(), BindIndex);
		}
		else if (VertexBuffer)
		{
			check(!VertexBuffer->Data && VertexBuffer->Buffer);
			if (!IsAtomicUAV(ShaderStage, BindIndex) && UAV->SourceView->GetLinearTexture(true))
			{
				SetShaderTexture(ShaderStage, UAV->SourceView->GetLinearTexture(true), BindIndex);
			}
			else
			{
				SetShaderBuffer(ShaderStage, VertexBuffer->Buffer, VertexBuffer->Data, 0, VertexBuffer->GetSize(), BindIndex, (EPixelFormat)UAV->SourceView->Format);
			}
		}
		else if (Texture)
		{
			if (!Surface)
			{
				Surface = GetMetalSurfaceFromRHITexture(Texture);
			}
			if (Surface != nullptr)
			{
				FMetalSurface* Source = GetMetalSurfaceFromRHITexture(Texture);
				
				FPlatformAtomics::InterlockedExchange(&Surface->Written, 1);
				FPlatformAtomics::InterlockedExchange(&Source->Written, 1);
				
				SetShaderTexture(ShaderStage, Surface->Texture, BindIndex);
			}
			else
			{
				SetShaderTexture(ShaderStage, nil, BindIndex);
			}
		}
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV, float CurrentTime)
{
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderResourceView(nullptr, SF_Pixel, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderResourceView(nullptr, SF_Vertex, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderResourceView(nullptr, SF_Compute, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_HULL:
			SetShaderResourceView(nullptr, SF_Hull, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_DOMAIN:
			SetShaderResourceView(nullptr, SF_Domain, BindIndex, SRV);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalSamplerState* RESTRICT SamplerState, float CurrentTime)
{
	check(SamplerState->State != nil);
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderSamplerState(SF_Pixel, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderSamplerState(SF_Vertex, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderSamplerState(SF_Compute, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_HULL:
			SetShaderSamplerState(SF_Hull, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_DOMAIN:
			SetShaderSamplerState(SF_Domain, SamplerState, BindIndex);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalUnorderedAccessView* RESTRICT UAV, float CurrentTime)
{
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderUnorderedAccessView(SF_Pixel, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderUnorderedAccessView(SF_Vertex, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderUnorderedAccessView(SF_Compute, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_HULL:
			SetShaderUnorderedAccessView(SF_Hull, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_DOMAIN:
			SetShaderUnorderedAccessView(SF_Domain, BindIndex, UAV);
			break;
			
		default:
			check(0);
			break;
	}
}


template <typename MetalResourceType>
inline int32 FMetalStateCache::SetShaderResourcesFromBuffer(uint32 ShaderStage, FMetalUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex, float CurrentTime)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);
			
			MetalResourceType* ResourcePtr = (MetalResourceType*)Resources[ResourceIndex].GetReference();
			
			// todo: could coalesce adjacent bound resources.
			SetResource(ShaderStage, BindIndex, ResourcePtr, CurrentTime);
			
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}

template <class ShaderType>
void FMetalStateCache::SetResourcesFromTables(ShaderType Shader, uint32 ShaderStage)
{
	checkSlow(Shader);
	
	if (!FShaderCache::IsPredrawCall(ShaderCacheContextState))
	{
		EShaderFrequency Frequency;
		switch(ShaderStage)
		{
			case CrossCompiler::SHADER_STAGE_VERTEX:
				Frequency = SF_Vertex;
				break;
			case CrossCompiler::SHADER_STAGE_HULL:
				Frequency = SF_Hull;
				break;
			case CrossCompiler::SHADER_STAGE_DOMAIN:
				Frequency = SF_Domain;
				break;
			case CrossCompiler::SHADER_STAGE_PIXEL:
				Frequency = SF_Pixel;
				break;
			case CrossCompiler::SHADER_STAGE_COMPUTE:
				Frequency = SF_Compute;
				break;
			default:
				Frequency = SF_NumFrequencies; //Silence a compiler warning/error
				check(false);
				break;
		}

        float CurrentTime = FPlatformTime::Seconds();

		// Mask the dirty bits by those buffers from which the shader has bound resources.
		uint32 DirtyBits = Shader->Bindings.ShaderResourceTable.ResourceTableBits & GetDirtyUniformBuffers(Frequency);
		while (DirtyBits)
		{
			// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
			const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
			const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
			DirtyBits ^= LowestBitMask;
			FMetalUniformBuffer* Buffer = (FMetalUniformBuffer*)GetBoundUniformBuffers(Frequency)[BufferIndex].GetReference();
			check(Buffer);
			check(BufferIndex < Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes.Num());
			check(Buffer->GetLayout().GetHash() == Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);
			
			// todo: could make this two pass: gather then set
			SetShaderResourcesFromBuffer<FRHITexture>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.TextureMap.GetData(), BufferIndex, CurrentTime);
			SetShaderResourcesFromBuffer<FMetalShaderResourceView>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex, CurrentTime);
			SetShaderResourcesFromBuffer<FMetalSamplerState>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.SamplerMap.GetData(), BufferIndex, CurrentTime);
			SetShaderResourcesFromBuffer<FMetalUnorderedAccessView>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.UnorderedAccessViewMap.GetData(), BufferIndex, CurrentTime);
		}
		SetDirtyUniformBuffers(Frequency, 0);
	}
}

void FMetalStateCache::CommitRenderResources(FMetalCommandEncoder* Raster)
{
	check(IsValidRef(GraphicsPSO));
    
    SetResourcesFromTables(GraphicsPSO->VertexShader, CrossCompiler::SHADER_STAGE_VERTEX);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedUniformBuffers(this, GraphicsPSO, nullptr, CrossCompiler::SHADER_STAGE_VERTEX, GetBoundUniformBuffers(SF_Vertex), GraphicsPSO->VertexShader->UniformBuffersCopyInfo);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedGlobals(this, Raster, SF_Vertex, GraphicsPSO->VertexShader->Bindings);
	
    if (IsValidRef(GraphicsPSO->PixelShader))
    {
    	SetResourcesFromTables(GraphicsPSO->PixelShader, CrossCompiler::SHADER_STAGE_PIXEL);
        GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedUniformBuffers(this, GraphicsPSO, nullptr, CrossCompiler::SHADER_STAGE_PIXEL, GetBoundUniformBuffers(SF_Pixel), GraphicsPSO->PixelShader->UniformBuffersCopyInfo);
        GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedGlobals(this, Raster, SF_Pixel, GraphicsPSO->PixelShader->Bindings);
    }
}

void FMetalStateCache::CommitTessellationResources(FMetalCommandEncoder* Raster, FMetalCommandEncoder* Compute)
{
	check(IsValidRef(GraphicsPSO));
    check(IsValidRef(GraphicsPSO->HullShader) && IsValidRef(GraphicsPSO->DomainShader));
    
    SetResourcesFromTables(GraphicsPSO->VertexShader, CrossCompiler::SHADER_STAGE_VERTEX);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedUniformBuffers(this, GraphicsPSO, nullptr, CrossCompiler::SHADER_STAGE_VERTEX, GetBoundUniformBuffers(SF_Vertex), GraphicsPSO->VertexShader->UniformBuffersCopyInfo);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedGlobals(this, Compute, SF_Vertex, GraphicsPSO->VertexShader->Bindings);
	
    if (IsValidRef(GraphicsPSO->PixelShader))
    {
    	SetResourcesFromTables(GraphicsPSO->PixelShader, CrossCompiler::SHADER_STAGE_PIXEL);
        GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedUniformBuffers(this, GraphicsPSO, nullptr, CrossCompiler::SHADER_STAGE_PIXEL, GetBoundUniformBuffers(SF_Pixel), GraphicsPSO->PixelShader->UniformBuffersCopyInfo);
        GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedGlobals(this, Raster, SF_Pixel, GraphicsPSO->PixelShader->Bindings);
    }
    
    SetResourcesFromTables(GraphicsPSO->HullShader, CrossCompiler::SHADER_STAGE_HULL);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_HULL).CommitPackedUniformBuffers(this, GraphicsPSO, nullptr, CrossCompiler::SHADER_STAGE_HULL, GetBoundUniformBuffers(SF_Hull), GraphicsPSO->HullShader->UniformBuffersCopyInfo);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_HULL).CommitPackedGlobals(this, Compute, SF_Hull, GraphicsPSO->HullShader->Bindings);
	
	SetResourcesFromTables(GraphicsPSO->DomainShader, CrossCompiler::SHADER_STAGE_DOMAIN);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_DOMAIN).CommitPackedUniformBuffers(this, GraphicsPSO, nullptr, CrossCompiler::SHADER_STAGE_DOMAIN, GetBoundUniformBuffers(SF_Domain), GraphicsPSO->DomainShader->UniformBuffersCopyInfo);
    GetShaderParameters(CrossCompiler::SHADER_STAGE_DOMAIN).CommitPackedGlobals(this, Raster, SF_Domain, GraphicsPSO->DomainShader->Bindings);
}

void FMetalStateCache::CommitComputeResources(FMetalCommandEncoder* Compute)
{
	check(IsValidRef(ComputeShader));
	SetResourcesFromTables(ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE);
	
	GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedUniformBuffers(this, GraphicsPSO, ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE, GetBoundUniformBuffers(SF_Compute), ComputeShader->UniformBuffersCopyInfo);
	GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedGlobals(this, Compute, SF_Compute, ComputeShader->Bindings);
}

bool FMetalStateCache::PrepareToRestart(void)
{
	if(CanRestartRenderPass())
	{
		return true;
	}
	else
	{
		if (SampleCount <= 1)
		{
			// Deferred store actions make life a bit easier...
			static bool bSupportsDeferredStore = GetMetalDeviceContext().GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
			
			FRHISetRenderTargetsInfo Info = GetRenderTargetsInfo();
			for (int32 RenderTargetIndex = 0; RenderTargetIndex < Info.NumColorRenderTargets; RenderTargetIndex++)
			{
				FRHIRenderTargetView& RenderTargetView = Info.ColorRenderTarget[RenderTargetIndex];
				RenderTargetView.LoadAction = ERenderTargetLoadAction::ELoad;
				check(RenderTargetView.Texture == nil || RenderTargetView.StoreAction == ERenderTargetStoreAction::EStore);
			}
			Info.bClearColor = false;
			
			if (Info.DepthStencilRenderTarget.Texture)
			{
				Info.DepthStencilRenderTarget.DepthLoadAction = ERenderTargetLoadAction::ELoad;
				check(bSupportsDeferredStore || !Info.DepthStencilRenderTarget.GetDepthStencilAccess().IsDepthWrite() || Info.DepthStencilRenderTarget.DepthStoreAction == ERenderTargetStoreAction::EStore);
				Info.bClearDepth = false;
				
				Info.DepthStencilRenderTarget.StencilLoadAction = ERenderTargetLoadAction::ELoad;
				// @todo Stencil writes that need to persist must use ERenderTargetStoreAction::EStore on iOS.
				// We should probably be using deferred store actions so that we can safely lazily instantiate encoders.
				check(bSupportsDeferredStore || !Info.DepthStencilRenderTarget.GetDepthStencilAccess().IsStencilWrite() || Info.DepthStencilRenderTarget.GetStencilStoreAction() == ERenderTargetStoreAction::EStore);
				Info.bClearStencil = false;
			}
			
			InvalidateRenderTargets();
			return SetRenderTargetsInfo(Info, GetVisibilityResultsBuffer(), true) && CanRestartRenderPass();
		}
		else
		{
			return false;
		}
	}
}

void FMetalStateCache::SetStateDirty(void)
{
	RasterBits = UINT32_MAX;
	for (uint32 i = 0; i < SF_NumFrequencies; i++)
	{
		ShaderBuffers[i].Bound = UINT32_MAX;
#if PLATFORM_MAC
#ifndef UINT128_MAX
#define UINT128_MAX (((__uint128_t)1 << 127) - (__uint128_t)1 + ((__uint128_t)1 << 127))
#endif
		ShaderTextures[i].Bound = UINT128_MAX;
#else
		ShaderTextures[i].Bound = UINT32_MAX;
#endif
		ShaderSamplers[i].Bound = UINT16_MAX;
	}
}

void FMetalStateCache::SetRenderStoreActions(FMetalCommandEncoder& CommandEncoder, bool const bConditionalSwitch)
{
	check(CommandEncoder.IsRenderCommandEncoderActive())
	{
		// Deferred store actions make life a bit easier...
		static bool bSupportsDeferredStore = GetMetalDeviceContext().GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
		if (bConditionalSwitch && bSupportsDeferredStore)
		{
			for (int32 RenderTargetIndex = 0; RenderTargetIndex < RenderTargetsInfo.NumColorRenderTargets; RenderTargetIndex++)
			{
				FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
				if(RenderTargetView.Texture != nil)
				{
					const bool bMultiSampled = ([RenderPassDesc.colorAttachments objectAtIndexedSubscript:RenderTargetIndex].texture.sampleCount > 1);
					ColorStore[RenderTargetIndex] = GetConditionalMetalRTStoreAction(bMultiSampled);
				}
			}
			
			if (RenderTargetsInfo.DepthStencilRenderTarget.Texture)
			{
				const bool bMultiSampled = RenderPassDesc.depthAttachment.texture && (RenderPassDesc.depthAttachment.texture.sampleCount > 1);
				DepthStore = GetConditionalMetalRTStoreAction(bMultiSampled);
				StencilStore = GetConditionalMetalRTStoreAction(false);
			}
		}
		CommandEncoder.SetRenderPassStoreActions(ColorStore, DepthStore, StencilStore);
	}
}

void FMetalStateCache::SetRenderState(FMetalCommandEncoder& CommandEncoder, FMetalCommandEncoder* PrologueEncoder)
{
	if (RasterBits)
	{
		if (RasterBits & EMetalRenderFlagViewport)
		{
			CommandEncoder.SetViewport(Viewport, ActiveViewports);
		}
		if (RasterBits & EMetalRenderFlagFrontFacingWinding)
		{
			CommandEncoder.SetFrontFacingWinding(MTLWindingCounterClockwise);
		}
		if (RasterBits & EMetalRenderFlagCullMode)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetCullMode(TranslateCullMode(RasterizerState->State.CullMode));
		}
		if (RasterBits & EMetalRenderFlagDepthBias)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetDepthBias(RasterizerState->State.DepthBias, RasterizerState->State.SlopeScaleDepthBias, FLT_MAX);
		}
		if ((RasterBits & EMetalRenderFlagScissorRect) && !FShaderCache::IsPredrawCall(ShaderCacheContextState))
		{
			CommandEncoder.SetScissorRect(Scissor, ActiveScissors);
		}
		if (RasterBits & EMetalRenderFlagTriangleFillMode)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetTriangleFillMode(TranslateFillMode(RasterizerState->State.FillMode));
		}
		if (RasterBits & EMetalRenderFlagBlendColor)
		{
			CommandEncoder.SetBlendColor(BlendFactor.R, BlendFactor.G, BlendFactor.B, BlendFactor.A);
		}
		if (RasterBits & EMetalRenderFlagDepthStencilState)
		{
			check(IsValidRef(DepthStencilState));
			CommandEncoder.SetDepthStencilState(DepthStencilState ? DepthStencilState->State : nil);
		}
		if (RasterBits & EMetalRenderFlagStencilReferenceValue)
		{
			CommandEncoder.SetStencilReferenceValue(StencilRef);
		}
		if (RasterBits & EMetalRenderFlagVisibilityResultMode)
		{
			CommandEncoder.SetVisibilityResultMode(VisibilityMode, VisibilityOffset);
		}
		// Some Intel drivers need RenderPipeline state to be set after DepthStencil state to work properly
		if (RasterBits & EMetalRenderFlagPipelineState)
		{
			check(GetPipelineState());
			CommandEncoder.SetRenderPipelineState(GetPipelineState());
			if (GetPipelineState().ComputePipelineState)
			{
				check(PrologueEncoder);
				PrologueEncoder->SetComputePipelineState(GetPipelineState());
			}
		}
		RasterBits = 0;
	}
}

void FMetalStateCache::CommitResourceTable(EShaderFrequency const Frequency, MTLFunctionType const Type, FMetalCommandEncoder& CommandEncoder)
{
	FMetalBufferBindings& BufferBindings = ShaderBuffers[Frequency];
	while(BufferBindings.Bound)
	{
		uint32 Index = __builtin_ctz(BufferBindings.Bound);
		BufferBindings.Bound &= ~(1 << Index);
		
		if (Index < ML_MaxBuffers)
		{
			FMetalBufferBinding& Binding = BufferBindings.Buffers[Index];
			if (Binding.Buffer)
			{
				CommandEncoder.SetShaderBuffer(Type, Binding.Buffer, Binding.Offset, Binding.Length, Index, Binding.Type);
			}
			else if (Binding.Bytes)
			{
				CommandEncoder.SetShaderData(Type, Binding.Bytes, Binding.Offset, Index);
			}
		}
	}
	
	FMetalTextureBindings& TextureBindings = ShaderTextures[Frequency];
#if PLATFORM_MAC
	uint64 LoTextures = (uint64)TextureBindings.Bound;
	while(LoTextures)
	{
		uint32 Index = __builtin_ctzll(LoTextures);
		LoTextures &= ~(uint64(1) << uint64(Index));
		
		if (Index < ML_MaxTextures && TextureBindings.Textures[Index])
		{
			CommandEncoder.SetShaderTexture(Type, TextureBindings.Textures[Index], Index);
		}
	}
	
	uint64 HiTextures = (uint64)(TextureBindings.Bound >> FMetalTextureMask(64));
	while(HiTextures)
	{
		uint32 Index = __builtin_ctzll(HiTextures);
		HiTextures &= ~(uint64(1) << uint64(Index));
		
		if (Index < ML_MaxTextures && TextureBindings.Textures[Index])
		{
			CommandEncoder.SetShaderTexture(Type, TextureBindings.Textures[Index], Index + 64);
		}
	}
	
	TextureBindings.Bound = FMetalTextureMask(LoTextures) | (FMetalTextureMask(HiTextures) << FMetalTextureMask(64));
	check(TextureBindings.Bound == 0);
#else
	while(TextureBindings.Bound)
	{
		uint32 Index = __builtin_ctz(TextureBindings.Bound);
		TextureBindings.Bound &= ~(FMetalTextureMask(FMetalTextureMask(1) << FMetalTextureMask(Index)));
		
		if (Index < ML_MaxTextures && TextureBindings.Textures[Index])
		{
			CommandEncoder.SetShaderTexture(Type, TextureBindings.Textures[Index], Index);
		}
	}
#endif
	
    FMetalSamplerBindings& SamplerBindings = ShaderSamplers[Frequency];
	while(SamplerBindings.Bound)
	{
		uint32 Index = __builtin_ctz(SamplerBindings.Bound);
		SamplerBindings.Bound &= ~(1 << Index);
		
		if (Index < ML_MaxSamplers && IsValidRef(SamplerBindings.Samplers[Index]))
		{
			CommandEncoder.SetShaderSamplerState(Type, SamplerBindings.Samplers[Index]->State, Index);
		}
	}
}

FTexture2DRHIRef FMetalStateCache::CreateFallbackDepthStencilSurface(uint32 Width, uint32 Height)
{
	if (!IsValidRef(FallbackDepthStencilSurface) || FallbackDepthStencilSurface->GetSizeX() != Width || FallbackDepthStencilSurface->GetSizeY() != Height)
	{
		FRHIResourceCreateInfo TexInfo;
		FallbackDepthStencilSurface = RHICreateTexture2D(Width, Height, PF_DepthStencil, 1, 1, TexCreate_DepthStencilTargetable, TexInfo);
	}
	check(IsValidRef(FallbackDepthStencilSurface));
	return FallbackDepthStencilSurface;
}

