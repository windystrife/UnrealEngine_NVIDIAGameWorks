// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAssetSearchBox.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SSearchBox.h"

void SAssetSearchBox::Construct( const FArguments& InArgs )
{
	OnTextChanged = InArgs._OnTextChanged;
	OnTextCommitted = InArgs._OnTextCommitted;
	OnKeyDownHandler = InArgs._OnKeyDownHandler;
	PossibleSuggestions = InArgs._PossibleSuggestions;
	PreCommittedText = InArgs._InitialText.Get();
	bMustMatchPossibleSuggestions = InArgs._MustMatchPossibleSuggestions.Get();

	ChildSlot
		[
			SAssignNew(SuggestionBox, SMenuAnchor)
			.Placement( InArgs._SuggestionListPlacement )
			[
				SAssignNew(InputText, SSearchBox)
				.InitialText(InArgs._InitialText)
				.HintText(InArgs._HintText)
				.OnTextChanged(this, &SAssetSearchBox::HandleTextChanged)
				.OnTextCommitted(this, &SAssetSearchBox::HandleTextCommitted)
				.SelectAllTextWhenFocused( false )
				.DelayChangeNotificationsWhileTyping( InArgs._DelayChangeNotificationsWhileTyping )
				.OnKeyDownHandler(this, &SAssetSearchBox::HandleKeyDown)
			]
			.MenuContent
				(
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				.Padding( FMargin(2) )
				[
					SNew(SBox)
					.WidthOverride(175)	// to enforce some minimum width, ideally we define the minimum, not a fixed width
					.HeightOverride(175) // avoids flickering, ideally this would be adaptive to the content without flickering
					[
						SAssignNew(SuggestionListView, SListView< TSharedPtr<FString> >)
						.ListItemsSource(&Suggestions)
						.SelectionMode( ESelectionMode::Single )							// Ideally the mouse over would not highlight while keyboard controls the UI
						.OnGenerateRow(this, &SAssetSearchBox::MakeSuggestionListItemWidget)
						.OnSelectionChanged( this, &SAssetSearchBox::OnSelectionChanged)
						.ItemHeight(18)
					]
				]
			)
		];
}

void SAssetSearchBox::SetText(const TAttribute< FText >& InNewText)
{
	InputText->SetText(InNewText);
	PreCommittedText = InNewText.Get();
}

void SAssetSearchBox::SetError( const FText& InError )
{
	InputText->SetError(InError);
}

void SAssetSearchBox::SetError( const FString& InError )
{
	InputText->SetError(InError);
}

FReply SAssetSearchBox::OnPreviewKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( SuggestionBox->IsOpen() && InKeyEvent.GetKey() == EKeys::Escape )
	{
		// Clear any selection first to prevent the currently selection being set in the text box
		SuggestionListView->ClearSelection();
		SuggestionBox->SetIsOpen(false, false);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SAssetSearchBox::HandleKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( SuggestionBox->IsOpen() && (InKeyEvent.GetKey() == EKeys::Up || InKeyEvent.GetKey() == EKeys::Down) )
	{
		const bool bSelectingUp = InKeyEvent.GetKey() == EKeys::Up;
		TSharedPtr<FString> SelectedSuggestion = GetSelectedSuggestion();

		if ( SelectedSuggestion.IsValid() )
		{
			// Find the selection index and select the previous or next one
			int32 TargetIdx = INDEX_NONE;
			for ( int32 SuggestionIdx = 0; SuggestionIdx < Suggestions.Num(); ++SuggestionIdx )
			{
				if ( Suggestions[SuggestionIdx] == SelectedSuggestion )
				{
					if ( bSelectingUp )
					{
						TargetIdx = SuggestionIdx - 1;
					}
					else
					{
						TargetIdx = SuggestionIdx + 1;
					}
					break;
				}
			}

			if ( Suggestions.IsValidIndex(TargetIdx) )
			{
				SuggestionListView->SetSelection( Suggestions[TargetIdx] );
				SuggestionListView->RequestScrollIntoView( Suggestions[TargetIdx] );
			}
		}
		else if ( !bSelectingUp && Suggestions.Num() > 0 )
		{
			// Nothing selected and pressed down, select the first item
			SuggestionListView->SetSelection( Suggestions[0] );
		}

		return FReply::Handled();
	}

	if (OnKeyDownHandler.IsBound())
	{
		return OnKeyDownHandler.Execute(MyGeometry, InKeyEvent);
	}

	return FReply::Unhandled();
}

bool SAssetSearchBox::SupportsKeyboardFocus() const
{
	return InputText->SupportsKeyboardFocus();
}

