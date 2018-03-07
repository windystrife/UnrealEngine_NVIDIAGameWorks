// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMesh.h: Unreal skeletal mesh objects.
=============================================================================*/

/*-----------------------------------------------------------------------------
	USkinnedMeshComponent.
-----------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "RenderResource.h"
#include "PackedNormal.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Components.h"
#include "Materials/MaterialInterface.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "ReferenceSkeleton.h"
#include "GPUSkinPublicDefs.h"
#include "Serialization/BulkData.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/SkinWeightVertexBuffer.h"

class FMaterialRenderProxy;
class FMeshElementCollector;
class FPrimitiveDrawInterface;
class FRawStaticIndexBuffer16or32Interface;
class UMorphTarget;
class UPrimitiveComponent;
class USkeletalMesh;
class USkinnedMeshComponent;

/** 
* A pair of bone indices
*/
struct FBoneIndexPair
{
	int32 BoneIdx[2];

	bool operator==(const FBoneIndexPair& Src) const
	{
		return (BoneIdx[0] == Src.BoneIdx[0]) && (BoneIdx[1] == Src.BoneIdx[1]);
	}

	friend FORCEINLINE uint32 GetTypeHash( const FBoneIndexPair& BonePair )
	{
		return FCrc::MemCrc_DEPRECATED(&BonePair, sizeof(FBoneIndexPair));
	}

	/**
	* Serialize to Archive
	*/
	friend FArchive& operator<<( FArchive& Ar, FBoneIndexPair& W )
	{
		return Ar << W.BoneIdx[0] << W.BoneIdx[1];
	}
};




class USkeletalMesh;


/*-----------------------------------------------------------------------------
	USkeletalMesh.
-----------------------------------------------------------------------------*/

struct FMeshWedge
{
	uint32			iVertex;			// Vertex index.
	FVector2D		UVs[MAX_TEXCOORDS];	// UVs.
	FColor			Color;			// Vertex color.
	friend FArchive &operator<<( FArchive& Ar, FMeshWedge& T )
	{
		Ar << T.iVertex;
		for( int32 UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx )
		{
			Ar << T.UVs[UVIdx];
		}
		Ar << T.Color;
		return Ar;
	}
};
template <> struct TIsPODType<FMeshWedge> { enum { Value = true }; };

struct FMeshFace
{
	// Textured Vertex indices.
	uint32		iWedge[3];
	// Source Material (= texture plus unique flags) index.
	uint16		MeshMaterialIndex;

	FVector	TangentX[3];
	FVector	TangentY[3];
	FVector	TangentZ[3];

	// 32-bit flag for smoothing groups.
	uint32   SmoothingGroups;
};
template <> struct TIsPODType<FMeshFace> { enum { Value = true }; };

// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	FTransform	Transform;

	// For collision testing / debug drawing...
	float       Length;
	float       XSize;
	float       YSize;
	float       ZSize;
};

template <> struct TIsPODType<VJointPos> { enum { Value = true }; };





// Textured triangle.
struct VTriangle
{
	// Point to three vertices in the vertex list.
	uint32   WedgeIndex[3];
	// Materials can be anything.
	uint8    MatIndex;
	// Second material from exporter (unused)
	uint8    AuxMatIndex;
	// 32-bit flag for smoothing groups.
	uint32   SmoothingGroups;

	FVector	TangentX[3];
	FVector	TangentY[3];
	FVector	TangentZ[3];


	VTriangle& operator=( const VTriangle& Other)
	{
		this->AuxMatIndex = Other.AuxMatIndex;
		this->MatIndex        =  Other.MatIndex;
		this->SmoothingGroups =  Other.SmoothingGroups;
		this->WedgeIndex[0]   =  Other.WedgeIndex[0];
		this->WedgeIndex[1]   =  Other.WedgeIndex[1];
		this->WedgeIndex[2]   =  Other.WedgeIndex[2];
		this->TangentX[0]   =  Other.TangentX[0];
		this->TangentX[1]   =  Other.TangentX[1];
		this->TangentX[2]   =  Other.TangentX[2];

		this->TangentY[0]   =  Other.TangentY[0];
		this->TangentY[1]   =  Other.TangentY[1];
		this->TangentY[2]   =  Other.TangentY[2];

		this->TangentZ[0]   =  Other.TangentZ[0];
		this->TangentZ[1]   =  Other.TangentZ[1];
		this->TangentZ[2]   =  Other.TangentZ[2];

		return *this;
	}
};
template <> struct TIsPODType<VTriangle> { enum { Value = true }; };

struct FVertInfluence 
{
	float Weight;
	uint32 VertIndex;
	FBoneIndexType BoneIndex;
	friend FArchive &operator<<( FArchive& Ar, FVertInfluence& F )
	{
		Ar << F.Weight << F.VertIndex << F.BoneIndex;
		return Ar;
	}
};
template <> struct TIsPODType<FVertInfluence> { enum { Value = true }; };

/**
* Data needed for importing an extra set of vertex influences
*/
struct FSkelMeshExtraInfluenceImportData
{
	FReferenceSkeleton		RefSkeleton;
	TArray<FVertInfluence> Influences;
	TArray<FMeshWedge> Wedges;
	TArray<FMeshFace> Faces;
	TArray<FVector> Points;
	int32 MaxBoneCountPerChunk;
};

//
//	FSoftSkinVertex
//

struct FSoftSkinVertex
{
	FVector			Position;
	
	// Tangent, U-direction
	FPackedNormal	TangentX;
	// Binormal, V-direction
	FPackedNormal	TangentY;
	// Normal
	FPackedNormal	TangentZ;
	
	// UVs
	FVector2D		UVs[MAX_TEXCOORDS];
	// VertexColor
	FColor			Color;
	uint8			InfluenceBones[MAX_TOTAL_INFLUENCES];
	uint8			InfluenceWeights[MAX_TOTAL_INFLUENCES];

	/** If this vert is rigidly weighted to a bone, return true and the bone index. Otherwise return false. */
	ENGINE_API bool GetRigidWeightBone(uint8& OutBoneIndex) const;

	/** Returns the maximum weight of any bone that influences this vertex. */
	ENGINE_API uint8 GetMaximumWeight() const;

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,FSoftSkinVertex& V);
};



/**
 * A structure for holding mesh-to-mesh triangle influences to skin one mesh to another (similar to a wrap deformer)
 */
struct FMeshToMeshVertData
{
	// Barycentric coords and distance along normal for the position of the final vert
	FVector4 PositionBaryCoordsAndDist;

	// Barycentric coords and distance along normal for the location of the unit normal endpoint
	// Actual normal = ResolvedNormalPosition - ResolvedPosition
	FVector4 NormalBaryCoordsAndDist;

