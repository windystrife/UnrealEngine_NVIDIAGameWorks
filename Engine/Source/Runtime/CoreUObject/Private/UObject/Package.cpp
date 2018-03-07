// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/Package.h"
#include "HAL/FileManager.h"
#include "Misc/ITransaction.h"
#include "UObject/MetaData.h"
#include "Misc/PackageName.h"
#include "UObject/LinkerLoad.h"
#include "UObject/LinkerManager.h"

/*-----------------------------------------------------------------------------
	UPackage.
-----------------------------------------------------------------------------*/

/** Delegate to notify subscribers when a package is about to be saved. */
UPackage::FPreSavePackage UPackage::PreSavePackageEvent;
/** Delegate to notify subscribers when a package has been saved. This is triggered when the package saving
 *  has completed and was successful. */
UPackage::FOnPackageSaved UPackage::PackageSavedEvent;
/** Delegate to notify subscribers when the dirty state of a package is changed.
 *  Allows the editor to register the modified package as one that should be prompted for source control checkout. 
 *  Use Package->IsDirty() to get the updated dirty state of the package */
UPackage::FOnPackageDirtyStateChanged UPackage::PackageDirtyStateChangedEvent;
/** 
 * Delegate to notify subscribers when a package is marked as dirty via UObjectBaseUtilty::MarkPackageDirty 
 * Note: Unlike FOnPackageDirtyStateChanged, this is always called, even when the package is already dirty
 * Use bWasDirty to check the previous dirty state of the package
 * Use Package->IsDirty() to get the updated dirty state of the package
 */
UPackage::FOnPackageMarkedDirty UPackage::PackageMarkedDirtyEvent;

void UPackage::PostInitProperties()
{
	Super::PostInitProperties();
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		bDirty = false;
	}

	MetaData = NULL;
	LinkerPackageVersion = GPackageFileUE4Version;
	LinkerLicenseeVersion = GPackageFileLicenseeUE4Version;
	PIEInstanceID = INDEX_NONE;
#if WITH_EDITORONLY_DATA
	bIsCookedForEditor = false;
	// Mark this package as editor-only by default. As soon as something in it is accessed through a non editor-only
	// property the flag will be removed.
	bLoadedByEditorPropertiesOnly = !HasAnyFlags(RF_ClassDefaultObject) && !HasAnyPackageFlags(PKG_CompiledIn) && (IsRunningCommandlet());
#endif
}


/**
 * Marks/Unmarks the package's bDirty flag
 */
void UPackage::SetDirtyFlag( bool bIsDirty )
{
	if ( GetOutermost() != GetTransientPackage() )
	{
		if ( GUndo != NULL
		// PIE world objects should never end up in the transaction buffer as we cannot undo during gameplay.
		&& !GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor|PKG_ContainsScript) )
		{
			// make sure we're marked as transactional
			SetFlags(RF_Transactional);

			// don't call Modify() since it calls SetDirtyFlag()
			GUndo->SaveObject( this );
		}

		// Update dirty bit
		bDirty = bIsDirty;

		if( GIsEditor									// Only fire the callback in editor mode
			&& !HasAnyPackageFlags(PKG_ContainsScript)	// Skip script packages
			&& !HasAnyPackageFlags(PKG_PlayInEditor)	// Skip packages for PIE
			&& GetTransientPackage() != this )			// Skip the transient package
		{
			// Package is changing dirty state, let the editor know so we may prompt for source control checkout
			PackageDirtyStateChangedEvent.Broadcast(this);
		}
	}
}

/**
 * Serializer
 * Save the value of bDirty into the transaction buffer, so that undo/redo will also mark/unmark the package as dirty, accordingly
 */
void UPackage::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if ( Ar.IsTransacting() )
	{
		Ar << bDirty;
	}
	if (Ar.IsCountingMemory())
	{		
		if (LinkerLoad)
		{
			FLinker* Loader = LinkerLoad;
			Loader->Serialize(Ar);
		}
	}
}

void UPackage::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UPackage* This = CastChecked<UPackage>(InThis);
#if WITH_EDITOR
	if( GIsEditor )
	{
		// Required by the unified GC when running in the editor
		Collector.AddReferencedObject(This->MetaData, This);
	}
#endif
	Super::AddReferencedObjects(This, Collector);
}

/**
 * Gets (after possibly creating) a metadata object for this package
 *
 * @return A valid UMetaData pointer for all objects in this package
 */
