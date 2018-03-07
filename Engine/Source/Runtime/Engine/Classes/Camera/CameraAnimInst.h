// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *	A CameraAnimInst is an active instance of a CameraAnim.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Camera/CameraTypes.h"

#include "CameraAnimInst.generated.h"

UCLASS(notplaceable, BlueprintType, transient)
class ENGINE_API UCameraAnimInst : public UObject
{
	GENERATED_UCLASS_BODY()

	/** which CameraAnim this is an instance of */
	UPROPERTY()
	class UCameraAnim* CamAnim;

private:
	/** the UInterpGroupInst used to do the interpolation */
	UPROPERTY(instanced)
	class UInterpGroupInst* InterpGroupInst;

public:
	/** Current time for the animation */
	float CurTime;

	/** True if the animation has finished, false otherwise. */
	uint32 bFinished : 1;

protected:

	/** If true, this anim inst will automatically stop itself when it finishes, otherwise, it will wait for an explicit Stop() call. */
	uint32 bStopAutomatically : 1;

	/** True if the animation should loop, false otherwise. */
	uint32 bLooping:1;

	/** True if currently blending in. */
	uint32 bBlendingIn : 1;

	/** True if currently blending out. */
	uint32 bBlendingOut : 1;

	/** True if this camera anim has a track modifying the FOV */
	uint32 bHasFOVTrack : 1;

	/** Time to interpolate in from zero, for smooth starts. */
	float BlendInTime;

	/** Time to interpolate out to zero, for smooth finishes. */
	float BlendOutTime;

	/** Current time for the blend-in.  I.e. how long we have been blending. */
	float CurBlendInTime;

	/** Current time for the blend-out.  I.e. how long we have been blending. */
	float CurBlendOutTime;

public:
	/** Multiplier for playback rate.  1.0 = normal. */
	UPROPERTY(BlueprintReadWrite, Category=CameraAnimInst)
	float PlayRate;

	/** "Intensity" value used to scale keyframe values. */
	float BasePlayScale;

	/** A supplemental scale factor, allowing external systems to scale this anim as necessary.  This is reset to 1.f each frame. */
	float TransientScaleModifier;

	/* Number in range [0..1], controlling how much this influence this instance should have. */
	float CurrentBlendWeight;

protected:
	/** How much longer to play the anim, if a specific duration is desired.  Has no effect if 0.  */
	float RemainingTime;

public:
	/** cached movement track from the currently playing anim so we don't have to go find it every frame */
	UPROPERTY(transient)
	class UInterpTrackMove* MoveTrack;

	UPROPERTY(transient)
	class UInterpTrackInstMove* MoveInst;

	UPROPERTY()
	TEnumAsByte<ECameraAnimPlaySpace::Type> PlaySpace;

	/** The user-defined space for UserDefined PlaySpace */
	FMatrix UserPlaySpaceMatrix;

	/** Camera Anim debug variable to trace back to previous location **/
	FVector LastCameraLoc;

	/** transform of initial anim key, used for treating anim keys as offsets from initial key */
	FTransform InitialCamToWorld;

	/** FOV of the initial anim key, used for treating fov keys as offsets from initial key. */
	float InitialFOV;

	/**
	 * Starts this instance playing the specified CameraAnim.
	 *
	 * @param CamAnim			The animation that should play on this instance.
	 * @param CamActor			The  AActor  that will be modified by this animation.
	 * @param InRate			How fast to play the animation.  1.f is normal.
	 * @param InScale			How intense to play the animation.  1.f is normal.
	 * @param InBlendInTime		Time over which to linearly ramp in.
	 * @param InBlendOutTime	Time over which to linearly ramp out.
	 * @param bInLoop			Whether or not to loop the animation.
	 * @param bRandomStartTime	Whether or not to choose a random time to start playing.  Only really makes sense for bLoop = true;
	 * @param Duration			optional specific playtime for this animation.  This is total time, including blends.
	 */
	void Play(class UCameraAnim* Anim, class AActor* CamActor, float InRate, float InScale, float InBlendInTime, float InBlendOutTime, bool bInLoop, bool bRandomStartTime, float Duration = 0.f);
	
	/** Updates this active instance with new parameters. */
	void Update(float NewRate, float NewScale, float NewBlendInTime, float NewBlendOutTime, float NewDuration = 0.f);
	
	/** advances the animation by the specified time - updates any modified interp properties, moves the group actor, etc */
	void AdvanceAnim(float DeltaTime, bool bJump);
	
	/** Jumps he camera anim to the given (unscaled) time. */
	void SetCurrentTime(float NewTime);

	/** Returns the current playback time. */
	float GetCurrentTime() const { return CurTime; };

	/** Stops this instance playing whatever animation it is playing. */
	UFUNCTION(BlueprintCallable, Category = CameraAnimInst)
	void Stop(bool bImmediate = false);
	
	/** Applies given scaling factor to the playing animation for the next update only. */
	void ApplyTransientScaling(float Scalar);
	
	/** Sets this anim to play in an alternate playspace */
	void SetPlaySpace(ECameraAnimPlaySpace::Type NewSpace, FRotator UserPlaySpace = FRotator::ZeroRotator);

	/** Changes the running duration of this active anim, while maintaining playback position. */
	UFUNCTION(BlueprintCallable, Category=CameraAnimInst)
	void SetDuration(float NewDuration);

	/** Changes the scale of the animation while playing.*/
	UFUNCTION(BlueprintCallable, Category = CameraAnimInst)
	void SetScale(float NewDuration);

	/** Takes the given view and applies the camera anim transform and fov changes to it. Does not affect PostProcess. */
	void ApplyToView(FMinimalViewInfo& InOutPOV) const;

	/** Sets whether this anim instance should automatically stop when finished. */
	void SetStopAutomatically(bool bNewStopAutoMatically) { bStopAutomatically = bNewStopAutoMatically; };

protected:
	/** Returns InterpGroupInst subobject **/
	class UInterpGroupInst* GetInterpGroupInst() const { return InterpGroupInst; }
};



