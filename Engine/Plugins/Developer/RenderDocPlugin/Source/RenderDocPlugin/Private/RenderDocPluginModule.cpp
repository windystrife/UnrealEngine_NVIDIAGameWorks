// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RenderDocPluginModule.h"

#include "Internationalization.h"
#include "RendererInterface.h"
#include "RenderingThread.h"

#include "RenderDocPluginNotification.h"
#include "RenderDocPluginSettings.h"
#include "AllowWindowsPlatformTypes.h"
#include "Async.h"
#include "FileManager.h"
#include "ConfigCacheIni.h"
#include "Engine/GameViewportClient.h"

#if WITH_EDITOR
#include "UnrealClient.h"
#include "Editor/EditorEngine.h"
extern UNREALED_API UEditorEngine* GEditor;
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif

DEFINE_LOG_CATEGORY(RenderDocPlugin);

#define LOCTEXT_NAMESPACE "RenderDocPlugin"

static TAutoConsoleVariable<int32> CVarRenderDocCaptureAllActivity(
	TEXT("renderdoc.CaptureAllActivity"),
	0,
	TEXT("0 - RenderDoc will only capture data from the current viewport. ")
	TEXT("1 - RenderDoc will capture all activity, in all viewports and editor windows for the entire frame."));
static TAutoConsoleVariable<int32> CVarRenderDocCaptureCallstacks(
	TEXT("renderdoc.CaptureCallstacks"),
	1,
	TEXT("0 - Callstacks will not be captured by RenderDoc. ")
	TEXT("1 - Capture callstacks for each API call."));
static TAutoConsoleVariable<int32> CVarRenderDocReferenceAllResources(
	TEXT("renderdoc.ReferenceAllResources"),
	0,
	TEXT("0 - Only include resources that are actually used. ")
	TEXT("1 - Include all rendering resources in the capture, even those that have not been used during the frame. ")
	TEXT("Please note that doing this will significantly increase capture size."));
static TAutoConsoleVariable<int32> CVarRenderDocSaveAllInitials(
	TEXT("renderdoc.SaveAllInitials"),
	0,
	TEXT("0 - Disregard initial states of resources. ")
	TEXT("1 - Always capture the initial state of all rendering resources. ")
	TEXT("Please note that doing this will significantly increase capture size."));

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper classes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct FRenderDocAsyncGraphTask : public FAsyncGraphTaskBase
{
	ENamedThreads::Type TargetThread;
	TFunction<void()> TheTask;

	FRenderDocAsyncGraphTask(ENamedThreads::Type Thread, TFunction<void()>&& Task) : TargetThread(Thread), TheTask(MoveTemp(Task)) { }
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) { TheTask(); }
	ENamedThreads::Type GetDesiredThread() { return(TargetThread); }
};

class FRenderDocFrameCapturer
{
public:
	static void BeginCapture(HWND WindowHandle, FRenderDocPluginLoader::RENDERDOC_API_CONTEXT* RenderDocAPI, FRenderDocPluginModule* Plugin)
	{
		UE4_GEmitDrawEvents_BeforeCapture = GEmitDrawEvents;
		GEmitDrawEvents = true;

		RENDERDOC_DevicePointer Device = GDynamicRHI->RHIGetNativeDevice();
		RenderDocAPI->StartFrameCapture(Device, WindowHandle);
	}
	static void EndCapture(HWND WindowHandle, FRenderDocPluginLoader::RENDERDOC_API_CONTEXT* RenderDocAPI, FRenderDocPluginModule* Plugin)
	{
		RENDERDOC_DevicePointer Device = GDynamicRHI->RHIGetNativeDevice();
		RenderDocAPI->EndFrameCapture(Device, WindowHandle);

		GEmitDrawEvents = UE4_GEmitDrawEvents_BeforeCapture;

		TGraphTask<FRenderDocAsyncGraphTask>::CreateTask().ConstructAndDispatchWhenReady(ENamedThreads::GameThread, [Plugin]()
		{
			Plugin->StartRenderDoc(FPaths::Combine(*FPaths::ProjectSavedDir(), *FString("RenderDocCaptures")));
		});
	}

private:
	static bool UE4_GEmitDrawEvents_BeforeCapture;
};
bool FRenderDocFrameCapturer::UE4_GEmitDrawEvents_BeforeCapture = false;

