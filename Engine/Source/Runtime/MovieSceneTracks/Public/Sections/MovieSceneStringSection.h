// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "Sections/IKeyframeSection.h"
#include "Curves/StringCurve.h"
#include "MovieSceneStringSection.generated.h"

/**
 * A single string section
 */
UCLASS(MinimalAPI)
class UMovieSceneStringSection
	: public UMovieSceneSection
	, public IKeyframeSection<FString>
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Evaluates this section at the specified time.
	 *
	 * @param Time The time at which to evaluate the section.
	 * @param DefaultString The default value to return.
	 */
	virtual FString Eval(float Time, const FString& DefaultString) const;

	/**
	 * Get the section's string curve.
	 *
	 * @return String curve.
	 */
	FStringCurve& GetStringCurve()
	{
		return StringCurve;
	}
	const FStringCurve& GetStringCurve() const
	{
		return StringCurve;
	}

public:

	//~ IKeyframeSection interface.

	virtual bool NewKeyIsNewData( float Time, const FString& KeyData ) const;
	virtual bool HasKeys( const FString& KeyData ) const;
	virtual void AddKey( float Time, const FString& KeyData, EMovieSceneKeyInterpolation KeyInterpolation );
	virtual void SetDefault( const FString& KeyData );
	virtual void ClearDefaults() override;

public:

	//~ UMovieSceneSection interface 

	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

private:

	/** Curve data */
	UPROPERTY()
	FStringCurve StringCurve;
};
