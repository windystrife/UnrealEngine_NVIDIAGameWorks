// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SOutputLog.h"
#include "Framework/Text/TextRange.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/TextLayout.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDeviceHelper.h"
#include "SlateOptMacros.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Text/SlateTextLayout.h"
#include "Framework/Text/SlateTextRun.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "EditorStyleSettings.h"
#include "EngineGlobals.h"
#include "Editor.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "SOutputLog"

/** Expression context to test the given messages against the current text filter */
class FLogFilter_TextFilterExpressionContext : public ITextFilterExpressionContext
{
public:
	explicit FLogFilter_TextFilterExpressionContext(const FLogMessage& InMessage) : Message(&InMessage) {}

	/** Test the given value against the strings extracted from the current item */
	virtual bool TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const override { return TextFilterUtils::TestBasicStringExpression(*Message->Message, InValue, InTextComparisonMode); }

	/**
	* Perform a complex expression test for the current item
	* No complex expressions in this case - always returns false
	*/
	virtual bool TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode) const override { return false; }

private:
	/** Message that is being filtered */
	const FLogMessage* Message;
};

/** Custom console editable text box whose only purpose is to prevent some keys from being typed */
class SConsoleEditableTextBox : public SEditableTextBox
{
public:
	SLATE_BEGIN_ARGS( SConsoleEditableTextBox ) {}
		
		/** Hint text that appears when there is no text in the text box */
		SLATE_ATTRIBUTE(FText, HintText)

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT(FOnTextChanged, OnTextChanged)

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT(FOnTextCommitted, OnTextCommitted)

	SLATE_END_ARGS()


	void Construct( const FArguments& InArgs )
	{
		SetStyle(&FCoreStyle::Get().GetWidgetStyle< FEditableTextBoxStyle >("NormalEditableTextBox"));

		SBorder::Construct(SBorder::FArguments()
			.BorderImage(this, &SConsoleEditableTextBox::GetConsoleBorder)
			.BorderBackgroundColor(Style->BackgroundColor)
			.ForegroundColor(Style->ForegroundColor)
			.Padding(Style->Padding)
			[
				SAssignNew( EditableText, SConsoleEditableText )
				.HintText( InArgs._HintText )
				.OnTextChanged( InArgs._OnTextChanged )
				.OnTextCommitted( InArgs._OnTextCommitted )
			] );
	}

private:
	class SConsoleEditableText : public SEditableText
	{
	public:
		SLATE_BEGIN_ARGS( SConsoleEditableText ) {}
			/** The text that appears when there is nothing typed into the search box */
			SLATE_ATTRIBUTE(FText, HintText)
			/** Called whenever the text is changed interactively by the user */
			SLATE_EVENT(FOnTextChanged, OnTextChanged)

			/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
			SLATE_EVENT(FOnTextCommitted, OnTextCommitted)
		SLATE_END_ARGS()

		void Construct( const FArguments& InArgs )
		{
			SEditableText::Construct
			( 
				SEditableText::FArguments()
				.HintText( InArgs._HintText )
				.OnTextChanged( InArgs._OnTextChanged )
				.OnTextCommitted( InArgs._OnTextCommitted ) 
				.ClearKeyboardFocusOnCommit( false )
				.IsCaretMovedWhenGainFocus( false ) 
				.MinDesiredWidth( 400.0f )
			);
		}

		virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
		{
			// Special case handling.  Intercept the tilde key.  It is not suitable for typing in the console
			if( InKeyEvent.GetKey() == EKeys::Tilde )
			{
				return FReply::Unhandled();
			}
			else
			{
				return SEditableText::OnKeyDown( MyGeometry, InKeyEvent );
			}
		}

		virtual FReply OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
		{
			// Special case handling.  Intercept the tilde key.  It is not suitable for typing in the console
			if( InCharacterEvent.GetCharacter() != 0x60 )
			{
				return SEditableText::OnKeyChar( MyGeometry, InCharacterEvent );
			}
			else
			{
				return FReply::Unhandled();
			}
		}

	};

	/** @return Border image for the text box based on the hovered and focused state */
	const FSlateBrush* GetConsoleBorder() const
	{
		if (EditableText->HasKeyboardFocus())
		{
			return &Style->BackgroundImageFocused;
		}
		else
		{
			if (EditableText->IsHovered())
			{
				return &Style->BackgroundImageHovered;
			}
			else
			{
				return &Style->BackgroundImageNormal;
			}
		}
	}

};

