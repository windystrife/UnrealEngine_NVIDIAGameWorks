// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "EnvironmentQueryGraph.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "AIGraphTypes.h"
#include "EdGraphSchema_EnvironmentQuery.h"
#include "AIGraphNode.h"
#include "EnvironmentQueryGraphNode.h"
#include "EnvironmentQueryGraphNode_Option.h"
#include "EnvironmentQueryGraphNode_Root.h"
#include "EnvironmentQueryGraphNode_Test.h"

//////////////////////////////////////////////////////////////////////////
// EnvironmentQueryGraph

namespace EQSGraphVersion
{
	const int32 Initial = 0;
	const int32 NestedNodes = 1;
	const int32 CopyPasteOutersBug = 2;
	const int32 BlueprintClasses = 3;

	const int32 Latest = BlueprintClasses;
}

UEnvironmentQueryGraph::UEnvironmentQueryGraph(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Schema = UEdGraphSchema_EnvironmentQuery::StaticClass();
}

struct FCompareNodeXLocation
{
	FORCEINLINE bool operator()(const UEdGraphPin& A, const UEdGraphPin& B) const
	{
		return A.GetOwningNode()->NodePosX < B.GetOwningNode()->NodePosX;
	}
};

void UEnvironmentQueryGraph::UpdateAsset(int32 UpdateFlags)
{
	if (IsLocked())
	{
		return;
	}

	// let's find root node
	UEnvironmentQueryGraphNode_Root* RootNode = NULL;
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		RootNode = Cast<UEnvironmentQueryGraphNode_Root>(Nodes[Idx]);
		if (RootNode != NULL)
		{
			break;
		}
	}

	UEnvQuery* Query = Cast<UEnvQuery>(GetOuter());
	Query->GetOptionsMutable().Reset();
	if (RootNode && RootNode->Pins.Num() > 0 && RootNode->Pins[0]->LinkedTo.Num() > 0)
	{
		UEdGraphPin* MyPin = RootNode->Pins[0];

		// sort connections so that they're organized the same as user can see in the editor
		MyPin->LinkedTo.Sort(FCompareNodeXLocation());

		for (int32 Idx = 0; Idx < MyPin->LinkedTo.Num(); Idx++)
		{
			UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(MyPin->LinkedTo[Idx]->GetOwningNode());
			if (OptionNode)
			{
				OptionNode->UpdateNodeData();

				UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(OptionNode->NodeInstance);
				if (OptionInstance && OptionInstance->Generator)
				{
					OptionInstance->Tests.Reset();

					for (int32 TestIdx = 0; TestIdx < OptionNode->SubNodes.Num(); TestIdx++)
					{
						UAIGraphNode* SubNode = OptionNode->SubNodes[TestIdx];
						if (SubNode == nullptr)
						{
							continue;
						}

						SubNode->ParentNode = OptionNode;

						UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(SubNode);
						if (TestNode && TestNode->bTestEnabled)
						{
							UEnvQueryTest* TestInstance = Cast<UEnvQueryTest>(TestNode->NodeInstance);
							if (TestInstance)
							{
								TestInstance->TestOrder = TestIdx;
								OptionInstance->Tests.Add(TestInstance);
							}
						}
					}

					Query->GetOptionsMutable().Add(OptionInstance);
				}
				
				// FORT-16508 tracking BEGIN: log invalid option
				if (OptionInstance && OptionInstance->Generator == nullptr)
				{
					FString DebugMessage = FString::Printf(TEXT("[%s] UpdateAsset found option instance [pin:%d] without a generator! tests:%d"),
						FPlatformTime::StrTimestamp(), Idx, OptionNode->SubNodes.Num());

					RootNode->LogDebugMessage(DebugMessage);
				}
				else if (OptionInstance == nullptr)
				{
					FString DebugMessage = FString::Printf(TEXT("[%s] UpdateAsset found option node [pin:%d] without an instance! tests:%d"),
						FPlatformTime::StrTimestamp(), Idx, OptionNode->SubNodes.Num());

					RootNode->LogDebugMessage(DebugMessage);
				}
				// FORT-16508 tracking END
			}
		}
	}

	RemoveOrphanedNodes();

	// FORT-16508 tracking BEGIN: find corrupted options
	if (RootNode)
	{
		for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
		{
			UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Nodes[Idx]);
			if (OptionNode)
			{
				UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(OptionNode->NodeInstance);
				if (OptionNode->NodeInstance == nullptr || OptionInstance == nullptr || OptionInstance->HasAnyFlags(RF_Transient))
				{
					FString DebugMessage = FString::Printf(TEXT("[%s] found corrupted node after RemoveOrphanedNodes! type:instance option:%s instance:%d transient:%d tests:%d"),
						FPlatformTime::StrTimestamp(),
						*GetNameSafe(OptionNode),
						OptionNode->NodeInstance ? (OptionInstance ? 1 : -1) : 0,
						OptionNode->NodeInstance ? (OptionNode->HasAnyFlags(RF_Transient) ? 1 : 0) : -1,
						OptionNode->SubNodes.Num());					

					RootNode->LogDebugError(DebugMessage);
				}

				if (OptionInstance && (OptionInstance->Generator == nullptr || OptionInstance->Generator->HasAnyFlags(RF_Transient)))
				{
					FString DebugMessage = FString::Printf(TEXT("[%s] found corrupted node after RemoveOrphanedNodes! type:generator option:%s instance:%d transient:%d tests:%d"),
						FPlatformTime::StrTimestamp(),
						*GetNameSafe(OptionNode),
						OptionNode->NodeInstance ? 1 : 0,
						OptionNode->NodeInstance ? (OptionNode->HasAnyFlags(RF_Transient) ? 1 : 0) : -1,
						OptionNode->SubNodes.Num());

					RootNode->LogDebugError(DebugMessage);
				}
			}
		}
	}
	// FORT-16508 tracking END

