// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayPrediction.h"
#include "AbilitySystemComponent.h"

/** The key to understanding this function is that when a key is received by the server, we note which connection gave it to us. We only serialize the key back to that client.  */
bool FPredictionKey::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// First bit for valid key for this connection or not. (most keys are not valid)
	uint8 ValidKeyForConnection = 0;
	if (Ar.IsSaving())
	{
		/**
		 *	Only serialize the payload if we have no owning connection (Client sending to server)
		 *	or if the owning connection is this connection (Server only sends the prediction key to the client who gave it to us)
		 *  or if this is a server initiated key (valid on all connections)
		 */		
		ValidKeyForConnection = (PredictiveConnection == nullptr || (Map == PredictiveConnection) || bIsServerInitiated) && (Current > 0);
	}
	Ar.SerializeBits(&ValidKeyForConnection, 1);

	// Second bit for base key (only if valid connection)
	uint8 HasBaseKey = 0;
	if (ValidKeyForConnection)
	{
		if (Ar.IsSaving())
		{
			HasBaseKey = Base > 0;
		}
		Ar.SerializeBits(&HasBaseKey, 1);
	}

	// Third bit for server initiated
	uint8 ServerInitiatedByte = bIsServerInitiated;
	Ar.SerializeBits(&ServerInitiatedByte, 1);
	bIsServerInitiated = ServerInitiatedByte & 1;

	// Conditionally Serialize the Current and Base keys
	if (ValidKeyForConnection)
	{
		Ar << Current;
		if (HasBaseKey)
		{
			Ar << Base;
		}
	}	
	if (Ar.IsLoading())
	{
		// We are reading this key: the connection that gave us this key is the predictive connection, and we will only serialize this key back to it.
		if (!bIsServerInitiated)
		{
			PredictiveConnection = Map;
		}
	}

	bOutSuccess = true;
	return true;
}

void FPredictionKey::GenerateNewPredictionKey()
{
	static KeyType GKey = 1;
	Current = GKey++;
	if (GKey < 0)
	{
		GKey = 1;
	}
	bIsStale = false;
}

void FPredictionKey::GenerateDependentPredictionKey()
{
	if (bIsServerInitiated)
	{
		// Can't have dependent keys on server keys, use same key
		return;
	}

	KeyType Previous = 0;
	if (Base == 0)
	{
		Base = Current;
	}
	else
	{
		Previous = Current;
	}

	GenerateNewPredictionKey();

	if (Previous > 0)
	{
		FPredictionKeyDelegates::AddDependency(Current, Previous);
	}
}

FPredictionKey FPredictionKey::CreateNewPredictionKey(UAbilitySystemComponent* OwningComponent)
{
	FPredictionKey NewKey;
	
	// We should never generate prediction keys on the authority
	if(OwningComponent->GetOwnerRole() != ROLE_Authority)
	{
		NewKey.GenerateNewPredictionKey();
	}
	return NewKey;
}

FPredictionKey FPredictionKey::CreateNewServerInitiatedKey(UAbilitySystemComponent* OwningComponent)
{
	FPredictionKey NewKey;

	// Only valid on the server
	if (OwningComponent->GetOwnerRole() == ROLE_Authority)
	{
		NewKey.GenerateNewPredictionKey();
		NewKey.bIsServerInitiated = true;
	}
	return NewKey;
}


FPredictionKeyEvent& FPredictionKey::NewRejectedDelegate()
{
	return FPredictionKeyDelegates::NewRejectedDelegate(Current);
}

FPredictionKeyEvent& FPredictionKey::NewCaughtUpDelegate()
{
	return FPredictionKeyDelegates::NewCaughtUpDelegate(Current);
}

void FPredictionKey::NewRejectOrCaughtUpDelegate(FPredictionKeyEvent Event)
{
	FPredictionKeyDelegates::NewRejectOrCaughtUpDelegate(Current, Event);
}

// -------------------------------------

FPredictionKeyDelegates& FPredictionKeyDelegates::Get()
{
	static FPredictionKeyDelegates StaticMap;
	return StaticMap;
}

FPredictionKeyEvent& FPredictionKeyDelegates::NewRejectedDelegate(FPredictionKey::KeyType Key)
{
	TArray<FPredictionKeyEvent>& DelegateList = Get().DelegateMap.FindOrAdd(Key).RejectedDelegates;
	DelegateList.Add(FPredictionKeyEvent());
	return DelegateList.Top();
}