SConsoleInputBox::SConsoleInputBox()
	: SelectedSuggestion(-1)
	, bIgnoreUIUpdate(false)
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SConsoleInputBox::Construct( const FArguments& InArgs )
{
	OnConsoleCommandExecuted = InArgs._OnConsoleCommandExecuted;
	ConsoleCommandCustomExec = InArgs._ConsoleCommandCustomExec;

	ChildSlot
	[
		SAssignNew( SuggestionBox, SMenuAnchor )
			.Placement( InArgs._SuggestionListPlacement )
			[
				SAssignNew(InputText, SConsoleEditableTextBox)
					.OnTextCommitted(this, &SConsoleInputBox::OnTextCommitted)
					.HintText( NSLOCTEXT( "ConsoleInputBox", "TypeInConsoleHint", "Enter console command" ) )
					.OnTextChanged(this, &SConsoleInputBox::OnTextChanged)
			]
			.MenuContent
			(
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				.Padding( FMargin(2) )
				[
					SNew(SBox)
					.HeightOverride(250) // avoids flickering, ideally this would be adaptive to the content without flickering
					[
						SAssignNew(SuggestionListView, SListView< TSharedPtr<FString> >)
							.ListItemsSource(&Suggestions)
							.SelectionMode( ESelectionMode::Single )							// Ideally the mouse over would not highlight while keyboard controls the UI
							.OnGenerateRow(this, &SConsoleInputBox::MakeSuggestionListItemWidget)
							.OnSelectionChanged(this, &SConsoleInputBox::SuggestionSelectionChanged)
							.ItemHeight(18)
					]
				]
			)
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SConsoleInputBox::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (!GIntraFrameDebuggingGameThread && !IsEnabled())
	{
		SetEnabled(true);
	}
	else if (GIntraFrameDebuggingGameThread && IsEnabled())
	{
		SetEnabled(false);
	}
}


void SConsoleInputBox::SuggestionSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if(bIgnoreUIUpdate)
	{
		return;
	}

	for(int32 i = 0; i < Suggestions.Num(); ++i)
	{
		if(NewValue == Suggestions[i])
		{
			SelectedSuggestion = i;
			MarkActiveSuggestion();

			// If the user selected this suggestion by clicking on it, then go ahead and close the suggestion
			// box as they've chosen the suggestion they're interested in.
			if( SelectInfo == ESelectInfo::OnMouseClick )
			{
				SuggestionBox->SetIsOpen( false );
			}

			// Ideally this would set the focus back to the edit control
//			FWidgetPath WidgetToFocusPath;
//			FSlateApplication::Get().GeneratePathToWidgetUnchecked( InputText.ToSharedRef(), WidgetToFocusPath );
//			FSlateApplication::Get().SetKeyboardFocus( WidgetToFocusPath, EFocusCause::SetDirectly );
			break;
		}
	}
}


TSharedRef<ITableRow> SConsoleInputBox::MakeSuggestionListItemWidget(TSharedPtr<FString> Text, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(Text.IsValid());

	FString Left, Mid, Right, TempRight, Combined;

	if(Text->Split(TEXT("\t"), &Left, &TempRight))
	{
		if (TempRight.Split(TEXT("\t"), &Mid, &Right))
		{
			Combined = Left + Mid + Right;
		}
		else
		{
			Combined = Left + Right;
		}
	}
	else
	{
		Combined = *Text;
	}

	FText HighlightText = FText::FromString(Mid);

	return
		SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
		[
			SNew(SBox)
			.WidthOverride(300)			// to enforce some minimum width, ideally we define the minimum, not a fixed width
			[
				SNew(STextBlock)
				.Text(FText::FromString(Combined))
				.TextStyle( FEditorStyle::Get(), "Log.Normal")
				.HighlightText(HighlightText)
			]
		];
}

class FConsoleVariableAutoCompleteVisitor 
{
public:
	// @param Name must not be 0
	// @param CVar must not be 0
	static void OnConsoleVariable(const TCHAR *Name, IConsoleObject* CVar,TArray<FString>& Sink)
	{
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if(CVar->TestFlags(ECVF_Cheat))
		{
			return;
		}
#endif // (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if(CVar->TestFlags(ECVF_Unregistered))
		{
			return;
		}

		Sink.Add(Name);
	}
};

void SConsoleInputBox::OnTextChanged(const FText& InText)
{
	if(bIgnoreUIUpdate)
	{
		return;
	}

	const FString& InputTextStr = InputText->GetText().ToString();
	if(!InputTextStr.IsEmpty())
	{
		TArray<FString> AutoCompleteList;

		// console variables
		{
			IConsoleManager::Get().ForEachConsoleObjectThatContains(
				FConsoleObjectVisitor::CreateStatic< TArray<FString>& >(
				&FConsoleVariableAutoCompleteVisitor::OnConsoleVariable,
				AutoCompleteList ), *InputTextStr);
		}

		AutoCompleteList.Sort();

		for(uint32 i = 0; i < (uint32)AutoCompleteList.Num(); ++i)
		{
			FString &ref = AutoCompleteList[i];
			int32 Start = ref.Find(InputTextStr);

			if (Start != INDEX_NONE)
			{
				ref = ref.Left(Start) + TEXT("\t") + ref.Mid(Start, InputTextStr.Len()) + TEXT("\t") + ref.RightChop(Start + InputTextStr.Len());
			}
		}

		SetSuggestions(AutoCompleteList, false);
	}
	else
	{
		ClearSuggestions();
	}
}

void SConsoleInputBox::OnTextCommitted( const FText& InText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		if (!InText.IsEmpty())
		{
			IConsoleManager::Get().AddConsoleHistoryEntry( *InText.ToString() );

			// Copy the exec text string out so we can clear the widget's contents.  If the exec command spawns
			// a new window it can cause the text box to lose focus, which will result in this function being
			// re-entered.  We want to make sure the text string is empty on re-entry, so we'll clear it out
			const FString ExecString = InText.ToString();

			// Clear the console input area
			bIgnoreUIUpdate = true;
			InputText->SetText(FText::GetEmpty());
			bIgnoreUIUpdate = false;
			
			// Exec!
			if (ConsoleCommandCustomExec.IsBound())
			{
				ConsoleCommandCustomExec.Execute(ExecString);
			}
			else
			{
				bool bWasHandled = false;
				UWorld* World = NULL;
				UWorld* OldWorld = NULL;

				// The play world needs to handle these commands if it exists
				if( GIsEditor && GEditor->PlayWorld && !GIsPlayInEditorWorld )
				{
					World = GEditor->PlayWorld;
					OldWorld = SetPlayInEditorWorld( GEditor->PlayWorld );
				}
				
				ULocalPlayer* Player = GEngine->GetDebugLocalPlayer();
				if( Player )
				{
					UWorld* PlayerWorld = Player->GetWorld();
					if( !World )
					{
						World = PlayerWorld;
					}
					bWasHandled = Player->Exec( PlayerWorld, *ExecString, *GLog );
				}
					
				if( !World )
				{
					World = GEditor->GetEditorWorldContext().World();
				}
				if( World )
				{
					if( !bWasHandled )
					{					
						AGameModeBase* const GameMode = World->GetAuthGameMode();
						AGameStateBase* const GameState = World->GetGameState();
						if( GameMode && GameMode->ProcessConsoleExec( *ExecString, *GLog, NULL ) )
						{
							bWasHandled = true;
						}
						else if (GameState && GameState->ProcessConsoleExec(*ExecString, *GLog, NULL))
						{
							bWasHandled = true;
						}
					}

					if( !bWasHandled && !Player)
					{
						if( GIsEditor )
						{
							bWasHandled = GEditor->Exec( World, *ExecString, *GLog );
						}
						else
						{
							bWasHandled = GEngine->Exec( World, *ExecString, *GLog );
						}	
					}
				}
				// Restore the old world of there was one
				if( OldWorld )
				{
					RestoreEditorWorld( OldWorld );
				}
			}
		}

		ClearSuggestions();

		OnConsoleCommandExecuted.ExecuteIfBound();
	}
}

