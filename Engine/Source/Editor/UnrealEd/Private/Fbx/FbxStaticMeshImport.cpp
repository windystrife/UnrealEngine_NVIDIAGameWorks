// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Static mesh creation from FBX data.
	Largely based on StaticMeshEdit.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/Object.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Factories/Factory.h"
#include "Factories/FbxSceneImportFactory.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Polys.h"
#include "Engine/StaticMeshSocket.h"
#include "Editor.h"
#include "RawMesh.h"

#include "StaticMeshResources.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Logging/TokenizedMessage.h"
#include "FbxImporter.h"
#include "GeomFitUtils.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/FbxErrors.h"
#include "PhysicsEngine/BodySetup.h"

#define LOCTEXT_NAMESPACE "FbxStaticMeshImport"

static const int32 LARGE_MESH_MATERIAL_INDEX_THRESHOLD = 64;

using namespace UnFbx;

struct ExistingStaticMeshData;
extern ExistingStaticMeshData* SaveExistingStaticMeshData(UStaticMesh* ExistingMesh, FBXImportOptions* ImportOptions, int32 LodIndex);
extern void RestoreExistingMeshSettings(struct ExistingStaticMeshData* ExistingMesh, UStaticMesh* NewMesh, int32 LODIndex);
extern void RestoreExistingMeshData(struct ExistingStaticMeshData* ExistingMeshDataPtr, UStaticMesh* NewMesh, int32 LodLevel, bool bResetMaterialSlots);
extern void UpdateSomeLodsImportMeshData(UStaticMesh* NewMesh, TArray<int32> *ReimportLodList);

