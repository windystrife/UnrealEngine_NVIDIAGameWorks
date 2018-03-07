// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackVectorBase.h"
#include "InterpTrackSound.generated.h"

class FCanvas;
class UInterpGroup;
class UInterpTrackInst;

/**
 *	A track that plays sounds on the groups Actor.
 */


/** Information for one sound in the track. */
USTRUCT()
struct FSoundTrackKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY()
	float Volume;

	UPROPERTY()
	float Pitch;

	UPROPERTY(EditAnywhere, Category=SoundTrackKey)
	class USoundBase* Sound;



		FSoundTrackKey()
		: Time(0)
		, Volume(1.f)
		, Pitch(1.f)
		, Sound(NULL)
		{
		}
	
};

UCLASS(MinimalAPI, meta=( DisplayName = "Sound Track" ) )
class UInterpTrackSound : public UInterpTrackVectorBase
{
	GENERATED_UCLASS_BODY()

	/** Array of sounds to play at specific times. */
	UPROPERTY()
	TArray<struct FSoundTrackKey> Sounds;

	/** if set, sound plays only when playing the matinee in reverse instead of when the matinee plays forward */
	UPROPERTY(EditAnywhere, Category=InterpTrackSound)
	uint32 bPlayOnReverse:1;

	/** If true, sounds on this track will not be forced to finish when the matinee sequence finishes. */
	UPROPERTY(EditAnywhere, Category=InterpTrackSound)
	uint32 bContinueSoundOnMatineeEnd:1;

	/** If true, don't show subtitles for sounds played by this track. */
	UPROPERTY(EditAnywhere, Category=InterpTrackSound)
	uint32 bSuppressSubtitles:1;

	/** If true and track is controlling a pawn, makes the pawn "speak" the given audio. */
	UPROPERTY(EditAnywhere, Category=InterpTrackSound)
	uint32 bTreatAsDialogue:1;

	UPROPERTY(EditAnywhere, Category=InterpTrackSound)
	uint32 bAttach:1;

	// True if the sound should have been playing at any point
	uint32 bPlaying:1;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin UInterpTrack Interface
	virtual int32 GetNumKeyframes() const override;
	virtual void GetTimeRange(float& StartTime, float& EndTime) const override;
	virtual float GetTrackEndTime() const override;
	virtual float GetKeyframeTime(int32 KeyIndex) const override;
	virtual int32 GetKeyframeIndex( float KeyTime ) const override;
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual int32 SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder=true) override;
	virtual void RemoveKeyframe(int32 KeyIndex) override;
	virtual int32 DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL) override;
	virtual bool GetClosestSnapPosition(float InPosition, TArray<int32> &IgnoreKeys, float& OutPosition) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	virtual void PreviewStopPlayback(class UInterpTrackInst* TrInst) override;
	virtual const FString	GetEdHelperClassName() const override;
	virtual const FString	GetSlateHelperClassName() const override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params ) override;
	virtual bool AllowStaticActors() override { return true; }
	virtual void SetTrackToSensibleDefault() override;
	//~ End UInterpTrack Interface

	/**
	 * Return the key at the specified position in the track.
	 */
	struct FSoundTrackKey& GetSoundTrackKeyAtPosition(float InPosition);

};



