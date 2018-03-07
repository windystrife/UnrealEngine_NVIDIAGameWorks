// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/FlexComponent.h"

#include "PhysXSupport.h"
#include "StaticMeshResources.h"

#include "PhysicsPublic.h"

#if WITH_FLEX

#include "FlexContainerInstance.h"
#include "FlexRender.h"

#if STATS

DECLARE_CYCLE_STAT(TEXT("Update Bounds (CPU)"), STAT_Flex_UpdateBoundsCpu, STATGROUP_Flex);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Active Mesh Particle Count"), STAT_Flex_ActiveParticleCount, STATGROUP_Flex);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Active Mesh Actor Count"), STAT_Flex_ActiveMeshActorCount, STATGROUP_Flex);

#endif


UFlexComponent::UFlexComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	// tick driven through container
	PrimaryComponentTick.bCanEverTick = false;

	OverrideAsset = false;
	AttachToRigids = false;
	ContainerTemplate = NULL;
	Mass = 1.0f;
	Mobility = EComponentMobility::Movable;
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetSimulatePhysics(false);
	SetViewOwnerDepthPriorityGroup(true, SDPG_World);

	AssetInstance = NULL;
	ContainerInstance = NULL;

	InflatablePressureMultiplier = 1.0f;

	TearingMaxStrainMultiplier = 1.0f;
	TearingBreakCount = 0;
	TearingAsset = NULL;

	MovingFrame = NULL;
}

void UFlexComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	if (GIsEditor && !GIsPlayInEditorWorld)
	{
		//this is executed on actor conversion and restores the collision and simulation settings
		SetSimulatePhysics(false);
		Mobility = EComponentMobility::Movable;
	}
#endif

#if WITH_FLEX

	if (GEngine && GetStaticMesh() && GetStaticMesh()->FlexAsset)
	{
		// use the actor's settings instead of the defaults from the asset
		if (!OverrideAsset)
		{
			ContainerTemplate = GetStaticMesh()->FlexAsset->ContainerTemplate;
			Phase = GetStaticMesh()->FlexAsset->Phase;
			Mass = GetStaticMesh()->FlexAsset->Mass;
			AttachToRigids = GetStaticMesh()->FlexAsset->AttachToRigids;
		}

		const int NumParticles = GetStaticMesh()->FlexAsset->Particles.Num();

		SimPositions.SetNum(NumParticles);
		SimNormals.SetNum(NumParticles);

		UpdateSimPositions();

		// request attach with the FlexContainer
		if (ContainerTemplate && (!GIsEditor  || GIsPlayInEditorWorld) && !AssetInstance)
		{
			FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

			if (PhysScene)
			{
				FFlexContainerInstance* Container = PhysScene->GetFlexContainer(ContainerTemplate);
				if (Container)
				{
					ContainerInstance = Container;
					ContainerInstance->Register(this);
				}
			}

			// ensure valid initial bounds for LOD
			UpdateBounds();
		}
	}

	// initialize moving frame for local space simulation
	MovingFrame = new NvFlexExtMovingFrame();
	
	USceneComponent* Parent = GetAttachParent();
	if (bLocalSpace && Parent)
	{
		//update frame
		const FTransform ParentTransform = Parent->GetComponentTransform();
		const FVector Translation = ParentTransform.GetTranslation();
		const FQuat Rotation = ParentTransform.GetRotation();

		NvFlexExtMovingFrameInit(MovingFrame, (float*)(&Translation.X), (float*)(&Rotation.X));
	}

#endif // WITH_FLEX
}

void UFlexComponent::OnUnregister()
{
	Super::OnUnregister();

#if WITH_FLEX

	if (ContainerInstance && AssetInstance)
	{
		DEC_DWORD_STAT_BY(STAT_Flex_ActiveParticleCount, AssetInstance->numParticles);
		DEC_DWORD_STAT(STAT_Flex_ActiveMeshActorCount);

		ContainerInstance->DestroyInstance(AssetInstance);
		AssetInstance = NULL;
	}

	if (ContainerInstance)
	{
		ContainerInstance->Unregister(this);
		ContainerInstance = NULL;
	}

	delete MovingFrame;

#endif // WITH_FLEX

}

