// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

class AActor;
class APlayerController;

class FGameplayDebuggerCategory_Abilities : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Abilities();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	struct FRepData
	{
		FString OwnedTags;

		struct FGameplayAbilityDebug
		{
			FString Ability;
			FString Source;
			int32 Level;
			bool bIsActive;
		};
		TArray<FGameplayAbilityDebug> Abilities;

		struct FGameplayEffectDebug
		{
			FString Effect;
			FString Context;
			float Duration;
			float Period;
			int32 Stacks;
			float Level;
		};
		TArray<FGameplayEffectDebug> GameplayEffects;

		void Serialize(FArchive& Ar);
	};
	FRepData DataPack;
};

#endif // WITH_GAMEPLAY_DEBUGGER
