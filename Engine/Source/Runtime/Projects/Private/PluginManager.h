// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PluginDescriptor.h"
#include "Interfaces/IPluginManager.h"

/**
 * Instance of a plugin in memory
 */
class FPlugin final : public IPlugin
{
public:
	/** The name of the plugin */
	FString Name;

	/** The filename that the plugin was loaded from */
	FString FileName;

	/** The plugin's settings */
	FPluginDescriptor Descriptor;

	/** Type of plugin */
	EPluginType Type;

	/** True if the plugin is marked as enabled */
	bool bEnabled;

	/**
	 * FPlugin constructor
	 */
	FPlugin(const FString &FileName, const FPluginDescriptor& InDescriptor, EPluginType InType);

	/**
	 * Destructor.
	 */
	virtual ~FPlugin();

	/* IPluginInfo interface */
	virtual FString GetName() const override;
	virtual FString GetDescriptorFileName() const override;
	virtual FString GetBaseDir() const override;
	virtual FString GetContentDir() const override;
	virtual FString GetMountedAssetPath() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsEnabledByDefault() const override;
	virtual bool IsHidden() const override;
	virtual bool CanContainContent() const override;
	virtual EPluginType GetType() const override;
	virtual EPluginLoadedFrom GetLoadedFrom() const override;
	virtual const FPluginDescriptor& GetDescriptor() const override;
	virtual bool UpdateDescriptor(const FPluginDescriptor& NewDescriptor, FText& OutFailReason) override;
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS
/**
 * FPluginManager manages available code and content extensions (both loaded and not loaded.)
 */
class FPluginManager final : public IPluginManager
{
public:
	/** Constructor */
	FPluginManager();

	/** Destructor */
	~FPluginManager();

	/** IPluginManager interface */
	virtual void RefreshPluginsList() override;
	virtual bool LoadModulesForEnabledPlugins( const ELoadingPhase::Type LoadingPhase ) override;
	virtual void GetLocalizationPathsForEnabledPlugins( TArray<FString>& OutLocResPaths ) override;
	virtual void SetRegisterMountPointDelegate( const FRegisterMountPointDelegate& Delegate ) override;
	virtual bool AreRequiredPluginsAvailable() override;
	virtual bool CheckModuleCompatibility( TArray<FString>& OutIncompatibleModules ) override;
	virtual TSharedPtr<IPlugin> FindPlugin(const FString& Name) override;
	virtual TArray<TSharedRef<IPlugin>> GetEnabledPlugins() override;
	virtual TArray<TSharedRef<IPlugin>> GetDiscoveredPlugins() override;
	virtual TArray< FPluginStatus > QueryStatusForAllPlugins() const override;
	virtual void AddPluginSearchPath(const FString& ExtraDiscoveryPath, bool bRefresh = true) override;
	virtual TArray<TSharedRef<IPlugin>> GetPluginsWithPakFile() const override;
	virtual FNewPluginMountedEvent& OnNewPluginMounted() override;
	virtual void MountNewlyCreatedPlugin(const FString& PluginName) override;

private:

	/** Searches for all plugins on disk and builds up the array of plugin objects.  Doesn't load any plugins. 
	    This is called when the plugin manager singleton is first accessed. */
	void DiscoverAllPlugins();

	/** Reads all the plugin descriptors */
	static void ReadAllPlugins(TMap<FString, TSharedRef<FPlugin>>& Plugins, const TSet<FString>& ExtraSearchPaths);

	/** Reads all the plugin descriptors from disk */
	static void ReadPluginsInDirectory(const FString& PluginsDirectory, const EPluginType Type, TMap<FString, TSharedRef<FPlugin>>& Plugins);

	/** Creates a FPlugin object and adds it to the given map */
	static void CreatePluginObject(const FString& FileName, const FPluginDescriptor& Descriptor, const EPluginType Type, TMap<FString, TSharedRef<FPlugin>>& Plugins);

	/** Finds all the plugin descriptors underneath a given directory */
	static void FindPluginsInDirectory(const FString& PluginsDirectory, TArray<FString>& FileNames);

	/** Finds all the plugin manifests in a given directory */
	static void FindPluginManifestsInDirectory(const FString& PluginManifestDirectory, TArray<FString>& FileNames);

	/** Sets the bPluginEnabled flag on all plugins found from DiscoverAllPlugins that are enabled in config */
	bool ConfigureEnabledPlugins();

	/** Adds a single enabled plugin, and all its dependencies */
	bool ConfigureEnabledPlugin(const FPluginReferenceDescriptor& FirstReference, TSet<FString>& EnabledPluginNames);

	/** Prompts the user to download a missing plugin from the given URL */
	static bool PromptToDownloadPlugin(const FString& PluginName, const FString& MarketplaceURL);

	/** Prompts the user to disable a plugin */
	static bool PromptToDisableMissingPlugin(const FString& PluginName, const FString& MissingPluginName);

	/** Prompts the user to disable a plugin */
	static bool PromptToDisableIncompatiblePlugin(const FString& PluginName, const FString& IncompatiblePluginName);

	/** Prompts the user to disable a plugin */
	static bool PromptToDisablePlugin(const FText& Caption, const FText& Message, const FString& PluginName);

	/** Checks whether a plugin is compatible with the current engine version */
	static bool IsPluginCompatible(const FPlugin& Plugin);

	/** Prompts the user to disable a plugin */
	static bool PromptToLoadIncompatiblePlugin(const FPlugin& Plugin, const FString& ReferencingPluginName);

	/** Gets the instance of a given plugin */
	TSharedPtr<FPlugin> FindPluginInstance(const FString& Name);

private:
	/** All of the plugins that we know about */
	TMap< FString, TSharedRef< FPlugin > > AllPlugins;

	TArray<TSharedRef<IPlugin>> PluginsWithPakFile;

	/** Delegate for mounting content paths.  Bound by FPackageName code in CoreUObject, so that we can access
	    content path mounting functionality from Core. */
	FRegisterMountPointDelegate RegisterMountPointDelegate;

	/** Set when all the appropriate plugins have been marked as enabled */
	bool bHaveConfiguredEnabledPlugins;

	/** Set if all the required plugins are available */
	bool bHaveAllRequiredPlugins;

	/** List of additional directory paths to search for plugins within */
	TSet<FString> PluginDiscoveryPaths;

	/** Callback for notifications that a new plugin was mounted */
	FNewPluginMountedEvent NewPluginMountedEvent;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS


