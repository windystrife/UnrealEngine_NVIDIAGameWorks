// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/AnimBlueprintThumbnailRenderer.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "Misc/App.h"
#include "Animation/AnimBlueprint.h"

// FPreviewScene derived helpers for rendering
#include "RendererInterface.h"
#include "EngineModule.h"

UAnimBlueprintThumbnailRenderer::UAnimBlueprintThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimBlueprintThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Object);
	if (AnimBlueprint && AnimBlueprint->GeneratedClass)
	{
		TSharedRef<FAnimBlueprintThumbnailScene> ThumbnailScene = ThumbnailScenes.EnsureThumbnailScene(AnimBlueprint->GeneratedClass);

		if(ThumbnailScene->SetAnimBlueprint(AnimBlueprint))
		{
			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
				.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

			ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
			ViewFamily.EngineShowFlags.MotionBlur = 0;
			ViewFamily.EngineShowFlags.LOD = 0;

			ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
			GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);
		}
	}
}

void UAnimBlueprintThumbnailRenderer::BeginDestroy()
{
	ThumbnailScenes.Clear();

	Super::BeginDestroy();
}
