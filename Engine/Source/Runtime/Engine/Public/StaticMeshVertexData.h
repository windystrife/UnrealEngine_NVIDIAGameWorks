// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "Rendering/StaticMeshVertexDataInterface.h"

/** The implementation of the static mesh vertex data storage type. */
template<typename VertexDataType>
class TStaticMeshVertexData :
	public FStaticMeshVertexDataInterface,
	public TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>
{
public:

	typedef TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT> ArrayType;

	/**
	* Constructor
	* @param InNeedsCPUAccess - true if resource array data should be CPU accessible
	*/
	TStaticMeshVertexData(bool InNeedsCPUAccess=false)
		:	TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>(InNeedsCPUAccess)
	{
	}

	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	*
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(uint32 NumVertices)
	{
		if((uint32)ArrayType::Num() < NumVertices)
		{
			// Enlarge the array.
			ArrayType::AddUninitialized(NumVertices - ArrayType::Num());
		}
		else if((uint32)ArrayType::Num() > NumVertices)
		{
			// Shrink the array.
			ArrayType::RemoveAt(NumVertices,ArrayType::Num() - NumVertices);
		}
	}
	/**
	* @return stride of the vertex type stored in the resource data array
	*/
	virtual uint32 GetStride() const
	{
		return sizeof(VertexDataType);
	}
	/**
	* @return uint8 pointer to the resource data array
	*/
	virtual uint8* GetDataPointer()
	{
		return (uint8*)&(*this)[0];
	}
	/**
	* @return resource array interface access
	*/
	virtual FResourceArrayInterface* GetResourceArray()
	{
		return this;
	}
	/**
	* Serializer for this class
	*
	* @param Ar - archive to serialize to
	* @param B - data to serialize
	*/
	virtual void Serialize(FArchive& Ar)
	{
		TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>::BulkSerialize(Ar);
	}
	/**
	* Assignment operator. This is currently the only method which allows for 
	* modifying an existing resource array
	*/
	TStaticMeshVertexData<VertexDataType>& operator=(const TArray<VertexDataType>& Other)
	{
		TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>::operator=(TArray<VertexDataType,TAlignedHeapAllocator<VERTEXBUFFER_ALIGNMENT> >(Other));
		return *this;
	}
};
