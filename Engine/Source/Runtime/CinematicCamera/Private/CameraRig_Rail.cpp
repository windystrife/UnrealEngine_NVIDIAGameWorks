// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CameraRig_Rail.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"

#define LOCTEXT_NAMESPACE "CameraRig_Rail"

ACameraRig_Rail::ACameraRig_Rail(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	CurrentPositionOnRail = 0.f;

	// Create components
	TransformComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TransformComponent"));

	// Make the scene component the root component
	RootComponent = TransformComponent;

	RailSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("RailSplineComponent"));
	RailSplineComponent->SetupAttachment(TransformComponent);

	RailCameraMount = CreateDefaultSubobject<USceneComponent>(TEXT("RailCameraMount"));
	RailCameraMount->SetupAttachment(RailSplineComponent);

#if WITH_EDITORONLY_DATA
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> RailMesh(TEXT("/Engine/EditorMeshes/Camera/SM_RailRig_Track.SM_RailRig_Track"));
		PreviewRailStaticMesh = RailMesh.Object;

		static ConstructorHelpers::FObjectFinder<UStaticMesh> MountMesh(TEXT("/Engine/EditorMeshes/Camera/SM_RailRig_Mount.SM_RailRig_Mount"));
		PreviewMesh_Mount = CreateOptionalDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh_Mount"));
		if (PreviewMesh_Mount)
		{
			PreviewMesh_Mount->SetStaticMesh(MountMesh.Object);
			PreviewMesh_Mount->bIsEditorOnly = true;
			PreviewMesh_Mount->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			PreviewMesh_Mount->bHiddenInGame = true;
			PreviewMesh_Mount->CastShadow = false;
			PreviewMesh_Mount->PostPhysicsComponentTick.bCanEverTick = false;

			PreviewMesh_Mount->SetupAttachment(RailCameraMount);
		}
	}
#endif
}

#if WITH_EDITORONLY_DATA
USplineMeshComponent* ACameraRig_Rail::CreateSplinePreviewSegment()
{
	USplineMeshComponent* const Segment = NewObject<USplineMeshComponent>(this);
	if (Segment)
	{
		Segment->SetStaticMesh(PreviewRailStaticMesh);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->bIsEditorOnly = true;
		Segment->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Segment->bHiddenInGame = true;
		Segment->CastShadow = false;
		Segment->PostPhysicsComponentTick.bCanEverTick = false;
	 
		Segment->SetupAttachment(TransformComponent);
		Segment->RegisterComponent();
	}

	return Segment;
}

void ACameraRig_Rail::UpdatePreviewMeshes()
{
	if (RailSplineComponent)
	{
		if (PreviewRailStaticMesh)
		{
			int32 const NumSplinePoints = RailSplineComponent->GetNumberOfSplinePoints();
			int32 const NumNeededPreviewMeshes = NumSplinePoints - 1;

			// make sure our preview mesh array is correctly sized and populated
			{
				int32 const NumExistingPreviewMeshes = PreviewRailMeshSegments.Num();
				if (NumExistingPreviewMeshes > NumNeededPreviewMeshes)
				{
					// we have too many meshes, remove some
					int32 const NumToRemove = NumExistingPreviewMeshes - NumNeededPreviewMeshes;
					for (int Idx = 0; Idx < NumToRemove; ++Idx)
					{
						USplineMeshComponent* const ElementToRemove = PreviewRailMeshSegments.Pop();
						ElementToRemove->UnregisterComponent();
					}
				}
				else if (NumExistingPreviewMeshes < NumNeededPreviewMeshes)
				{
					int32 const NumToAdd = NumNeededPreviewMeshes - NumExistingPreviewMeshes;

					for (int32 Idx = 0; Idx < NumToAdd; ++Idx)
					{
						USplineMeshComponent* PreviewMesh = CreateSplinePreviewSegment();
						PreviewRailMeshSegments.Add(PreviewMesh);
					}
				}
				check(PreviewRailMeshSegments.Num() == NumNeededPreviewMeshes);
			}

			for (int PtIdx = 0; PtIdx < NumSplinePoints - 1; ++PtIdx)
			{
				FVector StartLoc, StartTangent, EndLoc, EndTangent;
				RailSplineComponent->GetLocationAndTangentAtSplinePoint(PtIdx, StartLoc, StartTangent, ESplineCoordinateSpace::Local);
				RailSplineComponent->GetLocationAndTangentAtSplinePoint(PtIdx + 1, EndLoc, EndTangent, ESplineCoordinateSpace::Local);

				USplineMeshComponent* const SplineMeshComp = PreviewRailMeshSegments[PtIdx];
				if (SplineMeshComp)
				{
					SplineMeshComp->SetForwardAxis(ESplineMeshAxis::Z);
					SplineMeshComp->SetStartAndEnd(StartLoc, StartTangent, EndLoc, EndTangent, true);
				}
			}

			// Unregister any owned components that aren't in PreviewRailMeshSegments
			TArray<USplineMeshComponent*> OwnedSplineMeshComponents;
			GetComponents(OwnedSplineMeshComponents);
			for (auto OwnedComponent : OwnedSplineMeshComponents)
			{
				if (!PreviewRailMeshSegments.Contains(OwnedComponent) && OwnedComponent->IsRegistered())
				{
					OwnedComponent->UnregisterComponent();
				}
			}
		}

		// make visualization of the mount follow the contour of the rail
		if (PreviewMesh_Mount)
		{
			float const SplineLen = RailSplineComponent->GetSplineLength();
			FQuat const RailRot = RailSplineComponent->GetQuaternionAtDistanceAlongSpline(CurrentPositionOnRail*SplineLen, ESplineCoordinateSpace::World);
			PreviewMesh_Mount->SetWorldRotation(RailRot);
		}
	}
}
#endif

void ACameraRig_Rail::UpdateRailComponents()
{
	if (RailSplineComponent && RailCameraMount)
	{
		float const SplineLen = RailSplineComponent->GetSplineLength();
		FVector const MountPos = RailSplineComponent->GetLocationAtDistanceAlongSpline(CurrentPositionOnRail*SplineLen, ESplineCoordinateSpace::World);
		RailCameraMount->SetWorldLocation(MountPos);
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// set up preview mesh to match #todo
		UpdatePreviewMeshes();
	}
#endif
}

USceneComponent* ACameraRig_Rail::GetDefaultAttachComponent() const
{
	return RailCameraMount;
}

#if WITH_EDITOR
void ACameraRig_Rail::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateRailComponents();
}

void ACameraRig_Rail::PostEditUndo()
{
	Super::PostEditUndo();
	UpdateRailComponents();
}
#endif

void ACameraRig_Rail::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// feed exposed API into underlying components
	UpdateRailComponents();
}

bool ACameraRig_Rail::ShouldTickIfViewportsOnly() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
