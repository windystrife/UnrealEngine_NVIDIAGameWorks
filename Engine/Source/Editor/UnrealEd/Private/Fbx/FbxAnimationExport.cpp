// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
  Implementation of animation export related functionality from FbxExporter
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "Animation/AnimTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Animation/AnimSequence.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Matinee/MatineeActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "FbxExporter.h"
#include "Exporters/FbxExportOption.h"

DEFINE_LOG_CATEGORY_STATIC(LogFbxAnimationExport, Log, All);

namespace UnFbx
{


void FFbxExporter::ExportAnimSequenceToFbx(const UAnimSequence* AnimSeq,
									 const USkeletalMesh* SkelMesh,
									 TArray<FbxNode*>& BoneNodes,
									 FbxAnimLayer* InAnimLayer,
									 float AnimStartOffset,
									 float AnimEndOffset,
									 float AnimPlayRate,
									 float StartTime)
{
	USkeleton* Skeleton = AnimSeq->GetSkeleton();

	if (AnimSeq->SequenceLength == 0.f)
	{
		// something is wrong
		return;	
	}

	const float FrameRate			=  AnimSeq->NumFrames / AnimSeq->SequenceLength;

	// set time correctly
	FbxTime ExportedStartTime, ExportedStopTime;
	if ( FMath::IsNearlyEqual(FrameRate, DEFAULT_SAMPLERATE, 1.f) )
	{
		ExportedStartTime.SetGlobalTimeMode(FbxTime::eFrames30);
		ExportedStopTime.SetGlobalTimeMode(FbxTime::eFrames30);
	}
	else
	{
		ExportedStartTime.SetGlobalTimeMode(FbxTime::eCustom, FrameRate);
		ExportedStopTime.SetGlobalTimeMode(FbxTime::eCustom, FrameRate);
	}

	ExportedStartTime.SetSecondDouble(0.f);
	ExportedStopTime.SetSecondDouble(AnimSeq->SequenceLength);

	FbxTimeSpan ExportedTimeSpan;
	ExportedTimeSpan.Set(ExportedStartTime, ExportedStopTime);
	AnimStack->SetLocalTimeSpan(ExportedTimeSpan);
	
	// Add the animation data to the bone nodes
	for(int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex)
	{
		FbxNode* CurrentBoneNode = BoneNodes[BoneIndex];

		// Create the AnimCurves
		const uint32 NumberOfCurves = 9;
		FbxAnimCurve* Curves[NumberOfCurves];
		
		// Individual curves for translation, rotation and scaling
		Curves[0] = CurrentBoneNode->LclTranslation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[1] = CurrentBoneNode->LclTranslation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[2] = CurrentBoneNode->LclTranslation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
		
		Curves[3] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[4] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[5] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
		
		Curves[6] = CurrentBoneNode->LclScaling.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[7] = CurrentBoneNode->LclScaling.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[8] = CurrentBoneNode->LclScaling.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		float AnimTime					= AnimStartOffset;
		float AnimEndTime				= (AnimSeq->SequenceLength - AnimEndOffset);
		// Subtracts 1 because NumFrames includes an initial pose for 0.0 second
		double TimePerKey				= (AnimSeq->SequenceLength / (AnimSeq->NumFrames-1));
		const float AnimTimeIncrement	= TimePerKey * AnimPlayRate;

		FbxTime ExportTime;
		ExportTime.SetSecondDouble(StartTime);

		FbxTime ExportTimeIncrement;
		ExportTimeIncrement.SetSecondDouble( TimePerKey );

		int32 BoneTreeIndex = Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex(SkelMesh, BoneIndex);
		int32 BoneTrackIndex = Skeleton->GetAnimationTrackIndex(BoneTreeIndex, AnimSeq, true);
		if(BoneTrackIndex == INDEX_NONE)
		{
			// If this sequence does not have a track for the current bone, then skip it
			continue;
		}
		
		for (FbxAnimCurve* Curve : Curves)
		{
			Curve->KeyModifyBegin();
		}

		bool bLastKey = false;
		// Step through each frame and add the bone's transformation data
		while (!bLastKey)
		{
			FTransform BoneAtom;
			AnimSeq->GetBoneTransform(BoneAtom, BoneTrackIndex, AnimTime, true);

			FbxVector4 Translation = Converter.ConvertToFbxPos(BoneAtom.GetTranslation());
			FbxVector4 Rotation = Converter.ConvertToFbxRot(BoneAtom.GetRotation().Euler());
			FbxVector4 Scale = Converter.ConvertToFbxScale(BoneAtom.GetScale3D());
			FbxVector4 Vectors[3] = { Translation, Rotation, Scale };
		
			int32 lKeyIndex;

			bLastKey = AnimTime >= AnimEndTime;
			
			// Loop over each curve and channel to set correct values
			for (uint32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex)
			{
				for (uint32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
				{
					uint32 OffsetCurveIndex = (CurveIndex * 3) + ChannelIndex;

					lKeyIndex = Curves[OffsetCurveIndex]->KeyAdd(ExportTime);
					Curves[OffsetCurveIndex]->KeySetValue(lKeyIndex, Vectors[CurveIndex][ChannelIndex]);
					Curves[OffsetCurveIndex]->KeySetInterpolation(lKeyIndex, bLastKey ? FbxAnimCurveDef::eInterpolationConstant : FbxAnimCurveDef::eInterpolationCubic);

					if (bLastKey)
					{
						Curves[OffsetCurveIndex]->KeySetConstantMode(lKeyIndex, FbxAnimCurveDef::eConstantStandard);
					}
				}				
			}

			ExportTime += ExportTimeIncrement;
			AnimTime += AnimTimeIncrement;
		}

		for (FbxAnimCurve* Curve : Curves)
		{
			Curve->KeyModifyEnd();
		}
	}
}


// The curve code doesn't differentiate between angles and other data, so an interpolation from 179 to -179
// will cause the bone to rotate all the way around through 0 degrees.  So here we make a second pass over the 
// rotation tracks to convert the angles into a more interpolation-friendly format.  
void FFbxExporter::CorrectAnimTrackInterpolation( TArray<FbxNode*>& BoneNodes, FbxAnimLayer* InAnimLayer )
{
	// Add the animation data to the bone nodes
	for(int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex)
	{
		FbxNode* CurrentBoneNode = BoneNodes[BoneIndex];

		// Fetch the AnimCurves
		FbxAnimCurve* Curves[3];
		Curves[0] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[1] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[2] = CurrentBoneNode->LclRotation.GetCurve(InAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		for(int32 CurveIndex = 0; CurveIndex < 3; ++CurveIndex)
		{
			FbxAnimCurve* CurrentCurve = Curves[CurveIndex];
			CurrentCurve->KeyModifyBegin();

			float CurrentAngleOffset = 0.f;
			for(int32 KeyIndex = 1; KeyIndex < CurrentCurve->KeyGetCount(); ++KeyIndex)
			{
				float PreviousOutVal	= CurrentCurve->KeyGetValue( KeyIndex-1 );
				float CurrentOutVal		= CurrentCurve->KeyGetValue( KeyIndex );

				float DeltaAngle = (CurrentOutVal + CurrentAngleOffset) - PreviousOutVal;

				if(DeltaAngle >= 180)
				{
					CurrentAngleOffset -= 360;
				}
				else if(DeltaAngle <= -180)
				{
					CurrentAngleOffset += 360;
				}

				CurrentOutVal += CurrentAngleOffset;

				CurrentCurve->KeySetValue(KeyIndex, CurrentOutVal);
			}

			CurrentCurve->KeyModifyEnd();
		}
	}
}


FbxNode* FFbxExporter::ExportAnimSequence( const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, bool bExportSkelMesh, const TCHAR* MeshName, FbxNode* ActorRootNode )
{
	if( Scene == NULL || AnimSeq == NULL || SkelMesh == NULL )
	{
 		return NULL;
	}


	FbxNode* RootNode = (ActorRootNode)? ActorRootNode : Scene->GetRootNode();
	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton(SkelMesh, BoneNodes);
	RootNode->AddChild(SkeletonRootNode);


	// Export the anim sequence
	{
		ExportAnimSequenceToFbx(AnimSeq,
			SkelMesh,
			BoneNodes,
			AnimLayer,
			0.f,		// AnimStartOffset
			0.f,		// AnimEndOffset
			1.f,		// AnimPlayRate
			0.f);		// StartTime

		CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);
	}


	// Optionally export the mesh
	if(bExportSkelMesh)
	{
		FString MeshNodeName;
		
		if (MeshName)
		{
			MeshNodeName = MeshName;
		}
		else
		{
			SkelMesh->GetName(MeshNodeName);
		}

		// Add the mesh
		FbxNode* MeshRootNode = CreateMesh(SkelMesh, *MeshNodeName);
		if(MeshRootNode)
		{
			RootNode->AddChild(MeshRootNode);
		}

		if(SkeletonRootNode && MeshRootNode)
		{
			// Bind the mesh to the skeleton
			BindMeshToSkeleton(SkelMesh, MeshRootNode, BoneNodes);

			// Add the bind pose
			CreateBindPose(MeshRootNode);
		}
	}

	return SkeletonRootNode;
}


void FFbxExporter::ExportAnimSequencesAsSingle( USkeletalMesh* SkelMesh, const ASkeletalMeshActor* SkelMeshActor, const FString& ExportName, const TArray<UAnimSequence*>& AnimSeqList, const TArray<struct FAnimControlTrackKey>& TrackKeys )
{
	if (Scene == NULL || SkelMesh == NULL || AnimSeqList.Num() == 0 || AnimSeqList.Num() != TrackKeys.Num()) return;

	FbxNode* BaseNode = FbxNode::Create(Scene, Converter.ConvertToFbxString(ExportName));
	Scene->GetRootNode()->AddChild(BaseNode);

	if( SkelMeshActor )
	{
		// Set the default position of the actor on the transforms
		// The Unreal transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		BaseNode->LclTranslation.Set(Converter.ConvertToFbxPos(SkelMeshActor->GetActorLocation()));
		BaseNode->LclRotation.Set(Converter.ConvertToFbxRot(SkelMeshActor->GetActorRotation().Euler()));
		BaseNode->LclScaling.Set(Converter.ConvertToFbxScale(SkelMeshActor->GetRootComponent()->RelativeScale3D));

	}

	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton(SkelMesh, BoneNodes);
	BaseNode->AddChild(SkeletonRootNode);

	bool bAnyObjectMissingSourceData = false;
	float ExportStartTime = 0.f;
	for(int32 AnimSeqIndex = 0; AnimSeqIndex < AnimSeqList.Num(); ++AnimSeqIndex)
	{
		const UAnimSequence* AnimSeq = AnimSeqList[AnimSeqIndex];
		const FAnimControlTrackKey& TrackKey = TrackKeys[AnimSeqIndex];

		// Shift the anim sequences so the first one is at time zero in the FBX file
		const float CurrentStartTime = TrackKey.StartTime - ExportStartTime;

		ExportAnimSequenceToFbx(AnimSeq,
			SkelMesh,
			BoneNodes,
			AnimLayer,
			TrackKey.AnimStartOffset,
			TrackKey.AnimEndOffset,
			TrackKey.AnimPlayRate,
			CurrentStartTime);
	}

	CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);

	if (bAnyObjectMissingSourceData)
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Exporter_Error_SourceDataUnavailable", "No source data available for some objects.  See the log for details.") );
	}

}



