// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "UObject/ScriptInterface.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GameplayCueInterface.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Abilities/GameplayAbilityTargetDataFilter.h"
#include "AbilitySystemBlueprintLibrary.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

// meta =(RestrictedToClasses="GameplayAbility")
UCLASS()
class GAMEPLAYABILITIES_API UAbilitySystemBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintPure, Category = Ability)
	static UAbilitySystemComponent* GetAbilitySystemComponent(AActor *Actor);

	// NOTE: The Actor passed in must implement IAbilitySystemInterface! or else this function will silently fail to
	// send the event.  The actor needs the interface to find the UAbilitySystemComponent, and if the component isn't
	// found, the event will not be sent.
	UFUNCTION(BlueprintCallable, Category = Ability, Meta = (Tooltip = "This function can be used to trigger an ability on the actor in question with useful payload data."))
	static void SendGameplayEventToActor(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload);
	
	// -------------------------------------------------------------------------------
	//		Attribute
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static bool IsValid(FGameplayAttribute Attribute);

	/** Returns the value of Attribute from the ability system component belonging to Actor. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttribute(const class AActor* Actor, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the value of Attribute from the ability system component AbilitySystem. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttributeFromAbilitySystemComponent(const class UAbilitySystemComponent* AbilitySystem, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the base value of Attribute from the ability system component belonging to Actor. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttributeBase(const class AActor* Actor, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the base value of Attribute from the ability system component AbilitySystemComponent. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttributeBaseFromAbilitySystemComponent(const class UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the value of Attribute from the ability system component AbilitySystem after evaluating it with source and target tags. bSuccess indicates the success or failure of this operation. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float EvaluateAttributeValueWithTags(class UAbilitySystemComponent* AbilitySystem, FGameplayAttribute Attribute, const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags, bool& bSuccess);

	/** Returns the value of Attribute from the ability system component AbilitySystem after evaluating it with source and target tags using the passed in base value instead of the real base value. bSuccess indicates the success or failure of this operation. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float EvaluateAttributeValueWithTagsAndBase(class UAbilitySystemComponent* AbilitySystem, FGameplayAttribute Attribute, const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags, float BaseValue, bool& bSuccess);

	/** Simple equality operator for gameplay attributes */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Gameplay Attribute)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Ability|Attribute")
	static bool EqualEqual_GameplayAttributeGameplayAttribute(FGameplayAttribute AttributeA, FGameplayAttribute AttributeB);

	/** Simple inequality operator for gameplay attributes */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Not Equal (Gameplay Attribute)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Ability|Attribute")
	static bool NotEqual_GameplayAttributeGameplayAttribute(FGameplayAttribute AttributeA, FGameplayAttribute AttributeB);

	// -------------------------------------------------------------------------------
	//		TargetData
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle AppendTargetDataHandle(FGameplayAbilityTargetDataHandle TargetHandle, const FGameplayAbilityTargetDataHandle& HandleToAdd);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromLocations(const FGameplayAbilityTargetingLocationInfo& SourceLocation, const FGameplayAbilityTargetingLocationInfo& TargetLocation);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromHitResult(const FHitResult& HitResult);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static int32 GetDataCountFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromActor(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromActorArray(const TArray<AActor*>& ActorArray, bool OneTargetPerHandle);

	/** Create a new target data handle with filtration performed on the data */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	FilterTargetData(const FGameplayAbilityTargetDataHandle& TargetDataHandle, FGameplayTargetDataFilterHandle ActorFilterClass);

	/** Create a handle for filtering target data, filling out all fields */
	UFUNCTION(BlueprintPure, Category = "Filter")
	static FGameplayTargetDataFilterHandle MakeFilterHandle(FGameplayTargetDataFilter Filter, AActor* FilterActor);

	/** Create a spec handle, filling out all fields */
	UFUNCTION(BlueprintPure, Category = "Spec")
	static FGameplayEffectSpecHandle MakeSpecHandle(UGameplayEffect* InGameplayEffect, AActor* InInstigator, AActor* InEffectCauser, float InLevel = 1.0f);

	/** Create a spec handle, cloning another */
	UFUNCTION(BlueprintPure, Category = "Spec")
	static FGameplayEffectSpecHandle CloneSpecHandle(AActor* InNewInstigator, AActor* InEffectCauser, FGameplayEffectSpecHandle GameplayEffectSpecHandle_Clone);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static TArray<AActor*> GetActorsFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	/** Returns true if the given TargetData has the actor passed in targeted */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool DoesTargetDataContainActor(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index, AActor* Actor);

	/** Returns true if the given TargetData has at least 1 actor targeted */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasActor(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasHitResult(const FGameplayAbilityTargetDataHandle& HitResult, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FHitResult GetHitResultFromTargetData(const FGameplayAbilityTargetDataHandle& HitResult, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasOrigin(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FTransform GetTargetDataOrigin(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasEndPoint(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FVector GetTargetDataEndPoint(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FTransform GetTargetDataEndPointTransform(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index);

	// -------------------------------------------------------------------------------
	//		GameplayEffectContext
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "IsValid"))
	static bool EffectContextIsValid(FGameplayEffectContextHandle EffectContext);

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "IsInstigatorLocallyControlled"))
	static bool EffectContextIsInstigatorLocallyControlled(FGameplayEffectContextHandle EffectContext);

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetHitResult"))
	static FHitResult EffectContextGetHitResult(FGameplayEffectContextHandle EffectContext);

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "HasHitResult"))
	static bool EffectContextHasHitResult(FGameplayEffectContextHandle EffectContext);

	UFUNCTION(BlueprintCallable, Category = "Ability|EffectContext", Meta = (DisplayName = "AddHitResult"))
	static void EffectContextAddHitResult(FGameplayEffectContextHandle EffectContext, FHitResult HitResult, bool bReset);

	/** Gets the location the effect originated from */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetOrigin"))
	static FVector EffectContextGetOrigin(FGameplayEffectContextHandle EffectContext);

	/** Sets the location the effect originated from */
	UFUNCTION(BlueprintCallable, Category = "Ability|EffectContext", Meta = (DisplayName = "SetOrigin"))
	static void EffectContextSetOrigin(FGameplayEffectContextHandle EffectContext, FVector Origin);

	/** Gets the instigating actor (that holds the ability system component) of the EffectContext */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetInstigatorActor"))
	static AActor* EffectContextGetInstigatorActor(FGameplayEffectContextHandle EffectContext);

	/** Gets the original instigator actor that started the chain of events to cause this effect */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetOriginalInstigatorActor"))
	static AActor* EffectContextGetOriginalInstigatorActor(FGameplayEffectContextHandle EffectContext);

	/** Gets the physical actor that caused the effect, possibly a projectile or weapon */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetEffectCauser"))
	static AActor* EffectContextGetEffectCauser(FGameplayEffectContextHandle EffectContext);

	/** Gets the source object of the effect. */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetSourceObject"))
	static UObject* EffectContextGetSourceObject(FGameplayEffectContextHandle EffectContext);

	// -------------------------------------------------------------------------------
	//		GameplayCue
	// -------------------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category="Ability|GameplayCue")
	static bool IsInstigatorLocallyControlled(FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintPure, Category="Ability|GameplayCue")
	static bool IsInstigatorLocallyControlledPlayer(FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static int32 GetActorCount(FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static AActor* GetActorByIndex(FGameplayCueParameters Parameters, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static FHitResult GetHitResult(FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool HasHitResult(FGameplayCueParameters Parameters);

	/** Forwards the gameplay cue to another gameplay cue interface object */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayCue")
	static void ForwardGameplayCueToTarget(TScriptInterface<IGameplayCueInterface> TargetCueInterface, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Gets the instigating actor (that holds the ability system component) of the GameplayCue */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static AActor* GetInstigatorActor(FGameplayCueParameters Parameters);

	/** Gets instigating world location */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static FTransform GetInstigatorTransform(FGameplayCueParameters Parameters);

	/** Gets instigating world location */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static FVector GetOrigin(FGameplayCueParameters Parameters);

	/** Gets the best end location and normal for this gameplay cue. If there is hit result data, it will return this. Otherwise it will return the target actor's location/rotation. If none of this is available, it will return false. */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool GetGameplayCueEndLocationAndNormal(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Location, FVector& Normal);

	/** Gets the best normalized effect direction for this gameplay cue. This is useful for effects that require the direction of an enemy attack. Returns true if a valid direction could be calculated. */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool GetGameplayCueDirection(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Direction);

	/** Returns true if the aggregated source and target tags from the effect spec meets the tag requirements */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool DoesGameplayCueMeetTagRequirements(FGameplayCueParameters Parameters, UPARAM(ref) FGameplayTagRequirements& SourceTagReqs, UPARAM(ref) FGameplayTagRequirements& TargetTagReqs);

	// -------------------------------------------------------------------------------
	//		GameplayEffectSpec
	// -------------------------------------------------------------------------------
	
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AssignSetByCallerMagnitude(FGameplayEffectSpecHandle SpecHandle, FName DataName, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AssignTagSetByCallerMagnitude(FGameplayEffectSpecHandle SpecHandle, FGameplayTag DataTag, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle SetDuration(FGameplayEffectSpecHandle SpecHandle, float Duration);

	// This instance of the effect will now grant NewGameplayTag to the object that this effect is applied to.
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddGrantedTag(FGameplayEffectSpecHandle SpecHandle, FGameplayTag NewGameplayTag);

	// This instance of the effect will now grant NewGameplayTags to the object that this effect is applied to.
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddGrantedTags(FGameplayEffectSpecHandle SpecHandle, FGameplayTagContainer NewGameplayTags);

	// Adds NewGameplayTag to this instance of the effect.
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddAssetTag(FGameplayEffectSpecHandle SpecHandle, FGameplayTag NewGameplayTag);

	// Adds NewGameplayTags to this instance of the effect.
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddAssetTags(FGameplayEffectSpecHandle SpecHandle, FGameplayTagContainer NewGameplayTags);

	/** Adds LinkedGameplayEffectSpec to SpecHandles. LinkedGameplayEffectSpec will be applied when/if SpecHandle is applied successfully. LinkedGameplayEffectSpec will not be modified here. Returns the ORIGINAL SpecHandle (legacy decision) */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddLinkedGameplayEffectSpec(FGameplayEffectSpecHandle SpecHandle, FGameplayEffectSpecHandle LinkedGameplayEffectSpec);

	/** Adds LinkedGameplayEffect to SpecHandles. LinkedGameplayEffectSpec will be applied when/if SpecHandle is applied successfully. This will initialize the LinkedGameplayEffect's Spec for you. Returns to NEW linked spec in case you want to add more to it. */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddLinkedGameplayEffect(FGameplayEffectSpecHandle SpecHandle, TSubclassOf<UGameplayEffect> LinkedGameplayEffect);

	/** Sets the GameplayEffectSpec's StackCount to the specified amount (prior to applying) */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle SetStackCount(FGameplayEffectSpecHandle SpecHandle, int32 StackCount);

	/** Sets the GameplayEffectSpec's StackCount to the max stack count defined in the GameplayEffect definition */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle SetStackCountToMax(FGameplayEffectSpecHandle SpecHandle);

	/** Gets the GameplayEffectSpec's effect context handle */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectContextHandle GetEffectContext(FGameplayEffectSpecHandle SpecHandle);

	/** Returns handles for all Linked GE Specs that SpecHandle may apply. Useful if you want to append additional information to them. */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect")
	static TArray<FGameplayEffectSpecHandle> GetAllLinkedGameplayEffectSpecHandles(FGameplayEffectSpecHandle SpecHandle);

	// -------------------------------------------------------------------------------
	//		GameplayEffectSpec
	// -------------------------------------------------------------------------------

	/** Gets the magnitude of change for an attribute on an APPLIED GameplayEffectSpec. */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static float GetModifiedAttributeMagnitude(FGameplayEffectSpecHandle SpecHandle, FGameplayAttribute Attribute);

	/** Helper function that may be useful to call from native as well */
	static float GetModifiedAttributeMagnitude(const FGameplayEffectSpec& SpecHandle, FGameplayAttribute Attribute);

	// -------------------------------------------------------------------------------
	//		FActiveGameplayEffectHandle
	// -------------------------------------------------------------------------------
	
	/** Returns current stack count of an active Gameplay Effect. Will return 0 if the GameplayEffect is no longer valid. */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static int32 GetActiveGameplayEffectStackCount(FActiveGameplayEffectHandle ActiveHandle);

	/** Returns stack limit count of an active Gameplay Effect. Will return 0 if the GameplayEffect is no longer valid. */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static int32 GetActiveGameplayEffectStackLimitCount(FActiveGameplayEffectHandle ActiveHandle);

	/** Returns the start time (time which the GE was added) for a given GameplayEffect */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static float GetActiveGameplayEffectStartTime(FActiveGameplayEffectHandle ActiveHandle);

	/** Returns the expected end time (when we think the GE will expire) for a given GameplayEffect (note someone could remove or change it before that happens!) */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static float GetActiveGameplayEffectExpectedEndTime(FActiveGameplayEffectHandle ActiveHandle);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static float GetActiveGameplayEffectTotalDuration(FActiveGameplayEffectHandle ActiveHandle);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect", meta = (WorldContext = "WorldContextObject"))
	static float GetActiveGameplayEffectRemainingDuration(UObject* WorldContextObject, FActiveGameplayEffectHandle ActiveHandle);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect", Meta = (DisplayName = "Get Active GameplayEffect Debug String "))
	static FString GetActiveGameplayEffectDebugString(FActiveGameplayEffectHandle ActiveHandle);

};
