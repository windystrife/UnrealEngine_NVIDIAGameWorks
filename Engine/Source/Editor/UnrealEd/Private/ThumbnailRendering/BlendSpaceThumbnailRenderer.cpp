// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/BlendSpaceThumbnailRenderer.h"
#include "Misc/App.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "ThumbnailHelpers.h"

// FPreviewScene derived helpers for rendering
#include "RendererInterface.h"
#include "EngineModule.h"
#include "Animation/BlendSpaceBase.h"

UBlendSpaceThumbnailRenderer::UBlendSpaceThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ThumbnailScene = nullptr;
}

void UBlendSpaceThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Object);
	if (BlendSpace != nullptr)
	{
		if (ThumbnailScene == nullptr)
		{
			ThumbnailScene = new FBlendSpaceThumbnailScene();
		}

		if (ThumbnailScene->SetBlendSpace(BlendSpace))
		{
			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
				.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

			ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
			ViewFamily.EngineShowFlags.MotionBlur = 0;
			ViewFamily.EngineShowFlags.LOD = 0;

			ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
			GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);
			ThumbnailScene->SetBlendSpace(nullptr);
		}
	}
}

void UBlendSpaceThumbnailRenderer::BeginDestroy()
{
	if ( ThumbnailScene != nullptr )
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}
