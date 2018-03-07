// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandQueue.cpp: Metal command queue wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandQueue.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandList.h"
#include "MetalProfiler.h"
#if METAL_STATISTICS
#include "MetalStatistics.h"
#include "ModuleManager.h"
#endif
#include "Misc/ConfigCacheIni.h"

#pragma mark - Private C++ Constants
enum EMetalFeatureSet
{
    // Workaround compile errors when using an older OS
    EMetalFeatureSetMacOSv3 = 10003,
};

#pragma mark - Private C++ Statics -
uint64 FMetalCommandQueue::Features = 0;

#pragma mark - Public C++ Boilerplate -

FMetalCommandQueue::FMetalCommandQueue(id<MTLDevice> Device, uint32 const MaxNumCommandBuffers /* = 0 */)
: CommandQueue(nil)
#if METAL_STATISTICS
, Statistics(nullptr)
#endif
, RuntimeDebuggingLevel(EMetalDebugLevelOff)
{
	if(MaxNumCommandBuffers == 0)
	{
		CommandQueue = [Device newCommandQueue];
	}
	else
	{
		CommandQueue = [Device newCommandQueueWithMaxCommandBufferCount: MaxNumCommandBuffers];
	}
	check(CommandQueue);
	bool bNoMetalv2 = FParse::Param(FCommandLine::Get(), TEXT("nometalv2"));
#if PLATFORM_IOS
	NSOperatingSystemVersion Vers = [[NSProcessInfo processInfo] operatingSystemVersion];
	if(Vers.majorVersion >= 9)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset | EMetalFeaturesResourceOptions | EMetalFeaturesDepthStencilBlitOptions | EMetalFeaturesShaderVersions | EMetalFeaturesSetBytes;

#if PLATFORM_TVOS
		if(!bNoMetalv2 && [Device supportsFeatureSet:MTLFeatureSet_tvOS_GPUFamily1_v2])
		{
			Features |= EMetalFeaturesStencilView | EMetalFeaturesGraphicsUAVs;
		}
#else
		if ([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1])
		{
			Features |= EMetalFeaturesCountingQueries | EMetalFeaturesBaseVertexInstance | EMetalFeaturesIndirectBuffer | EMetalFeaturesMSAADepthResolve;
		}
		
		if(!bNoMetalv2 && ([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v3] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v3]))
		{
			Features |= EMetalFeaturesStencilView | EMetalFeaturesGraphicsUAVs | EMetalFeaturesMemoryLessResources /*| EMetalFeaturesHeaps | EMetalFeaturesFences*/;
		}
		
		if(!bNoMetalv2 && [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2])
		{
			Features |= EMetalFeaturesTessellation | EMetalFeaturesMSAAStoreAndResolve;
		}
		
		if(Vers.majorVersion >= 10 && Vers.minorVersion >= 3)
        {
            Features |= EMetalFeaturesGPUCommandBufferTimes;
			
			if(!bNoMetalv2 && ([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v3] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v3]))
			{
				Features |= EMetalFeaturesDeferredStoreActions | EMetalFeaturesCombinedDepthStencil;
			}
        }
		
		if(Vers.majorVersion >= 11)
		{
			Features |= EMetalFeaturesPresentMinDuration | EMetalFeaturesGPUCaptureManager;
        }
#endif
	}
	else if(Vers.majorVersion == 8 && Vers.minorVersion >= 3)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset;
	}
