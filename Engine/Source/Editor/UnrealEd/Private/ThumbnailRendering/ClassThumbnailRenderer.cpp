// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/ClassThumbnailRenderer.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "Misc/App.h"

// FPreviewScene derived helpers for rendering
#include "RendererInterface.h"
#include "EngineModule.h"

UClassThumbnailRenderer::UClassThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UClassThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	UClass* Class = Cast<UClass>(Object);

	// Only visualize actor based classes
	if (Class && Class->IsChildOf(AActor::StaticClass()))
	{
		// Try to find any visible primitive components in the class' CDO
		AActor* CDO = Class->GetDefaultObject<AActor>();

		TInlineComponentArray<UActorComponent*> Components;
		CDO->GetComponents(Components);

		for (auto CompIt = Components.CreateConstIterator(); CompIt; ++CompIt)
		{
			if (FClassThumbnailScene::IsValidComponentForVisualization(*CompIt))
			{
				return true;
			}
		}
	}

	return false;
}

void UClassThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UClass* Class = Cast<UClass>(Object);
	if (Class != nullptr)
	{
		TSharedRef<FClassThumbnailScene> ThumbnailScene = ThumbnailScenes.EnsureThumbnailScene(Class);

		ThumbnailScene->SetClass(Class);
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game) )
			.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;

		ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
		GetRendererModule().BeginRenderingViewFamily(Canvas,&ViewFamily);
	}
}

void UClassThumbnailRenderer::BeginDestroy()
{
	ThumbnailScenes.Clear();

	Super::BeginDestroy();
}
