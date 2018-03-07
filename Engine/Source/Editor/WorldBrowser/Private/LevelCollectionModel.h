// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Framework/Commands/UICommandList.h"
#include "Engine/World.h"
#include "TickableEditorObject.h"
#include "WorldBrowserDragDrop.h"
#include "Misc/IFilter.h"
#include "LevelModel.h"

#include "Misc/FilterCollection.h"

class FMenuBuilder;
class IDetailsView;
class UEditorEngine;
class UMaterialInterface;

typedef IFilter< const FLevelModel* >				LevelFilter;
typedef TFilterCollection< const FLevelModel* >		LevelFilterCollection;

/**
 * Interface for non-UI presentation logic for a world
 */
class FLevelCollectionModel
	: public TSharedFromThis<FLevelCollectionModel>	
	, public FTickableEditorObject
{
public:
	DECLARE_EVENT_OneParam( FLevelCollectionModel, FOnNewItemAdded, TSharedPtr<FLevelModel>);
	DECLARE_EVENT( FLevelCollectionModel, FSimpleEvent );


public:
	FLevelCollectionModel();
	virtual ~FLevelCollectionModel();

	/** FTickableEditorObject interface */
	void Tick( float DeltaTime ) override;
	bool IsTickable() const override { return true; }
	TStatId GetStatId() const override;
	/** FTickableEditorObject interface */
	
	/**	@return	Whether level collection is read only now */
	bool IsReadOnly() const;
	
	/**	@return	Whether level collection is in PIE/SIE mode */
	bool IsSimulating() const;

	/**	@return	Current simulation world */
	UWorld* GetSimulationWorld() const;

	/**	@return	Current editor world */
	UWorld* GetWorld(bool bEvenIfPendingKill = false) const { return CurrentWorld.Get(bEvenIfPendingKill); }

	/** @return	Whether current world has world origin rebasing enabled */
	bool IsOriginRebasingEnabled() const;

	/** Current world size  */
	FIntPoint GetWorldSize() const { return WorldSize; }
	
	/**	@return	Root list of levels in hierarchy */
	FLevelModelList& GetRootLevelList();

	/**	@return	All level list managed by this level collection */
	const FLevelModelList& GetAllLevels() const;

	/**	@return	List of filtered levels */
	const FLevelModelList& GetFilteredLevels() const;

	/**	@return	Currently selected level list */
	const FLevelModelList& GetSelectedLevels() const;

	/** Adds a filter which restricts the Levels shown in UI */
	void AddFilter(const TSharedRef<LevelFilter>& InFilter);

	/** Removes a filter which restricted the Levels shown in UI */
	void RemoveFilter(const TSharedRef<LevelFilter>& InFilter);

	/**	@return	Whether level filtering is active now */
	bool IsFilterActive() const;
	
	/**	Iterates through level hierarchy with given Visitor */
	void IterateHierarchy(FLevelModelVisitor& Visitor);

	/**	Sets selected level list */
	void SetSelectedLevels(const FLevelModelList& InList);
	
	/**	Sets selection to a levels that is currently marked as selected in UWorld */
	void SetSelectedLevelsFromWorld();

	/**	@return	Found level model which represents specified level object */
	TSharedPtr<FLevelModel> FindLevelModel(ULevel* InLevel) const;

	/**	@return	Found level model with specified level package name */
	TSharedPtr<FLevelModel> FindLevelModel(const FName& PackageName) const;

	/**	Hides level in the world */
	void HideLevels(const FLevelModelList& InLevelList);
	
	/**	Shows level in the world */
	void ShowLevels(const FLevelModelList& InLevelList);

	/**	Unlocks level in the world */
	void UnlockLevels(const FLevelModelList& InLevelList);
	
	/**	Locks level in the world */
	void LockLevels(const FLevelModelList& InLevelList);

	/**	Saves level to disk */
	void SaveLevels(const FLevelModelList& InLevelList);

	/**	Loads level from disk */
	void LoadLevels(const FLevelModelList& InLevelList);
	
	/**	Unloads levels from the editor */
	virtual void UnloadLevels(const FLevelModelList& InLevelList);

	/** Translate levels by specified delta */
	virtual void TranslateLevels(const FLevelModelList& InLevelList, FVector2D InAbsoluteDelta, bool bSnapDelta = true);
	
	/** Snaps translation delta */
	virtual FVector2D SnapTranslationDelta(const FLevelModelList& InLevelList, FVector2D InAbsoluteDelta, bool bBoundsSnapping, float SnappingValue);

	/**	Updates current translation delta, when user drags levels on minimap */
	virtual void UpdateTranslationDelta(const FLevelModelList& InLevelList, FVector2D InTranslationDelta, bool bBoundsSnapping, float SnappingValue);

	/** Attach levels as children to specified level */
	void AssignParent(const FLevelModelList& InLevels, TSharedPtr<FLevelModel> InParent);

	/** Adds all levels in worlds represented by the supplied world list as sublevels */
	virtual void AddExistingLevelsFromAssetData(const TArray<struct FAssetData>& WorldList);
			
	/**	Create drag drop operation for a selected level models */
	virtual TSharedPtr<WorldHierarchy::FWorldBrowserDragDropOp> CreateDragDropOp() const;

	/** Create a drag and drop operation for the specified level models */
	virtual TSharedPtr<WorldHierarchy::FWorldBrowserDragDropOp> CreateDragDropOp(const FLevelModelList& InLevels) const;
	
	/**	@return	Whether specified level passes all filters */
	virtual bool PassesAllFilters(const FLevelModel& InLevelModel) const;
	
	/**	Builds 'hierarchy' commands menu for a selected levels */
	virtual void BuildHierarchyMenu(FMenuBuilder& InMenuBuilder) const;
	
	/**	Customize 'File' section in main menu  */
	virtual void CustomizeFileMainMenu(FMenuBuilder& InMenuBuilder) const;
		
	/**	@return	Player view in the PIE/Simulation world */
	virtual bool GetPlayerView(FVector& Location, FRotator& Rotation) const;

	/**	@return	Observer view in the Editor/Similuation world */
	virtual bool GetObserverView(FVector& Location, FRotator& Rotation) const;

	/**	Compares 2 levels by Z order */
	virtual bool CompareLevelsZOrder(TSharedPtr<FLevelModel> InA, TSharedPtr<FLevelModel> InB) const;

	/**	Registers level details customizations */
	virtual void RegisterDetailsCustomization(class FPropertyEditorModule& PropertyModule, TSharedPtr<class IDetailsView> InDetailsView);
	
	/**	Unregisters level details customizations */
	virtual void UnregisterDetailsCustomization(class FPropertyEditorModule& PropertyModule, TSharedPtr<class IDetailsView> InDetailsView);

	/** @return	Whether this level collection model is a tile world */
	virtual bool IsTileWorld() const { return false; };

	/** Returns true if this collection model will support folders */
	virtual bool HasFolderSupport() const { return false; }

	/** Rebuilds levels collection */
	void PopulateLevelsList();

	/** Rebuilds the list of filtered Levels */
	void PopulateFilteredLevelsList();

	/**	Request to update levels cached information */
	void RequestUpdateAllLevels();
	
	/**	Request to redraw all levels */
	void RequestRedrawAllLevels();

	/**	Updates all levels cached information */
	void UpdateAllLevels();

	/**	Redraws all levels */
	void RedrawAllLevels();

	/** Updates level actor count for all levels */
	void UpdateLevelActorsCount();

	/** @return	whether exactly one level is selected */
	bool IsOneLevelSelected() const;

	/** @return	whether at least one level is selected */
	bool AreAnyLevelsSelected() const;

	/** @return whether all the currently selected levels are loaded */
	bool AreAllSelectedLevelsLoaded() const;

	/** @return whether any of the currently selected levels is loaded */
	bool AreAnySelectedLevelsLoaded() const;
	
	/** @return whether all the currently selected levels are unloaded */
	bool AreAllSelectedLevelsUnloaded() const;
	
	/** @return whether any of the currently selected levels is unloaded */
	bool AreAnySelectedLevelsUnloaded() const;

	/** @return whether all the currently selected levels are editable */
	bool AreAllSelectedLevelsEditable() const;

	/** @return whether all the currently selected levels are editable and not persistent */
	bool AreAllSelectedLevelsEditableAndNotPersistent() const;

	/** @return whether all the currently selected levels are editable and visible*/
	bool AreAllSelectedLevelsEditableAndVisible() const;

	/** @return whether any of the currently selected levels is editable */
	bool AreAnySelectedLevelsEditable() const;

	/** @return whether any of the currently selected levels is editable and visible*/
	bool AreAnySelectedLevelsEditableAndVisible() const;
	
	/** @return whether currently only one level selected and it is editable */
	bool IsSelectedLevelEditable() const;

	/** @return whether currently only one level selected and a lighting scenario */
	bool IsNewLightingScenarioState(bool bExistingState) const;

	void SetIsLightingScenario(bool bNewLightingScenario);

	/** @return whether any of the currently selected levels is dirty */
	bool AreAnySelectedLevelsDirty() const;

	/** @return	whether at least one actor is selected */
	bool AreActorsSelected() const;

	/** @return whether moving the selected actors to the selected level is a valid action */
	bool IsValidMoveActorsToLevel() const;

	/** @return whether moving the selected foliage to the selected level is a valid action */
	bool IsValidMoveFoliageToLevel() const;

	/** delegate used to pickup when the selection has changed */
	void OnActorSelectionChanged(UObject* obj);

	/** Sets a flag to re-cache whether the selected actors move to the selected level is valid */
	void OnActorOrLevelSelectionChanged();

	/** @return	whether 'display paths' is enabled */
	bool GetDisplayPathsState() const;

	/** Sets 'display paths', whether to show long package name in level display name */
	void SetDisplayPathsState(bool bDisplayPaths);

	/** @return	whether 'display actors count' is enabled */
	bool GetDisplayActorsCountState() const;

	/** Sets 'display actors count', whether to show actors count next to level name */
	void SetDisplayActorsCountState(bool bDisplayActorsCount);

	/**	Broadcasts whenever items selection has changed */
	FSimpleEvent SelectionChanged;
	void BroadcastSelectionChanged();

	/**	Broadcasts whenever items collection has changed */
	FSimpleEvent CollectionChanged;
	void BroadcastCollectionChanged();
		
	/** Broadcasts whenever items hierarchy has changed */
	FSimpleEvent HierarchyChanged;
	void BroadcastHierarchyChanged();

	/** Broadcasts before levels are unloaded */
	FSimpleEvent PreLevelsUnloaded;
	void BroadcastPreLevelsUnloaded();

	/** Broadcasts after levels are unloaded */
	FSimpleEvent PostLevelsUnloaded;
	void BroadcastPostLevelsUnloaded();
	
	/** Editable world axis length  */
	static float EditableAxisLength();

	/** Editable world bounds */
	static FBox EditableWorldArea();

	/**  */
	static void SCCCheckOut(const FLevelModelList& InList);
	static void SCCCheckIn(const FLevelModelList& InList);
	static void SCCOpenForAdd(const FLevelModelList& InList);
	static void SCCHistory(const FLevelModelList& InList);
	static void SCCRefresh(const FLevelModelList& InList);
	static void SCCDiffAgainstDepot(const FLevelModelList& InList, UEditorEngine* InEditor);
	
	/** @return	List of valid level package names from a specified level model list*/
	static TArray<FName> GetPackageNamesList(const FLevelModelList& InList);
	
	/** @return	List of valid level package filenames from a specified level model list*/
	static TArray<FString> GetFilenamesList(const FLevelModelList& InList);
	
	/** @return	List of valid packages from a specified level model list*/
	static TArray<UPackage*> GetPackagesList(const FLevelModelList& InList);
	
	/** @return	List of valid level objects from a specified level model list*/
	static TArray<ULevel*> GetLevelObjectList(const FLevelModelList& InList);

	/** @return	List of loaded level models from a specified level model list*/
	static FLevelModelList GetLoadedLevels(const FLevelModelList& InList);

	/** @return	List of all level models found while traversing hierarchy of specified level models */
	static FLevelModelList GetLevelsHierarchy(const FLevelModelList& InList);

	/** @return	Total bounding box of specified level models */
	static FBox GetLevelsBoundingBox(const FLevelModelList& InList, bool bIncludeChildren);

	/** @return	Total bounding box of specified visible level models */
	static FBox GetVisibleLevelsBoundingBox(const FLevelModelList& InList, bool bIncludeChildren);

	/** @return	The UICommandList supported by this collection */
	const TSharedRef<const FUICommandList> GetCommandList() const;

	/**  */
	void LoadSettings();
	
	/**  */
	void SaveSettings();

protected:
	/** Refreshes current cached data */
	void RefreshBrowser_Executed();
	
	/** Load selected levels to the world */
	void LoadSelectedLevels_Executed();

	/** Unload selected level from the world */
	void UnloadSelectedLevels_Executed();

	/** Make this Level the Current Level */
	void MakeLevelCurrent_Executed();

	/** Find selected levels in Content Browser */
	void FindInContentBrowser_Executed();

	/** Is FindInContentBrowser a valid action */
	bool IsValidFindInContentBrowser();

	/** Moves the selected actors to this level */
	void MoveActorsToSelected_Executed();

	/** Moves the selected foliage to this level */
	void MoveFoliageToSelected_Executed();

	/** Saves selected levels */
	void SaveSelectedLevels_Executed();

	/** Saves selected level under new name */
	void SaveSelectedLevelAs_Executed();
	
	/** Migrate selected levels */
	void MigrateSelectedLevels_Executed();

	/** Expand selected items hierarchy */
	void ExpandSelectedItems_Executed();
			
	/** Check-Out selected levels from SCC */
	void OnSCCCheckOut();

	/** Mark for Add selected levels from SCC */
	void OnSCCOpenForAdd();

	/** Check-In selected levels from SCC */
	void OnSCCCheckIn();

	/** Shows the SCC History of selected levels */
	void OnSCCHistory();

	/** Refreshes the states selected levels from SCC */
	void OnSCCRefresh();

	/** Diffs selected levels from with those in the SCC depot */
	void OnSCCDiffAgainstDepot();

	/** Enable source control features */
	void OnSCCConnect() const;

	/** Selects all levels in the collection view model */
	void SelectAllLevels_Executed();

	/** De-selects all levels in the collection view model */
	void DeselectAllLevels_Executed();

	/** Inverts level selection in the collection view model */
	void InvertSelection_Executed();
	
	/** Adds the Actors in the selected Levels from the viewport's existing selection */
	void SelectActors_Executed();

	/** Removes the Actors in the selected Levels from the viewport's existing selection */
	void DeselectActors_Executed();

	/** Toggles selected levels to a visible state in the viewports */
	void ShowSelectedLevels_Executed();

	/** Toggles selected levels to an invisible state in the viewports */
	void HideSelectedLevels_Executed();

	/** Toggles the selected levels to a visible state; toggles all other levels to an invisible state */
	void ShowOnlySelectedLevels_Executed();

	/** Toggles all levels to a visible state in the viewports */
	void ShowAllLevels_Executed();

	/** Hides all levels to an invisible state in the viewports */
	void HideAllLevels_Executed();
	
	/** Locks selected levels */
	void LockSelectedLevels_Executed();

	/** Unlocks selected levels */
	void UnlockSelectedLevels_Executed();

	/** Locks all levels */
	void LockAllLevels_Executed();

	/** Unlocks all levels */
	void UnockAllLevels_Executed();

	/** Toggle all read-only levels */
	void ToggleReadOnlyLevels_Executed();

	/** true if the SCC Check-Out option is available */
	bool CanExecuteSCCCheckOut() const
	{
		return bCanExecuteSCCCheckOut;
	}

	/** true if the SCC Check-In option is available */
	bool CanExecuteSCCCheckIn() const
	{
		return bCanExecuteSCCCheckIn;
	}

	/** true if the SCC Mark for Add option is available */
	bool CanExecuteSCCOpenForAdd() const
	{
		return bCanExecuteSCCOpenForAdd;
	}

	/** true if Source Control options are generally available. */
	bool CanExecuteSCC() const
	{
		return bCanExecuteSCC;
	}
	
	/** Fills MenuBulder with Lock level related commands */
	void FillLockSubMenu(class FMenuBuilder& MenuBuilder);
	
	/** Fills MenuBulder with level visisbility related commands */
	void FillVisibilitySubMenu(class FMenuBuilder& MenuBuilder);

	/** Fills MenuBulder with SCC related commands */
	void FillSourceControlSubMenu(class FMenuBuilder& MenuBuilder);
				
protected:
	/**  */
	virtual void Initialize(UWorld* InWorld);
	
	/**  */
	virtual void BindCommands();

	/** Removes the Actors in all read-only Levels from the viewport's existing selection */
	void DeselectActorsInAllReadOnlyLevel(const FLevelModelList& InLevelList);

	/** Removes the Actors in all read-only Levels from the viewport's existing selection */
	void DeselectSurfaceInAllReadOnlyLevel(const FLevelModelList& InLevelList);
	
	/** Called whenever level collection has been changed */
	virtual void OnLevelsCollectionChanged();
	
	/** Called whenever level selection has been changed */
	virtual void OnLevelsSelectionChanged();

	/** Called whenever level selection has been changed outside of this module, usually via World->SetSelectedLevels */
	void OnLevelsSelectionChangedOutside();
	
	/** Called whenever level collection hierarchy has been changed */
	virtual void OnLevelsHierarchyChanged();

	/** Called before loading specified level models into editor */
	virtual void OnPreLoadLevels(const FLevelModelList& InList) {};
	
	/** Called before making visible specified level models */
	virtual void OnPreShowLevels(const FLevelModelList& InList) {};

	/** Called when level was added to the world */
	void OnLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld);

	/** Called when level was removed from the world */
	void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);

	/** Handler for FEditorSupportDelegates::RedrawAllViewports event */
	void OnRedrawAllViewports();

	/** Handler for when an actor was added to a level */
	void OnLevelActorAdded(AActor* InActor);	

	/** Handler for when an actor was removed from a level */
	void OnLevelActorDeleted(AActor* InActor);
	
	/** Handler for level filter collection changes */
	void OnFilterChanged();

	/** Caches the variables for which SCC menu options are available */
	void CacheCanExecuteSourceControlVars() const;
		
