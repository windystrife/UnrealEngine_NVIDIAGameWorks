// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "PropertyHandle.h"
#include "IDetailCustomization.h"

struct FAssetData;

/////////////////////////////////////////////////////
// FAnimInstanceDetails 

class FAnimInstanceDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
	// End of IDetailCustomization interface

protected:

	// Creates a filtered object widget if the supplied property is an object property
	TSharedRef<SWidget> CreateFilteredObjectPropertyWidget(UProperty* TargetProperty, TSharedRef<IPropertyHandle> TargetPropertyHandle);

	/** Delegate to handle filtering of asset pickers */
	bool OnShouldFilterAnimAsset( const FAssetData& AssetData ) const;

	/** Path to the current blueprints skeleton to allow us to filter asset pickers */
	FString TargetSkeletonName;
};
