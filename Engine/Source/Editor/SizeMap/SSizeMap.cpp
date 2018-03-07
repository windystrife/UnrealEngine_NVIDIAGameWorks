// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSizeMap.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "Editor.h"
#include "AssetRegistryModule.h"
#include "AssetThumbnail.h"
#include "ClassIconFinder.h"
#include "Math/UnitConversion.h"

#define LOCTEXT_NAMESPACE "SizeMap"


SSizeMap::SSizeMap()
	: TreeMapWidget( nullptr ),
	  RootAssetPackageNames(),
	  RootTreeMapNode( new FTreeMapNodeData() ),

	  // @todo sizemap: Hard-coded thumbnail pool size.  Not a big deal, but ideally move the constants elsewhere
  	  AssetThumbnailPool( new FAssetThumbnailPool(1024) ),
	  SelectAssetOnDoubleClick(true)
{
}


SSizeMap::~SSizeMap()
{
	if( AssetThumbnailPool.IsValid() )
	{
		AssetThumbnailPool->ReleaseResources();
		AssetThumbnailPool.Reset();
	}
}


void SSizeMap::Construct( const FArguments& InArgs )
{
	SelectAssetOnDoubleClick = InArgs._SelectAssetOnDoubleClick;

	ChildSlot
	[
		SAssignNew( TreeMapWidget, STreeMap, RootTreeMapNode.ToSharedRef(), nullptr )
			.OnTreeMapNodeDoubleClicked( this, &SSizeMap::OnTreeMapNodeDoubleClicked )
	];
}


void SSizeMap::SetRootAssetPackageNames( const TArray<FName>& NewRootAssetPackageNames )
{
	RootAssetPackageNames = NewRootAssetPackageNames;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if ( AssetRegistryModule.Get().IsLoadingAssets() )
	{
		// We are still discovering assets, listen for the completion delegate before building the graph
		if (!AssetRegistryModule.Get().OnFilesLoaded().IsBoundToObject(this))
		{		
			AssetRegistryModule.Get().OnFilesLoaded().AddSP( this, &SSizeMap::OnInitialAssetRegistrySearchComplete );
		}
	}
	else
	{
		// All assets are already discovered, build the graph now.
		RefreshMap();
	}
}


namespace SizeMapInternals
{
	/** Serialization archive that discovers assets referenced by a specific Unreal object */
	class FAssetReferenceFinder : public FArchiveUObject
	{
	public:
		FAssetReferenceFinder( UObject* Object )
		{
			ArIsObjectReferenceCollector = true;
			ArIgnoreOuterRef = true;

			check( Object != nullptr );
			AllVisitedObjects.Add( Object );
			Object->Serialize( *this );
		}

		FArchive& operator<<( UObject*& Object )
		{
			// Only look at objects which are valid
			const bool bIsValidObject =
				Object != nullptr &&	// Object should not be null
				!Object->HasAnyFlags(RF_Transient) &&	// Should not be transient
				!Object->IsPendingKill(); // Should not be pending kill
			if( bIsValidObject )
			{
				// Skip objects that we've already processed
				if( !AllVisitedObjects.Contains( Object ) )
				{
					AllVisitedObjects.Add( Object );

					const bool bIsAsset =
						Object->GetOuter() != nullptr &&						// Not a package itself (such as a script package like '/Script/Engine')
						Object->GetOuter()->IsA( UPackage::StaticClass() ) &&	// Only want outer assets (these should be the only public assets, anyway)
						Object->HasAllFlags( RF_Public );						// Assets should be public

					if( bIsAsset )
					{
						ReferencedAssets.Add( Object );
					}
					else
					{
						// It's probably an inner object.  Recursively serialize.
						Object->Serialize( *this );

						// Make sure the object's class is serialized too, so that we catch any assets referenced from the class defaults
						AllVisitedObjects.Add( Object->GetClass() );
						Object->GetClass()->Serialize( *this );	// @todo sizemap urgent: Doesn't really seem to be needed
					}
				}
			}
			return *this;
		}

		TSet< UObject* >& GetReferencedAssets()
		{
			return ReferencedAssets;
		}

