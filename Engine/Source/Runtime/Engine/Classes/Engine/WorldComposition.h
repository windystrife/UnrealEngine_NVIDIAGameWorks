// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/WorldCompositionUtility.h"
#include "WorldComposition.generated.h"

class ULevel;
class ULevelStreaming;

/**
 * Helper structure which holds information about level package which participates in world composition
 */
struct FWorldCompositionTile
{
	FWorldCompositionTile()
		: StreamingLevelStateChangeTime(0.0)
	{
	}
	
	// Long package name
	FName					PackageName;
	// Found LOD levels since last rescan
	TArray<FName>			LODPackageNames;
	// Tile information
	FWorldTileInfo			Info;
	// Timestamp when we have changed streaming level state
	double					StreamingLevelStateChangeTime;

	friend FArchive& operator<<( FArchive& Ar, FWorldCompositionTile& D )
	{
		Ar << D.PackageName << D.Info << D.LODPackageNames;
		return Ar;
	}
						
	/** Matcher */
	struct FPackageNameMatcher
	{
		FPackageNameMatcher( const FName& InPackageName )
			: PackageName( InPackageName )
		{
		}

		bool Matches( const FWorldCompositionTile& Candidate ) const
		{
			return Candidate.PackageName == PackageName;
		}

		const FName& PackageName;
	};
};

/**
 * Helper structure which holds results of distance queries to a world composition
 */
struct FDistanceVisibleLevel
{
	int32				TileIdx;	
	ULevelStreaming*	StreamingLevel;
	int32				LODIndex; 
};

/**
 * WorldComposition represents world structure:
 *	- Holds list of all level packages participating in this world and theirs base parameters (bounding boxes, offset from origin)
 *	- Holds list of streaming level objects to stream in and out based on distance from current view point
 *  - Handles properly levels repositioning during level loading and saving
 */
UCLASS(config=Engine)
class ENGINE_API UWorldComposition : public UObject
{
	GENERATED_UCLASS_BODY()

	typedef TArray<FWorldCompositionTile> FTilesList;

	/** Adds or removes level streaming objects to world based on distance settings from players current view */
	void UpdateStreamingState();
	
	/** Adds or removes level streaming objects to world based on distance settings from current view point */
	void UpdateStreamingState(const FVector& InLocation);
	void UpdateStreamingState(const FVector* InLocation, int32 Num);
	void UpdateStreamingStateCinematic(const FVector* InLocation, int32 Num);

#if WITH_EDITOR
	/** Simulates streaming in editor world, only visibility, no loading/unloading, no LOD sub-levels 
	 *  @returns Whether streaming levels state was updated by this call
	 */
	bool UpdateEditorStreamingState(const FVector& InLocation);
#endif// WITH_EDITOR

	/**
	 * Evaluates current world origin location against provided view location
	 * Issues request for world origin rebasing in case location is far enough from current origin
	 */
	void EvaluateWorldOriginLocation(const FVector& ViewLocation);

	/** Returns currently visible and hidden levels based on distance based streaming */
	void GetDistanceVisibleLevels(const FVector& InLocation, TArray<FDistanceVisibleLevel>& OutVisibleLevels, TArray<FDistanceVisibleLevel>& OutHiddenLevels) const;
	void GetDistanceVisibleLevels(const FVector* InLocations, int32 Num, TArray<FDistanceVisibleLevel>& OutVisibleLevels, TArray<FDistanceVisibleLevel>& OutHiddenLevels) const;

	/** @returns Whether specified streaming level is distance dependent */
	bool IsDistanceDependentLevel(FName PackageName) const;

	/** @returns Currently opened world composition root folder (long PackageName)*/
	FString GetWorldRoot() const;
	
	/** @returns Currently managed world obejct */
	UWorld* GetWorld() const override;
	
	/** Handles level OnPostLoad event*/
	static void OnLevelPostLoad(ULevel* InLevel);

	/** Handles level just before it going to be saved to disk */
	void OnLevelPreSave(ULevel* InLevel);
	
	/** Handles level just after it was saved to disk */
	void OnLevelPostSave(ULevel* InLevel);
	
	/** Handles level is being added to world */
	void OnLevelAddedToWorld(ULevel* InLevel);

	/** Handles level is being removed from the world */
	void OnLevelRemovedFromWorld(ULevel* InLevel);