class FRenderDocDummyInputDevice : public IInputDevice
{
public:
	FRenderDocDummyInputDevice(FRenderDocPluginModule* InPlugin) : ThePlugin(InPlugin) { }
	virtual ~FRenderDocDummyInputDevice() { }

	/** Tick the interface (used for controlling full engine frame captures). */
	virtual void Tick(float DeltaTime) override
	{
		check(ThePlugin);
		ThePlugin->Tick(DeltaTime);
	}

	/** The remaining interfaces are irrelevant for this dummy input device. */
	virtual void SendControllerEvents() override { }
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override { }
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override { return(false); }
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override { }
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override { }

private:
	FRenderDocPluginModule* ThePlugin;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FRenderDocPluginModule
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<class IInputDevice> FRenderDocPluginModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	UE_LOG(RenderDocPlugin, Log, TEXT("Creating dummy input device (for intercepting engine ticks)"));
	FRenderDocDummyInputDevice* InputDev = new FRenderDocDummyInputDevice(this);
	return MakeShareable((IInputDevice*)InputDev);
}

void FRenderDocPluginModule::StartupModule()
{
#if !UE_BUILD_SHIPPING // Disable in shipping builds
	Loader.Initialize();
	RenderDocAPI = nullptr;
	TickNumber = 0;

#if WITH_EDITOR
	EditorExtensions = nullptr;
#endif

	if (Loader.RenderDocAPI == nullptr)
	{
		return;
	}

	InjectDebugExecKeybind();

	// Regrettably, GUsingNullRHI is set to true AFTER the PostConfigInit modules
	// have been loaded (RenderDoc plugin being one of them). When this code runs
	// the following condition will never be true, so it must be tested again in
	// the Toolbar initialization code.
	if (GUsingNullRHI)
	{
		UE_LOG(RenderDocPlugin, Warning, TEXT("RenderDoc Plugin will not be loaded because a Null RHI (Cook Server, perhaps) is being used."));
		return;
	}

	// Obtain a handle to the RenderDoc DLL that has been loaded by the RenderDoc
	// Loader Plugin; no need for error handling here since the Loader would have
	// already handled and logged these errors (but check() them just in case...)
	RenderDocAPI = Loader.RenderDocAPI;
	check(RenderDocAPI);

	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
	TickNumber = 0;

	// Setup RenderDoc settings
	FString RenderDocCapturePath = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("RenderDocCaptures"));
	if (!IFileManager::Get().DirectoryExists(*RenderDocCapturePath))
	{
		IFileManager::Get().MakeDirectory(*RenderDocCapturePath, true);
	}

	FString CapturePath = FPaths::Combine(*RenderDocCapturePath, *FDateTime::Now().ToString());
	CapturePath = FPaths::ConvertRelativePathToFull(CapturePath);
	FPaths::NormalizeDirectoryName(CapturePath);
	
	RenderDocAPI->SetLogFilePathTemplate(TCHAR_TO_ANSI(*CapturePath));

	RenderDocAPI->SetFocusToggleKeys(nullptr, 0);
	RenderDocAPI->SetCaptureKeys(nullptr, 0);

	RenderDocAPI->SetCaptureOptionU32(eRENDERDOC_Option_CaptureCallstacks, CVarRenderDocCaptureCallstacks.GetValueOnAnyThread() ? 1 : 0);
	RenderDocAPI->SetCaptureOptionU32(eRENDERDOC_Option_RefAllResources, CVarRenderDocReferenceAllResources.GetValueOnAnyThread() ? 1 : 0);
	RenderDocAPI->SetCaptureOptionU32(eRENDERDOC_Option_SaveAllInitials, CVarRenderDocSaveAllInitials.GetValueOnAnyThread() ? 1 : 0);

	RenderDocAPI->MaskOverlayBits(eRENDERDOC_Overlay_None, eRENDERDOC_Overlay_None);

#if WITH_EDITOR
	EditorExtensions = new FRenderDocPluginEditorExtension(this);
