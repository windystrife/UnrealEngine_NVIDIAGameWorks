// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayModMagnitudeCalculation.h"

UGameplayModMagnitudeCalculation::UGameplayModMagnitudeCalculation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAllowNonNetAuthorityDependencyRegistration(false)
{
}

float UGameplayModMagnitudeCalculation::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	return 0.f;
}

FOnExternalGameplayModifierDependencyChange* UGameplayModMagnitudeCalculation::GetExternalModifierDependencyMulticast(const FGameplayEffectSpec& Spec, UWorld* World) const
{
	return nullptr;
}

bool UGameplayModMagnitudeCalculation::ShouldAllowNonNetAuthorityDependencyRegistration() const
{
	ensureMsgf(!bAllowNonNetAuthorityDependencyRegistration || RelevantAttributesToCapture.Num() == 0, TEXT("Cannot have a client-side based custom mod calculation that relies on attribute capture!"));
	return bAllowNonNetAuthorityDependencyRegistration;
}

bool UGameplayModMagnitudeCalculation::GetCapturedAttributeMagnitude(const FGameplayEffectAttributeCaptureDefinition& Def, const FGameplayEffectSpec& Spec, const FAggregatorEvaluateParameters& EvaluationParameters, OUT float& Magnitude) const
{
	const FGameplayEffectAttributeCaptureSpec* CaptureSpec = Spec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(Def, true);
	if (CaptureSpec == nullptr)
	{
		ABILITY_LOG(Error, TEXT("GetCapturedAttributeMagnitude unable to get capture spec."));
		return false;
	}
	if (CaptureSpec->AttemptCalculateAttributeMagnitude(EvaluationParameters, Magnitude) == false)
	{
		ABILITY_LOG(Error, TEXT("GetCapturedAttributeMagnitude unable to calculate Health attribute magnitude."));
		return false;
	}

	return true;
}
