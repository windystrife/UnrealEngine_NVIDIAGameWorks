// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimInstanceProxy.h"

bool FAnimNotifyQueue::PassesFiltering(const FAnimNotifyEvent* Notify) const
{
	switch (Notify->NotifyFilterType)
	{
		case ENotifyFilterType::NoFiltering:
		{
			return true;
		}
		case ENotifyFilterType::LOD:
		{
			return Notify->NotifyFilterLOD > PredictedLODLevel;
		}
		default:
		{
			ensure(false); // Unknown Filter Type
		}
	}
	return true;
}

bool FAnimNotifyQueue::PassesChanceOfTriggering(const FAnimNotifyEvent* Event) const
{
	return Event->NotifyStateClass ? true : RandomStream.FRandRange(0.f, 1.f) < Event->NotifyTriggerChance;
}

void FAnimNotifyQueue::AddAnimNotifiesToDest(const TArray<const FAnimNotifyEvent*>& NewNotifies, TArray<const FAnimNotifyEvent*>& DestArray, const float InstanceWeight)
{
	// for now there is no filter whatsoever, it just adds everything requested
	for (const FAnimNotifyEvent* Notify : NewNotifies)
	{
		// only add if it is over TriggerWeightThreshold
		const bool bPassesDedicatedServerCheck = Notify->bTriggerOnDedicatedServer || !IsRunningDedicatedServer();
		if (bPassesDedicatedServerCheck && Notify->TriggerWeightThreshold <= InstanceWeight && PassesFiltering(Notify) && PassesChanceOfTriggering(Notify))
		{
			// Only add unique AnimNotifyState instances just once. We can get multiple triggers if looping over an animation.
			// It is the same state, so just report it once.
			Notify->NotifyStateClass ? DestArray.AddUnique(Notify) : DestArray.Add(Notify);
		}
	}
}

void FAnimNotifyQueue::AddAnimNotifies(const TArray<const FAnimNotifyEvent*>& NewNotifies, const float InstanceWeight)
{
	AddAnimNotifiesToDest(NewNotifies, AnimNotifies, InstanceWeight);
}

void FAnimNotifyQueue::AddAnimNotifies(const TMap<FName, TArray<const FAnimNotifyEvent*>>& NewNotifies, const float InstanceWeight)
{
	for (const TPair<FName, TArray<const FAnimNotifyEvent*>>& Pair : NewNotifies)
	{
		TArray<const FAnimNotifyEvent*>& Notifies = UnfilteredMontageAnimNotifies.FindOrAdd(Pair.Key);
		AddAnimNotifiesToDest(Pair.Value, Notifies, InstanceWeight);
	}
}

void FAnimNotifyQueue::Reset(USkeletalMeshComponent* Component)
{
	AnimNotifies.Reset();
	UnfilteredMontageAnimNotifies.Reset();
	PredictedLODLevel = Component ? Component->PredictedLODLevel : -1;
}

void FAnimNotifyQueue::Append(const FAnimNotifyQueue& Queue)
{
	// we dont just append here - we need to preserve uniqueness for AnimNotifyState instances
	for (const FAnimNotifyEvent* Notify : Queue.AnimNotifies)
	{
		Notify->NotifyStateClass ? AnimNotifies.AddUnique(Notify) : AnimNotifies.Add(Notify);
	}
	for (const TPair<FName, TArray<const FAnimNotifyEvent*>>& Pair : Queue.UnfilteredMontageAnimNotifies)
	{
		TArray<const FAnimNotifyEvent*>& Notifies = UnfilteredMontageAnimNotifies.FindOrAdd(Pair.Key);
		for (const FAnimNotifyEvent* Notify : Pair.Value)
		{
			Notify->NotifyStateClass ? Notifies.AddUnique(Notify) : Notifies.Add(Notify);
		}
	}
}

void FAnimNotifyQueue::ApplyMontageNotifies(const FAnimInstanceProxy& Proxy)
{
	for (const TPair<FName, TArray<const FAnimNotifyEvent*>>& Pair : UnfilteredMontageAnimNotifies)
	{
		if (Proxy.IsSlotNodeRelevantForNotifies(Pair.Key))
		{
			for (const FAnimNotifyEvent* Notify : Pair.Value)
			{
				Notify->NotifyStateClass ? AnimNotifies.AddUnique(Notify) : AnimNotifies.Add(Notify);
			}
		}
	}
	UnfilteredMontageAnimNotifies.Reset();
}
