// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_SkeletalControlBase.h"
#include "UnrealWidget.h"
#include "AnimationGraphSchema.h"
#include "Animation/AnimationSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_SkeletalControlBase

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_SkeletalControlBase::UAnimGraphNode_SkeletalControlBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UAnimGraphNode_SkeletalControlBase::GetWidgetCoordinateSystem(const USkeletalMeshComponent* SkelComp)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (GetWidgetMode(SkelComp) == FWidget::WM_Scale)
	{
		return COORD_Local;
	}
	else
	{
		return COORD_World;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

// returns int32 instead of EWidgetMode because of compiling issue on Mac
int32 UAnimGraphNode_SkeletalControlBase::GetWidgetMode(const USkeletalMeshComponent* SkelComp)
{
	return  (int32)FWidget::EWidgetMode::WM_None;
}

int32 UAnimGraphNode_SkeletalControlBase::ChangeToNextWidgetMode(const USkeletalMeshComponent* SkelComp, int32 CurWidgetMode)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GetWidgetMode(SkelComp);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FName UAnimGraphNode_SkeletalControlBase::FindSelectedBone()
{
	return NAME_None;
}

FLinearColor UAnimGraphNode_SkeletalControlBase::GetNodeTitleColor() const
{
	return FLinearColor(0.75f, 0.75f, 0.10f);
}

FString UAnimGraphNode_SkeletalControlBase::GetNodeCategory() const
{
	return TEXT("Skeletal Control Nodes");
}

FText UAnimGraphNode_SkeletalControlBase::GetControllerDescription() const
{
	return LOCTEXT("ImplementMe", "Implement me");
}

FText UAnimGraphNode_SkeletalControlBase::GetTooltipText() const
{
	return GetControllerDescription();
}

void UAnimGraphNode_SkeletalControlBase::CreateOutputPins()
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	CreatePin(EGPD_Output, Schema->PC_Struct, FString(), FComponentSpacePoseLink::StaticStruct(), TEXT("Pose"));
}


void UAnimGraphNode_SkeletalControlBase::ConvertToComponentSpaceTransform(const USkeletalMeshComponent* SkelComp, const FTransform & InTransform, FTransform & OutCSTransform, int32 BoneIndex, EBoneControlSpace Space) const
{
	USkeleton * Skeleton = SkelComp->SkeletalMesh->Skeleton;

	switch (Space)
	{
	case BCS_WorldSpace:
	{
		OutCSTransform = InTransform;
		OutCSTransform.SetToRelativeTransform(SkelComp->GetComponentTransform());
	}
		break;

	case BCS_ComponentSpace:
	{
		// Component Space, no change.
		OutCSTransform = InTransform;
	}
		break;

	case BCS_ParentBoneSpace:
		if (BoneIndex != INDEX_NONE)
		{
			const int32 ParentIndex = Skeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);
			if (ParentIndex != INDEX_NONE)																																																																																																																															
			{
				const int32 MeshParentIndex = Skeleton->GetMeshBoneIndexFromSkeletonBoneIndex(SkelComp->SkeletalMesh, ParentIndex);
				if (MeshParentIndex != INDEX_NONE)
				{
					const FTransform ParentTM = SkelComp->GetBoneTransform(MeshParentIndex);
					OutCSTransform = InTransform * ParentTM;
				}
				else
				{
					OutCSTransform = InTransform;
				}
			}
		}
		break;

	case BCS_BoneSpace:
		if (BoneIndex != INDEX_NONE)
		{
			const int32 MeshBoneIndex = Skeleton->GetMeshBoneIndexFromSkeletonBoneIndex(SkelComp->SkeletalMesh, BoneIndex);
			if (MeshBoneIndex != INDEX_NONE)
			{
				const FTransform BoneTM = SkelComp->GetBoneTransform(MeshBoneIndex);
				OutCSTransform = InTransform * BoneTM;
			}			
			else
			{
				OutCSTransform = InTransform;
			}
		}
		break;

	default:
		if (SkelComp->SkeletalMesh)
		{
			UE_LOG(LogAnimation, Warning, TEXT("ConvertToComponentSpaceTransform: Unknown BoneSpace %d  for Mesh: %s"), (uint8)Space, *SkelComp->SkeletalMesh->GetFName().ToString());
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("ConvertToComponentSpaceTransform: Unknown BoneSpace %d  for Skeleton: %s"), (uint8)Space, *Skeleton->GetFName().ToString());
		}
		break;
	}
}


