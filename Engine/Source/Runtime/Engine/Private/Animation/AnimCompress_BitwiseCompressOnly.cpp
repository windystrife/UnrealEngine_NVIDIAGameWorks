// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimCompress_BitwiseCompressionOnly.cpp: Bitwise animation compression only; performs no key reduction.
=============================================================================*/ 

#include "Animation/AnimCompress_BitwiseCompressOnly.h"
#include "AnimationCompression.h"
#include "AnimEncoding.h"

UAnimCompress_BitwiseCompressOnly::UAnimCompress_BitwiseCompressOnly(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Description = TEXT("Bitwise Compress Only");
}

#if WITH_EDITOR
void UAnimCompress_BitwiseCompressOnly::DoReduction(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData)
{
#if WITH_EDITORONLY_DATA
	// split the raw data into tracks
	TArray<FTranslationTrack> TranslationData;
	TArray<FRotationTrack> RotationData;
	TArray<FScaleTrack> ScaleData;
	SeparateRawDataIntoTracks( AnimSeq->GetRawAnimationData(), AnimSeq->SequenceLength, TranslationData, RotationData, ScaleData );

	// Remove Translation Keys from tracks marked bAnimRotationOnly
	FilterAnimRotationOnlyKeys(TranslationData, AnimSeq);

	// remove obviously redundant keys from the source data
	FilterTrivialKeys(TranslationData, RotationData, ScaleData, TRANSLATION_ZEROING_THRESHOLD, QUATERNION_ZEROING_THRESHOLD, SCALE_ZEROING_THRESHOLD);

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

#endif // WITH_EDITOR
