// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "AbilitySystemComponent.h"

UAbilityTask_NetworkSyncPoint::UAbilityTask_NetworkSyncPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicatedEventToListenFor = EAbilityGenericReplicatedEvent::MAX;
}

void UAbilityTask_NetworkSyncPoint::OnSignalCallback()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->ConsumeGenericReplicatedEvent(ReplicatedEventToListenFor, GetAbilitySpecHandle(), GetActivationPredictionKey());
	}
	SyncFinished();
}

UAbilityTask_NetworkSyncPoint* UAbilityTask_NetworkSyncPoint::WaitNetSync(class UGameplayAbility* OwningAbility, EAbilityTaskNetSyncType InSyncType)
{
	UAbilityTask_NetworkSyncPoint* MyObj = NewAbilityTask<UAbilityTask_NetworkSyncPoint>(OwningAbility);
	MyObj->SyncType = InSyncType;
	return MyObj;
}

void UAbilityTask_NetworkSyncPoint::Activate()
{
	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent, IsPredictingClient());

	if (AbilitySystemComponent)
	{
		if (IsPredictingClient())
		{
			if (SyncType != EAbilityTaskNetSyncType::OnlyServerWait )
			{
				// As long as we are waiting (!= OnlyServerWait), listen for the GenericSignalFromServer event
				ReplicatedEventToListenFor = EAbilityGenericReplicatedEvent::GenericSignalFromServer;
			}
			if (SyncType != EAbilityTaskNetSyncType::OnlyClientWait)
			{
				// As long as the server is waiting (!= OnlyClientWait), send the Server and RPC for this signal
				AbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericSignalFromClient, GetAbilitySpecHandle(), GetActivationPredictionKey(), AbilitySystemComponent->ScopedPredictionKey);
			}
			
		}
		else if (IsForRemoteClient())
		{
			if (SyncType != EAbilityTaskNetSyncType::OnlyClientWait )
			{
				// As long as we are waiting (!= OnlyClientWait), listen for the GenericSignalFromClient event
				ReplicatedEventToListenFor = EAbilityGenericReplicatedEvent::GenericSignalFromClient;
			}
			if (SyncType != EAbilityTaskNetSyncType::OnlyServerWait)
			{
				// As long as the client is waiting (!= OnlyServerWait), send the Server and RPC for this signal
				AbilitySystemComponent->ClientSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericSignalFromServer, GetAbilitySpecHandle(), GetActivationPredictionKey());
			}
		}

		if (ReplicatedEventToListenFor != EAbilityGenericReplicatedEvent::MAX)
		{
			CallOrAddReplicatedDelegate(ReplicatedEventToListenFor, FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UAbilityTask_NetworkSyncPoint::OnSignalCallback));
		}
		else
		{
			// We aren't waiting for a replicated event, so the sync is complete.
			SyncFinished();
		}
	}
}

void UAbilityTask_NetworkSyncPoint::SyncFinished()
{
	if (!IsPendingKill())
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnSync.Broadcast();
		}
		EndTask();
	}
}
