// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/**
 * An element type independent interface to the resource array.
 */
class FResourceArrayInterface
{
public:

	/**
	 * @return A pointer to the resource data.
	 */
	virtual const void* GetResourceData() const = 0;

	/**
	 * @return size of resource data allocation
	 */
	virtual uint32 GetResourceDataSize() const = 0;

	/** Called on non-UMA systems after the RHI has copied the resource data, and no longer needs the CPU's copy. */
	virtual void Discard() = 0;

	/**
	 * @return true if the resource array is static and shouldn't be modified
	 */
	virtual bool IsStatic() const = 0;

	/**
	 * @return true if the resource keeps a copy of its resource data after the RHI resource has been created
	 */
	virtual bool GetAllowCPUAccess() const = 0;

	/** 
	 * Sets whether the resource array will be accessed by CPU. 
	 */
	virtual void SetAllowCPUAccess( bool bInNeedsCPUAccess ) = 0;
};


/**
 * Allows for direct GPU mem allocation for bulk resource types.
 */
class FResourceBulkDataInterface
{
public:

	virtual ~FResourceBulkDataInterface() {}

	/** 
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual const void* GetResourceBulkData() const = 0;

	/** 
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const = 0;

	/**
	 * Free memory after it has been used to initialize RHI resource 
	 */
	virtual void Discard() = 0;
	
	enum class EBulkDataType
	{
		Default,
		MediaTexture,
		VREyeBuffer,
	};
	
	/**
	 * @return the type of bulk data for special handling
	 */
	virtual EBulkDataType GetResourceType() const
	{
		return EBulkDataType::Default;
	}
};


/**
 * Allows for direct GPU mem allocation for texture resource.
 */
class FTexture2DResourceMem : public FResourceBulkDataInterface
{
public:

	/**
	 * @param MipIdx index for mip to retrieve
	 * @return ptr to the offset in bulk memory for the given mip
	 */
	virtual void* GetMipData(int32 MipIdx) = 0;

	/**
	 * @return total number of mips stored in this resource
	 */
	virtual int32	GetNumMips() = 0;

	/** 
	 * @return width of texture stored in this resource
	 */
	virtual int32 GetSizeX() = 0;

	/** 
	 * @return height of texture stored in this resource
	 */
	virtual int32 GetSizeY() = 0;

	/**
	 * @return Whether the resource memory is properly allocated or not.
	 */
	virtual bool IsValid() = 0;

	/**
	 * @return Whether the resource memory has an async allocation request and it's been completed.
	 */
	virtual bool HasAsyncAllocationCompleted() const = 0;

	/** Blocks the calling thread until the allocation has been completed. */
	virtual void FinishAsyncAllocation() = 0;

	/** Cancels any async allocation. */
	virtual void CancelAsyncAllocation() = 0;

	virtual ~FTexture2DResourceMem() {}
};

