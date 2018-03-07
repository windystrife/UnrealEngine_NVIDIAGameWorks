// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_PoseDriver.h"
#include "Kismet2/CompilerResultsLog.h"
#include "AnimNodeEditModes.h"
#include "RBF/RBFSolver.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/SkeletalMeshComponent.h"

#define LOCTEXT_NAMESPACE "PoseDriver"

struct FPoseDriverCustomVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,
		// Add RBFData
		AddRBFData = 1,
		// Add multi-bone input support
		MultiBoneInput = 2,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FPoseDriverCustomVersion() {}
};

const FGuid FPoseDriverCustomVersion::GUID(0xAFE08691, 0x3A0D4952, 0xB673673B, 0x7CF22D1E);
FCustomVersionRegistration GRegisterPoseDriverCustomVersion(FPoseDriverCustomVersion::GUID, FPoseDriverCustomVersion::LatestVersion, TEXT("PoseDriverVer"));


//////////////////////////////////////////////////////////////////////////

UAnimGraphNode_PoseDriver::UAnimGraphNode_PoseDriver(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	SelectedTargetIndex = INDEX_NONE;
}

FText UAnimGraphNode_PoseDriver::GetTooltipText() const
{
	return LOCTEXT("UAnimGraphNode_PoseDriver_ToolTip", "Drive parameters base on a bones distance from a set of defined poses.");
}

FText UAnimGraphNode_PoseDriver::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("PoseDriver", "Pose Driver");
}

FText UAnimGraphNode_PoseDriver::GetMenuCategory() const
{
	return LOCTEXT("PoseAssetCategory_Label", "Poses");
}


void UAnimGraphNode_PoseDriver::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	if (Node.SourceBones.Num() == 0)
	{
		MessageLog.Warning(*LOCTEXT("PoseDriver_NoSourceBone", "You must specify at least one Source Bone").ToString(), this);
	}

	FName MissingBoneName = NAME_None;
	for (const FBoneReference& BoneRef : Node.SourceBones)
	{
		if (ForSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneRef.BoneName) == INDEX_NONE)
		{
			MissingBoneName = BoneRef.BoneName;
			break;
		}
	}

	if(MissingBoneName != NAME_None)
	{
		MessageLog.Warning(*LOCTEXT("SourceBoneNotFound", "Entry in SourceBones not found").ToString(), this);
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

FEditorModeID UAnimGraphNode_PoseDriver::GetEditorMode() const
{
	return AnimNodeEditModes::PoseDriver;
}

EAnimAssetHandlerType UAnimGraphNode_PoseDriver::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UPoseAsset::StaticClass()))
	{
		return EAnimAssetHandlerType::Supported;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

void UAnimGraphNode_PoseDriver::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerCustomVersion(FPoseDriverCustomVersion::GUID) < FPoseDriverCustomVersion::MultiBoneInput)
	{
		if (Node.SourceBone_DEPRECATED.BoneName != NAME_None)
		{
			Node.SourceBones.Add(Node.SourceBone_DEPRECATED);
		}
	}

	if (GetLinkerCustomVersion(FPoseDriverCustomVersion::GUID) < FPoseDriverCustomVersion::AddRBFData)
	{
		// Convert distance method
		if (Node.Type_DEPRECATED == EPoseDriverType::SwingAndTwist)
		{
			Node.DriveSource = EPoseDriverSource::Rotation;
			Node.RBFParams.DistanceMethod = ERBFDistanceMethod::Quaternion;
		}
		else if (Node.Type_DEPRECATED == EPoseDriverType::SwingOnly)
		{
			Node.DriveSource = EPoseDriverSource::Rotation;
			Node.RBFParams.DistanceMethod = ERBFDistanceMethod::SwingAngle;
		}
		else
		{
			Node.DriveSource = EPoseDriverSource::Translation;
			Node.RBFParams.DistanceMethod = ERBFDistanceMethod::Euclidean;
		}

		// Copy twist axis
		Node.RBFParams.TwistAxis = Node.TwistAxis_DEPRECATED;

		// Copy target data from pose asset
		CopyTargetsFromPoseAsset();

		// Set per-target scales
		float MaxDistance;
		AutoSetTargetScales(MaxDistance);

		// Set radiust to be max distance, and apply old overall radius scaling
		Node.RBFParams.Radius = MaxDistance * Node.RadialScaling_DEPRECATED;

		// Recompile if required to propagate changes to AnimInstance Class
		UAnimBlueprint* AnimBP = GetAnimBlueprint();
		if (AnimBP)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBP);
		}
	}
}


