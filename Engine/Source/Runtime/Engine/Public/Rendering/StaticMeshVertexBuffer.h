// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "RenderUtils.h"
#include "StaticMeshVertexData.h"
#include "PackedNormal.h"
#include "Components.h"

class FStaticMeshVertexDataInterface;

template<typename TangentTypeT>
struct TStaticMeshVertexTangentDatum
{
	TangentTypeT TangentX;
	TangentTypeT TangentZ;

	FORCEINLINE FVector GetTangentX() const
	{
		return TangentX;
	}

	FORCEINLINE FVector4 GetTangentZ() const
	{
		return TangentZ;
	}

	FORCEINLINE FVector GetTangentY() const
	{
		FVector  TanX = GetTangentX();
		FVector4 TanZ = GetTangentZ();

		return (FVector(TanZ) ^ TanX) * TanZ.W;
	}

	FORCEINLINE void SetTangents(FVector X, FVector Y, FVector Z)
	{
		TangentX = X;
		TangentZ = FVector4(Z, GetBasisDeterminantSign(X, Y, Z));
	}
};

template<typename UVTypeT, uint32 NumTexCoords>
struct TStaticMeshVertexUVsDatum
{
	UVTypeT UVs[NumTexCoords];

	FORCEINLINE FVector2D GetUV(uint32 TexCoordIndex) const
	{
		check(TexCoordIndex < NumTexCoords);

		return UVs[TexCoordIndex];
	}

	FORCEINLINE void SetUV(uint32 TexCoordIndex, FVector2D UV)
	{
		check(TexCoordIndex < NumTexCoords);

		UVs[TexCoordIndex] = UV;
	}
};

enum class EStaticMeshVertexTangentBasisType
{
	Default,
	HighPrecision,
};

enum class EStaticMeshVertexUVType
{
	Default,
	HighPrecision,
};

template<EStaticMeshVertexTangentBasisType TangentBasisType>
struct TStaticMeshVertexTangentTypeSelector
{
};

template<>
struct TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>
{
	typedef FPackedNormal TangentTypeT;
	static const EVertexElementType VertexElementType = VET_PackedNormal;
};

template<>
struct TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>
{
	typedef FPackedRGBA16N TangentTypeT;
	static const EVertexElementType VertexElementType = VET_UShort4N;
};

template<EStaticMeshVertexUVType UVType>
struct TStaticMeshVertexUVsTypeSelector
{
};

template<>
struct TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>
{
	typedef FVector2DHalf UVsTypeT;
};

template<>
struct TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>
{
	typedef FVector2D UVsTypeT;
};

#define ConstExprStaticMeshVertexTypeID(bUseHighPrecisionTangentBasis, bUseFullPrecisionUVs, TexCoordsCount) \
	(((((bUseHighPrecisionTangentBasis) ? 1 : 0) * 2) + ((bUseFullPrecisionUVs) ? 1 : 0)) * MAX_STATIC_TEXCOORDS) + ((TexCoordsCount) - 1)

FORCEINLINE uint32 ComputeStaticMeshVertexTypeID(bool bUseHighPrecisionTangentBasis, bool bUseFullPrecisionUVs, uint32 TexCoordsCount)
{
	return ConstExprStaticMeshVertexTypeID(bUseHighPrecisionTangentBasis, bUseFullPrecisionUVs, TexCoordsCount);
}

