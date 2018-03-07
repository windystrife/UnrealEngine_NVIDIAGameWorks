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
#include "Editor.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "FbxImporter.h"
#include "Misc/FbxErrors.h"
#include "HAL/FileManager.h"
#include "Factories/FbxSceneImportFactory.h"
#include "Toolkits/AssetEditorManager.h"
#include "AssetRegistryModule.h"

//Windows dialog popup
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "FbxCompareWindow.h"

//Meshes includes
#include "MeshUtilities.h"
#include "RawMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "GeomFitUtils.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"

//Static mesh includes
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "StaticMeshResources.h"
#include "Factories/FbxStaticMeshImportData.h"

//Skeletal mesh includes
#include "SkelImport.h"
#include "SkeletalMeshTypes.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AnimEncoding.h"
#include "ApexClothingUtils.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Assets/ClothingAsset.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "FbxCompareWindow.h"



#define LOCTEXT_NAMESPACE "FbxPreviewReimport"

using namespace UnFbx;

struct FCreateCompFromFbxArg
{
	FString MeshName;
	bool IsStaticMesh;
	bool IsStaticHasLodGroup;
};

void CreateCompFromSkeletalMesh(USkeletalMesh* SkeletalMesh, FCompMesh &CurrentData)
{
	//Fill the material array
	for (const FSkeletalMaterial &Material : SkeletalMesh->Materials)
	{
		FCompMaterial CompMaterial(Material.MaterialSlotName, Material.ImportedMaterialSlotName);
		CurrentData.CompMaterials.Add(CompMaterial);
	}
		
	//Fill the section topology
	if (SkeletalMesh->GetResourceForRendering())
	{
		CurrentData.CompLods.AddZeroed(SkeletalMesh->GetResourceForRendering()->LODModels.Num());
		//Fill sections data
		for (int32 LodIndex = 0; LodIndex < SkeletalMesh->GetResourceForRendering()->LODModels.Num(); ++LodIndex)
		{
			//Find the LodMaterialMap, which must be use for all LOD except the base
			TArray<int32> LODMaterialMap;
			if(LodIndex > 0 && SkeletalMesh->LODInfo.IsValidIndex(LodIndex))
			{
				const FSkeletalMeshLODInfo &SkeletalMeshLODInfo = SkeletalMesh->LODInfo[LodIndex];
				LODMaterialMap = SkeletalMeshLODInfo.LODMaterialMap;
			}

			const FStaticLODModel &StaticLodModel = SkeletalMesh->GetResourceForRendering()->LODModels[LodIndex];
			CurrentData.CompLods[LodIndex].Sections.AddZeroed(StaticLodModel.Sections.Num());
			for (int32 SectionIndex = 0; SectionIndex < StaticLodModel.Sections.Num(); ++SectionIndex)
			{
				const FSkelMeshSection &SkelMeshSection = StaticLodModel.Sections[SectionIndex];
				int32 MaterialIndex = SkelMeshSection.MaterialIndex;
				if (LodIndex > 0 && LODMaterialMap.IsValidIndex(MaterialIndex))
				{
					MaterialIndex = LODMaterialMap[MaterialIndex];
				}
				CurrentData.CompLods[LodIndex].Sections[SectionIndex].MaterialIndex = MaterialIndex;
			}
		}
	}

	//Fill the skeleton joint
	CurrentData.CompSkeleton.Joints.AddZeroed(SkeletalMesh->RefSkeleton.GetNum());
	for (int JointIndex = 0; JointIndex < CurrentData.CompSkeleton.Joints.Num(); ++JointIndex)
	{
		CurrentData.CompSkeleton.Joints[JointIndex].Name = SkeletalMesh->RefSkeleton.GetBoneName(JointIndex);
		CurrentData.CompSkeleton.Joints[JointIndex].ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(JointIndex);
		int32 ParentIndex = CurrentData.CompSkeleton.Joints[JointIndex].ParentIndex;
		if (CurrentData.CompSkeleton.Joints.IsValidIndex(ParentIndex))
		{
			CurrentData.CompSkeleton.Joints[ParentIndex].ChildIndexes.Add(JointIndex);
		}
	}

	USkeleton* Skeleton = SkeletalMesh->Skeleton;
	if (Skeleton != nullptr && !Skeleton->MergeAllBonesToBoneTree(SkeletalMesh))
	{
		CurrentData.CompSkeleton.bSkeletonFitMesh = false;
	}
}

