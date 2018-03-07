// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "Misc/AssertionMacros.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Misc/Guid.h"
#include "QoSReporterPrivate.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "AnalyticsBuildType.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Interfaces/IHttpResponse.h"
#include "Modules/ModuleManager.h"
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/EngineVersion.h"
#include "QoSReporter.h"

#if USE_SERVER_PERF_COUNTERS
	#include "PerfCountersModule.h"
	#include "Net/PerfCountersHelpers.h"
#endif

DEFINE_LOG_CATEGORY(LogQoSReporter);

IMPLEMENT_MODULE(FQoSReporterModule, QoSReporter);

// helps to version QoS events (date*10 to allow for 10 revisions per day)
#define QOS_EVENTS_REVISION					201602160

FString FQoSReporterModule::Config::GetDefaultAppVersion()
{ 
	return FString::Printf(TEXT("UE4-CL-%d"), FEngineVersion::Current().GetChangelist());
}

FString FQoSReporterModule::Config::GetDefaultAppEnvironment() 
{
	return AnalyticsBuildTypeToString(GetAnalyticsBuildType()); 
}

class FAnalyticsProviderQoSReporter : public IAnalyticsProvider
{
public:
	FAnalyticsProviderQoSReporter(const FQoSReporterModule::Config& ConfigValues);

	/** This provider does not have a concept of sessions */
	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override { return true; };
	/** This provider does not have a concept of sessions */
	virtual void EndSession() override {};
	/** This provider is not supposed to send many events, and due to nature of QoS we don't want to cache them */
	virtual void FlushEvents() override {};

	/** This provider is not using user IDs */
	virtual void SetUserID(const FString& InUserID) override {};
	/** This provider is not using user IDs, but we're (ab)using this API to return InstanceId */
	virtual FString GetUserID() const override { return InstanceId.ToString(); };

	/** This provider does not have a concept of sessions */
	virtual FString GetSessionID() const override { checkf(false, TEXT("FAnalyticsProviderQoSReporter is not session based"));  return TEXT("UnknownSessionId"); };
	/** This provider does not have a concept of sessions */
	virtual bool SetSessionID(const FString& InSessionID) override { return false; };

	/** We're (ab)using this API to set DeploymentName */
	virtual void SetLocation(const FString& InLocation) override { DeploymentName = InLocation; };

	virtual void RecordEvent(const FString& InEventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual ~FAnalyticsProviderQoSReporter();

	FString GetAPIKey() const { return APIKey; }

private:

	/** API key (also known as "upload type" on data router) */
	FString APIKey;
	/** API Server to use (also known as "endpoint"). */
	FString APIServer;
	/** The AppVersion to use. */
	FString AppVersion;
	/** The AppEnvironment to use. */
	FString AppEnvironment;
	/** The upload type to use. */
	FString UploadType;

	/** Unique identifier for this QoS reporter instance (only changed on module initialization) */
	FGuid InstanceId;
	/** Deployment name (if empty, it won't be sent). */
	FString DeploymentName;

	/**
	 * Delegate called when an event Http request completes
	 */
	void EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	/**
	 * Returns application role (server, client)
	 */
	static FString GetApplicationRole();
};

void FQoSReporterModule::StartupModule()
{
	// FQoSReporter::Initialize() is expected to be called by game code with proper config
}

void FQoSReporterModule::ShutdownModule()
{
	FQoSReporter::Shutdown();
}

TSharedPtr<IAnalyticsProvider> FQoSReporterModule::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		Config ConfigValues;
		ConfigValues.APIServer = GetConfigValue.Execute(Config::GetKeyNameForAPIServer(), true);
		ConfigValues.APIKey = GetConfigValue.Execute(Config::GetKeyNameForAPIKey(), false);
		ConfigValues.AppVersion = GetConfigValue.Execute(Config::GetKeyNameForAppVersion(), false);
		ConfigValues.AppEnvironment = GetConfigValue.Execute(Config::GetKeyNameForAppEnvironment(), false);
		ConfigValues.UploadType = GetConfigValue.Execute(Config::GetKeyNameForUploadType(), false);
		return CreateAnalyticsProvider(ConfigValues);
	}
	else
	{
		UE_LOG(LogQoSReporter, Warning, TEXT("CreateAnalyticsProvider called with an unbound delegate"));
	}
	return nullptr;
}

