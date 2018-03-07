// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class AActor;
struct FAssetData;
class FLevelCollectionModel;
class FLevelDragDropOp;
class FLevelModel;
class ULevel;
class ULevelStreaming;
template< typename TItemType > class IFilter;

typedef TArray<TSharedPtr<class FLevelModel>> FLevelModelList;
class FLevelCollectionModel;

/**
 *  Interface for level collection hierarchy traversal
 */
struct FLevelModelVisitor
{
	virtual ~FLevelModelVisitor() { }
	virtual void Visit(FLevelModel& Item) = 0;
};

/**
 * Interface for non-UI presentation logic for a level in a world
 */
class FLevelModel
	: public TSharedFromThis<FLevelModel>	
{
public:
	typedef IFilter<const TWeakObjectPtr<AActor>&> ActorFilter;

	DECLARE_EVENT( FLevelModel, FSimpleEvent );
	
public:
	FLevelModel(FLevelCollectionModel& InLevelCollectionModel);

	virtual ~FLevelModel();
	
	/** Traverses level model hierarchy */
	void Accept(FLevelModelVisitor& Vistor);

	/** Sets level selection flag */
	void SetLevelSelectionFlag(bool bExpanded);
	
	/** @return Level selection flag */
	bool GetLevelSelectionFlag() const;
		
	/** Sets level child hierarchy expansion flag */
	void SetLevelExpansionFlag(bool bExpanded);
	
	/** @return Level child hierarchy expansion flag */
	bool GetLevelExpansionFlag() const;

	/** Sets level filtered out flag */
	void SetLevelFilteredOutFlag(bool bFiltredOut);
	
	/** @return Whether this level model was filtered out */
	bool GetLevelFilteredOutFlag() const;

	/**	@return	Level display name */
	FString GetDisplayName() const;
	
	/**	@return	Level package file name */
	FString GetPackageFileName() const;

	/**	@return	Whether level model has valid package file */
	virtual bool HasValidPackage() const { return true; };
		
	/** @return Pointer to UObject to be used as key in SNodePanel */
	virtual UObject* GetNodeObject() = 0;

	/** @return ULevel object if any */
	virtual ULevel* GetLevelObject() const = 0;
		
	/**	@return	Level asset name */
	virtual FName GetAssetName() const = 0;

	/**	@return	Level package file name */
	virtual FName GetLongPackageName() const = 0;

	/** Update asset associated with level model */
	virtual void UpdateAsset(const FAssetData& AssetData) = 0;
	
	/** Refreshes cached data */
	virtual void Update();
	
	/** Refreshes visual information */
	virtual void UpdateVisuals();

	/**	@return	Whether level is in PIE/SIE mode */
	bool IsSimulating() const;
	
	/** @return Whether level is CurrentLevel */
	bool IsCurrent() const;

	/** @return Whether level is PersistentLevel */
	bool IsPersistent() const;

	/** @return Whether level is editable */
	bool IsEditable() const;

	/** @return Whether level is dirty */
	bool IsDirty() const;

	/** @return Whether level is a lighting scenario */
	bool IsLightingScenario() const;

	void SetIsLightingScenario(bool bNew);

	/** @return Whether level has loaded content */
	bool IsLoaded() const;

	/** @return Whether level is in process of loading content */
	bool IsLoading() const;

	/**	@return Whether level is visible in the world */
	bool IsVisible() const;

	/**	@return Whether level is locked */
	bool IsLocked() const;

	/** @return Whether level package file is read only */
	bool IsFileReadOnly() const;
		
	/** Loads level into editor */
	virtual void LoadLevel();

	/** Sets the Level's visibility */
	virtual void SetVisible(bool bVisible);

	/** Sets the Level's locked/unlocked state */
	void SetLocked(bool bLocked);

	/** Sets Level as current in the world */
	void MakeLevelCurrent();
	
	/** @return Whether specified point is hovering level */
	virtual bool HitTest2D(const FVector2D& Point) const;
	
	/** @return Level top left corner position */
	virtual FVector2D GetLevelPosition2D() const;

	/** @return XY size of level */
	virtual FVector2D GetLevelSize2D() const;

	/** @return Level bounding box */
	virtual FBox GetLevelBounds() const;
	
	/** @return level translation delta, when user moving level item */
	FVector2D GetLevelTranslationDelta() const;

	/** Sets new translation delta to this model and all descendants*/
	void SetLevelTranslationDelta(FVector2D InAbsoluteDelta);

	/** @return level color, used for visualization. (Show -> Advanced -> Level Coloration) */
	virtual FLinearColor GetLevelColor() const;

	/** Sets level color, used for visualization. (Show -> Advanced -> Level Coloration) */
	virtual void SetLevelColor(FLinearColor InColor);

	/** Whether level should be drawn in world composition view */
	virtual bool IsVisibleInCompositionView() const;
	
	/**	@return Whether level has associated blueprint script */
	bool HasKismet() const;

	/**	Opens level associated blueprint script */
	void OpenKismet();

	/** 
	 * Sets parent for this item
	 * @return false in case attaching has failed
	 */
	bool AttachTo(TSharedPtr<FLevelModel> InParent);

	/**	Notifies level model that filters has been changed */
	void OnFilterChanged();

	/**	@return Level child hierarchy */
	const FLevelModelList& GetChildren() const;
	
	/**	@return Parent level model */
	TSharedPtr<FLevelModel> GetParent() const;
	
	/**	Sets link to a parent model  */
	void SetParent(TSharedPtr<FLevelModel>);

	/**	Removes all entries from children list*/
	void RemoveAllChildren();

	/**	Removes specific child */
	void RemoveChild(TSharedPtr<FLevelModel> InChild);
	
	/**	Adds new entry to a children list */
	void AddChild(TSharedPtr<FLevelModel> InChild);
		
	/**	@return Whether this model has in ancestors specified level model */
	bool HasAncestor(const TSharedPtr<FLevelModel>& InLevel) const;

	/**	@return Whether this model has in descendants specified level model */
	bool HasDescendant(const TSharedPtr<FLevelModel>& InLevel) const;
	
	/** Returns the folder path that the level should use when displayed in the world hierarchy */
	virtual FName GetFolderPath() const { return NAME_None; }

	/** Sets the folder path that the level should use when displayed in the world hierarchy */
	virtual void SetFolderPath(const FName& InFolderPath) {}

	/** Returns true if the level model can be added to hierarchy folders */
	virtual bool HasFolderSupport() const { return false; }

	/**	@return Handles drop operation */
	virtual void OnDrop(const TSharedPtr<FLevelDragDropOp>& Op);
	
	/**	@return Whether it's possible to drop onto this level */
	virtual bool IsGoodToDrop(const TSharedPtr<FLevelDragDropOp>& Op) const;

	/** Notification when level was added(shown) to world */
	virtual void OnLevelAddedToWorld(ULevel* InLevel);

	/** Notification when level was removed(hidden) from world */
	virtual void OnLevelRemovedFromWorld();

	/** Notification on level reparenting */
	virtual void OnParentChanged() {};

	/** Event when level model has been changed */
	void BroadcastLevelChanged();
	
	/*
	 *
	 */
	struct FSimulationLevelStatus
	{
		bool bLoaded;
		bool bLoading;
		bool bVisible;
	};
	
	/** Updates this level simulation status  */
	void UpdateSimulationStatus(ULevelStreaming* StreamingLevel);

	/**	Broadcasts whenever level has changed */
	FSimpleEvent ChangedEvent;
	void BroadcastChangedEvent();

	/** Deselects all Actors in this level */
	void DeselectAllActors();

	/** Deselects all BSP surfaces in this level */
	void DeselectAllSurfaces();

	/**
	 *	Selects in the Editor all the Actors assigned to the Level, based on the specified conditions.
	 *
	 *	@param	bSelect					if true actors will be selected; If false, actors will be deselected
	 *	@param	bNotify					if true the editor will be notified of the selection change; If false, the editor will not
	 *	@param	bSelectEvenIfHidden		if true actors that are hidden will be selected; If false, they will be skipped
	 *	@param	Filter					Only actors which pass the filters restrictions will be selected
	 */
	void SelectActors(bool bSelect, bool bNotify, bool bSelectEvenIfHidden, 
						const TSharedPtr<ActorFilter>& Filter = TSharedPtr<ActorFilter>(NULL));
	
	/** Updates cached value of level actors count */
	void UpdateLevelActorsCount();

	/** Updates cached value of level display name */
	void UpdateDisplayName();

	/** @return the Level's Lightmass Size as a FString */
	FString GetLightmassSizeString() const;

	/** @return the Level's File Size as a FString */
	FString GetFileSizeString() const;
	
	/** @return Class used for streaming this level */
	virtual UClass* GetStreamingClass() const;

protected:
	/** Called when the map asset is renamed */
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);

protected:
	/** Level model display name */
	FString								DisplayName;

	/** Reference to owning collection model */
	FLevelCollectionModel&				LevelCollectionModel;
		
	/** The parent level  */
	TWeakPtr<FLevelModel>				Parent;
		
	/** Filtered children of this level  */
	FLevelModelList						FilteredChildren;

	/** All children of this level  */
	FLevelModelList						AllChildren;

	// Level simulation status
	FSimulationLevelStatus				SimulationStatus;

	// Whether this level model is selected
	bool								bSelected;
	
	// Whether this level model is expended in hierarchy view
	bool								bExpanded;

	// Whether this level model is in a process of loading content
	bool								bLoadingLevel;
	
	// Whether this level model does not pass filters
	bool								bFilteredOut;

	// Current translation delta
	FVector2D							LevelTranslationDelta;

	// Cached level actors count
	int32								LevelActorsCount;
};
