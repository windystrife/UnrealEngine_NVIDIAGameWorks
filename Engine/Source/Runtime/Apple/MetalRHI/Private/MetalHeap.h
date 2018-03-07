// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>
#include "MetalFence.h"

#pragma clang diagnostic ignored "-Wnullability-completeness"

#if !METAL_SUPPORTS_HEAPS

@class MTLHeapDescriptor;

#define MTLHeap IMTLHeap

@protocol IMTLHeap <NSObject>
/*!
 @property label
 @abstract A string to help identify this heap. */
@property (nullable, copy, atomic) NSString *label;
/*!
 @property device
 @abstract The device this heap was created against. This heap can only be used with this device. */
@property (readonly) id<MTLDevice> device;
/*!
 @property storageMode
 @abstract Current heap storage mode, default is MTLStorageModePrivate. @discussion All resources created from this heap share the same storage mode. */
@property (readonly) MTLStorageMode storageMode;
/*!
 @property cpuCacheMode
 @abstract CPU cache mode for the heap. Default is MTLCPUCacheModeDefaultCache. @discussion All resources created from this heap share the same cache mode.
 */
@property (readonly) MTLCPUCacheMode cpuCacheMode;
/*!
 @property size
 @abstract The total size of the heap specificed at creation, in bytes. */
@property (readonly) NSUInteger size;
/*!
 @property usedSize
 @abstract The size of all resources allocated from the heap, in bytes. */
@property (readonly) NSUInteger usedSize;
/*!
 @method maxAvailableSizeWithAlignment:
 @abstract The maximum size that can be successfully allocated from the heap in bytes, taking into
 notice given alignment. Alignment needs to be zero, or power of two. @discussion Provides a measure of fragmentation within the heap.
 */
- (NSUInteger)maxAvailableSizeWithAlignment:(NSUInteger)alignment;
/*!
 @abstract Create a new buffer backed by heap memory.
 @discussion The requested storage and CPU cache modes must match the storage and CPU cache modes of
 the heap.
 @return The buffer or nil if heap is full.
 */
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options;
/*!
 @method newTextureWithDescriptor:
 @abstract Create a new texture backed by heap memory.
 @discussion The requested storage and CPU cache modes must match the storage and CPU cache modes of
 the heap, with the exception that the requested storage mode can be MTLStorageModeMemoryless. @return The texture or nil if heap is full.
 */
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)desc;
/*!
 @method setPurgeabilityState:
 @abstract Set or query the purgeability state of the heap. */
- (MTLPurgeableState)setPurgeableState:(MTLPurgeableState)state;
@end

#define MTLHeapDescriptorRef NSObject*

#else

#define MTLHeapDescriptorRef MTLHeapDescriptor*

#endif

/**
 
The internal buffer pool should probably be improved - the tracking currently is based on frames,
but it may be less wasteful to track based on fences. The counter argument is that may introduce more dependencies 
that could be eliminated if the buffers aren't reused as frequently. Will have to consider the optimal strategy.
 
 */

@protocol TMTLHeapDescriptor <NSObject>
@property (readwrite, nonatomic) NSUInteger size;
@property (readwrite, nonatomic) MTLStorageMode storageMode;
@property (readwrite, nonatomic) MTLCPUCacheMode cpuCacheMode;
@end

struct FMTLHeapDescriptor
{
	NSUInteger Size;
	MTLStorageMode StorageMode;
	MTLCPUCacheMode CPUCacheMode;
	id<MTLDevice> Device;
};

enum
{
#if PLATFORM_MAC
	NumHeapBucketSizes = 50, /** Number of pool bucket sizes */
#else
	NumHeapBucketSizes = 46, /** Number of pool bucket sizes */
#endif
};

struct FMetalTextureDesc
{
	friend uint32 GetTypeHash(FMetalTextureDesc const& Other)
	{
		return (((Other.textureType << 28) | (Other.pixelFormat << 16) | Other.usage)
		| (Other.width * Other.height * Other.depth)
		| (Other.mipmapLevelCount * Other.sampleCount * Other.arrayLength)
		| Other.resourceOptions);
	}
	
