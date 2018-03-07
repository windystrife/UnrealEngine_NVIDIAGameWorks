// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetUtils.h"
#include "Modules/ModuleManager.h"
#include "MeshUtilities.h"
#include "ConvexDecompTool.h"
#include "Logging/MessageLog.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PreviewScene.h"
#include "ScopedSlowTask.h"
#include "SkinnedBoneTriangleCache.h"

namespace FPhysicsAssetUtils
{
	static const float	DefaultPrimSize = 15.0f;
	static const float	MinPrimSize = 0.5f;

	// Forward declarations
	bool CreateCollisionFromBoneInternal(UBodySetup* bs, USkeletalMesh* skelMesh, int32 BoneIndex, const FPhysAssetCreateParams& Params, const FBoneVertInfo& Info, const FSkinnedBoneTriangleCache& TriangleCache);

/** Returns INDEX_NONE if no children in the visual asset or if more than one parent */
static int32 GetChildIndex(int32 BoneIndex, USkeletalMesh* SkelMesh, const TArray<FBoneVertInfo>& Infos)
{
	int32 ChildIndex = INDEX_NONE;

	for(int32 i=0; i<SkelMesh->RefSkeleton.GetRawBoneNum(); i++)
	{
		int32 ParentIndex = SkelMesh->RefSkeleton.GetParentIndex(i);

		if (ParentIndex == BoneIndex && Infos[i].Positions.Num() > 0)
		{
			if(ChildIndex != INDEX_NONE)
			{
				return INDEX_NONE; // if we already have a child, this bone has more than one so return INDEX_NONE.
			}
			else
			{
				ChildIndex = i;
			}
		}
	}

	return ChildIndex;
}

static float CalcBoneInfoLength(const FBoneVertInfo& Info)
{
	FBox BoneBox(ForceInit);
	for(int32 j=0; j<Info.Positions.Num(); j++)
	{
		BoneBox += Info.Positions[j];
	}

	if(BoneBox.IsValid)
	{
		FVector BoxExtent = BoneBox.GetExtent();
		return BoxExtent.Size();
	}
	else
	{
		return 0.f;
	}
}

/**
 * For all bones below the give bone index, find each ones minimum box dimension, and return the maximum over those bones.
 * This is used to decide if we should create physics for a bone even if its small, because there are good-sized bones below it.
 */
static float GetMaximalMinSizeBelow(int32 BoneIndex, USkeletalMesh* SkelMesh, const TArray<FBoneVertInfo>& Infos)
{
	check( Infos.Num() == SkelMesh->RefSkeleton.GetRawBoneNum() );

	UE_LOG(LogPhysics, Log, TEXT("-------------------------------------------------"));

	float MaximalMinBoxSize = 0.f;

	// For all bones that are children of the supplied one...
	for(int32 i=BoneIndex; i<SkelMesh->RefSkeleton.GetRawBoneNum(); i++)
	{
		if( SkelMesh->RefSkeleton.BoneIsChildOf(i, BoneIndex) )
		{
			float MinBoneDim = CalcBoneInfoLength( Infos[i] );
			
			UE_LOG(LogPhysics, Log,  TEXT("Parent: %s Bone: %s Size: %f"), *SkelMesh->RefSkeleton.GetBoneName(BoneIndex).ToString(), *SkelMesh->RefSkeleton.GetBoneName(i).ToString(), MinBoneDim );

			MaximalMinBoxSize = FMath::Max(MaximalMinBoxSize, MinBoneDim);
		}
	}

	return MaximalMinBoxSize;
}

void AddInfoToParentInfo(const FTransform& LocalToParentTM, const FBoneVertInfo& ChildInfo, FBoneVertInfo& ParentInfo)
{
	ParentInfo.Positions.Reserve(ParentInfo.Positions.Num() + ChildInfo.Positions.Num());
	ParentInfo.Positions.Reserve(ParentInfo.Normals.Num() + ChildInfo.Normals.Num());

	for(const FVector& Pos : ChildInfo.Positions)
	{
		ParentInfo.Positions.Add( LocalToParentTM.TransformPosition(Pos) );
	}

	for (const FVector& Normal : ChildInfo.Normals)
	{
		ParentInfo.Normals.Add(LocalToParentTM.TransformVectorNoScale(Normal));
	}
}

bool CreateFromSkeletalMeshInternal(UPhysicsAsset* PhysicsAsset, USkeletalMesh* SkelMesh, const FPhysAssetCreateParams& Params, const FSkinnedBoneTriangleCache& TriangleCache)
{
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	// For each bone, get the vertices most firmly attached to it.
	TArray<FBoneVertInfo> Infos;
	MeshUtilities.CalcBoneVertInfos(SkelMesh, Infos, (Params.VertWeight == EVW_DominantWeight));
	check(Infos.Num() == SkelMesh->RefSkeleton.GetRawBoneNum());

	PhysicsAsset->CollisionDisableTable.Empty();

	//Given the desired min body size we work from the children up to "merge" bones together. We go from leaves up because usually fingers, toes, etc... are small bones that should be merged
	//The strategy is as follows:
	//If bone is big enough, make a body
	//If not, add bone to parent for possible merge

	const TArray<FTransform> LocalPose = SkelMesh->RefSkeleton.GetRefBonePose();
	TMap<int32, FBoneVertInfo> BoneToMergedBones;
	TMap<FName, TArray<FName>> BoneNameToMergedParent;
	const int32 NumBones = Infos.Num();

	TArray<float> MergedSizes;
	MergedSizes.AddZeroed(NumBones);
	for(int32 BoneIdx = NumBones-1; BoneIdx >=0; --BoneIdx)
	{
		const float MyMergedSize = MergedSizes[BoneIdx] += CalcBoneInfoLength(Infos[BoneIdx]);

		if(MyMergedSize < Params.MinBoneSize && MyMergedSize >= Params.MinWeldSize)
		{
			//Too small to make a body for, so let's merge with parent bone. TODO: use a merge threshold
			const int32 ParentIndex = SkelMesh->RefSkeleton.GetParentIndex(BoneIdx);
			if(ParentIndex != INDEX_NONE)
			{
				MergedSizes[ParentIndex] += MyMergedSize;
				FBoneVertInfo& ParentMergedBones = BoneToMergedBones.FindOrAdd(ParentIndex);	//Add this bone to its parent merged bones
				const FTransform LocalTM = LocalPose[BoneIdx];

				AddInfoToParentInfo(LocalTM, Infos[BoneIdx], ParentMergedBones);

				if(FBoneVertInfo* MyMergedBones = BoneToMergedBones.Find(BoneIdx))
				{
					//make sure any bones merged to this bone get merged into the parent
					AddInfoToParentInfo(LocalTM, *MyMergedBones, ParentMergedBones);
					BoneToMergedBones.Remove(BoneIdx);
				}
			}
		}
	}

	//We must ensure that there is a single root body no matter how small.
	int32 ForcedRootBoneIndex = INDEX_NONE;
	int32 FirstParentBoneIndex = INDEX_NONE;
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		if (MergedSizes[BoneIndex] > Params.MinBoneSize)
		{
			const int32 ParentBoneIndex = SkelMesh->RefSkeleton.GetParentIndex(BoneIndex);
			if(ParentBoneIndex == INDEX_NONE)
			{
				break;	//We already have a single root body, so don't worry about it
			}
			else if(FirstParentBoneIndex == INDEX_NONE)
			{
				FirstParentBoneIndex = ParentBoneIndex;	//record first parent to see if we have more than one root
			}
			else if(ParentBoneIndex == FirstParentBoneIndex)
			{
				ForcedRootBoneIndex = ParentBoneIndex;	//we have two "root" bodies so take their parent as the real root body
				break;
			}
		}
	}

	FScopedSlowTask SlowTask((float)NumBones * 2);
	SlowTask.MakeDialog();

	// Finally, iterate through all the bones and create bodies when needed
	for (int32 BoneIndex = 0; BoneIndex < NumBones; BoneIndex++)
	{
		// Determine if we should create a physics body for this bone
		bool bMakeBone = false;

		// If desired - make a body for EVERY bone
		if (Params.bBodyForAll)
		{
			bMakeBone = true;
		}
		else if (MergedSizes[BoneIndex] > Params.MinBoneSize)
		{
			// If bone is big enough - create physics.
			bMakeBone = true;
		}
		else if(BoneIndex == ForcedRootBoneIndex)
		{
			// If the bone is a forced root body we must create they body no matter how small
			bMakeBone = true;
		}

		if (bMakeBone)
		{
			// Go ahead and make this bone physical.
			FName BoneName = SkelMesh->RefSkeleton.GetBoneName(BoneIndex);

			SlowTask.EnterProgressFrame(1.0f, FText::Format(NSLOCTEXT("PhysicsAssetEditor", "ResetCollsionStepInfo", "Generating collision for {0}"), FText::FromName(BoneName)));

			int32 NewBodyIndex = CreateNewBody(PhysicsAsset, BoneName);
			UBodySetup* NewBodySetup = PhysicsAsset->SkeletalBodySetups[NewBodyIndex];
			check(NewBodySetup->BoneName == BoneName);

			//Construct the info - in the case of merged bones we append all the data
			FBoneVertInfo Info = Infos[BoneIndex];
			if(const FBoneVertInfo* MergedBones = BoneToMergedBones.Find(BoneIndex))
			{
				//Don't need to convert into parent space since this was already done
				Info.Normals.Append(MergedBones->Normals);
				Info.Positions.Append(MergedBones->Positions);
			}

			// Fill in collision info for this bone.
			const bool bSuccess = CreateCollisionFromBoneInternal(NewBodySetup, SkelMesh, BoneIndex, Params, Info, TriangleCache);
			if(bSuccess)
			{
				// create joint to parent body
				if (Params.bCreateJoints)
				{
					// Transform of child from parent is just child ref-pose entry.
					FTransform RelTM = FTransform::Identity;

					int32 ParentIndex = BoneIndex;
					int32 ParentBodyIndex = INDEX_NONE;
					FName ParentName;

					do
					{
						// Transform of child from parent is just child ref-pose entry.
						RelTM = RelTM * LocalPose[ParentIndex];

						//Travel up the hierarchy to find a parent which has a valid body
						ParentIndex = SkelMesh->RefSkeleton.GetParentIndex(ParentIndex);
						if(ParentIndex != INDEX_NONE)
						{
							ParentName = SkelMesh->RefSkeleton.GetBoneName(ParentIndex);
							ParentBodyIndex = PhysicsAsset->FindBodyIndex(ParentName);
						}
						else
						{
							//no more parents so just stop
							break;
						}
						
					}while(ParentBodyIndex == INDEX_NONE);

					if (ParentBodyIndex != INDEX_NONE)
					{
						//found valid parent body so create joint
						int32 NewConstraintIndex = CreateNewConstraint(PhysicsAsset, BoneName);
						UPhysicsConstraintTemplate* CS = PhysicsAsset->ConstraintSetup[NewConstraintIndex];
						
						// set angular constraint mode
						CS->DefaultInstance.SetAngularSwing1Motion(Params.AngularConstraintMode);
						CS->DefaultInstance.SetAngularSwing2Motion(Params.AngularConstraintMode);
						CS->DefaultInstance.SetAngularTwistMotion(Params.AngularConstraintMode);

						// Place joint at origin of child
						CS->DefaultInstance.ConstraintBone1 = BoneName;
						CS->DefaultInstance.Pos1 = FVector::ZeroVector;
						CS->DefaultInstance.PriAxis1 = FVector(1, 0, 0);
						CS->DefaultInstance.SecAxis1 = FVector(0, 1, 0);

						CS->DefaultInstance.ConstraintBone2 = ParentName;
						CS->DefaultInstance.Pos2 = RelTM.GetLocation();
						CS->DefaultInstance.PriAxis2 = RelTM.GetUnitAxis(EAxis::X);
						CS->DefaultInstance.SecAxis2 = RelTM.GetUnitAxis(EAxis::Y);

						CS->SetDefaultProfile(CS->DefaultInstance);

						// Disable collision between constrained bodies by default.
						PhysicsAsset->DisableCollision(NewBodyIndex, ParentBodyIndex);
					}
				}
			}
			else
			{
				DestroyBody(PhysicsAsset, NewBodyIndex);
			}
		}
	}

