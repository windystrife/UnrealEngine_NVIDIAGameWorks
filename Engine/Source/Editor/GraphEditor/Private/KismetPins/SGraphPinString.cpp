// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetPins/SGraphPinString.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EdGraphSchema_K2.h"
#include "ScopedTransaction.h"

void SGraphPinString::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinString::GetDefaultValueWidget()
{
	// Only allow actual string pins to be multi-line
	// Other text based pins (such as names and numbers) should be single-line only
	const bool bIsMultiLine = GraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_String;

	if (bIsMultiLine)
	{
		return SNew(SBox)
			.MinDesiredWidth(18)
			.MaxDesiredHeight(200)
			[
				SNew(SMultiLineEditableTextBox)
				.Style(FEditorStyle::Get(), "Graph.EditableTextBox")
				.Text(this, &SGraphPinString::GetTypeInValue)
				.SelectAllTextWhenFocused(true)
				.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
				.IsReadOnly(this, &SGraphPinString::GetDefaultValueIsReadOnly)
				.OnTextCommitted(this, &SGraphPinString::SetTypeInValue)
				.ForegroundColor(FSlateColor::UseForeground())
				.WrapTextAt(400)
				.ModiferKeyForNewLine(EModifierKey::Shift)
			];
	}
	else
	{
		return SNew(SBox)
			.MinDesiredWidth(18)
			.MaxDesiredWidth(400)
			[
				SNew(SEditableTextBox)
					.Style(FEditorStyle::Get(), "Graph.EditableTextBox")
					.Text(this, &SGraphPinString::GetTypeInValue)
					.SelectAllTextWhenFocused(true)
					.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
					.IsReadOnly(this, &SGraphPinString::GetDefaultValueIsReadOnly)
					.OnTextCommitted(this, &SGraphPinString::SetTypeInValue)
					.ForegroundColor(FSlateColor::UseForeground())
			];
	}
}

FText SGraphPinString::GetTypeInValue() const
{
	return FText::FromString( GraphPinObj->GetDefaultAsString() );
}

void SGraphPinString::SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	if(!GraphPinObj->GetDefaultAsString().Equals(NewTypeInValue.ToString()))
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeStringPinValue", "Change String Pin Value" ) );
		GraphPinObj->Modify();

		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, NewTypeInValue.ToString());
	}
}

bool SGraphPinString::GetDefaultValueIsReadOnly() const
{
	return GraphPinObj->bDefaultValueIsReadOnly;
}
