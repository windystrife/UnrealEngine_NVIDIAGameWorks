// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	ApplePlatformMemory.h: Apple platform memory functions common across all Apple OSes
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMemory.h"
#include <libkern/OSAtomic.h>
#include <Foundation/NSObject.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * NSObject subclass that can be used to override the allocation functions to go through UE4's memory allocator.
 * This ensures that memory allocated by custom Objective-C types can be tracked by UE4's tools and 
 * that we benefit from the memory allocator's efficiencies.
 */
@interface FApplePlatformObject : NSObject
{
@private
	OSQueueHead* AllocatorPtr;
}

/** Sub-classes should override to provide the OSQueueHead* necessary to allocate from - handled by the macro */
+ (nullable OSQueueHead*)classAllocator;

/** Sub-classes should override allocWithZone & alloc to call allocClass */
+ (id)allocClass: (Class)NewClass;

/** Override the core NSObject deallocation function to correctly destruct */
- (void)dealloc;

@end

#define APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(ClassName)		\
+ (nullable OSQueueHead*)classAllocator							\
{																\
	static OSQueueHead Queue = OS_ATOMIC_QUEUE_INIT;			\
	return &Queue;												\
}																\
+ (id)allocWithZone:(NSZone*) Zone								\
{																\
	return (ClassName*)[FApplePlatformObject allocClass:self];	\
}																\
+ (id)alloc														\
{																\
	return (ClassName*)[FApplePlatformObject allocClass:self];	\
}

/**
 *	Max implementation of the FGenericPlatformMemoryStats.
 */
struct FPlatformMemoryStats : public FGenericPlatformMemoryStats
{};

/**
 * Common Apple platform memory functions.
 */
struct CORE_API FApplePlatformMemory : public FGenericPlatformMemory
{
	//~ Begin FGenericPlatformMemory Interface
	static void Init();
	static FPlatformMemoryStats GetStats();
	static const FPlatformMemoryConstants& GetConstants();
	static FMalloc* BaseAllocator();
	static bool PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite);
	static void* BinnedAllocFromOS( SIZE_T Size );
	static void BinnedFreeToOS( void* Ptr, SIZE_T Size );
	//~ End FGenericPlatformMemory Interface
	
	/** Setup the current default CFAllocator to use our malloc functions. */
	static void ConfigureDefaultCFAllocator(void);
};

NS_ASSUME_NONNULL_END
