// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_BlendSpacePlayer.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditorActions.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_BlendSpacePlayer

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_BlendSpacePlayer::UAnimGraphNode_BlendSpacePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_BlendSpacePlayer::GetTooltipText() const
{
	// FText::Format() is slow, so we utilize the cached list title
	return GetNodeTitle(ENodeTitleType::ListView);
}

FText UAnimGraphNode_BlendSpacePlayer::GetNodeTitleForBlendSpace(ENodeTitleType::Type TitleType, UBlendSpaceBase* InBlendSpace) const
{
	const FText BlendSpaceName = FText::FromString(InBlendSpace->GetName());

	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("BlendSpaceName"), BlendSpaceName);
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("BlendspacePlayer", "Blendspace Player '{BlendSpaceName}'"), Args), this);
	}
	else
	{
		FFormatNamedArguments TitleArgs;
		TitleArgs.Add(TEXT("BlendSpaceName"), BlendSpaceName);
		FText Title = FText::Format(LOCTEXT("BlendSpacePlayerFullTitle", "{BlendSpaceName}\nBlendspace Player"), TitleArgs);

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

FText UAnimGraphNode_BlendSpacePlayer::GetNodeTitle(ENodeTitleType::Type TitleType) const
{	
	if (Node.BlendSpace == nullptr)
	{
		// we may have a valid variable connected or default pin value
		UEdGraphPin* BlendSpacePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_BlendSpacePlayer, BlendSpace));
		if (BlendSpacePin && BlendSpacePin->LinkedTo.Num() > 0)
		{
			return LOCTEXT("BlendspacePlayer_Variable_Title", "Blendspace Player");
		}
		else if (BlendSpacePin && BlendSpacePin->DefaultObject != nullptr)
		{
			return GetNodeTitleForBlendSpace(TitleType, CastChecked<UBlendSpaceBase>(BlendSpacePin->DefaultObject));
		}
		else
		{
			if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
			{
				return LOCTEXT("BlendspacePlayer_NONE_ListTitle", "Blendspace Player '(None)'");
			}
			else
			{
				return LOCTEXT("BlendspacePlayer_NONE_Title", "(None)\nBlendspace Player");
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

void UAnimGraphNode_BlendSpacePlayer::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	UBlendSpaceBase* BlendSpaceToCheck = Node.BlendSpace;
	UEdGraphPin* BlendSpacePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_BlendSpacePlayer, BlendSpace));
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
		if (BlendSpaceSkeleton && // if blend space doesn't have skeleton, it might be due to blend space not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!BlendSpaceSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references blendspace that uses different skeleton @@"), this, BlendSpaceSkeleton);
		}
	}
}

void UAnimGraphNode_BlendSpacePlayer::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	Node.GroupIndex = AnimBlueprint->FindOrAddGroup(SyncGroup.GroupName);
	Node.GroupRole = SyncGroup.GroupRole;
}

void UAnimGraphNode_BlendSpacePlayer::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		// add an option to convert to single frame
		Context.MenuBuilder->BeginSection("AnimGraphNodeBlendSpaceEvaluator", NSLOCTEXT("A3Nodes", "BlendSpaceHeading", "Blend Space"));
		{
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ConvertToBSEvaluator);
		}
		Context.MenuBuilder->EndSection();
	}
}