protected:
	
	// The editor world from where we pull our data
	TWeakObjectPtr<UWorld>				CurrentWorld;

	// Has request to update all levels cached 
	bool								bRequestedUpdateAllLevels;
	
	// Has request to redraw all levels
	bool								bRequestedRedrawAllLevels;

	// Has request to update actors count for all levels
	bool								bRequestedUpdateActorsCount;

	/** The list of commands with bound delegates for the Level collection */
	const TSharedRef<FUICommandList>	CommandList;

	/** The collection of filters used to restrict the Levels shown in UI */
	const TSharedRef<LevelFilterCollection> Filters;
	
	/** Levels in the root of hierarchy, persistent levels  */
	FLevelModelList						RootLevelsList;
	
	/** All levels found in the world */
	FLevelModelList						AllLevelsList;
	
	/** All levels in a map<PackageName, LevelModel> */
	TMap<FName, TSharedPtr<FLevelModel>> AllLevelsMap;

	/** Filtered levels from AllLevels list  */
	FLevelModelList						FilteredLevelsList;

	/** Currently selected levels  */
	FLevelModelList						SelectedLevelsList;

	/** Cached value of world size (sum of levels size) */
	FIntPoint							WorldSize;

	/** Whether we should show long package names in level display names */
	bool								bDisplayPaths;

	/** Whether we should show actors count next to level name */
	bool								bDisplayActorsCount;

	/** true if the SCC Check-Out option is available */
	mutable bool						bCanExecuteSCCCheckOut;

	/** true if the SCC Check-In option is available */
	mutable bool						bCanExecuteSCCOpenForAdd;

	/** true if the SCC Mark for Add option is available */
	mutable bool						bCanExecuteSCCCheckIn;

	/** true if Source Control options are generally available. */
	mutable bool						bCanExecuteSCC;

	/** Flag for whether the selection of levels or actors has changed */
	mutable bool						bSelectionHasChanged;

	/** Guard to avoid recursive level selection updates */
	bool								bUpdatingLevelsSelection;
};

