// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayEffect.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/PackageMapClient.h"
#include "Engine/NetConnection.h"
#include "AbilitySystemStats.h"
#include "GameplayTagsModule.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "GameplayModMagnitudeCalculation.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GameplayCueManager.h"

#if ENABLE_VISUAL_LOG
//#include "VisualLogger/VisualLogger.h"
#endif // ENABLE_VISUAL_LOG

const float FGameplayEffectConstants::INFINITE_DURATION = -1.f;
const float FGameplayEffectConstants::INSTANT_APPLICATION = 0.f;
const float FGameplayEffectConstants::NO_PERIOD = 0.f;
const float FGameplayEffectConstants::INVALID_LEVEL = -1.f;

const float UGameplayEffect::INFINITE_DURATION = FGameplayEffectConstants::INFINITE_DURATION;
const float UGameplayEffect::INSTANT_APPLICATION =FGameplayEffectConstants::INSTANT_APPLICATION;
const float UGameplayEffect::NO_PERIOD = FGameplayEffectConstants::NO_PERIOD;
const float UGameplayEffect::INVALID_LEVEL = FGameplayEffectConstants::INVALID_LEVEL;

DECLARE_CYCLE_STAT(TEXT("MakeQuery"), STAT_MakeGameplayEffectQuery, STATGROUP_AbilitySystem);

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UGameplayEffect
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------


UGameplayEffect::UGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	bExecutePeriodicEffectOnApplication = true;
	ChanceToApplyToTarget.SetValue(1.f);
	StackingType = EGameplayEffectStackingType::None;
	StackLimitCount = 0;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
	bRequireModifierSuccessToTriggerCues = true;

#if WITH_EDITORONLY_DATA
	ShowAllProperties = true;
	Template = nullptr;
#endif

}

void UGameplayEffect::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer.AppendTags(InheritableOwnedTagsContainer.CombinedTags);
}

void UGameplayEffect::PostLoad()
{
	Super::PostLoad();

	// Temporary post-load fix-up to preserve magnitude data
	for (FGameplayModifierInfo& CurModInfo : Modifiers)
	{
		// If the old magnitude actually had some value in it, copy it over and then clear out the old data
		static const FString GameplayEffectPostLoadContext(TEXT("UGameplayEffect::PostLoad"));
		if (CurModInfo.Magnitude.Value != 0.f || CurModInfo.Magnitude.Curve.IsValid(GameplayEffectPostLoadContext))
		{
			CurModInfo.ModifierMagnitude.ScalableFloatMagnitude = CurModInfo.Magnitude;
			CurModInfo.Magnitude = FScalableFloat();
		}

#if WITH_EDITOR
		CurModInfo.ModifierMagnitude.ReportErrors(GetPathName());
#endif // WITH_EDITOR
	}

	// We need to update when we first load to override values coming in from the superclass
	// We also copy the tags from the old tag containers into the inheritable tag containers

	UpdateInheritedTagProperties();

	for (FGameplayAbilitySpecDef& Def : GrantedAbilities)
	{
		if (Def.Level != INDEX_NONE)
		{
			Def.LevelScalableFloat.SetValue(Def.Level);
			Def.Level = INDEX_NONE;
		}
	}

	HasGrantedApplicationImmunityQuery = !GrantedApplicationImmunityQuery.IsEmpty();

#if WITH_EDITOR
	GETCURVE_REPORTERROR(Period.Curve);
	GETCURVE_REPORTERROR(ChanceToApplyToTarget.Curve);
	DurationMagnitude.ReportErrors(GetPathName());
#endif // WITH_EDITOR


	for (const TSubclassOf<UGameplayEffect>& ConditionalEffectClass : TargetEffectClasses_DEPRECATED)
	{
		FConditionalGameplayEffect ConditionalGameplayEffect;
		ConditionalGameplayEffect.EffectClass = ConditionalEffectClass;
		ConditionalGameplayEffects.Add(ConditionalGameplayEffect);
	}
	TargetEffectClasses_DEPRECATED.Empty();

	for (FGameplayEffectExecutionDefinition& Execution : Executions)
	{
		for (const TSubclassOf<UGameplayEffect>& ConditionalEffectClass : Execution.ConditionalGameplayEffectClasses_DEPRECATED)
		{
			FConditionalGameplayEffect ConditionalGameplayEffect;
			ConditionalGameplayEffect.EffectClass = ConditionalEffectClass;
			Execution.ConditionalGameplayEffects.Add(ConditionalGameplayEffect);
		}

		Execution.ConditionalGameplayEffectClasses_DEPRECATED.Empty();
	}
}

void UGameplayEffect::PostInitProperties()
{
	Super::PostInitProperties();

	InheritableGameplayEffectTags.PostInitProperties();
	InheritableOwnedTagsContainer.PostInitProperties();
	RemoveGameplayEffectsWithTags.PostInitProperties();
}

#if WITH_EDITOR

void UGameplayEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (PropertyThatChanged)
	{
		UGameplayEffect* Parent = Cast<UGameplayEffect>(GetClass()->GetSuperClass()->GetDefaultObject());
		FName PropName = PropertyThatChanged->GetFName();
		if (PropName == GET_MEMBER_NAME_CHECKED(UGameplayEffect, InheritableGameplayEffectTags))
		{
			InheritableGameplayEffectTags.UpdateInheritedTagProperties(Parent ? &Parent->InheritableGameplayEffectTags : NULL);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UGameplayEffect, InheritableOwnedTagsContainer))
		{
			InheritableOwnedTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableOwnedTagsContainer : NULL);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UGameplayEffect, RemoveGameplayEffectsWithTags))
		{
			RemoveGameplayEffectsWithTags.UpdateInheritedTagProperties(Parent ? &Parent->RemoveGameplayEffectsWithTags : NULL);
		}
	}

	HasGrantedApplicationImmunityQuery = !GrantedApplicationImmunityQuery.IsEmpty();

	UAbilitySystemGlobals::Get().GameplayEffectPostEditChangeProperty(this, PropertyChangedEvent);
}

#endif // #if WITH_EDITOR

void UGameplayEffect::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
	HasGrantedApplicationImmunityQuery = !GrantedApplicationImmunityQuery.IsEmpty();
}

void UGameplayEffect::UpdateInheritedTagProperties()
{
	UGameplayEffect* Parent = Cast<UGameplayEffect>(GetClass()->GetSuperClass()->GetDefaultObject());

	InheritableGameplayEffectTags.UpdateInheritedTagProperties(Parent ? &Parent->InheritableGameplayEffectTags : NULL);
	InheritableOwnedTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableOwnedTagsContainer : NULL);
	RemoveGameplayEffectsWithTags.UpdateInheritedTagProperties(Parent ? &Parent->RemoveGameplayEffectsWithTags : NULL);
}

void UGameplayEffect::ValidateGameplayEffect()
{
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FAttributeBasedFloat
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

float FAttributeBasedFloat::CalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec) const
{
	const FGameplayEffectAttributeCaptureSpec* CaptureSpec = InRelevantSpec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(BackingAttribute, true);
	checkf(CaptureSpec, TEXT("Attempted to calculate an attribute-based float from spec: %s that did not have the required captured attribute: %s"), 
		*InRelevantSpec.ToSimpleString(), *BackingAttribute.ToSimpleString());

	float AttribValue = 0.f;

	// Base value can be calculated w/o evaluation parameters
	if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeBaseValue)
	{
		CaptureSpec->AttemptCalculateAttributeBaseValue(AttribValue);
	}
	// Set up eval params to handle magnitude or bonus magnitude calculations
	else
	{
		FAggregatorEvaluateParameters EvaluationParameters;
		EvaluationParameters.SourceTags = InRelevantSpec.CapturedSourceTags.GetAggregatedTags();
		EvaluationParameters.TargetTags = InRelevantSpec.CapturedTargetTags.GetAggregatedTags();
		EvaluationParameters.AppliedSourceTagFilter = SourceTagFilter;
		EvaluationParameters.AppliedTargetTagFilter = TargetTagFilter;

		if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeMagnitude)
		{
			CaptureSpec->AttemptCalculateAttributeMagnitude(EvaluationParameters, AttribValue);
		}
		else if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeBonusMagnitude)
		{
			CaptureSpec->AttemptCalculateAttributeBonusMagnitude(EvaluationParameters, AttribValue);
		}
		else if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeMagnitudeEvaluatedUpToChannel)
		{
			const bool bRequestingValidChannel = UAbilitySystemGlobals::Get().IsGameplayModEvaluationChannelValid(FinalChannel);
			ensure(bRequestingValidChannel);
			const EGameplayModEvaluationChannel ChannelToUse = bRequestingValidChannel ? FinalChannel : EGameplayModEvaluationChannel::Channel0;

			CaptureSpec->AttemptCalculateAttributeMagnitudeUpToChannel(EvaluationParameters, ChannelToUse, AttribValue);
		}
	}

	// if a curve table entry is specified, use the attribute value as a lookup into the curve instead of using it directly
	static const FString CalculateMagnitudeContext(TEXT("FAttributeBasedFloat::CalculateMagnitude"));
	if (AttributeCurve.IsValid(CalculateMagnitudeContext))
	{
		AttributeCurve.Eval(AttribValue, &AttribValue, CalculateMagnitudeContext);
	}

	const float SpecLvl = InRelevantSpec.GetLevel();
	FString ContextString = FString::Printf(TEXT("FAttributeBasedFloat::CalculateMagnitude from spec %s"), *InRelevantSpec.ToSimpleString());
	return ((Coefficient.GetValueAtLevel(SpecLvl, &ContextString) * (AttribValue + PreMultiplyAdditiveValue.GetValueAtLevel(SpecLvl, &ContextString))) + PostMultiplyAdditiveValue.GetValueAtLevel(SpecLvl, &ContextString));
}

bool FAttributeBasedFloat::operator==(const FAttributeBasedFloat& Other) const
{
	if (Coefficient != Other.Coefficient ||
		PreMultiplyAdditiveValue != Other.PreMultiplyAdditiveValue ||
		PostMultiplyAdditiveValue != Other.PostMultiplyAdditiveValue ||
		BackingAttribute != Other.BackingAttribute ||
		AttributeCurve != Other.AttributeCurve ||
		AttributeCalculationType != Other.AttributeCalculationType)
	{
		return false;
	}
	if (SourceTagFilter.Num() != Other.SourceTagFilter.Num() ||
		!SourceTagFilter.HasAll(Other.SourceTagFilter))
	{
		return false;
	}
	if (TargetTagFilter.Num() != Other.TargetTagFilter.Num() ||
		!TargetTagFilter.HasAll(Other.TargetTagFilter))
	{
		return false;
	}

	return true;
}

bool FAttributeBasedFloat::operator!=(const FAttributeBasedFloat& Other) const
{
	return !(*this == Other);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FCustomCalculationBasedFloat
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

float FCustomCalculationBasedFloat::CalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec) const
{
	const UGameplayModMagnitudeCalculation* CalcCDO = CalculationClassMagnitude->GetDefaultObject<UGameplayModMagnitudeCalculation>();
	check(CalcCDO);

	float CustomBaseValue = CalcCDO->CalculateBaseMagnitude(InRelevantSpec);

	const float SpecLvl = InRelevantSpec.GetLevel();
	FString ContextString = FString::Printf(TEXT("FCustomCalculationBasedFloat::CalculateMagnitude from effect %s"), *CalcCDO->GetName());

	float FinalValue = ((Coefficient.GetValueAtLevel(SpecLvl, &ContextString) * (CustomBaseValue + PreMultiplyAdditiveValue.GetValueAtLevel(SpecLvl, &ContextString))) + PostMultiplyAdditiveValue.GetValueAtLevel(SpecLvl, &ContextString));
	if (FinalLookupCurve.IsValid(ContextString))
	{
		FinalValue = FinalLookupCurve.Eval(FinalValue, ContextString);
	}

	return FinalValue;
}

/** Equality/Inequality operators */
bool FCustomCalculationBasedFloat::operator==(const FCustomCalculationBasedFloat& Other) const
{
	if (CalculationClassMagnitude != Other.CalculationClassMagnitude)
	{
		return false;
	}
	if (Coefficient != Other.Coefficient ||
		PreMultiplyAdditiveValue != Other.PreMultiplyAdditiveValue ||
		PostMultiplyAdditiveValue != Other.PostMultiplyAdditiveValue)
	{
		return false;
	}

	return true;
}

bool FCustomCalculationBasedFloat::operator!=(const FCustomCalculationBasedFloat& Other) const
{
	return !(*this == Other);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectMagnitude
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

bool FGameplayEffectModifierMagnitude::CanCalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec) const
{
	// Only can calculate magnitude properly if all required capture definitions are fulfilled by the spec
	TArray<FGameplayEffectAttributeCaptureDefinition> ReqCaptureDefs;
	ReqCaptureDefs.Reset();

	GetAttributeCaptureDefinitions(ReqCaptureDefs);

	return InRelevantSpec.HasValidCapturedAttributes(ReqCaptureDefs);
}

bool FGameplayEffectModifierMagnitude::AttemptCalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, bool WarnIfSetByCallerFail, float DefaultSetbyCaller) const
{
	const bool bCanCalc = CanCalculateMagnitude(InRelevantSpec);
	if (bCanCalc)
	{
		FString ContextString = FString::Printf(TEXT("FGameplayEffectModifierMagnitude::AttemptCalculateMagnitude from effect %s"), *InRelevantSpec.ToSimpleString());

		switch (MagnitudeCalculationType)
		{
			case EGameplayEffectMagnitudeCalculation::ScalableFloat:
			{
				OutCalculatedMagnitude = ScalableFloatMagnitude.GetValueAtLevel(InRelevantSpec.GetLevel(), &ContextString);
			}
			break;

			case EGameplayEffectMagnitudeCalculation::AttributeBased:
			{
				OutCalculatedMagnitude = AttributeBasedMagnitude.CalculateMagnitude(InRelevantSpec);
			}
			break;

			case EGameplayEffectMagnitudeCalculation::CustomCalculationClass:
			{
				OutCalculatedMagnitude = CustomMagnitude.CalculateMagnitude(InRelevantSpec);
			}
			break;

			case EGameplayEffectMagnitudeCalculation::SetByCaller:
			{
				if (SetByCallerMagnitude.DataTag.IsValid())
				{
					OutCalculatedMagnitude = InRelevantSpec.GetSetByCallerMagnitude(SetByCallerMagnitude.DataTag, WarnIfSetByCallerFail, DefaultSetbyCaller);
				}
				else
				{
					PRAGMA_DISABLE_DEPRECATION_WARNINGS

					OutCalculatedMagnitude = InRelevantSpec.GetSetByCallerMagnitude(SetByCallerMagnitude.DataName, WarnIfSetByCallerFail, DefaultSetbyCaller);

					PRAGMA_ENABLE_DEPRECATION_WARNINGS
				}
			}
			break;

			default:
				ABILITY_LOG(Error, TEXT("Unknown MagnitudeCalculationType %d in AttemptCalculateMagnitude"), (int32)MagnitudeCalculationType);
				OutCalculatedMagnitude = 0.f;
				break;
		}
	}
	else
	{
		OutCalculatedMagnitude = 0.f;
	}

	return bCanCalc;
}

bool FGameplayEffectModifierMagnitude::AttemptRecalculateMagnitudeFromDependentAggregatorChange(const FGameplayEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, const FAggregator* ChangedAggregator) const
{
	TArray<FGameplayEffectAttributeCaptureDefinition > ReqCaptureDefs;
	ReqCaptureDefs.Reset();

	GetAttributeCaptureDefinitions(ReqCaptureDefs);

	// We could have many potential captures. If a single one matches our criteria, then we call AttemptCalculateMagnitude once and return.
	for (const FGameplayEffectAttributeCaptureDefinition& CaptureDef : ReqCaptureDefs)
	{
		if (CaptureDef.bSnapshot == false)
		{
			const FGameplayEffectAttributeCaptureSpec* CapturedSpec = InRelevantSpec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(CaptureDef, true);
			if (CapturedSpec && CapturedSpec->ShouldRefreshLinkedAggregator(ChangedAggregator))
			{
				return AttemptCalculateMagnitude(InRelevantSpec, OutCalculatedMagnitude);
			}
		}
	}

	return false;
}

void FGameplayEffectModifierMagnitude::GetAttributeCaptureDefinitions(OUT TArray<FGameplayEffectAttributeCaptureDefinition>& OutCaptureDefs) const
{
	OutCaptureDefs.Empty();

	switch (MagnitudeCalculationType)
	{
		case EGameplayEffectMagnitudeCalculation::AttributeBased:
		{
			OutCaptureDefs.Add(AttributeBasedMagnitude.BackingAttribute);
		}
		break;

		case EGameplayEffectMagnitudeCalculation::CustomCalculationClass:
		{
			if (CustomMagnitude.CalculationClassMagnitude)
			{
				const UGameplayModMagnitudeCalculation* CalcCDO = CustomMagnitude.CalculationClassMagnitude->GetDefaultObject<UGameplayModMagnitudeCalculation>();
				check(CalcCDO);

				OutCaptureDefs.Append(CalcCDO->GetAttributeCaptureDefinitions());
			}
		}
		break;
	}
}

bool FGameplayEffectModifierMagnitude::GetStaticMagnitudeIfPossible(float InLevel, float& OutMagnitude, const FString* ContextString) const
{
	if (MagnitudeCalculationType == EGameplayEffectMagnitudeCalculation::ScalableFloat)
	{
		OutMagnitude = ScalableFloatMagnitude.GetValueAtLevel(InLevel, ContextString);
		return true;
	}

	return false;
}

bool FGameplayEffectModifierMagnitude::GetSetByCallerDataNameIfPossible(FName& OutDataName) const
{
	if (MagnitudeCalculationType == EGameplayEffectMagnitudeCalculation::SetByCaller)
	{
		OutDataName = SetByCallerMagnitude.DataName;
		return true;
	}

	return false;
}

TSubclassOf<UGameplayModMagnitudeCalculation> FGameplayEffectModifierMagnitude::GetCustomMagnitudeCalculationClass() const
{
	TSubclassOf<UGameplayModMagnitudeCalculation> CustomCalcClass = nullptr;

	if (MagnitudeCalculationType == EGameplayEffectMagnitudeCalculation::CustomCalculationClass)
	{
		CustomCalcClass = CustomMagnitude.CalculationClassMagnitude;
	}

	return CustomCalcClass;
}

bool FGameplayEffectModifierMagnitude::operator==(const FGameplayEffectModifierMagnitude& Other) const
{
	if (MagnitudeCalculationType != Other.MagnitudeCalculationType)
	{
		return false;
	}

	switch (MagnitudeCalculationType)
	{
	case EGameplayEffectMagnitudeCalculation::ScalableFloat:
		if (ScalableFloatMagnitude != Other.ScalableFloatMagnitude)
		{
			return false;
		}
		break;
	case EGameplayEffectMagnitudeCalculation::AttributeBased:
		if (AttributeBasedMagnitude != Other.AttributeBasedMagnitude)
		{
			return false;
		}
		break;
	case EGameplayEffectMagnitudeCalculation::CustomCalculationClass:
		if (CustomMagnitude != Other.CustomMagnitude)
		{
			return false;
		}
		break;
	case EGameplayEffectMagnitudeCalculation::SetByCaller:
		if (SetByCallerMagnitude.DataName != Other.SetByCallerMagnitude.DataName)
		{
			return false;
		}
		break;
	}

	return true;
}

bool FGameplayEffectModifierMagnitude::operator!=(const FGameplayEffectModifierMagnitude& Other) const
{
	return !(*this == Other);
}

#if WITH_EDITOR
FText FGameplayEffectModifierMagnitude::GetValueForEditorDisplay() const
{
	switch (MagnitudeCalculationType)
	{
		case EGameplayEffectMagnitudeCalculation::ScalableFloat:
			return FText::Format(NSLOCTEXT("GameplayEffect", "ScalableFloatModifierMagnitude", "{0} s"), FText::AsNumber(ScalableFloatMagnitude.Value));
			
		case EGameplayEffectMagnitudeCalculation::AttributeBased:
			return NSLOCTEXT("GameplayEffect", "AttributeBasedModifierMagnitude", "Attribute Based");

		case EGameplayEffectMagnitudeCalculation::CustomCalculationClass:
			return NSLOCTEXT("GameplayEffect", "CustomCalculationClassModifierMagnitude", "Custom Calculation");

		case EGameplayEffectMagnitudeCalculation::SetByCaller:
			return NSLOCTEXT("GameplayEffect", "SetByCallerModifierMagnitude", "Set by Caller");
	}

	return NSLOCTEXT("GameplayEffect", "UnknownModifierMagnitude", "Unknown");
}

