// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimSequence.cpp: Skeletal mesh animation functions.
=============================================================================*/ 

#include "Animation/AnimSequence.h"
#include "Misc/MessageDialog.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Serialization/MemoryReader.h"
#include "UObject/UObjectIterator.h"
#include "UObject/PropertyPortFlags.h"
#include "EngineUtils.h"
#include "AnimEncoding.h"
#include "AnimationUtils.h"
#include "BonePose.h"
#include "AnimationRuntime.h"
#include "Animation/AnimCompress.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/Rig.h"
#include "Animation/AnimationSettings.h"
#include "EditorFramework/AssetImportData.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "DerivedDataCacheInterface.h"
#include "Interfaces/ITargetPlatform.h"
#include "Animation/AnimCompressionDerivedData.h"
#include "UObject/UObjectThreadContext.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#define USE_SLERP 0
#define LOCTEXT_NAMESPACE "AnimSequence"

DECLARE_CYCLE_STAT(TEXT("AnimSeq GetBonePose"), STAT_AnimSeq_GetBonePose, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("Build Anim Track Pairs"), STAT_BuildAnimTrackPairs, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("Extract Pose From Anim Data"), STAT_ExtractPoseFromAnimData, STATGROUP_Anim);

/////////////////////////////////////////////////////
// FRawAnimSequenceTrackNativeDeprecated

//@deprecated with VER_REPLACED_LAZY_ARRAY_WITH_UNTYPED_BULK_DATA
struct FRawAnimSequenceTrackNativeDeprecated
{
	TArray<FVector> PosKeys;
	TArray<FQuat> RotKeys;
	friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrackNativeDeprecated& T)
	{
		return	Ar << T.PosKeys << T.RotKeys;
	}
};

/////////////////////////////////////////////////////
// FCurveTrack

/** Returns true if valid curve weight exists in the array*/
bool FCurveTrack::IsValidCurveTrack()
{
	bool bValid = false;

	if ( CurveName != NAME_None )
	{
		for (int32 I=0; I<CurveWeights.Num(); ++I)
		{
			// it has valid weight
			if (CurveWeights[I]>KINDA_SMALL_NUMBER)
			{
				bValid = true;
				break;
			}
		}
	}

	return bValid;
}

/** This is very simple cut to 1 key method if all is same since I see so many redundant same value in every frame 
 *  Eventually this can get more complicated 
 *  Will return true if compressed to 1. Return false otherwise
 **/
bool FCurveTrack::CompressCurveWeights()
{
	// if always 1, no reason to do this
	if (CurveWeights.Num() > 1)
	{
		bool bCompress = true;
		// first weight
		float FirstWeight = CurveWeights[0];

		for (int32 I=1; I<CurveWeights.Num(); ++I)
		{
			// see if my key is same as previous
			if (fabs(FirstWeight - CurveWeights[I]) > SMALL_NUMBER)
			{
				// if not same, just get out, you don't like to compress this to 1 key
				bCompress = false;
				break;
			}
		} 

		if (bCompress)
		{
			CurveWeights.Empty();
			CurveWeights.Add(FirstWeight);
			CurveWeights.Shrink();
		}

		return bCompress;
	}

	// nothing changed
	return false;
}

/////////////////////////////////////////////////////////////

// since we want this change for hot fix, I can't change header file, 
// next time move this to the header
float GetIntervalPerKey(int32 NumFrames, float SequenceLength) 
{
	return (NumFrames > 1) ? (SequenceLength / (NumFrames-1)) : MINIMUM_ANIMATION_LENGTH;
}

/////////////////////////////////////////////////////
// UAnimSequence

UAnimSequence::UAnimSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Interpolation(EAnimInterpolationType::Linear)
	, bEnableRootMotion(false)
	, RootMotionRootLock(ERootMotionRootLock::RefPose)
	, bRootMotionSettingsCopiedFromMontage(false)
	, bUseRawDataOnly(!FPlatformProperties::RequiresCookedData())
{
	RateScale = 1.0;
	CompressedRawDataSize = 0.f;
}

void UAnimSequence::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
	MarkerDataUpdateCounter = 0;
#endif
	Super::PostInitProperties();
}

void UAnimSequence::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (CumulativeResourceSize.GetResourceSizeMode() == EResourceSizeMode::Exclusive)
	{
		// All of the sequence data is serialized and will be counted as part of the direct object size rather than as a resource
	}
	else
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes((CompressedTrackOffsets.Num() == 0) ? GetApproxRawSize() : GetApproxCompressedSize());
	}
}

void UAnimSequence::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}
#endif


	OutTags.Add(FAssetRegistryTag(TEXT("Compression Ratio"), FString::Printf(TEXT("%.03f"), (float)GetApproxCompressedSize() / (float)GetUncompressedRawSize()), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag(TEXT("Compressed Size (KB)"), FString::Printf(TEXT("%.02f"), (float)GetApproxCompressedSize() / 1024.0f), FAssetRegistryTag::TT_Numerical));

	Super::GetAssetRegistryTags(OutTags);
}

int32 UAnimSequence::GetUncompressedRawSize() const
{
	return ((sizeof(FVector) + sizeof(FQuat) + sizeof(FVector)) * RawAnimationData.Num() * NumFrames);
}

int32 UAnimSequence::GetApproxRawSize() const
{
	int32 Total = sizeof(FRawAnimSequenceTrack) * RawAnimationData.Num();
	for (int32 i=0;i<RawAnimationData.Num();++i)
	{
		const FRawAnimSequenceTrack& RawTrack = RawAnimationData[i];
		Total +=
			sizeof( FVector ) * RawTrack.PosKeys.Num() +
			sizeof( FQuat ) * RawTrack.RotKeys.Num() + 
			sizeof( FVector ) * RawTrack.ScaleKeys.Num(); 
	}
	return Total;
}

int32 UAnimSequence::GetApproxCompressedSize() const
{
	const int32 Total = sizeof(int32)*CompressedTrackOffsets.Num() + CompressedByteStream.Num() + CompressedScaleOffsets.GetMemorySize();
	return Total;
}

/**
 * Deserializes old compressed track formats from the specified archive.
 */
static void LoadOldCompressedTrack(FArchive& Ar, FCompressedTrack& Dst, int32 ByteStreamStride)
{
	// Serialize from the archive to a buffer.
	int32 NumBytes = 0;
	Ar << NumBytes;

	TArray<uint8> SerializedData;
	SerializedData.Empty( NumBytes );
	SerializedData.AddUninitialized( NumBytes );
	Ar.Serialize( SerializedData.GetData(), NumBytes );

	// Serialize the key times.
	Ar << Dst.Times;

	// Serialize mins and ranges.
	Ar << Dst.Mins[0] << Dst.Mins[1] << Dst.Mins[2];
	Ar << Dst.Ranges[0] << Dst.Ranges[1] << Dst.Ranges[2];
}

void UAnimSequence::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Animation);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	FRawCurveTracks RawCurveCache;

	if (Ar.IsCooking())
	{
		RawCurveCache.FloatCurves = MoveTemp(RawCurveData.FloatCurves);
		RawCurveData.FloatCurves.Reset();

#if WITH_EDITORONLY_DATA
		RawCurveCache.VectorCurves = MoveTemp(RawCurveData.VectorCurves);
		RawCurveData.VectorCurves.Reset();

		RawCurveCache.TransformCurves = MoveTemp(RawCurveData.TransformCurves);
		RawCurveData.TransformCurves.Reset();
#endif
	}

	Super::Serialize(Ar);

	if (Ar.IsCooking())
	{
		RawCurveData.FloatCurves = MoveTemp(RawCurveCache.FloatCurves);
#if WITH_EDITORONLY_DATA
		RawCurveData.VectorCurves = MoveTemp(RawCurveCache.VectorCurves);
		RawCurveData.TransformCurves = MoveTemp(RawCurveCache.TransformCurves);
#endif
	}

	FStripDataFlags StripFlags( Ar );
	if( !StripFlags.IsEditorDataStripped() )
	{
		Ar << RawAnimationData;
#if WITH_EDITORONLY_DATA
		if (!Ar.IsCooking())
		{
			if (Ar.UE4Ver() >= VER_UE4_ANIMATION_ADD_TRACKCURVES)
			{
				Ar << SourceRawAnimationData;
			}
		}
#endif // WITH_EDITORONLY_DATA
	}

	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MoveCompressedAnimDataToTheDDC)
	{
		// Serialize the compressed byte stream from the archive to the buffer.
		int32 NumBytes;
		Ar << NumBytes;

		TArray<uint8> SerializedData;
		SerializedData.AddUninitialized(NumBytes);
		Ar.Serialize(SerializedData.GetData(), NumBytes);
	}
	else
	{
		const bool bIsCooking = Ar.IsCooking();
		const bool bIsDuplicating = Ar.HasAnyPortFlags(PPF_DuplicateForPIE) || Ar.HasAnyPortFlags(PPF_Duplicate);
		const bool bIsTransacting = Ar.IsTransacting();
		const bool bIsCookingForDedicatedServer = bIsCooking && Ar.CookingTarget()->IsServerOnly();
		const bool bIsCountingMemory = Ar.IsCountingMemory();
		const bool bCookingTargetNeedsCompressedData = bIsCooking && (!UAnimationSettings::Get()->bStripAnimationDataOnDedicatedServer || !bIsCookingForDedicatedServer || bEnableRootMotion);

		bool bSerializeCompressedData = bCookingTargetNeedsCompressedData || bIsDuplicating || bIsTransacting || bIsCountingMemory;
		Ar << bSerializeCompressedData;

		if (bCookingTargetNeedsCompressedData)
		{
			if(GetSkeleton())
			{
				// Validate that we are cooking valid compressed data.
				checkf(Ar.IsObjectReferenceCollector() || (GetSkeletonVirtualBoneGuid() == GetSkeleton()->GetVirtualBoneGuid()), TEXT("Attempting to cook animation '%s' containing invalid virtual bone guid! Animation:%s Skeleton:%s"), *GetFullName(), *GetSkeletonVirtualBoneGuid().ToString(EGuidFormats::HexValuesInBraces), *GetSkeleton()->GetVirtualBoneGuid().ToString(EGuidFormats::HexValuesInBraces));
			}
		}

		if (bIsDuplicating)
		{
			Ar << bCompressionInProgress;
		}

		if (bSerializeCompressedData)
		{
			SerializeCompressedData(Ar,false);
			Ar << bUseRawDataOnly;
		}
	}

#if WITH_EDITORONLY_DATA
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ASSET_IMPORT_DATA_AS_JSON && !AssetImportData)
	{
		// AssetImportData should always be valid
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}

	// SourceFilePath and SourceFileTimestamp were moved into a subobject
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ADDED_FBX_ASSET_IMPORT_DATA && AssetImportData)
	{
		// AssetImportData should always have been set up in the constructor where this is relevant
		FAssetImportInfo Info;
		Info.Insert(FAssetImportInfo::FSourceFile(SourceFilePath_DEPRECATED));
		AssetImportData->SourceData = MoveTemp(Info);
		
		SourceFilePath_DEPRECATED = TEXT("");
		SourceFileTimestamp_DEPRECATED = TEXT("");
	}

#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
bool UAnimSequence::IsValidToPlay() const
{
	// make sure sequence length is valid and raw animation data exists, and compressed
	return ( SequenceLength > 0.f);
}
#endif

void UAnimSequence::SortSyncMarkers()
{
	// First make sure all SyncMarkers are within a valid range
	for (auto& SyncMarker : AuthoredSyncMarkers)
	{
		SyncMarker.Time = FMath::Clamp(SyncMarker.Time, 0.f, SequenceLength);
	}

	// Then sort
	AuthoredSyncMarkers.Sort();

	// Then refresh data
	RefreshSyncMarkerDataFromAuthored();
}

void UAnimSequence::PreSave(const class ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR
	// we have to bake it if it's not baked
	if (DoesNeedRebake())
	{
		BakeTrackCurvesToRawAnimation();
	}

	// make sure if it does contain transform curvesm it contains source data
	// empty track animation still can be made by retargeting to invalid skeleton
	// make sure to not trigger ensure if RawAnimationData is also null
	
	// Why should we not be able to have empty transform curves?
	ensure(!DoesContainTransformCurves() || (RawAnimationData.Num()==0 || SourceRawAnimationData.Num() != 0));

	if (DoesNeedRecompress())
	{
		RequestSyncAnimRecompression();
		ensureAlwaysMsgf(!bUseRawDataOnly,  TEXT("Animation : %s failed to compress"), *GetName());
	}
#endif

	Super::PreSave(TargetPlatform);
}

void UAnimSequence::PostLoad()
{
#if WITH_EDITOR
	if (!RawDataGuid.IsValid())
	{
		RawDataGuid = GenerateGuidFromRawData();
	}

	// I have to do this first thing in here
	// so that remove all NaNs before even being read
	if(GetLinkerUE4Version() < VER_UE4_ANIMATION_REMOVE_NANS)
	{
		RemoveNaNTracks();
	}

	VerifyTrackMap(nullptr);

#endif // WITH_EDITOR

	Super::PostLoad();

	// if valid additive, but if base additive isn't 
	// this seems to happen from retargeting sometimes, which we still have to investigate why, 
	// but this causes issue since once this happens this is unrecoverable until you delete from outside of editor
	if (IsValidAdditive())
	{
		if (RefPoseSeq && RefPoseSeq->GetSkeleton() != GetSkeleton())
		{
			// if this happens, there was a issue with retargeting, 
			UE_LOG(LogAnimation, Warning, TEXT("Animation %s - Invalid additive animation base animation (%s)"), *GetName(), *RefPoseSeq->GetName());
			RefPoseSeq = nullptr;
		}
	}

#if WITH_EDITOR
	static bool ForcedRecompressionSetting = FAnimationUtils::GetForcedRecompressionSetting();

	if (ForcedRecompressionSetting)
	{
		//Force recompression
		RawDataGuid = FGuid::NewGuid();
		bUseRawDataOnly = true;
	}

	if (bUseRawDataOnly && !bCompressionInProgress)
	{
		RequestSyncAnimRecompression();
	}
#endif

	// Ensure notifies are sorted.
	SortNotifies();

	// No animation data is found. Warn - this should check before we check CompressedTrackOffsets size
	// Otherwise, we'll see empty data set crashing game due to no CompressedTrackOffsets
	// You can't check RawAnimationData size since it gets removed during cooking
	if ( NumFrames == 0 && RawCurveData.FloatCurves.Num() == 0 )
	{
		UE_LOG(LogAnimation, Warning, TEXT("No animation data exists for sequence %s (%s)"), *GetName(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
#if WITH_EDITOR
		if (!IsRunningGame())
		{
			static FName NAME_LoadErrors("LoadErrors");
			FMessageLog LoadErrors(NAME_LoadErrors);

			TSharedRef<FTokenizedMessage> Message = LoadErrors.Warning();
			Message->AddToken(FTextToken::Create(LOCTEXT("EmptyAnimationData1", "The Animation ")));
			Message->AddToken(FAssetNameToken::Create(GetPathName(), FText::FromString(GetName())));
			Message->AddToken(FTextToken::Create(LOCTEXT("EmptyAnimationData2", " has no animation data. Recommend to remove.")));
			LoadErrors.Notify();
		}
#endif
	}
	// @remove temp hack for fixing length
	// @todo need to fix importer/editing feature
	else if ( SequenceLength == 0.f )
	{
		ensure(NumFrames == 1);
		SequenceLength = MINIMUM_ANIMATION_LENGTH;
	}
	// Raw data exists, but missing compress animation data
	else if( !bCompressionInProgress && GetSkeleton() && CompressedTrackOffsets.Num() == 0 && RawAnimationData.Num() > 0)
	{
		UE_LOG(LogAnimation, Fatal, TEXT("No animation compression exists for sequence %s (%s)"), *GetName(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
	}

	// If we're in the game and compressed animation data exists, whack the raw data.
	if (FPlatformProperties::RequiresCookedData())
	{
		if (GetSkeleton())
		{
			SetSkeletonVirtualBoneGuid(GetSkeleton()->GetVirtualBoneGuid());
		}
		if( RawAnimationData.Num() > 0  && CompressedTrackOffsets.Num() > 0 )
		{
#if 0//@todo.Cooker/Package...
			// Don't do this on consoles; raw animation data should have been stripped during cook!
			UE_LOG(LogAnimation, Fatal, TEXT("Cooker did not strip raw animation from sequence %s"), *GetName() );
#else
			// Remove raw animation data.
			for ( int32 TrackIndex = 0 ; TrackIndex < RawAnimationData.Num() ; ++TrackIndex )
			{
				FRawAnimSequenceTrack& RawTrack = RawAnimationData[TrackIndex];
				RawTrack.PosKeys.Empty();
				RawTrack.RotKeys.Empty();
				RawTrack.ScaleKeys.Empty();
			}
			
			RawAnimationData.Empty();
#endif
		}
	}

#if WITH_EDITORONLY_DATA
	bWasCompressedWithoutTranslations = false; //@todoanim: @fixmelh : AnimRotationOnly - GetAnimSet()->bAnimRotationOnly;
#endif // WITH_EDITORONLY_DATA

	if( IsRunningGame() )
	{
		// this probably will not show newly created animations in PIE but will show them in the game once they have been saved off
		INC_DWORD_STAT_BY( STAT_AnimationMemory, GetResourceSizeBytes(EResourceSizeMode::Exclusive) );
	}

	{
		LOG_SCOPE_VERBOSITY_OVERRIDE(LogAnimation, ELogVerbosity::Warning);
 		// convert animnotifies
 		for (int32 I=0; I<Notifies.Num(); ++I)
 		{
 			if (Notifies[I].Notify!=NULL)
 			{
				FString Label = Notifies[I].Notify->GetClass()->GetName();
				Label = Label.Replace(TEXT("AnimNotify_"), TEXT(""), ESearchCase::CaseSensitive);
				Notifies[I].NotifyName = FName(*Label);
 			}
 		}
	}

	for(FAnimNotifyEvent& Notify : Notifies)
	{
		if(Notify.DisplayTime_DEPRECATED != 0.0f)
		{
			Notify.Clear();
			Notify.LinkSequence(this, Notify.DisplayTime_DEPRECATED);
		}
		else
		{
			Notify.LinkSequence(this, Notify.GetTime());
		}
	
		if(Notify.Duration != 0.0f)
		{
			Notify.EndLink.LinkSequence(this, Notify.GetTime() + Notify.Duration);
		}
	}

	if (USkeleton* CurrentSkeleton = GetSkeleton())
	{
		VerifyCurveNames<FFloatCurve>(*CurrentSkeleton, USkeleton::AnimCurveMappingName, CompressedCurveData.FloatCurves);

#if WITH_EDITOR
		VerifyCurveNames<FTransformCurve>(*CurrentSkeleton, USkeleton::AnimTrackCurveMappingName, RawCurveData.TransformCurves);
#endif
	}

#if WITH_EDITOR

	// Compressed curve flags are not authoritative (they come from the DDC). Keep them up to date with
	// actual anim flags 
	for (FFloatCurve& Curve : RawCurveData.FloatCurves)
	{
		if (FAnimCurveBase* CompressedCurve = CompressedCurveData.GetCurveData(Curve.Name.UID))
		{
			CompressedCurve->SetCurveTypeFlags(Curve.GetCurveTypeFlags());
		}
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void ShowResaveMessage(const UAnimSequence* Sequence)
{
	if (IsRunningCommandlet())
	{
		UE_LOG(LogAnimation, Log, TEXT("Resave Animation Required(%s, %s): Fixing track data and recompressing."), *GetNameSafe(Sequence), *Sequence->GetPathName());

		/*static FName NAME_LoadErrors("LoadErrors");
		FMessageLog LoadErrors(NAME_LoadErrors);

		TSharedRef<FTokenizedMessage> Message = LoadErrors.Warning();
		Message->AddToken(FTextToken::Create(LOCTEXT("AnimationNeedsResave1", "The Animation ")));
		Message->AddToken(FAssetNameToken::Create(Sequence->GetPathName(), FText::FromString(GetNameSafe(Sequence))));
		Message->AddToken(FTextToken::Create(LOCTEXT("AnimationNeedsResave2", " needs resave.")));
		LoadErrors.Notify();*/
	}
}

void UAnimSequence::VerifyTrackMap(USkeleton* MySkeleton)
{
	USkeleton* UseSkeleton = (MySkeleton)? MySkeleton: GetSkeleton();

	if( AnimationTrackNames.Num() != TrackToSkeletonMapTable.Num() && UseSkeleton!=nullptr)
	{
		ShowResaveMessage(this);

		AnimationTrackNames.Empty();
		AnimationTrackNames.AddUninitialized(TrackToSkeletonMapTable.Num());
		for(int32 I=0; I<TrackToSkeletonMapTable.Num(); ++I)
		{
			const FTrackToSkeletonMap& TrackMap = TrackToSkeletonMapTable[I];
			AnimationTrackNames[I] = UseSkeleton->GetReferenceSkeleton().GetBoneName(TrackMap.BoneTreeIndex);
		}
	}
	else if (UseSkeleton != nullptr)
	{
		// first check if any of them needs to be removed
		{
			int32 NumTracks = AnimationTrackNames.Num();
			int32 NumSkeletonBone = UseSkeleton->GetReferenceSkeleton().GetRawBoneNum();

			// the first fix is to make sure 
			bool bNeedsFixing = false;
			// verify all tracks are still valid
			for(int32 TrackIndex=0; TrackIndex<NumTracks; TrackIndex++)
			{
				int32 SkeletonBoneIndex = TrackToSkeletonMapTable[TrackIndex].BoneTreeIndex;
				// invalid index found
				if(SkeletonBoneIndex == INDEX_NONE || NumSkeletonBone <= SkeletonBoneIndex)
				{
					// if one is invalid, fix up for all.
					// you don't know what index got messed up
					bNeedsFixing = true;
					break;
				}
			}

			if(bNeedsFixing)
			{
				ShowResaveMessage(this);

				for(int32 I=NumTracks-1; I>=0; --I)
				{
					int32 BoneTreeIndex = UseSkeleton->GetReferenceSkeleton().FindBoneIndex(AnimationTrackNames[I]);
					if(BoneTreeIndex == INDEX_NONE)
					{
						RemoveTrack(I);
					}
					else
					{
						TrackToSkeletonMapTable[I].BoneTreeIndex = BoneTreeIndex;
					}
				}
			}
		}
		
		for(int32 I=0; I<AnimationTrackNames.Num(); ++I)
		{
			FTrackToSkeletonMap& TrackMap = TrackToSkeletonMapTable[I];
			TrackMap.BoneTreeIndex = UseSkeleton->GetReferenceSkeleton().FindBoneIndex(AnimationTrackNames[I]);
		}		
	}
}

#endif // WITH_EDITOR
void UAnimSequence::BeginDestroy()
{
	Super::BeginDestroy();

	// clear any active codec links
	RotationCodec = NULL;
	TranslationCodec = NULL;

	if( IsRunningGame() )
	{
		DEC_DWORD_STAT_BY( STAT_AnimationMemory, GetResourceSizeBytes(EResourceSizeMode::Exclusive) );
	}
}

#if WITH_EDITOR
void UAnimSequence::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(!IsTemplate())
	{
		// Make sure package is marked dirty when doing stuff like adding/removing notifies
		MarkPackageDirty();
	}

	if (AdditiveAnimType != AAT_None)
	{
		if (RefPoseType == ABPT_None)
		{
			// slate will take care of change
			RefPoseType = ABPT_RefPose;
		}
	}

	if (RefPoseSeq != NULL)
	{
		if (RefPoseSeq->GetSkeleton() != GetSkeleton()) // @todo this may require to be changed when hierarchy of skeletons is introduced
		{
			RefPoseSeq = NULL;
		}
	}

	bool bAdditiveSettingsChanged = false;
	if(PropertyChangedEvent.Property)
	{
		const bool bChangedRefFrameIndex = PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimSequence, RefFrameIndex);

		if (bChangedRefFrameIndex)
		{
			bUseRawDataOnly = true;
		}

		if ((bChangedRefFrameIndex && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimSequence, RefPoseSeq) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimSequence, RefPoseType))
		{
			bAdditiveSettingsChanged = true;
		}
		
	}
	// @Todo fix me: This is temporary fix to make sure they always have compressed data
	if (RawAnimationData.Num() > 0 && (CompressedTrackOffsets.Num() == 0 || bAdditiveSettingsChanged))
	{
		PostProcessSequence();
	}
}

void UAnimSequence::PostDuplicate(bool bDuplicateForPIE)
{
	// if transform curve exists, mark as bake
	if (DoesContainTransformCurves())
	{
		bNeedsRebake = true;
	}

	Super::PostDuplicate(bDuplicateForPIE);
}
#endif // WITH_EDITOR

// @todo DB: Optimize!
template<typename TimeArray>
static int32 FindKeyIndex(float Time, const TimeArray& Times)
{
	int32 FoundIndex = 0;
	for ( int32 Index = 0 ; Index < Times.Num() ; ++Index )
	{
		const float KeyTime = Times(Index);
		if ( Time >= KeyTime )
		{
			FoundIndex = Index;
		}
		else
		{
			break;
		}
	}
	return FoundIndex;
}

void UAnimSequence::GetBoneTransform(FTransform& OutAtom, int32 TrackIndex, float Time, bool bUseRawData) const
{
	// If the caller didn't request that raw animation data be used . . .
	if ( !bUseRawData )
	{
		if ( CompressedTrackOffsets.Num() > 0 )
		{
			AnimationFormat_GetBoneAtom( OutAtom, *this, TrackIndex, Time );
			return;
		}
	}

	ExtractBoneTransform(RawAnimationData, OutAtom, TrackIndex, Time);
}

void UAnimSequence::ExtractBoneTransform(const TArray<struct FRawAnimSequenceTrack>& InRawAnimationData, FTransform& OutAtom, int32 TrackIndex, float Time) const
{
	// Bail out if the animation data doesn't exists (e.g. was stripped by the cooker).
	if(InRawAnimationData.Num() == 0)
	{
		UE_LOG(LogAnimation, Log, TEXT("UAnimSequence::GetBoneTransform : No anim data in AnimSequence[%s]!"),*GetFullName());
		OutAtom.SetIdentity();
		return;
	}

	ExtractBoneTransform(InRawAnimationData[TrackIndex], OutAtom, Time);
}

void UAnimSequence::ExtractBoneTransform(const struct FRawAnimSequenceTrack& RawTrack, FTransform& OutAtom, int32 KeyIndex) const
{
	// Bail out (with rather wacky data) if data is empty for some reason.
	if (RawTrack.PosKeys.Num() == 0 || RawTrack.RotKeys.Num() == 0)
	{
		UE_LOG(LogAnimation, Log, TEXT("UAnimSequence::GetBoneTransform : No anim data in AnimSequence!"));
		OutAtom.SetIdentity();
		return;
	}

	const int32 PosKeyIndex = FMath::Min(KeyIndex, RawTrack.PosKeys.Num() - 1);
	const int32 RotKeyIndex = FMath::Min(KeyIndex, RawTrack.RotKeys.Num() - 1);
	static const FVector DefaultScale3D = FVector(1.f);

	OutAtom.SetTranslation(RawTrack.PosKeys[PosKeyIndex]);
	OutAtom.SetRotation(RawTrack.RotKeys[RotKeyIndex]);
	if (RawTrack.ScaleKeys.Num() > 0)
	{
		const int32 ScaleKeyIndex = FMath::Min(KeyIndex, RawTrack.ScaleKeys.Num() - 1);
		OutAtom.SetScale3D(RawTrack.ScaleKeys[ScaleKeyIndex]);
	}
	else
	{
		OutAtom.SetScale3D(DefaultScale3D);
	}
}

void UAnimSequence::ExtractBoneTransform(const struct FRawAnimSequenceTrack& RawTrack, FTransform& OutAtom, float Time) const
{
	// Bail out (with rather wacky data) if data is empty for some reason.
	if(RawTrack.PosKeys.Num() == 0 || RawTrack.RotKeys.Num() == 0)
	{
		UE_LOG(LogAnimation, Log, TEXT("UAnimSequence::GetBoneTransform : No anim data in AnimSequence[%s]!"),*GetFullName());
		OutAtom.SetIdentity();
		return;
	}

	int32 KeyIndex1, KeyIndex2;
	float Alpha;
	FAnimationRuntime::GetKeyIndicesFromTime(KeyIndex1, KeyIndex2, Alpha, Time, NumFrames, SequenceLength);
	// @Todo fix me: this change is not good, it has lots of branches. But we'd like to save memory for not saving scale if no scale change exists
	const bool bHasScaleKey = (RawTrack.ScaleKeys.Num() > 0);
	static const FVector DefaultScale3D = FVector(1.f);

	if (Interpolation == EAnimInterpolationType::Step) 
	{
		Alpha = 0.f;
	}

	if(Alpha <= 0.f)
	{
		const int32 PosKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.PosKeys.Num()-1);
		const int32 RotKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.RotKeys.Num()-1);
		if(bHasScaleKey)
		{
			const int32 ScaleKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.ScaleKeys.Num()-1);
			OutAtom = FTransform(RawTrack.RotKeys[RotKeyIndex1], RawTrack.PosKeys[PosKeyIndex1], RawTrack.ScaleKeys[ScaleKeyIndex1]);
		}
		else
		{
			OutAtom = FTransform(RawTrack.RotKeys[RotKeyIndex1], RawTrack.PosKeys[PosKeyIndex1], DefaultScale3D);
		}
		return;
	}
	else if(Alpha >= 1.f)
	{
		const int32 PosKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.PosKeys.Num()-1);
		const int32 RotKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.RotKeys.Num()-1);
		if(bHasScaleKey)
		{
			const int32 ScaleKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.ScaleKeys.Num()-1);
			OutAtom = FTransform(RawTrack.RotKeys[RotKeyIndex2], RawTrack.PosKeys[PosKeyIndex2], RawTrack.ScaleKeys[ScaleKeyIndex2]);
		}
		else
		{
			OutAtom = FTransform(RawTrack.RotKeys[RotKeyIndex2], RawTrack.PosKeys[PosKeyIndex2], DefaultScale3D);
		}
		return;
	}

	const int32 PosKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.PosKeys.Num()-1);
	const int32 RotKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.RotKeys.Num()-1);

	const int32 PosKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.PosKeys.Num()-1);
	const int32 RotKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.RotKeys.Num()-1);

	FTransform KeyAtom1, KeyAtom2;

	if(bHasScaleKey)
	{
		const int32 ScaleKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.ScaleKeys.Num()-1);
		const int32 ScaleKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.ScaleKeys.Num()-1);

		KeyAtom1 = FTransform(RawTrack.RotKeys[RotKeyIndex1], RawTrack.PosKeys[PosKeyIndex1], RawTrack.ScaleKeys[ScaleKeyIndex1]);
		KeyAtom2 = FTransform(RawTrack.RotKeys[RotKeyIndex2], RawTrack.PosKeys[PosKeyIndex2], RawTrack.ScaleKeys[ScaleKeyIndex2]);
	}
	else
	{
		KeyAtom1 = FTransform(RawTrack.RotKeys[RotKeyIndex1], RawTrack.PosKeys[PosKeyIndex1], DefaultScale3D);
		KeyAtom2 = FTransform(RawTrack.RotKeys[RotKeyIndex2], RawTrack.PosKeys[PosKeyIndex2], DefaultScale3D);
	}

	// 	UE_LOG(LogAnimation, Log, TEXT(" *  *  *  Position. PosKeyIndex1: %3d, PosKeyIndex2: %3d, Alpha: %f"), PosKeyIndex1, PosKeyIndex2, Alpha);
	// 	UE_LOG(LogAnimation, Log, TEXT(" *  *  *  Rotation. RotKeyIndex1: %3d, RotKeyIndex2: %3d, Alpha: %f"), RotKeyIndex1, RotKeyIndex2, Alpha);

	OutAtom.Blend(KeyAtom1, KeyAtom2, Alpha);
	OutAtom.NormalizeRotation();
}

