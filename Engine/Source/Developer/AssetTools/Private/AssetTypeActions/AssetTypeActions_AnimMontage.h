// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_AnimationAsset.h"
#include "Animation/AnimMontage.h"

class FMenuBuilder;

class FAssetTypeActions_AnimMontage : public FAssetTypeActions_AnimationAsset
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimMontage", "Animation Montage"); }
	virtual FColor GetTypeColor() const override { return FColor(100,100,255); }
	virtual UClass* GetSupportedClass() const override { return UAnimMontage::StaticClass(); }
	virtual bool CanFilter() override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

private:
   	/* 
	 * Child Anim Montage: Child Anim Montage only can replace name of animations, and no other meaningful edits 
	 * as it will derive every data from Parent. There might be some other data that will allow to be replaced, but for now, it is
	 * not. 
	 */
	void CreateChildAnimMontage(TArray<TWeakObjectPtr<UAnimMontage>> AnimMontages);
};