	// Barycentric coords and distance along normal for the location of the unit Tangent endpoint
	// Actual normal = ResolvedNormalPosition - ResolvedPosition
	FVector4 TangentBaryCoordsAndDist;

	// Contains the 3 indices for verts in the source mesh forming a triangle, the last element
	// is a flag to decide how the skinning works, 0xffff uses no simulation, and just normal
	// skinning, anything else uses the source mesh and the above skin data to get the final position
	uint16	 SourceMeshVertIndices[4];

	// Dummy for alignment (16 bytes)
	uint32	 Padding[2];

	/**
	 * Serializer
	 *
	 * @param Ar - archive to serialize with
	 * @param V - vertex to serialize
	 * @return archive that was used
	 */
	friend FArchive& operator<<(FArchive& Ar, FMeshToMeshVertData& V)
	{
		Ar	<< V.PositionBaryCoordsAndDist 
			<< V.NormalBaryCoordsAndDist
			<< V.TangentBaryCoordsAndDist
			<< V.SourceMeshVertIndices[0]
			<< V.SourceMeshVertIndices[1]
			<< V.SourceMeshVertIndices[2]
			<< V.SourceMeshVertIndices[3]
			<< V.Padding[0]
			<< V.Padding[1];
		return Ar;
	}
};

/** Helper to convert the above enum to string */
static const TCHAR* TriangleSortOptionToString(ETriangleSortOption Option)
{
	switch(Option)
	{
		case TRISORT_CenterRadialDistance:
			return TEXT("CenterRadialDistance");
		case TRISORT_Random:
			return TEXT("Random");
		case TRISORT_MergeContiguous:
			return TEXT("MergeContiguous");
		case TRISORT_Custom:
			return TEXT("Custom");
		case TRISORT_CustomLeftRight:
			return TEXT("CustomLeftRight");
		default:
			return TEXT("None");
	}
}

struct FClothingSectionData
{
	FClothingSectionData()
		: AssetGuid()
		, AssetLodIndex(INDEX_NONE)
	{}

	bool IsValid()
	{
		return AssetGuid.IsValid() && AssetLodIndex != INDEX_NONE;
	}

	/** Guid of the clothing asset applied to this section */
	FGuid AssetGuid;

	/** LOD inside the applied asset that is used */
	int32 AssetLodIndex;

	friend FArchive& operator<<(FArchive& Ar, FClothingSectionData& Data)
	{
		Ar << Data.AssetGuid;
		Ar << Data.AssetLodIndex;

		return Ar;
	}
};

/**
 * A set of skeletal mesh triangles which use the same material
 */
struct FSkelMeshSection
{
	/** Material (texture) used for this section. */
	uint16 MaterialIndex;

	/** The offset of this section's indices in the LOD's index buffer. */
	uint32 BaseIndex;

	/** The number of triangles in this section. */
	uint32 NumTriangles;

	/** Current triangle sorting method */
	TEnumAsByte<ETriangleSortOption> TriangleSorting;

	/** Is this mesh selected? */
	uint8 bSelected:1;
	
	/** This section will recompute tangent in runtime */
	bool bRecomputeTangent;

	/** This section will cast shadow */
	bool bCastShadow;

	/** This Section can be disabled for cloth simulation and corresponding Cloth Section will be enabled*/
	bool bDisabled;
	
	/** Corresponding Section Index will be enabled when this section is disabled 
		because corresponding cloth section will be shown instead of this
		or disabled section index when this section is enabled for cloth simulation
	*/
	int16 CorrespondClothSectionIndex;

	/** Decide whether enabling clothing LOD for this section or not, just using skelmesh LOD_0's one to decide */
	/** no need anymore because each clothing LOD will be assigned to each mesh LOD  */
	uint8 bEnableClothLOD_DEPRECATED;

	/** The offset into the LOD's vertex buffer of this section's vertices. */
	uint32 BaseVertexIndex;

	/** The soft vertices of this section. */
	TArray<FSoftSkinVertex> SoftVertices;

	/** The extra vertex data for mapping to an APEX clothing simulation mesh. */
	TArray<FMeshToMeshVertData> ClothMappingData;

	/** The physical mesh vertices imported from the APEX file. */
	TArray<FVector> PhysicalMeshVertices;

	/** The physical mesh normals imported from the APEX file. */
	TArray<FVector> PhysicalMeshNormals;

	/** The bones which are used by the vertices of this section. Indices of bones in the USkeletalMesh::RefSkeleton array */
	TArray<FBoneIndexType> BoneMap;

	/** Number of vertices in this section (size of SoftVertices array). Available in non-editor builds. */
	int32 NumVertices;

	/** max # of bones used to skin the vertices in this section */
	int32 MaxBoneInfluences;

	// INDEX_NONE if not set
	int16 CorrespondClothAssetIndex;

	/** Clothing data for this section, clothing is only present if ClothingData.IsValid() returns true */
	FClothingSectionData ClothingData;

	FSkelMeshSection()
		: MaterialIndex(0)
		, BaseIndex(0)
		, NumTriangles(0)
		, TriangleSorting(0)
		, bSelected(false)
		, bRecomputeTangent(false)
		, bCastShadow(true)
		, bDisabled(false)
		, CorrespondClothSectionIndex(-1)
		, BaseVertexIndex(0)
		, NumVertices(0)
		, MaxBoneInfluences(4)
	{}


	/**
	* @return total num rigid verts for this section
	*/
	FORCEINLINE int32 GetNumVertices() const
	{
		// Either SoftVertices should be empty, or size should match NumVertices
		check(SoftVertices.Num() == 0 || SoftVertices.Num() == NumVertices);
		return NumVertices;
	}

	/**
	* @return starting index for rigid verts for this section in the LOD vertex buffer
	*/
	FORCEINLINE int32 GetVertexBufferIndex() const
	{
		return BaseVertexIndex;
	}

	/**
	* @return TRUE if we have cloth data for this section
	*/
	FORCEINLINE bool HasClothingData() const
	{
		return (ClothMappingData.Num() > 0);
	}

	/**
	* Calculate max # of bone influences used by this skel mesh section
	*/
	ENGINE_API void CalcMaxBoneInfluences();

	FORCEINLINE bool HasExtraBoneInfluences() const
	{
		return MaxBoneInfluences > MAX_INFLUENCES_PER_STREAM;
	}

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar, FSkelMeshSection& S);
};

/**
* Base vertex data for GPU skinned skeletal meshes
*	make sure to update GpuSkinCacheCommon.usf if the member sizes/order change!
*/
struct TGPUSkinVertexBase
{
	// Tangent, U-direction
	FPackedNormal	TangentX;
	// Normal
	FPackedNormal	TangentZ;

	/** Serializer */
	void Serialize(FArchive& Ar);
};

