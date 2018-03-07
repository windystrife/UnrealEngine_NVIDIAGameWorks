// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Customizations/HorizontalAlignmentCustomization.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "DetailWidgetRow.h"


#define LOCTEXT_NAMESPACE "UMG"

void FHorizontalAlignmentCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
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
			.ToolTipText(LOCTEXT("HAlignLeft", "Horizontally Align Left"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FHorizontalAlignmentCustomization::HandleCheckStateChanged, PropertyHandle, HAlign_Left)
			.IsChecked(this, &FHorizontalAlignmentCustomization::GetCheckState, PropertyHandle, HAlign_Left)
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
			.ToolTipText(LOCTEXT("HAlignCenter", "Horizontally Align Center"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FHorizontalAlignmentCustomization::HandleCheckStateChanged, PropertyHandle, HAlign_Center)
			.IsChecked(this, &FHorizontalAlignmentCustomization::GetCheckState, PropertyHandle, HAlign_Center)
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
			.ToolTipText(LOCTEXT("HAlignRight", "Horizontally Align Right"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FHorizontalAlignmentCustomization::HandleCheckStateChanged, PropertyHandle, HAlign_Right)
			.IsChecked(this, &FHorizontalAlignmentCustomization::GetCheckState, PropertyHandle, HAlign_Right)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("HorizontalAlignment_Right"))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(LOCTEXT("HAlignFill", "Horizontally Align Fill"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FHorizontalAlignmentCustomization::HandleCheckStateChanged, PropertyHandle, HAlign_Fill)
			.IsChecked(this, &FHorizontalAlignmentCustomization::GetCheckState, PropertyHandle, HAlign_Fill)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("HorizontalAlignment_Fill"))
			]
		]
	];
}

void FHorizontalAlignmentCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FHorizontalAlignmentCustomization::HandleCheckStateChanged(ECheckBoxState InCheckboxState, TSharedRef<IPropertyHandle> PropertyHandle, EHorizontalAlignment ToAlignment)
{
	PropertyHandle->SetValue((uint8)ToAlignment);
}

ECheckBoxState FHorizontalAlignmentCustomization::GetCheckState(TSharedRef<IPropertyHandle> PropertyHandle, EHorizontalAlignment ForAlignment) const
{
	uint8 Value;
	if ( PropertyHandle->GetValue(Value) == FPropertyAccess::Result::Success )
	{
		return Value == ForAlignment ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE
