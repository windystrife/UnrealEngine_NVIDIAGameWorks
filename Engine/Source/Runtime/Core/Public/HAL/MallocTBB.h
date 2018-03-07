// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformMemory.h"
#include "HAL/MemoryBase.h"

/**
 * TBB 64-bit scalable memory allocator.
 */
class FMallocTBB
	: public FMalloc
{
public:
	
	/**
	 * Default constructor.
	 */
	FMallocTBB() :
		MemTime(0.0)
	{ }

public:

	// FMalloc interface.
	
	virtual void* Malloc( SIZE_T Size, uint32 Alignment ) override;
	virtual void* Realloc( void* Ptr, SIZE_T NewSize, uint32 Alignment ) override;
	virtual void Free( void* Ptr ) override;
	virtual bool GetAllocationSize( void *Original, SIZE_T &SizeOut ) override;

	virtual bool IsInternallyThreadSafe( ) const override
	{ 
		return true;
	}

	virtual const TCHAR* GetDescriptiveName( ) override
	{
		return TEXT("TBB");
	}

protected:

	void OutOfMemory( uint64 Size, uint32 Alignment )
	{
		// this is expected not to return
		FPlatformMemory::OnOutOfMemory(Size, Alignment);
	}

private:

	double MemTime;
};