/** 
* 16 bit UV version of skeletal mesh vertex
*	make sure to update GpuSkinCacheCommon.usf if the member sizes/order change!
*/
template<uint32 NumTexCoords>
struct TGPUSkinVertexFloat16Uvs : public TGPUSkinVertexBase
{
	/** full float position **/
	FVector			Position;
	/** half float UVs */
	FVector2DHalf	UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TGPUSkinVertexFloat16Uvs& V)
	{
		V.Serialize(Ar);
		Ar << V.Position;

		for(uint32 UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << V.UVs[UVIndex];
		}
		return Ar;
	}
};

/** 
* 32 bit UV version of skeletal mesh vertex
*	make sure to update GpuSkinCacheCommon.usf if the member sizes/order change!
*/
template<uint32 NumTexCoords>
struct TGPUSkinVertexFloat32Uvs : public TGPUSkinVertexBase
{
	/** full float position **/
	FVector			Position;
	/** full float UVs */
	FVector2D UVs[NumTexCoords];

	/**
	* Serializer
	*
	* @param Ar - archive to serialize with
	* @param V - vertex to serialize
	* @return archive that was used
	*/
	friend FArchive& operator<<(FArchive& Ar,TGPUSkinVertexFloat32Uvs& V)
	{
		V.Serialize(Ar);
		Ar << V.Position;

		for(uint32 UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			Ar << V.UVs[UVIndex];
		}
		return Ar;
	}
};

/** An interface to the skel-mesh vertex data storage type. */
class FSkeletalMeshVertexDataInterface
{
public:

	/** Virtual destructor. */
	virtual ~FSkeletalMeshVertexDataInterface() {}

	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(uint32 NumVertices) = 0;

	/** @return The stride of the vertex data in the buffer. */
	virtual uint32 GetStride() const = 0;

	/** @return A pointer to the data in the buffer. */
	virtual uint8* GetDataPointer() = 0;

	/** @return number of vertices in the buffer */
	virtual uint32 GetNumVertices() = 0;

	/** @return A pointer to the FResourceArrayInterface for the vertex data. */
	virtual FResourceArrayInterface* GetResourceArray() = 0;

	/** Serializer. */
	virtual void Serialize(FArchive& Ar) = 0;
};


/** The implementation of the skeletal mesh vertex data storage type. */
template<typename VertexDataType>
class TSkeletalMeshVertexData :
	public FSkeletalMeshVertexDataInterface,
	public TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>
{
public:
	typedef TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT> ArrayType;

	/**
	* Constructor
	* @param InNeedsCPUAccess - true if resource array data should be CPU accessible
	*/
	TSkeletalMeshVertexData(bool InNeedsCPUAccess=false)
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
	* @return number of vertices stored in the resource data array
	*/
	virtual uint32 GetNumVertices()
	{
		return ArrayType::Num();
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
		ArrayType::BulkSerialize(Ar);
	}
	/**
	* Assignment operator. This is currently the only method which allows for 
	* modifying an existing resource array
	*/
	TSkeletalMeshVertexData<VertexDataType>& operator=(const TArray<VertexDataType>& Other)
	{
		ArrayType::operator=(TArray<VertexDataType,TAlignedHeapAllocator<VERTEXBUFFER_ALIGNMENT> >(Other));
		return *this;
	}
};