void UAnimSequence::HandleAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, struct FAnimNotifyQueue& NotifyQueue) const
{
	Super::HandleAssetPlayerTickedInternal(Context, PreviousTime, MoveDelta, Instance, NotifyQueue);

	if (bEnableRootMotion)
	{
		Context.RootMotionMovementParams.Accumulate(ExtractRootMotion(PreviousTime, MoveDelta, Instance.bLooping));
	}
}

FTransform UAnimSequence::ExtractRootTrackTransform(float Pos, const FBoneContainer * RequiredBones) const
{
	const TArray<FTrackToSkeletonMap> & TrackToSkeletonMap = bUseRawDataOnly ? TrackToSkeletonMapTable : CompressedTrackToSkeletonMapTable;

	// we assume root is in first data if available = SkeletonIndex == 0 && BoneTreeIndex == 0)
	if ((TrackToSkeletonMap.Num() > 0) && (TrackToSkeletonMap[0].BoneTreeIndex == 0))
	{
		// if we do have root data, then return root data
		FTransform RootTransform;
		GetBoneTransform(RootTransform, 0, Pos, bUseRawDataOnly);
		return RootTransform;
	}

	// Fallback to root bone from reference skeleton.
	if( RequiredBones )
	{
		const FReferenceSkeleton& RefSkeleton = RequiredBones->GetReferenceSkeleton();
		if( RefSkeleton.GetNum() > 0 )
		{
			return RefSkeleton.GetRefBonePose()[0];
		}
	}

	USkeleton * MySkeleton = GetSkeleton();
	// If we don't have a RequiredBones array, get root bone from default skeleton.
	if( !RequiredBones &&  MySkeleton )
	{
		const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
		if( RefSkeleton.GetNum() > 0 )
		{
			return RefSkeleton.GetRefBonePose()[0];
		}
	}

	// Otherwise, use identity.
	return FTransform::Identity;
}

FTransform UAnimSequence::ExtractRootMotion(float StartTime, float DeltaTime, bool bAllowLooping) const
{
	FRootMotionMovementParams RootMotionParams;

	if (DeltaTime != 0.f)
	{
		bool const bPlayingBackwards = (DeltaTime < 0.f);

		float PreviousPosition = StartTime;
		float CurrentPosition = StartTime;
		float DesiredDeltaMove = DeltaTime;

		do
		{
			// Disable looping here. Advance to desired position, or beginning / end of animation 
			const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, SequenceLength);

			// Verify position assumptions
			ensureMsgf(bPlayingBackwards ? (CurrentPosition <= PreviousPosition) : (CurrentPosition >= PreviousPosition), TEXT("in Animation %s(Skeleton %s) : bPlayingBackwards(%d), PreviousPosition(%0.2f), Current Position(%0.2f)"),
				*GetName(), *GetNameSafe(GetSkeleton()), bPlayingBackwards, PreviousPosition, CurrentPosition);

			RootMotionParams.Accumulate(ExtractRootMotionFromRange(PreviousPosition, CurrentPosition));

			// If we've hit the end of the animation, and we're allowed to loop, keep going.
			if ((AdvanceType == ETAA_Finished) && bAllowLooping)
			{
				const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
				DesiredDeltaMove -= ActualDeltaMove;

				PreviousPosition = bPlayingBackwards ? SequenceLength : 0.f;
				CurrentPosition = PreviousPosition;
			}
			else
			{
				break;
			}
		} while (true);
	}

	return RootMotionParams.GetRootMotionTransform();
}

FTransform UAnimSequence::ExtractRootMotionFromRange(float StartTrackPosition, float EndTrackPosition) const
{
	const FVector DefaultScale(1.f);

	FTransform InitialTransform = ExtractRootTrackTransform(0.f, NULL);
	FTransform StartTransform = ExtractRootTrackTransform(StartTrackPosition, NULL);
	FTransform EndTransform = ExtractRootTrackTransform(EndTrackPosition, NULL);

	if(IsValidAdditive())
	{
		StartTransform.SetScale3D(StartTransform.GetScale3D() + DefaultScale);
		EndTransform.SetScale3D(EndTransform.GetScale3D() + DefaultScale);
	}

	// Transform to Component Space Rotation (inverse root transform from first frame)
	const FTransform RootToComponentRot = FTransform(InitialTransform.GetRotation().Inverse());
	StartTransform = RootToComponentRot * StartTransform;
	EndTransform = RootToComponentRot * EndTransform;

	return EndTransform.GetRelativeTransform(StartTransform);
}

#ifdef WITH_EDITOR
TArray<const UAnimSequence*> CurrentBakingAnims;
#endif

#define DEBUG_POSE_OUTPUT 0

#if DEBUG_POSE_OUTPUT
void DebugPrintBone(const FCompactPose& OutPose, const FCompactPoseBoneIndex& BoneIndex, int32 OutIndent)
{
	for (int i = 0; i < OutIndent; ++i)
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("  "));
	}
	const FBoneContainer& Cont = OutPose.GetBoneContainer();

	FName BoneName = Cont.GetReferenceSkeleton().GetBoneName(Cont.MakeMeshPoseIndex(BoneIndex).GetInt());

	FVector T = OutPose[BoneIndex].GetTranslation();

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s - (%.2f, %.2f,%.2f)\n"), *BoneName.ToString(), T.X, T.Y, T.Z);
}
#endif

void UAnimSequence::GetAnimationPose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_GetAnimationPose);

	// @todo anim: if compressed and baked in the future, we don't have to do this 
	if (UseRawDataForPoseExtraction(OutPose.GetBoneContainer()) && IsValidAdditive())
	{
		if (AdditiveAnimType == AAT_LocalSpaceBase)
		{
			GetBonePose_Additive(OutPose, OutCurve, ExtractionContext);
		}
		else if (AdditiveAnimType == AAT_RotationOffsetMeshSpace)
		{
			GetBonePose_AdditiveMeshRotationOnly(OutPose, OutCurve, ExtractionContext);
		}
	}
	else
	{
		GetBonePose(OutPose, OutCurve, ExtractionContext);
	}

	// Check that all bone atoms coming from animation are normalized
#if DO_CHECK && WITH_EDITORONLY_DATA
	check(OutPose.IsNormalized());
#endif

#if DEBUG_POSE_OUTPUT
	TArray<TArray<int32>> ParentLevel;
	ParentLevel.Reserve(64);
	for (int32 i = 0; i < 64; ++i)
	{
		ParentLevel.Add(TArray<int32>());
	}
	ParentLevel[0].Add(0);

	FPlatformMisc::LowLevelOutputDebugString(TEXT("\nGetAnimationPose\n"));
	
	DebugPrintBone(OutPose, FCompactPoseBoneIndex(0), 0);
	for (FCompactPoseBoneIndex BoneIndex(1); BoneIndex < OutPose.GetNumBones(); ++BoneIndex)
	{
		FCompactPoseBoneIndex ParentIndex = OutPose.GetBoneContainer().GetParentBoneIndex(BoneIndex);
		int32 Indent = 0;
		for (; Indent < ParentLevel.Num(); ++Indent)
		{
			if (ParentLevel[Indent].Contains(ParentIndex.GetInt()))
			{
				break;
			}
		}
		Indent += 1;
		check(Indent < 64);
		ParentLevel[Indent].Add(BoneIndex.GetInt());

		DebugPrintBone(OutPose, BoneIndex, Indent);
	}
#endif
}

void UAnimSequence::ResetRootBoneForRootMotion(FTransform& BoneTransform, const FBoneContainer& RequiredBones, ERootMotionRootLock::Type InRootMotionRootLock) const
{
	switch (InRootMotionRootLock)
	{
		case ERootMotionRootLock::AnimFirstFrame: BoneTransform = ExtractRootTrackTransform(0.f, &RequiredBones); break;
		case ERootMotionRootLock::Zero: BoneTransform = FTransform::Identity; break;
		default:
		case ERootMotionRootLock::RefPose: BoneTransform = RequiredBones.GetRefPoseArray()[0]; break;
	}

	if (IsValidAdditive() && InRootMotionRootLock != ERootMotionRootLock::AnimFirstFrame)
	{
		//Need to remove default scale here for additives
		BoneTransform.SetScale3D(BoneTransform.GetScale3D() - FVector(1.f));
	}
}

struct FRetargetTracking
{
	const FCompactPoseBoneIndex PoseBoneIndex;
	const int32 SkeletonBoneIndex;

	FRetargetTracking(const FCompactPoseBoneIndex InPoseBoneIndex, const int32 InSkeletonBoneIndex)
		: PoseBoneIndex(InPoseBoneIndex), SkeletonBoneIndex(InSkeletonBoneIndex) 
	{
	}
};

struct FGetBonePoseScratchArea : public TThreadSingleton<FGetBonePoseScratchArea>
{
	BoneTrackArray RotationScalePairs;
	BoneTrackArray TranslationPairs;
	BoneTrackArray AnimScaleRetargetingPairs;
	BoneTrackArray AnimRelativeRetargetingPairs;
	TArray<FRetargetTracking> RetargetTracking;
	TArray<FVirtualBoneCompactPoseData> VirtualBoneCompactPoseData;
};

void UAnimSequence::GetBonePose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext, bool bForceUseRawData) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimSeq_GetBonePose);

	const FBoneContainer& RequiredBones = OutPose.GetBoneContainer();
	const bool bUseRawDataForPoseExtraction = bForceUseRawData || UseRawDataForPoseExtraction(RequiredBones);

	const bool bIsBakedAdditive = !bUseRawDataForPoseExtraction && IsValidAdditive();

	const USkeleton* MySkeleton = GetSkeleton();
	if (!MySkeleton)
	{
		if (bIsBakedAdditive)
		{
			OutPose.ResetToAdditiveIdentity(); 
		}
		else
		{
			OutPose.ResetToRefPose();
		}
		return;
	}

	const bool bDisableRetargeting = RequiredBones.GetDisableRetargeting();

	// initialize with ref-pose
	if (bIsBakedAdditive)
	{
		//When using baked additive ref pose is identity
		OutPose.ResetToAdditiveIdentity();
	}
	else
	{
		// if retargeting is disabled, we initialize pose with 'Retargeting Source' ref pose.
		if (bDisableRetargeting)
		{
			TArray<FTransform> const& AuthoredOnRefSkeleton = MySkeleton->GetRefLocalPoses(RetargetSource);
			TArray<FBoneIndexType> const& RequireBonesIndexArray = RequiredBones.GetBoneIndicesArray();

			int32 const NumRequiredBones = RequireBonesIndexArray.Num();
			for (FCompactPoseBoneIndex PoseBoneIndex : OutPose.ForEachBoneIndex())
			{
				int32 const& SkeletonBoneIndex = RequiredBones.GetSkeletonIndex(PoseBoneIndex);

				// Pose bone index should always exist in Skeleton
				checkSlow(SkeletonBoneIndex != INDEX_NONE);
				OutPose[PoseBoneIndex] = AuthoredOnRefSkeleton[SkeletonBoneIndex];
			}
		}
		else
		{
			OutPose.ResetToRefPose();
		}
	}

	// extract curve data . Even if no track, it can contain curve data
	EvaluateCurveData(OutCurve, ExtractionContext.CurrentTime, bUseRawDataForPoseExtraction);

	const int32 NumTracks = bUseRawDataForPoseExtraction ? TrackToSkeletonMapTable.Num() : CompressedTrackToSkeletonMapTable.Num();
	if (NumTracks == 0)
	{
		return;
	}

#if WITH_EDITOR
	// this happens only with editor data
	// Slower path for disable retargeting, that's only used in editor and for debugging.
	if (bUseRawDataForPoseExtraction)
	{
		const TArray<FRawAnimSequenceTrack>* AnimationData = NULL;

		if (RequiredBones.ShouldUseSourceData() && SourceRawAnimationData.Num() > 0)
		{
			AnimationData = &SourceRawAnimationData;
		}
		else
		{
			AnimationData = &RawAnimationData;
		}

		TArray<FRetargetTracking>& RetargetTracking = FGetBonePoseScratchArea::Get().RetargetTracking;
		RetargetTracking.Reset(NumTracks);

		TArray<FVirtualBoneCompactPoseData>& VBCompactPoseData = FGetBonePoseScratchArea::Get().VirtualBoneCompactPoseData;
		VBCompactPoseData = RequiredBones.GetVirtualBoneCompactPoseData();

		for (int32 TrackIndex = 0; TrackIndex < NumTracks; TrackIndex++)
		{
			const int32 SkeletonBoneIndex = GetSkeletonIndexFromRawDataTrackIndex(TrackIndex);
			// not sure it's safe to assume that SkeletonBoneIndex can never be INDEX_NONE
			if ((SkeletonBoneIndex != INDEX_NONE) && (SkeletonBoneIndex < MAX_BONES))
			{
				const FCompactPoseBoneIndex PoseBoneIndex = RequiredBones.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
				if (PoseBoneIndex != INDEX_NONE)
				{
					for (int32 Idx = 0; Idx < VBCompactPoseData.Num(); ++Idx)
					{
						FVirtualBoneCompactPoseData& VB = VBCompactPoseData[Idx];
						if (PoseBoneIndex == VB.VBIndex)
						{
							// Remove this bone as we have written data for it (false so we dont resize allocation)
							VBCompactPoseData.RemoveAtSwap(Idx, 1, false);
							break; //Modified TArray so must break here
						}
					}
					// extract animation
					ExtractBoneTransform(*AnimationData, OutPose[PoseBoneIndex], TrackIndex, ExtractionContext.CurrentTime);

					RetargetTracking.Add(FRetargetTracking(PoseBoneIndex, SkeletonBoneIndex));
				}
			}
		}

		//Build Virtual Bones
		if(VBCompactPoseData.Num() > 0)
		{
			FCSPose<FCompactPose> CSPose;
			CSPose.InitPose(OutPose);
	
			for (FVirtualBoneCompactPoseData& VB : VBCompactPoseData)
			{
				FTransform Source = CSPose.GetComponentSpaceTransform(VB.SourceIndex);
				FTransform Target = CSPose.GetComponentSpaceTransform(VB.TargetIndex);
				OutPose[VB.VBIndex] = Target.GetRelativeTransform(Source);
			}
		}

		if(!bDisableRetargeting)
		{
			for (const FRetargetTracking& RT : RetargetTracking)
			{
				RetargetBoneTransform(OutPose[RT.PoseBoneIndex], RT.SkeletonBoneIndex, RT.PoseBoneIndex, RequiredBones, bIsBakedAdditive);
			}
		}

		if ((ExtractionContext.bExtractRootMotion && bEnableRootMotion) || bForceRootLock)
		{
			ResetRootBoneForRootMotion(OutPose[FCompactPoseBoneIndex(0)], RequiredBones, RootMotionRootLock);
		}
		return;
	}
#endif // WITH_EDITOR

	TArray<int32> const& SkeletonToPoseBoneIndexArray = RequiredBones.GetSkeletonToPoseBoneIndexArray();

	BoneTrackArray& RotationScalePairs = FGetBonePoseScratchArea::Get().RotationScalePairs;
	BoneTrackArray& TranslationPairs = FGetBonePoseScratchArea::Get().TranslationPairs;
	BoneTrackArray& AnimScaleRetargetingPairs = FGetBonePoseScratchArea::Get().AnimScaleRetargetingPairs;
	BoneTrackArray& AnimRelativeRetargetingPairs = FGetBonePoseScratchArea::Get().AnimRelativeRetargetingPairs;

	// build a list of desired bones
	RotationScalePairs.Reset();
	TranslationPairs.Reset();
	AnimScaleRetargetingPairs.Reset();
	AnimRelativeRetargetingPairs.Reset();

	// Optimization: assuming first index is root bone. That should always be the case in Skeletons.
	checkSlow((SkeletonToPoseBoneIndexArray[0] == 0));
	// this is not guaranteed for AnimSequences though... If Root is not animated, Track will not exist.
	const bool bFirstTrackIsRootBone = (GetSkeletonIndexFromCompressedDataTrackIndex(0) == 0);

	{
		SCOPE_CYCLE_COUNTER(STAT_BuildAnimTrackPairs);

		// Handle root bone separately if it is track 0. so we start w/ Index 1.
		for (int32 TrackIndex = (bFirstTrackIsRootBone ? 1 : 0); TrackIndex < NumTracks; TrackIndex++)
		{
			const int32 SkeletonBoneIndex = GetSkeletonIndexFromCompressedDataTrackIndex(TrackIndex);
			// not sure it's safe to assume that SkeletonBoneIndex can never be INDEX_NONE
			if (SkeletonBoneIndex != INDEX_NONE)
			{
				const FCompactPoseBoneIndex BoneIndex = RequiredBones.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
				//Nasty, we break our type safety, code in the lower levels should be adjusted for this
				const int32 CompactPoseBoneIndex = BoneIndex.GetInt();
				if (CompactPoseBoneIndex != INDEX_NONE)
				{
					RotationScalePairs.Add(BoneTrackPair(CompactPoseBoneIndex, TrackIndex));

					// Skip extracting translation component for EBoneTranslationRetargetingMode::Skeleton.
					switch (MySkeleton->GetBoneTranslationRetargetingMode(SkeletonBoneIndex))
					{
					case EBoneTranslationRetargetingMode::Animation:
						TranslationPairs.Add(BoneTrackPair(CompactPoseBoneIndex, TrackIndex));
						break;
					case EBoneTranslationRetargetingMode::AnimationScaled:
						TranslationPairs.Add(BoneTrackPair(CompactPoseBoneIndex, TrackIndex));
						AnimScaleRetargetingPairs.Add(BoneTrackPair(CompactPoseBoneIndex, SkeletonBoneIndex));
						break;
					case EBoneTranslationRetargetingMode::AnimationRelative:
						TranslationPairs.Add(BoneTrackPair(CompactPoseBoneIndex, TrackIndex));

						// With baked additives, we can skip 'AnimationRelative' tracks, as the relative transform gets canceled out.
						// (A1 + Rel) - (A2 + Rel) = A1 - A2.
						if (!bIsBakedAdditive)
						{
							AnimRelativeRetargetingPairs.Add(BoneTrackPair(CompactPoseBoneIndex, SkeletonBoneIndex));
						}
						break;
					}
				}
			}
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ExtractPoseFromAnimData);
		// Handle Root Bone separately
		if (bFirstTrackIsRootBone)
		{
			const int32 TrackIndex = 0;
			FCompactPoseBoneIndex RootBone(0);
			FTransform& RootAtom = OutPose[RootBone];

			AnimationFormat_GetBoneAtom(
				RootAtom,
				*this,
				TrackIndex,
				ExtractionContext.CurrentTime);

			// @laurent - we should look into splitting rotation and translation tracks, so we don't have to process translation twice.
			RetargetBoneTransform(RootAtom, 0, RootBone, RequiredBones, bIsBakedAdditive);
		}

		if (RotationScalePairs.Num() > 0)
		{
			// get the remaining bone atoms
			OutPose.PopulateFromAnimation( //@TODO:@ANIMATION: Nasty hack, be good to not have this function on the pose
				*this,
				RotationScalePairs,
				TranslationPairs,
				RotationScalePairs,
				ExtractionContext.CurrentTime);

			/*AnimationFormat_GetAnimationPose(
				*((FTransformArray*)&OutAtoms), 
				RotationScalePairs,
				TranslationPairs,
				RotationScalePairs,
				*this,
				ExtractionContext.CurrentTime);*/
		}
	}

	// Once pose has been extracted, snap root bone back to first frame if we are extracting root motion.
	if ((ExtractionContext.bExtractRootMotion && bEnableRootMotion) || bForceRootLock)
	{
		ResetRootBoneForRootMotion(OutPose[FCompactPoseBoneIndex(0)], RequiredBones, RootMotionRootLock);
	}

	// Anim Scale Retargeting
	int32 const NumBonesToScaleRetarget = AnimScaleRetargetingPairs.Num();
	if (NumBonesToScaleRetarget > 0)
	{
		TArray<FTransform> const& AuthoredOnRefSkeleton = MySkeleton->GetRefLocalPoses(RetargetSource);

		for (const BoneTrackPair& BonePair : AnimScaleRetargetingPairs)
		{
			const FCompactPoseBoneIndex BoneIndex(BonePair.AtomIndex); //Nasty, we break our type safety, code in the lower levels should be adjusted for this
			int32 const& SkeletonBoneIndex = BonePair.TrackIndex;

			// @todo - precache that in FBoneContainer when we have SkeletonIndex->TrackIndex mapping. So we can just apply scale right away.
			float const SourceTranslationLength = AuthoredOnRefSkeleton[SkeletonBoneIndex].GetTranslation().Size();
			if (SourceTranslationLength > KINDA_SMALL_NUMBER)
			{
				float const TargetTranslationLength = RequiredBones.GetRefPoseTransform(BoneIndex).GetTranslation().Size();
				OutPose[BoneIndex].ScaleTranslation(TargetTranslationLength / SourceTranslationLength);
			}
		}
	}

	// Anim Relative Retargeting
	int32 const NumBonesToRelativeRetarget = AnimRelativeRetargetingPairs.Num();
	if (NumBonesToRelativeRetarget > 0)
	{
		TArray<FTransform> const& AuthoredOnRefSkeleton = MySkeleton->GetRefLocalPoses(RetargetSource);
	
		for (const BoneTrackPair& BonePair : AnimRelativeRetargetingPairs)
		{
			const FCompactPoseBoneIndex BoneIndex(BonePair.AtomIndex); //Nasty, we break our type safety, code in the lower levels should be adjusted for this
			int32 const& SkeletonBoneIndex = BonePair.TrackIndex;

			const FTransform& RefPose = RequiredBones.GetRefPoseTransform(BoneIndex);

			// Apply the retargeting as if it were an additive difference between the current skeleton and the retarget skeleton. 
			OutPose[BoneIndex].SetRotation(OutPose[BoneIndex].GetRotation() * AuthoredOnRefSkeleton[SkeletonBoneIndex].GetRotation().Inverse() * RefPose.GetRotation());
			OutPose[BoneIndex].SetTranslation(OutPose[BoneIndex].GetTranslation() + (RefPose.GetTranslation() - AuthoredOnRefSkeleton[SkeletonBoneIndex].GetTranslation()));
			OutPose[BoneIndex].SetScale3D(OutPose[BoneIndex].GetScale3D() * (RefPose.GetScale3D() * AuthoredOnRefSkeleton[SkeletonBoneIndex].GetSafeScaleReciprocal(AuthoredOnRefSkeleton[SkeletonBoneIndex].GetScale3D())));
			OutPose[BoneIndex].NormalizeRotation();
		}
	}
}

