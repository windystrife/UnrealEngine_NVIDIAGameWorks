// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IAnalyticsProviderET.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/OutputDeviceFile.h"
#include "Stats/Stats.h"
#include "Containers/Ticker.h"
#include "Misc/App.h"
#include "Misc/TimeGuard.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "Modules/ModuleManager.h"
#include "AnalyticsET.h"
#include "Analytics.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IHttpRequest.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Misc/EngineVersion.h"

/** When enabled (and -AnalyticsTrackPerf is specified on the command line, will log out analytics flush timings on a regular basis to Saved/AnalyticsTiming.csv. */
#define ANALYTICS_PERF_TRACKING_ENABLED !UE_BUILD_SHIPPING
#if ANALYTICS_PERF_TRACKING_ENABLED

/** Measures analytics bandwidth. Only active when -AnalyticsTrackPerf is specified on the command line. */
struct FAnalyticsPerfTracker : FTickerObjectBase
{
	FAnalyticsPerfTracker()
	{
		bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ANALYTICSTRACKPERF"));
		if (bEnabled)
		{
			LogFile.SetSuppressEventTag(true);
			LogFile.Serialize(TEXT("Date,CL,RunID,Time,WindowSeconds,ProfiledSeconds,Frames,Flushes,Events,Bytes"), ELogVerbosity::Log, FName());
			LastSubmitTime = StartTime;
			StartDate = FDateTime::UtcNow().ToIso8601();
			CL = Lex::ToString(FEngineVersion::Current().GetChangelist());
		}
	}

	/** Called once per flush */
	void RecordFlush(uint64 Bytes, uint64 NumEvents, double TimeSec)
	{
		if (bEnabled)
		{
			++FlushesThisWindow;
			BytesThisWindow += Bytes;
			NumEventsThisWindow += NumEvents;
			TimeThisWindow += TimeSec;
		}
	}

	static FAnalyticsPerfTracker& Get()
	{
		static FAnalyticsPerfTracker GTracker;
		return GTracker;
	}

	bool IsEnabled() const { return bEnabled; }

