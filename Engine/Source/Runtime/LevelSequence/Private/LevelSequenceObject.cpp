// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceObject.h"
#include "GameFramework/Actor.h"


/* FSequencerPossessedObject interface
 *****************************************************************************/

UObject* FLevelSequenceObject::GetObject() const
{
	// get component-less object
	if (ComponentName.IsEmpty())
	{
		return ObjectOrOwner.Get();
	}

	// get cached component
	if ((CachedComponent.IsValid()) && (CachedComponent->GetName() == ComponentName))
	{
		return CachedComponent.Get();
	}

	CachedComponent = nullptr;

	// find & cache component
	UObject* Object = ObjectOrOwner.Get();

	if (Object == nullptr)
	{
		return nullptr;
	}

	AActor* Owner = Cast<AActor>(Object);

	if (Owner == nullptr)
	{
		return Object;
	}

	for (UActorComponent* ActorComponent : Owner->GetComponents())
	{
		if (ActorComponent->GetName() == ComponentName)
		{
			CachedComponent = ActorComponent;

			return ActorComponent;
		}
	}

	// component not found
	return nullptr;
}