#if WITH_EDITORONLY_DATA
int32 UAnimSequence::AddNewRawTrack(FName TrackName, FRawAnimSequenceTrack* TrackData)
{
	const int32 SkeletonIndex = GetSkeleton() ? GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(TrackName) : INDEX_NONE;

	if(SkeletonIndex != INDEX_NONE)
	{
		int32 TrackIndex = AnimationTrackNames.IndexOfByKey(TrackName);
		if (TrackIndex != INDEX_NONE)
		{
			if (TrackData)
			{
				RawAnimationData[TrackIndex] = *TrackData;
			}
			return TrackIndex;
		}

		check(AnimationTrackNames.Num() == RawAnimationData.Num());
		TrackIndex = AnimationTrackNames.Add(TrackName);
		TrackToSkeletonMapTable.Add(FTrackToSkeletonMap(SkeletonIndex));
		if (TrackData)
		{
			RawAnimationData.Add(*TrackData);
		}
		else
		{
			RawAnimationData.Add(FRawAnimSequenceTrack());
		}
		return TrackIndex;
	}
	return INDEX_NONE;
}
#endif

void UAnimSequence::GetBonePose_Additive(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const
{
	if (!IsValidAdditive())
	{
		OutPose.ResetToAdditiveIdentity();
		return;
	}

	// Extract target pose
	GetBonePose(OutPose, OutCurve, ExtractionContext);

	// Extract base pose
	FCompactPose BasePose;
	FBlendedCurve BaseCurve;
	
	BasePose.SetBoneContainer(&OutPose.GetBoneContainer());
	BaseCurve.InitFrom(OutCurve);	

	GetAdditiveBasePose(BasePose, BaseCurve, ExtractionContext);

	// Create Additive animation
	FAnimationRuntime::ConvertPoseToAdditive(OutPose, BasePose);
	OutCurve.ConvertToAdditive(BaseCurve);
}

void UAnimSequence::GetAdditiveBasePose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const
{
	const bool bGetAdditiveBasePoseValid = (RefPoseType == ABPT_RefPose || !RefPoseSeq->IsValidAdditive() || RefPoseSeq->RawAnimationData.Num() > 0);
	if (!bGetAdditiveBasePoseValid) //If this fails there is not enough information to get the base pose
	{
		FString Name = GetName();
		FString SkelName = GetSkeleton() ? GetSkeleton()->GetName() : TEXT("NoSkeleton");
		FString RefName = RefPoseSeq->GetName();
		FString RefSkelName = RefPoseSeq->GetSkeleton() ? RefPoseSeq->GetSkeleton()->GetName() : TEXT("NoRefSeqSkeleton");
		const TCHAR* NeedsLoad = RefPoseSeq->HasAnyFlags(RF_NeedLoad) ? TEXT("Yes") : TEXT("No");

		checkf(false, TEXT("Cannot get valid base pose for Anim: ['%s' (Skel:%s)] RefSeq: ['%s' (Skel:%s)] RawAnimDataNum: %i NeedsLoad: %s"), *Name, *SkelName, *RefName, *RefSkelName, RefPoseSeq->RawAnimationData.Num(), NeedsLoad);
	}

	switch (RefPoseType)
	{
		// use whole animation as a base pose. Need BasePoseSeq.
		case ABPT_AnimScaled:
		{
			// normalize time to fit base seq
			const float Fraction = FMath::Clamp<float>(ExtractionContext.CurrentTime / SequenceLength, 0.f, 1.f);
			const float BasePoseTime = RefPoseSeq->SequenceLength * Fraction;

			FAnimExtractContext BasePoseExtractionContext(ExtractionContext);
			BasePoseExtractionContext.CurrentTime = BasePoseTime;
			RefPoseSeq->GetBonePose(OutPose, OutCurve, BasePoseExtractionContext, true);
			break;
		}
		// use animation as a base pose. Need BasePoseSeq and RefFrameIndex (will clamp if outside).
		case ABPT_AnimFrame:
		{
			const float Fraction = (RefPoseSeq->NumFrames > 0) ? FMath::Clamp<float>((float)RefFrameIndex / (float)RefPoseSeq->NumFrames, 0.f, 1.f) : 0.f;
			const float BasePoseTime = RefPoseSeq->SequenceLength * Fraction;

			FAnimExtractContext BasePoseExtractionContext(ExtractionContext);
			BasePoseExtractionContext.CurrentTime = BasePoseTime;
			RefPoseSeq->GetBonePose(OutPose, OutCurve, BasePoseExtractionContext, true);
			break;
		}
		// use ref pose of Skeleton as base
		case ABPT_RefPose:
		default:
			OutPose.ResetToRefPose();
			break;
	}

}

void UAnimSequence::GetBonePose_AdditiveMeshRotationOnly(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const
{
	if (!IsValidAdditive())
	{
		// since this is additive, need to initialize to identity
		OutPose.ResetToAdditiveIdentity();
		return;
	}

	// Get target pose
	GetBonePose(OutPose, OutCurve, ExtractionContext, true);

	// get base pose
	FCompactPose BasePose;
	FBlendedCurve BaseCurve;
	BasePose.SetBoneContainer(&OutPose.GetBoneContainer());
	BaseCurve.InitFrom(OutCurve);
	GetAdditiveBasePose(BasePose, BaseCurve, ExtractionContext);

	// Convert them to mesh rotation.
	FAnimationRuntime::ConvertPoseToMeshRotation(OutPose);
	FAnimationRuntime::ConvertPoseToMeshRotation(BasePose);

	// Turn into Additive
	FAnimationRuntime::ConvertPoseToAdditive(OutPose, BasePose);
	OutCurve.ConvertToAdditive(BaseCurve);
}

void UAnimSequence::RetargetBoneTransform(FTransform& BoneTransform, const int32 SkeletonBoneIndex, const FCompactPoseBoneIndex& BoneIndex, const FBoneContainer& RequiredBones, const bool bIsBakedAdditive) const
{
	const USkeleton* MySkeleton = GetSkeleton();
	FAnimationRuntime::RetargetBoneTransform(MySkeleton, RetargetSource, BoneTransform, SkeletonBoneIndex, BoneIndex, RequiredBones, bIsBakedAdditive);
}

#if WITH_EDITOR
/** Utility function to crop data from a RawAnimSequenceTrack */
static int32 CropRawTrack(FRawAnimSequenceTrack& RawTrack, int32 StartKey, int32 NumKeys, int32 TotalNumOfFrames)
{
	check(RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == TotalNumOfFrames);
	check(RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == TotalNumOfFrames);
	// scale key can be empty
	check(RawTrack.ScaleKeys.Num() == 0 || RawTrack.ScaleKeys.Num() == 1 || RawTrack.ScaleKeys.Num() == TotalNumOfFrames);

	if( RawTrack.PosKeys.Num() > 1 )
	{
		RawTrack.PosKeys.RemoveAt(StartKey, NumKeys);
		check(RawTrack.PosKeys.Num() > 0);
		RawTrack.PosKeys.Shrink();
	}

	if( RawTrack.RotKeys.Num() > 1 )
	{
		RawTrack.RotKeys.RemoveAt(StartKey, NumKeys);
		check(RawTrack.RotKeys.Num() > 0);
		RawTrack.RotKeys.Shrink();
	}

	if( RawTrack.ScaleKeys.Num() > 1 )
	{
		RawTrack.ScaleKeys.RemoveAt(StartKey, NumKeys);
		check(RawTrack.ScaleKeys.Num() > 0);
		RawTrack.ScaleKeys.Shrink();
	}

	// Update NumFrames below to reflect actual number of keys.
	return FMath::Max<int32>( RawTrack.PosKeys.Num(), FMath::Max<int32>(RawTrack.RotKeys.Num(), RawTrack.ScaleKeys.Num()) );
}

void UAnimSequence::ResizeSequence(float NewLength, int32 NewNumFrames, bool bInsert, int32 StartFrame/*inclusive */, int32 EndFrame/*inclusive*/)
{
	check (NewNumFrames > 0);
	check (StartFrame < EndFrame);

	int32 OldNumFrames = NumFrames;
	float OldSequenceLength = SequenceLength;

	// verify condition
	NumFrames = NewNumFrames;
	// Update sequence length to match new number of frames.
	SequenceLength = NewLength;

	float Interval = OldSequenceLength / OldNumFrames;
	ensure (Interval == SequenceLength/NumFrames);

	float OldStartTime = StartFrame * Interval;
	float OldEndTime = EndFrame * Interval;
	float Duration = OldEndTime - OldStartTime;

	// re-locate notifies
	for (auto& Notify: Notifies)
	{
		float CurrentTime = Notify.GetTime();
		float NewDuration = 0.f;
		if (bInsert)
		{
			// if state, make sure to adjust end time
			if(Notify.NotifyStateClass)
			{
				float NotifyDuration = Notify.GetDuration();
				float NotifyEnd = CurrentTime + NotifyDuration;
				if(NotifyEnd >= OldStartTime)
				{
					NewDuration = NotifyDuration + Duration;
				}
				else
				{
					NewDuration = NotifyDuration;
				}
			}

			// when insert, we only care about start time
			// if it's later than start time
			if (CurrentTime >= OldStartTime)
			{
				CurrentTime += Duration;
			}
		}
		else
		{
			// if state, make sure to adjust end time
			if(Notify.NotifyStateClass)
			{
				float NotifyDuration = Notify.GetDuration();
				float NotifyEnd = CurrentTime + NotifyDuration;
				NewDuration = NotifyDuration;
				if(NotifyEnd >= OldStartTime && NotifyEnd <= OldEndTime)
				{
					// small number @todo see if there is define for this
					NewDuration = 0.1f;
				}
				else if (NotifyEnd > OldEndTime)
				{
					NewDuration = NotifyEnd-Duration-CurrentTime;
				}
				else
				{
					NewDuration = NotifyDuration;
				}

				NewDuration = FMath::Max(NewDuration, 0.1f);
			}

			if (CurrentTime >= OldStartTime && CurrentTime <= OldEndTime)
			{
				CurrentTime = OldStartTime;
			}
			else if (CurrentTime > OldEndTime)
			{
				CurrentTime -= Duration;
			}
		}

		float ClampedCurrentTime = FMath::Clamp(CurrentTime, 0.f, SequenceLength);
		Notify.LinkSequence(this, ClampedCurrentTime);
		Notify.SetDuration(NewDuration);

		if (ClampedCurrentTime == 0.f)
		{
			Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetAfter);
		}
		else if (ClampedCurrentTime == SequenceLength)
		{
			Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
		}
	}

	for (FAnimSyncMarker& Marker : AuthoredSyncMarkers)
	{
		float CurrentTime = Marker.Time;
		if (bInsert)
		{
			// when insert, we only care about start time
			// if it's later than start time
			if (CurrentTime >= OldStartTime)
			{
				CurrentTime += Duration;
			}
		}
		else
		{
			if (CurrentTime >= OldStartTime && CurrentTime <= OldEndTime)
			{
				CurrentTime = OldStartTime;
			}
			else if (CurrentTime > OldEndTime)
			{
				CurrentTime -= Duration;
			}
		}
		Marker.Time = FMath::Clamp(CurrentTime, 0.f, SequenceLength);
	}
	// resize curves
	RawCurveData.Resize(NewLength, bInsert, OldStartTime, OldEndTime);
}

bool UAnimSequence::InsertFramesToRawAnimData( int32 StartFrame, int32 EndFrame, int32 CopyFrame)
{
	// make sure the copyframe is valid and start frame is valid
	int32 NumFramesToInsert = EndFrame-StartFrame;
	if ((CopyFrame>=0 && CopyFrame<NumFrames) && (StartFrame >= 0 && StartFrame <=NumFrames) && NumFramesToInsert > 0)
	{
		for (auto& RawData : RawAnimationData)
		{
			if (RawData.PosKeys.Num() > 1 && RawData.PosKeys.IsValidIndex(CopyFrame))
			{
				auto Source = RawData.PosKeys[CopyFrame];
				RawData.PosKeys.InsertZeroed(StartFrame, NumFramesToInsert);
				for (int32 Index=StartFrame; Index<EndFrame; ++Index)
				{
					RawData.PosKeys[Index] = Source;
				}
			}

			if(RawData.RotKeys.Num() > 1 && RawData.RotKeys.IsValidIndex(CopyFrame))
			{
				auto Source = RawData.RotKeys[CopyFrame];
				RawData.RotKeys.InsertZeroed(StartFrame, NumFramesToInsert);
				for(int32 Index=StartFrame; Index<EndFrame; ++Index)
				{
					RawData.RotKeys[Index] = Source;
				}
			}

			if(RawData.ScaleKeys.Num() > 1 && RawData.ScaleKeys.IsValidIndex(CopyFrame))
			{
				auto Source = RawData.ScaleKeys[CopyFrame];
				RawData.ScaleKeys.InsertZeroed(StartFrame, NumFramesToInsert);

				for(int32 Index=StartFrame; Index<EndFrame; ++Index)
				{
					RawData.ScaleKeys[Index] = Source;
				}
			}
		}

		float const FrameTime = SequenceLength / ((float)NumFrames);

		int32 NewNumFrames = NumFrames + NumFramesToInsert;
		ResizeSequence((float)NewNumFrames * FrameTime, NewNumFrames, true, StartFrame, EndFrame);

		UE_LOG(LogAnimation, Log, TEXT("\tSequenceLength: %f, NumFrames: %d"), SequenceLength, NumFrames);
		
		MarkRawDataAsModified();
		MarkPackageDirty();

		return true;
	}

	return false;
}

bool UAnimSequence::CropRawAnimData( float CurrentTime, bool bFromStart )
{
	// Length of one frame.
	float const FrameTime = SequenceLength / ((float)NumFrames);
	// Save Total Number of Frames before crop
	int32 TotalNumOfFrames = NumFrames;

	// if current frame is 1, do not try crop. There is nothing to crop
	if ( NumFrames <= 1 )
	{
		return false;
	}
	
	// If you're end or beginning, you can't cut all nor nothing. 
	// Avoiding ambiguous situation what exactly we would like to cut 
	// Below it clamps range to 1, TotalNumOfFrames-1
	// causing if you were in below position, it will still crop 1 frame. 
	// To be clearer, it seems better if we reject those inputs. 
	// If you're a bit before/after, we assume that you'd like to crop
	if ( CurrentTime == 0.f || CurrentTime == SequenceLength )
	{
		return false;
	}

	// Find the right key to cut at.
	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	// The reason I'm changing to TotalNumOfFrames is CT/SL = KeyIndexWithFraction/TotalNumOfFrames
	// To play TotalNumOfFrames, it takes SequenceLength. Each key will take SequenceLength/TotalNumOfFrames
	float const KeyIndexWithFraction = (CurrentTime * (float)(TotalNumOfFrames)) / SequenceLength;
	int32 KeyIndex = bFromStart ? FMath::FloorToInt(KeyIndexWithFraction) : FMath::CeilToInt(KeyIndexWithFraction);
	// Ensure KeyIndex is in range.
	KeyIndex = FMath::Clamp<int32>(KeyIndex, 1, TotalNumOfFrames-1); 
	// determine which keys need to be removed.
	int32 const StartKey = bFromStart ? 0 : KeyIndex;
	int32 const NumKeys = bFromStart ? KeyIndex : TotalNumOfFrames - KeyIndex ;

	// Recalculate NumFrames
	int32 NewNumFrames = TotalNumOfFrames - NumKeys;

	UE_LOG(LogAnimation, Log, TEXT("UAnimSequence::CropRawAnimData %s - CurrentTime: %f, bFromStart: %d, TotalNumOfFrames: %d, KeyIndex: %d, StartKey: %d, NumKeys: %d"), *GetName(), CurrentTime, bFromStart, TotalNumOfFrames, KeyIndex, StartKey, NumKeys);

	// Iterate over tracks removing keys from each one.
	for(int32 i=0; i<RawAnimationData.Num(); i++)
	{
		// Update NewNumFrames below to reflect actual number of keys while we crop the anim data
		CropRawTrack(RawAnimationData[i], StartKey, NumKeys, TotalNumOfFrames);
	}

	// Double check that everything is fine
	for(int32 i=0; i<RawAnimationData.Num(); i++)
	{
		FRawAnimSequenceTrack& RawTrack = RawAnimationData[i];
		check(RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NewNumFrames);
		check(RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NewNumFrames);
	}

	// Update sequence length to match new number of frames.
	ResizeSequence((float)NewNumFrames * FrameTime, NewNumFrames, false, StartKey, StartKey+NumKeys);

	UE_LOG(LogAnimation, Log, TEXT("\tSequenceLength: %f, NumFrames: %d"), SequenceLength, NumFrames);

	MarkRawDataAsModified();
	OnRawDataChanged();
	MarkPackageDirty();
	return true;
}

bool UAnimSequence::CompressRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack, float MaxPosDiff, float MaxAngleDiff)
{
	bool bRemovedKeys = false;

	// First part is to make sure we have valid input
	bool const bPosTrackIsValid = (RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NumFrames);
	if( !bPosTrackIsValid )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Found non valid position track for %s, %d frames, instead of %d. Chopping!"), *GetName(), RawTrack.PosKeys.Num(), NumFrames);
		bRemovedKeys = true;
		RawTrack.PosKeys.RemoveAt(1, RawTrack.PosKeys.Num()- 1);
		RawTrack.PosKeys.Shrink();
		check( RawTrack.PosKeys.Num() == 1);
	}

	bool const bRotTrackIsValid = (RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NumFrames);
	if( !bRotTrackIsValid )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Found non valid rotation track for %s, %d frames, instead of %d. Chopping!"), *GetName(), RawTrack.RotKeys.Num(), NumFrames);
		bRemovedKeys = true;
		RawTrack.RotKeys.RemoveAt(1, RawTrack.RotKeys.Num()- 1);
		RawTrack.RotKeys.Shrink();
		check( RawTrack.RotKeys.Num() == 1);
	}

	// scale keys can be empty, and that is valid 
	bool const bScaleTrackIsValid = (RawTrack.ScaleKeys.Num() == 0 || RawTrack.ScaleKeys.Num() == 1 || RawTrack.ScaleKeys.Num() == NumFrames);
	if( !bScaleTrackIsValid )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Found non valid Scaleation track for %s, %d frames, instead of %d. Chopping!"), *GetName(), RawTrack.ScaleKeys.Num(), NumFrames);
		bRemovedKeys = true;
		RawTrack.ScaleKeys.RemoveAt(1, RawTrack.ScaleKeys.Num()- 1);
		RawTrack.ScaleKeys.Shrink();
		check( RawTrack.ScaleKeys.Num() == 1);
	}

	// Second part is actual compression.

	// Check variation of position keys
	if( (RawTrack.PosKeys.Num() > 1) && (MaxPosDiff >= 0.0f) )
	{
		FVector FirstPos = RawTrack.PosKeys[0];
		bool bFramesIdentical = true;
		for(int32 j=1; j<RawTrack.PosKeys.Num() && bFramesIdentical; j++)
		{
			if( (FirstPos - RawTrack.PosKeys[j]).SizeSquared() > FMath::Square(MaxPosDiff) )
			{
				bFramesIdentical = false;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = true;
			RawTrack.PosKeys.RemoveAt(1, RawTrack.PosKeys.Num()- 1);
			RawTrack.PosKeys.Shrink();
			check( RawTrack.PosKeys.Num() == 1);
		}
	}

	// Check variation of rotational keys
	if( (RawTrack.RotKeys.Num() > 1) && (MaxAngleDiff >= 0.0f) )
	{
		FQuat FirstRot = RawTrack.RotKeys[0];
		bool bFramesIdentical = true;
		for(int32 j=1; j<RawTrack.RotKeys.Num() && bFramesIdentical; j++)
		{
			if( FQuat::Error(FirstRot, RawTrack.RotKeys[j]) > MaxAngleDiff )
			{
				bFramesIdentical = false;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = true;
			RawTrack.RotKeys.RemoveAt(1, RawTrack.RotKeys.Num()- 1);
			RawTrack.RotKeys.Shrink();
			check( RawTrack.RotKeys.Num() == 1);
		}			
	}
	
	float MaxScaleDiff = 0.0001f;

	// Check variation of Scaleition keys
	if( (RawTrack.ScaleKeys.Num() > 1) && (MaxScaleDiff >= 0.0f) )
	{
		FVector FirstScale = RawTrack.ScaleKeys[0];
		bool bFramesIdentical = true;
		for(int32 j=1; j<RawTrack.ScaleKeys.Num() && bFramesIdentical; j++)
		{
			if( (FirstScale - RawTrack.ScaleKeys[j]).SizeSquared() > FMath::Square(MaxScaleDiff) )
			{
				bFramesIdentical = false;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = true;
			RawTrack.ScaleKeys.RemoveAt(1, RawTrack.ScaleKeys.Num()- 1);
			RawTrack.ScaleKeys.Shrink();
			check( RawTrack.ScaleKeys.Num() == 1);
		}
	}

	return bRemovedKeys;
}

bool UAnimSequence::CompressRawAnimData(float MaxPosDiff, float MaxAngleDiff)
{
	bool bRemovedKeys = false;
#if WITH_EDITORONLY_DATA

	if (AnimationTrackNames.Num() > 0 && ensureMsgf(RawAnimationData.Num() > 0, TEXT("%s is trying to compress while raw animation is missing"), *GetName()))
	{
		// This removes trivial keys, and this has to happen before the removing tracks
		for(int32 TrackIndex=0; TrackIndex<RawAnimationData.Num(); TrackIndex++)
		{
			bRemovedKeys |= CompressRawAnimSequenceTrack(RawAnimationData[TrackIndex], MaxPosDiff, MaxAngleDiff);
		}

		const USkeleton* MySkeleton = GetSkeleton();

		if(MySkeleton)
		{
			bool bCompressScaleKeys = false;
			// go through remove keys if not needed
			for(int32 TrackIndex=0; TrackIndex<RawAnimationData.Num(); TrackIndex++)
			{
				FRawAnimSequenceTrack const& RawData = RawAnimationData[TrackIndex];
				if(RawData.ScaleKeys.Num() > 0)
				{
					// if scale key exists, see if we can just empty it
					if((RawData.ScaleKeys.Num() > 1) || (RawData.ScaleKeys[0].Equals(FVector(1.f)) == false))
					{
						bCompressScaleKeys = true;
						break;
					}
				}
			}

			// if we don't have scale, we should delete all scale keys
			// if you have one track that has scale, we still should support scale, so compress scale
			if(!bCompressScaleKeys)
			{
				// then remove all scale keys
				for(int32 TrackIndex=0; TrackIndex<RawAnimationData.Num(); TrackIndex++)
				{
					FRawAnimSequenceTrack& RawData = RawAnimationData[TrackIndex];
					RawData.ScaleKeys.Empty();
				}
			}
		}

		CompressedTrackOffsets.Empty();
		CompressedScaleOffsets.Empty();
	}
	else
	{
		CompressedTrackOffsets.Empty();
		CompressedScaleOffsets.Empty();
	}
	
#endif
	return bRemovedKeys;
}

bool UAnimSequence::CompressRawAnimData()
{
	const float MaxPosDiff = 0.0001f;
	const float MaxAngleDiff = 0.0003f;
	return CompressRawAnimData(MaxPosDiff, MaxAngleDiff);
}

/** 
 * Flip Rotation W for the RawTrack
 */
void FlipRotationW(FRawAnimSequenceTrack& RawTrack)
{
	int32 TotalNumOfRotKey = RawTrack.RotKeys.Num();

	for (int32 I=0; I<TotalNumOfRotKey; ++I)
	{
		FQuat& RotKey = RawTrack.RotKeys[I];
		RotKey.W *= -1.f;
	}
}


void UAnimSequence::FlipRotationWForNonRoot(USkeletalMesh * SkelMesh)
{
	if (!GetSkeleton())
	{
		return;
	}

	// Now add additive animation to destination.
	for(int32 TrackIdx=0; TrackIdx<TrackToSkeletonMapTable.Num(); TrackIdx++)
	{
		// Figure out which bone this track is mapped to
		const int32 BoneIndex = TrackToSkeletonMapTable[TrackIdx].BoneTreeIndex;
		if ( BoneIndex > 0 )
		{
			FlipRotationW( RawAnimationData[TrackIdx] );

		}
	}

	// Apply compression
	MarkRawDataAsModified();
	OnRawDataChanged();
}
#endif 

void UAnimSequence::RequestAnimCompression(bool bAsyncCompression, bool bAllowAlternateCompressor, bool bOutput)
{
	TSharedPtr<FAnimCompressContext> CompressContext = MakeShareable(new FAnimCompressContext(bAllowAlternateCompressor, bOutput));
	RequestAnimCompression(bAsyncCompression, CompressContext);
}

void UAnimSequence::RequestAnimCompression(bool bAsyncCompression, TSharedPtr<FAnimCompressContext> CompressContext)
{
#if WITH_EDITOR
	USkeleton* CurrentSkeleton = GetSkeleton();
	if (CurrentSkeleton == nullptr)
	{
		bUseRawDataOnly = true;
		return;
	}

	if (FPlatformProperties::RequiresCookedData())
	{
		return;
	}

	if (!CompressionScheme)
	{
		CompressionScheme = FAnimationUtils::GetDefaultAnimationCompressionAlgorithm();
	}

	if (!RawDataGuid.IsValid())
	{
		RawDataGuid = GenerateGuidFromRawData();
	}

	bAsyncCompression = false; //Just get sync working first
	bUseRawDataOnly = true;

	TGuardValue<bool> CompressGuard(bCompressionInProgress, true);

	const bool bDoCompressionInPlace = FUObjectThreadContext::Get().IsRoutingPostLoad;

	// Need to make sure this is up to date.
	VerifyCurveNames<FFloatCurve>(*CurrentSkeleton, USkeleton::AnimCurveMappingName, RawCurveData.FloatCurves);

	if (bAsyncCompression)
	{
	}
	else
	{
		TArray<uint8> OutData;
		FDerivedDataAnimationCompression* AnimCompressor = new FDerivedDataAnimationCompression(this, CompressContext, bDoCompressionInPlace);
		// For debugging DDC/Compression issues		
		const bool bSkipDDC = false;
		if (bSkipDDC || (CompressCommandletVersion == INDEX_NONE))
		{
			AnimCompressor->Build(OutData);

			delete AnimCompressor;
			AnimCompressor = nullptr;
		}
		else
		{
			if (AnimCompressor->CanBuild())
			{
				GetDerivedDataCacheRef().GetSynchronous(AnimCompressor, OutData);
			}
			else
			{
				// If we dont perform compression we need to clean this up
				delete AnimCompressor;
				AnimCompressor = nullptr;
			}
		}

		if (bUseRawDataOnly && OutData.Num() > 0)
		{
			FMemoryReader MemAr(OutData);
			SerializeCompressedData(MemAr, true);
			//This is only safe during sync anim compression
			SetSkeletonVirtualBoneGuid(GetSkeleton()->GetVirtualBoneGuid());
			bUseRawDataOnly = false;
		}
	}
#endif
}

#if WITH_EDITOR
struct FAnimDDCDebugData
{
	FString FullName;
	uint8   AdditiveSetting;
	FString CompressionSchemeName;
	FGuid   RawDataGuid;

	FAnimDDCDebugData(const UAnimSequence* AnimSequence, FArchive& Ar)
	{
		if (Ar.IsSaving())
		{
			FullName = AnimSequence->GetFullName();
			AdditiveSetting = (uint8)AnimSequence->AdditiveAnimType.GetValue();
			CompressionSchemeName = AnimSequence->CompressionScheme->GetFullName();
			RawDataGuid = AnimSequence->GetRawDataGuid();
		}

		Ar << FullName;
		Ar << AdditiveSetting;
		Ar << CompressionSchemeName;
		Ar << RawDataGuid;
	}
};
#endif

void UAnimSequence::SerializeCompressedData(FArchive& Ar, bool bDDCData)
{
	Ar << KeyEncodingFormat;
	Ar << TranslationCompressionFormat;
	Ar << RotationCompressionFormat;
	Ar << ScaleCompressionFormat;

	Ar << CompressedTrackOffsets;
	Ar << CompressedScaleOffsets;

	Ar << CompressedTrackToSkeletonMapTable;
	Ar << CompressedCurveData;

	Ar << CompressedRawDataSize;

	if (Ar.IsLoading())
	{
		// Serialize the compressed byte stream from the archive to the buffer.
		int32 NumBytes;
		Ar << NumBytes;

		TArray<uint8> SerializedData;
		SerializedData.Empty(NumBytes);
		SerializedData.AddUninitialized(NumBytes);
		Ar.Serialize(SerializedData.GetData(), NumBytes);

		// Swap the buffer into the byte stream.
		FMemoryReader MemoryReader(SerializedData, true);
		MemoryReader.SetByteSwapping(Ar.ForceByteSwapping());

		// we must know the proper codecs to use
		AnimationFormat_SetInterfaceLinks(*this);

		// and then use the codecs to byte swap
		check(RotationCodec != NULL);
		((AnimEncoding*)RotationCodec)->ByteSwapIn(*this, MemoryReader);
	}
	else if (Ar.IsSaving() || Ar.IsCountingMemory())
	{
		// Swap the byte stream into a buffer.
		TArray<uint8> SerializedData;

		// we must know the proper codecs to use
		AnimationFormat_SetInterfaceLinks(*this);

		// and then use the codecs to byte swap
		check(RotationCodec != NULL);
		((AnimEncoding*)RotationCodec)->ByteSwapOut(*this, SerializedData, Ar.ForceByteSwapping());

		// Make sure the entire byte stream was serialized.
		//check( CompressedByteStream.Num() == SerializedData.Num() );

		// Serialize the buffer to archive.
		int32 Num = SerializedData.Num();
		Ar << Num;
		Ar.Serialize(SerializedData.GetData(), SerializedData.Num());

		// Count compressed data.
		Ar.CountBytes(SerializedData.Num(), SerializedData.Num());
	}

#if WITH_EDITOR
	if (bDDCData)
	{
		//Skip ddc debug data if we are cooking
		FAnimDDCDebugData DebugData(this, Ar);

		if (Ar.IsLoading())
		{
			if(USkeleton* CurrentSkeleton = GetSkeleton())
			{
				VerifyCurveNames<FFloatCurve>(*CurrentSkeleton, USkeleton::AnimCurveMappingName, CompressedCurveData.FloatCurves);
				bUseRawDataOnly = !IsCompressedDataValid();
				ensureMsgf(!bUseRawDataOnly, TEXT("Anim Compression failed for Sequence '%s' Guid:%s CompressedDebugData:\n\tOriginal Anim:%s\n\tAdditiveSetting:%i\n\tCompression Scheme:%s\n\tRawDataGuid:%s"),
					*GetFullName(),
					*RawDataGuid.ToString(),
					*DebugData.FullName,
					DebugData.AdditiveSetting,
					*DebugData.CompressionSchemeName,
					*DebugData.RawDataGuid.ToString());
			}
		}
	}
#endif
}

#if WITH_EDITOR

bool UAnimSequence::CanBakeAdditive() const
{
	return	(NumFrames > 0) &&
			IsValidAdditive() &&
			GetSkeleton();
}

FFloatCurve* GetFloatCurve(FRawCurveTracks& RawCurveTracks, USkeleton::AnimCurveUID& CurveUID)
{
	return static_cast<FFloatCurve *>(RawCurveTracks.GetCurveData(CurveUID, ERawCurveTrackTypes::RCT_Float));
}

bool IsNewKeyDifferent(const FRichCurveKey& LastKey, float NewValue)
{
	return LastKey.Value != NewValue;
}

template <typename ArrayType>
void UpdateSHAWithArray(FSHA1& Sha, const TArray<ArrayType>& Array)
{
	Sha.Update((uint8*)Array.GetData(), Array.Num() * Array.GetTypeSize());
}

void UpdateSHAWithRawTrack(FSHA1& Sha, const FRawAnimSequenceTrack& RawTrack)
{
	UpdateSHAWithArray(Sha, RawTrack.PosKeys);
	UpdateSHAWithArray(Sha, RawTrack.RotKeys);
	UpdateSHAWithArray(Sha, RawTrack.ScaleKeys);
}

template<class DataType>
void UpdateWithData(FSHA1& Sha, const DataType& Data)
{
	Sha.Update((uint8*)(&Data), sizeof(DataType));
}

void UAnimSequence::UpdateSHAWithCurves(FSHA1& Sha, const FRawCurveTracks& InRawCurveData) const
{
	for (const FFloatCurve& Curve : InRawCurveData.FloatCurves)
	{
		UpdateWithData(Sha, Curve.Name.UID);
		UpdateWithData(Sha, Curve.FloatCurve.DefaultValue);
		UpdateSHAWithArray(Sha, Curve.FloatCurve.GetConstRefOfKeys());
		UpdateWithData(Sha, Curve.FloatCurve.PreInfinityExtrap);
		UpdateWithData(Sha, Curve.FloatCurve.PostInfinityExtrap);
	}
}

bool UAnimSequence::DoesSequenceContainZeroScale()
{
	for (const FRawAnimSequenceTrack& RawTrack : RawAnimationData)
	{
		for (const FVector ScaleKey : RawTrack.ScaleKeys)
		{
			if (ScaleKey.IsZero())
			{
				return true;
			}
		}
	}

	return false;
}

FGuid UAnimSequence::GenerateGuidFromRawData() const
{
	FSHA1 Sha;

	for (const FRawAnimSequenceTrack& Track : RawAnimationData)
	{
		UpdateSHAWithRawTrack(Sha, Track);
	}

	UpdateSHAWithCurves(Sha, RawCurveData);

	Sha.Final();

	uint32 Hash[5];
	Sha.GetHash((uint8*)Hash);
	FGuid Guid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
	return Guid;
}

void CopyTransformToRawAnimationData(const FTransform& BoneTransform, FRawAnimSequenceTrack& Track, int32 Frame)
{
	Track.PosKeys[Frame] = BoneTransform.GetTranslation();
	Track.RotKeys[Frame] = BoneTransform.GetRotation();
	Track.RotKeys[Frame].Normalize();
	Track.ScaleKeys[Frame] = BoneTransform.GetScale3D();
}

struct FByFramePoseEvalContext
{
private:
	const UAnimSequence* AnimToEval;
	
public:
	FBoneContainer RequiredBones;

	// Length of one frame.
	const float IntervalTime;

	TArray<FBoneIndexType> RequiredBoneIndexArray;

	FByFramePoseEvalContext(const UAnimSequence* InAnimToEval)
		: AnimToEval(InAnimToEval)
		, IntervalTime(InAnimToEval->SequenceLength / ((float)InAnimToEval->NumFrames - 1))
	{
		// Initialize RequiredBones for pose evaluation
		RequiredBones.SetUseRAWData(true);

		USkeleton* MySkeleton = AnimToEval->GetSkeleton();
		check(MySkeleton);

		RequiredBoneIndexArray.AddUninitialized(MySkeleton->GetReferenceSkeleton().GetNum());
		for (int32 BoneIndex = 0; BoneIndex < RequiredBoneIndexArray.Num(); ++BoneIndex)
		{
			RequiredBoneIndexArray[BoneIndex] = BoneIndex;
		}

		RequiredBones.InitializeTo(RequiredBoneIndexArray, FCurveEvaluationOption(true), *MySkeleton);
	}

};

void UAnimSequence::BakeOutVirtualBoneTracks()
{
	const int32 NumVirtualBones = GetSkeleton()->GetVirtualBones().Num();
	check( (RawAnimationData.Num() == TrackToSkeletonMapTable.Num()) && (RawAnimationData.Num() == AnimationTrackNames.Num()) ); //Make sure starting data is valid

	TArray<FRawAnimSequenceTrack> NewRawTracks = TArray<FRawAnimSequenceTrack>(RawAnimationData, NumVirtualBones);

	TArray<FTrackToSkeletonMap> NewTrackToSkeletonMapTable = TArray<FTrackToSkeletonMap>(TrackToSkeletonMapTable, NumVirtualBones);

	TArray<FName> NewAnimationTrackNames = TArray<FName>(AnimationTrackNames, NumVirtualBones);

	for (int32 VBIndex = 0; VBIndex < NumVirtualBones; ++VBIndex)
	{
		const int32 TrackIndex = NewRawTracks.Add(FRawAnimSequenceTrack());

		//Init new tracks
		NewRawTracks[TrackIndex].PosKeys.SetNumUninitialized(NumFrames);
		NewRawTracks[TrackIndex].RotKeys.SetNumUninitialized(NumFrames);
		NewRawTracks[TrackIndex].ScaleKeys.SetNumUninitialized(NumFrames);
		
		NewTrackToSkeletonMapTable.Add(FTrackToSkeletonMap(GetSkeleton()->GetReferenceSkeleton().GetRequiredVirtualBones()[VBIndex]));
		NewAnimationTrackNames.Add(GetSkeleton()->GetVirtualBones()[VBIndex].VirtualBoneName);
	}

	FMemMark Mark(FMemStack::Get());
	FByFramePoseEvalContext EvalContext(this);

	//Pose evaluation data
	FCompactPose Pose;
	Pose.SetBoneContainer(&EvalContext.RequiredBones);
	FAnimExtractContext ExtractContext;

	const TArray<FVirtualBoneRefData>& VBRefData = GetSkeleton()->GetReferenceSkeleton().GetVirtualBoneRefData();

	for (int Frame = 0; Frame < NumFrames; ++Frame)
	{
		// Initialise curve data from Skeleton
		FBlendedCurve Curve;
		Curve.InitFrom(EvalContext.RequiredBones);

		//Grab pose for this frame
		const float CurrentFrameTime = Frame * EvalContext.IntervalTime;
		ExtractContext.CurrentTime = CurrentFrameTime;
		GetAnimationPose(Pose, Curve, ExtractContext);

		for (int32 VBIndex = 0; VBIndex < VBRefData.Num(); ++VBIndex)
		{
			const FVirtualBoneRefData& VB = VBRefData[VBIndex];
			CopyTransformToRawAnimationData(Pose[FCompactPoseBoneIndex(VB.VBRefSkelIndex)], NewRawTracks[VBIndex + RawAnimationData.Num()], Frame);
		}
	}

	RawAnimationData = MoveTemp(NewRawTracks);
	AnimationTrackNames = MoveTemp(NewAnimationTrackNames);
	TrackToSkeletonMapTable = MoveTemp(NewTrackToSkeletonMapTable);

	CompressRawAnimData();
}

bool IsIdentity(const FVector& Pos)
{
	return Pos.Equals(FVector::ZeroVector);
}

bool IsIdentity(const FQuat& Rot)
{
	return Rot.Equals(FQuat::Identity);
}

template<class KeyType>
bool IsKeyArrayValidForRemoval(const TArray<KeyType>& Keys)
{
	return Keys.Num() == 0 || (Keys.Num() == 1 && IsIdentity(Keys[0]));
}

bool IsRawTrackValidForRemoval(const FRawAnimSequenceTrack& Track)
{
	return	IsKeyArrayValidForRemoval(Track.PosKeys) &&
			IsKeyArrayValidForRemoval(Track.RotKeys) &&
			IsKeyArrayValidForRemoval(Track.ScaleKeys);
}

void UAnimSequence::BakeOutAdditiveIntoRawData()
{
	if (!CanBakeAdditive())
	{
		return; // Nothing to do
	}

	USkeleton* MySkeleton = GetSkeleton();
	check(MySkeleton);

	if (RefPoseSeq && RefPoseSeq->HasAnyFlags(EObjectFlags::RF_NeedPostLoad))
	{
		RefPoseSeq->VerifyCurveNames<FFloatCurve>(*MySkeleton, USkeleton::AnimCurveMappingName, RefPoseSeq->RawCurveData.FloatCurves);
	}

	FMemMark Mark(FMemStack::Get());

	FByFramePoseEvalContext EvalContext(this);

	//New raw data
	FRawCurveTracks NewCurveTracks;

	TArray<FRawAnimSequenceTrack> NewRawTracks;
	NewRawTracks.SetNum(EvalContext.RequiredBoneIndexArray.Num());

	for (FRawAnimSequenceTrack& RawTrack : NewRawTracks)
	{
		RawTrack.PosKeys.SetNumUninitialized(NumFrames);
		RawTrack.RotKeys.SetNumUninitialized(NumFrames);
		RawTrack.ScaleKeys.SetNumUninitialized(NumFrames);
	}

	// keep the same buffer size
	TemporaryAdditiveBaseAnimationData = NewRawTracks;

	TArray<FTrackToSkeletonMap> NewTrackToSkeletonMapTable;
	NewTrackToSkeletonMapTable.SetNumUninitialized(EvalContext.RequiredBoneIndexArray.Num());

	TArray<FName> NewAnimationTrackNames;
	NewAnimationTrackNames.SetNumUninitialized(EvalContext.RequiredBoneIndexArray.Num());

	for (int32 TrackIndex = 0; TrackIndex < EvalContext.RequiredBoneIndexArray.Num(); ++TrackIndex)
	{
		NewTrackToSkeletonMapTable[TrackIndex].BoneTreeIndex = TrackIndex;
		NewAnimationTrackNames[TrackIndex] = GetSkeleton()->GetReferenceSkeleton().GetBoneName(TrackIndex);
	}

	//Pose evaluation data
	FCompactPose Pose;
	Pose.SetBoneContainer(&EvalContext.RequiredBones);
	FCompactPose BasePose;
	BasePose.SetBoneContainer(&EvalContext.RequiredBones);
	FAnimExtractContext ExtractContext;

	for (int Frame = 0; Frame < NumFrames; ++Frame)
	{
		// Initialise curve data from Skeleton
		FBlendedCurve Curve;
		Curve.InitFrom(EvalContext.RequiredBones);

		FBlendedCurve DummyBaseCurve;
		DummyBaseCurve.InitFrom(EvalContext.RequiredBones);

		//Grab pose for this frame
		const float CurrentFrameTime = Frame * EvalContext.IntervalTime;
		ExtractContext.CurrentTime = CurrentFrameTime;
		GetAnimationPose(Pose, Curve, ExtractContext);
		GetAdditiveBasePose(BasePose, DummyBaseCurve, ExtractContext);

		//Write out every track for this frame
		for (FCompactPoseBoneIndex TrackIndex(0); TrackIndex < NewRawTracks.Num(); ++TrackIndex)
		{
			CopyTransformToRawAnimationData(Pose[TrackIndex], NewRawTracks[TrackIndex.GetInt()], Frame);
			CopyTransformToRawAnimationData(BasePose[TrackIndex], TemporaryAdditiveBaseAnimationData[TrackIndex.GetInt()], Frame);
		}

		//Write out curve data for this frame
		const TArray<SmartName::UID_Type>& UIDList = *Curve.UIDList;
		for (int32 CurveIndex = 0; CurveIndex < UIDList.Num(); ++CurveIndex)
		{
			SmartName::UID_Type CurveUID = UIDList[CurveIndex];
			FCurveElement& CurveEL = Curve.Elements[CurveIndex];
			FFloatCurve* RawCurve = GetFloatCurve(NewCurveTracks, CurveUID);
			if (!RawCurve && CurveEL.Value > 0.f) //Only make a new curve if we are going to give it data
			{
				FSmartName NewCurveName;
				// if we don't have name, there is something wrong here. 
				ensureAlways(MySkeleton->GetSmartNameByUID(USkeleton::AnimCurveMappingName, CurveUID, NewCurveName));
				// curve flags don't matter much for compressed curves
				NewCurveTracks.AddCurveData(NewCurveName, 0, ERawCurveTrackTypes::RCT_Float);
				RawCurve = GetFloatCurve(NewCurveTracks, CurveUID);
			}

			if (RawCurve)
			{
				const bool bHasKeys = RawCurve->FloatCurve.GetNumKeys() > 0;
				if (!bHasKeys)
				{
					//Add pre key of 0
					if (Frame > 0)
					{
						const float PreKeyTime = (Frame - 1)*EvalContext.IntervalTime;
						RawCurve->UpdateOrAddKey(0.f, PreKeyTime);
					}

				}

				if (!bHasKeys || IsNewKeyDifferent(RawCurve->FloatCurve.GetLastKey(), CurveEL.Value))
				{
					RawCurve->UpdateOrAddKey(CurveEL.Value, CurrentFrameTime);
				}
			}
		}
	}

	RawAnimationData = MoveTemp(NewRawTracks);
	AnimationTrackNames = MoveTemp(NewAnimationTrackNames);
	TrackToSkeletonMapTable = MoveTemp(NewTrackToSkeletonMapTable);
	RawCurveData = NewCurveTracks;

	const FSmartNameMapping* Mapping = GetSkeleton()->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	check(Mapping); // Should always exist
	RawCurveData.RefreshName(Mapping);

#if 0 //Validate baked data
	for (FRawAnimSequenceTrack& RawTrack : RawAnimationData)
	{
		for (FQuat& Rot : RawTrack.RotKeys)
		{
			check(Rot.IsNormalized());
		}
	}
#endif

	CompressRawAnimData();

	// Note on (TrackIndex > 0) below : deliberately stop before track 0, compression code doesn't like getting a completely empty animation
	for (int32 TrackIndex = RawAnimationData.Num() - 1; TrackIndex > 0; --TrackIndex)
	{
		const FRawAnimSequenceTrack& Track = RawAnimationData[TrackIndex];
		if (IsRawTrackValidForRemoval(Track))
		{
			RawAnimationData.RemoveAtSwap(TrackIndex, 1, false);
			AnimationTrackNames.RemoveAtSwap(TrackIndex, 1, false);
			TrackToSkeletonMapTable.RemoveAtSwap(TrackIndex, 1, false);
		}
	}

}

void UAnimSequence::FlagDependentAnimationsAsRawDataOnly() const
{
	for (TObjectIterator<UAnimSequence> Iter; Iter; ++Iter)
	{
		UAnimSequence* Seq = *Iter;
		if (Seq->RefPoseSeq == this)
		{
			Seq->bUseRawDataOnly = true;
		}
	}
}

#endif

void UAnimSequence::RecycleAnimSequence()
{
#if WITH_EDITORONLY_DATA
	// Clear RawAnimData
	RawAnimationData.Empty();
	RawDataGuid.Invalidate();
	AnimationTrackNames.Empty();
	TrackToSkeletonMapTable.Empty();
	CompressedTrackToSkeletonMapTable.Empty();
	CompressedTrackOffsets.Empty(0);
	CompressedByteStream.Empty(0);
	CompressedScaleOffsets.Empty(0);
	SourceRawAnimationData.Empty(0);
	RawCurveData.Empty();
	CompressedCurveData.Empty();
	AuthoredSyncMarkers.Empty();
	UniqueMarkerNames.Empty();
	Notifies.Empty();
	AnimNotifyTracks.Empty();
	CompressionScheme = nullptr;
	TranslationCompressionFormat = RotationCompressionFormat = ScaleCompressionFormat = ACF_None;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UAnimSequence::CleanAnimSequenceForImport()
{
	// Clear RawAnimData
	RawAnimationData.Empty();
	RawDataGuid.Invalidate();
	AnimationTrackNames.Empty();
	TrackToSkeletonMapTable.Empty();
	CompressedTrackOffsets.Empty(0);
	CompressedByteStream.Empty(0);
	CompressedScaleOffsets.Empty(0);
	SourceRawAnimationData.Empty(0);
}
#endif // WITH_EDITOR

bool UAnimSequence::CopyAnimSequenceProperties(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq, bool bSkipCopyingNotifies)
{
#if WITH_EDITORONLY_DATA
	// Copy parameters
	DestAnimSeq->SequenceLength				= SourceAnimSeq->SequenceLength;
	DestAnimSeq->NumFrames					= SourceAnimSeq->NumFrames;
	DestAnimSeq->RateScale					= SourceAnimSeq->RateScale;
	DestAnimSeq->bDoNotOverrideCompression	= SourceAnimSeq->bDoNotOverrideCompression;

	// Copy Compression Settings
	DestAnimSeq->CompressionScheme = static_cast<UAnimCompress*>(StaticDuplicateObject(SourceAnimSeq->CompressionScheme, DestAnimSeq, NAME_None, RF_AllFlags, nullptr, EDuplicateMode::Normal, ~EInternalObjectFlags::RootSet));
	DestAnimSeq->TranslationCompressionFormat	= SourceAnimSeq->TranslationCompressionFormat;
	DestAnimSeq->RotationCompressionFormat		= SourceAnimSeq->RotationCompressionFormat;
	DestAnimSeq->AdditiveAnimType				= SourceAnimSeq->AdditiveAnimType;
	DestAnimSeq->RefPoseType					= SourceAnimSeq->RefPoseType;
	DestAnimSeq->RefPoseSeq						= SourceAnimSeq->RefPoseSeq;
	DestAnimSeq->RefFrameIndex					= SourceAnimSeq->RefFrameIndex;

	if( !bSkipCopyingNotifies )
	{
		// Copy Metadata information
		CopyNotifies(SourceAnimSeq, DestAnimSeq);
	}

	DestAnimSeq->MarkPackageDirty();

	// Copy Curve Data
	DestAnimSeq->RawCurveData = SourceAnimSeq->RawCurveData;
#endif // WITH_EDITORONLY_DATA

	return true;
}


bool UAnimSequence::CopyNotifies(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq)
{
#if WITH_EDITOR
	// Abort if source == destination.
	if( SourceAnimSeq == DestAnimSeq )
	{
		return true;
	}

	// If the destination sequence is shorter than the source sequence, we'll be dropping notifies that
	// occur at later times than the dest sequence is long.  Give the user a chance to abort if we
	// find any notifies that won't be copied over.
	if( DestAnimSeq->SequenceLength < SourceAnimSeq->SequenceLength )
	{
		for(int32 NotifyIndex=0; NotifyIndex<SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
		{
			// If a notify is found which occurs off the end of the destination sequence, prompt the user to continue.
			const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies[NotifyIndex];
			if( SrcNotifyEvent.DisplayTime_DEPRECATED > DestAnimSeq->SequenceLength )
			{
				const bool bProceed = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "SomeNotifiesWillNotBeCopiedQ", "Some notifies will not be copied because the destination sequence is not long enough.  Proceed?") );
				if( !bProceed )
				{
					return false;
				}
				else
				{
					break;
				}
			}
		}
	}

	// If the destination sequence contains any notifies, ask the user if they'd like
	// to delete the existing notifies before copying over from the source sequence.
	if( DestAnimSeq->Notifies.Num() > 0 )
	{
		const bool bDeleteExistingNotifies = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(
			NSLOCTEXT("UnrealEd", "DestSeqAlreadyContainsNotifiesMergeQ", "The destination sequence already contains {0} notifies.  Delete these before copying?"), FText::AsNumber(DestAnimSeq->Notifies.Num())) );
		if( bDeleteExistingNotifies )
		{
			DestAnimSeq->Notifies.Empty();
			DestAnimSeq->MarkPackageDirty();
		}
	}

	// Do the copy.
	TArray<int32> NewNotifyIndices;
	int32 NumNotifiesThatWereNotCopied = 0;

	for(int32 NotifyIndex=0; NotifyIndex<SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
	{
		const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies[NotifyIndex];

		// Skip notifies which occur at times later than the destination sequence is long.
		if( SrcNotifyEvent.DisplayTime_DEPRECATED > DestAnimSeq->SequenceLength )
		{
			continue;
		}

		// Do a linear-search through existing notifies to determine where
		// to insert the new notify.
		int32 NewNotifyIndex = 0;
		while( NewNotifyIndex < DestAnimSeq->Notifies.Num()
			&& DestAnimSeq->Notifies[NewNotifyIndex].DisplayTime_DEPRECATED <= SrcNotifyEvent.DisplayTime_DEPRECATED )
		{
			++NewNotifyIndex;
		}

		// Track the location of the new notify.
		NewNotifyIndices.Add(NewNotifyIndex);

		// Create a new empty on in the array.
		DestAnimSeq->Notifies.InsertZeroed(NewNotifyIndex);

		// Copy time and comment.
		FAnimNotifyEvent& Notify = DestAnimSeq->Notifies[NewNotifyIndex];
		Notify.DisplayTime_DEPRECATED = SrcNotifyEvent.DisplayTime_DEPRECATED;
		Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType( DestAnimSeq->CalculateOffsetForNotify(Notify.DisplayTime_DEPRECATED));
		Notify.NotifyName = SrcNotifyEvent.NotifyName;
		Notify.Duration = SrcNotifyEvent.Duration;

		// Copy the notify itself, and point the new one at it.
		if( SrcNotifyEvent.Notify )
		{
			DestAnimSeq->Notifies[NewNotifyIndex].Notify = static_cast<UAnimNotify*>(StaticDuplicateObject(SrcNotifyEvent.Notify, DestAnimSeq, NAME_None, RF_AllFlags, nullptr, EDuplicateMode::Normal, ~EInternalObjectFlags::RootSet));
		}
		else
		{
			DestAnimSeq->Notifies[NewNotifyIndex].Notify = NULL;
		}

		if( SrcNotifyEvent.NotifyStateClass )
		{
			DestAnimSeq->Notifies[NewNotifyIndex].NotifyStateClass = static_cast<UAnimNotifyState*>(StaticDuplicateObject(SrcNotifyEvent.NotifyStateClass, DestAnimSeq, NAME_None, RF_AllFlags, nullptr, EDuplicateMode::Normal, ~EInternalObjectFlags::RootSet));
		}
		else
		{
			DestAnimSeq->Notifies[NewNotifyIndex].NotifyStateClass = NULL;
		}

		// Make sure editor knows we've changed something.
		DestAnimSeq->MarkPackageDirty();
	}

	// Inform the user if some notifies weren't copied.
	if( SourceAnimSeq->Notifies.Num() > NewNotifyIndices.Num() )
	{
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format(
			NSLOCTEXT("UnrealEd", "SomeNotifiesWereNotCopiedF", "Because the destination sequence was shorter, {0} notifies were not copied."), FText::AsNumber(SourceAnimSeq->Notifies.Num() - NewNotifyIndices.Num())) );
	}
#endif // WITH_EDITOR

	return true;
}

bool UAnimSequence::IsValidAdditive() const		
{ 
	if (AdditiveAnimType != AAT_None)
	{
		switch (RefPoseType)
		{
		case ABPT_RefPose:
			return true;
		case ABPT_AnimScaled:
			return (RefPoseSeq != NULL);
		case ABPT_AnimFrame:
			return (RefPoseSeq != NULL) && (RefFrameIndex >= 0);
		default:
			return false;
		}
	}

	return false;
}

#if WITH_EDITOR

int32 FindMeshBoneIndexFromBoneName(USkeleton * Skeleton, const FName &BoneName)
{
	USkeletalMesh * PreviewMesh = Skeleton->GetPreviewMesh();
	const int32& SkeletonBoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);

	int32 BoneIndex = INDEX_NONE;

	if(SkeletonBoneIndex != INDEX_NONE)
	{
		BoneIndex = Skeleton->GetMeshBoneIndexFromSkeletonBoneIndex(PreviewMesh, SkeletonBoneIndex);
	}

	return BoneIndex;
}
void FillUpTransformBasedOnRig(USkeleton* Skeleton, TArray<FTransform>& NodeSpaceBases, TArray<FTransform> &Rotations, TArray<FTransform>& Translations, TArray<bool>& TranslationParentFlags)
{
	TArray<FTransform> SpaceBases;
	FAnimationRuntime::FillUpComponentSpaceTransformsRetargetBasePose(Skeleton, SpaceBases);

	const URig* Rig = Skeleton->GetRig();

	if (Rig)
	{
		// this one has to collect all Nodes in Rig data
		// since we're comparing two of them together. 
		int32 NodeNum = Rig->GetNodeNum();

		if (NodeNum > 0)
		{
			NodeSpaceBases.Empty(NodeNum);
			NodeSpaceBases.AddUninitialized(NodeNum);

			Rotations.Empty(NodeNum);
			Rotations.AddUninitialized(NodeNum);

			Translations.Empty(NodeNum);
			Translations.AddUninitialized(NodeNum);

			TranslationParentFlags.Empty(Translations.Num());
			TranslationParentFlags.AddZeroed(Translations.Num());

			const USkeletalMesh* PreviewMesh = Skeleton->GetPreviewMesh();

			for (int32 Index = 0; Index < NodeNum; ++Index)
			{
				const FName NodeName = Rig->GetNodeName(Index);
				const FName& BoneName = Skeleton->GetRigBoneMapping(NodeName);
				const int32& BoneIndex = FindMeshBoneIndexFromBoneName(Skeleton, BoneName);

				if (BoneIndex == INDEX_NONE)
				{
					// add identity
					NodeSpaceBases[Index].SetIdentity();
					Rotations[Index].SetIdentity();
					Translations[Index].SetIdentity();
				}
				else
				{
					// initialize with SpaceBases - assuming World Based
					NodeSpaceBases[Index] = SpaceBases[BoneIndex];
					Rotations[Index] = SpaceBases[BoneIndex];
					Translations[Index] = SpaceBases[BoneIndex];

					const FTransformBase* TransformBase = Rig->GetTransformBaseByNodeName(NodeName);

					if (TransformBase != NULL)
					{
						// orientation constraint			
						const auto& RotConstraint = TransformBase->Constraints[EControlConstraint::Type::Orientation];

						if (RotConstraint.TransformConstraints.Num() > 0)
						{
							const FName& ParentBoneName = Skeleton->GetRigBoneMapping(RotConstraint.TransformConstraints[0].ParentSpace);
							const int32& ParentBoneIndex = FindMeshBoneIndexFromBoneName(Skeleton, ParentBoneName);

							if (ParentBoneIndex != INDEX_NONE)
							{
								Rotations[Index] = SpaceBases[BoneIndex].GetRelativeTransform(SpaceBases[ParentBoneIndex]);
							}
						}

						// translation constraint
						const auto& TransConstraint = TransformBase->Constraints[EControlConstraint::Type::Translation];

						if (TransConstraint.TransformConstraints.Num() > 0)
						{
							const FName& ParentBoneName = Skeleton->GetRigBoneMapping(TransConstraint.TransformConstraints[0].ParentSpace);
							const int32& ParentBoneIndex = FindMeshBoneIndexFromBoneName(Skeleton, ParentBoneName);

							if (ParentBoneIndex != INDEX_NONE)
							{
								// I think translation has to include rotation, otherwise it won't work
								Translations[Index] = SpaceBases[BoneIndex].GetRelativeTransform(SpaceBases[ParentBoneIndex]);
								TranslationParentFlags[Index] = true;
							}
						}
					}
				}
			}
		}
	}
}

