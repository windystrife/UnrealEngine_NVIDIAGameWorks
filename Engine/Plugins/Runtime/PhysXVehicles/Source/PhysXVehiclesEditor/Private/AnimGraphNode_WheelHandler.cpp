// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_WheelHandler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_WheelHandler

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_WheelHandler::UAnimGraphNode_WheelHandler(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_WheelHandler::GetControllerDescription() const
{
	return LOCTEXT("AnimGraphNode_WheelHandler", "Wheel Handler for WheeledVehicle");
}

FText UAnimGraphNode_WheelHandler::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_WheelHandler_Tooltip", "This alters the wheel transform based on set up in Wheeled Vehicle. This only works when the owner is WheeledVehicle.");
}

FText UAnimGraphNode_WheelHandler::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText NodeTitle;
	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		NodeTitle = GetControllerDescription();
	}
	else
	{
		// we don't have any run-time information, so it's limited to print  
		// anymore than what it is it would be nice to print more data such as 
		// name of bones for wheels, but it's not available in Persona
		NodeTitle = FText(LOCTEXT("AnimGraphNode_WheelHandler_Title", "Wheel Handler"));
	}	
	return NodeTitle;
}

void UAnimGraphNode_WheelHandler::ValidateAnimNodePostCompile(class FCompilerResultsLog& MessageLog, class UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex)
{
	// we only support vehicle anim instance
	if ( CompiledClass->IsChildOf(UVehicleAnimInstance::StaticClass())  == false )
	{
		MessageLog.Error(TEXT("@@ is only allowwed in VehicleAnimInstance. If this is for vehicle, please change parent to be VehicleAnimInstancen (Reparent Class)."), this);
	}
}

bool UAnimGraphNode_WheelHandler::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	return (Blueprint != nullptr) && Blueprint->ParentClass->IsChildOf<UVehicleAnimInstance>() && Super::IsCompatibleWithGraph(TargetGraph);
}

#undef LOCTEXT_NAMESPACE