#if USE_EQS_DEBUGGER
	UEnvQueryManager::NotifyAssetUpdate(Query);
#endif
}

void UEnvironmentQueryGraph::Initialize()
{
	Super::Initialize();
	
	LockUpdates();
	SpawnMissingNodes();
	CalculateAllWeights();
	UnlockUpdates();
}

void UEnvironmentQueryGraph::OnLoaded()
{
	Super::OnLoaded();
	UpdateDeprecatedGeneratorClasses();
}

void UEnvironmentQueryGraph::CalculateAllWeights()
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Nodes[Idx]);
		if (OptionNode)
		{
			OptionNode->CalculateWeights();
		}
	}
}

void UEnvironmentQueryGraph::MarkVersion()
{
	GraphVersion = EQSGraphVersion::Latest;
}

void UEnvironmentQueryGraph::UpdateVersion()
{
	if (GraphVersion == EQSGraphVersion::Latest)
	{
		return;
	}

	// convert to nested nodes
	if (GraphVersion < EQSGraphVersion::NestedNodes)
	{
		UpdateVersion_NestedNodes();
	}

	if (GraphVersion < EQSGraphVersion::CopyPasteOutersBug)
	{
		UpdateVersion_FixupOuters();
	}

	if (GraphVersion < EQSGraphVersion::BlueprintClasses)
	{
		UpdateVersion_CollectClassData();
	}

	GraphVersion = EQSGraphVersion::Latest;
	Modify();
}

void UEnvironmentQueryGraph::UpdateVersion_NestedNodes()
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Nodes[Idx]);
		if (OptionNode)
		{
			UEnvironmentQueryGraphNode* Node = OptionNode;
			while (Node)
			{
				UEnvironmentQueryGraphNode* NextNode = NULL;
				for (int32 iPin = 0; iPin < Node->Pins.Num(); iPin++)
				{
					UEdGraphPin* TestPin = Node->Pins[iPin];
					if (TestPin && TestPin->Direction == EGPD_Output)
					{
						for (int32 iLink = 0; iLink < TestPin->LinkedTo.Num(); iLink++)
						{
							UEdGraphPin* LinkedTo = TestPin->LinkedTo[iLink];
							UEnvironmentQueryGraphNode_Test* LinkedTest = LinkedTo ? Cast<UEnvironmentQueryGraphNode_Test>(LinkedTo->GetOwningNode()) : NULL;
							if (LinkedTest)
							{
								LinkedTest->ParentNode = OptionNode;
								OptionNode->SubNodes.Add(LinkedTest);

								NextNode = LinkedTest;
								break;
							}
						}

						break;
					}
				}

				Node = NextNode;
			}
		}
	}

	for (int32 Idx = Nodes.Num() - 1; Idx >= 0; Idx--)
	{
		UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(Nodes[Idx]);
		if (TestNode)
		{
			TestNode->Pins.Empty();
			Nodes.RemoveAt(Idx);
			continue;
		}

		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Nodes[Idx]);
		if (OptionNode && OptionNode->Pins.IsValidIndex(1))
		{
			OptionNode->Pins[1]->MarkPendingKill();
			OptionNode->Pins.RemoveAt(1);
		}
	}
}

