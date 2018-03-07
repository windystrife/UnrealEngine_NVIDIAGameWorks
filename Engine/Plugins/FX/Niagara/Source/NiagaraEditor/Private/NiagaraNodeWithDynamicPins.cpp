// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "NiagaraNodeWithDynamicPins.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraGraph.h"
#include "UIAction.h"
#include "ScopedTransaction.h"
#include "MultiBoxBuilder.h"
#include "SEditableTextBox.h"
#include "SBox.h"
#include "SNiagaraGraphPinAdd.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraEditorUtilities.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeWithDynamicPins"

const FString UNiagaraNodeWithDynamicPins::AddPinSubCategory("DynamicAddPin");

void UNiagaraNodeWithDynamicPins::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	// Check if an add pin was connected and convert it to a typed connection.
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	if (Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc && Pin->PinType.PinSubCategory == AddPinSubCategory && Pin->LinkedTo.Num() > 0)
	{
		FNiagaraTypeDefinition LinkedPinType = Schema->PinToTypeDefinition(Pin->LinkedTo[0]);
		Pin->PinType = Schema->TypeDefinitionToPinType(LinkedPinType);
		Pin->PinName = Pin->LinkedTo[0]->GetName();

		CreateAddPin(Pin->Direction);
		OnNewTypedPinAdded(Pin);
		GetGraph()->NotifyGraphChanged();
	}
}

UEdGraphPin* GetAddPin(TArray<UEdGraphPin*> Pins, EEdGraphPinDirection Direction)
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == Direction &&
			Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc && 
			Pin->PinType.PinSubCategory == UNiagaraNodeWithDynamicPins::AddPinSubCategory)
		{
			return Pin;
		}
	}
	return nullptr;
}

bool UNiagaraNodeWithDynamicPins::AllowNiagaraTypeForAddPin(const FNiagaraTypeDefinition& InType)
{
	return  InType != FNiagaraTypeDefinition::GetGenericNumericDef() && InType.GetScriptStruct() != nullptr;
}

UEdGraphPin* UNiagaraNodeWithDynamicPins::RequestNewTypedPin(EEdGraphPinDirection Direction, const FNiagaraTypeDefinition& Type)
{
	FString DefaultName;
	if (Direction == EGPD_Input)
	{
		TArray<UEdGraphPin*> InPins;
		GetInputPins(InPins);
		DefaultName = TEXT("Input ") + LexicalConversion::ToString(InPins.Num());
	}
	else
	{
		TArray<UEdGraphPin*> OutPins;
		GetOutputPins(OutPins);
		DefaultName = TEXT("Output ") + LexicalConversion::ToString(OutPins.Num());
	}
	return RequestNewTypedPin(Direction, Type, DefaultName);
}

UEdGraphPin* UNiagaraNodeWithDynamicPins::RequestNewTypedPin(EEdGraphPinDirection Direction, const FNiagaraTypeDefinition& Type, FString InName)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	UEdGraphPin* AddPin = GetAddPin(GetAllPins(), Direction);
	checkf(AddPin != nullptr, TEXT("Add pin is missing"));
	AddPin->Modify();
	AddPin->PinType = Schema->TypeDefinitionToPinType(Type);
	AddPin->PinName = InName;

	CreateAddPin(Direction);
	OnNewTypedPinAdded(AddPin);
	UNiagaraGraph* Graph = GetNiagaraGraph();
	Graph->NotifyGraphNeedsRecompile();

	return AddPin;
}

void UNiagaraNodeWithDynamicPins::CreateAddPin(EEdGraphPinDirection Direction)
{
	CreatePin(Direction, FEdGraphPinType(UEdGraphSchema_Niagara::PinCategoryMisc, AddPinSubCategory, nullptr, EPinContainerType::None, false, FEdGraphTerminalType()), TEXT("Add"));
}

bool UNiagaraNodeWithDynamicPins::IsAddPin(const UEdGraphPin* Pin) const
{
	return Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc && 
		Pin->PinType.PinSubCategory == UNiagaraNodeWithDynamicPins::AddPinSubCategory;
}

bool UNiagaraNodeWithDynamicPins::CanRenamePin(const UEdGraphPin* Pin) const
{
	return IsAddPin(Pin) == false;
}

bool UNiagaraNodeWithDynamicPins::CanRemovePin(const UEdGraphPin* Pin) const
{
	return IsAddPin(Pin) == false;
}

bool UNiagaraNodeWithDynamicPins::CanMovePin(const UEdGraphPin* Pin) const
{
	return IsAddPin(Pin) == false;
}

void UNiagaraNodeWithDynamicPins::MoveDynamicPin(UEdGraphPin* Pin, int32 DirectionToMove)
{
	TArray<UEdGraphPin*> SameDirectionPins;
	if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		GetInputPins(SameDirectionPins);
	}
	else
	{
		GetOutputPins(SameDirectionPins);
	}

	for (int32 i = 0; i < SameDirectionPins.Num(); i++)
	{
		if (SameDirectionPins[i] == Pin)
		{
			if (i + DirectionToMove >= 0 && i + DirectionToMove < SameDirectionPins.Num())
			{
				FScopedTransaction MovePinTransaction(LOCTEXT("MovePinTransaction", "Moved pin"));
				Modify();
				UEdGraphPin* PinOld = SameDirectionPins[i + DirectionToMove];
				if (PinOld)
					PinOld->Modify();
				Pin->Modify();

				int32 RealPinIdx = INDEX_NONE;
				int32 SwapRealPinIdx = INDEX_NONE;
				Pins.Find(Pin, RealPinIdx);
				Pins.Find(PinOld, SwapRealPinIdx);
				
				Pins[SwapRealPinIdx] = Pin;
				Pins[RealPinIdx] = PinOld;
				GetGraph()->NotifyGraphChanged();
				break;
			}
		}
	}
}


