// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"

/** Indicates which resource size should be returned. */
namespace EResourceSizeMode
{
	enum Type
	{
		/** Only exclusive resource size */
		Exclusive,
		/** Resource size of the object and all of its references */
		Inclusive,
	};
};

/**
 * Struct used to count up the amount of memory used by a resource.
 * This is typically used for assets via UObject::GetResourceSizeEx(...).
 */
struct FResourceSizeEx
{
public:
	/**
	 * Default constructor. 
	 */
	explicit FResourceSizeEx()
		: ResourceSizeMode(EResourceSizeMode::Exclusive)
		, DedicatedSystemMemoryBytes(0)
		, SharedSystemMemoryBytes(0)
		, DedicatedVideoMemoryBytes(0)
		, SharedVideoMemoryBytes(0)
		, UnknownMemoryBytes(0)
	{
	}

	/**
	 * Construct using a given mode. 
	 */
	explicit FResourceSizeEx(const EResourceSizeMode::Type InResourceSizeMode)
		: ResourceSizeMode(InResourceSizeMode)
		, DedicatedSystemMemoryBytes(0)
		, SharedSystemMemoryBytes(0)
		, DedicatedVideoMemoryBytes(0)
		, SharedVideoMemoryBytes(0)
		, UnknownMemoryBytes(0)
	{
	}

	/**
	 * Construct from known sizes. 
	 */
	FResourceSizeEx(const EResourceSizeMode::Type InResourceSizeMode, const SIZE_T InDedicatedSystemMemoryBytes, const SIZE_T InSharedSystemMemoryBytes, const SIZE_T InDedicatedVideoMemoryBytes, const SIZE_T InSharedVideoMemoryBytes)
		: ResourceSizeMode(InResourceSizeMode)
		, DedicatedSystemMemoryBytes(InDedicatedSystemMemoryBytes)
		, SharedSystemMemoryBytes(InSharedSystemMemoryBytes)
		, DedicatedVideoMemoryBytes(InDedicatedVideoMemoryBytes)
		, SharedVideoMemoryBytes(InSharedVideoMemoryBytes)
		, UnknownMemoryBytes(0)
	{
	}

	/**
	 * Construct from legacy unknown size.
	 * Deliberately explicit to avoid accidental use.
	 */
	FResourceSizeEx(const EResourceSizeMode::Type InResourceSizeMode, const SIZE_T InUnknownMemoryBytes)
		: ResourceSizeMode(InResourceSizeMode)
		, DedicatedSystemMemoryBytes(0)
		, SharedSystemMemoryBytes(0)
		, DedicatedVideoMemoryBytes(0)
		, SharedVideoMemoryBytes(0)
		, UnknownMemoryBytes(InUnknownMemoryBytes)
	{
	}

	/**
	 * Get the type of resource size held in this struct.
	 */
	EResourceSizeMode::Type GetResourceSizeMode() const
	{
		return ResourceSizeMode;
	}

	/**
	 * Add the given number of bytes to the dedicated system memory count.
	 * @see DedicatedSystemMemoryBytes for a description of that memory type.
	 */
	FResourceSizeEx& AddDedicatedSystemMemoryBytes(const SIZE_T InMemoryBytes)
	{
		DedicatedSystemMemoryBytes += InMemoryBytes;
		return *this;
	}

	/**
	 * Get the number of bytes allocated from dedicated system memory.
	 * @see DedicatedSystemMemoryBytes for a description of that memory type.
	 */
	SIZE_T GetDedicatedSystemMemoryBytes() const
	{
		return DedicatedSystemMemoryBytes;
	}

	/**
	 * Add the given number of bytes to the shared system memory count.
	 * @see SharedSystemMemoryBytes for a description of that memory type.
	 */
	FResourceSizeEx& AddSharedSystemMemoryBytes(const SIZE_T InMemoryBytes)
	{
		SharedSystemMemoryBytes += InMemoryBytes;
		return *this;
	}

	/**
	 * Get the number of bytes allocated from shared system memory.
	 * @see SharedSystemMemoryBytes for a description of that memory type.
	 */
	SIZE_T GetSharedSystemMemoryBytes() const
	{
		return SharedSystemMemoryBytes;
	}

	/**
	 * Add the given number of bytes to the dedicated video memory count.
	 * @see DedicatedVideoMemoryBytes for a description of that memory type.
	 */
	FResourceSizeEx& AddDedicatedVideoMemoryBytes(const SIZE_T InMemoryBytes)
	{
		DedicatedVideoMemoryBytes += InMemoryBytes;
		return *this;
	}

