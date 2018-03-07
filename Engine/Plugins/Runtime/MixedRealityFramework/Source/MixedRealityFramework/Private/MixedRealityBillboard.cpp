// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityBillboard.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "MixedRealityUtilLibrary.h" // for GetHMDCameraComponent()
#include "MixedRealityCaptureComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstance.h"

/* MixedRealityProjection_Impl
 *****************************************************************************/

namespace MixedRealityProjection_Impl
{
	static APawn* FindTargetPlayer(const AMixedRealityProjectionActor* ProjectionActor)
	{
		APawn* TargetPawn = nullptr;

		USceneComponent* AttacheRoot = ProjectionActor->GetRootComponent();
		USceneComponent* AttachParent = ensure(AttacheRoot) ? AttacheRoot->GetAttachParent() : nullptr;
		AActor* OwningActor = AttachParent ? AttachParent->GetOwner() : nullptr;

		if (UWorld* TargetWorld = ProjectionActor->GetWorld())
		{
			APawn* FallbackPawn = nullptr;

			const TArray<ULocalPlayer*>& LocalPlayers = GEngine->GetGamePlayers(TargetWorld);
			for (ULocalPlayer* Player : LocalPlayers)
			{
				APlayerController* Controller = Player->GetPlayerController(TargetWorld);
				if (Controller == nullptr)
				{
					continue;
				}

				APawn* PlayerPawn = Controller->GetPawn();
				if (PlayerPawn == nullptr)
				{
					continue;
				}
				else if (FallbackPawn == nullptr)
				{
					FallbackPawn = PlayerPawn;
				}

				const bool bIsOwningPlayer =
					(OwningActor && (OwningActor->IsOwnedBy(Controller) || OwningActor->IsOwnedBy(PlayerPawn))) ||
					(ProjectionActor->IsOwnedBy(Controller) || ProjectionActor->IsOwnedBy(PlayerPawn));

				if (bIsOwningPlayer)
				{
					TargetPawn = PlayerPawn;
					break;
				}
			}

			if (!TargetPawn)
			{
				TargetPawn = FallbackPawn;
			}
		}

		return TargetPawn;
	}
}

//------------------------------------------------------------------------------
UMixedRealityBillboard::UMixedRealityBillboard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	if (AActor* Owner = GetOwner())
	{
		AddTickPrerequisiteActor(Owner);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityBillboard::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if (AActor* Owner = GetOwner())
	{
		USceneComponent* AttachComponent = Owner->GetRootComponent()->GetAttachParent();
		if ( /*ensure*/(AttachComponent) )
		{
			// assume we're being driven by AMixedRealityBillboardActor, attached to a UMixedRealityCaptureComponent
			const FVector CaptureFwdVec = AttachComponent->GetForwardVector();
			const FVector CaptureOrigin = AttachComponent->GetComponentLocation();

			ResetRelativeTransform();
			FVector TargetWorldPos = GetComponentLocation();

			const FVector ToTargetVect   = TargetWorldPos - CaptureOrigin;
			const FVector FwdProjection  = CaptureFwdVec * (/*DotProduct: */CaptureFwdVec | ToTargetVect);
			const FVector RelativeOffset = FwdProjection - ToTargetVect;

			SetRelativeLocationAndRotation(RelativeOffset, FRotator::ZeroRotator);
		}
		// @HACK: to work around UE-48605
		else if (Owner->IsA<AMixedRealityProjectionActor>())
		{
			Owner->Destroy();
		}
	}
}

/* AMixedRealityBillboardActor
 *****************************************************************************/

