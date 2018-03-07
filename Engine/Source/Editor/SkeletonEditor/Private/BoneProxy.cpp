// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BoneProxy.h"
#include "IPersonaPreviewScene.h"
#include "AnimNode_ModifyBone.h"
#include "AnimPreviewInstance.h"
#include "ScopedTransaction.h"
#include "Animation/DebugSkelMeshComponent.h"

#define LOCTEXT_NAMESPACE "BoneProxy"

UBoneProxy::UBoneProxy()
	: bLocalLocation(true)
	, bLocalRotation(true)
	, PreviousLocation(FVector::ZeroVector)
	, PreviousRotation(FRotator::ZeroRotator)
	, PreviousScale(FVector::ZeroVector)
	, bManipulating(false)
	, bIsTickable(false)
{
}

void UBoneProxy::Tick(float DeltaTime)
{
	if (!bManipulating)
	{
		if (UDebugSkelMeshComponent* Component = SkelMeshComponent.Get())
		{
			int32 BoneIndex = Component->GetBoneIndex(BoneName);
			if (Component->BoneSpaceTransforms.IsValidIndex(BoneIndex))
			{
				FTransform LocalTransform = Component->BoneSpaceTransforms[BoneIndex];
				FTransform BoneTransform = Component->GetBoneTransform(BoneIndex);

				if (bLocalLocation)
				{
					Location = LocalTransform.GetLocation();
				}
				else
				{
					Location = BoneTransform.GetLocation();
				}

				if (bLocalRotation)
				{
					Rotation = LocalTransform.GetRotation().Rotator();
				}
				else
				{
					Rotation = BoneTransform.GetRotation().Rotator();
				}

				Scale = LocalTransform.GetScale3D();

				FTransform ReferenceTransform = Component->SkeletalMesh->RefSkeleton.GetRefBonePose()[BoneIndex];
				ReferenceLocation = ReferenceTransform.GetLocation();
				ReferenceRotation = ReferenceTransform.GetRotation().Rotator();
				ReferenceScale = ReferenceTransform.GetScale3D();
			}
		}
	}
}

bool UBoneProxy::IsTickable() const
{
	return bIsTickable;
}

TStatId UBoneProxy::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBoneProxy, STATGROUP_Tickables);
}

void UBoneProxy::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	if (UDebugSkelMeshComponent* Component = SkelMeshComponent.Get())
	{
		if (Component->PreviewInstance && Component->AnimScriptInstance == Component->PreviewInstance)
		{
			bManipulating = true;

			Component->PreviewInstance->Modify();

			if (PropertyAboutToChange.GetActiveMemberNode()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Location))
			{
				PreviousLocation = Location;
			}
			else if (PropertyAboutToChange.GetActiveMemberNode()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Rotation))
			{
				PreviousRotation = Rotation;
			}
			else if (PropertyAboutToChange.GetActiveMemberNode()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Scale))
			{
				PreviousScale = Scale;
			}
		}
	}
}

void UBoneProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.Property != nullptr)
	{
		if (UDebugSkelMeshComponent* Component = SkelMeshComponent.Get())
		{
			if (Component->PreviewInstance && Component->AnimScriptInstance == Component->PreviewInstance)
			{
				bManipulating = (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive);

				int32 BoneIndex = Component->GetBoneIndex(BoneName);
				if (BoneIndex != INDEX_NONE && BoneIndex < Component->GetNumComponentSpaceTransforms())
				{
					FTransform BoneTransform = Component->GetBoneTransform(BoneIndex);
					FMatrix BoneLocalCoordSystem = Component->GetBoneTransform(BoneIndex).ToMatrixNoScale().RemoveTranslation();
					FAnimNode_ModifyBone& ModifyBone = Component->PreviewInstance->ModifyBone(BoneName);
					FTransform ModifyBoneTransform(ModifyBone.Rotation, ModifyBone.Translation, ModifyBone.Scale);
					FTransform BaseTransform = BoneTransform.GetRelativeTransformReverse(ModifyBoneTransform);

					if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Location))
					{
						FVector Delta = (Location - PreviousLocation);
						if (!Delta.IsNearlyZero())
						{
							if (bLocalLocation)
							{
								Delta = BoneLocalCoordSystem.TransformPosition(Delta);
							}

							FVector BoneSpaceDelta = BaseTransform.TransformVector(Delta);
							ModifyBone.Translation += BoneSpaceDelta;
						}
					}
					else if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Rotation))
					{
						FRotator Delta = (Rotation - PreviousRotation);
						if (!Delta.IsNearlyZero())
						{					
							if (bLocalRotation)
							{
								// get delta in current coord space
								Delta = (BoneLocalCoordSystem.Inverse() * FRotationMatrix(Delta) * BoneLocalCoordSystem).Rotator();
							}

							FVector RotAxis;
							float RotAngle;
							Delta.Quaternion().ToAxisAndAngle(RotAxis, RotAngle);

							FVector4 BoneSpaceAxis = BaseTransform.TransformVectorNoScale(RotAxis);

							//Calculate the new delta rotation
							FQuat NewDeltaQuat(BoneSpaceAxis, RotAngle);
							NewDeltaQuat.Normalize();

							FRotator NewRotation = (ModifyBoneTransform * FTransform(NewDeltaQuat)).Rotator();
							ModifyBone.Rotation = NewRotation;
						}
					}
					else if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Scale))
					{
						FVector Delta = (Scale - PreviousScale);
						if (!Delta.IsNearlyZero())
						{
							ModifyBone.Scale += Delta;
						}
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE