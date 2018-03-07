// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidMisc.h"
#include "AndroidJavaEnv.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/IConsoleManager.h"
#include <android/log.h>
#include <cpu-features.h>
#include "ModuleManager.h"
#include <android/keycodes.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/statfs.h>

#include "AndroidPlatformCrashContext.h"
#include "PlatformMallocCrash.h"
#include "AndroidJavaMessageBox.h"
#include "GenericPlatformChunkInstall.h"

#include "Misc/Parse.h"
#include "Internationalization/Regex.h"

#include <android_native_app_glue.h>
#include "Function.h"
#include "AndroidStats.h"
#include "CoreDelegates.h"

#if STATS
int32 FAndroidMisc::TraceMarkerFileDescriptor = -1;
#endif

// run time compatibility information
FString FAndroidMisc::AndroidVersion; // version of android we are running eg "4.0.4"
FString FAndroidMisc::DeviceMake; // make of the device we are running on eg. "samsung"
FString FAndroidMisc::DeviceModel; // model of the device we are running on eg "SAMSUNG-SGH-I437"
FString FAndroidMisc::OSLanguage; // language code the device is set to eg "deu"

// Build/API level we are running.
int32 FAndroidMisc::AndroidBuildVersion = 0;

// Whether or not the system handles the volume buttons (event will still be generated either way)
bool FAndroidMisc::VolumeButtonsHandledBySystem = true;

extern void AndroidThunkCpp_ForceQuit();

void FAndroidMisc::RequestExit( bool Force )
{
	UE_LOG(LogWindows, Log, TEXT("FAndroidMisc::RequestExit(%i)"), Force);
	if (Force)
	{
		AndroidThunkCpp_ForceQuit();
	}
	else
	{
		GIsRequestingExit = 1;
	}
}

void FAndroidMisc::LowLevelOutputDebugString(const TCHAR *Message)
{
	LocalPrint(Message);
}

void FAndroidMisc::LocalPrint(const TCHAR *Message)
{
	// Builds for distribution should not have logging in them:
	// http://developer.android.com/tools/publishing/preparing.html#publishing-configure
#if !UE_BUILD_SHIPPING
	const int MAX_LOG_LENGTH = 4096;
	// not static since may be called by different threads
	ANSICHAR MessageBuffer[MAX_LOG_LENGTH];

	const TCHAR* SourcePtr = Message;
	while (*SourcePtr)
	{
		ANSICHAR* WritePtr = MessageBuffer;
		int32 RemainingSpace = MAX_LOG_LENGTH;
		while (*SourcePtr && --RemainingSpace > 0)
		{
			if (*SourcePtr == TEXT('\r'))
			{
				// If next character is newline, skip it
				if (*(++SourcePtr) == TEXT('\n'))
					++SourcePtr;
				break;
			}
			else if (*SourcePtr == TEXT('\n'))
			{
				++SourcePtr;
				break;
			}
			else {
				*WritePtr++ = static_cast<ANSICHAR>(*SourcePtr++);
			}
		}
		*WritePtr = '\0';
		__android_log_write(ANDROID_LOG_DEBUG, "UE4", MessageBuffer);
	}
	//	__android_log_print(ANDROID_LOG_DEBUG, "UE4", "%s", TCHAR_TO_ANSI(Message));
#endif
}

// Test for device vulkan support.
static void EstablishVulkanDeviceSupport();

namespace FAndroidAppEntry
{
	extern void PlatformInit();
}

void FAndroidMisc::PlatformPreInit()
{
	FGenericPlatformMisc::PlatformPreInit();
	EstablishVulkanDeviceSupport();
	FAndroidAppEntry::PlatformInit();
}

static volatile bool HeadPhonesArePluggedIn = false;

static FAndroidMisc::FBatteryState CurrentBatteryState;

static FCriticalSection ReceiversLock;
static struct  
{
	int		Volume;
	double	TimeOfChange;
} CurrentVolume;

extern "C"
{

	JNIEXPORT void Java_com_epicgames_ue4_HeadsetReceiver_stateChanged(JNIEnv * jni, jclass clazz, jint state)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("nativeHeadsetEvent(%i)"), state);
		HeadPhonesArePluggedIn = (state == 1);
	}

	JNIEXPORT void Java_com_epicgames_ue4_VolumeReceiver_volumeChanged(JNIEnv * jni, jclass clazz, jint volume)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("nativeVolumeEvent(%i)"), volume);
		ReceiversLock.Lock();
		CurrentVolume.Volume = volume;
		CurrentVolume.TimeOfChange = FApp::GetCurrentTime();
		ReceiversLock.Unlock();
	}

	JNIEXPORT void Java_com_epicgames_ue4_BatteryReceiver_dispatchEvent(JNIEnv * jni, jclass clazz, jint status, jint level, jint temperature)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("nativeBatteryEvent(stat = %i, lvl = %i %, temp = %3.2f \u00B0C)"), status, level, float(temperature)/10.f);

		ReceiversLock.Lock();
		FAndroidMisc::FBatteryState state;
		state.State = (FAndroidMisc::EBatteryState)status;
		state.Level = level;
		state.Temperature = float(temperature)/10.f;
		CurrentBatteryState = state;
		ReceiversLock.Unlock();
	}
}


// Manage Java side OS event receivers.
static struct
{
	const char*		ClazzName;
	JNINativeMethod	Jnim;
	jclass			Clazz;
	jmethodID		StartReceiver;
	jmethodID		StopReceiver;
} JavaEventReceivers[] =
{
	{ "com/epicgames/ue4/VolumeReceiver",{ "volumeChanged", "(I)V",  (void *)Java_com_epicgames_ue4_VolumeReceiver_volumeChanged } },
	{ "com/epicgames/ue4/BatteryReceiver",{ "dispatchEvent", "(III)V",(void *)Java_com_epicgames_ue4_BatteryReceiver_dispatchEvent } },
	{ "com/epicgames/ue4/HeadsetReceiver",{ "stateChanged",  "(I)V",  (void *)Java_com_epicgames_ue4_HeadsetReceiver_stateChanged } },
};

void InitializeJavaEventReceivers()
{
	// Register natives to receive Volume, Battery, Headphones events
	JNIEnv* JEnv = AndroidJavaEnv::GetJavaEnv();
	if (nullptr != JEnv)
	{
		auto CheckJNIExceptions = [&JEnv]()
		{
			if (JEnv->ExceptionCheck())
			{
				JEnv->ExceptionDescribe();
				JEnv->ExceptionClear();
			}
		};
		auto GetStaticMethod = [&JEnv, &CheckJNIExceptions](const char* MethodName, jclass Clazz, const char* ClazzName)
		{
			jmethodID Method = JEnv->GetStaticMethodID(Clazz, MethodName, "(Landroid/app/Activity;)V");
			if (Method == 0)
			{
				UE_LOG(LogAndroid, Error, TEXT("Can't find method %s of class %s"), ANSI_TO_TCHAR(MethodName), ANSI_TO_TCHAR(ClazzName));
			}
			CheckJNIExceptions();
			return Method;
		};

		for (auto& JavaEventReceiver : JavaEventReceivers)
		{
			JavaEventReceiver.Clazz = AndroidJavaEnv::FindJavaClass(JavaEventReceiver.ClazzName);
			if (JavaEventReceiver.Clazz == nullptr)
			{
				UE_LOG(LogAndroid, Error, TEXT("Can't find class for %s"), ANSI_TO_TCHAR(JavaEventReceiver.ClazzName));
				continue;
			}
			if (JNI_OK != JEnv->RegisterNatives(JavaEventReceiver.Clazz, &JavaEventReceiver.Jnim, 1))
			{
				UE_LOG(LogAndroid, Error, TEXT("RegisterNatives failed for %s on %s"), ANSI_TO_TCHAR(JavaEventReceiver.ClazzName), ANSI_TO_TCHAR(JavaEventReceiver.Jnim.name));
				CheckJNIExceptions();
			}
			JavaEventReceiver.StartReceiver = GetStaticMethod("startReceiver", JavaEventReceiver.Clazz, JavaEventReceiver.ClazzName);
			JavaEventReceiver.StopReceiver = GetStaticMethod("stopReceiver", JavaEventReceiver.Clazz, JavaEventReceiver.ClazzName);
		}
	}
	else
	{
		UE_LOG(LogAndroid, Warning, TEXT("Failed to initialize java event receivers. JNIEnv is not valid."));
	}
}

