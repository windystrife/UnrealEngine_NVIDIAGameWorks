// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/RadialForceComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Actor.h"
#include "WorldCollision.h"
#include "Engine/World.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/MovementComponent.h"
#include "PhysicsEngine/RadialForceActor.h"
#include "DestructibleInterface.h"

#if WITH_FLEX
#include "PhysicsEngine/FlexActor.h"
#include "PhysicsEngine/FlexComponent.h"
#include "Particles/ParticleSystemComponent.h"
#endif

//////////////////////////////////////////////////////////////////////////
// RADIALFORCECOMPONENT
URadialForceComponent::URadialForceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Radius = 200.0f;
	Falloff = RIF_Constant;
	ImpulseStrength = 1000.0f;
	ForceStrength = 10.0f;
	bAutoActivate = true;
	FlexAttach = false;

	// by default we affect all 'dynamic' objects that can currently be affected by forces
	AddCollisionChannelToAffect(ECC_Pawn);
	AddCollisionChannelToAffect(ECC_PhysicsBody);
	AddCollisionChannelToAffect(ECC_Vehicle);
	AddCollisionChannelToAffect(ECC_Destructible);

	UpdateCollisionObjectQueryParams();
}

void URadialForceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(bIsActive)
	{
		const FVector Origin = GetComponentLocation();

		// Find objects within the sphere
		TArray<FOverlapResult> Overlaps;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(AddForceOverlap), false);
		Params.bTraceAsyncScene = true; // want to hurt stuff in async scene

		// Ignore owner actor if desired
		if (bIgnoreOwningActor)
		{
			Params.AddIgnoredActor(GetOwner());
		}

		GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, CollisionObjectQueryParams, FCollisionShape::MakeSphere(Radius), Params);

		// A component can have multiple physics presences (e.g. destructible mesh components).
		// The component should handle the radial force for all of the physics objects it contains
		// so here we grab all of the unique components to avoid applying impulses more than once.
		TArray<UPrimitiveComponent*, TInlineAllocator<1>> AffectedComponents;
		AffectedComponents.Reserve(Overlaps.Num());

		for(FOverlapResult& OverlapResult : Overlaps)
		{
			if(UPrimitiveComponent* PrimitiveComponent = OverlapResult.Component.Get())
			{
				AffectedComponents.AddUnique(PrimitiveComponent);
			}
		}

		for(UPrimitiveComponent* PrimitiveComponent : AffectedComponents)
		{
			PrimitiveComponent->AddRadialForce(Origin, Radius, ForceStrength, Falloff);

			// see if this is a target for a movement component
			AActor* ComponentOwner = PrimitiveComponent->GetOwner();
			if(ComponentOwner)
			{
				TInlineComponentArray<UMovementComponent*> MovementComponents;
				ComponentOwner->GetComponents<UMovementComponent>(MovementComponents);
				for(const auto& MovementComponent : MovementComponents)
				{
					if(MovementComponent->UpdatedComponent == PrimitiveComponent)
					{
						MovementComponent->AddRadialForce(Origin, Radius, ForceStrength, Falloff);
						break;
					}
				}
			}
		}
	
	#if WITH_FLEX
			if (ForceStrength != 0.0f)
			{
				FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
				const uint32 FlexBit = ECC_TO_BITFIELD(ECC_Flex);
				if (PhysScene && (CollisionObjectQueryParams.GetQueryBitfield() & FlexBit) != 0)
				{
					PhysScene->AddRadialForceToFlex(Origin, Radius, ForceStrength, Falloff);
	}
}
	#endif

	}
}

