// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Camera/CameraTypes.h"
#include "CameraShake.generated.h"

class AActor;

/************************************************************
 * Parameters for defining oscillating camera shakes
 ************************************************************/

/** Shake start offset parameter */
UENUM()
enum EInitialOscillatorOffset
{
	/** Start with random offset (default). */
	EOO_OffsetRandom,
	/** Start with zero offset. */
	EOO_OffsetZero,
	EOO_MAX,
};

/** Defines oscillation of a single number. */
USTRUCT()
struct ENGINE_API FFOscillator
{
	GENERATED_USTRUCT_BODY()

	/** Amplitude of the sinusoidal oscillation. */
	UPROPERTY(EditAnywhere, Category=FOscillator)
	float Amplitude;

	/** Frequency of the sinusoidal oscillation. */
	UPROPERTY(EditAnywhere, Category = FOscillator)
	float Frequency;

	/** Defines how to begin (either at zero, or at a randomized value. */
	UPROPERTY(EditAnywhere, Category=FOscillator)
	TEnumAsByte<enum EInitialOscillatorOffset> InitialOffset;
	
	FFOscillator()
		: Amplitude(0)
		, Frequency(0)
		, InitialOffset(0)
	{}

	/** Advances the oscillation time and returns the current value. */
	static float UpdateOffset(FFOscillator const& Osc, float& CurrentOffset, float DeltaTime);

	/** Returns the initial value of the oscillator. */
	static float GetInitialOffset(FFOscillator const& Osc);

	/** Returns the offset at the given time */
	static float GetOffsetAtTime(FFOscillator const& Osc, float InitialOffset, float Time);
};

/** Defines FRotator oscillation. */
USTRUCT()
struct FROscillator
{
	GENERATED_USTRUCT_BODY()

	/** Pitch oscillation. */
	UPROPERTY(EditAnywhere, Category=ROscillator)
	struct FFOscillator Pitch;

	/** Yaw oscillation. */
	UPROPERTY(EditAnywhere, Category = ROscillator)
	struct FFOscillator Yaw;

	/** Roll oscillation. */
	UPROPERTY(EditAnywhere, Category = ROscillator)
	struct FFOscillator Roll;

};

/** Defines FVector oscillation. */
USTRUCT()
struct FVOscillator
{
	GENERATED_USTRUCT_BODY()

	/** Oscillation in the X axis. */
	UPROPERTY(EditAnywhere, Category = VOscillator)
	struct FFOscillator X;

	/** Oscillation in the Y axis. */
	UPROPERTY(EditAnywhere, Category = VOscillator)
	struct FFOscillator Y;

	/** Oscillation in the Z axis. */
	UPROPERTY(EditAnywhere, Category = VOscillator)
	struct FFOscillator Z;

};


/**
 * A CameraShake is an asset that defines how to shake the camera in 
 * a particular way. CameraShakes can be authored as either oscillating shakes, 
 * animated shakes, or both.
 *
 * An oscillating shake will sinusoidally vibrate various camera parameters over time. Each location
 * and rotation axis can be oscillated independently with different parameters to create complex and
 * random-feeling shakes. These are easier to author and tweak, but can still feel mechanical and are
 * limited to vibration-style shakes, such as earthquakes.
 *
 * Animated shakes play keyframed camera animations.  These can take more effort to author, but enable
 * more natural-feeling results and things like directional shakes.  For instance, you can have an explosion
 * to the camera's right push it primarily to the left.
 */

UCLASS(Blueprintable, editinlinenew)
class ENGINE_API UCameraShake : public UObject
{
	GENERATED_UCLASS_BODY()

	/** 
	 *  If true to only allow a single instance of this shake class to play at any given time.
	 *  Subsequent attempts to play this shake will simply restart the timer.
	 */
	UPROPERTY(EditAnywhere, Category=CameraShake)
	uint32 bSingleInstance:1;

	/** Duration in seconds of current screen shake. Less than 0 means indefinite, 0 means no oscillation. */
	UPROPERTY(EditAnywhere, Category=Oscillation)
	float OscillationDuration;

	/** Duration of the blend-in, where the oscillation scales from 0 to 1. */
	UPROPERTY(EditAnywhere, Category=Oscillation, meta=(ClampMin = "0.0"))
	float OscillationBlendInTime;

	/** Duration of the blend-out, where the oscillation scales from 1 to 0. */
	UPROPERTY(EditAnywhere, Category=Oscillation, meta = (ClampMin = "0.0"))
	float OscillationBlendOutTime;

	/** Rotational oscillation */
	UPROPERTY(EditAnywhere, Category=Oscillation)
	struct FROscillator RotOscillation;

	/** Positional oscillation */
	UPROPERTY(EditAnywhere, Category=Oscillation)
	struct FVOscillator LocOscillation;

	/** FOV oscillation */
	UPROPERTY(EditAnywhere, Category=Oscillation)
	struct FFOscillator FOVOscillation;

	/************************************************************
	 * Parameters for defining CameraAnim-driven camera shakes
	 ************************************************************/

	/** Scalar defining how fast to play the anim. */
	UPROPERTY(EditAnywhere, Category=AnimShake, meta=(ClampMin = "0.001"))
	float AnimPlayRate;