/** 
* Vertex buffer with static lod chunk vertices for use with GPU skinning 
*/
class FSkeletalMeshVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Constructor
	*/
	ENGINE_API FSkeletalMeshVertexBuffer();

	/**
	* Destructor
	*/
	ENGINE_API virtual ~FSkeletalMeshVertexBuffer();

	/**
	* Assignment. Assumes that vertex buffer will be rebuilt 
	*/
	ENGINE_API FSkeletalMeshVertexBuffer& operator=(const FSkeletalMeshVertexBuffer& Other);
	/**
	* Constructor (copy)
	*/
	ENGINE_API FSkeletalMeshVertexBuffer(const FSkeletalMeshVertexBuffer& Other);

	/** 
	* Delete existing resources 
	*/
	void CleanUp();

	/** 
	* @return true is VertexData is valid 
	*/
	bool IsVertexDataValid() const 
	{ 
		return VertexData != NULL; 
	}

	/**
	* Initializes the buffer with the given vertices.
	* @param InVertices - The vertices to initialize the buffer with.
	*/
	ENGINE_API void Init(const TArray<FSoftSkinVertex>& InVertices);

	/**
	* Serializer for this class
	* @param Ar - archive to serialize to
	* @param B - data to serialize
	*/
	friend FArchive& operator<<(FArchive& Ar,FSkeletalMeshVertexBuffer& VertexBuffer);

	//~ Begin FRenderResource interface.

	/**
	* Initialize the RHI resource for this vertex buffer
	*/
	virtual void InitRHI() override;

	virtual void ReleaseRHI() override;

	/**
	* @return text description for the resource type
	*/
	virtual FString GetFriendlyName() const override;

	//~ End FRenderResource interface.

	//~ Vertex data accessors.

	/** 
	* Const access to entry in vertex data array
	*
	* @param VertexIndex - index into the vertex buffer
	* @return pointer to vertex data cast to base vertex type
	*/
	FORCEINLINE const TGPUSkinVertexBase* GetVertexPtr(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return (TGPUSkinVertexBase*)(Data + VertexIndex * Stride);
	}
	/** 
	* Non-Const access to entry in vertex data array
	*
	* @param VertexIndex - index into the vertex buffer
	* @return pointer to vertex data cast to base vertex type
	*/
	FORCEINLINE TGPUSkinVertexBase* GetVertexPtr(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return (TGPUSkinVertexBase*)(Data + VertexIndex * Stride);
	}

	/**
	* Get the vertex UV values at the given index in the vertex buffer.
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_TEXCOORDS] value to index into UVs array
	* @return 2D UV values
	*/
	FORCEINLINE FVector2D GetVertexUVFast(uint32 VertexIndex,uint32 UVIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		if( !bUseFullPrecisionUVs )
		{
			return ((TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
		}
		else
		{
			return ((TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS>*)(Data + VertexIndex * Stride))->UVs[UVIndex];
		}		
	}	

	/**
	* Get the vertex UV values at the given index in the vertex buffer.
	*
	* @param VertexIndex - index into the vertex buffer
	* @param UVIndex - [0,MAX_TEXCOORDS] value to index into UVs array
	* @return 2D UV values
	*/
	FORCEINLINE FVector2D GetVertexUV(uint32 VertexIndex,uint32 UVIndex) const
	{
		return GetVertexUVFast(VertexIndex, UVIndex);
	}

	/**
	* Get the vertex XYZ values at the given index in the vertex buffer
	*
	* @param VertexIndex - index into the vertex buffer
	* @return FVector 3D position
	*/
	FORCEINLINE FVector GetVertexPositionSlow(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return GetVertexPositionFast((const TGPUSkinVertexBase*)(Data + VertexIndex * Stride));
	}

	/**
	* Get the vertex XYZ values of the given SrcVertex
	*
	* @param TGPUSkinVertexBase *
	* @return FVector 3D position
	*/
	FORCEINLINE FVector GetVertexPositionFast(const TGPUSkinVertexBase* SrcVertex) const
	{
		if( !bUseFullPrecisionUVs )
		{
			return ((TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS>*)(SrcVertex))->Position;
		}
		else
		{
			return ((TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS>*)(SrcVertex))->Position;
		}		
	}

	/**
	* Get the vertex XYZ values of the given SrcVertex
	*
	* @param TGPUSkinVertexBase *
	* @return FVector 3D position
	*/
	FORCEINLINE FVector GetVertexPositionFast(uint32 VertexIndex) const
	{
		return GetVertexPositionFast((const TGPUSkinVertexBase*)(Data + VertexIndex * Stride));
	}

	//~ Other accessors.

	/** 
	* @return true if using 32 bit floats for UVs 
	*/
	FORCEINLINE bool GetUseFullPrecisionUVs() const
	{
		return bUseFullPrecisionUVs;
	}
	/** 
	* @param UseFull - set to true if using 32 bit floats for UVs 
	*/
	FORCEINLINE void SetUseFullPrecisionUVs(bool UseFull)
	{
		bUseFullPrecisionUVs = UseFull;
	}
	/** 
	* @return number of vertices in this vertex buffer
	*/
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}
	/** 
	* @return cached stride for vertex data type for this vertex buffer
	*/
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	/** 
	* @return total size of data in resource array
	*/
	FORCEINLINE uint32 GetVertexDataSize() const
	{
		return NumVertices * Stride;
	}
	/** 
	* @return Mesh Origin 
	*/
	FORCEINLINE const FVector& GetMeshOrigin() const 		
	{ 
		return MeshOrigin; 
	}

	/** 
	* @return Mesh Extension
	*/
	FORCEINLINE const FVector& GetMeshExtension() const
	{ 
		return MeshExtension;
	}

	/**
	 * @return the number of texture coordinate sets in this buffer
	 */
	FORCEINLINE uint32 GetNumTexCoords() const 
	{
		return NumTexCoords;
	}

	/** 
	* @param bInNeedsCPUAccess - set to true if the CPU needs access to this vertex buffer
	*/
	void SetNeedsCPUAccess(bool bInNeedsCPUAccess);
	bool GetNeedsCPUAccess() const { return bNeedsCPUAccess; }

	/**
	 * @param InNumTexCoords	The number of texture coordinate sets that should be in this mesh
	 */
	FORCEINLINE void SetNumTexCoords( uint32 InNumTexCoords ) 
	{
		NumTexCoords = InNumTexCoords;
	}
	
	/**
	 * Assignment operator. 
	 */
	template <uint32 NumTexCoordsT>
	FSkeletalMeshVertexBuffer& operator=(const TArray< TGPUSkinVertexFloat16Uvs<NumTexCoordsT> >& InVertices)
	{
		check(!bUseFullPrecisionUVs);
		AllocateData();

		*(TSkeletalMeshVertexData< TGPUSkinVertexFloat16Uvs<NumTexCoordsT> >*)VertexData = InVertices;

		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();

		return *this;
	}

	/**
	 * Assignment operator.  
	 */
	template <uint32 NumTexCoordsT>
	FSkeletalMeshVertexBuffer& operator=(const TArray< TGPUSkinVertexFloat32Uvs<NumTexCoordsT> >& InVertices)
	{
		check(bUseFullPrecisionUVs);
		AllocateData();

		*(TSkeletalMeshVertexData< TGPUSkinVertexFloat32Uvs<NumTexCoordsT> >*)VertexData = InVertices;

		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();

		return *this;
	}

	/**
	* Convert the existing data in this mesh from 16 bit to 32 bit UVs.
	* Without rebuilding the mesh (loss of precision)
	*/
	template<uint32 NumTexCoordsT>
	void ConvertToFullPrecisionUVs()
	{
		ConvertToFullPrecisionUVsTyped<NumTexCoordsT>();
	}
	
	// @param guaranteed only to be valid if the vertex buffer is valid
	FShaderResourceViewRHIParamRef GetSRV() const
	{
		return SRVValue;
	}

protected:
	// guaranteed only to be valid if the vertex buffer is valid
	FShaderResourceViewRHIRef SRVValue;

private:
	/** Corresponds to USkeletalMesh::bUseFullPrecisionUVs. if true then 32 bit UVs are used */
	bool bUseFullPrecisionUVs;
	/** true if this vertex buffer will be used with CPU skinning. Resource arrays are set to cpu accessible if this is true */
	bool bNeedsCPUAccess;
	/** The vertex data storage type */
	FSkeletalMeshVertexDataInterface* VertexData;
	/** The cached vertex data pointer. */
	uint8* Data;
	/** The cached vertex stride. */
	uint32 Stride;
	/** The cached number of vertices. */
	uint32 NumVertices;
	/** The number of unique texture coordinate sets in this buffer */
	uint32 NumTexCoords;

	/** The origin of Mesh **/
	FVector MeshOrigin;
	/** The scale of Mesh **/
	FVector MeshExtension;

	/** 
	* Allocates the vertex data storage type. Based on UV precision needed
	*/
	void AllocateData();	

	/** 
	* Copy the contents of the source vertex to the destination vertex in the buffer 
	*
	* @param VertexIndex - index into the vertex buffer
	* @param SrcVertex - source vertex to copy from
	*/
	void SetVertexSlow(uint32 VertexIndex,const FSoftSkinVertex& SrcVertex)
	{
		SetVertexFast(VertexIndex, SrcVertex);
	}

	/** Helper for concrete types */
	void SetVertexFast(uint32 VertexIndex,const FSoftSkinVertex& SrcVertex);

	/** Helper for concrete types */
	template<uint32 NumTexCoordsT>
	void ConvertToFullPrecisionUVsTyped();
};

/** 
 * A vertex buffer for holding skeletal mesh per APEX cloth information only. 
 * This buffer sits along side FSkeletalMeshVertexBuffer in each skeletal mesh lod
 */
class FSkeletalMeshVertexClothBuffer : public FVertexBuffer
{
public:
	/**
	 * Constructor
	 */
	ENGINE_API FSkeletalMeshVertexClothBuffer();

	/**
	 * Destructor
	 */
	ENGINE_API virtual ~FSkeletalMeshVertexClothBuffer();

	/**
	 * Assignment. Assumes that vertex buffer will be rebuilt 
	 */
	ENGINE_API FSkeletalMeshVertexClothBuffer& operator=(const FSkeletalMeshVertexClothBuffer& Other);
	
