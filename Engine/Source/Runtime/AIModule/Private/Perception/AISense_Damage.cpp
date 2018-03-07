// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Perception/AISense_Damage.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Perception/AIPerceptionListenerInterface.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseEvent_Damage.h"

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
FAIDamageEvent::FAIDamageEvent()
	: Amount(1.f), Location(FAISystem::InvalidLocation), HitLocation(FAISystem::InvalidLocation)
	, DamagedActor(nullptr), Instigator(nullptr)
{}

FAIDamageEvent::FAIDamageEvent(AActor* InDamagedActor, AActor* InInstigator, float DamageAmount, const FVector& EventLocation, const FVector& InHitLocation)
	: Amount(DamageAmount), Location(EventLocation), HitLocation(InHitLocation), DamagedActor(InDamagedActor), Instigator(InInstigator)
{
	Compile();
}

void FAIDamageEvent::Compile()
{
	if (DamagedActor == nullptr)
	{
		// nothing to do here, this event is invalid
		return;
	}

	const bool bHitLocationValid = FAISystem::IsValidLocation(HitLocation);
	const bool bEventLocationValid = FAISystem::IsValidLocation(Location);

	if (bHitLocationValid != bEventLocationValid)
	{
		if (bHitLocationValid)
		{
			HitLocation = Location;
		}
		else
		{
			Location = HitLocation;
		}
	}
	// both invalid
	else if ((bHitLocationValid || bEventLocationValid) == false)
	{
		HitLocation = Location = DamagedActor->GetActorLocation();
	}

}

IAIPerceptionListenerInterface* FAIDamageEvent::GetDamagedActorAsPerceptionListener() const
{
	IAIPerceptionListenerInterface* Listener = nullptr;
	if (DamagedActor)
	{
		Listener = Cast<IAIPerceptionListenerInterface>(DamagedActor);
		if (Listener == nullptr)
		{
			APawn* ListenerAsPawn = Cast<APawn>(DamagedActor);
			if (ListenerAsPawn)
			{
				Listener = Cast<IAIPerceptionListenerInterface>(ListenerAsPawn->GetController());
			}
		}
	}
	return Listener;
}
//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
UAISense_Damage::UAISense_Damage(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

float UAISense_Damage::Update()
{
	AIPerception::FListenerMap& ListenersMap = *GetListeners();

	for (int32 EventIndex = 0; EventIndex < RegisteredEvents.Num(); ++EventIndex)
	{
		const FAIDamageEvent& Event = RegisteredEvents[EventIndex];

		IAIPerceptionListenerInterface* PerceptionListener = Event.GetDamagedActorAsPerceptionListener();
		if (PerceptionListener != nullptr)
		{
			UAIPerceptionComponent* PerceptionComponent = PerceptionListener->GetPerceptionComponent();
			if (PerceptionComponent != nullptr && PerceptionComponent->GetListenerId().IsValid())
			{
				// this has to succeed, will assert a failure
				FPerceptionListener& Listener = ListenersMap[PerceptionComponent->GetListenerId()];

				if (Listener.HasSense(GetSenseID()))
				{
					Listener.RegisterStimulus(Event.Instigator, FAIStimulus(*this, Event.Amount, Event.Location, Event.HitLocation));
				}
			}
		}
	}

	RegisteredEvents.Reset();

	// return decides when next tick is going to happen
	return SuspendNextUpdate;
}

void UAISense_Damage::RegisterEvent(const FAIDamageEvent& Event)
{
	if (Event.IsValid())
	{
		RegisteredEvents.Add(Event);

		RequestImmediateUpdate();
	}
}

void UAISense_Damage::RegisterWrappedEvent(UAISenseEvent& PerceptionEvent)
{
	UAISenseEvent_Damage* DamageEvent = Cast<UAISenseEvent_Damage>(&PerceptionEvent);
	ensure(DamageEvent);
	if (DamageEvent)
	{
		RegisterEvent(DamageEvent->GetDamageEvent());
	}
}

void UAISense_Damage::ReportDamageEvent(UObject* WorldContextObject, AActor* DamagedActor, AActor* Instigator, float DamageAmount, FVector EventLocation, FVector HitLocation)
{
	UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(WorldContextObject);
	if (PerceptionSystem)
	{
		FAIDamageEvent Event(DamagedActor, Instigator, DamageAmount, EventLocation, HitLocation);
		PerceptionSystem->OnEvent(Event);
	}
}
