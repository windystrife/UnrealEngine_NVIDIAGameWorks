// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/IntegralCurve.h"
#include "MovieSceneSection.h"
#include "Sections/IKeyframeSection.h"
#include "MovieSceneIntegerSection.generated.h"


/**
 * A single integer section.
 */
UCLASS(MinimalAPI)
class UMovieSceneIntegerSection 
	: public UMovieSceneSection
	, public IKeyframeSection<int32>
{
	GENERATED_UCLASS_BODY()
public:

	/** Gets all the keys of this integer section */
	FIntegralCurve& GetCurve() { return IntegerCurve; }
	const FIntegralCurve& GetCurve() const { return IntegerCurve; }

public:

	//~ IKeyframeSection interface

	virtual void AddKey(float Time, const int32& Value, EMovieSceneKeyInterpolation KeyInterpolation) override;
	virtual bool NewKeyIsNewData(float Time, const int32& Value) const override;
	virtual bool HasKeys(const int32& Value) const override;
	virtual void SetDefault(const int32& Value) override;
	virtual void ClearDefaults() override;

public:

	//~ UMovieSceneSection interface 

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

private:

	/** Ordered curve data */
	UPROPERTY()
	FIntegralCurve IntegerCurve;
};
