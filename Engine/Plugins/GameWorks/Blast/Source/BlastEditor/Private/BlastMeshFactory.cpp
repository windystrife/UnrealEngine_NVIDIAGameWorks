#include "BlastMeshFactory.h"

#include "BlastAsset.h"
#include "BlastMesh.h"
#include "PhysicsPublic.h"
#include "LocalTimestampDirectoryVisitor.h"
#include "Public/FbxImporter.h"
#include "FbxErrors.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "NvBlastExtSerialization.h"
#include "BlastImportUI.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "FeedbackContext.h"
#include "PlatformFilemanager.h"
#include "BlastAssetImportData.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ComponentReregisterContext.h"
#include "BlastCollisionFbxImporter.h"
#include "NvBlastExtAssetUtils.h"
#include "NvBlastGlobals.h"
#include "BlastEditorModule.h"
#include "BlastMeshComponent.h"
#include "BlastDirectoryVisitor.h"
#include "IAssetTools.h"
#include "SkelImport.h"

#define LOCTEXT_NAMESPACE "Blast"

UBlastMeshFactory::UBlastMeshFactory()
{
	ImportUI = CreateDefaultSubobject<UBlastImportUI>(GET_MEMBER_NAME_CHECKED(UBlastMeshFactory, ImportUI));
	bEditorImport = true;
	SupportedClass = UBlastMesh::StaticClass();
	bCreateNew = false;
	Formats.Add(TEXT("blast;Blast Asset"));
}

