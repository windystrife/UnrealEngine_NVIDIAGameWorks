// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetUserData.h"

#include "AnimationModifiersAssetUserData.generated.h"

class UAnimationModifier;

/** Asset user data which can be added to a USkeleton or UAnimSequence to keep track of Animation Modifiers */
UCLASS()
class UAnimationModifiersAssetUserData : public UAssetUserData
{
	GENERATED_BODY()
public:
	void AddAnimationModifier(UAnimationModifier* Instance);
	void RemoveAnimationModifierInstance(UAnimationModifier* Instance);
	const TArray<UAnimationModifier*>& GetAnimationModifierInstances() const;
	void ChangeAnimationModifierIndex(UAnimationModifier* Instance, int32 Direction);

	/** Begin UAssetUserData overrides */
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;	

	virtual void Serialize(FArchive& Ar) override;

	/** End UAssetUserData overrides */
protected:
	void RemoveInvalidModifiers();
protected:
	UPROPERTY()
	TArray<UAnimationModifier*> AnimationModifierInstances;
};
