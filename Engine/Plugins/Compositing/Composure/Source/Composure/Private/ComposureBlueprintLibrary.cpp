// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComposureBlueprintLibrary.h"

#include "UObject/Package.h"
#include "Public/Slate/SceneViewport.h"
#include "Classes/Components/SceneCaptureComponent2D.h"
#include "Classes/Camera/PlayerCameraManager.h"
#include "Classes/GameFramework/PlayerController.h"
#include "Classes/Engine/LocalPlayer.h"

#include "ComposurePlayerCompositingTarget.h"
#include "ComposureUtils.h"



UComposureBlueprintLibrary::UComposureBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }


// static
UComposurePlayerCompositingTarget* UComposureBlueprintLibrary::CreatePlayerCompositingTarget(UObject* WorldContextObject)
{
	UObject* Outer = WorldContextObject ? WorldContextObject : GetTransientPackage();
	return NewObject< UComposurePlayerCompositingTarget>(Outer);
}

// static
void UComposureBlueprintLibrary::GetProjectionMatrixFromPostMoveSettings(
	const FComposurePostMoveSettings& PostMoveSettings, float HorizontalFOVAngle, float AspectRatio, FMatrix& ProjectionMatrix)
{
	ProjectionMatrix = PostMoveSettings.GetProjectionMatrix(HorizontalFOVAngle, AspectRatio);
}

// static
void UComposureBlueprintLibrary::GetCroppingUVTransformationMatrixFromPostMoveSettings(
	const FComposurePostMoveSettings& PostMoveSettings, float AspectRatio,
	FMatrix& CropingUVTransformationMatrix, FMatrix& UncropingUVTransformationMatrix)
{
	PostMoveSettings.GetCroppingUVTransformationMatrix(AspectRatio, &CropingUVTransformationMatrix, &UncropingUVTransformationMatrix);
}

// static
void UComposureBlueprintLibrary::GetRedGreenUVFactorsFromChromaticAberration(
	float ChromaticAberrationAmount, FVector2D& RedGreenUVFactors)
{
	RedGreenUVFactors = FComposureUtils::GetRedGreenUVFactorsFromChromaticAberration(
		FMath::Clamp(ChromaticAberrationAmount, 0.f, 1.f));
}

//static
void UComposureBlueprintLibrary::GetPlayerDisplayGamma(const APlayerCameraManager* PlayerCameraManager, float& DisplayGamma)
{
	DisplayGamma = 0;
	if (!PlayerCameraManager)
	{
		return;
	}

	UGameViewportClient* ViewportClient = PlayerCameraManager->PCOwner->GetLocalPlayer()->ViewportClient;
	if (!ViewportClient)
	{
		return;
	}

	FSceneViewport* SceneViewport = ViewportClient->GetGameViewport();

	DisplayGamma = SceneViewport ? SceneViewport->GetDisplayGamma() : 0.0;
}
