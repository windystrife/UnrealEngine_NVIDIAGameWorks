// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorProjectSettings.h"

UUMGEditorProjectSettings::UUMGEditorProjectSettings()
{
	bShowWidgetsFromEngineContent = false;
	bShowWidgetsFromDeveloperContent = true;
	bCookSlowConstructionWidgetTree = true;
}

#if WITH_EDITOR

FText UUMGEditorProjectSettings::GetSectionText() const
{
	return NSLOCTEXT("UMG", "WidgetDesignerTeamSettingsName", "Widget Designer (Team)");
}

FText UUMGEditorProjectSettings::GetSectionDescription() const
{
	return NSLOCTEXT("UMG", "WidgetDesignerTeamSettingsDescription", "Configure options for the Widget Designer that affect the whole team.");
}

#endif