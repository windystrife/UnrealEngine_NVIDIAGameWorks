// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

#include "Templates/Function.h"
#include "DetailWidgetRow.h"
#include "SAnimationBlendSpaceGridWidget.h"

struct FAssetData;
class FDetailWidgetRow;
class UBlendSpaceBase;

class FBlendSampleDetails : public IDetailCustomization
{
public:
	FBlendSampleDetails(const class UBlendSpaceBase* InBlendSpace, class SBlendSpaceGridWidget* InGridWidget);

	static TSharedRef<IDetailCustomization> MakeInstance(const class UBlendSpaceBase* InBlendSpace, class SBlendSpaceGridWidget* InGridWidget)
	{
		return MakeShareable( new FBlendSampleDetails(InBlendSpace, InGridWidget) );
	}

	// Begin IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
	// End IDetailCustomization interface
	
	static void GenerateBlendSampleWidget(TFunction<FDetailWidgetRow& (void)>InFunctor, FOnSampleMoved OnSampleMoved, const class UBlendSpaceBase* BlendSpace, const int32 SampleIndex, bool bShowLabel);

	static void GenerateAnimationWidget(FDetailWidgetRow& Row, const class UBlendSpaceBase* BlendSpace, TSharedPtr<IPropertyHandle> AnimationProperty);

	static bool ShouldFilterAssetStatic(const FAssetData& AssetData, const class UBlendSpaceBase* BlendSpaceBase);

protected:
	/** Checks whether or not the specified asset should not be shown in the mini content browser when changing the animation */
	bool ShouldFilterAsset(const FAssetData& AssetData) const;
private:
	/** Pointer to the current parent blend space for the customized blend sample*/
	const class UBlendSpaceBase* BlendSpace;
	/** Parent grid widget object */
	SBlendSpaceGridWidget* GridWidget;
	/** Cached flags to check whether or not an additive animation type is compatible with the blend space*/	
	TMap<FString, bool> bValidAdditiveTypes;
};