static FbxString GetNodeNameWithoutNamespace( FbxNode* Node )
{
	FbxString NodeName = Node->GetName();

	// Namespaces are marked with colons so find the last colon which will mark the start of the actual name
	int32 LastNamespceIndex = NodeName.ReverseFind(':');

	if( LastNamespceIndex == -1 )
	{
		// No namespace
		return NodeName;
	}
	else
	{
		// chop off the namespace
		return NodeName.Right( NodeName.GetLen() - (LastNamespceIndex + 1) );
	}
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
UStaticMesh* UnFbx::FFbxImporter::ImportStaticMesh(UObject* InParent, FbxNode* Node, const FName& Name, EObjectFlags Flags, UFbxStaticMeshImportData* ImportData, UStaticMesh* InStaticMesh, int LODIndex, void *ExistMeshDataPtr)
{
	TArray<FbxNode*> MeshNodeArray;
	
	if ( !Node->GetMesh())
	{
		return NULL;
	}
	
	MeshNodeArray.Add(Node);
	return ImportStaticMeshAsSingle(InParent, MeshNodeArray, Name, Flags, ImportData, InStaticMesh, LODIndex, ExistMeshDataPtr);
}

// Wraps some common code useful for multiple fbx import code path
struct FFBXUVs
{
	// constructor
	FFBXUVs(FbxMesh* Mesh)
		: UniqueUVCount(0)
	{
		check(Mesh);

		//
		//	store the UVs in arrays for fast access in the later looping of triangles 
		//
		// mapping from UVSets to Fbx LayerElementUV
		// Fbx UVSets may be duplicated, remove the duplicated UVSets in the mapping 
		int32 LayerCount = Mesh->GetLayerCount();
		if (LayerCount > 0)
		{
			int32 UVLayerIndex;
			for (UVLayerIndex = 0; UVLayerIndex<LayerCount; UVLayerIndex++)
			{
				FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
				int UVSetCount = lLayer->GetUVSetCount();
				if(UVSetCount)
				{
					FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
					for (int UVIndex = 0; UVIndex<UVSetCount; UVIndex++)
					{
						FbxLayerElementUV const* ElementUV = EleUVs[UVIndex];
						if (ElementUV)
						{
							const char* UVSetName = ElementUV->GetName();
							FString LocalUVSetName = UTF8_TO_TCHAR(UVSetName);
							if (LocalUVSetName.IsEmpty())
							{
								LocalUVSetName = TEXT("UVmap_") + FString::FromInt(UVLayerIndex);
							}

							UVSets.AddUnique(LocalUVSetName);
						}
					}
				}
			}
		}


		// If the the UV sets are named using the following format (UVChannel_X; where X ranges from 1 to 4)
		// we will re-order them based on these names.  Any UV sets that do not follow this naming convention
		// will be slotted into available spaces.
		if(UVSets.Num())
		{
			for(int32 ChannelNumIdx = 0; ChannelNumIdx < 4; ChannelNumIdx++)
			{
				FString ChannelName = FString::Printf( TEXT("UVChannel_%d"), ChannelNumIdx+1 );
				int32 SetIdx = UVSets.Find( ChannelName );

				// If the specially formatted UVSet name appears in the list and it is in the wrong spot,
				// we will swap it into the correct spot.
				if( SetIdx != INDEX_NONE && SetIdx != ChannelNumIdx )
				{
					// If we are going to swap to a position that is outside the bounds of the
					// array, then we pad out to that spot with empty data.
					for(int32 ArrSize = UVSets.Num(); ArrSize < ChannelNumIdx+1; ArrSize++)
					{
						UVSets.Add ( FString(TEXT("")) );
					}
					//Swap the entry into the appropriate spot.
					UVSets.Swap( SetIdx, ChannelNumIdx );
				}
			}
		}
	}

	void Phase2(FbxMesh* Mesh)
	{
		//
		//	store the UVs in arrays for fast access in the later looping of triangles 
		//
		UniqueUVCount = UVSets.Num();
		if (UniqueUVCount > 0)
		{
			LayerElementUV.AddZeroed(UniqueUVCount);
			UVReferenceMode.AddZeroed(UniqueUVCount);
			UVMappingMode.AddZeroed(UniqueUVCount);
		}
		for (int32 UVIndex = 0; UVIndex < UniqueUVCount; UVIndex++)
		{
			LayerElementUV[UVIndex] = NULL;
			for (int32 UVLayerIndex = 0, LayerCount = Mesh->GetLayerCount(); UVLayerIndex < LayerCount; UVLayerIndex++)
			{
				FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
				int UVSetCount = lLayer->GetUVSetCount();
				if(UVSetCount)
				{
					FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
					for (int32 FbxUVIndex = 0; FbxUVIndex<UVSetCount; FbxUVIndex++)
					{
						FbxLayerElementUV const* ElementUV = EleUVs[FbxUVIndex];
						if (ElementUV)
						{
							const char* UVSetName = ElementUV->GetName();
							FString LocalUVSetName = UTF8_TO_TCHAR(UVSetName);
							if (LocalUVSetName.IsEmpty())
							{
								LocalUVSetName = TEXT("UVmap_") + FString::FromInt(UVLayerIndex);
							}
							if (LocalUVSetName == UVSets[UVIndex])
							{
								LayerElementUV[UVIndex] = ElementUV;
								UVReferenceMode[UVIndex] = ElementUV->GetReferenceMode();
								UVMappingMode[UVIndex] = ElementUV->GetMappingMode();
								break;
							}
						}
					}
				}
			}
		}
		UniqueUVCount = FMath::Min<int32>(UniqueUVCount, MAX_MESH_TEXTURE_COORDS);
	}

	int32 FindLightUVIndex() const
	{
		// See if any of our UV set entry names match LightMapUV.
		for(int32 UVSetIdx = 0; UVSetIdx < UVSets.Num(); UVSetIdx++)
		{
			if( UVSets[UVSetIdx] == TEXT("LightMapUV"))
			{
				return UVSetIdx;
			}
		}

		// not found
		return INDEX_NONE;
	}
	
	// @param FaceCornerIndex usually TriangleIndex * 3 + CornerIndex but more complicated for mixed n-gons
	int32 ComputeUVIndex(int32 UVLayerIndex, int32 lControlPointIndex, int32 FaceCornerIndex) const
	{
		int32 UVMapIndex = (UVMappingMode[UVLayerIndex] == FbxLayerElement::eByControlPoint) ? lControlPointIndex : FaceCornerIndex;
						
		int32 Ret;

		if(UVReferenceMode[UVLayerIndex] == FbxLayerElement::eDirect)
		{
			Ret = UVMapIndex;
		}
		else
		{
			FbxLayerElementArrayTemplate<int>& Array = LayerElementUV[UVLayerIndex]->GetIndexArray();
			Ret = Array.GetAt(UVMapIndex);
		}

		return Ret;
	}

	// todo: is that needed? could the dtor do it?
	void Cleanup()
	{
		//
		// clean up.  This needs to happen before the mesh is destroyed
		//
		LayerElementUV.Empty();
		UVReferenceMode.Empty();
		UVMappingMode.Empty();
	}

	TArray<FString> UVSets;
	TArray<FbxLayerElementUV const*> LayerElementUV;
	TArray<FbxLayerElement::EReferenceMode> UVReferenceMode;
	TArray<FbxLayerElement::EMappingMode> UVMappingMode;
	int32 UniqueUVCount;
};

bool UnFbx::FFbxImporter::BuildStaticMeshFromGeometry(FbxNode* Node, UStaticMesh* StaticMesh, TArray<FFbxMaterial>& MeshMaterials, int32 LODIndex,FRawMesh& RawMesh,
	EVertexColorImportOption::Type VertexColorImportOption, const TMap<FVector, FColor>& ExistingVertexColorData, const FColor& VertexOverrideColor)
{
	check(StaticMesh->SourceModels.IsValidIndex(LODIndex));
	FbxMesh* Mesh = Node->GetMesh();
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];

	//remove the bad polygons before getting any data from mesh
	Mesh->RemoveBadPolygons();

	//Get the base layer of the mesh
	FbxLayer* BaseLayer = Mesh->GetLayer(0);
	if (BaseLayer == NULL)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_NoGeometryInMesh", "There is no geometry information in mesh '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_NoGeometry);
		return false;
	}

	FFBXUVs FBXUVs(Mesh);
	int32 FBXNamedLightMapCoordinateIndex = FBXUVs.FindLightUVIndex();
	if (FBXNamedLightMapCoordinateIndex != INDEX_NONE)
	{
		StaticMesh->LightMapCoordinateIndex = FBXNamedLightMapCoordinateIndex;
	}
	
	//
	// create materials
	//
	int32 MaterialCount = 0;
	TArray<UMaterialInterface*> Materials;
	if (ImportOptions->bImportMaterials)
	{
		bool bForSkeletalMesh = false;
		CreateNodeMaterials(Node, Materials, FBXUVs.UVSets, bForSkeletalMesh);
	}
	else if (ImportOptions->bImportTextures)
	{
		ImportTexturesFromNode(Node);
	}

	MaterialCount = Node->GetMaterialCount();
	check(!ImportOptions->bImportMaterials || Materials.Num() == MaterialCount);
	
	// Used later to offset the material indices on the raw triangle data
	int32 MaterialIndexOffset = MeshMaterials.Num();

	for (int32 MaterialIndex=0; MaterialIndex<MaterialCount; MaterialIndex++)
	{
		FFbxMaterial* NewMaterial = new(MeshMaterials) FFbxMaterial;
		FbxSurfaceMaterial *FbxMaterial = Node->GetMaterial(MaterialIndex);
		NewMaterial->FbxMaterial = FbxMaterial;
		if (ImportOptions->bImportMaterials)
		{
			NewMaterial->Material = Materials[MaterialIndex];
		}
		else
		{
			FString MaterialFullName = GetMaterialFullName(*FbxMaterial);
			FString BasePackageName = PackageTools::SanitizePackageName(FPackageName::GetLongPackagePath(StaticMesh->GetOutermost()->GetName()) / MaterialFullName);
			UMaterialInterface* UnrealMaterialInterface = FindObject<UMaterialInterface>(NULL, *(BasePackageName + TEXT(".") + MaterialFullName));
			if (UnrealMaterialInterface == NULL)
			{
				//In case we do not found the material we can see if the material is in the material list of the static mesh material
				FName MaterialFbxFullName = UTF8_TO_TCHAR(MakeName(FbxMaterial->GetName()));
				for (const FStaticMaterial &StaticMaterial : StaticMesh->StaticMaterials)
				{
					if (StaticMaterial.ImportedMaterialSlotName == MaterialFbxFullName)
					{
						UnrealMaterialInterface = StaticMaterial.MaterialInterface;
						break;
					}
				}
				
				if (UnrealMaterialInterface == NULL)
				{
					UnrealMaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
				}
			}
			NewMaterial->Material = UnrealMaterialInterface;
		}
	}

	if ( MaterialCount == 0 )
	{
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterial);
		FFbxMaterial* NewMaterial = new(MeshMaterials) FFbxMaterial;
		NewMaterial->Material = DefaultMaterial;
		NewMaterial->FbxMaterial = NULL;
		MaterialCount = 1;
	}

	//
	// Convert data format to unreal-compatible
	//

	// Must do this before triangulating the mesh due to an FBX bug in TriangulateMeshAdvance
	int32 LayerSmoothingCount = Mesh->GetLayerCount(FbxLayerElement::eSmoothing);
	for(int32 i = 0; i < LayerSmoothingCount; i++)
	{
		FbxLayerElementSmoothing const* SmoothingInfo = Mesh->GetLayer(0)->GetSmoothing();
		if (SmoothingInfo && SmoothingInfo->GetMappingMode() != FbxLayerElement::eByPolygon)
		{
			GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing (Mesh, i);
		}
	}

	if (!Mesh->IsTriangleMesh())
	{
		if(!GIsAutomationTesting)
		UE_LOG(LogFbx, Warning, TEXT("Triangulating static mesh %s"), UTF8_TO_TCHAR(Node->GetName()));

		const bool bReplace = true;
		FbxNodeAttribute* ConvertedNode = GeometryConverter->Triangulate(Mesh, bReplace);

		if( ConvertedNode != NULL && ConvertedNode->GetAttributeType() == FbxNodeAttribute::eMesh )
		{
			Mesh = (fbxsdk::FbxMesh*)ConvertedNode;
		}
		else
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_FailedToTriangulate", "Unable to triangulate mesh '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_TriangulationFailed);
			return false; // not clean, missing some dealloc
		}
	}
	
	// renew the base layer
	BaseLayer = Mesh->GetLayer(0);

	//
	//	get the "material index" layer.  Do this AFTER the triangulation step as that may reorder material indices
	//
	FbxLayerElementMaterial* LayerElementMaterial = BaseLayer->GetMaterials();
	FbxLayerElement::EMappingMode MaterialMappingMode = LayerElementMaterial ? 
		LayerElementMaterial->GetMappingMode() : FbxLayerElement::eByPolygon;

	//	todo second phase UV, ok to put in first phase?
	FBXUVs.Phase2(Mesh);

	//
	// get the smoothing group layer
	//
	bool bSmoothingAvailable = false;

	FbxLayerElementSmoothing const* SmoothingInfo = BaseLayer->GetSmoothing();
	FbxLayerElement::EReferenceMode SmoothingReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode SmoothingMappingMode(FbxLayerElement::eByEdge);
	if (SmoothingInfo)
	{

		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eByPolygon )
		{
			bSmoothingAvailable = true;
		}


		SmoothingReferenceMode = SmoothingInfo->GetReferenceMode();
		SmoothingMappingMode = SmoothingInfo->GetMappingMode();
	}

	//
	// get the first vertex color layer
	//
	FbxLayerElementVertexColor* LayerElementVertexColor = BaseLayer->GetVertexColors();
	FbxLayerElement::EReferenceMode VertexColorReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode VertexColorMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementVertexColor)
	{
		VertexColorReferenceMode = LayerElementVertexColor->GetReferenceMode();
		VertexColorMappingMode = LayerElementVertexColor->GetMappingMode();
	}

	//
	// get the first normal layer
	//
	FbxLayerElementNormal* LayerElementNormal = BaseLayer->GetNormals();
	FbxLayerElementTangent* LayerElementTangent = BaseLayer->GetTangents();
	FbxLayerElementBinormal* LayerElementBinormal = BaseLayer->GetBinormals();

	//whether there is normal, tangent and binormal data in this mesh
	bool bHasNTBInformation = LayerElementNormal && LayerElementTangent && LayerElementBinormal;

	FbxLayerElement::EReferenceMode NormalReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode NormalMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementNormal)
	{
		NormalReferenceMode = LayerElementNormal->GetReferenceMode();
		NormalMappingMode = LayerElementNormal->GetMappingMode();
	}

	FbxLayerElement::EReferenceMode TangentReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode TangentMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementTangent)
	{
		TangentReferenceMode = LayerElementTangent->GetReferenceMode();
		TangentMappingMode = LayerElementTangent->GetMappingMode();
	}

	FbxLayerElement::EReferenceMode BinormalReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode BinormalMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementBinormal)
	{
		BinormalReferenceMode = LayerElementBinormal->GetReferenceMode();
		BinormalMappingMode = LayerElementBinormal->GetMappingMode();
	}

	//
	// build collision
	//
	bool bImportedCollision = ImportCollisionModels(StaticMesh, GetNodeNameWithoutNamespace(Node));

	//If we import a collision or we "generate one and remove the degenerates triangles" we will automatically set the section collision boolean.
	bool bEnableCollision = bImportedCollision || (GBuildStaticMeshCollision && LODIndex == 0 && ImportOptions->bRemoveDegenerates);
	for(int32 SectionIndex=MaterialIndexOffset; SectionIndex<MaterialIndexOffset+MaterialCount; SectionIndex++)
	{
		FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);
		Info.bEnableCollision = bEnableCollision;
		StaticMesh->SectionInfoMap.Set(LODIndex, SectionIndex, Info);
	}

	//
	// build un-mesh triangles
	//

	// Construct the matrices for the conversion from right handed to left handed system
	FbxAMatrix TotalMatrix;
	FbxAMatrix TotalMatrixForNormal;
	TotalMatrix = ComputeTotalMatrix(Node);
	TotalMatrixForNormal = TotalMatrix.Inverse();
	TotalMatrixForNormal = TotalMatrixForNormal.Transpose();
	int32 TriangleCount = Mesh->GetPolygonCount();

	if(TriangleCount == 0)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_NoTrianglesFoundInMesh", "No triangles were found on mesh  '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::StaticMesh_NoTriangles);
		return false;
	}

	int32 VertexCount = Mesh->GetControlPointsCount();
	int32 WedgeCount = TriangleCount * 3;
	bool OddNegativeScale = IsOddNegativeScale(TotalMatrix);

	int32 VertexOffset = RawMesh.VertexPositions.Num();
	int32 WedgeOffset = RawMesh.WedgeIndices.Num();
	int32 TriangleOffset = RawMesh.FaceMaterialIndices.Num();

	int32 MaxMaterialIndex = 0;

	// Reserve space for attributes.
	RawMesh.FaceMaterialIndices.AddZeroed(TriangleCount);
	RawMesh.FaceSmoothingMasks.AddZeroed(TriangleCount);
	RawMesh.WedgeIndices.AddZeroed(WedgeCount);

	if (bHasNTBInformation || RawMesh.WedgeTangentX.Num() > 0 || RawMesh.WedgeTangentY.Num() > 0)
	{
		RawMesh.WedgeTangentX.AddZeroed(WedgeOffset + WedgeCount - RawMesh.WedgeTangentX.Num());
		RawMesh.WedgeTangentY.AddZeroed(WedgeOffset + WedgeCount - RawMesh.WedgeTangentY.Num());
	}

	if (LayerElementNormal || RawMesh.WedgeTangentZ.Num() > 0 )
	{
		RawMesh.WedgeTangentZ.AddZeroed(WedgeOffset + WedgeCount - RawMesh.WedgeTangentZ.Num());
	}

	if (LayerElementVertexColor || VertexColorImportOption != EVertexColorImportOption::Replace || RawMesh.WedgeColors.Num() )
	{
		int32 NumNewColors = WedgeOffset + WedgeCount - RawMesh.WedgeColors.Num();
		int32 FirstNewColor = RawMesh.WedgeColors.Num();
		RawMesh.WedgeColors.AddUninitialized(NumNewColors);
		for (int32 WedgeIndex = FirstNewColor; WedgeIndex < FirstNewColor + NumNewColors; ++WedgeIndex)
		{
			RawMesh.WedgeColors[WedgeIndex] = FColor::White;
		}
	}

	// When importing multiple mesh pieces to the same static mesh.  Ensure each mesh piece has the same number of Uv's
	int32 ExistingUVCount = 0;
	for( int32 ExistingUVIndex = 0; ExistingUVIndex < MAX_MESH_TEXTURE_COORDS; ++ExistingUVIndex )
	{
		if( RawMesh.WedgeTexCoords[ExistingUVIndex].Num() > 0 )
		{
			// Mesh already has UVs at this index
			++ExistingUVCount;
		}
		else
		{
			// No more UVs
			break;
		}
	}
	
	int32 UVCount = FMath::Max( FBXUVs.UniqueUVCount, ExistingUVCount );

	// At least one UV set must exist.  
	UVCount = FMath::Max( 1, UVCount );

	for (int32 UVLayerIndex = 0; UVLayerIndex<UVCount; UVLayerIndex++)
	{
		RawMesh.WedgeTexCoords[UVLayerIndex].AddZeroed(WedgeOffset + WedgeCount - RawMesh.WedgeTexCoords[UVLayerIndex].Num());
	}

	int32 TriangleIndex;
	TMap<int32,int32> IndexMap;
	bool bHasNonDegenerateTriangles = false;

	for( TriangleIndex = 0 ; TriangleIndex < TriangleCount ; TriangleIndex++ )
	{
		int32 DestTriangleIndex = TriangleOffset + TriangleIndex;
		FVector CornerPositions[3];

		for ( int32 CornerIndex=0; CornerIndex<3; CornerIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			int32 WedgeIndex = WedgeOffset + TriangleIndex * 3 + (OddNegativeScale ? 2 - CornerIndex : CornerIndex);

			// Store vertex index and position.
			int32 ControlPointIndex = Mesh->GetPolygonVertex(TriangleIndex, CornerIndex);
			int32* ExistingIndex = IndexMap.Find(ControlPointIndex);
			if (ExistingIndex)
			{
				RawMesh.WedgeIndices[WedgeIndex] = *ExistingIndex;
				CornerPositions[CornerIndex] = RawMesh.VertexPositions[*ExistingIndex];
			}
			else
			{
				FbxVector4 FbxPosition = Mesh->GetControlPoints()[ControlPointIndex];
				FbxVector4 FinalPosition = TotalMatrix.MultT(FbxPosition);
				int32 VertexIndex = RawMesh.VertexPositions.Add(Converter.ConvertPos(FinalPosition));
				RawMesh.WedgeIndices[WedgeIndex] = VertexIndex;
				IndexMap.Add(ControlPointIndex, VertexIndex);
				CornerPositions[CornerIndex] = RawMesh.VertexPositions[VertexIndex];
			}

			//
			// normals, tangents and binormals
			//
			if (LayerElementNormal)
			{
				int TriangleCornerIndex = TriangleIndex*3 + CornerIndex;
				//normals may have different reference and mapping mode than tangents and binormals
				int NormalMapIndex = (NormalMappingMode == FbxLayerElement::eByControlPoint) ? 
					ControlPointIndex : TriangleCornerIndex;
				int NormalValueIndex = (NormalReferenceMode == FbxLayerElement::eDirect) ? 
					NormalMapIndex : LayerElementNormal->GetIndexArray().GetAt(NormalMapIndex);

				//tangents and binormals share the same reference, mapping mode and index array
				if (bHasNTBInformation)
				{
					int TangentMapIndex = (TangentMappingMode == FbxLayerElement::eByControlPoint) ? 
						ControlPointIndex : TriangleCornerIndex;
					int TangentValueIndex = (TangentReferenceMode == FbxLayerElement::eDirect) ? 
						TangentMapIndex : LayerElementTangent->GetIndexArray().GetAt(TangentMapIndex);

					FbxVector4 TempValue = LayerElementTangent->GetDirectArray().GetAt(TangentValueIndex);
					TempValue = TotalMatrixForNormal.MultT(TempValue);
					FVector TangentX = Converter.ConvertDir(TempValue);
					RawMesh.WedgeTangentX[WedgeIndex] = TangentX.GetSafeNormal();

					int BinormalMapIndex = (BinormalMappingMode == FbxLayerElement::eByControlPoint) ? 
						ControlPointIndex : TriangleCornerIndex;
					int BinormalValueIndex = (BinormalReferenceMode == FbxLayerElement::eDirect) ? 
						BinormalMapIndex : LayerElementBinormal->GetIndexArray().GetAt(BinormalMapIndex);

					TempValue = LayerElementBinormal->GetDirectArray().GetAt(BinormalValueIndex);
					TempValue = TotalMatrixForNormal.MultT(TempValue);
					FVector TangentY = -Converter.ConvertDir(TempValue);
					RawMesh.WedgeTangentY[WedgeIndex] = TangentY.GetSafeNormal();
				}

				FbxVector4 TempValue = LayerElementNormal->GetDirectArray().GetAt(NormalValueIndex);
				TempValue = TotalMatrixForNormal.MultT(TempValue);
				FVector TangentZ = Converter.ConvertDir(TempValue);
				RawMesh.WedgeTangentZ[WedgeIndex] = TangentZ.GetSafeNormal();
			}

			//
			// vertex colors
			//
			if (VertexColorImportOption == EVertexColorImportOption::Replace)
			{
				if (LayerElementVertexColor)
				{
					int32 VertexColorMappingIndex = (VertexColorMappingMode == FbxLayerElement::eByControlPoint) ?
						Mesh->GetPolygonVertex(TriangleIndex, CornerIndex) : (TriangleIndex * 3 + CornerIndex);

					int32 VectorColorIndex = (VertexColorReferenceMode == FbxLayerElement::eDirect) ?
					VertexColorMappingIndex : LayerElementVertexColor->GetIndexArray().GetAt(VertexColorMappingIndex);

					FbxColor VertexColor = LayerElementVertexColor->GetDirectArray().GetAt(VectorColorIndex);

					RawMesh.WedgeColors[WedgeIndex] = FColor(
						uint8(255.f*VertexColor.mRed),
						uint8(255.f*VertexColor.mGreen),
						uint8(255.f*VertexColor.mBlue),
						uint8(255.f*VertexColor.mAlpha)
						);
				}
			}
			else if (VertexColorImportOption == EVertexColorImportOption::Ignore)
			{
				// try to match this triangles current vertex with one that existed in the previous mesh.
				// This is a find in a tmap which uses a fast hash table lookup.
				FVector Position = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIndex]];
				const FColor* PaintedColor = ExistingVertexColorData.Find(Position);
				if (PaintedColor)
				{
					// A matching color for this vertex was found
					RawMesh.WedgeColors[WedgeIndex] = *PaintedColor;
				}
			}
			else
			{
				// set the triangle's vertex color to a constant override
				check(VertexColorImportOption == EVertexColorImportOption::Override);
				RawMesh.WedgeColors[WedgeIndex] = VertexOverrideColor;
			}
		}

		// Check if the triangle just discovered is non-degenerate if we haven't found one yet
		if (!bHasNonDegenerateTriangles)
		{
			float ComparisonThreshold = ImportOptions->bRemoveDegenerates ? THRESH_POINTS_ARE_SAME : 0.0f;

			if (!(CornerPositions[0].Equals(CornerPositions[1], ComparisonThreshold)
				  || CornerPositions[0].Equals(CornerPositions[2], ComparisonThreshold)
				  || CornerPositions[1].Equals(CornerPositions[2], ComparisonThreshold)))
			{
				bHasNonDegenerateTriangles = true;
			}
		}

		//
		// smoothing mask
		//
		if (bSmoothingAvailable && SmoothingInfo)
		{
			if (SmoothingMappingMode == FbxLayerElement::eByPolygon)
			{
				int lSmoothingIndex = (SmoothingReferenceMode == FbxLayerElement::eDirect) ? TriangleIndex : SmoothingInfo->GetIndexArray().GetAt(TriangleIndex);
				RawMesh.FaceSmoothingMasks[DestTriangleIndex] = SmoothingInfo->GetDirectArray().GetAt(lSmoothingIndex);
			}
			else
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_UnsupportedSmoothingGroup", "Unsupported Smoothing group mapping mode on mesh  '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_UnsupportingSmoothingGroup);
			}
		}

		//
		// uvs
		//
		// In FBX file, the same UV may be saved multiple times, i.e., there may be same UV in LayerElementUV
		// So we don't import the duplicate UVs
		int32 UVLayerIndex;
		for (UVLayerIndex = 0; UVLayerIndex<FBXUVs.UniqueUVCount; UVLayerIndex++)
		{
			if (FBXUVs.LayerElementUV[UVLayerIndex] != NULL) 
			{
				for (int32 CornerIndex=0;CornerIndex<3;CornerIndex++)
				{
					// If there are odd number negative scale, invert the vertex order for triangles
					int32 WedgeIndex = WedgeOffset + TriangleIndex * 3 + (OddNegativeScale ? 2 - CornerIndex : CornerIndex);


					int lControlPointIndex = Mesh->GetPolygonVertex(TriangleIndex, CornerIndex);
					int UVMapIndex = (FBXUVs.UVMappingMode[UVLayerIndex] == FbxLayerElement::eByControlPoint) ? lControlPointIndex : TriangleIndex*3+CornerIndex;
					int32 UVIndex = (FBXUVs.UVReferenceMode[UVLayerIndex] == FbxLayerElement::eDirect) ? 
						UVMapIndex : FBXUVs.LayerElementUV[UVLayerIndex]->GetIndexArray().GetAt(UVMapIndex);

					FbxVector2	UVVector = FBXUVs.LayerElementUV[UVLayerIndex]->GetDirectArray().GetAt(UVIndex);

					RawMesh.WedgeTexCoords[UVLayerIndex][WedgeIndex].X = static_cast<float>(UVVector[0]);
					RawMesh.WedgeTexCoords[UVLayerIndex][WedgeIndex].Y = 1.f-static_cast<float>(UVVector[1]);   //flip the Y of UVs for DirectX
				}
			}
		}

		//
		// material index
		//
		int32 MaterialIndex = 0;
		if (MaterialCount>0)
		{
			if (LayerElementMaterial)
			{
				switch(MaterialMappingMode)
				{
					// material index is stored in the IndexArray, not the DirectArray (which is irrelevant with 2009.1)
				case FbxLayerElement::eAllSame:
					{	
						MaterialIndex = LayerElementMaterial->GetIndexArray().GetAt(0);
					}
					break;
				case FbxLayerElement::eByPolygon:
					{	
						MaterialIndex = LayerElementMaterial->GetIndexArray().GetAt(TriangleIndex);
					}
					break;
				}
			}
		}
		MaterialIndex += MaterialIndexOffset;

		if (MaterialIndex >= MaterialCount + MaterialIndexOffset || MaterialIndex < 0)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("Error_MaterialIndexInconsistency", "Face material index inconsistency - forcing to 0")), FFbxErrors::Generic_Mesh_MaterialIndexInconsistency);
			MaterialIndex = 0;
		}
	
		RawMesh.FaceMaterialIndices[DestTriangleIndex] = MaterialIndex;
	}
	
	// needed?
	FBXUVs.Cleanup();

	if (!bHasNonDegenerateTriangles)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("MeshName"), FText::FromString(StaticMesh->GetName()));
		FText ErrorMsg = LOCTEXT("MeshHasNoRenderableTriangles", "{MeshName} could not be created because all of its triangles are degenerate.");
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(ErrorMsg, Arguments)), FFbxErrors::StaticMesh_AllTrianglesDegenerate);
	}

	bool bIsValidMesh = bHasNonDegenerateTriangles;

	return bIsValidMesh;
}

