// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSequence.h"
#include "LevelSequenceObject.h"
#include "LevelSequenceBindingReference.h"
#include "LevelSequenceLegacyObjectReference.h"
#include "LevelSequence.generated.h"

class UMovieScene;

/**
 * Movie scene animation for Actors.
 */
UCLASS(BlueprintType)
class LEVELSEQUENCE_API ULevelSequence
	: public UMovieSceneSequence
{
	GENERATED_UCLASS_BODY()

public:

	/** Pointer to the movie scene that controls this animation. */
	UPROPERTY()
	UMovieScene* MovieScene;

public:

	/** Initialize this level sequence. */
	virtual void Initialize();

	/** Convert old-style lazy object ptrs to new-style references using the specified context */
	void ConvertPersistentBindingsToDefault(UObject* FixupContext);

public:

	// UMovieSceneSequence interface
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object, UObject* InPlaybackContext) const override;
	virtual void LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;
	virtual bool AllowsSpawnableObjects() const override;
	virtual bool CanRebindPossessable(const FMovieScenePossessable& InPossessable) const override;
	virtual UObject* MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName) override;
	virtual bool CanAnimateObject(UObject& InObject) const override;

	virtual void PostLoad() override;

protected:

	/** Legacy object references - should be read-only. Not deprecated because they need to still be saved */
	UPROPERTY()
	FLevelSequenceObjectReferenceMap ObjectReferences;

	/** References to bound objects. */
	UPROPERTY()
	FLevelSequenceBindingReferences BindingReferences;

	/** Deprecated property housing old possessed object bindings */
	UPROPERTY()
	TMap<FString, FLevelSequenceObject> PossessedObjects_DEPRECATED;
};