FVector UAnimGraphNode_SkeletalControlBase::ConvertCSVectorToBoneSpace(const USkeletalMeshComponent* SkelComp, FVector& InCSVector, FCSPose<FCompactHeapPose>& MeshBases, const FName& BoneName, const EBoneControlSpace Space)
{
	FVector OutVector = InCSVector;

	if (MeshBases.GetPose().IsValid())
	{
		const FMeshPoseBoneIndex MeshBoneIndex(SkelComp->GetBoneIndex(BoneName));
		const FCompactPoseBoneIndex BoneIndex = MeshBases.GetPose().GetBoneContainer().MakeCompactPoseIndex(MeshBoneIndex);

		switch (Space)
		{
			// World Space, no change in preview window
		case BCS_WorldSpace:
		case BCS_ComponentSpace:
			// Component Space, no change.
			break;

		case BCS_ParentBoneSpace:
		{
			const FCompactPoseBoneIndex ParentIndex = MeshBases.GetPose().GetParentBoneIndex(BoneIndex);
			if (ParentIndex != INDEX_NONE)
			{
				const FTransform& ParentTM = MeshBases.GetComponentSpaceTransform(ParentIndex);
				OutVector = ParentTM.InverseTransformVector(InCSVector);
			}
		}
			break;

		case BCS_BoneSpace:
		{
			const FTransform& BoneTM = MeshBases.GetComponentSpaceTransform(BoneIndex);
			OutVector = BoneTM.InverseTransformVector(InCSVector);
		}
			break;
		}
	}

	return OutVector;
}

FQuat UAnimGraphNode_SkeletalControlBase::ConvertCSRotationToBoneSpace(const USkeletalMeshComponent* SkelComp, FRotator& InCSRotator, FCSPose<FCompactHeapPose>& MeshBases, const FName& BoneName, const EBoneControlSpace Space)
{
	FQuat OutQuat = FQuat::Identity;

	if (MeshBases.GetPose().IsValid())
	{
		const FMeshPoseBoneIndex MeshBoneIndex(SkelComp->GetBoneIndex(BoneName));
		const FCompactPoseBoneIndex BoneIndex = MeshBases.GetPose().GetBoneContainer().MakeCompactPoseIndex(MeshBoneIndex);

		FVector RotAxis;
		float RotAngle;
		InCSRotator.Quaternion().ToAxisAndAngle(RotAxis, RotAngle);

		switch (Space)
		{
			// World Space, no change in preview window
		case BCS_WorldSpace:
		case BCS_ComponentSpace:
			// Component Space, no change.
			OutQuat = InCSRotator.Quaternion();
			break;

		case BCS_ParentBoneSpace:
		{
			const FCompactPoseBoneIndex ParentIndex = MeshBases.GetPose().GetParentBoneIndex(BoneIndex);
			if (ParentIndex != INDEX_NONE)
			{
				const FTransform& ParentTM = MeshBases.GetComponentSpaceTransform(ParentIndex);
				FTransform InverseParentTM = ParentTM.Inverse();
				//Calculate the new delta rotation
				FVector4 BoneSpaceAxis = InverseParentTM.TransformVector(RotAxis);
				FQuat DeltaQuat(BoneSpaceAxis, RotAngle);
				DeltaQuat.Normalize();
				OutQuat = DeltaQuat;
			}
		}
			break;

		case BCS_BoneSpace:
		{
			const FTransform& BoneTM = MeshBases.GetComponentSpaceTransform(BoneIndex);
			FTransform InverseBoneTM = BoneTM.Inverse();
			FVector4 BoneSpaceAxis = InverseBoneTM.TransformVector(RotAxis);
			//Calculate the new delta rotation
			FQuat DeltaQuat(BoneSpaceAxis, RotAngle);
			DeltaQuat.Normalize();
			OutQuat = DeltaQuat;
		}
			break;
		}
	}

	return OutQuat;
}

