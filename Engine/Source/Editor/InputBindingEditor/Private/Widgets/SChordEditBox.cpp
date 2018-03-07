// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChordEditBox.h"
#include "SlateOptMacros.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Widgets/SChordEditor.h"
#include "Widgets/Layout/SBox.h"


#define LOCTEXT_NAMESPACE "SChordEditBox"


/* SChordEditBox interface
 *****************************************************************************/

void SChordEditBox::Construct( const FArguments& InArgs, TSharedPtr<FUICommandInfo> InputCommand, EMultipleKeyBindingIndex InChordIndex)
{
	BorderImageNormal = FEditorStyle::GetBrush( "EditableTextBox.Background.Normal" );
	BorderImageHovered = FEditorStyle::GetBrush( "EditableTextBox.Background.Hovered" );
	BorderImageFocused = FEditorStyle::GetBrush( "EditableTextBox.Background.Focused" );

	static const FName InvertedForegroundName("InvertedForeground");

	ChildSlot
	[
		SAssignNew( ConflictPopup, SMenuAnchor )
		.Placement(MenuPlacement_ComboBox)
		.OnGetMenuContent( this, &SChordEditBox::OnGetContentForConflictPopup )
		.OnMenuOpenChanged(this, &SChordEditBox::OnConflictPopupOpenChanged)
		[
			SNew( SBox )
			.WidthOverride( 200.0f )
			[
				SNew( SBorder )
				.VAlign(VAlign_Center)
				.Padding( FMargin( 4.0f, 2.0f ) )
				.BorderImage( this, &SChordEditBox::GetBorderImage )
				.ForegroundColor( FEditorStyle::GetSlateColor(InvertedForegroundName) )
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.FillWidth(1)
					.VAlign(VAlign_Center)
					[
						SAssignNew( ChordEditor, SChordEditor, InputCommand, InChordIndex )
						.OnEditBoxLostFocus( this, &SChordEditBox::OnChordEditorLostFocus )
						.OnChordChanged( this, &SChordEditBox::OnChordChanged )
						.OnEditingStarted( this, &SChordEditBox::OnChordEditingStarted )
						.OnEditingStopped( this, &SChordEditBox::OnChordEditingStopped )
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						// Remove binding button
						SNew(SButton)
						.Visibility( this, &SChordEditBox::GetChordRemoveButtonVisibility )
						.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
						.ContentPadding(0)
						.OnClicked( this, &SChordEditBox::OnChordRemoveButtonClicked )
						.ForegroundColor( FSlateColor::UseForeground() )
						.IsFocusable(false)
						.ToolTipText(LOCTEXT("ChordEditButtonRemove_ToolTip", "Remove this binding") )
						[
							SNew( SImage )
							.Image( FEditorStyle::GetBrush( "Symbols.X" ) )
							.ColorAndOpacity( FLinearColor(.7f,0,0,.75f) )
						]
					]
				]
			]
		]
	];
}


const FSlateBrush* SChordEditBox::GetBorderImage() const
{
	if ( ChordEditor->HasKeyboardFocus() )
	{
		return BorderImageFocused;
	}
	else
	{
		if ( ChordEditor->IsHovered() )
		{
			return BorderImageHovered;
		}
		else
		{
			return BorderImageNormal;
		}
	}
}


FText SChordEditBox::GetNotificationMessage() const
{
	return ChordEditor->GetNotificationText();
}


void SChordEditBox::OnChordEditorLostFocus()
{
	if( (!ChordAcceptButton.IsValid() || ChordAcceptButton->HasMouseCapture() == false) && !ChordEditor->IsTyping() )
	{
		if( ChordEditor->IsEditing() && ChordEditor->IsEditedChordValid() && !ChordEditor->HasConflict() )
		{
			ChordEditor->CommitNewChord();
		}

		ChordEditor->StopEditing();
	}
}


void SChordEditBox::OnChordEditingStarted()
{
	ConflictPopup->SetIsOpen(false);
}


void SChordEditBox::OnChordEditingStopped()
{
	if( ChordEditor->IsEditedChordValid() && !ChordEditor->HasConflict() )
	{
		ChordEditor->CommitNewChord();
	}
}


void SChordEditBox::OnChordChanged()
{
	if( ChordEditor->HasConflict() )
	{
		ConflictPopup->SetIsOpen(true, true);
	}
	else
	{
		ConflictPopup->SetIsOpen(false);

		if (ChordEditor->IsEditedChordValid())
		{
			ChordEditor->CommitNewChord();
			ChordEditor->StopEditing();
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
		}
	}
}

EVisibility SChordEditBox::GetChordRemoveButtonVisibility() const
{
	if( !ChordEditor->IsEditing() && ChordEditor->IsActiveChordValid() )
	{
		// Chord should display a button.  What the button does is defined by the two cases above
		return EVisibility::Visible;
	}
	else
	{
		// Nothing to remove, but still take up the space
		return EVisibility::Hidden;
	}
}


FReply SChordEditBox::OnChordRemoveButtonClicked()
{
	if( !ChordEditor->IsEditing() )
	{
		ChordEditor->RemoveActiveChord();
	}

	return FReply::Handled();
}


FReply SChordEditBox::OnAcceptNewChordButtonClicked()
{
	if( ChordEditor->IsEditing() )
	{
		ChordEditor->CommitNewChord();
		ChordEditor->StopEditing();
		
	}

	ConflictPopup->SetIsOpen(false);

	return FReply::Handled();
}


TSharedRef<SWidget> SChordEditBox::OnGetContentForConflictPopup()
{
	return SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("NotificationList.ItemBackground")  )
		[
			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding( 2.0f, 0.0f )
				.AutoHeight()
				[
					SNew( STextBlock )
						.WrapTextAt(200.0f)
						.ColorAndOpacity( FLinearColor( .75f, 0.0f, 0.0f, 1.0f ) )
						.Text( this, &SChordEditBox::GetNotificationMessage )
						.Visibility( this, &SChordEditBox::GetNotificationVisibility )
				]

			+ SVerticalBox::Slot()
				.Padding( 2.0f )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.AutoHeight()
				[
					SAssignNew( ChordAcceptButton, SButton )
						.ContentPadding(1)
						.ToolTipText( LOCTEXT("ChordAcceptButton_ToolTip", "Accept this new binding") )
						.OnClicked( this, &SChordEditBox::OnAcceptNewChordButtonClicked )
						[
							SNew( SHorizontalBox )

							+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew( SImage )
										.Image( FEditorStyle::GetBrush( "Symbols.Check" ) )
										.ColorAndOpacity( FLinearColor(0,.7f,0,.75f) )
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(2.0f,0.0f)
								[
									SNew( STextBlock )
										.Text( LOCTEXT("ChordAcceptButtonText_Override", "Override") )
								]
						]
				]
		];
}

void SChordEditBox::OnConflictPopupOpenChanged(bool bIsOpen)
{
	if(!bIsOpen)
	{
		ChordEditor->StopEditing();
	}
}

EVisibility SChordEditBox::GetNotificationVisibility() const
{
	return !ChordEditor->GetNotificationText().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}


FReply SChordEditBox::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& InMouseEvent )
{
	// This is a passthrough if the chord edit box gets a mouse click in the button area and the button isn't visible. 
	// we should focus the lower level editing widget in this case
	if ( InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		if( !ChordEditor->IsEditing() )
		{
			ChordEditor->StartEditing();
		}
		return FReply::Handled().SetUserFocus(ChordEditor.ToSharedRef(), EFocusCause::Mouse);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
