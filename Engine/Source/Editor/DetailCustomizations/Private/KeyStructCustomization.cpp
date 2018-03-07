// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "KeyStructCustomization.h"
#include "DetailWidgetRow.h"
#include "SKeySelector.h"
#include "UnrealType.h"

#define LOCTEXT_NAMESPACE "FKeyStructCustomization"


/* FKeyStructCustomization static interface
 *****************************************************************************/

TSharedRef<IPropertyTypeCustomization> FKeyStructCustomization::MakeInstance( )
{
	return MakeShareable(new FKeyStructCustomization);
}


/* IPropertyTypeCustomization interface
 *****************************************************************************/

void FKeyStructCustomization::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	PropertyHandle = StructPropertyHandle;

	// create struct header
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f)
	.MaxDesiredWidth(325.0f)
	[
		SNew(SKeySelector)
		.CurrentKey(this, &FKeyStructCustomization::GetCurrentKey)
		.OnKeyChanged(this, &FKeyStructCustomization::OnKeyChanged)
		.Font(StructCustomizationUtils.GetRegularFont())
		.AllowClear(!StructPropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_NoClear))
	];
}

TOptional<FKey> FKeyStructCustomization::GetCurrentKey() const
{
	TArray<void*> StructPtrs;
	PropertyHandle->AccessRawData(StructPtrs);

	if (StructPtrs.Num() > 0)
	{
		FKey* SelectedKey = (FKey*)StructPtrs[0];

		if (SelectedKey)
		{
			for(int32 StructPtrIndex = 1; StructPtrIndex < StructPtrs.Num(); ++StructPtrIndex)
			{
				if (*(FKey*)StructPtrs[StructPtrIndex] != *SelectedKey)
				{
					return TOptional<FKey>();
				}
			}

			return *SelectedKey;
		}
	}

	return FKey();
}

void FKeyStructCustomization::OnKeyChanged(TSharedPtr<FKey> SelectedKey)
{
	PropertyHandle->SetValueFromFormattedString(SelectedKey->ToString());
}

#undef LOCTEXT_NAMESPACE
