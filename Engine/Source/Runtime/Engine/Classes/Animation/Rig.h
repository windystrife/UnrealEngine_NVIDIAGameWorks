// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/** 
 * This is the definition for a Rig that is used for retargeting animations
 *
 */

#pragma once

// @todo should I keep the hierarchy or not? - does it matter if hand is child of shoulder? - I think we should keep hierarchy
// @todo should we support reset data 
// @todo does it make sense to have "no constraint" on certain data? What does that mean? Just World? 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ReferenceSkeleton.h"
#include "Rig.generated.h"

class USkeleton;
struct FPropertyChangedEvent;

/** Rig Controller for bone transform **/
USTRUCT()
struct FNode
{
	GENERATED_USTRUCT_BODY()

	/** Name of the original node. We don't allow to change this. This is used for identity.**/
	UPROPERTY(VisibleAnywhere, Category="FNode")
	FName Name;

	/** We save Parent Node but if the parent node is removed, it will reset to root */
	UPROPERTY(VisibleAnywhere, Category="FNode")
	FName ParentName;

	/** Absolute transform of the node. Hoping to use this data in the future to render*/
	UPROPERTY()
	FTransform Transform;

	/** This is Display Name where it will be used to display in Retarget Manager. This name has to be unique. */
	UPROPERTY(EditAnywhere, Category="FNode")
	FString DisplayName;

	UPROPERTY(EditAnywhere, Category="FNode")
	bool bAdvanced;

	FNode()
		: Name(NAME_None)
		, ParentName(NAME_None)
		, bAdvanced(false)
	{
	}

	FNode(FName InNodeName, FName InParentName, const FTransform& InTransform)
		: Name(InNodeName)
		, ParentName(InParentName)
		, Transform(InTransform)
		, DisplayName(InNodeName.ToString())
		, bAdvanced(false)
	{
	}
};

/** Control Constraint Type */
UENUM()
namespace EControlConstraint
{
	enum Type
	{
		/** Rotation constraint. */
		Orientation,
		/** Translation constraint. */
		Translation,

		/** Max Number. */
		MAX
	};
}

/** Constraint Transform Type. - currently unused */
UENUM()
namespace EConstraintTransform
{
	enum Type
	{
		/** Absolute value. */
		Absolute,
		/** Apply relative transform from ref pose. */
		Relative,
	};
}

USTRUCT()
struct FRigTransformConstraint
{
	GENERATED_USTRUCT_BODY()

	/** What transform type **/
	UPROPERTY(/*EditAnywhere, Category="FTransformBaseConstraint"*/)
	TEnumAsByte<EConstraintTransform::Type>	TranformType;

	/** Parent space that are define **/
	UPROPERTY(EditAnywhere, Category="FTransformBaseConstraint")
	FName	ParentSpace;

	/** Weight of the influence - for future*/
	UPROPERTY(/*EditAnywhere, Category="FTransformBaseConstraint"*/)
	float	Weight;
};

/** This defines what constraint it is defined */
USTRUCT()
struct FTransformBaseConstraint
{
	GENERATED_USTRUCT_BODY()

	/** What transform type **/
	UPROPERTY(EditAnywhere, Category="FTransformBaseConstraint")
	TArray<FRigTransformConstraint>			TransformConstraints;
};


/** This is a mapping table between bone in a particular skeletal mesh and bone of this skeleton set. */
USTRUCT()
struct FTransformBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category="FTransformBase")
	FName					Node;

	UPROPERTY(EditAnywhere, Category="FTransformBase")
	FTransformBaseConstraint	Constraints[EControlConstraint::Type::MAX];
};

DECLARE_DELEGATE_RetVal_OneParam(int32, FGetParentIndex, FName);

/**
 *	URig : that has rigging data for skeleton
 *		- used for retargeting
 *		- support to share different animations
 */
UCLASS(hidecategories=Object, MinimalAPI)
class URig : public UObject
{
	GENERATED_UCLASS_BODY()

private:

	/** Skeleton bone tree - each contains name and parent index**/
	UPROPERTY(EditAnywhere, Category = Rig, EditFixedSize)
	TArray<FTransformBase> TransformBases;

	/** Skeleton bone tree - each contains name and parent index**/
	UPROPERTY(EditAnywhere, Category=Rig, EditFixedSize)
	TArray<FNode> Nodes;

#if WITH_EDITORONLY_DATA
	// this is source skeleton it's created from. 
	// since all node data can be modified after, 
	// to figure out what was original source skeleton, this is better to have it for reset and getting original transform
	struct FReferenceSkeleton SourceSkeleton;
#endif
public:

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	// node related functions
	int32 GetNodeNum() const;
	const FNode* GetNode(int32 NodeIndex) const;
	FName GetNodeName(int32 NodeIndex) const;
	FName GetParentNode(FName& NodeName) const;
	int32 FindNode(const FName& NodeName) const;
	const TArray<FNode> & GetNodes() const { return Nodes; }

	// create from skeleton
	ENGINE_API void CreateFromSkeleton(const USkeleton* Skeleton, const TMap<int32, int32>& RequiredBones);
	ENGINE_API void SetAllConstraintsToParents();
	ENGINE_API void SetAllConstraintsToWorld();

	// rig control related
	int32 GetTransformBaseNum() const;
	const TArray<FTransformBase> & GetTransformBases() const { return TransformBases; }
	const FTransformBase* GetTransformBase(int32 TransformBaseIndex) const;
	const FTransformBase* GetTransformBaseByNodeName(FName NodeName) const;
	int32 FindTransformBaseByNodeName(FName NodeName) const;
	int32 FindTransformParentNode(int32 NodeIndex, bool bTranslate, int32 Index=0) const;

#if WITH_EDITORONLY_DATA
	// source skeleton related, since this has been added later, it's possible 
	// some skeletons don't have it
	bool IsSourceReferenceSkeletonAvailable() const { return SourceSkeleton.GetRawBoneNum() > 0; }
	const FReferenceSkeleton& GetSourceReferenceSkeleton() const { return SourceSkeleton;  }
	ENGINE_API void SetSourceReferenceSkeleton(const FReferenceSkeleton& InSrcSkeleton);
#endif // WITH_EDITORONLY_DATA
#endif // WITH_EDITOR

	// not sure if we'd like to keep this
	ENGINE_API static FName WorldNodeName;

private:

#if WITH_EDITOR
	// for now these are privates since we don't have good control yet
	bool AddNode(FName Name, FName ParentNode, FTransform Transform);
	bool DeleteNode(FName Name);
	// rig constraint 
	bool AddRigConstraint(FName NodeName, EControlConstraint::Type	ConstraintType, EConstraintTransform::Type	TranformType, FName ParentSpace, float Weight = 1.f);

#endif // WITH_EDITOR
	
	// not useful so far
//	void CalculateComponentSpace(int32 NodeIndex, const FTransform& LocalTransform, const TArray<FTransform> & TransformBuffer, const FGetParentIndex& DelegateToGetParentIndex, FTransform& OutComponentSpaceTransform) const;
//	void CalculateLocalSpace(int32 NodeIndex, const FTransform& ComponentTransform, const TArray<FTransform> & TransformBuffer, const FGetParentIndex& DelegateToGetParentIndex, FTransform& OutLocalSpaceTransform) const;
};