int32 FindValidTransformParentTrack(const URig* Rig, int32 NodeIndex, bool bTranslate, const TArray<FName>& ValidNodeNames)
{
	int32 ParentIndex = Rig->FindTransformParentNode(NodeIndex, bTranslate);

	// verify if it exists in ValidNodeNames
	if (ParentIndex != INDEX_NONE)
	{
		FName NodeName = Rig->GetNodeName(ParentIndex);

		return ValidNodeNames.Find(NodeName);
	}

	return INDEX_NONE;
}


void UAnimSequence::RemapTracksToNewSkeleton( USkeleton* NewSkeleton, bool bConvertSpaces )
{
	// this is not cheap, so make sure it only happens in editor

	// @Todo : currently additive will work fine since we don't bake anything except when we extract
	// but in the future if we bake this can be problem
	if (bConvertSpaces)
	{
		USkeleton* OldSkeleton = GetSkeleton();

		// first check if both has same rig, if so, we'll retarget using it
		if (OldSkeleton && OldSkeleton->GetRig() != NULL && NewSkeleton->GetRig() == OldSkeleton->GetRig() && OldSkeleton->GetPreviewMesh() && NewSkeleton->GetPreviewMesh())
		{
			const URig* Rig = OldSkeleton->GetRig();

			// we'll have to save the relative space bases transform from old ref pose to new refpose
			TArray<FTransform> RelativeToNewSpaceBases;
			// save the ratio of translation change
			TArray<float> OldToNewTranslationRatio;
			// create relative transform in component space between old skeleton and new skeleton
			{
				// first calculate component space ref pose to get the relative transform between
				// two ref poses. It is very important update ref pose before getting here. 
				TArray<FTransform> NewRotations, OldRotations, NewSpaceBases, OldSpaceBases;
				TArray<FTransform> NewTranslations, OldTranslations;
				TArray<bool> NewTranslationParentFlags, OldTranslationParentFlags;
				// get the spacebases transform
				FillUpTransformBasedOnRig(NewSkeleton, NewSpaceBases, NewRotations, NewTranslations, NewTranslationParentFlags);
				FillUpTransformBasedOnRig(OldSkeleton, OldSpaceBases, OldRotations, OldTranslations, OldTranslationParentFlags);

				// now we'd like to get the relative transform from old to new ref pose in component space
				// PK2*K2 = PK1*K1*theta where theta => P1*R1*theta = P2*R2 
				// where	P1 - parent transform in component space for original skeleton
				//			R1 - local space of the current bone for original skeleton
				//			P2 - parent transform in component space for new skeleton
				//			R2 - local space of the current bone for new skeleton
				// what we're looking for is theta, so that we can apply that to animated transform
				// this has to have all of nodes since comparing two skeletons, that might have different configuration
				int32 NumNodes = Rig->GetNodeNum();
				// saves the theta data per node
				RelativeToNewSpaceBases.AddUninitialized(NumNodes);
				// saves the translation conversion datao
				OldToNewTranslationRatio.AddUninitialized(NumNodes);

				const TArray<FNode>& Nodes = Rig->GetNodes();
				// calculate the relative transform to new skeleton
				// so that we can apply the delta in component space
				for (int32 NodeIndex = 0; NodeIndex < NumNodes; ++NodeIndex)
				{
					// theta (RelativeToNewTransform) = (P1*R1)^(-1) * P2*R2 where theta => P1*R1*theta = P2*R2
					RelativeToNewSpaceBases[NodeIndex] = NewSpaceBases[NodeIndex].GetRelativeTransform(OldSpaceBases[NodeIndex]); 

					// also savees the translation difference between old to new
					FVector OldTranslation = OldTranslations[NodeIndex].GetTranslation();
					FVector NewTranslation = NewTranslations[NodeIndex].GetTranslation();

					// skip root because we don't really have clear relative point to test with it
					if (NodeIndex != 0 && NewTranslationParentFlags[NodeIndex] == OldTranslationParentFlags[NodeIndex])
					{
						// only do this if parent status matches, otherwise, you'll have invalid state 
						// where one is based on shoulder, where the other is missing the shoulder node
						float OldTranslationSize = OldTranslation.Size();
						float NewTranslationSize = NewTranslation.Size();

						OldToNewTranslationRatio[NodeIndex] = (FMath::IsNearlyZero(OldTranslationSize)) ? 1.f/*do not touch new translation size*/ : NewTranslationSize / OldTranslationSize;
					}
					else
					{
						OldToNewTranslationRatio[NodeIndex] = 1.f; // set to be 1, we don't know what it is
					}

					UE_LOG(LogAnimation, Verbose, TEXT("Retargeting (%s : %d) : OldtoNewTranslationRatio (%0.2f), Relative Transform (%s)"), *Nodes[NodeIndex].Name.ToString(), NodeIndex, 
						OldToNewTranslationRatio[NodeIndex], *RelativeToNewSpaceBases[NodeIndex].ToString());
					UE_LOG(LogAnimation, Verbose, TEXT("\tOldSpaceBase(%s), NewSpaceBase(%s)"), *OldSpaceBases[NodeIndex].ToString(), *NewSpaceBases[NodeIndex].ToString());
				}
			}

			FAnimSequenceTrackContainer RiggingAnimationData;

			// now convert animation data to rig data
			ConvertAnimationDataToRiggingData(RiggingAnimationData);

			// here we have to watch out the index
			// The RiggingAnimationData will contain only the nodes that are mapped to source skeleton
			// and here we convert everything that is in RiggingAnimationData which means based on source data
			// when mapped back to new skeleton, it will discard results that are not mapped to target skeleton
			
			TArray<FName> SrcValidNodeNames;
			int32 SrcNumTracks = OldSkeleton->GetMappedValidNodes(SrcValidNodeNames);

			// now convert to space bases animation 
			TArray< TArray<FTransform> > ComponentSpaceAnimations, ConvertedLocalSpaceAnimations, ConvertedSpaceAnimations;
			ComponentSpaceAnimations.AddZeroed(SrcNumTracks);
			ConvertedSpaceAnimations.AddZeroed(SrcNumTracks);
			ConvertedLocalSpaceAnimations.AddZeroed(SrcNumTracks);

			int32 NumKeys = NumFrames;
			float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

			// allocate arrays
			for(int32 SrcTrackIndex=0; SrcTrackIndex<SrcNumTracks; ++SrcTrackIndex)
			{
				ComponentSpaceAnimations[SrcTrackIndex].AddUninitialized(NumKeys);
				ConvertedLocalSpaceAnimations[SrcTrackIndex].AddUninitialized(NumKeys);
				ConvertedSpaceAnimations[SrcTrackIndex].AddUninitialized(NumKeys);
			}

			for (int32 SrcTrackIndex=0; SrcTrackIndex<SrcNumTracks; ++SrcTrackIndex)		
			{
				int32 NodeIndex = Rig->FindNode(SrcValidNodeNames[SrcTrackIndex]);
				check (NodeIndex != INDEX_NONE);
				auto& RawAnimation = RiggingAnimationData.AnimationTracks[SrcTrackIndex];

				// find rotation parent node
				int32 RotParentTrackIndex = FindValidTransformParentTrack(Rig, NodeIndex, false, SrcValidNodeNames);
				int32 TransParentTrackIndex = FindValidTransformParentTrack(Rig, NodeIndex, true, SrcValidNodeNames);
				// fill up keys - calculate PK1 * K1
				for(int32 Key=0; Key<NumKeys; ++Key)
				{
					FTransform AnimatedLocalKey;
					ExtractBoneTransform(RiggingAnimationData.AnimationTracks, AnimatedLocalKey, SrcTrackIndex, Interval*Key);

					AnimatedLocalKey.ScaleTranslation(OldToNewTranslationRatio[NodeIndex]);

					if(RotParentTrackIndex != INDEX_NONE)
					{
						FQuat ComponentSpaceRotation = ComponentSpaceAnimations[RotParentTrackIndex][Key].GetRotation() * AnimatedLocalKey.GetRotation();
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetRotation(ComponentSpaceRotation);
					}
					else
					{
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetRotation(AnimatedLocalKey.GetRotation());
					}

					if (TransParentTrackIndex != INDEX_NONE)
					{
						FVector ComponentSpaceTranslation = ComponentSpaceAnimations[TransParentTrackIndex][Key].TransformPosition(AnimatedLocalKey.GetTranslation());
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetTranslation(ComponentSpaceTranslation);
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetScale3D(AnimatedLocalKey.GetScale3D());
					}
					else
					{
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetTranslation(AnimatedLocalKey.GetTranslation());
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetScale3D(AnimatedLocalKey.GetScale3D());
					}
				}
			}

			// now animation is converted to component space
			TArray<struct FRawAnimSequenceTrack> NewRawAnimationData = RiggingAnimationData.AnimationTracks;
			for (int32 SrcTrackIndex=0; SrcTrackIndex<SrcNumTracks; ++SrcTrackIndex)
			{
				int32 NodeIndex = Rig->FindNode(SrcValidNodeNames[SrcTrackIndex]);
				// find rotation parent node
				int32 RotParentTrackIndex = FindValidTransformParentTrack(Rig, NodeIndex, false, SrcValidNodeNames);
				int32 TransParentTrackIndex = FindValidTransformParentTrack(Rig, NodeIndex, true, SrcValidNodeNames);

				// clear translation;
				RelativeToNewSpaceBases[NodeIndex].SetTranslation(FVector::ZeroVector);

				for(int32 Key=0; Key<NumKeys; ++Key)
				{
					// now convert to the new space and save to local spaces
					ConvertedSpaceAnimations[SrcTrackIndex][Key] = RelativeToNewSpaceBases[NodeIndex] * ComponentSpaceAnimations[SrcTrackIndex][Key];

					if(RotParentTrackIndex != INDEX_NONE)
					{
						FQuat LocalRotation = ConvertedSpaceAnimations[RotParentTrackIndex][Key].GetRotation().Inverse() * ConvertedSpaceAnimations[SrcTrackIndex][Key].GetRotation();
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetRotation(LocalRotation);
					}
					else
					{
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetRotation(ConvertedSpaceAnimations[SrcTrackIndex][Key].GetRotation());
					}

					if(TransParentTrackIndex != INDEX_NONE)
					{
						FVector LocalTranslation = ConvertedSpaceAnimations[SrcTrackIndex][Key].GetRelativeTransform(ConvertedSpaceAnimations[TransParentTrackIndex][Key]).GetTranslation();
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetTranslation(LocalTranslation);
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetScale3D(ConvertedSpaceAnimations[SrcTrackIndex][Key].GetScale3D());
					}
					else
					{
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetTranslation(ConvertedSpaceAnimations[SrcTrackIndex][Key].GetTranslation());
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetScale3D(ConvertedSpaceAnimations[SrcTrackIndex][Key].GetScale3D());
					}
				}

				auto& RawAnimation = NewRawAnimationData[SrcTrackIndex];
				RawAnimation.PosKeys.Empty(NumKeys);
				RawAnimation.PosKeys.AddUninitialized(NumKeys);
				RawAnimation.RotKeys.Empty(NumKeys);
				RawAnimation.RotKeys.AddUninitialized(NumKeys);
				RawAnimation.ScaleKeys.Empty(NumKeys);
				RawAnimation.ScaleKeys.AddUninitialized(NumKeys);

				for(int32 Key=0; Key<NumKeys; ++Key)
				{
					RawAnimation.PosKeys[Key] = ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].GetLocation();
					RawAnimation.RotKeys[Key] = ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].GetRotation();
					RawAnimation.ScaleKeys[Key] = ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].GetScale3D();

					// normalize rotation
					RawAnimation.RotKeys[Key].Normalize();
				}
			}

			RiggingAnimationData.AnimationTracks = MoveTemp(NewRawAnimationData);
			RiggingAnimationData.TrackNames = MoveTemp(SrcValidNodeNames);

			// set new skeleton
			SetSkeleton(NewSkeleton);

			// convert back to animated data with new skeleton
			ConvertRiggingDataToAnimationData(RiggingAnimationData);
		}
		// @todo end rig testing
		// @IMPORTANT: now otherwise this will try to do bone to bone mapping
		else if(OldSkeleton)
		{
			// this only replaces the primary one, it doesn't replace old ones
			TArray<struct FTrackToSkeletonMap> NewTrackToSkeletonMapTable;
			NewTrackToSkeletonMapTable.Empty(AnimationTrackNames.Num());
			NewTrackToSkeletonMapTable.AddUninitialized(AnimationTrackNames.Num());
			for (int32 Track = 0; Track < AnimationTrackNames.Num(); ++Track)
			{
				int32 BoneIndex = NewSkeleton->GetReferenceSkeleton().FindBoneIndex(AnimationTrackNames[Track]);
				NewTrackToSkeletonMapTable[Track].BoneTreeIndex = BoneIndex;
			}

			// now I have all NewTrack To Skeleton Map Table
			// I'll need to compare with old tracks and copy over if SkeletonIndex == 0
			// if SkeletonIndex != 0, we need to see if we can 
			for (int32 TableId = 0; TableId < NewTrackToSkeletonMapTable.Num(); ++TableId)
			{
				if (ensure(TrackToSkeletonMapTable.IsValidIndex(TableId)))
				{
					if (NewTrackToSkeletonMapTable[TableId].BoneTreeIndex != INDEX_NONE)
					{
						TrackToSkeletonMapTable[TableId].BoneTreeIndex = NewTrackToSkeletonMapTable[TableId].BoneTreeIndex;
					}
					else
					{
						// if not found, delete the track data
						RemoveTrack(TableId);
						NewTrackToSkeletonMapTable.RemoveAt(TableId);
						--TableId;
					}
				}
			}

			if (TrackToSkeletonMapTable.Num() == 0)
			{
				// no bones to retarget
				// return with error
				//@todo fail message
			}
			// make sure you do update reference pose before coming here
			
			// first calculate component space ref pose to get the relative transform between
			// two ref poses. It is very important update ref pose before getting here. 
			TArray<FTransform> NewSpaceBaseRefPose, OldSpaceBaseRefPose, RelativeToNewTransform;
			// get the spacebases transform
			FAnimationRuntime::FillUpComponentSpaceTransformsRefPose(NewSkeleton, NewSpaceBaseRefPose);
			FAnimationRuntime::FillUpComponentSpaceTransformsRefPose(OldSkeleton, OldSpaceBaseRefPose);

			const TArray<FTransform>& OldRefPose = OldSkeleton->GetReferenceSkeleton().GetRefBonePose();
			const TArray<FTransform>& NewRefPose = NewSkeleton->GetReferenceSkeleton().GetRefBonePose();

			// now we'd like to get the relative transform from old to new ref pose in component space
			// PK2*K2 = PK1*K1*theta where theta => P1*R1*theta = P2*R2 
			// where	P1 - parent transform in component space for original skeleton
			//			R1 - local space of the current bone for original skeleton
			//			P2 - parent transform in component space for new skeleton
			//			R2 - local space of the current bone for new skeleton
			// what we're looking for is theta, so that we can apply that to animated transform
			int32 NumBones = NewSpaceBaseRefPose.Num();
			// saves the theta data per bone
			RelativeToNewTransform.AddUninitialized(NumBones);
			TArray<float> OldToNewTranslationRatio;
			// saves the translation conversion data
			OldToNewTranslationRatio.AddUninitialized(NumBones);

			// calculate the relative transform to new skeleton
			// so that we can apply the delta in component space
			for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
			{
				// first find bone name of the idnex
				FName BoneName = NewSkeleton->GetReferenceSkeleton().GetRefBoneInfo()[BoneIndex].Name;
				// find it in old index
				int32 OldBoneIndex = OldSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);

				// get old bone index
				if(OldBoneIndex != INDEX_NONE)
				{
					// theta (RelativeToNewTransform) = (P1*R1)^(-1) * P2*R2 where theta => P1*R1*theta = P2*R2
					RelativeToNewTransform[BoneIndex] = NewSpaceBaseRefPose[BoneIndex].GetRelativeTransform(OldSpaceBaseRefPose[OldBoneIndex]);

					// also savees the translation difference between old to new
					FVector OldTranslation = OldRefPose[OldBoneIndex].GetTranslation();
					FVector NewTranslation = NewRefPose[BoneIndex].GetTranslation();

					float OldTranslationSize = OldTranslation.Size();
					float NewTranslationSize = NewTranslation.Size();
					OldToNewTranslationRatio[BoneIndex] = (FMath::IsNearlyZero(OldTranslationSize))? 1.f/*do not touch new translation size*/ : NewTranslationSize/OldTranslationSize;
				}
				else
				{
					RelativeToNewTransform[BoneIndex].SetIdentity();
				}
			}

			// 2d array of animated time [boneindex][time key]
			TArray< TArray<FTransform> > AnimatedSpaceBases, ConvertedLocalSpaces, ConvertedSpaceBases;
			AnimatedSpaceBases.AddZeroed(NumBones);
			ConvertedLocalSpaces.AddZeroed(NumBones);
			ConvertedSpaceBases.AddZeroed(NumBones);

			int32 NumKeys = NumFrames;
			float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

			// allocate arrays
			for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
			{
				AnimatedSpaceBases[BoneIndex].AddUninitialized(NumKeys);
				ConvertedLocalSpaces[BoneIndex].AddUninitialized(NumKeys);
				ConvertedSpaceBases[BoneIndex].AddUninitialized(NumKeys);
			}

			// now calculating old animated space bases
			// this one calculates aniamted space per bones and per key
			for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
			{
				FName BoneName = NewSkeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
				int32 OldBoneIndex = OldSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
				int32 TrackIndex = AnimationTrackNames.Find(BoneName);
				int32 ParentBoneIndex = NewSkeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

				if(TrackIndex != INDEX_NONE)
				{
					auto& RawAnimation = RawAnimationData[TrackIndex];
					// fill up keys - calculate PK1 * K1
					for(int32 Key=0; Key<NumKeys; ++Key)
					{
						FTransform AnimatedLocalKey;
						ExtractBoneTransform(RawAnimationData, AnimatedLocalKey, TrackIndex, Interval*Key);

						// note that we apply scale in the animated space
						// at this point, you should have scaled version of animated skeleton
						AnimatedLocalKey.ScaleTranslation(OldToNewTranslationRatio[BoneIndex]);

						if(ParentBoneIndex != INDEX_NONE)
						{
							AnimatedSpaceBases[BoneIndex][Key] = AnimatedLocalKey * AnimatedSpaceBases[ParentBoneIndex][Key];
						}
						else
						{
							AnimatedSpaceBases[BoneIndex][Key] = AnimatedLocalKey;
						}
					}
				}
				else
				{
					// get local spaces from refpose and use that to fill it up
					FTransform LocalTransform = (OldBoneIndex != INDEX_NONE)? OldSkeleton->GetReferenceSkeleton().GetRefBonePose()[OldBoneIndex] : FTransform::Identity;

					for(int32 Key=0; Key<NumKeys; ++Key)
					{
						if(ParentBoneIndex != INDEX_NONE)
						{
							AnimatedSpaceBases[BoneIndex][Key] = LocalTransform * AnimatedSpaceBases[ParentBoneIndex][Key];
						}
						else
						{
							AnimatedSpaceBases[BoneIndex][Key] = LocalTransform;
						}
					}
				}
			}

			// now apply the theta back to the animated space bases
			TArray<struct FRawAnimSequenceTrack> NewRawAnimationData = RawAnimationData;
			for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
			{
				FName BoneName = NewSkeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
				int32 TrackIndex = AnimationTrackNames.Find(BoneName);
				int32 ParentBoneIndex = NewSkeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

				for(int32 Key=0; Key<NumKeys; ++Key)
				{
					// thus PK2 & K2 =  PK1 * K1 * theta where theta = (P1*R1)^(-1) * P2*R2
					// where PK2	: parent transform in component space of animated key for new skeleton
					//		 K2		: local transform of animated key for new skeleton
					//		 PK1	: parent transform in component space of animated key for old skeleton
					//		 K1		: local transform of animated key for old skeleton
					FTransform SpaceBase;
					// we don't just apply it because translation is sensitive
					// we don't like to apply relative transform to tranlsation directly
					// rotation and scale we can, but translation we'd like to use scaled translation instead of transformed location
					// as their relative translation can be different
					SpaceBase.SetRotation(AnimatedSpaceBases[BoneIndex][Key].GetRotation() * RelativeToNewTransform[BoneIndex].GetRotation());
					SpaceBase.SetScale3D(AnimatedSpaceBases[BoneIndex][Key].GetScale3D() * RelativeToNewTransform[BoneIndex].GetScale3D());
					// use animated scaled translation directly
					SpaceBase.SetTranslation(AnimatedSpaceBases[BoneIndex][Key].GetTranslation());
					ConvertedSpaceBases[BoneIndex][Key] = SpaceBase;
					// now calculate local space for animation
					if(ParentBoneIndex != INDEX_NONE)
					{
						// K2 = PK2^(-1) * PK1 * K1 * (P1*R1)^(-1) * P2*R2
						ConvertedLocalSpaces[BoneIndex][Key] = SpaceBase.GetRelativeTransform(ConvertedSpaceBases[ParentBoneIndex][Key]);
					}
					else
					{
						ConvertedLocalSpaces[BoneIndex][Key] = SpaceBase;
					}
				}

				// now save back to animation data
				if(TrackIndex != INDEX_NONE)
				{
					auto& RawAnimation = NewRawAnimationData[TrackIndex];
					RawAnimation.PosKeys.Empty(NumKeys);
					RawAnimation.PosKeys.AddUninitialized(NumKeys);
					RawAnimation.RotKeys.Empty(NumKeys);
					RawAnimation.RotKeys.AddUninitialized(NumKeys);
					RawAnimation.ScaleKeys.Empty(NumKeys);
					RawAnimation.ScaleKeys.AddUninitialized(NumKeys);

					for(int32 Key=0; Key<NumKeys; ++Key)
					{
						RawAnimation.PosKeys[Key] = ConvertedLocalSpaces[BoneIndex][Key].GetLocation();
						RawAnimation.RotKeys[Key] = ConvertedLocalSpaces[BoneIndex][Key].GetRotation();
						RawAnimation.ScaleKeys[Key] = ConvertedLocalSpaces[BoneIndex][Key].GetScale3D();
					}
				}
			}
			RawAnimationData = NewRawAnimationData;
		}
		else
		{
			// this only replaces the primary one, it doesn't replace old ones
			TArray<struct FTrackToSkeletonMap> NewTrackToSkeletonMapTable;
			NewTrackToSkeletonMapTable.Empty(AnimationTrackNames.Num());
			NewTrackToSkeletonMapTable.AddUninitialized(AnimationTrackNames.Num());
			for (int32 Track = 0; Track < AnimationTrackNames.Num(); ++Track)
			{
				int32 BoneIndex = NewSkeleton->GetReferenceSkeleton().FindBoneIndex(AnimationTrackNames[Track]);
				NewTrackToSkeletonMapTable[Track].BoneTreeIndex = BoneIndex;
			}

			// now I have all NewTrack To Skeleton Map Table
			// I'll need to compare with old tracks and copy over if SkeletonIndex == 0
			// if SkeletonIndex != 0, we need to see if we can 
			for (int32 TableId = 0; TableId < NewTrackToSkeletonMapTable.Num(); ++TableId)
			{
				if (ensure(TrackToSkeletonMapTable.IsValidIndex(TableId)))
				{
					if (NewTrackToSkeletonMapTable[TableId].BoneTreeIndex != INDEX_NONE)
					{
						TrackToSkeletonMapTable[TableId].BoneTreeIndex = NewTrackToSkeletonMapTable[TableId].BoneTreeIndex;
					}
					else
					{
						// if not found, delete the track data
						RemoveTrack(TableId);
						NewTrackToSkeletonMapTable.RemoveAt(TableId);
						--TableId;
					}
				}
			}
		}

		// I have to set this here in order for compression
		// that has to happen outside of this after Skeleton changes
		SetSkeleton(NewSkeleton);
	}
	else
	{
		VerifyTrackMap(NewSkeleton);
	}

	SetSkeleton(NewSkeleton);
}

