// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalComputeCommandEncoder.h"
#include "MetalRenderCommandEncoder.h"
#include "MetalProfiler.h"

const uint32 EncoderRingBufferSize = 1024 * 1024;

#if METAL_DEBUG_OPTIONS
extern int32 GMetalBufferScribble;
#endif

#pragma mark - Block-allocated ring-buffer capture wrapper -
@interface FMetalRingBufferCapture : FApplePlatformObject
{
	@public
	TSharedPtr<FMetalRingBuffer, ESPMode::ThreadSafe> Buffer;
}
-(instancetype)initWithRingBuffer:(TSharedPtr<FMetalRingBuffer, ESPMode::ThreadSafe>)InBuffer;
@end

@implementation FMetalRingBufferCapture
APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalRingBufferCapture)
-(instancetype)initWithRingBuffer:(TSharedPtr<FMetalRingBuffer, ESPMode::ThreadSafe>)InBuffer TSAN_SAFE
{
	self = [super init];
	if (self)
	{
		Buffer = InBuffer;
	}
	return self;
}
@end

#pragma mark - Public C++ Boilerplate -

FMetalCommandEncoder::FMetalCommandEncoder(FMetalCommandList& CmdList)
: CommandList(CmdList)
, bSupportsMetalFeaturesSetBytes(CmdList.GetCommandQueue().SupportsFeature(EMetalFeaturesSetBytes))
, RingBuffer(CommandList.GetCommandQueue().GetDevice(), CmdList.GetCommandQueue().GetCompatibleResourceOptions(MTLResourceHazardTrackingModeUntracked), EncoderRingBufferSize, BufferOffsetAlignment)
, RenderPassDesc(nil)
, CommandBuffer(nil)
, RenderCommandEncoder(nil)
, ComputeCommandEncoder(nil)
, BlitCommandEncoder(nil)
, EncoderFence(nil)
, CompletionHandlers(nil)
, DebugGroups([NSMutableArray new])
{
	FMemory::Memzero(ShaderBuffers);

	MTLResourceOptions ResOptions = CmdList.GetCommandQueue().GetCompatibleResourceOptions(MTLResourceHazardTrackingModeUntracked);
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		ColorStoreActions[i] = MTLStoreActionUnknown;
	}
	DepthStoreAction = MTLStoreActionUnknown;
	StencilStoreAction = MTLStoreActionUnknown;
}

FMetalCommandEncoder::~FMetalCommandEncoder(void)
{
	if(CommandBuffer)
	{
		EndEncoding();
		CommitCommandBuffer(false);
	}
	
	check(!IsRenderCommandEncoderActive());
	check(!IsComputeCommandEncoderActive());
	check(!IsBlitCommandEncoderActive());
	
	[RenderPassDesc release];
	RenderPassDesc = nil;

	if(DebugGroups)
	{
		[DebugGroups release];
	}
}

void FMetalCommandEncoder::Reset(void)
{
    check(!CommandBuffer);
    check(IsRenderCommandEncoderActive() == false
          && IsComputeCommandEncoderActive() == false
          && IsBlitCommandEncoderActive() == false);
	
	if(RenderPassDesc)
	{
		UNTRACK_OBJECT(STAT_MetalRenderPassDescriptorCount, RenderPassDesc);
		[RenderPassDesc release];
		RenderPassDesc = nil;
	}
	
	static bool bDeferredStoreActions = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
	if (bDeferredStoreActions)
	{
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
		{
			ColorStoreActions[i] = MTLStoreActionUnknown;
		}
		DepthStoreAction = MTLStoreActionUnknown;
		StencilStoreAction = MTLStoreActionUnknown;
	}
	
    FMemory::Memzero(ShaderBuffers);
	
	[DebugGroups removeAllObjects];
}

#pragma mark - Public Command Buffer Mutators -

void FMetalCommandEncoder::StartCommandBuffer(void)
{
	check(!CommandBuffer);
	check(IsRenderCommandEncoderActive() == false
          && IsComputeCommandEncoderActive() == false
          && IsBlitCommandEncoderActive() == false);

	CommandBuffer = CommandList.GetCommandQueue().CreateCommandBuffer();
	TRACK_OBJECT(STAT_MetalCommandBufferCount, CommandBuffer);
	CommandBufferPtr.Reset();
	
	if ([DebugGroups count] > 0)
	{
		[CommandBuffer setLabel:[DebugGroups lastObject]];
	}
}
	
