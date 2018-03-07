// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/PoseAsset.h"
#include "UObject/FrameworkObjectVersion.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"

#define LOCTEXT_NAMESPACE "PoseAsset"

// utility function 
#if WITH_EDITOR
FSmartName GetUniquePoseName(USkeleton* Skeleton)
{
	check(Skeleton);
	int32 NameIndex = 0;

	SmartName::UID_Type NewUID;
	FName NewName;

	do
	{
		NewName = FName(*FString::Printf(TEXT("Pose_%d"), NameIndex++));
		NewUID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, NewName);
	} while (NewUID != SmartName::MaxUID);

	// if found, 
	FSmartName NewPoseName;
	Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, NewName, NewPoseName);

	return NewPoseName;
}
#endif 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FPoseDataContainer
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FPoseDataContainer::Reset()
{
	// clear everything
	PoseNames.Reset();
	Poses.Reset();
	Tracks.Reset();
	TrackMap.Reset();
	Curves.Reset();
}

void FPoseDataContainer::AddOrUpdatePose(const FSmartName& InPoseName, const TArray<FTransform>& InlocalSpacePose, const TArray<float>& InCurveData)
{
	// make sure the transform is correct size
	if (ensureAlways(InlocalSpacePose.Num() == Tracks.Num()))
	{
		// find or add pose data
		FPoseData* PoseDataPtr = FindOrAddPoseData(InPoseName);
		// now add pose
		PoseDataPtr->LocalSpacePose = InlocalSpacePose;
		PoseDataPtr->CurveData = InCurveData;
	}

	// for now we only supports same tracks
}

bool FPoseDataContainer::InsertTrack(const FName& InTrackName, USkeleton* InSkeleton, FName& InRetargetSourceName)
{
	check(InSkeleton);

	// make sure the transform is correct size
	if (Tracks.Contains(InTrackName) == false)
	{
		int32 SkeletonIndex = InSkeleton->GetReferenceSkeleton().FindBoneIndex(InTrackName);
		int32 TrackIndex = INDEX_NONE;
		if (SkeletonIndex != INDEX_NONE)
		{
			Tracks.Add(InTrackName);
			TrackMap.Add(InTrackName, SkeletonIndex);
			TrackIndex = Tracks.Num() - 1;

			// now insert default refpose
			const FTransform DefaultPose = GetDefaultTransform(SkeletonIndex, InSkeleton, InRetargetSourceName);

			for (auto& PoseData : Poses)
			{
				ensureAlways(PoseData.LocalSpacePose.Num() == TrackIndex);

				PoseData.LocalSpacePose.Add(DefaultPose);

				// make sure they always match
				ensureAlways(PoseData.LocalSpacePose.Num() == Tracks.Num());
			}

			return true;
		}

		return false;
	}

	return false;
}

void FPoseDataContainer::GetPoseCurve(const FPoseData* PoseData, FBlendedCurve& OutCurve) const
{
	if (PoseData)
	{
		const TArray<float>& CurveValues = PoseData->CurveData;
		checkSlow(CurveValues.Num() == Curves.Num());

		// extract curve - not optimized, can use optimization
		for (int32 CurveIndex = 0; CurveIndex < Curves.Num(); ++CurveIndex)
		{
			const FAnimCurveBase& Curve = Curves[CurveIndex];
			OutCurve.Set(Curve.Name.UID, CurveValues[CurveIndex]);
		}
	}
}
// remove track if all poses has identity key
// we don't want to save transform per track (good for compression) because that will slow down blending
void FPoseDataContainer::Shrink(USkeleton* InSkeleton, FName& InRetargetSourceName)
{
	for (auto& Pose : Poses)
	{
		check(Pose.LocalSpacePose.Num() == Tracks.Num());

		Pose.LocalSpacePoseMask.SetNumZeroed(Tracks.Num(), true);
		for (int32 TrackIndex = 0; TrackIndex < Tracks.Num(); ++TrackIndex)
		{
			FTransform DefaultTransform = GetDefaultTransform(Tracks[TrackIndex], InSkeleton, InRetargetSourceName);
			if (!Pose.LocalSpacePose[TrackIndex].Equals(DefaultTransform, KINDA_SMALL_NUMBER))
			{
				Pose.LocalSpacePoseMask[TrackIndex] = true;
			}
		}
	}

	for (int32 TrackIndex = 0; TrackIndex < Tracks.Num(); ++TrackIndex)
	{
		bool bDeleteTrack = true;
		for (auto& Pose : Poses)
		{
			if (Pose.LocalSpacePoseMask[TrackIndex])
			{
				bDeleteTrack = false;
				break;
			}
		}

		// if nothing contains any data, just delete this track
		if (bDeleteTrack)
		{
			DeleteTrack(TrackIndex);
			--TrackIndex;
		}
	}
}

void FPoseDataContainer::DeleteTrack(int32 TrackIndex)
{
	TrackMap.Remove(Tracks[TrackIndex]);
	Tracks.RemoveAt(TrackIndex);
	for (auto& Pose : Poses)
	{
		Pose.LocalSpacePoseMask.RemoveAt(TrackIndex);
		Pose.LocalSpacePose.RemoveAt(TrackIndex);
	}
}

FTransform FPoseDataContainer::GetDefaultTransform(const FName& InTrackName, USkeleton* InSkeleton, const FName& InRetargetSourceName) const
{
	int32 SkeletonIndex = InSkeleton->GetReferenceSkeleton().FindBoneIndex(InTrackName);
	if (SkeletonIndex != INDEX_NONE)
	{
		return GetDefaultTransform(SkeletonIndex, InSkeleton, InRetargetSourceName);
	}

	return FTransform::Identity;
}