	void SetRunID(const FString& InRunID)
	{
		if (bEnabled)
		{
			RunID = InRunID;
			StartDate = FDateTime::UtcNow().ToIso8601();
		}
	}

private:
	/** Check to see if we need to log another window of time. */
	virtual bool Tick(float DeltaTime) override
	{
		if (bEnabled)
		{
			++FramesThisWindow;
			double Now = FPlatformTime::Seconds();
			if (WindowExpired(Now))
			{
				LogFile.Serialize(*FString::Printf(TEXT("%s,%s,%s,%f,%f,%f,%d,%d,%d,%d"),
					*StartDate,
					*CL,
					*RunID,
					Now - StartTime,
					Now - LastSubmitTime,
					TimeThisWindow,
					FramesThisWindow,
					FlushesThisWindow,
					NumEventsThisWindow,
					BytesThisWindow),
					ELogVerbosity::Log, FName(), Now);
				ResetWindow(Now);
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	/** Helper to reset our window in Tick. */
	bool WindowExpired(double Now)
	{
		return Now > LastSubmitTime + 60.0;
	}

	/** Helper to reset our window in Tick. */
	void ResetWindow(double Now)
	{
		LastSubmitTime = Now;
		TimeThisWindow = 0.0;
		BytesThisWindow = 0;
		NumEventsThisWindow = 0;
		FlushesThisWindow = 0;
		FramesThisWindow = 0;
	}

	/** log file to use. */
	FOutputDeviceFile LogFile{ *FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("AnalyticsTiming.csv")) };
	FString StartDate;
	FString CL;
	FString RunID = FGuid().ToString().ToLower();
	// Window tracking data
	double LastSubmitTime = 0.0;
	double TimeThisWindow = 0.0;
	uint64 BytesThisWindow = 0;
	uint64 NumEventsThisWindow = 0;
	int FlushesThisWindow = 0;
	int FramesThisWindow = 0;
	// time when the first measurement was made.
	double StartTime = FPlatformTime::Seconds();
	/** Controls whether metrics gathering is enabled. */
	bool bEnabled = false;
};

/** Used to set the RunID between matches in game code. Must be carefully called only in situations where ANALYTICS_PERF_TRACKING_ENABLED = 1 */
ANALYTICSET_API void SetAnayticsETPerfTrackingRunID(const FString& RunID)
{
	FAnalyticsPerfTracker::Get().SetRunID(RunID);
}

#define ANALYTICS_FLUSH_TRACKING_BEGIN() double FlushStartTime = FPlatformTime::Seconds()
#define ANALYTICS_FLUSH_TRACKING_END(NumBytes, NumEvents) FAnalyticsPerfTracker::Get().RecordFlush(NumBytes, NumEvents, FPlatformTime::Seconds() - FlushStartTime)

#else

#define ANALYTICS_FLUSH_TRACKING_BEGIN(...)
#define ANALYTICS_FLUSH_TRACKING_END(...)

#endif

/**
 * Implementation of analytics for Epic Telemetry.
 * Supports caching events and flushing them periodically (currently hardcoded limits).
 * Also supports a set of default attributes that will be added to every event.
 * For efficiency, this set of attributes is added directly into the set of cached events
 * with a special flag to indicate its purpose. This allows the set of cached events to be used like
 * a set of commands to be executed on flush, and allows us to inject the default attributes
 * efficiently into many events without copying the array at all.
 */
class FAnalyticsProviderET :
	public IAnalyticsProviderET,
	public FTickerObjectBase,
	public TSharedFromThis<FAnalyticsProviderET>
{
public:
	FAnalyticsProviderET(const FAnalyticsET::Config& ConfigValues);

	// FTickerObjectBase

	bool Tick(float DeltaSeconds) override;

	// IAnalyticsProvider

	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual bool StartSession(TArray<FAnalyticsEventAttribute>&& Attributes) override;
	virtual void EndSession() override;
	virtual void FlushEvents() override;

	virtual void SetAppID(FString&& AppID) override;
	virtual const FString& GetAppID() const override;
	virtual void SetUserID(const FString& InUserID) override;
	virtual FString GetUserID() const override;

	virtual FString GetSessionID() const override;
	virtual bool SetSessionID(const FString& InSessionID) override;

	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void RecordEvent(FString EventName, TArray<FAnalyticsEventAttribute>&& Attributes) override;
	virtual void RecordEventJson(FString EventName, TArray<FAnalyticsEventAttribute>&& AttributesJson) override;
	virtual void SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes) override;
	virtual const TArray<FAnalyticsEventAttribute>& GetDefaultEventAttributes() const override;
	virtual void SetEventCallback(const OnEventRecorded& Callback) override;

	virtual ~FAnalyticsProviderET();

	FString GetAPIKey() const { return APIKey; }

private:
	/** 
	 * Determines whether we need to flush. Generally, this is only if we have cached events.
	 * Since the first event is always a control event, and we overwrite multiple control events in a row,
	 * we can safely say that if the array is longer than 1 item, it must have a real event in it to flush.
	 * 
	 * NOTE: This MUST be accessed inside a lock on CachedEventsCS!!
	 */
	bool ShouldFlush() const
	{
		return CachedEvents.Num() > 1;
	}

	bool bSessionInProgress;
	/** ET Game API Key - Get from your account manager */
	FString APIKey;
	/** ET API Server */
	FString APIServer;
	/** the unique UserID as passed to ET. */
	FString UserID;
	/** The session ID */
	FString SessionID;
	/** The AppVersion passed to ET. */
	FString AppVersion;
	/** Max number of analytics events to cache before pushing to server */
	const int32 MaxCachedNumEvents;
	/** Max time that can elapse before pushing cached events to server */
	const float MaxCachedElapsedTime;
	/** Min retry delay (in seconds) after a failure to submit. */
	const float RetryDelaySecs;
	/** Timecode of the last time a flush request failed to submit (for throttling). */
	FDateTime LastFailedFlush;
	/** Allows events to not be cached when -AnalyticsDisableCaching is used. This should only be used for debugging as caching significantly reduces bandwidth overhead per event. */
	bool bShouldCacheEvents;
	/** Current countdown timer to keep track of MaxCachedElapsedTime push */
	float FlushEventsCountdown;
	/** Track destructing for unbinding callbacks when firing events at shutdown */
	bool bInDestructor;
	/** True to use the legacy backend server protocol that uses URL params. */
	bool UseLegacyProtocol;
	/** False to disable dropping events when a flush fails due to network reasons */
	bool bDropEventsOnFlushFailure;
	/** AppEnvironment to use. */
	FString AppEnvironment;
	/** UploadType to use. */
	FString UploadType;
	/**
	* Analytics event entry to be cached
	*/
	struct FAnalyticsEventEntry
	{
		/** name of event */
		FString EventName;
		/** optional list of attributes */
		TArray<FAnalyticsEventAttribute> Attributes;
		/** local time when event was triggered */
		FDateTime TimeStamp;
		/** Whether this event was added using the Json API. */
		uint32 bIsJsonEvent : 1;
		/** Whether this event is setting the default attributes to add to all events. Every cached event list will start with one of these, though it may be empty. */
		uint32 bIsDefaultAttributes : 1;
		/**
		* Constructor. Requires rvalue-refs to ensure we move values efficiently into this struct.
		*/
		FAnalyticsEventEntry(FString&& InEventName, TArray<FAnalyticsEventAttribute>&& InAttributes, bool bInIsJsonEvent, bool bInIsDefaultAttributes)
			: EventName(MoveTemp(InEventName))
			, Attributes(MoveTemp(InAttributes))
			, TimeStamp(FDateTime::UtcNow())
			, bIsJsonEvent(bInIsJsonEvent)
			, bIsDefaultAttributes(bInIsDefaultAttributes)
		{}
	};
	
	/** 
	 * List of analytic events pending a server update .
	 * NOTE: This MUST be accessed inside a lock on CachedEventsCS!!
	 */
	TArray<FAnalyticsEventEntry> CachedEvents;

	/** Critical section for updating the CachedEvents. Mutable to allow const methods to access the list. */
	mutable FCriticalSection CachedEventsCS;

	TArray<OnEventRecorded> EventRecordedCallbacks;

	/**
	* Delegate called when an event Http request completes
	*/
	void EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TSharedPtr< TArray<FAnalyticsEventEntry> > FlushedEvents);
};

