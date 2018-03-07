// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

#include "MetalHeap.h"
#include "MetalContext.h"
#include "MetalProfiler.h"
#include "RenderUtils.h"
#include <objc/runtime.h>
#include "Misc/ConfigCacheIni.h"

DECLARE_STATS_GROUP(TEXT("Metal Heap"),STATGROUP_MetalHeap, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("# Buffer Heaps"), STAT_MetalHeapNumBufferHeaps, STATGROUP_MetalHeap);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("# Texture Heaps"), STAT_MetalHeapNumTextureHeaps, STATGROUP_MetalHeap);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("# Render-Target Heaps"), STAT_MetalHeapNumRenderTargetHeaps, STATGROUP_MetalHeap);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("# Textures Defragged"), STAT_MetalHeapNumTextureReallocs, STATGROUP_MetalHeap);
DECLARE_DWORD_COUNTER_STAT(TEXT("# Textures Defragged / Frame"), STAT_MetalHeapNumFrameTextureReallocs, STATGROUP_MetalHeap);

DECLARE_MEMORY_STAT(TEXT("Total Buffer Memory"),STAT_MetalHeapTotalBuffer,STATGROUP_MetalHeap);
DECLARE_MEMORY_STAT(TEXT("Total Texture Resource Memory"),STAT_MetalHeapTotalTexture,STATGROUP_MetalHeap);
DECLARE_MEMORY_STAT(TEXT("Total RenderTarget Memory"),STAT_MetalHeapTotalRenderTarget,STATGROUP_MetalHeap);

DECLARE_MEMORY_STAT(TEXT("Current Buffer Memory"),STAT_MetalHeapBufferMemory,STATGROUP_MetalHeap)
DECLARE_MEMORY_STAT(TEXT("Current Texture Resource Memory"),STAT_MetalHeapTextureMemory,STATGROUP_MetalHeap)
DECLARE_MEMORY_STAT(TEXT("Current RenderTarget Memory"),STAT_MetalHeapRenderTargetMemory,STATGROUP_MetalHeap);

DECLARE_MEMORY_STAT(TEXT("Peak Buffer Memory"),STAT_MetalHeapBufferPeakMemory,STATGROUP_MetalHeap);
DECLARE_MEMORY_STAT(TEXT("Peak Texture Resource Memory"),STAT_MetalHeapTexturePeakMemory,STATGROUP_MetalHeap);
DECLARE_MEMORY_STAT(TEXT("Peak RenderTarget Memory"),STAT_MetalHeapRenderTargetPeakMemory,STATGROUP_MetalHeap);

DECLARE_FLOAT_COUNTER_STAT(TEXT("Buffer Memory Allocated / Frame"),STAT_MetalHeapBufferAllocMemory,STATGROUP_MetalHeap);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Texture Resource Memory Allocated / Frame"),STAT_MetalHeapTextureAllocMemory,STATGROUP_MetalHeap);
DECLARE_FLOAT_COUNTER_STAT(TEXT("RenderTarget Memory Allocated / Frame"),STAT_MetalHeapRenderTargetAllocMemory,STATGROUP_MetalHeap);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Texture Memory Defragged / Frame"),STAT_MetalHeapTotalTextureReallocMemory,STATGROUP_MetalHeap);

int32 GMetalHeapMemToDefragPerFrame = PLATFORM_MAC ? 10 * 1024 * 1024 : 2 * 1024 * 1024;
static FAutoConsoleVariableRef CVarMetalHeapMemToDefragPerFrame(
	TEXT("rhi.Metal.HeapMemToDefragPerFrame"),
	GMetalHeapMemToDefragPerFrame,
	TEXT("How much MTLHeap memory (in bytes) to defrag each frame in order to reduce wasted space in MTLHeaps. (Mac: 10Mb, iOS/tvOS: 2Mb)"));

float GMetalHeapDefragUnderUtilisedFraction = 0.5f;
static FAutoConsoleVariableRef CVarMetalHeapDefragUnderUtilisedFraction(
	TEXT("rhi.Metal.HeapDefragUnderUtilisedFraction"),
	GMetalHeapDefragUnderUtilisedFraction,
	TEXT("Defines the fraction of a MTLHeap that must be free for that heap to be considered for defragging. (Default: 0.5f)"));

#if STATS
static FName NumTextureHeapStats[FMetalHeap::EMetalHeapTextureUsageNum] = {
	GET_STATFNAME(STAT_MetalHeapNumTextureHeaps), GET_STATFNAME(STAT_MetalHeapNumRenderTargetHeaps)
};
static FName TotalTextureHeapStats[FMetalHeap::EMetalHeapTextureUsageNum] = {
	GET_STATFNAME(STAT_MetalHeapTotalTexture), GET_STATFNAME(STAT_MetalHeapTotalRenderTarget)
};
static FName TextureHeapStats[FMetalHeap::EMetalHeapTextureUsageNum] = {
	GET_STATFNAME(STAT_MetalHeapTextureMemory), GET_STATFNAME(STAT_MetalHeapRenderTargetMemory)
};
static FName PeakTextureHeapStats[FMetalHeap::EMetalHeapTextureUsageNum] = {
	GET_STATFNAME(STAT_MetalHeapTexturePeakMemory), GET_STATFNAME(STAT_MetalHeapRenderTargetPeakMemory)
};
static FName AllocatedTextureHeapStats[FMetalHeap::EMetalHeapTextureUsageNum] = {
	GET_STATFNAME(STAT_MetalHeapTextureAllocMemory), GET_STATFNAME(STAT_MetalHeapRenderTargetAllocMemory)
};
static uint64 PeakTextureMemory[FMetalHeap::EMetalHeapTextureUsageNum] = {
	0, 0
};
static uint64 PeakBufferMemory = 0;
#define INC_MEMORY_STAT_FNAME_BY(Stat, Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(Stat, EStatOperation::Add, int64(Amount));\
}
#else
#define INC_MEMORY_STAT_FNAME_BY(Stat, Amount)
#endif

enum FMetalResouceType
{
	FMetalResouceTypeInvalid = 0,
	FMetalResouceTypeBuffer = 1,
	FMetalResouceTypeTexture = 2
};

/** The bucket sizes */
static uint32 HeapBufferBucketSizes[NumHeapBucketSizes] = {
#if !PLATFORM_MAC // Mac doesn't use MTLBuffers smaller than the page-size (the drivers would round up internally anyway)
	1024, 2048,
#endif
	4096, 8192, 16384, 32768, 32768 + 16384, 65536, 65536 + 16384, 65536 + 32768, 131072, 131072 + 65536,
	262144, 262144 + 131072, 524288, 262144 + 524288, 1048576, 1048576 + 524288, 2097152, 2097152 + 524288, 2097152 + 1048576,
	2097152 + 1048576 + 524288, 4194304, 4194304 + 524288, 4194304 + 1048576, 4194304 + 2097152, 4194304 + 2097152 + 1048576, 8388608, 8388608 + 2097152,
	8388608 + 4194304, 8388608 + 4194304 + 2097152, 16777216, 16777216 + 4194304, 16777216 + 8388608, 16777216 + 8388608 + 4194304,
	33554432, 33554432 + 4194304, 33554432 + 8388608, 33554432 + 16777216, 33554432 + 16777216 + 8388608, 67108864, 67108864 + 16777216,
	67108864 + 33554432, 134217728, 134217728 + 67108864, 268435456
#if PLATFORM_MAC // Mac allows for up-to 1GB but iOS does not, so add a few more sizes
	, 402653184, 536870912, 671088640, 805306368, 939524096, 1073741824
#endif
};

#if !UE_BUILD_SHIPPING
/** The wasted buffer memory per bucket size */
static int32 volatile HeapWastage[NumHeapBucketSizes];
#endif

/** Get the pool bucket index from the size
 * @param InputSize the number of bytes for the resource
 * @returns The bucket index.
 */