void UEnvironmentQueryGraph::UpdateVersion_FixupOuters()
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UEnvironmentQueryGraphNode* MyNode = Cast<UEnvironmentQueryGraphNode>(Nodes[Idx]);
		if (MyNode)
		{
			MyNode->PostEditImport();
		}
	}
}

void UEnvironmentQueryGraph::UpdateVersion_CollectClassData()
{
	UpdateClassData();
}

void UEnvironmentQueryGraph::CollectAllNodeInstances(TSet<UObject*>& NodeInstances)
{
	Super::CollectAllNodeInstances(NodeInstances);

	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UEnvironmentQueryGraphNode* MyNode = Cast<UEnvironmentQueryGraphNode>(Nodes[Idx]);
		UEnvQueryOption* OptionInstance = MyNode ? Cast<UEnvQueryOption>(MyNode->NodeInstance) : nullptr;
		if (OptionInstance && OptionInstance->Generator)
		{
			NodeInstances.Add(OptionInstance->Generator);
		}
	}
}

void UEnvironmentQueryGraph::OnNodeInstanceRemoved(UObject* NodeInstance)
{
	// FORT-16508 tracking BEGIN: log removing a node instance
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UEnvironmentQueryGraphNode_Root* RootNode = Cast<UEnvironmentQueryGraphNode_Root>(Nodes[Idx]);
		if (RootNode)
		{
			FString DebugMessage = FString::Printf(TEXT("[%s] RemoveInstance %s owner:%s wasTransient:%d"),
				FPlatformTime::StrTimestamp(),
				*GetNameSafe(NodeInstance),
				NodeInstance ? *GetNameSafe(NodeInstance->GetOuter()) : TEXT("??"),
				NodeInstance ? (NodeInstance->HasAnyFlags(RF_Transient) ? 1 : 0) : -1);

			RootNode->LogDebugMessage(DebugMessage);
		}
	}
	// FORT-16508 tracking END
}

void UEnvironmentQueryGraph::OnNodesPasted(const FString& ImportStr)
{
	// FORT-16508 tracking BEGIN: log removing a node instance
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UEnvironmentQueryGraphNode_Root* RootNode = Cast<UEnvironmentQueryGraphNode_Root>(Nodes[Idx]);
		if (RootNode)
		{
			FString DebugMessage = FString::Printf(TEXT("[%s] PasteNodes\n\n%s"),
				FPlatformTime::StrTimestamp(), *ImportStr);

			RootNode->LogDebugMessage(DebugMessage);
		}
	}
	// FORT-16508 tracking END
}

void UEnvironmentQueryGraph::UpdateDeprecatedGeneratorClasses()
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UEnvironmentQueryGraphNode* MyNode = Cast<UEnvironmentQueryGraphNode>(Nodes[Idx]);
		UEnvQueryOption* OptionInstance = MyNode ? Cast<UEnvQueryOption>(MyNode->NodeInstance) : nullptr;
		if (OptionInstance && OptionInstance->Generator)
		{
			MyNode->ErrorMessage = FGraphNodeClassHelper::GetDeprecationMessage(OptionInstance->Generator->GetClass());
		}
	}
}