void UAnimSequence::PostProcessSequence(bool bForceNewRawDatGuid)
{
	// pre process before compress raw animation data

	// if scale is too small, zero it out. Cause it hard to retarget when compress
	// inverse scale is applied to translation, and causing translation to be huge to retarget, but
	// compression can't handle that much precision. 
	for (auto Iter = RawAnimationData.CreateIterator(); Iter; ++Iter)
	{
		FRawAnimSequenceTrack& RawAnim = (*Iter);

		for (auto ScaleIter = RawAnim.ScaleKeys.CreateIterator(); ScaleIter; ++ScaleIter)
		{
			FVector& Scale3D = *ScaleIter;
			if ( FMath::IsNearlyZero(Scale3D.X) )
			{
				Scale3D.X = 0.f;
			}
			if ( FMath::IsNearlyZero(Scale3D.Y) )
			{
				Scale3D.Y = 0.f;
			}
			if ( FMath::IsNearlyZero(Scale3D.Z) )
			{
				Scale3D.Z = 0.f;
			}
		}

		// make sure Rotation part is normalized before compress
		for(auto RotIter = RawAnim.RotKeys.CreateIterator(); RotIter; ++RotIter)
		{
			FQuat& Rotation = *RotIter;
			if( !Rotation.IsNormalized() )
			{
				Rotation.Normalize();
			}
		}
	}

	CompressRawAnimData();
	// Apply compression
	MarkRawDataAsModified(bForceNewRawDatGuid);
	OnRawDataChanged();
	// initialize notify track
	InitializeNotifyTrack();
	//Make sure we dont have any notifies off the end of the sequence
	ClampNotifiesAtEndOfSequence();
	// mark package as dirty
	MarkPackageDirty();
}

