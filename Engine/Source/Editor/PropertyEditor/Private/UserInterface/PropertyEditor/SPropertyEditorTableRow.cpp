// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyEditorTableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"
#include "PropertyEditorHelpers.h"

#include "UserInterface/PropertyTree/PropertyTreeConstants.h"

#include "UserInterface/PropertyEditor/SPropertyEditor.h"
#include "UserInterface/PropertyEditor/SPropertyEditorNumeric.h"
#include "UserInterface/PropertyEditor/SPropertyEditorArray.h"
#include "UserInterface/PropertyEditor/SPropertyEditorCombo.h"
#include "UserInterface/PropertyEditor/SPropertyEditorEditInline.h"
#include "UserInterface/PropertyEditor/SPropertyEditorText.h"
#include "UserInterface/PropertyEditor/SPropertyEditorBool.h"
#include "UserInterface/PropertyEditor/SPropertyEditorColor.h"
#include "UserInterface/PropertyEditor/SPropertyEditorArrayItem.h"
#include "UserInterface/PropertyEditor/SPropertyEditorDateTime.h"


void SPropertyEditorTableRow::Construct( const FArguments& InArgs, const TSharedRef<FPropertyEditor>& InPropertyEditor, const TSharedRef< class IPropertyUtilities >& InPropertyUtilities, const TSharedRef<STableViewBase>& InOwnerTable )
{
	PropertyEditor = InPropertyEditor;
	PropertyUtilities = InPropertyUtilities;
	OnMiddleClicked = InArgs._OnMiddleClicked;
	ConstructExternalColumnCell = InArgs._ConstructExternalColumnCell;

	PropertyPath = FPropertyNode::CreatePropertyPath( PropertyEditor->GetPropertyNode() );

	this->SetToolTipText(PropertyEditor->GetToolTipText());

	SMultiColumnTableRow< TSharedPtr<FPropertyNode*> >::Construct( FSuperRowType::FArguments(), InOwnerTable );
}

TSharedRef<SWidget> SPropertyEditorTableRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if (ColumnName == PropertyTreeConstants::ColumnId_Name)
	{
		return ConstructNameColumnWidget();
	}
	else if (ColumnName == PropertyTreeConstants::ColumnId_Property)
	{
		return ConstructValueColumnWidget();
	}
	else if ( ConstructExternalColumnCell.IsBound() )
	{
		return ConstructExternalColumnCell.Execute( ColumnName, SharedThis( this ) );
	}

	return SNew(STextBlock).Text(NSLOCTEXT("PropertyEditor", "UnknownColumnId", "Unknown Column Id"));
}

TSharedRef< SWidget > SPropertyEditorTableRow::ConstructNameColumnWidget()
{
	TSharedRef< SHorizontalBox > NameColumnWidget =
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.VAlign(VAlign_Center)
		[
			SNew(SExpanderArrow, SharedThis( this ) )
		]
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew( SEditConditionWidget, PropertyEditor )
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew( SPropertyNameWidget, PropertyEditor )
			.OnDoubleClicked( this, &SPropertyEditorTableRow::OnNameDoubleClicked )
		];

		return NameColumnWidget;
}


TSharedRef< SWidget > SPropertyEditorTableRow::ConstructValueColumnWidget()
{
	TSharedRef<SHorizontalBox> HorizontalBox =
		SNew(SHorizontalBox)
		.IsEnabled(	PropertyEditor.ToSharedRef(), &FPropertyEditor::IsPropertyEditingEnabled );

	ValueEditorWidget = ConstructPropertyEditorWidget();

	HorizontalBox->AddSlot()
		.FillWidth(1) // Fill the entire width if possible
		.VAlign(VAlign_Center)	
		[
			ValueEditorWidget.ToSharedRef()
		];

	// The favorites star for this property
	HorizontalBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew( SButton )
			.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			.Visibility( this, &SPropertyEditorTableRow::OnGetFavoritesVisibility )
			.OnClicked( this, &SPropertyEditorTableRow::OnToggleFavoriteClicked )
			.ContentPadding(0)
			[
				SNew( SImage )
				.Image( this, &SPropertyEditorTableRow::OnGetFavoriteImage )
			]
		];

	TArray< TSharedRef<SWidget> > RequiredButtons;
	PropertyEditorHelpers::MakeRequiredPropertyButtons( PropertyEditor.ToSharedRef(), /*OUT*/RequiredButtons, TArray<EPropertyButton::Type>(), false );

	for( int32 ButtonIndex = 0; ButtonIndex < RequiredButtons.Num(); ++ButtonIndex )
	{
		HorizontalBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding( 2.0f, 1.0f )
			[ 
				RequiredButtons[ButtonIndex]
			];
	}

	return SNew(SBorder)
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.BorderImage_Static( &PropertyEditorConstants::GetOverlayBrush, PropertyEditor.ToSharedRef() )
		.VAlign(VAlign_Fill)
		[
			HorizontalBox
		];
}

FReply SPropertyEditorTableRow::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();

	if ( MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && OnMiddleClicked.IsBound() )
	{
		OnMiddleClicked.Execute( PropertyPath.ToSharedRef() );
		Reply = FReply::Handled();
	}

	return Reply;
}

TSharedPtr< FPropertyPath > SPropertyEditorTableRow::GetPropertyPath() const
{
	return PropertyPath.ToSharedRef();
}


