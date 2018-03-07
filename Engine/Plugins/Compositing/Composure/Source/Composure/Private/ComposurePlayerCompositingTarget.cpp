// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComposurePlayerCompositingTarget.h"
#include "ComposurePlayerCompositingCameraModifier.h"

#include "Classes/Camera/PlayerCameraManager.h"
#include "Classes/GameFramework/PlayerController.h"
#include "Classes/Engine/TextureRenderTarget2D.h"
#include "Classes/Engine/LocalPlayer.h"
#include "Classes/Materials/MaterialInstanceDynamic.h"
#include "Public/SceneView.h"

#include "ComposureUtils.h"
#include "ComposureInternals.h"


UComposurePlayerCompositingCameraModifier::UComposurePlayerCompositingCameraModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Target(nullptr)
{ }

bool UComposurePlayerCompositingCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	check(Target);

	// Adds itself to have programmatic control of FSceneView::FinalPostProcessSettings
	// in  UComposurePlayerCompositingTarget::OverrideBlendableSettings().
	InOutPOV.PostProcessSettings.AddBlendable(this, /* Weight = */ 1.0f);
	return true;
}

void UComposurePlayerCompositingCameraModifier::OverrideBlendableSettings(FSceneView& View, float Weight) const
{
	check(Target);

	// Forward call to the  UComposurePlayerCompositingTarget.
	Target->OverrideBlendableSettings(View, Weight);
}



UComposurePlayerCompositingTarget::UComposurePlayerCompositingTarget( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
	, PlayerCameraManager(nullptr)
	, PlayerCameraModifier(nullptr)
	, EngineShowFlagsBackup(ESFIM_Game)
{
	COMPOSURE_CREATE_DYMAMIC_MATERIAL(Material, ReplaceTonemapperMID, "ReplaceTonemapper/", "ComposureReplaceTonemapperByTexture");
}

UComposurePlayerCompositingTarget::~UComposurePlayerCompositingTarget()
{
	check(!PlayerCameraManager);
}

APlayerCameraManager* UComposurePlayerCompositingTarget::SetPlayerCameraManager(APlayerCameraManager* InPlayerCameraManager)
{
	if (InPlayerCameraManager == PlayerCameraManager)
	{
		return InPlayerCameraManager;
	}
	else if (PlayerCameraManager)
	{
		// Remove the camera modifier from the camera manager.
		check(PlayerCameraModifier);
		PlayerCameraManager->RemoveCameraModifier(PlayerCameraModifier);
		PlayerCameraModifier = nullptr;

		// Resume rendering any components.
		PlayerCameraManager->PCOwner->bRenderPrimitiveComponents = true;

		// Restore local player's showflags.
		UGameViewportClient* ViewportClient = PlayerCameraManager->PCOwner->GetLocalPlayer()->ViewportClient;
		if (ViewportClient)
		{
			ViewportClient->EngineShowFlags = EngineShowFlagsBackup;
		}
	}

	PlayerCameraManager = InPlayerCameraManager;

	if (PlayerCameraManager)
	{
		// Stop rendering any component.
		check(InPlayerCameraManager->PCOwner);
		InPlayerCameraManager->PCOwner->bRenderPrimitiveComponents = false;

		// Setup camera modifier to the camera manager.
		check(!PlayerCameraModifier);
		PlayerCameraModifier = Cast<UComposurePlayerCompositingCameraModifier>(
			PlayerCameraManager->AddNewCameraModifier(UComposurePlayerCompositingCameraModifier::StaticClass()));
		PlayerCameraModifier->Target = this;

		// Setup local player's showflags.
		FEngineShowFlags& EngineShowFlags = PlayerCameraManager->PCOwner->GetLocalPlayer()->ViewportClient->EngineShowFlags;
		EngineShowFlagsBackup = EngineShowFlags;
		FComposureUtils::SetEngineShowFlagsForPostprocessingOnly(EngineShowFlags);
	}

	return InPlayerCameraManager;
}

void UComposurePlayerCompositingTarget::SetRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
	ReplaceTonemapperMID->SetTextureParameterValue(TEXT("Input"), RenderTarget);
}

void UComposurePlayerCompositingTarget::FinishDestroy()
{
	SetPlayerCameraManager(nullptr);

	Super::FinishDestroy();
}

void UComposurePlayerCompositingTarget::OverrideBlendableSettings(class FSceneView& View, float Weight) const
{
	// Clear any blendables that could have been set by post process volumes.
	View.FinalPostProcessSettings.BlendableManager = FBlendableManager();

	// Setup the post process material that dump the render target.
	ReplaceTonemapperMID->OverrideBlendableSettings(View, Weight);
}