//------------------------------------------------------------------------------
AMixedRealityProjectionActor::AMixedRealityProjectionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial;
		float DefaultAspectRatio;

		FConstructorStatics()
			: DefaultMaterial(TEXT("/MixedRealityFramework/M_MRCamSrcProcessing"))
			, DefaultAspectRatio(16.f / 9.f)
		{}
	};
	static FConstructorStatics ConstructorStatics;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

	UWorld* MyWorld = GetWorld();
	const bool bIsEditorInst = MyWorld && (MyWorld->WorldType == EWorldType::Editor || MyWorld->WorldType == EWorldType::EditorPreview);
	
	ProjectionComponent = CreateDefaultSubobject<UMixedRealityBillboard>(TEXT("MR_ProjectionMesh"));
	ProjectionComponent->SetupAttachment(RootComponent);
	ProjectionComponent->AddElement(ConstructorStatics.DefaultMaterial.Object, 
		/*DistanceToOpacityCurve =*/nullptr, 
		/*bSizeIsInScreenSpace =*/true, 
		/*BaseSizeX =*/1.0f, 
		/*BaseSizeY=*/ConstructorStatics.DefaultAspectRatio, 
		/*DistanceToSizeCurve =*/nullptr);
	ProjectionComponent->CastShadow = false;
	ProjectionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// expects that this actor (or one of its owners) is used as the scene's view 
	// actor (hidden in the editor via UMixedRealityBillboard::GetHiddenEditorViews)
	ProjectionComponent->bOnlyOwnerSee = !bIsEditorInst;
	ProjectionComponent->SetComponentTickEnabled(false);
}

//------------------------------------------------------------------------------
void AMixedRealityProjectionActor::BeginPlay()
{
	Super::BeginPlay();

	UWorld* MyWorld = GetWorld();
	for (FConstPlayerControllerIterator PlayerIt = MyWorld->GetPlayerControllerIterator(); PlayerIt; ++PlayerIt)
	{
		if (APlayerController* PlayerController = PlayerIt->Get())
		{
			PlayerController->HiddenPrimitiveComponents.AddUnique(ProjectionComponent);
		}
	}

	SetActorRelativeLocation(FVector::ForwardVector * (GNearClippingPlane + 0.01f));
}

//------------------------------------------------------------------------------
void AMixedRealityProjectionActor::Tick(float DeltaSeconds)
{
	if (!AttachTarget.IsValid())
	{
		APawn* TargetPawn = MixedRealityProjection_Impl::FindTargetPlayer(this);
		SetDepthTarget(TargetPawn);
	}

	if (!AttachTarget.IsValid())
	{
		if (USceneComponent* AttachComponent = GetRootComponent()->GetAttachParent())
		{
			SetActorRelativeLocation(FVector::ForwardVector * (GNearClippingPlane + 0.01f));
		}
	}
	else
	{
		FVector WorldPos = AttachTarget->GetComponentLocation();
		RootComponent->SetWorldLocation(WorldPos);
	}
}

//------------------------------------------------------------------------------
void AMixedRealityProjectionActor::SetProjectionMaterial(UMaterialInterface* VidProcessingMat)
{
	ProjectionComponent->SetMaterial(/*ElementIndex =*/0, VidProcessingMat);
}

//------------------------------------------------------------------------------
void AMixedRealityProjectionActor::SetProjectionAspectRatio(const float NewAspectRatio)
{
	FMaterialSpriteElement& Sprite = ProjectionComponent->Elements[0];
	if (Sprite.BaseSizeY != NewAspectRatio)
	{
		Sprite.BaseSizeY = NewAspectRatio;
		ProjectionComponent->MarkRenderStateDirty();
	}	
}

//------------------------------------------------------------------------------
FVector AMixedRealityProjectionActor::GetTargetPosition() const
{
	if (AttachTarget.IsValid())
	{
		return AttachTarget->GetComponentLocation();
	}
	return GetActorLocation();
}

//------------------------------------------------------------------------------
void AMixedRealityProjectionActor::SetDepthTarget(const APawn* PlayerPawn)
{
	if (AttachTarget.IsValid())
	{
		RemoveTickPrerequisiteComponent(AttachTarget.Get());
	}

	UCameraComponent* HMDCam = UMixedRealityUtilLibrary::GetHMDCameraComponent(PlayerPawn);
	if (HMDCam)
	{
		AttachTarget = HMDCam;
	}
	else if (PlayerPawn)
	{
		AttachTarget = PlayerPawn->GetRootComponent();
	}
	else
	{
		AttachTarget.Reset();
	}
	RefreshTickState();
}

//------------------------------------------------------------------------------
void AMixedRealityProjectionActor::RefreshTickState()
{
	if (USceneComponent* AttachParent = RootComponent->GetAttachParent())
	{
		AddTickPrerequisiteComponent(AttachParent);
	}

	const bool bValidAttachTarget = AttachTarget.IsValid();
	if (bValidAttachTarget)
	{
		AddTickPrerequisiteComponent(AttachTarget.Get());
	}
	ProjectionComponent->SetComponentTickEnabled(bValidAttachTarget);
}

