// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxFactory.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Editor/EditorEngine.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/FbxTextureImportData.h"
#include "Factories/FbxImportUI.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"
#include "SkelImport.h"

#include "EditorReimportHandler.h"

#include "Logging/TokenizedMessage.h"
#include "FbxImporter.h"

#include "Misc/FbxErrors.h"
#include "AssetRegistryModule.h"
#include "ObjectTools.h"
#include "JsonObjectConverter.h"

#define LOCTEXT_NAMESPACE "FBXFactory"

UFbxFactory::UFbxFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = NULL;
	Formats.Add(TEXT("fbx;FBX meshes and animations"));
	Formats.Add(TEXT("obj;OBJ Static meshes"));
	//Formats.Add(TEXT("dae;Collada meshes and animations"));

	bCreateNew = false;
	bText = false;
	bEditorImport = true;
	bOperationCanceled = false;
	bDetectImportTypeOnImport = true;
}


void UFbxFactory::PostInitProperties()
{
	Super::PostInitProperties();
	bEditorImport = true;
	bText = false;

	ImportUI = NewObject<UFbxImportUI>(this, NAME_None, RF_NoFlags);
}


bool UFbxFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UStaticMesh::StaticClass() || Class == USkeletalMesh::StaticClass() || Class == UAnimSequence::StaticClass());
}

UClass* UFbxFactory::ResolveSupportedClass()
{
	UClass* ImportClass = NULL;

	if (ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh)
	{
		ImportClass = USkeletalMesh::StaticClass();
	}
	else if (ImportUI->MeshTypeToImport == FBXIT_Animation)
	{
		ImportClass = UAnimSequence::StaticClass();
	}
	else
	{
		ImportClass = UStaticMesh::StaticClass();
	}

	return ImportClass;
}


bool UFbxFactory::DetectImportType(const FString& InFilename)
{
	UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
	UnFbx::FFbxLoggerSetter Logger(FFbxImporter);
	int32 ImportType = FFbxImporter->GetImportType(InFilename);
	if ( ImportType == -1)
	{
		FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("NoImportTypeDetected", "Can't detect import type. No mesh is found or animation track.")), FFbxErrors::Generic_CannotDetectImportType);
		return false;
	}
	else if(!IsAutomatedImport() || ImportUI->bAutomatedImportShouldDetectType)
	{
		ImportUI->MeshTypeToImport = EFBXImportType(ImportType);
		ImportUI->OriginalImportType = ImportUI->MeshTypeToImport;
	}
	
	return true;
}


UObject* UFbxFactory::ImportANode(void* VoidFbxImporter, TArray<void*> VoidNodes, UObject* InParent, FName InName, EObjectFlags Flags, int32& NodeIndex, int32 Total, UObject* InMesh, int LODIndex)
{
	UnFbx::FFbxImporter* FFbxImporter = (UnFbx::FFbxImporter*)VoidFbxImporter;
	TArray<FbxNode*> Nodes;
	for (void* VoidNode : VoidNodes)
	{
		Nodes.Add((FbxNode*)VoidNode);
	}
	check(Nodes.Num() > 0 && Nodes[0] != nullptr);

	UObject* NewObject = NULL;
	FName OutputName = FFbxImporter->MakeNameForMesh(InName.ToString(), Nodes[0]);
	
	{
		// skip collision models
		FbxString NodeName(Nodes[0]->GetName());
		if ( NodeName.Find("UCX") != -1 || NodeName.Find("MCDCX") != -1 ||
			 NodeName.Find("UBX") != -1 || NodeName.Find("USP") != -1 || NodeName.Find("UCP") != -1 )
		{
			return NULL;
		}

		NewObject = FFbxImporter->ImportStaticMeshAsSingle( InParent, Nodes, OutputName, Flags, ImportUI->StaticMeshImportData, Cast<UStaticMesh>(InMesh), LODIndex );
	}

	if (NewObject)
	{
		NodeIndex++;
		FFormatNamedArguments Args;
		Args.Add( TEXT("NodeIndex"), NodeIndex );
		Args.Add( TEXT("ArrayLength"), Total );
		GWarn->StatusUpdate( NodeIndex, Total, FText::Format( NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args ) );
	}

	return NewObject;
}

