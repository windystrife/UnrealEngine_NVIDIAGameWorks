// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Framework/Commands/UICommandList.h"
#include "Misc/ConfigCacheIni.h"
#include "IProfilerClient.h"
#include "ISessionManager.h"
#include "ProfilerDataSource.h"
#include "ProfilerSession.h"
#include "ProfilerCommands.h"

class SProfilerWindow;

/** Contains all settings for the profiler, accessible through the profiler manager. */
class FProfilerSettings
{
public:
	FProfilerSettings(bool bInIsDefault = false)
		: bShowCoalescedViewModesInEventGraph(true)
		, bIsEditing(false)
		, bIsDefault(bInIsDefault)
	{
		if(!bIsDefault)
		{
			LoadFromConfig();
		}
	}

	~FProfilerSettings()
	{
		if(!bIsDefault)
		{
			SaveToConfig();
		}
	}

	void LoadFromConfig()
	{
		FConfigCacheIni::LoadGlobalIniFile(ProfilerSettingsIni, TEXT("ProfilerSettings"));

		GConfig->GetBool(TEXT("Profiler.ProfilerOptions"), TEXT("bShowCoalescedViewModesInEventGraph"), bShowCoalescedViewModesInEventGraph, ProfilerSettingsIni);
	}

	void SaveToConfig()
	{
		GConfig->SetBool(TEXT("Profiler.ProfilerOptions"), TEXT("bShowCoalescedViewModesInEventGraph"), bShowCoalescedViewModesInEventGraph, ProfilerSettingsIni);
		GConfig->Flush(false, ProfilerSettingsIni);
	}

	void EnterEditMode()
	{
		bIsEditing = true;
	}

	void ExitEditMode()
	{
		bIsEditing = false;
	}

	const bool IsEditing() const
	{
		return bIsEditing;
	}

	const FProfilerSettings& GetDefaults() const
	{
		return Defaults;
	}

//protected:

/** Contains default settings. */
	static FProfilerSettings Defaults;

	/** Profiler setting filename ini. */
	FString ProfilerSettingsIni;

	/** If True, coalesced view modes related functionality will be added to the event graph. */
	bool bShowCoalescedViewModesInEventGraph;

	/** Whether profiler settings is in edit mode. */
	bool bIsEditing;

	/** Whether this instance contains defaults. */
	bool bIsDefault;
};


/** Contains basic information about tracked stat. */
class FTrackedStat
	: public FNoncopyable
	, public TSharedFromThis<FTrackedStat>
{
public:

	/**
	 * Initialization constructor.
	 *
	 * @param InGraphDataSource Data source
	 * @param InGraphColor Color used to draw the average value
	 * @param InStatID The ID of the stat which will be tracked
	 *
	 */
	FTrackedStat
	(
		TSharedRef<const FGraphDataSource> InGraphDataSource,
		const FLinearColor InGraphColor,
		const uint32 InStatID	
	)
		: GraphDataSource(InGraphDataSource)
		, GraphColor(InGraphColor)
		, StatID(InStatID)
	{ }

	/** Destructor. */
	~FTrackedStat() { }

//protected:

	/** A shared reference to the graph data source for active profiler session for the specified stat ID. */
	TSharedRef<const FGraphDataSource> GraphDataSource;

	/** A color to visualize graph value for the data graph. */
	const FLinearColor GraphColor;

	/** The ID of the stat. */
	const uint32 StatID;
};


enum class EProfilerViewMode
{
	/** Regular line graphs, for the regular stats file. */
	LineIndexBased,
	/** Thread view graph, for the raw stats file. */
	ThreadViewTimeBased,
	/** Invalid enum type, may be used as a number of enumerations. */
	InvalidOrMax,
};


/** 
 ** This class manages following areas:
 **		Connecting/disconnecting to source or device through session manager
 **		Grabbing data from connected source or device through Profiler Transport Layer
 **		Creating a new async tasks for processing/preparing data for displaying/filtering etc
 **		Saving and loading profiler snapshots
 **		
 **		@TBD
 ** */
