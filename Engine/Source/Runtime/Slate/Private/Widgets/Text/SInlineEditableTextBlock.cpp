// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"


void SInlineEditableTextBlock::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	OnBeginTextEditDelegate = InArgs._OnBeginTextEdit;
	OnTextCommittedDelegate = InArgs._OnTextCommitted;
	IsSelected = InArgs._IsSelected;
	OnVerifyTextChanged= InArgs._OnVerifyTextChanged;
	Text = InArgs._Text;
	bIsReadOnly = InArgs._IsReadOnly;
	bIsMultiLine = InArgs._MultiLine;
	DoubleSelectDelay = 0.0f;

	OnEnterEditingMode = InArgs._OnEnterEditingMode;
	OnExitEditingMode = InArgs._OnExitEditingMode;

	ChildSlot
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
			
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SAssignNew(TextBlock, STextBlock)
			.Text(Text)
			.TextStyle( &InArgs._Style->TextStyle )
			.Font(InArgs._Font)
			.ColorAndOpacity( InArgs._ColorAndOpacity )
			.ShadowColorAndOpacity( InArgs._ShadowColorAndOpacity )
			.ShadowOffset( InArgs._ShadowOffset )
			.HighlightText( InArgs._HighlightText )
			.ToolTipText( InArgs._ToolTipText )
			.WrapTextAt( InArgs._WrapTextAt )
			.Justification( InArgs._Justification )
			.LineBreakPolicy( InArgs._LineBreakPolicy )
		]
	];

#if WITH_FANCY_TEXT
	if( bIsMultiLine )
	{
		SAssignNew(MultiLineTextBox, SMultiLineEditableTextBox)
			.Text(InArgs._Text)
			.Style(&InArgs._Style->EditableTextBoxStyle)
			.Font(InArgs._Font)
			.ToolTipText( InArgs._ToolTipText )
			.OnTextChanged(this, &SInlineEditableTextBlock::OnTextChanged)
			.OnTextCommitted(this, &SInlineEditableTextBlock::OnTextBoxCommitted)
			.WrapTextAt(InArgs._WrapTextAt)
			.Justification(InArgs._Justification)
			.SelectAllTextWhenFocused(true)
			.ClearKeyboardFocusOnCommit(true)
			.RevertTextOnEscape(true)
			.ModiferKeyForNewLine(InArgs._ModiferKeyForNewLine);
	}
	else
#endif //WITH_FANCY_TEXT
	{
		SAssignNew(TextBox, SEditableTextBox)
			.Text(InArgs._Text)
			.Style(&InArgs._Style->EditableTextBoxStyle)
			.Font(InArgs._Font)
			.ToolTipText( InArgs._ToolTipText )
			.OnTextChanged( this, &SInlineEditableTextBlock::OnTextChanged )
			.OnTextCommitted(this, &SInlineEditableTextBlock::OnTextBoxCommitted)
			.SelectAllTextWhenFocused(true)
			.ClearKeyboardFocusOnCommit(false);
	}
}

SInlineEditableTextBlock::~SInlineEditableTextBlock()
{
	if(IsInEditMode())
	{
		// Clear the error so it will vanish.
		SetTextBoxError( FText::GetEmpty() );
	}
}

void SInlineEditableTextBlock::CancelEditMode()
{
	ExitEditingMode();

	// Get the text from source again.
	SetEditableText(Text);
}

bool SInlineEditableTextBlock::SupportsKeyboardFocus() const
{
	//Can not have keyboard focus if it's status of being selected is managed by another widget.
	return !IsSelected.IsBound();
}

void SInlineEditableTextBlock::EnterEditingMode()
{
	if(!bIsReadOnly.Get() && FSlateApplication::Get().HasAnyMouseCaptor() == false)
	{
		if(TextBlock->GetVisibility() == EVisibility::Visible)
		{
			OnEnterEditingMode.ExecuteIfBound();

			const FText CurrentText = TextBlock->GetText();
			SetEditableText( CurrentText );

			TSharedPtr<SWidget> ActiveTextBox = GetEditableTextWidget();
			HorizontalBox->AddSlot()
				[
					ActiveTextBox.ToSharedRef()
				];

			WidgetToFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
			FSlateApplication::Get().SetKeyboardFocus(ActiveTextBox, EFocusCause::SetDirectly);

			TextBlock->SetVisibility(EVisibility::Collapsed);

			OnBeginTextEditDelegate.ExecuteIfBound( CurrentText );
		}
	}
}

void SInlineEditableTextBlock::ExitEditingMode()
{
	OnExitEditingMode.ExecuteIfBound();

	HorizontalBox->RemoveSlot( GetEditableTextWidget().ToSharedRef() );
	TextBlock->SetVisibility(EVisibility::Visible);
	// Clear the error so it will vanish.
	SetTextBoxError( FText::GetEmpty() );

	// Restore the original widget focus
	TSharedPtr<SWidget> WidgetToFocusPin = WidgetToFocus.Pin();
	if(WidgetToFocusPin.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPin, EFocusCause::SetDirectly);
	}
	else
	{
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
	}
}

bool SInlineEditableTextBlock::IsInEditMode() const
{
	return TextBlock->GetVisibility() == EVisibility::Collapsed;
}

