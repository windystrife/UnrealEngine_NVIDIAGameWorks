// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_StateMachineBase.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "Kismet2/Kismet2NameValidators.h"

#include "AnimationGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimGraphNode_StateMachine.h"
#include "Kismet2/KismetEditorUtilities.h"

/////////////////////////////////////////////////////
// FAnimStateMachineNodeNameValidator

class FAnimStateMachineNodeNameValidator : public FStringSetNameValidator
{
public:
	FAnimStateMachineNodeNameValidator(const UAnimGraphNode_StateMachineBase* InStateMachineNode)
		: FStringSetNameValidator(FString())
	{
		TArray<UAnimGraphNode_StateMachineBase*> Nodes;

		UAnimationGraph* StateMachine = CastChecked<UAnimationGraph>(InStateMachineNode->GetOuter());
		StateMachine->GetNodesOfClassEx<UAnimGraphNode_StateMachine, UAnimGraphNode_StateMachineBase>(Nodes);

		for (auto Node : Nodes)
		{
			if (Node != InStateMachineNode)
			{
				Names.Add(Node->GetStateMachineName());
			}
		}
	}
};

/////////////////////////////////////////////////////
// UAnimGraphNode_StateMachineBase

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_StateMachineBase::UAnimGraphNode_StateMachineBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_StateMachineBase::GetNodeTitleColor() const
{
	return FLinearColor(0.8f, 0.8f, 0.8f);
}

FText UAnimGraphNode_StateMachineBase::GetTooltipText() const
{
	return LOCTEXT("StateMachineTooltip", "Animation State Machine");
}

FText UAnimGraphNode_StateMachineBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::MenuTitle || TitleType == ENodeTitleType::ListView) && (EditorStateMachineGraph == nullptr))
	{
		return LOCTEXT("AddNewStateMachine", "Add New State Machine...");
	}
	else if (EditorStateMachineGraph == nullptr)
	{
		if (TitleType == ENodeTitleType::FullTitle)
		{
			return LOCTEXT("NullStateMachineFullTitle", "Error: No Graph\nState Machine");
		}
		else
		{
			return LOCTEXT("ErrorNoGraph", "Error: No Graph");
		}
	}
	else if (TitleType == ENodeTitleType::FullTitle)
	{
		if (CachedFullTitle.IsOutOfDate(this))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Title"), FText::FromName(EditorStateMachineGraph->GetFName()));
			// FText::Format() is slow, so we cache this to save on performance
			CachedFullTitle.SetCachedText(FText::Format(LOCTEXT("StateMachineFullTitle", "{Title}\nState Machine"), Args), this);
		}
		return CachedFullTitle;
	}

	return FText::FromName(EditorStateMachineGraph->GetFName());
}

FString UAnimGraphNode_StateMachineBase::GetNodeCategory() const
{
	return TEXT("State Machines");
}

void UAnimGraphNode_StateMachineBase::PostPlacedNewNode()
{
	// Create a new animation graph
	check(EditorStateMachineGraph == NULL);
	EditorStateMachineGraph = CastChecked<UAnimationStateMachineGraph>(FBlueprintEditorUtils::CreateNewGraph(this, NAME_None, UAnimationStateMachineGraph::StaticClass(), UAnimationStateMachineSchema::StaticClass()));
	check(EditorStateMachineGraph);
	EditorStateMachineGraph->OwnerAnimGraphNode = this;

	// Find an interesting name
	TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(EditorStateMachineGraph, NameValidator, TEXT("New State Machine"));

	// Initialize the anim graph
	const UEdGraphSchema* Schema = EditorStateMachineGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*EditorStateMachineGraph);

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = GetGraph();
	
	if(ParentGraph->SubGraphs.Find(EditorStateMachineGraph) == INDEX_NONE)
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Add(EditorStateMachineGraph);
	}
}

UObject* UAnimGraphNode_StateMachineBase::GetJumpTargetForDoubleClick() const
{
	// Open the state machine graph
	return EditorStateMachineGraph;
}

void UAnimGraphNode_StateMachineBase::JumpToDefinition() const
{
	if (UObject* HyperlinkTarget = GetJumpTargetForDoubleClick())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(HyperlinkTarget);
	}
}

void UAnimGraphNode_StateMachineBase::DestroyNode()
{
	UEdGraph* GraphToRemove = EditorStateMachineGraph;

	EditorStateMachineGraph = NULL;
	Super::DestroyNode();

	if (GraphToRemove)
	{
		UBlueprint* Blueprint = GetBlueprint();
		GraphToRemove->Modify();
		FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphToRemove, EGraphRemoveFlags::Recompile);
	}
}

void UAnimGraphNode_StateMachineBase::PostPasteNode()
{
	Super::PostPasteNode();

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = GetGraph();

	if(ParentGraph->SubGraphs.Find(EditorStateMachineGraph) == INDEX_NONE)
	{
		ParentGraph->SubGraphs.Add(EditorStateMachineGraph);
	}

	// Find an interesting name
	TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(EditorStateMachineGraph, NameValidator, EditorStateMachineGraph->GetName());

	//restore transactional flag that is lost during copy/paste process
	EditorStateMachineGraph->SetFlags(RF_Transactional);
}

FString UAnimGraphNode_StateMachineBase::GetStateMachineName()
{
	return (EditorStateMachineGraph != NULL) ? *(EditorStateMachineGraph->GetName()) : TEXT("(null)");
}

TSharedPtr<class INameValidatorInterface> UAnimGraphNode_StateMachineBase::MakeNameValidator() const
{
	return MakeShareable(new FAnimStateMachineNodeNameValidator(this));
}

FString UAnimGraphNode_StateMachineBase::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/AnimationStateMachine");
}

void UAnimGraphNode_StateMachineBase::OnRenameNode(const FString& NewName)
{
	FBlueprintEditorUtils::RenameGraph(EditorStateMachineGraph, NewName);
}

#undef LOCTEXT_NAMESPACE
