// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12ConstantBuffer.h: Public D3D Constant Buffer definitions.
	=============================================================================*/

#pragma once

/** Size of the default constant buffer. */
#define MAX_GLOBAL_CONSTANT_BUFFER_SIZE		4096

using namespace D3D12RHI;

class FD3D12FastConstantAllocator;

/**
 * A D3D constant buffer
 */
class FD3D12ConstantBuffer : public FD3D12DeviceChild
{
public:
#if USE_STATIC_ROOT_SIGNATURE
	FD3D12ConstantBufferView* View;
#endif
	FD3D12ConstantBuffer(FD3D12Device* InParent, FD3D12FastConstantAllocator& InAllocator);
	virtual ~FD3D12ConstantBuffer();

	/**
	* Updates a variable in the constant buffer.
	* @param Data - The data to copy into the constant buffer
	* @param Offset - The offset in the constant buffer to place the data at
	* @param InSize - The size of the data being copied
	*/
	FORCEINLINE void UpdateConstant(const uint8* Data, uint16 Offset, uint16 InSize)
	{
		// Check that the data we are shadowing fits in the allocated shadow data
		check((uint32)Offset + (uint32)InSize <= MAX_GLOBAL_CONSTANT_BUFFER_SIZE);
		FMemory::Memcpy(ShadowData + Offset, Data, InSize);
		CurrentUpdateSize = FMath::Max((uint32)(Offset + InSize), CurrentUpdateSize);

		bIsDirty = true;
	}

	FORCEINLINE void Reset() { CurrentUpdateSize = 0; }

	bool Version(FD3D12ResourceLocation& BufferOut, bool bDiscardSharedConstants);

protected:
	__declspec(align(16)) uint8 ShadowData[MAX_GLOBAL_CONSTANT_BUFFER_SIZE];
	
	/** Size of all constants that has been updated since the last call to Commit. */
	uint32	CurrentUpdateSize;

	/**
	 * Size of all constants that has been updated since the last Discard.
	 * Includes "shared" constants that don't necessarily gets updated between every Commit.
	 */
	uint32	TotalUpdateSize;
	
	// Indicates that a constant has been updated but this one hasn't been flushed.
	bool bIsDirty;

	FD3D12FastConstantAllocator& Allocator;
};

DECLARE_CYCLE_STAT_EXTERN(TEXT("Global Constant buffer update time"), STAT_D3D12GlobalConstantBufferUpdateTime, STATGROUP_D3D12RHI, );

