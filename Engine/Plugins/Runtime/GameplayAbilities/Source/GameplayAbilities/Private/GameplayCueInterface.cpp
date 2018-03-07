// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayCueInterface.h"
#include "AbilitySystemStats.h"
#include "GameplayTagsModule.h"
#include "AbilitySystemComponent.h"
#include "GameplayCueSet.h"


namespace GameplayCueInterfacePrivate
{
	struct FCueNameAndUFunction
	{
		FGameplayTag Tag;
		UFunction* Func;
	};
	typedef TMap<FGameplayTag, TArray<FCueNameAndUFunction> > FGameplayCueTagFunctionList;
	static TMap<FObjectKey, FGameplayCueTagFunctionList > PerClassGameplayTagToFunctionMap;
}


UGameplayCueInterface::UGameplayCueInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void IGameplayCueInterface::DispatchBlueprintCustomHandler(AActor* Actor, UFunction* Func, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	GameplayCueInterface_eventBlueprintCustomHandler_Parms Parms;
	Parms.EventType = EventType;
	Parms.Parameters = Parameters;

	Actor->ProcessEvent(Func, &Parms);
}

void IGameplayCueInterface::ClearTagToFunctionMap()
{
	GameplayCueInterfacePrivate::PerClassGameplayTagToFunctionMap.Empty();
}

void IGameplayCueInterface::HandleGameplayCues(AActor *Self, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	for (auto TagIt = GameplayCueTags.CreateConstIterator(); TagIt; ++TagIt)
	{
		HandleGameplayCue(Self, *TagIt, EventType, Parameters);
	}
}

bool IGameplayCueInterface::ShouldAcceptGameplayCue(AActor *Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	return true;
}

void IGameplayCueInterface::HandleGameplayCue(AActor *Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayCueInterface_HandleGameplayCue);

	// Look up a custom function for this gameplay tag. 
	UClass* Class = Self->GetClass();
	FGameplayTagContainer TagAndParentsContainer = GameplayCueTag.GetGameplayTagParents();

	Parameters.OriginalTag = GameplayCueTag;

	//Find entry for the class
	FObjectKey ClassObjectKey(Class);
	GameplayCueInterfacePrivate::FGameplayCueTagFunctionList& GameplayTagFunctionList = GameplayCueInterfacePrivate::PerClassGameplayTagToFunctionMap.FindOrAdd(ClassObjectKey);
	TArray<GameplayCueInterfacePrivate::FCueNameAndUFunction>* FunctionList = GameplayTagFunctionList.Find(GameplayCueTag);
	if (FunctionList == NULL)
	{
		//generate new function list
		FunctionList = &GameplayTagFunctionList.Add(GameplayCueTag);

		for (auto InnerTagIt = TagAndParentsContainer.CreateConstIterator(); InnerTagIt; ++InnerTagIt)
		{
			UFunction* Func = NULL;
			FName CueName = InnerTagIt->GetTagName();

			Func = Class->FindFunctionByName(CueName, EIncludeSuperFlag::IncludeSuper);
			// If the handler calls ForwardGameplayCueToParent, keep calling functions until one consumes the cue and doesn't forward it
			while (Func)
			{
				GameplayCueInterfacePrivate::FCueNameAndUFunction NewCueFunctionPair;
				NewCueFunctionPair.Tag = *InnerTagIt;
				NewCueFunctionPair.Func = Func;
				FunctionList->Add(NewCueFunctionPair);

				Func = Func->GetSuperFunction();
			}

			// Native functions cant be named with ".", so look for them with _. 
			FName NativeCueFuncName = *CueName.ToString().Replace(TEXT("."), TEXT("_"));
			Func = Class->FindFunctionByName(NativeCueFuncName, EIncludeSuperFlag::IncludeSuper);

			while (Func)
			{
				GameplayCueInterfacePrivate::FCueNameAndUFunction NewCueFunctionPair;
				NewCueFunctionPair.Tag = *InnerTagIt;
				NewCueFunctionPair.Func = Func;
				FunctionList->Add(NewCueFunctionPair);

				Func = Func->GetSuperFunction();
			}
		}
	}

	//Iterate through all functions in the list until we should no longer continue
	check(FunctionList);
		
	bool bShouldContinue = true;
	for (int32 FunctionIndex = 0; bShouldContinue && (FunctionIndex < FunctionList->Num()); ++FunctionIndex)
	{
		GameplayCueInterfacePrivate::FCueNameAndUFunction& CueFunctionPair = FunctionList->GetData()[FunctionIndex];
		UFunction* Func = CueFunctionPair.Func;
		Parameters.MatchedTagName = CueFunctionPair.Tag;

		// Reset the forward parameter now, so we can check it after function
		bForwardToParent = false;
		IGameplayCueInterface::DispatchBlueprintCustomHandler(Self, Func, EventType, Parameters);

		bShouldContinue = bForwardToParent;
	}

	if (bShouldContinue)
	{
		TArray<UGameplayCueSet*> Sets;
		GetGameplayCueSets(Sets);
		for (UGameplayCueSet* Set : Sets)
		{
			bShouldContinue = Set->HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
			if (!bShouldContinue)
			{
				break;
			}
		}
	}

	if (bShouldContinue)
	{
		Parameters.MatchedTagName = GameplayCueTag;
		GameplayCueDefaultHandler(EventType, Parameters);
	}
}