	//Go through and ensure any overlapping bodies are marked as disable collision
	FPreviewScene TmpScene;
	UWorld* TmpWorld = TmpScene.GetWorld();
	ASkeletalMeshActor* SkeletalMeshActor = TmpWorld->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform::Identity);
	SkeletalMeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkelMesh);
	USkeletalMeshComponent* SKC = SkeletalMeshActor->GetSkeletalMeshComponent();
	SKC->SetPhysicsAsset(PhysicsAsset);
	SkeletalMeshActor->RegisterAllComponents();
	

	const TArray<FBodyInstance*> Bodies = SKC->Bodies;
	const int32 NumBodies = Bodies.Num();
	for(int32 BodyIdx = 0; BodyIdx < NumBodies; ++BodyIdx)
	{
		FBodyInstance* BodyInstance = Bodies[BodyIdx];
		if(BodyInstance && BodyInstance->BodySetup.IsValid())
		{
			SlowTask.EnterProgressFrame(1.0f, FText::Format(NSLOCTEXT("PhysicsAssetEditor", "ResetCollsionStepInfoOverlaps", "Fixing overlaps for {0}"), FText::FromName(BodyInstance->BodySetup->BoneName)));

			FTransform BodyTM = BodyInstance->GetUnrealWorldTransform();

			for(int32 OtherBodyIdx = BodyIdx + 1; OtherBodyIdx < NumBodies; ++OtherBodyIdx)
			{
				FBodyInstance* OtherBodyInstance = Bodies[OtherBodyIdx];
				if(OtherBodyInstance && OtherBodyInstance->BodySetup.IsValid())
				{
					if(BodyInstance->OverlapTestForBody(BodyTM.GetLocation(), BodyTM.GetRotation(), OtherBodyInstance))
					{
						PhysicsAsset->DisableCollision(BodyIdx, OtherBodyIdx);
					}
				}
			}
		}
	}

	return NumBodies > 0;
}

bool CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, USkeletalMesh* SkelMesh, const FPhysAssetCreateParams& Params, FText& OutErrorMessage, bool bSetToMesh /*= true*/)
{
	PhysicsAsset->PreviewSkeletalMesh = SkelMesh;

	check(SkelMesh);

	FSkinnedBoneTriangleCache TriangleCache(*SkelMesh, Params);

	if ( Params.GeomType == EFG_MultiConvexHull )
	{
		TriangleCache.BuildCache();
	}

	bool bSuccess = CreateFromSkeletalMeshInternal(PhysicsAsset, SkelMesh, Params, TriangleCache);
	if (!bSuccess)
	{
		// try lower minimum bone size 
		FPhysAssetCreateParams LocalParams = Params;
		LocalParams.MinBoneSize = 1.f;

		bSuccess = CreateFromSkeletalMeshInternal(PhysicsAsset, SkelMesh, LocalParams, TriangleCache);

		if(!bSuccess)
		{
			OutErrorMessage = FText::Format(NSLOCTEXT("CreatePhysicsAsset", "CreatePhysicsAssetLinkFailed", "The bone size is too small to create Physics Asset '{0}' from Skeletal Mesh '{1}'. You will have to create physics asset manually."), FText::FromString(PhysicsAsset->GetName()), FText::FromString(SkelMesh->GetName()));
		}
	}

	if(bSuccess && bSetToMesh)
	{
		SkelMesh->PhysicsAsset = PhysicsAsset;
		SkelMesh->MarkPackageDirty();
	}

	return bSuccess;
}

FMatrix ComputeCovarianceMatrix(const FBoneVertInfo& VertInfo)
{
	if (VertInfo.Positions.Num() == 0)
	{
		return FMatrix::Identity;
	}

	const TArray<FVector> & Positions = VertInfo.Positions;

	//get average
	const float N = Positions.Num();
	FVector U = FVector::ZeroVector;
	for (int32 i = 0; i < N; ++i)
	{
		U += Positions[i];
	}

	U = U / N;

	//compute error terms
	TArray<FVector> Errors;
	Errors.AddUninitialized(N);

	for (int32 i = 0; i < N; ++i)
	{
		Errors[i] = Positions[i] - U;
	}

	FMatrix Covariance = FMatrix::Identity;
	for (int32 j = 0; j < 3; ++j)
	{
		FVector Axis = FVector::ZeroVector;
		float* Cj = &Axis.X;
		for (int32 k = 0; k < 3; ++k)
		{
			float Cjk = 0.f;
			for (int32 i = 0; i < N; ++i)
			{
				const float* error = &Errors[i].X;
				Cj[k] += error[j] * error[k];
			}
			Cj[k] /= N;
		}

		Covariance.SetAxis(j, Axis);
	}

	return Covariance;
}

FVector ComputeEigenVector(const FMatrix& A)
{
	//using the power method: this is ok because we only need the dominate eigenvector and speed is not critical: http://en.wikipedia.org/wiki/Power_iteration
	FVector Bk = FVector(0, 0, 1);
	for (int32 i = 0; i < 32; ++i)
	{
		float Length = Bk.Size();
		if (Length > 0.f)
		{
			Bk = A.TransformVector(Bk) / Length;
		}
	}

	return Bk.GetSafeNormal();
}

