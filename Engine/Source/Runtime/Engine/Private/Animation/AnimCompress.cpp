// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimCompress.cpp: Skeletal mesh animation compression.
=============================================================================*/ 

#include "Animation/AnimCompress.h"
#include "Misc/MessageDialog.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/FeedbackContext.h"
#include "AnimationCompression.h"
#include "AnimEncoding.h"
#include "Animation/AnimationSettings.h"

DEFINE_LOG_CATEGORY(LogAnimationCompression);


// Writes the specified data to Seq->CompresedByteStream with four-byte alignment.
#define AC_UnalignedWriteToStream( Src, Len )										\
	{																				\
		const int32 Ofs = Seq->CompressedByteStream.AddUninitialized( Len );						\
		FMemory::Memcpy( Seq->CompressedByteStream.GetData()+Ofs, (Src), (Len) );	\
	}

static const uint8 AnimationPadSentinel = 85; //(1<<1)+(1<<3)+(1<<5)+(1<<7)


static void PackVectorToStream(
	UAnimSequence* Seq, 
	AnimationCompressionFormat TargetTranslationFormat, 
	const FVector& Vec,
	const float* Mins,
	const float* Ranges)
{
	if ( TargetTranslationFormat == ACF_None )
	{
		AC_UnalignedWriteToStream( &Vec, sizeof(Vec) );
	}
	else if ( TargetTranslationFormat == ACF_Float96NoW )
	{
		AC_UnalignedWriteToStream( &Vec, sizeof(Vec) );
	}
	else if ( TargetTranslationFormat == ACF_IntervalFixed32NoW )
	{
		const FVectorIntervalFixed32NoW CompressedVec( Vec, Mins, Ranges );

		AC_UnalignedWriteToStream( &CompressedVec, sizeof(CompressedVec) );
	}
}

static void PackQuaternionToStream(
	UAnimSequence* Seq, 
	AnimationCompressionFormat TargetRotationFormat, 
	const FQuat& Quat,
	const float* Mins,
	const float* Ranges)
{
	if ( TargetRotationFormat == ACF_None )
	{
		AC_UnalignedWriteToStream( &Quat, sizeof(FQuat) );
	}
	else if ( TargetRotationFormat == ACF_Float96NoW )
	{
		const FQuatFloat96NoW QuatFloat96NoW( Quat );
		AC_UnalignedWriteToStream( &QuatFloat96NoW, sizeof(FQuatFloat96NoW) );
	}
	else if ( TargetRotationFormat == ACF_Fixed32NoW )
	{
		const FQuatFixed32NoW QuatFixed32NoW( Quat );
		AC_UnalignedWriteToStream( &QuatFixed32NoW, sizeof(FQuatFixed32NoW) );
	}
	else if ( TargetRotationFormat == ACF_Fixed48NoW )
	{
		const FQuatFixed48NoW QuatFixed48NoW( Quat );
		AC_UnalignedWriteToStream( &QuatFixed48NoW, sizeof(FQuatFixed48NoW) );
	}
	else if ( TargetRotationFormat == ACF_IntervalFixed32NoW )
	{
		const FQuatIntervalFixed32NoW QuatIntervalFixed32NoW( Quat, Mins, Ranges );
		AC_UnalignedWriteToStream( &QuatIntervalFixed32NoW, sizeof(FQuatIntervalFixed32NoW) );
	}
	else if ( TargetRotationFormat == ACF_Float32NoW )
	{
		const FQuatFloat32NoW QuatFloat32NoW( Quat );
		AC_UnalignedWriteToStream( &QuatFloat32NoW, sizeof(FQuatFloat32NoW) );
	}
}

uint8 MakeBitForFlag(uint32 Item, uint32 Position)
{
	checkSlow(Item < 2);
	return Item << Position;
}

//////////////////////////////////////////////////////////////////////////////////////
// FCompressionMemorySummary

FCompressionMemorySummary::FCompressionMemorySummary(bool bInEnabled)
	: bEnabled(bInEnabled)
	, bUsed(false)
	, TotalRaw(0)
	, TotalBeforeCompressed(0)
	, TotalAfterCompressed(0)
	, ErrorTotal(0)
	, ErrorCount(0)
	, AverageError(0)
	, MaxError(0)
	, MaxErrorTime(0)
	, MaxErrorBone(0)
	, MaxErrorBoneName(NAME_None)
	, MaxErrorAnimName(NAME_None)
{
	if (bEnabled)
	{
		GWarn->BeginSlowTask(NSLOCTEXT("CompressionMemorySummary", "BeginCompressingTaskMessage", "Compressing animations"), true);
	}
}