bool UFbxFactory::ConfigureProperties()
{
	bDetectImportTypeOnImport = true;
	EnableShowOption();

	return true;
}

UObject* UFbxFactory::FactoryCreateBinary
(
 UClass*			Class,
 UObject*			InParent,
 FName				Name,
 EObjectFlags		Flags,
 UObject*			Context,
 const TCHAR*		Type,
 const uint8*&		Buffer,
 const uint8*		BufferEnd,
 FFeedbackContext*	Warn,
 bool&				bOutOperationCanceled
 )
{
	CA_ASSUME(InParent);

	if( bOperationCanceled )
	{
		bOutOperationCanceled = true;
		FEditorDelegates::OnAssetPostImport.Broadcast(this, NULL);
		return NULL;
	}

	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

	UObject* NewObject = NULL;
	
	//Look if its a re-import, in that cazse we must call the re-import factory
	UObject *ExistingObject = nullptr;
	UFbxStaticMeshImportData* ExistingStaticMeshImportData = nullptr;
	UFbxSkeletalMeshImportData* ExistingSkeletalMeshImportData = nullptr;
	if (InParent != nullptr)
	{
		ExistingObject = StaticFindObject(UObject::StaticClass(), InParent, *(Name.ToString()));
		if (ExistingObject)
		{
			UStaticMesh *ExistingStaticMesh = Cast<UStaticMesh>(ExistingObject);
			USkeletalMesh *ExistingSkeletalMesh = Cast<USkeletalMesh>(ExistingObject);
			UObject *ObjectToReimport = nullptr;
			if (ExistingStaticMesh)
			{
				ObjectToReimport = ExistingStaticMesh;
			}
			else if (ExistingSkeletalMesh)
			{
				ObjectToReimport = ExistingSkeletalMesh;
			}

			if (ObjectToReimport != nullptr)
			{
				TArray<UObject*> ToReimportObjects;
				ToReimportObjects.Add(ObjectToReimport);
				TArray<FString> Filenames;
				Filenames.Add(UFactory::CurrentFilename);
				//Set the new fbx source path before starting the re-import
				FReimportManager::Instance()->UpdateReimportPaths(ObjectToReimport, Filenames);
				//Do the re-import and exit
				FReimportManager::Instance()->ValidateAllSourceFileAndReimport(ToReimportObjects);
				return ObjectToReimport;
			}
		}
	}

	//We are not re-importing
	ImportUI->bIsReimport = false;

	if ( bDetectImportTypeOnImport)
	{
		if ( !DetectImportType(UFactory::CurrentFilename) )
		{
			// Failed to read the file info, fail the import
			FEditorDelegates::OnAssetPostImport.Broadcast(this, NULL);
			return NULL;
		}
	}
	// logger for all error/warnings
	// this one prints all messages that are stored in FFbxImporter
	UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();
	UnFbx::FBXImportOptions* ImportOptions = FbxImporter->GetImportOptions();
	if (bShowOption)
	{
		//Clean up the options
		UnFbx::FBXImportOptions::ResetOptions(ImportOptions);
	}
	
	UnFbx::FFbxLoggerSetter Logger(FbxImporter);

	EFBXImportType ForcedImportType = FBXIT_StaticMesh;

	bool bIsObjFormat = false;
	if( FString(Type).Equals(TEXT("obj"), ESearchCase::IgnoreCase ) )
	{
		bIsObjFormat = true;
	}


	// Show the import dialog only when not in a "yes to all" state or when automating import
	bool bIsAutomated = IsAutomatedImport();
	bool bShowImportDialog = bShowOption && !bIsAutomated;
	bool bImportAll = false;
	ImportOptions = GetImportOptions(FbxImporter, ImportUI, bShowImportDialog, bIsAutomated, InParent->GetPathName(), bOperationCanceled, bImportAll, bIsObjFormat, bIsObjFormat, ForcedImportType, ExistingObject);
	bOutOperationCanceled = bOperationCanceled;
	
	if( bImportAll )
	{
		// If the user chose to import all, we don't show the dialog again and use the same settings for each object until importing another set of files
		bShowOption = false;
	}

	// Automated importing does not use the same settings and gets its settings straight from the user
	if(!bIsAutomated)
	{
		// For multiple files, use the same settings
		bDetectImportTypeOnImport = false;
	}

	if (ImportOptions)
	{

		Warn->BeginSlowTask( NSLOCTEXT("FbxFactory", "BeginImportingFbxMeshTask", "Importing FBX mesh"), true );
		if ( !FbxImporter->ImportFromFile( *UFactory::CurrentFilename, Type, true ) )
		{
			// Log the error message and fail the import.
			Warn->Log(ELogVerbosity::Error, FbxImporter->GetErrorMessage() );
		}
		else
		{
			// Log the import message and import the mesh.
			const TCHAR* errorMessage = FbxImporter->GetErrorMessage();
			if (errorMessage[0] != '\0')
			{
				Warn->Log( errorMessage );
			}

			FbxNode* RootNodeToImport = NULL;
			RootNodeToImport = FbxImporter->Scene->GetRootNode();

			// For animation and static mesh we assume there is at lease one interesting node by default
			int32 InterestingNodeCount = 1;
			TArray< TArray<FbxNode*>* > SkelMeshArray;

			bool bImportStaticMeshLODs = ImportUI->StaticMeshImportData->bImportMeshLODs;
			bool bCombineMeshes = ImportUI->StaticMeshImportData->bCombineMeshes;
			bool bCombineMeshesLOD = false;

			if ( ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh )
			{
				FbxImporter->FillFbxSkelMeshArrayInScene(RootNodeToImport, SkelMeshArray, false);
				InterestingNodeCount = SkelMeshArray.Num();
			}
			else if( ImportUI->MeshTypeToImport == FBXIT_StaticMesh )
			{
				FbxImporter->ApplyTransformSettingsToFbxNode(RootNodeToImport, ImportUI->StaticMeshImportData);

				if( bCombineMeshes && !bImportStaticMeshLODs )
				{
					// If Combine meshes and dont import mesh LODs, the interesting node count should be 1 so all the meshes are grouped together into one static mesh
					InterestingNodeCount = 1;
				}
				else
				{
					// count meshes in lod groups if we dont care about importing LODs
					bool bCountLODGroupMeshes = !bImportStaticMeshLODs;
					int32 NumLODGroups = 0;
					InterestingNodeCount = FbxImporter->GetFbxMeshCount(RootNodeToImport,bCountLODGroupMeshes,NumLODGroups);

					// if there were LODs in the file, do not combine meshes even if requested
					if( bImportStaticMeshLODs && bCombineMeshes && NumLODGroups > 0)
					{
						bCombineMeshes = false;
						//Combine all the LOD together and export one mesh with LODs
						bCombineMeshesLOD = true;
					}
				}
				//Find all collision models, even the one contain under a LOD Group
				FbxImporter->FillFbxCollisionMeshArray(RootNodeToImport);
			}

		
			if (InterestingNodeCount > 1)
			{
				// the option only works when there are only one asset
				ImportOptions->bUsedAsFullName = false;
			}

			const FString Filename( UFactory::CurrentFilename );
			if (RootNodeToImport && InterestingNodeCount > 0)
			{  
				int32 NodeIndex = 0;

				int32 ImportedMeshCount = 0;
				if ( ImportUI->MeshTypeToImport == FBXIT_StaticMesh )  // static mesh
				{
					UStaticMesh* NewStaticMesh = NULL;
					if (bCombineMeshes)
					{
						TArray<FbxNode*> FbxMeshArray;
						FbxImporter->FillFbxMeshArray(RootNodeToImport, FbxMeshArray, FbxImporter);
						if (FbxMeshArray.Num() > 0)
						{
							NewStaticMesh = FbxImporter->ImportStaticMeshAsSingle(InParent, FbxMeshArray, Name, Flags, ImportUI->StaticMeshImportData, NULL, 0);
							if (NewStaticMesh != nullptr)
							{
								//Build the staticmesh
								FbxImporter->PostImportStaticMesh(NewStaticMesh, FbxMeshArray);
								FbxImporter->UpdateStaticMeshImportData(NewStaticMesh, nullptr);
							}
						}

						ImportedMeshCount = NewStaticMesh ? 1 : 0;
					}
					else if (bCombineMeshesLOD)
					{
						TArray<FbxNode*> FbxMeshArray;
						TArray<FbxNode*> FbxLodGroups;
						TArray<TArray<FbxNode*>> FbxMeshesLod;
						FbxImporter->FillFbxMeshAndLODGroupArray(RootNodeToImport, FbxLodGroups, FbxMeshArray);
						FbxMeshesLod.Add(FbxMeshArray);
						for (FbxNode* LODGroup : FbxLodGroups)
						{
							if (LODGroup->GetNodeAttribute() && LODGroup->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup && LODGroup->GetChildCount() > 0)
							{
								for (int32 GroupLodIndex = 0; GroupLodIndex < LODGroup->GetChildCount(); ++GroupLodIndex)
								{
									if (GroupLodIndex >= MAX_STATIC_MESH_LODS)
									{
										FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("ImporterLimits_MaximumStaticMeshLODReach", "Reach the maximum LOD number({0}) for a staticmesh."), FText::AsNumber(MAX_STATIC_MESH_LODS))), FFbxErrors::Generic_Mesh_TooManyLODs);
										continue;
									}
									TArray<FbxNode*> AllNodeInLod;
									FbxImporter->FindAllLODGroupNode(AllNodeInLod, LODGroup, GroupLodIndex);
									if (AllNodeInLod.Num() > 0)
									{
										if (FbxMeshesLod.Num() <= GroupLodIndex)
										{
											FbxMeshesLod.Add(AllNodeInLod);
										}
										else
										{
											TArray<FbxNode*> &LODGroupArray = FbxMeshesLod[GroupLodIndex];
											for (FbxNode* NodeToAdd : AllNodeInLod)
											{
												LODGroupArray.Add(NodeToAdd);
											}
										}
									}
								}
							}
						}

						//Import the LOD root
						if (FbxMeshesLod.Num() > 0)
						{
							TArray<FbxNode*> &LODMeshesArray = FbxMeshesLod[0];
							NewStaticMesh = FbxImporter->ImportStaticMeshAsSingle(InParent, LODMeshesArray, Name, Flags, ImportUI->StaticMeshImportData, NULL, 0);
						}
						//Import all LODs
						for (int32 LODIndex = 1; LODIndex < FbxMeshesLod.Num(); ++LODIndex)
						{
							TArray<FbxNode*> &LODMeshesArray = FbxMeshesLod[LODIndex];
							FbxImporter->ImportStaticMeshAsSingle(InParent, LODMeshesArray, Name, Flags, ImportUI->StaticMeshImportData, NewStaticMesh, LODIndex);
						}
						
						//Build the staticmesh
						if (NewStaticMesh)
						{
							FbxImporter->PostImportStaticMesh(NewStaticMesh, FbxMeshesLod[0]);
							FbxImporter->UpdateStaticMeshImportData(NewStaticMesh, nullptr);
						}
					}
					else
					{
						TArray<UObject*> AllNewAssets;
						UObject* Object = RecursiveImportNode(FbxImporter,RootNodeToImport,InParent,Name,Flags,NodeIndex,InterestingNodeCount, AllNewAssets);

						NewStaticMesh = Cast<UStaticMesh>( Object );

						// Make sure to notify the asset registry of all assets created other than the one returned, which will notify the asset registry automatically.
						for ( auto AssetIt = AllNewAssets.CreateConstIterator(); AssetIt; ++AssetIt )
						{
							UObject* Asset = *AssetIt;
							if ( Asset != NewStaticMesh )
							{
								FAssetRegistryModule::AssetCreated(Asset);
								Asset->MarkPackageDirty();
							}
						}

						ImportedMeshCount = AllNewAssets.Num();
					}

					// Importing static mesh global sockets only if one mesh is imported
					if( ImportedMeshCount == 1 && NewStaticMesh)
					{
						FbxImporter->ImportStaticMeshGlobalSockets( NewStaticMesh );
					}

					NewObject = NewStaticMesh;

				}
				else if ( ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh )// skeletal mesh
				{
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
						MaxLODLevel = FMath::Min(MAX_SKELETAL_MESH_LODS, MaxLODLevel);
					
						int32 LODIndex;
						int32 SuccessfulLodIndex = 0;
						bool bImportSkeletalMeshLODs = ImportUI->SkeletalMeshImportData->bImportMeshLODs;
						for (LODIndex = 0; LODIndex < MaxLODLevel; LODIndex++)
						{
							//We need to know what is the imported lod index when importing the morph targets
							int32 ImportedSuccessfulLodIndex = INDEX_NONE;
							if ( !bImportSkeletalMeshLODs && LODIndex > 0) // not import LOD if UI option is OFF
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
							if (LODIndex == 0 && SkelMeshNodeArray.Num() != 0)
							{
								FName OutputName = FbxImporter->MakeNameForMesh(Name.ToString(), SkelMeshNodeArray[0]);

								UnFbx::FFbxImporter::FImportSkeletalMeshArgs ImportSkeletalMeshArgs;
								ImportSkeletalMeshArgs.InParent = InParent;
								ImportSkeletalMeshArgs.NodeArray = SkelMeshNodeArray;
								ImportSkeletalMeshArgs.Name = OutputName;
								ImportSkeletalMeshArgs.Flags = Flags;
								ImportSkeletalMeshArgs.TemplateImportData = ImportUI->SkeletalMeshImportData;
								ImportSkeletalMeshArgs.LodIndex = LODIndex;
								ImportSkeletalMeshArgs.bCancelOperation = &bOperationCanceled;
								ImportSkeletalMeshArgs.OutData = &OutData;

								USkeletalMesh* NewMesh = FbxImporter->ImportSkeletalMesh( ImportSkeletalMeshArgs );
								NewObject = NewMesh;

								if(bOperationCanceled)
								{
									// User cancelled, clean up and return
									FbxImporter->ReleaseScene();
									Warn->EndSlowTask();
									bOperationCanceled = true;
									return nullptr;
								}

								if ( NewMesh )
								{
									if (ImportUI->bImportAnimations)
									{
										// We need to remove all scaling from the root node before we set up animation data.
										// Othewise some of the global transform calculations will be incorrect.
										FbxImporter->RemoveTransformSettingsFromFbxNode(RootNodeToImport, ImportUI->SkeletalMeshImportData);
										FbxImporter->SetupAnimationDataFromMesh(NewMesh, InParent, SkelMeshNodeArray, ImportUI->AnimSequenceImportData, OutputName.ToString());

										// Reapply the transforms for the rest of the import
										FbxImporter->ApplyTransformSettingsToFbxNode(RootNodeToImport, ImportUI->SkeletalMeshImportData);
									}
									ImportedSuccessfulLodIndex = SuccessfulLodIndex;
									//Increment the LOD index
									SuccessfulLodIndex++;
								}
							}
							else if (NewObject) // the base skeletal mesh is imported successfully
							{
								USkeletalMesh* BaseSkeletalMesh = Cast<USkeletalMesh>(NewObject);
								FName LODObjectName = NAME_None;
								UnFbx::FFbxImporter::FImportSkeletalMeshArgs ImportSkeletalMeshArgs;
								ImportSkeletalMeshArgs.InParent = BaseSkeletalMesh->GetOutermost();
								ImportSkeletalMeshArgs.NodeArray = SkelMeshNodeArray;
								ImportSkeletalMeshArgs.Name = LODObjectName;
								ImportSkeletalMeshArgs.Flags = RF_Transient;
								ImportSkeletalMeshArgs.TemplateImportData = ImportUI->SkeletalMeshImportData;
								ImportSkeletalMeshArgs.LodIndex = SuccessfulLodIndex;
								ImportSkeletalMeshArgs.bCancelOperation = &bOperationCanceled;
								ImportSkeletalMeshArgs.OutData = &OutData;

								USkeletalMesh *LODObject = FbxImporter->ImportSkeletalMesh( ImportSkeletalMeshArgs );
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
						
							// import morph target
							if ( NewObject && ImportUI->SkeletalMeshImportData->bImportMorphTargets && ImportedSuccessfulLodIndex != INDEX_NONE)
							{
								// Disable material importing when importing morph targets
								uint32 bImportMaterials = ImportOptions->bImportMaterials;
								ImportOptions->bImportMaterials = 0;
								uint32 bImportTextures = ImportOptions->bImportTextures;
								ImportOptions->bImportTextures = 0;

								FbxImporter->ImportFbxMorphTarget(SkelMeshNodeArray, Cast<USkeletalMesh>(NewObject), InParent, ImportedSuccessfulLodIndex, OutData);
							
								ImportOptions->bImportMaterials = !!bImportMaterials;
								ImportOptions->bImportTextures = !!bImportTextures;
							}
						}
					
						if (NewObject)
						{
							NodeIndex++;
							FFormatNamedArguments Args;
							Args.Add( TEXT("NodeIndex"), NodeIndex );
							Args.Add( TEXT("ArrayLength"), SkelMeshArray.Num() );
							GWarn->StatusUpdate( NodeIndex, SkelMeshArray.Num(), FText::Format( NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args ) );
							
							USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(NewObject);
							UnFbx::FFbxImporter::UpdateSkeletalMeshImportData(SkeletalMesh, ImportUI->SkeletalMeshImportData, INDEX_NONE, nullptr, nullptr);

							//If we have import some morph target we have to rebuild the render resources since morph target are now using GPU
							if (SkeletalMesh->MorphTargets.Num() > 0)
							{
								SkeletalMesh->ReleaseResources();
								//Rebuild the resources with a post edit change since we have added some morph targets
								SkeletalMesh->PostEditChange();
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
				else if ( ImportUI->MeshTypeToImport == FBXIT_Animation )// animation
				{
					if (ImportOptions->SkeletonForAnimation)
					{
						// will return the last animation sequence that were added
						NewObject = UEditorEngine::ImportFbxAnimation( ImportOptions->SkeletonForAnimation, InParent, ImportUI->AnimSequenceImportData, *Filename, *Name.ToString(), true );
					}
				}
			}
			else
			{
				if (RootNodeToImport == NULL)
				{
					FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_InvalidRoot", "Could not find root node.")), FFbxErrors::SkeletalMesh_InvalidRoot);
				}
				else if (ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh)
				{
					FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_InvalidBone", "Failed to find any bone hierarchy. Try disabling the \"Import As Skeletal\" option to import as a rigid mesh. ")), FFbxErrors::SkeletalMesh_InvalidBone);
				}
				else
				{
					FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_InvalidNode", "Could not find any node.")), FFbxErrors::SkeletalMesh_InvalidNode);
				}
			}
		}

		if (NewObject == NULL)
		{
			FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_NoObject", "Import failed.")), FFbxErrors::Generic_ImportingNewObjectFailed);
		}

		FbxImporter->ReleaseScene();
		Warn->EndSlowTask();
	}

	FEditorDelegates::OnAssetPostImport.Broadcast(this, NewObject);

	return NewObject;
}