bool CreateCollisionFromBoneInternal(UBodySetup* bs, USkeletalMesh* skelMesh, int32 BoneIndex, const FPhysAssetCreateParams& Params, const FBoneVertInfo& Info, const FSkinnedBoneTriangleCache& TriangleCache)
{
#if WITH_EDITOR
	if (Params.GeomType != EFG_MultiConvexHull)	//multi convex hull can fail so wait to clear it
	{
		// Empty any existing collision.
		bs->RemoveSimpleCollision();
	}
#endif // WITH_EDITOR

	// Calculate orientation of to use for collision primitive.
	FMatrix ElemTM = FMatrix::Identity;
	bool ComputeFromVerts = false;

	if (Params.bAutoOrientToBone)
	{
		// Compute covariance matrix for verts of this bone
		// Then use axis with largest variance for orienting bone box
		const FMatrix CovarianceMatrix = ComputeCovarianceMatrix(Info);
		FVector ZAxis = ComputeEigenVector(CovarianceMatrix);
		FVector XAxis, YAxis;
		ZAxis.FindBestAxisVectors(YAxis, XAxis);
		ElemTM = FMatrix(XAxis, YAxis, ZAxis, FVector::ZeroVector);
	}

	// convert to FTransform now
	// Matrix inverse doesn't handle well when DET == 0, so 
	// convert to FTransform and use that data
	FTransform ElementTransform(ElemTM);
	// Get the (Unreal scale) bounding box for this bone using the rotation.
	FBox BoneBox(ForceInit);
	for (int32 j = 0; j < Info.Positions.Num(); j++)
	{
		BoneBox += ElementTransform.InverseTransformPosition(Info.Positions[j]);
	}

	FVector BoxCenter(0, 0, 0), BoxExtent(0, 0, 0);

	FBox TransformedBox = BoneBox;
	if (BoneBox.IsValid)
	{
		// make sure to apply scale to the box size
		FMatrix BoneMatrix = skelMesh->GetComposedRefPoseMatrix(BoneIndex);
		TransformedBox = BoneBox.TransformBy(FTransform(BoneMatrix));
		BoneBox.GetCenterAndExtents(BoxCenter, BoxExtent);
	}

	float MinRad = TransformedBox.GetExtent().GetMin();
	float MinAllowedSize = MinPrimSize;

	// If the primitive is going to be too small - just use some default numbers and let the user tweak.
	if (MinRad < MinAllowedSize)
	{
		// change min allowed size to be min, not DefaultPrimSize
		BoxExtent = FVector(MinAllowedSize, MinAllowedSize, MinAllowedSize);
	}

	FVector BoneOrigin = ElementTransform.TransformPosition(BoxCenter);
	ElementTransform.SetTranslation(BoneOrigin);

	if (Params.GeomType == EFG_Box)
	{
		// Add a new box geometry to this body the size of the bounding box.
		FKBoxElem BoxElem;

		BoxElem.SetTransform(ElementTransform);

		BoxElem.X = BoxExtent.X * 2.0f * 1.01f; // Side Lengths (add 1% to avoid graphics glitches)
		BoxElem.Y = BoxExtent.Y * 2.0f * 1.01f;
		BoxElem.Z = BoxExtent.Z * 2.0f * 1.01f;

		bs->AggGeom.BoxElems.Add(BoxElem);
	}
	else if (Params.GeomType == EFG_Sphere)
	{
		FKSphereElem SphereElem;

		SphereElem.Center = ElementTransform.GetTranslation();
		SphereElem.Radius = BoxExtent.GetMax() * 1.01f;

		bs->AggGeom.SphereElems.Add(SphereElem);
	}
	// Deal with creating a single convex hull
	else if (Params.GeomType == EFG_SingleConvexHull)
	{
		if (Info.Positions.Num())
		{
			FKConvexElem ConvexElem;

			// Add all of the vertices for this bone to the convex element
			for (int32 index = 0; index < Info.Positions.Num(); index++)
			{
				ConvexElem.VertexData.Add(Info.Positions[index]);
			}
			ConvexElem.UpdateElemBox();
			bs->AggGeom.ConvexElems.Add(ConvexElem);
		}
		else
		{
			FMessageLog EditorErrors("EditorErrors");
			EditorErrors.Warning(NSLOCTEXT("PhysicsAssetUtils", "ConvexNoPositions", "Unable to create a convex hull for the given bone as there are no vertices associated with the bone."));
			EditorErrors.Open();
			return false;
		}
	}
	else if (Params.GeomType == EFG_MultiConvexHull)
	{
		TArray<FVector> Verts;
		TArray<uint32> Indices;
		TriangleCache.GetVerticesAndIndicesForBone(BoneIndex, Verts, Indices);

		if (Verts.Num())
		{
			DecomposeMeshToHulls(bs, Verts, Indices, Params.HullAccuracy, Params.MaxHullVerts);
		}
		else
		{
			FMessageLog EditorErrors("EditorErrors");
			EditorErrors.Warning(NSLOCTEXT("PhysicsAssetUtils", "ConvexNoPositions", "Unable to create a convex hull for the given bone as there are no vertices associated with the bone."));
			EditorErrors.Open();
			return false;
		}
	}
	else if (Params.GeomType == EFG_Sphyl)
	{

		FKSphylElem SphylElem;

		if (BoxExtent.X > BoxExtent.Z && BoxExtent.X > BoxExtent.Y)
		{
			//X is the biggest so we must rotate X-axis into Z-axis
			SphylElem.SetTransform(FTransform(FQuat(FVector(0, 1, 0), -PI * 0.5f)) * ElementTransform);
			SphylElem.Radius = FMath::Max(BoxExtent.Y, BoxExtent.Z) * 1.01f;
			SphylElem.Length = BoxExtent.X * 1.01f;

		}
		else if (BoxExtent.Y > BoxExtent.Z && BoxExtent.Y > BoxExtent.X)
		{
			//Y is the biggest so we must rotate Y-axis into Z-axis
			SphylElem.SetTransform(FTransform(FQuat(FVector(1, 0, 0), PI * 0.5f)) * ElementTransform);
			SphylElem.Radius = FMath::Max(BoxExtent.X, BoxExtent.Z) * 1.01f;
			SphylElem.Length = BoxExtent.Y * 1.01f;
		}
		else
		{
			//Z is the biggest so use transform as is
			SphylElem.SetTransform(ElementTransform);

			SphylElem.Radius = FMath::Max(BoxExtent.X, BoxExtent.Y) * 1.01f;
			SphylElem.Length = BoxExtent.Z * 1.01f;
		}



		bs->AggGeom.SphylElems.Add(SphylElem);
	}

	return true;
}

