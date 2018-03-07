// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IWebBrowserSingleton;

/**
 * WebBrowser initialization settings, can be used to override default init behaviors.
 */
struct WEBBROWSER_API FWebBrowserInitSettings
{
public:
	/**
	 * Default constructor. Initializes all members with default behavior values.
	 */
	FWebBrowserInitSettings();

	// The string which is appended to the browser's user-agent value.
	FString ProductVersion;
};

/**
 * WebBrowserModule interface
 */
class IWebBrowserModule : public IModuleInterface
{
public:
	/**
	 * Get or load the Web Browser Module
	 * 
	 * @return The loaded module
	 */
	static inline IWebBrowserModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IWebBrowserModule >("WebBrowser");
	}
	
	/**
	 * Check whether the module has already been loaded
	 * 
	 * @return True if the module is loaded
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("WebBrowser");
	}

	/**
	 * Customize initialization settings. You must call this before the first GetSingleton call, in order to override init settings.
	 * 
	 * @param WebBrowserInitSettings The custom settings.
	 * @return true if the settings were used to initialize the singleton. False if the call was ignored due to singleton already existing.
	 */
	virtual bool CustomInitialize(const FWebBrowserInitSettings& WebBrowserInitSettings) = 0;

	/**
	 * Get the Web Browser Singleton
	 * 
	 * @return The Web Browser Singleton
	 */
	virtual IWebBrowserSingleton* GetSingleton() = 0;
};
