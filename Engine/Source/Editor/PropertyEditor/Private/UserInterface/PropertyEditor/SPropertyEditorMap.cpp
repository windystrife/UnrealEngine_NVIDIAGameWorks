// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyEditorMap.h"
#include "UObject/UnrealType.h"
#include "PropertyNode.h"
#include "Widgets/Text/STextBlock.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

void SPropertyEditorMap::Construct(const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor)
{
	PropertyEditor = InPropertyEditor;

	TAttribute<FText> TextAttr(this, &SPropertyEditorMap::GetSetTextValue);

	ChildSlot
	.Padding(0.0f, 0.0f, 2.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text(TextAttr)
		.Font(InArgs._Font)
	];

	SetToolTipText(GetMapTooltipText());

	SetEnabled(TAttribute<bool>(this, &SPropertyEditorMap::CanEdit));
}

bool SPropertyEditorMap::Supports(const TSharedRef< FPropertyEditor >& InPropertyEditor)
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const UProperty* Property = InPropertyEditor->GetProperty();

	if (!PropertyNode->HasNodeFlags(EPropertyNodeFlags::EditInlineNew)
		&& Property->IsA<UMapProperty>())
	{
		return true;
	}

	return false;
}

void SPropertyEditorMap::GetDesiredWidth(float& OutMinDesiredWidth, float& OutMaxDesiredWidth)
{
	OutMinDesiredWidth = 190.0f;
	OutMaxDesiredWidth = 190.0f;
}

FText SPropertyEditorMap::GetSetTextValue() const
{
	return FText::Format(LOCTEXT("NumMapItemsFmt", "{0} Map elements"), FText::AsNumber( PropertyEditor->GetPropertyNode()->GetNumChildNodes()));
}

FText SPropertyEditorMap::GetMapTooltipText() const
{
	return LOCTEXT("RichMapTooltipText", "Maps are associative, unordered containers that associate a set of keys with a set of values. Each key in a map must be unique, but values can be duplicated.");
}

bool SPropertyEditorMap::CanEdit() const
{
	return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
}

#undef LOCTEXT_NAMESPACE
