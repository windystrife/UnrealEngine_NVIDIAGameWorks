// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "Animation/AnimationAsset.h"
#include "EditorAnimUtils.h"

class FMenuBuilder;

class FAssetTypeActions_AnimationAsset : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimationAsset", "AnimationAsset"); }
	virtual FColor GetTypeColor() const override { return FColor(80,123,72); }
	virtual UClass* GetSupportedClass() const override { return UAnimationAsset::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;

private:
	/** Handler to fill the retarget submenu */
	void FillRetargetMenu( FMenuBuilder& MenuBuilder, const TArray<UObject*> InObjects );

	/** Handler for when FindSkeleton is selected */
	void ExecuteFindSkeleton(TArray<TWeakObjectPtr<UAnimationAsset>> Objects);

	/** Handle menu item for wanting to open asset in a new editor */
	void ExecuteOpenInNewWindow(TArray<TWeakObjectPtr<UAnimationAsset>> Objects);

	/** Open animation asset, will find existing editor if desired. */
	void OpenAnimAssetEditor(const TArray<UObject*>& InObjects, bool bForceNewEditor, TSharedPtr<IToolkitHost> EditWithinLevelEditor);

	/** Context menu item handler for changing the supplied assets skeletons */ 
	void RetargetAssets(TArray<UObject*> InAnimBlueprints, bool bDuplicateAssets, bool bOpenEditor, TSharedPtr<class IToolkitHost> EditWithinLevelEditor);

	/** Handler for retargeting */
	void RetargetAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bAllowRemapToExisting, bool bConvertSpaces, const EditorAnimUtils::FNameDuplicationRule* NameRule, TArray<TWeakObjectPtr<UObject>> InAnimAssets);
	void RetargetNonSkeletonAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bAllowRemapToExisting, bool bConvertSpaces, const EditorAnimUtils::FNameDuplicationRule* NameRule, TArray<TWeakObjectPtr<UObject>> InAnimAssets, TWeakPtr<class IToolkitHost> EditWithinLevelEditor);
};