void UEnvironmentQueryGraph::SpawnMissingNodes()
{
	UEnvQuery* QueryOwner = Cast<UEnvQuery>(GetOuter());
	if (QueryOwner == nullptr)
	{
		return;
	}

	TSet<UEnvQueryTest*> ExistingTests;
	TSet<UEnvQueryOption*> ExistingNodes;
	TArray<UEnvQueryOption*> OptionsCopy = QueryOwner->GetOptions();

	UAIGraphNode* MyRootNode = nullptr;
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UEnvironmentQueryGraphNode* MyNode = Cast<UEnvironmentQueryGraphNode>(Nodes[Idx]);
		UEnvQueryOption* OptionInstance = MyNode ? Cast<UEnvQueryOption>(MyNode->NodeInstance) : nullptr;
		if (OptionInstance && OptionInstance->Generator)
		{
			ExistingNodes.Add(OptionInstance);

			ExistingTests.Empty(ExistingTests.Num());
			for (int32 SubIdx = 0; SubIdx < MyNode->SubNodes.Num(); SubIdx++)
			{
				UEnvironmentQueryGraphNode* MySubNode = Cast<UEnvironmentQueryGraphNode>(MyNode->SubNodes[SubIdx]);
				UEnvQueryTest* TestInstance = MySubNode ? Cast<UEnvQueryTest>(MySubNode->NodeInstance) : nullptr;
				if (TestInstance)
				{
					ExistingTests.Add(TestInstance);
				}
				else
				{
					MyNode->RemoveSubNode(MySubNode);
					SubIdx--;
				}
			}

			SpawnMissingSubNodes(OptionInstance, ExistingTests, MyNode);
		}

		UEnvironmentQueryGraphNode_Root* RootNode = Cast<UEnvironmentQueryGraphNode_Root>(Nodes[Idx]);
		if (RootNode)
		{
			MyRootNode = RootNode;
		}
	}

	UEdGraphPin* RootOutPin = MyRootNode ? FindGraphNodePin(MyRootNode, EGPD_Output) : nullptr;
	ExistingTests.Empty(0);

	for (int32 Idx = 0; Idx < OptionsCopy.Num(); Idx++)
	{
		UEnvQueryOption* OptionInstance = OptionsCopy[Idx];
		if (ExistingNodes.Contains(OptionInstance) || OptionInstance == nullptr || OptionInstance->Generator == nullptr)
		{
			continue;
		}

		FGraphNodeCreator<UEnvironmentQueryGraphNode_Option> NodeBuilder(*this);
		UEnvironmentQueryGraphNode_Option* MyNode = NodeBuilder.CreateNode();
		UAIGraphNode::UpdateNodeClassDataFrom(OptionInstance->Generator->GetClass(), MyNode->ClassData);
		MyNode->ErrorMessage = MyNode->ClassData.GetDeprecatedMessage();
		NodeBuilder.Finalize();

		if (MyRootNode)
		{
			MyNode->NodePosX = MyRootNode->NodePosX + (Idx * 300);
			MyNode->NodePosY = MyRootNode->NodePosY + 100;
		}

		MyNode->NodeInstance = OptionInstance;
		SpawnMissingSubNodes(OptionInstance, ExistingTests, MyNode);

		UEdGraphPin* SpawnedInPin = FindGraphNodePin(MyNode, EGPD_Input);
		if (RootOutPin && SpawnedInPin)
		{
			RootOutPin->MakeLinkTo(SpawnedInPin);
		}
	}
}

void UEnvironmentQueryGraph::SpawnMissingSubNodes(UEnvQueryOption* Option, TSet<UEnvQueryTest*> ExistingTests, UEnvironmentQueryGraphNode* OptionNode)
{
	TArray<UEnvQueryTest*> TestsCopy = Option->Tests;
	for (int32 SubIdx = 0; SubIdx < TestsCopy.Num(); SubIdx++)
	{
		if (ExistingTests.Contains(TestsCopy[SubIdx]) || (TestsCopy[SubIdx] == nullptr))
		{
			continue;
		}

		UEnvironmentQueryGraphNode_Test* TestNode = NewObject<UEnvironmentQueryGraphNode_Test>(this);
		TestNode->NodeInstance = TestsCopy[SubIdx];
		TestNode->UpdateNodeClassData();

		OptionNode->AddSubNode(TestNode, this);
		TestNode->NodeInstance = TestsCopy[SubIdx];
	}
}

void UEnvironmentQueryGraph::ResetProfilerStats()
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Nodes[Idx]);
		if (OptionNode)
		{
			OptionNode->bStatShowOverlay = false;
			OptionNode->StatsPerGenerator.Reset();
			OptionNode->StatAvgPickRate = 0.0f;

			for (int32 TestIdx = 0; TestIdx < OptionNode->SubNodes.Num(); TestIdx++)
			{
				UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(OptionNode->SubNodes[TestIdx]);
				if (TestNode)
				{
					TestNode->bStatShowOverlay = false;
					TestNode->Stats = FEnvionmentQueryNodeStats();
				}
			}
		}
	}
}

#if USE_EQS_DEBUGGER

FEnvionmentQueryNodeStats GetOverlayStatsHelper(const FEQSDebugger::FStatsInfo& StatsInfo, int32 OptionIdx, int32 StepIdx)
{
	FEnvionmentQueryNodeStats OverlayInfo;
	OverlayInfo.AvgTime = 1000.0f * StatsInfo.TotalAvgData.OptionStats[OptionIdx].StepData[StepIdx].ExecutionTime / StatsInfo.TotalAvgCount;

	// make sure it exists in data from most expensive run
	if (StatsInfo.MostExpensive.OptionStats.IsValidIndex(OptionIdx) && StatsInfo.MostExpensive.OptionStats[OptionIdx].StepData.IsValidIndex(StepIdx))
	{
		OverlayInfo.MaxTime = StatsInfo.MostExpensive.OptionStats[OptionIdx].StepData[StepIdx].ExecutionTime * 1000.0f;
		OverlayInfo.MaxNumProcessedItems = StatsInfo.MostExpensive.OptionStats[OptionIdx].StepData[StepIdx].NumProcessedItems;
	}

	return OverlayInfo;
}

