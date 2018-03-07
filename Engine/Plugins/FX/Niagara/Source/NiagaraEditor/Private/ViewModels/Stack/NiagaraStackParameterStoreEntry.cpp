// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackParameterStoreEntry.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraSystemViewModel.h"
#include "NiagaraSystemScriptViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraStackObject.h"

#include "ScopedTransaction.h"
#include "Editor.h"
#include "UObject/StructOnScope.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistryModule.h"
#include "ARFilter.h"
#include "EdGraph/EdGraphPin.h"

#define LOCTEXT_NAMESPACE "UNiagaraStackParameterStoreEntry"
UNiagaraStackParameterStoreEntry::UNiagaraStackParameterStoreEntry()
	: ValueObjectEntry(nullptr)
	, ItemIndentLevel(0)
{
}

void UNiagaraStackParameterStoreEntry::BeginDestroy()
{
	Super::BeginDestroy();
}
int32 UNiagaraStackParameterStoreEntry::GetItemIndentLevel() const
{
	return ItemIndentLevel;
}

void UNiagaraStackParameterStoreEntry::SetItemIndentLevel(int32 InItemIndentLevel)
{
	ItemIndentLevel = InItemIndentLevel;
}

void UNiagaraStackParameterStoreEntry::Initialize(
	TSharedRef<FNiagaraSystemViewModel> InSystemViewModel,
	TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel,
	UNiagaraStackEditorData& InStackEditorData,
	UObject* InOwner,
	FNiagaraParameterStore* InParameterStore,
	FString InInputParameterHandle,
	FNiagaraTypeDefinition InInputType)
{
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	StackEditorData = &InStackEditorData;
	DisplayName = FText::FromString(InInputParameterHandle);
	ParameterName = *InInputParameterHandle;
	InputType = InInputType;
	Owner = InOwner;
	ParameterStore = InParameterStore;
}

const FNiagaraTypeDefinition& UNiagaraStackParameterStoreEntry::GetInputType() const
{
	return InputType;
}

void UNiagaraStackParameterStoreEntry::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	RefreshValueAndHandle();

	if (ValueObject != nullptr)
	{
		if(ValueObjectEntry == nullptr ||
			ValueObjectEntry->GetObject() != ValueObject)
		{
			ValueObjectEntry = NewObject<UNiagaraStackObject>(this);
			ValueObjectEntry->Initialize(GetSystemViewModel(), GetEmitterViewModel(), ValueObject);
			ValueObjectEntry->SetItemIndentLevel(ItemIndentLevel + 1);
		}
		NewChildren.Add(ValueObjectEntry);
	}
	else
	{
		ValueObjectEntry = nullptr;
	}
}

void UNiagaraStackParameterStoreEntry::RefreshValueAndHandle()
{
	TSharedPtr<FNiagaraVariable> CurrentValueVariable = GetCurrentValueVariable();
	if (CurrentValueVariable.IsValid() && CurrentValueVariable->GetType() == InputType && CurrentValueVariable->IsDataAllocated())
	{
		if (LocalValueStruct.IsValid() == false || LocalValueStruct->GetStruct() != CurrentValueVariable->GetType().GetScriptStruct())
		{
			LocalValueStruct = MakeShared<FStructOnScope>(InputType.GetScriptStruct());
		}
		CurrentValueVariable->CopyTo(LocalValueStruct->GetStructMemory());
	}
	else
	{
		LocalValueStruct.Reset();
	}

	ValueObject = GetCurrentValueObject();

	ValueChangedDelegate.Broadcast();
}

FText UNiagaraStackParameterStoreEntry::GetDisplayName() const
{
	return DisplayName;
}

FName UNiagaraStackParameterStoreEntry::GetTextStyleName() const
{
	return "NiagaraEditor.Stack.ParameterText";
}

bool UNiagaraStackParameterStoreEntry::GetCanExpand() const
{
	return true;
}

TSharedPtr<FStructOnScope> UNiagaraStackParameterStoreEntry::GetValueStruct()
{
	return LocalValueStruct;
}

UNiagaraDataInterface* UNiagaraStackParameterStoreEntry::GetValueObject()
{
	return ValueObject;
}

void UNiagaraStackParameterStoreEntry::NotifyBeginValueChange()
{
	GEditor->BeginTransaction(LOCTEXT("BeginEditModuleInputValue", "Edit module input value."));
	Owner->Modify();
}

void UNiagaraStackParameterStoreEntry::NotifyEndValueChange()
{
	if (GEditor->IsTransactionActive())
	{
		GEditor->EndTransaction();
	}
}

