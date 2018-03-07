// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "Curves/IntegralCurve.h"
#include "Sections/IKeyframeSection.h"
#include "MovieSceneBoolSection.generated.h"

/**
 * A single bool section.
 */
UCLASS(MinimalAPI)
class UMovieSceneBoolSection 
	: public UMovieSceneSection
	, public IKeyframeSection<bool>
{
	GENERATED_UCLASS_BODY()

public:

	/** The default value to use when no keys are present - use GetCurve().SetDefaultValue() */
	UPROPERTY()
	bool DefaultValue_DEPRECATED;

public:

	/**
	 * Update this section.
	 *
	 * @param Position The position in time within the movie scene.
	 */
	virtual bool Eval(float Position, bool DefaultValue) const;

	/** Gets all the keys of this boolean section. */
	FIntegralCurve& GetCurve() { return BoolCurve; }
	const FIntegralCurve& GetCurve() const { return BoolCurve; }
	
public:

	//~ IKeyframeSection interface

	virtual void AddKey(float Time, const bool& Value, EMovieSceneKeyInterpolation KeyInterpolation) override;
	virtual void SetDefault(const bool& Value) override;
	virtual bool NewKeyIsNewData(float Time, const bool& Value) const override;
	virtual bool HasKeys(const bool& Value) const override;
	virtual void ClearDefaults() override;

public:

	//~ UMovieSceneSection interface 

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

public:

	//~ UObject interface

	virtual void PostLoad() override;

private:

	/** Ordered curve data */
	// @todo Sequencer This could be optimized by packing the bools separately
	// but that may not be worth the effort
	UPROPERTY()
	FIntegralCurve BoolCurve;
};
