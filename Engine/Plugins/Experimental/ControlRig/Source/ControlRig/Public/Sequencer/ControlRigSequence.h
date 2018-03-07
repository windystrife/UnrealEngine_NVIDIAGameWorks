// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "LevelSequence.h"
#include "ControlRigSequenceObjectReference.h"
#include "ControlRigSequence.generated.h"

class UMovieScene;
class UAnimSequence;
class USkeletalMesh;

UCLASS(BlueprintType, Experimental)
class CONTROLRIG_API UControlRigSequence : public ULevelSequence
{
	GENERATED_UCLASS_BODY()

public:

	// ULevelSequence overrides
	virtual void Initialize() override;

	// UMovieSceneSequence overrides
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object, UObject* InPlaybackContext) const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual bool AllowsSpawnableObjects() const { return true; }
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;
	virtual bool CanRebindPossessable(const FMovieScenePossessable& InPossessable) const override { return false; }
	virtual UObject* MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName) override;
	virtual bool CanAnimateObject(UObject& InObject) const override;

public:
	/** The last animation sequence this control rig sequence was exported to */
	UPROPERTY(AssetRegistrySearchable)
	TSoftObjectPtr<UAnimSequence> LastExportedToAnimationSequence;

	/** The skeletal mesh that was used the last time we exported this sequence */
	UPROPERTY(AssetRegistrySearchable)
	TSoftObjectPtr<USkeletalMesh> LastExportedUsingSkeletalMesh;

	/** The frame rate that was used the last time we exported this sequence */
	UPROPERTY(AssetRegistrySearchable)
	float LastExportedFrameRate;
};
