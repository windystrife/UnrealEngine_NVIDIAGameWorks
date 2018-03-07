// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MovieScenePossessable.generated.h"

/**
 * MovieScenePossessable is a "typed slot" used to allow the MovieScene to control an already-existing object
 */
USTRUCT()
struct FMovieScenePossessable
{
	GENERATED_USTRUCT_BODY(FMovieScenePossessable)

public:

	/** Default constructor. */
	FMovieScenePossessable() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InitName
	 * @param InitPossessedObjectClass
	 */
	FMovieScenePossessable(const FString& InitName, UClass* InitPossessedObjectClass)
		: Guid(FGuid::NewGuid())
		, Name(InitName)
		, PossessedObjectClass(InitPossessedObjectClass)
	{ }

public:

	/**
	 * Get the unique identifier of the possessed object.
	 *
	 * @return Object GUID.
	 * @see GetName, GetPossessedObjectClass
	 */
	const FGuid& GetGuid() const
	{
		return Guid;
	}

	/** 
	* Set the unique identifier
	*
	* @param InGuid
	*/

	void SetGuid(const FGuid& InGuid)
	{
		Guid = InGuid;
	}

	/**
	 * Get the name of the possessed object.
	 *
	 * @return Object name.
	 * @see GetGuid, GetPossessedObjectClass
	 */
	const FString& GetName() const
	{
		return Name;
	}

	/**
	* Set the name of the possessed object
	*
	* @param InName
	*/
	void SetName(const FString& InName)
	{
		Name = InName;
	}

	/**
	 * Get the class of the possessed object.
	 *
	 * @return Object class.
	 * @see GetGuid, GetName
	 */
	const UClass* GetPossessedObjectClass() const
	{
		return PossessedObjectClass;
	}

	/**
	 * Get the guid of this possessable's parent, if applicable
	 *
	 * @return The guid.
	 */
	const FGuid& GetParent() const
	{
		return ParentGuid;
	}

	/**
	 * Set the guid of this possessable's parent
	 *
	 * @param InParentGuid The guid of this possessable's parent.
	 */
	void SetParent(const FGuid& InParentGuid)
	{
		ParentGuid = InParentGuid;
	}

private:

	/** Unique identifier of the possessable object. */
	// @todo sequencer: Guids need to be handled carefully when the asset is duplicated (or loaded after being copied on disk).
	//					Sometimes we'll need to generate fresh Guids.
	UPROPERTY()
	FGuid Guid;

	/** Name label for this slot */
	// @todo sequencer: Should be editor-only probably
	UPROPERTY()
	FString Name;

	/** Type of the object we'll be possessing */
	// @todo sequencer: Might be able to be editor-only.  We'll see.
	// @todo sequencer: This isn't used for anything yet.  We could use it to gate which types of objects can be bound to a
	// possessable "slot" though.  Or we could use it to generate a "preview" spawnable puppet when previewing with no
	// possessable object available.
	UPROPERTY()
	UClass* PossessedObjectClass;

	/** GUID relating to this possessable's parent, if applicable. */
	UPROPERTY()
	FGuid ParentGuid;
};