FReply SConsoleInputBox::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	if(SuggestionBox->IsOpen())
	{
		if(KeyEvent.GetKey() == EKeys::Up || KeyEvent.GetKey() == EKeys::Down)
		{
			if(KeyEvent.GetKey() == EKeys::Up)
			{
				if(SelectedSuggestion < 0)
				{
					// from edit control to end of list
					SelectedSuggestion = Suggestions.Num() - 1;
				}
				else
				{
					// got one up, possibly back to edit control
					--SelectedSuggestion;
				}
			}

			if(KeyEvent.GetKey() == EKeys::Down)
			{
				if(SelectedSuggestion < Suggestions.Num() - 1)
				{
					// go one down, possibly from edit control to top
					++SelectedSuggestion;
				}
				else
				{
					// back to edit control
					SelectedSuggestion = -1;
				}
			}

			MarkActiveSuggestion();

			return FReply::Handled();
		}
		else if (KeyEvent.GetKey() == EKeys::Tab)
		{
			if (Suggestions.Num())
			{
				if (SelectedSuggestion >= 0 && SelectedSuggestion < Suggestions.Num())
				{
					MarkActiveSuggestion();
					OnTextCommitted(InputText->GetText(), ETextCommit::OnEnter);
				}
				else
				{
					SelectedSuggestion = 0;
					MarkActiveSuggestion();
				}
			}

			return FReply::Handled();
		}
	}
	else
	{
		if(KeyEvent.GetKey() == EKeys::Up)
		{
			TArray<FString> History;

			IConsoleManager::Get().GetConsoleHistory(History);

			SetSuggestions(History, true);
			
			if(Suggestions.Num())
			{
				SelectedSuggestion = Suggestions.Num() - 1;
				MarkActiveSuggestion();
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SConsoleInputBox::SetSuggestions(TArray<FString>& Elements, bool bInHistoryMode)
{
	FString SelectionText;
	if (SelectedSuggestion >= 0 && SelectedSuggestion < Suggestions.Num())
	{
		SelectionText = *Suggestions[SelectedSuggestion];
	}

	SelectedSuggestion = -1;
	Suggestions.Empty();
	SelectedSuggestion = -1;

	for(uint32 i = 0; i < (uint32)Elements.Num(); ++i)
	{
		Suggestions.Add(MakeShareable(new FString(Elements[i])));

		if (Elements[i] == SelectionText)
		{
			SelectedSuggestion = i;
		}
	}

	if(Suggestions.Num())
	{
		// Ideally if the selection box is open the output window is not changing it's window title (flickers)
		SuggestionBox->SetIsOpen(true, false);
		SuggestionListView->RequestScrollIntoView(Suggestions.Last());
	}
	else
	{
		SuggestionBox->SetIsOpen(false);
	}
}

void SConsoleInputBox::OnFocusLost( const FFocusEvent& InFocusEvent )
{
//	SuggestionBox->SetIsOpen(false);
}

void SConsoleInputBox::MarkActiveSuggestion()
{
	bIgnoreUIUpdate = true;
	if(SelectedSuggestion >= 0)
	{
		SuggestionListView->SetSelection(Suggestions[SelectedSuggestion]);
		SuggestionListView->RequestScrollIntoView(Suggestions[SelectedSuggestion]);	// Ideally this would only scroll if outside of the view

		InputText->SetText(FText::FromString(GetSelectionText()));
	}
	else
	{
		SuggestionListView->ClearSelection();
	}
	bIgnoreUIUpdate = false;
}

void SConsoleInputBox::ClearSuggestions()
{
	SelectedSuggestion = -1;
	SuggestionBox->SetIsOpen(false);
	Suggestions.Empty();
}

FString SConsoleInputBox::GetSelectionText() const
{
	FString ret = *Suggestions[SelectedSuggestion];
	
	ret = ret.Replace(TEXT("\t"), TEXT(""));

	return ret;
}

TSharedRef< FOutputLogTextLayoutMarshaller > FOutputLogTextLayoutMarshaller::Create(TArray< TSharedPtr<FLogMessage> > InMessages, FLogFilter* InFilter)
{
	return MakeShareable(new FOutputLogTextLayoutMarshaller(MoveTemp(InMessages), InFilter));
}

FOutputLogTextLayoutMarshaller::~FOutputLogTextLayoutMarshaller()
{
}

void FOutputLogTextLayoutMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
{
	TextLayout = &TargetTextLayout;
	AppendMessagesToTextLayout(Messages);
}

void FOutputLogTextLayoutMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
{
	SourceTextLayout.GetAsText(TargetString);
}

bool FOutputLogTextLayoutMarshaller::AppendMessage(const TCHAR* InText, const ELogVerbosity::Type InVerbosity, const FName& InCategory)
{
	TArray< TSharedPtr<FLogMessage> > NewMessages;
	if(SOutputLog::CreateLogMessages(InText, InVerbosity, InCategory, NewMessages))
	{
		const bool bWasEmpty = Messages.Num() == 0;
		Messages.Append(NewMessages);

		// Add new message categories to the filter's available log categories
		for (const auto& NewMessage : NewMessages)
		{
			Filter->AddAvailableLogCategory(NewMessage->Category);
		}

		if(TextLayout)
		{
			// If we were previously empty, then we'd have inserted a dummy empty line into the document
			// We need to remove this line now as it would cause the message indices to get out-of-sync with the line numbers, which would break auto-scrolling
			if(bWasEmpty)
			{
				TextLayout->ClearLines();
			}

			// If we've already been given a text layout, then append these new messages rather than force a refresh of the entire document
			AppendMessagesToTextLayout(NewMessages);
		}
		else
		{
			MarkMessagesCacheAsDirty();
			MakeDirty();
		}

		return true;
	}

	return false;
}

void FOutputLogTextLayoutMarshaller::AppendMessageToTextLayout(const TSharedPtr<FLogMessage>& InMessage)
{
	if (!Filter->IsMessageAllowed(InMessage))
	{
		return;
	}

	// Increment the cached count if we're not rebuilding the log
	if ( !IsDirty() )
	{
		CachedNumMessages++;
	}

	const FTextBlockStyle& MessageTextStyle = FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(InMessage->Style);

	TSharedRef<FString> LineText = InMessage->Message;

	TArray<TSharedRef<IRun>> Runs;
	Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, MessageTextStyle));

	TextLayout->AddLine(FSlateTextLayout::FNewLineData(MoveTemp(LineText), MoveTemp(Runs)));
}

void FOutputLogTextLayoutMarshaller::AppendMessagesToTextLayout(const TArray<TSharedPtr<FLogMessage>>& InMessages)
{
	TArray<FTextLayout::FNewLineData> LinesToAdd;
	LinesToAdd.Reserve(InMessages.Num());

	int32 NumAddedMessages = 0;

	for (const auto& CurrentMessage : InMessages)
	{
		if (!Filter->IsMessageAllowed(CurrentMessage))
		{
			continue;
		}

		++NumAddedMessages;

		const FTextBlockStyle& MessageTextStyle = FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(CurrentMessage->Style);

		TSharedRef<FString> LineText = CurrentMessage->Message;

		TArray<TSharedRef<IRun>> Runs;
		Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, MessageTextStyle));

		LinesToAdd.Emplace(MoveTemp(LineText), MoveTemp(Runs));
	}

	// Increment the cached message count if the log is not being rebuilt
	if ( !IsDirty() )
	{
		CachedNumMessages += NumAddedMessages;
	}

	TextLayout->AddLines(LinesToAdd);
}