UStaticMesh* UnFbx::FFbxImporter::ReimportSceneStaticMesh(uint64 FbxNodeUniqueId, uint64 FbxUniqueId, UStaticMesh* Mesh, UFbxStaticMeshImportData* TemplateImportData)
{
	TArray<FbxNode*> FbxMeshArray;
	UStaticMesh* FirstBaseMesh = NULL;
	FbxNode* Node = NULL;

	// get meshes in Fbx file
	//the function also fill the collision models, so we can update collision models correctly
	FillFbxMeshArray(Scene->GetRootNode(), FbxMeshArray, this);

	if (FbxMeshArray.Num() < 1)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_NoFBXMeshAttributeFound", "No FBX attribute mesh found when reimport scene static mesh '{0}'. The FBX file contain no static mesh."), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_MeshNotFound);
		return Mesh;
	}
	else
	{
		//Find the first node using the mesh attribute with the unique ID
		for (FbxNode *MeshNode : FbxMeshArray)
		{
			if (FbxNodeUniqueId == INVALID_UNIQUE_ID || ImportOptions->bBakePivotInVertex == false)
			{
				if (FbxUniqueId == MeshNode->GetMesh()->GetUniqueID())
				{
					Node = MeshNode;
					break;
				}
			}
			else
			{
				if (FbxNodeUniqueId == MeshNode->GetUniqueID() && FbxUniqueId == MeshNode->GetMesh()->GetUniqueID())
				{
					Node = MeshNode;
					break;
				}
			}
		}
	}

	if (!Node)
	{
		//Cannot find the staticmesh name in the fbx scene file
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_NoFBXMeshNameFound", "No FBX attribute mesh with the same name was found when reimport scene static mesh '{0}'."), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_MeshNotFound);
		return Mesh;
	}

	struct ExistingStaticMeshData* ExistMeshDataPtr = SaveExistingStaticMeshData(Mesh, ImportOptions, INDEX_NONE);

	if (Node)
	{
		FbxNode* NodeParent = RecursiveFindParentLodGroup(Node->GetParent());

		// if the Fbx mesh is a part of LODGroup, update LOD
		if (NodeParent && NodeParent->GetNodeAttribute() && NodeParent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
		{
			TArray<UStaticMesh*> BaseMeshes;
			TArray<FbxNode*> AllNodeInLod;
			FindAllLODGroupNode(AllNodeInLod, NodeParent, 0);
			FirstBaseMesh = ImportStaticMeshAsSingle(Mesh->GetOutermost(), AllNodeInLod, *Mesh->GetName(), RF_Public | RF_Standalone, TemplateImportData, Mesh, 0, ExistMeshDataPtr);
			//If we have a valid LOD group name we don't want to re-import LODs since they will be automatically generate by the LODGroup reduce settings
			if(FirstBaseMesh && Mesh->LODGroup == NAME_None)
			{
				// import LOD meshes
				for (int32 LODIndex = 1; LODIndex < NodeParent->GetChildCount(); LODIndex++)
				{
					AllNodeInLod.Empty();
					FindAllLODGroupNode(AllNodeInLod, NodeParent, LODIndex);
					//For LOD we don't pass the ExistMeshDataPtr
					ImportStaticMeshAsSingle(Mesh->GetOutermost(), AllNodeInLod, *Mesh->GetName(), RF_Public | RF_Standalone, TemplateImportData, FirstBaseMesh, LODIndex, nullptr);
				}
			}
			if (FirstBaseMesh != nullptr)
			{
				FindAllLODGroupNode(AllNodeInLod, NodeParent, 0);
				PostImportStaticMesh(FirstBaseMesh, AllNodeInLod);
			}
		}
		else
		{
			FirstBaseMesh = ImportStaticMesh(Mesh->GetOutermost(), Node, *Mesh->GetName(), RF_Public | RF_Standalone, TemplateImportData, Mesh, 0, ExistMeshDataPtr);
			if (FirstBaseMesh != nullptr)
			{
				TArray<FbxNode*> AllNodeInLod;
				AllNodeInLod.Add(Node);
				PostImportStaticMesh(FirstBaseMesh, AllNodeInLod);
			}
		}
	}
	else
	{
		// no FBX mesh match, maybe the Unreal mesh is imported from multiple FBX mesh (enable option "Import As Single")
		if (FbxMeshArray.Num() > 0)
		{
			FirstBaseMesh = ImportStaticMeshAsSingle(Mesh->GetOutermost(), FbxMeshArray, *Mesh->GetName(), RF_Public | RF_Standalone, TemplateImportData, Mesh, 0, ExistMeshDataPtr);
			if (FirstBaseMesh != nullptr)
			{
				PostImportStaticMesh(FirstBaseMesh, FbxMeshArray);
			}
		}
		else // no mesh found in the FBX file
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_NoFBXMeshFound", "No FBX mesh found when reimport Unreal mesh '{0}'. The FBX file is crashed."), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_MeshNotFound);
		}
	}
	//Don't restore materials when reimporting scene
	RestoreExistingMeshData(ExistMeshDataPtr, FirstBaseMesh, INDEX_NONE, false);
	return FirstBaseMesh;
}