static uint32 GetHeapBucketIndex(uint32 InputSize)
{
	uint32 Size = InputSize;
	
	unsigned long Lower = 0;
	unsigned long Upper = NumHeapBucketSizes;
	unsigned long Middle;
	
	do
	{
		Middle = ( Upper + Lower ) >> 1;
		if( Size <= HeapBufferBucketSizes[Middle-1] )
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while( Upper - Lower > 1 );
	
	check( Size <= HeapBufferBucketSizes[Lower] );
	check( (Lower == 0 ) || ( Size > HeapBufferBucketSizes[Lower-1] ) );
	
	return Lower;
}

/** Get the pool bucket size from the index
 * @param Bucket the bucket index
 * @returns The bucket size.
 */
static uint32 GetHeapBucketSize(uint32 Bucket)
{
	uint32 Index = Bucket;
	checkf(Index < NumHeapBucketSizes, TEXT("%d %d"), Index, NumHeapBucketSizes);
	return HeapBufferBucketSizes[Index];
}

static MTLPixelFormat FromSRGBFormat(MTLPixelFormat Format)
{
	MTLPixelFormat MTLFormat = Format;
	
	switch (Format)
	{
		case MTLPixelFormatRGBA8Unorm_sRGB:
			MTLFormat = MTLPixelFormatRGBA8Unorm;
			break;
		case MTLPixelFormatBGRA8Unorm_sRGB:
			MTLFormat = MTLPixelFormatBGRA8Unorm;
			break;
#if PLATFORM_MAC
		case MTLPixelFormatBC1_RGBA_sRGB:
			MTLFormat = MTLPixelFormatBC1_RGBA;
			break;
		case MTLPixelFormatBC2_RGBA_sRGB:
			MTLFormat = MTLPixelFormatBC2_RGBA;
			break;
		case MTLPixelFormatBC3_RGBA_sRGB:
			MTLFormat = MTLPixelFormatBC3_RGBA;
			break;
		case MTLPixelFormatBC7_RGBAUnorm_sRGB:
			MTLFormat = MTLPixelFormatBC7_RGBAUnorm;
			break;
#endif //PLATFORM_MAC
#if PLATFORM_IOS
		case MTLPixelFormatR8Unorm_sRGB:
			MTLFormat = MTLPixelFormatR8Unorm;
			break;
		case MTLPixelFormatPVRTC_RGBA_2BPP_sRGB:
			MTLFormat = MTLPixelFormatPVRTC_RGBA_2BPP;
			break;
		case MTLPixelFormatPVRTC_RGBA_4BPP_sRGB:
			MTLFormat = MTLPixelFormatPVRTC_RGBA_4BPP;
			break;
		case MTLPixelFormatASTC_4x4_sRGB:
			MTLFormat = MTLPixelFormatASTC_4x4_LDR;
			break;
		case MTLPixelFormatASTC_6x6_sRGB:
			MTLFormat = MTLPixelFormatASTC_6x6_LDR;
			break;
		case MTLPixelFormatASTC_8x8_sRGB:
			MTLFormat = MTLPixelFormatASTC_8x8_LDR;
			break;
		case MTLPixelFormatASTC_10x10_sRGB:
			MTLFormat = MTLPixelFormatASTC_10x10_LDR;
			break;
		case MTLPixelFormatASTC_12x12_sRGB:
			MTLFormat = MTLPixelFormatASTC_12x12_LDR;
			break;
#endif //PLATFORM_IOS
		default:
			break;
	}
	
	return MTLFormat;
}

static EPixelFormat MetalToRHIPixelFormat(MTLPixelFormat Format)
{
	Format = FromSRGBFormat(Format);
	for (uint32 i = 0; i < PF_MAX; i++)
	{
		if(GPixelFormats[i].PlatformFormat == Format)
		{
			return (EPixelFormat)i;
		}
	}
	check(false);
	return PF_MAX;
}

static MTLSizeAndAlign TextureSizeAndAlign(MTLTextureType TextureType, uint32 Width, uint32 Height, uint32 Depth, MTLPixelFormat Format, uint32 MipCount, uint32 SampleCount, uint32 ArrayCount)
{
	MTLSizeAndAlign SizeAlign;
	SizeAlign.size = 0;
	SizeAlign.align = 0;
	
	uint32 Align = 0;
	switch (TextureType)
	{
		case MTLTextureType2D:
		case MTLTextureType2DMultisample:
			SizeAlign.size = RHICalcTexture2DPlatformSize(Width, Height, MetalToRHIPixelFormat(Format), MipCount, SampleCount, 0, Align);
			SizeAlign.align = Align;
			break;
		case MTLTextureType2DArray:
			SizeAlign.size = RHICalcTexture2DPlatformSize(Width, Height, MetalToRHIPixelFormat(Format), MipCount, SampleCount, 0, Align) * ArrayCount;
			SizeAlign.align = Align;
			break;
		case MTLTextureTypeCube:
			SizeAlign.size = RHICalcTextureCubePlatformSize(Width, MetalToRHIPixelFormat(Format), MipCount, 0, Align);
			SizeAlign.align = Align;
			break;
		case EMTLTextureTypeCubeArray:
			SizeAlign.size = RHICalcTextureCubePlatformSize(Width, MetalToRHIPixelFormat(Format), MipCount, 0, Align) * ArrayCount;
			SizeAlign.align = Align;
			break;
		case MTLTextureType3D:
			SizeAlign.size = RHICalcTexture3DPlatformSize(Width, Height, Depth, MetalToRHIPixelFormat(Format), MipCount, 0, Align);
			SizeAlign.align = Align;
			break;
		case MTLTextureType1D:
		case MTLTextureType1DArray:
		default:
			check(false);
			break;
	}
	
	return SizeAlign;
}

static MTLSizeAndAlign TextureSizeAndAlignForDescriptor(MTLTextureDescriptor* Desc)
{
#if !PLATFORM_MAC
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps))
	{
		return [GetMetalDeviceContext().GetDevice() heapTextureSizeAndAlignWithDescriptor:Desc];
	}
	else
#endif
	{
		return TextureSizeAndAlign(Desc.textureType, Desc.width, Desc.height, Desc.depth, Desc.pixelFormat, Desc.mipmapLevelCount, Desc.sampleCount, Desc.arrayLength);
	}
}

@interface FMetalResourceData : FApplePlatformObject
{
	@public
	FMetalResouceType type;
	NSUInteger size;
	uint64 timestamp;
	FMetalHeap::EMetalHeapStorage mode;
	uint32 usage;
	int32 volatile aliased;
	void* owner;
}
@property (retain) FMTLHeap* heap;
@property (assign) id resource;
@end
@implementation FMetalResourceData
@synthesize heap;
@synthesize resource;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalResourceData)

-(void)dealloc
{
	switch(self->type)
	{
		case FMetalResouceTypeBuffer:
		{
			DEC_DWORD_STAT(STAT_MetalBufferCount);
			DEC_MEMORY_STAT_BY(STAT_MetalPooledBufferMem, self->size);
			DEC_DWORD_STAT(STAT_MetalPooledBufferCount);
			DEC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, self->size);
			INC_DWORD_STAT(STAT_MetalBufferNativeFreed);
			INC_DWORD_STAT_BY(STAT_MetalBufferNativeMemFreed, self->size);
			break;
		}
		case FMetalResouceTypeTexture:
		{
			DEC_DWORD_STAT(STAT_MetalTextureCount);
#if STATS
			switch (self->mode)
			{
				case FMetalHeap::EMetalHeapStorageGPUCached:
				case FMetalHeap::EMetalHeapStorageGPUWriteCombine:
				{
					DEC_DWORD_STAT(STAT_MetalPrivateTextureCount);
					DEC_MEMORY_STAT_BY(STAT_MetalPrivateTextureMem, self->size);
					break;
				}
				case FMetalHeap::EMetalHeapStorageCPUCached:
				case FMetalHeap::EMetalHeapStorageCPUWriteCombine:
				case FMetalHeap::EMetalHeapStorageDMACached:
				case FMetalHeap::EMetalHeapStorageDMAWriteCombine:
				{
					DEC_DWORD_STAT(STAT_MetalManagedTextureCount);
					DEC_MEMORY_STAT_BY(STAT_MetalManagedTextureMem, self->size);
					break;
				}
				default:
				{
					checkf(false, TEXT("Invalid texture storage mode: %d."), (uint32)self->mode);
					break;
				}
			}
#endif
			break;
		}
		default:
		{
			check(false);
			break;
		}
	}
	
	[super dealloc];
}
@end

@interface NSObject (TMetalHeap)
@property (nonatomic, strong) NSNumber* createdTime;
@end

@implementation NSObject (TMetalHeap)
@dynamic createdTime;