void CreateCompFromStaticMesh(UStaticMesh* StaticMesh, FCompMesh &CurrentData)
{
	//Fill the material array
	for (const FStaticMaterial &Material : StaticMesh->StaticMaterials)
	{
		FCompMaterial CompMaterial(Material.MaterialSlotName, Material.ImportedMaterialSlotName);
		CurrentData.CompMaterials.Add(CompMaterial);
	}

	//Fill the section topology
	if (StaticMesh->RenderData)
	{
		CurrentData.CompLods.AddZeroed(StaticMesh->RenderData->LODResources.Num());

		//Fill sections data
		for (int32 LodIndex = 0; LodIndex < StaticMesh->RenderData->LODResources.Num(); ++LodIndex)
		{
			//StaticMesh->SectionInfoMap.Get()

			const FStaticMeshLODResources &StaticLodRessources = StaticMesh->RenderData->LODResources[LodIndex];
			CurrentData.CompLods[LodIndex].Sections.AddZeroed(StaticLodRessources.Sections.Num());
			for (int32 SectionIndex = 0; SectionIndex < StaticLodRessources.Sections.Num(); ++SectionIndex)
			{
				const FStaticMeshSection &StaticMeshSection = StaticLodRessources.Sections[SectionIndex];
				int32 MaterialIndex = StaticMeshSection.MaterialIndex;
				if (StaticMesh->SectionInfoMap.IsValidSection(LodIndex, SectionIndex))
				{
					FMeshSectionInfo MeshSectionInfo = StaticMesh->SectionInfoMap.Get(LodIndex, SectionIndex);
					MaterialIndex = MeshSectionInfo.MaterialIndex;
				}
				CurrentData.CompLods[LodIndex].Sections[SectionIndex].MaterialIndex = MaterialIndex;
			}
		}
	}
}

void GetSkeletalMeshCompData(FFbxImporter *FbxImporter, UFbxImportUI* ImportUI, const FCreateCompFromFbxArg &CreateCompFromFbxArg, FCompMesh &FbxData, const USkeletalMesh *SkeletalMeshRef, UObject **PreviewObject)
{
	USkeletalMesh *SkeletalMesh = Cast<USkeletalMesh>(StaticDuplicateObject(SkeletalMeshRef, GetTransientPackage()));

	USkeletalMesh *NewMesh = nullptr;
	if (SkeletalMesh != nullptr)
	{
		NewMesh = FbxImporter->ReimportSkeletalMesh(SkeletalMesh, ImportUI->SkeletalMeshImportData);
		if (NewMesh)
		{
			if (GEditor->IsObjectInTransactionBuffer(NewMesh) || GEditor->IsObjectInTransactionBuffer(SkeletalMesh))
			{
				GEditor->ResetTransaction(LOCTEXT("PreviewReimportSkeletalMeshTransactionReset", "Preview Reimporting a skeletal mesh which was in the undo buffer"));
			}
			CreateCompFromSkeletalMesh(NewMesh, FbxData);
		}
	}

	*PreviewObject = NewMesh != nullptr ? NewMesh : SkeletalMesh;
}

void GetStaticMeshCompData(FFbxImporter *FbxImporter, UFbxImportUI* ImportUI, const FCreateCompFromFbxArg &CreateCompFromFbxArg, FCompMesh &FbxData, const UStaticMesh* StaticMeshRef, UObject **PreviewObject)
{
	UStaticMesh *StaticMesh = Cast<UStaticMesh>(StaticDuplicateObject(StaticMeshRef, GetTransientPackage()));

	UStaticMesh *NewMesh = nullptr;
	if (StaticMesh != nullptr )
	{
		NewMesh = FbxImporter->ReimportStaticMesh(StaticMesh, ImportUI->StaticMeshImportData);
		if (NewMesh)
		{
			FbxImporter->ImportStaticMeshGlobalSockets(NewMesh);
			if (GEditor->IsObjectInTransactionBuffer(NewMesh) || GEditor->IsObjectInTransactionBuffer(StaticMesh))
			{
				GEditor->ResetTransaction(LOCTEXT("PreviewReimportStaticMeshTransactionReset", "Preview Reimporting a static mesh which was in the undo buffer"));
			}
			CreateCompFromStaticMesh(NewMesh, FbxData);
		}
	}

	*PreviewObject = NewMesh != nullptr ? NewMesh : StaticMesh;
}