	/** @returns Level offset from current origin, with respect to parent levels */
	FIntVector GetLevelOffset(ULevel* InLevel) const;

	/** @returns Level bounding box in current shifted space */
	FBox GetLevelBounds(ULevel* InLevel) const;

	/** Scans world root folder for relevant packages and initializes world composition structures */
	void Rescan();

	/**  */
	void ReinitializeForPIE();

	/** @returns Whether specified tile package name is managed by world composition */
	bool DoesTileExists(const FName& TilePackageName) const;

	/** @returns Tiles list in a world composition */
	FTilesList& GetTilesList();

#if WITH_EDITOR
	/** @returns FWorldTileInfo associated with specified package */
	FWorldTileInfo GetTileInfo(const FName& InPackageName) const;
	
	/** Notification from World browser about changes in tile info structure */
	void OnTileInfoUpdated(const FName& InPackageName, const FWorldTileInfo& InInfo);

	/** Restores dirty tiles information after world composition being rescanned */
	void RestoreDirtyTilesInfo(const FTilesList& TilesPrevState);
	
	/** Collect tiles package names to cook  */
	void CollectTilesToCook(TArray<FString>& PackageNames);

	// Event to enable/disable world composition in the world
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FEnableWorldCompositionEvent, UWorld*, bool);
	static FEnableWorldCompositionEvent EnableWorldCompositionEvent;
	
	// Event when world composition was successfully enabled/disabled in the world
	DECLARE_MULTICAST_DELEGATE_OneParam(FWorldCompositionChangedEvent, UWorld*);
	static FWorldCompositionChangedEvent WorldCompositionChangedEvent;

#endif //WITH_EDITOR

private:
	// UObject interface
	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostLoad() override;
	// UObject interface

	/** Populate streaming level objects using tiles information */
	void PopulateStreamingLevels();

	/** Calculates tiles absolute positions based on relative positions */
	void CaclulateTilesAbsolutePositions();

	/** Resets world composition structures */
	void Reset();
	
	/** @returns  Streaming level object for corresponding FWorldCompositionTile */
	ULevelStreaming* CreateStreamingLevel(const FWorldCompositionTile& Info) const;
		
	/** Fixups internal structures for PIE mode */
	void FixupForPIE(int32 PIEInstanceID);

	/**
	 * Finds tile by package name 
	 * @return Pointer to a found tile 
	 */
	int32 FindTileIndexByName(const FName& InPackageName) const;
	FWorldCompositionTile* FindTileByName(const FName& InPackageName) const;
	
	/** @returns Whether specified streaming level is distance dependent */
	bool IsDistanceDependentLevel(int32 TileIdx) const;

	/** Attempts to set new streaming state for a particular tile, could be rejected if state change on 'cooldown' */
	bool CommitTileStreamingState(UWorld* PersistenWorld, int32 TileIdx, bool bShouldBeLoaded, bool bShouldBeVisible, bool bShouldBlock, int32 LODIdx);

public:
#if WITH_EDITOR
	// Hack for a World Browser to be able to temporally show hidden levels 
	// regardless of current world origin and without offsetting them temporally 
	bool						bTemporallyDisableOriginTracking;
#endif //WITH_EDITOR

private:
	// Path to current world composition (long PackageName)
	FString						WorldRoot;
	
	// List of all tiles participating in the world composition
	FTilesList					Tiles;

public:
	// Streaming level objects for each tile
	UPROPERTY(transient)
	TArray<ULevelStreaming*>	TilesStreaming;

	// Time threshold between tile streaming state changes
	UPROPERTY(config)
	double						TilesStreamingTimeThreshold;

	// Whether all distance dependent tiles should be loaded and visible during cinematic
	UPROPERTY(config)
	bool						bLoadAllTilesDuringCinematic;

	// Whether to rebase origin in 3D space, otherwise only on XY plane
	UPROPERTY(config)
	bool						bRebaseOriginIn3DSpace;

#if WITH_EDITORONLY_DATA
	// Whether all tiles locations are locked
	UPROPERTY()
	bool						bLockTilesLocation;
#endif

	// Maximum distance to current view point where we should initiate origin rebasing
	UPROPERTY(config)
	float						RebaseOriginDistance;
};
