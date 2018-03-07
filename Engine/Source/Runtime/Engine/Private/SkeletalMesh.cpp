// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalMesh.cpp: Unreal skeletal mesh and animation implementation.
=============================================================================*/

#include "Engine/SkeletalMesh.h"
#include "Serialization/CustomVersion.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "RawIndexBuffer.h"
#include "Engine/TextureStreamingTypes.h"
#include "Engine/Brush.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Components/SkinnedMeshComponent.h"
#include "Animation/SmartName.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "ComponentReregisterContext.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/RenderingObjectVersion.h"
#include "EngineUtils.h"
#include "EditorSupportDelegates.h"
#include "GPUSkinVertexFactory.h"
#include "TessellationRendering.h"
#include "SkeletalRenderPublic.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "SceneManagement.h"
#include "PhysicsPublic.h"
#include "Animation/MorphTarget.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/AssetUserData.h"
#include "SkeletalMeshSorting.h"
#include "Engine/Engine.h"
#include "Animation/NodeMappingContainer.h"
#include "GPUSkinCache.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopedSlowTask.h" //#nv #Blast Ability to hide bones using a dynamic index buffer

#if WITH_EDITOR
#include "MeshUtilities.h"

#if WITH_APEX_CLOTHING
#include "ApexClothingUtils.h"
#endif

#endif // #if WITH_EDITOR

#include "Interfaces/ITargetPlatform.h"

#if WITH_APEX
#include "PhysXIncludes.h"
#endif// #if WITH_APEX

#include "EditorFramework/AssetImportData.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/BrushComponent.h"
#include "Streaming/UVChannelDensity.h"
#include "Paths.h"

#include "ClothingAssetInterface.h"

#if WITH_EDITOR
#include "ClothingAssetFactoryInterface.h"
#include "ClothingSystemEditorInterfaceModule.h"
#endif
#include "SkeletalDebugRendering.h"
#include "Misc/RuntimeErrors.h"

#define LOCTEXT_NAMESPACE "SkeltalMesh"

DEFINE_LOG_CATEGORY(LogSkeletalMesh);
DECLARE_CYCLE_STAT(TEXT("GetShadowShapes"), STAT_GetShadowShapes, STATGROUP_Anim);

TAutoConsoleVariable<int32> CVarDebugDrawSimpleBones(TEXT("a.DebugDrawSimpleBones"), 0, TEXT("When drawing bones (using Show Bones), draw bones as simple lines."));
TAutoConsoleVariable<int32> CVarDebugDrawBoneAxes(TEXT("a.DebugDrawBoneAxes"), 0, TEXT("When drawing bones (using Show Bones), draw bone axes."));

// Custom serialization version for SkeletalMesh types
struct FSkeletalMeshCustomVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,
		// Remove Chunks array in FStaticLODModel and combine with Sections array
		CombineSectionWithChunk = 1,
		// Remove FRigidSkinVertex and combine with FSoftSkinVertex array
		CombineSoftAndRigidVerts = 2,
		// Need to recalc max bone influences
		RecalcMaxBoneInfluences = 3,
		// Add NumVertices that can be accessed when stripping editor data
		SaveNumVertices = 4,
		// Regenerated clothing section shadow flags from source sections
		RegenerateClothingShadowFlags = 5,
		// Share color buffer structure with StaticMesh
		UseSharedColorBufferFormat = 6,
		// Use separate buffer for skin weights
		UseSeparateSkinWeightBuffer = 7,
		// Added new clothing systems
		NewClothingSystemAdded = 8,
		// Cached inv mass data for clothing assets
		CachedClothInverseMasses = 9,
		// Compact cloth vertex buffer, without dummy entries
		CompactClothVertexBuffer = 10,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FSkeletalMeshCustomVersion() {}
};

const FGuid FSkeletalMeshCustomVersion::GUID(0xD78A4A00, 0xE8584697, 0xBAA819B5, 0x487D46B4);
FCustomVersionRegistration GRegisterSkeletalMeshCustomVersion(FSkeletalMeshCustomVersion::GUID, FSkeletalMeshCustomVersion::LatestVersion, TEXT("SkeletalMeshVer"));

#if WITH_APEX_CLOTHING
/*-----------------------------------------------------------------------------
	utility functions for apex clothing 
-----------------------------------------------------------------------------*/

static apex::ClothingAsset* LoadApexClothingAssetFromBlob(const TArray<uint8>& Buffer)
{
	// Wrap this blob with the APEX read stream class
	physx::PxFileBuf* Stream = GApexSDK->createMemoryReadStream( Buffer.GetData(), Buffer.Num() );
	// Create an NvParameterized serializer
	NvParameterized::Serializer* Serializer = GApexSDK->createSerializer(NvParameterized::Serializer::NST_BINARY);
	// Deserialize into a DeserializedData buffer
	NvParameterized::Serializer::DeserializedData DeserializedData;
	Serializer->deserialize( *Stream, DeserializedData );
	apex::Asset* ApexAsset = NULL;
	if( DeserializedData.size() > 0 )
	{
		// The DeserializedData has something in it, so create an APEX asset from it
		ApexAsset = GApexSDK->createAsset( DeserializedData[0], NULL);
		// Make sure it's a Clothing asset
		if (ApexAsset 
			&& ApexAsset->getObjTypeID() != GApexModuleClothing->getModuleID()
			)
		{
			GPhysCommandHandler->DeferredRelease(ApexAsset);
			ApexAsset = NULL;
		}
	}

	apex::ClothingAsset* ApexClothingAsset = static_cast<apex::ClothingAsset*>(ApexAsset);
	// Release our temporary objects
	Serializer->release();
	GApexSDK->releaseMemoryReadStream( *Stream );

	return ApexClothingAsset;
}

static bool SaveApexClothingAssetToBlob(const apex::ClothingAsset *InAsset, TArray<uint8>& OutBuffer)
{
	bool bResult = false;
	uint32 Size = 0;
	// Get the NvParameterized data for our Clothing asset
	if( InAsset != NULL )
	{
		// Create an APEX write stream
		physx::PxFileBuf* Stream = GApexSDK->createMemoryWriteStream();
		// Create an NvParameterized serializer
		NvParameterized::Serializer* Serializer = GApexSDK->createSerializer(NvParameterized::Serializer::NST_BINARY);

		const NvParameterized::Interface* AssetParameterized = InAsset->getAssetNvParameterized();
		if( AssetParameterized != NULL )
		{
			// Serialize the data into the stream
			Serializer->serialize( *Stream, &AssetParameterized, 1 );
			// Read the stream data into our buffer for UE serialzation
			Size = Stream->getFileLength();
			OutBuffer.AddUninitialized( Size );
			Stream->read( OutBuffer.GetData(), Size );
			bResult = true;
		}

		// Release our temporary objects
		Serializer->release();
		Stream->release();
	}

	return bResult;
}

#endif//#if WITH_APEX_CLOTHING

////////////////////////////////////////////////
SIZE_T FClothingAssetData_Legacy::GetResourceSize() const
{
	return GetResourceSizeBytes();
}

void FClothingAssetData_Legacy::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
{
#if WITH_APEX_CLOTHING
	if (ApexClothingAsset)
	{
		physx::PxU32 LODLevel = ApexClothingAsset->getNumGraphicalLodLevels();
		for(physx::PxU32 LODId=0; LODId < LODLevel; ++LODId)
		{
			if(const apex::RenderMeshAsset* RenderAsset = ApexClothingAsset->getRenderMeshAsset(LODId))
			{
				apex::RenderMeshAssetStats AssetStats;
				RenderAsset->getStats(AssetStats);

				CumulativeResourceSize.AddUnknownMemoryBytes(AssetStats.totalBytes);
			}
		}
	}
#endif // #if WITH_APEX_CLOTHING
}

SIZE_T FClothingAssetData_Legacy::GetResourceSizeBytes() const
{
	FResourceSizeEx ResSize;
	GetResourceSizeEx(ResSize);
	return ResSize.GetTotalMemoryBytes();
}

/*-----------------------------------------------------------------------------
	FSkeletalMeshVertexBuffer
-----------------------------------------------------------------------------*/

/**
* Constructor
*/
FSkeletalMeshVertexBuffer::FSkeletalMeshVertexBuffer() 
:	bUseFullPrecisionUVs(false)
,	bNeedsCPUAccess(false)
,	VertexData(nullptr)
,	Data(nullptr)
,	Stride(0)
,	NumVertices(0)
,	MeshOrigin(FVector::ZeroVector)
, 	MeshExtension(FVector(1.f,1.f,1.f))
{
}

/**
* Destructor
*/
FSkeletalMeshVertexBuffer::~FSkeletalMeshVertexBuffer()
{
	CleanUp();
}

/**
* Assignment. Assumes that vertex buffer will be rebuilt 
*/
FSkeletalMeshVertexBuffer& FSkeletalMeshVertexBuffer::operator=(const FSkeletalMeshVertexBuffer& Other)
{
	CleanUp();
	bUseFullPrecisionUVs = Other.bUseFullPrecisionUVs;
	bNeedsCPUAccess = Other.bNeedsCPUAccess;
	return *this;
}

/**
* Constructor (copy)
*/
FSkeletalMeshVertexBuffer::FSkeletalMeshVertexBuffer(const FSkeletalMeshVertexBuffer& Other)
:	bUseFullPrecisionUVs(Other.bUseFullPrecisionUVs)
,	bNeedsCPUAccess(Other.bNeedsCPUAccess)
,	VertexData(nullptr)
,	Data(nullptr)
,	Stride(0)
,	NumVertices(0)
,	MeshOrigin(Other.MeshOrigin)
, 	MeshExtension(Other.MeshExtension)
{
}

/**
 * @return text description for the resource type
 */
FString FSkeletalMeshVertexBuffer::GetFriendlyName() const
{ 
	return TEXT("Skeletal-mesh vertex buffer"); 
}

/** 
 * Delete existing resources 
 */
void FSkeletalMeshVertexBuffer::CleanUp()
{
	delete VertexData;
	VertexData = nullptr;
}

void FSkeletalMeshVertexBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if( ResourceArray->GetResourceDataSize() > 0 )
	{
		// Create the vertex buffer.
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		
		// BUF_ShaderResource is needed for support of the SkinCache (we could make is dependent on GEnableGPUSkinCacheShaders or are there other users?)
		VertexBufferRHI = RHICreateVertexBuffer( ResourceArray->GetResourceDataSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
		SRVValue = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_UINT);
	}
}

void FSkeletalMeshVertexBuffer::ReleaseRHI()
{
	FVertexBuffer::ReleaseRHI();

	SRVValue.SafeRelease();
}



/**
* Serializer for this class
* @param Ar - archive to serialize to
* @param B - data to serialize
*/
FArchive& operator<<(FArchive& Ar,FSkeletalMeshVertexBuffer& VertexBuffer)
{
	FStripDataFlags StripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);

	Ar << VertexBuffer.NumTexCoords;
	Ar << VertexBuffer.bUseFullPrecisionUVs;

	bool bBackCompatExtraBoneInfluences = false;

	if (Ar.UE4Ver() >= VER_UE4_SUPPORT_GPUSKINNING_8_BONE_INFLUENCES && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::UseSeparateSkinWeightBuffer)
	{
		Ar << bBackCompatExtraBoneInfluences;
	}

	// Serialize MeshExtension and Origin
	// I need to save them for console to pick it up later
	Ar << VertexBuffer.MeshExtension << VertexBuffer.MeshOrigin;

	if( Ar.IsLoading() )
	{
		// allocate vertex data on load
		VertexBuffer.AllocateData();
	}

	// if Ar is counting, it still should serialize. Need to count VertexData
	if (!StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory())
	{
		// Special handling for loading old content
		if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::UseSeparateSkinWeightBuffer)
		{
			int32 ElementSize;
			Ar << ElementSize;

			int32 ArrayNum;
			Ar << ArrayNum;

			TArray<uint8> DummyBytes;
			DummyBytes.AddUninitialized(ElementSize * ArrayNum);
			Ar.Serialize(DummyBytes.GetData(), ElementSize * ArrayNum);
		}
		else
		{
			if (VertexBuffer.VertexData != NULL)
			{
				VertexBuffer.VertexData->Serialize(Ar);

				// update cached buffer info
				VertexBuffer.NumVertices = VertexBuffer.VertexData->GetNumVertices();
				VertexBuffer.Data = (VertexBuffer.NumVertices > 0) ? VertexBuffer.VertexData->GetDataPointer() : nullptr;
				VertexBuffer.Stride = VertexBuffer.VertexData->GetStride();
			}
		}
	}

	return Ar;
}

/**
* Initializes the buffer with the given vertices.
* @param InVertices - The vertices to initialize the buffer with.
*/
void FSkeletalMeshVertexBuffer::Init(const TArray<FSoftSkinVertex>& InVertices)
{
	// Make sure if this is console, use compressed otherwise, use not compressed
	AllocateData();
	
	VertexData->ResizeBuffer(InVertices.Num());
	
	if (InVertices.Num() > 0)
	{
		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
		NumVertices = VertexData->GetNumVertices();
	}
	
	for( int32 VertIdx=0; VertIdx < InVertices.Num(); VertIdx++ )
	{
		const FSoftSkinVertex& SrcVertex = InVertices[VertIdx];
		SetVertexFast(VertIdx,SrcVertex);
	}
}

void FSkeletalMeshVertexBuffer::SetNeedsCPUAccess(bool bInNeedsCPUAccess)
{
	bNeedsCPUAccess = bInNeedsCPUAccess;
}

// Handy macro for allocating the correct vertex data class (which has to be known at compile time) depending on the data type and number of UVs.  
#define ALLOCATE_VERTEX_DATA_TEMPLATE( VertexDataType, NumUVs )											\
	switch(NumUVs)																						\
	{																									\
		case 1: VertexData = new TSkeletalMeshVertexData< VertexDataType<1> >(bNeedsCPUAccess); break;	\
		case 2: VertexData = new TSkeletalMeshVertexData< VertexDataType<2> >(bNeedsCPUAccess); break;	\
		case 3: VertexData = new TSkeletalMeshVertexData< VertexDataType<3> >(bNeedsCPUAccess); break;	\
		case 4: VertexData = new TSkeletalMeshVertexData< VertexDataType<4> >(bNeedsCPUAccess); break;	\
		default: UE_LOG(LogSkeletalMesh, Fatal,TEXT("Invalid number of texture coordinates"));								\
	}																									\

/** 
* Allocates the vertex data storage type. 
*/
void FSkeletalMeshVertexBuffer::AllocateData()
{
	// Clear any old VertexData before allocating.
	CleanUp();

	if( !bUseFullPrecisionUVs )
	{
		ALLOCATE_VERTEX_DATA_TEMPLATE( TGPUSkinVertexFloat16Uvs, NumTexCoords );
	}
	else
	{
		ALLOCATE_VERTEX_DATA_TEMPLATE( TGPUSkinVertexFloat32Uvs, NumTexCoords );
	}
}

void FSkeletalMeshVertexBuffer::SetVertexFast(uint32 VertexIndex,const FSoftSkinVertex& SrcVertex)
{
	checkSlow(VertexIndex < GetNumVertices());
	auto* VertBase = (TGPUSkinVertexBase*)(Data + VertexIndex * Stride);
	VertBase->TangentX = SrcVertex.TangentX;
	VertBase->TangentZ = SrcVertex.TangentZ;
	// store the sign of the determinant in TangentZ.W
	VertBase->TangentZ.Vector.W = GetBasisDeterminantSignByte( SrcVertex.TangentX, SrcVertex.TangentY, SrcVertex.TangentZ );
	if( !bUseFullPrecisionUVs )
	{
		auto* Vertex = (TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS>*)VertBase;
		Vertex->Position = SrcVertex.Position;
		for( uint32 UVIndex = 0; UVIndex < NumTexCoords; ++UVIndex )
		{
			Vertex->UVs[UVIndex] = FVector2DHalf( SrcVertex.UVs[UVIndex] );
		}
	}
	else
	{
		auto* Vertex = (TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS>*)VertBase;
		Vertex->Position = SrcVertex.Position;
		for( uint32 UVIndex = 0; UVIndex < NumTexCoords; ++UVIndex )
		{
			Vertex->UVs[UVIndex] = FVector2D( SrcVertex.UVs[UVIndex] );
		}
	}
}

/**
* Convert the existing data in this mesh from 16 bit to 32 bit UVs.
* Without rebuilding the mesh (loss of precision)
*/
template<uint32 NumTexCoordsT>
void FSkeletalMeshVertexBuffer::ConvertToFullPrecisionUVsTyped()
{
	if( !bUseFullPrecisionUVs )
	{
		TArray< TGPUSkinVertexFloat32Uvs<NumTexCoordsT> > DestVertexData;
		TSkeletalMeshVertexData< TGPUSkinVertexFloat16Uvs<NumTexCoordsT> >& SrcVertexData = *(TSkeletalMeshVertexData< TGPUSkinVertexFloat16Uvs<NumTexCoordsT> >*)VertexData;			
		DestVertexData.AddUninitialized(SrcVertexData.Num());
		for( int32 VertIdx=0; VertIdx < SrcVertexData.Num(); VertIdx++ )
		{
			TGPUSkinVertexFloat16Uvs<NumTexCoordsT>& SrcVert = SrcVertexData[VertIdx];
			TGPUSkinVertexFloat32Uvs<NumTexCoordsT>& DestVert = DestVertexData[VertIdx];
			FMemory::Memcpy(&DestVert,&SrcVert,sizeof(TGPUSkinVertexBase));
			DestVert.Position = SrcVert.Position;
			for( uint32 UVIndex = 0; UVIndex < NumTexCoords; ++UVIndex )
			{
				DestVert.UVs[UVIndex] = FVector2D(SrcVert.UVs[UVIndex]);
			}

		}

		bUseFullPrecisionUVs = true;
		*this = DestVertexData;
	}
}


/*-----------------------------------------------------------------------------
FSkeletalMeshVertexAPEXClothBuffer
-----------------------------------------------------------------------------*/

/**
 * Constructor
 */
FSkeletalMeshVertexClothBuffer::FSkeletalMeshVertexClothBuffer() 
:	VertexData(nullptr),
	Data(nullptr),
	Stride(0),
	NumVertices(0)
{

}

/**
 * Destructor
 */
FSkeletalMeshVertexClothBuffer::~FSkeletalMeshVertexClothBuffer()
{
	// clean up everything
	CleanUp();
}

/**
 * Assignment. Assumes that vertex buffer will be rebuilt 
 */

FSkeletalMeshVertexClothBuffer& FSkeletalMeshVertexClothBuffer::operator=(const FSkeletalMeshVertexClothBuffer& Other)
{
	CleanUp();
	return *this;
}

/**
 * Copy Constructor
 */
FSkeletalMeshVertexClothBuffer::FSkeletalMeshVertexClothBuffer(const FSkeletalMeshVertexClothBuffer& Other)
:	VertexData(nullptr),
	Data(nullptr),
	Stride(0),
	NumVertices(0)
{

}

/**
 * @return text description for the resource type
 */
FString FSkeletalMeshVertexClothBuffer::GetFriendlyName() const
{
	return TEXT("Skeletal-mesh vertex APEX cloth mesh-mesh mapping buffer");
}

/** 
 * Delete existing resources 
 */
void FSkeletalMeshVertexClothBuffer::CleanUp()
{
	delete VertexData;
	VertexData = nullptr;
}

/**
 * Initialize the RHI resource for this vertex buffer
 */
void FSkeletalMeshVertexClothBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if( ResourceArray->GetResourceDataSize() > 0 )
	{
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer( ResourceArray->GetResourceDataSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
		VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(FVector4), PF_R32G32B32A32_UINT);
	}
}

/**
 * Serializer for this class
 * @param Ar - archive to serialize to
 * @param B - data to serialize
 */
FArchive& operator<<( FArchive& Ar, FSkeletalMeshVertexClothBuffer& VertexBuffer )
{
	FStripDataFlags StripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);

	if( Ar.IsLoading() )
	{
		VertexBuffer.AllocateData();
	}

	if (!StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory())
	{
		if( VertexBuffer.VertexData != NULL )
		{
			VertexBuffer.VertexData->Serialize( Ar );

			// update cached buffer info
			VertexBuffer.NumVertices = VertexBuffer.VertexData->GetNumVertices();
			VertexBuffer.Data = (VertexBuffer.NumVertices > 0) ? VertexBuffer.VertexData->GetDataPointer() : nullptr;
			VertexBuffer.Stride = VertexBuffer.VertexData->GetStride();
		}

		if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) >= FSkeletalMeshCustomVersion::CompactClothVertexBuffer)
		{
			Ar << VertexBuffer.ClothIndexMapping;
		}
	}

	return Ar;
}






/**
 * Initializes the buffer with the given vertices.
 * @param InVertices - The vertices to initialize the buffer with.
 */
void FSkeletalMeshVertexClothBuffer::Init(const TArray<FMeshToMeshVertData>& InMappingData, const TArray<uint64>& InClothIndexMapping)
{
	// Allocate new data
	AllocateData();

	// Resize the buffer to hold enough data for all passed in vertices
	VertexData->ResizeBuffer( InMappingData.Num() );

	Data = VertexData->GetDataPointer();
	Stride = VertexData->GetStride();
	NumVertices = VertexData->GetNumVertices();

	// Copy the vertices into the buffer.
	checkSlow(Stride*NumVertices == sizeof(FMeshToMeshVertData) * InMappingData.Num());
	//appMemcpy(Data, &InMappingData(0), Stride*NumVertices);
	for(int32 Index = 0;Index < InMappingData.Num();Index++)
	{
		const FMeshToMeshVertData& SourceMapping = InMappingData[Index];
		const int32 DestVertexIndex = Index;
		MappingData(DestVertexIndex) = SourceMapping;
	}
	ClothIndexMapping = InClothIndexMapping;
}

/** 
 * Allocates the vertex data storage type. 
 */
void FSkeletalMeshVertexClothBuffer::AllocateData()
{
	CleanUp();

	VertexData = new TSkeletalMeshVertexData<FMeshToMeshVertData>(true);
}


/*-----------------------------------------------------------------------------
FGPUSkinVertexBase
-----------------------------------------------------------------------------*/

/**
* Serializer
*
* @param Ar - archive to serialize with
*/
void TGPUSkinVertexBase::Serialize(FArchive& Ar)
{
	Ar << TangentX;
	Ar << TangentZ;
}

/*-----------------------------------------------------------------------------
	FSoftSkinVertex
-----------------------------------------------------------------------------*/

