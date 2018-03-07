// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/CurveTable.h"
#include "AttributeSet.h"
#include "EngineDefines.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectAggregator.h"
#include "GameplayPrediction.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayAbilitySpec.h"
#include "ActiveGameplayEffectIterator.h"
#include "UObject/ObjectKey.h"
#include "GameplayEffect.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UGameplayEffectCustomApplicationRequirement;
class UGameplayEffectExecutionCalculation;
class UGameplayEffectTemplate;
class UGameplayModMagnitudeCalculation;
struct FActiveGameplayEffectsContainer;
struct FGameplayEffectModCallbackData;
struct FGameplayEffectSpec;

/** Enumeration outlining the possible gameplay effect magnitude calculation policies. */
UENUM()
enum class EGameplayEffectMagnitudeCalculation : uint8
{
	/** Use a simple, scalable float for the calculation. */
	ScalableFloat,
	/** Perform a calculation based upon an attribute. */
	AttributeBased,
	/** Perform a custom calculation, capable of capturing and acting on multiple attributes, in either BP or native. */
	CustomCalculationClass,	
	/** This magnitude will be set explicitly by the code/blueprint that creates the spec. */
	SetByCaller,
};

/** Enumeration outlining the possible attribute based float calculation policies. */
UENUM()
enum class EAttributeBasedFloatCalculationType : uint8
{
	/** Use the final evaluated magnitude of the attribute. */
	AttributeMagnitude,
	/** Use the base value of the attribute. */
	AttributeBaseValue,
	/** Use the "bonus" evaluated magnitude of the attribute: Equivalent to (FinalMag - BaseValue). */
	AttributeBonusMagnitude,
	/** Use a calculated magnitude stopping with the evaluation of the specified "Final Channel" */
	AttributeMagnitudeEvaluatedUpToChannel
};

struct GAMEPLAYABILITIES_API FGameplayEffectConstants
{
	/** Infinite duration */
	static const float INFINITE_DURATION;

	/** No duration; Time specifying instant application of an effect */
	static const float INSTANT_APPLICATION;

	/** Constant specifying that the combat effect has no period and doesn't check for over time application */
	static const float NO_PERIOD;

	/** No Level/Level not set */
	static const float INVALID_LEVEL;
};

/** 
 * Struct representing a float whose magnitude is dictated by a backing attribute and a calculation policy, follows basic form of:
 * (Coefficient * (PreMultiplyAdditiveValue + [Eval'd Attribute Value According to Policy])) + PostMultiplyAdditiveValue
 */
USTRUCT()
struct FAttributeBasedFloat
{
	GENERATED_USTRUCT_BODY()

public:

	/** Constructor */
	FAttributeBasedFloat()
		: Coefficient(1.f)
		, PreMultiplyAdditiveValue(0.f)
		, PostMultiplyAdditiveValue(0.f)
		, BackingAttribute()
		, AttributeCalculationType(EAttributeBasedFloatCalculationType::AttributeMagnitude)
		, FinalChannel(EGameplayModEvaluationChannel::Channel0)
	{}

	/**
	 * Calculate and return the magnitude of the float given the specified gameplay effect spec.
	 * 
	 * @note:	This function assumes (and asserts on) the existence of the required captured attribute within the spec.
	 *			It is the responsibility of the caller to verify that the spec is properly setup before calling this function.
	 *			
	 *	@param InRelevantSpec	Gameplay effect spec providing the backing attribute capture
	 *	
	 *	@return Evaluated magnitude based upon the spec & calculation policy
	 */
	float CalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec) const;

	/** Coefficient to the attribute calculation */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FScalableFloat Coefficient;

	/** Additive value to the attribute calculation, added in before the coefficient applies */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FScalableFloat PreMultiplyAdditiveValue;

	/** Additive value to the attribute calculation, added in after the coefficient applies */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FScalableFloat PostMultiplyAdditiveValue;

	/** Attribute backing the calculation */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FGameplayEffectAttributeCaptureDefinition BackingAttribute;

	/** If a curve table entry is specified, the attribute will be used as a lookup into the curve instead of using the attribute directly. */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FCurveTableRowHandle AttributeCurve;

	/** Calculation policy in regards to the attribute */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	EAttributeBasedFloatCalculationType AttributeCalculationType;

	/** Channel to terminate evaluation on when using AttributeEvaluatedUpToChannel calculation type */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	EGameplayModEvaluationChannel FinalChannel;

	/** Filter to use on source tags; If specified, only modifiers applied with all of these tags will factor into the calculation */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FGameplayTagContainer SourceTagFilter;

	/** Filter to use on target tags; If specified, only modifiers applied with all of these tags will factor into the calculation */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FGameplayTagContainer TargetTagFilter;

	/** Equality/Inequality operators */
	bool operator==(const FAttributeBasedFloat& Other) const;
	bool operator!=(const FAttributeBasedFloat& Other) const;
};

/** Structure to encapsulate magnitudes that are calculated via custom calculation */
USTRUCT()
struct FCustomCalculationBasedFloat
{
	GENERATED_USTRUCT_BODY()

	FCustomCalculationBasedFloat()
		: CalculationClassMagnitude(nullptr)
		, Coefficient(1.f)
		, PreMultiplyAdditiveValue(0.f)
		, PostMultiplyAdditiveValue(0.f)
	{}

public:

	/**
	 * Calculate and return the magnitude of the float given the specified gameplay effect spec.
	 * 
	 * @note:	This function assumes (and asserts on) the existence of the required captured attribute within the spec.
	 *			It is the responsibility of the caller to verify that the spec is properly setup before calling this function.
	 *			
	 *	@param InRelevantSpec	Gameplay effect spec providing the backing attribute capture
	 *	
	 *	@return Evaluated magnitude based upon the spec & calculation policy
	 */
	float CalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec) const;

	UPROPERTY(EditDefaultsOnly, Category=CustomCalculation, DisplayName="Calculation Class")
	TSubclassOf<UGameplayModMagnitudeCalculation> CalculationClassMagnitude;

	/** Coefficient to the custom calculation */
	UPROPERTY(EditDefaultsOnly, Category=CustomCalculation)
	FScalableFloat Coefficient;

	/** Additive value to the attribute calculation, added in before the coefficient applies */
	UPROPERTY(EditDefaultsOnly, Category=CustomCalculation)
	FScalableFloat PreMultiplyAdditiveValue;

	/** Additive value to the attribute calculation, added in after the coefficient applies */
	UPROPERTY(EditDefaultsOnly, Category=CustomCalculation)
	FScalableFloat PostMultiplyAdditiveValue;

	/** If a curve table entry is specified, the OUTPUT of this custom class magnitude (including the pre and post additive values) lookup into the curve instead of using the attribute directly. */
	UPROPERTY(EditDefaultsOnly, Category=CustomCalculation)
	FCurveTableRowHandle FinalLookupCurve;

	/** Equality/Inequality operators */
	bool operator==(const FCustomCalculationBasedFloat& Other) const;
	bool operator!=(const FCustomCalculationBasedFloat& Other) const;
};

/** Struct for holding SetBytCaller data */
USTRUCT()
struct FSetByCallerFloat
{
	GENERATED_USTRUCT_BODY()

	FSetByCallerFloat()
	: DataName(NAME_None)
	{}

	/** The Name the caller (code or blueprint) will use to set this magnitude by. */
	UPROPERTY(VisibleDefaultsOnly, Category=SetByCaller)
	FName	DataName;

	UPROPERTY(EditDefaultsOnly, Category = SetByCaller, meta = (Categories = "SetByCaller"))
	FGameplayTag DataTag;

	/** Equality/Inequality operators */
	bool operator==(const FSetByCallerFloat& Other) const;
	bool operator!=(const FSetByCallerFloat& Other) const;
};

/** Struct representing the magnitude of a gameplay effect modifier, potentially calculated in numerous different ways */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayEffectModifierMagnitude
{
	GENERATED_USTRUCT_BODY()

public:

	/** Default Constructor */
	FGameplayEffectModifierMagnitude()
		: MagnitudeCalculationType(EGameplayEffectMagnitudeCalculation::ScalableFloat)
	{
	}

	/** Constructors for setting value in code (for automation tests) */
	FGameplayEffectModifierMagnitude(const FScalableFloat& Value)
		: MagnitudeCalculationType(EGameplayEffectMagnitudeCalculation::ScalableFloat)
		, ScalableFloatMagnitude(Value)
	{
	}
	FGameplayEffectModifierMagnitude(const FAttributeBasedFloat& Value)
		: MagnitudeCalculationType(EGameplayEffectMagnitudeCalculation::AttributeBased)
		, AttributeBasedMagnitude(Value)
	{
	}
	FGameplayEffectModifierMagnitude(const FCustomCalculationBasedFloat& Value)
		: MagnitudeCalculationType(EGameplayEffectMagnitudeCalculation::CustomCalculationClass)
		, CustomMagnitude(Value)
	{
	}
	FGameplayEffectModifierMagnitude(const FSetByCallerFloat& Value)
		: MagnitudeCalculationType(EGameplayEffectMagnitudeCalculation::SetByCaller)
		, SetByCallerMagnitude(Value)
	{
	}
 
	/**
	 * Determines if the magnitude can be properly calculated with the specified gameplay effect spec (could fail if relying on an attribute not present, etc.)
	 * 
	 * @param InRelevantSpec	Gameplay effect spec to check for magnitude calculation
	 * 
	 * @return Whether or not the magnitude can be properly calculated
	 */
	bool CanCalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec) const;

	/**
	 * Attempts to calculate the magnitude given the provided spec. May fail if necessary information (such as captured attributes) is missing from
	 * the spec.
	 * 
	 * @param InRelevantSpec			Gameplay effect spec to use to calculate the magnitude with
	 * @param OutCalculatedMagnitude	[OUT] Calculated value of the magnitude, will be set to 0.f in the event of failure
	 * 
	 * @return True if the calculation was successful, false if it was not
	 */
	bool AttemptCalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, bool WarnIfSetByCallerFail=true, float DefaultSetbyCaller=0.f) const;

	/** Attempts to recalculate the magnitude given a changed aggregator. This will only recalculate if we are a modifier that is linked (non snapshot) to the given aggregator. */
	bool AttemptRecalculateMagnitudeFromDependentAggregatorChange(const FGameplayEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, const FAggregator* ChangedAggregator) const;

	/**
	 * Gather all of the attribute capture definitions necessary to compute the magnitude and place them into the provided array
	 * 
	 * @param OutCaptureDefs	[OUT] Array populated with necessary attribute capture definitions
	 */
	void GetAttributeCaptureDefinitions(OUT TArray<FGameplayEffectAttributeCaptureDefinition>& OutCaptureDefs) const;

	EGameplayEffectMagnitudeCalculation GetMagnitudeCalculationType() const { return MagnitudeCalculationType; }

	/** Returns the magnitude as it was entered in data. Only applies to ScalableFloat or any other type that can return data without context */
	bool GetStaticMagnitudeIfPossible(float InLevel, float& OutMagnitude, const FString* ContextString = nullptr) const;

	/** Returns the DataName associated with this magnitude if it is set by caller */
	bool GetSetByCallerDataNameIfPossible(FName& OutDataName) const;

	/** Returns the custom magnitude calculation class, if any, for this magnitude. Only applies to CustomMagnitudes */
	TSubclassOf<UGameplayModMagnitudeCalculation> GetCustomMagnitudeCalculationClass() const;

	bool operator==(const FGameplayEffectModifierMagnitude& Other) const;
	bool operator!=(const FGameplayEffectModifierMagnitude& Other) const;

