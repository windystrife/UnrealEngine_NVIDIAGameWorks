// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

class FResourceArrayInterface;

/** An interface to the static-mesh vertex data storage type. */
class FStaticMeshVertexDataInterface
{
public:

	/** Virtual destructor. */
	virtual ~FStaticMeshVertexDataInterface() {}

	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(uint32 NumVertices) = 0;

	/** @return The stride of the vertex data in the buffer. */
	virtual uint32 GetStride() const = 0;

	/** @return A pointer to the data in the buffer. */
	virtual uint8* GetDataPointer() = 0;

	/** @return A pointer to the FResourceArrayInterface for the vertex data. */
	virtual FResourceArrayInterface* GetResourceArray() = 0;

	/** Serializer. */
	virtual void Serialize(FArchive& Ar) = 0;
};