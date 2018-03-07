// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//Google Play Services

#include "OnlineSubsystemGooglePlay.h"
#include "OnlineIdentityInterfaceGooglePlay.h"
#include "OnlineStoreGooglePlayCommon.h"
#include "OnlineAchievementsInterfaceGooglePlay.h"
#include "OnlineLeaderboardInterfaceGooglePlay.h"
#include "OnlineExternalUIInterfaceGooglePlay.h"
#include "OnlinePurchaseGooglePlay.h"
#include "OnlineStoreGooglePlay.h"
#include "AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "OnlineAsyncTaskManagerGooglePlay.h"
#include "OnlineAsyncTaskGooglePlayLogin.h"
#include "OnlineAsyncTaskGooglePlayLogout.h"
#include "OnlineAsyncTaskGooglePlayShowLoginUI.h"
#include "Misc/ConfigCacheIni.h"
#include "Async/TaskGraphInterfaces.h"

THIRD_PARTY_INCLUDES_START
#include <android_native_app_glue.h>

#include "gpg/android_initialization.h"
#include "gpg/builder.h"
#include "gpg/debug.h"
#include "gpg/android_support.h"
THIRD_PARTY_INCLUDES_END

using namespace gpg;

FOnlineSubsystemGooglePlay::FOnlineSubsystemGooglePlay()
	: IdentityInterface(nullptr)
	, LeaderboardsInterface(nullptr)
	, AchievementsInterface(nullptr)
	, StoreInterface(nullptr)
	, CurrentLoginTask(nullptr)
	, CurrentShowLoginUITask(nullptr)
	, CurrentLogoutTask(nullptr)
{
}

FOnlineSubsystemGooglePlay::FOnlineSubsystemGooglePlay(FName InInstanceName)
	: FOnlineSubsystemImpl(GOOGLEPLAY_SUBSYSTEM, InInstanceName)
	, IdentityInterface(nullptr)
	, LeaderboardsInterface(nullptr)
	, AchievementsInterface(nullptr)
	, StoreInterface(nullptr)
	, CurrentLoginTask(nullptr)
	, CurrentShowLoginUITask(nullptr)
	, CurrentLogoutTask(nullptr)
{
}

IOnlineIdentityPtr FOnlineSubsystemGooglePlay::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineStorePtr FOnlineSubsystemGooglePlay::GetStoreInterface() const
{
	return StoreInterface;
}

IOnlineStoreV2Ptr FOnlineSubsystemGooglePlay::GetStoreV2Interface() const
{
	return StoreV2Interface;
}

IOnlinePurchasePtr FOnlineSubsystemGooglePlay::GetPurchaseInterface() const
{
	return PurchaseInterface;
}

IOnlineSessionPtr FOnlineSubsystemGooglePlay::GetSessionInterface() const
{
	return nullptr;
}

IOnlineFriendsPtr FOnlineSubsystemGooglePlay::GetFriendsInterface() const
{
	return nullptr;
}

IOnlinePartyPtr FOnlineSubsystemGooglePlay::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemGooglePlay::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemGooglePlay::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemGooglePlay::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineLeaderboardsPtr FOnlineSubsystemGooglePlay::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemGooglePlay::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemGooglePlay::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}

IOnlineTimePtr FOnlineSubsystemGooglePlay::GetTimeInterface() const
{
	return nullptr;
}

IOnlineTitleFilePtr FOnlineSubsystemGooglePlay::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemGooglePlay::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemGooglePlay::GetAchievementsInterface() const
{
	return AchievementsInterface;
}

static bool WaitForLostFocus = false;
static bool WaitingForLogin = false;

bool FOnlineSubsystemGooglePlay::Init() 
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOnlineSubsystemGooglePlay::Init"));
	
	OnlineAsyncTaskThreadRunnable.Reset(new FOnlineAsyncTaskManagerGooglePlay);
	OnlineAsyncTaskThread.Reset(FRunnableThread::Create(OnlineAsyncTaskThreadRunnable.Get(), *FString::Printf(TEXT("OnlineAsyncTaskThread %s"), *InstanceName.ToString())));

	IdentityInterface = MakeShareable(new FOnlineIdentityGooglePlay(this));
	LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardsGooglePlay(this));
	AchievementsInterface = MakeShareable(new FOnlineAchievementsGooglePlay(this));
	ExternalUIInterface = MakeShareable(new FOnlineExternalUIGooglePlay(this));

	if (IsInAppPurchasingEnabled())
	{
		if (IsV2StoreEnabled())
		{
			StoreV2Interface = MakeShareable(new FOnlineStoreGooglePlayV2(this));
			StoreV2Interface->Init();
			PurchaseInterface = MakeShareable(new FOnlinePurchaseGooglePlay(this));
			PurchaseInterface->Init();
		}
		else
		{
			StoreInterface = MakeShareable(new FOnlineStoreGooglePlay(this));
			StoreInterface->Init();
		}
	}
	
	extern struct android_app* GNativeAndroidApp;
	check(GNativeAndroidApp != nullptr);
	AndroidInitialization::android_main(GNativeAndroidApp);

	extern jobject GJavaGlobalNativeActivity;
	PlatformConfiguration.SetActivity(GNativeAndroidApp->activity->clazz);

	OnActivityResultDelegateHandle = FJavaWrapper::OnActivityResultDelegate.AddRaw(this, &FOnlineSubsystemGooglePlay::OnActivityResult);

	return true;
}