/**
* Serializer
*
* @param Ar - archive to serialize with
* @param V - vertex to serialize
* @return archive that was used
*/
FArchive& operator<<(FArchive& Ar,FSoftSkinVertex& V)
{
	Ar << V.Position;
	Ar << V.TangentX << V.TangentY << V.TangentZ;

	for( int32 UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx )
	{
		Ar << V.UVs[UVIdx];
	}

	Ar << V.Color;

	// serialize bone and weight uint8 arrays in order
	// this is required when serializing as bulk data memory (see TArray::BulkSerialize notes)
	for(uint32 InfluenceIndex = 0;InfluenceIndex < MAX_INFLUENCES_PER_STREAM;InfluenceIndex++)
	{
		Ar << V.InfluenceBones[InfluenceIndex];
	}

	if (Ar.UE4Ver() >= VER_UE4_SUPPORT_8_BONE_INFLUENCES_SKELETAL_MESHES)
	{
		for(uint32 InfluenceIndex = MAX_INFLUENCES_PER_STREAM;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
		{
			Ar << V.InfluenceBones[InfluenceIndex];
		}
	}
	else
	{
		if (Ar.IsLoading())
		{
			for(uint32 InfluenceIndex = MAX_INFLUENCES_PER_STREAM;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
			{
				V.InfluenceBones[InfluenceIndex] = 0;
			}
		}
	}

	for(uint32 InfluenceIndex = 0;InfluenceIndex < MAX_INFLUENCES_PER_STREAM;InfluenceIndex++)
	{
		Ar << V.InfluenceWeights[InfluenceIndex];
	}

	if (Ar.UE4Ver() >= VER_UE4_SUPPORT_8_BONE_INFLUENCES_SKELETAL_MESHES)
	{
		for(uint32 InfluenceIndex = MAX_INFLUENCES_PER_STREAM;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
		{
			Ar << V.InfluenceWeights[InfluenceIndex];
		}
	}
	else
	{
		if (Ar.IsLoading())
		{
			for(uint32 InfluenceIndex = MAX_INFLUENCES_PER_STREAM;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
			{
				V.InfluenceWeights[InfluenceIndex] = 0;
			}
		}
	}

	return Ar;
}

bool FSoftSkinVertex::GetRigidWeightBone(uint8& OutBoneIndex) const
{
	bool bIsRigid = false;

	for (int32 WeightIdx = 0; WeightIdx < MAX_TOTAL_INFLUENCES; WeightIdx++)
	{
		if (InfluenceWeights[WeightIdx] == 255)
		{
			bIsRigid = true;
			OutBoneIndex = InfluenceBones[WeightIdx];
			break;
		}
	}

	return bIsRigid;
}

uint8 FSoftSkinVertex::GetMaximumWeight() const
{
	uint8 MaxInfluenceWeight = 0;

	for ( int32 Index = 0; Index < MAX_TOTAL_INFLUENCES; Index++ )
	{
		const uint8 Weight = InfluenceWeights[Index];

		if ( Weight > MaxInfluenceWeight )
		{
			MaxInfluenceWeight = Weight;
		}
	}

	return MaxInfluenceWeight;
}

/*-----------------------------------------------------------------------------
	FMultiSizeIndexBuffer
-------------------------------------------------------------------------------*/
FMultiSizeIndexContainer::~FMultiSizeIndexContainer()
{
	if (IndexBuffer)
	{
		delete IndexBuffer;
	}
}

/**
 * Initialize the index buffer's render resources.
 */
void FMultiSizeIndexContainer::InitResources()
{
	check(IsInGameThread());
	if( IndexBuffer )
	{
		BeginInitResource( IndexBuffer );
	}
}

/**
 * Releases the index buffer's render resources.
 */	
void FMultiSizeIndexContainer::ReleaseResources()
{
	check(IsInGameThread());
	if( IndexBuffer )
	{
		BeginReleaseResource( IndexBuffer );
	}
}

/**
 * Creates a new index buffer
 */
void FMultiSizeIndexContainer::CreateIndexBuffer(uint8 InDataTypeSize)
{
	check( IndexBuffer == NULL );
	bool bNeedsCPUAccess = true;

	DataTypeSize = InDataTypeSize;

	if (InDataTypeSize == sizeof(uint16))
	{
		IndexBuffer = new FRawStaticIndexBuffer16or32<uint16>(bNeedsCPUAccess);
	}
	else
	{
#if !DISALLOW_32BIT_INDICES
		IndexBuffer = new FRawStaticIndexBuffer16or32<uint32>(bNeedsCPUAccess);
#else
		UE_LOG(LogSkeletalMesh, Fatal, TEXT("When DISALLOW_32BIT_INDICES is defined, 32 bit indices should not be used") );
#endif
	}
}

/**
 * Repopulates the index buffer
 */
void FMultiSizeIndexContainer::RebuildIndexBuffer( const FMultiSizeIndexContainerData& InData )
{
	bool bNeedsCPUAccess = true;

	if( IndexBuffer )
	{
		delete IndexBuffer;
	}
	DataTypeSize = InData.DataTypeSize;

	if (DataTypeSize == sizeof(uint16))
	{
		IndexBuffer = new FRawStaticIndexBuffer16or32<uint16>(bNeedsCPUAccess);
	}
	else
	{
#if !DISALLOW_32BIT_INDICES
		IndexBuffer = new FRawStaticIndexBuffer16or32<uint32>(bNeedsCPUAccess);
#else
		UE_LOG(LogSkeletalMesh, Fatal, TEXT("When DISALLOW_32BIT_INDICES is defined, 32 bit indices should not be used") );
#endif
	}

	CopyIndexBuffer( InData.Indices );
}

/**
 * Returns a 32 bit version of the index buffer
 */
void FMultiSizeIndexContainer::GetIndexBuffer( TArray<uint32>& OutArray ) const
{
	check( IndexBuffer );

	OutArray.Reset();
	int32 NumIndices = IndexBuffer->Num();
	OutArray.AddUninitialized( NumIndices );

	for (int32 I = 0; I < NumIndices; ++I)
	{
		OutArray[I] = IndexBuffer->Get(I);
	}
}

/**
 * Populates the index buffer with a new set of indices
 */
void FMultiSizeIndexContainer::CopyIndexBuffer(const TArray<uint32>& NewArray)
{
	check( IndexBuffer );

	// On console the resource arrays can't have items added directly to them
	if (FPlatformProperties::HasEditorOnlyData() == false)
	{
		if (DataTypeSize == sizeof(uint16))
		{
			TArray<uint16> WordArray;
			for (int32 i = 0; i < NewArray.Num(); ++i)
			{
				WordArray.Add((uint16)NewArray[i]);
			}

			((FRawStaticIndexBuffer16or32<uint16>*)IndexBuffer)->AssignNewBuffer(WordArray);
		}
		else
		{
			((FRawStaticIndexBuffer16or32<uint32>*)IndexBuffer)->AssignNewBuffer(NewArray);
		}
	}
	else
	{
		IndexBuffer->Empty();
		for (int32 i = 0; i < NewArray.Num(); ++i)
		{
#if WITH_EDITOR
			if(DataTypeSize == sizeof(uint16) && NewArray[i] > MAX_uint16)
			{
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Attempting to copy %u into a uint16 index buffer - this value will overflow to %u, use RebuildIndexBuffer to create a uint32 index buffer!"), NewArray[i], (uint16)NewArray[i]);
			}
#endif
			IndexBuffer->AddItem(NewArray[i]);
		}
	}
}

void FMultiSizeIndexContainer::Serialize(FArchive& Ar, bool bNeedsCPUAccess)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FMultiSizeIndexContainer::Serialize"), STAT_MultiSizeIndexContainer_Serialize, STATGROUP_LoadTime );
	if (Ar.UE4Ver() < VER_UE4_KEEP_SKEL_MESH_INDEX_DATA)
	{
		bool bOldNeedsCPUAccess = true;
		Ar << bOldNeedsCPUAccess;
	}
	Ar << DataTypeSize;

	if (!IndexBuffer)
	{
		if (DataTypeSize == sizeof(uint16))
		{
			IndexBuffer = new FRawStaticIndexBuffer16or32<uint16>(bNeedsCPUAccess);
		}
		else
		{
#if !DISALLOW_32BIT_INDICES
			IndexBuffer = new FRawStaticIndexBuffer16or32<uint32>(bNeedsCPUAccess);
#else
			UE_LOG(LogSkeletalMesh, Fatal, TEXT("When DISALLOW_32BIT_INDICES is defined, 32 bit indices should not be used") );
#endif
		}
	}
	
	IndexBuffer->Serialize( Ar );
}

#if WITH_EDITOR
/**
 * Retrieves index buffer related data
 */
void FMultiSizeIndexContainer::GetIndexBufferData( FMultiSizeIndexContainerData& OutData ) const
{
	OutData.DataTypeSize = DataTypeSize;
	GetIndexBuffer( OutData.Indices );
}

FMultiSizeIndexContainer::FMultiSizeIndexContainer(const FMultiSizeIndexContainer& Other)
: DataTypeSize(sizeof(uint16))
, IndexBuffer(nullptr)
{
	// Cant copy this index buffer, assumes it will be rebuilt later
	IndexBuffer = nullptr;
}

FMultiSizeIndexContainer& FMultiSizeIndexContainer::operator=(const FMultiSizeIndexContainer& Buffer)
{
	// Cant copy this index buffer.  Delete the index buffer type.
	// assumes it will be rebuilt later
	if( IndexBuffer )
	{
		delete IndexBuffer;
		IndexBuffer = nullptr;
	}

	return *this;
}
#endif


/*-----------------------------------------------------------------------------
	FSkelMeshSection
-----------------------------------------------------------------------------*/

// Custom serialization version for RecomputeTangent
struct FRecomputeTangentCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
		// We serialize the RecomputeTangent Option
		RuntimeRecomputeTangent = 1,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FRecomputeTangentCustomVersion() {}
};

const FGuid FRecomputeTangentCustomVersion::GUID(0x5579F886, 0x933A4C1F, 0x83BA087B, 0x6361B92F);
// Register the custom version with core
FCustomVersionRegistration GRegisterRecomputeTangentCustomVersion(FRecomputeTangentCustomVersion::GUID, FRecomputeTangentCustomVersion::LatestVersion, TEXT("RecomputeTangentCustomVer"));

/** Legacy 'rigid' skin vertex */
struct FLegacyRigidSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX,	// Tangent, U-direction
		TangentY,	// Binormal, V-direction
		TangentZ;	// Normal
	FVector2D		UVs[MAX_TEXCOORDS]; // UVs
	FColor			Color;		// Vertex color.
	uint8			Bone;

	friend FArchive& operator<<(FArchive& Ar, FLegacyRigidSkinVertex& V)
	{
		Ar << V.Position;
		Ar << V.TangentX << V.TangentY << V.TangentZ;

		for (int32 UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx)
		{
			Ar << V.UVs[UVIdx];
		}

		Ar << V.Color;
		Ar << V.Bone;

		return Ar;
	}

	/** Util to convert from legacy */
	void ConvertToSoftVert(FSoftSkinVertex& DestVertex)
	{
		DestVertex.Position = Position;
		DestVertex.TangentX = TangentX;
		DestVertex.TangentY = TangentY;
		DestVertex.TangentZ = TangentZ;
		// store the sign of the determinant in TangentZ.W
		DestVertex.TangentZ.Vector.W = GetBasisDeterminantSignByte(TangentX, TangentY, TangentZ);

		// copy all texture coordinate sets
		FMemory::Memcpy(DestVertex.UVs, UVs, sizeof(FVector2D)*MAX_TEXCOORDS);

		DestVertex.Color = Color;
		DestVertex.InfluenceBones[0] = Bone;
		DestVertex.InfluenceWeights[0] = 255;
		for (int32 InfluenceIndex = 1; InfluenceIndex < MAX_TOTAL_INFLUENCES; InfluenceIndex++)
		{
			DestVertex.InfluenceBones[InfluenceIndex] = 0;
			DestVertex.InfluenceWeights[InfluenceIndex] = 0;
		}
	}
};



// Serialization.
FArchive& operator<<(FArchive& Ar,FSkelMeshSection& S)
{
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

	// When data is cooked for server platform some of the
	// variables are not serialized so that they're always
	// set to their initial values (for safety)
	FStripDataFlags StripFlags( Ar );

	Ar << S.MaterialIndex;

	Ar.UsingCustomVersion(FSkeletalMeshCustomVersion::GUID);
	if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSectionWithChunk)
	{
		uint16 DummyChunkIndex;
		Ar << DummyChunkIndex;
	}

	if (!StripFlags.IsDataStrippedForServer())
	{
		Ar << S.BaseIndex;
	}
		
	if (!StripFlags.IsDataStrippedForServer())
	{
		Ar << S.NumTriangles;
	}
		
	Ar << S.TriangleSorting;

	// for clothing info
	if( Ar.UE4Ver() >= VER_UE4_APEX_CLOTH )
	{
		Ar << S.bDisabled;
		Ar << S.CorrespondClothSectionIndex;
	}

	if( Ar.UE4Ver() >= VER_UE4_APEX_CLOTH_LOD )
	{
		Ar << S.bEnableClothLOD_DEPRECATED;
	}

	Ar.UsingCustomVersion(FRecomputeTangentCustomVersion::GUID);
	if (Ar.CustomVer(FRecomputeTangentCustomVersion::GUID) >= FRecomputeTangentCustomVersion::RuntimeRecomputeTangent)
	{
		Ar << S.bRecomputeTangent;
	}

	if (Ar.CustomVer(FEditorObjectVersion::GUID) >= FEditorObjectVersion::RefactorMeshEditorMaterials)
	{
		Ar << S.bCastShadow;
	}
	else
	{
		S.bCastShadow = true;
	}

	if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) >= FSkeletalMeshCustomVersion::CombineSectionWithChunk)
	{

		if (!StripFlags.IsDataStrippedForServer())
		{
			// This is so that BaseVertexIndex is never set to anything else that 0 (for safety)
			Ar << S.BaseVertexIndex;
		}

		if (!StripFlags.IsEditorDataStripped())
		{
			// For backwards compat, read rigid vert array into array
			TArray<FLegacyRigidSkinVertex> LegacyRigidVertices;
			if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
			{
				Ar << LegacyRigidVertices;
			}

			Ar << S.SoftVertices;

			// Once we have read in SoftVertices, convert and insert legacy rigid verts (if present) at start
			const int32 NumRigidVerts = LegacyRigidVertices.Num();
			if (NumRigidVerts > 0 && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
			{
				S.SoftVertices.InsertUninitialized(0, NumRigidVerts);

				for (int32 VertIdx = 0; VertIdx < NumRigidVerts; VertIdx++)
				{
					LegacyRigidVertices[VertIdx].ConvertToSoftVert(S.SoftVertices[VertIdx]);
				}
			}
		}

		// If loading content newer than CombineSectionWithChunk but older than SaveNumVertices, update NumVertices here
		if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::SaveNumVertices)
		{
			if (!StripFlags.IsDataStrippedForServer())
			{
				S.NumVertices = S.SoftVertices.Num();
			}
			else
			{
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Cannot set FSkelMeshSection::NumVertices for older content, loading in non-editor build."));
				S.NumVertices = 0;
			}
		}

		Ar << S.BoneMap;

		if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) >= FSkeletalMeshCustomVersion::SaveNumVertices)
		{
			Ar << S.NumVertices;
		}

		// Removed NumRigidVertices and NumSoftVertices
		if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
		{
			int32 DummyNumRigidVerts, DummyNumSoftVerts;
			Ar << DummyNumRigidVerts;
			Ar << DummyNumSoftVerts;

			if (DummyNumRigidVerts + DummyNumSoftVerts != S.SoftVertices.Num())
			{
				UE_LOG(LogSkeletalMesh, Error, TEXT("Legacy NumSoftVerts + NumRigidVerts != SoftVertices.Num()"));
			}
		}

		Ar << S.MaxBoneInfluences;

#if WITH_EDITOR
		// If loading content where we need to recalc 'max bone influences' instead of using loaded version, do that now
		if (!StripFlags.IsEditorDataStripped() && Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::RecalcMaxBoneInfluences)
		{
			S.CalcMaxBoneInfluences();
		}
#endif

		Ar << S.ClothMappingData;
		Ar << S.PhysicalMeshVertices;
		Ar << S.PhysicalMeshNormals;
		Ar << S.CorrespondClothAssetIndex;

		if(Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::NewClothingSystemAdded)
		{
			int16 DummyClothAssetSubmeshIndex;
			Ar << DummyClothAssetSubmeshIndex;
		}
		else
		{
			Ar << S.ClothingData;
		}
	}

	return Ar;
}

void FMorphTargetVertexInfoBuffers::InitRHI()
{
	check(NumTotalWorkItems > 0);

	{
		FRHIResourceCreateInfo CreateInfo;
		void* VertexIndicesVBData = nullptr;
		VertexIndicesVB = RHICreateAndLockVertexBuffer(VertexIndices.GetAllocatedSize(), BUF_Static | BUF_ShaderResource, CreateInfo, VertexIndicesVBData);
		FMemory::Memcpy(VertexIndicesVBData, VertexIndices.GetData(), VertexIndices.GetAllocatedSize());
		RHIUnlockVertexBuffer(VertexIndicesVB);
		VertexIndicesSRV = RHICreateShaderResourceView(VertexIndicesVB, 4, PF_R32_UINT);
	}
	{
		FRHIResourceCreateInfo CreateInfo;
		void* MorphDeltasVBData = nullptr;
		MorphDeltasVB = RHICreateAndLockVertexBuffer(MorphDeltas.GetAllocatedSize(), BUF_Static | BUF_ShaderResource, CreateInfo, MorphDeltasVBData);
		FMemory::Memcpy(MorphDeltasVBData, MorphDeltas.GetData(), MorphDeltas.GetAllocatedSize());
		RHIUnlockVertexBuffer(MorphDeltasVB);
		MorphDeltasSRV = RHICreateShaderResourceView(MorphDeltasVB, 2, PF_R16F);
	}

	VertexIndices.Empty();
	MorphDeltas.Empty();
}

void FMorphTargetVertexInfoBuffers::ReleaseRHI()
{
	VertexIndicesVB.SafeRelease();
	VertexIndicesSRV.SafeRelease();
	MorphDeltasVB.SafeRelease();
	MorphDeltasSRV.SafeRelease();
}

/*-----------------------------------------------------------------------------
	FStaticLODModel
-----------------------------------------------------------------------------*/

/** Legacy Chunk struct, now merged with FSkelMeshSection */
struct FLegacySkelMeshChunk
{
	uint32 BaseVertexIndex;
	TArray<FSoftSkinVertex> SoftVertices;
	TArray<FMeshToMeshVertData> ApexClothMappingData;
	TArray<FVector> PhysicalMeshVertices;
	TArray<FVector> PhysicalMeshNormals;
	TArray<FBoneIndexType> BoneMap;
	int32 MaxBoneInfluences;

	int16 CorrespondClothAssetIndex;
	int16 ClothAssetSubmeshIndex;

	FLegacySkelMeshChunk()
		: BaseVertexIndex(0)
		, MaxBoneInfluences(4)
		, CorrespondClothAssetIndex(INDEX_NONE)
		, ClothAssetSubmeshIndex(INDEX_NONE)
	{}

	void CopyToSection(FSkelMeshSection& Section)
	{
		Section.BaseVertexIndex = BaseVertexIndex;
		Section.SoftVertices = SoftVertices;
		Section.ClothMappingData = ApexClothMappingData;
		Section.PhysicalMeshVertices = PhysicalMeshVertices;
		Section.PhysicalMeshNormals = PhysicalMeshNormals;
		Section.BoneMap = BoneMap;
		Section.MaxBoneInfluences = MaxBoneInfluences;
		Section.CorrespondClothAssetIndex = CorrespondClothAssetIndex;
	}


	friend FArchive& operator<<(FArchive& Ar, FLegacySkelMeshChunk& C)
	{
		FStripDataFlags StripFlags(Ar);

		if (!StripFlags.IsDataStrippedForServer())
		{
			// This is so that BaseVertexIndex is never set to anything else that 0 (for safety)
			Ar << C.BaseVertexIndex;
		}
		if (!StripFlags.IsEditorDataStripped())
		{
			// For backwards compat, read rigid vert array into array
			TArray<FLegacyRigidSkinVertex> LegacyRigidVertices;
			if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
			{
				Ar << LegacyRigidVertices;
			}

			Ar << C.SoftVertices;

			// Once we have read in SoftVertices, convert and insert legacy rigid verts (if present) at start
			const int32 NumRigidVerts = LegacyRigidVertices.Num();
			if (NumRigidVerts > 0 && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
			{
				C.SoftVertices.InsertUninitialized(0, NumRigidVerts);

				for (int32 VertIdx = 0; VertIdx < NumRigidVerts; VertIdx++)
				{
					LegacyRigidVertices[VertIdx].ConvertToSoftVert(C.SoftVertices[VertIdx]);
				}
			}
		}
		Ar << C.BoneMap;

		// Removed NumRigidVertices and NumSoftVertices, just use array size
		if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSoftAndRigidVerts)
		{
			int32 DummyNumRigidVerts, DummyNumSoftVerts;
			Ar << DummyNumRigidVerts;
			Ar << DummyNumSoftVerts;

			if (DummyNumRigidVerts + DummyNumSoftVerts != C.SoftVertices.Num())
			{
				UE_LOG(LogSkeletalMesh, Error, TEXT("Legacy NumSoftVerts + NumRigidVerts != SoftVertices.Num()"));
			}
		}

		Ar << C.MaxBoneInfluences;


		if (Ar.UE4Ver() >= VER_UE4_APEX_CLOTH)
		{
			Ar << C.ApexClothMappingData;
			Ar << C.PhysicalMeshVertices;
			Ar << C.PhysicalMeshNormals;
			Ar << C.CorrespondClothAssetIndex;
			Ar << C.ClothAssetSubmeshIndex;
		}

		return Ar;
	}
};

void FStaticLODModel::Serialize( FArchive& Ar, UObject* Owner, int32 Idx )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FStaticLODModel::Serialize"), STAT_StaticLODModel_Serialize, STATGROUP_LoadTime );

	const uint8 LodAdjacencyStripFlag = 1;
	FStripDataFlags StripFlags( Ar, Ar.IsCooking() && !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::Tessellation) ? LodAdjacencyStripFlag : 0 );

	// Skeletal mesh buffers are kept in CPU memory after initialization to support merging of skeletal meshes.
	bool bKeepBuffersInCPUMemory = true;
#if !WITH_EDITOR
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FreeSkeletalMeshBuffers"));
	if(CVar)
	{
		bKeepBuffersInCPUMemory = !CVar->GetValueOnAnyThread();
	}
#endif

	if (StripFlags.IsDataStrippedForServer())
	{
		TArray<FSkelMeshSection> TempSections;
		Ar << TempSections;

		FMultiSizeIndexContainer	TempMultiSizeIndexContainer;
		TempMultiSizeIndexContainer.Serialize(Ar, bKeepBuffersInCPUMemory);

		TArray<FBoneIndexType> TempActiveBoneIndices;
		Ar << TempActiveBoneIndices;
	}
	else
	{
		Ar << Sections;
		MultiSizeIndexContainer.Serialize(Ar, bKeepBuffersInCPUMemory);
		Ar << ActiveBoneIndices;
	}

	// Array of Sections for backwards compat
	Ar.UsingCustomVersion(FSkeletalMeshCustomVersion::GUID);
	if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CombineSectionWithChunk)
	{
		TArray<FLegacySkelMeshChunk> LegacyChunks;

		Ar << LegacyChunks;

		check(LegacyChunks.Num() == Sections.Num());
		for (int32 ChunkIdx = 0; ChunkIdx < LegacyChunks.Num(); ChunkIdx++)
		{
			FSkelMeshSection& Section = Sections[ChunkIdx];

			LegacyChunks[ChunkIdx].CopyToSection(Section);

			// Set NumVertices for older content on load
			if (!StripFlags.IsDataStrippedForServer())
			{
				Section.NumVertices = Section.SoftVertices.Num();
			}
			else
			{
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Cannot set FSkelMeshSection::NumVertices for older content, loading in non-editor build."));
				Section.NumVertices = 0;
			}
		}
	}
	
	// no longer in use
	{
		uint32 LegacySize = 0;
		Ar << LegacySize;
	}

	if (!StripFlags.IsDataStrippedForServer())
	{
		Ar << NumVertices;
	}
	Ar << RequiredBones;

	if( !StripFlags.IsEditorDataStripped() )
	{
		RawPointIndices.Serialize( Ar, Owner );
	}

	if (StripFlags.IsDataStrippedForServer())
	{
		TArray<int32> TempMeshToImportVertexMap;
		Ar << TempMeshToImportVertexMap;

		int32 TempMaxImportVertex;
		Ar << TempMaxImportVertex;
	}
	else
	{
		Ar << MeshToImportVertexMap;
		Ar << MaxImportVertex;
	}

	if( !StripFlags.IsDataStrippedForServer() )
	{
		USkeletalMesh* SkelMeshOwner = CastChecked<USkeletalMesh>(Owner);

		if( Ar.IsLoading() )
		{
			// set cpu skinning flag on the vertex buffer so that the resource arrays know if they need to be CPU accessible
			bool bNeedsCPUAccess = bKeepBuffersInCPUMemory || SkelMeshOwner->GetImportedResource()->RequiresCPUSkinning(GMaxRHIFeatureLevel);
			VertexBufferGPUSkin.SetNeedsCPUAccess(bNeedsCPUAccess);
			SkinWeightVertexBuffer.SetNeedsCPUAccess(bNeedsCPUAccess);
		}
		Ar << NumTexCoords;
		Ar << VertexBufferGPUSkin;

		if (Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) >= FSkeletalMeshCustomVersion::UseSeparateSkinWeightBuffer)
		{
			Ar << SkinWeightVertexBuffer;
		}

		if( SkelMeshOwner->bHasVertexColors)
		{
			// Handling for old color buffer data
			if (Ar.IsLoading() && Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::UseSharedColorBufferFormat)
			{
				TArray<FColor> OldColors;
				FStripDataFlags LegacyColourStripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);
				OldColors.BulkSerialize(Ar);
			}
			else
			{
				ColorVertexBuffer.Serialize(Ar, bKeepBuffersInCPUMemory);
			}
		}

		if ( !StripFlags.IsClassDataStripped( LodAdjacencyStripFlag ) )
		{
			AdjacencyMultiSizeIndexContainer.Serialize(Ar,bKeepBuffersInCPUMemory);
		}

		if ( Ar.UE4Ver() >= VER_UE4_APEX_CLOTH && HasClothData() )
		{
			Ar << ClothVertexBuffer;
		}

		// validate sections and reset incorrect sorting mode
		if ( Ar.IsLoading() )
		{
			const int32 kNumIndicesPerPrimitive = 3;
			const int32 kNumSetsOfIndices = 2;
			for (int32 IdxSection = 0, PreLastSection = Sections.Num() - 1; IdxSection < PreLastSection; ++IdxSection)
			{
				FSkelMeshSection & Section = Sections[ IdxSection ];
				if (TRISORT_CustomLeftRight == Section.TriangleSorting)
				{
					uint32 IndicesInSection = Sections[ IdxSection + 1 ].BaseIndex - Section.BaseIndex;
					if (Section.NumTriangles * kNumIndicesPerPrimitive * kNumSetsOfIndices > IndicesInSection)
					{
						UE_LOG(LogSkeletalMesh, Warning, TEXT( "Section %d in LOD model %d of object %s doesn't have enough indices (%d, while %d are needed) to allow TRISORT_CustomLeftRight mode, resetting to TRISORT_None" ),
							IdxSection, Idx, *Owner->GetName(),
							IndicesInSection, Section.NumTriangles * kNumIndicesPerPrimitive * kNumSetsOfIndices
							);
						Section.TriangleSorting = TRISORT_None;
					}
				}
			}
			if (Sections.Num() > 0)
			{
				// last section is special case
				FSkelMeshSection & Section = Sections[Sections.Num() - 1];
				if (TRISORT_CustomLeftRight == Section.TriangleSorting)
				{
					uint32 IndicesInSection = MultiSizeIndexContainer.GetIndexBuffer()->Num() - Sections[Sections.Num() - 1].BaseIndex;
					if (Section.NumTriangles * kNumIndicesPerPrimitive * kNumSetsOfIndices > IndicesInSection)
					{
						UE_LOG(LogSkeletalMesh, Warning, TEXT("Section %d in LOD model %d of object %s doesn't have enough indices (%d, while %d are needed) to allow TRISORT_CustomLeftRight mode, resetting to TRISORT_None"),
							Sections.Num() - 1, Idx, *Owner->GetName(),
							IndicesInSection, Section.NumTriangles * kNumIndicesPerPrimitive * kNumSetsOfIndices
							);
						Section.TriangleSorting = TRISORT_None;
					}
				}
			}
		}
	}
}

void FStaticLODModel::InitResources(bool bNeedsVertexColors, int32 LODIndex, TArray<UMorphTarget*>& InMorphTargets)
{
	INC_DWORD_STAT_BY( STAT_SkeletalMeshIndexMemory, MultiSizeIndexContainer.IsIndexBufferValid() ? (MultiSizeIndexContainer.GetIndexBuffer()->Num() * MultiSizeIndexContainer.GetDataTypeSize()) : 0 );
	
	MultiSizeIndexContainer.InitResources();

	INC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, VertexBufferGPUSkin.GetVertexDataSize() );
	BeginInitResource(&VertexBufferGPUSkin);

	INC_DWORD_STAT_BY(STAT_SkeletalMeshVertexMemory, SkinWeightVertexBuffer.GetVertexDataSize());
	BeginInitResource(&SkinWeightVertexBuffer);

	if( bNeedsVertexColors )
	{	
		// Only init the color buffer if the mesh has vertex colors
		INC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, ColorVertexBuffer.GetAllocatedSize() );
		BeginInitResource(&ColorVertexBuffer);
	}

	if ( HasClothData() )
	{
		// Only init the color buffer if the mesh has vertex colors
		INC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, ClothVertexBuffer.GetVertexDataSize() );
		BeginInitResource(&ClothVertexBuffer);
	}

	if( RHISupportsTessellation(GMaxRHIShaderPlatform) ) 
	{
		AdjacencyMultiSizeIndexContainer.InitResources();
		INC_DWORD_STAT_BY( STAT_SkeletalMeshIndexMemory, AdjacencyMultiSizeIndexContainer.IsIndexBufferValid() ? (AdjacencyMultiSizeIndexContainer.GetIndexBuffer()->Num() * AdjacencyMultiSizeIndexContainer.GetDataTypeSize()) : 0 );
	}

	if (RHISupportsComputeShaders(GMaxRHIShaderPlatform) && InMorphTargets.Num() > 0)
	{
		MorphTargetVertexInfoBuffers.VertexIndices.Empty();
		MorphTargetVertexInfoBuffers.MorphDeltas.Empty();
		MorphTargetVertexInfoBuffers.WorkItemsPerMorph.Empty();
		MorphTargetVertexInfoBuffers.StartOffsetPerMorph.Empty();
		MorphTargetVertexInfoBuffers.MaximumValuePerMorph.Empty();
		MorphTargetVertexInfoBuffers.MinimumValuePerMorph.Empty();
		MorphTargetVertexInfoBuffers.NumTotalWorkItems = 0;

		// Populate the arrays to be filled in later in the render thread
		for (int32 AnimIdx = 0; AnimIdx < InMorphTargets.Num(); ++AnimIdx)
		{
			uint32 StartOffset = MorphTargetVertexInfoBuffers.NumTotalWorkItems;
			MorphTargetVertexInfoBuffers.StartOffsetPerMorph.Add(StartOffset);

			float MaximumValues[4] = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
			float MinimumValues[4] = { +FLT_MAX, +FLT_MAX, +FLT_MAX, +FLT_MAX };
			UMorphTarget* MorphTarget = InMorphTargets[AnimIdx];
			int32 NumSrcDeltas = 0;
			FMorphTargetDelta* MorphDeltas = MorphTarget->GetMorphTargetDelta(LODIndex, NumSrcDeltas);
			for (int32 DeltaIndex = 0; DeltaIndex < NumSrcDeltas; DeltaIndex++)
			{
				const auto& MorphDelta = MorphDeltas[DeltaIndex];
				// when import, we do check threshold, and also when adding weight, we do have threshold for how smaller weight can fit in
				// so no reason to check here another threshold
				MaximumValues[0] = FMath::Max(MaximumValues[0], MorphDelta.PositionDelta.X);
				MaximumValues[1] = FMath::Max(MaximumValues[1], MorphDelta.PositionDelta.Y);
				MaximumValues[2] = FMath::Max(MaximumValues[2], MorphDelta.PositionDelta.Z);
				MaximumValues[3] = FMath::Max(MaximumValues[3], FMath::Max(MorphDelta.TangentZDelta.X, FMath::Max(MorphDelta.TangentZDelta.Y, MorphDelta.TangentZDelta.Z)));

				MinimumValues[0] = FMath::Min(MinimumValues[0], MorphDelta.PositionDelta.X);
				MinimumValues[1] = FMath::Min(MinimumValues[1], MorphDelta.PositionDelta.Y);
				MinimumValues[2] = FMath::Min(MinimumValues[2], MorphDelta.PositionDelta.Z);
				MinimumValues[3] = FMath::Min(MinimumValues[3], FMath::Min(MorphDelta.TangentZDelta.X, FMath::Min(MorphDelta.TangentZDelta.Y, MorphDelta.TangentZDelta.Z)));

				MorphTargetVertexInfoBuffers.VertexIndices.Add(MorphDelta.SourceIdx);
				MorphTargetVertexInfoBuffers.MorphDeltas.Emplace(MorphDelta.PositionDelta, MorphDelta.TangentZDelta);
				MorphTargetVertexInfoBuffers.NumTotalWorkItems++;
			}

			uint32 MorphTargetSize =  MorphTargetVertexInfoBuffers.NumTotalWorkItems - StartOffset;
			if (MorphTargetSize > 0)
			{
				ensureMsgf(MaximumValues[0] < +32752.0f && MaximumValues[1] < +32752.0f && MaximumValues[2] < +32752.0f && MaximumValues[3] < +32752.0f, TEXT("Huge MorphTarget Delta found in %s at index %i, might break down because we use half float storage"), *MorphTarget->GetName(), AnimIdx);
				ensureMsgf(MinimumValues[0] > -32752.0f && MinimumValues[1] > -32752.0f && MinimumValues[2] > -32752.0f && MaximumValues[3] > -32752.0f, TEXT("Huge MorphTarget Delta found in %s at index %i, might break down because we use half float storage"), *MorphTarget->GetName(), AnimIdx);
			}
		
			MorphTargetVertexInfoBuffers.WorkItemsPerMorph.Add(MorphTargetSize);
			MorphTargetVertexInfoBuffers.MaximumValuePerMorph.Add(FVector4(MaximumValues[0], MaximumValues[1], MaximumValues[2], MaximumValues[3]));
			MorphTargetVertexInfoBuffers.MinimumValuePerMorph.Add(FVector4(MinimumValues[0], MinimumValues[1], MinimumValues[2], MinimumValues[3]));
		}

		check(MorphTargetVertexInfoBuffers.WorkItemsPerMorph.Num() == MorphTargetVertexInfoBuffers.StartOffsetPerMorph.Num());
		check(MorphTargetVertexInfoBuffers.WorkItemsPerMorph.Num() == MorphTargetVertexInfoBuffers.MaximumValuePerMorph.Num());
		check(MorphTargetVertexInfoBuffers.WorkItemsPerMorph.Num() == MorphTargetVertexInfoBuffers.MinimumValuePerMorph.Num());
		if (MorphTargetVertexInfoBuffers.NumTotalWorkItems > 0)
		{
			BeginInitResource(&MorphTargetVertexInfoBuffers);
		}
	}
}

