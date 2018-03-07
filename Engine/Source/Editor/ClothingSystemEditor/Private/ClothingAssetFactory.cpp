// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ClothingAssetFactory.h"

#include "Factories.h"
#include "PhysXPublic.h"
#include "PhysicsPublic.h"
#include "ClothingAsset.h"
#include "ClothingAssetAuthoring.h"
#include "ApexClothingUtils.h"
#include "ContentBrowserModule.h"
#include "Misc/FileHelper.h"
#include "Engine/SkeletalMesh.h"
#include "MessageDialog.h"
#include "ObjectTools.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SphereElem.h"
#include "ComponentReregisterContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Utils/ClothingMeshUtils.h"

#define LOCTEXT_NAMESPACE "ClothingAssetFactory"
DEFINE_LOG_CATEGORY(LogClothingAssetFactory)

using namespace nvidia::apex;

namespace ClothingFactoryConstants
{
	// For verifying the file 
	static const char ClothingAssetClass[] = "ClothingAssetParameters";

	// Import transformation params
	static const char ParamName_BoneActors[] = "boneActors";
	static const char ParamName_BoneSpheres[] = "boneSpheres";
	static const char ParamName_GravityDirection[] = "simulation.gravityDirection";
	static const char ParamName_UvOrigin[] = "textureUVOrigin";

	// UV flip params
	static const char ParamName_SubmeshArray[] = "submeshes";
	static const char ParamName_SubmeshBufferFormats[] = "vertexBuffer.vertexFormat.bufferFormats";
	static const char ParamName_VertexBuffers[] = "vertexBuffer.buffers";
	static const char ParamName_Semantic[] = "semantic";
	static const char ParamName_BufferData[] = "data";

	static const char ParamName_GLOD_Platforms[] = "platforms";
	static const char ParamName_GLOD_LOD[] = "lod";
	static const char ParamName_GLOD_PhysMeshID[] = "physicalMeshId";
	static const char ParamName_GLOD_RenderMeshAsset[] = "renderMeshAsset";
	static const char ParamName_GLOD_ImmediateClothMap[] = "immediateClothMap";
	static const char ParamName_GLOD_SkinClothMapB[] = "SkinClothMapB";
	static const char ParamName_GLOD_SkinClothMap[] = "SkinClothMap";
	static const char ParamName_GLOD_SkinClothMapThickness[] = "skinClothMapThickness";
	static const char ParamName_GLOD_SkinClothMapOffset[] = "skinClothMapOffset";
	static const char ParamName_GLOD_TetraMap[] = "tetraMap";
	static const char ParamName_GLOD_RenderMeshAssetSorting[] = "renderMeshAssetSorting";
	static const char ParamName_GLOD_PhysicsMeshPartitioning[] = "physicsMeshPartitioning";

	static const char ParamName_Partition_GraphicalSubmesh[] = "graphicalSubmesh";
	static const char ParamName_Partition_NumSimVerts[] = "numSimulatedVertices";
	static const char ParamName_Partition_NumSimVertsAdditional[] = "numSimulatedVerticesAdditional";
	static const char ParamName_Partition_NumSimIndices[] = "numSimulatedIndices";
}

void LogAndToastWarning(const FText& Error)
{
	FNotificationInfo Info(Error);
	Info.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	UE_LOG(LogClothingAssetFactory, Warning, TEXT("%s"), *Error.ToString());
}

UClothingAssetFactory::UClothingAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UClothingAssetBase* UClothingAssetFactory::Import
(
	const FString& Filename, 
	USkeletalMesh* TargetMesh,
	FName InName
)
{
#if WITH_APEX_CLOTHING
	if(!TargetMesh)
	{
		return nullptr;
	}

	UClothingAsset* NewClothingAsset = nullptr;

	TArray<uint8> FileBuffer;
	if(FFileHelper::LoadFileToArray(FileBuffer, *Filename, FILEREAD_Silent))
	{
		ClothingAsset* ApexAsset = ApexClothingUtils::CreateApexClothingAssetFromBuffer(FileBuffer.GetData(), FileBuffer.Num());
		ApexAsset = ConvertApexAssetCoordSystem(ApexAsset);

		if(InName == NAME_None)
		{
			InName = *FPaths::GetBaseFilename(Filename);
		}

		// Create an unreal clothing asset
		NewClothingAsset = Cast<UClothingAsset>(CreateFromApexAsset(ApexAsset, TargetMesh, InName));

		if(NewClothingAsset)
		{
			// Store import path
			NewClothingAsset->ImportedFilePath = Filename;

			// Push to the target mesh
			TargetMesh->AddClothingAsset(NewClothingAsset);
		}
	}

	return NewClothingAsset;
#else
	return nullptr;
#endif
}