class FAnalyticsProviderETNULL :
	public IAnalyticsProviderET,
	public TSharedFromThis<FAnalyticsProviderETNULL>
{
public:
	FAnalyticsProviderETNULL(const FAnalyticsET::Config& ConfigValues) {};

	// IAnalyticsProvider

	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override { return true; }
	virtual bool StartSession(TArray<FAnalyticsEventAttribute>&& Attributes) override { return true; }
	virtual void EndSession() override { }
	virtual void FlushEvents() override { }

	virtual void SetAppID(FString&& AppID) override { APIKey = MoveTemp(AppID); }
	virtual const FString& GetAppID() const override { return APIKey; }
	virtual void SetUserID(const FString& InUserID) override { UserID = InUserID; }
	virtual FString GetUserID() const override { return UserID; }

	virtual FString GetSessionID() const override { return SessionID; }
	virtual bool SetSessionID(const FString& InSessionID) override { SessionID = InSessionID; return true; }

	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override {}
	virtual void RecordEvent(FString EventName, TArray<FAnalyticsEventAttribute>&& Attributes) override {}
	virtual void RecordEventJson(FString EventName, TArray<FAnalyticsEventAttribute>&& AttributesJson) override {}
	virtual void SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes) override {}
	virtual void SetEventCallback(const OnEventRecorded& Callback) override {}

	virtual ~FAnalyticsProviderETNULL() {};

	FString GetAPIKey() const { return APIKey; }

	FString APIKey;
	FString UserID;
	FString SessionID;
};

TSharedPtr<IAnalyticsProviderET> FAnalyticsET::CreateAnalyticsProvider(const Config& ConfigValues) const
{
	// If we didn't have a proper APIKey, return NULL
	if (ConfigValues.APIKeyET.IsEmpty())
	{
		UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider config not contain required parameter %s"), *Config::GetKeyNameForAPIKey());
		return NULL;
	}
	//@todo sz
#if 0
	return TSharedPtr<IAnalyticsProviderET>(new FAnalyticsProviderETNULL(ConfigValues));
#else
	return TSharedPtr<IAnalyticsProviderET>(new FAnalyticsProviderET(ConfigValues));
#endif
}

/**
 * Perform any initialization.
 */