- (void)setCreatedTime:(NSNumber*)Time
{
	objc_setAssociatedObject(self, @selector(createdTime), Time, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (NSNumber*)createdTime
{
	return (NSNumber*)objc_getAssociatedObject(self, @selector(createdTime));
}
@end

@interface NSObject (TMetalResouceHeap)
@property (nonatomic, strong) FMetalResourceData* resourceData;
-(id<MTLHeap>) heap:(BOOL)bSupportsHeaps;
-(void) makeAliasable:(BOOL)bSupportsHeaps;
-(BOOL) isAliasable:(BOOL)bSupportsHeaps;
@end

@implementation FMTLHeap
@synthesize label;
@synthesize device;
@synthesize storageMode;
@synthesize cpuCacheMode;
@synthesize size;
@synthesize usedSize;
@synthesize poolSize;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMTLHeap)

- (id)initWithDescriptor:(FMTLHeapDescriptor *)descriptor
{
	FMTLHeap* NewSelf = (FMTLHeap*)[super init];
	if (NewSelf)
	{
		NewSelf->device = descriptor->Device;
		NewSelf->size = descriptor->Size;
		NewSelf->storageMode = descriptor->StorageMode;
		NewSelf->cpuCacheMode = descriptor->CPUCacheMode;
		NewSelf->usedSize = 0;
		NewSelf->poolSize = 0;
		NewSelf->PurgableState = MTLPurgeableStateNonVolatile;
		NewSelf->PoolMutex = new FCriticalSection;
		NewSelf->Resources = [NSMutableSet new];
		for (uint32 i = 0; i < NumHeapBucketSizes; i++)
		{
			BufferBuckets[i] = [NSMutableArray new];
		}
	}
	return NewSelf;
}

- (void)dealloc
{
	for (uint32 i = 0; i < NumHeapBucketSizes; i++)
	{
		[BufferBuckets[i] release];
	}
	for (auto Pair : TextureCache)
	{
		if (Pair.Value)
		{
			[Pair.Value release];
		}
	}
	delete PoolMutex;
	[Resources release];
	[super dealloc];
}

- (NSUInteger)maxAvailableSizeWithAlignment:(NSUInteger)alignment
{
	NSUInteger align = FMath::Max(alignment, NSUInteger(1));
	NSUInteger avail = self.size - self.usedSize;
	NSUInteger aligned = (avail % align) == 0 ? avail : Align(avail, align) - align;
	return aligned;
}

- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options
{
	FScopeLock Lock(PoolMutex);
	
	NSObject<MTLBuffer>* Buffer = nil;

	uint32 Index = GetHeapBucketIndex(length);
	Buffer = (NSObject<MTLBuffer>*)[BufferBuckets[Index].firstObject retain];
	if (Buffer != nil)
	{
		[BufferBuckets[Index] removeObjectAtIndex:0];
	}
	else
	{
		NSUInteger Size = GetHeapBucketSize(Index);
		Buffer = (NSObject<MTLBuffer>*)[device newBufferWithLength:Size options:options];
		
		TRACK_OBJECT(STAT_MetalBufferCount, Buffer);
		INC_DWORD_STAT(STAT_MetalPooledBufferCount);
		INC_MEMORY_STAT_BY(STAT_MetalPooledBufferMem, Size);
		INC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, Size);
		INC_DWORD_STAT(STAT_MetalBufferNativeAlloctations);
		INC_DWORD_STAT_BY(STAT_MetalBufferNativeMemAlloc, Size);
	}
	if (Buffer.resourceData == nil)
	{
		Buffer.resourceData = [[[FMetalResourceData alloc] init] autorelease];
	}
	Buffer.resourceData.resource = Buffer;
	Buffer.resourceData.heap = self;
	Buffer.resourceData->type = FMetalResouceTypeBuffer;
	FPlatformAtomics::InterlockedExchange(&Buffer.resourceData->aliased, 0);
	Buffer.resourceData->size = Buffer.length;
	Buffer.resourceData->timestamp = mach_absolute_time();
	Buffer.resourceData->mode = FMetalHeap::ResouceOptionsToStorage(MTLResourceOptions((MTLResourceStorageModeMask & (Buffer.storageMode << MTLResourceStorageModeShift)) | (MTLResourceCPUCacheModeMask & (Buffer.cpuCacheMode << MTLResourceCPUCacheModeShift))));
	Buffer.resourceData->usage = FMetalHeap::BufferSizeToIndex(length);
	[Resources addObject:Buffer];
	[Buffer setPurgeableState:PurgableState];
	self->usedSize += Buffer.length;
	
	INC_MEMORY_STAT_BY(STAT_MetalUsedPooledBufferMem, Buffer.length);
	INC_DWORD_STAT(STAT_MetalBufferAlloctations);
	INC_DWORD_STAT_BY(STAT_MetalBufferMemAlloc, Buffer.length);
	DEC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, Buffer.length);
	INC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buffer.length - length);
#if !UE_BUILD_SHIPPING
	FPlatformAtomics::InterlockedAdd(&HeapWastage[Index], (Buffer.length - length));
#endif
	
	return Buffer;
}

- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)desc
{
	FScopeLock Lock(PoolMutex);
	
	FMetalTextureDesc CacheDesc;
	CacheDesc.textureType = desc.textureType;
	CacheDesc.pixelFormat = desc.pixelFormat;
	CacheDesc.width = desc.width;
	CacheDesc.height = desc.height;
	CacheDesc.depth = desc.depth;
	CacheDesc.mipmapLevelCount = desc.mipmapLevelCount;
	CacheDesc.sampleCount = desc.sampleCount;
	CacheDesc.arrayLength = desc.arrayLength;
	
	static bool bSupportsResourceOpts = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
	if (bSupportsResourceOpts)
	{
		CacheDesc.resourceOptions = desc.resourceOptions;
		CacheDesc.cpuCacheMode = desc.cpuCacheMode;
		CacheDesc.storageMode = desc.storageMode;
		CacheDesc.usage = desc.usage;
	}
	else
	{
		CacheDesc.resourceOptions = MTLResourceCPUCacheModeDefaultCache;
		CacheDesc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
		CacheDesc.storageMode = MTLStorageModeShared;
		CacheDesc.usage = MTLTextureUsageUnknown;
	}
	
	NSObject<MTLTexture>* Tex = nil;
	NSMutableSet<id<MTLTexture>>* Cache = TextureCache.FindRef(CacheDesc);
	if (Cache && Cache.count > 0)
	{
		Tex = (NSObject<MTLTexture>*)[Cache.anyObject retain];
		[Cache removeObject:Tex];
	}
	if (!Tex)
	{
		Tex = (NSObject<MTLTexture>*)[device newTextureWithDescriptor:desc];
		TRACK_OBJECT(STAT_MetalTextureCount, Tex);
		
#if STATS
		switch (desc.storageMode)
		{
			case MTLStorageModePrivate:
			{
				INC_DWORD_STAT(STAT_MetalPrivateTextureCount);
				INC_MEMORY_STAT_BY(STAT_MetalPrivateTextureMem, TextureSizeAndAlignForDescriptor(desc).size);
				break;
			}
			case MTLStorageModeShared:
#if PLATFORM_MAC
			case MTLStorageModeManaged:
#endif
			{
				INC_DWORD_STAT(STAT_MetalManagedTextureCount);
				INC_MEMORY_STAT_BY(STAT_MetalManagedTextureMem, TextureSizeAndAlignForDescriptor(desc).size);
				break;
			}
			default:
			{
				checkf(false, TEXT("Invalid texture storage mode: %d."), (uint32)desc.storageMode);
				break;
			}
		}
#endif
	}
	if (Tex.resourceData == nil)
	{
		Tex.resourceData = [[[FMetalResourceData alloc] init] autorelease];
	}
	Tex.resourceData.resource = Tex;
	Tex.resourceData.heap = self;
	Tex.resourceData->type = FMetalResouceTypeTexture;
	FPlatformAtomics::InterlockedExchange(&Tex.resourceData->aliased, 0);
	Tex.resourceData->size = TextureSizeAndAlignForDescriptor(desc).size;
	Tex.resourceData->timestamp = mach_absolute_time();
	Tex.resourceData->mode = FMetalHeap::ResouceOptionsToStorage(CacheDesc.resourceOptions);
	Tex.resourceData->usage = FMetalHeap::TextureDescToIndex(desc);
	
	[Tex setPurgeableState:PurgableState];
	[Resources addObject:Tex];
	self->usedSize += Tex.resourceData->size;
	return Tex;
}

- (MTLPurgeableState)setPurgeableState:(MTLPurgeableState)state
{
	MTLPurgeableState Val = PurgableState;
	if (state != MTLPurgeableStateKeepCurrent)
	{
		FScopeLock Lock(PoolMutex);
		for (id<MTLResource> Resource : Resources)
		{
			[Resource setPurgeableState:state];
		}
		PurgableState = state;
	}
	return PurgableState;
}