void UAnimGraphNode_BlendSpacePlayer::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	struct GetMenuActions_Utils
	{
		static void SetNodeBlendSpace(UEdGraphNode* NewNode, bool /*bIsTemplateNode*/, TWeakObjectPtr<UBlendSpaceBase> BlendSpace)
		{
			UAnimGraphNode_BlendSpacePlayer* BlendSpaceNode = CastChecked<UAnimGraphNode_BlendSpacePlayer>(NewNode);
			BlendSpaceNode->Node.BlendSpace = BlendSpace.Get();
		}

		static UBlueprintNodeSpawner* MakeBlendSpaceAction(TSubclassOf<UEdGraphNode> const NodeClass, const UBlendSpaceBase* BlendSpace)
		{
			UBlueprintNodeSpawner* NodeSpawner = nullptr;

			bool const bIsAimOffset = BlendSpace->IsA(UAimOffsetBlendSpace::StaticClass()) ||
				BlendSpace->IsA(UAimOffsetBlendSpace1D::StaticClass());
			if (!bIsAimOffset)
			{
				NodeSpawner = UBlueprintNodeSpawner::Create(NodeClass);
				check(NodeSpawner != nullptr);

				TWeakObjectPtr<UBlendSpaceBase> BlendSpacePtr = BlendSpace;
				NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(GetMenuActions_Utils::SetNodeBlendSpace, BlendSpacePtr);
			}	
			return NodeSpawner;
		}
	};

	if (const UObject* RegistrarTarget = ActionRegistrar.GetActionKeyFilter())
	{
		if (const UBlendSpaceBase* TargetBlendSpace = Cast<UBlendSpaceBase>(RegistrarTarget))
		{
			if (UBlueprintNodeSpawner* NodeSpawner = GetMenuActions_Utils::MakeBlendSpaceAction(GetClass(), TargetBlendSpace))
			{
				ActionRegistrar.AddBlueprintAction(TargetBlendSpace, NodeSpawner);
			}
		}
		// else, the Blueprint database is specifically looking for actions pertaining to something different (not a BlendSpace asset)
	}
	else
	{
		UClass* NodeClass = GetClass();
		for (TObjectIterator<UBlendSpaceBase> BlendSpaceIt; BlendSpaceIt; ++BlendSpaceIt)
		{
			UBlendSpaceBase* BlendSpace = *BlendSpaceIt;
			if (UBlueprintNodeSpawner* NodeSpawner = GetMenuActions_Utils::MakeBlendSpaceAction(NodeClass, BlendSpace))
			{
				ActionRegistrar.AddBlueprintAction(BlendSpace, NodeSpawner);
			}
		}
	}
}

FBlueprintNodeSignature UAnimGraphNode_BlendSpacePlayer::GetSignature() const
{
	FBlueprintNodeSignature NodeSignature = Super::GetSignature();
	NodeSignature.AddSubObject(Node.BlendSpace);

	return NodeSignature;
}

void UAnimGraphNode_BlendSpacePlayer::SetAnimationAsset(UAnimationAsset* Asset)
{
	if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Asset))
	{
		Node.BlendSpace = BlendSpace;
	}
}

void UAnimGraphNode_BlendSpacePlayer::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if(Node.BlendSpace)
	{
		HandleAnimReferenceCollection(Node.BlendSpace, AnimationAssets);
	}
}

void UAnimGraphNode_BlendSpacePlayer::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	HandleAnimReferenceReplacement(Node.BlendSpace, AnimAssetReplacementMap);
}

bool UAnimGraphNode_BlendSpacePlayer::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_BlendSpacePlayer::GetAnimationAsset() const 
{
	UBlendSpaceBase* BlendSpace = Node.BlendSpace;
	UEdGraphPin* BlendSpacePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_BlendSpacePlayer, BlendSpace));
	if (BlendSpacePin != nullptr && BlendSpace == nullptr)
	{
		BlendSpace = Cast<UBlendSpaceBase>(BlendSpacePin->DefaultObject);
	}

	return BlendSpace;
}

const TCHAR* UAnimGraphNode_BlendSpacePlayer::GetTimePropertyName() const 
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_BlendSpacePlayer::GetTimePropertyStruct() const 
{
	return FAnimNode_BlendSpacePlayer::StaticStruct();
}

EAnimAssetHandlerType UAnimGraphNode_BlendSpacePlayer::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UBlendSpaceBase::StaticClass()) && !IsAimOffsetBlendSpace(AssetClass))
	{
		return EAnimAssetHandlerType::PrimaryHandler;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

#undef LOCTEXT_NAMESPACE