void UFlexComponent::ApplyLocalSpace()
{
	USceneComponent* Parent = GetAttachParent();
	if (bLocalSpace && Parent && ContainerInstance)
	{
		FTransform ParentTransform = Parent->GetComponentTransform();
		FVector Translation = ParentTransform.GetTranslation();
		FQuat Rotation = ParentTransform.GetRotation();
		
		NvFlexExtMovingFrameUpdate(MovingFrame, (float*)(&Translation.X), (float*)(&Rotation.X), ContainerInstance->AverageDeltaTime);

		if (AssetInstance)
		{
			const FVector4* RESTRICT Particles = ContainerInstance->Particles;
			const FVector4* RESTRICT Normals = ContainerInstance->Normals;
			const int* RESTRICT Indices = AssetInstance->particleIndices;

			for (int ParticleIndex=0; ParticleIndex < AssetInstance->numParticles; ++ParticleIndex)
			{
				FVector4* Positions = (FVector4*)&ContainerInstance->Particles[Indices[ParticleIndex]];
				FVector* Velocities = (FVector*)&ContainerInstance->Velocities[Indices[ParticleIndex]];

				NvFlexExtMovingFrameApply(MovingFrame, (float*)Positions, (float*)Velocities,
					1, InertialScale.LinearInertialScale, InertialScale.AngularInertialScale, ContainerInstance->AverageDeltaTime);
			}
		}
	}
}