void FStaticLODModel::ReleaseResources()
{
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshIndexMemory, MultiSizeIndexContainer.IsIndexBufferValid() ? (MultiSizeIndexContainer.GetIndexBuffer()->Num() * MultiSizeIndexContainer.GetDataTypeSize()) : 0 );
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshIndexMemory, AdjacencyMultiSizeIndexContainer.IsIndexBufferValid() ? (AdjacencyMultiSizeIndexContainer.GetIndexBuffer()->Num() * AdjacencyMultiSizeIndexContainer.GetDataTypeSize()) : 0 );
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, VertexBufferGPUSkin.GetVertexDataSize());
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, SkinWeightVertexBuffer.GetVertexDataSize());
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, ColorVertexBuffer.GetAllocatedSize() );
	DEC_DWORD_STAT_BY( STAT_SkeletalMeshVertexMemory, ClothVertexBuffer.GetVertexDataSize() );

	MultiSizeIndexContainer.ReleaseResources();
	AdjacencyMultiSizeIndexContainer.ReleaseResources();

	BeginReleaseResource(&VertexBufferGPUSkin);
	BeginReleaseResource(&SkinWeightVertexBuffer);
	BeginReleaseResource(&ColorVertexBuffer);
	BeginReleaseResource(&ClothVertexBuffer);
	BeginReleaseResource(&MorphTargetVertexInfoBuffers);
}

int32 FStaticLODModel::GetTotalFaces() const
{
	int32 TotalFaces = 0;
	for(int32 i=0; i<Sections.Num(); i++)
	{
		TotalFaces += Sections[i].NumTriangles;
	}

	return TotalFaces;
}

void FStaticLODModel::GetSectionFromVertexIndex(int32 InVertIndex, int32& OutSectionIndex, int32& OutVertIndex, bool& bOutHasExtraBoneInfluences) const
{
	OutSectionIndex = 0;
	OutVertIndex = 0;
	bOutHasExtraBoneInfluences = false;

	int32 VertCount = 0;

	// Iterate over each chunk
	for(int32 SectionCount = 0; SectionCount < Sections.Num(); SectionCount++)
	{
		const FSkelMeshSection& Section = Sections[SectionCount];
		OutSectionIndex = SectionCount;

		// Is it in Soft vertex range?
		if(InVertIndex < VertCount + Section.GetNumVertices())
		{
			OutVertIndex = InVertIndex - VertCount;
			bOutHasExtraBoneInfluences = SkinWeightVertexBuffer.HasExtraBoneInfluences();
			return;
		}
		VertCount += Section.GetNumVertices();
	}

	// InVertIndex should always be in some chunk!
	//check(false);
}

void FStaticLODModel::GetVertices(TArray<FSoftSkinVertex>& Vertices) const
{
	Vertices.Empty(NumVertices);
	Vertices.AddUninitialized(NumVertices);
		
	// Initialize the vertex data
	// All chunks are combined into one (rigid first, soft next)
	FSoftSkinVertex* DestVertex = (FSoftSkinVertex*)Vertices.GetData();
	for(int32 SectionIndex = 0; SectionIndex < Sections.Num(); SectionIndex++)
	{
		const FSkelMeshSection& Section = Sections[SectionIndex];
		FMemory::Memcpy(DestVertex, Section.SoftVertices.GetData(), Section.SoftVertices.Num() * sizeof(FSoftSkinVertex));
		DestVertex += Section.SoftVertices.Num();
	}
}

void FStaticLODModel::GetNonClothVertices(TArray<FSoftSkinVertex>& OutVertices) const
{
	// Get the number of sections to copy
	int32 NumSections = NumNonClothingSections();

	// Count number of verts
	int32 NumVertsToCopy = 0;
	for(int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
	{
		const FSkelMeshSection& Section = Sections[SectionIndex];
		NumVertsToCopy += Section.SoftVertices.Num();
	}

	OutVertices.Empty(NumVertsToCopy);
	OutVertices.AddUninitialized(NumVertsToCopy);

	// Initialize the vertex data
	// All chunks are combined into one (rigid first, soft next)
	FSoftSkinVertex* DestVertex = (FSoftSkinVertex*)OutVertices.GetData();
	for(int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
	{
		const FSkelMeshSection& Section = Sections[SectionIndex];
		FMemory::Memcpy(DestVertex, Section.SoftVertices.GetData(), Section.SoftVertices.Num() * sizeof(FSoftSkinVertex));
		DestVertex += Section.SoftVertices.Num();
	}
}

void FStaticLODModel::GetApexClothMappingData(TArray<FMeshToMeshVertData>& MappingData, TArray<uint64>& OutClothIndexMapping) const
{
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); SectionIndex++)
	{
		const FSkelMeshSection& Section = Sections[SectionIndex];
		if (Section.ClothMappingData.Num())
		{
			uint64 KeyValue = ((uint64)Section.BaseVertexIndex << (uint32)32) | (uint64)MappingData.Num();
			OutClothIndexMapping.Add(KeyValue);
			MappingData += Section.ClothMappingData;
		}
	}
}

void FStaticLODModel::BuildVertexBuffers(uint32 BuildFlags)
{
	bool bUseFullPrecisionUVs = (BuildFlags & EVertexFlags::UseFullPrecisionUVs) != 0;
	bool bHasVertexColors = (BuildFlags & EVertexFlags::HasVertexColors) != 0;

	TArray<FSoftSkinVertex> Vertices;
	GetVertices(Vertices);

	// match UV precision for mesh vertex buffer to setting from parent mesh
	VertexBufferGPUSkin.SetUseFullPrecisionUVs(bUseFullPrecisionUVs);
	// keep the buffer in CPU memory
	VertexBufferGPUSkin.SetNeedsCPUAccess(true);
	// Set the number of texture coordinate sets
	VertexBufferGPUSkin.SetNumTexCoords( NumTexCoords );
	// init vertex buffer with the vertex array
	VertexBufferGPUSkin.Init(Vertices);

	// Init skin weight buffer
	SkinWeightVertexBuffer.SetNeedsCPUAccess(true);
	SkinWeightVertexBuffer.SetHasExtraBoneInfluences(DoSectionsNeedExtraBoneInfluences());
	SkinWeightVertexBuffer.Init(Vertices);

	// Init the color buffer if this mesh has vertex colors.
	if( bHasVertexColors && Vertices.Num() > 0 && ColorVertexBuffer.GetAllocatedSize() == 0 )
	{
		ColorVertexBuffer.InitFromColorArray(&Vertices[0].Color, Vertices.Num(), sizeof(FSoftSkinVertex));
	}

	if( HasClothData() )
	{
		TArray<FMeshToMeshVertData> MappingData;
		TArray<uint64> ClothIndexMapping;
		GetApexClothMappingData(MappingData, ClothIndexMapping);
		ClothVertexBuffer.Init(MappingData, ClothIndexMapping);
	}
}

void FStaticLODModel::SortTriangles( FVector SortCenter, bool bUseSortCenter, int32 SectionIndex, ETriangleSortOption NewTriangleSorting )
{
#if WITH_EDITOR
	FSkelMeshSection& Section = Sections[SectionIndex];
	if( NewTriangleSorting == Section.TriangleSorting )
	{
		return;
	}

	if( NewTriangleSorting == TRISORT_CustomLeftRight )
	{
		// Make a second copy of index buffer data for this section
		int32 NumNewIndices = Section.NumTriangles*3;
		MultiSizeIndexContainer.GetIndexBuffer()->Insert(Section.BaseIndex, NumNewIndices);
		FMemory::Memcpy( MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(Section.BaseIndex), MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(Section.BaseIndex+NumNewIndices), NumNewIndices*MultiSizeIndexContainer.GetDataTypeSize() );

		// Fix up BaseIndex for indices in other sections
		for( int32 OtherSectionIdx = 0; OtherSectionIdx < Sections.Num(); OtherSectionIdx++ )
		{
			if( Sections[OtherSectionIdx].BaseIndex > Section.BaseIndex )
			{
				Sections[OtherSectionIdx].BaseIndex += NumNewIndices;
			}
		}
	}
	else if( Section.TriangleSorting == TRISORT_CustomLeftRight )
	{
		// Remove the second copy of index buffer data for this section
		int32 NumRemovedIndices = Section.NumTriangles*3;
		MultiSizeIndexContainer.GetIndexBuffer()->Remove(Section.BaseIndex, NumRemovedIndices);
		// Fix up BaseIndex for indices in other sections
		for( int32 OtherSectionIdx = 0; OtherSectionIdx < Sections.Num(); OtherSectionIdx++ )
		{
			if( Sections[OtherSectionIdx].BaseIndex > Section.BaseIndex )
			{
				Sections[OtherSectionIdx].BaseIndex -= NumRemovedIndices;
			}
		}
	}

	TArray<FSoftSkinVertex> Vertices;
	GetVertices(Vertices);

	switch( NewTriangleSorting )
	{
	case TRISORT_None:
		{
			TArray<uint32> Indices;
			MultiSizeIndexContainer.GetIndexBuffer( Indices );
			SortTriangles_None( Section.NumTriangles, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			MultiSizeIndexContainer.CopyIndexBuffer( Indices );
		}
		break;
	case TRISORT_CenterRadialDistance:
		{
			TArray<uint32> Indices;
			MultiSizeIndexContainer.GetIndexBuffer( Indices );
			if (bUseSortCenter)
			{
				SortTriangles_CenterRadialDistance( SortCenter, Section.NumTriangles, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			}
			else
			{
				SortTriangles_CenterRadialDistance( Section.NumTriangles, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			}
			MultiSizeIndexContainer.CopyIndexBuffer( Indices );
		}
		break;
	case TRISORT_Random:
		{
			TArray<uint32> Indices;
			MultiSizeIndexContainer.GetIndexBuffer( Indices );
			SortTriangles_Random( Section.NumTriangles, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			MultiSizeIndexContainer.CopyIndexBuffer( Indices );
		}
		break;
	case TRISORT_MergeContiguous:
		{
			TArray<uint32> Indices;
			MultiSizeIndexContainer.GetIndexBuffer( Indices );
			SortTriangles_MergeContiguous( Section.NumTriangles, NumVertices, Vertices.GetData(), Indices.GetData() + Section.BaseIndex );
			MultiSizeIndexContainer.CopyIndexBuffer( Indices );
		}
		break;
	case TRISORT_Custom:
	case TRISORT_CustomLeftRight:
		break;
	}

	Section.TriangleSorting = NewTriangleSorting;
#endif
}

void FStaticLODModel::ReleaseCPUResources()
{	
	if(!GIsEditor && !IsRunningCommandlet())
	{
		if(MultiSizeIndexContainer.IsIndexBufferValid())
		{
			MultiSizeIndexContainer.GetIndexBuffer()->Empty();
		}
		if(AdjacencyMultiSizeIndexContainer.IsIndexBufferValid())
		{
			AdjacencyMultiSizeIndexContainer.GetIndexBuffer()->Empty();
		}
		if(VertexBufferGPUSkin.IsVertexDataValid())
		{
			VertexBufferGPUSkin.CleanUp();
		}
		if (SkinWeightVertexBuffer.IsWeightDataValid())
		{
			SkinWeightVertexBuffer.CleanUp();
		}
	}
}

SIZE_T FStaticLODModel::GetResourceSize() const
{
	return GetResourceSizeBytes();
}

void FStaticLODModel::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
{
	CumulativeResourceSize.AddUnknownMemoryBytes(Sections.GetAllocatedSize());
	CumulativeResourceSize.AddUnknownMemoryBytes(ActiveBoneIndices.GetAllocatedSize()); 
	CumulativeResourceSize.AddUnknownMemoryBytes(RequiredBones.GetAllocatedSize());

	if(MultiSizeIndexContainer.IsIndexBufferValid())
	{
		const FRawStaticIndexBuffer16or32Interface* IndexBuffer = MultiSizeIndexContainer.GetIndexBuffer();
		if (IndexBuffer)
		{
			CumulativeResourceSize.AddUnknownMemoryBytes(IndexBuffer->GetResourceDataSize()); 
		}
	}

	if(AdjacencyMultiSizeIndexContainer.IsIndexBufferValid())
	{
		const FRawStaticIndexBuffer16or32Interface* AdjacentIndexBuffer = AdjacencyMultiSizeIndexContainer.GetIndexBuffer();
		if(AdjacentIndexBuffer)
		{
			CumulativeResourceSize.AddUnknownMemoryBytes(AdjacentIndexBuffer->GetResourceDataSize());
		}
	}

	CumulativeResourceSize.AddUnknownMemoryBytes(VertexBufferGPUSkin.GetVertexDataSize());
	CumulativeResourceSize.AddUnknownMemoryBytes(SkinWeightVertexBuffer.GetVertexDataSize());
	CumulativeResourceSize.AddUnknownMemoryBytes(ColorVertexBuffer.GetAllocatedSize());
	CumulativeResourceSize.AddUnknownMemoryBytes(ClothVertexBuffer.GetVertexDataSize());

	CumulativeResourceSize.AddUnknownMemoryBytes(RawPointIndices.GetBulkDataSize());
	CumulativeResourceSize.AddUnknownMemoryBytes(LegacyRawPointIndices.GetBulkDataSize());
	CumulativeResourceSize.AddUnknownMemoryBytes(MeshToImportVertexMap.GetAllocatedSize());

	// I suppose we add everything we could
	CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(int32));
}

SIZE_T FStaticLODModel::GetResourceSizeBytes() const
{
	FResourceSizeEx ResSize;
	GetResourceSizeEx(ResSize);
	return ResSize.GetTotalMemoryBytes();
}

void FStaticLODModel::RebuildIndexBuffer(FMultiSizeIndexContainerData* IndexBufferData, FMultiSizeIndexContainerData* AdjacencyIndexBufferData)
{
	if (IndexBufferData)
	{
		MultiSizeIndexContainer.RebuildIndexBuffer(*IndexBufferData);
	}

	if (AdjacencyIndexBufferData)
	{
		AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer(*AdjacencyIndexBufferData);
	}
}

#if WITH_EDITOR
void FStaticLODModel::RebuildIndexBuffer()
{
	// The index buffer needs to be rebuilt on copy.
	FMultiSizeIndexContainerData IndexBufferData;
	MultiSizeIndexContainer.GetIndexBufferData(IndexBufferData);

	FMultiSizeIndexContainerData AdjacencyIndexBufferData;
	AdjacencyMultiSizeIndexContainer.GetIndexBufferData(AdjacencyIndexBufferData);

	RebuildIndexBuffer(&IndexBufferData, &AdjacencyIndexBufferData);
}
#endif
/*-----------------------------------------------------------------------------
FStaticMeshSourceData
-----------------------------------------------------------------------------*/

/**
 * FSkeletalMeshSourceData - Source triangles and render data, editor-only.
 */
class FSkeletalMeshSourceData
{
public:
	FSkeletalMeshSourceData();
	~FSkeletalMeshSourceData();

#if WITH_EDITOR
	/** Initialize from static mesh render data. */
	void Init( const class USkeletalMesh* SkeletalMesh, FStaticLODModel& LODModel );

	/** Retrieve render data. */
	FORCEINLINE FStaticLODModel* GetModel() { return LODModel; }
#endif // #if WITH_EDITOR

#if WITH_EDITORONLY_DATA
	/** Free source data. */
	void Clear();
#endif // WITH_EDITORONLY_DATA

	/** Returns true if the source data has been initialized. */
	FORCEINLINE bool IsInitialized() const { return LODModel != NULL; }

	/** Serialization. */
	void Serialize( FArchive& Ar, USkeletalMesh* SkeletalMesh );

private:
	FStaticLODModel* LODModel;
};


FSkeletalMeshSourceData::FSkeletalMeshSourceData() : LODModel(nullptr)
{
}

FSkeletalMeshSourceData::~FSkeletalMeshSourceData()
{
	delete LODModel;
	LODModel = nullptr;
}

#if WITH_EDITOR

/** Initialize from static mesh render data. */
void FSkeletalMeshSourceData::Init( const USkeletalMesh* SkeletalMesh, FStaticLODModel& InLODModel )
{
	check( LODModel == NULL );

	/** Bulk data arrays need to be locked before a copy can be made. */
	InLODModel.RawPointIndices.Lock( LOCK_READ_ONLY );
	InLODModel.LegacyRawPointIndices.Lock( LOCK_READ_ONLY );

	/** Allocate a new LOD model to hold the data and copy everything over. */
	LODModel = new FStaticLODModel();
	*LODModel = InLODModel;

	/** Unlock the arrays as the copy has been made. */
	InLODModel.RawPointIndices.Unlock();
	InLODModel.LegacyRawPointIndices.Unlock();

	/** The index buffer needs to be rebuilt on copy. */
	FMultiSizeIndexContainerData IndexBufferData, AdjacencyIndexBufferData;
	InLODModel.MultiSizeIndexContainer.GetIndexBufferData( IndexBufferData );
	InLODModel.AdjacencyMultiSizeIndexContainer.GetIndexBufferData(AdjacencyIndexBufferData);
	LODModel->RebuildIndexBuffer( &IndexBufferData, &AdjacencyIndexBufferData );

	/** Vertex buffers also need to be rebuilt. Source data is always stored with full precision position data. */
	LODModel->BuildVertexBuffers(SkeletalMesh->GetVertexBufferFlags());
}

#endif // #if WITH_EDITOR

#if WITH_EDITORONLY_DATA

/** Free source data. */
void FSkeletalMeshSourceData::Clear()
{
	delete LODModel;
	LODModel = nullptr;
}

#endif // #if WITH_EDITORONLY_DATA

/** Serialization. */
void FSkeletalMeshSourceData::Serialize( FArchive& Ar, USkeletalMesh* SkeletalMesh )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FSkeletalMeshSourceData::Serialize"), STAT_SkeletalMeshSourceData_Serialize, STATGROUP_LoadTime );

	if ( Ar.IsLoading() )
	{
		bool bHaveSourceData = false;
		Ar << bHaveSourceData;
		if ( bHaveSourceData )
		{
			if (LODModel != nullptr)
			{
				delete LODModel;
				LODModel = nullptr;
			}
			LODModel = new FStaticLODModel();
			LODModel->Serialize( Ar, SkeletalMesh, INDEX_NONE );
		}
	}
	else
	{
		bool bHaveSourceData = IsInitialized();
		Ar << bHaveSourceData;
		if ( bHaveSourceData )
		{
			LODModel->Serialize( Ar, SkeletalMesh, INDEX_NONE );
		}
	}
}



/*-----------------------------------------------------------------------------
FreeSkeletalMeshBuffersSinkCallback
-----------------------------------------------------------------------------*/

void FreeSkeletalMeshBuffersSinkCallback()
{
	// If r.FreeSkeletalMeshBuffers==1 then CPU buffer copies are to be released.
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FreeSkeletalMeshBuffers"));
	bool bFreeSkeletalMeshBuffers = CVar->GetValueOnGameThread() == 1;
	if(bFreeSkeletalMeshBuffers)
	{
		FlushRenderingCommands();
		for (TObjectIterator<USkeletalMesh> It;It;++It)
		{
			if (!It->GetImportedResource()->RequiresCPUSkinning(GMaxRHIFeatureLevel))
			{
				It->ReleaseCPUResources();
			}
		}
	}
}

/*-----------------------------------------------------------------------------
USkeletalMesh
-----------------------------------------------------------------------------*/

/**
* Calculate max # of bone influences used by this skel mesh chunk
*/
void FSkelMeshSection::CalcMaxBoneInfluences()
{
	// if we only have rigid verts then there is only one bone
	MaxBoneInfluences = 1;
	// iterate over all the soft vertices for this chunk and find max # of bones used
	for( int32 VertIdx=0; VertIdx < SoftVertices.Num(); VertIdx++ )
	{
		FSoftSkinVertex& SoftVert = SoftVertices[VertIdx];

		// calc # of bones used by this soft skinned vertex
		int32 BonesUsed=0;
		for( int32 InfluenceIdx=0; InfluenceIdx < MAX_TOTAL_INFLUENCES; InfluenceIdx++ )
		{
			if( SoftVert.InfluenceWeights[InfluenceIdx] > 0 )
			{
				BonesUsed++;
			}
		}
		// reorder bones so that there aren't any unused influence entries within the [0,BonesUsed] range
		for( int32 InfluenceIdx=0; InfluenceIdx < BonesUsed; InfluenceIdx++ )
		{
			if( SoftVert.InfluenceWeights[InfluenceIdx] == 0 )
			{
				for( int32 ExchangeIdx=InfluenceIdx+1; ExchangeIdx < MAX_TOTAL_INFLUENCES; ExchangeIdx++ )
				{
					if( SoftVert.InfluenceWeights[ExchangeIdx] != 0 )
					{
						Exchange(SoftVert.InfluenceWeights[InfluenceIdx],SoftVert.InfluenceWeights[ExchangeIdx]);
						Exchange(SoftVert.InfluenceBones[InfluenceIdx],SoftVert.InfluenceBones[ExchangeIdx]);
						break;
					}
				}
			}
		}

		// maintain max bones used
		MaxBoneInfluences = FMath::Max(MaxBoneInfluences,BonesUsed);			
	}
}

/*-----------------------------------------------------------------------------
	FClothingAssetData
-----------------------------------------------------------------------------*/

FArchive& operator<<(FArchive& Ar, FClothingAssetData_Legacy& A)
{
	// Serialization to load and save ApexClothingAsset
	if( Ar.IsLoading() )
	{
		uint32 AssetSize;
		Ar << AssetSize;

		if( AssetSize > 0 )
		{
			// Load the binary blob data
			TArray<uint8> Buffer;
			Buffer.AddUninitialized( AssetSize );
			Ar.Serialize( Buffer.GetData(), AssetSize );
#if WITH_APEX_CLOTHING
			A.ApexClothingAsset = LoadApexClothingAssetFromBlob(Buffer);
#endif //#if WITH_APEX_CLOTHING
		}
	}
	else
	if( Ar.IsSaving() )
	{
#if WITH_APEX_CLOTHING
		if (A.ApexClothingAsset)
		{
			TArray<uint8> Buffer;
			SaveApexClothingAssetToBlob(A.ApexClothingAsset, Buffer);
			uint32 AssetSize = Buffer.Num();
			Ar << AssetSize;
			Ar.Serialize(Buffer.GetData(), AssetSize);
		}
		else
#endif// #if WITH_APEX_CLOTHING
		{
			uint32 AssetSize = 0;
			Ar << AssetSize;
		}
	}

	return Ar;
}

/*-----------------------------------------------------------------------------
	FSkeletalMeshResource
-----------------------------------------------------------------------------*/

FSkeletalMeshResource::FSkeletalMeshResource()
	: bInitialized(false)
{
}

void FSkeletalMeshResource::InitResources(bool bNeedsVertexColors, TArray<UMorphTarget*>& InMorphTargets)
{
	if (!bInitialized)
	{
		// initialize resources for each lod
		for( int32 LODIndex = 0;LODIndex < LODModels.Num();LODIndex++ )
		{
			LODModels[LODIndex].InitResources(bNeedsVertexColors, LODIndex, InMorphTargets);
		}
		bInitialized = true;
	}
}

void FSkeletalMeshResource::ReleaseResources()
{
	if (bInitialized)
	{
		// release resources for each lod
		for( int32 LODIndex = 0;LODIndex < LODModels.Num();LODIndex++ )
		{
			LODModels[LODIndex].ReleaseResources();
		}
		bInitialized = false;
	}
}

void FSkeletalMeshResource::Serialize(FArchive& Ar, USkeletalMesh* Owner)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("FSkeletalMeshResource::Serialize"), STAT_SkeletalMeshResource_Serialize, STATGROUP_LoadTime );

	LODModels.Serialize(Ar,Owner);
}

bool FSkeletalMeshResource::HasExtraBoneInfluences() const
{
	for (int32 LODIndex = 0; LODIndex < LODModels.Num(); ++LODIndex)
	{
		const FStaticLODModel& Model = LODModels[LODIndex];
		if (Model.DoSectionsNeedExtraBoneInfluences())
		{
			return true;
		}
	}

	return false;
}

int32 FSkeletalMeshResource::GetMaxBonesPerSection() const
{
	int32 MaxBonesPerSection = 0;
	for (int32 LODIndex = 0; LODIndex < LODModels.Num(); ++LODIndex)
	{
		const FStaticLODModel& Model = LODModels[LODIndex];
		for (int32 SectionIndex = 0; SectionIndex < Model.Sections.Num(); ++SectionIndex)
		{
			MaxBonesPerSection = FMath::Max<int32>(MaxBonesPerSection, Model.Sections[SectionIndex].BoneMap.Num());
		}
	}
	return MaxBonesPerSection;
}

bool FSkeletalMeshResource::RequiresCPUSkinning(ERHIFeatureLevel::Type FeatureLevel) const
{
	const int32 MaxGPUSkinBones = GetFeatureLevelMaxNumberOfBones(FeatureLevel);
	const int32 MaxBonesPerChunk = GetMaxBonesPerSection();
	// Do CPU skinning if we need too many bones per chunk, or if we have too many influences per vertex on lower end
	return (MaxBonesPerChunk > MaxGPUSkinBones) || (HasExtraBoneInfluences() && FeatureLevel < ERHIFeatureLevel::ES3_1);
}

SIZE_T FSkeletalMeshResource::GetResourceSize()
{
	return GetResourceSizeBytes();
}

void FSkeletalMeshResource::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	for(int32 LODIndex = 0; LODIndex < LODModels.Num(); ++LODIndex)
	{
		const FStaticLODModel& Model = LODModels[LODIndex];
		Model.GetResourceSizeEx(CumulativeResourceSize);
	}
}

SIZE_T FSkeletalMeshResource::GetResourceSizeBytes()
{
	FResourceSizeEx ResSize;
	GetResourceSizeEx(ResSize);
	return ResSize.GetTotalMemoryBytes();
}

#if WITH_EDITORONLY_DATA
void FSkeletalMeshResource::SyncUVChannelData(const TArray<FSkeletalMaterial>& ObjectData)
{
	TSharedPtr< TArray<FMeshUVChannelInfo> > UpdateData = TSharedPtr< TArray<FMeshUVChannelInfo> >(new TArray<FMeshUVChannelInfo>);
	UpdateData->Empty(ObjectData.Num());

	for (const FSkeletalMaterial& SkeletalMaterial : ObjectData)
	{
		UpdateData->Add(SkeletalMaterial.UVChannelData);
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SyncUVChannelData,
		FSkeletalMeshResource*, This, this,
		TSharedPtr< TArray<FMeshUVChannelInfo> >, Data, UpdateData,
		{
			FMemory::Memswap(&This->UVChannelDataPerMaterial, Data.Get(), sizeof(TArray<FMeshUVChannelInfo>));
		} );
}
#endif

FSkeletalMeshClothBuildParams::FSkeletalMeshClothBuildParams()
	: TargetAsset(nullptr)
	, TargetLod(INDEX_NONE)
	, bRemapParameters(false)
	, AssetName("Clothing")
	, LodIndex(0)
	, SourceSection(0)
	, bRemoveFromMesh(false)
	, PhysicsAsset(nullptr)
{

}


