// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	URig.cpp: Rig functionality for sharing animations
=============================================================================*/ 

#include "Animation/Rig.h"
#include "UObject/FrameworkObjectVersion.h"
#include "AnimationRuntime.h"

//@todo should move all this window stuff somewhere else. Persona?

#define LOCTEXT_NAMESPACE "Rig"

FName URig::WorldNodeName(TEXT("World"));

URig::URig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void URig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Nodes.Num() != TransformBases.Num())
	{
		int32 NodeNum = Nodes.Num();
		int32 TransformBasesNum = TransformBases.Num();

		// make sure to add name of nodes if not set
		for(int32 NewId=TransformBasesNum; NewId <NodeNum; ++NewId)
		{
			if (Nodes[NewId].Name == NAME_None)
			{
				int32 UniqueIndex = 1;

				while (1) 
				{
					// assign name of custom_# 
					FName PotentialName = *FString::Printf(TEXT("Custom_%d"), UniqueIndex++);
					if (FindNode(PotentialName) == INDEX_NONE)
					{
						Nodes[NewId].Name = PotentialName;
						Nodes[NewId].ParentName = WorldNodeName;
						Nodes[NewId].DisplayName = PotentialName.ToString();
						break;
					}
				} 
			}
		}

		// if their size doesn't match, make sure to add it
		if (NodeNum < TransformBasesNum)
		{
			// just remove the last elements
			TransformBases.RemoveAt(NodeNum, TransformBasesNum-NodeNum);
		}
		else
		{
			for (int32 NewId=TransformBasesNum; NewId <NodeNum; ++NewId)
			{
				AddRigConstraint(Nodes[NewId].Name, EControlConstraint::Translation, EConstraintTransform::Absolute, WorldNodeName, 1.f);
				AddRigConstraint(Nodes[NewId].Name, EControlConstraint::Orientation, EConstraintTransform::Absolute, WorldNodeName, 1.f);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

// for now these are privates since we don't have good control yet
bool URig::AddNode(FName Name, FName ParentNode, FTransform Transform)
{
	int32 NodeIndex = FindNode(Name);

	if (NodeIndex == INDEX_NONE)
	{
		Nodes.Add(FNode(Name, ParentNode, Transform));

		return true;
	}

	return false;
}

bool URig::DeleteNode(FName Name)
{
	int32 NodeIndex = FindNode(Name);

	if (NodeIndex != INDEX_NONE)
	{
		Nodes.RemoveAt(NodeIndex);
		return true;
	}

	return false;
}

int32 URig::FindNode(const FName& NodeName) const
{
	int32 Index=0;
	for (auto Node: Nodes)
	{
		if (Node.Name == NodeName)
		{
			return Index;
		}

		++Index;
	}

	return INDEX_NONE;
}

const FNode* URig::GetNode(int32 NodeIndex) const
{
	if ( Nodes.IsValidIndex(NodeIndex) )
	{
		return &Nodes[NodeIndex];
	}

	return NULL;
}

FName URig::GetNodeName(int32 NodeIndex) const
{
	if(Nodes.IsValidIndex(NodeIndex))
	{
		return Nodes[NodeIndex].Name;
	}

	return NAME_None;
}

FName URig::GetParentNode(FName& NodeName) const
{
	int32 NodeIndex = FindNode(NodeName);

	if (NodeIndex != INDEX_NONE)
	{
		if (Nodes[NodeIndex].ParentName != NAME_None)
		{
			return Nodes[NodeIndex].ParentName;
		}
	}

	return WorldNodeName;
}

bool URig::AddRigConstraint(FName NodeName, EControlConstraint::Type ConstraintType, EConstraintTransform::Type	TransformType, FName ParentSpace, float Weight /*= 1.f*/)
{
	if (ConstraintType == EControlConstraint::Type::MAX)
	{
		// invalid type
		return false;
	}

	// make sure the ParentSpace is valid
	int32 ParentIndex = FindNode(ParentSpace);
	if (ParentIndex == INDEX_NONE)
	{
		// if parent is invalid, set it to World
		ParentSpace = WorldNodeName;
	}

	int32 Index = FindTransformBaseByNodeName(NodeName);

	if (Index == INDEX_NONE)
	{
		FTransformBase NewTransformBase;
		NewTransformBase.Node = NodeName;

		FRigTransformConstraint NewTransformConstraint;
		NewTransformConstraint.TranformType = TransformType;
		NewTransformConstraint.ParentSpace = ParentSpace;
		NewTransformConstraint.Weight = Weight;

		NewTransformBase.Constraints[ConstraintType].TransformConstraints.Add(NewTransformConstraint);

		TransformBases.Add(NewTransformBase);
	}
	else
	{
		// it exists already, need to make sure we don't have different constraint types
		FTransformBase & TransformBase = TransformBases[Index];

		FTransformBaseConstraint & ControlConstraint = TransformBase.Constraints[ConstraintType];

		FRigTransformConstraint NewTransformConstraint;
		NewTransformConstraint.TranformType = TransformType;
		NewTransformConstraint.ParentSpace = ParentSpace;
		NewTransformConstraint.Weight = Weight;

		// add new transform constraint
		ControlConstraint.TransformConstraints.Add(NewTransformConstraint);
	}

	return true;
}

int32 URig::GetNodeNum() const
{
	return Nodes.Num();
}

int32 URig::GetTransformBaseNum() const
{
	return TransformBases.Num();
}

const FTransformBase* URig::GetTransformBase(int32 TransformBaseIndex) const
{
	if (TransformBases.IsValidIndex(TransformBaseIndex))
	{
		return &TransformBases[TransformBaseIndex];
	}

	return NULL;
}

const FTransformBase* URig::GetTransformBaseByNodeName(FName NodeName) const
{
	int32 TransformBaseIndex = FindTransformBaseByNodeName(NodeName);

	if (TransformBaseIndex != INDEX_NONE)
	{
		return &TransformBases[TransformBaseIndex];
	}

	return NULL;
}

int32 URig::FindTransformParentNode(int32 NodeIndex, bool bTranslate, int32 Index/*=0*/) const
{
	if (Nodes.IsValidIndex(NodeIndex))
	{
		const FTransformBase* TransformBase = GetTransformBaseByNodeName(Nodes[NodeIndex].Name);

		if(TransformBase)
		{
			FName ParentNodeName = NAME_None;
			if(bTranslate)
			{
				ParentNodeName = TransformBase->Constraints[EControlConstraint::Type::Translation].TransformConstraints[Index].ParentSpace;
			}
			else
			{
				ParentNodeName = TransformBase->Constraints[EControlConstraint::Type::Orientation].TransformConstraints[Index].ParentSpace;
			}

			if(ParentNodeName != NAME_None)
			{
				return  FindNode(ParentNodeName);
			}
		}
	}

	return INDEX_NONE;
}

int32 URig::FindTransformBaseByNodeName(FName NodeName) const
{
	int32 Index=0;

	for (auto TransformBase : TransformBases)
	{
		if (TransformBase.Node == NodeName)
		{
			return Index;
		}

		++Index;
	}

	return INDEX_NONE;
}

void URig::CreateFromSkeleton(const USkeleton* Skeleton, const TMap<int32, int32> & RequiredBones)
{
	// show dialog to choose which bone to add
	if(RequiredBones.Num() >0)
	{
		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		SourceSkeleton = RefSkeleton;
		TArray<FTransform> SpaceBaseRefPose;

		FAnimationRuntime::FillUpComponentSpaceTransformsRefPose(Skeleton, SpaceBaseRefPose);

		// once selected, add node to the rig
		for (auto It = RequiredBones.CreateConstIterator(); It; ++It)
		{
			int32 BoneIndex = (int32)(It.Key());
			int32 ParentIndex = (int32)(It.Value());

			check (BoneIndex != INDEX_NONE);

			FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
			FName ParentBoneName = WorldNodeName;
			if (ParentIndex != INDEX_NONE)
			{
				ParentBoneName = RefSkeleton.GetBoneName(ParentIndex);
			}

			const FTransform& Transform = SpaceBaseRefPose[BoneIndex];

			AddNode(BoneName, ParentBoneName, Transform);
		}

		// add constraint to parent space and relative transform 
		for(auto It = RequiredBones.CreateConstIterator(); It; ++It)
		{
			int32 BoneIndex = (int32)(It.Key());
			int32 ParentIndex = (int32)(It.Value());

			FName BoneName = RefSkeleton.GetBoneName(BoneIndex);

			if (ParentIndex == INDEX_NONE)
			{
				AddRigConstraint(BoneName, EControlConstraint::Translation, EConstraintTransform::Absolute, WorldNodeName, 1.f);
				AddRigConstraint(BoneName, EControlConstraint::Orientation, EConstraintTransform::Absolute, WorldNodeName, 1.f);
			}
			else
			{
				FName ParentBoneName = RefSkeleton.GetBoneName(ParentIndex);
				AddRigConstraint(BoneName, EControlConstraint::Translation, EConstraintTransform::Absolute, ParentBoneName, 1.f);
				AddRigConstraint(BoneName, EControlConstraint::Orientation, EConstraintTransform::Absolute, ParentBoneName, 1.f);
			}
		}
	}
}

/*
void URig::CalculateComponentSpace(int32 NodeIndex, const FTransform& LocalTransform, const TArray<FTransform> & TransformBuffer, const FGetParentIndex& DelegateToGetParentIndex, FTransform& OutComponentSpaceTransform) const
{
	int32 ConstraintIndex = FindTransformBaseByNodeName(Nodes[NodeIndex].Name);

	if (ConstraintIndex != INDEX_NONE)
	{
		// now find transform constraint data
		{
			const FTransformBaseConstraint& Constraints = TransformBases[ConstraintIndex].Constraints[EControlConstraint::Type::Orientation];

			// for now we only care for the first one
			const FName ParentName = Constraints.TransformConstraints[0].ParentSpace;
			int32 ParentIndex = INDEX_NONE;

			if(DelegateToGetParentIndex.IsBound())
			{
				ParentIndex = DelegateToGetParentIndex.Execute(ParentName);
			}
			else
			{
				ParentIndex = FindTransformParentNode(NodeIndex, false);
			}

			if(ParentIndex != INDEX_NONE && TransformBuffer.IsValidIndex(ParentIndex))
			{
				FQuat ParentRotation = TransformBuffer[ParentIndex].GetRotation();
				OutComponentSpaceTransform.SetRotation(ParentRotation * LocalTransform.GetRotation());
			}
		}

		// same thing for translation
		{
			const FTransformBaseConstraint& Constraints = TransformBases[ConstraintIndex].Constraints[EControlConstraint::Type::Translation];

			// for now we only care for the first one
			const FName ParentName = Constraints.TransformConstraints[0].ParentSpace;
			int32 ParentIndex = INDEX_NONE;

			if(DelegateToGetParentIndex.IsBound())
			{
				ParentIndex = DelegateToGetParentIndex.Execute(ParentName);
			}
			else
			{
				ParentIndex = FindTransformParentNode(NodeIndex, false);
			}

			if(ParentIndex != INDEX_NONE && TransformBuffer.IsValidIndex(ParentIndex))
			{
				FTransform ParentTransform = TransformBuffer[ParentIndex];
				OutComponentSpaceTransform.SetTranslation(ParentTransform.TransformVector(LocalTransform.GetTranslation()));
			}
		}
		// @todo fix this
		OutComponentSpaceTransform.SetScale3D(LocalTransform.GetScale3D());
	}
}

void URig::CalculateLocalSpace(int32 NodeIndex, const FTransform& ComponentTransform, const TArray<FTransform> & TransformBuffer, const FGetParentIndex& DelegateToGetParentIndex, FTransform& OutLocalSpaceTransform) const
{
	int32 ConstraintIndex = FindTransformBaseByNodeName(Nodes[NodeIndex].Name);

	if(ConstraintIndex != INDEX_NONE)
	{
		// now find transform constraint data
		{
			const FTransformBaseConstraint& Constraints = TransformBases[ConstraintIndex].Constraints[EControlConstraint::Type::Orientation];

			// for now we only care for the first one
			const FName ParentName = Constraints.TransformConstraints[0].ParentSpace;
			int32 ParentIndex = INDEX_NONE;

			if(DelegateToGetParentIndex.IsBound())
			{
				ParentIndex = DelegateToGetParentIndex.Execute(ParentName);
			}
			else
			{
				ParentIndex = FindTransformParentNode(NodeIndex, false);
			}

			if(ParentIndex != INDEX_NONE && TransformBuffer.IsValidIndex(ParentIndex))
			{
				FQuat ParentRotation = TransformBuffer[ParentIndex].GetRotation();
				OutLocalSpaceTransform.SetRotation(ParentRotation.Inverse() * ComponentTransform.GetRotation());
			}
		}

		// same thing for translation
		{
			const FTransformBaseConstraint& Constraints = TransformBases[ConstraintIndex].Constraints[EControlConstraint::Type::Translation];

			// for now we only care for the first one
			const FName ParentName = Constraints.TransformConstraints[0].ParentSpace;
			int32 ParentIndex = INDEX_NONE;

			if(DelegateToGetParentIndex.IsBound())
			{
				ParentIndex = DelegateToGetParentIndex.Execute(ParentName);
			}
			else
			{
				ParentIndex = FindTransformParentNode(NodeIndex, false);
			}

			if(ParentIndex != INDEX_NONE && TransformBuffer.IsValidIndex(ParentIndex))
			{
				FTransform LocalTransform = ComponentTransform.GetRelativeTransform(TransformBuffer[ParentIndex]);
				OutLocalSpaceTransform.SetTranslation(LocalTransform.GetTranslation());
			}
		}

		// @todo fix this
		OutLocalSpaceTransform.SetScale3D(ComponentTransform.GetScale3D());
	}
}*/

void URig::SetAllConstraintsToParents()
{
	for(auto & Control : TransformBases)
	{
		FName ParentNode = GetParentNode(Control.Node);

		Control.Constraints[EControlConstraint::Type::Translation].TransformConstraints[0].ParentSpace = ParentNode;
		Control.Constraints[EControlConstraint::Type::Orientation].TransformConstraints[0].ParentSpace = ParentNode;
	}
}
void URig::SetAllConstraintsToWorld()
{
	for (auto & Control : TransformBases)
	{
		Control.Constraints[EControlConstraint::Type::Translation].TransformConstraints[0].ParentSpace = WorldNodeName;
		Control.Constraints[EControlConstraint::Type::Orientation].TransformConstraints[0].ParentSpace = WorldNodeName;
	}
}

#if WITH_EDITORONLY_DATA
void URig::SetSourceReferenceSkeleton(const FReferenceSkeleton& InSrcSkeleton)
{
	SourceSkeleton = InSrcSkeleton;
	MarkPackageDirty();
}
#endif // WITH_EDITORONLY_DATA
#endif // WITH_EDITOR

void URig::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) >= FFrameworkObjectVersion::AddSourceReferenceSkeletonToRig)
	{
#if WITH_EDITORONLY_DATA
		Ar << SourceSkeleton;
#else
		FReferenceSkeleton Dummy;
		Ar << Dummy;
#endif 
	}
}

#undef LOCTEXT_NAMESPACE 