// called during the sychronous portion of the FlexContainer update
// i.e.: at this point there is no GPU work outstanding, so we may 
// modify particles freely, create instances, etc
void UFlexComponent::Synchronize()
{
#if WITH_FLEX
	if (!IsRegistered())
		return;

	ApplyLocalSpace();

	SynchronizeAttachments();

	if (ContainerInstance)
	{
		// if sim is enabled, then read back latest position and normal data for rendering
		const int* RESTRICT Indices = NULL;

		if (AssetInstance)
		{
			Indices = AssetInstance->particleIndices;
		}

		const FVector4* RESTRICT SrcParticles = ContainerInstance->Particles;
		const FVector4* RESTRICT SrcNormals = ContainerInstance->Normals;

		FBox WorldBounds(ForceInit);

		bool bFlexSolid = GetStaticMesh() && (GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetSolid::StaticClass());
		bool bFlexCloth = GetStaticMesh() && (GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetCloth::StaticClass());
		bool bFlexSoft = GetStaticMesh() && (GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetSoft::StaticClass());

		if (bFlexCloth)
		{
			const UFlexAssetCloth* ClothAsset = Cast<UFlexAssetCloth>(GetStaticMesh()->FlexAsset);

			if (ClothAsset->TearingEnabled && TearingAsset && AssetInstance)
			{
				// update tearing asset inflatable over pressure
				TearingAsset->inflatablePressure = ClothAsset->EnableInflatable?ClothAsset->OverPressure*InflatablePressureMultiplier:0.0f;

				// being tearing code
				const int FreeParticles = ContainerInstance->GetMaxParticleCount() - ContainerInstance->GetActiveParticleCount();

				const int MaxCopies = FreeParticles;
				const int MaxEdits = 1024;

				NvFlexExtTearingParticleClone* Copies = new NvFlexExtTearingParticleClone[MaxCopies];
				int NumCopies;

				NvFlexExtTearingMeshEdit* Edits = new NvFlexExtTearingMeshEdit[MaxEdits];
				int NumEdits;

				check(TearingAsset->numParticles == AssetInstance->numParticles);

				// update tearing asset memory from simulation
				// todo: this shouldn't be necessary if we just pass the vertex data straight into tear mesh
				for (int i=0; i < TearingAsset->numParticles; ++i)
				{
					if (Indices != NULL)
					{
						int ParticleIndex = Indices[i];

						check(ParticleIndex <= ContainerInstance->GetMaxParticleCount());

						((FVector4*)(TearingAsset->particles))[i] = SrcParticles[ParticleIndex];						
					}
				}

				const float MaxStrain = ClothAsset->TearingMaxStrain*TearingMaxStrainMultiplier;

				NvFlexExtTearClothMesh(TearingAsset, MaxStrain, ClothAsset->TearingMaxBreakRate, &Copies[0], &NumCopies, MaxCopies, &Edits[0], &NumEdits, MaxEdits);

				if (NumCopies)
				{
					check(NumEdits <= MaxEdits);
					check(NumCopies <= MaxCopies);

					// allocate new particles in the container
					const int Created = NvFlexExtAllocParticles(ContainerInstance->Container, NumCopies, AssetInstance->particleIndices + AssetInstance->numParticles);
				
					check(TearingAsset->numParticles <= TearingAsset->maxParticles);
					check(AssetInstance->numParticles + NumCopies <= TearingAsset->maxParticles);
					check(Created == NumCopies);
				
					NvFlexExtNotifyAssetChanged(ContainerInstance->Container, TearingAsset);

					// create new particles
					for (int i=0; i < NumCopies; ++i)
					{
						// perform copies on particle data
						ContainerInstance->CopyParticle(AssetInstance->particleIndices[Copies[i].srcIndex], AssetInstance->particleIndices[Copies[i].destIndex]);

						AssetInstance->numParticles++;
					}

					check(AssetInstance->numParticles == TearingAsset->numParticles);
				
					const float NewAlpha = ClothAsset->TearingVertexAlpha;

					// apply edits to the mesh
					ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
						FSendFlexClothMeshEdits,
						FFlexMeshSceneProxy*, SceneProxy, (FFlexMeshSceneProxy*)SceneProxy,
						NvFlexExtTearingMeshEdit*, Edits, Edits, 
						int, NumEdits, NumEdits,
						float, NewAlpha, NewAlpha,
						{
							SceneProxy->UpdateClothMesh(Edits, NumEdits, NewAlpha);
						});

					// fire Blueprint event
					OnTear();
				}
				else
				{
					delete[] Edits;
				}

				delete[] Copies;
			}
		}

		if (AssetInstance)
		{
			const int NumParticles = AssetInstance->numParticles;

			SimPositions.SetNum(NumParticles);
			SimNormals.SetNum(NumParticles);

			for (int i=0; i < NumParticles; ++i)
			{
				if (Indices != NULL)
				{
					int ParticleIndex = Indices[i];

					SimPositions[i] = SrcParticles[ParticleIndex];
					SimNormals[i] = SrcNormals[ParticleIndex];
				}

				if (bFlexCloth || bFlexSoft)
				{
					WorldBounds += FVector(SimPositions[i]);
				}
			}
		}

		if (AssetInstance && bFlexSolid)
		{
			const int ShapeIndex = AssetInstance->shapeIndex;

			if (ShapeIndex != -1)
			{
				const FQuat Rotation = *(FQuat*)AssetInstance->shapeRotations;
				const FVector Translation = *(FVector*)AssetInstance->shapeTranslations;

				const FTransform NewTransform(Rotation, Translation);

				// offset to handle case where object's pivot is not aligned with the object center of mass
                const FVector Offset = GetComponentTransform().TransformVector(FVector(AssetInstance->asset->shapeCenters[0], AssetInstance->asset->shapeCenters[1], AssetInstance->asset->shapeCenters[2]));
				const FVector MoveBy = NewTransform.GetLocation() - GetComponentTransform().GetLocation() - Offset;
				const FRotator NewRotation = NewTransform.Rotator();

				EMoveComponentFlags MoveFlags = IsCollisionEnabled() ? MOVECOMP_NoFlags : MOVECOMP_SkipPhysicsMove;
				MoveComponent(MoveBy, NewRotation, false, NULL, MoveFlags);
			}

			UpdateComponentToWorld();
		}
		else if (bFlexCloth || bFlexSoft)
		{
			if (AssetInstance && IsCollisionEnabled())
			{
				// move collision shapes according center of Bounds
				const FVector MoveBy = Bounds.Origin - GetComponentTransform().GetLocation();
				MoveComponent(MoveBy, FRotator::ZeroRotator, false, NULL, MOVECOMP_NoFlags);
				UpdateComponentToWorld();
			}

			LocalBounds = FBoxSphereBounds(WorldBounds).TransformBy(GetComponentTransform().Inverse());

			// Clamp bounds in case of instability
			const float MaxRadius = 1000000.0f;

			if (LocalBounds.SphereRadius > MaxRadius)
			{
				LocalBounds = FBoxSphereBounds(EForceInit::ForceInitToZero);
			}
		}

		
		// update render transform
		MarkRenderTransformDirty();

		// update render thread data
		MarkRenderDynamicDataDirty();
	}

	EnableSim();

	/*
	// check LOD conditions and enable/disable simulation	
	if (SceneProxy)
	{
		FVector PlayerLocation;
		if (GetPlayerPosition(PlayerLocation))
		{
			const float DistanceBeforeSleep = GSystemSettings.FlexDistanceBeforeSleep;
			if (FVector(PlayerLocation - Bounds.Origin).Size() - Bounds.SphereRadius > DistanceBeforeSleep && DistanceBeforeSleep > 0.0f)
				DisableSim();
			else		
			{
				const int LastRenderedFrame = SceneProxy->GetLastVisibleFrame();
				const int NumFramesBeforeSleep = GSystemSettings.FlexInvisibleFramesBeforeSleep;

				// if not rendered in the last N frames then remove this actor's
				// particles and constraints from the simulation
				if(int(GFrameNumber - LastRenderedFrame) > NumFramesBeforeSleep && NumFramesBeforeSleep > 0)
					DisableSim();
				else
					EnableSim();
			}
		}
	}
	*/
