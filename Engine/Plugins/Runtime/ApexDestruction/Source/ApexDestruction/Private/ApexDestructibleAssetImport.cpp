// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApexDestructibleAssetImport.cpp:

	Creation of an APEX NxDestructibleAsset from a binary buffer.
	SkeletalMesh creation from an APEX NxDestructibleAsset.

	SkeletalMesh creation largely based on FbxSkeletalMeshImport.cpp

=============================================================================*/

#include "ApexDestructibleAssetImport.h"


#if WITH_EDITOR

#include "Modules/ModuleManager.h"
#include "SkeletalMeshTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Factories/Factory.h"
#include "EditorFramework/AssetImportData.h"
#include "DestructibleComponent.h"
#include "ApexDestructionModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexDestructibleAssetImport, Log, All);

#include "PhysicsPublic.h"
#include "SkelImport.h"
#include "PhysXIncludes.h"
#include "MeshUtilities.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "DestructibleMesh.h"
#include "Factories/FbxSkeletalMeshImportData.h"

#if WITH_APEX


// Set to 1 to match UE transformation of FBX-imported meshes
#define INVERT_Y_AND_V	1

// Set to 1 to use temporary transform function
#define USE_TEMPORARY_TRANSFORMATION_FUNCTION 1


// Forward declarations and external processing functions
struct ExistingSkelMeshData;


// Temporary transform function, to be removed once the APEX SDK is updated
#if USE_TEMPORARY_TRANSFORMATION_FUNCTION

static void ApplyTransformationToApexDestructibleAsset( apex::DestructibleAsset& ApexDestructibleAsset, const physx::PxMat44& Transform )
{
	// Get the NvParameterized interface to the asset
	NvParameterized::Interface* AssetParams = const_cast<NvParameterized::Interface*>(ApexDestructibleAsset.getAssetNvParameterized());
	if (AssetParams != NULL)
	{
		// Name buffer used for parameterized array element lookup
		char ArrayElementName[1024];

		/* surfaceTrace default normal */
		physx::PxI32 SurfaceTraceSetCount;
		if (NvParameterized::getParamArraySize(*AssetParams, "surfaceTraceSets", SurfaceTraceSetCount))
		{
			for (physx::PxI32 i = 0; i < SurfaceTraceSetCount; ++i)
			{
				FCStringAnsi::Sprintf( ArrayElementName, "surfaceTraceSets[%d]", i );
				NvParameterized::Interface* SurfaceTraceSetParams;
				if (NvParameterized::getParamRef(*AssetParams, ArrayElementName, SurfaceTraceSetParams))
				{
					if (SurfaceTraceSetParams != NULL)
					{
						physx::PxI32 SurfaceTraceCount;
						if (NvParameterized::getParamArraySize(*SurfaceTraceSetParams, "traces", SurfaceTraceCount))
						{
							for (physx::PxI32 j = 0; j < SurfaceTraceCount; ++j)
							{
								FCStringAnsi::Sprintf( ArrayElementName, "traces[%d].defaultNormal", j );
								NvParameterized::Handle DefaultNormalHandle(*SurfaceTraceSetParams);
								if (NvParameterized::findParam(*SurfaceTraceSetParams, ArrayElementName, DefaultNormalHandle) != NULL)
								{
									physx::PxVec3 DefaultNormal;
									DefaultNormalHandle.getParamVec3( DefaultNormal );
									DefaultNormal = Transform.rotate( DefaultNormal );
									DefaultNormalHandle.setParamVec3( DefaultNormal );
								}
							}
						}
					}
				}
			}
		}

		/* For now, we'll just clear the current cached streams. */
		NvParameterized::Interface* CollisionDataParams;
		if (NvParameterized::getParamRef(*AssetParams, "collisionData", CollisionDataParams))
		{
			if (CollisionDataParams != NULL)
			{
				CollisionDataParams->destroy();
				NvParameterized::setParamRef(*AssetParams, "collisionData", NULL);
			}
		}

		/* chunk surface normal */
		physx::PxI32 AssetChunkCount;
		if (NvParameterized::getParamArraySize(*AssetParams, "chunks", AssetChunkCount))
		{
			for (physx::PxI32 i = 0; i < AssetChunkCount; ++i)
			{
				FCStringAnsi::Sprintf( ArrayElementName, "chunks[%d].surfaceNormal", i );
				NvParameterized::Handle ChunkSurfaceNormalHandle(*AssetParams);
				if (NvParameterized::findParam(*AssetParams, ArrayElementName, ChunkSurfaceNormalHandle) != NULL)
				{
					physx::PxVec3 ChunkSurfaceNormal;
					ChunkSurfaceNormalHandle.getParamVec3( ChunkSurfaceNormal );
					ChunkSurfaceNormal = Transform.rotate( ChunkSurfaceNormal );
					ChunkSurfaceNormalHandle.setParamVec3( ChunkSurfaceNormal );
				}
			}
		}

		/* bounds */
		physx::PxBounds3 Bounds;
		if (NvParameterized::getParamBounds3(*AssetParams, "bounds", Bounds))
		{
			if (!Bounds.isEmpty())
			{
				Bounds = physx::PxBounds3::basisExtent(Transform.transform(Bounds.getCenter()), physx::PxMat33(Transform.column0.getXYZ(), Transform.column1.getXYZ(), Transform.column2.getXYZ()), Bounds.getExtents());
				NvParameterized::setParamBounds3(*AssetParams, "bounds", Bounds);
			}
		}

		/* chunk convex hulls */
		physx::PxI32 ConvexHullCount;
		if (NvParameterized::getParamArraySize(*AssetParams, "chunkConvexHulls", ConvexHullCount))
		{
			for (physx::PxI32 i = 0; i < ConvexHullCount; ++i)
			{
				FCStringAnsi::Sprintf( ArrayElementName, "chunkConvexHulls[%d]", i );
				NvParameterized::Interface* ConvexHullParams;
				if (NvParameterized::getParamRef(*AssetParams, ArrayElementName, ConvexHullParams))
				{
					if (ConvexHullParams != NULL)
					{
						// planes
						physx::PxI32 UniquePlaneCount;
						if (NvParameterized::getParamArraySize(*ConvexHullParams, "uniquePlanes", UniquePlaneCount))
						{
							for (physx::PxI32 j = 0; j < UniquePlaneCount; ++j)
							{
								FCStringAnsi::Sprintf( ArrayElementName, "uniquePlanes[%d].normal", j );
								NvParameterized::Handle PlaneNormalHandle(*ConvexHullParams);
								if (NvParameterized::findParam(*ConvexHullParams, ArrayElementName, PlaneNormalHandle) != NULL)
								{
									physx::PxVec3 PlaneNormal;
									PlaneNormalHandle.getParamVec3( PlaneNormal );
									PlaneNormal = Transform.rotate( PlaneNormal );
									PlaneNormalHandle.setParamVec3( PlaneNormal );
								}
							}
						}

						// vertices
						physx::PxBounds3 HullBounds;
						HullBounds.setEmpty();
						physx::PxI32 HullVertexCount;
						if (NvParameterized::getParamArraySize(*ConvexHullParams, "vertices", HullVertexCount))
						{
							for (physx::PxI32 j = 0; j < HullVertexCount; ++j)
							{
								FCStringAnsi::Sprintf( ArrayElementName, "vertices[%d]", j );
								NvParameterized::Handle HullVertexHandle(*ConvexHullParams);
								if (NvParameterized::findParam(*ConvexHullParams, ArrayElementName, HullVertexHandle) != NULL)
								{
									physx::PxVec3 HullVertex;
									HullVertexHandle.getParamVec3( HullVertex );
									HullVertex = Transform.transform( HullVertex );
									HullVertexHandle.setParamVec3( HullVertex );
									HullBounds.include( HullVertex );
								}
							}
						}
						NvParameterized::setParamBounds3(*ConvexHullParams, "bounds", HullBounds);
					}
				}
			}
		}

		/* render mesh asset (bounding boxes only) */
		const physx::PxMat33 Basis(Transform.getBasis(0), Transform.getBasis(1), Transform.getBasis(2));
		const physx::PxVec3 Offset = Transform.getPosition();
		NvParameterized::Interface* RenderMeshAssetParams;
		if (NvParameterized::getParamRef(*AssetParams, "renderMeshAsset", RenderMeshAssetParams))
		{
			physx::PxI32 PartBoundsCount;
			if (NvParameterized::getParamArraySize(*RenderMeshAssetParams, "partBounds", PartBoundsCount))
			{
				for (physx::PxI32 i = 0; i < PartBoundsCount; ++i)
				{
					FCStringAnsi::Sprintf( ArrayElementName, "partBounds[%d]", i );
					NvParameterized::Handle PartBoundsHandle(*RenderMeshAssetParams);
					if (NvParameterized::findParam(*RenderMeshAssetParams, ArrayElementName, PartBoundsHandle) != NULL)
					{
						physx::PxBounds3 PartBounds;
						PartBoundsHandle.getParamBounds3( PartBounds );
						PartBounds = physx::PxBounds3::transformSafe(Basis, PartBounds);
						PartBounds.minimum += Offset;
						PartBounds.maximum += Offset;
						PartBoundsHandle.setParamBounds3( PartBounds );
					}
				}
			}
		}
	}
}
#endif // #if USE_TEMPORARY_TRANSFORMATION_FUNCTION