	protected:
		/** The set of referenced assets */
		TSet< UObject* > ReferencedAssets;

		/** Set of all objects we've visited, so we don't follow cycles */
		TSet< UObject* > AllVisitedObjects;
	};




	/** Given a size in bytes and a boolean that indicates whether the size is actually known to be correct, returns a pretty
	    string to represent that size, such as "256.0 MB", or "unknown size" */
	static FString MakeBestSizeString( const SIZE_T SizeInBytes, const bool bHasKnownSize )
	{
		FString BestSizeString;			

		const FNumericUnit<double> BestUnit = FUnitConversion::QuantizeUnitsToBestFit( (double)SizeInBytes, EUnit::Bytes );

		if( BestUnit.Units == EUnit::Bytes )
		{
			// We ended up with bytes, so show a decimal number
			BestSizeString = FString::Printf( TEXT( "%s %s" ), 
				*FText::AsNumber( static_cast<uint64>(SizeInBytes) ).ToString(), 
				*LOCTEXT( "Bytes", "bytes" ).ToString() );
		}
		else
		{
			// Show a fractional number with the best possible units
			FNumberFormattingOptions NumberFormattingOptions;
			NumberFormattingOptions.MaximumFractionalDigits = 1;	// @todo sizemap: We could make the number of digits customizable in the UI
			NumberFormattingOptions.MinimumFractionalDigits = 0;
			NumberFormattingOptions.MinimumIntegralDigits = 1;
			BestSizeString = FString::Printf( TEXT( "%s %s" ), 
				*FText::AsNumber( BestUnit.Value, &NumberFormattingOptions ).ToString(), 
				FUnitConversion::GetUnitDisplayString( BestUnit.Units ) );
		}

		if( !bHasKnownSize )
		{
			if( SizeInBytes == 0 )
			{
				BestSizeString = LOCTEXT( "UnknownSize", "unknown size" ).ToString();
			}
			else
			{
				BestSizeString = FString::Printf( TEXT( "%s %s" ),
					*LOCTEXT( "UnknownSizeButAtLeastThisBig", "at least" ).ToString(),
					*BestSizeString );
			}
		}
		return BestSizeString;
	}
}


