// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraGeneratedCodeView.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "ISequencer.h"
#include "NiagaraSystemViewModel.h"
#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraSystemScriptViewModel.h"
#include "EditorStyleSet.h"
#include "SScrollBox.h"
#include "UObjectGlobals.h"
#include "Class.h"
#include "Package.h"
#include "SequencerSettings.h"
#include "NiagaraSystem.h"
#include "NiagaraEditorStyle.h"
#include "PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "NiagaraGeneratedCodeView"

void SNiagaraGeneratedCodeView::Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
{
	TabState = 0;
	ScriptEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENiagaraScriptUsage"));
	ensure(ScriptEnum);

	SystemViewModel = InSystemViewModel;
	SystemViewModel->OnSelectedEmitterHandlesChanged().AddRaw(this, &SNiagaraGeneratedCodeView::SelectedEmitterHandlesChanged);
	SystemViewModel->GetSystemScriptViewModel()->OnSystemCompiled().AddRaw(this, &SNiagaraGeneratedCodeView::OnCodeCompiled);

	this->ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.OnClicked(this, &SNiagaraGeneratedCodeView::OnCopyPressed)
						.Text(LOCTEXT("CopyOutput", "Copy"))
						.ToolTipText(LOCTEXT("CopyOutputToolitp", "Press this button to put the contents of this tab in the clipboard."))
					]
					+ SHorizontalBox::Slot()
					[
						SNullWidget::NullWidget
					] 
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.Padding(2, 4, 2, 4)
					[
						SAssignNew(SearchBox, SSearchBox)
						.OnTextCommitted(this, &SNiagaraGeneratedCodeView::OnSearchTextCommitted)
						.HintText(NSLOCTEXT("SearchBox", "HelpHint", "Search For Text"))
						.OnTextChanged(this, &SNiagaraGeneratedCodeView::OnSearchTextChanged)
						.SelectAllTextWhenFocused(false)
						.DelayChangeNotificationsWhileTyping(true)
						.MinDesiredWidth(200)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2, 4, 2, 4)
					[
						SAssignNew(SearchFoundMOfNText, STextBlock)
						.MinDesiredWidth(25)
					]					
					/*
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 4, 2, 4)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.IsFocusable(false)
						.ToolTipText(LOCTEXT("UpToolTip", "Focus to next found search term"))
						.OnClicked(this, &SNiagaraGeneratedCodeView::SearchUpClicked)
						.Content()
						[
							SNew(STextBlock)
							.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
							.Text(FText::FromString(FString(TEXT("\xf062"))))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 4, 2, 4)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.IsFocusable(false)
						.ToolTipText(LOCTEXT("DownToolTip", "Focus to next found search term"))
						.OnClicked(this, &SNiagaraGeneratedCodeView::SearchDownClicked)
						.Content()
						[
							SNew(STextBlock)
							.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
							.Text(FText::FromString(FString(TEXT("\xf063"))))
						]
					]*/
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(CheckBoxContainer, SHorizontalBox)
					// We'll insert here on UI updating..
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoDataText", "Failed to compile or has not been compiled."))
				.Visibility_Lambda([&]() {
					if (TabHasScriptData())
						return EVisibility::Collapsed;
					return EVisibility::Visible;
				})
			]
			+ SVerticalBox::Slot()
			[
				SAssignNew(TextBodyContainer, SVerticalBox)
			]
		];
	UpdateUI();
	DoSearch(SearchBox->GetText());
}

FReply SNiagaraGeneratedCodeView::SearchUpClicked()
{
	CurrentFoundTextEntry--;
	if (CurrentFoundTextEntry < 0 && ActiveFoundTextEntries.Num() > 0)
	{
		CurrentFoundTextEntry = ActiveFoundTextEntries.Num() - 1;
	}
	else if (CurrentFoundTextEntry < 0 && ActiveFoundTextEntries.Num() == 0)
	{
		CurrentFoundTextEntry = 0;
	}

	if (ActiveFoundTextEntries.Num() > 0)
	{
		//GeneratedCode[TabState].Text->ScrollTo(ActiveFoundTextEntries[CurrentFoundTextEntry]);
	}

	SetSearchMofN();

	return FReply::Handled();
}

FReply SNiagaraGeneratedCodeView::SearchDownClicked()
{
	CurrentFoundTextEntry++;
	if (CurrentFoundTextEntry >= ActiveFoundTextEntries.Num())
	{
		CurrentFoundTextEntry = 0;
	}

	if (ActiveFoundTextEntries.Num() > 0)
	{
		//GeneratedCode[TabState].Text->ScrollTo(ActiveFoundTextEntries[CurrentFoundTextEntry]);
	}
	
	SetSearchMofN();

	return FReply::Handled();
}

