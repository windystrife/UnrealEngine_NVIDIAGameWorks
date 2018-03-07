// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackScriptItemGroup.h"
#include "NiagaraStackModuleItem.h"
#include "NiagaraStackSpacer.h"
#include "NiagaraStackAddScriptModuleItem.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraStackErrorItem.h"
#include "Internationalization.h"
#include "NiagaraStackGraphUtilities.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UNiagaraStackScriptItemGroup"

UNiagaraStackScriptItemGroup::UNiagaraStackScriptItemGroup()
	: AddModuleItem(nullptr)
	, BottomSpacer(nullptr)
{
}

void UNiagaraStackScriptItemGroup::Initialize(
	TSharedRef<FNiagaraSystemViewModel> InSystemViewModel,
	TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel,
	UNiagaraStackEditorData& InStackEditorData,
	TSharedRef<FNiagaraScriptViewModel> InScriptViewModel,
	ENiagaraScriptUsage InScriptUsage,
	int32 InScriptOccurrence)
{
	checkf(ScriptViewModel.IsValid() == false, TEXT("Can not set the script view model more than once."));
	Super::Initialize(InSystemViewModel, InEmitterViewModel, InStackEditorData);
	ScriptViewModel = InScriptViewModel;
	ScriptUsage = InScriptUsage;
	ScriptOccurrence = InScriptOccurrence;
}

FText UNiagaraStackScriptItemGroup::GetDisplayName() const
{
	return DisplayName;
}

void UNiagaraStackScriptItemGroup::SetDisplayName(FText InDisplayName)
{
	DisplayName = InDisplayName;
}

void UNiagaraStackScriptItemGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	UNiagaraGraph* Graph = ScriptViewModel->GetGraphViewModel()->GetGraph();
	FText ErrorMessage;
	if (FNiagaraStackGraphUtilities::ValidateGraphForOutput(*Graph, ScriptUsage, ScriptOccurrence, ErrorMessage) == false)
	{
		UE_LOG(LogNiagaraEditor, Error, TEXT("Failed to Create Stack.  Message: %s"), *ErrorMessage.ToString());
		Error = MakeShared<FError>();
		Error->ErrorText = LOCTEXT("InvalidErrorText", "The data used to generate the stack has been corrupted and can not be used.\nUsing the fix option will reset this part of the stack to its default empty state.");
		Error->ErrorSummaryText = LOCTEXT("InvalidErrorSummaryText", "The stack data is invalid");
		Error->Fix.BindLambda([=]()
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("FixStackGraph", "Fix invalid stack graph"));
			FNiagaraStackGraphUtilities::ResetGraphForOutput(*Graph, ScriptUsage, ScriptOccurrence);
		});
	}
	else
	{
		UNiagaraNodeOutput* MatchingOutputNode = Graph->FindOutputNode(ScriptUsage, ScriptOccurrence);
		TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
		FNiagaraStackGraphUtilities::GetOrderedModuleNodes(*MatchingOutputNode, ModuleNodes);
		int32 ModuleIndex = 0;
		for (UNiagaraNodeFunctionCall* ModuleNode : ModuleNodes)
		{
			UNiagaraStackModuleItem* ModuleItem = FindCurrentChildOfTypeByPredicate<UNiagaraStackModuleItem>(CurrentChildren, 
				[=](UNiagaraStackModuleItem* CurrentModuleItem) { return &CurrentModuleItem->GetModuleNode() == ModuleNode; });

			if (ModuleItem == nullptr)
			{
				ModuleItem = NewObject<UNiagaraStackModuleItem>(this);
				ModuleItem->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), *ModuleNode);
				ModuleItem->SetOnModifiedGroupItems(UNiagaraStackModuleItem::FOnModifiedGroupItems::CreateUObject(this, &UNiagaraStackScriptItemGroup::ChildModifiedGroupItems));
			}

			FName ModuleSpacerKey = *FString::Printf(TEXT("Module%i"), ModuleIndex);
			UNiagaraStackSpacer* ModuleSpacer = FindCurrentChildOfTypeByPredicate<UNiagaraStackSpacer>(CurrentChildren,
				[=](UNiagaraStackSpacer* CurrentModuleSpacer) { return CurrentModuleSpacer->GetSpacerKey() == ModuleSpacerKey; });

			if (ModuleSpacer == nullptr)
			{
				ModuleSpacer = NewObject<UNiagaraStackSpacer>(this);
				ModuleSpacer->Initialize(GetSystemViewModel(), GetEmitterViewModel(), ModuleSpacerKey);
			}

			NewChildren.Add(ModuleItem);
			NewChildren.Add(ModuleSpacer);
			ModuleIndex++;
		}

		if (AddModuleItem == nullptr)
		{
			AddModuleItem = NewObject<UNiagaraStackAddScriptModuleItem>(this);
			AddModuleItem->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), *MatchingOutputNode);
			AddModuleItem->SetOnItemAdded(UNiagaraStackAddModuleItem::FOnItemAdded::CreateUObject(this, &UNiagaraStackScriptItemGroup::ItemAdded));
		}

		if (BottomSpacer == nullptr)
		{
			BottomSpacer = NewObject<UNiagaraStackSpacer>(this);
			BottomSpacer->Initialize(GetSystemViewModel(), GetEmitterViewModel(), "ScriptStackBottom");
		}

		NewChildren.Add(AddModuleItem);
		NewChildren.Add(BottomSpacer);

		ENiagaraScriptCompileStatus Status = ScriptViewModel->GetScriptCompileStatus(GetScriptUsage(), GetScriptOccurrence());
		if (Status == ENiagaraScriptCompileStatus::NCS_Error)
		{
			Error = MakeShared<FError>();
			Error->ErrorText = ScriptViewModel->GetScriptErrors(GetScriptUsage(), GetScriptOccurrence());
			Error->ErrorSummaryText = LOCTEXT("ConpileErrorSummary", "The stack has compile errors.");
		}
		else
		{
			Error.Reset();
		}
	}
}

void UNiagaraStackScriptItemGroup::ItemAdded()
{
	RefreshChildren();
}

void UNiagaraStackScriptItemGroup::ChildModifiedGroupItems()
{
	RefreshChildren();
}

int32 UNiagaraStackScriptItemGroup::GetErrorCount() const
{
	return Error.IsValid() ? 1 : 0;
}

bool UNiagaraStackScriptItemGroup::GetErrorFixable(int32 ErrorIdx) const
{
	return Error.IsValid() && Error->Fix.IsBound();
}

bool UNiagaraStackScriptItemGroup::TryFixError(int32 ErrorIdx)
{
	if (Error.IsValid() && Error->Fix.IsBound())
	{
		Error->Fix.Execute();
		return true;
	}
	return false;
}

FText UNiagaraStackScriptItemGroup::GetErrorText(int32 ErrorIdx) const
{
	return Error->ErrorText;
}

FText UNiagaraStackScriptItemGroup::GetErrorSummaryText(int32 ErrorIdx) const
{
	return Error->ErrorSummaryText;
}

#undef LOCTEXT_NAMESPACE