void FGameplayEffectModifierMagnitude::ReportErrors(const FString& PathName) const
{
	if (MagnitudeCalculationType == EGameplayEffectMagnitudeCalculation::ScalableFloat)
	{
		GETCURVE_REPORTERROR_WITHPATHNAME(ScalableFloatMagnitude.Curve, PathName);
	}
	else if (MagnitudeCalculationType == EGameplayEffectMagnitudeCalculation::AttributeBased)
	{
		GETCURVE_REPORTERROR_WITHPATHNAME(AttributeBasedMagnitude.Coefficient.Curve, PathName);
		GETCURVE_REPORTERROR_WITHPATHNAME(AttributeBasedMagnitude.PreMultiplyAdditiveValue.Curve, PathName);
		GETCURVE_REPORTERROR_WITHPATHNAME(AttributeBasedMagnitude.PostMultiplyAdditiveValue.Curve, PathName);
	}
	else if (MagnitudeCalculationType == EGameplayEffectMagnitudeCalculation::CustomCalculationClass)
	{
		GETCURVE_REPORTERROR_WITHPATHNAME(CustomMagnitude.Coefficient.Curve, PathName);
		GETCURVE_REPORTERROR_WITHPATHNAME(CustomMagnitude.PreMultiplyAdditiveValue.Curve, PathName);
		GETCURVE_REPORTERROR_WITHPATHNAME(CustomMagnitude.PostMultiplyAdditiveValue.Curve, PathName);
	}
}
#endif // WITH_EDITOR


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectExecutionDefinition
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FGameplayEffectExecutionDefinition::GetAttributeCaptureDefinitions(OUT TArray<FGameplayEffectAttributeCaptureDefinition>& OutCaptureDefs) const
{
	OutCaptureDefs.Empty();

	if (CalculationClass)
	{
		const UGameplayEffectExecutionCalculation* CalculationCDO = Cast<UGameplayEffectExecutionCalculation>(CalculationClass->ClassDefaultObject);
		check(CalculationCDO);

		OutCaptureDefs.Append(CalculationCDO->GetAttributeCaptureDefinitions());
	}

	// Scoped modifiers might have custom magnitude calculations, requiring additional captured attributes
	for (const FGameplayEffectExecutionScopedModifierInfo& CurScopedMod : CalculationModifiers)
	{
		TArray<FGameplayEffectAttributeCaptureDefinition> ScopedModMagDefs;
		CurScopedMod.ModifierMagnitude.GetAttributeCaptureDefinitions(ScopedModMagDefs);

		OutCaptureDefs.Append(ScopedModMagDefs);
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FConditionalGameplayEffect
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

bool FConditionalGameplayEffect::CanApply(const FGameplayTagContainer& SourceTags, float SourceLevel) const
{
	// Right now we're just using the tags but in the future we may gate this by source level as well
	return SourceTags.HasAll(RequiredSourceTags);
}

FGameplayEffectSpecHandle FConditionalGameplayEffect::CreateSpec(FGameplayEffectContextHandle EffectContext, float SourceLevel) const
{
	const UGameplayEffect* EffectCDO = EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	return EffectCDO ? FGameplayEffectSpecHandle(new FGameplayEffectSpec(EffectCDO, EffectContext, SourceLevel)) : FGameplayEffectSpecHandle();
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectSpec
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FGameplayEffectSpec::FGameplayEffectSpec()
	: Def(nullptr)
	, Duration(UGameplayEffect::INSTANT_APPLICATION)
	, Period(UGameplayEffect::NO_PERIOD)
	, ChanceToApplyToTarget(1.f)
	, StackCount(1)
	, bCompletedSourceAttributeCapture(false)
	, bCompletedTargetAttributeCapture(false)
	, bDurationLocked(false)
	, Level(UGameplayEffect::INVALID_LEVEL)
{
}

FGameplayEffectSpec::FGameplayEffectSpec(const UGameplayEffect* InDef, const FGameplayEffectContextHandle& InEffectContext, float InLevel)
	: Def(InDef)
	, Duration(UGameplayEffect::INSTANT_APPLICATION)
	, Period(UGameplayEffect::NO_PERIOD)
	, ChanceToApplyToTarget(1.f)
	, StackCount(1)
	, bCompletedSourceAttributeCapture(false)
	, bCompletedTargetAttributeCapture(false)
	, bDurationLocked(false)
{
	Initialize(InDef, InEffectContext, InLevel);
}

FGameplayEffectSpec::FGameplayEffectSpec(const FGameplayEffectSpec& Other)
{
	*this = Other;
}

FGameplayEffectSpec::FGameplayEffectSpec(const FGameplayEffectSpec& Other, const FGameplayEffectContextHandle& InEffectContext)
{
	*this = Other;
	EffectContext = InEffectContext;
}

FGameplayEffectSpec::FGameplayEffectSpec(FGameplayEffectSpec&& Other)
	: Def(Other.Def)
	, ModifiedAttributes(MoveTemp(Other.ModifiedAttributes))
	, CapturedRelevantAttributes(MoveTemp(Other.CapturedRelevantAttributes))
	, TargetEffectSpecs(MoveTemp(Other.TargetEffectSpecs))
	, Duration(Other.Duration)
	, Period(Other.Period)
	, ChanceToApplyToTarget(Other.ChanceToApplyToTarget)
	, CapturedSourceTags(MoveTemp(Other.CapturedSourceTags))
	, CapturedTargetTags(MoveTemp(Other.CapturedTargetTags))
	, DynamicGrantedTags(MoveTemp(Other.DynamicGrantedTags))
	, DynamicAssetTags(MoveTemp(Other.DynamicAssetTags))
	, Modifiers(MoveTemp(Other.Modifiers))
	, StackCount(Other.StackCount)
	, bCompletedSourceAttributeCapture(Other.bCompletedSourceAttributeCapture)
	, bCompletedTargetAttributeCapture(Other.bCompletedTargetAttributeCapture)
	, bDurationLocked(Other.bDurationLocked)
	, GrantedAbilitySpecs(MoveTemp(Other.GrantedAbilitySpecs))
	, SetByCallerNameMagnitudes(MoveTemp(Other.SetByCallerNameMagnitudes))
	, SetByCallerTagMagnitudes(MoveTemp(Other.SetByCallerTagMagnitudes))
	, EffectContext(Other.EffectContext)
	, Level(Other.Level)
{

}

FGameplayEffectSpec& FGameplayEffectSpec::operator=(FGameplayEffectSpec&& Other)
{
	Def = Other.Def;
	ModifiedAttributes = MoveTemp(Other.ModifiedAttributes);
	CapturedRelevantAttributes = MoveTemp(Other.CapturedRelevantAttributes);
	TargetEffectSpecs = MoveTemp(Other.TargetEffectSpecs);
	Duration = Other.Duration;
	Period = Other.Period;
	ChanceToApplyToTarget = Other.ChanceToApplyToTarget;
	CapturedSourceTags = MoveTemp(Other.CapturedSourceTags);
	CapturedTargetTags = MoveTemp(Other.CapturedTargetTags);
	DynamicGrantedTags = MoveTemp(Other.DynamicGrantedTags);
	DynamicAssetTags = MoveTemp(Other.DynamicAssetTags);
	Modifiers = MoveTemp(Other.Modifiers);
	StackCount = Other.StackCount;
	bCompletedSourceAttributeCapture = Other.bCompletedSourceAttributeCapture;
	bCompletedTargetAttributeCapture = Other.bCompletedTargetAttributeCapture;
	bDurationLocked = Other.bDurationLocked;
	GrantedAbilitySpecs = MoveTemp(Other.GrantedAbilitySpecs);
	SetByCallerNameMagnitudes = MoveTemp(Other.SetByCallerNameMagnitudes);
	SetByCallerTagMagnitudes = MoveTemp(Other.SetByCallerTagMagnitudes);
	EffectContext = Other.EffectContext;
	Level = Other.Level;
	return *this;
}

FGameplayEffectSpec& FGameplayEffectSpec::operator=(const FGameplayEffectSpec& Other)
{
	Def = Other.Def;
	ModifiedAttributes = Other.ModifiedAttributes;
	CapturedRelevantAttributes = Other.CapturedRelevantAttributes;
	TargetEffectSpecs = Other.TargetEffectSpecs;
	Duration = Other.Duration;
	Period = Other.Period;
	ChanceToApplyToTarget = Other.ChanceToApplyToTarget;
	CapturedSourceTags = Other.CapturedSourceTags;
	CapturedTargetTags = Other.CapturedTargetTags;
	DynamicGrantedTags = Other.DynamicGrantedTags;
	DynamicAssetTags = Other.DynamicAssetTags;
	Modifiers = Other.Modifiers;
	StackCount = Other.StackCount;
	bCompletedSourceAttributeCapture = Other.bCompletedSourceAttributeCapture;
	bCompletedTargetAttributeCapture = Other.bCompletedTargetAttributeCapture;
	bDurationLocked = Other.bDurationLocked;
	GrantedAbilitySpecs =  Other.GrantedAbilitySpecs;
	SetByCallerNameMagnitudes = Other.SetByCallerNameMagnitudes;
	SetByCallerTagMagnitudes = Other.SetByCallerTagMagnitudes;
	EffectContext = Other.EffectContext;
	Level = Other.Level;
	return *this;
}

FGameplayEffectSpecForRPC::FGameplayEffectSpecForRPC()
	: Def(nullptr)
	, Level(UGameplayEffect::INVALID_LEVEL)
	, AbilityLevel(1)
{
}

FGameplayEffectSpecForRPC::FGameplayEffectSpecForRPC(const FGameplayEffectSpec& InSpec)
	: Def(InSpec.Def)
	, ModifiedAttributes()
	, EffectContext(InSpec.GetEffectContext())
	, AggregatedSourceTags(*InSpec.CapturedSourceTags.GetAggregatedTags())
	, AggregatedTargetTags(*InSpec.CapturedTargetTags.GetAggregatedTags())
	, Level(InSpec.GetLevel())
	, AbilityLevel(InSpec.GetEffectContext().GetAbilityLevel())
{
	// Only copy attributes that are in the gameplay cue info
	for (int32 i = InSpec.ModifiedAttributes.Num() - 1; i >= 0; i--)
	{
		if (Def)
		{
			for (const FGameplayEffectCue& CueInfo : Def->GameplayCues)
			{
				if (CueInfo.MagnitudeAttribute == InSpec.ModifiedAttributes[i].Attribute)
				{
					ModifiedAttributes.Add(InSpec.ModifiedAttributes[i]);
				}
			}
		}
	}
}

void FGameplayEffectSpec::Initialize(const UGameplayEffect* InDef, const FGameplayEffectContextHandle& InEffectContext, float InLevel)
{
	Def = InDef;
	check(Def);	
	SetLevel(InLevel);
	SetContext(InEffectContext);

	// Init our ModifierSpecs
	Modifiers.SetNum(Def->Modifiers.Num());

	// Prep the spec with all of the attribute captures it will need to perform
	SetupAttributeCaptureDefinitions();
	
	// Add the GameplayEffect asset tags to the source Spec tags
	CapturedSourceTags.GetSpecTags().AppendTags(InDef->InheritableGameplayEffectTags.CombinedTags);

	// Prepare source tags before accessing them in ConditionalGameplayEffects
	CaptureDataFromSource();

	// ------------------------------------------------
	//	Linked/Dependant Specs
	// ------------------------------------------------


	for (const FConditionalGameplayEffect& ConditionalEffect : InDef->ConditionalGameplayEffects)
	{
		if (ConditionalEffect.CanApply(CapturedSourceTags.GetActorTags(), InLevel))
		{
			FGameplayEffectSpecHandle SpecHandle = ConditionalEffect.CreateSpec(EffectContext, InLevel);
			if (SpecHandle.IsValid())
			{
				TargetEffectSpecs.Add(SpecHandle);
			}
		}
	}

	// ------------------------------------------------
	//	Granted Abilities
	// ------------------------------------------------

	// Make Granted AbilitySpecs (caller may modify these specs after creating spec, which is why we dont just reference them from the def)
	GrantedAbilitySpecs = InDef->GrantedAbilities;

	// if we're granting abilities and they don't specify a source object use the source of this GE
	for (FGameplayAbilitySpecDef& AbilitySpecDef : GrantedAbilitySpecs)
	{
		if (AbilitySpecDef.SourceObject == nullptr)
		{
			AbilitySpecDef.SourceObject = InEffectContext.GetSourceObject();
		}
	}
}

void FGameplayEffectSpec::InitializeFromLinkedSpec(const UGameplayEffect* InDef, const FGameplayEffectSpec& OriginalSpec)
{
	// We need to manually initialize the new GE spec. We want to pass on all of the tags from the originating GE *Except* for that GE's asset tags. (InheritableGameplayEffectTags).
	// But its very important that the ability tags and anything else that was added to the source tags in the originating GE carries over

	// Duplicate GE context
	const FGameplayEffectContextHandle& ExpiringSpecContextHandle = OriginalSpec.GetEffectContext();
	FGameplayEffectContextHandle NewContextHandle = ExpiringSpecContextHandle.Duplicate();

	// Make a full copy
	CapturedSourceTags = OriginalSpec.CapturedSourceTags;

	// But then remove the tags the originating GE added
	CapturedSourceTags.GetSpecTags().RemoveTags( OriginalSpec.Def->InheritableGameplayEffectTags.CombinedTags );

	// Now initialize like the normal cstor would have. Note that this will add the new GE's asset tags (in case they were removed in the line above / e.g., shared asset tags with the originating GE)					
	Initialize(InDef, NewContextHandle, OriginalSpec.GetLevel());
}

void FGameplayEffectSpec::SetupAttributeCaptureDefinitions()
{
	// Add duration if required
	if (Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
	{
		CapturedRelevantAttributes.AddCaptureDefinition(UAbilitySystemComponent::GetOutgoingDurationCapture());
		CapturedRelevantAttributes.AddCaptureDefinition(UAbilitySystemComponent::GetIncomingDurationCapture());
	}

	TArray<FGameplayEffectAttributeCaptureDefinition> CaptureDefs;

	// Gather capture definitions from duration	
	{
		CaptureDefs.Reset();
		Def->DurationMagnitude.GetAttributeCaptureDefinitions(CaptureDefs);
		for (const FGameplayEffectAttributeCaptureDefinition& CurDurationCaptureDef : CaptureDefs)
		{
			CapturedRelevantAttributes.AddCaptureDefinition(CurDurationCaptureDef);
		}
	}

	// Gather all capture definitions from modifiers
	for (int32 ModIdx = 0; ModIdx < Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Def->Modifiers[ModIdx];
		const FModifierSpec& ModSpec = Modifiers[ModIdx];

		CaptureDefs.Reset();
		ModDef.ModifierMagnitude.GetAttributeCaptureDefinitions(CaptureDefs);
		
		for (const FGameplayEffectAttributeCaptureDefinition& CurCaptureDef : CaptureDefs)
		{
			CapturedRelevantAttributes.AddCaptureDefinition(CurCaptureDef);
		}
	}

	// Gather all capture definitions from executions
	for (const FGameplayEffectExecutionDefinition& Exec : Def->Executions)
	{
		CaptureDefs.Reset();
		Exec.GetAttributeCaptureDefinitions(CaptureDefs);
		for (const FGameplayEffectAttributeCaptureDefinition& CurExecCaptureDef : CaptureDefs)
		{
			CapturedRelevantAttributes.AddCaptureDefinition(CurExecCaptureDef);
		}
	}
}

void FGameplayEffectSpec::CaptureAttributeDataFromTarget(UAbilitySystemComponent* TargetAbilitySystemComponent)
{
	CapturedRelevantAttributes.CaptureAttributes(TargetAbilitySystemComponent, EGameplayEffectAttributeCaptureSource::Target);
	bCompletedTargetAttributeCapture = true;
}

void FGameplayEffectSpec::CaptureDataFromSource()
{
	// Capture source actor tags
	RecaptureSourceActorTags();

	// Capture source Attributes
	// Is this the right place to do it? Do we ever need to create spec and capture attributes at a later time? If so, this will need to move.
	CapturedRelevantAttributes.CaptureAttributes(EffectContext.GetInstigatorAbilitySystemComponent(), EGameplayEffectAttributeCaptureSource::Source);

	// Now that we have source attributes captured, re-evaluate the duration since it could be based on the captured attributes.
	float DefCalcDuration = 0.f;
	if (AttemptCalculateDurationFromDef(DefCalcDuration))
	{
		SetDuration(DefCalcDuration, false);
	}

	bCompletedSourceAttributeCapture = true;
}

void FGameplayEffectSpec::RecaptureSourceActorTags()
{
	CapturedSourceTags.GetActorTags().Reset();
	EffectContext.GetOwnedGameplayTags(CapturedSourceTags.GetActorTags(), CapturedSourceTags.GetSpecTags());
}

bool FGameplayEffectSpec::AttemptCalculateDurationFromDef(OUT float& OutDefDuration) const
{
	check(Def);

	bool bCalculatedDuration = true;

	const EGameplayEffectDurationType DurType = Def->DurationPolicy;
	if (DurType == EGameplayEffectDurationType::Infinite)
	{
		OutDefDuration = UGameplayEffect::INFINITE_DURATION;
	}
	else if (DurType == EGameplayEffectDurationType::Instant)
	{
		OutDefDuration = UGameplayEffect::INSTANT_APPLICATION;
	}
	else
	{
		// The last parameters (false, 1.f) are so that if SetByCaller hasn't been set yet, we don't warn and default
		// to 1.f. This is so that the rest of the system doesn't treat the effect as an instant effect. 1.f is arbitrary
		// and this makes it illegal to SetByCaller something into an instant effect.
		bCalculatedDuration = Def->DurationMagnitude.AttemptCalculateMagnitude(*this, OutDefDuration, false, 1.f);
	}

	return bCalculatedDuration;
}

void FGameplayEffectSpec::SetLevel(float InLevel)
{
	Level = InLevel;
	if (Def)
	{
		float DefCalcDuration = 0.f;
		if (AttemptCalculateDurationFromDef(DefCalcDuration))
		{
			SetDuration(DefCalcDuration, false);
		}

		FString ContextString = FString::Printf(TEXT("FGameplayEffectSpec::SetLevel from effect %s"), *Def->GetName());
		Period = Def->Period.GetValueAtLevel(InLevel, &ContextString);
		ChanceToApplyToTarget = Def->ChanceToApplyToTarget.GetValueAtLevel(InLevel, &ContextString);
	}
}

float FGameplayEffectSpec::GetLevel() const
{
	return Level;
}

float FGameplayEffectSpec::GetDuration() const
{
	return Duration;
}

void FGameplayEffectSpec::SetDuration(float NewDuration, bool bLockDuration)
{
	if (!bDurationLocked)
	{
		Duration = NewDuration;
		bDurationLocked = bLockDuration;
		if (Duration > 0.f)
		{
			// We may have potential problems one day if a game is applying duration based gameplay effects from instantaneous effects
			// (E.g., every time fire damage is applied, a DOT is also applied). We may need to for Duration to always be captured.
			CapturedRelevantAttributes.AddCaptureDefinition(UAbilitySystemComponent::GetOutgoingDurationCapture());
		}
	}
}

float FGameplayEffectSpec::CalculateModifiedDuration() const
{
	FAggregator DurationAgg;

	const FGameplayEffectAttributeCaptureSpec* OutgoingCaptureSpec = CapturedRelevantAttributes.FindCaptureSpecByDefinition(UAbilitySystemComponent::GetOutgoingDurationCapture(), true);
	if (OutgoingCaptureSpec)
	{
		OutgoingCaptureSpec->AttemptAddAggregatorModsToAggregator(DurationAgg);
	}

	const FGameplayEffectAttributeCaptureSpec* IncomingCaptureSpec = CapturedRelevantAttributes.FindCaptureSpecByDefinition(UAbilitySystemComponent::GetIncomingDurationCapture(), true);
	if (IncomingCaptureSpec)
	{
		IncomingCaptureSpec->AttemptAddAggregatorModsToAggregator(DurationAgg);
	}

	FAggregatorEvaluateParameters Params;
	Params.SourceTags = CapturedSourceTags.GetAggregatedTags();
	Params.TargetTags = CapturedTargetTags.GetAggregatedTags();

	return DurationAgg.EvaluateWithBase(GetDuration(), Params);
}

float FGameplayEffectSpec::GetPeriod() const
{
	return Period;
}

float FGameplayEffectSpec::GetChanceToApplyToTarget() const
{
	return ChanceToApplyToTarget;
}

float FGameplayEffectSpec::GetModifierMagnitude(int32 ModifierIdx, bool bFactorInStackCount) const
{
	check(Modifiers.IsValidIndex(ModifierIdx) && Def && Def->Modifiers.IsValidIndex(ModifierIdx));

	const float SingleEvaluatedMagnitude = Modifiers[ModifierIdx].GetEvaluatedMagnitude();

	float ModMagnitude = SingleEvaluatedMagnitude;
	if (bFactorInStackCount)
	{
		ModMagnitude = GameplayEffectUtilities::ComputeStackedModifierMagnitude(SingleEvaluatedMagnitude, StackCount, Def->Modifiers[ModifierIdx].ModifierOp);
	}

	return ModMagnitude;
}

void FGameplayEffectSpec::CalculateModifierMagnitudes()
{
	for(int32 ModIdx = 0; ModIdx < Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Def->Modifiers[ModIdx];
		FModifierSpec& ModSpec = Modifiers[ModIdx];

		if (ModDef.ModifierMagnitude.AttemptCalculateMagnitude(*this, ModSpec.EvaluatedMagnitude) == false)
		{
			ModSpec.EvaluatedMagnitude = 0.f;
			ABILITY_LOG(Warning, TEXT("Modifier on spec: %s was asked to CalculateMagnitude and failed, falling back to 0."), *ToSimpleString());
		}
	}
}

bool FGameplayEffectSpec::HasValidCapturedAttributes(const TArray<FGameplayEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const
{
	return CapturedRelevantAttributes.HasValidCapturedAttributes(InCaptureDefsToCheck);
}

void FGameplayEffectSpec::RecaptureAttributeDataForClone(UAbilitySystemComponent* OriginalASC, UAbilitySystemComponent* NewASC)
{
	if (bCompletedSourceAttributeCapture == false)
	{
		// Only do this if we are the source
		if (EffectContext.GetInstigatorAbilitySystemComponent() == OriginalASC)
		{
			// Flip the effect context
			EffectContext.AddInstigator(NewASC->GetOwner(), EffectContext.GetEffectCauser());
			CaptureDataFromSource();
		}
	}

	if (bCompletedTargetAttributeCapture == false)
	{
		CaptureAttributeDataFromTarget(NewASC);
	}
}

const FGameplayEffectModifiedAttribute* FGameplayEffectSpec::GetModifiedAttribute(const FGameplayAttribute& Attribute) const
{
	for (const FGameplayEffectModifiedAttribute& ModifiedAttribute : ModifiedAttributes)
	{
		if (ModifiedAttribute.Attribute == Attribute)
		{
			return &ModifiedAttribute;
		}
	}
	return NULL;
}

FGameplayEffectModifiedAttribute* FGameplayEffectSpec::GetModifiedAttribute(const FGameplayAttribute& Attribute)
{
	for (FGameplayEffectModifiedAttribute& ModifiedAttribute : ModifiedAttributes)
	{
		if (ModifiedAttribute.Attribute == Attribute)
		{
			return &ModifiedAttribute;
		}
	}
	return NULL;
}

const FGameplayEffectModifiedAttribute* FGameplayEffectSpecForRPC::GetModifiedAttribute(const FGameplayAttribute& Attribute) const
{
	for (const FGameplayEffectModifiedAttribute& ModifiedAttribute : ModifiedAttributes)
	{
		if (ModifiedAttribute.Attribute == Attribute)
		{
			return &ModifiedAttribute;
		}
	}
	return NULL;
}

FString FGameplayEffectSpecForRPC::ToSimpleString() const
{
	return FString::Printf(TEXT("%s"), *Def->GetName());
}

FGameplayEffectModifiedAttribute* FGameplayEffectSpec::AddModifiedAttribute(const FGameplayAttribute& Attribute)
{
	FGameplayEffectModifiedAttribute NewAttribute;
	NewAttribute.Attribute = Attribute;
	return &ModifiedAttributes[ModifiedAttributes.Add(NewAttribute)];
}

void FGameplayEffectSpec::SetContext(FGameplayEffectContextHandle NewEffectContext)
{
	bool bWasAlreadyInit = EffectContext.IsValid();
	EffectContext = NewEffectContext;	
	if (bWasAlreadyInit)
	{
		CaptureDataFromSource();
	}
}

void FGameplayEffectSpec::GetAllGrantedTags(OUT FGameplayTagContainer& Container) const
{
	Container.AppendTags(DynamicGrantedTags);
	if (Def)
	{
		Container.AppendTags(Def->InheritableOwnedTagsContainer.CombinedTags);
	}
}

void FGameplayEffectSpec::GetAllAssetTags(OUT FGameplayTagContainer& Container) const
{
	Container.AppendTags(DynamicAssetTags);
	if (Def)
	{
		Container.AppendTags(Def->InheritableGameplayEffectTags.CombinedTags);
	}
}


void FGameplayEffectSpec::SetSetByCallerMagnitude(FName DataName, float Magnitude)
{
	if (DataName != NAME_None)
	{
		SetByCallerNameMagnitudes.FindOrAdd(DataName) = Magnitude;
	}
}

void FGameplayEffectSpec::SetSetByCallerMagnitude(FGameplayTag DataTag, float Magnitude)
{
	if (DataTag.IsValid())
	{
		SetByCallerTagMagnitudes.FindOrAdd(DataTag) = Magnitude;
	}
}

float FGameplayEffectSpec::GetSetByCallerMagnitude(FName DataName, bool WarnIfNotFound, float DefaultIfNotFound) const
{
	float Magnitude = DefaultIfNotFound;
	const float* Ptr = nullptr;
	
	if (DataName != NAME_None)
	{
		Ptr = SetByCallerNameMagnitudes.Find(DataName);
	}
	
	if (Ptr)
	{
		Magnitude = *Ptr;
	}
	else if (WarnIfNotFound)
	{
		ABILITY_LOG(Error, TEXT("FGameplayEffectSpec::GetMagnitude called for Data %s on Def %s when magnitude had not yet been set by caller."), *DataName.ToString(), *Def->GetName());
	}

	return Magnitude;
}

float FGameplayEffectSpec::GetSetByCallerMagnitude(FGameplayTag DataTag, bool WarnIfNotFound, float DefaultIfNotFound) const
{
	float Magnitude = DefaultIfNotFound;
	const float* Ptr = nullptr;

	if (DataTag.IsValid())
	{
		Ptr = SetByCallerTagMagnitudes.Find(DataTag);
	}

	if (Ptr)
	{
		Magnitude = *Ptr;
	}
	else if (WarnIfNotFound)
	{
		ABILITY_LOG(Error, TEXT("FGameplayEffectSpec::GetMagnitude called for Data %s on Def %s when magnitude had not yet been set by caller."), *DataTag.ToString(), *Def->GetName());
	}

	return Magnitude;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectAttributeCaptureSpec
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FGameplayEffectAttributeCaptureSpec::FGameplayEffectAttributeCaptureSpec()
{
}

FGameplayEffectAttributeCaptureSpec::FGameplayEffectAttributeCaptureSpec(const FGameplayEffectAttributeCaptureDefinition& InDefinition)
	: BackingDefinition(InDefinition)
{
}

bool FGameplayEffectAttributeCaptureSpec::HasValidCapture() const
{
	return (AttributeAggregator.Get() != nullptr);
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutMagnitude = Agg->Evaluate(InEvalParams);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeMagnitudeUpToChannel(const FAggregatorEvaluateParameters& InEvalParams, EGameplayModEvaluationChannel FinalChannel, OUT float& OutMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutMagnitude = Agg->EvaluateToChannel(InEvalParams, FinalChannel);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeMagnitudeWithBase(const FAggregatorEvaluateParameters& InEvalParams, float InBaseValue, OUT float& OutMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutMagnitude = Agg->EvaluateWithBase(InBaseValue, InEvalParams);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeBaseValue(OUT float& OutBaseValue) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutBaseValue = Agg->GetBaseValue();
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeBonusMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutBonusMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutBonusMagnitude = Agg->EvaluateBonus(InEvalParams);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeContributionMagnitude(const FAggregatorEvaluateParameters& InEvalParams, FActiveGameplayEffectHandle ActiveHandle, OUT float& OutBonusMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg && ActiveHandle.IsValid())
	{
		OutBonusMagnitude = Agg->EvaluateContribution(InEvalParams, ActiveHandle);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptGetAttributeAggregatorSnapshot(OUT FAggregator& OutAggregatorSnapshot) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutAggregatorSnapshot.TakeSnapshotOf(*Agg);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptAddAggregatorModsToAggregator(OUT FAggregator& OutAggregatorToAddTo) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutAggregatorToAddTo.AddModsFrom(*Agg);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptGatherAttributeMods(OUT TMap<EGameplayModEvaluationChannel, const TArray<FAggregatorMod>*>& OutModMap) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		Agg->GetAllAggregatorMods(OutModMap);
		return true;
	}

	return false;
}

void FGameplayEffectAttributeCaptureSpec::RegisterLinkedAggregatorCallback(FActiveGameplayEffectHandle Handle) const
{
	if (BackingDefinition.bSnapshot == false)
	{
		// Its possible the linked Aggregator is already gone.
		FAggregator* Agg = AttributeAggregator.Get();
		if (Agg)
		{
			Agg->AddDependent(Handle);
		}
	}
}

void FGameplayEffectAttributeCaptureSpec::UnregisterLinkedAggregatorCallback(FActiveGameplayEffectHandle Handle) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		Agg->RemoveDependent(Handle);
	}
}

bool FGameplayEffectAttributeCaptureSpec::ShouldRefreshLinkedAggregator(const FAggregator* ChangedAggregator) const
{
	return (BackingDefinition.bSnapshot == false && (ChangedAggregator == nullptr || AttributeAggregator.Get() == ChangedAggregator));
}

void FGameplayEffectAttributeCaptureSpec::SwapAggregator(FAggregatorRef From, FAggregatorRef To)
{
	if (AttributeAggregator.Get() == From.Get())
	{
		AttributeAggregator = To;
	}
}

const FGameplayEffectAttributeCaptureDefinition& FGameplayEffectAttributeCaptureSpec::GetBackingDefinition() const
{
	return BackingDefinition;
}

FGameplayEffectAttributeCaptureSpecContainer::FGameplayEffectAttributeCaptureSpecContainer()
	: bHasNonSnapshottedAttributes(false)
{
}

FGameplayEffectAttributeCaptureSpecContainer::FGameplayEffectAttributeCaptureSpecContainer(FGameplayEffectAttributeCaptureSpecContainer&& Other)
	: SourceAttributes(MoveTemp(Other.SourceAttributes))
	, TargetAttributes(MoveTemp(Other.TargetAttributes))
	, bHasNonSnapshottedAttributes(Other.bHasNonSnapshottedAttributes)
{
}

FGameplayEffectAttributeCaptureSpecContainer::FGameplayEffectAttributeCaptureSpecContainer(const FGameplayEffectAttributeCaptureSpecContainer& Other)
	: SourceAttributes(Other.SourceAttributes)
	, TargetAttributes(Other.TargetAttributes)
	, bHasNonSnapshottedAttributes(Other.bHasNonSnapshottedAttributes)
{
}

FGameplayEffectAttributeCaptureSpecContainer& FGameplayEffectAttributeCaptureSpecContainer::operator=(FGameplayEffectAttributeCaptureSpecContainer&& Other)
{
	SourceAttributes = MoveTemp(Other.SourceAttributes);
	TargetAttributes = MoveTemp(Other.TargetAttributes);
	bHasNonSnapshottedAttributes = Other.bHasNonSnapshottedAttributes;
	return *this;
}

FGameplayEffectAttributeCaptureSpecContainer& FGameplayEffectAttributeCaptureSpecContainer::operator=(const FGameplayEffectAttributeCaptureSpecContainer& Other)
{
	SourceAttributes = Other.SourceAttributes;
	TargetAttributes = Other.TargetAttributes;
	bHasNonSnapshottedAttributes = Other.bHasNonSnapshottedAttributes;
	return *this;
}

void FGameplayEffectAttributeCaptureSpecContainer::AddCaptureDefinition(const FGameplayEffectAttributeCaptureDefinition& InCaptureDefinition)
{
	const bool bSourceAttribute = (InCaptureDefinition.AttributeSource == EGameplayEffectAttributeCaptureSource::Source);
	TArray<FGameplayEffectAttributeCaptureSpec>& AttributeArray = (bSourceAttribute ? SourceAttributes : TargetAttributes);

	// Only add additional captures if this exact capture definition isn't already being handled
	if (!AttributeArray.ContainsByPredicate(
			[&](const FGameplayEffectAttributeCaptureSpec& Element)
			{
				return Element.GetBackingDefinition() == InCaptureDefinition;
			}))
	{
		AttributeArray.Add(FGameplayEffectAttributeCaptureSpec(InCaptureDefinition));

		if (!InCaptureDefinition.bSnapshot)
		{
			bHasNonSnapshottedAttributes = true;
		}
	}
}

void FGameplayEffectAttributeCaptureSpecContainer::CaptureAttributes(UAbilitySystemComponent* InAbilitySystemComponent, EGameplayEffectAttributeCaptureSource InCaptureSource)
{
	if (InAbilitySystemComponent)
	{
		const bool bSourceComponent = (InCaptureSource == EGameplayEffectAttributeCaptureSource::Source);
		TArray<FGameplayEffectAttributeCaptureSpec>& AttributeArray = (bSourceComponent ? SourceAttributes : TargetAttributes);

		// Capture every spec's requirements from the specified component
		for (FGameplayEffectAttributeCaptureSpec& CurCaptureSpec : AttributeArray)
		{
			InAbilitySystemComponent->CaptureAttributeForGameplayEffect(CurCaptureSpec);
		}
	}
}

const FGameplayEffectAttributeCaptureSpec* FGameplayEffectAttributeCaptureSpecContainer::FindCaptureSpecByDefinition(const FGameplayEffectAttributeCaptureDefinition& InDefinition, bool bOnlyIncludeValidCapture) const
{
	const bool bSourceAttribute = (InDefinition.AttributeSource == EGameplayEffectAttributeCaptureSource::Source);
	const TArray<FGameplayEffectAttributeCaptureSpec>& AttributeArray = (bSourceAttribute ? SourceAttributes : TargetAttributes);

	const FGameplayEffectAttributeCaptureSpec* MatchingSpec = AttributeArray.FindByPredicate(
		[&](const FGameplayEffectAttributeCaptureSpec& Element)
	{
		return Element.GetBackingDefinition() == InDefinition;
	});

	// Null out the found results if the caller only wants valid captures and we don't have one yet
	if (MatchingSpec && bOnlyIncludeValidCapture && !MatchingSpec->HasValidCapture())
	{
		MatchingSpec = nullptr;
	}

	return MatchingSpec;
}

bool FGameplayEffectAttributeCaptureSpecContainer::HasValidCapturedAttributes(const TArray<FGameplayEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const
{
	bool bHasValid = true;

	for (const FGameplayEffectAttributeCaptureDefinition& CurDef : InCaptureDefsToCheck)
	{
		const FGameplayEffectAttributeCaptureSpec* CaptureSpec = FindCaptureSpecByDefinition(CurDef, true);
		if (!CaptureSpec)
		{
			bHasValid = false;
			break;
		}
	}

	return bHasValid;
}

bool FGameplayEffectAttributeCaptureSpecContainer::HasNonSnapshottedAttributes() const
{
	return bHasNonSnapshottedAttributes;
}

void FGameplayEffectAttributeCaptureSpecContainer::RegisterLinkedAggregatorCallbacks(FActiveGameplayEffectHandle Handle) const
{
	for (const FGameplayEffectAttributeCaptureSpec& CaptureSpec : SourceAttributes)
	{
		CaptureSpec.RegisterLinkedAggregatorCallback(Handle);
	}

	for (const FGameplayEffectAttributeCaptureSpec& CaptureSpec : TargetAttributes)
	{
		CaptureSpec.RegisterLinkedAggregatorCallback(Handle);
	}
}

void FGameplayEffectAttributeCaptureSpecContainer::UnregisterLinkedAggregatorCallbacks(FActiveGameplayEffectHandle Handle) const
{
	for (const FGameplayEffectAttributeCaptureSpec& CaptureSpec : SourceAttributes)
	{
		CaptureSpec.UnregisterLinkedAggregatorCallback(Handle);
	}

	for (const FGameplayEffectAttributeCaptureSpec& CaptureSpec : TargetAttributes)
	{
		CaptureSpec.UnregisterLinkedAggregatorCallback(Handle);
	}
}

void FGameplayEffectAttributeCaptureSpecContainer::SwapAggregator(FAggregatorRef From, FAggregatorRef To)
{
	for (FGameplayEffectAttributeCaptureSpec& CaptureSpec : SourceAttributes)
	{
		CaptureSpec.SwapAggregator(From, To);
	}

	for (FGameplayEffectAttributeCaptureSpec& CaptureSpec : TargetAttributes)
	{
		CaptureSpec.SwapAggregator(From, To);
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FActiveGameplayEffect
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FActiveGameplayEffect::FActiveGameplayEffect()
	: StartServerWorldTime(0)
	, CachedStartServerWorldTime(0)
	, StartWorldTime(0.f)
	, bIsInhibited(true)
	, bPendingRepOnActiveGC(false)
	, bPendingRepWhileActiveGC(false)
	, IsPendingRemove(false)
	, ClientCachedStackCount(0)
	, PendingNext(nullptr)
{
}

FActiveGameplayEffect::FActiveGameplayEffect(const FActiveGameplayEffect& Other)
{
	*this = Other;
}

FActiveGameplayEffect::FActiveGameplayEffect(FActiveGameplayEffectHandle InHandle, const FGameplayEffectSpec &InSpec, float CurrentWorldTime, float InStartServerWorldTime, FPredictionKey InPredictionKey)
	: Handle(InHandle)
	, Spec(InSpec)
	, PredictionKey(InPredictionKey)
	, StartServerWorldTime(InStartServerWorldTime)
	, CachedStartServerWorldTime(InStartServerWorldTime)
	, StartWorldTime(CurrentWorldTime)
	, bIsInhibited(true)
	, bPendingRepOnActiveGC(false)
	, bPendingRepWhileActiveGC(false)
	, IsPendingRemove(false)
	, ClientCachedStackCount(0)
	, PendingNext(nullptr)
{
}

FActiveGameplayEffect::FActiveGameplayEffect(FActiveGameplayEffect&& Other)
	:Handle(Other.Handle)
	,Spec(MoveTemp(Other.Spec))
	,PredictionKey(Other.PredictionKey)
	,StartServerWorldTime(Other.StartServerWorldTime)
	,CachedStartServerWorldTime(Other.CachedStartServerWorldTime)
	,StartWorldTime(Other.StartWorldTime)
	,bIsInhibited(Other.bIsInhibited)
	,bPendingRepOnActiveGC(Other.bPendingRepOnActiveGC)
	,bPendingRepWhileActiveGC(Other.bPendingRepWhileActiveGC)
	,IsPendingRemove(Other.IsPendingRemove)
	,ClientCachedStackCount(0)
	,OnRemovedDelegate(Other.OnRemovedDelegate)
	,OnRemoved_InfoDelegate(Other.OnRemoved_InfoDelegate)
	,PeriodHandle(Other.PeriodHandle)
	,DurationHandle(Other.DurationHandle)
{

	ReplicationID = Other.ReplicationID;
	ReplicationKey = Other.ReplicationKey;

	// Note: purposefully not copying PendingNext pointer.
}

FActiveGameplayEffect& FActiveGameplayEffect::operator=(FActiveGameplayEffect&& Other)
{
	Handle = Other.Handle;
	Spec = MoveTemp(Other.Spec);
	PredictionKey = Other.PredictionKey;
	StartServerWorldTime = Other.StartServerWorldTime;
	CachedStartServerWorldTime = Other.CachedStartServerWorldTime;
	StartWorldTime = Other.StartWorldTime;
	bIsInhibited = Other.bIsInhibited;
	bPendingRepOnActiveGC = Other.bPendingRepOnActiveGC;
	bPendingRepWhileActiveGC = Other.bPendingRepWhileActiveGC;
	IsPendingRemove = Other.IsPendingRemove;
	ClientCachedStackCount = Other.ClientCachedStackCount;
	OnRemovedDelegate = Other.OnRemovedDelegate;
	OnRemoved_InfoDelegate = Other.OnRemoved_InfoDelegate;
	PeriodHandle = Other.PeriodHandle;
	DurationHandle = Other.DurationHandle;
	// Note: purposefully not copying PendingNext pointer.

	ReplicationID = Other.ReplicationID;
	ReplicationKey = Other.ReplicationKey;
	return *this;
}

FActiveGameplayEffect& FActiveGameplayEffect::operator=(const FActiveGameplayEffect& Other)
{
	Handle = Other.Handle;
	Spec = Other.Spec;
	PredictionKey = Other.PredictionKey;
	StartServerWorldTime = Other.StartServerWorldTime;
	CachedStartServerWorldTime = Other.CachedStartServerWorldTime;
	StartWorldTime = Other.StartWorldTime;
	bIsInhibited = Other.bIsInhibited;
	bPendingRepOnActiveGC = Other.bPendingRepOnActiveGC;
	bPendingRepWhileActiveGC = Other.bPendingRepWhileActiveGC;
	IsPendingRemove = Other.IsPendingRemove;
	ClientCachedStackCount = Other.ClientCachedStackCount;
	OnRemovedDelegate = Other.OnRemovedDelegate;
	OnRemoved_InfoDelegate = Other.OnRemoved_InfoDelegate;
	PeriodHandle = Other.PeriodHandle;
	DurationHandle = Other.DurationHandle;
	PendingNext = Other.PendingNext;

	ReplicationID = Other.ReplicationID;
	ReplicationKey = Other.ReplicationKey;
	return *this;
}

/** This is the core function that turns the ActiveGE 'on' or 'off */
void FActiveGameplayEffect::CheckOngoingTagRequirements(const FGameplayTagContainer& OwnerTags, FActiveGameplayEffectsContainer& OwningContainer, bool bInvokeGameplayCueEvents)
{
	bool bShouldBeInhibited = !Spec.Def->OngoingTagRequirements.RequirementsMet(OwnerTags);

	if (bIsInhibited != bShouldBeInhibited)
	{
		// All OnDirty callbacks must be inhibited until we update this entire GameplayEffect.
		FScopedAggregatorOnDirtyBatch	AggregatorOnDirtyBatcher;

		// Important to set this prior to adding or removing, so that any delegates that are triggered can query accurately against this GE
		bIsInhibited = bShouldBeInhibited;

		if (bShouldBeInhibited)
		{
			// Remove our ActiveGameplayEffects modifiers with our Attribute Aggregators
			OwningContainer.RemoveActiveGameplayEffectGrantedTagsAndModifiers(*this, bInvokeGameplayCueEvents);
		}
		else
		{
			OwningContainer.AddActiveGameplayEffectGrantedTagsAndModifiers(*this, bInvokeGameplayCueEvents);
		}
	}
}

void FActiveGameplayEffect::PreReplicatedRemove(const struct FActiveGameplayEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received PreReplicatedRemove with no UGameplayEffect def."));
		return;
	}

	ABILITY_LOG(Verbose, TEXT("PreReplicatedRemove: %s %s Marked as Pending Remove: %s"), *Handle.ToString(), *Spec.Def->GetName(), IsPendingRemove ? TEXT("TRUE") : TEXT("FALSE"));

	FGameplayEffectRemovalInfo GameplayEffectRemovalInfo;
	GameplayEffectRemovalInfo.StackCount = ClientCachedStackCount;
	//Check duration to set bPrematureRemoval as req.
	if (DurationHandle.IsValid())
	{
		float SecondsRemaining = GetTimeRemaining(const_cast<FActiveGameplayEffectsContainer&>(InArray).GetWorldTime());

		if (SecondsRemaining > 0.f)
		{
			GameplayEffectRemovalInfo.bPrematureRemoval = true;
		}
	}
	GameplayEffectRemovalInfo.EffectContext = Spec.GetEffectContext();

	const_cast<FActiveGameplayEffectsContainer&>(InArray).InternalOnActiveGameplayEffectRemoved(*this, !bIsInhibited, GameplayEffectRemovalInfo);	// Const cast is ok. It is there to prevent mutation of the GameplayEffects array, which this wont do.
}

void FActiveGameplayEffect::PostReplicatedAdd(const struct FActiveGameplayEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received ReplicatedGameplayEffect with no UGameplayEffect def."));
		return;
	}

	if (Spec.Modifiers.Num() != Spec.Def->Modifiers.Num())
	{
		// This can happen with older replays, where the replicated Spec.Modifiers size changed in the newer Spec.Def
		ABILITY_LOG(Error, TEXT("FActiveGameplayEffect::PostReplicatedAdd: Spec.Modifiers.Num() != Spec.Def->Modifiers.Num(). Spec: %s"), *Spec.ToSimpleString());
		Spec.Modifiers.Empty();
		return;
	}

	bool ShouldInvokeGameplayCueEvents = true;
	if (PredictionKey.IsLocalClientKey())
	{
		// PredictionKey will only be valid on the client that predicted it. So if this has a valid PredictionKey, we can assume we already predicted it and shouldn't invoke GameplayCues.
		// We may need to do more bookkeeping here in the future. Possibly give the predicted gameplayeffect a chance to pass something off to the new replicated gameplay effect.
		
		if (InArray.HasPredictedEffectWithPredictedKey(PredictionKey))
		{
			ShouldInvokeGameplayCueEvents = false;
		}
	}

	// Adjust start time for local clock
	{
		static const float MAX_DELTA_TIME = 3.f;

		// Was this actually just activated, or are we just finding out about it due to relevancy/join in progress?
		float WorldTimeSeconds = InArray.GetWorldTime();
		float ServerWorldTime = InArray.GetServerWorldTime();

		float DeltaServerWorldTime = ServerWorldTime - StartServerWorldTime;	// How long we think the effect has been playing

		// Set our local start time accordingly
		StartWorldTime = WorldTimeSeconds - DeltaServerWorldTime;
		CachedStartServerWorldTime = StartServerWorldTime;

		// Determine if we should invoke the OnActive GameplayCue event
		if (ShouldInvokeGameplayCueEvents)
		{
			// These events will get invoked if, after the parent array has been completely updated, this GE is still not inhibited
			bPendingRepOnActiveGC = (ServerWorldTime > 0 && FMath::Abs(DeltaServerWorldTime) < MAX_DELTA_TIME);
			bPendingRepWhileActiveGC = true;
		}
	}

	// Cache off StackCount
	ClientCachedStackCount = Spec.StackCount;

	// Handles are not replicated, so create a new one.
	Handle = FActiveGameplayEffectHandle::GenerateNewHandle(InArray.Owner);

	// Do stuff for adding GEs (add mods, tags, *invoke callbacks*
	const_cast<FActiveGameplayEffectsContainer&>(InArray).InternalOnActiveGameplayEffectAdded(*this);	// Const cast is ok. It is there to prevent mutation of the GameplayEffects array, which this wont do.
	
}

void FActiveGameplayEffect::PostReplicatedChange(const struct FActiveGameplayEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received ReplicatedGameplayEffect with no UGameplayEffect def."));
		return;
	}

	if (Spec.Modifiers.Num() != Spec.Def->Modifiers.Num())
	{
		// This can happen with older replays, where the replicated Spec.Modifiers size changed in the newer Spec.Def
		Spec.Modifiers.Empty();
		return;
	}

	// Handle potential duration refresh
	if (CachedStartServerWorldTime != StartServerWorldTime)
	{
		StartWorldTime = InArray.GetWorldTime() - static_cast<float>(InArray.GetServerWorldTime() - StartServerWorldTime);
		CachedStartServerWorldTime = StartServerWorldTime;

		const_cast<FActiveGameplayEffectsContainer&>(InArray).OnDurationChange(*this);
	}
	
	if (ClientCachedStackCount != Spec.StackCount)
	{
		// If its a stack count change, we just call OnStackCountChange and it will broadcast delegates and update attribute aggregators
		// Const cast is ok. It is there to prevent mutation of the GameplayEffects array, which this wont do.
		const_cast<FActiveGameplayEffectsContainer&>(InArray).OnStackCountChange(*this, ClientCachedStackCount, Spec.StackCount);
		ClientCachedStackCount = Spec.StackCount;
	}
	else
	{
		// Stack count didn't change, but something did (like a modifier magnitude). We need to update our attribute aggregators
		// Const cast is ok. It is there to prevent mutation of the GameplayEffects array, which this wont do.
		const_cast<FActiveGameplayEffectsContainer&>(InArray).UpdateAllAggregatorModMagnitudes(*this);
	}
}

FString FActiveGameplayEffect::GetDebugString()
{
	return FString::Printf(TEXT("(Def: %s. PredictionKey: %s)"), *GetNameSafe(Spec.Def), *PredictionKey.ToString());
}

void FActiveGameplayEffect::RecomputeStartWorldTime(const FActiveGameplayEffectsContainer& InArray)
{
	StartWorldTime = InArray.GetWorldTime() - static_cast<float>(InArray.GetServerWorldTime() - StartServerWorldTime);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FActiveGameplayEffectsContainer
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FActiveGameplayEffectsContainer::FActiveGameplayEffectsContainer()
	: Owner(nullptr)
	, OwnerIsNetAuthority(false)
	, ScopedLockCount(0)
	, PendingRemoves(0)
	, PendingGameplayEffectHead(nullptr)
{
	PendingGameplayEffectNext = &PendingGameplayEffectHead;
}

FActiveGameplayEffectsContainer::~FActiveGameplayEffectsContainer()
{
	FActiveGameplayEffect* PendingGameplayEffect = PendingGameplayEffectHead;
	if (PendingGameplayEffectHead)
	{
		FActiveGameplayEffect* Next = PendingGameplayEffectHead->PendingNext;
		delete PendingGameplayEffectHead;
		PendingGameplayEffectHead =	Next;
	}
}

void FActiveGameplayEffectsContainer::RegisterWithOwner(UAbilitySystemComponent* InOwner)
{
	if (Owner != InOwner)
	{
		Owner = InOwner;
		OwnerIsNetAuthority = Owner->IsOwnerActorAuthoritative();

		// Binding raw is ok here, since the owner is literally the UObject that owns us. If we are destroyed, its because that uobject is destroyed,
		// and if that is destroyed, the delegate wont be able to fire.
		Owner->RegisterGenericGameplayTagEvent().AddRaw(this, &FActiveGameplayEffectsContainer::OnOwnerTagChange);
	}
}

/** This is the main function that executes a GameplayEffect on Attributes and ActiveGameplayEffects */
void FActiveGameplayEffectsContainer::ExecuteActiveEffectsFrom(FGameplayEffectSpec &Spec, FPredictionKey PredictionKey)
{
	FGameplayEffectSpec& SpecToUse = Spec;

	// Capture our own tags.
	// TODO: We should only capture them if we need to. We may have snapshotted target tags (?) (in the case of dots with exotic setups?)

	SpecToUse.CapturedTargetTags.GetActorTags().Reset();
	Owner->GetOwnedGameplayTags(SpecToUse.CapturedTargetTags.GetActorTags());

	SpecToUse.CalculateModifierMagnitudes();

	// ------------------------------------------------------
	//	Modifiers
	//		These will modify the base value of attributes
	// ------------------------------------------------------
	
	bool ModifierSuccessfullyExecuted = false;

	for (int32 ModIdx = 0; ModIdx < SpecToUse.Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = SpecToUse.Def->Modifiers[ModIdx];
		
		FGameplayModifierEvaluatedData EvalData(ModDef.Attribute, ModDef.ModifierOp, SpecToUse.GetModifierMagnitude(ModIdx, true));
		ModifierSuccessfullyExecuted |= InternalExecuteMod(SpecToUse, EvalData);
	}

	// ------------------------------------------------------
	//	Executions
	//		This will run custom code to 'do stuff'
	// ------------------------------------------------------
	
	TArray< FGameplayEffectSpecHandle, TInlineAllocator<4> > ConditionalEffectSpecs;

	bool GameplayCuesWereManuallyHandled = false;

	for (const FGameplayEffectExecutionDefinition& CurExecDef : SpecToUse.Def->Executions)
	{
		bool bRunConditionalEffects = true; // Default to true if there is no CalculationClass specified.

		if (CurExecDef.CalculationClass)
		{
			const UGameplayEffectExecutionCalculation* ExecCDO = CurExecDef.CalculationClass->GetDefaultObject<UGameplayEffectExecutionCalculation>();
			check(ExecCDO);

			// Run the custom execution
			FGameplayEffectCustomExecutionParameters ExecutionParams(SpecToUse, CurExecDef.CalculationModifiers, Owner, CurExecDef.PassedInTags, PredictionKey);
			FGameplayEffectCustomExecutionOutput ExecutionOutput;
			ExecCDO->Execute(ExecutionParams, ExecutionOutput);

			bRunConditionalEffects = ExecutionOutput.ShouldTriggerConditionalGameplayEffects();

			// Execute any mods the custom execution yielded
			TArray<FGameplayModifierEvaluatedData>& OutModifiers = ExecutionOutput.GetOutputModifiersRef();

			const bool bApplyStackCountToEmittedMods = !ExecutionOutput.IsStackCountHandledManually();
			const int32 SpecStackCount = SpecToUse.StackCount;

			for (FGameplayModifierEvaluatedData& CurExecMod : OutModifiers)
			{
				// If the execution didn't manually handle the stack count, automatically apply it here
				if (bApplyStackCountToEmittedMods && SpecStackCount > 1)
				{
					CurExecMod.Magnitude = GameplayEffectUtilities::ComputeStackedModifierMagnitude(CurExecMod.Magnitude, SpecStackCount, CurExecMod.ModifierOp);
				}
				ModifierSuccessfullyExecuted |= InternalExecuteMod(SpecToUse, CurExecMod);
			}

			// If execution handled GameplayCues, we dont have to.
			if (ExecutionOutput.AreGameplayCuesHandledManually())
			{
				GameplayCuesWereManuallyHandled = true;
			}
		}

		if (bRunConditionalEffects)
		{
			// If successful, apply conditional specs
			for (const FConditionalGameplayEffect& ConditionalEffect : CurExecDef.ConditionalGameplayEffects)
			{
				if (ConditionalEffect.CanApply(SpecToUse.CapturedSourceTags.GetActorTags(), SpecToUse.GetLevel()))
				{
					FGameplayEffectSpecHandle SpecHandle = ConditionalEffect.CreateSpec(SpecToUse.GetEffectContext(), SpecToUse.GetLevel());
					if (SpecHandle.IsValid())
					{
						ConditionalEffectSpecs.Add(SpecHandle);
					}
				}
			}
		}
	}

	// ------------------------------------------------------
	//	Invoke GameplayCue events
	// ------------------------------------------------------
	
	// If there are no modifiers or we don't require modifier success to trigger, we apply the GameplayCue. 
	bool InvokeGameplayCueExecute = (SpecToUse.Modifiers.Num() == 0) || !Spec.Def->bRequireModifierSuccessToTriggerCues;

	// If there are modifiers, we only want to invoke the GameplayCue if one of them went through (could be blocked by immunity or % chance roll)
	if (SpecToUse.Modifiers.Num() > 0 && ModifierSuccessfullyExecuted)
	{
		InvokeGameplayCueExecute = true;
	}

	// Don't trigger gameplay cues if one of the executions says it manually handled them
	if (GameplayCuesWereManuallyHandled)
	{
		InvokeGameplayCueExecute = false;
	}

	if (InvokeGameplayCueExecute && SpecToUse.Def->GameplayCues.Num())
	{
		// TODO: check replication policy. Right now we will replicate every execute via a multicast RPC

		ABILITY_LOG(Log, TEXT("Invoking Execute GameplayCue for %s"), *SpecToUse.ToSimpleString());

		UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueExecuted_FromSpec(Owner, SpecToUse, PredictionKey);
	}

	// Apply any conditional linked effects
	for (const FGameplayEffectSpecHandle TargetSpec : ConditionalEffectSpecs)
	{
		if (TargetSpec.IsValid())
		{
			Owner->ApplyGameplayEffectSpecToSelf(*TargetSpec.Data.Get(), PredictionKey);
		}
	}
}

void FActiveGameplayEffectsContainer::ExecutePeriodicGameplayEffect(FActiveGameplayEffectHandle Handle)
{
	GAMEPLAYEFFECT_SCOPE_LOCK();
	FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
	if (ActiveEffect && !ActiveEffect->bIsInhibited)
	{
		FScopeCurrentGameplayEffectBeingApplied ScopedGEApplication(&ActiveEffect->Spec, Owner);

		if (UE_LOG_ACTIVE(VLogAbilitySystem, Log))
		{
			ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("Executed Periodic Effect %s"), *ActiveEffect->Spec.Def->GetFName().ToString());
			for (FGameplayModifierInfo Modifier : ActiveEffect->Spec.Def->Modifiers)
			{
				float Magnitude = 0.f;
				Modifier.ModifierMagnitude.AttemptCalculateMagnitude(ActiveEffect->Spec, Magnitude);
				ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EGameplayModOpToString(Modifier.ModifierOp), Magnitude);
			}
		}

		// Clear modified attributes before each periodic execution
		ActiveEffect->Spec.ModifiedAttributes.Empty();

		// Execute
		ExecuteActiveEffectsFrom(ActiveEffect->Spec);

		// Invoke Delegates for periodic effects being executed
		UAbilitySystemComponent* SourceASC = ActiveEffect->Spec.GetContext().GetInstigatorAbilitySystemComponent();
		Owner->OnPeriodicGameplayEffectExecuteOnSelf(SourceASC, ActiveEffect->Spec, Handle);
		if (SourceASC)
		{
			SourceASC->OnPeriodicGameplayEffectExecuteOnTarget(Owner, ActiveEffect->Spec, Handle);
		}
	}

}

FActiveGameplayEffect* FActiveGameplayEffectsContainer::GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle)
{
	for (FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			return &Effect;
		}
	}
	return nullptr;
}

const FActiveGameplayEffect* FActiveGameplayEffectsContainer::GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle) const
{
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			return &Effect;
		}
	}
	return nullptr;
}