bool SAssetSearchBox::HasKeyboardFocus() const
{
	// Since keyboard focus is forwarded to our editable text, we will test it instead
	return InputText->HasKeyboardFocus();
}

FReply SAssetSearchBox::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	// Forward keyboard focus to our editable text widget
	return FReply::Handled().SetUserFocus(InputText.ToSharedRef(), InFocusEvent.GetCause());
}

void SAssetSearchBox::HandleTextChanged(const FText& NewText)
{
	OnTextChanged.ExecuteIfBound(NewText);
	UpdateSuggestionList();
}

void SAssetSearchBox::HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	TSharedPtr<FString> SelectedSuggestion = GetSelectedSuggestion();

	bool bCommitText = true;
	FText CommittedText;
	if ( SelectedSuggestion.IsValid() && CommitType != ETextCommit::OnCleared )
	{
		// Pressed selected a suggestion, set the text
		CommittedText = FText::FromString( *SelectedSuggestion.Get() );
	}
	else
	{
		if ( CommitType == ETextCommit::OnCleared )
		{
			// Clear text when escape is pressed then commit an empty string
			CommittedText = FText::GetEmpty();
		}
		else if( PossibleSuggestions.Get().Contains(NewText.ToString()) )
		{
			// If the text is a suggestion, set the text.
			CommittedText = NewText;
		}
		else if( bMustMatchPossibleSuggestions )
		{
			// commit the original text if we have to match a suggestion
			CommittedText = PreCommittedText;
		}
		else
		{
			// otherwise, set the typed text
			CommittedText = NewText;
		}	
	}

	// Set the text and execute the delegate
	SetText(CommittedText);
	OnTextCommitted.ExecuteIfBound(CommittedText, CommitType);

	if(CommitType != ETextCommit::Default)
	{
		// Clear the suggestion box if the user has navigated away or set their own text.
		SuggestionBox->SetIsOpen(false, false);
	}
}

void SAssetSearchBox::OnSelectionChanged( TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo )
{
	// If the user clicked directly on an item to select it, then accept the choice and close the window
	if( SelectInfo == ESelectInfo::OnMouseClick )
	{
		SetText( FText::FromString( *NewValue.Get() ));
		SuggestionBox->SetIsOpen(false, false);
		FocusEditBox();		
	}
}


TSharedRef<ITableRow> SAssetSearchBox::MakeSuggestionListItemWidget(TSharedPtr<FString> Text, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(Text.IsValid());

	return
		SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(*Text.Get()))
			.HighlightText(this, &SAssetSearchBox::GetHighlightText)
		];
}

FText SAssetSearchBox::GetHighlightText() const
{
	return InputText->GetText();
}

void SAssetSearchBox::UpdateSuggestionList()
{
	const FString TypedText = InputText->GetText().ToString();

	Suggestions.Empty();

	if ( TypedText.Len() > 0 )
	{
		TArray<FString> AllSuggestions = PossibleSuggestions.Get();

		for ( auto SuggestionIt = AllSuggestions.CreateConstIterator(); SuggestionIt; ++SuggestionIt )
		{
			const FString& Suggestion = *SuggestionIt;
			if ( Suggestion.Contains(TypedText))
			{
				Suggestions.Add( MakeShareable(new FString(Suggestion)) );
			}
		}

		if ( Suggestions.Num() > 0 )
		{
			// At least one suggestion was found, open the menu
			SuggestionBox->SetIsOpen(true, false);
		}
		else
		{
			// No suggestions were found, close the menu
			SuggestionBox->SetIsOpen(false, false);
		}
	}
	else
	{
		// No text was typed, close the menu
		SuggestionBox->SetIsOpen(false, false);
	}

	SuggestionListView->RequestListRefresh();
}

void SAssetSearchBox::FocusEditBox()
{
	FWidgetPath WidgetToFocusPath;
	FSlateApplication::Get().GeneratePathToWidgetUnchecked( InputText.ToSharedRef(), WidgetToFocusPath );
	FSlateApplication::Get().SetKeyboardFocus( WidgetToFocusPath, EFocusCause::SetDirectly );
}

TSharedPtr<FString> SAssetSearchBox::GetSelectedSuggestion()
{
	TSharedPtr<FString> SelectedSuggestion;
	if ( SuggestionBox->IsOpen() )
	{
		const TArray< TSharedPtr<FString> >& SelectedSuggestionList = SuggestionListView->GetSelectedItems();
		if ( SelectedSuggestionList.Num() > 0 )
		{
			// Selection mode is Single, so there should only be one suggestion at the most
			SelectedSuggestion = SelectedSuggestionList[0];
		}
	}

	return SelectedSuggestion;
}