FVector UAnimGraphNode_SkeletalControlBase::ConvertWidgetLocation(const USkeletalMeshComponent* SkelComp, FCSPose<FCompactHeapPose>& MeshBases, const FName& BoneName, const FVector& Location, const EBoneControlSpace Space)
{
	FVector WidgetLoc = FVector::ZeroVector;

	if (MeshBases.GetPose().IsValid())
	{
		USkeleton * Skeleton = SkelComp->SkeletalMesh->Skeleton;
		const FMeshPoseBoneIndex MeshBoneIndex(SkelComp->GetBoneIndex(BoneName));
		const FCompactPoseBoneIndex CompactBoneIndex = MeshBases.GetPose().GetBoneContainer().MakeCompactPoseIndex(MeshBoneIndex);

		switch (Space)
		{
			// GetComponentTransform() must be Identity in preview window so same as ComponentSpace
		case BCS_WorldSpace:
		case BCS_ComponentSpace:
		{
			// Component Space, no change.
			WidgetLoc = Location;
		}
			break;

		case BCS_ParentBoneSpace:

			if (CompactBoneIndex != INDEX_NONE)
			{
				const FCompactPoseBoneIndex CompactParentIndex = MeshBases.GetPose().GetParentBoneIndex(CompactBoneIndex);
				if (CompactParentIndex != INDEX_NONE)
				{
					const FTransform& ParentTM = MeshBases.GetComponentSpaceTransform(CompactParentIndex);
					WidgetLoc = ParentTM.TransformPosition(Location);
				}
			}
			break;

		case BCS_BoneSpace:

			if (CompactBoneIndex != INDEX_NONE)
			{
				const FTransform& BoneTM = MeshBases.GetComponentSpaceTransform(CompactBoneIndex);
				WidgetLoc = BoneTM.TransformPosition(Location);
			}
		}
	}

	return WidgetLoc;
}

void UAnimGraphNode_SkeletalControlBase::GetDefaultValue(const FString& UpdateDefaultValueName, FVector& OutVec)
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->PinName == UpdateDefaultValueName)
		{
			if (GetSchema()->IsCurrentPinDefaultValid(Pin).IsEmpty())
			{
				FString DefaultString = Pin->GetDefaultAsString();

				// Existing nodes (from older versions) might have an empty default value string; in that case we just fall through and return the zero vector below (which is the default value in that case).
				if(!DefaultString.IsEmpty())
				{
					TArray<FString> ResultString;

					//Parse string to split its contents separated by ','
					DefaultString.TrimStartAndEndInline();
					DefaultString.ParseIntoArray(ResultString, TEXT(","), true);

					check(ResultString.Num() == 3);

					OutVec.Set(
						FCString::Atof(*ResultString[0]),
						FCString::Atof(*ResultString[1]),
						FCString::Atof(*ResultString[2])
						);
					return;
				}
			}
		}
	}
	OutVec = FVector::ZeroVector;
}

void UAnimGraphNode_SkeletalControlBase::SetDefaultValue(const FString& UpdateDefaultValueName, const FVector& Value)
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->PinName == UpdateDefaultValueName)
		{
			if (GetSchema()->IsCurrentPinDefaultValid(Pin).IsEmpty())
			{
				FString Str = FString::Printf(TEXT("%.3f,%.3f,%.3f"), Value.X, Value.Y, Value.Z);
				if (Pin->DefaultValue != Str)
				{
					PreEditChange(NULL);
					GetSchema()->TrySetDefaultValue(*Pin, Str);
					PostEditChange();
					break;
				}
			}
		}
	}
}

bool UAnimGraphNode_SkeletalControlBase::IsPinShown(const FString& PinName) const
{
	for (const FOptionalPinFromProperty& Pin : ShowPinForProperties)
	{
		if (Pin.PropertyName.ToString() == PinName)
		{
			return Pin.bShowPin;
		}
	}
	return false;
}

void UAnimGraphNode_SkeletalControlBase::ValidateAnimNodePostCompile(FCompilerResultsLog& MessageLog, UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex)
{
	if (UAnimationSettings::Get()->bEnablePerformanceLog)
	{
		const FAnimNode_SkeletalControlBase* Node = GetNode();
		if (Node && Node->LODThreshold < 0)
		{
			MessageLog.Warning(TEXT("@@ contains no LOD Threshold."), this);
		}
	}
}

#undef LOCTEXT_NAMESPACE
