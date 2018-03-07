// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This Instance only contains one AnimationAsset, and produce poses
 * Used by Preview in AnimGraph, Playing single animation in Kismet2 and etc
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimInstance.h"
#include "AnimSingleNodeInstance.generated.h"

struct FAnimInstanceProxy;

DECLARE_DYNAMIC_DELEGATE(FPostEvaluateAnimEvent);

UCLASS(transient, NotBlueprintable)
class ENGINE_API UAnimSingleNodeInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

	// Disable compiler-generated deprecation warnings by implementing our own destructor
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	~UAnimSingleNodeInstance() {}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Current Asset being played **/
	UPROPERTY(Transient)
	class UAnimationAsset* CurrentAsset;
	 
	UPROPERTY(Transient)
	FPostEvaluateAnimEvent PostEvaluateAnimEvent;

	//~ Begin UAnimInstance Interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativePostEvaluateAnimation() override;
	virtual void OnMontageInstanceStopped(FAnimMontageInstance& StoppedMontageInstance) override;

protected:
	virtual void Montage_Advance(float DeltaTime) override;
	//~ End UAnimInstance Interface
public:

	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetLooping(bool bIsLooping);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetPlayRate(float InPlayRate);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetReverse(bool bInReverse);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetPosition(float InPosition, bool bFireNotifies=true);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetPositionWithPreviousTime(float InPosition, float InPreviousTime, bool bFireNotifies=true);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetBlendSpaceInput(const FVector& InBlendInput);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetPlaying(bool bIsPlaying);
	UFUNCTION(BlueprintCallable, Category="Animation")
	float GetLength();
	/* For AnimSequence specific **/
	UFUNCTION(BlueprintCallable, Category="Animation")
	void PlayAnim(bool bIsLooping=false, float InPlayRate=1.f, float InStartPosition=0.f);
	UFUNCTION(BlueprintCallable, Category="Animation")
	void StopAnim();
	/** Set New Asset - calls InitializeAnimation, for now we need MeshComponent **/
	UFUNCTION(BlueprintCallable, Category="Animation")
	virtual void SetAnimationAsset(UAnimationAsset* NewAsset, bool bIsLooping=true, float InPlayRate=1.f);
	/** Get the currently used asset */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	virtual UAnimationAsset* GetAnimationAsset() const;
	/** Set pose value */
 	UFUNCTION(BlueprintCallable, Category = "Animation")
 	void SetPreviewCurveOverride(const FName& PoseName, float Value, bool bRemoveIfZero);
public:
	/** AnimSequence specific **/
	void StepForward();
	void StepBackward();

	/** custom evaluate pose **/
	virtual void RestartMontage(UAnimMontage * Montage, FName FromSection = FName());
	void SetMontageLoop(UAnimMontage* Montage, bool bIsLooping, FName StartingSection = FName());

	/** Set the montage slot to preview */
	void SetMontagePreviewSlot(FName PreviewSlot);

	/** Updates montage weights based on a jump in time (as this wont be handled by SetPosition) */
	void UpdateMontageWeightForTimeSkip(float TimeDifference);

	/** Updates the blendspace samples list in the case of our asset being a blendspace */
	void UpdateBlendspaceSamples(FVector InBlendInput);

	/** Check whether we are currently playing */
	bool IsPlaying() const;

	/** Check whether we are currently playing in reverse */
	bool IsReverse() const;

	/** Check whether we are currently looping */
	bool IsLooping() const;

	/** Get the current playback time */
	float GetCurrentTime() const;

	/** Get the current play rate multiplier */
	float GetPlayRate() const;

	/** Get the currently playing asset. Can return NULL */
	UAnimationAsset* GetCurrentAsset();

	/** Get the last filter output */
	FVector GetFilterLastOutput();
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};



