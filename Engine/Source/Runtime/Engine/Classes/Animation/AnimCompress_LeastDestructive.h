// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Reverts any animation compression, restoring the animation to the raw data.
 *
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimCompress.h"
#include "AnimCompress_LeastDestructive.generated.h"

UCLASS()
class UAnimCompress_LeastDestructive : public UAnimCompress
{
	GENERATED_UCLASS_BODY()


protected:
	//~ Begin UAnimCompress Interface
#if WITH_EDITOR
	virtual void DoReduction(class UAnimSequence* AnimSeq, const TArray<class FBoneData>& BoneData) override;
#endif // WITH_EDITOR
	//~ Begin UAnimCompress Interface
};