void CreateCompFromFbxData(FFbxImporter *FbxImporter, UFbxImportUI* ImportUI, const FString& FullPath, FCompMesh &FbxData, const FCreateCompFromFbxArg &CreateCompFromFbxArg, UStaticMesh *StaticMesh, USkeletalMesh *SkeletalMesh, UObject **PreviewObject)
{
	FbxImporter->ImportOptions->bIsReimportPreview = true;
	//////////////////////////////////////////////////////////////////////////
	//Static mesh reimport
	if (CreateCompFromFbxArg.IsStaticMesh)
	{
		GetStaticMeshCompData(FbxImporter, ImportUI, CreateCompFromFbxArg, FbxData, StaticMesh, PreviewObject);
		
	}
	//////////////////////////////////////////////////////////////////////////
	else //Skeletal mesh reimport
	{
		GetSkeletalMeshCompData(FbxImporter, ImportUI, CreateCompFromFbxArg, FbxData, SkeletalMesh, PreviewObject);
	}
	FbxImporter->ImportOptions->bIsReimportPreview = false;
}

void FFbxImporter::FillGeneralFbxFileInformation(void *GeneralInfoPtr)
{
	FGeneralFbxFileInfo &FbxGeneralInfo = *(FGeneralFbxFileInfo*)GeneralInfoPtr;
	FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();

	//Get the UE4 sdk version
	int32 SDKMajor, SDKMinor, SDKRevision;
	FbxManager::GetFileFormatVersion(SDKMajor, SDKMinor, SDKRevision);

	int32 FileMajor, FileMinor, FileRevision;
	Importer->GetFileVersion(FileMajor, FileMinor, FileRevision);

	FString DateVersion = FString(FbxManager::GetVersion(false));
	FbxGeneralInfo.UE4SdkVersion = TEXT("UE4 Sdk Version: ") + FString::FromInt(SDKMajor) + TEXT(".") + FString::FromInt(SDKMinor) + TEXT(".") + FString::FromInt(SDKRevision) + TEXT(" (") + DateVersion + TEXT(")");

	FbxIOFileHeaderInfo *FileHeaderInfo = Importer->GetFileHeaderInfo();
	if (FileHeaderInfo)
	{
		FbxGeneralInfo.ApplicationCreator = TEXT("Creator:    ") + FString(FileHeaderInfo->mCreator.Buffer());
		FbxGeneralInfo.FileVersion = TEXT("Fbx File Version:    ") + FString::FromInt(FileMajor) + TEXT(".") + FString::FromInt(FileMinor) + TEXT(".") + FString::FromInt(FileRevision) + TEXT(" (") + FString::FromInt(FileHeaderInfo->mFileVersion) + TEXT(")");
		FbxGeneralInfo.CreationDate = TEXT("Created Time:    ") + FString::FromInt(FileHeaderInfo->mCreationTimeStamp.mYear) + TEXT("-") + FString::FromInt(FileHeaderInfo->mCreationTimeStamp.mMonth) + TEXT("-") + FString::FromInt(FileHeaderInfo->mCreationTimeStamp.mDay) + TEXT(" (Y-M-D)");
	}
	int32 UpVectorSign = 1;
	FbxAxisSystem::EUpVector UpVector = FileAxisSystem.GetUpVector(UpVectorSign);

	int32 FrontVectorSign = 1;
	FbxAxisSystem::EFrontVector FrontVector = FileAxisSystem.GetFrontVector(FrontVectorSign);


	FbxAxisSystem::ECoordSystem CoordSystem = FileAxisSystem.GetCoorSystem();

	FbxGeneralInfo.AxisSystem = TEXT("File Axis System:    UP: ");
	if (UpVectorSign == -1)
	{
		FbxGeneralInfo.AxisSystem = TEXT("-");
	}
	FbxGeneralInfo.AxisSystem += (UpVector == FbxAxisSystem::EUpVector::eXAxis) ? TEXT("X, Front: ") : (UpVector == FbxAxisSystem::EUpVector::eYAxis) ? TEXT("Y, Front: ") : TEXT("Z, Front: ");
	if (FrontVectorSign == -1)
	{
		FbxGeneralInfo.AxisSystem = TEXT("-");
	}

	if (UpVector == FbxAxisSystem::EUpVector::eXAxis)
	{
		FbxGeneralInfo.AxisSystem += (FrontVector == FbxAxisSystem::EFrontVector::eParityEven) ? TEXT("Y") : TEXT("Z");
	}
	else if (UpVector == FbxAxisSystem::EUpVector::eYAxis)
	{
		FbxGeneralInfo.AxisSystem += (FrontVector == FbxAxisSystem::EFrontVector::eParityEven) ? TEXT("X") : TEXT("Z");
	}
	else if (UpVector == FbxAxisSystem::EUpVector::eZAxis)
	{
		FbxGeneralInfo.AxisSystem += (FrontVector == FbxAxisSystem::EFrontVector::eParityEven) ? TEXT("X") : TEXT("Y");
	}

	//Hand side
	FbxGeneralInfo.AxisSystem += (CoordSystem == FbxAxisSystem::ECoordSystem::eLeftHanded) ? TEXT(" Left Handed") : TEXT(" Right Handed");


	if (FileAxisSystem == FbxAxisSystem::MayaZUp)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Maya ZUp)");
	}
	else if (FileAxisSystem == FbxAxisSystem::MayaYUp)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Maya YUp)");
	}
	else if (FileAxisSystem == FbxAxisSystem::Max)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Max)");
	}
	else if (FileAxisSystem == FbxAxisSystem::Motionbuilder)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Motion Builder)");
	}
	else if (FileAxisSystem == FbxAxisSystem::OpenGL)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (OpenGL)");
	}
	else if (FileAxisSystem == FbxAxisSystem::DirectX)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (DirectX)");
	}
	else if (FileAxisSystem == FbxAxisSystem::Lightwave)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Lightwave)");
	}

	FbxGeneralInfo.UnitSystem = TEXT("Units:    ");
	if (FileUnitSystem == FbxSystemUnit::mm)
	{
		FbxGeneralInfo.UnitSystem += TEXT("mm (millimeter)");
	}
	else if (FileUnitSystem == FbxSystemUnit::cm)
	{
		FbxGeneralInfo.UnitSystem += TEXT("cm (centimeter)");
	}
	else if (FileUnitSystem == FbxSystemUnit::dm)
	{
		FbxGeneralInfo.UnitSystem += TEXT("dm (decimeter)");
	}
	else if (FileUnitSystem == FbxSystemUnit::m)
	{
		FbxGeneralInfo.UnitSystem += TEXT("m (meter)");
	}
	else if (FileUnitSystem == FbxSystemUnit::km)
	{
		FbxGeneralInfo.UnitSystem += TEXT("km (kilometer)");
	}
	else if (FileUnitSystem == FbxSystemUnit::Inch)
	{
		FbxGeneralInfo.UnitSystem += TEXT("Inch");
	}
	else if (FileUnitSystem == FbxSystemUnit::Foot)
	{
		FbxGeneralInfo.UnitSystem += TEXT("Foot");
	}
	else if (FileUnitSystem == FbxSystemUnit::Yard)
	{
		FbxGeneralInfo.UnitSystem += TEXT("Yard");
	}
	else if (FileUnitSystem == FbxSystemUnit::Mile)
	{
		FbxGeneralInfo.UnitSystem += TEXT("Mile");
	}
}