void SInlineEditableTextBlock::SetReadOnly(bool bInIsReadOnly)
{
	bIsReadOnly = bInIsReadOnly;
}

void SInlineEditableTextBlock::SetText( const TAttribute< FText >& InText )
{
	Text = InText;
	TextBlock->SetText( Text );
	SetEditableText( Text );
}

void SInlineEditableTextBlock::SetText( const FString& InText )
{
	Text = FText::FromString( InText );
	TextBlock->SetText( Text );
	SetEditableText( Text );
}

void SInlineEditableTextBlock::SetWrapTextAt( const TAttribute<float>& InWrapTextAt )
{
	TextBlock->SetWrapTextAt( InWrapTextAt );
}

FReply SInlineEditableTextBlock::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( !MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || MouseEvent.IsControlDown() || MouseEvent.IsShiftDown())
	{
		return FReply::Unhandled();
	}

	if (IsSelected.IsBound())
	{
		if (IsSelected.Execute() && !bIsReadOnly.Get() && !ActiveTimerHandle.IsValid())
		{
			RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SInlineEditableTextBlock::TriggerEditMode));
		}
	}
	else
	{
		// The widget is not managed by another widget, so handle the mouse input and enter edit mode if ready.
		if (HasKeyboardFocus() && !bIsReadOnly.Get())
		{
			EnterEditingMode();
			return FReply::Handled();
		}
	}

	// Do not handle the mouse input, this will allow for drag and dropping events to trigger.
	return FReply::Unhandled();
}

FReply SInlineEditableTextBlock::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// Cancel during a drag over event, otherwise the widget may enter edit mode.
	auto PinnedActiveTimerHandle = ActiveTimerHandle.Pin();
	if (PinnedActiveTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(PinnedActiveTimerHandle.ToSharedRef());
	}
	return FReply::Unhandled();
}

FReply SInlineEditableTextBlock::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	auto PinnedActiveTimerHandle = ActiveTimerHandle.Pin();
	if (PinnedActiveTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(PinnedActiveTimerHandle.ToSharedRef());
	}
	return FReply::Unhandled();
}

EActiveTimerReturnType SInlineEditableTextBlock::TriggerEditMode(double InCurrentTime, float InDeltaTime)
{
	EnterEditingMode();
	return EActiveTimerReturnType::Stop;
}

FReply SInlineEditableTextBlock::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if(InKeyEvent.GetKey() == EKeys::F2)
	{
		EnterEditingMode();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SInlineEditableTextBlock::OnTextChanged(const FText& InText)
{
	if(IsInEditMode())
	{
		FText OutErrorMessage;

		if(OnVerifyTextChanged.IsBound() && !OnVerifyTextChanged.Execute(InText, OutErrorMessage))
		{
			SetTextBoxError( OutErrorMessage );
		}
		else
		{
			SetTextBoxError( FText::GetEmpty() );
		}
	}
}

void SInlineEditableTextBlock::OnTextBoxCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		CancelEditMode();
		// Commit the name, certain actions might need to be taken by the bound function
		OnTextCommittedDelegate.ExecuteIfBound(Text.Get(), InCommitType);
	}
	else if(IsInEditMode())
	{
		if(OnVerifyTextChanged.IsBound())
		{
			if(InCommitType == ETextCommit::OnEnter)
			{
				FText OutErrorMessage;
				if(!OnVerifyTextChanged.Execute(InText, OutErrorMessage))
				{
					// Display as an error.
					SetTextBoxError( OutErrorMessage );
					return;
				}
			}
			else if(InCommitType == ETextCommit::OnUserMovedFocus)
			{
				FText OutErrorMessage;
				if(!OnVerifyTextChanged.Execute(InText, OutErrorMessage))
				{
					CancelEditMode();

					// Commit the name, certain actions might need to be taken by the bound function
					OnTextCommittedDelegate.ExecuteIfBound(Text.Get(), InCommitType);
				
					return;
				}
			}
			else // When the user removes all focus from the window, revert the name
			{
				CancelEditMode();

				// Commit the name, certain actions might need to be taken by the bound function
				OnTextCommittedDelegate.ExecuteIfBound(Text.Get(), InCommitType);
				return;
			}
		}
		
		ExitEditingMode();

		OnTextCommittedDelegate.ExecuteIfBound(InText, InCommitType);

		if ( !Text.IsBound() )
		{
			TextBlock->SetText( Text );
		}
	}
}

TSharedPtr<SWidget> SInlineEditableTextBlock::GetEditableTextWidget() const
{
#if WITH_FANCY_TEXT
	if( bIsMultiLine )
	{
		return MultiLineTextBox;
	}
	else
#endif //WITH_FANCY_TEXT
	{
		return TextBox;
	}
}

void SInlineEditableTextBlock::SetEditableText( const TAttribute< FText >& InNewText )
{
#if WITH_FANCY_TEXT
	bIsMultiLine ? MultiLineTextBox->SetText( Text ) :
#endif //WITH_FANCY_TEXT
				 TextBox->SetText( Text );
}

void SInlineEditableTextBlock::SetTextBoxError( const FText& ErrorText )
{
#if WITH_FANCY_TEXT
	bIsMultiLine ? MultiLineTextBox->SetError( ErrorText ) :
#endif //WITH_FANCY_TEXT
				 TextBox->SetError( ErrorText );
}
