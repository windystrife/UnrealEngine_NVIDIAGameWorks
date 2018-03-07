// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Rendering/SkinWeightVertexBuffer.h"
#include "SkeletalMeshTypes.h"
#include "EngineUtils.h"

///

FSkinWeightVertexBuffer::FSkinWeightVertexBuffer()
:	bNeedsCPUAccess(false)
,	bExtraBoneInfluences(false)
,	WeightData(nullptr)
,	Data(nullptr)
,	Stride(0)
,	NumVertices(0)
{
}

FSkinWeightVertexBuffer::FSkinWeightVertexBuffer( const FSkinWeightVertexBuffer &Other )
	: bNeedsCPUAccess(Other.bNeedsCPUAccess)
	, bExtraBoneInfluences(Other.bExtraBoneInfluences)
	, WeightData(nullptr)
	, Data(nullptr)
	, Stride(0)
	, NumVertices(0)
{
	
}

FSkinWeightVertexBuffer::~FSkinWeightVertexBuffer()
{
	CleanUp();
}

FSkinWeightVertexBuffer& FSkinWeightVertexBuffer::operator=(const FSkinWeightVertexBuffer& Other)
{
	CleanUp();
	bNeedsCPUAccess = Other.bNeedsCPUAccess;
	bExtraBoneInfluences = Other.bExtraBoneInfluences;
	return *this;
}

void FSkinWeightVertexBuffer::CleanUp()
{
	if (WeightData)
	{
		delete WeightData;
		WeightData = NULL;
	}
}

bool FSkinWeightVertexBuffer::IsWeightDataValid() const
{
	return WeightData != NULL;
}

void FSkinWeightVertexBuffer::Init(const TArray<FSoftSkinVertex>& InVertices)
{
	// Make sure if this is console, use compressed otherwise, use not compressed
	AllocateData();

	WeightData->ResizeBuffer(InVertices.Num());

	if (InVertices.Num() > 0)
	{
		Data = WeightData->GetDataPointer();
		Stride = WeightData->GetStride();
		NumVertices = InVertices.Num();
	}

	if (bExtraBoneInfluences)
	{
		for (int32 VertIdx = 0; VertIdx < InVertices.Num(); VertIdx++)
		{
			const FSoftSkinVertex& SrcVertex = InVertices[VertIdx];
			SetWeightsForVertex<true>(VertIdx, SrcVertex);
		}
	}
	else
	{
		for (int32 VertIdx = 0; VertIdx < InVertices.Num(); VertIdx++)
		{
			const FSoftSkinVertex& SrcVertex = InVertices[VertIdx];
			SetWeightsForVertex<false>(VertIdx, SrcVertex);
		}
	}
}

FArchive& operator<<(FArchive& Ar, FSkinWeightVertexBuffer& VertexBuffer)
{
	FStripDataFlags StripFlags(Ar);

	Ar << VertexBuffer.bExtraBoneInfluences;
	Ar << VertexBuffer.NumVertices;

	if (Ar.IsLoading() || VertexBuffer.WeightData == NULL)
	{
		// If we're loading, or we have no valid buffer, allocate container.
		VertexBuffer.AllocateData();
	}

	// if Ar is counting, it still should serialize. Need to count VertexData
	if (!StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory())
	{
		if (VertexBuffer.WeightData != NULL)
		{
			VertexBuffer.WeightData->Serialize(Ar);

			if (!Ar.IsCountingMemory())
			{
				// update cached buffer info
				VertexBuffer.Data = (VertexBuffer.NumVertices > 0 && VertexBuffer.WeightData->GetResourceArray()->GetResourceDataSize()) ? VertexBuffer.WeightData->GetDataPointer() : nullptr;
				VertexBuffer.Stride = VertexBuffer.WeightData->GetStride();
			}
		}
	}

	return Ar;
}

void FSkinWeightVertexBuffer::InitRHI()
{
	check(WeightData);
	FResourceArrayInterface* ResourceArray = WeightData->GetResourceArray();
	if (ResourceArray->GetResourceDataSize() > 0)
	{
		// Create the vertex buffer.
		FRHIResourceCreateInfo CreateInfo(ResourceArray);

		// BUF_ShaderResource is needed for support of the SkinCache (we could make is dependent on GEnableGPUSkinCacheShaders or are there other users?)
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
		SRVValue = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_UINT);
	}
}

void FSkinWeightVertexBuffer::ReleaseRHI()
{
	FVertexBuffer::ReleaseRHI();

	SRVValue.SafeRelease();
}




void FSkinWeightVertexBuffer::AllocateData()
{
	// Clear any old WeightData before allocating.
	CleanUp();

	if (bExtraBoneInfluences)
	{
		WeightData = new FSkinWeightVertexData< TSkinWeightInfo<true> >(bNeedsCPUAccess);
	}
	else
	{
		WeightData = new FSkinWeightVertexData< TSkinWeightInfo<false> >(bNeedsCPUAccess);
	}
}

template <bool bExtraBoneInfluencesT>
void FSkinWeightVertexBuffer::SetWeightsForVertex(uint32 VertexIndex, const FSoftSkinVertex& SrcVertex)
{
	checkSlow(VertexIndex < GetNumVertices());
	auto* VertBase = (TSkinWeightInfo<bExtraBoneInfluencesT>*)(Data + VertexIndex * Stride);
	FMemory::Memcpy(VertBase->InfluenceBones, SrcVertex.InfluenceBones, TSkinWeightInfo<bExtraBoneInfluencesT>::NumInfluences);
	FMemory::Memcpy(VertBase->InfluenceWeights, SrcVertex.InfluenceWeights, TSkinWeightInfo<bExtraBoneInfluencesT>::NumInfluences);
}
