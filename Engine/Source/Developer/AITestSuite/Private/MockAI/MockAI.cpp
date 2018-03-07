// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MockAI.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
void FTestTickHelper::Tick(float DeltaTime)
{
	if (Owner.IsValid())
	{
		Owner->TickMe(DeltaTime);
	}
}

TStatId FTestTickHelper::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FTestTickHelper, STATGROUP_Tickables);
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
UMockAI::UMockAI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UMockAI::~UMockAI()
{
	TickHelper.Owner.Reset();
}

void UMockAI::SetEnableTicking(bool bShouldTick)
{
	if (bShouldTick)
	{
		TickHelper.Owner = this;
	}
	else
	{
		TickHelper.Owner = NULL;
	}
}

void UMockAI::UseBlackboardComponent()
{
	BBComp = NewObject<UBlackboardComponent>(FAITestHelpers::GetWorld());
}

void UMockAI::UsePerceptionComponent()
{
	PerceptionComp = NewObject<UAIPerceptionComponent>(FAITestHelpers::GetWorld());
}

void UMockAI::UsePawnActionsComponent()
{
	PawnActionComp = NewObject<UPawnActionsComponent>(FAITestHelpers::GetWorld());
}

void UMockAI::TickMe(float DeltaTime)
{
	if (BBComp)
	{
		BBComp->TickComponent(DeltaTime, ELevelTick::LEVELTICK_All, NULL);
	}

	if (PerceptionComp)
	{
		PerceptionComp->TickComponent(DeltaTime, ELevelTick::LEVELTICK_All, NULL);
	}
	
	if (BrainComp)
	{
		BrainComp->TickComponent(DeltaTime, ELevelTick::LEVELTICK_All, NULL);
	}
	
	if (PawnActionComp)
	{
		PawnActionComp->TickComponent(DeltaTime, ELevelTick::LEVELTICK_All, NULL);
	}
}

