// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/DeveloperSettings.h"
#include "EditorPerformanceSettings.generated.h"


UCLASS(minimalapi, config=EditorSettings, meta=(DisplayName = "Performance", ToolTip="Settings to tweak the performance of the editor"))
class UEditorPerformanceSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()
	
	/** When enabled, the application frame rate, memory and Unreal object count will be displayed in the main editor UI */
	UPROPERTY(EditAnywhere, config, Category=EditorPerformance)
	uint32 bShowFrameRateAndMemory:1;

	/** Lowers CPU usage when the editor is in the background and not the active application */
	UPROPERTY(EditAnywhere, config, Category=EditorPerformance, meta=(DisplayName="Use Less CPU when in Background") )
	uint32 bThrottleCPUWhenNotForeground:1;

	/** When turned on, the editor will constantly monitor performance and adjust scalability settings for you when performance drops (disabled in debug) */
	UPROPERTY(EditAnywhere, config, Category=EditorPerformance)
	uint32 bMonitorEditorPerformance:1;

	/** 
	 * By default the editor will adjust scene scaling (quality) for high DPI in order to ensure consistent performance with very large render targets.
	 * Enabling this will disable automatic adjusting and use the screen percentage specified here
	 */
	UPROPERTY(EditAnywhere, config, Category=EditorPerformance, meta=(DisplayName="Disable DPI Based Editor Viewport Scaling", ConsoleVariable="Editor.OverrideDPIBasedEditorViewportScaling"))
	bool bOverrideDPIBasedEditorViewportScaling;

public:
	/** UObject interface */
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
};

