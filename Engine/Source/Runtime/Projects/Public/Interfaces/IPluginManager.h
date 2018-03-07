// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PluginDescriptor.h"

/**
 * Enum for where a plugin is loaded from
 */
enum class EPluginLoadedFrom
{
	/** Plugin is built-in to the engine */
	Engine,

	/** Project-specific plugin, stored within a game project directory */
	Project
};

/**
 * Enum for the type of a plugin
 */
enum class EPluginType
{
	/** Plugin is built-in to the engine */
	Engine,

	/** Standard enterprise plugin */
	Enterprise,

	/** Project-specific plugin, stored within a game project directory */
	Project,

	/** Plugin found in an external directory (found in an AdditionalPluginDirectory listed in the project file, or referenced on the command line) */
	External,

	/** Project-specific mod plugin */
	Mod,
};


/**
 * Simple data structure that is filled when querying information about plug-ins.
 */
struct FPluginStatus
{
	/** The name of this plug-in. */
	FString Name;

	/** Path to plug-in directory on disk. */
	FString PluginDirectory;

	/** True if plug-in is currently enabled. */
	bool bIsEnabled;

	/** Where the plugin was loaded from */
	EPluginLoadedFrom LoadedFrom;

	/** The plugin descriptor */
	FPluginDescriptor Descriptor;
};


/**
 * Information about an enabled plugin.
 */
class IPlugin
{
public:
	/* Virtual destructor */
	virtual ~IPlugin(){}

	/**
	 * Gets the plugin name.
	 *
	 * @return Name of the plugin.
	 */
	virtual FString GetName() const = 0;

	/**
	 * Get a path to the plugin's descriptor
	 *
	 * @return Path to the plugin's descriptor.
	 */
	virtual FString GetDescriptorFileName() const = 0;

	/**
	 * Get a path to the plugin's directory.
	 *
	 * @return Path to the plugin's base directory.
	 */
	virtual FString GetBaseDir() const = 0;

	/**
	 * Get a path to the plugin's content directory.
	 *
	 * @return Path to the plugin's content directory.
	 */
	virtual FString GetContentDir() const = 0;

	/**
	 * Get the virtual root path for assets.
	 *
	 * @return The mounted root path for assets in this plugin's content folder; typically /PluginName/.
	 */
	virtual FString GetMountedAssetPath() const = 0;

	/**
	 * Gets the type of a plugin
	 *
	 * @return The plugin type
	 */
	virtual EPluginType GetType() const = 0;

	/**
	 * Determines if the plugin is enabled.
	 *
	 * @return True if the plugin is currently enabled.
	 */
	virtual bool IsEnabled() const = 0;

	/**
	 * Determines if the plugin is enabled by default.
	 *
	 * @return True if the plugin is currently enabled by default.
	 */
	virtual bool IsEnabledByDefault() const = 0;

	/**
	 * Determines if the plugin is should be displayed in-editor for the user to enable/disable freely.
	 *
	 * @return True if the plugin should be hidden.
	 */
	virtual bool IsHidden() const = 0;

	/**
	 * Determines if the plugin can contain content.
	 *
	 * @return True if the plugin can contain content.
	 */
	virtual bool CanContainContent() const = 0;

	/**
	 * Returns the plugin's location
	 *
	 * @return Where the plugin was loaded from
	 */
	virtual EPluginLoadedFrom GetLoadedFrom() const = 0;

	/**
	 * Gets the plugin's descriptor
	 *
	 * @return Reference to the plugin's descriptor
	 */
	virtual const FPluginDescriptor& GetDescriptor() const = 0;

	/**
	 * Updates the plugin's descriptor
	 *
	 * @param NewDescriptor The new plugin descriptor
	 * @param OutFailReason The error message if the plugin's descriptor could not be updated
	 * @return True if the descriptor was updated, false otherwise. 
	 */ 
	virtual bool UpdateDescriptor(const FPluginDescriptor& NewDescriptor, FText& OutFailReason) = 0;
};

/**
 * PluginManager manages available code and content extensions (both loaded and not loaded).
 */
class IPluginManager
{
public:
	virtual ~IPluginManager() { }

	/** 
	 * Updates the list of plugins.
	 */
	virtual void RefreshPluginsList() = 0;