void FOutputLogTextLayoutMarshaller::ClearMessages()
{
	Messages.Empty();
	MakeDirty();
}

void FOutputLogTextLayoutMarshaller::CountMessages()
{
	// Do not re-count if not dirty
	if (!bNumMessagesCacheDirty)
	{
		return;
	}

	CachedNumMessages = 0;

	for (const auto& CurrentMessage : Messages)
	{
		if (Filter->IsMessageAllowed(CurrentMessage))
		{
			CachedNumMessages++;
		}
	}

	// Cache re-built, remove dirty flag
	bNumMessagesCacheDirty = false;
}

int32 FOutputLogTextLayoutMarshaller::GetNumMessages() const
{
	return Messages.Num();
}

int32 FOutputLogTextLayoutMarshaller::GetNumFilteredMessages()
{
	// No need to filter the messages if the filter is not set
	if (!Filter->IsFilterSet())
	{
		return GetNumMessages();
	}

	// Re-count messages if filter changed before we refresh
	if (bNumMessagesCacheDirty)
	{
		CountMessages();
	}

	return CachedNumMessages;
}

void FOutputLogTextLayoutMarshaller::MarkMessagesCacheAsDirty()
{
	bNumMessagesCacheDirty = true;
}

FOutputLogTextLayoutMarshaller::FOutputLogTextLayoutMarshaller(TArray< TSharedPtr<FLogMessage> > InMessages, FLogFilter* InFilter)
	: Messages(MoveTemp(InMessages))
	, CachedNumMessages(0)
	, Filter(InFilter)
	, TextLayout(nullptr)
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SOutputLog::Construct( const FArguments& InArgs )
{
	// Build list of available log categories from historical logs
	for (const auto& Message : InArgs._Messages)
	{
		Filter.AddAvailableLogCategory(Message->Category);
	}

	MessagesTextMarshaller = FOutputLogTextLayoutMarshaller::Create(InArgs._Messages, &Filter);

	MessagesTextBox = SNew(SMultiLineEditableTextBox)
		.Style(FEditorStyle::Get(), "Log.TextBox")
		.TextStyle(FEditorStyle::Get(), "Log.Normal")
		.ForegroundColor(FLinearColor::Gray)
		.Marshaller(MessagesTextMarshaller)
		.IsReadOnly(true)
		.AlwaysShowScrollbars(true)
		.OnVScrollBarUserScrolled(this, &SOutputLog::OnUserScrolled)
		.ContextMenuExtender(this, &SOutputLog::ExtendTextBoxMenu);

	ChildSlot
	[
		SNew(SVerticalBox)

		// Console output and filters
		+SVerticalBox::Slot()
		[
			SNew(SBorder)
			.Padding(3)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				// Output Log Filter
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
				[
					SNew(SHorizontalBox)
			
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SComboButton)
						.ComboButtonStyle(FEditorStyle::Get(), "GenericFilters.ComboButtonStyle")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(0)
						.ToolTipText(LOCTEXT("AddFilterToolTip", "Add an output log filter."))
						.OnGetMenuContent(this, &SOutputLog::MakeAddFilterMenu)
						.HasDownArrow(true)
						.ContentPadding(FMargin(1, 0))
						.ButtonContent()
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "GenericFilters.TextStyle")
								.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.9"))
								.Text(FText::FromString(FString(TEXT("\xf0b0"))) /*fa-filter*/)
							]

							+SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(2, 0, 0, 0)
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "GenericFilters.TextStyle")
								.Text(LOCTEXT("Filters", "Filters"))
							]
						]
					]

					+SHorizontalBox::Slot()
					.Padding(4, 1, 0, 0)
					[
						SAssignNew(FilterTextBox, SSearchBox)
						.HintText(LOCTEXT("SearchLogHint", "Search Log"))
						.OnTextChanged(this, &SOutputLog::OnFilterTextChanged)
						.OnTextCommitted(this, &SOutputLog::OnFilterTextCommitted)
						.DelayChangeNotificationsWhileTyping(true)
					]
				]

				// Output log area
				+SVerticalBox::Slot()
				.FillHeight(1)
				[
					MessagesTextBox.ToSharedRef()
				]
			]
		]

		// The console input box
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
		[
			SNew(SConsoleInputBox)
			.OnConsoleCommandExecuted(this, &SOutputLog::OnConsoleCommandExecuted)

			// Always place suggestions above the input line for the output log widget
			.SuggestionListPlacement(MenuPlacement_AboveAnchor)
		]
	];

	GLog->AddOutputDevice(this);
	// Remove itself on crash (crashmalloc has limited memory and echoing logs here at that point is useless).
	FCoreDelegates::OnHandleSystemError.AddRaw(this, &SOutputLog::OnCrash);

	bIsUserScrolled = false;
	RequestForceScroll();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SOutputLog::~SOutputLog()
{
	if (GLog != nullptr)
	{
		GLog->RemoveOutputDevice(this);
	}
	FCoreDelegates::OnHandleSystemError.RemoveAll(this);
}