#if WITH_EDITOR
	FText GetValueForEditorDisplay() const;
	void ReportErrors(const FString& PathName) const;
#endif

protected:

	/** Type of calculation to perform to derive the magnitude */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	EGameplayEffectMagnitudeCalculation MagnitudeCalculationType;

	/** Magnitude value represented by a scalable float */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	FScalableFloat ScalableFloatMagnitude;

	/** Magnitude value represented by an attribute-based float
	(Coefficient * (PreMultiplyAdditiveValue + [Eval'd Attribute Value According to Policy])) + PostMultiplyAdditiveValue */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	FAttributeBasedFloat AttributeBasedMagnitude;

	/** Magnitude value represented by a custom calculation class */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	FCustomCalculationBasedFloat CustomMagnitude;

	/** Magnitude value represented by a SetByCaller magnitude */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	FSetByCallerFloat SetByCallerMagnitude;

	// @hack: @todo: This is temporary to aid in post-load fix-up w/o exposing members publicly
	friend class UGameplayEffect;
	friend class FGameplayEffectModifierMagnitudeDetails;
};

/** 
 * Struct representing modifier info used exclusively for "scoped" executions that happen instantaneously. These are
 * folded into a calculation only for the extent of the calculation and never permanently added to an aggregator.
 */
USTRUCT(BlueprintType)
struct FGameplayEffectExecutionScopedModifierInfo
{
	GENERATED_USTRUCT_BODY()

	// Constructors
	FGameplayEffectExecutionScopedModifierInfo()
		: ModifierOp(EGameplayModOp::Additive)
	{}

	FGameplayEffectExecutionScopedModifierInfo(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef)
		: CapturedAttribute(InCaptureDef)
		, ModifierOp(EGameplayModOp::Additive)
	{
	}

	/** Backing attribute that the scoped modifier is for */
	UPROPERTY(VisibleDefaultsOnly, Category=Execution)
	FGameplayEffectAttributeCaptureDefinition CapturedAttribute;

	/** Modifier operation to perform */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	TEnumAsByte<EGameplayModOp::Type> ModifierOp;

	/** Magnitude of the scoped modifier */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	FGameplayEffectModifierMagnitude ModifierMagnitude;

	/** Evaluation channel settings of the scoped modifier */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	FGameplayModEvaluationChannelSettings EvaluationChannelSettings;

	/** Source tag requirements for the modifier to apply */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	FGameplayTagRequirements SourceTags;

	/** Target tag requirements for the modifier to apply */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	FGameplayTagRequirements TargetTags;
};

/**
 * Struct for gameplay effects that apply only if another gameplay effect (or execution) was successfully applied.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FConditionalGameplayEffect
{
	GENERATED_USTRUCT_BODY()

	bool CanApply(const FGameplayTagContainer& SourceTags, float SourceLevel) const;

	FGameplayEffectSpecHandle CreateSpec(FGameplayEffectContextHandle EffectContext, float SourceLevel) const;

	/** gameplay effect that will be applied to the target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayEffect)
	TSubclassOf<UGameplayEffect> EffectClass;

	/** Tags that the source must have for this GE to apply */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayEffect)
	FGameplayTagContainer RequiredSourceTags;
};

/** 
 * Struct representing the definition of a custom execution for a gameplay effect.
 * Custom executions run special logic from an outside class each time the gameplay effect executes.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEffectExecutionDefinition
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Gathers and populates the specified array with the capture definitions that the execution would like in order
	 * to perform its custom calculation. Up to the individual execution calculation to handle if some of them are missing
	 * or not.
	 * 
	 * @param OutCaptureDefs	[OUT] Capture definitions requested by the execution
	 */
	void GetAttributeCaptureDefinitions(OUT TArray<FGameplayEffectAttributeCaptureDefinition>& OutCaptureDefs) const;

	/** Custom execution calculation class to run when the gameplay effect executes */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	TSubclassOf<UGameplayEffectExecutionCalculation> CalculationClass;
	
	/** These tags are passed into the execution as is, and may be used to do conditional logic */
	UPROPERTY(EditDefaultsOnly, Category = Execution)
	FGameplayTagContainer PassedInTags;

	/** Modifiers that are applied "in place" during the execution calculation */
	UPROPERTY(EditDefaultsOnly, Category = Execution)
	TArray<FGameplayEffectExecutionScopedModifierInfo> CalculationModifiers;

	/** Deprecated. */
	UPROPERTY()
	TArray<TSubclassOf<UGameplayEffect>> ConditionalGameplayEffectClasses_DEPRECATED;

	/** Other Gameplay Effects that will be applied to the target of this execution if the execution is successful. Note if no execution class is selected, these will always apply. */
	UPROPERTY(EditDefaultsOnly, Category = Execution)
	TArray<FConditionalGameplayEffect> ConditionalGameplayEffects;
};

/**
 * FGameplayModifierInfo
 *	Tells us "Who/What we" modify
 *	Does not tell us how exactly
 *
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayModifierInfo
{
	GENERATED_USTRUCT_BODY()

	FGameplayModifierInfo()	
	: ModifierOp(EGameplayModOp::Additive)
	{

	}

	/** The Attribute we modify or the GE we modify modifies. */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier, meta=(FilterMetaTag="HideFromModifiers"))
	FGameplayAttribute Attribute;

	/** The numeric operation of this modifier: Override, Add, Multiply, etc  */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	TEnumAsByte<EGameplayModOp::Type> ModifierOp;

	// @todo: Remove this after content resave
	/** Now "deprecated," though being handled in a custom manner to avoid engine version bump. */
	UPROPERTY()
	FScalableFloat Magnitude;

	/** Magnitude of the modifier */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	FGameplayEffectModifierMagnitude ModifierMagnitude;

	/** Evaluation channel settings of the modifier */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	FGameplayModEvaluationChannelSettings EvaluationChannelSettings;

	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	FGameplayTagRequirements	SourceTags;

	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	FGameplayTagRequirements	TargetTags;

	/** Equality/Inequality operators */
	bool operator==(const FGameplayModifierInfo& Other) const;
	bool operator!=(const FGameplayModifierInfo& Other) const;
};

/**
 * FGameplayEffectCue
 *	This is a cosmetic cue that can be tied to a UGameplayEffect. 
 *  This is essentially a GameplayTag + a Min/Max level range that is used to map the level of a GameplayEffect to a normalized value used by the GameplayCue system.
 */
USTRUCT(BlueprintType)
struct FGameplayEffectCue
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectCue()
		: MinLevel(0.f)
		, MaxLevel(0.f)
	{
	}

	FGameplayEffectCue(const FGameplayTag& InTag, float InMinLevel, float InMaxLevel)
		: MinLevel(InMinLevel)
		, MaxLevel(InMaxLevel)
	{
		GameplayCueTags.AddTag(InTag);
	}

	/** The attribute to use as the source for cue magnitude. If none use level */
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	FGameplayAttribute MagnitudeAttribute;

	/** The minimum level that this Cue supports */
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	float	MinLevel;

	/** The maximum level that this Cue supports */
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	float	MaxLevel;

	/** Tags passed to the gameplay cue handler when this cue is activated */
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue, meta = (Categories="GameplayCue"))
	FGameplayTagContainer GameplayCueTags;

	float NormalizeLevel(float InLevel)
	{
		float Range = MaxLevel - MinLevel;
		if (Range <= KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}

		return FMath::Clamp((InLevel - MinLevel) / Range, 0.f, 1.0f);
	}
};

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FInheritedTagContainer
{
	GENERATED_USTRUCT_BODY()

	/** Tags that I inherited and tags that I added minus tags that I removed*/
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = Application)
	FGameplayTagContainer CombinedTags;

	/** Tags that I have in addition to my parent's tags */
	UPROPERTY(EditDefaultsOnly, Transient, BlueprintReadOnly, Category = Application)
	FGameplayTagContainer Added;

	/** Tags that should be removed if my parent had them */
	UPROPERTY(EditDefaultsOnly, Transient, BlueprintReadOnly, Category = Application)
	FGameplayTagContainer Removed;

	void UpdateInheritedTagProperties(const FInheritedTagContainer* Parent);

	void PostInitProperties();

	void AddTag(const FGameplayTag& TagToAdd);
	void RemoveTag(FGameplayTag TagToRemove);
};

/** Gameplay effect duration policies */
UENUM()
enum class EGameplayEffectDurationType : uint8
{
	/** This effect applies instantly */
	Instant,
	/** This effect lasts forever */
	Infinite,
	/** The duration of this effect will be specified by a magnitude */
	HasDuration
};

