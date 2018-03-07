// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"

struct FStaticMeshBuildVertex;

/**
* A vertex buffer of colors.
*/
class FColorVertexBuffer : public FVertexBuffer
{
public:
	/** Default constructor. */
	ENGINE_API FColorVertexBuffer();

	/** Destructor. */
	ENGINE_API ~FColorVertexBuffer();

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
	void Init(const FColorVertexBuffer& InVertexBuffer);

	/**
	* Removes the cloned vertices used for extruding shadow volumes.
	* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
	*/
	void RemoveLegacyShadowVolumeVertices(uint32 InNumVertices);

	/**
	* Serializer
	*
	* @param	Ar					Archive to serialize with
	* @param	bInNeedsCPUAccess	Whether the elements need to be accessed by the CPU
	*/
	void Serialize(FArchive& Ar, bool bNeedsCPUAccess);

	/**
	* Export the data to a string, used for editor Copy&Paste.
	* The method must not be called if there is no data.
	*/
	void ExportText(FString &ValueStr) const;

	/**
	* Export the data from a string, used for editor Copy&Paste.
	* @param SourceText - must not be 0
	*/
	void ImportText(const TCHAR* SourceText);

	/**
	* Specialized assignment operator, only used when importing LOD's.
	*/
	ENGINE_API void operator=(const FColorVertexBuffer &Other);

	FORCEINLINE FColor& VertexColor(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *(FColor*)(Data + VertexIndex * Stride);
	}

	FORCEINLINE const FColor& VertexColor(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *(FColor*)(Data + VertexIndex * Stride);
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

	/** Useful for memory profiling. */
	ENGINE_API uint32 GetAllocatedSize() const;

	/**
	* Gets all vertex colors in the buffer
	*
	* @param OutColors	The populated list of colors
	*/
	ENGINE_API void GetVertexColors(TArray<FColor>& OutColors) const;

	/**
	* Load from a array of colors
	* @param InColors - must not be 0
	* @param Count - must be > 0
	* @param Stride - in bytes, usually sizeof(FColor) but can be 0 to use a single input color or larger.
	*/
	ENGINE_API void InitFromColorArray(const FColor *InColors, uint32 Count, uint32 Stride = sizeof(FColor));

	/**
	* Load from raw color array.
	* @param InColors - InColors must not be empty
	*/
	void InitFromColorArray(const TArray<FColor> &InColors)
	{
		InitFromColorArray(InColors.GetData(), InColors.Num());
	}

	/**
	* Load from single color.
	* @param Count - must be > 0
	*/
	void InitFromSingleColor(const FColor &InColor, uint32 Count)
	{
		InitFromColorArray(&InColor, Count, 0);
	}

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("ColorOnly Mesh Vertices"); }

private:

	/** The vertex data storage type */
	class FColorVertexData* VertexData;

	/** The cached vertex data pointer. */
	uint8* Data;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of vertices. */
	uint32 NumVertices;

	/** Allocates the vertex data storage type. */
	void AllocateData(bool bNeedsCPUAccess = true);

	/** Purposely hidden */
	ENGINE_API FColorVertexBuffer(const FColorVertexBuffer &rhs);

	friend class FStaticLODModel;
};