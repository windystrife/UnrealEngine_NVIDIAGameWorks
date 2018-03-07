// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DestructibleFractureSettings.h"
#include "Materials/Material.h"
#include "PhysXPublic.h"

//////////////////////////////////////////////////////////////////////////

// Local utilities

#if WITH_EDITOR	//	Fracture code is only needed in editor

// Why doesn't TArray have this function?
template<typename ElementType>
inline void Resize(TArray<ElementType>& Array, const ElementType& Item, uint32 Size)
{
	const uint32 OldSize = Array.Num();
	if (Size < OldSize)
	{
		Array.RemoveAt(Size, OldSize-Size);
	}
	else
	if (Size > OldSize)
	{
		Array.Reserve(Size);
		for (uint32 Index = OldSize; Index < Size; ++Index)
		{
			Array.Add(Item);
		}
	}
}

#if WITH_APEX
static void BuildApexRenderMesh(apex::RenderMeshAssetAuthoring& RenderMeshAssetAuthor, apex::ExplicitHierarchicalMesh& HMesh, apex::RenderDataFormat::Enum VertexNormalFormat = apex::RenderDataFormat::FLOAT3)
{
	// Create a mesh building descriptor
	apex::RenderMeshAssetAuthoring::MeshDesc MeshDesc;
	TArray<apex::RenderMeshAssetAuthoring::SubmeshDesc> SubmeshDescs;
	SubmeshDescs.Init(apex::RenderMeshAssetAuthoring::SubmeshDesc(), HMesh.submeshCount());
	MeshDesc.m_numSubmeshes = (PxU32)SubmeshDescs.Num();
	MeshDesc.m_submeshes = SubmeshDescs.GetData();

	// Need to transpose the submesh/part arrays.  The outer arrays on the following are indexed by submesh:
	TArray< TArray<apex::Vertex> > SubmeshVertices;
	SubmeshVertices.Init(TArray<apex::Vertex>(), MeshDesc.m_numSubmeshes);
	TArray< TArray<PxU32> > SubmeshIndices;
	SubmeshIndices.Init(TArray<PxU32>(), MeshDesc.m_numSubmeshes);
	TArray< TArray<PxU32> > SubmeshPartIndices;
	SubmeshPartIndices.Init(TArray<PxU32>(), MeshDesc.m_numSubmeshes);
	TArray< TArray<PxU16> > SubmeshFlags;
	SubmeshFlags.Init(TArray<PxU16>(), MeshDesc.m_numSubmeshes);
	TArray< apex::RenderMeshAssetAuthoring::VertexBuffer > SubmeshVertexBuffers;
	SubmeshVertexBuffers.Init(apex::RenderMeshAssetAuthoring::VertexBuffer(), MeshDesc.m_numSubmeshes);

	for (uint32 SubmeshNum = 0; SubmeshNum < MeshDesc.m_numSubmeshes; ++SubmeshNum)
	{
		TArray<apex::Vertex>& Vertices = SubmeshVertices[SubmeshNum];
		TArray<PxU32>& Indices = SubmeshIndices[SubmeshNum];
		TArray<PxU16>& Flags = SubmeshFlags[SubmeshNum];
		TArray<PxU32>& PartIndices = SubmeshPartIndices[SubmeshNum];
		apex::RenderMeshAssetAuthoring::VertexBuffer& vb = SubmeshVertexBuffers[SubmeshNum];

		apex::RenderMeshAssetAuthoring::SubmeshDesc& SubmeshDesc = SubmeshDescs[SubmeshNum];

		SubmeshDesc.m_materialName = HMesh.submeshData(SubmeshNum)->mMaterialName;
		apex::ExplicitVertexFormat VertexFormat = HMesh.submeshData(SubmeshNum)->mVertexFormat;

		SubmeshDesc.m_numVertexBuffers = 1;
		SubmeshDesc.m_vertexBuffers = &vb;

		PartIndices.SetNumUninitialized(HMesh.partCount());
		for(uint32 PartIndex = 0; PartIndex < HMesh.partCount(); ++PartIndex)
		{
			TArray<apex::Vertex> PartVertices;
			const uint32 PartTriangleCount = HMesh.meshTriangleCount(PartIndex);
			apex::ExplicitRenderTriangle* PartTriangles = HMesh.meshTriangles(PartIndex);

			uint32 PartVertexCount = 0;
			for(uint32 PartTriangleIndex = 0; PartTriangleIndex < PartTriangleCount; ++PartTriangleIndex)
			{
				if (PartTriangles[PartTriangleIndex].submeshIndex == SubmeshNum)
				{
					PartVertexCount	+= 3;
				}
			}

			PartVertices.Init(apex::Vertex(), PartVertexCount);
			PartVertexCount = 0;
			for(uint32 PartTriangleIndex = 0; PartTriangleIndex < PartTriangleCount; ++PartTriangleIndex)
			{
				apex::ExplicitRenderTriangle& Triangle = PartTriangles[PartTriangleIndex];
				if (Triangle.submeshIndex == SubmeshNum)
				{
					PartVertices[PartVertexCount++]	= Triangle.vertices[0];
					PartVertices[PartVertexCount++]	= Triangle.vertices[1];
					PartVertices[PartVertexCount++]	= Triangle.vertices[2];
				}
			}

			PartIndices[PartIndex] = Indices.Num();

			if (PartVertices.Num() > 0)
			{
				TArray<PxU32> Map;
				Map.SetNumUninitialized(PartVertices.Num());
				const PxU32 ReducedPartVertexCount = RenderMeshAssetAuthor.createReductionMap(Map.GetData(), PartVertices.GetData(), NULL, (PxU32)PartVertices.Num(), PxVec3( 0.0001f ), 0.001f, 1.0f/256.01f);
				const PxU32 VertexPartStart = (PxU32)Vertices.Num();
				Resize(Vertices, apex::Vertex(), VertexPartStart + ReducedPartVertexCount);
				Resize(Indices, (uint32)0, PartIndices[PartIndex] + PartVertices.Num());
				Resize(Flags, (uint16)0, VertexPartStart + ReducedPartVertexCount);
				for(uint32 OldIndex = 0; OldIndex < (uint32)PartVertices.Num(); ++OldIndex)
				{
					const uint32 NewIndex = Map[OldIndex]+VertexPartStart;
					Indices[PartIndices[PartIndex]+OldIndex] = NewIndex;
					Vertices[NewIndex] = PartVertices[OldIndex];	// This will copy over several times, but with the same (or close enough) data
					Vertices[NewIndex].boneIndices[0] = PartIndex;
				}
			}
		}

		SubmeshDesc.m_numVertices = (PxU32)Vertices.Num();
		SubmeshDesc.m_numParts = (PxU32)PartIndices.Num();
		SubmeshDesc.m_partIndices = PartIndices.GetData();
		SubmeshDesc.m_numIndices = (PxU32)Indices.Num();
		SubmeshDesc.m_vertexIndices = Indices.GetData();

		if (SubmeshDesc.m_numParts == 0 || SubmeshDesc.m_vertexIndices == 0)
		{
			continue;
		}

		if (VertexFormat.mHasStaticPositions || VertexFormat.mHasDynamicPositions)
		{
			vb.setSemanticData( apex::RenderVertexSemantic::POSITION, &Vertices.GetData()->position, sizeof( apex::Vertex ), apex::RenderDataFormat::FLOAT3);
		}
		if (VertexFormat.mHasStaticNormals || VertexFormat.mHasDynamicNormals)
		{
			vb.setSemanticData( apex::RenderVertexSemantic::NORMAL, &Vertices.GetData()->normal, sizeof( apex::Vertex ), VertexNormalFormat, apex::RenderDataFormat::FLOAT3);
		}
		if (VertexFormat.mHasStaticTangents || VertexFormat.mHasDynamicTangents)
		{
			vb.setSemanticData( apex::RenderVertexSemantic::TANGENT, &Vertices.GetData()->tangent, sizeof( apex::Vertex ), VertexNormalFormat, apex::RenderDataFormat::FLOAT3);
		}
		if (VertexFormat.mHasStaticBinormals || VertexFormat.mHasDynamicBinormals)
		{
			vb.setSemanticData( apex::RenderVertexSemantic::BINORMAL, &Vertices.GetData()->binormal, sizeof( apex::Vertex ), VertexNormalFormat, apex::RenderDataFormat::FLOAT3);
		}
		if (VertexFormat.mHasStaticColors || VertexFormat.mHasDynamicColors)
		{
			vb.setSemanticData( apex::RenderVertexSemantic::COLOR, &Vertices.GetData()->color, sizeof( apex::Vertex ), apex::RenderDataFormat::R8G8B8A8, apex::RenderDataFormat::R32G32B32A32_FLOAT);
		}
		for (PxU32 uvNum = 0; uvNum < VertexFormat.mUVCount; ++uvNum)
		{
			vb.setSemanticData( (apex::RenderVertexSemantic::Enum)(uvNum+apex::RenderVertexSemantic::TEXCOORD0), &Vertices.GetData()->uv[uvNum], sizeof( apex::Vertex ), apex::RenderDataFormat::FLOAT2);
		}
		switch (VertexFormat.mBonesPerVertex)
		{
		case 1:
			vb.setSemanticData( apex::RenderVertexSemantic::BONE_INDEX, &Vertices.GetData()->boneIndices[0], sizeof( apex::Vertex ), apex::RenderDataFormat::USHORT1);
			break;
		case 2:
			vb.setSemanticData( apex::RenderVertexSemantic::BONE_INDEX, &Vertices.GetData()->boneIndices[0], sizeof( apex::Vertex ), apex::RenderDataFormat::USHORT2);
			vb.setSemanticData( apex::RenderVertexSemantic::BONE_WEIGHT, &Vertices.GetData()->boneWeights[0], sizeof( apex::Vertex ), apex::RenderDataFormat::FLOAT2);
			break;
		case 3:
			vb.setSemanticData( apex::RenderVertexSemantic::BONE_INDEX, &Vertices.GetData()->boneIndices[0], sizeof( apex::Vertex ), apex::RenderDataFormat::USHORT3);
			vb.setSemanticData( apex::RenderVertexSemantic::BONE_WEIGHT, &Vertices.GetData()->boneWeights[0], sizeof( apex::Vertex ), apex::RenderDataFormat::FLOAT3);
			break;
		case 4:
			vb.setSemanticData( apex::RenderVertexSemantic::BONE_INDEX, &Vertices.GetData()->boneIndices[0], sizeof( apex::Vertex ), apex::RenderDataFormat::USHORT4);
			vb.setSemanticData( apex::RenderVertexSemantic::BONE_WEIGHT, &Vertices.GetData()->boneWeights[0], sizeof( apex::Vertex ), apex::RenderDataFormat::FLOAT4);
			break;
		}
	}

	// BRG - this is being done until I add the ability to reset the submesh data without resetting the mesh data in the fracture tools
	while (MeshDesc.m_numSubmeshes > 0)
	{
		if (MeshDesc.m_submeshes[MeshDesc.m_numSubmeshes-1].m_vertexIndices != 0)
		{
			break;
		}
		--MeshDesc.m_numSubmeshes;
	}

	RenderMeshAssetAuthor.createRenderMesh(MeshDesc, false);
}