- (void)aliasBuffer:(id<MTLBuffer> _Nullable)buffer
{
	FScopeLock Lock(PoolMutex);
	
	NSObject<MTLBuffer>* res = (NSObject<MTLBuffer>*)buffer;
	check(res.resourceData != nil);
	check(res.resourceData->aliased == NO);
	
	FPlatformAtomics::InterlockedExchange(&res.resourceData->aliased, 1);
	res.resourceData->timestamp = mach_absolute_time();
	self->usedSize -= res.resourceData->size;
	self->poolSize += res.resourceData->size;
	
	DEC_MEMORY_STAT_BY(STAT_MetalUsedPooledBufferMem, buffer.length);
	INC_DWORD_STAT(STAT_MetalBufferFreed);
	INC_DWORD_STAT_BY(STAT_MetalBufferMemFreed, buffer.length);
	INC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, buffer.length);
	DEC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, buffer.length - res.resourceData->size);
	
	uint32 Index = GetHeapBucketIndex(buffer.length);
	check(![BufferBuckets[Index] containsObject:buffer]);
	[BufferBuckets[Index] addObject:buffer];
#if !UE_BUILD_SHIPPING
	FPlatformAtomics::InterlockedAdd(&HeapWastage[Index], -(buffer.length - res.resourceData->size));
#endif
}

- (void)aliasTexture:(id<MTLTexture> _Nullable)texture
{
	FScopeLock Lock(PoolMutex);
	
	NSObject<MTLTexture>* res = (NSObject<MTLTexture>*)texture;
	check(res.resourceData != nil);
	check(res.resourceData->aliased == NO);

	FMetalTextureDesc CacheDesc;
	CacheDesc.textureType = texture.textureType;
	CacheDesc.pixelFormat = texture.pixelFormat;
	CacheDesc.width = texture.width;
	CacheDesc.height = texture.height;
	CacheDesc.depth = texture.depth;
	CacheDesc.mipmapLevelCount = texture.mipmapLevelCount;
	CacheDesc.sampleCount = texture.sampleCount;
	CacheDesc.arrayLength = texture.arrayLength;
	FPlatformAtomics::InterlockedExchange(&res.resourceData->aliased, 1);
	
	static bool bSupportsResourceOpts = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
	if (bSupportsResourceOpts)
	{
		static MTLResourceOptions GeneralResourceOption = GetMetalDeviceContext().GetCommandQueue().GetCompatibleResourceOptions(MTLResourceHazardTrackingModeUntracked);
		CacheDesc.resourceOptions = ((texture.cpuCacheMode << MTLResourceCPUCacheModeShift) | (texture.storageMode << MTLResourceStorageModeShift) | GeneralResourceOption);
		CacheDesc.cpuCacheMode = texture.cpuCacheMode;
		CacheDesc.storageMode = texture.storageMode;
		CacheDesc.usage = texture.usage;
	}
	else
	{
		CacheDesc.resourceOptions = MTLResourceCPUCacheModeDefaultCache;
		CacheDesc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
		CacheDesc.storageMode = MTLStorageModeShared;
		CacheDesc.usage = MTLTextureUsageUnknown;
	}
	
	NSMutableSet<id<MTLTexture>>* Cache = TextureCache.FindRef(CacheDesc);
	if (!Cache)
	{
		Cache = [NSMutableSet new];
		TextureCache.Add(CacheDesc, Cache);
	}
	[Cache addObject:texture];
	
	res.resourceData->timestamp = mach_absolute_time();
	self->usedSize -= res.resourceData->size;
	self->poolSize += res.resourceData->size;
}

- (void)drain:(bool)bForce
{
	FScopeLock Lock(PoolMutex);
	for (uint32 i = 0; i < NumHeapBucketSizes; i++)
	{
		while (BufferBuckets[i].firstObject)
		{
			NSObject<MTLBuffer>* res = (NSObject<MTLBuffer>*)BufferBuckets[i].firstObject;
			if (bForce || self.usedSize == 0 || FPlatformTime::ToSeconds(mach_absolute_time() - res.resourceData->timestamp) >= 1.0f || (self.poolSize > (self.size / 5)))
			{
				[Resources removeObject:BufferBuckets[i].firstObject];
				[BufferBuckets[i] removeObjectAtIndex:0];
			}
			else
			{
				break;
			}
		}
	}
	for (auto Pair : TextureCache)
	{
		if (Pair.Value && Pair.Value.count)
		{
			while (Pair.Value.anyObject)
			{
				id<MTLTexture> Tex = Pair.Value.anyObject;
				NSObject<MTLTexture>* res = (NSObject<MTLTexture>*)Tex;
				if (bForce || self.usedSize == 0 || FPlatformTime::ToSeconds(mach_absolute_time() - res.resourceData->timestamp) >= 1.0f || (self.poolSize > (self.size / 5)))
				{
					self->poolSize -= res.resourceData->size;
					
					[Resources removeObject:Tex];
					[Pair.Value removeObject:Tex];
				}
				else
				{
					break;
				}
			}
		}
	}
}
@end

@implementation NSObject (TMetalResouceHeap)
@dynamic resourceData;

