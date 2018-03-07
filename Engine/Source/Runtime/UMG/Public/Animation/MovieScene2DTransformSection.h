// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSection.h"
#include "Slate/WidgetTransform.h"
#include "Sections/IKeyframeSection.h"
#include "MovieScene2DTransformSection.generated.h"

enum class EKey2DTransformChannel
{
	Translation,
	Rotation,
	Scale,
	Shear
};


enum class EKey2DTransformAxis
{
	X,
	Y,
	None
};

struct F2DTransformKey
{
	F2DTransformKey( EKey2DTransformChannel InChannel, EKey2DTransformAxis InAxis, float InValue )
	{
		Channel = InChannel;
		Axis = InAxis;
		Value = InValue;
	}
	EKey2DTransformChannel Channel;
	EKey2DTransformAxis Axis;
	float Value;
};


/**
 * A transform section
 */
UCLASS(MinimalAPI)
class UMovieScene2DTransformSection
	: public UMovieSceneSection
	, public IKeyframeSection<F2DTransformKey>
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneSection interface

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override;
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override;

public:

	UMG_API FRichCurve& GetTranslationCurve( EAxis::Type Axis );
	UMG_API const FRichCurve& GetTranslationCurve( EAxis::Type Axis ) const;

	UMG_API FRichCurve& GetRotationCurve();
	UMG_API const FRichCurve& GetRotationCurve() const;

	UMG_API FRichCurve& GetScaleCurve( EAxis::Type Axis );
	UMG_API const FRichCurve& GetScaleCurve( EAxis::Type Axis ) const;

	DEPRECATED(4.15, "Please use GetShearCurve.")
	UMG_API FRichCurve& GetSheerCurve( EAxis::Type Axis );
	
	UMG_API FRichCurve& GetShearCurve( EAxis::Type Axis );
	UMG_API const FRichCurve& GetShearCurve( EAxis::Type Axis ) const;

	DEPRECATED(4.15, "Evaluation is now the responsibility of FMovieScene2DTransformSectionTemplate")
	FWidgetTransform Eval( float Position, const FWidgetTransform& DefaultValue ) const;

	// IKeyframeSection interface.
	virtual bool NewKeyIsNewData( float Time, const struct F2DTransformKey& TransformKey ) const override;
	virtual bool HasKeys( const struct F2DTransformKey& TransformKey ) const override;
	virtual void AddKey( float Time, const struct F2DTransformKey& TransformKey, EMovieSceneKeyInterpolation KeyInterpolation ) override;
	virtual void SetDefault( const struct F2DTransformKey& TransformKey ) override;
	virtual void ClearDefaults() override;

private:

	/** Translation curves*/
	UPROPERTY()
	FRichCurve Translation[2];
	
	/** Rotation curve */
	UPROPERTY()
	FRichCurve Rotation;

	/** Scale curves */
	UPROPERTY()
	FRichCurve Scale[2];

	/** Shear curve */
	UPROPERTY()
	FRichCurve Shear[2];
};
