// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/IntegralCurve.h"
#include "MovieSceneSection.h"
#include "IKeyframeSection.h"
#include "MovieSceneEnumSection.generated.h"


/**
 * A single enum section.
 */
UCLASS(MinimalAPI)
class UMovieSceneEnumSection 
	: public UMovieSceneSection
	, public IKeyframeSectionEnum
{
	GENERATED_UCLASS_BODY()
public:

	/** Gets all the keys of this enum section */
	FIntegralCurve& GetCurve() { return EnumCurve; }
	const FIntegralCurve& GetCurve() const { return EnumCurve; }

public:

	//~ IKeyframeSection interface

	virtual void AddKey(float Time, const int64& Value, EMovieSceneKeyInterpolation KeyInterpolation) override;
	virtual bool NewKeyIsNewData(float Time, const int64& Value) const override;
	virtual bool HasKeys(const int64& Value) const override;
	virtual void SetDefault(const int64& Value) override;
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
	// @todo Sequencer This could be optimized by packing the enums separately
	// but that may not be worth the effort
	UPROPERTY()
	FIntegralCurve EnumCurve;
};
