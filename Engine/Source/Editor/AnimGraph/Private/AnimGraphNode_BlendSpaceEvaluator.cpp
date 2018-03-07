// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_BlendSpaceEvaluator.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditorActions.h"
#include "Kismet2/CompilerResultsLog.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_BlendSpaceEvaluator

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_BlendSpaceEvaluator::UAnimGraphNode_BlendSpaceEvaluator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_BlendSpaceEvaluator::GetTooltipText() const
{
	// FText::Format() is slow, so we utilize the cached list title
	return GetNodeTitle(ENodeTitleType::ListView);
}

FText UAnimGraphNode_BlendSpaceEvaluator::GetNodeTitleForBlendSpace(ENodeTitleType::Type TitleType, UBlendSpaceBase* InBlendSpace) const
{
	const FText BlendSpaceName = FText::FromString(InBlendSpace->GetName());

	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("BlendSpaceName"), BlendSpaceName);

		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("BlendSpaceEvaluatorListTitle", "Blendspace Evaluator '{BlendSpaceName}'"), Args), this);
	}
	else
	{
		FFormatNamedArguments TitleArgs;
		TitleArgs.Add(TEXT("BlendSpaceName"), BlendSpaceName);
		FText Title = FText::Format(LOCTEXT("BlendSpaceEvaluatorFullTitle", "{BlendSpaceName}\nBlendspace Evaluator"), TitleArgs);

		if ((TitleType == ENodeTitleType::FullTitle) && (SyncGroup.GroupName != NAME_None))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Title"), Title);
			Args.Add(TEXT("SyncGroupName"), FText::FromName(SyncGroup.GroupName));
			Title = FText::Format(LOCTEXT("BlendSpaceNodeGroupSubtitle", "{Title}\nSync group {SyncGroupName}"), Args);
		}
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitles.SetCachedTitle(TitleType, Title, this);
	}

	return CachedNodeTitles[TitleType];
}

FText UAnimGraphNode_BlendSpaceEvaluator::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Node.BlendSpace == nullptr)
	{
		// we may have a valid variable connected or default pin value
		UEdGraphPin* BlendSpacePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_BlendSpacePlayer, BlendSpace));
		if (BlendSpacePin && BlendSpacePin->LinkedTo.Num() > 0)
		{
			return LOCTEXT("BlendSpaceEvaluator_Variable_Title", "Blendspace Evaluator");
		}
		else if (BlendSpacePin && BlendSpacePin->DefaultObject != nullptr)
		{
			return GetNodeTitleForBlendSpace(TitleType, CastChecked<UBlendSpaceBase>(BlendSpacePin->DefaultObject));
		}
		else
		{
			if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
			{
				return LOCTEXT("BlendSpaceEvaluator_NONE_ListTitle", "Blendspace Evaluator '(None)'");
			}
			else
			{
				return LOCTEXT("BlendSpaceEvaluator_NONE_Title", "(None)\nBlendspace Evaluator");
			}
		}
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		return GetNodeTitleForBlendSpace(TitleType, Node.BlendSpace);
	}
}

void UAnimGraphNode_BlendSpaceEvaluator::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// Intentionally empty so that we don't get duplicate blend space entries.
	// You can convert a regular blend space player to an evaluator via the right click context menu
}

void UAnimGraphNode_BlendSpaceEvaluator::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	UBlendSpaceBase* BlendSpaceToCheck = Node.BlendSpace;
	UEdGraphPin* BlendSpacePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_BlendSpaceEvaluator, BlendSpace));
	if (BlendSpacePin != nullptr && BlendSpaceToCheck == nullptr)
	{
		BlendSpaceToCheck = Cast<UBlendSpaceBase>(BlendSpacePin->DefaultObject);
	}

	if (BlendSpaceToCheck == nullptr)
	{
		// we may have a connected node
		if (BlendSpacePin == nullptr || BlendSpacePin->LinkedTo.Num() == 0)
		{
			MessageLog.Error(TEXT("@@ references an unknown blend space"), this);
		}
	}
	else 
	{
		USkeleton* BlendSpaceSkeleton = BlendSpaceToCheck->GetSkeleton();
		if (BlendSpaceSkeleton&& // if blend space doesn't have skeleton, it might be due to blend space not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!BlendSpaceSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references blendspace that uses different skeleton @@"), this, BlendSpaceSkeleton);
		}
	}
}

void UAnimGraphNode_BlendSpaceEvaluator::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	Node.GroupIndex = AnimBlueprint->FindOrAddGroup(SyncGroup.GroupName);
	Node.GroupRole = SyncGroup.GroupRole;
}

void UAnimGraphNode_BlendSpaceEvaluator::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		// add an option to convert to single frame
		Context.MenuBuilder->BeginSection("AnimGraphNodeBlendSpacePlayer", NSLOCTEXT("A3Nodes", "BlendSpaceHeading", "Blend Space"));
		{
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ConvertToBSPlayer);
		}
		Context.MenuBuilder->EndSection();
	}
}

void UAnimGraphNode_BlendSpaceEvaluator::SetAnimationAsset(UAnimationAsset* Asset)
{
	if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Asset))
	{
		Node.BlendSpace = BlendSpace;
	}
}

bool UAnimGraphNode_BlendSpaceEvaluator::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_BlendSpaceEvaluator::GetAnimationAsset() const 
{
	UBlendSpaceBase* BlendSpace = Node.BlendSpace;
	UEdGraphPin* BlendSpacePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_BlendSpaceEvaluator, BlendSpace));
	if (BlendSpacePin != nullptr && BlendSpace == nullptr)
	{
		BlendSpace = Cast<UBlendSpaceBase>(BlendSpacePin->DefaultObject);
	}

	return BlendSpace;
}

const TCHAR* UAnimGraphNode_BlendSpaceEvaluator::GetTimePropertyName() const 
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_BlendSpaceEvaluator::GetTimePropertyStruct() const 
{
	return FAnimNode_BlendSpaceEvaluator::StaticStruct();
}

EAnimAssetHandlerType UAnimGraphNode_BlendSpaceEvaluator::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UBlendSpaceBase::StaticClass()) && !IsAimOffsetBlendSpace(AssetClass))
	{
		return EAnimAssetHandlerType::Supported;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

#undef LOCTEXT_NAMESPACE