FCompressionMemorySummary::~FCompressionMemorySummary()
{
	if (bEnabled)
	{
		GWarn->EndSlowTask();

		if (bUsed)
		{
			const int32 TotalBeforeSaving = TotalRaw - TotalBeforeCompressed;
			const int32 TotalAfterSaving = TotalRaw - TotalAfterCompressed;
			const float OldCompressionRatio = (TotalBeforeCompressed > 0.f) ? (static_cast<float>(TotalRaw) / TotalBeforeCompressed) : 0.f;
			const float NewCompressionRatio = (TotalAfterCompressed > 0.f) ? (static_cast<float>(TotalRaw) / TotalAfterCompressed) : 0.f;

			FNumberFormattingOptions Options;
			Options.MinimumIntegralDigits = 7;
			Options.MinimumFractionalDigits = 2;

			FFormatNamedArguments Args;
			Args.Add(TEXT("TotalRaw"), FText::AsMemory(TotalRaw, &Options));
			Args.Add(TEXT("TotalBeforeCompressed"), FText::AsMemory(TotalBeforeCompressed, &Options));
			Args.Add(TEXT("TotalBeforeSaving"), FText::AsMemory(TotalBeforeSaving, &Options));
			Args.Add(TEXT("OldCompressionRatio"), OldCompressionRatio);

			Args.Add(TEXT("TotalAfterCompressed"), FText::AsMemory(TotalAfterCompressed, &Options));
			Args.Add(TEXT("TotalAfterSaving"), FText::AsMemory(TotalAfterSaving, &Options));
			Args.Add(TEXT("NewCompressionRatio"), NewCompressionRatio);

			Args.Add(TEXT("AverageError"), FText::AsNumber(AverageError, &Options));
			Args.Add(TEXT("MaxError"), FText::AsNumber(MaxError, &Options));

			Args.Add(TEXT("MaxErrorAnimName"), FText::FromName(MaxErrorAnimName));
			Args.Add(TEXT("MaxErrorBoneName"), FText::FromName(MaxErrorBoneName));
			Args.Add(TEXT("MaxErrorBone"), MaxErrorBone);
			Args.Add(TEXT("MaxErrorTime"), FText::AsNumber(MaxErrorTime, &Options));

			const FText Message = FText::Format(NSLOCTEXT("Engine", "CompressionMemorySummary", "Raw: {TotalRaw} - Compressed: {TotalBeforeCompressed}\nSaving: {TotalBeforeSaving} ({OldCompressionRatio})\nRaw: {TotalRaw} - Compressed: {TotalAfterCompressed}\nSaving: {TotalAfterSaving} ({NewCompressionRatio})\n\nEnd Effector Translation Added By Compression:\n{AverageError} avg, {MaxError} max\nMax occurred in {MaxErrorAnimName}, Bone {MaxErrorBoneName}(#{MaxErrorBone}), at Time {MaxErrorTime}\n"), Args);

			FMessageDialog::Open(EAppMsgType::Ok, Message);
		}
	}
}

void FCompressionMemorySummary::GatherPreCompressionStats(UAnimSequence* Seq, int32 ProgressNumerator, int32 ProgressDenominator)
{
	if (bEnabled)
	{
		bUsed = true;
		FFormatNamedArguments Args;
		Args.Add(TEXT("AnimSequenceName"), FText::FromString(Seq->GetName()));
		Args.Add(TEXT("ProgressNumerator"), ProgressNumerator);
		Args.Add(TEXT("ProgressDenominator"), ProgressDenominator);

		GWarn->StatusUpdate(ProgressNumerator,
			ProgressDenominator,
			FText::Format(NSLOCTEXT("CompressionMemorySummary", "CompressingTaskStatusMessageFormat", "Compressing {AnimSequenceName} ({ProgressNumerator}/{ProgressDenominator})"), Args));

		TotalRaw += Seq->GetApproxRawSize();
		TotalBeforeCompressed += Seq->GetApproxCompressedSize();
	}
}

