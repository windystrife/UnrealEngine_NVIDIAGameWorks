// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/SkeletalMeshActor.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Engine/CollisionProfile.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshActor"


ASkeletalMeshActor::ASkeletalMeshActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent0"));
	SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPose;
	// check BaseEngine.ini for profile setup
	SkeletalMeshComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	RootComponent = SkeletalMeshComponent;

	bShouldDoAnimNotifies = true;
}

void ASkeletalMeshActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASkeletalMeshActor, ReplicatedMesh);
	DOREPLIFETIME(ASkeletalMeshActor, ReplicatedPhysAsset);
	DOREPLIFETIME(ASkeletalMeshActor, ReplicatedMaterial0);
	DOREPLIFETIME(ASkeletalMeshActor, ReplicatedMaterial1);
}

void ASkeletalMeshActor::PreviewBeginAnimControl(UInterpGroup* InInterpGroup)
{
	if (CanPlayAnimation())
	{
		UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
		if (!AnimInst)
		{
			SkeletalMeshComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
		}
	}
}


void ASkeletalMeshActor::PreviewFinishAnimControl(UInterpGroup* InInterpGroup)
{
	if (CanPlayAnimation())
	{
		// if in editor, reset the Animations, makes easier for artist to see them visually and align them
		// in game, we keep the last pose that matinee kept. If you'd like it to have animation, you'll need to have AnimTree or AnimGraph to handle correctly
		if (SkeletalMeshComponent->GetAnimationMode() == EAnimationMode::Type::AnimationBlueprint)
		{
			UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
			if (AnimInst)
			{
				AnimInst->Montage_Stop(0.f);
				AnimInst->UpdateAnimation(0.f, false);
			}
		}
		// Update space bases to reset it back to ref pose
		SkeletalMeshComponent->RefreshBoneTransforms();
		SkeletalMeshComponent->RefreshSlaveComponents();
		SkeletalMeshComponent->UpdateComponentToWorld();
	}
}


void ASkeletalMeshActor::PreviewSetAnimPosition(FName SlotName, int32 ChannelIndex, UAnimSequence* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies, float DeltaTime)
{
	if (CanPlayAnimation(InAnimSequence))
	{
		TWeakObjectPtr<class UAnimMontage>& CurrentlyPlayingMontage = CurrentlyPlayingMontages.FindOrAdd(SlotName);
		CurrentlyPlayingMontage = FAnimMontageInstance::PreviewMatineeSetAnimPositionInner(SlotName, SkeletalMeshComponent, InAnimSequence, InPosition, bLooping, bFireNotifies, DeltaTime);
	}
}

void ASkeletalMeshActor::PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos)
{
	//no support yet
}

void ASkeletalMeshActor::SetAnimWeights(const TArray<struct FAnimSlotInfo>& SlotInfos)
{
	//no support yet
}

/** Check SkeletalMeshActor for errors. */
#if WITH_EDITOR
void ASkeletalMeshActor::CheckForErrors()
{
	Super::CheckForErrors();

	if (SkeletalMeshComponent)
	{
		UPhysicsAsset * const PhysicsAsset = SkeletalMeshComponent->GetPhysicsAsset();
		if (!PhysicsAsset)
		{
			if (SkeletalMeshComponent->CastShadow
				&& SkeletalMeshComponent->bCastDynamicShadow)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("DetailedInfo"), FText::FromString(GetDetailedInfo()));
				FMessageLog("MapCheck").PerformanceWarning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_SkelMeshActorNoPhysAsset", "SkeletalMeshActor '{DetailedInfo}' casts shadow but has no PhysicsAsset assigned.  The shadow will be low res and inefficient"), Arguments)))
					->AddToken(FMapErrorToken::Create(FMapErrors::SkelMeshActorNoPhysAsset));
			}
		}

		if (SkeletalMeshComponent->CastShadow
			&& SkeletalMeshComponent->bCastDynamicShadow
			&& SkeletalMeshComponent->IsRegistered()
			&& SkeletalMeshComponent->Bounds.SphereRadius > 2000.0f)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("DetailedInfo"), FText::FromString(GetDetailedInfo()));
			// Large shadow casting objects that create preshadows will cause a massive performance hit, since preshadows are meant for small shadow casters.
			FMessageLog("MapCheck").PerformanceWarning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_ActorLargeShadowCaster", "{DetailedInfo} : Large actor casts a shadow and will cause an extreme performance hit unless shadow casting is disabled"), Arguments)))
				->AddToken(FMapErrorToken::Create(FMapErrors::ActorLargeShadowCaster));
		}

		if (SkeletalMeshComponent->SkeletalMesh == NULL)
		{
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_SkeletalMeshNull", "Skeletal mesh actor has NULL SkeletalMesh property")))
				->AddToken(FMapErrorToken::Create(FMapErrors::SkeletalMeshNull));
		}
	}
	else
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_SkeletalMeshComponent", "Skeletal mesh actor has NULL SkeletalMeshComponent property")))
			->AddToken(FMapErrorToken::Create(FMapErrors::SkeletalMeshComponent));
	}
}
#endif

