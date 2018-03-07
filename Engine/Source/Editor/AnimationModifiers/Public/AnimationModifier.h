// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimEnums.h"
#include "Animation/AnimCurveTypes.h"
#include "AnimationBlueprintLibrary.h"

#include "AnimationModifier.generated.h"

class USkeleton;

UCLASS(Blueprintable, Experimental, config = Editor, defaultconfig)
class ANIMATIONMODIFIERS_API UAnimationModifier : public UObject
{
	GENERATED_BODY()

	friend class FAnimationModifierDetailCustomization;
public:
	/** Applying and reverting the modifier for the given Animation Sequence */
	void ApplyToAnimationSequence(class UAnimSequence* InAnimationSequence);
	void RevertFromAnimationSequence(class UAnimSequence* InAnimationSequence);

	/** Executed when the Animation is initialized (native event for debugging / testing purposes) */
	UFUNCTION(BlueprintNativeEvent)
	void OnApply(UAnimSequence* AnimationSequence);
	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) {}
	
	UFUNCTION(BlueprintNativeEvent)
	void OnRevert(UAnimSequence* AnimationSequence);
	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) {}

	/** Whether or not the latest compiled version of the blueprint is applied for this instance */
	bool IsLatestRevisionApplied() const;

	// Begin UObject Overrides
	virtual void PostInitProperties() override;
	// End UObject Overrides
protected:
	// Derived class accessors to skeleton and anim sequence 
	const UAnimSequence* GetAnimationSequence();
	const USkeleton* GetSkeleton();

	/** Used for natively updating the revision GUID, fairly basic and relies on config files currently */
	virtual int32 GetNativeClassRevision() const;

	/** Checks if the animation data has to be re-baked / compressed and does so */
	void UpdateCompressedAnimationData();

	/** Updating of blueprint and native GUIDs*/
	void UpdateRevisionGuid(UClass* ModifierClass);
	void UpdateNativeRevisionGuid();
private:
	UAnimSequence* CurrentAnimSequence;
	USkeleton* CurrentSkeleton;

	void UpdateStoredRevisions();
	void ResetStoredRevisions();
	void SetInstanceRevisionGuid(FGuid Guid);

	// This holds the GUID representing the latest version of the modifier
	UPROPERTY(/*VisibleAnywhere for testing, Category = Revision*/)
	FGuid RevisionGuid;

	// This indicates whether or not the modifier is newer than what has been applied
	UPROPERTY(/*VisibleAnywhere for testing, Category = Revision */)
	FGuid AppliedGuid;

	// This holds the latest value returned by UpdateNativeRevisionGuid during the last PostLoad (changing this value will invalidate the GUIDs for all instances)
	UPROPERTY(config)
	int32 StoredNativeRevision;
};