bool FOnlineSubsystemGooglePlay::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (OnlineAsyncTaskThreadRunnable)
	{
		OnlineAsyncTaskThreadRunnable->GameTick();
	}

	return true;
}


bool FOnlineSubsystemGooglePlay::Shutdown() 
{
	UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemGooglePlay::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

	FJavaWrapper::OnActivityResultDelegate.Remove(OnActivityResultDelegateHandle);

#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface = NULL; \
	}

	// Destruct the interfaces
	DESTRUCT_INTERFACE(StoreInterface);
	DESTRUCT_INTERFACE(StoreV2Interface);
	DESTRUCT_INTERFACE(PurchaseInterface);
	DESTRUCT_INTERFACE(ExternalUIInterface);
	DESTRUCT_INTERFACE(AchievementsInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
	DESTRUCT_INTERFACE(IdentityInterface);
#undef DESTRUCT_INTERFACE

	OnlineAsyncTaskThread.Reset();
	OnlineAsyncTaskThreadRunnable.Reset();

	return true;
}


FString FOnlineSubsystemGooglePlay::GetAppId() const 
{
	//get app id from settings. 
	return TEXT( "AndroidAppIDPlaceHolder" );
}


bool FOnlineSubsystemGooglePlay::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) 
{
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}
	return false;
}

FText FOnlineSubsystemGooglePlay::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemGooglePlay", "OnlineServiceName", "Google Play");
}

bool FOnlineSubsystemGooglePlay::IsEnabled(void)
{
	bool bEnableGooglePlaySupport = true;
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bEnableGooglePlaySupport"), bEnableGooglePlaySupport, GEngineIni);

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOnlineSubsystemGooglePlay::IsEnabled %d"), bEnableGooglePlaySupport);
	return bEnableGooglePlaySupport;
}

bool FOnlineSubsystemGooglePlay::IsV2StoreEnabled()
{
	bool bUseStoreV2 = false;
	GConfig->GetBool(TEXT("OnlineSubsystemGooglePlay.Store"), TEXT("bUseStoreV2"), bUseStoreV2, GEngineIni);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOnlineSubsystemGooglePlay::IsV2StoreEnabled %d"), bUseStoreV2);
	return bUseStoreV2;
}

bool FOnlineSubsystemGooglePlay::IsInAppPurchasingEnabled()
{
	bool bSupportsInAppPurchasing = false;
	GConfig->GetBool(TEXT("OnlineSubsystemGooglePlay.Store"), TEXT("bSupportsInAppPurchasing"), bSupportsInAppPurchasing, GEngineIni);

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOnlineSubsystemGooglePlay::IsInAppPurchasingEnabled %d"), bSupportsInAppPurchasing);
	return bSupportsInAppPurchasing;
}

void FOnlineSubsystemGooglePlay::StartShowLoginUITask(int PlayerId, const FOnLoginUIClosedDelegate& Delegate)
{
	UE_LOG(LogOnline, Log, TEXT("StartShowLoginUITask PlayerId: %d"), PlayerId);

	if (AreAnyAsyncLoginTasksRunning())
	{
		UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemGooglePlay::StartShowLoginUITask: An asynchronous login task is already running."));
		Delegate.ExecuteIfBound(nullptr, PlayerId);
		return;
	}

	if (GameServicesPtr.get() == nullptr)
	{
		UE_LOG(LogOnline, Log, TEXT("StartShowLoginUITask Game Services was null"));
		// This is likely the first login attempt during this run. Attempt to create the
		// GameServices object, which will automatically start a "silent" login attempt.
		// If that succeeds, there's no need to show the login UI explicitly. If it fails,
		// we'll call ShowAuthorizationUI.
		
		auto TheDelegate = FOnlineAsyncTaskGooglePlayLogin::FOnCompletedDelegate::CreateLambda([this, PlayerId, Delegate]()
		{
			UE_LOG(LogOnline, Log, TEXT("StartShowLoginUITask starting ShowLoginUITask_Internal"));
			 StartShowLoginUITask_Internal(PlayerId, Delegate);
		});

		CurrentLoginTask = new FOnlineAsyncTaskGooglePlayLogin(this, PlayerId, TheDelegate);
		QueueAsyncTask(CurrentLoginTask);
	}
	else
	{
		UE_LOG(LogOnline, Log, TEXT("StartShowLoginUITask GameServicesPtr valid"));
		// We already have a GameServices object, so we can directly go to ShowAuthorizationUI.
		StartShowLoginUITask_Internal(PlayerId, Delegate);
	}
}