/** Enumeration of policies for dealing with duration of a gameplay effect while stacking */
UENUM()
enum class EGameplayEffectStackingDurationPolicy : uint8
{
	/** The duration of the effect will be refreshed from any successful stack application */
	RefreshOnSuccessfulApplication,

	/** The duration of the effect will never be refreshed */
	NeverRefresh,
};

/** Enumeration of policies for dealing with the period of a gameplay effect while stacking */
UENUM()
enum class EGameplayEffectStackingPeriodPolicy : uint8
{
	/** Any progress toward the next tick of a periodic effect is discarded upon any successful stack application */
	ResetOnSuccessfulApplication,

	/** The progress toward the next tick of a periodic effect will never be reset, regardless of stack applications */
	NeverReset,
};

/** Enumeration of policies for dealing gameplay effect stacks that expire (in duration based effects). */
UENUM()
enum class EGameplayEffectStackingExpirationPolicy : uint8
{
	/** The entire stack is cleared when the active gameplay effect expires  */
	ClearEntireStack,

	/** The current stack count will be decremented by 1 and the duration refreshed. The GE is not "reapplied", just continues to exist with one less stacks. */
	RemoveSingleStackAndRefreshDuration,

	/** The duration of the gameplay effect is refreshed. This essentially makes the effect infinite in duration. This can be used to manually handle stack decrements via XXX callback */
	RefreshDuration,
};

/** Holds evaluated magnitude from a GameplayEffect modifier */
USTRUCT()
struct FModifierSpec
{
	GENERATED_USTRUCT_BODY()

	FModifierSpec() : EvaluatedMagnitude(0.f) { }

	float GetEvaluatedMagnitude() const { return EvaluatedMagnitude; }

private:

	// @todo: Probably need to make the modifier info private so people don't go gunking around in the magnitude
	/** In the event that the modifier spec requires custom magnitude calculations, this is the authoritative, last evaluated value of the magnitude */
	UPROPERTY()
	float EvaluatedMagnitude;

	/** These structures are the only ones that should internally be able to update the EvaluatedMagnitude. Any gamecode that gets its hands on FModifierSpec should never be setting EvaluatedMagnitude manually */
	friend struct FGameplayEffectSpec;
	friend struct FActiveGameplayEffectsContainer;
};

/** Saves list of modified attributes, to use for gameplay cues or later processing */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayEffectModifiedAttribute
{
	GENERATED_USTRUCT_BODY()

	/** The attribute that has been modified */
	UPROPERTY()
	FGameplayAttribute Attribute;

	/** Total magnitude applied to that attribute */
	UPROPERTY()
	float TotalMagnitude;

	FGameplayEffectModifiedAttribute() : TotalMagnitude(0.0f) {}
};

/** Struct used to hold the result of a gameplay attribute capture; Initially seeded by definition data, but then populated by ability system component when appropriate */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayEffectAttributeCaptureSpec
{
	// Allow these as friends so they can seed the aggregator, which we don't otherwise want exposed
	friend struct FActiveGameplayEffectsContainer;
	friend class UAbilitySystemComponent;

	GENERATED_USTRUCT_BODY()

	// Constructors
	FGameplayEffectAttributeCaptureSpec();
	FGameplayEffectAttributeCaptureSpec(const FGameplayEffectAttributeCaptureDefinition& InDefinition);

	/**
	 * Returns whether the spec actually has a valid capture yet or not
	 * 
	 * @return True if the spec has a valid attribute capture, false if it does not
	 */
	bool HasValidCapture() const;

	/**
	 * Attempts to calculate the magnitude of the captured attribute given the specified parameters. Can fail if the spec doesn't have
	 * a valid capture yet.
	 * 
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutMagnitude) const;

	/**
	 * Attempts to calculate the magnitude of the captured attribute given the specified parameters, up to the specified evaluation channel (inclusive).
	 * Can fail if the spec doesn't have a valid capture yet.
	 * 
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param FinalChannel	Evaluation channel to terminate the calculation at
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeMagnitudeUpToChannel(const FAggregatorEvaluateParameters& InEvalParams, EGameplayModEvaluationChannel FinalChannel, OUT float& OutMagnitude) const;

	/**
	 * Attempts to calculate the magnitude of the captured attribute given the specified parameters, including a starting base value. 
	 * Can fail if the spec doesn't have a valid capture yet.
	 * 
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param InBaseValue	Base value to evaluate the attribute under
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeMagnitudeWithBase(const FAggregatorEvaluateParameters& InEvalParams, float InBaseValue, OUT float& OutMagnitude) const;

	/**
	 * Attempts to calculate the base value of the captured attribute given the specified parameters. Can fail if the spec doesn't have
	 * a valid capture yet.
	 * 
	 * @param OutBaseValue	[OUT] Computed base value
	 * 
	 * @return True if the base value was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeBaseValue(OUT float& OutBaseValue) const;

	/**
	 * Attempts to calculate the "bonus" magnitude (final - base value) of the captured attribute given the specified parameters. Can fail if the spec doesn't have
	 * a valid capture yet.
	 * 
	 * @param InEvalParams		Parameters to evaluate the attribute under
	 * @param OutBonusMagnitude	[OUT] Computed bonus magnitude
	 * 
	 * @return True if the bonus magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeBonusMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutBonusMagnitude) const;

	/**
	 * Attempts to calculate the contribution of the specified GE to the captured attribute given the specified parameters. Can fail if the spec doesn't have
	 * a valid capture yet.
	 *
	 * @param InEvalParams		Parameters to evaluate the attribute under
	 * @param ActiveHandle		Handle of the gameplay effect to query about
	 * @param OutBonusMagnitude	[OUT] Computed bonus magnitude
	 *
	 * @return True if the bonus magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeContributionMagnitude(const FAggregatorEvaluateParameters& InEvalParams, FActiveGameplayEffectHandle ActiveHandle, OUT float& OutBonusMagnitude) const;

	/**
	 * Attempts to populate the specified aggregator with a snapshot of the backing captured aggregator. Can fail if the spec doesn't have
	 * a valid capture yet.
	 *
	 * @param OutAggregatorSnapshot	[OUT] Snapshotted aggregator, if possible
	 *
	 * @return True if the aggregator was successfully snapshotted, false if it was not
	 */
	bool AttemptGetAttributeAggregatorSnapshot(OUT FAggregator& OutAggregatorSnapshot) const;

	/**
	 * Attempts to populate the specified aggregator with all of the mods of the backing captured aggregator. Can fail if the spec doesn't have
	 * a valid capture yet.
	 *
	 * @param OutAggregatorToAddTo	[OUT] Aggregator with mods appended, if possible
	 *
	 * @return True if the aggregator had mods successfully added to it, false if it did not
	 */
	bool AttemptAddAggregatorModsToAggregator(OUT FAggregator& OutAggregatorToAddTo) const;
	
	/** Gathers made for a given capture. Note that these mods are unqualified and direct references (not copies). Only use this if you know what you are doing. */
	bool AttemptGatherAttributeMods(OUT TMap<EGameplayModEvaluationChannel, const TArray<FAggregatorMod>*>& OutModMap) const;
	
	/** Simple accessor to backing capture definition */
	const FGameplayEffectAttributeCaptureDefinition& GetBackingDefinition() const;

	/** Register this handle with linked aggregators */
	void RegisterLinkedAggregatorCallback(FActiveGameplayEffectHandle Handle) const;

	/** Unregister this handle with linked aggregators */
	void UnregisterLinkedAggregatorCallback(FActiveGameplayEffectHandle Handle) const;
	
	/** Return true if this capture should be recalculated if the given aggregator has changed */
	bool ShouldRefreshLinkedAggregator(const FAggregator* ChangedAggregator) const;

	/** Swaps any internal references From aggregator To aggregator. Used when cloning */
	void SwapAggregator(FAggregatorRef From, FAggregatorRef To);
		
private:

	/** Copy of the definition the spec should adhere to for capturing */
	UPROPERTY()
	FGameplayEffectAttributeCaptureDefinition BackingDefinition;

	/** Ref to the aggregator for the captured attribute */
	FAggregatorRef AttributeAggregator;
};