FAggregatorRef& FActiveGameplayEffectsContainer::FindOrCreateAttributeAggregator(FGameplayAttribute Attribute)
{
	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr)
	{
		return *RefPtr;
	}

	// Create a new aggregator for this attribute.
	float CurrentBaseValueOfProperty = Owner->GetNumericAttributeBase(Attribute);
	ABILITY_LOG(Log, TEXT("Creating new entry in AttributeAggregatorMap for %s. CurrentValue: %.2f"), *Attribute.GetName(), CurrentBaseValueOfProperty);

	FAggregator* NewAttributeAggregator = new FAggregator(CurrentBaseValueOfProperty);
	
	if (Attribute.IsSystemAttribute() == false)
	{
		NewAttributeAggregator->OnDirty.AddUObject(Owner, &UAbilitySystemComponent::OnAttributeAggregatorDirty, Attribute, false);
		NewAttributeAggregator->OnDirtyRecursive.AddUObject(Owner, &UAbilitySystemComponent::OnAttributeAggregatorDirty, Attribute, true);
	}

	return AttributeAggregatorMap.Add(Attribute, FAggregatorRef(NewAttributeAggregator));
}

void FActiveGameplayEffectsContainer::OnAttributeAggregatorDirty(FAggregator* Aggregator, FGameplayAttribute Attribute, bool bFromRecursiveCall)
{
	check(AttributeAggregatorMap.FindChecked(Attribute).Get() == Aggregator);

	// Our Aggregator has changed, we need to reevaluate this aggregator and update the current value of the attribute.
	// Note that this is not an execution, so there are no 'source' and 'target' tags to fill out in the FAggregatorEvaluateParameters.
	// ActiveGameplayEffects that have required owned tags will be turned on/off via delegates, and will add/remove themselves from attribute
	// aggregators when that happens.
	
	FAggregatorEvaluateParameters EvaluationParameters;

	if (Owner->IsNetSimulating())
	{
		if (FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate && Aggregator->NetUpdateID != FScopedAggregatorOnDirtyBatch::NetUpdateID)
		{
			// We are a client. The current value of this attribute is the replicated server's "final" value. We dont actually know what the 
			// server's base value is. But we can calculate it with ReverseEvaluate(). Then, we can call Evaluate with IncludePredictiveMods=true
			// to apply our mods and get an accurate predicted value.
			// 
			// It is very important that we only do this exactly one time when we get a new value from the server. Once we set the new local value for this
			// attribute below, recalculating the base would give us the wrong server value. We should only do this when we are coming directly from a network update.
			// 
			// Unfortunately there are two ways we could get here from a network update: from the ActiveGameplayEffect container being updated or from a traditional
			// OnRep on the actual attribute uproperty. Both of these could happen in a single network update, or potentially only one could happen
			// (and in fact it could be either one! the AGE container could change in a way that doesnt change the final attribute value, or we could have the base value
			// of the attribute actually be modified (e.g,. losing health or mana which only results in an OnRep and not in a AGE being applied).
			// 
			// So both paths need to lead to this function, but we should only do it one time per update. Once we update the base value, we need to make sure we dont do it again
			// until we get a new network update. GlobalFromNetworkUpdate and NetUpdateID are what do this.
			// 
			// GlobalFromNetworkUpdate - only set to true when we are coming from an OnRep or when we are coming from an ActiveGameplayEffect container net update.
			// NetUpdateID - updated once whenever an AttributeSet is received over the network. It will be incremented one time per actor that gets an update.
					
			float BaseValue = 0.f;
			if (!FGameplayAttribute::IsGameplayAttributeDataProperty(Attribute.GetUProperty()))
			{
				// Legacy float attribute case requires the base value to be deduced from the final value, as it is not replicated
				float FinalValue = Owner->GetNumericAttribute(Attribute);
				BaseValue = Aggregator->ReverseEvaluate(FinalValue, EvaluationParameters);
				ABILITY_LOG(Log, TEXT("Reverse Evaluated %s. FinalValue: %.2f  BaseValue: %.2f "), *Attribute.GetName(), FinalValue, BaseValue);
			}
			else
			{
				BaseValue = Owner->GetNumericAttributeBase(Attribute);
			}
			
			Aggregator->SetBaseValue(BaseValue, false);
			Aggregator->NetUpdateID = FScopedAggregatorOnDirtyBatch::NetUpdateID;
		}

		EvaluationParameters.IncludePredictiveMods = true;
	}

	float NewValue = Aggregator->Evaluate(EvaluationParameters);

	if (EvaluationParameters.IncludePredictiveMods)
	{
		ABILITY_LOG(Log, TEXT("After Prediction, FinalValue: %.2f"), NewValue);
	}

	InternalUpdateNumericalAttribute(Attribute, NewValue, nullptr, bFromRecursiveCall);
}