#endif // WITH_FLEX
}

void UFlexComponent::UpdateSceneProxy(FFlexMeshSceneProxy* Proxy)
{	
	// The proxy can only be a FFlexMeshSceneProxy if the Component belongs to a 'non editor' world.
	check(!IsInEditorWorld());
	
	if (Proxy && GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetSoft::StaticClass())
	{
		// copy transforms to render thread
		const int NumShapes = GetStaticMesh()->FlexAsset->ShapeCenters.Num();
		
		FFlexShapeTransform* NewTransforms = new FFlexShapeTransform[NumShapes];

		if (AssetInstance)
		{
			PreSimShapeTranslations.SetNum(NumShapes);
			PreSimShapeRotations.SetNum(NumShapes);

			// set transforms based on the simulation object	
			for (int i=0; i < NumShapes; ++i)
			{
				NewTransforms[i].Translation = *(FVector*)&AssetInstance->shapeTranslations[i*3];
				NewTransforms[i].Rotation = *(FQuat*)&AssetInstance->shapeRotations[i*4];

				PreSimShapeTranslations[i] = NewTransforms[i].Translation;
				PreSimShapeRotations[i] = NewTransforms[i].Rotation;
			}
		}
		else
		{
			if (PreSimPositions.Num() && PreSimShapeTranslations.Num() && PreSimShapeRotations.Num())
			{
				for (int i = 0; i < NumShapes; ++i)
				{
					NewTransforms[i].Translation = PreSimShapeTranslations[i];
					NewTransforms[i].Rotation = PreSimShapeRotations[i];
				}
			}
			else
			{
				// if the simulation object isn't valid yet then set transforms 
				// based on the component transform and asset rest poses
				for (int i = 0; i < NumShapes; ++i)
				{
					NewTransforms[i].Translation = GetComponentTransform().TransformPosition(GetStaticMesh()->FlexAsset->ShapeCenters[i]);
					NewTransforms[i].Rotation = GetComponentTransform().GetRotation();
				}
			}

			// todo: apply presimulation transforms here
		}

		if (AssetInstance)
		{
			// Enqueue command to send to render thread
			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				FSendFlexShapeTransforms,
				FFlexMeshSceneProxy*, Proxy, Proxy,
				FFlexShapeTransform*, ShapeTransforms, NewTransforms,			
				int32, NumShapes, NumShapes,
				{
					Proxy->UpdateSoftTransforms(ShapeTransforms, NumShapes);
					delete [] ShapeTransforms;
				});
		}
		else
		{
			delete [] NewTransforms;
		}
	}

	// cloth
	if (Proxy && GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetCloth::StaticClass())
	{
		if (AssetInstance)
		{
			// Enqueue command to send to render thread
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				FSendFlexClothTransforms,
				FFlexMeshSceneProxy*, Proxy, Proxy,
				{
					Proxy->UpdateClothTransforms();
				});
		}
	}
}

void UFlexComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();

	// Can only downcast to FFlexMeshSceneProxy if we know it was not created in the editor. If it was created in the editor 
	// the SceneProxy is NOT derived from FFlexMeshSceneProxy.
	if (SceneProxy && !IsInEditorWorld())
		UpdateSceneProxy((FFlexMeshSceneProxy*)SceneProxy);
}

void UFlexComponent::SendRenderTransform_Concurrent()
{
	if (!ContainerInstance && SimPositions.Num() > 0)
	{
		UpdateSimPositions();

		if(PreSimPositions.Num() != 0)
		{
			const int NumParticles = GetStaticMesh()->FlexAsset->Particles.Num();
			FBox WorldBounds(ForceInit);

			bool bFlexCloth = GetStaticMesh() && (GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetCloth::StaticClass());
			bool bFlexSoft = GetStaticMesh() && (GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetSoft::StaticClass());

			if (bFlexCloth || bFlexSoft)
			{
				for (int i=0; i < NumParticles; ++i)
				{
					WorldBounds += FVector(SimPositions[i]);
				}

				LocalBounds = FBoxSphereBounds(WorldBounds).TransformBy(GetComponentTransform().Inverse());

				// Clamp bounds in case of instability
				const float MaxRadius = 1000000.0f;

				if (LocalBounds.SphereRadius > MaxRadius)
				{
					LocalBounds = FBoxSphereBounds(EForceInit::ForceInitToZero);
				}
			}
		}
	}

	Super::SendRenderTransform_Concurrent();
}

FBoxSphereBounds UFlexComponent::CalcBounds(const FTransform & LocalToWorld) const
{
#if WITH_FLEX
	if (GetStaticMesh() && ( ContainerInstance || PreSimPositions.Num() != 0 ) && Bounds.SphereRadius > 0.0f 
		&& (GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetCloth::StaticClass() || GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetSoft::StaticClass()))
	{
		return LocalBounds.TransformBy(LocalToWorld);
	}
	else
	{
		return Super::CalcBounds(LocalToWorld);
	}
#else
	return Super::CalcBounds(LocalToWorld);
#endif
}

void UFlexComponent::DisableSim()
{
#if WITH_FLEX
	if (ContainerInstance && AssetInstance)
	{
		DEC_DWORD_STAT_BY(STAT_Flex_ActiveParticleCount, AssetInstance->numParticles);
		DEC_DWORD_STAT(STAT_Flex_ActiveMeshActorCount);

		ContainerInstance->DestroyInstance(AssetInstance);
		AssetInstance = NULL;
	}
#endif // WITH_FLEX
}

bool UFlexComponent::IsTearingCloth()
{
	const UFlexAssetCloth* ClothAsset = Cast<UFlexAssetCloth>(GetStaticMesh()->FlexAsset);
	
	if (ClothAsset)
	{
		return ClothAsset->TearingEnabled;
	}
	else
	{
		return false;
	}
}

void UFlexComponent::OnTear_Implementation()
{
	const UFlexAssetCloth* ClothAsset = Cast<UFlexAssetCloth>(GetStaticMesh()->FlexAsset);

	if (ClothAsset->TearingEnabled && TearingAsset)
	{
		// update tearing asset inflatable over pressure
		TearingAsset->inflatable = false;
	}
}