UClothingAssetBase* UClothingAssetFactory::Reimport(const FString& Filename, USkeletalMesh* TargetMesh, UClothingAssetBase* OriginalAsset)
{
#if WITH_APEX_CLOTHING
	if(!TargetMesh)
	{
		return nullptr;
	}

	int32 OldIndex = INDEX_NONE;
	if(TargetMesh->MeshClothingAssets.Find(OriginalAsset, OldIndex))
	{
		TArray<UActorComponent*> ComponentsToReregister;
		for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			if(USkeletalMesh* UsedMesh = (*It)->SkeletalMesh)
			{
				if(UsedMesh == TargetMesh)
				{
					ComponentsToReregister.Add(*It);
				}
			}
		}
	
		FMultiComponentReregisterContext ReregisterContext(ComponentsToReregister);

		UClothingAsset* OldClothingAsset = Cast<UClothingAsset>(TargetMesh->MeshClothingAssets[OldIndex]);
		UClothingAsset* NewClothingAsset = nullptr;
		FName AssetName = NAME_None;

		if(!OldClothingAsset || !TargetMesh->MeshClothingAssets.IsValidIndex(OldIndex))
		{
			return nullptr;
		}

		TArray<uint8> FileBuffer;
		if(FFileHelper::LoadFileToArray(FileBuffer, *Filename, FILEREAD_Silent))
		{
			ClothingAsset* ApexAsset = ApexClothingUtils::CreateApexClothingAssetFromBuffer(FileBuffer.GetData(), FileBuffer.Num());
			ApexAsset = ConvertApexAssetCoordSystem(ApexAsset);
			AssetName = *FPaths::GetBaseFilename(Filename);

			// Work out the bindings to the old asset so we can reproduce them for the new asset
			struct Local_BindingInfo
			{
				int32 MeshLodIndex;
				int32 MeshLodSectionIndex;
				int32 AssetLodIndex;
			};
			TArray<Local_BindingInfo> AssetBindings;

			FSkeletalMeshResource* MeshResource = TargetMesh->GetImportedResource();
			if(MeshResource)
			{
				const int32 NumLods = MeshResource->LODModels.Num();

				for(int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
				{
					FStaticLODModel& LodModel = MeshResource->LODModels[LodIndex];

					const int32 NumSections = LodModel.Sections.Num();

					for(int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
					{
						FSkelMeshSection& Section = LodModel.Sections[SectionIndex];

						if(Section.ClothingData.AssetGuid == OldClothingAsset->AssetGuid && Section.bDisabled == true)
						{
							// Found a binding
							AssetBindings.AddDefaulted();

							Local_BindingInfo& Binding = AssetBindings.Last();
							Binding.MeshLodIndex = LodIndex;
							Binding.MeshLodSectionIndex = SectionIndex;
							Binding.AssetLodIndex = Section.ClothingData.AssetLodIndex;
						}
					}
				}
			}

			OldClothingAsset->UnbindFromSkeletalMesh(TargetMesh);

			// Create an unreal clothing asset
			NewClothingAsset = Cast<UClothingAsset>(CreateFromApexAsset(ApexAsset, TargetMesh, AssetName));

			if(NewClothingAsset)
			{
				// Store import path
				NewClothingAsset->ImportedFilePath = Filename;

				TargetMesh->MeshClothingAssets[OldIndex] = NewClothingAsset;

				for(Local_BindingInfo& Binding : AssetBindings)
				{
					NewClothingAsset->BindToSkeletalMesh(TargetMesh, Binding.MeshLodIndex, Binding.MeshLodSectionIndex, Binding.AssetLodIndex);
				}

			}
		}

		return NewClothingAsset;
	}
#endif

	return nullptr;
}

UClothingAssetBase* UClothingAssetFactory::CreateFromSkeletalMesh(USkeletalMesh* TargetMesh, FSkeletalMeshClothBuildParams& Params)
{
	// Need a valid skel mesh
	if(!TargetMesh)
	{
		return nullptr;
	}

	FSkeletalMeshResource* Mesh = TargetMesh->GetImportedResource();

	// Need a valid resource
	if(!Mesh)
	{
		return nullptr;
	}

	// Need a valid LOD model
	if(!Mesh->LODModels.IsValidIndex(Params.LodIndex))
	{
		return nullptr;
	}

	FStaticLODModel& LodModel = Mesh->LODModels[Params.LodIndex];

	// Need a valid section
	if(!LodModel.Sections.IsValidIndex(Params.SourceSection))
	{
		return nullptr;
	}

	// Ok, we have a valid mesh and section, we can now extract it as a sim mesh
	FSkelMeshSection& SourceSection = LodModel.Sections[Params.SourceSection];

	// Can't convert to a clothing asset if bound to clothing
	if(SourceSection.CorrespondClothSectionIndex != INDEX_NONE)
	{
		return nullptr;
	}

	FString SanitizedName = ObjectTools::SanitizeObjectName(Params.AssetName);
	FName ObjectName = MakeUniqueObjectName(TargetMesh, UClothingAsset::StaticClass(), FName(*SanitizedName));
	UClothingAsset* NewAsset = NewObject<UClothingAsset>(TargetMesh, ObjectName);
	NewAsset->SetFlags(RF_Transactional);

	// Adding a new LOD from this skeletal mesh
	NewAsset->LodData.AddDefaulted();
	FClothLODData& LodData = NewAsset->LodData.Last();

	if(ImportToLodInternal(TargetMesh, Params.LodIndex, Params.SourceSection, NewAsset, LodData))
	{
		if(Params.bRemoveFromMesh)
		{
			// User doesn't want the section anymore as a renderable, get rid of it
			TargetMesh->RemoveMeshSection(Params.LodIndex, Params.SourceSection);
		}

		// Set asset guid
		NewAsset->AssetGuid = FGuid::NewGuid();

		// Set physics asset, will be used when building actors for cloth collisions
		NewAsset->PhysicsAsset = Params.PhysicsAsset.LoadSynchronous();

		// Build the final bone map
		NewAsset->RefreshBoneMapping(TargetMesh);

		// Invalidate cached data as the mesh has changed
		NewAsset->InvalidateCachedData();

		return NewAsset;
	}

	return nullptr;
}

UClothingAssetBase* UClothingAssetFactory::ImportLodToClothing(USkeletalMesh* TargetMesh, FSkeletalMeshClothBuildParams& Params)
{
	if(!TargetMesh)
	{
		// Invalid target - can't continue.
		LogAndToastWarning(LOCTEXT("Warning_InvalidLodMesh", "Failed to import clothing LOD, invalid target mesh specified"));
		return nullptr;
	}

	if(!Params.TargetAsset.IsValid())
	{
		// Invalid target - can't continue.
		LogAndToastWarning(LOCTEXT("Warning_InvalidClothTarget", "Failed to import clothing LOD, invalid target clothing object"));
		return nullptr;
	}

	FSkeletalMeshResource* MeshResource = TargetMesh->GetImportedResource();
	check(MeshResource);

	const int32 NumMeshLods = MeshResource->LODModels.Num();

	if(UClothingAssetBase* TargetClothing = Params.TargetAsset.Get())
	{
		// Find the clothing asset in the mesh to verify the params are correct
		int32 MeshAssetIndex = INDEX_NONE;
		if(TargetMesh->MeshClothingAssets.Find(TargetClothing, MeshAssetIndex))
		{
			// Everything looks good, continue to actual import
			UClothingAsset* ConcreteTarget = CastChecked<UClothingAsset>(TargetClothing);

			FClothLODData* RemapSource = nullptr;

			if(Params.bRemapParameters)
			{
				if(Params.TargetLod == ConcreteTarget->LodData.Num())
				{
					// New LOD, remap from previous
					RemapSource = &ConcreteTarget->LodData.Last();
				}
				else
				{
					// This is a replacement, remap from current LOD
					check(ConcreteTarget->LodData.IsValidIndex(Params.TargetLod));
					RemapSource = &ConcreteTarget->LodData[Params.TargetLod];
				}
			}

			if(Params.TargetLod == ConcreteTarget->LodData.Num())
			{
				ConcreteTarget->LodData.AddDefaulted();
			}
			else if(!ConcreteTarget->LodData.IsValidIndex(Params.TargetLod))
			{
				LogAndToastWarning(LOCTEXT("Warning_InvalidLodTarget", "Failed to import clothing LOD, invalid target LOD."));
				return nullptr;
			}

			FClothLODData& NewLod = ConcreteTarget->LodData[Params.TargetLod];

			if(Params.TargetLod > 0 && Params.bRemapParameters)
			{
				RemapSource = &ConcreteTarget->LodData[Params.TargetLod - 1];
			}

			if(ImportToLodInternal(TargetMesh, Params.LodIndex, Params.SourceSection, ConcreteTarget, NewLod, RemapSource))
			{
				if(Params.bRemoveFromMesh)
				{
					// User doesn't want the section anymore as a renderable, get rid of it
					TargetMesh->RemoveMeshSection(Params.LodIndex, Params.SourceSection);
				}

				// Rebuild the final bone map
				ConcreteTarget->RefreshBoneMapping(TargetMesh);

				// Build Lod skinning map for smooth transitions
				ConcreteTarget->BuildLodTransitionData();

				// Invalidate cached data as the mesh has changed
				ConcreteTarget->InvalidateCachedData();

				return ConcreteTarget;
			}
		}
	}

	return nullptr;
}

UClothingAssetBase* UClothingAssetFactory::CreateFromApexAsset(nvidia::apex::ClothingAsset* InApexAsset, USkeletalMesh* TargetMesh, FName InName)
{
#if WITH_APEX_CLOTHING
	UClothingAsset* NewClothingAsset = NewObject<UClothingAsset>(TargetMesh, InName);
	NewClothingAsset->SetFlags(RF_Transactional);

	const NvParameterized::Interface* AssetParams = InApexAsset->getAssetNvParameterized();
	NvParameterized::Handle GraphicalLodArrayHandle(*AssetParams, "graphicalLods");

	int32 NumSuccessfulLods = 0;

	int32 NumLodsToBuild = 0;
	GraphicalLodArrayHandle.getArraySize(NumLodsToBuild);

	NewClothingAsset->LodData.AddZeroed(NumLodsToBuild);

	for(int32 CurrLodIdx = 0; CurrLodIdx < NumLodsToBuild; ++CurrLodIdx)
	{
		FClothLODData& CurrentLodData = NewClothingAsset->LodData[CurrLodIdx];

		TArray<FApexVertData> ApexVertData;

		ExtractLodPhysicalData(NewClothingAsset, *InApexAsset, CurrLodIdx, CurrentLodData, ApexVertData);
		ExtractSphereCollisions(NewClothingAsset, *InApexAsset, CurrLodIdx, CurrentLodData);
		ExtractMaterialParameters(NewClothingAsset, *InApexAsset);

		// Set to use legacy wind calculations, which is what APEX would normally have used
		NewClothingAsset->ClothConfig.WindMethod = EClothingWindMethod::Legacy;

		// Fixup unreal-side bone indices
		const int32 NumBoneDatas = CurrentLodData.PhysicalMeshData.BoneData.Num();
		check(NumBoneDatas == ApexVertData.Num());
		for(int32 BoneDataIndex = 0; BoneDataIndex < NumBoneDatas; ++BoneDataIndex)
		{
			FClothVertBoneData& BoneData = CurrentLodData.PhysicalMeshData.BoneData[BoneDataIndex];
			FApexVertData& CurrentVertData = ApexVertData[BoneDataIndex];
		
			for(int32 BoneInfluenceIdx = 0; BoneInfluenceIdx < MAX_TOTAL_INFLUENCES; ++BoneInfluenceIdx)
			{
				uint16 ApexBoneIndex = CurrentVertData.BoneIndices[BoneInfluenceIdx];
				BoneData.BoneIndices[BoneInfluenceIdx] = ApexBoneIndex;
			}
		}
	}

	ExtractBoneData(NewClothingAsset, *InApexAsset);

	// Now that we've extracted the APEX bone data, we need to fill the generic asset
	// data with bone data for Unreal rather than APEX internal representations
	const int32 NumUsedBones = NewClothingAsset->UsedBoneNames.Num();

	NewClothingAsset->UsedBoneIndices.AddDefaulted(NumUsedBones);
	for(int32 UsedBoneIndex = 0; UsedBoneIndex < NumUsedBones; ++UsedBoneIndex)
	{
		const FName& BoneName = NewClothingAsset->UsedBoneNames[UsedBoneIndex];
		const int32 UnrealBoneIndex = TargetMesh->RefSkeleton.FindBoneIndex(BoneName);

		// If we find an invalid bone then the asset is invalid, as it cannot be skinned to this mesh
		if(UnrealBoneIndex == INDEX_NONE)
		{
			FText ErrorText = FText::Format(LOCTEXT("InvalidBoneError", "Imported asset requires bone \"{0}\", which is not present in the skeletal mesh ({1})"), FText::FromName(BoneName), FText::FromString(TargetMesh->GetName()));
			LogAndToastWarning(ErrorText);

			return nullptr;
		}
		
		NewClothingAsset->UsedBoneIndices[UsedBoneIndex] = UnrealBoneIndex;
	}

	uint32 AssetInternalRootBoneIndex;
	verify(NvParameterized::getParamU32(*AssetParams, "rootBoneIndex", AssetInternalRootBoneIndex));
	FName ConvertedBoneName(*FString(InApexAsset->getBoneName(AssetInternalRootBoneIndex)).Replace(TEXT(" "), TEXT("-")));

	NewClothingAsset->AssetGuid = FGuid::NewGuid();
	NewClothingAsset->InvalidateCachedData();

	NewClothingAsset->BuildLodTransitionData();
	NewClothingAsset->BuildSelfCollisionData();
	NewClothingAsset->CalculateReferenceBoneIndex();

	// Add masks for parameters
	for(FClothLODData& Lod : NewClothingAsset->LodData)
	{
		FClothPhysicalMeshData& PhysMesh = Lod.PhysicalMeshData;

		// Didn't do anything previously - clear out incase there's something in there
		// so we can use it correctly now.
		Lod.ParameterMasks.Reset(3);

		// Max distances
		Lod.ParameterMasks.AddDefaulted();
		FClothParameterMask_PhysMesh& MaxDistanceMask = Lod.ParameterMasks.Last();
		MaxDistanceMask.CopyFromPhysMesh(PhysMesh, MaskTarget_PhysMesh::MaxDistance);
		MaxDistanceMask.bEnabled = true;

		if(PhysMesh.BackstopRadiuses.FindByPredicate([](const float& A) {return A != 0.0f; }))
		{
			// Backstop radii
			Lod.ParameterMasks.AddDefaulted();
			FClothParameterMask_PhysMesh& BackstopRadiusMask = Lod.ParameterMasks.Last();
			BackstopRadiusMask.CopyFromPhysMesh(PhysMesh, MaskTarget_PhysMesh::BackstopRadius);
			BackstopRadiusMask.bEnabled = true;

			// Backstop distances
			Lod.ParameterMasks.AddDefaulted();
			FClothParameterMask_PhysMesh& BackstopDistanceMask = Lod.ParameterMasks.Last();
			BackstopDistanceMask.CopyFromPhysMesh(PhysMesh, MaskTarget_PhysMesh::BackstopDistance);
			BackstopDistanceMask.bEnabled = true;
		}
	}

	return NewClothingAsset;
#endif
	return nullptr;
}

bool UClothingAssetFactory::CanImport(const FString& Filename)
{
#if WITH_APEX_CLOTHING
	// Need to read in the file and try to create an asset to get it's type
	TArray<uint8> FileBuffer;
	if(FFileHelper::LoadFileToArray(FileBuffer, *Filename, FILEREAD_Silent))
	{
		physx::PxFileBuf* Stream = GApexSDK->createMemoryReadStream(FileBuffer.GetData(), FileBuffer.Num());
		if(Stream)
		{
			NvParameterized::Serializer::SerializeType SerializeType = GApexSDK->getSerializeType(*Stream);
			if(NvParameterized::Serializer* Serializer = GApexSDK->createSerializer(SerializeType))
			{
				NvParameterized::Serializer::DeserializedData DeserializedData;
				Serializer->deserialize(*Stream, DeserializedData);

				if(DeserializedData.size() > 0)
				{
					NvParameterized::Interface* AssetInterface = DeserializedData[0];

					int32 StringLength = StringCast<TCHAR>(AssetInterface->className()).Length();
					FString ClassName(StringLength, StringCast<TCHAR>(AssetInterface->className()).Get());

					if(ClassName == ClothingFactoryConstants::ClothingAssetClass)
					{
						return true;
					}
				}
			}

			GApexSDK->releaseMemoryReadStream(*Stream);
		}
	}
#endif

	return false;
}

#if WITH_APEX_CLOTHING
ClothingAsset* UClothingAssetFactory::ConvertApexAssetCoordSystem(ClothingAsset* InAsset)
{
	// Build new asset interface to store the transformed asset
	const NvParameterized::Interface* OriginalInterface = InAsset->getAssetNvParameterized();
	NvParameterized::Interface* NewInterface = GApexSDK->getParameterizedTraits()->createNvParameterized(OriginalInterface->className());
	check(NewInterface);

	// Copy asset data
	NewInterface->copy(*OriginalInterface);

	ClothingAssetAuthoring* AssetAuthoring = (ClothingAssetAuthoring*)GApexSDK->createAssetAuthoring(NewInterface, nullptr);

	check(AssetAuthoring);

	// Need to check for bone actors and spheres, we can't have both
	PxI32 NumBoneActors = 0;
	PxI32 NumBoneSpheres = 0;

	verify(NvParameterized::getParamArraySize(*OriginalInterface, ClothingFactoryConstants::ParamName_BoneActors, NumBoneActors));
	verify(NvParameterized::getParamArraySize(*OriginalInterface, ClothingFactoryConstants::ParamName_BoneSpheres, NumBoneSpheres));

	// Remove collision if we have spheres and actors (actors will remain)
	if(NumBoneActors > 0 && NumBoneSpheres > 0)
	{
		AssetAuthoring->clearCollision();
	}

	// Y direction needs to be inverted
	PxMat44 YInvertMatrix(PxIdentity);
	YInvertMatrix.column1.y = -1.0f;

	// Matrix holding the coordinate space conversion required for the mesh
	PxMat44 ConversionTransform(PxIdentity);

	// Get gravity direction, as that should be -up
	PxVec3 GravityDirection;
	verify(NvParameterized::getParamVec3(*OriginalInterface, ClothingFactoryConstants::ParamName_GravityDirection, GravityDirection));

	// Y-up, needs conversion to z-up
	if(GravityDirection.z == 0.0f && FMath::Abs(GravityDirection.y) > 0.0f)
	{
		PxVec3 NewGravityDirection(GravityDirection.x, GravityDirection.z, GravityDirection.y);
		AssetAuthoring->setSimulationGravityDirection(NewGravityDirection);

		// Invert Y + 90 deg rotation on x
		ConversionTransform.column1.y = 0.0f;
		ConversionTransform.column1.z = 1.0f;
		ConversionTransform.column2.y = 1.0f;
		ConversionTransform.column2.z = 0.0f;
	}
	else
	{
		ConversionTransform = YInvertMatrix;
	}

	AssetAuthoring->applyTransformation(ConversionTransform, 1.0f, true, true);

	// Transform bind poses
	const uint32 NumUsedBones = InAsset->getNumUsedBones();

	TArray<PxMat44> TransformedBindPoses;
	TransformedBindPoses.Reserve(NumUsedBones);

	for(uint32 Idx = 0; Idx < NumUsedBones; ++Idx)
	{
		PxMat44 CurrentBindPose(PxIdentity);
		AssetAuthoring->getBoneBindPose(Idx, CurrentBindPose);
		TransformedBindPoses.Add(CurrentBindPose * YInvertMatrix);
	}

	AssetAuthoring->updateBindPoses(TransformedBindPoses.GetData(), TransformedBindPoses.Num(), true, true);

	const uint32 NumLods = AssetAuthoring->getNumLods();
	for(uint32 Idx = 0; Idx < NumLods; ++Idx)
	{
		if(NvParameterized::Interface* RenderMeshAuthoringInterface = AssetAuthoring->getRenderMeshAssetAuthoring(Idx))
		{
			NvParameterized::Handle RenderMeshAuthoringHandle(RenderMeshAuthoringInterface);

			bool bFlipU = false;
			bool bFlipV = false;

			NvParameterized::Interface* UVOriginParameter = NvParameterized::findParam(*RenderMeshAuthoringInterface, ClothingFactoryConstants::ParamName_UvOrigin, RenderMeshAuthoringHandle);
			if(UVOriginParameter)
			{
				uint32 UvOrigin = 0;
				RenderMeshAuthoringHandle.getParamU32(UvOrigin);

				switch(UvOrigin)
				{
				case TextureUVOrigin::ORIGIN_TOP_LEFT:
					bFlipU = false;
					bFlipV = false;
					break;
				case TextureUVOrigin::ORIGIN_TOP_RIGHT:
					bFlipU = true;
					bFlipV = false;
					break;
				case TextureUVOrigin::ORIGIN_BOTTOM_LEFT:
					bFlipU = false;
					bFlipV = true;
					break;
				case TextureUVOrigin::ORIGIN_BOTTOM_RIGHT:
					bFlipU = false;
					bFlipV = false;
					break;
				default:
					break;
				}

				RenderMeshAuthoringHandle.setParamU32(TextureUVOrigin::ORIGIN_TOP_LEFT);
			}

			// Flip UVs
			FlipAuthoringUvs(RenderMeshAuthoringInterface, bFlipU, bFlipV);
		}
		else
		{
			break;
		}
	}

	char AssetName[MAX_SPRINTF] = {0};
	FCStringAnsi::Strncpy(AssetName, InAsset->getName(), MAX_SPRINTF);

	InAsset->release();
	InAsset = nullptr;

	ClothingAsset* NewAsset = (ClothingAsset*)GApexSDK->createAsset(*AssetAuthoring, AssetName);

	check(NewAsset);

	AssetAuthoring->release();

	return NewAsset;
}

void UClothingAssetFactory::FlipAuthoringUvs(NvParameterized::Interface* InRenderMeshAuthoringInterface, bool bFlipU, bool bFlipV)
{
	if(!bFlipU && !bFlipV)
	{
		// Don't need to do anything
		return;
	}

	NvParameterized::Handle SubmeshArrayHandle(*InRenderMeshAuthoringInterface, ClothingFactoryConstants::ParamName_SubmeshArray);

	if(SubmeshArrayHandle.isValid())
	{
		int32 ArraySize = 0;
		SubmeshArrayHandle.getArraySize(ArraySize, 0);

		for(int32 SubmeshIdx = 0; SubmeshIdx < ArraySize; ++SubmeshIdx)
		{
			NvParameterized::Handle SubmeshHandle(SubmeshArrayHandle);
			SubmeshArrayHandle.getChildHandle(SubmeshIdx, SubmeshHandle);

			if(!SubmeshHandle.isValid())
			{
				// No submesh, move to next array entry
				continue;
			}

			NvParameterized::Interface* SubmeshInterface = nullptr;
			SubmeshHandle.getParamRef(SubmeshInterface);

			NvParameterized::Handle BufferFormatsHandle(SubmeshHandle);
			NvParameterized::findParam(*SubmeshInterface, ClothingFactoryConstants::ParamName_SubmeshBufferFormats, BufferFormatsHandle);

			if(!BufferFormatsHandle.isValid())
			{
				// No valid format array, move to next submesh
				continue;
			}

			int32 FormatArraySize = 0;
			BufferFormatsHandle.getArraySize(FormatArraySize);

			for(int32 FormatIdx = 0; FormatIdx < FormatArraySize; ++FormatIdx)
			{
				NvParameterized::Handle FormatHandle(BufferFormatsHandle);
				FormatHandle.set(FormatIdx);

				NvParameterized::Handle SemanticHandle(FormatHandle);
				FormatHandle.getChildHandle(FormatHandle.getInterface(), "semantic", SemanticHandle);

				if(!SemanticHandle.isValid())
				{
					// No valid semantic, move to next buffer format
					continue;
				}

				PxI32 BufferSemantic = -1;
				SemanticHandle.getParamI32(BufferSemantic);

				if(BufferSemantic >= RenderVertexSemantic::TEXCOORD0 &&
				   BufferSemantic <= RenderVertexSemantic::TEXCOORD3)
				{
					NvParameterized::Handle BufferArrayHandle(SubmeshHandle);
					NvParameterized::findParam(*SubmeshInterface, ClothingFactoryConstants::ParamName_VertexBuffers, BufferArrayHandle);

					int32 BufferArraySize = -1;
					BufferArrayHandle.getArraySize(BufferArraySize);
					check(BufferSemantic < BufferArraySize);

					if(BufferArraySize == -1)
					{
						// Failed to find array, move to next format
						continue;
					}

					BufferArrayHandle.set(BufferSemantic);

					NvParameterized::Handle DataHandle(BufferArrayHandle);
					NvParameterized::Interface* BufferInterface = nullptr;
					BufferArrayHandle.getParamRef(BufferInterface);

					check(BufferInterface);

					NvParameterized::findParam(*BufferInterface, ClothingFactoryConstants::ParamName_BufferData, DataHandle);

					if(!DataHandle.isValid())
					{
						// No data array, move to next format
						continue;
					}

					int32 DataArraySize = -1;
					DataHandle.getArraySize(DataArraySize);

					float MaxU = -MAX_flt;
					float MaxV = -MAX_flt;
					for(int32 DataIdx = 0; DataIdx < DataArraySize; ++DataIdx)
					{
						// Push to data entry
						DataHandle.set(DataIdx);

						// UV coord storage
						float Coord[2];

						DataHandle.set(0); // Inside data entry, get first element (U Coord)
						DataHandle.getParamF32(Coord[0]);
						DataHandle.popIndex(); // Back out to data entry

						DataHandle.set(1); // Inside data entry, get second element (V Coord)
						DataHandle.getParamF32(Coord[1]);
						DataHandle.popIndex(); // Back out to data entry

						MaxU = FMath::Max(MaxU, Coord[0] - DELTA);
						MaxV = FMath::Max(MaxV, Coord[1] - DELTA);

						DataHandle.popIndex(); // Back out to data array
					}

					MaxU = FMath::FloorToFloat(MaxU) + 1.0f;
					MaxV = FMath::FloorToFloat(MaxV) + 1.0f;

					for(int32 DataIdx = 0; DataIdx < DataArraySize; ++DataIdx)
					{
						DataHandle.set(DataIdx);

						float CoordPart = 0.0f;

						if(bFlipU)
						{
							DataHandle.set(0);
							DataHandle.getParamF32(CoordPart);
							DataHandle.setParamF32(MaxU - CoordPart);
							DataHandle.popIndex();
						}

						if(bFlipV)
						{
							DataHandle.set(1);
							DataHandle.getParamF32(CoordPart);
							DataHandle.setParamF32(MaxV - CoordPart);
							DataHandle.popIndex();
						}

						DataHandle.popIndex();
					}
				}
			}
		}
	}
}

void UClothingAssetFactory::ExtractBoneData(UClothingAsset* NewAsset, ClothingAsset &InApexAsset)
{
	const uint32 NumApexUsedBones = InApexAsset.getNumUsedBones();

	NewAsset->UsedBoneNames.Empty(NumApexUsedBones);

	for(uint32 BoneIdx = 0; BoneIdx < NumApexUsedBones; ++BoneIdx)
	{
		FString BoneName = FString(InApexAsset.getBoneName(BoneIdx)).Replace(TEXT(" "), TEXT("-"));
		NewAsset->UsedBoneNames.Add(*BoneName);
	}
}

void UClothingAssetFactory::ExtractSphereCollisions(UClothingAsset* NewAsset, nvidia::apex::ClothingAsset &InApexAsset, int32 InLodIdx, FClothLODData &InLodData)
{
	const NvParameterized::Interface* AssetParams = InApexAsset.getAssetNvParameterized();

	NvParameterized::Handle BoneSphereHandle(*AssetParams, "boneSpheres");

	int32 NumBoneSpheres = 0;
	BoneSphereHandle.getArraySize(NumBoneSpheres);

	FClothCollisionData& CollisionData = InLodData.CollisionData;
	CollisionData.Spheres.AddDefaulted(NumBoneSpheres);

	// Load the bone spheres
	for(int32 BoneSphereIndex = 0; BoneSphereIndex < NumBoneSpheres; ++BoneSphereIndex)
	{
		FClothCollisionPrim_Sphere& CurrentSphere = CollisionData.Spheres[BoneSphereIndex];

		BoneSphereHandle.set(BoneSphereIndex);

		NvParameterized::Handle ChildHandle(BoneSphereHandle);

		BoneSphereHandle.getChildHandle(BoneSphereHandle.getInterface(), "boneIndex", ChildHandle);
		ChildHandle.getParamI32(CurrentSphere.BoneIndex);

		BoneSphereHandle.getChildHandle(BoneSphereHandle.getInterface(), "radius", ChildHandle);
		ChildHandle.getParamF32(CurrentSphere.Radius);

		BoneSphereHandle.getChildHandle(BoneSphereHandle.getInterface(), "localPos", ChildHandle);
		physx::PxVec3 PxLocalPos;
		ChildHandle.getParamVec3(PxLocalPos);
		CurrentSphere.LocalPosition = P2UVector(PxLocalPos);

		BoneSphereHandle.popIndex();
	}

	// Next load "connections". A connection is used to turn 2 spheres into a capsule by connecting them
	NvParameterized::Handle BoneSphereConnectionHandle(*AssetParams, "boneSphereConnections");

	int32 NumConnections = 0;
	BoneSphereConnectionHandle.getArraySize(NumConnections);
	check(NumConnections % 2 == 0); // Needs to be even
	CollisionData.SphereConnections.AddDefaulted(NumConnections / 2);

	for(int32 ConnectionIndex = 0; ConnectionIndex < NumConnections; ConnectionIndex += 2)
	{
		FClothCollisionPrim_SphereConnection& CurrentConnection = CollisionData.SphereConnections[ConnectionIndex / 2];

		uint16 FirstSphereIndex;
		uint16 SecondSphereIndex;

		BoneSphereConnectionHandle.set(ConnectionIndex);
		BoneSphereConnectionHandle.getParamU16(FirstSphereIndex);
		BoneSphereConnectionHandle.popIndex();

		BoneSphereConnectionHandle.set(ConnectionIndex + 1);
		BoneSphereConnectionHandle.getParamU16(SecondSphereIndex);
		BoneSphereConnectionHandle.popIndex();

		CurrentConnection.SphereIndices[0] = FirstSphereIndex;
		CurrentConnection.SphereIndices[1] = SecondSphereIndex;
	}

	// Load boneactors. Bone actors are a different way to handle capsules
	// By defining a capsule height and radius.
	NvParameterized::Handle BoneActorHandle(*AssetParams, "boneActors");

	int32 NumActors = 0;
	BoneActorHandle.getArraySize(NumActors);
	
	for(int32 ActorIndex = 0; ActorIndex < NumActors; ++ActorIndex)
	{
		BoneActorHandle.set(ActorIndex);

		NvParameterized::Handle ChildHandle(BoneActorHandle);
		BoneActorHandle.getChildHandle(BoneActorHandle.getInterface(), "convexVerticesCount", ChildHandle);

		uint32 NumConvexVerts = 0;
		ChildHandle.getParamU32(NumConvexVerts);

		if(NumConvexVerts > 0)
		{
			// Convex mesh, extract the data
		}
		else
		{
			int32 BoneIndex;
			float Radius;
			float Height;
			physx::PxMat44 PxPoseMatrix;
			FMatrix PoseMatrix;

			BoneActorHandle.getChildHandle(BoneActorHandle.getInterface(), "boneIndex", ChildHandle);
			ChildHandle.getParamI32(BoneIndex);

			BoneActorHandle.getChildHandle(BoneActorHandle.getInterface(), "capsuleRadius", ChildHandle);
			ChildHandle.getParamF32(Radius);

			BoneActorHandle.getChildHandle(BoneActorHandle.getInterface(), "capsuleHeight", ChildHandle);
			ChildHandle.getParamF32(Height);

			BoneActorHandle.getChildHandle(BoneActorHandle.getInterface(), "localPose", ChildHandle);
			ChildHandle.getParamMat44(PxPoseMatrix);

			PoseMatrix = P2UMatrix(PxPoseMatrix);

			FVector HalfVector(0.0f, Height * 0.5f, 0.0f);
			FVector Sphere0Position = PoseMatrix.TransformPosition(HalfVector);
			FVector Sphere1Position = PoseMatrix.TransformPosition(-HalfVector);

			CollisionData.Spheres.AddDefaulted(2);
			FClothCollisionPrim_Sphere& Sphere0 = CollisionData.Spheres.Last(1);
			FClothCollisionPrim_Sphere& Sphere1 = CollisionData.Spheres.Last(0);

			Sphere0.LocalPosition = Sphere0Position;
			Sphere0.Radius = Radius;
			Sphere0.BoneIndex = BoneIndex;

			Sphere1.LocalPosition = Sphere1Position;
			Sphere1.Radius = Radius;
			Sphere1.BoneIndex = BoneIndex;

			CollisionData.SphereConnections.AddDefaulted();
			FClothCollisionPrim_SphereConnection& Connection = CollisionData.SphereConnections.Last();

			Connection.SphereIndices[0] = CollisionData.Spheres.Num() - 2;
			Connection.SphereIndices[1] = CollisionData.Spheres.Num() - 1;
		}

		BoneActorHandle.popIndex();
	}
}

void UClothingAssetFactory::ExtractMaterialParameters(UClothingAsset* NewAsset, nvidia::apex::ClothingAsset &InApexAsset)
{
	const NvParameterized::Interface* AssetParams = InApexAsset.getAssetNvParameterized();

	uint32 MaterialIndex = INDEX_NONE;
	NvParameterized::getParamU32(*AssetParams, "materialIndex", MaterialIndex);

	NvParameterized::Interface* MaterialLibraryParams = nullptr;
	NvParameterized::getParamRef(*AssetParams, "materialLibrary", MaterialLibraryParams);

	NvParameterized::Handle MaterialArrayHandle(*MaterialLibraryParams);
	MaterialLibraryParams->getParameterHandle("materials", MaterialArrayHandle);
	
	int32 NumMaterials = INDEX_NONE;
	MaterialArrayHandle.getArraySize(NumMaterials);

	check(MaterialIndex < (uint32)NumMaterials);

	MaterialArrayHandle.set(MaterialIndex);

	{
		FClothConfig& Config = NewAsset->ClothConfig;
		NvParameterized::Handle ChildHandle(MaterialArrayHandle);

		// Read out material params
		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "selfcollisionThickness", ChildHandle);
		ChildHandle.getParamF32(Config.SelfCollisionRadius);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "selfcollisionStiffness", ChildHandle);
		ChildHandle.getParamF32(Config.SelfCollisionStiffness);

		float ApexDamping;
		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "damping", ChildHandle);
		ChildHandle.getParamF32(ApexDamping);
		Config.Damping = FVector(ApexDamping);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "friction", ChildHandle);
		ChildHandle.getParamF32(Config.Friction);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "gravityScale", ChildHandle);
		ChildHandle.getParamF32(Config.GravityScale);

		// Tether parameters

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "tetherLimit", ChildHandle);
		ChildHandle.getParamF32(Config.TetherLimit);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "tetherStiffness", ChildHandle);
		ChildHandle.getParamF32(Config.TetherStiffness);

		// Drag and inertia have 2 components but APEX only uses one
		float Drag = 1.0f;
		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "drag", ChildHandle);
		ChildHandle.getParamF32(Drag);
		Config.LinearDrag = FVector(Drag);
		Config.AngularDrag = FVector(Drag);

		float InertiaScale = 1.0f;
		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "inertiaScale", ChildHandle);
		ChildHandle.getParamF32(InertiaScale);
		Config.LinearInertiaScale = FVector(InertiaScale);
		Config.AngularInertiaScale = FVector(InertiaScale);

		// Simulation frequencies
		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "stiffnessFrequency", ChildHandle);
		ChildHandle.getParamF32(Config.StiffnessFrequency);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "solverFrequency", ChildHandle);
		ChildHandle.getParamF32(Config.SolverFrequency);

		// Vertical constraint params

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "verticalStretchingStiffness", ChildHandle);
		ChildHandle.getParamF32(Config.VerticalConstraintConfig.Stiffness);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "verticalStiffnessScaling.compressionRange", ChildHandle);
		ChildHandle.getParamF32(Config.VerticalConstraintConfig.CompressionLimit);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "verticalStiffnessScaling.stretchRange", ChildHandle);
		ChildHandle.getParamF32(Config.VerticalConstraintConfig.StretchLimit);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "verticalStiffnessScaling.scale", ChildHandle);
		ChildHandle.getParamF32(Config.VerticalConstraintConfig.StiffnessMultiplier);

		// Horizontal constraint params

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "horizontalStretchingStiffness", ChildHandle);
		ChildHandle.getParamF32(Config.HorizontalConstraintConfig.Stiffness);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "horizontalStiffnessScaling.compressionRange", ChildHandle);
		ChildHandle.getParamF32(Config.HorizontalConstraintConfig.CompressionLimit);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "horizontalStiffnessScaling.stretchRange", ChildHandle);
		ChildHandle.getParamF32(Config.HorizontalConstraintConfig.StretchLimit);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "horizontalStiffnessScaling.scale", ChildHandle);
		ChildHandle.getParamF32(Config.HorizontalConstraintConfig.StiffnessMultiplier);

		// Bend constraint params

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "bendingStiffness", ChildHandle);
		ChildHandle.getParamF32(Config.BendConstraintConfig.Stiffness);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "bendingStiffnessScaling.compressionRange", ChildHandle);
		ChildHandle.getParamF32(Config.BendConstraintConfig.CompressionLimit);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "bendingStiffnessScaling.stretchRange", ChildHandle);
		ChildHandle.getParamF32(Config.BendConstraintConfig.StretchLimit);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "bendingStiffnessScaling.scale", ChildHandle);
		ChildHandle.getParamF32(Config.BendConstraintConfig.StiffnessMultiplier);

		// Shear constraint params

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "shearingStiffness", ChildHandle);
		ChildHandle.getParamF32(Config.ShearConstraintConfig.Stiffness);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "shearingStiffnessScaling.compressionRange", ChildHandle);
		ChildHandle.getParamF32(Config.ShearConstraintConfig.CompressionLimit);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "shearingStiffnessScaling.stretchRange", ChildHandle);
		ChildHandle.getParamF32(Config.ShearConstraintConfig.StretchLimit);

		MaterialArrayHandle.getChildHandle(MaterialArrayHandle.getInterface(), "shearingStiffnessScaling.scale", ChildHandle);
		ChildHandle.getParamF32(Config.ShearConstraintConfig.StiffnessMultiplier);

		// UE just used the vertical config for everything, so stomp the other configs
		Config.HorizontalConstraintConfig.CompressionLimit = Config.VerticalConstraintConfig.CompressionLimit;
		Config.HorizontalConstraintConfig.StretchLimit = Config.VerticalConstraintConfig.StretchLimit;
		Config.HorizontalConstraintConfig.StiffnessMultiplier = Config.VerticalConstraintConfig.StiffnessMultiplier;

		Config.BendConstraintConfig.CompressionLimit = Config.VerticalConstraintConfig.CompressionLimit;
		Config.BendConstraintConfig.StretchLimit = Config.VerticalConstraintConfig.StretchLimit;
		Config.BendConstraintConfig.StiffnessMultiplier = Config.VerticalConstraintConfig.StiffnessMultiplier;

		Config.ShearConstraintConfig.CompressionLimit = Config.VerticalConstraintConfig.CompressionLimit;
		Config.ShearConstraintConfig.StretchLimit = Config.VerticalConstraintConfig.StretchLimit;
		Config.ShearConstraintConfig.StiffnessMultiplier = Config.VerticalConstraintConfig.StiffnessMultiplier;

	}
}

