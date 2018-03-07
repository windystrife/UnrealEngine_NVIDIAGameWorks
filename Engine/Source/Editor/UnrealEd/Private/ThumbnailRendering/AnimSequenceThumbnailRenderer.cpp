// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/AnimSequenceThumbnailRenderer.h"
#include "Misc/App.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "Animation/AnimSequenceBase.h"
#include "ThumbnailHelpers.h"

// FPreviewScene derived helpers for rendering
#include "RendererInterface.h"
#include "EngineModule.h"

UAnimSequenceThumbnailRenderer::UAnimSequenceThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ThumbnailScene = nullptr;
}

void UAnimSequenceThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(Object);
	if (Anim != nullptr)
	{
		if ( ThumbnailScene == nullptr )
		{
			ThumbnailScene = new FAnimationSequenceThumbnailScene();
		}

		if(ThumbnailScene->SetAnimation(Anim))
		{
			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
				.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

			ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
			ViewFamily.EngineShowFlags.MotionBlur = 0;
			ViewFamily.EngineShowFlags.LOD = 0;

			ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
			GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);
			ThumbnailScene->SetAnimation(nullptr);
		}
	}
}

void UAnimSequenceThumbnailRenderer::BeginDestroy()
{
	if ( ThumbnailScene != nullptr )
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}
