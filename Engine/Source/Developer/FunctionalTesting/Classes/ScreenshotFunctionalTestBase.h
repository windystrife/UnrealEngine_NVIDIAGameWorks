// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "FunctionalTest.h"
#include "AutomationScreenshotOptions.h"

#include "ScreenshotFunctionalTestBase.generated.h"

class FAutomationTestScreenshotEnvSetup;

/**
* Base class for screenshot functional test
*/
UCLASS(Blueprintable, abstract)
class FUNCTIONALTESTING_API AScreenshotFunctionalTestBase : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AScreenshotFunctionalTestBase(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	// Set player view target to screenshot camera and call PrepareForScreenshot
	virtual void PrepareTest() override;

	// Handle screenshot delay
	virtual bool IsReady_Implementation() override;

	// Register OnScreenshotTakenAndCompared and call RequestScreenshot
	virtual void StartTest() override;

	// Call RestoreViewport and finish this test
	virtual void OnScreenshotTakenAndCompared();

	// Resize viewport to screenshot size (if possible) and set up screenshot environment (disable AA, etc.)
	void PrepareForScreenshot();

	// Doesn't actually request in base class. It simply register OnScreenshotCaptured
	virtual void RequestScreenshot();

	// Pass screenshot pixels and meta data to FAutomationTestFramework. Register
	// OnComparisonComplete which will be called the automation test system when
	// screenshot comparison is complete
	void OnScreenShotCaptured(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData);

	// Do some logging and trigger OnScreenshotTakenAndCompared
	void OnComparisonComplete(bool bWasNew, bool bWasSimilar, double MaxLocalDifference, double GlobalDifference, FString ErrorMessage);

	// Restore viewport size and restore original environment
	void RestoreGameViewport();

protected:

	UPROPERTY(BlueprintReadOnly, Category = "Screenshot")
	class UCameraComponent* ScreenshotCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screenshot", SimpleDisplay)
	FAutomationScreenshotOptions ScreenshotOptions;

	FIntPoint ViewportRestoreSize;

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)
	TSharedPtr<FAutomationTestScreenshotEnvSetup> ScreenshotEnvSetup;
#endif
};
