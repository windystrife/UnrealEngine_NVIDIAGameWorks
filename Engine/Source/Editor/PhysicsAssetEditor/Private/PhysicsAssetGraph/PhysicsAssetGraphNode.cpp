// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetGraphNode.h"
#include "PhysicsAssetGraph.h"
#include "EdGraph/EdGraphPin.h"

UPhysicsAssetGraphNode::UPhysicsAssetGraphNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputPin = nullptr;
	OutputPin = nullptr;
}

void UPhysicsAssetGraphNode::SetupPhysicsAssetNode()
{

}

UObject* UPhysicsAssetGraphNode::GetDetailsObject()
{
	return nullptr;
}

UPhysicsAssetGraph* UPhysicsAssetGraphNode::GetPhysicsAssetGraph() const
{
	return CastChecked<UPhysicsAssetGraph>(GetOuter());
}

FText UPhysicsAssetGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NodeTitle;
}

void UPhysicsAssetGraphNode::AllocateDefaultPins()
{
	InputPin = CreatePin(EEdGraphPinDirection::EGPD_Input, TEXT(""), TEXT(""), nullptr, TEXT(""));
	InputPin->bHidden = true;
	OutputPin = CreatePin(EEdGraphPinDirection::EGPD_Output, TEXT(""), TEXT(""), nullptr, TEXT(""));
	OutputPin->bHidden = true;
}

UEdGraphPin& UPhysicsAssetGraphNode::GetInputPin() const
{
	return *InputPin;
}

UEdGraphPin& UPhysicsAssetGraphNode::GetOutputPin() const
{
	return *OutputPin;
}