FString ASkeletalMeshActor::GetDetailedInfoInternal() const
{
	FString Result;

	if (SkeletalMeshComponent)
	{
		Result = SkeletalMeshComponent->GetDetailedInfoInternal();
	}
	else
	{
		Result = TEXT("No_SkeletalMeshComponent");
	}

	return Result;
}

void ASkeletalMeshActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// grab the current mesh for replication
	if (Role == ROLE_Authority && SkeletalMeshComponent)
	{
		ReplicatedMesh = SkeletalMeshComponent->SkeletalMesh;
	}

	// Unfix bodies flagged as 'full anim weight'
	if (SkeletalMeshComponent)
	{
		ReplicatedPhysAsset = SkeletalMeshComponent->GetPhysicsAsset();
	}
}

void ASkeletalMeshActor::OnRep_ReplicatedMesh()
{
	SkeletalMeshComponent->SetSkeletalMesh(ReplicatedMesh);
}

void ASkeletalMeshActor::OnRep_ReplicatedPhysAsset()
{
	SkeletalMeshComponent->SetPhysicsAsset(ReplicatedPhysAsset);
}

void ASkeletalMeshActor::OnRep_ReplicatedMaterial0()
{
	SkeletalMeshComponent->SetMaterial(0, ReplicatedMaterial0);
}

void ASkeletalMeshActor::OnRep_ReplicatedMaterial1()
{
	SkeletalMeshComponent->SetMaterial(1, ReplicatedMaterial1);
}

void ASkeletalMeshActor::BeginAnimControl(UInterpGroup* InInterpGroup)
{
	if (CanPlayAnimation())
	{
		UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
		if (!AnimInst)
		{
			SkeletalMeshComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
		}
	}
}

bool ASkeletalMeshActor::CanPlayAnimation(class UAnimSequenceBase* AnimAssetBase/*=NULL*/) const
{
	return (SkeletalMeshComponent->SkeletalMesh && SkeletalMeshComponent->SkeletalMesh->Skeleton &&
		(!AnimAssetBase || SkeletalMeshComponent->SkeletalMesh->Skeleton->IsCompatible(AnimAssetBase->GetSkeleton())));
}

void ASkeletalMeshActor::SetAnimPosition(FName SlotName, int32 ChannelIndex, UAnimSequence* InAnimSequence, float InPosition, bool bFireNotifies, bool bLooping)
{
	if (CanPlayAnimation(InAnimSequence))
	{
		TWeakObjectPtr<class UAnimMontage>& CurrentlyPlayingMontage = CurrentlyPlayingMontages.FindOrAdd(SlotName);
		CurrentlyPlayingMontage = FAnimMontageInstance::SetMatineeAnimPositionInner(SlotName, SkeletalMeshComponent, InAnimSequence, InPosition, bLooping);
	}
}

void ASkeletalMeshActor::FinishAnimControl(UInterpGroup* InInterpGroup)
{
	if (SkeletalMeshComponent->GetAnimationMode() == EAnimationMode::Type::AnimationBlueprint)
	{
		UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
		if (AnimInst)
		{
			AnimInst->Montage_Stop(0.f);
			AnimInst->UpdateAnimation(0.f, false);
		}

		// Update space bases to reset it back to ref pose
		SkeletalMeshComponent->RefreshBoneTransforms();
		SkeletalMeshComponent->RefreshSlaveComponents();
		SkeletalMeshComponent->UpdateComponentToWorld();
	}
}


#if WITH_EDITOR

bool ASkeletalMeshActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (SkeletalMeshComponent->SkeletalMesh)
	{
		Objects.Add(SkeletalMeshComponent->SkeletalMesh);
	}
	return true;
}

void ASkeletalMeshActor::EditorReplacedActor(AActor* OldActor)
{
	Super::EditorReplacedActor(OldActor);

	if (ASkeletalMeshActor * OldSkelMeshActor = Cast<ASkeletalMeshActor>(OldActor))
	{
		// if no skeletal mesh set, take one from previous actor
		if (SkeletalMeshComponent && OldSkelMeshActor->SkeletalMeshComponent &&
			SkeletalMeshComponent->SkeletalMesh == NULL)
		{
			SkeletalMeshComponent->SetSkeletalMesh(OldSkelMeshActor->SkeletalMeshComponent->SkeletalMesh);
		}
	}
}

void ASkeletalMeshActor::LoadedFromAnotherClass(const FName& OldClassName)
{
	Super::LoadedFromAnotherClass(OldClassName);

	if (GetLinkerUE4Version() < VER_UE4_REMOVE_SKELETALPHYSICSACTOR)
	{
		static FName SkeletalPhysicsActor_NAME(TEXT("SkeletalPhysicsActor"));
		static FName KAsset_NAME(TEXT("KAsset"));

		if (OldClassName == SkeletalPhysicsActor_NAME || OldClassName == KAsset_NAME)
		{
			SkeletalMeshComponent->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipSimulatingBones;
			SkeletalMeshComponent->BodyInstance.bSimulatePhysics = true;
			SkeletalMeshComponent->bBlendPhysics = true;

			bAlwaysRelevant = true;
			bReplicateMovement = true;
			SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
			bReplicates = true;
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