TSharedPtr<IAnalyticsProvider> FQoSReporterModule::CreateAnalyticsProvider(const Config& ConfigValues) const
{
	return TSharedPtr<IAnalyticsProvider>(new FAnalyticsProviderQoSReporter(ConfigValues));
}

/**
 * Perform any initialization.
 */
FAnalyticsProviderQoSReporter::FAnalyticsProviderQoSReporter(const FQoSReporterModule::Config& ConfigValues)
{
	UE_LOG(LogQoSReporter, Verbose, TEXT("Initializing QoS Reporter"));

	APIKey = ConfigValues.APIKey;
	if (APIKey.IsEmpty())
	{
		UE_LOG(LogQoSReporter, Error, TEXT("QoS API key is not configured, no QoS metrics will be reported."));
	}

	APIServer = ConfigValues.APIServer;
	if (APIServer.IsEmpty())
	{
		UE_LOG(LogQoSReporter, Error, TEXT("QoS API server is not configured, no QoS metrics will be reported."));
	}

	AppVersion = ConfigValues.AppVersion;
	if (AppVersion.IsEmpty())
	{
		AppVersion = ConfigValues.GetDefaultAppVersion();
	}

	AppEnvironment = ConfigValues.AppEnvironment;
	if (AppEnvironment.IsEmpty())
	{
		AppEnvironment = ConfigValues.GetDefaultAppEnvironment();
	}

	UploadType = ConfigValues.UploadType;
	if (UploadType.IsEmpty())
	{
		UploadType = ConfigValues.GetDefaultUploadType();
	}

	// add a unique id
	InstanceId = FGuid::NewGuid();
	FPlatformMisc::CreateGuid(InstanceId);

	UE_LOG(LogQoSReporter, Log, TEXT("QoSReporter initialized (InstanceId = '%s', SystemId = '%s')"), *InstanceId.ToString(), *FPlatformMisc::GetOperatingSystemId());
	UE_LOG(LogQoSReporter, Log, TEXT("APIKey = '%s'. APIServer = '%s'. AppVersion = '%s'. AppEnvironment = '%s'"), *APIKey, *APIServer, *AppVersion, *AppEnvironment);
}

FAnalyticsProviderQoSReporter::~FAnalyticsProviderQoSReporter()
{
	UE_LOG(LogQoSReporter, Verbose, TEXT("Destroying QoS Reporter"));
	EndSession();
}