FAnalyticsProviderET::FAnalyticsProviderET(const FAnalyticsET::Config& ConfigValues)
	:bSessionInProgress(false)
	, APIKey(ConfigValues.APIKeyET)
	, APIServer(ConfigValues.APIServerET)
	, MaxCachedNumEvents(20)
	, MaxCachedElapsedTime(60.0f)
	, RetryDelaySecs(120.0f)
	, LastFailedFlush(FDateTime::MinValue())
	, bShouldCacheEvents(true)
	, FlushEventsCountdown(MaxCachedElapsedTime)
	, bInDestructor(false)
	, UseLegacyProtocol(ConfigValues.UseLegacyProtocol)
	, bDropEventsOnFlushFailure(ConfigValues.bDropEventsOnFlushFailure)
{
	if (APIKey.IsEmpty() || APIServer.IsEmpty())
	{
		UE_LOG(LogAnalytics, Fatal, TEXT("AnalyticsET: APIKey (%s) and APIServer (%s) cannot be empty!"), *APIKey, *APIServer);
	}

	// force very verbose logging if we are force-disabling events.
	bool bForceDisableCaching = FParse::Param(FCommandLine::Get(), TEXT("ANALYTICSDISABLECACHING"));
	if (bForceDisableCaching)
	{
		UE_SET_LOG_VERBOSITY(LogAnalytics, VeryVerbose);
		bShouldCacheEvents = false;
	}

	// if we are caching events, presize the array to max size. Otherwise, we will never have more than two entries in the array (one for the default attributes, one for the actual event)
	CachedEvents.Reserve(bShouldCacheEvents ? MaxCachedNumEvents + 1 : 2);
	// make sure that we always start with one control event in the CachedEvents array.
	CachedEvents.Emplace(FString(), TArray<FAnalyticsEventAttribute>(), false, true);

	UE_LOG(LogAnalytics, Verbose, TEXT("[%s] Initializing ET Analytics provider"), *APIKey);

	// default to FEngineVersion::Current() if one is not provided, append FEngineVersion::Current() otherwise.
	FString ConfigAppVersion = ConfigValues.AppVersionET;
	// Allow the cmdline to force a specific AppVersion so it can be set dynamically.
	FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSAPPVERSION="), ConfigAppVersion, false);
	AppVersion = ConfigAppVersion.IsEmpty() 
		? FString(FApp::GetBuildVersion())
		: ConfigAppVersion.Replace(TEXT("%VERSION%"), FApp::GetBuildVersion(), ESearchCase::CaseSensitive);

	UE_LOG(LogAnalytics, Log, TEXT("[%s] APIServer = %s. AppVersion = %s"), *APIKey, *APIServer, *AppVersion);

	// only need these if we are using the data router protocol.
	if (!UseLegacyProtocol)
	{
		AppEnvironment = ConfigValues.AppEnvironment.IsEmpty()
			? FAnalyticsET::Config::GetDefaultAppEnvironment()
			: ConfigValues.AppEnvironment;
		UploadType = ConfigValues.UploadType.IsEmpty()
			? FAnalyticsET::Config::GetDefaultUploadType()
			: ConfigValues.UploadType;
	}

	// see if there is a cmdline supplied UserID.
#if !UE_BUILD_SHIPPING
	FString ConfigUserID;
	if (FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSUSERID="), ConfigUserID, false))
	{
		SetUserID(ConfigUserID);
	}
#endif // !UE_BUILD_SHIPPING
}

bool FAnalyticsProviderET::Tick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnalyticsProviderET_Tick);

	// There are much better ways to do this, but since most events are recorded and handled on the same (game) thread,
	// this is probably mostly fine for now, and simply favoring not crashing at the moment
	FScopeLock ScopedLock(&CachedEventsCS);

	if (ShouldFlush())
	{
		// Countdown to flush
		FlushEventsCountdown -= DeltaSeconds;
		// If reached countdown or already at max cached events then flush
		if (FlushEventsCountdown <= 0 ||
			CachedEvents.Num() >= MaxCachedNumEvents)
		{
			FTimespan TimeSinceLastFailure = FDateTime::UtcNow() - LastFailedFlush;
			if (TimeSinceLastFailure.GetTotalSeconds() >= RetryDelaySecs)
			{
				FlushEvents();
			}
		}
	}
	return true;
}

FAnalyticsProviderET::~FAnalyticsProviderET()
{
	UE_LOG(LogAnalytics, Verbose, TEXT("[%s] Destroying ET Analytics provider"), *APIKey);
	bInDestructor = true;
	EndSession();
}

/**
 * Start capturing stats for upload
 * Uses the unique ApiKey associated with your app
 */
bool FAnalyticsProviderET::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	// Have to copy Attributes array because this doesn't come in as an rvalue ref.
	return StartSession(TArray<FAnalyticsEventAttribute>(Attributes));
}

bool FAnalyticsProviderET::StartSession(TArray<FAnalyticsEventAttribute>&& Attributes)
{
	UE_LOG(LogAnalytics, Log, TEXT("[%s] AnalyticsET::StartSession"), *APIKey);
	
	// end/flush previous session before staring new one
	if (bSessionInProgress)
	{
		EndSession();
	}

	FGuid SessionGUID;
	FPlatformMisc::CreateGuid(SessionGUID);
	SessionID = SessionGUID.ToString(EGuidFormats::DigitsWithHyphensInBraces);

	// always ensure we send a few specific attributes on session start.
	TArray<FAnalyticsEventAttribute> AppendedAttributes(MoveTemp(Attributes));
	// this allows mapping to ad networks attribution data
	AppendedAttributes.Emplace(TEXT("AttributionId"), FPlatformMisc::GetUniqueAdvertisingId());
	// we should always know what platform is hosting this session.
	AppendedAttributes.Emplace(TEXT("Platform"), FString(FPlatformProperties::IniPlatformName()));

	RecordEvent(TEXT("SessionStart"), MoveTemp(AppendedAttributes));
	bSessionInProgress = true;
	return bSessionInProgress;
}

