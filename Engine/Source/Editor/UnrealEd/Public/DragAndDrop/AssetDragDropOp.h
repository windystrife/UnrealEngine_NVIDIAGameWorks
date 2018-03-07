// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "Input/DragAndDrop.h"
#include "Layout/Visibility.h"
#include "DragAndDrop/DecoratedDragDropOp.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
class UActorFactory;

class UNREALED_API FAssetDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAssetDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FAssetDragDropOp> New(const FAssetData& InAssetData, UActorFactory* ActorFactory = nullptr);

	static TSharedRef<FAssetDragDropOp> New(TArray<FAssetData> InAssetData, UActorFactory* ActorFactory = nullptr);

	static TSharedRef<FAssetDragDropOp> New(FString InAssetPath);

	static TSharedRef<FAssetDragDropOp> New(TArray<FString> InAssetPaths);

	static TSharedRef<FAssetDragDropOp> New(TArray<FAssetData> InAssetData, TArray<FString> InAssetPaths, UActorFactory* ActorFactory = nullptr);

	/** @return true if this drag operation contains assets */
	bool HasAssets() const
	{
		return AssetData.Num() > 0;
	}

	/** @return true if this drag operation contains asset paths */
	bool HasAssetPaths() const
	{
		return AssetPaths.Num() > 0;
	}

	/** @return The assets from this drag operation */
	const TArray<FAssetData>& GetAssets() const
	{
		return AssetData;
	}

	/** @return The asset paths from this drag operation */
	const TArray<FString>& GetAssetPaths() const
	{
		return AssetPaths;
	}

	/** @return The actor factory to use if converting this asset to an actor */
	UActorFactory* GetActorFactory() const
	{
		return ActorFactory.Get();
	}

public:
	virtual ~FAssetDragDropOp();

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	FText GetDecoratorText() const;

private:
	void Init();

	/** Data for the assets this item represents */
	TArray<FAssetData> AssetData;

	/** Data for the asset paths this item represents */
	TArray<FString> AssetPaths;

	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	/** Handle to the thumbnail resource */
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	/** The actor factory to use if converting this asset to an actor */
	TWeakObjectPtr<UActorFactory> ActorFactory;

	/** The size of the thumbnail */
	int32 ThumbnailSize;
};
