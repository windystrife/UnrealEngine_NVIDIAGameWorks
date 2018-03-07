// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_PoseBlendNode.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "GraphEditorActions.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "BlueprintActionFilter.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"

#define LOCTEXT_NAMESPACE "PoseBlendNode"

// Action to add a pose asset blend node to the graph
struct FNewPoseBlendNodeAction : public FEdGraphSchemaAction_K2NewNode
{
protected:
	FAssetData AssetInfo;
public:
	FNewPoseBlendNodeAction(const FAssetData& InAssetInfo, FText Title)
		: FEdGraphSchemaAction_K2NewNode(LOCTEXT("PoseAsset", "PoseAssets"), Title, LOCTEXT("EvalCurvesToMakePose", "Evaluates curves to produce a pose from pose asset"), 0, FText::FromName(InAssetInfo.ObjectPath))
	{
		AssetInfo = InAssetInfo;

		UAnimGraphNode_PoseBlendNode* Template = NewObject<UAnimGraphNode_PoseBlendNode>();
		NodeTemplate = Template;
	}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override
	{
		UAnimGraphNode_PoseBlendNode* SpawnedNode = CastChecked<UAnimGraphNode_PoseBlendNode>(FEdGraphSchemaAction_K2NewNode::PerformAction(ParentGraph, FromPin, Location, bSelectNewNode));
		SpawnedNode->Node.PoseAsset = Cast<UPoseAsset>(AssetInfo.GetAsset());

		return SpawnedNode;
	}
};
/////////////////////////////////////////////////////
// UAnimGraphNode_PoseBlendNode

UAnimGraphNode_PoseBlendNode::UAnimGraphNode_PoseBlendNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimGraphNode_PoseBlendNode::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if(Node.PoseAsset)
	{
		HandleAnimReferenceCollection(Node.PoseAsset, AnimationAssets);
	}
}

void UAnimGraphNode_PoseBlendNode::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	HandleAnimReferenceReplacement(Node.PoseAsset, AnimAssetReplacementMap);
}

FText UAnimGraphNode_PoseBlendNode::GetTooltipText() const
{
	// FText::Format() is slow, so we utilize the cached list title
	return GetNodeTitle(ENodeTitleType::ListView);
}

FText UAnimGraphNode_PoseBlendNode::GetNodeTitleForPoseAsset(ENodeTitleType::Type TitleType, UPoseAsset* InPoseAsset) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("PoseAssetName"), FText::FromString(InPoseAsset->GetName()));

	return FText::Format(LOCTEXT("PoseByName_Title", "{PoseAssetName}"), Args);
}

FText UAnimGraphNode_PoseBlendNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Node.PoseAsset == nullptr)
	{
		// we may have a valid variable connected or default pin value
		UEdGraphPin* PosePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseBlendNode, PoseAsset));
		if (PosePin && PosePin->LinkedTo.Num() > 0)
		{
			return LOCTEXT("PoseByName_TitleVariable", "Pose");
		}
		else if (PosePin && PosePin->DefaultObject != nullptr)
		{
			return GetNodeTitleForPoseAsset(TitleType, CastChecked<UPoseAsset>(PosePin->DefaultObject));
		}
		else
		{
			return LOCTEXT("PoseByName_TitleNONE", "Pose (None)");
		}
	}
	else
	{
		return GetNodeTitleForPoseAsset(TitleType, Node.PoseAsset);
	}
}