// Storage for destructible mesh settings (including base skeletal mesh)
struct ExistingDestMeshData
{
	ExistingDestMeshData() : SkelMeshData(NULL), BodySetup(NULL) {}

	ExistingSkelMeshData*			SkelMeshData;
	UBodySetup*						BodySetup;
	TArray<struct FFractureEffect>	FractureEffects;
};

ExistingDestMeshData* SaveExistingDestMeshData(UDestructibleMesh* ExistingDestructibleMesh)
{
	ExistingDestMeshData* ExistingDestMeshDataPtr = NULL;

	if (ExistingDestructibleMesh)
	{
		ExistingDestMeshDataPtr = new ExistingDestMeshData();

		// Only save off SkelMeshData if it's been created
		ExistingDestMeshDataPtr->SkelMeshData = NULL;
		
		ExistingDestMeshDataPtr->SkelMeshData = SaveExistingSkelMeshData(ExistingDestructibleMesh, true, INDEX_NONE);
		
		ExistingDestMeshDataPtr->BodySetup = ExistingDestructibleMesh->BodySetup;
		ExistingDestMeshDataPtr->FractureEffects = ExistingDestructibleMesh->FractureEffects;
	}

	return ExistingDestMeshDataPtr;
}

static void RestoreExistingDestMeshData(ExistingDestMeshData* MeshData, UDestructibleMesh* DestructibleMesh)
{
	if (MeshData && DestructibleMesh)
	{
		// Restore old settings, but resize arrays to make sense with the new NxDestructibleAsset
		if (MeshData->SkelMeshData != NULL)
		{
			RestoreExistingSkelMeshData(MeshData->SkelMeshData, DestructibleMesh, INDEX_NONE, false, false);
		}
		DestructibleMesh->BodySetup =  MeshData->BodySetup;
		DestructibleMesh->FractureEffects = MeshData->FractureEffects;

		// Resize the depth parameters array to the appropriate size
		const int32 ParamDepthDifference = (int32)DestructibleMesh->ApexDestructibleAsset->getDepthCount() - DestructibleMesh->DefaultDestructibleParameters.DepthParameters.Num();
		if (ParamDepthDifference > 0)
		{
			for (int32 i = 0; i < ParamDepthDifference; ++i)
			{
				DestructibleMesh->DefaultDestructibleParameters.DepthParameters.Add(FDestructibleDepthParameters());
			}
		}
		else
		if (ParamDepthDifference < 0)
		{
			DestructibleMesh->DefaultDestructibleParameters.DepthParameters.RemoveAt(DestructibleMesh->DefaultDestructibleParameters.DepthParameters.Num()+ParamDepthDifference, -ParamDepthDifference);
		}

		// Resize the fracture effects array to the appropriate size
		const int32 EffectsDepthDifference = (int32)DestructibleMesh->ApexDestructibleAsset->getDepthCount() - DestructibleMesh->FractureEffects.Num();
		if (EffectsDepthDifference > 0)
		{
			DestructibleMesh->FractureEffects.AddZeroed(EffectsDepthDifference);
		}
		else
		if (EffectsDepthDifference < 0)
		{
			DestructibleMesh->FractureEffects.RemoveAt(DestructibleMesh->FractureEffects.Num()+EffectsDepthDifference, -EffectsDepthDifference);
		}
	}
}