/**
 * End capturing stats and queue the upload 
 */
void FAnalyticsProviderET::EndSession()
{
	if (bSessionInProgress)
	{
		RecordEvent(TEXT("SessionEnd"), TArray<FAnalyticsEventAttribute>());
	}
	FlushEvents();
	SessionID.Empty();

	bSessionInProgress = false;
}

void FAnalyticsProviderET::FlushEvents()
{
	// Warn if this takes more than 2 ms
	SCOPE_TIME_GUARD_MS(TEXT("FAnalyticsProviderET::FlushEvents"), 2);

	// There are much better ways to do this, but since most events are recorded and handled on the same (game) thread,
	// this is probably mostly fine for now, and simply favoring not crashing at the moment
	FScopeLock ScopedLock(&CachedEventsCS);

	// Make sure we don't try to flush too many times. When we are not caching events it's possible this can be called when there are no events in the array.
	if (!ShouldFlush())
	{
		return;
	}
	
	ANALYTICS_FLUSH_TRACKING_BEGIN();
	int EventCount = 0;
	int PayloadSize = 0;

	if(ensure(FModuleManager::Get().IsModuleLoaded("HTTP")))
	{
		FString Payload;
		FDateTime CurrentTime = FDateTime::UtcNow();
		// Track the current set of default attributes. We move into this array instead of just referencing it
		// because at the end we will push the latest value back onto the list of cached events.
		// We can do this without actually copying the array this way.
		TArray<FAnalyticsEventAttribute> CurrentDefaultAttributes;

		if (!UseLegacyProtocol)
		{
			TSharedPtr< TArray<FAnalyticsEventEntry> > FlushedEvents;
			if (!bDropEventsOnFlushFailure)
			{
				FlushedEvents = TSharedPtr< TArray<FAnalyticsEventEntry> >(new TArray<FAnalyticsEventEntry>());
				FlushedEvents->Reserve(CachedEvents.Num());
			}

			TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&Payload);
			JsonWriter->WriteObjectStart();
			JsonWriter->WriteArrayStart(TEXT("Events"));
			for (FAnalyticsEventEntry& Entry : CachedEvents)
			{
				if (Entry.bIsDefaultAttributes)
				{
					// This is the default attributes, so update the array.
					if (FlushedEvents.IsValid())
					{
						CurrentDefaultAttributes = Entry.Attributes; // need to copy
						FlushedEvents->Emplace(MoveTemp(Entry));
					}
					else
					{
						CurrentDefaultAttributes = MoveTemp(Entry.Attributes);
					}
				}
				else
				{
					++EventCount;

					// event entry
					JsonWriter->WriteObjectStart();
					JsonWriter->WriteValue(TEXT("EventName"), Entry.EventName);
					FString DateOffset = (CurrentTime - Entry.TimeStamp).ToString();
					JsonWriter->WriteValue(TEXT("DateOffset"), DateOffset);
					// default attributes for this event
					for (const FAnalyticsEventAttribute& Attr : CurrentDefaultAttributes)
					{
						switch (Attr.AttrType)
						{
						case FAnalyticsEventAttribute::AttrTypeEnum::String:
							JsonWriter->WriteValue(Attr.AttrName, Attr.AttrValueString);
							break;
						case FAnalyticsEventAttribute::AttrTypeEnum::Number:
							JsonWriter->WriteValue(Attr.AttrName, Attr.AttrValueNumber);
							break;
						case FAnalyticsEventAttribute::AttrTypeEnum::Boolean:
							JsonWriter->WriteValue(Attr.AttrName, Attr.AttrValueBool);
							break;
						case FAnalyticsEventAttribute::AttrTypeEnum::JsonFragment:
							JsonWriter->WriteRawJSONValue(Attr.AttrName, Attr.AttrValueString);
							break;
						}
					}
					// optional attributes for this event
					if (!Entry.bIsJsonEvent)
					{
						for (const FAnalyticsEventAttribute& Attr : Entry.Attributes)
						{
							switch (Attr.AttrType)
							{
							case FAnalyticsEventAttribute::AttrTypeEnum::String:
								JsonWriter->WriteValue(Attr.AttrName, Attr.AttrValueString);
								break;
							case FAnalyticsEventAttribute::AttrTypeEnum::Number:
								JsonWriter->WriteValue(Attr.AttrName, Attr.AttrValueNumber);
								break;
							case FAnalyticsEventAttribute::AttrTypeEnum::Boolean:
								JsonWriter->WriteValue(Attr.AttrName, Attr.AttrValueBool);
								break;
							case FAnalyticsEventAttribute::AttrTypeEnum::JsonFragment:
								JsonWriter->WriteRawJSONValue(Attr.AttrName, Attr.AttrValueString);
								break;
							}
						}
					}
					else
					{
						for (const FAnalyticsEventAttribute& Attr : Entry.Attributes)
						{
							JsonWriter->WriteRawJSONValue(Attr.AttrName, Attr.AttrValueString);
						}
					}
					JsonWriter->WriteObjectEnd();

					// move the entry into the flushed
					if (FlushedEvents.IsValid())
					{
						FlushedEvents->Emplace(MoveTemp(Entry));
					}
				}
			}

			JsonWriter->WriteArrayEnd();
			JsonWriter->WriteObjectEnd();
			JsonWriter->Close();

			FString URLPath = FString::Printf(TEXT("datarouter/api/v1/public/data?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s&AppEnvironment=%s&UploadType=%s"),
				*FPlatformHttp::UrlEncode(SessionID),
				*FPlatformHttp::UrlEncode(APIKey),
				*FPlatformHttp::UrlEncode(AppVersion),
				*FPlatformHttp::UrlEncode(UserID),
				*FPlatformHttp::UrlEncode(AppEnvironment),
				*FPlatformHttp::UrlEncode(UploadType));
			PayloadSize = URLPath.Len() + Payload.Len();

			if (UE_LOG_ACTIVE(LogAnalytics, VeryVerbose))
			{
				// Recreate the URLPath for logging because we do not want to escape the parameters when logging.
				// We cannot simply UrlEncode the entire Path after logging it because UrlEncode(Params) != UrlEncode(Param1) & UrlEncode(Param2) ...
				FString LogString = FString::Printf(TEXT("[%s] AnalyticsET URL:datarouter/api/v1/public/data?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s&AppEnvironment=%s&UploadType=%s. Payload:%s"),
					*APIKey,
					*SessionID,
					*APIKey,
					*AppVersion,
					*UserID,
					*AppEnvironment,
					*UploadType,
					*Payload);
				UE_LOG(LogAnalytics, VeryVerbose, TEXT("%s"), *LogString);
			}

			// Create/send Http request for an event
			TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
			HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));

			HttpRequest->SetURL(APIServer + URLPath);
			HttpRequest->SetVerb(TEXT("POST"));
			HttpRequest->SetContentAsString(Payload);
			// Don't set a response callback if we are in our destructor, as the instance will no longer be there to call.
			if (!bInDestructor)
			{
				HttpRequest->OnProcessRequestComplete().BindSP(this, &FAnalyticsProviderET::EventRequestComplete, FlushedEvents);
			}
			HttpRequest->ProcessRequest();
		}
		else
		{
			// this is a legacy pathway that doesn't accept batch payloads of cached data. We'll just send one request for each event, which will be slow for a large batch of requests at once.
			for (auto& Event : CachedEvents)
			{
				if (Event.bIsDefaultAttributes)
				{
					// This is the default attributes, so update the array.
					CurrentDefaultAttributes = MoveTemp(Event.Attributes);
				}
				else
				{
					++EventCount;
					FString EventParams;
					int PayloadNdx = 0;
					// default attributes for this event
					for (int DefaultAttributeNdx = 0; DefaultAttributeNdx < CurrentDefaultAttributes.Num() && PayloadNdx < 40; ++DefaultAttributeNdx, ++PayloadNdx)
					{
						EventParams += FString::Printf(TEXT("&AttributeName%d=%s&AttributeValue%d=%s"),
							PayloadNdx,
							*FPlatformHttp::UrlEncode(CurrentDefaultAttributes[DefaultAttributeNdx].AttrName),
							PayloadNdx,
							*FPlatformHttp::UrlEncode(CurrentDefaultAttributes[DefaultAttributeNdx].ToString()));
					}
					// optional attributes for this event
					for (int AttrNdx = 0; AttrNdx < Event.Attributes.Num() && PayloadNdx < 40; ++AttrNdx, ++PayloadNdx)
					{
						EventParams += FString::Printf(TEXT("&AttributeName%d=%s&AttributeValue%d=%s"),
							PayloadNdx,
							*FPlatformHttp::UrlEncode(Event.Attributes[AttrNdx].AttrName),
							PayloadNdx,
							*FPlatformHttp::UrlEncode(Event.Attributes[AttrNdx].ToString()));
					}

					// log out the un-encoded values to make reading the log easier.
					UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] AnalyticsET URL:SendEvent.1?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s&EventName=%s%s"),
						*APIKey,
						*SessionID,
						*APIKey,
						*AppVersion,
						*UserID,
						*Event.EventName,
						*EventParams);

					// Create/send Http request for an event
					TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
					HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("text/plain"));
					// Don't need to URL encode the APIServer or the EventParams, which are already encoded, and contain parameter separaters that we DON'T want encoded.
					HttpRequest->SetURL(FString::Printf(TEXT("%sSendEvent.1?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s&EventName=%s%s"),
						*APIServer,
						*FPlatformHttp::UrlEncode(SessionID),
						*FPlatformHttp::UrlEncode(APIKey),
						*FPlatformHttp::UrlEncode(AppVersion),
						*FPlatformHttp::UrlEncode(UserID),
						*FPlatformHttp::UrlEncode(Event.EventName),
						*EventParams));
					PayloadSize = HttpRequest->GetURL().Len();
					HttpRequest->SetVerb(TEXT("GET"));
					if (!bInDestructor)
					{
						HttpRequest->OnProcessRequestComplete().BindSP(this, &FAnalyticsProviderET::EventRequestComplete, TSharedPtr< TArray<FAnalyticsEventEntry> >());
					}
					HttpRequest->ProcessRequest();
				}
			}
		}

		FlushEventsCountdown = MaxCachedElapsedTime;
		// clear the array but don't reclaim the memory.
		CachedEvents.Reset();
		// Push the current set of default attributes back onto the events list for next time we flush.
		// Can't call SetDefaultEventAttributes to do this because it already assumes we have one item in the array.
		CachedEvents.Emplace(FString(), MoveTemp(CurrentDefaultAttributes), false, true);
	}
	ANALYTICS_FLUSH_TRACKING_END(PayloadSize, EventCount);
}