bool SPropertyEditorTableRow::IsCursorHovering() const
{
	return SWidget::IsHovered();
}


EVisibility SPropertyEditorTableRow::OnGetFavoritesVisibility() const
{
	if( PropertyUtilities->AreFavoritesEnabled() && (!PropertyEditor->IsChildOfFavorite()) )
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FReply SPropertyEditorTableRow::OnToggleFavoriteClicked() 
{
	PropertyEditor->ToggleFavorite();
	return FReply::Handled();
}

const FSlateBrush* SPropertyEditorTableRow::OnGetFavoriteImage() const
{
	if( PropertyEditor->IsFavorite() )
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.Favorites_Enabled"));
	}

	return FEditorStyle::GetBrush(TEXT("PropertyWindow.Favorites_Disabled"));
}

void SPropertyEditorTableRow::OnEditConditionCheckChanged( ECheckBoxState CheckState )
{
	PropertyEditor->SetEditConditionState( CheckState == ECheckBoxState::Checked );
}

ECheckBoxState SPropertyEditorTableRow::OnGetEditConditionCheckState() const
{
	bool bEditConditionMet = PropertyEditor->IsEditConditionMet();
	return bEditConditionMet ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FReply SPropertyEditorTableRow::OnNameDoubleClicked()
{
	FReply Reply = FReply::Unhandled();

	if ( ValueEditorWidget.IsValid() )
	{
		// Get path to editable widget
		FWidgetPath EditableWidgetPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked( ValueEditorWidget.ToSharedRef(), EditableWidgetPath );

		// Set keyboard focus directly
		FSlateApplication::Get().SetKeyboardFocus( EditableWidgetPath, EFocusCause::SetDirectly );
		Reply = FReply::Handled();
	}
	else if ( DoesItemHaveChildren() )
	{
		ToggleExpansion();
		Reply = FReply::Handled();
	}

	return Reply;
}

TSharedRef<SWidget> SPropertyEditorTableRow::ConstructPropertyEditorWidget()
{
	TSharedPtr<SWidget> PropertyWidget; 
	const UProperty* const Property = PropertyEditor->GetProperty();

	const TSharedRef< FPropertyEditor > PropertyEditorRef = PropertyEditor.ToSharedRef();
	const TSharedRef< IPropertyUtilities > PropertyUtilitiesRef = PropertyUtilities.ToSharedRef();

	if( Property )
	{
		// ORDER MATTERS: first widget type to support the property node wins!
		if ( SPropertyEditorNumeric<float>::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorNumeric<float>, PropertyEditorRef );
		}
		else if (SPropertyEditorNumeric<int8>::Supports(PropertyEditorRef))
		{
			PropertyWidget = SNew(SPropertyEditorNumeric<int8>, PropertyEditorRef);
		}
		else if (SPropertyEditorNumeric<int16>::Supports(PropertyEditorRef))
		{
			PropertyWidget = SNew(SPropertyEditorNumeric<int16>, PropertyEditorRef);
		}
		else if (SPropertyEditorNumeric<int32>::Supports(PropertyEditorRef))
		{
			PropertyWidget = SNew(SPropertyEditorNumeric<int32>, PropertyEditorRef);
		}
		else if ( SPropertyEditorNumeric<int64>::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorNumeric<int64>, PropertyEditorRef );
		}
		else if ( SPropertyEditorNumeric<uint8>::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorNumeric<uint8>, PropertyEditorRef );
		}
		else if (SPropertyEditorNumeric<uint16>::Supports(PropertyEditorRef))
		{
			PropertyWidget = SNew(SPropertyEditorNumeric<uint16>, PropertyEditorRef);
		}
		else if (SPropertyEditorNumeric<uint32>::Supports(PropertyEditorRef))
		{
			PropertyWidget = SNew(SPropertyEditorNumeric<uint32>, PropertyEditorRef);
		}
		else if (SPropertyEditorNumeric<uint64>::Supports(PropertyEditorRef))
		{
			PropertyWidget = SNew(SPropertyEditorNumeric<uint64>, PropertyEditorRef);
		}
		else if ( SPropertyEditorArray::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorArray, PropertyEditorRef );
		}
		else if ( SPropertyEditorCombo::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorCombo, PropertyEditorRef );
		}
		else if ( SPropertyEditorEditInline::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorEditInline, PropertyEditorRef );
		}
		else if ( SPropertyEditorText::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorText, PropertyEditorRef );
		}
		else if ( SPropertyEditorBool::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorBool, PropertyEditorRef );
		}
		else if ( SPropertyEditorColor::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorColor, PropertyEditorRef, PropertyUtilitiesRef );
		}
		else if ( SPropertyEditorArrayItem::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorArrayItem, PropertyEditorRef );
		}
		else if ( SPropertyEditorDateTime::Supports(PropertyEditorRef) )
		{
			PropertyWidget = SNew( SPropertyEditorDateTime, PropertyEditorRef );
		}
	}

	if( !PropertyWidget.IsValid() )
	{
		PropertyWidget = SNew( SPropertyEditor, PropertyEditorRef );
	}

	PropertyWidget->SetToolTipText( PropertyEditor->GetToolTipText() );

	return PropertyWidget.ToSharedRef();
}
