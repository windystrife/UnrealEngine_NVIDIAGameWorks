// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_AimOffsetLookAt.h"
#include "GraphEditorActions.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "Animation/AnimationSettings.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_RotationOffsetBlendSpace

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_AimOffsetLookAt::UAnimGraphNode_AimOffsetLookAt(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_AimOffsetLookAt::GetTooltipText() const
{
	// FText::Format() is slow, so we utilize the cached list title
	return GetNodeTitle(ENodeTitleType::ListView);
}

FText UAnimGraphNode_AimOffsetLookAt::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UBlendSpaceBase* BlendSpaceToCheck = Node.BlendSpace;
	UEdGraphPin* BlendSpacePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_AimOffsetLookAt, BlendSpace));
	if (BlendSpacePin != nullptr && BlendSpaceToCheck == nullptr)
	{
		BlendSpaceToCheck = Cast<UBlendSpaceBase>(BlendSpacePin->DefaultObject);
	}

	if (BlendSpaceToCheck == nullptr)
	{
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			return LOCTEXT("AimOffsetLookAt_NONE_ListTitle", "LookAt AimOffset '(None)'");
		}
		else
		{
			return LOCTEXT("AimOffsetLookAt_NONE_Title", "(None)\nLookAt AimOffset");
		}
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		const FText BlendSpaceName = FText::FromString(BlendSpaceToCheck->GetName());

		FFormatNamedArguments Args;
		Args.Add(TEXT("BlendSpaceName"), BlendSpaceName);

		// FText::Format() is slow, so we cache this to save on performance
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AimOffsetLookAtListTitle", "LookAt AimOffset '{BlendSpaceName}'"), Args), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AimOffsetLookAtFullTitle", "{BlendSpaceName}\nLookAt AimOffset"), Args), this);
		}
	}
	return CachedNodeTitles[TitleType];
}

void UAnimGraphNode_AimOffsetLookAt::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	struct GetMenuActions_Utils
	{
		static void SetNodeBlendSpace(UEdGraphNode* NewNode, bool /*bIsTemplateNode*/, TWeakObjectPtr<UBlendSpaceBase> BlendSpace)
		{
			UAnimGraphNode_AimOffsetLookAt* BlendSpaceNode = CastChecked<UAnimGraphNode_AimOffsetLookAt>(NewNode);
			BlendSpaceNode->Node.BlendSpace = BlendSpace.Get();
		}

		static UBlueprintNodeSpawner* MakeBlendSpaceAction(TSubclassOf<UEdGraphNode> const NodeClass, const UBlendSpaceBase* BlendSpace)
		{
			UBlueprintNodeSpawner* NodeSpawner = nullptr;

			bool const bIsAimOffset = BlendSpace->IsA(UAimOffsetBlendSpace::StaticClass()) ||
				BlendSpace->IsA(UAimOffsetBlendSpace1D::StaticClass());
			if (bIsAimOffset)
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

FBlueprintNodeSignature UAnimGraphNode_AimOffsetLookAt::GetSignature() const
{
	FBlueprintNodeSignature NodeSignature = Super::GetSignature();
	NodeSignature.AddSubObject(Node.BlendSpace);

	return NodeSignature;
}

void UAnimGraphNode_AimOffsetLookAt::SetAnimationAsset(UAnimationAsset* Asset)
{
	if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Asset))
	{
		Node.BlendSpace = BlendSpace;
	}
}

