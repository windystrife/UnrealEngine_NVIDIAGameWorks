// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/RendererSettings.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"

/** The editor object. */
extern UNREALED_API class UEditorEngine* GEditor;
#endif // #if WITH_EDITOR

URendererSettings::URendererSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Rendering");
	TranslucentSortAxis = FVector(0.0f, -1.0f, 0.0f);
	bSupportStationarySkylight = true;
	bSupportPointLightWholeSceneShadows = true;
	bSupportAtmosphericFog = true;
	bSupportSkinCacheShaders = false;
}

void URendererSettings::PostInitProperties()
{
	Super::PostInitProperties();
	
	SanatizeReflectionCaptureResolution();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void URendererSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SanatizeReflectionCaptureResolution();

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);

		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URendererSettings, ReflectionCaptureResolution) && 
			PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
		{
			GEditor->UpdateReflectionCaptures();
		}
	}
}
#endif // #if WITH_EDITOR

void URendererSettings::SanatizeReflectionCaptureResolution()
{
	static const int32 MaxReflectionCaptureResolution = 1024;
	static const int32 MinReflectionCaptureResolution = 64;
	ReflectionCaptureResolution = FMath::Clamp(int32(FMath::RoundUpToPowerOfTwo(ReflectionCaptureResolution)), MinReflectionCaptureResolution, MaxReflectionCaptureResolution);
}

URendererOverrideSettings::URendererOverrideSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Rendering Overrides");	
}

void URendererOverrideSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void URendererOverrideSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);	

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);		
	}
}
#endif // #if WITH_EDITOR
