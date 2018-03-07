// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "IPropertyTypeCustomization.h"

class FDetailWidgetRow;
class IPropertyHandle;

class FCameraLensSettingsCustomization : public IPropertyTypeCustomization
{
public:
	FCameraLensSettingsCustomization();

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization instance */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

protected:

	TSharedPtr<IPropertyHandle> MinFocalLengthHandle;
	TSharedPtr<IPropertyHandle> MaxFocalLengthHandle;
	TSharedPtr<IPropertyHandle> MinFStopHandle;
	TSharedPtr<IPropertyHandle> MaxFStopHandle;
	TSharedPtr<IPropertyHandle> MinFocusDistanceHandle;
	TSharedPtr<IPropertyHandle> MaxReproRatioHandle;

	TSharedPtr<class SComboBox< TSharedPtr<FString> > > PresetComboBox;
	TArray< TSharedPtr< FString > >						PresetComboList;

	void OnPresetChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> MakePresetComboWidget(TSharedPtr<FString> InItem);

	FText GetPresetComboBoxContent() const;
	TSharedPtr<FString> GetPresetString() const;

protected:
};
