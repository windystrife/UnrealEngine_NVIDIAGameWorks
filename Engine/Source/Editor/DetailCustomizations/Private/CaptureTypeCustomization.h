// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "IPropertyUtilities.h"
#include "MovieSceneCaptureProtocolRegistry.h"

class FDetailWidgetRow;
class IDetailChildrenBuilder;
class STextBlock;

class FCaptureTypeCustomization : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils ) override {}

private:
	struct FCaptureProtocolType
	{
		FCaptureProtocolID ID;
		FText DisplayName;
	};

	void OnPropertyChanged(TSharedPtr<FCaptureProtocolType> CaptureType, ESelectInfo::Type);

	void SetCurrentIndex(FName ID);
	
private:
	/** Array of available capture types */
	TArray<TSharedPtr<FCaptureProtocolType>> CaptureTypes;
	/** The index of the type we're currently displaying */
	int32 CurrentIndex;
	/** The text of the current selection */
	TSharedPtr<STextBlock> CurrentText;
	/** Property handle of the property we're editing */
	TSharedPtr<IPropertyHandle> PropertyHandle;
	/** Property utilites */
	TSharedPtr<class IPropertyUtilities> PropertyUtilities;
};
