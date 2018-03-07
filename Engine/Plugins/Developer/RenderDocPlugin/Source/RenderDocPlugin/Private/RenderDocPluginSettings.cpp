// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RenderDocPluginSettings.h"
#include "UObject/UnrealType.h"

static FName DeveloperSettingsConsoleVariableMetaFName(TEXT("ConsoleVariable"));

void URenderDocPluginSettings::SaveSettings()
{
	SaveConfig(CPF_Config, *GetClass()->GetDefaultConfigFilename(), GConfig);
}

void URenderDocPluginSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
		SaveSettings();
	}
#endif
}

FName URenderDocPluginSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

#if WITH_EDITOR
void URenderDocPluginSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif
