// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CineCameraComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"

#define LOCTEXT_NAMESPACE "CineCameraComponent"


//////////////////////////////////////////////////////////////////////////
// UCameraComponent

/// @cond DOXYGEN_WARNINGS

UCineCameraComponent::UCineCameraComponent()
{
	// Super 35mm 4 Perf
	// These will be overridden if valid default presets are specified in ini
	FilmbackSettings.SensorWidth = 24.89f;
	FilmbackSettings.SensorHeight = 18.67;
	LensSettings.MinFocalLength = 50.f;
	LensSettings.MaxFocalLength = 50.f;
	LensSettings.MinFStop = 2.f;
	LensSettings.MaxFStop = 2.f;
	LensSettings.MinimumFocusDistance = 15.f;

#if WITH_EDITORONLY_DATA
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;
#endif

	bConstrainAspectRatio = true;

	// Default to CircleDOF, but allow the user to customize it
	PostProcessSettings.DepthOfFieldMethod = DOFM_CircleDOF;

	RecalcDerivedData();

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		// overrides CameraComponent's camera mesh
		static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(TEXT("/Engine/EditorMeshes/Camera/SM_CineCam.SM_CineCam"));
		CameraMesh = EditorCameraMesh.Object;

		static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/ArtTools/RenderToTexture/Meshes/S_1_Unit_Plane.S_1_Unit_Plane"));
		DebugFocusPlaneMesh = PlaneMesh.Object;

		static ConstructorHelpers::FObjectFinder<UMaterial> PlaneMat(TEXT("/Engine/EngineDebugMaterials/M_SimpleTranslucent.M_SimpleTranslucent"));
		DebugFocusPlaneMaterial = PlaneMat.Object;
	}
#endif
}

void UCineCameraComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// default filmback
	FNamedFilmbackPreset* const DefaultFilmbackPreset = FilmbackPresets.FindByPredicate(
		[&](FNamedFilmbackPreset const& Preset)
		{
			return (Preset.Name == DefaultFilmbackPresetName);
		}
	);
	if (DefaultFilmbackPreset)
	{
		FilmbackSettings = DefaultFilmbackPreset->FilmbackSettings;
	}

	// default lens
	FNamedLensPreset* const DefaultLensPreset = LensPresets.FindByPredicate(
		[&](FNamedLensPreset const& Preset)
		{
			return (Preset.Name == DefaultLensPresetName);
		}
	);
	if (DefaultLensPreset)
	{
		LensSettings = DefaultLensPreset->LensSettings;
	}

	// other lens defaults
	CurrentAperture = DefaultLensFStop;
	CurrentFocalLength = DefaultLensFocalLength;

	RecalcDerivedData();
}

void UCineCameraComponent::PostLoad()
{
	RecalcDerivedData();
	bResetInterpolation = true;
	Super::PostLoad();
}

void UCineCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
#if WITH_EDITORONLY_DATA
	UpdateDebugFocusPlane();
#endif
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

#if WITH_EDITORONLY_DATA

void UCineCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	RecalcDerivedData();

	// handle debug focus plane
	if (FocusSettings.bDrawDebugFocusPlane && (DebugFocusPlaneComponent == nullptr))
	{
		CreateDebugFocusPlane();
	}
	else if ((FocusSettings.bDrawDebugFocusPlane == false) && (DebugFocusPlaneComponent != nullptr))
	{
		DestroyDebugFocusPlane();
	}

	// set focus plane color in case that's what changed
	if (DebugFocusPlaneMID)
	{
		DebugFocusPlaneMID->SetVectorParameterValue(FName(TEXT("Color")), FocusSettings.DebugFocusPlaneColor.ReinterpretAsLinear());
	}

	// reset interpolation if the user changes anything
	bResetInterpolation = true;

	UpdateDebugFocusPlane();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UCineCameraComponent::ResetProxyMeshTransform()
{
	if (ProxyMeshComponent)
	{
		// CineCam mesh is offset 90deg yaw
		ProxyMeshComponent->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		ProxyMeshComponent->SetRelativeLocation(FVector(-46.f, 0, -24.f));
	}
}

#endif	// WITH_EDITORONLY_DATA

float UCineCameraComponent::GetHorizontalFieldOfView() const
{
	return (CurrentFocalLength > 0.f)
		? FMath::RadiansToDegrees(2.f * FMath::Atan(FilmbackSettings.SensorWidth / (2.f * CurrentFocalLength)))
		: 0.f;
}

float UCineCameraComponent::GetVerticalFieldOfView() const
{
	return (CurrentFocalLength > 0.f)
		? FMath::RadiansToDegrees(2.f * FMath::Atan(FilmbackSettings.SensorHeight / (2.f * CurrentFocalLength)))
		: 0.f;
}