FTransform FPoseDataContainer::GetDefaultTransform(int32 SkeletonIndex, USkeleton* InSkeleton, const FName& InRetargetSourceName) const
{
	// now insert default refpose
	const TArray<FTransform>& RefPose = InSkeleton->GetRefLocalPoses(InRetargetSourceName);

	if (RefPose.IsValidIndex(SkeletonIndex))
	{
		return RefPose[SkeletonIndex];
	}

	return FTransform::Identity;
}

bool FPoseDataContainer::FillUpDefaultPose(FPoseData* PoseData, USkeleton* InSkeleton, FName& InRetargetSourceName)
{
	if (PoseData)
	{
		int32 TrackIndex = 0;
		const TArray<FTransform>& RefPose = InSkeleton->GetRefLocalPoses(InRetargetSourceName);
		for (TPair<FName, int32>& TrackIter : TrackMap)
		{
			int32 SkeletonIndex = TrackIter.Value;
			PoseData->LocalSpacePose[TrackIndex] = RefPose[SkeletonIndex];

			++TrackIndex;
		}

		return true;
	}

	return false;
}

bool FPoseDataContainer::FillUpDefaultPose(const FSmartName& InPoseName, USkeleton* InSkeleton, FName& InRetargetSourceName)
{
	const TArray<FTransform>& RefPose = InSkeleton->GetRefLocalPoses(InRetargetSourceName);
	FPoseData* PoseData = FindPoseData(InPoseName);

	if (PoseData)
	{
		return FillUpDefaultPose(PoseData, InSkeleton, InRetargetSourceName);
	}

	return false;
}

FPoseData* FPoseDataContainer::FindPoseData(FSmartName PoseName)
{
	int32 PoseIndex = PoseNames.Find(PoseName);
	if (PoseIndex != INDEX_NONE)
	{
		return &Poses[PoseIndex];
	}

	return nullptr;
}

FPoseData* FPoseDataContainer::FindOrAddPoseData(FSmartName PoseName)
{
	int32 PoseIndex = PoseNames.Find(PoseName);
	if (PoseIndex == INDEX_NONE)
	{
		PoseIndex = PoseNames.Add(PoseName);
		check(PoseIndex == Poses.AddZeroed(1));
	}

	return &Poses[PoseIndex];
}

void FPoseDataContainer::RenamePose(FSmartName OldPoseName, FSmartName NewPoseName)
{
	int32 PoseIndex = PoseNames.Find(OldPoseName);
	if (PoseIndex != INDEX_NONE)
	{
		PoseNames[PoseIndex] = NewPoseName;
	}
}

bool FPoseDataContainer::DeletePose(FSmartName PoseName)
{
	int32 PoseIndex = PoseNames.Find(PoseName);
	if (PoseIndex != INDEX_NONE)
	{
		PoseNames.RemoveAt(PoseIndex);
		Poses.RemoveAt(PoseIndex);
		return true;
	}

	return false;
}

