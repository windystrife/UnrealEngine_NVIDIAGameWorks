// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ScreenshotFunctionalTest.h"

#include "Engine/GameViewportClient.h"
#include "AutomationBlueprintFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Misc/AutomationTest.h"

AScreenshotFunctionalTest::AScreenshotFunctionalTest( const FObjectInitializer& ObjectInitializer )
	: AScreenshotFunctionalTestBase(ObjectInitializer)
	, bCameraCutOnScreenshotPrep(false)
{
}

void AScreenshotFunctionalTest::PrepareTest()
{
	Super::PrepareTest();

	// Apply a camera cut if requested
	if (bCameraCutOnScreenshotPrep)
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

		if (PlayerController && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->bGameCameraCutThisFrame = true;
			if (ScreenshotCamera)
			{
				ScreenshotCamera->NotifyCameraCut();
			}
		}
	}

	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();
}

void AScreenshotFunctionalTest::RequestScreenshot()
{
	Super::RequestScreenshot();

	// Screenshots in UE4 work in this way:
	// 1. Call FScreenshotRequest::RequestScreenshot to ask the system to take a screenshot. The screenshot
	//    will have the same resolution as the current viewport;
	// 2. Register a callback to UGameViewportClient::OnScreenshotCaptured() delegate. The call back will be
	//    called with screenshot pixel data when the shot is taken;
	// 3. Wait till the next frame or call FSceneViewport::Invalidate to force a redraw. Screenshot is not
	//    taken until next draw where UGameViewportClient::ProcessScreenshots or
	//    FEditorViewportClient::ProcessScreenshots is called to read pixels back from the viewport. It also
	//    trigger the callback function registered in step 2.
	bool bShowUI = false;
	FScreenshotRequest::RequestScreenshot(bShowUI);
}
