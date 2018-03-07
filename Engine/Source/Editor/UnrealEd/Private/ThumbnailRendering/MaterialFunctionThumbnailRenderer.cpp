// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/MaterialFunctionThumbnailRenderer.h"
#include "Misc/App.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "Materials/MaterialFunction.h"
#include "Materials/Material.h"
#include "ThumbnailHelpers.h"


// FPreviewScene derived helpers for rendering
#include "RendererInterface.h"
#include "EngineModule.h"

UMaterialFunctionThumbnailRenderer::UMaterialFunctionThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ThumbnailScene = nullptr;
}

void UMaterialFunctionThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UMaterialFunction* MatFunc = Cast<UMaterialFunction>(Object);
	if (MatFunc != nullptr)
	{
		if (ThumbnailScene == nullptr || ensure(ThumbnailScene->GetWorld() != nullptr) == false)
		{
			if (ThumbnailScene)
			{
				FlushRenderingCommands();
				delete ThumbnailScene;
			}

			ThumbnailScene = new FMaterialThumbnailScene();
		}

		UMaterial* PreviewMaterial = MatFunc->GetPreviewMaterial();
		if( PreviewMaterial )
		{
			PreviewMaterial->ThumbnailInfo = MatFunc->ThumbnailInfo;
			ThumbnailScene->SetMaterialInterface( PreviewMaterial );
			FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game) )
				.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

			ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
			ViewFamily.EngineShowFlags.MotionBlur = 0;

			ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);

			if (ViewFamily.Views.Num() > 0)
			{
				GetRendererModule().BeginRenderingViewFamily(Canvas,&ViewFamily);
			}

			ThumbnailScene->SetMaterialInterface(nullptr);
		}
	}
}

void UMaterialFunctionThumbnailRenderer::BeginDestroy()
{ 	
	if ( ThumbnailScene != nullptr )
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}
