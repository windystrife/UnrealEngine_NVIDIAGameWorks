// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_SequenceEvaluator.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "Kismet2/CompilerResultsLog.h"
#include "GraphEditorActions.h"
#include "Animation/AnimComposite.h"
#include "AnimGraphNode_SequenceEvaluator.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_SequenceEvaluator

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_SequenceEvaluator::UAnimGraphNode_SequenceEvaluator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimGraphNode_SequenceEvaluator::PreloadRequiredAssets()
{
	PreloadObject(Node.Sequence);

	Super::PreloadRequiredAssets();
}

void UAnimGraphNode_SequenceEvaluator::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	Node.GroupIndex = AnimBlueprint->FindOrAddGroup(SyncGroup.GroupName);
	Node.GroupRole = SyncGroup.GroupRole;
}

void UAnimGraphNode_SequenceEvaluator::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if(Node.Sequence)
	{
		HandleAnimReferenceCollection(Node.Sequence, AnimationAssets);
	}
}

void UAnimGraphNode_SequenceEvaluator::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	HandleAnimReferenceReplacement(Node.Sequence, AnimAssetReplacementMap);
}

FText UAnimGraphNode_SequenceEvaluator::GetTooltipText() const
{
	// FText::Format() is slow, so we utilize the cached list title
	return GetNodeTitle(ENodeTitleType::ListView);
}

FText UAnimGraphNode_SequenceEvaluator::GetNodeTitleForSequence(ENodeTitleType::Type TitleType, UAnimSequenceBase* InSequence) const
{
	const FText SequenceName = FText::FromString(InSequence->GetName());

	FFormatNamedArguments Args;
	Args.Add(TEXT("SequenceName"), SequenceName);

	// FText::Format() is slow, so we cache this to save on performance
	if (InSequence->IsValidAdditive())
	{
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("EvaluateSequence_Additive", "Evaluate {SequenceName} (additive)"), Args), this);
	}
	else
	{
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("EvaluateSequence", "Evaluate {SequenceName}"), Args), this);
	}

	return CachedNodeTitle;
}

FText UAnimGraphNode_SequenceEvaluator::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Node.Sequence == nullptr)
	{
		// we may have a valid variable connected or default pin value
		UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequenceEvaluator, Sequence));
		if (SequencePin && SequencePin->LinkedTo.Num() > 0)
		{
			return LOCTEXT("EvaluateSequence_TitleVariable", "Evaluate Animation Sequence");
		}
		else if (SequencePin && SequencePin->DefaultObject != nullptr)
		{
			return GetNodeTitleForSequence(TitleType, CastChecked<UAnimSequenceBase>(SequencePin->DefaultObject));
		}
		else
		{
			return LOCTEXT("EvaluateSequence_TitleNONE", "Evaluate (None)");
		}
	}
	// @TODO: don't know enough about this node type to comfortably assert that
	//        the CacheName won't change after the node has spawned... until
	//        then, we'll leave this optimization off
	else //if (CachedNodeTitle.IsOutOfDate(this))
	{
		GetNodeTitleForSequence(TitleType, Node.Sequence);
	}

	return CachedNodeTitle;
}

void UAnimGraphNode_SequenceEvaluator::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// Intentionally empty; you can drop down a regular sequence player and convert into a sequence evaluator in the right-click menu.
}

void UAnimGraphNode_SequenceEvaluator::SetAnimationAsset(UAnimationAsset* Asset)
{
	if (UAnimSequenceBase* Seq =  Cast<UAnimSequence>(Asset))
	{
		Node.Sequence = Seq;
	}
}

void UAnimGraphNode_SequenceEvaluator::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	UAnimSequenceBase* SequenceToCheck = Node.Sequence;
	UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequenceEvaluator, Sequence));
	if (SequencePin != nullptr && SequenceToCheck == nullptr)
	{
		SequenceToCheck = Cast<UAnimSequenceBase>(SequencePin->DefaultObject);
	}

	if (SequenceToCheck == nullptr)
	{
		// we may have a connected node
		if (SequencePin == nullptr || SequencePin->LinkedTo.Num() == 0)
		{
			MessageLog.Error(TEXT("@@ references an unknown sequence"), this);
		}
	}
	else
	{
		USkeleton* SeqSkeleton = SequenceToCheck->GetSkeleton();
		if (SeqSkeleton&& // if anim sequence doesn't have skeleton, it might be due to anim sequence not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!SeqSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references sequence that uses different skeleton @@"), this, SeqSkeleton);
		}
	}
}

void UAnimGraphNode_SequenceEvaluator::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		// add an option to convert to a regular sequence player
		Context.MenuBuilder->BeginSection("AnimGraphNodeSequenceEvaluator", NSLOCTEXT("A3Nodes", "SequenceEvaluatorHeading", "Sequence Evaluator"));
		{
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ConvertToSeqPlayer);
		}
		Context.MenuBuilder->EndSection();
	}
}

bool UAnimGraphNode_SequenceEvaluator::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_SequenceEvaluator::GetAnimationAsset() const 
{
	UAnimSequenceBase* Sequence = Node.Sequence;
	UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequenceEvaluator, Sequence));
	if (SequencePin != nullptr && Sequence == nullptr)
	{
		Sequence = Cast<UAnimSequenceBase>(SequencePin->DefaultObject);
	}

	return Node.Sequence;
}

const TCHAR* UAnimGraphNode_SequenceEvaluator::GetTimePropertyName() const 
{
	return TEXT("ExplicitTime");
}

UScriptStruct* UAnimGraphNode_SequenceEvaluator::GetTimePropertyStruct() const 
{
	return FAnimNode_SequenceEvaluator::StaticStruct();
}

EAnimAssetHandlerType UAnimGraphNode_SequenceEvaluator::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UAnimSequence::StaticClass()) || AssetClass->IsChildOf(UAnimComposite::StaticClass()))
	{
		return EAnimAssetHandlerType::Supported;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

#undef LOCTEXT_NAMESPACE