/**
* All information about a static-mesh vertex with a variable number of texture coordinates.
* Position information is stored separately to reduce vertex fetch bandwidth in passes that only need position. (z prepass)
*/
template<EStaticMeshVertexTangentBasisType TangentBasisTypeT, EStaticMeshVertexUVType UVTypeT, uint32 NumTexCoordsT>
struct TStaticMeshFullVertex :
	public TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<TangentBasisTypeT>::TangentTypeT>,
	public TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<UVTypeT>::UVsTypeT, NumTexCoordsT>
{
	static_assert(NumTexCoordsT > 0, "Must have at least 1 texcoord.");

	static const EStaticMeshVertexTangentBasisType TangentBasisType = TangentBasisTypeT;
	static const EStaticMeshVertexUVType UVType = UVTypeT;
	static const uint32 NumTexCoords = NumTexCoordsT;

	static const uint32 VertexTypeID = ConstExprStaticMeshVertexTypeID(
		TangentBasisTypeT == EStaticMeshVertexTangentBasisType::HighPrecision,
		UVTypeT == EStaticMeshVertexUVType::HighPrecision,
		NumTexCoordsT
		);

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar, TStaticMeshFullVertex& Vertex)
	{
		Ar << Vertex.TangentX;
		Ar << Vertex.TangentZ;

		for (uint32 UVIndex = 0; UVIndex < NumTexCoordsT; UVIndex++)
		{
			Ar << Vertex.UVs[UVIndex];
		}

		return Ar;
	}
};

#define APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, UVCount, ...) \
	{ \
		typedef TStaticMeshFullVertex<TangentBasisType, UVType, UVCount> VertexType; \
		case VertexType::VertexTypeID: { __VA_ARGS__ } break; \
	}

#define SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(TangentBasisType, UVType, ...) \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 1, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 2, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 3, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 4, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 5, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 6, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 7, __VA_ARGS__); \
	APPLY_TO_STATIC_MESH_VERTEX(TangentBasisType, UVType, 8, __VA_ARGS__);

#define SELECT_STATIC_MESH_VERTEX_TYPE(bIsHighPrecisionTangentBais, bIsHigPrecisionUVs, NumTexCoords, ...) \
	{ \
		uint32 VertexTypeID = ComputeStaticMeshVertexTypeID(bIsHighPrecisionTangentBais, bIsHigPrecisionUVs, NumTexCoords); \
		switch (VertexTypeID) \
		{ \
			SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(EStaticMeshVertexTangentBasisType::Default,		  EStaticMeshVertexUVType::Default,		  __VA_ARGS__); \
			SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(EStaticMeshVertexTangentBasisType::Default,		  EStaticMeshVertexUVType::HighPrecision, __VA_ARGS__); \
			SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(EStaticMeshVertexTangentBasisType::HighPrecision,  EStaticMeshVertexUVType::Default,		  __VA_ARGS__); \
			SELECT_STATIC_MESH_VERTEX_TYPE_WITH_TEX_COORDS(EStaticMeshVertexTangentBasisType::HighPrecision,  EStaticMeshVertexUVType::HighPrecision, __VA_ARGS__); \
		}; \
	}