void EnableJavaEventReceivers(bool bEnableReceivers)
{
	JNIEnv* JEnv = AndroidJavaEnv::GetJavaEnv();
	if (nullptr != JEnv)
	{
		extern struct android_app* GNativeAndroidApp;
		for (auto& JavaEventReceiver : JavaEventReceivers)
		{
			jmethodID methodId = bEnableReceivers ? JavaEventReceiver.StartReceiver : JavaEventReceiver.StopReceiver;
			if (methodId != 0)
			{
				JEnv->CallStaticVoidMethod(JavaEventReceiver.Clazz, methodId, GNativeAndroidApp->activity->clazz);
			}
		}
	}
}

static FDelegateHandle AndroidOnBackgroundBinding;
static FDelegateHandle AndroidOnForegroundBinding;

void FAndroidMisc::PlatformInit()
{
	// Increase the maximum number of simultaneously open files
	// Display Timer resolution.
	// Get swap file info
	// Display memory info
	// Setup user specified thread affinity if any
	extern void AndroidSetupDefaultThreadAffinity();
	AndroidSetupDefaultThreadAffinity();

#if STATS
	// Setup trace file descriptor
	TraceMarkerFileDescriptor = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
	if (TraceMarkerFileDescriptor == -1)
	{
		UE_LOG(LogAndroid, Warning, TEXT("Trace Marker failed to open; trace support disabled"));
	}
#endif

	InitializeJavaEventReceivers();
	AndroidOnBackgroundBinding = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddStatic(EnableJavaEventReceivers, false);
	AndroidOnForegroundBinding = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddStatic(EnableJavaEventReceivers, true);
}

extern void AndroidThunkCpp_DismissSplashScreen();

void FAndroidMisc::PlatformTearDown()
{
#if STATS
	// Tear down trace file descriptor
	if (TraceMarkerFileDescriptor != -1)
	{
		close(TraceMarkerFileDescriptor);
	}
#endif

	auto RemoveBinding = [](FCoreDelegates::FApplicationLifetimeDelegate& ApplicationLifetimeDelegate, FDelegateHandle& DelegateBinding)
	{
		if (DelegateBinding.IsValid())
		{
			ApplicationLifetimeDelegate.Remove(DelegateBinding);
			DelegateBinding.Reset();
		}
	};

	RemoveBinding(FCoreDelegates::ApplicationWillEnterBackgroundDelegate, AndroidOnBackgroundBinding);
	RemoveBinding(FCoreDelegates::ApplicationHasEnteredForegroundDelegate, AndroidOnForegroundBinding);
}

void FAndroidMisc::PlatformHandleSplashScreen(bool ShowSplashScreen)
{
	if (!ShowSplashScreen)
	{
		AndroidThunkCpp_DismissSplashScreen();
	}
}

void FAndroidMisc::GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength)
{
	*Result = 0;
	// @todo Android : get environment variable.
}

const TCHAR* FAndroidMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	if (Error == 0)
	{
		Error = errno;
	}
	char ErrorBuffer[1024];
	strerror_r(Error, ErrorBuffer, 1024);
	FCString::Strcpy(OutBuffer, BufferCount, UTF8_TO_TCHAR((const ANSICHAR*)ErrorBuffer));
	return OutBuffer;
}

EAppReturnType::Type FAndroidMisc::MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption )
{
	FJavaAndroidMessageBox MessageBox;
	MessageBox.SetText(Text);
	MessageBox.SetCaption(Caption);
	EAppReturnType::Type * ResultValues = nullptr;
	static EAppReturnType::Type ResultsOk[] = {
		EAppReturnType::Ok };
	static EAppReturnType::Type ResultsYesNo[] = {
		EAppReturnType::Yes, EAppReturnType::No };
	static EAppReturnType::Type ResultsOkCancel[] = {
		EAppReturnType::Ok, EAppReturnType::Cancel };
	static EAppReturnType::Type ResultsYesNoCancel[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::Cancel };
	static EAppReturnType::Type ResultsCancelRetryContinue[] = {
		EAppReturnType::Cancel, EAppReturnType::Retry, EAppReturnType::Continue };
	static EAppReturnType::Type ResultsYesNoYesAllNoAll[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::YesAll,
		EAppReturnType::NoAll };
	static EAppReturnType::Type ResultsYesNoYesAllNoAllCancel[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::YesAll,
		EAppReturnType::NoAll, EAppReturnType::Cancel };
	static EAppReturnType::Type ResultsYesNoYesAll[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::YesAll };

	// TODO: Should we localize button text?

	switch (MsgType)
	{
	case EAppMsgType::Ok:
		MessageBox.AddButton(TEXT("Ok"));
		ResultValues = ResultsOk;
		break;
	case EAppMsgType::YesNo:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		ResultValues = ResultsYesNo;
		break;
	case EAppMsgType::OkCancel:
		MessageBox.AddButton(TEXT("Ok"));
		MessageBox.AddButton(TEXT("Cancel"));
		ResultValues = ResultsOkCancel;
		break;
	case EAppMsgType::YesNoCancel:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Cancel"));
		ResultValues = ResultsYesNoCancel;
		break;
	case EAppMsgType::CancelRetryContinue:
		MessageBox.AddButton(TEXT("Cancel"));
		MessageBox.AddButton(TEXT("Retry"));
		MessageBox.AddButton(TEXT("Continue"));
		ResultValues = ResultsCancelRetryContinue;
		break;
	case EAppMsgType::YesNoYesAllNoAll:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Yes To All"));
		MessageBox.AddButton(TEXT("No To All"));
		ResultValues = ResultsYesNoYesAllNoAll;
		break;
	case EAppMsgType::YesNoYesAllNoAllCancel:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Yes To All"));
		MessageBox.AddButton(TEXT("No To All"));
		MessageBox.AddButton(TEXT("Cancel"));
		ResultValues = ResultsYesNoYesAllNoAllCancel;
		break;
	case EAppMsgType::YesNoYesAll:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Yes To All"));
		ResultValues = ResultsYesNoYesAll;
		break;
	default:
		check(0);
	}
	int32 Choice = MessageBox.Show();
	if (Choice >= 0 && nullptr != ResultValues)
	{
		return ResultValues[Choice];
	}
	// Failed to show dialog, or failed to get a response,
	// return default cancel response instead.
	return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
}