void IGameplayCueInterface::GameplayCueDefaultHandler(EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	// No default handler, subclasses can implement
}

void IGameplayCueInterface::ForwardGameplayCueToParent()
{
	// Consumed by HandleGameplayCue
	bForwardToParent = true;
}

void FActiveGameplayCue::PreReplicatedRemove(const struct FActiveGameplayCueContainer &InArray)
{
	// We don't check the PredictionKey here like we do in PostReplicatedAdd. PredictionKey tells us
	// if we were predictely created, but this doesn't mean we will predictively remove ourselves.
	if (bPredictivelyRemoved == false)
	{
		// If predicted ignore the add/remove
		InArray.Owner->UpdateTagMap(GameplayCueTag, -1);
		InArray.Owner->InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Removed, Parameters);
	}
}

void FActiveGameplayCue::PostReplicatedAdd(const struct FActiveGameplayCueContainer &InArray)
{
	InArray.Owner->UpdateTagMap(GameplayCueTag, 1);

	if (PredictionKey.IsLocalClientKey() == false)
	{
		// If predicted ignore the add/remove
		InArray.Owner->InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive, Parameters);
	}
}

FString FActiveGameplayCue::GetDebugString()
{
	return FString::Printf(TEXT("(%s / %s"), *GameplayCueTag.ToString(), *PredictionKey.ToString());
}

void FActiveGameplayCueContainer::AddCue(const FGameplayTag& Tag, const FPredictionKey& PredictionKey, const FGameplayCueParameters& Parameters)
{
	UWorld* World = Owner->GetWorld();

	// Store the prediction key so the client can investigate it
	FActiveGameplayCue&	NewCue = GameplayCues[GameplayCues.AddDefaulted()];
	NewCue.GameplayCueTag = Tag;
	NewCue.PredictionKey = PredictionKey;
	NewCue.Parameters = Parameters;
	MarkItemDirty(NewCue);
	
	Owner->UpdateTagMap(Tag, 1);
}

void FActiveGameplayCueContainer::RemoveCue(const FGameplayTag& Tag)
{
	for (int32 idx=0; idx < GameplayCues.Num(); ++idx)
	{
		FActiveGameplayCue& Cue = GameplayCues[idx];

		if (Cue.GameplayCueTag == Tag)
		{
			GameplayCues.RemoveAt(idx);
			MarkArrayDirty();
			Owner->UpdateTagMap(Tag, -1);
			return;
		}
	}
}

void FActiveGameplayCueContainer::PredictiveRemove(const FGameplayTag& Tag)
{
	for (int32 idx=0; idx < GameplayCues.Num(); ++idx)
	{
		FActiveGameplayCue& Cue = GameplayCues[idx];
		if (Cue.GameplayCueTag == Tag)
		{			
			// Predictive remove: mark the cue as predictive remove, invoke remove event, update tag map.
			// DONT remove from the replicated array.
			Cue.bPredictivelyRemoved = true;
			Owner->UpdateTagMap(Tag, -1);
			Owner->InvokeGameplayCueEvent(Tag, EGameplayCueEvent::Removed, Cue.Parameters);	
			return;
		}
	}
}

void FActiveGameplayCueContainer::PredictiveAdd(const FGameplayTag& Tag, FPredictionKey& PredictionKey)
{
	Owner->UpdateTagMap(Tag, 1);	
	PredictionKey.NewRejectOrCaughtUpDelegate(FPredictionKeyEvent::CreateUObject(Owner, &UAbilitySystemComponent::OnPredictiveGameplayCueCatchup, Tag));
}

bool FActiveGameplayCueContainer::HasCue(const FGameplayTag& Tag) const
{
	for (int32 idx=0; idx < GameplayCues.Num(); ++idx)
	{
		const FActiveGameplayCue& Cue = GameplayCues[idx];
		if (Cue.GameplayCueTag == Tag)
		{
			return true;
		}
	}

	return false;
}

bool FActiveGameplayCueContainer::NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
{
	if (bMinimalReplication && (Owner && Owner->ReplicationMode == EReplicationMode::Full))
	{
		return false;
	}

	return FastArrayDeltaSerialize<FActiveGameplayCue>(GameplayCues, DeltaParms, *this);
}
