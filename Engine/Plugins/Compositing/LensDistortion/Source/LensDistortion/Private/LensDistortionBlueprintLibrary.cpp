// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LensDistortionBlueprintLibrary.h"



ULensDistortionBlueprintLibrary::ULensDistortionBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }


// static
void ULensDistortionBlueprintLibrary::GetUndistortOverscanFactor(
	const FLensDistortionCameraModel& CameraModel,
	float DistortedHorizontalFOV,
	float DistortedAspectRatio,
	float& UndistortOverscanFactor)
{
	UndistortOverscanFactor = CameraModel.GetUndistortOverscanFactor(DistortedHorizontalFOV, DistortedAspectRatio);
}


// static
void ULensDistortionBlueprintLibrary::DrawUVDisplacementToRenderTarget(
	const UObject* WorldContextObject,
	const FLensDistortionCameraModel& CameraModel,
	float DistortedHorizontalFOV,
	float DistortedAspectRatio,
	float UndistortOverscanFactor,
	class UTextureRenderTarget2D* OutputRenderTarget,
	float OutputMultiply,
	float OutputAdd)
{
	CameraModel.DrawUVDisplacementToRenderTarget(
		WorldContextObject->GetWorld(),
		DistortedHorizontalFOV, DistortedAspectRatio,
		UndistortOverscanFactor, OutputRenderTarget,
		OutputMultiply, OutputAdd);
}
