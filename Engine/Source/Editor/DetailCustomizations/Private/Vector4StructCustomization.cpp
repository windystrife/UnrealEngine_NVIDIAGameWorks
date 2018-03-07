// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "Vector4StructCustomization.h"
#include "IPropertyUtilities.h"
#include "SNumericEntryBox.h"
#include "SColorGradingPicker.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
TSharedRef<IPropertyTypeCustomization> FVector4StructCustomization::MakeInstance()
{
	return MakeShareable(new FVector4StructCustomization);
}

FVector4StructCustomization::FVector4StructCustomization()
	: FMathStructCustomization()
	, ColorGradingVectorCustomization(nullptr)
{
}

FVector4StructCustomization::~FVector4StructCustomization()
{
	//Release all resources
	ColorGradingVectorCustomization = nullptr;
}

void FVector4StructCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	UProperty* Property = StructPropertyHandle->GetProperty();
	if (Property)
	{
		const FString& ColorGradingModeString = Property->GetMetaData(TEXT("ColorGradingMode"));
		if (!ColorGradingModeString.IsEmpty())
		{
			//Create our color grading customization shared pointer
			TSharedPtr<FColorGradingVectorCustomization> ColorGradingCustomization = GetOrCreateColorGradingVectorCustomization(StructPropertyHandle);

			//Customize the childrens
			ColorGradingVectorCustomization->CustomizeChildren(StructBuilder, StructCustomizationUtils);
			
			// We handle the customize Children so just return here
			return;
		}
	}

	//Use the base class customize children
	FMathStructCustomization::CustomizeChildren(StructPropertyHandle, StructBuilder, StructCustomizationUtils);
}

TSharedPtr<FColorGradingVectorCustomization> FVector4StructCustomization::GetOrCreateColorGradingVectorCustomization(TSharedRef<IPropertyHandle>& StructPropertyHandle)
{
	//Create our color grading customization shared pointer
	if (!ColorGradingVectorCustomization.IsValid())
	{
		TArray<TWeakPtr<IPropertyHandle>> WeakChildArray;
		WeakChildArray.Append(SortedChildHandles);

		ColorGradingVectorCustomization = MakeShareable(new FColorGradingVectorCustomization(StructPropertyHandle, WeakChildArray));
	}

	return ColorGradingVectorCustomization;
}

void FVector4StructCustomization::MakeHeaderRow(TSharedRef<IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row)
{
	UProperty* Property = StructPropertyHandle->GetProperty();
	if (Property)
	{
		const FString& ColorGradingModeString = Property->GetMetaData(TEXT("ColorGradingMode"));
		if (!ColorGradingModeString.IsEmpty())
		{
			//Create our color grading customization shared pointer
			TSharedPtr<FColorGradingVectorCustomization> ColorGradingCustomization = GetOrCreateColorGradingVectorCustomization(StructPropertyHandle);

			ColorGradingVectorCustomization->MakeHeaderRow(Row, StaticCastSharedRef<FVector4StructCustomization>(AsShared()));

			// We handle the customize Children so just return here
			return;
		}
	}

	//Use the base class header row
	FMathStructCustomization::MakeHeaderRow(StructPropertyHandle, Row);
}