void FMetalCommandEncoder::CommitCommandBuffer(uint32 const Flags)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false
          && IsComputeCommandEncoderActive() == false
          && IsBlitCommandEncoderActive() == false);

	if(CommandBuffer.label == nil && [DebugGroups count] > 0)
	{
		[CommandBuffer setLabel:[DebugGroups lastObject]];
	}
	
	bool const bWait = (Flags & EMetalSubmitFlagsWaitOnCommandBuffer);
	if (!(Flags & EMetalSubmitFlagsBreakCommandBuffer))
	{
		uint32 RingBufferOffset = RingBuffer.GetOffset();
		uint32 StartOffset = RingBuffer.LastWritten;
		
		id<MTLBuffer> CurrentRingBuffer = RingBuffer.Buffer->Buffer;
		uint32 BytesWritten = 0;
		if (StartOffset <= RingBufferOffset)
		{
			BytesWritten = RingBufferOffset - StartOffset;
		}
		else
		{
			uint32 TrailLen = CurrentRingBuffer.length - StartOffset;
			BytesWritten = TrailLen + RingBufferOffset;
		}
		
		RingBuffer.FrameSize[GFrameNumberRenderThread % ARRAY_COUNT(RingBuffer.FrameSize)] += Align(BytesWritten, BufferOffsetAlignment);
		
		RingBuffer.LastWritten = RingBufferOffset;
        FMetalRingBufferCapture* WeakRingBufferRef = [[FMetalRingBufferCapture alloc] initWithRingBuffer: RingBuffer.Buffer];
        FPlatformMisc::MemoryBarrier();
		AddCompletionHandler(^(id <MTLCommandBuffer> Buffer)
		{
			TSharedPtr<FMetalRingBuffer, ESPMode::ThreadSafe>& CmdBufferRingBuffer = WeakRingBufferRef->Buffer;
#if METAL_DEBUG_OPTIONS
			if (GMetalBufferScribble && StartOffset != RingBufferOffset)
			{
				 if (StartOffset < RingBufferOffset)
				 {
					FMemory::Memset(((uint8*)CmdBufferRingBuffer->Buffer.contents) + StartOffset, 0xCD, RingBufferOffset - StartOffset);
				 }
				 else
				 {
					uint32 TrailLen = CmdBufferRingBuffer->Buffer.length - StartOffset;
					FMemory::Memset(((uint8*)CmdBufferRingBuffer->Buffer.contents) + StartOffset, 0xCD, TrailLen);
					FMemory::Memset(((uint8*)CmdBufferRingBuffer->Buffer.contents), 0xCD, RingBufferOffset);
				 }
			}
#endif
			CmdBufferRingBuffer->SetLastRead(RingBufferOffset);
			[WeakRingBufferRef release];
		});
	}
	
	if(CommandBufferPtr.IsValid())
	{
		TSharedPtr<MTLCommandBufferRef, ESPMode::ThreadSafe>* CmdBufRef = new TSharedPtr<MTLCommandBufferRef, ESPMode::ThreadSafe>(CommandBufferPtr);
		NSCondition* Condition = (*CmdBufRef)->Condition;
		
		AddCompletionHandler(^(id<MTLCommandBuffer> _Nonnull CompletedBuffer)
		{
			[Condition lock];
		 
			(*CmdBufRef)->bFinished = true;
			delete CmdBufRef;
		 
			[Condition broadcast];
		 
			[Condition unlock];
		});
	}

	CommandList.Commit(CommandBuffer, CompletionHandlers, bWait);
	
	CommandBufferPtr.Reset();
	CommandBuffer = nil;
	[CompletionHandlers release];
	CompletionHandlers = nil;
	if (Flags & EMetalSubmitFlagsCreateCommandBuffer)
	{
		StartCommandBuffer();
		check(CommandBuffer);
	}
}

#pragma mark - Public Command Encoder Accessors -
	
bool FMetalCommandEncoder::IsRenderCommandEncoderActive(void) const
{
	return RenderCommandEncoder != nil;
}

bool FMetalCommandEncoder::IsComputeCommandEncoderActive(void) const
{
	return ComputeCommandEncoder != nil;
}

bool FMetalCommandEncoder::IsBlitCommandEncoderActive(void) const
{
	return BlitCommandEncoder != nil;
}