void SSizeMap::GatherDependenciesRecursively( FAssetRegistryModule& AssetRegistryModule, TSharedPtr<FAssetThumbnailPool>& InAssetThumbnailPool, TMap<FName, TSharedPtr<FTreeMapNodeData>>& VisitedAssetPackageNames, const TArray<FName>& AssetPackageNames, const TSharedPtr<FTreeMapNodeData>& Node, TSharedPtr<FTreeMapNodeData>& SharedRootNode, int32& NumAssetsWhichFailedToLoad )
{
	for( const FName AssetPackageName : AssetPackageNames )
	{
		// Have we already added this asset to the tree?  If so, we'll either move it to a "shared" group or (if it's referenced again by the same
		// root-level asset) ignore it
		if( VisitedAssetPackageNames.Contains( AssetPackageName ) )
		{ 
			// OK, we've determined that this asset has already been referenced by something else in our tree.  We'll move it to a "shared" group
			// so all of the assets that are referenced in multiple places can be seen together.
			TSharedPtr<FTreeMapNodeData> ExistingNode = VisitedAssetPackageNames[ AssetPackageName ];

			// Is the existing node not already under the "shared" group?  Note that it might still be (indirectly) under
			// the "shared" group, in which case we'll still want to move it up to the root since we've figured out that it is
			// actually shared between multiple assets which themselves may be shared
			if( ExistingNode->Parent != SharedRootNode.Get() )
			{
				// Don't bother moving any of the assets at the root level into a "shared" bucket.  We're only trying to best
				// represent the memory used when all of the root-level assets have become loaded.  It's OK if root-level assets
				// are referenced by other assets in the set -- we don't need to indicate they are shared explicitly
				FTreeMapNodeData* ExistingNodeParent = ExistingNode->Parent;
				check( ExistingNodeParent != nullptr );
				const bool bExistingNodeIsAtRootLevel = ExistingNodeParent->Parent == nullptr || RootAssetPackageNames.Contains( AssetPackageName );
				if( !bExistingNodeIsAtRootLevel )
				{
					// OK, the current asset (AssetPackageName) is definitely not a root level asset, but its already in the tree
					// somewhere as a non-shared, non-root level asset.  We need to make sure that this Node's reference is not from the
					// same root-level asset as the ExistingNodeInTree.  Otherwise, there's no need to move it to a 'shared' group.
					FTreeMapNodeData* MyParentNode = Node.Get();
					check( MyParentNode != nullptr );
					FTreeMapNodeData* MyRootLevelAssetNode = MyParentNode;
					while( MyRootLevelAssetNode->Parent != nullptr && MyRootLevelAssetNode->Parent->Parent != nullptr )
					{
						MyRootLevelAssetNode = MyRootLevelAssetNode->Parent;
					}
					if( MyRootLevelAssetNode->Parent == nullptr )
					{
						// No root asset (Node must be a root level asset itself!)
						MyRootLevelAssetNode = nullptr;
					}
					
					// Find the existing node's root level asset node
					FTreeMapNodeData* ExistingNodeRootLevelAssetNode = ExistingNodeParent;
					while( ExistingNodeRootLevelAssetNode->Parent->Parent != nullptr )
					{
						ExistingNodeRootLevelAssetNode = ExistingNodeRootLevelAssetNode->Parent;
					}

					// If we're being referenced by another node within the same asset, no need to move it to a 'shared' group.  
					if( MyRootLevelAssetNode != ExistingNodeRootLevelAssetNode )
					{
						// This asset was already referenced by something else (or was in our top level list of assets to display sizes for)
						if( !SharedRootNode.IsValid() )
						{
							// Find the root-most tree node
							FTreeMapNodeData* RootNode = MyParentNode;
							while( RootNode->Parent != nullptr )
							{
								RootNode = RootNode->Parent;
							}

							SharedRootNode = MakeShareable( new FTreeMapNodeData() );
							RootNode->Children.Add( SharedRootNode );
							SharedRootNode->Parent = RootNode;	// Keep back-pointer to parent node
						}

						// Reparent the node that we've now determined to be shared
						ExistingNode->Parent->Children.Remove( ExistingNode );
						SharedRootNode->Children.Add( ExistingNode );
						ExistingNode->Parent = SharedRootNode.Get();
					}
				}
			}
		}
		else
		{
			// This asset is new to us so far!  Let's add it to the tree.  Later as we descend through references, we might find that the
			// asset is referenced by something else as well, in which case we'll pull it out and move it to a "shared" top-level box
					
			// Don't bother showing code references
			const FString AssetPackageNameString = AssetPackageName.ToString();
			if( !AssetPackageNameString.StartsWith( TEXT( "/Script/" ) ) )
			{
				FTreeMapNodeDataRef ChildTreeMapNode = MakeShareable( new FTreeMapNodeData() );
				Node->Children.Add( ChildTreeMapNode );
				ChildTreeMapNode->Parent = Node.Get();	// Keep back-pointer to parent node

				VisitedAssetPackageNames.Add( AssetPackageName, ChildTreeMapNode );

				FNodeSizeMapData& NodeSizeMapData = NodeSizeMapDataMap.Add( ChildTreeMapNode );

				// Set some defaults for this node.  These will be used if we can't actually locate the asset.
				// @todo sizemap urgent: We need a better indication in the UI when there are one or more missing assets.  Because missing assets have a size 
				//    of zero, they are nearly impossible to zoom into.  At the least, we should have some Output Log spew when assets cannot be loaded
				NodeSizeMapData.AssetData.AssetName = AssetPackageName;
				NodeSizeMapData.AssetData.AssetClass = FName( *LOCTEXT( "MissingAsset", "MISSING!" ).ToString() );
				NodeSizeMapData.AssetSize = 0;
				NodeSizeMapData.bHasKnownSize = false;

				// Find the asset using the asset registry
				// @todo sizemap: Asset registry-based reference gathering is faster but possibly not as exhaustive (no PostLoad created references, etc.)  Maybe should be optional?
				// @todo sizemap: With AR-based reference gathering, sometimes the size map is missing root level dependencies until you reopen it a few times (Buggy BP)
				// @todo sizemap: With AR-based reference gathering, reference changes at editor-time do not appear in the Size Map until you restart
				// @todo sizemap: With AR-based reference gathering, opening the size map for all engine content caused the window to not respond until a restart
				// @todo sizemap: We don't really need the asset registry given we need to load the objects to figure out their size, unless we make that AR-searchable.
				//   ---> This would allow us to not have to wait for AR initialization.  But if we made size AR-searchable, we could run very quickly for large data sets!
				const bool bUseAssetRegistryForDependencies = false;

				const FString AssetPathString = AssetPackageNameString + TEXT(".") + FPackageName::GetLongPackageAssetName( AssetPackageNameString );
				const FAssetData FoundAssetData = AssetRegistryModule.Get().GetAssetByObjectPath( FName( *AssetPathString ) );
				if( FoundAssetData.IsValid() )
				{
					NodeSizeMapData.AssetData = FoundAssetData;

					// Now actually load up the asset.  We need it in memory in order to accurately determine its size.
					// @todo sizemap: We could async load these packages to make the editor experience a bit nicer (smoother progress)
					UObject* Asset = StaticLoadObject( UObject::StaticClass(), nullptr, *AssetPathString );
					if( Asset != nullptr )
					{
						TArray<FName> ReferencedAssetPackageNames;
						if( bUseAssetRegistryForDependencies )
						{
							AssetRegistryModule.Get().GetDependencies( AssetPackageName, ReferencedAssetPackageNames );
						}
						else
						{
							SizeMapInternals::FAssetReferenceFinder References( Asset );
							for( UObject* Object : References.GetReferencedAssets() )
							{
								ReferencedAssetPackageNames.Add( FName( *Object->GetOutermost()->GetPathName() ) );
							}
						}

						// For textures, make sure we're getting the worst case size, not the size of the currently loaded set of mips
						// @todo sizemap: We should instead have a special EResourceSizeMode that asks for the worst case size.  Some assets (like UTextureCube) currently always report resident mip size, even when asked for inclusive size
						if( Asset->IsA( UTexture2D::StaticClass() ) )
						{
							NodeSizeMapData.AssetSize = Asset->GetResourceSizeBytes( EResourceSizeMode::Inclusive );
						}
						else
						{
							NodeSizeMapData.AssetSize = Asset->GetResourceSizeBytes( EResourceSizeMode::Exclusive );
						}

						NodeSizeMapData.bHasKnownSize = (NodeSizeMapData.AssetSize != 0);
						if( !NodeSizeMapData.bHasKnownSize )
						{
							// @todo sizemap urgent: Try to serialize to figure out how big it is (not into sub-assets though!)
							// FObjectMemoryAnalyzer ObjectMemoryAnalyzer( Asset );
						}

								
						// Now visit all of the assets that we are referencing
						GatherDependenciesRecursively( AssetRegistryModule, InAssetThumbnailPool, VisitedAssetPackageNames, ReferencedAssetPackageNames, ChildTreeMapNode, SharedRootNode, NumAssetsWhichFailedToLoad );
					}
					else
					{
						++NumAssetsWhichFailedToLoad;
					}
				}
				else
				{
					++NumAssetsWhichFailedToLoad;
				}
			}
		}
	}
}

	
void SSizeMap::FinalizeNodesRecursively( TSharedPtr<FTreeMapNodeData>& Node, const TSharedPtr<FTreeMapNodeData>& SharedRootNode, int32& TotalAssetCount, SIZE_T& TotalSize, bool& bAnyUnknownSizes )
{
	// Process children first, so we can get the totals for the root node and shared nodes
	int32 SubtreeAssetCount = 0;
	SIZE_T SubtreeSize = 0;
	bool bAnyUnknownSizesInSubtree = false;
	{
		for( TSharedPtr<FTreeMapNodeData> ChildNode : Node->Children )
		{
			FinalizeNodesRecursively( ChildNode, SharedRootNode, SubtreeAssetCount, SubtreeSize, bAnyUnknownSizesInSubtree );
		}

		TotalAssetCount += SubtreeAssetCount;
		TotalSize += SubtreeSize;
		if( bAnyUnknownSizesInSubtree )
		{
			bAnyUnknownSizes = true;
		}
	}

	if( Node == SharedRootNode )
	{
		// @todo sizemap: Should we indicate in a non-shared parent node how many if its dependents ended up being in the "shared" bucket?  Probably 
		// not that important, because the user can choose to view that asset in isolation to see the full tree.
		Node->Name = FString::Printf( TEXT( "%s  (%s)" ),
			*LOCTEXT( "SharedGroupName", "*SHARED*" ).ToString(),
			*SizeMapInternals::MakeBestSizeString( SubtreeSize, !bAnyUnknownSizes ) );

		// Container nodes are always auto-sized
		Node->Size = 0.0f;
	}
	else if( Node->Parent == nullptr )
	{
		// Tree root is always auto-sized
		Node->Size = 0.0f;
	}
	else
	{
		const FNodeSizeMapData& NodeSizeMapData = NodeSizeMapDataMap.FindChecked( Node.ToSharedRef() );

		++TotalAssetCount;
		TotalSize += NodeSizeMapData.AssetSize;

		if( !NodeSizeMapData.bHasKnownSize )
		{
			bAnyUnknownSizes = true;
		}

		// Setup a thumbnail
		const FSlateBrush* DefaultThumbnailSlateBrush;
		{
			// For non-class types, use the default based upon the actual asset class
			// This has the side effect of not showing a class icon for assets that don't have a proper thumbnail image available
			bool bIsClassType = false;
			const UClass* ThumbnailClass = FClassIconFinder::GetIconClassForAssetData( NodeSizeMapData.AssetData, &bIsClassType );
			const FName DefaultThumbnail = (bIsClassType) ? NAME_None : FName(*FString::Printf(TEXT("ClassThumbnail.%s"), *NodeSizeMapData.AssetData.AssetClass.ToString()));							
			DefaultThumbnailSlateBrush = FClassIconFinder::FindThumbnailForClass(ThumbnailClass, DefaultThumbnail);

			// @todo sizemap urgent: Actually implement rendered thumbnail support, not just class-based background images

			// const int32 ThumbnailSize = 128;	// @todo sizemap: Hard-coded thumbnail size.  Move this elsewhere
			//	TSharedRef<FAssetThumbnail> AssetThumbnail( new FAssetThumbnail( NodeSizeMapData.AssetData, ThumbnailSize, ThumbnailSize, AssetThumbnailPool ) );
			//	ChildTreeMapNode->AssetThumbnail = AssetThumbnail->MakeThumbnailImage();
		}

		if( Node->IsLeafNode() )
		{
			Node->CenterText = SizeMapInternals::MakeBestSizeString( NodeSizeMapData.AssetSize, NodeSizeMapData.bHasKnownSize );

			Node->Size = NodeSizeMapData.AssetSize;

			// The STreeMap widget is not expecting zero-sized leaf nodes.  So we make them very small instead.
			if( Node->Size == 0 )
			{
				Node->Size = 1;
			}

			// Leaf nodes get a background picture
			Node->BackgroundBrush = DefaultThumbnailSlateBrush;

			// "Asset name"
			// "Asset type"
			Node->Name = NodeSizeMapData.AssetData.AssetName.ToString();
			Node->Name2 = NodeSizeMapData.AssetData.AssetClass.ToString();
		}
		else
		{
			// Container nodes are always auto-sized
			Node->Size = 0.0f;

			// "Asset name (asset type, size)"
			Node->Name = FString::Printf( TEXT( "%s  (%s, %s)" ),
				*NodeSizeMapData.AssetData.AssetName.ToString(),
				*NodeSizeMapData.AssetData.AssetClass.ToString(),
				*SizeMapInternals::MakeBestSizeString( SubtreeSize + NodeSizeMapData.AssetSize, !bAnyUnknownSizesInSubtree && NodeSizeMapData.bHasKnownSize ) );

			const bool bNeedsSelfNode = NodeSizeMapData.AssetSize > 0;
			if( bNeedsSelfNode )
			{
				// We have children, so make some space for our own asset's size within our box
				FTreeMapNodeDataRef ChildSelfTreeMapNode = MakeShareable( new FTreeMapNodeData() );
				Node->Children.Add( ChildSelfTreeMapNode );
				ChildSelfTreeMapNode->Parent = Node.Get();	// Keep back-pointer to parent node

				// Map the "self" node to the same node data as its parent
				NodeSizeMapDataMap.Add( ChildSelfTreeMapNode, NodeSizeMapData );

				// "*SELF*"
				// "Asset type"
				ChildSelfTreeMapNode->Name = LOCTEXT( "SelfNodeLabel", "*SELF*" ).ToString();
				ChildSelfTreeMapNode->Name2 = NodeSizeMapData.AssetData.AssetClass.ToString();

				ChildSelfTreeMapNode->CenterText = SizeMapInternals::MakeBestSizeString( NodeSizeMapData.AssetSize, NodeSizeMapData.bHasKnownSize );
				ChildSelfTreeMapNode->Size = NodeSizeMapData.AssetSize;

				// Leaf nodes get a background picture
				ChildSelfTreeMapNode->BackgroundBrush = DefaultThumbnailSlateBrush;
			}
		}

	}

	// Sort all of my child nodes alphabetically.  This is just so that we get deterministic results when viewing the
	// same sets of assets.
	Node->Children.StableSort(
			[]( const FTreeMapNodeDataPtr& A, const FTreeMapNodeDataPtr& B )
			{
				return A->Name < B->Name;
			}
		);
}

	
void SSizeMap::RefreshMap()
{
	// Wipe the current tree out
	RootTreeMapNode->Children.Empty();
	NodeSizeMapDataMap.Empty();

	
	// First, do a pass to gather asset dependencies and build up a tree
	TMap<FName, TSharedPtr<FTreeMapNodeData>> VisitedAssetPackageNames;
	TSharedPtr<FTreeMapNodeData> SharedRootNode;
	int32 NumAssetsWhichFailedToLoad = 0;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	GatherDependenciesRecursively( AssetRegistryModule, AssetThumbnailPool, VisitedAssetPackageNames, RootAssetPackageNames, RootTreeMapNode, SharedRootNode, NumAssetsWhichFailedToLoad );


	// Next, do another pass over our tree to and count how big the assets are and to set the node labels.  Also in this pass, we may
	// create some additional "self" nodes for assets that have children but also take up size themselves.
	int32 TotalAssetCount = 0;
	SIZE_T TotalSize = 0;
	bool bAnyUnknownSizes = false;
	FinalizeNodesRecursively( RootTreeMapNode, SharedRootNode, TotalAssetCount, TotalSize, bAnyUnknownSizes );


	// Create a nice name for the tree!
	if( NumAssetsWhichFailedToLoad > 0 )
	{
		RootTreeMapNode->Name = FString::Printf( TEXT( "%s %i %s" ),
			*LOCTEXT( "RootNode_WarningPrefix", "WARNING:" ).ToString(),
			NumAssetsWhichFailedToLoad,
			*LOCTEXT( "RootNode_NAssetsFailedToLoad", "assets were missing!  Only partial results shown." ).ToString() );
	}
	else if( RootAssetPackageNames.Num() == 1 && !SharedRootNode.IsValid() )
	{
		// @todo sizemap: When zoomed right into one asset, can we use the Class color for the node instead of grey?

		FString OnlyAssetName = RootAssetPackageNames[ 0 ].ToString();
		if( RootTreeMapNode->Children.Num() > 0 )
		{
			// The root will only have one child, so go ahead and use that child as the actual root
			FTreeMapNodeDataPtr OnlyChild = RootTreeMapNode->Children[ 0 ];
			OnlyChild->CopyNodeInto( *RootTreeMapNode );
			RootTreeMapNode->Children = OnlyChild->Children;
			RootTreeMapNode->Parent = nullptr;
			for( const auto& ChildNode : RootTreeMapNode->Children )
			{
				ChildNode->Parent = RootTreeMapNode.Get();
			}

			OnlyAssetName = OnlyChild->Name;
		}

		// Use a more descriptive name for the root level node
		RootTreeMapNode->Name = FString::Printf( TEXT( "%s %s  (%i %s)" ),
			*LOCTEXT( "RootNode_SizeMapForOneAsset", "Size map for" ).ToString(),
			*OnlyAssetName,
			TotalAssetCount,
			*LOCTEXT( "RootNode_References", "total assets" ).ToString() );
	}
	else
	{
		// Multiple assets (or at least some shared assets) at the root level
		RootTreeMapNode->BackgroundBrush = nullptr;
		RootTreeMapNode->Size = 0.0f;
		RootTreeMapNode->Parent = nullptr;
		RootTreeMapNode->Name = FString::Printf( TEXT( "%s %i %s  (%i %s, %s)" ),
			*LOCTEXT( "RootNode_SizeMapForMultiple", "Size map for" ).ToString(),
			RootAssetPackageNames.Num(),
			*LOCTEXT( "RootNode_Assets", "assets" ).ToString(),
			TotalAssetCount,
			*LOCTEXT( "RootNode_References", "total assets" ).ToString(),
			*SizeMapInternals::MakeBestSizeString( TotalSize, !bAnyUnknownSizes ) );
	}

						
	// OK, now refresh the actual tree map widget so our new tree will be displayed.
	const bool bShouldPlayTransition = false;
	TreeMapWidget->RebuildTreeMap( bShouldPlayTransition );
}