void UAnimGraphNode_PoseBlendNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto LoadedAssetSetup = [](UEdGraphNode* NewNode, bool /*bIsTemplateNode*/, TWeakObjectPtr<UPoseAsset> PoseAssetPtr)
	{
		UAnimGraphNode_PoseBlendNode* PoseBlendNodeNode = CastChecked<UAnimGraphNode_PoseBlendNode>(NewNode);
		PoseBlendNodeNode->Node.PoseAsset = PoseAssetPtr.Get();
	};

	auto UnloadedAssetSetup = [](UEdGraphNode* NewNode, bool bIsTemplateNode, const FAssetData AssetData)
	{
		UAnimGraphNode_PoseBlendNode* PoseBlendNodeNode = CastChecked<UAnimGraphNode_PoseBlendNode>(NewNode);
		if (bIsTemplateNode)
		{
			AssetData.GetTagValue("Skeleton", PoseBlendNodeNode->UnloadedSkeletonName);
		}
		else
		{
			UPoseAsset* PoseAsset = Cast<UPoseAsset>(AssetData.GetAsset());
			check(PoseAsset != nullptr);
			PoseBlendNodeNode->Node.PoseAsset = PoseAsset;
		}
	};

	const UObject* QueryObject = ActionRegistrar.GetActionKeyFilter();
	if (QueryObject == nullptr)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		// define a filter to help in pulling UPoseAsset asset data from the registry
		FARFilter Filter;
		Filter.ClassNames.Add(UPoseAsset::StaticClass()->GetFName());
		Filter.bRecursiveClasses = true;
		// Find matching assets and add an entry for each one
		TArray<FAssetData> PoseAssetList;
		AssetRegistryModule.Get().GetAssets(Filter, /*out*/PoseAssetList);

		for (auto AssetIt = PoseAssetList.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset = *AssetIt;

			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			if (Asset.IsAssetLoaded())
			{
				TWeakObjectPtr<UPoseAsset> PoseAsset = Cast<UPoseAsset>(Asset.GetAsset());
				NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(LoadedAssetSetup, PoseAsset);
				NodeSpawner->DefaultMenuSignature.MenuName = GetTitleGivenAssetInfo(FText::FromName(PoseAsset->GetFName()));
			}
			else
			{
				NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(UnloadedAssetSetup, Asset);
				NodeSpawner->DefaultMenuSignature.MenuName = GetTitleGivenAssetInfo(FText::FromName(Asset.AssetName));
			}
			ActionRegistrar.AddBlueprintAction(Asset, NodeSpawner);
		}
	}
	else if (const UPoseAsset* PoseAsset = Cast<UPoseAsset>(QueryObject))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());

		TWeakObjectPtr<UPoseAsset> PoseAssetPtr = PoseAsset;
		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(LoadedAssetSetup, PoseAssetPtr);
		NodeSpawner->DefaultMenuSignature.MenuName = GetTitleGivenAssetInfo(FText::FromName(PoseAsset->GetFName()));

		ActionRegistrar.AddBlueprintAction(QueryObject, NodeSpawner);
	}
	else if (QueryObject == GetClass())
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		// define a filter to help in pulling UPoseAsset asset data from the registry
		FARFilter Filter;
		Filter.ClassNames.Add(UPoseAsset::StaticClass()->GetFName());
		Filter.bRecursiveClasses = true;
		// Find matching assets and add an entry for each one
		TArray<FAssetData> PoseAssetList;
		AssetRegistryModule.Get().GetAssets(Filter, /*out*/PoseAssetList);

		for (auto AssetIt = PoseAssetList.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset = *AssetIt;
			if (Asset.IsAssetLoaded())
			{
				continue;
			}

			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(UnloadedAssetSetup, Asset);
			NodeSpawner->DefaultMenuSignature.MenuName = GetTitleGivenAssetInfo(FText::FromName(Asset.AssetName));
			ActionRegistrar.AddBlueprintAction(Asset, NodeSpawner);
		}
	}
}

FText UAnimGraphNode_PoseBlendNode::GetTitleGivenAssetInfo(const FText& AssetName)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), AssetName);

	return FText::Format(LOCTEXT("PoseAssetNodeTitle", "Evaluate Pose {AssetName}"), Args);
}

bool UAnimGraphNode_PoseBlendNode::IsActionFilteredOut(class FBlueprintActionFilter const& Filter)
{
	bool bIsFilteredOut = false;
	FBlueprintActionContext const& FilterContext = Filter.Context;

	for (UBlueprint* Blueprint : FilterContext.Blueprints)
	{
		if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Blueprint))
		{
			if (Node.PoseAsset)
			{
				if (Node.PoseAsset->GetSkeleton() != AnimBlueprint->TargetSkeleton)
				{
					// PoseAsset does not use the same skeleton as the Blueprint, cannot use
					bIsFilteredOut = true;
					break;
				}
			}
			else
			{
				FAssetData SkeletonData(AnimBlueprint->TargetSkeleton);
				if (UnloadedSkeletonName != SkeletonData.GetExportTextName())
				{
					bIsFilteredOut = true;
					break;
				}
			}
		}
		else
		{
			// Not an animation Blueprint, cannot use
			bIsFilteredOut = true;
			break;
		}
	}
	return bIsFilteredOut;
}

FText UAnimGraphNode_PoseBlendNode::GetMenuCategory() const
{
	return LOCTEXT("PoseAssetCategory_Label", "Poses");
}

bool UAnimGraphNode_PoseBlendNode::DoesSupportTimeForTransitionGetter() const
{
	return false;
}

void UAnimGraphNode_PoseBlendNode::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		// add an option to convert to single frame
		Context.MenuBuilder->BeginSection("AnimGraphNodePoseBlender", LOCTEXT("PoseBlenderHeading", "Pose Blender"));
		{
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ConvertToPoseByName);
		}
		Context.MenuBuilder->EndSection();
	}
}

EAnimAssetHandlerType UAnimGraphNode_PoseBlendNode::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UPoseAsset::StaticClass()))
	{
		return EAnimAssetHandlerType::PrimaryHandler;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

#undef LOCTEXT_NAMESPACE