UStaticMesh* UnFbx::FFbxImporter::ReimportStaticMesh(UStaticMesh* Mesh, UFbxStaticMeshImportData* TemplateImportData)
{
	char MeshName[1024];
	FCStringAnsi::Strcpy(MeshName,1024,TCHAR_TO_UTF8(*Mesh->GetName()));
	TArray<FbxNode*> FbxMeshArray;
	FbxNode* Node = NULL;
	UStaticMesh* NewMesh = NULL;

	// get meshes in Fbx file
	//the function also fill the collision models, so we can update collision models correctly
	FillFbxMeshArray(Scene->GetRootNode(), FbxMeshArray, this);
	
	// if there is only one mesh, use it without name checking 
	// (because the "Used As Full Name" option enables users name the Unreal mesh by themselves
	if (FbxMeshArray.Num() == 1)
	{
		Node = FbxMeshArray[0];
	}
	else if(!ImportOptions->bCombineToSingle)
	{
		// find the Fbx mesh node that the Unreal Mesh matches according to name
		int32 MeshIndex;
		for ( MeshIndex = 0; MeshIndex < FbxMeshArray.Num(); MeshIndex++ )
		{
			const char* FbxMeshName = FbxMeshArray[MeshIndex]->GetName();
			// The name of Unreal mesh may have a prefix, so we match from end
			int32 i = 0;
			char* MeshPtr = MeshName + FCStringAnsi::Strlen(MeshName) - 1;
			if (FCStringAnsi::Strlen(FbxMeshName) <= FCStringAnsi::Strlen(MeshName))
			{
				const char* FbxMeshPtr = FbxMeshName + FCStringAnsi::Strlen(FbxMeshName) - 1;
				while (i < FCStringAnsi::Strlen(FbxMeshName))
				{
					if (*MeshPtr != *FbxMeshPtr)
					{
						break;
					}
					else
					{
						i++;
						MeshPtr--;
						FbxMeshPtr--;
					}
				}
			}

			if (i == FCStringAnsi::Strlen(FbxMeshName)) // matched
			{
				// check further
				if ( FCStringAnsi::Strlen(FbxMeshName) == FCStringAnsi::Strlen(MeshName) || // the name of Unreal mesh is full match
					*MeshPtr == '_')														// or the name of Unreal mesh has a prefix
				{
					Node = FbxMeshArray[MeshIndex];
					break;
				}
			}
		}
	}

	// If there is no match it may be because an LOD group was imported where
	// the mesh name does not match the file name. This is actually the common case.
	if (!Node && FbxMeshArray.IsValidIndex(0))
	{
		FbxNode* BaseLODNode = FbxMeshArray[0];
		
		FbxNode* NodeParent = BaseLODNode ? RecursiveFindParentLodGroup(BaseLODNode->GetParent()) : nullptr;
		if (NodeParent && NodeParent->GetNodeAttribute() && NodeParent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
		{
			// Reimport the entire LOD chain.
			Node = BaseLODNode;
		}
	}
	
	ImportOptions->bImportMaterials = false;
	ImportOptions->bImportTextures = false;

	struct ExistingStaticMeshData* ExistMeshDataPtr = SaveExistingStaticMeshData(Mesh, ImportOptions, INDEX_NONE);

	TArray<int32> ReimportLodList;
	if (Node)
	{
		FbxNode* NodeParent = RecursiveFindParentLodGroup(Node->GetParent());

		TArray<FbxNode*> LodZeroNodes;
		// if the Fbx mesh is a part of LODGroup, update LOD
		if (NodeParent && NodeParent->GetNodeAttribute() && NodeParent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
		{
			TArray<FbxNode*> AllNodeInLod;
			FindAllLODGroupNode(AllNodeInLod, NodeParent, 0);
			if (AllNodeInLod.Num() > 0)
			{
				LodZeroNodes = AllNodeInLod;
				NewMesh = ImportStaticMeshAsSingle(Mesh->GetOuter(), AllNodeInLod, *Mesh->GetName(), RF_Public | RF_Standalone, TemplateImportData, Mesh, 0, ExistMeshDataPtr);
				ReimportLodList.Add(0);
			}

			//If we have a valid LOD group name we don't want to re-import LODs since they will be automatically generate by the LODGroup reduce settings
			if (NewMesh && ImportOptions->bImportStaticMeshLODs && Mesh->LODGroup == NAME_None)
			{
				// import LOD meshes
				for (int32 LODIndex = 1; LODIndex < NodeParent->GetChildCount(); LODIndex++)
				{
					AllNodeInLod.Empty();
					FindAllLODGroupNode(AllNodeInLod, NodeParent, LODIndex);
					if (AllNodeInLod.Num() > 0)
					{
						//For LOD we don't pass the ExistMeshDataPtr
						ImportStaticMeshAsSingle(Mesh->GetOuter(), AllNodeInLod, *Mesh->GetName(), RF_Public | RF_Standalone, TemplateImportData, NewMesh, LODIndex, nullptr);
						ReimportLodList.Add(LODIndex);
					}
				}
			}
		}
		else
		{
			LodZeroNodes.Add(Node);
			NewMesh = ImportStaticMesh(Mesh->GetOuter(), Node, *Mesh->GetName(), RF_Public|RF_Standalone, TemplateImportData, Mesh, 0, ExistMeshDataPtr);
			ReimportLodList.Add(0);
		}

		if (NewMesh != nullptr)
		{
			PostImportStaticMesh(NewMesh, LodZeroNodes);
		}
		
	}
	else
	{
		// no FBX mesh match, maybe the Unreal mesh is imported from multiple FBX mesh (enable option "Import As Single")
		if (FbxMeshArray.Num() > 0)
		{
			NewMesh = ImportStaticMeshAsSingle(Mesh->GetOuter(), FbxMeshArray, *Mesh->GetName(), RF_Public|RF_Standalone, TemplateImportData, Mesh, 0, ExistMeshDataPtr);
			ReimportLodList.Add(0);
			if (NewMesh != nullptr)
			{
				PostImportStaticMesh(NewMesh, FbxMeshArray);
			}
		}
		else // no mesh found in the FBX file
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_NoFBXMeshFound", "No FBX mesh found when reimport Unreal mesh '{0}'. The FBX file is crashed."), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_MeshNotFound);
		}
	}
	if (NewMesh != nullptr)
	{
		UpdateSomeLodsImportMeshData(NewMesh, &ReimportLodList);
		RestoreExistingMeshData(ExistMeshDataPtr, NewMesh, INDEX_NONE, ImportOptions->bResetMaterialSlots);
	}
	return NewMesh;
}

void UnFbx::FFbxImporter::VerifyGeometry(UStaticMesh* StaticMesh)
{
	// Calculate bounding box to check if too small
	{
		FVector Center, Extents;
		ComputeBoundingBox(StaticMesh, Center, Extents);

		if (Extents.GetAbsMax() < 5.f)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("Prompt_MeshVerySmall", "Warning: The imported mesh is very small. This is most likely an issue with the units used when exporting to FBX.")), FFbxErrors::Generic_Mesh_SmallGeometry);
		}
	}
}