	/**
	 * Constructor (copy)
	 */
	ENGINE_API FSkeletalMeshVertexClothBuffer(const FSkeletalMeshVertexClothBuffer& Other);

	/** 
	 * Delete existing resources 
	 */
	void CleanUp();

	/**
	 * Initializes the buffer with the given vertices.
	 * @param InVertices - The vertices to initialize the buffer with.
	 * @param InClothIndexMapping - Packed Map: u32 Key, u32 Value.
	 */
	void Init(const TArray<FMeshToMeshVertData>& InMappingData, const TArray<uint64>& InClothIndexMapping);

	/**
	 * Serializer for this class
	 * @param Ar - archive to serialize to
	 * @param B - data to serialize
	 */
	friend FArchive& operator<<(FArchive& Ar,FSkeletalMeshVertexClothBuffer& VertexBuffer);

	//~ Begin FRenderResource interface.

	/**
	 * Initialize the RHI resource for this vertex buffer
	 */
	virtual void InitRHI() override;

	/**
	 * @return text description for the resource type
	 */
	virtual FString GetFriendlyName() const override;

	//~ End FRenderResource interface.

	//~ Vertex data accessors.
	
	FORCEINLINE FMeshToMeshVertData& MappingData(uint32 VertexIndex)
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *((FMeshToMeshVertData*)(Data + VertexIndex * Stride));
	}
	FORCEINLINE const FMeshToMeshVertData& MappingData(uint32 VertexIndex) const
	{
		checkSlow(VertexIndex < GetNumVertices());
		return *((FMeshToMeshVertData*)(Data + VertexIndex * Stride));
	}

	/** 
	 * @return number of vertices in this vertex buffer
	 */
	FORCEINLINE uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	/** 
	* @return cached stride for vertex data type for this vertex buffer
	*/
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	/** 
	* @return total size of data in resource array
	*/
	FORCEINLINE uint32 GetVertexDataSize() const
	{
		return NumVertices * Stride;
	}

	inline FShaderResourceViewRHIRef GetSRV() const
	{
		return VertexBufferSRV;
	}

	inline const TArray<uint64>& GetClothIndexMapping() const
	{
		return ClothIndexMapping;
	}

private:
	/** The vertex data storage type */
	FSkeletalMeshVertexDataInterface* VertexData;
	FShaderResourceViewRHIRef VertexBufferSRV;

	// Packed Map: u32 Key, u32 Value
	TArray<uint64> ClothIndexMapping;

	/** The cached vertex data pointer. */
	uint8* Data;
	/** The cached vertex stride. */
	uint32 Stride;
	/** The cached number of vertices. */
	uint32 NumVertices;

	/** 
	 * Allocates the vertex data storage type
	 */
	void AllocateData();	
};

class FMorphTargetVertexInfoBuffers : public FRenderResource
{
public:
	FMorphTargetVertexInfoBuffers() : NumTotalWorkItems(0)
	{
	}

	ENGINE_API virtual void InitRHI() override;
	ENGINE_API virtual void ReleaseRHI() override;

	uint32 GetNumWorkItems(uint32 index = UINT_MAX) const
	{
		check(index == UINT_MAX || index < (uint32)WorkItemsPerMorph.Num());
		return index != UINT_MAX ? WorkItemsPerMorph[index] : NumTotalWorkItems;
	}

	uint32 GetNumMorphs() const
	{
		return WorkItemsPerMorph.Num();
	}

	uint32 GetStartOffset(uint32 Index) const
	{
		check(Index < (uint32)StartOffsetPerMorph.Num());
		return StartOffsetPerMorph[Index];
	}

	const FVector4& GetMaximumMorphScale(uint32 Index) const
	{
		check(Index < (uint32)MaximumValuePerMorph.Num());
		return MaximumValuePerMorph[Index];
	}

	const FVector4& GetMinimumMorphScale(uint32 Index) const
	{
		check(Index < (uint32)MinimumValuePerMorph.Num());
		return MinimumValuePerMorph[Index];
	}

	FVertexBufferRHIRef VertexIndicesVB;
	FShaderResourceViewRHIRef VertexIndicesSRV;

	FVertexBufferRHIRef MorphDeltasVB;
	FShaderResourceViewRHIRef MorphDeltasSRV;

	// Changes to this struct must be reflected in MorphTargets.usf
	struct FMorphDelta
	{
		FMorphDelta(FVector InPosDelta, FVector InTangentDelta)
		{
			PosDelta[0] = FFloat16(InPosDelta.X);
			PosDelta[1] = FFloat16(InPosDelta.Y);
			PosDelta[2] = FFloat16(InPosDelta.Z);

			TangentDelta[0] = FFloat16(InTangentDelta.X);
			TangentDelta[1] = FFloat16(InTangentDelta.Y);
			TangentDelta[2] = FFloat16(InTangentDelta.Z);
		}

		FFloat16 PosDelta[3];
		FFloat16 TangentDelta[3];
	};

protected:
	// Transient data used while creating the vertex buffers, gets deleted as soon as VB gets initialized.
	TArray<uint32> VertexIndices;
	// Transient data used while creating the vertex buffers, gets deleted as soon as VB gets initialized.
	TArray<FMorphDelta> MorphDeltas;

	//x,y,y separate for position and shared w for tangent
	TArray<FVector4> MaximumValuePerMorph;
	TArray<FVector4> MinimumValuePerMorph;
	TArray<uint32> StartOffsetPerMorph;
	TArray<uint32> WorkItemsPerMorph;
	uint32 NumTotalWorkItems;

	friend class FStaticLODModel;
};

struct FMultiSizeIndexContainerData
{
	TArray<uint32> Indices;
	uint32 DataTypeSize;
};

/**
 * Skeletal mesh index buffers are 16 bit by default and 32 bit when called for.
 * This class adds a level of abstraction on top of the index buffers so that we can treat them all as 32 bit.
 */
class FMultiSizeIndexContainer
{
public:
	FMultiSizeIndexContainer()
	: DataTypeSize(sizeof(uint16))
	, IndexBuffer(NULL)
	{
	}

	ENGINE_API ~FMultiSizeIndexContainer();
	
	/**
	 * Initialize the index buffer's render resources.
	 */
	void InitResources();

	/**
	 * Releases the index buffer's render resources.
	 */	
	void ReleaseResources();

	/**
	 * Serialization.
	 * @param Ar				The archive with which to serialize.
	 * @param bNeedsCPUAccess	If true, the loaded data will remain in CPU memory
	 *							even after the RHI index buffer has been initialized.
	 */
	void Serialize(FArchive& Ar, bool bNeedsCPUAccess);

	/**
	 * Creates a new index buffer
	 */
	ENGINE_API void CreateIndexBuffer(uint8 DataTypeSize);