void URadialForceComponent::BeginPlay()
{
	Super::BeginPlay();

	UpdateCollisionObjectQueryParams();

#if WITH_FLEX
	
	// create rigid attachments to overlapping Flex actors
	if (FlexAttach)
	{
		const FVector Origin = GetComponentLocation();

		for (TActorIterator<AFlexActor> It(GetWorld()); It; ++It)
		{
			AFlexActor* FlexActor = (*It);
			UFlexComponent* FlexComponent = Cast<UFlexComponent>(FlexActor->GetRootComponent());

			if (FlexComponent)
			{
				const FBoxSphereBounds FlexBounds = FlexComponent->GetBounds();
				
				// dist of force field to flex bounds
				const float DistSq = FlexBounds.ComputeSquaredDistanceFromBoxToPoint(Origin);

				if (DistSq < Radius*Radius)
				{
					FlexComponent->AttachToComponent(this, Radius);
				}
			}
		}

		// Find all ParticleSystemComponents
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* TestActor= (*It);
			TArray<UActorComponent*> ParticleComponents = TestActor->GetComponentsByClass(UParticleSystemComponent::StaticClass());
			for (int32 ComponentIdx = 0; ComponentIdx < ParticleComponents.Num(); ++ComponentIdx)
			{
				UParticleSystemComponent* ParticleSystemComponet = Cast<UParticleSystemComponent>(ParticleComponents[ComponentIdx]);

				// is this a PSC with Flex?
				if (ParticleSystemComponet && ParticleSystemComponet->GetFirstFlexContainerTemplate())
				{
					const FBoxSphereBounds FlexBounds = ParticleSystemComponet->CalcBounds(FTransform::Identity);

					// dist of force field to flex bounds
					const float DistSq = FlexBounds.ComputeSquaredDistanceFromBoxToPoint(Origin);

					if (DistSq < Radius*Radius)
					{
						ParticleSystemComponet->AttachFlexToComponent(this, Radius);
					}
				}

			}
		}
	}

#endif
}

void URadialForceComponent::PostLoad()
{
	Super::PostLoad();

	UpdateCollisionObjectQueryParams();
}

void URadialForceComponent::FireImpulse()
{
	const FVector Origin = GetComponentLocation();

	// Find objects within the sphere
	TArray<FOverlapResult> Overlaps;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FireImpulseOverlap),  false);
	Params.bTraceAsyncScene = true; // want to hurt stuff in async scene

	// Ignore owner actor if desired
	if (bIgnoreOwningActor)
	{
		Params.AddIgnoredActor(GetOwner());
	}

	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, CollisionObjectQueryParams, FCollisionShape::MakeSphere(Radius), Params);

	// A component can have multiple physics presences (e.g. destructible mesh components).
	// The component should handle the radial force for all of the physics objects it contains
	// so here we grab all of the unique components to avoid applying impulses more than once.
	TArray<UPrimitiveComponent*, TInlineAllocator<1>> AffectedComponents;
	AffectedComponents.Reserve(Overlaps.Num());

	for(FOverlapResult& OverlapResult : Overlaps)
	{
		if(UPrimitiveComponent* PrimitiveComponent = OverlapResult.Component.Get())
		{
			AffectedComponents.AddUnique(PrimitiveComponent);
		}
	}

	for(UPrimitiveComponent* PrimitiveComponent : AffectedComponents)
	{
		if(DestructibleDamage > SMALL_NUMBER)
		{
			if(IDestructibleInterface* DestructibleInstance = Cast<IDestructibleInterface>(PrimitiveComponent))
			{
				DestructibleInstance->ApplyRadiusDamage(DestructibleDamage, Origin, Radius, ImpulseStrength, Falloff == RIF_Constant);
			}
		}

		// Apply impulse
		PrimitiveComponent->AddRadialImpulse(Origin, Radius, ImpulseStrength, Falloff, bImpulseVelChange);

		// See if this is a target for a movement component, if so apply the impulse to it
		if (PrimitiveComponent->bIgnoreRadialImpulse == false)
		{
			TInlineComponentArray<UMovementComponent*> MovementComponents;
			PrimitiveComponent->GetOwner()->GetComponents<UMovementComponent>(MovementComponents);
			for (const auto& MovementComponent : MovementComponents)
			{
				if (MovementComponent->UpdatedComponent == PrimitiveComponent)
				{
					MovementComponent->AddRadialImpulse(Origin, Radius, ImpulseStrength, Falloff, bImpulseVelChange);
					break;
				}
			}
		}

#if WITH_FLEX
	if (ImpulseStrength != 0.0f)
	{
		FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
		const uint32 FlexBit = ECC_TO_BITFIELD(ECC_Flex);
		if (PhysScene && (CollisionObjectQueryParams.GetQueryBitfield() & FlexBit) != 0)
		{
			PhysScene->AddRadialImpulseToFlex(Origin, Radius, ImpulseStrength, Falloff, bImpulseVelChange);
		}
	}
#endif
	}
}