bool FMetalCommandEncoder::IsImmediate(void) const
{
	return CommandList.IsImmediate();
}

bool FMetalCommandEncoder::IsRenderPassDescriptorValid(void) const
{
	return (RenderPassDesc != nil);
}

id<MTLRenderCommandEncoder> FMetalCommandEncoder::GetRenderCommandEncoder(void) const
{
	check(IsRenderCommandEncoderActive());
	return RenderCommandEncoder;
}

id<MTLComputeCommandEncoder> FMetalCommandEncoder::GetComputeCommandEncoder(void) const
{
	check(IsComputeCommandEncoderActive());
	return ComputeCommandEncoder;
}

id<MTLBlitCommandEncoder> FMetalCommandEncoder::GetBlitCommandEncoder(void) const
{
	check(IsBlitCommandEncoderActive());
	return BlitCommandEncoder;
}

id<MTLFence> FMetalCommandEncoder::GetEncoderFence(void) const
{
	return EncoderFence;
}
	
#pragma mark - Public Command Encoder Mutators -

void FMetalCommandEncoder::BeginRenderCommandEncoding(void)
{
	check(RenderPassDesc);
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	RenderCommandEncoder = [[CommandBuffer renderCommandEncoderWithDescriptor:RenderPassDesc] retain];
	
	check(!EncoderFence);
	NSString* Label = nil;
	
	if(GEmitDrawEvents)
	{
		Label = [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass");
		[RenderCommandEncoder setLabel:Label];
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
				{
					[((NSObject<MTLCommandBuffer>*)CommandBuffer).debugGroups addObject:Group];
				}
				[RenderCommandEncoder pushDebugGroup:Group];
			}
		}
	}
	
	EncoderFence = FMetalFence(CommandList.GetCommandQueue().CreateFence(Label));
}

void FMetalCommandEncoder::BeginComputeCommandEncoding(void)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	ComputeCommandEncoder = [[CommandBuffer computeCommandEncoder] retain];
	
	check(!EncoderFence);
	NSString* Label = nil;
	
	if(GEmitDrawEvents)
	{
		Label = [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass");
		[ComputeCommandEncoder setLabel:Label];
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
				{
					[((NSObject<MTLCommandBuffer>*)CommandBuffer).debugGroups addObject:Group];
				}
				[ComputeCommandEncoder pushDebugGroup:Group];
			}
		}
	}
	EncoderFence = FMetalFence(CommandList.GetCommandQueue().CreateFence(Label));
}

void FMetalCommandEncoder::BeginBlitCommandEncoding(void)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	BlitCommandEncoder = [[CommandBuffer blitCommandEncoder] retain];
	
	check(!EncoderFence);
	NSString* Label = nil;
	
	if(GEmitDrawEvents)
	{
		Label = [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass");
		[BlitCommandEncoder setLabel:Label];
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
				{
					[((NSObject<MTLCommandBuffer>*)CommandBuffer).debugGroups addObject:Group];
				}
				[BlitCommandEncoder pushDebugGroup:Group];
			}
		}
	}
	EncoderFence = FMetalFence(CommandList.GetCommandQueue().CreateFence(Label));
}