FString UCineCameraComponent::GetFilmbackPresetName() const
{
	TArray<FNamedFilmbackPreset> const& Presets = UCineCameraComponent::GetFilmbackPresets();
	int32 const NumPresets = Presets.Num();
	for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
	{
		FNamedFilmbackPreset const& P = Presets[PresetIdx];
		if (P.FilmbackSettings == FilmbackSettings)
		{
			return P.Name;
		}
	}

	return FString();
}

void UCineCameraComponent::SetFilmbackPresetByName(const FString& InPresetName)
{
	TArray<FNamedFilmbackPreset> const& Presets = UCineCameraComponent::GetFilmbackPresets();
	int32 const NumPresets = Presets.Num();
	for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
	{
		FNamedFilmbackPreset const& P = Presets[PresetIdx];
		if (P.Name == InPresetName)
		{
			FilmbackSettings = P.FilmbackSettings;
		}
	}
}

FString UCineCameraComponent::GetLensPresetName() const
{
	TArray<FNamedLensPreset> const& Presets = UCineCameraComponent::GetLensPresets();
	int32 const NumPresets = Presets.Num();
	for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
	{
		FNamedLensPreset const& P = Presets[PresetIdx];
		if (P.LensSettings == LensSettings)
		{
			return P.Name;
		}
	}

	return FString();
}

void UCineCameraComponent::SetLensPresetByName(const FString& InPresetName)
{
	TArray<FNamedLensPreset> const& Presets = UCineCameraComponent::GetLensPresets();
	int32 const NumPresets = Presets.Num();
	for (int32 PresetIdx = 0; PresetIdx < NumPresets; ++PresetIdx)
	{
		FNamedLensPreset const& P = Presets[PresetIdx];
		if (P.Name == InPresetName)
		{
			LensSettings = P.LensSettings;
		}
	}
}

float UCineCameraComponent::GetWorldToMetersScale() const
{
	UWorld const* const World = GetWorld();
	AWorldSettings const* const WorldSettings = World ? World->GetWorldSettings() : nullptr;
	return WorldSettings ? WorldSettings->WorldToMeters : 100.f;
}

// static
TArray<FNamedFilmbackPreset> const& UCineCameraComponent::GetFilmbackPresets()
{
	return GetDefault<UCineCameraComponent>()->FilmbackPresets;
}

// static
TArray<FNamedLensPreset> const& UCineCameraComponent::GetLensPresets()
{
	return GetDefault<UCineCameraComponent>()->LensPresets;
}

void UCineCameraComponent::RecalcDerivedData()
{
	// respect physical limits of the (simulated) hardware
	CurrentFocalLength = FMath::Clamp(CurrentFocalLength, LensSettings.MinFocalLength, LensSettings.MaxFocalLength);
	CurrentAperture = FMath::Clamp(CurrentAperture, LensSettings.MinFStop, LensSettings.MaxFStop);

	float const MinFocusDistInWorldUnits = LensSettings.MinimumFocusDistance * (GetWorldToMetersScale() / 1000.f);	// convert mm to uu
	FocusSettings.ManualFocusDistance = FMath::Max(FocusSettings.ManualFocusDistance, MinFocusDistInWorldUnits);

	FieldOfView = GetHorizontalFieldOfView();
	FilmbackSettings.SensorAspectRatio = (FilmbackSettings.SensorHeight > 0.f) ? (FilmbackSettings.SensorWidth / FilmbackSettings.SensorHeight) : 0.f;
	AspectRatio = FilmbackSettings.SensorAspectRatio;

#if WITH_EDITORONLY_DATA
	CurrentHorizontalFOV = FieldOfView;			// informational variable only, for editor users
#endif
}

/// @endcond

float UCineCameraComponent::GetDesiredFocusDistance(const FVector& InLocation) const
{
	float DesiredFocusDistance = 0.f;

	// get focus distance
	switch (FocusSettings.FocusMethod)
	{
	case ECameraFocusMethod::Manual:
		DesiredFocusDistance = FocusSettings.ManualFocusDistance;
		break;

	case ECameraFocusMethod::Tracking:
		{
			AActor const* const TrackedActor = FocusSettings.TrackingFocusSettings.ActorToTrack;

			FVector FocusPoint;
			if (TrackedActor)
			{
				FTransform const BaseTransform = TrackedActor->GetActorTransform();
				FocusPoint = BaseTransform.TransformPosition(FocusSettings.TrackingFocusSettings.RelativeOffset);
			}
			else
			{
				FocusPoint = FocusSettings.TrackingFocusSettings.RelativeOffset;
			}

			DesiredFocusDistance = (FocusPoint - InLocation).Size();
		}
		break;
	}
	
	// add in the adjustment offset
	DesiredFocusDistance += FocusSettings.FocusOffset;

	return DesiredFocusDistance;
}

void UCineCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	RecalcDerivedData();

	Super::GetCameraView(DeltaTime, DesiredView);

	UpdateCameraLens(DeltaTime, DesiredView);
}

