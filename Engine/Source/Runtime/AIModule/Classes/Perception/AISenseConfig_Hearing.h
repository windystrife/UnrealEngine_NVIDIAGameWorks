// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig.h"
#include "Perception/AISense_Hearing.h"
#include "AISenseConfig_Hearing.generated.h"

class FGameplayDebuggerCategory;
class UAIPerceptionComponent;

UCLASS(meta = (DisplayName = "AI Hearing config"))
class AIMODULE_API UAISenseConfig_Hearing : public UAISenseConfig
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", NoClear, config)
	TSubclassOf<UAISense_Hearing> Implementation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense")
	float HearingRange;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", meta = (EditCondition = "bUseLoSHearing"))
	float LoSHearingRange;

	/** Warning: has significant runtime cost */
	UPROPERTY(EditDefaultsOnly, Category = "Sense", meta = (InlineEditConditionToggle))
	uint32 bUseLoSHearing : 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", config)
	FAISenseAffiliationFilter DetectionByAffiliation;

	virtual TSubclassOf<UAISense> GetSenseImplementation() const override;

#if WITH_GAMEPLAY_DEBUGGER
	virtual void DescribeSelfToGameplayDebugger(const UAIPerceptionComponent* PerceptionComponent, FGameplayDebuggerCategory* DebuggerCategory) const;
#endif // WITH_GAMEPLAY_DEBUGGER
};
