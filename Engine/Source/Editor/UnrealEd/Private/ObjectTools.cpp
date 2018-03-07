// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ObjectTools.h"
#include "Engine/Level.h"
#include "UObject/UnrealType.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "Exporters/Exporter.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Serialization/FindReferencersArchive.h"
#include "Serialization/ArchiveReferenceMarker.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWindow.h"
#include "SEditableTextBox.h"
#include "RHI.h"
#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"
#include "Materials/Material.h"
#include "CanvasTypes.h"
#include "Engine/Brush.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "Engine/SkeletalMesh.h"
#include "Editor/UnrealEdEngine.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ThumbnailRendering/TextureThumbnailRenderer.h"
#include "Engine/StaticMesh.h"
#include "Factories/Factory.h"
#include "AssetToolsModule.h"
#include "Sound/SoundWave.h"
#include "GameFramework/Volume.h"
#include "UObject/MetaData.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/Selection.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserDefinedStruct.h"
#include "Animation/MorphTarget.h"
#include "Editor.h"
#include "EditorDirectories.h"
#include "FileHelpers.h"
#include "Dialogs/Dialogs.h"
#include "UnrealEdGlobals.h"
#include "PackageTools.h"
#include "Framework/Application/SlateApplication.h"

#include "BusyCursor.h"
#include "Dialogs/DlgMoveAssets.h"
#include "Dialogs/DlgReferenceTree.h"
#include "ARFilter.h"
#include "AssetDeleteModel.h"
#include "Dialogs/SDeleteAssetsDialog.h"
#include "AudioDevice.h"
#include "ReferencedAssetsUtils.h"
#include "AssetRegistryModule.h"
#include "PackagesDialog.h"
#include "Toolkits/AssetEditorManager.h"
#include "PropertyEditorModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "PackageHelperFunctions.h"
#include "EditorLevelUtils.h"
#include "DesktopPlatformModule.h"
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"
#include "ContentStreaming.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Layers/ILayers.h"
#include "Engine/SCS_Node.h"
#include "ShaderCompiler.h"
#include "UniquePtr.h"
#include "Engine/MapBuildDataRegistry.h"
#include "HAL/PlatformApplicationMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjectTools, Log, All);

