// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Frame/MainFrameActions.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "FRecentProjectsMenu"

/**
 * Static helper class for populating the "Recent Projects" menu.
 */
class FRecentProjectsMenu
{
public:

	/**
	 * Creates the menu.
	 *
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 */
	static void MakeMenu( FMenuBuilder& MenuBuilder )
	{
		for ( int32 ProjectIndex = 0; ProjectIndex < FMainFrameActionCallbacks::ProjectNames.Num() && ProjectIndex < FMainFrameCommands::Get().SwitchProjectCommands.Num(); ++ProjectIndex )
		{
			// If it is a project file, display the filename without extension. Otherwise just display the project name.
			const FString& ProjectName = FMainFrameActionCallbacks::ProjectNames[ ProjectIndex ];

			if (( IFileManager::Get().FileSize(*ProjectName) <= 0 ) ||
				( FPaths::GetProjectFilePath() == ProjectName ))
			{
				// Don't display project files that do not exist.
				continue;
			}

			const FText DisplayName = FText::FromString( FPaths::GetBaseFilename(*ProjectName) );
			const FText Tooltip = FText::FromString( IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ProjectName) );
			MenuBuilder.AddMenuEntry( FMainFrameCommands::Get().SwitchProjectCommands[ ProjectIndex ], NAME_None, DisplayName, Tooltip );
		}
	}
};


#undef LOCTEXT_NAMESPACE