void FAnalyticsProviderET::SetAppID(FString&& InAppID)
{
	if (APIKey != InAppID)
	{
		// Flush any cached events that would be using the old AppID.
		FlushEvents();
		APIKey = MoveTemp(InAppID);
	}
}

const FString& FAnalyticsProviderET::GetAppID() const
{
	return APIKey;
}

void FAnalyticsProviderET::SetUserID(const FString& InUserID)
{
	// command-line specified user ID overrides all attempts to reset it.
	if (!FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSUSERID="), UserID, false))
	{
		UE_LOG(LogAnalytics, Log, TEXT("[%s] SetUserId %s"), *APIKey, *InUserID);
		// Flush any cached events that would be using the old UserID.
		FlushEvents();
		UserID = InUserID;
	}
	else if (UserID != InUserID)
	{
		UE_LOG(LogAnalytics, Log, TEXT("[%s] Overriding SetUserId %s with cmdline UserId of %s."), *APIKey, *InUserID, *UserID);
	}
}

FString FAnalyticsProviderET::GetUserID() const
{
	return UserID;
}

FString FAnalyticsProviderET::GetSessionID() const
{
	return SessionID;
}

bool FAnalyticsProviderET::SetSessionID(const FString& InSessionID)
{
	if (SessionID != InSessionID)
	{
		// Flush any cached events that would be using the old SessionID.
		FlushEvents();
		SessionID = InSessionID;
		UE_LOG(LogAnalytics, Log, TEXT("[%s] Forcing SessionID to %s."), *APIKey, *SessionID);
	}
	return true;
}

void FAnalyticsProviderET::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	// Have to copy Attributes array because this doesn't come in as an rvalue ref.
	RecordEvent(EventName, TArray<FAnalyticsEventAttribute>(Attributes));
}