void FCompressionMemorySummary::GatherPostCompressionStats(UAnimSequence* Seq, TArray<FBoneData>& BoneData)
{
	if (bEnabled)
	{
		TotalAfterCompressed += Seq->GetApproxCompressedSize();

		if (Seq->GetSkeleton() != NULL)
		{
			// determine the error added by the compression
			AnimationErrorStats ErrorStats;
			FAnimationUtils::ComputeCompressionError(Seq, BoneData, ErrorStats);

			ErrorTotal += ErrorStats.AverageError;
			ErrorCount += 1.0f;
			AverageError = ErrorTotal / ErrorCount;

			if (ErrorStats.MaxError > MaxError)
			{
				MaxError = ErrorStats.MaxError;
				MaxErrorTime = ErrorStats.MaxErrorTime;
				MaxErrorBone = ErrorStats.MaxErrorBone;
				MaxErrorAnimName = Seq->GetFName();
				MaxErrorBoneName = BoneData[ErrorStats.MaxErrorBone].Name;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////
void FAnimCompressContext::GatherPreCompressionStats(UAnimSequence* Seq)
{
	CompressionSummary.GatherPreCompressionStats(Seq, AnimIndex, MaxAnimations);
}

void FAnimCompressContext::GatherPostCompressionStats(UAnimSequence* Seq, TArray<FBoneData>& BoneData)
{
	CompressionSummary.GatherPostCompressionStats(Seq, BoneData);
}

//////////////////////////////////////////////////////////////////////////////////////
// UAnimCompress

UAnimCompress::UAnimCompress(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Description = TEXT("None");
	TranslationCompressionFormat = ACF_None;
	RotationCompressionFormat = ACF_Float96NoW;

	UAnimationSettings* AnimationSettings = UAnimationSettings::Get();
	MaxCurveError = AnimationSettings->MaxCurveError;
}


void UAnimCompress::PrecalculateShortestQuaternionRoutes(
	TArray<struct FRotationTrack>& RotationData)
{
	const int32 NumTracks = RotationData.Num();
	for ( int32 TrackIndex = 0 ; TrackIndex < NumTracks ; ++TrackIndex )
	{
		FRotationTrack& SrcRot	= RotationData[TrackIndex];
		for ( int32 KeyIndex = 1 ; KeyIndex < SrcRot.RotKeys.Num() ; ++KeyIndex )
		{
			const FQuat& R0 = SrcRot.RotKeys[KeyIndex-1];
			FQuat& R1 = SrcRot.RotKeys[KeyIndex];
			
			if( (R0 | R1) < 0.f )
			{
				// invert R1 so that R0|R1 will always be >=0.f
				// making the delta between them the shortest possible route
				R1 = (R1 * -1);
			}
		}
	}
}

void PadByteStream(TArray<uint8>& CompressedByteStream, const int32 Alignment, uint8 sentinel)
{
	int32 Pad = Align( CompressedByteStream.Num(), 4 ) - CompressedByteStream.Num();
	for ( int32 i = 0 ; i < Pad ; ++i )
	{
		CompressedByteStream.Add(sentinel);
	}
}


void UAnimCompress::BitwiseCompressAnimationTracks(
	UAnimSequence* Seq, 
	AnimationCompressionFormat TargetTranslationFormat, 
	AnimationCompressionFormat TargetRotationFormat,
	AnimationCompressionFormat TargetScaleFormat,
	const TArray<FTranslationTrack>& TranslationData,
	const TArray<FRotationTrack>& RotationData,
	const TArray<FScaleTrack>& ScaleData,
	bool IncludeKeyTable)
{
	// Ensure supported compression formats.
	bool bInvalidCompressionFormat = false;
	if( !(TargetTranslationFormat == ACF_None) && !(TargetTranslationFormat == ACF_IntervalFixed32NoW) && !(TargetTranslationFormat == ACF_Float96NoW) )
	{
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( NSLOCTEXT("Engine", "UnknownTranslationCompressionFormat", "Unknown or unsupported translation compression format ({0})"), FText::AsNumber( (int32)TargetTranslationFormat ) ) );
		bInvalidCompressionFormat = true;
	}
	if ( !(TargetRotationFormat >= ACF_None && TargetRotationFormat < ACF_MAX) )
	{
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( NSLOCTEXT("Engine", "UnknownRotationCompressionFormat", "Unknown or unsupported rotation compression format ({0})"), FText::AsNumber( (int32)TargetRotationFormat ) ) );
		bInvalidCompressionFormat = true;
	}
	if( !(TargetScaleFormat == ACF_None) && !(TargetScaleFormat == ACF_IntervalFixed32NoW) && !(TargetScaleFormat == ACF_Float96NoW) )
	{
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( NSLOCTEXT("Engine", "UnknownScaleCompressionFormat", "Unknown or unsupported Scale compression format ({0})"), FText::AsNumber( (int32)TargetScaleFormat ) ) );
		bInvalidCompressionFormat = true;
	}
	if ( bInvalidCompressionFormat )
	{
		Seq->TranslationCompressionFormat	= ACF_None;
		Seq->RotationCompressionFormat		= ACF_None;
		Seq->ScaleCompressionFormat			= ACF_None;
		Seq->CompressedTrackOffsets.Empty();
		Seq->CompressedScaleOffsets.Empty();
		Seq->CompressedByteStream.Empty();
	}
	else
	{
		Seq->RotationCompressionFormat		= TargetRotationFormat;
		Seq->TranslationCompressionFormat	= TargetTranslationFormat;
		Seq->ScaleCompressionFormat	= TargetScaleFormat;

		check( TranslationData.Num() == RotationData.Num() );
		const int32 NumTracks = RotationData.Num();
		const bool bHasScale = ScaleData.Num() > 0;

		if ( NumTracks == 0 )
		{
			UE_LOG(LogAnimationCompression, Warning, TEXT("When compressing %s: no key-reduced data"), *Seq->GetName() );
		}

		Seq->CompressedTrackOffsets.Empty( NumTracks*4 );
		Seq->CompressedTrackOffsets.AddUninitialized( NumTracks*4 );

		// just empty it since there is chance this can be 0
		Seq->CompressedScaleOffsets.Empty();
		// only do this if Scale exists;
		if ( bHasScale )
		{
			Seq->CompressedScaleOffsets.SetStripSize(2);
			Seq->CompressedScaleOffsets.AddUninitialized( NumTracks );
		}

		Seq->CompressedByteStream.Empty();

		for ( int32 TrackIndex = 0 ; TrackIndex < NumTracks ; ++TrackIndex )
		{
			// Translation data.
			const FTranslationTrack& SrcTrans	= TranslationData[TrackIndex];

			const int32 OffsetTrans				= Seq->CompressedByteStream.Num();
			const int32 NumKeysTrans				= SrcTrans.PosKeys.Num();

			// Warn on empty data.
			if ( NumKeysTrans == 0 )
			{
				UE_LOG(LogAnimationCompression, Warning, TEXT("When compressing %s track %i: no translation keys"), *Seq->GetName(), TrackIndex );
			}

			checkf( (OffsetTrans % 4) == 0, TEXT("CompressedByteStream not aligned to four bytes" ) );
			Seq->CompressedTrackOffsets[TrackIndex*4] = OffsetTrans;
			Seq->CompressedTrackOffsets[TrackIndex*4+1] = NumKeysTrans;

			// Calculate the bounding box of the translation keys
			FBox PositionBounds(SrcTrans.PosKeys);

			float TransMins[3] = { PositionBounds.Min.X, PositionBounds.Min.Y, PositionBounds.Min.Z };
			float TransRanges[3] = { PositionBounds.Max.X - PositionBounds.Min.X, PositionBounds.Max.Y - PositionBounds.Min.Y, PositionBounds.Max.Z - PositionBounds.Min.Z };
			if ( TransRanges[0] == 0.f ) { TransRanges[0] = 1.f; }
			if ( TransRanges[1] == 0.f ) { TransRanges[1] = 1.f; }
			if ( TransRanges[2] == 0.f ) { TransRanges[2] = 1.f; }

			if (NumKeysTrans > 1)
			{
				// Write the mins and ranges if they'll be used on the other side
				if (TargetTranslationFormat == ACF_IntervalFixed32NoW)
				{
					AC_UnalignedWriteToStream( TransMins, sizeof(float)*3 );
					AC_UnalignedWriteToStream( TransRanges, sizeof(float)*3 );
				}

				// Pack the positions into the stream
				for ( int32 KeyIndex = 0 ; KeyIndex < NumKeysTrans ; ++KeyIndex )
				{
					const FVector& Vec = SrcTrans.PosKeys[KeyIndex];
					PackVectorToStream(Seq, TargetTranslationFormat, Vec, TransMins, TransRanges);
				}

				if (IncludeKeyTable)
				{
					// Align to four bytes.
					PadByteStream(Seq->CompressedByteStream, 4, AnimationPadSentinel );

					// write the key table
					const int32 NumFrames = Seq->NumFrames;
					const int32 LastFrame = Seq->NumFrames-1;
					const size_t FrameSize = Seq->NumFrames > 0xff ? sizeof(uint16) : sizeof(uint8);
					const float FrameRate = NumFrames / Seq->SequenceLength;

					const int32 TableSize= NumKeysTrans*FrameSize;
					const int32 TableDwords= (TableSize+3)>>2;
					const int32 StartingOffset = Seq->CompressedByteStream.Num();

					for ( int32 KeyIndex = 0 ; KeyIndex < NumKeysTrans ; ++KeyIndex )
					{
						// write the frame values for each key
						float KeyTime= SrcTrans.Times[KeyIndex];
						float FrameTime = KeyTime * FrameRate;
						int32 FrameIndex= FMath::Clamp(FMath::TruncToInt(FrameTime), 0, LastFrame);
						AC_UnalignedWriteToStream( &FrameIndex, FrameSize );
					}

					// Align to four bytes. Padding with 0's to round out the key table
					PadByteStream(Seq->CompressedByteStream, 4, 0 );

					const int32 EndingOffset = Seq->CompressedByteStream.Num();
					check((EndingOffset - StartingOffset) == (TableDwords*4));
				}
			}
			else if (NumKeysTrans == 1)
			{
				// A single translation key gets written out a single uncompressed float[3].
				AC_UnalignedWriteToStream( &(SrcTrans.PosKeys[0]), sizeof(FVector) );
			}
			else
			{
				UE_LOG(LogAnimationCompression, Warning, TEXT("When compressing %s track %i: no translation keys"), *Seq->GetName(), TrackIndex );
			}

			// Align to four bytes.
			PadByteStream(Seq->CompressedByteStream, 4, AnimationPadSentinel );

			// Compress rotation data.
			const FRotationTrack& SrcRot	= RotationData[TrackIndex];
			const int32 OffsetRot				= Seq->CompressedByteStream.Num();
			const int32 NumKeysRot			= SrcRot.RotKeys.Num();

			checkf( (OffsetRot % 4) == 0, TEXT("CompressedByteStream not aligned to four bytes" ) );
			Seq->CompressedTrackOffsets[TrackIndex*4+2] = OffsetRot;
			Seq->CompressedTrackOffsets[TrackIndex*4+3] = NumKeysRot;

			if ( NumKeysRot > 1 )
			{
				// Calculate the min/max of the XYZ components of the quaternion
				float MinX = 1.f;
				float MinY = 1.f;
				float MinZ = 1.f;
				float MaxX = -1.f;
				float MaxY = -1.f;
				float MaxZ = -1.f;
				for ( int32 KeyIndex = 0 ; KeyIndex < SrcRot.RotKeys.Num() ; ++KeyIndex )
				{
					FQuat Quat( SrcRot.RotKeys[KeyIndex] );
					if ( Quat.W < 0.f )
					{
						Quat.X = -Quat.X;
						Quat.Y = -Quat.Y;
						Quat.Z = -Quat.Z;
						Quat.W = -Quat.W;
					}
					Quat.Normalize();

					MinX = FMath::Min( MinX, Quat.X );
					MaxX = FMath::Max( MaxX, Quat.X );
					MinY = FMath::Min( MinY, Quat.Y );
					MaxY = FMath::Max( MaxY, Quat.Y );
					MinZ = FMath::Min( MinZ, Quat.Z );
					MaxZ = FMath::Max( MaxZ, Quat.Z );
				}
				const float Mins[3]	= { MinX,		MinY,		MinZ };
				float Ranges[3]		= { MaxX-MinX,	MaxY-MinY,	MaxZ-MinZ };
				if ( Ranges[0] == 0.f ) { Ranges[0] = 1.f; }
				if ( Ranges[1] == 0.f ) { Ranges[1] = 1.f; }
				if ( Ranges[2] == 0.f ) { Ranges[2] = 1.f; }

				// Write the mins and ranges if they'll be used on the other side
				if (TargetRotationFormat == ACF_IntervalFixed32NoW)
				{
					AC_UnalignedWriteToStream( Mins, sizeof(float)*3 );
					AC_UnalignedWriteToStream( Ranges, sizeof(float)*3 );
				}

				// n elements of the compressed type.
				for ( int32 KeyIndex = 0 ; KeyIndex < SrcRot.RotKeys.Num() ; ++KeyIndex )
				{
					const FQuat& Quat = SrcRot.RotKeys[KeyIndex];
					PackQuaternionToStream(Seq, TargetRotationFormat, Quat, Mins, Ranges);
				}

				// n elements of frame indices
				if (IncludeKeyTable)
				{
					// Align to four bytes.
					PadByteStream(Seq->CompressedByteStream, 4, AnimationPadSentinel );

					// write the key table
					const int32 NumFrames = Seq->NumFrames;
					const int32 LastFrame= Seq->NumFrames-1;
					const size_t FrameSize= Seq->NumFrames > 0xff ? sizeof(uint16) : sizeof(uint8);
					const float FrameRate = NumFrames / Seq->SequenceLength;

					const int32 TableSize= NumKeysRot*FrameSize;
					const int32 TableDwords= (TableSize+3)>>2;
					const int32 StartingOffset = Seq->CompressedByteStream.Num();

					for ( int32 KeyIndex = 0 ; KeyIndex < NumKeysRot ; ++KeyIndex )
					{
						// write the frame values for each key
						float KeyTime = SrcRot.Times[KeyIndex];
						float FrameTime = KeyTime * FrameRate;
						int32 FrameIndex = FMath::Clamp(FMath::TruncToInt(FrameTime), 0, LastFrame);
						AC_UnalignedWriteToStream( &FrameIndex, FrameSize );
					}

					// Align to four bytes. Padding with 0's to round out the key table
					PadByteStream(Seq->CompressedByteStream, 4, 0 );

					const int32 EndingOffset = Seq->CompressedByteStream.Num();
					check((EndingOffset - StartingOffset) == (TableDwords*4));

				}
			}
			else if ( NumKeysRot == 1 )
			{
				// For a rotation track of n=1 keys, the single key is packed as an FQuatFloat96NoW.
				const FQuat& Quat = SrcRot.RotKeys[0];
				const FQuatFloat96NoW QuatFloat96NoW( Quat );
				AC_UnalignedWriteToStream( &QuatFloat96NoW, sizeof(FQuatFloat96NoW) );
			}
			else
			{
				UE_LOG(LogAnimationCompression, Warning, TEXT("When compressing %s track %i: no rotation keys"), *Seq->GetName(), TrackIndex );
			}


			// Align to four bytes.
			PadByteStream(Seq->CompressedByteStream, 4, AnimationPadSentinel );

			// we also should do this only when scale exists. 
			if (bHasScale)
			{
				const FScaleTrack& SrcScale			= ScaleData[TrackIndex];

				const int32 OffsetScale				= Seq->CompressedByteStream.Num();
				const int32 NumKeysScale			= SrcScale.ScaleKeys.Num();

				checkf( (OffsetScale % 4) == 0, TEXT("CompressedByteStream not aligned to four bytes" ) );
				Seq->CompressedScaleOffsets.SetOffsetData(TrackIndex, 0, OffsetScale);
				Seq->CompressedScaleOffsets.SetOffsetData(TrackIndex, 1,  NumKeysScale);

				// Calculate the bounding box of the Scalelation keys
				FBox ScaleBoundsBounds(SrcScale.ScaleKeys);

				float ScaleMins[3] = { ScaleBoundsBounds.Min.X, ScaleBoundsBounds.Min.Y, ScaleBoundsBounds.Min.Z };
				float ScaleRanges[3] = { ScaleBoundsBounds.Max.X - ScaleBoundsBounds.Min.X, ScaleBoundsBounds.Max.Y - ScaleBoundsBounds.Min.Y, ScaleBoundsBounds.Max.Z - ScaleBoundsBounds.Min.Z };
				// @todo - this isn't good for scale 
				// 			if ( ScaleRanges[0] == 0.f ) { ScaleRanges[0] = 1.f; }
				// 			if ( ScaleRanges[1] == 0.f ) { ScaleRanges[1] = 1.f; }
				// 			if ( ScaleRanges[2] == 0.f ) { ScaleRanges[2] = 1.f; }

				if (NumKeysScale > 1)
				{
					// Write the mins and ranges if they'll be used on the other side
					if (TargetScaleFormat == ACF_IntervalFixed32NoW)
					{
						AC_UnalignedWriteToStream( ScaleMins, sizeof(float)*3 );
						AC_UnalignedWriteToStream( ScaleRanges, sizeof(float)*3 );
					}

					// Pack the positions into the stream
					for ( int32 KeyIndex = 0 ; KeyIndex < NumKeysScale ; ++KeyIndex )
					{
						const FVector& Vec = SrcScale.ScaleKeys[KeyIndex];
						PackVectorToStream(Seq, TargetScaleFormat, Vec, ScaleMins, ScaleRanges);
					}

					if (IncludeKeyTable)
					{
						// Align to four bytes.
						PadByteStream(Seq->CompressedByteStream, 4, AnimationPadSentinel );

						// write the key table
						const int32 NumFrames = Seq->NumFrames;
						const int32 LastFrame = Seq->NumFrames-1;
						const size_t FrameSize = Seq->NumFrames > 0xff ? sizeof(uint16) : sizeof(uint8);
						const float FrameRate = NumFrames / Seq->SequenceLength;

						const int32 TableSize= NumKeysScale*FrameSize;
						const int32 TableDwords= (TableSize+3)>>2;
						const int32 StartingOffset = Seq->CompressedByteStream.Num();

						for ( int32 KeyIndex = 0 ; KeyIndex < NumKeysScale ; ++KeyIndex )
						{
							// write the frame values for each key
							float KeyTime= SrcScale.Times[KeyIndex];
							float FrameTime = KeyTime * FrameRate;
							int32 FrameIndex= FMath::Clamp(FMath::TruncToInt(FrameTime), 0, LastFrame);
							AC_UnalignedWriteToStream( &FrameIndex, FrameSize );
						}

						// Align to four bytes. Padding with 0's to round out the key table
						PadByteStream(Seq->CompressedByteStream, 4, 0 );

						const int32 EndingOffset = Seq->CompressedByteStream.Num();
						check((EndingOffset - StartingOffset) == (TableDwords*4));
					}
				}
				else if (NumKeysScale == 1)
				{
					// A single Scalelation key gets written out a single uncompressed float[3].
					AC_UnalignedWriteToStream( &(SrcScale.ScaleKeys[0]), sizeof(FVector) );
				}
				else
				{
					UE_LOG(LogAnimationCompression, Warning, TEXT("When compressing %s track %i: no Scale keys"), *Seq->GetName(), TrackIndex );
				}

				// Align to four bytes.
				PadByteStream(Seq->CompressedByteStream, 4, AnimationPadSentinel );
			}
		}

		// Trim unused memory.
		Seq->CompressedByteStream.Shrink();
	}
}

#if WITH_EDITOR

FString UAnimCompress::MakeDDCKey()
{
	FString Key;
	TArray<uint8> TempBytes;
	TempBytes.Reserve(64);

	// Serialize the compression settings into a temporary array. The archive
	// is flagged as persistent so that machines of different endianness produce
	// identical binary results.
	FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);

	PopulateDDCKey(Ar);

	const uint8* SettingsAsBytes = TempBytes.GetData();
	Key.Reserve(TempBytes.Num() + 1);
	for (int32 ByteIndex = 0; ByteIndex < TempBytes.Num(); ++ByteIndex)
	{
		ByteToHex(SettingsAsBytes[ByteIndex], Key);
	}
	return Key;
}