bool FAndroidMisc::HasPlatformFeature(const TCHAR* FeatureName)
{
	if (FCString::Stricmp(FeatureName, TEXT("Vulkan")) == 0)
	{
		return FAndroidMisc::ShouldUseVulkan();
	}

	return FGenericPlatformMisc::HasPlatformFeature(FeatureName);
}

bool FAndroidMisc::AllowRenderThread()
{
	// Check for DisableThreadedRendering CVar from DeviceProfiles config
	// Any devices in the future that need to disable threaded rendering should be given a device profile and use this CVar
	const IConsoleVariable *const CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AndroidDisableThreadedRendering"));
	if (CVar && CVar->GetInt() != 0)
	{
		return false;
	}

	// there is a crash with the nvidia tegra dual core processors namely the optimus 2x and xoom 
	// when running multithreaded it can't handle multiple threads using opengl (bug)
	// tested with lg optimus 2x and motorola xoom 
	// come back and revisit this later 
	// https://code.google.com/p/android/issues/detail?id=32636
	if (FAndroidMisc::GetGPUFamily() == FString(TEXT("NVIDIA Tegra")) && FPlatformMisc::NumberOfCores() <= 2 && FAndroidMisc::GetGLVersion().StartsWith(TEXT("OpenGL ES 2.")))
	{
		return false;
	}

	// Vivante GC1000 with 2.x driver has issues with render thread
	if (FAndroidMisc::GetGPUFamily().StartsWith(TEXT("Vivante GC1000")) && FAndroidMisc::GetGLVersion().StartsWith(TEXT("OpenGL ES 2.")))
	{
		return false;
	}

	// there is an issue with presenting the buffer on kindle fire (1st gen) with multiple threads using opengl
	if (FAndroidMisc::GetDeviceModel() == FString(TEXT("Kindle Fire")))
	{
		return false;
	}

	// there is an issue with swapbuffer ordering on startup on samsung s3 mini with multiple threads using opengl
	if (FAndroidMisc::GetDeviceModel() == FString(TEXT("GT-I8190L")))
	{
		return false;
	}

	return true;
}

int32 FAndroidMisc::NumberOfCores()
{
	int32 NumberOfCores = android_getCpuCount();
	return NumberOfCores;
}

static FAndroidMisc::FCPUState CurrentCPUState;

FAndroidMisc::FCPUState& FAndroidMisc::GetCPUState(){
	uint64_t UserTime, NiceTime, SystemTime, SoftIRQTime, IRQTime, IdleTime, IOWaitTime;
	int32		Index = 0;
	ANSICHAR	Buffer[500];

	CurrentCPUState.CoreCount = FMath::Min( FAndroidMisc::NumberOfCores(), FAndroidMisc::FCPUState::MaxSupportedCores);
	FILE* FileHandle = fopen("/proc/stat", "r");
	if (FileHandle){
		CurrentCPUState.ActivatedCoreCount = 0;
		for (size_t n = 0; n < CurrentCPUState.CoreCount; n++) {
			CurrentCPUState.Status[n] = 0;
			CurrentCPUState.PreviousUsage[n] = CurrentCPUState.CurrentUsage[n];
		}
		
		while (fgets(Buffer, 100, FileHandle)) {
#if PLATFORM_64BITS
			sscanf(Buffer, "%5s %8lu %8lu %8lu %8lu %8lu %8lu %8lu", CurrentCPUState.Name,
				&UserTime, &NiceTime, &SystemTime, &IdleTime, &IOWaitTime, &IRQTime,
				&SoftIRQTime);
#else
			sscanf(Buffer, "%5s %8llu %8llu %8llu %8llu %8llu %8llu %8llu", CurrentCPUState.Name,
				&UserTime, &NiceTime, &SystemTime, &IdleTime, &IOWaitTime, &IRQTime,
				&SoftIRQTime);
#endif

			if (0 == strncmp(CurrentCPUState.Name, "cpu", 3)) {
				Index = CurrentCPUState.Name[3] - '0';
				if (Index >= 0 && Index < CurrentCPUState.CoreCount)
				{
					if(CurrentCPUState.Name[5] != '\0')
					{
						Index = atol(&CurrentCPUState.Name[3]);
					}
					CurrentCPUState.CurrentUsage[Index].IdleTime = IdleTime;
					CurrentCPUState.CurrentUsage[Index].NiceTime = NiceTime;
					CurrentCPUState.CurrentUsage[Index].SystemTime = SystemTime;
					CurrentCPUState.CurrentUsage[Index].SoftIRQTime = SoftIRQTime;
					CurrentCPUState.CurrentUsage[Index].IRQTime = IRQTime;
					CurrentCPUState.CurrentUsage[Index].IOWaitTime = IOWaitTime;
					CurrentCPUState.CurrentUsage[Index].UserTime = UserTime;
					CurrentCPUState.CurrentUsage[Index].TotalTime = UserTime + NiceTime + SystemTime + SoftIRQTime + IRQTime + IdleTime + IOWaitTime;
					CurrentCPUState.Status[Index] = 1;
					CurrentCPUState.ActivatedCoreCount++;
				}
				if (Index == CurrentCPUState.CoreCount-1)
					break;
			}
		}
		fclose(FileHandle);

		double WallTime;
		double CPULoad[CurrentCPUState.CoreCount];
		CurrentCPUState.AverageUtilization = 0.0; 
		for (size_t n = 0; n < CurrentCPUState.CoreCount; n++) {
			if (CurrentCPUState.CurrentUsage[n].TotalTime <= CurrentCPUState.PreviousUsage[n].TotalTime) {
				CPULoad[n] = 0;
				continue;
			}

			WallTime = CurrentCPUState.CurrentUsage[n].TotalTime - CurrentCPUState.PreviousUsage[n].TotalTime;
			IdleTime = CurrentCPUState.CurrentUsage[n].IdleTime - CurrentCPUState.PreviousUsage[n].IdleTime;

			if (!WallTime || WallTime <= IdleTime) {
				CPULoad[n] = 0;
				continue;
			}
			CPULoad[n] = (WallTime - (double)IdleTime) * 100.0 / WallTime;
			CurrentCPUState.Utilization[n] = CPULoad[n];
			CurrentCPUState.AverageUtilization += CPULoad[n];
		}
		CurrentCPUState.AverageUtilization /= (double)CurrentCPUState.CoreCount;
	}else{
		FMemory::Memzero(CurrentCPUState);		
	}
	return CurrentCPUState;
}


extern FString GFilePathBase;
extern FString GFontPathBase;

class FTestUtime
{
public:
	FTestUtime()
		: Supported(false)
	{
		static FString TestFilePath = GFilePathBase + FString(TEXT("/UE4UtimeTest.txt"));
		static const char * TestFilePathChar = StringCast<ANSICHAR>(*TestFilePath).Get();
		FILE * FileHandle = fopen(TestFilePathChar, "w");
		if(FileHandle)
		{
			fclose(FileHandle);

			// get file times
			struct stat FileInfo;
			if (stat(TestFilePathChar, &FileInfo) == -1)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Unable to get file stamp for file ('%s')"), TestFilePathChar);
			}
			else
			{
				struct utimbuf Times;
			    Times.actime = 0;
			    Times.modtime = 0;
				int Result = utime(TestFilePathChar, &Times);
				Supported = -1 != Result;
				unlink(TestFilePathChar);

				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("UTime failed for local caching supported test, with error code %d\n"), Result);
			}
			
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to create file for Local cache file test\n"), Supported);
		}
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Is Local Caching Supported? %d\n"), Supported);
	}

	bool Supported;
};

