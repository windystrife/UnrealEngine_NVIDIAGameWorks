// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Developer/Merge/Public/Merge.h"
#include "ThumbnailRendering/ThumbnailManager.h"

class IToolkitHost;

namespace EAssetTypeActivationMethod
{
	enum Type
	{
		DoubleClicked,
		Opened,
		Previewed
	};
}

class IToolkitHost;

/* Revision information for a single revision of a file in source control */
struct FRevisionInfo
{
	FString		Revision;
	int32		Changelist;
	FDateTime	Date;	

	static inline FRevisionInfo InvalidRevision()
	{
		static const FRevisionInfo Ret = { TEXT(""), -1, FDateTime() };
		return Ret;
	}
};

/** AssetTypeActions provide actions and other information about asset types */
class IAssetTypeActions : public TSharedFromThis<IAssetTypeActions>
{
public:
	/** Virtual destructor */
	virtual ~IAssetTypeActions(){}

	/** Returns the name of this type */
	virtual FText GetName() const = 0;

	/** Checks to see if the specified object is handled by this type. */
	virtual UClass* GetSupportedClass() const = 0;

	/** Returns the color associated with this type */
	virtual FColor GetTypeColor() const = 0;

	/** Returns true if this class can supply actions for InObjects. */
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const = 0;

	/** Generates a menubuilder for the specified objects. */
	virtual void GetActions( const TArray<UObject*>& InObjects, class FMenuBuilder& MenuBuilder ) = 0;

	/** Opens the asset editor for the specified objects. If EditWithinLevelEditor is valid, the world-centric editor will be used. */
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) = 0;

	/** Performs asset type specific activation for the supplied assets. This happens when the user double clicks, presses enter, or presses space. */
	virtual void AssetsActivated( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType ) = 0;

	/** Returns true if this class can be used as a filter in the content browser */
	virtual bool CanFilter() = 0;

	/** Returns true if this class can be localized */
	virtual bool CanLocalize() const = 0;

	/** Returns true if this class can be merged (either manually or automatically) */
	virtual bool CanMerge() const = 0;

	/** Begins a merge operation for InObject (automatically determines remote/base versions needed to resolve) */
	virtual void Merge( UObject* InObject ) = 0;

	/** Begins a merge between the specified assets */
	virtual void Merge(UObject* BaseAsset, UObject* RemoteAsset, UObject* LocalAsset, const FOnMergeResolved& ResolutionCallback) = 0;

	/** Returns the categories that this asset type. The return value is one or more flags from EAssetTypeCategories.  */
	virtual uint32 GetCategories() = 0;

	/** @return True if we should force world-centric mode for newly-opened assets */
	virtual bool ShouldForceWorldCentric() = 0;

	/** Performs asset-specific diff on the supplied asset */
	virtual void PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const = 0;

	/** Returns the thumbnail info for the specified asset, if it has one. */
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const = 0;

	/** Returns the default thumbnail type that should be rendered when rendering primitive shapes.  This does not need to be implemented if the asset does not render a primitive shape */
	virtual EThumbnailPrimType GetDefaultThumbnailPrimitiveType(UObject* Asset) const = 0;

	/** Optionally returns a custom widget to overlay on top of this assets' thumbnail */
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const struct FAssetData& AssetData) const = 0;

	/** Returns additional tooltip information for the specified asset, if it has any (otherwise return the null widget) */
	virtual FText GetAssetDescription(const struct FAssetData& AssetData) const = 0;

	/** Returns whether the asset was imported from an external source */
	virtual bool IsImportedAsset() const = 0;

	/** Collects the resolved source paths for the imported assets */
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const = 0;
};