void SOutputLog::OnCrash()
{
	if (GLog != nullptr)
	{
		GLog->RemoveOutputDevice(this);
	}
}

bool SOutputLog::CreateLogMessages( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category, TArray< TSharedPtr<FLogMessage> >& OutMessages )
{
	if (Verbosity == ELogVerbosity::SetColor)
	{
		// Skip Color Events
		return false;
	}
	else
	{
		// Get the style for this message. When piping output from child processes (eg. when cooking through the editor), we want to highlight messages
		// according to their original verbosity, so also check for "Error:" and "Warning:" substrings. This is consistent with how the build system processes logs.
		FName Style;
		if (Category == NAME_Cmd)
		{
			Style = FName(TEXT("Log.Command"));
		}
		else if (Verbosity == ELogVerbosity::Error || FCString::Stristr(V, TEXT("Error:")) != nullptr)
		{
			Style = FName(TEXT("Log.Error"));
		}
		else if (Verbosity == ELogVerbosity::Warning || FCString::Stristr(V, TEXT("Warning:")) != nullptr)
		{
			Style = FName(TEXT("Log.Warning"));
		}
		else
		{
			Style = FName(TEXT("Log.Normal"));
		}

		// Determine how to format timestamps
		static ELogTimes::Type LogTimestampMode = ELogTimes::None;
		if (UObjectInitialized() && !GExitPurge)
		{
			// Logging can happen very late during shutdown, even after the UObject system has been torn down, hence the init check above
			LogTimestampMode = GetDefault<UEditorStyleSettings>()->LogTimestampMode;
		}

		const int32 OldNumMessages = OutMessages.Num();

		// handle multiline strings by breaking them apart by line
		TArray<FTextRange> LineRanges;
		FString CurrentLogDump = V;
		FTextRange::CalculateLineRangesFromString(CurrentLogDump, LineRanges);

		bool bIsFirstLineInMessage = true;
		for (const FTextRange& LineRange : LineRanges)
		{
			if (!LineRange.IsEmpty())
			{
				FString Line = CurrentLogDump.Mid(LineRange.BeginIndex, LineRange.Len());
				Line = Line.ConvertTabsToSpaces(4);

				// Hard-wrap lines to avoid them being too long
				static const int32 HardWrapLen = 360;
				for (int32 CurrentStartIndex = 0; CurrentStartIndex < Line.Len();)
				{
					int32 HardWrapLineLen = 0;
					if (bIsFirstLineInMessage)
					{
						FString MessagePrefix = FOutputDeviceHelper::FormatLogLine(Verbosity, Category, nullptr, LogTimestampMode);
						
						HardWrapLineLen = FMath::Min(HardWrapLen - MessagePrefix.Len(), Line.Len() - CurrentStartIndex);
						FString HardWrapLine = Line.Mid(CurrentStartIndex, HardWrapLineLen);

						OutMessages.Add(MakeShared<FLogMessage>(MakeShared<FString>(MessagePrefix + HardWrapLine), Verbosity, Category, Style));
					}
					else
					{
						HardWrapLineLen = FMath::Min(HardWrapLen, Line.Len() - CurrentStartIndex);
						FString HardWrapLine = Line.Mid(CurrentStartIndex, HardWrapLineLen);

						OutMessages.Add(MakeShared<FLogMessage>(MakeShared<FString>(MoveTemp(HardWrapLine)), Verbosity, Category, Style));
					}

					bIsFirstLineInMessage = false;
					CurrentStartIndex += HardWrapLineLen;
				}
			}
		}

		return OldNumMessages != OutMessages.Num();
	}
}

void SOutputLog::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if ( MessagesTextMarshaller->AppendMessage(V, Verbosity, Category) )
	{
		// Don't scroll to the bottom automatically when the user is scrolling the view or has scrolled it away from the bottom.
		if( !bIsUserScrolled )
		{
			RequestForceScroll();
		}
	}
}

void SOutputLog::ExtendTextBoxMenu(FMenuBuilder& Builder)
{
	FUIAction ClearOutputLogAction(
		FExecuteAction::CreateRaw( this, &SOutputLog::OnClearLog ),
		FCanExecuteAction::CreateSP( this, &SOutputLog::CanClearLog )
		);

	Builder.AddMenuEntry(
		NSLOCTEXT("OutputLog", "ClearLogLabel", "Clear Log"), 
		NSLOCTEXT("OutputLog", "ClearLogTooltip", "Clears all log messages"), 
		FSlateIcon(), 
		ClearOutputLogAction
		);
}

