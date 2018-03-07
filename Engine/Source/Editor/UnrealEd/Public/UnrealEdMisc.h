// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Widgets/SWindow.h"
#include "Editor.h"

class FPerformanceAnalyticsStats;
class FTickableEditorObject;
class FUICommandInfo;

enum class EMapChangeType : uint8
{
	/** Map has just been loaded*/
	LoadMap,

	/** Map is about to be saved*/
	SaveMap,

	/** A new map is loaded*/
	NewMap,

	/** The world is about to be torn down */
	TearDownWorld,
};


/** The public interface for the unreal editor misc singleton. */
class UNREALED_API FUnrealEdMisc
{
public:

	/** The various autosave states that exist */
	struct EAutosaveState
	{
		enum Type
		{
			Inactive,
			Saving,
			Cancelled
		};
	};

	/** Singleton accessor */
	static FUnrealEdMisc& Get();

	/** Destructor */
	virtual ~FUnrealEdMisc();

	/** Initalizes various systems */
	virtual void OnInit();

	/* Check if this we are editing a template project, and if so mount any shared resource paths it uses */
	void MountTemplateSharedPaths();

	/* Cleans up various systems */
	virtual void OnExit();

	/** Performs any required cleanup in the case of a fatal error. */
	virtual void ShutdownAfterError();

	/**
	 *	Fetches the current state of the autosave in progress
	 *
	 *	@return	enum		the current state of the autosave
	 */
	EAutosaveState::Type GetAutosaveState() const
	{
		return AutosaveState;
	}

	/**
	 * Sets the new state for the autosave
	 *
	 * @param InState		New state for the autosave
	 */
	void SetAutosaveState( const EAutosaveState::Type InState )
	{
		AutosaveState = InState;
	}

	/**
	 *	Whether or not the map build in progressed was cancelled by the user. 
	 *
	 *	@return	bool		the current state of the flag
	 */
	bool GetMapBuildCancelled() const
	{
		return bCancelBuild;
	}

	/**
	 * Sets the flag that states whether or not the map build was cancelled.
	 *
	 * @param InCancelled	New state for the cancelled flag.
	 */
	void SetMapBuildCancelled( const bool InCancelled )
	{
		bCancelBuild = InCancelled;
	}

	/**
	 * Get the project name we will use to reload the editor when switching projects
	 *
	 * @return	FString		Name of the project the editor will switch to
	 */
	const FString& GetPendingProjectName() const
	{
		return PendingProjectName;
	}

	/**
	 * Set the project name we will use to reload the editor when switching projects
	 *
	 * @param ProjectName		Name of the project to switch to
	 */
	void SetPendingProjectName( const FString& ProjectName )
	{
		PendingProjectName = ProjectName;
	}

	/** Clear the project name we will use to reload the editor when switching projects */
	void ClearPendingProjectName( )
	{
		PendingProjectName.Empty();
	}

	/**
	 * Sets whether saving the layout on close is allowed.
	 *
	 * @param bIsEnabled	true if saving on close is allowed.
	 */
	void AllowSavingLayoutOnClose(bool bIsEnabled)
	{
		bSaveLayoutOnClose = bIsEnabled;
	}

	/** Returns true if saving layout on close is allowed. */
	bool IsSavingLayoutOnClosedAllowed()
	{
		return bSaveLayoutOnClose;
	}

	/**
	 * Sets the config file to use for restoring Config files.
	 *
	 * @param InBackupFile		The filename of the backup file to restore
	 * @param InConfigFile		The config filename to overwrite
	 */
	void SetConfigRestoreFilename(FString InBackupFile, FString InConfigFile)
	{
		RestoreConfigFiles.FindOrAdd(InConfigFile) = InBackupFile;
	}

	/** Clears the config restore name for restoring the specified config file */
	void ClearConfigRestoreFilename(FString Destination)
	{
		RestoreConfigFiles.Remove(Destination);
	}

	/** Clears all the restore names for restoring config files */
	void ClearConfigRestoreFilenames()
	{
		RestoreConfigFiles.Empty();
	}

	/** Retrieves the map of config->backup config filenames to be used for restoring config files */
	const TMap<FString, FString>& GetConfigRestoreFilenames()
	{
		return RestoreConfigFiles;
	}

	/**
	 * Sets whether the preferences file should be deleted.
	 *
	 * @param bIsEnabled	true if the preferences should be deleted.
	 */
	void ForceDeletePreferences(bool bIsEnabled)
	{
		bDeletePreferences = bIsEnabled;
	}

	/** Returns true if preferences should be deleted. */
	bool IsDeletePreferences()
	{
		return bDeletePreferences;
	}

	/** Opens the specified project file or game. Restarts the editor */
	void SwitchProject(const FString& GameOrProjectFileName, bool bWarn = true);

	/** Restarts the editor, reopening the current project, if any */
	void RestartEditor(bool bWarn = true);

	/** Ticks the performance analytics used by the analytics heartbeat */
	void TickPerformanceAnalytics();

	/** Triggers asset analytics if it hasn't been run yet */
	void TickAssetAnalytics();

	/**
	 * Fetches a URL from the config and optionally switches it to rocket if required
	 *
	 * @param InKey			The key to lookup in the config file
	 * @param OutURL		The URL string listed for the key in the config
	 * @param bCheckRocket	if true, will attempt to change the URL from udn to rocket if possible
	 *
	 * @returns true if successful
	 */
	bool GetURL( const TCHAR* InKey, FString& OutURL, const bool bCheckRocket = false ) const;

	/** Returns the editor executable to use to execute commandlets */
	FString GetExecutableForCommandlets() const;

