// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Perception/AISense_Touch.h"
#include "Perception/AIPerceptionListenerInterface.h"
#include "Perception/AIPerceptionComponent.h"

UAISense_Touch::UAISense_Touch(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
}

float UAISense_Touch::Update()
{
	AIPerception::FListenerMap& ListenersMap = *GetListeners();

	for (int32 EventIndex = 0; EventIndex < RegisteredEvents.Num(); ++EventIndex)
	{
		const FAITouchEvent& Event = RegisteredEvents[EventIndex];

		if (Event.TouchReceiver != NULL && Event.OtherActor != NULL)
		{
			IAIPerceptionListenerInterface* PerceptionListener = Cast<IAIPerceptionListenerInterface>(Event.TouchReceiver);
			if (PerceptionListener != NULL)
			{
				UAIPerceptionComponent* PerceptionComponent = PerceptionListener->GetPerceptionComponent();
				if (PerceptionComponent != NULL && ListenersMap.Contains(PerceptionComponent->GetListenerId()))
				{
					// this has to succeed, will assert a failure
					FPerceptionListener& Listener = ListenersMap[PerceptionComponent->GetListenerId()];
					if (Listener.HasSense(GetSenseID()))
					{
						Listener.RegisterStimulus(Event.OtherActor, FAIStimulus(*this, 1.f, Event.Location, Event.Location));
					}
				}
			}
		}
	}

	RegisteredEvents.Reset();

	// return decides when next tick is going to happen
	return SuspendNextUpdate;
}

void UAISense_Touch::RegisterEvent(const FAITouchEvent& Event)
{
	RegisteredEvents.Add(Event);

	RequestImmediateUpdate();
}
