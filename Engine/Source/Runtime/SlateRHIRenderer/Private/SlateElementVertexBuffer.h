// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Rendering/RenderingCommon.h"
#include "SlateGlobals.h"
#include "RHI.h"
#include "RenderResource.h"

DECLARE_MEMORY_STAT_EXTERN(TEXT("Vertex Buffer Memory (GPU)"), STAT_SlateVertexBufferMemory, STATGROUP_SlateMemory, );

/**
 * Vertex buffer containing all Slate vertices
 */
template <typename VertexType>
class TSlateElementVertexBuffer : public FVertexBuffer
{
public:
	TSlateElementVertexBuffer()
		: BufferSize(0)
		, MinBufferSize(0)
		, BufferUsageSize(0)
	{}

	~TSlateElementVertexBuffer() {};

	void Init( int32 MinNumVertices )
	{
		MinBufferSize = sizeof(VertexType) * FMath::Max( MinNumVertices, 100 );

		if ( IsInRenderingThread() )
		{
			InitResource();
		}
		else
		{
			BeginInitResource(this);
		}
	}

	void Destroy()
	{
		if ( IsInRenderingThread() )
		{
			ReleaseResource();
		}
		else
		{
			BeginReleaseResource(this);
		}
	}

	/** Initializes the vertex buffers RHI resource. */
	virtual void InitDynamicRHI()
	{
		if( !IsValidRef(VertexBufferRHI) )
		{
			check( MinBufferSize > 0 )
	
			SetBufferSize(MinBufferSize);

			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer( MinBufferSize, BUF_Dynamic, CreateInfo );

			// Ensure the vertex buffer could be created
			check(IsValidRef(VertexBufferRHI));
		}
	}

	/** Releases the vertex buffers RHI resource. */
	virtual void ReleaseDynamicRHI()
	{
		VertexBufferRHI.SafeRelease();
		SetBufferSize(0);
	}

	/** Returns a friendly name for this buffer. */
	virtual FString GetFriendlyName() const { return TEXT("SlateElementVertices"); }

	/** Returns the size of the buffer in bytes. */
	int32 GetBufferSize() const { return BufferSize; }

	/** Returns the used size of this buffer */
	int32 GetBufferUsageSize() const { return BufferUsageSize; }

	/** Resets the usage of the buffer */
	void ResetBufferUsage() { BufferUsageSize = 0; }

	/** Resizes buffer, accumulates states safely on render thread */
	void PreFillBuffer(int32 RequiredVertexCount, bool bShrinkToMinSize)
	{
		checkSlow(IsInRenderingThread());

		if (RequiredVertexCount > 0 )
		{
#if !SLATE_USE_32BIT_INDICES
			// make sure our index buffer can handle this
			checkf(RequiredVertexCount < 0xFFFF, TEXT("Slate vertex buffer is too large (%d) to work with uint16 indices"), RequiredVertexCount);
#endif
			int32 RequiredBufferSize = RequiredVertexCount*sizeof(VertexType);

			// resize if needed
			if(RequiredBufferSize > GetBufferSize() || bShrinkToMinSize)
			{
				ResizeBuffer(RequiredBufferSize);
			}

			BufferUsageSize = RequiredBufferSize;
		}

	}

	int32 GetMinBufferSize() const { return MinBufferSize; }

	void* LockBuffer_RenderThread(int32 NumVertices)
	{
		uint32 RequiredBufferSize = NumVertices*sizeof(VertexType);	

		return RHILockVertexBuffer( VertexBufferRHI, 0, RequiredBufferSize, RLM_WriteOnly );
	}

	void UnlockBuffer_RenderThread()
	{
		RHIUnlockVertexBuffer( VertexBufferRHI );
	}

private:
	/** Resizes the buffer to the passed in size.  Preserves internal data*/
	void ResizeBuffer( int32 NewSizeBytes )
	{
		checkSlow( IsInRenderingThread() );

		int32 FinalSize = FMath::Max( NewSizeBytes, MinBufferSize );

		if( FinalSize != 0 && FinalSize != BufferSize )
		{
			VertexBufferRHI.SafeRelease();

			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer(FinalSize, BUF_Dynamic, CreateInfo);

			check(IsValidRef(VertexBufferRHI));

			SetBufferSize(FinalSize);
		}
	}

	void SetBufferSize(int32 NewBufferSize)
	{
		DEC_MEMORY_STAT_BY(STAT_SlateVertexBufferMemory, BufferSize);
		BufferSize = NewBufferSize;
		INC_MEMORY_STAT_BY(STAT_SlateVertexBufferMemory, BufferSize);
	}

private:
	/** The size of the buffer in bytes. */
	int32 BufferSize;
	
	/** The minimum size the buffer should always be */
	int32 MinBufferSize;

	/** The size of the used portion of the buffer */
	int32 BufferUsageSize;

	/** Hidden copy methods. */
	TSlateElementVertexBuffer( const TSlateElementVertexBuffer& );
	void operator=(const TSlateElementVertexBuffer& );
};
