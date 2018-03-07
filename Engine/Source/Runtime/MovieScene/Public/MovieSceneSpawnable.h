// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MovieSceneSpawnable.generated.h"

class UMovieSceneSequence;

UENUM()
enum class ESpawnOwnership : uint8
{
	/** The object's lifetime is managed by the sequence that spawned it */
	InnerSequence,

	/** The object's lifetime is managed by the outermost sequence */
	MasterSequence,

	/** Once spawned, the object's lifetime is managed externally. */
	External,
};

/**
 * MovieSceneSpawnable describes an object that can be spawned for this MovieScene
 */
USTRUCT()
struct FMovieSceneSpawnable
{
	GENERATED_BODY()

	FMovieSceneSpawnable()
		: ObjectTemplate(nullptr)
		, Ownership(ESpawnOwnership::InnerSequence)
#if WITH_EDITORONLY_DATA
		, GeneratedClass_DEPRECATED(nullptr)
#endif
	{
	}

	/** FMovieSceneSpawnable initialization constructor */
	FMovieSceneSpawnable(const FString& InitName, UObject& InObjectTemplate)
		: Guid(FGuid::NewGuid())
		, Name(InitName)
		, ObjectTemplate(&InObjectTemplate)
		, Ownership(ESpawnOwnership::InnerSequence)
#if WITH_EDITORONLY_DATA
		, GeneratedClass_DEPRECATED(nullptr)
#endif
	{
		MarkSpawnableTemplate(InObjectTemplate);
	}

public:

	/**
	 * Check if the specified object is a spawnable template
	 * @param InObject 		The object to test
	 * @return true if the specified object is a spawnable template, false otherwise
	 */
	MOVIESCENE_API static bool IsSpawnableTemplate(const UObject& InObject);

	/**
	 * Indicate that the specified object is a spawnable template object
	 * @param InObject 		The object to mark
	 */
	MOVIESCENE_API static void MarkSpawnableTemplate(const UObject& InObject);

	/**
	 * Get the template object for this spawnable
	 *
	 * @return Object template
	 * @see GetGuid, GetName
	 */
	UObject* GetObjectTemplate()
	{
		return ObjectTemplate;
	}

	/**
	 * Get the template object for this spawnable
	 *
	 * @return Object template
	 * @see GetGuid, GetName
	 */
	const UObject* GetObjectTemplate() const
	{
		return ObjectTemplate;
	}

	/**
	 * Copy the specified object into this spawnable's template
	 *
	 * @param InSourceObject The source object to use. This object will be duplicated into the spawnable.
	 * @param MovieSceneSequence The movie scene sequence to which this spawnable belongs
	 */
	MOVIESCENE_API void CopyObjectTemplate(UObject& InSourceObject, UMovieSceneSequence& MovieSceneSequence);

	/**
	 * Get the unique identifier of the spawnable object.
	 *
	 * @return Object GUID.
	 * @see GetClass, GetName
	 */
	const FGuid& GetGuid() const
	{
		return Guid;
	}

	/**
	 * Set the unique identifier of this spawnable object
	 * @param InGuid The new GUID for this spawnable
	 * @note Be careful - this guid may be referenced by spawnable/possessable child-parent relationships.
	 * @see GetGuid
	 */
	void SetGuid(const FGuid& InGuid)
	{
		Guid = InGuid;
	}

	/**
	 * Get the name of the spawnable object.
	 *
	 * @return Object name.
	 * @see GetClass, GetGuid
	 */
	const FString& GetName() const
	{
		return Name;
	}

	/**
	 * Set the name of the spawnable object.
	 *
	 * @InName The desired spawnable name.
	 */
	void SetName(const FString& InName)
	{
		Name = InName;
	}

	/**
	 * Report the specified GUID as being an inner possessable dependency for this spawnable
	 *
	 * @param PossessableGuid The guid pertaining to the inner possessable
	 */
	void AddChildPossessable(const FGuid& PossessableGuid)
	{
		ChildPossessables.AddUnique(PossessableGuid);
	}

	/**
	 * Remove the specified GUID from this spawnables list of dependent possessables
	 *
	 * @param PossessableGuid The guid pertaining to the inner possessable
	 */
	void RemoveChildPossessable(const FGuid& PossessableGuid)
	{
		ChildPossessables.Remove(PossessableGuid);
	}

	/**
	 * @return const access to the child possessables set
	 */
	const TArray<FGuid>& GetChildPossessables() const
	{
		return ChildPossessables;
	}

	/**
	 * Get a value indicating what is responsible for this object once it's spawned
	 */
	ESpawnOwnership GetSpawnOwnership() const
	{
		return Ownership;
	}

	/**
	 * Set a value indicating what is responsible for this object once it's spawned
	 */
	void SetSpawnOwnership(ESpawnOwnership InOwnership)
	{
		Ownership = InOwnership;
	}

private:

	/** Unique identifier of the spawnable object. */
	// @todo sequencer: Guids need to be handled carefully when the asset is duplicated (or loaded after being copied on disk).
	//					Sometimes we'll need to generate fresh Guids.
	UPROPERTY()
	FGuid Guid;

	/** Name label */
	// @todo sequencer: Should be editor-only probably
	UPROPERTY()
	FString Name;
	
	UPROPERTY()
	UObject* ObjectTemplate;

	/** Set of GUIDs to possessable object bindings that are bound to an object inside this spawnable */
	// @todo sequencer: This should be a TSet, but they don't duplicate correctly atm
	UPROPERTY()
	TArray<FGuid> ChildPossessables;

	/** Property indicating where ownership responsibility for this object lies */
	UPROPERTY()
	ESpawnOwnership Ownership;

#if WITH_EDITORONLY_DATA
public:
	/** Deprecated generated class */
	UPROPERTY()
	UClass* GeneratedClass_DEPRECATED;
#endif
};