void FActiveGameplayEffectsContainer::OnMagnitudeDependencyChange(FActiveGameplayEffectHandle Handle, const FAggregator* ChangedAgg)
{
	if (Handle.IsValid())
	{
		GAMEPLAYEFFECT_SCOPE_LOCK();
		FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
		if (ActiveEffect)
		{
			// This handle registered with the ChangedAgg to be notified when the aggregator changed.
			// At this point we don't know what actually needs to be updated inside this active gameplay effect.
			FGameplayEffectSpec& Spec = ActiveEffect->Spec;

			// We must update attribute aggregators only if we are actually 'on' right now, and if we are non periodic (periodic effects do their thing on execute callbacks)
			const bool MustUpdateAttributeAggregators = (ActiveEffect->bIsInhibited == false && (Spec.GetPeriod() <= UGameplayEffect::NO_PERIOD));

			// As we update our modifier magnitudes, we will update our owner's attribute aggregators. When we do this, we have to clear them first of all of our (Handle's) previous mods.
			// Since we could potentially have two mods to the same attribute, one that gets updated, and one that doesnt - we need to do this in two passes.
			TSet<FGameplayAttribute> AttributesToUpdate;

			bool bMarkedDirty = false;

			// First pass: update magnitudes of our modifiers that changed
			for(int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
			{
				const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
				FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];

				float RecalculatedMagnitude = 0.f;
				if (ModDef.ModifierMagnitude.AttemptRecalculateMagnitudeFromDependentAggregatorChange(Spec, RecalculatedMagnitude, ChangedAgg))
				{
					// If this is the first pending magnitude change, need to mark the container item dirty as well as
					// wake the owner actor from dormancy so replication works properly
					if (!bMarkedDirty)
					{
						bMarkedDirty = true;
						if (Owner && Owner->OwnerActor && IsNetAuthority())
						{
							Owner->OwnerActor->FlushNetDormancy();
						}
						MarkItemDirty(*ActiveEffect);
					}

					ModSpec.EvaluatedMagnitude = RecalculatedMagnitude;

					// We changed, so we need to reapply/update our spot in the attribute aggregator map
					if (MustUpdateAttributeAggregators)
					{
						AttributesToUpdate.Add(ModDef.Attribute);
					}
				}
			}

			// Second pass, update the aggregators that we need to
			UpdateAggregatorModMagnitudes(AttributesToUpdate, *ActiveEffect);
		}
	}
}

void FActiveGameplayEffectsContainer::OnStackCountChange(FActiveGameplayEffect& ActiveEffect, int32 OldStackCount, int32 NewStackCount)
{
	MarkItemDirty(ActiveEffect);
	if (OldStackCount != NewStackCount)
	{
		// Only update attributes if stack count actually changed.
		UpdateAllAggregatorModMagnitudes(ActiveEffect);
	}

	if (ActiveEffect.Spec.Def != nullptr)
	{
		Owner->NotifyTagMap_StackCountChange(ActiveEffect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags);
	}

	Owner->NotifyTagMap_StackCountChange(ActiveEffect.Spec.DynamicGrantedTags);

	ActiveEffect.OnStackChangeDelegate.Broadcast(ActiveEffect.Handle, ActiveEffect.Spec.StackCount, OldStackCount);
}

/** Called when the duration or starttime of an AGE has changed */
void FActiveGameplayEffectsContainer::OnDurationChange(FActiveGameplayEffect& Effect)
{
	Effect.OnTimeChangeDelegate.Broadcast(Effect.Handle, Effect.StartWorldTime, Effect.GetDuration());
	Owner->OnGameplayEffectDurationChange(Effect);
}

void FActiveGameplayEffectsContainer::UpdateAllAggregatorModMagnitudes(FActiveGameplayEffect& ActiveEffect)
{
	// We should never be doing this for periodic effects since their mods are not persistent on attribute aggregators
	if (ActiveEffect.Spec.GetPeriod() > UGameplayEffect::NO_PERIOD)
	{
		return;
	}

	// we don't need to update inhibited effects
	if (ActiveEffect.bIsInhibited)
	{
		return;
	}

	const FGameplayEffectSpec& Spec = ActiveEffect.Spec;

	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UpdateAllAggregatorModMagnitudes called with no UGameplayEffect def."));
		return;
	}

	TSet<FGameplayAttribute> AttributesToUpdate;

	for (int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
		AttributesToUpdate.Add(ModDef.Attribute);
	}

	UpdateAggregatorModMagnitudes(AttributesToUpdate, ActiveEffect);
}

void FActiveGameplayEffectsContainer::UpdateAggregatorModMagnitudes(const TSet<FGameplayAttribute>& AttributesToUpdate, FActiveGameplayEffect& ActiveEffect)
{
	const FGameplayEffectSpec& Spec = ActiveEffect.Spec;
	for (const FGameplayAttribute& Attribute : AttributesToUpdate)
	{
		// skip over any modifiers for attributes that we don't have
		if (!Owner || Owner->HasAttributeSetForAttribute(Attribute) == false)
		{
			continue;
		}

		FAggregator* Aggregator = FindOrCreateAttributeAggregator(Attribute).Get();
		check(Aggregator);

		// Update the aggregator Mods.
		Aggregator->UpdateAggregatorMod(ActiveEffect.Handle, Attribute, Spec, ActiveEffect.PredictionKey.WasLocallyGenerated(), ActiveEffect.Handle);
	}
}

FActiveGameplayEffect* FActiveGameplayEffectsContainer::FindStackableActiveGameplayEffect(const FGameplayEffectSpec& Spec)
{
	FActiveGameplayEffect* StackableGE = nullptr;
	const UGameplayEffect* GEDef = Spec.Def;
	EGameplayEffectStackingType StackingType = GEDef->StackingType;

	if (StackingType != EGameplayEffectStackingType::None && Spec.GetDuration() != UGameplayEffect::INSTANT_APPLICATION)
	{
		// Iterate through GameplayEffects to see if we find a match. Note that we could cache off a handle in a map but we would still
		// do a linear search through GameplayEffects to find the actual FActiveGameplayEffect (due to unstable nature of the GameplayEffects array).
		// If this becomes a slow point in the profiler, the map may still be useful as an early out to avoid an unnecessary sweep.
		UAbilitySystemComponent* SourceASC = Spec.GetContext().GetInstigatorAbilitySystemComponent();
		for (FActiveGameplayEffect& ActiveEffect: this)
		{
			// Aggregate by source stacking additionally requires the source ability component to match
			if (ActiveEffect.Spec.Def == Spec.Def && ((StackingType == EGameplayEffectStackingType::AggregateByTarget) || (SourceASC && SourceASC == ActiveEffect.Spec.GetContext().GetInstigatorAbilitySystemComponent())))
			{
				StackableGE = &ActiveEffect;
				break;
			}
		}
	}

	return StackableGE;
}

bool FActiveGameplayEffectsContainer::HandleActiveGameplayEffectStackOverflow(const FActiveGameplayEffect& ActiveStackableGE, const FGameplayEffectSpec& OldSpec, const FGameplayEffectSpec& OverflowingSpec)
{
	const UGameplayEffect* StackedGE = OldSpec.Def;
	const bool bAllowOverflowApplication = !(StackedGE->bDenyOverflowApplication);

	FPredictionKey PredictionKey;
	for (TSubclassOf<UGameplayEffect> OverflowEffect : StackedGE->OverflowEffects)
	{
		if (const UGameplayEffect* CDO = OverflowEffect.GetDefaultObject())
		{
			FGameplayEffectSpec NewGESpec;
			NewGESpec.InitializeFromLinkedSpec(CDO, OverflowingSpec);
			Owner->ApplyGameplayEffectSpecToSelf(NewGESpec, PredictionKey);
		}
	}

	if (!bAllowOverflowApplication && StackedGE->bClearStackOnOverflow)
	{
		Owner->RemoveActiveGameplayEffect(ActiveStackableGE.Handle);
	}

	return bAllowOverflowApplication;
}

bool FActiveGameplayEffectsContainer::ShouldUseMinimalReplication()
{
	return IsNetAuthority() && (Owner->ReplicationMode == EReplicationMode::Minimal || Owner->ReplicationMode == EReplicationMode::Mixed);
}

void FActiveGameplayEffectsContainer::SetBaseAttributeValueFromReplication(FGameplayAttribute Attribute, float ServerValue)
{
	float OldValue = Owner->GetNumericAttribute(Attribute);

	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr && RefPtr->Get())
	{
		FAggregator* Aggregator = RefPtr->Get();
		
		FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate = true;
		OnAttributeAggregatorDirty(Aggregator, Attribute);
		FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate = false;
	}
	else
	{
		// No aggregators on the client but still broadcast the dirty delegate
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (FOnGameplayAttributeChange* LegacyDelegate = AttributeChangeDelegates.Find(Attribute))
		{
			LegacyDelegate->Broadcast(ServerValue, nullptr);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		if (FOnGameplayAttributeValueChange* Delegate = AttributeValueChangeDelegates.Find(Attribute))
		{
			FOnAttributeChangeData CallbackData;
			CallbackData.Attribute = Attribute;
			CallbackData.NewValue = ServerValue;
			CallbackData.OldValue = OldValue;
			CallbackData.GEModData = nullptr;

			Delegate->Broadcast(CallbackData);
		}
	}
}