	/**
	 * Loads all plug-ins
	 *
	 * @param	LoadingPhase	Which loading phase we're loading plug-in modules from.  Only modules that are configured to be
	 *							loaded at the specified loading phase will be loaded during this call.
	 */
	virtual bool LoadModulesForEnabledPlugins( const ELoadingPhase::Type LoadingPhase ) = 0;

	/**
	 * Get the localization paths for all enabled plugins.
	 *
	 * @param	OutLocResPaths	Array to populate with the localization paths for all enabled plugins.
	 */
	virtual void GetLocalizationPathsForEnabledPlugins( TArray<FString>& OutLocResPaths ) = 0;

	/** Delegate type for mounting content paths.  Used internally by FPackageName code. */
	DECLARE_DELEGATE_TwoParams( FRegisterMountPointDelegate, const FString& /* Root content path */, const FString& /* Directory name */ );

	/**
	 * Sets the delegate to call to register a new content mount point.  This is used internally by the plug-in manager system
	 * and should not be called by you.  This is registered at application startup by FPackageName code in CoreUObject.
	 *
	 * @param	Delegate	The delegate to that will be called when plug-in manager needs to register a mount point
	 */
	virtual void SetRegisterMountPointDelegate( const FRegisterMountPointDelegate& Delegate ) = 0;

	/**
	 * Checks if all the required plug-ins are available. If not, will present an error dialog the first time a plug-in is loaded or this function is called.
	 *
	 * @returns true if all the required plug-ins are available.
	 */
	virtual bool AreRequiredPluginsAvailable() = 0;

	/** 
	 * Checks whether modules for the enabled plug-ins are up to date.
	 *
	 * @param OutIncompatibleNames	Array to receive a list of incompatible module names.
	 * @returns true if the enabled plug-in modules are up to date.
	 */
	virtual bool CheckModuleCompatibility( TArray<FString>& OutIncompatibleModules ) = 0;

	/**
	 * Finds information for an enabled plugin.
	 *
	 * @return	 Pointer to the plugin's information, or nullptr.
	 */
	virtual TSharedPtr<IPlugin> FindPlugin(const FString& Name) = 0;

	/**
	 * Gets an array of all the enabled plugins.
	 *
	 * @return	Array of the enabled plugins.
	 */
	virtual TArray<TSharedRef<IPlugin>> GetEnabledPlugins() = 0;

	/**
	 * Gets an array of all the discovered plugins.
	 *
	 * @return	Array of the discovered plugins.
	 */
	virtual TArray<TSharedRef<IPlugin>> GetDiscoveredPlugins() = 0;

	/**
	 * Gets status about all currently known plug-ins.
	 *
	 * @return	 Array of plug-in status objects.
	 */
	DEPRECATED(4.18, "QueryStatusForAllPlugins() has been deprecated. Please use GetDiscoveredPlugins() instead.")
	virtual TArray<FPluginStatus> QueryStatusForAllPlugins() const = 0;

	/**
	 * Stores the specified path, utilizing it in future search passes when 
	 * searching for available plugins. Optionally refreshes the manager after 
	 * the new path has been added.
	 * 
	 * @param  ExtraDiscoveryPath	The path you want searched for additional plugins.
	 * @param  bRefresh				Signals the function to refresh the plugin database after the new path has been added
	 */
	virtual void AddPluginSearchPath(const FString& ExtraDiscoveryPath, bool bRefresh = true) = 0;

	/**
	 * Gets an array of plugins that loaded their own content pak file
	 */
	virtual TArray<TSharedRef<IPlugin>> GetPluginsWithPakFile() const = 0;

	/**
	 * Event signature for being notified that a new plugin has been mounted
	 */
	DECLARE_EVENT_OneParam(IPluginManager, FNewPluginMountedEvent, IPlugin&);

	/**
	 * Gets an array of plugins that loaded their own content pak file
	 */
	virtual FNewPluginMountedEvent& OnNewPluginMounted() = 0;

	/**
	 * Marks a newly created plugin as enabled, mounts its content and tries to load its modules
	 */
	virtual void MountNewlyCreatedPlugin(const FString& PluginName) = 0;

public:

	/**
	 * Static: Access singleton instance.
	 *
	 * @return	Reference to the singleton object.
	 */
	static PROJECTS_API IPluginManager& Get();
};
