// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "PackageTools.h"
#include "PackageReload.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectHash.h"
#include "Serialization/ArchiveFindCulprit.h"
#include "Misc/PackageName.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "Editor.h"
#include "Dialogs/Dialogs.h"
#include "Toolkits/AssetEditorManager.h"

#include "ObjectTools.h"
#include "KismetEditorUtilities.h"
#include "BusyCursor.h"

#include "FileHelpers.h"

#include "AssetRegistryModule.h"
#include "Logging/MessageLog.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "Engine/Selection.h"

#include "ShaderCompiler.h"
#include "DistanceFieldAtlas.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "PackageTools"

DEFINE_LOG_CATEGORY_STATIC(LogPackageTools, Log, All);

namespace PackageTools
{
	/** State passed to RestoreStandaloneOnReachableObjects. */
	static UPackage* PackageBeingUnloaded = nullptr;
	static TMap<UObject*,UObject*> ObjectsThatHadFlagsCleared;
	static FDelegateHandle ReachabilityCallbackHandle;

	/**
	 * Called during GC, after reachability analysis is performed but before garbage is purged.
	 * Restores RF_Standalone to objects in the package-to-be-unloaded that are still reachable.
	 */
	void RestoreStandaloneOnReachableObjects()
	{
		check(GIsEditor);

		ForEachObjectWithOuter(PackageBeingUnloaded, [](UObject* Object)
			{
				if ( ObjectsThatHadFlagsCleared.Find(Object) )
				{
					Object->SetFlags(RF_Standalone);
				}
			},
			true, RF_NoFlags, EInternalObjectFlags::Unreachable);
	}

	/**
	 * Filters the global set of packages.
	 *
	 * @param	OutGroupPackages			The map that receives the filtered list of group packages.
	 * @param	OutPackageList				The array that will contain the list of filtered packages.
	 */
	void GetFilteredPackageList(TSet<UPackage*>& OutFilteredPackageMap)
	{
		// The UObject list is iterated rather than the UPackage list because we need to be sure we are only adding
		// group packages that contain things the generic browser cares about.  The packages are derived by walking
		// the outer chain of each object.

		// Assemble a list of packages.  Only show packages that match the current resource type filter.
		for (UObject* Obj : TObjectRange<UObject>())
		{
			// This is here to hopefully catch a bit more info about a spurious in-the-wild problem which ultimately
			// crashes inside UObjectBaseUtility::GetOutermost(), which is called inside IsObjectBrowsable().
			checkf(Obj->IsValidLowLevel(), TEXT("GetFilteredPackageList: bad object found, address: %p, name: %s"), Obj, *Obj->GetName());

			// Make sure that we support displaying this object type
			bool bIsSupported = ObjectTools::IsObjectBrowsable( Obj );
			if( bIsSupported )
			{
				UPackage* ObjectPackage = Obj->GetOutermost();
				if( ObjectPackage != NULL )
				{
					OutFilteredPackageMap.Add( ObjectPackage );
				}
			}
		}
	}

	/**
	 * Fills the OutObjects list with all valid objects that are supported by the current
	 * browser settings and that reside withing the set of specified packages.
	 *
	 * @param	InPackages			Filters objects based on package.
	 * @param	OutObjects			[out] Receives the list of objects
	 * @param	bMustBeBrowsable	If specified, does a check to see if object is browsable. Defaults to true.
	 */
	void GetObjectsInPackages( const TArray<UPackage*>* InPackages, TArray<UObject*>& OutObjects )
	{
		if (InPackages)
		{
			for (UPackage* Package : *InPackages)
			{
				ForEachObjectWithOuter(Package,[&OutObjects](UObject* Obj)
					{
						if (ObjectTools::IsObjectBrowsable(Obj))
						{
							OutObjects.Add(Obj);
						}
					});
			}
		}
		else
		{
			for (TObjectIterator<UObject> It; It; ++It)
			{
				UObject* Obj = *It;

				if (ObjectTools::IsObjectBrowsable(Obj))
				{
					OutObjects.Add(Obj);
				}
			}
		}
	}

