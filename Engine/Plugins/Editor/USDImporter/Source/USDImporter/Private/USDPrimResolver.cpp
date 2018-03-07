// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "USDPrimResolver.h"
#include "USDImporter.h"
#include "USDConversionUtils.h"
#include "AssetData.h"
#include "ModuleManager.h"
#include "AssetRegistryModule.h"
#include "USDSceneImportFactory.h"
#include "AssetSelection.h"
#include "IUSDImporterModule.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "ActorFactories/ActorFactory.h"

#define LOCTEXT_NAMESPACE "USDImportPlugin"

void UUSDPrimResolver::Init()
{
	AssetRegistry = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
}

void UUSDPrimResolver::FindPrimsToImport(FUsdImportContext& ImportContext, TArray<FUsdPrimToImport>& OutPrimsToImport)
{
	IUsdPrim* RootPrim = ImportContext.RootPrim;

	FindPrimsToImport_Recursive(ImportContext, RootPrim, OutPrimsToImport);
}

void UUSDPrimResolver::FindActorsToSpawn(FUSDSceneImportContext& ImportContext, TArray<FActorSpawnData>& OutActorSpawnDatas)
{
	IUsdPrim* RootPrim = ImportContext.RootPrim;

	if (RootPrim->HasTransform())
	{
		FindActorsToSpawn_Recursive(ImportContext, RootPrim, nullptr, OutActorSpawnDatas);
	}
	else
	{
		for (int32 ChildIdx = 0; ChildIdx < RootPrim->GetNumChildren(); ++ChildIdx)
		{
			IUsdPrim* Child = RootPrim->GetChild(ChildIdx);

			FindActorsToSpawn_Recursive(ImportContext, Child, nullptr, OutActorSpawnDatas);
		}
	}
}