/** Struct used to handle a collection of captured source and target attributes */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayEffectAttributeCaptureSpecContainer
{
	GENERATED_USTRUCT_BODY()

public:

	FGameplayEffectAttributeCaptureSpecContainer();

	FGameplayEffectAttributeCaptureSpecContainer(FGameplayEffectAttributeCaptureSpecContainer&& Other);

	FGameplayEffectAttributeCaptureSpecContainer(const FGameplayEffectAttributeCaptureSpecContainer& Other);

	FGameplayEffectAttributeCaptureSpecContainer& operator=(FGameplayEffectAttributeCaptureSpecContainer&& Other);

	FGameplayEffectAttributeCaptureSpecContainer& operator=(const FGameplayEffectAttributeCaptureSpecContainer& Other);

	/**
	 * Add a definition to be captured by the owner of the container. Will not add the definition if its exact
	 * match already exists within the container.
	 * 
	 * @param InCaptureDefinition	Definition to capture with
	 */
	void AddCaptureDefinition(const FGameplayEffectAttributeCaptureDefinition& InCaptureDefinition);

	/**
	 * Capture source or target attributes from the specified component. Should be called by the container's owner.
	 * 
	 * @param InAbilitySystemComponent	Component to capture attributes from
	 * @param InCaptureSource			Whether to capture attributes as source or target
	 */
	void CaptureAttributes(class UAbilitySystemComponent* InAbilitySystemComponent, EGameplayEffectAttributeCaptureSource InCaptureSource);

	/**
	 * Find a capture spec within the container matching the specified capture definition, if possible.
	 * 
	 * @param InDefinition				Capture definition to use as the search basis
	 * @param bOnlyIncludeValidCapture	If true, even if a spec is found, it won't be returned if it doesn't also have a valid capture already
	 * 
	 * @return The found attribute spec matching the specified search params, if any
	 */
	const FGameplayEffectAttributeCaptureSpec* FindCaptureSpecByDefinition(const FGameplayEffectAttributeCaptureDefinition& InDefinition, bool bOnlyIncludeValidCapture) const;

	/**
	 * Determines if the container has specs with valid captures for all of the specified definitions.
	 * 
	 * @param InCaptureDefsToCheck	Capture definitions to check for
	 * 
	 * @return True if the container has valid capture attributes for all of the specified definitions, false if it does not
	 */
	bool HasValidCapturedAttributes(const TArray<FGameplayEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const;

	/** Returns whether the container has at least one spec w/o snapshotted attributes */
	bool HasNonSnapshottedAttributes() const;

	/** Registers any linked aggregators to notify this active handle if they are dirtied */
	void RegisterLinkedAggregatorCallbacks(FActiveGameplayEffectHandle Handle) const;

	/** Unregisters any linked aggregators from notifying this active handle if they are dirtied */
	void UnregisterLinkedAggregatorCallbacks(FActiveGameplayEffectHandle Handle) const;

	/** Swaps any internal references From aggregator To aggregator. Used when cloning */
	void SwapAggregator(FAggregatorRef From, FAggregatorRef To);

private:

	/** Captured attributes from the source of a gameplay effect */
	UPROPERTY()
	TArray<FGameplayEffectAttributeCaptureSpec> SourceAttributes;

	/** Captured attributes from the target of a gameplay effect */
	UPROPERTY()
	TArray<FGameplayEffectAttributeCaptureSpec> TargetAttributes;

	/** If true, has at least one capture spec that did not request a snapshot */
	UPROPERTY()
	bool bHasNonSnapshottedAttributes;
};

/**
 * GameplayEffect Specification. Tells us:
 *	-What UGameplayEffect (const data)
 *	-What Level
 *  -Who instigated
 *  
 * FGameplayEffectSpec is modifiable. We start with initial conditions and modifications be applied to it. In this sense, it is stateful/mutable but it
 * is still distinct from an FActiveGameplayEffect which in an applied instance of an FGameplayEffectSpec.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEffectSpec
{
	GENERATED_USTRUCT_BODY()

	// --------------------------------------------------------------------------------------------------------------------------
	//	IMPORTANT: Any state added to FGameplayEffectSpec must be handled in the move/copy constructor/operator!
	//	(When VS2012/2013 support is dropped, we can use compiler generated operators, but until then these need to be maintained manually!)
	// --------------------------------------------------------------------------------------------------------------------------

	FGameplayEffectSpec();

	FGameplayEffectSpec(const UGameplayEffect* InDef, const FGameplayEffectContextHandle& InEffectContext, float Level = FGameplayEffectConstants::INVALID_LEVEL);

	FGameplayEffectSpec(const FGameplayEffectSpec& Other);

	FGameplayEffectSpec(const FGameplayEffectSpec& Other, const FGameplayEffectContextHandle& InEffectContext);		//For cloning, copy all attributes, but set a new effectContext.

	FGameplayEffectSpec(FGameplayEffectSpec&& Other);

	FGameplayEffectSpec& operator=(FGameplayEffectSpec&& Other);

	FGameplayEffectSpec& operator=(const FGameplayEffectSpec& Other);

	// Can be called manually but it is preferred to use the 3 parameter constructor
	void Initialize(const UGameplayEffect* InDef, const FGameplayEffectContextHandle& InEffectContext, float Level = FGameplayEffectConstants::INVALID_LEVEL);

	// Initialize the spec as a linked spec. The original spec's context is preserved except for the original GE asset tags, which are stripped out
	void InitializeFromLinkedSpec(const UGameplayEffect* InDef, const FGameplayEffectSpec& OriginalSpec);

	/**
	 * Determines if the spec has capture specs with valid captures for all of the specified definitions.
	 * 
	 * @param InCaptureDefsToCheck	Capture definitions to check for
	 * 
	 * @return True if the container has valid capture attributes for all of the specified definitions, false if it does not
	 */
	bool HasValidCapturedAttributes(const TArray<FGameplayEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const;

	/** Looks for an existing modified attribute struct, may return NULL */
	const FGameplayEffectModifiedAttribute* GetModifiedAttribute(const FGameplayAttribute& Attribute) const;
	FGameplayEffectModifiedAttribute* GetModifiedAttribute(const FGameplayAttribute& Attribute);

	/** Adds a new modified attribute struct, will always add so check to see if it exists first */
	FGameplayEffectModifiedAttribute* AddModifiedAttribute(const FGameplayAttribute& Attribute);

	/**
	 * Helper function to attempt to calculate the duration of the spec from its GE definition
	 * 
	 * @param OutDefDuration	Computed duration of the spec from its GE definition; Not the actual duration of the spec
	 * 
	 * @return True if the calculation was successful, false if it was not
	 */
	bool AttemptCalculateDurationFromDef(OUT float& OutDefDuration) const;

	/** Sets duration. This should only be called as the GameplayEffect is being created and applied; Ignores calls after attribute capture */
	void SetDuration(float NewDuration, bool bLockDuration);

	float GetDuration() const;
	float GetPeriod() const;
	float GetChanceToApplyToTarget() const;

	/** Set the context info: who and where this spec came from. */
	void SetContext(FGameplayEffectContextHandle NewEffectContext);

	FGameplayEffectContextHandle GetContext() const
	{
		return EffectContext;
	}

	// Appends all tags granted by this gameplay effect spec
	void GetAllGrantedTags(OUT FGameplayTagContainer& Container) const;

	// Appends all tags that apply to this gameplay effect spec
	void GetAllAssetTags(OUT FGameplayTagContainer& Container) const;

	/** Sets the magnitude of a SetByCaller modifier */
	void SetSetByCallerMagnitude(FName DataName, float Magnitude);

	/** Sets the magnitude of a SetByCaller modifier */
	void SetSetByCallerMagnitude(FGameplayTag DataTag, float Magnitude);

	/** Returns the magnitude of a SetByCaller modifier. Will return 0.f and Warn if the magnitude has not been set. */
	float GetSetByCallerMagnitude(FName DataName, bool WarnIfNotFound = true, float DefaultIfNotFound = 0.f) const;

	/** Returns the magnitude of a SetByCaller modifier. Will return 0.f and Warn if the magnitude has not been set. */
	float GetSetByCallerMagnitude(FGameplayTag DataTag, bool WarnIfNotFound = true, float DefaultIfNotFound = 0.f) const;

	void SetLevel(float InLevel);

	float GetLevel() const;

	void PrintAll() const;

	FString ToSimpleString() const;

	const FGameplayEffectContextHandle& GetEffectContext() const
	{
		return EffectContext;
	}

	void DuplicateEffectContext()
	{
		EffectContext = EffectContext.Duplicate();
	}

	void CaptureAttributeDataFromTarget(UAbilitySystemComponent* TargetAbilitySystemComponent);

	/**
	 * Get the computed magnitude of the modifier on the spec with the specified index
	 * 
	 * @param ModifierIndx			Modifier to get
	 * @param bFactorInStackCount	If true, the calculation will include the stack count
	 * 
	 * @return Computed magnitude
	 */
	float GetModifierMagnitude(int32 ModifierIdx, bool bFactorInStackCount) const;

	void CalculateModifierMagnitudes();

	/** Recapture attributes from source and target for cloning */
	void RecaptureAttributeDataForClone(UAbilitySystemComponent* OriginalASC, UAbilitySystemComponent* NewASC);

	/** Recaptures source actor tags of this spec without modifying anything else */
	void RecaptureSourceActorTags();

	/** Helper function to initialize all of the capture definitions required by the spec */
	void SetupAttributeCaptureDefinitions();

	/** Helper function that returns the duration after applying relevant modifiers from the source and target ability system components */
	float CalculateModifiedDuration() const;

private:

	void CaptureDataFromSource();

public:

	// -----------------------------------------------------------------------

	/** GameplayEfect definition. The static data that this spec points to. */
	UPROPERTY()
	const UGameplayEffect* Def;
	
	/** A list of attributes that were modified during the application of this spec */
	UPROPERTY()
	TArray<FGameplayEffectModifiedAttribute> ModifiedAttributes;
	
	/** Attributes captured by the spec that are relevant to custom calculations, potentially in owned modifiers, etc.; NOT replicated to clients */
	UPROPERTY(NotReplicated)
	FGameplayEffectAttributeCaptureSpecContainer CapturedRelevantAttributes;

	/** other effects that need to be applied to the target if this effect is successful */
	TArray< FGameplayEffectSpecHandle > TargetEffectSpecs;

	// The duration in seconds of this effect
	// instantaneous effects should have a duration of FGameplayEffectConstants::INSTANT_APPLICATION
	// effects that last forever should have a duration of FGameplayEffectConstants::INFINITE_DURATION
	UPROPERTY()
	float Duration;

	// The period in seconds of this effect.
	// Nonperiodic effects should have a period of FGameplayEffectConstants::NO_PERIOD
	UPROPERTY()
	float Period;

	// The chance, in a 0.0-1.0 range, that this GameplayEffect will be applied to the target Attribute or GameplayEffect.
	UPROPERTY()
	float ChanceToApplyToTarget;

	// Captured Source Tags on GameplayEffectSpec creation.	
	UPROPERTY(NotReplicated)
	FTagContainerAggregator	CapturedSourceTags;

	// Tags from the target, captured during execute	
	UPROPERTY(NotReplicated)
	FTagContainerAggregator	CapturedTargetTags;

	/** Tags that are granted and that did not come from the UGameplayEffect def. These are replicated. */
	UPROPERTY()
	FGameplayTagContainer DynamicGrantedTags;

	/** Tags that are on this effect spec and that did not come from the UGameplayEffect def. These are replicated. */
	UPROPERTY()
	FGameplayTagContainer DynamicAssetTags;
	
	UPROPERTY()
	TArray<FModifierSpec> Modifiers;

	UPROPERTY()
	int32 StackCount;

	/** Whether the spec has had its source attribute capture completed or not yet */
	UPROPERTY(NotReplicated)
	uint32 bCompletedSourceAttributeCapture : 1;

	/** Whether the spec has had its target attribute capture completed or not yet */
	UPROPERTY(NotReplicated)
	uint32 bCompletedTargetAttributeCapture : 1;

	/** Whether the duration of the spec is locked or not; If it is, attempts to set it will fail */
	UPROPERTY(NotReplicated)
	uint32 bDurationLocked : 1;

	UPROPERTY()
	TArray<FGameplayAbilitySpecDef> GrantedAbilitySpecs;

private:

	/** Map of set by caller magnitudes */
	TMap<FName, float>			SetByCallerNameMagnitudes;
	TMap<FGameplayTag, float>	SetByCallerTagMagnitudes;
	
	UPROPERTY()
	FGameplayEffectContextHandle EffectContext; // This tells us how we got here (who / what applied us)
	
	UPROPERTY()
	float Level;	
};


/** This is a cut down version of the gameplay effect spec used for RPCs. */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayEffectSpecForRPC
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectSpecForRPC();

	FGameplayEffectSpecForRPC(const FGameplayEffectSpec& InSpec);

	/** GameplayEfect definition. The static data that this spec points to. */
	UPROPERTY()
	const UGameplayEffect* Def;

	UPROPERTY()
	TArray<FGameplayEffectModifiedAttribute> ModifiedAttributes;

	UPROPERTY()
	FGameplayEffectContextHandle EffectContext; // This tells us how we got here (who / what applied us)

	UPROPERTY()
	FGameplayTagContainer AggregatedSourceTags;

	UPROPERTY()
	FGameplayTagContainer AggregatedTargetTags;

	UPROPERTY()
	float Level;

	UPROPERTY()
	float AbilityLevel;

	FGameplayEffectContextHandle GetContext() const
	{
		return EffectContext;
	}

	float GetLevel() const
	{
		return Level;
	}

	float GetAbilityLevel() const
	{
		return AbilityLevel;
	}

	FString ToSimpleString() const;

	const FGameplayEffectModifiedAttribute* GetModifiedAttribute(const FGameplayAttribute& Attribute) const;
};


