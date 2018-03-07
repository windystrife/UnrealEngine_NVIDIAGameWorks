// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimStateNodeBase.cpp
=============================================================================*/

#include "AnimStateNodeBase.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Animation/AnimBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/KismetEditorUtilities.h"
/////////////////////////////////////////////////////
// FAnimStateNodeNameValidator

class FAnimStateNodeNameValidator : public FStringSetNameValidator
{
public:
	FAnimStateNodeNameValidator(const UAnimStateNodeBase* InStateNode)
		: FStringSetNameValidator(FString())
	{
		TArray<UAnimStateNodeBase*> Nodes;
		UAnimationStateMachineGraph* StateMachine = CastChecked<UAnimationStateMachineGraph>(InStateNode->GetOuter());

		StateMachine->GetNodesOfClass<UAnimStateNodeBase>(Nodes);
		for (auto NodeIt = Nodes.CreateIterator(); NodeIt; ++NodeIt)
		{
			auto Node = *NodeIt;
			if (Node != InStateNode)
			{
				Names.Add(Node->GetStateName());
			}
		}
	}
};

/////////////////////////////////////////////////////
// UAnimStateNodeBase

UAnimStateNodeBase::UAnimStateNodeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimStateNodeBase::PostPasteNode()
{
	Super::PostPasteNode();

	if (UEdGraph* BoundGraph = GetBoundGraph())
	{
		// Add the new graph as a child of our parent graph
		UEdGraph* ParentGraph = GetGraph();

		if(ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
		{
			ParentGraph->SubGraphs.Add(BoundGraph);
		}

		//@TODO: CONDUIT: Merge conflict - May no longer be necessary due to other changes?
//		FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, GetDesiredNewNodeName());
		//@ENDTODO

		// Restore transactional flag that is lost during copy/paste process
		BoundGraph->SetFlags(RF_Transactional);

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

UObject* UAnimStateNodeBase::GetJumpTargetForDoubleClick() const
{
	return GetBoundGraph();
}

bool UAnimStateNodeBase::CanJumpToDefinition() const
{
	return GetJumpTargetForDoubleClick() != nullptr;
}

void UAnimStateNodeBase::JumpToDefinition() const
{
	if (UObject* HyperlinkTarget = GetJumpTargetForDoubleClick())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(HyperlinkTarget);
	}
}

bool UAnimStateNodeBase::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(UAnimationStateMachineSchema::StaticClass());
}

void UAnimStateNodeBase::OnRenameNode(const FString& NewName)
{
	FBlueprintEditorUtils::RenameGraph(GetBoundGraph(), NewName);
}

TSharedPtr<class INameValidatorInterface> UAnimStateNodeBase::MakeNameValidator() const
{
	return MakeShareable(new FAnimStateNodeNameValidator(this));
}

FString UAnimStateNodeBase::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/AnimationStateMachine");
}

void UAnimStateNodeBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
}

void UAnimStateNodeBase::PostLoad()
{
	Super::PostLoad();

	const int32 CustomVersion = GetLinkerCustomVersion(FFrameworkObjectVersion::GUID);

	if(CustomVersion < FFrameworkObjectVersion::FixNonTransactionalPins)
	{
		int32 BrokenPinCount = 0;
		for(UEdGraphPin_Deprecated* Pin : DeprecatedPins)
		{
			if(!Pin->HasAnyFlags(RF_Transactional))
			{
				++BrokenPinCount;
				Pin->SetFlags(Pin->GetFlags() | RF_Transactional);
			}
		}

		if(BrokenPinCount > 0)
		{
			UE_LOG(LogAnimation, Log, TEXT("Fixed %d non-transactional pins in %s"), BrokenPinCount, *GetName());
		}
	}
}

UAnimBlueprint* UAnimStateNodeBase::GetAnimBlueprint() const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this);
	return CastChecked<UAnimBlueprint>(Blueprint);
}