AActor* UUSDPrimResolver::SpawnActor(FUSDSceneImportContext& ImportContext, const FActorSpawnData& SpawnData)
{
	UUSDImporter* USDImporter = IUSDImporterModule::Get().GetImporter();

	UUSDSceneImportOptions* ImportOptions = Cast<UUSDSceneImportOptions>(ImportContext.ImportOptions);

	const bool bFlattenHierarchy = ImportOptions->bFlattenHierarchy;

	AActor* ModifiedActor = nullptr;

	// Look for an existing actor and decide what to do based on the users choice
	AActor* ExistingActor = ImportContext.ExistingActors.FindRef(SpawnData.ActorName);

	bool bShouldSpawnNewActor = true;
	EExistingActorPolicy ExistingActorPolicy = ImportOptions->ExistingActorPolicy;

	const FMatrix& ActorMtx = SpawnData.WorldTransform;

	const FTransform ConversionTransform = ImportContext.ConversionTransform;

	const FTransform ActorTransform = ConversionTransform*FTransform(ActorMtx)*ConversionTransform;

	if (ExistingActor && ExistingActorPolicy == EExistingActorPolicy::UpdateTransform)
	{
		ExistingActor->Modify();
		ModifiedActor = ExistingActor;

		ExistingActor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
		ExistingActor->SetActorRelativeTransform(ActorTransform);

		bShouldSpawnNewActor = false;
	}
	else if (ExistingActor && ExistingActorPolicy == EExistingActorPolicy::Ignore)
	{
		// Ignore this actor and do nothing
		bShouldSpawnNewActor = false;
	}

	if (bShouldSpawnNewActor)
	{
		UActorFactory* ActorFactory = ImportContext.EmptyActorFactory;

		AActor* SpawnedActor = nullptr;

		// The asset which should be used to spawn the actor
		UObject* ActorAsset = nullptr;

		TMap<FString, int32> NameToCount;

		// Note: it is expected that MeshPrim and ActorClassName are mutually exclusive in that if there is a mesh we do not assume we have a custom actor class yet
		if (SpawnData.MeshPrim)
		{
			//SlowTask.EnterProgressFrame(1.0f / PrimsToImport.Num(), FText::Format(LOCTEXT("ImportingUSDMesh", "Importing Mesh {0} of {1}"), MeshCount + 1, PrimsToImport.Num()));

			FString FullPath;

			// If there is no asset path, come up with an asset path for the mesh to be imported
			if (SpawnData.AssetPath.IsEmpty())
			{
				FString MeshName = USDToUnreal::ConvertString(SpawnData.MeshPrim->GetPrimName());
				MeshName = ObjectTools::SanitizeObjectName(MeshName);

				// Generate a custom path to the actor

				if (ImportOptions->bGenerateUniquePathPerUSDPrim)
				{
					FString USDPath = USDToUnreal::ConvertString(SpawnData.ActorPrim->GetPrimPath());
					USDPath.RemoveFromEnd(MeshName);
					FullPath = ImportContext.ImportPathName / USDPath;
				}
				else
				{
					FullPath = ImportContext.ImportPathName;
				}

				// If we're generating unique meshes append a unique number if the mesh has already been found
				if (ImportOptions->bGenerateUniqueMeshes)
				{
					int32* Count = NameToCount.Find(MeshName);
					if (Count)
					{
						MeshName += TEXT("_");
						MeshName.AppendInt(*Count);
						++(*Count);
					}
					else
					{
						NameToCount.Add(MeshName, 1);
					}
				}

				FullPath /= MeshName;
			}
			else
			{
				FullPath = SpawnData.AssetPath;
			}

			ActorAsset = LoadObject<UObject>(nullptr, *FullPath, nullptr, LOAD_NoWarn | LOAD_Quiet);

			// Only import the asset if it doesnt exist || we're allowed to reimport it based on user settings
			const bool bImportAsset = ImportOptions->bImportMeshes && (!ActorAsset || ImportOptions->ExistingAssetPolicy == EExistingAssetPolicy::Reimport);

			if (bImportAsset)
			{
				if(IsValidPathForImporting(FullPath))
				{
					FString NewPackageName = FPackageName::ObjectPathToPackageName(FullPath);

					UPackage* Package = CreatePackage(nullptr, *NewPackageName);
					if (Package)
					{
						Package->FullyLoad();

						ImportContext.Parent = Package;
						ImportContext.ObjectName = FPackageName::GetLongPackageAssetName(Package->GetOutermost()->GetName());

						FUsdPrimToImport PrimToImport;
						PrimToImport.NumLODs = SpawnData.MeshPrim->GetNumLODs();
						PrimToImport.Prim = SpawnData.MeshPrim;

						// Compute a transform so that the mesh ends up in the correct place relative to the actor we are spawning.  This accounts for any skipped prims that should contribute to the final transform
						PrimToImport.CustomPrimTransform = USDToUnreal::ConvertMatrix(SpawnData.MeshPrim->GetLocalToAncestorTransform(SpawnData.ActorPrim));

						ActorAsset = USDImporter->ImportSingleMesh(ImportContext, ImportOptions->MeshImportType, PrimToImport);

						if (ActorAsset)
						{
							FAssetRegistryModule::AssetCreated(ActorAsset);
							Package->MarkPackageDirty();
						}
					}
				}
				else
				{
					ImportContext.AddErrorMessage(
						EMessageSeverity::Error, FText::Format(LOCTEXT("InvalidPathForImporting", "Could not import asset. '{0}' is not a valid path for assets"),
							FText::FromString(FullPath)));
				}
			}

		}
		else if (!SpawnData.ActorClassName.IsEmpty())
		{
			TSubclassOf<AActor> ActorClass = FindActorClass(ImportContext, SpawnData);
			if (ActorClass)
			{
				SpawnedActor = ImportContext.World->SpawnActor<AActor>(ActorClass);
			}
		}
		else if (!SpawnData.AssetPath.IsEmpty())
		{
			ActorAsset = LoadObject<UObject>(nullptr, *SpawnData.AssetPath);
			if (!ActorAsset)
			{
				ImportContext.AddErrorMessage(
					EMessageSeverity::Error, FText::Format(LOCTEXT("CouldNotFindUnrealAssetPath", "Could not find Unreal Asset '{0}' for USD prim '{1}'"),
						FText::FromString(SpawnData.AssetPath),
						FText::FromString(USDToUnreal::ConvertString(SpawnData.ActorPrim->GetPrimPath()))));

				UE_LOG(LogUSDImport, Error, TEXT("Could not find Unreal Asset '%s' for USD prim '%s'"), *SpawnData.AssetPath, *SpawnData.ActorName.ToString());
			}
		}

		if (ActorAsset)
		{
			UClass* AssetClass = ActorAsset->GetClass();

			UActorFactory* Factory = ImportContext.UsedFactories.FindRef(AssetClass);
			if (!Factory)
			{
				Factory = FActorFactoryAssetProxy::GetFactoryForAssetObject(ActorAsset);
				if (Factory)
				{
					ImportContext.UsedFactories.Add(AssetClass, Factory);
				}
				else
				{
					ImportContext.AddErrorMessage(
						EMessageSeverity::Error, FText::Format(LOCTEXT("CouldNotFindActorFactory", "Could not find an actor type to spawn for '{0}'"),
							FText::FromString(ActorAsset->GetName()))
					);
				}
			}

			ActorFactory = Factory;

		}

		if (ActorFactory)
		{
			SpawnedActor = ActorFactory->CreateActor(ActorAsset, ImportContext.World->GetCurrentLevel(), FTransform::Identity, RF_Transactional, SpawnData.ActorName);

			// For empty group actors set their initial mobility to static 
			if (ActorFactory == ImportContext.EmptyActorFactory)
			{
				SpawnedActor->GetRootComponent()->SetMobility(EComponentMobility::Static);
			}
		}

		if(SpawnedActor)
		{
			SpawnedActor->SetActorRelativeTransform(ActorTransform);

			if (SpawnData.AttachParentPrim && !bFlattenHierarchy)
			{
				// Spawned actor should be attached to a parent
				AActor* AttachPrim = PrimToActorMap.FindRef(SpawnData.AttachParentPrim);
				if (AttachPrim)
				{
					SpawnedActor->AttachToActor(AttachPrim, FAttachmentTransformRules::KeepRelativeTransform);
				}
			}

			FActorLabelUtilities::SetActorLabelUnique(SpawnedActor, SpawnData.ActorName.ToString(), &ImportContext.ActorLabels);
			ImportContext.ActorLabels.Add(SpawnedActor->GetActorLabel());
		}


		ModifiedActor = SpawnedActor;
	}

	PrimToActorMap.Add(SpawnData.ActorPrim, ModifiedActor);

	return ModifiedActor;
}


