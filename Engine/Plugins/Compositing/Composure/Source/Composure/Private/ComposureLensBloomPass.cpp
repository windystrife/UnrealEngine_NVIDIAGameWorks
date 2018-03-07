// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComposureLensBloomPass.h"
#include "ComposurePostProcessBlendable.h"

#include "Classes/Components/SceneCaptureComponent2D.h"
#include "Classes/Materials/Material.h"
#include "Classes/Materials/MaterialInstanceDynamic.h"
#include "ComposureInternals.h"
#include "ComposureUtils.h"


UComposureLensBloomPass::UComposureLensBloomPass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Replace the tonemapper with a post process material that outputs bloom only.
	COMPOSURE_CREATE_DYMAMIC_MATERIAL(Material, TonemapperReplacingMID, "ReplaceTonemapper/", "ComposureReplaceTonemapperComposeBloom");
}

void UComposureLensBloomPass::SetTonemapperReplacingMaterial(UMaterialInstanceDynamic* Material)
{
	UMaterial* Base = Material->GetBaseMaterial();

	if (Base->MaterialDomain == MD_PostProcess &&
		Base->BlendableLocation == BL_ReplacingTonemapper)
	{
		TonemapperReplacingMID = Material;
	}
}

void UComposureLensBloomPass::BloomToRenderTarget()
{
	// Exports the settings to the scene capture's post process settings.
	Settings.ExportToPostProcessSettings(&SceneCapture->PostProcessSettings);

	// The tonemapper is supposed to take care of the bloom intensity.
	TonemapperReplacingMID->SetScalarParameterValue(TEXT("BloomIntensity"), SceneCapture->PostProcessSettings.BloomIntensity);

	// Disables as much stuf as possible using showflags. 
	FComposureUtils::SetEngineShowFlagsForPostprocessingOnly(SceneCapture->ShowFlags);

	// Enables bloom.
	SceneCapture->ShowFlags.Bloom = true;

	// Adds the blendable to have programmatic control of FSceneView::FinalPostProcessSettings
	// in  UComposurePostProcessPass::OverrideBlendableSettings().
	SceneCapture->PostProcessSettings.AddBlendable(BlendableInterface, 1.0);

	// Update the render target output.
	SceneCapture->CaptureScene();
}