void UFlexComponent::EnableSim()
{
#if WITH_FLEX
	if (ContainerInstance && !AssetInstance)
	{
		// SimPositions count can be zero if asset internal FlexExtObject creation failed.
		if (SimPositions.Num() == 0)
			return;

		const NvFlexExtAsset* Asset = NULL;

		if (IsTearingCloth())
		{
			const UFlexAssetCloth* ClothAsset = Cast<UFlexAssetCloth>(GetStaticMesh()->FlexAsset);

			if (ClothAsset)
			{
				// clone asset for 
				TearingAsset = NvFlexExtCreateTearingClothFromMesh(
							(float*)&ClothAsset->Particles[0],
							ClothAsset->Particles.Num(),
							ClothAsset->Triangles.Num(),	// TODO: set limits correctly
							&ClothAsset->Triangles[0],
							ClothAsset->Triangles.Num()/3,
							ClothAsset->StretchStiffness,
							ClothAsset->BendStiffness,
							ClothAsset->EnableInflatable?ClothAsset->OverPressure:0.0f);

				Asset = TearingAsset;
			}
		}
		else
		{
			Asset = GetStaticMesh()->FlexAsset->GetFlexAsset();
		}


		AssetInstance = ContainerInstance->CreateInstance(Asset, GetComponentTransform().ToMatrixNoScale(), FVector(0.0f), ContainerInstance->GetPhase(Phase));

		if (AssetInstance)
		{
			INC_DWORD_STAT_BY(STAT_Flex_ActiveParticleCount, GetStaticMesh()->FlexAsset->Particles.Num());
			INC_DWORD_STAT(STAT_Flex_ActiveMeshActorCount);
		
			// if attach requested then generate attachment points for overlapping shapes
			if (AttachToRigids)
			{				
				for (int ParticleIndex=0; ParticleIndex < AssetInstance->numParticles; ++ParticleIndex)
				{
					FVector4 ParticlePos = SimPositions[ParticleIndex];

					// perform a point check (small sphere)
					FCollisionShape Shape;
					Shape.SetSphere(0.001f);

					// gather overlapping primitives, except owning actor
					TArray<FOverlapResult> Overlaps;
					FCollisionQueryParams QueryParams(false);
					QueryParams.AddIgnoredActor(GetOwner());
					GetWorld()->OverlapMultiByObjectType(Overlaps, ParticlePos, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllObjects), Shape, QueryParams);

					// pick first non-flex actor, that has a body and is not a trigger
					UPrimitiveComponent* PrimComp = NULL;
					int32 ItemIndex = INDEX_NONE;

					for (int32 OverlapIdx = 0; OverlapIdx<Overlaps.Num() && !PrimComp; ++OverlapIdx)
					{
						FOverlapResult const& O = Overlaps[OverlapIdx];
						
						if (!O.Component.IsValid() || O.Component.Get() == this)
							continue;

						UPrimitiveComponent* TmpPrimComp = O.Component.Get();

						if (TmpPrimComp->GetBodyInstance() == NULL)
							continue;

						ECollisionResponse Response = TmpPrimComp->GetCollisionResponseToChannel(ContainerInstance->Template->ObjectType);
						if (Response == ECollisionResponse::ECR_Ignore)
							continue; 
						
						PrimComp = TmpPrimComp;
						ItemIndex = O.ItemIndex;
					}

					if (PrimComp)
					{
						FTransform LocalToWorld;

						if (ItemIndex != INDEX_NONE)
						{
							const USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(PrimComp);
							if (SkeletalMeshComp)
							{
								FBodyInstance* Body = SkeletalMeshComp->Bodies[ItemIndex];
								LocalToWorld = Body->GetUnrealWorldTransform();
							}
							else
							{
								LocalToWorld = PrimComp->GetComponentToWorld();
							}
						}
						else
						{
							LocalToWorld = PrimComp->GetComponentToWorld();
						}

						// calculate local space position of particle in component
						FVector LocalPos = LocalToWorld.InverseTransformPosition(ParticlePos);

						FlexParticleAttachment Attachment;
						Attachment.Primitive = PrimComp;
						Attachment.ParticleIndex = ParticleIndex;
						Attachment.OldMass = ParticlePos.W;
						Attachment.LocalPos = LocalPos;
						Attachment.ItemIndex = ItemIndex;

						Attachments.Add(Attachment);
					}
				}
			}

			FBox WorldBounds(ForceInit);
			
			// apply any existing positions (pre-simulated particles)
			for (int32 i = 0; i < AssetInstance->numParticles; ++i)
			{
				ContainerInstance->Particles[AssetInstance->particleIndices[i]] = SimPositions[i];
				WorldBounds += FVector(SimPositions[i]);
			}

			LocalBounds = FBoxSphereBounds(WorldBounds).TransformBy(GetComponentTransform().Inverse());
			UpdateBounds();
		}
	}
#endif // WITH_FLEX
}