void UEnvironmentQueryGraph::StoreProfilerStats(const FEQSDebugger::FStatsInfo& Stats)
{
	TArray<float> OptionPickRate;
	for (int32 Idx = 0; Idx < Stats.TotalAvgData.OptionStats.Num(); Idx++)
	{
		OptionPickRate.Add(1.0f * Stats.TotalAvgData.OptionStats[Idx].NumRuns / Stats.TotalAvgCount);
	}

	// process connected option nodes, if asset no longer match recorder data, debug view will not be accurate!
	int32 AssetOptionIdx = 0;
	for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); NodeIdx++)
	{
		UEnvironmentQueryGraphNode_Root* RootNode = Cast<UEnvironmentQueryGraphNode_Root>(Nodes[NodeIdx]);
		if (RootNode)
		{
			for (int32 PinIdx = 0; PinIdx < RootNode->Pins.Num(); PinIdx++)
			{
				if (RootNode->Pins[PinIdx] && RootNode->Pins[PinIdx]->Direction == EEdGraphPinDirection::EGPD_Output)
				{
					for (int32 LinkedPinIdx = 0; LinkedPinIdx < RootNode->Pins[PinIdx]->LinkedTo.Num(); LinkedPinIdx++)
					{
						UEdGraphPin* LinkedPin = RootNode->Pins[PinIdx]->LinkedTo[LinkedPinIdx];
						UEnvironmentQueryGraphNode_Option* OptionNode = LinkedPin ? Cast<UEnvironmentQueryGraphNode_Option>(LinkedPin->GetOuter()) : nullptr;

						if (OptionNode)
						{
							int32 StatsOptionIdx = INDEX_NONE;
							for (int32 Idx = 0; Idx < Stats.TotalAvgData.OptionData.Num(); Idx++)
							{
								if (Stats.TotalAvgData.OptionData[Idx].OptionIdx == AssetOptionIdx)
								{
									StatsOptionIdx = Idx;
									break;
								}
							}

							if (StatsOptionIdx != INDEX_NONE)
							{
								// fill overlay values

								OptionNode->bStatShowOverlay = true;
								OptionNode->StatAvgPickRate = OptionPickRate.IsValidIndex(StatsOptionIdx) ? OptionPickRate[StatsOptionIdx] : 0.0f;
								OptionNode->StatsPerGenerator.Reset();

								if (Stats.TotalAvgData.OptionStats.IsValidIndex(StatsOptionIdx))
								{
									const int32 NumGenerators = Stats.TotalAvgData.OptionData[StatsOptionIdx].NumGenerators;
									for (int32 GenIdx = 0; GenIdx < NumGenerators; GenIdx++)
									{
										const FEnvionmentQueryNodeStats OverlayStatsInfo = GetOverlayStatsHelper(Stats, StatsOptionIdx, GenIdx);
										OptionNode->StatsPerGenerator.Add(OverlayStatsInfo);
									}

									for (int32 TestIdx = 0; TestIdx < OptionNode->SubNodes.Num(); TestIdx++)
									{
										UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(OptionNode->SubNodes[TestIdx]);
										if (TestNode)
										{
											int32 StatsStepIdx = INDEX_NONE;
											for (int32 Idx = 0; Idx < Stats.TotalAvgData.OptionData[StatsOptionIdx].TestIndices.Num(); Idx++)
											{
												if (Stats.TotalAvgData.OptionData[StatsOptionIdx].TestIndices[Idx] == TestIdx)
												{
													StatsStepIdx = Idx + NumGenerators;
													break;
												}
											}

											if (StatsStepIdx != INDEX_NONE)
											{
												TestNode->bStatShowOverlay = true;
												TestNode->Stats = GetOverlayStatsHelper(Stats, StatsOptionIdx, StatsStepIdx);
											}
										}
									}
								}
							}

							AssetOptionIdx++;
						}
					}
				}
			}

			break;
		}
	}
}

#endif // USE_EQS_DEBUGGER
