// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/UnrealString.h"
#include "AnalyticsEventAttribute.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IAnalyticsProviderModule.h"

class IAnalyticsProvider;
struct FAnalyticsEventAttribute;

enum class EQoSEventParam : uint32
{
	/** Time from initializing the QoS reporter module to the "ready" state of the game/program (e.g. login screen) */
	StartupTime,

	/** Sent regularly, includes a number of attributes, different for client and server */
	Heartbeat,

	/** Sent regularly, contains server performance counters */
	ServerPerfCounters,

	MAX
};

namespace EQoSEvents
{
	inline const TCHAR * ToString(EQoSEventParam EventParam)
	{
		switch (EventParam)
		{
			case EQoSEventParam::StartupTime: 
				return TEXT("StartupTime");
			case EQoSEventParam::Heartbeat:
				return TEXT("Heartbeat");
			case EQoSEventParam::ServerPerfCounters:
				return TEXT("ServerPerfCounters");
			default:
				break;
		}

		return TEXT("Undefined");
	}
}

/**
 * The public interface to this module
 */
class FQoSReporterModule : public IAnalyticsProviderModule
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
	static inline FQoSReporterModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FQoSReporterModule >("QoSReporter");
	}

	//--------------------------------------------------------------------------
	// Configuration functionality
	//--------------------------------------------------------------------------
public:
	/** Defines required configuration values for Swrve analytics provider. */
	struct Config
	{
		/** Endpoint (URL) to send events to */
		FString APIServer;
		/** API key arbitrary string that identifies the application. */
		FString APIKey;
		/** An arbitrary string that identifies the version of the application. */
		FString AppVersion;
		/** An arbitrary string that allows to distinguish running environment of a single application (Development, Production, Staging etc). */
		FString AppEnvironment;
		/** An arbitrary string that identifies type of events (QoS). */
		FString UploadType;

		/** Key name for API server (endpoint) */
		static FString GetKeyNameForAPIServer() { return TEXT("APIServerQoS"); }
		/** Key name for API key. */
		static FString GetKeyNameForAPIKey() { return "APIKeyQoS"; }
		/** Key name for app version. */
		static FString GetKeyNameForAppVersion() { return "AppVersionQoS"; }
		/** Key name for app environment. */
		static FString GetKeyNameForAppEnvironment() { return "AppEnvironmentQoS"; }
		/** Key name for upload type. */
		static FString GetKeyNameForUploadType() { return "UploadTypeQoS"; }

		/** Returns default app version. */
		static FString GetDefaultAppVersion();
		/** Returns default app environment. */
		static FString GetDefaultAppEnvironment();
		/** Returns default upload type. */
		static FString GetDefaultUploadType() { return TEXT("ue4qosmetrics"); }
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
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const;
	
	/** 
	 * Construct an analytics provider directly from a config object.
	 */
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const Config& ConfigValues) const;

private:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

/**
 * The public interface for the QoS analytics provider singleton.
 *
 */
class FQoSReporter : FNoncopyable
{
public:
	/**
	 * Return the provider instance. Not valid outside of Initialize/Shutdown calls.
	 * Note: must check IsAvailable() first else this code will assert if the provider is not valid.
	 */
	QOSREPORTER_API static IAnalyticsProvider& GetProvider();
	/** Helper function to determine if the provider is valid. */
	QOSREPORTER_API static bool IsAvailable() { return Analytics.IsValid(); }
	/** Called to initialize the singleton. */
	QOSREPORTER_API static void Initialize();
	/** Called to shut down the singleton */
	QOSREPORTER_API static void Shutdown();


	/**
	 * These calls implement logic common to all games/programs; they are expected
	 * to sprinkle calls to them in appropriate place
	 */

	/**
	 * This function is expected to be called when a game reached the point of interactivity.
	 * It is allowed to call this function more than once.
	 */
	static void QOSREPORTER_API ReportStartupCompleteEvent();

	/**
	 * This function is expected to be called periodically to update ongoing tasks.
	 */
	static void QOSREPORTER_API Tick();

	/**
	 * Returns instance id that QoS reporter is using in its events
	 */
	static FString QOSREPORTER_API GetQoSReporterInstanceId();

	/** @return the backend deployment name that was previously set */
	static FString QOSREPORTER_API GetBackendDeploymentName();

	/**
	 * Backend services are split into deployments; this can be used to distinguish between them.
	 *
	 * @param InDeploymentName deployment name (has a meaning for the game and QoS alerting). Won't be sent if empty.
	 */
	static void QOSREPORTER_API SetBackendDeploymentName(const FString& InDeploymentName);

	/**
	 * Whether or not hitches should be counted
	 *
	 * @param bEnable true if yes, false if no (default)
	 */
	static void QOSREPORTER_API EnableCountingHitches(bool bEnable);

private:

	/**
	 * Sends heartbeat stats.
	 */
	static void SendHeartbeat();

	/**
	 * Adds server attributes for a heartbeat event.
	 */
	static void AddServerHeartbeatAttributes(TArray<FAnalyticsEventAttribute> & OutArray);

	/**
	 * Adds client attributes for a heartbeat event.
	 */
	static void AddClientHeartbeatAttributes(TArray<FAnalyticsEventAttribute> & OutArray);

	/** Whether the module has been initialized. */
	static bool bIsInitialized;

	/** Chosen analytics provider. */
	static TSharedPtr<IAnalyticsProvider> Analytics;

	/** Heartbeat interval in seconds */
	static double HeartbeatInterval;

	/** Timestamp when we sent the last heartbeat */
	static double LastHeartbeatTimestamp;

	/** Time when Tick() was called previously */
	static double PreviousTickTime;

	/**
	 * Event-specific variables
	 */
	/** Used for startup event */
	static double ModuleInitializationTime;

	/** Whether startup event was reported. */
	static bool bStartupEventReported;

	/** Whether or not counting hitches has been suppressed. */
	static bool bCountHitches;
};
