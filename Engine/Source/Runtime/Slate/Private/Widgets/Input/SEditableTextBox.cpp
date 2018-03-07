// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SPopUpErrorText.h"

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SEditableTextBox::Construct( const FArguments& InArgs )
{
	check (InArgs._Style);
	SetStyle(InArgs._Style);

	PaddingOverride = InArgs._Padding;
	FontOverride = InArgs._Font;
	ForegroundColorOverride = InArgs._ForegroundColor;
	BackgroundColorOverride = InArgs._BackgroundColor;
	ReadOnlyForegroundColorOverride = InArgs._ReadOnlyForegroundColor;

	SBorder::Construct( SBorder::FArguments()
		.BorderImage( this, &SEditableTextBox::GetBorderImage )
		.BorderBackgroundColor( this, &SEditableTextBox::DetermineBackgroundColor )
		.ForegroundColor( this, &SEditableTextBox::DetermineForegroundColor )
		.Padding( 0 )
		[
			SAssignNew( Box, SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillWidth(1)
			[
				SAssignNew(PaddingBox, SBox)
				.Padding( this, &SEditableTextBox::DeterminePadding )
				.VAlign(VAlign_Center)
				[
					SAssignNew( EditableText, SEditableText )
					.Text( InArgs._Text )
					.HintText( InArgs._HintText )
					.SearchText( InArgs._SearchText )
					.Font( this, &SEditableTextBox::DetermineFont )
					.IsReadOnly( InArgs._IsReadOnly )
					.IsPassword( InArgs._IsPassword )
					.IsCaretMovedWhenGainFocus( InArgs._IsCaretMovedWhenGainFocus )
					.SelectAllTextWhenFocused( InArgs._SelectAllTextWhenFocused )
					.RevertTextOnEscape( InArgs._RevertTextOnEscape )
					.ClearKeyboardFocusOnCommit( InArgs._ClearKeyboardFocusOnCommit )
					.AllowContextMenu( InArgs._AllowContextMenu )
					.OnContextMenuOpening( InArgs._OnContextMenuOpening )
					.OnTextChanged( InArgs._OnTextChanged )
					.OnTextCommitted( InArgs._OnTextCommitted )
					.MinDesiredWidth( InArgs._MinDesiredWidth )
					.SelectAllTextOnCommit( InArgs._SelectAllTextOnCommit )
					.OnKeyDownHandler( InArgs._OnKeyDownHandler )
					.VirtualKeyboardType( InArgs._VirtualKeyboardType )
					.VirtualKeyboardTrigger( InArgs._VirtualKeyboardTrigger )
					.VirtualKeyboardDismissAction( InArgs._VirtualKeyboardDismissAction )
					.TextShapingMethod(InArgs._TextShapingMethod)
					.TextFlowDirection( InArgs._TextFlowDirection )
				]
			]
		]
	);

	ErrorReporting = InArgs._ErrorReporting;
	if ( ErrorReporting.IsValid() )
	{
		Box->AddSlot()
		.AutoWidth()
		.Padding(3,0)
		[
			ErrorReporting->AsWidget()
		];
	}

}

void SEditableTextBox::SetStyle(const FEditableTextBoxStyle* InStyle)
{
	Style = InStyle;

	if ( Style == nullptr )
	{
		FArguments Defaults;
		Style = Defaults._Style;
	}

	check(Style);

	BorderImageNormal = &Style->BackgroundImageNormal;
	BorderImageHovered = &Style->BackgroundImageHovered;
	BorderImageFocused = &Style->BackgroundImageFocused;
	BorderImageReadOnly = &Style->BackgroundImageReadOnly;
}


void SEditableTextBox::SetText( const TAttribute< FText >& InNewText )
{
	EditableText->SetText( InNewText );
}


void SEditableTextBox::SetError( const FText& InError )
{
	SetError( InError.ToString() );
}


void SEditableTextBox::SetError( const FString& InError )
{
	const bool bHaveError = !InError.IsEmpty();

	if ( !ErrorReporting.IsValid() )
	{
		// No error reporting was specified; make a default one
		TSharedPtr<SPopupErrorText> ErrorTextWidget;
		Box->AddSlot()
		.AutoWidth()
		.Padding(3,0)
		[
			SAssignNew( ErrorTextWidget, SPopupErrorText )
		];
		ErrorReporting = ErrorTextWidget;
	}

	ErrorReporting->SetError( InError );
}


void SEditableTextBox::SetOnKeyDownHandler(FOnKeyDown InOnKeyDownHandler)
{
	EditableText->SetOnKeyDownHandler(InOnKeyDownHandler);
}


void SEditableTextBox::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	EditableText->SetTextShapingMethod(InTextShapingMethod);
}


void SEditableTextBox::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	EditableText->SetTextFlowDirection(InTextFlowDirection);
}


bool SEditableTextBox::AnyTextSelected() const
{
	return EditableText->AnyTextSelected();
}


void SEditableTextBox::SelectAllText()
{
	EditableText->SelectAllText();
}


void SEditableTextBox::ClearSelection()
{
	EditableText->ClearSelection();
}


FText SEditableTextBox::GetSelectedText() const
{
	return EditableText->GetSelectedText();
}

void SEditableTextBox::GoTo(const FTextLocation& NewLocation)
{
	EditableText->GoTo(NewLocation);
}

void SEditableTextBox::ScrollTo(const FTextLocation& NewLocation)
{
	EditableText->ScrollTo(NewLocation);
}

