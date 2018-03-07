// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>

class FMetalCommandList;

/**
 * Enumeration of features which are present only on some OS/device combinations.
 * These have to be checked at runtime as well as compile time to ensure backward compatibility.
 */
enum EMetalFeatures
{
	/** Support for separate front & back stencil ref. values */
	EMetalFeaturesSeparateStencil = 1 << 0,
	/** Support for specifying an update to the buffer offset only */
	EMetalFeaturesSetBufferOffset = 1 << 1,
	/** Support for specifying the depth clip mode */
	EMetalFeaturesDepthClipMode = 1 << 2,
	/** Support for specifying resource usage & memory options */
	EMetalFeaturesResourceOptions = 1 << 3,
	/** Supports texture->buffer blit options for depth/stencil blitting */
	EMetalFeaturesDepthStencilBlitOptions = 1 << 4,
    /** Supports creating a native stencil texture view from a depth/stencil texture */
    EMetalFeaturesStencilView = 1 << 5,
    /** Supports a depth-16 pixel format */
    EMetalFeaturesDepth16 = 1 << 6,
	/** Supports NSUInteger counting visibility queries */
	EMetalFeaturesCountingQueries = 1 << 7,
	/** Supports base vertex/instance for draw calls */
	EMetalFeaturesBaseVertexInstance = 1 << 8,
	/** Supports indirect buffers for draw calls */
	EMetalFeaturesIndirectBuffer = 1 << 9,
	/** Supports layered rendering */
	EMetalFeaturesLayeredRendering = 1 << 10,
	/** Support for specifying small buffers as byte arrays */
	EMetalFeaturesSetBytes = 1 << 11,
	/** Supports different shader standard versions */
	EMetalFeaturesShaderVersions = 1 << 12,
	/** Supports tessellation rendering */
	EMetalFeaturesTessellation = 1 << 13,
	/** Supports arbitrary buffer/texture writes from graphics shaders */
	EMetalFeaturesGraphicsUAVs = 1 << 14,
	/** Supports framework-level validation */
	EMetalFeaturesValidation = 1 << 15,
	/** Supports absolute-time emulation using command-buffer completion handlers */
	EMetalFeaturesAbsoluteTimeQueries = 1 << 16,
	/** Supports detailed statistics */
	EMetalFeaturesStatistics= 1 << 17,
	/** Supports memory-less texture resources */
	EMetalFeaturesMemoryLessResources = 1 << 18,
	/** Supports the explicit MTLHeap APIs */
	EMetalFeaturesHeaps = 1 << 19,
	/** Supports the explicit MTLFence APIs */
	EMetalFeaturesFences = 1 << 20,
	/** Supports deferred store action speficication */
	EMetalFeaturesDeferredStoreActions = 1 << 21,
	/** Supports MSAA Depth Resolves */
	EMetalFeaturesMSAADepthResolve = 1 << 22,
	/** Supports Store & Resolve in a single store action */
	EMetalFeaturesMSAAStoreAndResolve = 1 << 23,
	/** Supports framework GPU frame capture */
	EMetalFeaturesGPUTrace = 1 << 24,
	/** Supports combined depth-stencil formats */
	EMetalFeaturesCombinedDepthStencil = 1 << 25,
	/** Supports the use of cubemap arrays */
	EMetalFeaturesCubemapArrays = 1 << 26,
	/** Supports the creation of texture-views using buffers as the backing store */
	EMetalFeaturesLinearTextures = 1 << 27,
	/** Supports the creation of texture-views for UAVs using buffers as the backing store */
	EMetalFeaturesLinearTextureUAVs = 1 << 28,
	/** Supports the specification of multiple viewports and scissor rects */
	EMetalFeaturesMultipleViewports = 1 << 29,
	/** Supports accurate GPU times for commandbuffer start/end */
    EMetalFeaturesGPUCommandBufferTimes = 1 << 30,
    /** Supports minimum on-glass duration for drawables */
    EMetalFeaturesPresentMinDuration = 1 << 31,
    /** Supports programmatic frame capture API */
    EMetalFeaturesGPUCaptureManager = 1 << 32,
	/** Supports toggling V-Sync on & off */
	EMetalFeaturesSupportsVSyncToggle = 1 << 33,
};

