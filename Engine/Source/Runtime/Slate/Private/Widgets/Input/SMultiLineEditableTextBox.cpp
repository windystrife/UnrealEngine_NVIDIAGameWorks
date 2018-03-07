// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SPopUpErrorText.h"

#if WITH_FANCY_TEXT

namespace
{
	/**
     * Helper function to solve some issues with ternary operators inside construction of a widget.
	 */
	TSharedRef< SWidget > AsWidgetRef( const TSharedPtr< SWidget >& InWidget )
	{
		if ( InWidget.IsValid() )
		{
			return InWidget.ToSharedRef();
		}
		else
		{
			return SNullWidget::NullWidget;
		}
	}
}

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SMultiLineEditableTextBox::Construct( const FArguments& InArgs )
{
	check (InArgs._Style);
	Style = InArgs._Style;

	BorderImageNormal = &InArgs._Style->BackgroundImageNormal;
	BorderImageHovered = &InArgs._Style->BackgroundImageHovered;
	BorderImageFocused = &InArgs._Style->BackgroundImageFocused;
	BorderImageReadOnly = &InArgs._Style->BackgroundImageReadOnly;

	PaddingOverride = InArgs._Padding;
	HScrollBarPaddingOverride = InArgs._HScrollBarPadding;
	VScrollBarPaddingOverride = InArgs._VScrollBarPadding;
	FontOverride = InArgs._Font;
	ForegroundColorOverride = InArgs._ForegroundColor;
	BackgroundColorOverride = InArgs._BackgroundColor;
	ReadOnlyForegroundColorOverride = InArgs._ReadOnlyForegroundColor;

	bHasExternalHScrollBar = InArgs._HScrollBar.IsValid();
	HScrollBar = InArgs._HScrollBar;
	if (!HScrollBar.IsValid())
	{
		// Create and use our own scrollbar
		HScrollBar = SNew(SScrollBar)
			.Style(&InArgs._Style->ScrollBarStyle)
			.Orientation(Orient_Horizontal)
			.AlwaysShowScrollbar(InArgs._AlwaysShowScrollbars)
			.Thickness(FVector2D(5.0f, 5.0f));
	}
	
	bHasExternalVScrollBar = InArgs._VScrollBar.IsValid();
	VScrollBar = InArgs._VScrollBar;
	if (!VScrollBar.IsValid())
	{
		// Create and use our own scrollbar
		VScrollBar = SNew(SScrollBar)
			.Style(&InArgs._Style->ScrollBarStyle)
			.Orientation(Orient_Vertical)
			.AlwaysShowScrollbar(InArgs._AlwaysShowScrollbars)
			.Thickness(FVector2D(5.0f, 5.0f));
	}

	SBorder::Construct( SBorder::FArguments()
		.BorderImage( this, &SMultiLineEditableTextBox::GetBorderImage )
		.BorderBackgroundColor( this, &SMultiLineEditableTextBox::DetermineBackgroundColor )
		.ForegroundColor( this, &SMultiLineEditableTextBox::DetermineForegroundColor )
		.Padding( this, &SMultiLineEditableTextBox::DeterminePadding )
		[
			SAssignNew( Box, SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillWidth(1)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillHeight(1)
				[
					SAssignNew( EditableText, SMultiLineEditableText )
					.Text( InArgs._Text )
					.HintText( InArgs._HintText )
					.SearchText( InArgs._SearchText )
					.TextStyle( InArgs._TextStyle )
					.Marshaller( InArgs._Marshaller )
					.Font( this, &SMultiLineEditableTextBox::DetermineFont )
					.IsReadOnly( InArgs._IsReadOnly )
					.OnContextMenuOpening( InArgs._OnContextMenuOpening )
					.OnTextChanged( InArgs._OnTextChanged )
					.OnTextCommitted( InArgs._OnTextCommitted )
					.OnCursorMoved( InArgs._OnCursorMoved )
					.ContextMenuExtender( InArgs._ContextMenuExtender )
					.CreateSlateTextLayout( InArgs._CreateSlateTextLayout )
					.Justification(InArgs._Justification)
					.RevertTextOnEscape(InArgs._RevertTextOnEscape)
					.SelectAllTextWhenFocused(InArgs._SelectAllTextWhenFocused)
					.ClearTextSelectionOnFocusLoss(InArgs._ClearTextSelectionOnFocusLoss)
					.ClearKeyboardFocusOnCommit(InArgs._ClearKeyboardFocusOnCommit)
					.LineHeightPercentage(InArgs._LineHeightPercentage)
					.Margin(InArgs._Margin)
					.WrapTextAt(InArgs._WrapTextAt)
					.AutoWrapText(InArgs._AutoWrapText)
					.WrappingPolicy(InArgs._WrappingPolicy)
					.HScrollBar(HScrollBar)
					.VScrollBar(VScrollBar)
					.OnHScrollBarUserScrolled(InArgs._OnHScrollBarUserScrolled)
					.OnVScrollBarUserScrolled(InArgs._OnVScrollBarUserScrolled)
					.OnKeyDownHandler( InArgs._OnKeyDownHandler)
					.ModiferKeyForNewLine(InArgs._ModiferKeyForNewLine)
					.VirtualKeyboardTrigger( InArgs._VirtualKeyboardTrigger )
					.VirtualKeyboardDismissAction( InArgs._VirtualKeyboardDismissAction )
					.TextShapingMethod(InArgs._TextShapingMethod)
					.TextFlowDirection(InArgs._TextFlowDirection)
					.AllowContextMenu(InArgs._AllowContextMenu)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(HScrollBarPaddingBox, SBox)
					.Padding( this, &SMultiLineEditableTextBox::DetermineHScrollBarPadding )
					[
						AsWidgetRef( HScrollBar )
					]
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(VScrollBarPaddingBox, SBox)
				.Padding( this, &SMultiLineEditableTextBox::DetermineVScrollBarPadding )
				[
					AsWidgetRef( VScrollBar )
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

void SMultiLineEditableTextBox::SetStyle(const FEditableTextBoxStyle* InStyle)
{
	if (InStyle)
	{
		Style = InStyle;
	}
	else
	{
		FArguments Defaults;
		Style = Defaults._Style;
	}

	check(Style);

	if (!bHasExternalHScrollBar && HScrollBar.IsValid())
	{
		HScrollBar->SetStyle(&Style->ScrollBarStyle);
	}

	if (!bHasExternalVScrollBar && VScrollBar.IsValid())
	{
		VScrollBar->SetStyle(&Style->ScrollBarStyle);
	}

	BorderImageNormal = &Style->BackgroundImageNormal;
	BorderImageHovered = &Style->BackgroundImageHovered;
	BorderImageFocused = &Style->BackgroundImageFocused;
	BorderImageReadOnly = &Style->BackgroundImageReadOnly;
}

FSlateColor SMultiLineEditableTextBox::DetermineForegroundColor() const
{
	check(Style);

	if ( EditableText->IsTextReadOnly() )
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

void SMultiLineEditableTextBox::SetText( const TAttribute< FText >& InNewText )
{
	EditableText->SetText( InNewText );
}

void SMultiLineEditableTextBox::SetHintText( const TAttribute< FText >& InHintText )
{
	EditableText->SetHintText( InHintText );
}

void SMultiLineEditableTextBox::SetSearchText(const TAttribute<FText>& InSearchText)
{
	EditableText->SetSearchText(InSearchText);
}

FText SMultiLineEditableTextBox::GetSearchText() const
{
	return EditableText->GetSearchText();
}

void SMultiLineEditableTextBox::SetTextBoxForegroundColor(const TAttribute<FSlateColor>& InForegroundColor)
{
	ForegroundColorOverride = InForegroundColor;
}

void SMultiLineEditableTextBox::SetTextBoxBackgroundColor(const TAttribute<FSlateColor>& InBackgroundColor)
{
	BackgroundColorOverride = InBackgroundColor;
}

void SMultiLineEditableTextBox::SetReadOnlyForegroundColor(const TAttribute<FSlateColor>& InReadOnlyForegroundColor)
{
	ReadOnlyForegroundColorOverride = InReadOnlyForegroundColor;
}

void SMultiLineEditableTextBox::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	EditableText->SetTextShapingMethod(InTextShapingMethod);
}

void SMultiLineEditableTextBox::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	EditableText->SetTextFlowDirection(InTextFlowDirection);
}

void SMultiLineEditableTextBox::SetWrapTextAt(const TAttribute<float>& InWrapTextAt)
{
	EditableText->SetWrapTextAt(InWrapTextAt);
}

void SMultiLineEditableTextBox::SetAutoWrapText(const TAttribute<bool>& InAutoWrapText)
{
	EditableText->SetAutoWrapText(InAutoWrapText);
}

void SMultiLineEditableTextBox::SetWrappingPolicy(const TAttribute<ETextWrappingPolicy>& InWrappingPolicy)
{
	EditableText->SetWrappingPolicy(InWrappingPolicy);
}

void SMultiLineEditableTextBox::SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage)
{
	EditableText->SetLineHeightPercentage(InLineHeightPercentage);
}

void SMultiLineEditableTextBox::SetMargin(const TAttribute<FMargin>& InMargin)
{
	EditableText->SetMargin(InMargin);
}

void SMultiLineEditableTextBox::SetJustification(const TAttribute<ETextJustify::Type>& InJustification)
{
	EditableText->SetJustification(InJustification);
}

void SMultiLineEditableTextBox::SetAllowContextMenu(const TAttribute< bool >& InAllowContextMenu)
{
	EditableText->SetAllowContextMenu(InAllowContextMenu);
}

void SMultiLineEditableTextBox::SetIsReadOnly(const TAttribute<bool>& InIsReadOnly)
{
	EditableText->SetIsReadOnly(InIsReadOnly);
}

void SMultiLineEditableTextBox::SetError( const FText& InError )
{
	SetError( InError.ToString() );
}

void SMultiLineEditableTextBox::SetError( const FString& InError )
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

/** @return Border image for the text box based on the hovered and focused state */
const FSlateBrush* SMultiLineEditableTextBox::GetBorderImage() const
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

bool SMultiLineEditableTextBox::SupportsKeyboardFocus() const
{
	return StaticCastSharedPtr<SWidget>(EditableText)->SupportsKeyboardFocus();
}

bool SMultiLineEditableTextBox::HasKeyboardFocus() const
{
	// Since keyboard focus is forwarded to our editable text, we will test it instead
	return SBorder::HasKeyboardFocus() || EditableText->HasKeyboardFocus();
}

FReply SMultiLineEditableTextBox::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	FReply Reply = FReply::Handled();

	if ( InFocusEvent.GetCause() != EFocusCause::Cleared )
	{
		// Forward keyboard focus to our editable text widget
		Reply.SetUserFocus(EditableText.ToSharedRef(), InFocusEvent.GetCause());
	}

	return Reply;
}

bool SMultiLineEditableTextBox::AnyTextSelected() const
{
	return EditableText->AnyTextSelected();
}

void SMultiLineEditableTextBox::SelectAllText()
{
	EditableText->SelectAllText();
}

void SMultiLineEditableTextBox::ClearSelection()
{
	EditableText->ClearSelection();
}

FText SMultiLineEditableTextBox::GetSelectedText() const
{
	return EditableText->GetSelectedText();
}

void SMultiLineEditableTextBox::InsertTextAtCursor(const FText& InText)
{
	EditableText->InsertTextAtCursor(InText);
}

void SMultiLineEditableTextBox::InsertTextAtCursor(const FString& InString)
{
	EditableText->InsertTextAtCursor(InString);
}

void SMultiLineEditableTextBox::InsertRunAtCursor(TSharedRef<IRun> InRun)
{
	EditableText->InsertRunAtCursor(InRun);
}

void SMultiLineEditableTextBox::GoTo(const FTextLocation& NewLocation)
{
	EditableText->GoTo(NewLocation);
}

void SMultiLineEditableTextBox::ScrollTo(const FTextLocation& NewLocation)
{
	EditableText->ScrollTo(NewLocation);
}

void SMultiLineEditableTextBox::ApplyToSelection(const FRunInfo& InRunInfo, const FTextBlockStyle& InStyle)
{
	EditableText->ApplyToSelection(InRunInfo, InStyle);
}

void SMultiLineEditableTextBox::BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase, const bool InReverse)
{
	EditableText->BeginSearch(InSearchText, InSearchCase, InReverse);
}

void SMultiLineEditableTextBox::AdvanceSearch(const bool InReverse)
{
	EditableText->AdvanceSearch(InReverse);
}

TSharedPtr<const IRun> SMultiLineEditableTextBox::GetRunUnderCursor() const
{
	return EditableText->GetRunUnderCursor();
}

TArray<TSharedRef<const IRun>> SMultiLineEditableTextBox::GetSelectedRuns() const
{
	return EditableText->GetSelectedRuns();
}

TSharedPtr<const SScrollBar> SMultiLineEditableTextBox::GetHScrollBar() const
{
	return EditableText->GetHScrollBar();
}

TSharedPtr<const SScrollBar> SMultiLineEditableTextBox::GetVScrollBar() const
{
	return EditableText->GetVScrollBar();
}

void SMultiLineEditableTextBox::Refresh()
{
	return EditableText->Refresh();
}

void SMultiLineEditableTextBox::SetOnKeyDownHandler(FOnKeyDown InOnKeyDownHandler)
{
	EditableText->SetOnKeyDownHandler(InOnKeyDownHandler);
}

#endif //WITH_FANCY_TEXT