void SSizeMap::OnInitialAssetRegistrySearchComplete()
{
	RefreshMap();
}


void SSizeMap::OnTreeMapNodeDoubleClicked( FTreeMapNodeData& TreeMapNodeData )
{
	if (SelectAssetOnDoubleClick)
	{
		const FNodeSizeMapData* NodeSizeMapData = NodeSizeMapDataMap.Find(TreeMapNodeData.AsShared());
		if (NodeSizeMapData != nullptr)
		{
			TArray<FAssetData> Assets;
			Assets.Add(NodeSizeMapData->AssetData);
			GEditor->SyncBrowserToObjects(Assets);
		}
	}
}



// @todo sizemap urgent: We should add a spinner while we discover and load assets
// @todo sizemap urgent: Audit common asset types and make sure they have a useful GetResourceSize() implementation
//		-> Some implementations are including the size of editor-only data (Static Mesh).  This should be configurable!
// @todo sizemap urgent: It would be great to be able to see 0-sized/unknown-sized/tiny-sized nodes somehow, or at least a count of them
//		- You'd almost want a way to zoom in super small.  Or a little laserpointer effect that shows labels for super-tiny nodes
// @todo sizemap urgent: Need inline help to figure out mousewheel?  Single click currently does nothing.  
// @todo sizemap urgent: When an asset is not reporting a size, this tool should help to escalate that by making it very obvious that its missing (no GetResourceSize function)
// @todo sizemap: Can "zoom into" <Self> nodes which is a bit weird.  (Asset name no longer draws).  Maybe disallow zooming all the way in?
//  --> Double-click current zooms directly into a single asset. Not super useful.  Maybe only allow zooming down to the deepest nodes, not leaves
// @todo sizemap: When the tab is restored from layout, it will be totally empty.  Instead it should probably have instructions for how to show the sizes for assets. (Same with Reference Viewer)
// @todo sizemap: We ideally want to replace the SReferenceTree code with SSizeMap
// @todo sizemap: Add a tree view that shows all of the references along with their sizes (so you can see Very Small references)
// @todo sizemap: It would be useful to be able to preview the sizes for specific platforms? (option in the UI)
// @todo sizemap: Should we show the percentage of total size as an option in the UI (though, the size of the boxes show this pretty well.)
// @todo sizemap: Should we show the folder part of the asset name as a tool-tip?
// @todo sizemap: It might be nice to unload assets that were loaded by us after the size map is built up
// @todo sizemap: Add a Refresh button. (Also, try to detect when objects change and auto-refresh, optionally)


#undef LOCTEXT_NAMESPACE