/** Current world origin location on XY plane  */
FORCEINLINE FIntPoint GetWorldOriginLocationXY(UWorld* InWorld)
{
	return FIntPoint(InWorld->OriginLocation.X, InWorld->OriginLocation.Y);
}

//
// Helper struct to temporally make specified UObject immune to dirtying
//
struct FUnmodifiableObject
{
	FUnmodifiableObject(UObject* InObject)
		: ImmuneObject(InObject)
		, bTransient(InObject->HasAnyFlags(RF_Transient))
	{
		if (!bTransient)
		{
			ImmuneObject->SetFlags(RF_Transient);
		}
	}
	
	~FUnmodifiableObject()
	{
		if (!bTransient)
		{
			ImmuneObject->ClearFlags(RF_Transient);
		}
	}

private:
	UObject*		ImmuneObject;
	bool			bTransient;
};

/**  */
struct FTiledLandscapeImportSettings
{
	FTiledLandscapeImportSettings()
		: Scale3D(100.f,100.f,100.f)
		, ComponentsNum(8)
		, QuadsPerSection(63)
		, SectionsPerComponent(1)
		, TilesCoordinatesOffset(0,0)
		, SizeX(1009)
		, bFlipYAxis(true)
	{}
	
	FVector				Scale3D;
	int32				ComponentsNum;
	int32				QuadsPerSection;
	int32				SectionsPerComponent;

	TArray<FString>		HeightmapFileList;
	TArray<FIntPoint>	TileCoordinates;
	FIntPoint			TilesCoordinatesOffset;
	int32				SizeX;
	bool				bFlipYAxis;


	TWeakObjectPtr<UMaterialInterface>	LandscapeMaterial;

	// Landscape layers 
	struct LandscapeLayerSettings
	{
		LandscapeLayerSettings()
			: bNoBlendWeight(false)
		{}

		FName						Name;
		bool						bNoBlendWeight;
		TMap<FIntPoint, FString>	WeightmapFiles;
	};

	TArray<LandscapeLayerSettings>	LandscapeLayerSettingsList;
};