void UNiagaraStackParameterStoreEntry::NotifyValueChanged()
{
	TSharedPtr<FNiagaraVariable> CurrentValue = GetCurrentValueVariable();
	if ((CurrentValue.IsValid() && LocalValueStruct.IsValid()) && FNiagaraEditorUtilities::DataMatches(*CurrentValue.Get(), *LocalValueStruct.Get()))
	{
		return;
	}
	else if ((CurrentValue.IsValid() && LocalValueStruct.IsValid()))
	{
		FNiagaraVariable DefaultVariable(InputType, ParameterName);
		ParameterStore->SetParameterData(LocalValueStruct->GetStructMemory(), DefaultVariable);
		GetSystemViewModel()->ResetSystem();
	}
}

bool UNiagaraStackParameterStoreEntry::CanReset() const
{
	return true;
}

void UNiagaraStackParameterStoreEntry::Reset()
{
	if (InputType.GetClass() == nullptr)
	{		
		// For struct inputs the override pin and anything attached to it should be removed.
		FScopedTransaction ScopedTransaction(LOCTEXT("ResetInputStructTransaction", "Reset the inputs value to default."));
	}
	else
	{
		// For object types make sure the override is setup to an input which matches the default object.
		FScopedTransaction ScopedTransaction(LOCTEXT("ResetInputObjectTransaction", "Reset the inputs data interface object to default."));
	}
	RefreshChildren();
}

bool UNiagaraStackParameterStoreEntry::CanRenameInput() const
{
	return false;
}

bool UNiagaraStackParameterStoreEntry::GetIsRenamePending() const
{
	return CanRenameInput() /*&& StackEditorData->GetModuleInputIsRenamePending(StackEditorDataKey)*/;
}

void UNiagaraStackParameterStoreEntry::SetIsRenamePending(bool bIsRenamePending)
{
	if (CanRenameInput())
	{
		//StackEditorData->SetModuleInputIsRenamePending(StackEditorDataKey, bIsRenamePending);
	}
}

void UNiagaraStackParameterStoreEntry::RenameInput(FString NewName)
{
	/*
	if (OwningAssignmentNode.IsValid() && InputParameterHandlePath.Num() == 1 && InputParameterHandle.GetName() != NewName)
	{
		bool bIsCurrentlyPinned = GetIsPinned();
		bool bIsCurrentlyExpanded = StackEditorData->GetStackEntryIsExpanded(FNiagaraStackGraphUtilities::GenerateStackModuleEditorDataKey(*OwningAssignmentNode), false);

		FNiagaraParameterHandle TargetHandle(OwningAssignmentNode->AssignmentTarget.GetName().ToString());
		FNiagaraParameterHandle RenamedTargetHandle(TargetHandle.GetNamespace(), NewName);
		OwningAssignmentNode->AssignmentTarget.SetName(*RenamedTargetHandle.GetParameterHandleString());
		OwningAssignmentNode->RefreshFromExternalChanges();

		InputParameterHandle = FNiagaraParameterHandle(InputParameterHandle.GetNamespace(), NewName);
		InputParameterHandlePath.Empty();
		InputParameterHandlePath.Add(InputParameterHandle);
		AliasedInputParameterHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(InputParameterHandle, OwningAssignmentNode.Get());
		DisplayName = FText::FromString(InputParameterHandle.GetName());

		UEdGraphPin* OverridePin = GetOverridePin();
		if (OverridePin != nullptr)
		{
			OverridePin->PinName = AliasedInputParameterHandle.GetParameterHandleString();
		}

		StackEditorDataKey = FNiagaraStackGraphUtilities::GenerateStackFunctionInputEditorDataKey(*OwningFunctionCallNode.Get(), InputParameterHandle);
		StackEditorData->SetModuleInputIsPinned(StackEditorDataKey, bIsCurrentlyPinned);
		StackEditorData->SetStackEntryIsExpanded(FNiagaraStackGraphUtilities::GenerateStackModuleEditorDataKey(*OwningAssignmentNode), bIsCurrentlyExpanded);

		CastChecked<UNiagaraGraph>(OwningAssignmentNode->GetGraph())->NotifyGraphNeedsRecompile();
	}
	*/
}

UNiagaraStackParameterStoreEntry::FOnValueChanged& UNiagaraStackParameterStoreEntry::OnValueChanged()
{
	return ValueChangedDelegate;
}

TSharedPtr<FNiagaraVariable> UNiagaraStackParameterStoreEntry::GetCurrentValueVariable()
{
	if (InputType.GetClass() == nullptr)
	{
		const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
		
		FNiagaraVariable DefaultVariable(InputType, ParameterName);
		const uint8* Data = ParameterStore->GetParameterData(DefaultVariable);
		DefaultVariable.SetData(Data);
		return MakeShared<FNiagaraVariable>(DefaultVariable);
	}
	return TSharedPtr<FNiagaraVariable>();
}

UNiagaraDataInterface* UNiagaraStackParameterStoreEntry::GetCurrentValueObject()
{
	if (InputType.GetClass() != nullptr)
	{
		FNiagaraVariable DefaultVariable(InputType, ParameterName);
		return ParameterStore->GetDataInterface(DefaultVariable);
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
