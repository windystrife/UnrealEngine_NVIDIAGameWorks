// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletalDebugRendering.h"
#include "DrawDebugHelpers.h"
#include "SceneManagement.h"

namespace SkeletalDebugRendering
{

void DrawWireBone(FPrimitiveDrawInterface* PDI, const FVector& InStart, const FVector& InEnd, const FLinearColor& InColor, ESceneDepthPriorityGroup InDepthPriority)
{
#if ENABLE_DRAW_DEBUG
	static const float SphereRadius = 1.0f;
	static const int32 NumSphereSides = 10;
	static const int32 NumConeSides = 4;

	// Calc cone size 
	const FVector EndToStart = (InStart - InEnd);
	const float ConeLength = EndToStart.Size();
	const float Angle = FMath::RadiansToDegrees(FMath::Atan(SphereRadius / ConeLength));

	// Render Sphere for bone end point and a cone between it and its parent.
	DrawWireSphere(PDI, InEnd, InColor, SphereRadius, NumSphereSides, InDepthPriority, 0.0f, 1.0f);

	TArray<FVector> Verts;
	DrawWireCone(PDI, Verts, FRotationMatrix::MakeFromX(EndToStart) * FTranslationMatrix(InEnd), ConeLength, Angle, NumConeSides, InColor, InDepthPriority, 0.0f, 1.0f);
#endif
}

void DrawAxes(FPrimitiveDrawInterface* PDI, const FTransform& Transform, ESceneDepthPriorityGroup InDepthPriority)
{
#if ENABLE_DRAW_DEBUG
	// Display colored coordinate system axes for this joint.
	const float AxisLength = 4.0f;
	const FVector Origin = Transform.GetLocation();

	// Red = X
	FVector XAxis = Transform.TransformVector(FVector(1.0f, 0.0f, 0.0f));
	XAxis.Normalize();
	PDI->DrawLine(Origin, Origin + XAxis * AxisLength, FColor(255, 80, 80), InDepthPriority, 0.0f, 1.0f);

	// Green = Y
	FVector YAxis = Transform.TransformVector(FVector(0.0f, 1.0f, 0.0f));
	YAxis.Normalize();
	PDI->DrawLine(Origin, Origin + YAxis * AxisLength, FColor(80, 255, 80), InDepthPriority, 0.0f, 1.0f); 

	// Blue = Z
	FVector ZAxis = Transform.TransformVector(FVector(0.0f, 0.0f, 1.0f));
	ZAxis.Normalize();
	PDI->DrawLine(Origin, Origin + ZAxis * AxisLength, FColor(80, 80, 255), InDepthPriority, 0.0f, 1.0f);
#endif
}

}