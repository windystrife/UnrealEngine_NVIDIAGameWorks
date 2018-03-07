// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Perception/AISenseEvent.h"
#include "Perception/AISense_Damage.h"
#include "AISenseEvent_Damage.generated.h"

UCLASS()
class AIMODULE_API UAISenseEvent_Damage : public UAISenseEvent
{
	GENERATED_BODY()

public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FAIDamageEvent Event;
	
	virtual FAISenseID GetSenseID() const override;

	FORCEINLINE FAIDamageEvent GetDamageEvent()
	{
		Event.Compile();
		return Event;
	}
};
