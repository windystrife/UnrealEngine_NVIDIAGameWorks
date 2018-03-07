// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_ControlRigComponentInputOutput.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "K2Node_ControlRigComponentInputOutput"

UK2Node_ControlRigComponentInputOutput::UK2Node_ControlRigComponentInputOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ControlRigComponentPinName = LOCTEXT("ControlRigComponentPinName", "ControlRig Component").ToString();
}

void UK2Node_ControlRigComponentInputOutput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UK2Node_ControlRigComponentInputOutput, ControlRigType))
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		Schema->ForceVisualizationCacheClear();

		ReconstructNode();
	}
}

void UK2Node_ControlRigComponentInputOutput::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Optionally create input pin for ControlRig
	if (IsInActor())
	{
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, FString(), UControlRigComponent::StaticClass(), ControlRigComponentPinName, EPinContainerType::None, true);
	}
}

bool UK2Node_ControlRigComponentInputOutput::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Graph->GetOuter());
	const bool bCompatibleWithValidParentClass = Super::IsCompatibleWithGraph(Graph) && Blueprint && Blueprint->ParentClass;
	return bCompatibleWithValidParentClass && (Blueprint->ParentClass->IsChildOf(UControlRigComponent::StaticClass()) || Blueprint->ParentClass->IsChildOf(AActor::StaticClass()));
}

void UK2Node_ControlRigComponentInputOutput::EarlyValidation(class FCompilerResultsLog& MessageLog) const
{
	Super::EarlyValidation(MessageLog);

	if (IsInActor())
	{
		if (ControlRigType.Get() == nullptr)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("ControlRigComponentInputOutput_MissingControlRigType", "Please specify a ControlRig Type in @@").ToString()), this);
		}
	}
}

const UClass* UK2Node_ControlRigComponentInputOutput::GetControlRigClassImpl() const
{
	if (IsInActor())
	{
		// if we are contained within an actor, we need to refer to our internal class setting to decide on what ControlRig we are going to use
		return ControlRigType.Get();
	}
	else
	{
		return GetControlRigClassFromBlueprint(GetBlueprint());
	}
}

bool UK2Node_ControlRigComponentInputOutput::IsInActor() const
{
	const UClass* ParentClass = GetBlueprint()->ParentClass.Get();
	return (ParentClass && ParentClass->IsChildOf(AActor::StaticClass()));
}

#undef LOCTEXT_NAMESPACE