- (void)setResourceData:(FMetalResourceData*)Data
{
	objc_setAssociatedObject(self, @selector(resourceData), Data, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (FMetalResourceData*)resourceData
{
	return (FMetalResourceData*)objc_getAssociatedObject(self, @selector(resourceData));
}

- (FMTLHeap*)metalHeap
{
	FMetalResourceData* data = [self resourceData];
	if(data)
	{
		return data.heap;
	}
	else
	{
		return nil;
	}
}

- (id<MTLHeap>)heap:(BOOL)bSupportsHeaps
{
#if METAL_SUPPORTS_HEAPS
	if (bSupportsHeaps && ![self metalHeap])
	{
		return [((id<MTLResource>)self) heap];
	}
	else
#endif
	{
		return [self metalHeap];
	}
}

- (FMetalResouceType)metalType
{
	FMetalResourceData* data = [self resourceData];
	if(data)
	{
		return data->type;
	}
	else
	{
		return FMetalResouceTypeInvalid;
	}
}

-(void)makeAliasable:(BOOL)bSupportsHeaps
{
#if METAL_SUPPORTS_HEAPS
	if (bSupportsHeaps && ![self metalHeap])
	{
		[((id<MTLResource>)self) makeAliasable];
	}
	else
#endif
	{
		FMTLHeap* heap = [self metalHeap];
		if (heap)
		{
			switch([self metalType])
			{
				case FMetalResouceTypeBuffer:
				{
					[heap aliasBuffer:(id<MTLBuffer>)self];
					break;
				}
				case FMetalResouceTypeTexture:
				{
					[heap aliasTexture:(id<MTLTexture>)self];
					break;
				}
				default:
				{
					check(false);
					break;
				}
			}
		}
	}
}

-(BOOL)isAliasable:(BOOL)bSupportsHeaps
{
#if METAL_SUPPORTS_HEAPS
	if (bSupportsHeaps && ![self metalHeap])
	{
		return [((id<MTLResource>)self) isAliasable];
	}
	else
#endif
	{
		FMetalResourceData* data = [self resourceData];
		if(data)
		{
			return data->aliased;
		}
		else
		{
			return NO;
		}
	}
}

@end

static double StaticTextureHeapSizes[] = {
	0.30, 0.20
};
static TCHAR const* StaticTextureHeapName[] = {
	TEXT("InitialTexturePoolFraction"),
	TEXT("InitialRenderTargetTexturePoolFraction"),
};

FMetalHeap::FMetalHeap(void)
: Queue(nullptr)
, TotalTextureMemory(0)
{
	
#if !UE_BUILD_SHIPPING
	FMemory::Memzero((void*)&HeapWastage[0], sizeof(HeapWastage));
#endif
	FMemory::Memzero(StaticTextureHeaps, sizeof(StaticTextureHeaps));
}

FMetalHeap::~FMetalHeap()
{
	for (uint32 i = 0; i < EMetalHeapStorageNum; i++)
	{
		for (uint32 j = 0; j < EMetalHeapBufferSizesNum; j++)
		{
			for(TPair<id<MTLHeap>, uint64> Entry : BufferHeaps[i][j])
			{
				[Entry.Key release];
			}
			
			for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
			{
				for(TPair<id<MTLHeap>, uint64> Entry : DynamicTextureHeaps[i][j][k])
				{
					[Entry.Key release];
				}
			}
		}
		
		for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
		{
			if (StaticTextureHeaps[i][k])
			{
				[StaticTextureHeaps[i][k] release];
			}
		}
	}
}

void FMetalHeap::Init(FMetalCommandQueue& InQueue)
{
	Queue = &InQueue;
	
	FMTLHeapDescriptor Descriptor;
	Descriptor.Device = InQueue.GetDevice();
	
	EMetalHeapStorage Storage = EMetalHeapStorageCPUCached;
	if (InQueue.SupportsFeature(EMetalFeaturesResourceOptions))
	{
		Storage = EMetalHeapStorageGPUCached;
	}
	
	Descriptor.StorageMode = (MTLStorageMode)(Storage / EMetalHeapCacheNum);
	Descriptor.CPUCacheMode = (MTLCPUCacheMode)(Storage % EMetalHeapCacheNum);
	
	check((uint32)Descriptor.StorageMode < (uint32)EMetalHeapTypeNum);
	check((uint32)Descriptor.CPUCacheMode < (uint32)EMetalHeapCacheNum);
	
#if PLATFORM_MAC
	static TCHAR const* Section = TEXT("/Script/MacTargetPlatform.MacTargetSettings");
#else
	static TCHAR const* Section = TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings");
#endif
	
	for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
	{
		GConfig->GetDouble(Section, StaticTextureHeapName[k], StaticTextureHeapSizes[k], GEngineIni);
	
		Descriptor.Size = (NSUInteger)(StaticTextureHeapSizes[k] * (double)GTexturePoolSize);
		if (Descriptor.Size > 0)
		{
			StaticTextureHeaps[Storage][k] = CreateHeap(Descriptor);
			TotalTextureMemory += Descriptor.Size;
			INC_DWORD_STAT_FName(NumTextureHeapStats[k]);
		}
	}
}

id<MTLHeap> FMetalHeap::CreateHeap(FMTLHeapDescriptor& Desc)
{
	FScopeLock Lock(&Mutex);
	id<MTLHeap> Heap = nil;
	
	for (id<MTLHeap> theHeap : ReleasedHeaps)
	{
		if (theHeap.storageMode == Desc.StorageMode && theHeap.cpuCacheMode == Desc.CPUCacheMode
			&& Desc.Size == theHeap.size)
		{
			Heap = theHeap;
			ReleasedHeaps.Remove(theHeap);
			break;
		}
	}
	
	if (!Heap)
	{
		if(FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps))
		{
			id<TMTLDevice> Device = (id<TMTLDevice>)Queue->GetDevice();
			Class MTLHeapDescriptorClass = NSClassFromString(@"MTLHeapDescriptor");
			id<TMTLHeapDescriptor> HeapDesc = (id<TMTLHeapDescriptor>)[[MTLHeapDescriptorClass alloc] init];
			
			HeapDesc.size = Desc.Size;
			HeapDesc.storageMode = Desc.StorageMode;
			HeapDesc.cpuCacheMode = Desc.CPUCacheMode;
			Heap = [Device newHeapWithDescriptor:(MTLHeapDescriptorRef)HeapDesc];
			
			[HeapDesc release];
		}
		else
		{
			Heap = [[FMTLHeap alloc] initWithDescriptor:&Desc];
		}
		
		((NSObject*)Heap).createdTime = [NSNumber numberWithUnsignedLongLong:mach_absolute_time()];
	}
	return Heap;
}

id<MTLBuffer> FMetalHeap::CreateBuffer(uint32 Size, MTLResourceOptions Options)
{
	@autoreleasepool {
		
	FScopeLock Lock(&Mutex);
	
	EMetalHeapStorage Storage = ResouceOptionsToStorage(Options);
	EMetalHeapBufferSizes Usage = BufferSizeToIndex(Size);
	
	MTLSizeAndAlign SizeAlign;
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps))
	{
		SizeAlign = [((id<TMTLDevice>)Queue->GetDevice()) heapBufferSizeAndAlignWithLength:Size options:
		 Options];
	}
	else
	{
		SizeAlign.size = Size;
		SizeAlign.align = 0;
	}
	
	id<MTLHeap> Heap = nil;
	if (Usage <= EMetalHeapBufferAllowLookAhead)
	{
		for (uint32 i = 0, j = Usage; i <= EMetalHeapBufferLookAhead && j <= EMetalHeapBufferAllowLookAhead; i++, j++)
		{
			for (TPair<id<MTLHeap>, uint64>& Entry : BufferHeaps[Storage][j])
			{
				if ([Entry.Key maxAvailableSizeWithAlignment:SizeAlign.align] >= SizeAlign.size)
				{
					Heap = Entry.Key;
					Entry.Value = GFrameNumberRenderThread;
					break;
				}
			}
		}
	}
	else
	{
		uint32 CurrentSize = 0;
		for (TPair<id<MTLHeap>, uint64>& Entry : BufferHeaps[Storage][Usage])
		{
			uint32 FreeSize = ((Entry.Key.size - Entry.Key.usedSize) - SizeAlign.size);
			if ([Entry.Key maxAvailableSizeWithAlignment:SizeAlign.align] >= SizeAlign.size && FreeSize < CurrentSize && (FreeSize < (Entry.Key.size * GMetalHeapDefragUnderUtilisedFraction)))
			{
				Heap = Entry.Key;
				Entry.Value = GFrameNumberRenderThread;
				CurrentSize = FreeSize;
			}
		}
	}
	if (!Heap)
	{
		FMTLHeapDescriptor Desc;
		Desc.Size = (Usage < EMetalHeapBufferExactSize ? HeapBufferBlock[Usage] : SizeAlign.size);
		Desc.Device = Queue->GetDevice();
		Desc.StorageMode = (MTLStorageMode)(Storage / EMetalHeapCacheNum);
		Desc.CPUCacheMode = (MTLCPUCacheMode)(Storage % EMetalHeapCacheNum);
		
		check((uint32)Desc.StorageMode < (uint32)EMetalHeapTypeNum);
		check((uint32)Desc.CPUCacheMode < (uint32)EMetalHeapCacheNum);
		
		Heap = CreateHeap(Desc);
		if (Heap)
		{
			BufferHeaps[Storage][Usage].Add(TPairInitializer<id<MTLHeap>, uint64>(Heap, GFrameNumberRenderThread));
			
			INC_MEMORY_STAT_BY(STAT_MetalHeapBufferMemory, SizeAlign.size);
			INC_MEMORY_STAT_BY(STAT_MetalHeapTotalBuffer, Heap.size);
			INC_DWORD_STAT(STAT_MetalHeapNumBufferHeaps);
		}
		else
		{
			UE_LOG(LogMetal, Warning, TEXT("Failed to create new buffer heap for size %d - allocations will happen on the device directly!"), HeapBufferSizes[Usage]);
		}
	}
	
	id<MTLBuffer> Buffer = nil;
	if (Heap)
	{
		Buffer = [Heap newBufferWithLength:(NSUInteger)Size options:(MTLResourceOptions)Options];
	}
	else
	{
		Buffer = [Queue->GetDevice() newBufferWithLength:(NSUInteger)Size options:(MTLResourceOptions)Options];
	}
		
	INC_FLOAT_STAT_BY(STAT_MetalHeapBufferAllocMemory, ((float)Size / 1024.f / 1024.f));
	
	NSObject<MTLBuffer>* TBuffer = (NSObject<MTLBuffer>*)Buffer;
	if (TBuffer.resourceData != nil)
	{
		check(TBuffer.resourceData->size >= Buffer.length);
		check(TBuffer.resourceData->type == FMetalResouceTypeBuffer);
		check(TBuffer.resourceData.resource == Buffer);
		TBuffer.resourceData->owner = nullptr;
		FPlatformAtomics::InterlockedExchange(&TBuffer.resourceData->aliased, 0);
	}
	else
	{
		TBuffer.resourceData = [[[FMetalResourceData alloc] init] autorelease];
		TBuffer.resourceData.resource = Buffer;
		TBuffer.resourceData.heap = nil;
		TBuffer.resourceData->type = FMetalResouceTypeBuffer;
		TBuffer.resourceData->size = Buffer.length;
		TBuffer.resourceData->timestamp = mach_absolute_time();
		TBuffer.resourceData->mode = Storage;
		TBuffer.resourceData->usage = Usage;
		TBuffer.resourceData->owner = nullptr;
		FPlatformAtomics::InterlockedExchange(&TBuffer.resourceData->aliased, 0);
		TRACK_OBJECT(STAT_MetalBufferCount, Buffer);
		INC_DWORD_STAT(STAT_MetalPooledBufferCount);
		INC_MEMORY_STAT_BY(STAT_MetalPooledBufferMem, Buffer.length);
		INC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, Buffer.length);
		INC_DWORD_STAT(STAT_MetalBufferNativeAlloctations);
		INC_DWORD_STAT_BY(STAT_MetalBufferNativeMemAlloc, Buffer.length);
	}
		