bool SupportsUTime()
{
	static FTestUtime Test;
	return Test.Supported;
}


bool FAndroidMisc::SupportsLocalCaching()
{
	return true;

	/*if ( SupportsUTime() )
	{
		return true;
	}*/


}

/**
 * Good enough default crash reporter.
 */
void DefaultCrashHandler(const FAndroidCrashContext& Context)
{
	static int32 bHasEntered = 0;
	if (FPlatformAtomics::InterlockedCompareExchange(&bHasEntered, 1, 0) == 0)
	{
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR StackTrace[StackTraceSize];
		StackTrace[0] = 0;

		// Walk the stack and dump it to the allocated memory.
		FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 0, Context.Context);
		UE_LOG(LogAndroid, Error, TEXT("\n%s\n"), ANSI_TO_TCHAR(StackTrace));

		if (GLog)
		{
			GLog->SetCurrentThreadAsMasterThread();
			GLog->Flush();
		}
		
		if (GWarn)
		{
			GWarn->Flush();
		}
	}
}

/** Global pointer to crash handler */
void (* GCrashHandlerPointer)(const FGenericCrashContext& Context) = NULL;

const int32 TargetSignals[] = 
{
	SIGQUIT, // SIGQUIT is a user-initiated "crash".
	SIGILL,
	SIGFPE,
	SIGBUS,
	SIGSEGV,
	SIGSYS
};

const int32 NumTargetSignals = ARRAY_COUNT(TargetSignals);

struct sigaction PrevActions[NumTargetSignals];
static bool PreviousSignalHandlersValid = false;

static void RestorePreviousSignalHandlers()
{
	if (PreviousSignalHandlersValid)
	{
		for (int32 i = 0; i < NumTargetSignals; ++i)
		{
			sigaction(TargetSignals[i], &PrevActions[i], NULL);
		}
		PreviousSignalHandlersValid = false;
	}
}

/** True system-specific crash handler that gets called first */
void PlatformCrashHandler(int32 Signal, siginfo* Info, void* Context)
{
	// Switch to malloc crash.
	//FGenericPlatformMallocCrash::Get().SetAsGMalloc(); @todo uncomment after verification

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Signal %d caught!"), Signal);

	// Restore system handlers so Android could catch this signal after we are done with crashreport
	RestorePreviousSignalHandlers();

	FAndroidCrashContext CrashContext;
	CrashContext.InitFromSignal(Signal, Info, Context);

	if (GCrashHandlerPointer)
	{
		GCrashHandlerPointer(CrashContext);
	}
	else
	{
		// call default one
		DefaultCrashHandler(CrashContext);
	}
}

void FAndroidMisc::SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context))
{
	GCrashHandlerPointer = CrashHandler;

	RestorePreviousSignalHandlers();
	FMemory::Memzero(&PrevActions, sizeof(PrevActions));

	// Passing -1 will leave these restored and won't trap them
	if ((PTRINT)CrashHandler == -1)
	{
		return;
	}

	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	Action.sa_sigaction = PlatformCrashHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

	for (int32 i = 0; i < NumTargetSignals; ++i)
	{
		sigaction(TargetSignals[i],	&Action, &PrevActions[i]);
	}
	PreviousSignalHandlersValid = true;
}

bool FAndroidMisc::GetUseVirtualJoysticks()
{
	// joystick on commandline means don't require virtual joysticks
	if (FParse::Param(FCommandLine::Get(), TEXT("joystick")))
	{
		return false;
	}

	// Amazon Fire TV doesn't require virtual joysticks
	if (FAndroidMisc::GetDeviceMake() == FString("Amazon"))
	{
		if (FAndroidMisc::GetDeviceModel().StartsWith(TEXT("AFT")))
		{
			return false;
		}
	}

	return true;
}


bool FAndroidMisc::SupportsTouchInput()
{
	// Amazon Fire TV doesn't require virtual joysticks
	if (FAndroidMisc::GetDeviceMake() == FString("Amazon"))
	{
		if (FAndroidMisc::GetDeviceModel().StartsWith(TEXT("AFT")))
		{
			return false;
		}
	}

	return true;
}

extern void AndroidThunkCpp_RegisterForRemoteNotifications();
extern void AndroidThunkCpp_UnregisterForRemoteNotifications();

void FAndroidMisc::RegisterForRemoteNotifications()
{
	AndroidThunkCpp_RegisterForRemoteNotifications();
}

void FAndroidMisc::UnregisterForRemoteNotifications()
{
	AndroidThunkCpp_UnregisterForRemoteNotifications();
}

TArray<uint8> FAndroidMisc::GetSystemFontBytes()
{
	TArray<uint8> FontBytes;
	static FString FullFontPath = GFontPathBase + FString(TEXT("DroidSans.ttf"));
	FFileHelper::LoadFileToArray(FontBytes, *FullFontPath);
	return FontBytes;
}

class IPlatformChunkInstall* FAndroidMisc::GetPlatformChunkInstall()
{
	static IPlatformChunkInstall* ChunkInstall = nullptr;
	static bool bIniChecked = false;
	if (!ChunkInstall || !bIniChecked)
	{
		FString ProviderName;
		IPlatformChunkInstallModule* PlatformChunkInstallModule = nullptr;
		if (!GEngineIni.IsEmpty())
		{
			FString InstallModule;
			GConfig->GetString(TEXT("StreamingInstall"), TEXT("DefaultProviderName"), InstallModule, GEngineIni);
			FModuleStatus Status;
			if (FModuleManager::Get().QueryModule(*InstallModule, Status))
			{
				PlatformChunkInstallModule = FModuleManager::LoadModulePtr<IPlatformChunkInstallModule>(*InstallModule);
				if (PlatformChunkInstallModule != nullptr)
				{
					// Attempt to grab the platform installer
					ChunkInstall = PlatformChunkInstallModule->GetPlatformChunkInstall();
				}
			}
			bIniChecked = true;
		}
		if (!ChunkInstall)
		{
			// Placeholder instance
			ChunkInstall = FGenericPlatformMisc::GetPlatformChunkInstall();
		}
	}

	return ChunkInstall;
}

void FAndroidMisc::SetVersionInfo( FString InAndroidVersion, FString InDeviceMake, FString InDeviceModel, FString InOSLanguage )
{
	AndroidVersion = InAndroidVersion;
	DeviceMake = InDeviceMake;
	DeviceModel = InDeviceModel;
	OSLanguage = InOSLanguage;

	UE_LOG(LogAndroid, Display, TEXT("Android Version Make Model Language: %s %s %s %s"), *AndroidVersion, *DeviceMake, *DeviceModel, *OSLanguage);
}

const FString FAndroidMisc::GetAndroidVersion()
{
	return AndroidVersion;
}

const FString FAndroidMisc::GetDeviceMake()
{
	return DeviceMake;
}

const FString FAndroidMisc::GetDeviceModel()
{
	return DeviceModel;
}

const FString FAndroidMisc::GetOSLanguage()
{
	return OSLanguage;
}

FString FAndroidMisc::GetDefaultLocale()
{
	return OSLanguage;
}

bool FAndroidMisc::GetVolumeButtonsHandledBySystem()
{
	return VolumeButtonsHandledBySystem;
}