UStaticMesh* UnFbx::FFbxImporter::ImportStaticMeshAsSingle(UObject* InParent, TArray<FbxNode*>& MeshNodeArray, const FName InName, EObjectFlags Flags, UFbxStaticMeshImportData* TemplateImportData, UStaticMesh* InStaticMesh, int LODIndex, void *ExistMeshDataPtr)
{
	struct ExistingStaticMeshData* ExistMeshData = (struct ExistingStaticMeshData*)ExistMeshDataPtr;
	bool bBuildStatus = true;

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	if (MeshNodeArray.Num() == 0)
	{
		return NULL;
	}
	
	// Count the number of verts
	int32 NumVerts = 0;
	int32 MeshIndex;
	for (MeshIndex = 0; MeshIndex < MeshNodeArray.Num(); MeshIndex++ )
	{
		FbxNode* Node = MeshNodeArray[MeshIndex];
		FbxMesh* FbxMesh = Node->GetMesh();

		if (FbxMesh)
		{
			NumVerts += FbxMesh->GetControlPointsCount();

			// If not combining meshes, reset the vert count between meshes
			if (!ImportOptions->bCombineToSingle)
			{
				NumVerts = 0;
			}
		}
	}

	Parent = InParent;
	
	FString MeshName = ObjectTools::SanitizeObjectName(InName.ToString());

	// warning for missing smoothing group info
	CheckSmoothingInfo(MeshNodeArray[0]->GetMesh());

	// Parent package to place new meshes
	UPackage* Package = NULL;
	if (ImportOptions->bImportScene && InParent != nullptr && InParent->IsA(UPackage::StaticClass()))
	{
		Package = StaticCast<UPackage*>(InParent);
	}

	// create empty mesh
	UStaticMesh*	StaticMesh = NULL;

	UStaticMesh* ExistingMesh = NULL;
	UObject* ExistingObject = NULL;

	// A mapping of vertex positions to their color in the existing static mesh
	TMap<FVector, FColor>		ExistingVertexColorData;

	EVertexColorImportOption::Type VertexColorImportOption = ImportOptions->VertexColorImportOption;
	FString NewPackageName;

	if( InStaticMesh == NULL || LODIndex == 0 )
	{
		// Create a package for each mesh
		if (Package == nullptr)
		{
			NewPackageName = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName()) + TEXT("/") + MeshName;
			NewPackageName = PackageTools::SanitizePackageName(NewPackageName);
			Package = CreatePackage(NULL, *NewPackageName);
		}
		Package->FullyLoad();

		ExistingMesh = FindObject<UStaticMesh>( Package, *MeshName );
		ExistingObject = FindObject<UObject>( Package, *MeshName );		
	}

	if (ExistingMesh)
	{
		ExistingMesh->GetVertexColorData(ExistingVertexColorData);

		if (0 == ExistingVertexColorData.Num())
		{
			// If there were no vertex colors and we specified to ignore FBX vertex colors, automatically take vertex colors from the file anyway.
			if (VertexColorImportOption == EVertexColorImportOption::Ignore)
			{
				VertexColorImportOption = EVertexColorImportOption::Replace;
			}
		}

		// Free any RHI resources for existing mesh before we re-create in place.
		ExistingMesh->PreEditChange(NULL);
	}
	else if (ExistingObject)
	{
		// Replacing an object.  Here we go!
		// Delete the existing object
		bool bDeleteSucceeded = ObjectTools::DeleteSingleObject( ExistingObject );

		if (bDeleteSucceeded)
		{
			// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

			// Create a package for each mesh
			Package = CreatePackage(NULL, *NewPackageName);

			// Require the parent because it will have been invalidated from the garbage collection
			Parent = Package;
		}
		else
		{
			// failed to delete
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("ContentBrowser_CannotDeleteReferenced", "{0} wasn't created.\n\nThe asset is referenced by other content."), FText::FromString(MeshName))), FFbxErrors::Generic_CannotDeleteReferenced);
			return NULL;
		}

		// Vertex colors should be copied always if there is no existing static mesh.
		if (VertexColorImportOption == EVertexColorImportOption::Ignore)
		{
			VertexColorImportOption = EVertexColorImportOption::Replace;
		}
	}
	else
	{ 
		// Vertex colors should be copied always if there is no existing static mesh.
		if (VertexColorImportOption == EVertexColorImportOption::Ignore)
		{
			VertexColorImportOption = EVertexColorImportOption::Replace;
		}
	}
	
	if( InStaticMesh != NULL && LODIndex > 0 )
	{
		StaticMesh = InStaticMesh;
	}
	else
	{
		StaticMesh = NewObject<UStaticMesh>(Package, FName(*MeshName), Flags | RF_Public);
	}

	if (StaticMesh->SourceModels.Num() < LODIndex+1)
	{
		// Add one LOD 
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
		
		if (StaticMesh->SourceModels.Num() < LODIndex+1)
		{
			LODIndex = StaticMesh->SourceModels.Num() - 1;
		}
	}
	
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
	if( InStaticMesh != NULL && LODIndex > 0 && !SrcModel.RawMeshBulkData->IsEmpty() )
	{
		// clear out the old mesh data
		FRawMesh EmptyRawMesh;
		SrcModel.RawMeshBulkData->SaveRawMesh( EmptyRawMesh );
	}
	
	// make sure it has a new lighting guid
	StaticMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
	StaticMesh->LightMapResolution = 64;
	StaticMesh->LightMapCoordinateIndex = 1;


	FRawMesh NewRawMesh;
	SrcModel.RawMeshBulkData->LoadRawMesh(NewRawMesh);

	TArray<FFbxMaterial> MeshMaterials;
	for (MeshIndex = 0; MeshIndex < MeshNodeArray.Num(); MeshIndex++ )
	{
		FbxNode* Node = MeshNodeArray[MeshIndex];

		if (Node->GetMesh())
		{
			if (!BuildStaticMeshFromGeometry(Node, StaticMesh, MeshMaterials, LODIndex, NewRawMesh,
											 VertexColorImportOption, ExistingVertexColorData, ImportOptions->VertexOverrideColor))
			{
				bBuildStatus = false;
				break;
			}
		}
	}

	// Store the new raw mesh.
	SrcModel.RawMeshBulkData->SaveRawMesh(NewRawMesh);


	if (bBuildStatus)
	{
		UE_LOG(LogFbx, Verbose, TEXT("== Initial material list:"));
		for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); ++MaterialIndex)
		{
			UE_LOG(LogFbx, Verbose, TEXT("%d: %s"), MaterialIndex, *MeshMaterials[MaterialIndex].GetName());
		}

		// Compress the materials array by removing any duplicates.
		bool bDoRemap = false;
		TArray<int32> MaterialMap;
		TArray<FFbxMaterial> UniqueMaterials;
		for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); ++MaterialIndex)
		{
			bool bUnique = true;
			for (int32 OtherMaterialIndex = MaterialIndex - 1; OtherMaterialIndex >= 0; --OtherMaterialIndex)
			{
				if (MeshMaterials[MaterialIndex].FbxMaterial == MeshMaterials[OtherMaterialIndex].FbxMaterial &&
					MeshMaterials[MaterialIndex].Material == MeshMaterials[OtherMaterialIndex].Material)
				{
					int32 UniqueIndex = MaterialMap[OtherMaterialIndex];

					MaterialMap.Add(UniqueIndex);
					bDoRemap = true;
					bUnique = false;
					break;
				}
			}
			if (bUnique)
			{
				int32 UniqueIndex = UniqueMaterials.Add(MeshMaterials[MaterialIndex]);

				MaterialMap.Add(UniqueIndex);
			}
			else
			{
				UE_LOG(LogFbx, Verbose, TEXT("  remap %d -> %d"), MaterialIndex, MaterialMap[MaterialIndex]);
			}
		}

		if (UniqueMaterials.Num() > LARGE_MESH_MATERIAL_INDEX_THRESHOLD)
		{
			AddTokenizedErrorMessage(
				FTokenizedMessage::Create(
					EMessageSeverity::Warning,
					FText::Format(LOCTEXT("Error_TooManyMaterials", "StaticMesh has a large number({0}) of materials and may render inefficently.  Consider breaking up the mesh into multiple Static Mesh Assets"),
						FText::AsNumber(UniqueMaterials.Num())
						)),
				FFbxErrors::StaticMesh_TooManyMaterials);
		}

		//The fix is required for blender file. The static mesh build have change and now required that
		//the sections (face declaration) must be declare in the same order as the material index
		TArray<uint32> SortedMaterialIndex;
		TArray<int32> UsedMaterials;
		for (int32 FaceMaterialIndex = 0; FaceMaterialIndex < NewRawMesh.FaceMaterialIndices.Num(); ++FaceMaterialIndex)
		{
			int32 MaterialIndex = NewRawMesh.FaceMaterialIndices[FaceMaterialIndex];
			if (!UsedMaterials.Contains(MaterialIndex))
			{
				int32 NewIndex = UsedMaterials.Add(MaterialIndex);
				if (NewIndex != MaterialIndex)
				{
					bDoRemap = true;
				}
			}
		}

		for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterials.Num(); ++MaterialIndex)
		{
			int32 SkinIndex = 0xffff;
			if (bDoRemap)
			{
				int32 UsedIndex = 0;
				if (UsedMaterials.Find(MaterialIndex, UsedIndex))
				{
					SkinIndex = UsedIndex;
				}
			}
			int32 RemappedIndex = MaterialMap[MaterialIndex];
			uint32 SortedMaterialKey = ((uint32)SkinIndex << 16) | ((uint32)RemappedIndex & 0xffff);
			if (!SortedMaterialIndex.IsValidIndex(SortedMaterialKey))
			{
				SortedMaterialIndex.Add(SortedMaterialKey);
			}
		}

		SortedMaterialIndex.Sort();


		UE_LOG(LogFbx, Verbose, TEXT("== After sorting:"));
		TArray<FFbxMaterial> SortedMaterials;
		for (int32 SortedIndex = 0; SortedIndex < SortedMaterialIndex.Num(); ++SortedIndex)
		{
			int32 RemappedIndex = SortedMaterialIndex[SortedIndex] & 0xffff;
			SortedMaterials.Add(UniqueMaterials[RemappedIndex]);
			UE_LOG(LogFbx, Verbose, TEXT("%d: %s"), SortedIndex, *UniqueMaterials[RemappedIndex].GetName());
		}
		UE_LOG(LogFbx, Verbose, TEXT("== Mapping table:"));
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialMap.Num(); ++MaterialIndex)
		{
			for (int32 SortedIndex = 0; SortedIndex < SortedMaterialIndex.Num(); ++SortedIndex)
			{
				int32 RemappedIndex = SortedMaterialIndex[SortedIndex] & 0xffff;
				if (MaterialMap[MaterialIndex] == RemappedIndex)
				{
					UE_LOG(LogFbx, Verbose, TEXT("  sort %d -> %d"), MaterialIndex, SortedIndex);
					MaterialMap[MaterialIndex] = SortedIndex;
					break;
				}
			}
		}

		// Remap material indices.
		int32 MaxMaterialIndex = 0;
		int32 FirstOpenUVChannel = 1;
		{
			FRawMesh LocalRawMesh;
			SrcModel.RawMeshBulkData->LoadRawMesh(LocalRawMesh);

			if (bDoRemap)
			{
				for (int32 TriIndex = 0; TriIndex < LocalRawMesh.FaceMaterialIndices.Num(); ++TriIndex)
				{
					LocalRawMesh.FaceMaterialIndices[TriIndex] = MaterialMap[LocalRawMesh.FaceMaterialIndices[TriIndex]];
				}
			}

			// Compact material indices so that we won't have any sections with zero triangles.
			LocalRawMesh.CompactMaterialIndices();

			// Also compact the sorted materials array.
			if (LocalRawMesh.MaterialIndexToImportIndex.Num() > 0)
			{
				TArray<FFbxMaterial> OldSortedMaterials;

				Exchange(OldSortedMaterials, SortedMaterials);
				SortedMaterials.Empty(LocalRawMesh.MaterialIndexToImportIndex.Num());
				for (int32 MaterialIndex = 0; MaterialIndex < LocalRawMesh.MaterialIndexToImportIndex.Num(); ++MaterialIndex)
				{
					FFbxMaterial Material;
					int32 ImportIndex = LocalRawMesh.MaterialIndexToImportIndex[MaterialIndex];
					if (OldSortedMaterials.IsValidIndex(ImportIndex))
					{
						Material = OldSortedMaterials[ImportIndex];
					}
					SortedMaterials.Add(Material);
				}
			}

			for (int32 TriIndex = 0; TriIndex < LocalRawMesh.FaceMaterialIndices.Num(); ++TriIndex)
			{
				MaxMaterialIndex = FMath::Max<int32>(MaxMaterialIndex, LocalRawMesh.FaceMaterialIndices[TriIndex]);
			}

			for (int32 i = 0; i < MAX_MESH_TEXTURE_COORDS; i++)
			{
				if (LocalRawMesh.WedgeTexCoords[i].Num() == 0)
				{
					FirstOpenUVChannel = i;
					break;
				}
			}

			SrcModel.RawMeshBulkData->SaveRawMesh(LocalRawMesh);
		}

		// Setup per-section info and the materials array.
		if (LODIndex == 0)
		{
			StaticMesh->StaticMaterials.Empty();
		}

		// Replace map of sections with the unique material set
		int32 NumMaterials = FMath::Min(SortedMaterials.Num(), MaxMaterialIndex + 1);
		for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
		{
			FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(LODIndex, MaterialIndex);

			int32 Index = 0;

			FName MaterialFName = FName(*(SortedMaterials[MaterialIndex].GetName()));
			FString CleanMaterialSlotName = MaterialFName.ToString();
			int32 SkinOffset = CleanMaterialSlotName.Find(TEXT("_skin"));
			if (SkinOffset != INDEX_NONE)
			{
				CleanMaterialSlotName = CleanMaterialSlotName.LeftChop(CleanMaterialSlotName.Len() - SkinOffset);
			}

			if (InStaticMesh)
			{
				Index = INDEX_NONE;
				FStaticMaterial StaticMaterialImported(SortedMaterials[MaterialIndex].Material, FName(*CleanMaterialSlotName), MaterialFName);
				for (int32 OriginalMaterialIndex = 0; OriginalMaterialIndex < InStaticMesh->StaticMaterials.Num(); ++OriginalMaterialIndex)
				{
					if (InStaticMesh->StaticMaterials[OriginalMaterialIndex] == StaticMaterialImported)
					{
						Index = OriginalMaterialIndex;
						break;
					}
				}
				if (Index == INDEX_NONE || (Index >= NumMaterials && Index >= InStaticMesh->StaticMaterials.Num()))
				{
					Index = StaticMesh->StaticMaterials.Add(FStaticMaterial(SortedMaterials[MaterialIndex].Material, FName(*CleanMaterialSlotName), MaterialFName));
				}
			}
			else
			{

				Index = StaticMesh->StaticMaterials.Add(FStaticMaterial(SortedMaterials[MaterialIndex].Material, FName(*CleanMaterialSlotName), MaterialFName));
			}

			Info.MaterialIndex = Index;
			StaticMesh->SectionInfoMap.Remove(LODIndex, MaterialIndex);
			StaticMesh->SectionInfoMap.Set(LODIndex, MaterialIndex, Info);
		}

		FRawMesh LocalRawMesh;
		SrcModel.RawMeshBulkData->LoadRawMesh(LocalRawMesh);

		// Setup default LOD settings based on the selected LOD group.
		if (LODIndex == 0)
		{
			ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
			check(CurrentPlatform);
			const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(ImportOptions->StaticMeshLODGroup);
			int32 NumLODs = LODGroup.GetDefaultNumLODs();
			while (StaticMesh->SourceModels.Num() < NumLODs)
			{
				new (StaticMesh->SourceModels) FStaticMeshSourceModel();
			}
			for (int32 ModelLODIndex = 0; ModelLODIndex < NumLODs; ++ModelLODIndex)
			{
				StaticMesh->SourceModels[ModelLODIndex].ReductionSettings = LODGroup.GetDefaultSettings(ModelLODIndex);
			}
			StaticMesh->LightMapResolution = LODGroup.GetDefaultLightMapResolution();
		}

		UFbxStaticMeshImportData* ImportData = UFbxStaticMeshImportData::GetImportDataForStaticMesh(StaticMesh, TemplateImportData);

		//@third party BEGIN SIMPLYGON
		/* ImportData->Update(UFactory::GetCurrentFilename());
		Developer Note: Update method above computed Hash internally. Hash is calculated based on the file size.
		Doing this for CAD files with thousands of components hugely increases the time.
		The following method uses a precomputed hash (once per file). Huge time savings.
		*/
		FString FactoryCurrentFileName = UFactory::GetCurrentFilename();
		if (!FactoryCurrentFileName.IsEmpty() && LODIndex == 0)
		{
			//The factory is instantiate only when importing or re-importing the LOD 0
			//The LOD re-import is not using the factory so the static function UFactory::GetCurrentFilename()
			//will return the last fbx imported asset name or no name if there was no imported asset before.
			ImportData->Update(FactoryCurrentFileName, UFactory::GetFileHash());
		}
		//@third party END SIMPLYGON


		// @todo This overrides restored values currently but we need to be able to import over the existing settings if the user chose to do so.
		SrcModel.BuildSettings.bRemoveDegenerates = ImportOptions->bRemoveDegenerates;
		SrcModel.BuildSettings.bBuildAdjacencyBuffer = ImportOptions->bBuildAdjacencyBuffer;
		SrcModel.BuildSettings.bBuildReversedIndexBuffer = ImportOptions->bBuildReversedIndexBuffer;
		SrcModel.BuildSettings.bRecomputeNormals = ImportOptions->NormalImportMethod == FBXNIM_ComputeNormals;
		SrcModel.BuildSettings.bRecomputeTangents = ImportOptions->NormalImportMethod != FBXNIM_ImportNormalsAndTangents;
		SrcModel.BuildSettings.bUseMikkTSpace = (ImportOptions->NormalGenerationMethod == EFBXNormalGenerationMethod::MikkTSpace) && (!ImportOptions->ShouldImportNormals() || !ImportOptions->ShouldImportTangents());
		if (ImportOptions->bGenerateLightmapUVs)
		{
			SrcModel.BuildSettings.bGenerateLightmapUVs = true;
			SrcModel.BuildSettings.DstLightmapIndex = FirstOpenUVChannel;
			StaticMesh->LightMapCoordinateIndex = FirstOpenUVChannel;
		}
		else
		{
			SrcModel.BuildSettings.bGenerateLightmapUVs = false;
		}

		StaticMesh->LODGroup = ImportOptions->StaticMeshLODGroup;

		//Set the Imported version before calling the build
		//We set it here because the remap index is build in RestoreExistingMeshSettings call before the build
		StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

		if (ExistMeshData && InStaticMesh)
		{
			RestoreExistingMeshSettings(ExistMeshData, InStaticMesh, StaticMesh->LODGroup != NAME_None ? INDEX_NONE : LODIndex);
		}

		

		// The code to check for bad lightmap UVs doesn't scale well with number of triangles.
		// Skip it here because Lightmass will warn about it during a light build anyway.
		bool bWarnOnBadLightmapUVs = false;
		if (bWarnOnBadLightmapUVs)
		{
			TArray< FString > MissingUVSets;
			TArray< FString > BadUVSets;
			TArray< FString > ValidUVSets;
			UStaticMesh::CheckLightMapUVs(StaticMesh, MissingUVSets, BadUVSets, ValidUVSets);

			// NOTE: We don't care about missing UV sets here, just bad ones!
			if (BadUVSets.Num() > 0)
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_UVSetLayoutProblem", "Warning: The light map UV set for static mesh '{0}' appears to have layout problems.  Either the triangle UVs are overlapping one another or the UV are out of bounds (0.0 - 1.0 range.)"), FText::FromString(MeshName))), FFbxErrors::StaticMesh_UVSetLayoutProblem);
			}
		}
	}
	else
	{
		// If we couldn't build the static mesh, its package is invalid. We should reject it entirely to prevent issues from arising from trying to use it in the editor.
		if (!NewPackageName.IsEmpty())
		{
			Package->RemoveFromRoot();
			Package->ConditionalBeginDestroy();
		}

		StaticMesh = NULL;
	}

	if (StaticMesh)
	{
		//warnings based on geometry
		VerifyGeometry(StaticMesh);

		ImportStaticMeshLocalSockets(StaticMesh, MeshNodeArray);
	}

	return StaticMesh;
}