	/**
	 * Repopulates the index buffer
	 */
	ENGINE_API void RebuildIndexBuffer( const FMultiSizeIndexContainerData& InData );

	/**
	 * Returns a 32 bit version of the index buffer
	 */
	ENGINE_API void GetIndexBuffer( TArray<uint32>& OutArray ) const;

	/**
	 * Populates the index buffer with a new set of indices
	 */
	ENGINE_API void CopyIndexBuffer(const TArray<uint32>& NewArray);

	bool IsIndexBufferValid() const { return IndexBuffer != NULL; }

	/**
	 * Accessors
	 */
	uint8 GetDataTypeSize() const { return DataTypeSize; }
	FRawStaticIndexBuffer16or32Interface* GetIndexBuffer() 
	{ 
		check( IndexBuffer != NULL );
		return IndexBuffer; 
	}
	const FRawStaticIndexBuffer16or32Interface* GetIndexBuffer() const
	{ 
		check( IndexBuffer != NULL );
		return IndexBuffer; 
	}
	
#if WITH_EDITOR
	/**
	 * Retrieves index buffer related data
	 */
	ENGINE_API void GetIndexBufferData( FMultiSizeIndexContainerData& OutData ) const;
	
	ENGINE_API FMultiSizeIndexContainer(const FMultiSizeIndexContainer& Other);
	ENGINE_API FMultiSizeIndexContainer& operator=(const FMultiSizeIndexContainer& Buffer);
#endif

	friend FArchive& operator<<(FArchive& Ar, FMultiSizeIndexContainer& Buffer);

private:
	/** Size of the index buffer's index type (should be 2 or 4 bytes) */
	uint8 DataTypeSize;
	/** The vertex index buffer */
	FRawStaticIndexBuffer16or32Interface* IndexBuffer;
};

/**
* All data to define a certain LOD model for a skeletal mesh.
* All necessary data to render smooth-parts is in SkinningStream, SmoothVerts, SmoothSections and SmoothIndexbuffer.
* For rigid parts: RigidVertexStream, RigidIndexBuffer, and RigidSections.
*/
class FStaticLODModel
{
public:
	/** Sections. */
	TArray<FSkelMeshSection> Sections;

	/** 
	* Bone hierarchy subset active for this chunk.
	* This is a map between the bones index of this LOD (as used by the vertex structs) and the bone index in the reference skeleton of this SkeletalMesh.
	*/
	TArray<FBoneIndexType> ActiveBoneIndices;  
	
	/** 
	* Bones that should be updated when rendering this LOD. This may include bones that are not required for rendering.
	* All parents for bones in this array should be present as well - that is, a complete path from the root to each bone.
	* For bone LOD code to work, this array must be in strictly increasing order, to allow easy merging of other required bones.
	*/
	TArray<FBoneIndexType> RequiredBones;

	/** 
	* Rendering data.
	*/

	// Index Buffer (MultiSize: 16bit or 32bit)
	FMultiSizeIndexContainer	MultiSizeIndexContainer; 

	uint32						NumVertices;
	/** The number of unique texture coordinate sets in this lod */
	uint32						NumTexCoords;

	/** Resources needed to render the model using PN-AEN */
	FMultiSizeIndexContainer	AdjacencyMultiSizeIndexContainer;

	/** static vertices from chunks for skinning on GPU */
	FSkeletalMeshVertexBuffer	VertexBufferGPUSkin;
	
	/** Skin weights for skinning */
	FSkinWeightVertexBuffer		SkinWeightVertexBuffer;

	/** A buffer for vertex colors */
	FColorVertexBuffer			ColorVertexBuffer;

	/** A buffer for cloth mesh-mesh mapping */
	FSkeletalMeshVertexClothBuffer	ClothVertexBuffer;

	/** Editor only data: array of the original point (wedge) indices for each of the vertices in a FStaticLODModel */
	FIntBulkData				RawPointIndices;
	FWordBulkData				LegacyRawPointIndices;

	/** Mapping from final mesh vertex index to raw import vertex index. Needed for vertex animation, which only stores positions for import verts. */
	TArray<int32>				MeshToImportVertexMap;
	/** The max index in MeshToImportVertexMap, ie. the number of imported (raw) verts. */
	int32						MaxImportVertex;

	/** GPU friendly access data for MorphTargets for an LOD */
	FMorphTargetVertexInfoBuffers	MorphTargetVertexInfoBuffers;

	/**
	* Initialize the LOD's render resources.
	*
	* @param Parent Parent mesh
	*/
	void InitResources(bool bNeedsVertexColors, int32 LODIndex, TArray<class UMorphTarget*>& InMorphTargets);

	/**
	* Releases the LOD's render resources.
	*/
	ENGINE_API void ReleaseResources();

	/**
	* Releases the LOD's CPU render resources.
	*/
	void ReleaseCPUResources();

	/** Constructor (default) */
	FStaticLODModel()
		: NumVertices(0)
		, NumTexCoords(0)
		, MaxImportVertex(-1)
	{
	}

	/**
	 * Special serialize function passing the owning UObject along as required by FUnytpedBulkData
	 * serialization.
	 *
	 * @param	Ar		Archive to serialize with
	 * @param	Owner	UObject this structure is serialized within
	 * @param	Idx		Index of current array entry being serialized
	 */
	void Serialize( FArchive& Ar, UObject* Owner, int32 Idx );

	/**
	* Fill array with vertex position and tangent data from skel mesh chunks.
	*
	* @param Vertices Array to fill.
	*/
	ENGINE_API void GetVertices(TArray<FSoftSkinVertex>& Vertices) const;

	/** 
	 * Similar to GetVertices but ignores vertices from clothing sections
	 * to avoid getting duplicate vertices from clothing sections if not needed
	 *
	 * @param OutVertices Array to fill
	 */
	ENGINE_API void GetNonClothVertices(TArray<FSoftSkinVertex>& OutVertices) const;

	/**
	* Fill array with APEX cloth mapping data.
	*
	* @param MappingData Array to fill.
	*/
	void GetApexClothMappingData(TArray<FMeshToMeshVertData>& MappingData, TArray<uint64>& OutClothIndexMapping) const;

	/** Flags used when building vertex buffers. */
	struct EVertexFlags
	{
		enum
		{
			None = 0x0,
			UseFullPrecisionUVs = 0x1,
			HasVertexColors = 0x2
		};
	};

	/**
	 * Initialize vertex buffers from skel mesh chunks.
	 * @param BuildFlags See EVertexFlags.
	 */
	ENGINE_API void BuildVertexBuffers(uint32 BuildFlags);

	/** Utility function for returning total number of faces in this LOD. */
	ENGINE_API int32 GetTotalFaces() const;

