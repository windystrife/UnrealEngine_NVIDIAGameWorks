// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "UnrealClient.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
struct FAIDataProviderValue;

class FAIDataProviderValueDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	TSharedPtr<IPropertyHandle> DataBindingProperty;
	TSharedPtr<IPropertyHandle> DataFieldProperty;
	TSharedPtr<IPropertyHandle> DefaultValueProperty;
	FAIDataProviderValue* DataPtr;

	TArray<FName> MatchingProperties;

	void OnBindingChanged();
	TSharedRef<SWidget> OnGetDataFieldContent();
	void OnDataFieldNameChange(int32 Index);
	FText GetDataFieldDesc() const;
	FText GetValueDesc() const;
	EVisibility GetBindingDescVisibility() const;
	EVisibility GetDataFieldVisibility() const;
	EVisibility GetDefaultValueVisibility() const;
};
