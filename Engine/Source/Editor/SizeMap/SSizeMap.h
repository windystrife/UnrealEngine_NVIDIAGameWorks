// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetData.h"
#include "ITreeMap.h"
#include "STreeMap.h"

class FAssetThumbnailPool;

/**
 * Tree map for displaying the size of assets
 */
class SSizeMap : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SSizeMap ) 
		: _SelectAssetOnDoubleClick(true)
	{}
		SLATE_ARGUMENT(bool, SelectAssetOnDoubleClick)
	SLATE_END_ARGS()

	/** Default constructor for SSizeMap */
	SSizeMap();

	/** Destructor for SSizeMap */
	~SSizeMap();

	/**
	 * Construct the widget
	 *
	 * @param	InArgs				A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

	/** Sets the assets to view at the root of the size map.  This will rebuild the map. */
	void SetRootAssetPackageNames( const TArray<FName>& NewRootAssetPackageNames );


protected:

	/** Called after the initial asset registry scan finishes */
	void OnInitialAssetRegistrySearchComplete();

	/** Recursively discovers and loads dependent assets, building up a tree map node hierarchy */
	void GatherDependenciesRecursively( class FAssetRegistryModule& AssetRegistryModule, TSharedPtr<class FAssetThumbnailPool>& AssetThumbnailPool, TMap<FName, TSharedPtr<class FTreeMapNodeData>>& VisitedAssetPackageNames, const TArray<FName>& AssetPackageNames, const TSharedPtr<FTreeMapNodeData>& ParentTreeMapNode, TSharedPtr<FTreeMapNodeData>& SharedRootNode, int32& NumAssetsWhichFailedToLoad );

	/** After the node tree is built up, this function is called to generate nice labels for the nodes and to do a final clean-up pass on the tree */
	void FinalizeNodesRecursively( TSharedPtr<FTreeMapNodeData>& Node, const TSharedPtr<FTreeMapNodeData>& SharedRootNode, int32& TotalAssetCount, SIZE_T& TotalSize, bool& bAnyUnknownSizes );

	/** Refreshes the display */
	void RefreshMap();

	/** Called when the user double-clicks on an asset in the tree */
	void OnTreeMapNodeDoubleClicked( class FTreeMapNodeData& TreeMapNodeData );


protected:

	/** Our tree map widget */
	TSharedPtr<class STreeMap> TreeMapWidget;

	/** The assets we were asked to look at */
	TArray<FName> RootAssetPackageNames;

	/** Our tree map source data */
	TSharedPtr<class FTreeMapNodeData> RootTreeMapNode;

	/** Thumbnail pool */
	TSharedPtr<class FAssetThumbnailPool> AssetThumbnailPool;

	/** Tell us if we should select the asset on the double click */
	bool SelectAssetOnDoubleClick;

	/** This struct will contain size map-specific payload data that we'll associate with tree map nodes using a hash table */
	struct FNodeSizeMapData
	{
		/** How big the asset is */
		SIZE_T AssetSize;

		/** Whether it has a known size or not */
		bool bHasKnownSize;

		/** Data from the asset registry about this asset */
		FAssetData AssetData;
	};

	/** Maps a tree node to our size map-specific user data for that node */
	typedef TMap<TSharedRef<FTreeMapNodeData>, FNodeSizeMapData> FNodeSizeMapDataMap;
	FNodeSizeMapDataMap NodeSizeMapDataMap;
};