class FProgressListener : public IProgressListener
{
public:
	// Begin IProgressListener interface
	virtual void	setProgress(int progress, const char* taskName = NULL) override {}
	// End IProgressListener interface
};

class FExplicitHierarchicalMeshEmbedding : public apex::ExplicitHierarchicalMesh::Embedding
{
public:
	virtual void	serialize(physx::general_PxIOStream2::PxFileBuf& stream, apex::ExplicitHierarchicalMesh::Embedding::DataType type) const override {}
	virtual void	deserialize(physx::general_PxIOStream2::PxFileBuf& stream, apex::ExplicitHierarchicalMesh::Embedding::DataType type, physx::PxU32 version) override {}
};

#endif // WITH_APEX
#endif // #if WITH_EDITOR	//	Fracture code is only needed in editor

//////////////////////////////////////////////////////////////////////////
// FFractureMaterial
//////////////////////////////////////////////////////////////////////////

#if WITH_APEX
void FFractureMaterial::FillNxFractureMaterialDesc(apex::FractureMaterialDesc& PFractureMaterialDesc)
{
#if WITH_EDITOR	//	Fracture code is only needed in editor
	PFractureMaterialDesc.uvScale = PxVec2(UVScale.X, UVScale.Y);
	PFractureMaterialDesc.uvOffset = PxVec2(UVOffset.X, UVOffset.Y);
	PFractureMaterialDesc.tangent = U2PVector(Tangent);
	PFractureMaterialDesc.uAngle = UAngle;
	PFractureMaterialDesc.interiorSubmeshIndex = InteriorElementIndex >= 0 ? (PxU32)InteriorElementIndex : 0xFFFFFFFF;	// We'll use this value to indicate we should create a new element
#endif // WITH_EDITOR
}
#endif // WITH_APEX