void UAnimCompress::PopulateDDCKey(FArchive& Ar)
{
	uint8 TCF, RCF, SCF;
	TCF = (uint8)TranslationCompressionFormat.GetValue();
	RCF = (uint8)RotationCompressionFormat.GetValue();
	SCF = (uint8)ScaleCompressionFormat.GetValue();

	Ar << TCF << RCF << SCF;

	Ar << MaxCurveError;
}

/**

 * Tracks
 */

bool UAnimCompress::Reduce(UAnimSequence* AnimSeq, bool bOutput)
{
	bool bResult = false;
#if WITH_EDITORONLY_DATA
	USkeleton* AnimSkeleton = AnimSeq->GetSkeleton();
	const bool bSkeletonExistsIfNeeded = ( AnimSkeleton || !bNeedsSkeleton);
	if ( bSkeletonExistsIfNeeded )
	{
		FAnimCompressContext CompressContext(false, bOutput);
		Reduce(AnimSeq, CompressContext);

		bResult = true;
	}
#endif // WITH_EDITORONLY_DATA

	return bResult;
}

bool UAnimCompress::Reduce(class UAnimSequence* AnimSeq, FAnimCompressContext& Context)
{
	bool bResult = false;

#if WITH_EDITORONLY_DATA
	// Build skeleton metadata to use during the key reduction.
	TArray<FBoneData> BoneData;
	FAnimationUtils::BuildSkeletonMetaData(AnimSeq->GetSkeleton(), BoneData);
	Context.GatherPreCompressionStats(AnimSeq);

	// General key reduction.
	DoReduction(AnimSeq, BoneData);

	AnimSeq->bWasCompressedWithoutTranslations = false; // @fixmelh : bAnimRotationOnly

	AnimSeq->EncodingPkgVersion = CURRENT_ANIMATION_ENCODING_PACKAGE_VERSION;
	AnimSeq->MarkPackageDirty();

	// determine the error added by the compression
	Context.GatherPostCompressionStats(AnimSeq, BoneData);
	bResult = true;
#endif // WITH_EDITORONLY_DATA

	return bResult;
}
#endif // WITH_EDITOR