/**
 * Extract the material names from the Apex Render Mesh contained within an Apex Destructible Asset.
 * @param ImportData - SkeletalMesh import data into which we are extracting information
 * @param ApexDestructibleAsset - the Apex Destructible Asset
 */
static void ImportMaterialsForSkelMesh(FSkeletalMeshImportData &ImportData, const apex::DestructibleAsset& ApexDestructibleAsset)
{
	physx::PxU32 SubmeshCount = 0;

	// Get the submesh count from the Destructible Asset's Render Mesh
	const apex::RenderMeshAsset* ApexRenderMesh = ApexDestructibleAsset.getRenderMeshAsset();
	if (ApexRenderMesh != NULL)
	{
		SubmeshCount = ApexRenderMesh->getSubmeshCount();
	}

	if( SubmeshCount == 0 )
	{
		// No material info, create a default material slot
		++SubmeshCount;
		UE_LOG(LogApexDestructibleAssetImport, Warning,TEXT("No material associated with skeletal mesh - using default"));
	}
	else
	{
		UE_LOG(LogApexDestructibleAssetImport, Warning,TEXT("Using default materials for material slot"));
	}

	// Create material slots
	UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	if (DefaultMaterial)
	{
		for (uint32 MatIndex = 0; MatIndex < SubmeshCount; MatIndex++)
		{
			ImportData.Materials.Add( VMaterial() );

			ImportData.Materials.Last().Material = DefaultMaterial;
			ImportData.Materials.Last().MaterialImportName = DefaultMaterial->GetName();
		}
	}
}

/**
 * Create the bones needed to hold the transforms for the destructible chunks associated with an Apex Destructible Asset.
 * @param ImportData - SkeletalMesh import data into which we are extracting information
 * @param ApexDestructibleAsset - the Apex Destructible Asset
 */
static void CreateBones(FSkeletalMeshImportData &ImportData, const apex::DestructibleAsset& ApexDestructibleAsset)
{
	// Just need to create ApexDestructibleAsset.getChunkCount() bones, all with identity transform poses
	const uint32 ChunkCount = ApexDestructibleAsset.getChunkCount();
	if( ChunkCount == 0 )
	{
		UE_LOG(LogApexDestructibleAssetImport, Warning,TEXT("%s has no chunks"), ANSI_TO_TCHAR(ApexDestructibleAsset.getName()) );
		return;
	}

	const uint32 BoneCount = ChunkCount + 1;	// Adding one more bone for the root bone, required by the skeletal mesh

	// Format for bone names
	uint32 Q = ChunkCount-1;
	int32 MaxNumberWidth = 1;
	while ((Q /= 10) != 0)
	{
		++MaxNumberWidth;
	}

	// Turn parts into bones
	for (uint32 BoneIndex=0; BoneIndex<BoneCount; ++BoneIndex)
	{
		ImportData.RefBonesBinary.Add( VBone() );
		// Set bone
		VBone& Bone = ImportData.RefBonesBinary[BoneIndex];
		if (BoneIndex == 0)
		{
			// Bone 0 is the required root bone
			Bone.Name = TEXT("Root");
			Bone.NumChildren = ChunkCount;
			Bone.ParentIndex = INDEX_NONE;
		}
		else
		{
			// The rest are the parts
			Bone.Name = FString::Printf( TEXT("Part%0*d"), MaxNumberWidth, BoneIndex-1);
			Bone.NumChildren = 0;
			// Creates a simple "flat" hierarchy
			Bone.ParentIndex = 0;
		}

		// Set transform to identity
		VJointPos& JointMatrix = Bone.BonePos;
		JointMatrix.Transform = FTransform::Identity;
		JointMatrix.Length = 1.0f;
		JointMatrix.XSize = 100.0f;
		JointMatrix.YSize = 100.0f;
		JointMatrix.ZSize = 100.0f;
	}
}

/**
 * Fill an FSkeletalMeshImportData with data from an APEX Destructible Asset.
 *
 * @param ImportData - SkeletalMesh import data into which we are extracting information
 * @param ApexDestructibleAsset - the Apex Destructible Asset
 * @param bHaveAllNormals - if the function is successful, this value is true iff every submesh has a normal channel
 * @param bHaveAllTangents - if the function is successful, this value is true iff every submesh has a tangent channel
 *
 * @return Boolean true iff the operation is successful
 */