//#nv begin #Blast Ability to hide bones using a dynamic index buffer
/*-----------------------------------------------------------------------------
FDynamicLODModelOverride
-----------------------------------------------------------------------------*/

void FDynamicLODModelOverride::InitResources(const FStaticLODModel& InitialData)
{
	Sections.SetNum(InitialData.Sections.Num());
	for (int32 S = 0; S < Sections.Num(); S++)
	{
		Sections[S].BaseIndex = InitialData.Sections[S].BaseIndex;
		Sections[S].NumTriangles = InitialData.Sections[S].NumTriangles;
	}

	FMultiSizeIndexContainerData TempData;
	TempData.DataTypeSize = InitialData.MultiSizeIndexContainer.GetDataTypeSize();
	InitialData.MultiSizeIndexContainer.GetIndexBuffer(TempData.Indices);
	MultiSizeIndexContainer.RebuildIndexBuffer(TempData);

	INC_DWORD_STAT_BY(STAT_SkeletalMeshIndexMemory, MultiSizeIndexContainer.IsIndexBufferValid() ? (MultiSizeIndexContainer.GetIndexBuffer()->Num() * MultiSizeIndexContainer.GetDataTypeSize()) : 0);

	MultiSizeIndexContainer.InitResources();

	//Need to check if the data was stripped in cooking or not
	if (RHISupportsTessellation(GMaxRHIShaderPlatform) && InitialData.AdjacencyMultiSizeIndexContainer.IsIndexBufferValid())
	{
		TempData.DataTypeSize = InitialData.AdjacencyMultiSizeIndexContainer.GetDataTypeSize();
		InitialData.AdjacencyMultiSizeIndexContainer.GetIndexBuffer(TempData.Indices);
		AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer(TempData);

		AdjacencyMultiSizeIndexContainer.InitResources();
		INC_DWORD_STAT_BY(STAT_SkeletalMeshIndexMemory, AdjacencyMultiSizeIndexContainer.IsIndexBufferValid() ? (AdjacencyMultiSizeIndexContainer.GetIndexBuffer()->Num() * AdjacencyMultiSizeIndexContainer.GetDataTypeSize()) : 0);
	}
}

void FDynamicLODModelOverride::ReleaseResources()
{
	DEC_DWORD_STAT_BY(STAT_SkeletalMeshIndexMemory, MultiSizeIndexContainer.IsIndexBufferValid() ? (MultiSizeIndexContainer.GetIndexBuffer()->Num() * MultiSizeIndexContainer.GetDataTypeSize()) : 0);
	DEC_DWORD_STAT_BY(STAT_SkeletalMeshIndexMemory, AdjacencyMultiSizeIndexContainer.IsIndexBufferValid() ? (AdjacencyMultiSizeIndexContainer.GetIndexBuffer()->Num() * AdjacencyMultiSizeIndexContainer.GetDataTypeSize()) : 0);

	MultiSizeIndexContainer.ReleaseResources();
	AdjacencyMultiSizeIndexContainer.ReleaseResources();
}

void FDynamicLODModelOverride::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
{
	CumulativeResourceSize.AddUnknownMemoryBytes(Sections.GetAllocatedSize());
	if (MultiSizeIndexContainer.IsIndexBufferValid())
	{
		const FRawStaticIndexBuffer16or32Interface* IndexBuffer = MultiSizeIndexContainer.GetIndexBuffer();
		if (IndexBuffer)
		{
			CumulativeResourceSize.AddUnknownMemoryBytes(IndexBuffer->GetResourceDataSize());
		}
	}

	if (AdjacencyMultiSizeIndexContainer.IsIndexBufferValid())
	{
		const FRawStaticIndexBuffer16or32Interface* AdjacentIndexBuffer = AdjacencyMultiSizeIndexContainer.GetIndexBuffer();
		if (AdjacentIndexBuffer)
		{
			CumulativeResourceSize.AddUnknownMemoryBytes(AdjacentIndexBuffer->GetResourceDataSize());
		}
	}

	//Not sure why this is added but FStaticLODModel does it 
	CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(int32));
}

SIZE_T FDynamicLODModelOverride::GetResourceSizeBytes() const
{
	FResourceSizeEx ResSize;
	GetResourceSizeEx(ResSize);
	return ResSize.GetTotalMemoryBytes();
}


/*-----------------------------------------------------------------------------
FSkeletalMeshDynamicOverride
-----------------------------------------------------------------------------*/

void FSkeletalMeshDynamicOverride::InitResources(const FSkeletalMeshResource& InitialData)
{
	if (!bInitialized)
	{
		// initialize resources for each lod
		for (int32 LODIndex = 0; LODIndex < InitialData.LODModels.Num(); LODIndex++)
		{
			FDynamicLODModelOverride* LODModel = new FDynamicLODModelOverride();
			LODModels.Add(LODModel);
			LODModel->InitResources(InitialData.LODModels[LODIndex]);
		}
		bInitialized = true;
	}
}

void FSkeletalMeshDynamicOverride::ReleaseResources()
{
	if (bInitialized)
	{
		// release resources for each lod
		for (int32 LODIndex = 0; LODIndex < LODModels.Num(); LODIndex++)
		{
			LODModels[LODIndex].ReleaseResources();
		}
		bInitialized = false;
	}
}


void FSkeletalMeshDynamicOverride::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	for (int32 LODIndex = 0; LODIndex < LODModels.Num(); ++LODIndex)
	{
		const FDynamicLODModelOverride& Model = LODModels[LODIndex];
		Model.GetResourceSizeEx(CumulativeResourceSize);
	}
}

SIZE_T FSkeletalMeshDynamicOverride::GetResourceSizeBytes()
{
	FResourceSizeEx ResSize;
	GetResourceSizeEx(ResSize);
	return ResSize.GetTotalMemoryBytes();
}
//nv end

/*-----------------------------------------------------------------------------
	USkeletalMesh
-----------------------------------------------------------------------------*/
USkeletalMesh::USkeletalMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SkelMirrorAxis = EAxis::X;
	SkelMirrorFlipAxis = EAxis::Z;
#if WITH_EDITORONLY_DATA
	SelectedEditorSection = INDEX_NONE;
	SelectedEditorMaterial = INDEX_NONE;
#endif
	ImportedResource = MakeShareable(new FSkeletalMeshResource());
}

void USkeletalMesh::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
	Super::PostInitProperties();
}

FBoxSphereBounds USkeletalMesh::GetBounds()
{
	return ExtendedBounds;
}

FBoxSphereBounds USkeletalMesh::GetImportedBounds()
{
	return ImportedBounds;
}

void USkeletalMesh::SetImportedBounds(const FBoxSphereBounds& InBounds)
{
	ImportedBounds = InBounds;
	CalculateExtendedBounds();
}

void USkeletalMesh::SetPositiveBoundsExtension(const FVector& InExtension)
{
	PositiveBoundsExtension = InExtension;
	CalculateExtendedBounds();
}

void USkeletalMesh::SetNegativeBoundsExtension(const FVector& InExtension)
{
	NegativeBoundsExtension = InExtension;
	CalculateExtendedBounds();
}

void USkeletalMesh::CalculateExtendedBounds()
{
	FBoxSphereBounds CalculatedBounds = ImportedBounds;

	// Convert to Min and Max
	FVector Min = CalculatedBounds.Origin - CalculatedBounds.BoxExtent;
	FVector Max = CalculatedBounds.Origin + CalculatedBounds.BoxExtent;
	// Apply bound extensions
	Min -= NegativeBoundsExtension;
	Max += PositiveBoundsExtension;
	// Convert back to Origin, Extent and update SphereRadius
	CalculatedBounds.Origin = (Min + Max) / 2;
	CalculatedBounds.BoxExtent = (Max - Min) / 2;
	CalculatedBounds.SphereRadius = CalculatedBounds.BoxExtent.GetAbsMax();

	ExtendedBounds = CalculatedBounds;
}

void USkeletalMesh::ValidateBoundsExtension()
{
	FVector HalfExtent = ImportedBounds.BoxExtent;

	PositiveBoundsExtension.X = FMath::Clamp(PositiveBoundsExtension.X, -HalfExtent.X, MAX_flt);
	PositiveBoundsExtension.Y = FMath::Clamp(PositiveBoundsExtension.Y, -HalfExtent.Y, MAX_flt);
	PositiveBoundsExtension.Z = FMath::Clamp(PositiveBoundsExtension.Z, -HalfExtent.Z, MAX_flt);

	NegativeBoundsExtension.X = FMath::Clamp(NegativeBoundsExtension.X, -HalfExtent.X, MAX_flt);
	NegativeBoundsExtension.Y = FMath::Clamp(NegativeBoundsExtension.Y, -HalfExtent.Y, MAX_flt);
	NegativeBoundsExtension.Z = FMath::Clamp(NegativeBoundsExtension.Z, -HalfExtent.Z, MAX_flt);
}

void USkeletalMesh::AddClothingAsset(UClothingAssetBase* InNewAsset)
{
	// Check the outer is us
	if(InNewAsset && InNewAsset->GetOuter() == this)
	{
		// Ok this should be a correctly created asset, we can add it
		MeshClothingAssets.AddUnique(InNewAsset);

#if WITH_EDITOR
		OnClothingChange.Broadcast();
#endif
	}
}

void USkeletalMesh::RemoveClothingAsset(int32 InLodIndex, int32 InSectionIndex)
{
	UClothingAssetBase* Asset = GetSectionClothingAsset(InLodIndex, InSectionIndex);

	if(Asset)
	{
		Asset->UnbindFromSkeletalMesh(this, InLodIndex);
		MeshClothingAssets.Remove(Asset);

#if WITH_EDITOR
		OnClothingChange.Broadcast();
#endif
	}
}

UClothingAssetBase* USkeletalMesh::GetSectionClothingAsset(int32 InLodIndex, int32 InSectionIndex)
{
	if(FSkeletalMeshResource* SkelResource = GetImportedResource())
	{
		if(SkelResource->LODModels.IsValidIndex(InLodIndex))
		{
			FStaticLODModel& LodModel = SkelResource->LODModels[InLodIndex];
			if(LodModel.Sections.IsValidIndex(InSectionIndex))
			{
				FSkelMeshSection& Section = LodModel.Sections[InSectionIndex];

				FGuid ClothingAssetGuid = Section.ClothingData.AssetGuid;

				if(ClothingAssetGuid.IsValid())
				{
					UClothingAssetBase** FoundAsset = MeshClothingAssets.FindByPredicate([&](UClothingAssetBase* InAsset)
					{
						return InAsset->GetAssetGuid() == ClothingAssetGuid;
					});
					
					return FoundAsset ? *FoundAsset : nullptr;
				}
			}
		}
	}

	return nullptr;
}

const UClothingAssetBase* USkeletalMesh::GetSectionClothingAsset(int32 InLodIndex, int32 InSectionIndex) const
{
	if(FSkeletalMeshResource* SkelResource = GetImportedResource())
	{
		if(SkelResource->LODModels.IsValidIndex(InLodIndex))
		{
			FStaticLODModel& LodModel = SkelResource->LODModels[InLodIndex];
			if(LodModel.Sections.IsValidIndex(InSectionIndex))
			{
				FSkelMeshSection& Section = LodModel.Sections[InSectionIndex];

				FGuid ClothingAssetGuid = Section.ClothingData.AssetGuid;

				if(ClothingAssetGuid.IsValid())
				{
					UClothingAssetBase* const* FoundAsset = MeshClothingAssets.FindByPredicate([&](UClothingAssetBase* InAsset)
					{
						return InAsset->GetAssetGuid() == ClothingAssetGuid;
					});

					return FoundAsset ? *FoundAsset : nullptr;
				}
			}
		}
	}

	return nullptr;
}

UClothingAssetBase* USkeletalMesh::GetClothingAsset(const FGuid& InAssetGuid) const
{
	if(!InAssetGuid.IsValid())
	{
		return nullptr;
	}

	UClothingAssetBase* const* FoundAsset = MeshClothingAssets.FindByPredicate([&](UClothingAssetBase* CurrAsset)
	{
		return CurrAsset->GetAssetGuid() == InAssetGuid;
	});

	return FoundAsset ? *FoundAsset : nullptr;
}

int32 USkeletalMesh::GetClothingAssetIndex(UClothingAssetBase* InAsset) const
{
	if(!InAsset)
	{
		return INDEX_NONE;
	}

	return GetClothingAssetIndex(InAsset->GetAssetGuid());
}

int32 USkeletalMesh::GetClothingAssetIndex(const FGuid& InAssetGuid) const
{
	const int32 NumAssets = MeshClothingAssets.Num();
	for(int32 SearchIndex = 0; SearchIndex < NumAssets; ++SearchIndex)
	{
		if(MeshClothingAssets[SearchIndex]->GetAssetGuid() == InAssetGuid)
		{
			return SearchIndex;
		}
	}

	return INDEX_NONE;
}

bool USkeletalMesh::HasActiveClothingAssets() const
{
	if(FSkeletalMeshResource* Resource = GetImportedResource())
	{
		for(FStaticLODModel& LodModel : Resource->LODModels)
		{
			int32 NumNonClothingSections = LodModel.NumNonClothingSections();
			for(int32 SectionIdx = 0; SectionIdx < NumNonClothingSections; ++SectionIdx)
			{
				FSkelMeshSection& Section = LodModel.Sections[SectionIdx];

				if(Section.ClothingData.AssetGuid.IsValid())
				{
					return true;
				}
			}
		}
	}

	return false;
}

void USkeletalMesh::GetClothingAssetsInUse(TArray<UClothingAssetBase*>& OutClothingAssets) const
{
	OutClothingAssets.Reset();
	
	if(FSkeletalMeshResource* Resource = GetImportedResource())
	{
		for(FStaticLODModel& LodModel : Resource->LODModels)
		{
			int32 NumNonClothingSections = LodModel.NumNonClothingSections();
			for(int32 SectionIdx = 0; SectionIdx < NumNonClothingSections; ++SectionIdx)
			{
				FSkelMeshSection& Section = LodModel.Sections[SectionIdx];

				if(Section.ClothingData.AssetGuid.IsValid())
				{
					UClothingAssetBase* Asset = GetClothingAsset(Section.ClothingData.AssetGuid);
					
					if(Asset)
					{
						OutClothingAssets.AddUnique(Asset);
					}
				}
			}
		}
	}
}

void USkeletalMesh::InitResources()
{
	UpdateUVChannelData(false);
	ImportedResource->InitResources(bHasVertexColors, MorphTargets);
}


void USkeletalMesh::ReleaseResources()
{
	ImportedResource->ReleaseResources();
	// insert a fence to signal when these commands completed
	ReleaseResourcesFence.BeginFence();
}

#if WITH_EDITORONLY_DATA
static void AccumulateUVDensities(float* OutWeightedUVDensities, float* OutWeights, const FStaticLODModel& LODModel, const FSkelMeshSection& Section)
{
	const int32 NumTotalTriangles = LODModel.GetTotalFaces();
	const int32 NumCoordinateIndex = FMath::Min<int32>(LODModel.NumTexCoords, TEXSTREAM_MAX_NUM_UVCHANNELS);

	FUVDensityAccumulator UVDensityAccs[TEXSTREAM_MAX_NUM_UVCHANNELS];
	for (int32 CoordinateIndex = 0; CoordinateIndex < NumCoordinateIndex; ++CoordinateIndex)
	{
		UVDensityAccs[CoordinateIndex].Reserve(NumTotalTriangles);
	}

	TArray<uint32> Indices;
	LODModel.MultiSizeIndexContainer.GetIndexBuffer( Indices );
	if (!Indices.Num()) return;

	const uint32* SrcIndices = Indices.GetData() + Section.BaseIndex;
	uint32 NumTriangles = Section.NumTriangles;

	// Figure out Unreal unit per texel ratios.
	for (uint32 TriangleIndex=0; TriangleIndex < NumTriangles; TriangleIndex++ )
	{
		//retrieve indices
		uint32 Index0 = SrcIndices[TriangleIndex*3];
		uint32 Index1 = SrcIndices[TriangleIndex*3+1];
		uint32 Index2 = SrcIndices[TriangleIndex*3+2];

		const float Aera = FUVDensityAccumulator::GetTriangleAera(
								LODModel.VertexBufferGPUSkin.GetVertexPositionFast(Index0),
								LODModel.VertexBufferGPUSkin.GetVertexPositionFast(Index1),
								LODModel.VertexBufferGPUSkin.GetVertexPositionFast(Index2));

		if (Aera > SMALL_NUMBER)
		{
			for (int32 CoordinateIndex = 0; CoordinateIndex < NumCoordinateIndex; ++CoordinateIndex)
			{
				const float UVAera = FUVDensityAccumulator::GetUVChannelAera(
										LODModel.VertexBufferGPUSkin.GetVertexUVFast(Index0, CoordinateIndex),
										LODModel.VertexBufferGPUSkin.GetVertexUVFast(Index1, CoordinateIndex),
										LODModel.VertexBufferGPUSkin.GetVertexUVFast(Index2, CoordinateIndex));

				UVDensityAccs[CoordinateIndex].PushTriangle(Aera, UVAera);
			}
		}
	}

	for (int32 CoordinateIndex = 0; CoordinateIndex < NumCoordinateIndex; ++CoordinateIndex)
	{
		UVDensityAccs[CoordinateIndex].AccumulateDensity(OutWeightedUVDensities[CoordinateIndex], OutWeights[CoordinateIndex]);
	}
}
#endif

void USkeletalMesh::UpdateUVChannelData(bool bRebuildAll)
{
#if WITH_EDITORONLY_DATA
	// Once cooked, the data requires to compute the scales will not be CPU accessible.
	FSkeletalMeshResource* Resource = GetImportedResource();
	if (FPlatformProperties::HasEditorOnlyData() && Resource)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
		{
			FMeshUVChannelInfo& UVChannelData = Materials[MaterialIndex].UVChannelData;

			// Skip it if we want to keep it.
			if (UVChannelData.bInitialized && (!bRebuildAll || UVChannelData.bOverrideDensities))
				continue;

			float WeightedUVDensities[TEXSTREAM_MAX_NUM_UVCHANNELS] = {0, 0, 0, 0};
			float Weights[TEXSTREAM_MAX_NUM_UVCHANNELS] = {0, 0, 0, 0};

			for (const FStaticLODModel& LODModel : Resource->LODModels)
			{
				for (const FSkelMeshSection& SectionInfo : LODModel.Sections)
				{
					if (SectionInfo.MaterialIndex != MaterialIndex)
							continue;

					AccumulateUVDensities(WeightedUVDensities, Weights, LODModel, SectionInfo);
				}
			}

			UVChannelData.bInitialized = true;
			UVChannelData.bOverrideDensities = false;
			for (int32 CoordinateIndex = 0; CoordinateIndex < TEXSTREAM_MAX_NUM_UVCHANNELS; ++CoordinateIndex)
			{
				UVChannelData.LocalUVDensities[CoordinateIndex] = (Weights[CoordinateIndex] > KINDA_SMALL_NUMBER) ? (WeightedUVDensities[CoordinateIndex] / Weights[CoordinateIndex]) : 0;
			}
		}

		Resource->SyncUVChannelData(Materials);
	}
#endif
}

const FMeshUVChannelInfo* USkeletalMesh::GetUVChannelData(int32 MaterialIndex) const
{
	if (Materials.IsValidIndex(MaterialIndex))
	{
		ensure(Materials[MaterialIndex].UVChannelData.bInitialized);
		return &Materials[MaterialIndex].UVChannelData;
	}

	return nullptr;
}

void USkeletalMesh::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (ImportedResource.IsValid())
	{
		ImportedResource->GetResourceSizeEx(CumulativeResourceSize);
	}

	if (CumulativeResourceSize.GetResourceSizeMode() == EResourceSizeMode::Inclusive)
	{
		for (const auto& MorphTarget : MorphTargets)
		{
			MorphTarget->GetResourceSizeEx(CumulativeResourceSize);
		}

		for(const FClothingAssetData_Legacy& LegacyAsset : ClothingAssets_DEPRECATED)
		{
			LegacyAsset.GetResourceSizeEx(CumulativeResourceSize);
		}

		for(UClothingAssetBase* ClothingAsset : MeshClothingAssets)
		{
			ClothingAsset->GetResourceSizeEx(CumulativeResourceSize);
		}

		TSet<UMaterialInterface*> UniqueMaterials;
		for(int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
		{
			UMaterialInterface* Material = Materials[MaterialIndex].MaterialInterface;
			bool bAlreadyCounted = false;
			UniqueMaterials.Add(Material, &bAlreadyCounted);
			if(!bAlreadyCounted && Material)
			{
				Material->GetResourceSizeEx(CumulativeResourceSize);
			}
		}

#if WITH_EDITORONLY_DATA
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RetargetBasePose.GetAllocatedSize());
#endif

		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RefBasesInvMatrix.GetAllocatedSize());
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RefSkeleton.GetDataSize());

		if (BodySetup)
		{
			BodySetup->GetResourceSizeEx(CumulativeResourceSize);
		}

		if (PhysicsAsset)
		{
			PhysicsAsset->GetResourceSizeEx(CumulativeResourceSize);
		}
	}
}

/**
 * Operator for MemCount only
 */
FArchive &operator<<( FArchive& Ar, FTriangleSortSettings& S )
{
	Ar << S.TriangleSorting;
	Ar << S.CustomLeftRightAxis;
	Ar << S.CustomLeftRightBoneName;
	return Ar;
}

/**
 * Operator for MemCount only, so it only serializes the arrays that needs to be counted.
 */
FArchive &operator<<( FArchive& Ar, FSkeletalMeshLODInfo& I )
{
	Ar << I.LODMaterialMap;

	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING )
	{
		Ar << I.bEnableShadowCasting_DEPRECATED;
	}

	Ar << I.TriangleSortSettings;

	return Ar;
}

void RefreshSkelMeshOnPhysicsAssetChange(const USkeletalMesh* InSkeletalMesh)
{
	if (InSkeletalMesh)
	{
		for (FObjectIterator Iter(USkeletalMeshComponent::StaticClass()); Iter; ++Iter)
		{
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(*Iter);
			// if PhysicsAssetOverride is NULL, it uses SkeletalMesh Physics Asset, so I'll need to update here
			if  (SkeletalMeshComponent->SkeletalMesh == InSkeletalMesh &&
				 SkeletalMeshComponent->PhysicsAssetOverride == NULL)
			{
				// it needs to recreate IF it already has been created
				if (SkeletalMeshComponent->IsPhysicsStateCreated())
				{
					// do not call SetPhysAsset as it will setup physics asset override
					SkeletalMeshComponent->RecreatePhysicsState();
					SkeletalMeshComponent->UpdateHasValidBodies();
				}
			}
		}
#if WITH_EDITOR
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
#endif // WITH_EDITOR
	}
}

#if WITH_EDITOR
void USkeletalMesh::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (GIsEditor &&
		PropertyAboutToChange &&
		PropertyAboutToChange->GetOuterUField() &&
		PropertyAboutToChange->GetOuterUField()->GetFName() == FName(TEXT("ClothPhysicsProperties")))
	{
		// if this is a member property of ClothPhysicsProperties, don't release render resources to drag sliders smoothly 
		return;
	}
	FlushRenderState();
}

void USkeletalMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bFullPrecisionUVsReallyChanged = false;

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	
	// if this is a member property of ClothPhysicsProperties, skip RestartRenderState to drag ClothPhysicsProperties sliders smoothly
	bool bSkipRestartRenderState = 
		GIsEditor &&
		PropertyThatChanged &&
		PropertyThatChanged->GetOuterUField() &&
		PropertyThatChanged->GetOuterUField()->GetFName() == FName(TEXT("ClothPhysicsProperties"));	

	if( GIsEditor &&
		PropertyThatChanged &&
		PropertyThatChanged->GetFName() == FName(TEXT("bUseFullPrecisionUVs")) )
	{
		bFullPrecisionUVsReallyChanged = true;
		if (!bUseFullPrecisionUVs && !GVertexElementTypeSupport.IsSupported(VET_Half2) )
		{
			bUseFullPrecisionUVs = true;
			UE_LOG(LogSkeletalMesh, Warning, TEXT("16 bit UVs not supported. Reverting to 32 bit UVs"));			
			bFullPrecisionUVsReallyChanged = false;
		}
	}

	// Apply any triangle sorting changes
	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("TriangleSorting")) )
	{
		FVector SortCenter;
		FSkeletalMeshResource* Resource = GetImportedResource();
		bool bHaveSortCenter = GetSortCenterPoint(SortCenter);
		for( int32 LODIndex = 0;LODIndex < Resource->LODModels.Num();LODIndex++ )
		{
			for( int32 SectionIndex = 0; SectionIndex < Resource->LODModels[LODIndex].Sections.Num();SectionIndex++ )
			{
				Resource->LODModels[LODIndex].SortTriangles( SortCenter, bHaveSortCenter, SectionIndex, (ETriangleSortOption)LODInfo[LODIndex].TriangleSortSettings[SectionIndex].TriangleSorting );
			}
		}
	}
	
	if (!bSkipRestartRenderState)
	{
		RestartRenderState();
	}

	if( GIsEditor &&
		PropertyThatChanged &&
		PropertyThatChanged->GetFName() == FName(TEXT("PhysicsAsset")) )
	{
		RefreshSkelMeshOnPhysicsAssetChange(this);
	}

	if( GIsEditor &&
		Cast<UObjectProperty>(PropertyThatChanged) &&
		Cast<UObjectProperty>(PropertyThatChanged)->PropertyClass == UMorphTarget::StaticClass() )
	{
		// A morph target has changed, reinitialize morph target maps
		InitMorphTargets();
	}

	if ( GIsEditor &&
		 PropertyThatChanged &&
		 PropertyThatChanged->GetFName() == FName(TEXT("bEnablePerPolyCollision"))
		)
	{
		BuildPhysicsData();
	}

	if(UProperty* MemberProperty = PropertyChangedEvent.MemberProperty)
	{
		if(MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USkeletalMesh, PositiveBoundsExtension) ||
			MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USkeletalMesh, NegativeBoundsExtension))
		{
			// If the bounds extensions change, recalculate extended bounds.
			ValidateBoundsExtension();
			CalculateExtendedBounds();
		}
	}

	if(PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(USkeletalMesh, PostProcessAnimBlueprint))
	{
		TArray<UActorComponent*> ComponentsToReregister;
		for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* MeshComponent = *It;
			if(MeshComponent && !MeshComponent->IsTemplate() && MeshComponent->SkeletalMesh == this)
			{
				ComponentsToReregister.Add(*It);
			}
		}
		FMultiComponentReregisterContext ReregisterContext(ComponentsToReregister);
	}

	UpdateUVChannelData(true);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void USkeletalMesh::PostEditUndo()
{
	Super::PostEditUndo();
	for( TObjectIterator<USkinnedMeshComponent> It; It; ++It )
	{
		USkinnedMeshComponent* MeshComponent = *It;
		if( MeshComponent && 
			!MeshComponent->IsTemplate() &&
			MeshComponent->SkeletalMesh == this )
		{
			FComponentReregisterContext Context(MeshComponent);
		}
	}

	if(MorphTargets.Num() > MorphTargetIndexMap.Num())
	{
		// A morph target remove has been undone, reinitialise
		InitMorphTargets();
	}
}
#endif // WITH_EDITOR

static void RecreateRenderState_Internal(const USkeletalMesh* InSkeletalMesh)
{
	if (InSkeletalMesh)
	{
		for( TObjectIterator<USkinnedMeshComponent> It; It; ++It )
		{
			USkinnedMeshComponent* MeshComponent = *It;
			if( MeshComponent && 
				!MeshComponent->IsTemplate() &&
				MeshComponent->SkeletalMesh == InSkeletalMesh )
			{
				MeshComponent->RecreateRenderState_Concurrent();
			}
		}
	}
}

void USkeletalMesh::BeginDestroy()
{
	Super::BeginDestroy();

	// remove the cache of link up
	if (Skeleton)
	{
		Skeleton->RemoveLinkup(this);
	}

#if WITH_APEX_CLOTHING
	// release clothing assets
	for (FClothingAssetData_Legacy& Data : ClothingAssets_DEPRECATED)
	{
		if (Data.ApexClothingAsset)
		{
			GPhysCommandHandler->DeferredRelease(Data.ApexClothingAsset);
			Data.ApexClothingAsset = nullptr;
		}
	}
#endif // #if WITH_APEX_CLOTHING

	// Release the mesh's render resources.
	ReleaseResources();
}