UObject* UFbxFactory::RecursiveImportNode(void* VoidFbxImporter, void* VoidNode, UObject* InParent, FName InName, EObjectFlags Flags, int32& NodeIndex, int32 Total, TArray<UObject*>& OutNewAssets)
{
	TArray<void*> TmpVoidArray;
	UObject* NewObject = NULL;
	UnFbx::FFbxImporter *FbxImporter = (UnFbx::FFbxImporter *)VoidFbxImporter;
	FbxNode* Node = (FbxNode*)VoidNode;
	if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup && Node->GetChildCount() > 0 )
	{
		TArray<FbxNode*> AllNodeInLod;
		// import base mesh
		FbxImporter->FindAllLODGroupNode(AllNodeInLod, Node, 0);
		if (AllNodeInLod.Num() > 0)
		{
			TmpVoidArray.Empty();
			for (FbxNode* LodNode : AllNodeInLod)
			{
				TmpVoidArray.Add(LodNode);
			}
			NewObject = ImportANode(VoidFbxImporter, TmpVoidArray, InParent, InName, Flags, NodeIndex, Total);
		}

		if ( NewObject )
		{
			OutNewAssets.AddUnique(NewObject);
		}

		bool bImportMeshLODs = ImportUI->StaticMeshImportData->bImportMeshLODs;

		if (NewObject && bImportMeshLODs)
		{
			// import LOD meshes
			for (int32 LODIndex = 1; LODIndex < Node->GetChildCount(); LODIndex++)
			{
				if (LODIndex >= MAX_STATIC_MESH_LODS)
				{
					FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("ImporterLimits_MaximumStaticMeshLODReach", "Reach the maximum LOD number({0}) for a staticmesh."), FText::AsNumber(MAX_STATIC_MESH_LODS))), FFbxErrors::Generic_Mesh_TooManyLODs);
					continue;
				}
				AllNodeInLod.Empty();
				FbxImporter->FindAllLODGroupNode(AllNodeInLod, Node, LODIndex);
				if (AllNodeInLod.Num() > 0)
				{
					TmpVoidArray.Empty();
					for (FbxNode* LodNode : AllNodeInLod)
					{
						TmpVoidArray.Add(LodNode);
					}
					ImportANode(VoidFbxImporter, TmpVoidArray, InParent, InName, Flags, NodeIndex, Total, NewObject, LODIndex);
				}
			}
		}
		
		if (NewObject)
		{
			UStaticMesh *NewStaticMesh = Cast<UStaticMesh>(NewObject);
			if (NewStaticMesh != nullptr)
			{
				//Reorder the material
				TArray<FbxNode*> Nodes;
				FbxImporter->FindAllLODGroupNode(Nodes, Node, 0);
				if (Nodes.Num() > 0)
				{
					FbxImporter->PostImportStaticMesh(NewStaticMesh, Nodes);
					FbxImporter->UpdateStaticMeshImportData(NewStaticMesh, nullptr);
				}
			}
		}
	}
	else
	{
		if (Node->GetMesh())
		{
			TmpVoidArray.Empty();
			TmpVoidArray.Add(Node);
			NewObject = ImportANode(VoidFbxImporter, TmpVoidArray, InParent, InName, Flags, NodeIndex, Total);

			if ( NewObject )
			{
				UStaticMesh *NewStaticMesh = Cast<UStaticMesh>(NewObject);
				if (NewStaticMesh != nullptr)
				{
					//Reorder the material
					TArray<FbxNode*> Nodes;
					Nodes.Add(Node);
					FbxImporter->PostImportStaticMesh(NewStaticMesh, Nodes);
					FbxImporter->UpdateStaticMeshImportData(NewStaticMesh, nullptr);
				}
				OutNewAssets.AddUnique(NewObject);
			}
		}
		
		for (int32 ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			UObject* SubObject = RecursiveImportNode(VoidFbxImporter,Node->GetChild(ChildIndex),InParent,InName,Flags,NodeIndex,Total,OutNewAssets);

			if ( SubObject )
			{
				OutNewAssets.AddUnique(SubObject);
			}

			if (NewObject==NULL)
			{
				NewObject = SubObject;
			}
		}
	}

	return NewObject;
}