FReply SNiagaraGeneratedCodeView::OnCopyPressed()
{
	if (TabState < (uint32)GeneratedCode.Num())
	{
		FPlatformApplicationMisc::ClipboardCopy(*GeneratedCode[TabState].Hlsl.ToString());
	}
	return FReply::Handled();
}

void SNiagaraGeneratedCodeView::OnSearchTextChanged(const FText& InFilterText)
{
	DoSearch(InFilterText);
}

void SNiagaraGeneratedCodeView::DoSearch(const FText& InFilterText)
{
	GeneratedCode[TabState].Text->SetHighlightText(InFilterText);
	InFilterText.ToString();

	FString SearchString = InFilterText.ToString();
	ActiveFoundTextEntries.Empty();
	if (SearchString.IsEmpty())
	{
		SetSearchMofN();
		return;
	}

	ActiveFoundTextEntries.Empty();
	CurrentFoundTextEntry = INDEX_NONE;
	for (int32 i = 0; i < GeneratedCode[TabState].HlslByLines.Num(); i++)
	{
		int32 LastPos = INDEX_NONE;
		int32 FoundPos = GeneratedCode[TabState].HlslByLines[i].Find(SearchString, ESearchCase::IgnoreCase, ESearchDir::FromStart, LastPos);
		while (FoundPos != INDEX_NONE)
		{
			ActiveFoundTextEntries.Add(FTextLocation(i, FoundPos));
			LastPos = FoundPos + 1;
			FoundPos = GeneratedCode[TabState].HlslByLines[i].Find(SearchString, ESearchCase::IgnoreCase, ESearchDir::FromStart, LastPos);
		}
	}

	if (ActiveFoundTextEntries.Num() > 0)
	{
		CurrentFoundTextEntry = 0;
		//GeneratedCode[TabState].Text->ScrollTo(ActiveFoundTextEntries[CurrentFoundTextEntry]);
	}

	SetSearchMofN();
}

void SNiagaraGeneratedCodeView::SetSearchMofN()
{
	//SearchFoundMOfNText->SetText(FText::Format(LOCTEXT("MOfN", "{0} of {1}"), FText::AsNumber(CurrentFoundTextEntry), FText::AsNumber(ActiveFoundTextEntries.Num())));
	SearchFoundMOfNText->SetText(FText::Format(LOCTEXT("MOfN", "{1} found"), FText::AsNumber(CurrentFoundTextEntry), FText::AsNumber(ActiveFoundTextEntries.Num())));
}

void SNiagaraGeneratedCodeView::OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType)
{
	OnSearchTextChanged(InFilterText);
}

void SNiagaraGeneratedCodeView::OnCodeCompiled()
{
	UpdateUI();
	DoSearch(SearchBox->GetText());
}

void SNiagaraGeneratedCodeView::SelectedEmitterHandlesChanged()
{
	UpdateUI();
	DoSearch(SearchBox->GetText());
}