UObject* UBlastMeshFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	//NOTE: This broadcasts "InName" as opposed to any changed name. No idea what effect this has.
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	// The return value
	UBlastMesh* BlastMesh = nullptr;
	TArray<FString> ReimportFilenames;
	if (bReimporting)
	{
		BlastMesh = ReimportMesh;
		BlastMesh->AssetImportData->ExtractFilenames(ReimportFilenames);
	}

	ImportUI->LoadConfig();

	if (bReimporting)
	{
		check(ReimportFilenames[0] == UFactory::GetCurrentFilename());
		ImportUI->ImportOptions = BlastMesh->AssetImportData->ImportOptions;
		ImportUI->ImportOptions.SkeletalMeshPath.FilePath = ReimportFilenames[1];
	}
	else
	{
		// Find a fbx skeletal mesh side by side with this llasset
		ImportUI->ImportOptions.SkeletalMeshPath.FilePath = GuessFBXPathFromAsset(UFactory::GetCurrentFilename());
		ImportUI->ImportOptions.RootName = InName;

		ReimportFilenames.SetNum(2);
		ReimportFilenames[0] = UFactory::CurrentFilename;
		ReimportFilenames[1] = ImportUI->ImportOptions.SkeletalMeshPath.FilePath;
	}
		

	// Get the current file and turn it into an absolute path
	FString sourceFile = UFactory::GetCurrentFilename();
	FPaths::MakeStandardFilename(sourceFile);

	bool bImport = true;
	if (!bReimporting)
	{
		bImport = ImportUI->GetBlastImportOptions(sourceFile);
	}

	bOutOperationCanceled = !bImport;
	if (bOutOperationCanceled)
	{
		return nullptr;
	}
	
	//TODO: Output name validation!

	auto LoadedAsset = TSharedPtr<NvBlastAsset>(UBlastAsset::DeserializeBlastAsset(Buffer, (uint32_t)(BufferEnd - Buffer)), [](NvBlastAsset* asset)
	{
		NVBLAST_FREE((void*)asset);
	});
		
	if (!LoadedAsset.IsValid())
	{
		FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("BlastAssetWrongFormatImport", "Failed to import {0}. The file you are trying to import is not low-level NvBlastAsset. Blast SDK files with extension .blast could contain different asset types inside. This plugins imports only low-level Blast Asset. Look into docs of the tool you used to export this file for more details."), FText::FromString(UFactory::CurrentFilename)));
		return nullptr;
	}

	TransformBlastAssetToUE4CoordinateSystem(LoadedAsset.Get(), ImportUI->FBXImportUI->SkeletalMeshImportData);
	
	if (BlastMesh == nullptr)
	{
		FName outputName = GetNameFromRoot(ImportUI->ImportOptions.RootName, "");
		BlastMesh = NewObject<UBlastMesh>(InParent, outputName, Flags);
	}
		
	if (BlastMesh->AssetImportData == nullptr)
	{
		BlastMesh->AssetImportData = NewObject<UBlastAssetImportData>(BlastMesh);
	}
	BlastMesh->AssetImportData->ImportOptions = ImportUI->ImportOptions;

	if (BlastMesh->Skeleton == nullptr)
	{
		FName SkeletonName = GetNameFromRoot(ImportUI->ImportOptions.RootName, "_Skeleton");
		BlastMesh->Skeleton = NewObject<USkeleton>(BlastMesh, SkeletonName);
	}
	ImportUI->FBXImportUI->Skeleton = BlastMesh->Skeleton;

	// This will store asset in serialized form.
	BlastMesh->CopyFromLoadedAsset(LoadedAsset.Get());


	// First, must try to either pair or import the skeletal mesh.
	TMap<FName, TArray<FBlastCollisionHull>> hulls;

	if (!ImportUI->FBXImportUI->SkeletalMeshImportData->bConvertScene 
		|| !ImportUI->FBXImportUI->SkeletalMeshImportData->bConvertSceneUnit)
	{
		UnFbx::FFbxImporter::GetInstance()->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("BlastImport_ConvertSceneWarning", "Convert Scene and Convert Scene Unit were not enabled. You may get mismatched Blast and rendering results.")), "BlastImportSettings");
	}
	//We are doing this later
	ImportUI->FBXImportUI->bCreatePhysicsAsset = false;
	ImportUI->FBXImportUI->PhysicsAsset = nullptr;
	ImportUI->FBXImportUI->bImportAnimations = false;

	//Don't pass flags on, we want to inherit from the outer like a normal object
	//USkeletalMesh* NewSkelMesh = ImportSkeletalMesh(ImportUI->ImportOptions.SkeletalMeshPath.FilePath, BlastMesh, ImportUI->ImportOptions.RootName, RF_NoFlags, TEXT("fbx"), Warn, hulls);
	FName skelMeshName = BlastMesh && BlastMesh->Mesh ? BlastMesh->Mesh->GetFName() : GetNameFromRoot(ImportUI->ImportOptions.RootName, "_SkelMesh");
	if (BlastMesh && BlastMesh->Mesh)
	{
		//If reimporting use the saved settings
		UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(BlastMesh->Mesh->AssetImportData);
		ImportUI->FBXImportUI->SkeletalMeshImportData = ImportData;
	}
	
	USkeletalMesh* NewSkelMesh = ImportSkeletalMesh(BlastMesh, skelMeshName, ImportUI->ImportOptions.SkeletalMeshPath.FilePath, 
		ImportUI->ImportOptions.bImportCollisionData, ImportUI->FBXImportUI, Warn, hulls);
	if (NewSkelMesh == nullptr)
	{
		UnFbx::FFbxImporter::GetInstance()->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("BlastImport_SkeletalImportFailure", "Import of skeletal mesh failed.")), "BlastImport_SkeletalImportFailure");
		return nullptr;
	}
	BlastMesh->Mesh = NewSkelMesh;
	BlastMesh->Mesh->Skeleton = BlastMesh->Skeleton;
	BlastMesh->Skeleton->SetPreviewMesh(BlastMesh->Mesh);
	// Passed - now, create a physics asset if required.

	if (BlastMesh->PhysicsAsset == nullptr)
	{
		FName PhysicsAssetName = GetNameFromRoot(ImportUI->ImportOptions.RootName, "_PhysicsAsset");
		BlastMesh->PhysicsAsset = NewObject<UPhysicsAsset>(BlastMesh, PhysicsAssetName, RF_NoFlags);
	}
	if (!RebuildPhysicsAsset(BlastMesh, hulls))
	{
		UnFbx::FFbxImporter::GetInstance()->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("BlastImport_PhysicsAssetFailure", "Import of physics asset failed.")), "BlastImport_PhysicsAssetFailure");
		return nullptr;
	}

	// Have to manually call this, since it doesn't get called on create
	BlastMesh->RebuildIndexToBoneNameMap();
	BlastMesh->RebuildCookedBodySetupsIfRequired(true);
	BlastMesh->Mesh->RebuildIndexBufferRanges();


	if (BlastMesh && BlastMesh->Mesh && BlastMesh->PhysicsAsset && BlastMesh->Skeleton)
	{
		SetReimportPaths(BlastMesh, ReimportFilenames);
		//Success! 
		FEditorDelegates::OnAssetPostImport.Broadcast(this, BlastMesh);
		FAssetRegistryModule::AssetCreated(BlastMesh);

		return BlastMesh;
	}
	return nullptr;
}