//////////////////////////////////////////////////////////////////////////
// UDestructibleFractureSettings
//////////////////////////////////////////////////////////////////////////

UDestructibleFractureSettings::UDestructibleFractureSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CellSiteCount = 25;
	OriginalSubmeshCount = 0;
}

UDestructibleFractureSettings::UDestructibleFractureSettings(FVTableHelper& Helper)
	: Super(Helper)
{

}

void UDestructibleFractureSettings::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR	//	Fracture code is only needed in editor

	if (Ar.IsLoading())
	{
		// Buffer for the NxDestructibleAsset(Authoring)
		TArray<uint8> Buffer;
		uint32 Size;
		Ar << Size;
		if (Size > 0)
		{
			// Size is non-zero, so a binary blob follows
			Buffer.AddUninitialized(Size);
			Ar.Serialize(Buffer.GetData(), Size);
#if WITH_APEX
			check(ApexDestructibleAssetAuthoring != NULL);
			// Wrap this blob with the APEX read stream class
			physx::PxFileBuf* Stream = GApexSDK->createMemoryReadStream(Buffer.GetData(), Size);
			if (Stream != NULL)
			{
				// Now deserialize the ExplicitHierarchicalMeshes
				FExplicitHierarchicalMeshEmbedding ExplicitHierarchicalMeshEmbedding;
				ApexDestructibleAssetAuthoring->getExplicitHierarchicalMesh().deserialize(*Stream, ExplicitHierarchicalMeshEmbedding);
				ApexDestructibleAssetAuthoring->getCoreExplicitHierarchicalMesh().deserialize(*Stream, ExplicitHierarchicalMeshEmbedding);
				// Release the stream
				GApexSDK->releaseMemoryReadStream(*Stream);
			}
#endif
		}
	}
	else if (Ar.IsSaving())
	{
		uint32 size=0;
		Ar << size;
	}