#if STATS
	uint64 UsedBuffer = 0;
	
	for (uint32 i = 0; i < EMetalHeapStorageNum; i++)
	{
		for (uint32 j = 0; j < EMetalHeapBufferSizesNum; j++)
		{
			for(uint32 k = 0; k < BufferHeaps[i][j].Num(); k++)
			{
				id<MTLHeap> theHeap = BufferHeaps[i][j][k].Key;
				
				UsedBuffer += (theHeap ? theHeap.usedSize : 0);
			}
		}
	}
	
	PeakBufferMemory = FMath::Max(PeakBufferMemory, UsedBuffer);
	SET_MEMORY_STAT(STAT_MetalHeapBufferPeakMemory, PeakBufferMemory);
		
#endif
	
	return Buffer;
	}
}

id<MTLTexture> FMetalHeap::CreateTexture(MTLTextureDescriptor* Desc, FMetalSurface* Surface)
{
	@autoreleasepool {
	
	FScopeLock Lock(&Mutex);
	EMetalHeapStorage Storage = ResouceOptionsToStorage(Desc.resourceOptions);
	EMetalHeapTextureUsage Usage = TextureDescToIndex(Desc);
	
	MTLSizeAndAlign SizeAlign;
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps))
	{
		SizeAlign = [((id<TMTLDevice>)Queue->GetDevice()) heapTextureSizeAndAlignWithDescriptor:Desc];
	}
	else
	{
		SizeAlign = TextureSizeAndAlignForDescriptor(Desc);
	}
	
    EMetalHeapBufferSizes Size = BufferSizeToIndex(SizeAlign.size);
    id<MTLHeap> Heap = nil;
	if (StaticTextureHeaps[Storage][Usage] && [StaticTextureHeaps[Storage][Usage] maxAvailableSizeWithAlignment:SizeAlign.align] >= SizeAlign.size)
	{
		Heap = StaticTextureHeaps[Storage][Usage];
	}
	if (!Heap)
	{
		if (Size <= EMetalHeapBufferAllowLookAhead)
		{
			for (uint32 i = 0, j = Size; i <= EMetalHeapBufferLookAhead && j <= EMetalHeapBufferAllowLookAhead; i++, j++)
			{
				for (TPair<id<MTLHeap>, uint64>& Entry : DynamicTextureHeaps[Storage][j][Usage])
				{
					if ([Entry.Key maxAvailableSizeWithAlignment:SizeAlign.align] >= SizeAlign.size)
					{
						Heap = Entry.Key;
						Entry.Value = GFrameNumberRenderThread;
						break;
					}
				}
			}
		}
		else
		{
			uint32 CurrentSize = 0;
			for (TPair<id<MTLHeap>, uint64>& Entry : DynamicTextureHeaps[Storage][Size][Usage])
			{
				uint32 FreeSize = ((Entry.Key.size - Entry.Key.usedSize) - SizeAlign.size);
				if ([Entry.Key maxAvailableSizeWithAlignment:SizeAlign.align] >= SizeAlign.size && FreeSize < CurrentSize && (FreeSize < (Entry.Key.size * GMetalHeapDefragUnderUtilisedFraction)))
				{
					Heap = Entry.Key;
					Entry.Value = GFrameNumberRenderThread;
					CurrentSize = FreeSize;
				}
			}
		}
	}
	if (!Heap)
	{
		FMTLHeapDescriptor Descriptor;
		Descriptor.Size = (Size < EMetalHeapBufferExactSize ? HeapBufferBlock[Size] : SizeAlign.size);
		Descriptor.Device = Queue->GetDevice();
		Descriptor.StorageMode = (MTLStorageMode)(Storage / EMetalHeapCacheNum);
		Descriptor.CPUCacheMode = (MTLCPUCacheMode)(Storage % EMetalHeapCacheNum);
		
		check((uint32)Descriptor.StorageMode < (uint32)EMetalHeapTypeNum);
		check((uint32)Descriptor.CPUCacheMode < (uint32)EMetalHeapCacheNum);
		
		if (GTexturePoolSize > 0 && (TotalTextureMemory + Descriptor.Size) > GTexturePoolSize)
		{
			UE_LOG(LogMetal, Verbose, TEXT("Texture heap allocations (%.2f) will exceed texture pool size (%.2f) - performance may suffer and the application may be subject to OS low-memory handling!"), (float)(TotalTextureMemory + Descriptor.Size) / 1024.f / 1024.f, ((float)GTexturePoolSize / 1024.f / 1024.f));
		}
		
		Heap = CreateHeap(Descriptor);
		if (Heap)
        {
			DynamicTextureHeaps[Storage][Size][Usage].Add(TPairInitializer<id<MTLHeap>, uint64>(Heap, GFrameNumberRenderThread));
			
			INC_MEMORY_STAT_FNAME_BY(TextureHeapStats[Usage], SizeAlign.size);
			INC_MEMORY_STAT_FNAME_BY(TotalTextureHeapStats[Usage], Heap.size);
			INC_DWORD_STAT_FName(NumTextureHeapStats[Usage]);
		}
		else
		{
			UE_LOG(LogMetal, Warning, TEXT("Failed to create new texture heap for descriptor %s - allocations will happen on the device directly!"), *FString([Desc description]));
		}
		
		TotalTextureMemory += Descriptor.Size;
	}
	
	return CreateTexture(Heap, Desc, Surface);
	}
}

id<MTLTexture> FMetalHeap::CreateTexture(id<MTLHeap> Heap, MTLTextureDescriptor* Desc, FMetalSurface* Surface)
{
	EMetalHeapStorage Storage = ResouceOptionsToStorage(Desc.resourceOptions);
	EMetalHeapTextureUsage Usage = TextureDescToIndex(Desc);
	
	id<MTLTexture> Tex = nil;
	if (Heap)
	{
		Tex = [Heap newTextureWithDescriptor:Desc];
		
		TSet<NSObject<MTLTexture>*>& Textures = TextureResources.FindOrAdd(Heap);
		Textures.Add((NSObject<MTLTexture>*)Tex);
	}
	else
	{
		
		Tex = [Queue->GetDevice() newTextureWithDescriptor:Desc];
	}
	
	INC_FLOAT_STAT_BY_FName(AllocatedTextureHeapStats[Usage], ((float)TextureSizeAndAlignForDescriptor(Desc).size / 1024.f / 1024.f));
	
	NSObject<MTLTexture>* TTexture = (NSObject<MTLTexture>*)Tex;
	if (TTexture.resourceData != nil)
	{
		check(TTexture.resourceData->size == TextureSizeAndAlignForDescriptor(Desc).size);
		check(TTexture.resourceData->type == FMetalResouceTypeTexture);
		check(TTexture.resourceData.resource == Tex);
		TTexture.resourceData->owner = Surface;
		FPlatformAtomics::InterlockedExchange(&TTexture.resourceData->aliased, 0);
	}
	else
	{
		TTexture.resourceData = [[[FMetalResourceData alloc] init] autorelease];
		TTexture.resourceData.resource = Tex;
		TTexture.resourceData.heap = nil;
		TTexture.resourceData->type = FMetalResouceTypeTexture;
		TTexture.resourceData->size = TextureSizeAndAlignForDescriptor(Desc).size;
		TTexture.resourceData->timestamp = mach_absolute_time();
		TTexture.resourceData->mode = Storage;
		TTexture.resourceData->usage = Usage;
		TTexture.resourceData->owner = Surface;
		FPlatformAtomics::InterlockedExchange(&TTexture.resourceData->aliased, 0);
		TRACK_OBJECT(STAT_MetalTextureCount, Tex);
#if STATS
		switch (Desc.storageMode)
		{
			case MTLStorageModePrivate:
			{
				INC_DWORD_STAT(STAT_MetalPrivateTextureCount);
				INC_MEMORY_STAT_BY(STAT_MetalPrivateTextureMem, TTexture.resourceData->size);
				break;
			}
			case MTLStorageModeShared:
#if PLATFORM_MAC
			case MTLStorageModeManaged:
#endif
			{
				INC_DWORD_STAT(STAT_MetalManagedTextureCount);
				INC_MEMORY_STAT_BY(STAT_MetalManagedTextureMem, TTexture.resourceData->size);
				break;
			}
			default:
			{
				checkf(false, TEXT("Invalid texture storage mode: %d."), (uint32)Desc.storageMode);
				break;
			}
		}
#endif
	}
	
#if STATS
	uint64 UsedSize[EMetalHeapTextureUsageNum] = {0, 0};
	for (uint32 i = 0; i < EMetalHeapStorageNum; i++)
	{
		for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
		{
			for (uint32 j = 0; j < EMetalHeapBufferSizesNum; j++)
			{
				for(uint32 l = 0; l < DynamicTextureHeaps[i][j][k].Num(); l++)
				{
					id<MTLHeap> theHeap = DynamicTextureHeaps[i][j][k][l].Key;
					UsedSize[k] += (theHeap ? theHeap.usedSize : 0);
				}
			}
			
			if (StaticTextureHeaps[i][k])
			{
				id<MTLHeap> theHeap = StaticTextureHeaps[i][k];
				UsedSize[k] += (theHeap ? theHeap.usedSize : 0);
			}
		}
	}
	
	for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
	{
		PeakTextureMemory[k] = FMath::Max(PeakTextureMemory[k], UsedSize[k]);
		SET_MEMORY_STAT_FName(PeakTextureHeapStats[k], PeakTextureMemory[k]);
	}
#endif
	
	return Tex;
}

