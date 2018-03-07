// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ClothingSimulation.h"

#include "PhysicsEngine/PhysicsSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "ClothingSimulationInterface.h"
#include "ClothingSystemRuntimeModule.h"
#include "ClothingAsset.h"


DECLARE_CYCLE_STAT(TEXT("Skin Physics Mesh"), STAT_ClothSkinPhysMesh, STATGROUP_Physics);

FClothingSimulationBase::FClothingSimulationBase()
{
	MaxPhysicsDelta = UPhysicsSettings::Get()->MaxPhysicsDeltaTime;
}

void FClothingSimulationBase::SkinPhysicsMesh(UClothingAsset* InAsset, const FClothPhysicalMeshData& InMesh, const FTransform& RootBoneTransform, const FMatrix* InBoneMatrices, const int32 InNumBoneMatrices, TArray<FVector>& OutPositions, TArray<FVector>& OutNormals)
{
	SCOPE_CYCLE_COUNTER(STAT_ClothSkinPhysMesh);

	// Ignore any user scale. It's already accounted for in our skinning matrices
	FTransform RootBoneTransformInternal = RootBoneTransform;
	RootBoneTransformInternal.SetScale3D(FVector(1.0f));

	const uint32 NumVerts = InMesh.Vertices.Num();

	OutPositions.Reset(NumVerts);
	OutNormals.Reset(NumVerts);
	OutPositions.AddZeroed(NumVerts);
	OutNormals.AddZeroed(NumVerts);

	const int32 MaxInfluences = InMesh.MaxBoneWeights;
	const FMatrix* RESTRICT BoneMatrices = InBoneMatrices;
	const TArray<int32>& BoneMap = InAsset->UsedBoneIndices;

	for(uint32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
	{
		// Fixed particle, needs to be skinned
		const uint16* RESTRICT BoneIndices = InMesh.BoneData[VertIndex].BoneIndices;
		const float* RESTRICT BoneWeights = InMesh.BoneData[VertIndex].BoneWeights;

		// WARNING - HORRIBLE UNROLLED LOOP + JUMP TABLE BELOW
		// done this way because this is a pretty tight and perf critical loop. essentially
		// rather than checking each influence we can just jump into this switch and fall through
		// everything to compose the final skinned data
		const FVector& RefParticle = InMesh.Vertices[VertIndex];
		const FVector& RefNormal = InMesh.Normals[VertIndex];
		FVector& OutPosition = OutPositions[VertIndex];
		FVector& OutNormal = OutNormals[VertIndex];
		switch(InMesh.BoneData[VertIndex].NumInfluences)
		{
			default: break;
			case 8:
			{
				const FMatrix& BoneMatrix = BoneMatrices[BoneMap[BoneIndices[7]]];
				const float Weight = BoneWeights[7];

				OutPosition += BoneMatrix.TransformPosition(RefParticle) * Weight;
				OutNormal += BoneMatrix.TransformVector(RefNormal) * Weight;
			}
			case 7:
			{
				const FMatrix& BoneMatrix = BoneMatrices[BoneMap[BoneIndices[6]]];
				const float Weight = BoneWeights[6];

				OutPosition += BoneMatrix.TransformPosition(RefParticle) * Weight;
				OutNormal += BoneMatrix.TransformVector(RefNormal) * Weight;
			}
			case 6:
			{
				const FMatrix& BoneMatrix = BoneMatrices[BoneMap[BoneIndices[5]]];
				const float Weight = BoneWeights[5];

				OutPosition += BoneMatrix.TransformPosition(RefParticle) * Weight;
				OutNormal += BoneMatrix.TransformVector(RefNormal) * Weight;
			}
			case 5:
			{
				const FMatrix& BoneMatrix = BoneMatrices[BoneMap[BoneIndices[4]]];
				const float Weight = BoneWeights[4];

				OutPosition += BoneMatrix.TransformPosition(RefParticle) * Weight;
				OutNormal += BoneMatrix.TransformVector(RefNormal) * Weight;
			}
			case 4:
			{
				const FMatrix& BoneMatrix = BoneMatrices[BoneMap[BoneIndices[3]]];
				const float Weight = BoneWeights[3];

				OutPosition += BoneMatrix.TransformPosition(RefParticle) * Weight;
				OutNormal += BoneMatrix.TransformVector(RefNormal) * Weight;
			}
			case 3:
			{
				const FMatrix& BoneMatrix = BoneMatrices[BoneMap[BoneIndices[2]]];
				const float Weight = BoneWeights[2];

				OutPosition += BoneMatrix.TransformPosition(RefParticle) * Weight;
				OutNormal += BoneMatrix.TransformVector(RefNormal) * Weight;
			}
			case 2:
			{
				const FMatrix& BoneMatrix = BoneMatrices[BoneMap[BoneIndices[1]]];
				const float Weight = BoneWeights[1];

				OutPosition += BoneMatrix.TransformPosition(RefParticle) * Weight;
				OutNormal += BoneMatrix.TransformVector(RefNormal) * Weight;
			}
			case 1:
			{
				const FMatrix& BoneMatrix = BoneMatrices[BoneMap[BoneIndices[0]]];
				const float Weight = BoneWeights[0];

				OutPosition += BoneMatrix.TransformPosition(RefParticle) * Weight;
				OutNormal += BoneMatrix.TransformVector(RefNormal) * Weight;
			}
		}

		OutPosition = RootBoneTransformInternal.InverseTransformPosition(OutPosition);
		OutNormal = RootBoneTransformInternal.InverseTransformVector(OutNormal);
		OutNormal = OutNormal.GetUnsafeNormal();
	}
}

void FClothingSimulationBase::FillContext(USkeletalMeshComponent* InComponent, float InDeltaTime, IClothingSimulationContext* InOutContext)
{
	FClothingSimulationContextBase* BaseContext = static_cast<FClothingSimulationContextBase*>(InOutContext);
	BaseContext->ComponentToWorld = InComponent->GetComponentTransform();
	BaseContext->PredictedLod = InComponent->PredictedLODLevel;
	InComponent->GetWindForCloth_GameThread(BaseContext->WindVelocity, BaseContext->WindAdaption);
	USkeletalMesh* SkelMesh = InComponent->SkeletalMesh;

	if(USkinnedMeshComponent* MasterComponent = InComponent->MasterPoseComponent.Get())
	{
		int32 NumBones = InComponent->MasterBoneMap.Num();

		if(NumBones == 0)
		{
			if(InComponent->SkeletalMesh)
			{
				// This case indicates an invalid master pose component (e.g. no skeletal mesh)
				NumBones = InComponent->SkeletalMesh->RefSkeleton.GetNum();

				BaseContext->BoneTransforms.Empty(NumBones);
				BaseContext->BoneTransforms.AddDefaulted(NumBones);
			}
		}
		else
		{
		    BaseContext->BoneTransforms.Reset(NumBones);
		    BaseContext->BoneTransforms.AddDefaulted(NumBones);
    
		    for(int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		    {
			    bool bFoundMaster = false;
			    if(InComponent->MasterBoneMap.IsValidIndex(BoneIndex))
			    {
				    const int32 MasterIndex = InComponent->MasterBoneMap[BoneIndex];
    
				    if(MasterIndex != INDEX_NONE)
				    {
					    BaseContext->BoneTransforms[BoneIndex] = MasterComponent->GetComponentSpaceTransforms()[MasterIndex];
					    bFoundMaster = true;
				    }
			    }
    
			    if(!bFoundMaster)
			    {
				    if(SkelMesh)
				    {
					    const int32 ParentIndex = SkelMesh->RefSkeleton.GetParentIndex(BoneIndex);
    
					    if(ParentIndex != INDEX_NONE)
					    {
						    BaseContext->BoneTransforms[BoneIndex] = BaseContext->BoneTransforms[ParentIndex] * SkelMesh->RefSkeleton.GetRefBonePose()[BoneIndex];
					    }
					    else
					    {
						    BaseContext->BoneTransforms[BoneIndex] = SkelMesh->RefSkeleton.GetRefBonePose()[BoneIndex];
					    }
				    }
			    }
		    }
		}
	}
	else
	{
		BaseContext->BoneTransforms = InComponent->GetComponentSpaceTransforms();
	}

	UWorld* ComponentWorld = InComponent->GetWorld();
	check(ComponentWorld);

	BaseContext->DeltaSeconds = FMath::Min(InDeltaTime, MaxPhysicsDelta);

	BaseContext->TeleportMode = InComponent->ClothTeleportMode;

	BaseContext->MaxDistanceScale = InComponent->GetClothMaxDistanceScale();
}