bool UBlastMeshFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UBlastMesh* ExistingBlastMesh = Cast<UBlastMesh>(Obj);
	if (ExistingBlastMesh && ExistingBlastMesh->AssetImportData)
	{
		OutFilenames.Reset();
		ExistingBlastMesh->AssetImportData->ExtractFilenames(OutFilenames);
		return OutFilenames.Num() == 2;
	}
	return false;
}

void UBlastMeshFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UBlastMesh* ExistingBlastMesh = Cast<UBlastMesh>(Obj);
	//Some paths through the UE code only pass the primary file
	if (ExistingBlastMesh && ExistingBlastMesh->AssetImportData && ensure(NewReimportPaths.Num() > 0))
	{
		FString BlastAssetPath = NewReimportPaths[0];
		FString FBXPath;
		if (NewReimportPaths.Num() > 1)
		{
			//Easy
			FBXPath = NewReimportPaths[1];
		}
		else
		{
			//Need to re-guess it
			FBXPath = GuessFBXPathFromAsset(BlastAssetPath);
		}

		ExistingBlastMesh->AssetImportData->Update(BlastAssetPath);
		//Add the second file
		if (FBXPath.Len() > 0)
		{
			ExistingBlastMesh->AssetImportData->SourceData.SourceFiles.Emplace(FString(), IFileManager::Get().GetTimeStamp(*FBXPath), FMD5Hash::HashFile(*FBXPath));
			//This calls the private SanitizeImportFilename()
			ExistingBlastMesh->AssetImportData->UpdateFilenameOnly(FBXPath, 1);
		}
	}
}

EReimportResult::Type UBlastMeshFactory::Reimport(UObject* Obj)
{
	UBlastMesh* ExistingBlastMesh = Cast<UBlastMesh>(Obj);
	if (!ExistingBlastMesh)
	{
		return EReimportResult::Failed;
	}

	// Make sure file is valid and exists
	const TArray<FString> Filenames = ExistingBlastMesh->AssetImportData->ExtractFilenames();
	if (Filenames.Num() != 2)
	{
		return EReimportResult::Failed;
	}

	for (const FString& Filename : Filenames)
	{
		if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
		{
			return EReimportResult::Failed;
		}
	}

	// Reimport the Blast asset

	// Set some state so that the reimport populates the right asset.
	bReimporting = true;
	ReimportMesh = ExistingBlastMesh;

	// Run the import again
	EReimportResult::Type Result = EReimportResult::Failed;
	bool OutCanceled = false;

	if (ImportObject(ExistingBlastMesh->GetClass(), ExistingBlastMesh->GetOuter(), *ExistingBlastMesh->GetName(), ExistingBlastMesh->GetFlags(), Filenames[0], nullptr, OutCanceled) != nullptr)
	{
		UE_LOG(LogBlastEditor, Log, TEXT("Imported successfully"));
		ExistingBlastMesh->MarkPackageDirty();

		for (UBlastMeshComponent* ExistingComponent : TObjectRange<UBlastMeshComponent>())
		{
			if (ExistingComponent->GetBlastMesh() == ExistingBlastMesh)
			{
				FComponentReregisterContext ReregisterContext(ExistingComponent);
				//Clear the cached data
				ExistingComponent->SetModifiedAsset(nullptr);
			}
		}

		Result = EReimportResult::Succeeded;
	}
	else
	{
		if (OutCanceled)
		{
			UE_LOG(LogBlastEditor, Warning, TEXT("-- import canceled"));
		}
		else
		{
			UE_LOG(LogBlastEditor, Warning, TEXT("-- import failed"));
		}

		Result = EReimportResult::Failed;
	}

	bReimporting = false;

	return Result;
}

FTransform UBlastMeshFactory::GetTransformUE4ToBlastCoordinateSystem(UFbxSkeletalMeshImportData* SkeletalMeshImportData)
{
	return GetTransformBlastToUE4CoordinateSystem(SkeletalMeshImportData).Inverse();
}