#endif // #if WITH_EDITOR	//	Fracture code is only needed in editor
}

void UDestructibleFractureSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR	//	Fracture code is only needed in editor
#if WITH_APEX
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		ApexDestructibleAssetAuthoring = static_cast<apex::DestructibleAssetAuthoring*>(apex::GetApexSDK()->createAssetAuthoring(DESTRUCTIBLE_AUTHORING_TYPE_NAME));
	}
#endif // WITH_APEX
#endif // #if WITH_EDITOR	//	Fracture code is only needed in editor
}

void UDestructibleFractureSettings::BeginDestroy()
{
#if WITH_EDITOR	//	Fracture code is only needed in editor
#if WITH_APEX
	if (ApexDestructibleAssetAuthoring != NULL)
	{
		GPhysCommandHandler->DeferredRelease(ApexDestructibleAssetAuthoring);
		ApexDestructibleAssetAuthoring = NULL;
	}
#endif // WITH_APEX
#endif // #if WITH_EDITOR	//	Fracture code is only needed in editor

	Super::BeginDestroy();
}

#if WITH_APEX && WITH_EDITOR
void UDestructibleFractureSettings::BuildDestructibleAssetCookingDesc(apex::DestructibleAssetCookingDesc& DestructibleAssetCookingDesc)
{
	//	Fracture code is only needed in editor
	// Retrieve the authoring mesh
	apex::ExplicitHierarchicalMesh& HMesh = ApexDestructibleAssetAuthoring->getExplicitHierarchicalMesh();

	if (ChunkParameters.Num() < (int32)HMesh.chunkCount())
	{
		const int32 OldParametersCount = ChunkParameters.Num();
		ChunkParameters.InsertZeroed(ChunkParameters.Num(), HMesh.chunkCount()-ChunkParameters.Num());
		for (int32 ChunkIndex = OldParametersCount; ChunkIndex < ChunkParameters.Num(); ++ChunkIndex)
		{
			ChunkParameters[ChunkIndex] = FDestructibleChunkParameters();
		}
	}

	// Set up chunk desc array
	ChunkDescs.Init(apex::DestructibleChunkDesc(), HMesh.chunkCount());
	DestructibleAssetCookingDesc.chunkDescs = ChunkDescs.GetData();
	DestructibleAssetCookingDesc.chunkDescCount = ChunkDescs.Num();
	for (uint32 ChunkIndex = 0; ChunkIndex < DestructibleAssetCookingDesc.chunkDescCount; ++ChunkIndex)
	{
		FDestructibleChunkParameters& IndexedChunkParameters = ChunkParameters[ChunkIndex];
		DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].setToDefault();
		DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].parentIndex = *HMesh.parentIndex(ChunkIndex);
		DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].meshIndex = *HMesh.partIndex(ChunkIndex);
		DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].isSupportChunk = IndexedChunkParameters.bIsSupportChunk;
		DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].doNotFracture = IndexedChunkParameters.bDoNotFracture;
		DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].doNotDamage = IndexedChunkParameters.bDoNotDamage;
		DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].doNotCrumble = IndexedChunkParameters.bDoNotCrumble;
		const bool instancing = (*HMesh.chunkFlags(ChunkIndex) & apex::DestructibleAsset::ChunkIsInstanced) != 0;

		DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].useInstancedRendering = instancing;
		if (instancing)
		{
			DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].instancePositionOffset = *HMesh.instancedPositionOffset(ChunkIndex);
			DestructibleAssetCookingDesc.chunkDescs[ChunkIndex].instanceUVOffset = *HMesh.instancedUVOffset(ChunkIndex);
		}
	}

	// Set up geometry desc array
	GeometryDescs.Init(apex::DestructibleGeometryDesc(), HMesh.partCount());
	DestructibleAssetCookingDesc.geometryDescs = GeometryDescs.GetData();
	DestructibleAssetCookingDesc.geometryDescCount = GeometryDescs.Num();
	for (uint32 GeometryIndex = 0; GeometryIndex < DestructibleAssetCookingDesc.geometryDescCount; ++GeometryIndex)
	{
		DestructibleAssetCookingDesc.geometryDescs[GeometryIndex].setToDefault();
		DestructibleAssetCookingDesc.geometryDescs[GeometryIndex].convexHullCount = HMesh.convexHullCount(GeometryIndex);
		DestructibleAssetCookingDesc.geometryDescs[GeometryIndex].convexHulls = HMesh.convexHulls(GeometryIndex);
	}
}

