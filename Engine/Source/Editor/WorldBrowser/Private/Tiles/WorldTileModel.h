// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/WorldCompositionUtility.h"
#include "LevelModel.h"
#include "LandscapeProxy.h"

struct FAssetData;
class FLevelDragDropOp;
class FWorldTileCollectionModel;
class FWorldTileModel;
class ULevelStreaming;
class UMaterialInterface;
class UWorldTileDetails;

//
typedef TArray<TSharedPtr<class FWorldTileModel>> FWorldTileModelList;


/**
 * The non-UI presentation logic for a single Level (Tile) in world composition
 */
class FWorldTileModel 
	: public FLevelModel	
{
public:
	struct FCompareByLongPackageName
	{
		FORCEINLINE bool operator()(const TSharedPtr<FLevelModel>& A, 
									const TSharedPtr<FLevelModel>& B) const
		{
			return A->GetLongPackageName() < B->GetLongPackageName();
		}
	};

	enum EWorldDirections
	{
		XNegative,
		YNegative,
		XPositive,
		YPositive,
		Any
	};

	/**
	 * 
	 */
	struct FLandscapeImportSettings
	{
		// Depending on landscape guid import code will spawn Landscape actor or LandscapeProxy actor
		FGuid								LandscapeGuid;
		FTransform							LandscapeTransform;
		UMaterialInterface*					LandscapeMaterial;
		int32								ComponentSizeQuads;
		int32								SectionsPerComponent;
		int32								QuadsPerSection;
		int32								SizeX;
		int32								SizeY;
		TArray<uint16>						HeightData;
		TArray<FLandscapeImportLayerInfo>	ImportLayers;
		FString								HeightmapFilename;
		ELandscapeImportAlphamapType		ImportLayerType;
	};
	
	/**
	 *	FWorldTileModel Constructor
	 *
	 *	@param	InWorldData		WorldBrowser world data
	 *	@param	TileIdx			Tile index in world composition tiles list
	 */
	FWorldTileModel(FWorldTileCollectionModel& InWorldData, int32 TileIdx);
	~FWorldTileModel();

public:
	// FLevelModel interface
	virtual UObject* GetNodeObject() override;
	virtual ULevel* GetLevelObject() const override;
	virtual FName GetAssetName() const override;
	virtual FName GetLongPackageName() const override;
	virtual void Update() override;
	virtual void UpdateAsset(const FAssetData& AssetData) override;
	virtual void LoadLevel() override;
	virtual void SetVisible(bool bVisible) override;
	virtual FVector2D GetLevelPosition2D() const override;
	virtual FVector2D GetLevelSize2D() const override;
	virtual void OnDrop(const TSharedPtr<FLevelDragDropOp>& Op) override;
	virtual bool IsGoodToDrop(const TSharedPtr<FLevelDragDropOp>& Op) const override;
	virtual void OnLevelAddedToWorld(ULevel* InLevel) override;
	virtual void OnLevelRemovedFromWorld() override;
	virtual void OnParentChanged() override;
	virtual bool IsVisibleInCompositionView() const override;
	virtual FLinearColor GetLevelColor() const override;
	virtual void SetLevelColor(FLinearColor InColor) override;
	// FLevelModel interface end
	
	/** Adds new streaming level*/
	void AddStreamingLevel(UClass* InStreamingClass, const FName& InPackageName);

	/** Assigns level to provided layer*/
	void AssignToLayer(const FWorldTileLayer& InLayer);

	/**	@return Whether tile is root of hierarchy */
	bool IsRootTile() const;

	/** Whether level should be visible in given area*/
	bool ShouldBeVisible(FBox EditableArea) const;

	/**	@return Whether level is shelved */
	bool IsShelved() const;

	/** Hide a level from the editor */
	void Shelve();
	
	/** Show a level in the editor */
	void Unshelve();

	/** Whether this level landscape based or not */
	bool IsLandscapeBased() const;

	/** Whether this level based on tiled landscape or not */
	bool IsTiledLandscapeBased() const;
		
	/** Whether this level has ALandscapeProxy or not */
	bool IsLandscapeProxy() const;

	/** @return The landscape actor in case this level is landscape based */
	ALandscapeProxy* GetLandscape() const;
	
	/** Whether this level in provided layers list */
	bool IsInLayersList(const TArray<FWorldTileLayer>& InLayerList) const;
	
	/** @return Level position in shifted space */
	FVector2D GetLevelCurrentPosition() const;

	/** @return Level relative position */
	FIntPoint GetRelativeLevelPosition() const;
	
	/** @return Level absolute position in non shifted space */
	FIntPoint GetAbsoluteLevelPosition() const;
	
	/** @return Calculates Level absolute position in non shifted space based on relative position */
	FIntPoint CalcAbsoluteLevelPosition() const;
		
	/** @return ULevel bounding box in shifted space*/
	FBox GetLevelBounds() const override;
	
	/** @return Landscape component world size */
	FVector2D GetLandscapeComponentSize() const;

	/** Translate level center to new position */
	void SetLevelPosition(const FIntPoint& InPosition);

	/** Recursively sort all children by name */
	void SortRecursive();

	/** 
	 *	@return associated streaming level object for this tile
	 *	Creates a new object in case it does not exists in a persistent world
	 */
	ULevelStreaming* GetAssosiatedStreamingLevel();

	/**  */
	bool CreateAdjacentLandscapeProxy(ALandscapeProxy* SourceLandscape, FIntPoint SourceTileOffset, FWorldTileModel::EWorldDirections InWhere);

	/**  */
	ALandscapeProxy* ImportLandscapeTile(const FLandscapeImportSettings& Settings);

private:
	/** Flush world info to package and level objects */
	void OnLevelInfoUpdated();

	/** Fixup invalid streaming objects inside level */
	void FixupStreamingObjects();

	/** When level with landscape is moved we need to update internal landscape coordinates to match landscape component grid  */
	void UpdateLandscapeSectionsOffset(FIntPoint LevelOffset);
	
	/** Handler for LevelBoundsActorUpdated event */
	void OnLevelBoundsActorUpdated();

	/** Spawns AlevelBounds actor in the level in case it doesn't has one */
	void EnsureLevelHasBoundsActor();

	/** Handler for PostEditUndo  */
	void OnPostUndoEvent();
	
	/** Handler for PositionChanged event from Tile details object  */
	void OnPositionPropertyChanged();
	
	/** Handler for ParentPackageName event from Tile details object  */
	void OnParentPackageNamePropertyChanged();

	/** Handler for LOD settings changes event from Tile details object  */
	void OnLODSettingsPropertyChanged();
	
	/** Handler for ZOrder changes event from Tile details object  */
	void OnZOrderPropertyChanged();

	/** Handler for bHideInTileView changes event from Tile details object  */
	void OnHideInTileViewChanged();

	/** Set the asset name based on the passed in package name */
	void SetAssetName(const FName& PackageName);

public:
	/** This tile index in world composition tile list */
	int32									TileIdx;
	
	/** Package name this item represents */
	FName									AssetName;
	
	/** UObject which holds tile properties to be able to edit them via details panel*/
	UWorldTileDetails*						TileDetails;
	
	/** The Level this object represents */
	TWeakObjectPtr<ULevel>					LoadedLevel;

	// Whether this level was shelved: hidden by World Browser decision
	bool									bWasShelved;

private:
	/** Whether level has landscape components in it */
	TWeakObjectPtr<ALandscapeProxy>			Landscape;
	FVector									LandscapeComponentResolution;
	TArray<FIntPoint>						LandscapeComponentsXY;
	FIntRect								LandscapeComponentsRectXY;
	FVector2D								LandscapeComponentSize;
	FVector2D								LandscapePosition;	
};