void UNiagaraNodeWithDynamicPins::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);
	if (Context.Pin != nullptr)
	{
		Context.MenuBuilder->BeginSection("EdGraphSchema_NiagaraPinActions", LOCTEXT("EditPinMenuHeader", "Edit Pin"));
		if (CanRenamePin(Context.Pin))
		{
			UEdGraphPin* Pin = const_cast<UEdGraphPin*>(Context.Pin);
			TSharedRef<SWidget> RenameWidget =
				SNew(SBox)
				.WidthOverride(100)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(SEditableTextBox)
					.Text_UObject(this, &UNiagaraNodeWithDynamicPins::GetPinNameText, Pin)
					.OnTextCommitted_UObject(this, &UNiagaraNodeWithDynamicPins::PinNameTextCommitted, Pin)
				];
			Context.MenuBuilder->AddWidget(RenameWidget, LOCTEXT("NameMenuItem", "Name"));
		}
		if (CanRemovePin(Context.Pin))
		{
			Context.MenuBuilder->AddMenuEntry(
				LOCTEXT("RemoveDynamicPin", "Remove pin"),
				LOCTEXT("RemoveDynamicPinToolTip", "Remove this pin and any connections."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateUObject(const_cast<UNiagaraNodeWithDynamicPins*>(this), &UNiagaraNodeWithDynamicPins::RemoveDynamicPin, const_cast<UEdGraphPin*>(Context.Pin))));
		}
		if (CanMovePin(Context.Pin))
		{
			TArray<UEdGraphPin*> SameDirectionPins;
			if (Context.Pin->Direction == EEdGraphPinDirection::EGPD_Input)
			{
				GetInputPins(SameDirectionPins);
			}
			else
			{
				GetOutputPins(SameDirectionPins);
			}
			int32 PinIdx = INDEX_NONE;
			SameDirectionPins.Find(const_cast<UEdGraphPin*>(Context.Pin), PinIdx);

			if (PinIdx != 0)
			{
				Context.MenuBuilder->AddMenuEntry(
					LOCTEXT("MoveDynamicPinUp", "Move pin up"),
					LOCTEXT("MoveDynamicPinToolTipUp", "Move this pin and any connections one slot up."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject(const_cast<UNiagaraNodeWithDynamicPins*>(this), &UNiagaraNodeWithDynamicPins::MoveDynamicPin, const_cast<UEdGraphPin*>(Context.Pin), -1)));
			}
			if (PinIdx >= 0 && PinIdx < SameDirectionPins.Num() - 1)
			{
				Context.MenuBuilder->AddMenuEntry(
					LOCTEXT("MoveDynamicPinDown", "Move pin down"),
					LOCTEXT("MoveDynamicPinToolTipDown", "Move this pin and any connections one slot down."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject(const_cast<UNiagaraNodeWithDynamicPins*>(this), &UNiagaraNodeWithDynamicPins::MoveDynamicPin, const_cast<UEdGraphPin*>(Context.Pin), 1)));
			}
		}
	}
}

TSharedRef<SWidget> UNiagaraNodeWithDynamicPins::GenerateAddPinMenu(const FString& InWorkingPinName, SNiagaraGraphPinAdd* InPin)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	BuildTypeMenu(MenuBuilder, InWorkingPinName, InPin);
	return MenuBuilder.MakeWidget();
}

void UNiagaraNodeWithDynamicPins::BuildTypeMenu(FMenuBuilder& InMenuBuilder, const FString& InWorkingName, SNiagaraGraphPinAdd* InPin)
{
	TArray<FNiagaraTypeDefinition> Types = FNiagaraTypeRegistry::GetRegisteredTypes();
	Types.Sort([](const FNiagaraTypeDefinition& A, const FNiagaraTypeDefinition& B) { return (A.GetNameText().ToLower().ToString() < B.GetNameText().ToLower().ToString()); });

	for (const FNiagaraTypeDefinition& RegisteredType : Types)
	{
		bool bAllowType = false;
		bAllowType = AllowNiagaraTypeForAddPin(RegisteredType);

		if (bAllowType)
		{
			FNiagaraVariable Var(RegisteredType, *InWorkingName);
			FNiagaraEditorUtilities::ResetVariableToDefaultValue(Var);

			InMenuBuilder.AddMenuEntry(
				RegisteredType.GetNameText(),
				FText::Format(LOCTEXT("AddButtonTypeEntryToolTipFormat", "Add a new {0} pin"), RegisteredType.GetNameText()),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(InPin, &SNiagaraGraphPinAdd::OnAddType, Var)));
		}
	}
}

void UNiagaraNodeWithDynamicPins::RemoveDynamicPin(UEdGraphPin* Pin)
{
	FScopedTransaction RemovePinTransaction(LOCTEXT("RemovePinTransaction", "Remove pin"));
	RemovePin(Pin);
	GetGraph()->NotifyGraphChanged();
}

FText UNiagaraNodeWithDynamicPins::GetPinNameText(UEdGraphPin* Pin) const
{
	return FText::FromString(Pin->PinName);
}


void UNiagaraNodeWithDynamicPins::PinNameTextCommitted(const FText& Text, ETextCommit::Type CommitType, UEdGraphPin* Pin)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		FScopedTransaction RemovePinTransaction(LOCTEXT("RenamePinTransaction", "Rename pin"));
		Modify();

		Pin->PinName = Text.ToString();
		OnPinRenamed(Pin);
	}
}

#undef LOCTEXT_NAMESPACE