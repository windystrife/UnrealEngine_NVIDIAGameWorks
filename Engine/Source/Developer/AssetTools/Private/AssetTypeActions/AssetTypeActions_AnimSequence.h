// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Animation/AnimationAsset.h"
#include "AssetTypeActions/AssetTypeActions_AnimationAsset.h"

class FMenuBuilder;
class UAnimSequence;
class UFactory;

/** Delegate used when creating Assets from an AnimSequence */
DECLARE_DELEGATE_TwoParams(FOnConfigureFactory, class UFactory*, class UAnimSequence* );

class FAssetTypeActions_AnimSequence : public FAssetTypeActions_AnimationAsset
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimSequence", "Animation Sequence"); }
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual bool CanFilter() override { return true; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;

private:
	/** Handler for when OpenInExternalEditor is selected */
	void ExecuteReimportWithNewSource(TArray<TWeakObjectPtr<UAnimSequence>> Objects);

	/** Handler for when Create AnimComposite is selected */
	void ExecuteNewAnimComposite(TArray<TWeakObjectPtr<UAnimSequence>> Objects) const;

	/** Handler for when Create AnimMontage is selected */
	void ExecuteNewAnimMontage(TArray<TWeakObjectPtr<UAnimSequence>> Objects) const;

	/** Handler for when Create Pose Asset is selected */
	void ExecuteNewPoseAsset(TArray<TWeakObjectPtr<UAnimSequence>> Objects) const;

	/** Delegate handler passed to CreateAnimationAssets when creating AnimComposites */
	void ConfigureFactoryForAnimComposite(UFactory* AssetFactory, UAnimSequence* SourceAnimation) const;

	/** Delegate handler passed to CreateAnimationAssets when creating AnimMontages */
	void ConfigureFactoryForAnimMontage(UFactory* AssetFactory, UAnimSequence* SourceAnimation) const;

	/** Delegate handler passed to CreateAnimationAssets when creating PoseAssets */
	void ConfigureFactoryForPoseAsset(UFactory* AssetFactory, UAnimSequence* SourceAnimation) const;

	/** Creates animation assets of the supplied class */
	void CreateAnimationAssets(const TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences, TSubclassOf<UAnimationAsset> AssetClass, UFactory* AssetFactory, const FString& InSuffix, FOnConfigureFactory OnConfigureFactory) const;

	void FillCreateMenu(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<UAnimSequence>> Sequences) const;
};