void FAnalyticsProviderET::RecordEvent(FString EventName, TArray<FAnalyticsEventAttribute>&& Attributes)
{
	// fire any callbacks
	for (const auto& Cb : EventRecordedCallbacks)
	{
		Cb(EventName, Attributes, false);
	}

	// There are much better ways to do this, but since most events are recorded and handled on the same (game) thread,
	// this is probably mostly fine for now, and simply favoring not crashing at the moment
	FScopeLock ScopedLock(&CachedEventsCS);
	CachedEvents.Emplace(MoveTemp(EventName), MoveTemp(Attributes), false, false);
	// if we aren't caching events, flush immediately. This is really only for debugging as it will significantly affect bandwidth.
	if (!bShouldCacheEvents)
	{
		FlushEvents();
	}
}

void FAnalyticsProviderET::RecordEventJson(FString EventName, TArray<FAnalyticsEventAttribute>&& AttributesJson)
{
	checkf(!UseLegacyProtocol, TEXT("Cannot use Json events with legacy protocol"));

	// fire any callbacks
	for (const auto& Cb : EventRecordedCallbacks)
	{
		Cb(EventName, AttributesJson, true);
	}

	// There are much better ways to do this, but since most events are recorded and handled on the same (game) thread,
	// this is probably mostly fine for now, and simply favoring not crashing at the moment
	FScopeLock ScopedLock(&CachedEventsCS);
	CachedEvents.Emplace(MoveTemp(EventName), MoveTemp(AttributesJson), true, false);
	// if we aren't caching events, flush immediately. This is really only for debugging as it will significantly affect bandwidth.
	if (!bShouldCacheEvents)
	{
		FlushEvents();
	}
}