bool CreateCollisionFromBone(UBodySetup* bs, USkeletalMesh* skelMesh, int32 BoneIndex, const FPhysAssetCreateParams& Params, const FBoneVertInfo& Info)
{
	check(skelMesh);

	FSkinnedBoneTriangleCache TriangleCache(*skelMesh, Params);

	if ( Params.GeomType == EFG_MultiConvexHull )
	{
		TriangleCache.BuildCache();
	}

	return CreateCollisionFromBoneInternal(bs, skelMesh, BoneIndex, Params, Info, TriangleCache);
}

bool CreateCollisionFromBones(UBodySetup* bs, USkeletalMesh* skelMesh, const TArray<int32>& BoneIndices, const FPhysAssetCreateParams& Params, const FBoneVertInfo& Info)
{
	check(skelMesh);

	FSkinnedBoneTriangleCache TriangleCache(*skelMesh, Params);

	if (Params.GeomType == EFG_MultiConvexHull)
	{
		TriangleCache.BuildCache();
	}

	bool bAllSuccessful = true;
	for ( int Index = 0; Index < BoneIndices.Num(); ++Index )
	{
		if ( !CreateCollisionFromBoneInternal(bs, skelMesh, BoneIndices[Index], Params, Info, TriangleCache) )
		{
			bAllSuccessful = false;
		}
	}

	return bAllSuccessful;
}

