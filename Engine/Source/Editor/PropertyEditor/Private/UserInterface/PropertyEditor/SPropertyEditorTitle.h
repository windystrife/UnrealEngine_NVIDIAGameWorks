// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PropertyNode.h"
#include "Fonts/SlateFontInfo.h"
#include "EditorStyleSet.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"
#include "Widgets/Text/STextBlock.h"

class FCategoryPropertyNode;
class FItemPropertyNode;

class SPropertyEditorTitle : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SPropertyEditorTitle )
		: _StaticDisplayName()
		, _PropertyFont( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) )
		, _CategoryFont( FEditorStyle::GetFontStyle( PropertyEditorConstants::CategoryFontStyle ) )
		{}
		SLATE_ARGUMENT( FText, StaticDisplayName )
		SLATE_ATTRIBUTE( FSlateFontInfo, PropertyFont )
		SLATE_ATTRIBUTE( FSlateFontInfo, CategoryFont )
		SLATE_EVENT( FOnClicked, OnDoubleClicked )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<FPropertyEditor>& InPropertyEditor )
	{
		PropertyEditor = InPropertyEditor;
		OnDoubleClicked = InArgs._OnDoubleClicked;

		const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
		FCategoryPropertyNode* CategoryNode = PropertyNode->AsCategoryNode();
		FItemPropertyNode* ItemPropertyNode = PropertyNode->AsItemPropertyNode();

		TSharedPtr<SWidget> NameWidget;
		const bool bHasStaticName = !InArgs._StaticDisplayName.IsEmpty();
		if ( CategoryNode != NULL )
		{
			NameWidget =
				SNew( STextBlock )
				.Text( bHasStaticName ? InArgs._StaticDisplayName : InPropertyEditor->GetDisplayName() )
				.Font( InArgs._CategoryFont );
		}
		else if ( ItemPropertyNode != NULL )
		{
			if( bHasStaticName )
			{
				NameWidget =
					SNew( STextBlock )
					.Text( InArgs._StaticDisplayName )
					.Font( InArgs._PropertyFont );
			}		
			else
			{
				NameWidget =
					SNew( STextBlock )
					.Text( InPropertyEditor, &FPropertyEditor::GetDisplayName )
					.Font( InArgs._PropertyFont );
			}
		}
		else
		{
			NameWidget = 
				SNew( STextBlock )
				.Text( bHasStaticName ? InArgs._StaticDisplayName : InPropertyEditor->GetDisplayName() )
				.Font( InArgs._PropertyFont );
		}

		ChildSlot
		[
			NameWidget.ToSharedRef()
		];
	}


private:

	FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
	{
		if ( InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			if( OnDoubleClicked.IsBound() )
			{
				OnDoubleClicked.Execute();
				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}


private:

	/** The delegate to execute when this text is double clicked */
	FOnClicked OnDoubleClicked;

	TSharedPtr<FPropertyEditor> PropertyEditor;
};
