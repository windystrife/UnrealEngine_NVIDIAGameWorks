// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyEditorDateTime.h"
#include "Widgets/Input/SEditableTextBox.h"


void SPropertyEditorDateTime::Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;

	ChildSlot
	[
		SAssignNew( PrimaryWidget, SEditableTextBox )
		.Text( InPropertyEditor, &FPropertyEditor::GetValueAsText )
		.Font( InArgs._Font )
		.SelectAllTextWhenFocused(true)
		.ClearKeyboardFocusOnCommit(false)
		.OnTextCommitted(this, &SPropertyEditorDateTime::HandleTextCommitted)
		.SelectAllTextOnCommit(true)
		.IsReadOnly(InPropertyEditor->IsEditConst())
	];

	if( InPropertyEditor->PropertyIsA( UObjectPropertyBase::StaticClass() ) )
	{
		// Object properties should display their entire text in a tooltip
		PrimaryWidget->SetToolTipText( TAttribute<FText>( InPropertyEditor, &FPropertyEditor::GetValueAsText ) );
	}
}


bool SPropertyEditorDateTime::Supports( const TSharedRef< FPropertyEditor >& InPropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const UProperty* Property = InPropertyEditor->GetProperty();

	if (Property->IsA(UStructProperty::StaticClass()))
	{
		const UStructProperty* StructProp = Cast<const UStructProperty>(Property);
		extern UScriptStruct* Z_Construct_UScriptStruct_FDateTime();	// It'd be really nice if StaticStruct() worked on types declared in Object.h
		if (Z_Construct_UScriptStruct_FDateTime() == StructProp->Struct)
		{
			return true;
		}
	}

	return false;
}


void SPropertyEditorDateTime::HandleTextCommitted( const FText& NewText, ETextCommit::Type /*CommitInfo*/ )
{
	const TSharedRef<FPropertyNode> PropertyNode = PropertyEditor->GetPropertyNode();
	const TSharedRef<IPropertyHandle> PropertyHandle = PropertyEditor->GetPropertyHandle();

	PropertyHandle->SetValueFromFormattedString(NewText.ToString());
}