class FProfilerManager 
	: public TSharedFromThis<FProfilerManager>
{
	friend class FProfilerActionManager;

public:
	/**
	 * Creates a profiler manager, only one instance can exist.
	 *
	 * @param InSessionManager The session manager to use
	 *
	 */
	FProfilerManager(const TSharedRef<ISessionManager>& InSessionManager);

	/** Virtual destructor. */
	virtual ~FProfilerManager();

	/**
	 * Creates an instance of the profiler manager and initializes global instance with the previously created instance of the profiler manager.
	 *
	 * @param InSessionManager The session manager to use
	 */
	static TSharedPtr<FProfilerManager> Initialize(const TSharedRef<ISessionManager>& SessionManager)
	{
		if (FProfilerManager::Instance.IsValid())
		{
			FProfilerManager::Instance.Reset();
		}

		FProfilerManager::Instance = MakeShareable(new FProfilerManager(SessionManager));
		FProfilerManager::Instance->PostConstructor();

		return FProfilerManager::Instance;
	}

	void AssignProfilerWindow(const TSharedRef<SProfilerWindow>& InProfilerWindow)
	{
		ProfilerWindow = InProfilerWindow;
	}

	/** Shutdowns the profiler manager. */
	void Shutdown()
	{
		FProfilerManager::Instance.Reset();
	}

protected:
	/**
	 * Finishes initialization of the profiler manager
	 */
	void PostConstructor();

	/**	Binds our UI commands to delegates. */
	void BindCommands();

public:
	/**
	 * @return the global instance of the FProfilerManager. 
	 * This is an internal singleton and cannot be used outside ProfilerModule. 
	 * For external use: 
	 *		IProfilerModule& ProfilerModule = FModuleManager::Get().LoadModuleChecked<IProfilerModule>("Profiler");
	 *		ProfilerModule.GetProfilerManager();
	 */
	static TSharedPtr<FProfilerManager> Get();

	/**
	 * @return an instance of the profiler action manager.
	 */
	static FProfilerActionManager& GetActionManager();

	/**
	 * @return an instance of the profiler settings.
	 */
	static FProfilerSettings& GetSettings();

	/** @returns UI command list for the profiler manager. */
	const TSharedRef< FUICommandList > GetCommandList() const;

	/**
	 * @return an instance of the profiler commands.
	 */
	static const FProfilerCommands& GetCommands();

	/**
	 * @return an instance of the profiler session.
	 */
	TSharedPtr<FProfilerSession> GetProfilerSession();

	/*-----------------------------------------------------------------------------
		Stat tracking, Session instance management
	-----------------------------------------------------------------------------*/

	bool TrackStat(const uint32 StatID);
	bool UntrackStat(const uint32 StatID);
	void TrackDefaultStats();
	void ClearStatsAndInstances();

	/**
	 * @return true, if the specified stat is currently tracked by the profiler.
	 */
	const bool IsStatTracked(const uint32 StatID) const;

	/**
	 * @return True, if the profiler has at least one fully processed capture file
	 */
	const bool IsCaptureFileFullyProcessed() const
	{
		return bHasCaptureFileFullyProcessed;
	}

	/**
	 * @return true, if the profiler is connected to a valid session
	 */
	const bool IsConnected() const
	{
		const bool bIsValid = ActiveSession.IsValid() && ActiveInstanceID.IsValid();
		return bIsValid;
	}

	const bool HasValidSession() const
	{
		return ProfilerSession.IsValid();
	}

	/**
	 * @return true, if the profiler is currently showing the latest data
	 */
	const bool IsLivePreview() const
	{
		return bLivePreview;
	}

public:
	/** @return true, if all session instances are previewing data */
	const bool IsDataPreviewing() const;

	/**
	 * Sets the data preview state for all session instances and sends message for remote profiler services.
	 *
	 * @param bRequestedDataPreviewState - data preview state that should be set
	 *
	 */
	void SetDataPreview(const bool bRequestedDataPreviewState);

	/** @return true, if all sessions instances are capturing data to a file, only valid if profiler is connected to network based session */
	const bool IsDataCapturing() const;

	/**
	 * Sets the data capture state for all session instances and sends message for remote profiler services.
	 *
	 * @param bRequestedDataCaptureState - data capture state that should be set
	 *
	 */
	void SetDataCapture(const bool bRequestedDataCaptureState);

	void ProfilerSession_OnCaptureFileProcessed(const FGuid ProfilerInstanceID);
	void ProfilerSession_OnAddThreadTime(int32 FrameIndex, const TMap<uint32, float>& ThreadMS, const TSharedRef<FProfilerStatMetaData>& StatMetaData);

	/*-----------------------------------------------------------------------------
		Event graphs management
	-----------------------------------------------------------------------------*/

	void CreateEventGraphTab(const FGuid ProfilerInstanceID);
	void CloseAllEventGraphTabs();

	/*-----------------------------------------------------------------------------
		Data graphs management
	-----------------------------------------------------------------------------*/

	void DataGraph_OnSelectionChangedForIndex(uint32 FrameStartIndex, uint32 FrameEndIndex);

	/*-----------------------------------------------------------------------------
		Events declarations
	-----------------------------------------------------------------------------*/
	
public:
	DECLARE_EVENT_OneParam(FProfilerManager, FViewModeChangedEvent, EProfilerViewMode /*NewViewMode*/);
	FViewModeChangedEvent& OnViewModeChanged()
	{
		return OnViewModeChangedEvent;
	}
	
protected:
	/** The event to execute when the profiler loaded a new stats files and the view mode needs to be changed. */
	FViewModeChangedEvent OnViewModeChangedEvent;
	

public:
	/** The event to execute when the status of specified tracked stat has changed. */
	DECLARE_EVENT_TwoParams(FProfilerManager, FTrackedStatChangedEvent, const TSharedPtr<FTrackedStat>& /*TrackedStat*/, bool /*IsTracked*/);
	FTrackedStatChangedEvent& OnTrackedStatChanged()
	{
		return TrackedStatChangedEvent;
	}
	
protected:
	/** The event to execute when the status of specified tracked stat has changed. */
	FTrackedStatChangedEvent TrackedStatChangedEvent;

	/** OnMetaDataUpdated. */

public:
	/**
	 * The event to execute when a new frame has been added to the specified profiler session instance.
	 *
	 * @param const TSharedPtr<FProfilerSession>& - a shared pointer to the profiler session instance which received a new frame
	 */
	DECLARE_EVENT_OneParam(FProfilerManager, FFrameAddedEvent, const TSharedPtr<FProfilerSession>&);
	FFrameAddedEvent& OnFrameAdded()
	{
		return FrameAddedEvent;
	}

public:
	/**
	 * The event to be invoked once per second, for example to update the data graph.
	 */
	DECLARE_EVENT(FProfilerManager, FOneSecondPassedEvent);
	FOneSecondPassedEvent& OnOneSecondPassed()
	{
		return OneSecondPassedEvent;
	}
	
protected:
	/** The event to be invoked once per second, for example to update the data graph. */
	FOneSecondPassedEvent OneSecondPassedEvent;
	

public:
	/**
	 * The event to execute when the list of session instances have changed.
	 */
	DECLARE_EVENT(FProfilerManager, FOnSessionsUpdatedEvent);
	FOnSessionsUpdatedEvent& OnSessionInstancesUpdated()
	{
		return SessionInstancesUpdatedEvent;
	}
	
protected:
	/** The event to execute when the list of session instances have changed. */
	FOnSessionsUpdatedEvent SessionInstancesUpdatedEvent;


public:
	/**
	 * The event to execute when the filter and presets widget should be updated with the latest data.
	 */
	DECLARE_EVENT(FProfilerManager, FRequestFilterAndPresetsUpdateEvent);
	FRequestFilterAndPresetsUpdateEvent& OnRequestFilterAndPresetsUpdate()
	{
		return RequestFilterAndPresetsUpdateEvent;
	}
	
protected:
	/** The event to execute when the filter and presets widget should be updated with the latest data. */
	FRequestFilterAndPresetsUpdateEvent RequestFilterAndPresetsUpdateEvent;
	

public:
	/**
	 * Creates a new profiler session instance and loads a saved profiler capture from the specified location.
	 *
	 * @param ProfilerCaptureFilepath	- The path to the file containing a captured session instance
	 */
	void LoadProfilerCapture(const FString& ProfilerCaptureFilepath);

	/** Creates a new profiler session instance and load a raw stats file from the specified location. */
	void LoadRawStatsFile(const FString& RawStatsFileFileath);

protected:
	void ProfilerClient_OnProfilerData(const FGuid& InstanceID, const FProfilerDataFrame& Content);
	void ProfilerClient_OnClientConnected(const FGuid& SessioID, const FGuid& InstanceID);
	void ProfilerClient_OnClientDisconnected(const FGuid& SessionID, const FGuid& InstanceID);
	void ProfilerClient_OnMetaDataUpdated(const FGuid& InstanceID, const FStatMetaData& MetaData);
	void ProfilerClient_OnLoadCompleted(const FGuid& InstanceID);
	void ProfilerClient_OnLoadCancelled(const FGuid& InstanceID);
	void ProfilerClient_OnLoadStarted(const FGuid& InstanceID);
	void ProfilerClient_OnProfilerFileTransfer(const FString& Filename, int64 FileProgress, int64 FileSize);
	void SessionManager_OnInstanceSelectionChanged(const TSharedPtr<ISessionInstanceInfo>& Instance, bool Selected);

public:
	const FLinearColor& GetColorForStatID(const uint32 StatID) const;

protected:
	/** Updates this manager, done through FCoreTicker. */
	bool Tick(float DeltaTime);

public:

	/**
	 * Converts profiler window weak pointer to a shared pointer and returns it.
	 * Make sure the returned pointer is valid before trying to dereference it.
	 */
	TSharedPtr<class SProfilerWindow> GetProfilerWindow() const
	{
		return ProfilerWindow.Pin();
	}

	/** Sets a new view mode for the profiler. */
	void SetViewMode(EProfilerViewMode NewViewMode);

protected:
	/** The delegate to be invoked when this profiler manager ticks. */
	FTickerDelegate OnTick;

	/** Handle to the registered OnTick. */
	FDelegateHandle OnTickHandle;

	/** A weak pointer to the profiler window. */
	TWeakPtr<class SProfilerWindow> ProfilerWindow;

	/** A shared pointer to the session manager. */
	TSharedPtr<ISessionManager> SessionManager;

	/** A shared pointer to the currently selected session in the session browser. */
	TSharedPtr<ISessionInfo> ActiveSession;

	/** A shared pointer to the currently selected instance in the session browser. */
	FGuid/*TSharedPtr<ISessionInstanceInfo>*/ ActiveInstanceID;

	/** Profiler session, to be removed from here. */
	TSharedPtr<FProfilerSession> ProfilerSession;

	/** A shared pointer to the profiler client, which is used to deliver all profiler data from the active session. */
	TSharedPtr<IProfilerClient> ProfilerClient;

	/** List of UI commands for the profiler manager. This will be filled by this and corresponding classes. */
	TSharedRef< FUICommandList > CommandList;

	/** An instance of the profiler action manager. */
	FProfilerActionManager ProfilerActionManager;

	/** An instance of the profiler options. */
	FProfilerSettings Settings;

	/** A shared pointer to the global instance of the profiler manager. */
	static TSharedPtr<FProfilerManager> Instance;

	/*-----------------------------------------------------------------------------
		Events and misc
	-----------------------------------------------------------------------------*/

	/** The event to execute when a new frame has been added to the specified profiler session instance. */
	FFrameAddedEvent FrameAddedEvent;

	/** Contains all currently tracked stats, stored as StatID -> FTrackedStat. */
	TMap<uint32, TSharedPtr<FTrackedStat>> TrackedStats;

	
	/*-----------------------------------------------------------------------------
		Profiler manager states
	-----------------------------------------------------------------------------*/

	/** Profiler session type that is currently initialized. */
	EProfilerSessionTypes ProfilerType;

	/** Profiler view mode. */
	EProfilerViewMode ViewMode;

	// TODO: Bool should be replaces with type similar to ECheckBoxState {Checked,Unchecked,Undertermined}

	/** True, if the profiler is currently showing the latest data, only valid if profiler is connected to network based session. */
	bool bLivePreview;

	/** True, if the profiler has at least one fully processed capture file. */
	bool bHasCaptureFileFullyProcessed;
};
