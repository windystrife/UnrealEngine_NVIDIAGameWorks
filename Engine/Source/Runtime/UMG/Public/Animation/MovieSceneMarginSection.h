// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSection.h"
#include "Layout/Margin.h"
#include "Sections/IKeyframeSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieSceneMarginSection.generated.h"

enum class EKeyMarginChannel
{
	Left,
	Top,
	Right,
	Bottom
};


struct FMarginKey
{
	FMarginKey( EKeyMarginChannel InChannel, float InValue )
	{
		Channel = InChannel;
		Value = InValue;
	}
	EKeyMarginChannel Channel;
	float Value;
};


/**
 * A section in a Margin track
 */
UCLASS(MinimalAPI)
class UMovieSceneMarginSection 
	: public UMovieSceneSection
	, public IKeyframeSection<FMarginKey>
{
	GENERATED_UCLASS_BODY()
public:
	/** UMovieSceneSection interface */
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override;
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override;

	// IKeyframeSection interface.
	virtual void AddKey( float Time, const FMarginKey& MarginKey, EMovieSceneKeyInterpolation KeyInterpolation ) override;
	virtual bool NewKeyIsNewData(float Time, const FMarginKey& Key ) const override;
	virtual bool HasKeys(const FMarginKey& Key) const override;
	virtual void SetDefault(const FMarginKey& Key) override;
	virtual void ClearDefaults() override;

	/**
	 * Gets the top curve
	 *
	 * @return The rich curve for this margin channel
	 */
	FRichCurve& GetTopCurve() { return TopCurve; }
	const FRichCurve& GetTopCurve() const { return TopCurve; }

	/**
	 * Gets the left curve
	 *
	 * @return The rich curve for this margin channel
	 */
	FRichCurve& GetLeftCurve() { return LeftCurve; }
	const FRichCurve& GetLeftCurve() const { return LeftCurve; }
	/**
	 * Gets the right curve
	 *
	 * @return The rich curve for this margin channel
	 */
	FRichCurve& GetRightCurve() { return RightCurve; }
	const FRichCurve& GetRightCurve() const { return RightCurve; }
	
	/**
	 * Gets the bottom curve
	 *
	 * @return The rich curve for this margin channel
	 */
	FRichCurve& GetBottomCurve() { return BottomCurve; }
	const FRichCurve& GetBottomCurve() const { return BottomCurve; }

private:

	/** Red curve data */
	UPROPERTY()
	FRichCurve TopCurve;

	/** Green curve data */
	UPROPERTY()
	FRichCurve LeftCurve;

	/** Blue curve data */
	UPROPERTY()
	FRichCurve RightCurve;

	/** Alpha curve data */
	UPROPERTY()
	FRichCurve BottomCurve;
};