void FActiveGameplayEffectsContainer::GetAllActiveGameplayEffectSpecs(TArray<FGameplayEffectSpec>& OutSpecCopies) const
{
	for (const FActiveGameplayEffect& ActiveEffect : this)	
	{
		OutSpecCopies.Add(ActiveEffect.Spec);
	}
}

void FActiveGameplayEffectsContainer::GetGameplayEffectStartTimeAndDuration(FActiveGameplayEffectHandle Handle, float& EffectStartTime, float& EffectDuration) const
{
	EffectStartTime = UGameplayEffect::INFINITE_DURATION;
	EffectDuration = UGameplayEffect::INFINITE_DURATION;

	if (Handle.IsValid())
	{
		for (const FActiveGameplayEffect& ActiveEffect : this)
		{
			if (ActiveEffect.Handle == Handle)
			{
				EffectStartTime = ActiveEffect.StartWorldTime;
				EffectDuration = ActiveEffect.GetDuration();
				return;
			}
		}
	}

	ABILITY_LOG(Warning, TEXT("GetGameplayEffectStartTimeAndDuration called with invalid Handle: %s"), *Handle.ToString());
}

float FActiveGameplayEffectsContainer::GetGameplayEffectMagnitude(FActiveGameplayEffectHandle Handle, FGameplayAttribute Attribute) const
{
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			for(int32 ModIdx = 0; ModIdx < Effect.Spec.Modifiers.Num(); ++ModIdx)
			{
				const FGameplayModifierInfo& ModDef = Effect.Spec.Def->Modifiers[ModIdx];
				const FModifierSpec& ModSpec = Effect.Spec.Modifiers[ModIdx];
			
				if (ModDef.Attribute == Attribute)
				{
					return ModSpec.GetEvaluatedMagnitude();
				}
			}
		}
	}

	ABILITY_LOG(Warning, TEXT("GetGameplayEffectMagnitude called with invalid Handle: %s"), *Handle.ToString());
	return -1.f;
}

void FActiveGameplayEffectsContainer::SetActiveGameplayEffectLevel(FActiveGameplayEffectHandle ActiveHandle, int32 NewLevel)
{
	for (FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Handle == ActiveHandle)
		{
			Effect.Spec.SetLevel(NewLevel);
			MarkItemDirty(Effect);
			Effect.Spec.CalculateModifierMagnitudes();
			UpdateAllAggregatorModMagnitudes(Effect);
			break;
		}
	}
}

const FGameplayTagContainer* FActiveGameplayEffectsContainer::GetGameplayEffectSourceTagsFromHandle(FActiveGameplayEffectHandle Handle) const
{
	// @todo: Need to consider this with tag changes
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			return Effect.Spec.CapturedSourceTags.GetAggregatedTags();
		}
	}

	return nullptr;
}

const FGameplayTagContainer* FActiveGameplayEffectsContainer::GetGameplayEffectTargetTagsFromHandle(FActiveGameplayEffectHandle Handle) const
{
	// @todo: Need to consider this with tag changes
	const FActiveGameplayEffect* Effect = GetActiveGameplayEffect(Handle);
	if (Effect)
	{
		return Effect->Spec.CapturedTargetTags.GetAggregatedTags();
	}

	return nullptr;
}

void FActiveGameplayEffectsContainer::CaptureAttributeForGameplayEffect(OUT FGameplayEffectAttributeCaptureSpec& OutCaptureSpec)
{
	FAggregatorRef& AttributeAggregator = FindOrCreateAttributeAggregator(OutCaptureSpec.BackingDefinition.AttributeToCapture);
	
	if (OutCaptureSpec.BackingDefinition.bSnapshot)
	{
		OutCaptureSpec.AttributeAggregator.TakeSnapshotOf(AttributeAggregator);
	}
	else
	{
		OutCaptureSpec.AttributeAggregator = AttributeAggregator;
	}
}

void FActiveGameplayEffectsContainer::InternalUpdateNumericalAttribute(FGameplayAttribute Attribute, float NewValue, const FGameplayEffectModCallbackData* ModData, bool bFromRecursiveCall)
{
	ABILITY_LOG(Log, TEXT("Property %s new value is: %.2f"), *Attribute.GetName(), NewValue);

	float OldValue = Owner->GetNumericAttribute(Attribute);
	Owner->SetNumericAttribute_Internal(Attribute, NewValue);
	
	if (!bFromRecursiveCall)
	{
		// We should only have one: either cached CurrentModcallbackData, or explicit callback data passed directly in.
		if (ModData && CurrentModcallbackData)
		{
			ABILITY_LOG(Warning, TEXT("Had passed in ModData and cached CurrentModcallbackData in FActiveGameplayEffectsContainer::InternalUpdateNumericalAttribute. For attribute %s on %s."), *Attribute.GetName(), *Owner->GetFullName() );
		}
		
		const FGameplayEffectModCallbackData* DataToShare = ModData ? ModData : CurrentModcallbackData;

		// DEPRECATED Delegate
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (FOnGameplayAttributeChange* LegacyDelegate = AttributeChangeDelegates.Find(Attribute))
		{
			LegacyDelegate->Broadcast(NewValue, DataToShare);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// NEW Delegate
		if (FOnGameplayAttributeValueChange* NewDelegate = AttributeValueChangeDelegates.Find(Attribute))
		{
			FOnAttributeChangeData CallbackData;
			CallbackData.Attribute = Attribute;
			CallbackData.NewValue = NewValue;
			CallbackData.OldValue = OldValue;
			CallbackData.GEModData = DataToShare;
			NewDelegate->Broadcast(CallbackData);
		}
	}
	CurrentModcallbackData = nullptr;
}

void FActiveGameplayEffectsContainer::SetAttributeBaseValue(FGameplayAttribute Attribute, float NewBaseValue)
{
	const UAttributeSet* Set = Owner->GetAttributeSubobject(Attribute.GetAttributeSetClass());
	if (ensure(Set))
	{
		Set->PreAttributeBaseChange(Attribute, NewBaseValue);
	}

	// if we're using the new attributes we should always update their base value
	bool bIsGameplayAttributeDataProperty = FGameplayAttribute::IsGameplayAttributeDataProperty(Attribute.GetUProperty());
	if (bIsGameplayAttributeDataProperty)
	{
		const UStructProperty* StructProperty = Cast<UStructProperty>(Attribute.GetUProperty());
		check(StructProperty);
		FGameplayAttributeData* DataPtr = StructProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(const_cast<UAttributeSet*>(Set));
		if (ensure(DataPtr))
		{
			DataPtr->SetBaseValue(NewBaseValue);
		}
	}

	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr)
	{
		// There is an aggregator for this attribute, so set the base value. The dirty callback chain
		// will update the actual AttributeSet property value for us.		
		RefPtr->Get()->SetBaseValue(NewBaseValue);
	}
	// if there is no aggregator set the current value (base == current in this case)
	else
	{
		InternalUpdateNumericalAttribute(Attribute, NewBaseValue, nullptr);
	}
}

float FActiveGameplayEffectsContainer::GetAttributeBaseValue(FGameplayAttribute Attribute) const
{
	float BaseValue = 0.f;
	const FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	// if this attribute is of type FGameplayAttributeData then use the base value stored there
	if (FGameplayAttribute::IsGameplayAttributeDataProperty(Attribute.GetUProperty()))
	{
		const UStructProperty* StructProperty = Cast<UStructProperty>(Attribute.GetUProperty());
		check(StructProperty);
		const UAttributeSet* AttributeSet = Owner->GetAttributeSubobject(Attribute.GetAttributeSetClass());
		ensure(AttributeSet);
		const FGameplayAttributeData* DataPtr = StructProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeSet);
		if (DataPtr)
		{
			BaseValue = DataPtr->GetBaseValue();
		}
	}
	// otherwise, if we have an aggregator use the base value in the aggregator
	else if (RefPtr)
	{
		BaseValue = RefPtr->Get()->GetBaseValue();
	}
	// if the attribute is just a float and there is no aggregator then the base value is the current value
	else
	{
		BaseValue = Owner->GetNumericAttribute(Attribute);
	}

	return BaseValue;
}

float FActiveGameplayEffectsContainer::GetEffectContribution(const FAggregatorEvaluateParameters& Parameters, FActiveGameplayEffectHandle ActiveHandle, FGameplayAttribute Attribute)
{
	FAggregatorRef Aggregator = FindOrCreateAttributeAggregator(Attribute);
	return Aggregator.Get()->EvaluateContribution(Parameters, ActiveHandle);
}

bool FActiveGameplayEffectsContainer::InternalExecuteMod(FGameplayEffectSpec& Spec, FGameplayModifierEvaluatedData& ModEvalData)
{
	check(Owner);

	bool bExecuted = false;

	UAttributeSet* AttributeSet = nullptr;
	UClass* AttributeSetClass = ModEvalData.Attribute.GetAttributeSetClass();
	if (AttributeSetClass && AttributeSetClass->IsChildOf(UAttributeSet::StaticClass()))
	{
		AttributeSet = const_cast<UAttributeSet*>(Owner->GetAttributeSubobject(AttributeSetClass));
	}

	if (AttributeSet)
	{
		FGameplayEffectModCallbackData ExecuteData(Spec, ModEvalData, *Owner);

		/**
		 *  This should apply 'gamewide' rules. Such as clamping Health to MaxHealth or granting +3 health for every point of strength, etc
		 *	PreAttributeModify can return false to 'throw out' this modification.
		 */
		if (AttributeSet->PreGameplayEffectExecute(ExecuteData))
		{
			float OldValueOfProperty = Owner->GetNumericAttribute(ModEvalData.Attribute);
			ApplyModToAttribute(ModEvalData.Attribute, ModEvalData.ModifierOp, ModEvalData.Magnitude, &ExecuteData);

			FGameplayEffectModifiedAttribute* ModifiedAttribute = Spec.GetModifiedAttribute(ModEvalData.Attribute);
			if (!ModifiedAttribute)
			{
				// If we haven't already created a modified attribute holder, create it
				ModifiedAttribute = Spec.AddModifiedAttribute(ModEvalData.Attribute);
			}
			ModifiedAttribute->TotalMagnitude += ModEvalData.Magnitude;

			/** This should apply 'gamewide' rules. Such as clamping Health to MaxHealth or granting +3 health for every point of strength, etc */
			AttributeSet->PostGameplayEffectExecute(ExecuteData);

#if ENABLE_VISUAL_LOG
			DebugExecutedGameplayEffectData DebugData;
			DebugData.GameplayEffectName = Spec.Def->GetName();
			DebugData.ActivationState = "INSTANT";
			DebugData.Attribute = ModEvalData.Attribute;
			DebugData.Magnitude = Owner->GetNumericAttribute(ModEvalData.Attribute) - OldValueOfProperty;
			DebugExecutedGameplayEffects.Add(DebugData);
#endif // ENABLE_VISUAL_LOG

			bExecuted = true;
		}
	}
	else
	{
		// Our owner doesn't have this attribute, so we can't do anything
		ABILITY_LOG(Log, TEXT("%s does not have attribute %s. Skipping modifier"), *Owner->GetPathName(), *ModEvalData.Attribute.GetName());
	}

	return bExecuted;
}

void FActiveGameplayEffectsContainer::ApplyModToAttribute(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude, const FGameplayEffectModCallbackData* ModData)
{
	CurrentModcallbackData = ModData;
	float CurrentBase = GetAttributeBaseValue(Attribute);
	float NewBase = FAggregator::StaticExecModOnBaseValue(CurrentBase, ModifierOp, ModifierMagnitude);

	SetAttributeBaseValue(Attribute, NewBase);

	if (CurrentModcallbackData)
	{
		// We expect this to be cleared for us in InternalUpdateNumericalAttribute
		ABILITY_LOG(Warning, TEXT("FActiveGameplayEffectsContainer::ApplyModToAttribute CurrentModcallbackData was not consumed For attribute %s on %s."), *Attribute.GetName(), *Owner->GetFullName());
		CurrentModcallbackData = nullptr;
	}
}

FActiveGameplayEffect* FActiveGameplayEffectsContainer::ApplyGameplayEffectSpec(const FGameplayEffectSpec& Spec, FPredictionKey& InPredictionKey, bool& bFoundExistingStackableGE)
{
	SCOPE_CYCLE_COUNTER(STAT_ApplyGameplayEffectSpec);

	GAMEPLAYEFFECT_SCOPE_LOCK();

	bFoundExistingStackableGE = false;

	if (Owner && Owner->OwnerActor && IsNetAuthority())
	{
		Owner->OwnerActor->FlushNetDormancy();
	}

	FActiveGameplayEffect* AppliedActiveGE = nullptr;
	FActiveGameplayEffect* ExistingStackableGE = FindStackableActiveGameplayEffect(Spec);

	bool bSetDuration = true;
	bool bSetPeriod = true;
	int32 StartingStackCount = 0;
	int32 NewStackCount = 0;

	// Check if there's an active GE this application should stack upon
	if (ExistingStackableGE)
	{
		if (!IsNetAuthority())
		{
			// Don't allow prediction of stacking for now
			return nullptr;
		}
		else
		{
			// Server invalidates the prediction key for this GE since client is not predicting it
			InPredictionKey = FPredictionKey();
		}

		bFoundExistingStackableGE = true;

		FGameplayEffectSpec& ExistingSpec = ExistingStackableGE->Spec;
		StartingStackCount = ExistingSpec.StackCount;

		// This is now the global "being applied GE"
		UAbilitySystemGlobals::Get().SetCurrentAppliedGE(&ExistingSpec);
		
		// How to apply multiple stacks at once? What if we trigger an overflow which can reject the application?
		// We still want to apply the stacks that didnt push us over, but we also want to call HandleActiveGameplayEffectStackOverflow.
		
		// For now: call HandleActiveGameplayEffectStackOverflow only if we are ALREADY at the limit. Else we just clamp stack limit to max.
		if (ExistingSpec.StackCount == ExistingSpec.Def->StackLimitCount)
		{
			if (!HandleActiveGameplayEffectStackOverflow(*ExistingStackableGE, ExistingSpec, Spec))
			{
				return nullptr;
			}
		}
		
		NewStackCount = ExistingSpec.StackCount + Spec.StackCount;
		if (ExistingSpec.Def->StackLimitCount > 0)
		{
			NewStackCount = FMath::Min(NewStackCount, ExistingSpec.Def->StackLimitCount);
		}

		// Need to unregister callbacks because the source aggregators could potentially be different with the new application. They will be
		// re-registered later below, as necessary.
		ExistingSpec.CapturedRelevantAttributes.UnregisterLinkedAggregatorCallbacks(ExistingStackableGE->Handle);

		// @todo: If dynamically granted tags differ (which they shouldn't), we'll actually have to diff them
		// and cause a removal and add of only the ones that have changed. For now, ensure on this happening and come
		// back to this later.
		ensureMsgf(ExistingSpec.DynamicGrantedTags == Spec.DynamicGrantedTags, TEXT("While adding a stack of the gameplay effect: %s, the old stack and the new application had different dynamically granted tags, which is currently not resolved properly!"), *Spec.Def->GetName());
		
		// We only grant abilities on the first apply. So we *dont* want the new spec's GrantedAbilitySpecs list
		TArray<FGameplayAbilitySpecDef>	GrantedSpecTempArray(MoveTemp(ExistingStackableGE->Spec.GrantedAbilitySpecs));

		// @todo: If dynamic asset tags differ (which they shouldn't), we'll actually have to diff them
		// and cause a removal and add of only the ones that have changed. For now, ensure on this happening and come
		// back to this later.
		ensureMsgf(ExistingSpec.DynamicAssetTags == Spec.DynamicAssetTags, TEXT("While adding a stack of the gameplay effect: %s, the old stack and the new application had different dynamic asset tags, which is currently not resolved properly!"), *Spec.Def->GetName());

		ExistingStackableGE->Spec = Spec;
		ExistingStackableGE->Spec.StackCount = NewStackCount;

		// Swap in old granted ability spec
		ExistingStackableGE->Spec.GrantedAbilitySpecs = MoveTemp(GrantedSpecTempArray);
		
		AppliedActiveGE = ExistingStackableGE;

		const UGameplayEffect* GEDef = ExistingSpec.Def;

		// Make sure the GE actually wants to refresh its duration
		if (GEDef->StackDurationRefreshPolicy == EGameplayEffectStackingDurationPolicy::NeverRefresh)
		{
			bSetDuration = false;
		}
		else
		{
			RestartActiveGameplayEffectDuration(*ExistingStackableGE);
		}

		// Make sure the GE actually wants to reset its period
		if (GEDef->StackPeriodResetPolicy == EGameplayEffectStackingPeriodPolicy::NeverReset)
		{
			bSetPeriod = false;
		}
	}
	else
	{
		FActiveGameplayEffectHandle NewHandle = FActiveGameplayEffectHandle::GenerateNewHandle(Owner);

		if (ScopedLockCount > 0 && GameplayEffects_Internal.GetSlack() <= 0)
		{
			/**
			 *	If we have no more slack and we are scope locked, we need to put this addition on our pending GE list, which will be moved
			 *	onto the real active GE list once the scope lock is over.
			 *	
			 *	To avoid extra heap allocations, each active gameplayeffect container keeps a linked list of pending GEs. This list is allocated
			 *	on demand and re-used in subsequent pending adds. The code below will either 1) Alloc a new pending GE 2) reuse an existing pending GE.
			 *	The move operator is used to move stuff to and from these pending GEs to avoid deep copies.
			 */

			check(PendingGameplayEffectNext);
			if (*PendingGameplayEffectNext == nullptr)
			{
				// We have no memory allocated to put our next pending GE, so make a new one.
				// [#1] If you change this, please change #1-3!!!
				AppliedActiveGE = new FActiveGameplayEffect(NewHandle, Spec, GetWorldTime(), GetServerWorldTime(), InPredictionKey);
				*PendingGameplayEffectNext = AppliedActiveGE;
			}
			else
			{
				// We already had memory allocated to put a pending GE, move in.
				// [#2] If you change this, please change #1-3!!!
				**PendingGameplayEffectNext = FActiveGameplayEffect(NewHandle, Spec, GetWorldTime(), GetServerWorldTime(), InPredictionKey);
				AppliedActiveGE = *PendingGameplayEffectNext;
			}

			// The next pending GameplayEffect goes to where our PendingNext points
			PendingGameplayEffectNext = &AppliedActiveGE->PendingNext;
		}
		else
		{

			// [#3] If you change this, please change #1-3!!!
			AppliedActiveGE = new(GameplayEffects_Internal) FActiveGameplayEffect(NewHandle, Spec, GetWorldTime(), GetServerWorldTime(), InPredictionKey);
		}
	}

	// This is now the global "being applied GE"
	UAbilitySystemGlobals::Get().SetCurrentAppliedGE(&AppliedActiveGE->Spec);

	FGameplayEffectSpec& AppliedEffectSpec = AppliedActiveGE->Spec;
	UAbilitySystemGlobals::Get().GlobalPreGameplayEffectSpecApply(AppliedEffectSpec, Owner);

	// Make sure our target's tags are collected, so we can properly filter infinite effects
	AppliedEffectSpec.CapturedTargetTags.GetActorTags().Reset();
	Owner->GetOwnedGameplayTags(AppliedEffectSpec.CapturedTargetTags.GetActorTags());

	// Calc all of our modifier magnitudes now. Some may need to update later based on attributes changing, etc, but those should
	// be done through delegate callbacks.
	AppliedEffectSpec.CaptureAttributeDataFromTarget(Owner);
	AppliedEffectSpec.CalculateModifierMagnitudes();

	// Build ModifiedAttribute list so GCs can have magnitude info if non-period effect
	// Note: One day may want to not call gameplay cues unless ongoing tag requirements are met (will need to move this there)
	{
		const bool HasModifiedAttributes = AppliedEffectSpec.ModifiedAttributes.Num() > 0;
		const bool HasDurationAndNoPeriod = AppliedEffectSpec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration && AppliedEffectSpec.GetPeriod() == UGameplayEffect::NO_PERIOD;
		const bool HasPeriodAndNoDuration = AppliedEffectSpec.Def->DurationPolicy == EGameplayEffectDurationType::Instant &&
											AppliedEffectSpec.GetPeriod() != UGameplayEffect::NO_PERIOD;
		const bool ShouldBuildModifiedAttributeList = !HasModifiedAttributes && (HasDurationAndNoPeriod || HasPeriodAndNoDuration);
		if (ShouldBuildModifiedAttributeList)
		{
			int32 ModifierIndex = -1;
			for (const FGameplayModifierInfo& Mod : AppliedEffectSpec.Def->Modifiers)
			{
				++ModifierIndex;

				// Take magnitude from evaluated magnitudes
				float Magnitude = 0.0f;
				if (AppliedEffectSpec.Modifiers.IsValidIndex(ModifierIndex))
				{
					const FModifierSpec& ModSpec = AppliedEffectSpec.Modifiers[ModifierIndex];
					Magnitude = ModSpec.GetEvaluatedMagnitude();
				}
				
				// Add to ModifiedAttribute list if it doesn't exist already
				FGameplayEffectModifiedAttribute* ModifiedAttribute = AppliedEffectSpec.GetModifiedAttribute(Mod.Attribute);
				if (!ModifiedAttribute)
				{
					ModifiedAttribute = AppliedEffectSpec.AddModifiedAttribute(Mod.Attribute);
				}
				ModifiedAttribute->TotalMagnitude += Magnitude;
			}
		}
	}

	// Register Source and Target non snapshot capture delegates here
	AppliedEffectSpec.CapturedRelevantAttributes.RegisterLinkedAggregatorCallbacks(AppliedActiveGE->Handle);
	
	if (bSetDuration)
	{
		// Re-calculate the duration, as it could rely on target captured attributes
		float DefCalcDuration = 0.f;
		if (AppliedEffectSpec.AttemptCalculateDurationFromDef(DefCalcDuration))
		{
			AppliedEffectSpec.SetDuration(DefCalcDuration, false);
		}
		else if (AppliedEffectSpec.Def->DurationMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller)
		{
			AppliedEffectSpec.Def->DurationMagnitude.AttemptCalculateMagnitude(AppliedEffectSpec, AppliedEffectSpec.Duration);
		}

		const float DurationBaseValue = AppliedEffectSpec.GetDuration();

		// Calculate Duration mods if we have a real duration
		if (DurationBaseValue > 0.f)
		{
			float FinalDuration = AppliedEffectSpec.CalculateModifiedDuration();

			// We cannot mod ourselves into an instant or infinite duration effect
			if (FinalDuration <= 0.f)
			{
				ABILITY_LOG(Error, TEXT("GameplayEffect %s Duration was modified to %.2f. Clamping to 0.1s duration."), *AppliedEffectSpec.Def->GetName(), FinalDuration);
				FinalDuration = 0.1f;
			}

			AppliedEffectSpec.SetDuration(FinalDuration, true);

			// ABILITY_LOG(Warning, TEXT("SetDuration for %s. Base: %.2f, Final: %.2f"), *NewEffect.Spec.Def->GetName(), DurationBaseValue, FinalDuration);

			// Register duration callbacks with the timer manager
			if (Owner)
			{
				FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();
				FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::CheckDurationExpired, AppliedActiveGE->Handle);
				TimerManager.SetTimer(AppliedActiveGE->DurationHandle, Delegate, FinalDuration, false);
				if (!ensureMsgf(AppliedActiveGE->DurationHandle.IsValid(), TEXT("Invalid Duration Handle after attempting to set duration for GE %s @ %.2f"), 
					*AppliedActiveGE->GetDebugString(), FinalDuration))
				{
					// Force this off next frame
					TimerManager.SetTimerForNextTick(Delegate);
				}
			}
		}
	}
	
	// Register period callbacks with the timer manager
	if (bSetPeriod && Owner && (AppliedEffectSpec.GetPeriod() != UGameplayEffect::NO_PERIOD))
	{
		FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::ExecutePeriodicEffect, AppliedActiveGE->Handle);
			
		// The timer manager moves things from the pending list to the active list after checking the active list on the first tick so we need to execute here
		if (AppliedEffectSpec.Def->bExecutePeriodicEffectOnApplication)
		{
			TimerManager.SetTimerForNextTick(Delegate);
		}

		TimerManager.SetTimer(AppliedActiveGE->PeriodHandle, Delegate, AppliedEffectSpec.GetPeriod(), true);
	}

	if (InPredictionKey.IsLocalClientKey() == false || IsNetAuthority())	// Clients predicting a GameplayEffect must not call MarkItemDirty
	{
		MarkItemDirty(*AppliedActiveGE);

		ABILITY_LOG(Verbose, TEXT("Added GE: %s. ReplicationID: %d. Key: %d. PredictionLey: %d"), *AppliedActiveGE->Spec.Def->GetName(), AppliedActiveGE->ReplicationID, AppliedActiveGE->ReplicationKey, InPredictionKey.Current);
	}
	else
	{
		// Clients predicting should call MarkArrayDirty to force the internal replication map to be rebuilt.
		MarkArrayDirty();

		// Once replicated state has caught up to this prediction key, we must remove this gameplay effect.
		InPredictionKey.NewRejectOrCaughtUpDelegate(FPredictionKeyEvent::CreateUObject(Owner, &UAbilitySystemComponent::RemoveActiveGameplayEffect_NoReturn, AppliedActiveGE->Handle, -1));
		
	}

	// @note @todo: This is currently assuming (potentially incorrectly) that the inhibition state of a stacked GE won't change
	// as a result of stacking. In reality it could in complicated cases with differing sets of dynamically-granted tags.
	if (ExistingStackableGE)
	{
		OnStackCountChange(*ExistingStackableGE, StartingStackCount, NewStackCount);
		
	}
	else
	{
		InternalOnActiveGameplayEffectAdded(*AppliedActiveGE);
	}

	return AppliedActiveGE;
}

