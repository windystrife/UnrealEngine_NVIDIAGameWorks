// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraTypeCustomizations.h"
#include "CoreMinimal.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "NiagaraTypes.h"
#include "STextBlock.h"
#include "SCheckBox.h"
#include "IDetailChildrenBuilder.h"



void FNiagaraNumericCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(ValueHandle.IsValid() ? 125.f : 200.f)
		[
			// Some Niagara numeric types have no value so in that case just display their type name
			ValueHandle.IsValid()
			? ValueHandle->CreatePropertyValueWidget()
			: SNew(STextBlock)
			  .Text(FText::FromString(FName::NameToDisplayString(Cast<UStructProperty>(PropertyHandle->GetProperty())->Struct->GetName(), false)))
			  .Font(IDetailLayoutBuilder::GetDetailFont())
		];
}


void FNiagaraBoolCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));

	static const FName DefaultForegroundName("DefaultForeground");

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &FNiagaraBoolCustomization::OnCheckStateChanged)
			.IsChecked(this, &FNiagaraBoolCustomization::OnGetCheckState)
			.ForegroundColor(FEditorStyle::GetSlateColor(DefaultForegroundName))
			.Padding(0.0f)
		];
}

ECheckBoxState FNiagaraBoolCustomization::OnGetCheckState() const
{
	ECheckBoxState CheckState = ECheckBoxState::Undetermined;
	int32 Value;
	FPropertyAccess::Result Result = ValueHandle->GetValue(Value);
	if (Result == FPropertyAccess::Success)
	{
		CheckState = Value == FNiagaraBool::True ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return CheckState;
}

void FNiagaraBoolCustomization::OnCheckStateChanged(ECheckBoxState InNewState)
{
	if (InNewState == ECheckBoxState::Checked)
	{
		ValueHandle->SetValue(FNiagaraBool::True);
	}
	else
	{
		ValueHandle->SetValue(FNiagaraBool::False);
	}
}

void FNiagaraMatrixCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildNum = 0; ChildNum < NumChildren; ++ChildNum)
	{
		ChildBuilder.AddProperty(PropertyHandle->GetChildHandle(ChildNum).ToSharedRef());
	}
}
