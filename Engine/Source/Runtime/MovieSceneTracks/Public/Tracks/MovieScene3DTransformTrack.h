// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "MovieScene3DTransformTrack.generated.h"

enum class EMovieSceneTransformChannel : uint32;
class UMovieScene3DTransformSection;
struct FMovieSceneInterrogationKey;

struct FTrajectoryKey
{
	struct FData
	{
		FData(UMovieScene3DTransformSection* InSection, ERichCurveInterpMode InInterpMode, EMovieSceneTransformChannel InChannel = EMovieSceneTransformChannel::None)
			: Section(InSection), InterpMode(InInterpMode), Channel(InChannel)
		{}

		TWeakObjectPtr<UMovieScene3DTransformSection> Section;
		ERichCurveInterpMode InterpMode;
		EMovieSceneTransformChannel Channel;
	};

	FTrajectoryKey(float InTime) : Time(InTime) {}

	bool Is(ERichCurveInterpMode InInterpMode) const
	{
		for (const FData& Value : KeyData)
		{
			if (Value.InterpMode != InInterpMode)
			{
				return false;
			}
		}
		return true;
	}

	float Time;

	TArray<FData, TInlineAllocator<1>> KeyData;
};

/**
 * Handles manipulation of component transforms in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieScene3DTransformTrack
	: public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;

	MOVIESCENETRACKS_API TArray<FTrajectoryKey> GetTrajectoryData(float Time, int32 MaxNumDataPoints) const;

	/**
	 * Access the interrogation key for transform data - any interrgation data stored with this key is guaranteed to be of type 'FTransform'
	 */
	MOVIESCENETRACKS_API static FMovieSceneInterrogationKey GetInterrogationKey();
};
