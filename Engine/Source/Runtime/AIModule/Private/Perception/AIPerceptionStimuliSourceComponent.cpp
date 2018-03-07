// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AIPerceptionSystem.h"

UAIPerceptionStimuliSourceComponent::UAIPerceptionStimuliSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSuccessfullyRegistered = false;
}

void UAIPerceptionStimuliSourceComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// when in the editor world we don't remove the null entries
	// since those can get changed to something else by the user
	if (!GIsEditor || GIsPlayInEditorWorld)
#endif // WITH_EDITOR
	{
		RegisterAsSourceForSenses.RemoveAllSwap([](TSubclassOf<UAISense> SenseClass) {
			return SenseClass == nullptr;
		});
	}

	if (bAutoRegisterAsSource)
	{
		RegisterWithPerceptionSystem();
	}
}

void UAIPerceptionStimuliSourceComponent::RegisterWithPerceptionSystem()
{
	if (bSuccessfullyRegistered)
	{
		return;
	}
	if (RegisterAsSourceForSenses.Num() == 0)
	{
		bSuccessfullyRegistered = true;
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (World)
	{
		UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(World);
		if (PerceptionSystem)
		{
			for (auto& SenseClass : RegisterAsSourceForSenses)
			{
				if(SenseClass)
				{
					PerceptionSystem->RegisterSourceForSenseClass(SenseClass, *OwnerActor);
					bSuccessfullyRegistered = true;
				}
				// we just ignore the empty entries
			}
		}
	}
}

void UAIPerceptionStimuliSourceComponent::RegisterForSense(TSubclassOf<UAISense> SenseClass)
{
	if (!SenseClass)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (World)
	{
		UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(World);
		if (PerceptionSystem)
		{
			PerceptionSystem->RegisterSourceForSenseClass(SenseClass, *OwnerActor);
			bSuccessfullyRegistered = true;
		}
	}
}

void UAIPerceptionStimuliSourceComponent::UnregisterFromPerceptionSystem()
{
	if (bSuccessfullyRegistered == false)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (World)
	{
		UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(World);
		if (PerceptionSystem)
		{
			for (auto& SenseClass : RegisterAsSourceForSenses)
			{
				PerceptionSystem->UnregisterSource(*OwnerActor, SenseClass);
			}
		}
	}

	bSuccessfullyRegistered = false;
}

void UAIPerceptionStimuliSourceComponent::UnregisterFromSense(TSubclassOf<UAISense> SenseClass)
{
	if (!SenseClass)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (World)
	{
		UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(World);
		if (PerceptionSystem)
		{
			PerceptionSystem->UnregisterSource(*OwnerActor, SenseClass);
			RegisterAsSourceForSenses.RemoveSingleSwap(SenseClass, /*bAllowShrinking=*/false);
			bSuccessfullyRegistered = RegisterAsSourceForSenses.Num() > 0;
		}
	}
}