FTransform UBlastMeshFactory::GetTransformBlastToUE4CoordinateSystem( UFbxSkeletalMeshImportData* SkeletalMeshImportData)
{
	//Blast coordinate system interpretation is : X = right, Y = forward, Z = up. centimeters
	//UE4 is X = forward, Y = right, Z = up, centimeters
	//Confusingly in FFbxImporter::ConvertScene:
	/*
	// we use -Y as forward axis here when we import. This is odd considering our forward axis is technically +X
	// but this is to mimic Maya/Max behavior where if you make a model facing +X facing,
	// when you import that mesh, you want +X facing in engine.
	// only thing that doesn't work is hand flipping because Max/Maya is RHS but UE is LHS
	// On the positive note, we now have import transform set up you can do to rotate mesh if you don't like default setting
	*/

	FTransform BlastToUE4Transform;
	//This pretty confusing, but the internal -Y flip becomes a -X flip due to the Y->X front conversion defined above
	BlastToUE4Transform.SetScale3D(FVector(-1.0f, 1.0f, 1.0f));
	FTransform ImportTransform(FTransform::Identity);
	if (SkeletalMeshImportData != nullptr)
	{
		if (SkeletalMeshImportData->bConvertScene && SkeletalMeshImportData->bForceFrontXAxis)
		{
			BlastToUE4Transform.SetRotation(FRotator(0, -90.0f, 0).Quaternion());
		}

		ImportTransform = FTransform(SkeletalMeshImportData->ImportRotation,
			SkeletalMeshImportData->ImportTranslation, FVector(SkeletalMeshImportData->ImportUniformScale));
	}
	return BlastToUE4Transform * ImportTransform;
}

void UBlastMeshFactory::TransformBlastAssetToUE4CoordinateSystem(NvBlastAsset* asset, UFbxSkeletalMeshImportData* SkeletalMeshImportData)
{
	FTransform CombinedImportTransform = GetTransformBlastToUE4CoordinateSystem(SkeletalMeshImportData);
	NvcQuat BlastToUE4Rotation = {CombinedImportTransform.GetRotation().X, CombinedImportTransform.GetRotation().Y, CombinedImportTransform.GetRotation().Z, CombinedImportTransform.GetRotation().W};
	NvcVec3 BlastToUE4Scale = {CombinedImportTransform.GetScale3D().X, CombinedImportTransform.GetScale3D().Y, CombinedImportTransform.GetScale3D().Z};
	NvcVec3 BlastToUE4Translation = {CombinedImportTransform.GetTranslation().X, CombinedImportTransform.GetTranslation().Y, CombinedImportTransform.GetTranslation().Z};
	NvBlastExtAssetTransformInPlace(asset, &BlastToUE4Scale, &BlastToUE4Rotation, &BlastToUE4Translation);
}

void UBlastMeshFactory::TransformBlastAssetFromUE4ToBlastCoordinateSystem(NvBlastAsset* asset, UFbxSkeletalMeshImportData* SkeletalMeshImportData)
{
	FTransform CombinedImportTransform = GetTransformUE4ToBlastCoordinateSystem(SkeletalMeshImportData);
	NvcQuat BlastToUE4Rotation = { CombinedImportTransform.GetRotation().X, CombinedImportTransform.GetRotation().Y, CombinedImportTransform.GetRotation().Z, CombinedImportTransform.GetRotation().W };
	NvcVec3 BlastToUE4Scale = { CombinedImportTransform.GetScale3D().X, CombinedImportTransform.GetScale3D().Y, CombinedImportTransform.GetScale3D().Z };
	NvcVec3 BlastToUE4Translation = { CombinedImportTransform.GetTranslation().X, CombinedImportTransform.GetTranslation().Y, CombinedImportTransform.GetTranslation().Z };
	NvBlastExtAssetTransformInPlace(asset, &BlastToUE4Scale, &BlastToUE4Rotation, &BlastToUE4Translation);
}