void UAnimCompress::FilterTrivialPositionKeys(
	FTranslationTrack& Track, 
	float MaxPosDelta)
{
	const int32 KeyCount = Track.Times.Num();
	check( Track.PosKeys.Num() == Track.Times.Num() );

	// Only bother doing anything if we have some keys!
	if( KeyCount > 1 )
	{
		const FVector& FirstPos = Track.PosKeys[0];

		bool bFramesIdentical = true;
		for(int32 KeyIndex=1; KeyIndex < KeyCount; ++KeyIndex)
		{
			const FVector& ThisPos = Track.PosKeys[KeyIndex];

			if( FMath::Abs(ThisPos.X - FirstPos.X) > MaxPosDelta || 
				FMath::Abs(ThisPos.Y - FirstPos.Y) > MaxPosDelta || 
				FMath::Abs(ThisPos.Z - FirstPos.Z) > MaxPosDelta )
			{
				bFramesIdentical = false;
				break;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			Track.PosKeys.RemoveAt(1, Track.PosKeys.Num()- 1);
			Track.PosKeys.Shrink();
			Track.Times.RemoveAt(1, Track.Times.Num()- 1);
			Track.Times.Shrink();
			Track.Times[0] = 0.0f;
		}
	}
}

void UAnimCompress::FilterTrivialPositionKeys(
	TArray<FTranslationTrack>& InputTracks, 
	float MaxPosDelta)
{
	const int32 NumTracks = InputTracks.Num();
	for( int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		FTranslationTrack& Track = InputTracks[TrackIndex];
		FilterTrivialPositionKeys(Track, MaxPosDelta);
	}
}


void UAnimCompress::FilterTrivialScaleKeys(
	FScaleTrack& Track, 
	float MaxScaleDelta)
{
	const int32 KeyCount = Track.Times.Num();
	check( Track.ScaleKeys.Num() == Track.Times.Num() );

	// Only bother doing anything if we have some keys!
	if( KeyCount > 1 )
	{
		const FVector& FirstPos = Track.ScaleKeys[0];

		bool bFramesIdentical = true;
		for(int32 KeyIndex=1; KeyIndex < KeyCount; ++KeyIndex)
		{
			const FVector& ThisPos = Track.ScaleKeys[KeyIndex];

			if( FMath::Abs(ThisPos.X - FirstPos.X) > MaxScaleDelta || 
				FMath::Abs(ThisPos.Y - FirstPos.Y) > MaxScaleDelta || 
				FMath::Abs(ThisPos.Z - FirstPos.Z) > MaxScaleDelta )
			{
				bFramesIdentical = false;
				break;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			Track.ScaleKeys.RemoveAt(1, Track.ScaleKeys.Num()- 1);
			Track.ScaleKeys.Shrink();
			Track.Times.RemoveAt(1, Track.Times.Num()- 1);
			Track.Times.Shrink();
			Track.Times[0] = 0.0f;
		}
	}
}

void UAnimCompress::FilterTrivialScaleKeys(
	TArray<FScaleTrack>& InputTracks, 
	float MaxScaleDelta)
{
	const int32 NumTracks = InputTracks.Num();
	for( int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		FScaleTrack& Track = InputTracks[TrackIndex];
		FilterTrivialScaleKeys(Track, MaxScaleDelta);
	}
}

void UAnimCompress::FilterTrivialRotationKeys(
	FRotationTrack& Track, 
	float MaxRotDelta)
{
	const int32 KeyCount = Track.Times.Num();
	check( Track.RotKeys.Num() == Track.Times.Num() );

	// Only bother doing anything if we have some keys!
	if(KeyCount > 1)
	{
		const FQuat& FirstRot = Track.RotKeys[0];
		bool bFramesIdentical = true;
		for(int32 KeyIndex=1; KeyIndex<KeyCount; ++KeyIndex)
		{
			if( FQuat::Error(FirstRot, Track.RotKeys[KeyIndex]) > MaxRotDelta )
			{
				bFramesIdentical = false;
				break;
			}
		}

		if(bFramesIdentical)
		{
			Track.RotKeys.RemoveAt(1, Track.RotKeys.Num()- 1);
			Track.RotKeys.Shrink();
			Track.Times.RemoveAt(1, Track.Times.Num()- 1);
			Track.Times.Shrink();
			Track.Times[0] = 0.0f;
		}			
	}
}


void UAnimCompress::FilterTrivialRotationKeys(
	TArray<FRotationTrack>& InputTracks, 
	float MaxRotDelta)
{
	const int32 NumTracks = InputTracks.Num();
	for( int32 TrackIndex = 0 ; TrackIndex < NumTracks ; ++TrackIndex )
	{
		FRotationTrack& Track = InputTracks[TrackIndex];
		FilterTrivialRotationKeys(Track, MaxRotDelta);
	}
}


void UAnimCompress::FilterTrivialKeys(
	TArray<FTranslationTrack>& PositionTracks,
	TArray<FRotationTrack>& RotationTracks, 
	TArray<FScaleTrack>& ScaleTracks,
	float MaxPosDelta,
	float MaxRotDelta,
	float MaxScaleDelta)
{
	FilterTrivialRotationKeys(RotationTracks, MaxRotDelta);
	FilterTrivialPositionKeys(PositionTracks, MaxPosDelta);
	FilterTrivialScaleKeys(ScaleTracks, MaxScaleDelta);
}


void UAnimCompress::FilterAnimRotationOnlyKeys(TArray<FTranslationTrack> & PositionTracks, UAnimSequence* AnimSeq)
{

}



void UAnimCompress::FilterIntermittentPositionKeys(
	FTranslationTrack& Track, 
	int32 StartIndex,
	int32 Interval)
{
	const int32 KeyCount = Track.Times.Num();
	const int32 FinalIndex = KeyCount - 1;
	StartIndex = FMath::Min(StartIndex, FinalIndex);

	check(Track.Times.Num() == Track.PosKeys.Num());

	TArray<FVector> NewPosKeys;
	TArray<float> NewTimes;

	NewTimes.Empty(KeyCount);
	NewPosKeys.Empty(KeyCount);

	// step through and retain the desired interval
	for (int32 KeyIndex = StartIndex; KeyIndex < KeyCount; KeyIndex += Interval )
	{
		NewTimes.Add( Track.Times[KeyIndex] );
		NewPosKeys.Add( Track.PosKeys[KeyIndex] );
	}

	NewTimes.Shrink();
	NewPosKeys.Shrink();

	Track.Times = NewTimes;
	Track.PosKeys = NewPosKeys;
}


void UAnimCompress::FilterIntermittentPositionKeys(
	TArray<FTranslationTrack>& PositionTracks, 
	int32 StartIndex,
	int32 Interval)
{
	const int32 NumPosTracks = PositionTracks.Num();

	// copy intermittent position keys
	for( int32 TrackIndex = 0; TrackIndex < NumPosTracks; ++TrackIndex )
	{
		FTranslationTrack& OldTrack	= PositionTracks[TrackIndex];
		FilterIntermittentPositionKeys(OldTrack, StartIndex, Interval);
	}
}


void UAnimCompress::FilterIntermittentRotationKeys(
	FRotationTrack& Track,
	int32 StartIndex,
	int32 Interval)
{
	const int32 KeyCount = Track.Times.Num();
	const int32 FinalIndex = KeyCount-1;
	StartIndex = FMath::Min(StartIndex, FinalIndex);

	check(Track.Times.Num() == Track.RotKeys.Num());

	TArray<FQuat> NewRotKeys;
	TArray<float> NewTimes;

	NewTimes.Empty(KeyCount);
	NewRotKeys.Empty(KeyCount);

	// step through and retain the desired interval
	for (int32 KeyIndex = StartIndex; KeyIndex < KeyCount; KeyIndex += Interval )
	{
		NewTimes.Add( Track.Times[KeyIndex] );
		NewRotKeys.Add( Track.RotKeys[KeyIndex] );
	}

	NewTimes.Shrink();
	NewRotKeys.Shrink();
	Track.Times = NewTimes;
	Track.RotKeys = NewRotKeys;
}


void UAnimCompress::FilterIntermittentRotationKeys(
	TArray<FRotationTrack>& RotationTracks, 
	int32 StartIndex,
	int32 Interval)
{
	const int32 NumRotTracks = RotationTracks.Num();

	// copy intermittent position keys
	for( int32 TrackIndex = 0; TrackIndex < NumRotTracks; ++TrackIndex )
	{
		FRotationTrack& OldTrack = RotationTracks[TrackIndex];
		FilterIntermittentRotationKeys(OldTrack, StartIndex, Interval);
	}
}


void UAnimCompress::FilterIntermittentKeys(
	TArray<FTranslationTrack>& PositionTracks, 
	TArray<FRotationTrack>& RotationTracks, 
	int32 StartIndex,
	int32 Interval)
{
	FilterIntermittentPositionKeys(PositionTracks, StartIndex, Interval);
	FilterIntermittentRotationKeys(RotationTracks, StartIndex, Interval);
}


void UAnimCompress::SeparateRawDataIntoTracks(
	const TArray<FRawAnimSequenceTrack>& RawAnimData,
	float SequenceLength,
	TArray<FTranslationTrack>& OutTranslationData,
	TArray<FRotationTrack>& OutRotationData, 
	TArray<FScaleTrack>& OutScaleData)
{
	const int32 NumTracks = RawAnimData.Num();

	OutTranslationData.Empty( NumTracks );
	OutRotationData.Empty( NumTracks );
	OutScaleData.Empty( NumTracks );
	OutTranslationData.AddZeroed( NumTracks );
	OutRotationData.AddZeroed( NumTracks );
	OutScaleData.AddZeroed( NumTracks );

	// only compress scale if it has valid scale keys
	bool bCompressScaleKeys = false;

	for ( int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		const FRawAnimSequenceTrack& RawTrack	= RawAnimData[TrackIndex];
		FTranslationTrack&	TranslationTrack	= OutTranslationData[TrackIndex];
		FRotationTrack&		RotationTrack		= OutRotationData[TrackIndex];
		FScaleTrack&		ScaleTrack			= OutScaleData[TrackIndex];

		const int32 PrevNumPosKeys = RawTrack.PosKeys.Num();
		const int32 PrevNumRotKeys = RawTrack.RotKeys.Num();
		const bool	bHasScale = (RawTrack.ScaleKeys.Num() != 0);
		bCompressScaleKeys |= bHasScale;

		// Do nothing if the data for this track is empty.
		if( PrevNumPosKeys == 0 || PrevNumRotKeys == 0 )
		{
			continue;
		}

		// Copy over position keys.
		for ( int32 PosIndex = 0; PosIndex < RawTrack.PosKeys.Num(); ++PosIndex )
		{
			TranslationTrack.PosKeys.Add( RawTrack.PosKeys[PosIndex] );
		}

		// Copy over rotation keys.
		for ( int32 RotIndex = 0; RotIndex < RawTrack.RotKeys.Num(); ++RotIndex )
		{
			RotationTrack.RotKeys.Add( RawTrack.RotKeys[RotIndex] );
		}

		// Set times for the translation track.
		if ( TranslationTrack.PosKeys.Num() > 1 )
		{
			const float PosFrameInterval = SequenceLength / static_cast<float>(TranslationTrack.PosKeys.Num()-1);
			for ( int32 PosIndex = 0; PosIndex < TranslationTrack.PosKeys.Num(); ++PosIndex )
			{
				TranslationTrack.Times.Add( PosIndex * PosFrameInterval );
			}
		}
		else
		{
			TranslationTrack.Times.Add( 0.f );
		}

		// Set times for the rotation track.
		if ( RotationTrack.RotKeys.Num() > 1 )
		{
			const float RotFrameInterval = SequenceLength / static_cast<float>(RotationTrack.RotKeys.Num()-1);
			for ( int32 RotIndex = 0; RotIndex < RotationTrack.RotKeys.Num(); ++RotIndex )
			{
				RotationTrack.Times.Add( RotIndex * RotFrameInterval );
			}
		}
		else
		{
			RotationTrack.Times.Add( 0.f );
		}

		if (bHasScale)
		{
			// Copy over scalekeys.
			for ( int32 ScaleIndex = 0; ScaleIndex < RawTrack.ScaleKeys.Num(); ++ScaleIndex )
			{
				ScaleTrack.ScaleKeys.Add( RawTrack.ScaleKeys[ScaleIndex] );
			}
					
			// Set times for the rotation track.
			if ( ScaleTrack.ScaleKeys.Num() > 1 )
			{
				const float ScaleFrameInterval = SequenceLength / static_cast<float>(ScaleTrack.ScaleKeys.Num()-1);
				for ( int32 ScaleIndex = 0; ScaleIndex < ScaleTrack.ScaleKeys.Num(); ++ScaleIndex )
				{
					ScaleTrack.Times.Add( ScaleIndex * ScaleFrameInterval );
				}
			}
			else
			{
				ScaleTrack.Times.Add( 0.f );
			}
		}

		// Trim unused memory.
		TranslationTrack.PosKeys.Shrink();
		TranslationTrack.Times.Shrink();
		RotationTrack.RotKeys.Shrink();
		RotationTrack.Times.Shrink();
		ScaleTrack.ScaleKeys.Shrink();
		ScaleTrack.Times.Shrink();
	}

	// if nothing to compress, empty the ScaleData
	// that way we don't have to worry about compressing scale data. 
	if (!bCompressScaleKeys)
	{
		OutScaleData.Empty();
	}
}