FPredictionKeyEvent& FPredictionKeyDelegates::NewCaughtUpDelegate(FPredictionKey::KeyType Key)
{
	TArray<FPredictionKeyEvent>& DelegateList = Get().DelegateMap.FindOrAdd(Key).CaughtUpDelegates;
	DelegateList.Add(FPredictionKeyEvent());
	return DelegateList.Top();
}

void FPredictionKeyDelegates::NewRejectOrCaughtUpDelegate(FPredictionKey::KeyType Key, FPredictionKeyEvent NewEvent)
{
	FDelegates& Delegates = Get().DelegateMap.FindOrAdd(Key);
	Delegates.CaughtUpDelegates.Add(NewEvent);
	Delegates.RejectedDelegates.Add(NewEvent);
}

void FPredictionKeyDelegates::BroadcastRejectedDelegate(FPredictionKey::KeyType Key)
{
	// Intentionally making a copy of the delegate list since it may change when firing one of the delegates
	static TArray<FPredictionKeyEvent> DelegateList;
	DelegateList.Reset();
	DelegateList = Get().DelegateMap.FindOrAdd(Key).RejectedDelegates;
	for (auto& Delegate : DelegateList)
	{
		Delegate.ExecuteIfBound();
	}
}

void FPredictionKeyDelegates::BroadcastCaughtUpDelegate(FPredictionKey::KeyType Key)
{
	// Intentionally making a copy of the delegate list since it may change when firing one of the delegates
	static TArray<FPredictionKeyEvent> DelegateList;
	DelegateList.Reset();
	DelegateList = Get().DelegateMap.FindOrAdd(Key).CaughtUpDelegates;
	for (auto& Delegate : DelegateList)
	{
		Delegate.ExecuteIfBound();
	}
}

void FPredictionKeyDelegates::Reject(FPredictionKey::KeyType Key)
{
	FDelegates* DelPtr = Get().DelegateMap.Find(Key);
	if (DelPtr)
	{
		for (auto& Delegate : DelPtr->RejectedDelegates)
		{
			Delegate.ExecuteIfBound();
		}

		Get().DelegateMap.Remove(Key);
	}
}

void FPredictionKeyDelegates::CatchUpTo(FPredictionKey::KeyType Key)
{
	FDelegates* DelPtr = Get().DelegateMap.Find(Key);
	if (DelPtr)
	{
		for (auto& Delegate : DelPtr->CaughtUpDelegates)
		{
			Delegate.ExecuteIfBound();
		}
		Get().DelegateMap.Remove(Key);
	}


#if 0 
	// Sanity checking
	for (auto MapIt = Get().DelegateMap.CreateIterator(); MapIt; ++MapIt)
	{
		if (MapIt.Key() <= Key)
		{	
			ABILITY_LOG(Warning, TEXT("PredictionKey is stale: %d after CatchUp to: %d"), MapIt.Key(), Key);

		}
	}
#endif
}

void FPredictionKeyDelegates::AddDependency(FPredictionKey::KeyType ThisKey, FPredictionKey::KeyType DependsOn)
{
	NewRejectedDelegate(DependsOn).BindStatic(&FPredictionKeyDelegates::Reject, ThisKey);
	NewCaughtUpDelegate(DependsOn).BindStatic(&FPredictionKeyDelegates::CatchUpTo, ThisKey);
}

// -------------------------------------

FScopedPredictionWindow::FScopedPredictionWindow(UAbilitySystemComponent* AbilitySystemComponent, FPredictionKey InPredictionKey, bool InSetReplicatedPredictionKey /*=true*/)
{
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}

	// This is used to set an already generated prediction key as the current scoped prediction key.
	// Should be called on the server for logical scopes where a given key is valid. E.g, "client gave me this key, we both are going to run Foo()".
	
	if (AbilitySystemComponent->IsNetSimulating() == false)
	{
		Owner = AbilitySystemComponent;
		check(Owner.IsValid());
		RestoreKey = Owner->ScopedPredictionKey;
		Owner->ScopedPredictionKey = InPredictionKey;
		ClearScopedPredictionKey = true;
		SetReplicatedPredictionKey = InSetReplicatedPredictionKey;
	}
}