	DEPRECATED(4.13, "Please use GetSectionFromVertIndex.")
	ENGINE_API void GetChunkAndSkinType(int32 InVertIndex, int32& OutChunkIndex, int32& OutVertIndex, bool& bOutSoftVert, bool& bOutHasExtraBoneInfluences) const
	{
		GetSectionFromVertexIndex(InVertIndex, OutChunkIndex, OutVertIndex, bOutHasExtraBoneInfluences);
		bOutSoftVert = true;
	}

	/** Utility for finding the section that a particular vertex is in. */
	ENGINE_API void GetSectionFromVertexIndex(int32 InVertIndex, int32& OutSectionIndex, int32& OutVertIndex, bool& bOutHasExtraBoneInfluences) const;

	/**
	 * Sort the triangles with the specified sorting method
	 * @param ETriangleSortOption NewTriangleSorting new sorting method
	 */
	ENGINE_API void SortTriangles( FVector SortCenter, bool bUseSortCenter, int32 SectionIndex, ETriangleSortOption NewTriangleSorting );

	/**
	* @return true if any chunks have cloth data.
	*/
	bool HasClothData() const
	{
		for( int32 SectionIdx=0; SectionIdx<Sections.Num(); SectionIdx++ )
		{
			if(Sections[SectionIdx].HasClothingData() )
			{
				return true;
			}
		}
		return false;
	}

	int32 GetApexClothSectionIndex(TArray<int32>& SectionIndices) const
	{
		SectionIndices.Empty();

		uint32 Count = 0;
		for( int32 SectionIdx =0; SectionIdx<Sections.Num(); SectionIdx++ )
		{
			if(Sections[SectionIdx].HasClothingData() )
			{
				SectionIndices.Add(SectionIdx);
				Count++;
			}
		}
		return Count;
	}

	bool HasClothData(int32 SectionIndex) const
	{
		return Sections[SectionIndex].HasClothingData();
	}

	int32 NumNonClothingSections() const
	{
		int32 NumSections = Sections.Num();

		for(int32 i=0; i < NumSections; i++)
		{
			// If we have found the start of the clothing section, return that index, since it is equal to the number of non-clothing entries.
			if((Sections[i].bDisabled == false)
			&& (Sections[i].CorrespondClothSectionIndex >= 0))
			{
				return i;
			}
		}

		return NumSections;
	}

	int32 GetNumNonClothingVertices() const
	{
		int32 NumVerts = 0;
		int32 NumSections = Sections.Num();

		for(int32 i = 0; i < NumSections; i++)
		{
			const FSkelMeshSection& Section = Sections[i];

			// Stop when we hit clothing sections
			if(Section.ClothingData.AssetGuid.IsValid() && !Section.bDisabled)
			{
				break;
			}

			NumVerts += Section.SoftVertices.Num();
		}

		return NumVerts;
	}

	bool DoesVertexBufferHaveExtraBoneInfluences() const
	{
		return SkinWeightVertexBuffer.HasExtraBoneInfluences();
	}

	bool DoSectionsNeedExtraBoneInfluences() const
	{
		for (int32 SectionIdx = 0; SectionIdx < Sections.Num(); ++SectionIdx)
		{
			if (Sections[SectionIdx].HasExtraBoneInfluences())
			{
				return true;
			}
		}

		return false;
	}

	// O(1)
	// @return -1 if not found
	uint32 FindSectionIndex(const FSkelMeshSection& Section) const
	{
		const FSkelMeshSection* Start = Sections.GetData();

		if(Start == nullptr)
		{
			return -1;
		}

		uint32 Ret = &Section - Start;

		if(Ret >= (uint32)Sections.Num())
		{
			Ret = -1;
		}

		return Ret;
	}

	/**
	 * Get Resource Size
	 */
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	SIZE_T GetResourceSize() const;
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;
	SIZE_T GetResourceSizeBytes() const;

	/** Rebuild index buffer for everything **/
#if WITH_EDITOR
	ENGINE_API void RebuildIndexBuffer();
#endif
	ENGINE_API void RebuildIndexBuffer(FMultiSizeIndexContainerData* IndexBufferData, FMultiSizeIndexContainerData* AdjacencyData);
};

/**
 * Resources required to render a skeletal mesh.
 */
class FSkeletalMeshResource
{
public:
	/** Per-LOD render data. */
	TIndirectArray<FStaticLODModel> LODModels;

	/** Default constructor. */
	FSkeletalMeshResource();

	/** Initializes rendering resources. */
	void InitResources(bool bNeedsVertexColors, TArray<UMorphTarget*>& InMorphTargets);
	
	/** Releases rendering resources. */
	ENGINE_API void ReleaseResources();

	/** Serialize to/from the specified archive.. */
	void Serialize(FArchive& Ar, USkeletalMesh* Owner);

	/**
	 * Computes the maximum number of bones per section used to render this mesh.
	 */
//#nv begin #Blast Engine export
	ENGINE_API int32 GetMaxBonesPerSection() const;
//nv end

	/** Returns true if this resource must be skinned on the CPU for the given feature level. */
	bool RequiresCPUSkinning(ERHIFeatureLevel::Type FeatureLevel) const;

	/** Returns true if there are more than MAX_INFLUENCES_PER_STREAM influences per vertex. */
	bool HasExtraBoneInfluences() const;

	/** 
	 *	Return the resource size
	 */
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	SIZE_T GetResourceSize();
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize);
	SIZE_T GetResourceSizeBytes();

#if WITH_EDITORONLY_DATA
	/** UV data used for streaming accuracy debug view modes. In sync for rendering thread */
	TArray<FMeshUVChannelInfo> UVChannelDataPerMaterial;

	void SyncUVChannelData(const TArray<struct FSkeletalMaterial>& ObjectData);
#endif

private:
	/** True if the resource has been initialized. */
	bool bInitialized;
};

//#nv begin #Blast Ability to hide bones using a dynamic index buffer
struct FSkelMeshSectionOverride
{
	/** The offset of this section's indices in the LOD's index buffer. */
	uint32 BaseIndex;

	/** The number of triangles in this section. */
	uint32 NumTriangles;

	FSkelMeshSectionOverride()
		: BaseIndex(0)
		, NumTriangles(0)
	{}
};

class ENGINE_API FDynamicLODModelOverride
{
public:
	/** Sections. */
	TArray<FSkelMeshSectionOverride> Sections;

	// Index Buffer (MultiSize: 16bit or 32bit)
	FMultiSizeIndexContainer	MultiSizeIndexContainer;

	/** Resources needed to render the model using PN-AEN */
	FMultiSizeIndexContainer	AdjacencyMultiSizeIndexContainer;
	/**
	* Initialize the LOD's render resources.
	*
	* @param Parent Parent mesh
	*/
	void InitResources(const FStaticLODModel& InitialData);