void UAnimSequence::RemoveNaNTracks()
{
	bool bRecompress = false;

	for( int32 TrackIndex=0; TrackIndex<RawAnimationData.Num(); ++TrackIndex )
	{
		const FRawAnimSequenceTrack& RawTrack = RawAnimationData[TrackIndex];

		bool bContainsNaN = false;
		for ( auto Key : RawTrack.PosKeys )
		{
			bContainsNaN |= Key.ContainsNaN();
		}

		if (!bContainsNaN)
		{
			for(auto Key : RawTrack.RotKeys)
			{
				bContainsNaN |= Key.ContainsNaN();
			}
		}

		if (!bContainsNaN)
		{
			for(auto Key : RawTrack.ScaleKeys)
			{
				bContainsNaN |= Key.ContainsNaN();
			}
		}

		if (bContainsNaN)
		{
			UE_LOG(LogAnimation, Warning, TEXT("Animation raw data contains NaNs - Removing the following track [%s Track (%s)]"), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()), *AnimationTrackNames[TrackIndex].ToString());
			// remove this track
			RemoveTrack(TrackIndex);
			--TrackIndex;

			bRecompress = true;
		}
	}

	if(bRecompress)
	{
		MarkRawDataAsModified();
		OnRawDataChanged();
	}
}


void UAnimSequence::RemoveTrack(int32 TrackIndex)
{
	if (RawAnimationData.IsValidIndex(TrackIndex))
	{
		RawAnimationData.RemoveAt(TrackIndex);
		AnimationTrackNames.RemoveAt(TrackIndex);
		TrackToSkeletonMapTable.RemoveAt(TrackIndex);
		// source raw animation only exists if edited
		if (SourceRawAnimationData.IsValidIndex(TrackIndex))
		{
			SourceRawAnimationData.RemoveAt(TrackIndex);
		}

		check (RawAnimationData.Num() == AnimationTrackNames.Num() && AnimationTrackNames.Num() == TrackToSkeletonMapTable.Num() );
	}
}

int32 FindFirstChildTrack(const USkeleton* MySkeleton, const FReferenceSkeleton& RefSkeleton, const TArray<FName>& AnimationTrackNames, FName BoneName)
{
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	if(BoneIndex == INDEX_NONE)
	{
		// get out, nothing to do
		return INDEX_NONE;
	}

	// find children
	TArray<int32> Childs;
	if(MySkeleton->GetChildBones(BoneIndex, Childs) > 0)
	{
		// first look for direct children
		for(auto ChildIndex : Childs)
		{
			FName ChildBoneName = RefSkeleton.GetBoneName(ChildIndex);
			int32 ChildTrackIndex = AnimationTrackNames.Find(ChildBoneName);
			if(ChildTrackIndex != INDEX_NONE)
			{
				// found the new track
				return ChildTrackIndex;
			}
		}

		int32 BestGrandChildIndex = INDEX_NONE;
		// if you didn't find yet, now you have to go through all children
		for(auto ChildIndex : Childs)
		{
			FName ChildBoneName = RefSkeleton.GetBoneName(ChildIndex);
			// now I have to go through all childrewn and find who is earliest since I don't know which one might be the closest one
			int32 GrandChildIndex = FindFirstChildTrack(MySkeleton, RefSkeleton, AnimationTrackNames, ChildBoneName);
			if (GrandChildIndex != INDEX_NONE)
			{
				if (BestGrandChildIndex == INDEX_NONE)
				{
					BestGrandChildIndex = GrandChildIndex;
				}
				else if (BestGrandChildIndex > GrandChildIndex)
				{
					// best should be earlier track index
					BestGrandChildIndex = GrandChildIndex;
				}
			}
		}

		return BestGrandChildIndex;
	}
	else
	{
		// there is no child, just add at the end
		return AnimationTrackNames.Num();
	}
}

int32 UAnimSequence::InsertTrack(const FName& BoneName)
{
	// first verify if it doesn't exists, if it does, return
	int32 CurrentTrackIndex = AnimationTrackNames.Find(BoneName);
	if (CurrentTrackIndex != INDEX_NONE)
	{
		return CurrentTrackIndex;
	}

	USkeleton * MySkeleton = GetSkeleton();
	// should not call this if skeleton was empty
	if (ensure(MySkeleton) == false)
	{
		return INDEX_NONE;
	}

	const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
	int32 NewTrackIndex = FindFirstChildTrack(MySkeleton, RefSkeleton, AnimationTrackNames, BoneName);
	int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	if (NewTrackIndex != INDEX_NONE)
	{
		const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();

		FRawAnimSequenceTrack RawTrack;
		RawTrack.PosKeys.Add(RefPose[BoneIndex].GetTranslation());
		RawTrack.RotKeys.Add(RefPose[BoneIndex].GetRotation());
		RawTrack.ScaleKeys.Add(RefPose[BoneIndex].GetScale3D());

		// now insert to the track
		RawAnimationData.Insert(RawTrack, NewTrackIndex);
		AnimationTrackNames.Insert(BoneName, NewTrackIndex);
		SourceRawAnimationData.Insert(RawTrack, NewTrackIndex);

		RefreshTrackMapFromAnimTrackNames();

		check(RawAnimationData.Num() == AnimationTrackNames.Num() && AnimationTrackNames.Num() == TrackToSkeletonMapTable.Num());
	}

	return NewTrackIndex;
}

bool UAnimSequence::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive /*= true*/)
{
	Super::GetAllAnimationSequencesReferred(AnimationAssets, bRecursive);
	if (RefPoseSeq  && RefPoseSeq != this && !AnimationAssets.Contains(RefPoseSeq))
	{
		RefPoseSeq->HandleAnimReferenceCollection(AnimationAssets, bRecursive);
	}
	return AnimationAssets.Num() > 0;
}

void UAnimSequence::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap)
{
	Super::ReplaceReferredAnimations(ReplacementMap);

	if (RefPoseSeq)
	{
		UAnimSequence* const* ReplacementAsset = (UAnimSequence*const*)ReplacementMap.Find(RefPoseSeq);
		if (ReplacementAsset)
		{
			RefPoseSeq = *ReplacementAsset;
		}
	}
}

bool UAnimSequence::AddLoopingInterpolation()
{
	int32 NumTracks = AnimationTrackNames.Num();
	float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

	if(NumFrames > 0)
	{
		// added one more key
		int32 NewNumKeys = NumFrames +1 ;

		// now I need to calculate back to new animation data
		for(int32 TrackIndex=0; TrackIndex<NumTracks; ++TrackIndex)
		{
			auto& RawAnimation = RawAnimationData[TrackIndex];
			if (RawAnimation.PosKeys.Num() > 1)
			{
				FVector FirstKey = RawAnimation.PosKeys[0];
				RawAnimation.PosKeys.Add(FirstKey);
			}

			if(RawAnimation.RotKeys.Num() > 1)
			{
				FQuat FirstKey = RawAnimation.RotKeys[0];
				RawAnimation.RotKeys.Add(FirstKey);
			}

			if(RawAnimation.ScaleKeys.Num() > 1)
			{
				FVector FirstKey = RawAnimation.ScaleKeys[0];
				RawAnimation.ScaleKeys.Add(FirstKey);
			}
		}

		SequenceLength += Interval;
		NumFrames = NewNumKeys;

		PostProcessSequence();
		return true;
	}

	return false;
}
int32 FindParentNodeIndex(URig* Rig, USkeleton* Skeleton, FName ParentNodeName)
{
	const int32& ParentNodeIndex = Rig->FindNode(ParentNodeName);
	const FName& ParentBoneName = Skeleton->GetRigBoneMapping(ParentNodeName);
	
	return Skeleton->GetReferenceSkeleton().FindBoneIndex(ParentBoneName);
}

int32 UAnimSequence::GetSpaceBasedAnimationData(TArray< TArray<FTransform> >& AnimationDataInComponentSpace, FAnimSequenceTrackContainer * RiggingAnimationData) const
{
	USkeleton* MySkeleton = GetSkeleton();

	check(MySkeleton);
	const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
	int32 NumBones = RefSkeleton.GetNum();

	AnimationDataInComponentSpace.Empty(NumBones);
	AnimationDataInComponentSpace.AddZeroed(NumBones);

	// 2d array of animated time [boneindex][time key]
	int32 NumKeys = NumFrames;
	float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

	// allocate arrays
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		AnimationDataInComponentSpace[BoneIndex].AddUninitialized(NumKeys);
	}

	if (RiggingAnimationData)
	{
		const URig* Rig = MySkeleton->GetRig();

		check(Rig);

		// to fix the issue where parent of rig doesn't correspond to parent of this skeleton
		// we do this in multiple iteration if needed. 
		// this flag will be used to evaluate all of them until done
		TArray<bool> BoneEvaluated;
		BoneEvaluated.AddZeroed(NumBones);

		bool bCompleted = false;
		do
		{
			for(int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				if ( !BoneEvaluated[BoneIndex] )
				{
					const FName& BoneName = RefSkeleton.GetBoneName(BoneIndex);
					const FName& NodeName = MySkeleton->GetRigNodeNameFromBoneName(BoneName);
					const FTransformBase* TransformBase = Rig->GetTransformBaseByNodeName(NodeName);
					const int32 NodeIndex = RiggingAnimationData->TrackNames.Find(NodeName);
					if(NodeIndex != INDEX_NONE)
					{
						check(TransformBase);

						// now calculate the component space
						const TArray<FRigTransformConstraint>	& RotTransformConstraints = TransformBase->Constraints[EControlConstraint::Type::Orientation].TransformConstraints;

						FQuat ComponentRotation;
						FTransform ComponentTranslation;
						FVector ComponentScale;

						// rotation first
						// this is easy since we just make sure it's evaluated or not
						{
							const FName& ParentNodeName = RotTransformConstraints[0].ParentSpace;
							const FName& ParentBoneName = MySkeleton->GetRigBoneMapping(ParentNodeName);
							const int32& ParentBoneIndex = RefSkeleton.FindBoneIndex(ParentBoneName);

							if(ParentBoneIndex != INDEX_NONE)
							{
								if (BoneEvaluated[ParentBoneIndex])
								{
									for(int32 Key = 0; Key < NumKeys; ++Key)
									{
										ComponentRotation = AnimationDataInComponentSpace[ParentBoneIndex][Key].GetRotation() * RiggingAnimationData->AnimationTracks[NodeIndex].RotKeys[Key];
										AnimationDataInComponentSpace[BoneIndex][Key].SetRotation(ComponentRotation);
									}

									BoneEvaluated[BoneIndex] = true;
								}
							}
							else
							{
								for(int32 Key = 0; Key < NumKeys; ++Key)
								{
									ComponentRotation = RiggingAnimationData->AnimationTracks[NodeIndex].RotKeys[Key];
									AnimationDataInComponentSpace[BoneIndex][Key].SetRotation(ComponentRotation);
								}

								BoneEvaluated[BoneIndex] = true;
							}
						}

						const TArray<FRigTransformConstraint>	& PosTransformConstraints = TransformBase->Constraints[EControlConstraint::Type::Translation].TransformConstraints;

						// now time to check translation
						// this is a bit more complicated
						// since we have to make sure if it's true to start with
						// did we succeed on getting rotation?
						if (BoneEvaluated[BoneIndex])
						{
							const FName& ParentNodeName = PosTransformConstraints[0].ParentSpace;
							const FName& ParentBoneName = MySkeleton->GetRigBoneMapping(ParentNodeName);
							const int32& ParentBoneIndex = RefSkeleton.FindBoneIndex(ParentBoneName);

							if(ParentBoneIndex != INDEX_NONE)
							{
								// this has to be check
								if (BoneEvaluated[ParentBoneIndex])
								{
									for(int32 Key = 0; Key < NumKeys; ++Key)
									{
										const FTransform& AnimCompSpace = AnimationDataInComponentSpace[ParentBoneIndex][Key];
										ComponentTranslation = FTransform(RiggingAnimationData->AnimationTracks[NodeIndex].PosKeys[Key]) * AnimCompSpace;
										AnimationDataInComponentSpace[BoneIndex][Key].SetTranslation(ComponentTranslation.GetTranslation());

										ComponentScale = AnimCompSpace.GetScale3D() * RiggingAnimationData->AnimationTracks[NodeIndex].ScaleKeys[Key];
										AnimationDataInComponentSpace[BoneIndex][Key].SetScale3D(ComponentScale);
									}
								}
								else
								{
									// if we failed to get parent clear the flag
									// because if translation has been calculated, BoneEvaluated[BoneIndex] might be true
									BoneEvaluated[BoneIndex] = false;
								}
							}
							else
							{
								for(int32 Key = 0; Key < NumKeys; ++Key)
								{
									ComponentTranslation = FTransform(RiggingAnimationData->AnimationTracks[NodeIndex].PosKeys[Key]);
									AnimationDataInComponentSpace[BoneIndex][Key].SetTranslation(ComponentTranslation.GetTranslation());
									
									ComponentScale = RiggingAnimationData->AnimationTracks[NodeIndex].ScaleKeys[Key];
									AnimationDataInComponentSpace[BoneIndex][Key].SetScale3D(ComponentScale);
								}
							}
						}
					}
					else
					{
						int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
						const FTransform& LocalSpace = RefSkeleton.GetRefBonePose()[BoneIndex];
						if(ParentIndex != INDEX_NONE)
						{
							// if parent is evaluated, do it
							if (BoneEvaluated[ParentIndex])
							{
								for(int32 Key = 0; Key < NumKeys; ++Key)
								{
									AnimationDataInComponentSpace[BoneIndex][Key] = LocalSpace * AnimationDataInComponentSpace[ParentIndex][Key];
								}

								BoneEvaluated[BoneIndex] = true;
							}
						}
						else
						{
							BoneEvaluated[BoneIndex] = true;

							for(int32 Key = 0; Key < NumKeys; ++Key)
							{
								AnimationDataInComponentSpace[BoneIndex][Key] = LocalSpace;
							}
						}
					}
				}
			}

			bCompleted = true;
			// see if we can get out, brute force for now
			for(int32 BoneIndex = 0; BoneIndex < NumBones && bCompleted; ++BoneIndex)
			{
				bCompleted &= !!BoneEvaluated[BoneIndex];
			}
		} while (bCompleted == false);
	}
	else
	{
		// now calculating old animated space bases
		// this one calculates aniamted space per bones and per key
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			FName BoneName = MySkeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
			int32 TrackIndex = AnimationTrackNames.Find(BoneName);
			int32 ParentBoneIndex = MySkeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

			if (TrackIndex != INDEX_NONE)
			{
				auto& RawAnimation = RawAnimationData[TrackIndex];
				// fill up keys - calculate PK1 * K1
				for (int32 Key = 0; Key < NumKeys; ++Key)
				{
					FTransform AnimatedLocalKey;
					ExtractBoneTransform(RawAnimationData, AnimatedLocalKey, TrackIndex, Interval*Key);

					if (ParentBoneIndex != INDEX_NONE)
					{
						AnimationDataInComponentSpace[BoneIndex][Key] = AnimatedLocalKey * AnimationDataInComponentSpace[ParentBoneIndex][Key];
					}
					else
					{
						AnimationDataInComponentSpace[BoneIndex][Key] = AnimatedLocalKey;
					}
				}
			}
			else
			{
				// get local spaces from refpose and use that to fill it up
				FTransform LocalTransform = MySkeleton->GetReferenceSkeleton().GetRefBonePose()[BoneIndex];

				for (int32 Key = 0; Key < NumKeys; ++Key)
				{
					if (ParentBoneIndex != INDEX_NONE)
					{
						AnimationDataInComponentSpace[BoneIndex][Key] = LocalTransform * AnimationDataInComponentSpace[ParentBoneIndex][Key];
					}
					else
					{
						AnimationDataInComponentSpace[BoneIndex][Key] = LocalTransform;
					}
				}
			}	
		}

	}

	return AnimationDataInComponentSpace.Num();
}

bool UAnimSequence::ConvertAnimationDataToRiggingData(FAnimSequenceTrackContainer& RiggingAnimationData)
{
	USkeleton* MySkeleton = GetSkeleton();
	if (MySkeleton && MySkeleton->GetRig())
	{
		const URig* Rig = MySkeleton->GetRig();
		TArray<FName> ValidNodeNames;
		int32 NumNodes = MySkeleton->GetMappedValidNodes(ValidNodeNames);
		TArray< TArray<FTransform> > AnimationDataInComponentSpace;
		int32 NumBones = GetSpaceBasedAnimationData(AnimationDataInComponentSpace, NULL);

		if (NumBones > 0)
		{
			RiggingAnimationData.Initialize(ValidNodeNames);

			// first we copy all space bases back to it
			for (int32 NodeIndex = 0; NodeIndex < NumNodes; ++NodeIndex)
			{
				struct FRawAnimSequenceTrack& Track = RiggingAnimationData.AnimationTracks[NodeIndex];
				const FName& NodeName = ValidNodeNames[NodeIndex];
				const FName& BoneName = MySkeleton->GetRigBoneMapping(NodeName);
				const int32& BoneIndex = MySkeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);

				if (ensure(BoneIndex != INDEX_NONE))
				{
					Track.PosKeys.Empty(NumFrames);
					Track.RotKeys.Empty(NumFrames);
					Track.ScaleKeys.Empty(NumFrames);
					Track.PosKeys.AddUninitialized(NumFrames);
					Track.RotKeys.AddUninitialized(NumFrames);
					Track.ScaleKeys.AddUninitialized(NumFrames);

					int32 RigConstraintIndex = Rig->FindTransformBaseByNodeName(NodeName);

					if (RigConstraintIndex != INDEX_NONE)
					{
						const auto* RigConstraint = Rig->GetTransformBase(RigConstraintIndex);

						// apply orientation - for now only one
						const TArray<FRigTransformConstraint>& RotationTransformConstraint = RigConstraint->Constraints[EControlConstraint::Type::Orientation].TransformConstraints;

						if (RotationTransformConstraint.Num() > 0)
						{
							const FName& ParentSpace = RotationTransformConstraint[0].ParentSpace;
							const FName& ParentBoneName = MySkeleton->GetRigBoneMapping(ParentSpace);
							const int32& ParentBoneIndex = MySkeleton->GetReferenceSkeleton().FindBoneIndex(ParentBoneName);
							if (ParentBoneIndex != INDEX_NONE)
							{
								// if no rig control, component space is used
								for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
								{
									FTransform ParentTransform = AnimationDataInComponentSpace[ParentBoneIndex][KeyIndex];
									FTransform RelativeTransform = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRelativeTransform(ParentTransform);
									Track.RotKeys[KeyIndex] = RelativeTransform.GetRotation();
								}
							}
							else
							{
								// if no rig control, component space is used
								for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
								{
									Track.RotKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRotation();
								}
							}
						}
						else
						{
							// if no rig control, component space is used
							for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
							{
								Track.RotKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRotation();
							}
						}

						// apply translation - for now only one
						const TArray<FRigTransformConstraint>& TranslationTransformConstraint = RigConstraint->Constraints[EControlConstraint::Type::Translation].TransformConstraints;

						if (TranslationTransformConstraint.Num() > 0)
						{
							const FName& ParentSpace = TranslationTransformConstraint[0].ParentSpace;
							const FName& ParentBoneName = MySkeleton->GetRigBoneMapping(ParentSpace);
							const int32& ParentBoneIndex = MySkeleton->GetReferenceSkeleton().FindBoneIndex(ParentBoneName);
							if (ParentBoneIndex != INDEX_NONE)
							{
								// if no rig control, component space is used
								for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
								{
									FTransform ParentTransform = AnimationDataInComponentSpace[ParentBoneIndex][KeyIndex];
									FTransform RelativeTransform = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRelativeTransform(ParentTransform);
									Track.PosKeys[KeyIndex] = RelativeTransform.GetTranslation();
									Track.ScaleKeys[KeyIndex] = RelativeTransform.GetScale3D();
								}
							}
							else
							{
								for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
								{
									Track.PosKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetTranslation();
									Track.ScaleKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetScale3D();
								}
							}
						}
						else
						{
							for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
							{
								Track.PosKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetTranslation();
								Track.ScaleKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetScale3D();
							}
						}
					}
					else
					{
						// if no rig control, component space is used
						for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
						{
							Track.PosKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetTranslation();
							Track.RotKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRotation();
							Track.ScaleKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetScale3D();
						}
					}
				}
			}
		}

		return true;
	}

	return false;
}