	bool HandleFullyLoadingPackages( const TArray<UPackage*>& TopLevelPackages, const FText& OperationText )
	{
		bool bSuccessfullyCompleted = true;

		// whether or not to suppress the ask to fully load message
		bool bSuppress = GetDefault<UEditorPerProjectUserSettings>()->bSuppressFullyLoadPrompt;

		// Make sure they are all fully loaded.
		bool bNeedsUpdate = false;
		for( int32 PackageIndex=0; PackageIndex<TopLevelPackages.Num(); PackageIndex++ )
		{
			UPackage* TopLevelPackage = TopLevelPackages[PackageIndex];
			check( TopLevelPackage );
			check( TopLevelPackage->GetOuter() == NULL );

			if( !TopLevelPackage->IsFullyLoaded() )
			{	
				// Ask user to fully load or suppress the message and just fully load
				if(bSuppress || EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, FText::Format(
					NSLOCTEXT("UnrealEd", "NeedsToFullyLoadPackageF", "Package {0} is not fully loaded. Do you want to fully load it? Not doing so will abort the '{1}' operation."),
					FText::FromString(TopLevelPackage->GetName()), OperationText ) ) )
				{
					// Fully load package.
					const FScopedBusyCursor BusyCursor;
					GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "FullyLoadingPackages", "Fully loading packages"), true );
					TopLevelPackage->FullyLoad();
					GWarn->EndSlowTask();
					bNeedsUpdate = true;
				}
				// User declined abort operation.
				else
				{
					bSuccessfullyCompleted = false;
					UE_LOG(LogPackageTools, Log, TEXT("Aborting operation as %s was not fully loaded."),*TopLevelPackage->GetName());
					break;
				}
			}
		}

		// no need to refresh content browser here as UPackage::FullyLoad() already does this
		return bSuccessfullyCompleted;
	}
	
	/**
	 * Loads the specified package file (or returns an existing package if it's already loaded.)
	 *
	 * @param	InFilename	File name of package to load
	 *
	 * @return	The loaded package (or NULL if something went wrong.)
	 */
	UPackage* LoadPackage( FString InFilename )
	{
		// Detach all components while loading a package.
		// This is necessary for the cases where the load replaces existing objects which may be referenced by the attached components.
		FGlobalComponentReregisterContext ReregisterContext;

		// record the name of this file to make sure we load objects in this package on top of in-memory objects in this package
		GEditor->UserOpenedFile = InFilename;

		// clear any previous load errors
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("PackageName"), FText::FromString(InFilename));
		FMessageLog("LoadErrors").NewPage(FText::Format(LOCTEXT("LoadPackageLogPage", "Loading package: {PackageName}"), Arguments));

		UPackage* Package = ::LoadPackage( NULL, *InFilename, 0 );

		// display any load errors that happened while loading the package
		FEditorDelegates::DisplayLoadErrors.Broadcast();

		// reset the opened package to nothing
		GEditor->UserOpenedFile = FString();

		// If a script package was loaded, update the
		// actor browser in case a script package was loaded
		if ( Package != NULL )
		{
			if (Package->HasAnyPackageFlags(PKG_ContainsScript))
			{
				GEditor->BroadcastClassPackageLoadedOrUnloaded();
			}
		}

		return Package;
	}


	bool UnloadPackages( const TArray<UPackage*>& TopLevelPackages )
	{
		FText ErrorMessage;
		bool bResult = UnloadPackages(TopLevelPackages, ErrorMessage);
		if(!ErrorMessage.IsEmpty())
		{
			FMessageDialog::Open( EAppMsgType::Ok, ErrorMessage );
		}

		return bResult;
	}


	bool UnloadPackages( const TArray<UPackage*>& TopLevelPackages, FText& OutErrorMessage )
	{
		bool bResult = false;

		// Get outermost packages, in case groups were selected.
		TArray<UPackage*> PackagesToUnload;

		// Split the set of selected top level packages into packages which are dirty (and thus cannot be unloaded)
		// and packages that are not dirty (and thus can be unloaded).
		TArray<UPackage*> DirtyPackages;
		for ( int32 PackageIndex = 0 ; PackageIndex < TopLevelPackages.Num() ; ++PackageIndex )
		{
			UPackage* Package = TopLevelPackages[PackageIndex];
			if( Package != NULL )
			{
				if ( Package->IsDirty() )
				{
					DirtyPackages.Add( Package );
				}
				else
				{
					PackagesToUnload.AddUnique( Package->GetOutermost() ? Package->GetOutermost() : Package );
				}
			}
		}

		// Inform the user that dirty packages won't be unloaded.
		if ( DirtyPackages.Num() > 0 )
		{
			FString DirtyPackagesList;
			for ( int32 PackageIndex = 0 ; PackageIndex < DirtyPackages.Num() ; ++PackageIndex )
			{
				DirtyPackagesList += FString::Printf( TEXT("\n    %s"), *DirtyPackages[PackageIndex]->GetName() );
			}

			FFormatNamedArguments Args;
			Args.Add( TEXT("DirtyPackages"),FText::FromString( DirtyPackagesList ) );

			OutErrorMessage = FText::Format( NSLOCTEXT("UnrealEd", "UnloadDirtyPackagesList", "The following assets have been modified and cannot be unloaded:{DirtyPackages}\nSaving these assets will allow them to be unloaded."), Args );
		}

		if ( PackagesToUnload.Num() > 0 )
		{
			const FScopedBusyCursor BusyCursor;

			// Complete any load/streaming requests, then lock IO.
			FlushAsyncLoading();
			(*GFlushStreamingFunc)();

			// Remove potential references to to-be deleted objects from the GB selection set.
			GEditor->GetSelectedObjects()->DeselectAll();

			// Set the callback for restoring RF_Standalone post reachability analysis.
			// GC will call this function before purging objects, allowing us to restore RF_Standalone
			// to any objects that have not been marked RF_Unreachable.
			ReachabilityCallbackHandle = FCoreUObjectDelegates::PostReachabilityAnalysis.AddStatic(RestoreStandaloneOnReachableObjects);

			bool bScriptPackageWasUnloaded = false;

			GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "Unloading", "Unloading"), true );

			// First add all packages to unload to the root set so they don't get garbage collected while we are operating on them
			TArray<UPackage*> PackagesAddedToRoot;
			for ( int32 PackageIndex = 0 ; PackageIndex < PackagesToUnload.Num() ; ++PackageIndex )
			{
				UPackage* Pkg = PackagesToUnload[PackageIndex];
				if ( !Pkg->IsRooted() )
				{
					Pkg->AddToRoot();
					PackagesAddedToRoot.Add(Pkg);
				}
			}

			// Now try to clean up assets in all packages to unload.
			for ( int32 PackageIndex = 0 ; PackageIndex < PackagesToUnload.Num() ; ++PackageIndex )
			{
				PackageBeingUnloaded = PackagesToUnload[PackageIndex];

				GWarn->StatusUpdate( PackageIndex, PackagesToUnload.Num(), FText::Format(NSLOCTEXT("UnrealEd", "Unloadingf", "Unloading {0}..."), FText::FromString(PackageBeingUnloaded->GetName()) ) );

				// Flush all pending render commands, as unloading the package may invalidate render resources.
				FlushRenderingCommands();

				// Close any open asset editors
				ForEachObjectWithOuter(PackageBeingUnloaded, [](UObject* Obj)
				{
					if (Obj->IsAsset())
					{
						FAssetEditorManager::Get().CloseAllEditorsForAsset(Obj);
					}
				}, false);

				PackageBeingUnloaded->bHasBeenFullyLoaded = false;
				PackageBeingUnloaded->ClearFlags(RF_WasLoaded);
				if ( PackageBeingUnloaded->HasAnyPackageFlags(PKG_ContainsScript) )
				{
					bScriptPackageWasUnloaded = true;
				}

				// Clear RF_Standalone flag from objects in the package to be unloaded so they get GC'd.
				{
					TArray<UObject*> ObjectsInPackage;
					GetObjectsWithOuter(PackageBeingUnloaded, ObjectsInPackage);
					for ( UObject* Object : ObjectsInPackage )
					{
						if (Object->HasAnyFlags(RF_Standalone))
						{
							Object->ClearFlags(RF_Standalone);
							ObjectsThatHadFlagsCleared.Add(Object, Object);
						}
					}
				}

				// Reset loaders
				ResetLoaders(PackageBeingUnloaded);

				// Collect garbage.
				CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
				
				if( PackageBeingUnloaded->IsDirty() )
				{
					// The package was marked dirty as a result of something that happened above (e.g callbacks in CollectGarbage).  
					// Dirty packages we actually care about unloading were filtered above so if the package becomes dirty here it should still be unloaded
					PackageBeingUnloaded->SetDirtyFlag(false);
				}

				// Cleanup.
				ObjectsThatHadFlagsCleared.Empty();
				PackageBeingUnloaded = NULL;
				bResult = true;
			}
			
			// Now remove from root all the packages we added earlier so they may be GCed if possible
			for ( int32 PackageIndex = 0 ; PackageIndex < PackagesAddedToRoot.Num() ; ++PackageIndex )
			{
				PackagesAddedToRoot[PackageIndex]->RemoveFromRoot();
			}
			PackagesAddedToRoot.Empty();

			GWarn->EndSlowTask();

			// Remove the post reachability callback.
			FCoreUObjectDelegates::PostReachabilityAnalysis.Remove(ReachabilityCallbackHandle);

			// Clear the standalone flag on metadata objects that are going to be GC'd below.
			// This resolves the circular dependency between metadata and packages.
			TArray<TWeakObjectPtr<UMetaData>> PackageMetaDataWithClearedStandaloneFlag;
			for ( UPackage* PackageToUnload : PackagesToUnload )
			{
				UMetaData* PackageMetaData = PackageToUnload ? PackageToUnload->MetaData : nullptr;
				if ( PackageMetaData && PackageMetaData->HasAnyFlags(RF_Standalone) )
				{
					PackageMetaData->ClearFlags(RF_Standalone);
					PackageMetaDataWithClearedStandaloneFlag.Add(PackageMetaData);
				}
			}

			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

			// Restore the standalone flag on any metadata objects that survived the GC
			for ( const TWeakObjectPtr<UMetaData>& WeakPackageMetaData : PackageMetaDataWithClearedStandaloneFlag )
			{
				UMetaData* MetaData = WeakPackageMetaData.Get();
				if ( MetaData )
				{
					MetaData->SetFlags(RF_Standalone);
				}
			}

			// Update the actor browser if a script package was unloaded
			if ( bScriptPackageWasUnloaded )
			{
				GEditor->BroadcastClassPackageLoadedOrUnloaded();
			}
		}
		return bResult;
	}


	bool ReloadPackages( const TArray<UPackage*>& TopLevelPackages )
	{
		FText ErrorMessage;
		const bool bResult = ReloadPackages(TopLevelPackages, ErrorMessage, /*bInteractive*/true);
		
		if (!ErrorMessage.IsEmpty())
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		}

		return bResult;
	}


	bool ReloadPackages( const TArray<UPackage*>& TopLevelPackages, FText& OutErrorMessage, const bool bInteractive )
	{
		bool bResult = false;

		FTextBuilder ErrorMessageBuilder;

		// Split the set of selected top level packages into packages which are dirty or in-memory (and thus cannot be reloaded) and packages that are not dirty (and thus can be reloaded).
		TArray<UPackage*> PackagesToReload;
		{
			TArray<UPackage*> DirtyPackages;
			TArray<UPackage*> InMemoryPackages;
			for (UPackage* TopLevelPackage : TopLevelPackages)
			{
				if (TopLevelPackage)
				{
					// Get outermost packages, in case groups were selected.
					UPackage* RealPackage = TopLevelPackage->GetOutermost() ? TopLevelPackage->GetOutermost() : TopLevelPackage;

					if (RealPackage->IsDirty())
					{
						DirtyPackages.AddUnique(RealPackage);
					}
					else if (RealPackage->HasAnyPackageFlags(PKG_InMemoryOnly))
					{
						InMemoryPackages.AddUnique(RealPackage);
					}
					else
					{
						PackagesToReload.AddUnique(RealPackage);
					}
				}

				// Ask the user whether dirty packages should be reloaded.
				if (bInteractive && DirtyPackages.Num() > 0)
				{
					FTextBuilder ReloadDirtyPackagesMsgBuilder;
					ReloadDirtyPackagesMsgBuilder.AppendLine(NSLOCTEXT("UnrealEd", "ShouldReloadDirtyPackagesHeader", "The following packages have been modified:"));
					{
						ReloadDirtyPackagesMsgBuilder.Indent();
						for (UPackage* DirtyPackage : DirtyPackages)
						{
							ReloadDirtyPackagesMsgBuilder.AppendLine(DirtyPackage->GetFName());
						}
						ReloadDirtyPackagesMsgBuilder.Unindent();
					}
					ReloadDirtyPackagesMsgBuilder.AppendLine(NSLOCTEXT("UnrealEd", "ShouldReloadDirtyPackagesFooter", "Would you like to reload these packages? This will revert any changes you have made."));

					if (FMessageDialog::Open(EAppMsgType::YesNo, ReloadDirtyPackagesMsgBuilder.ToText()) == EAppReturnType::Yes)
					{
						for (UPackage* DirtyPackage : DirtyPackages)
						{
							DirtyPackage->SetDirtyFlag(false);
							PackagesToReload.AddUnique(DirtyPackage);
						}
						DirtyPackages.Reset();
					}
				}
			}

			// Inform the user that dirty packages won't be reloaded.
			if (DirtyPackages.Num() > 0)
			{
				if (!ErrorMessageBuilder.IsEmpty())
				{
					ErrorMessageBuilder.AppendLine();
				}

				ErrorMessageBuilder.AppendLine(NSLOCTEXT("UnrealEd", "Error_ReloadDirtyPackagesHeader", "The following packages have been modified and cannot be reloaded:"));
				{
					ErrorMessageBuilder.Indent();
					for (UPackage* DirtyPackage : DirtyPackages)
					{
						ErrorMessageBuilder.AppendLine(DirtyPackage->GetFName());
					}
					ErrorMessageBuilder.Unindent();
				}
				ErrorMessageBuilder.AppendLine(NSLOCTEXT("UnrealEd", "Error_ReloadDirtyPackagesFooter", "Saving these packages will allow them to be reloaded."));
			}

			// Inform the user that in-memory packages won't be reloaded.
			if (InMemoryPackages.Num() > 0)
			{
				if (!ErrorMessageBuilder.IsEmpty())
				{
					ErrorMessageBuilder.AppendLine();
				}

				ErrorMessageBuilder.AppendLine(NSLOCTEXT("UnrealEd", "Error_ReloadInMemoryPackagesHeader", "The following packages are in-memory only and cannot be reloaded:"));
				{
					ErrorMessageBuilder.Indent();
					for (UPackage* InMemoryPackage : InMemoryPackages)
					{
						ErrorMessageBuilder.AppendLine(InMemoryPackage->GetFName());
					}
					ErrorMessageBuilder.Unindent();
				}
			}
		}

		// Check to see if we need to reload the current world.
		FName WorldNameToReload;
		PackagesToReload.RemoveAll([&](UPackage* PackageToReload) -> bool
		{
			// Is this the currently loaded world? If so, we just reset the current world and load it again at the end rather than let it go 
			// through ReloadPackage (which doesn't work for the editor due to some assumptions it makes about worlds, and their lifetimes).
			if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
			{
				const bool bIsCurrentWorld = EditorWorld->GetOutermost() == PackageToReload;
				if (bIsCurrentWorld)
				{
					WorldNameToReload = *EditorWorld->GetPathName();
					return true; // remove world package
				}
			}
			return false; // leave non-world package
		});

		// Unload the current world (if needed).
		TMap<FName, const UMapBuildDataRegistry*> LevelsToMapBuildData;
		if (!WorldNameToReload.IsNone())
		{
			GEditor->CreateNewMapForEditing();
		}
		// Cache the current map build data for the levels of the current world so we can see if they change due to a reload (we skip this if reloading the current world).
		else if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
		{
			for (int32 LevelIndex = 0; LevelIndex < EditorWorld->GetNumLevels(); ++LevelIndex)
			{
				ULevel* Level = EditorWorld->GetLevel(LevelIndex);
				LevelsToMapBuildData.Add(Level->GetFName(), Level->MapBuildData);
			}
		}

		if (PackagesToReload.Num() > 0)
		{
			const FScopedBusyCursor BusyCursor;

			// We need to sort the packages to reload so that dependencies are reloaded before the assets that depend on them
			::SortPackagesForReload(PackagesToReload);

			// Remove potential references to to-be deleted objects from the global selection set.
			GEditor->GetSelectedObjects()->DeselectAll();

			// Detach all components while loading a package.
			// This is necessary for the cases where the load replaces existing objects which may be referenced by the attached components.
			FGlobalComponentReregisterContext ReregisterContext;

			bool bScriptPackageWasReloaded = false;
			TArray<FReloadPackageData> PackagesToReloadData;
			PackagesToReloadData.Reserve(PackagesToReload.Num());
			for (UPackage* PackageToReload : PackagesToReload)
			{
				check(PackageToReload);
				bScriptPackageWasReloaded |= PackageToReload->HasAnyPackageFlags(PKG_ContainsScript);
				PackagesToReloadData.Emplace(PackageToReload, LOAD_None);
			}

			TArray<UPackage*> ReloadedPackages;
			::ReloadPackages(PackagesToReloadData, ReloadedPackages, 500);

			TArray<UPackage*> FailedPackages;
			for (int32 PackageIndex = 0; PackageIndex < PackagesToReload.Num(); ++PackageIndex)
			{
				UPackage* ExistingPackage = PackagesToReload[PackageIndex];
				UPackage* ReloadedPackage = ReloadedPackages[PackageIndex];

				if (ReloadedPackage)
				{
					bScriptPackageWasReloaded |= ReloadedPackage->HasAnyPackageFlags(PKG_ContainsScript);
					bResult = true;
				}
				else
				{
					FailedPackages.Add(ExistingPackage);
				}
			}

			// Inform the user of any packages that failed to reload.
			if (FailedPackages.Num() > 0)
			{
				if (!ErrorMessageBuilder.IsEmpty())
				{
					ErrorMessageBuilder.AppendLine();
				}

				ErrorMessageBuilder.AppendLine(NSLOCTEXT("UnrealEd", "Error_ReloadFailedPackagesHeader", "The following packages failed to reload:"));
				{
					ErrorMessageBuilder.Indent();
					for (UPackage* FailedPackage : FailedPackages)
					{
						ErrorMessageBuilder.AppendLine(FailedPackage->GetFName());
					}
					ErrorMessageBuilder.Unindent();
				}
			}

			// Update the actor browser if a script package was reloaded.
			if (bScriptPackageWasReloaded)
			{
				GEditor->BroadcastClassPackageLoadedOrUnloaded();
			}
		}

		// Load the previous world (if needed).
		if (!WorldNameToReload.IsNone())
		{
			TArray<FName> WorldNamesToReload;
			WorldNamesToReload.Add(WorldNameToReload);
			FAssetEditorManager::Get().OpenEditorsForAssets(WorldNamesToReload);
		}
		// Update the rendering resources for the levels of the current world if their map build data has changed (we skip this if reloading the current world).
		else if (LevelsToMapBuildData.Num() > 0)
		{
			UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
			check(EditorWorld);

			for (int32 LevelIndex = 0; LevelIndex < EditorWorld->GetNumLevels(); ++LevelIndex)
			{
				ULevel* Level = EditorWorld->GetLevel(LevelIndex);
				const UMapBuildDataRegistry* OldMapBuildData = LevelsToMapBuildData.FindRef(Level->GetFName());

				if (OldMapBuildData && OldMapBuildData != Level->MapBuildData)
				{
					Level->ReleaseRenderingResources();
					Level->InitializeRenderingResources();
				}
			}
		}

		OutErrorMessage = ErrorMessageBuilder.ToText();

		return bResult;
	}


	/**
	 * Wrapper method for multiple objects at once.
	 *
	 * @param	TopLevelPackages		the packages to be export
	 * @param	LastExportPath			the path that the user last exported assets to
	 * @param	FilteredClasses			if specified, set of classes that should be the only types exported if not exporting to single file
	 * @param	bUseProvidedExportPath	If true, use LastExportPath as the user's export path w/o prompting for a directory, where applicable
	 *
	 * @return	the path that the user chose for the export.
	 */
	FString DoBulkExport(const TArray<UPackage*>& TopLevelPackages, FString LastExportPath, const TSet<UClass*>* FilteredClasses /* = NULL */, bool bUseProvidedExportPath/* = false*/ )
	{
		// Disallow export if any packages are cooked.
		if (HandleFullyLoadingPackages( TopLevelPackages, NSLOCTEXT("UnrealEd", "BulkExportE", "Bulk Export...") ) )
		{
			TArray<UObject*> ObjectsInPackages;
			GetObjectsInPackages(&TopLevelPackages, ObjectsInPackages);

			// See if any filtering has been requested. Objects can be filtered by class and/or localization filter.
			TArray<UObject*> FilteredObjects;
			if ( FilteredClasses )
			{
				// Present the user with a warning that only the filtered types are being exported
				FSuppressableWarningDialog::FSetupInfo Info( NSLOCTEXT("UnrealEd", "BulkExport_FilteredWarning", "Asset types are currently filtered within the Content Browser. Only objects of the filtered types will be exported."),
					LOCTEXT("BulkExport_FilteredWarning_Title", "Asset Filter in Effect"), "BulkExportFilterWarning" );
				Info.ConfirmText = NSLOCTEXT("ModalDialogs", "BulkExport_FilteredWarningConfirm", "Close");

				FSuppressableWarningDialog PromptAboutFiltering( Info );
				PromptAboutFiltering.ShowModal();
				
				for ( TArray<UObject*>::TConstIterator ObjIter(ObjectsInPackages); ObjIter; ++ObjIter )
				{
					UObject* CurObj = *ObjIter;

					// Only add the object if it passes all of the specified filters
					if ( CurObj && FilteredClasses->Contains( CurObj->GetClass() ) )
					{
						FilteredObjects.Add( CurObj );
					}
				}
			}

			// If a filtered set was provided, export the filtered objects array; otherwise, export all objects in the packages
			TArray<UObject*>& ObjectsToExport = FilteredClasses ? FilteredObjects : ObjectsInPackages;

			// Prompt the user about how many objects will be exported before proceeding.
			const bool bProceed = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, FText::Format(
				NSLOCTEXT("UnrealEd", "Prompt_AboutToBulkExportNItems_F", "About to bulk export {0} items.  Proceed?"), FText::AsNumber(ObjectsToExport.Num()) ) );
			if ( bProceed )
			{
				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

				AssetToolsModule.Get().ExportAssets(ObjectsToExport, LastExportPath);
			}
		}

		return LastExportPath;
	}

	void CheckOutRootPackages( const TArray<UPackage*>& Packages )
	{
		if (ISourceControlModule::Get().IsEnabled())
		{
			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

			// Update to the latest source control state.
			SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), Packages);

			TArray<FString> TouchedPackageNames;
			bool bCheckedSomethingOut = false;
			for( int32 PackageIndex = 0 ; PackageIndex < Packages.Num() ; ++PackageIndex )
			{
				UPackage* Package = Packages[PackageIndex];
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Package, EStateCacheUsage::Use);
				if( SourceControlState.IsValid() && SourceControlState->CanCheckout() )
				{
					// The package is still available, so do the check out.
					bCheckedSomethingOut = true;
					TouchedPackageNames.Add(Package->GetName());
				}
				else
				{
					// The status on the package has changed to something inaccessible, so we have to disallow the check out.
					// Don't warn if the file isn't in the depot.
					if (SourceControlState.IsValid() && SourceControlState->IsSourceControlled())
					{			
						FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_PackageStatusChanged", "Package can't be checked out - status has changed!") );
					}
				}
			}

			// Synchronize source control state if something was checked out.
			SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), SourceControlHelpers::PackageFilenames(TouchedPackageNames));
		}
	}

	/**
	 * Checks if the passed in path is in an external directory. I.E Ones not found automatically in the content directory
	 *
	 * @param	PackagePath	Path of the package to check, relative or absolute
	 * @return	true if PackagePath points to an external location
	 */
	bool IsPackagePathExternal( const FString& PackagePath )
	{
		bool bIsExternal = true;
		TArray< FString > Paths;
		GConfig->GetArray( TEXT("Core.System"), TEXT("Paths"), Paths, GEngineIni );
	
		FString PackageFilename = FPaths::ConvertRelativePathToFull(PackagePath);

		// absolute path of the package that was passed in, without the actual name of the package
		FString PackageFullPath = FPaths::GetPath(PackageFilename);

		for(int32 pathIdx = 0; pathIdx < Paths.Num(); ++pathIdx)
		{ 
			FString AbsolutePathName = FPaths::ConvertRelativePathToFull(Paths[ pathIdx ]);

			// check if the package path is within the list of paths the engine searches.
			if( PackageFullPath.Contains( AbsolutePathName ) )
			{
				bIsExternal = false;
				break;
			}
		}

		return bIsExternal;
	}

	/**
	 * Checks if the passed in package's filename is in an external directory. I.E Ones not found automatically in the content directory
	 *
	 * @param	Package	The package to check
	 * @return	true if the package points to an external filename
	 */
	bool IsPackageExternal(const UPackage& Package)
	{
		FString FileString;
		FPackageName::DoesPackageExist(Package.GetName(), NULL, &FileString);

		return IsPackagePathExternal( FileString );
	}
	/**
	 * Checks if the passed in packages have any references to  externally loaded packages.  I.E Ones not found automatically in the content directory
	 *
	 * @param	PackagesToCheck					The packages to check
	 * @param	OutPackagesWithExternalRefs		Optional list of packages that have external references
	 * @param	LevelToCheck					The ULevel to check
	 * @param	OutObjectsWithExternalRefs		List of objects gathered from within the given ULevel that have external references
	 * @return	true if PackageToCheck has references to an externally loaded package
	 */
	bool CheckForReferencesToExternalPackages(const TArray<UPackage*>* PackagesToCheck, TArray<UPackage*>* OutPackagesWithExternalRefs, ULevel* LevelToCheck/*=NULL*/, TArray<UObject*>* OutObjectsWithExternalRefs/*=NULL*/ )
	{
		bool bHasExternalPackageRefs = false;

		// Find all external packages
		TSet<UPackage*> FilteredPackageMap;
		GetFilteredPackageList(FilteredPackageMap);

		TArray< UPackage* > ExternalPackages;
		ExternalPackages.Reserve(FilteredPackageMap.Num());
		for (UPackage* Pkg : FilteredPackageMap)
		{
			FString OutFilename;
			const FString PackageName = Pkg->GetName();
			const FGuid PackageGuid = Pkg->GetGuid();

			FPackageName::DoesPackageExist( PackageName, &PackageGuid, &OutFilename );

			if( OutFilename.Len() > 0 && IsPackageExternal( *Pkg ) )
			{
				ExternalPackages.Add(Pkg);
			}
		}

		// get all the objects in the external packages and make sure they aren't referenced by objects in a package being checked
		TArray< UObject* > ObjectsInExternalPackages;
		TArray< UObject* > ObjectsInPackageToCheck;
		
		if(PackagesToCheck)
		{
			GetObjectsInPackages( &ExternalPackages, ObjectsInExternalPackages );
			GetObjectsInPackages( PackagesToCheck, ObjectsInPackageToCheck );
		}
		else
		{
			GetObjectsWithOuter(LevelToCheck, ObjectsInPackageToCheck);

			// Gather all objects in any loaded external packages			
			for (const UPackage* Package : ExternalPackages)
			{
				ForEachObjectWithOuter(Package,
									   [&ObjectsInExternalPackages](UObject* Obj)
									   {
										   if (ObjectTools::IsObjectBrowsable(Obj))
										   {
											   ObjectsInExternalPackages.Add(Obj);
										   }
									   });
			}
		}

		// only check objects which are in packages to be saved.  This should greatly reduce the overhead by not searching through objects we don't intend to save
		for (UObject* CheckObject : ObjectsInPackageToCheck)
		{
			for (UObject* ExternalObject : ObjectsInExternalPackages)
			{
				FArchiveFindCulprit ArFind( ExternalObject, CheckObject, false );
				if( ArFind.GetCount() > 0 )
				{
					if( OutPackagesWithExternalRefs )
					{
						OutPackagesWithExternalRefs->Add( CheckObject->GetOutermost() );
					}
					if(OutObjectsWithExternalRefs)
					{
						OutObjectsWithExternalRefs->Add( CheckObject );
					}
					bHasExternalPackageRefs = true;
					break;
				}
			}
		}

		return bHasExternalPackageRefs;
	}

	bool SavePackagesForObjects(const TArray<UObject*>& ObjectsToSave)
	{
		// Retrieve all dirty packages for the objects 
		TArray<UPackage*> PackagesToSave;
		for (UObject* Object : ObjectsToSave)
		{
			if (Object->GetOutermost()->IsDirty())
			{
				PackagesToSave.AddUnique(Object->GetOutermost());
			}
		}

		TArray< UPackage* > PackagesWithExternalRefs;
		FString PackageNames;
		if (PackageTools::CheckForReferencesToExternalPackages(&PackagesToSave, &PackagesWithExternalRefs))
		{
			for (int32 PkgIdx = 0; PkgIdx < PackagesWithExternalRefs.Num(); ++PkgIdx)
			{
				PackageNames += FString::Printf(TEXT("%s\n"), *PackagesWithExternalRefs[PkgIdx]->GetName());
			}
			bool bProceed = EAppReturnType::Yes == FMessageDialog::Open(
				EAppMsgType::YesNo,
				FText::Format(
					NSLOCTEXT("UnrealEd", "Warning_ExternalPackageRef", "The following assets have references to external assets: \n{0}\nExternal assets won't be found when in a game and all references will be broken.  Proceed?"),
					FText::FromString(PackageNames)));
			if (!bProceed)
			{
				return false;
			}
		}

		const bool bCheckDirty = false;
		const bool bPromptToSave = false;
		const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave);
		
		return (PackagesToSave.Num() > 0) && Return == FEditorFileUtils::EPromptReturnCode::PR_Success;
	}

	bool IsSingleAssetPackage(const FString& PackageName)
	{
		FString PackageFileName;
		if ( FPackageName::DoesPackageExist(PackageName, NULL, &PackageFileName) )
		{
			return FPaths::GetExtension(PackageFileName, /*bIncludeDot=*/true) == FPackageName::GetAssetPackageExtension();
		}

		// If it wasn't found in the package file cache, this package does not yet
		// exist so it is assumed to be saved as a UAsset file.
		return true;
	}

	FString SanitizePackageName (const FString& InPackageName)
	{
		FString SanitizedName;
		FString InvalidChars = INVALID_LONGPACKAGE_CHARACTERS;

		// See if the name contains invalid characters.
		FString Char;
		for( int32 CharIdx = 0; CharIdx < InPackageName.Len(); ++CharIdx )
		{
			Char = InPackageName.Mid(CharIdx, 1);

			if ( InvalidChars.Contains(*Char) )
			{
				SanitizedName += TEXT("_");
			}
			else
			{
				SanitizedName += Char;
			}
		}

		// Remove double-slashes
		SanitizedName.ReplaceInline(TEXT("//"), TEXT("/"));

		return SanitizedName;
	}
}

#undef LOCTEXT_NAMESPACE

// EOF