bool UDestructibleFractureSettings::SetRootMesh(const TArray<apex::ExplicitRenderTriangle>& MeshTriangles, const TArray<UMaterialInterface*>& InMaterials, const TArray<apex::ExplicitSubmeshData>& SubmeshData,
												const TArray<uint32>& MeshPartition, bool bFirstPartitionIsDepthZero)
{
	bool Success = false;
	physx::PxI32 NegativeOne = -1;

	if (ApexDestructibleAssetAuthoring != NULL)
	{
		Success = ApexDestructibleAssetAuthoring->setRootMesh(MeshTriangles.GetData(), MeshTriangles.Num(), SubmeshData.GetData(), 
															  SubmeshData.Num(), (uint32*)MeshPartition.GetData(), MeshPartition.Num(), &NegativeOne, bFirstPartitionIsDepthZero ? 1 : 0);
		if (Success)
		{
			apex::CollisionDesc CollisionDesc;
			ApexDestructibleAssetAuthoring->getExplicitHierarchicalMesh().buildCollisionGeometryForRootChunkParts(CollisionDesc);
		}

		// Resize the chunk parameters
		apex::ExplicitHierarchicalMesh& HMesh = ApexDestructibleAssetAuthoring->getExplicitHierarchicalMesh();
		ChunkParameters.Init(FDestructibleChunkParameters(), HMesh.chunkCount());
	}

	OriginalSubmeshCount = SubmeshData.Num();

	Materials.Init(NULL, SubmeshData.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
	{
		UMaterialInterface* Material = NULL;
		if (MaterialIndex < InMaterials.Num())
		{
			Material = InMaterials[MaterialIndex];
		}
		if (Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		Materials[MaterialIndex] = Material;
	}

	return Success;
}

bool UDestructibleFractureSettings::BuildRootMeshFromApexDestructibleAsset(apex::DestructibleAsset& ApexDestructibleAsset, EDestructibleImportOptions::Type Options)
{
	bool Success = false;

	if (ApexDestructibleAssetAuthoring != NULL)
	{
		Success = ApexDestructibleAssetAuthoring->importDestructibleAssetToRootMesh(ApexDestructibleAsset, 0);

		apex::ExplicitHierarchicalMesh& EHM = ApexDestructibleAssetAuthoring->getExplicitHierarchicalMesh();

		if (!(Options & EDestructibleImportOptions::PreserveSettings))
		{
			// Now apply the y -> -y and v -> 1-v transformation to all vertex data
			for (PxU32 PartIndex = 0; PartIndex < EHM.partCount(); ++PartIndex)
			{
				apex::ExplicitRenderTriangle* Triangles = EHM.meshTriangles(PartIndex);
				for (PxU32 TriangleIndex = 0; TriangleIndex < EHM.meshTriangleCount(PartIndex); ++TriangleIndex)
				{
					apex::ExplicitRenderTriangle& Triangle = Triangles[TriangleIndex];
					for (PxU32 VertexNum = 0; VertexNum < 3; ++VertexNum)
					{
						apex::Vertex& Vertex = Triangle.vertices[VertexNum];
						Vertex.position.y *= -1.0f;
						Vertex.normal.y *= -1.0f;
						Vertex.tangent.y *= -1.0f;
						Vertex.binormal.y *= -1.0f;
						
						for (PxU32 UVNum = 0; UVNum < apex::VertexFormat::MAX_UV_COUNT; ++UVNum)
						{
							Vertex.uv[UVNum].v = 1.0f - Vertex.uv[UVNum].v;
						}
					}
					Swap(Triangle.vertices[0], Triangle.vertices[2]);
				}
			}

			// BRG - Until APEX 1.2.3 or later is in place, chunk flags will not be imported automaticlally, so we need to do this here:
			ChunkParameters.Init(FDestructibleChunkParameters(), ApexDestructibleAsset.getChunkCount());
			enum ChunkFlags	// Copied from DestructibleAsset.h:
			{
				SupportChunk =	(1 << 0),
				UnfractureableChunk =	(1 << 1),
				DescendantUnfractureable =	(1 << 2),
				UndamageableChunk =	(1 << 3),
				UncrumbleableChunk =	(1 << 4),

				Instanced = (1 << 8),
			};
			const NvParameterized::Interface* Params = ApexDestructibleAsset.getAssetNvParameterized();
			if (Params != NULL)
			{
				// Damage parameters
				for (PxU32 ChunkIndex = 0; ChunkIndex < EHM.chunkCount(); ++ChunkIndex)
				{
					char ChunkFlagsName[MAX_SPRINTF];
					FCStringAnsi::Sprintf(ChunkFlagsName, "chunks[%d].flags", ChunkIndex);
					PxU16 ChunkFlags = 0;
					verify( NvParameterized::getParamU16(*Params, ChunkFlagsName, ChunkFlags) );
					FDestructibleChunkParameters& IndexedChunkParameters = ChunkParameters[ChunkIndex];
					IndexedChunkParameters.bIsSupportChunk = (ChunkFlags & SupportChunk) != 0;
					IndexedChunkParameters.bDoNotFracture = (ChunkFlags & UnfractureableChunk) != 0;
					IndexedChunkParameters.bDoNotDamage = (ChunkFlags & UndamageableChunk) != 0;
					IndexedChunkParameters.bDoNotCrumble = (ChunkFlags & UncrumbleableChunk) != 0;
				}
			}
		}
	}

	return Success;
}

void UDestructibleFractureSettings::CreateVoronoiSitesInRootMesh()
{
	if (ApexDestructibleAssetAuthoring != NULL)
	{
		VoronoiSites.Init(FVector(0.0f), CellSiteCount);
		// Progress listener for reporting progress - for now, just a dummy
		FProgressListener ProgressListener;
		check(sizeof(FVector) == sizeof(PxVec3));
		ApexDestructibleAssetAuthoring->createVoronoiSitesInsideMesh((PxVec3*)VoronoiSites.GetData(), nullptr, VoronoiSites.Num(), (PxU32*)&RandomSeed, NULL, apex::BSPOpenMode::Automatic, ProgressListener);
	}
}

bool UDestructibleFractureSettings::VoronoiSplitMesh()
{
	bool Success = false;

	if (ApexDestructibleAssetAuthoring != NULL)
	{
		// Fill MeshProcessingParameters
		FractureTools::MeshProcessingParameters FTMeshProcessingParameters;
		FTMeshProcessingParameters.islandGeneration = false;	// expose

		// Fill Voronoi splitting descriptor
		FractureTools::FractureVoronoiDesc FTFractureVoronoiDesc;
		FTFractureVoronoiDesc.siteCount = VoronoiSites.Num();
		check(sizeof(FVector) == sizeof(PxVec3));
		FTFractureVoronoiDesc.sites = (PxVec3*)VoronoiSites.GetData();

		// Material descriptor
		FractureMaterialDesc.FillNxFractureMaterialDesc(FTFractureVoronoiDesc.materialDesc);
		// Retrieve the authoring mesh to see if the interiorSubmeshIndex is valid
		apex::ExplicitHierarchicalMesh& HMesh = ApexDestructibleAssetAuthoring->getExplicitHierarchicalMesh();
		if (FTFractureVoronoiDesc.materialDesc.interiorSubmeshIndex >= HMesh.submeshCount())
		{
			// For now just copy the submesh data from the 0 submesh.
			apex::ExplicitSubmeshData SubmeshData;
			apex::ExplicitSubmeshData* SourceSubmeshData = HMesh.submeshData(0);
			if (SourceSubmeshData != NULL)
			{
				SubmeshData = *SourceSubmeshData;
			}

			// BRG - this is being done until I add the ability to reset the submesh data without resetting the mesh data in the fracture tools
			if (HMesh.submeshCount() > (uint32)OriginalSubmeshCount)
			{
				FTFractureVoronoiDesc.materialDesc.interiorSubmeshIndex = (uint32)OriginalSubmeshCount;
			}
			else
			{
				FTFractureVoronoiDesc.materialDesc.interiorSubmeshIndex = HMesh.addSubmesh(SubmeshData);
			}

			// Parallel storage of UE materials
			while (Materials.Num() <= OriginalSubmeshCount)
			{
				Materials.Add(UMaterial::GetDefaultMaterial(MD_Surface));
			}
		}

		// Collision volume descriptor
		apex::CollisionDesc CollisionVolumeDesc;

		// Progress listener for reporting progress - for now, just a dummy
		FProgressListener ProgressListener;
		Success = ApexDestructibleAssetAuthoring->createVoronoiSplitMesh(FTMeshProcessingParameters, FTFractureVoronoiDesc, CollisionVolumeDesc, false, 0, RandomSeed, ProgressListener);
	}

	return Success;
}

apex::DestructibleAsset* UDestructibleFractureSettings::CreateApexDestructibleAsset(const apex::DestructibleAssetCookingDesc& DestructibleAssetCookingDesc)
{
	apex::DestructibleAsset* ApexDestructibleAsset = NULL;

	if (ApexDestructibleAssetAuthoring != NULL && DestructibleAssetCookingDesc.isValid())
	{
		apex::RenderMeshAssetAuthoring* ApexRenderMeshAssetAuthoring = static_cast<apex::RenderMeshAssetAuthoring*>(apex::GetApexSDK()->createAssetAuthoring(RENDER_MESH_AUTHORING_TYPE_NAME));
		if (ApexRenderMeshAssetAuthoring != NULL)
		{
			BuildApexRenderMesh(*ApexRenderMeshAssetAuthoring, ApexDestructibleAssetAuthoring->getExplicitHierarchicalMesh());
			apex::RenderMeshAsset* ApexRenderMeshAsset = static_cast<apex::RenderMeshAsset*>(apex::GetApexSDK()->createAsset(ApexRenderMeshAssetAuthoring->releaseAndReturnNvParameterizedInterface(), NULL ));
			if (ApexRenderMeshAsset != NULL)
			{
				ApexDestructibleAssetAuthoring->setRenderMeshAsset(ApexRenderMeshAsset);
				ApexDestructibleAssetAuthoring->cookChunks(DestructibleAssetCookingDesc);
				ApexDestructibleAsset = static_cast<apex::DestructibleAsset*>(apex::GetApexSDK()->createAsset(*ApexDestructibleAssetAuthoring, NULL));
			}
		}
	}

	return ApexDestructibleAsset;
}

#endif // WITH_APEX && WITH_EDITOR