void FAnalyticsProviderET::SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)
{
	FScopeLock ScopedLock(&CachedEventsCS);
	// we know we always have one entry in CachedEvents, so no need to check for Num() > 0.
	// If we are trying to add two default attribute events in a row, just overwrite the last one.
	if (CachedEvents.Last().bIsDefaultAttributes)
	{
		CachedEvents.Last() = FAnalyticsEventEntry(FString(), MoveTemp(Attributes), false, true);
	}
	else
	{
		CachedEvents.Emplace(FString(), MoveTemp(Attributes), false, true);
	}
}

const TArray<FAnalyticsEventAttribute>& FAnalyticsProviderET::GetDefaultEventAttributes() const
{
	FScopeLock ScopedLock(&CachedEventsCS);

	int DefaultIndex = CachedEvents.FindLastByPredicate([](const FAnalyticsEventEntry& Entry) { return Entry.bIsDefaultAttributes == 1; });
	checkf(DefaultIndex != INDEX_NONE, TEXT("failed to find default attributes entry in analytics cached events list"));
	return CachedEvents[DefaultIndex].Attributes;
}

void FAnalyticsProviderET::SetEventCallback(const OnEventRecorded& Callback)
{
	EventRecordedCallbacks.Add(Callback);
}

void FAnalyticsProviderET::EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool, TSharedPtr< TArray<FAnalyticsEventEntry> > FlushedEvents)
{
	// process responses
	bool bEventsDelivered = false;
	if (HttpResponse.IsValid())
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] ET response for [%s]. Code: %d. Payload: %s"), *APIKey, *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			bEventsDelivered = true;
		}
	}
	else
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] ET response for [%s]. No response"), *APIKey, *HttpRequest->GetURL());
	}

	// if the events were not delivered
	if (!bEventsDelivered)
	{
		// record the time (for throttling) so we don't retry again immediately
		LastFailedFlush = FDateTime::UtcNow();

		// if FlushedEvents is passed, re-queue the events for next time
		if (FlushedEvents.IsValid())
		{
			// add a dropped submission event so we can see how often this is happening
			if (bShouldCacheEvents && CachedEvents.Num() < 1024)
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Emplace(FAnalyticsEventAttribute(FString(TEXT("HTTP_STATUS")), FString::Printf(TEXT("%d"), HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0)));
				Attributes.Emplace(FAnalyticsEventAttribute(FString(TEXT("EVENTS_IN_BATCH")), FString::Printf(TEXT("%d"), FlushedEvents->Num())));
				Attributes.Emplace(FAnalyticsEventAttribute(FString(TEXT("EVENTS_QUEUED")), FString::Printf(TEXT("%d"), CachedEvents.Num())));
				CachedEvents.Emplace(FAnalyticsEventEntry(FString(TEXT("ET.DroppedSubmission")), MoveTemp(Attributes), false, false));
			}

			// if we're being super spammy or have been offline forever, just leave it at the ET.DroppedSubmission event
			if (bShouldCacheEvents && CachedEvents.Num() < 256)
			{
				UE_LOG(LogAnalytics, Log, TEXT("[%s] ET Requeuing %d analytics events due to failure to send"), *APIKey, FlushedEvents->Num());

				// put them at the beginning since it should include a default attributes entry and we don't want to change the current default attributes
				CachedEvents.Insert(*FlushedEvents, 0);
			}
			else
			{
				UE_LOG(LogAnalytics, Error, TEXT("[%s] ET dropping %d analytics events due to too many in queue (%d)"), *APIKey, FlushedEvents->Num(), CachedEvents.Num());
			}
		}
	}
}
