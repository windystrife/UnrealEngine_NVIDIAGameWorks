// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComposurePostProcessPass.h"
#include "ComposurePostProcessBlendable.h"

#include "Classes/Components/SceneCaptureComponent2D.h"
#include "Classes/Materials/Material.h"
#include "Classes/Materials/MaterialInstanceDynamic.h"
#include "Classes/Materials/MaterialInterface.h"
#include "Public/SceneView.h"
#include "ComposureInternals.h"
#include "ComposureUtils.h"


UComposurePostProcessBlendable::UComposurePostProcessBlendable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Target(nullptr)
{ }

void UComposurePostProcessBlendable::OverrideBlendableSettings(class FSceneView& View, float Weight) const
{
	check(Target);
	Target->OverrideBlendableSettings(View, Weight);
}


UComposurePostProcessPass::UComposurePostProcessPass( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
	, SceneCapture(nullptr)
	, BlendableInterface(nullptr)
{
	bWantsInitializeComponent = true;

	// Sets a default setup material that sets black.
	COMPOSURE_GET_MATERIAL(Material, SetupMaterial, "PassSetup/", "ComposureBeforeTranslucencySetBlack");
	SetSetupMaterial(SetupMaterial);
}

void UComposurePostProcessPass::SetSetupMaterial(UMaterialInterface* Material)
{
	if (!Material)
	{
		UE_LOG(Composure, Error, TEXT("Can't set setup a null material to a UComposurePostProcessPass."));
		return;
	}
	UMaterial* BaseMaterial = Material->GetMaterial();
	if (BaseMaterial->MaterialDomain != MD_PostProcess)
	{
		UE_LOG(Composure, Error,
			TEXT("Can't set setup material %s: is not in the post process domain."),
			*BaseMaterial->GetName());
		return;
	}
	else if (BaseMaterial->BlendableLocation != BL_BeforeTranslucency)
	{
		UE_LOG(Composure, Error,
			TEXT("Can't set setup material %s: is not at the before translucency post process location."),
			*BaseMaterial->GetName());
		return;
	}

	SetupMaterial = Material;
}

UTextureRenderTarget2D* UComposurePostProcessPass::GetOutputRenderTarget() const
{
	return SceneCapture->TextureTarget;
}

void UComposurePostProcessPass::SetOutputRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
	SceneCapture->TextureTarget = RenderTarget;
}

void UComposurePostProcessPass::InitializeComponent()
{
	Super::InitializeComponent();

	SceneCapture = NewObject<USceneCaptureComponent2D>(this, TEXT("SceneCapture"));

	// Create underlying scene capture.
	SceneCapture->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);

	// Avoid drawing any primitive by using the empty show only lists.
	SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

	// Avoid capturing every frame and on movement.
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;

	// Sets the capture source to final color to enable post processing.
	SceneCapture->CaptureSource = SCS_FinalColorLDR;

	// Post process materials, eye adaptation, FFT bloom requires the view state.
	SceneCapture->bAlwaysPersistRenderingState = true;

	// Create private blendable interface.
	BlendableInterface = NewObject<UComposurePostProcessBlendable>(this, TEXT("PostProcessBlendable"));
	BlendableInterface->Target = this;

	SceneCapture->RegisterComponent();
}

void UComposurePostProcessPass::UninitializeComponent()
{
	SceneCapture->UnregisterComponent();
	SceneCapture = nullptr;

	BlendableInterface->Target = nullptr;
	BlendableInterface = nullptr;

	Super::UninitializeComponent();
}

void UComposurePostProcessPass::OverrideBlendableSettings(class FSceneView& View, float Weight) const
{
	// Clear any blendables that could have been set by post process volumes.
	View.FinalPostProcessSettings.BlendableManager = FBlendableManager();

	// Setup the post process materials.
	SetupMaterial->OverrideBlendableSettings(View, Weight);
	if (TonemapperReplacingMID)
	{
		TonemapperReplacingMID->OverrideBlendableSettings(View, Weight);
	}
}