/**
 * Active GameplayEffect instance
 *	-What GameplayEffect Spec
 *	-Start time
 *  -When to execute next
 *  -Replication callbacks
 *
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FActiveGameplayEffect : public FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()

	// ---------------------------------------------------------------------------------------------------------------------------------
	//  IMPORTANT: Any new state added to FActiveGameplayEffect must be handled in the copy/move constructor/operator
	//	(When VS2012/2013 support is dropped, we can use compiler generated operators, but until then these need to be maintained manually)
	// ---------------------------------------------------------------------------------------------------------------------------------

	FActiveGameplayEffect();

	FActiveGameplayEffect(const FActiveGameplayEffect& Other);

	FActiveGameplayEffect(FActiveGameplayEffectHandle InHandle, const FGameplayEffectSpec &InSpec, float CurrentWorldTime, float InStartServerWorldTime, FPredictionKey InPredictionKey);
	
	FActiveGameplayEffect(FActiveGameplayEffect&& Other);

	FActiveGameplayEffect& operator=(FActiveGameplayEffect&& other);

	FActiveGameplayEffect& operator=(const FActiveGameplayEffect& other);

	float GetTimeRemaining(float WorldTime) const
	{
		float Duration = GetDuration();		
		return (Duration == FGameplayEffectConstants::INFINITE_DURATION ? -1.f : Duration - (WorldTime - StartWorldTime));
	}
	
	float GetDuration() const
	{
		return Spec.GetDuration();
	}

	float GetPeriod() const
	{
		return Spec.GetPeriod();
	}

	float GetEndTime() const
	{
		float Duration = GetDuration();		
		return (Duration == FGameplayEffectConstants::INFINITE_DURATION ? -1.f : Duration + StartWorldTime);
	}

	void CheckOngoingTagRequirements(const FGameplayTagContainer& OwnerTags, struct FActiveGameplayEffectsContainer& OwningContainer, bool bInvokeGameplayCueEvents = false);

	void PrintAll() const;

	void PreReplicatedRemove(const struct FActiveGameplayEffectsContainer &InArray);
	void PostReplicatedAdd(const struct FActiveGameplayEffectsContainer &InArray);
	void PostReplicatedChange(const struct FActiveGameplayEffectsContainer &InArray);

	// Debug string used by Fast Array serialization
	FString GetDebugString();

	/** Refreshes the cached StartWorldTime for this effect. To be used when the server/client world time delta changes significantly to keep the start time in sync. */
	void RecomputeStartWorldTime(const FActiveGameplayEffectsContainer& InArray);

	bool operator==(const FActiveGameplayEffect& Other)
	{
		return Handle == Other.Handle;
	}

	// ---------------------------------------------------------------------------------------------------------------------------------

	/** Globally unique ID for identify this active gameplay effect. Can be used to look up owner. Not networked. */
	FActiveGameplayEffectHandle Handle;

	UPROPERTY()
	FGameplayEffectSpec Spec;

	UPROPERTY()
	FPredictionKey	PredictionKey;

	/** Server time this started */
	UPROPERTY()
	float StartServerWorldTime;

	/** Used for handling duration modifications being replicated */
	UPROPERTY(NotReplicated)
	float CachedStartServerWorldTime;

	UPROPERTY(NotReplicated)
	float StartWorldTime;

	// Not sure if this should replicate or not. If replicated, we may have trouble where IsInhibited doesn't appear to change when we do tag checks (because it was previously inhibited, but replication made it inhibited).
	UPROPERTY()
	bool bIsInhibited;

	/** When replicated down, we cue the GC events until the entire list of active gameplay effects has been received */
	mutable bool bPendingRepOnActiveGC;
	mutable bool bPendingRepWhileActiveGC;

	bool IsPendingRemove;

	/** Last StackCount that the client had. Used to tell if the stackcount has changed in PostReplicatedChange */
	int32 ClientCachedStackCount;

	FOnActiveGameplayEffectRemoved OnRemovedDelegate;
	FOnActiveGameplayEffectRemoved_Info OnRemoved_InfoDelegate;

	FOnActiveGameplayEffectStackChange OnStackChangeDelegate;

	FOnActiveGameplayEffectTimeChange OnTimeChangeDelegate;

	FTimerHandle PeriodHandle;

	FTimerHandle DurationHandle;

	FActiveGameplayEffect* PendingNext;
};

DECLARE_DELEGATE_RetVal_OneParam(bool, FActiveGameplayEffectQueryCustomMatch, const FActiveGameplayEffect&);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FActiveGameplayEffectQueryCustomMatch_Dynamic, FActiveGameplayEffect, Effect, bool&, bMatches);

/** Every set condition within this query must match in order for the query to match. i.e. individual query elements are ANDed together. */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEffectQuery
{
	GENERATED_USTRUCT_BODY()

public:
	// ctors and operators
	FGameplayEffectQuery();
	FGameplayEffectQuery(const FGameplayEffectQuery& Other);
	FGameplayEffectQuery(FActiveGameplayEffectQueryCustomMatch InCustomMatchDelegate);
	FGameplayEffectQuery(FGameplayEffectQuery&& Other);
	FGameplayEffectQuery& operator=(FGameplayEffectQuery&& Other);
	FGameplayEffectQuery& operator=(const FGameplayEffectQuery& Other);

	/** Native delegate for providing custom matching conditions. */
	FActiveGameplayEffectQueryCustomMatch CustomMatchDelegate;

	/** BP-exposed delegate for providing custom matching conditions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FActiveGameplayEffectQueryCustomMatch_Dynamic CustomMatchDelegate_BP;

	/** Query that is matched against tags this GE gives */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FGameplayTagQuery OwningTagQuery;

	/** Query that is matched against tags this GE has */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FGameplayTagQuery EffectTagQuery;

	/** Query that is matched against tags the source of this GE has */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FGameplayTagQuery SourceTagQuery;

	/** Matches on GameplayEffects which modify given attribute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FGameplayAttribute ModifyingAttribute;

	/** Matches on GameplayEffects which come from this source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	const UObject* EffectSource;

	/** Matches on GameplayEffects with this definition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	TSubclassOf<UGameplayEffect> EffectDefinition;

	/** Handles to ignore as matches, even if other criteria is met */
	TArray<FActiveGameplayEffectHandle> IgnoreHandles;

	/** Returns true if Effect matches all specified criteria of this query, including CustomMatch delegates if bound. Returns false otherwise. */
	bool Matches(const FActiveGameplayEffect& Effect) const;

	/** Returns true if Effect matches all specified criteria of this query. This does NOT check FActiveGameplayEffectQueryCustomMatch since this is performed on the spec (possibly prior to applying).
	 *	Note: it would be reasonable to support a custom delegate that operated on the FGameplayEffectSpec itself.
	 */
	bool Matches(const FGameplayEffectSpec& Effect) const;

	/** Returns true if the query is empty/default. E.g., it has no data set. */
	bool IsEmpty() const;

	/** 
	 * Shortcuts for easily creating common query types 
	 * @todo: add more as dictated by use cases
	 */

	/** Creates an effect query that will match if there are any common tags between the given tags and an ActiveGameplayEffect's owning tags */
	static FGameplayEffectQuery MakeQuery_MatchAnyOwningTags(const FGameplayTagContainer& InTags);
	/** Creates an effect query that will match if all of the given tags are in the ActiveGameplayEffect's owning tags */
	static FGameplayEffectQuery MakeQuery_MatchAllOwningTags(const FGameplayTagContainer& InTags);
	/** Creates an effect query that will match if there are no common tags between the given tags and an ActiveGameplayEffect's owning tags */
	static FGameplayEffectQuery MakeQuery_MatchNoOwningTags(const FGameplayTagContainer& InTags);
	
	/** Creates an effect query that will match if there are any common tags between the given tags and an ActiveGameplayEffect's tags */
	static FGameplayEffectQuery MakeQuery_MatchAnyEffectTags(const FGameplayTagContainer& InTags);
	/** Creates an effect query that will match if all of the given tags are in the ActiveGameplayEffect's tags */
	static FGameplayEffectQuery MakeQuery_MatchAllEffectTags(const FGameplayTagContainer& InTags);
	/** Creates an effect query that will match if there are no common tags between the given tags and an ActiveGameplayEffect's tags */
	static FGameplayEffectQuery MakeQuery_MatchNoEffectTags(const FGameplayTagContainer& InTags);

	/** Creates an effect query that will match if there are any common tags between the given tags and an ActiveGameplayEffect's source tags */
	static FGameplayEffectQuery MakeQuery_MatchAnySourceTags(const FGameplayTagContainer& InTags);
	/** Creates an effect query that will match if all of the given tags are in the ActiveGameplayEffect's source tags */
	static FGameplayEffectQuery MakeQuery_MatchAllSourceTags(const FGameplayTagContainer& InTags);
	/** Creates an effect query that will match if there are no common tags between the given tags and an ActiveGameplayEffect's source tags */
	static FGameplayEffectQuery MakeQuery_MatchNoSourceTags(const FGameplayTagContainer& InTags);
};