void UAnimGraphNode_AimOffsetLookAt::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	UBlendSpaceBase* BlendSpaceToCheck = Node.BlendSpace;
	UEdGraphPin* BlendSpacePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_AimOffsetLookAt, BlendSpace));
	if (BlendSpacePin != nullptr && BlendSpaceToCheck == nullptr)
	{
		BlendSpaceToCheck = Cast<UBlendSpaceBase>(BlendSpacePin->DefaultObject);
	}

	if (!BlendSpaceToCheck)
	{
		// we may have a connected node
		if (BlendSpacePin == nullptr || BlendSpacePin->LinkedTo.Num() == 0)
		{
			MessageLog.Error(TEXT("@@ references an unknown blend space"), this);
		}
	}
	else if (Cast<UAimOffsetBlendSpace>(BlendSpaceToCheck) == nullptr &&
		Cast<UAimOffsetBlendSpace1D>(BlendSpaceToCheck) == nullptr)
	{
		MessageLog.Error(TEXT("@@ references an invalid blend space (one that is not an aim offset)"), this);
	}
	else
	{
		const USkeleton* BlendSpaceSkeleton = BlendSpaceToCheck->GetSkeleton();
		if (BlendSpaceSkeleton && // if blend space doesn't have skeleton, it might be due to blend space not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!BlendSpaceSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references blendspace that uses different skeleton @@"), this, BlendSpaceSkeleton);
		}

		// Make sure that the source socket name is a valid one for the skeleton
		UEdGraphPin* SocketNamePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_AimOffsetLookAt, SourceSocketName));
		FName SocketNameToCheck = (SocketNamePin != nullptr) ? FName(*SocketNamePin->DefaultValue) : Node.SourceSocketName;

		// Temporary fix where skeleton is not fully loaded during AnimBP compilation and thus the socket name check is invalid UE-39499 (NEED FIX) 
		if (BlendSpaceSkeleton && !BlendSpaceSkeleton->HasAnyFlags(RF_NeedPostLoad))
		{
			const bool bValidValue = SocketNamePin == nullptr && BlendSpaceSkeleton->FindSocket(Node.SourceSocketName);
			const bool bValidPinValue = SocketNamePin != nullptr && BlendSpaceSkeleton->FindSocket(FName(*SocketNamePin->DefaultValue));
			const bool bValidConnectedPin = SocketNamePin != nullptr && SocketNamePin->LinkedTo.Num();

			if (!bValidValue && !bValidPinValue && !bValidConnectedPin)
			{ 
				FFormatNamedArguments Args;
				Args.Add(TEXT("SocketName"), FText::FromName(SocketNameToCheck));

				const FText Msg = FText::Format(LOCTEXT("SocketNameNotFound", "@@ - Socket {SocketName} not found in Skeleton"), Args);
				MessageLog.Error(*Msg.ToString(), this);
			}
		}
	}

	if (UAnimationSettings::Get()->bEnablePerformanceLog)
	{
		if (Node.LODThreshold < 0)
		{
			MessageLog.Warning(TEXT("@@ contains no LOD Threshold."), this);
		}
	}

	if(FMath::IsNearlyZero(Node.SocketAxis.SizeSquared()))
	{
		MessageLog.Error(TEXT("Socket axis for node @@ is zero."), this);
	}
}

void UAnimGraphNode_AimOffsetLookAt::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		// add an option to convert to single frame
		Context.MenuBuilder->BeginSection("AnimGraphNodeBlendSpacePlayer", NSLOCTEXT("A3Nodes", "BlendSpaceHeading", "Blend Space"));
		{
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ConvertToAimOffsetSimple);
		}
		Context.MenuBuilder->EndSection();
	}
}

void UAnimGraphNode_AimOffsetLookAt::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if (Node.BlendSpace)
	{
		HandleAnimReferenceCollection(Node.BlendSpace, AnimationAssets);
	}
}

void UAnimGraphNode_AimOffsetLookAt::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	HandleAnimReferenceReplacement(Node.BlendSpace, AnimAssetReplacementMap);
}

void UAnimGraphNode_AimOffsetLookAt::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	// Hide input pins that are not relevant for this child class.
	UBlendSpaceBase * BlendSpace = GetBlendSpace();
	if (BlendSpace != NULL)
	{
		if ((SourcePropertyName == TEXT("X")) || (SourcePropertyName == FName(*BlendSpace->GetBlendParameter(0).DisplayName)))
		{
			Pin->bHidden = true;
		}
		if ((SourcePropertyName == TEXT("Y")) || (SourcePropertyName == FName(*BlendSpace->GetBlendParameter(1).DisplayName)))
		{
			Pin->bHidden = true;
		}
		if ((SourcePropertyName == TEXT("Z")) || (SourcePropertyName == FName(*BlendSpace->GetBlendParameter(2).DisplayName)))
		{
			Pin->bHidden = true;
		}
	}
}

#undef LOCTEXT_NAMESPACE