bool UAnimSequence::ConvertRiggingDataToAnimationData(FAnimSequenceTrackContainer& RiggingAnimationData)
{
	if (RiggingAnimationData.GetNum() > 0)
	{
		TArray< TArray<FTransform> > AnimationDataInComponentSpace;
		int32 NumBones = GetSpaceBasedAnimationData(AnimationDataInComponentSpace, &RiggingAnimationData);

		USkeleton* MySkeleton = GetSkeleton();
		TArray<FRawAnimSequenceTrack> OldAnimationData = RawAnimationData;
		TArray<FName> OldAnimationTrackNames = AnimationTrackNames;
		TArray<FName> ValidNodeNames;
		MySkeleton->GetMappedValidNodes(ValidNodeNames);
		// remove from ValidNodeNames if it doesn't belong to AnimationTrackNames
		for (int32 NameIndex=0; NameIndex<ValidNodeNames.Num(); ++NameIndex)
		{
			if (RiggingAnimationData.TrackNames.Contains(ValidNodeNames[NameIndex]) == false)
			{
				ValidNodeNames.RemoveAt(NameIndex);
				--NameIndex;
			}
		}

		int32 ValidNumNodes = ValidNodeNames.Num();

		// get local spaces
		// add all tracks?
		AnimationTrackNames.Empty(ValidNumNodes);
		AnimationTrackNames.AddUninitialized(ValidNumNodes);
		RawAnimationData.Empty(ValidNumNodes);
		RawAnimationData.AddZeroed(ValidNumNodes);

		// if source animation exists, clear it, it won't matter anymore
		if (SourceRawAnimationData.Num() > 0)
		{
			ClearBakedTransformData();
		}

		const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
		const URig* Rig = MySkeleton->GetRig();
		for (int32 NodeIndex = 0; NodeIndex < ValidNumNodes; ++NodeIndex)
		{
			FName BoneName = MySkeleton->GetRigBoneMapping(ValidNodeNames[NodeIndex]);
			int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);

			if (BoneIndex != INDEX_NONE)
			{
				// add track names
				AnimationTrackNames[NodeIndex] = BoneName;

				// update bone trasfnrom
				FRawAnimSequenceTrack& Track = RawAnimationData[NodeIndex];

				Track.PosKeys.Empty();
				Track.RotKeys.Empty();
				Track.ScaleKeys.Empty();
				Track.PosKeys.AddUninitialized(NumFrames);
				Track.RotKeys.AddUninitialized(NumFrames);
				Track.ScaleKeys.AddUninitialized(NumFrames);

				const int32& ParentBoneIndex = RefSkeleton.GetParentIndex(BoneIndex);

				if(ParentBoneIndex != INDEX_NONE)
				{
					for(int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
					{
						FTransform LocalTransform = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRelativeTransform(AnimationDataInComponentSpace[ParentBoneIndex][KeyIndex]);

						Track.PosKeys[KeyIndex] = LocalTransform.GetTranslation();
						Track.RotKeys[KeyIndex] = LocalTransform.GetRotation();
						Track.ScaleKeys[KeyIndex] = LocalTransform.GetScale3D();
					}
				}
				else
				{
					for(int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
					{
						FTransform LocalTransform = AnimationDataInComponentSpace[BoneIndex][KeyIndex];

						Track.PosKeys[KeyIndex] = LocalTransform.GetTranslation();
						Track.RotKeys[KeyIndex] = LocalTransform.GetRotation();
						Track.ScaleKeys[KeyIndex] = LocalTransform.GetScale3D();
					}
				}
			}
		}

		// recreate track map
		TrackToSkeletonMapTable.Empty(AnimationTrackNames.Num());
		TrackToSkeletonMapTable.AddUninitialized(AnimationTrackNames.Num());
		int32 TrackIdx = 0;
		for (auto TrackName : AnimationTrackNames)
		{
			TrackToSkeletonMapTable[TrackIdx++].BoneTreeIndex = MySkeleton->GetReferenceSkeleton().FindBoneIndex(TrackName);
		}
		PostProcessSequence();

		return true;
	}

	return false;
}

void UAnimSequence::ClearBakedTransformData()
{
	UE_LOG(LogAnimation, Warning, TEXT("[%s] Detected previous edited data is invalidated. Clearing transform curve data and Source Data. This can happen if you do retarget another animation to this. If not, please report back to Epic. "), *GetName());
	SourceRawAnimationData.Empty();
	//Clear Transform curve data
	RawCurveData.DeleteAllCurveData(ERawCurveTrackTypes::RCT_Transform);
}

void UAnimSequence::BakeTrackCurvesToRawAnimation()
{
	// now bake the curves to the RawAnimationData
	if(NumFrames == 0)
	{
		// fail error?
		return;
	}

	if (!DoesContainTransformCurves())
	{
		if (SourceRawAnimationData.Num() > 0)
		{
			// if curve doesn't exists, we just bring back Source to Raw, and clears Source
			RawAnimationData = MoveTemp(SourceRawAnimationData);
			PostProcessSequence();
		}
	}
	else
	{
		if(SourceRawAnimationData.Num() == 0)
		{
			// if source data is empty, this is first time
			// copies the data
			SourceRawAnimationData = RawAnimationData;
		}
		else
		{
			// we copy SourceRawAnimationData because we'd need to create additive on top of current one
			RawAnimationData = SourceRawAnimationData;
		}

		USkeleton * CurSkeleton = GetSkeleton();
		check(CurSkeleton);

		const FSmartNameMapping* NameMapping = CurSkeleton->GetSmartNameContainer(USkeleton::AnimTrackCurveMappingName);
		// if no mapping, that means there is no transform curves
		if(!NameMapping)
		{
			// if no name mapping is found but curve exists, we should verify curve namex
			VerifyCurveNames<FTransformCurve>(*CurSkeleton, USkeleton::AnimTrackCurveMappingName, RawCurveData.TransformCurves);
			NameMapping = CurSkeleton->GetSmartNameContainer(USkeleton::AnimTrackCurveMappingName);
		}
		
		// since now I'm about to modify Scale Keys. I should add all of them here at least one key. 
		// if all turns out to be same, it will clear it up. 
		for (auto & RawTrack: RawAnimationData)
		{
			if (RawTrack.ScaleKeys.Num() == 0)
			{
				// at least add one
				static FVector ScaleConstantKey(1.f);
				RawTrack.ScaleKeys.Add(ScaleConstantKey);
			}
		}

		for(const auto& Curve : RawCurveData.TransformCurves)
		{
			// find curves first, and then see what is index of this curve
			FName BoneName;

			if(Curve.GetCurveTypeFlag(AACF_Disabled)== false &&
				ensureAlways(NameMapping->GetName(Curve.Name.UID, BoneName)))
			{
				int32 TrackIndex = AnimationTrackNames.Find(BoneName);

				// the animation data doesn't have this track, so insert it
				if(TrackIndex == INDEX_NONE)
				{
					TrackIndex = InsertTrack(BoneName);
					// if it still didn't find, something went horribly wrong
					if(ensure(TrackIndex != INDEX_NONE) == false)
					{
						UE_LOG(LogAnimation, Warning, TEXT("Animation Baking : Error adding %s track."), *BoneName.ToString());
						// I can't do anything about it
						continue;
					}
				}

				// now modify data
				auto& RawTrack = RawAnimationData[TrackIndex];

				// since now we're editing keys, 
				// if 1 (which meant constant), just expands to # of frames
				if(RawTrack.PosKeys.Num() == 1)
				{
					FVector OneKey = RawTrack.PosKeys[0];
					RawTrack.PosKeys.Init(OneKey, NumFrames);
				}
				else
				{
					ensure(RawTrack.PosKeys.Num() == NumFrames);
				}

				if(RawTrack.RotKeys.Num() == 1)
				{
					FQuat OneKey = RawTrack.RotKeys[0];
					RawTrack.RotKeys.Init(OneKey, NumFrames);
				}
				else
				{
					ensure(RawTrack.RotKeys.Num() == NumFrames);
				}

				// although we don't allow edit of scale
				// it is important to consider scale when apply transform
				// so make sure this also is included
				if(RawTrack.ScaleKeys.Num() == 1)
				{
					FVector OneKey = RawTrack.ScaleKeys[0];
					RawTrack.ScaleKeys.Init(OneKey, NumFrames);
				}
				else
				{
					ensure(RawTrack.ScaleKeys.Num() == NumFrames);
				}

				// NumFrames can't be zero (filtered earlier)
				float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

				// now we have all data ready to apply
				for(int32 KeyIndex=0; KeyIndex < NumFrames; ++KeyIndex)
				{
					// now evaluate
					FTransformCurve* TransformCurve = static_cast<FTransformCurve*>(RawCurveData.GetCurveData(Curve.Name.UID, ERawCurveTrackTypes::RCT_Transform));

					if(ensure(TransformCurve))
					{
						FTransform AdditiveTransform = TransformCurve->Evaluate(KeyIndex * Interval, 1.0);
						FTransform LocalTransform(RawTrack.RotKeys[KeyIndex], RawTrack.PosKeys[KeyIndex], RawTrack.ScaleKeys[KeyIndex]);
						//  						LocalTransform = LocalTransform * AdditiveTransform;
						//  						RawTrack.RotKeys[KeyIndex] = LocalTransform.GetRotation();
						//  						RawTrack.PosKeys[KeyIndex] = LocalTransform.GetTranslation();
						//  						RawTrack.ScaleKeys[KeyIndex] = LocalTransform.GetScale3D();

						RawTrack.RotKeys[KeyIndex] = LocalTransform.GetRotation() * AdditiveTransform.GetRotation();
						RawTrack.PosKeys[KeyIndex] = LocalTransform.TransformPosition(AdditiveTransform.GetTranslation());
						RawTrack.ScaleKeys[KeyIndex] = LocalTransform.GetScale3D() * AdditiveTransform.GetScale3D();
					}
					else
					{
						UE_LOG(LogAnimation, Warning, TEXT("Animation Baking : Missing Curve for %s."), *BoneName.ToString());
					}
				}
			}
		}

		PostProcessSequence();
	}

	bNeedsRebake = false;
}

bool UAnimSequence::DoesNeedRebake() const
{
	return (bNeedsRebake);
}

bool UAnimSequence::DoesContainTransformCurves() const
{
	return (RawCurveData.TransformCurves.Num() > 0);
}

void UAnimSequence::AddKeyToSequence(float Time, const FName& BoneName, const FTransform& AdditiveTransform)
{
	// if source animation exists, but doesn't match with raw animation number, it's possible this has been retargetted
	// or for any other reason, track has been modified. Just log here. 
	if (SourceRawAnimationData.Num()>0 && SourceRawAnimationData.Num() != RawAnimationData.Num())
	{
		// currently it contains invalid data to edit
		// clear and start over
		ClearBakedTransformData();
	}

	// find if this already exists, then just add curve data only
	FName CurveName = BoneName;
	USkeleton * CurrentSkeleton = GetSkeleton();
	check (CurrentSkeleton);

	FSmartName NewCurveName;
	CurrentSkeleton->AddSmartNameAndModify(USkeleton::AnimTrackCurveMappingName, CurveName, NewCurveName);

	// add curve - this won't add duplicate curve
	RawCurveData.AddCurveData(NewCurveName, AACF_DriveTrack | AACF_Editable, ERawCurveTrackTypes::RCT_Transform);

	//Add this curve
	FTransformCurve* TransformCurve = static_cast<FTransformCurve*>(RawCurveData.GetCurveData(NewCurveName.UID, ERawCurveTrackTypes::RCT_Transform));
	check(TransformCurve);

	TransformCurve->UpdateOrAddKey(AdditiveTransform, Time);	

	bNeedsRebake = true;
}

void UAnimSequence::ResetAnimation()
{
	// clear everything. Making new animation, so need to reset all the things that belong here
	NumFrames = 0;
	SequenceLength = 0.f;
	RawAnimationData.Empty();
	SourceRawAnimationData.Empty();
	AnimationTrackNames.Empty();
	TrackToSkeletonMapTable.Empty();
	CompressedTrackOffsets.Empty();
	CompressedScaleOffsets.Empty();
	CompressedByteStream.Empty();

	Notifies.Empty();
	AuthoredSyncMarkers.Empty();
	UniqueMarkerNames.Empty();
	AnimNotifyTracks.Empty();
	RawCurveData.Empty();
	RateScale = 1.f;
}

void UAnimSequence::RefreshTrackMapFromAnimTrackNames()
{
	TrackToSkeletonMapTable.Empty();

	const USkeleton * MySkeleton = GetSkeleton();
	const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
	const int32 NumBones = AnimationTrackNames.Num();
	TrackToSkeletonMapTable.AddUninitialized(NumBones);

	bool bNeedsFixing = false;
	const int32 NumTracks = AnimationTrackNames.Num();
	for(int32 I=NumTracks-1; I>=0; --I)
	{
		int32 BoneTreeIndex = RefSkeleton.FindBoneIndex(AnimationTrackNames[I]);
		if(BoneTreeIndex == INDEX_NONE)
		{
			RemoveTrack(I);
		}
		else
		{
			TrackToSkeletonMapTable[I].BoneTreeIndex = BoneTreeIndex;
		}
	}
}

uint8* UAnimSequence::FindSyncMarkerPropertyData(int32 SyncMarkerIndex, UArrayProperty*& ArrayProperty)
{
	ArrayProperty = NULL;

	if (AuthoredSyncMarkers.IsValidIndex(SyncMarkerIndex))
	{
		return FindArrayProperty(TEXT("AuthoredSyncMarkers"), ArrayProperty, SyncMarkerIndex);
	}
	return NULL;
}

bool UAnimSequence::CreateAnimation(USkeletalMesh* Mesh)
{
	// create animation from Mesh's ref pose
	if (Mesh)
	{
		ResetAnimation();

		const FReferenceSkeleton& RefSkeleton = Mesh->RefSkeleton;
		SequenceLength = MINIMUM_ANIMATION_LENGTH;
		NumFrames = 1;

		const int32 NumBones = RefSkeleton.GetRawBoneNum();
		RawAnimationData.AddZeroed(NumBones);
		AnimationTrackNames.AddUninitialized(NumBones);

		const TArray<FTransform>& RefBonePose = RefSkeleton.GetRawRefBonePose();

		check (RefBonePose.Num() == NumBones);

		for (int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
		{
			AnimationTrackNames[BoneIndex] = RefSkeleton.GetBoneName(BoneIndex);

			FRawAnimSequenceTrack& RawTrack = RawAnimationData[BoneIndex];

			RawTrack.PosKeys.Add(RefBonePose[BoneIndex].GetTranslation());
			RawTrack.RotKeys.Add(RefBonePose[BoneIndex].GetRotation());
			RawTrack.ScaleKeys.Add(RefBonePose[BoneIndex].GetScale3D());
		}

		// refresh TrackToskeletonMapIndex
		RefreshTrackMapFromAnimTrackNames();

		// should recreate track map
		PostProcessSequence();
		return true;
	}

	return false;
}

bool UAnimSequence::CreateAnimation(USkeletalMeshComponent* MeshComponent)
{
	if(MeshComponent && MeshComponent->SkeletalMesh)
	{
		USkeletalMesh * Mesh = MeshComponent->SkeletalMesh;

		ResetAnimation();

		const FReferenceSkeleton& RefSkeleton = Mesh->RefSkeleton;
		SequenceLength = MINIMUM_ANIMATION_LENGTH;
		NumFrames = 1;

		const int32 NumBones = RefSkeleton.GetRawBoneNum();
		RawAnimationData.AddZeroed(NumBones);
		AnimationTrackNames.AddUninitialized(NumBones);

		const TArray<FTransform>& BoneSpaceTransforms = MeshComponent->BoneSpaceTransforms;

		check(BoneSpaceTransforms.Num() >= NumBones);

		for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
		{
			AnimationTrackNames[BoneIndex] = RefSkeleton.GetBoneName(BoneIndex);

			FRawAnimSequenceTrack& RawTrack = RawAnimationData[BoneIndex];

			RawTrack.PosKeys.Add(BoneSpaceTransforms[BoneIndex].GetTranslation());
			RawTrack.RotKeys.Add(BoneSpaceTransforms[BoneIndex].GetRotation());
			RawTrack.ScaleKeys.Add(BoneSpaceTransforms[BoneIndex].GetScale3D());
		}

		// refresh TrackToskeletonMapIndex
		RefreshTrackMapFromAnimTrackNames();

		// should recreate track map
		PostProcessSequence();
		return true;
	}

	return false;
}

bool UAnimSequence::CreateAnimation(UAnimSequence* Sequence)
{
	if(Sequence)
	{
		ResetAnimation();

		SequenceLength = Sequence->SequenceLength;
		NumFrames = Sequence->NumFrames;

		RawAnimationData = Sequence->RawAnimationData;
		AnimationTrackNames = Sequence->AnimationTrackNames;

		Notifies = Sequence->Notifies;
		AnimNotifyTracks = Sequence->AnimNotifyTracks;
		RawCurveData = Sequence->RawCurveData;
		// keep the same setting as source
		bNeedsRebake = Sequence->DoesNeedRebake();
		SourceRawAnimationData = Sequence->SourceRawAnimationData;

		// refresh TrackToskeletonMapIndex
		RefreshTrackMapFromAnimTrackNames();

		// should recreate track map
		PostProcessSequence();
		return true;
	}
	
	return false;
}

#endif

void UAnimSequence::RefreshCacheData()
{
	SortSyncMarkers();
#if WITH_EDITOR
	for (int32 TrackIndex = 0; TrackIndex < AnimNotifyTracks.Num(); ++TrackIndex)
	{
		AnimNotifyTracks[TrackIndex].SyncMarkers.Empty();
	}
	for (FAnimSyncMarker& SyncMarker : AuthoredSyncMarkers)
	{
		const int32 TrackIndex = SyncMarker.TrackIndex;
		if (AnimNotifyTracks.IsValidIndex(TrackIndex))
		{
			AnimNotifyTracks[TrackIndex].SyncMarkers.Add(&SyncMarker);
		}
		else
		{
			// This should not happen, but if it does we must find somewhere else to add it
			ensureMsgf(0, TEXT("AnimNotifyTrack: Wrong indices found"));
			AnimNotifyTracks[0].SyncMarkers.Add(&SyncMarker);
			SyncMarker.TrackIndex = 0;
		}
	}
#endif
	Super::RefreshCacheData();
}

void UAnimSequence::EvaluateCurveData(FBlendedCurve& OutCurve, float CurrentTime, bool bForceUseRawData) const
{
	if (bUseRawDataOnly || bForceUseRawData)
	{
		Super::EvaluateCurveData(OutCurve, CurrentTime);
	}
	else
	{
		CompressedCurveData.EvaluateCurveData(OutCurve, CurrentTime);
	}
}

const FRawCurveTracks& UAnimSequence::GetCurveData() const
{
	if (bUseRawDataOnly)
	{
		return Super::GetCurveData();
	}
	else
	{
		return CompressedCurveData;
	}
}

void UAnimSequence::RefreshSyncMarkerDataFromAuthored()
{
#if WITH_EDITOR
	MarkerDataUpdateCounter++;
#endif

	if (AuthoredSyncMarkers.Num() > 0)
	{
		UniqueMarkerNames.Reset();
		UniqueMarkerNames.Reserve(AuthoredSyncMarkers.Num());

		const FAnimSyncMarker* PreviousMarker = nullptr;
		for (const FAnimSyncMarker& Marker : AuthoredSyncMarkers)
		{
			UniqueMarkerNames.AddUnique(Marker.MarkerName);
			PreviousMarker = &Marker;
		}
	}
	else
	{
		UniqueMarkerNames.Empty();
	}
}

bool IsMarkerValid(const FAnimSyncMarker* Marker, bool bLooping, const TArray<FName>& ValidMarkerNames)
{
	return (Marker == nullptr && !bLooping) || (Marker && ValidMarkerNames.Contains(Marker->MarkerName));
}

void UAnimSequence::AdvanceMarkerPhaseAsLeader(bool bLooping, float MoveDelta, const TArray<FName>& ValidMarkerNames, float& CurrentTime, FMarkerPair& PrevMarker, FMarkerPair& NextMarker, TArray<FPassedMarker>& MarkersPassed) const
{
	check(MoveDelta != 0.f);
	const bool bPlayingForwards = MoveDelta > 0.f;
	float CurrentMoveDelta = MoveDelta * RateScale;

	bool bOffsetInitialized = false;
	float MarkerTimeOffset = 0.f;

	// Hard to reproduce issue triggering this, ensure & clamp for now
	ensureMsgf(CurrentTime >= 0.f && CurrentTime <= SequenceLength, TEXT("Current time inside of AdvanceMarkerPhaseAsLeader is out of range %.3f of 0.0 to %.3f\n    Sequence: %s"), CurrentTime, SequenceLength, *GetFullName());

	CurrentTime = FMath::Clamp(CurrentTime, 0.f, SequenceLength);

	if (bPlayingForwards)
	{
		while (true)
		{
			if (NextMarker.MarkerIndex == -1)
			{
				float PrevCurrentTime = CurrentTime;
				CurrentTime = FMath::Min(CurrentTime + CurrentMoveDelta, SequenceLength);
				NextMarker.TimeToMarker = SequenceLength - CurrentTime;
				PrevMarker.TimeToMarker -= CurrentTime - PrevCurrentTime; //Add how far we moved to distance from previous marker
				break;
			}
			const FAnimSyncMarker& NextSyncMarker = AuthoredSyncMarkers[NextMarker.MarkerIndex];
			checkSlow(ValidMarkerNames.Contains(NextSyncMarker.MarkerName));
			if (!bOffsetInitialized)
			{
				bOffsetInitialized = true;
				if (NextSyncMarker.Time < CurrentTime)
				{
					MarkerTimeOffset = SequenceLength;
				}
			}
			const float NextMarkerTime = NextSyncMarker.Time + MarkerTimeOffset;
			const float TimeToMarker = NextMarkerTime - CurrentTime;

			if (CurrentMoveDelta > TimeToMarker)
			{
				CurrentTime = NextSyncMarker.Time;
				CurrentMoveDelta -= TimeToMarker;

				PrevMarker.MarkerIndex = NextMarker.MarkerIndex;
				PrevMarker.TimeToMarker = -CurrentMoveDelta;

				int32 PassedMarker = MarkersPassed.Add(FPassedMarker());
				MarkersPassed[PassedMarker].PassedMarkerName = NextSyncMarker.MarkerName;
				MarkersPassed[PassedMarker].DeltaTimeWhenPassed = CurrentMoveDelta;

				do
				{
					++NextMarker.MarkerIndex;
					if (NextMarker.MarkerIndex >= AuthoredSyncMarkers.Num())
					{
						if (!bLooping)
						{
							NextMarker.MarkerIndex = -1;
							break;
						}
						NextMarker.MarkerIndex = 0;
						MarkerTimeOffset += SequenceLength;
					}
				} while (!ValidMarkerNames.Contains(AuthoredSyncMarkers[NextMarker.MarkerIndex].MarkerName));
			}
			else
			{
				CurrentTime = FMath::Fmod(CurrentTime + CurrentMoveDelta, SequenceLength);
				if (CurrentTime < 0.f)
				{
					CurrentTime += SequenceLength;
				}
				NextMarker.TimeToMarker = TimeToMarker - CurrentMoveDelta;
				PrevMarker.TimeToMarker -= CurrentMoveDelta;
				break;
			}
		}
	}
	else
	{
		while (true)
		{
			if (PrevMarker.MarkerIndex == -1)
			{
				float PrevCurrentTime = CurrentTime;
				CurrentTime = FMath::Max(CurrentTime + CurrentMoveDelta, 0.f);
				PrevMarker.TimeToMarker = CurrentTime;
				NextMarker.TimeToMarker -= CurrentTime - PrevCurrentTime; //Add how far we moved to distance from previous marker
				break;
			}
			const FAnimSyncMarker& PrevSyncMarker = AuthoredSyncMarkers[PrevMarker.MarkerIndex];
			checkSlow(ValidMarkerNames.Contains(PrevSyncMarker.MarkerName));
			if (!bOffsetInitialized)
			{
				bOffsetInitialized = true;
				if (PrevSyncMarker.Time > CurrentTime)
				{
					MarkerTimeOffset = -SequenceLength;
				}
			}
			const float PrevMarkerTime = PrevSyncMarker.Time + MarkerTimeOffset;
			const float TimeToMarker = PrevMarkerTime - CurrentTime;

			if (CurrentMoveDelta < TimeToMarker)
			{
				CurrentTime = PrevSyncMarker.Time;
				CurrentMoveDelta -= TimeToMarker;

				NextMarker.MarkerIndex = PrevMarker.MarkerIndex;
				NextMarker.TimeToMarker = -CurrentMoveDelta;

				int32 PassedMarker = MarkersPassed.Add(FPassedMarker());
				MarkersPassed[PassedMarker].PassedMarkerName = PrevSyncMarker.MarkerName;
				MarkersPassed[PassedMarker].DeltaTimeWhenPassed = CurrentMoveDelta;

				do
				{
					--PrevMarker.MarkerIndex;
					if (PrevMarker.MarkerIndex < 0)
					{
						if (!bLooping)
						{
							PrevMarker.MarkerIndex = -1;
							break;
						}
						PrevMarker.MarkerIndex = AuthoredSyncMarkers.Num() - 1;
						MarkerTimeOffset -= SequenceLength;
					}
				} while (!ValidMarkerNames.Contains(AuthoredSyncMarkers[PrevMarker.MarkerIndex].MarkerName));
			}
			else
			{
				CurrentTime = FMath::Fmod(CurrentTime + CurrentMoveDelta, SequenceLength);
				if (CurrentTime < 0.f)
				{
					CurrentTime += SequenceLength;
				}
				PrevMarker.TimeToMarker = TimeToMarker - CurrentMoveDelta;
				NextMarker.TimeToMarker -= CurrentMoveDelta;
				break;
			}
		}
	}

	check(CurrentTime >= 0.f && CurrentTime <= SequenceLength);
}

void AdvanceMarkerForwards(int32& Marker, FName MarkerToFind, bool bLooping, const TArray<FAnimSyncMarker>& AuthoredSyncMarkers)
{
	int32 MaxIterations = AuthoredSyncMarkers.Num();
	while ((AuthoredSyncMarkers[Marker].MarkerName != MarkerToFind) && (--MaxIterations >= 0))
	{
		++Marker;
		if (Marker == AuthoredSyncMarkers.Num() && !bLooping)
		{
			break;
		}
		Marker %= AuthoredSyncMarkers.Num();
	}

	if (!AuthoredSyncMarkers.IsValidIndex(Marker) || (AuthoredSyncMarkers[Marker].MarkerName != MarkerToFind))
	{
		Marker = MarkerIndexSpecialValues::AnimationBoundary;
	}
}

int32 MarkerCounterSpaceTransform(int32 MaxMarker, int32 Source)
{
	return MaxMarker - 1 - Source;
}

void AdvanceMarkerBackwards(int32& Marker, FName MarkerToFind, bool bLooping, const TArray<FAnimSyncMarker>& AuthoredSyncMarkers)
{
	int32 MaxIterations = AuthoredSyncMarkers.Num();
	const int32 MarkerMax = AuthoredSyncMarkers.Num();
	int32 Counter = MarkerCounterSpaceTransform(MarkerMax, Marker);
	while ((AuthoredSyncMarkers[Marker].MarkerName != MarkerToFind) && (--MaxIterations >= 0))
	{
		if ((Marker == 0) && !bLooping)
		{
			break;
		}
		Counter = (Counter + 1) % MarkerMax;
		Marker = MarkerCounterSpaceTransform(MarkerMax, Counter);
	}

	if (!AuthoredSyncMarkers.IsValidIndex(Marker) || (AuthoredSyncMarkers[Marker].MarkerName != MarkerToFind))
	{
		Marker = MarkerIndexSpecialValues::AnimationBoundary;
	}
}

bool MarkerMatchesPosition(const UAnimSequence* Sequence, int32 MarkerIndex, FName CorrectMarker)
{
	checkf(MarkerIndex != MarkerIndexSpecialValues::Unitialized, TEXT("Uninitialized marker supplied to MarkerMatchesPosition. Anim: %s Expecting marker %s (Added to help debug Jira OR-9675)"), *Sequence->GetName(), *CorrectMarker.ToString());
	return MarkerIndex == MarkerIndexSpecialValues::AnimationBoundary || CorrectMarker == Sequence->AuthoredSyncMarkers[MarkerIndex].MarkerName;
}

void UAnimSequence::ValidateCurrentPosition(const FMarkerSyncAnimPosition& Position, bool bPlayingForwards, bool bLooping, float&CurrentTime, FMarkerPair& PreviousMarker, FMarkerPair& NextMarker) const
{
	if (bPlayingForwards)
	{
		if (!MarkerMatchesPosition(this, PreviousMarker.MarkerIndex, Position.PreviousMarkerName))
		{
			AdvanceMarkerForwards(PreviousMarker.MarkerIndex, Position.PreviousMarkerName, bLooping, AuthoredSyncMarkers);
			NextMarker.MarkerIndex = (PreviousMarker.MarkerIndex + 1);
			if(NextMarker.MarkerIndex >= AuthoredSyncMarkers.Num())
			{
				NextMarker.MarkerIndex = bLooping ? NextMarker.MarkerIndex % AuthoredSyncMarkers.Num() : MarkerIndexSpecialValues::AnimationBoundary;
			}
		}

		if (!MarkerMatchesPosition(this, NextMarker.MarkerIndex, Position.NextMarkerName))
		{
			AdvanceMarkerForwards(NextMarker.MarkerIndex, Position.NextMarkerName, bLooping, AuthoredSyncMarkers);
		}
	}
	else
	{
		const int32 MarkerRange = AuthoredSyncMarkers.Num();
		if (!MarkerMatchesPosition(this, NextMarker.MarkerIndex, Position.NextMarkerName))
		{
			AdvanceMarkerBackwards(NextMarker.MarkerIndex, Position.NextMarkerName, bLooping, AuthoredSyncMarkers);
			if(NextMarker.MarkerIndex == MarkerIndexSpecialValues::AnimationBoundary || (NextMarker.MarkerIndex == 0 && bLooping))
			{
				PreviousMarker.MarkerIndex = AuthoredSyncMarkers.Num() - 1;
			}
			else
			{
				PreviousMarker.MarkerIndex = NextMarker.MarkerIndex - 1;
			}
		}
		if (!MarkerMatchesPosition(this, PreviousMarker.MarkerIndex, Position.PreviousMarkerName))
		{
			AdvanceMarkerBackwards(PreviousMarker.MarkerIndex, Position.PreviousMarkerName, bLooping, AuthoredSyncMarkers);
		}
	}

	checkSlow(MarkerMatchesPosition(this, PreviousMarker.MarkerIndex, Position.PreviousMarkerName));
	checkSlow(MarkerMatchesPosition(this, NextMarker.MarkerIndex, Position.NextMarkerName));

	// Only reset position if we found valid markers. Otherwise stay where we are to not pop.
	if ((PreviousMarker.MarkerIndex != MarkerIndexSpecialValues::AnimationBoundary) && (NextMarker.MarkerIndex != MarkerIndexSpecialValues::AnimationBoundary))
	{
		CurrentTime = GetCurrentTimeFromMarkers(PreviousMarker, NextMarker, Position.PositionBetweenMarkers);
	}
}

bool UAnimSequence::UseRawDataForPoseExtraction(const FBoneContainer& RequiredBones) const
{
	return bUseRawDataOnly || (GetSkeletonVirtualBoneGuid() != GetSkeleton()->GetVirtualBoneGuid()) || RequiredBones.GetDisableRetargeting() || RequiredBones.ShouldUseRawData() || RequiredBones.ShouldUseSourceData();
}

void UAnimSequence::AdvanceMarkerPhaseAsFollower(const FMarkerTickContext& Context, float DeltaRemaining, bool bLooping, float& CurrentTime, FMarkerPair& PreviousMarker, FMarkerPair& NextMarker) const
{
	const bool bPlayingForwards = DeltaRemaining > 0.f;

	ValidateCurrentPosition(Context.GetMarkerSyncStartPosition(), bPlayingForwards, bLooping, CurrentTime, PreviousMarker, NextMarker);
	if (bPlayingForwards)
	{
		int32 PassedMarkersIndex = 0;
		do
		{
			if (NextMarker.MarkerIndex == -1)
			{
				check(!bLooping || Context.GetMarkerSyncEndPosition().NextMarkerName == NAME_None); // shouldnt have an end of anim marker if looping
				CurrentTime = FMath::Min(CurrentTime + DeltaRemaining, SequenceLength);
				break;
			}
			else if (PassedMarkersIndex < Context.MarkersPassedThisTick.Num())
			{
				PreviousMarker.MarkerIndex = NextMarker.MarkerIndex;
				checkSlow(NextMarker.MarkerIndex != -1);
				const FPassedMarker& PassedMarker = Context.MarkersPassedThisTick[PassedMarkersIndex];
				AdvanceMarkerForwards(NextMarker.MarkerIndex, PassedMarker.PassedMarkerName, bLooping, AuthoredSyncMarkers);
				if (NextMarker.MarkerIndex == -1)
				{
					DeltaRemaining = PassedMarker.DeltaTimeWhenPassed;
				}
				++PassedMarkersIndex;
			}
		} while (PassedMarkersIndex < Context.MarkersPassedThisTick.Num());

		const FMarkerSyncAnimPosition& End = Context.GetMarkerSyncEndPosition();
		
		if (End.NextMarkerName == NAME_None)
		{
			NextMarker.MarkerIndex = -1;
		}

		if (NextMarker.MarkerIndex != -1 && Context.MarkersPassedThisTick.Num() > 0)
		{
			AdvanceMarkerForwards(NextMarker.MarkerIndex, End.NextMarkerName, bLooping, AuthoredSyncMarkers);
		}

		//Validation
		if (NextMarker.MarkerIndex != -1)
		{
			check(AuthoredSyncMarkers[NextMarker.MarkerIndex].MarkerName == End.NextMarkerName);
		}

		// End Validation
		// Only reset position if we found valid markers. Otherwise stay where we are to not pop.
		if ((PreviousMarker.MarkerIndex != MarkerIndexSpecialValues::AnimationBoundary) && (NextMarker.MarkerIndex != MarkerIndexSpecialValues::AnimationBoundary))
		{
			CurrentTime = GetCurrentTimeFromMarkers(PreviousMarker, NextMarker, End.PositionBetweenMarkers);
		}
	}
	else
	{
		int32 PassedMarkersIndex = 0;
		do
		{
			if (PreviousMarker.MarkerIndex == -1)
			{
				check(!bLooping || Context.GetMarkerSyncEndPosition().PreviousMarkerName == NAME_None); // shouldn't have an end of anim marker if looping
				CurrentTime = FMath::Max(CurrentTime + DeltaRemaining, 0.f);
				break;
			}
			else if (PassedMarkersIndex < Context.MarkersPassedThisTick.Num())
			{
				NextMarker.MarkerIndex = PreviousMarker.MarkerIndex;
				checkSlow(PreviousMarker.MarkerIndex != -1);
				const FPassedMarker& PassedMarker = Context.MarkersPassedThisTick[PassedMarkersIndex];
				AdvanceMarkerBackwards(PreviousMarker.MarkerIndex, PassedMarker.PassedMarkerName, bLooping, AuthoredSyncMarkers);
				if (PreviousMarker.MarkerIndex == -1)
				{
					DeltaRemaining = PassedMarker.DeltaTimeWhenPassed;
				}
				++PassedMarkersIndex;
			}
		} while (PassedMarkersIndex < Context.MarkersPassedThisTick.Num());

		const FMarkerSyncAnimPosition& End = Context.GetMarkerSyncEndPosition();

		if (PreviousMarker.MarkerIndex != -1 && Context.MarkersPassedThisTick.Num() > 0)
		{
			AdvanceMarkerBackwards(PreviousMarker.MarkerIndex, End.PreviousMarkerName, bLooping, AuthoredSyncMarkers);
		}

		if (End.PreviousMarkerName == NAME_None)
		{
			PreviousMarker.MarkerIndex = -1;
		}

		//Validation
		if (PreviousMarker.MarkerIndex != -1)
		{
			check(AuthoredSyncMarkers[PreviousMarker.MarkerIndex].MarkerName == End.PreviousMarkerName);
		}

		// End Validation
		// Only reset position if we found valid markers. Otherwise stay where we are to not pop.
		if ((PreviousMarker.MarkerIndex != MarkerIndexSpecialValues::AnimationBoundary) && (NextMarker.MarkerIndex != MarkerIndexSpecialValues::AnimationBoundary))
		{
			CurrentTime = GetCurrentTimeFromMarkers(PreviousMarker, NextMarker, End.PositionBetweenMarkers);
		}
	}
}

void UAnimSequence::GetMarkerIndicesForTime(float CurrentTime, bool bLooping, const TArray<FName>& ValidMarkerNames, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker) const
{
	const int LoopModStart = bLooping ? -1 : 0;
	const int LoopModEnd = bLooping ? 2 : 1;

	OutPrevMarker.MarkerIndex = -1;
	OutPrevMarker.TimeToMarker = -CurrentTime;
	OutNextMarker.MarkerIndex = -1;
	OutNextMarker.TimeToMarker = SequenceLength - CurrentTime;

	for (int32 LoopMod = LoopModStart; LoopMod < LoopModEnd; ++LoopMod)
	{
		const float LoopModTime = LoopMod * SequenceLength;
		for (int Idx = 0; Idx < AuthoredSyncMarkers.Num(); ++Idx)
		{
			const FAnimSyncMarker& Marker = AuthoredSyncMarkers[Idx];
			if (ValidMarkerNames.Contains(Marker.MarkerName))
			{
				const float MarkerTime = Marker.Time + LoopModTime;
				if (MarkerTime < CurrentTime)
				{
					OutPrevMarker.MarkerIndex = Idx;
					OutPrevMarker.TimeToMarker = MarkerTime - CurrentTime;
				}
				else if (MarkerTime >= CurrentTime)
				{
					OutNextMarker.MarkerIndex = Idx;
					OutNextMarker.TimeToMarker = MarkerTime - CurrentTime;
					break; // Done
				}
			}
		}
		if (OutNextMarker.MarkerIndex != -1)
		{
			break; // Done
		}
	}
}

FMarkerSyncAnimPosition UAnimSequence::GetMarkerSyncPositionfromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime) const
{
	FMarkerSyncAnimPosition SyncPosition;
	float PrevTime, NextTime;
	
	if (PrevMarker != -1 && ensureAlwaysMsgf(AuthoredSyncMarkers.IsValidIndex(PrevMarker),
		TEXT("%s - MarkerCount: %d, PrevMarker : %d, NextMarker: %d, CurrentTime : %0.2f"), *GetFullName(), AuthoredSyncMarkers.Num(), PrevMarker, NextMarker, CurrentTime))
	{
		PrevTime = AuthoredSyncMarkers[PrevMarker].Time;
		SyncPosition.PreviousMarkerName = AuthoredSyncMarkers[PrevMarker].MarkerName;
	}
	else
	{
		PrevTime = 0.f;
	}

	if (NextMarker != -1 && ensureAlwaysMsgf(AuthoredSyncMarkers.IsValidIndex(NextMarker),
		TEXT("%s - MarkerCount: %d, PrevMarker : %d, NextMarker: %d, CurrentTime : %0.2f"), *GetFullName(), AuthoredSyncMarkers.Num(), PrevMarker, NextMarker, CurrentTime))
	{
		NextTime = AuthoredSyncMarkers[NextMarker].Time;
		SyncPosition.NextMarkerName = AuthoredSyncMarkers[NextMarker].MarkerName;
	}
	else
	{
		NextTime = SequenceLength;
	}

	// Account for looping
	PrevTime = (PrevTime > CurrentTime) ? PrevTime - SequenceLength : PrevTime;
	NextTime = (NextTime < CurrentTime) ? NextTime + SequenceLength : NextTime;

	if (PrevTime == NextTime)
	{
		PrevTime -= SequenceLength;
	}

	check(NextTime > PrevTime);

	SyncPosition.PositionBetweenMarkers = (CurrentTime - PrevTime) / (NextTime - PrevTime);
	return SyncPosition;
}

float UAnimSequence::GetCurrentTimeFromMarkers(FMarkerPair& PrevMarker, FMarkerPair& NextMarker, float PositionBetweenMarkers) const
{
	float PrevTime = (PrevMarker.MarkerIndex != -1) ? AuthoredSyncMarkers[PrevMarker.MarkerIndex].Time : 0.f;
	float NextTime = (NextMarker.MarkerIndex != -1) ? AuthoredSyncMarkers[NextMarker.MarkerIndex].Time : SequenceLength;

	if (PrevTime >= NextTime)
	{
		PrevTime -= SequenceLength; //Account for looping
	}
	float CurrentTime = PrevTime + PositionBetweenMarkers * (NextTime - PrevTime);
	if (CurrentTime < 0.f)
	{
		CurrentTime += SequenceLength;
	}
	CurrentTime = FMath::Clamp<float>(CurrentTime, 0, SequenceLength);

	PrevMarker.TimeToMarker = PrevTime - CurrentTime;
	NextMarker.TimeToMarker = NextTime - CurrentTime;
	return CurrentTime;
}

void UAnimSequence::GetMarkerIndicesForPosition(const FMarkerSyncAnimPosition& SyncPosition, bool bLooping, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker, float& OutCurrentTime) const
{
	// If we're not looping, assume we're playing a transition and we need to stay where we are.
	if (!bLooping)
	{
		OutPrevMarker.MarkerIndex = -1;
		OutNextMarker.MarkerIndex = -1;

		for (int32 Idx = 0; Idx<AuthoredSyncMarkers.Num(); Idx++)
		{
			const float MarkerTime = AuthoredSyncMarkers[Idx].Time;
			if (OutCurrentTime > MarkerTime)
			{
				OutPrevMarker.MarkerIndex = Idx;
				OutPrevMarker.TimeToMarker = MarkerTime - OutCurrentTime;
			}
			else if (OutCurrentTime < MarkerTime)
			{
				OutNextMarker.MarkerIndex = Idx;
				OutNextMarker.TimeToMarker = MarkerTime - OutCurrentTime;
				break;
			}
		}

		return;
	}

	if (SyncPosition.PreviousMarkerName == NAME_None)
	{
		OutPrevMarker.MarkerIndex = -1;
		check(SyncPosition.NextMarkerName != NAME_None);

		for (int32 Idx = 0; Idx < AuthoredSyncMarkers.Num(); ++Idx)
		{
			const FAnimSyncMarker& Marker = AuthoredSyncMarkers[Idx];
			if (Marker.MarkerName == SyncPosition.NextMarkerName)
			{
				OutNextMarker.MarkerIndex = Idx;
				OutCurrentTime = GetCurrentTimeFromMarkers(OutPrevMarker, OutNextMarker, SyncPosition.PositionBetweenMarkers);
				return;
			}
		}
		// Should have found a marker above!
		checkf(false, TEXT("Next Marker not found in GetMarkerIndicesForPosition. Anim: %s Expecting marker %s (Added to help debug Jira OR-9675)"), *GetName(), *SyncPosition.NextMarkerName.ToString());
	}

	if (SyncPosition.NextMarkerName == NAME_None)
	{
		OutNextMarker.MarkerIndex = -1;
		check(SyncPosition.PreviousMarkerName != NAME_None);

		for (int32 Idx = AuthoredSyncMarkers.Num() - 1; Idx >= 0; --Idx)
		{
			const FAnimSyncMarker& Marker = AuthoredSyncMarkers[Idx];
			if (Marker.MarkerName == SyncPosition.PreviousMarkerName)
			{
				OutPrevMarker.MarkerIndex = Idx;
				OutCurrentTime = GetCurrentTimeFromMarkers(OutPrevMarker, OutNextMarker, SyncPosition.PositionBetweenMarkers);
				return;
			}
		}
		// Should have found a marker above!
		checkf(false, TEXT("Previous Marker not found in GetMarkerIndicesForPosition. Anim: %s Expecting marker %s (Added to help debug Jira OR-9675)"), *GetName(), *SyncPosition.PreviousMarkerName.ToString());
	}

	float DiffToCurrentTime = FLT_MAX;
	const float CurrentInputTime  = OutCurrentTime;

	for (int32 PrevMarkerIdx = 0; PrevMarkerIdx < AuthoredSyncMarkers.Num(); ++PrevMarkerIdx)
	{
		const FAnimSyncMarker& PrevMarker = AuthoredSyncMarkers[PrevMarkerIdx];
		if (PrevMarker.MarkerName == SyncPosition.PreviousMarkerName)
		{
			const int32 EndMarkerSearchStart = PrevMarkerIdx + 1;

			const int32 EndCount = bLooping ? AuthoredSyncMarkers.Num() + EndMarkerSearchStart : AuthoredSyncMarkers.Num();
			for (int32 NextMarkerCount = EndMarkerSearchStart; NextMarkerCount < EndCount; ++NextMarkerCount)
			{
				const int32 NextMarkerIdx = NextMarkerCount % AuthoredSyncMarkers.Num();

				if (AuthoredSyncMarkers[NextMarkerIdx].MarkerName == SyncPosition.NextMarkerName)
				{
					float NextMarkerTime = AuthoredSyncMarkers[NextMarkerIdx].Time;
					if (NextMarkerTime < PrevMarker.Time)
					{
						NextMarkerTime += SequenceLength;
					}
					float ThisCurrentTime = PrevMarker.Time + SyncPosition.PositionBetweenMarkers * (NextMarkerTime - PrevMarker.Time);
					if (ThisCurrentTime > SequenceLength)
					{
						ThisCurrentTime -= SequenceLength;
					}
					float ThisDiff = FMath::Abs(ThisCurrentTime - CurrentInputTime);
					if (ThisDiff < DiffToCurrentTime)
					{
						DiffToCurrentTime = ThisDiff;
						OutPrevMarker.MarkerIndex = PrevMarkerIdx;
						OutNextMarker.MarkerIndex = NextMarkerIdx;
						OutCurrentTime = GetCurrentTimeFromMarkers(OutPrevMarker, OutNextMarker, SyncPosition.PositionBetweenMarkers);
					}

					// this marker test is done, move onto next one
					break;
				}
			}

			// If we get here and we haven't found a match and we are not looping then there 
			// is no point running the rest of the loop set up something as relevant as we can and carry on
			if (OutPrevMarker.MarkerIndex == MarkerIndexSpecialValues::Unitialized)
			{
				//Find nearest previous marker that is earlier than our current time
				DiffToCurrentTime = OutCurrentTime - PrevMarker.Time;
				int32 PrevMarkerToUse = PrevMarkerIdx + 1;
				while (DiffToCurrentTime > 0.f && PrevMarkerToUse < AuthoredSyncMarkers.Num())
				{
					DiffToCurrentTime = OutCurrentTime - AuthoredSyncMarkers[PrevMarkerToUse].Time;
					++PrevMarkerToUse;
				}
				OutPrevMarker.MarkerIndex = PrevMarkerToUse - 1;	// We always go one past the marker we actually want to use
				
				OutNextMarker.MarkerIndex = -1;						// This goes to minus one as the very fact we are here means
																	// that there is no next marker to use
				OutCurrentTime = GetCurrentTimeFromMarkers(OutPrevMarker, OutNextMarker, SyncPosition.PositionBetweenMarkers);
				break; // no need to keep searching, we are done
			}
		}
	}
	// Should have found a markers above!
	checkf(OutPrevMarker.MarkerIndex != MarkerIndexSpecialValues::Unitialized, TEXT("Prev Marker not found in GetMarkerIndicesForPosition. Anim: %s Expecting marker %s (Added to help debug Jira OR-9675)"), *GetName(), *SyncPosition.PreviousMarkerName.ToString());
	checkf(OutNextMarker.MarkerIndex != MarkerIndexSpecialValues::Unitialized, TEXT("Next Marker not found in GetMarkerIndicesForPosition. Anim: %s Expecting marker %s (Added to help debug Jira OR-9675)"), *GetName(), *SyncPosition.NextMarkerName.ToString());
}

float UAnimSequence::GetFirstMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition) const
{
	if ((InMarkerSyncGroupPosition.PreviousMarkerName == NAME_None) || (InMarkerSyncGroupPosition.NextMarkerName == NAME_None))
	{
		return 0.f;
	}

	for (int32 PrevMarkerIdx = 0; PrevMarkerIdx < AuthoredSyncMarkers.Num()-1; PrevMarkerIdx++)
	{
		const FAnimSyncMarker& PrevMarker = AuthoredSyncMarkers[PrevMarkerIdx];
		const FAnimSyncMarker& NextMarker = AuthoredSyncMarkers[PrevMarkerIdx+1];
		if ((PrevMarker.MarkerName == InMarkerSyncGroupPosition.PreviousMarkerName) && (NextMarker.MarkerName == InMarkerSyncGroupPosition.NextMarkerName))
		{
			return FMath::Lerp(PrevMarker.Time, NextMarker.Time, InMarkerSyncGroupPosition.PositionBetweenMarkers);
		}
	}

	return 0.f;
}