/**
 *	Generic querying data structure for active GameplayEffects. Lets us ask things like:
 *		Give me duration/magnitude of active gameplay effects with these tags
 *		Give me handles to all activate gameplay effects modifying this attribute.
 *		
 *	Any requirements specified in the query are required: must meet "all" not "one".
 */
USTRUCT()
struct FActiveGameplayEffectQuery
{
	GENERATED_USTRUCT_BODY()

	FActiveGameplayEffectQuery()
		: OwningTagContainer(nullptr)
		, EffectTagContainer(nullptr)
		, OwningTagContainer_Rejection(nullptr)
		, EffectTagContainer_Rejection(nullptr)
		, EffectSource(nullptr)
		, EffectDef(nullptr)
	{
	}

	FActiveGameplayEffectQuery(const FGameplayTagContainer* InOwningTagContainer)
		: OwningTagContainer(InOwningTagContainer)
		, EffectTagContainer(nullptr)
		, OwningTagContainer_Rejection(nullptr)
		, EffectTagContainer_Rejection(nullptr)
		, EffectSource(nullptr)
		, EffectDef(nullptr)
	{
	}

	/** Bind this to override the default query-matching code. */
	FActiveGameplayEffectQueryCustomMatch CustomMatch;

	/** Returns true if Effect matches the criteria of this query, which will be overridden by CustomMatch if it is bound. Returns false otherwise. */
	bool Matches(const FActiveGameplayEffect& Effect) const;

	/** used to match with InheritableOwnedTagsContainer */
	const FGameplayTagContainer* OwningTagContainer;

	/** used to match with InheritableGameplayEffectTags */
	const FGameplayTagContainer* EffectTagContainer;

	/** used to reject matches with InheritableOwnedTagsContainer */
	const FGameplayTagContainer* OwningTagContainer_Rejection;

	/** used to reject matches with InheritableGameplayEffectTags */
	const FGameplayTagContainer* EffectTagContainer_Rejection;

	// Matches on GameplayEffects which modify given attribute
	FGameplayAttribute ModifyingAttribute;

	// Matches on GameplayEffects which come from this source
	const UObject* EffectSource;

	// Matches on GameplayEffects with this definition
	const UGameplayEffect* EffectDef;

	// Handles to ignore as matches, even if other criteria is met
	TArray<FActiveGameplayEffectHandle> IgnoreHandles;
};

/** Helper struct to hold data about external dependencies for custom modifiers */
struct FCustomModifierDependencyHandle
{
	FCustomModifierDependencyHandle()
		: ActiveEffectHandles()
		, ActiveDelegateHandle()
	{}

	/** Set of handles of active gameplay effects dependent upon a particular external dependency */
	TSet<FActiveGameplayEffectHandle> ActiveEffectHandles;

	/** Delegate handle populated as a result of binding to an external dependency delegate */
	FDelegateHandle ActiveDelegateHandle;
};

/**
 * Active GameplayEffects Container
 *	-Bucket of ActiveGameplayEffects
 *	-Needed for FFastArraySerialization
 *  
 * This should only be used by UAbilitySystemComponent. All of this could just live in UAbilitySystemComponent except that we need a distinct USTRUCT to implement FFastArraySerializer.
 *
 * The preferred way to iterate through the ActiveGameplayEffectContainer is with CreateConstITerator/CreateIterator or stl style range iteration:
 * 
 * 
 *	for (const FActiveGameplayEffect& Effect : this)
 *	{
 *	}
 *
 *	for (auto It = CreateConstIterator(); It; ++It) 
 *	{
 *	}
 *
 */
USTRUCT()
struct GAMEPLAYABILITIES_API FActiveGameplayEffectsContainer : public FFastArraySerializer
{
	GENERATED_USTRUCT_BODY();

	friend struct FActiveGameplayEffect;
	friend class UAbilitySystemComponent;
	friend struct FScopedActiveGameplayEffectLock;
	friend class AAbilitySystemDebugHUD;
	friend class FActiveGameplayEffectIterator<const FActiveGameplayEffect, FActiveGameplayEffectsContainer>;
	friend class FActiveGameplayEffectIterator<FActiveGameplayEffect, FActiveGameplayEffectsContainer>;

	typedef FActiveGameplayEffectIterator<const FActiveGameplayEffect, FActiveGameplayEffectsContainer> ConstIterator;
	typedef FActiveGameplayEffectIterator<FActiveGameplayEffect, FActiveGameplayEffectsContainer> Iterator;

	FActiveGameplayEffectsContainer();
	virtual ~FActiveGameplayEffectsContainer();

	UAbilitySystemComponent* Owner;
	bool OwnerIsNetAuthority;

	FOnGivenActiveGameplayEffectRemoved	OnActiveGameplayEffectRemovedDelegate;

	struct DebugExecutedGameplayEffectData
	{
		FString GameplayEffectName;
		FString ActivationState;
		FGameplayAttribute Attribute;
		TEnumAsByte<EGameplayModOp::Type> ModifierOp;
		float Magnitude;
		int32 StackCount;
	};

#if ENABLE_VISUAL_LOG
	// Stores a record of gameplay effects that have executed and their results. Useful for debugging.
	TArray<DebugExecutedGameplayEffectData> DebugExecutedGameplayEffects;

	void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

	void GetActiveGameplayEffectDataByAttribute(TMultiMap<FGameplayAttribute, FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData>& EffectMap) const;

	void RegisterWithOwner(UAbilitySystemComponent* Owner);	
	
	FActiveGameplayEffect* ApplyGameplayEffectSpec(const FGameplayEffectSpec& Spec, FPredictionKey& InPredictionKey, bool& bFoundExistingStackableGE);

	FActiveGameplayEffect* GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle);

	const FActiveGameplayEffect* GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle) const;

	void ExecuteActiveEffectsFrom(FGameplayEffectSpec &Spec, FPredictionKey PredictionKey = FPredictionKey() );
	
	void ExecutePeriodicGameplayEffect(FActiveGameplayEffectHandle Handle);	// This should not be outward facing to the skill system API, should only be called by the owning AbilitySystemComponent

	bool RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove);

	void GetGameplayEffectStartTimeAndDuration(FActiveGameplayEffectHandle Handle, float& EffectStartTime, float& EffectDuration) const;

	float GetGameplayEffectMagnitude(FActiveGameplayEffectHandle Handle, FGameplayAttribute Attribute) const;

	void SetActiveGameplayEffectLevel(FActiveGameplayEffectHandle ActiveHandle, int32 NewLevel);

	void SetAttributeBaseValue(FGameplayAttribute Attribute, float NewBaseValue);

	float GetAttributeBaseValue(FGameplayAttribute Attribute) const;

	float GetEffectContribution(const FAggregatorEvaluateParameters& Parameters, FActiveGameplayEffectHandle ActiveHandle, FGameplayAttribute Attribute);

	/** Actually applies given mod to the attribute */
	void ApplyModToAttribute(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude, const FGameplayEffectModCallbackData* ModData=nullptr);

	/**
	 * Get the source tags from the gameplay spec represented by the specified handle, if possible
	 * 
	 * @param Handle	Handle of the gameplay effect to retrieve source tags from
	 * 
	 * @return Source tags from the gameplay spec represented by the handle, if possible
	 */
	const FGameplayTagContainer* GetGameplayEffectSourceTagsFromHandle(FActiveGameplayEffectHandle Handle) const;

	/**
	 * Get the target tags from the gameplay spec represented by the specified handle, if possible
	 * 
	 * @param Handle	Handle of the gameplay effect to retrieve target tags from
	 * 
	 * @return Target tags from the gameplay spec represented by the handle, if possible
	 */
	const FGameplayTagContainer* GetGameplayEffectTargetTagsFromHandle(FActiveGameplayEffectHandle Handle) const;

	/**
	 * Populate the specified capture spec with the data necessary to capture an attribute from the container
	 * 
	 * @param OutCaptureSpec	[OUT] Capture spec to populate with captured data
	 */
	void CaptureAttributeForGameplayEffect(OUT FGameplayEffectAttributeCaptureSpec& OutCaptureSpec);

	void PrintAllGameplayEffects() const;

	/**
	 *	Returns the total number of gameplay effects.
	 *	NOTE this does include GameplayEffects that pending removal.
	 *	Any pending remove gameplay effects are deleted at the end of their scope lock
	 */
	FORCEINLINE int32 GetNumGameplayEffects() const
	{
		int32 NumPending = 0;
		FActiveGameplayEffect* PendingGameplayEffect = PendingGameplayEffectHead;
		FActiveGameplayEffect* Stop = *PendingGameplayEffectNext;
		while (PendingGameplayEffect && PendingGameplayEffect != Stop)
		{
			++NumPending;
			PendingGameplayEffect = PendingGameplayEffect->PendingNext;
		}

		return GameplayEffects_Internal.Num() + NumPending;
	}

	void CheckDuration(FActiveGameplayEffectHandle Handle);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	void Uninitialize();	

	// ------------------------------------------------

	bool CanApplyAttributeModifiers(const UGameplayEffect *GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext);
	
	TArray<float> GetActiveEffectsTimeRemaining(const FGameplayEffectQuery& Query) const;

	TArray<float> GetActiveEffectsDuration(const FGameplayEffectQuery& Query) const;

	TArray<TPair<float,float>> GetActiveEffectsTimeRemainingAndDuration(const FGameplayEffectQuery& Query) const;

	TArray<FActiveGameplayEffectHandle> GetActiveEffects(const FGameplayEffectQuery& Query) const;

	float GetActiveEffectsEndTime(const FGameplayEffectQuery& Query) const;
	bool GetActiveEffectsEndTimeAndDuration(const FGameplayEffectQuery& Query, float& EndTime, float& Duration) const;

	/** Returns an array of all of the active gameplay effect handles */
	TArray<FActiveGameplayEffectHandle> GetAllActiveEffectHandles() const;

	void ModifyActiveEffectStartTime(FActiveGameplayEffectHandle Handle, float StartTimeDiff);

	int32 RemoveActiveEffects(const FGameplayEffectQuery& Query, int32 StacksToRemove);

	/**
	 * Get the count of the effects matching the specified query (including stack count)
	 * 
	 * @return Count of the effects matching the specified query
	 */
	int32 GetActiveEffectCount(const FGameplayEffectQuery& Query, bool bEnforceOnGoingCheck = true) const;

	float GetServerWorldTime() const;

	float GetWorldTime() const;

	bool HasReceivedEffectWithPredictedKey(FPredictionKey PredictionKey) const;

	bool HasPredictedEffectWithPredictedKey(FPredictionKey PredictionKey) const;
		
	void SetBaseAttributeValueFromReplication(FGameplayAttribute Attribute, float BaseBalue);

	void GetAllActiveGameplayEffectSpecs(TArray<FGameplayEffectSpec>& OutSpecCopies) const;

	void DebugCyclicAggregatorBroadcasts(struct FAggregator* Aggregator);

	/** Performs a deep copy on the source container, duplicating all gameplay effects and reconstructing the attribute aggregator map to match the passed in source. */
	void CloneFrom(const FActiveGameplayEffectsContainer& Source);

	// -------------------------------------------------------------------------------------------

	DEPRECATED(4.17, "Use GetGameplayAttributeValueChangeDelegate (the delegate signature has changed)")
	FOnGameplayAttributeChange& RegisterGameplayAttributeEvent(FGameplayAttribute Attribute);

	FOnGameplayAttributeValueChange& GetGameplayAttributeValueChangeDelegate(FGameplayAttribute Attribute);

	void OnOwnerTagChange(FGameplayTag TagChange, int32 NewCount);

	bool HasApplicationImmunityToSpec(const FGameplayEffectSpec& SpecToApply, const FActiveGameplayEffect*& OutGEThatProvidedImmunity) const;

	void IncrementLock();
	void DecrementLock();
	
	FORCEINLINE ConstIterator CreateConstIterator() const { return ConstIterator(*this);	}
	FORCEINLINE Iterator CreateIterator() { return Iterator(*this);	}

