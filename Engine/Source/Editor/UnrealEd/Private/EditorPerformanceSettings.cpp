// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Editor/EditorPerformanceSettings.h"
#include "EditorViewportClient.h"
#include "Editor.h"

static TAutoConsoleVariable<int32> CVarOverrideDPIBasedEditorViewportScaling(
	TEXT("Editor.OverrideDPIBasedEditorViewportScaling"),
	0,
	TEXT("Sets whether or not we should globally override screen percentage in editor and PIE viewports"),
	ECVF_Default);

UEditorPerformanceSettings::UEditorPerformanceSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bShowFrameRateAndMemory(false)
	, bThrottleCPUWhenNotForeground(true)
	, bMonitorEditorPerformance(true)
	, bOverrideDPIBasedEditorViewportScaling(false)
{

}

void UEditorPerformanceSettings::PostInitProperties()
{
	Super::PostInitProperties();

	CVarOverrideDPIBasedEditorViewportScaling->Set(bOverrideDPIBasedEditorViewportScaling, ECVF_SetByProjectSetting);

}

void UEditorPerformanceSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	if (PropertyChangedEvent.Property && (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UEditorPerformanceSettings, bOverrideDPIBasedEditorViewportScaling)))
	{
		// Tell all viewports to refresh their screen percentage when the dpi scaling override changes
		for (FEditorViewportClient* Client : GEditor->AllViewportClients)
		{
			if (Client)
			{
				Client->RequestUpdateEditorScreenPercentage();
			}
		}

		if (GEngine->GameViewport)
		{
			GEngine->GameViewport->RequestUpdateEditorScreenPercentage();
		}
	}
}