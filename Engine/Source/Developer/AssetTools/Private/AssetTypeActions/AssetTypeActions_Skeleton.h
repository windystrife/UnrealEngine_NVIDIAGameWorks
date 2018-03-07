// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/Skeleton.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "EditorAnimUtils.h"

class FMenuBuilder;

/**
 * Remap Skeleton Asset Data
 */
struct FAssetToRemapSkeleton
{
	FName					PackageName;
	TWeakObjectPtr<UObject> Asset;
	FText					FailureReason;
	bool					bRemapFailed;

	FAssetToRemapSkeleton(FName InPackageName)
		: PackageName(InPackageName)
		, bRemapFailed(false)
	{}

	// report it failed
	void ReportFailed(const FText& InReason)
	{
		FailureReason = InReason;
		bRemapFailed = true;
	}
};

class FAssetTypeActions_Skeleton : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Skeleton", "Skeleton"); }
	virtual FColor GetTypeColor() const override { return FColor(105,181,205); }
	virtual UClass* GetSupportedClass() const override { return USkeleton::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }

private:
	/** Handler for when Skeleton Retarget is selected */
	void ExecuteRetargetSkeleton(TArray<TWeakObjectPtr<USkeleton>> Skeletons);

	/** Handler for when Create Rig is selected */
	void ExecuteCreateRig(TArray<TWeakObjectPtr<USkeleton>> Skeletons);

private: // Helper functions
	/** Creates animation assets using the BaseName+Suffix */
	void CreateRig(const TWeakObjectPtr<USkeleton> Skeleton);

 	/** 
	 * Main function for handling retargeting old Skeleton to new Skeleton
	 * 
	 * @param OldSkeleton : Old Skeleton that it changes from
	 * @param NewSkeleton : New Skeleton that it would change to
	 * @param Referencers : List of asset names that reference Old Skeleton
	 * @param bConvertSpaces : Whether to convert spaces or not
	 */
 	void PerformRetarget(USkeleton *OldSkeleton, USkeleton *NewSkeleton, TArray<FName> Packages, bool bConvertSpaces) const;

	// utility functions for performing retargeting
	// these codes are from AssetRenameManager workflow
	void LoadPackages(TArray<FAssetToRemapSkeleton>& AssetsToRemap, TArray<UPackage*>& OutPackagesToSave) const;
	bool CheckOutPackages(TArray<FAssetToRemapSkeleton>& AssetsToRemap, TArray<UPackage*>& InOutPackagesToSave) const;
	void ReportFailures(const TArray<FAssetToRemapSkeleton>& AssetsToRemap) const;
	void RetargetSkeleton(TArray<FAssetToRemapSkeleton>& AssetsToRemap, USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bConvertSpaces) const;
	void SavePackages(const TArray<UPackage*> PackagesToSave) const;
	void DetectReadOnlyPackages(TArray<FAssetToRemapSkeleton>& AssetsToRemap, TArray<UPackage*>& InOutPackagesToSave) const;
	void FillCreateMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeleton>> Skeletons) const;
	void OnAssetCreated(TArray<UObject*> NewAssets) const;

	/** Handler for retargeting */
	void RetargetAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bAllowRemapToExisting, bool bConvertSpaces, const EditorAnimUtils::FNameDuplicationRule* NameRule);
};
