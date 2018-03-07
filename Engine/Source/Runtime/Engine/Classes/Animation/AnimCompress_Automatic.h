// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Animation compression algorithm that is just a shell for trying the range of other compression schemes and pikcing the
 * smallest result within a configurable error threshold.
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimCompress.h"
#include "AnimCompress_Automatic.generated.h"

UCLASS(MinimalAPI)
class UAnimCompress_Automatic : public UAnimCompress
{
	GENERATED_UCLASS_BODY()

	/** Maximum amount of error that a compression technique can introduce in an end effector 
	* Determines the current setting for world - space error tolerance in the animation compressor.
	* When requested, animation being compressed will also consider an alternative compression
	* method if the end result of that method produces less error than the AlternativeCompressionThreshold.
	* Also known as "Alternative Compression Threshold"
	*/
	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	float MaxEndEffectorError;

	/** If true, the uniform bitwise techniques will be tried */
	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bTryFixedBitwiseCompression:1;

	/** If true, the per-track compressor techniques will be tried */
	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bTryPerTrackBitwiseCompression:1;

	/** If true, the linear key removal techniques will be tried */
	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bTryLinearKeyRemovalCompression:1;

	/** If true, the resampling techniques will be tried */
	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bTryIntervalKeyRemoval:1;

	/** If true, then the animation will be first recompressed with it's current compressor if non-NULL, or with the global default compressor (specified in the engine ini)
	* Also known as "First Recompress Using Current Or Default"
	*/
	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bRunCurrentDefaultCompressor:1;

	/** If true and the existing compression error is greater than Max End Effector Error, then any compression technique (even one that increases the size) with a lower error will be used until it falls below the threshold
	* Also known as "force below threshold"
	*/
	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bAutoReplaceIfExistingErrorTooGreat:1;

	/** If true and the existing compression error is greater than Max End Effector Error, then Max End Effector Error will be effectively raised to the existing error level */
	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bRaiseMaxErrorToExisting:1;


protected:
	//~ Begin UAnimCompress Interface
#if WITH_EDITOR
	virtual void DoReduction(class UAnimSequence* AnimSeq, const TArray<class FBoneData>& BoneData) override;
	virtual void PopulateDDCKey(FArchive& Ar) override;
#endif // WITH_EDITOR
	//~ Begin UAnimCompress Interface
};