id<MTLFence> FMetalCommandEncoder::EndEncoding(void)
{
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	id<MTLFence> Fence = nil;
	@autoreleasepool
	{
		if(IsRenderCommandEncoderActive())
		{
			check(!bSupportsFences || EncoderFence);
			static bool bDeferredStoreActions = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
			if (bDeferredStoreActions)
			{
				check(RenderPassDesc);
				
				for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
				{
					if (RenderPassDesc.colorAttachments[i].texture && RenderPassDesc.colorAttachments[i].storeAction == MTLStoreActionUnknown)
					{
						MTLStoreAction Action = ColorStoreActions[i];
						check(Action != MTLStoreActionUnknown);
						[RenderCommandEncoder setColorStoreAction : Action atIndex : i];
					}
				}
				if (RenderPassDesc.depthAttachment.texture && RenderPassDesc.depthAttachment.storeAction == MTLStoreActionUnknown)
				{
					MTLStoreAction Action = DepthStoreAction;
					check(Action != MTLStoreActionUnknown);
					[RenderCommandEncoder setDepthStoreAction : Action];
				}
				if (RenderPassDesc.stencilAttachment.texture && RenderPassDesc.stencilAttachment.storeAction == MTLStoreActionUnknown)
				{
					MTLStoreAction Action = StencilStoreAction;
					check(Action != MTLStoreActionUnknown);
					[RenderCommandEncoder setStencilStoreAction : Action];
				}
			}
			
			Fence = EncoderFence;
			UpdateFence(Fence);
			
			[RenderCommandEncoder endEncoding];
			[RenderCommandEncoder release];
			RenderCommandEncoder = nil;
			EncoderFence.Reset();
		}
		else if(IsComputeCommandEncoderActive())
		{
			check(!bSupportsFences || EncoderFence);
			Fence = EncoderFence;
			UpdateFence(Fence);
			
			[ComputeCommandEncoder endEncoding];
			[ComputeCommandEncoder release];
			ComputeCommandEncoder = nil;
			EncoderFence.Reset();
		}
		else if(IsBlitCommandEncoderActive())
		{
			check(!bSupportsFences || EncoderFence);
			Fence = EncoderFence;
			UpdateFence(Fence);
			
			[BlitCommandEncoder endEncoding];
			[BlitCommandEncoder release];
			BlitCommandEncoder = nil;
			EncoderFence.Reset();
		}
	}
    
    FMemory::Memzero(ShaderBuffers);
    return Fence;
}

void FMetalCommandEncoder::InsertCommandBufferFence(FMetalCommandBufferFence& Fence, MTLCommandBufferHandler Handler)
{
	check(CommandBuffer);
	
	if(!CommandBufferPtr.IsValid())
	{
		CommandBufferPtr = TSharedPtr<MTLCommandBufferRef, ESPMode::ThreadSafe>(new MTLCommandBufferRef([CommandBuffer retain], [NSCondition new]));
	}
	check(CommandBufferPtr.IsValid());
	
	Fence.CommandBufferRef = CommandBufferPtr;
	
	if (Handler)
	{
		AddCompletionHandler(^(id<MTLCommandBuffer> _Nonnull CompletedBuffer)
		{
			Handler(CompletedBuffer);
		});
	}
}

void FMetalCommandEncoder::AddCompletionHandler(MTLCommandBufferHandler Handler)
{
	check(Handler);
	if (!CompletionHandlers)
	{
		CompletionHandlers = [NSMutableArray new];
	}
	check(CompletionHandlers);
	
	MTLCommandBufferHandler HeapHandler = Block_copy(Handler);
	[CompletionHandlers addObject:HeapHandler];
	Block_release(HeapHandler);
}

void FMetalCommandEncoder::UpdateFence(id<MTLFence> Fence)
{
	check(IsRenderCommandEncoderActive() || IsComputeCommandEncoderActive() || IsBlitCommandEncoderActive());
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	if (bSupportsFences METAL_DEBUG_OPTION(|| CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation))
	{
		check(Fence);
		if (RenderCommandEncoder)
		{
			[(id<MTLRenderCommandEncoderExtensions>)RenderCommandEncoder updateFence:Fence afterStages:(MTLRenderStages)(MTLRenderStageVertex|MTLRenderStageFragment)];
		}
		else if (ComputeCommandEncoder)
		{
			[(id<MTLComputeCommandEncoderExtensions>)ComputeCommandEncoder updateFence:Fence];
		}
		else if (BlitCommandEncoder)
		{
			[(id<MTLBlitCommandEncoderExtensions>)BlitCommandEncoder updateFence:Fence];
		}
	}
}

void FMetalCommandEncoder::WaitForFence(id<MTLFence> Fence)
{
	check(IsRenderCommandEncoderActive() || IsComputeCommandEncoderActive() || IsBlitCommandEncoderActive());
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	if (bSupportsFences METAL_DEBUG_OPTION(|| CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation))
	{
		check(Fence);
		if (RenderCommandEncoder)
		{
			[(id<MTLRenderCommandEncoderExtensions>)RenderCommandEncoder waitForFence:Fence beforeStages:(MTLRenderStages)(MTLRenderStageVertex|MTLRenderStageFragment)];
		}
		else if (ComputeCommandEncoder)
		{
			[(id<MTLComputeCommandEncoderExtensions>)ComputeCommandEncoder waitForFence:Fence];
		}
		else if (BlitCommandEncoder)
		{
			[(id<MTLBlitCommandEncoderExtensions>)BlitCommandEncoder waitForFence:Fence];
		}
	}
}

#pragma mark - Public Debug Support -