void FOnlineSubsystemGooglePlay::StartLogoutTask(int32 LocalUserNum)
{
	if (CurrentLogoutTask != nullptr)
	{
		UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemGooglePlay::StartLogoutTask: A logout task is already in progress."));
		IdentityInterface->TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		return;
	}

	CurrentLogoutTask = new FOnlineAsyncTaskGooglePlayLogout(this, LocalUserNum);
	QueueAsyncTask(CurrentLogoutTask);
}

void FOnlineSubsystemGooglePlay::ProcessGoogleClientConnectResult(bool bInSuccessful, FString AccessToken)
{
	if (CurrentShowLoginUITask != nullptr)
	{
		// Only one login task should be active at a time
		check(CurrentLoginTask == nullptr);

		CurrentShowLoginUITask->ProcessGoogleClientConnectResult(bInSuccessful, AccessToken);
	}
}

void FOnlineSubsystemGooglePlay::StartShowLoginUITask_Internal(int PlayerId, const FOnLoginUIClosedDelegate& Delegate)
{
	check(!AreAnyAsyncLoginTasksRunning());

	UE_LOG(LogOnline, Log, TEXT("StartShowLoginUITask_Internal"));
	CurrentShowLoginUITask = new FOnlineAsyncTaskGooglePlayShowLoginUI(this, PlayerId, Delegate);
	QueueAsyncTask(CurrentShowLoginUITask);
}

void FOnlineSubsystemGooglePlay::QueueAsyncTask(FOnlineAsyncTask* AsyncTask)
{
	check(OnlineAsyncTaskThreadRunnable);
	OnlineAsyncTaskThreadRunnable->AddToInQueue(AsyncTask);
}

void FOnlineSubsystemGooglePlay::OnAuthActionFinished(AuthOperation Op, AuthStatus Status)
{
	if (Op == AuthOperation::SIGN_IN)
	{
		UE_LOG(LogOnline, Log, TEXT("OnAuthActionFinished SIGN IN %d"), (int32)Status);
		if (CurrentLoginTask != nullptr)
		{
			// Only one login task should be active at a time
			check(CurrentShowLoginUITask == nullptr);

			CurrentLoginTask->OnAuthActionFinished(Op, Status);
		}
		else if(CurrentShowLoginUITask != nullptr)
		{
			// Only one login task should be active at a time
			check(CurrentLoginTask == nullptr);

			CurrentShowLoginUITask->OnAuthActionFinished(Op, Status);
		}
		else
		{
			UE_LOG(LogOnline, Log, TEXT("OnAuthActionFinished no handler!"));
		}
	}
	else if (Op == AuthOperation::SIGN_OUT)
	{
		UE_LOG(LogOnline, Log, TEXT("OnAuthActionFinished SIGN OUT %d"), (int32)Status);
		if (CurrentLogoutTask != nullptr)
		{
			CurrentLogoutTask->OnAuthActionFinished(Op, Status);
		}
	}
}

std::string FOnlineSubsystemGooglePlay::ConvertFStringToStdString(const FString& InString)
{
	int32 SrcLen  = InString.Len() + 1;
	int32 DestLen = FPlatformString::ConvertedLength<ANSICHAR>(*InString, SrcLen);
	TArray<ANSICHAR> Converted;
	Converted.AddUninitialized(DestLen);
	
	FPlatformString::Convert(Converted.GetData(), DestLen, *InString, SrcLen);

	return std::string(Converted.GetData());
}

void FOnlineSubsystemGooglePlay::OnActivityResult(JNIEnv *env, jobject thiz, jobject activity, jint requestCode, jint resultCode, jobject data)
{
	// Pass the result on to google play - otherwise, some callbacks for the turn based system do not get called.
	AndroidSupport::OnActivityResult(env, activity, requestCode, resultCode, data);
}

JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeGoogleClientConnectCompleted(JNIEnv* jenv, jobject thiz, jboolean bSuccess, jstring accessToken)
{
	FString AccessToken;
	if (bSuccess)
	{
		const char* charsToken = jenv->GetStringUTFChars(accessToken, 0);
		AccessToken = FString(UTF8_TO_TCHAR(charsToken));
		jenv->ReleaseStringUTFChars(accessToken, charsToken);
	}

	UE_LOG(LogOnline, Log, TEXT("nativeGoogleClientConnectCompleted Success: %d Token: %s"), bSuccess, *AccessToken);

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessGoogleClientConnectResult"), STAT_FSimpleDelegateGraphTask_ProcessGoogleClientConnectResult, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Google Client connected %s, Access Token: %s\n"), bSuccess ? TEXT("successfully") : TEXT("unsuccessfully"), *AccessToken);
			if (FOnlineSubsystemGooglePlay* const OnlineSub = static_cast<FOnlineSubsystemGooglePlay* const>(IOnlineSubsystem::Get(GOOGLEPLAY_SUBSYSTEM)))
			{
				OnlineSub->ProcessGoogleClientConnectResult(bSuccess, AccessToken);
			}
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessGoogleClientConnectResult),
		nullptr,
		ENamedThreads::GameThread
	);
}