// This function should ONLY be needed by ConsolidateObjects and ForceDeleteObjects
// Use anywhere else could be dangerous as this involves a map transition and GC
void ReloadEditorWorldForReferenceReplacementIfNecessary(TArray< TWeakObjectPtr<UObject> >& InOutObjectsToReplace)
{
	// If we are force-deleting or consolidating the editor world, first transition to an empty map to prevent reference problems.
	// Then, re-load the world from disk to set it up for delete as an inactive world which isn't attached to the editor engine or other systems.
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

	// Also get the map build data, we'll need reqquire it after reloading the level because it will be gc'd when NewMap is called.
	UMapBuildDataRegistry* MapBuildData = EditorWorld->PersistentLevel->MapBuildData;

	// Remove the world from ObjectsToDelete since NewMap() will delete the object naturally
	int32 NumEntriesRemoved = InOutObjectsToReplace.Remove(EditorWorld);
	if (NumEntriesRemoved > 0)
	{
		bool bMapBuildDataRemoved = false;
		if (MapBuildData)
		{
			bMapBuildDataRemoved = InOutObjectsToReplace.Remove(MapBuildData) == 1;
		}

		const FString ObjectPath = EditorWorld->GetPathName();

		// Transition to a new map. This will invoke garbage collection and destroy the EditorWorld
		GEditor->NewMap();

		// Attempt to reload the editor world so we can make sure the file gets deleted and everything is handled normally.
		// It is okay for this to fail. If we could not reload the world, it is not on disk and is gone.
		UWorld* ReloadedEditorWorld = LoadObject<UWorld>(nullptr, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
		if (ReloadedEditorWorld)
		{
			InOutObjectsToReplace.Add(ReloadedEditorWorld);

			if (bMapBuildDataRemoved && ReloadedEditorWorld->PersistentLevel->MapBuildData)
			{
				InOutObjectsToReplace.Add(ReloadedEditorWorld->PersistentLevel->MapBuildData);
			}
		}

		
	}
}

namespace ObjectTools
{
	/** Returns true if the specified object can be displayed in a content browser */
	bool IsObjectBrowsable( UObject* Obj )	// const
	{
		bool bIsSupported = false;

		// Check object prerequisites
		if (ensure(Obj) && Obj->IsAsset() )
		{
			UPackage* ObjectPackage = Obj->GetOutermost();
			if( ObjectPackage != NULL )
			{
				if( ObjectPackage != GetTransientPackage()
					&& (ObjectPackage->HasAnyPackageFlags(PKG_PlayInEditor) == false)
					&& !Obj->IsPendingKill() )
				{
					bIsSupported = true;
				}
			}
		}

		return bIsSupported;
	}

	/** 
	 * FArchiveTopLevelReferenceCollector constructor
	 * @todo: comment
	 */
	FArchiveTopLevelReferenceCollector::FArchiveTopLevelReferenceCollector(
		TArray<UObject*>* InObjectArray,
		const TArray<UObject*>& InIgnoreOuters,
		const TArray<UClass*>& InIgnoreClasses )
			:	ObjectArray( InObjectArray )
			,	IgnoreOuters( InIgnoreOuters )
			,	IgnoreClasses( InIgnoreClasses )
	{
		// Mark objects.
		for ( FObjectIterator It ; It ; ++It )
		{
			if ( ShouldSearchForAssets(*It) )
			{
				It->Mark(OBJECTMARK_TagExp);
			}
			else
			{
				It->UnMark(OBJECTMARK_TagExp);
			}
		}
	}

	/** 
	 * UObject serialize operator implementation
	 *
	 * @param Object	reference to Object reference
	 * @return reference to instance of this class
	 */
	FArchive& FArchiveTopLevelReferenceCollector::operator<<( UObject*& Obj )
	{
		if ( Obj != NULL && Obj->HasAnyMarks(OBJECTMARK_TagExp) )
		{
			// Clear the search flag so we don't revisit objects
			Obj->UnMark(OBJECTMARK_TagExp);
			if ( Obj->IsA(UField::StaticClass()) )
			{
				// skip all of the other stuff because the serialization of UFields will quickly overflow
				// our stack given the number of temporary variables we create in the below code
				Obj->Serialize(*this);
			}
			else
			{
				// Only report this object reference if it supports display in a browser.
				// this eliminates all of the random objects like functions, properties, etc.
				const bool bShouldReportAsset = ObjectTools::IsObjectBrowsable( Obj );
				if (Obj->IsValidLowLevel())
				{
					if ( bShouldReportAsset )
					{
						ObjectArray->Add( Obj );
					}
					// Check this object for any potential object references.
					Obj->Serialize(*this);
				}
			}
		}
		return *this;
	}


	void FMoveInfo::Set(const TCHAR* InFullPackageName, const TCHAR* InNewObjName)
	{
		FullPackageName = InFullPackageName;
		NewObjName = InNewObjName;
		check( IsValid() );
	}
	
	/** @return		true once valid (non-empty) move info exists. */
	bool FMoveInfo::IsValid() const
	{
		return ( FullPackageName.Len() > 0 && NewObjName.Len() > 0 );
	}

	/**
	 * Handles fully loading packages for a set of passed in objects.
	 *
	 * @param	Objects				Array of objects whose packages need to be fully loaded
	 * @param	OperationString		Localization key for a string describing the operation; appears in the warning string presented to the user.
	 * 
	 * @return true if all packages where fully loaded, false otherwise
	 */
	bool HandleFullyLoadingPackages( const TArray<UObject*>& Objects, const FText& OperationText )
	{
		// Get list of outermost packages.
		TArray<UPackage*> TopLevelPackages;
		for( int32 ObjectIndex=0; ObjectIndex<Objects.Num(); ObjectIndex++ )
		{
			UObject* Object = Objects[ObjectIndex];
			if( Object )
			{
				TopLevelPackages.AddUnique( Object->GetOutermost() );
			}
		}

		return PackageTools::HandleFullyLoadingPackages( TopLevelPackages, OperationText );
	}



	void DuplicateObjects( const TArray<UObject*>& SelectedObjects, const FString& SourcePath, const FString& DestinationPath, bool bOpenDialog, TArray<UObject*>* OutNewObjects )
	{
		if ( SelectedObjects.Num() < 1 )
		{
			return;
		}

		FMoveDialogInfo MoveDialogInfo;
		MoveDialogInfo.bOkToAll = !bOpenDialog;
		// The default value for save packages is true if SCC is enabled because the user can use SCC to revert a change
		MoveDialogInfo.bSavePackages = ISourceControlModule::Get().IsEnabled();

		bool bSawSuccessfulDuplicate = false;
		TSet<UPackage*> PackagesUserRefusedToFullyLoad;
		TArray<UPackage*> OutermostPackagesToSave;

		for( int32 ObjectIndex = 0 ; ObjectIndex < SelectedObjects.Num() ; ++ObjectIndex )
		{
			UObject* Object = SelectedObjects[ObjectIndex];
			if( !Object )
			{
				continue;
			}

			if ( !GetMoveDialogInfo(NSLOCTEXT("UnrealEd", "DuplicateObjects", "Copy Objects"), Object, /*bUniqueDefaultName=*/true, SourcePath, DestinationPath, MoveDialogInfo) )
			{
				// The user aborted the operation
				return;
			}

			UObject* NewObject = DuplicateSingleObject(Object, MoveDialogInfo.PGN, PackagesUserRefusedToFullyLoad);
			if ( NewObject != NULL )
			{
				if ( OutNewObjects != NULL )
				{
					OutNewObjects->Add(NewObject);
				}

				OutermostPackagesToSave.Add(NewObject->GetOutermost());
				bSawSuccessfulDuplicate = true;
			}
		}

		// Update the browser if something was actually moved.
		if ( bSawSuccessfulDuplicate )
		{
			bool bUpdateSCC = false;
			if ( MoveDialogInfo.bSavePackages )
			{
				const bool bCheckDirty = false;
				const bool bPromptToSave = false;
				FEditorFileUtils::PromptForCheckoutAndSave(OutermostPackagesToSave, bCheckDirty, bPromptToSave);
				bUpdateSCC = true;
			}

			if ( bUpdateSCC )
			{
				ISourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FUpdateStatus>(), OutermostPackagesToSave);
			}
		}
	}

	UObject* DuplicateSingleObject(UObject* Object, const FPackageGroupName& PGN, TSet<UPackage*>& InOutPackagesUserRefusedToFullyLoad, bool bPromptToOverwrite)
	{
		UObject* ReturnObject = NULL;

		const FString& NewPackageName = PGN.PackageName;
		const FString& NewGroupName = PGN.GroupName;
		const FString& NewObjectName = PGN.ObjectName;

		const FScopedBusyCursor BusyCursor;

		// Check validity of each reference dup name.
		FString ErrorMessage;
		FText Reason;
		FString ObjectsToOverwriteName;
		FString ObjectsToOverwritePackage;
		FString ObjectsToOverwriteClass;
		TArray<UObject*> ObjectsToDelete;
		bool	bUserDeclinedToFullyLoadPackage = false;

		FMoveInfo MoveInfo;

		// Make sure that a target package exists.
		if ( !NewPackageName.Len() )
		{
			ErrorMessage += TEXT("Invalid package name supplied\n");
		}
		else
		{
			// Make a full path from the target package and group.
			const FString FullPackageName = NewGroupName.Len()
				? FString::Printf(TEXT("%s.%s"), *NewPackageName, *NewGroupName)
				: NewPackageName;

			// Make sure the packages being duplicated into are fully loaded.
			TArray<UPackage*> TopLevelPackages;
			UPackage* ExistingPackage = FindPackage(NULL, *FullPackageName);

			// If we did not find the package, it may not be loaded at all.
			if ( !ExistingPackage )
			{
				FString Filename;
				if ( FPackageName::DoesPackageExist(FullPackageName, NULL, &Filename) )
				{
					// There is an unloaded package file at the destination.
					ExistingPackage = LoadPackage(NULL, *FullPackageName, LOAD_None);
				}
			}

			if( ExistingPackage )
			{
				TopLevelPackages.Add( ExistingPackage->GetOutermost() );
			}

			if( (ExistingPackage && InOutPackagesUserRefusedToFullyLoad.Contains(ExistingPackage)) ||
				!PackageTools::HandleFullyLoadingPackages( TopLevelPackages, NSLOCTEXT("UnrealEd", "Duplicate", "Duplicate") ) )
			{
				// HandleFullyLoadingPackages should never return false for empty input.
				check( ExistingPackage );
				InOutPackagesUserRefusedToFullyLoad.Add( ExistingPackage );
				bUserDeclinedToFullyLoadPackage = true;
			}
			else
			{
				UObject* ExistingObject = ExistingPackage ? StaticFindObject(UObject::StaticClass(), ExistingPackage, *NewObjectName) : NULL;

				if( !NewObjectName.Len() )
				{
					ErrorMessage += TEXT("Invalid object name\n");
				}
				else if(!FName(*NewObjectName).IsValidObjectName( Reason ) 
					||	!FPackageName::IsValidLongPackageName( NewPackageName, /*bIncludeReadOnlyRoots=*/false, &Reason )
					||	!FName(*NewGroupName).IsValidGroupName( Reason,true) )
				{
					// Make sure the object name is valid.
					ErrorMessage += FString::Printf(TEXT("    %s to %s.%s: %s\n"), *Object->GetPathName(), *FullPackageName, *NewObjectName, *Reason.ToString() );
				}
				else if (ExistingObject == Object)
				{
					ErrorMessage += TEXT("Can't duplicate an object onto itself!\n");
				}
				else
				{
					// If the object already exists in this package with the given name, give the user 
					// the opportunity to overwrite the object. So, don't treat this as an error.
					if ( ExistingPackage && !IsUniqueObjectName(*NewObjectName, ExistingPackage, Reason) )
					{
						ObjectsToOverwriteName += *NewObjectName;
						ObjectsToOverwritePackage += *FullPackageName;
						ObjectsToOverwriteClass += *ExistingObject->GetClass()->GetName();

						ObjectsToDelete.Add(ExistingObject);
					}

					// NOTE: Set the move info if this object already exists in-case the user wants to 
					// overwrite the existing asset. To overwrite the object, the move info is needed.

					// No errors!  Set asset move info.
					MoveInfo.Set( *FullPackageName, *NewObjectName );
				}
			}
		}

		// User declined to fully load the target package; no need to display message box.
		if( bUserDeclinedToFullyLoadPackage )
		{
			return NULL;
		}

		// If any errors are present, display them and abort this object.
		else if( ErrorMessage.Len() > 0 )
		{
			FMessageDialog::Open( EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "CannotDuplicateList", "Cannot duplicate object: '{0}'\n{1}"), FText::FromString(Object->GetName()), FText::FromString(ErrorMessage)) );
			return NULL;
		}

		// If there are objects that already exist with the same name, give the user the option to overwrite the 
		// object. This will delete the object so the new one can be created in its place.
		if(bPromptToOverwrite && ObjectsToOverwriteName.Len() > 0 )
		{
			bool bOverwriteExistingObjects =
				EAppReturnType::Yes == FMessageDialog::Open(
				EAppMsgType::YesNo,
				FText::Format(
				NSLOCTEXT("UnrealEd", "ReplaceExistingObjectInPackage_F", "An object [{0}] of class [{1}] already exists in file [{2}].  Do you want to replace the existing object?  If you click 'Yes', the existing object will be deleted.  Otherwise, click 'No' and choose a unique name for your new object." ),
				FText::FromString(ObjectsToOverwriteName),
				FText::FromString(ObjectsToOverwriteClass),
				FText::FromString(ObjectsToOverwritePackage) ) );					

			// The user didn't want to overwrite the existing opitons, so bail out of the duplicate operation.
			if( !bOverwriteExistingObjects )
			{
				return NULL;
			}
		}

		// If some objects need to be deleted, delete them.
		if (ObjectsToDelete.Num() > 0)
		{
			TArray<UPackage*> DeletedObjectPackages;

			// Add all packages for deleted objects to the root set if they are not already so we can reuse them later.
			// This will prevent DeleteObjects from marking the file for delete in source control
			for ( auto ObjIt = ObjectsToDelete.CreateConstIterator(); ObjIt; ++ObjIt )
			{
				UPackage* Pkg = (*ObjIt)->GetOutermost();

				if ( Pkg && !Pkg->IsRooted() )
				{
					DeletedObjectPackages.AddUnique(Pkg);
					Pkg->AddToRoot();
				}
			}

			const int32 NumObjectsDeleted = ObjectTools::DeleteObjects(ObjectsToDelete, bPromptToOverwrite);

			// Remove all packages that we added to the root set above.
			for ( auto PkgIt = DeletedObjectPackages.CreateConstIterator(); PkgIt; ++PkgIt )
			{
				(*PkgIt)->RemoveFromRoot();
			}

			if (NumObjectsDeleted != ObjectsToDelete.Num())
			{
				UE_LOG(LogObjectTools, Warning,
					TEXT("Existing objects could not be deleted, unable to duplicate %s"),
					*Object->GetFullName());
				return NULL;
			}
		}

		// Create ReplacementMap for replacing references.
		TMap<UObject*, UObject*> ReplacementMap;

		check( MoveInfo.IsValid() );

		const FString& PkgName = MoveInfo.FullPackageName;
		const FString& ObjName = MoveInfo.NewObjName;

		// Make sure the referenced object is deselected before duplicating it.
		GEditor->GetSelectedObjects()->Deselect( Object );

		UObject* DupObject = NULL;

		UPackage* ExistingPackage = FindPackage(NULL, *PkgName);
		UObject* ExistingObject = ExistingPackage ? StaticFindObject(UObject::StaticClass(), ExistingPackage, *ObjName) : NULL;

		// Any existing objects should be deleted and garbage collected by now
		if ( ensure(ExistingObject == NULL) )
		{
			EDuplicateMode::Type DuplicateMode = Object->IsA(UWorld::StaticClass()) ? EDuplicateMode::World : EDuplicateMode::Normal;
			DupObject = StaticDuplicateObject( Object, CreatePackage(NULL,*PkgName), *ObjName, RF_AllFlags, nullptr, DuplicateMode );
		}

		if( DupObject )
		{
			ReplacementMap.Add( Object, DupObject );
			DupObject->MarkPackageDirty();

			// if the source object is in the MyLevel package and it's being duplicated into a content package, we need
			// to mark it RF_Standalone so that it will be saved (UWorld::CleanupWorld() clears this flag for all objects
			// inside the package)
			if (!Object->HasAnyFlags(RF_Standalone)
				&&	Object->GetOutermost()->ContainsMap()
				&&	!DupObject->GetOutermost()->ContainsMap() )
			{
				DupObject->SetFlags(RF_Standalone);
			}

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(DupObject);

			ReturnObject = DupObject;
		}

		GEditor->GetSelectedObjects()->Select( Object );

		// Replace all references
		FArchiveReplaceObjectRef<UObject> ReplaceAr( DupObject, ReplacementMap, false, true, true );

		return ReturnObject;
	}
		
	/**
	 * Helper struct for passing multiple arrays to and from ForceReplaceReferences
	 */
	struct FForceReplaceInfo
	{
		// A list of packages which were dirtied as a result of a force replace
		TArray<UPackage*> DirtiedPackages;
		// Objects whose references were successfully replaced
		TArray<UObject*> ReplaceableObjects;
		// Objects whose references could not be successfully replaced
		TArray<UObject*> UnreplaceableObjects;

		void AppendUnique(const FForceReplaceInfo& ForceReplaceInfo)
		{
			const TArray<UPackage*>& ForceReplaceInfoDirtiedPackages = ForceReplaceInfo.DirtiedPackages;
			DirtiedPackages.Reserve(DirtiedPackages.Num() + ForceReplaceInfoDirtiedPackages.Num());
			for (UPackage* Package : ForceReplaceInfoDirtiedPackages)
			{
				DirtiedPackages.AddUnique(Package);
			}

			const TArray<UObject*>& ForceReplaceInfoReplaceableObjects = ForceReplaceInfo.ReplaceableObjects;
			ReplaceableObjects.Reserve(ReplaceableObjects.Num() + ForceReplaceInfoReplaceableObjects.Num());
			for (UObject* Object : ForceReplaceInfoReplaceableObjects)
			{
				ReplaceableObjects.Add(Object);
			}

			const TArray<UObject*>& ForceReplaceInfoUnreplaceableObjects = ForceReplaceInfo.UnreplaceableObjects;
			UnreplaceableObjects.Reserve(UnreplaceableObjects.Num() + ForceReplaceInfoUnreplaceableObjects.Num());
			for (UObject* Object : ForceReplaceInfoUnreplaceableObjects)
			{
				UnreplaceableObjects.Add(Object);
			}
		}
	};

	/**
	 * Forcefully replaces references to passed in objects
	 *
	 * @param ObjectToReplaceWith	Any references found to 'ObjectsToReplace' will be replaced with this object.  If the object is NULL references will be nulled.
	 * @param ObjectsToReplace		An array of objects that should be replaced with 'ObjectToReplaceWith'
	 * @param OutInfo				FForceReplaceInfo struct containing useful information about the result of the call to this function
	 * @param bWarnAboutRootSet		If True a message will be displayed to a user asking them if they would like to remove the rootset flag from objects which have it set.  
									If False, the message will not be displayed and rootset is automatically removed 
	 */
	static void ForceReplaceReferences( UObject* ObjectToReplaceWith, TArray<UObject*>& ObjectsToReplace, FForceReplaceInfo& OutInfo, bool bWarnAboutRootSet = true)				
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.RemoveDeletedObjects( ObjectsToReplace );

		TSet<UObject*> RootSetObjects;

		GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_RootSetCheck", "Checking Assets for Root Set...") );

		// Iterate through all the objects to replace and see if they are in the root set.  If they are, offer to remove them from the root set.
		for ( TArray<UObject*>::TConstIterator ReplaceItr( ObjectsToReplace ); ReplaceItr; ++ReplaceItr )
		{
			UObject* CurObjToReplace = *ReplaceItr;
			if ( CurObjToReplace )
			{
				const bool bFlaggedRootSet = CurObjToReplace->IsRooted();
				if ( bFlaggedRootSet )
				{
					RootSetObjects.Add( CurObjToReplace );
				}
			}
		}
		if ( RootSetObjects.Num() )
		{
			if( bWarnAboutRootSet )
			{
				// Collect names of root set assets
				FString RootSetObjectNames;
				for ( TSet<UObject*>::TConstIterator RootSetIter( RootSetObjects ); RootSetIter; ++RootSetIter )
				{
					UObject* CurRootSetObject = *RootSetIter;
					RootSetObjectNames += CurRootSetObject->GetName() + TEXT("\n");
				}

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Objects"), FText::FromString( RootSetObjectNames ));
				FText MessageFormatting = NSLOCTEXT("ObjectTools", "ConsolidateAssetsRootSetDlgMsgFormatting", "The assets below were in the root set and we must remove that flag in order to proceed.  Being in the root set means that this was loaded at startup and is meant to remain in memory during gameplay.  For most assets this should be fine.  If, for some reason, there is an error, you will be notified.  Would you like to remove this flag?\n\n{Objects}");
				FText Message = FText::Format( MessageFormatting, Arguments );

				// Prompt the user to see if they'd like to remove the root set flag from the assets and attempt to replace them
				EAppReturnType::Type UserRepsonse = OpenMsgDlgInt( EAppMsgType::YesNo, Message, NSLOCTEXT("ObjectTools", "ConsolidateAssetsRootSetDlg_Title", "Failed to Consolidate Assets") );

				// The user elected to not remove the root set flag, so cancel the replacement
				if ( UserRepsonse == EAppReturnType::No )
				{
					return;
				}
			}

			for ( FObjectIterator ObjIter; ObjIter; ++ObjIter )
			{
				UObject* CurrentObject = *ObjIter;
				if ( CurrentObject )
				{
					// If the current object is one of the objects the user is attempting to replace but is marked RF_RootSet, strip the flag by removing it
					// from root
					if ( RootSetObjects.Find( CurrentObject ) )
					{
						CurrentObject->RemoveFromRoot();
					}
					// If the current object is inside one of the objects to replace but is marked RF_RootSet, strip the flag by removing it from root
					else
					{
						for( UObject* CurObjOuter = CurrentObject->GetOuter(); CurObjOuter; CurObjOuter = CurObjOuter->GetOuter() )
						{
							if ( RootSetObjects.Find( CurObjOuter ) )
							{
								CurrentObject->RemoveFromRoot();
								break;
							}
						}
					}
				}
			}
		}

		// Reset linker loaders to remove the possibility that any references to 'ObjectsToReplace' exist in the loaders (these can't get picked up by the replace archives)
		ResetLoaders(nullptr);

		TMap<UObject*, int32> ObjToNumRefsMap;
		if( ObjectToReplaceWith != NULL )
		{
			GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_CheckAssetValidity", "Determining Validity of Assets...") );
			// Determine if the "object to replace with" has any references to any of the "objects to replace," if so, we don't
			// want to allow those objects to be replaced, as the object would end up referring to itself!
			// We can skip this check if "object to replace with" is NULL since it is not useful to check for null references
			FFindReferencersArchive FindRefsAr( ObjectToReplaceWith, ObjectsToReplace );
			FindRefsAr.GetReferenceCounts( ObjToNumRefsMap );
		}

		// Objects already loaded and in memory have to have any of their references to the objects to replace swapped with a reference to
		// the "object to replace with". FArchiveReplaceObjectRef can serve this purpose, but it expects a TMap of object to replace : object to replace with. 
		// Therefore, populate a map with all of the valid objects to replace as keys, with the object to replace with as the value for each one.
		TMap<UObject*, UObject*> ReplacementMap;
		for ( TArray<UObject*>::TConstIterator ReplaceItr( ObjectsToReplace ); ReplaceItr; ++ReplaceItr )
		{
			UObject* CurObjToReplace = *ReplaceItr;
			if ( CurObjToReplace )
			{				
				// If any of the objects to replace are marked RF_RootSet at this point, an error has occurred
				const bool bFlaggedRootSet = CurObjToReplace->IsRooted();
				check( !bFlaggedRootSet );

				// Exclude root packages from being replaced
				const bool bRootPackage = ( CurObjToReplace->GetClass() == UPackage::StaticClass() ) && !( CurObjToReplace->GetOuter() );

				// Additionally exclude any objects that the "object to replace with" contains references to, in order to prevent the "object to replace with" from
				// referring to itself
				int32 NumRefsInObjToReplaceWith = 0;
				int32* PtrToNumRefs = ObjToNumRefsMap.Find( CurObjToReplace );
				if ( PtrToNumRefs )
				{
					NumRefsInObjToReplaceWith = *PtrToNumRefs;
				}

				if ( !bRootPackage && NumRefsInObjToReplaceWith == 0 )
				{
					ReplacementMap.Add( CurObjToReplace, ObjectToReplaceWith );

					// Fully load the packages of objects to replace
					CurObjToReplace->GetOutermost()->FullyLoad();
				}
				// If an object is "unreplaceable" store it separately to warn the user about later
				else
				{
					OutInfo.UnreplaceableObjects.AddUnique(CurObjToReplace);
				}
			}
		}

		GWarn->StatusUpdate( 0, 0, NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_FindingReferences", "Finding Asset References...") );

		ReplacementMap.GenerateKeyArray( OutInfo.ReplaceableObjects );

		// Find all the properties (and their corresponding objects) that refer to any of the objects to be replaced
		TMap< UObject*, TArray<UProperty*> > ReferencingPropertiesMap;
		for ( FObjectIterator ObjIter; ObjIter; ++ObjIter )
		{
			UObject* CurObject = *ObjIter;

			// Unless the "object to replace with" is null, ignore any of the objects to replace to themselves
			if ( ObjectToReplaceWith == NULL || !ReplacementMap.Find( CurObject ) )
			{
				// Find the referencers of the objects to be replaced
				FFindReferencersArchive FindRefsArchive( CurObject, OutInfo.ReplaceableObjects );

				// Inform the object referencing any of the objects to be replaced about the properties that are being forcefully
				// changed, and store both the object doing the referencing as well as the properties that were changed in a map (so that
				// we can correctly call PostEditChange later)
				TMap<UObject*, int32> CurNumReferencesMap;
				TMultiMap<UObject*, UProperty*> CurReferencingPropertiesMMap;
				if ( FindRefsArchive.GetReferenceCounts( CurNumReferencesMap, CurReferencingPropertiesMMap ) > 0  )
				{
					TArray<UProperty*> CurReferencedProperties;
					CurReferencingPropertiesMMap.GenerateValueArray( CurReferencedProperties );
					ReferencingPropertiesMap.Add( CurObject, CurReferencedProperties );
					if ( CurReferencedProperties.Num() > 0)
					{
						for ( TArray<UProperty*>::TConstIterator RefPropIter( CurReferencedProperties ); RefPropIter; ++RefPropIter )
						{
							CurObject->PreEditChange( *RefPropIter );
						}
					}
					else
					{
						CurObject->PreEditChange(nullptr);
					}
				}
			}
		}
		
		// Iterate over the map of referencing objects/changed properties, forcefully replacing the references and
		int32 NumObjsReplaced = 0;
		for ( TMap< UObject*, TArray<UProperty*> >::TConstIterator MapIter( ReferencingPropertiesMap ); MapIter; ++MapIter )
		{
			++NumObjsReplaced;
			GWarn->StatusUpdate( NumObjsReplaced, ReferencingPropertiesMap.Num(), NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_ReplacingReferences", "Replacing Asset References...") );

			UObject* CurReplaceObj = MapIter.Key();

			FArchiveReplaceObjectRef<UObject> ReplaceAr( CurReplaceObj, ReplacementMap, false, true, false );
		}

		// Now alter the referencing objects the change has completed via PostEditChange, 
		// this is done in a separate loop to prevent reading of data that we want to overwrite
		int32 NumObjsPostEdited = 0;
		for ( TMap< UObject*, TArray<UProperty*> >::TConstIterator MapIter( ReferencingPropertiesMap ); MapIter; ++MapIter )
		{
			++NumObjsPostEdited;
			GWarn->StatusUpdate( NumObjsPostEdited, ReferencingPropertiesMap.Num(), NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_PostEditing", "Performing Post Update Edits...") );

			UObject* CurReplaceObj = MapIter.Key();
			const TArray<UProperty*>& RefPropArray = MapIter.Value();

			if (RefPropArray.Num() > 0)
			{
				for ( TArray<UProperty*>::TConstIterator RefPropIter( RefPropArray ); RefPropIter; ++RefPropIter )
				{
					FPropertyChangedEvent PropertyEvent(*RefPropIter, EPropertyChangeType::Redirected);
					CurReplaceObj->PostEditChangeProperty( PropertyEvent );
				}
			}
			else
			{
				FPropertyChangedEvent PropertyEvent(nullptr, EPropertyChangeType::Redirected);
				CurReplaceObj->PostEditChangeProperty(PropertyEvent);
			}

			if ( !CurReplaceObj->HasAnyFlags(RF_Transient) && CurReplaceObj->GetOutermost() != GetTransientPackage() )
			{
				if ( !CurReplaceObj->RootPackageHasAnyFlags(PKG_CompiledIn) )
				{
					CurReplaceObj->MarkPackageDirty();
					OutInfo.DirtiedPackages.AddUnique( CurReplaceObj->GetOutermost() );
				}
			}
		}
	}

	FConsolidationResults ConsolidateObjects( UObject* ObjectToConsolidateTo, TArray<UObject*>& ObjectsToConsolidate, bool bShowDeleteConfirmation )
	{
		FConsolidationResults ConsolidationResults;

		// Ensure the consolidation is headed toward a valid object and this isn't occurring in game
		if ( ObjectToConsolidateTo )
		{
			// Confirm that the consolidate was intentional
			if ( bShowDeleteConfirmation )
			{
				if ( !ShowDeleteConfirmationDialog( ObjectsToConsolidate ) )
				{
					return ConsolidationResults;
				}
			}

			// Close all editors to avoid changing references to temporary objects used by the editor
			if ( !FAssetEditorManager::Get().CloseAllAssetEditors() )
			{
				// Failed to close at least one editor. It is possible that this editor has in-memory object references
				// which are not prepared to be changed dynamically so it is not safe to continue
				return ConsolidationResults;
			}

			GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_Consolidating", "Consolidating Assets..." ), true );

			// Clear audio components to allow previewed sounds to be consolidated
			GEditor->ClearPreviewComponents();

			// Make sure none of the objects are referenced by the editor's USelection
			GEditor->GetSelectedObjects()->Deselect( ObjectToConsolidateTo );
			for (int32 ObjectIdx = 0; ObjectIdx < ObjectsToConsolidate.Num(); ++ObjectIdx)
			{
				GEditor->GetSelectedObjects()->Deselect( ObjectsToConsolidate[ObjectIdx] );
			}

			// Keep track of which objects, if any, cannot be consolidated, in order to notify the user later
			TArray<UObject*> UnconsolidatableObjects;

			// Keep track of objects which became partially consolidated but couldn't be deleted for some reason;
			// these are critical failures, and the user needs to be alerted
			TArray<UObject*> CriticalFailureObjects;

			// Keep track of which packages the consolidate operation has dirtied so the user can be alerted to them
			// during a critical failure
			TArray<UPackage*> DirtiedPackages;

			// Keep track of root set objects so the user can be prompted about stripping the flag from them
			TSet<UObject*> RootSetObjects;

			// List of objects successfully deleted
			TArray<UObject*> ConsolidatedObjects;

			// A list of names for object redirectors created during the delete process
			// This is needed because the redirectors may not have the same name as the
			// objects they are replacing until the objects are garbage collected
			TMap<UObjectRedirector*, FName> RedirectorToObjectNameMap;

			{
				// Note reloading the world via ReloadEditorWorldForReferenceReplacementIfNecessary will cause a garbage collect and potentially cause entries in the ObjectsToConsolidate list to become invalid
				// We refresh the list here after reloading the editor world
				TArray< TWeakObjectPtr<UObject> > ObjectsToConsolidateWeakList;
				for(UObject* Object : ObjectsToConsolidate)
				{
					ObjectsToConsolidateWeakList.Add(Object);
				}

				ObjectsToConsolidate.Empty();

				// If the current editor world is in this list, transition to a new map and reload the world to finish the delete
				ReloadEditorWorldForReferenceReplacementIfNecessary(ObjectsToConsolidateWeakList);

				for(TWeakObjectPtr<UObject> WeakObject : ObjectsToConsolidateWeakList)
				{
					if( WeakObject.IsValid() )
					{
						ObjectsToConsolidate.Add(WeakObject.Get());
					}
				}
			}

			FForceReplaceInfo ReplaceInfo;
			FForceReplaceInfo GeneratedClassReplaceInfo;

			// Scope the reregister context below to complete after object deletion and before garbage collection
			{
				// Replacing references inside already loaded objects could cause rendering issues, so globally detach all components from their scenes for now
				FGlobalComponentRecreateRenderStateContext ReregisterContext;
				
				ForceReplaceReferences(ObjectToConsolidateTo, ObjectsToConsolidate, ReplaceInfo);

				if (UBlueprint* ObjectToConsolidateTo_BP = Cast<UBlueprint>(ObjectToConsolidateTo))
				{
					// Replace all UClass/TSubClassOf properties of generated class.
					TArray<UObject*> ObjectsToConsolidate_BP;
					TArray<UClass*> OldGeneratedClasses;
					ObjectsToConsolidate_BP.Reserve(ObjectsToConsolidate.Num());
					OldGeneratedClasses.Reserve(ObjectsToConsolidate.Num());
					for (UObject* ObjectToConsolidate : ObjectsToConsolidate)
					{
						UClass* OldGeneratedClass = Cast<UBlueprint>(ObjectToConsolidate)->GeneratedClass;
						ObjectsToConsolidate_BP.Add(OldGeneratedClass);
						OldGeneratedClasses.Add(OldGeneratedClass);
					}

					ForceReplaceReferences(ObjectToConsolidateTo_BP->GeneratedClass, ObjectsToConsolidate_BP, GeneratedClassReplaceInfo);

					// Repair the references of GeneratedClass on the object being consolidated so they can be properly disposed of upon deletion.
					for (int32 Index = 0, MaxIndex = ObjectsToConsolidate.Num(); Index < MaxIndex; ++Index)
					{
						Cast<UBlueprint>(ObjectsToConsolidate[Index])->GeneratedClass = OldGeneratedClasses[Index];
					}

					ReplaceInfo.AppendUnique(GeneratedClassReplaceInfo);
				}
				DirtiedPackages.Append( ReplaceInfo.DirtiedPackages );
				UnconsolidatableObjects.Append( ReplaceInfo.UnreplaceableObjects );
			}

			// See if this is a blueprint consolidate and replace instances of the generated class
			UBlueprint* BlueprintToConsolidateTo = Cast<UBlueprint>(ObjectToConsolidateTo);
			if ( BlueprintToConsolidateTo != NULL && ensure(BlueprintToConsolidateTo->GeneratedClass) )
			{
				for ( TArray<UObject*>::TConstIterator ConsolIter( ReplaceInfo.ReplaceableObjects ); ConsolIter; ++ConsolIter )
				{
					UBlueprint* BlueprintToConsolidate = Cast<UBlueprint>(*ConsolIter);
					if ( BlueprintToConsolidate != NULL && ensure(BlueprintToConsolidate->GeneratedClass) )
					{
						// Replace all instances of objects based on the old blueprint's class with objects based on the new class,
						// then repair the references on the object being consolidated so those objects can be properly disposed of upon deletion.
						UClass* OldClass = BlueprintToConsolidate->GeneratedClass;
						UClass* OldSkeletonClass = BlueprintToConsolidate->SkeletonGeneratedClass;
						FBlueprintCompileReinstancer::ReplaceInstancesOfClass(OldClass, BlueprintToConsolidateTo->GeneratedClass, nullptr, nullptr, true );
						BlueprintToConsolidate->GeneratedClass = OldClass;
						BlueprintToConsolidate->SkeletonGeneratedClass = OldSkeletonClass;
					}
				}

				// Clean up the actors we replaced
				CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
			}

			FEditorDelegates::OnAssetsPreDelete.Broadcast(ReplaceInfo.ReplaceableObjects);

			TSet<FString> AlreadyMappedObjectPaths;

			// With all references to the objects to consolidate to eliminated from objects that are currently loaded, it should now be safe to delete
			// the objects to be consolidated themselves, leaving behind a redirector in their place to fix up objects that were not currently loaded at the time
			// of this operation.
			for ( TArray<UObject*>::TConstIterator ConsolIter( ReplaceInfo.ReplaceableObjects ); ConsolIter; ++ConsolIter )
			{
				GWarn->StatusUpdate( ConsolIter.GetIndex(), ReplaceInfo.ReplaceableObjects.Num(), NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_DeletingObjects", "Deleting Assets...") );

				UObject* CurObjToConsolidate = *ConsolIter;
				UObject* CurObjOuter = CurObjToConsolidate->GetOuter();
				UPackage* CurObjPackage = CurObjToConsolidate->GetOutermost();
				const FName CurObjName = CurObjToConsolidate->GetFName();
				const FString CurObjPath = CurObjToConsolidate->GetPathName();
				UBlueprint* BlueprintToConsolidate = Cast<UBlueprint>(CurObjToConsolidate);

				// Attempt to delete the object that was consolidated
				if ( DeleteSingleObject( CurObjToConsolidate ) )
				{
					// DONT GC YET!!! we still need these objects around to notify other tools that they are gone and to create redirectors
					ConsolidatedObjects.Add(CurObjToConsolidate);

					if ( AlreadyMappedObjectPaths.Contains(CurObjPath) )
					{
						continue;
					}

					// Create a redirector with a unique name
					// It will have the same name as the object that was consolidated after the garbage collect
					UObjectRedirector* Redirector = NewObject<UObjectRedirector>(CurObjOuter, NAME_None, RF_Standalone | RF_Public);
					check( Redirector );

					// Set the redirector to redirect to the object to consolidate to
					Redirector->DestinationObject = ObjectToConsolidateTo;

					// Keep track of the object name so we can rename the redirector later
					RedirectorToObjectNameMap.Add(Redirector, CurObjName);
					AlreadyMappedObjectPaths.Add(CurObjPath);

					// If consolidating blueprints, make sure redirectors are created for the consolidated blueprint class and CDO
					if ( BlueprintToConsolidateTo != NULL && BlueprintToConsolidate != NULL )
					{
						// One redirector for the class
						UObjectRedirector* ClassRedirector = NewObject<UObjectRedirector>(CurObjOuter, NAME_None, RF_Standalone | RF_Public);
						check( ClassRedirector );
						ClassRedirector->DestinationObject = BlueprintToConsolidateTo->GeneratedClass;
						RedirectorToObjectNameMap.Add(ClassRedirector, BlueprintToConsolidate->GeneratedClass->GetFName());
						AlreadyMappedObjectPaths.Add(BlueprintToConsolidate->GeneratedClass->GetPathName());

						// One redirector for the CDO
						UObjectRedirector* CDORedirector = NewObject<UObjectRedirector>(CurObjOuter, NAME_None, RF_Standalone | RF_Public);
						check( CDORedirector );
						CDORedirector->DestinationObject = BlueprintToConsolidateTo->GeneratedClass->GetDefaultObject();
						RedirectorToObjectNameMap.Add(CDORedirector, BlueprintToConsolidate->GeneratedClass->GetDefaultObject()->GetFName());
						AlreadyMappedObjectPaths.Add(BlueprintToConsolidate->GeneratedClass->GetDefaultObject()->GetPathName());
					}

					DirtiedPackages.AddUnique( CurObjPackage );
				}
				// If the object couldn't be deleted, store it in the array that will be used to show the user which objects had errors
				else
				{
					CriticalFailureObjects.Add( CurObjToConsolidate );
				}
			}

			TArray<UPackage*> PotentialPackagesToDelete;
			for ( int32 ObjIdx = 0; ObjIdx < ConsolidatedObjects.Num(); ++ObjIdx )
			{
				PotentialPackagesToDelete.AddUnique(ConsolidatedObjects[ObjIdx]->GetOutermost());
			}

			CleanupAfterSuccessfulDelete(PotentialPackagesToDelete);

			// Empty the provided array so it's not full of pointers to deleted objects
			ObjectsToConsolidate.Empty();
			ConsolidatedObjects.Empty();

			// Now that the old objects have been garbage collected, give the redirectors a proper name
			for (TMap<UObjectRedirector*, FName>::TIterator RedirectIt(RedirectorToObjectNameMap); RedirectIt; ++RedirectIt)
			{
				UObjectRedirector* Redirector = RedirectIt.Key();
				const FName ObjName = RedirectIt.Value();

				if ( Redirector->Rename(*ObjName.ToString(), NULL, REN_Test) )
				{
					Redirector->Rename(*ObjName.ToString(), NULL, REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
					FAssetRegistryModule::AssetCreated(Redirector);
				}
				else
				{
					// Could not rename the redirector back to the original object's name. This indicates the original
					// object could not be garbage collected even though DeleteSingleObject returned true.
					CriticalFailureObjects.AddUnique(Redirector);
				}
			}

			GWarn->EndSlowTask();

			ConsolidationResults.DirtiedPackages = DirtiedPackages;
			ConsolidationResults.FailedConsolidationObjs = CriticalFailureObjects;
			ConsolidationResults.InvalidConsolidationObjs = UnconsolidatableObjects;

			// If some objects failed to consolidate, notify the user of the failed objects
			if ( UnconsolidatableObjects.Num() > 0 )
			{
				FString FailedObjectNames;
				for ( TArray<UObject*>::TConstIterator FailedIter( UnconsolidatableObjects ); FailedIter; ++FailedIter )
				{
					UObject* CurFailedObject = *FailedIter;
					FailedObjectNames += CurFailedObject->GetName() + TEXT("\n");
				}

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Objects"), FText::FromString( FailedObjectNames ));
				FText MessageFormatting = NSLOCTEXT("ObjectTools", "ConsolidateAssetsFailureDlgMFormattings", "The assets below were unable to be consolidated. This is likely because they are referenced by the object to consolidate to.\n\n{Objects}");
				FText Message = FText::Format( MessageFormatting, Arguments );

				OpenMsgDlgInt( EAppMsgType::Ok, Message, NSLOCTEXT("ObjectTools", "ConsolidateAssetsFailureDlg_Title", "Failed to Consolidate Assets") );
			}

			// Alert the user to critical object failure
			if ( CriticalFailureObjects.Num() > 0 )
			{
				FString CriticalFailedObjectNames;
				for ( TArray<UObject*>::TConstIterator FailedIter( CriticalFailureObjects ); FailedIter; ++FailedIter )
				{
					const UObject* CurFailedObject = *FailedIter;
					CriticalFailedObjectNames += CurFailedObject->GetName() + TEXT("\n");
				}

				FString DirtiedPackageNames;
				for ( TArray<UPackage*>::TConstIterator DirtyPkgIter( DirtiedPackages ); DirtyPkgIter; ++DirtyPkgIter )
				{
					const UPackage* CurDirtyPkg = *DirtyPkgIter;
					DirtiedPackageNames += CurDirtyPkg->GetName() + TEXT("\n");
				}

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Assets"), FText::FromString( CriticalFailedObjectNames ));
				Arguments.Add(TEXT("Packages"), FText::FromString( DirtiedPackageNames ));
				FText MessageFormatting = NSLOCTEXT("ObjectTools", "ConsolidateAssetsCriticalFailureDlgMsgFormatting", "CRITICAL FAILURE:\nOne or more assets were partially consolidated, yet still cannot be deleted for some reason. It is highly recommended that you restart the editor without saving any of the assets or packages.\n\nAffected Assets:\n{Assets}\n\nPotentially Affected Packages:\n{Packages}");
				FText Message = FText::Format( MessageFormatting, Arguments );

				OpenMsgDlgInt( EAppMsgType::Ok, Message, NSLOCTEXT("ObjectTools", "ConsolidateAssetsCriticalFailureDlg_Title", "Critical Failure to Consolidate Assets") );
			}
		}

		return ConsolidationResults;
	}

	/**
	 * Copies references for selected generic browser objects to the clipboard.
	 */
	void CopyReferences( const TArray< UObject* >& SelectedObjects ) // const
	{
		FString Ref;
		for ( int32 Index = 0 ; Index < SelectedObjects.Num() ; ++Index )
		{
			if( Ref.Len() )
			{
				Ref += LINE_TERMINATOR;
			}
			Ref += SelectedObjects[Index]->GetPathName();
		}

		FPlatformApplicationMisc::ClipboardCopy( *Ref );
	}

	/**
	 * Show the referencers of a selected object
	 *
	 * @param SelectedObjects	Array of the currently selected objects; the referencers of the first object are shown
	 */
	void ShowReferencers( const TArray< UObject* >& SelectedObjects ) // const
	{
		if( SelectedObjects.Num() > 0 )
		{
			UObject* Object = SelectedObjects[ 0 ];
			if ( Object )
			{
				GEditor->GetSelectedObjects()->Deselect( Object );

				CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

				FReferencerInformationList Refs;

				if (IsReferenced(Object, RF_Public, EInternalObjectFlags::Native, true, &Refs))
				{
					FStringOutputDevice Ar;
					Object->OutputReferencers(Ar, &Refs);
					UE_LOG(LogObjectTools, Warning, TEXT("%s"), *Ar);  // also print the objects to the log so you can actually utilize the data

					// Display a dialog containing all referencers; the dialog is designed to destroy itself upon being closed, so this
					// allocation is ok and not a memory leak
					SGenericDialogWidget::OpenDialog(NSLOCTEXT("ObjectTools", "ShowReferencers", "Show Referencers"), SNew(SEditableTextBox).Text(FText::FromString(Ar)).IsReadOnly(true));
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "ObjectNotReferenced", "Object '{0}' Is Not Referenced"), FText::FromString(Object->GetName())));
				}

				GEditor->GetSelectedObjects()->Select( Object );
			}
		}
	}

	/**
	 * Displays a tree(currently) of all assets which reference the passed in object.  
	 *
	 * @param ObjectToGraph		The object to find references to.
	 */
	void ShowReferenceGraph( UObject* ObjectToGraph )
	{
		SReferenceTree::OpenDialog(ObjectToGraph);
	}

	/**
	 * Displays all of the objects the passed in object references
	 *
	 * @param	Object	Object whose references should be displayed
	 * @param	bGenerateCollection If true, generate a collection 
	 */
	void ShowReferencedObjs( UObject* Object, const FString& CollectionName, ECollectionShareType::Type ShareType )
	{
		if( Object )
		{
			GEditor->GetSelectedObjects()->Deselect( Object );

			// Find references.
			TSet<UObject*> ReferencedObjects;
			{
				const FScopedBusyCursor BusyCursor;
				TArray<UClass*> IgnoreClasses;
				TArray<FString> IgnorePackageNames;
				TArray<UObject*> IgnorePackages;

				// Assemble an ignore list.
				IgnoreClasses.Add( ULevel::StaticClass() );
				IgnoreClasses.Add( UWorld::StaticClass() );
				IgnoreClasses.Add( UPhysicalMaterial::StaticClass() );

				// Load the asset registry module
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

				TArray<FAssetData> AssetData;
				FARFilter Filter;
				Filter.PackagePaths.Add(FName(TEXT("/Engine/EngineMaterials")));
				Filter.PackagePaths.Add(FName(TEXT("/Engine/EditorMeshes")));
				Filter.PackagePaths.Add(FName(TEXT("/Engine/EditorResources")));
				Filter.PackagePaths.Add(FName(TEXT("/Engine/EngineMaterials")));
				Filter.PackagePaths.Add(FName(TEXT("/Engine/EngineFonts")));
				Filter.PackagePaths.Add(FName(TEXT("/Engine/EngineResources")));

				AssetRegistryModule.Get().GetAssets(Filter, AssetData);

				for (int32 AssetIdx = 0; AssetIdx < AssetData.Num(); ++AssetIdx)
				{
					IgnorePackageNames.Add( AssetData[AssetIdx].PackageName.ToString() );
				}
				
				// Construct the ignore package list.
				for( int32 PackageNameItr = 0; PackageNameItr < IgnorePackageNames.Num(); ++PackageNameItr )
				{
					UObject* PackageToIgnore = FindObject<UPackage>(NULL,*(IgnorePackageNames[PackageNameItr]),true);

					if( PackageToIgnore == NULL )
					{// An invalid package name was provided.
						UE_LOG(LogObjectTools, Log,  TEXT("Package to ignore \"%s\" in the list of referenced objects is NULL and should be removed from the list"), *(IgnorePackageNames[PackageNameItr]) );
					}
					else
					{
						IgnorePackages.Add(PackageToIgnore);
					}
				}

				FFindReferencedAssets::BuildAssetList( Object, IgnoreClasses, IgnorePackages, ReferencedObjects );
			}

			const int32 NumReferencedObjects = ReferencedObjects.Num();
			
			// Make sure that the only referenced object (if there's only one) isn't the object itself before outputting object references
			if ( NumReferencedObjects > 1 || ( NumReferencedObjects == 1 && !ReferencedObjects.Contains( Object ) ) )
			{
				if (CollectionName.Len() == 0)
				{
					FString OutString( FString::Printf( TEXT("\nObjects referenced by %s:\r\n"), *Object->GetFullName() ) );
					for(TSet<UObject*>::TConstIterator SetIt(ReferencedObjects); SetIt; ++SetIt)
					{
						const UObject *ReferencedObject = *SetIt;
						check(ReferencedObject);

						// Don't list an object as referring to itself.
						if ( ReferencedObject != Object )
						{
							OutString += FString::Printf( TEXT("\t%s:\r\n"), *ReferencedObject->GetFullName() );
						}
					}

					UE_LOG(LogObjectTools, Warning, TEXT("%s"), *OutString );

					// Display the object references in a copy-friendly dialog; the dialog is designed to destroy itself upon being closed, so this
					// allocation is ok and not a memory leak
					SGenericDialogWidget::OpenDialog(NSLOCTEXT("ObjectTools", "ShowReferencedAssets", "Show Referenced Assets"), SNew(SEditableTextBox).Text(FText::FromString(OutString)).IsReadOnly(true));
				}
				else
				{
					TArray<FName> ObjectsToAdd;
					for(TSet<UObject*>::TConstIterator SetIt(ReferencedObjects); SetIt; ++SetIt)
					{
						UObject* RefObj = *SetIt;
						if (RefObj != NULL)
						{
							if (RefObj != Object)
							{
								ObjectsToAdd.Add(FName(*RefObj->GetPathName()));
							}
						}
					}

					if (ObjectsToAdd.Num() > 0)
					{
						FContentHelper* ContentHelper = new FContentHelper();
						if (ContentHelper->Initialize() == true)
						{
							FName CollectionFName = FName(*CollectionName);
							ContentHelper->ClearCollection(CollectionFName, ShareType);
							const bool CollectionCreated = ContentHelper->SetCollection(CollectionFName, ShareType, ObjectsToAdd);

							// Notify the user whether the collection was successfully created
							FNotificationInfo Info( FText::Format( NSLOCTEXT("ObjectTools", "SuccessfulAddCollection", "{0} sucessfully added as a new collection."), FText::FromName(CollectionFName)) );
							Info.ExpireDuration = 3.0f;
							Info.bUseLargeFont = false;

							if ( !CollectionCreated )
							{
								ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
								if ( !SourceControlModule.IsEnabled() && ShareType != ECollectionShareType::CST_Local )
								{
									// Private and Shared collection types require a source control connection
									Info.Text = NSLOCTEXT("ObjectTools", "FailedToAddCollection_SCC", "Failed to create new collection, requires source control connection");
								}
								else
								{
									Info.Text = NSLOCTEXT("ObjectTools", "FailedToAddCollection_Unknown", "Failed to create new collection");
								}
							}

							TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
							if ( Notification.IsValid() )
							{
								Notification->SetCompletionState( CollectionCreated ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail );
							}
						}
					} //-V773
				}
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, FText::Format( NSLOCTEXT("UnrealEd", "ObjectNoReferences", "Object '{0}' doesn't refer to any non-ignored objects."), FText::FromString(Object->GetName()) ) );
			}

			GEditor->GetSelectedObjects()->Select( Object );
		}
	}

	/**
	 * Select the object referencers in the level
	 *
	 * @param	Object			Object whose references are to be selected
	 *
	 */
	void SelectActorsInLevelDirectlyReferencingObject( UObject* RefObj )
	{
		UPackage* Package = RefObj->GetOutermost();
		if (Package && Package->ContainsMap())
		{
			// Walk the chain of outers to find the object that is 'in' the level...
			UObject* ObjToSelect = NULL;
			UObject* CurrObject = RefObj;
			UObject* Outer = RefObj->GetOuter();
			while ((ObjToSelect == NULL) && (Outer != NULL) && (Outer != Package))
			{
				ULevel* Level = Cast<ULevel>(Outer);
				if (Level)
				{
					// We found it!
					ObjToSelect = CurrObject;
				}
				else
				{
					UObject* TempObject = Outer;
					Outer = Outer->GetOuter();
					CurrObject = TempObject;
				}
			}

			if (ObjToSelect)
			{
				AActor* ActorToSelect = Cast<AActor>(ObjToSelect);
				if (ActorToSelect)
				{
					GEditor->SelectActor( ActorToSelect, true, true );
				}
			}
		}
	}

	/**
	 * Select the object and it's external referencers' referencers in the level.
	 * This function calls AccumulateObjectReferencersForObjectRecursive to
	 * recursively build a list of objects to check for referencers in the level
	 *
	 * @param	Object				Object whose references are to be selected
	 * @param	bRecurseMaterial	Whether or not we're allowed to recurse the material
	 *
	 */
	void SelectObjectAndExternalReferencersInLevel( UObject* Object, const bool bRecurseMaterial )
	{
		if(Object)
		{
			if(IsReferenced(Object, RF_Public, EInternalObjectFlags::Native))
			{
				TArray<UObject*> ObjectsToSelect;

				GEditor->SelectNone( true, true );
				
				// Generate the list of objects.  This function is necessary if the object
				//	in question is indirectly referenced by an actor.  For example, a
				//	material used on a static mesh that is instanced in the level
				AccumulateObjectReferencersForObjectRecursive( Object, ObjectsToSelect, bRecurseMaterial );

				// Select the objects in the world
				for ( TArray<UObject*>::TConstIterator ObjToSelectItr( ObjectsToSelect ); ObjToSelectItr; ++ObjToSelectItr )
				{
					UObject* ObjToSelect = *ObjToSelectItr;
					SelectActorsInLevelDirectlyReferencingObject(ObjToSelect);
				}

				GEditor->GetSelectedObjects()->Select( Object );
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "ObjectNotReferenced", "Object '{0}' Is Not Referenced"), FText::FromString(Object->GetName())) );
			}
		}
	}


	/**
	 * Recursively add the objects referencers to a single array
	 *
	 * @param	Object				Object whose references are to be selected
	 * @param	Referencers			Array of objects being referenced in level
	 * @param	bRecurseMaterial	Whether or not we're allowed to recurse the material
	 *
	 */
	void AccumulateObjectReferencersForObjectRecursive( UObject* Object, TArray<UObject*>& Referencers, const bool bRecurseMaterial )
	{
		TArray<FReferencerInformation> OutInternalReferencers;
		TArray<FReferencerInformation> OutExternalReferencers;
		Object->RetrieveReferencers(&OutInternalReferencers, &OutExternalReferencers);

		// dump the referencers
		for (int32 ExtIndex = 0; ExtIndex < OutExternalReferencers.Num(); ExtIndex++)
		{
			UObject* RefdObject = OutExternalReferencers[ExtIndex].Referencer;
			if (RefdObject)
			{
				Referencers.Push( RefdObject );
				// Recursively search for static meshes and materials so that textures and materials will recurse back
				// to the meshes in which they are used
				if	( !(Object->IsA(UStaticMesh::StaticClass()) ) // Added this check for safety in case of a circular reference
					&& (	(RefdObject->IsA(UStaticMesh::StaticClass())) 
						||	(RefdObject->IsA(UMaterialInterface::StaticClass()) && bRecurseMaterial)	// Only recurse the material if we're interested in it's children
						)
					)
				{
					AccumulateObjectReferencersForObjectRecursive( RefdObject, Referencers, bRecurseMaterial );
				}
			}
		}
	}

	bool ShowDeleteConfirmationDialog ( const TArray<UObject*>& ObjectsToDelete )
	{
		TArray<UPackage*> PackagesToDelete;

		// Gather a list of packages which may need to be deleted once the objects are deleted.
		for ( int32 ObjIdx = 0; ObjIdx < ObjectsToDelete.Num(); ++ObjIdx )
		{
			PackagesToDelete.AddUnique(ObjectsToDelete[ObjIdx]->GetOutermost());
		}

		// Cull out packages which cannot be found on disk or are not UAssets
		for ( int32 PackageIdx = PackagesToDelete.Num() - 1; PackageIdx >= 0; --PackageIdx )
		{
			UPackage* Package = PackagesToDelete[PackageIdx];

			FString PackageFilename;
			if( !FPackageName::DoesPackageExist( Package->GetName(), NULL, &PackageFilename ) )
			{
				// Could not determine filename for package so we can not delete
				PackagesToDelete.RemoveAt(PackageIdx);
			}
		}

		// If we found any packages that we may delete
		if ( PackagesToDelete.Num() )
		{
			// Set up the delete package dialog
			FPackagesDialogModule& PackagesDialogModule = FModuleManager::LoadModuleChecked<FPackagesDialogModule>( TEXT("PackagesDialog") );
			PackagesDialogModule.CreatePackagesDialog(NSLOCTEXT("PackagesDialogModule", "DeleteAssetsDialogTitle", "Delete Assets"), NSLOCTEXT("PackagesDialogModule", "DeleteAssetsDialogMessage", "The following assets will be deleted."), /*InReadOnly=*/true);
			PackagesDialogModule.AddButton(DRT_Save, NSLOCTEXT("PackagesDialogModule", "DeleteSelectedButton", "Delete"), NSLOCTEXT("PackagesDialogModule", "DeleteSelectedButtonTip", "Delete the listed assets"));
			if(!ISourceControlModule::Get().IsEnabled())
			{
				PackagesDialogModule.AddButton(DRT_MakeWritable, NSLOCTEXT("PackagesDialogModule", "MakeWritableAndDeleteSelectedButton", "Make Writable and Delete"), NSLOCTEXT("PackagesDialogModule", "MakeWritableAndDeleteSelectedButtonTip", "Makes the listed assets writable and deletes them"));
			}
			PackagesDialogModule.AddButton(DRT_Cancel, NSLOCTEXT("PackagesDialogModule", "CancelButton", "Cancel"), NSLOCTEXT("PackagesDialogModule", "CancelDeleteButtonTip", "Do not delete any assets and cancel the current operation"));

			for ( int32 PackageIdx = 0; PackageIdx < PackagesToDelete.Num(); ++PackageIdx )
			{
				UPackage* Package = PackagesToDelete[PackageIdx];
				PackagesDialogModule.AddPackageItem(Package, Package->GetName(), ECheckBoxState::Checked);
			}

			// Display the delete dialog
			const EDialogReturnType UserResponse = PackagesDialogModule.ShowPackagesDialog();

			if(UserResponse == DRT_MakeWritable)
			{
				// make each file writable before attempting to delete
				for ( int32 PackageIdx = 0; PackageIdx < PackagesToDelete.Num(); ++PackageIdx )
				{
					const UPackage* Package = PackagesToDelete[PackageIdx];
					FString PackageFilename;
					if(FPackageName::DoesPackageExist(Package->GetName(), NULL, &PackageFilename))
					{
						FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*PackageFilename, false);
					}
				}
			}

			// If the user selected a "Delete" option return true
			return UserResponse == DRT_Save || UserResponse == DRT_MakeWritable;
		}
		else
		{
			// There are no packages that are considered for deletion. Return true because this is a safe delete.
			return true;
		}
	}

	void CleanupAfterSuccessfulDelete (const TArray<UPackage*>& PotentialPackagesToDelete, bool bPerformReferenceCheck)
	{
		TArray<UPackage*> PackagesToDelete = PotentialPackagesToDelete;
		TArray<UPackage*> EmptyPackagesToUnload;
		TArray<FString> PackageFilesToDelete;
		TArray<TSharedRef<ISourceControlState, ESPMode::ThreadSafe> > PackageSCCStates;
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

		GWarn->BeginSlowTask( NSLOCTEXT("ObjectTools", "OldPackageCleanupSlowTask", "Cleaning Up Old Assets"), true );
		const int32 OriginalNumPackagesToDelete = PackagesToDelete.Num();
		// Cull out packages which are still referenced, dont exist on disk, or are not UAssets
		// Record the filename and SCC state of any package which is not culled.
		for ( int32 PackageIdx = PackagesToDelete.Num() - 1; PackageIdx >= 0; --PackageIdx )
		{
			GWarn->StatusUpdate(OriginalNumPackagesToDelete - PackageIdx, OriginalNumPackagesToDelete, NSLOCTEXT("ObjectTools", "OldPackageCleanupSlowTask", "Cleaning Up Old Assets"));
			UObject* Package = PackagesToDelete[PackageIdx];

			bool bIsReferenced = false;
			
			if ( Package != nullptr && bPerformReferenceCheck )
			{
				FReferencerInformationList FoundReferences;
				bIsReferenced = IsReferenced(Package, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags,  true, &FoundReferences);
				if ( bIsReferenced )
				{
					// determine whether the transaction buffer is the only thing holding a reference to the object
					// and if so, offer the user the option to reset the transaction buffer.
					GEditor->Trans->DisableObjectSerialization();
					bIsReferenced = IsReferenced(Package, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, true, &FoundReferences);
					GEditor->Trans->EnableObjectSerialization();

					// only ref to this object is the transaction buffer, clear the transaction buffer
					if ( !bIsReferenced )
					{
						GEditor->Trans->Reset(NSLOCTEXT("UnrealEd", "DeleteSelectedItem", "Delete Selected Item"));
					}
				}
			}

			if ( bIsReferenced )
			{
				PackagesToDelete.RemoveAt(PackageIdx);
			}
			else
			{
				UPackage* CurrentPackage = Cast<UPackage>(Package);

				FString PackageFilename;
				if( Package != nullptr && FPackageName::DoesPackageExist( Package->GetName(), NULL, &PackageFilename ) )
				{
					PackageFilesToDelete.Add(PackageFilename);
					CurrentPackage->SetDirtyFlag(false);
				}
				else
				{
					// Could not determine filename for package so we can not delete
					PackagesToDelete.RemoveAt(PackageIdx);

					if (UPackage::IsEmptyPackage(CurrentPackage))
					{
						// If the package is empty, unload it anyway
						EmptyPackagesToUnload.Add(CurrentPackage);
						CurrentPackage->SetDirtyFlag(false);
					}
				}
			}
		}

		// Get the current source control states of all the package files we're deleting at once.
		if (ISourceControlModule::Get().IsEnabled())
		{
			SourceControlProvider.GetState(PackageFilesToDelete, PackageSCCStates, EStateCacheUsage::ForceUpdate);
		}

		GWarn->EndSlowTask();

		if (GUnrealEd)
		{
			// Let the package auto-saver know that it needs to ignore the deleted packages
			GUnrealEd->GetPackageAutoSaver().OnPackagesDeleted(PackagesToDelete);
		}

		// Let the asset registry know that these packages are being removed
		for (UPackage* PackageToDelete : PackagesToDelete)
		{
			FAssetRegistryModule::PackageDeleted(PackageToDelete);
		}

		// Unload the packages and collect garbage.
		if ( PackagesToDelete.Num() > 0 || EmptyPackagesToUnload.Num() > 0 )
		{
			TArray<UPackage*> AllPackagesToUnload;
			AllPackagesToUnload.Reserve(PackagesToDelete.Num() + EmptyPackagesToUnload.Num());
			AllPackagesToUnload.Append(PackagesToDelete);
			AllPackagesToUnload.Append(EmptyPackagesToUnload);

			PackageTools::UnloadPackages(AllPackagesToUnload);
		}
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		// Now delete all packages that have become empty
		bool bMakeWritable = false;
		bool bSilent = false;
		TArray<FString> SCCFilesToRevert;
		TArray<FString> SCCFilesToDelete;

		for ( int32 PackageFileIdx = 0; PackageFileIdx < PackageFilesToDelete.Num(); ++PackageFileIdx )
		{
			const FString& PackageFilename = PackageFilesToDelete[PackageFileIdx];
			if ( ISourceControlModule::Get().IsEnabled() )
			{
				const FSourceControlStateRef SourceControlState = PackageSCCStates[PackageFileIdx];
				const bool bInDepot = SourceControlState->IsSourceControlled();
				if ( bInDepot )
				{
					// The file is managed by source control. Open it for delete.
					FString FullPackageFilename = FPaths::ConvertRelativePathToFull(PackageFilename);

					// Revert the file if it is checked out
					const bool bIsAdded = SourceControlState->IsAdded();
					if ( SourceControlState->IsCheckedOut() || bIsAdded || SourceControlState->IsDeleted() )
					{
						// Batch the revert operation so that we only make one request to the source control module.
						SCCFilesToRevert.Add(FullPackageFilename);
					}

					if ( bIsAdded )
					{
						// The file was open for add and reverted, this leaves the file on disk so here we delete it
						IFileManager::Get().Delete(*PackageFilename);
					}
					else
					{
						// Batch this file for deletion so that we only send one deletion request to the source control module.
						if (SourceControlState->CanDelete())
						{
							SCCFilesToDelete.Add(FullPackageFilename);
						}
						else
						{
							UE_LOG(LogObjectTools, Warning, TEXT("SCC failed to open '%s' for deletion."), *PackageFilename);
						}
					}
				}
				else
				{
					// The file was never submitted to the depo, delete it locally
					IFileManager::Get().Delete(*PackageFilename);
				}
			}
			else
			{
				// Source control is compiled in, but is not enabled for some reason, delete the file locally
				if(IFileManager::Get().IsReadOnly(*PackageFilename))
				{
					EAppReturnType::Type ReturnType = EAppReturnType::No;
					if(!bMakeWritable && !bSilent)
					{
						FFormatNamedArguments Args;
						Args.Add(TEXT("Filename"), FText::FromString(PackageFilename));
						const FText Message = FText::Format(NSLOCTEXT("ObjectTools", "DeleteReadOnlyWarning", "File '{Filename}' is read-only on disk, are you sure you want to delete it?"), Args);

						ReturnType = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAll, Message);
						bMakeWritable = ReturnType == EAppReturnType::YesAll;
						bSilent = ReturnType == EAppReturnType::NoAll;
					}

					if(bMakeWritable || ReturnType == EAppReturnType::Yes)
					{
						FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*PackageFilename, false);
						IFileManager::Get().Delete(*PackageFilename);
					}
				}
				else
				{
					IFileManager::Get().Delete(*PackageFilename);
				}
			}
		}

		// Handle all source control revert and delete operations as a batched operation.
		if (ISourceControlModule::Get().IsEnabled())
		{
			if (SCCFilesToRevert.Num() > 0)
			{
				SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), SCCFilesToRevert);
			}

			if (SCCFilesToDelete.Num() > 0)
			{
				if (SourceControlProvider.Execute(ISourceControlOperation::Create<FDelete>(), SCCFilesToDelete) == ECommandResult::Failed)
				{
					UE_LOG(LogObjectTools, Warning, TEXT("SCC failed to open the selected files for deletion."));
				}
			}
		}
	}

	int32 DeleteAssets( const TArray<FAssetData>& AssetsToDelete, bool bShowConfirmation )
	{
		TArray<TWeakObjectPtr<UPackage>> PackageFilesToDelete;
		TArray<UObject*> ObjectsToDelete;
		for ( int i = 0; i < AssetsToDelete.Num(); i++ )
		{
			const FAssetData& AssetData = AssetsToDelete[i];
			UObject *ObjectToDelete = AssetData.GetAsset();
			// Assets can be loaded even when their underlying type/class no longer exists...
			if ( ObjectToDelete!=nullptr )
			{
				ObjectsToDelete.Add( ObjectToDelete );
			}
			else if ( AssetData.IsUAsset() )
			{
				// ... In this cases there is no underlying asset or type so remove the package itself directly after confirming it's valid to do so.
				FString PackageFilename;
				if( !FPackageName::DoesPackageExist( AssetData.PackageName.ToString(), NULL, &PackageFilename ) )
				{
					// Could not determine filename for package so we can not delete
					continue;
				}

				UPackage* Package = FindPackage(nullptr, *AssetData.PackageName.ToString());
				if ( Package )
				{
					PackageFilesToDelete.Add(Package);
				}
			}
		}

		int32 NumObjectsToDelete = ObjectsToDelete.Num();
		if ( NumObjectsToDelete > 0 )
		{
			NumObjectsToDelete = DeleteObjects( ObjectsToDelete, bShowConfirmation );
		}

		const int32 NumPackagesToDelete = PackageFilesToDelete.Num();
		if (NumPackagesToDelete > 0)
		{
			TArray<UPackage*> PackagePointers;
			for ( const auto& PkgIt : PackageFilesToDelete )
			{
				UPackage* Package = PkgIt.Get();
				if ( Package )
				{
					PackagePointers.Add(Package);
				}
			}
			
			if ( PackagePointers.Num() > 0 )
			{
				const bool bPerformReferenceCheck = true;
				CleanupAfterSuccessfulDelete(PackagePointers, bPerformReferenceCheck);
			}
		}

		return NumPackagesToDelete + NumObjectsToDelete;
	}

	int32 DeleteObjects( const TArray< UObject* >& ObjectsToDelete, bool bShowConfirmation )
	{
		// Allows deleting of sounds after they have been previewed
		GEditor->ClearPreviewComponents();

		// Ensure the audio manager is not holding on to any sounds
		FAudioDeviceManager* AudioDeviceManager = GEditor->GetAudioDeviceManager();
		if (AudioDeviceManager != nullptr)
		{
			AudioDeviceManager->UpdateActiveAudioDevices(false);

			const int32 NumAudioDevices = AudioDeviceManager->GetNumActiveAudioDevices();
			for (int32 DeviceIndex = 0; DeviceIndex < NumAudioDevices; DeviceIndex++)
			{
				FAudioDevice* AudioDevice = AudioDeviceManager->GetAudioDevice(DeviceIndex);
				if (AudioDevice != nullptr)
				{
					AudioDevice->StopAllSounds();
				}
			}
		}

		const FScopedBusyCursor BusyCursor;

		// Make sure packages being saved are fully loaded.
		if( !HandleFullyLoadingPackages( ObjectsToDelete, NSLOCTEXT("UnrealEd", "Delete", "Delete") ) )
		{
			return 0;
		}

		// Load the asset registry module
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// Don't delete anything if we're still building the asset registry, warn the user and don't delete.
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			FNotificationInfo Info( NSLOCTEXT("UnrealEd", "Warning_CantDeleteRebuildingAssetRegistry", "Unable To Delete While Discovering Assets") );
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
			return 0;
		}

		// let systems clean up any unnecessary references that they may have 
		// (so that they're not flagged in the dialog)
		FEditorDelegates::OnAssetsPreDelete.Broadcast(ObjectsToDelete);

		TSharedRef<FAssetDeleteModel> DeleteModel = MakeShareable(new FAssetDeleteModel(ObjectsToDelete));

		if ( bShowConfirmation )
		{
			const FVector2D DEFAULT_WINDOW_SIZE = FVector2D( 600, 700 );

			/** Create the window to host our package dialog widget */
			TSharedRef< SWindow > DeleteAssetsWindow = SNew( SWindow )
				.Title( FText::FromString( "Delete Assets" ) )
				.ClientSize( DEFAULT_WINDOW_SIZE );

			/** Set the content of the window to our package dialog widget */
			TSharedRef< SDeleteAssetsDialog > DeleteDialog =
				SNew(SDeleteAssetsDialog, DeleteModel)
				.ParentWindow( DeleteAssetsWindow );

			DeleteAssetsWindow->SetContent( DeleteDialog );

			/** Show the package dialog window as a modal window */
			GEditor->EditorAddModalWindow( DeleteAssetsWindow );

			return DeleteModel->GetDeletedObjectCount();
		}
		
		bool bUserCanceled = false;

		GWarn->BeginSlowTask(NSLOCTEXT("UnrealEd", "VerifyingDelete", "Verifying Delete"), true, true);
		while ( !bUserCanceled && DeleteModel->GetState() != FAssetDeleteModel::Finished )
		{
			DeleteModel->Tick(0);
			GWarn->StatusUpdate((int32)( DeleteModel->GetProgress() * 100 ), 100, DeleteModel->GetProgressText());

			bUserCanceled = GWarn->ReceivedUserCancel();
		}
		GWarn->EndSlowTask();

		if ( bUserCanceled )
		{
			return 0;
		}

		if ( !DeleteModel->DoDelete() )
		{
			//@todo ndarnell explain why the delete failed?  Maybe we should show the delete UI
			// when this fails?
		}

		return DeleteModel->GetDeletedObjectCount();
	}

	static bool MakeReadOnlyPackageWritable(UObject* ObjectToDelete, bool& bMakeWritable, bool& bSilent)
	{
		// If an object's package is read only, and source control is not enabled, ask the user whether they wish
		// to make it writable.
		if (!ISourceControlModule::Get().IsEnabled())
		{
			UPackage* ObjectPackage = ObjectToDelete->GetOutermost();
			check(ObjectPackage != nullptr);

			FString PackageFilename;
			if (FPackageName::DoesPackageExist(ObjectPackage->GetName(), nullptr, &PackageFilename))
			{
				if (IFileManager::Get().IsReadOnly(*PackageFilename))
				{
					EAppReturnType::Type ReturnType = EAppReturnType::No;
					if (!bMakeWritable && !bSilent)
					{
						FFormatNamedArguments Args;
						Args.Add(TEXT("Filename"), FText::FromString(PackageFilename));
						const FText Message = FText::Format(NSLOCTEXT("ObjectTools", "DeleteReadOnlyWarning", "File '{Filename}' is read-only on disk, are you sure you want to delete it?"), Args);

						ReturnType = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAll, Message);
						bMakeWritable = ReturnType == EAppReturnType::YesAll;
						bSilent = ReturnType == EAppReturnType::NoAll;
					}

					if (bMakeWritable || ReturnType == EAppReturnType::Yes)
					{
						FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*PackageFilename, false);
					}
					else
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	int32 DeleteObjectsUnchecked(const TArray< UObject* >& ObjectsToDelete)
	{
		GWarn->BeginSlowTask( NSLOCTEXT( "UnrealEd", "Deleting", "Deleting" ), true );

		TArray<UObject*> ObjectsDeletedSuccessfully;

		bool bSawSuccessfulDelete = false;
		bool bMakeWritable = false;
		bool bSilent = false;

		for ( int32 Index = 0; Index < ObjectsToDelete.Num(); Index++ )
		{
			GWarn->StatusUpdate( Index, ObjectsToDelete.Num(), FText::Format( NSLOCTEXT( "UnrealEd", "Deletingf", "Deleting ({0} of {1})" ), FText::AsNumber( Index ), FText::AsNumber( ObjectsToDelete.Num() ) ) );
			UObject* ObjectToDelete = ObjectsToDelete[Index];

			if ( !ensure( ObjectToDelete != NULL ) )
			{
				continue;
			}

			// Early exclusion for assets contained in read-only packages if the user chooses not to write enable them
			if (!MakeReadOnlyPackageWritable(ObjectToDelete, bMakeWritable, bSilent))
			{
				continue;
			}

			// We already know it's not referenced or we wouldn't be performing the safe delete, so don't repeat the reference check.
			bool bPerformReferenceCheck = false;
			if ( DeleteSingleObject( ObjectToDelete, bPerformReferenceCheck ) )
			{
				ObjectsDeletedSuccessfully.Push( ObjectToDelete );
				bSawSuccessfulDelete = true;
			}
		}

		GWarn->EndSlowTask();

		// Record the number of objects deleted successfully so we can clear the list (once it is just full of pointers to deleted objects)
		const int32 NumObjectsDeletedSuccessfully = ObjectsDeletedSuccessfully.Num();

		// Update the browser if something was actually deleted.
		if ( bSawSuccessfulDelete )
		{
			TArray<UClass*> DeletedObjectClasses;
			TArray<UPackage*> PotentialPackagesToDelete;
			for ( int32 ObjIdx = 0; ObjIdx < ObjectsDeletedSuccessfully.Num(); ++ObjIdx )
			{
				DeletedObjectClasses.AddUnique(ObjectsDeletedSuccessfully[ObjIdx]->GetClass());
				PotentialPackagesToDelete.AddUnique( ObjectsDeletedSuccessfully[ObjIdx]->GetOutermost() );
			}
			// Broadcast the classes of the successfully deleted objects (before cleanup)
			FEditorDelegates::OnAssetsDeleted.Broadcast(DeletedObjectClasses);

			bool bPerformReferenceCheck = false;
			CleanupAfterSuccessfulDelete( PotentialPackagesToDelete, bPerformReferenceCheck );
			ObjectsDeletedSuccessfully.Empty();
		}

		return NumObjectsDeletedSuccessfully;
	}

	bool DeleteSingleObject( UObject* ObjectToDelete, bool bPerformReferenceCheck )
	{
		GEditor->GetSelectedObjects()->Deselect( ObjectToDelete );

		{
			// @todo Animation temporary HACK to allow deleting of UMorphTargets. This will be removed when UMorphTargets are subobjects of USkeleton.
			// Get the base skeleton and unregister this morphtarget
			UMorphTarget* MorphTarget = Cast<UMorphTarget>(ObjectToDelete);
			if (MorphTarget && MorphTarget->BaseSkelMesh)
			{
				MorphTarget->BaseSkelMesh->UnregisterMorphTarget(MorphTarget);
			}
		}

		if ( bPerformReferenceCheck )
		{
			FReferencerInformationList Refs;

			// Check and see whether we are referenced by any objects that won't be garbage collected. 
			bool bIsReferenced = IsReferenced(ObjectToDelete, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, true, &Refs);
			if ( bIsReferenced )
			{
				// determine whether the transaction buffer is the only thing holding a reference to the object
				// and if so, offer the user the option to reset the transaction buffer.
				GEditor->Trans->DisableObjectSerialization();
				bIsReferenced = IsReferenced(ObjectToDelete, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, true, &Refs);
				GEditor->Trans->EnableObjectSerialization();

				// only ref to this object is the transaction buffer, clear the transaction buffer
				if ( !bIsReferenced )
				{
					GEditor->Trans->Reset( NSLOCTEXT( "UnrealEd", "DeleteSelectedItem", "Delete Selected Item" ) );
				}
			}

			if ( bIsReferenced )
			{
				// We cannot safely delete this object. Print out a list of objects referencing this one
				// that prevent us from being able to delete it.
				FStringOutputDevice Ar;
				ObjectToDelete->OutputReferencers( Ar, &Refs );
				FMessageDialog::Open( EAppMsgType::Ok,
					FText::Format( NSLOCTEXT( "UnrealEd", "Error_InUse", "{0} is in use.\n\n---\nRunning the editor with '-NoLoadStartupPackages' may help if the object is loaded at startup.\n---\n\n{1}" ),
					FText::FromString( ObjectToDelete->GetFullName() ), FText::FromString( *Ar ) ) );

				// Reselect the object as it failed to be deleted
				GEditor->GetSelectedObjects()->Select( ObjectToDelete );

				return false;
			}
		}

		// Mark its package as dirty as we're going to delete it.
		ObjectToDelete->MarkPackageDirty();

		// Remove standalone flag so garbage collection can delete the object.
		ObjectToDelete->ClearFlags( RF_Standalone );

		// Notify the asset registry
		FAssetRegistryModule::AssetDeleted( ObjectToDelete );

		return true;
	}

	int32 ForceDeleteObjects( const TArray< UObject* >& InObjectsToDelete, bool ShowConfirmation )
	{
		int32 NumDeletedObjects = 0;

		// Confirm that the delete was intentional
		if ( ShowConfirmation && !ShowDeleteConfirmationDialog(InObjectsToDelete) )
		{
			return 0;
		}

		// Close all editors to avoid changing references to temporary objects used by the editor
		if ( !FAssetEditorManager::Get().CloseAllAssetEditors() )
		{
			// Failed to close at least one editor. It is possible that this editor has in-memory object references
			// which are not prepared to be changed dynamically so it is not safe to continue
			return 0;
		}

		GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "Deleting", "Deleting"), true );

		struct FSCSNodeToDelete
		{
			USimpleConstructionScript* SimpleConstructionScript;
			USCS_Node* SCS_Node;
		};

		TArray<FSCSNodeToDelete> SCSNodesToDelete;
		TArray<UActorComponent*> ComponentsToDelete;
		TArray<AActor*> ActorsToDelete;
		TArray<TWeakObjectPtr<UObject>> ObjectsToDelete;
		bool bNeedsGarbageCollection = false;
		bool bMakeWritable = false;
		bool bSilent = false;

		// Clear audio components to allow previewed sounds to be consolidated
		GEditor->ClearPreviewComponents();

		for ( TArray<UObject*>::TConstIterator ObjectItr(InObjectsToDelete); ObjectItr; ++ObjectItr )
		{
			UObject* CurrentObject = *ObjectItr;

			GEditor->GetSelectedObjects()->Deselect( CurrentObject );

			// Early exclusion for assets contained in read-only packages if the user chooses not to write enable them
			if (!MakeReadOnlyPackageWritable(CurrentObject, bMakeWritable, bSilent))
			{
				continue;
			}

			ObjectsToDelete.Add( CurrentObject );

			// If the object about to be deleted is a Blueprint asset, make sure that any instances of the Blueprint class get deleted as well
			UBlueprint* BlueprintObject = Cast<UBlueprint>(CurrentObject);
			if ( BlueprintObject && BlueprintObject->GeneratedClass && BlueprintObject->GeneratedClass->ClassDefaultObject )
			{
				TArray<UObject*> InstancesToDelete;
				BlueprintObject->GeneratedClass->ClassDefaultObject->GetArchetypeInstances( InstancesToDelete );

				for ( TArray<UObject*>::TConstIterator InstanceItr( InstancesToDelete ); InstanceItr; ++InstanceItr )
				{
					UObject* CurrentInstance = *InstanceItr;

					// Don't include derived class CDOs.
					if(CurrentInstance->HasAnyFlags(RF_ClassDefaultObject))
					{
						continue;
					}

					AActor* CurrentInstanceAsActor = Cast<AActor>( CurrentInstance );
					UActorComponent* CurrentInstanceAsComponent = Cast<UActorComponent>(CurrentInstance);
					if ( CurrentInstanceAsActor )
					{
						ActorsToDelete.Add( CurrentInstanceAsActor );
					}
					else if ( CurrentInstanceAsComponent )
					{
						ComponentsToDelete.Add( CurrentInstanceAsComponent );

						// Find all the SCS_Node references that need to be destroyed before this component is destroyed.
						UBlueprintGeneratedClass* UBGC = CurrentInstanceAsComponent->GetTypedOuter<UBlueprintGeneratedClass>();
						if (UBGC && UBGC->SimpleConstructionScript)
						{
							for (USCS_Node* SCS_Node : UBGC->SimpleConstructionScript->GetAllNodes())
							{
								if (SCS_Node && SCS_Node->ComponentTemplate == CurrentInstanceAsComponent)
								{
									FSCSNodeToDelete DeleteNode;
									DeleteNode.SimpleConstructionScript = UBGC->SimpleConstructionScript;
									DeleteNode.SCS_Node = SCS_Node;
									SCSNodesToDelete.Add(DeleteNode);
								}
							}
						}
					}
					else
					{
						ObjectsToDelete.Add( CurrentInstance );
					}
				}
			}
		}

		// Destroy all SCSNodes
		if (SCSNodesToDelete.Num() > 0)
		{
			for (TArray<FSCSNodeToDelete>::TConstIterator SCSNodeItr(SCSNodesToDelete); SCSNodeItr; ++SCSNodeItr)
			{
				FSCSNodeToDelete SCSNodeToDelete = *SCSNodeItr;
				
				SCSNodeToDelete.SimpleConstructionScript->RemoveNodeAndPromoteChildren(SCSNodeToDelete.SCS_Node);

				GWarn->StatusUpdate(SCSNodeItr.GetIndex(), SCSNodesToDelete.Num(), NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_DeletingSCSNodes", "Deleting Blueprint Component references..."));
			}
		}

		bool bSelectionChanged = false;

		// Destroy all Components
		if (ComponentsToDelete.Num() > 0)
		{
			for (TArray<UActorComponent*>::TConstIterator ComponentItr(ComponentsToDelete); ComponentItr; ++ComponentItr)
			{
				UActorComponent* CurComponent = *ComponentItr;

				// Skip if already pending GC
				if (!CurComponent->IsPendingKill())
				{
					// Deselect if active
					USelection* SelectedComponents = GEditor->GetSelectedComponents();
					if (SelectedComponents && CurComponent->IsSelected())
					{
						SelectedComponents->Deselect(CurComponent);

						bSelectionChanged = true;
					}

					// Destroy the Component Instance
					CurComponent->DestroyComponent(true);

					bNeedsGarbageCollection = true;
				}

				GWarn->StatusUpdate(ComponentItr.GetIndex(), ComponentsToDelete.Num(), NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_DeletingComponentInstances", "Deleting Component Instances..."));
			}
		}

		// Destroy all Actor instances
		if ( ActorsToDelete.Num() > 0 )
		{
			for ( TArray<AActor*>::TConstIterator ActorItr( ActorsToDelete ); ActorItr; ++ActorItr )
			{
				AActor* CurActor = *ActorItr;

				// Skip if already pending GC
				if ( !CurActor->IsPendingKill() )
				{
					// Deselect if active
					USelection* SelectedActors = GEditor->GetSelectedActors();
					if ( SelectedActors && CurActor->IsSelected() )
					{
						SelectedActors->Deselect( CurActor );

						bSelectionChanged = true;
					}

					// Destroy the Actor instance. This is similar to edactDeleteSelected(), but we don't request user confirmation here.
					GEditor->Layers->DisassociateActorFromLayers( CurActor );
					if( CurActor->GetWorld() )
					{
						CurActor->GetWorld()->EditorDestroyActor( CurActor, false );
					}

					bNeedsGarbageCollection = true;
				}

				GWarn->StatusUpdate( ActorItr.GetIndex(), ActorsToDelete.Num(), NSLOCTEXT( "UnrealEd", "ConsolidateAssetsUpdate_DeletingActorInstances", "Deleting Actor Instances..." ) );
			}
		}

		if (bSelectionChanged)
		{
			GEditor->NoteSelectionChange();
		}

		{
			// If the current editor world is in this list, transition to a new map and reload the world to finish the delete
			ReloadEditorWorldForReferenceReplacementIfNecessary(ObjectsToDelete);
		}

		{
			int32 ReplaceableObjectsNum = 0;
			{
				TArray<UObject*> ObjectsToReplace;
				ObjectsToReplace.Reserve(ObjectsToDelete.Num());

				for(TWeakObjectPtr<UObject>& Object : ObjectsToDelete)
				{
					if(Object.IsValid())
					{
						ObjectsToReplace.Add(Object.Get());

						UBlueprint* BlueprintObject = Cast<UBlueprint>(Object.Get());
						if (BlueprintObject)
						{
							// If we're a blueprint add our generated class as well
							if (BlueprintObject->GeneratedClass)
							{
								ObjectsToReplace.AddUnique(BlueprintObject->GeneratedClass);
							}

							// Reparent any direct children to the parent class of the blueprint that's about to be deleted
							if (BlueprintObject->ParentClass != nullptr)
							{
								for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
								{
									UClass* ChildClass = *ClassIt;
									if (ChildClass->GetSuperStruct() == BlueprintObject->GeneratedClass)
									{
										UBlueprint* ChildBlueprint = Cast<UBlueprint>(ChildClass->ClassGeneratedBy);
										if (ChildBlueprint != nullptr)
										{
											// Do not reparent and recompile a Blueprint that is going to be deleted.
											if (ObjectsToDelete.Find(ChildBlueprint) == INDEX_NONE)
											{
												ChildBlueprint->Modify();
												ChildBlueprint->ParentClass = BlueprintObject->ParentClass;

												// Recompile the child blueprint to fix up the generated class
												FKismetEditorUtilities::CompileBlueprint(ChildBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);

												// Defer garbage collection until after we're done processing the list of objects
												bNeedsGarbageCollection = true;
											}
										}
									}
								}
							}

							BlueprintObject->RemoveGeneratedClasses();
						}
					}
				}

				// Replacing references inside already loaded objects could cause rendering issues, so globally detach all components from their scenes for now
				FGlobalComponentRecreateRenderStateContext ReregisterContext;

				// UserDefinedStructs (probably all SctiptStructs) should be replaced with the FallbackStruct
				{
					TArray<UObject*> UDStructToReplace;
					for (int32 Iter = 0; Iter < ObjectsToReplace.Num(); )
					{
						if (auto UDStruct = Cast<UUserDefinedStruct>(ObjectsToReplace[Iter]))
						{
							ObjectsToReplace.RemoveAtSwap(Iter);
							UDStructToReplace.Add(UDStruct);
						}
						else
						{
							Iter++;
						}
					}

					if (UDStructToReplace.Num())
					{
						FForceReplaceInfo ReplaceInfo;
						ForceReplaceReferences(GetFallbackStruct(), UDStructToReplace, ReplaceInfo, false);
						ReplaceableObjectsNum += ReplaceInfo.ReplaceableObjects.Num();
					}
				}

				{
					FForceReplaceInfo ReplaceInfo;
					ForceReplaceReferences(NULL, ObjectsToReplace, ReplaceInfo, false);
					ReplaceableObjectsNum += ReplaceInfo.ReplaceableObjects.Num();
				}
			}

			// Handle deferred garbage collection
			if (bNeedsGarbageCollection)
			{
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
				bNeedsGarbageCollection = false;
			}

			// Load the asset tools module to get access to the browser type maps
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

			int32 Count = 0;
			for(TWeakObjectPtr<UObject>& Object : ObjectsToDelete)
			{
				UObject* CurObject = Object.Get();

				if ( !ensure(CurObject != NULL) )
				{
					continue;
				}

				if( DeleteSingleObject( CurObject ) )
				{
					// Only count the objects we were given to delete, as this function may have added more (eg, BP instances)
					if (InObjectsToDelete.Contains(CurObject))
					{
						// Update return val
						++NumDeletedObjects;
					}
				}

				GWarn->StatusUpdate(Count, ReplaceableObjectsNum, NSLOCTEXT("UnrealEd", "ConsolidateAssetsUpdate_DeletingObjects", "Deleting Assets..."));
				++Count;

			}
		}

		TArray<UPackage*> PotentialPackagesToDelete;
		for(TWeakObjectPtr<UObject>& Object : ObjectsToDelete)
		{
			if(Object.IsValid())
			{
				PotentialPackagesToDelete.AddUnique(Object->GetOutermost());
			}
		}

		if (PotentialPackagesToDelete.Num() > 0)
		{
			CleanupAfterSuccessfulDelete(PotentialPackagesToDelete);
		}
		ObjectsToDelete.Empty();

		GWarn->EndSlowTask();

		// Redraw viewports
		GUnrealEd->RedrawAllViewports();

		return NumDeletedObjects;
	}	

	
	/**
	 * Utility function to compose a string list of referencing objects
	 *
	 * @param References			Array of references to the relevant object
	 * @param RefObjNames			String list of all objects
	 * @param DefObjNames			String list of all objects referenced in default properties
	 *
	 * @return Whether or not any objects are in default properties
	 */
	bool ComposeStringOfReferencingObjects( TArray<FReferencerInformation>& References, FString& RefObjNames, FString& DefObjNames )
	{
		bool bInDefaultProperties = false;

		for ( TArray<FReferencerInformation>::TConstIterator ReferenceInfoItr( References ); ReferenceInfoItr; ++ReferenceInfoItr )
		{
			FReferencerInformation RefInfo = *ReferenceInfoItr;
			UObject* ReferencingObject = RefInfo.Referencer;
			RefObjNames = RefObjNames + TEXT("\n") + ReferencingObject->GetPathName();

			if( ReferencingObject->GetPathName().Contains( FString(DEFAULT_OBJECT_PREFIX)) )
			{
				DefObjNames = DefObjNames + TEXT("\n") + ReferencingObject->GetName();
				bInDefaultProperties = true;
			}
		}

		return bInDefaultProperties;
	}

	void DeleteRedirector (UObjectRedirector* Redirector)
	{
		// We can't actually delete the redirector. We will just send it to the transient package where it will get cleaned up later
		if (Redirector)
		{
			FAssetRegistryModule::AssetDeleted(Redirector);

			// Remove public flag if set and set transient flag to ensure below rename doesn't create a redirect.
			Redirector->ClearFlags( RF_Public );
			Redirector->SetFlags( RF_Transient );

			// Instead of deleting we rename the redirector into a dummy package where it will be GCed later.
			Redirector->Rename(NULL, GetTransientPackage(), REN_DontCreateRedirectors);
			Redirector->DestinationObject = NULL;
		}
	}

	bool GetMoveDialogInfo(const FText& DialogTitle, UObject* Object, bool bUniqueDefaultName, const FString& SourcePath, const FString& DestinationPath, FMoveDialogInfo& InOutInfo)
	{
		if ( !ensure(Object) )
		{
			return false;
		}

		const FString CurrentPackageName = Object->GetOutermost()->GetName();

		FString PreviousPackage = InOutInfo.PGN.PackageName;
		FString PreviousGroup = InOutInfo.PGN.GroupName;

		FString PackageName;
		FString GroupName;
		FString ObjectName;

		ObjectName = Object->GetName();
		
		const bool bIsRelativeOperation = SourcePath.Len() && DestinationPath.Len() && CurrentPackageName.StartsWith(SourcePath);
		if ( bIsRelativeOperation )
		{
			// Folder copy/move.
			
			// Collect the relative path then use it to determine the new location
			// For example, if SourcePath = /Game/MyPath and CurrentPackageName = /Game/MyPath/MySubPath/MyAsset
			//     /Game/MyPath/MySubPath/MyAsset -> /MySubPath/

			const int32 ShortPackageNameLen = FPackageName::GetLongPackageAssetName(CurrentPackageName).Len();
			const int32 RelativePathLen = CurrentPackageName.Len() - ShortPackageNameLen - SourcePath.Len();
			const FString RelativeDestPath = CurrentPackageName.Mid(SourcePath.Len(), RelativePathLen);

			PackageName = DestinationPath + RelativeDestPath + ObjectName;
			GroupName = TEXT("");

			// Folder copies dont need a dialog
			InOutInfo.bOkToAll = true;
		}
		else if ( PreviousPackage.Len() )
		{
			// Use the last supplied path
			// Non-relative move/copy, use the location from the previous operation
			PackageName = FPackageName::GetLongPackagePath(PreviousPackage) + "/" + ObjectName;
			GroupName = TEXT("");
		}
		else if ( DestinationPath.Len() )
		{
			// Use the passed in default path
			// Normal path
			PackageName = DestinationPath + "/" + ObjectName;
			GroupName = TEXT("");
		}
		else
		{
			// Use the path from the old package
			PackageName = Object->GetOutermost()->GetName();
				
			GroupName = TEXT("");
		}

		// If the target package already exists, check for name clashes and find a unique name
		if ( InOutInfo.bOkToAll || bUniqueDefaultName )
		{
			UPackage* NewPackage = FindPackage(NULL, *PackageName);

			if ( NewPackage )
			{
				NewPackage->FullyLoad();
			}
			else
			{
				FString PackageFilename;
				if ( FPackageName::DoesPackageExist(PackageName, NULL, &PackageFilename) )
				{
					NewPackage = LoadPackage(NULL, *PackageFilename, LOAD_None);
				}
			}

			if (NewPackage)
			{
				FString ObjectPrefix = ObjectName;
				int32 Suffix = 2;

				// Check if this is already a copied object name and increment it if it is
				FString LeftSplit;
				FString RightSplit;
				if( ObjectName.Split( "_", &LeftSplit, &RightSplit, ESearchCase::CaseSensitive, ESearchDir::FromEnd ) == true )
				{
					bool bOnlyNumeric = true;
					for( int index = 0; index < RightSplit.Len(); index++ )
					{
						if( FChar::IsDigit(RightSplit[index] ) == false )
						{
							bOnlyNumeric = false;
							break;
						}
					}
					if( bOnlyNumeric == true )
					{
						Suffix = FCString::Atoi(*RightSplit) + 1;
						ObjectPrefix = LeftSplit;
					}
				}
				
				// If the package and object names were equal before, ensure that the generated names are also equal
				const FString PackageShortName = FPackageName::GetLongPackageAssetName(*PackageName);
				const FString PackagePath = FPackageName::GetLongPackagePath(*PackageName);
				FString PackagePrefix = (ObjectName == PackageShortName) ? (PackagePath / ObjectPrefix) : PackageName;

				for (; NewPackage && StaticFindObjectFast(NULL, NewPackage, FName(*ObjectName)); Suffix++)
				{
					// DlgName exists in DlgPackage - generate a new one with a numbered suffix
					ObjectName = FString::Printf(TEXT("%s_%d"), *ObjectPrefix, Suffix);

					// Don't change the package name if we encounter an object name clash when moving to a legacy package
					{
						PackageName = FString::Printf(TEXT("%s_%d"), *PackagePrefix, Suffix);
						NewPackage = FindPackage(NULL, *PackageName);

						if ( NewPackage )
						{
							NewPackage->FullyLoad();
						}
						else
						{
							FString PackageFilename;
							if ( FPackageName::DoesPackageExist(PackageName, NULL, &PackageFilename) )
							{
								NewPackage = LoadPackage(NULL, *PackageFilename, LOAD_None);
							}
						}
					}
				}
			}
		}

		if( !InOutInfo.bOkToAll )
		{
			// Present the user with a rename dialog for each asset.
			FDlgMoveAsset MoveDialog(/*bIsLegacyOrMapPackage*/ false, PackageName, GroupName, ObjectName, DialogTitle);

			const FDlgMoveAsset::EResult MoveDialogResult = MoveDialog.ShowModal();

			// Abort if the user cancelled.
			if( MoveDialogResult == FDlgMoveAsset::Cancel)
			{
				return false;
			}

			// Don't show the dialog again if "Ok to All" was selected.
			if( MoveDialogResult == FDlgMoveAsset::OKToAll )
			{
				InOutInfo.bOkToAll = true;
			}

			// Store the entered package/group/name for later retrieval.
			PackageName = MoveDialog.GetNewPackage();
			GroupName = MoveDialog.GetNewGroup();
			ObjectName = MoveDialog.GetNewName();

			// @todo asset: Should we interactively add localized packages
			//bSawOKToAll |= bLocPackages;
		}

		InOutInfo.PGN.PackageName = PackageName;
		InOutInfo.PGN.GroupName = GroupName;
		InOutInfo.PGN.ObjectName = ObjectName;

		return true;
	}


	bool RenameObjectsInternal( const TArray<UObject*>& Objects, bool bLocPackages, const TMap< UObject*, FString >* ObjectToLanguageExtMap, const FString& SourcePath, const FString& DestinationPath, bool bOpenDialog )
	{
		TSet<UPackage*> PackagesUserRefusedToFullyLoad;
		TArray<UPackage*> OutermostPackagesToSave;
		FText ErrorMessage;
		
		bool bSawSuccessfulRename = false;

		FMoveDialogInfo MoveDialogInfo;
		MoveDialogInfo.bOkToAll = !bOpenDialog;

		// The default value for save packages is true if SCC is enabled because the user can use SCC to revert a change
		MoveDialogInfo.bSavePackages = ISourceControlModule::Get().IsEnabled();

		for( int32 Index = 0; Index < Objects.Num(); Index++ )
		{
			UObject* Object = Objects[ Index ];
			if( !Object )
			{
				continue;
			}

			if ( !GetMoveDialogInfo(NSLOCTEXT("UnrealEd", "RenameObjects", "Move/Rename Objects" ), Object, /*bUniqueDefaultName=*/false, SourcePath, DestinationPath, MoveDialogInfo) )
			{
				// The user aborted the operation
				return false;
			}

			UPackage* OldPackage = Object->GetOutermost();
			if ( RenameSingleObject(Object, MoveDialogInfo.PGN, PackagesUserRefusedToFullyLoad, ErrorMessage, ObjectToLanguageExtMap) )
			{
				OutermostPackagesToSave.AddUnique( OldPackage );
				OutermostPackagesToSave.AddUnique( Object->GetOutermost() );
				bSawSuccessfulRename = true;
			}
		} // Selected objects.

		// Display any error messages that accumulated.
		if ( !ErrorMessage.IsEmpty() )
		{
			FMessageDialog::Open( EAppMsgType::Ok, ErrorMessage );
		}

		// Update the browser if something was actually renamed.
		if ( bSawSuccessfulRename )
		{
			bool bUpdateSCC = false;
			if ( MoveDialogInfo.bSavePackages )
			{
				const bool bCheckDirty = false;
				const bool bPromptToSave = false;
				FEditorFileUtils::PromptForCheckoutAndSave(OutermostPackagesToSave, bCheckDirty, bPromptToSave);
				bUpdateSCC = true;
			}

			if ( bUpdateSCC )
			{
				ISourceControlModule::Get().QueueStatusUpdate(OutermostPackagesToSave);
			}
		}

		return ErrorMessage.IsEmpty();
	}

	bool RenameSingleObject(UObject* Object, FPackageGroupName& PGN, TSet<UPackage*>& InOutPackagesUserRefusedToFullyLoad, FText& InOutErrorMessage, const TMap< UObject*, FString >* ObjectToLanguageExtMap, bool bLeaveRedirector)
	{
		FString ErrorMessage;

		if( !Object )
		{
			// Can not rename NULL objects.
			return false;
		}

		// @todo asset: Find an appropriate place for localized sounds
		bool bLocPackages = false;

		const FString& NewPackageName = PGN.PackageName;
		const FString& NewGroupName = PGN.GroupName;
		const FString& NewObjectName = PGN.ObjectName;

		const FScopedBusyCursor BusyCursor;

		bool bMoveFailed = false;
		bool bMoveRedirectorFailed = false;
		FMoveInfo MoveInfo;

		// The language extension for localized packages. Defaults to int32
		FString LanguageExt = TEXT("INT");

		// If the package the object is being moved to is new
		bool bPackageIsNew = false;

		if( bLocPackages && NewPackageName != Object->GetOutermost()->GetName() )
		{
			// If localized sounds are being moved to a different package 
			// make sure the package they are being moved to is valid
			if( ObjectToLanguageExtMap )
			{
				// Language extension package this object is in
				const FString* FoundLanguageExt = ObjectToLanguageExtMap->Find( Object );

				if( FoundLanguageExt && *FoundLanguageExt != TEXT("INT") )
				{
					// A language extension has been found for this object.
					// Append the package name with the language extension.
					// Do not append int32 packages as they have no extension
					LanguageExt = *FoundLanguageExt->ToUpper();
					PGN.PackageName += FString::Printf( TEXT("_%s"), *LanguageExt );
					PGN.GroupName += FString::Printf( TEXT("_%s"), *LanguageExt );
				}

			}

			// Check to see if the language specific path is the same as the path in the filename
			const FString LanguageSpecificPath = FString::Printf( TEXT("%s/%s"), TEXT("Sounds"), *LanguageExt );

			// Filename of the package we are moving from
			FString OriginPackageFilename;
			// If the object was is in a localized directory.  SoundWaves in non localized package file paths should  be able to move anywhere.
			bool bOriginPackageInLocalizedDir = false;
			if ( FPackageName::DoesPackageExist( Object->GetOutermost()->GetName(), NULL, &OriginPackageFilename ) )
			{
				// if the language specific path cant be found in the origin package filename, this package is not in a directory for only localized packages
				bOriginPackageInLocalizedDir = (OriginPackageFilename.Contains( LanguageSpecificPath ) );
			}

			// Filename of the package we are moving to
			FString DestPackageName;
			// Find the package filename of the package we are moving to.
			bPackageIsNew = !FPackageName::DoesPackageExist( NewPackageName, NULL, &DestPackageName );
			if( !bPackageIsNew && bOriginPackageInLocalizedDir && !DestPackageName.Contains( LanguageSpecificPath ) )
			{	
				// Skip new packages or packages not in localized dirs (objects in these can move anywhere)
				// If the the language specific path cannot be found in the destination package filename
				// This package is being moved to an invalid location.
				bMoveFailed = true;
				ErrorMessage += FText::Format( NSLOCTEXT("UnrealEd", "Error_InvalidMoveOfLocalizedObject", "Attempting to move localized sound {0} into non localized package or package with different localization.\n" ),
					FText::FromString(Object->GetName()) ).ToString();
			}
		}

		if ( !bMoveFailed )
		{
			// Make sure that a target package exists.
			if ( !NewPackageName.Len() )
			{
				ErrorMessage += TEXT("Invalid package name supplied\n");
				bMoveFailed = true;
			}
			else
			{
				// Make a full path from the target package and group.
				const FString FullPackageName = NewGroupName.Len()
					? FString::Printf(TEXT("%s.%s"), *NewPackageName, *NewGroupName)
					: NewPackageName;

				// Make sure the target package is fully loaded.
				TArray<UPackage*> TopLevelPackages;
				UPackage* ExistingPackage = FindPackage(NULL, *FullPackageName);
				UPackage* ExistingOutermostPackage = NewGroupName.Len() ? FindPackage(NULL, *NewPackageName) : ExistingPackage;

				if( ExistingPackage )
				{
					TopLevelPackages.Add( ExistingPackage->GetOutermost() );
				}

				// If there's an existing outermost package, try to find its filename
				FString ExistingOutermostPackageFilename;
				if ( ExistingOutermostPackage )
				{
					FPackageName::DoesPackageExist( ExistingOutermostPackage->GetName(), NULL, &ExistingOutermostPackageFilename );
				}

				// Fully load the ref objects package
				TopLevelPackages.Add( Object->GetOutermost() );

				// Used in the IsValidObjectName checks below
				FText Reason;

				if( ExistingPackage && ( InOutPackagesUserRefusedToFullyLoad.Contains(ExistingPackage) || !PackageTools::HandleFullyLoadingPackages( TopLevelPackages, NSLOCTEXT("UnrealEd", "Rename", "Rename") ) ) )
				{
					// HandleFullyLoadingPackages should never return false for empty input.
					check( ExistingPackage );
					InOutPackagesUserRefusedToFullyLoad.Add( ExistingPackage );
					bMoveFailed = true;
				}
				// Don't allow a move/rename to occur into a package that has a filename invalid for saving. This is a rare case
				// that should not happen often, but could occur using packages created before the editor checked against file name length
				else if ( ExistingOutermostPackage && ExistingOutermostPackageFilename.Len() > 0 && !FFileHelper::IsFilenameValidForSaving( ExistingOutermostPackageFilename, Reason ) )
				{
					bMoveFailed = true;
				}
				else if( !NewObjectName.Len() )
				{
					ErrorMessage += TEXT("Invalid object name\n");
					bMoveFailed = true;
				}
				else if(!FName(*NewObjectName).IsValidObjectName( Reason ) 
					||	!FPackageName::IsValidLongPackageName( NewPackageName, /*bIncludeReadOnlyRoots=*/false, &Reason )
					||	!FName(*NewGroupName).IsValidGroupName(Reason,true) )
				{
					// Make sure the object name is valid.
					ErrorMessage += FString::Printf(TEXT("    %s to %s.%s: %s\n"), *Object->GetPathName(), *FullPackageName, *NewObjectName, *Reason.ToString() );
					bMoveFailed = true;
				}
				else
				{
					// We can rename on top of an object redirection (basically destroy the redirection and put us in its place).
					UPackage* NewPackage = CreatePackage( NULL, *FullPackageName );
					NewPackage->GetOutermost()->FullyLoad();

					// Make sure we copy all the cooked package flags if the asset was already cooked.
					if (Object->GetOutermost()->HasAnyPackageFlags(PKG_FilterEditorOnly))
					{
						NewPackage->SetPackageFlags(PKG_FilterEditorOnly);
					}
					NewPackage->bIsCookedForEditor = Object->GetOutermost()->bIsCookedForEditor;

					UObjectRedirector* Redirector = Cast<UObjectRedirector>( StaticFindObject(UObjectRedirector::StaticClass(), NewPackage, *NewObjectName) );
					bool bFoundCompatibleRedirector = false;
					// If we found a redirector, check that the object it points to is of the same class.
					if ( Redirector
						&& Redirector->DestinationObject
						&& Redirector->DestinationObject->GetClass() == Object->GetClass() )
					{
						// Test renaming the redirector into a dummy package.
						if ( Redirector->Rename(*Redirector->GetName(), CreatePackage(NULL, TEXT("/Temp/TempRedirectors")), REN_Test) )
						{
							// Actually rename the redirector here so it doesn't get in the way of the rename below.
							Redirector->Rename(*Redirector->GetName(), CreatePackage(NULL, TEXT("/Temp/TempRedirectors")), REN_DontCreateRedirectors);

							bFoundCompatibleRedirector = true;
						}
						else
						{
							bMoveFailed = true;
							bMoveRedirectorFailed = true;
						}
					}

					if ( !bMoveFailed )
					{
						// Test to see if the rename will succeed.
						if ( Object->Rename(*NewObjectName, NewPackage, REN_Test) )
						{
							// No errors!  Set asset move info.
							MoveInfo.Set( *FullPackageName, *NewObjectName );

							// @todo asset: Find an appropriate place for localized sounds
							bLocPackages = false;
							if( bLocPackages && bPackageIsNew )
							{
								// Setup the path this localized package should be saved to.
								FString Path;

								// Newly renamed objects must have the single asset package extension
								Path = FPaths::Combine(*FPaths::ProjectDir(), TEXT("Content"), TEXT("Sounds"), *LanguageExt, *(FPackageName::GetLongPackageAssetName(NewPackageName) + FPackageName::GetAssetPackageExtension()));

								// Move the package into the correct file location by saving it
								GUnrealEd->Exec( NULL, *FString::Printf(TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\""), *NewPackageName, *Path) );
							}
						}
						else
						{
							const FString FullObjectPath = FString::Printf(TEXT("%s.%s"), *FullPackageName, *NewObjectName);
							ErrorMessage += FText::Format( NSLOCTEXT("UnrealEd", "Error_ObjectNameAlreadyExists", "An object named '{0}' already exists.\n"), FText::FromString(FullObjectPath) ).ToString();
							bMoveFailed = true;
						}
					}

					if (bFoundCompatibleRedirector)
					{
						// Rename the redirector back since we are just testing
						UPackage* DestinationPackage = FindPackage(NULL, *FullPackageName);

						if ( ensure(DestinationPackage) )
						{
							if ( Redirector->Rename(*Redirector->GetName(), DestinationPackage, REN_Test) )
							{
								Redirector->Rename(*Redirector->GetName(), DestinationPackage, REN_DontCreateRedirectors);
							}
							else
							{
								UE_LOG(LogObjectTools, Warning, TEXT("RenameObjectsInternal failed to return a redirector '%s' to its original location. This was because there was already an asset in the way. Deleting redirector."), *Redirector->GetName());
								DeleteRedirector(Redirector);
								Redirector = NULL;
							}
						}
					}
				}
			} // NewPackageName valid?
		}

		if ( !bMoveFailed )
		{
			// Actually perform the move!
			check( MoveInfo.IsValid() );

			const FString& PkgName = MoveInfo.FullPackageName;
			const FString& ObjName = MoveInfo.NewObjName;
			const FString FullObjectPath = FString::Printf(TEXT("%s.%s"), *PkgName, *ObjName);

			// We can rename on top of an object redirection (basically destroy the redirection and put us in its place).
			UObjectRedirector* Redirector = Cast<UObjectRedirector>( StaticFindObject(UObjectRedirector::StaticClass(), NULL, *FullObjectPath) );
			// If we found a redirector, check that the object it points to is of the same class.
			if ( Redirector
				&& Redirector->DestinationObject
				&& Redirector->DestinationObject->GetClass() == Object->GetClass() )
			{
				DeleteRedirector(Redirector);
				Redirector = NULL;
			}

			UPackage* NewPackage = CreatePackage( NULL, *PkgName );
			// if this object is being renamed out of the MyLevel package into a content package, we need to mark it RF_Standalone
			// so that it will be saved (UWorld::CleanupWorld() clears this flag for all objects inside the package)
			if (!Object->HasAnyFlags(RF_Standalone)
				&&	Object->GetOutermost()->ContainsMap()
				&&	!NewPackage->GetOutermost()->ContainsMap() )
			{
				Object->SetFlags(RF_Standalone);
			}

			UPackage *OldPackage = Object->GetOutermost();
			FString OldObjectFullName = Object->GetFullName();
			FString OldObjectPathName = Object->GetPathName();
			GEditor->RenameObject( Object, NewPackage, *ObjName, bLeaveRedirector ? REN_None : REN_DontCreateRedirectors );

			if (OldPackage && OldPackage->MetaData)
			{
				// Remove any metadata from old package pointing to moved objects
				OldPackage->MetaData->RemoveMetaDataOutsidePackage();
			}

			// Notify the asset registry of the rename
			FAssetRegistryModule::AssetRenamed(Object, OldObjectPathName);

			// If a redirector was created, notify the asset registry
			UObjectRedirector* NewRedirector = FindObject<UObjectRedirector>(NULL, *OldObjectPathName);
			if ( NewRedirector )
			{
				FAssetRegistryModule::AssetCreated(NewRedirector);
			}

			// Saw Successful Rename
			InOutErrorMessage = FText::FromString( ErrorMessage );
			return true;
		}
		else
		{
			if(bMoveRedirectorFailed)
			{
				ErrorMessage += FText::Format( NSLOCTEXT("UnrealEd", "Error_CouldntRenameObjectRedirectorF", "Couldn't rename '{0}' object because there is an object redirector of the same name, please fixup redirect from editor by enabling Show Redirects in content browser.\n"),
					FText::FromString(Object->GetFullName()) ).ToString();
			}
			else
			{
				ErrorMessage += FText::Format( NSLOCTEXT("UnrealEd", "Error_CouldntRenameObjectF", "Couldn't rename '{0}'.\n"), FText::FromString(Object->GetFullName()) ).ToString();
			}

			// @todo asset: Find an appropriate place for localized sounds
			bLocPackages = false;
			if( bLocPackages )
			{
				// Inform the user that no localized objects will be moved or renamed
				ErrorMessage += FString::Printf( TEXT("No localized objects could be moved"));
				// break out of the main loop, 
				//break;
			}
		}

		InOutErrorMessage = FText::FromString( ErrorMessage );
		return false;
	}

	/** 
	 * Finds all language variants for the passed in sound wave
	 * 
	 * @param OutObjects	A list of found localized sound wave objects
	 * @param OutObjectToLanguageExtMap	A mapping of sound wave objects to their language extension
	 * @param Wave	The sound wave to search for
	 */
	void AddLanguageVariants( TArray<UObject*>& OutObjects, TMap< UObject*, FString >& OutObjectToLanguageExtMap, USoundWave* Wave )
	{
		//@todo-packageloc Handle sound localization packages.
	}

	bool RenameObjects( const TArray< UObject* >& SelectedObjects, bool bIncludeLocInstances, const FString& SourcePath, const FString& DestinationPath, bool bOpenDialog ) 
	{
		// @todo asset: Find a proper location for localized files
		bIncludeLocInstances = false; //-V763
		if( !bIncludeLocInstances )
		{
			return RenameObjectsInternal( SelectedObjects, bIncludeLocInstances, NULL, SourcePath, DestinationPath, bOpenDialog );
		}
		else
		{
			bool bSucceed = true;
			// For each object, find any localized variations and rename them as well
			for( int32 Index = 0; Index < SelectedObjects.Num(); Index++ )
			{
				TArray<UObject*> LocObjects;
				LocObjects.Empty();

				UObject* Object = SelectedObjects[ Index ];
				if( Object )
				{
					// NOTE: Only supported for SoundWaves right now
					USoundWave* Wave = ExactCast<USoundWave>( Object );
					if( Wave )
					{
						// A mapping of object to language extension, so we know where to move the localized sounds to if the user requests it.
						TMap< UObject*, FString > ObjectToLanguageExtMap;
						// Find if this is localized and add in the other languages
						AddLanguageVariants( LocObjects, ObjectToLanguageExtMap, Wave );
						// Prompt the user, and rename the files.
						bSucceed &= RenameObjectsInternal( LocObjects, bIncludeLocInstances, &ObjectToLanguageExtMap, SourcePath, DestinationPath, bOpenDialog );
					}
				}
			}

			return bSucceed;
		}
	}

	FString SanitizeObjectName (const FString& InObjectName)
	{
		FString SanitizedName;
		FString InvalidChars = INVALID_OBJECTNAME_CHARACTERS;

		// See if the name contains invalid characters.
		FString Char;
		for( int32 CharIdx = 0; CharIdx < InObjectName.Len(); ++CharIdx )
		{
			Char = InObjectName.Mid(CharIdx, 1);

			if ( InvalidChars.Contains(*Char) )
			{
				SanitizedName += TEXT("_");
			}
			else
			{
				SanitizedName += Char;
			}
		}

		return SanitizedName;
	}

	/**
	 * Internal helper function to obtain format descriptions and extensions of formats supported by the provided factory
	 *
	 * @param	InFactory			Factory whose formats should be retrieved
	 * @param	out_Descriptions	Array of format descriptions associated with the current factory; should equal the number of extensions
	 * @param	out_Extensions		Array of format extensions associated with the current factory; should equal the number of descriptions
	 */
	void InternalGetFactoryFormatInfo( const UFactory* InFactory, TArray<FString>& out_Descriptions, TArray<FString>& out_Extensions )
	{
		check(InFactory);

		// Iterate over each format the factory accepts
		for ( TArray<FString>::TConstIterator FormatIter( InFactory->Formats ); FormatIter; ++FormatIter )
		{
			const FString& CurFormat = *FormatIter;

			// Parse the format into its extension and description parts
			TArray<FString> FormatComponents;
			CurFormat.ParseIntoArray( FormatComponents, TEXT(";"), false );

			for ( int32 ComponentIndex = 0; ComponentIndex < FormatComponents.Num(); ComponentIndex += 2 )
			{
				check( FormatComponents.IsValidIndex( ComponentIndex + 1 ) );
				out_Extensions.Add( FormatComponents[ComponentIndex] );
				out_Descriptions.Add( FormatComponents[ComponentIndex + 1] );
			}
		}
	}

	/**
	 * Populates two strings with all of the file types and extensions the provided factory supports.
	 *
	 * @param	InFactory		Factory whose supported file types and extensions should be retrieved
	 * @param	out_Filetypes	File types supported by the provided factory, concatenated into a string
	 * @param	out_Extensions	Extensions supported by the provided factory, concatenated into a string
	 */
	void GenerateFactoryFileExtensions( UFactory* InFactory, FString& out_Filetypes, FString& out_Extensions, TMultiMap<uint32, UFactory*>& out_FilterIndexToFactory )
	{
		// Place the factory in an array and call the overloaded version of this function
		TArray<UFactory*> FactoryArray;
		FactoryArray.Add( InFactory );
		GenerateFactoryFileExtensions( FactoryArray, out_Filetypes, out_Extensions, out_FilterIndexToFactory );
	}

	/**
	 * Populates two strings with all of the file types and extensions the provided factories support.
	 *
	 * @param	InFactories		Factories whose supported file types and extensions should be retrieved
	 * @param	out_Filetypes	File types supported by the provided factory, concatenated into a string
	 * @param	out_Extensions	Extensions supported by the provided factory, concatenated into a string
	 */
	void GenerateFactoryFileExtensions( const TArray<UFactory*>& InFactories, FString& out_Filetypes, FString& out_Extensions, TMultiMap<uint32, UFactory*>& out_FilterIndexToFactory )
	{
		// Store all the descriptions and their corresponding extensions in a map
		TMultiMap<FString, FString> DescToExtensionMap;
		TMultiMap<FString, UFactory*> DescToFactory;

		// Iterate over each factory, retrieving their supported file descriptions and extensions, and storing them into the map
		for ( TArray<UFactory*>::TConstIterator FactoryIter(InFactories); FactoryIter; ++FactoryIter )
		{
			const UFactory* CurFactory = *FactoryIter;
			check(CurFactory);

			TArray<FString> Descriptions;
			TArray<FString> Extensions;
			InternalGetFactoryFormatInfo( CurFactory, Descriptions, Extensions );
			check( Descriptions.Num() == Extensions.Num() );

			// Make sure to only store each key, value pair once
			for ( int32 FormatIndex = 0; FormatIndex < Descriptions.Num() && FormatIndex < Extensions.Num(); ++FormatIndex )
			{
				DescToExtensionMap.AddUnique( Descriptions[FormatIndex], Extensions[FormatIndex ] );
				DescToFactory.AddUnique( Descriptions[FormatIndex], *FactoryIter );
			}
		}
		
		// Zero out the output strings in case they came in with data already
		out_Filetypes = ""; 
		out_Extensions = "";

		// Sort the map's keys alphabetically
		DescToExtensionMap.KeySort( TLess<FString>() );
		
		// Retrieve an array of all of the unique keys within the map
		TArray<FString> DescriptionKeyMap;
		DescToExtensionMap.GetKeys( DescriptionKeyMap );
		const TArray<FString>& DescriptionKeys = DescriptionKeyMap;

		uint32 IdxFilter = 1; // the type list will start by an all supported files wildcard value

		// Iterate over each unique map key, retrieving all of each key's associated values in order to populate the strings
		for ( TArray<FString>::TConstIterator DescIter( DescriptionKeys ); DescIter; ++DescIter )
		{
			const FString& CurDescription = *DescIter;
			
			// Retrieve each value associated with the current key
			TArray<FString> Extensions;
			DescToExtensionMap.MultiFind( CurDescription, Extensions );
			if ( Extensions.Num() > 0 )
			{
				// Sort each extension alphabetically, so that the output is alphabetical by description, and in the event of
				// a description with multiple extensions, alphabetical by extension as well
				Extensions.Sort();
				
				for ( TArray<FString>::TConstIterator ExtIter( Extensions ); ExtIter; ++ExtIter )
				{
					const FString& CurExtension = *ExtIter;
					const FString& CurLine = FString::Printf( TEXT("%s (*.%s)|*.%s"), *CurDescription, *CurExtension, *CurExtension );

					// The same extension could be used for multiple types (like with t3d), so ensure any given extension is only added to the string once
					if ( !out_Extensions.Contains( CurExtension) )
					{
						if ( out_Extensions.Len() > 0 )
						{
							out_Extensions += TEXT(";");
						}
						out_Extensions += FString::Printf(TEXT("*.%s"), *CurExtension);
					}

					// Each description-extension pair can only appear once in the map, so no need to check the string for duplicates
					if ( out_Filetypes.Len() > 0 )
					{
						out_Filetypes += TEXT("|");
					}
					out_Filetypes += CurLine;

					// save the order in which descriptions are added to be able to identify 
					// factories using filter index
					TArray<UFactory*> Factories;
					DescToFactory.MultiFind( CurDescription, Factories );
					TArray<UFactory*>::TIterator FactIt(Factories);
					for (;FactIt;++FactIt)
					{
						out_FilterIndexToFactory.Add( IdxFilter, *FactIt );
					}
					++IdxFilter;
				}
			}
		}
	}

	/**
	 * Generates a list of file types for a given class.
	 */
	void AppendFactoryFileExtensions ( UFactory* InFactory, FString& out_Filetypes, FString& out_Extensions )
	{
		TArray<FString> Descriptions;
		TArray<FString> Extensions;
		InternalGetFactoryFormatInfo( InFactory, Descriptions, Extensions );
		check( Descriptions.Num() == Extensions.Num() );

		for ( int32 FormatIndex = 0; FormatIndex < Descriptions.Num() && FormatIndex < Extensions.Num(); ++FormatIndex )
		{
			const FString& CurDescription = Descriptions[FormatIndex];
			const FString& CurExtension = Extensions[FormatIndex];
			const FString& CurLine = FString::Printf( TEXT("%s (*.%s)|*.%s"), *CurDescription, *CurExtension, *CurExtension );

			// Only append the extension if it's not already one of the found extensions
			if ( !out_Extensions.Contains( CurExtension) )
			{
				if ( out_Extensions.Len() > 0 )
				{
					out_Extensions += TEXT(";");
				}
				out_Extensions += FString::Printf(TEXT("*.%s"), *CurExtension);
			}

			// Only append the line if it's not already one of the found filetypes
			if ( !out_Filetypes.Contains( CurLine) )
			{
				if ( out_Filetypes.Len() > 0 )
				{
					out_Filetypes += TEXT("|");
				}
				out_Filetypes += CurLine;
			}
		}
	}

	/**
	 * Iterates over all classes and assembles a list of non-abstract UExport-derived type instances.
	 */
	void AssembleListOfExporters(TArray<UExporter*>& OutExporters)
	{
		auto TransientPackage = GetTransientPackage();

		// @todo DB: Assemble this set once.
		OutExporters.Empty();
		for( TObjectIterator<UClass> It ; It ; ++It )
		{
			if( It->IsChildOf(UExporter::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract) )
			{
				UExporter* Exporter = NewObject<UExporter>(TransientPackage, *It);
				OutExporters.Add( Exporter );		
			}
		}
	}

	/**
	 * Assembles a path from the outer chain of the specified object.
	 */
	void GetDirectoryFromObjectPath(const UObject* Obj, FString& OutResult)
	{
		if( Obj )
		{
			GetDirectoryFromObjectPath( Obj->GetOuter(), OutResult );
			OutResult /= Obj->GetName();
		}
	}
	
	/**
	 * Exports the specified objects to file.
	 *
	 * @param	ObjectsToExport					The set of objects to export.
	 * @param	bPromptIndividualFilenames		If true, prompt individually for filenames.  If false, bulk export to a single directory.
	 * @param	ExportPath						receives the value of the path the user chose for exporting.
	 * @param	bUseProvidedExportPath			If true and out_ExportPath is specified, use the value in out_ExportPath as the export path w/o prompting for a directory when applicable
	 */
	void ExportObjects(const TArray<UObject*>& ObjectsToExport, bool bPromptIndividualFilenames, FString* ExportPath/*=NULL*/, bool bUseProvidedExportPath /*= false*/ )
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

		if (ExportPath && bUseProvidedExportPath && !bPromptIndividualFilenames)
		{
			AssetToolsModule.Get().ExportAssets(ObjectsToExport, *ExportPath);
		}
		else
		{
			AssetToolsModule.Get().ExportAssetsWithDialog(ObjectsToExport, bPromptIndividualFilenames);
		}
	}

	/**
	 * Tags objects which are in use by levels specified by the search option 
	 *
	 * @param SearchOption	 The search option for finding in use objects
	 */
	void TagInUseObjects( EInUseSearchOption SearchOption )
	{
		UWorld* World = GWorld;
		TSet<UObject*> LevelPackages;
		TSet<UObject*> Levels;

		if( !World )
		{
			// Don't do anything if there is no World.  This could be called during a level load transition
			return;
		}

		switch( SearchOption )
		{
		case SO_CurrentLevel:
			LevelPackages.Add( World->GetCurrentLevel()->GetOutermost() );
			Levels.Add( World->GetCurrentLevel() );
			break;
		case SO_VisibleLevels:
			// Add the persistent level if its visible
			if( FLevelUtils::IsLevelVisible( World->PersistentLevel ) ) 
			{
				LevelPackages.Add( World->PersistentLevel->GetOutermost() );
				Levels.Add( World->PersistentLevel );
			}
			// Add all other levels if they are visible
			for( int32 LevelIndex = 0; LevelIndex < World->StreamingLevels.Num(); ++LevelIndex )
			{
				ULevelStreaming* StreamingLevel = World->StreamingLevels[ LevelIndex ];

				if( StreamingLevel != NULL && FLevelUtils::IsLevelVisible( StreamingLevel ) )
				{
					ULevel* Level = StreamingLevel->GetLoadedLevel();
					
					if ( Level != NULL )
					{
						LevelPackages.Add( Level->GetOutermost() );
						Levels.Add( Level );
					}
				}
			}
			break;
		case SO_LoadedLevels:
			// Add the persistent level as its always loaded
			LevelPackages.Add( World->PersistentLevel->GetOutermost() );
			Levels.Add( World->PersistentLevel );
			
			// Add all other levels
			for( int32 LevelIndex = 0; LevelIndex < World->StreamingLevels.Num(); ++LevelIndex )
			{
				ULevelStreaming* StreamingLevel = World->StreamingLevels[ LevelIndex ];

				if( StreamingLevel != NULL )
				{
					ULevel* Level = StreamingLevel->GetLoadedLevel();
					
					if ( Level != NULL )
					{
						LevelPackages.Add( Level->GetOutermost() );
						Levels.Add( Level );
					}
				}
			}
			break;
		default:
			// A bad option was passed in.
			check(0);
		}	

		TArray<UObject*> ObjectsInLevels;

		for( FObjectIterator It; It; ++It )
		{
			UObject* Obj = *It;

			// Clear all marked flags that could have been tagged in a previous search or by another system.
			Obj->UnMark(EObjectMark(OBJECTMARK_TagImp | OBJECTMARK_TagExp));

		
			// If the object is not flagged for GC and it is in one of the level packages do an indepth search to see what references it.
			if( !Obj->IsPendingKillOrUnreachable() && LevelPackages.Find( Obj->GetOutermost() ) != NULL )
			{
				// Determine if the current object is in one of the search levels.  This is the same as UObject::IsIn except that we can
				// search through many levels at once.
				for ( UObject* ObjectOuter = Obj->GetOuter(); ObjectOuter; ObjectOuter = ObjectOuter->GetOuter() )
				{
					if ( Levels.Find(ObjectOuter) != NULL )
					{
						// this object was contained within one of our ReferenceRoots
						ObjectsInLevels.Add( Obj );

						// If the object is using a blueprint generated class, also add the blueprint as a reference
						UBlueprint* const Blueprint = Cast<UBlueprint>(Obj->GetClass()->ClassGeneratedBy);
						if ( Blueprint )
						{
							ObjectsInLevels.Add( Blueprint );
						}
						break;
					}
				}
			}
			else if( Obj->IsA( AWorldSettings::StaticClass() ) )
			{
				// If a skipped object is a world info ensure it is not serialized because it may contain 
				// references to levels (and by extension, their actors) that we are not searching for references to.
				Obj->Mark(OBJECTMARK_TagImp);
			}
		}

		// Tag all objects that are referenced by objects in the levels were are searching.
		FArchiveReferenceMarker Marker( ObjectsInLevels );
	}

	TSharedPtr<SWindow> OpenPropertiesForSelectedObjects( const TArray<UObject*>& SelectedObjects )
	{
		TSharedPtr<SWindow> FloatingDetailsView;
		if ( SelectedObjects.Num() > 0 )
		{
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

			FloatingDetailsView = PropertyEditorModule.CreateFloatingDetailsView( SelectedObjects, false );
		}

		return FloatingDetailsView;
	}
	
	void RemoveDeletedObjectsFromPropertyWindows( TArray<UObject*>& DeletedObjects )
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.RemoveDeletedObjects( DeletedObjects );
	}

	bool IsAssetValidForPlacing(UWorld* InWorld, const FString& ObjectPath)
	{
		bool bResult = ObjectPath.Len() > 0;
		if ( bResult )
		{
			bResult = !FEditorFileUtils::IsMapPackageAsset(ObjectPath);
			if ( !bResult )
			{
				// if this map is loaded, allow the asset to be placed
				FString AssetPackageName = FEditorFileUtils::ExtractPackageName(ObjectPath);
				if ( AssetPackageName.Len() > 0 )
				{
					UPackage* AssetPackage = FindObjectSafe<UPackage>(NULL, *AssetPackageName, true);
					if ( AssetPackage != NULL )
					{
						// so it's loaded - make sure it is the current map
						TArray<UWorld*> CurrentMapWorlds;
						EditorLevelUtils::GetWorlds(InWorld, CurrentMapWorlds, true);
						for ( int32 WorldIndex = 0; WorldIndex < CurrentMapWorlds.Num(); WorldIndex++ )
						{
							UWorld* World = CurrentMapWorlds[WorldIndex];
							if ( World != NULL && World->GetOutermost() == AssetPackage )
							{
								bResult = true;
								break;
							}
						}
					}
				}
			}
		}

		return bResult;
	}

	bool IsClassValidForPlacing(const UClass* InClass)
	{
		check(InClass);

		const bool bIsPlaceable = !InClass->HasAllClassFlags(CLASS_NotPlaceable) && (InClass->IsChildOf(AActor::StaticClass()) || InClass->IsChildOf(ABrush::StaticClass()) || InClass->IsChildOf(AVolume::StaticClass()));
		const bool bIsAbstractOrDeprecated = InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
		const bool bIsSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(InClass);
		
		return bIsPlaceable && !bIsAbstractOrDeprecated && !bIsSkeletonClass;
	}

	bool AreObjectsOfEquivalantType( const TArray<UObject*>& InProposedObjects )
	{
		if ( InProposedObjects.Num() > 0 )
		{
			// Use the first proposed object as the basis for the compatible check.
			const UObject* ComparisonObject = InProposedObjects[0];
			check( ComparisonObject );

			const UClass* ComparisonClass = ComparisonObject->GetClass();
			check( ComparisonClass );

			// Iterate over each proposed consolidation object, checking if each shares a common class with the consolidation objects, or at least, a common base that
			// is allowed as an exception (currently only exceptions made for textures and materials).
			for ( TArray<UObject*>::TConstIterator ProposedObjIter( InProposedObjects ); ProposedObjIter; ++ProposedObjIter )
			{
				UObject* CurProposedObj = *ProposedObjIter;
				check( CurProposedObj );

				const UClass* CurProposedClass = CurProposedObj->GetClass();

				if (ComparisonClass->IsChildOf(UBlueprint::StaticClass()) && CurProposedClass->IsChildOf(UBlueprint::StaticClass()))
				{
					if (*CastChecked<UBlueprint>(ComparisonObject)->ParentClass != *CastChecked<UBlueprint>(CurProposedObj)->ParentClass)
					{
						return false;
					}
				}

				if ( !AreClassesInterchangeable( ComparisonClass, CurProposedClass ) )
				{
					return false;
				}
			}
		}

		return true;
	}

	bool IsClassRedirector( const UClass* Class )
	{
		if ( Class == nullptr )
		{
			return false;
		}

		// You may not consolidate object redirectors
		if ( Class->IsChildOf( UObjectRedirector::StaticClass() ) )
		{
			return true;
		}

		return false;
	}

	bool AreClassesInterchangeable( const UClass* ClassA, const UClass* ClassB )
	{
		// You may not consolidate object redirectors
		if ( IsClassRedirector( ClassB ) )
		{
			return false;
		}

		if ( ClassB != ClassA )
		{
			const UClass* NearestCommonBase = ClassB->FindNearestCommonBaseClass( ClassA );

			// If the proposed object doesn't share a common class or a common base that is allowed as an exception, it is not a compatible object
			if ( !( NearestCommonBase->IsChildOf( UTexture::StaticClass() ) ) && !( NearestCommonBase->IsChildOf( UMaterialInterface::StaticClass() ) ) )
			{
				return false;
			}
		}

		return true;
	}
}