	bool operator==(FMetalTextureDesc const& Other) const
	{
		if (this != &Other)
		{
			return (textureType == Other.textureType &&
			pixelFormat == Other.pixelFormat &&
			width == Other.width &&
			height == Other.height &&
			depth == Other.depth &&
			mipmapLevelCount == Other.mipmapLevelCount &&
			sampleCount == Other.sampleCount &&
			arrayLength == Other.arrayLength &&
			resourceOptions == Other.resourceOptions &&
			cpuCacheMode == Other.cpuCacheMode &&
			storageMode == Other.storageMode &&
			usage == Other.usage);
		}
		return true;
	}
	
	MTLTextureType textureType;
	
	MTLPixelFormat pixelFormat;
	
	NSUInteger width;
	
	NSUInteger height;
	
	NSUInteger depth;
	
	NSUInteger mipmapLevelCount;
	
	NSUInteger sampleCount;
	
	NSUInteger arrayLength;
	
	MTLResourceOptions resourceOptions;
	
	MTLCPUCacheMode cpuCacheMode;
	
	MTLStorageMode storageMode;
	
	MTLTextureUsage usage;
};

@interface FMTLHeap : FApplePlatformObject <MTLHeap>
{
	MTLPurgeableState PurgableState;
	FCriticalSection* PoolMutex;
	NSMutableSet<id<MTLResource>>* Resources;
	NSMutableArray<id<MTLBuffer>>* BufferBuckets[NumHeapBucketSizes];
	TMap<FMetalTextureDesc, NSMutableSet<id<MTLTexture>>*> TextureCache;
};
@property (readonly) NSUInteger poolSize;
- (id)initWithDescriptor:(FMTLHeapDescriptor*)descriptor;
- (void)aliasBuffer:(id<MTLBuffer>)buffer;
- (void)aliasTexture:(id<MTLTexture>)texture;
- (void)drain:(bool)bForce;
@end

@protocol TMTLResource <MTLResource>
-(id<MTLHeap>) heap:(BOOL)bSupportsHeaps;
-(void) makeAliasable:(BOOL)bSupportsHeaps;
-(BOOL) isAliasable:(BOOL)bSupportsHeaps;
@end

@protocol TMTLBuffer <MTLBuffer>
-(id<MTLHeap>) heap:(BOOL)bSupportsHeaps;
-(void) makeAliasable:(BOOL)bSupportsHeaps;
-(BOOL) isAliasable:(BOOL)bSupportsHeaps;
@end

@protocol TMTLTexture <MTLTexture>
-(id<MTLHeap>) heap:(BOOL)bSupportsHeaps;
-(void) makeAliasable:(BOOL)bSupportsHeaps;
-(BOOL) isAliasable:(BOOL)bSupportsHeaps;
@end

@protocol TMTLDevice <MTLDevice>
- (MTLSizeAndAlign)heapTextureSizeAndAlignWithDescriptor:(MTLTextureDescriptor *)desc;
- (MTLSizeAndAlign)heapBufferSizeAndAlignWithLength:(NSUInteger)length options:
(MTLResourceOptions)options;
- (id <MTLHeap>)newHeapWithDescriptor:(MTLHeapDescriptorRef)descriptor;
- (id <MTLFence>)newFence;
@end

class FMetalHeap
{
public:
	enum EMetalHeapConstants
	{
		EMetalHeapConstantsCullAfterFrames = 30
	};
	
	enum EMetalHeapStorage
	{
		/** CPU memory available to the GPU - typically used for reading back from the GPU. */
		EMetalHeapStorageCPUCached = 0,
		EMetalHeapStorageCPUWriteCombine = 1,
		/** Memory used to feed data to the GPU dynamically - underlying type depends on GPU. */
		EMetalHeapStorageDMACached = 2,
		EMetalHeapStorageDMAWriteCombine = 3,
		/** GPU memory exclusive memory - only really useful on discrete GPUs. */
		EMetalHeapStorageGPUCached = 4,
		EMetalHeapStorageGPUWriteCombine = 5,
		/** Number of storage types */
		EMetalHeapStorageNum = 6,
		EMetalHeapCacheNum = 2,
		EMetalHeapTypeNum = 3
	};
	
	enum EMetalHeapTextureUsage
	{
		/** Regular texture resource */
		EMetalHeapTextureUsageResource = 0,
		/** Render target or UAV that can be aliased */
		EMetalHeapTextureUsageRenderTarget = 1,
		/** Number of texture usage types */
		EMetalHeapTextureUsageNum = 2
	};
	
