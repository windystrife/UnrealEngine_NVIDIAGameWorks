// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSection.h"
#include "Sections/IKeyframeSection.h"
#include "MovieSceneFloatSection.generated.h"

/**
 * A single floating point section
 */
UCLASS( MinimalAPI )
class UMovieSceneFloatSection
	: public UMovieSceneSection
	, public IKeyframeSection<float>
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	virtual float Eval( float Position, float DefaultValue ) const;

	/**
	 * @return The float curve on this section
	 */
	FRichCurve& GetFloatCurve() { return FloatCurve; }
	const FRichCurve& GetFloatCurve() const { return FloatCurve; }

public:

	//~ IKeyframeSection interface

	virtual void AddKey( float Time, const float& Value, EMovieSceneKeyInterpolation KeyInterpolation ) override;
	virtual bool NewKeyIsNewData(float Time, const float& Value) const override;
	virtual bool HasKeys( const float& Value ) const override;
	virtual void SetDefault( const float& Value ) override;
	virtual void ClearDefaults() override;

public:

	//~ UMovieSceneSection interface

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override;
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override;

private:
	
	/** Curve data */
	UPROPERTY()
	FRichCurve FloatCurve;
};
