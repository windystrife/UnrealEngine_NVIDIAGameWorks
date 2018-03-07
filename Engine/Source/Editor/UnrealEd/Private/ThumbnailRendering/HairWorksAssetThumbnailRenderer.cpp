// @third party code - BEGIN HairWorks
#include "Classes/ThumbnailRendering/HairWorksAssetThumbnailRenderer.h"
#include "SceneView.h"
#include "RendererInterface.h"
#include "EngineModule.h"
#include "Engine/HairWorksAsset.h"
#include "ThumbnailHelpers.h"

UHairWorksAssetThumbnailRenderer::UHairWorksAssetThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHairWorksAssetThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UHairWorksAsset* HairAsset = Cast<UHairWorksAsset>(Object);
	if(HairAsset == nullptr || HairAsset->IsPendingKill())
		return;

	if(ThumbnailScene == nullptr)
	{
		ThumbnailScene = new FHairWorksAssetThumbnailScene();
	}

	ThumbnailScene->SetHairAsset(HairAsset);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
		.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
	GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);
	ThumbnailScene->SetHairAsset(nullptr);
}

void UHairWorksAssetThumbnailRenderer::BeginDestroy()
{
	if ( ThumbnailScene != nullptr )
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}
// @third party code - END HairWorks