namespace ThumbnailTools
{

	/** Renders a thumbnail for the specified object */
	void RenderThumbnail( UObject* InObject, const uint32 InImageWidth, const uint32 InImageHeight, EThumbnailTextureFlushMode::Type InFlushMode, FTextureRenderTargetResource* InTextureRenderTargetResource, FObjectThumbnail* OutThumbnail )
	{
		// Renderer must be initialized before generating thumbnails
		check( GIsRHIInitialized );

		// Store dimensions
		if ( OutThumbnail )
		{
			OutThumbnail->SetImageSize( InImageWidth, InImageHeight );
		}

		// Grab the actual render target resource from the texture.  Note that we're absolutely NOT ALLOWED to
		// dereference this pointer.  We're just passing it along to other functions that will use it on the render
		// thread.  The only thing we're allowed to do is check to see if it's NULL or not.
		FTextureRenderTargetResource* RenderTargetResource = InTextureRenderTargetResource;
		if ( RenderTargetResource == NULL )
		{
			// No render target was supplied, just use a scratch texture render target
			const uint32 MinRenderTargetSize = FMath::Max( InImageWidth, InImageHeight );
			UTextureRenderTarget2D* RenderTargetTexture = GEditor->GetScratchRenderTarget( MinRenderTargetSize );
			check( RenderTargetTexture != NULL );

			// Make sure the input dimensions are OK.  The requested dimensions must be less than or equal to
			// our scratch render target size.
			check( InImageWidth <= RenderTargetTexture->GetSurfaceWidth() );
			check( InImageHeight <= RenderTargetTexture->GetSurfaceHeight() );

			RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();
		}
		check( RenderTargetResource != NULL );

		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->ProcessAsyncResults(false, true);
		}

