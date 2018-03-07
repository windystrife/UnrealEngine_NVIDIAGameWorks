// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

/**
 * Public identifiers for the viewport layouts available in LevelViewportLayoutX.h files
 * These are names rather than enums as they're also used to persist states in config
 * When editing this list you also need to edit FLevelViewportTabContent::ConstructViewportLayoutByTypeName()
 */
namespace LevelViewportConfigurationNames
{
	static FName TwoPanesHoriz("TwoPanesHoriz");
	static FName TwoPanesVert("TwoPanesVert");
	static FName ThreePanesLeft("ThreePanesLeft");
	static FName ThreePanesRight("ThreePanesRight");
	static FName ThreePanesTop("ThreePanesTop");
	static FName ThreePanesBottom("ThreePanesBottom");
	static FName FourPanesLeft("FourPanesLeft");
	static FName FourPanesRight("FourPanesRight");
	static FName FourPanesTop("FourPanesTop");
	static FName FourPanesBottom("FourPanesBottom");
	static FName FourPanes2x2("FourPanes2x2");
	static FName OnePane("OnePane");
}

/**
 * Class containing commands for level viewport actions
 */
class LEVELEDITOR_API FLevelViewportCommands : public TCommands<FLevelViewportCommands>
{
public:
	FLevelViewportCommands() 
		: TCommands<FLevelViewportCommands>
		(
			TEXT("LevelViewport"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "LevelViewports", "Level Viewports"), // Localized context name for displaying
			TEXT("EditorViewport"), // Parent context name.  
			FEditorStyle::GetStyleSetName() // Icon Style Set
		)
	{
	}
	virtual ~FLevelViewportCommands();

	/** Get the singleton instance of this set of commands. */
	FORCENOINLINE static FLevelViewportCommands& Get()
	{
		return *(Instance.Pin());
	}

	struct FShowMenuCommand
	{
		TSharedPtr<FUICommandInfo> ShowMenuItem;
		FText LabelOverride;

		FShowMenuCommand( TSharedPtr<FUICommandInfo> InShowMenuItem, const FText& InLabelOverride )
			: ShowMenuItem( InShowMenuItem )
			, LabelOverride( InLabelOverride )
		{
		}

		FShowMenuCommand( TSharedPtr<FUICommandInfo> InShowMenuItem )
			: ShowMenuItem( InShowMenuItem )
		{
		}
	};

	/** Opens the advanced viewport settings */
	TSharedPtr< FUICommandInfo > AdvancedSettings;

	/** Toggles game preview in the viewport */
	TSharedPtr< FUICommandInfo > ToggleGameView;

	/** Toggles immersive mode in the viewport */
	TSharedPtr< FUICommandInfo > ToggleImmersive;

	/** Toggles maximize mode in the viewport */
	TSharedPtr< FUICommandInfo > ToggleMaximize;

	/** Creates a cameraActor at current perspective viewport position */
	TSharedPtr< FUICommandInfo > CreateCamera;

	/** Opens the control panel for high resolution screenshots */
	TSharedPtr< FUICommandInfo > HighResScreenshot;

	/** Reset all show flags to default */
	TSharedPtr< FUICommandInfo > UseDefaultShowFlags;

	/** Allows this viewport to be used for previewing cinematic animations */ 
	TSharedPtr< FUICommandInfo > ToggleCinematicPreview;

	/** Finds instances of selected object in level script. */ 
	TSharedPtr< FUICommandInfo > FindInLevelScriptBlueprint;

	/** List of commands for show flags and their localized names.  One for each command */
	TArray< FShowMenuCommand > ShowFlagCommands;

	/** Shows all volume classes */
	TSharedPtr< FUICommandInfo > ShowAllVolumes;

	/** Hides all volume classes */
	TSharedPtr< FUICommandInfo > HideAllVolumes;

	/** List of commands for showing volume classes and their localized names.  One for each command */
	TArray< FShowMenuCommand > ShowVolumeCommands;

	/** Shows all layers */
	TSharedPtr< FUICommandInfo > ShowAllLayers;

	/** Hides all layers */
	TSharedPtr< FUICommandInfo > HideAllLayers;