UMetaData* UPackage::GetMetaData()
{
	checkf(!FPlatformProperties::RequiresCookedData(), TEXT("MetaData is only allowed in the Editor."));

	// If there is no MetaData, try to find it.
	if (MetaData == NULL)
	{
		MetaData = FindObjectFast<UMetaData>(this, FName(NAME_PackageMetaData));

		// If MetaData is NULL then it wasn't loaded by linker, so we have to create it.
		if(MetaData == NULL)
		{
			MetaData = NewObject<UMetaData>(this, NAME_PackageMetaData, RF_Standalone | RF_LoadCompleted);
		}
	}

	check(MetaData);

	if (MetaData->HasAnyFlags(RF_NeedLoad))
	{
		MetaData->GetLinker()->Preload(MetaData);
	}

	return MetaData;
}

/**
 * Fully loads this package. Safe to call multiple times and won't clobber already loaded assets.
 */
void UPackage::FullyLoad()
{
	// Make sure we're a topmost package.
	checkf(GetOuter()==nullptr, TEXT("Package is not topmost. Name:%s Path: %s"), *GetName(), *GetPathName());

	// Only perform work if we're not already fully loaded.
	if(!IsFullyLoaded())
	{
		// Re-load this package.
		LoadPackage(nullptr, *GetName(), LOAD_None);
	}
}

/** Tags generated objects with flags */
void UPackage::TagSubobjects(EObjectFlags NewFlags)
{
	Super::TagSubobjects(NewFlags);

	if (MetaData)
	{
		MetaData->SetFlags(NewFlags);
	}
}

/**
 * Returns whether the package is fully loaded.
 *
 * @return true if fully loaded or no file associated on disk, false otherwise
 */
bool UPackage::IsFullyLoaded() const
{
	// Newly created packages aren't loaded and therefore haven't been marked as being fully loaded. They are treated as fully
	// loaded packages though in this case, which is why we are looking to see whether the package exists on disk and assume it
	// has been fully loaded if it doesn't.
	if( !bHasBeenFullyLoaded && !HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading) )
	{
		FString DummyFilename;
		FString SourcePackageName = FileName != NAME_None ? FileName.ToString() : GetName();
		// Try to find matching package in package file cache. We use the source package name here as it may be loaded into a temporary package
		if (	!GetConvertedDynamicPackageNameToTypeName().Contains(GetFName()) &&
				(
					!FPackageName::DoesPackageExist(*SourcePackageName, NULL, &DummyFilename ) ||
					(GIsEditor && IFileManager::Get().FileSize(*DummyFilename) < 0) 
				)
			)
		{
			// Package has NOT been found, so we assume it's a newly created one and therefore fully loaded.
			bHasBeenFullyLoaded = true;
		}
	}

	return bHasBeenFullyLoaded;
}

void UPackage::BeginDestroy()
{
	// Detach linker if still attached
	if (LinkerLoad)
	{
		LinkerLoad->Detach();
		FLinkerManager::Get().RemoveLinker(LinkerLoad);
		LinkerLoad = nullptr;
	}

	Super::BeginDestroy();
}

// UE-21181 - Tracking where the loaded editor level's package gets flagged as a PIE object
#if WITH_EDITOR
UPackage* UPackage::EditorPackage = nullptr;
void UPackage::SetPackageFlagsTo( uint32 NewFlags )
{
	PackageFlagsPrivate = NewFlags;
	ensure(((NewFlags & PKG_PlayInEditor) == 0) || (this != EditorPackage));
}
#endif

#if WITH_EDITORONLY_DATA
void FixupPackageEditorOnlyFlag(FName PackageThatGotEditorOnlyFlagCleared, bool bRecursive);

void UPackage::SetLoadedByEditorPropertiesOnly(bool bIsEditorOnly, bool bRecursive /*= false*/)
{
	const bool bWasEditorOnly = bLoadedByEditorPropertiesOnly;
	bLoadedByEditorPropertiesOnly = bIsEditorOnly;
	if (bWasEditorOnly && !bIsEditorOnly)
	{
		FixupPackageEditorOnlyFlag(GetFName(), bRecursive);
	}
}
#endif


IMPLEMENT_CORE_INTRINSIC_CLASS(UPackage, UObject,
	{
		Class->ClassAddReferencedObjects = &UPackage::AddReferencedObjects;
		Class->EmitObjectReference(STRUCT_OFFSET(UPackage, MetaData), TEXT("MetaData"));
	}
);