/** This is called anytime a new ActiveGameplayEffect is added, on both client and server in all cases */
void FActiveGameplayEffectsContainer::InternalOnActiveGameplayEffectAdded(FActiveGameplayEffect& Effect)
{
	SCOPE_CYCLE_COUNTER(STAT_OnActiveGameplayEffectAdded);

	const UGameplayEffect* EffectDef = Effect.Spec.Def;

	if (EffectDef == nullptr)
	{
		ABILITY_LOG(Error, TEXT("FActiveGameplayEffectsContainer serialized new GameplayEffect with NULL Def!"));
		return;
	}

	GAMEPLAYEFFECT_SCOPE_LOCK();
	UE_VLOG(Owner->OwnerActor ? Owner->OwnerActor : Owner->GetOuter(), LogGameplayEffects, Log, TEXT("Added: %s"), *GetNameSafe(EffectDef->GetClass()));

	// Add our ongoing tag requirements to the dependency map. We will actually check for these tags below.
	for (const FGameplayTag& Tag : EffectDef->OngoingTagRequirements.IgnoreTags)
	{
		ActiveEffectTagDependencies.FindOrAdd(Tag).Add(Effect.Handle);
	}

	for (const FGameplayTag& Tag : EffectDef->OngoingTagRequirements.RequireTags)
	{
		ActiveEffectTagDependencies.FindOrAdd(Tag).Add(Effect.Handle);
	}

	// Add any external dependencies that might dirty the effect, if necessary
	AddCustomMagnitudeExternalDependencies(Effect);

	// Check if we should actually be turned on or not (this will turn us on for the first time)
	static FGameplayTagContainer OwnerTags;
	OwnerTags.Reset();

	Owner->GetOwnedGameplayTags(OwnerTags);
	
	Effect.bIsInhibited = true; // Effect has to start inhibited, if it should be uninhibited, CheckOnGoingTagRequirements will handle that state change
	Effect.CheckOngoingTagRequirements(OwnerTags, *this);
}

void FActiveGameplayEffectsContainer::AddActiveGameplayEffectGrantedTagsAndModifiers(FActiveGameplayEffect& Effect, bool bInvokeGameplayCueEvents)
{
	if (Effect.Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("AddActiveGameplayEffectGrantedTagsAndModifiers called with null Def!"));
		return;
	}

	GAMEPLAYEFFECT_SCOPE_LOCK();

	// Register this ActiveGameplayEffects modifiers with our Attribute Aggregators
	if (Effect.Spec.GetPeriod() <= UGameplayEffect::NO_PERIOD)
	{
		for (int32 ModIdx = 0; ModIdx < Effect.Spec.Modifiers.Num(); ++ModIdx)
		{
			if (Effect.Spec.Def->Modifiers.IsValidIndex(ModIdx) == false)
			{
				// This should not be possible but is happening for us in some replay scenerios. Possibly a backward compat issue: GE def has changed and removed modifiers, but replicated data sends the old number of mods
				ensureMsgf(false, TEXT("Spec Modifiers[%d] (max %d) is invalid with Def (%s) modifiers (max %d)"), ModIdx, Effect.Spec.Modifiers.Num(), *GetNameSafe(Effect.Spec.Def), Effect.Spec.Def ? Effect.Spec.Def->Modifiers.Num() : -1);
				continue;
			}

			const FGameplayModifierInfo &ModInfo = Effect.Spec.Def->Modifiers[ModIdx];

			// skip over any modifiers for attributes that we don't have
			if (!Owner || Owner->HasAttributeSetForAttribute(ModInfo.Attribute) == false)
			{
				continue;
			}

			// Note we assume the EvaluatedMagnitude is up to do. There is no case currently where we should recalculate magnitude based on
			// Ongoing tags being met. We either calculate magnitude one time, or its done via OnDirty calls (or potentially a frequency timer one day)

			float EvaluatedMagnitude = Effect.Spec.GetModifierMagnitude(ModIdx, true);	// Note this could cause an attribute aggregator to be created, so must do this before calling/caching the Aggregator below!

			FAggregator* Aggregator = FindOrCreateAttributeAggregator(Effect.Spec.Def->Modifiers[ModIdx].Attribute).Get();
			if (ensure(Aggregator))
			{
				Aggregator->AddAggregatorMod(EvaluatedMagnitude, ModInfo.ModifierOp, ModInfo.EvaluationChannelSettings.GetEvaluationChannel(), &ModInfo.SourceTags, &ModInfo.TargetTags, Effect.PredictionKey.WasLocallyGenerated(), Effect.Handle);
			}
		}
	}

	// Update our owner with the tags this GameplayEffect grants them
	Owner->UpdateTagMap(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags, 1);
	Owner->UpdateTagMap(Effect.Spec.DynamicGrantedTags, 1);
	if (ShouldUseMinimalReplication())
	{
		Owner->AddMinimalReplicationGameplayTags(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags);
		Owner->AddMinimalReplicationGameplayTags(Effect.Spec.DynamicGrantedTags);
	}

	// Immunity
	ApplicationImmunityGameplayTagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.RequireTags, 1);
	ApplicationImmunityGameplayTagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.IgnoreTags, 1);

	if (Effect.Spec.Def->HasGrantedApplicationImmunityQuery)
	{	
		ApplicationImmunityQueryEffects.Add(Effect.Spec.Def);
	}

	// Grant abilities
	if (IsNetAuthority() && !Owner->bSuppressGrantAbility)
	{
		for (FGameplayAbilitySpecDef& AbilitySpecDef : Effect.Spec.GrantedAbilitySpecs)
		{
			// Only do this if we haven't assigned the ability yet! This prevents cases where stacking GEs
			// would regrant the ability every time the stack was applied
			if (AbilitySpecDef.AssignedHandle.IsValid() == false)
			{
				Owner->GiveAbility( FGameplayAbilitySpec(AbilitySpecDef, Effect.Spec.GetLevel(), Effect.Handle) );

				ABILITY_LOG(Display, TEXT("::AddActiveGameplayEffectGrantedTagsAndModifiers granted ability %s (Handle %s) from GE %s (Handle: %s)"), *GetNameSafe(AbilitySpecDef.Ability), *AbilitySpecDef.AssignedHandle.ToString(), *Effect.GetDebugString(), *Effect.Handle.ToString());
			}
		}	
	}

	// Update GameplayCue tags and events
	if (!Owner->bSuppressGameplayCues)
	{
		for (const FGameplayEffectCue& Cue : Effect.Spec.Def->GameplayCues)
		{
			Owner->UpdateTagMap(Cue.GameplayCueTags, 1);

			if (bInvokeGameplayCueEvents)
			{
				Owner->InvokeGameplayCueEvent(Effect.Spec, EGameplayCueEvent::OnActive);
				Owner->InvokeGameplayCueEvent(Effect.Spec, EGameplayCueEvent::WhileActive);
			}

			if (ShouldUseMinimalReplication())
			{
				for (const FGameplayTag& CueTag : Cue.GameplayCueTags)
				{
					// We are now replicating the EffectContext in minimally replicated cues. It may be worth allowing this be determined on a per cue basis one day.
					// (not sending the EffectContext can make things wrong. E.g, the EffectCauser becomes the target of the GE rather than the source)
					Owner->AddGameplayCue_MinimalReplication(CueTag, Effect.Spec.GetEffectContext());
				}
			}
		}
	}

	// Generic notify for anyone listening
	Owner->OnActiveGameplayEffectAddedDelegateToSelf.Broadcast(Owner, Effect.Spec, Effect.Handle);
}

/** Called on server to remove a GameplayEffect */
bool FActiveGameplayEffectsContainer::RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove)
{
	// Iterating through manually since this is a removal operation and we need to pass the index into InternalRemoveActiveGameplayEffect
	int32 NumGameplayEffects = GetNumGameplayEffects();
	for (int32 ActiveGEIdx = 0; ActiveGEIdx < NumGameplayEffects; ++ActiveGEIdx)
	{
		FActiveGameplayEffect& Effect = *GetActiveGameplayEffect(ActiveGEIdx);
		if (Effect.Handle == Handle && Effect.IsPendingRemove == false)
		{
			UE_VLOG(Owner->OwnerActor, LogGameplayEffects, Log, TEXT("Removed: %s"), *GetNameSafe(Effect.Spec.Def->GetClass()));
			if (UE_LOG_ACTIVE(VLogAbilitySystem, Log))
			{
				ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("Removed %s"), *Effect.Spec.Def->GetFName().ToString());
				for (FGameplayModifierInfo Modifier : Effect.Spec.Def->Modifiers)
				{
					float Magnitude = 0.f;
					Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Effect.Spec, Magnitude);
					ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EGameplayModOpToString(Modifier.ModifierOp), Magnitude);
				}
			}

			InternalRemoveActiveGameplayEffect(ActiveGEIdx, StacksToRemove, true);
			return true;
		}
	}
	ABILITY_LOG(Log, TEXT("RemoveActiveGameplayEffect called with invalid Handle: %s"), *Handle.ToString());
	return false;
}

/** Called by server to actually remove a GameplayEffect */
bool FActiveGameplayEffectsContainer::InternalRemoveActiveGameplayEffect(int32 Idx, int32 StacksToRemove, bool bPrematureRemoval)
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveActiveGameplayEffect);

	bool IsLocked = (ScopedLockCount > 0);	// Cache off whether we were previously locked
	GAMEPLAYEFFECT_SCOPE_LOCK();			// Apply lock so no one else can change the AGE list (we may still change it if IsLocked is false)
	
	if (ensure(Idx < GetNumGameplayEffects()))
	{
		FActiveGameplayEffect& Effect = *GetActiveGameplayEffect(Idx);
		if (!ensure(!Effect.IsPendingRemove))
		{
			// This effect is already being removed. This probably means a bug at the callsite, but we can handle it gracefully here by earlying out and pretending the effect was removed.
			return true;
		}

		ABILITY_LOG(Verbose, TEXT("InternalRemoveActiveGameplayEffect: Auth: %s Handle: %s Def: %s"), IsNetAuthority() ? TEXT("TRUE") : TEXT("FALSE"), *Effect.Handle.ToString(), Effect.Spec.Def ? *Effect.Spec.Def->GetName() : TEXT("NONE"));

		FGameplayEffectRemovalInfo GameplayEffectRemovalInfo;
		GameplayEffectRemovalInfo.StackCount = Effect.Spec.StackCount;
		GameplayEffectRemovalInfo.bPrematureRemoval = bPrematureRemoval;
		GameplayEffectRemovalInfo.EffectContext = Effect.Spec.GetEffectContext();

		if (StacksToRemove > 0 && Effect.Spec.StackCount > StacksToRemove)
		{
			// This won't be a full remove, only a change in StackCount.
			int32 StartingStackCount = Effect.Spec.StackCount;
			Effect.Spec.StackCount -= StacksToRemove;
			OnStackCountChange(Effect, StartingStackCount, Effect.Spec.StackCount);
			return false;
		}

		// Invoke Remove GameplayCue event
		bool ShouldInvokeGameplayCueEvent = true;
		const bool bIsNetAuthority = IsNetAuthority();
		if (!bIsNetAuthority && Effect.PredictionKey.IsLocalClientKey() && Effect.PredictionKey.WasReceived() == false)
		{
			// This was an effect that we predicted. Don't invoke GameplayCue event if we have another GameplayEffect that shares the same predictionkey and was received from the server
			if (HasReceivedEffectWithPredictedKey(Effect.PredictionKey))
			{
				ShouldInvokeGameplayCueEvent = false;
			}
		}

		// Don't invoke the GC event if the effect is inhibited, and thus the GC is already not active
		ShouldInvokeGameplayCueEvent &= !Effect.bIsInhibited;

		// Mark the effect pending remove, and remove all side effects from the effect
		InternalOnActiveGameplayEffectRemoved(Effect, ShouldInvokeGameplayCueEvent, GameplayEffectRemovalInfo);

		if (Effect.DurationHandle.IsValid())
		{
			Owner->GetWorld()->GetTimerManager().ClearTimer(Effect.DurationHandle);
		}
		if (Effect.PeriodHandle.IsValid())
		{
			Owner->GetWorld()->GetTimerManager().ClearTimer(Effect.PeriodHandle);
		}

		if (bIsNetAuthority && Owner->OwnerActor)
		{
			Owner->OwnerActor->FlushNetDormancy();
		}

		// Remove this handle from the global map
		Effect.Handle.RemoveFromGlobalMap();

		// Attempt to apply expiration effects, if necessary
		InternalApplyExpirationEffects(Effect.Spec, bPrematureRemoval);

		bool ModifiedArray = false;

		// Finally remove the ActiveGameplayEffect
		if (IsLocked)
		{
			// We are locked, so this removal is now pending.
			PendingRemoves++;

			ABILITY_LOG(Verbose, TEXT("InternalRemoveActiveGameplayEffect while locked; Counting as a Pending Remove: Auth: %s Handle: %s Def: %s"), IsNetAuthority() ? TEXT("TRUE") : TEXT("FALSE"), *Effect.Handle.ToString(), Effect.Spec.Def ? *Effect.Spec.Def->GetName() : TEXT("NONE"));
		}
		else
		{
			// Not locked, so do the removal right away.

			// If we are not scope locked, then there is no way this idx should be referring to something on the pending add list.
			// It is possible to remove a GE that is pending add, but it would happen while the scope lock is still in effect, resulting
			// in a pending remove being set.
			check(Idx < GameplayEffects_Internal.Num());

			GameplayEffects_Internal.RemoveAtSwap(Idx);
			ModifiedArray = true;
		}

		MarkArrayDirty();

		// Hack: force netupdate on owner. This isn't really necessary in real gameplay but is nice
		// during debugging where breakpoints or pausing can mess up network update times. Open issue
		// with network team.
		Owner->GetOwner()->ForceNetUpdate();
		
		return ModifiedArray;
	}

	ABILITY_LOG(Warning, TEXT("InternalRemoveActiveGameplayEffect called with invalid index: %d"), Idx);
	return false;
}

/** Called by client and server: This does cleanup that has to happen whether the effect is being removed locally or due to replication */
void FActiveGameplayEffectsContainer::InternalOnActiveGameplayEffectRemoved(FActiveGameplayEffect& Effect, bool bInvokeGameplayCueEvents, const FGameplayEffectRemovalInfo& GameplayEffectRemovalInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_OnActiveGameplayEffectRemoved);

	// Mark the effect as pending removal
	Effect.IsPendingRemove = true;

	if (Effect.Spec.Def)
	{
		// Remove our tag requirements from the dependency map
		RemoveActiveEffectTagDependency(Effect.Spec.Def->OngoingTagRequirements.IgnoreTags, Effect.Handle);
		RemoveActiveEffectTagDependency(Effect.Spec.Def->OngoingTagRequirements.RequireTags, Effect.Handle);

		// Only Need to update tags and modifiers if the gameplay effect is active.
		if (!Effect.bIsInhibited)
		{
			RemoveActiveGameplayEffectGrantedTagsAndModifiers(Effect, bInvokeGameplayCueEvents);
		}

		RemoveCustomMagnitudeExternalDependencies(Effect);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("InternalOnActiveGameplayEffectRemoved called with no GameplayEffect: %s"), *Effect.Handle.ToString());
	}

	Effect.OnRemovedDelegate.Broadcast();
	Effect.OnRemoved_InfoDelegate.Broadcast(GameplayEffectRemovalInfo);
	OnActiveGameplayEffectRemovedDelegate.Broadcast(Effect);
}

void FActiveGameplayEffectsContainer::RemoveActiveGameplayEffectGrantedTagsAndModifiers(const FActiveGameplayEffect& Effect, bool bInvokeGameplayCueEvents)
{
	// Update AttributeAggregators: remove mods from this ActiveGE Handle
	if (Effect.Spec.GetPeriod() <= UGameplayEffect::NO_PERIOD)
	{
		for(const FGameplayModifierInfo& Mod : Effect.Spec.Def->Modifiers)
		{
			if(Mod.Attribute.IsValid())
			{
				FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Mod.Attribute);
				if(RefPtr)
				{
					RefPtr->Get()->RemoveAggregatorMod(Effect.Handle);
				}
			}
		}
	}

	// Update gameplaytag count and broadcast delegate if we are at 0
	Owner->UpdateTagMap(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags, -1);
	Owner->UpdateTagMap(Effect.Spec.DynamicGrantedTags, -1);

	if (ShouldUseMinimalReplication())
	{
		Owner->RemoveMinimalReplicationGameplayTags(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags);
		Owner->RemoveMinimalReplicationGameplayTags(Effect.Spec.DynamicGrantedTags);
	}

	// Immunity
	ApplicationImmunityGameplayTagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.RequireTags, -1);
	ApplicationImmunityGameplayTagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.IgnoreTags, -1);

	if (Effect.Spec.Def->HasGrantedApplicationImmunityQuery)
	{
		ApplicationImmunityQueryEffects.Remove(Effect.Spec.Def);
	}

	// Cancel/remove granted abilities
	if (IsNetAuthority())
	{
		for (const FGameplayAbilitySpecDef& AbilitySpecDef : Effect.Spec.GrantedAbilitySpecs)
		{
			if (AbilitySpecDef.AssignedHandle.IsValid())
			{
				switch(AbilitySpecDef.RemovalPolicy)
				{
				case EGameplayEffectGrantedAbilityRemovePolicy::CancelAbilityImmediately:
					{
						Owner->ClearAbility(AbilitySpecDef.AssignedHandle);
						break;
					}
				case EGameplayEffectGrantedAbilityRemovePolicy::RemoveAbilityOnEnd:
					{
						Owner->SetRemoveAbilityOnEnd(AbilitySpecDef.AssignedHandle);
						break;
					}
				default:
					{
						// Do nothing to granted ability
						break;
					}
				}
			}
		}
	}

	// Update GameplayCue tags and events
	if (!Owner->bSuppressGameplayCues)
	{
		for (const FGameplayEffectCue& Cue : Effect.Spec.Def->GameplayCues)
		{
			Owner->UpdateTagMap(Cue.GameplayCueTags, -1);

			if (bInvokeGameplayCueEvents)
			{
				Owner->InvokeGameplayCueEvent(Effect.Spec, EGameplayCueEvent::Removed);
			}

			if (ShouldUseMinimalReplication())
			{
				for (const FGameplayTag& CueTag : Cue.GameplayCueTags)
				{
					Owner->RemoveGameplayCue_MinimalReplication(CueTag);
				}
			}
		}
	}
}

void FActiveGameplayEffectsContainer::RemoveActiveEffectTagDependency(const FGameplayTagContainer& Tags, FActiveGameplayEffectHandle Handle)
{
	for(const FGameplayTag& Tag : Tags)
	{
		auto Ptr = ActiveEffectTagDependencies.Find(Tag);
		if (Ptr)
		{
			Ptr->Remove(Handle);
			if (Ptr->Num() <= 0)
			{
				ActiveEffectTagDependencies.Remove(Tag);
			}
		}
	}
}

void FActiveGameplayEffectsContainer::AddCustomMagnitudeExternalDependencies(FActiveGameplayEffect& Effect)
{
	const UGameplayEffect* GEDef = Effect.Spec.Def;
	if (GEDef)
	{
		const bool bIsNetAuthority = IsNetAuthority();

		// Check each modifier to see if it has a custom external dependency
		for (const FGameplayModifierInfo& CurMod : GEDef->Modifiers)
		{
			TSubclassOf<UGameplayModMagnitudeCalculation> ModCalcClass = CurMod.ModifierMagnitude.GetCustomMagnitudeCalculationClass();
			if (ModCalcClass)
			{
				const UGameplayModMagnitudeCalculation* ModCalcClassCDO = ModCalcClass->GetDefaultObject<UGameplayModMagnitudeCalculation>();
				if (ModCalcClassCDO)
				{
					// Only register the dependency if acting as net authority or if the calculation class has indicated it wants non-net authorities
					// to be allowed to perform the calculation as well
					UWorld* World = Owner ? Owner->GetWorld() : nullptr;
					FOnExternalGameplayModifierDependencyChange* ExternalDelegate = ModCalcClassCDO->GetExternalModifierDependencyMulticast(Effect.Spec, World);
					if (ExternalDelegate && (bIsNetAuthority || ModCalcClassCDO->ShouldAllowNonNetAuthorityDependencyRegistration()))
					{
						FObjectKey ModCalcClassKey(*ModCalcClass);
						FCustomModifierDependencyHandle* ExistingDependencyHandle = CustomMagnitudeClassDependencies.Find(ModCalcClassKey);
						
						// If the dependency has already been registered for this container, just add the handle of the effect to the existing list
						if (ExistingDependencyHandle)
						{
							ExistingDependencyHandle->ActiveEffectHandles.Add(Effect.Handle);
						}
						// If the dependency is brand new, bind an update to the delegate and cache off the handle
						else
						{
							FCustomModifierDependencyHandle& NewDependencyHandle = CustomMagnitudeClassDependencies.Add(ModCalcClassKey);
							NewDependencyHandle.ActiveDelegateHandle = ExternalDelegate->AddRaw(this, &FActiveGameplayEffectsContainer::OnCustomMagnitudeExternalDependencyFired, ModCalcClass);
							NewDependencyHandle.ActiveEffectHandles.Add(Effect.Handle);
						}
					}
				}
			}
		}
	}
}