	/**
	* Releases the LOD's render resources.
	*/
	void ReleaseResources();

	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;
	SIZE_T GetResourceSizeBytes() const;
};

class ENGINE_API FSkeletalMeshDynamicOverride
{
public:
	/** Per-LOD render data. */
	TIndirectArray<FDynamicLODModelOverride> LODModels;

	/** Default constructor. */
	FSkeletalMeshDynamicOverride() : bInitialized(false) {}

	/** Initializes rendering resources. */
	void InitResources(const FSkeletalMeshResource& InitialData);

	/** Releases rendering resources. */
	void ReleaseResources();

	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize);
	SIZE_T GetResourceSizeBytes();

	inline bool IsInitialized() const { return bInitialized; }

private:
	/** True if the resource has been initialized. */
	bool bInitialized;
};
//nv end

/**
 *	Contains the vertices that are most dominated by that bone. Vertices are in Bone space.
 *	Not used at runtime, but useful for fitting physics assets etc.
 */
struct FBoneVertInfo
{
	// Invariant: Arrays should be same length!
	TArray<FVector>	Positions;
	TArray<FVector>	Normals;
};



/*-----------------------------------------------------------------------------
FSkeletalMeshSceneProxy
-----------------------------------------------------------------------------*/
class USkinnedMeshComponent;

/**
 * A skeletal mesh component scene proxy.
 */
class ENGINE_API FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** 
	 * Constructor. 
	 * @param	Component - skeletal mesh primitive being added
	 */
	FSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshResource* InSkelMeshResource);

#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	
	virtual bool HasDynamicIndirectShadowCasterRepresentation() const override;
	virtual void GetShadowShapes(TArray<FCapsuleShape>& CapsuleShapes) const override;

	/** Returns a pre-sorted list of shadow capsules's bone indicies */
	const TArray<uint16>& GetSortedShadowBoneIndices() const
	{
		return ShadowCapsuleBoneIndices;
	}

	/**
	 * Returns the world transform to use for drawing.
	 * @param OutLocalToWorld - Will contain the local-to-world transform when the function returns.
	 * @param OutWorldToLocal - Will contain the world-to-local transform when the function returns.
	 * 
	 * @return true if out matrices are valid 
	 */
	bool GetWorldMatrices( FMatrix& OutLocalToWorld, FMatrix& OutWorldToLocal ) const;

	/** Util for getting LOD index currently used by this SceneProxy. */
	int32 GetCurrentLODIndex();

	/** 
	 * Render physics asset for debug display
	 */
//#nv begin #Blast Made virtual
	virtual void DebugDrawPhysicsAsset(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const;
//nv end

	/** Render the bones of the skeleton for debug display */
	void DebugDrawSkeleton(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const;

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODSections.GetAllocatedSize() ); }

	/**
	* Updates morph material usage for materials referenced by each LOD entry
	*
	* @param bNeedsMorphUsage - true if the materials used by this skeletal mesh need morph target usage
	*/
	void UpdateMorphMaterialUsage_GameThread(bool bNeedsMorphUsage);


#if WITH_EDITORONLY_DATA
	virtual bool GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const override;
	virtual bool GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const override;
	virtual bool GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4* OneOverScales, FIntVector4* UVChannelIndices) const override;
#endif

	friend class FSkeletalMeshSectionIter;

protected:
	AActor* Owner;
	class FSkeletalMeshObject* MeshObject;
	class FSkeletalMeshResource* SkelMeshResource;

	/** The points to the skeletal mesh and physics assets are purely for debug purposes. Access is NOT thread safe! */
	const USkeletalMesh* SkeletalMeshForDebug;
	class UPhysicsAsset* PhysicsAssetForDebug;

	/** data copied for rendering */
	uint32 bForceWireframe : 1;
	uint32 bIsCPUSkinned : 1;
	uint32 bCanHighlightSelectedSections : 1;
	FMaterialRelevance MaterialRelevance;

	/** info for section element in an LOD */
	struct FSectionElementInfo
	{
		FSectionElementInfo(UMaterialInterface* InMaterial, bool bInEnableShadowCasting, int32 InUseMaterialIndex)
		: Material( InMaterial )
		, bEnableShadowCasting( bInEnableShadowCasting )
		, UseMaterialIndex( InUseMaterialIndex )
#if WITH_EDITOR
		, HitProxy(NULL)
#endif
		{}
		
		UMaterialInterface* Material;
		
		/** Whether shadow casting is enabled for this section. */
		bool bEnableShadowCasting;
		
		/** Index into the materials array of the skel mesh or the component after LOD mapping */
		int32 UseMaterialIndex;

#if WITH_EDITOR
		/** The editor needs to be able to individual sub-mesh hit detection, so we store a hit proxy on each mesh. */
		HHitProxy* HitProxy;
#endif
	};

	/** Section elements for a particular LOD */
	struct FLODSectionElements
	{
		TArray<FSectionElementInfo> SectionElements;
	};
	
	/** Array of section elements for each LOD */
	TArray<FLODSectionElements> LODSections;

	/** 
	 * BoneIndex->capsule pairs used for rendering sphere/capsule shadows 
	 * Note that these are in refpose component space, NOT bone space.
	 */
	TArray<TPair<int32, FCapsuleShape>> ShadowCapsuleData;
	TArray<uint16> ShadowCapsuleBoneIndices;

	/** Set of materials used by this scene proxy, safe to access from the game thread. */
	TSet<UMaterialInterface*> MaterialsInUse_GameThread;
	bool bMaterialsNeedMorphUsage_GameThread;
	
#if WITH_EDITORONLY_DATA
	/** The component streaming distance multiplier */
	float StreamingDistanceMultiplier;
#endif

	void GetDynamicElementsSection(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
								   const FStaticLODModel& LODModel, const int32 LODIndex, const int32 SectionIndex, bool bSectionSelected,
								   const FSectionElementInfo& SectionElementInfo, const FTwoVectors& CustomLeftRightVectors, bool bInSelectable, FMeshElementCollector& Collector) const;

	void GetMeshElementsConditionallySelectable(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, bool bInSelectable, uint32 VisibilityMap, FMeshElementCollector& Collector) const;
};

/** Used to recreate all skeletal mesh components for a given skeletal mesh */
class ENGINE_API FSkeletalMeshComponentRecreateRenderStateContext
{
public:

	/** Initialization constructor. */
	FSkeletalMeshComponentRecreateRenderStateContext(USkeletalMesh* InSkeletalMesh, bool InRefreshBounds = false);


	/** Destructor: recreates render state for all components that had their render states destroyed in the constructor. */
	~FSkeletalMeshComponentRecreateRenderStateContext();
	

private:

	TArray< class USkeletalMeshComponent*> SkeletalMeshComponents;
	bool bRefreshBounds;
};