void UFbxFactory::CleanUp() 
{
	UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
	bDetectImportTypeOnImport = true;
	bShowOption = true;
	// load options
	if (FFbxImporter)
	{
		struct UnFbx::FBXImportOptions* ImportOptions = FFbxImporter->GetImportOptions();
		if ( ImportOptions )
		{
			ImportOptions->SkeletonForAnimation = NULL;
			ImportOptions->PhysicsAsset = NULL;
		}
	}
}

bool UFbxFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if( Extension == TEXT("fbx") || Extension == TEXT("obj") )
	{
		return true;
	}
	return false;
}

IImportSettingsParser* UFbxFactory::GetImportSettingsParser()
{
	return ImportUI;
}

UFbxImportUI::UFbxImportUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsReimport = false;
	bAutomatedImportShouldDetectType = true;
	//Make sure we are transactional to allow undo redo
	this->SetFlags(RF_Transactional);
	
	StaticMeshImportData = CreateDefaultSubobject<UFbxStaticMeshImportData>(TEXT("StaticMeshImportData"));
	StaticMeshImportData->SetFlags(RF_Transactional);
	StaticMeshImportData->LoadOptions();
	
	SkeletalMeshImportData = CreateDefaultSubobject<UFbxSkeletalMeshImportData>(TEXT("SkeletalMeshImportData"));
	SkeletalMeshImportData->SetFlags(RF_Transactional);
	SkeletalMeshImportData->LoadOptions();
	
	AnimSequenceImportData = CreateDefaultSubobject<UFbxAnimSequenceImportData>(TEXT("AnimSequenceImportData"));
	AnimSequenceImportData->SetFlags(RF_Transactional);
	AnimSequenceImportData->LoadOptions();
	
	TextureImportData = CreateDefaultSubobject<UFbxTextureImportData>(TEXT("TextureImportData"));
	TextureImportData->SetFlags(RF_Transactional);
	TextureImportData->LoadOptions();
}


