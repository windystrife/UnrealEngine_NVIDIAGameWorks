// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorObjectVersion.h"
#include "MovieSceneTrackIdentifier.generated.h"

USTRUCT()
struct FMovieSceneTrackIdentifier
{
	GENERATED_BODY()

	FMovieSceneTrackIdentifier()
		: Value(-1)
	{}

	static FMovieSceneTrackIdentifier Invalid() { return FMovieSceneTrackIdentifier(); }

	FMovieSceneTrackIdentifier& operator++()
	{
		++Value;
		return *this;
	}

	FORCEINLINE friend bool operator==(FMovieSceneTrackIdentifier A, FMovieSceneTrackIdentifier B)
	{
		return A.Value == B.Value;
	}

	FORCEINLINE friend bool operator!=(FMovieSceneTrackIdentifier A, FMovieSceneTrackIdentifier B)
	{
		return A.Value != B.Value;
	}

	FORCEINLINE friend bool operator<(FMovieSceneTrackIdentifier A, FMovieSceneTrackIdentifier B)
	{
		return A.Value < B.Value;
	}

	FORCEINLINE friend bool operator>(FMovieSceneTrackIdentifier A, FMovieSceneTrackIdentifier B)
	{
		return A.Value > B.Value;
	}

	FORCEINLINE friend uint32 GetTypeHash(FMovieSceneTrackIdentifier In)
	{
		return In.Value;
	}

	/** Custom serialized to reduce memory footprint */
	bool Serialize(FArchive& Ar)
	{
		if (Ar.CustomVer(FEditorObjectVersion::GUID) < FEditorObjectVersion::MovieSceneMetaDataSerialization)
		{
			return false;
		}

		Ar << Value;
		return true;
	}

	friend FArchive& operator<<(FArchive& Ar, FMovieSceneTrackIdentifier& Identifier)
	{
		Identifier.Serialize(Ar);
		return Ar;
	}

private:

	friend struct FMovieSceneEvaluationTemplate;
	
	UPROPERTY()
	uint32 Value;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneTrackIdentifier> : public TStructOpsTypeTraitsBase2<FMovieSceneTrackIdentifier>
{
	enum
	{
		WithSerializer = true, WithIdenticalViaEquality = true
	};
};