void FMetalCommandEncoder::InsertDebugSignpost(NSString* const String)
{
	if (String)
	{
		if (CommandBuffer && CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
		{
			[((NSObject<MTLCommandBuffer>*)CommandBuffer).debugGroups addObject:String];
		}
		if (RenderCommandEncoder)
		{
			[RenderCommandEncoder insertDebugSignpost:String];
		}
		if (ComputeCommandEncoder)
		{
			[ComputeCommandEncoder insertDebugSignpost:String];
		}
		if (BlitCommandEncoder)
		{
			[BlitCommandEncoder insertDebugSignpost:String];
		}
	}
}

void FMetalCommandEncoder::PushDebugGroup(NSString* const String)
{
	if (String)
	{
		if (CommandBuffer && CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
		{
			[((NSObject<MTLCommandBuffer>*)CommandBuffer).debugGroups addObject:String];
		}
		[DebugGroups addObject:String];
		if (RenderCommandEncoder)
		{
			[RenderCommandEncoder pushDebugGroup:String];
		}
		else if (ComputeCommandEncoder)
		{
			[ComputeCommandEncoder pushDebugGroup:String];
		}
		else if (BlitCommandEncoder)
		{
			[BlitCommandEncoder pushDebugGroup:String];
		}
	}
}

void FMetalCommandEncoder::PopDebugGroup(void)
{
	if (DebugGroups.count > 0)
	{
		[DebugGroups removeLastObject];
		if (RenderCommandEncoder)
		{
			[RenderCommandEncoder popDebugGroup];
		}
		else if (ComputeCommandEncoder)
		{
			[ComputeCommandEncoder popDebugGroup];
		}
		else if (BlitCommandEncoder)
		{
			[BlitCommandEncoder popDebugGroup];
		}
	}
}
	
#pragma mark - Public Render State Mutators -

void FMetalCommandEncoder::SetRenderPassDescriptor(MTLRenderPassDescriptor* const RenderPass)
{
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	check(RenderPass);
	
	if(RenderPass != RenderPassDesc)
	{
		[RenderPass retain];
		if(RenderPassDesc)
		{
			GetMetalDeviceContext().ReleaseObject(RenderPassDesc);
		}
		RenderPassDesc = RenderPass;
		
		static bool bDeferredStoreActions = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
		if (bDeferredStoreActions)
		{
			for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
			{
				ColorStoreActions[i] = MTLStoreActionUnknown;
			}
			DepthStoreAction = MTLStoreActionUnknown;
			StencilStoreAction = MTLStoreActionUnknown;
		}
	}
	check(RenderPassDesc);
	
	FMemory::Memzero(ShaderBuffers);
}

void FMetalCommandEncoder::SetRenderPassStoreActions(MTLStoreAction const* const ColorStore, MTLStoreAction const DepthStore, MTLStoreAction const StencilStore)
{
	check(RenderPassDesc);
	static bool bDeferredStoreActions = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
	if (bDeferredStoreActions)
	{
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
		{
			ColorStoreActions[i] = ColorStore[i];
		}
		DepthStoreAction = DepthStore;
		StencilStoreAction = StencilStore;
	}
}

void FMetalCommandEncoder::SetRenderPipelineState(FMetalShaderPipeline* PipelineState)
{
	check (RenderCommandEncoder);
	{
		METAL_SET_RENDER_REFLECTION(RenderCommandEncoder, PipelineState);
		[RenderCommandEncoder setRenderPipelineState:PipelineState.RenderPipelineState];
	}
}

void FMetalCommandEncoder::SetViewport(MTLViewport const Viewport[], uint32 NumActive)
{
	check(RenderCommandEncoder);
	check(NumActive >= 1 && NumActive < ML_MaxViewports);
	if (NumActive == 1)
	{
		[RenderCommandEncoder setViewport:Viewport[0]];
	}
	else
	{
		check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports));
		[((id<IMetalRenderCommandEncoder>)RenderCommandEncoder) setViewports:Viewport count:NumActive];
	}
}

void FMetalCommandEncoder::SetFrontFacingWinding(MTLWinding const InFrontFacingWinding)
{
    check (RenderCommandEncoder);
	{
		[RenderCommandEncoder setFrontFacingWinding:InFrontFacingWinding];
	}
}

