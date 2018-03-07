// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ContentBrowserSettings.h: Declares the UContentBrowserSettings class.
=============================================================================*/

#pragma once


#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ContentBrowserSettings.generated.h"

/**
 * Implements content browser settings.  These are global not per-project
 */
UCLASS(config=EditorSettings)
class UNREALED_API UContentBrowserSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** The number of objects to load at once in the Content Browser before displaying a warning about loading many assets */
	UPROPERTY(EditAnywhere, config, Category=ContentBrowser, meta=(DisplayName = "Assets to Load at Once Before Warning"))
	int32 NumObjectsToLoadBeforeWarning;

	/** Whether the Content Browser should open the Sources Panel by default */
	UPROPERTY(EditAnywhere, config, Category = ContentBrowser)
	bool bOpenSourcesPanelByDefault;

	/** Whether to render thumbnails for loaded assets in real-time in the Content Browser */
	UPROPERTY(config)
	bool RealTimeThumbnails;

	/** Whether to display folders in the asset view of the content browser. Note that this implies 'Show Only Assets in Selected Folders'. */
	UPROPERTY(config)
	bool DisplayFolders;

	/** Whether to empty display folders in the asset view of the content browser. */
	UPROPERTY(config)
	bool DisplayEmptyFolders;

public:

	/** Sets whether we are allowed to display the engine folder or not, optional flag for setting override instead */
	void SetDisplayEngineFolder( bool bInDisplayEngineFolder, bool bOverride = false )
	{ 
		bOverride ? OverrideDisplayEngineFolder = bInDisplayEngineFolder : DisplayEngineFolder = bInDisplayEngineFolder;
	}

	/** Gets whether we are allowed to display the engine folder or not, optional flag ignoring the override */
	bool GetDisplayEngineFolder( bool bExcludeOverride = false ) const
	{ 
		return ( ( bExcludeOverride ? false : OverrideDisplayEngineFolder ) || DisplayEngineFolder );
	}

	/** Sets whether we are allowed to display the developers folder or not, optional flag for setting override instead */
	void SetDisplayDevelopersFolder( bool bInDisplayDevelopersFolder, bool bOverride = false )
	{ 
		bOverride ? OverrideDisplayDevelopersFolder = bInDisplayDevelopersFolder : DisplayDevelopersFolder = bInDisplayDevelopersFolder;
	}

	/** Gets whether we are allowed to display the developers folder or not, optional flag ignoring the override */
	bool GetDisplayDevelopersFolder( bool bExcludeOverride = false ) const
	{ 
		return ( ( bExcludeOverride ? false : OverrideDisplayDevelopersFolder ) || DisplayDevelopersFolder );
	}

	/** Sets whether we are allowed to display the L10N folder (contains localized assets) or not */
	void SetDisplayL10NFolder(bool bInDisplayL10NFolder)
	{
		DisplayL10NFolder = bInDisplayL10NFolder;
	}

	/** Gets whether we are allowed to display the L10N folder (contains localized assets) or not */
	bool GetDisplayL10NFolder() const
	{
		return DisplayL10NFolder;
	}

	/** Sets whether we are allowed to display the plugin folders or not, optional flag for setting override instead */
	void SetDisplayPluginFolders( bool bInDisplayPluginFolders, bool bOverride = false )
	{ 
		bOverride ? OverrideDisplayPluginFolders = bInDisplayPluginFolders : DisplayPluginFolders = bInDisplayPluginFolders;
	}

	/** Gets whether we are allowed to display the plugin folders or not, optional flag ignoring the override */
	bool GetDisplayPluginFolders( bool bExcludeOverride = false ) const
	{ 
		return ( ( bExcludeOverride ? false : OverrideDisplayPluginFolders ) || DisplayPluginFolders );
	}

	/** Sets whether we are allowed to display collection folders or not */
	void SetDisplayCollections(bool bInDisplayCollections)
	{
		DisplayCollections = bInDisplayCollections;
	}

	/** Gets whether we are allowed to display the collection folders or not*/
	bool GetDisplayCollections() const
	{
		return DisplayCollections;
	}

	/** Sets whether we are allowed to display C++ folders or not */
	void SetDisplayCppFolders(bool bDisplay)
	{
		DisplayCppFolders = bDisplay;
	}

	/** Gets whether we are allowed to display the C++ folders or not*/
	bool GetDisplayCppFolders() const
	{
		return DisplayCppFolders;
	}

	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UContentBrowserSettings, FSettingChangedEvent, FName /*PropertyName*/);
	static FSettingChangedEvent& OnSettingChanged( ) { return SettingChangedEvent; }

protected:

	// UObject overrides

	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;

private:

	/** Whether to display the engine folder in the assets view of the content browser. */
	UPROPERTY(config)
	bool DisplayEngineFolder;

	/** If true, overrides the DisplayEngine setting */
	bool OverrideDisplayEngineFolder;

	/** Whether to display the developers folder in the path view of the content browser */
	UPROPERTY(config)
	bool DisplayDevelopersFolder;

	UPROPERTY(config)
	bool DisplayL10NFolder;

	/** If true, overrides the DisplayDev setting */
	bool OverrideDisplayDevelopersFolder;

	/** List of plugin folders to display in the content browser. */
	UPROPERTY(config)
	bool DisplayPluginFolders;

	/** Temporary override for the DisplayPluginFolders setting */
	bool OverrideDisplayPluginFolders;

	UPROPERTY(config)
	bool DisplayCollections;

	UPROPERTY(config)
	bool DisplayCppFolders;

	// Holds an event delegate that is executed when a setting has changed.
	static FSettingChangedEvent SettingChangedEvent;
};
