// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/UICommandList.h"
#include "Editor/LevelEditor/Private/SLevelEditor.h"

/**
 * Unreal level editor main toolbar
 */
class FLevelEditorToolBar
{

public:

	/**
	 * Static: Creates a widget for the main tool bar
	 *
	 * @return	New widget
	 */
	static TSharedRef< SWidget > MakeLevelEditorToolBar( const TSharedRef<FUICommandList>& InCommandList, const TSharedRef<SLevelEditor> InLevelEditor );


protected:

	/**
	 * Generates menu content for the build combo button drop down menu
	 *
	 * @return	Menu content widget
	 */
	static TSharedRef< SWidget > GenerateBuildMenuContent( TSharedRef<FUICommandList> InCommandList );

	/**
	 * Generates menu content for the create actor combo button drop down menu
	 *
	 * @return	Menu content widget
	 */
	static TSharedRef< SWidget > GenerateCreateContent( TSharedRef<FUICommandList> InCommandList );

	/**
	 * Generates menu content for the quick settings combo button drop down menu
	 *
	 * @return	Menu content widget
	 */
	static TSharedRef< SWidget > GenerateQuickSettingsMenu( TSharedRef<FUICommandList> InCommandList );

	/**
	 * Generates menu content for the source control combo button drop down menu
	 *
	 * @return	Menu content widget
	 */
	static TSharedRef< SWidget > GenerateSourceControlMenu(TSharedRef<FUICommandList> InCommandList);

	/**
	 * Generates menu content for the compile combo button drop down menu
	 *
	 * @return	Menu content widget
	 */
	static TSharedRef< SWidget > GenerateOpenBlueprintMenuContent( TSharedRef<FUICommandList> InCommandList, TWeakPtr< SLevelEditor > InLevelEditor );

	/**
	 * Generates menu content for the Cinematics combo button drop down menu
	 *
	 * @return	Menu content widget
	 */
	static TSharedRef< SWidget > GenerateCinematicsMenuContent( TSharedRef<FUICommandList> InCommandList, TWeakPtr<SLevelEditor> LevelEditorWeakPtr );

	/**
	 * Delegate for actor selection within the Cinematics popup menu's SceneOutliner.
	 * Opens the editor for the selected actor and dismisses all popup menus.
	 */
	static void OnCinematicsActorPicked( AActor* Actor );

	/**
	 * Callback to open a sub-level script Blueprint
	 *
	 * @param InLevel	The level to open the Blueprint of (creates if needed)
	 */
	static void OnOpenSubLevelBlueprint( ULevel* InLevel );
};