		// Create a canvas for the render target and clear it to black
		FCanvas Canvas( RenderTargetResource, NULL, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime, GMaxRHIFeatureLevel );
		Canvas.Clear( FLinearColor::Black );


		// Get the rendering info for this object
		FThumbnailRenderingInfo* RenderInfo =
			GUnrealEd->GetThumbnailManager()->GetRenderingInfo( InObject );

		// Wait for all textures to be streamed in before we render the thumbnail
		// @todo CB: This helps but doesn't result in 100%-streamed-in resources every time! :(
		if( InFlushMode == EThumbnailTextureFlushMode::AlwaysFlush )
		{
			FlushAsyncLoading();

			IStreamingManager::Get().StreamAllResources( 100.0f );
		}

		// If this object's thumbnail will be rendered to a texture on the GPU.
		bool bUseGPUGeneratedThumbnail = true;

		if( RenderInfo != NULL && RenderInfo->Renderer != NULL )
		{
			const float ZoomFactor = 1.0f;

			uint32 DrawWidth = InImageWidth;
			uint32 DrawHeight = InImageHeight;
			if ( OutThumbnail )
			{
				// Find how big the thumbnail WANTS to be
				uint32 DesiredWidth = 0;
				uint32 DesiredHeight = 0;
				{
					// Currently we only allow textures/icons (and derived classes) to override our desired size
					// @todo CB: Some thumbnail renderers (like particles and lens flares) hard code their own
					//	   arbitrary thumbnail size even though they derive from TextureThumbnailRenderer
					if( RenderInfo->Renderer->IsA( UTextureThumbnailRenderer::StaticClass() ) )
					{
						RenderInfo->Renderer->GetThumbnailSize(
							InObject,
							ZoomFactor,
							DesiredWidth,		// Out
							DesiredHeight );	// Out
					}
				}

				// Does this thumbnail have a size associated with it?  Materials and textures often do!
				if( DesiredWidth > 0 && DesiredHeight > 0 )
				{
					// Scale the desired size down if it's too big, preserving aspect ratio
					if( DesiredWidth > InImageWidth )
					{
						DesiredHeight = ( DesiredHeight * InImageWidth ) / DesiredWidth;
						DesiredWidth = InImageWidth;
					}
					if( DesiredHeight > InImageHeight )
					{
						DesiredWidth = ( DesiredWidth * InImageHeight ) / DesiredHeight;
						DesiredHeight = InImageHeight;
					}

					// Update dimensions
					DrawWidth = FMath::Max<uint32>(1, DesiredWidth);
					DrawHeight = FMath::Max<uint32>(1, DesiredHeight);
					OutThumbnail->SetImageSize( DrawWidth, DrawHeight );
				}
			}
			
			// Draw the thumbnail
			const int32 XPos = 0;
			const int32 YPos = 0;
			RenderInfo->Renderer->Draw(
				InObject,
				XPos,
				YPos,
				DrawWidth,
				DrawHeight,
				RenderTargetResource,
				&Canvas
				);
		}