bool UFbxImportUI::CanEditChange( const UProperty* InProperty ) const
{
	bool bIsMutable = Super::CanEditChange( InProperty );
	if( bIsMutable && InProperty != NULL )
	{
		FName PropName = InProperty->GetFName();

		if(PropName == TEXT("StartFrame") || PropName == TEXT("EndFrame"))
		{
			bIsMutable = AnimSequenceImportData->AnimationLength == FBXALIT_SetRange && bImportAnimations;
		}
		else if(PropName == TEXT("bImportCustomAttribute") || PropName == TEXT("AnimationLength"))
		{
			bIsMutable = bImportAnimations;
		}

		if(bIsObjImport && InProperty->GetBoolMetaData(TEXT("OBJRestrict")))
		{
			bIsMutable = false;
		}
	}

	return bIsMutable;
}

void UFbxImportUI::ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson)
{
	// Skip instanced object references. 
	int64 SkipFlags = CPF_InstancedReference;
	FJsonObjectConverter::JsonObjectToUStruct(ImportSettingsJson, GetClass(), this, 0, SkipFlags);

	bAutomatedImportShouldDetectType = true;
	if(ImportSettingsJson->TryGetField("MeshTypeToImport").IsValid())
	{
		// Import type was specified by the user if MeshTypeToImport exists
		bAutomatedImportShouldDetectType = false;
	}

	const TSharedPtr<FJsonObject>* StaticMeshImportJson = nullptr;
	ImportSettingsJson->TryGetObjectField(TEXT("StaticMeshImportData"), StaticMeshImportJson);
	if(StaticMeshImportJson)
	{
		FJsonObjectConverter::JsonObjectToUStruct(StaticMeshImportJson->ToSharedRef(), StaticMeshImportData->GetClass(), StaticMeshImportData, 0, 0);
	}

	const TSharedPtr<FJsonObject>* SkeletalMeshImportJson = nullptr;
	ImportSettingsJson->TryGetObjectField(TEXT("SkeletalMeshImportData"), SkeletalMeshImportJson);
	if (SkeletalMeshImportJson)
	{
		FJsonObjectConverter::JsonObjectToUStruct(SkeletalMeshImportJson->ToSharedRef(), SkeletalMeshImportData->GetClass(), SkeletalMeshImportData, 0, 0);
	}

	const TSharedPtr<FJsonObject>* AnimImportJson = nullptr;
	ImportSettingsJson->TryGetObjectField(TEXT("AnimSequenceImportData"), AnimImportJson);
	if (AnimImportJson)
	{
		FJsonObjectConverter::JsonObjectToUStruct(AnimImportJson->ToSharedRef(), AnimSequenceImportData->GetClass(), AnimSequenceImportData, 0, 0);
	}

	const TSharedPtr<FJsonObject>* TextureImportJson = nullptr;
	ImportSettingsJson->TryGetObjectField(TEXT("TextureImportData"), TextureImportJson);
	if (TextureImportJson)
	{
		FJsonObjectConverter::JsonObjectToUStruct(TextureImportJson->ToSharedRef(), TextureImportData->GetClass(), TextureImportData, 0, 0);
	}
}

void UFbxImportUI::ResetToDefault()
{
	ReloadConfig();
	AnimSequenceImportData->ReloadConfig();
	StaticMeshImportData->ReloadConfig();
	SkeletalMeshImportData->ReloadConfig();
	TextureImportData->ReloadConfig();
}

#undef LOCTEXT_NAMESPACE
