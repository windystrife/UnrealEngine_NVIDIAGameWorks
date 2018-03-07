// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SpringComponentVisualizer.h"
#include "SceneManagement.h"

#include "PhysicsEngine/PhysicsSpringComponent.h"

static const FColor	DisabledColor(128, 128, 128);
static const FColor	CompressedColor(255, 0, 0);
static const FColor RestColor(0, 255, 0);

void FSpringComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if (const UPhysicsSpringComponent* SpringComp = Cast<const UPhysicsSpringComponent>(Component))
	{
		//interpolate color by compression
		const float CompressionNormalized = SpringComp->GetNormalizedCompressionScalar();
		float R = FMath::Lerp(RestColor.R, CompressedColor.R, CompressionNormalized);
		float G = FMath::Lerp(RestColor.G, CompressedColor.G, CompressionNormalized);
		float B = FMath::Lerp(RestColor.B, CompressedColor.B, CompressionNormalized);
		const FColor CurrentColor = SpringComp->bIsActive ? FColor(R, G, B) : DisabledColor;
		
		//draw capsule for spring sweep
		const FTransform& WorldTM = SpringComp->GetComponentToWorld();
		const FVector SpringStart = WorldTM.GetLocation();
		const FVector SpringEnd = SpringComp->GetSpringCurrentEndPoint();
		const float HalfHeight = (SpringEnd - SpringStart).Size() * 0.5f + SpringComp->SpringRadius;
		DrawWireCapsule(PDI, FMath::Lerp(SpringStart, SpringEnd, 0.5f), WorldTM.GetUnitAxis(EAxis::Z), WorldTM.GetUnitAxis(EAxis::Y), WorldTM.GetUnitAxis(EAxis::X), CurrentColor, SpringComp->SpringRadius, HalfHeight, 25, SDPG_World);
	}
}