void FAndroidMisc::SetVolumeButtonsHandledBySystem(bool enabled)
{
	VolumeButtonsHandledBySystem = enabled;
}

int32 FAndroidMisc::GetAndroidBuildVersion()
{
	if (AndroidBuildVersion > 0)
	{
		return AndroidBuildVersion;
	}
	if (AndroidBuildVersion <= 0)
	{
		JNIEnv* JEnv = AndroidJavaEnv::GetJavaEnv();
		if (nullptr != JEnv)
		{
			jclass Class = AndroidJavaEnv::FindJavaClass("com/epicgames/ue4/GameActivity");
			if (nullptr != Class)
			{
				jfieldID Field = JEnv->GetStaticFieldID(Class, "ANDROID_BUILD_VERSION", "I");
				if (nullptr != Field)
				{
					AndroidBuildVersion = JEnv->GetStaticIntField(Class, Field);
				}
				JEnv->DeleteLocalRef(Class);
			}
		}
	}
	return AndroidBuildVersion;
}

bool FAndroidMisc::ShouldDisablePluginAtRuntime(const FString& PluginName)
{
#if PLATFORM_ANDROID_ARM64 || PLATFORM_ANDROID_X64
	// disable OnlineSubsystemGooglePlay for unsupported Android architectures
	if (PluginName.Equals(TEXT("OnlineSubsystemGooglePlay")))
	{
		return true;
	}
#endif
	return false;
}

extern void AndroidThunkCpp_SetThreadName(const char * name);
void FAndroidMisc::SetThreadName(const char* name)
{
	AndroidThunkCpp_SetThreadName(name);
}

///////////////////////////////////////////////////////////////////////////////
//
// Extracted from vk_platform.h and vulkan.h with modifications just to allow
// vkCreateInstance/vkDestroyInstance to be called to check if a driver is actually
// available (presence of libvulkan.so only means it may be available, not that
// there is an actual usable one). Cannot wait for VulkanRHI init to do this (too
// late) and vulkan.h header not guaranteed to be available. This part of the header
// is unlikely to change in future so safe enough to use this truncated version.
//

#if PLATFORM_ANDROID_ARM
// On Android/ARMv7a, Vulkan functions use the armeabi-v7a-hard calling
#define VKAPI_ATTR __attribute__((pcs("aapcs-vfp")))
#define VKAPI_CALL
#define VKAPI_PTR  VKAPI_ATTR
#else
// On other platforms, use the default calling convention
#define VKAPI_ATTR
#define VKAPI_CALL
#define VKAPI_PTR
#endif

#define VK_MAKE_VERSION(major, minor, patch) \
    (((major) << 22) | ((minor) << 12) | (patch))

#define VK_VERSION_MAJOR(version) ((uint32)(version) >> 22)
#define VK_VERSION_MINOR(version) (((uint32)(version) >> 12) & 0x3ff)
#define VK_VERSION_PATCH(version) ((uint32)(version) & 0xfff)

typedef uint32 VkFlags;
typedef uint32 VkBool32;

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;

VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_HANDLE(VkPhysicalDevice)

typedef enum VkResult {
	VK_SUCCESS = 0,
	VK_NOT_READY = 1,
	VK_TIMEOUT = 2,
	VK_EVENT_SET = 3,
	VK_EVENT_RESET = 4,
	VK_INCOMPLETE = 5,
	VK_ERROR_OUT_OF_HOST_MEMORY = -1,
	VK_ERROR_OUT_OF_DEVICE_MEMORY = -2,
	VK_ERROR_INITIALIZATION_FAILED = -3,
	VK_ERROR_DEVICE_LOST = -4,
	VK_ERROR_MEMORY_MAP_FAILED = -5,
	VK_ERROR_LAYER_NOT_PRESENT = -6,
	VK_ERROR_EXTENSION_NOT_PRESENT = -7,
	VK_ERROR_FEATURE_NOT_PRESENT = -8,
	VK_ERROR_INCOMPATIBLE_DRIVER = -9,
	VK_ERROR_TOO_MANY_OBJECTS = -10,
	VK_ERROR_FORMAT_NOT_SUPPORTED = -11,
	VK_ERROR_SURFACE_LOST_KHR = -1000000000,
	VK_ERROR_NATIVE_WINDOW_IN_USE_KHR = -1000000001,
	VK_SUBOPTIMAL_KHR = 1000001003,
	VK_ERROR_OUT_OF_DATE_KHR = -1000001004,
	VK_ERROR_INCOMPATIBLE_DISPLAY_KHR = -1000003001,
	VK_ERROR_VALIDATION_FAILED_EXT = -1000011001,
	VK_ERROR_INVALID_SHADER_NV = -1000012000,
	VK_RESULT_BEGIN_RANGE = VK_ERROR_FORMAT_NOT_SUPPORTED,
	VK_RESULT_END_RANGE = VK_INCOMPLETE,
	VK_RESULT_RANGE_SIZE = (VK_INCOMPLETE - VK_ERROR_FORMAT_NOT_SUPPORTED + 1),
	VK_RESULT_MAX_ENUM = 0x7FFFFFFF
} VkResult;