bool USkeletalMesh::IsReadyForFinishDestroy()
{
	// see if we have hit the resource flush fence
	return ReleaseResourcesFence.IsFenceComplete();
}

void USkeletalMesh::Serialize( FArchive& Ar )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("USkeletalMesh::Serialize"), STAT_SkeletalMesh_Serialize, STATGROUP_LoadTime );

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
	Ar.UsingCustomVersion(FSkeletalMeshCustomVersion::GUID);
	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	FStripDataFlags StripFlags( Ar );

	Ar << ImportedBounds;
	Ar << Materials;

	Ar << RefSkeleton;

	if (Ar.IsLoading())
	{
		const bool bRebuildNameMap = false;
		RefSkeleton.RebuildRefSkeleton(Skeleton, bRebuildNameMap);
	}

	// Serialize the default resource.
	ImportedResource->Serialize( Ar, this );

	// Build adjacency information for meshes that have not yet had it built.
#if WITH_EDITOR
	for ( int32 LODIndex = 0; LODIndex < ImportedResource->LODModels.Num(); ++LODIndex )
	{
		FStaticLODModel& LODModel = ImportedResource->LODModels[ LODIndex ];

		if ( !LODModel.AdjacencyMultiSizeIndexContainer.IsIndexBufferValid()
#if WITH_APEX_CLOTHING
		|| (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_APEX_CLOTH_TESSELLATION && LODModel.HasClothData())
#endif // WITH_APEX_CLOTHING
			)
		{
			TArray<FSoftSkinVertex> Vertices;
			FMultiSizeIndexContainerData IndexData;
			FMultiSizeIndexContainerData AdjacencyIndexData;
			IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

			UE_LOG(LogSkeletalMesh, Warning, TEXT("Building adjacency information for skeletal mesh '%s'. Please resave the asset."), *GetPathName() );
			LODModel.GetVertices( Vertices );
			LODModel.MultiSizeIndexContainer.GetIndexBufferData( IndexData );
			AdjacencyIndexData.DataTypeSize = IndexData.DataTypeSize;
			MeshUtilities.BuildSkeletalAdjacencyIndexBuffer( Vertices, LODModel.NumTexCoords, IndexData.Indices, AdjacencyIndexData.Indices );
			LODModel.AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer( AdjacencyIndexData );
		}
	}
#endif // #if WITH_EDITOR

	// make sure we're counting properly
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		Ar << RefBasesInvMatrix;
	}

	if( Ar.UE4Ver() < VER_UE4_REFERENCE_SKELETON_REFACTOR )
	{
		TMap<FName, int32> DummyNameIndexMap;
		Ar << DummyNameIndexMap;
	}

	//@todo legacy
	TArray<UObject*> DummyObjs;
	Ar << DummyObjs;

	if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::TextureStreamingMeshUVChannelData)
	{
		TArray<float> CachedStreamingTextureFactors;
		Ar << CachedStreamingTextureFactors;
	}

	if ( !StripFlags.IsEditorDataStripped() )
	{
		FSkeletalMeshSourceData& SkelSourceData = *(FSkeletalMeshSourceData*)( &SourceData );
		SkelSourceData.Serialize( Ar, this );
	}

#if WITH_EDITORONLY_DATA
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ASSET_IMPORT_DATA_AS_JSON && !AssetImportData)
	{
		// AssetImportData should always be valid
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
	
	// SourceFilePath and SourceFileTimestamp were moved into a subobject
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ADDED_FBX_ASSET_IMPORT_DATA && AssetImportData)
	{
		// AssetImportData should always have been set up in the constructor where this is relevant
		FAssetImportInfo Info;
		Info.Insert(FAssetImportInfo::FSourceFile(SourceFilePath_DEPRECATED));
		AssetImportData->SourceData = MoveTemp(Info);
		
		SourceFilePath_DEPRECATED = TEXT("");
		SourceFileTimestamp_DEPRECATED = TEXT("");
	}
#endif // WITH_EDITORONLY_DATA
	if (Ar.UE4Ver() >= VER_UE4_APEX_CLOTH)
	{
		if(Ar.CustomVer(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::NewClothingSystemAdded)
		{
		// Serialize non-UPROPERTY ApexClothingAsset data.
			for(int32 Idx = 0; Idx < ClothingAssets_DEPRECATED.Num(); Idx++)
		{
				Ar << ClothingAssets_DEPRECATED[Idx];
			}
		}

		if (Ar.UE4Ver() < VER_UE4_REFERENCE_SKELETON_REFACTOR)
		{
			RebuildRefSkeletonNameToIndexMap();
		}
	}

	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING )
	{
		// Previous to this version, shadowcasting flags were stored in the LODInfo array
		// now they're in the Materials array so we need to move them over
		MoveDeprecatedShadowFlagToMaterials();
	}
#if WITH_EDITORONLY_DATA
	if (Ar.UE4Ver() < VER_UE4_SKELETON_ASSET_PROPERTY_TYPE_CHANGE)
	{
		PreviewAttachedAssetContainer.SaveAttachedObjectsFromDeprecatedProperties();
	}
#endif

	if (bEnablePerPolyCollision)
	{
		Ar << BodySetup;
	}

#if WITH_EDITORONLY_DATA
	if (Ar.CustomVer(FEditorObjectVersion::GUID) < FEditorObjectVersion::RefactorMeshEditorMaterials)
	{
		MoveMaterialFlagsToSections();
	}
#endif

#if WITH_EDITORONLY_DATA
	bRequiresLODScreenSizeConversion = Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::LODsUseResolutionIndependentScreenSize;
	bRequiresLODHysteresisConversion = Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::LODHysteresisUseResolutionIndependentScreenSize;
#endif

}

void USkeletalMesh::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	USkeletalMesh* This = CastChecked<USkeletalMesh>(InThis);
#if WITH_EDITOR
	if( GIsEditor )
	{
		// Required by the unified GC when running in the editor
		for( int32 Index = 0; Index < This->Materials.Num(); Index++ )
		{
			Collector.AddReferencedObject( This->Materials[ Index ].MaterialInterface, This );
		}
	}
#endif
	Super::AddReferencedObjects( This, Collector );
}

void USkeletalMesh::FlushRenderState()
{
	//TComponentReregisterContext<USkeletalMeshComponent> ReregisterContext;

	// Release the mesh's render resources.
	ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still
	// allocated, and potentially accessing the mesh data.
	ReleaseResourcesFence.Wait();
}

bool USkeletalMesh::GetSortCenterPoint(FVector& OutSortCenter) const
{
	OutSortCenter = FVector::ZeroVector;
	bool bFoundCenter = false;
	USkeletalMeshSocket const* Socket = FindSocket( FName(TEXT("SortCenter")) );
	if( Socket )
	{
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Socket->BoneName);
		if( BoneIndex != INDEX_NONE )
		{
			bFoundCenter = true;
			OutSortCenter = RefSkeleton.GetRefBonePose()[BoneIndex].GetTranslation() + Socket->RelativeLocation;
		}
	}
	return bFoundCenter;
}

uint32 USkeletalMesh::GetVertexBufferFlags() const
{
	uint32 VertexFlags = FStaticLODModel::EVertexFlags::None;
	if (bUseFullPrecisionUVs)
	{
		VertexFlags |= FStaticLODModel::EVertexFlags::UseFullPrecisionUVs;
	}
	if (bHasVertexColors)
	{
		VertexFlags |= FStaticLODModel::EVertexFlags::HasVertexColors;
	}
	return VertexFlags;
}

void USkeletalMesh::RestartRenderState()
{
	FSkeletalMeshResource* Resource = GetImportedResource();

	// rebuild vertex buffers
	uint32 VertexFlags = GetVertexBufferFlags();
	for( int32 LODIndex = 0;LODIndex < Resource->LODModels.Num();LODIndex++ )
	{
		Resource->LODModels[LODIndex].BuildVertexBuffers(VertexFlags);
	}

	// reinitialize resource
	InitResources();

	RecreateRenderState_Internal(this);
}

void USkeletalMesh::PreSave(const class ITargetPlatform* TargetPlatform)
{
	// check the parent index of the root bone is invalid
	check((RefSkeleton.GetNum() == 0) || (RefSkeleton.GetRefBoneInfo()[0].ParentIndex == INDEX_NONE));

	Super::PreSave(TargetPlatform);
}

// Pre-calculate refpose-to-local transforms
void USkeletalMesh::CalculateInvRefMatrices()
{
	const int32 NumRealBones = RefSkeleton.GetRawBoneNum();

	if( RefBasesInvMatrix.Num() != NumRealBones)
	{
		RefBasesInvMatrix.Empty(NumRealBones);
		RefBasesInvMatrix.AddUninitialized(NumRealBones);

		// Reset cached mesh-space ref pose
		CachedComposedRefPoseMatrices.Empty(NumRealBones);
		CachedComposedRefPoseMatrices.AddUninitialized(NumRealBones);

		// Precompute the Mesh.RefBasesInverse.
		for( int32 b=0; b<NumRealBones; b++)
		{
			// Render the default pose.
			CachedComposedRefPoseMatrices[b] = GetRefPoseMatrix(b);

			// Construct mesh-space skeletal hierarchy.
			if( b>0 )
			{
				int32 Parent = RefSkeleton.GetRawParentIndex(b);
				CachedComposedRefPoseMatrices[b] = CachedComposedRefPoseMatrices[b] * CachedComposedRefPoseMatrices[Parent];
			}

			FVector XAxis, YAxis, ZAxis;

			CachedComposedRefPoseMatrices[b].GetScaledAxes(XAxis, YAxis, ZAxis);
			if(	XAxis.IsNearlyZero(SMALL_NUMBER) &&
				YAxis.IsNearlyZero(SMALL_NUMBER) &&
				ZAxis.IsNearlyZero(SMALL_NUMBER))
			{
				// this is not allowed, warn them 
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Reference Pose for joint (%s) includes NIL matrix. Zero scale isn't allowed on ref pose. "), *RefSkeleton.GetBoneName(b).ToString());
			}

			// Precompute inverse so we can use from-refpose-skin vertices.
			RefBasesInvMatrix[b] = CachedComposedRefPoseMatrices[b].Inverse(); 
		}

#if WITH_EDITORONLY_DATA
		if(RetargetBasePose.Num() == 0)
		{
			RetargetBasePose = RefSkeleton.GetRawRefBonePose();
		}
#endif // WITH_EDITORONLY_DATA
	}
}

void USkeletalMesh::CalculateRequiredBones(class FStaticLODModel& LODModel, const struct FReferenceSkeleton& RefSkeleton, const TMap<FBoneIndexType, FBoneIndexType> * BonesToRemove)
{
	// RequiredBones for base model includes all raw bones.
	int32 RequiredBoneCount = RefSkeleton.GetRawBoneNum();
	LODModel.RequiredBones.Empty(RequiredBoneCount);
	for(int32 i=0; i<RequiredBoneCount; i++)
	{
		// Make sure it's not in BonesToRemove
		// @Todo change this to one TArray
		if (!BonesToRemove || BonesToRemove->Find(i) == NULL)
		{
			LODModel.RequiredBones.Add(i);
		}
	}

	LODModel.RequiredBones.Shrink();	
}