void SOutputLog::OnClearLog()
{
	// Make sure the cursor is back at the start of the log before we clear it
	MessagesTextBox->GoTo(FTextLocation(0));

	MessagesTextMarshaller->ClearMessages();
	MessagesTextBox->Refresh();
	bIsUserScrolled = false;
}

void SOutputLog::OnUserScrolled(float ScrollOffset)
{
	bIsUserScrolled = ScrollOffset < 1.0 && !FMath::IsNearlyEqual(ScrollOffset, 1.0f);
}

bool SOutputLog::CanClearLog() const
{
	return MessagesTextMarshaller->GetNumMessages() > 0;
}

void SOutputLog::OnConsoleCommandExecuted()
{
	RequestForceScroll();
}

void SOutputLog::RequestForceScroll()
{
	if (MessagesTextMarshaller->GetNumFilteredMessages() > 0)
	{
		MessagesTextBox->ScrollTo(FTextLocation(MessagesTextMarshaller->GetNumFilteredMessages() - 1));
		bIsUserScrolled = false;
	}
}

void SOutputLog::Refresh()
{
	// Re-count messages if filter changed before we refresh
	MessagesTextMarshaller->CountMessages();

	MessagesTextBox->GoTo(FTextLocation(0));
	MessagesTextMarshaller->MakeDirty();
	MessagesTextBox->Refresh();
	RequestForceScroll();
}

void SOutputLog::OnFilterTextChanged(const FText& InFilterText)
{
	if (Filter.GetFilterText().ToString().Equals(InFilterText.ToString(), ESearchCase::CaseSensitive))
	{
		// nothing to do
		return;
	}

	// Flag the messages count as dirty
	MessagesTextMarshaller->MarkMessagesCacheAsDirty();

	// Set filter phrases
	Filter.SetFilterText(InFilterText);

	// Report possible syntax errors back to the user
	FilterTextBox->SetError(Filter.GetSyntaxErrors());

	// Repopulate the list to show only what has not been filtered out.
	Refresh();

	// Apply the new search text
	MessagesTextBox->BeginSearch(InFilterText);
}

void SOutputLog::OnFilterTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType)
{
	OnFilterTextChanged(InFilterText);
}

TSharedRef<SWidget> SOutputLog::MakeAddFilterMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	
	MenuBuilder.BeginSection("OutputLogVerbosityEntries", LOCTEXT("OutputLogVerbosityHeading", "Verbosity"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowMessages", "Messages"), 
			LOCTEXT("ShowMessages_Tooltip", "Filter the Output Log to show messages"), 
			FSlateIcon(), 
			FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::VerbosityLogs_Execute), 
				FCanExecuteAction::CreateLambda([] { return true; }), 
				FIsActionChecked::CreateSP(this, &SOutputLog::VerbosityLogs_IsChecked)), 
			NAME_None, 
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowWarnings", "Warnings"), 
			LOCTEXT("ShowWarnings_Tooltip", "Filter the Output Log to show warnings"), 
			FSlateIcon(), 
			FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::VerbosityWarnings_Execute), 
				FCanExecuteAction::CreateLambda([] { return true; }), 
				FIsActionChecked::CreateSP(this, &SOutputLog::VerbosityWarnings_IsChecked)), 
			NAME_None, 
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowErrors", "Errors"), 
			LOCTEXT("ShowErrors_Tooltip", "Filter the Output Log to show errors"), 
			FSlateIcon(), 
			FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::VerbosityErrors_Execute), 
				FCanExecuteAction::CreateLambda([] { return true; }), 
				FIsActionChecked::CreateSP(this, &SOutputLog::VerbosityErrors_IsChecked)), 
			NAME_None, 
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("OutputLogMiscEntries", LOCTEXT("OutputLogMiscHeading", "Miscellaneous"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("Categories", "Categories"), 
			LOCTEXT("SelectCategoriesToolTip", "Select Categories to display."), 
			FNewMenuDelegate::CreateSP(this, &SOutputLog::MakeSelectCategoriesSubMenu)
		);
	}

	return MenuBuilder.MakeWidget();
}

void SOutputLog::MakeSelectCategoriesSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("OutputLogCategoriesEntries");
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowAllCategories", "Show All"),
			LOCTEXT("ShowAllCategories_Tooltip", "Filter the Output Log to show all categories"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::CategoriesShowAll_Execute),
			FCanExecuteAction::CreateLambda([] { return true; }),
			FIsActionChecked::CreateSP(this, &SOutputLog::CategoriesShowAll_IsChecked)),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
		
		for (const FName Category : Filter.GetAvailableLogCategories())
		{
			MenuBuilder.AddMenuEntry(
				FText::AsCultureInvariant(Category.ToString()),
				FText::Format(LOCTEXT("Category_Tooltip", "Filter the Output Log to show Category: %s"), FText::AsCultureInvariant(Category.ToString())),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SOutputLog::CategoriesSingle_Execute, Category),
				FCanExecuteAction::CreateLambda([] { return true; }),
				FIsActionChecked::CreateSP(this, &SOutputLog::CategoriesSingle_IsChecked, Category)),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
	MenuBuilder.EndSection();
}

