// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"

class FSlateElementIndexBuffer : public FIndexBuffer
{
public:
	/**
	 * Constructor
	 * 
	 * @param MinNumIndices	The minimum number of indices this buffer should always support
	 */
	FSlateElementIndexBuffer();
	~FSlateElementIndexBuffer();

	void Init( int32 MinNumIndices );
	void Destroy();

	/** Initializes the index buffers RHI resource. */
	virtual void InitDynamicRHI() override;

	/** Releases the index buffers RHI resource. */
	virtual void ReleaseDynamicRHI() override;

	/** Returns a friendly name for this buffer. */
	virtual FString GetFriendlyName() const override { return TEXT("SlateElementIndices"); }

	/** Returns the size of this buffer */
	int32 GetBufferSize() const { return BufferSize; }

	/** Returns the used size of this buffer */
	int32 GetBufferUsageSize() const { return BufferUsageSize; }

	/** Resets the usage of the buffer */
	void ResetBufferUsage() { BufferUsageSize = 0; }

	/** Resizes buffer, accumulates states safely on render thread */
	void PreFillBuffer(int32 RequiredIndexCount, bool bShrinkToMinSize);

	void* LockBuffer_RenderThread(int32 NumIndices);
	void UnlockBuffer_RenderThread();

	int32 GetMinBufferSize() const { return MinBufferSize; }

private:
	/** Resizes the buffer to the passed in size.  Preserves internal data */
	void ResizeBuffer( int32 NewSizeBytes );

	/** Sets the buffer size variable and updates stats. */
	void SetBufferSize(int32 NewBufferSize);
private:
	/** The current size of the buffer in bytes. */
	int32 BufferSize;
	
	/** The minimum size the buffer should always be */
	int32 MinBufferSize;

	/** The size of the currently used portion of the buffer */
	int32 BufferUsageSize;

	/** Hidden copy methods. */
	FSlateElementIndexBuffer( const FSlateElementIndexBuffer& );
	void operator=(const FSlateElementIndexBuffer& );

};