float UAnimSequence::GetNextMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition, const float& StartingPosition) const
{
	if ((InMarkerSyncGroupPosition.PreviousMarkerName == NAME_None) || (InMarkerSyncGroupPosition.NextMarkerName == NAME_None))
	{
		return StartingPosition;
	}

	for (int32 PrevMarkerIdx = 0; PrevMarkerIdx < AuthoredSyncMarkers.Num() - 1; PrevMarkerIdx++)
	{
		const FAnimSyncMarker& PrevMarker = AuthoredSyncMarkers[PrevMarkerIdx];
		const FAnimSyncMarker& NextMarker = AuthoredSyncMarkers[PrevMarkerIdx + 1];

		if (NextMarker.Time < StartingPosition)
		{
			continue;
		}

		if ((PrevMarker.MarkerName == InMarkerSyncGroupPosition.PreviousMarkerName) && (NextMarker.MarkerName == InMarkerSyncGroupPosition.NextMarkerName))
		{
			const float FoundTime = FMath::Lerp(PrevMarker.Time, NextMarker.Time, InMarkerSyncGroupPosition.PositionBetweenMarkers);
			if (FoundTime < StartingPosition)
			{
				continue;
			}
			return FoundTime;
		}
	}

	return StartingPosition;
}

float UAnimSequence::GetPrevMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition, const float& StartingPosition) const
{
	if ((InMarkerSyncGroupPosition.PreviousMarkerName == NAME_None) || (InMarkerSyncGroupPosition.NextMarkerName == NAME_None) || (AuthoredSyncMarkers.Num() < 2))
	{
		return StartingPosition;
	}

	for (int32 PrevMarkerIdx = AuthoredSyncMarkers.Num() - 2; PrevMarkerIdx >= 0; PrevMarkerIdx--)
	{
		const FAnimSyncMarker& PrevMarker = AuthoredSyncMarkers[PrevMarkerIdx];
		const FAnimSyncMarker& NextMarker = AuthoredSyncMarkers[PrevMarkerIdx + 1];

		if (PrevMarker.Time > StartingPosition)
		{
			continue;
		}

		if ((PrevMarker.MarkerName == InMarkerSyncGroupPosition.PreviousMarkerName) && (NextMarker.MarkerName == InMarkerSyncGroupPosition.NextMarkerName))
		{
			const float FoundTime = FMath::Lerp(PrevMarker.Time, NextMarker.Time, InMarkerSyncGroupPosition.PositionBetweenMarkers);
			if (FoundTime > StartingPosition)
			{
				continue;
			}
			return FoundTime;
		}
	}

	return StartingPosition;
}

void UAnimSequence::EnableRootMotionSettingFromMontage(bool bInEnableRootMotion, const ERootMotionRootLock::Type InRootMotionRootLock)
{
	if (!bRootMotionSettingsCopiedFromMontage)
	{
		bEnableRootMotion = bInEnableRootMotion;
		RootMotionRootLock = InRootMotionRootLock;
		bRootMotionSettingsCopiedFromMontage = true;
	}
}

#if WITH_EDITOR
void UAnimSequence::OnRawDataChanged()
{
	CompressedTrackOffsets.Empty();
	CompressedScaleOffsets.Empty();
	CompressedByteStream.Empty();
	bUseRawDataOnly = true;

	RequestSyncAnimRecompression(false);
	//MDW - Once we have async anim ddc requests we should do this too
	//RequestDependentAnimRecompression();
}

bool UAnimSequence::IsCompressedDataValid() const
{
	return CompressedByteStream.Num() > 0 || RawAnimationData.Num() == 0 ||
		   (TranslationCompressionFormat == ACF_Identity && RotationCompressionFormat == ACF_Identity && ScaleCompressionFormat == ACF_Identity);
}

#endif

/*-----------------------------------------------------------------------------
	AnimNotify& subclasses
-----------------------------------------------------------------------------*/

#if !UE_BUILD_SHIPPING

void GatherAnimSequenceStats(FOutputDevice& Ar)
{
	int32 AnimationKeyFormatNum[AKF_MAX];
	int32 TranslationCompressionFormatNum[ACF_MAX];
	int32 RotationCompressionFormatNum[ACF_MAX];
	int32 ScaleCompressionFormatNum[ACF_MAX];
	FMemory::Memzero( AnimationKeyFormatNum, AKF_MAX * sizeof(int32) );
	FMemory::Memzero( TranslationCompressionFormatNum, ACF_MAX * sizeof(int32) );
	FMemory::Memzero( RotationCompressionFormatNum, ACF_MAX * sizeof(int32) );
	FMemory::Memzero( ScaleCompressionFormatNum, ACF_MAX * sizeof(int32) );

	Ar.Logf( TEXT(" %60s, Frames,NTT,NRT, NT1,NR1, TotTrnKys,TotRotKys,Codec,ResBytes"), TEXT("Sequence Name") );
	int32 GlobalNumTransTracks = 0;
	int32 GlobalNumRotTracks = 0;
	int32 GlobalNumScaleTracks = 0;
	int32 GlobalNumTransTracksWithOneKey = 0;
	int32 GlobalNumRotTracksWithOneKey = 0;
	int32 GlobalNumScaleTracksWithOneKey = 0;
	int32 GlobalApproxCompressedSize = 0;
	int32 GlobalApproxKeyDataSize = 0;
	int32 GlobalNumTransKeys = 0;
	int32 GlobalNumRotKeys = 0;
	int32 GlobalNumScaleKeys = 0;

	for( TObjectIterator<UAnimSequence> It; It; ++It )
	{
		UAnimSequence* Seq = *It;

		int32 NumTransTracks = 0;
		int32 NumRotTracks = 0;
		int32 NumScaleTracks = 0;
		int32 TotalNumTransKeys = 0;
		int32 TotalNumRotKeys = 0;
		int32 TotalNumScaleKeys = 0;
		float TranslationKeySize = 0.0f;
		float RotationKeySize = 0.0f;
		float ScaleKeySize = 0.0f;
		int32 OverheadSize = 0;
		int32 NumTransTracksWithOneKey = 0;
		int32 NumRotTracksWithOneKey = 0;
		int32 NumScaleTracksWithOneKey = 0;

		AnimationFormat_GetStats(
			Seq, 
			NumTransTracks,
			NumRotTracks,
			NumScaleTracks,
			TotalNumTransKeys,
			TotalNumRotKeys,
			TotalNumScaleKeys,
			TranslationKeySize,
			RotationKeySize,
			ScaleKeySize, 
			OverheadSize,
			NumTransTracksWithOneKey,
			NumRotTracksWithOneKey,
			NumScaleTracksWithOneKey);

		GlobalNumTransTracks += NumTransTracks;
		GlobalNumRotTracks += NumRotTracks;
		GlobalNumScaleTracks += NumScaleTracks;
		GlobalNumTransTracksWithOneKey += NumTransTracksWithOneKey;
		GlobalNumRotTracksWithOneKey += NumRotTracksWithOneKey;
		GlobalNumScaleTracksWithOneKey += NumScaleTracksWithOneKey;

		GlobalApproxCompressedSize += Seq->GetApproxCompressedSize();
		GlobalApproxKeyDataSize += (int32)((TotalNumTransKeys * TranslationKeySize) + (TotalNumRotKeys * RotationKeySize) + (TotalNumScaleKeys * ScaleKeySize));

		GlobalNumTransKeys += TotalNumTransKeys;
		GlobalNumRotKeys += TotalNumRotKeys;
		GlobalNumScaleKeys += TotalNumScaleKeys;

		Ar.Logf(TEXT(" %60s, %3i, %3i,%3i,%3i, %3i,%3i,%3i, %10i,%10i,%10i, %s, %d"),
			*Seq->GetName(),
			Seq->NumFrames,
			NumTransTracks, NumRotTracks, NumScaleTracks,
			NumTransTracksWithOneKey, NumRotTracksWithOneKey, NumScaleTracksWithOneKey,
			TotalNumTransKeys, TotalNumRotKeys, TotalNumScaleKeys,
			*FAnimationUtils::GetAnimationKeyFormatString(static_cast<AnimationKeyFormat>(Seq->KeyEncodingFormat)),
			(int32)Seq->GetResourceSizeBytes(EResourceSizeMode::Exclusive) );
	}
	Ar.Logf( TEXT("======================================================================") );
	Ar.Logf( TEXT("Total Num Tracks: %i trans, %i rot, %i scale, %i trans1, %i rot1, %i scale1"), GlobalNumTransTracks, GlobalNumRotTracks, GlobalNumScaleTracks, GlobalNumTransTracksWithOneKey, GlobalNumRotTracksWithOneKey, GlobalNumScaleTracksWithOneKey  );
	Ar.Logf( TEXT("Total Num Keys: %i trans, %i rot, %i scale"), GlobalNumTransKeys, GlobalNumRotKeys, GlobalNumScaleKeys );

	Ar.Logf( TEXT("Approx Compressed Memory: %i bytes"), GlobalApproxCompressedSize);
	Ar.Logf( TEXT("Approx Key Data Memory: %i bytes"), GlobalApproxKeyDataSize);
}

#endif // !UE_BUILD_SHIPPING

FArchive& operator<<(FArchive& Ar, FCompressedOffsetData& D)
{
	Ar << D.OffsetData << D.StripSize;
	return Ar;
}


#undef LOCTEXT_NAMESPACE 
