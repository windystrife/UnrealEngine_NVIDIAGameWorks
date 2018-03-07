// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "Perception/AISense.h"
#include "AISense_Blueprint.generated.h"

class APawn;
class UAIPerceptionComponent;
class UAISenseEvent;
class UUserDefinedStruct;

UCLASS(ClassGroup = AI, Abstract, Blueprintable)
class AIMODULE_API UAISense_Blueprint : public UAISense
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sense")
	TSubclassOf<UUserDefinedStruct> ListenerDataType;

	UPROPERTY(BlueprintReadOnly, Category = "Sense")
	TArray<UAIPerceptionComponent*> ListenerContainer;

	UPROPERTY()
	TArray<UAISenseEvent*> UnprocessedEvents;

public:
	UAISense_Blueprint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//void RegisterEvent(const FAITeamStimulusEvent& Event);	

	/** returns requested amount of time to pass until next frame. 
	 *	Return 0 to get update every frame (WARNING: hits performance) */
	UFUNCTION(BlueprintImplementableEvent)
	float OnUpdate(const TArray<UAISenseEvent*>& EventsToProcess);

	/**
	 *	@param PerceptionComponent is ActorListener's AIPerceptionComponent instance
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void OnListenerRegistered(AActor* ActorListener, UAIPerceptionComponent* PerceptionComponent);

	/**
	 *	@param PerceptionComponent is ActorListener's AIPerceptionComponent instance
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void OnListenerUpdated(AActor* ActorListener, UAIPerceptionComponent* PerceptionComponent);

	/** called when a listener unregistered from this sense. Most often this is called due to actor's death
	 *	@param PerceptionComponent is ActorListener's AIPerceptionComponent instance
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void OnListenerUnregistered(AActor* ActorListener, UAIPerceptionComponent* PerceptionComponent);
	
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void GetAllListenerActors(TArray<AActor*>& ListenerActors) const;

	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void GetAllListenerComponents(TArray<UAIPerceptionComponent*>& ListenerComponents) const;

	/** called when sense's instance gets notified about new pawn that has just been spawned */
	UFUNCTION(BlueprintImplementableEvent, DisplayName="OnNewPawn")
	void K2_OnNewPawn(APawn* NewPawn);

	virtual FAISenseID UpdateSenseID() override;
	virtual void RegisterWrappedEvent(UAISenseEvent& PerceptionEvent) override;

protected:
	virtual void OnNewPawn(APawn& NewPawn) override;
	virtual float Update() override;
	
	void OnNewListenerImpl(const FPerceptionListener& NewListener);
	void OnListenerUpdateImpl(const FPerceptionListener& UpdatedListener);
	void OnListenerRemovedImpl(const FPerceptionListener& UpdatedListener);

private:
	static TMap<NAME_INDEX, FAISenseID> BPSenseToSenseID;
};
