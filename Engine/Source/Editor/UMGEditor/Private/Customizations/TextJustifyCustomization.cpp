// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Customizations/TextJustifyCustomization.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "DetailWidgetRow.h"


#define LOCTEXT_NAMESPACE "UMG"

void FTextJustifyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const FMargin OuterPadding(2);
	const FMargin ContentPadding(2);

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew( SCheckBox )
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(LOCTEXT("AlignTextLeft", "Align Text Left"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FTextJustifyCustomization::HandleCheckStateChanged, PropertyHandle, ETextJustify::Left)
			.IsChecked(this, &FTextJustifyCustomization::GetCheckState, PropertyHandle, ETextJustify::Left)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("HorizontalAlignment_Left"))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(LOCTEXT("AlignTextCenter", "Align Text Center"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FTextJustifyCustomization::HandleCheckStateChanged, PropertyHandle, ETextJustify::Center)
			.IsChecked(this, &FTextJustifyCustomization::GetCheckState, PropertyHandle, ETextJustify::Center)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("HorizontalAlignment_Center"))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(LOCTEXT("AlignTextRight", "Align Text Right"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FTextJustifyCustomization::HandleCheckStateChanged, PropertyHandle, ETextJustify::Right)
			.IsChecked(this, &FTextJustifyCustomization::GetCheckState, PropertyHandle, ETextJustify::Right)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("HorizontalAlignment_Right"))
			]
		]
	];
}

void FTextJustifyCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FTextJustifyCustomization::HandleCheckStateChanged(ECheckBoxState InCheckboxState, TSharedRef<IPropertyHandle> PropertyHandle, ETextJustify::Type ToAlignment)
{
	PropertyHandle->SetValue((uint8)ToAlignment);
}

ECheckBoxState FTextJustifyCustomization::GetCheckState(TSharedRef<IPropertyHandle> PropertyHandle, ETextJustify::Type ForAlignment) const
{
	uint8 Value;
	if ( PropertyHandle->GetValue(Value) == FPropertyAccess::Result::Success)
	{
		return Value == ForAlignment ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE
