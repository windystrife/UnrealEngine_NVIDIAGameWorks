// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSection.h"
#include "MovieSceneParameterSection.generated.h"


/**
 * Structure representing the animated value of a scalar parameter.
 */
struct FScalarParameterNameAndValue
{
	/** Creates a new FScalarParameterAndValue with a parameter name and a value. */
	FScalarParameterNameAndValue(FName InParameterName, float InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the scalar parameter. */
	FName ParameterName;

	/** The animated value of the scalar parameter. */
	float Value;
};


/**
 * Structure representing the animated value of a vector parameter.
 */
struct FVectorParameterNameAndValue
{
	/** Creates a new FVectorParameterAndValue with a parameter name and a value. */
	FVectorParameterNameAndValue(FName InParameterName, FVector InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the vector parameter. */
	FName ParameterName;

	/** The animated value of the vector parameter. */
	FVector Value;
};


/**
 * Structure representing the animated value of a color parameter.
 */
struct FColorParameterNameAndValue
{
	/** Creates a new FColorParameterAndValue with a parameter name and a value. */
	FColorParameterNameAndValue(FName InParameterName, FLinearColor InValue)
	{
		ParameterName = InParameterName;
		Value = InValue;
	}

	/** The name of the color parameter. */
	FName ParameterName;

	/** The animated value of the color parameter. */
	FLinearColor Value;
};


/**
 * Structure representing an animated scalar parameter and it's associated animation curve.
 */
USTRUCT()
struct FScalarParameterNameAndCurve
{
	GENERATED_USTRUCT_BODY()

	FScalarParameterNameAndCurve()
	{
		Index = 0;
	}

	/** Creates a new FScalarParameterNameAndCurve for a specific scalar parameter. */
	FScalarParameterNameAndCurve(FName InParameterName);

	/** The name of the scalar parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	UPROPERTY()
	int32 Index;

	/** The curve which contains the animation data for the scalar parameter. */
	UPROPERTY()
	FRichCurve ParameterCurve;
};


/**
 * Structure representing an animated vector parameter and it's associated animation curve.
 */
USTRUCT()
struct FVectorParameterNameAndCurves
{
	GENERATED_USTRUCT_BODY()

	FVectorParameterNameAndCurves() 
	{
		Index = 0;
	}

	/** Creates a new FVectorParameterNameAndCurve for a specific vector parameter. */
	FVectorParameterNameAndCurves(FName InParameterName);

	/** The name of the vector parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	UPROPERTY()
	int32 Index;

	/** The curve which contains the animation data for the x component of the vector parameter. */
	UPROPERTY()
	FRichCurve XCurve;

	/** The curve which contains the animation data for the y component of the vector parameter. */
	UPROPERTY()
	FRichCurve YCurve;

	/** The curve which contains the animation data for the z component of the vector parameter. */
	UPROPERTY()
	FRichCurve ZCurve;
};


/**
* Structure representing an animated vector parameter and it's associated animation curve.
*/
USTRUCT()
struct FColorParameterNameAndCurves
{
	GENERATED_USTRUCT_BODY()

	FColorParameterNameAndCurves()
	{
		Index = 0;
	}

	/** Creates a new FVectorParameterNameAndCurve for a specific color parameter. */
	FColorParameterNameAndCurves(FName InParameterName);

	/** The name of the vector parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	UPROPERTY()
	int32 Index;

	/** The curve which contains the animation data for the red component of the color parameter. */
	UPROPERTY()
	FRichCurve RedCurve;

	/** The curve which contains the animation data for the green component of the color parameter. */
	UPROPERTY()
	FRichCurve GreenCurve;

	/** The curve which contains the animation data for the blue component of the color parameter. */
	UPROPERTY()
	FRichCurve BlueCurve;

	/** The curve which contains the animation data for the alpha component of the color parameter. */
	UPROPERTY()
	FRichCurve AlphaCurve;
};


/**
 * A single movie scene section which can contain data for multiple named parameters.
 */
UCLASS(MinimalAPI)
class UMovieSceneParameterSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:
	/** Adds a a key for a specific scalar parameter at the specified time with the specified value. */
	void AddScalarParameterKey(FName InParameterName, float InTime, float InValue);

	/** Adds a a key for a specific vector parameter at the specified time with the specified value. */
	void AddVectorParameterKey(FName InParameterName, float InTime, FVector InValue);

	/** Adds a a key for a specific color parameter at the specified time with the specified value. */
	void AddColorParameterKey(FName InParameterName, float InTime, FLinearColor InValue);

	/** 
	 * Removes a scalar parameter from this section. 
	 * 
	 * @param InParameterName The name of the scalar parameter to remove.
	 * @returns True if a parameter with that name was found and removed, otherwise false.
	 */
	MOVIESCENETRACKS_API bool RemoveScalarParameter(FName InParameterName);

	/**
	 * Removes a vector parameter from this section.
	 *
	 * @param InParameterName The name of the vector parameter to remove.
	 * @returns True if a parameter with that name was found and removed, otherwise false.
	 */
	MOVIESCENETRACKS_API bool RemoveVectorParameter(FName InParameterName);

	/**
	 * Removes a color parameter from this section.
	 *
	 * @param InParameterName The name of the color parameter to remove.
	 * @returns True if a parameter with that name was found and removed, otherwise false.
	 */
	MOVIESCENETRACKS_API bool RemoveColorParameter(FName InParameterName);

	/** Gets the animated scalar parameters and their associated curves. */
	MOVIESCENETRACKS_API TArray<FScalarParameterNameAndCurve>* GetScalarParameterNamesAndCurves();
	MOVIESCENETRACKS_API const TArray<FScalarParameterNameAndCurve>& GetScalarParameterNamesAndCurves() const;

	/** Gets the animated vector parameters and their associated curves. */
	MOVIESCENETRACKS_API TArray<FVectorParameterNameAndCurves>* GetVectorParameterNamesAndCurves();
	MOVIESCENETRACKS_API const TArray<FVectorParameterNameAndCurves>& GetVectorParameterNamesAndCurves() const;

	/** Gets the animated color parameters and their associated curves. */
	MOVIESCENETRACKS_API TArray<FColorParameterNameAndCurves>* GetColorParameterNamesAndCurves();
	MOVIESCENETRACKS_API const TArray<FColorParameterNameAndCurves>& GetColorParameterNamesAndCurves() const;

	/** Gets the set of all parameter names used by this section. */
	MOVIESCENETRACKS_API void GetParameterNames(TSet<FName>& ParameterNames) const;

public:

	//~ UMovieSceneSection interface

	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

private:

	void UpdateParameterIndicesFromRemoval(int32 RemovedIndex);
	void GatherCurves(TArray<const FRichCurve*> &OutCurves) const;
	void GatherCurves(TArray<FRichCurve*> &OutCurves);

private:

	/** The scalar parameter names and their associated curves. */
	UPROPERTY()
	TArray<FScalarParameterNameAndCurve> ScalarParameterNamesAndCurves;

	/** The vector parameter names and their associated curves. */
	UPROPERTY()
	TArray<FVectorParameterNameAndCurves> VectorParameterNamesAndCurves;

	/** The vector parameter names and their associated curves. */
	UPROPERTY()
	TArray<FColorParameterNameAndCurves> ColorParameterNamesAndCurves;
};