static bool FillSkelMeshImporterFromApexDestructibleAsset(FSkeletalMeshImportData& ImportData, const apex::DestructibleAsset& ApexDestructibleAsset, bool& bHaveAllNormals, bool& bHaveAllTangents)
{
	// The APEX Destructible Asset contains an APEX Render Mesh Asset, get a pointer to this
	const apex::RenderMeshAsset* ApexRenderMesh = ApexDestructibleAsset.getRenderMeshAsset();
	if (ApexRenderMesh == NULL)
	{
		return false;
	}

	if (ApexDestructibleAsset.getChunkCount() != ApexRenderMesh->getPartCount())
	{
		UE_LOG(LogApexDestructibleAssetImport, Warning,TEXT("Chunk count does not match part count.  APEX Destructible Asset with chunk instancing not yet supported."));
		return false;
	}

	// Apex Render Mesh uses triangle lists only, currently.  No need to triangulate.

	// Assume there are no vertex colors
	ImportData.bHasVertexColors = false;

	// Different submeshes can have different UV counts.  Get the max.
	uint32 UniqueUVCount = 0;

	// Count vertices and triangles
	uint32 VertexCount = 0;
	uint32 TriangleCount = 0;

	for (uint32 SubmeshIndex = 0; SubmeshIndex < ApexRenderMesh->getSubmeshCount(); ++SubmeshIndex)
	{
		const apex::RenderSubmesh& Submesh = ApexRenderMesh->getSubmesh(SubmeshIndex);
		const apex::VertexBuffer& VB = Submesh.getVertexBuffer();
		const apex::VertexFormat& VBFormat = VB.getFormat();

		// Count UV channels in this VB
		uint32 UVNum;
		for (UVNum = 0; UVNum < apex::VertexFormat::MAX_UV_COUNT; ++UVNum)
		{
			const apex::VertexFormat::BufferID BufferID = VBFormat.getSemanticID((apex::RenderVertexSemantic::Enum)(apex::RenderVertexSemantic::TEXCOORD0 + UVNum));
			if (VBFormat.getBufferIndexFromID(BufferID) < 0)
			{
				break;
			}
		}
		UniqueUVCount = FMath::Max<uint32>( UniqueUVCount, UVNum );

		// See if this VB has a color channel
		const apex::VertexFormat::BufferID BufferID = VBFormat.getSemanticID(apex::RenderVertexSemantic::COLOR);
		if (VBFormat.getBufferIndexFromID(BufferID) >= 0)
		{
			ImportData.bHasVertexColors = true;
		}

		// Count vertices
		VertexCount += VB.getVertexCount();

		// Count triangles
		uint32 IndexCount = 0;
		for (uint32 PartIndex = 0; PartIndex < ApexRenderMesh->getPartCount(); ++PartIndex)
		{
			IndexCount += Submesh.getIndexCount(PartIndex);
		}
		check(IndexCount%3 == 0);
		TriangleCount += IndexCount/3;
	}

	// One UV set is required but only import up to MAX_TEXCOORDS number of uv layers
	ImportData.NumTexCoords = FMath::Clamp<uint32>(UniqueUVCount, 1, MAX_TEXCOORDS);

	// Expand buffers in ImportData:
	ImportData.Points.AddUninitialized(VertexCount);
	ImportData.Influences.AddUninitialized(VertexCount);

	ImportData.Wedges.AddUninitialized(3*TriangleCount);
	uint32 WedgeIndex = 0;

	ImportData.Faces.AddUninitialized(TriangleCount);
	uint32 TriangleIndex = 0;

	uint32 VertexIndexBase = 0;

	// True until proven otherwise
	bHaveAllNormals = true;
	bHaveAllTangents = true;

	// APEX render meshes are organized by submesh (render elements)
	// Looping through submeshes first, can be done either way
	for (uint32 SubmeshIndex = 0; SubmeshIndex < ApexRenderMesh->getSubmeshCount(); ++SubmeshIndex)
	{
		// Submesh data
		const apex::RenderSubmesh& Submesh = ApexRenderMesh->getSubmesh(SubmeshIndex);
		const apex::VertexBuffer& VB = Submesh.getVertexBuffer();
		const apex::VertexFormat& VBFormat = VB.getFormat();
		const physx::PxU32 SubmeshVertexCount = VB.getVertexCount();

		if(SubmeshVertexCount == 0)
		{
			// Empty submesh, but the mesh as a whole may be valid, keep looking for more submeshes.
			continue;
		}

		// Get VB data semantic indices:

		// Positions
		const PxI32 PositionBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(apex::RenderVertexSemantic::POSITION));
		if (!VB.getBufferData(&ImportData.Points[VertexIndexBase], apex::RenderDataFormat::FLOAT3, sizeof(FVector), PositionBufferIndex, 0, SubmeshVertexCount))
		{
			return false;	// Need a position buffer!
		}

#if INVERT_Y_AND_V
		for (uint32 VertexNum = 0; VertexNum < SubmeshVertexCount; ++VertexNum)
		{
			ImportData.Points[VertexIndexBase + VertexNum].Y *= -1.0f;
		}
#endif

		// Normals
		const PxI32 NormalBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(apex::RenderVertexSemantic::NORMAL));
		TArray<FVector> Normals;
		Normals.AddUninitialized(SubmeshVertexCount);
		const bool bHaveNormals = VB.getBufferData(Normals.GetData(), apex::RenderDataFormat::FLOAT3, sizeof(FVector), NormalBufferIndex, 0, SubmeshVertexCount);
		if (!bHaveNormals)
		{
			FMemory::Memset(Normals.GetData(), 0, SubmeshVertexCount*sizeof(FVector));	// Fill with zeros
		}

		// Tangents
		const PxI32 TangentBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(apex::RenderVertexSemantic::TANGENT));
		TArray<FVector> Tangents;
		Tangents.AddUninitialized(SubmeshVertexCount);
		const bool bHaveTangents = VB.getBufferData(Tangents.GetData(), apex::RenderDataFormat::FLOAT3, sizeof(FVector), TangentBufferIndex, 0, SubmeshVertexCount);
		if (!bHaveTangents)
		{
			FMemory::Memset(Tangents.GetData(), 0, SubmeshVertexCount*sizeof(FVector));	// Fill with zeros
		}

		// Update bHaveAllNormals and bHaveAllTangents
		bHaveAllNormals = bHaveAllNormals && bHaveNormals;
		bHaveAllTangents = bHaveAllTangents && bHaveTangents;

		// Binormomals
		const PxI32 BinormalBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(apex::RenderVertexSemantic::BINORMAL));
		TArray<FVector> Binormals;
		Binormals.AddUninitialized(SubmeshVertexCount);
		bool bHaveBinormals = VB.getBufferData(Binormals.GetData(), apex::RenderDataFormat::FLOAT3, sizeof(FVector), BinormalBufferIndex, 0, SubmeshVertexCount);
		if (!bHaveBinormals)
		{
			bHaveBinormals = bHaveNormals && bHaveTangents;
			for (uint32 i = 0; i < SubmeshVertexCount; ++i)
			{
				Binormals[i] = Normals[i]^Tangents[i];	// Build from normals and tangents.  If one of these doesn't exist we'll get (0,0,0)'s
			}
		}

		// Colors
		const PxI32 ColorBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID(apex::RenderVertexSemantic::COLOR));
		TArray<FColor> Colors;
		Colors.AddUninitialized(SubmeshVertexCount);
		const bool bHaveColors = VB.getBufferData(Colors.GetData(), apex::RenderDataFormat::B8G8R8A8, sizeof(FColor), ColorBufferIndex, 0, SubmeshVertexCount);
		if (!bHaveColors)
		{
			FMemory::Memset(Colors.GetData(), 0xFF, SubmeshVertexCount*sizeof(FColor));	// Fill with 0xFF
		}

		// UVs
		TArray<FVector2D> UVs[apex::VertexFormat::MAX_UV_COUNT];
		for (uint32 UVNum = 0; UVNum < ImportData.NumTexCoords; ++UVNum)
		{
			const PxI32 UVBufferIndex = VBFormat.getBufferIndexFromID(VBFormat.getSemanticID((apex::RenderVertexSemantic::Enum)(apex::RenderVertexSemantic::TEXCOORD0 + UVNum)));
			UVs[UVNum].AddUninitialized(SubmeshVertexCount);
			if (!VB.getBufferData(&UVs[UVNum][0].X, apex::RenderDataFormat::FLOAT2, sizeof(FVector2D), UVBufferIndex, 0, SubmeshVertexCount))
			{
				FMemory::Memset(&UVs[UVNum][0].X, 0, SubmeshVertexCount*sizeof(FVector2D));	// Fill with zeros
			}
		}

		// Bone indices will not be imported - they're implicitly the PartIndex

		// Each submesh is partitioned into parts.  Currently we're assuming a 1-1 correspondence between chunks and parts,
		// which means that instanced chunks are not supported.  However, we will not assume that the chunk and part ordering is the same.
		// Therefore, instead of looping through parts, we loop through chunks here, and get the part index.
		for (uint32 ChunkIndex = 0; ChunkIndex < ApexDestructibleAsset.getChunkCount(); ++ChunkIndex)
		{
			const physx::PxU32 PartIndex = ApexDestructibleAsset.getPartIndex(ChunkIndex);
			const physx::PxU32* PartIndexBuffer = Submesh.getIndexBuffer(PartIndex);
			const physx::PxU32* PartIndexBufferStop = PartIndexBuffer + Submesh.getIndexCount(PartIndex);
			while (PartIndexBuffer < PartIndexBufferStop)
			{
				physx::PxU32 SubmeshVertexIndex[3];
#if !INVERT_Y_AND_V
				SubmeshVertexIndex[2] = *PartIndexBuffer++;
				SubmeshVertexIndex[1] = *PartIndexBuffer++;
				SubmeshVertexIndex[0] = *PartIndexBuffer++;
#else
				SubmeshVertexIndex[0] = *PartIndexBuffer++;
				SubmeshVertexIndex[1] = *PartIndexBuffer++;
				SubmeshVertexIndex[2] = *PartIndexBuffer++;
#endif

				// Fill triangle
				VTriangle& Triangle = ImportData.Faces[TriangleIndex++];

				// set the face smoothing by default. It could be any number, but not zero
				Triangle.SmoothingGroups = 255; 

				// Material index
				Triangle.MatIndex = SubmeshIndex;
				Triangle.AuxMatIndex = 0;

				// Per-vertex
				for (uint32 V = 0; V < 3; ++V)
				{
					// Tangent basis
					Triangle.TangentX[V] = Tangents[SubmeshVertexIndex[V]];
					Triangle.TangentY[V] = Binormals[SubmeshVertexIndex[V]];
					Triangle.TangentZ[V] = Normals[SubmeshVertexIndex[V]];
#if INVERT_Y_AND_V
					Triangle.TangentX[V].Y *= -1.0f;
					Triangle.TangentY[V].Y *= -1.0f;
					Triangle.TangentZ[V].Y *= -1.0f;
#endif

					// Wedges
					Triangle.WedgeIndex[V] = WedgeIndex;
					VVertex& Wedge = ImportData.Wedges[WedgeIndex++];
					Wedge.VertexIndex = VertexIndexBase + SubmeshVertexIndex[V];
					Wedge.MatIndex = Triangle.MatIndex;
					Wedge.Color = Colors[SubmeshVertexIndex[V]];
					Wedge.Reserved = 0;
					for (uint32 UVNum = 0; UVNum < ImportData.NumTexCoords; ++UVNum)
					{
						const FVector2D& UV = UVs[UVNum][SubmeshVertexIndex[V]];
#if !INVERT_Y_AND_V
						Wedge.UVs[UVNum] = UV;
#else
						Wedge.UVs[UVNum] = FVector2D(UV.X, 1.0f-UV.Y);
#endif
					}
				}
			}

			// Bone influences
			const physx::PxU32 PartVertexStart = Submesh.getFirstVertexIndex(PartIndex);
			const physx::PxU32 PartVertexStop = PartVertexStart + Submesh.getVertexCount(PartIndex);
			for (uint32 PartVertexIndex = PartVertexStart; PartVertexIndex < PartVertexStop; ++PartVertexIndex)
			{
				const physx::PxU32 VertexIndex = VertexIndexBase + PartVertexIndex;
				// Note, by using ChunkIndex instead of PartInedx we are effectively setting PartIndex = ChunkIndex, which is OK since we won't be supporting instancing with the SkeletalMesh.
				ImportData.Influences[VertexIndex].BoneIndex = ChunkIndex + 1;	// Adding 1, since the 0 bone will have no geometry from the Apex Destructible Asset.
				ImportData.Influences[VertexIndex].Weight = 1.0;
				ImportData.Influences[VertexIndex].VertexIndex = VertexIndex;
			}
		}

		VertexIndexBase += SubmeshVertexCount;
	}

	if(ImportData.Points.Num() == 0)
	{
		// No points were loaded, can't make a mesh.
		UE_LOG(LogApexDestructibleAssetImport, Error, TEXT("Failed to import destructible mesh - No positions were found in any submeshes."));
		return false;
	}

	// Create mapping from import to raw- @TODO trivial at the moment, do we need this info for destructibles?
	ImportData.PointToRawMap.AddUninitialized(ImportData.Points.Num());
	for(int32 PointIdx=0; PointIdx<ImportData.PointToRawMap.Num(); PointIdx++)
	{
		ImportData.PointToRawMap[PointIdx] = PointIdx;
	}

	return true;
}

