// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyEditorArrayItem.h"
#include "UObject/UnrealType.h"
#include "PropertyNode.h"
#include "Widgets/Text/STextBlock.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"

void SPropertyEditorArrayItem::Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor>& InPropertyEditor )
{
	static const FName TitlePropertyFName = FName(TEXT("TitleProperty"));

	PropertyEditor = InPropertyEditor;

	ChildSlot
	.Padding( 0.0f, 0.0f, 5.0f, 0.0f )
	[
		SNew( STextBlock )
		.Text( this, &SPropertyEditorArrayItem::GetValueAsString )
		.Font( InArgs._Font )
	];

	SetEnabled( TAttribute<bool>( this, &SPropertyEditorArrayItem::CanEdit ) );

	// if this is a struct property, try to find a representative element to use as our stand in
	if (PropertyEditor->PropertyIsA( UStructProperty::StaticClass() ))
	{
		const UProperty* MainProperty = PropertyEditor->GetProperty();
		const UProperty* ArrayProperty = MainProperty ? Cast<const UProperty>( MainProperty->GetOuter() ) : nullptr;
		if (ArrayProperty) // should always be true
		{
			// see if this structure has a TitleProperty we can use to summarize
			const FString& RepPropertyName = ArrayProperty->GetMetaData(TitlePropertyFName);
			if (!RepPropertyName.IsEmpty())
			{
				TitlePropertyHandle = PropertyEditor->GetPropertyHandle()->GetChildHandle(FName(*RepPropertyName), false);
			}
		}
	}
}

void SPropertyEditorArrayItem::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 130.0f;
	OutMaxDesiredWidth = 500.0f;
}

bool SPropertyEditorArrayItem::Supports( const TSharedRef< class FPropertyEditor >& PropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = PropertyEditor->GetPropertyNode();
	const UProperty* Property = PropertyEditor->GetProperty();

	if (!Cast<const UClassProperty>(Property) && PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly))
	{
		if (Cast<const UArrayProperty>(Property->GetOuter()) &&
			!(Cast<const UArrayProperty>(Property->GetOuter())->PropertyFlags & CPF_EditConst))
		{
			return true;
		}

		if (Cast<const UMapProperty>(Property->GetOuter()) &&
			!(Cast<const UMapProperty>(Property->GetOuter())->PropertyFlags & CPF_EditConst))
		{
			return true;
		}
	}
	return false;
}

FText SPropertyEditorArrayItem::GetValueAsString() const
{
	if (TitlePropertyHandle.IsValid())
	{
		FText TextOut;
		if (FPropertyAccess::Success == TitlePropertyHandle->GetValueAsDisplayText(TextOut))
		{
			return TextOut;
		}
	}
	
	if( PropertyEditor->GetProperty() && PropertyEditor->PropertyIsA( UStructProperty::StaticClass() ) )
	{
		return FText::Format( NSLOCTEXT("PropertyEditor", "NumStructItems", "{0} members"), FText::AsNumber( PropertyEditor->GetPropertyNode()->GetNumChildNodes() ) );
	}

	return PropertyEditor->GetValueAsDisplayText();
}

bool SPropertyEditorArrayItem::CanEdit() const
{
	return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
}
