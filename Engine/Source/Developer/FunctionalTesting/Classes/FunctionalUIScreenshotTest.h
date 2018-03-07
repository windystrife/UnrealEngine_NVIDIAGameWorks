// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ScreenshotFunctionalTestBase.h"
#include "AutomationScreenshotOptions.h"
#include "Blueprint/UserWidget.h"

#include "FunctionalUIScreenshotTest.generated.h"

class UTextureRenderTarget2D;

UENUM()
enum class EWidgetTestAppearLocation
{
	Viewport,
	PlayerScreen
};

/**
 * 
 */
UCLASS(Blueprintable, MinimalAPI)
class AFunctionalUIScreenshotTest : public AScreenshotFunctionalTestBase
{
	GENERATED_BODY()

public:
	AFunctionalUIScreenshotTest(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void PrepareTest() override;

	virtual void OnScreenshotTakenAndCompared() override;

	virtual void RequestScreenshot() override;
	
protected:

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY()
	UUserWidget* SpawnedWidget;

	UPROPERTY(EditAnywhere, Category = "UI")
	EWidgetTestAppearLocation WidgetLocation;

	UPROPERTY(Transient, DuplicateTransient)
	UTextureRenderTarget2D* ScreenshotRT;
};