void UnFbx::FFbxImporter::PostImportStaticMesh(UStaticMesh* StaticMesh, TArray<FbxNode*>& MeshNodeArray)
{
	if (StaticMesh == nullptr)
	{
		return;
	}

	// Build the staticmesh, we move the build here because we want to avoid building the staticmesh for every LOD
	// when we import the mesh.
	TArray<FText> BuildErrors;

	//Prebuild the static mesh when we use LodGroup and we want to modify the LodNumber
	if (!ImportOptions->bImportScene)
	{
		//Set the minimum LOD
		if (ImportOptions->MinimumLodNumber > 0)
		{
			StaticMesh->MinLOD = ImportOptions->MinimumLodNumber;
		}

		//User specify a number of LOD.
		if (ImportOptions->LodNumber > 0)
		{
			//In case we plan to change the LodNumber we will build the static mesh 2 time
			//We have to disable the distance field calculation so it get calculated only during the second build
			bool bSpecifiedLodGroup = ImportOptions->StaticMeshLODGroup != NAME_None;
			if (bSpecifiedLodGroup)
			{
				//Avoid building the distance field when we prebuild
				static const auto CVarDistanceField = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
				int32 OriginalCVarDistanceFieldValue = CVarDistanceField->GetValueOnGameThread();
				IConsoleVariable* CVarDistanceFieldInterface = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GenerateMeshDistanceFields"));
				if (OriginalCVarDistanceFieldValue != 0 && CVarDistanceFieldInterface)
				{
					//Hack we change the distance field user console variable to control the build, but we put back the value after the first build
					CVarDistanceFieldInterface->SetWithCurrentPriority(0);
				}
				bool bOriginalGenerateMeshDistanceField = StaticMesh->bGenerateMeshDistanceField;
				StaticMesh->bGenerateMeshDistanceField = false;

				StaticMesh->Build(false, &BuildErrors);
				for (FText& Error : BuildErrors)
				{
					AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, Error), FFbxErrors::StaticMesh_BuildError);
				}

				StaticMesh->bGenerateMeshDistanceField = bOriginalGenerateMeshDistanceField;
				if (OriginalCVarDistanceFieldValue != 0 && CVarDistanceFieldInterface)
				{
					CVarDistanceFieldInterface->SetWithCurrentPriority(OriginalCVarDistanceFieldValue);
				}
			}

			//Set the Number of LODs, this has to be done after we build the specified LOD Group
			int32 LODCount = ImportOptions->LodNumber;
			if (LODCount < 0)
			{
				LODCount = 0;
			}
			if (LODCount > MAX_STATIC_MESH_LODS)
			{
				LODCount = MAX_STATIC_MESH_LODS;
			}

			//Remove extra LODs
			if (StaticMesh->SourceModels.Num() > LODCount)
			{
				int32 NumToRemove = StaticMesh->SourceModels.Num() - LODCount;
				StaticMesh->SourceModels.RemoveAt(LODCount, NumToRemove);
			}
			//Add missing LODs
			while (StaticMesh->SourceModels.Num() < LODCount)
			{
				FStaticMeshSourceModel* SrcModel = new(StaticMesh->SourceModels) FStaticMeshSourceModel();
			}
		}
	}

	StaticMesh->Build(false, &BuildErrors);
	for (FText& Error : BuildErrors)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, Error), FFbxErrors::StaticMesh_BuildError);
	}

	//Set the specified LOD distances for every LODs we have to do this after the build in case there is a specified Lod Group
	if (!ImportOptions->bAutoComputeLodDistances && !ImportOptions->bImportScene)
	{
		StaticMesh->bAutoComputeLODScreenSize = false;

		for (int32 LodIndex = 0; LodIndex < StaticMesh->SourceModels.Num(); ++LodIndex)
		{
			FStaticMeshSourceModel &StaticMeshSourceModel = StaticMesh->SourceModels[LodIndex];
			StaticMeshSourceModel.ScreenSize = ImportOptions->LodDistances.IsValidIndex(LodIndex) ? ImportOptions->LodDistances[LodIndex] : 0.0f;
		}
	}

	// this is damage control. After build, we'd like to absolutely sure that 
	// all index is pointing correctly and they're all used. Otherwise we remove them
	FMeshSectionInfoMap TempOldSectionInfoMap = StaticMesh->SectionInfoMap;
	StaticMesh->SectionInfoMap.Clear();
	StaticMesh->OriginalSectionInfoMap.Clear();
	// fix up section data
	for (int32 LODResoureceIndex = 0; LODResoureceIndex < StaticMesh->RenderData->LODResources.Num(); ++LODResoureceIndex)
	{
		FStaticMeshLODResources& LOD = StaticMesh->RenderData->LODResources[LODResoureceIndex];
		int32 NumSections = LOD.Sections.Num();
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			FMeshSectionInfo Info = TempOldSectionInfoMap.Get(LODResoureceIndex, SectionIndex);
			if (StaticMesh->StaticMaterials.IsValidIndex(Info.MaterialIndex))
			{
				StaticMesh->SectionInfoMap.Set(LODResoureceIndex, SectionIndex, Info);
				StaticMesh->OriginalSectionInfoMap.Set(LODResoureceIndex, SectionIndex, Info);
			}
		}
	}

	//collision generation must be done after the build, this will ensure a valid BodySetup
	if (StaticMesh->bCustomizedCollision == false && ImportOptions->bAutoGenerateCollision && StaticMesh->BodySetup)
	{
		FKAggregateGeom & AggGeom = StaticMesh->BodySetup->AggGeom;
		AggGeom.ConvexElems.Empty(1);	//if no custom collision is setup we just regenerate collision when reimport

		const int32 NumDirs = 18;
		TArray<FVector> Dirs;
		Dirs.AddUninitialized(NumDirs);
		for (int32 DirIdx = 0; DirIdx < NumDirs; ++DirIdx) { Dirs[DirIdx] = KDopDir18[DirIdx]; }
		GenerateKDopAsSimpleCollision(StaticMesh, Dirs);
	}

	//If there is less the 2 materials in the fbx file there is no need to reorder them
	if (StaticMesh->StaticMaterials.Num() < 2)
	{
		return;
	}

	TArray<FString> MeshMaterials;
	for (int32 MeshIndex = 0; MeshIndex < MeshNodeArray.Num(); MeshIndex++)
	{
		FbxNode* Node = MeshNodeArray[MeshIndex];
		if (Node->GetMesh())
		{
			int32 MaterialCount = Node->GetMaterialCount();

			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; MaterialIndex++)
			{
				//Get the original fbx import name
				FbxSurfaceMaterial *FbxMaterial = Node->GetMaterial(MaterialIndex);
				FString FbxMaterialName = FbxMaterial ? ANSI_TO_TCHAR(FbxMaterial->GetName()) : TEXT("None");
				if (!MeshMaterials.Contains(FbxMaterialName))
				{
					MeshMaterials.Add(FbxMaterialName);
				}
			}
		}
	}

	//There is no material in the fbx node
	if (MeshMaterials.Num() < 1)
	{
		return;
	}

	//If there is some skinxx material name we will reorder the material to follow the skinxx workflow instead of the fbx order
	bool IsUsingSkinxxWorkflow = true;
	TArray<FString> MeshMaterialsSkinXX;
	MeshMaterialsSkinXX.AddZeroed(MeshMaterials.Num());
	for (int32 FbxMaterialIndex = 0; FbxMaterialIndex < MeshMaterials.Num(); ++FbxMaterialIndex)
	{
		const FString &FbxMaterialName = MeshMaterials[FbxMaterialIndex];
		//If we have all skinxx material name we have to re-order to skinxx workflow
		int32 Offset = FbxMaterialName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Offset == INDEX_NONE)
		{
			IsUsingSkinxxWorkflow = false;
			MeshMaterialsSkinXX.Empty();
			break;
		}
		int32 SkinIndex = INDEX_NONE;
		// Chop off the material name so we are left with the number in _SKINXX
		FString SkinXXNumber = FbxMaterialName.Right(FbxMaterialName.Len() - (Offset + 1)).RightChop(4);
		if (SkinXXNumber.IsNumeric())
		{
			SkinIndex = FPlatformString::Atoi(*SkinXXNumber);
		}

		if (SkinIndex >= MeshMaterialsSkinXX.Num())
		{
			MeshMaterialsSkinXX.AddZeroed((SkinIndex + 1) - MeshMaterialsSkinXX.Num());
		}
		if (MeshMaterialsSkinXX.IsValidIndex(SkinIndex))
		{
			MeshMaterialsSkinXX[SkinIndex] = FbxMaterialName;
		}
		else
		{
			//Cannot reorder this item
			IsUsingSkinxxWorkflow = false;
			MeshMaterialsSkinXX.Empty();
			break;
		}
	}

	if (IsUsingSkinxxWorkflow)
	{
		//Shrink the array to valid entry, in case the skinxx has some hole like _skin[01, 02, 04, 05...]
		for (int32 FbxMaterialIndex = MeshMaterialsSkinXX.Num() - 1; FbxMaterialIndex >= 0; --FbxMaterialIndex)
		{
			const FString &FbxMaterial = MeshMaterialsSkinXX[FbxMaterialIndex];
			if (FbxMaterial.IsEmpty())
			{
				MeshMaterialsSkinXX.RemoveAt(FbxMaterialIndex);
			}
		}
		//Replace the fbx ordered materials by the skinxx ordered material
		MeshMaterials = MeshMaterialsSkinXX;
	}

	//Reorder the StaticMaterials array to reflect the order in the fbx file
	//So we make sure the order reflect the material ID in the DCCs
	FMeshSectionInfoMap OldSectionInfoMap = StaticMesh->SectionInfoMap;
	TArray<int32> FbxRemapMaterials;
	TArray<FStaticMaterial> NewStaticMaterials;
	for (int32 FbxMaterialIndex = 0; FbxMaterialIndex < MeshMaterials.Num(); ++FbxMaterialIndex)
	{
		const FString &FbxMaterial = MeshMaterials[FbxMaterialIndex];
		int32 FoundMaterialIndex = INDEX_NONE;
		for (int32 BuildMaterialIndex = 0; BuildMaterialIndex < StaticMesh->StaticMaterials.Num(); ++BuildMaterialIndex)
		{
			FStaticMaterial &BuildMaterial = StaticMesh->StaticMaterials[BuildMaterialIndex];
			if (FbxMaterial.Compare(BuildMaterial.ImportedMaterialSlotName.ToString()) == 0)
			{
				FoundMaterialIndex = BuildMaterialIndex;
				break;
			}
		}
		
		if (FoundMaterialIndex != INDEX_NONE)
		{
			FbxRemapMaterials.Add(FoundMaterialIndex);
			NewStaticMaterials.Add(StaticMesh->StaticMaterials[FoundMaterialIndex]);
		}
	}
	//Add the materials not used by the LOD 0 at the end of the array. The order here is irrelevant since it can be used by many LOD other then LOD 0 and in different order
	for (int32 BuildMaterialIndex = 0; BuildMaterialIndex < StaticMesh->StaticMaterials.Num(); ++BuildMaterialIndex)
	{
		const FStaticMaterial &StaticMaterial = StaticMesh->StaticMaterials[BuildMaterialIndex];
		bool bFoundMaterial = false;
		for (const FStaticMaterial &BuildMaterial : NewStaticMaterials)
		{
			if (StaticMaterial == BuildMaterial)
			{
				bFoundMaterial = true;
				break;
			}
		}
		if (!bFoundMaterial)
		{
			FbxRemapMaterials.Add(BuildMaterialIndex);
			NewStaticMaterials.Add(StaticMaterial);
		}
	}

	StaticMesh->StaticMaterials.Empty();
	for (const FStaticMaterial &BuildMaterial : NewStaticMaterials)
	{
		StaticMesh->StaticMaterials.Add(BuildMaterial);
	}

	//Remap the material instance of the staticmaterial array and remap the material index of all sections
	for (int32 LODResoureceIndex = 0; LODResoureceIndex < StaticMesh->RenderData->LODResources.Num(); ++LODResoureceIndex)
	{
		FStaticMeshLODResources& LOD = StaticMesh->RenderData->LODResources[LODResoureceIndex];
		int32 NumSections = LOD.Sections.Num();
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			FMeshSectionInfo Info = OldSectionInfoMap.Get(LODResoureceIndex, SectionIndex);
			int32 RemapIndex = FbxRemapMaterials.Find(Info.MaterialIndex);
			if (StaticMesh->StaticMaterials.IsValidIndex(RemapIndex))
			{
				Info.MaterialIndex = RemapIndex;
				StaticMesh->SectionInfoMap.Set(LODResoureceIndex, SectionIndex, Info);
				StaticMesh->OriginalSectionInfoMap.Set(LODResoureceIndex, SectionIndex, Info);
			}
		}
	}
}

