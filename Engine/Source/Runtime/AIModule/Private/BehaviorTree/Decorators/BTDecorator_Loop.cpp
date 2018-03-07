// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Decorators/BTDecorator_Loop.h"
#include "Engine/World.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"

UBTDecorator_Loop::UBTDecorator_Loop(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Loop";
	NumLoops = 3;
	InfiniteLoopTimeoutTime = -1.f;
	bNotifyActivation = true;
	
	bAllowAbortNone = false;
	bAllowAbortLowerPri = false;
	bAllowAbortChildNodes = false;
}

void UBTDecorator_Loop::OnNodeActivation(FBehaviorTreeSearchData& SearchData)
{
	FBTLoopDecoratorMemory* DecoratorMemory = GetNodeMemory<FBTLoopDecoratorMemory>(SearchData);
	FBTCompositeMemory* ParentMemory = GetParentNode()->GetNodeMemory<FBTCompositeMemory>(SearchData);
	const bool bIsSpecialNode = GetParentNode()->IsA(UBTComposite_SimpleParallel::StaticClass());

	if ((bIsSpecialNode && ParentMemory->CurrentChild == BTSpecialChild::NotInitialized) ||
		(!bIsSpecialNode && ParentMemory->CurrentChild != ChildIndex))
	{
		// initialize counter if it's first activation
		DecoratorMemory->RemainingExecutions = NumLoops;
		DecoratorMemory->TimeStarted = GetWorld()->GetTimeSeconds();
	}

	bool bShouldLoop = false;
	if (bInfiniteLoop)
	{
		// protect from truly infinite loop within single search
		if (SearchData.SearchId != DecoratorMemory->SearchId)
		{
			if ((InfiniteLoopTimeoutTime < 0.f) || ((DecoratorMemory->TimeStarted + InfiniteLoopTimeoutTime) > GetWorld()->GetTimeSeconds()))
			{
				bShouldLoop = true;
			}
		}

		DecoratorMemory->SearchId = SearchData.SearchId;
	}
	else
	{
		DecoratorMemory->RemainingExecutions--;
		bShouldLoop = DecoratorMemory->RemainingExecutions > 0;
	}


	// set child selection overrides
	if (bShouldLoop)
	{
		GetParentNode()->SetChildOverride(SearchData, ChildIndex);
	}
}

FString UBTDecorator_Loop::GetStaticDescription() const
{
	// basic info: infinite / num loops
	if (bInfiniteLoop)
	{
		if (InfiniteLoopTimeoutTime < 0.f)
		{
			return FString::Printf(TEXT("%s: infinite"), *Super::GetStaticDescription());
		}
		else
		{
			return FString::Printf(TEXT("%s: loop for %s seconds"), *Super::GetStaticDescription(), *FString::SanitizeFloat(InfiniteLoopTimeoutTime));
		}
	}
	else
	{
		return FString::Printf(TEXT("%s: %d loops"), *Super::GetStaticDescription(), NumLoops);
	}
}

void UBTDecorator_Loop::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	if (!bInfiniteLoop)
	{
		FBTLoopDecoratorMemory* DecoratorMemory = (FBTLoopDecoratorMemory*)NodeMemory;
		Values.Add(FString::Printf(TEXT("loops remaining: %d"), DecoratorMemory->RemainingExecutions));
	}
	else if (InfiniteLoopTimeoutTime > 0.f)
	{
		FBTLoopDecoratorMemory* DecoratorMemory = (FBTLoopDecoratorMemory*)NodeMemory;

		const float TimeRemaining = InfiniteLoopTimeoutTime - (GetWorld()->GetTimeSeconds() - DecoratorMemory->TimeStarted);
		Values.Add(FString::Printf(TEXT("time remaining: %s"), *FString::SanitizeFloat(InfiniteLoopTimeoutTime)));
	}
}

uint16 UBTDecorator_Loop::GetInstanceMemorySize() const
{
	return sizeof(FBTLoopDecoratorMemory);
}

#if WITH_EDITOR

FName UBTDecorator_Loop::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.Loop.Icon");
}

#endif	// WITH_EDITOR