	/** Scalar defining how "intense" to play the anim. */
	UPROPERTY(EditAnywhere, Category=AnimShake, meta=(ClampMin = "0.0"))
	float AnimScale;

	/** Linear blend-in time. */
	UPROPERTY(EditAnywhere, Category=AnimShake, meta=(ClampMin = "0.0"))
	float AnimBlendInTime;

	/** Linear blend-out time. */
	UPROPERTY(EditAnywhere, Category=AnimShake, meta=(ClampMin = "0.0"))
	float AnimBlendOutTime;

	/** When bRandomAnimSegment is true, this defines how long the anim should play. */
	UPROPERTY(EditAnywhere, Category = AnimShake, meta = (ClampMin = "0.0", editcondition = "bRandomAnimSegment"))
	float RandomAnimSegmentDuration;

	/** Source camera animation to play. Can be null. */
	UPROPERTY(EditAnywhere, Category = AnimShake)
	class UCameraAnim* Anim;

	/**
	* If true, play a random snippet of the animation of length Duration.  Implies bLoop and bRandomStartTime = true for the CameraAnim.
	* If false, play the full anim once, non-looped. Useful for getting variety out of a single looped CameraAnim asset.
	*/
	UPROPERTY(EditAnywhere, Category = AnimShake)
	uint32 bRandomAnimSegment : 1;

protected:

	// INSTANCE DATA
	
	/** True if this shake is currently blending in. */
	uint16 bBlendingIn:1;

	/** True if this shake is currently blending out. */
	uint16 bBlendingOut:1;

	/** What space to play the shake in before applying to the camera.  Affects both Anim and Oscillation shakes. */
	ECameraAnimPlaySpace::Type PlaySpace;

	/** How long this instance has been blending in. */
	float CurrentBlendInTime;

	/** How long this instance has been blending out. */
	float CurrentBlendOutTime;

	UPROPERTY(transient, BlueprintReadOnly, Category = CameraShake)
	class APlayerCameraManager* CameraOwner;

	/** Current location sinusoidal offset. */
	FVector LocSinOffset;

	/** Current rotational sinusoidal offset. */
	FVector RotSinOffset;

	/** Current FOV sinusoidal offset. */
	float FOVSinOffset;

	/** Initial offset (could have been assigned at random). */
	FVector InitialLocSinOffset;

	/** Initial offset (could have been assigned at random). */
	FVector InitialRotSinOffset;

	/** Initial offset (could have been assigned at random). */
	float InitialFOVSinOffset;

	/** Matrix defining the playspace, used when PlaySpace == CAPS_UserDefined */
	FMatrix UserPlaySpaceMatrix;

	/** Temp actor to use for playing camera anims. Used when playing a camera anim in non-gameplay context, e.g. in the editor */
	AActor* TempCameraActorForCameraAnims;

public:
	/** Overall intensity scale for this shake instance. */
	UPROPERTY(transient, BlueprintReadOnly, Category = CameraShake)
	float ShakeScale;

	/** Time remaining for oscillation shakes. Less than 0.f means shake infinitely. */
	UPROPERTY(transient, BlueprintReadOnly, Category = CameraShake)
	float OscillatorTimeRemaining;

	/** The playing instance of the CameraAnim-based shake, if any. */
	UPROPERTY(transient, BlueprintReadOnly, Category = CameraShake)
	class UCameraAnimInst* AnimInst;

public:
	// Blueprint API

	/** Called every tick to let the shake modify the point of view */
	UFUNCTION(BlueprintImplementableEvent, Category=CameraShake)
	void BlueprintUpdateCameraShake(float DeltaTime, float Alpha, const FMinimalViewInfo& POV, FMinimalViewInfo& ModifiedPOV);

	/** Called when the shake starts playing */
	UFUNCTION(BlueprintImplementableEvent, Category = CameraShake)
	void ReceivePlayShake(float Scale);

	/** Called to allow a shake to decide when it's finished playing. */
	UFUNCTION(BlueprintNativeEvent, Category = CameraShake)
	bool ReceiveIsFinished() const;

	/** 
	 * Called when the shake is explicitly stopped. 
	 * @param bImmediatly		If true, shake stops right away regardless of blend out settings. If false, shake may blend out according to its settings.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = CameraShake)
	void ReceiveStopShake(bool bImmediately);

	// Native API
	virtual void UpdateAndApplyCameraShake(float DeltaTime, float Alpha, FMinimalViewInfo& InOutPOV);
	virtual void PlayShake(class APlayerCameraManager* Camera, float Scale, ECameraAnimPlaySpace::Type InPlaySpace, FRotator UserPlaySpaceRot = FRotator::ZeroRotator);
	virtual bool IsFinished() const;

	/** 
	 * Stops this shake from playing. 
	 * @param bImmediatly		If true, shake stops right away regardless of blend out settings. If false, shake may blend out according to its settings.
	 */
	virtual void StopShake(bool bImmediately = true);

	// Returns true if this camera shake will loop forever
	bool IsLooping() const;

	/** Sets current playback time and applies the shake (both oscillation and cameraanim) to the given POV. */
	void SetCurrentTimeAndApplyShake(float NewTime, FMinimalViewInfo& POV);

	void SetTempCameraAnimActor(AActor* Actor) { TempCameraActorForCameraAnims = Actor; }
};