void WeldBodies(UPhysicsAsset* PhysAsset, int32 BaseBodyIndex, int32 AddBodyIndex, USkeletalMeshComponent* SkelComp)
{
	if(BaseBodyIndex == INDEX_NONE || AddBodyIndex == INDEX_NONE)
		return;

	if (SkelComp == NULL || SkelComp->SkeletalMesh == NULL)
	{
		return;
	}

	UBodySetup* Body1 = PhysAsset->SkeletalBodySetups[BaseBodyIndex];
	int32 Bone1Index = SkelComp->SkeletalMesh->RefSkeleton.FindBoneIndex(Body1->BoneName);
	check(Bone1Index != INDEX_NONE);
	FTransform Bone1TM = SkelComp->GetBoneTransform(Bone1Index);
	Bone1TM.RemoveScaling();

	UBodySetup* Body2 = PhysAsset->SkeletalBodySetups[AddBodyIndex];
	int32 Bone2Index = SkelComp->SkeletalMesh->RefSkeleton.FindBoneIndex(Body2->BoneName);
	check(Bone2Index != INDEX_NONE);
	FTransform Bone2TM = SkelComp->GetBoneTransform(Bone2Index);
	Bone2TM.RemoveScaling();

	FTransform Bone2ToBone1TM = Bone2TM.GetRelativeTransform(Bone1TM);

	// First copy all collision info over.
	for(int32 i=0; i<Body2->AggGeom.SphereElems.Num(); i++)
	{
		int32 NewPrimIndex = Body1->AggGeom.SphereElems.Add( Body2->AggGeom.SphereElems[i] );
		Body1->AggGeom.SphereElems[NewPrimIndex].Center = Bone2ToBone1TM.TransformPosition( Body2->AggGeom.SphereElems[i].Center ); // Make transform relative to body 1 instead of body 2
	}

	for(int32 i=0; i<Body2->AggGeom.BoxElems.Num(); i++)
	{
		int32 NewPrimIndex = Body1->AggGeom.BoxElems.Add( Body2->AggGeom.BoxElems[i] );
		Body1->AggGeom.BoxElems[NewPrimIndex].SetTransform( Body2->AggGeom.BoxElems[i].GetTransform() * Bone2ToBone1TM );
	}

	for(int32 i=0; i<Body2->AggGeom.SphylElems.Num(); i++)
	{
		int32 NewPrimIndex = Body1->AggGeom.SphylElems.Add( Body2->AggGeom.SphylElems[i] );
		Body1->AggGeom.SphylElems[NewPrimIndex].SetTransform( Body2->AggGeom.SphylElems[i].GetTransform() * Bone2ToBone1TM );
	}

	for(int32 i=0; i<Body2->AggGeom.ConvexElems.Num(); i++)
	{
		FKConvexElem& Elem2 = Body2->AggGeom.ConvexElems[i];
		FTransform Elem2TM = Elem2.GetTransform() * Bone2TM;
		FTransform Elem2ToBone1TM = Elem2TM.GetRelativeTransform(Bone1TM);

		// No transform on new element - we transform all the vertices into the new ref frame instead.
		int32 NewPrimIndex = Body1->AggGeom.ConvexElems.Add( Body2->AggGeom.ConvexElems[i] );
		FKConvexElem* cElem= &Body1->AggGeom.ConvexElems[NewPrimIndex];

		for(int32 j=0; j<cElem->VertexData.Num(); j++)
		{
			cElem->VertexData[j] = Elem2ToBone1TM.TransformPosition( cElem->VertexData[j] );
		}

		// Update face data.
		cElem->UpdateElemBox();
	}

	// After changing collision, need to recreate meshes
	Body1->InvalidatePhysicsData();
	Body1->CreatePhysicsMeshes();

	// We need to update the collision disable table to shift any pairs that included body2 to include body1 instead.
	// We remove any pairs that include body2 & body1.

	for(int32 i=0; i<PhysAsset->SkeletalBodySetups.Num(); i++)
	{
		if(i == AddBodyIndex) 
			continue;

		FRigidBodyIndexPair Key(i, AddBodyIndex);

		if( PhysAsset->CollisionDisableTable.Find(Key) )
		{
			PhysAsset->CollisionDisableTable.Remove(Key);

			// Only re-add pair if its not between 'base' and 'add' bodies.
			if(i != BaseBodyIndex)
			{
				FRigidBodyIndexPair NewKey(i, BaseBodyIndex);
				PhysAsset->CollisionDisableTable.Add(NewKey, 0);
			}
		}
	}

	// Make a sensible guess for the other flags
	ECollisionEnabled::Type NewCollisionEnabled = FMath::Min(Body1->DefaultInstance.GetCollisionEnabled(), Body2->DefaultInstance.GetCollisionEnabled());
	Body1->DefaultInstance.SetCollisionEnabled(NewCollisionEnabled);

	// if different
	if (Body1->PhysicsType != Body2->PhysicsType)
	{
		// i don't think this is necessarily good, but I think better than default
		Body1->PhysicsType = FMath::Max(Body1->PhysicsType, Body2->PhysicsType);
	}

	// Then deal with any constraints.

	TArray<int32>	Body2Constraints;
	PhysAsset->BodyFindConstraints(AddBodyIndex, Body2Constraints);

	while( Body2Constraints.Num() > 0 )
	{
		int32 ConstraintIndex = Body2Constraints[0];
		FConstraintInstance& Instance = PhysAsset->ConstraintSetup[ConstraintIndex]->DefaultInstance;

		FName OtherBodyName;
		if( Instance.ConstraintBone1 == Body2->BoneName )
			OtherBodyName = Instance.ConstraintBone2;
		else
			OtherBodyName = Instance.ConstraintBone1;

		// If this is a constraint between the two bodies we are welding, we just destroy it.
		if(OtherBodyName == Body1->BoneName)
		{
			DestroyConstraint(PhysAsset, ConstraintIndex);
		}
		else // Otherwise, we reconnect it to body1 (the 'base' body) instead of body2 (the 'weldee').
		{
			if(Instance.ConstraintBone2 == Body2->BoneName)
			{
				Instance.ConstraintBone2 = Body1->BoneName;

				FTransform ConFrame = Instance.GetRefFrame(EConstraintFrame::Frame2);
				Instance.SetRefFrame(EConstraintFrame::Frame2, ConFrame * FTransform(Bone2ToBone1TM));
			}
			else
			{
				Instance.ConstraintBone1 = Body1->BoneName;

				FTransform ConFrame = Instance.GetRefFrame(EConstraintFrame::Frame1);
				Instance.SetRefFrame(EConstraintFrame::Frame1, ConFrame * FTransform(Bone2ToBone1TM));
			}
		}

		// See if we have any more constraints to body2.
		PhysAsset->BodyFindConstraints(AddBodyIndex, Body2Constraints);
	}

	// Finally remove the body
	DestroyBody(PhysAsset, AddBodyIndex);
}