private:

	/**
	 *	Accessors for internal functions to get GameplayEffects directly by index.
	 *	Note this will return GameplayEffects that are pending removal!
	 *	
	 *	To iterate over all 'valid' gameplay effects, use the CreateConstIterator/CreateIterator or the stl style range iterator
	 */
	FORCEINLINE const FActiveGameplayEffect* GetActiveGameplayEffect(int32 idx) const
	{
		return const_cast<FActiveGameplayEffectsContainer*>(this)->GetActiveGameplayEffect(idx);
	}

	FORCEINLINE FActiveGameplayEffect* GetActiveGameplayEffect(int32 idx)
	{
		if (idx < GameplayEffects_Internal.Num())
		{
			return &GameplayEffects_Internal[idx];
		}

		idx -= GameplayEffects_Internal.Num();
		FActiveGameplayEffect* Ptr = PendingGameplayEffectHead;
		FActiveGameplayEffect* Stop = *PendingGameplayEffectNext;

		// Advance until the desired index or until hitting the actual end of the pending list currently in use (need to check both Ptr and Ptr->PendingNext to prevent hopping
		// the pointer too far along)
		while (idx-- > 0 && Ptr && Ptr != Stop && Ptr->PendingNext != Stop)
		{
			Ptr = Ptr->PendingNext;
		}

		return idx <= 0 ? Ptr : nullptr;
	}

	/** Our active list of Effects. Do not access this directly (Even from internal functions!) Use GetNumGameplayEffect() / GetGameplayEffect() ! */
	UPROPERTY()
	TArray<FActiveGameplayEffect>	GameplayEffects_Internal;

	void InternalUpdateNumericalAttribute(FGameplayAttribute Attribute, float NewValue, const FGameplayEffectModCallbackData* ModData, bool bFromRecursiveCall=false);

	/** Cached pointer to current mod data needed for callbacks. We cache it in the AGE struct to avoid passing it through all the delegate/aggregator plumbing */
	const struct FGameplayEffectModCallbackData* CurrentModcallbackData;
	
	/**
	 * Helper function to execute a mod on owned attributes
	 * 
	 * @param Spec			Gameplay effect spec executing the mod
	 * @param ModEvalData	Evaluated data for the mod
	 * 
	 * @return True if the mod successfully executed, false if it did not
	 */
	bool InternalExecuteMod(FGameplayEffectSpec& Spec, FGameplayModifierEvaluatedData& ModEvalData);

	bool IsNetAuthority() const
	{
		return OwnerIsNetAuthority;
	}

	/** Called internally to actually remove a GameplayEffect or to reduce its StackCount. Returns true if we resized our internal GameplayEffect array. */
	bool InternalRemoveActiveGameplayEffect(int32 Idx, int32 StacksToRemove, bool bPrematureRemoval);
	
	/** Called both in server side creation and replication creation/deletion */
	void InternalOnActiveGameplayEffectAdded(FActiveGameplayEffect& Effect);
	void InternalOnActiveGameplayEffectRemoved(FActiveGameplayEffect& Effect, bool bInvokeGameplayCueEvents, const FGameplayEffectRemovalInfo& GameplayEffectRemovalInfo);

	void RemoveActiveGameplayEffectGrantedTagsAndModifiers(const FActiveGameplayEffect& Effect, bool bInvokeGameplayCueEvents);
	void AddActiveGameplayEffectGrantedTagsAndModifiers(FActiveGameplayEffect& Effect, bool bInvokeGameplayCueEvents);

	/** Updates tag dependency map when a GameplayEffect is removed */
	void RemoveActiveEffectTagDependency(const FGameplayTagContainer& Tags, FActiveGameplayEffectHandle Handle);

	/** Internal helper function to bind the active effect to all of the custom modifier magnitude external dependency delegates it contains, if any */
	void AddCustomMagnitudeExternalDependencies(FActiveGameplayEffect& Effect);

	/** Internal helper function to unbind the active effect from all of the custom modifier magnitude external dependency delegates it may have bound to, if any */
	void RemoveCustomMagnitudeExternalDependencies(FActiveGameplayEffect& Effect);

	/** Internal callback fired as a result of a custom modifier magnitude external dependency delegate firing; Updates affected active gameplay effects as necessary */
	void OnCustomMagnitudeExternalDependencyFired(TSubclassOf<UGameplayModMagnitudeCalculation> MagnitudeCalculationClass);

	/** Internal helper function to apply expiration effects from a removed/expired gameplay effect spec */
	void InternalApplyExpirationEffects(const FGameplayEffectSpec& ExpiringSpec, bool bPrematureRemoval);

	void RestartActiveGameplayEffectDuration(FActiveGameplayEffect& ActiveGameplayEffect);

	// -------------------------------------------------------------------------------------------

	TMap<FGameplayAttribute, FAggregatorRef>		AttributeAggregatorMap;

	// DEPRECATED: use AttributeValueChangeDelegates
	TMap<FGameplayAttribute, FOnGameplayAttributeChange> AttributeChangeDelegates;
	
	TMap<FGameplayAttribute, FOnGameplayAttributeValueChange> AttributeValueChangeDelegates;


	TMap<FGameplayTag, TSet<FActiveGameplayEffectHandle> >	ActiveEffectTagDependencies;

	/** Mapping of custom gameplay modifier magnitude calculation class to dependency handles for triggering updates on external delegates firing */
	TMap<FObjectKey, FCustomModifierDependencyHandle> CustomMagnitudeClassDependencies;

	/** A map to manage stacking while we are the source */
	TMap<TWeakObjectPtr<UGameplayEffect>, TArray<FActiveGameplayEffectHandle> >	SourceStackingMap;
	
	/** Acceleration struct for immunity tests */
	FGameplayTagCountContainer ApplicationImmunityGameplayTagCountContainer;

	/** Active GEs that have immunity queries. This is an acceleration list to avoid searching through the Active GameplayEffect list frequetly. (We only search for the active GE if immunity procs) */
	UPROPERTY()
	TArray<const UGameplayEffect*> ApplicationImmunityQueryEffects;

	FAggregatorRef& FindOrCreateAttributeAggregator(FGameplayAttribute Attribute);

	void OnAttributeAggregatorDirty(FAggregator* Aggregator, FGameplayAttribute Attribute, bool FromRecursiveCall=false);

	void OnMagnitudeDependencyChange(FActiveGameplayEffectHandle Handle, const FAggregator* ChangedAgg);

	void OnStackCountChange(FActiveGameplayEffect& ActiveEffect, int32 OldStackCount, int32 NewStackCount);

	void OnDurationChange(FActiveGameplayEffect& ActiveEffect);

	void UpdateAllAggregatorModMagnitudes(FActiveGameplayEffect& ActiveEffect);

	void UpdateAggregatorModMagnitudes(const TSet<FGameplayAttribute>& AttributesToUpdate, FActiveGameplayEffect& ActiveEffect);

	/** Helper function to find the active GE that the specified spec can stack with, if any */
	FActiveGameplayEffect* FindStackableActiveGameplayEffect(const FGameplayEffectSpec& Spec);
	
	/** Helper function to handle the case of same-effect stacking overflow; Returns true if the overflow application should apply, false if it should not */
	bool HandleActiveGameplayEffectStackOverflow(const FActiveGameplayEffect& ActiveStackableGE, const FGameplayEffectSpec& OldSpec, const FGameplayEffectSpec& OverflowingSpec);

	/** After application has gone through, give stacking rules a chance to do something as the source of the gameplay effect (E.g., remove an old version) */
	virtual void ApplyStackingLogicPostApplyAsSource(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle) { }

	bool ShouldUseMinimalReplication();

	mutable int32 ScopedLockCount;
	int32 PendingRemoves;

	FActiveGameplayEffect*	PendingGameplayEffectHead;	// Head of pending GE linked list
	FActiveGameplayEffect** PendingGameplayEffectNext;	// Points to the where to store the next pending GE (starts pointing at head, as more are added, points further down the list).

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */

	FORCEINLINE friend Iterator begin(FActiveGameplayEffectsContainer* Container) { return Container->CreateIterator(); }
	FORCEINLINE friend Iterator end(FActiveGameplayEffectsContainer* Container) { return Iterator(*Container, -1); }

	FORCEINLINE friend ConstIterator begin(const FActiveGameplayEffectsContainer* Container) { return Container->CreateConstIterator(); }
	FORCEINLINE friend ConstIterator end(const FActiveGameplayEffectsContainer* Container) { return ConstIterator(*Container, -1); }
};

