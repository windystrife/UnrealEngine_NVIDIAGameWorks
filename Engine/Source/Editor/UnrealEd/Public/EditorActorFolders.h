// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/GCObject.h"
#include "EditorActorFolders.generated.h"

class AActor;

/** Multicast delegates for broadcasting various folder events */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActorFolderCreate, UWorld&, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActorFolderDelete, UWorld&, FName);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActorFolderMove, UWorld&, FName /* src */, FName /* dst */);

USTRUCT()
struct FActorFolderProps
{
	GENERATED_USTRUCT_BODY()

	FActorFolderProps() : bIsExpanded(true) {}

	/** Serializer */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FActorFolderProps& Folder)
	{
		return Ar << Folder.bIsExpanded;
	}

	bool bIsExpanded;
};

/** Actor Folder UObject. This is used to support undo/redo reliably */
UCLASS()
class UEditorActorFolders : public UObject
{
public:
	GENERATED_BODY()
public:
	virtual void Serialize(FArchive& Ar) override;

	TMap<FName, FActorFolderProps> Folders;
};

/** Class responsible for managing an in-memory representation of actor folders in the editor */
struct UNREALED_API FActorFolders : public FGCObject
{
	FActorFolders();
	~FActorFolders();

	// FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End FGCObject Interface

	/** Check whether the singleton is valid */
	static bool IsAvailable() { return Singleton != nullptr; }

	/** Singleton access - only valid if IsAvailable() */
	static FActorFolders& Get();

	/** Initialize the singleton instance - called on Editor Startup */
	static void Init();

	/** Clean up the singleton instance - called on Editor Exit */
	static void Cleanup();

	/** Folder creation and deletion events. Called whenever a folder is created or deleted in a world. */
	static FOnActorFolderCreate OnFolderCreate;
	static FOnActorFolderMove 	OnFolderMove;
	static FOnActorFolderDelete OnFolderDelete;

	/** Check if the specified path is a child of the specified parent */
	static bool PathIsChildOf(const FString& InPotentialChild, const FString& InParent);

	/** Get a map of folder properties for the specified world (map of folder path -> properties) */
	const TMap<FName, FActorFolderProps>& GetFolderPropertiesForWorld(UWorld& InWorld);

	/** Get the folder properties for the specified path. Returns nullptr if no properties exist */
	FActorFolderProps* GetFolderProperties(UWorld& InWorld, FName InPath);

	/** Get a default folder name under the specified parent path */
	FName GetDefaultFolderName(UWorld& InWorld, FName ParentPath = FName());
	
	/** Get a new default folder name that would apply to the current selection */
	FName GetDefaultFolderNameForSelection(UWorld& InWorld);

	/** Create a new folder in the specified world, of the specified path */
	void CreateFolder(UWorld& InWorld, FName Path);

	/** Same as CreateFolder, but moves the current actor selection into the new folder as well */
	void CreateFolderContainingSelection(UWorld& InWorld, FName Path);

	/** Sets the folder path for all the selected actors */
	void SetSelectedFolderPath(FName Path) const;

	/** Delete the specified folder in the world */
	void DeleteFolder(UWorld& InWorld, FName FolderToDelete);

	/** Rename the specified path to a new name */
	bool RenameFolderInWorld(UWorld& World, FName OldPath, FName NewPath);

private:

	/** Returns true if folders have been created for the specified world */
	bool FoldersExistForWorld(UWorld& InWorld) const;

	/** Get or create a folder container for the specified world */
	UEditorActorFolders& GetOrCreateFoldersForWorld(UWorld& InWorld);

	/** Create and update a folder container for the specified world */
	UEditorActorFolders& InitializeForWorld(UWorld& InWorld);

	/** Rebuild the folder list for the specified world. This can be very slow as it
		iterates all actors in memory to rebuild the array of actors for this world */
	void RebuildFolderListForWorld(UWorld& InWorld);

	/** Called when an actor's folder has changed */
	void OnActorFolderChanged(const AActor* InActor, FName OldPath);

	/** Called when the actor list of the current world has changed */
	void OnLevelActorListChanged();

	/** Called when the global map in the editor has changed */
	void OnMapChange(uint32 MapChangeFlags);

	/** Called after a world has been saved */
	void OnWorldSaved(uint32 SaveFlags, UWorld* World, bool bSuccess);

	/** Remove any references to folder arrays for dead worlds */
	void Housekeeping();

	/** Add a folder to the folder map for the specified world. Does not trigger any events. */
	bool AddFolderToWorld(UWorld& InWorld, FName Path);

	/** Transient map of folders, keyed on world pointer */
	TMap<TWeakObjectPtr<UWorld>, UEditorActorFolders*> TemporaryWorldFolders;

	/** Singleton instance maintained by the editor */
	static FActorFolders* Singleton;
};
