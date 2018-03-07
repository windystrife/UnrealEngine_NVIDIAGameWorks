// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "StaticMeshVertexData.h"
#include "GPUSkinPublicDefs.h"

struct FSoftSkinVertex;
class FStaticMeshVertexDataInterface;

/** Struct for storing skin weight info in vertex buffer */
template <bool bExtraBoneInfluences>
struct TSkinWeightInfo
{
	enum
	{
		NumInfluences = bExtraBoneInfluences ? MAX_TOTAL_INFLUENCES : MAX_INFLUENCES_PER_STREAM,
	};
	uint8	InfluenceBones[NumInfluences];
	uint8	InfluenceWeights[NumInfluences];

	friend FArchive& operator<<(FArchive& Ar, TSkinWeightInfo& I)
	{
		// serialize bone and weight uint8 arrays in order
		// this is required when serializing as bulk data memory (see TArray::BulkSerialize notes)
		for (uint32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; InfluenceIndex++)
		{
			Ar << I.InfluenceBones[InfluenceIndex];
		}

		for (uint32 InfluenceIndex = 0; InfluenceIndex < NumInfluences; InfluenceIndex++)
		{
			Ar << I.InfluenceWeights[InfluenceIndex];
		}

		return Ar;
	}
};

/** The implementation of the skin weight vertex data storage type. */
template<typename VertexDataType>
class FSkinWeightVertexData : public TStaticMeshVertexData<VertexDataType>
{
public:
	FSkinWeightVertexData(bool InNeedsCPUAccess = false)
		: TStaticMeshVertexData<VertexDataType>(InNeedsCPUAccess)
	{
	}

	TStaticMeshVertexData<VertexDataType>& operator=(const TArray<VertexDataType>& Other)
	{
		TStaticMeshVertexData<VertexDataType>::operator=(Other);
		return *this;
	}
};

/** A vertex buffer for skin weights. */
class FSkinWeightVertexBuffer : public FVertexBuffer
{
public:
	/** Default constructor. */
	ENGINE_API FSkinWeightVertexBuffer();

	/** Constructor (copy) */
	ENGINE_API FSkinWeightVertexBuffer(const FSkinWeightVertexBuffer& Other);

	/** Destructor. */
	ENGINE_API ~FSkinWeightVertexBuffer();

	/** Assignment. Assumes that vertex buffer will be rebuilt */
	ENGINE_API FSkinWeightVertexBuffer& operator=(const FSkinWeightVertexBuffer& Other);

	/** Delete existing resources */
	ENGINE_API void CleanUp();

	/** @return true is WeightData is valid */
	bool IsWeightDataValid() const;

	/** Init from another skin weight buffer */
	ENGINE_API void Init(const TArray<FSoftSkinVertex>& InVertices);

	friend FArchive& operator<<(FArchive& Ar, FSkinWeightVertexBuffer& VertexBuffer);

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;

	virtual FString GetFriendlyName() const override 
	{ 
		return TEXT("SkeletalMesh Vertex Weights"); 
	}

	/** Const access to entry in weight data */
	template <bool bExtraBoneInfluencesT>
	FORCEINLINE const TSkinWeightInfo<bExtraBoneInfluencesT>* GetSkinWeightPtr(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return (TSkinWeightInfo<bExtraBoneInfluencesT>*)(Data + VertexIndex * Stride);
	}

	/** Non-const access to entry in weight data */
	template <bool bExtraBoneInfluencesT>
	FORCEINLINE TSkinWeightInfo<bExtraBoneInfluencesT>* GetSkinWeightPtr(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return (TSkinWeightInfo<bExtraBoneInfluencesT>*)(Data + VertexIndex * Stride);
	}

	/** @return number of vertices in this vertex buffer */
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	/** @return cached stride for vertex data type for this vertex buffer */
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}

	/** @return total size of data in resource array */
	FORCEINLINE uint32 GetVertexDataSize() const
	{
		return NumVertices * Stride;
	}

	// @param guaranteed only to be valid if the vertex buffer is valid
	FShaderResourceViewRHIParamRef GetSRV() const
	{
		return SRVValue;
	}

	/** Set if the CPU needs access to this vertex buffer */
	void SetNeedsCPUAccess(bool bInNeedsCPUAccess)
	{
		bNeedsCPUAccess = bInNeedsCPUAccess;
	}

	bool GetNeedsCPUAccess() const
	{
		return bNeedsCPUAccess;
	}

	/** Set if this will have extra streams for bone indices & weights. */
	void SetHasExtraBoneInfluences(bool bInHasExtraBoneInfluences)
	{
		bExtraBoneInfluences = bInHasExtraBoneInfluences;
	}

	bool HasExtraBoneInfluences() const
	{
		return bExtraBoneInfluences;
	}

	/** Assignment operator for assigning array of weights to this buffer */
	template <bool bExtraBoneInfluencesT>
	FSkinWeightVertexBuffer& operator=(const TArray< TSkinWeightInfo<bExtraBoneInfluencesT> >& InWeights)
	{
		check(bExtraBoneInfluences == bExtraBoneInfluencesT);
		AllocateData();

		*(FSkinWeightVertexData< TSkinWeightInfo<bExtraBoneInfluencesT> >*)WeightData = InWeights;

		Data = WeightData->GetDataPointer();
		Stride = WeightData->GetStride();
		NumVertices = InWeights.Num();

		return *this;
	}

protected:
	// guaranteed only to be valid if the vertex buffer is valid
	FShaderResourceViewRHIRef SRVValue;

private:

	/** true if this vertex buffer will be used with CPU skinning. Resource arrays are set to cpu accessible if this is true */
	bool bNeedsCPUAccess;

	/** Has extra bone influences per Vertex, which means using a different TGPUSkinVertexBase */
	bool bExtraBoneInfluences;

	/** The vertex data storage type */
	FStaticMeshVertexDataInterface* WeightData;

	/** The cached vertex data pointer. */
	uint8* Data;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of vertices. */
	uint32 NumVertices;

	/** Allocates the vertex data storage type. */
	void AllocateData();

	/** Helper for concrete types */
	template <bool bUsesExtraBoneInfluences>
	void SetWeightsForVertex(uint32 VertexIndex, const FSoftSkinVertex& SrcVertex);
};