void UCineCameraComponent::UpdateDebugFocusPlane()
{
#if WITH_EDITORONLY_DATA
	if (FocusSettings.bDrawDebugFocusPlane && DebugFocusPlaneMesh && DebugFocusPlaneComponent)
	{
		FVector const CamLocation = GetComponentTransform().GetLocation();
		FVector const CamDir = GetComponentTransform().GetRotation().Vector();
		FVector const FocusPoint = GetComponentTransform().GetLocation() + CamDir * GetDesiredFocusDistance(CamLocation);
		DebugFocusPlaneComponent->SetWorldLocation(FocusPoint);
	}
#endif
}


void UCineCameraComponent::UpdateCameraLens(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	if (FocusSettings.FocusMethod == ECameraFocusMethod::None)
	{
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldMethod = false;
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldFstop = false;
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldFocalDistance = false;
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldSensorWidth = false;
	}
	else
	{
		// Update focus/DoF
		DesiredView.PostProcessBlendWeight = 1.f;
		DesiredView.PostProcessSettings.bOverride_DepthOfFieldMethod = true;
		DesiredView.PostProcessSettings.DepthOfFieldMethod = PostProcessSettings.DepthOfFieldMethod;

		DesiredView.PostProcessSettings.bOverride_DepthOfFieldFstop = true;
		DesiredView.PostProcessSettings.DepthOfFieldFstop = CurrentAperture;

		CurrentFocusDistance = GetDesiredFocusDistance(DesiredView.Location);

		// clamp to min focus distance
		float const MinFocusDistInWorldUnits = LensSettings.MinimumFocusDistance * (GetWorldToMetersScale() / 1000.f);	// convert mm to uu
		CurrentFocusDistance = FMath::Max(CurrentFocusDistance, MinFocusDistInWorldUnits);

		// smoothing, if desired
		if (FocusSettings.bSmoothFocusChanges)
		{
			if (bResetInterpolation == false)
			{
				CurrentFocusDistance = FMath::FInterpTo(LastFocusDistance, CurrentFocusDistance, DeltaTime, FocusSettings.FocusSmoothingInterpSpeed);
			}
		}
		LastFocusDistance = CurrentFocusDistance;

		DesiredView.PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
		DesiredView.PostProcessSettings.DepthOfFieldFocalDistance = CurrentFocusDistance;

		DesiredView.PostProcessSettings.bOverride_DepthOfFieldSensorWidth = true;
		DesiredView.PostProcessSettings.DepthOfFieldSensorWidth = FilmbackSettings.SensorWidth;
	}

	bResetInterpolation = false;
}

void UCineCameraComponent::NotifyCameraCut()
{
	Super::NotifyCameraCut();

	// reset any interpolations
	bResetInterpolation = true;
}

#if WITH_EDITORONLY_DATA
void UCineCameraComponent::CreateDebugFocusPlane()
{
	if (AActor* const MyOwner = GetOwner())
	{
		if (DebugFocusPlaneComponent == nullptr)
		{
			DebugFocusPlaneComponent = NewObject<UStaticMeshComponent>(MyOwner, NAME_None, RF_Transactional | RF_TextExportTransient);
			DebugFocusPlaneComponent->SetupAttachment(this);
			DebugFocusPlaneComponent->bIsEditorOnly = true;
			DebugFocusPlaneComponent->SetStaticMesh(DebugFocusPlaneMesh);
			DebugFocusPlaneComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			DebugFocusPlaneComponent->bHiddenInGame = false;
			DebugFocusPlaneComponent->CastShadow = false;
			DebugFocusPlaneComponent->PostPhysicsComponentTick.bCanEverTick = false;
			DebugFocusPlaneComponent->CreationMethod = CreationMethod;
			DebugFocusPlaneComponent->bSelectable = false;

			DebugFocusPlaneComponent->RelativeScale3D = FVector(10000.f, 10000.f, 1.f);
			DebugFocusPlaneComponent->RelativeRotation = FRotator(90.f, 0.f, 0.f);

			DebugFocusPlaneComponent->RegisterComponentWithWorld(GetWorld());

			DebugFocusPlaneMID = DebugFocusPlaneComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, DebugFocusPlaneMaterial);
			if (DebugFocusPlaneMID)
			{
				DebugFocusPlaneMID->SetVectorParameterValue(FName(TEXT("Color")), FocusSettings.DebugFocusPlaneColor.ReinterpretAsLinear());
			}
		}
	}
}

void UCineCameraComponent::DestroyDebugFocusPlane()
{
	if (DebugFocusPlaneComponent)
	{
		DebugFocusPlaneComponent->SetVisibility(false);
		DebugFocusPlaneComponent = nullptr;

		DebugFocusPlaneMID = nullptr;
	}
}
#endif

void UCineCameraComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	ResetProxyMeshTransform();
#endif
}

#if WITH_EDITOR
void UCineCameraComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	if (DebugFocusPlaneComponent)
	{
		DebugFocusPlaneComponent->DestroyComponent();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