void FMetalCommandEncoder::SetCullMode(MTLCullMode const InCullMode)
{
    check (RenderCommandEncoder);
	{
		[RenderCommandEncoder setCullMode:InCullMode];
	}
}

void FMetalCommandEncoder::SetDepthBias(float const InDepthBias, float const InSlopeScale, float const InClamp)
{
    check (RenderCommandEncoder);
	{
		[RenderCommandEncoder setDepthBias:InDepthBias slopeScale:InSlopeScale clamp:InClamp];
	}
}

void FMetalCommandEncoder::SetScissorRect(MTLScissorRect const Rect[], uint32 NumActive)
{
    check(RenderCommandEncoder);
	check(NumActive >= 1 && NumActive < ML_MaxViewports);
	if (NumActive == 1)
	{
		[RenderCommandEncoder setScissorRect:Rect[0]];
	}
	else
	{
		check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports));
		[((id<IMetalRenderCommandEncoder>)RenderCommandEncoder) setScissorRects:Rect count:NumActive];
	}
}

void FMetalCommandEncoder::SetTriangleFillMode(MTLTriangleFillMode const InFillMode)
{
    check(RenderCommandEncoder);
	{
		[RenderCommandEncoder setTriangleFillMode:InFillMode];
	}
}

void FMetalCommandEncoder::SetBlendColor(float const Red, float const Green, float const Blue, float const Alpha)
{
	check(RenderCommandEncoder);
	{
		[RenderCommandEncoder setBlendColorRed:Red green:Green blue:Blue alpha:Alpha];
	}
}

void FMetalCommandEncoder::SetDepthStencilState(id<MTLDepthStencilState> const InDepthStencilState)
{
    check (RenderCommandEncoder);
	{
		[RenderCommandEncoder setDepthStencilState:InDepthStencilState];
	}
}

void FMetalCommandEncoder::SetStencilReferenceValue(uint32 const ReferenceValue)
{
    check (RenderCommandEncoder);
	{
		[RenderCommandEncoder setStencilReferenceValue:ReferenceValue];
	}
}

void FMetalCommandEncoder::SetVisibilityResultMode(MTLVisibilityResultMode const Mode, NSUInteger const Offset)
{
    check (RenderCommandEncoder);
	{
		check(Mode == MTLVisibilityResultModeDisabled || RenderPassDesc.visibilityResultBuffer);
		[RenderCommandEncoder setVisibilityResultMode: Mode offset:Offset];
	}
}
	
#pragma mark - Public Shader Resource Mutators -

void FMetalCommandEncoder::SetShaderBuffer(MTLFunctionType const FunctionType, id<MTLBuffer> Buffer, NSUInteger const Offset, NSUInteger const Length, NSUInteger index, EPixelFormat const Format)
{
	check(index < ML_MaxBuffers);
    if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSetBufferOffset) && Buffer && (ShaderBuffers[FunctionType].Bound & (1 << index)) && ShaderBuffers[FunctionType].Buffers[index] == Buffer)
    {
		SetShaderBufferOffset(FunctionType, Offset, Length, index);
		ShaderBuffers[FunctionType].Lengths[index+ML_MaxBuffers] = Format;
    }
    else
    {
		if(Buffer)
		{
			ShaderBuffers[FunctionType].Bound |= (1 << index);
		}
		else
		{
			ShaderBuffers[FunctionType].Bound &= ~(1 << index);
		}
		ShaderBuffers[FunctionType].Buffers[index] = Buffer;
		ShaderBuffers[FunctionType].Bytes[index] = nil;
		ShaderBuffers[FunctionType].Offsets[index] = Offset;
		ShaderBuffers[FunctionType].Lengths[index] = Length;
		ShaderBuffers[FunctionType].Lengths[index+ML_MaxBuffers] = Format;
		
		SetShaderBufferInternal(FunctionType, index);
    }
}

void FMetalCommandEncoder::SetShaderData(MTLFunctionType const FunctionType, FMetalBufferData* Data, NSUInteger const Offset, NSUInteger const Index)
{
	check(Index < ML_MaxBuffers);
	
#if METAL_DEBUG_OPTIONS
	if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() > EMetalDebugLevelResetOnBind)
	{
		SetShaderBuffer(FunctionType, nil, 0, 0, Index);
	}
