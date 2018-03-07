// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"

struct FStaticMeshBuildVertex;

/** A vertex that stores just position. */
struct FPositionVertex
{
	FVector	Position;

	friend FArchive& operator<<(FArchive& Ar, FPositionVertex& V)
	{
		Ar << V.Position;
		return Ar;
	}
};

/** A vertex buffer of positions. */
class FPositionVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	ENGINE_API FPositionVertexBuffer();

	/** Destructor. */
	ENGINE_API ~FPositionVertexBuffer();

	/** Delete existing resources */
	ENGINE_API void CleanUp();

	/**
	* Initializes the buffer with the given vertices, used to convert legacy layouts.
	* @param InVertices - The vertices to initialize the buffer with.
	*/
	ENGINE_API void Init(const TArray<FStaticMeshBuildVertex>& InVertices);

	/**
	* Initializes this vertex buffer with the contents of the given vertex buffer.
	* @param InVertexBuffer - The vertex buffer to initialize from.
	*/
	void Init(const FPositionVertexBuffer& InVertexBuffer);

	ENGINE_API void Init(const TArray<FVector>& InPositions);

	/**
	* Removes the cloned vertices used for extruding shadow volumes.
	* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
	*/
	void RemoveLegacyShadowVolumeVertices(uint32 InNumVertices);

	/**
	* Serializer
	*
	* @param	Ar				Archive to serialize with
	* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
	*/
	void Serialize(FArchive& Ar, bool bNeedsCPUAccess);

	/**
	* Specialized assignment operator, only used when importing LOD's.
	*/
	ENGINE_API void operator=(const FPositionVertexBuffer &Other);

	// Vertex data accessors.
	FORCEINLINE FVector& VertexPosition(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FPositionVertex*)(Data + VertexIndex * Stride))->Position;
	}
	FORCEINLINE const FVector& VertexPosition(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return ((FPositionVertex*)(Data + VertexIndex * Stride))->Position;
	}
	// Other accessors.
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("PositionOnly Static-mesh vertices"); }

private:

	/** The vertex data storage type */
	class FPositionVertexData* VertexData;

	/** The cached vertex data pointer. */
	uint8* Data;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of vertices. */
	uint32 NumVertices;

	/** Allocates the vertex data storage type. */
	void AllocateData(bool bNeedsCPUAccess = true);
};