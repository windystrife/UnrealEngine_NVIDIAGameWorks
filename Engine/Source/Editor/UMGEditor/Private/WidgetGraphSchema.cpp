// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WidgetGraphSchema.h"

UWidgetGraphSchema::UWidgetGraphSchema(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UWidgetGraphSchema::BackwardCompatibilityNodeConversion(UEdGraph* Graph, bool bOnlySafeChanges) const
{
	Super::BackwardCompatibilityNodeConversion(Graph, bOnlySafeChanges);

	//if ( Graph )
	//{
	//	{
	//		static const FString BlueprintPinName(TEXT("Blueprint"));
	//		static const FString ClassPinName(TEXT("Class"));
	//		TArray<UK2Node_SpawnActor*> SpawnActorNodes;
	//		Graph->GetNodesOfClass(SpawnActorNodes);
	//		for ( auto It = SpawnActorNodes.CreateIterator(); It; ++It )
	//		{
	//			FBackwardCompatibilityConversionHelper::ConvertNode(
	//				*It, BlueprintPinName, NewObject<UK2Node_SpawnActorFromClass>(Graph),
	//				ClassPinName, *this, bOnlySafeChanges);
	//		}
	//	}

	//	{
	//		auto Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	//		if ( Blueprint && *Blueprint->SkeletonGeneratedClass )
	//		{
	//			TArray<UK2Node_CallFunction*> Nodes;
	//			Graph->GetNodesOfClass(Nodes);
	//			for ( const auto& FunctionRedirect : EditoronlyBPFunctionRedirects )
	//			{
	//				FBackwardCompatibilityConversionHelper::ConvertFunctionCallNodes(
	//					FBackwardCompatibilityConversionHelper::FFunctionCallParams(FunctionRedirect),
	//					Nodes, Graph, *this, bOnlySafeChanges);
	//			}
	//		}
	//		else
	//		{
	//			UE_LOG(LogBlueprint, Log, TEXT("BackwardCompatibilityNodeConversion: Blueprint '%s' cannot be fully converted. It has no skeleton class!"),
	//				Blueprint ? *Blueprint->GetName() : TEXT("Unknown"));
	//		}
	//	}

	//	// Call per-node deprecation functions
	//	TArray<UK2Node*> PossiblyDeprecatedNodes;
	//	Graph->GetNodesOfClass<UK2Node>(PossiblyDeprecatedNodes);

	//	for ( UK2Node* Node : PossiblyDeprecatedNodes )
	//	{
	//		Node->ConvertDeprecatedNode(Graph, bOnlySafeChanges);
	//	}
	//}
}