#endif
	
	if(Data)
	{
		ShaderBuffers[FunctionType].Bound |= (1 << Index);
	}
	else
	{
		ShaderBuffers[FunctionType].Bound &= ~(1 << Index);
	}
	
	ShaderBuffers[FunctionType].Buffers[Index] = nil;
	ShaderBuffers[FunctionType].Bytes[Index] = Data;
	ShaderBuffers[FunctionType].Offsets[Index] = Offset;
	ShaderBuffers[FunctionType].Lengths[Index] = Data ? (Data->Len - Offset) : 0;
	
	SetShaderBufferInternal(FunctionType, Index);
}

void FMetalCommandEncoder::SetShaderBytes(MTLFunctionType const FunctionType, uint8 const* Bytes, NSUInteger const Length, NSUInteger const Index)
{
	check(Index < ML_MaxBuffers);
	
#if METAL_DEBUG_OPTIONS
	if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() > EMetalDebugLevelResetOnBind)
	{
		SetShaderBuffer(FunctionType, nil, 0, 0, Index);
	}
#endif
	
	if(Bytes && Length)
	{
		ShaderBuffers[FunctionType].Bound |= (1 << Index);

		uint32 Offset = RingBuffer.Allocate(Length, BufferOffsetAlignment);
		id<MTLBuffer> Buffer = RingBuffer.Buffer->Buffer;
		
		ShaderBuffers[FunctionType].Buffers[Index] = Buffer;
		ShaderBuffers[FunctionType].Bytes[Index] = nil;
		ShaderBuffers[FunctionType].Offsets[Index] = Offset;
		ShaderBuffers[FunctionType].Lengths[Index] = Length;
		
		FMemory::Memcpy(((uint8*)[Buffer contents]) + Offset, Bytes, Length);
	}
	else
	{
		ShaderBuffers[FunctionType].Bound &= ~(1 << Index);
		
		ShaderBuffers[FunctionType].Buffers[Index] = nil;
		ShaderBuffers[FunctionType].Bytes[Index] = nil;
		ShaderBuffers[FunctionType].Offsets[Index] = 0;
		ShaderBuffers[FunctionType].Lengths[Index] = 0;
	}
	
	SetShaderBufferInternal(FunctionType, Index);
}

