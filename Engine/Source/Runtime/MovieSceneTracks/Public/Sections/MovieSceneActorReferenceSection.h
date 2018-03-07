// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "Curves/IntegralCurve.h"
#include "Sections/IKeyframeSection.h"
#include "MovieSceneActorReferenceSection.generated.h"

/**
 * A single actor reference point section
 */
UCLASS(MinimalAPI)
class UMovieSceneActorReferenceSection
	: public UMovieSceneSection
	, public IKeyframeSection<FGuid>
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	MOVIESCENETRACKS_API FGuid Eval(float Position) const;

	// IKeyframeSection interface.

	virtual void AddKey(float Time, const FGuid& Value, EMovieSceneKeyInterpolation KeyInterpolation) override;
	virtual bool NewKeyIsNewData(float Time, const FGuid& Value) const override;
	virtual bool HasKeys(const FGuid& Value) const override;
	virtual void SetDefault(const FGuid& Value) override;
	virtual void ClearDefaults() override;

	/**
	 * @return The integral curve on this section
	 */
	FIntegralCurve& GetActorReferenceCurve()
	{
		return ActorGuidIndexCurve;
	}

	const FIntegralCurve& GetActorReferenceCurve() const { return ActorGuidIndexCurve; }

	const TArray<FGuid>& GetActorGuids() const { return ActorGuids; }

public:

	MOVIESCENETRACKS_API FKeyHandle AddKey(float Time, const FGuid& Value);

	//~ UMovieSceneSection interface

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

public:

	//~ UObject interface

	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostLoad() override;

private:

	/** Curve data */
	UPROPERTY()
	FIntegralCurve ActorGuidIndexCurve;

	TArray<FGuid> ActorGuids;

	UPROPERTY()
	TArray<FString> ActorGuidStrings;
};
