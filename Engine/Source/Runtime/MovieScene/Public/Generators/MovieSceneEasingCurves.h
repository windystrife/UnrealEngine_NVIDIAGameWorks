// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneEasingFunction.h"
#include "Curves/RichCurve.h"
#include "MovieSceneEasingCurves.generated.h"

class UCurveFloat;

UENUM()
enum class EMovieSceneBuiltInEasing : uint8
{
	// Linear easing
	Linear UMETA(Grouping=Linear),
	// Sinusoidal easing
	SinIn UMETA(Grouping=Sinusoidal), SinOut UMETA(Grouping=Sinusoidal), SinInOut UMETA(Grouping=Sinusoidal),
	// Quadratic easing
	QuadIn UMETA(Grouping=Quadratic), QuadOut UMETA(Grouping=Quadratic), QuadInOut UMETA(Grouping=Quadratic),
	// Cubic easing
	CubicIn UMETA(Grouping=Cubic), CubicOut UMETA(Grouping=Cubic), CubicInOut UMETA(Grouping=Cubic),
	// Quartic easing
	QuartIn UMETA(Grouping=Quartic), QuartOut UMETA(Grouping=Quartic), QuartInOut UMETA(Grouping=Quartic),
	// Quintic easing
	QuintIn UMETA(Grouping=Quintic), QuintOut UMETA(Grouping=Quintic), QuintInOut UMETA(Grouping=Quintic),
	// Exponential easing
	ExpoIn UMETA(Grouping=Exponential), ExpoOut UMETA(Grouping=Exponential), ExpoInOut UMETA(Grouping=Exponential),
	// Circular easing
	CircIn UMETA(Grouping=Circular), CircOut UMETA(Grouping=Circular), CircInOut UMETA(Grouping=Circular),
};


UCLASS(BlueprintType, meta=(DisplayName = "Built-In Function"))
class MOVIESCENE_API UMovieSceneBuiltInEasingFunction : public UObject, public IMovieSceneEasingFunction
{
public:
	GENERATED_BODY()

	UMovieSceneBuiltInEasingFunction(const FObjectInitializer& Initiailizer);

	virtual float Evaluate(float InTime) const override;

	UPROPERTY(EditAnywhere, Category=Easing)
	EMovieSceneBuiltInEasing Type;
};


UCLASS(meta=(DisplayName="Curve Asset"))
class MOVIESCENE_API UMovieSceneEasingExternalCurve : public UObject, public IMovieSceneEasingFunction
{
public:
	GENERATED_BODY()

	virtual float Evaluate(float InTime) const override;

	/** Curve data */
	UPROPERTY(EditAnywhere, Category=Easing)
	UCurveFloat* Curve;
};