static apex::DestructibleAsset* CreateApexDestructibleAssetFromPxStream(physx::PxFileBuf& Stream)
{
	// Peek into the buffer to see what kind of data it is (binary or xml)
	NvParameterized::Serializer::SerializeType SerializeType = GApexSDK->getSerializeType(Stream);
	// Create an NvParameterized serializer for the correct data type
	NvParameterized::Serializer* Serializer = GApexSDK->createSerializer(SerializeType);

	if (Serializer)
	{
		// Deserialize into a DeserializedData buffer
		NvParameterized::Serializer::DeserializedData DeserializedData;
		Serializer->deserialize(Stream, DeserializedData);
		apex::Asset* ApexAsset = NULL;
		if( DeserializedData.size() > 0 )
		{
			// The DeserializedData has something in it, so create an APEX asset from it
			ApexAsset = GApexSDK->createAsset( DeserializedData[0], NULL );
			// Make sure it's a Destructible asset
			if (ApexAsset->getObjTypeID() != GApexModuleDestructible->getModuleID())
			{
				GPhysCommandHandler->DeferredRelease(ApexAsset);
				ApexAsset = NULL;
			}
		}
		// Release the serializer
		Serializer->release();

		return (apex::DestructibleAsset*)ApexAsset;
	}

	return NULL;
}

