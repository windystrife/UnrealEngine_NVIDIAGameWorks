// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeGraphNode_Root.h"
#include "UObject/UObjectIterator.h"
#include "BehaviorTreeEditorTypes.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BehaviorTree.h"

UBehaviorTreeGraphNode_Root::UBehaviorTreeGraphNode_Root(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsReadOnly = true;
}

void UBehaviorTreeGraphNode_Root::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	// pick first available blackboard asset, hopefully something will be loaded...
	for (FObjectIterator It(UBlackboardData::StaticClass()); It; ++It)
	{
		UBlackboardData* TestOb = (UBlackboardData*)*It;
		if (!TestOb->HasAnyFlags(RF_ClassDefaultObject))
		{
			BlackboardAsset = TestOb;
			UpdateBlackboard();
			break;
		}
	}
}

void UBehaviorTreeGraphNode_Root::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UBehaviorTreeEditorTypes::PinCategory_SingleComposite, FString(), nullptr, TEXT("In"));
}

FText UBehaviorTreeGraphNode_Root::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("BehaviorTreeEditor", "Root", "ROOT");
}

FName UBehaviorTreeGraphNode_Root::GetNameIcon() const
{
	return FName("BTEditor.Graph.BTNode.Root.Icon");
}

FText UBehaviorTreeGraphNode_Root::GetTooltipText() const
{
	return UEdGraphNode::GetTooltipText();
}

void UBehaviorTreeGraphNode_Root::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UBehaviorTreeGraphNode_Root, BlackboardAsset))
	{
		UpdateBlackboard();
	}
}

void UBehaviorTreeGraphNode_Root::PostEditUndo()
{
	Super::PostEditUndo();
	UpdateBlackboard();
}

FText UBehaviorTreeGraphNode_Root::GetDescription() const
{
	return FText::FromString(GetNameSafe(BlackboardAsset));
}

void UBehaviorTreeGraphNode_Root::UpdateBlackboard()
{
	UBehaviorTreeGraph* MyGraph = GetBehaviorTreeGraph();
	UBehaviorTree* BTAsset = Cast<UBehaviorTree>(MyGraph->GetOuter());
	if (BTAsset && BTAsset->BlackboardAsset != BlackboardAsset)
	{
		BTAsset->BlackboardAsset = BlackboardAsset;
		MyGraph->UpdateBlackboardChange();
	}
}
