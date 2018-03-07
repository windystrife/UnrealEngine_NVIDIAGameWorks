// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectAggregator.h"
#include "GameplayEffect.h"
#include "GameplayEffectCalculation.h"
#include "GameplayEffectExecutionCalculation.generated.h"

class UAbilitySystemComponent;

/** Struct representing parameters for a custom gameplay effect execution. Should not be held onto via reference, used just for the scope of the execution */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEffectCustomExecutionParameters
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructors
	FGameplayEffectCustomExecutionParameters();
	FGameplayEffectCustomExecutionParameters(FGameplayEffectSpec& InOwningSpec, const TArray<FGameplayEffectExecutionScopedModifierInfo>& InScopedMods, UAbilitySystemComponent* InTargetAbilityComponent, const FGameplayTagContainer& InPassedIntags, const FPredictionKey& InPredictionKey);
	FGameplayEffectCustomExecutionParameters(FGameplayEffectSpec& InOwningSpec, const TArray<FGameplayEffectExecutionScopedModifierInfo>& InScopedMods, UAbilitySystemComponent* InTargetAbilityComponent, const FGameplayTagContainer& InPassedIntags, const FPredictionKey& InPredictionKey, const TArray<FActiveGameplayEffectHandle>& InIgnoreHandles);

	/** Simple accessor to owning gameplay spec */
	const FGameplayEffectSpec& GetOwningSpec() const;

	/** Non const access. Be careful with this, especially when modifying a spec after attribute capture. */
	FGameplayEffectSpec* GetOwningSpecForPreExecuteMod() const;

	/** Simple accessor to target ability system component */
	UAbilitySystemComponent* GetTargetAbilitySystemComponent() const;

	/** Simple accessor to source ability system component (could be null!) */
	UAbilitySystemComponent* GetSourceAbilitySystemComponent() const;
	
	/** Simple accessor to the Passed In Tags to this execution */
	const FGameplayTagContainer& GetPassedInTags() const;

	TArray<FActiveGameplayEffectHandle> GetIgnoreHandles() const;
	
	FPredictionKey GetPredictionKey() const;

	/**
	 * Attempts to calculate the magnitude of a captured attribute given the specified parameters. Can fail if the gameplay spec doesn't have
	 * a valid capture for the attribute.
	 * 
	 * @param InCaptureDef	Attribute definition to attempt to calculate the magnitude of
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateCapturedAttributeMagnitude(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutMagnitude) const;
	
	/**
	 * Attempts to calculate the magnitude of a captured attribute given the specified parameters, including a starting base value. 
	 * Can fail if the gameplay spec doesn't have a valid capture for the attribute.
	 * 
	 * @param InCaptureDef	Attribute definition to attempt to calculate the magnitude of
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param InBaseValue	Base value to evaluate the attribute under
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateCapturedAttributeMagnitudeWithBase(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, float InBaseValue, OUT float& OutMagnitude) const;

	/**
	 * Attempts to calculate the base value of a captured attribute given the specified parameters. Can fail if the gameplay spec doesn't have
	 * a valid capture for the attribute.
	 * 
	 * @param InCaptureDef	Attribute definition to attempt to calculate the base value of
	 * @param OutBaseValue	[OUT] Computed base value
	 * 
	 * @return True if the base value was successfully calculated, false if it was not
	 */
	bool AttemptCalculateCapturedAttributeBaseValue(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, OUT float& OutBaseValue) const;

	/**
	 * Attempts to calculate the bonus magnitude of a captured attribute given the specified parameters. Can fail if the gameplay spec doesn't have
	 * a valid capture for the attribute.
	 * 
	 * @param InCaptureDef		Attribute definition to attempt to calculate the bonus magnitude of
	 * @param InEvalParams		Parameters to evaluate the attribute under
	 * @param OutBonusMagnitude	[OUT] Computed bonus magnitude
	 * 
	 * @return True if the bonus magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateCapturedAttributeBonusMagnitude(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutBonusMagnitude) const;
	
	/**
	 * Attempts to populate the specified aggregator with a snapshot of a backing captured aggregator. Can fail if the gameplay spec doesn't have
	 * a valid capture for the attribute.
	 * 
	 * @param InCaptureDef				Attribute definition to attempt to snapshot
	 * @param OutSnapshottedAggregator	[OUT] Snapshotted aggregator, if possible
	 * 
	 * @return True if the aggregator was successfully snapshotted, false if it was not
	 */
	bool AttemptGetCapturedAttributeAggregatorSnapshot(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, OUT FAggregator& OutSnapshottedAggregator) const;

	/** 
	 *	Returns all modifiers for a given captured def. Note the returned list is a direct reference to the internal list stored in the attribute aggregators (possibly a snapshot copy or possibly the "real" ones).
	 *	Also note the modifiers returned or NOT qualified! You will still want to qualifiy them on an FAggregatorEvaluateParameters yourself. (Having this function internally do it would onyl be possible if this function
	 *	returned a copy of the mod list which is something we would like to avoid).
	 *	
	 *	Consider using ForEachQualifiedAttributeMod when you want to "do something for every qualifier mod"
	 */
	bool AttemptGatherAttributeMods(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, OUT TMap<EGameplayModEvaluationChannel, const TArray<FAggregatorMod>*>& OutModMap) const;

	/** Runs given TFunction on every qualifier mod for a given AttributeCaptureDefinition */
	bool ForEachQualifiedAttributeMod(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, TFunction< void(EGameplayModEvaluationChannel, EGameplayModOp::Type, const FAggregatorMod&) >) const;

