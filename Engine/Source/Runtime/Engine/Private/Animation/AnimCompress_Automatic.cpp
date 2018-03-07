// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Animation/AnimCompress_Automatic.h"
#include "Animation/AnimationSettings.h"

UAnimCompress_Automatic::UAnimCompress_Automatic(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Description = TEXT("Automatic");
	UAnimationSettings* AnimationSettings = UAnimationSettings::Get();
	MaxEndEffectorError = AnimationSettings->AlternativeCompressionThreshold;
	bTryFixedBitwiseCompression = AnimationSettings->bTryFixedBitwiseCompression;
	bTryPerTrackBitwiseCompression = AnimationSettings->bTryPerTrackBitwiseCompression;
	bTryLinearKeyRemovalCompression = AnimationSettings->bTryLinearKeyRemovalCompression;
	bTryIntervalKeyRemoval = AnimationSettings->bTryIntervalKeyRemoval;
	bRunCurrentDefaultCompressor = AnimationSettings->bFirstRecompressUsingCurrentOrDefault;
	bAutoReplaceIfExistingErrorTooGreat = AnimationSettings->bForceBelowThreshold;
	bRaiseMaxErrorToExisting = AnimationSettings->bRaiseMaxErrorToExisting;
}

#if WITH_EDITOR
void UAnimCompress_Automatic::DoReduction(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData)
{
	FAnimCompressContext CompressContext(MaxEndEffectorError > 0.0f, false);
#if WITH_EDITORONLY_DATA
	FAnimationUtils::CompressAnimSequenceExplicit(
		AnimSeq,
		CompressContext,
		MaxEndEffectorError,
		bRunCurrentDefaultCompressor,
		bAutoReplaceIfExistingErrorTooGreat,
		bRaiseMaxErrorToExisting,
		bTryFixedBitwiseCompression,
		bTryPerTrackBitwiseCompression,
		bTryLinearKeyRemovalCompression,
		bTryIntervalKeyRemoval);
	AnimSeq->CompressionScheme = static_cast<UAnimCompress*>( StaticDuplicateObject( AnimSeq->CompressionScheme, AnimSeq) );
#endif // WITH_EDITORONLY_DATA
}

void UAnimCompress_Automatic::PopulateDDCKey(FArchive& Ar)
{
	Super::PopulateDDCKey(Ar);

	Ar << MaxEndEffectorError;

	uint8 Flags =	MakeBitForFlag(bTryFixedBitwiseCompression, 0) +
					MakeBitForFlag(bTryPerTrackBitwiseCompression, 1) +
					MakeBitForFlag(bTryLinearKeyRemovalCompression, 2) +
					MakeBitForFlag(bTryIntervalKeyRemoval, 3) +
					MakeBitForFlag(bRunCurrentDefaultCompressor, 4) +
					MakeBitForFlag(bAutoReplaceIfExistingErrorTooGreat, 5) +
					MakeBitForFlag(bRaiseMaxErrorToExisting, 6);
	Ar << Flags;
}

#endif // WITH_EDITOR
