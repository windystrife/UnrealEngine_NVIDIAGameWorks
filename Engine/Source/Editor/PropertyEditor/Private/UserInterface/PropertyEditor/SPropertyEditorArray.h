// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Fonts/SlateFontInfo.h"
#include "EditorStyleSet.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "PropertyEditorHelpers.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "PropertyEditor"

class SPropertyEditorArray : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SPropertyEditorArray )
		: _Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) ) 
		{}
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<FPropertyEditor>& InPropertyEditor )
	{
		PropertyEditor = InPropertyEditor;

		TAttribute<FText> TextAttr;
		if( PropertyEditorHelpers::IsStaticArray( *InPropertyEditor->GetPropertyNode() ) )
		{
			// Static arrays need special case handling for their values
			TextAttr.Set( GetArrayTextValue() );
		}
		else
		{
			TextAttr.Bind( this, &SPropertyEditorArray::GetArrayTextValue );
		}

		ChildSlot
		.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
		[
			SNew( STextBlock )
			.Text( TextAttr )
			.Font( InArgs._Font )
		];

		SetEnabled( TAttribute<bool>( this, &SPropertyEditorArray::CanEdit ) );
	}

	static bool Supports( const TSharedRef<FPropertyEditor>& InPropertyEditor )
	{
		const UProperty* NodeProperty = InPropertyEditor->GetProperty();

		return PropertyEditorHelpers::IsStaticArray( *InPropertyEditor->GetPropertyNode() ) 
			|| PropertyEditorHelpers::IsDynamicArray( *InPropertyEditor->GetPropertyNode() );
	}

	void GetDesiredWidth( float& OutMinDesiredWidth, float &OutMaxDesiredWidth )
	{
		OutMinDesiredWidth = 170.0f;
		OutMaxDesiredWidth = 170.0f;
	}
private:
	FText GetArrayTextValue() const
	{
		return FText::Format( LOCTEXT("NumArrayItemsFmt", "{0} Array elements"), FText::AsNumber(PropertyEditor->GetPropertyNode()->GetNumChildNodes()) );
	}

	/** @return True if the property can be edited */
	bool CanEdit() const
	{
		return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
	}
private:
	TSharedPtr<FPropertyEditor> PropertyEditor;
};

#undef LOCTEXT_NAMESPACE