	enum EMetalHeapBufferSizes
	{
		/** Max. block size of 16k */
		/** Heap size of 1Mb */
		EMetalHeapBufferSizes16k,
		
		/** Max. block size of 64k */
		/** Heap size of 2Mb */
		EMetalHeapBufferSizes64k,
		
		/** Max. block size of 256k */
		/** Heap size of 2Mb */
		EMetalHeapBufferSizes256k,
		
		/** Max. block size of 1Mb */
		/** Heap size of 4Mb */
		EMetalHeapBufferSizes1Mb,

		/** Max. block size of 4Mb */
		/** Heap size of 12Mb */
		EMetalHeapBufferSizes4Mb,
		
		/** Max. block size of 16Mb */
		/** Heap size of 32Mb */
		EMetalHeapBufferSizes16Mb,
		
		/** Max. block size of 64Mb */
		/** Heap size of 128Mb */
		EMetalHeapBufferSizes64Mb,
		
#if PLATFORM_MAC
		/** Max. block size of 128Mb */
		/** Heap size of 128Mb */
		EMetalHeapBufferSizes128Mb,
#endif

		/** The start bucket for heaps that are sized to the allocation, rather than bucketed for sharing */
		EMetalHeapBufferExactSize,
		/** Sizes greater than this aren't allocated in bucketed heaps as the size means trying to alias leads to wasteful memory usage */
		EMetalHeapBufferSizesNum,

		/** To avoid allocating more heaps look through the larger heap sizes (up to EMetalHeapBufferLookAhead) for existing an existing heap. */
		EMetalHeapBufferLookAhead = 1,
		
		/** Only heaps at or beneath this index may perform look ahead */
		EMetalHeapBufferAllowLookAhead = EMetalHeapBufferSizes64Mb,
	};
	
public:
	FMetalHeap(void);
	~FMetalHeap();
	
	void Init(FMetalCommandQueue& Queue);
	
	id<MTLHeap> CreateHeap(FMTLHeapDescriptor& Desc);
	
	id<MTLBuffer> CreateBuffer(uint32 Size, MTLResourceOptions Options);
	id<MTLTexture> CreateTexture(MTLTextureDescriptor* Desc, FMetalSurface* Surface);
	
	void ReleaseHeap(id<MTLHeap> Heap);
	void ReleaseTexture(FMetalSurface* Surface, id<MTLTexture> Texture);
	
	void Compact(FMetalDeviceContext& Context, bool const bForce);

	static EMetalHeapStorage ResouceOptionsToStorage(MTLResourceOptions Options);
	static EMetalHeapBufferSizes BufferSizeToIndex(uint32 Size);
	static EMetalHeapTextureUsage TextureDescToIndex(MTLTextureDescriptor* Desc);
	
private:
	void Defrag(FMetalDeviceContext& Context, bool const bForce);
	void Drain(FMetalDeviceContext& Context, bool const bForce);
	id<MTLTexture> CreateTexture(id<MTLHeap> Heap, MTLTextureDescriptor* Desc, FMetalSurface* Surface);
	id<MTLHeap> FindDefragHeap(EMetalHeapStorage Storage, EMetalHeapTextureUsage Usage, MTLTextureDescriptor* Desc, id<MTLHeap> CurrentHeap);
	
private:
	static uint32 HeapBufferSizes[EMetalHeapBufferSizesNum];
	static uint32 HeapBufferBlock[EMetalHeapBufferSizesNum];
	
	FCriticalSection Mutex;
	
	FMetalCommandQueue* Queue;
	
	uint64 TotalTextureMemory;
	
	TArray<TPair<id<MTLHeap>, uint64>> BufferHeaps[EMetalHeapStorageNum][EMetalHeapBufferSizesNum];
	TArray<TPair<id<MTLHeap>, uint64>> DynamicTextureHeaps[EMetalHeapStorageNum][EMetalHeapBufferSizesNum][EMetalHeapTextureUsageNum];
	id<MTLHeap> StaticTextureHeaps[EMetalHeapStorageNum][EMetalHeapTextureUsageNum];
	
	TMap<id<MTLHeap>, TSet<NSObject<MTLTexture>*>> TextureResources;
	TSet<id<MTLHeap>> ReleasedHeaps;
};