void URadialForceComponent::AddCollisionChannelToAffect(enum ECollisionChannel CollisionChannel)
{
	EObjectTypeQuery ObjectType = UEngineTypes::ConvertToObjectType(CollisionChannel);
	if(ObjectType != ObjectTypeQuery_MAX)
	{
		AddObjectTypeToAffect(ObjectType);
	}
}

void URadialForceComponent::AddObjectTypeToAffect(TEnumAsByte<enum EObjectTypeQuery> ObjectType)
{
	ObjectTypesToAffect.AddUnique(ObjectType);
	UpdateCollisionObjectQueryParams();
}

void URadialForceComponent::RemoveObjectTypeToAffect(TEnumAsByte<enum EObjectTypeQuery> ObjectType)
{
	ObjectTypesToAffect.Remove(ObjectType);
	UpdateCollisionObjectQueryParams();
}

#if WITH_EDITOR

void URadialForceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// If we have edited the object types to effect, update our bitfield.
	if(PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == TEXT("ObjectTypesToAffect"))
	{
		UpdateCollisionObjectQueryParams();
	}
}

#endif

void URadialForceComponent::UpdateCollisionObjectQueryParams()
{
	CollisionObjectQueryParams = FCollisionObjectQueryParams(ObjectTypesToAffect);
}


//////////////////////////////////////////////////////////////////////////
// ARB_RADIALFORCEACTOR
ARadialForceActor::ARadialForceActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ForceComponent = CreateDefaultSubobject<URadialForceComponent>(TEXT("ForceComponent0"));

#if WITH_EDITOR
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		// Structure to hold one-time initialization
		if (!IsRunningCommandlet())
		{
			struct FConstructorStatics
			{
				ConstructorHelpers::FObjectFinderOptional<UTexture2D> RadialForceTexture;
				FName ID_Physics;
				FText NAME_Physics;
				FConstructorStatics()
					: RadialForceTexture(TEXT("/Engine/EditorResources/S_RadForce.S_RadForce"))
					, ID_Physics(TEXT("Physics"))
					, NAME_Physics(NSLOCTEXT( "SpriteCategory", "Physics", "Physics" ))
				{
				}
			};
			static FConstructorStatics ConstructorStatics;

			SpriteComponent->Sprite = ConstructorStatics.RadialForceTexture.Get();

#if WITH_EDITORONLY_DATA
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Physics;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Physics;
#endif // WITH_EDITORONLY_DATA
		}

		SpriteComponent->RelativeScale3D.X = 0.5f;
		SpriteComponent->RelativeScale3D.Y = 0.5f;
		SpriteComponent->RelativeScale3D.Z = 0.5f;
		SpriteComponent->SetupAttachment(ForceComponent);
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif

	RootComponent = ForceComponent;
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	NetUpdateFrequency = 0.1f;
}

#if WITH_EDITOR
void ARadialForceActor::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	FVector ModifiedScale = DeltaScale * ( AActor::bUsePercentageBasedScaling ? 500.0f : 5.0f );

	const float Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;
	if(ForceComponent)
	{
		ForceComponent->Radius += Multiplier * ModifiedScale.Size();
		ForceComponent->Radius = FMath::Max( 0.f, ForceComponent->Radius );
	}
}
#endif

void ARadialForceActor::FireImpulse()
{
	if(ForceComponent)
	{
		ForceComponent->FireImpulse();
	}
}

void ARadialForceActor::EnableForce()
{
	if(ForceComponent)
	{
		ForceComponent->Activate();
	}
}

void ARadialForceActor::DisableForce()
{
	if(ForceComponent)
	{
		ForceComponent->Deactivate();
	}
}

void ARadialForceActor::ToggleForce()
{
	if(ForceComponent)
	{
		ForceComponent->ToggleActive();
	}
}