#else // Assume that Mac & other platforms all support these from the start. They can diverge later.
	Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset | EMetalFeaturesDepthClipMode | EMetalFeaturesResourceOptions | EMetalFeaturesDepthStencilBlitOptions | EMetalFeaturesCountingQueries | EMetalFeaturesBaseVertexInstance | EMetalFeaturesIndirectBuffer | EMetalFeaturesLayeredRendering | EMetalFeaturesShaderVersions | EMetalFeaturesCombinedDepthStencil | EMetalFeaturesCubemapArrays;
    if (!FParse::Param(FCommandLine::Get(),TEXT("nometalv2")) && [Device supportsFeatureSet:MTLFeatureSet_OSX_GPUFamily1_v2])
    {
        Features |= EMetalFeaturesStencilView | EMetalFeaturesDepth16 | EMetalFeaturesTessellation | EMetalFeaturesGraphicsUAVs | EMetalFeaturesDeferredStoreActions | EMetalFeaturesMSAADepthResolve | EMetalFeaturesMSAAStoreAndResolve;
        
        // Assume that set*Bytes only works on macOS Sierra and above as no-one has tested it anywhere else.
		Features |= EMetalFeaturesSetBytes;
    }
    else if ([Device.name rangeOfString:@"Nvidia" options:NSCaseInsensitiveSearch].location != NSNotFound)
    {
		// Using set*Bytes fixes bugs on Nvidia for 10.11 so we should use it...
    	Features |= EMetalFeaturesSetBytes;
    }
    
    if([Device supportsFeatureSet:(MTLFeatureSet)EMetalFeatureSetMacOSv3])
    {
        Features |= EMetalFeaturesMultipleViewports | EMetalFeaturesGPUCommandBufferTimes | EMetalFeaturesGPUCaptureManager | EMetalFeaturesAbsoluteTimeQueries | EMetalFeaturesSupportsVSyncToggle;
    }
    // Time query emulation breaks on AMD < 10.13 - disable by default until they can explain why, should work everywhere else.
	else if ([Device.name rangeOfString:@"AMD" options:NSCaseInsensitiveSearch].location == NSNotFound || FParse::Param(FCommandLine::Get(),TEXT("metaltimequery")))
	{
		Features |= EMetalFeaturesAbsoluteTimeQueries;
	}
#endif
	
	Class MTLDebugDevice = NSClassFromString(@"MTLDebugDevice");
	if ([Device isKindOfClass:MTLDebugDevice])
	{
		Features |= EMetalFeaturesValidation;
	}
	
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));
	if (CVar->GetInt() == 0 || FParse::Param(FCommandLine::Get(),TEXT("metalshaderdebug")))
	{
		Features |= EMetalFeaturesGPUTrace;
	}
    
    int32 MaxShaderVersion = 0;
#if PLATFORM_MAC
	int32 DefaultMaxShaderVersion = 2;
    const TCHAR* const Settings = TEXT("/Script/MacTargetPlatform.MacTargetSettings");
#else
	int32 DefaultMaxShaderVersion = 0;
    const TCHAR* const Settings = TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings");
#endif
    if(!GConfig->GetInt(Settings, TEXT("MaxShaderLanguageVersion"), MaxShaderVersion, GEngineIni))
    {
        MaxShaderVersion = DefaultMaxShaderVersion;
    }
	
#if METAL_STATISTICS
    IMetalStatisticsModule* StatsModule = FModuleManager::Get().LoadModulePtr<IMetalStatisticsModule>(TEXT("MetalStatistics"));
    
    if(StatsModule && FParse::Param(FCommandLine::Get(),TEXT("metalstats")))
    {
        Statistics = StatsModule->CreateMetalStatistics(CommandQueue);
        if(Statistics->SupportsStatistics())
        {
            Features |= EMetalFeaturesStatistics;
        }
        else
        {
            delete Statistics;
            Statistics = nullptr;
        }
    }
#endif
    
	PermittedOptions = 0;
	PermittedOptions |= MTLResourceCPUCacheModeDefaultCache;
	PermittedOptions |= MTLResourceCPUCacheModeWriteCombined;
	if (Features & EMetalFeaturesResourceOptions)
	{
		PermittedOptions |= MTLResourceStorageModeShared;
		PermittedOptions |= MTLResourceStorageModePrivate;
#if PLATFORM_MAC
		PermittedOptions |= MTLResourceStorageModeManaged;
#else
		if (Features & EMetalFeaturesMemoryLessResources)
		{
			PermittedOptions |= MTLResourceStorageModeMemoryless;
		}
		if (Features & EMetalFeaturesFences)
		{
			PermittedOptions |= MTLResourceHazardTrackingModeUntracked;
		}
#endif
	}
}

FMetalCommandQueue::~FMetalCommandQueue(void)
{
#if METAL_STATISTICS
	delete Statistics;
#endif
	
	[CommandQueue release];
	CommandQueue = nil;
}
	
#pragma mark - Public Command Buffer Mutators -

id<MTLCommandBuffer> FMetalCommandQueue::CreateCommandBuffer(void)
{
	static bool bUnretainedRefs = !FParse::Param(FCommandLine::Get(),TEXT("metalretainrefs"));
	id<MTLCommandBuffer> CmdBuffer = nil;
	@autoreleasepool
	{
		CmdBuffer = bUnretainedRefs ? [[CommandQueue commandBufferWithUnretainedReferences] retain] : [[CommandQueue commandBuffer] retain];
		if (RuntimeDebuggingLevel > EMetalDebugLevelLogDebugGroups)
		{
			CmdBuffer = [[FMetalDebugCommandBuffer alloc] initWithCommandBuffer:CmdBuffer];
		}
		else if (RuntimeDebuggingLevel == EMetalDebugLevelLogDebugGroups)
		{
			((NSObject<MTLCommandBuffer>*)CmdBuffer).debugGroups = [NSMutableArray new];
		}
	}
	INC_DWORD_STAT(STAT_MetalCommandBufferCreatedPerFrame);
	TRACK_OBJECT(STAT_MetalCommandBufferCount, CmdBuffer);
	return CmdBuffer;
}

