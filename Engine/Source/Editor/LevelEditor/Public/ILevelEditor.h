// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Editor/UnrealEdTypes.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Commands/UICommandList.h"
#include "AssetThumbnail.h"
#include "Toolkits/IToolkitHost.h"

class ILevelViewport;
class SLevelViewport;

/**
 * Public interface to SLevelEditor
 */
class ILevelEditor : public SCompoundWidget, public IToolkitHost
{

public:

	/** Summons a context menu for this level editor at the mouse cursor's location */
	virtual void SummonLevelViewportContextMenu() = 0;

	/** Summons a context menu for view options */
	virtual void SummonLevelViewportViewOptionMenu(ELevelViewportType ViewOption) = 0;

	/** Returns a list of all of the toolkits that are currently hosted by this toolkit host */
	virtual const TArray< TSharedPtr< IToolkit > >& GetHostedToolkits() const = 0;

	/** Gets an array of all viewports in this level editor */
	virtual TArray< TSharedPtr< ILevelViewport > > GetViewports() const = 0;
	
	/** Gets the active level viewport for this level editor */
	virtual TSharedPtr<ILevelViewport> GetActiveViewportInterface() = 0;

	/** Get the thumbnail pool used by this level editor */
	virtual TSharedPtr< class FAssetThumbnailPool > GetThumbnailPool() const = 0;

	/** Access the level editor's action command list */
	virtual const TSharedPtr< FUICommandList >& GetLevelEditorActions() const = 0;

	/** Called to process a key down event in a viewport when in immersive mode */
	virtual FReply OnKeyDownInViewport( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) = 0;

	/** Append commands to the command list for the level editor */
	virtual void AppendCommands( const TSharedRef<FUICommandList>& InCommandsToAppend ) = 0;

	/** After spawning a new level viewport outside of the editor's tab system, this function must be called so that
	    the editor can keep track of that viewport */
	virtual void AddStandaloneLevelViewport( const TSharedRef<SLevelViewport>& LevelViewport ) = 0;

	/** Spawns an Actor Details widget */
	virtual TSharedRef<SWidget> CreateActorDetails( const FName TabIdentifier ) = 0;

	/** Spawns a level editor ToolBox widget (aka. "Modes") */
	virtual TSharedRef<SWidget> CreateToolBox() = 0;

};


