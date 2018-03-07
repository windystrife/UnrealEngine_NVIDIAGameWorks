// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Customizations/SlateChildSizeCustomization.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "UMG"

void FSlateChildSizeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> ValueHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSlateChildSize, Value));
	TSharedPtr<IPropertyHandle> RuleHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSlateChildSize, SizeRule));

	const FMargin OuterPadding(2, 0);
	const FMargin ContentPadding(4, 2);

	if ( !( ValueHandle.IsValid() || RuleHandle.IsValid() ) )
	{
		return;
	}

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(OuterPadding)

			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SCheckBox)
				.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
				.ToolTipText(LOCTEXT("Auto_ToolTip", "Only requests as much room as it needs based on the widgets desired size."))
				.Padding(ContentPadding)
				.OnCheckStateChanged(this, &FSlateChildSizeCustomization::HandleCheckStateChanged, RuleHandle, ESlateSizeRule::Automatic)
				.IsChecked(this, &FSlateChildSizeCustomization::GetCheckState, RuleHandle, ESlateSizeRule::Automatic)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Auto", "Auto"))
				]
			]

			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SCheckBox)
				.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
				.ToolTipText(LOCTEXT("Fill_ToolTip", "Greedily attempts to fill all available room based on the percentage value 0..1"))
				.Padding(ContentPadding)
				.OnCheckStateChanged(this, &FSlateChildSizeCustomization::HandleCheckStateChanged, RuleHandle, ESlateSizeRule::Fill)
				.IsChecked(this, &FSlateChildSizeCustomization::GetCheckState, RuleHandle, ESlateSizeRule::Fill)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Fill", "Fill"))
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew(SBox)
			.WidthOverride(40)
			[
				SNew( SNumericEntryBox<float> )
				.LabelVAlign(VAlign_Center)
				.Visibility(this, &FSlateChildSizeCustomization::GetValueVisiblity, RuleHandle)
				.Value(this, &FSlateChildSizeCustomization::GetValue, ValueHandle)
				.OnValueCommitted(this, &FSlateChildSizeCustomization::HandleValueComitted, ValueHandle)
				.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
			]
		]
	];
}

void FSlateChildSizeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	
}

void FSlateChildSizeCustomization::HandleCheckStateChanged(ECheckBoxState InCheckboxState, TSharedPtr<IPropertyHandle> PropertyHandle, ESlateSizeRule::Type ToRule)
{
	PropertyHandle->SetValue((uint8)ToRule);
}

ECheckBoxState FSlateChildSizeCustomization::GetCheckState(TSharedPtr<IPropertyHandle> PropertyHandle, ESlateSizeRule::Type ForRule) const
{
	uint8 Value;
	if ( PropertyHandle->GetValue(Value) == FPropertyAccess::Result::Success )
	{
		return Value == ForRule ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

TOptional<float> FSlateChildSizeCustomization::GetValue(TSharedPtr<IPropertyHandle> ValueHandle) const
{
	float Value;
	if ( ValueHandle->GetValue(Value) == FPropertyAccess::Result::Success)
	{
		return Value;
	}

	return TOptional<float>();
}

void FSlateChildSizeCustomization::HandleValueComitted(float NewValue, ETextCommit::Type CommitType, TSharedPtr<IPropertyHandle> ValueHandle)
{
	ValueHandle->SetValue(NewValue);
}

EVisibility FSlateChildSizeCustomization::GetValueVisiblity(TSharedPtr<IPropertyHandle> RuleHandle) const
{
	uint8 Value;
	if ( RuleHandle->GetValue(Value) == FPropertyAccess::Result::Success)
	{
		return Value == ESlateSizeRule::Fill ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