void FMetalHeap::ReleaseHeap(id<MTLHeap> Heap)
{
	FScopeLock Lock(&Mutex);

	// Whatever was tracked before, this heap is truly dead now.
	TextureResources.Remove(Heap);
	ReleasedHeaps.Remove(Heap);
	[Heap release];
}

void FMetalHeap::ReleaseTexture(FMetalSurface* Surface, id<MTLTexture> Texture)
{
	FScopeLock Lock(&Mutex);
	
	NSObject<MTLTexture>* Res = (NSObject<MTLTexture>*)Texture;
	if (Res.resourceData && Res.resourceData->owner == Surface)
	{
		Res.resourceData->owner = nullptr;
		id<MTLHeap> Heap = [Res heap:FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps)];
		if (Heap)
		{
			TSet<NSObject<MTLTexture>*>* Textures = TextureResources.Find(Heap);
			if (Textures)
			{
				Textures->Remove(Res);
			}
		}
	}
}

id<MTLHeap> FMetalHeap::FindDefragHeap(EMetalHeapStorage Storage, EMetalHeapTextureUsage Usage, MTLTextureDescriptor* Desc, id<MTLHeap> CurrentHeap)
{
	MTLSizeAndAlign SizeAlign = TextureSizeAndAlignForDescriptor(Desc);
	EMetalHeapBufferSizes Size = BufferSizeToIndex(SizeAlign.size);
	
	id<MTLHeap> Heap = nil;
	if (StaticTextureHeaps[Storage][Usage] && [StaticTextureHeaps[Storage][Usage] maxAvailableSizeWithAlignment:SizeAlign.align] >= SizeAlign.size)
	{
		Heap = StaticTextureHeaps[Storage][Usage];
	}
	if (!Heap)
	{
		uint64 CurrentTimestamp = ((NSObject*)CurrentHeap).createdTime.unsignedLongLongValue;
	
		for (uint32 i = 0, j = Size; i <= EMetalHeapBufferLookAhead && j <= EMetalHeapBufferAllowLookAhead; i++, j++)
		{
			for (TPair<id<MTLHeap>, uint64>& Entry : DynamicTextureHeaps[Storage][j][Usage])
			{
				uint64 FreeSize = (Entry.Key.size - Entry.Key.usedSize) - SizeAlign.size;
				uint64 NewTimestamp = ((NSObject*)Entry.Key).createdTime.unsignedLongLongValue;
				if (Entry.Key != CurrentHeap && (FreeSize < CurrentHeap.size) && [Entry.Key maxAvailableSizeWithAlignment:SizeAlign.align] >= SizeAlign.size
				&& (CurrentTimestamp > NewTimestamp))
				{
					Heap = Entry.Key;
					Entry.Value = GFrameNumberRenderThread;
					break;
				}
			}
		}
	}
	return Heap;
}

void FMetalHeap::Compact(FMetalDeviceContext& Context, bool const bForce)
{
	Defrag(Context, bForce);
	Drain(Context, bForce);
}