	/** Shows all sprite categories */
	TSharedPtr< FUICommandInfo > ShowAllSprites;

	/** Hides all sprite categories  */
	TSharedPtr< FUICommandInfo > HideAllSprites;

	/** List of commands for showing sprite categories and their localized names.  One for each command */
	TArray< FShowMenuCommand > ShowSpriteCommands;

	/** Hides all Stats categories  */
	TSharedPtr< FUICommandInfo > HideAllStats;

	/** A map of stat categories and the commands that belong in them */
	TMap< FString, TArray< FShowMenuCommand > > ShowStatCatCommands;

	/** Applys a material to an actor */
	TSharedPtr< FUICommandInfo > ApplyMaterialToActor;

	struct FBufferVisualizationRecord
	{
		FName Name;
		TSharedPtr< FUICommandInfo > Command;
	};

	typedef TMultiMap<FName, FBufferVisualizationRecord> TBufferVisualizationModeCommandMap;
	TBufferVisualizationModeCommandMap BufferVisualizationModeCommands;
	
	TSharedPtr< FUICommandInfo > FocusViewportToSelectedActors;

	/**
	 * Bookmarks                   
	 */
	TArray< TSharedPtr< FUICommandInfo > > JumpToBookmarkCommands;
	TArray< TSharedPtr< FUICommandInfo > > SetBookmarkCommands;
	TArray< TSharedPtr< FUICommandInfo > > ClearBookmarkCommands;
	TSharedPtr< FUICommandInfo > ClearAllBookMarks;

	/** Actor pilot commands */
	TSharedPtr< FUICommandInfo > EjectActorPilot;
	TSharedPtr< FUICommandInfo > PilotSelectedActor;

	/** Toggles showing the exact camera view when locking a viewport to a camera */
	TSharedPtr< FUICommandInfo > ToggleActorPilotCameraView;

	/** Viewport pane configurations */
	TSharedPtr< FUICommandInfo > ViewportConfig_OnePane;
	TSharedPtr< FUICommandInfo > ViewportConfig_TwoPanesH;
	TSharedPtr< FUICommandInfo > ViewportConfig_TwoPanesV;
	TSharedPtr< FUICommandInfo > ViewportConfig_ThreePanesLeft;
	TSharedPtr< FUICommandInfo > ViewportConfig_ThreePanesRight;
	TSharedPtr< FUICommandInfo > ViewportConfig_ThreePanesTop;
	TSharedPtr< FUICommandInfo > ViewportConfig_ThreePanesBottom;
	TSharedPtr< FUICommandInfo > ViewportConfig_FourPanesLeft;
	TSharedPtr< FUICommandInfo > ViewportConfig_FourPanesRight;
	TSharedPtr< FUICommandInfo > ViewportConfig_FourPanesTop;
	TSharedPtr< FUICommandInfo > ViewportConfig_FourPanesBottom;
	TSharedPtr< FUICommandInfo > ViewportConfig_FourPanes2x2;

	TSharedPtr< FUICommandInfo > SetDefaultViewportType;

	TSharedPtr< FUICommandInfo > ToggleViewportToolbar;

	TSharedPtr< FUICommandInfo > EnablePreviewMesh;
	TSharedPtr< FUICommandInfo > CyclePreviewMesh;

	/** Delegate we fire every time a new stat has been had a command added */
	DECLARE_EVENT_TwoParams(FLevelViewportCommands, FOnNewStatCommandAdded, const TSharedPtr<FUICommandInfo>, const FString&);
	static FOnNewStatCommandAdded NewStatCommandDelegate;

public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() override;

	/** Registers our commands with the binding system for showing volumes. */
	void RegisterShowVolumeCommands();

private:
	/** Registers additional commands as they are loaded */
	void HandleNewStatGroup(const TArray<FStatNameAndInfo>& NameAndInfos);
	void HandleNewStat(const FName& InStatName, const FName& InStatCategory, const FText& InStatDescription);
	int32 FindStatIndex(const TArray< FShowMenuCommand >* ShowStatCommands, const FString& InCommandName) const;
};