void FFbxImporter::ShowFbxReimportPreview(UObject *ReimportObj, UFbxImportUI* ImportUI, const FString& FullPath)
{
	if (ReimportObj == nullptr || ImportUI == nullptr)
	{
		return;
	}
	//Show a dialog
	UStaticMesh *StaticMesh = Cast<UStaticMesh>(ReimportObj);
	USkeletalMesh *SkeletalMesh = Cast<USkeletalMesh>(ReimportObj);
	UObject *PreviewObject = nullptr;
	FCompMesh CurrentData;
	FCompMesh FbxData;
	FCreateCompFromFbxArg CreateCompFromFbxArg;
	FString Filename;
	//Create the current data to compare from
	if (StaticMesh)
	{
		CreateCompFromStaticMesh(StaticMesh, CurrentData);
		CreateCompFromFbxArg.MeshName = StaticMesh->GetName();
		CreateCompFromFbxArg.IsStaticMesh = true;
		CreateCompFromFbxArg.IsStaticHasLodGroup = StaticMesh->LODGroup != NAME_None;
		Filename = ImportUI->StaticMeshImportData->GetFirstFilename();
	}
	else if (SkeletalMesh)
	{
		CreateCompFromSkeletalMesh(SkeletalMesh, CurrentData);
		CreateCompFromFbxArg.MeshName = SkeletalMesh->GetName();
		CreateCompFromFbxArg.IsStaticMesh = false;
		Filename = ImportUI->SkeletalMeshImportData->GetFirstFilename();
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Set the import options
	ApplyImportUIToImportOptions(ImportUI, *ImportOptions);

	// Set all reimport options
	ImportOptions->bAutoComputeLodDistances = true;
	ImportOptions->LodNumber = 0;
	ImportOptions->MinimumLodNumber = 0;
	ImportOptions->bImportRigidMesh = true;
	ImportOptions->bImportMaterials = false;
	ImportOptions->bImportTextures = false;
	ImportOptions->bImportAnimations = false;

	//////////////////////////////////////////////////////////////////////////
	// Open the fbx file

	const FString FileExtension = FPaths::GetExtension(Filename);
	const bool bIsValidFile = FileExtension.Equals(TEXT("fbx"), ESearchCase::IgnoreCase) || FileExtension.Equals("obj", ESearchCase::IgnoreCase);
	if (!bIsValidFile || !(Filename.Len()) || (IFileManager::Get().FileSize(*Filename) == INDEX_NONE) ||
		!ImportFromFile(*Filename, FPaths::GetExtension(Filename), true))
	{
		return;
	}

	//Query general information
	FGeneralFbxFileInfo FbxGeneralInfo;
	FillGeneralFbxFileInformation(&FbxGeneralInfo);

	//Apply the Transform to the scene
	UFbxAssetImportData* ImportAssetData = nullptr;
	if (CreateCompFromFbxArg.IsStaticMesh)
	{
		ImportAssetData = ImportUI->StaticMeshImportData;
	}
	else
	{
		ImportAssetData = ImportUI->SkeletalMeshImportData;
	}
	ApplyTransformSettingsToFbxNode(Scene->GetRootNode(), ImportAssetData);

	UnFbx::FbxSceneInfo SceneInfo;
	//Read the scene and found all instance with their scene information.
	GetSceneInfo(Filename, SceneInfo, true);
	//Convert old structure to the new scene export structure
	TSharedPtr<FFbxSceneInfo> SceneInfoPtr = UFbxSceneImportFactory::ConvertSceneInfo(this, &SceneInfo);
	//Get import material info
	UFbxSceneImportFactory::ExtractMaterialInfo(this, SceneInfoPtr);

	CreateCompFromFbxData(this, ImportUI, FullPath, FbxData, CreateCompFromFbxArg, StaticMesh, SkeletalMesh, &PreviewObject);
	TArray<TSharedPtr<FString>> AssetReferencingSkeleton;
	if (SkeletalMesh != nullptr && SkeletalMesh->Skeleton != nullptr && !FbxData.CompSkeleton.bSkeletonFitMesh)
	{
		UObject* SelectedObject = SkeletalMesh->Skeleton;
		if (SelectedObject)
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			const FName SelectedPackageName = SelectedObject->GetOutermost()->GetFName();
			//Get the Hard dependencies
			TArray<FName> HardDependencies;
			AssetRegistryModule.Get().GetReferencers(SelectedPackageName, HardDependencies, EAssetRegistryDependencyType::Hard);
			//Get the Soft dependencies
			TArray<FName> SoftDependencies;
			AssetRegistryModule.Get().GetReferencers(SelectedPackageName, SoftDependencies, EAssetRegistryDependencyType::Soft);
			//Compose the All dependencies array
			TArray<FName> AllDependencies = HardDependencies;
			AllDependencies += SoftDependencies;
			if (AllDependencies.Num() > 0)
			{
				for (const FName AssetDependencyName : AllDependencies)
				{
					const FString PackageString = AssetDependencyName.ToString();
					const FString FullAssetPathName = FString::Printf(TEXT("%s.%s"), *PackageString, *FPackageName::GetLongPackageAssetName(PackageString));
					
					FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FName(*FullAssetPathName));
					if (AssetData.GetClass() != nullptr)
					{
						TSharedPtr<FString> AssetReferencing = MakeShareable(new FString(AssetData.AssetClass.ToString() + TEXT(" ") + FullAssetPathName));
						AssetReferencingSkeleton.Add(AssetReferencing);
					}
				}
			}
		}
	}

	//Create the modal dialog window to let the user see the result of the compare
	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("UnrealEd", "FbxMaterialConflictOpionsTitle", "FBX Import Conflict"))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(700, 650))
		.MinWidth(700)
		.MinHeight(650);

	TSharedPtr<SFbxCompareWindow> FbxMaterialConflictWindow;
	Window->SetContent
		(
			SNew(SFbxCompareWindow)
			.WidgetWindow(Window)
			.FullFbxPath(FText::FromString(Filename))
			.FbxSceneInfo(SceneInfoPtr)
			.FbxGeneralInfo(FbxGeneralInfo)
			.AssetReferencingSkeleton(&AssetReferencingSkeleton)
			.CurrentMeshData(&CurrentData)
			.FbxMeshData(&FbxData)
			.PreviewObject(PreviewObject)
			);

	// @todo: we can make this slow as showing progress bar later
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	if (PreviewObject != nullptr)
	{
		FAssetEditorManager::Get().CloseAllEditorsForAsset(PreviewObject);
		PreviewObject->MarkPendingKill();
		PreviewObject = nullptr;
	}
}
#undef LOCTEXT_NAMESPACE
