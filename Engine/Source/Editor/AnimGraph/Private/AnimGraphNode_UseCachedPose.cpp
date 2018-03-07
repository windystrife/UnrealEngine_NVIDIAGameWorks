// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_UseCachedPose

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_UseCachedPose::UAnimGraphNode_UseCachedPose(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimGraphNode_UseCachedPose::EarlyValidation(class FCompilerResultsLog& MessageLog) const
{
	Super::EarlyValidation(MessageLog);
	
	bool bRefreshSavCachedPoseNode = true;

	// Check to see the current cached node is still valid (and not deleted, by checking pin connections)
	if(SaveCachedPoseNode.IsValid())
	{
		// The node has a single pin, make sure it's there
		check(SaveCachedPoseNode->Pins.Num());

		// Deleted nodes have no links, otherwise we will be doing some wasted work on unlinked nodes
		if(SaveCachedPoseNode->Pins[0]->LinkedTo.Num())
		{
			// The node has links, it's valid, continue to use it
			bRefreshSavCachedPoseNode = false;
		}
	}

	// We need to refresh the cached pose node this node is linked to
	if(bRefreshSavCachedPoseNode && !NameOfCache.IsEmpty())
	{
		UBlueprint* GraphBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(GetGraph());
		check(GraphBlueprint);

		TArray<UEdGraph*> AllGraphs;
		GraphBlueprint->GetAllGraphs(AllGraphs);

		for(UEdGraph* Graph : AllGraphs)
		{
			// Get a list of all save cached pose nodes
			TArray<UAnimGraphNode_SaveCachedPose*> CachedPoseNodes;
			Graph->GetNodesOfClass(CachedPoseNodes);

			// Go through all the nodes and find one with a title that matches ours
			for (auto NodeIt = CachedPoseNodes.CreateIterator(); NodeIt; ++NodeIt)
			{
				if((*NodeIt)->CacheName == NameOfCache)
				{
					// Fix the original Blueprint node as well as the compiled version
					MessageLog.FindSourceObjectTypeChecked<UAnimGraphNode_UseCachedPose>(this)->SaveCachedPoseNode = *NodeIt;
					SaveCachedPoseNode = *NodeIt;
					break;
				}
			}
		}
	}
}

FText UAnimGraphNode_UseCachedPose::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_UseCachedPose_Tooltip", "References an animation tree elsewhere in the blueprint, which will be evaluated at most once per frame.");
}

FText UAnimGraphNode_UseCachedPose::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;
	if(SaveCachedPoseNode.IsValid())
	{
		NameOfCache = SaveCachedPoseNode->CacheName;
	}
	Args.Add(TEXT("CachePoseName"), FText::FromString(NameOfCache));
	return FText::Format(LOCTEXT("AnimGraphNode_UseCachedPose_Title", "Use cached pose '{CachePoseName}'"), Args);
}

FString UAnimGraphNode_UseCachedPose::GetNodeCategory() const
{
	return TEXT("Cached Poses");
}

void UAnimGraphNode_UseCachedPose::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto PostSpawnSetupLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FString CacheNodeName, UAnimGraphNode_SaveCachedPose* SaveCachePoseNode)
	{
		UAnimGraphNode_UseCachedPose* UseCachedPose = CastChecked<UAnimGraphNode_UseCachedPose>(NewNode);
		// we use an empty CacheName in GetNodeTitle() to relay the proper menu title
		UseCachedPose->SaveCachedPoseNode = SaveCachePoseNode;
	};


	UObject const* ActionKey = ActionRegistrar.GetActionKeyFilter();

	if(UBlueprint const* Blueprint = Cast<UBlueprint>(ActionKey))
	{
		// Get a list of all save cached pose nodes
		TArray<UAnimGraphNode_SaveCachedPose*> CachedPoseNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UAnimGraphNode_SaveCachedPose>(Blueprint, /*out*/ CachedPoseNodes);

		// Offer a use node for each of them
		for (auto NodeIt = CachedPoseNodes.CreateIterator(); NodeIt; ++NodeIt)
		{
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(PostSpawnSetupLambda, (*NodeIt)->CacheName, *NodeIt);

			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
		}
	}
}

bool UAnimGraphNode_UseCachedPose::IsActionFilteredOut(class FBlueprintActionFilter const& Filter)
{
	bool bIsFilteredOut = false;
	if(SaveCachedPoseNode.IsValid())
	{
		FBlueprintActionContext const& FilterContext = Filter.Context;
		for(UBlueprint* Blueprint : FilterContext.Blueprints)
		{
			if(SaveCachedPoseNode->GetBlueprint() != Blueprint)
			{
				bIsFilteredOut = true;
				break;
			}
		}
	}
	return bIsFilteredOut;
}

#undef LOCTEXT_NAMESPACE
