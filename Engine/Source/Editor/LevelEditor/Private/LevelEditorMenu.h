// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once


#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Editor/LevelEditor/Private/SLevelEditor.h"

/**
 * Level editor menu
 */
class FLevelEditorMenu
{

public:

	/**
	 * Static: Creates a widget for the level editor's menu
	 *
	 * @return	New widget
	 */
	static TSharedRef< SWidget > MakeLevelEditorMenu( const TSharedPtr<FUICommandList>& CommandList, TSharedPtr<class SLevelEditor> LevelEditor );

	static TSharedRef< SWidget > MakeNotificationBar( const TSharedPtr<FUICommandList>& CommandList, TSharedPtr<class SLevelEditor> LevelEditor );
};
