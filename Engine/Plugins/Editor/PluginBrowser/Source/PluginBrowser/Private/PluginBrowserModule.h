// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Modules/ModuleManager.h"
#include "IPluginBrowser.h"
#include "Widgets/SWindow.h"

class FSpawnTabArgs;

DECLARE_MULTICAST_DELEGATE(FOnNewPluginCreated);

class FPluginBrowserModule : public IPluginBrowser
{
public:
	/** Accessor for the module interface */
	static FPluginBrowserModule& Get()
	{
		return FModuleManager::Get().GetModuleChecked<FPluginBrowserModule>(TEXT("PluginBrowser"));
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Gets a delegate so that you can register/unregister to receive callbacks when plugins are created */
	FOnNewPluginCreated& OnNewPluginCreated() {return NewPluginCreatedDelegate;}

	/** Broadcasts callback to notify registrants that a plugin has been created */
	void BroadcastNewPluginCreated() const {NewPluginCreatedDelegate.Broadcast();}

	/**
	 * Sets whether a plugin is pending enable/disable
	 * @param PluginName The name of the plugin
	 * @param bCurrentlyEnabled The current state of this plugin, so that we can decide whether a change is no longer pending
	 * @param bPendingEnabled Whether to set this plugin to pending enable or disable
	 */
	void SetPluginPendingEnableState(const FString& PluginName, bool bCurrentlyEnabled, bool bPendingEnabled);

	/**
	 * Gets whether a plugin is pending enable/disable
	 * This should only be used when you know this is the case after using HasPluginPendingEnable
	 * @param PluginName The name of the plugin
	 */
	bool GetPluginPendingEnableState(const FString& PluginName) const;

	/** Whether there are any plugins pending enable/disable */
	bool HasPluginsPendingEnable() const {return PendingEnablePlugins.Num() > 0;}

	/**
	 * Whether a specific plugin is pending enable/disable
	 * @param PluginName The name of the plugin
	 */
	bool HasPluginPendingEnable(const FString& PluginName) const;

	/** Checks whether the given plugin should be displayed with a 'NEW' label */
	bool IsNewlyInstalledPlugin(const FString& PluginName) const;

	/** ID name for the plugins editor major tab */
	static const FName PluginsEditorTabName;

	/** ID name for the plugin creator tab */
	static const FName PluginCreatorTabName;

	/** Spawns the plugin creator tab with a specific wizard definition */
	virtual TSharedRef<SDockTab> SpawnPluginCreatorTab(const FSpawnTabArgs& SpawnTabArgs, TSharedPtr<IPluginWizardDefinition> PluginWizardDefinition) override;

private:
	/** Called to spawn the plugin browser tab */
	TSharedRef<SDockTab> HandleSpawnPluginBrowserTab(const FSpawnTabArgs& SpawnTabArgs);

	/** Called to spawn the plugin creator tab */
	TSharedRef<SDockTab> HandleSpawnPluginCreatorTab(const FSpawnTabArgs& SpawnTabArgs);

	/** Callback for the main frame finishing load */
	void OnMainFrameLoaded(TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow);

	/** Callback for when the user selects to edit installed plugins */
	void OnNewPluginsPopupSettingsClicked();

	/** Callback for when the user selects to edit installed plugins */
	void OnNewPluginsPopupDismissClicked();

	/** Updates the user's config file with the list of installed plugins that they've seen. */
	void UpdatePreviousInstalledPlugins();

	/** The spawned browser tab */
	TWeakPtr<SDockTab> PluginBrowserTab;

	/** List of plugins that are pending enable/disable */
	TMap<FString, bool> PendingEnablePlugins;

	/** List of all the installed plugins (as opposed to built-in engine plugins) */
	TArray<FString> InstalledPlugins;

	/** List of plugins that have been recently installed */
	TSet<FString> NewlyInstalledPlugins;

	/** Delegate called when a new plugin is created */
	FOnNewPluginCreated NewPluginCreatedDelegate;

	/** Notification popup that new plugins are available */
	TWeakPtr<SNotificationItem> NewPluginsNotification;
};
