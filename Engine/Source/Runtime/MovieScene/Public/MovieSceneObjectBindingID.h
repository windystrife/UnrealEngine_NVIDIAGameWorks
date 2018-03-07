// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneObjectBindingID.generated.h"

struct FMovieSceneSequenceHierarchy;

/** Enumeration specifying how a movie scene object binding ID relates to the sequence */
UENUM()
enum class EMovieSceneObjectBindingSpace : uint8
{
	/** The object binding sequence ID resolves from a local sequence (ie, it may need to accumulate a parent sequence ID before it resolves correctly) */
	Local,
	/** The object binding sequence ID resolves from the root of the sequence */
	Root,
};


/** Persistent identifier to a specific object binding within a sequence hierarchy. */
USTRUCT(BlueprintType, meta=(HasNativeMake))
struct FMovieSceneObjectBindingID
{
	GENERATED_BODY()

	/** Default construction to an invalid object binding ID */
	FMovieSceneObjectBindingID()
		: SequenceID(int32(MovieSceneSequenceID::Root.GetInternalValue())), Space(EMovieSceneObjectBindingSpace::Root)
	{
	}

	/** Construction from an object binding guid, and the specific sequence instance ID in which it resides */
	FMovieSceneObjectBindingID(const FGuid& InGuid, FMovieSceneSequenceID InSequenceID, EMovieSceneObjectBindingSpace InSpace = EMovieSceneObjectBindingSpace::Root)
		: SequenceID(int32(InSequenceID.GetInternalValue())), Space(InSpace), Guid(InGuid)
	{
	}

	/**
	 * Check whether this object binding ID has been set to something valied
	 * @note: does not imply that the ID resolves to a valid object
	 */
	bool IsValid() const
	{
		return Guid.IsValid();
	}

	/**
	 * Access the identifier for the sequence in which the object binding resides
	 */
	FMovieSceneSequenceID GetSequenceID() const
	{
		return FMovieSceneSequenceID(uint32(SequenceID));
	}

	/**
	 * Access the guid that identifies the object binding within the sequence
	 */
	const FGuid& GetGuid() const
	{
		return Guid;
	}
	
	/**
	 * Access how this binding's sequence ID relates to the master sequence
	 */
	EMovieSceneObjectBindingSpace GetBindingSpace() const
	{
		return Space;
	}

	/**
	 * Resolve this binding ID from a local binding to be accessible from the root, by treating the specified local sequence ID as this binding's root
	 */
	MOVIESCENE_API FMovieSceneObjectBindingID ResolveLocalToRoot(FMovieSceneSequenceID LocalSequenceID, const FMovieSceneSequenceHierarchy& Hierarchy) const;

public:

	friend uint32 GetTypeHash(const FMovieSceneObjectBindingID& A)
	{
		return GetTypeHash(A.Guid) ^ GetTypeHash(A.SequenceID);
	}

	friend bool operator==(const FMovieSceneObjectBindingID& A, const FMovieSceneObjectBindingID& B)
	{
		return A.Guid == B.Guid && A.SequenceID == B.SequenceID;
	}

	friend bool operator!=(const FMovieSceneObjectBindingID& A, const FMovieSceneObjectBindingID& B)
	{
		return A.Guid != B.Guid || A.SequenceID != B.SequenceID;
	}

private:

	/** Sequence ID stored as an int32 so that it can be used in the blueprint VM */
	UPROPERTY()
	int32 SequenceID;

	/** The binding's resolution space */
	UPROPERTY()
	EMovieSceneObjectBindingSpace Space;

	/** Identifier for the object binding within the sequence */
	UPROPERTY(EditAnywhere, Category="Binding")
	FGuid Guid;
};