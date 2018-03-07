// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GenericTeamAgentInterface.h"
#include "Perception/AISense.h"
#include "AISense_Team.generated.h"

class UAISense_Team;

USTRUCT()
struct AIMODULE_API FAITeamStimulusEvent
{	
	GENERATED_USTRUCT_BODY()

	typedef UAISense_Team FSenseClass;

	FVector LastKnowLocation;
private:
	FVector BroadcastLocation;
public:
	float RangeSq;
	float InformationAge;
	FGenericTeamId TeamIdentifier;
	float Strength;
private:
	UPROPERTY()
	AActor* Broadcaster;
public:
	UPROPERTY()
	AActor* Enemy;
		
	FAITeamStimulusEvent(){}	
	FAITeamStimulusEvent(AActor* InBroadcaster, AActor* InEnemy, const FVector& InLastKnowLocation, float EventRange, float PassedInfoAge = 0.f, float InStrength = 1.f);

	FORCEINLINE void CacheBroadcastLocation()
	{
		BroadcastLocation = Broadcaster ? Broadcaster->GetActorLocation() : FAISystem::InvalidLocation;
	}

	FORCEINLINE const FVector& GetBroadcastLocation() const 
	{
		return BroadcastLocation;
	}
};

UCLASS(ClassGroup=AI)
class AIMODULE_API UAISense_Team : public UAISense
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FAITeamStimulusEvent> RegisteredEvents;

public:		
	void RegisterEvent(const FAITeamStimulusEvent& Event);	

protected:
	virtual float Update() override;
};