FScopedPredictionWindow::FScopedPredictionWindow(UAbilitySystemComponent* InAbilitySystemComponent, bool bCanGenerateNewKey)
{
	// On the server, this will do nothing since he is authoritative and doesn't need a prediction key for anything.
	// On the client, this will generate a new prediction key if bCanGenerateNewKey is true, and we have a invalid prediction key.

	ClearScopedPredictionKey = false;
	SetReplicatedPredictionKey = false;
	Owner = InAbilitySystemComponent;

	if (!ensure(Owner.IsValid()) || Owner->IsNetSimulating() == false)
	{
		return;
	}
	
	// InAbilitySystemComponent->GetPredictionKey().IsValidForMorePrediction() == false && 
	if (bCanGenerateNewKey)
	{
		check(InAbilitySystemComponent != NULL); // Should have bailed above with ensure(Owner.IsValid())
		ClearScopedPredictionKey = true;
		RestoreKey = InAbilitySystemComponent->ScopedPredictionKey;
		InAbilitySystemComponent->ScopedPredictionKey.GenerateDependentPredictionKey();
		
	}
}

FScopedPredictionWindow::~FScopedPredictionWindow()
{
	if (Owner.IsValid())
	{
		if (SetReplicatedPredictionKey)
		{
			// It is important to not set the ReplicatedPredictionKey unless it is valid (>0).
			// If we werent given a new prediction key for this scope from the client, then setting the
			// replicated prediction key back to 0 could cause OnReps to be missed on the client during high PL.
			// (for example, predict w/ key 100 -> prediction key replication dropped -> predict w/ invalid key -> next rep of prediction key is 0).
			if (Owner->ScopedPredictionKey.IsValidKey())
			{
				Owner->ReplicatedPredictionKeyMap.ReplicatePredictionKey(Owner->ScopedPredictionKey);
			}
		}
		if (ClearScopedPredictionKey)
		{
			Owner->ScopedPredictionKey = RestoreKey;
		}
	}
}

// -----------------------------------

void FReplicatedPredictionKeyItem::OnRep()
{
	ABILITY_LOG(Verbose, TEXT("FReplicatedPredictionKeyItem::OnRep %s"), *PredictionKey.ToString());
	
	// Every predictive action we've done up to and including the current value of ReplicatedPredictionKey needs to be wiped
	FPredictionKeyDelegates::CatchUpTo(PredictionKey.Current);

	// Sanity checking
	int32 Index = PredictionKey.Current % FReplicatedPredictionKeyMap::KeyRingBufferSize;
	for (auto MapIt = FPredictionKeyDelegates::Get().DelegateMap.CreateIterator(); MapIt; ++MapIt)
	{
		// If older key
		if (MapIt.Key() <= PredictionKey.Current)
		{	
			// Older key that would have gone in this slot
			if (MapIt.Key() % FReplicatedPredictionKeyMap::KeyRingBufferSize == Index)
			{
				// Warn
				ABILITY_LOG(Warning, TEXT("Passed PredictionKey %d in Delegate map while OnRep'ing %s"), MapIt.Key(), *PredictionKey.ToString());

				// Execute CaughtUp delegates
				for (auto& Delegate : MapIt.Value().CaughtUpDelegates)
				{
					Delegate.ExecuteIfBound();
				}

				// Cleanup
				MapIt.RemoveCurrent();
			}
		}
	}
}

const int32 FReplicatedPredictionKeyMap::KeyRingBufferSize = 32;

FReplicatedPredictionKeyMap::FReplicatedPredictionKeyMap()
{
	PredictionKeys.SetNum(KeyRingBufferSize);
}

bool FReplicatedPredictionKeyMap::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	return FastArrayDeltaSerialize<FReplicatedPredictionKeyItem>(PredictionKeys, DeltaParms, *this);
}

void FReplicatedPredictionKeyMap::ReplicatePredictionKey(FPredictionKey Key)
{	
	int32 Index = (Key.Current % KeyRingBufferSize);
	PredictionKeys[Index].PredictionKey = Key;
	MarkItemDirty(PredictionKeys[Index]);
}

FString FReplicatedPredictionKeyMap::GetDebugString() const
{
	FPredictionKey HighKey;
	for (const FReplicatedPredictionKeyItem& Item : PredictionKeys)
	{
		if (Item.PredictionKey.Current > HighKey.Current)
		{
			HighKey = Item.PredictionKey;
		}
	}

	return HighKey.ToString();
}