template<>
struct TStructOpsTypeTraits< FActiveGameplayEffectsContainer > : public TStructOpsTypeTraitsBase2< FActiveGameplayEffectsContainer >
{
	enum
	{
		WithNetDeltaSerializer = true,
		WithCopy = false,
	};
};

/**
 *	FScopedActiveGameplayEffectLock
 *	Provides a mechanism for locking the active gameplay effect list while possibly invoking callbacks into gamecode.
 *	For example, if some internal code in FActiveGameplayEffectsContainer is iterating through the active GE list
 *	or holding onto a pointer to something in that list, any changes to that list could cause memory the move out from underneath.
 *	
 *	This scope lock will queue deletions and additions until after the scope is over. The additions and deletions will actually 
 *	go through, but we will defer the memory operations to the active gameplay effect list.
 */
struct GAMEPLAYABILITIES_API FScopedActiveGameplayEffectLock
{
	FScopedActiveGameplayEffectLock(FActiveGameplayEffectsContainer& InContainer);
	~FScopedActiveGameplayEffectLock();

private:
	FActiveGameplayEffectsContainer& Container;
};

#define GAMEPLAYEFFECT_SCOPE_LOCK()	FScopedActiveGameplayEffectLock ActiveScopeLock(*this);


// -------------------------------------------------------------------------------------

/**
 * UGameplayEffect
 *	The GameplayEffect definition. This is the data asset defined in the editor that drives everything.
 *  This is only blueprintable to allow for templating gameplay effects. Gameplay effects should NOT contain blueprint graphs.
 */
UCLASS(Blueprintable, meta = (ShortTooltip="A GameplayEffect modifies attributes and tags."))
class GAMEPLAYABILITIES_API UGameplayEffect : public UObject, public IGameplayTagAssetInterface
{

public:
	GENERATED_UCLASS_BODY()

	// These are deprecated but remain for backwards compat, please use FGameplayEffectConstants:: instead.
	static const float INFINITE_DURATION;
	static const float INSTANT_APPLICATION;
	static const float NO_PERIOD;	
	static const float INVALID_LEVEL;

#if WITH_EDITORONLY_DATA
	/** Template to derive starting values and editing customization from */
	UPROPERTY()
	UGameplayEffectTemplate*	Template;

	/** When false, show a limited set of properties for editing, based on the template we are derived from */
	UPROPERTY()
	bool ShowAllProperties;
#endif

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Policy for the duration of this effect */
	UPROPERTY(EditDefaultsOnly, Category=GameplayEffect)
	EGameplayEffectDurationType DurationPolicy;

	/** Duration in seconds. 0.0 for instantaneous effects; -1.0 for infinite duration. */
	UPROPERTY(EditDefaultsOnly, Category=GameplayEffect)
	FGameplayEffectModifierMagnitude DurationMagnitude;

	/** Period in seconds. 0.0 for non-periodic effects */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Period)
	FScalableFloat	Period;
	
	/** If true, the effect executes on application and then at every period interval. If false, no execution occurs until the first period elapses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Period)
	bool bExecutePeriodicEffectOnApplication;

	/** Array of modifiers that will affect the target of this effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=GameplayEffect)
	TArray<FGameplayModifierInfo> Modifiers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayEffect)
	TArray<FGameplayEffectExecutionDefinition>	Executions;

	/** Probability that this gameplay effect will be applied to the target actor (0.0 for never, 1.0 for always) */
	UPROPERTY(EditDefaultsOnly, Category=Application, meta=(GameplayAttribute="True"))
	FScalableFloat	ChanceToApplyToTarget;

	UPROPERTY(EditDefaultsOnly, Category=Application, DisplayName="Application Requirement")
	TArray<TSubclassOf<UGameplayEffectCustomApplicationRequirement> > ApplicationRequirements;

	/** Deprecated. Use ConditionalGameplayEffects instead */
	UPROPERTY()
	TArray<TSubclassOf<UGameplayEffect>> TargetEffectClasses_DEPRECATED;

	/** other gameplay effects that will be applied to the target of this effect if this effect applies */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayEffect)
	TArray<FConditionalGameplayEffect> ConditionalGameplayEffects;

	/** Effects to apply when a stacking effect "overflows" its stack count through another attempted application. Added whether the overflow application succeeds or not. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Overflow)
	TArray<TSubclassOf<UGameplayEffect>> OverflowEffects;

	/** If true, stacking attempts made while at the stack count will fail, resulting in the duration and context not being refreshed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Overflow)
	bool bDenyOverflowApplication;

	/** If true, the entire stack of the effect will be cleared once it overflows */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Overflow, meta=(EditCondition="bDenyOverflowApplication"))
	bool bClearStackOnOverflow;

	/** Effects to apply when this effect is made to expire prematurely (like via a forced removal, clear tags, etc.); Only works for effects with a duration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Expiration)
	TArray<TSubclassOf<UGameplayEffect>> PrematureExpirationEffectClasses;

	/** Effects to apply when this effect expires naturally via its duration; Only works for effects with a duration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Expiration)
	TArray<TSubclassOf<UGameplayEffect>> RoutineExpirationEffectClasses;

	// ------------------------------------------------
	// Gameplay tag interface
	// ------------------------------------------------

	/** Overridden to return requirements tags */
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	void UpdateInheritedTagProperties();
	void ValidateGameplayEffect();

	virtual void PostLoad() override;
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;

	// ----------------------------------------------

	/** If true, cues will only trigger when GE modifiers succeed being applied (whether through modifiers or executions) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Display)
	bool bRequireModifierSuccessToTriggerCues;

	/** If true, GameplayCues will only be triggered for the first instance in a stacking GameplayEffect. */
	UPROPERTY(EditDefaultsOnly, Category = Display)
	bool bSuppressStackingCues;

	/** Cues to trigger non-simulated reactions in response to this GameplayEffect such as sounds, particle effects, etc */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Display)
	TArray<FGameplayEffectCue>	GameplayCues;

	/** Data for the UI representation of this effect. This should include things like text, icons, etc. Not available in server-only builds. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = Display)
	class UGameplayEffectUIData* UIData;

	// ----------------------------------------------------------------------
	//	Tag Containers
	// ----------------------------------------------------------------------
	
	/** The GameplayEffect's Tags: tags the the GE *has* and DOES NOT give to the actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags, meta = (DisplayName = "GameplayEffectAssetTag", Categories="GameplayEffectTagsCategory"))
	FInheritedTagContainer InheritableGameplayEffectTags;
	
	/** "These tags are applied to the actor I am applied to" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags, meta=(DisplayName="GrantedTags", Categories="OwnedTagsCategory"))
	FInheritedTagContainer InheritableOwnedTagsContainer;
	
	/** Once Applied, these tags requirements are used to determined if the GameplayEffect is "on" or "off". A GameplayEffect can be off and do nothing, but still applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags, meta=(Categories="OngoingTagRequirementsCategory"))
	FGameplayTagRequirements OngoingTagRequirements;

	/** Tag requirements for this GameplayEffect to be applied to a target. This is pass/fail at the time of application. If fail, this GE fails to apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags, meta=(Categories="ApplicationTagRequirementsCategory"))
	FGameplayTagRequirements ApplicationTagRequirements;

	/** GameplayEffects that *have* tags in this container will be cleared upon effect application. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags, meta=(Categories="RemoveTagRequirementsCategory"))
	FInheritedTagContainer RemoveGameplayEffectsWithTags;

	/** Grants the owner immunity from these source tags.  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Immunity, meta = (DisplayName = "GrantedApplicationImmunityTags", Categories="GrantedApplicationImmunityTagsCategory"))
	FGameplayTagRequirements GrantedApplicationImmunityTags;

	/** Grants immunity to GameplayEffects that match this query. Queries are more powerful but slightly slower than GrantedApplicationImmunityTags. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Immunity)
	FGameplayEffectQuery GrantedApplicationImmunityQuery;

	/** Cached !GrantedApplicationImmunityQuery.IsEmpty(). Set on PostLoad. */
	bool HasGrantedApplicationImmunityQuery;

	// ----------------------------------------------------------------------
	//	Stacking
	// ----------------------------------------------------------------------
	
	/** How this GameplayEffect stacks with other instances of this same GameplayEffect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	EGameplayEffectStackingType	StackingType;

	/** Stack limit for StackingType */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	int32 StackLimitCount;

	/** Policy for how the effect duration should be refreshed while stacking */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	EGameplayEffectStackingDurationPolicy StackDurationRefreshPolicy;

	/** Policy for how the effect period should be reset (or not) while stacking */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	EGameplayEffectStackingPeriodPolicy StackPeriodResetPolicy;

	/** Policy for how to handle duration expiring on this gameplay effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	EGameplayEffectStackingExpirationPolicy StackExpirationPolicy;

	// ----------------------------------------------------------------------
	//	Granted abilities
	// ----------------------------------------------------------------------
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granted Abilities")
	TArray<FGameplayAbilitySpecDef>	GrantedAbilities;
};