USkeletalMesh* UBlastMeshFactory::ImportSkeletalMesh(UBlastMesh* BlastMesh, FName skelMeshName, FString path, bool bImportCollisionData, 
	UFbxImportUI* FBXImportUI, FFeedbackContext* Warn, TMap<FName, TArray<FBlastCollisionHull>>& hulls)
{
	UFbxSkeletalMeshImportData* SkeletalMeshImportData = FBXImportUI->SkeletalMeshImportData;
	USkeletalMesh* NewMesh = BlastMesh->Mesh;

	// logger for all error/warnings
	// this one prints all messages that are stored in FFbxImporter
	UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();
	UnFbx::FBXImportOptions* FBXImportOptions = FbxImporter->GetImportOptions();
	//Clean up the options
	UnFbx::FBXImportOptions::ResetOptions(FBXImportOptions);

	UnFbx::ApplyImportUIToImportOptions(FBXImportUI, *FBXImportOptions);

	UnFbx::FFbxLoggerSetter Logger(FbxImporter);

	//Force this off since we do it manually
	FBXImportOptions->bCreatePhysicsAsset = false;

	//setup correct skeletal mesh
	FBXImportOptions->SkeletonForAnimation = BlastMesh->Skeleton;

	bool bOperationCanceled = false;

	Warn->BeginSlowTask(LOCTEXT("BeginImportingFbxMeshTask", "Importing FBX mesh"), true);
	if (!FbxImporter->ImportFromFile(path, TEXT("fbx")))
	{
		// Log the error message and fail the import.
		Warn->Log(ELogVerbosity::Error, FbxImporter->GetErrorMessage());
	}
	else
	{
		// Log the import message and import the mesh.
		const TCHAR* errorMessage = FbxImporter->GetErrorMessage();
		if (errorMessage[0] != '\0')
		{
			Warn->Log(errorMessage);
		}

		FbxNode* RootNodeToImport = NULL;
		RootNodeToImport = FbxImporter->Scene->GetRootNode();

		FBlastCollisionFbxImporter collisionImporter;
		collisionImporter.FindCollisionNodes(RootNodeToImport, bImportCollisionData);
		if (bImportCollisionData)
		{
			collisionImporter.ReadMeshSpaceCollisionHullsFromFBX(BlastMesh, hulls);
		}
		else
		{
			hulls.Empty();
		}

		// For animation and static mesh we assume there is at lease one interesting node by default
		int32 InterestingNodeCount = 1;
		TArray< TArray<FbxNode*>* > SkelMeshArray;

		FbxImporter->FillFbxSkelMeshArrayInScene(RootNodeToImport, SkelMeshArray, false);

		//Remove collision nodes to avoid duplicates. This is the only part that actually needs all this copy and pasted code, otherwise we could use the normal FBX factory
		collisionImporter.RemoveCollisionNodesFromImportList(SkelMeshArray);

		InterestingNodeCount = SkelMeshArray.Num();

		//const FString Filename(UFactory::CurrentFilename);
		if (RootNodeToImport && InterestingNodeCount > 0)
		{
			// NOTE: If we've got more than one entry in SkelMeshArray we're probably trying to import bad data, since that means more than one skeletal mesh in that FBX

			int32 TotalNumNodes = 0;
			for (int32 i = 0; i < SkelMeshArray.Num(); i++)
			{
				TArray<FbxNode*> NodeArray = *SkelMeshArray[i];

				TotalNumNodes += NodeArray.Num();

				// check if there is LODGroup for this skeletal mesh
				int32 MaxLODLevel = 1;
				for (int32 j = 0; j < NodeArray.Num(); j++)
				{
					FbxNode* Node = NodeArray[j];
					if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
					{
						// get max LODgroup level
						if (MaxLODLevel < Node->GetChildCount())
						{
							MaxLODLevel = Node->GetChildCount();
						}
					}
				}

				int32 LODIndex;
				int32 SuccessfulLodIndex = 0;
				bool bImportSkeletalMeshLODs = SkeletalMeshImportData->bImportMeshLODs;
				for (LODIndex = 0; LODIndex < MaxLODLevel; LODIndex++)
				{
					//We need to know what is the imported lod index when importing the morph targets
					int32 ImportedSuccessfulLodIndex = INDEX_NONE;
					if (!bImportSkeletalMeshLODs && LODIndex > 0) // not import LOD if UI option is OFF
					{
						break;
					}

					TArray<FbxNode*> SkelMeshNodeArray;
					for (int32 j = 0; j < NodeArray.Num(); j++)
					{
						FbxNode* Node = NodeArray[j];
						if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
						{
							TArray<FbxNode*> NodeInLod;
							if (Node->GetChildCount() > LODIndex)
							{
								FbxImporter->FindAllLODGroupNode(NodeInLod, Node, LODIndex);
							}
							else // in less some LODGroups have less level, use the last level
							{
								FbxImporter->FindAllLODGroupNode(NodeInLod, Node, Node->GetChildCount() - 1);
							}

							for (FbxNode *MeshNode : NodeInLod)
							{
								SkelMeshNodeArray.Add(MeshNode);
							}
						}
						else
						{
							SkelMeshNodeArray.Add(Node);
						}
					}
					
					FSkeletalMeshImportData OutData;
					UnFbx::FFbxImporter::FImportSkeletalMeshArgs ImportArgs;
					ImportArgs.InParent = BlastMesh;
					ImportArgs.NodeArray = SkelMeshNodeArray;
					ImportArgs.TemplateImportData = SkeletalMeshImportData;
					ImportArgs.bCancelOperation = &bOperationCanceled;
					ImportArgs.OutData = &OutData;

					if (LODIndex == 0 && SkelMeshNodeArray.Num() != 0)
					{
						ImportArgs.LodIndex = LODIndex;
						ImportArgs.Flags = RF_NoFlags;
						//TODO
						//FString PreviousFileName = UFactory::GetCurrentFilename();
						//UFactory::CurrentFilename = SkeletalMeshPath;
						NewMesh = FbxImporter->ImportSkeletalMesh(ImportArgs);

						//UFactory::CurrentFilename = PreviousFileName;
						if (bOperationCanceled)
						{
							FbxImporter->ReleaseScene();
							Warn->EndSlowTask();
							return nullptr;
						}

						if (NewMesh)
						{
							//Increment the LOD index
							SuccessfulLodIndex++;
						}
					}
					else if (NewMesh) // the base skeletal mesh is imported successfully
					{
						ImportArgs.LodIndex = SuccessfulLodIndex;
						ImportArgs.Flags = RF_Transient;

						USkeletalMesh* BaseSkeletalMesh = NewMesh;
						USkeletalMesh *LODObject = FbxImporter->ImportSkeletalMesh(ImportArgs);
						bool bImportSucceeded = !bOperationCanceled && FbxImporter->ImportSkeletalMeshLOD(LODObject, BaseSkeletalMesh, SuccessfulLodIndex, false);

						if (bImportSucceeded)
						{
							BaseSkeletalMesh->LODInfo[SuccessfulLodIndex].ScreenSize = 1.0f / (MaxLODLevel * SuccessfulLodIndex);
							ImportedSuccessfulLodIndex = SuccessfulLodIndex;
							SuccessfulLodIndex++;
						}
						else
						{
							FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_SkeletalMeshLOD", "Failed to import Skeletal mesh LOD.")), FFbxErrors::SkeletalMesh_LOD_FailedToImport);
						}
					}

					if (NewMesh && SkeletalMeshImportData->bImportMorphTargets && ImportedSuccessfulLodIndex != INDEX_NONE)
					{
						// Disable material importing when importing morph targets
						uint32 bImportMaterials = FBXImportOptions->bImportMaterials;
						FBXImportOptions->bImportMaterials = 0;
						uint32 bImportTextures = FBXImportOptions->bImportTextures;
						FBXImportOptions->bImportTextures = 0;

						FbxImporter->ImportFbxMorphTarget(SkelMeshNodeArray, NewMesh, BlastMesh, ImportedSuccessfulLodIndex, OutData);

						FBXImportOptions->bImportMaterials = !!bImportMaterials;
						FBXImportOptions->bImportTextures = !!bImportTextures;
					}
				}
			}

			for (int32 i = 0; i < SkelMeshArray.Num(); i++)
			{
				delete SkelMeshArray[i];
			}

			// if total nodes we found is 0, we didn't find anything. 
			if (TotalNumNodes == 0)
			{
				FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_NoMeshFoundOnRoot", "Could not find any valid mesh on the root hierarchy. If you have mesh in the sub hierarchy, please enable option of [Import Meshes In Bone Hierarchy] when import.")),
					FFbxErrors::SkeletalMesh_NoMeshFoundOnRoot);
			}
		}
	}

	if (NewMesh == nullptr)
	{
		FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_NoObject", "Import failed.")), FFbxErrors::Generic_ImportingNewObjectFailed);
	}
	else
	{
		NewMesh->CalculateInvRefMatrices();
	}

	FbxImporter->ReleaseScene();
	Warn->EndSlowTask();

	return NewMesh;
}