void FAnalyticsProviderQoSReporter::RecordEvent(const FString& InEventName, const TArray<FAnalyticsEventAttribute>& InAttributes)
{
	if (APIKey.IsEmpty() || APIServer.IsEmpty())
	{
		return;
	}

	// for data router, it is actually preferable to have different events than attach more attributes, so
	// append application role to the event name instead
	FString EventName = InEventName;
	EventName += TEXT(".");
	EventName += GetApplicationRole();

	// add attributes common to each QoS event first
	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Add(FAnalyticsEventAttribute(TEXT("QoSRevision"), QOS_EVENTS_REVISION));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("SystemId"), FPlatformMisc::GetOperatingSystemId()));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("InstanceId"), InstanceId.ToString()));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Platform"), FString(FPlatformProperties::PlatformName())));
	if (LIKELY(DeploymentName.Len() > 0))
	{
		Attributes.Add(FAnalyticsEventAttribute(TEXT("Deployment"), DeploymentName));
	}
	else
	{
		UE_LOG(LogQoSReporter, Warning, TEXT("QoSReporter was not configured for any deployment; metrics will be likely discarded."));
	}

	// append the rest
	Attributes += InAttributes;

	// encode params as Json
	if (ensure(FModuleManager::Get().IsModuleLoaded("HTTP")))
	{
		FString Payload;

		FDateTime CurrentTime = FDateTime::UtcNow();

		TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&Payload);
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteArrayStart(TEXT("Events"));

		// write just a single event
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue(TEXT("EventName"), EventName);

		if (Attributes.Num() > 0)
		{
			// optional attributes for this event
			for (int32 AttrIdx = 0; AttrIdx < Attributes.Num(); AttrIdx++)
			{
				const FAnalyticsEventAttribute& Attr = Attributes[AttrIdx];
				JsonWriter->WriteValue(Attr.AttrName, Attr.ToString());
			}
		}
		JsonWriter->WriteObjectEnd();

		JsonWriter->WriteArrayEnd();
		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();

		FString URLPath = FString::Printf(TEXT("?AppID=%s&AppVersion=%s&AppEnvironment=%s&UploadType=%s"),
			*FGenericPlatformHttp::UrlEncode(APIKey),
			*FGenericPlatformHttp::UrlEncode(AppVersion),
			*FGenericPlatformHttp::UrlEncode(AppEnvironment),
			*FGenericPlatformHttp::UrlEncode(UploadType)
			);

		// Recreate the URLPath for logging because we do not want to escape the parameters when logging.
		// We cannot simply UrlEncode the entire Path after logging it because UrlEncode(Params) != UrlEncode(Param1) & UrlEncode(Param2) ...
		UE_LOG(LogQoSReporter, VeryVerbose, TEXT("[%s] QoS URL:%s?AppID=%s&AppVersion=%s&AppEnvironment=%s&UploadType=%s. Payload:%s"),
			*APIKey,
			*APIServer,
			*APIKey,
			*AppVersion,
			*AppEnvironment,
			*UploadType,
			*Payload);

		// Create/send Http request for an event
		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));

		HttpRequest->SetURL(APIServer + URLPath);
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetContentAsString(Payload);
		HttpRequest->OnProcessRequestComplete().BindRaw(this, &FAnalyticsProviderQoSReporter::EventRequestComplete);
		HttpRequest->ProcessRequest();

	}
}

void FAnalyticsProviderQoSReporter::EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	bool bCountTowardsFailedAttempts = true;

	if (bSucceeded && HttpResponse.IsValid())
	{
		// normal operation is silent, but any problems are reported as warnings
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			UE_LOG(LogQoSReporter, VeryVerbose, TEXT("QoS response for [%s]. Code: %d. Payload: %s"),
				*HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());

			bCountTowardsFailedAttempts = false;
		}
		else
		{
			UE_LOG(LogQoSReporter, Warning, TEXT("Bad QoS response for [%s] - code: %d. Payload: %s"),
				*HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
		}
	}
	else
	{
		// if we cannot report QoS metrics this is pretty bad; report at least a warning
		UE_LOG(LogQoSReporter, Warning, TEXT("QoS response for [%s]. No response"), *HttpRequest->GetURL());
	}

	if (bCountTowardsFailedAttempts)
	{
		// FIXME: should use retrial with exponential backoff here
#if USE_SERVER_PERF_COUNTERS && !IS_PROGRAM
		PerfCountersIncrement(TEXT("FailedQoSRequests"), 0, IPerfCounters::Flags::Transient);
#endif // USE_SERVER_PERF_COUNTERS
	}
}

/**
 * Returns application role (server, client)
 */
FString FAnalyticsProviderQoSReporter::GetApplicationRole()
{
	if (IsRunningDedicatedServer())
	{
		static FString DedicatedServer(TEXT("DedicatedServer"));
		return DedicatedServer;
	}
	else if (IsRunningClientOnly())
	{
		static FString ClientOnly(TEXT("ClientOnly"));
		return ClientOnly;
	}
	else if (IsRunningGame())
	{
		static FString StandaloneGame(TEXT("StandaloneGame"));
		return StandaloneGame;
	}

	static FString Editor(TEXT("Editor"));
	return Editor;
}