		// GPU based thumbnail rendering only
		if( bUseGPUGeneratedThumbnail )
		{
			// Tell the rendering thread to draw any remaining batched elements
			Canvas.Flush_GameThread();


			{
				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
					UpdateThumbnailRTCommand,
					FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
				{
					// Copy (resolve) the rendered thumbnail from the render target to its texture
					RHICmdList.CopyToResolveTarget(
						RenderTargetResource->GetRenderTargetTexture(),		// Source texture
						RenderTargetResource->TextureRHI,					// Dest texture
						false,												// Do we need the source image content again?
						FResolveParams() );									// Resolve parameters
				});

				if(OutThumbnail)
				{
					const FIntRect InSrcRect(0,	0, OutThumbnail->GetImageWidth(), OutThumbnail->GetImageHeight());

					TArray<uint8>& OutData = OutThumbnail->AccessImageData();

					OutData.Empty();
					OutData.AddUninitialized(OutThumbnail->GetImageWidth() * OutThumbnail->GetImageHeight() * sizeof(FColor));

					// Copy the contents of the remote texture to system memory
					// NOTE: OutRawImageData must be a preallocated buffer!
					RenderTargetResource->ReadPixelsPtr((FColor*)OutData.GetData(), FReadSurfaceDataFlags(), InSrcRect);
				}
			}
		}
	}


	/** Generates a thumbnail for the specified object and caches it */
	FObjectThumbnail* GenerateThumbnailForObjectToSaveToDisk( UObject* InObject )
	{
		// Does the object support thumbnails?
		FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo( InObject );
		if( RenderInfo != NULL && RenderInfo->Renderer != NULL )
		{
			// Set the size of cached thumbnails
			const int32 ImageWidth = 	ThumbnailTools::DefaultThumbnailSize;
			const int32 ImageHeight = ThumbnailTools::DefaultThumbnailSize;
			
			// For cached thumbnails we want to make sure that textures are fully streamed in so that the thumbnail we're saving won't have artifacts
			// However, this can add 30s - 100s to editor load
			//@todo - come up with a cleaner solution for this, preferably not blocking on texture streaming at all but updating when textures are fully streamed in
			ThumbnailTools::EThumbnailTextureFlushMode::Type TextureFlushMode = ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush;

			// When generating a material thumbnail to save in a package, make sure we finish compilation on the material first
			if ( UMaterial* InMaterial = Cast<UMaterial>(InObject) )
			{
				FScopedSlowTask SlowTask(0, NSLOCTEXT( "ObjectTools", "FinishingCompilationStatus", "Finishing Shader Compilation..." ) );
				SlowTask.MakeDialog();

				// Block until the shader maps that we will save have finished being compiled
				InMaterial->GetMaterialResource(GMaxRHIFeatureLevel)->FinishCompilation();
			}

			// Generate the thumbnail
			FObjectThumbnail NewThumbnail;
			ThumbnailTools::RenderThumbnail(
				InObject, ImageWidth, ImageHeight, TextureFlushMode, NULL,
				&NewThumbnail );		// Out

			UPackage* MyOutermostPackage = InObject->GetOutermost();
			return CacheThumbnail( InObject->GetFullName(), &NewThumbnail, MyOutermostPackage );
		}

		return NULL;
	}

	/**
	 * Caches a thumbnail into a package's thumbnail map.
	 *
	 * @param	ObjectFullName	the full name for the object to associate with the thumbnail
	 * @param	Thumbnail		the thumbnail to cache; specify NULL to remove the current cached thumbnail
	 * @param	DestPackage		the package that will hold the cached thumbnail
	 *
	 * @return	pointer to the thumbnail data that was cached into the package
	 */
	FObjectThumbnail* CacheThumbnail( const FString& ObjectFullName, FObjectThumbnail* Thumbnail, UPackage* DestPackage )
	{
		FObjectThumbnail* Result = NULL;

		if ( ObjectFullName.Len() > 0 && DestPackage != NULL )
		{
			// Create a new thumbnail map if we don't have one already
			if( !DestPackage->ThumbnailMap )
			{
				DestPackage->ThumbnailMap = MakeUnique<FThumbnailMap>();
			}

			// @todo thumbnails: Backwards compat
			FName ObjectFullNameFName( *ObjectFullName );
			FObjectThumbnail* CachedThumbnail = DestPackage->ThumbnailMap->Find( ObjectFullNameFName );
			if ( Thumbnail != NULL )
			{
				// Cache the thumbnail (possibly replacing an existing thumb!)
				Result = &DestPackage->ThumbnailMap->Add( ObjectFullNameFName, *Thumbnail );
			}
			//only let thumbnails loaded from disk to be removed.  
			//When capturing thumbnails from the content browser, it will only exist in memory until it is saved out to a package.
			//Don't let the recycling purge them
			else if ((CachedThumbnail != NULL) && (CachedThumbnail->IsLoadedFromDisk()))
			{
				DestPackage->ThumbnailMap->Remove( ObjectFullNameFName );
			}
		
		}

		return Result;
	}



	/**
	 * Caches an empty thumbnail entry
	 *
	 * @param	ObjectFullName	the full name for the object to associate with the thumbnail
	 * @param	DestPackage		the package that will hold the cached thumbnail
	 */
	void CacheEmptyThumbnail( const FString& ObjectFullName, UPackage* DestPackage )
	{
		FObjectThumbnail EmptyThumbnail;
		CacheThumbnail( ObjectFullName, &EmptyThumbnail, DestPackage );
	}



	bool QueryPackageFileNameForObject( const FString& InFullName, FString& OutPackageFileName )
	{
		// First strip off the class name
		int32 FirstSpaceIndex = InFullName.Find( TEXT( " " ) );
		if( FirstSpaceIndex == INDEX_NONE || FirstSpaceIndex <= 0 )
		{
			// Malformed full name
			return false;
		}

		// Determine the package file path/name for the specified object
		FString ObjectPathName = InFullName.Mid( FirstSpaceIndex + 1 );

		// Pull the package out of the fully qualified object path
		int32 FirstDotIndex = ObjectPathName.Find( TEXT( "." ) );
		if( FirstDotIndex == INDEX_NONE || FirstDotIndex <= 0 )
		{
			// Malformed object path
			return false;
		}

		FString PackageName = ObjectPathName.Left( FirstDotIndex );

		// Ask the package file cache for the full path to this package
		if( !FPackageName::DoesPackageExist( PackageName, NULL, &OutPackageFileName ) )
		{
			// Couldn't find the package in our cache
			return false;
		}

		return true;
	}



	/** Searches for an object's thumbnail in memory and returns it if found */
	FObjectThumbnail* FindCachedThumbnailInPackage( UPackage* InPackage, const FName InObjectFullName )
	{
		FObjectThumbnail* FoundThumbnail = NULL;

		// We're expecting this to be an outermost package!
		check( InPackage->GetOutermost() == InPackage );

		// Does the package have any thumbnails?
		if( InPackage->HasThumbnailMap() )
		{
			// @todo thumbnails: Backwards compat
			FThumbnailMap& PackageThumbnailMap = InPackage->AccessThumbnailMap();
			FoundThumbnail = PackageThumbnailMap.Find( InObjectFullName );
		}

		return FoundThumbnail;
	}



	/** Searches for an object's thumbnail in memory and returns it if found */
	FObjectThumbnail* FindCachedThumbnailInPackage( const FString& InPackageFileName, const FName InObjectFullName )
	{
		FObjectThumbnail* FoundThumbnail = NULL;

		FString PackageName = InPackageFileName;
		FPackageName::TryConvertFilenameToLongPackageName(PackageName, PackageName);

		// First check to see if the package is already in memory.  If it is, some or all of the thumbnails
		// may already be loaded and ready.
		UObject* PackageOuter = NULL;
		UPackage* Package = FindPackage( PackageOuter, *PackageName);
		if( Package != NULL )
		{
			FoundThumbnail = FindCachedThumbnailInPackage( Package, InObjectFullName );
		}

		return FoundThumbnail;
	}



	/** Searches for an object's thumbnail in memory and returns it if found */
	const FObjectThumbnail* FindCachedThumbnail( const FString& InFullName )
	{
		// Determine the package file path/name for the specified object
		FString PackageFilePathName;
		if( !QueryPackageFileNameForObject( InFullName, PackageFilePathName ) )
		{
			// Couldn't find the package in our cache
			return NULL;
		}

		return FindCachedThumbnailInPackage( PackageFilePathName, FName( *InFullName ) );
	}



	/** Returns the thumbnail for the specified object or NULL if one doesn't exist yet */
	FObjectThumbnail* GetThumbnailForObject( UObject* InObject )
	{
		UPackage* ObjectPackage = InObject->GetOutermost();
		return FindCachedThumbnailInPackage( ObjectPackage, FName( *InObject->GetFullName() ) );
	}



	/** Loads thumbnails from the specified package file name */
	bool LoadThumbnailsFromPackage( const FString& InPackageFileName, const TSet< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails )
	{
		// Create a file reader to load the file
		TUniquePtr< FArchive > FileReader( IFileManager::Get().CreateFileReader( *InPackageFileName ) );
		if( FileReader == nullptr )
		{
			// Couldn't open the file
			return false;
		}


		// Read package file summary from the file
		FPackageFileSummary FileSummary;
		(*FileReader) << FileSummary;


		// Make sure this is indeed a package
		if( FileSummary.Tag != PACKAGE_FILE_TAG )
		{
			// Unrecognized or malformed package file
			return false;
		}

		
		// Does the package contains a thumbnail table?
		if( FileSummary.ThumbnailTableOffset == 0 )
		{
			// No thumbnails to be loaded
			return false;
		}

		
		// Seek the the part of the file where the thumbnail table lives
		FileReader->Seek( FileSummary.ThumbnailTableOffset );

		int32 LastFileOffset = -1;
		// Load the thumbnail table of contents
		TMap< FName, int32 > ObjectNameToFileOffsetMap;
		{
			// Load the thumbnail count
			int32 ThumbnailCount = 0;
			*FileReader << ThumbnailCount;

			// Load the names and file offsets for the thumbnails in this package
			for( int32 CurThumbnailIndex = 0; CurThumbnailIndex < ThumbnailCount; ++CurThumbnailIndex )
			{
				bool bHaveValidClassName = false;
				FString ObjectClassName;
				*FileReader << ObjectClassName;

				// Object path
				FString ObjectPathWithoutPackageName;
				*FileReader << ObjectPathWithoutPackageName;

				FString ObjectPath;
				
				// handle UPackage thumbnails differently from usual assets
				if (ObjectClassName == UPackage::StaticClass()->GetName())
				{
					ObjectPath = ObjectPathWithoutPackageName;
				}
				else
				{
					ObjectPath = ( FPackageName::FilenameToLongPackageName(InPackageFileName) + TEXT( "." ) + ObjectPathWithoutPackageName );
				}
				
				// If the thumbnail was stored with a missing class name ("???") when we'll catch that here
				if( ObjectClassName.Len() > 0 && ObjectClassName != TEXT( "???" ) )
				{
					bHaveValidClassName = true;
				}
				else
				{
					// Class name isn't valid.  Probably legacy data.  We'll try to fix it up below.
				}


				if( !bHaveValidClassName )
				{
					// Try to figure out a class name based on input assets.  This should really only be needed
					// for packages saved by older versions of the editor (VER_CONTENT_BROWSER_FULL_NAMES)
					for ( TSet<FName>::TConstIterator It(InObjectFullNames); It; ++It )
					{
						const FName& CurObjectFullNameFName = *It;

						FString CurObjectFullName;
						CurObjectFullNameFName.ToString( CurObjectFullName );

						if( CurObjectFullName.EndsWith( ObjectPath ) )
						{
							// Great, we found a path that matches -- we just need to add that class name
							const int32 FirstSpaceIndex = CurObjectFullName.Find( TEXT( " " ) );
							check( FirstSpaceIndex != -1 );
							ObjectClassName = CurObjectFullName.Left( FirstSpaceIndex );
							
							// We have a useful class name now!
							bHaveValidClassName = true;
							break;
						}
					}
				}


				// File offset to image data
				int32 FileOffset = 0;
				*FileReader << FileOffset;

				if ( FileOffset != -1 && FileOffset < LastFileOffset )
				{
					UE_LOG(LogObjectTools, Warning, TEXT("Loaded thumbnail '%s' out of order!: FileOffset:%i    LastFileOffset:%i"), *ObjectPath, FileOffset, LastFileOffset);
				}


				if( bHaveValidClassName )
				{
					// Create a full name string with the object's class and fully qualified path
					const FString ObjectFullName( ObjectClassName + TEXT( " " ) + ObjectPath );


					// Add to our map
					ObjectNameToFileOffsetMap.Add( FName( *ObjectFullName ), FileOffset );
				}
				else
				{
					// Oh well, we weren't able to fix the class name up.  We won't bother making this
					// thumbnail available to load
				}
			}
		}


		// @todo CB: Should sort the thumbnails to load by file offset to reduce seeks [reviewed; pre-qa release]
		for ( TSet<FName>::TConstIterator It(InObjectFullNames); It; ++It )
		{
			const FName& CurObjectFullName = *It;

			// Do we have this thumbnail in the file?
			// @todo thumbnails: Backwards compat
			const int32* pFileOffset = ObjectNameToFileOffsetMap.Find(CurObjectFullName);
			if ( pFileOffset != NULL )
			{
				// Seek to the location in the file with the image data
				FileReader->Seek( *pFileOffset );

				// Load the image data
				FObjectThumbnail LoadedThumbnail;
				LoadedThumbnail.Serialize( *FileReader );

				// Store the data!
				InOutThumbnails.Add( CurObjectFullName, LoadedThumbnail );
			}
			else
			{
				// Couldn't find the requested thumbnail in the file!
			}		
		}


		return true;
	}



	/** Loads thumbnails from a package unless they're already cached in that package's thumbnail map */
	bool ConditionallyLoadThumbnailsFromPackage( const FString& InPackageFileName, const TSet< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails )
	{
		// First check to see if any of the requested thumbnails are already in memory
		TSet< FName > ObjectFullNamesToLoad;
		ObjectFullNamesToLoad.Empty(InObjectFullNames.Num());
		for ( TSet<FName>::TConstIterator It(InObjectFullNames); It; ++It )
		{
			const FName& CurObjectFullName = *It;

			// Do we have this thumbnail in our cache already?
			// @todo thumbnails: Backwards compat
			const FObjectThumbnail* FoundThumbnail = FindCachedThumbnailInPackage( InPackageFileName, CurObjectFullName );
			if( FoundThumbnail != NULL )
			{
				// Great, we already have this thumbnail in memory!  Copy it to our output map.
				InOutThumbnails.Add( CurObjectFullName, *FoundThumbnail );
			}
			else
			{
				ObjectFullNamesToLoad.Add(CurObjectFullName);
			}
		}


		// Did we find all of the requested thumbnails in our cache?
		if( ObjectFullNamesToLoad.Num() == 0 )
		{
			// Done!
			return true;
		}

		// OK, go ahead and load the remaining thumbnails!
		return LoadThumbnailsFromPackage( InPackageFileName, ObjectFullNamesToLoad, InOutThumbnails );
	}



	/** Loads thumbnails for the specified objects (or copies them from a cache, if they're already loaded.) */
	bool ConditionallyLoadThumbnailsForObjects( const TArray< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails )
	{
		// Create a list of unique package file names that we'll need to interrogate
		struct FObjectFullNamesForPackage
		{
			TSet< FName > ObjectFullNames;
		};

		typedef TMap< FString, FObjectFullNamesForPackage > PackageFileNameToObjectPathsMap;
		PackageFileNameToObjectPathsMap PackagesToProcess;
		for( int32 CurObjectIndex = 0; CurObjectIndex < InObjectFullNames.Num(); ++CurObjectIndex )
		{
			const FName ObjectFullName = InObjectFullNames[ CurObjectIndex ];


			// Determine the package file path/name for the specified object
			FString PackageFilePathName;
			if( !QueryPackageFileNameForObject( ObjectFullName.ToString(), PackageFilePathName ) )
			{
				// Couldn't find the package in our cache
				return false;
			}


			// Do we know about this package yet?
			FObjectFullNamesForPackage* ObjectFullNamesForPackage = PackagesToProcess.Find( PackageFilePathName );
			if( ObjectFullNamesForPackage == NULL )
			{
				ObjectFullNamesForPackage = &PackagesToProcess.Add( PackageFilePathName, FObjectFullNamesForPackage() );
			}

			if ( ObjectFullNamesForPackage->ObjectFullNames.Find(ObjectFullName) == NULL )
			{
				ObjectFullNamesForPackage->ObjectFullNames.Add(ObjectFullName);
			}
		}


		// Load thumbnails, one package at a time
		for( PackageFileNameToObjectPathsMap::TConstIterator PackageIt( PackagesToProcess ); PackageIt; ++PackageIt )
		{
			const FString& CurPackageFileName = PackageIt.Key();
			const FObjectFullNamesForPackage& CurPackageObjectPaths = PackageIt.Value();
			
			if( !ConditionallyLoadThumbnailsFromPackage( CurPackageFileName, CurPackageObjectPaths.ObjectFullNames, InOutThumbnails ) )
			{
				// Failed to load thumbnail data
				return false;
			}
		}


		return true;
	}

	UNREALED_API bool AssetHasCustomThumbnail(const FAssetData& InAssetData)
	{
		const FObjectThumbnail* CachedThumbnail = FindCachedThumbnail(InAssetData.GetFullName());
		if (CachedThumbnail != NULL && !CachedThumbnail->IsEmpty())
		{
			return true;
		}

		// If we don't yet have a thumbnail map, check the disk
		FName ObjectFullName = FName(*InAssetData.GetFullName());
		TArray<FName> ObjectFullNames;
		FThumbnailMap LoadedThumbnails;
		ObjectFullNames.Add(ObjectFullName);
		if (ConditionallyLoadThumbnailsForObjects(ObjectFullNames, LoadedThumbnails))
		{
			const FObjectThumbnail* Thumbnail = LoadedThumbnails.Find(ObjectFullName);

			if (Thumbnail != NULL && !Thumbnail->IsEmpty())
			{
				return true;
			}
		}
		return false;
	}
}