apex::DestructibleAsset* CreateApexDestructibleAssetFromBuffer(const uint8* Buffer, int32 BufferSize)
{
	apex::DestructibleAsset* ApexDestructibleAsset = NULL;

	// Wrap Buffer with the APEX read stream class
	physx::PxFileBuf* Stream = GApexSDK->createMemoryReadStream(Buffer, BufferSize);

	if (Stream != NULL)
	{
		ApexDestructibleAsset = CreateApexDestructibleAssetFromPxStream(*Stream);

		// Release our stream
		GApexSDK->releaseMemoryReadStream(*Stream);
	}

	return ApexDestructibleAsset;
}

apex::DestructibleAsset* CreateApexDestructibleAssetFromFile(const FString& Filename)
{
	apex::DestructibleAsset* ApexDestructibleAsset = NULL;

	// Create a stream to read the file
	physx::PxFileBuf* Stream = GApexSDK->createStream(TCHAR_TO_ANSI(*Filename), PxFileBuf::OPEN_READ_ONLY);

	if (Stream != NULL)
	{
		ApexDestructibleAsset = CreateApexDestructibleAssetFromPxStream(*Stream);

		// Release our stream
		Stream->release();
	}

	return ApexDestructibleAsset;
}

bool SetApexDestructibleAsset(UDestructibleMesh& DestructibleMesh, apex::DestructibleAsset& ApexDestructibleAsset, FSkeletalMeshImportData* OutData, EDestructibleImportOptions::Type Options)
{
	DestructibleMesh.PreEditChange(NULL);

	ExistingDestMeshData * ExistDestMeshDataPtr = nullptr;
	if(Options & EDestructibleImportOptions::PreserveSettings)
	{
		ExistDestMeshDataPtr = SaveExistingDestMeshData(&DestructibleMesh);
	}
	
	// The asset is going away, which will destroy any actors created from it.  We must destroy the physics state of any destructible mesh components before we release the asset.
	for(TObjectIterator<UDestructibleComponent> It; It; ++It)
	{
		UDestructibleComponent* DestructibleComponent = *It;
		if(DestructibleComponent->SkeletalMesh == &DestructibleMesh && DestructibleComponent->IsPhysicsStateCreated())
		{
			DestructibleComponent->DestroyPhysicsState();
		}
	}

	// Release old NxDestructibleAsset if it exists
	if (DestructibleMesh.ApexDestructibleAsset != NULL && DestructibleMesh.ApexDestructibleAsset != &ApexDestructibleAsset)
	{
		GPhysCommandHandler->DeferredRelease(DestructibleMesh.ApexDestructibleAsset);
	}

	// BRGTODO - need to remove the render data from the ApexDestructibleAsset, no longer need it
	// Removing const cast ... we'll have to make it non-const anyway when we modify it
	DestructibleMesh.ApexDestructibleAsset = &ApexDestructibleAsset;

	if ( !(Options&EDestructibleImportOptions::PreserveSettings) )
	{
		// Resize the depth parameters array to the appropriate size
		DestructibleMesh.DefaultDestructibleParameters.DepthParameters.Init(FDestructibleDepthParameters(), ApexDestructibleAsset.getDepthCount());

		// Resize the fracture effects array to the appropriate size
		DestructibleMesh.FractureEffects.AddZeroed(ApexDestructibleAsset.getDepthCount());

		// Load the UnrealEd-editable parameters from the destructible asset
		DestructibleMesh.LoadDefaultDestructibleParametersFromApexAsset();
	}
		
	// Create body setup for the destructible mesh
	DestructibleMesh.CreateBodySetup();

#if 0	// BRGTODO
	// warning for missing smoothing group info
	CheckSmoothingInfo(FbxMesh);
#endif
	
	FSkeletalMeshImportData TempData;
	// Fill with data from buffer
	FSkeletalMeshImportData* SkelMeshImportDataPtr = &TempData;
	if( OutData )
	{
		SkelMeshImportDataPtr = OutData;
	}
	
	// Get all material names here
	ImportMaterialsForSkelMesh(*SkelMeshImportDataPtr, ApexDestructibleAsset);

	// Import animation hierarchy, although this is trivial for an Apex Destructible Asset
	CreateBones(*SkelMeshImportDataPtr, ApexDestructibleAsset);

	// Import graphics data
	bool bHaveNormals, bHaveTangents;
	if (!FillSkelMeshImporterFromApexDestructibleAsset(*SkelMeshImportDataPtr, ApexDestructibleAsset, bHaveNormals, bHaveTangents))
	{
		return false;
	}

#if 0	// BRGTODO - what is this?
	if( SkelMeshImportDataPtr->Materials.Num() == FbxMatList.Num() )
	{
		// reorder material according to "SKinXX" in material name
		SetMaterialSkinXXOrder(*SkelMeshImportDataPtr, FbxMatList );
	}
#endif

#if 0	// BRGTODO - what is this?
	if( ImportOptions->bSplitNonMatchingTriangles )
	{
		DoUnSmoothVerts(*SkelMeshImportDataPtr);
	}
#endif

	// process materials from import data
	ProcessImportMeshMaterials( DestructibleMesh.Materials,*SkelMeshImportDataPtr );
	
	// process reference skeleton from import data
	int32 SkeletalDepth=0;
	if(!ProcessImportMeshSkeleton(DestructibleMesh.Skeleton, DestructibleMesh.RefSkeleton, SkeletalDepth, *SkelMeshImportDataPtr))
	{
		return false;
	}
	UE_LOG(LogApexDestructibleAssetImport, Warning, TEXT("Bones digested - %i  Depth of hierarchy - %i"), DestructibleMesh.RefSkeleton.GetNum(), SkeletalDepth);

	// process bone influences from import data
	ProcessImportMeshInfluences(*SkelMeshImportDataPtr);

	FSkeletalMeshResource& DestructibleMeshResource = *DestructibleMesh.GetImportedResource();
	check(DestructibleMeshResource.LODModels.Num() == 0);
	DestructibleMeshResource.LODModels.Empty();
	new(DestructibleMeshResource.LODModels)FStaticLODModel();

	DestructibleMesh.LODInfo.Empty();
	DestructibleMesh.LODInfo.AddZeroed();
	DestructibleMesh.LODInfo[0].LODHysteresis = 0.02f;

	// Create initial bounding box based on expanded version of reference pose for meshes without physics assets. Can be overridden by artist.
	FBox BoundingBox(SkelMeshImportDataPtr->Points.GetData(), SkelMeshImportDataPtr->Points.Num());
	DestructibleMesh.SetImportedBounds(FBoxSphereBounds(BoundingBox));

	// Store whether or not this mesh has vertex colors
	DestructibleMesh.bHasVertexColors = SkelMeshImportDataPtr->bHasVertexColors;

	FStaticLODModel& LODModel = DestructibleMeshResource.LODModels[0];
	
	LODModel.ActiveBoneIndices.Add(0);

	// Pass the number of texture coordinate sets to the LODModel.  Ensure there is at least one UV coord
	LODModel.NumTexCoords = FMath::Max<uint32>(1,SkelMeshImportDataPtr->NumTexCoords);
//	if( bCreateRenderData )	// We always create render data
	{
		// copy vertex data needed to generate skinning streams for LOD
		TArray<FVector> LODPoints;
		TArray<FMeshWedge> LODWedges;
		TArray<FMeshFace> LODFaces;
		TArray<FVertInfluence> LODInfluences;
		TArray<int32> LODPointToRawMap;
		SkelMeshImportDataPtr->CopyLODImportData(LODPoints,LODWedges,LODFaces,LODInfluences,LODPointToRawMap);

		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

		IMeshUtilities::MeshBuildOptions BuildOptions;
		BuildOptions.bKeepOverlappingVertices = false;
		BuildOptions.bComputeNormals = !bHaveNormals;
		BuildOptions.bComputeTangents = !bHaveTangents;

		// Create actual rendering data.
		if (!MeshUtilities.BuildSkeletalMesh(DestructibleMeshResource.LODModels[0], DestructibleMesh.RefSkeleton, LODInfluences,LODWedges,LODFaces,LODPoints,LODPointToRawMap,BuildOptions))
		{
			DestructibleMesh.MarkPendingKill();
			return false;
		}

		// Presize the per-section shadow casting array with the number of sections in the imported LOD.
		const int32 NumSections = LODModel.Sections.Num();

		for ( int32 SectionIndex = 0 ; SectionIndex < NumSections ; ++SectionIndex )
		{
			DestructibleMesh.LODInfo[0].TriangleSortSettings.AddZeroed();
		}

		if (ExistDestMeshDataPtr)
		{
			RestoreExistingDestMeshData(ExistDestMeshDataPtr, &DestructibleMesh);
			delete ExistDestMeshDataPtr;
			ExistDestMeshDataPtr = NULL;
		}

		DestructibleMesh.CalculateInvRefMatrices();
		DestructibleMesh.PostEditChange();
		DestructibleMesh.MarkPackageDirty();

#if 0 // BRGTODO : Check, we don't need this, do we?
		// We have to go and fix any AnimSetMeshLinkup objects that refer to this skeletal mesh, as the reference skeleton has changed.
		for(TObjectIterator<UAnimSet> It;It;++It)
		{
			UAnimSet* AnimSet = *It;

			// Get DestructibleMesh path name
			FName SkelMeshName = FName( *DestructibleMesh.GetPathName() );

			// See if we have already cached this Skeletal Mesh.
			const int32* IndexPtr = AnimSet->SkelMesh2LinkupCache.Find( SkelMeshName );

			if( IndexPtr )
			{
				AnimSet->LinkupCache( *IndexPtr ).BuildLinkup( &DestructibleMesh, AnimSet );
			}
		}
#endif

		// Now iterate over all skeletal mesh components re-initialising them.
		for(TObjectIterator<UDestructibleComponent> It; It; ++It)
		{
			UDestructibleComponent* DestructibleComponent = *It;
			if(DestructibleComponent->SkeletalMesh == &DestructibleMesh)
			{
				FComponentReregisterContext ReregisterContext(DestructibleComponent);
			}
		}
	}

#if INVERT_Y_AND_V
	// Apply transformation for Y inversion
	const physx::PxMat44 MirrorY = physx::PxMat44(physx::PxVec4(1.0f, -1.0f, 1.0f, 1.0f));
#if !USE_TEMPORARY_TRANSFORMATION_FUNCTION
	ApexDestructibleAsset.applyTransformation(MirrorY, 1.0f);
#else
	ApplyTransformationToApexDestructibleAsset( ApexDestructibleAsset, MirrorY );
#endif
#endif

	return true;
}

