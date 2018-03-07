// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Settings/WidgetDesignerSettings.h"

UWidgetDesignerSettings::UWidgetDesignerSettings()
{
	CategoryName = TEXT("ContentEditors");

	GridSnapEnabled = true;
	GridSnapSize = 4;
	bShowOutlines = true;
	bExecutePreConstructEvent = true;
	bRespectLocks = true;
}

#if WITH_EDITOR

FText UWidgetDesignerSettings::GetSectionText() const
{
	return NSLOCTEXT("UMG", "WidgetDesignerSettingsName", "Widget Designer");
}

FText UWidgetDesignerSettings::GetSectionDescription() const
{
	return NSLOCTEXT("UMG", "WidgetDesignerSettingsDescription", "Configure options for the Widget Designer.");
}

#endif