bool SOutputLog::VerbosityLogs_IsChecked() const
{
	return Filter.bShowLogs;
}

bool SOutputLog::VerbosityWarnings_IsChecked() const
{
	return Filter.bShowWarnings;
}

bool SOutputLog::VerbosityErrors_IsChecked() const
{
	return Filter.bShowErrors;
}

void SOutputLog::VerbosityLogs_Execute()
{ 
	Filter.bShowLogs = !Filter.bShowLogs;

	// Flag the messages count as dirty
	MessagesTextMarshaller->MarkMessagesCacheAsDirty();

	Refresh();
}

void SOutputLog::VerbosityWarnings_Execute()
{
	Filter.bShowWarnings = !Filter.bShowWarnings;

	// Flag the messages count as dirty
	MessagesTextMarshaller->MarkMessagesCacheAsDirty();

	Refresh();
}

void SOutputLog::VerbosityErrors_Execute()
{
	Filter.bShowErrors = !Filter.bShowErrors;

	// Flag the messages count as dirty
	MessagesTextMarshaller->MarkMessagesCacheAsDirty();

	Refresh();
}

bool SOutputLog::CategoriesShowAll_IsChecked() const
{
	return Filter.bShowAllCategories;
}

bool SOutputLog::CategoriesSingle_IsChecked(FName InName) const
{
	return Filter.IsLogCategoryEnabled(InName);
}

void SOutputLog::CategoriesShowAll_Execute()
{
	Filter.bShowAllCategories = !Filter.bShowAllCategories;

	Filter.ClearSelectedLogCategories();
	if (Filter.bShowAllCategories)
	{
		for (const auto& AvailableCategory : Filter.GetAvailableLogCategories())
		{
			Filter.ToggleLogCategory(AvailableCategory);
		}
	}

	// Flag the messages count as dirty
	MessagesTextMarshaller->MarkMessagesCacheAsDirty();

	Refresh();
}

void SOutputLog::CategoriesSingle_Execute(FName InName)
{
	Filter.ToggleLogCategory(InName);

	// Flag the messages count as dirty
	MessagesTextMarshaller->MarkMessagesCacheAsDirty();

	Refresh();
}

bool FLogFilter::IsMessageAllowed(const TSharedPtr<FLogMessage>& Message)
{
	// Filter Verbosity
	{
		if (Message->Verbosity == ELogVerbosity::Error && !bShowErrors)
		{
			return false;
		}

		if (Message->Verbosity == ELogVerbosity::Warning && !bShowWarnings)
		{
			return false;
		}

		if (Message->Verbosity != ELogVerbosity::Error && Message->Verbosity != ELogVerbosity::Warning && !bShowLogs)
		{
			return false;
		}
	}

	// Filter by Category
	{
		if (!IsLogCategoryEnabled(Message->Category))
		{
			return false;
		}
	}

	// Filter search phrase
	{
		if (!TextFilterExpressionEvaluator.TestTextFilter(FLogFilter_TextFilterExpressionContext(*Message)))
		{
			return false;
		}
	}

	return true;
}

void FLogFilter::AddAvailableLogCategory(FName& LogCategory)
{
	// Use an insert-sort to keep AvailableLogCategories alphabetically sorted
	int32 InsertIndex = 0;
	for (InsertIndex = AvailableLogCategories.Num()-1; InsertIndex >= 0; --InsertIndex)
	{
		FName CheckCategory = AvailableLogCategories[InsertIndex];
		// No duplicates
		if (CheckCategory == LogCategory)
		{
			return;
		}
		else if (CheckCategory.Compare(LogCategory) < 0)
		{
			break;
		}
	}
	AvailableLogCategories.Insert(LogCategory, InsertIndex+1);
	if (bShowAllCategories)
	{
		ToggleLogCategory(LogCategory);
	}
}

void FLogFilter::ToggleLogCategory(const FName& LogCategory)
{
	int32 FoundIndex = SelectedLogCategories.Find(LogCategory);
	if (FoundIndex == INDEX_NONE)
	{
		SelectedLogCategories.Add(LogCategory);
	}
	else
	{
		SelectedLogCategories.RemoveAt(FoundIndex, /*Count=*/1, /*bAllowShrinking=*/false);
	}
}

bool FLogFilter::IsLogCategoryEnabled(const FName& LogCategory) const
{
	return SelectedLogCategories.Contains(LogCategory);
}

void FLogFilter::ClearSelectedLogCategories()
{
	// No need to churn memory each time the selected categories are cleared
	SelectedLogCategories.Reset(SelectedLogCategories.GetAllocatedSize());
}
#undef LOCTEXT_NAMESPACE