void UAnimGraphNode_PoseDriver::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FPoseDriverCustomVersion::GUID);
}

void UAnimGraphNode_PoseDriver::CopyNodeDataToPreviewNode(FAnimNode_Base* InPreviewNode)
{
	FAnimNode_PoseDriver* PreviewPoseDriver = static_cast<FAnimNode_PoseDriver*>(InPreviewNode);

	PreviewPoseDriver->RBFParams.Radius = Node.RBFParams.Radius;
	PreviewPoseDriver->RBFParams.Function = Node.RBFParams.Function;
	PreviewPoseDriver->RBFParams.DistanceMethod = Node.RBFParams.DistanceMethod;
	PreviewPoseDriver->RBFParams.TwistAxis = Node.RBFParams.TwistAxis;
	PreviewPoseDriver->RBFParams.WeightThreshold = Node.RBFParams.WeightThreshold;
	PreviewPoseDriver->DriveOutput = Node.DriveOutput;
	PreviewPoseDriver->DriveSource = Node.DriveSource;
	PreviewPoseDriver->PoseTargets = Node.PoseTargets;
	PreviewPoseDriver->bCachedDrivenIDsAreDirty = true;
}

FAnimNode_PoseDriver* UAnimGraphNode_PoseDriver::GetPreviewPoseDriverNode() const
{
	FAnimNode_PoseDriver* PreviewNode = nullptr;

	if (LastPreviewComponent != nullptr && LastPreviewComponent->GetAnimInstance() != nullptr)
	{
		PreviewNode = static_cast<FAnimNode_PoseDriver*>(FindDebugAnimNode(LastPreviewComponent));
	}

	return PreviewNode;
}

/** Util to return transform of a bone from the pose asset in component space, by walking up tracks in pose asset */
FTransform GetComponentSpaceTransform(FName BoneName, TArray<FTransform>& LocalTransforms, UPoseAsset* PoseAsset)
{
	const FReferenceSkeleton& RefSkel = PoseAsset->GetSkeleton()->GetReferenceSkeleton();

	// Init component space transform with local transform
	FTransform ComponentSpaceTransform = FTransform::Identity;

	// Start to walk up parent chain until we reach root (ParentIndex == INDEX_NONE)
	int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
	while (BoneIndex != INDEX_NONE)
	{
		BoneName = RefSkel.GetBoneName(BoneIndex);
		int32 TrackIndex = PoseAsset->GetTrackIndexByName(BoneName);

		// If a track for parent, get local space transform from that
		// If not, get from ref pose
		FTransform BoneLocalTM = (TrackIndex != INDEX_NONE) ? LocalTransforms[TrackIndex] : RefSkel.GetRefBonePose()[BoneIndex];

		// Continue to build component space transform
		ComponentSpaceTransform = ComponentSpaceTransform * BoneLocalTM;

		// Now move up to parent
		BoneIndex = RefSkel.GetParentIndex(BoneIndex);
	}

	return ComponentSpaceTransform;
}