void UFlexComponent::AttachToComponent(USceneComponent* Component, float Radius)
{
	const FTransform Transform = Component->GetComponentTransform();

	for (int ParticleIndex = 0; ParticleIndex < SimPositions.Num(); ++ParticleIndex)
	{
		FVector4 ParticlePos = SimPositions[ParticleIndex];

		// skip infinite mass particles as they may already be attached to another component
		if (ParticlePos.W == 0.0f)
			continue;

		// test distance from component origin
		FVector Delta = FVector(ParticlePos) - Transform.GetTranslation();

		if (Delta.Size() < Radius)
		{
			// calculate local space position of particle in component
			FVector LocalPos = Transform.InverseTransformPosition(ParticlePos);

			FlexParticleAttachment Attachment;
			Attachment.Primitive = Component;
			Attachment.ParticleIndex = ParticleIndex;
			Attachment.OldMass = ParticlePos.W;
			Attachment.LocalPos = LocalPos;
			Attachment.ItemIndex = INDEX_NONE;

			Attachments.Add(Attachment);
		}
	}
}

FMatrix UFlexComponent::GetRenderMatrix() const
{
#if WITH_FLEX
	// Flex components components created in an editor world, do not have FFlexSceneMeshProxy - and so cannot simulate
	// Only need to return the Identity when we know the SceneProxy is full flex proxy
	if (GetStaticMesh() && GetStaticMesh()->FlexAsset &&  !IsInEditorWorld())
	{
		UClass* FlexAssetCls = GetStaticMesh()->FlexAsset->GetClass();
		if (FlexAssetCls == UFlexAssetCloth::StaticClass() || FlexAssetCls == UFlexAssetSoft::StaticClass())
		{
			// particles are simulated in world space
			return FMatrix::Identity;
		}
	}
#endif

	return Super::GetRenderMatrix();
}

bool UFlexComponent::IsInEditorWorld() const
{
	UWorld* World = GetWorld();
	// If there is no world - then safer to say it's in the editor (to avoid incorrect downcasting for SceneProxy)
	return !(World && World->IsGameWorld());
}

FPrimitiveSceneProxy* UFlexComponent::CreateSceneProxy()
{
#if WITH_FLEX
	// if this component has a flex asset then use the subtitute scene proxy for rendering (cloth and soft bodies only)
	if (GetStaticMesh() && GetStaticMesh()->FlexAsset && !IsInEditorWorld())
	{
		UClass* cls = GetStaticMesh()->FlexAsset->GetClass();
		if (cls == UFlexAssetCloth::StaticClass() || cls == UFlexAssetSoft::StaticClass())
		{
			FFlexMeshSceneProxy* Proxy = new FFlexMeshSceneProxy(this);
			// JS: UpdateSceneProxy is not needed because will be updated when  UFlexComponent::SendRenderDynamicData_Concurrent() at the begining of rendering phase
			// send initial render data to the proxy
			//UpdateSceneProxy(Proxy);
			return Proxy;
		}
	}
#endif // WITH_FLEX

	//@todo: figure out why i need a ::new (gcc3-specific)
	return Super::CreateSceneProxy();

}
bool UFlexComponent::ShouldRecreateProxyOnUpdateTransform() const
{
#if WITH_FLEX
	// if this component has a flex asset then don't recreate the proxy
	if (GetStaticMesh() && GetStaticMesh()->FlexAsset && (GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetCloth::StaticClass() ||
											   GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetSoft::StaticClass()))
	{
		if (AssetInstance && ContainerInstance)
		{
			return false;
		}
		else
		{
			return true;
		}
	}
#endif
	
	return Super::ShouldRecreateProxyOnUpdateTransform();
}