void FMetalCommandEncoder::SetShaderBufferOffset(MTLFunctionType FunctionType, NSUInteger const Offset, NSUInteger const Length, NSUInteger const index)
{
	check(index < ML_MaxBuffers);
    checkf(ShaderBuffers[FunctionType].Buffers[index] && (ShaderBuffers[FunctionType].Bound & (1 << index)), TEXT("Buffer must already be bound"));
	check(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSetBufferOffset));
	ShaderBuffers[FunctionType].Offsets[index] = Offset;
	ShaderBuffers[FunctionType].Lengths[index] = Length;
	switch (FunctionType)
	{
		case MTLFunctionTypeVertex:
			check (RenderCommandEncoder);
			[RenderCommandEncoder setVertexBufferOffset:Offset atIndex:index];
			break;
		case MTLFunctionTypeFragment:
			check(RenderCommandEncoder);
			[RenderCommandEncoder setFragmentBufferOffset:Offset atIndex:index];
			break;
		case MTLFunctionTypeKernel:
			check (ComputeCommandEncoder);
			[ComputeCommandEncoder setBufferOffset:Offset atIndex:index];
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderTexture(MTLFunctionType FunctionType, id<MTLTexture> Texture, NSUInteger index)
{
	check(index < ML_MaxTextures);
	switch (FunctionType)
	{
		case MTLFunctionTypeVertex:
			check (RenderCommandEncoder);
			[RenderCommandEncoder setVertexTexture:Texture atIndex:index];
			break;
		case MTLFunctionTypeFragment:
			check(RenderCommandEncoder);
			[RenderCommandEncoder setFragmentTexture:Texture atIndex:index];
			break;
		case MTLFunctionTypeKernel:
			check (ComputeCommandEncoder);
			[ComputeCommandEncoder setTexture:Texture atIndex:index];
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderSamplerState(MTLFunctionType FunctionType, id<MTLSamplerState> Sampler, NSUInteger index)
{
	check(index < ML_MaxSamplers);
	switch (FunctionType)
	{
		case MTLFunctionTypeVertex:
       		check (RenderCommandEncoder);
			[RenderCommandEncoder setVertexSamplerState:Sampler atIndex:index];
			break;
		case MTLFunctionTypeFragment:
			check (RenderCommandEncoder);
			[RenderCommandEncoder setFragmentSamplerState:Sampler atIndex:index];
			break;
		case MTLFunctionTypeKernel:
			check (ComputeCommandEncoder);
			[ComputeCommandEncoder setSamplerState:Sampler atIndex:index];
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderSideTable(MTLFunctionType const FunctionType, NSUInteger const Index)
{
	if (Index < ML_MaxBuffers)
	{
		SetShaderBytes(FunctionType, (uint8*)ShaderBuffers[FunctionType].Lengths, sizeof(ShaderBuffers[FunctionType].Lengths), Index);
	}
}

#pragma mark - Public Compute State Mutators -

void FMetalCommandEncoder::SetComputePipelineState(FMetalShaderPipeline* State)
{
	check (ComputeCommandEncoder);
	{
        METAL_SET_COMPUTE_REFLECTION(ComputeCommandEncoder, State);
		[ComputeCommandEncoder setComputePipelineState:State.ComputePipelineState];
	}
}

#pragma mark - Public Ring-Buffer Accessor -
	
FRingBuffer& FMetalCommandEncoder::GetRingBuffer(void)
{
	return RingBuffer;
}

#pragma mark - Private Functions -

void FMetalCommandEncoder::SetShaderBufferInternal(MTLFunctionType Function, uint32 Index)
{
	id<MTLBuffer> Buffer = ShaderBuffers[Function].Buffers[Index];
	NSUInteger Offset = ShaderBuffers[Function].Offsets[Index];
	bool bBufferHasBytes = ShaderBuffers[Function].Bytes[Index] != nil;
	if (!Buffer && bBufferHasBytes && !bSupportsMetalFeaturesSetBytes)
	{
		uint8 const* Bytes = (((uint8 const*)ShaderBuffers[Function].Bytes[Index]->Data) + ShaderBuffers[Function].Offsets[Index]);
		uint32 Len = ShaderBuffers[Function].Bytes[Index]->Len - ShaderBuffers[Function].Offsets[Index];
		
		Offset = RingBuffer.Allocate(Len, BufferOffsetAlignment);
		Buffer = RingBuffer.Buffer->Buffer;
		
		FMemory::Memcpy(((uint8*)[Buffer contents]) + Offset, Bytes, Len);
	}
	
	if (Buffer != nil)
	{
		switch (Function)
		{
			case MTLFunctionTypeVertex:
				ShaderBuffers[Function].Bound |= (1 << Index);
				check(RenderCommandEncoder);
				[RenderCommandEncoder setVertexBuffer:Buffer offset:Offset atIndex:Index];
				break;
			case MTLFunctionTypeFragment:
				ShaderBuffers[Function].Bound |= (1 << Index);
				check(RenderCommandEncoder);
				[RenderCommandEncoder setFragmentBuffer:Buffer offset:Offset atIndex:Index];
				break;
			case MTLFunctionTypeKernel:
				ShaderBuffers[Function].Bound |= (1 << Index);
				check(ComputeCommandEncoder);
				[ComputeCommandEncoder setBuffer:Buffer offset:Offset atIndex:Index];
				break;
			default:
				check(false);
				break;
		}
	}
	else if (bBufferHasBytes && bSupportsMetalFeaturesSetBytes)
	{
		uint8 const* Bytes = (((uint8 const*)ShaderBuffers[Function].Bytes[Index]->Data) + ShaderBuffers[Function].Offsets[Index]);
		uint32 Len = ShaderBuffers[Function].Bytes[Index]->Len - ShaderBuffers[Function].Offsets[Index];
		
		switch (Function)
		{
			case MTLFunctionTypeVertex:
				ShaderBuffers[Function].Bound |= (1 << Index);
				check(RenderCommandEncoder);
				[RenderCommandEncoder setVertexBytes:Bytes length:Len atIndex:Index];
				break;
			case MTLFunctionTypeFragment:
				ShaderBuffers[Function].Bound |= (1 << Index);
				check(RenderCommandEncoder);
				[RenderCommandEncoder setFragmentBytes:Bytes length:Len atIndex:Index];
				break;
			case MTLFunctionTypeKernel:
				ShaderBuffers[Function].Bound |= (1 << Index);
				check(ComputeCommandEncoder);
				[ComputeCommandEncoder setBytes:Bytes length:Len atIndex:Index];
				break;
			default:
				check(false);
				break;
		}
	}
}