void FMetalHeap::Defrag(FMetalDeviceContext& Context, bool const bForce)
{
	if(FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps))
	{
		FScopeLock Lock(&Mutex);
		
		int64 MemoryDefragged = 0;
		
		for (uint32 i = 0; i < EMetalHeapStorageNum; i++)
		{
			for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
			{
				// Largest to smallest - compacting smaller textures matters less
				for (int32 j = (EMetalHeapBufferSizesNum - 1); j >= 0; j--)
				{
					for (TPair<id<MTLHeap>, uint64> Pair : DynamicTextureHeaps[i][j][k])
					{
						// If we're only using half the space... or we've not been allocated from in a long time...
						if (((Pair.Key.size - Pair.Key.usedSize) >= (Pair.Key.size * GMetalHeapDefragUnderUtilisedFraction)) || ((Pair.Value - GFrameNumberRenderThread) > EMetalHeapConstantsCullAfterFrames))
						{
							TSet<NSObject<MTLTexture>*>& Textures = TextureResources.FindChecked(Pair.Key);
							TSet<NSObject<MTLTexture>*> TexturesCopy;
							for (NSObject<MTLTexture>* TextureResource : Textures)
							{
								// If this resource will fit and is a second or more old then we should compact it
								if (FPlatformTime::ToSeconds(mach_absolute_time() - TextureResource.resourceData->timestamp) >= 1.0f)
								{
									TexturesCopy.Add(TextureResource);
								}
							}

							// Only if all resources are old enough to defrag should we bother - otherwise we'll just bloat the memory use.
							if (TexturesCopy.Num() == Textures.Num())
							{
								for (NSObject<MTLTexture>* TextureResource : TexturesCopy)
								{
									MTLTextureDescriptor* Desc = [[MTLTextureDescriptor new] autorelease];
									Desc.textureType = TextureResource.textureType;
									Desc.pixelFormat = TextureResource.pixelFormat;
									Desc.width = TextureResource.width;
									Desc.height = TextureResource.height;
									Desc.depth = TextureResource.depth;
									Desc.mipmapLevelCount = TextureResource.mipmapLevelCount;
									Desc.sampleCount = TextureResource.sampleCount;
									Desc.arrayLength = TextureResource.arrayLength;
									
									static MTLResourceOptions GeneralResourceOption = Context.GetCommandQueue().GetCompatibleResourceOptions(MTLResourceHazardTrackingModeUntracked);
									
									Desc.resourceOptions = ((TextureResource.cpuCacheMode << MTLResourceCPUCacheModeShift) | (TextureResource.storageMode << MTLResourceStorageModeShift) | GeneralResourceOption);
									Desc.cpuCacheMode = TextureResource.cpuCacheMode;
									Desc.storageMode = TextureResource.storageMode;
									Desc.usage = TextureResource.usage;
									
									id<MTLHeap> Heap = FindDefragHeap((EMetalHeapStorage)i, (EMetalHeapTextureUsage)k, Desc, Pair.Key);
									if (Heap)
									{
										check(TextureResource.resourceData->owner);
										FMetalSurface* Surface = (FMetalSurface*)TextureResource.resourceData->owner;
										
										id<MTLTexture> NewTexture = CreateTexture(Heap, Desc, Surface);
										check(NewTexture);
										
										INC_DWORD_STAT(STAT_MetalHeapNumTextureReallocs);
										INC_DWORD_STAT(STAT_MetalHeapNumFrameTextureReallocs);
										INC_FLOAT_STAT_BY(STAT_MetalHeapTotalTextureReallocMemory, (float)TextureResource.resourceData->size / 1024.f / 1024.f);
										
										MemoryDefragged += TextureResource.resourceData->size;
										
										Surface->ReplaceTexture(Context, TextureResource, NewTexture);
										
										Textures.Remove(TextureResource);
										
										if (!bForce && (MemoryDefragged > GMetalHeapMemToDefragPerFrame))
										{
											// Early out from the nested loops as we've defragged as much as we can this time around...
											return;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void FMetalHeap::Drain(FMetalDeviceContext& Context, bool const bForce)
{
	FScopeLock Lock(&Mutex);
	
#if STATS
	SET_MEMORY_STAT(STAT_MetalHeapTotalBuffer, 0);
	SET_MEMORY_STAT(STAT_MetalHeapTotalTexture, 0);
	SET_MEMORY_STAT(STAT_MetalHeapTotalRenderTarget, 0);
	
	SET_MEMORY_STAT(STAT_MetalHeapBufferMemory, 0);
	SET_MEMORY_STAT(STAT_MetalHeapTextureMemory, 0);
	SET_MEMORY_STAT(STAT_MetalHeapRenderTargetMemory, 0);
	
	uint64 UsedBuffer = 0;
	uint64 UsedSize[EMetalHeapTextureUsageNum] = {0, 0};
	
	for (uint32 i = 0; i < EMetalHeapStorageNum; i++)
	{
		for (uint32 j = 0; j < EMetalHeapBufferSizesNum; j++)
		{
			for(uint32 k = 0; k < BufferHeaps[i][j].Num(); k++)
			{
				id<MTLHeap> Heap = BufferHeaps[i][j][k].Key;
				
				UsedBuffer += (Heap ? Heap.usedSize : 0);
				
				INC_MEMORY_STAT_BY(STAT_MetalHeapBufferMemory, (Heap ? Heap.usedSize : 0));
				INC_MEMORY_STAT_BY(STAT_MetalHeapTotalBuffer, (Heap ? Heap.size : 0));
			}
			
			for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
			{
				for(uint32 l = 0; l < DynamicTextureHeaps[i][j][k].Num(); l++)
				{
					id<MTLHeap> Heap = DynamicTextureHeaps[i][j][k][l].Key;
					
					UsedSize[k] += (Heap ? Heap.usedSize : 0);
					
					INC_MEMORY_STAT_FNAME_BY(TextureHeapStats[k], (Heap ? Heap.usedSize : 0));
					INC_MEMORY_STAT_FNAME_BY(TotalTextureHeapStats[k], (Heap ? Heap.size : 0));
				}
			}
		}
		
		for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
		{
			if (StaticTextureHeaps[i][k])
			{
				id<MTLHeap> Heap = StaticTextureHeaps[i][k];
				
				UsedSize[k] += (Heap ? Heap.usedSize : 0);
				
				INC_MEMORY_STAT_FNAME_BY(TextureHeapStats[k], (Heap ? Heap.usedSize : 0));
				INC_MEMORY_STAT_FNAME_BY(TotalTextureHeapStats[k], (Heap ? Heap.size : 0));
			}
		}
	}
	
	for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
	{
		PeakTextureMemory[k] = FMath::Max(PeakTextureMemory[k], UsedSize[k]);
		SET_MEMORY_STAT_FName(PeakTextureHeapStats[k], PeakTextureMemory[k]);
	}
	
	PeakBufferMemory = FMath::Max(PeakBufferMemory, UsedBuffer);
	SET_MEMORY_STAT(STAT_MetalHeapBufferPeakMemory, PeakBufferMemory);
#endif
	
	// Purge buffer memory that hasn't been used recently
	// On OS X GART will unwire any page that hasn't been used <1sec so its important to keep reuse high.
	if(!(FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps)))
	{
		// Emulated heaps can be drained
		for (uint32 i = 0; i < EMetalHeapStorageNum; i++)
		{
			for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
			{
				if (StaticTextureHeaps[i][k])
				{
					FMTLHeap* Heap = (FMTLHeap*)StaticTextureHeaps[i][k];
					[Heap drain:bForce];
				}
			}
		}
	}

	// Real heaps must be released
	{
		for (uint32 i = 0; i < EMetalHeapStorageNum; i++)
		{
			for (uint32 j = 0; j < EMetalHeapBufferSizesNum; j++)
			{
				for(uint32 k = 0; k < BufferHeaps[i][j].Num(); k++)
				{
					if(!(FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps)))
					{
						FMTLHeap* Heap = (FMTLHeap*)BufferHeaps[i][j][k].Key;
						[Heap drain:bForce];
					}
					
					uint64 LastUsedFrame = BufferHeaps[i][j][k].Value;
					uint64 NumFrames = GFrameNumberRenderThread - LastUsedFrame;
					if ((bForce || NumFrames > EMetalHeapConstantsCullAfterFrames || j == EMetalHeapBufferExactSize) && BufferHeaps[i][j][k].Key.usedSize == 0)
					{
						DEC_DWORD_STAT(STAT_MetalHeapNumBufferHeaps);
						
						ReleasedHeaps.Add(BufferHeaps[i][j][k].Key);
						Context.ReleaseHeap(BufferHeaps[i][j][k].Key);
						
						BufferHeaps[i][j].RemoveAt(k);
						k--;
					}
				}
				
				for (uint32 k = 0; k < EMetalHeapTextureUsageNum; k++)
				{
					for(uint32 l = 0; l < DynamicTextureHeaps[i][j][k].Num(); l++)
					{
						if(!(FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps)))
						{
							FMTLHeap* Heap = (FMTLHeap*)DynamicTextureHeaps[i][j][k][l].Key;
							[Heap drain:bForce];
						}
						
						uint64 LastUsedFrame = DynamicTextureHeaps[i][j][k][l].Value;
						uint64 NumFrames = GFrameNumberRenderThread - LastUsedFrame;
						
						// There may be heaps with no resources currently in-use but the memory has yet to be released, we can dispose of these heaps
						TSet<NSObject<MTLTexture>*>& Textures = TextureResources.FindChecked(DynamicTextureHeaps[i][j][k][l].Key);
						if (((bForce || NumFrames > EMetalHeapConstantsCullAfterFrames || j == EMetalHeapBufferExactSize) && Textures.Num() == 0) && (DynamicTextureHeaps[i][j][k][l].Key.usedSize == 0) && (((FMTLHeap*)DynamicTextureHeaps[i][j][k][l].Key).poolSize == 0))
						{
							TotalTextureMemory -= DynamicTextureHeaps[i][j][k][l].Key.size;
							DEC_DWORD_STAT_FName(NumTextureHeapStats[k]);
							
							ReleasedHeaps.Add(DynamicTextureHeaps[i][j][k][l].Key);
							Context.ReleaseHeap(DynamicTextureHeaps[i][j][k][l].Key);

							DynamicTextureHeaps[i][j][k].RemoveAt(l);
							l--;
						}
					}
				}
			}
		}
	}
}

FMetalHeap::EMetalHeapStorage FMetalHeap::ResouceOptionsToStorage(MTLResourceOptions Options)
{
	uint32 Storage = ((Options & MTLResourceStorageModeMask) >> MTLResourceStorageModeShift);
	uint32 Cache = ((Options & MTLResourceCPUCacheModeMask) >> MTLResourceCPUCacheModeShift);

	check(Storage < EMetalHeapTypeNum);
	check(Cache < EMetalHeapCacheNum);
	
	EMetalHeapStorage Type = (EMetalHeapStorage)((Storage * EMetalHeapCacheNum) + Cache);
	return Type;
}

uint32 FMetalHeap::HeapBufferSizes[] = {
		16384, 65536, 262144, 1048576, 4194304, 16777216, 67108864
#if PLATFORM_MAC
		, 134217728
		, 1073741824
#else
		, 268435456
#endif
};
uint32 FMetalHeap::HeapBufferBlock[] = {
		1048576, 2097152, 2097152, 4194304, 8388608 + 4194304, 33554432, 134217728
#if PLATFORM_MAC
		, 134217728
		, 1073741824
#else
		, 268435456
#endif
};

FMetalHeap::EMetalHeapBufferSizes FMetalHeap::BufferSizeToIndex(uint32 Size)
{
	unsigned long Lower = 0;
	unsigned long Upper = EMetalHeapBufferSizesNum;
	unsigned long Middle;
	
	do
	{
		Middle = ( Upper + Lower ) >> 1;
		if( Size <= HeapBufferSizes[Middle-1] )
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while( Upper - Lower > 1 );
	
	check( Size <= HeapBufferSizes[Lower] );
	check( (Lower == 0 ) || ( Size > HeapBufferSizes[Lower-1] ) );
	
	return (EMetalHeapBufferSizes)Lower;
}

FMetalHeap::EMetalHeapTextureUsage FMetalHeap::TextureDescToIndex(MTLTextureDescriptor* Desc)
{
	check(Desc);
	if (!(Desc.usage & (MTLTextureUsageShaderWrite|MTLTextureUsageRenderTarget)))
	{
		return EMetalHeapTextureUsageResource;
	}
	else
	{
		return EMetalHeapTextureUsageRenderTarget;
	}
}