void UFlexComponent::UpdateSimPositions()
{
	if (GetStaticMesh() == NULL || GetStaticMesh()->FlexAsset == NULL)
		return;

	const int NumParticles = GetStaticMesh()->FlexAsset->Particles.Num();

	float InvMassScale = 1.0f;
	if (OverrideAsset)
	{
		InvMassScale = (Mass > 0.0f) ? (GetStaticMesh()->FlexAsset->Mass / Mass) : 0.0f;
	}

	if (NumParticles == PreSimPositions.Num())
	{
		// if pre-sim state still matches the static mesh apply any pre-simulated positions to the particles
		for (int i = 0; i < NumParticles; ++i)
		{
			float mass = GetStaticMesh()->FlexAsset->Particles[i].W*InvMassScale;

			SimPositions[i] = FVector4(PreSimPositions[i], mass);
		}

		RelativeLocation = PreSimRelativeLocation;
		RelativeRotation = PreSimRelativeRotation;
		SetComponentToWorld(PreSimTransform);		
	}
	else
	{
		// particles are static mesh positions transformed by actor position
		for (int i = 0; i < NumParticles; ++i)
		{
			FVector LocalPos = GetStaticMesh()->FlexAsset->Particles[i];
			float mass = GetStaticMesh()->FlexAsset->Particles[i].W*InvMassScale;

			SimPositions[i] = FVector4(FVector(GetComponentTransform().TransformPosition(LocalPos)), mass);
		}
	}

	// calculate normals for initial particle positions, this is necessary because otherwise 
	// the mesh will be rendered incorrectly if it is visible before it is first simulated
	// todo: serialize these initial normals
	if (GetStaticMesh()->FlexAsset->GetClass() == UFlexAssetCloth::StaticClass())
	{
		const TArray<int>& TriIndices = GetStaticMesh()->FlexAsset->Triangles;
		int NumTriangles = TriIndices.Num() / 3;

		const FVector4* RESTRICT Particles = &SimPositions[0];
		FVector* RESTRICT Normals = &SimNormals[0];

		// iterate over triangles updating vertex normals
		for (int i = 0; i < NumTriangles; ++i)
		{
			const int a = TriIndices[i * 3 + 0];
			const int b = TriIndices[i * 3 + 1];
			const int c = TriIndices[i * 3 + 2];

			FVector Vertex0 = Particles[a];
			FVector Vertex1 = Particles[b];
			FVector Vertex2 = Particles[c];

			FVector TriNormal = (Vertex1 - Vertex0) ^ (Vertex2 - Vertex0);

			Normals[a] += TriNormal;
			Normals[b] += TriNormal;
			Normals[c] += TriNormal;
		}

		// normalize normals
		for (int i = 0; i < NumParticles; ++i)
			Normals[i] = Normals[i].GetSafeNormal();
	}
}

void UFlexComponent::SynchronizeAttachments()
{
	if (ContainerInstance && AssetInstance)
	{
		// process attachments
		for (int AttachmentIndex=0; AttachmentIndex < Attachments.Num(); )
		{
			const FlexParticleAttachment& Attachment = Attachments[AttachmentIndex];
			const USceneComponent* SceneComp = Attachment.Primitive.Get();
		
			// index into the simulation data, we need to modify the container's copy
			// of the data so that the new positions get sent back to the sim
			const int ParticleIndex = AssetInstance->particleIndices[Attachment.ParticleIndex];

			if(SceneComp)
			{
				FTransform AttachTransform;

				const UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(SceneComp);

				if (PrimComp)
				{
					// primitive component attachments use the physics bodies
					
					FTransform LocalToWorld;

					if (Attachment.ItemIndex != INDEX_NONE)
					{
						const USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(PrimComp);
						if (SkeletalMeshComp)
						{
							FBodyInstance* Body = SkeletalMeshComp->Bodies[Attachment.ItemIndex];
							AttachTransform = Body->GetUnrealWorldTransform();
						}
						else
						{
							// todo: try destructible actors
							AttachTransform = PrimComp->GetComponentToWorld();
						}					
					}
					else
					{
						AttachTransform = PrimComp->GetComponentToWorld();
					}
				}
				else
				{
					// regular components attach to the actor transform
					AttachTransform = SceneComp->GetComponentTransform();
				}

				const FVector AttachedPos = AttachTransform.TransformPosition(Attachment.LocalPos);

				ContainerInstance->Particles[ParticleIndex] = FVector4(AttachedPos, 0.0f);
				ContainerInstance->Velocities[ParticleIndex] = FVector(0.0f);

				++AttachmentIndex;
			}
			else // process detachments
			{
				ContainerInstance->Particles[ParticleIndex].W = Attachment.OldMass;
				ContainerInstance->Velocities[ParticleIndex] = FVector(0.0f);
				
				Attachments.RemoveAtSwap(AttachmentIndex);
			}
		}
	}
}

UFlexContainer* UFlexComponent::GetContainerTemplate()
{
	return ContainerInstance ? ContainerInstance->Template : nullptr;
}

#endif // WITH_FLEX
