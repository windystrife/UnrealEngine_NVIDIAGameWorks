// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FunctionalUIScreenshotTest.h"

#include "Engine/GameViewportClient.h"
#include "AutomationBlueprintFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Misc/AutomationTest.h"
#include "Widgets/SViewport.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/SceneViewport.h"
#include "Slate/WidgetRenderer.h"

AFunctionalUIScreenshotTest::AFunctionalUIScreenshotTest( const FObjectInitializer& ObjectInitializer )
	: AScreenshotFunctionalTestBase(ObjectInitializer)
{
	WidgetLocation = EWidgetTestAppearLocation::Viewport;
}

/**
 * Get pixel format and color space of a backbuffer. Do nothing if the viewport doesn't
 * render into backbuffer directly
 * @InViewport - the viewport to get backbuffer from
 * @OutPixelFormat - pixel format of the backbuffer
 * @OutIsSRGB - whether the backbuffer stores pixels in sRGB space
 */ 
void GetBackbufferInfo(const FViewport* InViewport, EPixelFormat* OutPixelFormat, bool* OutIsSRGB)
{
	if (!InViewport->GetViewportRHI())
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(GetBackbufferFormatCmd)(
		[InViewport, OutPixelFormat, OutIsSRGB](FRHICommandListImmediate& RHICmdList)
	{
		FViewportRHIRef ViewportRHI = InViewport->GetViewportRHI();
		check(ViewportRHI.IsValid());
		FTexture2DRHIRef BackbufferTexture = RHICmdList.GetViewportBackBuffer(ViewportRHI);
		check(BackbufferTexture.IsValid());
		*OutPixelFormat = BackbufferTexture->GetFormat();
		*OutIsSRGB = (BackbufferTexture->GetFlags() & TexCreate_SRGB) == TexCreate_SRGB;
	});
	FlushRenderingCommands();
}

void AFunctionalUIScreenshotTest::PrepareTest()
{
	// Resize viewport to screenshot size
	Super::PrepareTest();

	TSharedPtr<SViewport> GameViewportWidget = GEngine->GameViewport->GetGameViewportWidget();
	check(GameViewportWidget.IsValid());

	// If render directly to backbuffer, just read from backbuffer
	if (!GameViewportWidget->ShouldRenderDirectly())
	{
		// Resize screenshot render target to have the same size as the game viewport. Also
		// make sure they have the same data format (pixel format, color space, etc.) if possible
		const FSceneViewport* GameViewport = GEngine->GameViewport->GetGameViewport();
		FIntPoint ScreenshotSize = GameViewport->GetSizeXY();
		EPixelFormat PixelFormat = PF_A2B10G10R10;
		bool bIsSRGB = false;
		GetBackbufferInfo(GameViewport, &PixelFormat, &bIsSRGB);

		if (!ScreenshotRT)
		{
			ScreenshotRT = NewObject<UTextureRenderTarget2D>(this);
		}
		ScreenshotRT->ClearColor = FLinearColor::Transparent;
		ScreenshotRT->InitCustomFormat(ScreenshotSize.X, ScreenshotSize.Y, PixelFormat, !bIsSRGB);
	}

	// Spawn the widget
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	SpawnedWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
	
	if (SpawnedWidget)
	{
		if (WidgetLocation == EWidgetTestAppearLocation::Viewport)
		{
			SpawnedWidget->AddToViewport();
		}
		else
		{
			// Add to the game viewport and restrain the widget within
			// owning player's sub-rect
			SpawnedWidget->AddToPlayerScreen();
		}

		SpawnedWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();
}

void AFunctionalUIScreenshotTest::OnScreenshotTakenAndCompared()
{
	if (SpawnedWidget)
	{
		SpawnedWidget->RemoveFromParent();
	}

	// Restore viewport size and finish the test
	Super::OnScreenshotTakenAndCompared();
}

/**
 * Read all pixels from backbuffer
 * @InViewport - backbuffer comes from this viewport
 * @OutPixels
 */
void ReadBackbuffer(const FViewport* InViewport, TArray<FColor>* OutPixels)
{
	// Make sure rendering to the viewport backbuffer is finished
	FlushRenderingCommands();

	ENQUEUE_RENDER_COMMAND(CopyBackbufferCmd)(
		[InViewport, OutPixels](FRHICommandListImmediate& RHICmdList)
	{
		FViewportRHIRef ViewportRHI = InViewport->GetViewportRHI();
		FTexture2DRHIRef BackbufferTexture = RHICmdList.GetViewportBackBuffer(ViewportRHI);
		RHICmdList.ReadSurfaceData(
			BackbufferTexture,
			FIntRect(0, 0, BackbufferTexture->GetSizeX(), BackbufferTexture->GetSizeY()),
			*OutPixels,
			FReadSurfaceDataFlags());
	});
	FlushRenderingCommands();
}

void ReadPixelsFromRT(UTextureRenderTarget2D* InRT, TArray<FColor>* OutPixels)
{
	ENQUEUE_RENDER_COMMAND(ReadScreenshotRTCmd)(
		[InRT, OutPixels](FRHICommandListImmediate& RHICmdList)
	{
		FTextureRenderTarget2DResource* RTResource =
			static_cast<FTextureRenderTarget2DResource*>(InRT->GetRenderTargetResource());
		RHICmdList.ReadSurfaceData(
			RTResource->GetTextureRHI(),
			FIntRect(0, 0, InRT->SizeX, InRT->SizeY),
			*OutPixels,
			FReadSurfaceDataFlags());
	});
	FlushRenderingCommands();
}

void AFunctionalUIScreenshotTest::RequestScreenshot()
{
	// Register a handler to UGameViewportClient::OnScreenshotCaptured
	Super::RequestScreenshot();

	UGameViewportClient* GameViewportClient = GEngine->GameViewport;
	TSharedPtr<SViewport> ViewportWidget = GameViewportClient->GetGameViewportWidget();
	FIntPoint ScreenshotSize = GameViewportClient->GetGameViewport()->GetSizeXY();
	TArray<FColor> OutColorData;

	if (ViewportWidget.IsValid())
	{
		if (ViewportWidget->ShouldRenderDirectly())
		{
			const FSceneViewport* GameViewport = GameViewportClient->GetGameViewport();
			ReadBackbuffer(GameViewport, &OutColorData);
		}
		else
		{
			// Draw the game viewport (overlaid with the widget to screenshot) to our ScreenshotRT.
			// Need to do this manually because the game viewport doesn't have a valid FViewportRHIRef
			// when rendering to a separate render target
			TSharedPtr<FWidgetRenderer> WidgetRenderer = MakeShareable(new FWidgetRenderer(true, false));
			check(WidgetRenderer.IsValid());
			WidgetRenderer->DrawWidget(ScreenshotRT, ViewportWidget.ToSharedRef(), ScreenshotSize, 0.f);
			FlushRenderingCommands();

			ReadPixelsFromRT(ScreenshotRT, &OutColorData);
		}

		// For UI, we only care about what the final image looks like. So don't compare alpha channel.
		for (int32 Idx = 0; Idx < OutColorData.Num(); ++Idx)
		{
			OutColorData[Idx].A = 0xff;
		}
	}

	check(OutColorData.Num() == ScreenshotSize.X * ScreenshotSize.Y);
	GameViewportClient->OnScreenshotCaptured().Broadcast(ScreenshotSize.X, ScreenshotSize.Y, OutColorData);
}
