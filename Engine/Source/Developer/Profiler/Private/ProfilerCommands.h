// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/Commands.h"

class FMenuBuilder;

/**
 * Class that holds all profiler commands.
 */
class FProfilerCommands
	: public TCommands<FProfilerCommands>
{
public:

	/** Default constructor. */
	FProfilerCommands();
	
	/** Initialize commands. */
	virtual void RegisterCommands() override;

public:

	/*-----------------------------------------------------------------------------
		Global and custom commands.	Need to implement following methods:

		void Map_<CommandName>_Global();
		const FUIAction <CommandName>_Custom(...) const;
	-----------------------------------------------------------------------------*/

	/** Toggles the data preview for all session instances. Global and custom command. */
	TSharedPtr< FUICommandInfo > ToggleDataPreview;

	/** Toggles the data capture for all session instances. Global and custom command. */
	TSharedPtr< FUICommandInfo > ToggleDataCapture;

	/** Toggles showing all data graphs for all session instances. Global and custom command. */
	TSharedPtr< FUICommandInfo > ToggleShowDataGraph;

	/** Opens event graph for all session instances. Global and custom command. */
	TSharedPtr< FUICommandInfo > OpenEventGraph;

	/*-----------------------------------------------------------------------------
		Global commands. Need to implement following methods:

		void Map_<CommandName>_Global();
	-----------------------------------------------------------------------------*/

	/** Saves all collected data to file or files. */
	TSharedPtr< FUICommandInfo > ProfilerManager_Save;

	/** Stats Profiler */
	TSharedPtr< FUICommandInfo > StatsProfiler;

	/** Memory Profiler */
	TSharedPtr< FUICommandInfo > MemoryProfiler;

	/** FPS Chart */
	TSharedPtr< FUICommandInfo > FPSChart;

	/** Open settings for the profiler manager */
	TSharedPtr< FUICommandInfo > OpenSettings;


	/** Load profiler data. Global version. */
	TSharedPtr< FUICommandInfo > ProfilerManager_Load;

	/** Load multiple profiler data. Global version. */
	TSharedPtr< FUICommandInfo > ProfilerManager_LoadMultiple;

	/** Toggles the real time live preview. Global version. */
	TSharedPtr< FUICommandInfo > ProfilerManager_ToggleLivePreview;

	/** Toggles the data graph view mode between time based and index based. */
	TSharedPtr< FUICommandInfo > DataGraph_ToggleViewMode;

	/** Sets the data graph view mode to the time based. */
	TSharedPtr< FUICommandInfo > DataGraph_ViewMode_SetTimeBased;

	/** Sets the data graph view mode to the index based. */
	TSharedPtr< FUICommandInfo > DataGraph_ViewMode_SetIndexBased;

	/** Select all frame in the data graph and display them in the event graph, technically switches to the begin of history. */
	TSharedPtr< FUICommandInfo > EventGraph_SelectAllFrames;
};


class FProfilerMenuBuilder
{
public:
	/**
	 * Helper method for adding a customized menu entry using the global UI command info.
	 * FUICommandInfo cannot be executed with custom parameters, so we need to create a custom FUIAction,
	 * but sometime we have global and local version for the UI command, so reuse data from the global UI command info.
	 * Ex:
	 *		SessionInstance_ToggleCapture			- Global version will toggle capture process for all active session instances
	 *		SessionInstance_ToggleCapture_OneParam	- Local version will toggle capture process only for the specified session instance
	 *
	 * @param MenuBuilder The menu to add items to
	 * @param FUICommandInfo A shared pointer to the UI command info
	 * @param UIAction Customized version of the UI command info stored in an UI action 
	 *
	 */
	static void AddMenuEntry( FMenuBuilder& MenuBuilder, const TSharedPtr< FUICommandInfo >& UICommandInfo, const FUIAction& UIAction );
};


/** 
 * Class that provides helper functions for the commands to avoid cluttering profiler manager with many small functions. Can't contain any variables.
 * Directly operates on the profiler manager instance.
 */
class FProfilerActionManager
{
	friend class FProfilerManager;