/**
 * Exports all the animation sequences part of a single Group in a Matinee sequence
 * as a single animation in the FBX document.  The animation is created by sampling the
 * sequence at DEFAULT_SAMPLERATE updates/second and extracting the resulting bone transforms from the given
 * skeletal mesh
 */
void FFbxExporter::ExportMatineeGroup(class AMatineeActor* MatineeActor, USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (Scene == NULL || MatineeActor == NULL || SkeletalMeshComponent == NULL || MatineeActor->MatineeData->InterpLength == 0)
	{
		return;
	}

	FbxString NodeName("MatineeSequence");

	FbxNode* BaseNode = FbxNode::Create(Scene, NodeName);
	Scene->GetRootNode()->AddChild(BaseNode);

	AActor* Owner = SkeletalMeshComponent->GetOwner();
	if(Owner && Owner->GetRootComponent())
	{
		// Set the default position of the actor on the transforms
		// The UE3 transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		BaseNode->LclTranslation.Set(Converter.ConvertToFbxPos(Owner->GetActorLocation()));
		BaseNode->LclRotation.Set(Converter.ConvertToFbxRot(Owner->GetActorRotation().Euler()));
		BaseNode->LclScaling.Set(Converter.ConvertToFbxScale(Owner->GetRootComponent()->RelativeScale3D));
	}
	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton(SkeletalMeshComponent->SkeletalMesh, BoneNodes);
	FbxSkeletonRoots.Add(SkeletalMeshComponent, SkeletonRootNode);
	BaseNode->AddChild(SkeletonRootNode);

	FMatineeAnimTrackAdapter AnimTrackAdapter(MatineeActor);
	ExportAnimTrack(AnimTrackAdapter, Owner, SkeletalMeshComponent);
}