void UAnimGraphNode_PoseDriver::CopyTargetsFromPoseAsset()
{
	UPoseAsset* PoseAsset = Node.PoseAsset;

	if (PoseAsset && PoseAsset->GetSkeleton()) // Use PoseAsset here not CurrentPoseAsset, because we want to be able to run this on on nodes that have not been init'd yet
	{
		Node.PoseTargets.Empty();

		// For each pose we create a target
		const TArray<FSmartName> PoseNames = PoseAsset->GetPoseNames();
		for (int32 PoseIdx = 0; PoseIdx < PoseAsset->GetNumPoses(); PoseIdx++)
		{
			FPoseDriverTarget PoseTarget;
			PoseTarget.DrivenName = PoseNames[PoseIdx].DisplayName;

			// Create entry for each bone
			for (const FBoneReference& SourceBoneRef : Node.SourceBones)
			{
				FTransform SourceBoneTransform = FTransform::Identity;

				// Don't want to create target for base pose in additive case
				bool bIsBasePose = (PoseAsset->IsValidAdditive() && PoseIdx == PoseAsset->GetBasePoseIndex());
				if (!bIsBasePose)
				{
					// Get transforms from pose (this also converts from additive if necessary)
					TArray<FTransform> PoseTransforms;
					if (PoseAsset->GetFullPose(PoseIdx, PoseTransforms))
					{
						// If eval'ing in different space (and that space is valid)
						if (Node.EvalSpaceBone.BoneName != NAME_None)
						{
							FTransform SourceCompSpace = GetComponentSpaceTransform(SourceBoneRef.BoneName, PoseTransforms, PoseAsset);
							FTransform EvalCompSpace = GetComponentSpaceTransform(Node.EvalSpaceBone.BoneName, PoseTransforms, PoseAsset);

							SourceBoneTransform = SourceCompSpace.GetRelativeTransform(EvalCompSpace);
						}
						else
						{
							// Check we have a track for the source bone
							int32 SourceTrackIndex = PoseAsset->GetTrackIndexByName(SourceBoneRef.BoneName);
							if (SourceTrackIndex != INDEX_NONE)
							{
								SourceBoneTransform = PoseTransforms[SourceTrackIndex];
							}
						}
					}
				}

				// If we got a valid transform, add a pose target now
				FPoseDriverTransform PoseTransform;
				PoseTransform.TargetTranslation = SourceBoneTransform.GetTranslation();
				PoseTransform.TargetRotation = SourceBoneTransform.Rotator();

				PoseTarget.BoneTransforms.Add(PoseTransform);
			}

			Node.PoseTargets.Add(PoseTarget);
		}

		Node.bCachedDrivenIDsAreDirty = true;
	}
}

void UAnimGraphNode_PoseDriver::AddNewTarget()
{
	FPoseDriverTarget& NewTarget = Node.PoseTargets[Node.PoseTargets.Add(FPoseDriverTarget())];

	// Create entry for each bone
	NewTarget.BoneTransforms.AddDefaulted(Node.SourceBones.Num());
}


void UAnimGraphNode_PoseDriver::ReserveTargetTransforms()
{
	// reallocate transforms array in each target
	for(FPoseDriverTarget& PoseTarget : Node.PoseTargets)
	{
		PoseTarget.BoneTransforms.SetNum(Node.SourceBones.Num());
	}
}

void UAnimGraphNode_PoseDriver::AutoSetTargetScales(float &OutMaxDistance)
{
	TArray<FRBFTarget> RBFTargets;
	Node.GetRBFTargets(RBFTargets);

	// Find distances from targets to nearest neighbours
	TArray<float> Distances;
	bool bSuccess = FRBFSolver::FindTargetNeighbourDistances(Node.RBFParams, RBFTargets, Distances);
	if (bSuccess)
	{
		// Find overall largest distance 
		OutMaxDistance = KINDA_SMALL_NUMBER; // ensure result > 0
		for (float Distance : Distances)
		{
			OutMaxDistance = FMath::Max(OutMaxDistance, Distance);
		}

		// Set scales so largest distance is 1.0, and others are less than that
		for (int32 TargetIdx = 0; TargetIdx < Node.PoseTargets.Num(); TargetIdx++)
		{
			FPoseDriverTarget& PoseTarget = Node.PoseTargets[TargetIdx];
			PoseTarget.TargetScale = Distances[TargetIdx] / OutMaxDistance;
		}
	}
}

#undef LOCTEXT_NAMESPACE
