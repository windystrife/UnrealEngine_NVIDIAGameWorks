// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComposureTonemapperPass.h"
#include "ComposurePostProcessBlendable.h"

#include "Classes/Components/SceneCaptureComponent2D.h"
#include "Classes/Materials/MaterialInstanceDynamic.h"
#include "ComposureUtils.h"


UComposureTonemapperPass::UComposureTonemapperPass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Do not replace the engine's tonemapper.
	TonemapperReplacingMID = nullptr;

	ChromaticAberration = 0.0;
}

void UComposureTonemapperPass::TonemapToRenderTarget()
{
	// Exports the settings to the scene capture's post process settings.
	ColorGradingSettings.ExportToPostProcessSettings(&SceneCapture->PostProcessSettings);
	FilmStockSettings.ExportToPostProcessSettings(&SceneCapture->PostProcessSettings);

	// Disables as much stuf as possible using showflags. 
	FComposureUtils::SetEngineShowFlagsForPostprocessingOnly(SceneCapture->ShowFlags);

	// Override some tone mapper non exposed settings to not have post process material changing them.
	{
		SceneCapture->PostProcessSettings.bOverride_SceneColorTint = true;
		SceneCapture->PostProcessSettings.SceneColorTint = FLinearColor::White;

		SceneCapture->PostProcessSettings.bOverride_VignetteIntensity = true;
		SceneCapture->PostProcessSettings.VignetteIntensity = 0;

		SceneCapture->PostProcessSettings.bOverride_GrainIntensity = true;
		SceneCapture->PostProcessSettings.GrainIntensity = 0;

		SceneCapture->PostProcessSettings.bOverride_BloomDirtMask = true;
		SceneCapture->PostProcessSettings.BloomDirtMask = nullptr;
		SceneCapture->PostProcessSettings.bOverride_BloomDirtMaskIntensity = true;
		SceneCapture->PostProcessSettings.BloomDirtMaskIntensity = 0;
	}

	//SceneCapture->ShowFlags.EyeAdaptation = true;

	SceneCapture->PostProcessSettings.bOverride_SceneFringeIntensity = true;
	SceneCapture->PostProcessSettings.SceneFringeIntensity = ChromaticAberration;

	// Adds the blendable to have programmatic control of FSceneView::FinalPostProcessSettings
	// in  UComposurePostProcessPass::OverrideBlendableSettings().
	SceneCapture->PostProcessSettings.AddBlendable(BlendableInterface, 1.0);

	// Update the render target output.
	SceneCapture->CaptureScene();
}
