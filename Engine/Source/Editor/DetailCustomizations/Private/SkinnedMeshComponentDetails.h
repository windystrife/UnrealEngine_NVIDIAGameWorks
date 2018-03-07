// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class FDetailWidgetRow;
class IDetailLayoutBuilder;
class UPhysicsAsset;

class FSkinnedMeshComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	void CreateActuallyUsedPhysicsAssetWidget(FDetailWidgetRow& OutWidgetRow, IDetailLayoutBuilder* DetailBuilder) const;

	FText GetUsedPhysicsAssetAsText(IDetailLayoutBuilder* DetailBuilder) const;
	void BrowseUsedPhysicsAsset(IDetailLayoutBuilder* DetailBuilder) const;

	bool FindUniqueUsedPhysicsAsset(IDetailLayoutBuilder* DetailBuilder, UPhysicsAsset*& OutFoundPhysicsAsset) const;
};
