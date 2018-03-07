// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheThumbnailRenderer.h"
#include "Misc/App.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "GeometryCacheThumbnailScene.h"
#include "RendererInterface.h"
#include "EngineModule.h"
#include "GeometryCache.h"

UGeometryCacheThumbnailRenderer::UGeometryCacheThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ThumbnailScene = nullptr;
}

void UGeometryCacheThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UGeometryCache* GeometryCache = Cast<UGeometryCache>(Object);
	if (GeometryCache != nullptr && !GeometryCache->IsPendingKill())
	{
		if (ThumbnailScene == nullptr)
		{
			ThumbnailScene = new FGeometryCacheThumbnailScene();
		}

		ThumbnailScene->SetGeometryCache(GeometryCache);
		ThumbnailScene->GetScene()->UpdateSpeedTreeWind(0.0);

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
			.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.LOD = 0;

		ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
		GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);
		ThumbnailScene->SetGeometryCache(nullptr);
	}
}

void UGeometryCacheThumbnailRenderer::BeginDestroy()
{
	if (ThumbnailScene != nullptr)
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}
