// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "ShaderCache.h"

TGlobalResource<TBoundShaderStateHistory<10000>> FMetalRHICommandContext::BoundShaderStateHistory;

FMetalDeviceContext& GetMetalDeviceContext()
{
	FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
	check(Context);
	return ((FMetalDeviceContext&)Context->GetInternalContext());
}

void SafeReleaseMetalObject(id Object)
{
	if(GIsRHIInitialized && GDynamicRHI)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseObject(Object);
			return;
		}
	}
	[Object release];
}

void SafeReleaseMetalResource(id<MTLResource> Object)
{
	if(GIsRHIInitialized && GDynamicRHI)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseResource(Object);
			return;
		}
	}
	[Object release];
}

void SafeReleasePooledBuffer(id<MTLBuffer> Buffer)
{
	if(GIsRHIInitialized && GDynamicRHI)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleasePooledBuffer(Buffer);
		}
	}
}

void SafeReleaseMetalFence(id Object)
{
	if(GIsRHIInitialized && GDynamicRHI && Object)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseFence((id<MTLFence>)Object);
			return;
		}
	}
}

FMetalRHICommandContext::FMetalRHICommandContext(struct FMetalGPUProfiler* InProfiler, FMetalContext* WrapContext)
: Context(WrapContext)
, Profiler(InProfiler)
, PendingVertexBufferOffset(0xFFFFFFFF)
, PendingVertexDataStride(0)
, PendingIndexBufferOffset(0xFFFFFFFF)
, PendingIndexDataStride(0)
, PendingPrimitiveType(0)
, PendingNumPrimitives(0)
{
	check(Context);
	Context->GetCurrentState().SetShaderCacheStateObject(FShaderCache::CreateOrFindCacheStateForContext(this));	
}

FMetalRHICommandContext::~FMetalRHICommandContext()
{
	FShaderCache::RemoveCacheStateForContext(this);
	delete Context;
}

FMetalRHIComputeContext::FMetalRHIComputeContext(struct FMetalGPUProfiler* InProfiler, FMetalContext* WrapContext)
: FMetalRHICommandContext(InProfiler, WrapContext)
{
}

FMetalRHIComputeContext::~FMetalRHIComputeContext()
{
}

void FMetalRHIComputeContext::RHISetAsyncComputeBudget(EAsyncComputeBudget Budget)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false);
	}
	FMetalRHICommandContext::RHISetAsyncComputeBudget(Budget);
}

void FMetalRHIComputeContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false);
	}
	FMetalRHICommandContext::RHISetComputeShader(ComputeShader);
}

void FMetalRHIComputeContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false);
	}
	FMetalRHICommandContext::RHISetComputePipelineState(ComputePipelineState);
}

void FMetalRHIComputeContext::RHISubmitCommandsHint()
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false);
	}
	Context->FinishFrame();
	
#if ENABLE_METAL_GPUPROFILE
	FMetalContext::MakeCurrent(&GetMetalDeviceContext());
#endif
}

FMetalRHIImmediateCommandContext::FMetalRHIImmediateCommandContext(struct FMetalGPUProfiler* InProfiler, FMetalContext* WrapContext)
	: FMetalRHICommandContext(InProfiler, WrapContext)
{
}
