// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Rendering/StaticMeshVertexBuffer.h"
#include "EngineUtils.h"


FStaticMeshVertexBuffer::FStaticMeshVertexBuffer() : 
	VertexData(NULL),
	NumTexCoords(0),
	Data(NULL),
	Stride(0),
	NumVertices(0),
	bUseFullPrecisionUVs(false),
	bUseHighPrecisionTangentBasis(false)
{}

FStaticMeshVertexBuffer::~FStaticMeshVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FStaticMeshVertexBuffer::CleanUp()
{
	delete VertexData;
	VertexData = NULL;
}

/**
* Initializes the buffer with the given vertices.
* @param InVertices - The vertices to initialize the buffer with.
* @param InNumTexCoords - The number of texture coordinate to store in the buffer.
*/
void FStaticMeshVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices, uint32 InNumTexCoords)
{
	NumTexCoords = InNumTexCoords;
	NumVertices = InVertices.Num();

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = VertexData->GetDataPointer();

	// Copy the vertices into the buffer.
	for (int32 VertexIndex = 0; VertexIndex < InVertices.Num(); VertexIndex++)
	{
		const FStaticMeshBuildVertex& SourceVertex = InVertices[VertexIndex];
		const uint32 DestVertexIndex = VertexIndex;
		SetVertexTangents(DestVertexIndex, SourceVertex.TangentX, SourceVertex.TangentY, SourceVertex.TangentZ);

		for (uint32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++)
		{
			SetVertexUV(DestVertexIndex, UVIndex, SourceVertex.UVs[UVIndex]);
		}
	}
}

/**
* Initializes this vertex buffer with the contents of the given vertex buffer.
* @param InVertexBuffer - The vertex buffer to initialize from.
*/
void FStaticMeshVertexBuffer::Init(const FStaticMeshVertexBuffer& InVertexBuffer)
{
	NumTexCoords = InVertexBuffer.GetNumTexCoords();
	NumVertices = InVertexBuffer.GetNumVertices();
	bUseFullPrecisionUVs = InVertexBuffer.GetUseFullPrecisionUVs();
	bUseHighPrecisionTangentBasis = InVertexBuffer.GetUseHighPrecisionTangentBasis();

	if (NumVertices)
	{
		AllocateData();
		check(GetStride() == InVertexBuffer.GetStride());
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		const uint8* InData = InVertexBuffer.GetRawVertexData();
		FMemory::Memcpy(Data, InData, Stride * NumVertices);
	}
}

/**
* Removes the cloned vertices used for extruding shadow volumes.
* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
*/
void FStaticMeshVertexBuffer::RemoveLegacyShadowVolumeVertices(uint32 InNumVertices)
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
void FStaticMeshVertexBuffer::Serialize(FArchive& Ar, bool bNeedsCPUAccess)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FStaticMeshVertexBuffer::Serialize"), STAT_StaticMeshVertexBuffer_Serialize, STATGROUP_LoadTime);

	FStripDataFlags StripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);

	Ar << NumTexCoords << Stride << NumVertices;
	Ar << bUseFullPrecisionUVs;
	Ar << bUseHighPrecisionTangentBasis;

	if (Ar.IsLoading())
	{
		// Allocate the vertex data storage type.
		AllocateData(bNeedsCPUAccess);
	}

	if (!StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory())
	{
		if (VertexData != NULL)
		{
			// Serialize the vertex data.
			VertexData->Serialize(Ar);

			// Make a copy of the vertex data pointer.
			Data = VertexData->GetDataPointer();
		}
	}
}


/**
* Specialized assignment operator, only used when importing LOD's.
*/
void FStaticMeshVertexBuffer::operator=(const FStaticMeshVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	VertexData = NULL;
	bUseFullPrecisionUVs = Other.bUseFullPrecisionUVs;
	bUseHighPrecisionTangentBasis = Other.bUseHighPrecisionTangentBasis;
}

void FStaticMeshVertexBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if (ResourceArray->GetResourceDataSize())
	{
		// Create the vertex buffer.
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(), BUF_Static, CreateInfo);
	}
}

void FStaticMeshVertexBuffer::AllocateData(bool bNeedsCPUAccess /*= true*/)
{
	// Clear any old VertexData before allocating.
	CleanUp();

	SELECT_STATIC_MESH_VERTEX_TYPE(
		GetUseHighPrecisionTangentBasis(),
		GetUseFullPrecisionUVs(),
		GetNumTexCoords(),
		VertexData = new TStaticMeshVertexData<VertexType>(bNeedsCPUAccess);
	);

	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}