#endif

	static FAutoConsoleCommand CCmdRenderDocCaptureFrame = FAutoConsoleCommand(
		TEXT("renderdoc.CaptureFrame"),
		TEXT("Captures the rendering commands of the next frame and launches RenderDoc"),
		FConsoleCommandDelegate::CreateRaw(this, &FRenderDocPluginModule::CaptureFrame));

	UE_LOG(RenderDocPlugin, Log, TEXT("RenderDoc plugin is ready!"));
#endif
}

void FRenderDocPluginModule::BeginCapture()
{
	UE_LOG(RenderDocPlugin, Log, TEXT("Capture frame and launch renderdoc!"));
#if WITH_EDITOR
	FRenderDocPluginNotification::Get().ShowNotification(LOCTEXT("CaptureNotification", "Capturing frame"));
#else
	GEngine->AddOnScreenDebugMessage((uint64)-1, 2.0f, FColor::Emerald, FString("RenderDoc: Capturing frame"));
#endif

	pRENDERDOC_SetCaptureOptionU32 SetOptions = Loader.RenderDocAPI->SetCaptureOptionU32;
	int ok = SetOptions(eRENDERDOC_Option_CaptureCallstacks, CVarRenderDocCaptureCallstacks.GetValueOnAnyThread() ? 1 : 0); check(ok);
		ok = SetOptions(eRENDERDOC_Option_RefAllResources, CVarRenderDocReferenceAllResources.GetValueOnAnyThread() ? 1 : 0); check(ok);
		ok = SetOptions(eRENDERDOC_Option_SaveAllInitials, CVarRenderDocSaveAllInitials.GetValueOnAnyThread() ? 1 : 0); check(ok);

	HWND WindowHandle = GetActiveWindow();

	typedef FRenderDocPluginLoader::RENDERDOC_API_CONTEXT RENDERDOC_API_CONTEXT;
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		StartRenderDocCapture,
		HWND, WindowHandle, WindowHandle,
		RENDERDOC_API_CONTEXT*, RenderDocAPI, RenderDocAPI,
		FRenderDocPluginModule*, Plugin, this,
		{
			FRenderDocFrameCapturer::BeginCapture(WindowHandle, RenderDocAPI, Plugin);
		});
}

void FRenderDocPluginModule::InjectDebugExecKeybind()
{
	// Inject our key bind into the debug execs
	FConfigFile* ConfigFile = nullptr;
	// Look for the first matching INI file entry
	for (TMap<FString, FConfigFile>::TIterator It(*GConfig); It; ++It)
	{
		if (It.Key().EndsWith(TEXT("Input.ini")))
		{
			ConfigFile = &It.Value();
			break;
		}
	}
	check(ConfigFile != nullptr);
	FConfigSection* Section = ConfigFile->Find(TEXT("/Script/Engine.PlayerInput"));
	if (Section != nullptr)
	{
		Section->HandleAddCommand(TEXT("DebugExecBindings"), TEXT("(Key=F12,Command=\"RenderDoc.CaptureFrame\", Alt=true)"), true);
	}
}

void FRenderDocPluginModule::EndCapture()
{
	HWND WindowHandle = GetActiveWindow();

	typedef FRenderDocPluginLoader::RENDERDOC_API_CONTEXT RENDERDOC_API_CONTEXT;
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		EndRenderDocCapture,
		HWND, WindowHandle, WindowHandle,
		RENDERDOC_API_CONTEXT*, RenderDocAPI, RenderDocAPI,
		FRenderDocPluginModule*, Plugin, this,
		{
			FRenderDocFrameCapturer::EndCapture(WindowHandle, RenderDocAPI, Plugin);
		});
}

void FRenderDocPluginModule::CaptureFrame()
{
	if (CVarRenderDocCaptureAllActivity.GetValueOnAnyThread())
	{
		CaptureEntireFrame();
	}	
	else
	{
		CaptureCurrentViewport();
	}
}