bool UClothingAssetFactory::ImportToLodInternal(USkeletalMesh* SourceMesh, int32 SourceLodIndex, int32 SourceSectionIndex, UClothingAsset* DestAsset, FClothLODData& DestLod, FClothLODData* InParameterRemapSource)
{
	if(!SourceMesh || !SourceMesh->GetImportedResource())
	{
		// Invalid mesh
		return false;
	}

	FSkeletalMeshResource* SkeletalResource = SourceMesh->GetImportedResource();

	if(!SkeletalResource->LODModels.IsValidIndex(SourceLodIndex))
	{
		// Invalid LOD
		return false;
	}

	FStaticLODModel& SourceLod = SkeletalResource->LODModels[SourceLodIndex];

	if(!SourceLod.Sections.IsValidIndex(SourceSectionIndex))
	{
		// Invalid Section
		return false;
	}

	FSkelMeshSection& SourceSection = SourceLod.Sections[SourceSectionIndex];

	const int32 NumVerts = SourceSection.SoftVertices.Num();
	const int32 NumIndices = SourceSection.NumTriangles * 3;
	const int32 BaseIndex = SourceSection.BaseIndex;
	const int32 BaseVertexIndex = SourceSection.BaseVertexIndex;

	// We need to weld the mesh verts to get rid of duplicates (happens for smoothing groups)
	TArray<FVector> UniqueVerts;
	TArray<uint32> OriginalIndexes;

	TArray<uint32> IndexRemap;
	IndexRemap.AddDefaulted(NumVerts);
	{
		float ThreshSq = SMALL_NUMBER * SMALL_NUMBER;

		for(int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
		{
			const FSoftSkinVertex& SourceVert = SourceSection.SoftVertices[VertIndex];

			bool bUnique = true;
			int32 RemapIndex = INDEX_NONE;

			const int32 NumUniqueVerts = UniqueVerts.Num();
			for(int32 UniqueVertIndex = 0; UniqueVertIndex < NumUniqueVerts; ++UniqueVertIndex)
			{
				FVector& UniqueVert = UniqueVerts[UniqueVertIndex];

				if((UniqueVert - SourceVert.Position).SizeSquared() <= ThreshSq)
				{
					// Not unique
					bUnique = false;
					RemapIndex = UniqueVertIndex;

					break;
				}
			}

			if(bUnique)
			{
				// Unique
				UniqueVerts.Add(SourceVert.Position);
				OriginalIndexes.Add(VertIndex);
				IndexRemap[VertIndex] = UniqueVerts.Num() - 1;
			}
			else
			{
				IndexRemap[VertIndex] = RemapIndex;
			}
		}
	}

	const int32 NumUniqueVerts = UniqueVerts.Num();

	// If we're going to remap the parameters we need to cache the remap source
	// data. We copy it here incase the destination and remap source
	// lod models are aliased (as in a reimport)
	TArray<FVector> CachedPositions;
	TArray<FVector> CachedNormals;
	TArray<uint32> CachedIndices;
	int32 NumSourceMasks = 0;
	TArray<FClothParameterMask_PhysMesh> SourceMaskCopy;
	
	bool bPerformParamterRemap = false;

	if(InParameterRemapSource)
	{
		FClothPhysicalMeshData& RemapPhysMesh = InParameterRemapSource->PhysicalMeshData;
		CachedPositions = RemapPhysMesh.Vertices;
		CachedNormals = RemapPhysMesh.Normals;
		CachedIndices = RemapPhysMesh.Indices;
		SourceMaskCopy = InParameterRemapSource->ParameterMasks;
		NumSourceMasks = SourceMaskCopy.Num();

		bPerformParamterRemap = true;
	}

	FClothPhysicalMeshData& PhysMesh = DestLod.PhysicalMeshData;
	PhysMesh.Reset(NumUniqueVerts);
	PhysMesh.Indices.Reset(NumIndices);
	PhysMesh.Indices.AddZeroed(NumIndices);

	for(int32 VertexIndex = 0; VertexIndex < NumUniqueVerts; ++VertexIndex)
	{
		const FSoftSkinVertex& SourceVert = SourceSection.SoftVertices[OriginalIndexes[VertexIndex]];

		PhysMesh.Vertices[VertexIndex] = SourceVert.Position;
		PhysMesh.Normals[VertexIndex] = SourceVert.TangentZ;
		PhysMesh.MaxDistances[VertexIndex] = 0.0f;

		PhysMesh.BackstopRadiuses[VertexIndex] = 0.0f;
		PhysMesh.BackstopDistances[VertexIndex] = 0.0f;

		FClothVertBoneData& BoneData = PhysMesh.BoneData[VertexIndex];
		for(int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
		{
			const uint16 SourceIndex = SourceSection.BoneMap[SourceVert.InfluenceBones[InfluenceIndex]];

			if(SourceIndex != INDEX_NONE)
			{
				FName BoneName = SourceMesh->RefSkeleton.GetBoneName(SourceIndex);
				BoneData.BoneIndices[InfluenceIndex] = DestAsset->UsedBoneNames.AddUnique(BoneName);
				BoneData.BoneWeights[InfluenceIndex] = (float)SourceVert.InfluenceWeights[InfluenceIndex] / 255.0f;
			}
		}
	}

	// Add a max distance parameter mask to begin with
	DestLod.ParameterMasks.AddDefaulted();
	FClothParameterMask_PhysMesh& Mask = DestLod.ParameterMasks.Last();
	Mask.CopyFromPhysMesh(PhysMesh, MaskTarget_PhysMesh::MaxDistance);
	Mask.bEnabled = true;

	PhysMesh.MaxBoneWeights = SourceSection.MaxBoneInfluences;

	FMultiSizeIndexContainerData IndexData;
	SourceLod.MultiSizeIndexContainer.GetIndexBufferData(IndexData);
	for(int32 IndexIndex = 0; IndexIndex < NumIndices; ++IndexIndex)
	{
		PhysMesh.Indices[IndexIndex] = IndexData.Indices[BaseIndex + IndexIndex] - BaseVertexIndex;
		PhysMesh.Indices[IndexIndex] = IndexRemap[PhysMesh.Indices[IndexIndex]];
	}

	// Validate the generated triangles. If the source mesh has colinear triangles then clothing simulation will fail
	const int32 NumTriangles = PhysMesh.Indices.Num() / 3;
	for(int32 TriIndex = 0; TriIndex < NumTriangles; ++TriIndex)
	{
		FVector A = PhysMesh.Vertices[PhysMesh.Indices[TriIndex * 3 + 0]];
		FVector B = PhysMesh.Vertices[PhysMesh.Indices[TriIndex * 3 + 1]];
		FVector C = PhysMesh.Vertices[PhysMesh.Indices[TriIndex * 3 + 2]];

		FVector TriNormal = (B - A) ^ (C - A);
		if(TriNormal.SizeSquared() <= SMALL_NUMBER)
		{
			// This triangle is colinear
			LogAndToastWarning(FText::Format(LOCTEXT("Colinear_Error", "Failed to generate clothing sim mesh due to degenerate triangle, found conincident vertices in triangle A={0} B={1} C={2}"), FText::FromString(A.ToString()), FText::FromString(B.ToString()), FText::FromString(C.ToString())));

			return false;
		}
	}

	if(bPerformParamterRemap)
	{
		ClothingMeshUtils::FVertexParameterMapper ParameterRemapper(PhysMesh.Vertices, PhysMesh.Normals, CachedPositions, CachedNormals, CachedIndices);

		DestLod.ParameterMasks.Reset(NumSourceMasks);

		for(int32 MaskIndex = 0; MaskIndex < NumSourceMasks; ++MaskIndex)
		{
			FClothParameterMask_PhysMesh& SourceMask = SourceMaskCopy[MaskIndex];

			DestLod.ParameterMasks.AddDefaulted();
			FClothParameterMask_PhysMesh& DestMask = DestLod.ParameterMasks.Last();

			DestMask.Initialize(PhysMesh);
			DestMask.CurrentTarget = SourceMask.CurrentTarget;
			DestMask.bEnabled = SourceMask.bEnabled;

			ParameterRemapper.Map(SourceMask.GetValueArray(), DestMask.Values);
		}
	}

	return true;
}

void UClothingAssetFactory::ExtractLodPhysicalData(UClothingAsset* NewAsset, ClothingAsset &InApexAsset, int32 InLodIdx, FClothLODData &InLodData, TArray<FApexVertData>& OutApexVertData)
{
	const NvParameterized::Interface* AssetParams = InApexAsset.getAssetNvParameterized();

	FClothPhysicalMeshData& PhysData = InLodData.PhysicalMeshData;

	NvParameterized::Handle GraphicalMeshArrayHandle(*AssetParams, "graphicalLods");

	int32 NumGraphicalLods = 0;
	GraphicalMeshArrayHandle.getArraySize(NumGraphicalLods);

	uint32 PhysicalMeshIndex = INDEX_NONE;

	for(int32 GraphicalMeshIndex = 0; GraphicalMeshIndex < NumGraphicalLods; ++GraphicalMeshIndex)
	{
		NvParameterized::Handle GraphicalMeshHandle(GraphicalMeshArrayHandle);
		GraphicalMeshArrayHandle.getChildHandle(GraphicalMeshIndex, GraphicalMeshHandle);

		NvParameterized::Interface* MeshInterface = nullptr;
		GraphicalMeshHandle.getParamRef(MeshInterface);

		NvParameterized::Handle MeshPropertyHandle(MeshInterface);

		MeshInterface->getParameterHandle("lod", MeshPropertyHandle);

		uint32 MeshLodIndex = INDEX_NONE;
		MeshPropertyHandle.getParamU32(MeshLodIndex);

		if(MeshLodIndex == InLodIdx)
		{
			// This is the LOD we want
			MeshInterface->getParameterHandle("physicalMeshId", MeshPropertyHandle);
			MeshPropertyHandle.getParamU32(PhysicalMeshIndex);
		}
	}

	check(PhysicalMeshIndex != INDEX_NONE);

	NvParameterized::Handle PhysicalMeshArrayHandle(*AssetParams, "physicalMeshes");

	int32 NumPhysicalMeshes = 0;
	PhysicalMeshArrayHandle.getArraySize(NumPhysicalMeshes);

	check(PhysicalMeshIndex < (uint32)NumPhysicalMeshes)

	{
		NvParameterized::Handle PhysMeshHandle(PhysicalMeshArrayHandle);
		PhysicalMeshArrayHandle.getChildHandle(PhysicalMeshIndex, PhysMeshHandle);

		NvParameterized::Interface* PhysicalMeshRef = nullptr;
		PhysMeshHandle.getParamRef(PhysicalMeshRef);

		NvParameterized::Handle TempHandle(PhysicalMeshRef);

		uint32 NumVertices = 0;
		uint32 NumIndices = 0;
		TempHandle.getParameter("physicalMesh.numVertices");
		TempHandle.getParam(NumVertices);

		TempHandle.getParameter("physicalMesh.numIndices");
		TempHandle.getParam(NumIndices);

		PhysData.Vertices.Empty();
		PhysData.Normals.Empty();
		PhysData.Vertices.AddUninitialized(NumVertices);
		PhysData.Normals.AddUninitialized(NumVertices);

		// Extract verts
		TempHandle.getParameter("physicalMesh.vertices");
		int32 VertArraySize = 0;
		TempHandle.getArraySize(VertArraySize);

		check(VertArraySize == NumVertices);

		NvParameterized::Handle IterHandle(TempHandle);
		for(int32 Idx = 0; Idx < VertArraySize; ++Idx)
		{
			TempHandle.getChildHandle(Idx, IterHandle);

			PxVec3 PxPosition;
			IterHandle.getParamVec3(PxPosition);

			PhysData.Vertices[Idx] = P2UVector(PxPosition);
		}

		// Extract normals
		TempHandle.getParameter("physicalMesh.normals");
		int32 NormalArraySize = 0;
		TempHandle.getArraySize(NormalArraySize);

		check(NormalArraySize == NumVertices);

		IterHandle = NvParameterized::Handle(TempHandle);
		for(int32 Idx = 0; Idx < VertArraySize; ++Idx)
		{
			TempHandle.getChildHandle(Idx, IterHandle);

			PxVec3 PxNormal;
			IterHandle.getParamVec3(PxNormal);

			PhysData.Normals[Idx] = P2UVector(PxNormal);
		}

		// Extract indices
		TempHandle.getParameter("physicalMesh.indices");
		int32 IndexArraySize = 0;
		TempHandle.getArraySize(IndexArraySize);

		PhysData.Indices.AddZeroed(IndexArraySize);

		IterHandle = NvParameterized::Handle(TempHandle);
		for(int32 Idx = 0; Idx < IndexArraySize; ++Idx)
		{
			TempHandle.getChildHandle(Idx, IterHandle);

			PxU32 Index;
			IterHandle.getParamU32(Index);

			PhysData.Indices[Idx] = Index;
		}

		// Bone data
		NvParameterized::Handle IndexHandle(TempHandle);

		TempHandle.getParameter("physicalMesh.boneWeights");
		IndexHandle.getParameter("physicalMesh.boneIndices");

		int32 BoneWeightArraySize = 0;
		TempHandle.getArraySize(BoneWeightArraySize);

		if(BoneWeightArraySize > 0)
		{
			int32 BoneIndexArraySize = 0;
			IndexHandle.getArraySize(BoneIndexArraySize);
			check(BoneIndexArraySize == BoneWeightArraySize);

			int32 MaxWeights = BoneWeightArraySize / PhysData.Vertices.Num();

			PhysData.MaxBoneWeights = MaxWeights;
			PhysData.BoneData.AddZeroed(PhysData.Vertices.Num());

			// Allocate apex only data
			OutApexVertData.AddDefaulted(PhysData.Vertices.Num());

			NvParameterized::Handle WeightChildHandle(TempHandle);
			NvParameterized::Handle IndexChildHandle(IndexHandle);

			for(int32 WeightIdx = 0; WeightIdx < BoneWeightArraySize; ++WeightIdx)
			{
				TempHandle.getChildHandle(WeightIdx, WeightChildHandle);
				IndexHandle.getChildHandle(WeightIdx, IndexChildHandle);

				int32 VertIdx = WeightIdx / MaxWeights;
				int32 VertWeightIdx = WeightIdx % MaxWeights;

				if(VertWeightIdx < MAX_TOTAL_INFLUENCES)
				{
					WeightChildHandle.getParamF32(PhysData.BoneData[VertIdx].BoneWeights[VertWeightIdx]);
					IndexChildHandle.getParamU16(OutApexVertData[VertIdx].BoneIndices[VertWeightIdx]);
				}
				else
				{
					UE_LOG(LogClothingAssetFactory, Warning, TEXT("Warning, encountered a bone influence index greater than %d, skipping this influence."), MAX_TOTAL_INFLUENCES);
				}
			}
		}
		
		// Extract max distances and backstops
		TempHandle.getParameter("physicalMesh.constrainCoefficients");
		int32 CoeffArraySize = 0;
		TempHandle.getArraySize(CoeffArraySize);

		check(CoeffArraySize == NumVertices);

		PhysData.MaxDistances.AddZeroed(CoeffArraySize);
		PhysData.BackstopDistances.AddZeroed(CoeffArraySize);
		PhysData.BackstopRadiuses.AddZeroed(CoeffArraySize);

		IterHandle = NvParameterized::Handle(TempHandle);
		NvParameterized::Handle ChildHandle(PhysicalMeshRef);
		for(int32 Idx = 0; Idx < CoeffArraySize; ++Idx)
		{
			TempHandle.getChildHandle(Idx, IterHandle);

			IterHandle.getChildHandle(PhysicalMeshRef, "maxDistance", ChildHandle);
			ChildHandle.getParamF32(PhysData.MaxDistances[Idx]);

			IterHandle.getChildHandle(AssetParams, "collisionSphereDistance", ChildHandle);
			ChildHandle.getParamF32(PhysData.BackstopDistances[Idx]);

			IterHandle.getChildHandle(AssetParams, "collisionSphereRadius", ChildHandle);
			ChildHandle.getParamF32(PhysData.BackstopRadiuses[Idx]);

			PhysData.BackstopDistances[Idx] += PhysData.BackstopRadiuses[Idx];
		}

		// Calculate how many fixed verts we have
		PhysData.NumFixedVerts = 0;
		for(float Distance : PhysData.MaxDistances)
		{
			if(Distance == 0.0f)
			{
				++PhysData.NumFixedVerts;
			}
		}

		UE_LOG(LogClothingAssetFactory, Log, TEXT("Finished physical mesh import"));
	}
}
#endif

#undef LOCTEXT_NAMESPACE
