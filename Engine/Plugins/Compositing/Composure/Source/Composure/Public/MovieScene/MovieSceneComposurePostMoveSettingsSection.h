// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "Sections/IKeyframeSection.h"
#include "MovieSceneComposurePostMoveSettingsSection.generated.h"


/** Defines channels which represent each property of a post move settings animation. */
enum class EComposurePostMoveSettingsChannel
{
	Pivot,
	Translation,
	RotationAngle,
	Scale,
};


/** Defines axes for animating child properties on the properties of a post move settings animation. */
enum class EComposurePostMoveSettingsAxis
{
	X,
	Y,
	None
};


/** Defines a single key in a post move settings animation .*/
struct FComposurePostMoveSettingsKey
{
	FComposurePostMoveSettingsKey(EComposurePostMoveSettingsChannel InChannel, EComposurePostMoveSettingsAxis InAxis, float InValue)
	{
		Channel = InChannel;
		Axis = InAxis;
		Value = InValue;
	}
	EComposurePostMoveSettingsChannel Channel;
	EComposurePostMoveSettingsAxis Axis;
	float Value;
};


/**
* A movie scene section for animating FComposurePostMoveSettings properties.
*/
UCLASS(MinimalAPI)
class UMovieSceneComposurePostMoveSettingsSection
	: public UMovieSceneSection
	, public IKeyframeSection<FComposurePostMoveSettingsKey>
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneSection interface

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

public:

	/** Gets the curve for the requested channel and axis. Requesting an invalid channel/axis combination results in a fatal failure. */
	COMPOSURE_API FRichCurve& GetCurve(EComposurePostMoveSettingsChannel Channel, EComposurePostMoveSettingsAxis Axis);

	/** Gets the curve for the requested channel and axis. Requesting an invalid channel/axis combination results in a fatal failure. */
	COMPOSURE_API const FRichCurve& GetCurve(EComposurePostMoveSettingsChannel Channel, EComposurePostMoveSettingsAxis Axis) const;

	// IKeyframeSection interface.
	virtual bool NewKeyIsNewData(float Time, const struct FComposurePostMoveSettingsKey& TransformKey) const override;
	virtual bool HasKeys(const struct FComposurePostMoveSettingsKey& TransformKey) const override;
	virtual void AddKey(float Time, const struct FComposurePostMoveSettingsKey& TransformKey, EMovieSceneKeyInterpolation KeyInterpolation) override;
	virtual void SetDefault(const struct FComposurePostMoveSettingsKey& TransformKey) override;
	virtual void ClearDefaults() override;

private:
	/** Gets an array of all of the curves in this section. */
	void GetAllCurves(TArray<FRichCurve*>& AllCurves);

	/** Gets an array of all of the curves in this section. */
	void GetAllCurves(TArray<const FRichCurve*>& AllCurves) const;

private:

	/** The curves for animating the pivot property. */
	UPROPERTY()
	FRichCurve Pivot[2];

	/** The curves for animating the translation property. */
	UPROPERTY()
	FRichCurve Translation[2];

	/** The curve for animating the rotation angle property. */
	UPROPERTY()
	FRichCurve RotationAngle;

	/** The curve for animating the scale property. */
	UPROPERTY()
	FRichCurve Scale;
};
