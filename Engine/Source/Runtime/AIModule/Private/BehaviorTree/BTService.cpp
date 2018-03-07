// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/BTService.h"

UBTService::UBTService(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bNotifyTick = true;
	bNotifyOnSearch = true;
	bTickIntervals = true;
	bCallTickOnSearchStart = false;
	bRestartTimerOnEachActivation = false;

	Interval = 0.5f;
	RandomDeviation = 0.1f;
}

void UBTService::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	ScheduleNextTick(NodeMemory);
}

void UBTService::OnSearchStart(FBehaviorTreeSearchData& SearchData)
{
	// empty in base class
}

void UBTService::NotifyParentActivation(FBehaviorTreeSearchData& SearchData)
{
	if (bNotifyOnSearch || bNotifyTick)
	{
		UBTNode* NodeOb = bCreateNodeInstance ? GetNodeInstance(SearchData) : this;
		if (NodeOb)
		{
			UBTService* ServiceNodeOb = (UBTService*)NodeOb;
			uint8* NodeMemory = GetNodeMemory<uint8>(SearchData);

			if (bNotifyTick)
			{
				const float RemainingTime = bRestartTimerOnEachActivation ? 0.0f : GetNextTickRemainingTime(NodeMemory);
				if (RemainingTime <= 0.0f)
				{
					ServiceNodeOb->ScheduleNextTick(NodeMemory);
				}
			}

			if (bNotifyOnSearch)
			{
				ServiceNodeOb->OnSearchStart(SearchData);
			}

			if (bCallTickOnSearchStart)
			{
				ServiceNodeOb->TickNode(SearchData.OwnerComp, NodeMemory, 0.0f);
			}
		}
	}
}

FString UBTService::GetStaticTickIntervalDescription() const
{
	FString IntervalDesc = (RandomDeviation > 0.0f) ?
		FString::Printf(TEXT("%.2fs..%.2fs"), FMath::Max(0.0f, Interval - RandomDeviation), (Interval + RandomDeviation)) :
		FString::Printf(TEXT("%.2fs"), Interval);

	return FString::Printf(TEXT("tick every %s"), *IntervalDesc);
}

FString UBTService::GetStaticServiceDescription() const
{
	return GetStaticTickIntervalDescription();
}

FString UBTService::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s"), *UBehaviorTreeTypes::GetShortTypeName(this), *GetStaticServiceDescription());
}

#if WITH_EDITOR

FName UBTService::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Service.Icon");
}

#endif // WITH_EDITOR

void UBTService::ScheduleNextTick(uint8* NodeMemory)
{
	const float NextTickTime = FMath::FRandRange(FMath::Max(0.0f, Interval - RandomDeviation), (Interval + RandomDeviation));
	SetNextTickTime(NodeMemory, NextTickTime);
}