void SEditableTextBox::BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase, const bool InReverse)
{
	EditableText->BeginSearch(InSearchText, InSearchCase, InReverse);
}

void SEditableTextBox::AdvanceSearch(const bool InReverse)
{
	EditableText->AdvanceSearch(InReverse);
}

bool SEditableTextBox::HasError() const
{
	return ErrorReporting.IsValid() && ErrorReporting->HasError();
}

const FSlateBrush* SEditableTextBox::GetBorderImage() const
{
	if ( EditableText->IsTextReadOnly() )
	{
		return BorderImageReadOnly;
	}
	else if ( EditableText->HasKeyboardFocus() )
	{
		return BorderImageFocused;
	}
	else
	{
		if ( EditableText->IsHovered() )
		{
			return BorderImageHovered;
		}
		else
		{
			return BorderImageNormal;
		}
	}
}


bool SEditableTextBox::SupportsKeyboardFocus() const
{
	return StaticCastSharedPtr<SWidget>(EditableText)->SupportsKeyboardFocus();
}


bool SEditableTextBox::HasKeyboardFocus() const
{
	// Since keyboard focus is forwarded to our editable text, we will test it instead
	return SBorder::HasKeyboardFocus() || EditableText->HasKeyboardFocus();
}


FReply SEditableTextBox::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	FReply Reply = FReply::Handled();

	if ( InFocusEvent.GetCause() != EFocusCause::Cleared )
	{
		// Forward keyboard focus to our editable text widget
		Reply.SetUserFocus(EditableText.ToSharedRef(), InFocusEvent.GetCause());
	}

	return Reply;
}


FReply SEditableTextBox::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape && EditableText->HasKeyboardFocus())
	{
		// Clear focus
		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Cleared);
	}

	return FReply::Unhandled();
}

FSlateColor SEditableTextBox::DetermineForegroundColor() const
{
	check(Style);  
	
	if (EditableText->IsTextReadOnly())
	{
		if (ReadOnlyForegroundColorOverride.IsSet())
		{
			return ReadOnlyForegroundColorOverride.Get();
		}
		if (ForegroundColorOverride.IsSet())
		{
			return ForegroundColorOverride.Get();
		}

		return Style->ReadOnlyForegroundColor;
	}
	else
	{
		return ForegroundColorOverride.IsSet() ? ForegroundColorOverride.Get() : Style->ForegroundColor;
	}
}

void SEditableTextBox::SetHintText(const TAttribute< FText >& InHintText)
{
	EditableText->SetHintText(InHintText);
}


void SEditableTextBox::SetSearchText(const TAttribute<FText>& InSearchText)
{
	EditableText->SetSearchText(InSearchText);
}


FText SEditableTextBox::GetSearchText() const
{
	return EditableText->GetSearchText();
}


void SEditableTextBox::SetIsReadOnly(TAttribute< bool > InIsReadOnly)
{
	EditableText->SetIsReadOnly(InIsReadOnly);
}


void SEditableTextBox::SetIsPassword(TAttribute< bool > InIsPassword)
{
	EditableText->SetIsPassword(InIsPassword);
}


void SEditableTextBox::SetFont(const TAttribute<FSlateFontInfo>& InFont)
{
	FontOverride = InFont;
}

void SEditableTextBox::SetTextBoxForegroundColor(const TAttribute<FSlateColor>& InForegroundColor)
{
	ForegroundColorOverride = InForegroundColor;
}

void SEditableTextBox::SetTextBoxBackgroundColor(const TAttribute<FSlateColor>& InBackgroundColor)
{
	BackgroundColorOverride = InBackgroundColor;
}


void SEditableTextBox::SetReadOnlyForegroundColor(const TAttribute<FSlateColor>& InReadOnlyForegroundColor)
{
	ReadOnlyForegroundColorOverride = InReadOnlyForegroundColor;
}


void SEditableTextBox::SetMinimumDesiredWidth(const TAttribute<float>& InMinimumDesiredWidth)
{
	EditableText->SetMinDesiredWidth(InMinimumDesiredWidth);
}


void SEditableTextBox::SetIsCaretMovedWhenGainFocus(const TAttribute<bool>& InIsCaretMovedWhenGainFocus)
{
	EditableText->SetIsCaretMovedWhenGainFocus(InIsCaretMovedWhenGainFocus);
}


void SEditableTextBox::SetSelectAllTextWhenFocused(const TAttribute<bool>& InSelectAllTextWhenFocused)
{
	EditableText->SetSelectAllTextWhenFocused(InSelectAllTextWhenFocused);
}


void SEditableTextBox::SetRevertTextOnEscape(const TAttribute<bool>& InRevertTextOnEscape)
{
	EditableText->SetRevertTextOnEscape(InRevertTextOnEscape);
}


void SEditableTextBox::SetClearKeyboardFocusOnCommit(const TAttribute<bool>& InClearKeyboardFocusOnCommit)
{
	EditableText->SetClearKeyboardFocusOnCommit(InClearKeyboardFocusOnCommit);
}


void SEditableTextBox::SetSelectAllTextOnCommit(const TAttribute<bool>& InSelectAllTextOnCommit)
{
	EditableText->SetSelectAllTextOnCommit(InSelectAllTextOnCommit);
}

void SEditableTextBox::SetAllowContextMenu(TAttribute<bool> InAllowContextMenu)
{
	EditableText->SetAllowContextMenu(InAllowContextMenu);
}
