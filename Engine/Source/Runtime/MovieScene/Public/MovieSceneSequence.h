// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MovieSceneSignedObject.h"
#include "MovieSceneTrack.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "MovieSceneSequence.generated.h"

class ITargetPlatform;
class UMovieScene;
struct FMovieScenePossessable;

/**
 * Abstract base class for movie scene animations (C++ version).
 */
UCLASS(MinimalAPI)
class UMovieSceneSequence
	: public UMovieSceneSignedObject
{
public:

	GENERATED_BODY()

	MOVIESCENE_API UMovieSceneSequence(const FObjectInitializer& Init);

public:

	/**
	 * Called when Sequencer has created an object binding for a possessable object
	 * 
	 * @param ObjectId The guid used to map to the possessable object.  Note the guid can be bound to multiple objects at once
	 * @param PossessedObject The runtime object which was possessed.
	 * @param Context Optional context required to bind the specified object (for instance, a parent spawnable object)
	 * @see UnbindPossessableObjects
	 */
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) PURE_VIRTUAL(UMovieSceneSequence::BindPossessableObject,);
	
	/**
	 * Check whether the given object can be possessed by this animation.
	 *
	 * @param Object The object to check.
	 * @param InPlaybackContext The current playback context
	 * @return true if the object can be possessed, false otherwise.
	 */
	virtual bool CanPossessObject(UObject& Object, UObject* InPlaybackContext) const PURE_VIRTUAL(UMovieSceneSequence::CanPossessObject, return false;);

	DEPRECATED(4.15, "Please implement LocateBoundObjects instead")
	virtual UObject* FindPossessableObject(const FGuid& ObjectId, UObject* Context) const
	{
		TArray<UObject*, TInlineAllocator<1>> OutObjects;
		LocateBoundObjects(ObjectId, Context, OutObjects);
		return OutObjects.Num() ? OutObjects[0] : nullptr;
	}

	/**
	 * Locate all the objects that correspond to the specified object ID, using the specified context
	 *
	 * @param ObjectId				The unique identifier of the object.
	 * @param Context				Optional context to use to find the required object (for instance, a parent spawnable object)
	 * @param OutObjects			Destination array to add found objects to
	 */
	virtual void LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const PURE_VIRTUAL(UMovieSceneSequence::LocateBoundObjects, );

	/**
	 * Locate all the objects that correspond to the specified object ID, using the specified context
	 *
	 * @param ObjectId				The unique identifier of the object.
	 * @param Context				Optional context to use to find the required object (for instance, a parent spawnable object)
	 * @return An array of all bound objects
	 */
	TArray<UObject*, TInlineAllocator<1>> LocateBoundObjects(const FGuid& ObjectId, UObject* Context) const
	{
		TArray<UObject*, TInlineAllocator<1>> OutObjects;
		LocateBoundObjects(ObjectId, Context, OutObjects);
		return OutObjects;
	}

	DEPRECATED(4.15, "Please use IMovieScenePlayer::FindObjectId or FindPossessableObjectId(UObject&, UObject*) instead.")
	virtual FGuid FindPossessableObjectId(UObject& Object) const PURE_VIRTUAL(UMovieSceneSequence::FindPossessableObjectId, return FGuid(); );

	/**
	 * Attempt to find the guid relating to the specified object
	 *
	 * @param ObjectId				The unique identifier of the object.
	 * @param Context				Optional context to use to find the required object (for instance, a parent spawnable object or its world)
	 * @return The object's guid, or zero guid if the object is not a valid possessable in the current context
	 */
	MOVIESCENE_API FGuid FindPossessableObjectId(UObject& Object, UObject* Context) const;

	/**
	 * Get the movie scene that controls this animation.
	 *
	 * The returned movie scene represents the root movie scene.
	 * It may contain additional child movie scenes.
	 *
	 * @return The movie scene.
	 */
	virtual UMovieScene* GetMovieScene() const PURE_VIRTUAL(UMovieSceneSequence::GetMovieScene(), return nullptr;);

	/**
	 * Get the logical parent object for the supplied object (not necessarily its outer).
	 *
	 * @param Object The object whose parent to get.
	 * @return The parent object, or nullptr if the object has no logical parent.
	 */
	virtual UObject* GetParentObject(UObject* Object) const PURE_VIRTUAL(UMovieSceneSequence::GetParentObject(), return nullptr;);

	/**
	 * Whether objects can be spawned at run-time.
	 *
	 * @return true if objects can be spawned by sequencer, false if only existing objects can be possessed.
	 */
	virtual bool AllowsSpawnableObjects() const { return false; }

	/**
	 * Unbinds all possessable objects from the provided GUID.
	 *
	 * @param ObjectId The guid bound to possessable objects that should be removed.
	 * @see BindPossessableObject
	 */
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) PURE_VIRTUAL(UMovieSceneSequence::UnbindPossessableObjects,);

	/**
	 * Create a spawnable object template from the specified source object
	 *
	 * @param InSourceObject The source object to copy
	 * @param ObjectName The name of the object
	 * @return A new object template of the specified name
	 */
	virtual UObject* MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName) { return nullptr; }

	/**
	 * Specifies whether this sequence allows rebinding of the specified possessable
	 *
	 * @param InPossessable The possessable to check
	 * @return true if rebinding this possessable is valid at runtime, false otherwise
	 */
	virtual bool CanRebindPossessable(const FMovieScenePossessable& InPossessable) const { return false; }

	/**
	 * Specifies whether this sequence can animate the object in question (either as a spawnable or possessable)
	 *
	 * @param	InObject	The object to check
	 * @return true if this object can be animated.
	 */
	virtual bool CanAnimateObject(UObject& InObject) const { return true; }

public:

	MOVIESCENE_API virtual void PostLoad() override;

	MOVIESCENE_API virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITORONLY_DATA
	MOVIESCENE_API virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif

	MOVIESCENE_API virtual void GenerateEvaluationTemplate(FMovieSceneEvaluationTemplate& Template, const FMovieSceneTrackCompilationParams& Params, FMovieSceneSequenceTemplateStore& Store);

	UPROPERTY()
	FCachedMovieSceneEvaluationTemplate EvaluationTemplate;

	UPROPERTY()
	FMovieSceneTrackCompilationParams TemplateParameters;

	UPROPERTY()
	TMap<UObject*, FCachedMovieSceneEvaluationTemplate> InstancedSubSequenceEvaluationTemplates;

public:

	/**
	 * true if the result of GetParentObject is significant in object resolution for LocateBoundObjects.
	 */
	bool AreParentContextsSignificant() const
	{
		return bParentContextsAreSignificant;
	}

protected:

	/**
	 * true if the result of GetParentObject is significant in object resolution for LocateBoundObjects.
	 * When true, if GetParentObject returns nullptr, the PlaybackContext will be used for LocateBoundObjects, other wise the object's parent will be used
	 * When false, the PlaybackContext will always be used for LocateBoundObjects
	 */
	UPROPERTY()
	bool bParentContextsAreSignificant;

public:

#if WITH_EDITOR

	/**
	 * Get the display name for this movie sequence
	 */
	virtual FText GetDisplayName() const { return FText::FromName(GetFName()); }
#endif
};
