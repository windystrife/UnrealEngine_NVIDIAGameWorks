// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSelectedEmitterHandle.h"
#include "SNiagaraEmitterHeader.h"
#include "NiagaraEditorStyle.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "NiagaraEditorModule.h"

#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NiagaraSelectedEmitterHandle"

void SNiagaraSelectedEmitterHandle::Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
{
	SystemViewModel = InSystemViewModel;
	SystemViewModel->OnSelectedEmitterHandlesChanged().AddRaw(this, &SNiagaraSelectedEmitterHandle::SelectedEmitterHandlesChanged);
	StackViewModel = NewObject<UNiagaraStackViewModel>();
	FNiagaraEditorModule NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(EmitterHeaderContainer, SBox)
			]
			+ SVerticalBox::Slot()
			[
				NiagaraEditorModule.CreateStackWidget(StackViewModel)
			]
		]
		+ SOverlay::Slot()
		.Padding(0, 20, 0, 0)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SNiagaraSelectedEmitterHandle::GetUnsupportedSelectionText)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.SelectedEmitter.UnsupportedSelectionText")
			.Visibility(this, &SNiagaraSelectedEmitterHandle::GetUnsupportedSelectionTextVisibility)
		]
	];

	RefreshEmitterWidgets();
}

SNiagaraSelectedEmitterHandle::~SNiagaraSelectedEmitterHandle()
{
	SystemViewModel->OnEmitterHandleViewModelsChanged().RemoveAll(this);
	SystemViewModel->OnSelectedEmitterHandlesChanged().RemoveAll(this);
	StackViewModel->Initialize(nullptr, nullptr);
}

void SNiagaraSelectedEmitterHandle::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(StackViewModel);
}

void SNiagaraSelectedEmitterHandle::RefreshEmitterWidgets()
{
	TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
	SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
	if (SelectedEmitterHandles.Num() == 1)
	{
		EmitterHeaderContainer->SetContent(SNew(SNiagaraEmitterHeader, SelectedEmitterHandles[0]));
		StackViewModel->Initialize(SystemViewModel, SelectedEmitterHandles[0]->GetEmitterViewModel());
	}
	else
	{
		EmitterHeaderContainer->SetContent(SNullWidget::NullWidget);
		StackViewModel->Initialize(SystemViewModel, nullptr);
	}
}

void SNiagaraSelectedEmitterHandle::SelectedEmitterHandlesChanged()
{
	RefreshEmitterWidgets();
}

EVisibility SNiagaraSelectedEmitterHandle::GetUnsupportedSelectionTextVisibility() const
{
	return SystemViewModel->GetSelectedEmitterHandleIds().Num() != 1 ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SNiagaraSelectedEmitterHandle::GetUnsupportedSelectionText() const
{
	if (SystemViewModel->GetSelectedEmitterHandleIds().Num() == 0)
	{
		return LOCTEXT("NoSelectionMessage", "Select an emitter in the timeline.");
	}
	else if (SystemViewModel->GetSelectedEmitterHandleIds().Num() > 1)
	{
		return LOCTEXT("MultipleSelectionMessage", "Multiple selected emitters are not currently supported.");
	}
	else
	{
		return FText();
	}
}

#undef LOCTEXT_NAMESPACE