	/** 
	 * Opens the Unreal Engine Launcher marketplace page
	 *
	 * @param CustomLocation	Optional custom location within the marketplace to navigate to.  If not specified the launcher will open to the root marketplace page
	 */
	void OpenMarketplace(const FString& CustomLocation = TEXT(""));

	/** Constructor, private - use Get() function */
	FUnrealEdMisc();

	/** Displays a property dialog based upon what is currently selected. If any actors are selected, the actor property dialog is displayed. */
	void CB_SelectedProps();
	void CB_DisplayLoadErrors();
	void CB_RefreshEditor();
	void PreSaveWorld(uint32 SaveFlags, class UWorld* World);

	/** Tells the editor that something has been done to change the map.  Can be anything from loading a whole new map to changing the BSP. */
	void CB_MapChange( uint32 InFlags );
	void CB_RedrawAllViewports();
	void CB_EditorModeWindowClosed(const TSharedRef<SWindow>&);
	void CB_LevelActorsAdded(class AActor* InActor);

	/** Called right before unit testing is about to begin */
	void CB_PreAutomationTesting();

	/** Called right after unit testing concludes */
	void CB_PostAutomationTesting();

	void OnEditorChangeMode(FEditorModeID NewEditorMode);
	void OnEditorPreModal();
	void OnEditorPostModal();

	/** Called from tab manager when the tab changes */
	void OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated);
	void OnTabForegrounded(TSharedPtr<SDockTab> ForegroundTab, TSharedPtr<SDockTab> BackgroundTab);
	void OnUserActivityTabChanged(TSharedPtr<SDockTab> InTab);

	/** Delegate that gets called by modules that can't directly access Engine */
	void OnDeferCommand( const FString& DeferredCommand );

	/** Start the performance survey that attempts to monitor editor performance in the default or simple startup maps */
	void BeginPerformanceSurvey();

	/** End the performance survey that attempts to monitor editor performance in the default or simple startup maps */
	void CancelPerformanceSurvey();

	/**
	 * Called when a map is changed (loaded,saved,new map, etc)
	 */
	void OnMapChanged( UWorld* World, EMapChangeType MapChangeType );

	/** Called when the input manager records a user-defined chord */
	void OnUserDefinedChordChanged(const FUICommandInfo& CommandInfo);

	/** Delegate for (default) message log UObject token activation - selects the object that the token refers to (if any) */
	void OnMessageTokenActivated(const TSharedRef<class IMessageToken>& Token);

	/** Delegate for (default) display name of UObject tokens. Can display the name of the actor if an object is/is part of one */
	FText OnGetDisplayName(const UObject* InObject, const bool bFullPath);

	/** Delegate for (default) message log message selection - selects the objects that the tokens refer to (if any) */
	void OnMessageSelectionChanged(TArray< TSharedRef<class FTokenizedMessage> >& Selection);

	/** Delegate for URL generation, used to generate UDN pages */
	FString GenerateURL(const FString& InUDNPage);

	/** Delegate used to go to assets in the content browser */
	void OnGotoAsset(const FString& InAssetPath) const;

	/** Delegate used to update the map of asset update counts */
	void OnObjectSaved(UObject* SavedObject);

	/** Delegate used to update the map of asset update counts (for UWorlds specifically) */
	void OnWorldSaved(uint32 SaveFlags, UWorld* SavedWorld);

	/** Logs an update to an asset */
	void LogAssetUpdate(UObject* UpdatedAsset);

	/** Initialize engine analytics */
	void InitEngineAnalytics();

	/** Called when the heartbeat event should be sent to engine analytics */
	void EditorAnalyticsHeartbeat();

	/** Handles "Enable World Composition" option in WorldSettings */
	bool EnableWorldComposition(UWorld* InWorld, bool bEnable);

private:

	/** The current state of the autosave */
	EAutosaveState::Type AutosaveState;

	/** Stores whether or not the current map build was cancelled. */
	bool bCancelBuild;

	/** Whenther the system has been initialised */
	bool bInitialized;

	/** The name of a pending project.  When the editor shuts down it will switch to this project if not empty */ 
	FString PendingProjectName;

	/** Map of config->backup filenames to restore on shutdown of the editor */
	TMap<FString, FString> RestoreConfigFiles;

	/** true if the layout should be saved when closing the editor. */
	bool bSaveLayoutOnClose;

	/** true if the preferences config file should be deleted. */
	bool bDeletePreferences;

	/** true if editor performance is being monitored */
	bool bIsSurveyingPerformance;

	/** true if an asset analytics pass is pending */
	bool bIsAssetAnalyticsPending;

	/** The time that the last performance survey frame rate sample happened */
	FDateTime LastFrameRateTime;

	/** An array of frame rate samples used by the performance survey */
	TArray<float> FrameRateSamples;

	/** Statistical information needed by the analytics to report editor performance */
	TUniquePtr<FPerformanceAnalyticsStats> PerformanceAnalyticsStats;

	/** handler to notify about navigation building process */
	TSharedPtr<FTickableEditorObject> NavigationBuildingNotificationHandler;

	/** Package names and the number of times they have been updated */
	TMap<FName, uint32> NumUpdatesByAssetName;

	/** Handle to the registered OnUserDefinedChordChanged delegate. */
	FDelegateHandle OnUserDefinedChordChangedDelegateHandle;

	/** Handle to the registered OnMapChanged delegate. */
	FDelegateHandle OnMapChangedDelegateHandle;
	
	/** Handle to the registered OnActiveTabChanged delegate. */
	FDelegateHandle OnActiveTabChangedDelegateHandle;

	/** Handle to the registered OnTabForegrounded delegate. */
	FDelegateHandle OnTabForegroundedDelegateHandle;

	FTimerHandle EditorAnalyticsHeartbeatTimerHandle;	
};