void FFbxExporter::ExportAnimTrack(IAnimTrackAdapter& AnimTrackAdapter, AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent)
{
	static const float SamplingRate = 1.f / DEFAULT_SAMPLERATE;

	float AnimationStart = AnimTrackAdapter.GetAnimationStart();
	float AnimationLength = AnimTrackAdapter.GetAnimationLength();
	float AnimationEnd = AnimationStart + AnimationLength;
	// show a status update every 1 second worth of samples
	const float UpdateFrequency = 1.0f;
	float NextUpdateTime = UpdateFrequency;

	// find root and find the bone array
	TArray<FbxNode*> BoneNodes;

	if ( FindSkeleton(SkeletalMeshComponent, BoneNodes)==false )
	{
		// error
		return;
	}

	FTransform InitialInvParentTransform;

	float SampleTime;
	for(SampleTime = AnimationStart; SampleTime <= AnimationEnd; SampleTime += SamplingRate)
	{
		if (SampleTime == AnimationStart)
		{
			InitialInvParentTransform = Actor->GetRootComponent()->GetComponentTransform().Inverse();
		}

		// This will call UpdateSkelPose on the skeletal mesh component to move bones based on animations in the matinee group
		AnimTrackAdapter.UpdateAnimation( SampleTime );

		// Update space bases so new animation position has an effect.
		// @todo - hack - this will be removed at some point
		SkeletalMeshComponent->TickAnimation(0.03f, false);

		SkeletalMeshComponent->RefreshBoneTransforms();
		SkeletalMeshComponent->RefreshSlaveComponents();
		SkeletalMeshComponent->UpdateComponentToWorld();
		SkeletalMeshComponent->FinalizeBoneTransform();
		SkeletalMeshComponent->MarkRenderTransformDirty();
		SkeletalMeshComponent->MarkRenderDynamicDataDirty();

		FbxTime ExportTime;
		ExportTime.SetSecondDouble(SampleTime);

		NextUpdateTime -= SamplingRate;

		if( NextUpdateTime <= 0.0f )
		{
			NextUpdateTime = UpdateFrequency;
			GWarn->StatusUpdate( FMath::RoundToInt( SampleTime ), FMath::RoundToInt(AnimationLength), NSLOCTEXT("FbxExporter", "ExportingToFbxStatus", "Exporting to FBX") );
		}

		// Add the animation data to the bone nodes
		for(int32 BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex)
		{
			FName BoneName = SkeletalMeshComponent->SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex);
			FbxNode* CurrentBoneNode = BoneNodes[BoneIndex];

			// Create the AnimCurves
			FbxAnimCurve* Curves[6];
			Curves[0] = CurrentBoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			Curves[1] = CurrentBoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			Curves[2] = CurrentBoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

			Curves[3] = CurrentBoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			Curves[4] = CurrentBoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			Curves[5] = CurrentBoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

			for(int32 i = 0; i < 6; ++i)
			{
				Curves[i]->KeyModifyBegin();
			}

			FTransform BoneTransform = SkeletalMeshComponent->BoneSpaceTransforms[BoneIndex];

			if (ExportOptions->MapSkeletalMotionToRoot && BoneIndex == 0)
			{
				BoneTransform = SkeletalMeshComponent->GetSocketTransform(BoneName) * InitialInvParentTransform;
			}

			FbxVector4 Translation = Converter.ConvertToFbxPos(BoneTransform.GetLocation());
			FbxVector4 Rotation = Converter.ConvertToFbxRot(BoneTransform.GetRotation().Euler());

			int32 lKeyIndex;

			for(int32 i = 0, j=3; i < 3; ++i, ++j)
			{
				lKeyIndex = Curves[i]->KeyAdd(ExportTime);
				Curves[i]->KeySetValue(lKeyIndex, Translation[i]);
				Curves[i]->KeySetInterpolation(lKeyIndex, FbxAnimCurveDef::eInterpolationCubic);

				lKeyIndex = Curves[j]->KeyAdd(ExportTime);
				Curves[j]->KeySetValue(lKeyIndex, Rotation[i]);
				Curves[j]->KeySetInterpolation(lKeyIndex, FbxAnimCurveDef::eInterpolationCubic);
			}

			for(int32 i = 0; i < 6; ++i)
			{
				Curves[i]->KeyModifyEnd();
			}
		}
	}

	CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);
}

} // namespace UnFbx