void FActiveGameplayEffectsContainer::RemoveCustomMagnitudeExternalDependencies(FActiveGameplayEffect& Effect)
{
	const UGameplayEffect* GEDef = Effect.Spec.Def;
	if (GEDef && CustomMagnitudeClassDependencies.Num() > 0)
	{
		const bool bIsNetAuthority = IsNetAuthority();
		for (const FGameplayModifierInfo& CurMod : GEDef->Modifiers)
		{
			TSubclassOf<UGameplayModMagnitudeCalculation> ModCalcClass = CurMod.ModifierMagnitude.GetCustomMagnitudeCalculationClass();
			if (ModCalcClass)
			{
				const UGameplayModMagnitudeCalculation* ModCalcClassCDO = ModCalcClass->GetDefaultObject<UGameplayModMagnitudeCalculation>();
				if (ModCalcClassCDO)
				{
					UWorld* World = Owner ? Owner->GetWorld() : nullptr;
					FOnExternalGameplayModifierDependencyChange* ExternalDelegate = ModCalcClassCDO->GetExternalModifierDependencyMulticast(Effect.Spec, World);
					if (ExternalDelegate && (bIsNetAuthority || ModCalcClassCDO->ShouldAllowNonNetAuthorityDependencyRegistration()))
					{
						FObjectKey ModCalcClassKey(*ModCalcClass);
						FCustomModifierDependencyHandle* ExistingDependencyHandle = CustomMagnitudeClassDependencies.Find(ModCalcClassKey);
						
						// If this dependency was bound for this effect, remove it
						if (ExistingDependencyHandle)
						{
							ExistingDependencyHandle->ActiveEffectHandles.Remove(Effect.Handle);

							// If this was the last effect for this dependency, unbind the delegate and remove the dependency entirely
							if (ExistingDependencyHandle->ActiveEffectHandles.Num() == 0)
							{
								ExternalDelegate->Remove(ExistingDependencyHandle->ActiveDelegateHandle);
								CustomMagnitudeClassDependencies.Remove(ModCalcClassKey);
							}
						}
					}
				}
			}
		}
	}
}

void FActiveGameplayEffectsContainer::OnCustomMagnitudeExternalDependencyFired(TSubclassOf<UGameplayModMagnitudeCalculation> MagnitudeCalculationClass)
{
	if (MagnitudeCalculationClass)
	{
		FObjectKey ModCalcClassKey(*MagnitudeCalculationClass);
		FCustomModifierDependencyHandle* ExistingDependencyHandle = CustomMagnitudeClassDependencies.Find(ModCalcClassKey);
		if (ExistingDependencyHandle)
		{
			const bool bIsNetAuthority = IsNetAuthority();
			const UGameplayModMagnitudeCalculation* CalcClassCDO = MagnitudeCalculationClass->GetDefaultObject<UGameplayModMagnitudeCalculation>();
			const bool bRequiresDormancyFlush = CalcClassCDO ? !CalcClassCDO->ShouldAllowNonNetAuthorityDependencyRegistration() : false;

			const TSet<FActiveGameplayEffectHandle>& HandlesNeedingUpdate = ExistingDependencyHandle->ActiveEffectHandles;

			// Iterate through all effects, updating the ones that specifically respond to the external dependency being updated
			for (FActiveGameplayEffect& Effect : this)
			{
				if (HandlesNeedingUpdate.Contains(Effect.Handle))
				{
					if (bIsNetAuthority)
					{
						// By default, a dormancy flush should be required here. If a calculation class has requested that
						// non-net authorities can respond to external dependencies, the dormancy flush is skipped as a desired optimization
						if (bRequiresDormancyFlush && Owner && Owner->OwnerActor)
						{
							Owner->OwnerActor->FlushNetDormancy();
						}

						MarkItemDirty(Effect);
					}

					Effect.Spec.CalculateModifierMagnitudes();
					UpdateAllAggregatorModMagnitudes(Effect);
				}
			}
		}
	}
}

void FActiveGameplayEffectsContainer::InternalApplyExpirationEffects(const FGameplayEffectSpec& ExpiringSpec, bool bPrematureRemoval)
{
	GAMEPLAYEFFECT_SCOPE_LOCK();

	check(Owner);

	// Don't allow prediction of expiration effects
	if (IsNetAuthority())
	{
		const UGameplayEffect* ExpiringGE = ExpiringSpec.Def;
		if (ExpiringGE)
		{
			// Determine the appropriate type of effect to apply depending on whether the effect is being prematurely removed or not
			const TArray<TSubclassOf<UGameplayEffect>>& ExpiryEffects = (bPrematureRemoval ? ExpiringGE->PrematureExpirationEffectClasses : ExpiringGE->RoutineExpirationEffectClasses);
			for (const TSubclassOf<UGameplayEffect>& CurExpiryEffect : ExpiryEffects)
			{
				if (CurExpiryEffect)
				{
					const UGameplayEffect* CurExpiryCDO = CurExpiryEffect->GetDefaultObject<UGameplayEffect>();
					check(CurExpiryCDO);

					FGameplayEffectSpec NewSpec;
					NewSpec.InitializeFromLinkedSpec(CurExpiryCDO, ExpiringSpec);
					
					Owner->ApplyGameplayEffectSpecToSelf(NewSpec);
				}
			}
		}
	}
}

void FActiveGameplayEffectsContainer::RestartActiveGameplayEffectDuration(FActiveGameplayEffect& ActiveGameplayEffect)
{
	ActiveGameplayEffect.StartServerWorldTime = GetServerWorldTime();
	ActiveGameplayEffect.CachedStartServerWorldTime = ActiveGameplayEffect.StartServerWorldTime;
	ActiveGameplayEffect.StartWorldTime = GetWorldTime();
	MarkItemDirty(ActiveGameplayEffect);

	OnDurationChange(ActiveGameplayEffect);
}

void FActiveGameplayEffectsContainer::OnOwnerTagChange(FGameplayTag TagChange, int32 NewCount)
{
	// It may be beneficial to do a scoped lock on attribute re-evaluation during this function

	auto Ptr = ActiveEffectTagDependencies.Find(TagChange);
	if (Ptr)
	{
		GAMEPLAYEFFECT_SCOPE_LOCK();

		FGameplayTagContainer OwnerTags;
		Owner->GetOwnedGameplayTags(OwnerTags);

		TSet<FActiveGameplayEffectHandle>& Handles = *Ptr;
		for (const FActiveGameplayEffectHandle& Handle : Handles)
		{
			FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
			if (ActiveEffect)
			{
				ActiveEffect->CheckOngoingTagRequirements(OwnerTags, *this, true);
			}
		}
	}
}

bool FActiveGameplayEffectsContainer::HasApplicationImmunityToSpec(const FGameplayEffectSpec& SpecToApply, const FActiveGameplayEffect*& OutGEThatProvidedImmunity) const
{
	SCOPE_CYCLE_COUNTER(STAT_HasApplicationImmunityToSpec)

	const FGameplayTagContainer* AggregatedSourceTags = SpecToApply.CapturedSourceTags.GetAggregatedTags();
	if (!ensure(AggregatedSourceTags))
	{
		return false;
	}

	// Query
	for (const UGameplayEffect* EffectDef : ApplicationImmunityQueryEffects)
	{
		if (EffectDef->GrantedApplicationImmunityQuery.Matches(SpecToApply))
		{
			// This is blocked, but who blocked? Search for that Active GE
			for (const FActiveGameplayEffect& Effect : this)
			{
				if (Effect.Spec.Def == EffectDef)
				{
					OutGEThatProvidedImmunity = &Effect;
					return true;
				}
			}
			ABILITY_LOG(Error, TEXT("Application Immunity was triggered for Applied GE: %s by Granted GE: %s. But this GE was not found in the Active GameplayEffects list!"), *GetNameSafe(SpecToApply.Def), *GetNameSafe( EffectDef));
			break;
		}
	}

	// Quick map test
	if (!AggregatedSourceTags->HasAny(ApplicationImmunityGameplayTagCountContainer.GetExplicitGameplayTags()))
	{
		return false;
	}

	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Spec.Def->GrantedApplicationImmunityTags.IsEmpty() == false && Effect.Spec.Def->GrantedApplicationImmunityTags.RequirementsMet( *AggregatedSourceTags ))
		{
			OutGEThatProvidedImmunity = &Effect;
			return true;
		}
	}

	return false;
}

bool FActiveGameplayEffectsContainer::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	if (Owner)
	{
		EReplicationMode ReplicationMode = Owner->ReplicationMode;
		if (ReplicationMode == EReplicationMode::Minimal)
		{
			return false;
		}
		else if (ReplicationMode == EReplicationMode::Mixed)
		{
			if (UPackageMapClient* Client = Cast<UPackageMapClient>(DeltaParms.Map))
			{
				UNetConnection* Connection = Client->GetConnection();

				// Even in mixed mode, we should always replicate out to replays so it has all information.
				if (Connection->GetDriver()->NetDriverName != NAME_DemoNetDriver)
				{
					// In mixed mode, we only want to replicate to the owner of this channel, minimal replication
					// data will go to everyone else.
					if (!Owner->GetOwner()->IsOwnedBy(Connection->OwningActor))
					{
						return false;
					}
				}
			}
		}
	}

	bool RetVal = FastArrayDeltaSerialize<FActiveGameplayEffect>(GameplayEffects_Internal, DeltaParms, *this);

	// After the array has been replicated, invoke GC events ONLY if the effect is not inhibited
	// We postpone this check because in the same net update we could receive multiple GEs that affect if one another is inhibited
	
	if (DeltaParms.Writer == nullptr && Owner != nullptr)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ActiveGameplayEffectsContainer_NetDeltaSerialize_CheckRepGameplayCues);

		if (!DeltaParms.bOutHasMoreUnmapped) // Do not invoke GCs when we have missing information (like AActor*s in EffectContext)
		{
			if (Owner->IsReadyForGameplayCues())
			{
				Owner->HandleDeferredGameplayCues(this);
			}
		}
	}

	return RetVal;
}

void FActiveGameplayEffectsContainer::Uninitialize()
{
	for (FActiveGameplayEffect& CurEffect : this)
	{
		RemoveCustomMagnitudeExternalDependencies(CurEffect);
	}
	ensure(CustomMagnitudeClassDependencies.Num() == 0);
}

float FActiveGameplayEffectsContainer::GetServerWorldTime() const
{
	UWorld* World = Owner->GetWorld();
	AGameStateBase* GameState = World->GetGameState();
	if (GameState)
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

float FActiveGameplayEffectsContainer::GetWorldTime() const
{
	UWorld *World = Owner->GetWorld();
	return World->GetTimeSeconds();
}

void FActiveGameplayEffectsContainer::CheckDuration(FActiveGameplayEffectHandle Handle)
{
	GAMEPLAYEFFECT_SCOPE_LOCK();
	// Intentionally iterating through only the internal list since we need to pass the index for removal
	// and pending effects will never need to be checked for duration expiration (They will be added to the real list first)
	for (int32 ActiveGEIdx = 0; ActiveGEIdx < GameplayEffects_Internal.Num(); ++ActiveGEIdx)
	{
		FActiveGameplayEffect& Effect = GameplayEffects_Internal[ActiveGEIdx];
		if (Effect.Handle == Handle)
		{
			if (Effect.IsPendingRemove)
			{
				// break is this effect is pending remove. 
				// (Note: don't combine this with the above if statement that is looking for the effect via handle, since we want to stop iteration if we find a matching handle but are pending remove).
				break;
			}

			FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();

			// The duration may have changed since we registered this callback with the timer manager.
			// Make sure that this effect should really be destroyed now
			float Duration = Effect.GetDuration();
			float CurrentTime = GetWorldTime();

			int32 StacksToRemove = -2;
			bool RefreshStartTime = false;
			bool RefreshDurationTimer = false;
			bool CheckForFinalPeriodicExec = false;

			if (Duration > 0.f && (((Effect.StartWorldTime + Duration) < CurrentTime) || FMath::IsNearlyZero(CurrentTime - Duration - Effect.StartWorldTime, KINDA_SMALL_NUMBER)))
			{
				// Figure out what to do based on the expiration policy
				switch(Effect.Spec.Def->StackExpirationPolicy)
				{
				case EGameplayEffectStackingExpirationPolicy::ClearEntireStack:
					StacksToRemove = -1; // Remove all stacks
					CheckForFinalPeriodicExec = true;					
					break;

				case EGameplayEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration:
					StacksToRemove = 1;
					CheckForFinalPeriodicExec = (Effect.Spec.StackCount == 1);
					RefreshStartTime = true;
					RefreshDurationTimer = true;
					break;
				case EGameplayEffectStackingExpirationPolicy::RefreshDuration:
					RefreshStartTime = true;
					RefreshDurationTimer = true;
					break;
				};					
			}
			else
			{
				// Effect isn't finished, just refresh its duration timer
				RefreshDurationTimer = true;
			}

			if (CheckForFinalPeriodicExec)
			{
				// This gameplay effect has hit its duration. Check if it needs to execute one last time before removing it.
				if (Effect.PeriodHandle.IsValid() && TimerManager.TimerExists(Effect.PeriodHandle))
				{
					float PeriodTimeRemaining = TimerManager.GetTimerRemaining(Effect.PeriodHandle);
					if (PeriodTimeRemaining <= KINDA_SMALL_NUMBER && !Effect.bIsInhibited)
					{
						FScopeCurrentGameplayEffectBeingApplied ScopedGEApplication(&Effect.Spec, Owner);

						ExecuteActiveEffectsFrom(Effect.Spec);

						// The above call to ExecuteActiveEffectsFrom could cause this effect to be explicitly removed
						// (for example it could kill the owner and cause the effect to be wiped via death).
						// In that case, we need to early out instead of possibly continueing to the below calls to InternalRemoveActiveGameplayEffect
						if ( Effect.IsPendingRemove )
						{
							break;
						}
					}

					// Forcibly clear the periodic ticks because this effect is going to be removed
					TimerManager.ClearTimer(Effect.PeriodHandle);
				}
			}

			if (StacksToRemove >= -1)
			{
				InternalRemoveActiveGameplayEffect(ActiveGEIdx, StacksToRemove, false);
			}

			if (RefreshStartTime)
			{
				RestartActiveGameplayEffectDuration(Effect);
			}

			if (RefreshDurationTimer)
			{
				// Always reset the timer, since the duration might have been modified
				FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::CheckDurationExpired, Effect.Handle);

				float NewTimerDuration = (Effect.StartWorldTime + Duration) - CurrentTime;
				TimerManager.SetTimer(Effect.DurationHandle, Delegate, NewTimerDuration, false);

				if (Effect.DurationHandle.IsValid() == false)
				{
					ABILITY_LOG(Warning, TEXT("Failed to set new timer in ::CheckDuration. Timer trying to be set for: %.2f. Removing GE instead"), NewTimerDuration);
					if (!Effect.IsPendingRemove)
					{
						InternalRemoveActiveGameplayEffect(ActiveGEIdx, -1, false);
					}
					check(Effect.IsPendingRemove);
				}
			}

			break;
		}
	}
}

bool FActiveGameplayEffectsContainer::CanApplyAttributeModifiers(const UGameplayEffect* GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext)
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsCanApplyAttributeModifiers);

	FGameplayEffectSpec	Spec(GameplayEffect, EffectContext, Level);

	Spec.CalculateModifierMagnitudes();
	
	for(int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
		const FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];
	
		// It only makes sense to check additive operators
		if (ModDef.ModifierOp == EGameplayModOp::Additive)
		{
			if (!ModDef.Attribute.IsValid())
			{
				continue;
			}
			const UAttributeSet* Set = Owner->GetAttributeSubobject(ModDef.Attribute.GetAttributeSetClass());
			float CurrentValue = ModDef.Attribute.GetNumericValueChecked(Set);
			float CostValue = ModSpec.GetEvaluatedMagnitude();

			if (CurrentValue + CostValue < 0.f)
			{
				return false;
			}
		}
	}
	return true;
}

TArray<float> FActiveGameplayEffectsContainer::GetActiveEffectsTimeRemaining(const FGameplayEffectQuery& Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetActiveEffectsTimeRemaining);

	float CurrentTime = GetWorldTime();

	TArray<float>	ReturnList;

	for (const FActiveGameplayEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		float Elapsed = CurrentTime - Effect.StartWorldTime;
		float Duration = Effect.GetDuration();

		ReturnList.Add(Duration - Elapsed);
	}

	// Note: keep one return location to avoid copy operation.
	return ReturnList;
}

TArray<float> FActiveGameplayEffectsContainer::GetActiveEffectsDuration(const FGameplayEffectQuery& Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetActiveEffectsDuration);

	TArray<float>	ReturnList;

	for (const FActiveGameplayEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		ReturnList.Add(Effect.GetDuration());
	}

	// Note: keep one return location to avoid copy operation.
	return ReturnList;
}

TArray<TPair<float,float>> FActiveGameplayEffectsContainer::GetActiveEffectsTimeRemainingAndDuration(const FGameplayEffectQuery& Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetActiveEffectsTimeRemainingAndDuration);

	TArray<TPair<float,float>> ReturnList;

	float CurrentTime = GetWorldTime();

	for (const FActiveGameplayEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		float Elapsed = CurrentTime - Effect.StartWorldTime;
		float Duration = Effect.GetDuration();

		ReturnList.Emplace(Duration - Elapsed, Duration);
	}

	// Note: keep one return location to avoid copy operation.
	return ReturnList;
}

TArray<FActiveGameplayEffectHandle> FActiveGameplayEffectsContainer::GetActiveEffects(const FGameplayEffectQuery& Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetActiveEffects);

	TArray<FActiveGameplayEffectHandle> ReturnList;

	for (const FActiveGameplayEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		ReturnList.Add(Effect.Handle);
	}

	return ReturnList;
}

float FActiveGameplayEffectsContainer::GetActiveEffectsEndTime(const FGameplayEffectQuery& Query) const
{
	float EndTime = 0.f;
	float Duration = 0.f;
	GetActiveEffectsEndTimeAndDuration(Query, EndTime, Duration);
	return EndTime;
}

bool FActiveGameplayEffectsContainer::GetActiveEffectsEndTimeAndDuration(const FGameplayEffectQuery& Query, float& EndTime, float& Duration) const
{
	bool FoundSomething = false;
	
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}
		
		FoundSomething = true;

		float ThisEndTime = Effect.GetEndTime();
		if (ThisEndTime <= UGameplayEffect::INFINITE_DURATION)
		{
			// This is an infinite duration effect, so this end time is indeterminate
			EndTime = -1.f;
			Duration = -1.f;
			return true;
		}

		if (ThisEndTime > EndTime)
		{
			EndTime = ThisEndTime;
			Duration = Effect.GetDuration();
		}
	}
	return FoundSomething;
}

TArray<FActiveGameplayEffectHandle> FActiveGameplayEffectsContainer::GetAllActiveEffectHandles() const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetAllActiveEffectHandles);

	TArray<FActiveGameplayEffectHandle> ReturnList;

	for (const FActiveGameplayEffect& Effect : this)
	{
		ReturnList.Add(Effect.Handle);
	}

	return ReturnList;
}

void FActiveGameplayEffectsContainer::ModifyActiveEffectStartTime(FActiveGameplayEffectHandle Handle, float StartTimeDiff)
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsModifyActiveEffectStartTime);

	FActiveGameplayEffect* Effect = GetActiveGameplayEffect(Handle);

	if (Effect)
	{
		Effect->StartWorldTime += StartTimeDiff;
		Effect->StartServerWorldTime += StartTimeDiff;

		// Check if we are now expired
		CheckDuration(Handle);

		// Broadcast to anyone listening
		OnDurationChange(*Effect);

		MarkItemDirty(*Effect);
	}
}

int32 FActiveGameplayEffectsContainer::RemoveActiveEffects(const FGameplayEffectQuery& Query, int32 StacksToRemove)
{
	// Force a lock because the removals could cause other removals earlier in the array, so iterating backwards is not safe all by itself
	GAMEPLAYEFFECT_SCOPE_LOCK();
	int32 NumRemoved = 0;

	// Manually iterating through in reverse because this is a removal operation
	for (int32 idx = GetNumGameplayEffects() - 1; idx >= 0; --idx)
	{
		const FActiveGameplayEffect& Effect = *GetActiveGameplayEffect(idx);
		if (Effect.IsPendingRemove == false && Query.Matches(Effect))
		{
			InternalRemoveActiveGameplayEffect(idx, StacksToRemove, true);
			++NumRemoved;
		}
	}
	return NumRemoved;
}

int32 FActiveGameplayEffectsContainer::GetActiveEffectCount(const FGameplayEffectQuery& Query, bool bEnforceOnGoingCheck) const
{
	int32 Count = 0;

	for (const FActiveGameplayEffect& Effect : this)
	{
		if (!Effect.bIsInhibited || !bEnforceOnGoingCheck)
		{
			if (Query.Matches(Effect))
			{
				Count += Effect.Spec.StackCount;
			}
		}
	}

	return Count;
}

FOnGameplayAttributeChange& FActiveGameplayEffectsContainer::RegisterGameplayAttributeEvent(FGameplayAttribute Attribute)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return AttributeChangeDelegates.FindOrAdd(Attribute);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FOnGameplayAttributeValueChange& FActiveGameplayEffectsContainer::GetGameplayAttributeValueChangeDelegate(FGameplayAttribute Attribute)
{
	return AttributeValueChangeDelegates.FindOrAdd(Attribute);
}

bool FActiveGameplayEffectsContainer::HasReceivedEffectWithPredictedKey(FPredictionKey PredictionKey) const
{
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.PredictionKey == PredictionKey && Effect.PredictionKey.WasReceived() == true)
		{
			return true;
		}
	}

	return false;
}

bool FActiveGameplayEffectsContainer::HasPredictedEffectWithPredictedKey(FPredictionKey PredictionKey) const
{
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.PredictionKey == PredictionKey && Effect.PredictionKey.WasReceived() == false)
		{
			return true;
		}
	}

	return false;
}