typedef enum VkStructureType {
	VK_STRUCTURE_TYPE_APPLICATION_INFO = 0,
	VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1,
	VK_STRUCTURE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkStructureType;

typedef VkFlags VkInstanceCreateFlags;

typedef struct VkApplicationInfo {
	VkStructureType	sType;
	const void*		pNext;
	const char*		pApplicationName;
	uint32			applicationVersion;
	const char*		pEngineName;
	uint			engineVersion;
	uint			apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo {
	VkStructureType				sType;
	const void*					pNext;
	VkInstanceCreateFlags		flags;
	const VkApplicationInfo*	pApplicationInfo;
	uint32						enabledLayerCount;
	const char* const*			ppEnabledLayerNames;
	uint32						enabledExtensionCount;
	const char* const*			ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct VkAllocationCallbacks {
	void*	pUserData;
	void*	pfnAllocation;
	void*	pfnReallocation;
	void*	pfnFree;
	void*	pfnInternalAllocation;
	void*	pfnInternalFree;
} VkAllocationCallbacks;

typedef uint64 VkDeviceSize;
typedef VkFlags VkSampleCountFlags;

typedef struct VkPhysicalDeviceLimits {
	uint32				maxImageDimension1D;
	uint32				maxImageDimension2D;
	uint32				maxImageDimension3D;
	uint32				maxImageDimensionCube;
	uint32				maxImageArrayLayers;
	uint32				maxTexelBufferElements;
	uint32				maxUniformBufferRange;
	uint32				maxStorageBufferRange;
	uint32				maxPushConstantsSize;
	uint32				maxMemoryAllocationCount;
	uint32				maxSamplerAllocationCount;
	VkDeviceSize		bufferImageGranularity;
	VkDeviceSize		sparseAddressSpaceSize;
	uint32				maxBoundDescriptorSets;
	uint32				maxPerStageDescriptorSamplers;
	uint32				maxPerStageDescriptorUniformBuffers;
	uint32				maxPerStageDescriptorStorageBuffers;
	uint32				maxPerStageDescriptorSampledImages;
	uint32				maxPerStageDescriptorStorageImages;
	uint32				maxPerStageDescriptorInputAttachments;
	uint32				maxPerStageResources;
	uint32				maxDescriptorSetSamplers;
	uint32				maxDescriptorSetUniformBuffers;
	uint32				maxDescriptorSetUniformBuffersDynamic;
	uint32				maxDescriptorSetStorageBuffers;
	uint32				maxDescriptorSetStorageBuffersDynamic;
	uint32				maxDescriptorSetSampledImages;
	uint32				maxDescriptorSetStorageImages;
	uint32				maxDescriptorSetInputAttachments;
	uint32				maxVertexInputAttributes;
	uint32				maxVertexInputBindings;
	uint32				maxVertexInputAttributeOffset;
	uint32				maxVertexInputBindingStride;
	uint32				maxVertexOutputComponents;
	uint32				maxTessellationGenerationLevel;
	uint32				maxTessellationPatchSize;
	uint32				maxTessellationControlPerVertexInputComponents;
	uint32				maxTessellationControlPerVertexOutputComponents;
	uint32				maxTessellationControlPerPatchOutputComponents;
	uint32				maxTessellationControlTotalOutputComponents;
	uint32				maxTessellationEvaluationInputComponents;
	uint32				maxTessellationEvaluationOutputComponents;
	uint32				maxGeometryShaderInvocations;
	uint32				maxGeometryInputComponents;
	uint32				maxGeometryOutputComponents;
	uint32				maxGeometryOutputVertices;
	uint32				maxGeometryTotalOutputComponents;
	uint32				maxFragmentInputComponents;
	uint32				maxFragmentOutputAttachments;
	uint32				maxFragmentDualSrcAttachments;
	uint32				maxFragmentCombinedOutputResources;
	uint32				maxComputeSharedMemorySize;
	uint32				maxComputeWorkGroupCount[3];
	uint32				maxComputeWorkGroupInvocations;
	uint32				maxComputeWorkGroupSize[3];
	uint32				subPixelPrecisionBits;
	uint32				subTexelPrecisionBits;
	uint32				mipmapPrecisionBits;
	uint32				maxDrawIndexedIndexValue;
	uint32				maxDrawIndirectCount;
	float				maxSamplerLodBias;
	float				maxSamplerAnisotropy;
	uint32				maxViewports;
	uint32				maxViewportDimensions[2];
	float				viewportBoundsRange[2];
	uint32				viewportSubPixelBits;
	size_t				minMemoryMapAlignment;
	VkDeviceSize		minTexelBufferOffsetAlignment;
	VkDeviceSize		minUniformBufferOffsetAlignment;
	VkDeviceSize		minStorageBufferOffsetAlignment;
	int32				minTexelOffset;
	uint32				maxTexelOffset;
	int32				minTexelGatherOffset;
	uint32				maxTexelGatherOffset;
	float				minInterpolationOffset;
	float				maxInterpolationOffset;
	uint32				subPixelInterpolationOffsetBits;
	uint32				maxFramebufferWidth;
	uint32				maxFramebufferHeight;
	uint32				maxFramebufferLayers;
	VkSampleCountFlags	framebufferColorSampleCounts;
	VkSampleCountFlags	framebufferDepthSampleCounts;
	VkSampleCountFlags	framebufferStencilSampleCounts;
	VkSampleCountFlags	framebufferNoAttachmentsSampleCounts;
	uint32				maxColorAttachments;
	VkSampleCountFlags	sampledImageColorSampleCounts;
	VkSampleCountFlags	sampledImageIntegerSampleCounts;
	VkSampleCountFlags	sampledImageDepthSampleCounts;
	VkSampleCountFlags	sampledImageStencilSampleCounts;
	VkSampleCountFlags	storageImageSampleCounts;
	uint32				maxSampleMaskWords;
	VkBool32			timestampComputeAndGraphics;
	float				timestampPeriod;
	uint32				maxClipDistances;
	uint32				maxCullDistances;
	uint32				maxCombinedClipAndCullDistances;
	uint32				discreteQueuePriorities;
	float				pointSizeRange[2];
	float				lineWidthRange[2];
	float				pointSizeGranularity;
	float				lineWidthGranularity;
	VkBool32			strictLines;
	VkBool32			standardSampleLocations;
	VkDeviceSize		optimalBufferCopyOffsetAlignment;
	VkDeviceSize		optimalBufferCopyRowPitchAlignment;
	VkDeviceSize		nonCoherentAtomSize;
} VkPhysicalDeviceLimits;

typedef struct VkPhysicalDeviceSparseProperties {
	VkBool32	residencyStandard2DBlockShape;
	VkBool32	residencyStandard2DMultisampleBlockShape;
	VkBool32	residencyStandard3DBlockShape;
	VkBool32	residencyAlignedMipSize;
	VkBool32	residencyNonResidentStrict;
} VkPhysicalDeviceSparseProperties;

typedef enum VkPhysicalDeviceType {
	VK_PHYSICAL_DEVICE_TYPE_OTHER = 0,
	VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU = 1,
	VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU = 2,
	VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU = 3,
	VK_PHYSICAL_DEVICE_TYPE_CPU = 4,
	VK_PHYSICAL_DEVICE_TYPE_BEGIN_RANGE = VK_PHYSICAL_DEVICE_TYPE_OTHER,
	VK_PHYSICAL_DEVICE_TYPE_END_RANGE = VK_PHYSICAL_DEVICE_TYPE_CPU,
	VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE = (VK_PHYSICAL_DEVICE_TYPE_CPU - VK_PHYSICAL_DEVICE_TYPE_OTHER + 1),
	VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkPhysicalDeviceType;

#define VK_MAX_PHYSICAL_DEVICE_NAME_SIZE 256
#define VK_UUID_SIZE 16

typedef struct VkPhysicalDeviceProperties {
	uint32								apiVersion;
	uint32								driverVersion;
	uint32								vendorID;
	uint32								deviceID;
	VkPhysicalDeviceType				deviceType;
	char								deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
	uint8_t								pipelineCacheUUID[VK_UUID_SIZE];
	VkPhysicalDeviceLimits				limits;
	VkPhysicalDeviceSparseProperties	sparseProperties;
} VkPhysicalDeviceProperties;

typedef VkResult(VKAPI_PTR *PFN_vkCreateInstance)(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance);
typedef void (VKAPI_PTR *PFN_vkDestroyInstance)(VkInstance instance, const VkAllocationCallbacks* pAllocator);
typedef VkResult(VKAPI_PTR *PFN_vkEnumeratePhysicalDevices)(VkInstance instance, uint32* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices);
typedef void (VKAPI_PTR *PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);

///////////////////////////////////////////////////////////////////////////////

#define UE_VK_API_VERSION	VK_MAKE_VERSION(1, 0, 1)

enum class EDeviceVulkanSupportStatus
{
	Uninitialized,
	NotSupported,
	Supported
};

static FString VulkanVersionString;
static EDeviceVulkanSupportStatus VulkanSupport = EDeviceVulkanSupportStatus::Uninitialized;

static EDeviceVulkanSupportStatus AttemptVulkanInit(void* VulkanLib)
{
	if (VulkanLib == nullptr)
	{
		return EDeviceVulkanSupportStatus::NotSupported;
	}

	// Try to get required functions to check for driver
	PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)dlsym(VulkanLib, "vkCreateInstance");
	PFN_vkDestroyInstance vkDestroyInstance = (PFN_vkDestroyInstance)dlsym(VulkanLib, "vkDestroyInstance");
	PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)dlsym(VulkanLib, "vkEnumeratePhysicalDevices");
	PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)dlsym(VulkanLib, "vkGetPhysicalDeviceProperties");
	
	if (!vkCreateInstance || !vkDestroyInstance || !vkEnumeratePhysicalDevices || !vkGetPhysicalDeviceProperties)
	{
		return EDeviceVulkanSupportStatus::NotSupported;
	}
		
	// try to create instance to verify driver available
	VkApplicationInfo App;
	FMemory::Memzero(App);
	App.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	App.pApplicationName = "UE4";
	App.applicationVersion = 0;
	App.pEngineName = "UE4";
	App.engineVersion = 0;
	App.apiVersion = UE_VK_API_VERSION;

	VkInstanceCreateInfo InstInfo;
	FMemory::Memzero(InstInfo);
	InstInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	InstInfo.pNext = nullptr;
	InstInfo.pApplicationInfo = &App;
	InstInfo.enabledExtensionCount = 0;
	InstInfo.ppEnabledExtensionNames = nullptr;

	VkInstance Instance;
	VkResult Result = vkCreateInstance(&InstInfo, nullptr, &Instance);
	if (Result != VK_SUCCESS)
	{
		return EDeviceVulkanSupportStatus::NotSupported;
	}

	// Determine Vulkan device's API level.
	uint32 GpuCount = 0;
	Result = vkEnumeratePhysicalDevices(Instance, &GpuCount, nullptr);
	if (Result != VK_SUCCESS || GpuCount == 0)
	{
		vkDestroyInstance(Instance, nullptr);
		return EDeviceVulkanSupportStatus::NotSupported;
	}

	TArray<VkPhysicalDevice> PhysicalDevices;
	PhysicalDevices.AddZeroed(GpuCount);
	Result = vkEnumeratePhysicalDevices(Instance, &GpuCount, PhysicalDevices.GetData());
	if (Result != VK_SUCCESS)
	{
		vkDestroyInstance(Instance, nullptr);
		return EDeviceVulkanSupportStatus::NotSupported;
	}

	// Don't care which device - This code is making the assumption that all devices will have same api version.
	VkPhysicalDeviceProperties DeviceProperties;
	vkGetPhysicalDeviceProperties(PhysicalDevices[0], &DeviceProperties);

	VulkanVersionString = FString::Printf(TEXT("%d.%d.%d"), VK_VERSION_MAJOR(DeviceProperties.apiVersion), VK_VERSION_MINOR(DeviceProperties.apiVersion), VK_VERSION_PATCH(DeviceProperties.apiVersion));
	vkDestroyInstance(Instance, nullptr);

	return EDeviceVulkanSupportStatus::Supported;
}

extern int32 AndroidThunkCpp_GetMetaDataInt(const FString& Key);

// Test for device vulkan support.
static void EstablishVulkanDeviceSupport()
{
	// just do this check once
	check(VulkanSupport == EDeviceVulkanSupportStatus::Uninitialized);
	// assume no
	VulkanSupport = EDeviceVulkanSupportStatus::NotSupported;
	VulkanVersionString = TEXT("0.0.0");

	// make sure the Vulkan RHI is compiled in
	if (FModuleManager::Get().ModuleExists(TEXT("VulkanRHI")))
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Testing for Vulkan availability:"));

		// does commandline override (using GL or ES2 for legacy commandlines)
		bool bForceOpenGL = FParse::Param(FCommandLine::Get(), TEXT("GL")) || FParse::Param(FCommandLine::Get(), TEXT("OpenGL")) || FParse::Param(FCommandLine::Get(), TEXT("ES2"));

		if (!bForceOpenGL)
		{
			// check for libvulkan.so
			void* VulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
			if (VulkanLib != nullptr)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("Vulkan library detected, checking for available driver"));

				// if Nougat, we can check the Vulkan version
				if (FAndroidMisc::GetAndroidBuildVersion() >= 24)
				{
					int32 VulkanVersion = AndroidThunkCpp_GetMetaDataInt(TEXT("android.hardware.vulkan.version"));
					if (VulkanVersion >= UE_VK_API_VERSION)
					{
						// final check, try initializing the instance
						VulkanSupport = AttemptVulkanInit(VulkanLib);
					}
				}
				else
				{
					// otherwise, we need to try initializing the instance
					VulkanSupport = AttemptVulkanInit(VulkanLib);
				}

				dlclose(VulkanLib);

				if (VulkanSupport == EDeviceVulkanSupportStatus::Supported)
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT("VulkanRHI is available, Vulkan capable device detected."));
				}
				else
				{
					FPlatformMisc::LowLevelOutputDebugString(TEXT("Vulkan driver NOT available."));
				}
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("Vulkan library NOT detected."));
			}
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("VulkanRHI disabled due to command line forcing OpenGL ES."));
		}
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("VulkanRHI not present."));
	}
}

