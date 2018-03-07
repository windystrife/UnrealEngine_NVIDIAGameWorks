// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"
#include "STableRow.h"
#include "STableViewBase.h"
#include "STreeView.h"
#include "NiagaraSystemViewModel.h"
#include "TickableEditorObject.h"
#include "WeakObjectPtrTemplates.h"
#include "NiagaraDataSet.h"
#include "SharedPointer.h"
#include "Map.h"
#include "SCheckBox.h"
#include "SSearchBox.h"
#include "SMultiLineEditableTextBox.h"

class SNiagaraGeneratedCodeView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraGeneratedCodeView)
	{}

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);
	virtual ~SNiagaraGeneratedCodeView();

	void OnCodeCompiled();

protected:

	void UpdateUI();

	struct TabInfo
	{
		FText UsageName;
		FText Hlsl;
		ENiagaraScriptUsage Usage;
		int32 UsageIndex;

		TArray<FString> HlslByLines;
		TSharedPtr<STextBlock> Text;
		TSharedPtr<STextBlock> TextName;
		TSharedPtr < SCheckBox > CheckBox;
		TSharedPtr<SScrollBar> HorizontalScrollBar;
		TSharedPtr<SScrollBar> VerticalScrollBar;
		TSharedPtr<SVerticalBox> Container;
	};

	TArray<TabInfo> GeneratedCode;
	TSharedPtr<SHorizontalBox> CheckBoxContainer;
	TSharedPtr<SVerticalBox> TextBodyContainer;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<STextBlock> SearchFoundMOfNText;
	TArray<FTextLocation> ActiveFoundTextEntries;
	int32 CurrentFoundTextEntry;

	void SetSearchMofN();

	void SelectedEmitterHandlesChanged();
	void OnTabChanged(ECheckBoxState State,	uint32 Tab);
	ECheckBoxState GetTabCheckedState(uint32 Tab) const;
	EVisibility GetViewVisibility(uint32 Tab) const;
	bool TabHasScriptData() const;
	FReply OnCopyPressed();
	void OnSearchTextChanged(const FText& InFilterText);
	void OnSearchTextCommitted(const FText& InFilterText, ETextCommit::Type InCommitType);
	FReply SearchUpClicked();
	FReply SearchDownClicked();

	void DoSearch(const FText& InFilterText);

	uint32 TabState;

	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;

	UEnum* ScriptEnum;
};


