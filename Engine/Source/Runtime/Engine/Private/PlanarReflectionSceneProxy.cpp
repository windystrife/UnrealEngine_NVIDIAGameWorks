// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PlanarReflectionProxy.cpp
=============================================================================*/

#include "PlanarReflectionSceneProxy.h"
#include "GameFramework/Actor.h"
#include "Components/PlanarReflectionComponent.h"

FPlanarReflectionSceneProxy::FPlanarReflectionSceneProxy(UPlanarReflectionComponent* Component, FPlanarReflectionRenderTarget* InRenderTarget) :
	bIsStereo(false)
{
	RenderTarget = InRenderTarget;

	float ClampedFadeStart = FMath::Max(Component->DistanceFromPlaneFadeoutStart, 0.0f);
	float ClampedFadeEnd = FMath::Max(Component->DistanceFromPlaneFadeoutEnd, 0.0f);

	DistanceFromPlaneFadeEnd = ClampedFadeEnd;

	float DistanceFadeScale = 1.0f / FMath::Max(ClampedFadeEnd - ClampedFadeStart, DELTA);

	PlanarReflectionParameters = FVector(
		DistanceFadeScale,
		-ClampedFadeStart * DistanceFadeScale,
		Component->NormalDistortionStrength);

	const float CosFadeStart = FMath::Cos(FMath::Clamp(Component->AngleFromPlaneFadeStart, 0.1f, 89.9f) * (float)PI / 180.0f);
	const float CosFadeEnd = FMath::Cos(FMath::Clamp(Component->AngleFromPlaneFadeEnd, 0.1f, 89.9f) * (float)PI / 180.0f);
	const float Range = 1.0f / FMath::Max(CosFadeStart - CosFadeEnd, DELTA);

	PlanarReflectionParameters2 = FVector2D(
		Range,
		-CosFadeEnd * Range);

// WaveWorks Start
	PlanarReflectionWaveWorksParameters = FVector4(
		Component->WaterTransmittance.X, 
		Component->WaterTransmittance.Y, 
		Component->WaterTransmittance.Z, 
		Component->WaterTransmittancePower);
// WaveWorks End

	Component->GetProjectionWithExtraFOV(ProjectionWithExtraFOV[0], 0);
	Component->GetProjectionWithExtraFOV(ProjectionWithExtraFOV[1], 1);

	ScreenScaleBias[0] = Component->GetScreenScaleBias(0);
	ScreenScaleBias[1] = Component->GetScreenScaleBias(1);

	OwnerName = Component->GetOwner() ? Component->GetOwner()->GetFName() : NAME_None;

	UpdateTransform(Component->GetComponentTransform().ToMatrixWithScale());

	PlanarReflectionId = Component->GetPlanarReflectionId();
	PrefilterRoughness = Component->PrefilterRoughness;
	PrefilterRoughnessDistance = Component->PrefilterRoughnessDistance;

// WaveWorks Start
	bAlwaysVisible = Component->bAlwaysVisible;
// WaveWorks End
}
