// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Decorators/BTDecorator_DoesPathExist.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTDecorator_DoesPathExist::UBTDecorator_DoesPathExist(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Does path exist";

	// accept only actors and vectors
	BlackboardKeyA.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_DoesPathExist, BlackboardKeyA), AActor::StaticClass());
	BlackboardKeyA.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_DoesPathExist, BlackboardKeyA));
	BlackboardKeyB.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_DoesPathExist, BlackboardKeyB), AActor::StaticClass());
	BlackboardKeyB.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_DoesPathExist, BlackboardKeyB));

	bAllowAbortLowerPri = false;
	bAllowAbortNone = true;
	bAllowAbortChildNodes = false;
	FlowAbortMode = EBTFlowAbortMode::None;

	BlackboardKeyA.SelectedKeyName = FBlackboard::KeySelf;
	PathQueryType = EPathExistanceQueryType::HierarchicalQuery;
}

void UBTDecorator_DoesPathExist::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (bUseSelf)
	{
		BlackboardKeyA.SelectedKeyName = FBlackboard::KeySelf;
		bUseSelf = false;
	}

	UBlackboardData* BBAsset = GetBlackboardAsset();
	if (ensure(BBAsset))
	{
		BlackboardKeyA.ResolveSelectedKey(*BBAsset);
		BlackboardKeyB.ResolveSelectedKey(*BBAsset);
	}
}

bool UBTDecorator_DoesPathExist::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (BlackboardComp == NULL)
	{
		return false;
	}

	FVector PointA = FVector::ZeroVector;
	FVector PointB = FVector::ZeroVector;	
	const bool bHasPointA = BlackboardComp->GetLocationFromEntry(BlackboardKeyA.GetSelectedKeyID(), PointA);
	const bool bHasPointB = BlackboardComp->GetLocationFromEntry(BlackboardKeyB.GetSelectedKeyID(), PointB);
	
	bool bHasPath = false;

	const UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(OwnerComp.GetWorld());
	if (NavSys && bHasPointA && bHasPointB)
	{
		const AAIController* AIOwner = OwnerComp.GetAIOwner();
		const ANavigationData* NavData = AIOwner ? NavSys->GetNavDataForProps(AIOwner->GetNavAgentPropertiesRef()) : NULL;
		if (NavData)
		{
			FSharedConstNavQueryFilter QueryFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, AIOwner, FilterClass);

			if (PathQueryType == EPathExistanceQueryType::NavmeshRaycast2D)
			{
#if WITH_RECAST
				const ARecastNavMesh* RecastNavMesh = Cast<const ARecastNavMesh>(NavData);
				bHasPath = RecastNavMesh && RecastNavMesh->IsSegmentOnNavmesh(PointA, PointB, QueryFilter);
#endif
			}
			else
			{
				EPathFindingMode::Type TestMode = (PathQueryType == EPathExistanceQueryType::HierarchicalQuery) ? EPathFindingMode::Hierarchical : EPathFindingMode::Regular;
				bHasPath = NavSys->TestPathSync(FPathFindingQuery(AIOwner, *NavData, PointA, PointB, QueryFilter), TestMode);
			}
		}
	}

	return bHasPath;
}

FString UBTDecorator_DoesPathExist::GetStaticDescription() const 
{
	const UEnum* PathTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPathExistanceQueryType"));
	check(PathTypeEnum);

	return FString::Printf(TEXT("%s: Find path from %s to %s (mode:%s)"),
		*Super::GetStaticDescription(),
		*BlackboardKeyA.SelectedKeyName.ToString(),
		*BlackboardKeyB.SelectedKeyName.ToString(),
		*PathTypeEnum->GetNameStringByValue(PathQueryType));
}

#if WITH_EDITOR

FName UBTDecorator_DoesPathExist::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.DoesPathExist.Icon");
}

#endif	// WITH_EDITOR
