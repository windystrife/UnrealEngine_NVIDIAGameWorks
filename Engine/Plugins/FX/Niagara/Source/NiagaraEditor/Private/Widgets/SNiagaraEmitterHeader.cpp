// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraEmitterHeader.h"
#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraEditorStyle.h"
#include "SInlineEditableTextBlock.h"
#include "SBoxPanel.h"
#include "SCheckBox.h"
#include "STextBlock.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterHeader"

void SNiagaraEmitterHeader::Construct(const FArguments& InArgs, TSharedRef<FNiagaraEmitterHandleViewModel> InViewModel)
{
	ViewModel = InViewModel;

	TSharedRef<SWidget> AdditionalHeaderContent = InArgs._AdditionalHeaderContent.Widget;

	ChildSlot
	[
		SNew(SVerticalBox)
		//~ Enabled check box rename text box, and external header controls.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		[
			//~ Enabled
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			.Padding(2)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("EnabledToolTip", "Toggles whether this emitter is enabled. Disabled emitters don't simulate or render."))
				.IsChecked(ViewModel.ToSharedRef(), &FNiagaraEmitterHandleViewModel::GetIsEnabledCheckState)
				.OnCheckStateChanged(ViewModel.ToSharedRef(), &FNiagaraEmitterHandleViewModel::OnIsEnabledCheckStateChanged)
			]

			//~ Name
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			.HAlign(HAlign_Left)
			[
				SNew(SInlineEditableTextBlock)
				.ToolTipText(NSLOCTEXT("NiagaraEmitterEditor", "NameTextToolTip", "Click to edit the emitter name."))
				.Style(FNiagaraEditorStyle::Get(), "NiagaraEditor.HeadingInlineEditableText")
				.WrapTextAt(150.0f)
				.Text(ViewModel.ToSharedRef(), &FNiagaraEmitterHandleViewModel::GetNameText)
				.OnTextCommitted(ViewModel.ToSharedRef(), &FNiagaraEmitterHandleViewModel::OnNameTextComitted)
				.OnVerifyTextChanged(ViewModel.ToSharedRef(), &FNiagaraEmitterHandleViewModel::VerifyNameTextChanged)
			]

			//~ External controls.
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				AdditionalHeaderContent
			]
		]

		//~ Stats
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(2)
		[
			SNew(STextBlock)
			.Text(ViewModel->GetEmitterViewModel(), &FNiagaraEmitterViewModel::GetStatsText)
		]
	];
}

FReply SNiagaraEmitterHeader::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	ViewModel->OpenSourceEmitter();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE // "NiagaraEmitterHeader"