bool FAndroidMisc::ShouldUseVulkan()
{
	check(VulkanSupport != EDeviceVulkanSupportStatus::Uninitialized);
	static int CachedVulkanSupport = -1;

	if (CachedVulkanSupport == -1)
	{
		static const auto CVarDisableVulkan = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Android.DisableVulkanSupport"));
		bool bSupportsVulkan = false;
		GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bSupportsVulkan"), bSupportsVulkan, GEngineIni);
		const bool bShouldUseVulkan = bSupportsVulkan && VulkanSupport == EDeviceVulkanSupportStatus::Supported && CVarDisableVulkan->GetValueOnAnyThread() == 0;
		CachedVulkanSupport = bShouldUseVulkan ? 1 : 0;

		if (bShouldUseVulkan)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("VulkanRHI will be used!"));
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("VulkanRHI will NOT be used:"));
			if(!bSupportsVulkan)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT(" ** Vulkan support is disabled in config."));
			}
			if (CVarDisableVulkan->GetValueOnAnyThread() != 0)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT(" ** Vulkan is disabled via console variable."));
			}
			if (VulkanSupport != EDeviceVulkanSupportStatus::Supported)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT(" ** Vulkan is not support by the device."));
			}
			FPlatformMisc::LowLevelOutputDebugString(TEXT("OpenGL ES will be used."));
		}
	}

	return CachedVulkanSupport == 1;
}

FString FAndroidMisc::GetVulkanVersion()
{
	check(VulkanSupport != EDeviceVulkanSupportStatus::Uninitialized);
	return VulkanVersionString;
}

extern bool AndroidThunkCpp_HasMetaDataKey(const FString& Key);

bool FAndroidMisc::IsDaydreamApplication()
{
	static const bool bIsDaydreamApplication = AndroidThunkCpp_HasMetaDataKey(TEXT("com.epicgames.ue4.GameActivity.bDaydream"));
	return bIsDaydreamApplication;
}

#if !UE_BUILD_SHIPPING
bool FAndroidMisc::IsDebuggerPresent()
{
	bool Result = false;
#if 0
	JNIEnv* JEnv = AndroidJavaEnv::GetJavaEnv();
	if (nullptr != JEnv)
	{
		jclass Class = AndroidJavaEnv::FindJavaClass("android/os/Debug");
		if (nullptr != Class)
		{
			// This segfaults for some reason. So this is all disabled for now.
			jmethodID Method = JEnv->GetStaticMethodID(Class, "isDebuggerConnected", "()Z");
			if (nullptr != Method)
			{
				Result = JEnv->CallStaticBooleanMethod(Class, Method);
			}
		}
	}
#endif
	return Result;
}
#endif