/** Vertex buffer for a static mesh LOD */
class FStaticMeshVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	FStaticMeshVertexBuffer();

	/** Destructor. */
	ENGINE_API ~FStaticMeshVertexBuffer();

	/** Delete existing resources */
	ENGINE_API void CleanUp();

	/**
	* Initializes the buffer with the given vertices.
	* @param InVertices - The vertices to initialize the buffer with.
	* @param InNumTexCoords - The number of texture coordinate to store in the buffer.
	*/
	ENGINE_API void Init(const TArray<FStaticMeshBuildVertex>& InVertices, uint32 InNumTexCoords);

	/**
	* Initializes this vertex buffer with the contents of the given vertex buffer.
	* @param InVertexBuffer - The vertex buffer to initialize from.
	*/
	void Init(const FStaticMeshVertexBuffer& InVertexBuffer);

	/**
	* Removes the cloned vertices used for extruding shadow volumes.
	* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
	*/
	void RemoveLegacyShadowVolumeVertices(uint32 NumVertices);

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
	ENGINE_API void operator=(const FStaticMeshVertexBuffer &Other);

	template<EStaticMeshVertexTangentBasisType TangentBasisTypeT>
	FORCEINLINE FVector4 VertexTangentX_Typed(uint32 VertexIndex)const
	{
		return reinterpret_cast<const TStaticMeshFullVertex<TangentBasisTypeT, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->GetTangentX();
	}

	FORCEINLINE FVector4 VertexTangentX(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());

		if (GetUseHighPrecisionTangentBasis())
		{
			return VertexTangentX_Typed<EStaticMeshVertexTangentBasisType::HighPrecision>(VertexIndex);
		}
		else
		{
			return VertexTangentX_Typed<EStaticMeshVertexTangentBasisType::Default>(VertexIndex);
		}
	}

	template<EStaticMeshVertexTangentBasisType TangentBasisTypeT>
	FORCEINLINE FVector4 VertexTangentZ_Typed(uint32 VertexIndex)const
	{
		return reinterpret_cast<const TStaticMeshFullVertex<TangentBasisTypeT, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->GetTangentZ();
	}

	FORCEINLINE FVector VertexTangentZ(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());

		if (GetUseHighPrecisionTangentBasis())
		{
			return VertexTangentZ_Typed<EStaticMeshVertexTangentBasisType::HighPrecision>(VertexIndex);
		}
		else
		{
			return VertexTangentZ_Typed<EStaticMeshVertexTangentBasisType::Default>(VertexIndex);
		}
	}

	template<EStaticMeshVertexTangentBasisType TangentBasisTypeT>
	FORCEINLINE FVector4 VertexTangentY_Typed(uint32 VertexIndex)const
	{
		return reinterpret_cast<const TStaticMeshFullVertex<TangentBasisTypeT, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->GetTangentY();
	}

	/**
	* Calculate the binormal (TangentY) vector using the normal,tangent vectors
	*
	* @param VertexIndex - index into the vertex buffer
	* @return binormal (TangentY) vector
	*/
	FORCEINLINE FVector VertexTangentY(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());

		if (GetUseHighPrecisionTangentBasis())
		{
			return VertexTangentY_Typed<EStaticMeshVertexTangentBasisType::HighPrecision>(VertexIndex);
		}
		else
		{
			return VertexTangentY_Typed<EStaticMeshVertexTangentBasisType::Default>(VertexIndex);
		}
	}

	FORCEINLINE void SetVertexTangents(uint32 VertexIndex, FVector X, FVector Y, FVector Z)
	{
		checkSlow(VertexIndex < GetNumVertices());

		if (GetUseHighPrecisionTangentBasis())
		{
			return reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->SetTangents(X, Y, Z);
		}
		else
		{
			return reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default, 1>*>(Data + VertexIndex * Stride)->SetTangents(X, Y, Z);
		}
	}

	/**
	* Set the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_STATIC_TEXCOORDS] value to index into UVs array
	* @param Vec2D - UV values to set
	*/
	FORCEINLINE void SetVertexUV(uint32 VertexIndex, uint32 UVIndex, const FVector2D& Vec2D)
	{
		checkSlow(VertexIndex < GetNumVertices());
		checkSlow(UVIndex < GetNumTexCoords());

		if (GetUseHighPrecisionTangentBasis())
		{
			if (GetUseFullPrecisionUVs())
			{
				reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::HighPrecision, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->SetUV(UVIndex, Vec2D);
			}
			else
			{
				reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->SetUV(UVIndex, Vec2D);
			}
		}
		else
		{
			if (GetUseFullPrecisionUVs())
			{
				reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::HighPrecision, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->SetUV(UVIndex, Vec2D);
			}
			else
			{
				reinterpret_cast<TStaticMeshFullVertex<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->SetUV(UVIndex, Vec2D);
			}
		}
	}

	template<EStaticMeshVertexTangentBasisType TangentBasisTypeT, EStaticMeshVertexUVType UVTypeT>
	FORCEINLINE FVector2D GetVertexUV_Typed(uint32 VertexIndex, uint32 UVIndex)const
	{
		return reinterpret_cast<const TStaticMeshFullVertex<TangentBasisTypeT, UVTypeT, MAX_STATIC_TEXCOORDS>*>(Data + VertexIndex * Stride)->GetUV(UVIndex);
	}

	/**
	* Set the vertex UV values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_STATIC_TEXCOORDS] value to index into UVs array
	* @param 2D UV values
	*/
	FORCEINLINE FVector2D GetVertexUV(uint32 VertexIndex, uint32 UVIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		checkSlow(UVIndex < GetNumTexCoords());

		if (GetUseHighPrecisionTangentBasis())
		{
			if (GetUseFullPrecisionUVs())
			{
				return GetVertexUV_Typed<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::HighPrecision>(VertexIndex, UVIndex);
			}
			else
			{
				return GetVertexUV_Typed<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default>(VertexIndex, UVIndex);
			}
		}
		else
		{
			if (GetUseFullPrecisionUVs())
			{
				return GetVertexUV_Typed<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::HighPrecision>(VertexIndex, UVIndex);
			}
			else
			{
				return GetVertexUV_Typed<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default>(VertexIndex, UVIndex);
			}
		}
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

	FORCEINLINE uint32 GetNumTexCoords() const
	{
		return NumTexCoords;
	}

	FORCEINLINE bool GetUseFullPrecisionUVs() const
	{
		return bUseFullPrecisionUVs;
	}

	FORCEINLINE void SetUseFullPrecisionUVs(bool UseFull)
	{
		bUseFullPrecisionUVs = UseFull;
	}

	FORCEINLINE bool GetUseHighPrecisionTangentBasis() const
	{
		return bUseHighPrecisionTangentBasis;
	}

	FORCEINLINE void SetUseHighPrecisionTangentBasis(bool bUseHighPrecision)
	{
		bUseHighPrecisionTangentBasis = bUseHighPrecision;
	}

	const uint8* GetRawVertexData() const
	{
		check(Data != NULL);
		return Data;
	}

	/**
	* Convert the existing data in this mesh without rebuilding the mesh (loss of precision)
	*/
	template<typename SrcVertexTypeT, typename DstVertexTypeT>
	void ConvertVertexFormat()
	{
		CA_SUPPRESS(6326);
		if (SrcVertexTypeT::TangentBasisType == DstVertexTypeT::TangentBasisType &&
			SrcVertexTypeT::UVType == DstVertexTypeT::UVType)
		{
			return;
		}

		static_assert(SrcVertexTypeT::NumTexCoords == DstVertexTypeT::NumTexCoords, "NumTexCoords don't match");

		auto& SrcVertexData = *static_cast<TStaticMeshVertexData<SrcVertexTypeT>*>(VertexData);

		TArray<DstVertexTypeT> DstVertexData;
		DstVertexData.AddUninitialized(SrcVertexData.Num());

		for (int32 VertIdx = 0; VertIdx < SrcVertexData.Num(); VertIdx++)
		{
			SrcVertexTypeT& SrcVert = SrcVertexData[VertIdx];
			DstVertexTypeT& DstVert = DstVertexData[VertIdx];

			DstVert.SetTangents(SrcVert.GetTangentX(), SrcVert.GetTangentY(), SrcVert.GetTangentZ());

			for (int32 UVIdx = 0; UVIdx < DstVertexTypeT::NumTexCoords; UVIdx++)
			{
				DstVert.SetUV(UVIdx, SrcVert.GetUV(UVIdx));
			}
		}

		CA_SUPPRESS(6326);
		bUseHighPrecisionTangentBasis = DstVertexTypeT::TangentBasisType == EStaticMeshVertexTangentBasisType::HighPrecision;
		CA_SUPPRESS(6326);
		bUseFullPrecisionUVs = DstVertexTypeT::UVType == EStaticMeshVertexUVType::HighPrecision;

		AllocateData();
		*static_cast<TStaticMeshVertexData<DstVertexTypeT>*>(VertexData) = DstVertexData;

		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
	}

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("Static-mesh vertices"); }

private:

	/** The vertex data storage type */
	FStaticMeshVertexDataInterface* VertexData;

	/** The number of texcoords/vertex in the buffer. */
	uint32 NumTexCoords;

	/** The cached vertex data pointer. */
	uint8* Data;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of vertices. */
	uint32 NumVertices;

	/** Corresponds to UStaticMesh::UseFullPrecisionUVs. if true then 32 bit UVs are used */
	bool bUseFullPrecisionUVs;

	/** If true then RGB10A2 is used to store tangent else RGBA8 */
	bool bUseHighPrecisionTangentBasis;

	/** Allocates the vertex data storage type. */
	void AllocateData(bool bNeedsCPUAccess = true);
};