private:

	/** Mapping of capture definition to aggregator with scoped modifiers added in; Used to process scoped modifiers w/o modifying underlying aggregators in the capture */
	TMap<FGameplayEffectAttributeCaptureDefinition, FAggregator> ScopedModifierAggregators;

	/** Owning gameplay effect spec */
	FGameplayEffectSpec* OwningSpec;

	/** Target ability system component of the execution */
	TWeakObjectPtr<UAbilitySystemComponent> TargetAbilitySystemComponent;

	/** The extra tags that were passed in to this execution */
	FGameplayTagContainer PassedInTags;

	TArray<FActiveGameplayEffectHandle> IgnoreHandles;
	
	FPredictionKey PredictionKey;
};

/** Struct representing the output of a custom gameplay effect execution. */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEffectCustomExecutionOutput
{
	GENERATED_USTRUCT_BODY()

public:

	/** Constructor */
	FGameplayEffectCustomExecutionOutput();

	/** Mark that the execution has manually handled the stack count and the GE system should not attempt to automatically act upon it for emitted modifiers */
	void MarkStackCountHandledManually();

	/** Simple accessor for determining whether the execution has manually handled the stack count or not */
	bool IsStackCountHandledManually() const;

	/** Accessor for determining if GameplayCue events have already been handled */
	bool AreGameplayCuesHandledManually() const;

	/** Mark that the execution wants conditional gameplay effects to trigger */
	void MarkConditionalGameplayEffectsToTrigger();

	/** Mark that the execution wants conditional gameplay effects to trigger */
	void MarkGameplayCuesHandledManually();

	/** Simple accessor for determining whether the execution wants conditional gameplay effects to trigger or not */
	bool ShouldTriggerConditionalGameplayEffects() const;

	/** Add the specified evaluated data to the execution's output modifiers */
	void AddOutputModifier(const FGameplayModifierEvaluatedData& InOutputMod);

	/** Simple accessor to output modifiers of the execution */
	const TArray<FGameplayModifierEvaluatedData>& GetOutputModifiers() const;

	/** Simple accessor to output modifiers of the execution */
	void GetOutputModifiers(OUT TArray<FGameplayModifierEvaluatedData>& OutOutputModifiers) const;

	/** Returns direct access to output modifiers of the execution (avoid copy) */
	TArray<FGameplayModifierEvaluatedData>& GetOutputModifiersRef() { return OutputModifiers; }

private:

	/** Modifiers emitted by the execution */
	UPROPERTY()
	TArray<FGameplayModifierEvaluatedData> OutputModifiers;

	/** If true, the execution wants to trigger conditional gameplay effects when it completes */
	UPROPERTY()
	uint32 bTriggerConditionalGameplayEffects : 1;
	
	/** If true, the execution itself has manually handled the stack count of the effect and the GE system doesn't have to automatically handle it */
	UPROPERTY()
	uint32 bHandledStackCountManually : 1;

	/** If true, the execution itself has manually invoked all gameplay cues and the GE system doesn't have to automatically handle them. */
	UPROPERTY()
	uint32 bHandledGameplayCuesManually : 1;
};

UCLASS(BlueprintType, Blueprintable, Abstract)
class GAMEPLAYABILITIES_API UGameplayEffectExecutionCalculation : public UGameplayEffectCalculation
{
	GENERATED_UCLASS_BODY()

protected:

	/** Used to indicate if this execution uses Passed In Tags */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Attributes)
	bool bRequiresPassedInTags;

#if WITH_EDITORONLY_DATA

protected:

	/** Any attribute in this list will not show up as a valid option for scoped modifiers; Used to allow attribute capture for internal calculation while preventing modification */
	UPROPERTY(EditDefaultsOnly, Category=Attributes)
	TArray<FGameplayEffectAttributeCaptureDefinition> InvalidScopedModifierAttributes;

public:
	/**
	 * Gets the collection of capture attribute definitions that the calculation class will accept as valid scoped modifiers
	 * 
	 * @param OutScopableModifiers	[OUT] Array to populate with definitions valid as scoped modifiers
	 */
	virtual void GetValidScopedModifierAttributeCaptureDefinitions(OUT TArray<FGameplayEffectAttributeCaptureDefinition>& OutScopableModifiers) const;

	/** Returns if this execution requires passed in tags */
	virtual bool DoesRequirePassedInTags() const;

#endif // #if WITH_EDITORONLY_DATA

public:

	/**
	 * Called whenever the owning gameplay effect is executed. Allowed to do essentially whatever is desired, including generating new
	 * modifiers to instantly execute as well.
	 * 
	 * @note: Native subclasses should override the auto-generated Execute_Implementation function and NOT this one.
	 * 
	 * @param ExecutionParams		Parameters for the custom execution calculation
	 * @param OutExecutionOutput	[OUT] Output data populated by the execution detailing further behavior or results of the execution
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Calculation")
	void Execute(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const;
};



// -------------------------------------------------------------------------
//	Helper macros for declaring attribute captures 
// -------------------------------------------------------------------------

#define DECLARE_ATTRIBUTE_CAPTUREDEF(P) \
	UProperty* P##Property; \
	FGameplayEffectAttributeCaptureDefinition P##Def; \

#define DEFINE_ATTRIBUTE_CAPTUREDEF(S, P, T, B) \
{ \
	P##Property = FindFieldChecked<UProperty>(S::StaticClass(), GET_MEMBER_NAME_CHECKED(S, P)); \
	P##Def = FGameplayEffectAttributeCaptureDefinition(P##Property, EGameplayEffectAttributeCaptureSource::T, B); \
}
