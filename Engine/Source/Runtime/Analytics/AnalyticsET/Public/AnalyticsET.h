// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IAnalyticsProviderModule.h"

class IAnalyticsProvider;
class IAnalyticsProviderET;



class IAnalyticsProviderET;

/**
 *  Public implementation of EpicGames.MCP.AnalyticsProvider
 */
class FAnalyticsET : public IAnalyticsProviderModule
{
	//--------------------------------------------------------------------------
	// Module functionality
	//--------------------------------------------------------------------------
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FAnalyticsET& Get()
	{
		return FModuleManager::LoadModuleChecked< FAnalyticsET >( "AnalyticsET" );
	}

	//--------------------------------------------------------------------------
	// Configuration functionality
	//--------------------------------------------------------------------------
public:
	/** Defines required configuration values for ET analytics provider. */
	struct Config
	{
		/** ET APIKey - Get from your account manager */
		FString APIKeyET;
		/** ET API Server - Base URL to send events. */
		FString APIServerET;
		/** 
		 * AppVersion - defines the app version passed to the provider. By default this will be FEngineVersion::Current(), but you can supply your own. 
		 * As a convenience, you can use -AnalyticsAppVersion=XXX to force the AppVersion to a specific value. Useful for playtest etc where you want to define a specific version string dynamically.
		 * If you supply your own Version string, occurrences of "%VERSION%" are replaced with FEngineVersion::Current(). ie, -AnalyticsAppVersion=MyCustomID-%VERSION%.
		 */
		FString AppVersionET;
		/** When true, sends events using the legacy ET protocol that passes all attributes as URL parameters. Defaults to false. */
		bool UseLegacyProtocol = false;
		/** When true (default), events are dropped if flush fails */
		bool bDropEventsOnFlushFailure = true;
		/** The AppEnvironment that the data router should use. Defaults to GetDefaultAppEnvironment. */
		FString AppEnvironment;
		/** The UploadType that the data router should use. Defaults to GetDefaultUploadType. */
		FString UploadType;

		/** Default ctor to ensure all values have their proper default. */
		Config() : UseLegacyProtocol(false) {}
		/** Ctor exposing common configurables . */
		Config(FString InAPIKeyET, FString InAPIServerET, FString InAppVersionET = FString(), bool InUseLegacyProtocol = false, FString InAppEnvironment = FString(), FString InUploadType = FString()) 
			: APIKeyET(MoveTemp(InAPIKeyET))
			, APIServerET(MoveTemp(InAPIServerET))
			, AppVersionET(MoveTemp(InAppVersionET))
			, UseLegacyProtocol(InUseLegacyProtocol)
			, AppEnvironment(MoveTemp(InAppEnvironment))
			, UploadType(MoveTemp(InUploadType))
		{}

		/** KeyName required for APIKey configuration. */
		static FString GetKeyNameForAPIKey() { return TEXT("APIKeyET"); }
		/** KeyName required for APIServer configuration. */
		static FString GetKeyNameForAPIServer() { return TEXT("APIServerET"); }
		/** KeyName required for AppVersion configuration. */
		static FString GetKeyNameForAppVersion() { return TEXT("AppVersionET"); }
		/** Optional parameter to use the legacy backend protocol. */
		static FString GetKeyNameForUseLegacyProtocol() { return TEXT("UseLegacyProtocol"); }
		/** For the the data router backend protocol. */
		static FString GetKeyNameForAppEnvironment() { return TEXT("AppEnvironment"); }
		/** For the the data router backend protocol. */
		static FString GetKeyNameForUploadType() { return TEXT("UploadType"); }
		/** Default value if no APIServer configuration is provided. */
		static FString GetDefaultAppEnvironment() { return TEXT("datacollector-binary"); }
		/** Default value if no UploadType is given, and UseDataRouter protocol is specified. */
		static FString GetDefaultUploadType() { return TEXT("eteventstream"); }
	};

	//--------------------------------------------------------------------------
	// provider factory functions
	//--------------------------------------------------------------------------
public:
	/**
	 * IAnalyticsProviderModule interface.
	 * Creates the analytics provider given a configuration delegate.
	 * The keys required exactly match the field names in the Config object. 
	 */
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const override;
	
	/** 
	 * Construct an ET analytics provider directly from a config object.
	 */
	virtual TSharedPtr<IAnalyticsProviderET> CreateAnalyticsProvider(const Config& ConfigValues) const;

private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