TSubclassOf<AActor> UUSDPrimResolver::FindActorClass(FUSDSceneImportContext& ImportContext, const FActorSpawnData& SpawnData) const
{
	TSubclassOf<AActor> ActorClass = nullptr;

	FString ActorClassName = SpawnData.ActorClassName;
	FName ActorClassFName = *ActorClassName;

	// Attempt to use the fully qualified path first.  If not use the expensive slow path.
	{
		ActorClass = LoadClass<AActor>(nullptr, *ActorClassName, nullptr);
	}

	if (!ActorClass)
	{
		UObject* TestObject = nullptr;
		TArray<FAssetData> AssetDatas;

		AssetRegistry->GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), AssetDatas);

		UClass* TestClass = nullptr;
		for (const FAssetData& AssetData : AssetDatas)
		{
			if (AssetData.AssetName == ActorClassFName)
			{
				TestClass = Cast<UBlueprint>(AssetData.GetAsset())->GeneratedClass;
				break;
			}
		}

		if (TestClass && TestClass->IsChildOf<AActor>())
		{
			ActorClass = TestClass;
		}

		if (!ActorClass)
		{
			ImportContext.AddErrorMessage(
				EMessageSeverity::Error, FText::Format(LOCTEXT("CouldNotFindUnrealActorClass", "Could not find Unreal Actor Class '{0}' for USD prim '{1}'"),
					FText::FromString(ActorClassName),
					FText::FromString(USDToUnreal::ConvertString(SpawnData.ActorPrim->GetPrimPath()))));

		}
	}

	return ActorClass;
}