#if STATS
void FAndroidMisc::BeginNamedEvent(const struct FColor& Color, const TCHAR* Text)
{
	const int MAX_TRACE_MESSAGE_LENGTH = 256;

	// not static since may be called by different threads
	ANSICHAR TextBuffer[MAX_TRACE_MESSAGE_LENGTH];

	const TCHAR* SourcePtr = Text;
	ANSICHAR* WritePtr = TextBuffer;
	int32 RemainingSpace = MAX_TRACE_MESSAGE_LENGTH;
	while (*SourcePtr && --RemainingSpace > 0)
	{
		*WritePtr++ = static_cast<ANSICHAR>(*SourcePtr++);
	}
	*WritePtr = '\0';

	BeginNamedEvent(Color, TextBuffer);
}

void FAndroidMisc::BeginNamedEvent(const struct FColor& Color, const ANSICHAR* Text)
{
	const int MAX_TRACE_EVENT_LENGTH = 256;

	ANSICHAR EventBuffer[MAX_TRACE_EVENT_LENGTH];
	int EventLength = snprintf(EventBuffer, MAX_TRACE_EVENT_LENGTH, "B|%d|%s", getpid(), Text);
	write(TraceMarkerFileDescriptor, EventBuffer, EventLength);
}

void FAndroidMisc::EndNamedEvent()
{
	const ANSICHAR EventTerminatorChar = 'E';
	write(TraceMarkerFileDescriptor, &EventTerminatorChar, 1);
}
#endif

int FAndroidMisc::GetVolumeState(double* OutTimeOfChangeInSec)
{
	int v;
	ReceiversLock.Lock();
	v = CurrentVolume.Volume;
	if (OutTimeOfChangeInSec)
	{
		*OutTimeOfChangeInSec = CurrentVolume.TimeOfChange;
	}
	ReceiversLock.Unlock();
	return v;
}

const TCHAR* FAndroidMisc::GamePersistentDownloadDir()
{
	extern FString GExternalFilePath;
	return *GExternalFilePath;
}

FString FAndroidMisc::GetLoginId()
{
	static FString LoginId = TEXT("");

	// Return already loaded or generated Id
	if (!LoginId.IsEmpty())
	{
		return LoginId;
	}

	// Check for existing identifier file
	extern FString GExternalFilePath;
	FString LoginIdFilename = GExternalFilePath / TEXT("login-identifier.txt");
	if (FPaths::FileExists(LoginIdFilename))
	{
		if (FFileHelper::LoadFileToString(LoginId, *LoginIdFilename))
		{
			return LoginId;
		}
	}

	// Generate a new one and write to file
	FGuid DeviceGuid;
	FPlatformMisc::CreateGuid(DeviceGuid);
	LoginId = DeviceGuid.ToString();
	FFileHelper::SaveStringToFile(LoginId, *LoginIdFilename);

	return LoginId;
}

extern FString AndroidThunkCpp_GetAndroidId();

FString FAndroidMisc::GetDeviceId()
{
	static FString DeviceId = AndroidThunkCpp_GetAndroidId();

	// note: this can be empty or NOT unique depending on the OEM implementation!
	return DeviceId;
}

extern FString AndroidThunkCpp_GetAdvertisingId();

FString FAndroidMisc::GetUniqueAdvertisingId()
{
	static FString AdvertisingId = AndroidThunkCpp_GetAdvertisingId();

	// note: this can be empty if Google Play not installed, or user is blocking it!
	return AdvertisingId;
}

FAndroidMisc::FBatteryState FAndroidMisc::GetBatteryState()
{
	FBatteryState CurState;
	ReceiversLock.Lock();
	CurState = CurrentBatteryState;
	ReceiversLock.Unlock();
	return CurState;
}

int FAndroidMisc::GetBatteryLevel()
{
	FBatteryState BatteryState = GetBatteryState();
	return BatteryState.Level;
}

bool FAndroidMisc::IsRunningOnBattery()
{
	FBatteryState BatteryState = GetBatteryState();
	return BatteryState.State == BATTERY_STATE_DISCHARGING;
}

bool FAndroidMisc::AreHeadPhonesPluggedIn()
{
	return HeadPhonesArePluggedIn;
}

bool FAndroidMisc::HasActiveWiFiConnection()
{
	extern bool AndroidThunkCpp_HasActiveWiFiConnection();
	return AndroidThunkCpp_HasActiveWiFiConnection();
}

static FAndroidMisc::ReInitWindowCallbackType OnReInitWindowCallback;

FAndroidMisc::ReInitWindowCallbackType FAndroidMisc::GetOnReInitWindowCallback()
{
	return OnReInitWindowCallback;
}

void FAndroidMisc::SetOnReInitWindowCallback(FAndroidMisc::ReInitWindowCallbackType InOnReInitWindowCallback)
{
	OnReInitWindowCallback = InOnReInitWindowCallback;
}

FString FAndroidMisc::GetCPUVendor()
{	
	return DeviceMake;
}

FString FAndroidMisc::GetCPUBrand()
{
	return DeviceModel;
}

void FAndroidMisc::GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel)
{
	out_OSVersionLabel = TEXT("Android");
	out_OSSubVersionLabel = AndroidVersion;
}

FString FAndroidMisc::GetOSVersion()
{
	return GetAndroidVersion();
}

bool FAndroidMisc::GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes)
{
	extern FString GExternalFilePath;
	struct statfs FSStat = { 0 };
	FTCHARToUTF8 Converter(*GExternalFilePath);
	int Err = statfs((ANSICHAR*)Converter.Get(), &FSStat);
	
	if (Err == 0)
	{
		TotalNumberOfBytes = FSStat.f_blocks * FSStat.f_bsize;
		NumberOfFreeBytes = FSStat.f_bavail * FSStat.f_bsize;
	}
	else
	{
		int ErrNo = errno;
		UE_LOG(LogAndroid, Warning, TEXT("Unable to statfs('%s'): errno=%d (%s)"), *GExternalFilePath, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
	}
	
	return (Err == 0);
}

uint32 FAndroidMisc::GetCoreFrequency(int32 CoreIndex, ECoreFrequencyProperty CoreFrequencyProperty)
{
	uint32 ReturnFrequency = 0;
	char QueryFile[256];
	const char* FreqProperty = nullptr;
	static const char* CurrentFrequencyString = "scaling_cur_freq";
	static const char* MaxFrequencyString = "cpuinfo_max_freq";
	static const char* MinFrequencyString = "cpuinfo_min_freq";
	switch (CoreFrequencyProperty)
	{
		case ECoreFrequencyProperty::MaxFrequency:
			FreqProperty = MaxFrequencyString;
			break;
		case ECoreFrequencyProperty::MinFrequency:
			FreqProperty = MinFrequencyString;
			break;
		default:
		case ECoreFrequencyProperty::CurrentFrequency:
			FreqProperty = CurrentFrequencyString;
			break;
	}
	sprintf(QueryFile, "/sys/devices/system/cpu/cpu%d/cpufreq/%s", CoreIndex, FreqProperty);

	if (FILE* CoreFreqStateFile = fopen(QueryFile, "r"))
	{
		char curr_corefreq[32] = { 0 };
		if( fgets(curr_corefreq, ARRAY_COUNT(curr_corefreq), CoreFreqStateFile) != nullptr)
		{
			ReturnFrequency = atol(curr_corefreq);
		}
		fclose(CoreFreqStateFile);
	}
	return ReturnFrequency;
}
