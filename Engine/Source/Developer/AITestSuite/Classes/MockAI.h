// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AITestsCommon.h"
#include "Tickable.h"

#include "MockAI.generated.h"

class UAIPerceptionComponent;
class UBlackboardComponent;
class UMockAI;

struct FTestTickHelper : FTickableGameObject
{
	TWeakObjectPtr<class UMockAI> Owner;

	FTestTickHelper() : Owner(NULL) {}
	virtual void Tick(float DeltaTime);
	virtual bool IsTickable() const { return Owner.IsValid(); }
	virtual bool IsTickableInEditor() const { return true; }
	virtual TStatId GetStatId() const;
};

UCLASS()
class UMockAI : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual ~UMockAI();

	FTestTickHelper TickHelper;

	UPROPERTY()
	UBlackboardComponent* BBComp;

	UPROPERTY()
	UBrainComponent* BrainComp;

	UPROPERTY()
	UAIPerceptionComponent* PerceptionComp;

	UPROPERTY()
	UPawnActionsComponent* PawnActionComp;
	
	template<typename TBrainClass>
	void UseBrainComponent()
	{
		BrainComp = NewObject<TBrainClass>(FAITestHelpers::GetWorld());
	}

	void UseBlackboardComponent();
	void UsePerceptionComponent();
	void UsePawnActionsComponent();

	void SetEnableTicking(bool bShouldTick);

	virtual void TickMe(float DeltaTime);
};
