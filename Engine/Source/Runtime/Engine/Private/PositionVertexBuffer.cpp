// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Rendering/PositionVertexBuffer.h"

#include "CoreMinimal.h"
#include "RHI.h"
#include "Components.h"

#include "StaticMeshVertexData.h"

/*-----------------------------------------------------------------------------
	FPositionVertexBuffer
-----------------------------------------------------------------------------*/

/** The implementation of the static mesh position-only vertex data storage type. */
class FPositionVertexData :
	public TStaticMeshVertexData<FPositionVertex>
{
public:
	FPositionVertexData( bool InNeedsCPUAccess=false )
		: TStaticMeshVertexData<FPositionVertex>( InNeedsCPUAccess )
	{
	}
};


FPositionVertexBuffer::FPositionVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{}

FPositionVertexBuffer::~FPositionVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FPositionVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

/**
 * Initializes the buffer with the given vertices, used to convert legacy layouts.
 * @param InVertices - The vertices to initialize the buffer with.
 */
void FPositionVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices)
{
	NumVertices = InVertices.Num();

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = VertexData->GetDataPointer();

	// Copy the vertices into the buffer.
	for(int32 VertexIndex = 0;VertexIndex < InVertices.Num();VertexIndex++)
	{
		const FStaticMeshBuildVertex& SourceVertex = InVertices[VertexIndex];
		const uint32 DestVertexIndex = VertexIndex;
		VertexPosition(DestVertexIndex) = SourceVertex.Position;
	}
}

/**
 * Initializes this vertex buffer with the contents of the given vertex buffer.
 * @param InVertexBuffer - The vertex buffer to initialize from.
 */
void FPositionVertexBuffer::Init(const FPositionVertexBuffer& InVertexBuffer)
{
	NumVertices = InVertexBuffer.GetNumVertices();
	if ( NumVertices )
	{
		AllocateData();
		check( Stride == InVertexBuffer.GetStride() );
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		const uint8* InData = InVertexBuffer.Data;
		FMemory::Memcpy( Data, InData, Stride * NumVertices );
	}
}

void FPositionVertexBuffer::Init(const TArray<FVector>& InPositions)
{
	NumVertices = InPositions.Num();
	if ( NumVertices )
	{
		AllocateData();
		check( Stride == InPositions.GetTypeSize() );
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		FMemory::Memcpy( Data, InPositions.GetData(), Stride * NumVertices );
	}
}

/**
* Removes the cloned vertices used for extruding shadow volumes.
* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
*/
void FPositionVertexBuffer::RemoveLegacyShadowVolumeVertices(uint32 InNumVertices)
{
	check(VertexData);
	VertexData->ResizeBuffer(InNumVertices);
	NumVertices = InNumVertices;

	// Make a copy of the vertex data pointer.
	Data = VertexData->GetDataPointer();
}

/**
* Serializer
*
* @param	Ar				Archive to serialize with
* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
*/
void FPositionVertexBuffer::Serialize( FArchive& Ar, bool bNeedsCPUAccess )
{
	Ar << Stride << NumVertices;

	if(Ar.IsLoading())
	{
		// Allocate the vertex data storage type.
		AllocateData( bNeedsCPUAccess );
	}

	if(VertexData != NULL)
	{
		// Serialize the vertex data.
		VertexData->Serialize(Ar);

		// Make a copy of the vertex data pointer.
		Data = VertexData->GetDataPointer();
	}
}

/**
* Specialized assignment operator, only used when importing LOD's.  
*/
void FPositionVertexBuffer::operator=(const FPositionVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	VertexData = NULL;
}

void FPositionVertexBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if(ResourceArray->GetResourceDataSize())
	{
		// Create the vertex buffer.
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(),BUF_Static,CreateInfo);
	}
}

void FPositionVertexBuffer::AllocateData( bool bNeedsCPUAccess /*= true*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FPositionVertexData(bNeedsCPUAccess);
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

