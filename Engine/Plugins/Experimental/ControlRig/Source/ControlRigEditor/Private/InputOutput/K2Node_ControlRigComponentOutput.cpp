// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_ControlRigComponentOutput.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "ControlRig.h"
#include "Textures/SlateIcon.h"
#include "ControlRigComponent.h"
#include "ControlRigField.h"

#define LOCTEXT_NAMESPACE "K2Node_ControlRigComponentOutput"

UK2Node_ControlRigComponentOutput::UK2Node_ControlRigComponentOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeTooltip = LOCTEXT("NodeTooltip", "Gets animation ControlRig outputs");
}

void UK2Node_ControlRigComponentOutput::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	CreateOutputPins();
}

void UK2Node_ControlRigComponentOutput::CreateOutputPins()
{
	TArray<TSharedRef<IControlRigField>> OutputInfos = GetOutputVariableInfo(DisabledOutputs);

	for (const TSharedRef<IControlRigField>& OutputInfo : OutputInfos)
	{
		CreatePin(EGPD_Output, OutputInfo->GetPinType(), OutputInfo->GetPinString());
	}
}

FText UK2Node_ControlRigComponentOutput::GetTooltipText() const
{
	return NodeTooltip;
}

FText UK2Node_ControlRigComponentOutput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("ControlRigOutput_Title", "Get Animation ControlRig Output");
}

void UK2Node_ControlRigComponentOutput::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (CompilerContext.bIsFullCompile)
	{
		if (UClass* Class = GetControlRigClass())
		{
			const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

			// Add getter for ControlRig
			UK2Node_CallFunction* CallFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			CallFunction->SetFromFunction(UControlRigComponent::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UControlRigComponent, BP_GetControlRig)));
			CallFunction->AllocateDefaultPins();
			UEdGraphPin* ControlRigValuePin = CallFunction->GetReturnValuePin();

			// hook up self if we are in an actor
			if (IsInActor())
			{
				UEdGraphPin* SelfPin = CallFunction->FindPinChecked(UEdGraphSchema_K2::PN_Self, EGPD_Input);
				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(ControlRigComponentPinName, EGPD_Input), *SelfPin);
			}

			// Cast to correct type
			UK2Node_DynamicCast* DynamicCast = CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(this, SourceGraph);
			DynamicCast->TargetType = Class;
			DynamicCast->SetPurity(true);
			DynamicCast->AllocateDefaultPins();

			ControlRigValuePin->MakeLinkTo(DynamicCast->GetCastSourcePin());
			ControlRigValuePin = DynamicCast->GetCastResultPin();
			DynamicCast->NotifyPinConnectionListChanged(DynamicCast->GetCastSourcePin());

			// Hook up output parameter pins to getters
			TArray<UEdGraphPin*> OutputPins;
			TArray<TSharedRef<IControlRigField>> FieldInfo;
			GetOutputParameterPins(DisabledOutputs, OutputPins, FieldInfo);

			for (int32 PinIndex = 0; PinIndex < OutputPins.Num(); PinIndex++)
			{
				UEdGraphPin* ExecPath = nullptr;
				FieldInfo[PinIndex]->ExpandPin(GetControlRigClass(), CompilerContext, SourceGraph, this, OutputPins[PinIndex], ControlRigValuePin, false, ExecPath);
			}
		}

		BreakAllNodeLinks();
	}
}

#undef LOCTEXT_NAMESPACE