void SNiagaraGeneratedCodeView::UpdateUI()
{
	TArray<UNiagaraScript*> Scripts;
	UNiagaraSystem& System = SystemViewModel->GetSystem();
	Scripts.Add(System.GetSystemSpawnScript(false));
	Scripts.Add(System.GetSystemUpdateScript(false));
	Scripts.Add(System.GetSystemSpawnScript(true));
	Scripts.Add(System.GetSystemUpdateScript(true));
	
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
	SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
	if (SelectedEmitterHandles.Num() == 1)
	{
		FNiagaraEmitterHandle* Handle = SelectedEmitterHandles[0]->GetEmitterHandle();
		if (Handle)
		{
			TArray<UNiagaraScript*> EmitterScripts;
			Handle->GetInstance()->GetScripts(EmitterScripts);
			Scripts.Append(EmitterScripts);
		}
	}

	GeneratedCode.SetNum(Scripts.Num());

	if (TabState >= (uint32)GeneratedCode.Num())
	{
		TabState = 0;
	}

	CheckBoxContainer->ClearChildren();
	TextBodyContainer->ClearChildren();

	for (int32 i = 0; i < GeneratedCode.Num(); i++)
	{
		GeneratedCode[i].Usage = Scripts[i]->Usage;
		GeneratedCode[i].UsageIndex = Scripts[i]->UsageIndex;

		TArray<FString> OutputByLines;
		GeneratedCode[i].Hlsl = FText::GetEmpty();
		FString SumString;
		Scripts[i]->LastHlslTranslation.ParseIntoArrayLines(OutputByLines, false);
		GeneratedCode[i].HlslByLines.SetNum(OutputByLines.Num());
		for (int32 k = 0; k < OutputByLines.Num(); k++)
		{
			GeneratedCode[i].HlslByLines[k] = FString::Printf(TEXT("/*%04d*/\t\t%s\r\n"), k, *OutputByLines[k]);
			SumString.Append(GeneratedCode[i].HlslByLines[k]);
		}
		GeneratedCode[i].Hlsl = FText::FromString(SumString);
		
		if (Scripts[i]->Usage == ENiagaraScriptUsage::ParticleEventScript)
		{
			GeneratedCode[i].UsageName = FText::Format(LOCTEXT("UsageNameEvent", "{0}[{1}]"), ScriptEnum->GetDisplayNameTextByValue((int64)Scripts[i]->Usage), FText::AsNumber(Scripts[i]->UsageIndex));
		}
		else
		{
			GeneratedCode[i].UsageName = FText::Format(LOCTEXT("UsageName", "{0}"), ScriptEnum->GetDisplayNameTextByValue((int64)Scripts[i]->Usage));
		}

		if (!GeneratedCode[i].HorizontalScrollBar.IsValid())
		{
			GeneratedCode[i].HorizontalScrollBar = SNew(SScrollBar)
				.Orientation(Orient_Horizontal)
				.Thickness(FVector2D(8.0f, 8.0f));
		}

		if (!GeneratedCode[i].VerticalScrollBar.IsValid())
		{
			GeneratedCode[i].VerticalScrollBar = SNew(SScrollBar)
				.Orientation(Orient_Vertical)
				.Thickness(FVector2D(8.0f, 8.0f));
		}

		if (!GeneratedCode[i].CheckBox.IsValid())
		{
			SAssignNew(GeneratedCode[i].CheckBox, SCheckBox)
				//.Style(FEditorStyle::Get(), "PlacementBrowser.Tab")
				//.Style(FEditorStyle::Get(), i == 0 ? "Property.ToggleButton.Start" : (i < GeneratedCode.Num() - 1 ? "Property.ToggleButton.Middle" : "Property.ToggleButton.End"))
				.OnCheckStateChanged(this, &SNiagaraGeneratedCodeView::OnTabChanged, (uint32)i)
				.IsChecked(this, &SNiagaraGeneratedCodeView::GetTabCheckedState, (uint32)i)
				.Style(FEditorStyle::Get(), "PlacementBrowser.Tab")
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					.Padding(FMargin(6, 0, 15, 0))
					.VAlign(VAlign_Center)
					[
						SAssignNew(GeneratedCode[i].TextName, STextBlock)
						.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.CodeView.Checkbox.Text")
						.Text(GeneratedCode[i].UsageName)
					]
				];
		}
		else
		{
			GeneratedCode[i].TextName->SetText(GeneratedCode[i].UsageName);
		}

		if (!GeneratedCode[i].Container.IsValid())
		{
			SAssignNew(GeneratedCode[i].Container, SVerticalBox)
				.Visibility(this, &SNiagaraGeneratedCodeView::GetViewVisibility, (uint32)i)
				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SScrollBox)
						.Orientation(Orient_Horizontal)
						.ExternalScrollbar(GeneratedCode[i].HorizontalScrollBar)
						+ SScrollBox::Slot()
						[
							SNew(SScrollBox)
							.Orientation(Orient_Vertical)
							.ExternalScrollbar(GeneratedCode[i].VerticalScrollBar)
							+ SScrollBox::Slot()
							[
								SAssignNew(GeneratedCode[i].Text, STextBlock)
								.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.CodeView.Hlsl.Normal")
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						GeneratedCode[i].VerticalScrollBar.ToSharedRef()
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					GeneratedCode[i].HorizontalScrollBar.ToSharedRef()
				];
		}

		GeneratedCode[i].Text->SetText(GeneratedCode[i].Hlsl);

		CheckBoxContainer->AddSlot()
			[
				GeneratedCode[i].CheckBox.ToSharedRef()
			];
		TextBodyContainer->AddSlot()
			[
				GeneratedCode[i].Container.ToSharedRef()
			];		
	}
}

SNiagaraGeneratedCodeView::~SNiagaraGeneratedCodeView()
{
	if (SystemViewModel.IsValid())
	{
		SystemViewModel->OnSelectedEmitterHandlesChanged().RemoveAll(this);
		SystemViewModel->GetSystemScriptViewModel()->OnSystemCompiled().RemoveAll(this);
	}
	
}

void SNiagaraGeneratedCodeView::OnTabChanged(ECheckBoxState State, uint32 Tab)
{
	if (State == ECheckBoxState::Checked)
	{
		TabState = Tab;
		DoSearch(SearchBox->GetText());
	}
}


bool SNiagaraGeneratedCodeView::TabHasScriptData() const
{
	return !GeneratedCode[TabState].Hlsl.IsEmpty();
}

ECheckBoxState SNiagaraGeneratedCodeView::GetTabCheckedState(uint32 Tab) const
{
	return TabState == Tab ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SNiagaraGeneratedCodeView::GetViewVisibility(uint32 Tab) const
{
	return TabState == Tab ? EVisibility::Visible : EVisibility::Collapsed;
}


#undef LOCTEXT_NAMESPACE