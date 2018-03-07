// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CollectionManagerTypes.h"
#include "ISourceControlProvider.h"
#include "IAssetTools.h"
#include "Misc/TextFilterExpressionEvaluator.h"
#include "FrontendFilterBase.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

class FMenuBuilder;

/** A filter for text search */
class FFrontendFilter_Text : public FFrontendFilter
{
public:
	FFrontendFilter_Text();
	~FFrontendFilter_Text();

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("TextFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_Text", "Text"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_TextTooltip", "Show only assets that match the input text"); }

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

public:
	/** Returns the unsanitized and unsplit filter terms */
	FText GetRawFilterText() const;

	/** Set the Text to be used as the Filter's restrictions */
	void SetRawFilterText(const FText& InFilterText);

	/** Get the last error returned from lexing or compiling the current filter text */
	FText GetFilterErrorText() const;

	/** If bIncludeClassName is true, the text filter will include an asset's class name in the search */
	void SetIncludeClassName(const bool InIncludeClassName);

private:
	/** Handles an on collection created event */
	void HandleCollectionCreated(const FCollectionNameType& Collection);

	/** Handles an on collection destroyed event */
	void HandleCollectionDestroyed(const FCollectionNameType& Collection);

	/** Handles an on collection renamed event */
	void HandleCollectionRenamed(const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection);

	/** Handles an on collection updated event */
	void HandleCollectionUpdated(const FCollectionNameType& Collection);

	/** Rebuild the array of dynamic collections that are being referenced by the current query */
	void RebuildReferencedDynamicCollections();

	/** An array of dynamic collections that are being referenced by the current query. These should be tested against each asset when it's looking for collections that contain it */
	TArray<FCollectionNameType> ReferencedDynamicCollections;

	/** Transient context data, used when calling PassesFilter. Kept around to minimize re-allocations between multiple calls to PassesFilter */
	TSharedRef<class FFrontendFilter_TextFilterExpressionContext> TextFilterExpressionContext;

	/** Expression evaluator that can be used to perform complex text filter queries */
	FTextFilterExpressionEvaluator TextFilterExpressionEvaluator;

	/** Delegate handles */
	FDelegateHandle OnCollectionCreatedHandle;
	FDelegateHandle OnCollectionDestroyedHandle;
	FDelegateHandle OnCollectionRenamedHandle;
	FDelegateHandle OnCollectionUpdatedHandle;
};

/** A filter that displays only checked out assets */
class FFrontendFilter_CheckedOut : public FFrontendFilter, public TSharedFromThis<FFrontendFilter_CheckedOut>
{
public:
	FFrontendFilter_CheckedOut(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("CheckedOut"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_CheckedOut", "Checked Out"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_CheckedOutTooltip", "Show only assets that you have checked out or pending for add."); }
	virtual void ActiveStateChanged(bool bActive) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
	
	/** Request the source control status for this filter */
	void RequestStatus();

	/** Callback when source control operation has completed */
	void SourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);
};

/** A filter that displays only modified assets */
class FFrontendFilter_Modified : public FFrontendFilter
{
public:
	FFrontendFilter_Modified(TSharedPtr<FFrontendFilterCategory> InCategory);
	~FFrontendFilter_Modified();

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("Modified"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_Modified", "Modified"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_ModifiedTooltip", "Show only assets that have been modified and not yet saved."); }
	virtual void ActiveStateChanged(bool bActive) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:

	/** Handler for when a package's dirty state has changed */
	void OnPackageDirtyStateUpdated(UPackage* Package);

	bool bIsCurrentlyActive;
};

/** A filter that displays blueprints that have replicated properties */
class FFrontendFilter_ReplicatedBlueprint : public FFrontendFilter
{
public:
	FFrontendFilter_ReplicatedBlueprint(TSharedPtr<FFrontendFilterCategory> InCategory) : FFrontendFilter(InCategory) {}

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("ReplicatedBlueprint"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FFrontendFilter_ReplicatedBlueprint", "Replicated Blueprints"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FFrontendFilter_ReplicatedBlueprintToolTip", "Show only blueprints with replicated properties."); }

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;
};