	/*-----------------------------------------------------------------------------
		ProfilerManager_Load
	-----------------------------------------------------------------------------*/
public:
	/** Maps UI command info ProfilerManager_Load with the specified UI command list. */
	void Map_ProfilerManager_Load();
	void Map_ProfilerManager_LoadMultiple();

protected:
	/** Handles FExecuteAction for ProfilerManager_Load. */
	void ProfilerManager_Load_Execute();
	void ProfilerManager_LoadMultiple_Execute();
	/** Handles FCanExecuteAction for ProfilerManager_Load. */
	bool ProfilerManager_Load_CanExecute() const;

	/*-----------------------------------------------------------------------------
		ToggleDataPreview
		NOTE: Sends a message to the profiler service for this
	-----------------------------------------------------------------------------*/
public:
	/** Maps UI command info ToggleDataPreview with the specified UI command list. */
	void Map_ToggleDataPreview_Global();

	/**
	 * UI action that Toggles the data preview for the specified session instance.
	 *
	 * @param SessionInstanceID - the session instance that this action will be executed on, if not valid, all session instances will be used
	 *
	 */
	const FUIAction ToggleDataPreview_Custom( const FGuid SessionInstanceID ) const;
		
protected:
	/** Handles FExecuteAction for ToggleDataPreview. */
	void ToggleDataPreview_Execute( const FGuid SessionInstanceID );
	/** Handles FCanExecuteAction for ToggleDataPreview. */
	bool ToggleDataPreview_CanExecute( const FGuid SessionInstanceID ) const;
	/** Handles FGetActionCheckState for ToggleDataPreview. */
	ECheckBoxState ToggleDataPreview_GetCheckState( const FGuid SessionInstanceID ) const;

	/*-----------------------------------------------------------------------------
		ProfilerManager_ToggleLivePreview
	-----------------------------------------------------------------------------*/
public:
	/** Maps UI command info ProfilerManager_ToggleLivePreview with the specified UI command list. */
	void Map_ProfilerManager_ToggleLivePreview_Global();

protected:
	/** Handles FExecuteAction for ProfilerManager_ToggleLivePreview. */
	void ProfilerManager_ToggleLivePreview_Execute();
	/** Handles FCanExecuteAction for ProfilerManager_ToggleLivePreview. */
	bool ProfilerManager_ToggleLivePreview_CanExecute( ) const;
	/** Handles FGetActionCheckState for ProfilerManager_ToggleLivePreview. */
	ECheckBoxState ProfilerManager_ToggleLivePreview_GetCheckState() const;

	/*-----------------------------------------------------------------------------
		ToggleDataCapture
		NOTE: Sends a message to the profiler service for this
	-----------------------------------------------------------------------------*/
public:
	/** Maps UI command info ToggleDataCapture with the specified UI command list. */
	void Map_ToggleDataCapture_Global();
	
	/**
	 * UI action that toggles the data capture for the specified session instance.
	 *
	 * @param SessionInstanceID - the session instance that this action will be executed on, if not valid, all session instances will be used
	 *
	 */
	const FUIAction ToggleDataCapture_Custom( const FGuid SessionInstanceID ) const;
		
protected:
	/** Handles FExecuteAction for ToggleDataCapture. */
	void ToggleDataCapture_Execute( const FGuid SessionInstanceID );
	/** Handles FCanExecuteAction for ToggleDataCapture. */
	bool ToggleDataCapture_CanExecute( const FGuid SessionInstanceID ) const;
	/** Handles FGetActionCheckState for ToggleDataCapture. */
	ECheckBoxState ToggleDataCapture_GetCheckState( const FGuid SessionInstanceID ) const;

private:
	/** Private constructor. */
	FProfilerActionManager( class FProfilerManager* Instance )
		: This( Instance )
	{}

	/*-----------------------------------------------------------------------------
		OpenSettings
	-----------------------------------------------------------------------------*/
	
public:
	/** Maps UI command info OpenSettings with the specified UI command list. */
	void Map_OpenSettings_Global();
	
	/** Add comment here */
	const FUIAction OpenSettings_Custom() const;
		
protected:
	/** Handles FExecuteAction for OpenSettings. */
	void OpenSettings_Execute();
	/** Handles FCanExecuteAction for OpenSettings. */
	bool OpenSettings_CanExecute() const;
	/** Handles FIsActionChecked for OpenSettings. */

	/** Reference to the global instance of the profiler manager. */
	class FProfilerManager* This;
};
