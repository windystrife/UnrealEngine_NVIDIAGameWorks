// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyEditorSet.h"
#include "UObject/UnrealType.h"
#include "PropertyNode.h"
#include "Widgets/Text/STextBlock.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

void SPropertyEditorSet::Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;

	TAttribute<FText> TextAttr(this, &SPropertyEditorSet::GetSetTextValue);

	ChildSlot
	.Padding(0.0f, 0.0f, 2.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text(TextAttr)
		.Font(InArgs._Font)
	];

	SetToolTipText(GetSetTooltipText());

	SetEnabled(TAttribute<bool>(this, &SPropertyEditorSet::CanEdit));
}

bool SPropertyEditorSet::Supports( const TSharedRef< FPropertyEditor >& InPropertyEditor)
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const UProperty* Property = InPropertyEditor->GetProperty();

	if (!PropertyNode->HasNodeFlags(EPropertyNodeFlags::EditInlineNew)
		&& Property->IsA<USetProperty>())
	{
		return true;
	}

	return false;
}

void SPropertyEditorSet::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 190.0f;
	OutMaxDesiredWidth = 190.0f;
}

FText SPropertyEditorSet::GetSetTextValue() const
{
	return FText::Format(LOCTEXT("NumSetItemsFmt", "{0} Set elements"), FText::AsNumber(PropertyEditor->GetPropertyNode()->GetNumChildNodes()));
}

FText SPropertyEditorSet::GetSetTooltipText() const
{
	return LOCTEXT("RichSetTooltipText", "Sets are unordered containers. Each element in a set must be unique.");
}

bool SPropertyEditorSet::CanEdit() const
{
	return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
}

#undef LOCTEXT_NAMESPACE