/** A filter that compares the value of an asset registry tag to a target value */
class FFrontendFilter_ArbitraryComparisonOperation : public FFrontendFilter
{
public:
	FFrontendFilter_ArbitraryComparisonOperation(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTipText() const override;
	virtual void ModifyContextMenu(FMenuBuilder& MenuBuilder) override;
	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const override;
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

protected:
	static FString ConvertOperationToString(ETextFilterComparisonOperation Op);
	
	void SetComparisonOperation(ETextFilterComparisonOperation NewOp);
	bool IsComparisonOperationEqualTo(ETextFilterComparisonOperation TestOp) const;

	FText GetKeyValueAsText() const;
	FText GetTargetValueAsText() const;
	void OnKeyValueTextCommitted(const FText& InText, ETextCommit::Type InCommitType);
	void OnTargetValueTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

public:
	FName TagName;
	FString TargetTagValue;
	ETextFilterComparisonOperation ComparisonOp;
};

/** An inverse filter that allows display of content in developer folders that are not the current user's */
class FFrontendFilter_ShowOtherDevelopers : public FFrontendFilter
{
public:
	/** Constructor */
	FFrontendFilter_ShowOtherDevelopers(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("ShowOtherDevelopers"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_ShowOtherDevelopers", "Other Developers"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_ShowOtherDevelopersTooltip", "Allow display of assets in developer folders that aren't yours."); }
	virtual bool IsInverseFilter() const override { return true; }
	virtual void SetCurrentFilter(const FARFilter& InFilter) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

public:
	/** Sets if we should filter out assets from other developers */
	void SetShowOtherDeveloperAssets(bool bValue);

	/** Gets if we should filter out assets from other developers */
	bool GetShowOtherDeveloperAssets() const;

private:
	FString BaseDeveloperPath;
	FString UserDeveloperPath;
	bool bIsOnlyOneDeveloperPathSelected;
	bool bShowOtherDeveloperAssets;
};

/** An inverse filter that allows display of object redirectors */
class FFrontendFilter_ShowRedirectors : public FFrontendFilter
{
public:
	/** Constructor */
	FFrontendFilter_ShowRedirectors(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("ShowRedirectors"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_ShowRedirectors", "Show Redirectors"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_ShowRedirectorsToolTip", "Allow display of Redirectors."); }
	virtual bool IsInverseFilter() const override { return true; }
	virtual void SetCurrentFilter(const FARFilter& InFilter) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
	bool bAreRedirectorsInBaseFilter;
	FName RedirectorClassName;
};

/** A filter that only displays assets used by loaded levels */
class FFrontendFilter_InUseByLoadedLevels : public FFrontendFilter
{
public:
	/** Constructor/Destructor */
	FFrontendFilter_InUseByLoadedLevels(TSharedPtr<FFrontendFilterCategory> InCategory);
	~FFrontendFilter_InUseByLoadedLevels();

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("InUseByLoadedLevels"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_InUseByLoadedLevels", "In Use By Level"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_InUseByLoadedLevelsToolTip", "Show only assets that are currently in use by any loaded level."); }
	virtual void ActiveStateChanged(bool bActive) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

	/** Handler for when maps change in the editor */
	void OnEditorMapChange( uint32 MapChangeFlags );

	/** Handler for when an asset is renamed */
	void OnAssetPostRename(const TArray<FAssetRenameData>& AssetsAndNames);

private:
	bool bIsCurrentlyActive;
};


/** A filter that only displays assets used by any level */
class FFrontendFilter_UsedInAnyLevel: public FFrontendFilter
{
public:
	/** Constructor/Destructor */
	FFrontendFilter_UsedInAnyLevel(TSharedPtr<FFrontendFilterCategory> InCategory);
	virtual ~FFrontendFilter_UsedInAnyLevel();

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("UsedInAnyLevel"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FFrontendFilter_UsedInAnyLevel", "Used In Any Level"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FFrontendFilter_UsedInAnyLevelTooltip", "Show only assets that are used in any level."); }
	virtual void ActiveStateChanged(bool bActive) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
	class IAssetRegistry* AssetRegistry;
	TSet<FName> LevelsDependencies;
};

/** A filter that only displays assets used by any level */
class FFrontendFilter_NotUsedInAnyLevel : public FFrontendFilter
{
public:
	/** Constructor/Destructor */
	FFrontendFilter_NotUsedInAnyLevel(TSharedPtr<FFrontendFilterCategory> InCategory);
	virtual ~FFrontendFilter_NotUsedInAnyLevel();

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("NotUsedInAnyLevel"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FFrontendFilter_NotUsedInAnyLevel", "Not Used In Any Level"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FFrontendFilter_NotUsedInAnyLevelTooltip", "Show only assets that are not used in any level."); }
	virtual void ActiveStateChanged(bool bActive) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
	class IAssetRegistry* AssetRegistry;
	TSet<FName> LevelsDependencies;
};

#undef LOCTEXT_NAMESPACE