void UnFbx::FFbxImporter::UpdateStaticMeshImportData(UStaticMesh *StaticMesh, UFbxStaticMeshImportData* StaticMeshImportData)
{
	if (StaticMesh == nullptr)
	{
		return;
	}
	UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(StaticMesh->AssetImportData);
	if (!ImportData && StaticMeshImportData)
	{
		ImportData = UFbxStaticMeshImportData::GetImportDataForStaticMesh(StaticMesh, StaticMeshImportData);
	}

	if (ImportData)
	{
		ImportData->ImportMaterialOriginalNameData.Empty();
		ImportData->ImportMeshLodData.Empty();

		for (const FStaticMaterial &Material : StaticMesh->StaticMaterials)
		{
			ImportData->ImportMaterialOriginalNameData.Add(Material.ImportedMaterialSlotName);
		}
		for (int32 LODResoureceIndex = 0; LODResoureceIndex < StaticMesh->RenderData->LODResources.Num(); ++LODResoureceIndex)
		{
			ImportData->ImportMeshLodData.AddZeroed();
			FStaticMeshLODResources& LOD = StaticMesh->RenderData->LODResources[LODResoureceIndex];
			int32 NumSections = LOD.Sections.Num();
			for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				int32 MaterialLodSectionIndex = LOD.Sections[SectionIndex].MaterialIndex;
				if (StaticMesh->SectionInfoMap.GetSectionNumber(LODResoureceIndex) > SectionIndex)
				{
					//In case we have a different ordering then the original fbx order use the sectioninfomap
					const FMeshSectionInfo &SectionInfo = StaticMesh->SectionInfoMap.Get(LODResoureceIndex, SectionIndex);
					MaterialLodSectionIndex = SectionInfo.MaterialIndex;
				}
				if (ImportData->ImportMaterialOriginalNameData.IsValidIndex(MaterialLodSectionIndex))
				{
					ImportData->ImportMeshLodData[LODResoureceIndex].SectionOriginalMaterialName.Add(ImportData->ImportMaterialOriginalNameData[MaterialLodSectionIndex]);
				}
				else
				{
					ImportData->ImportMeshLodData[LODResoureceIndex].SectionOriginalMaterialName.Add(TEXT("InvalidMaterialIndex"));
				}
			}
		}
	}
}

struct FbxSocketNode
{
	FName SocketName;
	FbxNode* Node;
};

static void FindMeshSockets( FbxNode* StartNode, TArray<FbxSocketNode>& OutFbxSocketNodes )
{
	if( !StartNode )
	{
		return;
	}

	static const FString SocketPrefix( TEXT("SOCKET_") );
	if( StartNode->GetNodeAttributeCount() > 0 )
	{
		// Find null attributes, they cold be sockets
		FbxNodeAttribute* Attribute = StartNode->GetNodeAttribute();

		if( Attribute != NULL && Attribute->GetAttributeType() == FbxNodeAttribute::eNull )
		{
			// Is this prefixed correctly? If so it is a socket
			FString SocketName = UTF8_TO_TCHAR( StartNode->GetName() );
			if( SocketName.StartsWith( SocketPrefix ) )
			{
				// Remove the prefix from the name
				SocketName = SocketName.RightChop( SocketPrefix.Len() );

				FbxSocketNode NewNode;
				NewNode.Node = StartNode;
				NewNode.SocketName = *SocketName;
				OutFbxSocketNodes.Add( NewNode );
			}
		}
	}

	// Recursively examine all children
	for ( int32 ChildIndex=0; ChildIndex < StartNode->GetChildCount(); ++ChildIndex )
	{
		FindMeshSockets( StartNode->GetChild(ChildIndex), OutFbxSocketNodes );
	}
}

void UnFbx::FFbxImporter::ImportStaticMeshLocalSockets(UStaticMesh* StaticMesh, TArray<FbxNode*>& MeshNodeArray)
{
	check(MeshNodeArray.Num());
	FbxNode *MeshRootNode = MeshNodeArray[0];
	const FbxAMatrix &MeshTotalMatrix = ComputeTotalMatrix(MeshRootNode);
	for (FbxNode* RootNode : MeshNodeArray)
	{
		// Find all nodes that are sockets
		TArray<FbxSocketNode> SocketNodes;
		FindMeshSockets(RootNode, SocketNodes);

		// Create a UStaticMeshSocket for each fbx socket
		for (int32 SocketIndex = 0; SocketIndex < SocketNodes.Num(); ++SocketIndex)
		{
			FbxSocketNode& SocketNode = SocketNodes[SocketIndex];

			UStaticMeshSocket* Socket = StaticMesh->FindSocket(SocketNode.SocketName);
			if (!Socket)
			{
				// If the socket didn't exist create a new one now
				Socket = NewObject<UStaticMeshSocket>(StaticMesh);
				Socket->bSocketCreatedAtImport = true;
				check(Socket);

				Socket->SocketName = SocketNode.SocketName;
				StaticMesh->Sockets.Add(Socket);
			}

			if (Socket)
			{
				const FbxAMatrix& SocketMatrix = Scene->GetAnimationEvaluator()->GetNodeLocalTransform(SocketNode.Node);
				FbxAMatrix FinalSocketMatrix = MeshTotalMatrix * SocketMatrix;
				FTransform SocketTransform;
				SocketTransform.SetTranslation(Converter.ConvertPos(FinalSocketMatrix.GetT()));
				SocketTransform.SetRotation(Converter.ConvertRotToQuat(FinalSocketMatrix.GetQ()));
				SocketTransform.SetScale3D(Converter.ConvertScale(FinalSocketMatrix.GetS()));

				Socket->RelativeLocation = SocketTransform.GetLocation();
				Socket->RelativeRotation = SocketTransform.GetRotation().Rotator();
				Socket->RelativeScale = SocketTransform.GetScale3D();
			}
		}
		// Delete mesh sockets that were removed from the import data
		if (StaticMesh->Sockets.Num() != SocketNodes.Num())
		{
			for (int32 MeshSocketIx = 0; MeshSocketIx < StaticMesh->Sockets.Num(); ++MeshSocketIx)
			{
				bool Found = false;
				UStaticMeshSocket* MeshSocket = StaticMesh->Sockets[MeshSocketIx];
				//Do not remove socket that was not generated at import
				if (!MeshSocket->bSocketCreatedAtImport)
				{
					continue;
				}

				for (int32 FbxSocketIx = 0; FbxSocketIx < SocketNodes.Num(); FbxSocketIx++)
				{
					if (SocketNodes[FbxSocketIx].SocketName == MeshSocket->SocketName)
					{
						Found = true;
						break;
					}
				}
				if (!Found)
				{
					StaticMesh->Sockets.RemoveAt(MeshSocketIx);
					MeshSocketIx--;
				}
			}
		}
	}
}

void UnFbx::FFbxImporter::ImportStaticMeshGlobalSockets( UStaticMesh* StaticMesh )
{
	FbxNode* RootNode = Scene->GetRootNode();

	// Find all nodes that are sockets
	TArray<FbxSocketNode> SocketNodes;
	FindMeshSockets( RootNode, SocketNodes );

	// Create a UStaticMeshSocket for each fbx socket
	for( int32 SocketIndex = 0; SocketIndex < SocketNodes.Num(); ++SocketIndex )
	{
		FbxSocketNode& SocketNode = SocketNodes[ SocketIndex ];

		UStaticMeshSocket* Socket = StaticMesh->FindSocket( SocketNode.SocketName );
		if( !Socket )
		{
			// If the socket didn't exist create a new one now
			Socket = NewObject<UStaticMeshSocket>(StaticMesh);
			check(Socket);

			Socket->SocketName = SocketNode.SocketName;
			StaticMesh->Sockets.Add(Socket);

			const FbxAMatrix& SocketMatrix = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(SocketNode.Node);
			FTransform SocketTransform;
			SocketTransform.SetTranslation(Converter.ConvertPos(SocketMatrix.GetT()));
			SocketTransform.SetRotation(Converter.ConvertRotToQuat(SocketMatrix.GetQ()));
			SocketTransform.SetScale3D(Converter.ConvertScale(SocketMatrix.GetS()));

			Socket->RelativeLocation = SocketTransform.GetLocation();
			Socket->RelativeRotation = SocketTransform.GetRotation().Rotator();
			Socket->RelativeScale = SocketTransform.GetScale3D();

			Socket->bSocketCreatedAtImport = true;
		}
	}
	// Delete mesh sockets that were removed from the import data
	if (StaticMesh->Sockets.Num() != SocketNodes.Num())
	{
		for (int32 MeshSocketIx = 0; MeshSocketIx < StaticMesh->Sockets.Num(); ++MeshSocketIx)
		{
			bool Found = false;
			UStaticMeshSocket* MeshSocket = StaticMesh->Sockets[MeshSocketIx];
			//Do not remove socket that was not generated at import
			if (!MeshSocket->bSocketCreatedAtImport)
			{
				continue;
			}

			for (int32 FbxSocketIx = 0; FbxSocketIx < SocketNodes.Num(); FbxSocketIx++)
			{
				if (SocketNodes[FbxSocketIx].SocketName == MeshSocket->SocketName)
				{
					Found = true;
					break;
				}
			}
			if (!Found)
			{
				StaticMesh->Sockets.RemoveAt(MeshSocketIx);
				MeshSocketIx--;
			}
		}
	}
}