void FActiveGameplayEffectsContainer::GetActiveGameplayEffectDataByAttribute(TMultiMap<FGameplayAttribute, FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData>& EffectMap) const
{
	EffectMap.Empty();

	// Add all of the active gameplay effects
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Spec.Modifiers.Num() == Effect.Spec.Def->Modifiers.Num())
		{
			for (int32 Idx = 0; Idx < Effect.Spec.Modifiers.Num(); ++Idx)
			{
				FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData Data;
				Data.Attribute = Effect.Spec.Def->Modifiers[Idx].Attribute;
				Data.ActivationState = Effect.bIsInhibited ? TEXT("INHIBITED") : TEXT("ACTIVE");
				Data.GameplayEffectName = Effect.Spec.Def->GetName();
				Data.ModifierOp = Effect.Spec.Def->Modifiers[Idx].ModifierOp;
				Data.Magnitude = Effect.Spec.Modifiers[Idx].GetEvaluatedMagnitude();
				if (Effect.Spec.StackCount > 1)
				{
					Data.Magnitude = GameplayEffectUtilities::ComputeStackedModifierMagnitude(Data.Magnitude, Effect.Spec.StackCount, Data.ModifierOp);
				}
				Data.StackCount = Effect.Spec.StackCount;

				EffectMap.Add(Data.Attribute, Data);
			}
		}
	}
#if ENABLE_VISUAL_LOG
	// Add the executed gameplay effects if we recorded them
	for (FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData Data : DebugExecutedGameplayEffects)
	{
		EffectMap.Add(Data.Attribute, Data);
	}
#endif // ENABLE_VISUAL_LOG
}

#if ENABLE_VISUAL_LOG
void FActiveGameplayEffectsContainer::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory ActiveEffectsCategory;
	ActiveEffectsCategory.Category = TEXT("Effects");

	TMultiMap<FGameplayAttribute, FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData> EffectMap;

	GetActiveGameplayEffectDataByAttribute(EffectMap);

	// For each attribute that was modified go through all of its modifiers and list them
	TArray<FGameplayAttribute> AttributeKeys;
	EffectMap.GetKeys(AttributeKeys);

	for (const FGameplayAttribute& Attribute : AttributeKeys)
	{
		float CombinedModifierValue = 0.f;
		ActiveEffectsCategory.Add(TEXT(" --- Attribute --- "), Attribute.GetName());

		TArray<FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData> AttributeEffects;
		EffectMap.MultiFind(Attribute, AttributeEffects);

		for (const FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData& DebugData : AttributeEffects)
		{
			ActiveEffectsCategory.Add(DebugData.GameplayEffectName, DebugData.ActivationState);
			ActiveEffectsCategory.Add(TEXT("Magnitude"), FString::Printf(TEXT("%f"), DebugData.Magnitude));

			if (DebugData.ActivationState != "INHIBITED")
			{
				CombinedModifierValue += DebugData.Magnitude;
			}
		}

		ActiveEffectsCategory.Add(TEXT("Total Modification"), FString::Printf(TEXT("%f"), CombinedModifierValue));
	}

	Snapshot->Status.Add(ActiveEffectsCategory);
}
#endif // ENABLE_VISUAL_LOG

void FActiveGameplayEffectsContainer::DebugCyclicAggregatorBroadcasts(FAggregator* TriggeredAggregator)
{
	for (auto It = AttributeAggregatorMap.CreateIterator(); It; ++It)
	{
		FAggregatorRef AggregatorRef = It.Value();
		FGameplayAttribute Attribute = It.Key();
		if (FAggregator* Aggregator = AggregatorRef.Get())
		{
			if (Aggregator == TriggeredAggregator)
			{
				ABILITY_LOG(Warning, TEXT(" Attribute %s was the triggered aggregator (%s)"), *Attribute.GetName(), *Owner->GetPathName());
			}
			else if (Aggregator->BroadcastingDirtyCount > 0)
			{
				ABILITY_LOG(Warning, TEXT(" Attribute %s is broadcasting dirty (%s)"), *Attribute.GetName(), *Owner->GetPathName());
			}
			else
			{
				continue;
			}

			for (FActiveGameplayEffectHandle Handle : Aggregator->Dependents)
			{
				UAbilitySystemComponent* ASC = Handle.GetOwningAbilitySystemComponent();
				if (ASC)
				{
					ABILITY_LOG(Warning, TEXT("  Dependant (%s) GE: %s"), *ASC->GetPathName(), *GetNameSafe(ASC->GetGameplayEffectDefForHandle(Handle)));
				}
			}
		}
	}
}

void FActiveGameplayEffectsContainer::CloneFrom(const FActiveGameplayEffectsContainer& Source)
{
	// Make a full copy of the source's gameplay effects
	GameplayEffects_Internal = Source.GameplayEffects_Internal;

	// Build our AttributeAggregatorMap by deep copying the source's
	AttributeAggregatorMap.Reset();

	TArray< TPair<FAggregatorRef, FAggregatorRef> >	SwappedAggregators;

	for (auto& It : Source.AttributeAggregatorMap)
	{
		const FGameplayAttribute& Attribute = It.Key;
		const FAggregatorRef& SourceAggregatorRef = It.Value;

		FAggregatorRef& NewAggregatorRef = FindOrCreateAttributeAggregator(Attribute);
		FAggregator* NewAggregator = NewAggregatorRef.Get();
		FAggregator::FOnAggregatorDirty OnDirtyDelegate = NewAggregator->OnDirty;

		// Make full copy of the source aggregator
		*NewAggregator = *SourceAggregatorRef.Get();

		// But restore the OnDirty delegate to point to our proxy ASC
		NewAggregator->OnDirty = OnDirtyDelegate;

		TPair<FAggregatorRef, FAggregatorRef> SwappedPair;
		SwappedPair.Key = SourceAggregatorRef;
		SwappedPair.Value = NewAggregatorRef;

		SwappedAggregators.Add(SwappedPair);
	}

	// Make all of our copied GEs "unique" by giving them a new handle
	TMap<FActiveGameplayEffectHandle, FActiveGameplayEffectHandle> SwappedHandles;

	for (FActiveGameplayEffect& Effect : this)
	{
		// Copy the Spec's context so we can modify it
		Effect.Spec.DuplicateEffectContext();
		Effect.Spec.SetupAttributeCaptureDefinitions();

		// For client only, capture attribute data since this data is constructed for replicated active gameplay effects by default
		Effect.Spec.RecaptureAttributeDataForClone(Source.Owner, Owner);

		FActiveGameplayEffectHandle& NewHandleRef = SwappedHandles.Add(Effect.Handle);
		Effect.Spec.CapturedRelevantAttributes.UnregisterLinkedAggregatorCallbacks(Effect.Handle);

		Effect.Handle = FActiveGameplayEffectHandle::GenerateNewHandle(Owner);
		Effect.Spec.CapturedRelevantAttributes.RegisterLinkedAggregatorCallbacks(Effect.Handle);
		NewHandleRef = Effect.Handle;

		// Update any captured attribute references to the proxy source.
		for (TPair<FAggregatorRef, FAggregatorRef>& SwapAgg : SwappedAggregators)
		{
			Effect.Spec.CapturedRelevantAttributes.SwapAggregator( SwapAgg.Key, SwapAgg.Value );
		}
	}	

	// Now go through our aggregator map and replace dependency references to the source's GEs with our GEs.
	for (auto& It : AttributeAggregatorMap)
	{
		FGameplayAttribute& Attribute = It.Key;
		FAggregatorRef& AggregatorRef = It.Value;
		FAggregator* Aggregator = AggregatorRef.Get();
		if (Aggregator)
		{
			Aggregator->OnActiveEffectDependenciesSwapped(SwappedHandles);
		}
	}

	// Broadcast dirty on everything so that the UAttributeSet properties get updated
	for (auto& It : AttributeAggregatorMap)
	{
		FAggregatorRef& AggregatorRef = It.Value;
		AggregatorRef.Get()->BroadcastOnDirty();
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	Misc
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

namespace GlobalActiveGameplayEffectHandles
{
	static TMap<FActiveGameplayEffectHandle, TWeakObjectPtr<UAbilitySystemComponent>>	Map;
}

void FActiveGameplayEffectHandle::ResetGlobalHandleMap()
{
	GlobalActiveGameplayEffectHandles::Map.Reset();
}

FActiveGameplayEffectHandle FActiveGameplayEffectHandle::GenerateNewHandle(UAbilitySystemComponent* OwningComponent)
{
	static int32 GHandleID=0;
	FActiveGameplayEffectHandle NewHandle(GHandleID++);

	TWeakObjectPtr<UAbilitySystemComponent> WeakPtr(OwningComponent);

	GlobalActiveGameplayEffectHandles::Map.Add(NewHandle, WeakPtr);

	return NewHandle;
}

UAbilitySystemComponent* FActiveGameplayEffectHandle::GetOwningAbilitySystemComponent()
{
	TWeakObjectPtr<UAbilitySystemComponent>* Ptr = GlobalActiveGameplayEffectHandles::Map.Find(*this);
	if (Ptr)
	{
		return Ptr->Get();
	}

	return nullptr;	
}

const UAbilitySystemComponent* FActiveGameplayEffectHandle::GetOwningAbilitySystemComponent() const
{
	TWeakObjectPtr<UAbilitySystemComponent>* Ptr = GlobalActiveGameplayEffectHandles::Map.Find(*this);
	if (Ptr)
	{
		return Ptr->Get();
	}

	return nullptr;
}

void FActiveGameplayEffectHandle::RemoveFromGlobalMap()
{
	GlobalActiveGameplayEffectHandles::Map.Remove(*this);
}

// -----------------------------------------------------------------

FGameplayEffectQuery::FGameplayEffectQuery()
	: EffectSource(nullptr),
	EffectDefinition(nullptr)
{
}

FGameplayEffectQuery::FGameplayEffectQuery(const FGameplayEffectQuery& Other)
{
	*this = Other;
}


FGameplayEffectQuery::FGameplayEffectQuery(FActiveGameplayEffectQueryCustomMatch InCustomMatchDelegate)
	: CustomMatchDelegate(InCustomMatchDelegate),
	EffectSource(nullptr),
	EffectDefinition(nullptr)
{
}

FGameplayEffectQuery::FGameplayEffectQuery(FGameplayEffectQuery&& Other)
{
	*this = MoveTemp(Other);
}

FGameplayEffectQuery& FGameplayEffectQuery::operator=(FGameplayEffectQuery&& Other)
{
	CustomMatchDelegate = MoveTemp(Other.CustomMatchDelegate);
	CustomMatchDelegate_BP = MoveTemp(Other.CustomMatchDelegate_BP);
	OwningTagQuery = MoveTemp(Other.OwningTagQuery);
	EffectTagQuery = MoveTemp(Other.EffectTagQuery);
	SourceTagQuery = MoveTemp(Other.SourceTagQuery);
	ModifyingAttribute = MoveTemp(Other.ModifyingAttribute);
	EffectSource = Other.EffectSource;
	EffectDefinition = Other.EffectDefinition;
	IgnoreHandles = MoveTemp(Other.IgnoreHandles);
	return *this;
}

FGameplayEffectQuery& FGameplayEffectQuery::operator=(const FGameplayEffectQuery& Other)
{
	CustomMatchDelegate = Other.CustomMatchDelegate;
	CustomMatchDelegate_BP = Other.CustomMatchDelegate_BP;
	OwningTagQuery = Other.OwningTagQuery;
	EffectTagQuery = Other.EffectTagQuery;
	SourceTagQuery = Other.SourceTagQuery;
	ModifyingAttribute = Other.ModifyingAttribute;
	EffectSource = Other.EffectSource;
	EffectDefinition = Other.EffectDefinition;
	IgnoreHandles = Other.IgnoreHandles;
	return *this;
}


bool FGameplayEffectQuery::Matches(const FActiveGameplayEffect& Effect) const
{
	// since all of these query conditions must be met to be considered a match, failing
	// any one of them means we can return false

	// Anything in the ignore handle list is an immediate non-match
	if (IgnoreHandles.Contains(Effect.Handle))
	{
		return false;
	}

	if (CustomMatchDelegate.IsBound())
	{
		if (CustomMatchDelegate.Execute(Effect) == false)
		{
			return false;
		}
	}

	if (CustomMatchDelegate_BP.IsBound())
	{
		bool bDelegateMatches = false;
		CustomMatchDelegate_BP.Execute(Effect, bDelegateMatches);
		if (bDelegateMatches == false)
		{
			return false;
		}
	}

	return Matches(Effect.Spec);

}

bool FGameplayEffectQuery::Matches(const FGameplayEffectSpec& Spec) const
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Matches called with no UGameplayEffect def."));
		return false;
	}

	if (OwningTagQuery.IsEmpty() == false)
	{
		// Combine tags from the definition and the spec into one container to match queries that may span both
		// static to avoid memory allocations every time we do a query
		check(IsInGameThread());
		static FGameplayTagContainer TargetTags;
		TargetTags.Reset();
		if (Spec.Def->InheritableGameplayEffectTags.CombinedTags.Num() > 0)
		{
			TargetTags.AppendTags(Spec.Def->InheritableGameplayEffectTags.CombinedTags);
		}
		if (Spec.Def->InheritableOwnedTagsContainer.CombinedTags.Num() > 0)
		{
			TargetTags.AppendTags(Spec.Def->InheritableOwnedTagsContainer.CombinedTags);
		}
		if (Spec.DynamicGrantedTags.Num() > 0)
		{
			TargetTags.AppendTags(Spec.DynamicGrantedTags);
		}
		
		if (OwningTagQuery.Matches(TargetTags) == false)
		{
			return false;
		}
	}

	if (EffectTagQuery.IsEmpty() == false)
	{
		// Combine tags from the definition and the spec into one container to match queries that may span both
		// static to avoid memory allocations every time we do a query
		check(IsInGameThread());
		static FGameplayTagContainer GETags;
		GETags.Reset();
		if (Spec.Def->InheritableGameplayEffectTags.CombinedTags.Num() > 0)
		{
			GETags.AppendTags(Spec.Def->InheritableGameplayEffectTags.CombinedTags);
		}
		if (Spec.DynamicAssetTags.Num() > 0)
		{
			GETags.AppendTags(Spec.DynamicAssetTags);
		}

		if (EffectTagQuery.Matches(GETags) == false)
		{
			return false;
		}
	}

	if (SourceTagQuery.IsEmpty() == false)
	{
		FGameplayTagContainer const& SourceTags = Spec.CapturedSourceTags.GetSpecTags();
		if (SourceTagQuery.Matches(SourceTags) == false)
		{
			return false;
		}
	}

	// if we are looking for ModifyingAttribute go over each of the Spec Modifiers and check the Attributes
	if (ModifyingAttribute.IsValid())
	{
		bool bEffectModifiesThisAttribute = false;

		for (int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
		{
			const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
			const FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];

			if (ModDef.Attribute == ModifyingAttribute)
			{
				bEffectModifiesThisAttribute = true;
				break;
			}
		}
		if (bEffectModifiesThisAttribute == false)
		{
			// effect doesn't modify the attribute we are looking for, no match
			return false;
		}
	}

	// check source object
	if (EffectSource != nullptr)
	{
		if (Spec.GetEffectContext().GetSourceObject() != EffectSource)
		{
			return false;
		}
	}

	// check definition
	if (EffectDefinition != nullptr)
	{
		if (Spec.Def != EffectDefinition.GetDefaultObject())
		{
			return false;
		}
	}

	return true;
}

bool FGameplayEffectQuery::IsEmpty() const
{
	return 
	(
		OwningTagQuery.IsEmpty() &&
		EffectTagQuery.IsEmpty() &&
		SourceTagQuery.IsEmpty() &&
		!ModifyingAttribute.IsValid() &&
		!EffectSource &&
		!EffectDefinition
	);
}

// static
FGameplayEffectQuery FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(const FGameplayTagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeGameplayEffectQuery);
	FGameplayEffectQuery OutQuery;
	OutQuery.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(InTags);
	return OutQuery;
}

// static
FGameplayEffectQuery FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(const FGameplayTagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeGameplayEffectQuery);
	FGameplayEffectQuery OutQuery;
	OutQuery.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(InTags);
	return OutQuery;
}

// static
FGameplayEffectQuery FGameplayEffectQuery::MakeQuery_MatchNoOwningTags(const FGameplayTagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeGameplayEffectQuery);
	FGameplayEffectQuery OutQuery;
	OutQuery.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchNoTags(InTags);
	return OutQuery;
}

// static
FGameplayEffectQuery FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(const FGameplayTagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeGameplayEffectQuery);
	FGameplayEffectQuery OutQuery;
	OutQuery.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(InTags);
	return OutQuery;
}

// static
FGameplayEffectQuery FGameplayEffectQuery::MakeQuery_MatchAllEffectTags(const FGameplayTagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeGameplayEffectQuery);
	FGameplayEffectQuery OutQuery;
	OutQuery.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(InTags);
	return OutQuery;
}

// static
FGameplayEffectQuery FGameplayEffectQuery::MakeQuery_MatchNoEffectTags(const FGameplayTagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeGameplayEffectQuery);
	FGameplayEffectQuery OutQuery;
	OutQuery.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchNoTags(InTags);
	return OutQuery;
}

// static
FGameplayEffectQuery FGameplayEffectQuery::MakeQuery_MatchAnySourceTags(const FGameplayTagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeGameplayEffectQuery);
	FGameplayEffectQuery OutQuery;
	OutQuery.SourceTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(InTags);
	return OutQuery;
}

// static
FGameplayEffectQuery FGameplayEffectQuery::MakeQuery_MatchAllSourceTags(const FGameplayTagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeGameplayEffectQuery);
	FGameplayEffectQuery OutQuery;
	OutQuery.SourceTagQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(InTags);
	return OutQuery;
}

// static
FGameplayEffectQuery FGameplayEffectQuery::MakeQuery_MatchNoSourceTags(const FGameplayTagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeGameplayEffectQuery);
	FGameplayEffectQuery OutQuery;
	OutQuery.SourceTagQuery = FGameplayTagQuery::MakeQuery_MatchNoTags(InTags);
	return OutQuery;
}

bool FGameplayModifierInfo::operator==(const FGameplayModifierInfo& Other) const
{
	if (Attribute != Other.Attribute)
	{
		return false;
	}

	if (ModifierOp != Other.ModifierOp)
	{
		return false;
	}

	if (ModifierMagnitude != Other.ModifierMagnitude)
	{
		return false;
	}

	if (SourceTags.RequireTags.Num() != Other.SourceTags.RequireTags.Num() || !SourceTags.RequireTags.HasAll(Other.SourceTags.RequireTags))
	{
		return false;
	}
	if (SourceTags.IgnoreTags.Num() != Other.SourceTags.IgnoreTags.Num() || !SourceTags.IgnoreTags.HasAll(Other.SourceTags.IgnoreTags))
	{
		return false;
	}

	if (TargetTags.RequireTags.Num() != Other.TargetTags.RequireTags.Num() || !TargetTags.RequireTags.HasAll(Other.TargetTags.RequireTags))
	{
		return false;
	}
	if (TargetTags.IgnoreTags.Num() != Other.TargetTags.IgnoreTags.Num() || !TargetTags.IgnoreTags.HasAll(Other.TargetTags.IgnoreTags))
	{
		return false;
	}

	return true;
}

bool FGameplayModifierInfo::operator!=(const FGameplayModifierInfo& Other) const
{
	return !(*this == Other);
}

void FInheritedTagContainer::UpdateInheritedTagProperties(const FInheritedTagContainer* Parent)
{
	// Make sure we've got a fresh start
	CombinedTags.Reset();

	// Re-add the Parent's tags except the one's we have removed
	if (Parent)
	{
		for (auto Itr = Parent->CombinedTags.CreateConstIterator(); Itr; ++Itr)
		{
			if (!Itr->MatchesAny(Removed))
			{
				CombinedTags.AddTag(*Itr);
			}
		}
	}

	// Add our own tags
	for (auto Itr = Added.CreateConstIterator(); Itr; ++Itr)
	{
		// Remove trumps add for explicit matches but not for parent tags.
		// This lets us remove all inherited tags starting with Foo but still add Foo.Bar
		if (!Removed.HasTagExact(*Itr))
		{
			CombinedTags.AddTag(*Itr);
		}
	}
}

void FInheritedTagContainer::PostInitProperties()
{
	// we shouldn't inherit the added and removed tags from our parents
	// make sure that these fields are clear
	Added.Reset();
	Removed.Reset();
}

void FInheritedTagContainer::AddTag(const FGameplayTag& TagToAdd)
{
	CombinedTags.AddTag(TagToAdd);
}

void FInheritedTagContainer::RemoveTag(FGameplayTag TagToRemove)
{
	CombinedTags.RemoveTag(TagToRemove);
}

void FActiveGameplayEffectsContainer::IncrementLock()
{
	ScopedLockCount++;
}

void FActiveGameplayEffectsContainer::DecrementLock()
{
	if (--ScopedLockCount == 0)
	{
		// ------------------------------------------
		// Move any pending effects onto the real list
		// ------------------------------------------
		FActiveGameplayEffect* PendingGameplayEffect = PendingGameplayEffectHead;
		FActiveGameplayEffect* Stop = *PendingGameplayEffectNext;
		bool ModifiedArray = false;

		while (PendingGameplayEffect != Stop)
		{
			if (!PendingGameplayEffect->IsPendingRemove)
			{
				GameplayEffects_Internal.Add(MoveTemp(*PendingGameplayEffect));
				ModifiedArray = true;
			}
			else
			{
				PendingRemoves--;
			}
			PendingGameplayEffect = PendingGameplayEffect->PendingNext;
		}

		// Reset our pending GameplayEffect linked list
		PendingGameplayEffectNext = &PendingGameplayEffectHead;

		// -----------------------------------------
		// Delete any pending remove effects
		// -----------------------------------------
		for (int32 idx=GameplayEffects_Internal.Num()-1; idx >= 0 && PendingRemoves > 0; --idx)
		{
			FActiveGameplayEffect& Effect = GameplayEffects_Internal[idx];

			if (Effect.IsPendingRemove)
			{
				ABILITY_LOG(Verbose, TEXT("DecrementLock decrementing a pending remove: Auth: %s Handle: %s Def: %s"), IsNetAuthority() ? TEXT("TRUE") : TEXT("FALSE"), *Effect.Handle.ToString(), Effect.Spec.Def ? *Effect.Spec.Def->GetName() : TEXT("NONE"));
				GameplayEffects_Internal.RemoveAtSwap(idx, 1, false);
				ModifiedArray = true;
				PendingRemoves--;
			}
		}

		if (!ensure(PendingRemoves == 0))
		{
			ABILITY_LOG(Error, TEXT("~FScopedActiveGameplayEffectLock has %d pending removes after a scope lock removal"), PendingRemoves);
			PendingRemoves = 0;
		}

		if (ModifiedArray)
		{
			MarkArrayDirty();
		}
	}
}

FScopedActiveGameplayEffectLock::FScopedActiveGameplayEffectLock(FActiveGameplayEffectsContainer& InContainer)
	: Container(InContainer)
{
	Container.IncrementLock();
}

FScopedActiveGameplayEffectLock::~FScopedActiveGameplayEffectLock()
{
	Container.DecrementLock();
}