int32 CreateNewConstraint(UPhysicsAsset* PhysAsset, FName InConstraintName, UPhysicsConstraintTemplate* InConstraintSetup)
{
	// constraintClass must be a subclass of UPhysicsConstraintTemplate
	int32 ConstraintIndex = PhysAsset->FindConstraintIndex(InConstraintName);
	if(ConstraintIndex != INDEX_NONE)
	{
		return ConstraintIndex;
	}

	UPhysicsConstraintTemplate* NewConstraintSetup = NewObject<UPhysicsConstraintTemplate>(PhysAsset, NAME_None, RF_Transactional);
	if(InConstraintSetup)
	{
		NewConstraintSetup->DefaultInstance.CopyConstraintParamsFrom( &InConstraintSetup->DefaultInstance );
	}

	int32 ConstraintSetupIndex = PhysAsset->ConstraintSetup.Add( NewConstraintSetup );
	NewConstraintSetup->DefaultInstance.JointName = InConstraintName;

	return ConstraintSetupIndex;
}

void DestroyConstraint(UPhysicsAsset* PhysAsset, int32 ConstraintIndex)
{
	check(PhysAsset);
	PhysAsset->ConstraintSetup.RemoveAt(ConstraintIndex);
}


int32 CreateNewBody(UPhysicsAsset* PhysAsset, FName InBodyName)
{
	check(PhysAsset);

	int32 BodyIndex = PhysAsset->FindBodyIndex(InBodyName);
	if(BodyIndex != INDEX_NONE)
	{
		return BodyIndex; // if we already have one for this name - just return that.
	}

	USkeletalBodySetup* NewBodySetup = NewObject<USkeletalBodySetup>(PhysAsset, NAME_None, RF_Transactional);
	// make default to be use complex as simple 
	NewBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	// newly created bodies default to simulating
	NewBodySetup->PhysicsType = PhysType_Default;

	int32 BodySetupIndex = PhysAsset->SkeletalBodySetups.Add( NewBodySetup );
	NewBodySetup->BoneName = InBodyName;

	PhysAsset->UpdateBodySetupIndexMap();
	PhysAsset->UpdateBoundsBodiesArray();

	// Return index of new body.
	return BodySetupIndex;
}

void DestroyBody(UPhysicsAsset* PhysAsset, int32 bodyIndex)
{
	check(PhysAsset);

	// First we must correct the CollisionDisableTable.
	// All elements which refer to bodyIndex are removed.
	// All elements which refer to a body with index >bodyIndex are adjusted. 

	TMap<FRigidBodyIndexPair,bool> NewCDT;
	for(int32 i=1; i<PhysAsset->SkeletalBodySetups.Num(); i++)
	{
		for(int32 j=0; j<i; j++)
		{
			FRigidBodyIndexPair Key(j,i);

			// If there was an entry for this pair, and it doesn't refer to the removed body, we need to add it to the new CDT.
			if( PhysAsset->CollisionDisableTable.Find(Key) )
			{
				if(i != bodyIndex && j != bodyIndex)
				{
					int32 NewI = (i > bodyIndex) ? i-1 : i;
					int32 NewJ = (j > bodyIndex) ? j-1 : j;

					FRigidBodyIndexPair NewKey(NewJ, NewI);
					NewCDT.Add(NewKey, 0);
				}
			}
		}
	}

	PhysAsset->CollisionDisableTable = NewCDT;

	// Now remove any constraints that were attached to this body.
	// This is a bit yuck and slow...
	TArray<int32> Constraints;
	PhysAsset->BodyFindConstraints(bodyIndex, Constraints);

	while(Constraints.Num() > 0)
	{
		DestroyConstraint( PhysAsset, Constraints[0] );
		PhysAsset->BodyFindConstraints(bodyIndex, Constraints);
	}

	// Remove pointer from array. Actual objects will be garbage collected.
	PhysAsset->SkeletalBodySetups.RemoveAt(bodyIndex);

	PhysAsset->UpdateBodySetupIndexMap();
	// Update body indices.
	PhysAsset->UpdateBoundsBodiesArray();
}

}; // namespace FPhysicsAssetUtils