bool UnFbx::FFbxImporter::FillCollisionModelList(FbxNode* Node)
{
	FbxString NodeName = GetNodeNameWithoutNamespace( Node );

	if ( NodeName.Find("UCX") != -1 || NodeName.Find("MCDCX") != -1 ||
		 NodeName.Find("UBX") != -1 || NodeName.Find("USP") != -1 || NodeName.Find("UCP") != -1)
	{
		// Get name of static mesh that the collision model connect to
		uint32 StartIndex = NodeName.Find('_') + 1;
		int32 TmpEndIndex = NodeName.Find('_', StartIndex);
		int32 EndIndex = TmpEndIndex;
		// Find the last '_' (underscore)
		while (TmpEndIndex >= 0)
		{
			EndIndex = TmpEndIndex;
			TmpEndIndex = NodeName.Find('_', EndIndex+1);
		}
		
		const int32 NumMeshNames = 2;
		FbxString MeshName[NumMeshNames];
		if ( EndIndex >= 0 )
		{
			// all characters between the first '_' and the last '_' are the FBX mesh name
			// convert the name to upper because we are case insensitive
			MeshName[0] = NodeName.Mid(StartIndex, EndIndex - StartIndex).Upper();
			
			// also add a version of the mesh name that includes what follows the last '_'
			// in case that's not a suffix but, instead, is part of the mesh name
			if (StartIndex < (int32)NodeName.GetLen())
			{            
				MeshName[1] = NodeName.Mid(StartIndex).Upper();
			}
		}
		else if (StartIndex < (int32)NodeName.GetLen())
		{            
			MeshName[0] = NodeName.Mid(StartIndex).Upper();
		}

		for (int32 NameIdx = 0; NameIdx < NumMeshNames; ++NameIdx)
		{
			if ((int32)MeshName[NameIdx].GetLen() > 0)
			{
				FbxMap<FbxString, TSharedPtr<FbxArray<FbxNode* > > >::RecordType const *Models = CollisionModels.Find(MeshName[NameIdx]);
				TSharedPtr< FbxArray<FbxNode* > > Record;
				if ( !Models )
				{
					Record = MakeShareable( new FbxArray<FbxNode*>() );
					CollisionModels.Insert(MeshName[NameIdx], Record);
				}
				else
				{
					Record = Models->GetValue();
				}
				Record->Add(Node);
			}
		}

		return true;
	}

	return false;
}

extern void AddConvexGeomFromVertices( const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName );
extern void AddSphereGeomFromVerts(const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName);
extern void AddCapsuleGeomFromVerts(const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName);
extern void AddBoxGeomFromTris(const TArray<FPoly>& Tris, FKAggregateGeom* AggGeom, const TCHAR* ObjName);
extern void DecomposeUCXMesh( const TArray<FVector>& CollisionVertices, const TArray<int32>& CollisionFaceIdx, UBodySetup* BodySetup );

bool UnFbx::FFbxImporter::ImportCollisionModels(UStaticMesh* StaticMesh, const FbxString& InNodeName)
{
	// find collision models
	bool bRemoveEmptyKey = false;
	FbxString EmptyKey;

	// convert the name to upper because we are case insensitive
	FbxMap<FbxString, TSharedPtr< FbxArray<FbxNode* > > >::RecordType const *Record = CollisionModels.Find(InNodeName.Upper());
	if ( !Record )
	{
		// compatible with old collision name format
		// if CollisionModels has only one entry and the key is ""
		if ( CollisionModels.GetSize() == 1 )
		{
			Record = CollisionModels.Find( EmptyKey );
		}
		if ( !Record ) 
		{
			return false;
		}
		else
		{
			bRemoveEmptyKey = true;
		}
	}

	TSharedPtr< FbxArray<FbxNode*> > Models = Record->GetValue();

	StaticMesh->bCustomizedCollision = true;

	StaticMesh->CreateBodySetup();

	TArray<FVector>	CollisionVertices;
	TArray<int32>		CollisionFaceIdx;

	// construct collision model
	for (int32 ModelIndex=0; ModelIndex<Models->GetCount(); ModelIndex++)
	{
		FbxNode* Node = Models->GetAt(ModelIndex);
		FbxMesh* FbxMesh = Node->GetMesh();

		FbxMesh->RemoveBadPolygons();

		// Must do this before triangulating the mesh due to an FBX bug in TriangulateMeshAdvance
		int32 LayerSmoothingCount = FbxMesh->GetLayerCount(FbxLayerElement::eSmoothing);
		for(int32 LayerIndex = 0; LayerIndex < LayerSmoothingCount; LayerIndex++)
		{
			GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing (FbxMesh, LayerIndex);
		}

		if (!FbxMesh->IsTriangleMesh())
		{
			FString NodeName = UTF8_TO_TCHAR(MakeName(Node->GetName()));
			UE_LOG(LogFbx, Warning, TEXT("Triangulating mesh %s for collision model"), *NodeName);

			const bool bReplace = true;
			FbxNodeAttribute* ConvertedNode = GeometryConverter->Triangulate(FbxMesh, bReplace); // not in place ! the old mesh is still there

			if( ConvertedNode != NULL && ConvertedNode->GetAttributeType() == FbxNodeAttribute::eMesh )
			{
				FbxMesh = (fbxsdk::FbxMesh*)ConvertedNode;
			}
			else
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Error_FailedToTriangulate", "Unable to triangulate mesh '{0}'"), FText::FromString(NodeName))), FFbxErrors::Generic_Mesh_TriangulationFailed);
				return false;
			}
		}

		int32 ControlPointsIndex;
		int32 ControlPointsCount = FbxMesh->GetControlPointsCount();
		FbxVector4* ControlPoints = FbxMesh->GetControlPoints();
		FbxAMatrix Matrix = ComputeTotalMatrix(Node);

		for ( ControlPointsIndex = 0; ControlPointsIndex < ControlPointsCount; ControlPointsIndex++ )
		{
			new(CollisionVertices)FVector(Converter.ConvertPos(Matrix.MultT(ControlPoints[ControlPointsIndex])));
		}

		int32 TriangleCount = FbxMesh->GetPolygonCount();
		int32 TriangleIndex;
		for ( TriangleIndex = 0 ; TriangleIndex < TriangleCount ; TriangleIndex++ )
		{
			new(CollisionFaceIdx)int32(FbxMesh->GetPolygonVertex(TriangleIndex,0));
			new(CollisionFaceIdx)int32(FbxMesh->GetPolygonVertex(TriangleIndex,1));
			new(CollisionFaceIdx)int32(FbxMesh->GetPolygonVertex(TriangleIndex,2));
		}

		TArray<FPoly> CollisionTriangles;

		// Make triangles
		for(int32 x = 0;x < CollisionFaceIdx.Num();x += 3)
		{
			FPoly*	Poly = new( CollisionTriangles ) FPoly();

			Poly->Init();

			new(Poly->Vertices) FVector( CollisionVertices[CollisionFaceIdx[x + 2]] );
			new(Poly->Vertices) FVector( CollisionVertices[CollisionFaceIdx[x + 1]] );
			new(Poly->Vertices) FVector( CollisionVertices[CollisionFaceIdx[x + 0]] );
			Poly->iLink = x / 3;

			Poly->CalcNormal(1);
		}

		// Construct geometry object
		FbxString ModelName(Node->GetName());
		if ( ModelName.Find("UCX") != -1 || ModelName.Find("MCDCX") != -1 )
		{
			if( !ImportOptions->bOneConvexHullPerUCX )
			{
				DecomposeUCXMesh( CollisionVertices, CollisionFaceIdx, StaticMesh->BodySetup );
			}
			else
			{
				FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

				// This function cooks the given data, so we cannot test for duplicates based on the position data
				// before we call it
				AddConvexGeomFromVertices( CollisionVertices, &AggGeo, ANSI_TO_TCHAR(Node->GetName()) );

				// Now test the late element in the AggGeo list and remove it if its a duplicate
				if(AggGeo.ConvexElems.Num() > 1)
				{
					FKConvexElem& NewElem = AggGeo.ConvexElems.Last();

					for(int32 ElementIndex = 0; ElementIndex < AggGeo.ConvexElems.Num()-1; ++ElementIndex)
					{
						FKConvexElem& CurrentElem = AggGeo.ConvexElems[ElementIndex];
					
						if(CurrentElem.VertexData.Num() == NewElem.VertexData.Num())
						{
							bool bFoundDifference = false;
							for(int32 VertexIndex = 0; VertexIndex < NewElem.VertexData.Num(); ++VertexIndex)
							{
								if(CurrentElem.VertexData[VertexIndex] != NewElem.VertexData[VertexIndex])
								{
									bFoundDifference = true;
									break;
								}
							}

							if(!bFoundDifference)
							{
								// The new collision geo is a duplicate, delete it
								AggGeo.ConvexElems.RemoveAt(AggGeo.ConvexElems.Num()-1);
								break;
							}
						}
					}
				}
			}
		}
		else if ( ModelName.Find("UBX") != -1 )
		{
			FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

			AddBoxGeomFromTris( CollisionTriangles, &AggGeo, ANSI_TO_TCHAR(Node->GetName()) );

			// Now test the last element in the AggGeo list and remove it if its a duplicate
			if(AggGeo.BoxElems.Num() > 1)
			{
				FKBoxElem& NewElem = AggGeo.BoxElems.Last();

				for(int32 ElementIndex = 0; ElementIndex < AggGeo.BoxElems.Num()-1; ++ElementIndex)
				{
					FKBoxElem& CurrentElem = AggGeo.BoxElems[ElementIndex];

					if(	CurrentElem == NewElem )
					{
						// The new element is a duplicate, remove it
						AggGeo.BoxElems.RemoveAt(AggGeo.BoxElems.Num()-1);
						break;
					}
				}
			}
		}
		else if ( ModelName.Find("USP") != -1 )
		{
			FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

			AddSphereGeomFromVerts( CollisionVertices, &AggGeo, ANSI_TO_TCHAR(Node->GetName()) );

			// Now test the late element in the AggGeo list and remove it if its a duplicate
			if(AggGeo.SphereElems.Num() > 1)
			{
				FKSphereElem& NewElem = AggGeo.SphereElems.Last();

				for(int32 ElementIndex = 0; ElementIndex < AggGeo.SphereElems.Num()-1; ++ElementIndex)
				{
					FKSphereElem& CurrentElem = AggGeo.SphereElems[ElementIndex];

					if(	CurrentElem == NewElem )
					{
						// The new element is a duplicate, remove it
						AggGeo.SphereElems.RemoveAt(AggGeo.SphereElems.Num()-1);
						break;
					}
				}
			}
		}
		else if (ModelName.Find("UCP") != -1)
		{
			FKAggregateGeom& AggGeo = StaticMesh->BodySetup->AggGeom;

			AddCapsuleGeomFromVerts(CollisionVertices, &AggGeo, ANSI_TO_TCHAR(Node->GetName()));

			// Now test the late element in the AggGeo list and remove it if its a duplicate
			if (AggGeo.SphylElems.Num() > 1)
			{
				FKSphylElem& NewElem = AggGeo.SphylElems.Last();
				for (int32 ElementIndex = 0; ElementIndex < AggGeo.SphylElems.Num() - 1; ++ElementIndex)
				{
					FKSphylElem& CurrentElem = AggGeo.SphylElems[ElementIndex];
					if (CurrentElem == NewElem)
					{
						// The new element is a duplicate, remove it
						AggGeo.SphylElems.RemoveAt(AggGeo.SphylElems.Num() - 1);
						break;
					}
				}
			}
		}

		// Clear any cached rigid-body collision shapes for this body setup.
		StaticMesh->BodySetup->ClearPhysicsMeshes();

		// Remove the empty key because we only use the model once for the first mesh
		if (bRemoveEmptyKey)
		{
			CollisionModels.Remove(EmptyKey);
		}

		CollisionVertices.Empty();
		CollisionFaceIdx.Empty();
	}

	// Create new GUID
	StaticMesh->BodySetup->InvalidatePhysicsData();

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(*StaticMesh);
		
	return true;
}
#undef LOCTEXT_NAMESPACE
