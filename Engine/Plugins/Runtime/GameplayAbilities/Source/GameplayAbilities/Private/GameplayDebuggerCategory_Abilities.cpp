// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerCategory_Abilities.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

FGameplayDebuggerCategory_Abilities::FGameplayDebuggerCategory_Abilities()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Abilities::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_Abilities());
}

void FGameplayDebuggerCategory_Abilities::FRepData::Serialize(FArchive& Ar)
{
	Ar << OwnedTags;

	int32 NumAbilities = Abilities.Num();
	Ar << NumAbilities;
	if (Ar.IsLoading())
	{
		Abilities.SetNum(NumAbilities);
	}

	for (int32 Idx = 0; Idx < NumAbilities; Idx++)
	{
		Ar << Abilities[Idx].Ability;
		Ar << Abilities[Idx].Source;
		Ar << Abilities[Idx].Level;
		Ar << Abilities[Idx].bIsActive;
	}

	int32 NumGE = GameplayEffects.Num();
	Ar << NumGE;
	if (Ar.IsLoading())
	{
		GameplayEffects.SetNum(NumGE);
	}

	for (int32 Idx = 0; Idx < NumGE; Idx++)
	{
		Ar << GameplayEffects[Idx].Effect;
		Ar << GameplayEffects[Idx].Context;
		Ar << GameplayEffects[Idx].Duration;
		Ar << GameplayEffects[Idx].Period;
		Ar << GameplayEffects[Idx].Stacks;
		Ar << GameplayEffects[Idx].Level;
	}
}

void FGameplayDebuggerCategory_Abilities::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	UAbilitySystemComponent* AbilityComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DebugActor);
	if (AbilityComp)
	{
		static FGameplayTagContainer OwnerTags;
		OwnerTags.Reset();
		AbilityComp->GetOwnedGameplayTags(OwnerTags);
		DataPack.OwnedTags = OwnerTags.ToStringSimple();

		TArray<FGameplayEffectSpec> ActiveEffectSpecs;
		AbilityComp->GetAllActiveGameplayEffectSpecs(ActiveEffectSpecs);
		for (int32 Idx = 0; Idx < ActiveEffectSpecs.Num(); Idx++)
		{
			const FGameplayEffectSpec& EffectSpec = ActiveEffectSpecs[Idx];
			FRepData::FGameplayEffectDebug ItemData;

			ItemData.Effect = EffectSpec.ToSimpleString();
			ItemData.Effect.RemoveFromStart(DEFAULT_OBJECT_PREFIX);
			ItemData.Effect.RemoveFromEnd(TEXT("_C"));

			ItemData.Context = EffectSpec.GetContext().ToString();
			ItemData.Duration = EffectSpec.GetDuration();
			ItemData.Period = EffectSpec.GetPeriod();
			ItemData.Stacks = EffectSpec.StackCount;
			ItemData.Level = EffectSpec.GetLevel();

			DataPack.GameplayEffects.Add(ItemData);
		}

		const TArray<FGameplayAbilitySpec>& AbilitySpecs = AbilityComp->GetActivatableAbilities();
		for (int32 Idx = 0; Idx < AbilitySpecs.Num(); Idx++)
		{
			const FGameplayAbilitySpec& AbilitySpec = AbilitySpecs[Idx];
			FRepData::FGameplayAbilityDebug ItemData;

			ItemData.Ability = GetNameSafe(AbilitySpec.Ability);
			ItemData.Ability.RemoveFromStart(DEFAULT_OBJECT_PREFIX);
			ItemData.Ability.RemoveFromEnd(TEXT("_C"));

			ItemData.Source = GetNameSafe(AbilitySpec.SourceObject);
			ItemData.Source.RemoveFromStart(DEFAULT_OBJECT_PREFIX);

			ItemData.Level = AbilitySpec.Level;
			ItemData.bIsActive = AbilitySpec.IsActive();

			DataPack.Abilities.Add(ItemData);
		}
	}
}

void FGameplayDebuggerCategory_Abilities::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	CanvasContext.Printf(TEXT("Owned Tags: {yellow}%s"), *DataPack.OwnedTags);

	AActor* LocalDebugActor = FindLocalDebugActor();
	UAbilitySystemComponent* AbilityComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(LocalDebugActor);
	if (AbilityComp)
	{
		static FGameplayTagContainer OwnerTags;
		OwnerTags.Reset();
		AbilityComp->GetOwnedGameplayTags(OwnerTags);

		CanvasContext.Printf(TEXT("Local Tags: {cyan}%s"), *OwnerTags.ToStringSimple());
	}

	CanvasContext.Printf(TEXT("Gameplay Effects: {yellow}%d"), DataPack.GameplayEffects.Num());
	for (int32 Idx = 0; Idx < DataPack.GameplayEffects.Num(); Idx++)
	{
		const FRepData::FGameplayEffectDebug& ItemData = DataPack.GameplayEffects[Idx];

		FString Desc = FString::Printf(TEXT("\t{yellow}%s {grey}source:{white}%s {grey}duration:{white}"), *ItemData.Effect, *ItemData.Context);
		Desc += (ItemData.Duration > 0.0f) ? FString::Printf(TEXT("%.2f"), ItemData.Duration) : FString(TEXT("INF"));

		if (ItemData.Period > 0.0f)
		{
			Desc += FString::Printf(TEXT(" {grey}period:{white}%.2f"), ItemData.Period);
		}

		if (ItemData.Stacks > 1)
		{
			Desc += FString::Printf(TEXT(" {grey}stacks:{white}%d"), ItemData.Stacks);
		}

		if (ItemData.Level > 1.0f)
		{
			Desc += FString::Printf(TEXT(" {grey}level:{white}%.2f"), ItemData.Level);
		}

		CanvasContext.Print(Desc);
	}

	CanvasContext.Printf(TEXT("Gameplay Abilities: {yellow}%d"), DataPack.Abilities.Num());
	for (int32 Idx = 0; Idx < DataPack.Abilities.Num(); Idx++)
	{
		const FRepData::FGameplayAbilityDebug& ItemData = DataPack.Abilities[Idx];

		CanvasContext.Printf(TEXT("\t{yellow}%s {grey}source:{white}%s {grey}level:{white}%d {grey}active:{white}%s"),
			*ItemData.Ability, *ItemData.Source, ItemData.Level, ItemData.bIsActive ? TEXT("YES") : TEXT("no"));
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