void USkeletalMesh::PostLoad()
{
	Super::PostLoad();

	// If LODInfo is missing - create array of correct size.
	if( LODInfo.Num() != ImportedResource->LODModels.Num() )
	{
		LODInfo.Empty(ImportedResource->LODModels.Num());
		LODInfo.AddZeroed(ImportedResource->LODModels.Num());

		for(int32 i=0; i<LODInfo.Num(); i++)
		{
			LODInfo[i].LODHysteresis = 0.02f;
		}
	}

	int32 TotalLODNum = LODInfo.Num();
	for (int32 LodIndex = 0; LodIndex<TotalLODNum; LodIndex++)
	{
		FSkeletalMeshLODInfo& ThisLODInfo = LODInfo[LodIndex];
		FStaticLODModel& ThisLODModel = ImportedResource->LODModels[LodIndex];

		// Presize the per-section TriangleSortSettings array
		if( ThisLODInfo.TriangleSortSettings.Num() > ThisLODModel.Sections.Num() )
		{
			ThisLODInfo.TriangleSortSettings.RemoveAt( ThisLODModel.Sections.Num(), ThisLODInfo.TriangleSortSettings.Num()-ThisLODModel.Sections.Num() );
		}
		else
		if( ThisLODModel.Sections.Num() > ThisLODInfo.TriangleSortSettings.Num() )
		{
			ThisLODInfo.TriangleSortSettings.AddZeroed( ThisLODModel.Sections.Num()-ThisLODInfo.TriangleSortSettings.Num() );
		}

#if WITH_EDITOR
		if (ThisLODInfo.ReductionSettings.BonesToRemove_DEPRECATED.Num() > 0)
		{
			for (auto& BoneToRemove : ThisLODInfo.ReductionSettings.BonesToRemove_DEPRECATED)
			{
				AddBoneToReductionSetting(LodIndex, BoneToRemove.BoneName);
			}

			// since in previous system, we always removed from previous LOD, I'm adding this 
			// here for previous LODs
			for (int32 CurLodIndx = LodIndex + 1; CurLodIndx < TotalLODNum; ++CurLodIndx)
			{
				AddBoneToReductionSetting(CurLodIndx, ThisLODInfo.RemovedBones_DEPRECATED);
			}

			// we don't apply this change here, but this will be applied when you re-gen simplygon
			ThisLODInfo.ReductionSettings.BonesToRemove_DEPRECATED.Empty();
		}

		if (ThisLODInfo.ReductionSettings.BakePose_DEPRECATED != nullptr)
		{
			ThisLODInfo.BakePose = ThisLODInfo.ReductionSettings.BakePose_DEPRECATED;
			ThisLODInfo.ReductionSettings.BakePose_DEPRECATED = nullptr;
		}
#endif
	}

	// Revert to using 32 bit Float UVs on hardware that doesn't support rendering with 16 bit Float UVs 
	if( !bUseFullPrecisionUVs && !GVertexElementTypeSupport.IsSupported(VET_Half2) )
	{
		bUseFullPrecisionUVs=true;
		// convert each LOD level to 32 bit UVs
		for( int32 LODIdx=0; LODIdx < ImportedResource->LODModels.Num(); LODIdx++ )
		{
			FStaticLODModel& LODModel = ImportedResource->LODModels[LODIdx];
			// Determine the correct version of ConvertToFullPrecisionUVs based on the number of UVs in the vertex buffer
			const uint32 NumTexCoords = LODModel.VertexBufferGPUSkin.GetNumTexCoords();
			switch(NumTexCoords)
			{
			case 1: LODModel.VertexBufferGPUSkin.ConvertToFullPrecisionUVs<1>(); break;
			case 2: LODModel.VertexBufferGPUSkin.ConvertToFullPrecisionUVs<2>(); break; 
			case 3: LODModel.VertexBufferGPUSkin.ConvertToFullPrecisionUVs<3>(); break; 
			case 4: LODModel.VertexBufferGPUSkin.ConvertToFullPrecisionUVs<4>(); break; 
			}
		}
	}

#if WITH_EDITORONLY_DATA
	// Rebuild vertex buffers if needed
	if (GetLinkerCustomVersion(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CompactClothVertexBuffer)
	{
		FSkeletalMeshResource* Resource = GetImportedResource();
		if (Resource && FPlatformProperties::HasEditorOnlyData())
		{
			uint32 VertexFlags = GetVertexBufferFlags();
			for (int32 LODIndex = 0; LODIndex < Resource->LODModels.Num(); LODIndex++)
			{
				Resource->LODModels[LODIndex].BuildVertexBuffers(VertexFlags);
			}
		}
	}

	if (GetLinkerCustomVersion(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::FixedMeshUVDensity)
	{
		UpdateUVChannelData(true);
	}
#endif // WITH_EDITOR

	// init morph targets. 
	// should do this before InitResource, so that we clear invalid morphtargets
	InitMorphTargets();

	// initialize rendering resources
	if (FApp::CanEverRender())
	{
		InitResources();
	}
	else
	{
		// Update any missing data when cooking.
		UpdateUVChannelData(false);
	}

	CalculateInvRefMatrices();

	// validate influences for existing clothing
	if(FSkeletalMeshResource* SkelResource = GetImportedResource())
	{
		for(int32 LODIndex = 0; LODIndex < SkelResource->LODModels.Num(); ++LODIndex)
		{
			FStaticLODModel& CurLODModel = SkelResource->LODModels[LODIndex];

			for (int32 SectionIdx = 0; SectionIdx< CurLODModel.Sections.Num(); SectionIdx++)
			{
				FSkelMeshSection& CurSection = CurLODModel.Sections[SectionIdx];

				if(CurSection.CorrespondClothSectionIndex != INDEX_NONE && CurSection.MaxBoneInfluences > MAX_INFLUENCES_PER_STREAM)
				{
				UE_LOG(LogSkeletalMesh, Warning, TEXT("Section %d for LOD %d in skeletal mesh %s has clothing associated but has %d influences. Clothing only supports a maximum of %d influences - reduce influences on chunk and reimport mesh."),
					SectionIdx,
						LODIndex,
						*GetName(),
					CurSection.MaxBoneInfluences,
						MAX_INFLUENCES_PER_STREAM);
				}
			}
		}
	}

	if( GetLinkerUE4Version() < VER_UE4_REFERENCE_SKELETON_REFACTOR )
	{
		RebuildRefSkeletonNameToIndexMap();
	}

	if (GetLinkerUE4Version() < VER_UE4_SORT_ACTIVE_BONE_INDICES)
	{
		for (int32 LodIndex = 0; LodIndex < LODInfo.Num(); LodIndex++)
		{
			FStaticLODModel & ThisLODModel = ImportedResource->LODModels[LodIndex];
			ThisLODModel.ActiveBoneIndices.Sort();
		}
	}

#if WITH_EDITORONLY_DATA
	if (RetargetBasePose.Num() == 0)
	{
		RetargetBasePose = RefSkeleton.GetRefBonePose();
	}
#endif

	// Bounds have been loaded - apply extensions.
	CalculateExtendedBounds();

	if(GetLinkerCustomVersion(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::RegenerateClothingShadowFlags)
	{
		if(FSkeletalMeshResource* MeshResource = GetImportedResource())
		{
			for(FStaticLODModel& LodModel : MeshResource->LODModels)
			{
				for(FSkelMeshSection& Section : LodModel.Sections)
				{
					if(Section.HasClothingData())
					{
						check(LodModel.Sections.IsValidIndex(Section.CorrespondClothSectionIndex));

						FSkelMeshSection& OriginalSection = LodModel.Sections[Section.CorrespondClothSectionIndex];
						Section.bCastShadow = OriginalSection.bCastShadow;
					}
				}
			}
		}
	}

#if WITH_EDITORONLY_DATA
	if (bRequiresLODScreenSizeConversion || bRequiresLODHysteresisConversion)
	{
		// Convert screen area to screen size
		ConvertLegacyLODScreenSize();
	}
#endif

	// Can only do an old-> new clothing asset upgrade in the editor.
	// And only if APEX clothing is available to upgrade from
#if WITH_EDITOR && WITH_APEX_CLOTHING
	if(ClothingAssets_DEPRECATED.Num() > 0)
	{
		// Upgrade the old deprecated clothing assets in to new clothing assets
		TMap<int32, TArray<int32>> OldLodMappings; // Map asset index to multiple lod indices
		TMap<int32, TArray<int32>> OldSectionMappings; // Map asset index to a section per LOD
		for(int32 AssetIdx = 0; AssetIdx < ClothingAssets_DEPRECATED.Num(); ++AssetIdx)
		{
			FClothingAssetData_Legacy& OldAssetData = ClothingAssets_DEPRECATED[AssetIdx];

			OldLodMappings.Add(AssetIdx);
			OldSectionMappings.Add(AssetIdx);

			if(ImportedResource.IsValid())
			{
				int32 FoundLod = INDEX_NONE;
				int32 FoundSection = INDEX_NONE;
				for(int32 LodIdx = 0; LodIdx < ImportedResource->LODModels.Num(); ++LodIdx)
				{
					FStaticLODModel& LodModel = ImportedResource->LODModels[LodIdx];

					for(int32 SecIdx = 0; SecIdx < LodModel.Sections.Num(); ++SecIdx)
					{
						FSkelMeshSection& Section = LodModel.Sections[SecIdx];

						if(Section.CorrespondClothSectionIndex != INDEX_NONE && Section.bDisabled)
						{
							FSkelMeshSection& ClothSection = LodModel.Sections[Section.CorrespondClothSectionIndex];

							if(ClothSection.CorrespondClothAssetIndex == AssetIdx)
							{
								FoundSection = SecIdx;
								break;
							}
						}
					}

					if(FoundSection != INDEX_NONE)
					{
						OldLodMappings[AssetIdx].Add(LodIdx);
						OldSectionMappings[AssetIdx].Add(FoundSection);

						// Reset for next LOD
						FoundSection = INDEX_NONE;
					}
				}
			}

			FClothingSystemEditorInterfaceModule& ClothingEditorModule = FModuleManager::Get().LoadModuleChecked<FClothingSystemEditorInterfaceModule>(TEXT("ClothingSystemEditorInterface"));
			UClothingAssetFactoryBase* Factory = ClothingEditorModule.GetClothingAssetFactory();
			if(Factory)
			{
				UClothingAssetBase* NewAsset = Factory->CreateFromApexAsset(OldAssetData.ApexClothingAsset, this, *FPaths::GetBaseFilename(OldAssetData.ApexFileName));
				check(NewAsset);

				// Pull the path across so reimports work as expected
				NewAsset->ImportedFilePath = OldAssetData.ApexFileName;

				AddClothingAsset(NewAsset);
			}
		}

		// Go back over the old assets and remove them from the skeletal mesh so the indices are preserved while
		// calculating the LOD and section mappings above.
		for(int32 AssetIdx = ClothingAssets_DEPRECATED.Num() - 1; AssetIdx >= 0; --AssetIdx)
		{
			ApexClothingUtils::RemoveAssetFromSkeletalMesh(this, AssetIdx, false);
		}

		check(OldLodMappings.Num() == OldSectionMappings.Num());

		for(int32 NewAssetIdx = 0; NewAssetIdx < MeshClothingAssets.Num(); ++NewAssetIdx)
		{
			UClothingAssetBase* CurrAsset = MeshClothingAssets[NewAssetIdx];

			for(int32 MappedLodIdx = 0; MappedLodIdx < OldLodMappings[NewAssetIdx].Num(); ++MappedLodIdx)
			{
				const int32 MappedLod = OldLodMappings[NewAssetIdx][MappedLodIdx];
				const int32 MappedSection = OldSectionMappings[NewAssetIdx][MappedLodIdx];

				// Previously Clothing LODs were required to match skeletal mesh LODs, which is why we pass
				// MappedLod for both the mesh and clothing LODs here when doing an upgrade to the new
				// system. This restriction is now lifted and any mapping can be selected in Persona
				CurrAsset->BindToSkeletalMesh(this, MappedLod, MappedSection, MappedLod);
			}
		}
	}
#endif

	// If inverse masses have never been cached, invalidate data so it will be recalculated
	if(GetLinkerCustomVersion(FSkeletalMeshCustomVersion::GUID) < FSkeletalMeshCustomVersion::CachedClothInverseMasses)
	{
		for(UClothingAssetBase* ClothingAsset : MeshClothingAssets)
		{
			ClothingAsset->InvalidateCachedData();
		}
	}
}

void USkeletalMesh::RebuildRefSkeletonNameToIndexMap()
{
		TArray<FBoneIndexType> DuplicateBones;
		// Make sure we have no duplicate bones. Some content got corrupted somehow. :(
		RefSkeleton.RemoveDuplicateBones(this, DuplicateBones);

		// If we have removed any duplicate bones, we need to fix up any broken LODs as well.
		// Duplicate bones are given from highest index to lowest. 
		// so it's safe to decrease indices for children, we're not going to lose the index of the remaining duplicate bones.
	for (int32 Index = 0; Index < DuplicateBones.Num(); Index++)
		{
			const FBoneIndexType& DuplicateBoneIndex = DuplicateBones[Index];
		for (int32 LodIndex = 0; LodIndex < LODInfo.Num(); LodIndex++)
			{
				FStaticLODModel & ThisLODModel = ImportedResource->LODModels[LodIndex];
				{
					int32 FoundIndex;
				if (ThisLODModel.RequiredBones.Find(DuplicateBoneIndex, FoundIndex))
					{
						ThisLODModel.RequiredBones.RemoveAt(FoundIndex, 1);
						// we need to shift indices of the remaining bones.
					for (int32 j = FoundIndex; j < ThisLODModel.RequiredBones.Num(); j++)
						{
							ThisLODModel.RequiredBones[j] = ThisLODModel.RequiredBones[j] - 1;
						}
					}
				}

				{
					int32 FoundIndex;
				if (ThisLODModel.ActiveBoneIndices.Find(DuplicateBoneIndex, FoundIndex))
					{
						ThisLODModel.ActiveBoneIndices.RemoveAt(FoundIndex, 1);
						// we need to shift indices of the remaining bones.
					for (int32 j = FoundIndex; j < ThisLODModel.ActiveBoneIndices.Num(); j++)
						{
							ThisLODModel.ActiveBoneIndices[j] = ThisLODModel.ActiveBoneIndices[j] - 1;
						}
					}
				}
			}
		}

		// Rebuild name table.
		RefSkeleton.RebuildNameToIndexMap();
}

void USkeletalMesh::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	int32 NumTriangles = 0;
	if ( ImportedResource->LODModels.Num() > 0 && &ImportedResource->LODModels[0] != NULL )
	{
		const FStaticLODModel& LODModel = ImportedResource->LODModels[0];
		NumTriangles = LODModel.GetTotalFaces();
	}

	OutTags.Add(FAssetRegistryTag("Triangles", FString::FromInt(NumTriangles), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Bones", FString::FromInt(RefSkeleton.GetRawBoneNum()), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("MorphTargets", FString::FromInt(MorphTargets.Num()), FAssetRegistryTag::TT_Numerical));

#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}
#endif
	
	Super::GetAssetRegistryTags(OutTags);
}

#if WITH_EDITOR
void USkeletalMesh::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
	Super::GetAssetRegistryTagMetadata(OutMetadata);
	OutMetadata.Add("PhysicsAsset", FAssetRegistryTagMetadata().SetImportantValue(TEXT("None")));
}
#endif

void USkeletalMesh::DebugVerifySkeletalMeshLOD()
{
	// if LOD do not have displayfactor set up correctly
	if (LODInfo.Num() > 1)
	{
		for(int32 i=1; i<LODInfo.Num(); i++)
		{
			if (LODInfo[i].ScreenSize <= 0.1f)
			{
				// too small
				UE_LOG(LogSkeletalMesh, Warning, TEXT("SkelMeshLOD (%s) : ScreenSize for LOD %d may be too small (%0.5f)"), *GetPathName(), i, LODInfo[i].ScreenSize);
			}
		}
	}
	else
	{
		// no LODInfo
		UE_LOG(LogSkeletalMesh, Warning, TEXT("SkelMeshLOD (%s) : LOD does not exist"), *GetPathName());
	}
}

void USkeletalMesh::RegisterMorphTarget(UMorphTarget* MorphTarget)
{
	if ( MorphTarget )
	{
		// if MorphTarget has SkelMesh, make sure you unregister before registering yourself
		if ( MorphTarget->BaseSkelMesh && MorphTarget->BaseSkelMesh!=this )
		{
			MorphTarget->BaseSkelMesh->UnregisterMorphTarget(MorphTarget);
		}

		// if the input morphtarget doesn't have valid data, do not add to the base morphtarget
		ensureMsgf(MorphTarget->HasValidData(), TEXT("RegisterMorphTarget: %s has empty data."), *MorphTarget->GetName());

		MorphTarget->BaseSkelMesh = this;

		bool bRegistered = false;

		for ( int32 Index = 0; Index < MorphTargets.Num(); ++Index )
		{
			if ( MorphTargets[Index]->GetFName() == MorphTarget->GetFName() )
			{
				UE_LOG( LogSkeletalMesh, Log, TEXT("RegisterMorphTarget: %s already exists, replacing"), *MorphTarget->GetName() );
				MorphTargets[Index] = MorphTarget;
				bRegistered = true;
				break;
			}
		}

		if (!bRegistered)
		{
			MorphTargets.Add( MorphTarget );
			bRegistered = true;
		}

		if (bRegistered)
		{
			MarkPackageDirty();
			// need to refresh the map
			InitMorphTargets();
		}
	}
}

void USkeletalMesh::UnregisterMorphTarget(UMorphTarget* MorphTarget)
{
	if ( MorphTarget )
	{
		// Do not remove with MorphTarget->GetFName(). The name might have changed
		// Search the value, and delete	
		for ( int32 I=0; I<MorphTargets.Num(); ++I)
		{
			if ( MorphTargets[I] == MorphTarget )
			{
				MorphTargets.RemoveAt(I);
				--I;
				MarkPackageDirty();
				// need to refresh the map
				InitMorphTargets();
				return;
			}
		}

		UE_LOG( LogSkeletalMesh, Log, TEXT("UnregisterMorphTarget: %s not found."), *MorphTarget->GetName() );
	}
}

void USkeletalMesh::InitMorphTargets()
{
	MorphTargetIndexMap.Empty();

	for (int32 Index = 0; Index < MorphTargets.Num(); ++Index)
	{
		UMorphTarget* MorphTarget = MorphTargets[Index];
		// if we don't have a valid data, just remove it
		if (!MorphTarget->HasValidData())
		{
			MorphTargets.RemoveAt(Index);
			--Index;
			continue;
		}

		FName const ShapeName = MorphTarget->GetFName();
		if (MorphTargetIndexMap.Find(ShapeName) == nullptr)
		{
			MorphTargetIndexMap.Add(ShapeName, Index);

			// register as morphtarget curves
			if (Skeleton)
			{
				FSmartName CurveName;
				CurveName.DisplayName = ShapeName;
				
				// verify will make sure it adds to the curve if not found
				// the reason of using this is to make sure it works in editor/non-editor
				Skeleton->VerifySmartName(USkeleton::AnimCurveMappingName, CurveName);
				Skeleton->AccumulateCurveMetaData(ShapeName, false, true);
			}
		}
	}
}

UMorphTarget* USkeletalMesh::FindMorphTarget(FName MorphTargetName) const
{
	int32 Index;
	return FindMorphTargetAndIndex(MorphTargetName, Index);
}

UMorphTarget* USkeletalMesh::FindMorphTargetAndIndex(FName MorphTargetName, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	if( MorphTargetName != NAME_None )
	{
		const int32* Found = MorphTargetIndexMap.Find(MorphTargetName);
		if (Found)
		{
			OutIndex = *Found;
			return MorphTargets[*Found];
		}
	}

	return nullptr;
}

USkeletalMeshSocket* USkeletalMesh::FindSocket(FName InSocketName) const
{
	int32 DummyIdx;
	return FindSocketAndIndex(InSocketName, DummyIdx);
}

USkeletalMeshSocket* USkeletalMesh::FindSocketAndIndex(FName InSocketName, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	if (InSocketName == NAME_None)
	{
		return nullptr;
	}

	for (int32 i = 0; i < Sockets.Num(); i++)
	{
		USkeletalMeshSocket* Socket = Sockets[i];
		if (Socket && Socket->SocketName == InSocketName)
		{
			OutIndex = i;
			return Socket;
		}
	}

	// If the socket isn't on the mesh, try to find it on the skeleton
	if (Skeleton)
	{
		USkeletalMeshSocket* SkeletonSocket = Skeleton->FindSocketAndIndex(InSocketName, OutIndex);
		if (SkeletonSocket != nullptr)
		{
			OutIndex += Sockets.Num();
		}
		return SkeletonSocket;
	}

	return nullptr;
}

int32 USkeletalMesh::NumSockets() const
{
	return Sockets.Num() + (Skeleton ? Skeleton->Sockets.Num() : 0);
}

USkeletalMeshSocket* USkeletalMesh::GetSocketByIndex(int32 Index) const
{
	if (Index < Sockets.Num())
	{
		return Sockets[Index];
	}

	if (Skeleton && Index < Skeleton->Sockets.Num())
	{
		return Skeleton->Sockets[Index];
	}

	return nullptr;
}



/**
 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
 * you have a component of interest but what you really want is some characteristic that you can use to track
 * down where it came from.  
 */
FString USkeletalMesh::GetDetailedInfoInternal() const
{
	return GetPathName(nullptr);
}


FMatrix USkeletalMesh::GetRefPoseMatrix( int32 BoneIndex ) const
{
	check( BoneIndex >= 0 && BoneIndex < RefSkeleton.GetRawBoneNum() );
	FTransform BoneTransform = RefSkeleton.GetRawRefBonePose()[BoneIndex];
	// Make sure quaternion is normalized!
	BoneTransform.NormalizeRotation();
	return BoneTransform.ToMatrixWithScale();
}

FMatrix USkeletalMesh::GetComposedRefPoseMatrix( FName InBoneName ) const
{
	FMatrix LocalPose( FMatrix::Identity );

	if ( InBoneName != NAME_None )
	{
		int32 BoneIndex = RefSkeleton.FindBoneIndex(InBoneName);
		if (BoneIndex != INDEX_NONE)
		{
			return GetComposedRefPoseMatrix(BoneIndex);
		}
		else
		{
			USkeletalMeshSocket const* Socket = FindSocket(InBoneName);

			if(Socket != NULL)
			{
				BoneIndex = RefSkeleton.FindBoneIndex(Socket->BoneName);

				if(BoneIndex != INDEX_NONE)
				{
					const FRotationTranslationMatrix SocketMatrix(Socket->RelativeRotation, Socket->RelativeLocation);
					LocalPose = SocketMatrix * GetComposedRefPoseMatrix(BoneIndex);
				}
			}
		}
	}

	return LocalPose;
}

FMatrix USkeletalMesh::GetComposedRefPoseMatrix(int32 InBoneIndex) const
{
	return CachedComposedRefPoseMatrices[InBoneIndex];
}

TArray<USkeletalMeshSocket*>& USkeletalMesh::GetMeshOnlySocketList()
{
	return Sockets;
}

const TArray<USkeletalMeshSocket*>& USkeletalMesh::GetMeshOnlySocketList() const
{
	return Sockets;
}

void USkeletalMesh::MoveDeprecatedShadowFlagToMaterials()
{
	// First, the easy case where there's no LOD info (in which case, default to true!)
	if ( LODInfo.Num() == 0 )
	{
		for ( auto Material = Materials.CreateIterator(); Material; ++Material )
		{
			Material->bEnableShadowCasting_DEPRECATED = true;
		}

		return;
	}
	
	TArray<bool> PerLodShadowFlags;
	bool bDifferenceFound = false;

	// Second, detect whether the shadow casting flag is the same for all sections of all lods
	for ( auto LOD = LODInfo.CreateConstIterator(); LOD; ++LOD )
	{
		if ( LOD->bEnableShadowCasting_DEPRECATED.Num() )
		{
			PerLodShadowFlags.Add( LOD->bEnableShadowCasting_DEPRECATED[0] );
		}

		if ( !AreAllFlagsIdentical( LOD->bEnableShadowCasting_DEPRECATED ) )
		{
			// We found a difference in the sections of this LOD!
			bDifferenceFound = true;
			break;
		}
	}

	if ( !bDifferenceFound && !AreAllFlagsIdentical( PerLodShadowFlags ) )
	{
		// Difference between LODs
		bDifferenceFound = true;
	}

	if ( !bDifferenceFound )
	{
		// All the same, so just copy the shadow casting flag to all materials
		for ( auto Material = Materials.CreateIterator(); Material; ++Material )
		{
			Material->bEnableShadowCasting_DEPRECATED = PerLodShadowFlags.Num() ? PerLodShadowFlags[0] : true;
		}
	}
	else
	{
		FSkeletalMeshResource* Resource = GetImportedResource();
		check( Resource->LODModels.Num() == LODInfo.Num() );

		TArray<FSkeletalMaterial> NewMaterialArray;

		// There was a difference, so we need to build a new material list which has all the combinations of UMaterialInterface and shadow casting flag required
		for ( int32 LODIndex = 0; LODIndex < Resource->LODModels.Num(); ++LODIndex )
		{
			check( Resource->LODModels[LODIndex].Sections.Num() == LODInfo[LODIndex].bEnableShadowCasting_DEPRECATED.Num() );

			for ( int32 i = 0; i < Resource->LODModels[LODIndex].Sections.Num(); ++i )
			{
				NewMaterialArray.Add( FSkeletalMaterial( Materials[ Resource->LODModels[LODIndex].Sections[i].MaterialIndex ].MaterialInterface, LODInfo[LODIndex].bEnableShadowCasting_DEPRECATED[i], false, NAME_None, NAME_None ) );
			}
		}

		// Reassign the materials array to the new one
		Materials = NewMaterialArray;
		int32 NewIndex = 0;

		// Remap the existing LODModels to point at the correct new material index
		for ( int32 LODIndex = 0; LODIndex < Resource->LODModels.Num(); ++LODIndex )
		{
			check( Resource->LODModels[LODIndex].Sections.Num() == LODInfo[LODIndex].bEnableShadowCasting_DEPRECATED.Num() );

			for ( int32 i = 0; i < Resource->LODModels[LODIndex].Sections.Num(); ++i )
			{
				Resource->LODModels[LODIndex].Sections[i].MaterialIndex = NewIndex;
				++NewIndex;
			}
		}
	}
}

void USkeletalMesh::MoveMaterialFlagsToSections()
{
	//No LOD we cant set the value
	if (LODInfo.Num() == 0)
	{
		return;
	}

	for (FStaticLODModel &StaticLODModel : ImportedResource->LODModels)
	{
		for (int32 SectionIndex = 0; SectionIndex < StaticLODModel.Sections.Num(); ++SectionIndex)
		{
			FSkelMeshSection &Section = StaticLODModel.Sections[SectionIndex];
			//Prior to FEditorObjectVersion::RefactorMeshEditorMaterials Material index match section index
			if (Materials.IsValidIndex(SectionIndex))
			{
				Section.bCastShadow = Materials[SectionIndex].bEnableShadowCasting_DEPRECATED;

				Section.bRecomputeTangent = Materials[SectionIndex].bRecomputeTangent_DEPRECATED;
			}
			else
			{
				//Default cast shadow to true this is a fail safe code path it should not go here if the data
				//is valid
				Section.bCastShadow = true;
				//Recompute tangent is serialize prior to FEditorObjectVersion::RefactorMeshEditorMaterials
				// We just keep the serialize value
			}
		}
	}
}

#if WITH_EDITOR
FDelegateHandle USkeletalMesh::RegisterOnClothingChange(const FSimpleMulticastDelegate::FDelegate& InDelegate)
{
	return OnClothingChange.Add(InDelegate);
}

void USkeletalMesh::UnregisterOnClothingChange(const FDelegateHandle& InHandle)
{
	OnClothingChange.Remove(InHandle);
}
#endif

bool USkeletalMesh::AreAllFlagsIdentical( const TArray<bool>& BoolArray ) const
{
	if ( BoolArray.Num() == 0 )
	{
		return true;
	}

	for ( int32 i = 0; i < BoolArray.Num() - 1; ++i )
	{
		if ( BoolArray[i] != BoolArray[i + 1] )
		{
			return false;
		}
	}

	return true;
}

bool operator== ( const FSkeletalMaterial& LHS, const FSkeletalMaterial& RHS )
{
	return ( LHS.MaterialInterface == RHS.MaterialInterface );
}

bool operator== ( const FSkeletalMaterial& LHS, const UMaterialInterface& RHS )
{
	return ( LHS.MaterialInterface == &RHS );
}

bool operator== ( const UMaterialInterface& LHS, const FSkeletalMaterial& RHS )
{
	return ( RHS.MaterialInterface == &LHS );
}

FArchive& operator<<(FArchive& Ar, FMeshUVChannelInfo& ChannelData)
{
	Ar << ChannelData.bInitialized;
	Ar << ChannelData.bOverrideDensities;

	for (int32 CoordIndex = 0; CoordIndex < TEXSTREAM_MAX_NUM_UVCHANNELS; ++CoordIndex)
	{
		Ar << ChannelData.LocalUVDensities[CoordIndex];
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FSkeletalMaterial& Elem)
{
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

	Ar << Elem.MaterialInterface;

	//Use the automatic serialization instead of this custom operator
	if (Ar.CustomVer(FEditorObjectVersion::GUID) >= FEditorObjectVersion::RefactorMeshEditorMaterials)
	{
		Ar << Elem.MaterialSlotName;
#if WITH_EDITORONLY_DATA
		if (!Ar.IsCooking() || Ar.CookingTarget()->HasEditorOnlyData())
		{
			Ar << Elem.ImportedMaterialSlotName;
		}
#endif //#if WITH_EDITORONLY_DATA
	}
	else
	{
		if (Ar.UE4Ver() >= VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING)
		{
			Ar << Elem.bEnableShadowCasting_DEPRECATED;
		}

		Ar.UsingCustomVersion(FRecomputeTangentCustomVersion::GUID);
		if (Ar.CustomVer(FRecomputeTangentCustomVersion::GUID) >= FRecomputeTangentCustomVersion::RuntimeRecomputeTangent)
		{
			Ar << Elem.bRecomputeTangent_DEPRECATED;
		}
	}
	
	if (!Ar.IsLoading() || Ar.CustomVer(FRenderingObjectVersion::GUID) >= FRenderingObjectVersion::TextureStreamingMeshUVChannelData)
	{
		Ar << Elem.UVChannelData;
	}

	return Ar;
}

TArray<USkeletalMeshSocket*> USkeletalMesh::GetActiveSocketList() const
{
	TArray<USkeletalMeshSocket*> ActiveSockets = Sockets;

	// Then the skeleton sockets that aren't in the mesh
	if (Skeleton)
	{
		for (auto SkeletonSocketIt = Skeleton->Sockets.CreateConstIterator(); SkeletonSocketIt; ++SkeletonSocketIt)
		{
			USkeletalMeshSocket* Socket = *(SkeletonSocketIt);

			if (!IsSocketOnMesh(Socket->SocketName))
			{
				ActiveSockets.Add(Socket);
			}
		}
	}
	return ActiveSockets;
}

bool USkeletalMesh::IsSocketOnMesh(const FName& InSocketName) const
{
	for(int32 SocketIdx=0; SocketIdx < Sockets.Num(); SocketIdx++)
	{
		USkeletalMeshSocket* Socket = Sockets[SocketIdx];

		if(Socket != NULL && Socket->SocketName == InSocketName)
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR


FStaticLODModel& USkeletalMesh::GetSourceModel()
{
	check( ImportedResource->LODModels.Num() );
	FSkeletalMeshSourceData& SkelSourceData = *(FSkeletalMeshSourceData*)( &SourceData );
	if ( SkelSourceData.IsInitialized() )
	{
		return *SkelSourceData.GetModel();
	}
	return ImportedResource->LODModels[0];
}


FStaticLODModel& USkeletalMesh::PreModifyMesh()
{
	FSkeletalMeshSourceData& SkelSourceData = *(FSkeletalMeshSourceData*)( &SourceData );
	if ( !SkelSourceData.IsInitialized() && ImportedResource->LODModels.Num() )
	{
		SkelSourceData.Init( this, ImportedResource->LODModels[0] );
	}
	check( SkelSourceData.IsInitialized() );
	return GetSourceModel();
}

int32 USkeletalMesh::ValidatePreviewAttachedObjects()
{
	int32 NumBrokenAssets = PreviewAttachedAssetContainer.ValidatePreviewAttachedObjects();

	if (NumBrokenAssets > 0)
	{
		MarkPackageDirty();
	}
	return NumBrokenAssets;
}

void USkeletalMesh::RemoveMeshSection(int32 InLodIndex, int32 InSectionIndex)
{
	// Need a mesh resource
	if(!ImportedResource.IsValid())
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to remove skeletal mesh section, ImportedResource is invalid."));
		return;
	}

	// Need a valid LOD
	if(!ImportedResource->LODModels.IsValidIndex(InLodIndex))
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to remove skeletal mesh section, LOD%d does not exist in the mesh"), InLodIndex);
		return;
	}

	FStaticLODModel& LodModel = ImportedResource->LODModels[InLodIndex];

	// Need a valid section
	if(!LodModel.Sections.IsValidIndex(InSectionIndex))
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to remove skeletal mesh section, Section %d does not exist in LOD%d."), InSectionIndex, InLodIndex);
		return;
	}

	FSkelMeshSection& SectionToRemove = LodModel.Sections[InSectionIndex];

	if(SectionToRemove.CorrespondClothSectionIndex != INDEX_NONE)
	{
		// Can't remove this, clothing currently relies on it
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Failed to remove skeletal mesh section, clothing is currently bound to Lod%d Section %d, unbind clothing before removal."), InLodIndex, InSectionIndex);
		return;
	}

	// Valid to remove, dirty the mesh
	Modify();
	PreEditChange(nullptr);

	// Prepare reregister context to unregister all users
	TArray<UActorComponent*> Components;
	for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		USkeletalMeshComponent* MeshComponent = *It;
		if(MeshComponent && !MeshComponent->IsTemplate() && MeshComponent->SkeletalMesh == this)
		{
			Components.Add(MeshComponent);
		}
	}
	FMultiComponentReregisterContext ReregisterContext(Components);

	// Begin section removal
	const uint32 NumVertsToRemove = SectionToRemove.GetNumVertices();
	const uint32 BaseVertToRemove = SectionToRemove.BaseVertexIndex;
	const uint32 NumIndicesToRemove = SectionToRemove.NumTriangles * 3;
	const uint32 BaseIndexToRemove = SectionToRemove.BaseIndex;

	FMultiSizeIndexContainerData NewIndexData;
	LodModel.MultiSizeIndexContainer.GetIndexBufferData(NewIndexData);

	// Strip indices
	NewIndexData.Indices.RemoveAt(BaseIndexToRemove, NumIndicesToRemove);

	// Fixup indices above base vert
	for(uint32& Index : NewIndexData.Indices)
	{
		if(Index >= BaseVertToRemove)
		{
			Index -= NumVertsToRemove;
		}
	}

	// Rebuild index data
	int32 NumVertsAfterRemoval = LodModel.NumVertices - NumVertsToRemove;
	if(NumVertsAfterRemoval > MAX_uint16)
	{
		NewIndexData.DataTypeSize = sizeof(uint32);
	}
	else
	{
		NewIndexData.DataTypeSize = sizeof(uint16);
	}

	// Push back to lod model
	LodModel.MultiSizeIndexContainer.RebuildIndexBuffer(NewIndexData);
	LodModel.Sections.RemoveAt(InSectionIndex);
	LodModel.NumVertices -= NumVertsToRemove;

	// Fixup anything needing section indices
	for(FSkelMeshSection& Section : LodModel.Sections)
	{
		// Push back clothing indices
		if(Section.CorrespondClothSectionIndex > InSectionIndex)
		{
			Section.CorrespondClothSectionIndex--;
		}

		// Removed indices, rebase further sections
		if(Section.BaseIndex > BaseIndexToRemove)
		{
			Section.BaseIndex -= NumIndicesToRemove;
		}

		// Remove verts, rebase further sections
		if(Section.BaseVertexIndex > BaseVertToRemove)
		{
			Section.BaseVertexIndex -= NumVertsToRemove;
		}
	}

	PostEditChange();
}

//#nv begin #Blast Ability to hide bones using a dynamic index buffer
void USkeletalMesh::RebuildIndexBufferRanges()
{
	IndexBufferRanges.Reset();
	const int32 BoneCount = RefSkeleton.GetNum();
	IndexBufferRanges.SetNum(BoneCount);

	FSkeletalMeshResource* Resource = GetResourceForRendering();
	if (!Resource)
	{
		return;
	}

	FScopedSlowTask Progress(float(Resource->LODModels.Num()), LOCTEXT("RebuildIndexBufferRangesProgress", "Rebuilding bone index buffer ranges"));

	TArray<uint32> TempBuffer;
	for (int32 I = 0; I < BoneCount; I++)
	{
		IndexBufferRanges[I].LODModels.SetNum(Resource->LODModels.Num());
	}

	TArray<int32> TempBoneIndicies;
	for (int32 I = 0; I < Resource->LODModels.Num(); I++)
	{
		Progress.EnterProgressFrame();
		const FStaticLODModel& SrcModel = Resource->LODModels[I];
		if (SrcModel.MultiSizeIndexContainer.IsIndexBufferValid())
		{
			SrcModel.MultiSizeIndexContainer.GetIndexBuffer(TempBuffer);
		}

		const int32 SectionCount = SrcModel.Sections.Num();
		for (int32 B = 0; B < BoneCount; B++)
		{
			FSkeletalMeshIndexBufferRanges::FPerLODInfo& DestModel = IndexBufferRanges[B].LODModels[I];
			DestModel.Sections.SetNum(SectionCount);
		}

		for (int32 S = 0; S < SectionCount; S++)
		{
			const FSkelMeshSection& SrcSection = SrcModel.Sections[S];
			for (uint32 TI = 0; TI < SrcSection.NumTriangles; TI++)
			{
				const int32 IndexIndex = SrcSection.BaseIndex + TI * 3;
				//The indices required for this triangle
				FInt32Range CurIndexRange(IndexIndex, IndexIndex + 3);

				TempBoneIndicies.Reset(3);
				for (int32 VI = 0; VI < 3; VI++)
				{
					const int32 VertexIndex = TempBuffer[IndexIndex + VI] - SrcSection.BaseVertexIndex;
					const FSoftSkinVertex& VertexData = SrcSection.SoftVertices[VertexIndex];
					for (int32 WeightIdx = 0; WeightIdx < MAX_TOTAL_INFLUENCES; WeightIdx++)
					{
						if (VertexData.InfluenceWeights[WeightIdx] > 0)
						{
							int32 BoneIdx = SrcSection.BoneMap[VertexData.InfluenceBones[WeightIdx]];
							if (BoneIdx != INDEX_NONE)
							{
								TempBoneIndicies.AddUnique(BoneIdx);
							}
						}
					}
				}

				for (int32 BoneIndex : TempBoneIndicies)
				{
					FSkeletalMeshIndexBufferRanges::FPerLODInfo& DestModel = IndexBufferRanges[BoneIndex].LODModels[I];
					FSkeletalMeshIndexBufferRanges::FPerSectionInfo& DestSection = DestModel.Sections[S];

					bool bJoined = false;
					for (FInt32Range& Existing : DestSection.Regions)
					{
						if (Existing.Contiguous(CurIndexRange))
						{
							Existing = FInt32Range::Hull(Existing, CurIndexRange);
							bJoined = true;
							break;
						}
					}
					if (!bJoined)
					{
						DestSection.Regions.Add(CurIndexRange);
					}
				}
			}
		}
	}
}
//nv end
#endif // #if WITH_EDITOR

void USkeletalMesh::ReleaseCPUResources()
{
	FSkeletalMeshResource* Resource = GetImportedResource();
	for(int32 Index = 0; Index < Resource->LODModels.Num(); ++Index)
	{
		Resource->LODModels[Index].ReleaseCPUResources();
	}
}

/** Allocate and initialise bone mirroring table for this skeletal mesh. Default is source = destination for each bone. */
void USkeletalMesh::InitBoneMirrorInfo()
{
	SkelMirrorTable.Empty(RefSkeleton.GetNum());
	SkelMirrorTable.AddZeroed(RefSkeleton.GetNum());

	// By default, no bone mirroring, and source is ourself.
	for(int32 i=0; i<SkelMirrorTable.Num(); i++)
	{
		SkelMirrorTable[i].SourceIndex = i;
	}
}

/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::CopyMirrorTableFrom(USkeletalMesh* SrcMesh)
{
	// Do nothing if no mirror table in source mesh
	if(SrcMesh->SkelMirrorTable.Num() == 0)
	{
		return;
	}

	// First, allocate and default mirroring table.
	InitBoneMirrorInfo();

	// Keep track of which entries in the source we have already copied
	TArray<bool> EntryCopied;
	EntryCopied.AddZeroed( SrcMesh->SkelMirrorTable.Num() );

	// Mirror table must always be size of ref skeleton.
	check(SrcMesh->SkelMirrorTable.Num() == SrcMesh->RefSkeleton.GetNum());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(int32 i=0; i<SrcMesh->SkelMirrorTable.Num(); i++)
	{
		if(!EntryCopied[i])
		{
			// Get name of source and dest bone for this entry in the source table.
			FName DestBoneName = SrcMesh->RefSkeleton.GetBoneName(i);
			int32 SrcBoneIndex = SrcMesh->SkelMirrorTable[i].SourceIndex;
			FName SrcBoneName = SrcMesh->RefSkeleton.GetBoneName(SrcBoneIndex);
			EAxis::Type FlipAxis = SrcMesh->SkelMirrorTable[i].BoneFlipAxis;

			// Look up bone names in target mesh (this one)
			int32 DestBoneIndexTarget = RefSkeleton.FindBoneIndex(DestBoneName);
			int32 SrcBoneIndexTarget = RefSkeleton.FindBoneIndex(SrcBoneName);

			// If both bones found, copy data to this mesh's mirror table.
			if( DestBoneIndexTarget != INDEX_NONE && SrcBoneIndexTarget != INDEX_NONE )
			{
				SkelMirrorTable[DestBoneIndexTarget].SourceIndex = SrcBoneIndexTarget;
				SkelMirrorTable[DestBoneIndexTarget].BoneFlipAxis = FlipAxis;


				SkelMirrorTable[SrcBoneIndexTarget].SourceIndex = DestBoneIndexTarget;
				SkelMirrorTable[SrcBoneIndexTarget].BoneFlipAxis = FlipAxis;

				// Flag entries as copied, so we don't try and do it again.
				EntryCopied[i] = true;
				EntryCopied[SrcBoneIndex] = true;
			}
		}
	}
}

/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::ExportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo)
{
	// Do nothing if no mirror table in source mesh
	if( SkelMirrorTable.Num() == 0 )
	{
		return;
	}
	
	// Mirror table must always be size of ref skeleton.
	check(SkelMirrorTable.Num() == RefSkeleton.GetNum());

	MirrorExportInfo.Empty(SkelMirrorTable.Num());
	MirrorExportInfo.AddZeroed(SkelMirrorTable.Num());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(int32 i=0; i<SkelMirrorTable.Num(); i++)
	{
		MirrorExportInfo[i].BoneName		= RefSkeleton.GetBoneName(i);
		MirrorExportInfo[i].SourceBoneName	= RefSkeleton.GetBoneName(SkelMirrorTable[i].SourceIndex);
		MirrorExportInfo[i].BoneFlipAxis	= SkelMirrorTable[i].BoneFlipAxis;
	}
}


/** Utility for copying and converting a mirroring table from another SkeletalMesh. */
void USkeletalMesh::ImportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo)
{
	// Do nothing if no mirror table in source mesh
	if( MirrorExportInfo.Num() == 0 )
	{
		return;
	}

	// First, allocate and default mirroring table.
	InitBoneMirrorInfo();

	// Keep track of which entries in the source we have already copied
	TArray<bool> EntryCopied;
	EntryCopied.AddZeroed( RefSkeleton.GetNum() );

	// Mirror table must always be size of ref skeleton.
	check(SkelMirrorTable.Num() == RefSkeleton.GetNum());

	// Iterate over each entry in the source mesh mirror table.
	// We assume that the src table is correct, and don't check for errors here (ie two bones using the same one as source).
	for(int32 i=0; i<MirrorExportInfo.Num(); i++)
	{
		FName DestBoneName	= MirrorExportInfo[i].BoneName;
		int32 DestBoneIndex	= RefSkeleton.FindBoneIndex(DestBoneName);

		if( DestBoneIndex != INDEX_NONE && !EntryCopied[DestBoneIndex] )
		{
			FName SrcBoneName	= MirrorExportInfo[i].SourceBoneName;
			int32 SrcBoneIndex	= RefSkeleton.FindBoneIndex(SrcBoneName);
			EAxis::Type FlipAxis		= MirrorExportInfo[i].BoneFlipAxis;

			// If both bones found, copy data to this mesh's mirror table.
			if( SrcBoneIndex != INDEX_NONE )
			{
				SkelMirrorTable[DestBoneIndex].SourceIndex = SrcBoneIndex;
				SkelMirrorTable[DestBoneIndex].BoneFlipAxis = FlipAxis;

				SkelMirrorTable[SrcBoneIndex].SourceIndex = DestBoneIndex;
				SkelMirrorTable[SrcBoneIndex].BoneFlipAxis = FlipAxis;

				// Flag entries as copied, so we don't try and do it again.
				EntryCopied[DestBoneIndex]	= true;
				EntryCopied[SrcBoneIndex]	= true;
			}
		}
	}
}