	/**
	 * Get the number of bytes allocated from dedicated video memory.
	 * @see DedicatedVideoMemoryBytes for a description of that memory type.
	 */
	SIZE_T GetDedicatedVideoMemoryBytes() const
	{
		return DedicatedVideoMemoryBytes;
	}

	/**
	 * Add the given number of bytes to the shared video memory count.
	 * @see SharedVideoMemoryBytes for a description of that memory type.
	 */
	FResourceSizeEx& AddSharedVideoMemoryBytes(const SIZE_T InMemoryBytes)
	{
		SharedVideoMemoryBytes += InMemoryBytes;
		return *this;
	}

	/**
	 * Get the number of bytes allocated from shared video memory.
	 * @see SharedVideoMemoryBytes for a description of that memory type.
	 */
	SIZE_T GetSharedVideoMemoryBytes() const
	{
		return SharedVideoMemoryBytes;
	}

	/**
	 * Add the given number of bytes to the unknown memory count.
	 * @see UnknownMemoryBytes for a description of that memory type.
	 */
	FResourceSizeEx& AddUnknownMemoryBytes(const SIZE_T InMemoryBytes)
	{
		UnknownMemoryBytes += InMemoryBytes;
		return *this;
	}

	/**
	 * Get the number of bytes allocated from unknown memory.
	 * @see UnknownMemoryBytes for a description of that memory type.
	 */
	SIZE_T GetUnknownMemoryBytes() const
	{
		return UnknownMemoryBytes;
	}

	/**
	 * Get the total number of bytes allocated from any memory.
	 */
	SIZE_T GetTotalMemoryBytes() const
	{
		return DedicatedSystemMemoryBytes + SharedSystemMemoryBytes + DedicatedVideoMemoryBytes + SharedVideoMemoryBytes + UnknownMemoryBytes;
	}

	/**
	 * Add another FResourceSizeEx to this one.
	 */
	FResourceSizeEx& operator+=(const FResourceSizeEx& InRHS)
	{
		ensureAlwaysMsgf(ResourceSizeMode == InRHS.ResourceSizeMode, TEXT("The two resource sizes use different counting modes. The result of adding them together may be incorrect."));

		DedicatedSystemMemoryBytes += InRHS.DedicatedSystemMemoryBytes;
		SharedSystemMemoryBytes += InRHS.SharedSystemMemoryBytes;
		DedicatedVideoMemoryBytes += InRHS.DedicatedVideoMemoryBytes;
		SharedVideoMemoryBytes += InRHS.SharedVideoMemoryBytes;
		UnknownMemoryBytes += InRHS.UnknownMemoryBytes;
		return *this;
	}

	/**
	 * Add two FResourceSizeEx instances together and return a copy.
	 */
	friend FResourceSizeEx operator+(FResourceSizeEx InLHS, const FResourceSizeEx& InRHS)
	{
		InLHS += InRHS;
		return InLHS;
	}

private:
	/**
	 * Type of resource size held in this struct.
	 */
	EResourceSizeMode::Type ResourceSizeMode;

	/**
	 * The number of bytes of memory that this resource is using for CPU resources that have been allocated from dedicated system memory.
	 * On platforms with unified memory, this typically refers to the things allocated in the preferred memory for CPU use.
	 */
	SIZE_T DedicatedSystemMemoryBytes;

	/**
	 * The number of bytes of memory that this resource is using for CPU resources that have been allocated from non-dedicated system memory (typically zero).
	 * On platforms with unified memory, this typically refers to the things allocated in the memory outside the preferred memory for CPU use.
	 */
	SIZE_T SharedSystemMemoryBytes;

	/**
	 * The number of bytes of memory that this resource is using for GPU resources that have been allocated from dedicated video memory.
	 * On platforms with unified memory, this typically refers to the things allocated in the preferred memory for GPU use.
	 */
	SIZE_T DedicatedVideoMemoryBytes;

	/**
	 * The number of bytes of memory that this resource is using for GPU resources that have been allocated from non-dedicated video memory.
	 * On platforms with unified memory, this typically refers to the things allocated in the memory outside the preferred memory for GPU use.
	 */
	SIZE_T SharedVideoMemoryBytes;

	/**
	 * The number of bytes of memory that this resource is using from an unspecified section of memory.
	 * This exists so that the legacy GetResourceSize(...) functions can still report back memory usage until they're updated to use FResourceSizeEx, and should not be used in new memory tracking code.
	 */
	SIZE_T UnknownMemoryBytes;
};
