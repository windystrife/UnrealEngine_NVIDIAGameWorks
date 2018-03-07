// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimCompress_RemoveEverySecondKey.cpp: Keyframe reduction algorithm that simply removes every second key.
=============================================================================*/ 

#include "Animation/AnimCompress_RemoveEverySecondKey.h"
#include "AnimationCompression.h"
#include "AnimEncoding.h"

UAnimCompress_RemoveEverySecondKey::UAnimCompress_RemoveEverySecondKey(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Description = TEXT("Remove Every Second Key");
	MinKeys = 10;
}

#if WITH_EDITOR
void UAnimCompress_RemoveEverySecondKey::DoReduction(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData)
{
#if WITH_EDITORONLY_DATA
	const int32 StartIndex = bStartAtSecondKey ? 1 : 0;
	const int32 Interval = 2;

	// split the filtered data into tracks
	TArray<FTranslationTrack> TranslationData;
	TArray<FRotationTrack> RotationData;
	TArray<FScaleTrack> ScaleData;
	SeparateRawDataIntoTracks( AnimSeq->GetRawAnimationData(), AnimSeq->SequenceLength, TranslationData, RotationData, ScaleData );

	// Remove Translation Keys from tracks marked bAnimRotationOnly
	FilterAnimRotationOnlyKeys(TranslationData, AnimSeq);

	// remove obviously redundant keys from the source data
	FilterTrivialKeys(TranslationData, RotationData, ScaleData, TRANSLATION_ZEROING_THRESHOLD, QUATERNION_ZEROING_THRESHOLD, SCALE_ZEROING_THRESHOLD);

	// remove intermittent keys from the source data
	FilterIntermittentKeys(TranslationData, RotationData, StartIndex, Interval);

	// bitwise compress the tracks into the anim sequence buffers
	BitwiseCompressAnimationTracks(
		AnimSeq,
		static_cast<AnimationCompressionFormat>(TranslationCompressionFormat),
		static_cast<AnimationCompressionFormat>(RotationCompressionFormat),
		static_cast<AnimationCompressionFormat>(ScaleCompressionFormat),
		TranslationData,
		RotationData, 
		ScaleData);

	// record the proper runtime decompressor to use
	AnimSeq->KeyEncodingFormat = AKF_ConstantKeyLerp;
	AnimationFormat_SetInterfaceLinks(*AnimSeq);
	AnimSeq->CompressionScheme = static_cast<UAnimCompress*>( StaticDuplicateObject( this, AnimSeq ) );
#endif // WITH_EDITORONLY_DATA
}

void UAnimCompress_RemoveEverySecondKey::PopulateDDCKey(FArchive& Ar)
{
	Super::PopulateDDCKey(Ar);
	Ar << MinKeys;
	bool bVal = bStartAtSecondKey;
	Ar << bVal;
}

#endif // WITH_EDITOR