/** 
 *	Utility for checking that the bone mirroring table of this mesh is good.
 *	Return true if mirror table is OK, false if there are problems.
 *	@param	ProblemBones	Output string containing information on bones that are currently bad.
 */
bool USkeletalMesh::MirrorTableIsGood(FString& ProblemBones)
{
	TArray<int32>	BadBoneMirror;

	for(int32 i=0; i<SkelMirrorTable.Num(); i++)
	{
		int32 SrcIndex = SkelMirrorTable[i].SourceIndex;
		if( SkelMirrorTable[SrcIndex].SourceIndex != i)
		{
			BadBoneMirror.Add(i);
		}
	}

	if(BadBoneMirror.Num() > 0)
	{
		for(int32 i=0; i<BadBoneMirror.Num(); i++)
		{
			int32 BoneIndex = BadBoneMirror[i];
			FName BoneName = RefSkeleton.GetBoneName(BoneIndex);

			ProblemBones += FString::Printf( TEXT("%s (%d)\n"), *BoneName.ToString(), BoneIndex );
		}

		return false;
	}
	else
	{
		return true;
	}
}

void USkeletalMesh::CreateBodySetup()
{
	if (BodySetup == nullptr)
	{
		BodySetup = NewObject<UBodySetup>(this);
		BodySetup->bSharedCookedData = true;
	}
}

UBodySetup* USkeletalMesh::GetBodySetup()
{
	CreateBodySetup();
	return BodySetup;
}

#if WITH_EDITOR
void USkeletalMesh::BuildPhysicsData()
{
	CreateBodySetup();
	BodySetup->CookedFormatData.FlushData();	//we need to force a re-cook because we're essentially re-creating the bodysetup so that it swaps whether or not it has a trimesh
	BodySetup->InvalidatePhysicsData();
	BodySetup->CreatePhysicsMeshes();
}
#endif

bool USkeletalMesh::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return bEnablePerPolyCollision;
}

bool USkeletalMesh::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool bInUseAllTriData)
{
#if WITH_EDITORONLY_DATA

	// Fail if no mesh or not per poly collision
	if (!ImportedResource.IsValid() || !bEnablePerPolyCollision)
	{
		return false;
	}

	const FStaticLODModel& Model = ImportedResource->LODModels[0];

	{
		// Copy all verts into collision vertex buffer.
		CollisionData->Vertices.Empty();
		CollisionData->Vertices.AddUninitialized(Model.NumVertices);
		const uint32 NumSections = Model.Sections.Num();

		for (uint32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
				{
			const FSkelMeshSection& Section = Model.Sections[SectionIdx];
			{
				//soft
				const uint32 SoftOffset = Section.GetVertexBufferIndex();
				const uint32 NumSoftVerts = Section.GetNumVertices();
				for (uint32 SoftIdx = 0; SoftIdx < NumSoftVerts; ++SoftIdx)
				{
					CollisionData->Vertices[SoftIdx + SoftOffset] = Section.SoftVertices[SoftIdx].Position;
				}
			}

		}
	}

	{
		// Copy indices into collision index buffer
		const FMultiSizeIndexContainer& IndexBufferContainer = Model.MultiSizeIndexContainer;

		TArray<uint32> Indices;
		IndexBufferContainer.GetIndexBuffer(Indices);

		const uint32 NumTris = Indices.Num() / 3;
		CollisionData->Indices.Empty();
		CollisionData->Indices.Reserve(NumTris);

		FTriIndices TriIndex;
		for (int32 SectionIndex = 0; SectionIndex < Model.Sections.Num(); ++SectionIndex)
		{
			const FSkelMeshSection& Section = Model.Sections[SectionIndex];

			const uint32 OnePastLastIndex = Section.BaseIndex + Section.NumTriangles * 3;

			for (uint32 i = Section.BaseIndex; i < OnePastLastIndex; i += 3)
			{
				TriIndex.v0 = Indices[i];
				TriIndex.v1 = Indices[i + 1];
				TriIndex.v2 = Indices[i + 2];

				CollisionData->Indices.Add(TriIndex);
				CollisionData->MaterialIndices.Add(Section.MaterialIndex);
			}
		}
	}

	CollisionData->bFlipNormals = true;
	CollisionData->bDeformableMesh = true;

	// We only have a valid TriMesh if the CollisionData has vertices AND indices. For meshes with disabled section collision, it
	// can happen that the indices will be empty, in which case we do not want to consider that as valid trimesh data
	return CollisionData->Vertices.Num() > 0 && CollisionData->Indices.Num() > 0;
#else // #if WITH_EDITORONLY_DATA
	return false;
#endif // #if WITH_EDITORONLY_DATA
}

void USkeletalMesh::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* USkeletalMesh::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void USkeletalMesh::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* USkeletalMesh::GetAssetUserDataArray() const
{
	return &AssetUserData;
}

////// SKELETAL MESH THUMBNAIL SUPPORT ////////

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString USkeletalMesh::GetDesc()
{
	FSkeletalMeshResource* Resource = GetImportedResource();
	check(Resource->LODModels.Num() > 0);
	return FString::Printf( TEXT("%d Triangles, %d Bones"), Resource->LODModels[0].GetTotalFaces(), RefSkeleton.GetRawBoneNum() );
}

bool USkeletalMesh::IsSectionUsingCloth(int32 InSectionIndex, bool bCheckCorrespondingSections) const
{
	if(ImportedResource.IsValid())
	{
		for(FStaticLODModel& LodModel : ImportedResource->LODModels)
		{
			if(LodModel.Sections.IsValidIndex(InSectionIndex))
			{
				FSkelMeshSection* SectionToCheck = &LodModel.Sections[InSectionIndex];
				if(SectionToCheck->bDisabled && bCheckCorrespondingSections)
				{
					SectionToCheck = &LodModel.Sections[SectionToCheck->CorrespondClothSectionIndex];
				}
				
				if(SectionToCheck->HasClothingData())
				{
					return true;
				}
			}
		}
	}

	return false;
}

#if WITH_EDITOR
void USkeletalMesh::AddBoneToReductionSetting(int32 LODIndex, const TArray<FName>& BoneNames)
{
	if (LODInfo.IsValidIndex(LODIndex))
	{
		for (auto& BoneName : BoneNames)
		{
			LODInfo[LODIndex].BonesToRemove.AddUnique(BoneName);
		}
	}
}
void USkeletalMesh::AddBoneToReductionSetting(int32 LODIndex, FName BoneName)
{
	if (LODInfo.IsValidIndex(LODIndex))
	{
		LODInfo[LODIndex].BonesToRemove.AddUnique(BoneName);
	}
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void USkeletalMesh::ConvertLegacyLODScreenSize()
{
	if (LODInfo.Num() == 1)
	{
		// Only one LOD
		LODInfo[0].ScreenSize = 1.0f;
	}
	else
	{
		// Use 1080p, 90 degree FOV as a default, as this should not cause runtime regressions in the common case.
		// LODs will appear different in Persona, however.
		const float HalfFOV = PI * 0.25f;
		const float ScreenWidth = 1920.0f;
		const float ScreenHeight = 1080.0f;
		const FPerspectiveMatrix ProjMatrix(HalfFOV, ScreenWidth, ScreenHeight, 1.0f);
		FBoxSphereBounds Bounds = GetBounds();

		// Multiple models, we should have LOD screen area data.
		for (int32 LODIndex = 0; LODIndex < LODInfo.Num(); ++LODIndex)
		{
			FSkeletalMeshLODInfo& LODInfoEntry = LODInfo[LODIndex];

			if (bRequiresLODScreenSizeConversion)
			{
				if (LODInfoEntry.ScreenSize == 0.0f)
				{
					LODInfoEntry.ScreenSize = 1.0f;
				}
				else
				{
					// legacy screen size was scaled by a fixed constant of 320.0f, so its kinda arbitrary. Convert back to distance based metric first.
					const float ScreenDepth = FMath::Max(ScreenWidth / 2.0f * ProjMatrix.M[0][0], ScreenHeight / 2.0f * ProjMatrix.M[1][1]) * Bounds.SphereRadius / (LODInfoEntry.ScreenSize * 320.0f);

					// Now convert using the query function
					LODInfoEntry.ScreenSize = ComputeBoundsScreenSize(FVector::ZeroVector, Bounds.SphereRadius, FVector(0.0f, 0.0f, ScreenDepth), ProjMatrix);
				}
			}

			if (bRequiresLODHysteresisConversion)
			{
				if (LODInfoEntry.LODHysteresis != 0.0f)
				{
					// Also convert the hysteresis as if it was a screen size topo
					const float ScreenHysteresisDepth = FMath::Max(ScreenWidth / 2.0f * ProjMatrix.M[0][0], ScreenHeight / 2.0f * ProjMatrix.M[1][1]) * Bounds.SphereRadius / (LODInfoEntry.LODHysteresis * 320.0f);
					LODInfoEntry.LODHysteresis = ComputeBoundsScreenSize(FVector::ZeroVector, Bounds.SphereRadius, FVector(0.0f, 0.0f, ScreenHysteresisDepth), ProjMatrix);
				}
			}
		}
	}
}
#endif

class UNodeMappingContainer* USkeletalMesh::GetNodeMappingContainer(class UBlueprint* SourceAsset) const
{
	for (int32 Index = 0; Index < NodeMappingData.Num(); ++Index)
	{
		UNodeMappingContainer* Iter = NodeMappingData[Index];
		if (Iter && Iter->GetSourceAsset() == SourceAsset)
		{
			return Iter;
		}
	}

	return nullptr;
}
/*-----------------------------------------------------------------------------
USkeletalMeshSocket
-----------------------------------------------------------------------------*/
USkeletalMeshSocket::USkeletalMeshSocket(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bForceAlwaysAnimated(true)
{
	RelativeScale = FVector(1.0f, 1.0f, 1.0f);
}

void USkeletalMeshSocket::InitializeSocketFromLocation(const class USkeletalMeshComponent* SkelComp, FVector WorldLocation, FVector WorldNormal)
{
	if (ensureAsRuntimeWarning(SkelComp))
	{
		BoneName = SkelComp->FindClosestBone(WorldLocation);
		if (BoneName != NAME_None)
		{
			SkelComp->TransformToBoneSpace(BoneName, WorldLocation, WorldNormal.Rotation(), RelativeLocation, RelativeRotation);
		}
	}
}

FVector USkeletalMeshSocket::GetSocketLocation(const class USkeletalMeshComponent* SkelComp) const
{
	if (ensureAsRuntimeWarning(SkelComp))
	{
		FMatrix SocketMatrix;
		if (GetSocketMatrix(SocketMatrix, SkelComp))
		{
			return SocketMatrix.GetOrigin();
		}

		// Fall back to MeshComp origin, so it's visible in case of failure.
		return SkelComp->GetComponentLocation();
	}
	return FVector(0.f);
}

bool USkeletalMeshSocket::GetSocketMatrix(FMatrix& OutMatrix, const class USkeletalMeshComponent* SkelComp) const
{
	const int32 BoneIndex = SkelComp ? SkelComp->GetBoneIndex(BoneName) : INDEX_NONE;
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		FScaleRotationTranslationMatrix RelSocketMatrix( RelativeScale, RelativeRotation, RelativeLocation );
		OutMatrix = RelSocketMatrix * BoneMatrix;
		return true;
	}

	return false;
}

FTransform USkeletalMeshSocket::GetSocketLocalTransform() const
{
	return FTransform(RelativeRotation, RelativeLocation, RelativeScale);
}

FTransform USkeletalMeshSocket::GetSocketTransform(const class USkeletalMeshComponent* SkelComp) const
{
	FTransform OutTM;

	const int32 BoneIndex = SkelComp ? SkelComp->GetBoneIndex(BoneName) : INDEX_NONE;
	if(BoneIndex != INDEX_NONE)
	{
		FTransform BoneTM = SkelComp->GetBoneTransform(BoneIndex);
		FTransform RelSocketTM( RelativeRotation, RelativeLocation, RelativeScale );
		OutTM = RelSocketTM * BoneTM;
	}

	return OutTM;
}

bool USkeletalMeshSocket::GetSocketMatrixWithOffset(FMatrix& OutMatrix, class USkeletalMeshComponent* SkelComp, const FVector& InOffset, const FRotator& InRotation) const
{
	const int32 BoneIndex = SkelComp ? SkelComp->GetBoneIndex(BoneName) : INDEX_NONE;
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		FScaleRotationTranslationMatrix RelSocketMatrix(RelativeScale, RelativeRotation, RelativeLocation);
		FRotationTranslationMatrix RelOffsetMatrix(InRotation, InOffset);
		OutMatrix = RelOffsetMatrix * RelSocketMatrix * BoneMatrix;
		return true;
	}

	return false;
}


bool USkeletalMeshSocket::GetSocketPositionWithOffset(FVector& OutPosition, class USkeletalMeshComponent* SkelComp, const FVector& InOffset, const FRotator& InRotation) const
{
	const int32 BoneIndex = SkelComp ? SkelComp->GetBoneIndex(BoneName) : INDEX_NONE;
	if(BoneIndex != INDEX_NONE)
	{
		FMatrix BoneMatrix = SkelComp->GetBoneMatrix(BoneIndex);
		FScaleRotationTranslationMatrix RelSocketMatrix(RelativeScale, RelativeRotation, RelativeLocation);
		FRotationTranslationMatrix RelOffsetMatrix(InRotation, InOffset);
		FMatrix SocketMatrix = RelOffsetMatrix * RelSocketMatrix * BoneMatrix;
		OutPosition = SocketMatrix.GetOrigin();
		return true;
	}

	return false;
}

/** 
 *	Utility to associate an actor with a socket
 *	
 *	@param	Actor			The actor to attach to the socket
 *	@param	SkelComp		The skeletal mesh component that the socket comes from
 *
 *	@return	bool			true if successful, false if not
 */
bool USkeletalMeshSocket::AttachActor(AActor* Actor, class USkeletalMeshComponent* SkelComp) const
{
	bool bAttached = false;
	if (ensureAlways(SkelComp))
	{
		// Don't support attaching to own socket
		if ((Actor != SkelComp->GetOwner()) && Actor->GetRootComponent())
		{
			FMatrix SocketTM;
			if (GetSocketMatrix(SocketTM, SkelComp))
			{
				Actor->Modify();

				Actor->SetActorLocation(SocketTM.GetOrigin(), false);
				Actor->SetActorRotation(SocketTM.Rotator());
				Actor->GetRootComponent()->AttachToComponent(SkelComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

	#if WITH_EDITOR
				if (GIsEditor)
				{
					Actor->PreEditChange(NULL);
					Actor->PostEditChange();
				}
	#endif // WITH_EDITOR

				bAttached = true;
			}
		}
	}
	return bAttached;
}

#if WITH_EDITOR
void USkeletalMeshSocket::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ChangedEvent.Broadcast(this, PropertyChangedEvent.MemberProperty);
	}
}

void USkeletalMeshSocket::CopyFrom(const class USkeletalMeshSocket* OtherSocket)
{
	if (OtherSocket)
	{
		SocketName = OtherSocket->SocketName;
		BoneName = OtherSocket->BoneName;
		RelativeLocation = OtherSocket->RelativeLocation;
		RelativeRotation = OtherSocket->RelativeRotation;
		RelativeScale = OtherSocket->RelativeScale;
		bForceAlwaysAnimated = OtherSocket->bForceAlwaysAnimated;
	}
}

#endif

void USkeletalMeshSocket::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	if(Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MeshSocketScaleUtilization)
	{
		// Set the relative scale to 1.0. As it was not used before this should allow existing data
		// to work as expected.
		RelativeScale = FVector(1.0f, 1.0f, 1.0f);
	}
}


/*-----------------------------------------------------------------------------
FSkeletalMeshSceneProxy
-----------------------------------------------------------------------------*/
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"

const FQuat SphylBasis(FVector(1.0f / FMath::Sqrt(2.0f), 0.0f, 1.0f / FMath::Sqrt(2.0f)), PI);

/** 
 * Constructor. 
 * @param	Component - skeletal mesh primitive being added
 */
FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshResource* InSkelMeshResource)
		:	FPrimitiveSceneProxy(Component, Component->SkeletalMesh->GetFName())
		,	Owner(Component->GetOwner())
		,	MeshObject(Component->MeshObject)
		,	SkelMeshResource(InSkelMeshResource)
		,	SkeletalMeshForDebug(Component->SkeletalMesh)
		,	PhysicsAssetForDebug(Component->GetPhysicsAsset())
		,	bForceWireframe(Component->bForceWireframe)
		,	bCanHighlightSelectedSections(Component->bCanHighlightSelectedSections)
		,	MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		,	bMaterialsNeedMorphUsage_GameThread(false)
#if WITH_EDITORONLY_DATA
		,	StreamingDistanceMultiplier(FMath::Max(0.0f, Component->StreamingDistanceMultiplier))
#endif
{
	check(MeshObject);
	check(SkelMeshResource);
	check(SkeletalMeshForDebug);

	bIsCPUSkinned = MeshObject->IsCPUSkinned();

	bCastCapsuleDirectShadow = Component->bCastDynamicShadow && Component->CastShadow && Component->bCastCapsuleDirectShadow;
	bCastsDynamicIndirectShadow = Component->bCastDynamicShadow && Component->CastShadow && Component->bCastCapsuleIndirectShadow;

	DynamicIndirectShadowMinVisibility = FMath::Clamp(Component->CapsuleIndirectShadowMinVisibility, 0.0f, 1.0f);

	// Force inset shadows if capsule shadows are requested, as they can't be supported with full scene shadows
	bCastInsetShadow = bCastInsetShadow || bCastCapsuleDirectShadow;

	const USkeletalMeshComponent* SkeletalMeshComponent = Cast<const USkeletalMeshComponent>(Component);
	if(SkeletalMeshComponent && SkeletalMeshComponent->bPerBoneMotionBlur)
	{
		bAlwaysHasVelocity = true;
	}

	const auto FeatureLevel = GetScene().GetFeatureLevel();

	// setup materials and performance classification for each LOD.
	extern bool GForceDefaultMaterial;
	bool bCastShadow = Component->CastShadow;
	bool bAnySectionCastsShadow = false;
	LODSections.Reserve(SkelMeshResource->LODModels.Num());
	LODSections.AddZeroed(SkelMeshResource->LODModels.Num());
	for(int32 LODIdx=0; LODIdx < SkelMeshResource->LODModels.Num(); LODIdx++)
	{
		const FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIdx];
		const FSkeletalMeshLODInfo& Info = Component->SkeletalMesh->LODInfo[LODIdx];

		FLODSectionElements& LODSection = LODSections[LODIdx];

		// Presize the array
		LODSection.SectionElements.Empty( LODModel.Sections.Num() );
		for(int32 SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
		{
			const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

			// If we are at a dropped LOD, route material index through the LODMaterialMap in the LODInfo struct.
			int32 UseMaterialIndex = Section.MaterialIndex;			
			if(LODIdx > 0)
			{
				if(Section.MaterialIndex < Info.LODMaterialMap.Num())
				{
					UseMaterialIndex = Info.LODMaterialMap[Section.MaterialIndex];
					UseMaterialIndex = FMath::Clamp( UseMaterialIndex, 0, Component->SkeletalMesh->Materials.Num() );
				}
			}

			// If Section is hidden, do not cast shadow
			bool bSectionHidden = MeshObject->IsMaterialHidden(LODIdx,UseMaterialIndex);

			// Disable rendering for cloth mapped sections
			bSectionHidden |= Section.bDisabled;

			// If the material is NULL, or isn't flagged for use with skeletal meshes, it will be replaced by the default material.
			UMaterialInterface* Material = Component->GetMaterial(UseMaterialIndex);
			if (GForceDefaultMaterial && Material && !IsTranslucentBlendMode(Material->GetBlendMode()))
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= Material->GetRelevance(FeatureLevel);
			}

			// if this is a clothing section, then enabled and will be drawn but the corresponding original section should be disabled
			bool bClothSection = (!Section.bDisabled && Section.CorrespondClothSectionIndex >= 0);

			if(bClothSection)
			{
				// the cloth section's material index must be same as the original section's material index
				check(Section.MaterialIndex == LODModel.Sections[Section.CorrespondClothSectionIndex].MaterialIndex);
			}

			if(!Material || !Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh) ||
			  (bClothSection && !Material->CheckMaterialUsage_Concurrent(MATUSAGE_Clothing)))
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= Material->GetRelevance(FeatureLevel);
			}

			const bool bRequiresAdjacencyInformation = RequiresAdjacencyInformation( Material, &TGPUSkinVertexFactory<false>::StaticType, FeatureLevel );
			if ( bRequiresAdjacencyInformation && LODModel.AdjacencyMultiSizeIndexContainer.IsIndexBufferValid() == false )
			{
				UE_LOG(LogSkeletalMesh, Warning, 
					TEXT("Material %s requires adjacency information, but skeletal mesh %s does not have adjacency information built. The mesh must be rebuilt to be used with this material. The mesh will be rendered with DefaultMaterial."), 
					*Material->GetPathName(), 
					*Component->SkeletalMesh->GetPathName() )
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
				MaterialRelevance |= UMaterial::GetDefaultMaterial(MD_Surface)->GetRelevance(FeatureLevel);
			}

			bool bSectionCastsShadow = !bSectionHidden && bCastShadow &&
				(Component->SkeletalMesh->Materials.IsValidIndex(UseMaterialIndex) == false || Section.bCastShadow);

			bAnySectionCastsShadow |= bSectionCastsShadow;
			LODSection.SectionElements.Add(
				FSectionElementInfo(
					Material,
					bSectionCastsShadow,
					UseMaterialIndex
					));
			MaterialsInUse_GameThread.Add(Material);
		}
	}

	bCastDynamicShadow = bCastDynamicShadow && bAnySectionCastsShadow;

	// Try to find a color for level coloration.
	if( Owner )
	{
		ULevel* Level = Owner->GetLevel();
		ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
		if ( LevelStreaming )
		{
			LevelColor = LevelStreaming->LevelColor;
		}
	}

	// Get a color for property coloration
	FColor NewPropertyColor;
	GEngine->GetPropertyColorationColor( (UObject*)Component, NewPropertyColor );
	PropertyColor = NewPropertyColor;

	// Copy out shadow physics asset data
	if(SkeletalMeshComponent)
	{
		UPhysicsAsset* ShadowPhysicsAsset = SkeletalMeshComponent->SkeletalMesh->ShadowPhysicsAsset;

		if (ShadowPhysicsAsset
			&& SkeletalMeshComponent->CastShadow
			&& (SkeletalMeshComponent->bCastCapsuleDirectShadow || SkeletalMeshComponent->bCastCapsuleIndirectShadow))
		{
			for (int32 BodyIndex = 0; BodyIndex < ShadowPhysicsAsset->SkeletalBodySetups.Num(); BodyIndex++)
			{
				UBodySetup* BodySetup = ShadowPhysicsAsset->SkeletalBodySetups[BodyIndex];
				int32 BoneIndex = SkeletalMeshComponent->GetBoneIndex(BodySetup->BoneName);

				if (BoneIndex != INDEX_NONE)
				{
					const FMatrix& RefBoneMatrix = SkeletalMeshComponent->SkeletalMesh->GetComposedRefPoseMatrix(BoneIndex);

					const int32 NumSpheres = BodySetup->AggGeom.SphereElems.Num();
					for (int32 SphereIndex = 0; SphereIndex < NumSpheres; SphereIndex++)
					{
						const FKSphereElem& SphereShape = BodySetup->AggGeom.SphereElems[SphereIndex];
						ShadowCapsuleData.Emplace(BoneIndex, FCapsuleShape(RefBoneMatrix.TransformPosition(SphereShape.Center), SphereShape.Radius, FVector(0.0f, 0.0f, 1.0f), 0.0f));
					}

					const int32 NumCapsules = BodySetup->AggGeom.SphylElems.Num();
					for (int32 CapsuleIndex = 0; CapsuleIndex < NumCapsules; CapsuleIndex++)
					{
						const FKSphylElem& SphylShape = BodySetup->AggGeom.SphylElems[CapsuleIndex];
						ShadowCapsuleData.Emplace(BoneIndex, FCapsuleShape(RefBoneMatrix.TransformPosition(SphylShape.Center), SphylShape.Radius, RefBoneMatrix.TransformVector((SphylShape.Rotation.Quaternion() * SphylBasis).Vector()), SphylShape.Length));
					}

					if (NumSpheres > 0 || NumCapsules > 0)
					{
						ShadowCapsuleBoneIndices.AddUnique(BoneIndex);
					}
				}
			}
		}
	}

	// Sort to allow merging with other bone hierarchies
	if (ShadowCapsuleBoneIndices.Num())
	{
		ShadowCapsuleBoneIndices.Sort();
	}
}


// FPrimitiveSceneProxy interface.

/** 
 * Iterates over sections,chunks,elements based on current instance weight usage 
 */
class FSkeletalMeshSectionIter
{
public:
	FSkeletalMeshSectionIter(const int32 InLODIdx, const FSkeletalMeshObject& InMeshObject, const FStaticLODModel& InLODModel, const FSkeletalMeshSceneProxy::FLODSectionElements& InLODSectionElements)
		: SectionIndex(0)
		, MeshObject(InMeshObject)
		, LODSectionElements(InLODSectionElements)
		, Sections(InLODModel.Sections)
#if WITH_EDITORONLY_DATA
		, SectionIndexPreview(InMeshObject.SectionIndexPreview)
		, MaterialIndexPreview(InMeshObject.MaterialIndexPreview)
#endif
	{
		while (NotValidPreviewSection())
		{
			SectionIndex++;
		}
	}
	FORCEINLINE FSkeletalMeshSectionIter& operator++()
	{
		do 
		{
		SectionIndex++;
		} while (NotValidPreviewSection());
		return *this;
	}
	FORCEINLINE operator bool() const
	{
		return ((SectionIndex < Sections.Num()) && LODSectionElements.SectionElements.IsValidIndex(GetSectionElementIndex()));
	}
	FORCEINLINE const FSkelMeshSection& GetSection() const
	{
		return Sections[SectionIndex];
	}
	FORCEINLINE const FTwoVectors& GetCustomLeftRightVectors() const
	{
		return MeshObject.GetCustomLeftRightVectors(SectionIndex);
	}
	FORCEINLINE const int32 GetSectionElementIndex() const
	{
		return SectionIndex;
	}
	FORCEINLINE const FSkeletalMeshSceneProxy::FSectionElementInfo& GetSectionElementInfo() const
	{
		int32 SectionElementInfoIndex = GetSectionElementIndex();
		return LODSectionElements.SectionElements[SectionElementInfoIndex];
	}
	FORCEINLINE bool NotValidPreviewSection()
	{
#if WITH_EDITORONLY_DATA
		if (MaterialIndexPreview == INDEX_NONE)
		{
			int32 ActualPreviewSectionIdx = SectionIndexPreview;
			if (ActualPreviewSectionIdx != INDEX_NONE && Sections.IsValidIndex(ActualPreviewSectionIdx))
			{
				const FSkelMeshSection& PreviewSection = Sections[ActualPreviewSectionIdx];
				if (PreviewSection.bDisabled && PreviewSection.CorrespondClothSectionIndex != INDEX_NONE)
				{
					ActualPreviewSectionIdx = PreviewSection.CorrespondClothSectionIndex;
				}
			}

			return	(SectionIndex < Sections.Num()) &&
				((ActualPreviewSectionIdx >= 0) && (ActualPreviewSectionIdx != SectionIndex));
		}
		else
		{
			int32 ActualPreviewMaterialIdx = MaterialIndexPreview;
			int32 ActualPreviewSectionIdx = INDEX_NONE;
			if (ActualPreviewMaterialIdx != INDEX_NONE && Sections.IsValidIndex(SectionIndex))
			{
				const FSkeletalMeshSceneProxy::FSectionElementInfo& SectionInfo = LODSectionElements.SectionElements[SectionIndex];
				if (SectionInfo.UseMaterialIndex == ActualPreviewMaterialIdx)
				{
					ActualPreviewSectionIdx = SectionIndex;
				}
				if (ActualPreviewSectionIdx != INDEX_NONE)
				{
					const FSkelMeshSection& PreviewSection = Sections[ActualPreviewSectionIdx];
					if (PreviewSection.bDisabled && PreviewSection.CorrespondClothSectionIndex != INDEX_NONE)
					{
						ActualPreviewSectionIdx = PreviewSection.CorrespondClothSectionIndex;
					}
				}
			}

			return	(SectionIndex < Sections.Num()) &&
				((ActualPreviewMaterialIdx >= 0) && (ActualPreviewSectionIdx != SectionIndex));
		}
#else
		return false;
#endif
	}
private:
	int32 SectionIndex;
	const FSkeletalMeshObject& MeshObject;
	const FSkeletalMeshSceneProxy::FLODSectionElements& LODSectionElements;
	const TArray<FSkelMeshSection>& Sections;
#if WITH_EDITORONLY_DATA
	const int32 SectionIndexPreview;
	const int32 MaterialIndexPreview;
#endif
};