bool BuildDestructibleMeshFromFractureSettings(UDestructibleMesh& DestructibleMesh, FSkeletalMeshImportData* OutData)
{
	bool Success = false;

#if WITH_APEX
	apex::DestructibleAsset* NewApexDestructibleAsset = NULL;

#if WITH_EDITORONLY_DATA
	if (DestructibleMesh.FractureSettings != NULL)
	{
		TArray<UMaterialInterface*> OverrideMaterials;
		OverrideMaterials.SetNumUninitialized(DestructibleMesh.Materials.Num());	//save old materials
		for (int32 MaterialIndex = 0; MaterialIndex < DestructibleMesh.Materials.Num(); ++MaterialIndex)
		{
			OverrideMaterials[MaterialIndex] = DestructibleMesh.Materials[MaterialIndex].MaterialInterface;
		}

		DestructibleMesh.Materials.SetNum(DestructibleMesh.FractureSettings->Materials.Num());

		for (int32 MaterialIndex = 0; MaterialIndex < DestructibleMesh.Materials.Num(); ++MaterialIndex)
		{
			if (MaterialIndex < OverrideMaterials.Num())	//if user has overridden materials use it
			{
				DestructibleMesh.Materials[MaterialIndex].MaterialInterface = OverrideMaterials[MaterialIndex];
			}
			else
			{
				DestructibleMesh.Materials[MaterialIndex].MaterialInterface = DestructibleMesh.FractureSettings->Materials[MaterialIndex];
			}

		}

		apex::DestructibleAssetCookingDesc DestructibleAssetCookingDesc;
		DestructibleMesh.FractureSettings->BuildDestructibleAssetCookingDesc(DestructibleAssetCookingDesc);
		NewApexDestructibleAsset = DestructibleMesh.FractureSettings->CreateApexDestructibleAsset(DestructibleAssetCookingDesc);
	}
#endif	// WITH_EDITORONLY_DATA

	if (NewApexDestructibleAsset != NULL)
	{
		Success = SetApexDestructibleAsset(DestructibleMesh, *NewApexDestructibleAsset, OutData, EDestructibleImportOptions::PreserveSettings);
	}
#endif // WITH_APEX

	return Success;
}