bool UBlastMeshFactory::RebuildPhysicsAsset(UBlastMesh* BlastMesh, const TMap<FName, TArray<FBlastCollisionHull>>& hulls)
{
	// NOTE: We don't care if the skeletal mesh currently has an attached physics asset.
	UPhysicsAsset* Asset = BlastMesh->PhysicsAsset;

	//Clean it out
	Asset->SkeletalBodySetups.Reset();
	Asset->ConstraintSetup.Reset();
	Asset->BoundsBodies.Reset();
	Asset->UpdateBodySetupIndexMap();
	Asset->UpdateBoundsBodiesArray();

	Asset->PreviewSkeletalMesh = BlastMesh->Mesh;

	if (hulls.Num() > 0)
	{
		for (auto& chunk : hulls)
		{
			FName boneName = chunk.Key;
			int32 NewBodyIndex = FPhysicsAssetUtils::CreateNewBody(Asset, boneName);
			UBodySetup* bs = Asset->SkeletalBodySetups[NewBodyIndex];
			bs->RemoveSimpleCollision();

			const FMatrix WorldToBoneXForm = BlastMesh->Mesh->GetComposedRefPoseMatrix(boneName).Inverse();
			for (const FBlastCollisionHull& Hull : chunk.Value)
			{
				FKConvexElem ConvexElem;
				for (int VertIdx = 0; VertIdx < Hull.Points.Num(); VertIdx++)
				{
					FVector p = Hull.Points[VertIdx];
					p = WorldToBoneXForm.TransformPosition(p);
					ConvexElem.VertexData.Add(p);
				}
				ConvexElem.UpdateElemBox();
				bs->AggGeom.ConvexElems.Add(ConvexElem);
			}

			bs->InvalidatePhysicsData();
			bs->CreatePhysicsMeshes();
		}
	}
	else
	{
		FPhysAssetCreateParams parms;

		// Irrelevant params for this
		//	parms.MinBoneSize = 5.0f;
		//	parms.bAlignDownBone = true;
		//	parms.AngularConstraintMode = ACM_Limited;
		//	parms.bWalkPastSmall = true;

		parms.GeomType = EFG_MultiConvexHull; // Is this correct?
		parms.VertWeight = EVW_DominantWeight;
		parms.bCreateJoints = false;
		parms.bBodyForAll = true;
		parms.MaxHullVerts = 32;
		parms.bWalkPastSmall = false;

		FText CreationErrorMessage;

		bool bSuccess = FPhysicsAssetUtils::CreateFromSkeletalMesh(Asset, BlastMesh->Mesh, parms, CreationErrorMessage, false);
		if (!bSuccess)
		{
			return false;
		}
	}

	return true;
}

FName UBlastMeshFactory::GetNameFromRoot(FName rootName, FString suffix)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	FString DefaultSuffix;
	FString AssetName;
	FString PackageName;
	AssetToolsModule.Get().CreateUniqueAssetName(rootName.ToString(), suffix, /*out*/ PackageName, /*out*/ AssetName);

	// Since we have a parent when we're creating the asset, we can ignore the package name

	return FName(*AssetName);
}


FString UBlastMeshFactory::GuessFBXPathFromAsset(const FString& BlastAssetPath)
{
	FString assetName = FPaths::GetBaseFilename(BlastAssetPath);
	FString rootPath = FPaths::GetPath(BlastAssetPath);

	IPlatformFile &platformFile = FPlatformFileManager::Get().GetPlatformFile();

	FBlastDirectoryVisitor Visitor(platformFile, assetName, ".fbx");
	platformFile.IterateDirectory(*rootPath, Visitor);

	//TODO: Smarter
	if (Visitor.FilesFound.Num() > 0)
	{
		return Visitor.FilesFound[0];
	}
	return FString();
}

#undef LOCTEXT_NAMESPACE
