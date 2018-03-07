// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "Blueprint/UserWidget.h"
#include "LevelSequenceBurnIn.generated.h"

/**
 * Base class for level sequence burn ins
 */
UCLASS()
class LEVELSEQUENCE_API ULevelSequenceBurnIn : public UUserWidget
{
public:
	GENERATED_BODY()

	ULevelSequenceBurnIn(const FObjectInitializer& ObjectInitializer);

	/** Initialize this burn in */
	void TakeSnapshotsFrom(ALevelSequenceActor& InActor);

	/** Called when this burn in is receiving its settings */
	UFUNCTION(BlueprintImplementableEvent, Category="Burn In")
	void SetSettings(UObject* InSettings);

	/** Get the settings class to use for this burn in */
	UFUNCTION(BlueprintNativeEvent, Category="Burn In")
	TSubclassOf<ULevelSequenceBurnInInitSettings> GetSettingsClass() const;
	TSubclassOf<ULevelSequenceBurnInInitSettings> GetSettingsClass_Implementation() const { return nullptr; }

protected:

	/** Called as part of the game tick loop when the sequence has been updated */
	void OnSequenceUpdated(const UMovieSceneSequencePlayer& Player, float CurrentTime, float PreviousTime);

protected:

	/** Snapshot of frame information. */
	UPROPERTY(BlueprintReadOnly, Category="Burn In")
	FLevelSequencePlayerSnapshot FrameInformation;

	/** The actor to get our burn in frames from */
	UPROPERTY(BlueprintReadOnly, Category="Burn In")
	ALevelSequenceActor* LevelSequenceActor;
};