UDestructibleMesh* ImportDestructibleMeshFromApexDestructibleAsset(UObject* InParent, apex::DestructibleAsset& ApexDestructibleAsset, FName Name, EObjectFlags Flags, FSkeletalMeshImportData* OutData, EDestructibleImportOptions::Type Options)
{
	// The APEX Destructible Asset contains an APEX Render Mesh Asset, get a pointer to this
	const apex::RenderMeshAsset* ApexRenderMesh = ApexDestructibleAsset.getRenderMeshAsset();
	if (ApexRenderMesh == NULL)
	{
		return NULL;
	}

	// Number of submeshes (aka "elements" in Unreal)
	const physx::PxU32 SubmeshCount = ApexRenderMesh->getSubmeshCount();
	if (SubmeshCount == 0)
	{
		return NULL;
	}

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	UDestructibleMesh* DestructibleMesh = FindObject<UDestructibleMesh>(InParent, *Name.ToString());
	if(DestructibleMesh)
	{
		// we found an existing mesh (reimport), clean up LODModels for the import process
		if(FSkeletalMeshResource* ImportedResource = DestructibleMesh->GetImportedResource())
		{
			for(FStaticLODModel& LODModel : ImportedResource->LODModels)
			{
				LODModel.ReleaseResources();
			}

			// Although we flushed above to make sure the resources weren't being used, we need
			// to flush again as the call to Empty below will call destructors on the lod models.
			// The renderer must release the resources before that happens.
			FlushRenderingCommands();

			ImportedResource->LODModels.Empty(1);
		}
	}
	else
	{
		// Create the new UDestructibleMesh object if the one with the same name does not exist
		DestructibleMesh = NewObject<UDestructibleMesh>(InParent, Name, Flags);
	}
	
	if (!(Options & EDestructibleImportOptions::PreserveSettings))
	{
		// Store the current file path and timestamp for re-import purposes
		// @todo AssetImportData make a data class for Apex destructible assets
		DestructibleMesh->AssetImportData = NewObject<UAssetImportData>(DestructibleMesh);
		DestructibleMesh->AssetImportData->Update(UFactory::GetCurrentFilename());
	}

	DestructibleMesh->PreEditChange(NULL);

	// Build FractureSettings from ApexDestructibleAsset in case we want to re-fracture
#if WITH_EDITORONLY_DATA
	DestructibleMesh->CreateFractureSettings();
	DestructibleMesh->FractureSettings->BuildRootMeshFromApexDestructibleAsset(ApexDestructibleAsset, Options);
	// Fill materials
	DestructibleMesh->FractureSettings->Materials.Reset(DestructibleMesh->Materials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < DestructibleMesh->Materials.Num(); ++MaterialIndex)
	{
		DestructibleMesh->FractureSettings->Materials.Insert(DestructibleMesh->Materials[MaterialIndex].MaterialInterface, MaterialIndex);
	}
#endif	// WITH_EDITORONLY_DATA

	if (!SetApexDestructibleAsset(*DestructibleMesh, ApexDestructibleAsset, OutData, Options))
	{
		// should remove this destructible mesh. if not, this object causes a crash when ticking because it doesn't have proper rendering resources
		// @TODO : creates this destructible mesh after loading data completely
		DestructibleMesh->PostEditChange();
		DestructibleMesh->ConditionalBeginDestroy();
		return NULL;
	}

	return DestructibleMesh;
}

#endif // WITH_APEX
#endif