bool FPoseDataContainer::DeleteCurve(FSmartName CurveName)
{
	for (int32 CurveIndex = 0; CurveIndex < Curves.Num(); ++CurveIndex)
	{
		if (Curves[CurveIndex].Name == CurveName)
		{
			Curves.RemoveAt(CurveIndex);

			// delete this index from all poses
			for (int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
			{
				Poses[PoseIndex].CurveData.RemoveAt(CurveIndex);
			}

			return true;
		}
	}

	return false;
}
void FPoseDataContainer::ConvertToFullPose(int32 InBasePoseIndex, const TArray<FTransform>& InBasePose, const TArray<float>& InBaseCurve)
{
	check(InBaseCurve.Num() == Curves.Num());

	for (int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
	{
		// if this pose is not base pose
		if (PoseIndex != InBasePoseIndex)
		{
			// should it be move? Why? I need that buffer still
			TArray<FTransform> AdditivePose = Poses[PoseIndex].LocalSpacePose;
			FPoseData& PoseData = Poses[PoseIndex];

			const ScalarRegister AdditiveWeight(1.f);

			check(AdditivePose.Num() == InBasePose.Num());
			for (int32 BoneIndex = 0; BoneIndex < AdditivePose.Num(); ++BoneIndex)
			{
				PoseData.LocalSpacePose[BoneIndex] = InBasePose[BoneIndex];
				PoseData.LocalSpacePose[BoneIndex].AccumulateWithAdditiveScale(AdditivePose[BoneIndex], AdditiveWeight);
			}

			int32 CurveNum = Curves.Num();
			checkSlow(CurveNum == PoseData.CurveData.Num());
			for (int32 CurveIndex = 0; CurveIndex < CurveNum; ++CurveIndex)
			{
				PoseData.CurveData[CurveIndex] = InBaseCurve[CurveIndex] + PoseData.CurveData[CurveIndex];
			}
		}
	}
}

void FPoseDataContainer::ConvertToAdditivePose(int32 InBasePoseIndex, const TArray<FTransform>& InBasePose, const TArray<float>& InBaseCurve)
{
	check(InBaseCurve.Num() == Curves.Num());
	for (int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
	{
		// if this pose is not base pose
		if (PoseIndex != InBasePoseIndex)
		{
			FPoseData& PoseData = Poses[PoseIndex];
			check(PoseData.LocalSpacePose.Num() == InBasePose.Num());
			for (int32 BoneIndex = 0; BoneIndex < InBasePose.Num(); ++BoneIndex)
			{
				FAnimationRuntime::ConvertTransformToAdditive(PoseData.LocalSpacePose[BoneIndex], InBasePose[BoneIndex]);
			}

			int32 CurveNum = Curves.Num();
			checkSlow(CurveNum == PoseData.CurveData.Num());
			for (int32 CurveIndex = 0; CurveIndex < CurveNum; ++CurveIndex)
			{
				PoseData.CurveData[CurveIndex] = PoseData.CurveData[CurveIndex] - InBaseCurve[CurveIndex];
			}
		}
	}
}

/////////////////////////////////////////////////////
// UPoseAsset
/////////////////////////////////////////////////////
UPoseAsset::UPoseAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAdditivePose(false)
	, BasePoseIndex(-1)
{
}

/**
 * Local utility struct that keeps skeleton bone index and compact bone index together for retargeting
 */
struct FBoneIndices
{
	int32 SkeletonBoneIndex;
	FCompactPoseBoneIndex CompactBoneIndex;

	FBoneIndices(int32 InSkeletonBoneIndex, FCompactPoseBoneIndex InCompactBoneIndex)
		: SkeletonBoneIndex(InSkeletonBoneIndex)
		, CompactBoneIndex(InCompactBoneIndex)
	{}
};

void UPoseAsset::GetBaseAnimationPose(struct FCompactPose& OutPose, FBlendedCurve& OutCurve) const
{
	if (bAdditivePose && PoseContainer.Poses.IsValidIndex(BasePoseIndex))
	{
		const FBoneContainer& RequiredBones = OutPose.GetBoneContainer();
		USkeleton* MySkeleton = GetSkeleton();

		OutPose.ResetToRefPose();

		// this contains compact bone pose list that this pose cares
		TArray<FBoneIndices> BoneIndices;

		const int32 TrackNum = PoseContainer.TrackMap.Num();

		for (const TPair<FName, int32>& TrackPair : PoseContainer.TrackMap)
		{
			const int32 SkeletonBoneIndex = TrackPair.Value;
			const FCompactPoseBoneIndex PoseBoneIndex = RequiredBones.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
			// we add even if it's invalid because we want it to match with track index
			BoneIndices.Add(FBoneIndices(SkeletonBoneIndex, PoseBoneIndex));
		}

		const TArray<FTransform>& PoseTransform = PoseContainer.Poses[BasePoseIndex].LocalSpacePose;

		for (int32 TrackIndex = 0; TrackIndex < TrackNum; ++TrackIndex)
		{
			const FBoneIndices& LocalBoneIndices = BoneIndices[TrackIndex];

			if (LocalBoneIndices.CompactBoneIndex != INDEX_NONE)
			{
				FTransform& OutTransform = OutPose[LocalBoneIndices.CompactBoneIndex];
				OutTransform = PoseTransform[TrackIndex];
				FAnimationRuntime::RetargetBoneTransform(MySkeleton, RetargetSource, OutTransform, LocalBoneIndices.SkeletonBoneIndex, LocalBoneIndices.CompactBoneIndex, RequiredBones, false);
			}
		}

		PoseContainer.GetPoseCurve(&PoseContainer.Poses[BasePoseIndex], OutCurve);
	}
	else
	{
		OutPose.ResetToRefPose();
	}
}

bool UPoseAsset::GetAnimationPose(struct FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const
{
	const FBoneContainer& RequiredBones = OutPose.GetBoneContainer();
	USkeleton* MySkeleton = GetSkeleton();

	// this contains compact bone pose list that this pose cares
	TArray<FBoneIndices> BoneIndices;

	const int32 TrackNum = PoseContainer.TrackMap.Num();

	for (const TPair<FName, int32>& TrackPair : PoseContainer.TrackMap)
	{
		const int32 SkeletonBoneIndex = TrackPair.Value;
		const FCompactPoseBoneIndex PoseBoneIndex = RequiredBones.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
		// we add even if it's invalid because we want it to match with track index
		BoneIndices.Add(FBoneIndices(SkeletonBoneIndex, PoseBoneIndex));
	}

	check(PoseContainer.IsValid());

	if (bAdditivePose)
	{
		OutPose.ResetToAdditiveIdentity();
	}
	else
	{
		OutPose.ResetToRefPose();
	}

	bool bNormalizeWeight = bAdditivePose == false;

	if (ensureAlways(ExtractionContext.PoseCurves.Num() == PoseContainer.Poses.Num()))
	{
		TMap<const FPoseData*, float> IndexToWeightMap;
		float TotalWeight = 0.f;
		for (int32 PoseIndex = 0; PoseIndex < PoseContainer.Poses.Num(); ++PoseIndex)
		{
			const FSmartName& PoseName = PoseContainer.PoseNames[PoseIndex];
			const FPoseData& PoseData = PoseContainer.Poses[PoseIndex];

			float Value = ExtractionContext.PoseCurves[PoseIndex];

			// we only add to the list if it's not additive Or if it's additive, we don't want to add base pose index
			// and has weight
			if ((!bAdditivePose || PoseIndex != BasePoseIndex) && FAnimationRuntime::HasWeight(Value))
			{
				IndexToWeightMap.Add(&PoseData, Value);
				TotalWeight += Value;
			}
		}

		const int32 TotalNumberOfValidPoses = IndexToWeightMap.Num();
		if (TotalNumberOfValidPoses > 0)
		{
			TArray<FTransform> BlendedBoneTransform;
			bool bFirstPose = true;
			TArray<FBlendedCurve> PoseCurves;
			TArray<float>	CurveWeights;

			//if full pose, we'll have to normalize by weight
			if (bNormalizeWeight && TotalWeight > 1.f)
			{
				for (TPair<const FPoseData*, float>& WeightPair : IndexToWeightMap)
				{
					WeightPair.Value /= TotalWeight;
				}
			}

			BlendedBoneTransform.AddUninitialized(TrackNum);
			for (int32 TrackIndex = 0; TrackIndex < TrackNum; ++TrackIndex)
			{
				// If invalid compact bone index, BlendedBoneTransform[TrackIndex] won't be used (see 'blend curves' below), so don't bother filling it in
				const FCompactPoseBoneIndex CompactIndex = BoneIndices[TrackIndex].CompactBoneIndex;
				if (CompactIndex != INDEX_NONE)
				{
					TArray<FTransform> BlendingTransform;
					TArray<float> BlendingWeights;
					float TotalLocalWeight = 0.f;
					for (const TPair<const FPoseData*, float>& ActivePosePair : IndexToWeightMap)
					{
						const FPoseData* Pose = ActivePosePair.Key;
						const float Weight = ActivePosePair.Value;

						if (Pose->LocalSpacePoseMask[TrackIndex])
						{
							BlendingTransform.Add(Pose->LocalSpacePose[TrackIndex]);
							BlendingWeights.Add(Weight);
							TotalLocalWeight += Weight;
						}
					}

					const int32 StartBlendLoopIndex = (TotalLocalWeight < 1.f) ? 0 : 1;

					if (BlendingTransform.Num() == 0)
					{
						// copy from out default pose
						BlendedBoneTransform[TrackIndex] = OutPose[CompactIndex];
					}
					else
					{
						if (StartBlendLoopIndex == 0)
						{
							BlendedBoneTransform[TrackIndex] = OutPose[CompactIndex] * ScalarRegister(1.f - TotalLocalWeight);
						}
						else
						{
							BlendedBoneTransform[TrackIndex] = BlendingTransform[0] * ScalarRegister(BlendingWeights[0]);
						}
					}

					for (int32 BlendIndex = StartBlendLoopIndex; BlendIndex < BlendingTransform.Num(); ++BlendIndex)
					{
						BlendedBoneTransform[TrackIndex].AccumulateWithShortestRotation(BlendingTransform[BlendIndex], ScalarRegister(BlendingWeights[BlendIndex]));
					}
				}
			}

			// collect curves
			PoseCurves.AddDefaulted(TotalNumberOfValidPoses);
			CurveWeights.AddUninitialized(TotalNumberOfValidPoses);
			int32 PoseIndex = 0;
			for (const TPair<const FPoseData*, float>& ActivePosePair : IndexToWeightMap)
			{
				const FPoseData* Pose = ActivePosePair.Key;
				const float Weight = ActivePosePair.Value;

				CurveWeights[PoseIndex] = Weight;
				PoseCurves[PoseIndex].InitFrom(OutCurve);
				PoseContainer.GetPoseCurve(Pose, PoseCurves[PoseIndex]);
				++PoseIndex;
			}

			// blend curves
			BlendCurves(PoseCurves, CurveWeights, OutCurve);

			for (int32 TrackIndex = 0; TrackIndex < TrackNum; ++TrackIndex)
			{
				const FBoneIndices& LocalBoneIndices = BoneIndices[TrackIndex];
				if (LocalBoneIndices.CompactBoneIndex != INDEX_NONE)
				{
					FAnimationRuntime::RetargetBoneTransform(MySkeleton, RetargetSource, BlendedBoneTransform[TrackIndex], LocalBoneIndices.SkeletonBoneIndex, LocalBoneIndices.CompactBoneIndex, RequiredBones, false);
					OutPose[LocalBoneIndices.CompactBoneIndex] = BlendedBoneTransform[TrackIndex];
					OutPose.NormalizeRotations();
				}
			}

			return true;
		}
	}

	return false;
}

void UPoseAsset::PostLoad()
{
	Super::PostLoad();

	// fix curve names
	USkeleton* MySkeleton = GetSkeleton();
	if (MySkeleton)
	{
		MySkeleton->VerifySmartNames(USkeleton::AnimCurveMappingName, PoseContainer.PoseNames);

		for (auto& Curve : PoseContainer.Curves)
		{
			MySkeleton->VerifySmartName(USkeleton::AnimCurveMappingName, Curve.Name);
		}

		// double loop but this check only should happen once per asset
		// this should continue to add if skeleton hasn't been saved either 
		if (GetLinkerCustomVersion(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MoveCurveTypesToSkeleton 
			|| MySkeleton->GetLinkerCustomVersion(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MoveCurveTypesToSkeleton)
		{
			// fix up curve flags to skeleton
			for (auto& Curve : PoseContainer.Curves)
			{
				bool bMorphtargetSet = Curve.GetCurveTypeFlag(AACF_DriveMorphTarget_DEPRECATED);
				bool bMaterialSet = Curve.GetCurveTypeFlag(AACF_DriveMaterial_DEPRECATED);

				// only add this if that has to 
				if (bMorphtargetSet || bMaterialSet)
				{
					MySkeleton->AccumulateCurveMetaData(Curve.Name.DisplayName, bMaterialSet, bMorphtargetSet);
				}
			}
		}
	}

	// I  have to fix pose names
	RecacheTrackmap();
}

void UPoseAsset::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);;

	Super::Serialize(Ar);

	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::PoseAssetSupportPerBoneMask)
	{
		// fix curve names
		USkeleton* MySkeleton = GetSkeleton();
		if (MySkeleton)
		{
			PoseContainer.Shrink(MySkeleton, RetargetSource);
		}
	}
}

void UPoseAsset::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	// Number of poses
	OutTags.Add(FAssetRegistryTag("Poses", FString::FromInt(GetNumPoses()), FAssetRegistryTag::TT_Numerical));
#if WITH_EDITOR
	TArray<FName> Names;
	Names.Reserve(PoseContainer.PoseNames.Num() + PoseContainer.Curves.Num());

	for (const FSmartName& SmartName : PoseContainer.PoseNames)
	{
		Names.Add(SmartName.DisplayName);
	}

	for (const FAnimCurveBase& Curve : PoseContainer.Curves)
	{
		Names.AddUnique(Curve.Name.DisplayName);
	}

	FString PoseNameList;
	for(const FName& Name : Names)
	{
		PoseNameList += FString::Printf(TEXT("%s%s"), *Name.ToString(), *USkeleton::CurveTagDelimiter);
	}
	OutTags.Add(FAssetRegistryTag(USkeleton::CurveNameTag, PoseNameList, FAssetRegistryTag::TT_Hidden)); //write pose names as curve tag as they use 
#endif
}

int32 UPoseAsset::GetNumPoses() const
{ 
	return PoseContainer.GetNumPoses();
}

int32 UPoseAsset::GetNumCurves() const
{
	return PoseContainer.Curves.Num();
}

int32 UPoseAsset::GetNumTracks() const
{
	return PoseContainer.Tracks.Num();
}


const TArray<FSmartName> UPoseAsset::GetPoseNames() const
{
	return PoseContainer.PoseNames;
}

const TArray<FName>	UPoseAsset::GetTrackNames() const
{
	return PoseContainer.Tracks;
}

const TArray<FSmartName> UPoseAsset::GetCurveNames() const
{
	TArray<FSmartName> CurveNames;
	for (int32 CurveIndex = 0; CurveIndex < PoseContainer.Curves.Num(); ++CurveIndex)
	{
		CurveNames.Add(PoseContainer.Curves[CurveIndex].Name);
	}

	return CurveNames;
}

const TArray<FAnimCurveBase> UPoseAsset::GetCurveData() const
{
	return PoseContainer.Curves;
}

const TArray<float> UPoseAsset::GetCurveValues(const int32 PoseIndex) const
{
	TArray<float> ResultCurveData;

	if (PoseContainer.Poses.IsValidIndex(PoseIndex))
	{
		ResultCurveData = PoseContainer.Poses[PoseIndex].CurveData;
	}

	return ResultCurveData;
}

bool UPoseAsset::GetLocalPoseForTrack(const int32 PoseIndex, const int32 TrackIndex, FTransform& OutTransform) const
{
	bool bSuccess = false;

	if (PoseContainer.Poses.IsValidIndex(PoseIndex))
	{
		const FPoseData& PoseData = PoseContainer.Poses[PoseIndex];
		if (PoseData.LocalSpacePose.IsValidIndex(TrackIndex))
		{
			OutTransform = PoseData.LocalSpacePose[TrackIndex];
			bSuccess = true;
		}
	}

	return bSuccess;
}

bool UPoseAsset::GetCurveValue(const int32 PoseIndex, const int32 CurveIndex, float& OutValue) const
{
	bool bSuccess = false;

	if (PoseContainer.Poses.IsValidIndex(PoseIndex))
	{
		const FPoseData& PoseData = PoseContainer.Poses[PoseIndex];
		if (PoseData.CurveData.IsValidIndex(CurveIndex))
		{
			OutValue = PoseData.CurveData[CurveIndex];
			bSuccess = true;
		}
	}

	return bSuccess;
}

const int32 UPoseAsset::GetTrackIndexByName(const FName& InTrackName) const
{
	int32 ResultTrackIndex = INDEX_NONE;

	// Only search if valid name passed in
	if (InTrackName != NAME_None)
	{
		ResultTrackIndex = PoseContainer.Tracks.Find(InTrackName);
	}

	return ResultTrackIndex;
}


bool UPoseAsset::ContainsPose(const FName& InPoseName) const
{
	for (const auto& PoseName : PoseContainer.PoseNames)
	{
		if (PoseName.DisplayName == InPoseName)
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR
bool UPoseAsset::AddOrUpdatePoseWithUniqueName(USkeletalMeshComponent* MeshComponent, FSmartName* OutPoseName /*= nullptr*/)
{
	bool bSuccess = true;
	bool bSavedAdditivePose = bAdditivePose;

	// if it's already additive, convert to full pose first
	if (bAdditivePose)
	{
		// convert to full pose
		bSuccess = ConvertToFullPose();
	}

	if (bSuccess)
	{
		FSmartName NewPoseName = GetUniquePoseName(GetSkeleton());
		AddOrUpdatePose(NewPoseName, MeshComponent);

		if (OutPoseName)
		{
			*OutPoseName = NewPoseName;
		}

		// convert back to additive if it was that way
		if (bSavedAdditivePose)
		{
			ConvertToAdditivePose(bSavedAdditivePose);
		}

		OnPoseListChanged.Broadcast();
	}

	return bSuccess;
}

void UPoseAsset::AddOrUpdatePose(const FSmartName& PoseName, USkeletalMeshComponent* MeshComponent)
{
	USkeleton* MySkeleton = GetSkeleton();
	if (MySkeleton && MeshComponent && MeshComponent->SkeletalMesh)
	{
		TArray<FName> TrackNames;
		// note this ignores root motion
		TArray<FTransform> BoneTransform = MeshComponent->GetComponentSpaceTransforms();
		const FReferenceSkeleton& RefSkeleton = MeshComponent->SkeletalMesh->RefSkeleton;
		for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
		{
			TrackNames.Add(RefSkeleton.GetBoneName(BoneIndex));
		}

		// convert to local space
		for (int32 BoneIndex = BoneTransform.Num() - 1; BoneIndex >= 0; --BoneIndex)
		{
			const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
			if (ParentIndex != INDEX_NONE)
			{
				BoneTransform[BoneIndex] = BoneTransform[BoneIndex].GetRelativeTransform(BoneTransform[ParentIndex]);
			}
		}

		const USkeleton* MeshSkeleton = MeshComponent->SkeletalMesh->Skeleton;
		const FSmartNameMapping* Mapping = MeshSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
		
		TArray<float> NewCurveValues;
		NewCurveValues.AddZeroed(PoseContainer.Curves.Num());

		if(Mapping)
		{
			const FBlendedHeapCurve& MeshCurves = MeshComponent->GetAnimationCurves();
 
			for (int32 NewCurveIndex = 0; NewCurveIndex < NewCurveValues.Num(); ++NewCurveIndex)
			{
				FAnimCurveBase& Curve = PoseContainer.Curves[NewCurveIndex];
				SmartName::UID_Type CurveUID = Mapping->FindUID(Curve.Name.DisplayName);
				if (CurveUID != SmartName::MaxUID)
				{
					const float MeshCurveValue = MeshCurves.Get(CurveUID);
					NewCurveValues[NewCurveIndex] = MeshCurveValue;
				}
			}
		}

		AddOrUpdatePose(PoseName, TrackNames, BoneTransform, NewCurveValues);
		PoseContainer.Shrink(MySkeleton, RetargetSource);
	}
}

void UPoseAsset::AddOrUpdatePose(const FSmartName& PoseName, const TArray<FName>& TrackNames, const TArray<FTransform>& LocalTransform, const TArray<float>& CurveValues)
{
	USkeleton* MySkeleton = GetSkeleton();
	if (MySkeleton)
	{
		// first combine track, we want to make sure all poses contains tracks with this
		CombineTracks(TrackNames);

		FPoseData* PoseData = PoseContainer.FindOrAddPoseData(PoseName);
		// now copy all transform back to it. 
		check(PoseData);
		int32 TotalTracks = TrackNames.Num();
		PoseData->LocalSpacePose.Reset(TotalTracks);
		PoseData->LocalSpacePose.AddUninitialized(TotalTracks);
		PoseData->LocalSpacePoseMask.SetNumZeroed(TotalTracks, true);
		PoseContainer.FillUpDefaultPose(PoseData, MySkeleton, RetargetSource);
		check(CurveValues.Num() == PoseContainer.Curves.Num());
		PoseData->CurveData = CurveValues;

		const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
		for (int32 Index = 0; Index < TrackNames.Num(); ++Index)
		{
			// now get poseData track index
			const FName& TrackName = TrackNames[Index];
			int32 SkeletonIndex = RefSkeleton.FindBoneIndex(TrackName);
			int32 InternalTrackIndex = PoseContainer.Tracks.Find(TrackName);
			PoseData->LocalSpacePose[InternalTrackIndex] = LocalTransform[Index];
			// shrink should take care of this but if somebody just calls this function, it should set it to dirty
			PoseData->LocalSpacePoseMask[InternalTrackIndex] = true;
		}
	}
}
#endif // WITH_EDITOR

void UPoseAsset::CombineTracks(const TArray<FName>& NewTracks)
{
	USkeleton* MySkeleton = GetSkeleton();
	if (MySkeleton)
	{
		for (const auto& NewTrack : NewTracks)
		{
			if (PoseContainer.Tracks.Contains(NewTrack) == false)
			{
				// if we don't have it, then we'll have to add this track and then 
				// right now it doesn't have to be in the hierarchy
				// @todo: it is probably best to keep the hierarchy of the skeleton, so in the future, we might like to sort this by track after
				PoseContainer.InsertTrack(NewTrack, MySkeleton, RetargetSource);
			}
		}
	}
}

#if WITH_EDITOR
void UPoseAsset::Reinitialize()
{
	PoseContainer.Reset();

	bAdditivePose = false;
	BasePoseIndex = INDEX_NONE;
}

void UPoseAsset::RenameSmartName(const FName& InOriginalName, const FName& InNewName)
{
	for (FSmartName SmartName : PoseContainer.PoseNames)
	{
		if (SmartName.DisplayName == InOriginalName)
		{
			SmartName.DisplayName = InNewName;
			break;
		}
	}

	for (FAnimCurveBase& Curve : PoseContainer.Curves)
	{
		if (Curve.Name.DisplayName == InOriginalName)
		{
			Curve.Name.DisplayName = InNewName;
			break;
		}
	}
}

void UPoseAsset::RemoveSmartNames(const TArray<FName>& InNamesToRemove)
{
	DeletePoses(InNamesToRemove);
	DeleteCurves(InNamesToRemove);
}

void UPoseAsset::CreatePoseFromAnimation(class UAnimSequence* AnimSequence, const TArray<FSmartName>* InPoseNames/*== nullptr*/)
{
	if (AnimSequence)
	{
		USkeleton* TargetSkeleton = AnimSequence->GetSkeleton();
		if (TargetSkeleton)
		{
			SetSkeleton(TargetSkeleton);
			SourceAnimation = AnimSequence;

			// reinitialize, now we're making new pose from this animation
			Reinitialize();

			const int32 NumPoses = AnimSequence->GetNumberOfFrames();

			// make sure we have more than one pose
			if (NumPoses > 0)
			{
				// stack allocator for extracting curve
				FMemMark Mark(FMemStack::Get());

				// set up track data - @todo: add revaliation code when checked
				for (const FName& TrackName : AnimSequence->GetAnimationTrackNames())
				{
					PoseContainer.Tracks.Add(TrackName);
				}

				// now create pose transform
				TArray<FTransform> NewPose;
				
				const int32 NumTracks = AnimSequence->GetAnimationTrackNames().Num();
				NewPose.Reset(NumTracks);
				NewPose.AddUninitialized(NumTracks);

				// @Todo fill up curve data
				TArray<float> CurveData;
				float IntervalBetweenKeys = (NumPoses > 1)? AnimSequence->SequenceLength / (NumPoses -1 ) : 0.f;

				// add curves - only float curves
				const int32 TotalFloatCurveCount = AnimSequence->RawCurveData.FloatCurves.Num();
				
				// have to construct own UIDList;
				TArray<SmartName::UID_Type> UIDList;

				if (TotalFloatCurveCount > 0)
				{
					for (const FFloatCurve& Curve : AnimSequence->RawCurveData.FloatCurves)
					{
						PoseContainer.Curves.Add(FAnimCurveBase(Curve.Name, Curve.GetCurveTypeFlags()));
						UIDList.Add(Curve.Name.UID);
					}
				}

				CurveData.AddZeroed(UIDList.Num());
				// add to skeleton UID, so that it knows the curve data
				for (int32 PoseIndex = 0; PoseIndex < NumPoses; ++PoseIndex)
				{
					FSmartName NewPoseName = (InPoseNames && InPoseNames->IsValidIndex(PoseIndex))? (*InPoseNames)[PoseIndex] : GetUniquePoseName(TargetSkeleton);
					// now get rawanimationdata, and each key is converted to new pose
					for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
					{
						const auto& RawTrack = AnimSequence->GetRawAnimationTrack(TrackIndex);
						AnimSequence->ExtractBoneTransform(RawTrack, NewPose[TrackIndex], PoseIndex);
					}

					if (TotalFloatCurveCount > 0)
					{
						// get curve data
						// have to do iterate over time
						// support curve
						FBlendedCurve SourceCurve;
						SourceCurve.InitFrom(&UIDList);
						AnimSequence->EvaluateCurveData(SourceCurve, PoseIndex*IntervalBetweenKeys, true);

						// copy back to CurveData
						for (int32 CurveIndex = 0; CurveIndex < CurveData.Num(); ++CurveIndex)
						{
							CurveData[CurveIndex] = SourceCurve.Get(UIDList[CurveIndex]);
						}

						check(CurveData.Num() == PoseContainer.Curves.Num());
					}
				
					// add new pose
					PoseContainer.AddOrUpdatePose(NewPoseName, NewPose, CurveData);
				}


				PoseContainer.Shrink(TargetSkeleton, RetargetSource);
				RecacheTrackmap();
			}
		}
	}
}

void UPoseAsset::UpdatePoseFromAnimation(class UAnimSequence* AnimSequence)
{
	if (AnimSequence)
	{
		// when you update pose, right now, it just only keeps pose names
		// in the future we might want to make it more flexible
		// back up old pose names
		const TArray<FSmartName> OldPoseNames = PoseContainer.PoseNames;
		const bool bOldAdditive = bAdditivePose;
		int32 OldBasePoseIndex = BasePoseIndex;
		CreatePoseFromAnimation(AnimSequence, &OldPoseNames);

		// fix up additive info if it's additive
		if (bOldAdditive)
		{
			if (PoseContainer.Poses.IsValidIndex(OldBasePoseIndex) == false)
			{
				// if it's pointing at invalid index, just reset to ref pose
				OldBasePoseIndex = INDEX_NONE;
			}

			// Convert to additive again
			ConvertToAdditivePose(OldBasePoseIndex);
		}

		OnPoseListChanged.Broadcast();
	}
}

bool UPoseAsset::ModifyPoseName(FName OldPoseName, FName NewPoseName, const SmartName::UID_Type* NewUID)
{
	USkeleton* MySkeleton = GetSkeleton();

	if (ContainsPose(NewPoseName))
	{
		// already exists, return 
		return false;
	}

	FSmartName OldPoseSmartName;
	ensureAlways(MySkeleton->GetSmartNameByName(USkeleton::AnimCurveMappingName, OldPoseName, OldPoseSmartName));

	if (FPoseData* PoseData = PoseContainer.FindPoseData(OldPoseSmartName))
	{
		FSmartName NewPoseSmartName;
		if (NewUID)
		{
			MySkeleton->GetSmartNameByUID(USkeleton::AnimCurveMappingName, *NewUID, NewPoseSmartName);
		}
		else
		{
			MySkeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, NewPoseName, NewPoseSmartName);
		}

		PoseContainer.RenamePose(OldPoseSmartName, NewPoseSmartName);
		OnPoseListChanged.Broadcast();

		return true;
	}

	return false;
}
#endif // WITH_EDITOR

int32 UPoseAsset::DeletePoses(TArray<FName> PoseNamesToDelete)
{
	int32 ItemsDeleted = 0;

	USkeleton* MySkeleton = GetSkeleton();

	for (const auto& PoseName : PoseNamesToDelete)
	{
		FSmartName PoseSmartName;
		if (MySkeleton->GetSmartNameByName(USkeleton::AnimCurveMappingName, PoseName, PoseSmartName) 
			&& 	PoseContainer.DeletePose(PoseSmartName))
		{
			++ItemsDeleted;
		}
	}

	PoseContainer.Shrink(GetSkeleton(), RetargetSource);
	OnPoseListChanged.Broadcast();

	return ItemsDeleted;
}

int32 UPoseAsset::DeleteCurves(TArray<FName> CurveNamesToDelete)
{
	int32 ItemsDeleted = 0;

	USkeleton* MySkeleton = GetSkeleton();

	for (const auto& CurveName : CurveNamesToDelete)
	{
		FSmartName CurveSmartName;
		if (MySkeleton->GetSmartNameByName(USkeleton::AnimCurveMappingName, CurveName, CurveSmartName))
		{
			PoseContainer.DeleteCurve(CurveSmartName);
			++ItemsDeleted;
		}
	}

	OnPoseListChanged.Broadcast();

	return ItemsDeleted;
}
bool UPoseAsset::ConvertToFullPose()
{
	if (ensureAlways(bAdditivePose))
	{
		TArray<FTransform>	BasePose;
		TArray<float>		BaseCurves;
		GetBasePoseTransform(BasePose, BaseCurves);

		PoseContainer.ConvertToFullPose(BasePoseIndex, BasePose, BaseCurves);

		bAdditivePose = false;

		return true;
	}

	return false;
}

bool UPoseAsset::ConvertToAdditivePose(int32 NewBasePoseIndex)
{
	if (ensureAlways(!bAdditivePose))
	{
		// make sure it's valid
		check(NewBasePoseIndex == -1 || PoseContainer.Poses.IsValidIndex(NewBasePoseIndex));

		BasePoseIndex = NewBasePoseIndex;

		TArray<FTransform> BasePose;
		TArray<float>		BaseCurves;
		GetBasePoseTransform(BasePose, BaseCurves);

		PoseContainer.ConvertToAdditivePose(BasePoseIndex, BasePose, BaseCurves);

		bAdditivePose = true;

		return true;
	}

	return false;
}

bool UPoseAsset::GetFullPose(int32 PoseIndex, TArray<FTransform>& OutTransforms) const
{
	if (!PoseContainer.Poses.IsValidIndex(PoseIndex))
	{
		return false;
	}

	bool bSuccess = false;

	if (bAdditivePose)
	{
		// if this pose is not base pose
		if (PoseIndex != BasePoseIndex)
		{
			TArray<FTransform> BasePose;
			TArray<float> BaseCurves;
			GetBasePoseTransform(BasePose, BaseCurves);

			const FPoseData& PoseData = PoseContainer.Poses[PoseIndex];
			OutTransforms.AddUninitialized(PoseData.LocalSpacePose.Num());

			const ScalarRegister AdditiveWeight(1.f);

			for (int32 BoneIdx = 0; BoneIdx < OutTransforms.Num(); ++BoneIdx)
			{
				OutTransforms[BoneIdx] = BasePose[BoneIdx];
				OutTransforms[BoneIdx].AccumulateWithAdditiveScale(PoseData.LocalSpacePose[BoneIdx], AdditiveWeight);
			}

			bSuccess = true;
		}
	}
	else
	{
		OutTransforms = PoseContainer.Poses[PoseIndex].LocalSpacePose;
		bSuccess = true;
	}

	return bSuccess;
}

bool UPoseAsset::ConvertSpace(bool bNewAdditivePose, int32 NewBasePoseInde)
{
	// first convert to full pose first
	if (bAdditivePose)
	{
		// convert to full pose
		if (!ConvertToFullPose())
		{
			// issue with converting full pose
			return false;
		}
	}

	// now we have full pose
	if (bNewAdditivePose)
	{
		ConvertToAdditivePose(NewBasePoseInde);
	}

	return true;
}

const int32 UPoseAsset::GetPoseIndexByName(const FName& InBasePoseName) const
{
	for (int32 PoseIndex = 0; PoseIndex < PoseContainer.PoseNames.Num(); ++PoseIndex)
	{
		if (PoseContainer.PoseNames[PoseIndex].DisplayName == InBasePoseName)
		{
			return PoseIndex;
		}
	}

	return INDEX_NONE;
}

const int32 UPoseAsset::GetCurveIndexByName(const FName& InCurveName) const
{
	for (int32 TestIdx = 0; TestIdx < PoseContainer.Curves.Num(); TestIdx++)
	{
		const FAnimCurveBase& Curve = PoseContainer.Curves[TestIdx];
		if (Curve.Name.DisplayName == InCurveName)
		{
			return TestIdx;
		}
	}
	return INDEX_NONE;
}


void UPoseAsset::RecacheTrackmap()
{
	USkeleton* MySkeleton = GetSkeleton();
	PoseContainer.TrackMap.Reset();

	if (MySkeleton)
	{
		const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();

		// set up track data - @todo: add revaliation code when checked
		for (int32 TrackIndex = 0; TrackIndex < PoseContainer.Tracks.Num(); ++TrackIndex)
		{
			const FName& TrackName = PoseContainer.Tracks[TrackIndex];
			const int32 SkeletonTrackIndex = RefSkeleton.FindBoneIndex(TrackName);
			ensureAlways(SkeletonTrackIndex != INDEX_NONE);
			PoseContainer.TrackMap.Add(TrackName, SkeletonTrackIndex);
		}
	}
}

#if WITH_EDITOR
void UPoseAsset::RemapTracksToNewSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces)
{
	Super::RemapTracksToNewSkeleton(NewSkeleton, bConvertSpaces);
	// @todo: should allow deleted tracks and so on
	RecacheTrackmap();
}

bool UPoseAsset::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive /*= true*/)
{
	Super::GetAllAnimationSequencesReferred(AnimationAssets, bRecursive);
	if (SourceAnimation)
	{
		SourceAnimation->HandleAnimReferenceCollection(AnimationAssets, bRecursive);
	}

	return AnimationAssets.Num() > 0;
}

void UPoseAsset::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap)
{
	Super::ReplaceReferredAnimations(ReplacementMap);
	if (SourceAnimation)
	{
		UAnimSequence* const* ReplacementAsset = (UAnimSequence*const*)ReplacementMap.Find(SourceAnimation);
		if (ReplacementAsset)
		{
			SourceAnimation = *ReplacementAsset;
		}
	}
}

#endif // WITH_EDITOR

bool UPoseAsset::GetBasePoseTransform(TArray<FTransform>& OutBasePose, TArray<float>& OutCurve) const
{
	int32 TotalNumTrack = PoseContainer.Tracks.Num();
	OutBasePose.Reset(TotalNumTrack);

	if (BasePoseIndex == -1)
	{
		OutBasePose.AddUninitialized(TotalNumTrack);
		for (int32 TrackIndex = 0; TrackIndex < PoseContainer.Tracks.Num(); ++TrackIndex)
		{
			const FName& TrackName = PoseContainer.Tracks[TrackIndex];
			OutBasePose[TrackIndex] = PoseContainer.GetDefaultTransform(TrackName, GetSkeleton(), RetargetSource);
		}

		// add zero curves
		OutCurve.AddZeroed(PoseContainer.Curves.Num());
		check(OutBasePose.Num() == TotalNumTrack);
		return true;
	}
	else if (PoseContainer.Poses.IsValidIndex(BasePoseIndex))
	{
		OutBasePose = PoseContainer.Poses[BasePoseIndex].LocalSpacePose;
		OutCurve = PoseContainer.Poses[BasePoseIndex].CurveData;
		check(OutBasePose.Num() == TotalNumTrack);
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE 