void FMetalCommandQueue::CommitCommandBuffer(id<MTLCommandBuffer> const CommandBuffer)
{
	check(CommandBuffer);
	UNTRACK_OBJECT(STAT_MetalCommandBufferCount, CommandBuffer);
	
	INC_DWORD_STAT(STAT_MetalCommandBufferCommittedPerFrame);
	
	[CommandBuffer commit];
	
	// Wait for completion when debugging command-buffers.
#if METAL_DEBUG_OPTIONS
	if (RuntimeDebuggingLevel >= EMetalDebugLevelWaitForComplete)
	{
		[CommandBuffer waitUntilCompleted];
	}
#endif
	
	UNTRACK_OBJECT(STAT_MetalCommandBufferCount, CommandBuffer);
	[CommandBuffer release];
}

void FMetalCommandQueue::SubmitCommandBuffers(NSArray<id<MTLCommandBuffer>>* BufferList, uint32 Index, uint32 Count)
{
	check(BufferList);
	CommandBuffers.SetNumZeroed(Count);
	CommandBuffers[Index] = BufferList;
	bool bComplete = true;
	for (uint32 i = 0; i < Count; i++)
	{
		bComplete &= (CommandBuffers[i] != nullptr);
	}
	if (bComplete)
	{
		GetMetalDeviceContext().SubmitCommandsHint();
		
		for (uint32 i = 0; i < Count; i++)
		{
			NSArray<id<MTLCommandBuffer>>* CmdBuffers = CommandBuffers[i];
			check(CmdBuffers);
			for (id<MTLCommandBuffer> Buffer in CmdBuffers)
			{
				check(Buffer);
				CommitCommandBuffer(Buffer);
			}
			[CommandBuffers[i] release];
			CommandBuffers[i] = nullptr;
		}
	}
}

id<MTLFence> FMetalCommandQueue::CreateFence(NSString* Label) const
{
	id<MTLFence> InternalFence = nil;;
	if(Features & EMetalFeaturesFences)
	{
		InternalFence = [(id<MTLDeviceExtensions>)CommandQueue.device newFence];
	}
#if METAL_DEBUG_OPTIONS
	if (RuntimeDebuggingLevel >= EMetalDebugLevelValidation)
	{
		FMetalDebugFence* Fence = [FMetalDebugFence new];
		Fence.Inner = InternalFence;
		[InternalFence release];
		InternalFence = Fence;
	}
#endif
	if(InternalFence && Label)
	{
		InternalFence.label = Label;
	}
	return InternalFence;
}

#pragma mark - Public Command Queue Accessors -
	
id<MTLDevice> FMetalCommandQueue::GetDevice(void) const
{
	return CommandQueue.device;
}

NSUInteger FMetalCommandQueue::GetCompatibleResourceOptions(MTLResourceOptions Options) const
{
	NSUInteger NewOptions = (Options & PermittedOptions);
#if PLATFORM_IOS // Swizzle Managed to Shared for iOS - we can do this as they are equivalent, unlike Shared -> Managed on Mac.
	if ((Features & EMetalFeaturesResourceOptions) && (Options & (1 /*MTLStorageModeManaged*/ << MTLResourceStorageModeShift)))
	{
		NewOptions |= MTLResourceStorageModeShared;
	}
#endif
	return NewOptions;
}

#pragma mark - Public Debug Support -

void FMetalCommandQueue::InsertDebugCaptureBoundary(void)
{
	[CommandQueue insertDebugCaptureBoundary];
}

void FMetalCommandQueue::SetRuntimeDebuggingLevel(int32 const Level)
{
	RuntimeDebuggingLevel = Level;
}

int32 FMetalCommandQueue::GetRuntimeDebuggingLevel(void) const
{
	return RuntimeDebuggingLevel;
}

#if METAL_STATISTICS
#pragma mark - Public Statistics Extensions -

IMetalStatistics* FMetalCommandQueue::GetStatistics(void)
{
	return Statistics;
}
#endif