#if WITH_EDITOR
HHitProxy* FSkeletalMeshSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	if ( Component->GetOwner() )
	{
		if ( LODSections.Num() > 0 )
		{
			for ( int32 LODIndex = 0; LODIndex < SkelMeshResource->LODModels.Num(); LODIndex++ )
			{
				const FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

				FLODSectionElements& LODSection = LODSections[LODIndex];

				check(LODSection.SectionElements.Num() == LODModel.Sections.Num());

				for ( int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++ )
				{
					HHitProxy* ActorHitProxy;

					int32 MaterialIndex = LODModel.Sections[SectionIndex].MaterialIndex;
					if ( Component->GetOwner()->IsA(ABrush::StaticClass()) && Component->IsA(UBrushComponent::StaticClass()) )
					{
						ActorHitProxy = new HActor(Component->GetOwner(), Component, HPP_Wireframe, SectionIndex, MaterialIndex);
					}
					else
					{
						ActorHitProxy = new HActor(Component->GetOwner(), Component, SectionIndex, MaterialIndex);
					}

					// Set the hitproxy.
					check(LODSection.SectionElements[SectionIndex].HitProxy == NULL);
					LODSection.SectionElements[SectionIndex].HitProxy = ActorHitProxy;
					OutHitProxies.Add(ActorHitProxy);
				}
			}
		}
		else
		{
			return FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);
		}
	}

	return NULL;
}
#endif

void FSkeletalMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshSceneProxy_GetMeshElements);
	GetMeshElementsConditionallySelectable(Views, ViewFamily, true, VisibilityMap, Collector);
}

void FSkeletalMeshSceneProxy::GetMeshElementsConditionallySelectable(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, bool bInSelectable, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	if( !MeshObject )
	{
		return;
	}	
	MeshObject->PreGDMECallback(ViewFamily.Scene->GetGPUSkinCache(), ViewFamily.FrameNumber);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			MeshObject->UpdateMinDesiredLODLevel(View, GetBounds(), ViewFamily.FrameNumber);
		}
	}

	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;

	const int32 LODIndex = MeshObject->GetLOD();
	check(LODIndex < SkelMeshResource->LODModels.Num());
	const FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

	if( LODSections.Num() > 0 )
	{
		const FLODSectionElements& LODSection = LODSections[LODIndex];

		check(LODSection.SectionElements.Num() == LODModel.Sections.Num());

#if WITH_EDITORONLY_DATA
		//Find the real editor selected section
		int32 RealSelectedEditorSection = SkeletalMeshForDebug->SelectedEditorSection;
		if (RealSelectedEditorSection != INDEX_NONE && LODModel.Sections.IsValidIndex(SkeletalMeshForDebug->SelectedEditorSection))
		{
			const FSkelMeshSection& SelectEditorSection = LODModel.Sections[SkeletalMeshForDebug->SelectedEditorSection];
			if (SelectEditorSection.bDisabled && SelectEditorSection.CorrespondClothSectionIndex != INDEX_NONE && LODModel.Sections.IsValidIndex(SelectEditorSection.CorrespondClothSectionIndex))
			{
				RealSelectedEditorSection = SelectEditorSection.CorrespondClothSectionIndex;
			}
		}
#endif

		for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODModel, LODSection); Iter; ++Iter)
		{
			const FSkelMeshSection& Section = Iter.GetSection();
			const int32 SectionIndex = Iter.GetSectionElementIndex();
			const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();
			const FTwoVectors& CustomLeftRightVectors = Iter.GetCustomLeftRightVectors();

			bool bSectionSelected = false;

#if WITH_EDITORONLY_DATA
			// TODO: This is not threadsafe! A render command should be used to propagate SelectedEditorSection to the scene proxy.
			if (SkeletalMeshForDebug->SelectedEditorMaterial != INDEX_NONE)
			{
				bSectionSelected = (SkeletalMeshForDebug->SelectedEditorMaterial == SectionElementInfo.UseMaterialIndex);
			}
			else
			{
				bSectionSelected = (RealSelectedEditorSection == SectionIndex);
			}
			
#endif
			// If hidden skip the draw
			if (MeshObject->IsMaterialHidden(LODIndex, SectionElementInfo.UseMaterialIndex))
			{
				continue;
			}

			// If disabled, then skip the draw
			if(Section.bDisabled)
			{
				continue;
			}

			GetDynamicElementsSection(Views, ViewFamily, VisibilityMap, LODModel, LODIndex, SectionIndex, bSectionSelected, SectionElementInfo, CustomLeftRightVectors, bInSelectable, Collector);
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if( PhysicsAssetForDebug )
			{
				DebugDrawPhysicsAsset(ViewIndex, Collector, ViewFamily.EngineShowFlags);
			}

			if (EngineShowFlags.MassProperties && DebugMassData.Num() > 0)
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				const TArray<FTransform>& ComponentSpaceTransforms = *MeshObject->GetComponentSpaceTransforms();

				for (const FDebugMassData& DebugMass : DebugMassData)
				{
					if(ComponentSpaceTransforms.IsValidIndex(DebugMass.BoneIndex))
					{			
						const FTransform BoneToWorld = ComponentSpaceTransforms[DebugMass.BoneIndex] * FTransform(GetLocalToWorld());
						DebugMass.DrawDebugMass(PDI, BoneToWorld);
					}
				}
			}

			if (ViewFamily.EngineShowFlags.SkeletalMeshes)
			{
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}

			if (ViewFamily.EngineShowFlags.Bones)
			{
				DebugDrawSkeleton(ViewIndex, Collector, ViewFamily.EngineShowFlags);
			}
		}
	}
#endif
}

void FSkeletalMeshSceneProxy::GetDynamicElementsSection(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, 
	const FStaticLODModel& LODModel, const int32 LODIndex, const int32 SectionIndex, bool bSectionSelected,
	const FSectionElementInfo& SectionElementInfo, const FTwoVectors& CustomLeftRightVectors, bool bInSelectable, FMeshElementCollector& Collector ) const
{
	const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	//// If hidden skip the draw
	//if (Section.bDisabled || MeshObject->IsMaterialHidden(LODIndex,SectionElementInfo.UseMaterialIndex))
	//{
	//	return;
	//}

#if !WITH_EDITOR
	const bool bIsSelected = false;
#else // #if !WITH_EDITOR
	bool bIsSelected = IsSelected();

	// if the mesh isn't selected but the mesh section is selected in the AnimSetViewer, find the mesh component and make sure that it can be highlighted (ie. are we rendering for the AnimSetViewer or not?)
	if( !bIsSelected && bSectionSelected && bCanHighlightSelectedSections )
	{
		bIsSelected = true;
	}
#endif // #if WITH_EDITOR

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	const ERHIFeatureLevel::Type FeatureLevel = ViewFamily.GetFeatureLevel();
//#nv begin #Blast Ability to hide bones using a dynamic index buffer
	FSkeletalMeshDynamicOverride* DynamicOverride = MeshObject->GetSkeletalMeshDynamicOverride();
	FDynamicLODModelOverride* LODModelDynamicOverride = nullptr;
	FSkelMeshSectionOverride* SectionDynamicOverride = nullptr;
	if (DynamicOverride)
	{
		LODModelDynamicOverride = &DynamicOverride->LODModels[LODIndex];
		SectionDynamicOverride = &LODModelDynamicOverride->Sections[SectionIndex];
	}
//nv end

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			Mesh.DynamicVertexData = nullptr;
			Mesh.UseDynamicData = false;
			Mesh.LCI = NULL;
			Mesh.bWireframe |= bForceWireframe;
			Mesh.Type = PT_TriangleList;
			Mesh.VertexFactory = MeshObject->GetSkinVertexFactory(View, LODIndex, SectionIndex);
			
			if(!Mesh.VertexFactory)
			{
				// hide this part
				continue;
			}

			Mesh.bSelectable = bInSelectable;
//#nv begin #Blast Ability to hide bones using a dynamic index buffer
			int32 SectionNumTriangles;
			if (SectionDynamicOverride) //If one is valid both are valid, no need to check both
			{
				BatchElement.FirstIndex = SectionDynamicOverride->BaseIndex;
				BatchElement.IndexBuffer = LODModelDynamicOverride->MultiSizeIndexContainer.GetIndexBuffer();
				SectionNumTriangles = SectionDynamicOverride->NumTriangles;
				if (SectionNumTriangles == 0)
				{
					continue;
				}
			}
			else
			{
				BatchElement.FirstIndex = Section.BaseIndex;
				BatchElement.IndexBuffer = LODModel.MultiSizeIndexContainer.GetIndexBuffer();
				SectionNumTriangles = Section.NumTriangles;
			}
//nv end

			BatchElement.MaxVertexIndex = LODModel.NumVertices - 1;
			BatchElement.VertexFactoryUserData = FGPUSkinCache::GetFactoryUserData(MeshObject->SkinCacheEntry, SectionIndex);

			const bool bRequiresAdjacencyInformation = RequiresAdjacencyInformation( SectionElementInfo.Material, Mesh.VertexFactory->GetType(), ViewFamily.GetFeatureLevel() );
			if ( bRequiresAdjacencyInformation )
			{
//#nv begin #Blast Ability to hide bones using a dynamic index buffer
				if (LODModelDynamicOverride)
				{
					check(LODModelDynamicOverride->AdjacencyMultiSizeIndexContainer.IsIndexBufferValid());
					BatchElement.IndexBuffer = LODModelDynamicOverride->AdjacencyMultiSizeIndexContainer.GetIndexBuffer();
				}
				else
				{
					check(LODModel.AdjacencyMultiSizeIndexContainer.IsIndexBufferValid());
					BatchElement.IndexBuffer = LODModel.AdjacencyMultiSizeIndexContainer.GetIndexBuffer();
				}
//nv end
				Mesh.Type = PT_12_ControlPointPatchList;
				BatchElement.FirstIndex *= 4;
			}

			Mesh.MaterialRenderProxy = SectionElementInfo.Material->GetRenderProxy(false, IsHovered());
		#if WITH_EDITOR
			Mesh.BatchHitProxyId = SectionElementInfo.HitProxy ? SectionElementInfo.HitProxy->Id : FHitProxyId();

			if (bSectionSelected && bCanHighlightSelectedSections)
			{
				Mesh.bUseSelectionOutline = true;
			}
			else
			{
				Mesh.bUseSelectionOutline = !bCanHighlightSelectedSections && bIsSelected;
			}
		#endif

			BatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();

			// Select which indices to use if TRISORT_CustomLeftRight
			if( Section.TriangleSorting == TRISORT_CustomLeftRight )
			{
				switch( MeshObject->CustomSortAlternateIndexMode )
				{
				case CSAIM_Left:
					// Left view - use second set of indices.
					BatchElement.FirstIndex += SectionNumTriangles * 3; //#nv #Blast Ability to hide bones using a dynamic index buffer
					break;
				case  CSAIM_Right:
					// Right view - use first set of indices.
					break;
				default:
					// Calculate the viewing direction
					FVector SortWorldOrigin = GetLocalToWorld().TransformPosition(CustomLeftRightVectors.v1);
					FVector SortWorldDirection = GetLocalToWorld().TransformVector(CustomLeftRightVectors.v2);

					if( (SortWorldDirection | (SortWorldOrigin - View->ViewMatrices.GetViewOrigin())) < 0.f )
					{
						BatchElement.FirstIndex += SectionNumTriangles * 3; //#nv #Blast Ability to hide bones using a dynamic index buffer
					}
					break;
				}
			}

			BatchElement.NumPrimitives = SectionNumTriangles; //#nv #Blast Ability to hide bones using a dynamic index buffer

#if WITH_EDITORONLY_DATA
			if( GIsEditor && MeshObject->ProgressiveDrawingFraction != 1.f )
			{
				if (Mesh.MaterialRenderProxy && Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->GetBlendMode() == BLEND_Translucent)
				{
					BatchElement.NumPrimitives = FMath::RoundToInt(((float)BatchElement.NumPrimitives)*FMath::Clamp<float>(MeshObject->ProgressiveDrawingFraction,0.f,1.f)); //#nv #Blast Ability to hide bones using a dynamic index buffer
					if( BatchElement.NumPrimitives == 0 )
					{
						continue;
					}
				}
			}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bIsSelected)
			{
				if (ViewFamily.EngineShowFlags.VertexColors && AllowDebugViewmodes())
				{
					// Override the mesh's material with our material that draws the vertex colors
					UMaterial* VertexColorVisualizationMaterial = NULL;
					switch (GVertexColorViewMode)
					{
					case EVertexColorViewMode::Color:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_ColorOnly;
						break;

					case EVertexColorViewMode::Alpha:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_AlphaAsColor;
						break;

					case EVertexColorViewMode::Red:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_RedOnly;
						break;

					case EVertexColorViewMode::Green:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_GreenOnly;
						break;

					case EVertexColorViewMode::Blue:
						VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_BlueOnly;
						break;
					}
					check(VertexColorVisualizationMaterial != NULL);

					auto VertexColorVisualizationMaterialInstance = new FColoredMaterialRenderProxy(
						VertexColorVisualizationMaterial->GetRenderProxy(Mesh.MaterialRenderProxy->IsSelected(), Mesh.MaterialRenderProxy->IsHovered()),
						GetSelectionColor(FLinearColor::White, bSectionSelected, IsHovered())
					);

					Collector.RegisterOneFrameMaterialProxy(VertexColorVisualizationMaterialInstance);
					Mesh.MaterialRenderProxy = VertexColorVisualizationMaterialInstance;
				}
			}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif // WITH_EDITORONLY_DATA

			BatchElement.MinVertexIndex = Section.BaseVertexIndex;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.CastShadow = SectionElementInfo.bEnableShadowCasting;

			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.bUseWireframeSelectionColoring = bIsSelected;

		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			BatchElement.VisualizeElementIndex = SectionIndex;
			Mesh.VisualizeLODIndex = LODIndex;
		#endif

			if ( ensureMsgf(Mesh.MaterialRenderProxy, TEXT("GetDynamicElementsSection with invalid MaterialRenderProxy. Owner:%s LODIndex:%d UseMaterialIndex:%d"), *GetOwnerName().ToString(), LODIndex, SectionElementInfo.UseMaterialIndex) &&
				 ensureMsgf(Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel), TEXT("GetDynamicElementsSection with invalid FMaterial. Owner:%s LODIndex:%d UseMaterialIndex:%d"), *GetOwnerName().ToString(), LODIndex, SectionElementInfo.UseMaterialIndex) )
			{
				Collector.AddMesh(ViewIndex, Mesh);
			}

			const int32 NumVertices = Section.GetNumVertices();
			INC_DWORD_STAT_BY(STAT_GPUSkinVertices,(uint32)(bIsCPUSkinned ? 0 : NumVertices));
			INC_DWORD_STAT_BY(STAT_SkelMeshTriangles,Mesh.GetNumPrimitives());
			INC_DWORD_STAT(STAT_SkelMeshDrawCalls);
		}
	}
}

bool FSkeletalMeshSceneProxy::HasDynamicIndirectShadowCasterRepresentation() const
{
	return CastsDynamicShadow() && CastsDynamicIndirectShadow();
}

void FSkeletalMeshSceneProxy::GetShadowShapes(TArray<FCapsuleShape>& CapsuleShapes) const 
{
	SCOPE_CYCLE_COUNTER(STAT_GetShadowShapes);

	const TArray<FMatrix>& ReferenceToLocalMatrices = MeshObject->GetReferenceToLocalMatrices();
	const FMatrix& ProxyLocalToWorld = GetLocalToWorld();

	int32 CapsuleIndex = CapsuleShapes.Num();
	CapsuleShapes.SetNum(CapsuleShapes.Num() + ShadowCapsuleData.Num(), false);

	for(const TPair<int32, FCapsuleShape>& CapsuleData : ShadowCapsuleData)
	{
		FMatrix ReferenceToWorld = ReferenceToLocalMatrices[CapsuleData.Key] * ProxyLocalToWorld;
		const float MaxScale = ReferenceToWorld.GetScaleVector().GetMax();

		FCapsuleShape& NewCapsule = CapsuleShapes[CapsuleIndex++];

		NewCapsule.Center = ReferenceToWorld.TransformPosition(CapsuleData.Value.Center);
		NewCapsule.Radius = CapsuleData.Value.Radius * MaxScale;
		NewCapsule.Orientation = ReferenceToWorld.TransformVector(CapsuleData.Value.Orientation).GetSafeNormal();
		NewCapsule.Length = CapsuleData.Value.Length * MaxScale;
	}
}

/**
 * Returns the world transform to use for drawing.
 * @param OutLocalToWorld - Will contain the local-to-world transform when the function returns.
 * @param OutWorldToLocal - Will contain the world-to-local transform when the function returns.
 */
bool FSkeletalMeshSceneProxy::GetWorldMatrices( FMatrix& OutLocalToWorld, FMatrix& OutWorldToLocal ) const
{
	OutLocalToWorld = GetLocalToWorld();
	if (OutLocalToWorld.GetScaledAxis(EAxis::X).IsNearlyZero(SMALL_NUMBER) &&
		OutLocalToWorld.GetScaledAxis(EAxis::Y).IsNearlyZero(SMALL_NUMBER) &&
		OutLocalToWorld.GetScaledAxis(EAxis::Z).IsNearlyZero(SMALL_NUMBER))
	{
		return false;
	}
	OutWorldToLocal = GetLocalToWorld().InverseFast();
	return true;
}

/**
 * Relevance is always dynamic for skel meshes unless they are disabled
 */
FPrimitiveViewRelevance FSkeletalMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.SkeletalMeshes;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	
	MaterialRelevance.SetPrimitiveViewRelevance(Result);

#if !UE_BUILD_SHIPPING
	Result.bSeparateTranslucencyRelevance |= View->Family->EngineShowFlags.Constraints;
#endif

	return Result;
}

bool FSkeletalMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest && !ShouldRenderCustomDepth();
}

/** Util for getting LOD index currently used by this SceneProxy. */
int32 FSkeletalMeshSceneProxy::GetCurrentLODIndex()
{
	if(MeshObject)
	{
		return MeshObject->GetLOD();
	}
	else
	{
		return 0;
	}
}


/** 
 * Render physics asset for debug display
 */
void FSkeletalMeshSceneProxy::DebugDrawPhysicsAsset(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const
{
	FMatrix ProxyLocalToWorld, WorldToLocal;
	if (!GetWorldMatrices(ProxyLocalToWorld, WorldToLocal))
	{
		return; // Cannot draw this, world matrix not valid
	}

	FMatrix ScalingMatrix = ProxyLocalToWorld;
	FVector TotalScale = ScalingMatrix.ExtractScaling();

	// Only if valid
	if( !TotalScale.IsNearlyZero() )
	{
		FTransform LocalToWorldTransform(ProxyLocalToWorld);

		TArray<FTransform>* BoneSpaceBases = MeshObject->GetComponentSpaceTransforms();
		if(BoneSpaceBases)
		{
			//TODO: These data structures are not double buffered. This is not thread safe!
			check(PhysicsAssetForDebug);
			if (EngineShowFlags.Collision && IsCollisionEnabled())
			{
				PhysicsAssetForDebug->GetCollisionMesh(ViewIndex, Collector, SkeletalMeshForDebug, *BoneSpaceBases, LocalToWorldTransform, TotalScale);
			}
			if (EngineShowFlags.Constraints)
			{
				PhysicsAssetForDebug->DrawConstraints(ViewIndex, Collector, SkeletalMeshForDebug, *BoneSpaceBases, LocalToWorldTransform, TotalScale.X);
			}
		}
	}
}

void FSkeletalMeshSceneProxy::DebugDrawSkeleton(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const
{
	FMatrix ProxyLocalToWorld, WorldToLocal;
	if (!GetWorldMatrices(ProxyLocalToWorld, WorldToLocal))
	{
		return; // Cannot draw this, world matrix not valid
	}

	FTransform LocalToWorldTransform(ProxyLocalToWorld);

	auto MakeRandomColorForSkeleton = [](uint32 InUID)
	{
		FRandomStream Stream((int32)InUID);
		const uint8 Hue = (uint8)(Stream.FRand()*255.f);
		return FLinearColor::FGetHSV(Hue, 0, 255);
	};

	FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
	TArray<FTransform>& ComponentSpaceTransforms = *MeshObject->GetComponentSpaceTransforms();

	for (int32 Index = 0; Index < ComponentSpaceTransforms.Num(); ++Index)
	{
		const int32 ParentIndex = SkeletalMeshForDebug->RefSkeleton.GetParentIndex(Index);
		FVector Start, End;
		
		FLinearColor LineColor = MakeRandomColorForSkeleton(GetPrimitiveComponentId().PrimIDValue);
		const FTransform Transform = ComponentSpaceTransforms[Index] * LocalToWorldTransform;

		if (ParentIndex >= 0)
		{
			Start = (ComponentSpaceTransforms[ParentIndex] * LocalToWorldTransform).GetLocation();
			End = Transform.GetLocation();
		}
		else
		{
			Start = LocalToWorldTransform.GetLocation();
			End = Transform.GetLocation();
		}

		if(EngineShowFlags.Bones)
		{
			if(CVarDebugDrawSimpleBones.GetValueOnRenderThread() != 0)
			{
				PDI->DrawLine(Start, End, LineColor, SDPG_Foreground, 0.0f, 1.0f);
			}
			else
			{
				SkeletalDebugRendering::DrawWireBone(PDI, Start, End, LineColor, SDPG_Foreground);
			}

			if(CVarDebugDrawBoneAxes.GetValueOnRenderThread() != 0)
			{
				SkeletalDebugRendering::DrawAxes(PDI, Transform, SDPG_Foreground);
			}
		}
	}
}

/**
* Updates morph material usage for materials referenced by each LOD entry
*
* @param bNeedsMorphUsage - true if the materials used by this skeletal mesh need morph target usage
*/
void FSkeletalMeshSceneProxy::UpdateMorphMaterialUsage_GameThread(bool bNeedsMorphUsage)
{
	if( bNeedsMorphUsage != bMaterialsNeedMorphUsage_GameThread )
	{
		// keep track of current morph material usage for the proxy
		bMaterialsNeedMorphUsage_GameThread = bNeedsMorphUsage;

		TSet<UMaterialInterface*> MaterialsToSwap;
		for (auto It = MaterialsInUse_GameThread.CreateConstIterator(); It; ++It)
		{
			UMaterialInterface* Material = *It;
			if (Material)
			{
				const bool bCheckMorphUsage = !bMaterialsNeedMorphUsage_GameThread || (bMaterialsNeedMorphUsage_GameThread && Material->CheckMaterialUsage_Concurrent(MATUSAGE_MorphTargets));
				const bool bCheckSkelUsage = Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh);
				// make sure morph material usage and default skeletal usage are both valid
				if( !bCheckMorphUsage || !bCheckSkelUsage  )
				{
					MaterialsToSwap.Add(Material);
				}
			}
		}

		// update the new LODSections on the render thread proxy
		if (MaterialsToSwap.Num())
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
			UpdateSkelProxyLODSectionElementsCmd,
				TSet<UMaterialInterface*>,MaterialsToSwap,MaterialsToSwap,
				UMaterialInterface*,DefaultMaterial,UMaterial::GetDefaultMaterial(MD_Surface),
				ERHIFeatureLevel::Type, FeatureLevel, GetScene().GetFeatureLevel(),
			FSkeletalMeshSceneProxy*,SkelMeshSceneProxy,this,
			{
					for( int32 LodIdx=0; LodIdx < SkelMeshSceneProxy->LODSections.Num(); LodIdx++ )
					{
						FLODSectionElements& LODSection = SkelMeshSceneProxy->LODSections[LodIdx];
						for( int32 SectIdx=0; SectIdx < LODSection.SectionElements.Num(); SectIdx++ )
						{
							FSectionElementInfo& SectionElement = LODSection.SectionElements[SectIdx];
							if( MaterialsToSwap.Contains(SectionElement.Material) )
							{
								// fallback to default material if needed
								SectionElement.Material = DefaultMaterial;
							}
						}
					}
					SkelMeshSceneProxy->MaterialRelevance |= DefaultMaterial->GetRelevance(FeatureLevel);
			});
		}
	}
}

#if WITH_EDITORONLY_DATA

bool FSkeletalMeshSceneProxy::GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const
{

	if (FPrimitiveSceneProxy::GetPrimitiveDistance(LODIndex, SectionIndex, ViewOrigin, PrimitiveDistance))
	{
		const float OneOverDistanceMultiplier = 1.f / FMath::Max<float>(SMALL_NUMBER, StreamingDistanceMultiplier);
		PrimitiveDistance *= OneOverDistanceMultiplier;
		return true;
	}
	return false;
}

bool FSkeletalMeshSceneProxy::GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const
{
	if (LODSections.IsValidIndex(LODIndex) && LODSections[LODIndex].SectionElements.IsValidIndex(SectionIndex))
	{
		// The LOD-section data is stored per material index as it is only used for texture streaming currently.
		const int32 MaterialIndex = LODSections[LODIndex].SectionElements[SectionIndex].UseMaterialIndex;
		if (SkelMeshResource && SkelMeshResource->UVChannelDataPerMaterial.IsValidIndex(MaterialIndex))
		{
			const float TransformScale = GetLocalToWorld().GetMaximumAxisScale();
			const float* LocalUVDensities = SkelMeshResource->UVChannelDataPerMaterial[MaterialIndex].LocalUVDensities;

			WorldUVDensities.Set(
				LocalUVDensities[0] * TransformScale,
				LocalUVDensities[1] * TransformScale,
				LocalUVDensities[2] * TransformScale,
				LocalUVDensities[3] * TransformScale);
			
			return true;
	}
		}
	return FPrimitiveSceneProxy::GetMeshUVDensities(LODIndex, SectionIndex, WorldUVDensities);
	}

bool FSkeletalMeshSceneProxy::GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4* OneOverScales, FIntVector4* UVChannelIndices) const
	{
	if (LODSections.IsValidIndex(LODIndex) && LODSections[LODIndex].SectionElements.IsValidIndex(SectionIndex))
	{
		const UMaterialInterface* Material = LODSections[LODIndex].SectionElements[SectionIndex].Material;
		if (Material)
		{
			// This is thread safe because material texture data is only updated while the renderthread is idle.
			for (const FMaterialTextureInfo TextureData : Material->GetTextureStreamingData())
	{
				const int32 TextureIndex = TextureData.TextureIndex;
				if (TextureData.IsValid(true))
		{
					OneOverScales[TextureIndex / 4][TextureIndex % 4] = 1.f / TextureData.SamplingScale;
					UVChannelIndices[TextureIndex / 4][TextureIndex % 4] = TextureData.UVChannelIndex;
		}
	}
			return true;
	}
	}
	return false;
}
#endif

FSkeletalMeshComponentRecreateRenderStateContext::FSkeletalMeshComponentRecreateRenderStateContext(USkeletalMesh* InSkeletalMesh, bool InRefreshBounds /*= false*/)
	: bRefreshBounds(InRefreshBounds)
{
	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		if (It->SkeletalMesh == InSkeletalMesh)
		{
			checkf(!It->IsUnreachable(), TEXT("%s"), *It->GetFullName());

			if (It->IsRenderStateCreated())
			{
				check(It->IsRegistered());
				It->DestroyRenderState_Concurrent();
				SkeletalMeshComponents.Add(*It);
			}
		}
	}

	// Flush the rendering commands generated by the detachments.
	// The static mesh scene proxies reference the UStaticMesh, and this ensures that they are cleaned up before the UStaticMesh changes.
	FlushRenderingCommands();
}

FSkeletalMeshComponentRecreateRenderStateContext::~FSkeletalMeshComponentRecreateRenderStateContext()
{
	const int32 ComponentCount = SkeletalMeshComponents.Num();
	for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
	{
		USkeletalMeshComponent* Component = SkeletalMeshComponents[ComponentIndex];

		if (bRefreshBounds)
		{
			Component->UpdateBounds();
		}

		if (Component->IsRegistered() && !Component->IsRenderStateCreated())
		{
			Component->CreateRenderState_Concurrent();
		}
	}
}

#undef LOCTEXT_NAMESPACE