void UUSDPrimResolver::FindPrimsToImport_Recursive(FUsdImportContext& ImportContext, IUsdPrim* Prim, TArray<FUsdPrimToImport>& OutTopLevelPrims)
{
	const FString PrimName = USDToUnreal::ConvertString(Prim->GetPrimName());

	const FString KindName = USDToUnreal::ConvertString(Prim->GetKind());

	bool bHasUnrealAssetPath = Prim->GetUnrealAssetPath() != nullptr;
	bool bHasUnrealActorClass = Prim->GetUnrealActorClass() != nullptr;

	
	// Ignore prims with unreal asset path or actor class specified as these are custom and should not spawn geometry
	bool bShouldImportGeometry = !bHasUnrealActorClass && !bHasUnrealAssetPath;
	if (bShouldImportGeometry && Prim->HasGeometryData())
	{
		// Note if there are lod's the prim is not expected to have it's own geometry

		//@todo LODs are not expected to have children with geometry 
		FUsdPrimToImport NewPrim;
		NewPrim.NumLODs = Prim->GetNumLODs();
		NewPrim.Prim = Prim;

		OutTopLevelPrims.Add(NewPrim);
	}

	if (!ImportContext.bFindUnrealAssetReferences && Prim->GetNumLODs() == 0)
	{
		// prim has no geometry data or LODs and does not define an unreal asset path (or we arent checking). Look at children
		int32 NumChildren = Prim->GetNumChildren();
		for (int32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
		{
			FindPrimsToImport_Recursive(ImportContext, Prim->GetChild(ChildIdx), OutTopLevelPrims);
		}
	}
}

void UUSDPrimResolver::FindActorsToSpawn_Recursive(FUSDSceneImportContext& ImportContext, IUsdPrim* Prim, IUsdPrim* ParentPrim, TArray<FActorSpawnData>& OutSpawnDatas)
{
	TArray<FActorSpawnData>* SpawnDataArray = &OutSpawnDatas;

	UUSDSceneImportOptions* ImportOptions = Cast<UUSDSceneImportOptions>(ImportContext.ImportOptions);

	FString AssetPath;
	FName ActorClassName;
	if (Prim->HasTransform())
	{
		FActorSpawnData SpawnData;

		if (Prim->GetUnrealActorClass())
		{
			SpawnData.ActorClassName = USDToUnreal::ConvertString(Prim->GetUnrealActorClass());
		}

		if (Prim->GetUnrealAssetPath())
		{
			SpawnData.AssetPath = USDToUnreal::ConvertString(Prim->GetUnrealAssetPath());
		}

		if (Prim->HasGeometryData())
		{
			SpawnData.MeshPrim = Prim;
		}

		FName PrimName = USDToUnreal::ConvertName(Prim->GetPrimName());
		SpawnData.ActorName = PrimName;
		SpawnData.WorldTransform = USDToUnreal::ConvertMatrix(Prim->GetLocalToWorldTransform());
		SpawnData.AttachParentPrim = ParentPrim;
		SpawnData.ActorPrim = Prim;

		if (ImportOptions->ExistingActorPolicy == EExistingActorPolicy::Replace && ImportContext.ExistingActors.Contains(SpawnData.ActorName))
		{
			ImportContext.ActorsToDestroy.Add(SpawnData.ActorName);
		}

		OutSpawnDatas.Add(SpawnData);
	}

	if (!ImportContext.bFindUnrealAssetReferences || AssetPath.IsEmpty())
	{
		for (int32 ChildIdx = 0; ChildIdx < Prim->GetNumChildren(); ++ChildIdx)
		{
			IUsdPrim* Child = Prim->GetChild(ChildIdx);

			FindActorsToSpawn_Recursive(ImportContext, Child, Prim, *SpawnDataArray);
		}
	}
}


bool UUSDPrimResolver::IsValidPathForImporting(const FString& TestPath) const
{
	return FPackageName::GetPackageMountPoint(TestPath) != NAME_None;
}

#undef LOCTEXT_NAMESPACE