void FRenderDocPluginModule::CaptureCurrentViewport()
{
	BeginCapture();

	// infer the intended viewport to intercept/capture:
	FViewport* Viewport (nullptr);
	check(GEngine);
	if (GEngine->GameViewport)
	{
		check(GEngine->GameViewport->Viewport);
		if (GEngine->GameViewport->Viewport->HasFocus())
			Viewport = GEngine->GameViewport->Viewport;
	}
#if WITH_EDITOR
	if (!Viewport && GEditor)
	{
		// WARNING: capturing from a "PIE-Eject" Editor viewport will not work as
		// expected; in such case, capture via the console command
		// (this has something to do with the 'active' editor viewport when the UI
		// button is clicked versus the one which the console is attached to)
		Viewport = GEditor->GetActiveViewport();
	}
#endif
	check(Viewport);
	Viewport->Draw(true);

	EndCapture();
}

void FRenderDocPluginModule::CaptureEntireFrame()
{
	// Are we already in the workings of capturing an entire engine frame?
	if (TickNumber != 0)
		return;

	// Begin tracking the global tick counter so that the Tick() method below can
	// identify the beginning and end of a complete engine update cycle.
	// NOTE: GFrameCounter counts engine ticks, while GFrameNumber counts render
	// frames. Multiple frames might get rendered in a single engine update tick.
	// All active windows are updated, in a round-robin fashion, within a single
	// engine tick. This includes thumbnail images for material preview, material
	// editor previews, cascade/persona previes, etc.
	TickNumber = GFrameCounter;
}

void FRenderDocPluginModule::Tick(float DeltaTime)
{
	if (TickNumber == 0)
		return;

	const uint32 TickDiff = GFrameCounter - TickNumber;
	const uint32 MaxCount = 2;

	check(TickDiff <= MaxCount);

	if (TickDiff == 1)
	{
		BeginCapture();
	}

	if (TickDiff == MaxCount)
	{
		EndCapture();
		TickNumber = 0;
	}
}

void FRenderDocPluginModule::StartRenderDoc(FString FrameCaptureBaseDirectory)
{
#if WITH_EDITOR
	FRenderDocPluginNotification::Get().ShowNotification(LOCTEXT("LaunchNotification", "Launching RenderDoc GUI") );
#else
	GEngine->AddOnScreenDebugMessage((uint64)-1, 2.0f, FColor::Emerald, FString("RenderDoc: Launching RenderDoc GUI"));
#endif

	FString NewestCapture = GetNewestCapture(FrameCaptureBaseDirectory);
	FString ArgumentString = FString::Printf(TEXT("\"%s\""), *FPaths::ConvertRelativePathToFull(NewestCapture).Append(TEXT(".log")));

	if (!NewestCapture.IsEmpty())
	{
		if (!RenderDocAPI->IsRemoteAccessConnected())
		{
			uint32 PID = RenderDocAPI->LaunchReplayUI(true, TCHAR_TO_ANSI(*ArgumentString));

			if (0 == PID)
			{
				UE_LOG(LogTemp, Error, TEXT("Could not launch RenderDoc!!"));
			}
		}
	}

#if WITH_EDITOR
	FRenderDocPluginNotification::Get().ShowNotification(LOCTEXT("LaunchCompletedNotification", "RenderDoc GUI Launched!") );
#else
	GEngine->AddOnScreenDebugMessage((uint64)-1, 2.0f, FColor::Emerald, FString("RenderDoc: GUI Launched!"));
#endif
}

FString FRenderDocPluginModule::GetNewestCapture(FString BaseDirectory)
{
	char LogFile[512];
	uint64_t Timestamp;
	uint32_t LogPathLength = 512;
	uint32_t Index = 0;
	FString OutString;
	
	while (RenderDocAPI->GetCapture(Index, LogFile, &LogPathLength, &Timestamp))
	{
		OutString = FString(LogPathLength, ANSI_TO_TCHAR(LogFile));

		Index++;
	}
	
	return OutString;
}

void FRenderDocPluginModule::ShutdownModule()
{
	if (GUsingNullRHI)
		return;

#if WITH_EDITOR
	delete EditorExtensions;
#endif

	Loader.Release();

	RenderDocAPI = nullptr;
}

#undef LOCTEXT_NAMESPACE

#include "HideWindowsPlatformTypes.h"

IMPLEMENT_MODULE(FRenderDocPluginModule, RenderDocPlugin)