/**
 * FMetalCommandQueue:
 */
class FMetalCommandQueue
{
public:
#pragma mark - Public C++ Boilerplate -

	/**
	 * Constructor
	 * @param Device The Metal device to create on.
	 * @param MaxNumCommandBuffers The maximum number of incomplete command-buffers, defaults to 0 which implies the system default.
	 */
	FMetalCommandQueue(id<MTLDevice> Device, uint32 const MaxNumCommandBuffers = 0);
	
	/** Destructor */
	~FMetalCommandQueue(void);
	
#pragma mark - Public Command Buffer Mutators -

	/**
	 * Start encoding to CommandBuffer. It is an error to call this with any outstanding command encoders or current command buffer.
	 * Instead call EndEncoding & CommitCommandBuffer before calling this.
	 * @param CommandBuffer The new command buffer to begin encoding to.
	 */
	id<MTLCommandBuffer> CreateCommandBuffer(void);
	
	/**
	 * Commit the supplied command buffer immediately.
	 * @param CommandBuffer The command buffer to commit, must be non-nil.
 	 */
	void CommitCommandBuffer(id<MTLCommandBuffer> const CommandBuffer);
	
	/**
	 * Deferred contexts submit their internal lists of command-buffers out of order, the command-queue takes ownership and handles reordering them & lazily commits them once all command-buffer lists are submitted.
	 * @param BufferList The list of buffers to enqueue into the command-queue at the given index.
	 * @param Index The 0-based index to commit BufferList's contents into relative to other active deferred contexts.
	 * @param Count The total number of deferred contexts that will submit - only once all are submitted can any command-buffer be committed.
	 */
	void SubmitCommandBuffers(NSArray<id<MTLCommandBuffer>>* BufferList, uint32 Index, uint32 Count);

	/** @returns Creates a new MTLFence or nil if this is unsupported */
	id<MTLFence> CreateFence(NSString* Label) const;
	
#pragma mark - Public Command Queue Accessors -
	
	/** @returns The command queue's native device. */
	id<MTLDevice> GetDevice(void) const;
	
	/** Converts a Metal v1.1+ resource option to something valid on the current version. */
	NSUInteger GetCompatibleResourceOptions(MTLResourceOptions Options) const;
	
	/**
	 * @param InFeature A specific Metal feature to check for.
	 * @returns True if the requested feature is supported, else false.
	 */
	static inline bool SupportsFeature(EMetalFeatures InFeature) { return ((Features & InFeature) != 0); }

	/**
	* @param InFeature A specific Metal feature to check for.
	* @returns True if RHISupportsSeparateMSAAAndResolveTextures will be true.  
	* Currently Mac only.
	*/
	static inline bool SupportsSeparateMSAAAndResolveTarget() { return PLATFORM_MAC != 0; }

#pragma mark - Public Debug Support -

	/** Inserts a boundary that marks the end of a frame for the debug capture tool. */
	void InsertDebugCaptureBoundary(void);
	
	/** Enable or disable runtime debugging features. */
	void SetRuntimeDebuggingLevel(int32 const Level);
	
	/** @returns The level of runtime debugging features enabled. */
	int32 GetRuntimeDebuggingLevel(void) const;

#if METAL_STATISTICS
#pragma mark - Public Statistics Extensions -

	/** @returns An object that provides Metal statistics information or nullptr. */
	class IMetalStatistics* GetStatistics(void);
#endif
	
private:
#pragma mark - Private Member Variables -
	id<MTLCommandQueue> CommandQueue;
#if METAL_STATISTICS
	class IMetalStatistics* Statistics;
#endif
	TArray<NSArray<id<MTLCommandBuffer>>*> CommandBuffers;
	int32 RuntimeDebuggingLevel;
	NSUInteger PermittedOptions;
	static uint64 Features;
};
