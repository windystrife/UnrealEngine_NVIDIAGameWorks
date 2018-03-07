// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ScreenshotFunctionalTestBase.h"
#include "AutomationScreenshotOptions.h"

#include "ScreenshotFunctionalTest.generated.h"

class FAutomationTestScreenshotEnvSetup;

/**
 * No UI
 */
UCLASS(Blueprintable)
class FUNCTIONALTESTING_API AScreenshotFunctionalTest : public AScreenshotFunctionalTestBase
{
	GENERATED_BODY()

public:
	AScreenshotFunctionalTest(const FObjectInitializer& ObjectInitializer);

	// Tests relying on temporal effects can force a camera cut to flush stale data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", SimpleDisplay)
	bool bCameraCutOnScreenshotPrep;

protected:
	virtual void PrepareTest() override;

	virtual void RequestScreenshot() override;
};
