// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Services/BTService_BlackboardBase.h"

UBTService_BlackboardBase::UBTService_BlackboardBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "BlackboardBase";

	// empty KeySelector = allow everything
}

void UBTService_BlackboardBase::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	UBlackboardData* BBAsset = GetBlackboardAsset();
	if (ensure(BBAsset))
	{
		BlackboardKey.ResolveSelectedKey(*BBAsset);
	}
}
